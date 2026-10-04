#include "../App/Inc/emergency_codes.h"






typedef enum {

    MOTIVO_ROUTINE = 0,

    MOTIVO_EMERGENCY

} payload_event_origin_t;



typedef struct {

    uint8_t sensor_id;

    payload_event_origin_t motivo;

    int16_t valor;





    emerg_code_t emerg_codigo;



} payload_msg_t;



_Bool PayloadQueue_Enqueue(const payload_msg_t* msg);

_Bool PayloadQueue_Dequeue(payload_msg_t* msg);
