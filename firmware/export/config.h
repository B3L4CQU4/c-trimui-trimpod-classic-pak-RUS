/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2002 by Daniel Stenberg
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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "autoconf.h"

/* symbolic names for multiple choice configurations: */

/* CONFIG_STORAGE (note these are combineable bit-flags) */
#define STORAGE_ATA_NUM     0
#define STORAGE_MMC_NUM     1
#define STORAGE_SD_NUM      2
#define STORAGE_NAND_NUM    3
#define STORAGE_RAMDISK_NUM 4
#define STORAGE_USB_NUM     5
#define STORAGE_HOSTFS_NUM  6
#define STORAGE_NUM_TYPES   7

#define STORAGE_ATA         (1 << STORAGE_ATA_NUM)
#define STORAGE_MMC         (1 << STORAGE_MMC_NUM)
#define STORAGE_SD          (1 << STORAGE_SD_NUM)
#define STORAGE_NAND        (1 << STORAGE_NAND_NUM)
#define STORAGE_RAMDISK     (1 << STORAGE_RAMDISK_NUM)
#define STORAGE_USB         (1 << STORAGE_USB_NUM)
 /* meant for APPLICATION targets (implicit for SIMULATOR) */
#define STORAGE_HOSTFS      (1 << STORAGE_HOSTFS_NUM)

/* tuner types (note these are combineable bit-flags) */
#define TEA5767    0x02 /* Philips */
#define LV24020LP  0x04 /* Sanyo */
#define SI4700     0x08 /* Silicon Labs */
#define TEA5760    0x10 /* Philips */
#define LV240000   0x20 /* Sanyo */
#define IPOD_REMOTE_TUNER   0x40 /* Apple */
#define RDA5802    0x80 /* RDA Microelectronics */
#define STFM1000   0x100 /* Sigmatel */

/* CONFIG_CPU */
#define MCF5249      5249
#define MCF5250      5250
#define PP5002       5002
#define PP5020       5020
#define PP5022       5022
#define PP5024       5024
#define PP6100       6100
#define S3C2440      2440
#define DSC25          25
#define DM320         320
#define IMX31L         31
#define TCC7801      7801
#define S5L8700      8700
#define S5L8701      8701
#define S5L8702      8702
#define S5L8720      8720
#define S5L8723      8723
#define S5L8730      8730
#define S5L8740      8740
#define JZ4732       4732
#define JZ4760B     47602
#define AS3525       3525
#define AS3525v2    35252
#define IMX233        233
#define RK27XX       2700
#define X1000        1000
#define STM32H743   32743
#define N10480H     10480

/* platforms
 * bit fields to allow PLATFORM_HOSTED to be OR'ed e.g. with a
 * possible future PLATFORM_ANDROID (some OSes might need totally different
 * handling to run on them than a stand-alone application) */
#define PLATFORM_NATIVE  (1<<0)
#define PLATFORM_HOSTED  (1<<1)
#define PLATFORM_ANDROID (1<<2)
#define PLATFORM_SDL     (1<<3)
#define PLATFORM_CTRU    (1<<4)

