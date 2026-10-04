#include "build/test/mocks/mock_power_rail_manager.h"
#include "mock_config_module.h"
#include "../App/Inc/fsm.h"
#include "../App/Inc/power_fsm.h"
#include "/Users/eduardogonzalezsalgado/.rbenv/versions/3.2.3/lib/ruby/gems/3.2.0/gems/ceedling-0.31.1/vendor/unity/src/unity.h"




static power_fsm_t mi_power;



void setUp(void) {

    __builtin___memset_chk (&mi_power, 0, sizeof(power_fsm_t), __builtin_object_size (&mi_power, 0));





    Config_ApplySystemMode_CMockExpect(27, POWER_STATE_CONSERVACION);

    PowerFSMInit(&mi_power, POWER_STATE_CONSERVACION);

}



void tearDown(void) {}

void test_VoltajeBajoUmbralEmergencia_CortaRailYAvisaModoEmergencia(void) {



    PowerManager_PushVoltaje(&mi_power, 2500);





    PowerRail_EmergencyCutoff_CMockExpect(45, RAIL_EXPANSION);

    Config_ApplySystemMode_CMockExpect(46, POWER_STATE_EMERGENCIA);

    fsm_fire((fsm_t*)&mi_power);

}









void test_VoltajeBajoUmbralConservacion_AvisaModoConservacion(void) {



    PowerManager_PushConexionBateria(&mi_power, 1);

    Config_ApplySystemMode_CMockExpect(56, POWER_STATE_ABUNDANCIA);

    fsm_fire((fsm_t*)&mi_power);





    PowerManager_PushConexionBateria(&mi_power, 0);

    PowerManager_PushVoltaje(&mi_power, 2900);

    Config_ApplySystemMode_CMockExpect(62, POWER_STATE_CONSERVACION);

    fsm_fire((fsm_t*)&mi_power);

}

void test_RecuperacionDeEmergencia_RequiereSuperarUmbralMasHisteresis(void) {



    PowerManager_PushVoltaje(&mi_power, 2500);

    PowerRail_EmergencyCutoff_CMockExpect(76, RAIL_EXPANSION);

    Config_ApplySystemMode_CMockExpect(77, POWER_STATE_EMERGENCIA);

    fsm_fire((fsm_t*)&mi_power);









    PowerManager_PushVoltaje(&mi_power, 2700);

    fsm_fire((fsm_t*)&mi_power);





    PowerManager_PushVoltaje(&mi_power, 2810);

    Config_ApplySystemMode_CMockExpect(88, POWER_STATE_CONSERVACION);

    fsm_fire((fsm_t*)&mi_power);

}







void test_RecuperacionDeConservacion_AAbundancia_RequiereHisteresis(void) {



    PowerManager_PushVoltaje(&mi_power, 3200);

    fsm_fire((fsm_t*)&mi_power);



    PowerManager_PushVoltaje(&mi_power, 3250);

    Config_ApplySystemMode_CMockExpect(100, POWER_STATE_ABUNDANCIA);

    fsm_fire((fsm_t*)&mi_power);

}















void test_BateriaExternaConectada_AvisaAbundancia(void) {

    PowerManager_PushConexionBateria(&mi_power, 1);

    Config_ApplySystemMode_CMockExpect(112, POWER_STATE_ABUNDANCIA);

    fsm_fire((fsm_t*)&mi_power);

}

void test_ActualizarUmbrales_CambiaElComportamientoSiguiente(void) {



    PowerManager_PushBatteryThresholds(&mi_power, 2700, 3500);

    fsm_fire((fsm_t*)&mi_power);











    PowerManager_PushVoltaje(&mi_power, 3700);

    Config_ApplySystemMode_CMockExpect(134, POWER_STATE_ABUNDANCIA);

    fsm_fire((fsm_t*)&mi_power);

}
