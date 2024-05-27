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

/** Pixel interface.
Pixel timing is generated simultaneouly for eight strings of LEDs using Timer 3
and DMA1 channels 6, 3 and 2. When enabled the timer generates the following
events that issue DMA requests:

    Update event        -> DMA1 channel 3
    Compare 3 match     -> DMA1 channel 2
    Compare 1 match     -> DMA1 channel 6

DMA1 channel 3 is configured to write the pattern 0xFF to the BSRR of the output
port, hence raising all the outputs to a high level. This is the start of the
bits. The compare 3 match writes values from the 'bits' array to the BRR of the
port, lowering the outputs to the low level again. The match is set up to
trigger after Tlow. The Compare 1 match finally writes the pattern 0xFF to the
BRR. It triggers at Thigh and terminates the '1' bit in case it has not been
terminated by compare match 3 already.
Setting a bit in the 'bits' array thus causes the output to be lowered after
Tlow by the write to the BRR, generating a '0' bit, while clearing a bit will
generate a '1' bit.
After the DMA complete timer 3 is reconfigured in one pulse mode to expire after
the reset duration.

Timing for WS2812B:

                Thigh       Tlow        Tbit
    '0' bit     350ns       800ns       1,250ns
    '1' bit     700ns       600ns       1,300ns
    Reset                              50,000ns

    Thigh, Tlow +- 150ns

Timing for SK6812:
                Thigh       Tlow        Tbit
    '0' bit     300ns       900ns       1,200ns
    '1' bit     600ns       600ns       1,200ns
    Reset                              80,000ns

    Thigh, Tlow +- 150ns

Performance thus is about 3*8*1200ns = 30us per LED plus another 80us for the
reset. 24 bytes of RAM are needed for every set of LEDs (3 colors with 8 bits
each, 8 LEDs in parallel). 512 LEDs per string then add up to about 13k of RAM.

The used Timer 3 instance is clocked from the APB1 bus clock.
The APB1 bus clock is limited to 36MHz so if the system clock is 72MHz the bus
clock is derived from the system clock by prescaling it by two. The timer clock
is restored to 72MHz again in this case by an intermediate frequency doubler.

Running at 72MHz gives a period of T = 13.888..ns.
*/

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "cmsis/stm32f10x.h"

#include "system.h"
#include "config.h"
#include "buffer.h"
#include "timeout.h"

#include "leds.h"

#if (MAXLEDS < 0) || (MAXLEDS > 512)
#   error Invalid LED count
#endif

#if MAXBUFF < 1
#   error Invalid buffer size
#endif

/* Timing in nanoseconds */
#define T_1                    750
#define T_0                    300
#define T_BIT                 1400
#define T_RESET             100000

/* Enable startup delay in milliseconds */
#define STARTUP             100

static const uint8_t ones =
            GPIO_BRR_BR2 |
            GPIO_BRR_BR3 |
            GPIO_BRR_BR4 |
            GPIO_BRR_BR5 |
            GPIO_BRR_BR6 |
            GPIO_BRR_BR7;

#define MAXBITS             ((MAXLEDS) * 3 * 8)
static uint16_t nbits = MAXBITS;

/* Alignment required for uint32_t aliasing */
static uint8_t bits[MAXBITS] __ALIGNED(4);
static uint8_t sred, sgreen, sblue;

volatile bool capture;

static uint32_t trr(uint32_t nsecs)
{
    /* Timer reload value.
    The timer reload value is computed as follows:
        n = f * T

    with
        n in timer ticks,
        f being the timer clock in Hertz and
        T being the desired duration in seconds.

    To stay within the integer's range the equation is scaled by 1/1000 which
    gives
        n = f * T / 1e3 * 1e3

    Converting T to nanoseconds gives
        T = T' / 1e9
        n = f * T' / 1e9 / 1e3 * 1e3

    Rearranging finally yields
        n = f / 1e3 * T' / 1e6

    With the timer's 72MHz APB1 clock and 16 bit counter register the maximum
    delay is almost one millisecond. */
    return
        SystemCoreClock
        / UINT32_C(1000)
        * nsecs
        / UINT32_C(1000000);
}


