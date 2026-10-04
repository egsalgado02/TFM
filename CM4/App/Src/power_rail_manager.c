#include "power_rail_manager.h"
#include "power_rail_hw.h"

// Un contador por cada rail. uint8_t llega de sobra: nunca tendré
// 255 módulos compartiendo el mismo rail.
static uint8_t ref_counts[MAX_POWER_RAILS];

void PowerRail_Init(void) {
    // Al arrancar, todos los rails apagados y refcount a 0.
    for (int i = 0; i < MAX_POWER_RAILS; i++) {
        ref_counts[i] = 0;
        PowerRailHW_SetState((power_rail_t)i, false);
    }
}

void PowerRail_Request(power_rail_t rail) {
    if (rail >= MAX_POWER_RAILS) return;   // proteger contra valores raros
    // Si nadie lo tenía pedido, lo encendemos. Si ya estaba encendido,
    // solo subimos el contador.
    if (ref_counts[rail] == 0) {
        PowerRailHW_SetState(rail, true);
    }
    if (ref_counts[rail] < 255) {
        ref_counts[rail]++;
    }
}

void PowerRail_Release(power_rail_t rail) {
    if (rail >= MAX_POWER_RAILS) return;
    // Solo decrementamos si había alguien pidiéndolo. Cuando baja a 0,
    // apagamos el rail.
    if (ref_counts[rail] > 0) {
        ref_counts[rail]--;
        if (ref_counts[rail] == 0) {
            PowerRailHW_SetState(rail, false);
        }
    }
}

void PowerRail_EmergencyCutoff(power_rail_t rail) {
    if (rail >= MAX_POWER_RAILS) return;
    // Cortar YA sin importar quién lo tuviera pedido.
    // Lo usa el power_fsm al entrar en EMERGENCIA.
    ref_counts[rail] = 0;
    PowerRailHW_SetState(rail, false);
}

uint8_t PowerRail_GetRefCount(power_rail_t rail) {
    if (rail >= MAX_POWER_RAILS) return 0;
    return ref_counts[rail];
}
