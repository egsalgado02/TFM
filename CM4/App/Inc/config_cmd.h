#ifndef INC_CONFIG_CMD_H_
#define INC_CONFIG_CMD_H_

#include <stdint.h>
#include "power_fsm.h"

// El "comando" es lo que viaja por las colas de configuración:
// desde el módulo de despacho (config_module) hacia cada sensor.
// Cada sensor (accel/bme/ina/adc/payload) recibe estos comandos en
// SU cola y los aplica en su propio contexto de tarea.

typedef enum {
    CMD_SET_TIMEOUT   = 0,  // 'value' = tiempo en ms, 'mode' = a qué modo aplica
    CMD_SET_MODE,           // 'value' = el nuevo modo (lo manda el power_fsm en broadcast)
    CMD_SET_ENABLE,         // 'value' = 0 (apagar) o 1 (encender)
    CMD_SET_THRESHOLD,      // 'value' = umbral; en accel es global (umbral de alarma),
                            //   en bme/ina es el deadband por modo
    CMD_FORCE_READ,         // sin campos: dispara una medida ya
    CMD_SET_WINDOW,         // 'value' = min_mv, 'value2' = max_mv (usado por ADC)
} cmd_type_t;

// Estructura plana. NO uso union etiquetada para mantenerlo simple;
// el "coste" es que algunos campos quedan sin usar según el tipo.
typedef struct {
    uint8_t       sensor_id;   // a qué sensor va (índice de la tabla de routing)
    power_state_t mode;        // modo de energía al que aplica (cuando aplica)
    cmd_type_t    type;        // el opcode del comando
    uint32_t      value;       // ver enum arriba para el significado
    uint32_t      value2;      // solo CMD_SET_WINDOW: max_mv
} config_cmd_t;

#endif /* INC_CONFIG_CMD_H_ */
