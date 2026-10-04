#include "app_tasks.h"

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "stm32wlxx_nucleo.h"

#include "fsm.h"
// Este fichero usa directamente CMSIS-OS/FreeRTOS, no rtos_iface.h: aquí están
// las tareas y eso no se prueba en PC.
#include "power_fsm.h"
#include "power_rail_manager.h"
#include "config_module.h"
#include "accel_fsm.h"
#include "accel_port.h"      // AccelPort_Init, se llama al arrancar StartAccelTask
#include "bme_fsm.h"
#include "ina_fsm.h"
#include "adc_fsm.h"
#include "payload_builder_fsm.h"
#include "sensor_ids.h"
#include "i2c1_lock.h"       // lock del bus I2C1 (expansión)
#include "i2c2_lock.h"       // lock del bus I2C2 (sensores de la placa)
#include "uart_link.h"       // enlace serie con el PC, en lugar de la radio
#include <string.h>

// Trazas por el puerto de consola, igual que en main.c
extern UART_HandleTypeDef hcom_uart[];
#define DBG(msg) HAL_UART_Transmit(&hcom_uart[0], (uint8_t*)(msg), strlen(msg), 200)

// ==================================================================
//  Las instancias de las FSM (las usan las tareas y los drivers)
// ==================================================================

power_fsm_t   mi_power;
accel_fsm_t   mi_accel;
bme_fsm_t     mi_bme;
ina_fsm_t     mi_ina_int;   /* hw_id 0: I2C2, RAIL_INTERNO (PA8) */
ina_fsm_t     mi_ina_exp;   /* hw_id 1: I2C1, RAIL_EXPANSION (PA1), rama del mikroBUS */
adc_fsm_t     mi_adc_fsm;
payload_fsm_t mi_payload;

// ==================================================================
//  Handles generados por CubeMX (app_freertos.c)
// ==================================================================

extern osMessageQueueId_t payloadQHandle;   // cola de salida
extern osMessageQueueId_t colaPowerHandle;  // cola de tensión, del ADC al power_fsm

/* Handles de las tareas (los crea CubeMX en app_freertos.c). Se registran
 * en config_module para que, cuando llegue un comando, pueda despertar a la
 * tarea del sensor al que va. */
extern osThreadId_t accelTaskHandle;
extern osThreadId_t bmeTaskHandle;
extern osThreadId_t inaTaskHandle;
extern osThreadId_t adcTaskHandle;
extern osThreadId_t payloadTaskHandle;

// ==================================================================
//  Arranque: se llama una vez desde main() antes de arrancar el planificador
// ==================================================================

