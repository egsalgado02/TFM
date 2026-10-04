/*
 * test_ina_fsm.c - pruebas de ina_fsm en el PC, sin placa.
 *
 * ciclo_ina() hace un ciclo completo (arranque y medida) sin fijar el orden
 * de las llamadas al driver. Se comprueba lo que llega a la cola de salida.
 *
 * Raíl, temporizadores y disparo de la medida se ignoran salvo donde son lo
 * que se prueba: soltar el raíl al desactivar a mitad de una medida y
 * rearmar el temporizador al cambiar de modo.
 *
 * El INA mide dos cosas en cada ciclo, corriente en mA y tensión de bus en
 * mV, y cada una tiene su id, su banda muerta y su estado de envío.
 *
 * Dobles: mock_ina_port.h, mock_payload_queue.h, mock_power_rail_manager.h
 * y mock_rtos_iface.h.
 *
 * Aquí se prueba: la banda muerta de cada magnitud por separado, los
 * reintentos en el arranque y en la lectura, soltar el raíl y que cada
 * instancia publique con sus propios ids.
 */

#include "unity.h"
#include "ina_fsm.h"
#include "fsm.h"
#include "mock_ina_port.h"
#include "mock_payload_queue.h"
#include "mock_power_rail_manager.h"
#include "mock_rtos_iface.h"
#include <string.h>

static ina_fsm_t mi_ina;

static const uint8_t      HW_ID           = 0;
static const uint8_t      PAYLOAD_ID_CORR = 20;
static const uint8_t      PAYLOAD_ID_VOLT = 21;
static const power_rail_t MY_RAIL         = RAIL_EXPANSION;

static const uint32_t def_t    [NUMERO_ESTADOS] = {10000, 30000, 90000};
static const uint8_t  def_db_ma[NUMERO_ESTADOS] = {5,     10,    20};
static const uint8_t  def_db_mv[NUMERO_ESTADOS] = {50,    50,    100};

/* ------------------------------------------------------------------ */
/*  Captura de mensajes encolados                                      */
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
/*  Stubs de lectura: inyectan corriente, voltaje y tick              */
/* ------------------------------------------------------------------ */

static int16_t  s_corriente;
static int16_t  s_voltaje;
static uint32_t s_tick;

static bool stub_read_data(uint8_t hw_id, int16_t* c, int16_t* v, int num_calls) {
    (void)hw_id;  (void)num_calls;
    *c = s_corriente;
    *v = s_voltaje;
    return true;
}

static uint32_t stub_get_tick(int num_calls) {
    (void)num_calls;
    return s_tick;
}

/* ------------------------------------------------------------------ */
/*  Stub con fallos para CheckStatusI2C                                */
/* ------------------------------------------------------------------ */

static int s_check_fail_remaining;

