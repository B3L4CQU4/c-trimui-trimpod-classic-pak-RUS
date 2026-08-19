#!/usr/bin/env python3
"""Merge two Rockbox RB12 .fnt files (stdlib only).

    mergefnt.py [options] BASE.fnt ADD.fnt OUT.fnt
    mergefnt.py --show U+3042 FONT.fnt        ASCII-render one glyph (debug)

--quantize-add snaps ADD's 4-bit pixels to solid ink/blank.  Use it when ADD
is a pixel font rasterized at an exact integer scale (e.g. PixelMplus12 at
24ppem = 2x): any intermediate shade is FreeType hinter drift, not design.
--quantize-base does the same for BASE. --spacing N adds N blank pixels to the
right side of every visible glyph (blank space glyphs keep their normal width).

--prefer-add SPEC makes ADD replace BASE for the listed Unicode characters.
SPEC is comma-separated and accepts U+0401 and U+0410-U+044F forms.
--fit-add-metrics aligns ADD to BASE's baseline and pads/crops its cell when
height/ascent differ. --synthesize-yo creates U+0401 from ADD's U+0415 when the
source font supplies every other Russian letter but lacks uppercase Yo.

The merged font keeps BASE's metrics and defaultchar. Chars absent from both
alias BASE's defaultchar glyph, matching what convttf emits for gaps. Format
per tools/convttf.c writer / firmware/font.c font_load_header: 36-byte header,
4-bit AA pixel data (2 px/byte, low nibble first, 0xF=blank), byte-aligned per
glyph, then the offset table (u16, or u32 when nbits >= 0xFFDB) and width table.

The complete, reproducible PixelMplus + Mulmaru recipe lives in
tools/build_trimpod_fonts.sh.  convttf numeric arguments are decimal only.
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


def decode_glyph(bits, width, height):
    values = []
    for byte in bits:
        values.extend((byte & 0x0f, byte >> 4))
    values = values[:width * height]
    return [values[y * width:(y + 1) * width] for y in range(height)]


def encode_glyph(rows):
    values = [value for row in rows for value in row]
    if len(values) & 1:
        values.append(0x0f)
    return bytes(values[i] | (values[i + 1] << 4)
                 for i in range(0, len(values), 2))


def add_glyph_spacing(glyph, height, columns):
    """Increase a visible glyph's advance by appending blank pixel columns."""
    if columns <= 0:
        return glyph
    bits, width = glyph
    rows = decode_glyph(bits, width, height)
    # Keep regular/non-breaking spaces unchanged: spacing belongs between
    # visible glyphs and must not also inflate the gap between words.
    if not any(value < 0x0f for row in rows for value in row):
        return glyph
    width += columns
    if width > 255:  # the RB12 width table stores one byte per glyph
        sys.exit(f'glyph width {width} exceeds RB12 limit after spacing')
    for row in rows:
        row.extend([0x0f] * columns)
    return encode_glyph(rows), width


def render_add_glyph(add, code, base, fit_metrics, quantize_add):
    bits, width = add.glyph(code)
    if add.height == base.height and add.ascent == base.ascent:
        rendered = bits
    else:
        if not fit_metrics:
            sys.exit('metric mismatch: ADD is '
                     f'h={add.height}/ascent={add.ascent}, BASE is '
                     f'h={base.height}/ascent={base.ascent}; '
                     'pass --fit-add-metrics')
        source = decode_glyph(bits, width, add.height)
        rows = [[0x0f] * width for _ in range(base.height)]
        row_offset = base.ascent - add.ascent
        for source_y, row in enumerate(source):
            target_y = source_y + row_offset
            if 0 <= target_y < base.height:
                rows[target_y] = row[:]
        rendered = encode_glyph(rows)
    return (quantize(rendered) if quantize_add else rendered), width


