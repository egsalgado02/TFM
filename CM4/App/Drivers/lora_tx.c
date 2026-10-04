/*
 * lora_tx.c
 *
 *  Created on: Jun 19, 2026
 *      Author: eduardogonzalezsalgado
 */

#include "lora_tx.h"
#include "uart_link.h"

/* En la Rev.0 la radio del WL55 no responde, así que las tramas salen por
 * USART1 hacia el simulador de pasarela del PC (tools/gateway_sim.py).
 *
 * El nombre y los parámetros de LoRa_SendPayload no cambian: quien la
 * llama (payload_builder_fsm.c) y los tests no se enteran de que por
 * debajo va por el puerto serie en vez de por la radio. Cuando haya una
 * placa con la radio funcionando, basta con volver a llamar aquí a
 * LmHandlerSend con el fport. */

void LoRa_SendPayload(uint8_t* payload, uint8_t length, uint8_t fport){
    UART_Link_Send(payload, length, fport);
}
