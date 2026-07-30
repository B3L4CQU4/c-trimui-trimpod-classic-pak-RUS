/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2007 Jonathan Gordon
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

#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include "config.h"
#include "sound.h"
#include "lang.h"
#include "action.h"
#include "settings.h"
#include "menu.h"
#include "eq_menu.h"
#include "exported_menus.h"
#include "menu_common.h"
#include "splash.h"
#include "kernel.h"
#include "option_select.h"
#include "misc.h"

/***********************************/
/*    SOUND MENU                   */
/* Trimpod: "Volume" and "Maximum Volume Limit" are not exposed in the menu --
 * volume is handled globally by the hardware rocker, and the underlying dB /
 * perceptual-loudness scale is not a user-facing concept. volume_limit stays at
 * its default (max, no cap); see the %mv volume bar in skin_tokens.c. */
#ifdef AUDIOHW_HAVE_BASS
MENUITEM_SETTING(bass, &global_settings.bass,
#ifdef HAVE_SW_TONE_CONTROLS
    lowlatency_callback
#else
    NULL
#endif
);

#ifdef AUDIOHW_HAVE_BASS_CUTOFF
MENUITEM_SETTING(bass_cutoff, &global_settings.bass_cutoff, NULL);
#endif
#endif /* AUDIOHW_HAVE_BASS */


#ifdef AUDIOHW_HAVE_TREBLE
MENUITEM_SETTING(treble, &global_settings.treble,
#ifdef HAVE_SW_TONE_CONTROLS
    lowlatency_callback
#else
    NULL
#endif
);

#ifdef AUDIOHW_HAVE_TREBLE_CUTOFF
MENUITEM_SETTING(treble_cutoff, &global_settings.treble_cutoff, NULL);
#endif
#endif /* AUDIOHW_HAVE_TREBLE */


MENUITEM_SETTING(balance, &global_settings.balance, NULL);
MENUITEM_SETTING(channel_config, &global_settings.channel_config,
    lowlatency_callback
);
MENUITEM_SETTING(stereo_width, &global_settings.stereo_width,
    lowlatency_callback
);

#ifdef AUDIOHW_HAVE_DEPTH_3D
MENUITEM_SETTING(depth_3d, &global_settings.depth_3d, NULL);
#endif

#ifdef AUDIOHW_HAVE_FILTER_ROLL_OFF
MENUITEM_SETTING(roll_off, &global_settings.roll_off, NULL);
#endif


