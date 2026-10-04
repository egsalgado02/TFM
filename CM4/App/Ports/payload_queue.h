#ifndef INC_PAYLOAD_QUEUE_H_
#define INC_PAYLOAD_QUEUE_H_

// Cola global de payload: aquí van los mensajes que los sensores
// quieren mandar por LoRa. El payload_builder_fsm los consume desde
// el otro lado, los empaqueta y dispara LoRa_SendPayload.
//
// OJO: payload_msg_t y payload_event_origin_t se DEFINEN aquí, no en
// payload_builder_fsm.h. Es así porque payload_builder_fsm.h ya incluye
// este fichero, y si lo hiciera al revés sería una dependencia circular.

#include <stdbool.h>
#include <stdint.h>
#include "emergency_codes.h"

// "Por qué" llega el mensaje: rutina (normal) o emergencia (urgente).
// El builder usa este campo para decidir si empaqueta o transmite YA.
typedef enum {
    MOTIVO_ROUTINE = 0,
    MOTIVO_EMERGENCY
} payload_event_origin_t;

typedef struct {
    uint8_t                sensor_id;     // de qué sensor viene
    payload_event_origin_t motivo;        // rutina o emergencia
    int16_t                valor;         // dato significativo en rutina;
                                          // en emergencia solo si emerg_codigo
                                          // tiene activo EMERG_VALOR_FLAG
    emerg_code_t           emerg_codigo;  // EMERG_NONE en rutina;
                                          // EMERG_* concreto en emergencia
} payload_msg_t;

bool PayloadQueue_Enqueue(const payload_msg_t* msg);
bool PayloadQueue_Dequeue(payload_msg_t* msg);

#endif /* INC_PAYLOAD_QUEUE_H_ */
