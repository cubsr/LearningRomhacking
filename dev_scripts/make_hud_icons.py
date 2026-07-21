#!/usr/bin/env python3
"""Generate graphics/interface/hud_icons.png: a 16x(16*N) indexed 4bpp-ready sheet."""
import zlib, struct, sys

PAL = [
    (255, 0, 255),    # 0 transparent
    (32, 32, 40),     # 1 outline black
    (248, 248, 248),  # 2 white
    (168, 176, 192),  # 3 light gray
    (88, 96, 112),    # 4 dark gray
    (216, 72, 64),    # 5 red
    (144, 40, 40),    # 6 dark red
    (72, 120, 216),   # 7 blue
    (240, 200, 72),   # 8 yellow
    (88, 192, 112),   # 9 green
    (168, 112, 64),   # 10 brown
    (240, 144, 48),   # 11 orange
    (48, 64, 112),    # 12 dark blue
    (0, 0, 0), (0, 0, 0), (0, 0, 0),
]

CH = {'.': 0, 'k': 1, 'w': 2, 'g': 3, 'd': 4, 'r': 5, 'R': 6,
      'b': 7, 'y': 8, 'n': 9, 'B': 10, 'o': 11, 'D': 12}

POKEDEX = """
................
..kkkkkkkkkkkk..
..kRRRRRRRRRRk..
..kRwwwwwwwwRk..
..kRwDDDDDDwRk..
..kRwDbbbbDwRk..
..kRwDDDDDDwRk..
..kRwwwwwwwwRk..
..kRRRRRRRRRRk..
..kRrrrrrrrrRk..
..kRrwwrrwwrRk..
..kRrrrrrrrrRk..
..kRRRRRRRRRRk..
..kkkkkkkkkkkk..
................
................
"""

PARTY = """
................
.....kkkkkk.....
...kkrrrrrrkk...
..krrrrrrrrrrk..
..krrrrrrrrrrk..
.krrrrrrrrrrrrk.
.krrrkkkkkkrrrk.
.kkkkkwwwwkkkkk.
.kwwwkwkkwkwwwk.
.kwwwkkwwkkwwwk.
.kwwwwkkkkwwwwk.
..kwwwwwwwwwwk..
..kwwwwwwwwwwk..
...kkwwwwwwkk...
.....kkkkkk.....
................
"""

BAG = """
................
.....kkkkkk.....
....kwwwwwwk....
...kwkkkkkkwk...
..kkkkkkkkkkkk..
..kBBBBBBBBBBk..
..kBBoooooBBBk..
..kBoooooooBBk..
..kBoowwwooBBk..
..kBowwwwwoBBk..
..kBoowwwooBBk..
..kBBoooooBBBk..
..kBBBBBBBBBBk..
..kkkkkkkkkkkk..
................
................
"""

PC = """
................
..kkkkkkkkkkkk..
..kwwwwwwwwwwk..
..kwDDDDDDDDwk..
..kwDbbbbbbDwk..
..kwDbggggbDwk..
..kwDbggggbDwk..
..kwDbbbbbbDwk..
..kwDDDDDDDDwk..
..kwwwwwwwwwwk..
..kkkkkkkkkkkk..
.....kkkkkk.....
.....kddddk.....
...kkkkkkkkkk...
...kwwwwwwwwk...
....kkkkkkkk....
"""

CARD = """
................
.kkkkkkkkkkkkkk.
.kDDDDDDDDDDDDk.
.kDwwwwDbbbbbDk.
.kDwddwDDDDDDDk.
.kDwddwDbbbbbDk.
.kDwwwwDDDDDDDk.
.kDwwwwDbbbbDDk.
.kDwwwwDDDDDDDk.
.kDDDDDDDDDDDDk.
.kDyyyyDyyyyDDk.
.kDDDDDDDDDDDDk.
.kkkkkkkkkkkkkk.
................
................
................
"""

SAVE = """
................
..kkkkkkkkkkkk..
..kDDDDDDDDDDk..
..kDwwwwwwDDDk..
..kDwddddwDDDk..
..kDwddddwDDDk..
..kDwwwwwwDDDk..
..kDDDDDDDDDDk..
..kDwwwwwwwwDk..
..kDwkkkkkkwDk..
..kDwkkkkkkwDk..
..kDwwwwwwwwDk..
..kkkkkkkkkkkk..
................
................
................
"""