#ifdef AUDIOHW_HAVE_POWER_MODE
MENUITEM_SETTING(power_mode, &global_settings.power_mode, NULL);
#endif

    /* Crossfeed Submenu */
    MENUITEM_SETTING(crossfeed, &global_settings.crossfeed, lowlatency_callback);
    MENUITEM_SETTING(crossfeed_direct_gain,
                     &global_settings.crossfeed_direct_gain, lowlatency_callback);
    MENUITEM_SETTING(crossfeed_cross_gain,
                     &global_settings.crossfeed_cross_gain, lowlatency_callback);
    MENUITEM_SETTING(crossfeed_hf_attenuation,
                     &global_settings.crossfeed_hf_attenuation, lowlatency_callback);
    MENUITEM_SETTING(crossfeed_hf_cutoff,
                     &global_settings.crossfeed_hf_cutoff, lowlatency_callback);
    MAKE_MENU(crossfeed_menu,ID2P(LANG_CROSSFEED), NULL, Icon_NOICON,
              &crossfeed, &crossfeed_direct_gain, &crossfeed_cross_gain,
              &crossfeed_hf_attenuation, &crossfeed_hf_cutoff);


    MENUITEM_SETTING(dithering_enabled,
                     &global_settings.dithering_enabled, lowlatency_callback);
    MENUITEM_SETTING(afr_enabled,
                     &global_settings.afr_enabled, lowlatency_callback);
    MENUITEM_SETTING(pbe,
                     &global_settings.pbe, lowlatency_callback);
    MENUITEM_SETTING(pbe_precut,
                     &global_settings.pbe_precut, lowlatency_callback);
    MAKE_MENU(pbe_menu,ID2P(LANG_PBE), NULL, Icon_NOICON,
              &pbe,&pbe_precut);
    MENUITEM_SETTING(surround_enabled,
                     &global_settings.surround_enabled, lowlatency_callback);
    MENUITEM_SETTING(surround_balance,
                     &global_settings.surround_balance, lowlatency_callback);
    MENUITEM_SETTING(surround_fx1,
                     &global_settings.surround_fx1, lowlatency_callback);
    MENUITEM_SETTING(surround_fx2,
                     &global_settings.surround_fx2, lowlatency_callback);
    MENUITEM_SETTING(surround_method2,
                     &global_settings.surround_method2, lowlatency_callback);
    MENUITEM_SETTING(surround_mix,
                     &global_settings.surround_mix, lowlatency_callback);
    MAKE_MENU(surround_menu,ID2P(LANG_SURROUND), NULL, Icon_NOICON,
              &surround_enabled,&surround_balance,&surround_fx1,&surround_fx2,&surround_method2,&surround_mix);

    /* compressor submenu */
    MENUITEM_SETTING(compressor_threshold,
                     &global_settings.compressor_settings.threshold,
                     lowlatency_callback);
    MENUITEM_SETTING(compressor_gain,
                     &global_settings.compressor_settings.makeup_gain,
                     lowlatency_callback);
    MENUITEM_SETTING(compressor_ratio,
                     &global_settings.compressor_settings.ratio,
                     lowlatency_callback);
    MENUITEM_SETTING(compressor_knee,
                     &global_settings.compressor_settings.knee,
                     lowlatency_callback);
    MENUITEM_SETTING(compressor_attack,
                     &global_settings.compressor_settings.attack_time,
                     lowlatency_callback);
    MENUITEM_SETTING(compressor_release,
                     &global_settings.compressor_settings.release_time,
                     lowlatency_callback);
    MAKE_MENU(compressor_menu,ID2P(LANG_COMPRESSOR), NULL, Icon_NOICON,
              &compressor_threshold, &compressor_gain, &compressor_ratio,
              &compressor_knee, &compressor_attack, &compressor_release);


#ifdef AUDIOHW_HAVE_EQ
#endif /* AUDIOHW_HAVE_EQ */

/* replay gain submenu */
static int replaygain_callback(int action,
                               const struct menu_item_ex *this_item,
                               struct gui_synclist *this_list)
{
    (void)this_item;
    (void)this_list;
    switch (action)
    {
        case ACTION_EXIT_MENUITEM: /* on exit */
            replaygain_update();
            break;
    }
    return action;
}
MENUITEM_SETTING(replaygain_noclip,
                 &global_settings.replaygain_settings.noclip,
                 replaygain_callback);
MENUITEM_SETTING(replaygain_type,
                 &global_settings.replaygain_settings.type,
                 replaygain_callback);
MENUITEM_SETTING(replaygain_preamp,
                 &global_settings.replaygain_settings.preamp,
                 replaygain_callback);
MAKE_MENU(replaygain_settings_menu,ID2P(LANG_REPLAYGAIN),0, Icon_NOICON,
          &replaygain_type, &replaygain_noclip, &replaygain_preamp);

MAKE_MENU(sound_settings, ID2P(LANG_SOUND_SETTINGS), NULL, Icon_Audio,
          /* sub-pages first (entries that open their own page) */
          &crossfeed_menu, &equalizer_menu,
#ifdef AUDIOHW_HAVE_EQ
          &audiohw_eq_tone_controls,
#endif
          &surround_menu, &pbe_menu, &compressor_menu, &replaygain_settings_menu
          /* then in-page adjustable settings */
#ifdef AUDIOHW_HAVE_BASS
          ,&bass
#endif
#ifdef AUDIOHW_HAVE_BASS_CUTOFF
          ,&bass_cutoff
#endif
#ifdef AUDIOHW_HAVE_TREBLE
          ,&treble
#endif
#ifdef AUDIOHW_HAVE_TREBLE_CUTOFF
          ,&treble_cutoff
#endif
          ,&balance,&channel_config,&stereo_width
#ifdef AUDIOHW_HAVE_DEPTH_3D
          ,&depth_3d
#endif
#ifdef AUDIOHW_HAVE_FILTER_ROLL_OFF
          ,&roll_off
#endif
#ifdef AUDIOHW_HAVE_POWER_MODE
          ,&power_mode
#endif
          ,&dithering_enabled, &afr_enabled
         );
