#!/usr/bin/env python3
"""Regenerate the 512x384 Trimpod theme's 1-bit bitmap assets.

Icon sprite sheets use an exact 2x nearest-neighbour scale so every source
pixel stays square. The two progress-bar capsules are resized to the 448x22
logical slot used by the 512x384 WPS. No third-party image library is needed.
"""

from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = (
    ROOT
    / "assets"
    / "theme"
    / "1ST_GEN_REMIX"
    / ".rockbox"
    / "wps"
    / "1ST_GEN_REMIX"
)

TARGETS = {
    "battery.bmp": ((40, 90), (80, 180)),
    "lock.bmp": ((14, 18), (28, 36)),
    "playmodes.bmp": ((22, 198), (44, 396)),
    "repeat_status.bmp": ((25, 110), (50, 220)),
    "sad_mac.bmp": ((95, 120), (190, 240)),
    "shuffle.bmp": ((25, 44), (50, 88)),
    "pb.bmp": ((280, 14), (448, 22)),
    "pb_back.bmp": ((281, 14), (448, 22)),
}


def read_1bpp(path):
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError(f"{path}: not a BMP")

    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size, width, signed_height, planes, bpp, compression = struct.unpack_from(
        "<IiiHHI", data, 14
    )
    if dib_size < 40 or planes != 1 or bpp != 1 or compression != 0:
        raise ValueError(f"{path}: expected an uncompressed 1-bit BMP")

    height = abs(signed_height)
    top_down = signed_height < 0
    palette = data[14 + dib_size : pixel_offset]
    stride = (width + 31) // 32 * 4
    pixels = []
    for y in range(height):
        stored_y = y if top_down else height - 1 - y
        row = data[pixel_offset + stored_y * stride : pixel_offset + (stored_y + 1) * stride]
        pixels.append([(row[x // 8] >> (7 - x % 8)) & 1 for x in range(width)])
    return pixels, palette


def write_1bpp(path, pixels, palette):
    height = len(pixels)
    width = len(pixels[0])
    stride = (width + 31) // 32 * 4
    image_size = stride * height
    dib_size = 40
    palette = palette[:8].ljust(8, b"\0")
    pixel_offset = 14 + dib_size + len(palette)
    file_size = pixel_offset + image_size

    out = bytearray(struct.pack("<2sIHHI", b"BM", file_size, 0, 0, pixel_offset))
    out += struct.pack(
        "<IiiHHIIiiII",
        dib_size,
        width,
        height,
        1,
        1,
        0,
        image_size,
        2835,
        2835,
        2,
        2,
    )
    out += palette

    for row in reversed(pixels):
        packed = bytearray(stride)
        for x, value in enumerate(row):
            if value:
                packed[x // 8] |= 1 << (7 - x % 8)
        out += packed
    path.write_bytes(out)


def resize_nearest(pixels, width, height):
    src_h = len(pixels)
    src_w = len(pixels[0])
    return [
        [
            pixels[min(src_h - 1, (2 * y + 1) * src_h // (2 * height))][
                min(src_w - 1, (2 * x + 1) * src_w // (2 * width))
            ]
            for x in range(width)
        ]
        for y in range(height)
    ]


def regenerate(name, width, height):
    path = ASSET_DIR / name
    pixels, palette = read_1bpp(path)
    write_1bpp(path, resize_nearest(pixels, width, height), palette)
    print(f"{name}: {len(pixels[0])}x{len(pixels)} -> {width}x{height}")


def main():
    for name, (source_size, target_size) in TARGETS.items():
        path = ASSET_DIR / name
        pixels, _ = read_1bpp(path)
        current_size = (len(pixels[0]), len(pixels))
        if current_size == target_size:
            print(f"{name}: already {target_size[0]}x{target_size[1]}")
            continue
        if current_size != source_size:
            raise ValueError(
                f"{name}: expected {source_size} or {target_size}, got {current_size}"
            )
        regenerate(name, *target_size)


if __name__ == "__main__":
    main()
