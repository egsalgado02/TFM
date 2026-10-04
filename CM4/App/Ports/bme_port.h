#ifndef INC_BME_PORT_H_
#define INC_BME_PORT_H_

#include <stdint.h>
#include <stdbool.h>

// Puerto del BME680 (temperatura, humedad y presión).
// El BME necesita pasar por una secuencia: encender GPIO de
// alimentación -> esperar boot -> mandar "forced reading" -> esperar
// el tiempo de medida -> leer datos. Por eso hay tantos métodos.

void     BmePort_SetPowerGPIO        (bool estado);   // enciende/apaga GPIO de alimentación
void     BmePort_StartBootTimer      (void);          // arma timer para el delay de boot
bool     BmePort_CheckStatusI2C      (void);          // ¿el sensor responde por I2C?
void     BmePort_TriggerForcedReading(void);          // dispara una conversión
void     BmePort_StartMedidaTimer    (void);          // arma timer para el delay de medida
bool     BmePort_IsDataReady         (void);          // ¿ya hay datos para leer?
bool     BmePort_ReadData            (int16_t* temperatura, int16_t* humedad, int16_t* presion);
void     BmePort_StartSleepTimer     (uint32_t timeout_ms);   // próximo despertar
uint32_t BmePort_GetTickMs           (void);

#endif /* INC_BME_PORT_H_ */
