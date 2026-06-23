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
#ifndef _POWER_TARGET_H_
#define _POWER_TARGET_H_

#include <stdbool.h>
#include "config.h"

unsigned int power_get_battery_capacity(void);
/* Trimpod CPU Frequency (Settings -> Power -> CPU). power-target.c is the single
 * owner: defines, persists (cpu_freq.txt) and applies the choice. launch.sh only
 * saves/restores the system's original cpufreq state. */
void retrohh_cpu_set_freq(int khz);   /* pin CPU to khz (performance) */
void retrohh_cpu_set_dynamic(void);   /* Dynamic: tuned interactive, full range */
int  retrohh_cpu_get_freq(void);      /* current scaling_cur_freq in khz */
bool retrohh_cpu_is_dynamic(void);    /* true unless pinned (performance) */
void retrohh_cpu_save_choice(int khz);/* persist choice (khz<=0 -> dynamic) */
void retrohh_cpu_apply_saved(void);   /* apply persisted choice (call at startup) */
#endif /* _POWER_RHH_H_ */
