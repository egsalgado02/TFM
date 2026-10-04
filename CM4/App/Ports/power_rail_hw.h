#ifndef INC_POWER_RAIL_HW_H_
#define INC_POWER_RAIL_HW_H_

#include "power_rail_manager.h"
#include <stdbool.h>

// Capa más baja de los rails: solo escribe al GPIO del MOSFET.
// La gestión de refcount vive en power_rail_manager; esto solo
// enciende o apaga el rail físico cuando se lo pide.

void PowerRailHW_SetState(power_rail_t rail, bool encendido);

#endif /* INC_POWER_RAIL_HW_H_ */
