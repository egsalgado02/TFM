#include "bme_fsm.h"
#include "payload_queue.h"
#include "bme_port.h"
#include <string.h>

// Estados de la FSM. El orden no es casualidad: la tabla de transiciones de
// abajo asume DORMIDO=0 y ahí es donde arrancamos.
enum bme_estados {
    ESTADO_DORMIDO = 0,
    ESTADO_ESPERANDO_BOOT,
    ESTADO_ESPERANDO_MEDIDA,
    ESTADO_DESACTIVADO
};

// ==================================================================
//  Guardas
//
//  Regla: las guardas son PURAS, solo miran flags. Ningún I2C aquí.
//  El que habla con el HW es el Notify correspondiente (más abajo).
// ==================================================================

static int check_toca_medir(fsm_t* m) {
    bme_fsm_t* p = (bme_fsm_t*)m;
    return (p->events & BME_EV_MEDIR) != 0;
}

static int check_boot_ok(fsm_t* m) {
    bme_fsm_t* p = (bme_fsm_t*)m;
    return (p->events & BME_EV_BOOT_OK) != 0;
}

static int check_boot_fail_reintento(fsm_t* m) {
    bme_fsm_t* p = (bme_fsm_t*)m;
    // ha fallado el arranque pero todavía nos quedan intentos
    return (p->events & BME_EV_BOOT_FAIL) && (p->intentos_arranque < BME_MAX_BOOT_TRIES);
}

static int check_boot_fail_abort(fsm_t* m) {
    bme_fsm_t* p = (bme_fsm_t*)m;
    // ha fallado el arranque y ya no quedan reintentos -> nos rendimos
    return (p->events & BME_EV_BOOT_FAIL) && (p->intentos_arranque >= BME_MAX_BOOT_TRIES);
}

static int check_data_ok(fsm_t* m) {
    bme_fsm_t* p = (bme_fsm_t*)m;
    return (p->events & BME_EV_DATA_OK) != 0;
}

static int check_data_fail_reintento(fsm_t* m) {
    bme_fsm_t* p = (bme_fsm_t*)m;
    return (p->events & BME_EV_DATA_FAIL) && (p->intentos_lectura < BME_MAX_READ_TRIES);
}

static int check_data_fail_abort(fsm_t* m) {
    bme_fsm_t* p = (bme_fsm_t*)m;
    return (p->events & BME_EV_DATA_FAIL) && (p->intentos_lectura >= BME_MAX_READ_TRIES);
}

// ==================================================================
//  Acciones
//
//  Cada acción limpia el flag que ha consumido para no re-disparar.
// ==================================================================

static void accion_encender_hardware(fsm_t* m) {
    bme_fsm_t* p = (bme_fsm_t*)m;
    p->events &= ~BME_EV_MEDIR;        // ya hemos atendido el "toca medir"
    p->intentos_arranque = 0;
    BmePort_SetPowerGPIO(true);
    BmePort_StartBootTimer();
}

static void accion_pedir_medida(fsm_t* m) {
    bme_fsm_t* p = (bme_fsm_t*)m;
    p->events &= ~BME_EV_BOOT_OK;      // consumimos el OK del arranque
    p->intentos_lectura = 0;
    BmePort_TriggerForcedReading();
    BmePort_StartMedidaTimer();
}

static void accion_reintentar_arranque(fsm_t* m) {
    bme_fsm_t* p = (bme_fsm_t*)m;
    p->events &= ~BME_EV_BOOT_FAIL;
    p->intentos_arranque++;
    BmePort_StartBootTimer();
}

static void accion_reintentar_lectura(fsm_t* m) {
    bme_fsm_t* p = (bme_fsm_t*)m;
    p->events &= ~BME_EV_DATA_FAIL;
    p->intentos_lectura++;
    BmePort_StartMedidaTimer();
}

static void accion_abortar_por_fallo(fsm_t* m) {
    bme_fsm_t* p = (bme_fsm_t*)m;
    // limpiamos cualquier flag residual antes de volver a dormir
    p->events &= ~(BME_EV_BOOT_FAIL | BME_EV_DATA_FAIL);
    BmePort_SetPowerGPIO(false);
    // Rearmo el temporizador; cuando venza, el driver llamará a
    // BmeFSM_TriggerMedida y despertará a la tarea.
    BmePort_StartSleepTimer(p->timeouts_ms[p->modo_energia_actual]);
}