void TIM4_IRQHandler(void) __USED;
void TIM4_IRQHandler(void)
{
    /* Test DMA EN in case the interrupt has temporarily disabled TIM3 */
    if ( (TIM3->CR1 & TIM_CR1_CEN) || (DMA1_Channel6->CCR & DMA_CCR6_EN) ) {
        /* Skip frame if DMA has not finished yet */
        TIM4->SR = ~TIM_SR_UIF;
        return;
    }

    if (TIM4->CR1 & TIM_CR1_OPM) {
        TIM4->CR1 &= ~TIM_CR1_OPM;
        if (TIM4->ARR > 0) {
            /* Start frame rate generator if framerate requested */
            TIM4->CR1 |= TIM_CR1_CEN;
        }
        else {
            /* Disable frame rate generator if single default frame */
            TIM4->CR1 &= ~TIM_CR1_CEN;
            NVIC_DisableIRQ(TIM4_IRQn);
        }
    }

    led_universe();
    TIM4->SR = ~TIM_SR_UIF;
}

void DMA1_Channel6_IRQHandler(void) __USED;
void DMA1_Channel6_IRQHandler(void)
{
    /* Preset timer for reset duration */
    TIM3->CR1 &= ~TIM_CR1_CEN;
    TIM3->ARR = trr(T_RESET);
    TIM3->CNT = 0;
    TIM3->CR1 |= TIM_CR1_OPM | TIM_CR1_CEN;

    /* Disable DMA and interrupt */
    DMA1_Channel6->CCR &= ~(DMA_CCR6_EN | DMA_CCR6_TCIE);
    DMA1_Channel3->CCR &= ~DMA_CCR3_EN;
    DMA1_Channel2->CCR &= ~DMA_CCR2_EN;
}


void led_universe(void)
{
    if (!NVIC_GetEnableIRQ(DMA1_Channel6_IRQn)) {
        /* Prevent parasitical currents when 5V power is disabled */
        capture = false;
        return;
    }

    /* Halt timer and DMA */
    NVIC_DisableIRQ(DMA1_Channel6_IRQn);
    TIM3->CR1 &= ~(TIM_CR1_OPM | TIM_CR1_CEN);
    DMA1_Channel6->CCR &= ~(DMA_CCR6_EN | DMA_CCR6_TCIE);
    DMA1_Channel3->CCR &= ~DMA_CCR3_EN;
    DMA1_Channel2->CCR &= ~DMA_CCR2_EN;
    __DSB();

    /* Clear pending DMA requests and reset timer */
    TIM3->DIER = 0;
    TIM3->ARR = trr(T_BIT);
    TIM3->CNT = TIM3->ARR;
    TIM3->DIER = TIM_DIER_UDE | TIM_DIER_CC3DE | TIM_DIER_CC1DE;

    /* Reset bits */
    GPIOB->BRR = ones;

    /* Restart.
    CMAR/CPAR keep their values, so no need to reset. */
    DMA1->IFCR = DMA_IFCR_CGIF6;
    DMA1_Channel6->CNDTR = nbits;
    DMA1_Channel3->CNDTR = nbits;
    DMA1_Channel2->CNDTR = nbits;
    DMA1_Channel6->CCR |= DMA_CCR6_EN | DMA_CCR6_TCIE;
    DMA1_Channel3->CCR |= DMA_CCR3_EN;
    DMA1_Channel2->CCR |= DMA_CCR2_EN;
    __DSB();

    capture = false;
    TIM3->CR1 |= TIM_CR1_CEN;
    NVIC_EnableIRQ(DMA1_Channel6_IRQn);
}

bool led_busy(void)
{
    return TIM3->CR1 & TIM_CR1_CEN;
}

bool led_capture(void)
{
    /* Start-up delay */
    if (TIM4->CR1 & TIM_CR1_OPM)
        return false;

    if (capture)
        return false;

    /* Inhibit frame generation */
    NVIC_DisableIRQ(TIM4_IRQn);

    /* Wait for DMA to complete */
    if (DMA1_Channel6->CCR & DMA_CCR6_EN)
        return false;

    capture = true;
    return true;
}

void led_release(void)
{
    /* Release frame rate generator */
    NVIC_EnableIRQ(TIM4_IRQn);

    if (capture) {
        /* Immediately generate asynchronous frame */
        if ( !(TIM4->CR1 & TIM_CR1_CEN) )
            led_universe();
    }
}

