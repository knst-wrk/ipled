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

#ifndef TPM2_H
#define TPM2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define TPM2_TIMEOUT                1000
#define TPM2_FRAME_TIMEOUT          4
#define TPM2_TPZ

size_t tp2_digest(const uint8_t *buf, size_t length);

void tp2_baud(uint32_t baud);
void tp2_enable(bool enable);
bool tp2_detect(void);

bool tp2_trip(void);
void tp2_clear(void);
void tp2_reset(void);

void tp2_prepare(void);

#endif
