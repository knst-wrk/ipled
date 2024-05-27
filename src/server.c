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
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "cmsis/stm32f10x.h"

#include "timeout.h"
#include "handler.h"
#include "version.h"
#include "system.h"
#include "analog.h"
#include "buffer.h"
#include "config.h"
#include "scene.h"
#include "rfio.h"
#include "tty.h"


#include "server.h"


/* Receiver buffer */
#define BUF     ((char *) buffer)
#define BUFEND  ((char *) &buffer[MAXBUFF-1])
static char *volatile pbuf;
static volatile bool rq;

static void digester(uint32_t status, uint8_t ch)
{
    char *p = pbuf;
    if (rq)
        return;

    if (status & (USART_SR_FE | USART_SR_NE)) {
        /* Flush input */
        pbuf = BUF;
        rq = false;
    }
    else {
        if (pbuf < BUFEND) {
            if (p > BUF) {
                /* Detect end of header */
                if (p[-1] == '\n' && ch == '\n') {
                    p[-1] = '\0';
                    rq = true;
                }
            }

            *p++ = ch;
            pbuf = p;
        }
        else {
            /* Overflow without end of header */
            pbuf = BUF;
            rq = false;
        }
    }
}

static void flush(void)
{
    /* Inhibit digester interrupt, then reset rq */
    rq = true;
    pbuf = BUF;
    rq = false;
}

static char *cats(char *p, const char *s)
{
    while (*s && p < BUFEND)
        *p++ = *s++;
    *p = '\0';
    return p;
}


static char *cati(char *p, int32_t i, uint8_t base)
{
    if (i < 0) {
        /* Need space for sign and one digit at least */
        if (p < BUFEND - 1)
            *p++ = '-';
        else
            return p;
    }
    else if (p == BUFEND) {
        /* Need space for '0' at least */
        return p;
    }

    char *b = p;
    do {
        /* Append digits in reverse order */
        int digit = abs(i % base);
        *p++ = (digit <= 9) ? digit + '0' : digit - 10 + 'A';
        i /= base;
    } while (i && p < BUFEND);

    if (i && p == BUFEND) {
        /* Not enough space left -> abort */
        p = (i < 0) ? b - 1: b;
    }
    else {
        /* Reverse digits */
        for (char *r = p; r > b; ) {
            char ch = *--r;
            *r = *b;
            *b++ = ch;
        }
    }

    *p = '\0';
    return p;
}

void srv_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    char *p = pbuf;
    while (*fmt && p < BUFEND) {
        if (*fmt == '%') {
            if (!*++fmt)
                break;

            switch (*fmt) {
                int i;
                const char *s;
                case 'i':
                    i = va_arg(ap, int);
                    p = cati(p, i, 10);
                    break;
                case 'x':
                    i = va_arg(ap, int);
                    p = cati(p, i, 16);
                    break;

                case 's':
                    s = va_arg(ap, const char *);
                    p = cats(p, s);
                    break;


                case '%':
                    *p++ = '%';
                    break;

                default:
                    break;
            }

            fmt++;
        }
        else {
            /* Plain character */
            *p++ = *fmt++;
        }
    }

    *p = '\0';
    pbuf = p;
}


static char *scni(char *p, int32_t *i, int32_t min, int32_t max)
{
    /* Skip whitespace */
    while (isspace(*p))
        p++;

    /* Read sign */
    char sign = '\0';
    if (*p == '-' || *p == '+')
        sign = *p++;

    while (isspace(*p))
        p++;

    if (!isdigit(*p))
        return 0;

    /* Read digits */
    int32_t value = 0;
    while (isdigit(*p)) {
        int dg = *p++ - '0';
        if (sign == '-') {
            if (value < INT32_MIN / 10)
                return 0;
            value *= 10;
            if (value < INT32_MIN + dg)
                return 0;
            value -= dg;
        }
        else {
            if (value > INT32_MAX / 10)
                return 0;
            value *= 10;
            if (value > INT32_MAX - dg)
                return 0;
            value += dg;
        }
    }

    if (value < min || value > max)
        return 0;
    *i = value;

    /* Also skip trailing whitespace for convenience */
    while (isspace(*p))
        p++;
    return p;
}


