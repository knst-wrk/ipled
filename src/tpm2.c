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

/** TP2M Decoding.
This module decodes TPM2 data according to the specification V1.0 as of 2013,
to be found at
    http://www.ledstyles.de
*/

#include <stdint.h>
#include <stdbool.h>

#include "cmsis/stm32f10x.h"

#include "tty.h"
#include "buffer.h"
#include "timeout.h"

#include "tpm2.h"

/* Magic bytes */
#define TPM2_SER_BLOCK_START_BYTE   0xC9    /* 'NEW BLOCK BYTE' for TPM2.Serial */
#define TPM2_BLOCK_TYPE_DATA        0xDA    /* Block is a  'DATA BLOCK' */
#define TPM2_BLOCK_TYPE_ZDATA       0xCA
#define TPM2_BLOCK_TYPE_CMD         0xC0    /* Block is a  'COMMAND BLOCK' */
#define TPM2_BLOCK_TYPE_ACK         0xAC    /* Block is an 'ANSWER without DATA' (Acknowledge) */
#define TPM2_BLOCK_TYPE_ACK_DATA    0xAD    /* Block is an 'ANSWER containing DATA' */
#define TPM2_BLOCK_END_BYTE         0x36    /* Last Byte of a TMP2 Block */

/* TPM2 blocks are composed as follows:
      offset   value
        0      0xC9    TPM2_SER_BLOCK_START_BYTE
        1      0xDA    TPM2_BLOCK_TYPE_DATA
        2       ..     block length MSB
        3       ..     block length LSB
        4       ..
        n       ..     block data
        n+1    0x36    TPM2_BLOCK_END_BYTE

So in a continuous sequence of blocks the concatenated values of the end byte
and the start bytes can actually be used as the block start.
*/
#define TPM2_MAGIC(type) \
    ( ((uint32_t) TPM2_BLOCK_END_BYTE           << 16) | \
      ((uint32_t) TPM2_SER_BLOCK_START_BYTE     <<  8) | \
      ((uint32_t) (type)                        <<  0) )

#define SHIFT_THRESHOLD         16

const uint32_t baudrates[] = {
    9600,
    19200,
    38400,
    57600,
    115200,
    230400,
    460800,
    500000,
};

enum {
    detect_state,

    start_state,
    type_state,

    length0_state,
    length1_state,

    data_state,
    repeat_state,
    skip_state,

    end_state
};


static uint16_t length;
static uint16_t index;
static uint32_t ch0;
static uint32_t ch1;
static uint8_t state;
static uint32_t timeout;
static timeout_t ftimeout;

static volatile bool trip;
static volatile bool trap;
static volatile uint8_t shift;


#ifdef TPM2_TPZ
static inline void unroll(uint16_t n)
{
    /* RLE unrolling.
    Note that this mus be fast enough to take place within the reception of the
    TPM2 data stream, possibly at maximum baud rate.

        800850c:	f812 1b01 	ldrb.w	r1, [r2], #1
        8008510:	f803 1b01 	strb.w	r1, [r3], #1
        8008514:	4298      	cmp	r0, r3
        8008516:	d3f9      	bcc.n	800850c <digest+0x114>
    */
    /*uint8_t *p = &buffer[index - 3];
    uint8_t *q = &buffer[index];
    uint8_t *qd = &buffer[index = n];
    while (qd < q)
        *q++ = *p++;*/
    uint8_t *p = &buffer[index - 3];
    uint8_t *q = &buffer[index];
    uint8_t *qd = &buffer[index = n];
    while (q < qd)
        *q++ = *p++;
}
#endif

