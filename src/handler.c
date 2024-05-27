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

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "cmsis/stm32f10x.h"

#include "timeout.h"
#include "version.h"
#include "system.h"
#include "analog.h"
#include "config.h"
#include "scene.h"
#include "rfio.h"
#include "tpm2.h"
#include "ui.h"


#include "handler.h"


/* Magic packets and commands */
static const uint8_t wup[] = { 0xCA, 0xFE, 0xBA, 0xBE };
static const uint8_t slp[] = { 0xDE, 0xAD, 0xBE, 0xEF };

#define HND_PING        0x01
#define HND_START       0x33
#define HND_SKIP        0x34
#define HND_STOP        0x35
#define HND_PAUSE       0x37
#define HND_FRAME       0x99
/* Reserved:            0xCA (wup[0])*/
#define HND_DIM         0xD1
#define HND_TPM2        0xDA
/* Reserved:            0xDE (slp[0]) */
#define HND_FINGER      0xF1

static uint8_t msg[MAXPACK];

static uint8_t pack(const char *fmt, ...)
{
    uint8_t *p = msg;
    va_list va;
    va_start(va, fmt);
    while (*fmt) {
        switch (*fmt++) {
        union {
            uint8_t b;
            uint8_t u8;
            int8_t s8;
            uint16_t u16;
            int16_t s16;
            uint32_t u32;
            int32_t s32;
        } variant;

        case 'b':
            variant.b = (va_arg(va, int) != 0) ? 1 : 0;
            memcpy(p, &variant.b, sizeof(uint8_t));
            p += sizeof(uint8_t);
            break;

        case 'c':
            variant.s8 = va_arg(va, int);
            memcpy(p, &variant.s8, sizeof(int8_t));
            p += sizeof(int8_t);
            break;

        case '!': /* Basically an uint8_t for the command */
        case 'C':
            variant.u8 = va_arg(va, unsigned int);
            memcpy(p, &variant.u8, sizeof(uint8_t));
            p += sizeof(uint8_t);
            break;

        case 'w':
#if INT16_MAX < INT_MAX
            variant.s16 = va_arg(va, int);
#else
            variant.s16 = va_arg(va, int16_t);
#endif
            memcpy(p, &variant.s16, sizeof(int16_t));
            p += sizeof(int16_t);
            break;

        case 'W':
#if UINT16_MAX < UINT_MAX
            variant.u16 = va_arg(va, unsigned int);
#else
            variant.u16 = va_arg(va, uint16_t);
#endif
            memcpy(p, &variant.u16, sizeof(uint16_t));
            p += sizeof(uint16_t);
            break;

        case 'l':
#if INT32_MAX < INT_MAX
            variant.s32 = va_arg(va, int);
#else
            variant.s32 = va_arg(va, int32_t);
#endif
            memcpy(p, &variant.s32, sizeof(int32_t));
            p += sizeof(int32_t);
            break;

        case 'L':
#if UINT32_MAX < UINT_MAX
            variant.u32 = va_arg(va, unsigned int);
#else
            variant.u32 = va_arg(va, uint32_t);
#endif
            memcpy(p, &variant.u32, sizeof(uint32_t));
            p += sizeof(uint32_t);
            break;


        case '@':
            memcpy(p, va_arg(va, uint8_t *), 4);
            p += 4;
            break;

        default:
            break;
        }
    }

    va_end(va);
    return p - msg;
}

static bool unpack(uint8_t length, const char *fmt, ...)
{
    uint8_t *p = msg;
    va_list va;
    va_start(va, fmt);
    while (*fmt) {
        switch (*fmt++) {
        case 'b': {
            uint8_t b;
            memcpy(&b, p, sizeof(uint8_t));
            *va_arg(va, bool *) = (b != 0);
            p += sizeof(uint8_t);
            } break;

        case 'c':
            memcpy(va_arg(va, int8_t *), p, sizeof(int8_t));
            p += sizeof(int8_t);
            break;

        case 'C':
            memcpy(va_arg(va, uint8_t *), p, sizeof(uint8_t));
            /* fall-through */
        case '!': /* Ignore command when unpacking */
            p += sizeof(uint8_t);
            break;

        case 'w':
            memcpy(va_arg(va, int16_t *), p, sizeof(int16_t));
            p += sizeof(int16_t);
            break;

        case 'W':
            memcpy(va_arg(va, uint16_t *), p, sizeof(uint16_t));
            p += sizeof(uint16_t);
            break;

        case 'l':
            memcpy(va_arg(va, int32_t *), p, sizeof(int32_t));
            p += sizeof(int32_t);
            break;

        case 'L':
            memcpy(va_arg(va, uint32_t *), p,sizeof(uint32_t));
            p += sizeof(uint32_t);
            break;


        case '@':
            p += 4;
            break;

        default:
            break;
        }
    }

    va_end(va);
    return (p - msg) == length;
}