bool base64decode(uint8_t *buf, uint16_t *length)
{
#define mm          0xFF
    static const unsigned char d[] = {
        mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm,
        mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm,
        mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, 62, mm, mm, mm, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, mm, mm, mm, mm, mm, mm,
        mm,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, mm, mm, mm, mm, mm,
        mm, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, mm, mm, mm, mm, mm,
        mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm,
        mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm,
        mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm,
        mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm,
        mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm,
        mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm,
        mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm,
        mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm, mm
    };

    uint8_t *in = buf;
    uint8_t *out = buf;
    uint32_t triplet = 0;
    uint8_t round = 0;
    while ((*length)--) {
        uint8_t ch = *in++;
        if (isspace(ch))
            continue;
        else if (ch == '=' || ch == '\0')
            break;
        else if (d[ch] == 0xFF)
            return false;

        /* Shift in */
        triplet <<= 6;
        triplet |= d[ch];
        round++;

        if (round == 4) {
            /* Decode */
            *out++ = (triplet >> 16) & 0xFF;
            *out++ = (triplet >>  8) & 0xFF;
            *out++ = (triplet >>  0) & 0xFF;
            round = 0;
        }
    }

    /* Decode rest */
    if (round == 3) {
        *out++ = (triplet >> (2 + 8)) & 0xFF;
        *out++ = (triplet >> (2 + 0)) & 0xFF;
    }
    else if (round == 2) {
        *out++ = (triplet >> (4 + 0)) & 0xFF;
    }

    *length = out - buf;
    return true;
}



static void response(uint16_t code, const char *text)
{
    /* Inhibit digester */
    rq = true;
    pbuf = BUF;

    srv_printf("%i %s\n", code, text);
}

static void helo_request(char *p)
{
    (void) p;
    response(SRV_OK, "Ready");
    srv_printf("Hardware version: %i\n", HARDWARE_VERSION);
    srv_printf("Software version: %i\n", SOFTWARE_VERSION);
    srv_printf("Vbat: %i\n", ad_vbat());
    srv_printf("Temperature: %i\n", ad_temp());

    /* Print UID with fixed length */
    uint32_t uid = sys_uid();
    srv_printf("Identifier: ");
    for (size_t i = 0; i < 8; i++) {
        srv_printf("%x", (int) ((uid >> 28) & 0xF));
        uid <<= 4;
    }
    srv_printf("\n");
}

static void wake_request(char *p)
{
    int32_t id;
    if (!(p = scni(p, &id, 1, 254))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }

    bool res = hnd_wake(id);
    if (id != 0xFF) {
        if (res)
            response(SRV_OK, "Wake up");
        else
            response(SRV_NO_NODE, "No node");
    }
    else {
        response(SRV_OK, "Wake up broadcast");
    }
}

static void sleep_request(char *p)
{
    int32_t id;
    if (!(p = scni(p, &id, 1, 254))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }

    if (hnd_sleep(id))
        response(SRV_OK, "Sleep");
    else
        response(SRV_NO_NODE, "No node");
}

static void ping_request(char *p)
{
    int32_t id;
    if (!(p = scni(p, &id, 0, 254))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }

    /* Send ping */
    uint16_t vbat;
    int16_t rssi, temp;
    if (hnd_ping(id, &vbat, &rssi, &temp)) {
        response(SRV_OK, "Pong");
        srv_printf("Vbat: %i\n", vbat);
        srv_printf("Rssi: %i\n", rssi);
        srv_printf("Temperature: %i\n", temp);
    }
    else {
        response(SRV_NO_NODE, "No node");
    }
}

static void finger_request(char *p)
{
    int32_t id;
    if (!(p = scni(p, &id, 0, 254))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }

    /* Send finger */
    uint32_t uid;
    uint16_t hv, sv;
    if (hnd_finger(id, &uid, &hv, &sv)) {
        response(SRV_OK, "Finger");
        srv_printf("Hardware version: %i\n", hv);
        srv_printf("Software version: %i\n", sv);

        /* Print UID with fixed length */
        srv_printf("Identifier: ");
        for (size_t i = 0; i < 8; i++) {
            srv_printf("%x", (int) ((uid >> 28) & 0xF));
            uid <<= 4;
        }
        srv_printf("\n");
    }
    else {
        response(SRV_NO_NODE, "No node");
    }
}

static void start_request(char *p)
{
    int32_t id;
    if (!(p = scni(p, &id, 0, 254))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }

    int32_t scene;
    if ((p = scni(p, &scene, 0, UINT16_MAX))) {
        /* Send scene request */
        if (hnd_start(id, scene)) {
            response(SRV_OK, "Playing");
            srv_printf("Scene: %i\n", scene);
        }
        else {
            response(SRV_NO_NODE, "No node");
        }
    }
    else {
        response(SRV_ILL_ARG, "Illegal argument");
    }
}

static void pause_request(char *p)
{
    int32_t id;
    if (!(p = scni(p, &id, 0, 254))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }

    /* Stop scene */
    if (hnd_pause(id))
        response(SRV_OK, "Paused");
    else
        response(SRV_NO_NODE, "No node");
}

