################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../App/Src/accel_fsm.c \
../App/Src/adc_fsm.c \
../App/Src/app_tasks.c \
../App/Src/bme_fsm.c \
../App/Src/config_module.c \
../App/Src/fsm.c \
../App/Src/ina_fsm.c \
../App/Src/payload_builder_fsm.c \
../App/Src/power_fsm.c \
../App/Src/power_rail_manager.c \
../App/Src/rtos_iface_cmsis.c 

OBJS += \
./App/Src/accel_fsm.o \
./App/Src/adc_fsm.o \
./App/Src/app_tasks.o \
./App/Src/bme_fsm.o \
./App/Src/config_module.o \
./App/Src/fsm.o \
./App/Src/ina_fsm.o \
./App/Src/payload_builder_fsm.o \
./App/Src/power_fsm.o \
./App/Src/power_rail_manager.o \
./App/Src/rtos_iface_cmsis.o 

C_DEPS += \
./App/Src/accel_fsm.d \
./App/Src/adc_fsm.d \
./App/Src/app_tasks.d \
./App/Src/bme_fsm.d \
./App/Src/config_module.d \
./App/Src/fsm.d \
./App/Src/ina_fsm.d \
./App/Src/payload_builder_fsm.d \
./App/Src/power_fsm.d \
./App/Src/power_rail_manager.d \
./App/Src/rtos_iface_cmsis.d 


# Each subdirectory must supply rules for building sources it contributes
App/Src/%.o App/Src/%.su App/Src/%.cyclo: ../App/Src/%.c App/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DCORE_CM4 -DUSE_HAL_DRIVER -DSTM32WL55xx -DUSE_NUCLEO_64 -c -I../Core/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32WLxx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32WLxx/Include -I../../Drivers/CMSIS/Include -I/Users/eduardogonzalezsalgado/.rbenv/versions/3.2.3/lib/ruby/gems/3.2.0/gems/ceedling-0.31.1/vendor/unity/src -I/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/CM4/Core/Inc -I../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I"/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/CM4/App/Inc" -I"/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/CM4/App/Ports" -I"/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/CM4/App/Drivers" -I../LoRaWAN/App -I../LoRaWAN/Target -I../MbMux -I../../Common/MbMux -I../../Utilities/trace/adv_trace -I../../Utilities/misc -I../../Utilities/timer -I../../Utilities/lpm/tiny_lpm -I../../Middlewares/Third_Party/LoRaWAN/Mac/Region -I../../Middlewares/Third_Party/LoRaWAN/Mac -I../../Middlewares/Third_Party/LoRaWAN/LmHandler -I../../Middlewares/Third_Party/LoRaWAN/Utilities -I../../Middlewares/Third_Party/SubGHz_Phy -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-App-2f-Src

clean-App-2f-Src:
	-$(RM) ./App/Src/accel_fsm.cyclo ./App/Src/accel_fsm.d ./App/Src/accel_fsm.o ./App/Src/accel_fsm.su ./App/Src/adc_fsm.cyclo ./App/Src/adc_fsm.d ./App/Src/adc_fsm.o ./App/Src/adc_fsm.su ./App/Src/app_tasks.cyclo ./App/Src/app_tasks.d ./App/Src/app_tasks.o ./App/Src/app_tasks.su ./App/Src/bme_fsm.cyclo ./App/Src/bme_fsm.d ./App/Src/bme_fsm.o ./App/Src/bme_fsm.su ./App/Src/config_module.cyclo ./App/Src/config_module.d ./App/Src/config_module.o ./App/Src/config_module.su ./App/Src/fsm.cyclo ./App/Src/fsm.d ./App/Src/fsm.o ./App/Src/fsm.su ./App/Src/ina_fsm.cyclo ./App/Src/ina_fsm.d ./App/Src/ina_fsm.o ./App/Src/ina_fsm.su ./App/Src/payload_builder_fsm.cyclo ./App/Src/payload_builder_fsm.d ./App/Src/payload_builder_fsm.o ./App/Src/payload_builder_fsm.su ./App/Src/power_fsm.cyclo ./App/Src/power_fsm.d ./App/Src/power_fsm.o ./App/Src/power_fsm.su ./App/Src/power_rail_manager.cyclo ./App/Src/power_rail_manager.d ./App/Src/power_rail_manager.o ./App/Src/power_rail_manager.su ./App/Src/rtos_iface_cmsis.cyclo ./App/Src/rtos_iface_cmsis.d ./App/Src/rtos_iface_cmsis.o ./App/Src/rtos_iface_cmsis.su

.PHONY: clean-App-2f-Src

