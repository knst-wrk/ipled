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

/** Configuration.
*/

#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "ff/ff.h"

#include "sd.h"
#include "leds.h"
#include "rfio.h"
#include "scene.h"
#include "system.h"
#include "buffer.h"
#include "timeout.h"

#include "config.h"

struct config_t config = {
    .rf = {
        .frequency = 868000000,
        .bitrate = 4800,
        .afcbw = 15600,
        .rxbw = 10400,
        .fdev = 5000,
        .power = 13,
        .sensitivity = -90,

        .mesh = 0xAAAA,
        .node = 1,
    },

    .leds = {
        .length = MAXLEDS,
        .framerate = 1,

        .red = 0xFF,
        .green = 0xFF,
        .blue = 0xFF,

        .map = {
            [0] = {
                .string = 0xFF,
            }
        },

        .default_ = 0
    },

    .mode = {
        .mode = no_mode,
        .listen = 1000,

        .scenes_ = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        .mode_ = 0,
    },
};

static int ch0;
static FATFS fs;
static FIL index;
static size_t line;

static void ungetch(int ch)
{
    if (ch >= 0)
        ch0 = ch;
}

static int getch(void)
{
    if (ch0 >= 0) {
        int ch = ch0;
        ch0 = -1;
        return ch;
    }
    else {
        uint8_t ch;
        UINT br;
        if (f_read(&index, &ch, 1, &br) != FR_OK)
            return -1;
        else if (br != 1)
            return -1;

        if (ch == '\n')
            line++;

        return ch;
    }
}

static FSIZE_t tell(void)
{
    return f_tell(&index);
}

static void seek(FSIZE_t p)
{
    ch0 = -1;
    f_lseek(&index, p);
}

static bool fail(char *s);
#define FAIL(s)     fail(s)

