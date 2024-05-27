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

#ifndef DMX_H
#define DMX_H

#include <stdint.h>
#include <stdbool.h>

#define MAXDMX 512

#define DMX_TIMEOUT                 1000

void dmx_enable(bool enable);
bool dmx_detect(void);

bool dmx_trip(void);
void dmx_clear(void);

void dmx_prepare(void);

#endif