void led_framerate(uint16_t fps)
{
    /* Stop and inhibit frame rate generator */
    TIM4->CR1 &= ~TIM_CR1_CEN;
    NVIC_DisableIRQ(TIM4_IRQn);

    /* Frame generator.
    TIM4 is a 16 bit wide counter. Its clock must be divided by the prescaler to
    less than 65565Hz to be able to achieve the lowest desired framerate of
    1fps. */
    if (fps == 0) {
        /* Manual triggering via led_universe() */
        TIM4->ARR = 0;
    }
    else {
        if (fps > 50)
            fps = 50;

        /* Timer has auto-preload reload enabled */
        const uint16_t arr = TIM4->ARR;
        TIM4->ARR = UINT32_C(10000) / fps;

        if (NVIC_GetEnableIRQ(DMA1_Channel6_IRQn)) {
            if (TIM4->CR1 & TIM_CR1_OPM) {
                /* Continue with start-up delay */
                const uint16_t remaining = arr - TIM4->CNT;
                TIM4->CNT = TIM4->ARR - remaining;

                /* Re-start frame rate generator for first frame */
                TIM4->CR1 |= TIM_CR1_OPM | TIM_CR1_CEN;
            }
            else {
                /* Continue at frame rate */
                TIM4->CNT = 0;
                TIM4->SR = ~TIM_SR_UIF;
                TIM4->CR1 |= TIM_CR1_CEN;
            }

            /* Release frame rate generator */
            NVIC_EnableIRQ(TIM4_IRQn);
        }

    }
}

void led_enable(bool enable)
{
    /* Inhibit and stop frame rate generator */
    NVIC_DisableIRQ(TIM4_IRQn);
    TIM4->CR1 &= ~(TIM_CR1_CEN | TIM_CR1_OPM);

    if (enable) {
        /* Wait for last universe to be finished */
        NVIC_EnableIRQ(DMA1_Channel6_IRQn);
        while (led_busy());

        /* Enable voltage regulator */
        sys_vcc(VCC_LED, 0);

        /* Clear LEDs */
        led_clear();

        /* Generate first frame after start-up delay */
        TIM4->CNT = TIM4->ARR - (STARTUP * 10);

        /* Start frame rate generator for first frame */
        TIM4->SR = ~TIM_SR_UIF;
        TIM4->CR1 |= TIM_CR1_OPM | TIM_CR1_CEN;

        /* Release frame rate generator */
        NVIC_EnableIRQ(TIM4_IRQn);
    }
    else {
        /* Clear LEDs */
        for (int i = 0; i < 3; i++) {
            while (led_busy());
            led_clear();
            led_universe();
        }

        /* Wait for last universe to be finished */
        while (led_busy());
        NVIC_DisableIRQ(DMA1_Channel6_IRQn);

        /* Pull down driver inputs to prevent power leakage to LEDs */
        GPIOB->BRR = ones;

        /* Disable voltage regulator */
        sys_vcc(0, VCC_LED);
    }
}

void led_length(uint16_t length)
{
    if (length < 1)
        length = 1;
    else if (length > MAXLEDS)
        length = MAXLEDS;

    nbits = length * 3 * 8;
}


static void transpose(uint16_t offset, uint8_t string, uint32_t triplet)
{
    /* Pointer aliasing and transposition.
    bits is aligned to 4 bytes and alias is offset by multiples of 24 so alias
    is also aligned to 4 bytes and can thus be accessed as uint32_t.

    Each byte in the bits array contains a single bit for all the eight ports.
    Every four bytes are transposed to an uint32_t alias that then contains a
    pattern of four bits for each port. mask is used to clear the bits in for
    consecutive bytes in the pattern at once. */
    uint8_t port = 2 + string;
    uint32_t mask = 0x01010101 << port;
    uint32_t *alias = (uint32_t *) &bits[offset * 3 * 8];

#define TRANSPOSE(x, srcbit, dstbyte) \
    ( (((x) >> (srcbit)) & 1) << ((dstbyte) * 8) )

    *alias &= ~mask;
    *alias++ |= (
        TRANSPOSE(triplet, 23, 0) |
        TRANSPOSE(triplet, 22, 1) |
        TRANSPOSE(triplet, 21, 2) |
        TRANSPOSE(triplet, 20, 3)) << port;

    *alias &= ~mask;
    *alias++ |= (
        TRANSPOSE(triplet, 19, 0) |
        TRANSPOSE(triplet, 18, 1) |
        TRANSPOSE(triplet, 17, 2) |
        TRANSPOSE(triplet, 16, 3)) << port;

    *alias &= ~mask;
    *alias++ |= (
        TRANSPOSE(triplet, 15, 0) |
        TRANSPOSE(triplet, 14, 1) |
        TRANSPOSE(triplet, 13, 2) |
        TRANSPOSE(triplet, 12, 3)) << port;

    *alias &= ~mask;
    *alias++ |= (
        TRANSPOSE(triplet, 11, 0) |
        TRANSPOSE(triplet, 10, 1) |
        TRANSPOSE(triplet,  9, 2) |
        TRANSPOSE(triplet,  8, 3)) << port;

    *alias &= ~mask;
    *alias++ |= (
        TRANSPOSE(triplet,  7, 0) |
        TRANSPOSE(triplet,  6, 1) |
        TRANSPOSE(triplet,  5, 2) |
        TRANSPOSE(triplet,  4, 3)) << port;

    *alias &= ~mask;
    *alias++ |= (
        TRANSPOSE(triplet,  3, 0) |
        TRANSPOSE(triplet,  2, 1) |
        TRANSPOSE(triplet,  1, 2) |
        TRANSPOSE(triplet,  0, 3)) << port;
}

