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
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#ifndef _USB_H_
#define _USB_H_

#include "config.h"
#include "kernel.h"
#include "button.h"

/** USB introduction
 * Targets which do not have any hardware support for USB, and cannot even detect
 * it must define USB_NONE. Otherwise, they must at least implement USB
 * detection.
 *
 * USB architecture
 * The USB code is split into several parts:
 * - usb: manages the USB connection
 * - usb_core: implements a software USB stack based on usb_drv
 * - usb_drv: implements the USB protocol based on some hardware transceiver/core
 * - usb_{hid,storage,...}: implement USB functionalities based on usb_core
 * Note that not all those are compiled in, in particular in the case of a
 * hardware USB stack, or when the driver doesn't support all types of transfers.
 *
 * Software versus hardware USB stack
 * A very important thing to keep in mind is that there are two very different
 * situations:
 * - software USB stack: the device only provides a USB transceiver and the
 *   USB stack must be implemented entirely in software. In this case the target
 *   must define HAVE_USBSTACK, correctly set CONFIG_USBOTG and implement a driver
 *   for the transceiver.
 * - hardware USB stack: the device has a dedicated chip which implements the
 *   USB stack in hardware. In this case the target must *NOT* define HAVE_USBSTACK
 *   but can still define CONFIG_USBOTG and implement a driver to enable/disable
 *   the USB hardware.
 *
 * USB ignore buttons
 * In some cases, the user wants to prevent Rockbox from entering USB mode. It
 * can do so by holding a button while inserting the cable. By default any button
 * will prevent the USB mode from kicking-in, so targets can optionally define
 * USBPOWER_BTN_IGNORE to a mask of buttons to ignore in this check.
 *
 * USB states
 * It is important to understand that the usb code can be in one of three states:
 * - extracted: no USB cable is plugged
 * - powered-only: a USB cable is plugged but the USB mode will not be entered,
 *   either because no host was detected or because the user requested so.
 * - inserted: a USB cable is plugged and the USB mode has been entered, either
 *   the software or hardware stack is running.
 *
 * USB exclusive mode
 * Either in hardware or software stack, if the USB was configured to run in
 * mass storage mode, it will require exclusive access to the disk and ask all
 * threads to release any file handle and stop using the disks. It does so by
 * broadcasting a SYS_USB_CONNECTED message, which threads must acknowledge using
 * usb_acknowledge(SYS_USB_CONNECTED_ACK, ev.data). They must not access the disk
 * until SYS_USB_DISCONNECTED is broadcast. To ease waiting, threads can call
 * usb_wait_for_disconnect() or usb_wait_for_disconnect_w_tmo() on their waiting
 * queue.
 *
 * USB detection
 * Except when no usb code is compiled at all (USB_NONE), the usb thread keeps
 * track of the USB insertion state, which can be either USB_INSERTED (meaning
 * 5v is present) or USB_EXTRACTED. Each target must implement usb_detect()
 * to report the insertion state.
 * Targets which support insertion/extraction interrupts must define
 * USB_STATUS_BY_EVENT and notify the thread on changes by calling
 * usb_status_event() with the new state. Other targets must *not* define
 * USB_STATUS_BY_EVENT and the usb thread by regularly poll the insertion state
 * using usb_detect().
 *
 * USB powering & charging
 * Device which can be powered from USB must define HAVE_USB_POWER. Note that
 * powering doesn't imply charging (for example a AA-powered device can be
 * powered from USB but not charged), charging sources are reported by the
 * power subsystem (see power.h). The USB specification mandates the maximum
 * current which can be drawn under which cirmcunstances. Device which cannot
 * control the charge current should make sure it is always <100mA to meet the
 * USB specification. Device with configurable charging current which support
 * >=100mA must define HAVE_USB_CHARGING_ENABLE and implement
 * usb_charging_maxcurrent_change() to let the usb thread control the maximum
 * charging control.
 * */


/* Messages from usb_tick and thread states */
enum
{
    USB_SCREENDUMP = -1,     /* State */
    USB_EXTRACTED = 0,       /* Event+State */
    USB_INSERTED,            /* Event+State */
    USB_POWERED,             /* State - transitional indicator if no host */
#if (CONFIG_STORAGE & STORAGE_MMC)
    USB_REENABLE,            /* Event */
#endif
#ifdef USB_FIREWIRE_HANDLING
    USB_REQUEST_REBOOT,      /* Event */
#endif
    USB_QUIT,                /* Event */
};

/* Supported usb modes. */
enum
{
    USB_MODE_MASS_STORAGE,
    USB_MODE_CHARGE,
    USB_MODE_ADB
};



/* initialise the usb code and thread */
void usb_init(void) INIT_ATTR;
/* target must implement this to enable/disable the usb transceiver/core */
void usb_enable(bool on);
/* called after host has been detected */
void usb_attach(void);
/* enable usb detection monitoring; before this function is called, all usb
 * detection changes are ignored */
void usb_start_monitoring(void) INIT_ATTR;
void usb_close(void);
/* acknowledge usb connection, typically with SYS_USB_CONNECTED_ACK */
void usb_acknowledge(long id, intptr_t seqnum);
/* block the current thread until SYS_USB_DISCONNECTED has been broadcast */
void usb_wait_for_disconnect(struct event_queue *q);
/* same as usb_wait_for_disconnect() but with a timeout, returns 1 on timeout */
int usb_wait_for_disconnect_w_tmo(struct event_queue *q, int ticks);
/* check whether USB is plugged, note that this is the official value which has
 * been reported to the thread */
bool usb_inserted(void);
/* check whether USB is plugged, note that this is the raw hardware value */
int usb_detect(void);
#ifdef USB_STATUS_BY_EVENT
/* Notify USB insertion state (USB_INSERTED or USB_EXTRACTED) */
void usb_status_event(int current_status);
#endif

/* broadcast usb insertion event to enable exclusive storage */
void usb_request_exclusive_storage(void);
/* finish exclusive storage access if enabled and mount volumes */
void usb_release_exclusive_storage(void);

#ifdef USB_FIREWIRE_HANDLING
bool firewire_detect(void);
void usb_firewire_connect_event(void);
#endif




#if !defined(USB_NONE)
/* initialise the USB hardware, this is a one-time init and it should setup what
 * is necessary to do proper USB detection, and it should call usb_drv_startup()
 * to do the one-time initialisation of the USB driver */
void usb_init_device(void);
#endif

#endif
