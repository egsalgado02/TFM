#include "mock_rtos_iface.h"
#include "mock_payload_queue.h"
#include "mock_accel_port.h"
#include "../App/Inc/fsm.h"
#include "../App/Inc/accel_fsm.h"
#include "/Users/eduardogonzalezsalgado/.rbenv/versions/3.2.3/lib/ruby/gems/3.2.0/gems/ceedling-0.31.1/vendor/unity/src/unity.h"




static accel_fsm_t mi_accel;



static const uint32_t def_t [NUMERO_ESTADOS] = {60000, 120000, 600000};

static const uint8_t def_th = 4;

static const uint8_t def_db[NUMERO_ESTADOS] = {5, 10, 20};













static payload_msg_t s_msgs[8];

static int s_n_msgs;



static _Bool stub_enqueue(const payload_msg_t* msg, int num_calls) {

    (void)num_calls;

    if (s_n_msgs < 8) s_msgs[s_n_msgs++] = *msg;

    return 1;

}











static int16_t s_x, s_y, s_z;

static uint32_t s_tick;



static _Bool stub_leer_buffer(int16_t* x, int16_t* y, int16_t* z, int num_calls) {

    (void)num_calls;

    *x = s_x; *y = s_y; *z = s_z;

    return 1;

}



static _Bool stub_leer_buffer_fail(int16_t* x, int16_t* y, int16_t* z, int num_calls) {

    (void)x; (void)y; (void)z; (void)num_calls;

    return 0;

}



static uint32_t stub_get_tick(int num_calls) {

    (void)num_calls;

    return s_tick;

}











static _Bool s_es_robo;

static _Bool s_es_caida;



static _Bool stub_leer_alarma(_Bool* p_es_robo, _Bool* p_es_caida, int num_calls) {

    (void)num_calls;

    *p_es_robo = s_es_robo;

    *p_es_caida = s_es_caida;

    return 1;

}







void setUp(void) {

    __builtin___memset_chk (&mi_accel, 0, sizeof(accel_fsm_t), __builtin_object_size (&mi_accel, 0));

    s_n_msgs = 0;

    Queue_Create_CMockIgnoreAndReturn(90, (queue_handle_t)0x01);









    AccelPort_GetTickMs_CMockIgnoreAndReturn(95, 0);

    AccelPort_StartSleepTimer_CMockIgnore();



    AccelFSM_Init(&mi_accel, def_th, def_t, def_db, 4);

}



void tearDown(void) {}















static void medir(int16_t rx, int16_t ry, int16_t rz, uint32_t tick) {

    s_x = rx; s_y = ry; s_z = rz; s_tick = tick;

    s_n_msgs = 0;



    AccelPort_IniciarLecturaI2C_CMockIgnore();

    AccelPort_DatosListos_CMockIgnoreAndReturn(114, 1);

    AccelPort_LeerBuffer_Stub(stub_leer_buffer);

    AccelPort_GetTickMs_Stub(stub_get_tick);







    AccelPort_StartSleepTimer_CMockIgnore();



    PayloadQueue_Enqueue_Stub(stub_enqueue);



    AccelFSM_TriggerMedida(&mi_accel);

    fsm_fire((fsm_t*)&mi_accel);

    fsm_fire((fsm_t*)&mi_accel);

}











void test_Init_TrasMedirReprogramaConTimeoutDeConservacion(void) {







    AccelPort_IniciarLecturaI2C_CMockIgnore();

    AccelPort_DatosListos_CMockIgnoreAndReturn(138, 1);

    AccelPort_LeerBuffer_Stub(stub_leer_buffer);

    AccelPort_GetTickMs_Stub(stub_get_tick);

    PayloadQueue_Enqueue_Stub(stub_enqueue);

    s_x = 0; s_y = 0; s_z = 0; s_tick = 0; s_n_msgs = 0;





    AccelPort_StartSleepTimer_CMockExpect(145, def_t[POWER_STATE_CONSERVACION]);



    AccelFSM_TriggerMedida(&mi_accel);

    fsm_fire((fsm_t*)&mi_accel);

    fsm_fire((fsm_t*)&mi_accel);

}

