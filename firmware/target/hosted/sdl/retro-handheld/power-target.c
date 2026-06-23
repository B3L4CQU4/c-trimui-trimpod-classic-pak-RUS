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
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "system.h"
#include "power-target.h"
#include "power.h"
#include "powermgmt.h"
#include "panic.h"
#include "sysfs.h"

#include "tick.h"

#ifdef HAVE_TRIMUI_SAFE_POWEROFF
/* The NextUI/MinUI launcher runs poweroff_next (safe AXP2202 PMIC shutdown:
 * kill SD users -> sync -> unmount -> PMIC off) when it finds this flag after
 * a pak exits. So just drop the flag; the real poweroff happens once we quit. */
void trimui_safe_poweroff(void)
{
    int fd = open("/tmp/poweroff", O_WRONLY | O_CREAT, 0644);
    if (fd >= 0)
        close(fd);
}
#endif

#ifdef HAVE_TRIMUI_DEEP_SLEEP
/* Run NextUI's own `suspend` helper ($SYSTEM_PATH/bin/suspend): stops wifi/bt,
 * `echo mem > /sys/power/state`, restores on resume. Blocks until the power
 * button (the only enabled wake source) wakes the device. */
void trimui_deep_sleep(void)
{
    const char *sys = getenv("SYSTEM_PATH");
    char cmd[256];
    if (!sys || !*sys)
        sys = "/mnt/SDCARD/.system/tg5040";   /* Brick default if env is bare */
    snprintf(cmd, sizeof cmd, "'%s/bin/suspend'", sys);
    system(cmd);
}
#endif

/* We get called multiple times per tick, let's cut that back! */
static long last_tick = 0;
static bool last_power = false;

static char* get_path(const char *env_key)
{
    char *p = getenv(env_key);
    return (p && *p) ? p : NULL;
}

bool charging_state(void)
{
    if ((current_tick - last_tick) > HZ/2 ) {
        char buf[12] = {0};
        
        char *path = get_path("BATTERY_STATUS");

        if (path)
            sysfs_get_string(path, buf, sizeof(buf));

        last_tick = current_tick;
        last_power = (strncmp(buf, "Charging", 8) == 0);
    }
    return last_power;
}

unsigned int power_input_status(void)
{
    int present = 0;
    
    char *path = get_path("POWER_STATUS");

    if (path)
        sysfs_get_int(path, &present);

    return present ? POWER_INPUT_USB_CHARGER : POWER_INPUT_NONE;
}

unsigned int power_get_battery_capacity(void)
{
    int battery_level;

    char *path = get_path("CAPACITY_STATUS");

    if (path)
        sysfs_get_int(path, &battery_level);

    return battery_level;
}

/* Trimpod: CPU frequency control (TrimUI Brick / Allwinner A133).
 * Pins the policy to a single frequency via scaling_min/max_freq. This is a
 * runtime-only change -- launch.sh saves the system cpufreq state and restores
 * it when Trimpod exits, so it never affects the device permanently. */
#define CPUFREQ_POLICY "/sys/devices/system/cpu/cpufreq/policy0"
#define CPUFREQ_INT    "/sys/devices/system/cpu/cpufreq/interactive"
#define CPU_FREQ_FILE  ROCKBOX_DIR "/cpu_freq.txt"
#define CPU_FREQ_MIN_KHZ  408000   /* idle floor for Dynamic */
#define CPU_FREQ_MAX_KHZ  2000000  /* A133 top step */

/* This file is the SINGLE owner of the CPU Frequency policy (Settings -> Power
 * -> CPU): the two modes are defined here once, persisted here, and applied here
 * both at startup (retrohh_cpu_apply_saved) and on a live menu change. launch.sh
 * only saves the system's original cpufreq state on entry and restores it on
 * exit -- it sets no governor/frequency of its own. */

/* "Pinned": LOCK the cores at a single fixed frequency by collapsing the scaling
 * window to one value (scaling_min == scaling_max == khz). The `performance`
 * governor then simply holds scaling_max_freq -- which is khz, not 2 GHz -- so the
 * clock is fixed at the chosen step (no scaling up or down). */
