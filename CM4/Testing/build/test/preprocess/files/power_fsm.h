#include "../App/Inc/fsm.h"






typedef enum {

    POWER_STATE_ABUNDANCIA = 0,

    POWER_STATE_CONSERVACION,

    POWER_STATE_EMERGENCIA,

    NUMERO_ESTADOS

} power_state_t;









typedef struct {

    fsm_t motor;



    uint16_t voltaje_actual_mv;

    _Bool bateria_externa_conectada;



    uint16_t umbral_emergencia_mv;

    uint16_t umbral_conservacion_mv;







    uint32_t events;

    uint16_t rx_umbral_emergencia_mv;

    uint16_t rx_umbral_conservacion_mv;

} power_fsm_t;



void PowerFSMInit(power_fsm_t* p_fsm, power_state_t estado_inicial);



void PowerManager_PushVoltaje (power_fsm_t* p_fsm, uint16_t mv);

void PowerManager_PushConexionBateria (power_fsm_t* p_fsm, _Bool conectada);

void PowerManager_PushBatteryThresholds(power_fsm_t* p_fsm,

                                        uint16_t umbral_emergencia_mv,

                                        uint16_t umbral_conservacion_mv);
