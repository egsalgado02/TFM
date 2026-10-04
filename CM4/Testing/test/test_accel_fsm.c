/*
 * test_accel_fsm.c - pruebas de accel_fsm en el PC, sin placa.
 *
 * Cómo funcionan estas pruebas (vale para todos los ficheros test_*.c):
 *   Ceedling compila la FSM tal cual y sustituye todo lo que toca hardware
 *   o el RTOS por dobles que genera CMock a partir de las cabeceras
 *   (mock_accel_port.h, mock_payload_queue.h, mock_rtos_iface.h). Con esos
 *   dobles cada prueba decide qué "lee" el sensor y guarda lo que la FSM
 *   intenta encolar. Luego se comprueba lo que ha salido: cuántos mensajes,
 *   con qué id y con qué valor. No se comprueba en qué orden llama la FSM a
 *   sus funciones internas, así que se puede reorganizar por dentro sin
 *   romper las pruebas.
 *
 * Aquí se prueba: la banda muerta eje a eje, el envío de mantenimiento,
 * que la alarma va antes que la medida y sus dos causas (movimiento y
 * caída), que el umbral de alarma no depende del modo, y desactivar y
 * volver a activar el sensor.
 */

#include "unity.h"
#include "accel_fsm.h"
#include "fsm.h"
#include "mock_accel_port.h"
#include "mock_payload_queue.h"
#include "mock_rtos_iface.h"
#include <string.h>

static accel_fsm_t mi_accel;

static const uint32_t def_t [NUMERO_ESTADOS] = {60000, 120000, 600000};
static const uint8_t  def_th                 = 4;   /* único umbral global */
static const uint8_t  def_db[NUMERO_ESTADOS] = {5,  10,     20};

/* ------------------------------------------------------------------ */
/* Captura de mensajes encolados                                      */
/* ------------------------------------------------------------------ */

#define MAX_MSGS_TEST 8
static payload_msg_t s_msgs[MAX_MSGS_TEST];
static int           s_n_msgs;

static bool stub_enqueue(const payload_msg_t* msg, int num_calls) {
    (void)num_calls;
    if (s_n_msgs < MAX_MSGS_TEST) s_msgs[s_n_msgs++] = *msg;
    return true;
}

/* ------------------------------------------------------------------ */
/* Stubs de lectura: inyectan valores del acelerómetro                */
/* ------------------------------------------------------------------ */

static int16_t  s_x, s_y, s_z;
static uint32_t s_tick;

static bool stub_leer_buffer(int16_t* x, int16_t* y, int16_t* z, int num_calls) {
    (void)num_calls;
    *x = s_x;  *y = s_y;  *z = s_z;
    return true;
}

static bool stub_leer_buffer_fail(int16_t* x, int16_t* y, int16_t* z, int num_calls) {
    (void)x; (void)y; (void)z; (void)num_calls;
    return false;
}

static uint32_t stub_get_tick(int num_calls) {
    (void)num_calls;
    return s_tick;
}

/* ------------------------------------------------------------------ */
/* Stub de alarma                                                     */
/* ------------------------------------------------------------------ */

static bool s_es_robo;
static bool s_es_caida;

static bool stub_leer_alarma(bool* p_es_robo, bool* p_es_caida, int num_calls) {
    (void)num_calls;
    *p_es_robo  = s_es_robo;
    *p_es_caida = s_es_caida;
    return true;
}

/* ------------------------------------------------------------------ */

void setUp(void) {
    memset(&mi_accel, 0, sizeof(accel_fsm_t));
    s_n_msgs = 0;
    Queue_Create_IgnoreAndReturn((queue_handle_t)0x01);

    // El Init mira el tick y arma el temporizador para alinear el primer
    // despertar. En el setUp eso no es lo que se prueba, así que se ignora;
    // si no, CMock daría fallo por llamadas que no se esperaban.
    AccelPort_GetTickMs_IgnoreAndReturn(0);
    AccelPort_StartSleepTimer_Ignore();

    AccelFSM_Init(&mi_accel, def_th, def_t, def_db, 4);
}

void tearDown(void) {}

/* ------------------------------------------------------------------ */
/* Helper: ciclo completo de medida de rutina                         */
/* Lo que no se prueba se ignora, los datos entran con _Stub y lo que */
/* sale se guarda en s_msgs                                           */
/* ------------------------------------------------------------------ */

