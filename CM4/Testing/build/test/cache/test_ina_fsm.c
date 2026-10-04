#include "build/test/mocks/mock_rtos_iface.h"
#include "build/test/mocks/mock_power_rail_manager.h"
#include "build/test/mocks/mock_payload_queue.h"
#include "mock_ina_port.h"
#include "../App/Inc/fsm.h"
#include "../App/Inc/ina_fsm.h"
#include "/Users/eduardogonzalezsalgado/.rbenv/versions/3.2.3/lib/ruby/gems/3.2.0/gems/ceedling-0.31.1/vendor/unity/src/unity.h"




static ina_fsm_t mi_ina;



static const uint8_t HW_ID = 0;

static const uint8_t PAYLOAD_ID_CORR = 20;

static const uint8_t PAYLOAD_ID_VOLT = 21;

static const power_rail_t MY_RAIL = RAIL_EXPANSION;



static const uint32_t def_t [NUMERO_ESTADOS] = {10000, 30000, 90000};

static const uint8_t def_db_ma[NUMERO_ESTADOS] = {5, 10, 20};

static const uint8_t def_db_mv[NUMERO_ESTADOS] = {50, 50, 100};













static payload_msg_t s_msgs[8];

static int s_n_msgs;



static _Bool stub_enqueue(const payload_msg_t* msg, int num_calls) {

    (void)num_calls;

    if (s_n_msgs < 8) s_msgs[s_n_msgs++] = *msg;

    return 1;

}











static int16_t s_corriente;

static int16_t s_voltaje;

static uint32_t s_tick;



static _Bool stub_read_data(uint8_t hw_id, int16_t* c, int16_t* v, int num_calls) {

    (void)hw_id; (void)num_calls;

    *c = s_corriente;

    *v = s_voltaje;

    return 1;

}



static uint32_t stub_get_tick(int num_calls) {

    (void)num_calls;

    return s_tick;

}











static int s_check_fail_remaining;



static _Bool stub_check_con_fallo(uint8_t hw_id, int num_calls) {

    (void)hw_id; (void)num_calls;

    if (s_check_fail_remaining > 0) {

        s_check_fail_remaining--;

        return 0;

    }

    return 1;

}







void setUp(void) {

    __builtin___memset_chk (&mi_ina, 0, sizeof(ina_fsm_t), __builtin_object_size (&mi_ina, 0));

    s_n_msgs = 0;

    s_check_fail_remaining = 0;

    s_corriente = 0;

    s_voltaje = 0;

    s_tick = 0;

    Queue_Create_CMockIgnoreAndReturn(100, (queue_handle_t)0x01);





    InaPort_GetTickMs_Stub(stub_get_tick);

    InaPort_StartSleepTimer_CMockExpect(104, HW_ID, def_t[POWER_STATE_CONSERVACION]);

    InaFSM_Init(&mi_ina, HW_ID, PAYLOAD_ID_CORR, PAYLOAD_ID_VOLT,

                MY_RAIL, def_t, def_db_ma, def_db_mv, 4);

}



void tearDown(void) {}











static void ciclo_ina(int16_t corriente, int16_t voltaje, uint32_t tick) {

    s_corriente = corriente; s_voltaje = voltaje; s_tick = tick;

    s_n_msgs = 0;





    PowerRail_Request_CMockIgnore();

    InaPort_StartBootTimer_CMockIgnore();

    InaPort_TriggerForcedReading_CMockIgnore();

    InaPort_StartMedidaTimer_CMockIgnore();





    InaPort_StartSleepTimer_CMockIgnore();

    PowerRail_Release_CMockIgnore();





    InaPort_CheckStatusI2C_CMockIgnoreAndReturn(130, 1);

    InaPort_IsDataReady_CMockIgnoreAndReturn(131, 1);

    InaPort_ReadData_Stub(stub_read_data);

    InaPort_GetTickMs_Stub(stub_get_tick);





    PayloadQueue_Enqueue_Stub(stub_enqueue);



    InaFSM_TriggerMedida(&mi_ina);

    fsm_fire((fsm_t*)&mi_ina);

    InaFSM_NotifyBootTimer(&mi_ina);

    fsm_fire((fsm_t*)&mi_ina);

    InaFSM_NotifyMedidaTimer(&mi_ina);

    fsm_fire((fsm_t*)&mi_ina);

}