/* CONFIG_KEYPAD */
#define IRIVER_H100_PAD     4
#define IRIVER_H300_PAD     5
#define IAUDIO_X5M5_PAD     6
#define IPOD_4G_PAD         7
#define IPOD_3G_PAD         8
#define IPOD_1G2G_PAD       9
#define GIGABEAT_PAD       11
#define IRIVER_H10_PAD     12
#define SANSA_E200_PAD     13
#define SANSA_C200_PAD     14
#define MROBE100_PAD       17
#define MROBE500_PAD       18
#define GIGABEAT_S_PAD     19
#define COWON_D2_PAD        22
#define IAUDIO_M3_PAD      23
#define SANSA_M200_PAD     25
#define PHILIPS_SA9200_PAD 27
#define PHILIPS_HDD1630_PAD 29
#define ONDAVX747_PAD      31
#define ONDAVX767_PAD      32
#define SANSA_CLIP_PAD     35
#define SANSA_FUZE_PAD     36
#define SAMSUNG_YH820_PAD  38
#define ONDAVX777_PAD      39
#define PHILIPS_HDD6330_PAD 42
#define PBELL_VIBE500_PAD 43
#define MPIO_HD200_PAD     44
#define ANDROID_PAD        45
#define SDL_PAD            46
#define MPIO_HD300_PAD     47
#define SANSA_FUZEPLUS_PAD 48
#define RK27XX_GENERIC_PAD 49
#define HM60X_PAD          50
#define HM801_PAD          51
#define SANSA_CONNECT_PAD  52
#define SAMSUNG_YPR0_PAD   53
#define CREATIVE_ZENXFI2_PAD 54
#define CREATIVE_ZENXFI3_PAD 55
#define MA_PAD            56
#define SONY_NWZ_PAD       57
#define CREATIVE_ZEN_PAD   58
#define CREATIVE_ZENV_PAD  59
#define IHIFI_PAD          60
#define SAMSUNG_YPR1_PAD   61
#define SAMSUNG_YH92X_PAD  62
#define DX50_PAD           63
#define SONY_NWZA860_PAD   64 /* The NWZ-A860 is too different (touchscreen) */
#define AGPTEK_ROCKER_PAD  65
#define XDUOO_X3_PAD       66
#define IHIFI_770_PAD      67
#define IHIFI_800_PAD      68
#define XDUOO_X3II_PAD     69
#define XDUOO_X20_PAD      70
#define FIIO_M3K_LINUX_PAD 71
#define EROSQ_PAD          72
#define FIIO_M3K_PAD       73
#define SHANLING_Q1_PAD    74
#define ECHO_R1_PAD        75
#define SURFANS_F28_PAD    76
#define RG_NANO_PAD        77
#define CTRU_PAD           78
#define HIBY_R3PROII_PAD   79
#define RETRO_HANDHELD_PAD 80

/* CONFIG_REMOTE_KEYPAD */
#define H100_REMOTE   1
#define H300_REMOTE   2
#define IAUDIO_REMOTE 3
#define MROBE_REMOTE  4

/* CONFIG_BACKLIGHT_FADING */
/* No fading capabilities at all (yet) */
#define BACKLIGHT_NO_FADING         0x0
/* Backlight fading is controlled using a hardware PWM mechanism */
#define BACKLIGHT_FADING_PWM        0x1
/* Backlight is controlled using a software implementation
 * BACKLIGHT_FADING_SW_SETTING means that backlight is turned on by only setting
 * the brightness (i.e. no real difference between backlight_on and
 * backlight_set_brightness)
 * BACKLIGHT_FADING_SW_HW_REG means that backlight brightness is restored
 * "in hardware", from a hardware register upon backlight_on
 * Both types need to have minor adjustments in the software fading code */
#define BACKLIGHT_FADING_SW_SETTING 0x2
#define BACKLIGHT_FADING_SW_HW_REG  0x4
/* Backlight fading is done in a target specific way
 * for example in hardware, but not controllable*/
#define BACKLIGHT_FADING_TARGET     0x8

/* CONFIG_CHARGING */

/* Generic types */
#define CHARGING_SIMPLE  1 /* Simple, hardware controlled charging
                            * (CPU cannot read charger state but may read
                            *  when power is plugged-in). */
#define CHARGING_MONITOR 2 /* Hardware controlled charging with monitoring
                            * (CPU is able to read HW charging state and
                            *  when power is plugged-in). */

/* Mostly target-specific code in the /target tree */
#define CHARGING_TARGET  3 /* Any algorithm - usually software controlled
                            * charging or specific programming is required to
                            * use the charging hardware. */

/* CONFIG_BATTERY_MEASURE bits */
/* If both VOLTAGE_MEASURE and PERCENTAGE_MEASURE are defined,
 * _battery_level() (percentage) will be preferred, unless _battery_level()
 * returns -1, then voltage will be used from _voltage_level(). */
#define VOLTAGE_MEASURE     1 /* Target can report battery voltage
                               * Usually native ports */
#define PERCENTAGE_MEASURE  2 /* Target can report remaining capacity in %
                               * Usually application/hosted ports */
