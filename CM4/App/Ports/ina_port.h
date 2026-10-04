#ifndef INC_INA_PORT_H_
#define INC_INA_PORT_H_

#include <stdint.h>
#include <stdbool.h>

// Puerto del INA (sensor de corriente).
// Como puede haber VARIAS instancias físicas (una interna, otras en
// la expansión), TODAS las funciones llevan un hw_id para distinguir
// a cuál hablamos. La única excepción es GetTickMs, que es global.

void     InaPort_StartBootTimer      (uint8_t hw_id);
bool     InaPort_CheckStatusI2C      (uint8_t hw_id);
void     InaPort_TriggerForcedReading(uint8_t hw_id);
void     InaPort_StartMedidaTimer    (uint8_t hw_id);
bool     InaPort_IsDataReady         (uint8_t hw_id);
bool     InaPort_ReadData            (uint8_t hw_id, int16_t* corriente, int16_t* voltaje);  // mA, mV
void     InaPort_StartSleepTimer     (uint8_t hw_id, uint32_t time_ms);
uint32_t InaPort_GetTickMs           (void);    // tick del sistema (global)

#endif /* INC_INA_PORT_H_ */
