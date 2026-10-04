/*
 * i2c1_lock.c - lock del bus I2C1 (expansión). El uso está en i2c1_lock.h.
 */

#include "i2c1_lock.h"
#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t s_mutex = NULL;

void I2C1_Lock_Init(void)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
    }
}

bool I2C1_Lock_Take(void)
{
    if (s_mutex == NULL) return false;
    return xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE;
}

void I2C1_Lock_Give(void)
{
    if (s_mutex == NULL) return;
    xSemaphoreGive(s_mutex);
}
