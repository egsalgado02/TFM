





















void AccelPort_Init (void);

void AccelPort_IniciarLecturaI2C (void);

_Bool AccelPort_DatosListos (void);

_Bool AccelPort_LeerBuffer (int16_t* x, int16_t* y, int16_t* z);

_Bool AccelPort_LeerCausasAlarma (_Bool* p_es_robo, _Bool* p_es_caida);

void AccelPort_SetThreshold (uint8_t threshold_g);

void AccelPort_StartSleepTimer (uint32_t time_ms);

uint32_t AccelPort_GetTickMs (void);
