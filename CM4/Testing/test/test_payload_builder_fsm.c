/*
 * test_payload_builder_fsm.c - pruebas de payload_builder_fsm en el PC.
 *
 * Las medidas de rutina se acumulan (máximo, mínimo y media por magnitud) y
 * salen en una trama al cerrar la ventana; las emergencias salen en el acto.
 *
 * La cola de salida la lee la tarea (StartPayloadTask), no la FSM. Por eso
 * aquí se hace lo mismo que haría la tarea al recibir un mensaje: se copia
 * en rx_msg, se marca EV_NEW_MESSAGE, se vacía la cola de comandos y se
 * dispara la FSM. El envío por radio es un doble que guarda la trama, su
 * longitud y el FPORT, y eso es lo que se comprueba.
 *
 * Dobles: mock_lora_tx.h, mock_rtos_iface.h
 *
 * Aquí se prueba: el formato compacto (5 bytes) y el plano (7 bytes),
 * varias magnitudes en una trama, emergencias con y sin valor, que una
 * ventana vacía no envía nada, que las estadísticas se reinician al
 * enviar y el periodo de la ventana en cada modo.
 */

#include "unity.h"
#include "payload_builder_fsm.h"
#include "fsm.h"
#include "lora_tx.h"           // FPORT_RUTINA, FPORT_EMERGENCIA
#include "emergency_codes.h"   // EMERG_ROBO, EMERG_CAIDA, EMERG_VALOR_FLAG, ...
#include "mock_lora_tx.h"
#include "mock_rtos_iface.h"
#include <string.h>

static payload_fsm_t mi_payload;

/* Ventana de agregación por modo: abundancia / conservación / emergencia.
   Los tests que no miran el periodo solo necesitan que exista el vector. */
static const uint32_t s_payload_t[NUMERO_ESTADOS] = { 60000, 360000, 900000 };

/* Captura de la última trama LoRa enviada (payload + length + FPort).
   El FPort nos sirve para verificar que las rutinas salen por FPORT_RUTINA
   y las emergencias por FPORT_EMERGENCIA. */
static uint8_t s_frame[MAX_PAYLOAD_BUFFER];
static uint8_t s_frame_len;
static uint8_t s_frame_fport;
static int     s_n_envios;

static void stub_capturar_frame(uint8_t* payload, uint8_t length, uint8_t fport,
                                int num_calls) {
    (void)num_calls;
    memcpy(s_frame, payload, length);
    s_frame_len   = length;
    s_frame_fport = fport;
    s_n_envios++;
}

/* Cola de mensajes que vamos a inyectar como si vinieran de los sensores */
#define MAX_MSGS_INY 32
static payload_msg_t s_msgs_iny[MAX_MSGS_INY];
static int           s_n_msgs_iny;
static int           s_idx_iny;

/* Helper para encolar un mensaje de RUTINA. El emerg_codigo se queda
   en EMERG_NONE, que es el valor neutro cuando no es emergencia. */
static void inyectar(uint8_t id, payload_event_origin_t motivo, int16_t valor) {
    if (s_n_msgs_iny >= MAX_MSGS_INY) return;
    s_msgs_iny[s_n_msgs_iny].sensor_id    = id;
    s_msgs_iny[s_n_msgs_iny].motivo       = motivo;
    s_msgs_iny[s_n_msgs_iny].valor        = valor;
    s_msgs_iny[s_n_msgs_iny].emerg_codigo = EMERG_NONE;
    s_n_msgs_iny++;
}

/* Helper específico para EMERGENCIAS: añade el código emerg_codigo, que
   es el byte que viaja en la trama LoRa después del sensor_id. */
static void inyectar_emergencia(uint8_t id, emerg_code_t codigo, int16_t valor) {
    if (s_n_msgs_iny >= MAX_MSGS_INY) return;
    s_msgs_iny[s_n_msgs_iny].sensor_id    = id;
    s_msgs_iny[s_n_msgs_iny].motivo       = MOTIVO_EMERGENCY;
    s_msgs_iny[s_n_msgs_iny].valor        = valor;
    s_msgs_iny[s_n_msgs_iny].emerg_codigo = codigo;
    s_n_msgs_iny++;
}

void setUp(void) {
    memset(&mi_payload, 0, sizeof(payload_fsm_t));
    memset(s_frame, 0, sizeof(s_frame));
    memset(s_msgs_iny, 0, sizeof(s_msgs_iny));
    s_frame_len   = 0;
    s_frame_fport = 0;
    s_n_envios    = 0;
    s_n_msgs_iny  = 0;
    s_idx_iny     = 0;
    Queue_Create_IgnoreAndReturn((queue_handle_t)0x01);
    // DrainCmdQueue hace Queue_Receive sobre cmd_queue. En estos tests no
    // inyectamos comandos, así que la cola está siempre vacía.
    Queue_Receive_IgnoreAndReturn(false);
    PayloadFSM_Init(&mi_payload, s_payload_t, 4);
}

