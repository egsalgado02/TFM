/*
 * i2c1_lock.h - lock del bus I2C1, el de expansión.
 *
 * En I2C1 (PB6/PB7) están el INA238 de expansión, el zócalo mikroBUS y el
 * conector de expansión. Es un bus aparte del de los sensores de la placa
 * para que un periférico externo que se cuelgue no bloquee la telemetría
 * interna, y por eso tiene su propio lock.
 *
 * La HAL no reparte el bus entre tareas: si dos tareas llaman a HAL_I2C_*
 * a la vez, las transferencias se mezclan. Este lock las pone en fila.
 *
 * Uso:
 *   I2C1_Lock_Init() una sola vez en AppTasks_InitAll, antes de que
 *   ninguna tarea use el bus.
 *
 *   En cada acceso a hi2c1:
 *     I2C1_Lock_Take();
 *     HAL_I2C_Mem_Read/Write(...);
 *     I2C1_Lock_Give();
 *
 * Es un mutex de FreeRTOS, así que ni Take ni Give se pueden llamar desde
 * una interrupción: el mutex tiene herencia de prioridad y necesita saber
 * qué tarea lo tiene, y en una interrupción no hay tarea. Si hace falta
 * leer del bus después de una interrupción, la interrupción avisa a la
 * tarea con una notificación y la lectura la hace la tarea.
 */

#ifndef INC_I2C1_LOCK_H_
#define INC_I2C1_LOCK_H_

#include <stdbool.h>

/* Crea el mutex. Si se llama más de una vez, solo cuenta la primera. */
void I2C1_Lock_Init(void);

/* Coge el mutex; espera lo que haga falta a que esté libre.
 * Devuelve false solo si todavía no se ha creado. */
bool I2C1_Lock_Take(void);

/* Suelta el mutex. */
void I2C1_Lock_Give(void);

#endif /* INC_I2C1_LOCK_H_ */
