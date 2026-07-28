#!/usr/bin/env python3
"""Merge two Rockbox RB12 .fnt files (stdlib only).

    mergefnt.py [--quantize-add] BASE.fnt ADD.fnt OUT.fnt   merge (BASE wins)
    mergefnt.py --show U+3042 FONT.fnt        ASCII-render one glyph (debug)

--quantize-add snaps ADD's 4-bit pixels to solid ink/blank.  Use it when ADD
is a pixel font rasterized at an exact integer scale (e.g. PixelMplus12 at
24ppem = 2x): any intermediate shade is FreeType hinter drift, not design.

Both inputs must agree on height/ascent/depth; the merged font keeps BASE's
metrics and defaultchar, and BASE's bitmap bytes verbatim (its glyph data is
the unmodified prefix of the output's bitmap block).  Chars absent from both
alias BASE's defaultchar glyph, matching what convttf emits for gaps.  Format
per tools/convttf.c writer / firmware/font.c font_load_header: 36-byte header,
4-bit AA pixel data (2 px/byte, low nibble first, 0xF=blank), byte-aligned per
glyph, then the offset table (u16, or u32 when nbits >= 0xFFDB) and width table.

Shipped-font recipe (PixelMplus12 ADD rasters that were merged into the
ChicagoFLF bases; convttf args are atoi — DECIMAL ONLY):
    24px:  convttf -p 24 -X 72 -Ta 2 -Td 1 -e 32 -s 12288 -l 65518
    20px:  convttf -p 20 -Ta 1 -e 24 -s 12288 -l 65518
    18px:  convttf -p 18 -Ta 1 -Td 1 -e 16 -s 12288 -l 65518
-X 72 gives true 24ppem (exact 2x of the 12px pixel font); trims map hhea
27/22 to cell 24/20.  The embolden values (-e, units of 1/64 px) put edges at
sub-pixel positions so FreeType antialiases them like ChicagoFLF AND thicken
toward Chicago's weight — user-reviewed: -e 32 at 24px (pure 2x read as "not
antialiased", -e 48 clogged dense kanji).  Do NOT pass --quantize-add for
emboldened rasters (shades are real); BASE bytes stay verbatim either way.
"""
import struct
import sys

LONG_OFFSET_MIN = 0xFFDB          # firmware/font.c MAX_FONTSIZE_FOR_16_BIT_OFFSETS
HDR = struct.Struct('<4s4H6L')    # magic, maxwidth,height,ascent,depth, 6 longs


class Font:
    def __init__(self, path):
        data = open(path, 'rb').read()
        (magic, self.maxwidth, self.height, self.ascent, self.depth,
         self.firstchar, self.defaultchar, self.size,
         nbits, noffset, nwidth) = HDR.unpack_from(data, 0)
        if magic != b'RB12':
            sys.exit(f'{path}: not an RB12 font')
        if self.depth != 1:
            sys.exit(f'{path}: only 4-bit AA fonts supported (depth=1)')
        pos = HDR.size
        self.bits = data[pos:pos + nbits]
        pos += nbits
        if nbits >= LONG_OFFSET_MIN:
            pos = (pos + 3) & ~3
            self.offsets = list(struct.unpack_from(f'<{noffset}L', data, pos))
            pos += 4 * noffset
        else:
            pos = (pos + 1) & ~1
            self.offsets = list(struct.unpack_from(f'<{noffset}H', data, pos))
            pos += 2 * noffset
        self.widths = list(struct.unpack_from(f'<{nwidth}B', data, pos))

    def glyph_bytes(self, width):
        return (width * self.height + 1) // 2

    def has(self, code):
        """True if `code` has its own glyph (not a gap aliased to defaultchar,
        not an uninitialized entry from a convttf load failure)."""
        i = code - self.firstchar
        if not 0 <= i < self.size:
            return False
        off, w = self.offsets[i], self.widths[i]
        if w == 0 or w > self.maxwidth or off + self.glyph_bytes(w) > len(self.bits):
            return False
        if code == self.defaultchar:
            return True
        d = self.defaultchar - self.firstchar
        return not (off == self.offsets[d] and w == self.widths[d])

    def glyph(self, code):
        i = code - self.firstchar
        off, w = self.offsets[i], self.widths[i]
        return self.bits[off:off + self.glyph_bytes(w)], w


