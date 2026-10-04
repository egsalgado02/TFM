#include "build/test/mocks/mock_rtos_iface.h"
#include "build/test/mocks/mock_payload_queue.h"
#include "mock_bme_port.h"
#include "../App/Inc/fsm.h"
#include "../App/Inc/bme_fsm.h"
#include "/Users/eduardogonzalezsalgado/.rbenv/versions/3.2.3/lib/ruby/gems/3.2.0/gems/ceedling-0.31.1/vendor/unity/src/unity.h"




static bme_fsm_t mi_bme;



static const uint32_t def_timeouts[NUMERO_ESTADOS] = {30000, 60000, 180000};

static const uint8_t def_deadband[NUMERO_ESTADOS] = {3, 5, 10};













static payload_msg_t s_msgs[8];

static int s_n_msgs;



static _Bool stub_enqueue(const payload_msg_t* msg, int num_calls) {

    (void)num_calls;

    if (s_n_msgs < 8) s_msgs[s_n_msgs++] = *msg;

    return 1;

}











static int16_t s_temp, s_hum, s_pres;

static uint32_t s_tick;



static _Bool stub_read_data(int16_t* t, int16_t* h, int16_t* pr, int num_calls) {

    (void)num_calls;

    *t = s_temp; *h = s_hum; *pr = s_pres;

    return 1;

}



static uint32_t stub_get_tick(int num_calls) {

    (void)num_calls;

    return s_tick;

}













static int s_check_fail_remaining;



static _Bool stub_check_con_fallo(int num_calls) {

    (void)num_calls;

    if (s_check_fail_remaining > 0) {

        s_check_fail_remaining--;

        return 0;

    }

    return 1;

}







void setUp(void) {

    __builtin___memset_chk (&mi_bme, 0, sizeof(bme_fsm_t), __builtin_object_size (&mi_bme, 0));

    s_n_msgs = 0;

    s_check_fail_remaining = 0;

    s_tick = 0;

    Queue_Create_CMockIgnoreAndReturn(88, (queue_handle_t)0x01);





    BmePort_GetTickMs_Stub(stub_get_tick);

    BmePort_StartSleepTimer_CMockExpect(92, def_timeouts[POWER_STATE_CONSERVACION]);

    BmeFSM_Init(&mi_bme, def_timeouts, def_deadband, (20UL * 1000UL), 4);

}



void tearDown(void) {}













static void ciclo_bme(int16_t t, int16_t h, int16_t pr, uint32_t tick) {

    s_temp = t; s_hum = h; s_pres = pr; s_tick = tick;

    s_n_msgs = 0;





    BmePort_SetPowerGPIO_CMockIgnore();

    BmePort_StartBootTimer_CMockIgnore();

    BmePort_TriggerForcedReading_CMockIgnore();

    BmePort_StartMedidaTimer_CMockIgnore();





    BmePort_StartSleepTimer_CMockIgnore();





    BmePort_CheckStatusI2C_CMockIgnoreAndReturn(117, 1);

    BmePort_IsDataReady_CMockIgnoreAndReturn(118, 1);

    BmePort_ReadData_Stub(stub_read_data);

    BmePort_GetTickMs_Stub(stub_get_tick);





    PayloadQueue_Enqueue_Stub(stub_enqueue);



    BmeFSM_TriggerMedida(&mi_bme);

    fsm_fire((fsm_t*)&mi_bme);

    BmeFSM_NotifyBootTimer(&mi_bme);

    fsm_fire((fsm_t*)&mi_bme);

    BmeFSM_NotifyMedidaTimer(&mi_bme);

    fsm_fire((fsm_t*)&mi_bme);

}

void test_Init_ModoDefectoEsConservacion(void) {

    config_cmd_t cmd;

    __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_MODE;

    cmd.value = POWER_STATE_CONSERVACION;







    BmePort_StartSleepTimer_CMockExpect(153, def_timeouts[POWER_STATE_CONSERVACION]);

    BmeFSM_ApplyCmd(&mi_bme, &cmd);

}







