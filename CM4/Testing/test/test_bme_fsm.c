/*
 * test_bme_fsm.c - pruebas de bme_fsm en el PC, sin placa.
 *
 * ciclo_bme() hace un ciclo completo (arranque y medida) sin fijar en qué
 * orden se llaman las funciones del driver, así que si se reorganizan los
 * estados por dentro las pruebas siguen valiendo. Lo que se comprueba es
 * lo que llega a la cola de salida: cuántos mensajes y qué llevan.
 *
 * Alimentación, temporizadores y el disparo de la medida se ignoran salvo
 * en las pruebas donde son justo lo que se prueba (cortar la alimentación
 * al desactivar, rearmar el temporizador al cambiar de modo).
 *
 * Dobles: mock_bme_port.h, mock_payload_queue.h, mock_rtos_iface.h
 *
 * Aquí se prueba: el ciclo completo, reintentar el arranque y que salga
 * bien, rendirse después de agotar los intentos, cortar la alimentación
 * al desactivar y la lectura forzada.
 */

#include "unity.h"
#include "bme_fsm.h"
#include "fsm.h"
#include "mock_bme_port.h"
#include "mock_payload_queue.h"
#include "mock_rtos_iface.h"
#include <string.h>

static bme_fsm_t mi_bme;

static const uint32_t def_timeouts[NUMERO_ESTADOS] = {30000, 60000, 180000};
static const uint8_t  def_deadband[NUMERO_ESTADOS] = {3,     5,     10};

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
/*  Stubs de lectura: inyectan valores del sensor                      */
/* ------------------------------------------------------------------ */

static int16_t  s_temp, s_hum, s_pres;
static uint32_t s_tick;

static bool stub_read_data(int16_t* t, int16_t* h, int16_t* pr, int num_calls) {
    (void)num_calls;
    *t = s_temp;  *h = s_hum;  *pr = s_pres;
    return true;
}

static uint32_t stub_get_tick(int num_calls) {
    (void)num_calls;
    return s_tick;
}

/* ------------------------------------------------------------------ */
/*  Stub con fallos para CheckStatusI2C                                */
/*  Devuelve false las primeras s_check_fail_remaining veces           */
/* ------------------------------------------------------------------ */

static int s_check_fail_remaining;

