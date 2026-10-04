/*
 * test_config_module.c - pruebas de config_module en el PC.
 *
 * config_module es el que recibe las tramas de bajada: las descodifica en
 * ConfigModule_HandleLoRaFrame y manda cada comando a la cola que toca.
 *
 * Se comprueba que:
 *   - RegisterSensor acepta una cola válida y rechaza una nula.
 *   - Cada una de las siete operaciones llega a la cola correcta con los
 *     campos bien sacados de la trama.
 *   - Forzar modo (0x07) llega a todas las colas registradas.
 *   - Los umbrales de tensión (0x04) van directos al power_fsm.
 *   - Una trama nula, corta o con un código desconocido no hace nada.
 *
 * Dobles: mock_rtos_iface.h (para ver qué se manda con Queue_Send) y
 * mock_power_fsm.h (para ver que llegan los umbrales).
 */

#include "unity.h"
#include "config_module.h"
#include "mock_rtos_iface.h"
#include "mock_power_fsm.h"
#include <string.h>

static const queue_handle_t COLA_A = (queue_handle_t)0xA1;
static const queue_handle_t COLA_B = (queue_handle_t)0xB2;
static const queue_handle_t COLA_C = (queue_handle_t)0xC3;

static power_fsm_t dummy_power;

/* Captura del último comando enviado vía Queue_Send */
static config_cmd_t   s_ultimo_cmd;
static queue_handle_t s_ultima_cola;

static bool stub_capturar_cmd(queue_handle_t q, const void* item,
                              uint32_t timeout_ms, int num_calls) {
    (void)timeout_ms; (void)num_calls;
    s_ultima_cola = q;
    s_ultimo_cmd  = *(const config_cmd_t*)item;
    return true;
}

void setUp(void) {
    memset(&dummy_power, 0, sizeof(power_fsm_t));
    memset(&s_ultimo_cmd, 0, sizeof(s_ultimo_cmd));
    s_ultima_cola = NULL;

    ConfigModule_Init(&dummy_power);
}

void tearDown(void) {}

/* ================================================================== */
/*  RegisterSensor                                                     */
/* ================================================================== */

// Registrar un sensor con una cola válida devuelve true.
void test_RegisterSensor_ColaValida_DevuelveTrue(void) {
    TEST_ASSERT_TRUE(ConfigModule_RegisterSensor(1, COLA_A));
}

// Si se pasa NULL como cola, no debe registrarse y devolver false.
void test_RegisterSensor_ColaNull_DevuelveFalse(void) {
    TEST_ASSERT_FALSE(ConfigModule_RegisterSensor(1, NULL));
}

/* ================================================================== */
/*  HandleLoRaFrame: cada opcode produce el comando esperado           */
/*  en la cola del sensor correcto                                     */
/* ================================================================== */

// Una trama SET_TIMEOUT debe descodificarse y acabar en la cola del
// sensor de destino con el opcode y los milisegundos correctos.
void test_FrameSetTimeout_VaAlSensorYContieneMs(void) {
    ConfigModule_RegisterSensor(7, COLA_A);

    // [0x01][sid=7][mode=CONSERVACION=1][timeout_ms=10000 = 0x002710]
    uint8_t frame[] = {0x01, 7, 1, 0x00, 0x27, 0x10};

    Queue_Send_Stub(stub_capturar_cmd);
    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));

    TEST_ASSERT_EQUAL_PTR(COLA_A, s_ultima_cola);
    TEST_ASSERT_EQUAL_UINT8(7, s_ultimo_cmd.sensor_id);
    TEST_ASSERT_EQUAL_INT(CMD_SET_TIMEOUT, s_ultimo_cmd.type);
    TEST_ASSERT_EQUAL_INT(POWER_STATE_CONSERVACION, s_ultimo_cmd.mode);
    TEST_ASSERT_EQUAL_UINT32(10000, s_ultimo_cmd.value);
}

// Una trama SET_THRESHOLD debe llegar a la cola del sensor con el
// umbral y el modo de energía indicados.
void test_FrameSetThreshold_VaAlSensorYContieneThreshold(void) {
    ConfigModule_RegisterSensor(3, COLA_B);

    // [0x02][sid=3][mode=ABUNDANCIA=0][threshold=25]
    uint8_t frame[] = {0x02, 3, 0, 25};

    Queue_Send_Stub(stub_capturar_cmd);
    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));

    TEST_ASSERT_EQUAL_PTR(COLA_B, s_ultima_cola);
    TEST_ASSERT_EQUAL_INT(CMD_SET_THRESHOLD, s_ultimo_cmd.type);
    TEST_ASSERT_EQUAL_UINT32(25, s_ultimo_cmd.value);
}