void test_CicloCompleto_PrimeraMedida_EnviaTresMagnitudes(void) {



    ciclo_bme(200, 500, 1013, 0);

    UnityAssertEqualNumber((UNITY_INT)((3)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(162), UNITY_DISPLAY_STYLE_INT);

}







void test_SegundaMedidaIgual_NoEnviaPorDeadband(void) {

    ciclo_bme(200, 500, 1013, 0);

    ciclo_bme(200, 500, 1013, 1000);

    UnityAssertEqualNumber((UNITY_INT)((0)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(170), UNITY_DISPLAY_STYLE_INT);

}







void test_CambioSuperiorADeadband_EnviaLaMagnitudAfectada(void) {

    ciclo_bme(200, 500, 1013, 0);



    ciclo_bme(210, 500, 1013, 1000);

    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(179), UNITY_DISPLAY_STYLE_INT);

}









void test_Keepalive_EnviaAunqueSinCambioAlPasar24h(void) {

    ciclo_bme(200, 500, 1013, 0);

    ciclo_bme(200, 500, 1013, (20UL * 1000UL) + 1);

    UnityAssertEqualNumber((UNITY_INT)((3)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(188), UNITY_DISPLAY_STYLE_INT);

}

void test_BootRetry_ExitoEnSegundoIntento_EntregaPayloadCorrectamente(void) {



    s_check_fail_remaining = 1;

    s_temp = 200; s_hum = 500; s_pres = 1013; s_tick = 0; s_n_msgs = 0;



    BmePort_SetPowerGPIO_CMockIgnore();

    BmePort_StartBootTimer_CMockIgnore();

    BmePort_TriggerForcedReading_CMockIgnore();

    BmePort_StartMedidaTimer_CMockIgnore();

    BmePort_StartSleepTimer_CMockIgnore();

    BmePort_CheckStatusI2C_Stub(stub_check_con_fallo);

    BmePort_IsDataReady_CMockIgnoreAndReturn(209, 1);

    BmePort_ReadData_Stub(stub_read_data);

    BmePort_GetTickMs_Stub(stub_get_tick);

    PayloadQueue_Enqueue_Stub(stub_enqueue);



    BmeFSM_TriggerMedida(&mi_bme);

    fsm_fire((fsm_t*)&mi_bme);

    BmeFSM_NotifyBootTimer(&mi_bme);

    fsm_fire((fsm_t*)&mi_bme);

    BmeFSM_NotifyBootTimer(&mi_bme);

    fsm_fire((fsm_t*)&mi_bme);

    BmeFSM_NotifyMedidaTimer(&mi_bme);

    fsm_fire((fsm_t*)&mi_bme);





    UnityAssertEqualNumber((UNITY_INT)((3)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(224), UNITY_DISPLAY_STYLE_INT);

}









void test_BootAbort_TrasMaxIntentos_NoEnviaPayload(void) {



    s_n_msgs = 0;

    BmePort_CheckStatusI2C_CMockIgnoreAndReturn(233, 0);

    BmePort_SetPowerGPIO_CMockIgnore();

    BmePort_StartBootTimer_CMockIgnore();

    BmePort_StartSleepTimer_CMockIgnore();

    PayloadQueue_Enqueue_Stub(stub_enqueue);



    BmeFSM_TriggerMedida(&mi_bme);

    fsm_fire((fsm_t*)&mi_bme);





    for (int i = 0; i <= 3; i++) {

        BmeFSM_NotifyBootTimer(&mi_bme);

        fsm_fire((fsm_t*)&mi_bme);

    }





    UnityAssertEqualNumber((UNITY_INT)((0)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(249), UNITY_DISPLAY_STYLE_INT);

}

void test_ApplyCmd_SetMode_ReprogramaTimerConElTimeoutDelNuevoModo(void) {

    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_MODE;

    cmd.value = POWER_STATE_ABUNDANCIA;







    BmePort_StartSleepTimer_CMockExpect(266, def_timeouts[POWER_STATE_ABUNDANCIA]);

    BmeFSM_ApplyCmd(&mi_bme, &cmd);

}









void test_ApplyCmd_Disable_ApagaGPIOYBloqueaElSensor(void) {

    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_ENABLE;

    cmd.value = 0;





    BmePort_SetPowerGPIO_CMockExpect(279, 0);

    BmeFSM_ApplyCmd(&mi_bme, &cmd);







    BmeFSM_TriggerMedida(&mi_bme);

    fsm_fire((fsm_t*)&mi_bme);

}









void test_ApplyCmd_ForceRead_ProvocaUnCicloDeMedida(void) {





    BmePort_SetPowerGPIO_CMockIgnore();

    BmePort_StartBootTimer_CMockIgnore();

    BmePort_TriggerForcedReading_CMockIgnore();

    BmePort_StartMedidaTimer_CMockIgnore();

    BmePort_StartSleepTimer_CMockIgnore();

    BmePort_CheckStatusI2C_CMockIgnoreAndReturn(299, 1);

    BmePort_IsDataReady_CMockIgnoreAndReturn(300, 1);

    BmePort_ReadData_Stub(stub_read_data);

    BmePort_GetTickMs_Stub(stub_get_tick);

    PayloadQueue_Enqueue_Stub(stub_enqueue);

    s_temp = 100; s_hum = 50; s_pres = 1010; s_tick = 0; s_n_msgs = 0;



    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_FORCE_READ;

    BmeFSM_ApplyCmd(&mi_bme, &cmd);



    fsm_fire((fsm_t*)&mi_bme);

    BmeFSM_NotifyBootTimer(&mi_bme);

    fsm_fire((fsm_t*)&mi_bme);

    BmeFSM_NotifyMedidaTimer(&mi_bme);

    fsm_fire((fsm_t*)&mi_bme);





    UnityAssertEqualNumber((UNITY_INT)((3)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(317), UNITY_DISPLAY_STYLE_INT);

}
