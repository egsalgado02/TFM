#ifndef BME_FSM_H
#define BME_FSM_H

#include <stdint.h>
#include <stdbool.h>
#include "fsm.h"
#include "power_fsm.h"
#include "rtos_iface.h"
#include "config_cmd.h"
#include "sensor_mag_state.h"

// IDs de payload (lo que mete cada magnitud en la cola de salida)
#define ID_BME_TEMP  4
#define ID_BME_HUM   5
/* La presión va en décimas de hPa (unas 10130) y no cabe en 12 bits. Con
 * un id mayor que 15 el payload builder la manda en el formato de 16 bits. */
#define ID_BME_PRES  22

#define BME_MAGNITUDES     3
#define BME_MAX_BOOT_TRIES 3
#define BME_MAX_READ_TRIES 3
/* Envío de mantenimiento: si una magnitud lleva este tiempo sin publicarse,
 * se publica aunque no haya cambiado. 20 s para las pruebas en banco; en
 * despliegue serían 24 h, (24UL*60UL*60UL*1000UL). */
#define BME_KEEPALIVE_MS   (20UL * 1000UL)                     // 20 s

// Bits del campo events.
// Los _OK / _FAIL los pone el Notify cuando le toca, mirando el HW UNA sola
// vez. Las guardas solo testean estos flags, sin tocar el I2C.
typedef enum {
    BME_EV_MEDIR     = (1 << 0),   // el timer de sueño expiró: toca medir
    BME_EV_BOOT_OK   = (1 << 1),   // el sensor ha arrancado y contesta al I2C
    BME_EV_BOOT_FAIL = (1 << 2),   // no contesta tras el delay de boot
    BME_EV_DATA_OK   = (1 << 3),   // ya hay medida lista para leer
    BME_EV_DATA_FAIL = (1 << 4),   // se acabó el tiempo de espera sin medida
} bme_event_t;

typedef struct {
    fsm_t  fsm;

    uint32_t events;
    bool     activado;
    uint8_t  intentos_arranque;
    uint8_t  intentos_lectura;

    power_state_t      modo_energia_actual;
    uint32_t           timeouts_ms[NUMERO_ESTADOS];
    uint8_t            deadband_threshold[NUMERO_ESTADOS];
    uint32_t           t_keepalive_ms;
    sensor_mag_state_t mag[BME_MAGNITUDES];

    queue_handle_t cmd_queue;
} bme_fsm_t;

void BmeFSM_Init(bme_fsm_t*      p,
                 const uint32_t* def_timeouts,
                 const uint8_t*  def_deadband,
                 uint32_t        keepalive_ms,
                 uint32_t        cmd_queue_depth);

void BmeFSM_DrainCmdQueue  (bme_fsm_t* p);
void BmeFSM_ApplyCmd       (bme_fsm_t* p, const config_cmd_t* cmd);

void BmeFSM_TriggerMedida    (bme_fsm_t* p);

// Estas dos funciones ya no se limitan a poner un flag de "el timer expiró":
// también consultan el HW (CheckStatusI2C / IsDataReady) y ponen BOOT_OK/FAIL
// o DATA_OK/FAIL. Así las guardas se quedan limpias.
void BmeFSM_NotifyBootTimer  (bme_fsm_t* p);
void BmeFSM_NotifyMedidaTimer(bme_fsm_t* p);

#endif /* BME_FSM_H */
