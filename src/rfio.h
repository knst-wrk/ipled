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

#ifndef RFIO_H
#define RFIO_H

#include <stdint.h>
#include <stdbool.h>

#define RF_XTAL             32000000

#define RF_AFC_TIMEOUT      30000
#define RF_TX_TIMEOUT       1000

#define MAXPACK     60

void rf_calibrate(void);
void rf_frequency(uint32_t f);
void rf_rxbw(uint32_t bandwidth);
void rf_afcbw(uint32_t bandwidth);
void rf_fdev(uint32_t dev);
void rf_bitrate(uint16_t rate);
void rf_power(int8_t power);
void rf_sensitivity(int16_t sens);
int16_t rf_rssi(void);
int32_t rf_fei(void);

void rf_meshid(uint16_t id);
void rf_nodeid(uint8_t id);
void rf_promiscuous(bool p);

bool rf_trip(void);
void rf_sendto(uint8_t to, const uint8_t *msg, uint8_t length);
bool rf_sent(void);
uint8_t rf_receive(uint8_t *msg, uint8_t *length);
bool rf_received(void);
void rf_enable(bool enable);
void rf_listen(uint16_t idle, uint16_t rx);

void rf_configure(void);
void rf_prepare(void);

#ifdef DEBUG
void rf_debug(void);
#endif

#endif
