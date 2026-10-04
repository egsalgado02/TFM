#ifndef INC_ACCEL_PORT_H_
#define INC_ACCEL_PORT_H_

#include <stdint.h>
#include <stdbool.h>

// Puerto del acelerómetro: contrato que la FSM usa para hablar con el HW
// real. La implementación concreta vive en App/Drivers/ y traduce estas
// llamadas a I2C, GPIOs, registros del chip, etc.
//
// Tener este puerto separado es lo que me permite testear la FSM en host
// con CMock (mock_accel_port.h).

void     AccelPort_Init              (void);
void     AccelPort_IniciarLecturaI2C (void);  // lee los tres ejes y avisa a la tarea
bool     AccelPort_DatosListos       (void);  // ¿ha terminado la lectura?
bool     AccelPort_LeerBuffer        (int16_t* x, int16_t* y, int16_t* z);
bool     AccelPort_LeerCausasAlarma  (bool* p_es_robo, bool* p_es_caida);
void     AccelPort_SetThreshold      (uint8_t threshold_g);  // umbral de alarma
void     AccelPort_StartSleepTimer   (uint32_t time_ms);     // próximo despertar
uint32_t AccelPort_GetTickMs         (void);                 // tick del sistema

#endif /* INC_ACCEL_PORT_H_ */
