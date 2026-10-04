/*
 * test_adc_fsm.c - pruebas de adc_fsm en el PC, sin placa.
 *
 * run_ciclo() hace un ciclo de medida con la tensión que se le pase y
 * apunta si se ha mandado algo a la cola del power_fsm y qué.
 *
 * Encender y apagar el raíl y armar el temporizador son parte del ciclo
 * pero no lo que se prueba, así que en las pruebas de medida se ignoran.
 * Solo se exigen (con _Expect) en las pruebas donde eso sí es lo que se
 * comprueba, como que al cambiar de modo se rearme el temporizador con el
 * periodo nuevo.
 *
 * Dobles: mock_adc_port.h, mock_power_queue.h, mock_power_rail_manager.h,
 * mock_rtos_iface.h y mock_payload_queue.h (la tensión también sale en la
 * telemetría).
 *
 * Aquí se prueba: la ventana de validez por los dos extremos, el envío de
 * mantenimiento, el efecto de cambiar la ventana y el periodo por modo.
 */

#include "unity.h"
#include "adc_fsm.h"
#include "fsm.h"
#include "mock_adc_port.h"
#include "mock_power_queue.h"
#include "mock_power_rail_manager.h"
#include "mock_rtos_iface.h"
#include "mock_payload_queue.h"
#include <string.h>

static adc_fsm_t mi_adc;

/* CONSERVACION: ventana [3000, 4200] mV */
static const uint16_t def_min[NUMERO_ESTADOS] = {3200, 3000, 2800};
static const uint16_t def_max[NUMERO_ESTADOS] = {4500, 4200, 3500};
static const uint32_t def_t  [NUMERO_ESTADOS] = {30000, 60000, 120000};

#define KEEPALIVE_24H (24UL * 60UL * 60UL * 1000UL)

/* ------------------------------------------------------------------ */
/*  Captura de envíos a ColaPower                                      */
/* ------------------------------------------------------------------ */

static uint16_t s_voltaje_enviado;
static int      s_n_envios;

static void stub_cola_power(uint16_t v, int num_calls) {
    (void)num_calls;
    s_voltaje_enviado = v;
    s_n_envios++;
}

/* ------------------------------------------------------------------ */
/*  Stubs de puerto: inyectan valores de batería y tick                */
/* ------------------------------------------------------------------ */

static uint16_t s_voltaje_bateria;
static uint32_t s_tick_adc;

static bool stub_leer_bateria(uint16_t* p_mv, int num_calls) {
    (void)num_calls;
    *p_mv = s_voltaje_bateria;
    return true;
}

static uint32_t stub_get_tick_adc(int num_calls) {
    (void)num_calls;
    return s_tick_adc;
}

/* ------------------------------------------------------------------ */

void setUp(void) {
    memset(&mi_adc, 0, sizeof(adc_fsm_t));
    s_n_envios        = 0;
    s_voltaje_enviado = 0;
    s_tick_adc        = 0;
    Queue_Create_IgnoreAndReturn((queue_handle_t)0x01);
    // AdcFSM_Init consulta GetTickMs (-> 0) y calcula first = period (rem=0).
    // Consumimos la llamada con el valor exacto que se pasará.
    AdcPort_GetTickMs_Stub(stub_get_tick_adc);
    AdcPort_StartSleepTimer_Expect(def_t[POWER_STATE_CONSERVACION]);
    AdcFSM_Init(&mi_adc, def_min, def_max, def_t, 4);
}

void tearDown(void) {}

/* ------------------------------------------------------------------ */
/*  Helper: ciclo completo de medida ADC                               */
/*  Lo que no se prueba se ignora, los datos entran con _Stub y lo que */
/*  sale se cuenta en s_n_envios                                      */
/* ------------------------------------------------------------------ */

