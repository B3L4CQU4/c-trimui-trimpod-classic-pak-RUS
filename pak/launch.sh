#!/bin/sh
# Trimpod — Rockbox-based music player for the TrimUI Brick (NextUI).
# Launches our independently-built Rockbox SDL app with the 1ST_GEN_REMIX theme.
PAK_DIR="$(dirname "$0")"
RBDIR="$PAK_DIR/trimpod"
RBDIR_BIND="/tmp/trimpod"
cd "$PAK_DIR" || exit 1

# Rockbox stores its config/playlists under HOME; keep it on the SD card.
HOME="$USERDATA_PATH"

# Per-device sysfs paths (battery/backlight) + integer-ish display zoom value.
if [ "$PLATFORM" = "tg5040" ]; then
  if [ "$DEVICE" = "brick" ]; then
    RBDEVICE="TUI-Brick"; . "$RBDIR/systems/tui-brick.sys"
  else
    RBDEVICE="TUI-SmartPro"; . "$RBDIR/systems/tui-spoon.sys"
  fi
elif [ "$PLATFORM" = "my355" ]; then
  RBDEVICE="Miyoo-Flip"; . "$RBDIR/systems/my355.sys"
else
  RBDEVICE="fallback"; . "$RBDIR/systems/fallback.sys"
fi

# The app's data dir is built as /tmp/trimpod, so bind our pak data there.
if [ ! -f "$RBDIR_BIND/rockbox" ]; then
  mkdir -p "$RBDIR_BIND"
  mount --bind "$RBDIR" "$RBDIR_BIND"
fi

# Rewrite bundled theme cfgs from the canonical /.rockbox to the live bind path.
for theme in "$RBDIR"/themes/*.cfg; do
  [ -f "$theme" ] && sed -i 's#/\.rockbox#/tmp/trimpod#g' "$theme"
done

# --- CPU frequency (runtime-only; restored on exit) --------------------------
# Trimpod owns the CPU Frequency policy IN THE APP (Settings -> Power -> CPU):
# power-target.c is the single source that defines/persists (trimpod/cpu_freq.txt)
# and applies the choice -- at startup and on a live menu change. Here we only
# snapshot the system's original cpufreq state so we can restore it on exit
# (below), so whatever the app sets is never left on the device permanently.
CPUP=/sys/devices/system/cpu/cpufreq/policy0
if [ -d "$CPUP" ]; then
  TRIMPOD_OLD_GOV=$(cat "$CPUP/scaling_governor" 2>/dev/null)
  TRIMPOD_OLD_MIN=$(cat "$CPUP/scaling_min_freq" 2>/dev/null)
  TRIMPOD_OLD_MAX=$(cat "$CPUP/scaling_max_freq" 2>/dev/null)
fi

unset SDL_HQ_SCALER SDL_ROTATION SDL_BLITTER_DISABLED

# Input: the app reads the gamepad (and volume rocker) directly through SDL's
# joystick layer and the power key as an SDL keyboard scancode -- the same way
# NextUI does -- so there is no gptokeyb2 shim to start here.

# Side switch = input lock only. Freeze NextUI's keymon (it buzzes/dims/mutes on
# that switch) while we run; thaw on exit. Fallback: blank MutedVolume @ byte 56.
KEYMON_PIDS="$(pidof keymon.elf 2>/dev/null)"
SHM=/dev/shm/SharedSettings
if [ -f "$SHM" ]; then
  dd if="$SHM" bs=1 skip=56 count=4 of=/tmp/trimpod_muted_vol 2>/dev/null
  printf '\273\377\377\377' | dd of="$SHM" bs=1 seek=56 count=4 conv=notrunc 2>/dev/null
fi
[ -n "$KEYMON_PIDS" ] && kill -STOP $KEYMON_PIDS 2>/dev/null

# Audio: pin the codec to NextUI's safe ceiling, then let Rockbox's SOFTWARE
# volume ride underneath it.  NextUI (LoveRetro libmsettings SetRawVolume) never
# drives 'DAC volume' above 160/255 and, at its max, opens 'digital volume' fully
# (its reversed attenuation: raw 0 = loudest).  So that pair = the loudest the
# device is shipped to deliver; matching it means Trimpod at full software volume
# can't over-drive the little speaker.  We snapshot both and restore the user's
# NextUI levels on exit.  (Rockbox only does software volume via pcm_mixer here.)
TRIMPOD_DV="$(amixer sget 'digital volume' 2>/dev/null | sed -n 's/.*Mono: \([0-9][0-9]*\).*/\1/p')"
TRIMPOD_DAC="$(amixer sget 'DAC volume' 2>/dev/null | sed -n 's/.*Front Left: \([0-9][0-9]*\).*/\1/p')"
[ -n "$TRIMPOD_DV" ]  && amixer -q sset 'digital volume' 0   2>/dev/null
[ -n "$TRIMPOD_DAC" ] && amixer -q sset 'DAC volume'     160 2>/dev/null

