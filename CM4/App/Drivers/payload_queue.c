// Cola de salida: de las FSM de sensor al payload builder.
// Cada FSM de sensor llama a PayloadQueue_Enqueue cuando tiene una medida.
// La cola la crea CubeMX en app_freertos.c (payloadQHandle, 32 mensajes).
// Enqueue no espera: si la cola está llena, el mensaje se pierde.

#include "payload_queue.h"
#include "cmsis_os.h"

extern osMessageQueueId_t payloadQHandle;

bool PayloadQueue_Enqueue(const payload_msg_t* msg) {
    if (payloadQHandle == NULL) return false;
    return osMessageQueuePut(payloadQHandle, msg, 0U, 0U) == osOK;
}

bool PayloadQueue_Dequeue(payload_msg_t* msg) {
    if (payloadQHandle == NULL) return false;
    return osMessageQueueGet(payloadQHandle, msg, NULL, 0U) == osOK;
}
