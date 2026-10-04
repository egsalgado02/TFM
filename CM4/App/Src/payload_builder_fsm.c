#include "payload_builder_fsm.h"
#include "lora_tx.h"
#include "payload_queue.h"
#include <string.h>

// La FSM solo tiene un estado real: IDLE. Las transiciones son
// IDLE -> IDLE con acciones distintas según qué evento haya entrado.
#define IDLE 0

// ==================================================================
//  Guardas
// ==================================================================

static int check_es_emergencia(fsm_t* m) {
    payload_fsm_t* p = (payload_fsm_t*)m;
    // Hay mensaje nuevo y es una emergencia: camino rápido
    return (p->events & EV_NEW_MESSAGE) && (p->rx_msg.motivo == MOTIVO_EMERGENCY);
}

static int check_es_rutina(fsm_t* m) {
    payload_fsm_t* p = (payload_fsm_t*)m;
    // Hay mensaje nuevo y es rutina: solo se acumula
    return (p->events & EV_NEW_MESSAGE) && (p->rx_msg.motivo == MOTIVO_ROUTINE);
}

static int check_force_o_timeout(fsm_t* m) {
    payload_fsm_t* p = (payload_fsm_t*)m;
    // Toca empaquetar y enviar (por timeout periódico o por orden explícita)
    return (p->events & (EV_TIMEOUT_LORA | EV_FORCE_SEND)) != 0;
}

// ==================================================================
//  Acciones
// ==================================================================

// Acumula el mensaje en las estadísticas de su sensor_id.
// NO envía nada todavía; eso lo hará accion_empaquetar_y_enviar
// cuando expire el timer de LoRa o llegue un ForceSend.
static void accion_acumular(fsm_t* m) {
    payload_fsm_t* p = (payload_fsm_t*)m;
    p->events &= ~EV_NEW_MESSAGE;

    uint8_t id  = p->rx_msg.sensor_id;
    int16_t val = p->rx_msg.valor;

    if (id < MAX_SENSOR_IDS) {
        sensor_stats_t* s = &p->stats[id];
        if (s->contador == 0) {
            // primera vez para este sensor en esta ventana
            s->maximo = val;
            s->minimo = val;
        } else {
            if (val > s->maximo) s->maximo = val;
            if (val < s->minimo) s->minimo = val;
        }
        s->sumatorio += (int32_t)val;
        s->contador++;
    }
}