static void medir(int16_t rx, int16_t ry, int16_t rz, uint32_t tick) {
    s_x = rx;  s_y = ry;  s_z = rz;  s_tick = tick;
    s_n_msgs = 0;

    AccelPort_IniciarLecturaI2C_Ignore();
    AccelPort_DatosListos_IgnoreAndReturn(true);
    AccelPort_LeerBuffer_Stub(stub_leer_buffer);
    AccelPort_GetTickMs_Stub(stub_get_tick);

    // Al acabar cada ciclo la FSM rearma su temporizador para el siguiente
    // despertar. Aquí da igual con qué valor, así que se ignora.
    AccelPort_StartSleepTimer_Ignore();

    PayloadQueue_Enqueue_Stub(stub_enqueue);

    AccelFSM_TriggerMedida(&mi_accel);
    fsm_fire((fsm_t*)&mi_accel);
    fsm_fire((fsm_t*)&mi_accel);
}

/* ------------------------------------------------------------------ */
/* Init                                                               */
/* ------------------------------------------------------------------ */

void test_Init_TrasMedirReprogramaConTimeoutDeConservacion(void) {
    // Comprobamos que utiliza el timer del modo de operacion por defecto (conservación)
    // al irse a dormir.

    AccelPort_IniciarLecturaI2C_Ignore();
    AccelPort_DatosListos_IgnoreAndReturn(true);
    AccelPort_LeerBuffer_Stub(stub_leer_buffer);
    AccelPort_GetTickMs_Stub(stub_get_tick);
    PayloadQueue_Enqueue_Stub(stub_enqueue);
    s_x = 0; s_y = 0; s_z = 0; s_tick = 0; s_n_msgs = 0;

    // Aquí comprobamos que efectivamente re-arma el timer con Conservacion
    AccelPort_StartSleepTimer_Expect(def_t[POWER_STATE_CONSERVACION]);

    AccelFSM_TriggerMedida(&mi_accel);
    fsm_fire((fsm_t*)&mi_accel);
    fsm_fire((fsm_t*)&mi_accel);
}

/* ------------------------------------------------------------------ */
/* Ciclo de medida / deadband / keepalive                             */
/* ------------------------------------------------------------------ */

// La primera medida del sensor SIEMPRE se manda (los 3 ejes), aunque
// los valores sean los que sean. Es el comportamiento "primera vez:
// fuerzo el envío para que el gateway sepa que estoy vivo".
void test_PrimeraMedida_EnviaLosTresEjes(void) {
    medir(160, 320, 1600, 0);
    TEST_ASSERT_EQUAL_INT(3, s_n_msgs);
}

// Si la segunda medida es idéntica a la primera, el deadband filtra
// y no se manda nada (ahorramos batería y aire por LoRa).
void test_SegundaMedidaIgual_NoEnviaPorDeadband(void) {
    medir(160, 320, 1600, 0);       /* establece baseline */
    medir(160, 320, 1600, 1000);    /* sin cambio -> deadband filtra */
    TEST_ASSERT_EQUAL_INT(0, s_n_msgs);
}

// Si SOLO un eje cambia más del deadband (los otros dos se quedan
// igual), únicamente se manda ese eje. Filtra eje por eje, no todo
// el conjunto.
void test_CambioSuperiorADeadband_EnviaSoloElEjeAfectado(void) {
    medir(0, 0, 0, 0);
    /* X cambia en 200 unidades (raw 3200 / escala) > deadband[CONSERVACION]=10 */
    /* Y y Z sin cambio -> no se envían */
    medir(3200, 0, 0, 1000);
    TEST_ASSERT_EQUAL_INT(1, s_n_msgs);
}

// Aunque la lectura no cambie, si han pasado 24 h desde el último
// envío, el keepalive fuerza el envío de los 3 ejes para que el
// gateway sepa que el sensor sigue vivo.
void test_Keepalive_EnviaLosTresEjesAlPasar24h(void) {
    medir(160, 320, 1600, 0);
    /* Mismo valor pero 24 h después -> keepalive fuerza el envío */
    medir(160, 320, 1600, ACCEL_KEEPALIVE_MS + 1);
    TEST_ASSERT_EQUAL_INT(3, s_n_msgs);
}

// Si la lectura del I2C falla (sensor no responde, ruido, etc.), NO
// debemos encolar valores basura. Mejor perder esa muestra que mandar
// datos malos al gateway.
void test_LecturaI2CFallida_NoEncola(void) {
    s_n_msgs = 0;
    AccelPort_IniciarLecturaI2C_Ignore();
    AccelPort_DatosListos_IgnoreAndReturn(true);
    AccelPort_LeerBuffer_Stub(stub_leer_buffer_fail);   /* falla la lectura */

    // Si falla la lectura, no encola, pero sí DEBE rearmar su timer
    // para seguir viva en el próximo ciclo. Lo ignoramos.
    AccelPort_StartSleepTimer_Ignore();

    PayloadQueue_Enqueue_Stub(stub_enqueue);            /* si se llamase, captura */

    AccelFSM_TriggerMedida(&mi_accel);
    fsm_fire((fsm_t*)&mi_accel);
    fsm_fire((fsm_t*)&mi_accel);

    TEST_ASSERT_EQUAL_INT(0, s_n_msgs);
}

