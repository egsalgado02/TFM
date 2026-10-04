/*
 * i2c2_lock.c - lock del bus I2C2. El uso está explicado en i2c2_lock.h.
 *
 * Es un semáforo binario y no un mutex de FreeRTOS. Un mutex solo lo
 * puede soltar la misma tarea que lo cogió, porque lleva la herencia de
 * prioridad, y no se puede soltar desde una interrupción. Con un semáforo
 * binario esa restricción no existe, y la herencia de prioridad aquí no
 * hace falta. Por eso Give mira si está en una interrupción y usa la
 * versión FromISR si toca.
 *
 * Hoy todos los accesos a I2C2 son bloqueantes y se hacen desde tareas,
 * así que el caso de la interrupción no se da.
 */

#include "i2c2_lock.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

static SemaphoreHandle_t s_sem = NULL;

void I2C2_Lock_Init(void)
{
    if (s_sem == NULL) {
        s_sem = xSemaphoreCreateBinary();
        if (s_sem != NULL) {
            (void)xSemaphoreGive(s_sem);   /* un binario se crea cogido, lo suelto */
        }
    }
}

bool I2C2_Lock_Take(void)
{
    if (s_sem == NULL) return false;
    return xSemaphoreTake(s_sem, portMAX_DELAY) == pdTRUE;
}

void I2C2_Lock_Give(void)
{
    if (s_sem == NULL) return;
    if (xPortIsInsideInterrupt()) {
        BaseType_t hp_woken = pdFALSE;
        (void)xSemaphoreGiveFromISR(s_sem, &hp_woken);
        portYIELD_FROM_ISR(hp_woken);
    } else {
        (void)xSemaphoreGive(s_sem);
    }
}
