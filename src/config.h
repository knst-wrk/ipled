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

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ff/ff.h"

#include "leds.h"

struct config_t
{
    struct config_rf_t {
        uint32_t frequency;
        uint16_t bitrate;
        uint32_t afcbw;
        uint32_t rxbw;
        uint32_t fdev;
        int8_t power;
        int16_t sensitivity;

        uint16_t mesh;
        uint8_t node;
    } rf;

    struct config_leds_t {
        uint16_t length;
        uint16_t framerate;

        uint8_t red;
        uint8_t green;
        uint8_t blue;

        struct led_map_t map[16];

        FSIZE_t default_;
    } leds;

    struct config_mode_t {
        enum {
            no_mode,

            beacon_mode,
            dmx_mode,
            rx_mode,
            scene_mode,
            standalone_mode,
            tpm2_mode,
            tx_mode,
        } mode;
        uint32_t listen;

        FSIZE_t scenes_[10];
        FSIZE_t mode_;
    } mode;
};

extern struct config_t config;



void cfg_default(void);
void cfg_map(FSIZE_t map_);

FSIZE_t cfg_scene(uint16_t scene);
FSIZE_t cfg_command(FSIZE_t s);

void cfg_prepare(void);

#endif