static void sndack(uint8_t length)
{
    /* Usually called after pack() but not required */
    rf_sendto(config.rf.node, msg, length);
    while (!rf_sent())
        sc_play();
}

static bool rcvack(uint8_t id, uint8_t *length)
{
    /* Must be called following rf_send()/rf_sendto() */
    while (!rf_sent());

    timeout_t timeout = tot_set(HND_TIMEOUT);
    while (!tot_expired(timeout)) {
        if (rf_received()) {
            *length = MAXPACK;
            uint8_t sender = rf_receive(msg, length);
            if (sender == id)
                return true;
        }
    }

    return false;
}


static bool sleep_listen(void)
{
    /* Save energy */
    sc_stop();

    tot_delay(100);
    led_enable(false);

    ui_led(false);

    /* Detach from RFIO clock */
    //sys_alarm(10 * 60 * 1000UL); // <-- TODO
    sys_hsi();

    /* Halt and wait for packet */
    uint8_t rcpt;
    uint8_t length;
    do {
        rf_listen(config.mode.listen, 100);
        do {
            sys_stop();
        } while (!rf_trip());

        /* Reset msg, just in case */
        length = MAXPACK;
        rcpt = rf_receive(msg, &length);
    } while ( (length < sizeof(wup)/sizeof(*wup)) || memcmp(msg, wup, sizeof(wup)/sizeof(*wup)) != 0);

    /* Stop listen mode */
    sys_hse();
    //sys_alarm(0); // <-- TODO

    led_enable(true);
    rf_enable(true);

    /* Continue */
    sc_skip();

    /* Reply after end of burst if not broadcast */
    if (rcpt != 0xFF) {
        uint32_t remaining;
        if (unpack(length, "@L", &remaining)) {
            if (remaining > config.mode.listen)
                remaining = config.mode.listen;

            remaining /= 50;
            while (remaining--) {
                tot_delay(50);
                led_enable(remaining & 1);
            }

            sndack(0);
        }
    }

    return true;
}

bool hnd_sleep(uint8_t id)
{
    /* Send 0xDEADBEEF token */
    uint8_t length = pack("@", slp);
    rf_sendto(id, msg, length);
    return rcvack(id, &length) && unpack(length, "");
}

bool hnd_wake(uint8_t id)
{
    /* Send 0xCAFEBABE burst */
    uint8_t length;
    uint32_t period = config.mode.listen + 150;
    timeout_t timeout = tot_set(period);
    do {
        length = pack("@L", wup, (uint32_t) tot_remaining(timeout));
        rf_sendto(id, msg, length);
        while (!rf_sent());
        tot_delay(42);
    } while (!tot_expired(timeout));

    if (id == 0xFF)
        return true;
    else
        return rcvack(id, &length) && unpack(length, "");
}


bool hnd_ping(uint8_t id, uint16_t *vbat, int16_t *rssi, int16_t *temp)
{
    uint8_t length = pack("!", HND_PING);
    rf_sendto(id, msg, length);
    return rcvack(id, &length) && unpack(length, "Www", vbat, rssi, temp);
}


bool hnd_start(uint8_t id, uint16_t scene)
{
    uint8_t length = pack("!W", HND_START, scene);
    rf_sendto(id, msg, length);
    return rcvack(id, &length) && unpack(length, "");
}

bool hnd_pause(uint8_t id)
{
    uint8_t length = pack("!", HND_PAUSE);
    rf_sendto(id, msg, length);
    return rcvack(id, &length) && unpack(length, "");
}