#define EXPECT(x)   \
    do {                                                \
        if (token() != (x))                             \
            return FAIL("Expected " #x);   \
    } while (0)                                         \


static const char *const keywords[] =
{
    /* Must be in alphabetic order for usage with bsearch() */
    "afcbw",
    "bitrate",
    "cmy",
    "default",
    "dim",
    "fdev",
    "framerate",
    "frequency",
    "leds",
    "length",
    "listen",
    "map",
    "mesh",
    "mode",
    "node",
    "pause",
    "power",
    "rf",
    "rgb",
    "rxbw",
    "scene",
    "sensitivity",
};

enum token_t
{
    tok_eof = 0,

    tok_semicolon,
    tok_colon,
    tok_comma,
    tok_lbrace,
    tok_rbrace,
    tok_lparen,
    tok_rparen,
    tok_assign,

    tok_string,
    tok_color,
    tok_range,
    tok_int,

    /* Keyword tokens must be aligned with the keywords array */
    tok_keyword,
    tok_keyword_afcbw,
    tok_keyword_bitrate,
    tok_keyword_cmy,
    tok_keyword_default,
    tok_keyword_dim,
    tok_keyword_fdev,
    tok_keyword_framerate,
    tok_keyword_frequency,
    tok_keyword_leds,
    tok_keyword_length,
    tok_keyword_listen,
    tok_keyword_map,
    tok_keyword_mesh,
    tok_keyword_mode,
    tok_keyword_node,
    tok_keyword_pause,
    tok_keyword_power,
    tok_keyword_rf,
    tok_keyword_rgb,
    tok_keyword_rxbw,
    tok_keyword_scene,
    tok_keyword_sensitivity,
};

static enum token_t tok;

static int cmpkeyword(const void *a, const void *b) {
    const char *aa = (const char *) a;
    const char *bb = *(char **) b;
    return strcmp(aa, bb);
}

static enum token_t token(void)
{
    int ch;
    while ( (ch = getch()) != EOF ) {
        /* Skip whitespace */
        if (isspace(ch))
            continue;

        /* Strip comments */
        if (ch == '/') {
            ch = getch();
            if (ch == '/') {
                /* Single line comment */
                do {
                    ch = getch();
                } while (ch >= 0 && ch != '\n');
                continue;
            }
            else if (ch == '*') {
                /* Block comment */
                do {
                    do {
                        ch = getch();
                    } while (ch >= 0 && ch != '*');

                    ch = getch();
                } while (ch >= 0 && ch != '/');
                continue;
            }
            else {
                ungetch(ch);
                ch = '/';
            }
        }

        /* Literals */
        if (ch == '"') {
            /* Strings */
            ungetch(ch);
            return tok_string;
        }
        else if (isdigit(ch) || ch == '-' || ch == '+') {
            /* Integers */
            ungetch(ch);
            return tok_int;
        }
        else if (ch == '[') {
            ungetch(ch);
            return tok_range;
        }
        else if (ch == '&') {
            /* Colors */
            ungetch(ch);
            return tok_color;
        }

        /* Single character tokens */
        switch (ch)
        {
            case '{':
                return tok_lbrace;

            case '}':
                return tok_rbrace;

            case '(':
                return tok_lparen;

            case ')':
                return tok_rparen;

            case ';':
                return tok_semicolon;

            case '=':
                return tok_assign;

            case ':':
                return tok_colon;

            case ',':
                return tok_comma;

            default:
                break;
        }

        /* Keywords */
        if (isalpha(ch)) {
            char keyword[16 + 1];
            char *p = keyword;
            do {
                if (p < &keyword[sizeof(keyword)/sizeof(*keyword)])
                    *p++ = ch;
            } while (isalpha( ch = getch() ));
            ungetch(ch);
            *p = '\0';

            const char **match = bsearch(keyword,
                keywords, sizeof(keywords)/sizeof(*keywords), sizeof(*keywords),
                &cmpkeyword);
            if (match)
                return tok_keyword + (match - keywords) + 1;
            else
                return FAIL("Unknown keyword");
        }

        return FAIL("Stray character");
    }

    return tok_eof;
}


/* Strings.
Syntax: '"' characters... '"'
Where characters... can contain the following escapes:
    \t      tabulator
    \r      carriage return
    \n      line feed
    \\      literal backslash
    \"      literal double quote
*/
static bool read_string(char *buf, size_t buflen)
{
    int ch = getch();
    if (ch != '"')
        return FAIL("Expected '\"'");

    if (buflen)
        buflen--;
    else
        buf = 0;

    while ( (ch = getch()) >= 0 ) {
        if (ch == '"') {
            break;
        }
        else if (ch == '\\') {
            ch = getch();
            if (ch < 0)
                break;

            switch (ch) {
                case 't':   ch = '\t';  break;
                case 'r':   ch = '\r';  break;
                case 'n':   ch = '\n';  break;

                case '"':
                case '\\':
                    break;

                default:
                    ungetch(ch);
                    ch = '\\';
                    break;
            };
        }

        if (buf) {
            if (!buflen)
                return FAIL("String too long");

            buflen--;
            *buf++ = ch;
        }
    }

    if (buf)
        *buf = '\0';
    return true;
}

/* Integers.
Syntax: ['+' | '-'] ['0' ['x']] digits...
The sign is optional. A leading zero indicates base 8 (octal) digits, a leading
zero followed by 'x' indicates base 16 (hexadecimal) digits. Otherwise base is
10 (decimal).
*/
static bool read_int(int32_t *i, int32_t min, int32_t max)
{
    bool sign = false;
    int32_t value = 0;
    int ch = getch();
    if (ch == '-' || ch == '+') {
        sign = (ch == '-');
        ch = getch();
    }

    while (isspace(ch))
        ch = getch();

    int base = 10;
    if (ch == '0') {
        ch = getch();
        if (tolower(ch) == 'x' ) {
            base = 16;
            ch = getch();
        }
        else if (!isxdigit(ch)) {
            /* Literal zero */
            goto clip;
        }
        else {
            base = 8;
        }
    }

    if (!isxdigit(ch))
        return FAIL("Expected integer digit");

    while (isxdigit(ch) ) {
        if (isdigit(ch))
            ch -= '0';
        else
            ch = 10 + toupper(ch) - 'A';

        if (ch >= base)
            return FAIL("Invalid digit for base");

        if (sign) {
            if (value < INT32_MIN / base)
                return FAIL("Integer underflown");
            value *= base;
            if (value < INT32_MIN + ch)
                return FAIL("Integer underflown");
            value -= ch;
        }
        else {
            if (value > INT32_MAX / base)
                return FAIL("Integer overflown");
            value *= base;
            if (value > INT32_MAX - ch)
                return FAIL("Integer overflown");
            value += ch;
        }

        ch = getch();
    }

clip:
    ungetch(ch);
    if (value > max)
        return FAIL("Integer out of range");
    else if (value < min)
        return FAIL("Integer out of range");

    if (i)
        *i = value;
    return true;
}

/* Color components.
Syntax: INTEGER ['%']
*/
static bool read_color_comp(uint8_t *c)
{
    int32_t i;
    if (!read_int(&i, 0, 255))
        return false;

    int ch;
    while (isspace(ch = getch()));
    if (ch == '%') {
        if (i > 100)
            return FAIL("Percentage out of range");
        *c = i * 255 / 100;
    }
    else {
        *c = i;
        ungetch(ch);
    }

    return true;
}

/* Colors.
Syntax: '&' ['rgb' | 'cmy'] ( COMPONENT, ... )
Syntax: INTEGER
*/
static bool read_color(uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (getch() != '&')
        return FAIL("Expected '&'");

    uint8_t comp[3];
    tok = token();
    switch (tok) {
        case tok_keyword_rgb:
        case tok_keyword_cmy:
            EXPECT(tok_lparen);
            for (int i = 0; i < 3; i++) {
                if (i)
                    EXPECT(tok_comma);

                EXPECT(tok_int);
                if (!read_color_comp(&comp[i]))
                    return FAIL("Invalid color component");
            }

            EXPECT(tok_rparen);
            if (tok == tok_keyword_cmy) {
                comp[0] = ~comp[0];
                comp[1] = ~comp[1];
                comp[2] = ~comp[2];
            }
            break;

        case tok_int:
            if (!read_color_comp(comp))
                return FAIL("Invalid integer in gray color spec");
            comp[2] = comp[1] = comp[0];
            break;

        default:
            return FAIL("Unknown color spec");
    }

    if (r) *r = comp[0];
    if (g) *g = comp[1];
    if (b) *b = comp[2];
    return true;
}

static bool read_range(uint16_t *restrict begin, uint16_t *restrict end, int8_t *restrict step, uint16_t max)
{
    int32_t i;
    if (getch() != '[')
        return FAIL("Expected '['");

    int ch;
    uint16_t b, e;
    int8_t s;
    while (isspace(ch = getch()));
    if (ch == '^') {
        b = 0;
    }
    else {
        ungetch(ch);
        EXPECT(tok_int);
        if (!read_int(&i, 0, max))
            return FAIL("Invalid integer for range offset");
        b = i;
    }

    while (isspace(ch = getch()));
    if (ch == '.') {
        ch = getch();
        if (ch != '.')
            return FAIL("Expected range");

        /* Range */
        while (isspace(ch = getch()));
        if (ch == '$') {
            e = max;
        }
        else {
            ungetch(ch);
            EXPECT(tok_int);
            if (!read_int(&i, 0, max))
                return FAIL("Invalid integer for range end");
            e = i;
        }

        /* Step */
        while (isspace(ch = getch()));
        if (ch == '%') {
            EXPECT(tok_int);
            if (!read_int(&i, 0, 100))
                return FAIL("Invalid integer for range step");

            while (isspace(ch = getch()));
        }
        else {
            i = 1;
        }

        if (e > b)
            s = +i;
        else if (e < b)
            s = -i;
        else
            s = 1;

        /* Adjust range to be a multiple of step */
        uint16_t steps = (e - b) / s;
        e = b + steps * s;
    }
    else {
        /* Single item */
        e = b;
        s = 1;
    }

    if (ch != ']')
        return FAIL("Expected ']'");

    if (begin)  *begin = b;
    if (end)    *end = e;
    if (step)   *step = s;
    return true;
}

static bool read_block( bool (*proc)(void *), void *p )
{
    tok = token();
    if (tok == tok_lbrace) {
        while ( (tok = token()) != tok_eof ) {
            if (tok == tok_rbrace)
                break;

            if (! (*proc)(p) )
                return false;
        }
    }
    else {
        if (! (*proc)(p) )
            return false;
    }

    return true;
}

static bool map_statement(void *p)
{
    struct led_map_t map;
    if (tok != tok_int)
        return FAIL("Expected map");

    int32_t i;
    if (!read_int(&i, 0, 5))
        return FAIL("Invalid integer for string index");
    map.string = i;

    EXPECT(tok_colon);
    EXPECT(tok_range);
    if (!read_range(&map.begin, &map.end, &map.step, MAXLEDS-1))
        return FAIL("Invalid string range");

    EXPECT(tok_assign);
    tok = token();
    switch (tok) {
    case tok_color:
        /* Fixed color */
        map.flags = MAP_STATIC_RED | MAP_STATIC_GREEN | MAP_STATIC_BLUE;
        map.red.step = map.green.step = map.blue.step = 0;
        if (!read_color(&map.red.value, &map.green.value, &map.blue.value))
            return FAIL("Invalid color spec for static map");
        break;

    case tok_keyword_rgb:
    case tok_keyword_cmy:
        map.flags = (tok == tok_keyword_cmy) ? MAP_CMY : 0;

        EXPECT(tok_lparen);
        tok = token();
        if (tok == tok_range) {
            if (!read_range(&map.red.begin, &map.red.end, &map.red.step, MAXBUFF-1))
                return FAIL("Invalid red range for map");
        }
        else if (tok == tok_int) {
            map.flags |= MAP_STATIC_RED;
            map.red.step = 0;
            if (!read_color_comp(&map.red.value))
                return FAIL("Invalid fixed red color component");
        }
        else {
            return FAIL("Invalid red color spec");
        }

        EXPECT(tok_comma);
        tok = token();
        if (tok == tok_range) {
            if (!read_range(&map.green.begin, &map.green.end, &map.green.step, MAXBUFF-1))
                return FAIL("Invalid green range for map");
        }
        else if (tok == tok_int) {
            map.flags |= MAP_STATIC_GREEN;
            map.green.step = 0;
            if (!read_color_comp(&map.green.value))
                return FAIL("Invalid fixed green color component");
        }
        else {
            return FAIL("Invalid green color spec");
        }

        EXPECT(tok_comma);
        tok = token();
        if (tok == tok_range) {
            if (!read_range(&map.blue.begin, &map.blue.end, &map.blue.step, MAXBUFF-1))
                return FAIL("Invalid blue range for map");
        }
        else if (tok == tok_int) {
            map.flags |= MAP_STATIC_BLUE;
            map.blue.step = 0;
            if (!read_color_comp(&map.blue.value))
                return FAIL("Invalid fixed blue color component");
        }
        else {
            return FAIL("Invalid blue color spec");
        }

        EXPECT(tok_rparen);
        break;

    default:
        return FAIL("Expected map spec");
    }

    if (p) {
        /* Store */
        uint8_t *i = (uint8_t *) p;
        if (*i < 0xFF) {
            if (*i == sizeof(config.leds.map)/sizeof(*config.leds.map))
                return FAIL("Map count exceeded");
            else
                config.leds.map[(*i)++] = map;
        }
    }
    else {
        /* Apply */
        led_map(&map);
    }

    EXPECT(tok_semicolon);
    return true;
}

static bool leds_statement(void *p)
{
    (void) p;

    /* Blocks */
    switch (tok) {
    int32_t i;
    uint8_t n;
    uint8_t r, g, b;

    case tok_keyword_default:
        config.leds.default_ = tell();
        n = 0xFF;
        return read_block(&map_statement, &n);

    case tok_keyword_map:
        n = 0;
        if (!read_block(&map_statement, &n))
            return false;

        /* Terminate */
        if (n < sizeof(config.leds.map)/sizeof(*config.leds.map))
            config.leds.map[n].string = 0xFF;
        return true;

    case tok_keyword_length:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, 1, MAXLEDS))
            return FAIL("Invalid integer for string length");
        config.leds.length = i;
        break;

    case tok_keyword_framerate:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, 0, 30))
            return FAIL("Invalid framerate");
        config.leds.framerate = i;
        break;

    case tok_keyword_dim:
        EXPECT(tok_colon);
        EXPECT(tok_color);
        if (!read_color(&r, &g, &b))
            return FAIL("Invalid color spec for global dim");
        config.leds.red = r;
        config.leds.green = g;
        config.leds.blue = b;
        break;

    default:
        return FAIL("Unknown statement in leds block");
    }

    EXPECT(tok_semicolon);
    return true;
}

