#include "mock_power_fsm.h"
#include "build/test/mocks/mock_rtos_iface.h"
#include "../App/Inc/config_module.h"
#include "/Users/eduardogonzalezsalgado/.rbenv/versions/3.2.3/lib/ruby/gems/3.2.0/gems/ceedling-0.31.1/vendor/unity/src/unity.h"


static const queue_handle_t COLA_A = (queue_handle_t)0xA1;

static const queue_handle_t COLA_B = (queue_handle_t)0xB2;

static const queue_handle_t COLA_C = (queue_handle_t)0xC3;



static power_fsm_t dummy_power;





static config_cmd_t s_ultimo_cmd;

static queue_handle_t s_ultima_cola;



static _Bool stub_capturar_cmd(queue_handle_t q, const void* item,

                              uint32_t timeout_ms, int num_calls) {

    (void)timeout_ms; (void)num_calls;

    s_ultima_cola = q;

    s_ultimo_cmd = *(const config_cmd_t*)item;

    return 1;

}



void setUp(void) {

    __builtin___memset_chk (&dummy_power, 0, sizeof(power_fsm_t), __builtin_object_size (&dummy_power, 0));

    __builtin___memset_chk (&s_ultimo_cmd, 0, sizeof(s_ultimo_cmd), __builtin_object_size (&s_ultimo_cmd, 0));

    s_ultima_cola = ((void*)0);



    ConfigModule_Init(&dummy_power);

}



void tearDown(void) {}













void test_RegisterSensor_ColaValida_DevuelveTrue(void) {

    do {if ((ConfigModule_RegisterSensor(1, COLA_A))) {} else {UnityFail( ((" Expected TRUE Was FALSE")), (UNITY_UINT)((UNITY_UINT)(59)));}} while(0);

}





void test_RegisterSensor_ColaNull_DevuelveFalse(void) {

    do {if (!(ConfigModule_RegisterSensor(1, ((void*)0)))) {} else {UnityFail( ((" Expected FALSE Was TRUE")), (UNITY_UINT)((UNITY_UINT)(64)));}} while(0);

}

void test_FrameSetTimeout_VaAlSensorYContieneMs(void) {

    ConfigModule_RegisterSensor(7, COLA_A);





    uint8_t frame[] = {0x01, 7, 1, 0x00, 0x27, 0x10};



    Queue_Send_Stub(stub_capturar_cmd);

    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));



    UnityAssertEqualNumber((UNITY_INT64)((COLA_A)), (UNITY_INT64)((s_ultima_cola)), (((void*)0)), (UNITY_UINT)(83), UNITY_DISPLAY_STYLE_HEX64);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((7)), (UNITY_INT)(UNITY_UINT8 )((s_ultimo_cmd.sensor_id)), (((void*)0)), (UNITY_UINT)(84), UNITY_DISPLAY_STYLE_UINT8);

    UnityAssertEqualNumber((UNITY_INT)((CMD_SET_TIMEOUT)), (UNITY_INT)((s_ultimo_cmd.type)), (((void*)0)), (UNITY_UINT)(85), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)((POWER_STATE_CONSERVACION)), (UNITY_INT)((s_ultimo_cmd.mode)), (((void*)0)), (UNITY_UINT)(86), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)((10000)), (UNITY_INT)(UNITY_UINT32)((s_ultimo_cmd.value)), (((void*)0)), (UNITY_UINT)(87), UNITY_DISPLAY_STYLE_UINT32);

}







void test_FrameSetThreshold_VaAlSensorYContieneThreshold(void) {

    ConfigModule_RegisterSensor(3, COLA_B);





    uint8_t frame[] = {0x02, 3, 0, 25};



    Queue_Send_Stub(stub_capturar_cmd);

    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));



    UnityAssertEqualNumber((UNITY_INT64)((COLA_B)), (UNITY_INT64)((s_ultima_cola)), (((void*)0)), (UNITY_UINT)(101), UNITY_DISPLAY_STYLE_HEX64);

    UnityAssertEqualNumber((UNITY_INT)((CMD_SET_THRESHOLD)), (UNITY_INT)((s_ultimo_cmd.type)), (((void*)0)), (UNITY_UINT)(102), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)((25)), (UNITY_INT)(UNITY_UINT32)((s_ultimo_cmd.value)), (((void*)0)), (UNITY_UINT)(103), UNITY_DISPLAY_STYLE_UINT32);

}