bool hnd_skip(uint8_t id)
{
    uint8_t length = pack("!", HND_SKIP);
    rf_sendto(id, msg, length);
    return rcvack(id, &length) && unpack(length, "");
}

bool hnd_stop(uint8_t id)
{
    uint8_t length = pack("!", HND_STOP);
    rf_sendto(id, msg, length);
    return rcvack(id, &length) && unpack(length, "");
}


bool hnd_frame(uint8_t id)
{
    uint8_t length = pack("!", HND_FRAME);
    rf_sendto(id, msg, length);
    return rcvack(id, &length) && unpack(length, "");
}


bool hnd_finger(uint8_t id, uint32_t *uid, uint16_t *hv, uint16_t *sv)
{
    uint8_t length = pack("!", HND_FINGER);
    rf_sendto(id, msg, length);
    return rcvack(id, &length) && unpack(length, "LWW", uid, hv, sv);
}


bool hnd_dim(uint8_t id, uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t length = pack("!CCC", HND_DIM, red, green, blue);
    rf_sendto(id, msg, length);
    return rcvack(id, &length) && unpack(length, "");
}

bool hnd_tpm2(uint8_t id, uint8_t *buf, uint16_t length)
{
    do {
        uint8_t l = pack("!", HND_TPM2);

        /* Chunks */
        uint8_t chunk = MAXPACK - l;
        if (chunk > length)
            chunk = length;

        memcpy(&msg[l], buf, chunk);
        l += chunk;
        buf += chunk;
        length -= chunk;

        rf_sendto(id, msg, l);
        if (!rcvack(id, &l) || !unpack(l, ""))
            return false;
    } while (length);

    return true;
}

bool hnd_handle(void)
{
    sc_play();
    if (!rf_received())
        return false;

    uint8_t length = MAXPACK;
    uint8_t rcpt = rf_receive(msg, &length);
    if (length == sizeof(slp)/sizeof(*slp)) {
        if (memcmp(msg, slp, sizeof(slp)/sizeof(*slp)) == 0) {
            if (rcpt != 0xFF)
                sndack(0);

            return sleep_listen();
        }
    }

    if (!length)
        return false;

    switch (msg[0]) {
    case HND_PING:
        if (!unpack(length, "!"))
            return false;

        length = pack("Www", ad_vbat(), rf_rssi(), ad_temp());
        sndack(length);
        break;

    case HND_START: {
        uint16_t scene;
        if (!unpack(length, "!W", &scene))
            return false;

        if (sc_start(scene))
            sndack(0);
        } break;

    case HND_PAUSE:
        if (!unpack(length, "!"))
            return false;

        sc_pause();
        sndack(0);
        break;

    case HND_SKIP:
        if (!unpack(length, "!"))
            return false;

        sc_skip();
        sndack(0);
        break;

    case HND_STOP:
        if (!unpack(length, "!"))
            return false;

        sc_stop();
        sndack(0);
        break;

    case HND_FRAME:
        if (!unpack(length, "!"))
            return false;

        led_universe();
        sndack(0);
        break;

    case HND_FINGER: {
        if (!unpack(length, "!"))
            return false;

        uint32_t uid = sys_uid();
        uint16_t hv = HARDWARE_VERSION;
        uint16_t sv = SOFTWARE_VERSION;
        length = pack("LWW", uid, hv, sv);
        sndack(length);
        } break;

    case HND_DIM: {
        uint8_t red, green, blue;
        if (!unpack(length, "!CCC", &red, &green, &blue))
            return false;

        led_dim(red, green, blue);
        while (!led_capture());
        led_maps();
        led_release();
        sndack(0);
        } break;

    case HND_TPM2:
        if (unpack(length, "!")) {
            /* No data */
            sc_stop();
            tp2_reset();
        }
        else {
            tp2_digest(&msg[1], length - 1);
            if (tp2_trip()) {
                led_enable(true);
                while (!led_capture());
                led_maps();
                led_release();
                tp2_clear();
            }
        }

        sndack(0);
        break;

    default:
        return false;
    }

    return true;
}

void hnd_prepare(void)
{
    /* vakat */
}
