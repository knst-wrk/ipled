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

/** UART input and output.
*/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "cmsis/stm32f10x.h"

#include "timeout.h"
#include "system.h"

#include "tty.h"

static volatile tty_hook_t hook;

static uint16_t brr(uint32_t baud)
{
    /* The baud rate for USART 1 is derived via a fractional divider from the
    APB2's clock of 72MHz. The required divider is computed as
        R = f_APB2 / (16 * Baud)

    The divider is loaded into the BRR after reformatting it with four bits of
    fraction:
        R = Mantissa + Fraction / 16
        BRR = (Mantissa << 4) | Fraction

    The bit shift complements the division so the BRR is loaded with
        BRR = f_APB2 / Baud

    The actual baud rate after rounding is calculated as
        Baud = f_APB2 / (16 * (Mantissa + Fraction / 16))
    */
    const uint32_t r = 72000000 / baud;
    return r;
}

static void rxen(bool rx)
{
    if (rx)
        USART1->CR1 |= USART_CR1_RE;
    else
        USART1->CR1 &= ~USART_CR1_RE;

    /* Flush */
    while (USART1->SR & USART_SR_RXNE)
        (void) USART1->DR;
}

static void txen(bool tx)
{
    if (tx) {
        USART1->CR1 &= ~USART_CR1_RE;
        GPIOA->BSRR = GPIO_BSRR_BS12;
        tot_delay(1);

        USART1->CR1 |= USART_CR1_TE;
        GPIOA->CRH |= GPIO_CRH_CNF9_1;
    }
    else {
        /* Wait for transmisstion to complete */
        while (!(USART1->SR & USART_SR_TC));
        GPIOA->CRH &= ~GPIO_CRH_CNF9_1;
        USART1->CR1 &= ~USART_CR1_TE;
        tot_delay(1);

        GPIOA->BSRR = GPIO_BSRR_BR12;

        rxen(true);
    }
}


static void sentinel(uint32_t status, uint8_t ch)
{
    (void) status;
    (void) ch;
    /* vakat */
}

void USART1_IRQHandler(void) __USED;
void USART1_IRQHandler(void)
{
    /* Clear status flags and interrupt request */
    const uint32_t status = USART1->SR;
    const uint8_t ch = USART1->DR;
    (*hook)(status, ch);
}

void tty_baud(uint32_t baud)
{
    USART1->CR1 &= ~USART_CR1_UE;
    while ( (USART1->SR & (USART_SR_TXE | USART_SR_TC)) != (USART_SR_TXE | USART_SR_TC) );
    USART1->BRR = brr(baud);
    USART1->CR1 |= USART_CR1_UE;
}

void tty_hook(tty_hook_t h)
{
    if (h) {
        NVIC_DisableIRQ(USART1_IRQn);
        hook = h;
        __DSB();
        NVIC_EnableIRQ(USART1_IRQn);
    }
    else {
        NVIC_DisableIRQ(USART1_IRQn);
        hook = sentinel;
        __DSB();
    }
}

void tty_enable(bool enable)
{
    txen(false);
    if (enable) {
        /* Enable voltage regulator */
        sys_vcc(VCC_TTY, 0);
        tot_delay(1);

        rxen(true);
    }
    else {
        rxen(false);

        /* Disable voltage regulator */
        sys_vcc(0, VCC_TTY);
    }
}

void tty_puts(const char *buf)
{
    txen(true);
    while (*buf) {
        while (!(USART1->SR & USART_SR_TXE));
        USART1->DR = *buf++;
    }
    txen(false);
}

void tty_putchar(char c)
{
    txen(true);
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = c;
    txen(false);
}

void tty_prepare(void)
{
    /* Port */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    __DSB();
    GPIOA->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10_0);    /* RxD */
    GPIOA->CRH |= GPIO_CRH_CNF10_1;
    GPIOA->ODR |= GPIO_ODR_ODR10;

    GPIOA->CRH &= ~(GPIO_CRH_CNF12 | GPIO_CRH_MODE12_0);    /* TXEN */
    GPIOA->CRH |= GPIO_CRH_MODE12_1;
    GPIOA->BRR = GPIO_BRR_BR12;

    GPIOA->CRH &= ~(GPIO_CRH_CNF9_0 | GPIO_CRH_MODE9_0);    /* TxD */
    GPIOA->CRH |= GPIO_CRH_CNF9_1 | GPIO_CRH_MODE9_1;
    GPIOA->ODR |= GPIO_ODR_ODR9;

    /* USART */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    __DSB();
    NVIC_DisableIRQ(USART1_IRQn);
    USART1->CR1 =
        USART_CR1_UE |
        USART_CR1_RXNEIE;
    USART1->CR2 = 0;
    USART1->CR3 = 0;
    USART1->GTPR = 0;
    USART1->BRR = brr(9600);

    hook = &sentinel;
}