void AppTasks_InitAll(void) {
    // Los locks de los dos buses I2C van lo primero: los drivers los usan en
    // cuanto arrancan sus tareas.
    I2C1_Lock_Init();
    I2C2_Lock_Init();

    PowerRail_Init();
    /* config_module va antes que el power_fsm. PowerFSMInit difunde el modo
     * inicial con Config_ApplySystemMode, que recorre la tabla de registro de
     * config_module, y esa tabla tiene que estar ya inicializada. */
    ConfigModule_Init(&mi_power);
    PowerFSMInit(&mi_power, POWER_STATE_CONSERVACION);

    /* ====================================================================
     * Periodos por modo, en ms, en el orden {abundancia, conservación,
     * emergencia}. Son valores para probar en banco, no los de despliegue:
     *
     *   abundancia:   pocos segundos, para que salgan tramas a menudo y se
     *                 vea el contenido.
     *   conservación: 30 minutos. El nodo arranca en conservación, así que
     *                 en la práctica no publica nada solo. Cualquier trama
     *                 que salga antes es por un comando enviado desde el
     *                 PC, y así se ve que la bajada funciona.
     *   emergencia:   entre los dos.
     *
     * Con force_mode abundancia se pasa a los periodos cortos y publica todo.
     *
     * Los periodos reales salen del modelo energético (hoja de cálculo) y
     * se pueden cambiar por comando sin reprogramar. Los que había antes
     * de estas pruebas eran:
     *   accel_t = { 60000, 120000, 600000 }
     *   bme_t   = { 30000, 60000,  180000 }
     *   ina_t   = { 10000, 30000,  90000 }
     *   adc_t   = { 1000,  1000,   1000 }
     *   ventana del payload = 60000
     * ==================================================================== */

    // Acelerómetro
    const uint32_t accel_t [NUMERO_ESTADOS] = { 15000, 1800000, 60000 };  /* abund / cons=30min / emerg */
    const uint8_t  accel_db[NUMERO_ESTADOS] = { 5, 10, 20 };
    AccelFSM_Init(&mi_accel, /*threshold*/ 4, accel_t, accel_db, 4);

    // BME680
    const uint32_t bme_t [NUMERO_ESTADOS] = { 10000, 1800000, 30000 };    /* abund / cons=30min / emerg */
    const uint8_t  bme_db[NUMERO_ESTADOS] = { 3, 5, 10 };
    BmeFSM_Init(&mi_bme, bme_t, bme_db, BME_KEEPALIVE_MS, 4);

    // Los dos INA238: el interno y el de la rama de expansión
    const uint32_t ina_t    [NUMERO_ESTADOS] = { 3000, 1800000, 15000 };  /* abund / cons=30min / emerg */
    const uint8_t  ina_db_ma[NUMERO_ESTADOS] = { 5, 10, 20 };
    const uint8_t  ina_db_mv[NUMERO_ESTADOS] = { 50, 100, 200 };
    InaFSM_Init(&mi_ina_int, /*hw_id*/ 0, ID_INA_INT_CORRIENTE, ID_INA_INT_VOLTAJE,
                RAIL_INTERNO, ina_t, ina_db_ma, ina_db_mv, 4);
    InaFSM_Init(&mi_ina_exp, /*hw_id*/ 1, ID_INA_CORRIENTE, ID_INA_VOLTAJE,
                RAIL_EXPANSION, ina_t, ina_db_ma, ina_db_mv, 4);

    // ADC: tensión del supercondensador
    const uint16_t adc_min[NUMERO_ESTADOS] = { 3200, 3000, 2800 };
    const uint16_t adc_max[NUMERO_ESTADOS] = { 4500, 4200, 3500 };
    const uint32_t adc_t  [NUMERO_ESTADOS] = { 1000, 1800000, 1000 };  /* abund / cons=30min / emerg */
    AdcFSM_Init(&mi_adc_fsm, adc_min, adc_max, adc_t, 4);

    // Payload builder: duración de la ventana de agregación en cada modo.
    // Mismo criterio de banco que los sensores: 10 s en abundancia para que
    // se vean tramas, 30 min en conservación para que no envíe solo y 60 s
    // en emergencia. Los de despliegue son los del modelo energético: un
    // envío cada 90 min en abundancia, cada 6 h en conservación y uno al
    // día en emergencia.
    const uint32_t payload_t[NUMERO_ESTADOS] = { 10000, 1800000, 60000 };
    PayloadFSM_Init(&mi_payload, payload_t, 4);

    // Registro en config_module de la cola de comandos de cada FSM
    ConfigModule_RegisterSensor(ROUTE_ID_ACCEL,   mi_accel.cmd_queue);
    ConfigModule_RegisterSensor(ROUTE_ID_BME,     mi_bme.cmd_queue);
    ConfigModule_RegisterSensor(ROUTE_ID_INA,     mi_ina_exp.cmd_queue);
    ConfigModule_RegisterSensor(ROUTE_ID_INA_INT, mi_ina_int.cmd_queue);
    ConfigModule_RegisterSensor(ROUTE_ID_ADC,     mi_adc_fsm.cmd_queue);
    ConfigModule_RegisterSensor(ROUTE_ID_PAYLOAD, mi_payload.cmd_queue);

    /* Y de la tarea de cada una. Si solo se encolara el comando, la tarea
     * seguiría dormida hasta su siguiente periodo (media hora en
     * conservación). Con el handle, config_module la despierta al encolar
     * y el comando se aplica en el momento. Las dos instancias del INA
     * comparten tarea. */
    ConfigModule_RegisterTaskHandle(ROUTE_ID_ACCEL,   accelTaskHandle);
    ConfigModule_RegisterTaskHandle(ROUTE_ID_BME,     bmeTaskHandle);
    ConfigModule_RegisterTaskHandle(ROUTE_ID_INA,     inaTaskHandle);
    ConfigModule_RegisterTaskHandle(ROUTE_ID_INA_INT, inaTaskHandle);
    ConfigModule_RegisterTaskHandle(ROUTE_ID_ADC,     adcTaskHandle);
    ConfigModule_RegisterTaskHandle(ROUTE_ID_PAYLOAD, payloadTaskHandle);

    // Todos empiezan en conservación
    Config_ApplySystemMode(POWER_STATE_CONSERVACION);

    // Enlace serie con el PC, en lugar de la radio. Va al final porque
    // necesita que main() ya haya inicializado USART1.
    UART_Link_Init();
}