OPTIONS = """
................
..kkkkkkkkkkkk..
..kwwwwwwwwwwk..
..kwddddddddwk..
..kwdwwddddwwk..
..kwddddddddwk..
..kwwwwwwwwwwk..
..kwddddddddwk..
..kwddddwwddwk..
..kwddddddddwk..
..kwwwwwwwwwwk..
..kwddddddddwk..
..kkkkkkkkkkkk..
................
................
................
"""

POKENAV = """
................
...kkkkkkkkkk...
...kbbbbbbbbk...
...kbwwwwwwbk...
...kbwnnnnwbk...
...kbwnnnnwbk...
...kbwnnnnwbk...
...kbwwwwwwbk...
...kbbbbbbbbk...
...kbwbwbwbbk...
...kbbbbbbbbk...
...kbwbwbwbbk...
...kbbbbbbbbk...
...kkkkkkkkkk...
................
................
"""

DEXNAV = """
................
.....kkkkk......
...kkwwwwwkk....
..kwwbbbbbwwk...
..kwbbwwwbbwk...
.kwbbwwwwwbbwk..
.kwbbwwwwwbbwk..
.kwbbwwwwwbbwk..
..kwbbwwwbbwk...
..kwwbbbbbwwk...
...kkwwwwwkkk...
.....kkkkkkkk...
..........kkkk..
...........kkk..
............kk..
................
"""

EXIT = """
................
..kkkkkkkkkkkk..
..kwwwwwwwwwwk..
..kwrrwwwwrrwk..
..kwrrrwwrrrwk..
..kwwrrrrrrwwk..
..kwwwrrrrwwwk..
..kwwwrrrrwwwk..
..kwwrrrrrrwwk..
..kwrrrwwrrrwk..
..kwrrwwwwrrwk..
..kwwwwwwwwwwk..
..kkkkkkkkkkkk..
................
................
................
"""

GENERIC = """
................
..kkkkkkkkkkkk..
..kwwwwwwwwwwk..
..kwwwddddwwwk..
..kwwddwwddwwk..
..kwwwwwwddwwk..
..kwwwwwddwwwk..
..kwwwwddwwwwk..
..kwwwwddwwwwk..
..kwwwwwwwwwwk..
..kwwwwddwwwwk..
..kwwwwwwwwwwk..
..kkkkkkkkkkkk..
................
................
................
"""

CURSOR = """
kkkkkkkkkkkkkkkk
kyyyyyyyyyyyyyyk
kyk..........kyk
kyk..........kyk
k.............yk
k..............k
k..............k
k..............k
k..............k
k..............k
k..............k
k..............k
ky.............k
kyk..........kyk
kyk..........kyk
kyyyyyyyyyyyyyyk
"""

ICONS = [POKEDEX, PARTY, BAG, PC, CARD, SAVE, OPTIONS,
         POKENAV, DEXNAV, EXIT, GENERIC]


def parse(art, name):
    rows = [r for r in art.strip('\n').split('\n')]
    assert len(rows) == 16, f"{name}: {len(rows)} rows"
    for i, r in enumerate(rows):
        assert len(r) == 16, f"{name}: row {i} has {len(r)} cols"
    return [[CH[c] for c in r] for r in rows]


def main(path):
    pixels = []
    for i, art in enumerate(ICONS):
        pixels += parse(art, f"icon{i}")
    w, h = 16, len(pixels)

    raw = b''.join(b'\x00' + bytes(row) for row in pixels)
    plte = b''.join(bytes(c) for c in PAL)
    trns = b'\x00' + b'\xff' * (len(PAL) - 1)

    def chunk(tag, data):
        c = tag + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c))

    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 3, 0, 0, 0))
           + chunk(b'PLTE', plte)
           + chunk(b'tRNS', trns)
           + chunk(b'IDAT', zlib.compress(raw, 9))
           + chunk(b'IEND', b''))
    with open(path, 'wb') as f:
        f.write(png)
    print(f"wrote {path}: {w}x{h}, {len(ICONS)} icons")


main(sys.argv[1])
