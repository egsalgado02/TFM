#ifndef POWER_RAIL_MANAGER_H
#define POWER_RAIL_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// Gestor de "rails" de alimentación con contador de referencias.
// Idea: varios módulos pueden compartir un mismo rail (un MOSFET que
// alimenta a varios sensores). Cada uno hace Request cuando lo
// necesita y Release cuando termina. El rail se enciende cuando
// llega el primer Request y se apaga cuando todos sueltan.
//
// Ejemplo: el INA del STM y el ADC comparten RAIL_INTERNO.

typedef enum {
    RAIL_INTERNO   = 0,    // alimenta INA del STM + divisor del ADC
    RAIL_EXPANSION,        // MikroBUS + INA externos
    MAX_POWER_RAILS        // tamaño del array
} power_rail_t;

void    PowerRail_Init           (void);
void    PowerRail_Request        (power_rail_t rail);   // refcount++; enciende si era 0
void    PowerRail_Release        (power_rail_t rail);   // refcount--; apaga si llega a 0
void    PowerRail_EmergencyCutoff(power_rail_t rail);   // corta YA, refcount = 0
uint8_t PowerRail_GetRefCount    (power_rail_t rail);

#endif /* POWER_RAIL_MANAGER_H */
