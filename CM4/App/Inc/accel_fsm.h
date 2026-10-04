#ifndef INC_ACCEL_FSM_H_
#define INC_ACCEL_FSM_H_

#include "fsm.h"
#include <stdint.h>
#include <stdbool.h>
#include "power_fsm.h"
#include "rtos_iface.h"
#include "config_cmd.h"
#include "sensor_mag_state.h"

// FSM del acelerómetro. Dos cosas que hace:
//   1) Medida de rutina: pasados X ms, leer los 3 ejes y mandarlos
//      (con banda muerta y envío de mantenimiento).
//   2) Alarma: cuando el sensor mete una interrupción, mandamos
//      una trama de emergencia con la causa (robo o caída).

// IDs que llevan los mensajes al payload (uno por eje).
// Las alarmas van por FPORT_EMERGENCIA con sensor_id = ID_ACCEL_X;
// el emerg_codigo ya identifica la causa (EMERG_ROBO, EMERG_CAIDA…).
// No hace falta un cuarto ID: el FPort distingue rutina de emergencia.
#define ID_ACCEL_X  0
#define ID_ACCEL_Y  1
#define ID_ACCEL_Z  2

#define ACCEL_MAGNITUDES    3                                  // X, Y, Z
/* Envío de mantenimiento: si una magnitud lleva este tiempo sin publicarse,
 * se publica aunque no haya cambiado. 20 s para las pruebas en banco; en
 * despliegue serían 24 h, (24UL*60UL*60UL*1000UL). */
#define ACCEL_KEEPALIVE_MS  (20UL * 1000UL)                    // 20 s

// Bits del campo events:
#define ACCEL_EV_MEDIR  (1 << 0)   // toca leer
#define ACCEL_EV_ALARM  (1 << 1)   // ha saltado la alarma

typedef struct {
    fsm_t fsm;        // SIEMPRE primer campo: el motor genérico

    // Periodo de muestreo según el modo de energía (más espaciado en EMERGENCIA).
    uint32_t           timeouts_ms[NUMERO_ESTADOS];

    // Umbral de alarma. UN solo valor: una vibración peligrosa lo es
    // independientemente del modo de energía.
    uint8_t            threshold_g;

    // Deadband por modo (más grande en EMERGENCIA para mandar menos).
    uint8_t            deadband_lsb[NUMERO_ESTADOS];

    power_state_t      modo_energia_actual;
    bool               activado;
    uint32_t           events;       // bitmask: ACCEL_EV_MEDIR | ACCEL_EV_ALARM

    sensor_mag_state_t mag[ACCEL_MAGNITUDES];   // estado X/Y/Z para deadband

    queue_handle_t     cmd_queue;    // cola por la que llegan los config_cmd_t
} accel_fsm_t;

// API pública:
void AccelFSM_Init(accel_fsm_t*    p,
                   uint8_t         def_threshold,
                   const uint32_t* def_timeouts,
                   const uint8_t*  def_deadband,
                   uint32_t        cmd_queue_depth);

void AccelFSM_DrainCmdQueue(accel_fsm_t* p);   // vacía la cola de comandos y los aplica
void AccelFSM_ApplyCmd     (accel_fsm_t* p, const config_cmd_t* cmd);

// Entradas de evento: la tarea (o una ISR) las usa para "despertar" la FSM.
void AccelFSM_TriggerMedida(accel_fsm_t* p);   // timer de sueño expiró
void AccelFSM_TriggerAlarma(accel_fsm_t* p);   // ISR del acelerómetro

#endif /* INC_ACCEL_FSM_H_ */
