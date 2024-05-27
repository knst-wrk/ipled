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

/** System.
After reset the CPU is clocked by the HSI. If the RFIO chip has been reset too
there will be a 1MHz clock available from the HSE. If the RFIO has retained its
configuration the HSE will deliver 8MHz. If only the RFIO has been reset its
clock divider must be reconfigured.

Clock tree:
    - HSI @ 8MHz
    - HSE @ 8MHz after configuration of RFIO
    |--- SYSCLK @ 72MHz via PLL*9/1
       |--- AHB @ 72MHz
          |--- HCLK for bus, core and memory
          |--- APB1 @ 36MHz
          |  |-- PCLK1 @ 36MHz to peripherals
          |  |-- Timers 1..7&12..14 @ 72MHz via PLL*2
          |--- APB2 @ 72MHz
              |-- PCLK2 @ 72MHz to peripherals
              |-- Timers 2&8..11 @ 72MHz
              |-- ADCCLK @ 9MHz via prescaler/8

NOTE A silicon defect may cause a corrupt instruction fetch from flash memory
    when the FLITF is configured with two wait states and prefetch enabled and
    the FLITF clock is halted when entering SLEEP mode.
    This is the currently used FLITF configuration. STOP mode is thus used.

NOTE Due to a silicon defect an instruction following WFE may be missed after
    wakeup. A workaround using sacrificial NOPs is used in place.
*/

#include <stdint.h>
#include <stdio.h>

#include "cmsis/stm32f10x.h"

#include "dld/ld.h"

#include "system.h"


#ifdef DEBUG
/* Via rdimon.spec */
extern void initialise_monitor_handles(void);
#endif

static uint8_t vccclients;

void sys_hsi(void)
{
    /* The HSI oscillator provides 8MHz and its clock output is fed into the
    clock tree via the system clock switch *after* the PLL. This setup thus is
    always compatible with the rest of the clock tree. */

    /* Start HSI */
    RCC->CIR = 0x00000000;
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY));

    /* Switch to HSI */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
    while ( (RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI );
    RCC->CFGR = 0x00000000;
    SystemCoreClock = 8000000UL;

    /* Disable CSS and PLL */
    RCC->CR &= ~(RCC_CR_CSSON | RCC_CR_PLLON);
    RCC->CIR = RCC_CIR_CSSC;
}

