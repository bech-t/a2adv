#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pi1_to_pcx.py -- reconditionne un bitmap Atari ST .PI1 deja converti
(cf. player/atarist/img2st/img2st.py) en .PCX DOS (cf. img2dos.py), SANS
repartir de l'image source : reutilise tels quels les pixels et la palette
16 couleurs deja choisis pour le ST (cadrage, quantification, tramage --
souvent d'un rendu plus flatteur qu'une nouvelle quantification 254
couleurs faite a l'aveugle sur un visuel source proche du niveaux de gris).

Les index de pixel restent 0-15 (jamais touches par ce script) ; seules
deux entrees de palette SUPPLEMENTAIRES sont ecrites, 254 (blanc) et 255
(noir), pour le texte dessine par-dessus par scr.c -- jamais referencees
par le bitmap ST lui-meme, donc aucun conflit avec sa palette 0-15.

Usage :
    python3 pi1_to_pcx.py IMG00.PI1 -o IMG00.PCX
"""
import argparse
import struct
import sys

import numpy as np

ST_W = 320
ST_ROWS = 200
WORDS_PER_PLANE = ST_W // 16

PAL_WHITE = 254
PAL_BLACK = 255


def st_word_to_rgb(word):
    r3 = (word >> 8) & 7
    g3 = (word >> 4) & 7
    b3 = word & 7
    return (r3 * 255 // 7, g3 * 255 // 7, b3 * 255 // 7)


def load_pi1(path):
    data = open(path, "rb").read()
    rez = struct.unpack(">H", data[:2])[0]
    if rez != 0:
        raise ValueError("%s : pas en basse resolution ST (rez=%d)" % (path, rez))
    pal_words = struct.unpack(">16H", data[2:34])
    palette16 = [st_word_to_rgb(w) for w in pal_words]
    bitmap = data[34:34 + 32000]

    words = np.frombuffer(bitmap, dtype=">u2").astype(np.uint16)
    words = words.reshape(ST_ROWS, WORDS_PER_PLANE, 4)
    idx = np.zeros((ST_ROWS, ST_W), dtype=np.uint8)
    for p in range(4):
        w = words[:, :, p]
        for bit in range(16):
            mask = ((w >> (15 - bit)) & 1).astype(np.uint8)
            idx[:, bit::16] |= (mask << p)
    return idx, palette16


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("input", help=".PI1 source (Atari ST)")
    p.add_argument("-o", "--output", required=True, help=".PCX de sortie")
    args = p.parse_args(argv)

    idx, palette16 = load_pi1(args.input)

    # img2dos.py fournit encode_rle_scanline/build_pcx : reutilises tels
    # quels plutot que redupliques ici.
    sys.path.insert(0, __file__.rsplit("/", 1)[0])
    from img2dos import build_pcx, DOS_ROWS

    palette = [None] * 256
    for i, rgb in enumerate(palette16):
        palette[i] = rgb
    palette[PAL_WHITE] = (255, 255, 255)
    palette[PAL_BLACK] = (0, 0, 0)

    pcx = build_pcx(idx, DOS_ROWS, palette)
    with open(args.output, "wb") as f:
        f.write(pcx)
    print("%s ecrit (%d octets, repris de %s)" % (args.output, len(pcx), args.input))
    return 0


if __name__ == "__main__":
    sys.exit(main())
