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

#ifndef LD_H
#define LD_H

/* End of stack */
extern void *_estack;

/* Data sections to be copied to RAM */
extern void *__data_regions_array_start;
extern void *__data_regions_array_end;

/* BSS sections to be zeroed */
extern void *__bss_regions_array_start;
extern void *__bss_regions_array_end;

/* ARM-style initialization and finalization function pointer tables */
extern void *__preinit_array_start;
extern void *__preinit_array_end;
extern void *__init_array_start;
extern void *__init_array_end;
extern void *__fini_array_start;
extern void *__fini_array_end;

/* Program break */
extern void *end;

#endif