#define TIME_MEASURE        4 /* Target can report remaining time estimation
                                 Usually application ports, and only
                                 if the estimation is better that ours
                                 (which it probably is) */
#define CURRENT_MEASURE     8 /* Target can report battery charge and/or
                               * discharge current */
/* CONFIG_LCD */
#define LCD_SSD1815   1 /* as used by Sansa M200 and others */
#define LCD_S1D15E06  3 /* as used by iRiver H100 series */
#define LCD_H300      4 /* as used by iRiver H300 series, exact model name is
                           unknown at the time of this writing */
#define LCD_X5        5 /* as used by iAudio X5 series, exact model name is
                          unknown at the time of this writing */
#define LCD_IPODCOLOR 6 /* as used by iPod Color/Photo */
#define LCD_IPODNANO  7 /* as used by iPod Nano */
#define LCD_IPODVIDEO 8 /* as used by iPod Video */
#define LCD_IPOD2BPP  9 /* as used by all fullsize greyscale iPods */
#define LCD_IPODMINI 10 /* as used by iPod Mini g1/g2 */
#define LCD_GIGABEAT 12
#define LCD_H10_20GB 13 /* as used by iriver H10 20Gb */
#define LCD_H10_5GB  14 /* as used by iriver H10 5Gb */
#define LCD_C200     17 /* as used by Sandisk Sansa c200 */
#define LCD_MROBE500 18 /* as used by Olympus M:Robe 500i */
#define LCD_MROBE100 19 /* as used by Olympus M:Robe 100 */
#define LCD_TL0350A  23 /* as used by the iAudio M3 remote, treated as main LCD */
#define LCD_COWOND2  24 /* as used by Cowon D2 - LTV250QV, TCC7801 driver */
#define LCD_SA9200   25 /* as used by the Philips SA9200 */
#define LCD_S6B33B2  26 /* as used by the Samsumg YH820 */
#define LCD_HDD1630  27 /* as used by the Philips HDD1630 */
#define LCD_ONDAVX747 29 /* as used by the Onda VX747 */
#define LCD_ONDAVX767 30 /* as used by the Onda VX767 */
#define LCD_SSD1303   31 /* as used by the Sansa Clip */
#define LCD_FUZE      32 /* as used by the Sansa Fuze */
#define LCD_YH925     34 /* as used by Samsung YH-925 (similar to the H10 20GB) */
#define LCD_VIEW      35 /* as used by the Sansa View */
#define LCD_NANO2G    36 /* as used by the iPod Nano 2nd Generation */
#define LCD_HDD6330   38 /* as used by the Philips HDD6330 */
#define LCD_VIBE500   39 /* as used by the Packard Bell Vibe 500 */
#define LCD_IPOD6GNANO3G4G   40 /* as used by the iPod Classic, Nano 3G and Nano 4G */
#define LCD_FUZEPLUS  41
#define LCD_SPFD5420A 42 /* rk27xx */
#define LCD_CLIPZIP   43 /* as used by the Sandisk Sansa Clip Zip */
#define LCD_HX8340B   44 /* as used by the HiFiMAN HM-601/HM-602/HM-801 */
#define LCD_CONNECT   45 /* as used by the Sandisk Sansa Connect */
#define LCD_GIGABEATS 46
#define LCD_YPR0      47
#define LCD_CREATIVEZXFI2 48 /* as used by the Creative Zen X-Fi2 */
#define LCD_CREATIVEZXFI3 49 /* as used by the Creative Zen X-Fi3 */
#define LCD_ILI9342   50 /* as used by HiFi E.T MA9/MA8 */
#define LCD_NWZE370   51 /* as used by Sony NWZ-E370 series */
#define LCD_NWZE360   52 /* as used by Sony NWZ-E360 series */
#define LCD_CREATIVEZEN  55 /* as used by the Creative ZEN (X-Fi) (LMS250GF03-001(S6D0139)) */
#define LCD_CREATIVEZENMOZAIC 56 /* as used by the Creative ZEN Mozaic (FGD0801) */
#define LCD_ILI9342C   57 /* another type of lcd used by HiFi E.T MA9/MA8 */
#define LCD_CREATIVEZENV  58 /* as used by the Creative Zen V (Plus) */
#define LCD_IHIFI         60 /* as used by IHIFI 760/960 */
#define LCD_CREATIVEZENXFISTYLE 61 /* as used by Creative Zen X-Fi Style */
#define LCD_SAMSUNGYPR1   62 /* as used by Samsung YP-R1 */
#define LCD_NWZ_LINUX   63 /* as used in the Linux-based NWZ series */
#define LCD_INGENIC_LINUX 64
#define LCD_XDUOOX3       65 /* as used by the xDuoo X3 */
#define LCD_IHIFI770      66 /* as used by IHIFI 770 */
#define LCD_IHIFI770C     67 /* as used by IHIFI 770C */
#define LCD_IHIFI800      68 /* as used by IHIFI 800 */
#define LCD_FIIOM3K       69 /* as used by the FiiO M3K */
#define LCD_SHANLING_Q1   70 /* as used by the Shanling Q1 */
#define LCD_EROSQ         71 /* as used by the ErosQ (native) */
#define LCD_ECHO_R1       72 /* ILI9342, as used by the Echo R1 */

