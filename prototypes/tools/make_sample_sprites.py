"""Writes the original sample sprites used by the editor prototypes.

The sprites are drawn here as text so they are 100% original (no third-party art)
and so anyone can see exactly what the pixels are. Each character in a row is one
pixel; the legend maps characters to RGBA colors ('.' is transparent).

Only the Python standard library is used: a PNG is just a small header, the raw
pixel rows compressed with zlib, and CRC checksums.

Run from the repository root:  python prototypes/tools/make_sample_sprites.py
"""

import pathlib
import struct
import zlib

OUT_DIR = pathlib.Path(__file__).resolve().parent.parent / "sample_assets"

LEGEND = {
    ".": (0, 0, 0, 0),          # transparent
    "k": (24, 24, 40, 255),     # outline
    "g": (120, 128, 150, 255),  # metal grey
    "w": (230, 236, 245, 255),  # highlight
    "c": (40, 200, 230, 255),   # cyan eye
    "r": (220, 60, 60, 255),    # red antenna light
    "y": (250, 200, 40, 255),   # gold
    "o": (200, 130, 20, 255),   # dark gold
}

# 16x16 robot, drawn at 1 character per pixel.
ROBOT = [
    ".......rr.......",
    ".......kk.......",
    "....kkkkkkkk....",
    "...kwwggggggk...",
    "...kwgggggggk...",
    "...kgcckkccgk...",
    "...kgcckkccgk...",
    "...kggggggggk...",
    "...kgkkkkkkgk...",
    "....kkkkkkkk....",
    "..kkggggggggkk..",
    ".kgkwggggggkgk..",
    ".kgkggggggggkgk.",
    ".kk.kggggggk.kk.",
    "....kgk..kgk....",
    "....kkk..kkk....",
]

# 8x8 coin.
COIN = [
    "..kkkk..",
    ".kyyyyk.",
    "kyywyyok",
    "kyywyyok",
    "kyyyyyok",
    "kyyyyyok",
    ".kooook.",
    "..kkkk..",
]


def write_png(path: pathlib.Path, rows: list[str]) -> None:
    """Encodes `rows` as an 8-bit RGBA PNG file."""
    height = len(rows)
    width = len(rows[0])
    raw = bytearray()
    for row in rows:
        raw.append(0)  # PNG filter type 0 ("None") at the start of every row
        for ch in row:
            raw.extend(LEGEND[ch])

    def chunk(kind: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(kind + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", crc)

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)  # 8-bit RGBA
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    path.write_bytes(png)
    print(f"wrote {path} ({width}x{height})")


if __name__ == "__main__":
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    write_png(OUT_DIR / "robot.png", ROBOT)
    write_png(OUT_DIR / "coin.png", COIN)