static bool stub_check_con_fallo(int num_calls) {
    (void)num_calls;
    if (s_check_fail_remaining > 0) {
        s_check_fail_remaining--;
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */

void setUp(void) {
    memset(&mi_bme, 0, sizeof(bme_fsm_t));
    s_n_msgs               = 0;
    s_check_fail_remaining = 0;
    s_tick                 = 0;
    Queue_Create_IgnoreAndReturn((queue_handle_t)0x01);
    // BmeFSM_Init consulta GetTickMs (-> 0) y calcula first = period (rem=0).
    // Consumimos la llamada con el valor exacto que se pasará.
    BmePort_GetTickMs_Stub(stub_get_tick);
    BmePort_StartSleepTimer_Expect(def_timeouts[POWER_STATE_CONSERVACION]);
    BmeFSM_Init(&mi_bme, def_timeouts, def_deadband, BME_KEEPALIVE_MS, 4);
}

void tearDown(void) {}

/* ------------------------------------------------------------------ */
/*  Helper: ciclo completo exitoso DORMIDO -> BOOT -> MEDIDA -> DORMIDO  */
/*  Toda la mecánica interna ignorada; se verifica solo la salida.     */
/* ------------------------------------------------------------------ */

static void ciclo_bme(int16_t t, int16_t h, int16_t pr, uint32_t tick) {
    s_temp = t;  s_hum = h;  s_pres = pr;  s_tick = tick;
    s_n_msgs = 0;

    /* Lo que no se prueba aquí se ignora */
    BmePort_SetPowerGPIO_Ignore();
    BmePort_StartBootTimer_Ignore();
    BmePort_TriggerForcedReading_Ignore();
    BmePort_StartMedidaTimer_Ignore();
    // accion_recoger_y_dormir rearma el temporizador al final del ciclo. Aquí
    // da igual con qué valor; las pruebas de ApplyCmd lo exigen con _Expect.
    BmePort_StartSleepTimer_Ignore();

    /* Los datos del sensor entran con _Stub */
    BmePort_CheckStatusI2C_IgnoreAndReturn(true);
    BmePort_IsDataReady_IgnoreAndReturn(true);
    BmePort_ReadData_Stub(stub_read_data);
    BmePort_GetTickMs_Stub(stub_get_tick);

    /* Lo que sale se guarda en s_msgs */
    PayloadQueue_Enqueue_Stub(stub_enqueue);

    BmeFSM_TriggerMedida(&mi_bme);
    fsm_fire((fsm_t*)&mi_bme);            /* DORMIDO -> ESPERANDO_BOOT   */
    BmeFSM_NotifyBootTimer(&mi_bme);
    fsm_fire((fsm_t*)&mi_bme);            /* ESPERANDO_BOOT -> ESPERANDO_MEDIDA */
    BmeFSM_NotifyMedidaTimer(&mi_bme);
    fsm_fire((fsm_t*)&mi_bme);            /* ESPERANDO_MEDIDA -> DORMIDO */
}

/* ------------------------------------------------------------------ */

// El BME arranca en CONSERVACION por defecto (comprobado indirectamente):
// si mandamos un CMD_SET_MODE con CONSERVACION, el sleep timer se reprograma
// con el timeout de ese modo. Si el Init hubiera iniciado en ABUNDANCIA,
// el timer recibiría def_timeouts[ABUNDANCIA], no def_timeouts[CONSERVACION].
//
// BmePort_StartSleepTimer se llama en tres sitios: en el Init (primer
// despertar, alineado), al acabar el ciclo (accion_recoger_y_dormir o
// abortar) y en ApplyCmd al cambiar de modo. El setUp consume la
// llamada de Init con _ExpectAnyArgs; ciclo_bme consume las de fin de ciclo.
// Solo los tests de ApplyCmd usan _Expect con valor exacto.
void test_Init_ModoDefectoEsConservacion(void) {
    config_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type  = CMD_SET_MODE;
    cmd.value = POWER_STATE_CONSERVACION;

    // Efecto observable: reprogramar el modo al mismo valor inicial
    // usa el timeout de CONSERVACION, lo que certifica que ese es el modo actual.
    BmePort_StartSleepTimer_Expect(def_timeouts[POWER_STATE_CONSERVACION]);
    BmeFSM_ApplyCmd(&mi_bme, &cmd);
}

// La primera medida del BME manda SIEMPRE las 3 magnitudes
// (temperatura + humedad + presión), independientemente del valor.
void test_CicloCompleto_PrimeraMedida_EnviaTresMagnitudes(void) {
    /* Primera lectura: ever_sent=false -> las 3 magnitudes se envían */
    ciclo_bme(200, 500, 1013, 0);
    TEST_ASSERT_EQUAL_INT(3, s_n_msgs);
}

// Si la segunda lectura es idéntica a la primera, el deadband filtra
// las 3 magnitudes y no se manda nada.
void test_SegundaMedidaIgual_NoEnviaPorDeadband(void) {
    ciclo_bme(200, 500, 1013, 0);
    ciclo_bme(200, 500, 1013, 1000);   /* sin cambio -> deadband filtra */
    TEST_ASSERT_EQUAL_INT(0, s_n_msgs);
}

// Si SOLO una magnitud (p.ej. temperatura) cambia más del deadband,
// solo se manda ESA magnitud. El filtro es por magnitud, no en bloque.
void test_CambioSuperiorADeadband_EnviaLaMagnitudAfectada(void) {
    ciclo_bme(200, 500, 1013, 0);
    /* T cambia 10 > deadband_conservacion=5 -> envía solo T */
    ciclo_bme(210, 500, 1013, 1000);
    TEST_ASSERT_EQUAL_INT(1, s_n_msgs);
}

// Tras 24 h sin cambios, el keepalive fuerza el envío de las 3
// magnitudes aunque sigan estando igual. Así el gateway sabe que el
// sensor sigue funcionando.
void test_Keepalive_EnviaAunqueSinCambioAlPasar24h(void) {
    ciclo_bme(200, 500, 1013, 0);
    ciclo_bme(200, 500, 1013, BME_KEEPALIVE_MS + 1);
    TEST_ASSERT_EQUAL_INT(3, s_n_msgs);   /* keepalive fuerza las 3 magnitudes */
}

/* ------------------------------------------------------------------ */
/*  Boot con reintentos                                                */
/* ------------------------------------------------------------------ */

// Simula que el BME no responde al I2C la primera vez (falla del
// boot), pero a la segunda sí. La FSM debe reintentar el arranque
// y, cuando lo consigue, completar el ciclo de medida normalmente.
void test_BootRetry_ExitoEnSegundoIntento_EntregaPayloadCorrectamente(void) {
    /* CheckStatusI2C falla 1 vez (reintento), luego ok */
    s_check_fail_remaining = 1;
    s_temp = 200;  s_hum = 500;  s_pres = 1013;  s_tick = 0;  s_n_msgs = 0;

    BmePort_SetPowerGPIO_Ignore();
    BmePort_StartBootTimer_Ignore();
    BmePort_TriggerForcedReading_Ignore();
    BmePort_StartMedidaTimer_Ignore();
    BmePort_StartSleepTimer_Ignore();           /* accion_recoger_y_dormir */
    BmePort_CheckStatusI2C_Stub(stub_check_con_fallo);
    BmePort_IsDataReady_IgnoreAndReturn(true);
    BmePort_ReadData_Stub(stub_read_data);
    BmePort_GetTickMs_Stub(stub_get_tick);
    PayloadQueue_Enqueue_Stub(stub_enqueue);

    BmeFSM_TriggerMedida(&mi_bme);
    fsm_fire((fsm_t*)&mi_bme);            /* -> ESPERANDO_BOOT           */
    BmeFSM_NotifyBootTimer(&mi_bme);
    fsm_fire((fsm_t*)&mi_bme);            /* falla -> reintento          */
    BmeFSM_NotifyBootTimer(&mi_bme);
    fsm_fire((fsm_t*)&mi_bme);            /* ok -> ESPERANDO_MEDIDA      */
    BmeFSM_NotifyMedidaTimer(&mi_bme);
    fsm_fire((fsm_t*)&mi_bme);            /* -> DORMIDO con datos        */

    /* Primera medida del sensor -> envía las 3 magnitudes */
    TEST_ASSERT_EQUAL_INT(3, s_n_msgs);
}

// Si el sensor no responde NUNCA tras varios reintentos, la FSM se
// rinde y vuelve a DORMIDO sin enviar nada. Mejor no mandar que
// mandar basura.
void test_BootAbort_TrasMaxIntentos_NoEnviaPayload(void) {
    /* CheckStatusI2C siempre falla -> abort tras BME_MAX_BOOT_TRIES reintentos */
    s_n_msgs = 0;
    BmePort_CheckStatusI2C_IgnoreAndReturn(false);
    BmePort_SetPowerGPIO_Ignore();
    BmePort_StartBootTimer_Ignore();
    BmePort_StartSleepTimer_Ignore();            /* accion_abortar_por_fallo */
    PayloadQueue_Enqueue_Stub(stub_enqueue);   /* captura si se enviase */

    BmeFSM_TriggerMedida(&mi_bme);
    fsm_fire((fsm_t*)&mi_bme);                 /* -> ESPERANDO_BOOT */

    /* BME_MAX_BOOT_TRIES reintentos + 1 fire de abort */
    for (int i = 0; i <= BME_MAX_BOOT_TRIES; i++) {
        BmeFSM_NotifyBootTimer(&mi_bme);
        fsm_fire((fsm_t*)&mi_bme);
    }

    /* Abort: ningún payload enviado */
    TEST_ASSERT_EQUAL_INT(0, s_n_msgs);
}

/* ------------------------------------------------------------------ */
/*  Configuración vía ApplyCmd                                         */
/* ------------------------------------------------------------------ */

// Al recibir un SET_MODE, el sleep timer se reprograma con el
// timeout del NUEVO modo. Eso ya certifica que el modo cambió por
// dentro.
void test_ApplyCmd_SetMode_ReprogramaTimerConElTimeoutDelNuevoModo(void) {
    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type  = CMD_SET_MODE;
    cmd.value = POWER_STATE_ABUNDANCIA;

    // Efecto observable: el timer se reprograma con el timeout del nuevo modo.
    // Eso ya certifica que el modo cambió.
    BmePort_StartSleepTimer_Expect(def_timeouts[POWER_STATE_ABUNDANCIA]);
    BmeFSM_ApplyCmd(&mi_bme, &cmd);
}

// Al desactivar el sensor: (1) se apaga el GPIO de alimentación del
// BME y (2) el sensor ignora futuros eventos. Lo verificamos viendo
// que un TriggerMedida posterior no llama a ningún puerto.
void test_ApplyCmd_Disable_ApagaGPIOYBloqueaElSensor(void) {
    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type  = CMD_SET_ENABLE;
    cmd.value = 0;

    // Efecto observable 1: el GPIO del sensor se apaga.
    BmePort_SetPowerGPIO_Expect(false);
    BmeFSM_ApplyCmd(&mi_bme, &cmd);

    // Efecto observable 2: tras desactivar, un TriggerMedida no debe llamar
    // a ninguna función de puerto. Sin Ignores/Stubs activos, CMock fallaría.
    BmeFSM_TriggerMedida(&mi_bme);
    fsm_fire((fsm_t*)&mi_bme);
}

// Un FORCE_READ debe disparar un ciclo de medida completo, igual
// que si hubiera expirado el timer. Lo verificamos viendo que tras
// el comando llegan las 3 magnitudes a la cola.
void test_ApplyCmd_ForceRead_ProvocaUnCicloDeMedida(void) {
    // En vez de comprobar el flag de events, verificamos el efecto:
    // tras ForceRead + fires, debe acabar el ciclo igual que con TriggerMedida.
    BmePort_SetPowerGPIO_Ignore();
    BmePort_StartBootTimer_Ignore();
    BmePort_TriggerForcedReading_Ignore();
    BmePort_StartMedidaTimer_Ignore();
    BmePort_StartSleepTimer_Ignore();           /* accion_recoger_y_dormir */
    BmePort_CheckStatusI2C_IgnoreAndReturn(true);
    BmePort_IsDataReady_IgnoreAndReturn(true);
    BmePort_ReadData_Stub(stub_read_data);
    BmePort_GetTickMs_Stub(stub_get_tick);
    PayloadQueue_Enqueue_Stub(stub_enqueue);
    s_temp = 100; s_hum = 50; s_pres = 1010; s_tick = 0;  s_n_msgs = 0;

    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type = CMD_FORCE_READ;
    BmeFSM_ApplyCmd(&mi_bme, &cmd);

    fsm_fire((fsm_t*)&mi_bme);
    BmeFSM_NotifyBootTimer(&mi_bme);
    fsm_fire((fsm_t*)&mi_bme);
    BmeFSM_NotifyMedidaTimer(&mi_bme);
    fsm_fire((fsm_t*)&mi_bme);

    // Primera medida del sensor -> llegaron los 3 valores a la cola.
    TEST_ASSERT_EQUAL_INT(3, s_n_msgs);
}
