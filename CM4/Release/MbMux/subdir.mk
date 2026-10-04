################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../MbMux/LmHandler_mbwrapper.c \
../MbMux/mbmux.c \
../MbMux/mbmuxif_lora.c \
../MbMux/mbmuxif_radio.c \
../MbMux/mbmuxif_sys.c \
../MbMux/mbmuxif_trace.c \
../MbMux/radio_mbwrapper.c 

OBJS += \
./MbMux/LmHandler_mbwrapper.o \
./MbMux/mbmux.o \
./MbMux/mbmuxif_lora.o \
./MbMux/mbmuxif_radio.o \
./MbMux/mbmuxif_sys.o \
./MbMux/mbmuxif_trace.o \
./MbMux/radio_mbwrapper.o 

C_DEPS += \
./MbMux/LmHandler_mbwrapper.d \
./MbMux/mbmux.d \
./MbMux/mbmuxif_lora.d \
./MbMux/mbmuxif_radio.d \
./MbMux/mbmuxif_sys.d \
./MbMux/mbmuxif_trace.d \
./MbMux/radio_mbwrapper.d 


# Each subdirectory must supply rules for building sources it contributes
MbMux/%.o MbMux/%.su MbMux/%.cyclo: ../MbMux/%.c MbMux/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -DCORE_CM4 -DUSE_HAL_DRIVER -DSTM32WL55xx -DUSE_NUCLEO_64 -c -I../Core/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc -I../../Drivers/STM32WLxx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32WLxx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32WLxx/Include -I../../Drivers/CMSIS/Include -I../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../LoRaWAN/App -I../LoRaWAN/Target -I../MbMux -I../../Common/MbMux -I../../Utilities/trace/adv_trace -I../../Utilities/misc -I../../Utilities/timer -I../../Utilities/lpm/tiny_lpm -I../../Middlewares/Third_Party/LoRaWAN/Mac/Region -I../../Middlewares/Third_Party/LoRaWAN/Mac -I../../Middlewares/Third_Party/LoRaWAN/LmHandler -I../../Middlewares/Third_Party/LoRaWAN/Utilities -I../../Middlewares/Third_Party/SubGHz_Phy -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-MbMux

clean-MbMux:
	-$(RM) ./MbMux/LmHandler_mbwrapper.cyclo ./MbMux/LmHandler_mbwrapper.d ./MbMux/LmHandler_mbwrapper.o ./MbMux/LmHandler_mbwrapper.su ./MbMux/mbmux.cyclo ./MbMux/mbmux.d ./MbMux/mbmux.o ./MbMux/mbmux.su ./MbMux/mbmuxif_lora.cyclo ./MbMux/mbmuxif_lora.d ./MbMux/mbmuxif_lora.o ./MbMux/mbmuxif_lora.su ./MbMux/mbmuxif_radio.cyclo ./MbMux/mbmuxif_radio.d ./MbMux/mbmuxif_radio.o ./MbMux/mbmuxif_radio.su ./MbMux/mbmuxif_sys.cyclo ./MbMux/mbmuxif_sys.d ./MbMux/mbmuxif_sys.o ./MbMux/mbmuxif_sys.su ./MbMux/mbmuxif_trace.cyclo ./MbMux/mbmuxif_trace.d ./MbMux/mbmuxif_trace.o ./MbMux/mbmuxif_trace.su ./MbMux/radio_mbwrapper.cyclo ./MbMux/radio_mbwrapper.d ./MbMux/radio_mbwrapper.o ./MbMux/radio_mbwrapper.su

.PHONY: clean-MbMux

