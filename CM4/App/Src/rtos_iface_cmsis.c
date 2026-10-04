#include "rtos_iface.h"
#include "cmsis_os.h"

// Adaptador real del puerto de RTOS sobre CMSIS-OS V2.
// El stub para tests (rtos_iface_stub.c) sigue existiendo, pero NO
// se compila en el build de producción (en CubeIDE puedes excluirlo
// con clic derecho, Resource Configurations, Exclude from Build).

// Las funciones reciben/devuelven el queue_handle_t opaco (void*) y
// hacen el cast a osMessageQueueId_t por dentro. Es el "encaje" entre
// el dominio (que no sabe de CMSIS) y FreeRTOS (que vive por debajo).

queue_handle_t Queue_Create(uint32_t depth, size_t item_size) {
    return (queue_handle_t) osMessageQueueNew(depth, item_size, NULL);
}

bool Queue_Send(queue_handle_t q, const void* item, uint32_t timeout_ms) {
    uint32_t ticks = (timeout_ms == QUEUE_WAIT_EVER) ? osWaitForever
                                                     : pdMS_TO_TICKS(timeout_ms);
    return osMessageQueuePut((osMessageQueueId_t)q, item, 0U, ticks) == osOK;
}

bool Queue_Receive(queue_handle_t q, void* out_item, uint32_t timeout_ms) {
    uint32_t ticks = (timeout_ms == QUEUE_WAIT_EVER) ? osWaitForever
                                                     : pdMS_TO_TICKS(timeout_ms);
    return osMessageQueueGet((osMessageQueueId_t)q, out_item, NULL, ticks) == osOK;
}

uint32_t Queue_ItemsWaiting(queue_handle_t q) {
    return osMessageQueueGetCount((osMessageQueueId_t)q);
}
