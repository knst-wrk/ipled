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

/** DMX input.
DMX universes are continuously received and written to the input buffer,
starting at an offset of MAXDMX. This is considered the intermediate DMX buffer.
At the end of each universe the intermediate buffer is copied at once to the
input buffer, starting at the very beginning. This is done only if the trip is
clear; it will be set in turn.
This is to avoid flicker due to incomplete universes being rendered.
*/

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "cmsis/stm32f10x.h"

#include "tty.h"
#include "buffer.h"
#include "timeout.h"

#include "dmx.h"

#define DMX_BAUD                250000UL

#define DMX_START               0x00

#if MAXBUFF < 2*MAXDMX
#   error MAXBUFF must be twice as big as MAXDMX
#endif


static timeout_t timeout;
static uint16_t index;

static volatile bool trip;
static volatile bool trap;

static void digester(uint32_t status, uint8_t ch)
{
    if (status & (USART_SR_FE | USART_SR_NE)) {
        /* Assume break */
        index = 0;
    }
    else {
        if (!index) {
            if (ch == DMX_START) {
                trap = true;
                if (!trip) {
                    memcpy(&buffer[0], &buffer[MAXDMX], MAXDMX);
                    trip = true;
                }
            }
            else {
                index = MAXDMX;
            }
        }
        else if (index < MAXDMX) {
            buffer[MAXDMX + index] = ch;
        }
    }
}

bool dmx_trip(void)
{
    return trip;
}

void dmx_clear(void)
{
    trip = false;
}

void dmx_enable(bool enable)
{
    trip = false;
    trap = false;
    index = MAXDMX;
    timeout = tot_set(DMX_TIMEOUT);

    if (enable) {
        tty_baud(DMX_BAUD);
        tty_hook(&digester);
    }
    else {
        tty_hook(0);
    }
}

bool dmx_detect(void)
{
    if (trap) {
        trap = false;
        timeout = tot_set(DMX_TIMEOUT);
        return true;
    }
    else if (!tot_expired(timeout)) {
        return true;
    }

    return false;
}

void dmx_prepare(void)
{

}
