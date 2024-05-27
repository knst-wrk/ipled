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

#include <stdio.h>
#include <string.h>

#include "cmsis/stm32f10x.h"

#include "timeout.h"
#include "handler.h"
#include "analog.h"
#include "system.h"
#include "config.h"
#include "buffer.h"
#include "server.h"
#include "scene.h"
#include "rfio.h"
#include "leds.h"
#include "tpm2.h"
#include "dmx.h"
#include "tty.h"
#include "sd.h"
#include "ui.h"

#include "main.h"

#if 0
void HardFault_Handler(void) __USED;
void HardFault_Handler(void)
{
    printf("HARDFAULT");

    /*  volatile unsigned long stacked_r0 ;
  volatile unsigned long stacked_r1 ;
  volatile unsigned long stacked_r2 ;
  volatile unsigned long stacked_r3 ;
  volatile unsigned long stacked_r12 ;
  volatile unsigned long stacked_lr ;
  volatile unsigned long stacked_pc ;
  volatile unsigned long stacked_psr ;*/
  volatile unsigned long _CFSR ;
  volatile unsigned long _HFSR ;
  volatile unsigned long _DFSR ;
  volatile unsigned long _AFSR ;
  volatile unsigned long _BFAR ;
  volatile unsigned long _MMAR ;

  /*stacked_r0 = ((unsigned long)hardfault_args[0]) ;
  stacked_r1 = ((unsigned long)hardfault_args[1]) ;
  stacked_r2 = ((unsigned long)hardfault_args[2]) ;
  stacked_r3 = ((unsigned long)hardfault_args[3]) ;
  stacked_r12 = ((unsigned long)hardfault_args[4]) ;
  stacked_lr = ((unsigned long)hardfault_args[5]) ;
  stacked_pc = ((unsigned long)hardfault_args[6]) ;
  stacked_psr = ((unsigned long)hardfault_args[7]) ;
*/
  // Configurable Fault Status Register
  // Consists of MMSR, BFSR and UFSR
  _CFSR = (*((volatile unsigned long *)(0xE000ED28))) ;

  // Hard Fault Status Register
  _HFSR = (*((volatile unsigned long *)(0xE000ED2C))) ;

  // Debug Fault Status Register
  _DFSR = (*((volatile unsigned long *)(0xE000ED30))) ;

  // Auxiliary Fault Status Register
  _AFSR = (*((volatile unsigned long *)(0xE000ED3C))) ;

  // Read the Fault Address Registers. These may not contain valid values.
  // Check BFARVALID/MMARVALID to see if they are valid values
  // MemManage Fault Address Register
  _MMAR = (*((volatile unsigned long *)(0xE000ED34))) ;
  // Bus Fault Address Register
  _BFAR = (*((volatile unsigned long *)(0xE000ED38))) ;

  __asm("BKPT #0\n") ; // Break into the debugger
}

#endif


static uint8_t index = 0;

/** Standalone mode.
In this mode a range of test patterns as well as DMX input is provided:
    hex     description
     0      all LEDs turned off.

     1      RGBW rainbows:
                string 1: full color rainbow
                string 2: red rainbow
                string 3: green rainbow
                string 4: blue rainbow
                string 5: white rainbow
                string 6: reversed full color rainbow

     2      constant red at very low brightness

     3      constant green at very low brightness

     4      constant blue at very low brightness

     5      constant white at full brightness
*/
static void standalone_task(void)
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    if (!led_capture())
        return;

    switch (ui_hex()) {
    /* LEDs off */
    default:
    case '0':
        led_clear();
        break;

    /* Color rainbows */
    case '1':
        for (uint16_t i = 0; i < MAXLEDS; i++) {
            uint8_t pos = index + i;
            if (pos < 85) {
                r = 255 - pos * 3;
                g = 0;
                b = pos * 3;
            }
            else if (pos < 170) {
                pos -= 85;
                r = 0;
                g = pos * 3;
                b = 255 - pos * 3;
            }
            else {
                pos -= 170;
                r = pos * 3;
                g = 255 - pos * 3;
                b = 0;
            }

            led_rgb(i, 0, r, g, b);
            led_rgb(i, 1, r, 0, 0);
            led_rgb(i, 2, 0, g, 0);
            led_rgb(i, 3, 0, 0, b);
            led_rgb(i, 4, r, r, r);
            led_rgb(MAXLEDS - 1 - i, 5, r, g, b);
        }
        break;

    /* Static R/G/B or white */
    case '2': r = 10; goto unicolor;
    case '3': g = 10; goto unicolor;
    case '4': b = 10; goto unicolor;
    case '5': r = g = b = 255; goto unicolor;
    unicolor:
        for (uint16_t i = 0; i < MAXLEDS; i++) {
            for (uint8_t string = 0; string < 6; string++)
                led_rgb(i, string, r, g, b);
        }

        break;
    }

    led_release();
    ui_led(index++ & 1);
}