/* LCD_PIXELFORMAT */
#define HORIZONTAL_PACKING 1
#define VERTICAL_PACKING 2
#define HORIZONTAL_INTERLEAVED 3
#define VERTICAL_INTERLEAVED 4
#define RGB565 565
#define RGB565SWAPPED 3553
#define RGB888 888
#define XRGB8888 8888

/* LCD_STRIDEFORMAT */
#define VERTICAL_STRIDE     1
#define HORIZONTAL_STRIDE   2

/* CONFIG_ORIENTATION */
#define SCREEN_PORTRAIT     0
#define SCREEN_LANDSCAPE    1
#define SCREEN_SQUARE       2

/* CONFIG_I2C */
#define I2C_NONE     0 /* For targets that do not use I2C - as the
Lyre prototype 1 */
#define I2C_COLDFIRE 3 /* Coldfire style */
#define I2C_PP5002   4 /* PP5002 style */
#define I2C_PP5020   5 /* PP5020 style */
#define I2C_S3C2440  7
#define I2C_PP5024   8 /* PP5024 style */
#define I2C_IMX31L   9
#define I2C_TCC780X 11
#define I2C_DM320   12 /* DM320 style */
#define I2C_S5L8700 13
#define I2C_JZ47XX  14 /* Ingenic Jz47XX style */
#define I2C_AS3525  15
#define I2C_S5L8702 16 /* Same as S5L8700, but with two channels */
#define I2C_IMX233  17
#define I2C_RK27XX  18
#define I2C_X1000   19

/* CONFIG_LED */
#define LED_REAL     1 /* SW controlled LED (Archos recorders, player) */
#define LED_VIRTUAL  2 /* Virtual LED (icon) (Archos Ondio) */
/* else                   HW controlled LED (iRiver H1x0) */

/* CONFIG_NAND */
#define NAND_TCC     2
#define NAND_SAMSUNG 3
#define NAND_CC      4 /* ChinaChip */
#define NAND_RK27XX  5
#define NAND_IMX233  6

/* CONFIG_RTC */
#define RTC_HOSTED   1 /* Generic hosted */
#define RTC_PCF50605 2 /* iPod 3G, 4G & Mini */
#define RTC_PCF50606 3 /* iriver H300 */
#define RTC_S3C2440  4
#define RTC_E8564    5 /* iriver H10 */
#define RTC_AS3514   6 /* Sandisk Sansa series */
#define RTC_DS1339_DS3231   7 /* h1x0 RTC mod */
#define RTC_IMX31L   8
#define RTC_RX5X348AB 9
#define RTC_TCC780X  11
#define RTC_MR100  12
#define RTC_MC13783  13 /* Freescale MC13783 PMIC */
#define RTC_S5L8700  14
#define RTC_JZ4740   16 /* Ingenic Jz4740 */
#define RTC_NANO2G   17 /* This seems to be a PCF5063x */
#define RTC_D2       18 /* Either PCF50606 or PCF50635 */
#define RTC_S35380A  19
#define RTC_IMX233   20
#define RTC_STM41T62 21 /* ST M41T62 */
#define RTC_JZ4760   22 /* Ingenic Jz4760 */
#define RTC_X1000    23 /* Ingenic X1000 */
#define RTC_CONNECT  24 /* Sansa Connect AVR */
#define RTC_NANO3G   25 /* Dialog Semiconductor D1671 ??? */
#define RTC_NANO4G   26 /* Dialog Semiconductor D1759 ??? */
#define RTC_STM32H743 27

