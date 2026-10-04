#ifndef INC_UART_LINK_H_
#define INC_UART_LINK_H_

#include <stdint.h>

/* ============================================================
 * uart_link: enlace binario por USART1 (PA9 TX, PA10 RX, en el
 * conector de expansión).
 *
 * Sustituye a la radio mientras no funcione. Al otro lado hay un
 * adaptador USB-UART y el simulador de pasarela del PC
 * (tools/gateway_sim.py).
 *
 * El resto del firmware no cambia: el payload builder sigue llamando a
 * LoRa_SendPayload(payload, length, fport), y lora_tx.c por dentro llama
 * a UART_Link_Send. En la bajada, cuando llega una trama completa, se
 * llama a ConfigModule_HandleLoRaFrame igual que con la radio.
 *
 * Formato de trama, igual en los dos sentidos:
 *
 *   [0x7E][FPORT: 1 byte][LEN: 1 byte][LEN bytes de datos]
 *
 *   - 0x7E marca el inicio. Si se pierde la sincronización, se recupera
 *     con el siguiente 0x7E.
 *   - FPORT: el mismo que se usaría en LoRa (1 = rutina, 2 = emergencia).
 *     En la bajada no se usa, igual que con la radio.
 *   - LEN: número de bytes de datos, de 0 a 255.
 *   - Datos: exactamente los mismos bytes que irían por LoRa.
 *
 * No lleva CRC. Con un cable USB-UART corto los errores de bit son muy
 * raros, y si se pierde un byte la trama se descarta y se resincroniza.
 * ============================================================ */

/* Arranca el enlace. Se llama una sola vez desde AppTasks_InitAll,
 * después de MX_USART1_UART_Init. Pide el primer byte por interrupción; a
 * partir de ahí cada byte que llega se guarda en el buffer interno hasta
 * que lo saque UART_Link_DrainRx. */
void UART_Link_Init(void);

/* Envía una trama (bloqueante). Tiene los mismos parámetros que
 * LoRa_SendPayload para que lora_tx.c solo tenga que llamarla. A 115200
 * baudios una trama de 30 bytes tarda unos 2,6 ms.
 * No lleva lock porque solo envía una tarea, la del payload builder. */
void UART_Link_Send(uint8_t* payload, uint8_t length, uint8_t fport);

/* Hay que llamarla de vez en cuando desde una tarea (en el proyecto,
 * al final del bucle de StartPayloadTask). Saca los bytes recibidos,
 * monta las tramas y por cada una completa llama a
 * ConfigModule_HandleLoRaFrame. No bloquea. */
void UART_Link_DrainRx(void);

#endif /* INC_UART_LINK_H_ */