void sys_hse(void)
{
    /* Enable bypassed HSE */
    RCC->CR &= ~(RCC_CR_CSSON | RCC_CR_HSEON);
    while (RCC->CR & RCC_CR_HSERDY);
    RCC->CR |= RCC_CR_HSEBYP | RCC_CR_HSEON;
    while ( !(RCC->CR & RCC_CR_HSERDY) );

    /* Enable CSS */
   // RCC->CR |= RCC_CR_CSSON;

    /* Insert wait states for flash memory acces and enable prefetch buffer */
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;
    while ( !(FLASH->ACR & FLASH_ACR_PRFTBS) );

    /* Set up PLL for HSE*9 and adjust bus clock dividers for 72MHz SYSCLK */
    RCC->CR &= ~RCC_CR_PLLON;
    RCC->CFGR =
        RCC_CFGR_PLLSRC |
        RCC_CFGR_PLLMULL9 |
        RCC_CFGR_ADCPRE_DIV8 |
        RCC_CFGR_PPRE2_DIV1 |
        RCC_CFGR_PPRE1_DIV2 |
        RCC_CFGR_HPRE_DIV1 |
        RCC_CFGR_SW_HSI;

    /* Enable PLL */
    RCC->CR |= RCC_CR_PLLON;
    while ( !(RCC->CR & RCC_CR_PLLRDY) );

    /* Switch to PLL */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ( (RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL );
    SystemCoreClock = 72000000UL;

    /* Disable now unused HSI oscillator */
    RCC->CR &= ~RCC_CR_HSION;
}

void sys_vcc(uint8_t on, uint8_t off)
{
    vccclients |= on;
    vccclients &= ~off;
    if (vccclients)
        GPIOB->BSRR = GPIO_BSRR_BR8;
    else
        GPIOB->BSRR = GPIO_BSRR_BS8;
}

/* Workarounds according to erratum, which recommends a function consisting
only of the WFE and a NOP:

    __asm void _WFE(void) {
        WFE
        NOP
        BX lr
    }

This is adapted in a more portable way below. */
static inline void wfe(void)
{
    __ASM volatile(
        "\n\t       nop"
        "\n\t       bl      1f"
        "\n\t       b       2f"
        "\n\t       nop"
        "\n\t"
        "\n\t 1:    wfe"
        "\n\t       nop"
        "\n\t       bx      lr"
        "\n\t"
        "\n\t 2:    nop"
        :
        :
        : "cc", "lr"
    );
}

static inline void wfi(void)
{
    __ASM volatile(
        "\n\t       nop"
        "\n\t       bl      1f"
        "\n\t       b       2f"
        "\n\t       nop"
        "\n\t"
        "\n\t 1:    wfi"
        "\n\t       nop"
        "\n\t       bx      lr"
        "\n\t"
        "\n\t 2:    nop"
        :
        :
        : "cc", "lr"
    );
}

void sys_stop(void)
{
    /* Disable SysTick interrupt as at this point a remaining couple of clocks
    may be generated by the RFIO that could increase the SysTick counter until
    a wake up event is generated. */
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

    SCB->SCR = SCB_SCR_SLEEPDEEP_Msk;
    wfe();

    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
}

void sys_alarm(uint32_t dt)
{
    if (dt) {
        PWR->CR |= PWR_CR_DBP;
        while (!(RTC->CRL & RTC_CRL_RTOFF));
        RTC->CRL = RTC_CRL_CNF;

        while (!(RTC->CRL & RTC_CRL_RSF));
        uint32_t cnt = (RTC->CNTH << 16) | RTC->CNTL;

        cnt += dt;
        RTC->ALRH = ((cnt >> 16) & 0xFFFF);
        RTC->ALRL = ((cnt >>  0) & 0xFFFF);

        /* CRL contains ALRF, clear it */
        RTC->CRL = 0;
        EXTI->EMR |= EXTI_EMR_MR17;
        while (!(RTC->CRL & RTC_CRL_RTOFF));
        PWR->CR &= ~PWR_CR_DBP;
    }
    else {
        EXTI->EMR &= ~EXTI_EMR_MR17;
    }
}

uint32_t sys_time(void)
{
    PWR->CR |= PWR_CR_DBP;
    RTC->CRL = 0;

    while (!(RTC->CRL & RTC_CRL_RSF));
    uint32_t cnt = (RTC->CNTH << 16) | RTC->CNTL;

    PWR->CR &= ~PWR_CR_DBP;
    return cnt;
}


static inline void *sp(void)
{
    void *p;
    __ASM volatile (
        "\n\t       mov     %0, sp"
        : "=r" (p)
    );

    return p;
}

void sys_stat(void)
{
    /* Memory layout:

        20K +-----------+   <-- _estack
            |   stack   |
            |...........|   <-- SP
            |           |
            |           |
            |           |
            |           |
            +-----------+   <-- end
            |  .noinit  |
            +-----------+
            |   .bss    |
            +-----------+
            |           |
            |   .data   |
            |           |
        0x0 +-----------+
    */
    unsigned total = (unsigned) &_estack - (unsigned) &end;
    unsigned used = (unsigned) &_estack - (unsigned) sp();
}

uint32_t sys_uid(void)
{
    uint8_t data[] = {
        U_ID->U_ID0  >>  0,
        U_ID->U_ID0  >>  8,
        U_ID->U_ID1  >>  0,
        U_ID->U_ID1  >>  8,
        U_ID->U_ID23 >>  0,
        U_ID->U_ID23 >>  8,
        U_ID->U_ID23 >> 16,
        U_ID->U_ID23 >> 24,
        U_ID->U_ID45 >>  0,
        U_ID->U_ID45 >>  8,
        U_ID->U_ID45 >> 16,
        U_ID->U_ID45 >> 24
    };

    /* Jenkins hash */
    uint32_t uid = 0;
    for (size_t i = 0; i < sizeof(data)/sizeof(*data); i++) {
        uid += data[i++];
        uid += uid << 10;
        uid ^= uid >> 6;
    }

    uid += uid << 3;
    uid ^= uid >> 11;
    uid += uid << 15;
    return uid;
}


void panic(void)
{
    /* Port */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    __DSB();

    GPIOA->CRL &= ~(GPIO_CRL_CNF3 | GPIO_CRL_MODE3_0);      /* LED */
    GPIOA->CRL |= GPIO_CRL_MODE3_1;
    GPIOA->BRR = GPIO_BRR_BR3;

    uint32_t pattern = 0x05477715;
    for (;;) {
        if (pattern & 1)
            GPIOA->BSRR = GPIO_BSRR_BS3;
        else
            GPIOA->BSRR = GPIO_BSRR_BR3;

        pattern = (pattern >> 1) | (pattern << 31);

        /* Best guess for a delay */
        for (uint32_t d = SystemCoreClock / 2 / 16; d; d--)
            __NOP();
    }
}


void sys_prepare(void)
{
    /* Switch to HSI.
    HSI running at 8MHz is compatible with all the peripherals, buses and
    clock subsystems. Also it will work with either prefetch buffer enabled
    or disabled and with any number of waitstates. */
    sys_hsi();

    /* 5V power supply is shared along LEDs and RS485 transceiver */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    __DSB();
    GPIOB->ODR |= GPIO_ODR_ODR8;                        /* /LEDEN */
    GPIOB->CRH &= ~(GPIO_CRH_CNF8 | GPIO_CRH_MODE8_0);
    GPIOB->CRH |= GPIO_CRH_MODE8_1;
    vccclients = 0;

    /* Prepare for STOP */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    __DSB();
    PWR->CR = PWR_CR_LPDS;

    /* Start LSI */
    RCC->CSR = RCC_CSR_LSION;
    while (!(RCC->CSR & RCC_CSR_LSIRDY));

    /* Start RTC in event mode at TR_CLK = 1kHz */
    NVIC_DisableIRQ(RTC_IRQn);
    PWR->CR |= PWR_CR_DBP;
    RCC->BDCR = RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL_LSI;
    EXTI->RTSR |= EXTI_RTSR_TR17;
    EXTI->FTSR &= ~EXTI_FTSR_TR17;
    EXTI->IMR &= ~EXTI_IMR_MR17;
    EXTI->EMR &= ~EXTI_EMR_MR17;

    while (!(RTC->CRL & RTC_CRL_RSF));
    while (!(RTC->CRL & RTC_CRL_RTOFF));
    RTC->CRL = RTC_CRL_CNF;

    RTC->PRLH = ((RTC_RCOSC / 1000) >> 16) & 0xFFFF;
    RTC->PRLL = ((RTC_RCOSC / 1000) >>  0) & 0xFFFF;

    RTC->CRL = 0;
    while (!(RTC->CRL & RTC_CRL_RTOFF));
    PWR->CR &= ~PWR_CR_DBP;


#ifdef DEBUG
    /* Preserve clocks in stop/sleep modes */
    DBGMCU->CR |= DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_SLEEP;

    /* Semihosted debugging (RDI) */
    initialise_monitor_handles();
#endif
}