/* USB On-the-go */
#define USBOTG_M66591   6591 /* M:Robe 500 */
#define USBOTG_ISP1362  1362 /* iriver H300 */
#define USBOTG_ISP1583  1583 /* Creative Zen Vision:M */
#define USBOTG_M5636    5636 /* iAudio X5 */
#define USBOTG_ARC      5020 /* PortalPlayer 502x and IMX233 */
#define USBOTG_JZ4740   4740 /* Ingenic Jz4740/Jz4732 */
#define USBOTG_JZ4760   4760 /* Ingenic Jz4760/Jz4760B */
#define USBOTG_AS3525   3525 /* AMS AS3525 */
#define USBOTG_S3C6400X 6400 /* Samsung S3C6400X, also used in the S5L8701/S5L8702/S5L8720 */
#define USBOTG_DESIGNWARE 6401 /* Synopsys DesignWare OTG, used in S5L8701/S5L8702/S5L8720/AS3252v2 */
#define USBOTG_RK27XX   2700 /* Rockchip rk27xx */
#define USBOTG_TNETV105 105  /* TI TNETV105 */

/* Multiple cores */
#define CPU 0
#define COP 1

/* CONFIG_BUFLIB_BACKEND */
#define BUFLIB_BACKEND_MEMPOOL      0 /* Default memory pool backed buflib */
#define BUFLIB_BACKEND_MALLOC       1 /* malloc() buflib (for debugging) */

/* CONFIG_BINFMT */
#define BINFMT_ROCK                 0 /* Rockbox ".rock" format */
#define BINFMT_DLOPEN               1 /* dlopen-based */

/* now go and pick yours */
#include "config/retro-handheld.h"

#ifndef CONFIG_CPU
#define CONFIG_CPU 0
#endif

// NOTE: should be placed before sim.h (where CONFIG_CPU is undefined)
#if !(CONFIG_CPU >= PP5002 && CONFIG_CPU <= PP5022) && CODEC_SIZE >= 0x80000
#define CODEC_AAC_SBR_DEC
#endif


#ifndef CONFIG_BUFLIB_BACKEND
# define CONFIG_BUFLIB_BACKEND BUFLIB_BACKEND_MEMPOOL
#endif

#if defined(APPLICATION)
#ifndef CONFIG_CPU
#define CONFIG_CPU 0
#endif
#endif



#ifndef CONFIG_PLATFORM
#define CONFIG_PLATFORM PLATFORM_NATIVE
#endif

#ifndef CONFIG_BINFMT
# define CONFIG_BINFMT BINFMT_DLOPEN
#endif

#if CONFIG_BINFMT == BINFMT_ROCK
#endif

/* Codec buffering requires the ability to load code from RAM */

/* setup basic macros from capability masks */
#include "config_caps.h"


/* now set any CONFIG_ defines correctly if they are not used,
   No need to do this on CONFIG_'s which are compulsory (e.g CONFIG_CODEC ) */

#if !defined(CONFIG_BACKLIGHT_FADING)
#define CONFIG_BACKLIGHT_FADING BACKLIGHT_NO_FADING
#endif

#ifndef CONFIG_I2C
#define CONFIG_I2C I2C_NONE
#endif

#ifndef CONFIG_USBOTG
#define CONFIG_USBOTG 0
#endif

#ifndef CONFIG_LED
#define CONFIG_LED LED_VIRTUAL
#endif

#ifndef CONFIG_CHARGING
#define CONFIG_CHARGING 0
#endif

#ifndef CONFIG_BATTERY_MEASURE
#define CONFIG_BATTERY_MEASURE 0
#define NO_LOW_BATTERY_SHUTDOWN
#endif

