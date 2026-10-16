#!/usr/bin/env python3
"""Render a binary STL with a small orthographic z-buffer (Python stdlib only)."""
import math
import struct
import sys
import zlib
from pathlib import Path

src = Path(sys.argv[1] if len(sys.argv) > 1 else "Kibo.stl")
out = Path(sys.argv[2] if len(sys.argv) > 2 else "docs/images/kibo-case-render.png")
data = src.read_bytes()
if len(data) < 84:
    raise SystemExit("Not a binary STL")
count = struct.unpack_from("<I", data, 80)[0]
if 84 + count * 50 > len(data):
    raise SystemExit("STL facet table is truncated")
triangles = []
offset = 84
for _ in range(count):
    v = struct.unpack_from("<12fH", data, offset)
    offset += 50
    triangles.append((v[3:6], v[6:9], v[9:12]))

# Camera looks down onto the model from above and from its front-right corner.
right = (math.sqrt(0.5), math.sqrt(0.5), 0.0)
up = (-1 / math.sqrt(6), 1 / math.sqrt(6), 2 / math.sqrt(6))
toward_camera = (1 / math.sqrt(3), -1 / math.sqrt(3), 1 / math.sqrt(3))
verts = [p for tri in triangles for p in tri]
center = tuple((min(p[i] for p in verts) + max(p[i] for p in verts)) / 2 for i in range(3))
def dot(a, b): return sum(x * y for x, y in zip(a, b))
def project(p):
    q = tuple(p[i] - center[i] for i in range(3))
    return dot(q, right), dot(q, up), dot(q, toward_camera)
projected = [[project(p) for p in tri] for tri in triangles]
W, H = 1100, 760
all2d = [p for tri in projected for p in tri]
minx, maxx = min(p[0] for p in all2d), max(p[0] for p in all2d)
miny, maxy = min(p[1] for p in all2d), max(p[1] for p in all2d)
scale = min(850 / (maxx - minx), 550 / (maxy - miny))
ox = (W - (maxx - minx) * scale) / 2 - minx * scale
oy = (H - (maxy - miny) * scale) / 2 + maxy * scale
screen = [[(ox + p[0] * scale, oy - p[1] * scale, p[2]) for p in tri] for tri in projected]
background = (249, 247, 242)
pixels = bytearray(background * (W * H))
depths = [-float("inf")] * (W * H)
light = (0.25, -0.55, 0.80)
ln = math.sqrt(dot(light, light)); light = tuple(c / ln for c in light)
base = (240, 112, 34)
for tri3, tri2 in zip(triangles, screen):
    (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = tri2
    area = (x1-x0)*(y2-y0) - (y1-y0)*(x2-x0)
    if abs(area) < 1e-8: continue
    ax, ay, az = tri3[0]; bx, by, bz = tri3[1]; cx, cy, cz = tri3[2]
    ab, ac = (bx-ax, by-ay, bz-az), (cx-ax, cy-ay, cz-az)
    n = (ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0])
    nl = math.sqrt(dot(n,n)) or 1.0
    diffuse = abs(dot(tuple(c/nl for c in n), light))
    shade = 0.33 + 0.67 * diffuse
    color = tuple(max(0, min(255, int(c * shade))) for c in base)
    xmin, xmax = max(0, int(min(x0,x1,x2))), min(W-1, int(max(x0,x1,x2)+1))
    ymin, ymax = max(0, int(min(y0,y1,y2))), min(H-1, int(max(y0,y1,y2)+1))
    for py in range(ymin, ymax+1):
        y = py + 0.5
        for px in range(xmin, xmax+1):
            x = px + 0.5
            w1 = ((x-x0)*(y2-y0) - (y-y0)*(x2-x0)) / area
            w2 = ((x1-x0)*(y-y0) - (y1-y0)*(x-x0)) / area
            w0 = 1.0 - w1 - w2
            if w0 < -1e-5 or w1 < -1e-5 or w2 < -1e-5: continue
            depth = w0*z0 + w1*z1 + w2*z2
            idx = py*W + px
            if depth <= depths[idx]: continue
            depths[idx] = depth
            j = idx * 3
            pixels[j:j+3] = bytes(color)

def png_chunk(kind, payload):
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff)
raw = b"".join(b"\x00" + pixels[y*W*3:(y+1)*W*3] for y in range(H))
png = b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", struct.pack(">2I5B", W, H, 8, 2, 0, 0, 0)) + png_chunk(b"IDAT", zlib.compress(raw, 9)) + png_chunk(b"IEND", b"")
out.parent.mkdir(parents=True, exist_ok=True)
out.write_bytes(png)
print(f"Rendered {count} STL facets to {out} ({W}x{H})")
