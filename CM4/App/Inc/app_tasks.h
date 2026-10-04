#ifndef INC_APP_TASKS_H_
#define INC_APP_TASKS_H_

// Cuerpos de las tareas de FreeRTOS y arranque centralizado de las FSMs.
//
// CubeMX no toca estos ficheros al regenerar, porque están fuera de main.c
// y de app_freertos.c. Así el código de aplicación no se pierde al cambiar
// el .ioc.
//
// Cómo se usa desde main.c (USER CODE BEGIN 2, ANTES de osKernelInitialize):
//   AppTasks_InitAll();
//
// Cómo se usa desde el .ioc:
//   En cada tarea que crees con CubeMX, pon "Entry Function" =
//   StartXxxTask y "Code Generation" = "As external".

#include "cmsis_os.h"

// Llamar UNA vez al arrancar (USER CODE 2 de main.c).
//   - Inicializa power_rail_manager
//   - Inicializa todas las FSMs con configuración por defecto
//   - Inicializa config_module y registra los sensores
void AppTasks_InitAll(void);

// Entry points de las tareas (los referencia CubeMX en el .ioc).
// NOTA: no hay StartLoraTask. En dual-core la radio vive en el M0+ y el
// M4 solo necesita inicializar el stack (main.c) y llamar LmHandlerSend
// desde payload_builder cuando toque enviar. No hay tarea LoRa dedicada.
void StartAccelTask  (void* arg);
void StartBmeTask    (void* arg);
void StartInaTask    (void* arg);
void StartAdcTask    (void* arg);
void StartPowerTask  (void* arg);
void StartPayloadTask(void* arg);

#endif /* INC_APP_TASKS_H_ */
