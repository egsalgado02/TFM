#ifndef INC_PAYLOAD_BUILDER_FSM_H_
#define INC_PAYLOAD_BUILDER_FSM_H_

#include <stdint.h>
#include <stdbool.h>
#include "fsm.h"
#include "power_fsm.h"
#include "payload_queue.h"
#include "rtos_iface.h"
#include "config_cmd.h"

// Construye la trama LoRa final a partir de los mensajes que los sensores
// encolan en PayloadQueue.
//
// Solo hay UNA forma de empaquetar las medidas de rutina: estadística
// (max + min + media por sensor_id). Se acumula durante la ventana y
// al expirar el timer (o cuando alguien hace ForceSend), se transmite.
//
// Las EMERGENCIAS van por su camino directo: en cuanto entran, se mandan
// como un único valor (sin acumular ni estadísticas). Tiene sentido
// porque una alarma es un evento puntual, no una serie temporal.

#define MAX_PAYLOAD_BUFFER  51    // tamaño máximo de la trama LoRa
#define MAX_SENSOR_IDS      255   // tamaño del array de estadísticas

typedef enum {
    EV_NEW_MESSAGE  = (1 << 0),   // ha entrado un mensaje nuevo desde la cola
    EV_TIMEOUT_LORA = (1 << 1),   // se ha cumplido el tiempo, toca empaquetar y enviar
    EV_FORCE_SEND   = (1 << 2),   // alguien ha pedido envío inmediato
} payload_event_t;

// Estadísticas acumuladas por sensor. Cada vez que llega un mensaje
// de rutina, actualizamos esto. Al enviar, reseteamos.
typedef struct {
    int32_t  sumatorio;   // suma de todos los valores recibidos (para la media)
    int16_t  maximo;      // máximo visto
    int16_t  minimo;      // mínimo visto
    uint16_t contador;    // cuántos valores se han acumulado
} sensor_stats_t;

typedef struct {
    fsm_t          fsm;            // motor genérico (primer campo)

    uint32_t       events;         // bitmask con los EV_* de arriba
    payload_msg_t  rx_msg;         // mensaje recién sacado de PayloadQueue

    // Una entrada por cada sensor_id posible. Si contador==0, ese sensor
    // no ha mandado nada en esta ventana y se ignora al empaquetar.
    sensor_stats_t stats[MAX_SENSOR_IDS];

    // Duración de la ventana de agregación según el modo de energía, una
    // entrada por modo, igual que los timeouts_ms de accel/bme/ina/adc.
    // El envío es el evento que más energía consume del nodo, así que es
    // la cadencia que más pesa en el balance: en CONSERVACION y en
    // EMERGENCIA la ventana se alarga y el nodo transmite menos veces.
    uint32_t       timeouts_ms[NUMERO_ESTADOS];
    power_state_t  modo_energia_actual;

    // Periodo en vigor. Es una copia de timeouts_ms[modo_energia_actual]
    // que mantiene apply_cmd; la tarea lee este campo y no el vector.
    uint32_t       lora_report_timeout_ms;
    bool           hay_datos_pendientes;   // true si en el último envío no
                                           // cabía todo y queda por mandar

    queue_handle_t cmd_queue;
} payload_fsm_t;

// def_timeouts: vector de NUMERO_ESTADOS entradas, en el orden del enum
// power_state_t (abundancia, conservación, emergencia).
void PayloadFSM_Init          (payload_fsm_t* p, const uint32_t* def_timeouts,
                                uint32_t cmd_queue_depth);

// Drena la cola de comandos de configuración del builder. La tarea
// (StartPayloadTask) lee la PayloadQueue por su cuenta con Queue_Receive
// para poder bloquearse a la vez por mensaje y por timeout.
void PayloadFSM_DrainCmdQueue (payload_fsm_t* p);

// Eventos que disparan envío:
void PayloadFSM_TriggerTimeout(payload_fsm_t* p);   // expiró el timer LoRa
void PayloadFSM_ForceSend     (payload_fsm_t* p);   // alguien quiere mandar ya

// Permite que un agente externo (p. ej. el config_module) meta comandos
// directos en la cola del builder.
bool PayloadFSM_PostCmd       (void* ctx, const config_cmd_t* cmd);

#endif /* INC_PAYLOAD_BUILDER_FSM_H_ */
