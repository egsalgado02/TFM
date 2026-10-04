#ifndef CONFIG_MODULE_H_
#define CONFIG_MODULE_H_

#include <stdint.h>
#include <stdbool.h>
#include "config_cmd.h"
#include "rtos_iface.h"
#include "power_fsm.h"

#define MAX_SENSORES 255

// Opcodes del protocolo LoRa de entrada.
// Antes vivían en lora_command_parser.h; ahora que el parser está aquí
// dentro, los movemos a este header.
//
// Formato de las tramas:
//   LORA_SET_TIMEOUT       [0x01][sensor_id:1][mode:1][timeout_ms:3 BE]   6 bytes
//   LORA_SET_THRESHOLD     [0x02][sensor_id:1][mode:1][threshold:1]       4 bytes
//   LORA_FORCE_READ        [0x03][sensor_id:1]                            2 bytes
//   LORA_SET_BATTERY_TH    [0x04][emerg_mv:2 BE][conserv_mv:2 BE]         5 bytes
//   LORA_ENABLE_SENSOR     [0x05][sensor_id:1][enable:1]                  3 bytes
//   LORA_SET_WINDOW        [0x06][sensor_id:1][mode:1][min_mv:2 BE][max_mv:2 BE]  7 bytes
//   LORA_FORCE_SYSTEM_MODE [0x07][mode:1]                                 2 bytes
typedef enum {
    LORA_SET_TIMEOUT        = 0x01,
    LORA_SET_THRESHOLD      = 0x02,
    LORA_FORCE_READ         = 0x03,
    LORA_SET_BATTERY_TH     = 0x04,
    LORA_ENABLE_SENSOR      = 0x05,
    LORA_SET_WINDOW         = 0x06,
    LORA_FORCE_SYSTEM_MODE  = 0x07,
} lora_opcode_t;

// Inicializa el módulo. Hay que llamarla antes de nada.
//   p_power_fsm : puntero al power_fsm global, necesario para los dos
//                 comandos que no van por sensor (umbral de batería y
//                 fuerza de modo de sistema).
void ConfigModule_Init(power_fsm_t* p_power_fsm);

// Registra la cola de comandos de un sensor bajo su sensor_id.
// El sensor_id ES el índice del array interno (lookup O(1) por índice).
// Devuelve false si la cola es NULL o el id está fuera de rango.
bool ConfigModule_RegisterSensor(uint8_t sensor_id, queue_handle_t queue);

// Asocia el handle de la tarea del sensor a su sensor_id. Cuando llega un
// comando desde el UART/LoRa, route_to_sensor hace Queue_Send al buzon del
// sensor Y despues xTaskNotifyGive al handle para despertar la tarea AHORA
// mismo, en lugar de esperar al proximo sleep_timer. Sin esto, force_read
// desde el gateway no tenia efecto perceptible: el comando se encolaba y
// se aplicaba en el siguiente ciclo natural del sensor.
//
// El tipo void* es para no acoplar este header a FreeRTOS. El .c lo casteara
// a TaskHandle_t. Si task_handle es NULL, no se notifica (compatibilidad).
bool ConfigModule_RegisterTaskHandle(uint8_t sensor_id, void* task_handle);

// Punto de entrada ÚNICO desde la radio: bytes -> decodificación ->
// despacho (routing 1:1, broadcast 1:N o caso especial al power_fsm).
// Aquí dentro vive todo lo que antes hacía lora_command_parser.
void ConfigModule_HandleLoRaFrame(const uint8_t* payload, uint8_t size);

// Broadcast del modo de energía a TODOS los sensores registrados.
// Lo llama el power_fsm cuando cambia de estado.
void Config_ApplySystemMode(power_state_t mode);

#endif /* CONFIG_MODULE_H_ */
