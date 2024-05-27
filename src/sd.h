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

#ifndef SD_H
#define SD_H

#include <stdint.h>
#include <stdbool.h>

#define SD_INITIALIZATION_TIMEOUT   1000
#define SD_SELECT_TIMEOUT           500
#define SD_READ_TIMEOUT             200
#define SD_WRITE_TIMEOUT            200
#define SD_RETRIES                  3

#define SD_HARDWARE_CRC

void sd_enable(bool enable);
bool sd_identify(void);

uint8_t sd_type(void);
#define SD_NONE                     0x00
#define SD_SDv1                     0x01
#define SD_SDv2                     0x02
#define SD_MMC                      0x04

bool sd_ocr(uint32_t *ocr);
bool sd_csd(uint8_t *csd);
bool sd_sync(void);

bool sd_read(uint32_t sector, uint8_t *data, uint16_t n);
bool sd_write(uint32_t sector, const uint8_t *data, uint16_t n);

void sd_prepare(void);

#endif
