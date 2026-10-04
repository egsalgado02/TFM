/*
 * uart_link.c - enlace serie por USART1 que sustituye a la radio.
 *
 * El formato de trama y el porqué están en uart_link.h.
 *
 *   Envío: HAL_UART_Transmit bloqueante. Lo llama el payload builder desde
 *   su tarea (StartPayloadTask), así que mientras envía no para a ningún
 *   sensor.
 *
 *   Recepción: se lee byte a byte por interrupción. Cada byte que llega
 *   se mete en un buffer circular y se vuelve a pedir el siguiente. Luego
 *   UART_Link_DrainRx, desde la tarea del payload builder, saca los bytes
 *   del buffer y los pasa por una pequeña máquina de estados que monta la
 *   trama.
 *
 *   Con la trama completa se llama a ConfigModule_HandleLoRaFrame, la
 *   misma función que se llamaba al recibir por radio. Para config_module
 *   da igual de dónde venga la trama.
 */

#include "uart_link.h"
#include "config_module.h"      /* ConfigModule_HandleLoRaFrame */
#include "usart.h"              /* huart1 (declarada extern por CubeMX) */
#include <string.h>

/* ============================================================
 * Formato de trama
 * ============================================================ */

#define UART_LINK_SOF          0x7Eu
#define UART_LINK_MAX_PAYLOAD  255u

/* Buffer circular de recepción. El tamaño es potencia de 2 para dar la
 * vuelta con una máscara en vez de con un módulo. 256 bytes dan para
 * unas cinco tramas entre dos llamadas a DrainRx. */
#define RX_BUF_SIZE            256u
#define RX_BUF_MASK            (RX_BUF_SIZE - 1u)

/* ============================================================
 * Estado interno
 * ============================================================ */

static volatile uint8_t  s_rx_buf[RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0;  /* lo avanza la interrupción */
static volatile uint16_t s_rx_tail = 0;  /* lo avanza DrainRx */

/* Aquí deja la HAL el byte recibido antes de llamar al callback */
static uint8_t s_rx_byte;

/* Estados del montaje de la trama */
typedef enum {
    DFR_WAIT_SOF,      /* buscando el byte de inicio */
    DFR_WAIT_FPORT,    /* llegó el inicio, falta el FPORT */
    DFR_WAIT_LEN,      /* llegó el FPORT, falta la longitud */
    DFR_WAIT_PAYLOAD   /* llegó la longitud, juntando los datos */
} deframer_state_t;

static deframer_state_t s_dfr_state = DFR_WAIT_SOF;
static uint8_t s_dfr_fport;
static uint8_t s_dfr_len;
static uint8_t s_dfr_payload[UART_LINK_MAX_PAYLOAD];
static uint8_t s_dfr_payload_idx;

/* ============================================================
 * Callback de la HAL: ha llegado un byte.
 * Este callback es uno solo para todas las UART del micro, por eso lo
 * primero es mirar que venga de USART1.
 * ============================================================ */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) {
        return;
    }

    /* Al buffer. Si está lleno, el byte se pierde; en una interrupción
     * no se puede esperar. */
    uint16_t next = (uint16_t)((s_rx_head + 1u) & RX_BUF_MASK);
    if (next != s_rx_tail) {
        s_rx_buf[s_rx_head] = s_rx_byte;
        s_rx_head = next;
    }

    /* Pido el siguiente byte */
    (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
}

/* ============================================================
 * Callback de error de la UART.
 * Cuando hay un error de overrun, de trama o de ruido, la HAL cancela la
 * recepción por interrupción. Si no se vuelve a pedir aquí, la recepción
 * se queda parada para siempre y dejan de llegar comandos.
 * ============================================================ */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) {
        return;
    }
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
}

/* ============================================================
 * Funciones públicas
 * ============================================================ */

void UART_Link_Init(void)
{
    s_rx_head = 0;
    s_rx_tail = 0;
    s_dfr_state = DFR_WAIT_SOF;

    /* La UART ya la ha inicializado MX_USART1_UART_Init en main().
     * Aquí solo se pide el primer byte. */
    (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
}

void UART_Link_Send(uint8_t* payload, uint8_t length, uint8_t fport)
{
    /* La trama se monta en la pila: inicio + FPORT + longitud + hasta
     * 255 bytes, 258 en total. La tarea del payload builder tiene
     * pila de sobra para esto. */
    uint8_t frame[3u + UART_LINK_MAX_PAYLOAD];

    frame[0] = UART_LINK_SOF;
    frame[1] = fport;
    frame[2] = length;
    if (length > 0u && payload != NULL) {
        memcpy(&frame[3], payload, length);
    }

    /* A 115200 baudios cada byte son unos 87 us: una trama de 30 bytes
     * tarda unos 2,6 ms. Con 100 ms de timeout sobra hasta para 255. */
    (void)HAL_UART_Transmit(&huart1, frame,
                            (uint16_t)(3u + (uint16_t)length), 100u);
}

void UART_Link_DrainRx(void)
{
    /* Saco todos los bytes que haya y los paso por la máquina de estados */
    while (s_rx_tail != s_rx_head) {
        uint8_t b = s_rx_buf[s_rx_tail];
        s_rx_tail = (uint16_t)((s_rx_tail + 1u) & RX_BUF_MASK);

        switch (s_dfr_state) {
            case DFR_WAIT_SOF:
                if (b == UART_LINK_SOF) {
                    s_dfr_state = DFR_WAIT_FPORT;
                }
                /* Lo que llegue fuera de una trama se tira. Así, si se
                 * pierde un byte o entra ruido, se vuelve a sincronizar
                 * sola en el siguiente byte de inicio. */
                break;

            case DFR_WAIT_FPORT:
                s_dfr_fport = b;
                s_dfr_state = DFR_WAIT_LEN;
                break;

            case DFR_WAIT_LEN:
                s_dfr_len = b;
                s_dfr_payload_idx = 0;
                if (s_dfr_len == 0u) {
                    /* Trama vacía: no se pasa y vuelvo a buscar el inicio */
                    s_dfr_state = DFR_WAIT_SOF;
                } else {
                    s_dfr_state = DFR_WAIT_PAYLOAD;
                }
                break;

            case DFR_WAIT_PAYLOAD:
                s_dfr_payload[s_dfr_payload_idx++] = b;
                if (s_dfr_payload_idx >= s_dfr_len) {
                    /* Trama completa, se la paso a config_module. El FPORT
                     * no se usa, igual que al recibir por radio. */
                    ConfigModule_HandleLoRaFrame(s_dfr_payload, s_dfr_len);
                    s_dfr_state = DFR_WAIT_SOF;
                }
                break;

            default:
                /* No debería pasar; por si acaso, vuelvo al principio */
                s_dfr_state = DFR_WAIT_SOF;
                break;
        }
    }
}