def synthesize_upper_yo(add, base, fit_metrics, quantize_add):
    """Build U+0401 from ADD's U+0415 using pixel dots above the E glyph."""
    bits, width = render_add_glyph(add, 0x0415, base, fit_metrics, quantize_add)
    rows = decode_glyph(bits, width, base.height)
    ink_rows = [y for y, row in enumerate(rows) if any(value < 0x0f for value in row)]
    if not ink_rows:
        sys.exit('cannot synthesize U+0401: ADD U+0415 is blank')

    top = ink_rows[0]
    dot_size = max(1, base.height // 24)
    dot_y = max(0, top - dot_size - 1)
    centers = (max(1, width // 3), min(width - 2, (2 * width) // 3))
    for center in centers:
        start_x = max(0, center - (dot_size - 1) // 2)
        for y in range(dot_y, min(base.height, dot_y + dot_size)):
            for x in range(start_x, min(width, start_x + dot_size)):
                rows[y][x] = 0
    return encode_glyph(rows), width


def parse_codepoints(spec):
    result = set()
    for part in spec.split(','):
        part = part.strip().upper().replace('U+', '')
        if not part:
            continue
        if '-' in part:
            first, last = part.split('-', 1)
            result.update(range(int(first, 16), int(last, 16) + 1))
        else:
            result.add(int(part, 16))
    return result


def merge(basef, addf, outpath, quantize_add=False, quantize_base=False,
          prefer_add=None, fit_add_metrics=False, synthesize_yo=False,
          spacing=0):
    base, add = Font(basef), Font(addf)
    if base.depth != add.depth:
        sys.exit(f'metric mismatch: depth {base.depth} vs {add.depth}')
    prefer_add = prefer_add or set()

    for code in sorted(prefer_add):
        can_synthesize = synthesize_yo and code == 0x0401 and add.has(0x0415)
        if not add.has(code) and not can_synthesize:
            sys.exit(f'preferred ADD glyph U+{code:04X} is missing')

    firstchar = min(base.firstchar, add.firstchar,
                    min(prefer_add) if prefer_add else base.firstchar)
    lastchar = max(base.firstchar + base.size - 1,
                   add.firstchar + add.size - 1,
                   max(prefer_add) if prefer_add else base.firstchar)
    size = lastchar - firstchar + 1

    bits = bytearray()
    entries = []
    n_base = n_add = 0
    for code in range(firstchar, lastchar + 1):
        glyph = None
        if code in prefer_add and add.has(code):
            glyph = render_add_glyph(add, code, base, fit_add_metrics, quantize_add)
            n_add += 1
        elif code in prefer_add and synthesize_yo and code == 0x0401:
            glyph = synthesize_upper_yo(add, base, fit_add_metrics, quantize_add)
            n_add += 1
        elif base.has(code):
            glyph = base.glyph(code)
            if quantize_base:
                glyph = quantize(glyph[0]), glyph[1]
            n_base += 1
        elif add.has(code):
            glyph = render_add_glyph(add, code, base, fit_add_metrics, quantize_add)
            n_add += 1
        if glyph:
            glyph = add_glyph_spacing(glyph, base.height, spacing)
            glyph_bits, width = glyph
            entries.append((len(bits), width))
            bits += glyph_bits
        else:
            entries.append(None)

    default_index = base.defaultchar - firstchar
    if not 0 <= default_index < len(entries) or entries[default_index] is None:
        sys.exit(f'BASE default glyph U+{base.defaultchar:04X} is unavailable')
    default_entry = entries[default_index]
    offsets = [entry[0] if entry else default_entry[0] for entry in entries]
    widths = [entry[1] if entry else default_entry[1] for entry in entries]

    out = bytearray(HDR.pack(b'RB12', max(widths),
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

    # Verify BASE glyphs that were not overridden and ensure every preferred
    # character resolves to a real glyph in the result.
    m = Font(outpath)
    for code in range(base.firstchar, base.firstchar + base.size):
        if base.has(code) and code not in prefer_add:
            expected = base.glyph(code)
            if quantize_base:
                expected = quantize(expected[0]), expected[1]
            expected = add_glyph_spacing(expected, base.height, spacing)
            if m.glyph(code) != expected:
                sys.exit(f'verify FAILED at U+{code:04X}')
    for code in prefer_add:
        if not m.has(code):
            sys.exit(f'verify FAILED: preferred U+{code:04X} is missing')
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
    args = sys.argv[1:]
    if len(args) == 3 and args[0] == '--show':
        show(args[1], args[2])
        sys.exit(0)

    kwargs = {
        'quantize_add': False,
        'quantize_base': False,
        'prefer_add': set(),
        'fit_add_metrics': False,
        'synthesize_yo': False,
        'spacing': 0,
    }
    positional = []
    i = 0
    while i < len(args):
        arg = args[i]
        if arg == '--quantize-add':
            kwargs['quantize_add'] = True
        elif arg == '--quantize-base':
            kwargs['quantize_base'] = True
        elif arg == '--fit-add-metrics':
            kwargs['fit_add_metrics'] = True
        elif arg == '--synthesize-yo':
            kwargs['synthesize_yo'] = True
        elif arg == '--prefer-add':
            i += 1
            if i >= len(args):
                sys.exit('--prefer-add requires a Unicode specification')
            kwargs['prefer_add'].update(parse_codepoints(args[i]))
        elif arg.startswith('--prefer-add='):
            kwargs['prefer_add'].update(parse_codepoints(arg.split('=', 1)[1]))
        elif arg == '--spacing':
            i += 1
            if i >= len(args):
                sys.exit('--spacing requires a non-negative integer')
            kwargs['spacing'] = int(args[i])
        elif arg.startswith('--spacing='):
            kwargs['spacing'] = int(arg.split('=', 1)[1])
        elif arg.startswith('--'):
            sys.exit(f'unknown option: {arg}\n{__doc__}')
        else:
            positional.append(arg)
        i += 1

    if kwargs['spacing'] < 0:
        sys.exit('--spacing requires a non-negative integer')

    if len(positional) != 3:
        sys.exit(__doc__)
    merge(*positional, **kwargs)
