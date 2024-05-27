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

#ifndef TTY_H
#define TTY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define TTYBUFF             128

typedef void (*tty_hook_t)(uint32_t status, uint8_t ch);
void tty_hook(tty_hook_t h);

void tty_puts(const char *buf);
void tty_putchar(char c);

void tty_baud(uint32_t baud);
void tty_enable(bool enable);

void tty_prepare(void);

#endif