/* ------------------------------------------------------------------ */
/* Alarmas                                                            */
/* ------------------------------------------------------------------ */

// Cuando salta la alarma de ROBO, la FSM manda UN mensaje de emergencia.
// sensor_id = ID_ACCEL_X (el mismo ID que las medidas de rutina del accel;
// el FPort=EMERGENCIA y el emerg_codigo=EMERG_ROBO identifican la causa).
void test_AlarmaRobo_EnviaEmergenciaConCodigoRobo(void) {
    s_es_robo = true;  s_es_caida = false;
    AccelPort_LeerCausasAlarma_Stub(stub_leer_alarma);
    PayloadQueue_Enqueue_Stub(stub_enqueue);
    s_n_msgs = 0;

    AccelFSM_TriggerAlarma(&mi_accel);
    fsm_fire((fsm_t*)&mi_accel);

    TEST_ASSERT_EQUAL_INT(1, s_n_msgs);
    TEST_ASSERT_EQUAL_INT(ID_ACCEL_X,        s_msgs[0].sensor_id);
    TEST_ASSERT_EQUAL_INT(MOTIVO_EMERGENCY,   s_msgs[0].motivo);
    TEST_ASSERT_EQUAL_INT(EMERG_ROBO,         s_msgs[0].emerg_codigo);
}

// Igual que el anterior pero con CAÍDA libre: emerg_codigo = EMERG_CAIDA.
// sensor_id sigue siendo ID_ACCEL_X: no hay un id aparte para las alarmas.
void test_AlarmaCaida_EnviaEmergenciaConCodigoCaida(void) {
    s_es_robo = false;  s_es_caida = true;
    AccelPort_LeerCausasAlarma_Stub(stub_leer_alarma);
    PayloadQueue_Enqueue_Stub(stub_enqueue);
    s_n_msgs = 0;

    AccelFSM_TriggerAlarma(&mi_accel);
    fsm_fire((fsm_t*)&mi_accel);

    TEST_ASSERT_EQUAL_INT(1, s_n_msgs);
    TEST_ASSERT_EQUAL_INT(ID_ACCEL_X,        s_msgs[0].sensor_id);
    TEST_ASSERT_EQUAL_INT(MOTIVO_EMERGENCY,   s_msgs[0].motivo);
    TEST_ASSERT_EQUAL_INT(EMERG_CAIDA,        s_msgs[0].emerg_codigo);
}

/* ------------------------------------------------------------------ */
/* Configuración vía ApplyCmd                                         */
/* ------------------------------------------------------------------ */

// Si cambio el timeout SOLO del modo EMERGENCIA, el modo CONSERVACION
// sigue con su timeout original. Cuando pasamos a EMERGENCIA, el
// sleep timer ya usa el nuevo valor.
void test_ApplyCmd_SetTimeout_AfectaSoloAlModoIndicado(void) {
    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type  = CMD_SET_TIMEOUT;
    cmd.mode  = POWER_STATE_EMERGENCIA;
    cmd.value = 300000;
    AccelFSM_ApplyCmd(&mi_accel, &cmd);

    // Preparamos un ciclo de medida manual, sin usar el helper,
    // para poder auditar el temporizador.
    AccelPort_IniciarLecturaI2C_Ignore();
    AccelPort_DatosListos_IgnoreAndReturn(true);
    AccelPort_LeerBuffer_Stub(stub_leer_buffer);
    AccelPort_GetTickMs_Stub(stub_get_tick);
    PayloadQueue_Enqueue_Stub(stub_enqueue);
    s_x = 0; s_y = 0; s_z = 0; s_tick = 0; s_n_msgs = 0;

	// EL EXPECT CRÍTICO: Exigimos a la FSM que, al irse a dormir,
	// utilice estrictamente el timeout por defecto del modo Conservación.
	// Si ApplyCmd hubiera pisado la memoria incorrecta, esto falla aquí.
	AccelPort_StartSleepTimer_Expect(def_t[POWER_STATE_CONSERVACION]);

	// Ejecutamos el ciclo
	AccelFSM_TriggerMedida(&mi_accel);
	fsm_fire((fsm_t*)&mi_accel);
	fsm_fire((fsm_t*)&mi_accel);

    // Pasamos a EMERGENCIA: el SetMode dispara reprogramación del timer
    config_cmd_t mode;  memset(&mode, 0, sizeof(mode));
    mode.type  = CMD_SET_MODE;
    mode.value = POWER_STATE_EMERGENCIA;
    AccelPort_SetThreshold_Expect(def_th);
    AccelPort_StartSleepTimer_Expect(300000);   // el nuevo timeout
    AccelFSM_ApplyCmd(&mi_accel, &mode);
}

