################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../LoRaWAN/App/app_lorawan.c \
../LoRaWAN/App/lora_app.c 

OBJS += \
./LoRaWAN/App/app_lorawan.o \
./LoRaWAN/App/lora_app.o 

C_DEPS += \
./LoRaWAN/App/app_lorawan.d \
./LoRaWAN/App/lora_app.d 


# Each subdirectory must supply rules for building sources it contributes
LoRaWAN/App/%.o LoRaWAN/App/%.su LoRaWAN/App/%.cyclo: ../LoRaWAN/App/%.c LoRaWAN/App/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DCORE_CM0PLUS -DDEBUG '-DCORE_CM0+' -DSTM32WL55xx -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -c -I../Core/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32WLxx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32WLxx/Include -I../../Drivers/CMSIS/Include -I../LoRaWAN/App -I../LoRaWAN/Target -I../MbMux -I../../Common/MbMux -I../../Utilities/trace/adv_trace -I../../Utilities/misc -I../../Utilities/sequencer -I../../Utilities/timer -I../../Utilities/lpm/tiny_lpm -I../../Middlewares/Third_Party/LoRaWAN/LmHandler/Packages -I../../Middlewares/Third_Party/LoRaWAN/Crypto -I../../Middlewares/Third_Party/LoRaWAN/Mac/Region -I../../Middlewares/Third_Party/LoRaWAN/Mac -I../../Middlewares/Third_Party/LoRaWAN/LmHandler -I../../Middlewares/Third_Party/LoRaWAN/Utilities -I../../Middlewares/Third_Party/SubGHz_Phy -I../../Middlewares/Third_Party/SubGHz_Phy/stm32_radio_driver -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-LoRaWAN-2f-App

clean-LoRaWAN-2f-App:
	-$(RM) ./LoRaWAN/App/app_lorawan.cyclo ./LoRaWAN/App/app_lorawan.d ./LoRaWAN/App/app_lorawan.o ./LoRaWAN/App/app_lorawan.su ./LoRaWAN/App/lora_app.cyclo ./LoRaWAN/App/lora_app.d ./LoRaWAN/App/lora_app.o ./LoRaWAN/App/lora_app.su

.PHONY: clean-LoRaWAN-2f-App

