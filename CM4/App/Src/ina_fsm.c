#include "ina_fsm.h"
#include "payload_queue.h"
#include "ina_port.h"
#include "power_rail_manager.h"
#include <string.h>

enum ina_estados {
    ESTADO_DORMIDO = 0,
    ESTADO_ESPERANDO_BOOT,
    ESTADO_ESPERANDO_MEDIDA,
    ESTADO_DESACTIVADO
};

// ==================================================================
//  Guardas: todas puras, solo leen flags. Los flags OK/FAIL los
//  pone el Notify correspondiente cuando consulta el HW.
// ==================================================================

static int check_toca_medir(fsm_t* m) {
    ina_fsm_t* p = (ina_fsm_t*)m;
    return (p->events & INA_EV_MEDIR) != 0;
}

static int check_boot_ok(fsm_t* m) {
    ina_fsm_t* p = (ina_fsm_t*)m;
    return (p->events & INA_EV_BOOT_OK) != 0;
}

static int check_boot_fail_reintento(fsm_t* m) {
    ina_fsm_t* p = (ina_fsm_t*)m;
    return (p->events & INA_EV_BOOT_FAIL) && (p->intentos_arranque < INA_MAX_BOOT_TRIES);
}

static int check_boot_fail_abort(fsm_t* m) {
    ina_fsm_t* p = (ina_fsm_t*)m;
    return (p->events & INA_EV_BOOT_FAIL) && (p->intentos_arranque >= INA_MAX_BOOT_TRIES);
}

static int check_data_ok(fsm_t* m) {
    ina_fsm_t* p = (ina_fsm_t*)m;
    return (p->events & INA_EV_DATA_OK) != 0;
}

static int check_data_fail_reintento(fsm_t* m) {
    ina_fsm_t* p = (ina_fsm_t*)m;
    return (p->events & INA_EV_DATA_FAIL) && (p->intentos_lectura < INA_MAX_READ_TRIES);
}

static int check_data_fail_abort(fsm_t* m) {
    ina_fsm_t* p = (ina_fsm_t*)m;
    return (p->events & INA_EV_DATA_FAIL) && (p->intentos_lectura >= INA_MAX_READ_TRIES);
}

// ==================================================================
//  Acciones: cada una limpia el flag que ha consumido
// ==================================================================

static void accion_encender_hardware(fsm_t* m) {
    ina_fsm_t* p = (ina_fsm_t*)m;
    p->events &= ~INA_EV_MEDIR;
    p->intentos_arranque = 0;
    PowerRail_Request(p->rail);          // pedimos el rail antes de hablar con el sensor
    InaPort_StartBootTimer(p->hw_id);
}

static void accion_pedir_medida(fsm_t* m) {
    ina_fsm_t* p = (ina_fsm_t*)m;
    p->events &= ~INA_EV_BOOT_OK;
    p->intentos_lectura = 0;
    InaPort_TriggerForcedReading(p->hw_id);
    InaPort_StartMedidaTimer(p->hw_id);
}

static void accion_reintentar_arranque(fsm_t* m) {
    ina_fsm_t* p = (ina_fsm_t*)m;
    p->events &= ~INA_EV_BOOT_FAIL;
    p->intentos_arranque++;
    InaPort_StartBootTimer(p->hw_id);
}

static void accion_reintentar_lectura(fsm_t* m) {
    ina_fsm_t* p = (ina_fsm_t*)m;
    p->events &= ~INA_EV_DATA_FAIL;
    p->intentos_lectura++;
    InaPort_StartMedidaTimer(p->hw_id);
}

static void accion_abortar_por_fallo(fsm_t* m) {
    ina_fsm_t* p = (ina_fsm_t*)m;
    p->events &= ~(INA_EV_BOOT_FAIL | INA_EV_DATA_FAIL);
    PowerRail_Release(p->rail);          // soltamos el rail al abortar
    // Rearmo el temporizador; cuando venza, el driver llamará a
    // InaFSM_TriggerMedida y despertará a la tarea.
    InaPort_StartSleepTimer(p->hw_id, p->timeouts_ms[p->modo_actual]);
}

