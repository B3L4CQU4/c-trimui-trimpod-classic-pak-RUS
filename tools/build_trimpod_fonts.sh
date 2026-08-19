#!/usr/bin/env bash
# Rebuild the Russian Trimpod UI font family.
#
# English uses the original checked-in ChicagoFLF .fnt files directly and is
# not generated here. Russian uses Mulmaru for every glyph it provides: Latin,
# digits, punctuation, symbols and Cyrillic. PixelMplus only fills missing
# Japanese.
# Mulmaru does not publish U+0401, so mergefnt.py builds it from U+0415.
# Modified Mulmaru derivatives use the neutral name TrimpodRus per the OFL.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONVTTF="${CONVTTF:-$ROOT/tools/convttf}"
OUT_DIR="${OUT_DIR:-$ROOT/assets/fonts}"
PIXELMPLUS="$ROOT/assets/fonts/PixelMplus12-Regular.ttf"
MULMARU="$ROOT/assets/fonts/sources/Mulmaru.ttf"
PREFER_CYRILLIC='U+0401,U+0410-U+044F,U+0451'

for source in "$PIXELMPLUS" "$MULMARU"; do
    [ -f "$source" ] || { echo "Missing font source: $source" >&2; exit 1; }
done

if [ ! -x "$CONVTTF" ]; then
    echo ">> Building the host convttf utility"
    make -C "$ROOT/tools" CC=gcc convttf
fi

mkdir -p "$OUT_DIR"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

verify_common_glyphs() {
    output="$1"
    shift
    for code in "$@"; do
        python3 "$ROOT/tools/mergefnt.py" --show "$code" "$output" >/dev/null
    done
}

build_russian() {
    size="$1"
    base="$work/${size}-mulmaru.fnt"
    japanese="$work/${size}-pixelmplus-ru.fnt"
    base_japanese="$work/${size}-mulmaru-japanese.fnt"
    cyrillic="$work/${size}-mulmaru-cyrillic.fnt"
    output="$OUT_DIR/${size}-TrimpodRus.fnt"

    echo ">> TrimpodRus ${size}px (Mulmaru UI, +1px glyph spacing)"
    # Keep Mulmaru as BASE so every glyph it contains wins, including Latin,
    # numbers, punctuation and symbols. PixelMplus is strictly a fallback.
    "$CONVTTF" -p "$size" -X 72 -s 32 -l 65518 \
        -o "$base" "$MULMARU"
    "$CONVTTF" -p "$size" -X 72 -s 12288 -l 65518 \
        -o "$japanese" "$PIXELMPLUS"
    python3 "$ROOT/tools/mergefnt.py" \
        "$base" "$japanese" "$base_japanese" --fit-add-metrics

    "$CONVTTF" -p "$size" -X 72 -s 1024 -l 1279 \
        -o "$cyrillic" "$MULMARU"
    python3 "$ROOT/tools/mergefnt.py" \
        "$base_japanese" "$cyrillic" "$output" \
        --fit-add-metrics --quantize-base --quantize-add --synthesize-yo \
        --prefer-add "$PREFER_CYRILLIC" --spacing 1

    # These probes make a missing Mulmaru ASCII/digit/symbol glyph fatal rather
    # than silently falling back to the default character.
    verify_common_glyphs "$output" \
        U+0021 U+0023 U+002B U+0030 U+0041 U+005A U+0061 U+007A \
        U+0401 U+0410 U+042F U+0430 U+044F U+0451
}

# Drop every obsolete ChicagoRus derivative and superseded Russian size.
# English loads the original ChicagoFLF files directly.
rm -f "$OUT_DIR/18-ChicagoRus.fnt" "$OUT_DIR/20-ChicagoRus.fnt" \
      "$OUT_DIR/24-ChicagoRus.fnt" "$OUT_DIR/27-ChicagoRus.fnt" \
      "$OUT_DIR/29-ChicagoRus.fnt" "$OUT_DIR/32-ChicagoRus.fnt" \
      "$OUT_DIR/38-ChicagoRus.fnt" "$OUT_DIR/27-TrimpodRus.fnt" \
      "$OUT_DIR/32-TrimpodRus.fnt"

# Russian uses the all-Mulmaru family at the same three UI roles.
build_russian 18
build_russian 20
build_russian 24

echo ">> Generated fonts in $OUT_DIR"