void test_FrameForceRead_VaAlSensorComoForceRead(void) {

    ConfigModule_RegisterSensor(10, COLA_A);



    uint8_t frame[] = {0x03, 10};

    Queue_Send_Stub(stub_capturar_cmd);

    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));



    UnityAssertEqualNumber((UNITY_INT)((CMD_FORCE_READ)), (UNITY_INT)((s_ultimo_cmd.type)), (((void*)0)), (UNITY_UINT)(115), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((10)), (UNITY_INT)(UNITY_UINT8 )((s_ultimo_cmd.sensor_id)), (((void*)0)), (UNITY_UINT)(116), UNITY_DISPLAY_STYLE_UINT8);

}







void test_FrameEnableSensor_VaConFlagDeEnable(void) {

    ConfigModule_RegisterSensor(2, COLA_A);





    uint8_t frame[] = {0x05, 2, 1};

    Queue_Send_Stub(stub_capturar_cmd);

    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));



    UnityAssertEqualNumber((UNITY_INT)((CMD_SET_ENABLE)), (UNITY_INT)((s_ultimo_cmd.type)), (((void*)0)), (UNITY_UINT)(129), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)((1)), (UNITY_INT)(UNITY_UINT32)((s_ultimo_cmd.value)), (((void*)0)), (UNITY_UINT)(130), UNITY_DISPLAY_STYLE_UINT32);

}







void test_FrameSetWindow_VaConMinYMax(void) {

    ConfigModule_RegisterSensor(8, COLA_C);





    uint8_t frame[] = {0x06, 8, 1, 0x0B, 0xB8, 0x10, 0x68};

    Queue_Send_Stub(stub_capturar_cmd);

    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));



    UnityAssertEqualNumber((UNITY_INT)((CMD_SET_WINDOW)), (UNITY_INT)((s_ultimo_cmd.type)), (((void*)0)), (UNITY_UINT)(143), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)((3000)), (UNITY_INT)(UNITY_UINT32)((s_ultimo_cmd.value)), (((void*)0)), (UNITY_UINT)(144), UNITY_DISPLAY_STYLE_UINT32);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)((4200)), (UNITY_INT)(UNITY_UINT32)((s_ultimo_cmd.value2)), (((void*)0)), (UNITY_UINT)(145), UNITY_DISPLAY_STYLE_UINT32);

}















void test_FrameSetBatteryThreshold_LlamaDirectamenteAlPowerFsm(void) {



    uint8_t frame[] = {0x04, 0x0A, 0xF0, 0x0C, 0x1C};



    PowerManager_PushBatteryThresholds_CMockExpect(158, &dummy_power, 2800, 3100);

    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));

}







void test_FrameForceSystemMode_HaceBroadcastATodosLosSensores(void) {



    ConfigModule_RegisterSensor(0x10, COLA_A);

    ConfigModule_RegisterSensor(0x20, COLA_B);

    ConfigModule_RegisterSensor(0x30, COLA_C);





    Queue_Send_CMockExpectAndReturn(171, COLA_A, ((void*)0), 0, 1);

    Queue_Send_CMockIgnoreArg_item(172);

    Queue_Send_CMockExpectAndReturn(173, COLA_B, ((void*)0), 0, 1);

    Queue_Send_CMockIgnoreArg_item(174);

    Queue_Send_CMockExpectAndReturn(175, COLA_C, ((void*)0), 0, 1);

    Queue_Send_CMockIgnoreArg_item(176);





    uint8_t frame[] = {0x07, POWER_STATE_EMERGENCIA};

    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));

}













void test_FrameNull_NoHaceNada(void) {



    ConfigModule_HandleLoRaFrame(((void*)0), 5);

}







void test_FrameDemasiadoCorta_NoHaceNada(void) {

    uint8_t frame[] = {0x01};

    ConfigModule_HandleLoRaFrame(frame, 1);

}







void test_OpcodeDesconocido_SeIgnora(void) {

    uint8_t frame[] = {0xFF, 0xAA, 0xBB};

    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));

}