static void accion_recoger_y_dormir(fsm_t* m) {
    ina_fsm_t* p = (ina_fsm_t*)m;
    p->events &= ~INA_EV_DATA_OK;

    int16_t intensidad = 0;
    int16_t voltaje    = 0;

    if (InaPort_ReadData(p->hw_id, &intensidad, &voltaje)) {
        uint32_t now   = InaPort_GetTickMs();
        uint8_t  db_ma = p->deadband_ma[p->modo_actual];
        uint8_t  db_mv = p->deadband_mv[p->modo_actual];

        if (sensor_should_send(&p->mag[INA_MAG_CORRIENTE], intensidad, db_ma, INA_KEEPALIVE_MS, now)) {
            payload_msg_t msg = {
                .sensor_id    = p->payload_id_corriente,
                .motivo       = MOTIVO_ROUTINE,
                .valor        = intensidad,
                .emerg_codigo = EMERG_NONE
            };
            PayloadQueue_Enqueue(&msg);
            p->mag[INA_MAG_CORRIENTE].last_sent         = intensidad;
            p->mag[INA_MAG_CORRIENTE].tick_last_sent_ms = now;
            p->mag[INA_MAG_CORRIENTE].ever_sent         = true;
        }

        if (sensor_should_send(&p->mag[INA_MAG_VOLTAJE], voltaje, db_mv, INA_KEEPALIVE_MS, now)) {
            payload_msg_t msg = {
                .sensor_id    = p->payload_id_voltaje,
                .motivo       = MOTIVO_ROUTINE,
                .valor        = voltaje,
                .emerg_codigo = EMERG_NONE
            };
            PayloadQueue_Enqueue(&msg);
            p->mag[INA_MAG_VOLTAJE].last_sent         = voltaje;
            p->mag[INA_MAG_VOLTAJE].tick_last_sent_ms = now;
            p->mag[INA_MAG_VOLTAJE].ever_sent         = true;
        }
    }

    PowerRail_Release(p->rail);
    // Rearmo el temporizador; cuando venza, el driver llamará a
    // InaFSM_TriggerMedida y despertará a la tarea.
    InaPort_StartSleepTimer(p->hw_id, p->timeouts_ms[p->modo_actual]);
}

// ==================================================================
//  Tabla de transiciones
// ==================================================================

static fsm_trans_t tabla_ina[] = {
    {ESTADO_DORMIDO,          check_toca_medir,           ESTADO_ESPERANDO_BOOT,   accion_encender_hardware},

    {ESTADO_ESPERANDO_BOOT,   check_boot_ok,              ESTADO_ESPERANDO_MEDIDA, accion_pedir_medida},
    {ESTADO_ESPERANDO_BOOT,   check_boot_fail_reintento,  ESTADO_ESPERANDO_BOOT,   accion_reintentar_arranque},
    {ESTADO_ESPERANDO_BOOT,   check_boot_fail_abort,      ESTADO_DORMIDO,          accion_abortar_por_fallo},

    {ESTADO_ESPERANDO_MEDIDA, check_data_ok,              ESTADO_DORMIDO,          accion_recoger_y_dormir},
    {ESTADO_ESPERANDO_MEDIDA, check_data_fail_reintento,  ESTADO_ESPERANDO_MEDIDA, accion_reintentar_lectura},
    {ESTADO_ESPERANDO_MEDIDA, check_data_fail_abort,      ESTADO_DORMIDO,          accion_abortar_por_fallo},

    {-1, NULL, -1, NULL}
};

// ==================================================================
//  Aplicación de comandos
// ==================================================================