void test_Init_TrasMedirUsaHwIdYRailCorrectos(void) {

    InaPort_TriggerForcedReading_CMockIgnore();

    InaPort_CheckStatusI2C_CMockIgnoreAndReturn(156, 1);

    InaPort_IsDataReady_CMockIgnoreAndReturn(157, 1);

    InaPort_ReadData_Stub(stub_read_data);

    InaPort_GetTickMs_Stub(stub_get_tick);

    PayloadQueue_Enqueue_Stub(stub_enqueue);

    s_corriente = 100; s_voltaje = 3700; s_tick = 0;



    PowerRail_Request_CMockExpect(163, MY_RAIL);

    InaPort_StartBootTimer_CMockExpect(164, HW_ID);

    InaPort_StartMedidaTimer_CMockExpect(165, HW_ID);

    PowerRail_Release_CMockExpect(166, MY_RAIL);





    InaPort_StartSleepTimer_CMockExpect(169, HW_ID, def_t[POWER_STATE_CONSERVACION]);



    InaFSM_TriggerMedida(&mi_ina);

    fsm_fire((fsm_t*)&mi_ina);

    InaFSM_NotifyBootTimer(&mi_ina);

    fsm_fire((fsm_t*)&mi_ina);

    InaFSM_NotifyMedidaTimer(&mi_ina);

    fsm_fire((fsm_t*)&mi_ina);

}

void test_PrimeraMedida_EnviaCorrienteYVoltaje(void) {

    ciclo_ina(500, 3700, 0);

    UnityAssertEqualNumber((UNITY_INT)((2)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(188), UNITY_DISPLAY_STYLE_INT);





    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((PAYLOAD_ID_CORR)), (UNITY_INT)(UNITY_UINT8 )((s_msgs[0].sensor_id)), (((void*)0)), (UNITY_UINT)(191), UNITY_DISPLAY_STYLE_UINT8);

    UnityAssertEqualNumber((UNITY_INT)((MOTIVO_ROUTINE)), (UNITY_INT)((s_msgs[0].motivo)), (((void*)0)), (UNITY_UINT)(192), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT16)((500)), (UNITY_INT)(UNITY_INT16)((s_msgs[0].valor)), (((void*)0)), (UNITY_UINT)(193), UNITY_DISPLAY_STYLE_INT16);





    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((PAYLOAD_ID_VOLT)), (UNITY_INT)(UNITY_UINT8 )((s_msgs[1].sensor_id)), (((void*)0)), (UNITY_UINT)(196), UNITY_DISPLAY_STYLE_UINT8);

    UnityAssertEqualNumber((UNITY_INT)((MOTIVO_ROUTINE)), (UNITY_INT)((s_msgs[1].motivo)), (((void*)0)), (UNITY_UINT)(197), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT16)((3700)), (UNITY_INT)(UNITY_INT16)((s_msgs[1].valor)), (((void*)0)), (UNITY_UINT)(198), UNITY_DISPLAY_STYLE_INT16);

}















void test_SegundaMedida_AmbosDentroDeadband_NoEnvia(void) {

    ciclo_ina(500, 3700, 0);





    ciclo_ina(503, 3720, 1000);

    UnityAssertEqualNumber((UNITY_INT)((0)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(212), UNITY_DISPLAY_STYLE_INT);

}







void test_CorrienteFueraDeadband_SoloEnviaCorreinte(void) {

    ciclo_ina(500, 3700, 0);

    ciclo_ina(600, 3710, 1000);

    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(220), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((PAYLOAD_ID_CORR)), (UNITY_INT)(UNITY_UINT8 )((s_msgs[0].sensor_id)), (((void*)0)), (UNITY_UINT)(221), UNITY_DISPLAY_STYLE_UINT8);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT16)((600)), (UNITY_INT)(UNITY_INT16)((s_msgs[0].valor)), (((void*)0)), (UNITY_UINT)(222), UNITY_DISPLAY_STYLE_INT16);

}















void test_VoltajeFueraDeadband_SoloEnviaVoltaje(void) {

    ciclo_ina(500, 3700, 0);

    ciclo_ina(504, 3800, 1000);

    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(234), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((PAYLOAD_ID_VOLT)), (UNITY_INT)(UNITY_UINT8 )((s_msgs[0].sensor_id)), (((void*)0)), (UNITY_UINT)(235), UNITY_DISPLAY_STYLE_UINT8);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT16)((3800)), (UNITY_INT)(UNITY_INT16)((s_msgs[0].valor)), (((void*)0)), (UNITY_UINT)(236), UNITY_DISPLAY_STYLE_INT16);

}















