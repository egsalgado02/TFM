/*
 * power_rail_hw.c - la parte que toca los pines de los dominios.
 *
 * power_rail_manager.c lleva la cuenta de quién está usando cada dominio y
 * decide cuándo encender o apagar. Esto solo pone el pin como le dicen.
 *
 * Dominios y pines (esquemático y .ioc):
 *   RAIL_INTERNO   -> PA8 (PWR_INA_INT): INA238 interno y divisor del ADC
 *   RAIL_EXPANSION -> PA1 (PWR_EXP): mikroBUS, conector e INA238 de expansión
 *
 * Pin a 1 = interruptor cerrado = dominio encendido. Si en otra placa fuera
 * al revés, solo habría que cambiarlo aquí.
 *
 * Se compila en las dos configuraciones (Debug y DebugStubs). Los drivers
 * de los sensores tienen una versión falsa en ports_stub.c, pero esto son
 * dos pines y funciona aunque no haya ningún chip soldado.
 */

#include "power_rail_hw.h"
#include "main.h"                     /* PWR_INA_INT_Pin, PWR_EXP_Pin (nombres del .ioc) */
#include "stm32wlxx_hal.h"

void PowerRailHW_SetState(power_rail_t rail, bool encendido)
{
    GPIO_PinState val = encendido ? GPIO_PIN_SET : GPIO_PIN_RESET;
    switch (rail) {
        case RAIL_INTERNO:
            HAL_GPIO_WritePin(PWR_INA_INT_GPIO_Port, PWR_INA_INT_Pin, val);
            break;
        case RAIL_EXPANSION:
            HAL_GPIO_WritePin(PWR_EXP_GPIO_Port, PWR_EXP_Pin, val);
            break;
        default:
            /* Dominio que no existe: no hago nada */
            break;
    }
}