void tearDown(void) {}

/* ------------------------------------------------------------------
   Periodo de la ventana de envío por modo de energía.

   El envío es el evento más caro del nodo, así que la ventana tiene que
   escalarse con el modo igual que la cadencia de los sensores. Estos dos
   casos fijan ese contrato: que el arranque deje el periodo del modo con
   el que se inicializa, que la difusión de modo lo cambie, y que fijar el
   periodo de un modo no toque el de los otros dos.
   ------------------------------------------------------------------ */

/* Helper: entrega un comando a apply_cmd por la vía real, la cola. */
static config_cmd_t s_cmd_iny;
static int          s_cmd_pendiente;

static bool stub_recibir_cmd(queue_handle_t q, void* dst, uint32_t to,
                             int num_calls) {
    (void)q; (void)to; (void)num_calls;
    if (!s_cmd_pendiente) return false;
    memcpy(dst, &s_cmd_iny, sizeof(config_cmd_t));
    s_cmd_pendiente = 0;
    return true;
}

static void enviar_cmd(cmd_type_t tipo, power_state_t modo, uint32_t valor) {
    memset(&s_cmd_iny, 0, sizeof(s_cmd_iny));
    s_cmd_iny.type  = tipo;
    s_cmd_iny.mode  = modo;
    s_cmd_iny.value = valor;
    s_cmd_pendiente = 1;
    Queue_Receive_Stub(stub_recibir_cmd);
    PayloadFSM_DrainCmdQueue(&mi_payload);
}

void test_arranca_con_el_periodo_de_conservacion(void) {
    /* Igual que los FSM de sensor, el builder arranca en CONSERVACION
       hasta que llega la primera difusión de modo. */
    TEST_ASSERT_EQUAL_UINT32(s_payload_t[POWER_STATE_CONSERVACION],
                             mi_payload.lora_report_timeout_ms);
}

void test_el_cambio_de_modo_cambia_el_periodo_de_envio(void) {
    enviar_cmd(CMD_SET_MODE, POWER_STATE_ABUNDANCIA, POWER_STATE_ABUNDANCIA);
    TEST_ASSERT_EQUAL_UINT32(s_payload_t[POWER_STATE_ABUNDANCIA],
                             mi_payload.lora_report_timeout_ms);

    enviar_cmd(CMD_SET_MODE, POWER_STATE_EMERGENCIA, POWER_STATE_EMERGENCIA);
    TEST_ASSERT_EQUAL_UINT32(s_payload_t[POWER_STATE_EMERGENCIA],
                             mi_payload.lora_report_timeout_ms);
}

void test_fijar_el_periodo_de_un_modo_no_altera_los_otros(void) {
    enviar_cmd(CMD_SET_TIMEOUT, POWER_STATE_EMERGENCIA, 123456u);

    /* El modo activo sigue siendo CONSERVACION, así que el periodo en
       vigor no debe haberse movido. */
    TEST_ASSERT_EQUAL_UINT32(s_payload_t[POWER_STATE_CONSERVACION],
                             mi_payload.lora_report_timeout_ms);
    TEST_ASSERT_EQUAL_UINT32(s_payload_t[POWER_STATE_ABUNDANCIA],
                             mi_payload.timeouts_ms[POWER_STATE_ABUNDANCIA]);
    TEST_ASSERT_EQUAL_UINT32(123456u,
                             mi_payload.timeouts_ms[POWER_STATE_EMERGENCIA]);

    /* Y al entrar en ese modo, el nuevo periodo es el que manda. */
    enviar_cmd(CMD_SET_MODE, POWER_STATE_EMERGENCIA, POWER_STATE_EMERGENCIA);
    TEST_ASSERT_EQUAL_UINT32(123456u, mi_payload.lora_report_timeout_ms);
}

/* Simula lo que hace la tarea (StartPayloadTask) en un ciclo:
     - Saca un mensaje de la cola fake.
     - Lo pone en rx_msg + EV_NEW_MESSAGE (equivalente a Queue_Receive==true).
     - Drena cmd_queue (vacía en estos tests).
     - Dispara fsm_fire.
   Un mensaje por iteración. Con MAX_MSGS_INY vueltas basta. */
