#include "build/test/mocks/mock_rtos_iface.h"
#include "mock_lora_tx.h"
#include "../App/Inc/emergency_codes.h"
#include "../App/Inc/fsm.h"
#include "../App/Inc/payload_builder_fsm.h"
#include "/Users/eduardogonzalezsalgado/.rbenv/versions/3.2.3/lib/ruby/gems/3.2.0/gems/ceedling-0.31.1/vendor/unity/src/unity.h"






static payload_fsm_t mi_payload;







static const uint32_t s_payload_t[NUMERO_ESTADOS] = { 60000, 360000, 900000 };









static uint8_t s_frame[51];

static uint8_t s_frame_len;

static uint8_t s_frame_fport;

static int s_n_envios;



static void stub_capturar_frame(uint8_t* payload, uint8_t length, uint8_t fport,

                                int num_calls) {

    (void)num_calls;

    __builtin___memcpy_chk (s_frame, payload, length, __builtin_object_size (s_frame, 0));

    s_frame_len = length;

    s_frame_fport = fport;

    s_n_envios++;

}







static payload_msg_t s_msgs_iny[32];

static int s_n_msgs_iny;

static int s_idx_iny;







static void inyectar(uint8_t id, payload_event_origin_t motivo, int16_t valor) {

    if (s_n_msgs_iny >= 32) return;

    s_msgs_iny[s_n_msgs_iny].sensor_id = id;

    s_msgs_iny[s_n_msgs_iny].motivo = motivo;

    s_msgs_iny[s_n_msgs_iny].valor = valor;

    s_msgs_iny[s_n_msgs_iny].emerg_codigo = EMERG_NONE;

    s_n_msgs_iny++;

}







static void inyectar_emergencia(uint8_t id, emerg_code_t codigo, int16_t valor) {

    if (s_n_msgs_iny >= 32) return;

    s_msgs_iny[s_n_msgs_iny].sensor_id = id;

    s_msgs_iny[s_n_msgs_iny].motivo = MOTIVO_EMERGENCY;

    s_msgs_iny[s_n_msgs_iny].valor = valor;

    s_msgs_iny[s_n_msgs_iny].emerg_codigo = codigo;

    s_n_msgs_iny++;

}



void setUp(void) {

    __builtin___memset_chk (&mi_payload, 0, sizeof(payload_fsm_t), __builtin_object_size (&mi_payload, 0));

    __builtin___memset_chk (s_frame, 0, sizeof(s_frame), __builtin_object_size (s_frame, 0));

    __builtin___memset_chk (s_msgs_iny, 0, sizeof(s_msgs_iny), __builtin_object_size (s_msgs_iny, 0));

    s_frame_len = 0;

    s_frame_fport = 0;

    s_n_envios = 0;

    s_n_msgs_iny = 0;

    s_idx_iny = 0;

    Queue_Create_CMockIgnoreAndReturn(90, (queue_handle_t)0x01);





    Queue_Receive_CMockIgnoreAndReturn(93, 0);

    PayloadFSM_Init(&mi_payload, s_payload_t, 4);

}



void tearDown(void) {}

static config_cmd_t s_cmd_iny;

static int s_cmd_pendiente;



static _Bool stub_recibir_cmd(queue_handle_t q, void* dst, uint32_t to,

                             int num_calls) {

    (void)q; (void)to; (void)num_calls;

    if (!s_cmd_pendiente) return 0;

    __builtin___memcpy_chk (dst, &s_cmd_iny, sizeof(config_cmd_t), __builtin_object_size (dst, 0));

    s_cmd_pendiente = 0;

    return 1;

}



static void enviar_cmd(cmd_type_t tipo, power_state_t modo, uint32_t valor) {

    __builtin___memset_chk (&s_cmd_iny, 0, sizeof(s_cmd_iny), __builtin_object_size (&s_cmd_iny, 0));

    s_cmd_iny.type = tipo;

    s_cmd_iny.mode = modo;

    s_cmd_iny.value = valor;

    s_cmd_pendiente = 1;

    Queue_Receive_Stub(stub_recibir_cmd);

    PayloadFSM_DrainCmdQueue(&mi_payload);

}



void test_arranca_con_el_periodo_de_conservacion(void) {





    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)((s_payload_t[POWER_STATE_CONSERVACION])), (UNITY_INT)(UNITY_UINT32)((mi_payload.lora_report_timeout_ms)), (((void*)0)), (UNITY_UINT)(136), UNITY_DISPLAY_STYLE_UINT32);



}