static inline uint32_t scale(uint8_t red, uint8_t green, uint8_t blue)
{
    /* (color > 0) is equal to 1 if color is nonzero. This compensates for the
    right shift by 8 bits that is used instead of a division by 255. */
    uint32_t r = ( ((uint16_t) red      + (red   > 0) )   * sred)   >> 8;
    uint32_t g = ( ((uint16_t) green    + (green > 0) ) * sgreen)   >> 8;
    uint32_t b = ( ((uint16_t) blue     + (blue  > 0) )  * sblue)   >> 8;

    /* Bit arrangement.
                23 .. 16    15 ..  8     7 ..  0
    WS2812:     G7 .. G0    R7 .. R0    B7 .. B0 */
    return ~(
        (g << (2 * 8)) |
        (r << (1 * 8)) |
        (b << (0 * 8)));
}

void led_cmy(uint16_t offset, uint8_t string, uint8_t cyan, uint8_t magenta, uint8_t yellow)
{
    if (string > 5)
        string = 5;
    if (offset >= MAXLEDS)
        offset = MAXLEDS - 1;

    uint32_t triplet = scale(~cyan, ~magenta, ~yellow);
    transpose(offset, string, triplet);
}


void led_rgb(uint16_t offset, uint8_t string, uint8_t red, uint8_t green, uint8_t blue)
{
    if (string > 5)
        string = 5;
    if (offset >= MAXLEDS)
        offset = MAXLEDS - 1;

    uint32_t triplet = scale(red, green, blue);
    transpose(offset, string, triplet);
}

void led_clear(void)
{
    memset(bits, 0xFF, MAXBITS);
}

void led_dim(uint8_t red, uint8_t green, uint8_t blue)
{
    sred = red;
    sgreen = green;
    sblue = blue;
}

static inline void map2(struct led_map_t *restrict map, const uint8_t *restrict buf)
{
    uint8_t r = map->red.value;
    const uint8_t *br, *er, *pr;
    if (map->flags & MAP_STATIC_RED) {
        br = &r;
        er = 0;
    }
    else {
        br = &buf[map->red.begin];
        er = &buf[map->red.end];
    }

    uint8_t g = map->green.value;
    const uint8_t *bg, *eg, *pg;
    if (map->flags & MAP_STATIC_GREEN) {
        bg = &g;
        eg = 0;
    }
    else {
        bg = &buf[map->green.begin];
        eg = &buf[map->green.end];
    }

    uint8_t b = map->blue.value;
    const uint8_t *bb, *eb, *pb;
    if (map->flags & MAP_STATIC_BLUE) {
        bb = &b;
        eb = 0;
    }
    else {
        bb = &buf[map->blue.begin];
        eb = &buf[map->blue.end];
    }

    pr = br;
    pg = bg;
    pb = bb;

    /* Sanity */
    if (map->string > 5)
        map->string = 5;
    if (map->begin >= MAXLEDS)
        map->begin = MAXLEDS - 1;

    for (uint16_t i = map->begin; i < MAXLEDS; i += map->step) {
        uint32_t triplet;
        if (map->flags & MAP_CMY)
            triplet = scale(~*pr, ~*pg, ~*pb);
        else
            triplet = scale(*pr, *pg, *pb);

        transpose(i, map->string, triplet);

        if (i == map->end)
            break;

        if (pr == er)
            pr = br;
        else
            pr += map->red.step;

        if (pg == eg)
            pg = bg;
        else
            pg += map->green.step;

        if (pb == eb)
            pb = bb;
        else
            pb += map->blue.step;
    }
}

void led_map(struct led_map_t *restrict map)
{
    map2(map, buffer);
}

