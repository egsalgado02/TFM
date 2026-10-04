################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/adc.c \
../Core/Src/app_freertos.c \
../Core/Src/gpio.c \
../Core/Src/i2c.c \
../Core/Src/ipcc.c \
../Core/Src/ipcc_if.c \
../Core/Src/lptim.c \
../Core/Src/main.c \
../Core/Src/rtc.c \
../Core/Src/spi.c \
../Core/Src/stm32_lpm_if.c \
../Core/Src/stm32wlxx_hal_msp.c \
../Core/Src/stm32wlxx_hal_timebase_tim.c \
../Core/Src/stm32wlxx_it.c \
../Core/Src/sys_app.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/tim.c \
../Core/Src/timer_if.c \
../Core/Src/usart.c 

OBJS += \
./Core/Src/adc.o \
./Core/Src/app_freertos.o \
./Core/Src/gpio.o \
./Core/Src/i2c.o \
./Core/Src/ipcc.o \
./Core/Src/ipcc_if.o \
./Core/Src/lptim.o \
./Core/Src/main.o \
./Core/Src/rtc.o \
./Core/Src/spi.o \
./Core/Src/stm32_lpm_if.o \
./Core/Src/stm32wlxx_hal_msp.o \
./Core/Src/stm32wlxx_hal_timebase_tim.o \
./Core/Src/stm32wlxx_it.o \
./Core/Src/sys_app.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/tim.o \
./Core/Src/timer_if.o \
./Core/Src/usart.o 

C_DEPS += \
./Core/Src/adc.d \
./Core/Src/app_freertos.d \
./Core/Src/gpio.d \
./Core/Src/i2c.d \
./Core/Src/ipcc.d \
./Core/Src/ipcc_if.d \
./Core/Src/lptim.d \
./Core/Src/main.d \
./Core/Src/rtc.d \
./Core/Src/spi.d \
./Core/Src/stm32_lpm_if.d \
./Core/Src/stm32wlxx_hal_msp.d \
./Core/Src/stm32wlxx_hal_timebase_tim.d \
./Core/Src/stm32wlxx_it.d \
./Core/Src/sys_app.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/tim.d \
./Core/Src/timer_if.d \
./Core/Src/usart.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DCORE_CM4 -DUSE_HAL_DRIVER -DSTM32WL55xx -DUSE_NUCLEO_64 -c -I../Core/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32WLxx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32WLxx/Include -I../../Drivers/CMSIS/Include -I/Users/eduardogonzalezsalgado/.rbenv/versions/3.2.3/lib/ruby/gems/3.2.0/gems/ceedling-0.31.1/vendor/unity/src -I/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/CM4/Core/Inc -I../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I"/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/CM4/App/Inc" -I"/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/CM4/App/Ports" -I"/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/CM4/App/Drivers" -I../LoRaWAN/App -I../LoRaWAN/Target -I../MbMux -I../../Common/MbMux -I../../Utilities/trace/adv_trace -I../../Utilities/misc -I../../Utilities/timer -I../../Utilities/lpm/tiny_lpm -I../../Middlewares/Third_Party/LoRaWAN/Mac/Region -I../../Middlewares/Third_Party/LoRaWAN/Mac -I../../Middlewares/Third_Party/LoRaWAN/LmHandler -I../../Middlewares/Third_Party/LoRaWAN/Utilities -I../../Middlewares/Third_Party/SubGHz_Phy -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/adc.cyclo ./Core/Src/adc.d ./Core/Src/adc.o ./Core/Src/adc.su ./Core/Src/app_freertos.cyclo ./Core/Src/app_freertos.d ./Core/Src/app_freertos.o ./Core/Src/app_freertos.su ./Core/Src/gpio.cyclo ./Core/Src/gpio.d ./Core/Src/gpio.o ./Core/Src/gpio.su ./Core/Src/i2c.cyclo ./Core/Src/i2c.d ./Core/Src/i2c.o ./Core/Src/i2c.su ./Core/Src/ipcc.cyclo ./Core/Src/ipcc.d ./Core/Src/ipcc.o ./Core/Src/ipcc.su ./Core/Src/ipcc_if.cyclo ./Core/Src/ipcc_if.d ./Core/Src/ipcc_if.o ./Core/Src/ipcc_if.su ./Core/Src/lptim.cyclo ./Core/Src/lptim.d ./Core/Src/lptim.o ./Core/Src/lptim.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/rtc.cyclo ./Core/Src/rtc.d ./Core/Src/rtc.o ./Core/Src/rtc.su ./Core/Src/spi.cyclo ./Core/Src/spi.d ./Core/Src/spi.o ./Core/Src/spi.su ./Core/Src/stm32_lpm_if.cyclo ./Core/Src/stm32_lpm_if.d ./Core/Src/stm32_lpm_if.o ./Core/Src/stm32_lpm_if.su ./Core/Src/stm32wlxx_hal_msp.cyclo ./Core/Src/stm32wlxx_hal_msp.d ./Core/Src/stm32wlxx_hal_msp.o ./Core/Src/stm32wlxx_hal_msp.su ./Core/Src/stm32wlxx_hal_timebase_tim.cyclo ./Core/Src/stm32wlxx_hal_timebase_tim.d ./Core/Src/stm32wlxx_hal_timebase_tim.o ./Core/Src/stm32wlxx_hal_timebase_tim.su ./Core/Src/stm32wlxx_it.cyclo ./Core/Src/stm32wlxx_it.d ./Core/Src/stm32wlxx_it.o ./Core/Src/stm32wlxx_it.su ./Core/Src/sys_app.cyclo ./Core/Src/sys_app.d ./Core/Src/sys_app.o ./Core/Src/sys_app.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/tim.cyclo ./Core/Src/tim.d ./Core/Src/tim.o ./Core/Src/tim.su ./Core/Src/timer_if.cyclo ./Core/Src/timer_if.d ./Core/Src/timer_if.o ./Core/Src/timer_if.su ./Core/Src/usart.cyclo ./Core/Src/usart.d ./Core/Src/usart.o ./Core/Src/usart.su

.PHONY: clean-Core-2f-Src