# ALL teardown lives here so it runs on every catchable exit path -- a clean
# binary return, or INT/TERM/HUP (e.g. NextUI stopping the pak) -- not just the
# fall-through after the binary returns.  Idempotent and ordered: stop the app +
# input, restore NextUI's keymon/audio/CPU, then release the bind mount.  The
# bind mount in particular used to never be unmounted -> it leaked, surviving
# exit and stacking on aborted runs.  (SIGKILL is uncatchable; nothing can help
# that, but it's the only gap now.)
cleanup() {
  kill -9 "$(pidof trimpod)"   2>/dev/null
  [ -n "$KEYMON_PIDS" ] && kill -CONT $KEYMON_PIDS 2>/dev/null
  [ -f /tmp/trimpod_muted_vol ] && dd if=/tmp/trimpod_muted_vol of=/dev/shm/SharedSettings bs=1 seek=56 count=4 conv=notrunc 2>/dev/null
  [ -n "$TRIMPOD_DV" ] && amixer -q sset "digital volume" "$TRIMPOD_DV" 2>/dev/null
  [ -n "$TRIMPOD_DAC" ] && amixer -q sset "DAC volume" "$TRIMPOD_DAC" 2>/dev/null
  if [ -d "$CPUP" ] && [ -n "$TRIMPOD_OLD_GOV" ]; then
    echo 408000 > "$CPUP/scaling_min_freq"
    echo "$TRIMPOD_OLD_MAX" > "$CPUP/scaling_max_freq"
    echo "$TRIMPOD_OLD_MIN" > "$CPUP/scaling_min_freq"
    echo "$TRIMPOD_OLD_GOV" > "$CPUP/scaling_governor"
  fi
  # Re-enable the battery charger (AXP2202 reg 0x19 bit 1) on exit, in case the
  # in-app Charge Limit had disabled it -- guaranteed even on SIGKILL of the
  # binary, which the in-app code can't catch.  Skipped when the standalone
  # Battery Care daemon is running, since it owns the bit (the two never fight).
  BC_REGS=/sys/kernel/debug/regmap/6-0034/registers
  BC_PID=$(cat /tmp/battery-care-daemon.pid 2>/dev/null)
  if [ -f "$BC_REGS" ] && ! { [ -n "$BC_PID" ] && tr -d '\0' < "/proc/$BC_PID/cmdline" 2>/dev/null | grep -q battery-care-daemon; }; then
    bcval=$(grep '^19:' "$BC_REGS" 2>/dev/null | awk '{print $2}')
    [ -n "$bcval" ] && printf '19 %02x\n' "$(( 0x$bcval | 2 ))" > "$BC_REGS" 2>/dev/null
  fi
  while mount 2>/dev/null | grep -q "$RBDIR_BIND"; do umount -l "$RBDIR_BIND" 2>/dev/null; done
}
trap cleanup EXIT
trap 'exit' INT TERM HUP

"$RBDIR/trimpod" --zoom "$ZOOMVAL" > "$PAK_DIR/trimpod.log" 2>&1
