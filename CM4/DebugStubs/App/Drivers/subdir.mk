################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../App/Drivers/i2c1_lock.c \
../App/Drivers/i2c2_lock.c \
../App/Drivers/lora_tx.c \
../App/Drivers/payload_queue.c \
../App/Drivers/power_queue.c \
../App/Drivers/power_rail_hw.c \
../App/Drivers/uart_link.c 

OBJS += \
./App/Drivers/i2c1_lock.o \
./App/Drivers/i2c2_lock.o \
./App/Drivers/lora_tx.o \
./App/Drivers/payload_queue.o \
./App/Drivers/power_queue.o \
./App/Drivers/power_rail_hw.o \
./App/Drivers/uart_link.o 

C_DEPS += \
./App/Drivers/i2c1_lock.d \
./App/Drivers/i2c2_lock.d \
./App/Drivers/lora_tx.d \
./App/Drivers/payload_queue.d \
./App/Drivers/power_queue.d \
./App/Drivers/power_rail_hw.d \
./App/Drivers/uart_link.d 


# Each subdirectory must supply rules for building sources it contributes
App/Drivers/%.o App/Drivers/%.su App/Drivers/%.cyclo: ../App/Drivers/%.c App/Drivers/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DCORE_CM4 -DUSE_HAL_DRIVER -DSTM32WL55xx -DUSE_NUCLEO_64 -c -I../Core/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32WLxx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32WLxx/Include -I../../Drivers/CMSIS/Include -I/Users/eduardogonzalezsalgado/.rbenv/versions/3.2.3/lib/ruby/gems/3.2.0/gems/ceedling-0.31.1/vendor/unity/src -I/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/CM4/Core/Inc -I../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I"/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/CM4/App/Inc" -I"/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/CM4/App/Ports" -I"/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/CM4/App/Drivers" -I../LoRaWAN/App -I../LoRaWAN/Target -I../MbMux -I../../Common/MbMux -I../../Utilities/trace/adv_trace -I../../Utilities/misc -I../../Utilities/timer -I../../Utilities/lpm/tiny_lpm -I../../Middlewares/Third_Party/LoRaWAN/Mac/Region -I../../Middlewares/Third_Party/LoRaWAN/Mac -I../../Middlewares/Third_Party/LoRaWAN/LmHandler -I../../Middlewares/Third_Party/LoRaWAN/Utilities -I../../Middlewares/Third_Party/SubGHz_Phy -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-App-2f-Drivers

clean-App-2f-Drivers:
	-$(RM) ./App/Drivers/i2c1_lock.cyclo ./App/Drivers/i2c1_lock.d ./App/Drivers/i2c1_lock.o ./App/Drivers/i2c1_lock.su ./App/Drivers/i2c2_lock.cyclo ./App/Drivers/i2c2_lock.d ./App/Drivers/i2c2_lock.o ./App/Drivers/i2c2_lock.su ./App/Drivers/lora_tx.cyclo ./App/Drivers/lora_tx.d ./App/Drivers/lora_tx.o ./App/Drivers/lora_tx.su ./App/Drivers/payload_queue.cyclo ./App/Drivers/payload_queue.d ./App/Drivers/payload_queue.o ./App/Drivers/payload_queue.su ./App/Drivers/power_queue.cyclo ./App/Drivers/power_queue.d ./App/Drivers/power_queue.o ./App/Drivers/power_queue.su ./App/Drivers/power_rail_hw.cyclo ./App/Drivers/power_rail_hw.d ./App/Drivers/power_rail_hw.o ./App/Drivers/power_rail_hw.su ./App/Drivers/uart_link.cyclo ./App/Drivers/uart_link.d ./App/Drivers/uart_link.o ./App/Drivers/uart_link.su

.PHONY: clean-App-2f-Drivers

