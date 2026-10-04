





typedef void* queue_handle_t;













queue_handle_t Queue_Create (uint32_t depth, size_t item_size);

_Bool Queue_Send (queue_handle_t q, const void* item, uint32_t timeout_ms);

_Bool Queue_Receive (queue_handle_t q, void* out_item, uint32_t timeout_ms);

uint32_t Queue_ItemsWaiting(queue_handle_t q);
