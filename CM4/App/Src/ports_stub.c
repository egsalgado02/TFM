// Stubs de TODOS los puertos de hardware.
// Sustituye este fichero por implementaciones reales (en accel_port.c,
// bme_port.c, ina_port.c, adc_port.c, lora_tx.c, payload_queue.c,
// power_queue.c, power_rail_hw.c) cuando integres cada sensor.
//
// Mientras tanto, esto permite que el firmware LINKE y ARRANQUE.
// Las tareas correrán pero los sensores no harán nada real: devuelven
// valores fijos, los timers no hacen nada, etc.

#include <stdint.h>
#include <stdbool.h>

#include "accel_port.h"
#include "bme_port.h"
#include "ina_port.h"
#include "adc_port.h"
// power_rail_hw.h no hace falta aquí: su implementación real está en Drivers/power_rail_hw.c

#include "FreeRTOS.h"
#include "task.h"

// ========== Acelerómetro ==========
void     AccelPort_Init              (void) {}
void     AccelPort_IniciarLecturaI2C (void) {}
bool     AccelPort_DatosListos       (void) { return true; }
bool     AccelPort_LeerBuffer        (int16_t* x, int16_t* y, int16_t* z) {
    *x = 0; *y = 0; *z = 1000;   // valor fijo para no encolar nada útil
    return true;
}
bool     AccelPort_LeerCausasAlarma  (bool* p_robo, bool* p_caida) {
    *p_robo = false; *p_caida = false;
    return true;
}
void     AccelPort_SetThreshold      (uint8_t g) { (void)g; }
void     AccelPort_StartSleepTimer   (uint32_t ms) { (void)ms; }
uint32_t AccelPort_GetTickMs         (void) { return xTaskGetTickCount(); }

// ========== BME680 ==========
void     BmePort_SetPowerGPIO        (bool on) { (void)on; }
void     BmePort_StartBootTimer      (void) {}
bool     BmePort_CheckStatusI2C      (void) { return true; }
void     BmePort_TriggerForcedReading(void) {}
void     BmePort_StartMedidaTimer    (void) {}
bool     BmePort_IsDataReady         (void) { return true; }
bool     BmePort_ReadData            (int16_t* t, int16_t* h, int16_t* pr) {
    *t = 200; *h = 500; *pr = 1013;
    return true;
}
void     BmePort_StartSleepTimer     (uint32_t ms) { (void)ms; }
uint32_t BmePort_GetTickMs           (void) { return xTaskGetTickCount(); }

// ========== INA238 ==========
void     InaPort_StartBootTimer      (uint8_t id) { (void)id; }
bool     InaPort_CheckStatusI2C      (uint8_t id) { (void)id; return true; }
void     InaPort_TriggerForcedReading(uint8_t id) { (void)id; }
void     InaPort_StartMedidaTimer    (uint8_t id) { (void)id; }
bool     InaPort_IsDataReady         (uint8_t id) { (void)id; return true; }
bool     InaPort_ReadData            (uint8_t id, int16_t* c, int16_t* v) {
    (void)id;
    *c = 100;
    *v = 3700;
    return true;
}
void     InaPort_StartSleepTimer     (uint8_t id, uint32_t ms) { (void)id; (void)ms; }
uint32_t InaPort_GetTickMs           (void) { return xTaskGetTickCount(); }

// ========== ADC interno ==========
// Stubs para la config DebugStubs. La implementación REAL está en
// CM4/App/Drivers/adc_port.c y solo se compila en la config Debug.
bool     AdcPort_LeerBateriaBloqueante(uint16_t* p_voltaje_mv) {
    *p_voltaje_mv = 3300;   // valor fijo de bateria "cargada"
    return true;
}
void     AdcPort_StartSleepTimer     (uint32_t ms) { (void)ms; }
uint32_t AdcPort_GetTickMs           (void) { return xTaskGetTickCount(); }


// ========== Cola de payload ==========
// Implementación REAL en CM4/App/Drivers/payload_queue.c

// ========== Cola de power ==========
// Implementación REAL en CM4/App/Drivers/power_queue.c

// ========== Power rail HW ==========
// La implementación REAL vive en CM4/App/Drivers/power_rail_hw.c y se
// compila en las dos configuraciones (Debug y DebugStubs). No tiene versión
// falsa porque son dos pines y no merece la pena simularlos.
