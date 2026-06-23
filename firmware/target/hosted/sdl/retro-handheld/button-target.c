/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2025 Hairo R. Carela
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/


#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <SDL.h>
#include "button.h"
#include "button-target.h"

/* Hold/lock switch (Brick side toggle = gpio243, value 1 = engaged, same line
 * NextUI reads). Engaged -> button_read_device() returns BUTTON_NONE. */
#define HOLD_SWITCH_PATH "/sys/class/gpio/gpio243/value"

bool retrohh_hold_switch(void)
{
    static int fd = -2;            /* -2 = not opened, -1 = unavailable */
    if (fd == -2)
        fd = open(HOLD_SWITCH_PATH, O_RDONLY);
    if (fd < 0)
        return false;

    char v = '0';
    if (pread(fd, &v, 1, 0) < 1)   /* sysfs: re-read from offset 0 each poll */
        return false;
    return v == '1';               /* 1 = engaged -> block input */
}

/* Hardware volume rocker.
 * On the Brick the rocker is NOT a dedicated key device: it emits
 * KEY_VOLUMEUP / KEY_VOLUMEDOWN on the *gamepad* evdev node ("TRIMUI Player1").
 * gptokeyb2 claims that node as an SDL game controller, and game controllers
 * have no notion of volume keys, so those presses are dropped before they can
 * reach us as SDL key events.  So -- exactly like NextUI's keymon -- we read
 * the evdev node(s) directly (shared, no EVIOCGRAB so we don't steal the pad
 * buttons) and translate the volume keys into BUTTON_VOL_UP / BUTTON_VOL_DOWN.
 * action.c's global volume handler does the rest. */
#define VOL_BITS_PER_LONG (8 * (int)sizeof(long))
#define VOL_NLONGS(b)     (((b) + VOL_BITS_PER_LONG - 1) / VOL_BITS_PER_LONG)
#define VOL_TEST_BIT(a,b) (((a)[(b) / VOL_BITS_PER_LONG] >> ((b) % VOL_BITS_PER_LONG)) & 1)
#define VOL_MAX_FDS 8

static void vol_open_devices(int *fds, int *nfds)
{
    *nfds = 0;
    for (int i = 0; i < 12 && *nfds < VOL_MAX_FDS; i++)
    {
        char path[32];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;

        /* keep only nodes capable of emitting the volume keys */
        unsigned long keybits[VOL_NLONGS(KEY_MAX + 1)];
        memset(keybits, 0, sizeof(keybits));
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) >= 0 &&
            (VOL_TEST_BIT(keybits, KEY_VOLUMEUP) ||
             VOL_TEST_BIT(keybits, KEY_VOLUMEDOWN)))
            fds[(*nfds)++] = fd;
        else
            close(fd);
    }
}

int retrohh_read_volume_rocker(void)
{
    static int fds[VOL_MAX_FDS];
    static int nfds = -1;          /* -1 = not yet scanned */
    static int state = 0;          /* currently-held BUTTON_VOL_* bits */

    if (nfds < 0)
        vol_open_devices(fds, &nfds);

    struct input_event ev;
    for (int i = 0; i < nfds; i++)
    {
        while (read(fds[i], &ev, sizeof(ev)) == (int)sizeof(ev))
        {
            int bit;
            if (ev.type != EV_KEY)
                continue;
            if (ev.code == KEY_VOLUMEUP)
                bit = BUTTON_VOL_UP;
            else if (ev.code == KEY_VOLUMEDOWN)
                bit = BUTTON_VOL_DOWN;
            else
                continue;

            if (ev.value)          /* press (1) or autorepeat (2) */
                state |= bit;
            else                   /* release (0) */
                state &= ~bit;
        }
    }
    return state;
}

int key_to_button(int keyboard_key)
{
    int new_btn = BUTTON_NONE;
    switch (keyboard_key)
    {
        case SDLK_l:
            new_btn = BUTTON_A;
            break;
        case SDLK_k:
            new_btn = BUTTON_B;
            break;
        case SDLK_i:
            new_btn = BUTTON_X;
            break;
        case SDLK_j:
            new_btn = BUTTON_Y;
            break;
        case SDLK_w:
            new_btn = BUTTON_UP;
            break;
        case SDLK_s:
            new_btn = BUTTON_DOWN;
            break;
        case SDLK_a:
            new_btn = BUTTON_LEFT;
            break;
        case SDLK_d:
            new_btn = BUTTON_RIGHT;
            break;
        case SDLK_g:
            new_btn = BUTTON_START;
            break;
        case SDLK_f:
            new_btn = BUTTON_SELECT;
            break;
        case SDLK_q:
            new_btn = BUTTON_L;
            break;
        case SDLK_e:
            new_btn = BUTTON_R;
            break;
        case SDLK_z:
            new_btn = BUTTON_L2;
            break;
        case SDLK_c:
            new_btn = BUTTON_R2;
            break;
        /* Bluetooth media controls */
        case SDLK_AUDIOPLAY:
            new_btn = RC_BUTTON_PLAY;
            break;
        case SDLK_AUDIOPREV:
            new_btn = RC_BUTTON_PREVSONG;
            break;
        case SDLK_AUDIONEXT:
            new_btn = RC_BUTTON_NEXTSONG;
            break;
        /* Hardware volume rocker (TrimUI Brick: sunxi-keyboard) */
        case SDLK_VOLUMEUP:
            new_btn = BUTTON_VOL_UP;
            break;
        case SDLK_VOLUMEDOWN:
            new_btn = BUTTON_VOL_DOWN;
            break;
        default:
            break;
        /* SDLK_x is used for shutdown */
        /* SDLK_h is used for hold */
    }
    return new_btn;
}
