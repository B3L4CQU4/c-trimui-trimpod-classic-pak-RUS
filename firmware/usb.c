/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 by Linus Nielsen Feltzing
 *
 * iPod driver based on code from the ipodlinux project - http://ipodlinux.org
 * Adapted for Rockbox in January 2006
 * Original file: podzilla/usb.c
 * Copyright (C) 2005 Adam Johnston
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
#include "config.h"
#include "cpu.h"
#include "kernel.h"
#include "thread.h"
#include "system.h"
#include "debug.h"
#include "storage.h"
#include "disk.h"
#include "panic.h"
#include "lcd.h"
#include "usb.h"
#include "button.h"
#include "string.h"
#include "logf.h"
#include "screendump.h"
#include "powermgmt.h"

#include "misc.h"
#include "gui/yesno.h"
#include "settings.h"
#include "lang_enum.h"
#include "gui/skin_engine/skin_engine.h"



#define USB_FULL_INIT

/* USB detect debouncing interval (200ms taken from the usb polling code) */
#define USB_DEBOUNCE_POLL (200*HZ/1000)
/* NOTE: "usb_dw_gonak_effective:failed!" *PANIC*s can be observed when
         disconnecting the FiiO M3K from USB with the debounce interval
         for USB_STATUS_BY_EVENT set to 200ms, as above (see e75a3fb).
         Adjusting the interval to 10ms reduces likelihood of a panic. */
#define USB_DEBOUNCE_TIME (10*HZ/1000)

bool do_screendump_instead_of_usb = false;

/* Dummy functions for USB_NONE  */

bool usb_inserted(void)
{
    return false;
}

void usb_acknowledge(long id, intptr_t seqnum)
{
    (void)id;
    (void)seqnum;
}

void usb_init(void)
{
}

void usb_start_monitoring(void)
{
}

int usb_detect(void)
{
    return USB_EXTRACTED;
}

void usb_wait_for_disconnect(struct event_queue *q)
{
   (void)q;
}