void test_PrimeraMedida_EnviaLosTresEjes(void) {

    medir(160, 320, 1600, 0);

    UnityAssertEqualNumber((UNITY_INT)((3)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(161), UNITY_DISPLAY_STYLE_INT);

}







void test_SegundaMedidaIgual_NoEnviaPorDeadband(void) {

    medir(160, 320, 1600, 0);

    medir(160, 320, 1600, 1000);

    UnityAssertEqualNumber((UNITY_INT)((0)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(169), UNITY_DISPLAY_STYLE_INT);

}









void test_CambioSuperiorADeadband_EnviaSoloElEjeAfectado(void) {

    medir(0, 0, 0, 0);





    medir(3200, 0, 0, 1000);

    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(180), UNITY_DISPLAY_STYLE_INT);

}









void test_Keepalive_EnviaLosTresEjesAlPasar24h(void) {

    medir(160, 320, 1600, 0);



    medir(160, 320, 1600, (20UL * 1000UL) + 1);

    UnityAssertEqualNumber((UNITY_INT)((3)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(190), UNITY_DISPLAY_STYLE_INT);

}









void test_LecturaI2CFallida_NoEncola(void) {

    s_n_msgs = 0;

    AccelPort_IniciarLecturaI2C_CMockIgnore();

    AccelPort_DatosListos_CMockIgnoreAndReturn(199, 1);

    AccelPort_LeerBuffer_Stub(stub_leer_buffer_fail);







    AccelPort_StartSleepTimer_CMockIgnore();



    PayloadQueue_Enqueue_Stub(stub_enqueue);



    AccelFSM_TriggerMedida(&mi_accel);

    fsm_fire((fsm_t*)&mi_accel);

    fsm_fire((fsm_t*)&mi_accel);



    UnityAssertEqualNumber((UNITY_INT)((0)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(212), UNITY_DISPLAY_STYLE_INT);

}

void test_AlarmaRobo_EnviaEmergenciaConCodigoRobo(void) {

    s_es_robo = 1; s_es_caida = 0;

    AccelPort_LeerCausasAlarma_Stub(stub_leer_alarma);

    PayloadQueue_Enqueue_Stub(stub_enqueue);

    s_n_msgs = 0;



    AccelFSM_TriggerAlarma(&mi_accel);

    fsm_fire((fsm_t*)&mi_accel);



    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(231), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)((0)), (UNITY_INT)((s_msgs[0].sensor_id)), (((void*)0)), (UNITY_UINT)(232), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)((MOTIVO_EMERGENCY)), (UNITY_INT)((s_msgs[0].motivo)), (((void*)0)), (UNITY_UINT)(233), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)((EMERG_ROBO)), (UNITY_INT)((s_msgs[0].emerg_codigo)), (((void*)0)), (UNITY_UINT)(234), UNITY_DISPLAY_STYLE_INT);

}







void test_AlarmaCaida_EnviaEmergenciaConCodigoCaida(void) {

    s_es_robo = 0; s_es_caida = 1;

    AccelPort_LeerCausasAlarma_Stub(stub_leer_alarma);

    PayloadQueue_Enqueue_Stub(stub_enqueue);

    s_n_msgs = 0;



    AccelFSM_TriggerAlarma(&mi_accel);

    fsm_fire((fsm_t*)&mi_accel);



    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_msgs)), (((void*)0)), (UNITY_UINT)(248), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)((0)), (UNITY_INT)((s_msgs[0].sensor_id)), (((void*)0)), (UNITY_UINT)(249), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)((MOTIVO_EMERGENCY)), (UNITY_INT)((s_msgs[0].motivo)), (((void*)0)), (UNITY_UINT)(250), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)((EMERG_CAIDA)), (UNITY_INT)((s_msgs[0].emerg_codigo)), (((void*)0)), (UNITY_UINT)(251), UNITY_DISPLAY_STYLE_INT);

}

