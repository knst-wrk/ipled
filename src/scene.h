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

#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>
#include <stdbool.h>

#include "ff/ff.h"

void sc_do_tpm2(const char *const file);
void sc_do_pause(uint32_t t);
void sc_do_map(FSIZE_t map_);
void sc_do_framerate(uint16_t fps);
void sc_do_dim(uint8_t red, uint8_t green, uint8_t blue);

bool sc_start(uint16_t s);
void sc_pause(void);
void sc_skip(void);
void sc_stop(void);

bool sc_play(void);
void sc_prepare(void);

#endif