// Empaqueta TODAS las estadísticas acumuladas (un grupo de
// max+min+media por cada sensor que tenga contador>0) y manda la trama.
//
// Formato por sensor:
//   id <= 15 (cabe en 4 bits): 5 bytes empaquetados
//     [id|max_hi][max_lo][min_hi][min_lo|mean_hi][mean_lo]
//   id  > 15: 7 bytes en plano
//     [id][max_hi][max_lo][min_hi][min_lo][mean_hi][mean_lo]
//
// Si no cabe todo ponemos hay_datos_pendientes=true y mandamos lo que
// haya entrado. El resto irá en el siguiente ciclo.
static void accion_empaquetar_y_enviar(fsm_t* m) {
    payload_fsm_t* p = (payload_fsm_t*)m;
    p->events &= ~(EV_TIMEOUT_LORA | EV_FORCE_SEND);

    uint8_t lora_frame[MAX_PAYLOAD_BUFFER];
    uint8_t index             = 0;
    bool    quedan_pendientes = false;

    for (int id = 0; id < MAX_SENSOR_IDS; id++) {
        // Saltar sensores que no han mandado nada en esta ventana
        if (p->stats[id].contador == 0) continue;

        // ¿Cuántos bytes necesita este sensor en la trama?
        uint8_t bytes_needed = (id <= 15) ? 5u : 7u;

        // ¿Cabe? Si no, marcamos pendientes y cortamos.
        if ((index + bytes_needed) > MAX_PAYLOAD_BUFFER) {
            quedan_pendientes = true;
            break;
        }

        // Casteo a uint16 para hacer los shifts sin sorpresas de signo
        uint16_t max  = (uint16_t)p->stats[id].maximo;
        uint16_t min  = (uint16_t)p->stats[id].minimo;
        uint16_t mean = (uint16_t)((int16_t)(p->stats[id].sumatorio /
                                              p->stats[id].contador));

        if (id <= 15) {
            // Empaquetamos id+max+min+mean en 5 bytes:
            // todos los valores se truncan a 12 bits (0x0FFF)
            max &= 0x0FFF; min &= 0x0FFF; mean &= 0x0FFF;
            lora_frame[index++] = (uint8_t)(((id & 0x0F) << 4) | (max >> 8));
            lora_frame[index++] = (uint8_t)(max & 0xFF);
            lora_frame[index++] = (uint8_t)(min >> 4);
            lora_frame[index++] = (uint8_t)(((min & 0x0F) << 4) | (mean >> 8));
            lora_frame[index++] = (uint8_t)(mean & 0xFF);
        } else {
            // id ocupa 1 byte completo; mandamos max/min/mean a 16 bits
            lora_frame[index++] = (uint8_t)id;
            lora_frame[index++] = (uint8_t)(max >> 8);
            lora_frame[index++] = (uint8_t)(max & 0xFF);
            lora_frame[index++] = (uint8_t)(min >> 8);
            lora_frame[index++] = (uint8_t)(min & 0xFF);
            lora_frame[index++] = (uint8_t)(mean >> 8);
            lora_frame[index++] = (uint8_t)(mean & 0xFF);
        }

        // Reseteamos las estadísticas de este sensor: empezamos
        // una ventana nueva.
        p->stats[id].contador  = 0;
        p->stats[id].sumatorio = 0;
        p->stats[id].maximo    = 0;
        p->stats[id].minimo    = 0;
    }

    // Solo enviamos si conseguimos meter algo en la trama.
    // FPORT_RUTINA indica al receptor que esto es estadística (sin byte
    // de tipo en el payload; el FPort hace la distinción).
    if (index > 0) {
        LoRa_SendPayload(lora_frame, index, FPORT_RUTINA);
    }
    p->hay_datos_pendientes = quedan_pendientes;
}

// Emergencia: NO esperamos a empaquetar nada, transmitimos YA por su
// FPort dedicado (FPORT_EMERGENCIA), que el receptor del gateway usa
// para distinguir de los frames de rutina sin ambigüedad.
//
// Formato de la trama:
//   2 bytes:  [sensor_id][emerg_codigo]
//             cuando emerg_codigo NO tiene activo EMERG_VALOR_FLAG.
//             Ejemplo: caída del acelerómetro [10][0x02].
//
//   4 bytes:  [sensor_id][emerg_codigo][valor_hi][valor_lo]
//             cuando emerg_codigo tiene activo EMERG_VALOR_FLAG (bit 7).
//             Ejemplo (futuro): sobrecorriente del INA [20][0x81][va_hi][va_lo].
static void accion_envio_emergencia(fsm_t* m) {
    payload_fsm_t* p = (payload_fsm_t*)m;
    p->events &= ~EV_NEW_MESSAGE;

    uint8_t emergency_frame[4];
    emergency_frame[0] = p->rx_msg.sensor_id;
    emergency_frame[1] = (uint8_t)p->rx_msg.emerg_codigo;
    uint8_t len = 2;

    // Si el código tiene activo el bit "lleva valor", añadimos los 2 bytes.
    if (p->rx_msg.emerg_codigo & EMERG_VALOR_FLAG) {
        uint16_t val = (uint16_t)p->rx_msg.valor;
        emergency_frame[2] = (uint8_t)(val >> 8);
        emergency_frame[3] = (uint8_t)(val & 0xFF);
        len = 4;
    }

    LoRa_SendPayload(emergency_frame, len, FPORT_EMERGENCIA);
}

// ==================================================================
//  Tabla de transiciones
//
//  El orden importa: si entran rutina y timeout a la vez, primero
//  acumulamos (rutina) y en el siguiente fire empaquetamos.
// ==================================================================

static fsm_trans_t tabla_payload[] = {
    {IDLE, check_es_emergencia,   IDLE, accion_envio_emergencia},
    {IDLE, check_es_rutina,       IDLE, accion_acumular},
    {IDLE, check_force_o_timeout, IDLE, accion_empaquetar_y_enviar},
    {-1, NULL, -1, NULL}
};

