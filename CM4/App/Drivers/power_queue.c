// Cola de tensión: del adc_fsm al power_fsm.
// ColaPower_EnviarVoltaje mete la tensión medida en colaPowerHandle y
// StartPowerTask (app_tasks.c) la saca y dispara el power_fsm.
//
// La cola (4 valores de uint16_t) la crea CubeMX en app_freertos.c, como la
// de salida; aquí solo se usa.

#include "power_queue.h"
#include "cmsis_os.h"

extern osMessageQueueId_t colaPowerHandle;

void ColaPower_EnviarVoltaje(uint16_t voltaje_mv) {
    if (colaPowerHandle == NULL) return;
    // Timeout 0: no bloquear. Si la cola está llena (4 slots), se descarta.
    // El power_fsm no necesita todos los valores, con el último es suficiente.
    osMessageQueuePut(colaPowerHandle, &voltaje_mv, 0U, 0U);
}