static bool stub_check_con_fallo(uint8_t hw_id, int num_calls) {
    (void)hw_id;  (void)num_calls;
    if (s_check_fail_remaining > 0) {
        s_check_fail_remaining--;
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */

void setUp(void) {
    memset(&mi_ina, 0, sizeof(ina_fsm_t));
    s_n_msgs               = 0;
    s_check_fail_remaining = 0;
    s_corriente            = 0;
    s_voltaje              = 0;
    s_tick                 = 0;
    Queue_Create_IgnoreAndReturn((queue_handle_t)0x01);
    // InaFSM_Init consulta GetTickMs (-> 0) y calcula first = period (rem=0).
    // Consumimos la llamada con el valor exacto que se pasará.
    InaPort_GetTickMs_Stub(stub_get_tick);
    InaPort_StartSleepTimer_Expect(HW_ID, def_t[POWER_STATE_CONSERVACION]);
    InaFSM_Init(&mi_ina, HW_ID, PAYLOAD_ID_CORR, PAYLOAD_ID_VOLT,
                MY_RAIL, def_t, def_db_ma, def_db_mv, 4);
}

void tearDown(void) {}

/* ------------------------------------------------------------------ */
/*  Helper: ciclo completo exitoso DORMIDO -> BOOT -> MEDIDA -> DORMIDO  */
/* ------------------------------------------------------------------ */

static void ciclo_ina(int16_t corriente, int16_t voltaje, uint32_t tick) {
    s_corriente = corriente;  s_voltaje = voltaje;  s_tick = tick;
    s_n_msgs    = 0;

    /* Lo que no se prueba aquí se ignora */
    PowerRail_Request_Ignore();
    InaPort_StartBootTimer_Ignore();
    InaPort_TriggerForcedReading_Ignore();
    InaPort_StartMedidaTimer_Ignore();
    // accion_recoger_y_dormir rearma el temporizador al final del ciclo. Aquí
    // da igual con qué valor; las pruebas de ApplyCmd lo exigen con _Expect.
    InaPort_StartSleepTimer_Ignore();
    PowerRail_Release_Ignore();

    /* Los datos del sensor entran con _Stub */
    InaPort_CheckStatusI2C_IgnoreAndReturn(true);
    InaPort_IsDataReady_IgnoreAndReturn(true);
    InaPort_ReadData_Stub(stub_read_data);
    InaPort_GetTickMs_Stub(stub_get_tick);

    /* Lo que sale se guarda en s_msgs */
    PayloadQueue_Enqueue_Stub(stub_enqueue);

    InaFSM_TriggerMedida(&mi_ina);
    fsm_fire((fsm_t*)&mi_ina);            /* DORMIDO -> ESPERANDO_BOOT   */
    InaFSM_NotifyBootTimer(&mi_ina);
    fsm_fire((fsm_t*)&mi_ina);            /* ESPERANDO_BOOT -> ESPERANDO_MEDIDA */
    InaFSM_NotifyMedidaTimer(&mi_ina);
    fsm_fire((fsm_t*)&mi_ina);            /* ESPERANDO_MEDIDA -> DORMIDO */
}

/* ------------------------------------------------------------------ */
/*  Inicialización y hw_id/rail                                        */
/* ------------------------------------------------------------------ */

// El ciclo de medida tiene que pedir el rail correcto y llamar al puerto
// con el hw_id configurado en todos los pasos. Al finalizar el ciclo,
// accion_recoger_y_dormir arma el siguiente despertar:
// verificamos que usa el hw_id y el timeout del modo actual (CONSERVACION).
void test_Init_TrasMedirUsaHwIdYRailCorrectos(void) {
    InaPort_TriggerForcedReading_Ignore();
    InaPort_CheckStatusI2C_IgnoreAndReturn(true);
    InaPort_IsDataReady_IgnoreAndReturn(true);
    InaPort_ReadData_Stub(stub_read_data);
    InaPort_GetTickMs_Stub(stub_get_tick);
    PayloadQueue_Enqueue_Stub(stub_enqueue);
    s_corriente = 100;  s_voltaje = 3700;  s_tick = 0;

    PowerRail_Request_Expect(MY_RAIL);
    InaPort_StartBootTimer_Expect(HW_ID);
    InaPort_StartMedidaTimer_Expect(HW_ID);
    PowerRail_Release_Expect(MY_RAIL);
    // Al finalizar el ciclo, accion_recoger_y_dormir arma el próximo timer
    // con el hw_id correcto y el timeout del modo actual (CONSERVACION).
    InaPort_StartSleepTimer_Expect(HW_ID, def_t[POWER_STATE_CONSERVACION]);

    InaFSM_TriggerMedida(&mi_ina);
    fsm_fire((fsm_t*)&mi_ina);
    InaFSM_NotifyBootTimer(&mi_ina);
    fsm_fire((fsm_t*)&mi_ina);
    InaFSM_NotifyMedidaTimer(&mi_ina);
    fsm_fire((fsm_t*)&mi_ina);
}

/* ------------------------------------------------------------------ */
/*  Primera medida: envío de corriente y voltaje                       */
/* ------------------------------------------------------------------ */

// La primera medida siempre se manda para ambas magnitudes.
// Verificamos que llegan dos mensajes con los payload_ids correctos y
// los valores que devolvió el sensor.
void test_PrimeraMedida_EnviaCorrienteYVoltaje(void) {
    ciclo_ina(500, 3700, 0);
    TEST_ASSERT_EQUAL_INT(2, s_n_msgs);

    /* corriente */
    TEST_ASSERT_EQUAL_UINT8(PAYLOAD_ID_CORR, s_msgs[0].sensor_id);
    TEST_ASSERT_EQUAL_INT(MOTIVO_ROUTINE,     s_msgs[0].motivo);
    TEST_ASSERT_EQUAL_INT16(500,              s_msgs[0].valor);

    /* voltaje */
    TEST_ASSERT_EQUAL_UINT8(PAYLOAD_ID_VOLT, s_msgs[1].sensor_id);
    TEST_ASSERT_EQUAL_INT(MOTIVO_ROUTINE,     s_msgs[1].motivo);
    TEST_ASSERT_EQUAL_INT16(3700,             s_msgs[1].valor);
}

/* ------------------------------------------------------------------ */
/*  Deadband de corriente                                              */
/* ------------------------------------------------------------------ */

// Si corriente y voltaje cambian menos que sus respectivos deadbands,
// no se manda ningún mensaje.
void test_SegundaMedida_AmbosDentroDeadband_NoEnvia(void) {
    ciclo_ina(500, 3700, 0);
    /* delta_corriente=3 < db_ma[CONSERVACION]=10
       delta_voltaje=20 < db_mv[CONSERVACION]=50  */
    ciclo_ina(503, 3720, 1000);
    TEST_ASSERT_EQUAL_INT(0, s_n_msgs);
}

// Si la corriente supera el deadband (delta=100>10) pero el voltaje no
// (delta=10<50), solo se manda el mensaje de corriente.
void test_CorrienteFueraDeadband_SoloEnviaCorreinte(void) {
    ciclo_ina(500, 3700, 0);
    ciclo_ina(600, 3710, 1000);
    TEST_ASSERT_EQUAL_INT(1, s_n_msgs);
    TEST_ASSERT_EQUAL_UINT8(PAYLOAD_ID_CORR, s_msgs[0].sensor_id);
    TEST_ASSERT_EQUAL_INT16(600, s_msgs[0].valor);
}

/* ------------------------------------------------------------------ */
/*  Deadband de voltaje                                                */
/* ------------------------------------------------------------------ */

// Si el voltaje supera el deadband (delta=100>50) pero la corriente no
// (delta=4<10), solo se manda el mensaje de voltaje.
void test_VoltajeFueraDeadband_SoloEnviaVoltaje(void) {
    ciclo_ina(500, 3700, 0);
    ciclo_ina(504, 3800, 1000);
    TEST_ASSERT_EQUAL_INT(1, s_n_msgs);
    TEST_ASSERT_EQUAL_UINT8(PAYLOAD_ID_VOLT, s_msgs[0].sensor_id);
    TEST_ASSERT_EQUAL_INT16(3800, s_msgs[0].valor);
}

/* ------------------------------------------------------------------ */
/*  Keepalive                                                          */
/* ------------------------------------------------------------------ */

// Si han pasado 24 h sin enviar, ambas magnitudes se mandan aunque no
// hayan cambiado nada.
void test_Keepalive_EnviaAmbosTras24h(void) {
    ciclo_ina(500, 3700, 0);
    ciclo_ina(500, 3700, INA_KEEPALIVE_MS + 1);
    TEST_ASSERT_EQUAL_INT(2, s_n_msgs);
}

/* ------------------------------------------------------------------ */
/*  Boot con reintentos                                                */
/* ------------------------------------------------------------------ */

// Si el primer check de I2C falla pero el segundo va bien, se completa
// el ciclo y se mandan los dos payloads (primera vez).
void test_BootRetry_ExitoEnSegundoIntento_EntregaPayloads(void) {
    s_check_fail_remaining = 1;
    s_corriente = 750;  s_voltaje = 3650;  s_tick = 0;  s_n_msgs = 0;

    PowerRail_Request_Ignore();
    PowerRail_Release_Ignore();
    InaPort_StartBootTimer_Ignore();
    InaPort_TriggerForcedReading_Ignore();
    InaPort_StartMedidaTimer_Ignore();
    InaPort_StartSleepTimer_Ignore();           /* accion_recoger_y_dormir */
    InaPort_CheckStatusI2C_Stub(stub_check_con_fallo);
    InaPort_IsDataReady_IgnoreAndReturn(true);
    InaPort_ReadData_Stub(stub_read_data);
    InaPort_GetTickMs_Stub(stub_get_tick);
    PayloadQueue_Enqueue_Stub(stub_enqueue);

    InaFSM_TriggerMedida(&mi_ina);
    fsm_fire((fsm_t*)&mi_ina);            /* -> ESPERANDO_BOOT          */
    InaFSM_NotifyBootTimer(&mi_ina);
    fsm_fire((fsm_t*)&mi_ina);            /* falla -> reintento         */
    InaFSM_NotifyBootTimer(&mi_ina);
    fsm_fire((fsm_t*)&mi_ina);            /* ok -> ESPERANDO_MEDIDA     */
    InaFSM_NotifyMedidaTimer(&mi_ina);
    fsm_fire((fsm_t*)&mi_ina);            /* -> DORMIDO con datos       */

    TEST_ASSERT_EQUAL_INT(2, s_n_msgs);
    TEST_ASSERT_EQUAL_INT16(750,  s_msgs[0].valor);   /* corriente */
    TEST_ASSERT_EQUAL_INT16(3650, s_msgs[1].valor);   /* voltaje   */
}

// Si el INA no responde tras todos los reintentos, se aborta sin enviar nada.
void test_BootAbort_TrasMaxIntentos_NoEnviaPayload(void) {
    s_n_msgs = 0;
    InaPort_CheckStatusI2C_IgnoreAndReturn(false);
    PowerRail_Request_Ignore();
    PowerRail_Release_Ignore();
    InaPort_StartBootTimer_Ignore();
    InaPort_StartSleepTimer_Ignore();           /* accion_abortar_por_fallo */
    PayloadQueue_Enqueue_Stub(stub_enqueue);

    InaFSM_TriggerMedida(&mi_ina);
    fsm_fire((fsm_t*)&mi_ina);

    for (int i = 0; i <= INA_MAX_BOOT_TRIES; i++) {
        InaFSM_NotifyBootTimer(&mi_ina);
        fsm_fire((fsm_t*)&mi_ina);
    }

    TEST_ASSERT_EQUAL_INT(0, s_n_msgs);
}

/* ------------------------------------------------------------------ */
/*  Gestión de recursos: PowerRail                                     */
/* ------------------------------------------------------------------ */

// Si llega un DISABLE mientras el INA está en plena medida, el rail se
// suelta antes de desactivarse para no dejarlo encendido para siempre.
void test_DisableDuranteMedida_LiberaElRail(void) {
    InaFSM_TriggerMedida(&mi_ina);
    PowerRail_Request_Expect(MY_RAIL);
    InaPort_StartBootTimer_Ignore();
    fsm_fire((fsm_t*)&mi_ina);            /* estado: ESPERANDO_BOOT */

    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type  = CMD_SET_ENABLE;
    cmd.value = 0;

    PowerRail_Release_Expect(MY_RAIL);
    InaFSM_ApplyCmd(&mi_ina, &cmd);
}

/* ------------------------------------------------------------------ */
/*  Configuración vía ApplyCmd                                         */
/* ------------------------------------------------------------------ */

// Al cambiar de modo el sleep timer se reprograma con el timeout del nuevo
// modo. Eso certifica que el campo modo_actual se actualizó correctamente.
void test_ApplyCmd_SetMode_ReprogramaTimerConElTimeoutDelNuevoModo(void) {
    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type  = CMD_SET_MODE;
    cmd.value = POWER_STATE_EMERGENCIA;

    InaPort_StartSleepTimer_Expect(HW_ID, def_t[POWER_STATE_EMERGENCIA]);
    InaFSM_ApplyCmd(&mi_ina, &cmd);
}