static bool rf_statement(void *p)
{
    (void) p;

    switch (tok) {
    int32_t i;

    case tok_keyword_frequency:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, 290000000, 1020000000))
            return FAIL("Invalid RF frequency");
        config.rf.frequency = i;
        break;

    case tok_keyword_bitrate:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, 1200, 300000))
            return FAIL("Invalid RF bitrate");
        config.rf.bitrate = i;
        break;

    case tok_keyword_fdev:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, 600, 300000))
            return FAIL("Invalid RF frequency deviation");
        config.rf.fdev = i;
        break;

    case tok_keyword_afcbw:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, 2600, 500000))
            return FAIL("Invalid RF AFC bandwidth");
        config.rf.afcbw = i;
        break;

    case tok_keyword_rxbw:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, 2600, 500000))
            return FAIL("Invalid RF receiver bandwidth");
        config.rf.rxbw = i;
        break;

    case tok_keyword_power:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, -18, 13))
            return FAIL("Invalid RF transmitter power");
        config.rf.power = i;
        break;

    case tok_keyword_sensitivity:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, -127, 0))
            return FAIL("Invalid RF sensitivity");
        config.rf.sensitivity = i;
        break;

    case tok_keyword_mesh:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, 1, 0xFFFE))
            return FAIL("Invalid RF mesh");
        config.rf.mesh = i;
        break;

    case tok_keyword_node:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, 1, 0xFE))
            return FAIL("Invalid RF node");
        config.rf.node = i;
        break;

    default:
        return FAIL("Unknown statement in rf block");
    }

    EXPECT(tok_semicolon);
    return true;
}

