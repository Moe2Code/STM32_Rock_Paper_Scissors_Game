################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/it.c \
../Src/main_.c \
../Src/msp.c \
../Src/syscalls.c \
../Src/system_stm32f4xx.c 

OBJS += \
./Src/it.o \
./Src/main_.o \
./Src/msp.o \
./Src/syscalls.o \
./Src/system_stm32f4xx.o 

C_DEPS += \
./Src/it.d \
./Src/main_.d \
./Src/msp.d \
./Src/syscalls.d \
./Src/system_stm32f4xx.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o: ../Src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU GCC Compiler'
	@echo $(PWD)
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 '-D__weak=__attribute__((weak))' '-D__packed=__attribute__((__packed__))' -DUSE_HAL_DRIVER -DSTM32F407xx -I"C:/Users/426180/Documents/Courses/Udemy_MBED2/STM32SWB_Workspace/Disc_F407VG/Two_Boards_Game/Inc" -I"C:/Users/426180/Documents/Courses/Udemy_MBED2/STM32SWB_Workspace/Disc_F407VG/Two_Boards_Game/Drivers/STM32F4xx_HAL_Driver/Inc" -I"C:/Users/426180/Documents/Courses/Udemy_MBED2/STM32SWB_Workspace/Disc_F407VG/Two_Boards_Game/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy" -I"C:/Users/426180/Documents/Courses/Udemy_MBED2/STM32SWB_Workspace/Disc_F407VG/Two_Boards_Game/Drivers/CMSIS/Device/ST/STM32F4xx/Include" -I"C:/Users/426180/Documents/Courses/Udemy_MBED2/STM32SWB_Workspace/Disc_F407VG/Two_Boards_Game/Drivers/CMSIS/Include"  -Og -g3 -Wall -fmessage-length=0 -ffunction-sections -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