void test_el_cambio_de_modo_cambia_el_periodo_de_envio(void) {

    enviar_cmd(CMD_SET_MODE, POWER_STATE_ABUNDANCIA, POWER_STATE_ABUNDANCIA);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)((s_payload_t[POWER_STATE_ABUNDANCIA])), (UNITY_INT)(UNITY_UINT32)((mi_payload.lora_report_timeout_ms)), (((void*)0)), (UNITY_UINT)(142), UNITY_DISPLAY_STYLE_UINT32);





    enviar_cmd(CMD_SET_MODE, POWER_STATE_EMERGENCIA, POWER_STATE_EMERGENCIA);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)((s_payload_t[POWER_STATE_EMERGENCIA])), (UNITY_INT)(UNITY_UINT32)((mi_payload.lora_report_timeout_ms)), (((void*)0)), (UNITY_UINT)(146), UNITY_DISPLAY_STYLE_UINT32);



}



void test_fijar_el_periodo_de_un_modo_no_altera_los_otros(void) {

    enviar_cmd(CMD_SET_TIMEOUT, POWER_STATE_EMERGENCIA, 123456u);







    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)((s_payload_t[POWER_STATE_CONSERVACION])), (UNITY_INT)(UNITY_UINT32)((mi_payload.lora_report_timeout_ms)), (((void*)0)), (UNITY_UINT)(155), UNITY_DISPLAY_STYLE_UINT32);



    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)((s_payload_t[POWER_STATE_ABUNDANCIA])), (UNITY_INT)(UNITY_UINT32)((mi_payload.timeouts_ms[POWER_STATE_ABUNDANCIA])), (((void*)0)), (UNITY_UINT)(157), UNITY_DISPLAY_STYLE_UINT32);



    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)((123456u)), (UNITY_INT)(UNITY_UINT32)((mi_payload.timeouts_ms[POWER_STATE_EMERGENCIA])), (((void*)0)), (UNITY_UINT)(159), UNITY_DISPLAY_STYLE_UINT32);







    enviar_cmd(CMD_SET_MODE, POWER_STATE_EMERGENCIA, POWER_STATE_EMERGENCIA);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)((123456u)), (UNITY_INT)(UNITY_UINT32)((mi_payload.lora_report_timeout_ms)), (((void*)0)), (UNITY_UINT)(163), UNITY_DISPLAY_STYLE_UINT32);

}















static void procesar_pendientes(void) {

    for (int i = 0; i < 32; i++) {

        if (s_idx_iny < s_n_msgs_iny) {

            mi_payload.rx_msg = s_msgs_iny[s_idx_iny++];

            mi_payload.events |= EV_NEW_MESSAGE;

        }

        PayloadFSM_DrainCmdQueue(&mi_payload);

        fsm_fire((fsm_t*)&mi_payload);

    }

}

void test_Emergencia_SinValor_EnviaDosBytesEnFPortEmergencia(void) {





    inyectar_emergencia(10, EMERG_CAIDA, 0);



    LoRa_SendPayload_Stub(stub_capturar_frame);

    procesar_pendientes();



    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(199), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((2)), (UNITY_INT)(UNITY_UINT8 )((s_frame_len)), (((void*)0)), (UNITY_UINT)(200), UNITY_DISPLAY_STYLE_UINT8);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT8 )((0x0A)), (UNITY_INT)(UNITY_INT8 )((s_frame[0])), (((void*)0)), (UNITY_UINT)(201), UNITY_DISPLAY_STYLE_HEX8);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT8 )((EMERG_CAIDA)), (UNITY_INT)(UNITY_INT8 )((s_frame[1])), (((void*)0)), (UNITY_UINT)(202), UNITY_DISPLAY_STYLE_HEX8);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((2)), (UNITY_INT)(UNITY_UINT8 )((s_frame_fport)), (((void*)0)), (UNITY_UINT)(203), UNITY_DISPLAY_STYLE_UINT8);

}









void test_Emergencia_ConValor_EnviaCuatroBytes(void) {





    emerg_code_t cod_con_valor = (emerg_code_t)(EMERG_VALOR_FLAG | 0x01);

    inyectar_emergencia(20, cod_con_valor, 500);



    LoRa_SendPayload_Stub(stub_capturar_frame);

    procesar_pendientes();



    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(218), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((4)), (UNITY_INT)(UNITY_UINT8 )((s_frame_len)), (((void*)0)), (UNITY_UINT)(219), UNITY_DISPLAY_STYLE_UINT8);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT8 )((0x14)), (UNITY_INT)(UNITY_INT8 )((s_frame[0])), (((void*)0)), (UNITY_UINT)(220), UNITY_DISPLAY_STYLE_HEX8);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT8 )((0x81)), (UNITY_INT)(UNITY_INT8 )((s_frame[1])), (((void*)0)), (UNITY_UINT)(221), UNITY_DISPLAY_STYLE_HEX8);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT8 )((0x01)), (UNITY_INT)(UNITY_INT8 )((s_frame[2])), (((void*)0)), (UNITY_UINT)(222), UNITY_DISPLAY_STYLE_HEX8);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT8 )((0xF4)), (UNITY_INT)(UNITY_INT8 )((s_frame[3])), (((void*)0)), (UNITY_UINT)(223), UNITY_DISPLAY_STYLE_HEX8);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((2)), (UNITY_INT)(UNITY_UINT8 )((s_frame_fport)), (((void*)0)), (UNITY_UINT)(224), UNITY_DISPLAY_STYLE_UINT8);

}















