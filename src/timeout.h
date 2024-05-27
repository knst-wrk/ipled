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

#ifndef TIMEOUT_H
#define TIMEOUT_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t timeout_t;
timeout_t tot_set(uint32_t msecs);
bool tot_expired(timeout_t t);
uint32_t tot_remaining(timeout_t t);

void tot_delay(uint32_t msecs);

void tot_prepare(void);

#endif
