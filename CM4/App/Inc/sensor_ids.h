#ifndef INC_SENSOR_IDS_H_
#define INC_SENSOR_IDS_H_

// Catálogo único de IDs del proyecto.
// Hay DOS espacios distintos que aquí están claramente separados:
//
//   - IDs de PAYLOAD: identifican magnitudes EN LOS MENSAJES que el
//     dispositivo manda al gateway. Los lleva payload_msg_t.sensor_id.
//
//   - IDs de ROUTING: identifican sensores COMO DESTINO de un comando
//     recibido por LoRa. Los lleva config_cmd_t.sensor_id.
//
// Para evitar confusión, los IDs de routing arrancan en 100. Cualquier
// número >= 100 en la tabla de routing significa "es un destino", no
// "es una magnitud".

// ============================================================
//  IDs de PAYLOAD (datos que SALEN del dispositivo)
//  Rango reservado: 0 .. 99
// ============================================================

// Acelerómetro: una entrada por eje. Las alarmas usan ID_ACCEL_X como
// sensor_id; el FPort distingue rutina de emergencia, y emerg_codigo
// identifica la causa. No existe ID_ACCEL_ALARMA.
// Los define accel_fsm.h:
//   ID_ACCEL_X = 0
//   ID_ACCEL_Y = 1
//   ID_ACCEL_Z = 2
//
// BME (también en bme_fsm.h):
//   ID_BME_TEMP = 4
//   ID_BME_HUM  = 5
//   ID_BME_PRES = 22 (ver abajo)
//
// INA: los payload_ids se pasan por argumento al Init.
#define ID_INA_CORRIENTE   20   // corriente en mA
#define ID_INA_VOLTAJE     21   // tensión de bus en mV

// ID_BME_PRES = 22 (definido en bme_fsm.h). Está por encima de 15 porque
// las décimas de hPa no caben en los 12 bits del formato compacto.

// ADC interno: tensión de batería en mV. Debe ser > 15 (formato plano
// de 16 bits): hasta 4500 mV no cabe en los 12 bits del formato packed.
#define ID_ADC_BATERIA     23   // batería en mV

// INA interno: el segundo INA238, en I2C2 con dirección 0x40, alimentado
// por PA8 (RAIL_INTERNO, el mismo que el divisor del ADC). Los IDs 20 y 21
// son los del INA de expansión (I2C1, PA1, RAIL_EXPANSION).
#define ID_INA_INT_CORRIENTE  24   // corriente del nodo en mA
#define ID_INA_INT_VOLTAJE    25   // tensión de bus interna en mV

// ============================================================
//  IDs de ROUTING (destinos de comandos que ENTRAN)
//  Rango reservado: 100 .. 254
//
//  El gateway tiene que conocer estos números para poder mandar
//  comandos a cada sensor. Documentar bien.
// ============================================================

#define ROUTE_ID_ACCEL    100   // comandos al acelerómetro entero
#define ROUTE_ID_BME      101   // comandos al BME entero
#define ROUTE_ID_INA      102   // comandos al INA
#define ROUTE_ID_ADC      103   // comandos al ADC
#define ROUTE_ID_PAYLOAD  104   // comandos al payload builder (p. ej. SET_TIMEOUT del envío LoRa)
#define ROUTE_ID_INA_INT  105   // comandos al INA interno (ROUTE_ID_INA es el de expansión)

#endif /* INC_SENSOR_IDS_H_ */
