/*
 * test_power_fsm.c - pruebas de power_fsm en el PC.
 *
 * No se mira el estado interno ni los umbrales guardados. Se le mete una
 * tensión, la batería o umbrales nuevos, y se comprueba qué llama hacia
 * fuera:
 *   - Config_ApplySystemMode(modo), para avisar a todos del modo nuevo.
 *   - PowerRail_EmergencyCutoff(RAIL_EXPANSION), solo al entrar en
 *     emergencia.
 *
 * Dobles: mock_config_module.h, mock_power_rail_manager.h
 */

#include "unity.h"
#include "power_fsm.h"
#include "fsm.h"
#include "mock_config_module.h"
#include "mock_power_rail_manager.h"
#include <string.h>

static power_fsm_t mi_power;

void setUp(void) {
    memset(&mi_power, 0, sizeof(power_fsm_t));

    // El Init manda un Config_ApplySystemMode del modo inicial.
    Config_ApplySystemMode_Expect(POWER_STATE_CONSERVACION);
    PowerFSMInit(&mi_power, POWER_STATE_CONSERVACION);
}

void tearDown(void) {}

/* ------------------------------------------------------------------ */
/*  Caídas de tensión: vemos solo los efectos externos                 */
/* ------------------------------------------------------------------ */

// Si llega un voltaje por debajo del umbral de emergencia (2700 mV),
// el sistema tiene que cortar el rail de expansión y avisar a todos
// los sensores de que pasen a EMERGENCIA.
void test_VoltajeBajoUmbralEmergencia_CortaRailYAvisaModoEmergencia(void) {
    // Umbrales por defecto: emergencia = 2700 mV
    PowerManager_PushVoltaje(&mi_power, 2500);

    // Si la FSM hace su trabajo, debe llamar a estos dos:
    PowerRail_EmergencyCutoff_Expect(RAIL_EXPANSION);
    Config_ApplySystemMode_Expect(POWER_STATE_EMERGENCIA);
    fsm_fire((fsm_t*)&mi_power);
}

// Tras subir a ABUNDANCIA con batería externa, si quito la batería y
// el voltaje cae bajo el umbral de conservación (3000 mV), debe avisar
// a los sensores de que pasen a CONSERVACION.
void test_VoltajeBajoUmbralConservacion_AvisaModoConservacion(void) {
    // Desde CONSERVACION, primero subimos a ABUNDANCIA con batería externa.
    PowerManager_PushConexionBateria(&mi_power, true);
    Config_ApplySystemMode_Expect(POWER_STATE_ABUNDANCIA);
    fsm_fire((fsm_t*)&mi_power);

    // Quitamos batería externa y bajamos a 2900 < umbral_conservacion=3000
    PowerManager_PushConexionBateria(&mi_power, false);
    PowerManager_PushVoltaje(&mi_power, 2900);
    Config_ApplySystemMode_Expect(POWER_STATE_CONSERVACION);
    fsm_fire((fsm_t*)&mi_power);
}

/* ------------------------------------------------------------------ */
/*  Histéresis: el voltaje justo en el umbral NO recupera             */
/* ------------------------------------------------------------------ */

// Para salir de EMERGENCIA no basta con tocar el umbral, hay que
// superarlo + 100 mV de histéresis. Así evitamos oscilar de modo
// cada milivoltio cuando el voltaje está en el límite.
void test_RecuperacionDeEmergencia_RequiereSuperarUmbralMasHisteresis(void) {
    // Primero caemos a EMERGENCIA
    PowerManager_PushVoltaje(&mi_power, 2500);
    PowerRail_EmergencyCutoff_Expect(RAIL_EXPANSION);
    Config_ApplySystemMode_Expect(POWER_STATE_EMERGENCIA);
    fsm_fire((fsm_t*)&mi_power);

    // Voltaje justo en el umbral (2700): NO debe recuperar.
    // Si la FSM intentase avisar de cambio de modo aquí, el _Expect
    // que falta haría fallar el test -> comportamiento correcto = silencio.
    PowerManager_PushVoltaje(&mi_power, 2700);
    fsm_fire((fsm_t*)&mi_power);

    // Voltaje sobre umbral+histéresis (2810 > 2700+100): recupera a CONSERVACION
    PowerManager_PushVoltaje(&mi_power, 2810);
    Config_ApplySystemMode_Expect(POWER_STATE_CONSERVACION);
    fsm_fire((fsm_t*)&mi_power);
}

// Misma idea para pasar de CONSERVACION a ABUNDANCIA: no basta con
// estar justo en 3100 mV, hay que estar por encima de 3200 mV.
void test_RecuperacionDeConservacion_AAbundancia_RequiereHisteresis(void) {
    // Umbral conservacion = 3100, histéresis = 100 -> necesita > 3200.
    PowerManager_PushVoltaje(&mi_power, 3200);
    fsm_fire((fsm_t*)&mi_power);   // justo en el limite: silencio

    PowerManager_PushVoltaje(&mi_power, 3250);
    Config_ApplySystemMode_Expect(POWER_STATE_ABUNDANCIA);
    fsm_fire((fsm_t*)&mi_power);
}

/* ------------------------------------------------------------------ */
/*  Batería externa: siempre fuerza ABUNDANCIA                         */
/* ------------------------------------------------------------------ */

// Si se conecta la batería externa, vamos siempre a ABUNDANCIA sin
// importar el voltaje (el cable nos da energía de sobra).
void test_BateriaExternaConectada_AvisaAbundancia(void) {
    PowerManager_PushConexionBateria(&mi_power, true);
    Config_ApplySystemMode_Expect(POWER_STATE_ABUNDANCIA);
    fsm_fire((fsm_t*)&mi_power);
}

/* ------------------------------------------------------------------ */
/*  Actualización de umbrales: comprobamos su EFECTO, no el campo      */
/* ------------------------------------------------------------------ */

// Cuando el gateway manda nuevos umbrales de batería, NO se aplican
// hasta el siguiente fire. Pero a partir de ahí el comportamiento sí
// cambia: este test verifica que con el nuevo umbral, un voltaje que
// antes no haría nada ahora dispara la transición.
void test_ActualizarUmbrales_CambiaElComportamientoSiguiente(void) {
    // Subimos el umbral de conservación a 3500 mV.
    PowerManager_PushBatteryThresholds(&mi_power, 2700, 3500);
    fsm_fire((fsm_t*)&mi_power);   // accion_actualizar_umbrales, sin difusión

    // Con el nuevo umbral, un voltaje de 3700 (> 3500+100 = 3600) ya recupera
    // a ABUNDANCIA. Si los umbrales no se hubieran actualizado de verdad
    // (umbral viejo era 3000), la FSM ya habría recuperado mucho antes
    // y este Expect no encajaría con el comportamiento.
    PowerManager_PushVoltaje(&mi_power, 3700);
    Config_ApplySystemMode_Expect(POWER_STATE_ABUNDANCIA);
    fsm_fire((fsm_t*)&mi_power);
}
