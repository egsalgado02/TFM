#ifndef INC_POWER_FSM_H_
#define INC_POWER_FSM_H_

#include "fsm.h"
#include <stdint.h>
#include <stdbool.h>

// Gestor de modos de energía del sistema. Esta FSM "manda" sobre las demás:
// según el voltaje de batería (y si hay batería externa o no) decide en qué
// modo está el sistema entero y lo difunde a todos los sensores.
//
// Modos, de más alegre a más austero:
//   ABUNDANCIA   - todo a tope, periodos cortos, todo encendido
//   CONSERVACION - periodos medios, ahorro suave
//   EMERGENCIA   - mínimos, apagamos el rail de expansión, todo lento
//
// Entradas:
//   - PushVoltaje(mv)            - viene del adc_fsm
//   - PushConexionBateria(bool)  - viene del GPIO de detección
//   - PushBatteryThresholds(...) - viene por LoRa, cambiamos umbrales
// Salidas:
//   - Config_ApplySystemMode(modo)        - broadcast a todos los sensores
//   - PowerRail_EmergencyCutoff(EXPANSION) - al entrar en EMERGENCIA

// Bit del campo events:
#define EV_NEW_THRESHOLDS (1 << 0)   // han llegado umbrales nuevos por LoRa

typedef enum {
    POWER_STATE_ABUNDANCIA  = 0,
    POWER_STATE_CONSERVACION,
    POWER_STATE_EMERGENCIA,
    NUMERO_ESTADOS   // <-- usado como tamaño en arrays por modo
} power_state_t;

// Alias por si alguien lo busca con el otro nombre
#define NUMERO_POWER_STATES NUMERO_ESTADOS

typedef struct {
    fsm_t motor;     // SIEMPRE primer campo

    uint16_t voltaje_actual_mv;
    bool     bateria_externa_conectada;

    uint16_t umbral_emergencia_mv;
    uint16_t umbral_conservacion_mv;

    // Cuando llega una orden de cambiar umbrales por LoRa,
    // se guardan aquí y se aplican en el próximo fire.
    uint32_t events;
    uint16_t rx_umbral_emergencia_mv;
    uint16_t rx_umbral_conservacion_mv;
} power_fsm_t;

void PowerFSMInit(power_fsm_t* p_fsm, power_state_t estado_inicial);

void PowerManager_PushVoltaje          (power_fsm_t* p_fsm, uint16_t mv);
void PowerManager_PushConexionBateria  (power_fsm_t* p_fsm, bool conectada);
void PowerManager_PushBatteryThresholds(power_fsm_t* p_fsm,
                                        uint16_t umbral_emergencia_mv,
                                        uint16_t umbral_conservacion_mv);

#endif /* INC_POWER_FSM_H_ */
