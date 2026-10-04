#include "mock_power_rail_hw.h"
#include "../App/Inc/power_rail_manager.h"
#include "/Users/eduardogonzalezsalgado/.rbenv/versions/3.2.3/lib/ruby/gems/3.2.0/gems/ceedling-0.31.1/vendor/unity/src/unity.h"


void setUp(void) {

    PowerRailHW_SetState_CMockExpect(21, RAIL_INTERNO, 0);

    PowerRailHW_SetState_CMockExpect(22, RAIL_EXPANSION, 0);

    PowerRail_Init();

}



void tearDown(void) {}











void test_PrimerRequest_HabilitaHardwareYCuentaUno(void) {

    PowerRailHW_SetState_CMockExpect(33, RAIL_INTERNO, 1);

    PowerRail_Request(RAIL_INTERNO);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((1)), (UNITY_INT)(UNITY_UINT8 )((PowerRail_GetRefCount(RAIL_INTERNO))), (((void*)0)), (UNITY_UINT)(35), UNITY_DISPLAY_STYLE_UINT8);

}







void test_SegundoRequest_NoVuelveAEncenderHardware_YCuentaDos(void) {

    PowerRailHW_SetState_CMockExpect(41, RAIL_INTERNO, 1);

    PowerRail_Request(RAIL_INTERNO);



    PowerRail_Request(RAIL_INTERNO);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((2)), (UNITY_INT)(UNITY_UINT8 )((PowerRail_GetRefCount(RAIL_INTERNO))), (((void*)0)), (UNITY_UINT)(45), UNITY_DISPLAY_STYLE_UINT8);

}







void test_Release_ConUnConsumidor_ApagaElRail(void) {

    PowerRailHW_SetState_CMockExpect(51, RAIL_INTERNO, 1);

    PowerRail_Request(RAIL_INTERNO);



    PowerRailHW_SetState_CMockExpect(54, RAIL_INTERNO, 0);

    PowerRail_Release(RAIL_INTERNO);



    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((0)), (UNITY_INT)(UNITY_UINT8 )((PowerRail_GetRefCount(RAIL_INTERNO))), (((void*)0)), (UNITY_UINT)(57), UNITY_DISPLAY_STYLE_UINT8);

}







void test_Release_ConDosConsumidores_NoApagaEnLaPrimera(void) {

    PowerRailHW_SetState_CMockExpect(63, RAIL_INTERNO, 1);

    PowerRail_Request(RAIL_INTERNO);

    PowerRail_Request(RAIL_INTERNO);





    PowerRail_Release(RAIL_INTERNO);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((1)), (UNITY_INT)(UNITY_UINT8 )((PowerRail_GetRefCount(RAIL_INTERNO))), (((void*)0)), (UNITY_UINT)(69), UNITY_DISPLAY_STYLE_UINT8);

}







void test_Release_EnCeroPrevio_NoLlamaAlHardware(void) {



    PowerRail_Release(RAIL_INTERNO);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((0)), (UNITY_INT)(UNITY_UINT8 )((PowerRail_GetRefCount(RAIL_INTERNO))), (((void*)0)), (UNITY_UINT)(77), UNITY_DISPLAY_STYLE_UINT8);

}







void test_EmergencyCutoff_ApagaRailSinImportarRefcount(void) {

    PowerRailHW_SetState_CMockExpect(83, RAIL_EXPANSION, 1);

    PowerRail_Request(RAIL_EXPANSION);

    PowerRail_Request(RAIL_EXPANSION);



    PowerRailHW_SetState_CMockExpect(87, RAIL_EXPANSION, 0);

    PowerRail_EmergencyCutoff(RAIL_EXPANSION);



    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((0)), (UNITY_INT)(UNITY_UINT8 )((PowerRail_GetRefCount(RAIL_EXPANSION))), (((void*)0)), (UNITY_UINT)(90), UNITY_DISPLAY_STYLE_UINT8);

}







void test_RailInvalido_NoProduceCrashNiLlamaHardware(void) {



    PowerRail_Request(MAX_POWER_RAILS);

    PowerRail_Release(MAX_POWER_RAILS);

    PowerRail_EmergencyCutoff(MAX_POWER_RAILS);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((0)), (UNITY_INT)(UNITY_UINT8 )((PowerRail_GetRefCount(MAX_POWER_RAILS))), (((void*)0)), (UNITY_UINT)(100), UNITY_DISPLAY_STYLE_UINT8);

}
