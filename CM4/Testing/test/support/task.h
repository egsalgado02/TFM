/* Sustituto de task.h para los tests. En test_config_module nunca se
 * registra un handle de tarea, asi que xTaskNotifyGive no llega a
 * llamarse; basta con que compile. */
#ifndef TASK_H_TEST_STUB
#define TASK_H_TEST_STUB

typedef void* TaskHandle_t;
#define xTaskNotifyGive(h) ((void)(h))

#endif
