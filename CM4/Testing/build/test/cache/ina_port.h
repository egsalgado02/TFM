

















void InaPort_StartBootTimer (uint8_t hw_id);

_Bool InaPort_CheckStatusI2C (uint8_t hw_id);

void InaPort_TriggerForcedReading(uint8_t hw_id);

void InaPort_StartMedidaTimer (uint8_t hw_id);

_Bool InaPort_IsDataReady (uint8_t hw_id);

_Bool InaPort_ReadData (uint8_t hw_id, int16_t* corriente, int16_t* voltaje);

void InaPort_StartSleepTimer (uint8_t hw_id, uint32_t time_ms);

uint32_t InaPort_GetTickMs (void);
