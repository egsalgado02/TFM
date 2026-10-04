#ifndef INC_RTOS_IFACE_H_
#define INC_RTOS_IFACE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Esto es un PUERTO: una capa fina sobre las colas del RTOS para que
// mi código de aplicación NO dependa directamente de CMSIS/FreeRTOS.
// Sirve para dos cosas:
//   1) Tests en host: el mock_rtos_iface.h sustituye las colas reales.
//   2) Portabilidad: si mañana cambio de RTOS, solo toco el adaptador.
//
// El handle es void* a propósito. La implementación real (sobre CMSIS)
// lo casteará a osMessageQueueId_t (que también es un puntero por dentro).

typedef void* queue_handle_t;

// Centinelas de espera, mapeables luego a osWaitForever (0xFFFFFFFF).
#define QUEUE_NO_WAIT   0
#define QUEUE_WAIT_EVER 0xFFFFFFFF

// API minimal: lo justo para enviar/recibir comandos.
queue_handle_t Queue_Create      (uint32_t depth, size_t item_size);
bool           Queue_Send        (queue_handle_t q, const void* item,     uint32_t timeout_ms);
bool           Queue_Receive     (queue_handle_t q, void*       out_item, uint32_t timeout_ms);
uint32_t       Queue_ItemsWaiting(queue_handle_t q);

#endif /* INC_RTOS_IFACE_H_ */
