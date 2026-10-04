/*
 * test_power_rail_manager.c - pruebas del gestor de dominios en el PC.
 *
 * Se comprueba que:
 *   - el raíl se enciende con la primera petición y se apaga cuando lo
 *     suelta el último que lo usaba;
 *   - la cuenta es correcta con varios usuarios a la vez;
 *   - el corte de emergencia apaga en el acto;
 *   - pedir un raíl que no existe no rompe nada.
 *
 * Doble: mock_power_rail_hw.h. Con _Expect se exige exactamente qué
 * encendidos y apagados llegan a los pines, y cualquier otro hace fallar
 * la prueba.
 */

#include "unity.h"
#include "power_rail_manager.h"
#include "mock_power_rail_hw.h"

void setUp(void) {
    PowerRailHW_SetState_Expect(RAIL_INTERNO,   false);
    PowerRailHW_SetState_Expect(RAIL_EXPANSION, false);
    PowerRail_Init();
}

void tearDown(void) {}

/* ------------------------------------------------------------------ */

// Cuando NADIE tenía el rail pedido y llega el primer Request,
// el hardware tiene que encenderse (no antes), y el contador a 1.
void test_PrimerRequest_HabilitaHardwareYCuentaUno(void) {
    PowerRailHW_SetState_Expect(RAIL_INTERNO, true);
    PowerRail_Request(RAIL_INTERNO);
    TEST_ASSERT_EQUAL_UINT8(1, PowerRail_GetRefCount(RAIL_INTERNO));
}

// Si dos módulos piden el mismo rail, el hardware solo se enciende
// UNA vez (en el primer Request) y el contador llega a 2.
void test_SegundoRequest_NoVuelveAEncenderHardware_YCuentaDos(void) {
    PowerRailHW_SetState_Expect(RAIL_INTERNO, true);
    PowerRail_Request(RAIL_INTERNO);
    /* Sin segundo Expect -> CMock falla si se llama a SetState de nuevo */
    PowerRail_Request(RAIL_INTERNO);
    TEST_ASSERT_EQUAL_UINT8(2, PowerRail_GetRefCount(RAIL_INTERNO));
}

// Cuando el único consumidor suelta el rail, el hardware se apaga
// y el contador vuelve a 0.
void test_Release_ConUnConsumidor_ApagaElRail(void) {
    PowerRailHW_SetState_Expect(RAIL_INTERNO, true);
    PowerRail_Request(RAIL_INTERNO);

    PowerRailHW_SetState_Expect(RAIL_INTERNO, false);
    PowerRail_Release(RAIL_INTERNO);

    TEST_ASSERT_EQUAL_UINT8(0, PowerRail_GetRefCount(RAIL_INTERNO));
}

// Si hay dos consumidores y solo uno suelta, el rail debe seguir
// encendido para el otro. El contador pasa de 2 a 1.
void test_Release_ConDosConsumidores_NoApagaEnLaPrimera(void) {
    PowerRailHW_SetState_Expect(RAIL_INTERNO, true);
    PowerRail_Request(RAIL_INTERNO);
    PowerRail_Request(RAIL_INTERNO);

    /* Sin Expect de false -> el rail sigue encendido */
    PowerRail_Release(RAIL_INTERNO);
    TEST_ASSERT_EQUAL_UINT8(1, PowerRail_GetRefCount(RAIL_INTERNO));
}

// Si alguien hace Release cuando el contador ya está a 0 (no debería
// pasar, pero por si acaso) no debe romperse nada ni tocar hardware.
void test_Release_EnCeroPrevio_NoLlamaAlHardware(void) {
    /* Refcount ya es 0 -> Release debe ser un no-op seguro */
    PowerRail_Release(RAIL_INTERNO);
    TEST_ASSERT_EQUAL_UINT8(0, PowerRail_GetRefCount(RAIL_INTERNO));
}

// El corte de emergencia apaga el rail de inmediato, da igual cuántos
// consumidores lo tenían pedido. El contador se fuerza a 0.
void test_EmergencyCutoff_ApagaRailSinImportarRefcount(void) {
    PowerRailHW_SetState_Expect(RAIL_EXPANSION, true);
    PowerRail_Request(RAIL_EXPANSION);
    PowerRail_Request(RAIL_EXPANSION);   /* refcount = 2 */

    PowerRailHW_SetState_Expect(RAIL_EXPANSION, false);
    PowerRail_EmergencyCutoff(RAIL_EXPANSION);

    TEST_ASSERT_EQUAL_UINT8(0, PowerRail_GetRefCount(RAIL_EXPANSION));
}

// Si alguien pasa un rail inválido (fuera de rango), las funciones
// deben ignorarlo sin tocar hardware ni romperse.
void test_RailInvalido_NoProduceCrashNiLlamaHardware(void) {
    /* MAX_POWER_RAILS es un valor inválido; todas las ops deben ignorarlo */
    PowerRail_Request(MAX_POWER_RAILS);
    PowerRail_Release(MAX_POWER_RAILS);
    PowerRail_EmergencyCutoff(MAX_POWER_RAILS);
    TEST_ASSERT_EQUAL_UINT8(0, PowerRail_GetRefCount(MAX_POWER_RAILS));
}