// ==================================================================
//  Tareas de los sensores
//
//  Las tareas no tienen lógica. Están bloqueadas en ulTaskNotifyTake, sin
//  gastar CPU, hasta que alguien las despierta con una notificación: un
//  temporizador del driver, la interrupción del acelerómetro o
//  config_module al llegar un comando. Al despertar hacen siempre lo mismo:
//
//    1. vaciar su cola de comandos (DrainCmdQueue)
//    2. disparar su FSM una vez (fsm_fire)
//
//  Primero los comandos: si ha llegado un force_read, el fire de esta
//  misma vuelta ya lo atiende, y no hay que esperar al siguiente despertar.
//
//  ulTaskNotifyTake con pdFALSE resta uno al contador en vez de ponerlo a
//  cero. Si llegan dos avisos a la vez (la alarma y el temporizador, por
//  ejemplo), la tarea da dos vueltas, una por aviso. Con pdTRUE se
//  juntarían en una sola, fsm_fire solo haría una transición y el segundo
//  evento se quedaría marcado sin que nadie lo atendiera.
// ==================================================================

void StartAccelTask(void* arg) {
    (void)arg;
    /* El LIS2DW12 se configura aquí y no en main(): AccelPort_Init usa
     * vTaskDelay y el lock del bus, y los dos necesitan el planificador en
     * marcha. Sin esta llamada el sensor se queda apagado y sin interrupción. */
    AccelPort_Init();
    for (;;) {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        AccelFSM_DrainCmdQueue(&mi_accel);
        fsm_fire((fsm_t*)&mi_accel);
    }
}

void StartBmeTask(void* arg) {
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        BmeFSM_DrainCmdQueue(&mi_bme);
        fsm_fire((fsm_t*)&mi_bme);
    }
}

void StartInaTask(void* arg) {
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        /* Una sola tarea para los dos INA. Cada aviso da una vuelta a los
         * dos; al que no tiene nada pendiente el fire no le hace nada. */
        InaFSM_DrainCmdQueue(&mi_ina_int);
        fsm_fire((fsm_t*)&mi_ina_int);
        InaFSM_DrainCmdQueue(&mi_ina_exp);
        fsm_fire((fsm_t*)&mi_ina_exp);
    }
}

void StartAdcTask(void* arg) {
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        AdcFSM_DrainCmdQueue(&mi_adc_fsm);
        fsm_fire((fsm_t*)&mi_adc_fsm);

        // El parpadeo del LED verde está quitado: en la Nucleo ese LED va en
        // PA9, que en mi placa es la TX del USART1, y el toggle se cargaba la
        // telemetría. Está explicado en main.c, donde está BSP_LED_Init.
        // BSP_LED_Toggle(LED_GREEN);
    }
}

// ==================================================================
//  Tareas de sistema
// ==================================================================

void StartPowerTask(void* arg) {
    (void)arg;
    for (;;) {
        uint16_t mv;
        // Espera a que el ADC mande una tensión y dispara el power_fsm.
        // Ojo: es un fire por tensión, así que si acaban de llegar umbrales
        // nuevos, este fire los aplica y la tensión se evalúa en el siguiente.
        if (osMessageQueueGet(colaPowerHandle, &mv, NULL, osWaitForever) == osOK) {
            PowerManager_PushVoltaje(&mi_power, mv);
            fsm_fire((fsm_t*)&mi_power);
        }
    }
}

