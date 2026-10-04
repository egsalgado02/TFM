

















void BmePort_SetPowerGPIO (_Bool estado);

void BmePort_StartBootTimer (void);

_Bool BmePort_CheckStatusI2C (void);

void BmePort_TriggerForcedReading(void);

void BmePort_StartMedidaTimer (void);

_Bool BmePort_IsDataReady (void);

_Bool BmePort_ReadData (int16_t* temperatura, int16_t* humedad, int16_t* presion);

void BmePort_StartSleepTimer (uint32_t timeout_ms);

uint32_t BmePort_GetTickMs (void);