static void skip_request(char *p)
{
    int32_t id;
    if (!(p = scni(p, &id, 0, 254))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }

    /* Stop scene */
    if (hnd_skip(id))
        response(SRV_OK, "Skipped");
    else
        response(SRV_NO_NODE, "No node");
}

static void stop_request(char *p)
{
    int32_t id;
    if (!(p = scni(p, &id, 0, 254))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }

    /* Stop scene */
    if (hnd_stop(id))
        response(SRV_OK, "Stopped");
    else
        response(SRV_NO_NODE, "No node");
}

static void frame_request(char *p)
{
    int32_t id;
    if (!(p = scni(p, &id, 0, 254))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }

    /* Stop scene */
    if (hnd_frame(id))
        response(SRV_OK, "Frame generated");
    else
        response(SRV_NO_NODE, "No node");
}

static void dim_request(char *p)
{
    int32_t id;
    if (!(p = scni(p, &id, 0, 254))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }

    int32_t red, green, blue;
    if (!(p = scni(p, &red, 0, 255))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }
    if (!(p = scni(p, &green, 0, 255))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }
    if (!(p = scni(p, &blue, 0, 255))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }

    /* Send scene request */
    if (hnd_dim(id, red, green, blue)) {
        response(SRV_OK, "Dimmed");
    }
    else {
        response(SRV_NO_NODE, "No node");
    }
}

static void rssi_request(char *p)
{
    (void) p;

#ifdef DEBUG
    srv_printf("***\n");
    rf_debug();
    srv_printf("***\n");
#endif

    if (rf_received()) {
        response(SRV_OK, "Rssi");

        srv_printf("Rssi: %i\n", rf_rssi());
        srv_printf("Fei: %i\n", rf_fei());

        /* Discard packet */
        uint8_t length = MAXPACK;
        uint8_t msg[MAXPACK];
        rf_receive(msg, &length);
        srv_printf("Package:");
        for (size_t i = 0; i < length; i++)
            srv_printf(" 0x%x", msg[i]);
        srv_printf("\n");
    }
    else {
        response(300, "No rssi");
    }
}

static void tpm2_request(char *p)
{
    int32_t id;
    if (!(p = scni(p, &id, 0, 254))) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }

    /* Translate */
    uint8_t *buf = (uint8_t *) p;
    uint16_t length = strlen(p);
    if (!base64decode(buf, &length)) {
        response(SRV_ILL_ARG, "Illegal argument");
        return;
    }

    /* Send frame */
    if (hnd_tpm2(id, buf, length))
        response(SRV_OK, "Frame sent");
    else
        response(SRV_NO_NODE, "No node");
}


struct request_t {
    const char *const name;
    void (*proc)(char *p);
};

/* Keep in alphabetical order */
static const struct request_t requests[] = {
    { "DIM", &dim_request },
    { "FINGER", &finger_request },
    { "FRAME", &frame_request },
    { "HELO", &helo_request },
    { "PAUSE", &pause_request },
    { "PING", &ping_request },
    { "RSSI", &rssi_request },
    { "SKIP", &skip_request },
    { "SLEEP", &sleep_request },
    { "START", &start_request },
    { "STOP", &stop_request },
    { "TPM2", &tpm2_request },
    { "WAKE", &wake_request },
};

static int cmprequest(const void *a, const void *b)
{
    const char *aa = (const char *) a;
    const struct request_t *bb = (struct request_t *) b;
    return strcmp(aa, bb->name);
}

bool srv_serve(void)
{
    if (!rq)
        return false;

    /* Flush if solely whitespace */
    char *p = BUF;
    while (isspace(*p))
        p++;

    if (!*p) {
        flush();
        return false;
    }

    /* Parse request type and arguments */
    const char *type = p;
    while (isalnum(*p))
        *p++ = toupper(*p);

    char *arg = (*p) ? p + 1 : p;
    *p = '\0';

    const struct request_t *request = bsearch(type,
        requests, sizeof(requests)/sizeof(*requests), sizeof(*requests),
        &cmprequest);

    if (!request) {
        response(400, "Bad Request");
        tty_puts(BUF);
        tty_puts("\n");
        flush();
        return false;
    }
    else {
        (*request->proc)(arg);
        tty_puts(BUF);
        tty_puts("\n");
        flush();
        return true;
    }
}

void srv_enable(bool enable)
{
    flush();
    if (enable) {
        tty_baud(57600);
        tty_hook(&digester);

        rf_promiscuous(true);
    }
    else {
        rf_promiscuous(false);

        tty_hook(0);
    }
}

void srv_prepare(void)
{
    pbuf = BUF;
}