static void procesar_pendientes(void) {
    for (int i = 0; i < MAX_MSGS_INY; i++) {
        if (s_idx_iny < s_n_msgs_iny) {
            mi_payload.rx_msg  = s_msgs_iny[s_idx_iny++];
            mi_payload.events |= EV_NEW_MESSAGE;
        }
        PayloadFSM_DrainCmdQueue(&mi_payload);
        fsm_fire((fsm_t*)&mi_payload);
    }
}

/* ------------------------------------------------------------------ */
/*  Emergencias: se envían YA, sin esperar timeout                     */
/* ------------------------------------------------------------------ */

// Una emergencia SIN valor (caída, robo) sale en 2 bytes: [sensor_id]
// y [emerg_codigo]. El campo "valor" del payload_msg_t no viaja porque
// el código no tiene activo EMERG_VALOR_FLAG (bit 7). Sale por su FPort
// dedicado (FPORT_EMERGENCIA), no por el de rutina.
void test_Emergencia_SinValor_EnviaDosBytesEnFPortEmergencia(void) {
    // sensor_id=10 (valor de prueba), código = EMERG_CAIDA (0x02)
    // -> trama esperada: [0x0A, 0x02] por FPORT_EMERGENCIA
    inyectar_emergencia(10, EMERG_CAIDA, 0);

    LoRa_SendPayload_Stub(stub_capturar_frame);
    procesar_pendientes();

    TEST_ASSERT_EQUAL_INT(1, s_n_envios);
    TEST_ASSERT_EQUAL_UINT8(2, s_frame_len);
    TEST_ASSERT_EQUAL_HEX8(0x0A, s_frame[0]);
    TEST_ASSERT_EQUAL_HEX8(EMERG_CAIDA, s_frame[1]);
    TEST_ASSERT_EQUAL_UINT8(FPORT_EMERGENCIA, s_frame_fport);
}