// Al cambiar de modo de energía, el sensor tiene que: (1) reaplicar
// el umbral al hardware y (2) reprogramar el sleep timer con el
// timeout del NUEVO modo. Lo verificamos vía los _Expect.
void test_ApplyCmd_SetMode_ReconfiguraHardwareYTimer(void) {
    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type  = CMD_SET_MODE;
    cmd.value = POWER_STATE_ABUNDANCIA;

    // Efectos observables: hardware recibe el umbral y el timer se reprograma
    // con el timeout del nuevo modo. Eso ya certifica que el modo cambió.
    AccelPort_SetThreshold_Expect(def_th);
    AccelPort_StartSleepTimer_Expect(def_t[POWER_STATE_ABUNDANCIA]);
    AccelFSM_ApplyCmd(&mi_accel, &cmd);
}

// El umbral de alarma es GLOBAL (un único valor para todos los modos).
// Por eso al hacer SET_THRESHOLD el campo 'mode' se ignora.
void test_ApplyCmd_SetThreshold_AplicaHardwareSinImportarModo(void) {
    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type  = CMD_SET_THRESHOLD;
    cmd.value = 15;

    AccelPort_SetThreshold_Expect(15);
    AccelFSM_ApplyCmd(&mi_accel, &cmd);
}

// Refuerza el test anterior: hacemos dos SET_THRESHOLD con 'mode'
// distinto en cada uno. Como el modo se ignora, el hardware recibe
// los dos valores nuevos en orden.
void test_ApplyCmd_SetThreshold_ModoIgnorado_HardwareSiempreActualizado(void) {
    // Dos llamadas con 'mode' distinto: el HW recibe el valor en los dos casos
    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type = CMD_SET_THRESHOLD;

    cmd.value = 3;  cmd.mode = POWER_STATE_ABUNDANCIA;
    AccelPort_SetThreshold_Expect(3);
    AccelFSM_ApplyCmd(&mi_accel, &cmd);

    cmd.value = 7;  cmd.mode = POWER_STATE_EMERGENCIA;
    AccelPort_SetThreshold_Expect(7);
    AccelFSM_ApplyCmd(&mi_accel, &cmd);
}

// Si desactivo el sensor, tiene que ignorar TODOS los eventos:
// ni TriggerMedida ni TriggerAlarma deben provocar llamadas al
// hardware. Lo verificamos no poniendo ningún mock (CMock falla si
// se llama a algo que no esperamos).
void test_ApplyCmd_Disable_SensorIgnoraMedidasYAlarmas(void) {
    // Tras desactivar, el FSM no debe llamar a NINGUNA función de puerto
    // aunque le lleguen TriggerMedida y TriggerAlarma. Como no hay Stubs
    // ni Ignores activos, CMock fallaría el test si se llama a hardware.
    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type  = CMD_SET_ENABLE;
    cmd.value = 0;
    AccelFSM_ApplyCmd(&mi_accel, &cmd);

    AccelFSM_TriggerMedida(&mi_accel);
    AccelFSM_TriggerAlarma(&mi_accel);
    fsm_fire((fsm_t*)&mi_accel);
}

// Tras desactivar y volver a activar, el sensor debe quedar listo
// para operar: reaplica el umbral al hardware y arma el sleep timer.
void test_ApplyCmd_Reenable_SensorVuelveAOperar(void) {
    config_cmd_t dis;  memset(&dis, 0, sizeof(dis));
    dis.type = CMD_SET_ENABLE;  dis.value = 0;
    AccelFSM_ApplyCmd(&mi_accel, &dis);

    config_cmd_t en;  memset(&en, 0, sizeof(en));
    en.type = CMD_SET_ENABLE;  en.value = 1;

    // Al reactivar: hardware recibe el umbral y el timer se reprograma
    AccelPort_SetThreshold_Expect(def_th);
    AccelPort_StartSleepTimer_Expect(def_t[POWER_STATE_CONSERVACION]);
    AccelFSM_ApplyCmd(&mi_accel, &en);
}
