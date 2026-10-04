#ifndef INC_POWER_QUEUE_H_
#define INC_POWER_QUEUE_H_

#include <stdint.h>

// Canal del adc_fsm hacia el power_fsm para mandarle el voltaje medido.
// Es un puerto fino con una sola función: por dentro, la implementación
// real mete el voltaje en una cola que el power_fsm consume.

void ColaPower_EnviarVoltaje(uint16_t voltaje_mv);

#endif /* INC_POWER_QUEUE_H_ */
