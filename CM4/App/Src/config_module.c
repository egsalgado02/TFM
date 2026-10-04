#include "config_module.h"
#include <string.h>


#include "FreeRTOS.h"
#include "task.h"

// ==================================================================
//  Estado interno del módulo
// ==================================================================

typedef struct {
    queue_handle_t queue;
    void*          task_handle;  /* TaskHandle_t opaco. NULL = no notify. */
} sensor_entry_t;

// Tabla indexada directamente por sensor_id (sensor_id = índice del array).
// Si la cola es NULL, ese slot está libre.
static sensor_entry_t s_registry[MAX_SENSORES];

// Puntero al power_fsm. Lo necesitamos solo para los dos comandos
// especiales (umbral de batería y fuerza de modo de sistema).
static power_fsm_t* s_power_fsm = NULL;

// ==================================================================
//  Helpers privados de decodificación
// ==================================================================

// Lee 2 bytes en big-endian.
static inline uint16_t read_u16_be(const uint8_t* buf) {
    return (uint16_t)((buf[0] << 8) | buf[1]);
}

// Lee 3 bytes en big-endian (para timeouts de hasta ~16 millones de ms).
static inline uint32_t read_u24_be(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 16) |
           ((uint32_t)buf[1] <<  8) |
            (uint32_t)buf[2];
}



static bool route_to_sensor(const config_cmd_t* cmd) {
    if (cmd == NULL) return false;
    if (s_registry[cmd->sensor_id].queue == NULL) return false;

    bool ok = Queue_Send(s_registry[cmd->sensor_id].queue, cmd, QUEUE_NO_WAIT);

    /* Despertar la tarea del sensor destino AHORA MISMO, sin esperar al
     * proximo sleep_timer. Sin esto, el comando quedaba encolado y solo
     * se aplicaba en el ciclo natural del sensor. Con esto, force_read
     * desde el gateway dispara la lectura casi inmediata. */
    if (ok && s_registry[cmd->sensor_id].task_handle != NULL) {
        xTaskNotifyGive((TaskHandle_t)s_registry[cmd->sensor_id].task_handle);
    }
    return ok;
}

// ==================================================================
//  API pública
// ==================================================================

void ConfigModule_Init(power_fsm_t* p_power_fsm) {
    // limpiamos toda la tabla; cada slot empieza libre
    for (int i = 0; i < MAX_SENSORES; i++) {
        s_registry[i].queue       = NULL;
        s_registry[i].task_handle = NULL;
    }
    s_power_fsm = p_power_fsm;
}

bool ConfigModule_RegisterSensor(uint8_t sensor_id, queue_handle_t queue) {
    if (queue == NULL || sensor_id >= MAX_SENSORES) return false;
    s_registry[sensor_id].queue = queue;
    return true;
}

bool ConfigModule_RegisterTaskHandle(uint8_t sensor_id, void* task_handle) {
    if (sensor_id >= MAX_SENSORES) return false;
    s_registry[sensor_id].task_handle = task_handle;
    return true;
}

// Difusión 1:N a todos los sensores registrados.

void Config_ApplySystemMode(power_state_t mode) {
    config_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.type  = CMD_SET_MODE;
    cmd.mode  = mode;
    cmd.value = (uint32_t)mode;

    for (int i = 0; i < MAX_SENSORES; i++) {
        if (s_registry[i].queue != NULL) {       // slot ocupado
            cmd.sensor_id = (uint8_t)i;          // el id ES el índice
            bool ok = Queue_Send(s_registry[i].queue, &cmd, QUEUE_NO_WAIT);
            /* Despierto a la tarea para que aplique el modo ya. Si no, un
             * sensor en conservación, con periodo de 30 min, no se enteraría
             * del cambio hasta su siguiente despertar. Igual que en
             * route_to_sensor. Si no hay handle (en los tests, o un sensor
             * sin registrar), no se notifica y ya está. */
            if (ok && s_registry[i].task_handle != NULL) {
                xTaskNotifyGive((TaskHandle_t)s_registry[i].task_handle);
            }
        }
    }
}

// ==================================================================
//  Punto de entrada ÚNICO desde la radio
// ==================================================================

void ConfigModule_HandleLoRaFrame(const uint8_t* payload, uint8_t size) {
    if (payload == NULL || size < 2) return;

    lora_opcode_t opcode = (lora_opcode_t)payload[0];
    config_cmd_t  cmd;
    memset(&cmd, 0, sizeof(cmd));

    switch (opcode) {

        case LORA_SET_TIMEOUT: {
            // [0x01][sid:1][mode:1][ms:3 BE] = 6 bytes
            if (size < 6) break;
            cmd.sensor_id = payload[1];
            cmd.mode      = (power_state_t)payload[2];
            cmd.type      = CMD_SET_TIMEOUT;
            cmd.value     = read_u24_be(&payload[3]);
            route_to_sensor(&cmd);
            break;
        }

        case LORA_SET_THRESHOLD: {
            // [0x02][sid:1][mode:1][th:1] = 4 bytes
            if (size < 4) break;
            cmd.sensor_id = payload[1];
            cmd.mode      = (power_state_t)payload[2];
            cmd.type      = CMD_SET_THRESHOLD;
            cmd.value     = payload[3];
            route_to_sensor(&cmd);
            break;
        }

        case LORA_FORCE_READ: {
            // [0x03][sid:1] = 2 bytes (size>=2 ya garantizado arriba)
            cmd.sensor_id = payload[1];
            cmd.type      = CMD_FORCE_READ;
            route_to_sensor(&cmd);
            break;
        }

        case LORA_SET_BATTERY_TH: {
            // [0x04][emerg:2 BE][conserv:2 BE] = 5 bytes
            // Caso especial: no va por sensor_id, ataca al power_fsm directamente.
            if (size < 5 || s_power_fsm == NULL) break;
            uint16_t emerg   = read_u16_be(&payload[1]);
            uint16_t conserv = read_u16_be(&payload[3]);
            PowerManager_PushBatteryThresholds(s_power_fsm, emerg, conserv);
            break;
        }

        case LORA_ENABLE_SENSOR: {
            // [0x05][sid:1][en:1] = 3 bytes
            if (size < 3) break;
            cmd.sensor_id = payload[1];
            cmd.type      = CMD_SET_ENABLE;
            cmd.value     = (payload[2] != 0) ? 1u : 0u;
            route_to_sensor(&cmd);
            break;
        }

        case LORA_SET_WINDOW: {
            // [0x06][sid:1][mode:1][min:2 BE][max:2 BE] = 7 bytes
            if (size < 7) break;
            cmd.sensor_id = payload[1];
            cmd.mode      = (power_state_t)payload[2];
            cmd.type      = CMD_SET_WINDOW;
            cmd.value     = read_u16_be(&payload[3]);   // min_mv
            cmd.value2    = read_u16_be(&payload[5]);   // max_mv
            route_to_sensor(&cmd);
            break;
        }

        case LORA_FORCE_SYSTEM_MODE: {
            // [0x07][mode:1] = 2 bytes
            // Otro caso especial: broadcast directo a todos los sensores.
            if (size < 2) break;
            power_state_t mode = (power_state_t)payload[1];
            if (mode < NUMERO_ESTADOS) {
                Config_ApplySystemMode(mode);
            }
            break;
        }

        default:
            // opcode desconocido -> lo ignoramos en silencio
            break;
    }
}