#ifndef CONFIG_RTC
#define CONFIG_RTC 0
#endif

#ifndef BATTERY_CAPACITY_DEFAULT
#define BATTERY_CAPACITY_DEFAULT 0
#endif

#ifndef BATTERY_CAPACITY_MIN
#define BATTERY_CAPACITY_MIN BATTERY_CAPACITY_DEFAULT
#endif

#ifndef BATTERY_CAPACITY_MAX
#define BATTERY_CAPACITY_MAX BATTERY_CAPACITY_DEFAULT
#endif

#ifndef BATTERY_CAPACITY_INC
#define BATTERY_CAPACITY_INC 0
#endif



#ifndef CONFIG_ORIENTATION
#if LCD_HEIGHT > LCD_WIDTH
#define CONFIG_ORIENTATION SCREEN_PORTRAIT
#elif LCD_HEIGHT < LCD_WIDTH
#define CONFIG_ORIENTATION SCREEN_LANDSCAPE
#else
#define CONFIG_ORIENTATION SCREEN_SQUARE
#endif
#endif

/* Pixel aspect ratio is defined in terms of a multiplier for pixel width and
 * height, and is set to 1:1 if the target does not set a value
 */
#ifndef LCD_PIXEL_ASPECT_HEIGHT
#define LCD_PIXEL_ASPECT_HEIGHT 1
#endif
#ifndef LCD_PIXEL_ASPECT_WIDTH
#define LCD_PIXEL_ASPECT_WIDTH 1
#endif

/* Used for split displays (Sansa Clip). Set to 0 otherwise */
#ifndef LCD_SPLIT_LINES
#define LCD_SPLIT_LINES 0
#endif

/* Most displays have a horizontal stride */
#ifndef LCD_STRIDEFORMAT
# define LCD_STRIDEFORMAT HORIZONTAL_STRIDE
#endif

/* Simulator LCD dimensions. Set to standard dimensions if undefined */
#ifndef SIM_LCD_WIDTH
#define SIM_LCD_WIDTH LCD_WIDTH
#endif
#ifndef SIM_LCD_HEIGHT
#define SIM_LCD_HEIGHT (LCD_HEIGHT + LCD_SPLIT_LINES)
#endif

/* define this in the target config.h to use a different size */
#ifndef CONFIG_DEFAULT_ICON_HEIGHT
#define CONFIG_DEFAULT_ICON_HEIGHT 8
#endif
#ifndef CONFIG_DEFAULT_ICON_WIDTH
#define CONFIG_DEFAULT_ICON_WIDTH 6
#endif
#ifndef CONFIG_REMOTE_DEFAULT_ICON_HEIGHT
#define CONFIG_REMOTE_DEFAULT_ICON_HEIGHT 8
#endif
#ifndef CONFIG_REMOTE_DEFAULT_ICON_WIDTH
#define CONFIG_REMOTE_DEFAULT_ICON_WIDTH 6
#endif

#if LCD_DEPTH > 1
#define HAVE_BACKDROP_IMAGE
#endif


/* determine which setting/manual text to use */
#if (CONFIG_BACKLIGHT_FADING == BACKLIGHT_FADING_PWM)

/* possibly overridden in target config */
#if !defined(HAVE_BACKLIGHT_FADING_BOOL_SETTING)
#endif

#elif  (CONFIG_BACKLIGHT_FADING == BACKLIGHT_FADING_SW_SETTING) \
    || (CONFIG_BACKLIGHT_FADING == BACKLIGHT_FADING_SW_HW_REG) \
    || (CONFIG_BACKLIGHT_FADING == BACKLIGHT_FADING_TARGET)

/* possibly overridden in target config */
#if !defined(HAVE_BACKLIGHT_FADING_BOOL_SETTING)
#define HAVE_BACKLIGHT_FADING_BOOL_SETTING
#endif

#endif /* CONFIG_BACKLIGHT_FADING */

/* Storage related config handling */

#if (CONFIG_STORAGE & (CONFIG_STORAGE - 1)) != 0
/* Multiple storage drivers */
#define CONFIG_STORAGE_MULTI
#endif

