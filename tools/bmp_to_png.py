#!/usr/bin/env python3
"""Converte as capturas BMP do simulador em PNG (so biblioteca padrao).

    python tools/bmp_to_png.py <pasta_ou_arquivo.bmp> <pasta_saida>

O simulador grava BMP porque e um formato trivial de escrever em C; o PNG
fica bem menor e e o que vai para a documentacao e para os slides.
"""
import struct
import sys
import zlib
from pathlib import Path


def read_bmp(path: Path):
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError(f"{path} nao e BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp != 24:
        raise ValueError(f"{path}: esperado BMP de 24 bits")
    stride = (width * 3 + 3) & ~3
    top_down = height < 0
    height = abs(height)
    rows = []
    for y in range(height):
        src_y = y if top_down else height - 1 - y
        row = data[offset + src_y * stride: offset + src_y * stride + width * 3]
        # BMP guarda BGR; PNG quer RGB
        rgb = bytearray(len(row))
        rgb[0::3] = row[2::3]
        rgb[1::3] = row[1::3]
        rgb[2::3] = row[0::3]
        rows.append(bytes(rgb))
    return width, height, rows


def write_png(path: Path, width: int, height: int, rows):
    def chunk(tag: bytes, payload: bytes) -> bytes:
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + r for r in rows)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src = Path(sys.argv[1])
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    files = sorted(src.glob("*.bmp")) if src.is_dir() else [src]
    for f in files:
        w, h, rows = read_bmp(f)
        dest = out / (f.stem + ".png")
        write_png(dest, w, h, rows)
        print(f"{dest} ({dest.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
