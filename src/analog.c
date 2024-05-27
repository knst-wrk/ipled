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

/** Analog Interface.
The ADC is clocked at 9MHz from AHB.
Only the injected conversion is used as it provides a set of buffer registers
for the conversion results, hence sparing a DMA.

NOTE ADC channel 0 is affected by a silicon defect and may experience voltage
    glithes. It is thus not used.
*/

#include <stdint.h>

#include "cmsis/stm32f10x.h"

#include "analog.h"


struct ad_t
{
    uint16_t vref;
    uint16_t vbat;
    uint16_t vled;
    uint16_t vtemp;
};

static volatile struct ad_t ad;

void ADC1_2_IRQHandler(void) __USED;
void ADC1_2_IRQHandler(void)
{
    ad.vref = ADC1->JDR1;
    ad.vbat = ADC1->JDR4;
    ad.vled = ADC1->JDR3;
    ad.vtemp = ADC1->JDR2;

    ADC1->SR = ~(ADC_SR_JSTRT | ADC_SR_JEOC);
}

/* Analog conversion results.
The ADC converts all the channels using the voltage of the V_ref+ pin as its
reference, which is internally or externally connected to Vdd. The embedded
reference voltage source provides a conversion result for a voltage of
    V_ref_int = 1.2V

This reference voltage is also converted using V_ref+ so its conversion result
can be related to V_ref+ using
    X(V_ref_int) = V_ref_int / Vdd * 4096

with 4096 being the ADC's resolution. The constant Vdd can hence be eliminated
from a channel's conversion result X(V_ch) using
    Vdd = V_ref_int * 4096 / X(V_ref_int)
    X(V_ch) = V_ch / Vdd * 4096
            = V_ch / (V_ref_int * 4096 / X(V_ref_int)) * 4096
            = V_ch / V_ref_int / 4096 * X(V_ref_int) * 4096
            = V_ch / V_ref_int * X(V_ref_int)

which finally yields
    V_ch = V_ref_int * X(V_ch) / X(V_ref_int)
*/
uint16_t ad_vbat(void)
{
    /* Voltage divider: 220k : 27k */
    return (uint32_t) AD_REFERENCE * (220 + 27) * ad.vbat / ad.vref / 27;
}

uint16_t ad_vled(void)
{
    /* Voltage divider: 47k : 27k */
    return (uint32_t) AD_REFERENCE * (47 + 27) * ad.vled / ad.vref / 27;
}

int16_t ad_temp(void)
{
    /* Using the nominal coeeficients for V_25 and Avg_Slope from the device
    datasheet computing
        T = (V_25 - V_SENSE) / Avg_Slope} + 25°C
    */
    const int32_t v_sense = (int32_t) AD_REFERENCE * ad.vtemp / ad.vref;
    const int32_t v_25 = 1430;           /* 1.43V at 25°C */
    const int32_t avg_slope = 43;       /* 4.3mV/K */
    int32_t temp = (v_25 - v_sense) * 10 / avg_slope + 25;
    if (temp < -65)
        temp = -65;
    else if (temp > 150)
        temp = 150;
    return temp;
}

void ad_convert(void)
{
    if (!(ADC1->SR & ADC_SR_JSTRT)) {
        ADC1->CR1 |= ADC_CR1_SCAN;
        ADC1->CR2 |= ADC_CR2_ADON;
    }
}


void ad_calibrate(void)
{
    NVIC_DisableIRQ(ADC1_2_IRQn);
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while (ADC1->CR2 & ADC_CR2_RSTCAL);

    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL);
    NVIC_EnableIRQ(ADC1_2_IRQn);
}

void ad_prepare(void)
{
    /* Port */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    __DSB();
    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);        /* IN1 */
    GPIOA->CRL &= ~(GPIO_CRL_MODE2 | GPIO_CRL_CNF2);        /* IN2 */

    /* ADC */
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    __DSB();
    NVIC_DisableIRQ(ADC1_2_IRQn);
    ADC1->CR1 = ADC_CR1_JAUTO | ADC_CR1_JEOCIE;
    ADC1->CR2 = ADC_CR2_TSVREFE;
    ADC1->SMPR1 =
        ADC_SMPR1_SMP17_2 |
        ADC_SMPR1_SMP17_1 |
        ADC_SMPR1_SMP17_0 |                         /* > 17.1µs for V_temp */
        ADC_SMPR1_SMP16_2;
    ADC1->SMPR2 = ADC_SMPR2_SMP2_2 | ADC_SMPR2_SMP1_2;
    ADC1->SQR1 = ADC_SQR1_L_1;                      /* 1 conversion */
    ADC1->SQR2 = 0;
    ADC1->SQR3 = ADC_SQR3_SQ1_4 | ADC_SQR3_SQ1_0;   /* SQ1 = IN17 = V_ref_int */
    ADC1->JSQR =
        ADC_JSQR_JL_1 | ADC_JSQR_JL_0 |             /* 4 conversions */
        ADC_JSQR_JSQ4_0 |                           /* SQ4 = IN1 */
        ADC_JSQR_JSQ3_1 |                           /* SQ3 = IN2 */
        ADC_JSQR_JSQ2_4 |                           /* SQ2 = IN16 = V_temp */
        ADC_JSQR_JSQ1_4 | ADC_JSQR_JSQ1_0;          /* SQ1 = IN17 = V_ref_int */

    /* First write of '1' will wake up ADC */
    ADC1->CR2 |= ADC_CR2_ADON;

    /* Will enable interrupt afterwards */
    ad_calibrate();
}