/** Direct TPM2 input mode.
     9      direct TPM2 input:
                string 1:   Every three consecutive bytes in the TPM2 data
                            stream map to the red, green and blue intensities
                            respectively of the LEDs.
*/
static void tpm2_task(void)
{
    /* Direct TPM2 input */
    if (tp2_detect()) {
        if (tp2_trip()) {
            if (led_capture()) {
                led_maps();
                led_release();
                tp2_clear();

                ui_led(index++ & 1);
            }
        }
    }
    else {
        if (led_capture()) {
            cfg_default();
            led_release();

            ui_led(false);
        }
    }
}

/** Direct DMX input mode.
     D      direct DMX input in various configurations:
                string 1:   Every three consecutive DMX channels map to the red,
                            green and blue intensities of the LEDs, starting at
                            the first DMX channel.

                string 2:   The DMX channels control the red intensity.

                string 3:   The DMX channels control the green intensity.

                string 4:   The DMX channels control the blue intensity.

                string 5:   The DMX channels control the white intensity (red+green+blue)

                string 6:   The first three DMX channels control the red, green
                            and blue intensities of all the LEDs.
*/
static void dmx_task(void)
{
    /* Direct DMX input */
    if (dmx_detect()) {
        if (dmx_trip()) {
            if (led_capture()) {
                led_maps();
                led_release();
                dmx_clear();

                ui_led(index++ & 1);
            }
        }
    }
    else {
        if (led_capture()) {
            cfg_default();
            led_release();

            ui_led(false);
        }
    }
}

static void scene_task(void)
{
    static uint8_t scene = 0xFF;
    uint8_t hex = ui_hex();
    if (hex <= '9')
        hex = hex - '0';
    else
        hex = hex - 'A' + 10;

    if (scene != hex)
        sc_start( (scene = hex) );

    if (sc_play()) {
        ui_led(index++ & 4);
    }
    else {
        if (led_capture()) {
            cfg_default();
            led_release();

            ui_led(false);
        }
    }
}


static void tx_task(void)
{
    if (srv_serve())
        ui_led(index++ & 1);
}

static void rx_task(void)
{
    if (hnd_handle())
        ui_led(index++ & 1);
}

static void beacon_task(void)
{
    if (led_capture()) {
        const uint8_t bcn[] = { 0xBA, 0xDA, 0x55, index };
        rf_sendto(0, bcn, sizeof(bcn)/sizeof(*bcn));
        while (!rf_sent());

        index++;
        ui_led(index & 1);
        for (uint16_t i = 0; i < MAXLEDS; i++) {
            for (uint8_t string = 0; string < 6; string++)
                led_rgb(i, string, (index & 1) ? 16 : 0, 0, 0);
        }

        led_release();
    }
}

