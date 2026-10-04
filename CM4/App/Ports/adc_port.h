#ifndef INC_ADC_PORT_H_
#define INC_ADC_PORT_H_

#include <stdint.h>
#include <stdbool.h>

// Puerto del ADC. La lectura es bloqueante porque la conversión es muy
// corta (unos 11 us por canal a 16 MHz), no merece la pena montar DMA
// para algo así. Además solo se llama de vez en cuando.

bool     AdcPort_LeerBateriaBloqueante(uint16_t* p_voltaje_mv);
void     AdcPort_StartSleepTimer      (uint32_t time_ms);
uint32_t AdcPort_GetTickMs            (void);

#endif /* INC_ADC_PORT_H_ */
