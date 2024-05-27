/** This file is part of ipled - a versatile LED stripe controller.
Copyright (C) 2024 Sven Pauli <sven@knst-wrk.de>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

.syntax unified
.cpu cortex-m3
.fpu softvfp
.thumb

.equ  BootRAM, 0xF108F85F


/* Entry point.
*/
.section .text
.global _start
.weak _start
.type _start, %function
_start:
        /* Setup stack pointer */
        ldr     sp, =_estack


        /* Relocate .data segments */
        ldr     r5, =__data_regions_array_start
        ldr     r6, =__data_regions_array_end
        b       2f

    1:
        ldr     r2, [r5], #4                            // Load source pointer
        ldr     r3, [r5], #4                            // Load destination start pointer
        ldr     r4, [r5], #4                            // Load destination end pointer

    11:
        ldr     r0, [r2], #4                            // Load data byte from flash
        str     r0, [r3], #4                            // Store data byte to RAM
    22:
        cmp     r3, r4
        blo     11b

    2:
        cmp     r5, r6
        blo     1b


        /* Clear .bss segments */
        ldr     r5, =__bss_regions_array_start
        ldr     r6, =__bss_regions_array_end
        mov     r0, #0
        b       2f

    1:
        ldr     r3, [r5], #4                            // Load destination start pointer
        ldr     r4, [r5], #4                            // Load destination end pointer

    11:
        str     r0, [r3], #4                            // Store zero to RAM
    22:
        cmp     r3, r4
        blo     11b

    2:
        cmp     r5, r6
        blo     1b



        /* Execute preinit functions */
        ldr     r5, =__preinit_array_start
        ldr     r6, =__preinit_array_end
        b       2f

    1:
        ldr     r0, [r5], #4                            // Load function pointer
        orr     r0, r0, #1
        blx     r0                                      // Call function
    2:
        cmp     r5, r6
        blo     1b


        /* Execute init functions */
        ldr     r5, =__init_array_start
        ldr     r6, =__init_array_end
        b       2f

    1:
        ldr     r0, [r5], #4                            // Load function pointer
        orr     r0, r0, #1
        blx     r0                                      // Call function
    2:
        cmp     r5, r6
        blo     1b

        /* Execute _init function */
        bl      _init

        /* Branch to application */
        b       main

    1:
        b       1b

.size _start, . - _start


/* Reset interrupt handler.
*/
.section .text
.global Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
        ldr     r0, =_start
        bx      r0

.size Reset_Handler, . - Reset_Handler


/* Default interrupt handler.
*/
.section .text
.global Default_Handler
.weak Default_Handler
.type Default_Handler, %function
Default_Handler:
0:
        bkpt
        b       0b

.size Default_Handler, . - Default_Handler


/* Interrupt vector.
*/
.macro isr name
.word \name
.ifnc \name, 0
.weak \name
.thumb_set \name, Default_Handler
.endif
.endm

.section .isr_vector, "a", %progbits
.global isr_vector
.type isr_vector, %object
isr_vector:

.word _estack

/* Core exceptions and interrupts */
.word Reset_Handler
isr NMI_Handler
isr HardFault_Handler
isr MemManage_Handler
isr BusFault_Handler
isr UsageFault_Handler
isr 0
isr 0
isr 0
isr 0
isr SVC_Handler
isr DebugMon_Handler
isr 0
isr PendSV_Handler
isr SysTick_Handler

/* Peripheral interrupts */
isr WWDG_IRQHandler
isr PVD_IRQHandler
isr TAMPER_IRQHandler
isr RTC_IRQHandler
isr FLASH_IRQHandler
isr RCC_IRQHandler
isr EXTI0_IRQHandler
isr EXTI1_IRQHandler
isr EXTI2_IRQHandler
isr EXTI3_IRQHandler
isr EXTI4_IRQHandler
isr DMA1_Channel1_IRQHandler
isr DMA1_Channel2_IRQHandler
isr DMA1_Channel3_IRQHandler
isr DMA1_Channel4_IRQHandler
isr DMA1_Channel5_IRQHandler
isr DMA1_Channel6_IRQHandler
isr DMA1_Channel7_IRQHandler
isr ADC1_2_IRQHandler
isr USB_HP_CAN1_TX_IRQHandler
isr USB_LP_CAN1_RX0_IRQHandler
isr CAN1_RX1_IRQHandler
isr CAN1_SCE_IRQHandler
isr EXTI9_5_IRQHandler
isr TIM1_BRK_IRQHandler
isr TIM1_UP_IRQHandler
isr TIM1_TRG_COM_IRQHandler
isr TIM1_CC_IRQHandler
isr TIM2_IRQHandler
isr TIM3_IRQHandler
isr TIM4_IRQHandler
isr I2C1_EV_IRQHandler
isr I2C1_ER_IRQHandler
isr I2C2_EV_IRQHandler
isr I2C2_ER_IRQHandler
isr SPI1_IRQHandler
isr SPI2_IRQHandler
isr USART1_IRQHandler
isr USART2_IRQHandler
isr USART3_IRQHandler
isr EXTI15_10_IRQHandler
isr RTCAlarm_IRQHandler
isr USBWakeUp_IRQHandler
isr 0
isr 0
isr 0
isr 0
isr 0
isr 0
isr 0

.word   BootRAM

.size isr_vector, . - isr_vector