void test_Rutina_SolaSinTimeout_NoMandaNada(void) {

    inyectar(1, MOTIVO_ROUTINE, 100);







    procesar_pendientes();

    UnityAssertEqualNumber((UNITY_INT)((0)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(239), UNITY_DISPLAY_STYLE_INT);

}









void test_Timeout_ConUnSensorIdPequenio_MandaCincoBytes(void) {





    inyectar(3, MOTIVO_ROUTINE, 100);

    inyectar(3, MOTIVO_ROUTINE, 200);

    inyectar(3, MOTIVO_ROUTINE, 300);



    LoRa_SendPayload_Stub(stub_capturar_frame);

    procesar_pendientes();



    PayloadFSM_TriggerTimeout(&mi_payload);

    fsm_fire((fsm_t*)&mi_payload);



    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(258), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((5)), (UNITY_INT)(UNITY_UINT8 )((s_frame_len)), (((void*)0)), (UNITY_UINT)(259), UNITY_DISPLAY_STYLE_UINT8);



    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((1)), (UNITY_INT)(UNITY_UINT8 )((s_frame_fport)), (((void*)0)), (UNITY_UINT)(261), UNITY_DISPLAY_STYLE_UINT8);

}







void test_Timeout_ConUnSensorIdGrande_MandaSieteBytes(void) {



    inyectar(20, MOTIVO_ROUTINE, 500);

    inyectar(20, MOTIVO_ROUTINE, 600);



    LoRa_SendPayload_Stub(stub_capturar_frame);

    procesar_pendientes();



    PayloadFSM_TriggerTimeout(&mi_payload);

    fsm_fire((fsm_t*)&mi_payload);



    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(277), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((7)), (UNITY_INT)(UNITY_UINT8 )((s_frame_len)), (((void*)0)), (UNITY_UINT)(278), UNITY_DISPLAY_STYLE_UINT8);

}







void test_Timeout_ConDosSensores_AmbosAparecenEnLaTrama(void) {





    inyectar(4, MOTIVO_ROUTINE, 100);

    inyectar(20, MOTIVO_ROUTINE, 500);



    LoRa_SendPayload_Stub(stub_capturar_frame);

    procesar_pendientes();



    PayloadFSM_TriggerTimeout(&mi_payload);

    fsm_fire((fsm_t*)&mi_payload);



    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(295), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((12)), (UNITY_INT)(UNITY_UINT8 )((s_frame_len)), (((void*)0)), (UNITY_UINT)(296), UNITY_DISPLAY_STYLE_UINT8);

}







void test_ForceSend_ConDatosAcumulados_MandaSinEsperarTimeout(void) {

    inyectar(3, MOTIVO_ROUTINE, 50);



    LoRa_SendPayload_Stub(stub_capturar_frame);

    procesar_pendientes();



    PayloadFSM_ForceSend(&mi_payload);

    fsm_fire((fsm_t*)&mi_payload);



    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(310), UNITY_DISPLAY_STYLE_INT);

    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )((5)), (UNITY_INT)(UNITY_UINT8 )((s_frame_len)), (((void*)0)), (UNITY_UINT)(311), UNITY_DISPLAY_STYLE_UINT8);

}







void test_TimeoutConStatsVacios_NoMandaNada(void) {



    PayloadFSM_TriggerTimeout(&mi_payload);



    fsm_fire((fsm_t*)&mi_payload);

    UnityAssertEqualNumber((UNITY_INT)((0)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(321), UNITY_DISPLAY_STYLE_INT);

}









void test_TrasEnvio_LosStatsSeResetean(void) {



    inyectar(2, MOTIVO_ROUTINE, 100);

    inyectar(2, MOTIVO_ROUTINE, 200);



    LoRa_SendPayload_Stub(stub_capturar_frame);

    procesar_pendientes();

    PayloadFSM_TriggerTimeout(&mi_payload);

    fsm_fire((fsm_t*)&mi_payload);

    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(336), UNITY_DISPLAY_STYLE_INT);





    PayloadFSM_TriggerTimeout(&mi_payload);

    fsm_fire((fsm_t*)&mi_payload);

    UnityAssertEqualNumber((UNITY_INT)((1)), (UNITY_INT)((s_n_envios)), (((void*)0)), (UNITY_UINT)(341), UNITY_DISPLAY_STYLE_INT);

}