static bool digest(uint8_t ch)
{
    /* Shift register */
    ch0 = (ch0 << 8) | ch;
    ch1 = (ch1 << 8) | (ch0 >> 3*8);

    switch (state) {
#ifdef TPM2_TPZ
    static bool repeat;
    uint16_t n;
#endif

    default:
    case start_state:
        if (ch == TPM2_SER_BLOCK_START_BYTE)
            state = type_state;
        break;

    case type_state:
        /* Switch block type */
#ifdef TPM2_TPZ
        repeat = false;
        if (ch == TPM2_BLOCK_TYPE_ZDATA) {
            state = length0_state;
            repeat = true;
        }
        else
#endif
        if (ch == TPM2_BLOCK_TYPE_DATA) {
            state = length0_state;
        }
        else {
            state = start_state;
        }
        break;

    case length0_state:
        state = length1_state;
        break;
    case length1_state:
        index = 0;
        length = ch0 & 0xFFFF;
        if (!length)
            state = end_state;
        else if (trip || index >= MAXBUFF)
            state = skip_state;
        else
            state = data_state;
        break;

    case skip_state:
        if (!--length)
            state = end_state;
        break;

    case data_state:
        buffer[index++] = ch0 & 0xFF;
        if (!--length) {
            state = end_state;
            trip = true;
        }
        else if (index >= MAXBUFF) {
            state = skip_state;
            trip = true;
        }
#ifdef TPM2_TPZ
        else if (repeat) {
            if ( index >= 6 && (ch0 & 0xFFFFFF) == (ch1 & 0xFFFFFF) )
                state = repeat_state;
        }
#endif
        break;

#ifdef TPM2_TPZ
    case repeat_state:
        n = index + (ch0 & 0xFF) * 3;
        if (n >= MAXBUFF) {
            /* Sanity */
            state = skip_state;
            n = MAXBUFF;
        }
        else {
            state = data_state;
        }

        ch0 >>= 8;
        ch1 = ch0;
        if (!--length)
            state = end_state;

        /* Unroll RLE */
        unroll(n);

        if (state != data_state)
            trip = true;
        break;
#endif

    case end_state:
        state = start_state;
        if (ch == TPM2_BLOCK_END_BYTE) {
            trap = true;
            return true;
        }

        break;
    }

    return false;
}

static void digester(uint32_t status, uint8_t ch)
{
    if (status & (USART_SR_FE | USART_SR_NE)) {
        length = 0;
        state = detect_state;
        if (shift < SHIFT_THRESHOLD)
            shift++;
    }
    else if (state == detect_state) {
        ch0 = (ch0 << 8) | ch;
        const uint32_t magic = ch0 & 0xFFFFFF;
        if ( (magic == TPM2_MAGIC(TPM2_BLOCK_TYPE_DATA))
#ifdef TPM2_TPZ
             || (magic == TPM2_MAGIC(TPM2_BLOCK_TYPE_ZDATA))
#endif
        ) {
            /* Count blocks */
            if (++length == 5) {
                ftimeout = tot_set(TPM2_FRAME_TIMEOUT);
                state = length0_state;
                trip = false;
                trap = false;
            }
        }
    }
    else {
        /* Try to align to frames.
        As the TPM2 protocol does not provide any means of frame sync missing
        one single byte in the stream causes infinite misalignment. By catching
        on a short interruption in the stream this can be re-aligned, provided
        that the sender actually inserts the interruption. */
        if (tot_expired(ftimeout))
            state = start_state;

        if (status & USART_SR_RXNE) {
            ftimeout = tot_set(TPM2_FRAME_TIMEOUT);
            digest(ch);
        }
    }
}

void tp2_enable(bool enable)
{
    tp2_reset();
    if (enable)
        tty_hook(&digester);
    else
        tty_hook(0);
}

bool tp2_detect(void)
{
    if (trap) {
        trap = false;
        timeout = tot_set(TPM2_TIMEOUT);
        return true;
    }
    else if (!tot_expired(timeout)) {
        if (shift < SHIFT_THRESHOLD)
            return true;
    }

    /* Try another baud rate */
    if (++index >= sizeof(baudrates)/sizeof(*baudrates))
        index = 0;
    tty_baud(baudrates[index]);

    timeout = tot_set(TPM2_TIMEOUT);
    shift = 0;
    return false;
}

bool tp2_trip(void)
{
    return trip;
}

void tp2_clear(void)
{
    trip = false;
}

size_t tp2_digest(const uint8_t *data, size_t n)
{
    const uint8_t *p = data;
    while (n--) {
        if (digest(*p++))
            break;
    }

    return p - data;
}

void tp2_reset(void)
{
    ch0 = 0;
    ch1 = 0;
    trip = false;
    trap = false;
    shift = 0;

    timeout = tot_set(TPM2_TIMEOUT);
    state = detect_state;
}

void tp2_prepare(void)
{
    tp2_reset();
}