void InaFSM_ApplyCmd(ina_fsm_t* p, const config_cmd_t* cmd) {
    switch (cmd->type) {
        case CMD_SET_TIMEOUT:
            if (cmd->mode < NUMERO_ESTADOS)
                p->timeouts_ms[cmd->mode] = cmd->value;
            break;

        case CMD_SET_MODE:
            if (cmd->value < NUMERO_ESTADOS) {
                p->modo_actual = (power_state_t)cmd->value;
                if (p->activado && p->fsm.current_state == ESTADO_DORMIDO)
                    InaPort_StartSleepTimer(p->hw_id, p->timeouts_ms[p->modo_actual]);
            }
            break;

        case CMD_SET_ENABLE:
            if (p->activado == (bool)cmd->value) break;
            p->activado = (bool)cmd->value;
            if (!p->activado) {
                // si nos pillan a media medida, hay que soltar el rail
                // o se queda encendido para siempre
                if (p->fsm.current_state == ESTADO_ESPERANDO_BOOT ||
                    p->fsm.current_state == ESTADO_ESPERANDO_MEDIDA)
                    PowerRail_Release(p->rail);
                p->events &= ~(INA_EV_MEDIR | INA_EV_BOOT_OK | INA_EV_BOOT_FAIL |
                               INA_EV_DATA_OK | INA_EV_DATA_FAIL);
                p->fsm.current_state = ESTADO_DESACTIVADO;
            } else {
                p->fsm.current_state = ESTADO_DORMIDO;
                InaPort_StartSleepTimer(p->hw_id, p->timeouts_ms[p->modo_actual]);
            }
            break;

        case CMD_SET_THRESHOLD:
            // en INA el "threshold" es el deadband en mA por modo
            if (cmd->mode < NUMERO_ESTADOS)
                p->deadband_ma[cmd->mode] = (uint8_t)cmd->value;
            break;

        case CMD_FORCE_READ:
            if (p->activado && p->fsm.current_state == ESTADO_DORMIDO)
                p->events |= INA_EV_MEDIR;
            break;

        case CMD_SET_WINDOW:
            // el INA no usa ventanas de tensión
            break;

        default:
            break;
    }
}

// ==================================================================
//  API pública
// ==================================================================

void InaFSM_Init(ina_fsm_t*      p,
                 uint8_t         hw_id,
                 uint8_t         payload_id_corriente,
                 uint8_t         payload_id_voltaje,
                 power_rail_t    rail,
                 const uint32_t* def_timeouts,
                 const uint8_t*  def_deadband_ma,
                 const uint8_t*  def_deadband_mv,
                 uint32_t        cmd_queue_depth)
{
    fsm_init((fsm_t*)p, tabla_ina);
    p->fsm.current_state    = ESTADO_DORMIDO;
    p->activado             = true;
    p->events               = 0;
    p->intentos_arranque    = 0;
    p->intentos_lectura     = 0;
    p->hw_id                = hw_id;
    p->payload_id_corriente = payload_id_corriente;
    p->payload_id_voltaje   = payload_id_voltaje;
    p->rail                 = rail;
    p->modo_actual          = POWER_STATE_CONSERVACION;

    for (int i = 0; i < NUMERO_ESTADOS; i++) {
        p->timeouts_ms[i] = def_timeouts[i];
        p->deadband_ma[i] = def_deadband_ma[i];
        p->deadband_mv[i] = def_deadband_mv[i];
    }

    memset(p->mag, 0, sizeof(p->mag));
    p->cmd_queue = Queue_Create(cmd_queue_depth, sizeof(config_cmd_t));

    // Arranca el primer sleep timer alineado al siguiente múltiplo del
    // período, igual que hacía alinear_y_esperar en el modelo task-driven.
    {
        uint32_t period = p->timeouts_ms[p->modo_actual];
        uint32_t now    = InaPort_GetTickMs();
        uint32_t rem    = now % period;
        InaPort_StartSleepTimer(p->hw_id, rem == 0u ? period : (period - rem));
    }
}

void InaFSM_DrainCmdQueue(ina_fsm_t* p) {
    config_cmd_t cmd;
    while (Queue_Receive(p->cmd_queue, &cmd, QUEUE_NO_WAIT))
        InaFSM_ApplyCmd(p, &cmd);
}

void InaFSM_TriggerMedida(ina_fsm_t* p) {
    p->events |= INA_EV_MEDIR;
}

// El timer de boot ha expirado: aquí leemos el I2C una sola vez y
// dejamos el resultado como flag para que las guardas lo encuentren.
void InaFSM_NotifyBootTimer(ina_fsm_t* p) {
    if (InaPort_CheckStatusI2C(p->hw_id))
        p->events |= INA_EV_BOOT_OK;
    else
        p->events |= INA_EV_BOOT_FAIL;
}

// Mismo patrón para la medida.
void InaFSM_NotifyMedidaTimer(ina_fsm_t* p) {
    if (InaPort_IsDataReady(p->hw_id))
        p->events |= INA_EV_DATA_OK;
    else
        p->events |= INA_EV_DATA_FAIL;
}