#ifndef NUM_DRIVES
#define NUM_DRIVES 1
#endif

#if   (CONFIG_STORAGE & STORAGE_SD)
/* SD routinely have multiple partitions */
#endif

/* Number of volumes per drive */
#define NUM_VOLUMES_PER_DRIVE 1

/* note to remove multi-partition booting this could be changed to MULTIDRIVE */

/* The lowest numbered volume to read a multiboot redirect from; default is to
 * allow any volume but some targets may wish to exclude the internal drive. */

#define NUM_VOLUMES (NUM_DRIVES * NUM_VOLUMES_PER_DRIVE)

/* Sanity check sector size options */
#if defined(MAX_VARIABLE_LOG_SECTOR) && defined(MAX_VIRT_SECTOR_SIZE)
#if (MAX_VIRT_SECTOR_SIZE < MAX_VARIABLE_LOG_SECTOR)
#error "optional MAX_VIRT_SECTOR_SIZE must be at least as large as MAX_VARIABLE_LOG_SECTOR"
#endif
#endif




#define HAVE_EXTENDED_MESSAGING_AND_NAME
#define HAVE_WAKEUP_EXT_CB

#if defined(ASSEMBLER_THREADS) \
    || defined(HAVE_SIGALTSTACK_THREADS) \
    || defined(CTRU)
#define HAVE_PRIORITY_SCHEDULING
#endif

#if (CONFIG_CPU == JZ4732) || (CONFIG_CPU == JZ4760B) \
    || (CONFIG_CPU == AS3525) || (CONFIG_CPU == AS3525v2) \
    || defined(CPU_S5L87XX) || (CONFIG_CPU == S3C2440) \
    || defined(APPLICATION) || (CONFIG_CPU == PP5002) \
    || (CONFIG_CPU == RK27XX) || (CONFIG_CPU == IMX233) ||              \
    (defined(HAVE_LCD_COLOR) && (LCD_STRIDEFORMAT == HORIZONTAL_STRIDE))
#define HAVE_SEMAPHORE_OBJECTS
#endif

/*include support for crossfading - requires significant PCM buffer space*/
#if MEMORYSIZE > 2
#define HAVE_CROSSFADE
#endif


/* -Wunterminates-string-initialization will complain if we try to shove
  a "string" into an array that is too small.  Sometimes this actually
  intentional, where you are merely using "string" as a standin for
  "non-terminated sequence of bytes" -- in which case we need to mark
  the "string" as "not actually a string" with an attribute.  Applies to
  GCC >=8, but this warning isn't pulled in by -Wextra until >= 15.
*/
#if __GNUC__ >= 8
#define __NONSTRING __attribute__((__nonstring__))
#else
#define __NONSTRING
#endif

/*
 * These macros are for switching on unified syntax in inline assembly.
 * Older versions of GCC emit assembly in divided syntax with no option
 * to enable unified syntax.
 */
#if (__GNUC__ < 8) && (defined(CPU_ARM_CLASSIC)||defined(CPU_ARM_APPLICATION))
#define BEGIN_ARM_ASM_SYNTAX_UNIFIED ".syntax unified\n"
#define END_ARM_ASM_SYNTAX_UNIFIED   ".syntax divided\n"
#else
#define BEGIN_ARM_ASM_SYNTAX_UNIFIED
#define END_ARM_ASM_SYNTAX_UNIFIED
#endif



#ifndef CODEC_SIZE
#define CODEC_SIZE 0
#endif

/* This attribute can be used to ensure that certain symbols are never profiled
 * which can be important as profiling a function de-inlines it */
#ifdef RB_PROFILE
#define NO_PROF_ATTR __attribute__ ((no_instrument_function))
#else
#define NO_PROF_ATTR
#endif


# define ICODE_ATTR
# define ICONST_ATTR
# define IDATA_ATTR
# define IBSS_ATTR

#define INIT_ATTR
#define INITDATA_ATTR

/* We need to call storage_init more than once only if USB storage mode is
 * handled in hardware:
 * Deinit storage -> let hardware handle USB mode -> storage_init() again
 */
