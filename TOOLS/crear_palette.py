# gpl_to_palette_dat.py

import sys

INPUT = "palette.gpl"
OUTPUT = "palette.dat"

colors = []

with open(INPUT, "r", encoding="utf-8") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("#") or line.startswith("GIMP"):
            continue

        parts = line.split()
        if len(parts) < 3:
            continue

        r = int(parts[0]) // 4
        g = int(parts[1]) // 4
        b = int(parts[2]) // 4

        colors.append((r, g, b))

# Aseguramos 256 entradas
while len(colors) < 256:
    colors.append((0, 0, 0))

colors = colors[:256]

with open(OUTPUT, "wb") as f:
    for r, g, b in colors:
        f.write(bytes([r, g, b]))

print("palette.dat generado (768 bytes)")
