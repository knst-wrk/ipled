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

/** Timeout generation.
The timeout generator governs the Cortex' SysTick interrupt. The SysTick is
configured to trigger every millisecond and the SysTick handler uses it to
increase the 'timeout' counter.

The maximum timeout is UINT32_MAX/2 milliseconds, or approximately 600 hours.
The timeout must be tested within this period.

NOTE A silicon defect causes the SysTick timer interrupt to wake up the device
    from STOP mode.
*/

#include "cmsis/stm32f10x.h"

#include "timeout.h"

static volatile uint32_t timeout;

void SysTick_Handler(void) __USED;
void SysTick_Handler(void)
{
    timeout++;
}


timeout_t tot_set(uint32_t msecs)
{
    if (msecs >= (UINT32_MAX / 2))
        msecs = UINT32_MAX / 2;
    return timeout + msecs;
}

bool tot_expired(timeout_t t)
{
    /* Subtracting the timeout's current value from t yields the number of
    milliseconds until expiration of the timeout, or an unsigned integer
    overflow in case the timeout has already expired.
    The then overflown result will continuously decrease from UINT32_MAX, so an
    expired timeout will test as expired for about 600 hours. Obviously it must
    be tested at least once within that period. */
    return (t - timeout) > (UINT32_MAX / 2);
}

uint32_t tot_remaining(timeout_t t)
{
    uint32_t dt = t - timeout;
    if (dt > UINT32_MAX / 2)
        return 0;
    else
        return dt;
}


void tot_delay(uint32_t msecs)
{
    timeout_t timeout = tot_set(msecs);
    while (!tot_expired(timeout));
}

void tot_prepare(void)
{
    timeout = 0;
    SysTick_Config(SystemCoreClock / 1000);
}
