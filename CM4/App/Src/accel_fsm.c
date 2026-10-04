#include "accel_fsm.h"
#include "accel_port.h"
#include "payload_queue.h"
#include <string.h>

// Estados internos. La FSM es muy pequeña:
//   DORMIDO  -- esperando a que toque medir o salte una alarma
//   MIDIENDO -- pidiendo datos al acelerómetro por I2C (DMA)
//   DESACTIVADO -- el sensor está apagado, no respondo a nada
enum accel_estados {
    ESTADO_DORMIDO = 0,
    ESTADO_MIDIENDO,
    ESTADO_DESACTIVADO
};

/* ================================================================== */
/*  Guards                                                             */
/* ================================================================== */

static int check_toca_medir(fsm_t* m) {
    accel_fsm_t* p = (accel_fsm_t*)m;
    return (p->events & ACCEL_EV_MEDIR) != 0;
}

static int check_i2c_listo(fsm_t* m) {
    (void)m;
    return AccelPort_DatosListos();
}

static int check_alarma(fsm_t* m) {
    accel_fsm_t* p = (accel_fsm_t*)m;
    return (p->events & ACCEL_EV_ALARM) != 0;
}

/* ================================================================== */
/*  Actions                                                            */
/* ================================================================== */

static void accion_pedir_lectura(fsm_t* m) {
    accel_fsm_t* p = (accel_fsm_t*)m;
    p->events &= ~ACCEL_EV_MEDIR;
    AccelPort_IniciarLecturaI2C();
}

static void accion_recoger_y_enviar(fsm_t* m) {
    accel_fsm_t* p = (accel_fsm_t*)m;
    int16_t x = 0, y = 0, z = 0;

    // Pedimos los 3 ejes al puerto. Si la lectura falla, simplemente
    // no encolamos nada y reprogramamos el siguiente sueño.
    if (AccelPort_LeerBuffer(&x, &y, &z)) {
        uint32_t now = AccelPort_GetTickMs();
        uint8_t  db  = p->deadband_lsb[p->modo_energia_actual];

        // AccelPort_LeerBuffer ya devuelve los 12 bits útiles (el >>4 lo hace
        // el driver). Aquí no hay que volver a desplazar: si se hace dos
        // veces, la aceleración sale dividida entre 256 y los valores siguen
        // pareciendo razonables, que es lo que hace difícil verlo.
        const int16_t vals[ACCEL_MAGNITUDES] = {x, y, z};
        const uint8_t ids [ACCEL_MAGNITUDES] = {ID_ACCEL_X, ID_ACCEL_Y, ID_ACCEL_Z};

        // Para cada eje: lo mando solo si pasa el filtro de deadband/keepalive.
        for (int i = 0; i < ACCEL_MAGNITUDES; i++) {
            if (sensor_should_send(&p->mag[i], vals[i], db, ACCEL_KEEPALIVE_MS, now)) {
                payload_msg_t msg = {
                    .sensor_id    = ids[i],
                    .motivo       = MOTIVO_ROUTINE,
                    .valor        = vals[i],
                    .emerg_codigo = EMERG_NONE
                };
                PayloadQueue_Enqueue(&msg);
                p->mag[i].last_sent         = vals[i];
                p->mag[i].tick_last_sent_ms = now;
                p->mag[i].ever_sent         = true;
            }
        }
    }
    // Rearmo el temporizador; cuando venza, el driver llamará a
    // AccelFSM_TriggerMedida y despertará a la tarea, que mientras tanto
    // está bloqueada en ulTaskNotifyTake.
    AccelPort_StartSleepTimer(p->timeouts_ms[p->modo_energia_actual]);
}

static void accion_procesar_alarma(fsm_t* m) {
    accel_fsm_t* p = (accel_fsm_t*)m;
    p->events &= ~ACCEL_EV_ALARM;

    // Pregunto al sensor por qué saltó: ¿robo (movimiento) o caída?
    // El código de emergencia identifica la causa de forma explícita.
    // El campo "valor" queda a 0 porque estas emergencias son eventos
    // puros (sin dato adicional); el receptor solo procesa los 2 bytes
    // [sensor_id][emerg_codigo].
    bool es_robo = false, es_caida = false;
    if (AccelPort_LeerCausasAlarma(&es_robo, &es_caida)) {
        // sensor_id = ID_ACCEL_X identifica el acelerómetro como fuente.
        // No hay un ID_ACCEL_ALARMA separado: el FPort ya distingue
        // rutina de emergencia; emerg_codigo dice la causa concreta.
        if (es_robo) {
            payload_msg_t msg = {
                .sensor_id    = ID_ACCEL_X,
                .motivo       = MOTIVO_EMERGENCY,
                .valor        = 0,
                .emerg_codigo = EMERG_ROBO
            };
            PayloadQueue_Enqueue(&msg);
        } else if (es_caida) {
            payload_msg_t msg = {
                .sensor_id    = ID_ACCEL_X,
                .motivo       = MOTIVO_EMERGENCY,
                .valor        = 0,
                .emerg_codigo = EMERG_CAIDA
            };
            PayloadQueue_Enqueue(&msg);
        }
    }
}