static bool scene_statement(void *p)
{
    bool run = *(bool *) p;

    switch (tok) {
    int32_t i;
    uint8_t r, g, b;
    uint8_t n;
    char buf[256];

    case tok_string:
        read_string(buf, sizeof(buf)/sizeof(*buf));
        if (run)
            sc_do_tpm2(buf);
        break;

    case tok_keyword_pause:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, 0, 60*60*1000))
            return FAIL("Invalid pause");

        if (run)
            sc_do_pause(i);
        break;

    case tok_keyword_map:
        if (run)
            sc_do_map(tell());

        n = 0xFF;
        return read_block(&map_statement, &n);

    case tok_keyword_framerate:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, 0, 30))
            return FAIL("Invalid framerate");

        if (run)
            sc_do_framerate(i);
        break;

    case tok_keyword_dim:
        EXPECT(tok_colon);
        EXPECT(tok_color);
        if (!read_color(&r, &g, &b))
            return FAIL("Invalid color spec for global dim");

        if (run)
            sc_do_dim(r, g, b);
        break;

    default:
        return FAIL("Unknown statement in scene block");
    }

    EXPECT(tok_semicolon);
    return true;
}

static bool mode_statement(void *p)
{
    /* Blocks */
    switch (tok) {
    int32_t i;

    case tok_keyword_scene:
        EXPECT(tok_int);
        if (!read_int(&i, 0, UINT16_MAX))
            return FAIL("Invalid scene index");

        if ((unsigned) i < sizeof(config.mode.scenes_)/sizeof(*config.mode.scenes_))
            config.mode.scenes_[i] = tell();

        bool run = false;
        if (p) {
            /* Scanning to scene */
            uint16_t scene = *(uint16_t *) p;
            if (scene == i)
                /* Found, abort parsing */
                return false;
            else
                /* Parse off block */
                return read_block(&scene_statement, &run);
        }
        else {
            /* Parsing through */
            return read_block(&scene_statement, &run);
        }

    case tok_keyword_listen:
        EXPECT(tok_colon);
        EXPECT(tok_int);
        if (!read_int(&i, 1, 20000))
            return FAIL("Invalid listen period");
        config.mode.listen = i;
        break;

    default:
        return FAIL("Unknown statement in mode block");
    }

    EXPECT(tok_semicolon);
    return true;
}

