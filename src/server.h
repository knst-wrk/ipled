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

#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>
#include <stdint.h>

#define SRV_OK              100
#define SRV_ILL_ARG         401
#define SRV_NO_NODE         404

bool srv_serve(void);
bool srv_handle(void);

void srv_printf(const char *fmt, ...);

void srv_enable(bool enable);
void srv_prepare(void);

#endif
