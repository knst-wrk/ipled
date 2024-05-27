/** This file is part of ipled - a versatile LED strip controller.
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

#ifndef MAIN_H
#define MAIN_H

/** Ressouce usage

    GPIOA
        Pin 1       Vbat
        Pin 2       Vled

        Pin 3       LED

        Pin 4       RFIO chip interface (SPI1 NSS)
        Pin 5       RFIO chip interface (SPI1 SCK)
        Pin 6       RFIO chip interface (SPI1 MISO)
        Pin 7       RFIO chip interface (SPI1 MOSI)

        Pin 8       SD card power enable

        Pin 9       DMX (USART1 TxD)
        Pin 10      DMX (USART1 RxD)

        Pin 11      /Input

        Pin 12      DMX (TXEN)

    GPIOB
        Pin 0       RFIO DIO1
        Pin 1       RFIO DIO0

        Pin 2       LED pixel output
        Pin 3       LED pixel output
        Pin 4       LED pixel output
        Pin 5       LED pixel output
        Pin 6       LED pixel output
        Pin 7       LED pixel output

        Pin 8       LED power enable

        Pin 9       Hex switch (B3)

        Pin 12      SD card interface (SPI1 NSS)
        Pin 13      SD card interface (SPI1 SCK)
        Pin 14      SD card interface (SPI1 MISO)
        Pin 15      SD card interface (SPI1 MOSI)

    GPIOC
        Pin 13      Hex switch (B1)
        Pin 14      Hex switch (B0)
        Pin 15      Hex switch (B2)

    DMA1
        Channel 4   SD card RX
        Channel 5   SD card TX
        Channel 2   LED pixel timing
        Channel 3   LED pixel timing
        Channel 6   LED pixel timing

    TIM3            LED pixel timing
    TIM4            LED pixel frame rate generator

    SPI1            RFIO chip interface

    SPI2            SD card interface

    USART1          DMX, Direct TPM2

    AD              Voltage monitoring
        IN1         Vbat
        IN2         Vled
        IN16        temperature sensor

    RTC             Wakeup event generation

    SysTick         Timeout generator

*/

/** Memory consumption.



*/

#endif
