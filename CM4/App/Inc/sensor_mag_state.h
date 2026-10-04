#ifndef INC_SENSOR_MAG_STATE_H_
#define INC_SENSOR_MAG_STATE_H_

#include <stdint.h>
#include <stdbool.h>

// Estado por magnitud para deadband + keepalive. Lo usan todos los sensores
// que tienen varias "cosas medibles" (BME tiene 3: T/H/P; acelerómetro 3:
// X/Y/Z; INA solo 1: corriente).
//
// La idea:
//   - solo mando un valor nuevo si CAMBIA bastante (deadband) o si
//     ha pasado mucho tiempo desde el último envío (keepalive).
//   - así no satureo el aire por LoRa con valores casi iguales.

typedef struct {
    int16_t  last_sent;          // el último valor que llegó a transmitirse
    uint32_t tick_last_sent_ms;  // momento en que se envió
    bool     ever_sent;          // ¿se ha enviado alguna vez? (primera vez = sí)
} sensor_mag_state_t;

// Devuelve true si toca mandar el valor nuevo.
//   - Si NO se ha enviado nunca: sí, primera vez siempre se manda.
//   - Si el delta absoluto supera el deadband: sí.
//   - Si han pasado más de keepalive_ms desde el último envío: sí.
//   - En otro caso: no, ahorramos aire.
//
// Lo dejo inline porque es muy chiquitín y se llama mucho.
static inline bool sensor_should_send(const sensor_mag_state_t* mag,
                                      int16_t  new_val,
                                      uint8_t  dead_band,
                                      uint32_t keepalive_ms,
                                      uint32_t now_ms)
{
    if (!mag->ever_sent) return true;

    // delta absoluto con int16_t para no truncar
    int16_t delta = new_val - mag->last_sent;
    if (delta < 0) delta = -delta;
    if (delta > (int16_t)dead_band) return true;

    // keepalive: hace mucho que no mando nada
    return (now_ms - mag->tick_last_sent_ms) >= keepalive_ms;
}

#endif /* INC_SENSOR_MAG_STATE_H_ */