void test_Keepalive_EnviaAmbosTras24h(void) {

    ciclo_ina(500, 3700, 0);

    ciclo_ina(500, 3700, (20UL * 1000UL) + 1);

    UnityAssertEqualNumber((UNITY_INT)((2)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(248), UNITY_DISPLAY_STYLE_INT);

}















void test_BootRetry_ExitoEnSegundoIntento_EntregaPayloads(void) {

    s_check_fail_remaining = 1;

    s_corriente = 750; s_voltaje = 3650; s_tick = 0; s_n_msgs = 0;



    PowerRail_Request_CMockIgnore();

    PowerRail_Release_CMockIgnore();

    InaPort_StartBootTimer_CMockIgnore();

    InaPort_TriggerForcedReading_CMockIgnore();

    InaPort_StartMedidaTimer_CMockIgnore();

    InaPort_StartSleepTimer_CMockIgnore();

    InaPort_CheckStatusI2C_Stub(stub_check_con_fallo);

    InaPort_IsDataReady_CMockIgnoreAndReturn(268, 1);

    InaPort_ReadData_Stub(stub_read_data);

    InaPort_GetTickMs_Stub(stub_get_tick);

    PayloadQueue_Enqueue_Stub(stub_enqueue);



    InaFSM_TriggerMedida(&mi_ina);

    fsm_fire((fsm_t*)&mi_ina);

    InaFSM_NotifyBootTimer(&mi_ina);

    fsm_fire((fsm_t*)&mi_ina);

    InaFSM_NotifyBootTimer(&mi_ina);

    fsm_fire((fsm_t*)&mi_ina);

    InaFSM_NotifyMedidaTimer(&mi_ina);

    fsm_fire((fsm_t*)&mi_ina);



    UnityAssertEqualNumber((UNITY_INT)((2)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(282), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT16)((750)), (UNITY_INT)(UNITY_INT16)((s_msgs[0].valor)), (((void*)0)), (UNITY_UINT)(283), UNITY_DISPLAY_STYLE_INT16);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT16)((3650)), (UNITY_INT)(UNITY_INT16)((s_msgs[1].valor)), (((void*)0)), (UNITY_UINT)(284), UNITY_DISPLAY_STYLE_INT16);

}





void test_BootAbort_TrasMaxIntentos_NoEnviaPayload(void) {

    s_n_msgs = 0;

    InaPort_CheckStatusI2C_CMockIgnoreAndReturn(290, 0);

    PowerRail_Request_CMockIgnore();

    PowerRail_Release_CMockIgnore();

    InaPort_StartBootTimer_CMockIgnore();

    InaPort_StartSleepTimer_CMockIgnore();

    PayloadQueue_Enqueue_Stub(stub_enqueue);



    InaFSM_TriggerMedida(&mi_ina);

    fsm_fire((fsm_t*)&mi_ina);



    for (int i = 0; i <= 3; i++) {

        InaFSM_NotifyBootTimer(&mi_ina);

        fsm_fire((fsm_t*)&mi_ina);

    }



    UnityAssertEqualNumber((UNITY_INT)((0)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(305), UNITY_DISPLAY_STYLE_INT);

}















void test_DisableDuranteMedida_LiberaElRail(void) {

    InaFSM_TriggerMedida(&mi_ina);

    PowerRail_Request_CMockExpect(316, MY_RAIL);

    InaPort_StartBootTimer_CMockIgnore();

    fsm_fire((fsm_t*)&mi_ina);



    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_ENABLE;

    cmd.value = 0;



    PowerRail_Release_CMockExpect(324, MY_RAIL);

    InaFSM_ApplyCmd(&mi_ina, &cmd);

}















void test_ApplyCmd_SetMode_ReprogramaTimerConElTimeoutDelNuevoModo(void) {

    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_MODE;

    cmd.value = POWER_STATE_EMERGENCIA;



    InaPort_StartSleepTimer_CMockExpect(339, HW_ID, def_t[POWER_STATE_EMERGENCIA]);

    InaFSM_ApplyCmd(&mi_ina, &cmd);

}