// Una emergencia CON valor (código con EMERG_VALOR_FLAG activo) sale
// en 4 bytes: [sensor_id, emerg_codigo, valor_hi, valor_lo]. Usamos
// 0x81 como ejemplo de "emergencia con valor" (futuro código de INA).
void test_Emergencia_ConValor_EnviaCuatroBytes(void) {
    // sensor_id=20, código = 0x81 (= EMERG_VALOR_FLAG | 0x01), valor=500 (0x01F4)
    // -> trama esperada: [0x14, 0x81, 0x01, 0xF4] por FPORT_EMERGENCIA
    emerg_code_t cod_con_valor = (emerg_code_t)(EMERG_VALOR_FLAG | 0x01);
    inyectar_emergencia(20, cod_con_valor, 500);

    LoRa_SendPayload_Stub(stub_capturar_frame);
    procesar_pendientes();

    TEST_ASSERT_EQUAL_INT(1, s_n_envios);
    TEST_ASSERT_EQUAL_UINT8(4, s_frame_len);
    TEST_ASSERT_EQUAL_HEX8(0x14, s_frame[0]);
    TEST_ASSERT_EQUAL_HEX8(0x81, s_frame[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, s_frame[2]);
    TEST_ASSERT_EQUAL_HEX8(0xF4, s_frame[3]);
    TEST_ASSERT_EQUAL_UINT8(FPORT_EMERGENCIA, s_frame_fport);
}

/* ------------------------------------------------------------------ */
/*  Rutina: SIEMPRE estadístico, NO envía hasta timeout o force        */
/* ------------------------------------------------------------------ */

// Un mensaje de rutina solo se acumula, NO se manda. Hay que esperar
// al timeout periódico o a un ForceSend para que salga por LoRa.
void test_Rutina_SolaSinTimeout_NoMandaNada(void) {
    inyectar(1, MOTIVO_ROUTINE, 100);

    // Sin LoRa_SendPayload_Expect ni Stub: si la FSM llamara a LoRa,
    // CMock fallaría el test.
    procesar_pendientes();
    TEST_ASSERT_EQUAL_INT(0, s_n_envios);
}

// Tras inyectar varios valores de un sensor con id<=15 y disparar
// el timeout, se manda UNA trama de 5 bytes con max+min+media
// empaquetados (formato compacto para ids pequeños).
void test_Timeout_ConUnSensorIdPequenio_MandaCincoBytes(void) {
    // Un solo sensor de id <=15 -> al empaquetar son 5 bytes
    // (max+min+media empaquetados con la cabecera del id en 4 bits)
    inyectar(3, MOTIVO_ROUTINE, 100);
    inyectar(3, MOTIVO_ROUTINE, 200);
    inyectar(3, MOTIVO_ROUTINE, 300);

    LoRa_SendPayload_Stub(stub_capturar_frame);
    procesar_pendientes();

    PayloadFSM_TriggerTimeout(&mi_payload);
    fsm_fire((fsm_t*)&mi_payload);

    TEST_ASSERT_EQUAL_INT(1, s_n_envios);
    TEST_ASSERT_EQUAL_UINT8(5, s_frame_len);
    // Las rutinas siempre salen por FPORT_RUTINA, nunca por el de emergencia.
    TEST_ASSERT_EQUAL_UINT8(FPORT_RUTINA, s_frame_fport);
}

// Mismo caso pero con un sensor de id>15: como el id no cabe en
// 4 bits, la trama por sensor son 7 bytes (id + 6 bytes de stats).
void test_Timeout_ConUnSensorIdGrande_MandaSieteBytes(void) {
    // Un solo sensor de id >15 -> 7 bytes (id + max + min + media a 16 bits)
    inyectar(20, MOTIVO_ROUTINE, 500);
    inyectar(20, MOTIVO_ROUTINE, 600);

    LoRa_SendPayload_Stub(stub_capturar_frame);
    procesar_pendientes();

    PayloadFSM_TriggerTimeout(&mi_payload);
    fsm_fire((fsm_t*)&mi_payload);

    TEST_ASSERT_EQUAL_INT(1, s_n_envios);
    TEST_ASSERT_EQUAL_UINT8(7, s_frame_len);
}

// Si hay datos de dos sensores (uno pequeño y otro grande) en la
// misma ventana, los dos salen en la misma trama: 5 + 7 = 12 bytes.
void test_Timeout_ConDosSensores_AmbosAparecenEnLaTrama(void) {
    // Dos sensores distintos (uno id<=15 y otro id>15) deben aparecer
    // los dos en la misma trama: 5 + 7 = 12 bytes.
    inyectar(4,  MOTIVO_ROUTINE, 100);
    inyectar(20, MOTIVO_ROUTINE, 500);

    LoRa_SendPayload_Stub(stub_capturar_frame);
    procesar_pendientes();

    PayloadFSM_TriggerTimeout(&mi_payload);
    fsm_fire((fsm_t*)&mi_payload);

    TEST_ASSERT_EQUAL_INT(1, s_n_envios);
    TEST_ASSERT_EQUAL_UINT8(12, s_frame_len);
}

// El ForceSend manda lo acumulado SIN esperar a que expire el timer
// de LoRa. Útil para "vacía la cola ya".
void test_ForceSend_ConDatosAcumulados_MandaSinEsperarTimeout(void) {
    inyectar(3, MOTIVO_ROUTINE, 50);

    LoRa_SendPayload_Stub(stub_capturar_frame);
    procesar_pendientes();

    PayloadFSM_ForceSend(&mi_payload);
    fsm_fire((fsm_t*)&mi_payload);

    TEST_ASSERT_EQUAL_INT(1, s_n_envios);
    TEST_ASSERT_EQUAL_UINT8(5, s_frame_len);   // un sensor id<=15
}

// Si el timeout expira pero no se ha acumulado ningún mensaje, NO
// se manda trama vacía: simplemente no hay nada que enviar.
void test_TimeoutConStatsVacios_NoMandaNada(void) {
    // No hemos inyectado nada -> al disparar timeout, no debe haber envío.
    PayloadFSM_TriggerTimeout(&mi_payload);
    // Sin Expect/Stub de LoRa -> si se llama, CMock falla
    fsm_fire((fsm_t*)&mi_payload);
    TEST_ASSERT_EQUAL_INT(0, s_n_envios);
}

// Tras enviar la trama, las estadísticas tienen que quedar a cero
// para empezar la siguiente ventana limpia. Lo verificamos viendo
// que el siguiente Timeout sin mensajes nuevos NO manda nada.
void test_TrasEnvio_LosStatsSeResetean(void) {
    // Primera ventana: un sensor con varios valores
    inyectar(2, MOTIVO_ROUTINE, 100);
    inyectar(2, MOTIVO_ROUTINE, 200);

    LoRa_SendPayload_Stub(stub_capturar_frame);
    procesar_pendientes();
    PayloadFSM_TriggerTimeout(&mi_payload);
    fsm_fire((fsm_t*)&mi_payload);
    TEST_ASSERT_EQUAL_INT(1, s_n_envios);

    // Segunda "ventana": sin nuevos mensajes, no debe haber envío
    PayloadFSM_TriggerTimeout(&mi_payload);
    fsm_fire((fsm_t*)&mi_payload);
    TEST_ASSERT_EQUAL_INT(1, s_n_envios);   // sigue siendo 1, no se incrementa
}