static void run_ciclo(uint16_t voltaje, uint32_t tick) {
    s_voltaje_bateria = voltaje;
    s_tick_adc        = tick;
    s_n_envios        = 0;

    AdcFSM_TriggerMedida(&mi_adc);

    PowerRail_Request_Ignore();
    AdcPort_LeerBateriaBloqueante_Stub(stub_leer_bateria);
    AdcPort_GetTickMs_Stub(stub_get_tick_adc);
    ColaPower_EnviarVoltaje_Stub(stub_cola_power);
    /* Lo que va a la cola de salida aquí da igual: lo que se prueba es lo
     * que llega a la cola del power_fsm. */
    PayloadQueue_Enqueue_IgnoreAndReturn(true);
    PowerRail_Release_Ignore();
    // accion_medir rearma el temporizador al final del ciclo. Aquí da igual
    // con qué valor; las pruebas de ApplyCmd lo exigen con _Expect.
    AdcPort_StartSleepTimer_Ignore();

    fsm_fire((fsm_t*)&mi_adc);
}

/* ------------------------------------------------------------------ */

// La primera lectura del ADC SIEMPRE se manda al power_fsm, aunque
// el voltaje esté dentro de la ventana. Es la regla "primera vez:
// fuerzo el envío" para que el power tenga un valor inicial.
void test_PrimeraLectura_SiempreEnviaAPowerQueue(void) {
    // ever_sent arranca en false, así que la primera medida fuerza envío
    // aunque el voltaje esté dentro de la ventana. No hace falta leer ever_sent
    // por dentro: basta con verificar que se envió.
    run_ciclo(3600, 0);
    TEST_ASSERT_EQUAL_INT(1, s_n_envios);
}

// Si el voltaje cae por debajo del límite inferior de la ventana,
// se manda al power_fsm para que decida si hay que cambiar de modo.
void test_VoltajeFueraVentana_PorDebajo_EnviaAPowerQueue(void) {
    run_ciclo(3600, 0);              /* primera: establece baseline */
    run_ciclo(2500, 1000);           /* debajo de min=3000 -> envía */
    TEST_ASSERT_EQUAL_INT(1, s_n_envios);
    TEST_ASSERT_EQUAL_UINT16(2500, s_voltaje_enviado);
}

// Mismo caso pero por arriba: el voltaje supera el máximo (p.ej.
// se ha conectado un cargador). También se notifica al power.
void test_VoltajeFueraVentana_PorArriba_EnviaAPowerQueue(void) {
    run_ciclo(3600, 0);
    run_ciclo(4300, 1000);           /* encima de max=4200 -> envía */
    TEST_ASSERT_EQUAL_INT(1, s_n_envios);
}

// Si el voltaje está dentro de la ventana (estado normal de la
// batería), no hace falta mandar nada. Solo cuando algo se sale o
// pasa el keepalive.
void test_VoltajeDentroVentana_NoEnviaHastaKeepalive(void) {
    run_ciclo(3600, 0);              /* primera -> envía */
    run_ciclo(3700, 1000);           /* dentro de [3000,4200], poco tiempo -> no envía */
    TEST_ASSERT_EQUAL_INT(0, s_n_envios);
}

// Cuando pasan 24 h sin envíos, el keepalive fuerza uno aunque el
// voltaje siga estando perfecto. Así el power sabe que el ADC vive.
void test_Keepalive_EnviaAunqueDentroDeVentana_AlPasar24h(void) {
    run_ciclo(3600, 0);
    run_ciclo(3600, KEEPALIVE_24H + 1);   /* mismo valor, pero han pasado 24 h */
    TEST_ASSERT_EQUAL_INT(1, s_n_envios);
}

/* ------------------------------------------------------------------ */
/*  Configuración vía ApplyCmd                                         */
/* ------------------------------------------------------------------ */

