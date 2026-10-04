#include "rtos_iface.h"

queue_handle_t Queue_Create(uint32_t depth, size_t item_size) {
    (void)depth;
    (void)item_size;
    return (queue_handle_t)1;
}

bool Queue_Send(queue_handle_t q, const void* item, uint32_t timeout_ms) {
    (void)q;
    (void)item;
    (void)timeout_ms;
    return true;
}

bool Queue_Receive(queue_handle_t q, void* out_item, uint32_t timeout_ms) {
    (void)q;
    (void)out_item;
    (void)timeout_ms;
    return false;
}

uint32_t Queue_ItemsWaiting(queue_handle_t q) {
    (void)q;
    return 0;
}