def quantize(bits):
    """Snap 4-bit AA pixels to solid: nibbles 0..7 -> 0 (ink), 8..15 -> 0xF."""
    snap = bytes((0 if b & 0xF < 8 else 0xF) | (0 if b >> 4 < 8 else 0xF0)
                 for b in range(256))
    return bytes(snap[b] for b in bits)


def merge(basef, addf, outpath, quantize_add=False):
    base, add = Font(basef), Font(addf)
    for f in ('height', 'ascent', 'depth'):
        if getattr(base, f) != getattr(add, f):
            sys.exit(f'metric mismatch: {f} {getattr(base, f)} vs {getattr(add, f)}')
    if quantize_add:
        add.bits = quantize(add.bits)

    firstchar = min(base.firstchar, add.firstchar)
    lastchar = max(base.firstchar + base.size, add.firstchar + add.size) - 1
    size = lastchar - firstchar + 1

    # base bits verbatim, then all of add's bits rebased after them
    bits = base.bits + add.bits
    rebase = len(base.bits)

    dflt_i = base.defaultchar - base.firstchar
    dflt = (base.offsets[dflt_i], base.widths[dflt_i])

    offsets, widths = [], []
    n_base = n_add = 0
    for code in range(firstchar, lastchar + 1):
        if base.has(code):
            i = code - base.firstchar
            offsets.append(base.offsets[i]); widths.append(base.widths[i])
            n_base += 1
        elif add.has(code):
            i = code - add.firstchar
            offsets.append(add.offsets[i] + rebase); widths.append(add.widths[i])
            n_add += 1
        else:
            offsets.append(dflt[0]); widths.append(dflt[1])

    out = bytearray(HDR.pack(b'RB12', max(base.maxwidth, add.maxwidth),
                             base.height, base.ascent, base.depth,
                             firstchar, base.defaultchar, size,
                             len(bits), size, size))
    out += bits
    if len(bits) >= LONG_OFFSET_MIN:
        out += b'\0' * (-len(bits) & 3)
        out += struct.pack(f'<{size}L', *offsets)
    else:
        out += b'\0' * (-len(bits) & 1)
        out += struct.pack(f'<{size}H', *offsets)
    out += struct.pack(f'<{size}B', *widths)
    open(outpath, 'wb').write(out)

    # verify: every source glyph must read back byte-identical from the output
    m = Font(outpath)
    for src, own in ((base, base), (add, base)):
        for code in range(src.firstchar, src.firstchar + src.size):
            if not src.has(code) or (src is add and base.has(code)):
                continue
            if m.glyph(code) != src.glyph(code):
                sys.exit(f'verify FAILED at U+{code:04X}')
    print(f'{outpath}: {size} slots U+{firstchar:04X}..U+{lastchar:04X}, '
          f'{n_base} from base, {n_add} from add, '
          f'{size - n_base - n_add} aliased to default; verified OK')


def show(codearg, path):
    f = Font(path)
    code = int(codearg.replace('U+', '0x'), 16)
    print(f'{path}: h={f.height} ascent={f.ascent} maxw={f.maxwidth} '
          f'chars U+{f.firstchar:04X}+{f.size}')
    if not f.has(code):
        sys.exit(f'U+{code:04X}: no glyph (aliases defaultchar)')
    g, w = f.glyph(code)
    px = []
    for b in g:                       # 2 px/byte, low nibble first, 0xF = blank
        px += [b & 0xF, b >> 4]
    shades = '@#*+=-:. '
    for row in range(f.height):
        line = ''.join(shades[min(px[row * w + c] * 9 // 16, 8)]
                       for c in range(w))
        print(f'|{line}|' + ('  <- baseline' if row == f.ascent - 1 else ''))


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == '--show':
        show(sys.argv[2], sys.argv[3])
    elif len(sys.argv) == 5 and sys.argv[1] == '--quantize-add':
        merge(*sys.argv[2:5], quantize_add=True)
    elif len(sys.argv) == 4:
        merge(*sys.argv[1:4])
    else:
        sys.exit(__doc__)
