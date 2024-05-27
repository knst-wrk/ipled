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

/** User interface.
The external digital input is debounced using an analoc RC lowpass and the
Schmitt trigger of the pin peripheral.
*/

#include <stdint.h>
#include <stdbool.h>

#include "cmsis/stm32f10x.h"

#include "timeout.h"

#include "ui.h"

static timeout_t timeout;
static uint8_t hex;

static uint8_t decode_hex(void)
{
    const uint32_t b = ~GPIOB->IDR;
    const uint32_t c = ~GPIOC->IDR;
    return
        ((c & GPIO_IDR_IDR14) >> (14 - 0)) |
        ((c & GPIO_IDR_IDR13) >> (13 - 1)) |
        ((c & GPIO_IDR_IDR15) >> (15 - 2)) |
        ((b & GPIO_IDR_IDR9) >> (9 - 3));
}

char ui_hex(void)
{
    if (hex < 10)
        return '0' + hex;
    else
        return 'A' + hex - 10;
}

bool ui_input(void)
{
    /* Debounced in hardware */
    return !(GPIOA->IDR & GPIO_IDR_IDR11);
}

bool ui_debounce(void)
{
    static uint8_t hex0 = 0;
    static uint8_t debounce = 0;
    if (tot_expired(timeout)) {
        timeout = tot_set(UI_DEBOUNCE_TIMEOUT);

        uint8_t h = decode_hex();
        if (h != hex0) {
            hex0 = h;
            debounce = 0;
        }
        else if (debounce < UI_DEBOUNCE_DEPTH) {
            debounce++;
        }
        else {
            hex = hex0;
        }
    }

    return (hex == hex0) && (debounce == UI_DEBOUNCE_DEPTH);
}

void ui_led(bool led)
{
    if (led)
        GPIOA->BSRR = GPIO_BSRR_BS3;
    else
        GPIOA->BSRR = GPIO_BSRR_BR3;
}

void ui_prepare(void)
{
    /* Port */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    __DSB();
    GPIOA->CRH &= ~(GPIO_CRH_MODE11 | GPIO_CRH_CNF11_0);    /* /input */
    GPIOA->CRH |= GPIO_CRH_CNF11_1;
    GPIOA->ODR |= GPIO_ODR_ODR11;

    GPIOA->CRL &= ~(GPIO_CRL_CNF3 | GPIO_CRL_MODE3_0);      /* LED */
    GPIOA->CRL |= GPIO_CRL_MODE3_1;
    GPIOA->BRR = GPIO_BRR_BR3;

    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    __DSB();
    GPIOB->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9_0);      /* code b3 */
    GPIOB->CRH |= GPIO_CRH_CNF9_1;
    GPIOB->ODR |= GPIO_ODR_ODR9;

    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(                                        /* code b0 .. b2 */
        GPIO_CRH_MODE13 | GPIO_CRH_CNF13_0 |
        GPIO_CRH_MODE14 | GPIO_CRH_CNF14_0 |
        GPIO_CRH_MODE15 | GPIO_CRH_CNF15_0);
    GPIOC->CRH |=
        GPIO_CRH_CNF13_1 |
        GPIO_CRH_CNF14_1 |
        GPIO_CRH_CNF15_1;
    GPIOC->ODR |=
        GPIO_ODR_ODR13 |
        GPIO_ODR_ODR14 |
        GPIO_ODR_ODR15;

    timeout = tot_set(0);
    while (!ui_debounce());
}
