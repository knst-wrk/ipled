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

#ifndef LEDS_H
#define LEDS_H

#include <stdint.h>
#include <stdbool.h>

/* Maximum number of LEDs per string */
#ifndef DEBUG
#define MAXLEDS 500
#else
/* Reduced in debug build to reserve some stack for printf() */
#define MAXLEDS 300
#endif


void led_universe(void);
bool led_busy(void);

void led_cmy(uint16_t offset, uint8_t string, uint8_t cyan, uint8_t magenta, uint8_t yellow);
void led_rgb(uint16_t offset, uint8_t string, uint8_t red, uint8_t green, uint8_t blue);
void led_clear(void);

#define MAP_STATIC_RED          0x01
#define MAP_STATIC_GREEN        0x02
#define MAP_STATIC_BLUE         0x04
#define MAP_CMY                 0x80

struct led_map_t {
    uint8_t string;
    uint16_t begin;
    uint16_t end;
    int8_t step;

    struct {
        uint16_t begin;
        uint16_t end;
        int8_t step;
        uint8_t value;
    } red, green, blue;

    uint8_t flags;
};

void led_map(struct led_map_t *restrict map);
void led_maps(void);

void led_framerate(uint16_t fps);
void led_enable(bool enable);
void led_length(uint16_t length);
void led_dim(uint8_t red, uint8_t green, uint8_t blue);

bool led_capture(void);
void led_release(void);

void led_configure(void);
void led_prepare(void);

#endif
