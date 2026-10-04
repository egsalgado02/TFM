





typedef enum {

    RAIL_INTERNO = 0,

    RAIL_EXPANSION,

    MAX_POWER_RAILS

} power_rail_t;



void PowerRail_Init (void);

void PowerRail_Request (power_rail_t rail);

void PowerRail_Release (power_rail_t rail);

void PowerRail_EmergencyCutoff(power_rail_t rail);

uint8_t PowerRail_GetRefCount (power_rail_t rail);
