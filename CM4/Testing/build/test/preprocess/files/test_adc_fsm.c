#include "build/test/mocks/mock_payload_queue.h"
#include "build/test/mocks/mock_rtos_iface.h"
#include "mock_power_rail_manager.h"
#include "mock_power_queue.h"
#include "mock_adc_port.h"
#include "../App/Inc/fsm.h"
#include "../App/Inc/adc_fsm.h"
#include "/Users/eduardogonzalezsalgado/.rbenv/versions/3.2.3/lib/ruby/gems/3.2.0/gems/ceedling-0.31.1/vendor/unity/src/unity.h"




static adc_fsm_t mi_adc;





static const uint16_t def_min[NUMERO_ESTADOS] = {3200, 3000, 2800};

static const uint16_t def_max[NUMERO_ESTADOS] = {4500, 4200, 3500};

static const uint32_t def_t [NUMERO_ESTADOS] = {30000, 60000, 120000};















static uint16_t s_voltaje_enviado;

static int s_n_envios;



static void stub_cola_power(uint16_t v, int num_calls) {

    (void)num_calls;

    s_voltaje_enviado = v;

    s_n_envios++;

}











static uint16_t s_voltaje_bateria;

static uint32_t s_tick_adc;



static _Bool stub_leer_bateria(uint16_t* p_mv, int num_calls) {

    (void)num_calls;

    *p_mv = s_voltaje_bateria;

    return 1;

}



static uint32_t stub_get_tick_adc(int num_calls) {

    (void)num_calls;

    return s_tick_adc;

}







void setUp(void) {

    __builtin___memset_chk (&mi_adc, 0, sizeof(adc_fsm_t), __builtin_object_size (&mi_adc, 0));

    s_n_envios = 0;

    s_voltaje_enviado = 0;

    s_tick_adc = 0;

    Queue_Create_CMockIgnoreAndReturn(78, (queue_handle_t)0x01);





    AdcPort_GetTickMs_Stub(stub_get_tick_adc);

    AdcPort_StartSleepTimer_CMockExpect(82, def_t[POWER_STATE_CONSERVACION]);

    AdcFSM_Init(&mi_adc, def_min, def_max, def_t, 4);

}



void tearDown(void) {}















static void run_ciclo(uint16_t voltaje, uint32_t tick) {

    s_voltaje_bateria = voltaje;

    s_tick_adc = tick;

    s_n_envios = 0;



    AdcFSM_TriggerMedida(&mi_adc);



    PowerRail_Request_CMockIgnore();

    AdcPort_LeerBateriaBloqueante_Stub(stub_leer_bateria);

    AdcPort_GetTickMs_Stub(stub_get_tick_adc);

    ColaPower_EnviarVoltaje_Stub(stub_cola_power);





    PayloadQueue_Enqueue_CMockIgnoreAndReturn(107, 1);

    PowerRail_Release_CMockIgnore();





    AdcPort_StartSleepTimer_CMockIgnore();



    fsm_fire((fsm_t*)&mi_adc);

}













void test_PrimeraLectura_SiempreEnviaAPowerQueue(void) {







    run_ciclo(3600, 0);

    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(126), UNITY_DISPLAY_STYLE_INT);

}







void test_VoltajeFueraVentana_PorDebajo_EnviaAPowerQueue(void) {

    run_ciclo(3600, 0);

    run_ciclo(2500, 1000);

    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(134), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT16)((2500)), (UNITY_INT)(UNITY_UINT16)((s_voltaje_enviado)), (((void*)0)), (UNITY_UINT)(135), UNITY_DISPLAY_STYLE_UINT16);

}







void test_VoltajeFueraVentana_PorArriba_EnviaAPowerQueue(void) {

    run_ciclo(3600, 0);

    run_ciclo(4300, 1000);

    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(143), UNITY_DISPLAY_STYLE_INT);

}









void test_VoltajeDentroVentana_NoEnviaHastaKeepalive(void) {

    run_ciclo(3600, 0);

    run_ciclo(3700, 1000);

    UnityAssertEqualNumber((UNITY_INT)((0)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(152), UNITY_DISPLAY_STYLE_INT);

}







void test_Keepalive_EnviaAunqueDentroDeVentana_AlPasar24h(void) {

    run_ciclo(3600, 0);

    run_ciclo(3600, (24UL * 60UL * 60UL * 1000UL) + 1);

    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(160), UNITY_DISPLAY_STYLE_INT);

}

void test_ApplyCmd_SetWindow_LaNuevaVentanaCambiaElComportamiento(void) {





    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_WINDOW;

    cmd.mode = POWER_STATE_CONSERVACION;

    cmd.value = 3300;

    cmd.value2 = 4000;

    AdcFSM_ApplyCmd(&mi_adc, &cmd);



    run_ciclo(3600, 0);

    run_ciclo(4100, 1000);

    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(182), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT16)((4100)), (UNITY_INT)(UNITY_UINT16)((s_voltaje_enviado)), (((void*)0)), (UNITY_UINT)(183), UNITY_DISPLAY_STYLE_UINT16);

}









void test_ApplyCmd_SetTimeout_AfectaSoloAlModoIndicado(void) {





    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_TIMEOUT;

    cmd.mode = POWER_STATE_EMERGENCIA;

    cmd.value = 300000;

    AdcFSM_ApplyCmd(&mi_adc, &cmd);



    config_cmd_t mode; __builtin___memset_chk (&mode, 0, sizeof(mode), __builtin_object_size (&mode, 0));

    mode.type = CMD_SET_MODE;

    mode.value = POWER_STATE_EMERGENCIA;

    AdcPort_StartSleepTimer_CMockExpect(201, 300000);

    AdcFSM_ApplyCmd(&mi_adc, &mode);

}







void test_ApplyCmd_SetMode_ReprogramaTimerConElTimeoutDelNuevoModo(void) {

    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_MODE;

    cmd.value = POWER_STATE_EMERGENCIA;





    AdcPort_StartSleepTimer_CMockExpect(213, def_t[POWER_STATE_EMERGENCIA]);

    AdcFSM_ApplyCmd(&mi_adc, &cmd);

}







void test_ApplyCmd_Disable_SensorNoRealizaMedidas(void) {

    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_ENABLE;

    cmd.value = 0;

    AdcFSM_ApplyCmd(&mi_adc, &cmd);





    AdcFSM_TriggerMedida(&mi_adc);

    fsm_fire((fsm_t*)&mi_adc);

}







void test_ApplyCmd_Reenable_SensorVuelveAMedir(void) {

    config_cmd_t dis; __builtin___memset_chk (&dis, 0, sizeof(dis), __builtin_object_size (&dis, 0));

    dis.type = CMD_SET_ENABLE; dis.value = 0;

    AdcFSM_ApplyCmd(&mi_adc, &dis);



    config_cmd_t en; __builtin___memset_chk (&en, 0, sizeof(en), __builtin_object_size (&en, 0));

    en.type = CMD_SET_ENABLE; en.value = 1;





    AdcPort_StartSleepTimer_CMockExpect(241, def_t[POWER_STATE_CONSERVACION]);

    AdcFSM_ApplyCmd(&mi_adc, &en);

}
