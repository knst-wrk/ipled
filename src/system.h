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

#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

#include "cmsis/stm32f10x.h"
#include "cmsis/core_cm3.h"

#define RTC_RCOSC   40000

void sys_hse(void);
void sys_hsi(void);

#define VCC_LED         0x01
#define VCC_TTY         0x02
void sys_vcc(uint8_t on, uint8_t off);

void sys_stop(void);
void sys_alarm(uint32_t dt);
uint32_t sys_time(void);


void panic(void);



/* Following the scheme in the original CMSIS headers */
typedef struct
{
  __IO uint16_t U_ID0;
  __IO uint16_t U_ID1;
  __IO uint32_t U_ID23;
  __IO uint32_t U_ID45;
} U_ID_TypeDef;

#define U_ID_BASE           (0x1FFFF7E8)
#define U_ID                ((U_ID_TypeDef *) U_ID_BASE)

void sys_stat(void);
uint32_t sys_uid(void);

void sys_prepare(void);

#endif