// Cuando llega un SET_WINDOW, la nueva ventana se aplica de verdad.
// Lo verificamos viendo que un voltaje que ANTES estaba dentro de
// la ventana ahora queda fuera y se manda.
void test_ApplyCmd_SetWindow_LaNuevaVentanaCambiaElComportamiento(void) {
    // con la nueva ventana [3300, 4000], un voltaje de 4100 ya está fuera
    // y debe enviarse, cuando antes (ventana [3000, 4200]) no lo haría.
    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type   = CMD_SET_WINDOW;
    cmd.mode   = POWER_STATE_CONSERVACION;
    cmd.value  = 3300;
    cmd.value2 = 4000;
    AdcFSM_ApplyCmd(&mi_adc, &cmd);

    run_ciclo(3600, 0);              // primera lectura: siempre se envía
    run_ciclo(4100, 1000);           // 4100 > nuevo max=4000 -> envía
    TEST_ASSERT_EQUAL_INT(1, s_n_envios);
    TEST_ASSERT_EQUAL_UINT16(4100, s_voltaje_enviado);
}

// Igual que en el accel: SET_TIMEOUT solo afecta al modo indicado.
// Verificamos que al pasar a ese modo, el sleep timer usa el valor
// nuevo (no el original).
void test_ApplyCmd_SetTimeout_AfectaSoloAlModoIndicado(void) {
    // Cambiamos el timeout de EMERGENCIA y probamos su efecto: tras pasar
    // a EMERGENCIA, el sleep usa el nuevo valor.
    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type  = CMD_SET_TIMEOUT;
    cmd.mode  = POWER_STATE_EMERGENCIA;
    cmd.value = 300000;
    AdcFSM_ApplyCmd(&mi_adc, &cmd);

    config_cmd_t mode;  memset(&mode, 0, sizeof(mode));
    mode.type  = CMD_SET_MODE;
    mode.value = POWER_STATE_EMERGENCIA;
    AdcPort_StartSleepTimer_Expect(300000);
    AdcFSM_ApplyCmd(&mi_adc, &mode);
}

// Al cambiar de modo, el sleep timer se reprograma con el timeout
// que corresponde al nuevo modo.
void test_ApplyCmd_SetMode_ReprogramaTimerConElTimeoutDelNuevoModo(void) {
    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type  = CMD_SET_MODE;
    cmd.value = POWER_STATE_EMERGENCIA;

    // Efecto observable: el timer se reprograma con el timeout del nuevo modo.
    AdcPort_StartSleepTimer_Expect(def_t[POWER_STATE_EMERGENCIA]);
    AdcFSM_ApplyCmd(&mi_adc, &cmd);
}

// Tras un DISABLE, los TriggerMedida no provocan llamadas al
// hardware. Si por error hubiera alguna, CMock fallaría.
void test_ApplyCmd_Disable_SensorNoRealizaMedidas(void) {
    config_cmd_t cmd;  memset(&cmd, 0, sizeof(cmd));
    cmd.type  = CMD_SET_ENABLE;
    cmd.value = 0;
    AdcFSM_ApplyCmd(&mi_adc, &cmd);

    // Sin Stubs/Ignore activos: si el FSM intenta llamar a hardware, CMock falla.
    AdcFSM_TriggerMedida(&mi_adc);
    fsm_fire((fsm_t*)&mi_adc);
}

// Al reactivar tras un DISABLE, el sensor debe armar el sleep
// timer con el timeout del modo actual y quedar listo para medir.
void test_ApplyCmd_Reenable_SensorVuelveAMedir(void) {
    config_cmd_t dis;  memset(&dis, 0, sizeof(dis));
    dis.type = CMD_SET_ENABLE;  dis.value = 0;
    AdcFSM_ApplyCmd(&mi_adc, &dis);

    config_cmd_t en;  memset(&en, 0, sizeof(en));
    en.type = CMD_SET_ENABLE;  en.value = 1;

    // Al reactivar: el timer se reprograma con el timeout del modo actual.
    AdcPort_StartSleepTimer_Expect(def_t[POWER_STATE_CONSERVACION]);
    AdcFSM_ApplyCmd(&mi_adc, &en);
}