static void accion_recoger_y_dormir(fsm_t* m) {
    bme_fsm_t* p = (bme_fsm_t*)m;
    p->events &= ~BME_EV_DATA_OK;      // consumimos el dato

    int16_t t = 0, h = 0, pr = 0;

    if (BmePort_ReadData(&t, &h, &pr)) {
        uint32_t now = BmePort_GetTickMs();
        uint8_t  db  = p->deadband_threshold[p->modo_energia_actual];

        // Empaquetamos las tres magnitudes para iterar en bucle
        const int16_t vals[BME_MAGNITUDES] = {t,           h,          pr};
        const uint8_t ids [BME_MAGNITUDES] = {ID_BME_TEMP, ID_BME_HUM, ID_BME_PRES};

        for (int i = 0; i < BME_MAGNITUDES; i++) {
            // sensor_should_send aplica deadband + keepalive
            if (sensor_should_send(&p->mag[i], vals[i], db, p->t_keepalive_ms, now)) {
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

    BmePort_SetPowerGPIO(false);
    // Rearmo el temporizador; cuando venza, el driver llamará a
    // BmeFSM_TriggerMedida y despertará a la tarea.
    BmePort_StartSleepTimer(p->timeouts_ms[p->modo_energia_actual]);
}

// ==================================================================
//  Tabla de transiciones
//
//  El orden de las filas marca la prioridad: en cada fire se elige la
//  PRIMERA fila cuya guarda devuelva true.
// ==================================================================

static fsm_trans_t tabla_bme[] = {
    // arranque del ciclo de medida
    {ESTADO_DORMIDO,          check_toca_medir,           ESTADO_ESPERANDO_BOOT,   accion_encender_hardware},

    // tras el delay de boot
    {ESTADO_ESPERANDO_BOOT,   check_boot_ok,              ESTADO_ESPERANDO_MEDIDA, accion_pedir_medida},
    {ESTADO_ESPERANDO_BOOT,   check_boot_fail_reintento,  ESTADO_ESPERANDO_BOOT,   accion_reintentar_arranque},
    {ESTADO_ESPERANDO_BOOT,   check_boot_fail_abort,      ESTADO_DORMIDO,          accion_abortar_por_fallo},

    // tras el tiempo de medida
    {ESTADO_ESPERANDO_MEDIDA, check_data_ok,              ESTADO_DORMIDO,          accion_recoger_y_dormir},
    {ESTADO_ESPERANDO_MEDIDA, check_data_fail_reintento,  ESTADO_ESPERANDO_MEDIDA, accion_reintentar_lectura},
    {ESTADO_ESPERANDO_MEDIDA, check_data_fail_abort,      ESTADO_DORMIDO,          accion_abortar_por_fallo},

    {-1, NULL, -1, NULL}   // centinela: fin de tabla
};

// ==================================================================
//  Aplicación de comandos llegados por LoRa
// ==================================================================

void BmeFSM_ApplyCmd(bme_fsm_t* p, const config_cmd_t* cmd) {
    switch (cmd->type) {
        case CMD_SET_TIMEOUT:
            if (cmd->mode < NUMERO_ESTADOS)
                p->timeouts_ms[cmd->mode] = cmd->value;
            break;

        case CMD_SET_MODE:
            if (cmd->value < NUMERO_ESTADOS) {
                p->modo_energia_actual = (power_state_t)cmd->value;
                // si estamos durmiendo, replanificamos al nuevo periodo
                if (p->activado &&
                    (p->fsm.current_state == ESTADO_DORMIDO ||
                     p->fsm.current_state == ESTADO_DESACTIVADO))
                    BmePort_StartSleepTimer(p->timeouts_ms[p->modo_energia_actual]);
            }
            break;

        case CMD_SET_ENABLE:
            if (p->activado == (bool)cmd->value) break;   // sin cambios
            p->activado = (bool)cmd->value;
            if (!p->activado) {
                BmePort_SetPowerGPIO(false);
                // limpiamos cualquier flag pendiente antes de desactivar
                p->events &= ~(BME_EV_MEDIR | BME_EV_BOOT_OK | BME_EV_BOOT_FAIL |
                               BME_EV_DATA_OK | BME_EV_DATA_FAIL);
                p->fsm.current_state = ESTADO_DESACTIVADO;
            } else {
                p->fsm.current_state = ESTADO_DORMIDO;
                BmePort_StartSleepTimer(p->timeouts_ms[p->modo_energia_actual]);
            }
            break;

        case CMD_SET_THRESHOLD:
            // en BME el "threshold" es el deadband por modo
            if (cmd->mode < NUMERO_ESTADOS)
                p->deadband_threshold[cmd->mode] = (uint8_t)cmd->value;
            break;

        case CMD_FORCE_READ:
            if (p->activado && p->fsm.current_state == ESTADO_DORMIDO)
                p->events |= BME_EV_MEDIR;
            break;

        case CMD_SET_WINDOW:
            // el BME no usa ventanas de tensión
            break;

        default:
            break;
    }
}

// ==================================================================
//  API pública
// ==================================================================

void BmeFSM_Init(bme_fsm_t*      p,
                 const uint32_t* def_timeouts,
                 const uint8_t*  def_deadband,
                 uint32_t        keepalive_ms,
                 uint32_t        cmd_queue_depth)
{
    fsm_init((fsm_t*)p, tabla_bme);
    p->fsm.current_state   = ESTADO_DORMIDO;
    p->activado            = true;
    p->events              = 0;
    p->intentos_arranque   = 0;
    p->intentos_lectura    = 0;
    p->modo_energia_actual = POWER_STATE_CONSERVACION;
    p->t_keepalive_ms      = keepalive_ms;

    for (int i = 0; i < NUMERO_ESTADOS; i++) {
        p->timeouts_ms[i]        = def_timeouts[i];
        p->deadband_threshold[i] = def_deadband[i];
    }

    memset(p->mag, 0, sizeof(p->mag));
    p->cmd_queue = Queue_Create(cmd_queue_depth, sizeof(config_cmd_t));

    // Arranca el primer sleep timer alineado al siguiente múltiplo del
    // período. Dos sensores con el mismo período inicializados con unos
    // milisegundos de diferencia despertarán a la vez en el primer ciclo
    // y permanecerán en fase de ahí en adelante.
    {
        uint32_t period = p->timeouts_ms[p->modo_energia_actual];
        uint32_t now    = BmePort_GetTickMs();
        uint32_t rem    = now % period;
        BmePort_StartSleepTimer(rem == 0u ? period : (period - rem));
    }
}

void BmeFSM_DrainCmdQueue(bme_fsm_t* p) {
    // vaciamos toda la cola, aplicando cada comando en orden
    config_cmd_t cmd;
    while (Queue_Receive(p->cmd_queue, &cmd, QUEUE_NO_WAIT))
        BmeFSM_ApplyCmd(p, &cmd);
}

void BmeFSM_TriggerMedida(bme_fsm_t* p) {
    p->events |= BME_EV_MEDIR;
}

// El timer de arranque ha expirado. Es AQUÍ donde miramos el I2C UNA vez
// y decidimos si el sensor está vivo (BOOT_OK) o muerto (BOOT_FAIL).
// Las guardas luego solo leen el flag, no hablan con el HW.
void BmeFSM_NotifyBootTimer(bme_fsm_t* p) {
    if (BmePort_CheckStatusI2C())
        p->events |= BME_EV_BOOT_OK;
    else
        p->events |= BME_EV_BOOT_FAIL;
}

// Mismo patrón para la medida: el timer expira, miramos si ya hay dato
// y dejamos el resultado en un flag.
void BmeFSM_NotifyMedidaTimer(bme_fsm_t* p) {
    if (BmePort_IsDataReady())
        p->events |= BME_EV_DATA_OK;
    else
        p->events |= BME_EV_DATA_FAIL;
}
