/*
 * i2c2_lock.h - lock del bus I2C2, el de los sensores de la placa.
 *
 * En I2C2 (PA11/PA12) están el LIS2DW12, el BME680 y el INA238 interno,
 * y cada uno se atiende desde su propia tarea. La HAL no reparte el bus
 * entre tareas: si dos tareas llaman a HAL_I2C_* a la vez, las
 * transferencias se mezclan. Este lock hace que entren de una en una.
 *
 * Uso:
 *   I2C2_Lock_Init() una sola vez en AppTasks_InitAll, antes de que
 *   ninguna tarea use el bus.
 *
 *   En cada acceso a hi2c2:
 *     I2C2_Lock_Take();
 *     HAL_I2C_Mem_Read/Write(...);
 *     I2C2_Lock_Give();
 *
 * Take no se puede llamar desde una interrupción, porque puede bloquear.
 * Si una interrupción necesita leer del bus, lo que hace es avisar a la
 * tarea y es la tarea la que lee (así lo hace el acelerómetro).
 */

#ifndef INC_I2C2_LOCK_H_
#define INC_I2C2_LOCK_H_

#include <stdbool.h>

void I2C2_Lock_Init(void);
bool I2C2_Lock_Take(void);
void I2C2_Lock_Give(void);

#endif /* INC_I2C2_LOCK_H_ */
