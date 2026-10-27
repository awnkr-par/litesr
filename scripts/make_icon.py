"""Wrap the provided, unredesigned LiteSR PNG in a Windows ICO container.

Modern Windows supports PNG-compressed 256px ICO frames. This deliberately
avoids redrawing, recoloring, or generating substitute artwork.
"""
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[1]
png_file = root / "assets" / "litesr-original.png"
ico_file = root / "assets" / "litesr.ico"
png = png_file.read_bytes()
if not png.startswith(b"\x89PNG\r\n\x1a\n"):
    raise SystemExit("Original logo is not a valid PNG")
width, height = struct.unpack(">II", png[16:24])
if (width, height) != (256, 256):
    raise SystemExit(f"Expected 256x256 PNG; got {width}x{height}")
# ICONDIR, ICONDIRENTRY (0 width/height denote 256), then the original PNG.
ico = struct.pack("<HHH", 0, 1, 1)
ico += struct.pack("<BBBBHHII", 0, 0, 0, 0, 1, 32, len(png), 22)
ico += png
ico_file.write_bytes(ico)
print(f"Icon generated from original logo: {ico_file} ({len(ico)} bytes)")
