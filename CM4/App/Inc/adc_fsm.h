#ifndef ADC_FSM_H_
#define ADC_FSM_H_

#include "fsm.h"
#include <stdint.h>
#include <stdbool.h>
#include "power_fsm.h"
#include "rtos_iface.h"
#include "config_cmd.h"

// FSM del ADC: mide la tensión de batería con cierta periodicidad y la manda
// SOLO si está fuera de la "ventana" [min, max] del modo actual, o si toca
// envío de mantenimiento (lleva demasiado tiempo sin mandar nada).
//
// Es una FSM con un solo estado activo (DORMIDO). Un solo "fire" hace
// todo el ciclo: pide el rail, mide, envía si toca, suelta el rail y
// reprograma el siguiente despertar.

// Bit del campo events:
#define ADC_EV_MEDIR  (1 << 0)   // toca medir

// Keepalive en milisegundos (en el header para que lo vean los tests).
/* Envío de mantenimiento: si una magnitud lleva este tiempo sin publicarse,
 * se publica aunque no haya cambiado. 20 s para las pruebas en banco; en
 * despliegue serían 24 h, (24UL*60UL*60UL*1000UL). */
#define ADC_KEEPALIVE_MS (20UL * 1000UL)                       // 20 s

typedef struct {
    fsm_t fsm;        // motor genérico, SIEMPRE primer campo

    // Periodos por modo (más espaciado en EMERGENCIA)
    uint32_t      timeouts_ms[NUMERO_ESTADOS];

    // Ventana de tensión por modo: si el voltaje sale de [inf, sup] -> envío.
    uint16_t      umbrales_inf_mv[NUMERO_ESTADOS];
    uint16_t      umbrales_sup_mv[NUMERO_ESTADOS];

    power_state_t modo_actual;
    bool          activado;
    uint32_t      events;       // bitmask: ADC_EV_MEDIR

    // Estado para keepalive. No uso sensor_mag_state porque aquí
    // no aplico deadband, sino ventana.
    uint32_t      tick_last_sent_ms;
    bool          ever_sent;

    queue_handle_t cmd_queue;
} adc_fsm_t;

void AdcFSM_Init(adc_fsm_t*      p,
                 const uint16_t  min_mv[NUMERO_ESTADOS],
                 const uint16_t  max_mv[NUMERO_ESTADOS],
                 const uint32_t  t[NUMERO_ESTADOS],
                 uint32_t        cmd_queue_depth);

void AdcFSM_DrainCmdQueue(adc_fsm_t* p);
void AdcFSM_ApplyCmd     (adc_fsm_t* p, const config_cmd_t* cmd);
void AdcFSM_TriggerMedida(adc_fsm_t* p);   // el timer de sueño expiró

#endif /* ADC_FSM_H_ */