#if defined(USB_NONE)
#define STORAGE_INIT_ATTR INIT_ATTR
#else
#define STORAGE_INIT_ATTR
#endif

#if (CONFIG_PLATFORM & PLATFORM_HOSTED) && defined(__APPLE__)
#define DATA_ATTR       __attribute__ ((section("__DATA, .data")))
#else
#define DATA_ATTR       __attribute__ ((section(".data")))
#endif

#ifndef IRAM_LCDFRAMEBUFFER
/* if the LCD framebuffer has not been moved to IRAM, define it empty here */
#define IRAM_LCDFRAMEBUFFER
#endif




#ifndef NUM_CORES
/* Default to single core */
#define NUM_CORES 1
#define CURRENT_CORE    CPU
/* Attributes for core-shared data in DRAM - no caching considerations */
#define SHAREDBSS_ATTR
#define SHAREDDATA_ATTR
#ifndef NOCACHEBSS_ATTR
#define NOCACHEBSS_ATTR
#define NOCACHEDATA_ATTR
#endif

#define IF_COP(...)
#define IF_COP_VOID(...)    void
#define IF_COP_CORE(core)   CURRENT_CORE

#endif /* NUM_CORES */



#if (!defined(SIMULATOR) && !defined(HAVE_HOSTFS) && !(CONFIG_STORAGE & STORAGE_HOSTFS))
#define STORAGE_GET_INFO
#endif

#if defined(HAVE_SIGALTSTACK_THREADS)
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600   /* For sigaltstack */
#endif
#endif





/* Trimpod: the pitch/speed adjust screen was plugin-backed (pitch_screen.rock);
   with plugins removed there is no UI to change pitch, so the whole feature is
   disabled for this target. */

/* enable logging messages to disk*/
#define ROCKBOX_HAS_LOGDISKF

#if defined(HAVE_SDL_AUDIO) \
    && !defined(HAVE_SW_VOLUME_CONTROL)
/* SW volume is needed for accurate control and no double buffering should be
 * required. If target uses SW volume, then its definitions are used instead
 * so things are as on target. */
#define HAVE_SW_VOLUME_CONTROL
#define PCM_SW_VOLUME_UNBUFFERED /* pcm driver itself is buffered */
#define PCM_SW_VOLUME_FRACBITS  (16)
#endif /* default SDL SW volume conditions */

#define HAVE_SCREENDUMP

/* Trimpod: perceptual volume ON -- the rocker steps and the bar both move in
 * equal perceived-loudness increments (volume_adjust_mode=perceptual,
 * volume_adjust_norm_steps=20), so the bar reads as a plain 0..10 loudness dial
 * (middle = half), not the raw logarithmic dB. The dB stays the storage unit;
 * users never see it (the volume_adjust_* knobs are hidden from the menu). */
#define HAVE_PERCEPTUAL_VOLUME

#if defined(SDMMC_HOST_NUM_SD_CONTROLLERS) || \
    defined(SDMMC_HOST_NUM_MMC_CONTROLLERS)
#endif


/*
 * Turn off legacy codepage handling in the filesystem code for bootloaders,
 * and support ISO-8859-1 (Latin-1) only. This only affects DOS 8.3 filename
 * parsing when FAT32 long names are unavailable; long names are Unicode and
 * can always be decoded properly regardless of this setting.
 *
 * In reality, bootloaders never supported codepages other than Latin-1 in
 * the first place. They did contain the code to load codepages from disk,
 * but had no way to actually change the codepage away from Latin-1.
 */
#define HAVE_FILESYSTEM_CODEPAGE

/* null audiohw setting macro for when codec header is included for reasons
   other than audio support */
#define AUDIOHW_SETTING(name, us, nd, st, minv, maxv, defv, expr...)

/* Trying to enable the setting without the underlying functions doesn't work */

/* Support for unicode codepoints > U+FFFF */
#if (MEMORYSIZE > 2)
#define UNICODE32
#endif

#ifdef UNICODE32
#define ucschar_t unsigned int
#else
#define ucschar_t unsigned short
#endif

#endif /* __CONFIG_H__ */
