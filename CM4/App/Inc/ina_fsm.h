#ifndef INC_INA_FSM_H_
#define INC_INA_FSM_H_

#include "fsm.h"
#include "power_fsm.h"
#include "power_rail_manager.h"
#include "rtos_iface.h"
#include "config_cmd.h"
#include "sensor_mag_state.h"
#include <stdint.h>
#include <stdbool.h>

// Mismos flags que el BME: el Notify hace UNA lectura al HW y deja
// OK/FAIL como evento. Las guardas solo miran flags.
#define INA_EV_MEDIR      (1 << 0)   // toca medir (timer de sueño expiró)
#define INA_EV_BOOT_OK    (1 << 1)   // sensor responde al I2C tras boot
#define INA_EV_BOOT_FAIL  (1 << 2)   // no responde
#define INA_EV_DATA_OK    (1 << 3)   // medida lista
#define INA_EV_DATA_FAIL  (1 << 4)   // se acabó el tiempo y no hay medida

#define INA_MAX_BOOT_TRIES  3
#define INA_MAX_READ_TRIES  3
/* Envío de mantenimiento: si una magnitud lleva este tiempo sin publicarse,
 * se publica aunque no haya cambiado. 20 s para las pruebas en banco; en
 * despliegue serían 24 h, (24UL*60UL*60UL*1000UL). */
#define INA_KEEPALIVE_MS    (20UL * 1000UL)                    // 20 s

#define INA_MAGNITUDES      2
#define INA_MAG_CORRIENTE   0
#define INA_MAG_VOLTAJE     1

typedef struct {
    fsm_t fsm;

    uint8_t      hw_id;                // qué INA físico (si hay varios)
    uint8_t      payload_id_corriente; // ID en la cola de payload para corriente (mA)
    uint8_t      payload_id_voltaje;   // ID en la cola de payload para tensión de bus (mV)
    power_rail_t rail;                 // de qué rail tira

    uint32_t      timeouts_ms[NUMERO_ESTADOS];
    uint8_t       deadband_ma[NUMERO_ESTADOS];   // deadband de corriente en mA
    uint8_t       deadband_mv[NUMERO_ESTADOS];   // deadband de tensión en mV
    power_state_t modo_actual;
    bool          activado;

    uint8_t  intentos_arranque;
    uint8_t  intentos_lectura;
    uint32_t events;

    sensor_mag_state_t mag[INA_MAGNITUDES];  // [0]=corriente, [1]=voltaje bus

    queue_handle_t cmd_queue;
} ina_fsm_t;

void InaFSM_Init(ina_fsm_t*      p,
                 uint8_t         hw_id,
                 uint8_t         payload_id_corriente,
                 uint8_t         payload_id_voltaje,
                 power_rail_t    rail,
                 const uint32_t* def_timeouts,
                 const uint8_t*  def_deadband_ma,
                 const uint8_t*  def_deadband_mv,
                 uint32_t        cmd_queue_depth);

void InaFSM_DrainCmdQueue    (ina_fsm_t* p);
void InaFSM_ApplyCmd         (ina_fsm_t* p, const config_cmd_t* cmd);

void InaFSM_TriggerMedida    (ina_fsm_t* p);

// Estos dos Notify ahora también leen el HW (una sola vez) y dejan el
// resultado en los flags OK/FAIL.
void InaFSM_NotifyBootTimer  (ina_fsm_t* p);
void InaFSM_NotifyMedidaTimer(ina_fsm_t* p);

#endif /* INC_INA_FSM_H_ */