void led_maps(void)
{
    for (size_t i = 0; i < sizeof(config.leds.map)/sizeof(*config.leds.map); i++) {
        if (config.leds.map[i].string == 0xFF)
            break;
        led_map(&config.leds.map[i]);
    }
}

void led_configure(void)
{
    led_dim(config.leds.red, config.leds.green, config.leds.blue);
    led_length(config.leds.length);
    led_framerate(config.leds.framerate);
}

void led_prepare(void)
{
    /* Port.
    Remap JTAG interface from PB3 and PB4. */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    __DSB();
    AFIO->MAPR &= ~AFIO_MAPR_SWJ_CFG;
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;

    GPIOB->CRL &= ~(
        GPIO_CRL_CNF2 |
        GPIO_CRL_CNF3 |
        GPIO_CRL_CNF4 |
        GPIO_CRL_CNF5 |
        GPIO_CRL_CNF6 |
        GPIO_CRL_CNF7);
    GPIOB->CRL |=
        GPIO_CRL_MODE2 |
        GPIO_CRL_MODE3 |
        GPIO_CRL_MODE4 |
        GPIO_CRL_MODE5 |
        GPIO_CRL_MODE6 |
        GPIO_CRL_MODE7;
    GPIOB->BRR =
        GPIO_BRR_BR2 |
        GPIO_BRR_BR3 |
        GPIO_BRR_BR4 |
        GPIO_BRR_BR5 |
        GPIO_BRR_BR6 |
        GPIO_BRR_BR7;

    /* Pixel timing */
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    __DSB();
    NVIC_DisableIRQ(TIM3_IRQn);
    TIM3->CR1 = 0;
    TIM3->CR2 = 0;
    TIM3->SMCR = 0;
    TIM3->DIER = 0;
    TIM3->CCER = 0;
    TIM3->CCMR1 = 0;
    TIM3->CCMR2 = 0;
    TIM3->PSC = 0;
    TIM3->CCR4 = 0;
    TIM3->CCR3 = trr(T_0);      /* Thigh '0' */
    TIM3->CCR2 = 0;
    TIM3->CCR1 = trr(T_1);      /* Thigh '1' */
    TIM3->ARR = trr(T_BIT);     /* Tbit */
    TIM3->CNT = TIM3->ARR;

    /* Pixel DMA.
    The AHB/APB bridge will expand narrow accesses to the destination register
    width by mirroring bytes, which may impact unrelated bits in the BRR/BSRR
    registers. The DMA PSIZE must therefore be set to 32bit as the DMA transfer
    will expand the missing bits with zeroes that will not interfere. */
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    __DSB();
    NVIC_DisableIRQ(DMA1_Channel3_IRQn);
    DMA1_Channel3->CCR =  DMA_CCR3_PSIZE_1 | DMA_CCR3_DIR;
    DMA1_Channel3->CNDTR = 0;
    DMA1_Channel3->CMAR = (uint32_t) &ones;
    DMA1_Channel3->CPAR = (uint32_t) &GPIOB->BSRR;

    NVIC_DisableIRQ(DMA1_Channel2_IRQn);
    DMA1_Channel2->CCR = DMA_CCR2_PSIZE_1 | DMA_CCR2_MINC | DMA_CCR2_DIR;
    DMA1_Channel2->CNDTR = 0;
    DMA1_Channel2->CMAR = (uint32_t) &bits;
    DMA1_Channel2->CPAR = (uint32_t) &GPIOB->BRR;

    NVIC_DisableIRQ(DMA1_Channel6_IRQn);
    DMA1_Channel6->CCR = DMA_CCR6_PSIZE_1 | DMA_CCR6_DIR | DMA_CCR6_TCIE;
    DMA1_Channel6->CNDTR = 0;
    DMA1_Channel6->CMAR = (uint32_t) &ones;
    DMA1_Channel6->CPAR = (uint32_t) &GPIOB->BRR;

    /* Frame rate generator */
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    __DSB();
    NVIC_DisableIRQ(TIM4_IRQn);
    TIM4->CR1 = TIM_CR1_ARPE;
    TIM4->CR2 = 0;
    TIM4->SMCR = 0;
    TIM4->DIER = TIM_DIER_UIE;
    TIM4->CCER = 0;
    TIM4->CCMR1 = 0;
    TIM4->CCMR2 = 0;
    TIM4->PSC = SystemCoreClock / UINT32_C(10000) - 1;
    TIM4->ARR = 500;
    TIM4->CNT = TIM4->ARR;

    nbits = MAXBITS;
    capture = false;
    led_clear();

    led_configure();
}