static bool read_mode(void)
{
    /* To be aligned with the enum */
    static const char *const modes[] = {
        "beacon",
        "dmx",
        "rx",
        "scene",
        "standalone",
        "tpm2",
        "tx",
    };

    char buf[16];
    EXPECT(tok_string);
    if (!read_string(buf, sizeof(buf)/sizeof(*buf)))
        return false;

    const char **match = bsearch(buf,
        modes, sizeof(modes)/sizeof(*modes), sizeof(*modes),
        &cmpkeyword);

    if (!match)
        return FAIL("Unknown mode");

    config.mode.mode = (match - modes) + 1;
    return true;
}

static bool parse(void)
{
    ch0 = -1;
    line = 1;
    while ( (tok = token()) != tok_eof ) {
        switch (tok) {
        case tok_keyword_rf:
            if (!read_block(&rf_statement, 0))
                return false;
            rf_configure();
            break;

        case tok_keyword_leds:
            if (!read_block(&leds_statement, 0))
                return false;
            led_configure();
            break;

        case tok_keyword_mode:
            if (config.mode.mode_)
                return FAIL("Mode already set");

            if (!read_mode())
                return false;

            config.mode.mode_ = tell();
            if (!read_block(&mode_statement, 0))
                return false;
            break;

        default:
            return FAIL("Unknown top level block");
        }
    }

    return true;
}


