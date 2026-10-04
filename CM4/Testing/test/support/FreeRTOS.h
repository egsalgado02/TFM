/* Sustituto de FreeRTOS.h para compilar en el PC con Ceedling.
 * config_module.c lo incluye solo para poder llamar a xTaskNotifyGive;
 * aqui no hace falta nada del kernel real. */
#ifndef FREERTOS_H_TEST_STUB
#define FREERTOS_H_TEST_STUB
#endif
