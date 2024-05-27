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

#ifndef HANDLER_H
#define HANDLER_H

#include <stdbool.h>
#include <stdint.h>

#define HND_TIMEOUT         500


bool hnd_sleep(uint8_t id);
bool hnd_wake(uint8_t id);

bool hnd_ping(uint8_t id, uint16_t *vbat, int16_t *rssi, int16_t *temp);

bool hnd_start(uint8_t id, uint16_t scene);
bool hnd_pause(uint8_t id);
bool hnd_skip(uint8_t id);
bool hnd_stop(uint8_t id);

bool hnd_frame(uint8_t id);
bool hnd_finger(uint8_t id, uint32_t *uid, uint16_t *hv, uint16_t *sv);
bool hnd_dim(uint8_t id, uint8_t red, uint8_t green, uint8_t blue);
bool hnd_tpm2(uint8_t id, uint8_t *buf, uint16_t length);

bool hnd_handle(void);

void hnd_prepare(void);

#endif
