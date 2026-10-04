#ifndef INC_LORA_TX_H_
#define INC_LORA_TX_H_

#include <stdint.h>

// Puerto de salida hacia la radio LoRa. La pila LoRaWAN corre en el M0+ y
// la trama le llegaría por IPCC. En la Rev.0 la radio no funciona, así que
// la implementación (lora_tx.c) la saca por el enlace serie.
//
// El parámetro fport corresponde al "FPort" del protocolo LoRaWAN: un
// número de puerto de aplicación (1..223) que viaja en la cabecera MAC
// y que el gateway entrega al backend junto con el payload. El
// decodificador del backend elige cómo parsear el payload según el
// FPort. NO consume bytes del payload.
//
// Convención del proyecto:
//   FPORT_RUTINA     -> trama estadística (máximo, mínimo y media por magnitud)
//   FPORT_EMERGENCIA -> trama de evento urgente (sensor_id + emerg_code
//                      + opcionalmente 2 bytes de valor)

#define FPORT_RUTINA      1
#define FPORT_EMERGENCIA  2

void LoRa_SendPayload(uint8_t* payload, uint8_t length, uint8_t fport);

#endif /* INC_LORA_TX_H_ */