void retrohh_cpu_set_freq(int khz)
{
    if (khz <= 0)
        return;
    char gov[] = "performance";          /* holds scaling_max_freq, = khz here */
    sysfs_set_string(CPUFREQ_POLICY "/scaling_governor", gov);
    /* lower the floor first so min<=max always holds, then pin both to khz */
    sysfs_set_int(CPUFREQ_POLICY "/scaling_min_freq", CPU_FREQ_MIN_KHZ);
    sysfs_set_int(CPUFREQ_POLICY "/scaling_max_freq", khz);
    sysfs_set_int(CPUFREQ_POLICY "/scaling_min_freq", khz);
}

/* Trimpod "Dynamic": the tuned `interactive` governor across the full range --
 * idles at 408 MHz (cool) and ramps to 2 GHz only under sustained load. The
 * interactive tunables are global and reset to defaults whenever the governor is
 * (re)selected, so they are re-applied here every time: hispeed_freq caps the
 * load-jump and above_hispeed_delay slows the climb, so 30fps peak-meter bursts
 * don't spike to 2 GHz while the visualizer/decode still ramp. */
void retrohh_cpu_set_dynamic(void)
{
    char gov[] = "interactive";
    sysfs_set_int(CPUFREQ_POLICY "/scaling_min_freq", CPU_FREQ_MIN_KHZ);
    sysfs_set_int(CPUFREQ_POLICY "/scaling_max_freq", CPU_FREQ_MAX_KHZ);
    sysfs_set_string(CPUFREQ_POLICY "/scaling_governor", gov);
    /* tunables live under the governor's dir, present only once it is active */
    sysfs_set_int(CPUFREQ_INT "/target_loads", 95);
    sysfs_set_int(CPUFREQ_INT "/hispeed_freq", 1008000);
    sysfs_set_int(CPUFREQ_INT "/above_hispeed_delay", 20000);
    sysfs_set_int(CPUFREQ_INT "/timer_rate", 30000);
    sysfs_set_int(CPUFREQ_INT "/min_sample_time", 100000);
}

/* Persist the menu choice; read back by retrohh_cpu_apply_saved at next startup.
 * khz <= 0 -> "dynamic"; otherwise the pinned step in kHz. */
void retrohh_cpu_save_choice(int khz)
{
    int fd = open(CPU_FREQ_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return;
    char buf[16];
    int n = (khz <= 0) ? snprintf(buf, sizeof buf, "dynamic\n")
                       : snprintf(buf, sizeof buf, "%d\n", khz);
    if (n > 0)
        write(fd, buf, n);
    close(fd);
}

/* Apply the persisted CPU Frequency choice. Called once at app startup; the menu
 * applies live on change. Default (no saved file) is Dynamic. */
void retrohh_cpu_apply_saved(void)
{
    char buf[16] = {0};
    int khz = 0;                         /* 0 / "dynamic" -> Dynamic */
    int fd = open(CPU_FREQ_FILE, O_RDONLY);
    if (fd >= 0)
    {
        int r = read(fd, buf, sizeof buf - 1);
        close(fd);
        if (r > 0)
            khz = atoi(buf);             /* "dynamic" -> 0 */
    }
    if (khz > 0)
        retrohh_cpu_set_freq(khz);
    else
        retrohh_cpu_set_dynamic();
}

/* True when the CPU is in Dynamic mode (a scaling governor); pinned mode uses
 * "performance". */
bool retrohh_cpu_is_dynamic(void)
{
    char gov[24] = {0};
    if (!sysfs_get_string(CPUFREQ_POLICY "/scaling_governor", gov, sizeof gov))
        return false;
    return gov[0] && strncmp(gov, "performance", 11) != 0;
}

int retrohh_cpu_get_freq(void)
{
    int khz = 0;
    sysfs_get_int(CPUFREQ_POLICY "/scaling_cur_freq", &khz);
    return khz;
}