int main(void)
{
    sys_prepare();
    rf_prepare();
    sys_hse();

    tot_prepare();
    ui_prepare();
    led_prepare();
    tty_prepare();
    dmx_prepare();
    tp2_prepare();
    sd_prepare();
    ad_prepare();
    cfg_prepare();
    sc_prepare();
    srv_prepare();
    hnd_prepare();

    /* Clear prevalent data in LEDs */
    led_enable(true);
    tot_delay(500);
    for (int i = 0; i < 3; i++) {
        led_universe();
        while (!led_busy());
    }
    led_enable(false);

    uint8_t mode = config.mode.mode;
    if (mode == no_mode) {
        /* No SD card present */
        led_enable(true);
        switch (ui_hex()) {
            default:
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
                led_framerate(20);
                led_dim(0xFF, 0xFF, 0xFF);
                led_length(MAXLEDS);

                mode = standalone_mode;
                break;

            case '9':
                config.leds.map[0] = (struct led_map_t) {
                    .string = 0,
                    .begin = 0,
                    .end = MAXBUFF/3 - 1,
                    .step = 1,

                    .red = {    .begin = 0, .end = MAXBUFF, .step = 3 },
                    .green = {  .begin = 1, .end = MAXBUFF, .step = 3 },
                    .blue = {   .begin = 2, .end = MAXBUFF, .step = 3 },

                    .flags = 0
                };

                config.leds.map[1].string = 0xFF;

                led_framerate(0);
                led_dim(0xFF, 0xFF, 0xFF);
                led_length(MAXLEDS);

                mode = tpm2_mode;
                break;

            case 'D':
                config.leds.map[0] = (struct led_map_t) {
                    .string = 0,
                    .begin = 0,
                    .end = MAXDMX/3 - 1,
                    .step = 1,

                    .red = {    .begin = 0, .end = MAXDMX,  .step = 3 },
                    .green = {  .begin = 1, .end = MAXDMX,  .step = 3 },
                    .blue = {   .begin = 2, .end = MAXDMX,  .step = 3 },

                    .flags = 0
                };

                config.leds.map[1] = (struct led_map_t) {
                    .string = 1,
                    .begin = 0,
                    .end = MAXDMX - 1,
                    .step = 1,

                    .red = {    .begin = 0, .end = MAXDMX,  .step = 1 },
                    .green = {  .value = 0,                 .step = 0 },
                    .blue = {   .value = 0,                 .step = 0 },

                    .flags = MAP_STATIC_GREEN | MAP_STATIC_BLUE
                };

                config.leds.map[2] = (struct led_map_t) {
                    .string = 2,
                    .begin = 0,
                    .end = MAXDMX - 1,
                    .step = 1,

                    .red = {    .value = 0,                 .step = 0 },
                    .green = {  .begin = 0, .end = MAXDMX,  .step = 1 },
                    .blue = {   .value = 0,                 .step = 0 },

                    .flags = MAP_STATIC_RED | MAP_STATIC_BLUE
                };

                config.leds.map[3] = (struct led_map_t) {
                    .string = 3,
                    .begin = 0,
                    .end = MAXDMX - 1,
                    .step = 1,

                    .red = {    .value = 0,                 .step = 0 },
                    .green = {  .value = 0,                 .step = 0 },
                    .blue = {   .begin = 0, .end = MAXDMX,  .step = 1 },

                    .flags = MAP_STATIC_RED | MAP_STATIC_GREEN
                };

                config.leds.map[4] = (struct led_map_t) {
                    .string = 4,
                    .begin = 0,
                    .end = MAXDMX - 1,
                    .step = 1,

                    .red = {    .begin = 0, .end = MAXDMX,  .step = 1 },
                    .green = {  .begin = 0, .end = MAXDMX,  .step = 1 },
                    .blue = {   .begin = 0, .end = MAXDMX,  .step = 1 },

                    .flags = 0
                };

                config.leds.map[5] = (struct led_map_t) {
                    .string = 5,
                    .begin = 0,
                    .end = MAXDMX - 1,
                    .step = 1,

                    .red = {    .begin = 0, .end = 0,       .step = 0 },
                    .green = {  .begin = 1, .end = 1,       .step = 0 },
                    .blue = {   .begin = 2, .end = 2,       .step = 0 },

                    .flags = 0
                };

                config.leds.map[6].string = 0xFF;

                led_framerate(0);
                led_dim(0xFF, 0xFF, 0xFF);
                led_length(MAXLEDS);

                mode = dmx_mode;
                break;


            case '6': mode = rx_mode; break;
            case '7': mode = tx_mode; break;
        }
    }

    void (*task)(void) = 0;
    switch (mode) {
        default:
        case standalone_mode:
            led_enable(true);
            task = &standalone_task;
            break;

        case scene_mode:
            led_enable(true);
            task = &scene_task;
            break;

        case tpm2_mode:
            tp2_enable(true);
            tty_enable(true);
            led_enable(true);
            task = tpm2_task;
            break;

        case dmx_mode:
            dmx_enable(true);
            tty_enable(true);
            led_enable(true);
            task = &dmx_task;
            break;

        /* RF modes with SD card only */
        case tx_mode:
            rf_nodeid(0);
            rf_enable(true);
            tty_enable(true);
            srv_enable(true);
            task = &tx_task;
            break;

        case rx_mode:
            rf_enable(true);
            task = &rx_task;
            break;

        case beacon_mode:
            led_enable(true);
            rf_nodeid(0);
            rf_enable(true);
            task = &beacon_task;
            break;
    }

    if (!task)
        for (;;);

    for (;;) {
        ui_debounce();
        ad_convert();

        (*task)();
    }
}
