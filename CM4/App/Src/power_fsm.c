#include "power_fsm.h"
#include <stddef.h>
#include "config_module.h"
#include "power_rail_manager.h"

// Histéresis para evitar oscilaciones cuando recupero modo:
// si el umbral es 2700, para subir de EMERGENCIA a CONSERVACION
// necesito > 2800. Así no me paso saltando de modo en cada milivoltio.
#define HISTERESIS_MV 100

static int condicion_bateria_presente(fsm_t* m) {
    power_fsm_t* p = (power_fsm_t*)m;
    return (p->bateria_externa_conectada == true);
}

static int condicion_caer_a_emergencia(fsm_t* m) {
    power_fsm_t* p = (power_fsm_t*)m;
    return (!p->bateria_externa_conectada &&
            p->voltaje_actual_mv < p->umbral_emergencia_mv);
}

static int condicion_caer_a_conservacion(fsm_t* m) {
    power_fsm_t* p = (power_fsm_t*)m;
    return (!p->bateria_externa_conectada &&
            p->voltaje_actual_mv < p->umbral_conservacion_mv);
}

static int condicion_recuperacion_a_conservacion(fsm_t* m) {
    power_fsm_t* p = (power_fsm_t*)m;
    return (!p->bateria_externa_conectada &&
            p->voltaje_actual_mv > (p->umbral_emergencia_mv + HISTERESIS_MV));
}

static int condicion_recuperacion_a_abundancia(fsm_t* m) {
    power_fsm_t* p = (power_fsm_t*)m;
    return (!p->bateria_externa_conectada &&
            p->voltaje_actual_mv > (p->umbral_conservacion_mv + HISTERESIS_MV));
}

static int check_nuevos_umbrales(fsm_t* m) {
    power_fsm_t* p = (power_fsm_t*)m;
    return (p->events & EV_NEW_THRESHOLDS);
}

// Entrar en EMERGENCIA implica DOS cosas:
//   1) cortar el rail de expansión por seguridad
//   2) avisar a todos los sensores para que pasen a su modo de emergencia
static void accion_entrar_emergencia(fsm_t* m) {
    (void)m;
    PowerRail_EmergencyCutoff(RAIL_EXPANSION);
    Config_ApplySystemMode(POWER_STATE_EMERGENCIA);
}

static void accion_entrar_conservacion(fsm_t* m) {
    (void)m;
    Config_ApplySystemMode(POWER_STATE_CONSERVACION);
}

static void accion_entrar_abundancia(fsm_t* m) {
    (void)m;
    Config_ApplySystemMode(POWER_STATE_ABUNDANCIA);
}

static void accion_actualizar_umbrales(fsm_t* m) {
    power_fsm_t* p = (power_fsm_t*)m;
    p->events &= ~EV_NEW_THRESHOLDS;
    p->umbral_emergencia_mv   = p->rx_umbral_emergencia_mv;
    p->umbral_conservacion_mv = p->rx_umbral_conservacion_mv;
}

// Tabla de transiciones. El orden importa porque fsm_fire usa la PRIMERA
// fila que case con el estado actual + guarda en true.
static fsm_trans_t tabla_power[] = {
    // En cualquier estado, si llegan umbrales nuevos por LoRa, los aplicamos
    // (autotransición sin cambiar de modo).
    {POWER_STATE_ABUNDANCIA,   check_nuevos_umbrales, POWER_STATE_ABUNDANCIA,   accion_actualizar_umbrales},
    {POWER_STATE_CONSERVACION, check_nuevos_umbrales, POWER_STATE_CONSERVACION, accion_actualizar_umbrales},
    {POWER_STATE_EMERGENCIA,   check_nuevos_umbrales, POWER_STATE_EMERGENCIA,   accion_actualizar_umbrales},

    // Si hay batería externa conectada, siempre vamos a ABUNDANCIA.
    {POWER_STATE_CONSERVACION, condicion_bateria_presente, POWER_STATE_ABUNDANCIA, accion_entrar_abundancia},
    {POWER_STATE_EMERGENCIA,   condicion_bateria_presente, POWER_STATE_ABUNDANCIA, accion_entrar_abundancia},

    // Caídas de tensión: la emergencia tiene prioridad sobre la conservación.
    {POWER_STATE_ABUNDANCIA,   condicion_caer_a_emergencia,   POWER_STATE_EMERGENCIA,   accion_entrar_emergencia},
    {POWER_STATE_ABUNDANCIA,   condicion_caer_a_conservacion, POWER_STATE_CONSERVACION, accion_entrar_conservacion},
    {POWER_STATE_CONSERVACION, condicion_caer_a_emergencia,   POWER_STATE_EMERGENCIA,   accion_entrar_emergencia},

    // Recuperaciones (con histéresis: ver HISTERESIS_MV arriba).
    {POWER_STATE_EMERGENCIA,   condicion_recuperacion_a_conservacion, POWER_STATE_CONSERVACION, accion_entrar_conservacion},
    {POWER_STATE_CONSERVACION, condicion_recuperacion_a_abundancia,   POWER_STATE_ABUNDANCIA,   accion_entrar_abundancia},

    {-1, NULL, -1, NULL}   // centinela
};

void PowerFSMInit(power_fsm_t* p_fsm, power_state_t estado_inicial) {
    fsm_init((fsm_t*)p_fsm, tabla_power);
    p_fsm->motor.current_state    = estado_inicial;
    // Valores razonables de partida. La realidad llega con el primer
    // PushVoltaje y PushConexionBateria desde fuera.
    p_fsm->voltaje_actual_mv         = 3500;
    p_fsm->bateria_externa_conectada = false;
    /* Umbrales elegidos para las pruebas en banco, donde la placa se
     * alimenta a 3,30 V o a 3,03 V y cada tensión tiene que caer en un modo:
     *   3,30 V > 3,20 V (3100 + histéresis) -> ABUNDANCIA
     *   3,03 V < 3,10 V                     -> CONSERVACION
     *   ninguna baja de 2,70 V              -> EMERGENCIA solo si se fuerza
     * Se cambian desde el PC con set_battery_th <emerg_mV> <cons_mV>. */
    p_fsm->umbral_emergencia_mv      = 2700;
    p_fsm->umbral_conservacion_mv    = 3100;
    p_fsm->events                    = 0;
    p_fsm->rx_umbral_emergencia_mv   = 0;
    p_fsm->rx_umbral_conservacion_mv = 0;

    // Si arranco directamente en EMERGENCIA, corto el rail desde ya.
    if (estado_inicial == POWER_STATE_EMERGENCIA) {
        PowerRail_EmergencyCutoff(RAIL_EXPANSION);
    }
    // Y aviso a los sensores del modo de arranque (broadcast).
    Config_ApplySystemMode(estado_inicial);
}

void PowerManager_PushVoltaje(power_fsm_t* p_fsm, uint16_t mv) {
    p_fsm->voltaje_actual_mv = mv;
}

void PowerManager_PushConexionBateria(power_fsm_t* p_fsm, bool conectada) {
    p_fsm->bateria_externa_conectada = conectada;
}

void PowerManager_PushBatteryThresholds(power_fsm_t* p_fsm,
                                        uint16_t umbral_emergencia_mv,
                                        uint16_t umbral_conservacion_mv) {
    p_fsm->rx_umbral_emergencia_mv   = umbral_emergencia_mv;
    p_fsm->rx_umbral_conservacion_mv = umbral_conservacion_mv;
    p_fsm->events |= EV_NEW_THRESHOLDS;
}
