"""Build LiteSR's Windows icon from the original uploaded artwork.

No redrawing, restyling, or substitution.
"""
from pathlib import Path
import struct
import zlib

root = Path(__file__).resolve().parents[1]
png_file = root / "assets" / "litesr-original.png"
ico_file = root / "assets" / "litesr.ico"
png = png_file.read_bytes()
if not png.startswith(b"\x89PNG\r\n\x1a\n"):
    raise SystemExit("Logo is not a PNG")

# Validate every PNG chunk, including its CRC, before embedding it in an ICO.
pos = 8
while pos + 12 <= len(png):
    length = struct.unpack(">I", png[pos:pos + 4])[0]
    end = pos + 12 + length
    if end > len(png):
        raise SystemExit("PNG chunk exceeds the input file")
    chunk = png[pos + 4:pos + 8]
    content = png[pos + 8:pos + 8 + length]
    stored = struct.unpack(">I", png[pos + 8 + length:end])[0]
    actual = zlib.crc32(chunk + content) & 0xFFFFFFFF
    if actual != stored:
        raise SystemExit("PNG failed chunk CRC validation")
    pos = end
    if chunk == b"IEND":
        break
if pos != len(png) or chunk != b"IEND":
    raise SystemExit("PNG is truncated or contains invalid trailing data")
width, height = struct.unpack(">II", png[16:24])
if (width, height) != (128, 128):
    raise SystemExit(f"Expected 128x128 icon, found {width}x{height}")
ico = struct.pack("<HHH", 0, 1, 1)
ico += struct.pack("<BBBBHHII", width, height, 0, 0, 1, 32, len(png), 22)
ico += png
ico_file.write_bytes(ico)
print(f"Created verified Windows icon: {ico_file} ({len(ico)} bytes)")