// ==================================================================
//  Comandos
// ==================================================================

static void apply_cmd(payload_fsm_t* p, const config_cmd_t* cmd) {
    switch (cmd->type) {
        case CMD_SET_TIMEOUT:
            // Periodo de la ventana de envio, POR MODO de energia, igual
            // que en accel/bme/ina/adc. Si el modo tocado es el activo,
            // el periodo en vigor cambia en el acto.
            if (cmd->mode < NUMERO_ESTADOS) {
                p->timeouts_ms[cmd->mode] = cmd->value;
                if (cmd->mode == p->modo_energia_actual) {
                    p->lora_report_timeout_ms = cmd->value;
                }
            }
            break;

        case CMD_SET_MODE:
            // Difusion desde power_fsm via Config_ApplySystemMode. El envio
            // es el evento que mas energia consume del nodo, asi que la
            // ventana tiene que escalarse con el modo igual que la cadencia
            // de los sensores. La tarea recalcula su plazo en la siguiente
            // vuelta: al cambiar el periodo, el plazo vigente vence de
            // inmediato y se rearma ya con el nuevo, de modo que el cambio
            // de modo produce un envio y a partir de ahi la cadencia nueva.
            if (cmd->value < NUMERO_ESTADOS) {
                p->modo_energia_actual    = (power_state_t)cmd->value;
                p->lora_report_timeout_ms = p->timeouts_ms[p->modo_energia_actual];
            }
            break;

        case CMD_FORCE_READ:
            /* Un force_read dirigido al payload builder significa "envía
             * ya lo que tengas". Es lo que manda el simulador (flush) dos
             * segundos después de un force_read a un sensor: el sensor mide y
             * su medida se acumula, pero en conservación la ventana dura
             * 30 min y no se vería. Como la tarea vacía los comandos antes
             * del fire, el envío sale en esa misma vuelta. */
            PayloadFSM_ForceSend(p);
            break;

        default:
            break;
    }
}

// ==================================================================
//  API pública
// ==================================================================

void PayloadFSM_Init(payload_fsm_t* p, const uint32_t* def_timeouts,
                     uint32_t cmd_queue_depth) {
    fsm_init((fsm_t*)p, tabla_payload);
    p->fsm.current_state    = IDLE;
    p->events               = 0;
    p->hay_datos_pendientes = false;

    // Mismo arranque que los FSM de sensor: CONSERVACION. El modo real
    // llega enseguida por la difusion de Config_ApplySystemMode.
    p->modo_energia_actual = POWER_STATE_CONSERVACION;
    for (int i = 0; i < NUMERO_ESTADOS; i++) {
        p->timeouts_ms[i] = def_timeouts[i];
    }
    p->lora_report_timeout_ms = p->timeouts_ms[p->modo_energia_actual];

    memset(p->stats, 0, sizeof(p->stats));
    p->cmd_queue = Queue_Create(cmd_queue_depth, sizeof(config_cmd_t));
}

void PayloadFSM_DrainCmdQueue(payload_fsm_t* p) {
    // Solo procesa la cola de COMANDOS de configuración (CMD_SET_TIMEOUT,
    // CMD_SET_ENABLE, etc.). La PayloadQueue de mensajes de sensores la
    // lee directamente la tarea con Queue_Receive (bloqueo combinado
    // mensaje + timeout).
    config_cmd_t cmd;
    while (Queue_Receive(p->cmd_queue, &cmd, QUEUE_NO_WAIT)) {
        apply_cmd(p, &cmd);
    }
}

void PayloadFSM_TriggerTimeout(payload_fsm_t* p) {
    p->events |= EV_TIMEOUT_LORA;
}

void PayloadFSM_ForceSend(payload_fsm_t* p) {
    p->events |= EV_FORCE_SEND;
}

bool PayloadFSM_PostCmd(void* ctx, const config_cmd_t* cmd) {
    payload_fsm_t* p = (payload_fsm_t*)ctx;
    return Queue_Send(p->cmd_queue, cmd, QUEUE_NO_WAIT);
}
