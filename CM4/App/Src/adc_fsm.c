#include "adc_fsm.h"
#include "adc_port.h"
#include "power_queue.h"
#include "power_rail_manager.h"
#include "payload_queue.h"   /* la tensión también sale en la telemetría */
#include "sensor_ids.h"      /* ID_ADC_BATERIA */
#include <string.h>

// Solo dos estados. Cuando está DORMIDO y toca medir, la transición
// es DORMIDO -> DORMIDO con una acción que hace el ciclo de un tirón.
// Si está DESACTIVADO, nada le saca de ahí salvo un CMD_SET_ENABLE.
enum adc_estados {
    ESTADO_DORMIDO = 0,
    ESTADO_DESACTIVADO
};

/* ================================================================== */
/*  Guards                                                             */
/* ================================================================== */

static int condicion_toca_medir(fsm_t* m) {
    adc_fsm_t* p = (adc_fsm_t*)m;
    return (p->events & ADC_EV_MEDIR) != 0;
}

/* ================================================================== */
/*  Actions                                                            */
/* ================================================================== */

static void accion_medir(fsm_t* m) {
    adc_fsm_t* p = (adc_fsm_t*)m;
    p->events &= ~ADC_EV_MEDIR;

    uint16_t voltaje_mv = 0;

    // 1) Pido el rail interno: el divisor resistivo de la batería
    //    se alimenta a través de un MOSFET que controla el rail.
    PowerRail_Request(RAIL_INTERNO);

    if (AdcPort_LeerBateriaBloqueante(&voltaje_mv)) {
        uint16_t min     = p->umbrales_inf_mv[p->modo_actual];
        uint16_t max     = p->umbrales_sup_mv[p->modo_actual];
        uint32_t now     = AdcPort_GetTickMs();

        // 2) ¿Toca enviar? Fuera de ventana, primera vez, o keepalive.
        bool fuera_rango = (voltaje_mv < min || voltaje_mv > max);
        bool keepalive   = !p->ever_sent ||
                           ((now - p->tick_last_sent_ms) >= ADC_KEEPALIVE_MS);

        if (fuera_rango || keepalive) {
            ColaPower_EnviarVoltaje(voltaje_mv);   // a la cola del power_fsm

            /* Y a la cola de salida, como una magnitud más. Si solo fuera
             * al power_fsm, desde fuera no se vería por qué el nodo cambia
             * de modo. */
            payload_msg_t msg = {
                .sensor_id    = ID_ADC_BATERIA,
                .motivo       = MOTIVO_ROUTINE,
                .valor        = (int16_t)voltaje_mv,
                .emerg_codigo = EMERG_NONE
            };
            PayloadQueue_Enqueue(&msg);

            p->tick_last_sent_ms = now;
            p->ever_sent         = true;
        }
    }

    // 3) Suelta el raíl y rearma el temporizador. Cuando venza, el driver
    //    llamará a AdcFSM_TriggerMedida y despertará a la tarea.
    PowerRail_Release(RAIL_INTERNO);
    AdcPort_StartSleepTimer(p->timeouts_ms[p->modo_actual]);
}

/* ================================================================== */
/*  Transition table                                                   */
/* ================================================================== */

static fsm_trans_t tabla_adc[] = {
    {ESTADO_DORMIDO, condicion_toca_medir, ESTADO_DORMIDO, accion_medir},
    {-1, NULL, -1, NULL}
};

/* ================================================================== */
/*  Command dispatcher                                                 */
/* ================================================================== */

void AdcFSM_ApplyCmd(adc_fsm_t* p, const config_cmd_t* cmd) {
    switch (cmd->type) {

        case CMD_SET_TIMEOUT:
            if (cmd->mode < NUMERO_ESTADOS)
                p->timeouts_ms[cmd->mode] = cmd->value;
            break;

        case CMD_SET_MODE:
            if (cmd->value < NUMERO_ESTADOS) {
                p->modo_actual = (power_state_t)cmd->value;
                if (p->activado && p->fsm.current_state == ESTADO_DORMIDO)
                    AdcPort_StartSleepTimer(p->timeouts_ms[p->modo_actual]);
            }
            break;

        case CMD_SET_ENABLE:
            if (p->activado == (bool)cmd->value) break;
            p->activado = (bool)cmd->value;
            if (!p->activado) {
                p->events &= ~ADC_EV_MEDIR;
                p->fsm.current_state = ESTADO_DESACTIVADO;
            } else {
                p->fsm.current_state = ESTADO_DORMIDO;
                AdcPort_StartSleepTimer(p->timeouts_ms[p->modo_actual]);
            }
            break;

        case CMD_SET_WINDOW:
            if (cmd->mode < NUMERO_ESTADOS) {
                p->umbrales_inf_mv[cmd->mode] = (uint16_t)cmd->value;
                p->umbrales_sup_mv[cmd->mode] = (uint16_t)cmd->value2;
            }
            break;

        case CMD_FORCE_READ:
            if (p->activado && p->fsm.current_state == ESTADO_DORMIDO)
                p->events |= ADC_EV_MEDIR;
            break;

        case CMD_SET_THRESHOLD:
            /* El ADC no usa umbral: publica por ventana, así que se ignora */
            break;

        default:
            break;
    }
}

/* ================================================================== */
/*  Public API                                                         */
/* ================================================================== */

void AdcFSM_Init(adc_fsm_t*      p,
                 const uint16_t  min_mv[NUMERO_ESTADOS],
                 const uint16_t  max_mv[NUMERO_ESTADOS],
                 const uint32_t  t[NUMERO_ESTADOS],
                 uint32_t        cmd_queue_depth)
{
    fsm_init((fsm_t*)p, tabla_adc);
    p->fsm.current_state = ESTADO_DORMIDO;
    p->activado          = true;
    p->events            = 0;
    p->modo_actual       = POWER_STATE_CONSERVACION;
    p->ever_sent         = false;
    p->tick_last_sent_ms = 0;

    for (int i = 0; i < NUMERO_ESTADOS; i++) {
        p->umbrales_inf_mv[i] = min_mv[i];
        p->umbrales_sup_mv[i] = max_mv[i];
        p->timeouts_ms[i]     = t[i];
    }

    p->cmd_queue = Queue_Create(cmd_queue_depth, sizeof(config_cmd_t));

    // Arranca el primer sleep timer alineado al siguiente múltiplo del
    // período, igual que hacía alinear_y_esperar en el modelo task-driven.
    {
        uint32_t period = p->timeouts_ms[p->modo_actual];
        uint32_t now    = AdcPort_GetTickMs();
        uint32_t rem    = now % period;
        AdcPort_StartSleepTimer(rem == 0u ? period : (period - rem));
    }
}

void AdcFSM_DrainCmdQueue(adc_fsm_t* p) {
    config_cmd_t cmd;
    while (Queue_Receive(p->cmd_queue, &cmd, QUEUE_NO_WAIT))
        AdcFSM_ApplyCmd(p, &cmd);
}

void AdcFSM_TriggerMedida(adc_fsm_t* p) {
    p->events |= ADC_EV_MEDIR;
}
