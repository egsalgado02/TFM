################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/Middlewares/Third_Party/SubGHz_Phy/stm32_radio_driver/radio.c \
/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/Middlewares/Third_Party/SubGHz_Phy/stm32_radio_driver/radio_driver.c \
/Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/Middlewares/Third_Party/SubGHz_Phy/stm32_radio_driver/radio_fw.c 

OBJS += \
./Middlewares/Third_Party/SubGHz_Phy/radio.o \
./Middlewares/Third_Party/SubGHz_Phy/radio_driver.o \
./Middlewares/Third_Party/SubGHz_Phy/radio_fw.o 

C_DEPS += \
./Middlewares/Third_Party/SubGHz_Phy/radio.d \
./Middlewares/Third_Party/SubGHz_Phy/radio_driver.d \
./Middlewares/Third_Party/SubGHz_Phy/radio_fw.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Third_Party/SubGHz_Phy/radio.o: /Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/Middlewares/Third_Party/SubGHz_Phy/stm32_radio_driver/radio.c Middlewares/Third_Party/SubGHz_Phy/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DCORE_CM0PLUS -DDEBUG '-DCORE_CM0+' -DSTM32WL55xx -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -c -I../Core/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32WLxx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32WLxx/Include -I../../Drivers/CMSIS/Include -I../LoRaWAN/App -I../LoRaWAN/Target -I../MbMux -I../../Common/MbMux -I../../Utilities/trace/adv_trace -I../../Utilities/misc -I../../Utilities/sequencer -I../../Utilities/timer -I../../Utilities/lpm/tiny_lpm -I../../Middlewares/Third_Party/LoRaWAN/LmHandler/Packages -I../../Middlewares/Third_Party/LoRaWAN/Crypto -I../../Middlewares/Third_Party/LoRaWAN/Mac/Region -I../../Middlewares/Third_Party/LoRaWAN/Mac -I../../Middlewares/Third_Party/LoRaWAN/LmHandler -I../../Middlewares/Third_Party/LoRaWAN/Utilities -I../../Middlewares/Third_Party/SubGHz_Phy -I../../Middlewares/Third_Party/SubGHz_Phy/stm32_radio_driver -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Middlewares/Third_Party/SubGHz_Phy/radio_driver.o: /Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/Middlewares/Third_Party/SubGHz_Phy/stm32_radio_driver/radio_driver.c Middlewares/Third_Party/SubGHz_Phy/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DCORE_CM0PLUS -DDEBUG '-DCORE_CM0+' -DSTM32WL55xx -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -c -I../Core/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32WLxx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32WLxx/Include -I../../Drivers/CMSIS/Include -I../LoRaWAN/App -I../LoRaWAN/Target -I../MbMux -I../../Common/MbMux -I../../Utilities/trace/adv_trace -I../../Utilities/misc -I../../Utilities/sequencer -I../../Utilities/timer -I../../Utilities/lpm/tiny_lpm -I../../Middlewares/Third_Party/LoRaWAN/LmHandler/Packages -I../../Middlewares/Third_Party/LoRaWAN/Crypto -I../../Middlewares/Third_Party/LoRaWAN/Mac/Region -I../../Middlewares/Third_Party/LoRaWAN/Mac -I../../Middlewares/Third_Party/LoRaWAN/LmHandler -I../../Middlewares/Third_Party/LoRaWAN/Utilities -I../../Middlewares/Third_Party/SubGHz_Phy -I../../Middlewares/Third_Party/SubGHz_Phy/stm32_radio_driver -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"
Middlewares/Third_Party/SubGHz_Phy/radio_fw.o: /Users/eduardogonzalezsalgado/STM32CubeIDE/workspace_tfm/TFM/Middlewares/Third_Party/SubGHz_Phy/stm32_radio_driver/radio_fw.c Middlewares/Third_Party/SubGHz_Phy/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DCORE_CM0PLUS -DDEBUG '-DCORE_CM0+' -DSTM32WL55xx -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -c -I../Core/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32WLxx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32WLxx/Include -I../../Drivers/CMSIS/Include -I../LoRaWAN/App -I../LoRaWAN/Target -I../MbMux -I../../Common/MbMux -I../../Utilities/trace/adv_trace -I../../Utilities/misc -I../../Utilities/sequencer -I../../Utilities/timer -I../../Utilities/lpm/tiny_lpm -I../../Middlewares/Third_Party/LoRaWAN/LmHandler/Packages -I../../Middlewares/Third_Party/LoRaWAN/Crypto -I../../Middlewares/Third_Party/LoRaWAN/Mac/Region -I../../Middlewares/Third_Party/LoRaWAN/Mac -I../../Middlewares/Third_Party/LoRaWAN/LmHandler -I../../Middlewares/Third_Party/LoRaWAN/Utilities -I../../Middlewares/Third_Party/SubGHz_Phy -I../../Middlewares/Third_Party/SubGHz_Phy/stm32_radio_driver -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Middlewares-2f-Third_Party-2f-SubGHz_Phy

clean-Middlewares-2f-Third_Party-2f-SubGHz_Phy:
	-$(RM) ./Middlewares/Third_Party/SubGHz_Phy/radio.cyclo ./Middlewares/Third_Party/SubGHz_Phy/radio.d ./Middlewares/Third_Party/SubGHz_Phy/radio.o ./Middlewares/Third_Party/SubGHz_Phy/radio.su ./Middlewares/Third_Party/SubGHz_Phy/radio_driver.cyclo ./Middlewares/Third_Party/SubGHz_Phy/radio_driver.d ./Middlewares/Third_Party/SubGHz_Phy/radio_driver.o ./Middlewares/Third_Party/SubGHz_Phy/radio_driver.su ./Middlewares/Third_Party/SubGHz_Phy/radio_fw.cyclo ./Middlewares/Third_Party/SubGHz_Phy/radio_fw.d ./Middlewares/Third_Party/SubGHz_Phy/radio_fw.o ./Middlewares/Third_Party/SubGHz_Phy/radio_fw.su

.PHONY: clean-Middlewares-2f-Third_Party-2f-SubGHz_Phy

