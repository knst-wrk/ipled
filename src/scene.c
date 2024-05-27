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

#include <stdint.h>
#include <stdlib.h>

#include "ff/ff.h"

#include "config.h"
#include "tpm2.h"
#include "leds.h"
#include "timeout.h"

#include "scene.h"

static union
{
    struct {
        uint8_t buf[128];
        FIL f;
        UINT br;
        UINT bp;
    } tpm2;

    struct {
        timeout_t timeout;
        bool expired;
    } pause;

    struct {
        FSIZE_t map_;
    } map;

    struct {
        uint16_t fps;
    } framerate;

    struct {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } dim;
} arg;


struct command_proc_t
{
    bool (*play_proc)(void);
    void (*stop_proc)(void);
};

enum command_t
{
    stop_command = 0,

    tpm2_command,
    pause_command,
    map_command,
    framerate_command,
    dim_command,
};

static uint16_t scene;
static FSIZE_t pos;

/* command < 0 ---> paused */
static int command;


/******************************************************************************
 * Stop
 */
static bool play_stop(void)
{
    /* Dead end.
    This intentionally does not led_clear() or led_enable() as it is used for
    static display as well. */
    return true;
}

void sc_do_stop(void)
{
    sc_skip();
    led_enable(false);

    command = stop_command;
}

/******************************************************************************
 * TPM2
 */
static bool play_tpm2(void)
{
    if (tp2_trip()) {
        /* Synchronize to frame generator */
        if (led_capture()) {
            led_maps();
            led_release();
            tp2_clear();
            return true;
        }
    }
    else {
        /* Digest more TPM2 data until one frame is complete */
        do {
            if (!arg.tpm2.br) {
                arg.tpm2.bp = 0;
                arg.tpm2.br = sizeof(arg.tpm2.buf)/sizeof(*arg.tpm2.buf);
                if (f_read(&arg.tpm2.f, arg.tpm2.buf, arg.tpm2.br, &arg.tpm2.br) != FR_OK)
                    return false;
            }

            size_t digested = tp2_digest(&arg.tpm2.buf[arg.tpm2.bp], arg.tpm2.br);
            arg.tpm2.bp += digested;
            arg.tpm2.br -= digested;
            if (tp2_trip())
                break;
        } while (arg.tpm2.br || !f_eof(&arg.tpm2.f));
    }

    return tp2_trip();
}

static void stop_tpm2(void)
{
    led_enable(false);
    f_close(&arg.tpm2.f);
}

void sc_do_tpm2(const char *const file)
{
    sc_skip();
    led_enable(true);

    FRESULT fr = f_open(&arg.tpm2.f, (TCHAR *) file, FA_READ);
    if (fr == FR_OK) {
        tp2_reset();
        arg.tpm2.br = 0;
        command = tpm2_command;
    }
}


/******************************************************************************
 * Pause
 */
static bool play_pause(void)
{
    if (!arg.pause.expired && !tot_expired(arg.pause.timeout))
        return true;

    arg.pause.expired = true;
    return false;
}

void sc_do_pause(uint32_t t)
{
    sc_skip();

    arg.pause.expired = false;
    arg.pause.timeout = tot_set(t);
    command = pause_command;
}


/******************************************************************************
 * Map
 */
static bool play_map(void)
{
    if (!led_capture())
        return true;

    cfg_map(arg.map.map_);
    led_release();
    return false;
}

static void stop_map(void)
{
    led_enable(false);
}

void sc_do_map(FSIZE_t map_)
{
    sc_skip();
    led_enable(true);

    arg.map.map_ = map_;
    command = map_command;
}


/******************************************************************************
 * Framerate
 */
static bool play_framerate(void)
{
    if (!led_capture())
        return true;

    led_framerate(arg.framerate.fps);
    led_release();
    return false;
}

void sc_do_framerate(uint16_t fps)
{
    sc_skip();

    arg.framerate.fps = fps;
    command = framerate_command;
}


/******************************************************************************
 * Dim
 */
static bool play_dim(void)
{
    if (!led_capture())
        return true;

    led_dim(arg.dim.red, arg.dim.green, arg.dim.blue);
    led_release();
    return false;
}

void sc_do_dim(uint8_t red, uint8_t green, uint8_t blue)
{
    sc_skip();

    arg.dim.red = red;
    arg.dim.green = green;
    arg.dim.blue = blue;
    command = dim_command;
}



static const struct command_proc_t commands[] = {
    [stop_command] = {
        .play_proc = &play_stop,
        .stop_proc = 0
    },

    [tpm2_command] = {
        .play_proc = &play_tpm2,
        .stop_proc = &stop_tpm2
    },

    [pause_command] = {
        .play_proc = &play_pause,
        .stop_proc = 0
    },

    [map_command] = {
        .play_proc = &play_map,
        .stop_proc = &stop_map,
    },

    [framerate_command] = {
        .play_proc = &play_framerate,
        .stop_proc = 0
    },

    [dim_command] = {
        .play_proc = &play_dim,
        .stop_proc = 0
    },
};

bool sc_play(void)
{
    if (command < 0)
        /* Pause */
        return false;

    if (commands[command].play_proc) {
        if ( (*commands[command].play_proc)() ) {
            /* Run command until it terminates */
            return true;
        }
        else {
            /* Stop command */
            sc_skip();
        }
    }

    /* Request next command */
    if (pos) {
        pos = cfg_command(pos);
        return true;
    }
    else {
        return false;
    }
}


bool sc_start(uint16_t s)
{
    if (scene != s)
        pos = 0;

    if (!pos) {
        /* Start scene */
        sc_skip();
        scene = s;
        pos = cfg_scene(scene);
        if (pos)
            pos = cfg_command(pos);
    }
    else {
        /* Continue scene */
        command = abs(command);

        /* Compensate for erratic timing */
        switch (command) {
        case pause_command:
            arg.pause.expired = true;
            break;

        default:
            break;
        }
    }

    return pos;
}

void sc_pause(void)
{
    if (command > 0)
        command = -command;
}

void sc_stop(void)
{
    sc_skip();
    pos = 0;

    sc_do_stop();
}

void sc_skip(void)
{
    if (command) {
        command = abs(command);
        if (commands[command].stop_proc)
            (*commands[command].stop_proc)();
        command = 0;
    }
}


void sc_prepare(void)
{
    scene = 0;
    pos = 0;

    command = stop_command;
}