static bool mount(void)
{
    uint8_t retries = 3;
    goto on;
    while (retries--) {
        /* Power cycle SD card */
        sd_enable(false);
        tot_delay(200);

    on:
        /* Turn on SD power supply */
        sd_enable(true);
        tot_delay(200);

        /* Try to mount */
        if (!sd_identify())
            continue;
        else if (sd_type() == SD_NONE)
            continue;
        else if (f_mount(&fs, "", 1) != FR_OK)
            continue;

        return true;
    }

    return false;
}

void cfg_default(void)
{
    if (config.leds.default_)
        cfg_map(config.leds.default_);
}

void cfg_map(FSIZE_t map_)
{
    led_clear();
    seek(map_);
    read_block(&map_statement, 0);
}


FSIZE_t cfg_scene(uint16_t scene)
{
    if (scene < sizeof(config.mode.scenes_)/sizeof(*config.mode.scenes_)) {
        /* Directly accessible scene */
        if (!config.mode.scenes_[scene])
            return 0;

        seek(config.mode.scenes_[scene]);
    }
    else {
        /* Linear adressable scene */
        if (!config.mode.mode_)
            return 0;

        seek(config.mode.mode_);
        if (read_block(&mode_statement, &scene))
            /* Parsed through all the scenes without match */
            return 0;
    }

    tok = token();
    return (tok == tok_lbrace) ? tell() : 0;
}

FSIZE_t cfg_command(FSIZE_t s)
{
    if (!s)
        return 0;

    seek(s);
    tok = token();
    if (tok == tok_rbrace)
        return 0;

    bool run = true;
    scene_statement(&run);
    return tell();
}


static bool fail(char *s)
{
    f_close(&index);
    if (f_open(&index, "index.txt", FA_WRITE | FA_OPEN_APPEND) == FR_OK) {
        /* Sanity to prevent flooding */
        if (f_size(&index) > 4UL * 1024 * 1024)
            panic();

        /* itoa */
        char *l = (char *) &buffer[MAXBUFF];
        *--l = '\0';
        do *--l = '0' + (line % 10); while (line /= 10);

        /* Leave a diagnostic at the end of the index file */
        f_puts(
            "\n\n"
            "/***************************************************\n"
            "****************************************************\n"
            "\n\n"
            "    Error reading configuration file:\n"
            "    At line ",
            &index);

        f_puts(l, &index);
        f_puts(": ", &index);
        f_puts(s, &index);
        f_puts(
            "\n"
            "        \\\n"
            "         \\   ^__^\n"
            "          \\  (oo)\\_______\n"
            "             (__)\\       )\\/\n"
            "                 ||----w |\n"
            "                 ||     ||\n"
            "***************************************************/\n",
            &index);

        f_sync(&index);
        f_close(&index);
    }

    return false;
}


void cfg_prepare(void)
{
    /* Try to access SD card */
    config.mode.mode = no_mode;
    if (mount()) {
        if (f_open(&index, "index.txt", FA_READ) == FR_OK) {
            if (!parse()) {
                config.mode.mode = no_mode;
                panic();
            }
        }
    }
}