// Una trama FORCE_READ debe llegar a la cola del sensor como un
// comando "dispara una medida ya".
void test_FrameForceRead_VaAlSensorComoForceRead(void) {
    ConfigModule_RegisterSensor(10, COLA_A);

    uint8_t frame[] = {0x03, 10};
    Queue_Send_Stub(stub_capturar_cmd);
    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));

    TEST_ASSERT_EQUAL_INT(CMD_FORCE_READ, s_ultimo_cmd.type);
    TEST_ASSERT_EQUAL_UINT8(10, s_ultimo_cmd.sensor_id);
}

// Una trama ENABLE_SENSOR con value=1 debe llegar como CMD_SET_ENABLE
// con value=1 a la cola del sensor (encender).
void test_FrameEnableSensor_VaConFlagDeEnable(void) {
    ConfigModule_RegisterSensor(2, COLA_A);

    // [0x05][sid=2][enable=1]
    uint8_t frame[] = {0x05, 2, 1};
    Queue_Send_Stub(stub_capturar_cmd);
    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));

    TEST_ASSERT_EQUAL_INT(CMD_SET_ENABLE, s_ultimo_cmd.type);
    TEST_ASSERT_EQUAL_UINT32(1, s_ultimo_cmd.value);
}

// Una trama SET_WINDOW debe llegar al sensor con los dos extremos
// de la ventana (min y max) correctamente parseados.
void test_FrameSetWindow_VaConMinYMax(void) {
    ConfigModule_RegisterSensor(8, COLA_C);

    // [0x06][sid=8][mode=CONSERVACION=1][min=3000=0x0BB8][max=4200=0x1068]
    uint8_t frame[] = {0x06, 8, 1, 0x0B, 0xB8, 0x10, 0x68};
    Queue_Send_Stub(stub_capturar_cmd);
    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));

    TEST_ASSERT_EQUAL_INT(CMD_SET_WINDOW, s_ultimo_cmd.type);
    TEST_ASSERT_EQUAL_UINT32(3000, s_ultimo_cmd.value);
    TEST_ASSERT_EQUAL_UINT32(4200, s_ultimo_cmd.value2);
}

/* ================================================================== */
/*  Casos especiales: van fuera del routing                            */
/* ================================================================== */

// La trama SET_BATTERY_TH NO va a ningún sensor: ataca directamente
// al power_fsm pasando los dos umbrales nuevos.
void test_FrameSetBatteryThreshold_LlamaDirectamenteAlPowerFsm(void) {
    // [0x04][emerg=2800=0x0AF0][conserv=3100=0x0C1C]
    uint8_t frame[] = {0x04, 0x0A, 0xF0, 0x0C, 0x1C};

    PowerManager_PushBatteryThresholds_Expect(&dummy_power, 2800, 3100);
    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));
}

// La trama FORCE_SYSTEM_MODE hace broadcast: mete un cmd a TODAS
// las colas de sensores registrados, no solo a uno.
void test_FrameForceSystemMode_HaceBroadcastATodosLosSensores(void) {
    // Registramos tres sensores y forzamos modo EMERGENCIA.
    ConfigModule_RegisterSensor(0x10, COLA_A);
    ConfigModule_RegisterSensor(0x20, COLA_B);
    ConfigModule_RegisterSensor(0x30, COLA_C);

    // El broadcast debe meter UN cmd en cada cola.
    Queue_Send_ExpectAndReturn(COLA_A, NULL, QUEUE_NO_WAIT, true);
    Queue_Send_IgnoreArg_item();
    Queue_Send_ExpectAndReturn(COLA_B, NULL, QUEUE_NO_WAIT, true);
    Queue_Send_IgnoreArg_item();
    Queue_Send_ExpectAndReturn(COLA_C, NULL, QUEUE_NO_WAIT, true);
    Queue_Send_IgnoreArg_item();

    // [0x07][mode=EMERGENCIA=2]
    uint8_t frame[] = {0x07, POWER_STATE_EMERGENCIA};
    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));
}

/* ================================================================== */
/*  Tramas inválidas: ignorar sin crash                                */
/* ================================================================== */

// Si la trama es NULL, no debemos llamar a nadie ni romper nada.
void test_FrameNull_NoHaceNada(void) {
    // Sin Queue_Send Expect -> si se llama, falla.
    ConfigModule_HandleLoRaFrame(NULL, 5);
}

// Si la trama no llega a los bytes mínimos del opcode (por ejemplo,
// SET_TIMEOUT con 1 byte cuando necesita 6), se ignora sin error.
void test_FrameDemasiadoCorta_NoHaceNada(void) {
    uint8_t frame[] = {0x01};   // SET_TIMEOUT necesita 6 bytes; le damos 1
    ConfigModule_HandleLoRaFrame(frame, 1);
}

// Un opcode que no conocemos (0xFF, por ejemplo) tiene que tragarse
// silenciosamente sin enrutar nada.
void test_OpcodeDesconocido_SeIgnora(void) {
    uint8_t frame[] = {0xFF, 0xAA, 0xBB};
    ConfigModule_HandleLoRaFrame(frame, sizeof(frame));
}
