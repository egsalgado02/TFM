#ifndef INC_EMERGENCY_CODES_H_
#define INC_EMERGENCY_CODES_H_

#include <stdint.h>

// Códigos de emergencia que viajan en el byte de "tipo" dentro de la
// trama LoRa cuando se envía una emergencia (FPort = FPORT_EMERGENCIA).
//
// CONVENCIÓN del bit 7 (EMERG_VALOR_FLAG):
//   - 0: la trama es solo [sensor_id][código] (2 bytes), sin valor.
//   - 1: la trama es [sensor_id][código][valor_hi][valor_lo] (4 bytes).
//
// Esto permite que cualquier sensor mande una emergencia "simple" (un
// evento sin datos: caída, robo) o una emergencia "con dato" (p. ej. el
// INA que indica el valor de corriente que disparó la alarma).
//
// Rango 0x00–0x7F: códigos sin valor (eventos puros).
// Rango 0x80–0xFF: códigos con valor (2 bytes de int16 detrás).
//
// El receptor del gateway lee el código, comprueba el bit 7 y sabe
// exactamente cuántos bytes consumir.

typedef enum {
    EMERG_NONE        = 0x00,    // sin emergencia (valor neutro al inicializar)

    // -----------------------------------------------------------------
    //  Emergencias SIN valor (rango 0x01–0x7F)
    // -----------------------------------------------------------------
    EMERG_ROBO        = 0x01,    // acelerómetro: movimiento sospechoso
    EMERG_CAIDA       = 0x02,    // acelerómetro: caída libre detectada
    // 0x03 ... 0x7F reservados para futuros eventos sin valor

    // -----------------------------------------------------------------
    //  Máscara del flag "lleva valor" (bit 7)
    // -----------------------------------------------------------------
    EMERG_VALOR_FLAG  = 0x80,

    // -----------------------------------------------------------------
    //  Ejemplos de emergencias CON valor (rango 0x80–0xFF)
    //  Descomentar / añadir cuando algún sensor las necesite.
    // -----------------------------------------------------------------
    // EMERG_INA_SOBRECARGA   = 0x81,   // INA: mA que dispararon la alarma
    // EMERG_BME_TEMP_ALTA    = 0x82,   // BME: grados * 10 que provocaron la alarma
    // EMERG_BME_TEMP_BAJA    = 0x83,

} emerg_code_t;

#endif /* INC_EMERGENCY_CODES_H_ */