// Cada cuánto como mucho se miran los bytes recibidos por el enlace serie.
// Es la latencia máxima de un comando que llega del PC.
#define TICK_DRENAJE  pdMS_TO_TICKS(5000u)

void StartPayloadTask(void* arg) {
    (void)arg;

    // Plazo absoluto: el instante en que hay que enviar, lleguen los mensajes
    // que lleguen. Se renueva cuando vence, no con cada mensaje. Si en cada
    // vuelta se esperara un tiempo fijo, cada mensaje que llegara volvería a
    // empezar la cuenta y con sensores publicando a menudo no se enviaría
    // nunca.
    TickType_t deadline = xTaskGetTickCount()
                        + pdMS_TO_TICKS(mi_payload.lora_report_timeout_ms);

    for (;;) {
        payload_msg_t msg;

        // Cuánto falta para el plazo. Si ya ha pasado, remaining = 0 y no se
        // espera. Se calcula con el periodo en vigor, que cambia con el modo.
        TickType_t period    = pdMS_TO_TICKS(mi_payload.lora_report_timeout_ms);
        TickType_t now       = xTaskGetTickCount();
        TickType_t elapsed   = now - (deadline - period);
        TickType_t remaining = (elapsed >= period) ? 0 : (period - elapsed);

        // La espera se hace en trozos de como mucho TICK_DRENAJE. Los bytes
        // del enlace serie se procesan al final de este bucle; si la tarea se
        // quedara esperando el plazo entero, un comando del PC tardaría en
        // aplicarse lo que dure la ventana del modo, hasta media hora en
        // conservación. Troceando, la latencia queda en 5 s y los periodos de
        // envío no cambian.
        // Es una solución para el banco. Lo suyo sería procesar los bytes
        // desde el callback de recepción, como se hacía con la radio desde
        // la interrupción de IPCC.
        TickType_t espera = (remaining > TICK_DRENAJE) ? TICK_DRENAJE : remaining;

        if (osMessageQueueGet(payloadQHandle, &msg, NULL, espera) == osOK) {
            // Ha llegado una medida antes del plazo: se la paso a la FSM,
            // sin tocar el plazo.
            mi_payload.rx_msg  = msg;
            mi_payload.events |= EV_NEW_MESSAGE;
        } else if (remaining <= TICK_DRENAJE) {
            // Ha vencido el plazo: la FSM envía lo acumulado y empieza la
            // siguiente ventana.
            PayloadFSM_TriggerTimeout(&mi_payload);
            deadline = xTaskGetTickCount()
                     + pdMS_TO_TICKS(mi_payload.lora_report_timeout_ms);
        }
        // Si no es ninguna de las dos, solo ha vencido el trozo de espera.
        // No hay evento y el fire no hace nada; esta vuelta solo sirve para
        // el UART_Link_DrainRx del final.

        /* Comandos antes que el fire, igual que en los sensores: si ha
         * llegado un force_read para el payload builder, marca el envío y
         * el fire de esta misma vuelta ya empaqueta y envía. */
        PayloadFSM_DrainCmdQueue(&mi_payload);
        fsm_fire((fsm_t*)&mi_payload);

        // Bytes recibidos del PC: cada trama completa va a
        // ConfigModule_HandleLoRaFrame, igual que las de la radio.
        UART_Link_DrainRx();
    }
}

// ==================================================================
//  No hay tarea de LoRa.
//
//  En el doble núcleo la pila LoRaWAN corre entera en el M0+. El M4 solo
//  la inicializa (en StartDefaultTask, app_freertos.c) y para enviar el
//  payload builder llama a LoRa_SendPayload (lora_tx.c), que con la radio
//  llama a LmHandlerSend y la petición cruza al M0+ por MBMUX.
//
//  Lo que se recibe llega al M4 como callback OnRxData (lora_app.c),
//  desde la interrupción de IPCC, y de ahí va a ConfigModule_HandleLoRaFrame.
//
//  En doble núcleo no se llama a LmHandlerProcess: eso lo hace el M0+.
// ==================================================================