/* ================================================================== */
/*  Transition table                                                   */
/* ================================================================== */

// Tabla de transiciones. El orden marca la prioridad:
// la alarma se procesa ANTES que la medida de rutina si ambas están pendientes.
static fsm_trans_t tabla_accel[] = {
    {ESTADO_DORMIDO,  check_alarma,     ESTADO_DORMIDO,  accion_procesar_alarma},
    {ESTADO_DORMIDO,  check_toca_medir, ESTADO_MIDIENDO, accion_pedir_lectura},
    {ESTADO_MIDIENDO, check_i2c_listo,  ESTADO_DORMIDO,  accion_recoger_y_enviar},
    {-1, NULL, -1, NULL}
};

/* ================================================================== */
/*  Command dispatcher                                                 */
/* ================================================================== */

void AccelFSM_ApplyCmd(accel_fsm_t* p, const config_cmd_t* cmd) {
    switch (cmd->type) {

        case CMD_SET_TIMEOUT:
            if (cmd->mode < NUMERO_ESTADOS)
                p->timeouts_ms[cmd->mode] = cmd->value;
            break;

        case CMD_SET_MODE:
            if (cmd->value < NUMERO_ESTADOS) {
                p->modo_energia_actual = (power_state_t)cmd->value;
                AccelPort_SetThreshold(p->threshold_g);
                if (p->activado && p->fsm.current_state == ESTADO_DORMIDO)
                    AccelPort_StartSleepTimer(p->timeouts_ms[p->modo_energia_actual]);
            }
            break;

        case CMD_SET_ENABLE:
            if (p->activado == (bool)cmd->value) break;
            p->activado = (bool)cmd->value;
            if (!p->activado) {
                p->events &= ~(ACCEL_EV_MEDIR | ACCEL_EV_ALARM);
                p->fsm.current_state = ESTADO_DESACTIVADO;
            } else {
                p->fsm.current_state = ESTADO_DORMIDO;
                AccelPort_SetThreshold(p->threshold_g);
                AccelPort_StartSleepTimer(p->timeouts_ms[p->modo_energia_actual]);
            }
            break;

        case CMD_SET_THRESHOLD:
            p->threshold_g = (uint8_t)cmd->value;
            AccelPort_SetThreshold(p->threshold_g);
            break;

        case CMD_SET_WINDOW:
            if (cmd->mode < NUMERO_ESTADOS)
                p->deadband_lsb[cmd->mode] = (uint8_t)cmd->value;
            break;

        case CMD_FORCE_READ:
            if (p->activado && p->fsm.current_state == ESTADO_DORMIDO)
                p->events |= ACCEL_EV_MEDIR;
            break;

        default:
            break;
    }
}

/* ================================================================== */
/*  Public API                                                         */
/* ================================================================== */

void AccelFSM_Init(accel_fsm_t*    p,
                   uint8_t         def_threshold,
                   const uint32_t* def_timeouts,
                   const uint8_t*  def_deadband,
                   uint32_t        cmd_queue_depth)
{
    fsm_init((fsm_t*)p, tabla_accel);
    p->fsm.current_state   = ESTADO_DORMIDO;
    p->activado            = true;
    p->events              = 0;
    p->modo_energia_actual = POWER_STATE_CONSERVACION;
    p->threshold_g         = def_threshold;

    for (int i = 0; i < NUMERO_ESTADOS; i++) {
        p->timeouts_ms[i]  = def_timeouts[i];
        p->deadband_lsb[i] = def_deadband[i];
    }

    memset(p->mag, 0, sizeof(p->mag));
    p->cmd_queue = Queue_Create(cmd_queue_depth, sizeof(config_cmd_t));

    // Primer sleep timer alineado al siguiente múltiplo del período.
    // Mismo patrón que BME/INA/ADC: el callback llama TriggerMedida +
    // xTaskNotify. La tarea es un loop puro de ulTaskNotifyTake.
    {
        uint32_t period = p->timeouts_ms[p->modo_energia_actual];
        uint32_t now    = AccelPort_GetTickMs();
        uint32_t rem    = now % period;
        AccelPort_StartSleepTimer(rem == 0u ? period : (period - rem));
    }
}

void AccelFSM_DrainCmdQueue(accel_fsm_t* p) {
    config_cmd_t cmd;
    while (Queue_Receive(p->cmd_queue, &cmd, QUEUE_NO_WAIT))
        AccelFSM_ApplyCmd(p, &cmd);
}

void AccelFSM_TriggerMedida(accel_fsm_t* p) { p->events |= ACCEL_EV_MEDIR; }
void AccelFSM_TriggerAlarma(accel_fsm_t* p) { p->events |= ACCEL_EV_ALARM; }