void test_ApplyCmd_SetTimeout_AfectaSoloAlModoIndicado(void) {

    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_TIMEOUT;

    cmd.mode = POWER_STATE_EMERGENCIA;

    cmd.value = 300000;

    AccelFSM_ApplyCmd(&mi_accel, &cmd);







    AccelPort_IniciarLecturaI2C_CMockIgnore();

    AccelPort_DatosListos_CMockIgnoreAndReturn(271, 1);

    AccelPort_LeerBuffer_Stub(stub_leer_buffer);

    AccelPort_GetTickMs_Stub(stub_get_tick);

    PayloadQueue_Enqueue_Stub(stub_enqueue);

    s_x = 0; s_y = 0; s_z = 0; s_tick = 0; s_n_msgs = 0;









 AccelPort_StartSleepTimer_CMockExpect(280, def_t[POWER_STATE_CONSERVACION]);





 AccelFSM_TriggerMedida(&mi_accel);

 fsm_fire((fsm_t*)&mi_accel);

 fsm_fire((fsm_t*)&mi_accel);





    config_cmd_t mode; __builtin___memset_chk (&mode, 0, sizeof(mode), __builtin_object_size (&mode, 0));

    mode.type = CMD_SET_MODE;

    mode.value = POWER_STATE_EMERGENCIA;

    AccelPort_SetThreshold_CMockExpect(291, def_th);

    AccelPort_StartSleepTimer_CMockExpect(292, 300000);

    AccelFSM_ApplyCmd(&mi_accel, &mode);

}









void test_ApplyCmd_SetMode_ReconfiguraHardwareYTimer(void) {

    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_MODE;

    cmd.value = POWER_STATE_ABUNDANCIA;







    AccelPort_SetThreshold_CMockExpect(306, def_th);

    AccelPort_StartSleepTimer_CMockExpect(307, def_t[POWER_STATE_ABUNDANCIA]);

    AccelFSM_ApplyCmd(&mi_accel, &cmd);

}







void test_ApplyCmd_SetThreshold_AplicaHardwareSinImportarModo(void) {

    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_THRESHOLD;

    cmd.value = 15;



    AccelPort_SetThreshold_CMockExpect(318, 15);

    AccelFSM_ApplyCmd(&mi_accel, &cmd);

}









void test_ApplyCmd_SetThreshold_ModoIgnorado_HardwareSiempreActualizado(void) {



    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_THRESHOLD;



    cmd.value = 3; cmd.mode = POWER_STATE_ABUNDANCIA;

    AccelPort_SetThreshold_CMockExpect(331, 3);

    AccelFSM_ApplyCmd(&mi_accel, &cmd);



    cmd.value = 7; cmd.mode = POWER_STATE_EMERGENCIA;

    AccelPort_SetThreshold_CMockExpect(335, 7);

    AccelFSM_ApplyCmd(&mi_accel, &cmd);

}











void test_ApplyCmd_Disable_SensorIgnoraMedidasYAlarmas(void) {







    config_cmd_t cmd; __builtin___memset_chk (&cmd, 0, sizeof(cmd), __builtin_object_size (&cmd, 0));

    cmd.type = CMD_SET_ENABLE;

    cmd.value = 0;

    AccelFSM_ApplyCmd(&mi_accel, &cmd);



    AccelFSM_TriggerMedida(&mi_accel);

    AccelFSM_TriggerAlarma(&mi_accel);

    fsm_fire((fsm_t*)&mi_accel);

}







void test_ApplyCmd_Reenable_SensorVuelveAOperar(void) {

    config_cmd_t dis; __builtin___memset_chk (&dis, 0, sizeof(dis), __builtin_object_size (&dis, 0));

    dis.type = CMD_SET_ENABLE; dis.value = 0;

    AccelFSM_ApplyCmd(&mi_accel, &dis);



    config_cmd_t en; __builtin___memset_chk (&en, 0, sizeof(en), __builtin_object_size (&en, 0));

    en.type = CMD_SET_ENABLE; en.value = 1;





    AccelPort_SetThreshold_CMockExpect(368, def_th);

    AccelPort_StartSleepTimer_CMockExpect(369, def_t[POWER_STATE_CONSERVACION]);

    AccelFSM_ApplyCmd(&mi_accel, &en);

}
