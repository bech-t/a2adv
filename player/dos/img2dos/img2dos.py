#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
img2dos.py -- convertit une image en bitmap VGA mode 13h (320x200, 256
couleurs) au format .PCX (ZSoft, RLE -- format DOS standard de l'epoque,
pas invente pour l'occasion, cf. le Degas .PI1 reel cote Atari ST) charge
par scr_load_hgr (player/dos/src/scr.c).

Deux index de palette sont TOUJOURS reserves (cf. quantize_dos) : 255 = noir,
254 = blanc -- ce sont ceux que scr.c utilise pour dessiner du texte
par-dessus l'image (mode mixte), quelle que soit la palette propre a
l'image -- meme principe que le "blanc=0/noir=1" reserve d'img2st.py cote
Atari ST (mais deux couleurs SUPPLEMENTAIRES, pas au prix des 14 couleurs
utiles restantes comme sur le ST a 16 couleurs : le mode 13h en garde 254).

Exemples :
    python3 img2dos.py photo.jpg -o PIC.PCX --preview pic.png
    python3 img2dos.py logo.png  -o PIC.PCX --mixed --preset logo
"""

import argparse
import os
import sys

import numpy as np
from PIL import Image, ImageEnhance, ImageFilter

DOS_W = 320
DOS_ROWS = 200
DOS_MIXED_ROWS = 160     # memes proportions que MIXED_ROWS d'img2st.py : 4
                          # rangees de texte (32px) + 1 rangee d'air (8px)
N_COLORS = 256
PAL_WHITE = 254
PAL_BLACK = 255

PRESETS = {
    "photo": dict(denoise=3, autolevels=True, saturation=1.30,
                  contrast=1.05, sharpen=1.4, gamma=0.95),
    "flat":  dict(denoise=3, autolevels=False, saturation=1.10,
                  contrast=1.05, sharpen=1.0, gamma=1.0),
    "logo":  dict(denoise=0, autolevels=True, saturation=1.0,
                  contrast=1.30, sharpen=0.0, gamma=1.0),
}


def autolevels(img, clip=0.01):
    a = np.asarray(img, dtype=np.float32)
    lum = 0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2]
    lo = float(np.quantile(lum, clip))
    hi = float(np.quantile(lum, 1.0 - clip))
    if hi - lo < 1.0:
        return img
    a = (a - lo) * (255.0 / (hi - lo))
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), "RGB")


def preprocess(img, denoise=0, do_autolevels=False, brightness=1.0,
               contrast=1.0, saturation=1.0):
    if denoise:
        size = denoise if denoise % 2 else denoise + 1
        img = img.filter(ImageFilter.MedianFilter(size=size))
    if do_autolevels:
        img = autolevels(img)
    if brightness != 1.0:
        img = ImageEnhance.Brightness(img).enhance(brightness)
    if contrast != 1.0:
        img = ImageEnhance.Contrast(img).enhance(contrast)
    if saturation != 1.0:
        img = ImageEnhance.Color(img).enhance(saturation)
    return img


def prepare_image(path, rows, fit="stretch", gamma=1.0, sharpen=0.0,
                   pre=None):
    """Retourne un tableau float32 (rows, 320, 3)."""
    img = Image.open(path).convert("RGB")

    if pre:
        img = preprocess(img, **pre)

    if fit == "stretch":
        img = img.resize((DOS_W, rows), Image.LANCZOS)
    elif fit == "fit":
        src_w, src_h = img.size
        scale = min(DOS_W / src_w, rows / src_h)
        new_w = max(1, int(round(src_w * scale)))
        new_h = max(1, int(round(src_h * scale)))
        img = img.resize((new_w, new_h), Image.LANCZOS)
        canvas = Image.new("RGB", (DOS_W, rows), (0, 0, 0))
        canvas.paste(img, ((DOS_W - new_w) // 2, (rows - new_h) // 2))
        img = canvas
    elif fit == "crop":
        src_w, src_h = img.size
        scale = max(DOS_W / src_w, rows / src_h)
        new_w = max(DOS_W, int(round(src_w * scale)))
        new_h = max(rows, int(round(src_h * scale)))
        img = img.resize((new_w, new_h), Image.LANCZOS)
        left = (new_w - DOS_W) // 2
        top = (new_h - rows) // 2
        img = img.crop((left, top, left + DOS_W, top + rows))
    else:
        raise ValueError("fit inconnu : " + fit)

    if sharpen > 0.0:
        img = img.filter(ImageFilter.UnsharpMask(
            radius=1.2, percent=int(100 * sharpen), threshold=2))

    arr = np.asarray(img, dtype=np.float32)
    if gamma != 1.0:
        arr = 255.0 * np.power(np.clip(arr / 255.0, 0, 1), gamma)
    return arr


# --------------------------------------------------------------------------
# Quantification 254 couleurs adaptatives + 2 reservees (254=blanc,
# 255=noir, cf. entete).
# --------------------------------------------------------------------------
def quantize_dos(arr, dither=True):
    """arr : float32 (rows, 320, 3). Renvoie (idx uint8 (rows,320) valeurs
    0..255, palette liste de 256 tuples RGB 0..255)."""
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB")
    q = img.quantize(
        colors=N_COLORS - 2, method=Image.MEDIANCUT,
        dither=Image.FLOYDSTEINBERG if dither else Image.NONE)
    pal_flat = q.getpalette()[: (N_COLORS - 2) * 3]
    idx = np.asarray(q, dtype=np.uint8)      # 0..253, deja libres de conflit
    palette = [None] * 256
    for i in range(N_COLORS - 2):
        palette[i] = tuple(pal_flat[i * 3: i * 3 + 3])
    palette[PAL_WHITE] = (255, 255, 255)
    palette[PAL_BLACK] = (0, 0, 0)
    return idx, palette


# --------------------------------------------------------------------------
# Encodage RLE d'une rangee (symetrique du decodeur, player/dos/src/scr.c :
# un octet >= 0xC0 est un compteur de repetition 1-63 (6 bits bas) suivi de
# la valeur repetee ; sinon l'octet EST le pixel. Toute valeur >= 0xC0 doit
# donc etre ECHAPPEE meme seule (sinon elle serait relue comme un compteur).
# --------------------------------------------------------------------------
def encode_rle_scanline(row_bytes):
    out = bytearray()
    n = len(row_bytes)
    i = 0
    while i < n:
        val = row_bytes[i]
        run = 1
        while i + run < n and row_bytes[i + run] == val and run < 63:
            run += 1
        if run > 1 or val >= 0xC0:
            out.append(0xC0 | run)
            out.append(val)
        else:
            out.append(val)
        i += run
    return bytes(out)


def decode_rle(data, width, height):
    """Symetrique de l'encodeur -- sert aussi a l'apercu (--preview),
    verifie ainsi le fichier .PCX ECRIT, pas juste ce que Pillow/numpy ont
    produit avant encodage."""
    out = np.empty((height, width), dtype=np.uint8)
    pos = 0
    for row in range(height):
        col = 0
        while col < width:
            b = data[pos]; pos += 1
            if (b & 0xC0) == 0xC0:
                run = b & 0x3F
                val = data[pos]; pos += 1
            else:
                run, val = 1, b
            out[row, col:col + run] = val
            col += run
    return out


# --------------------------------------------------------------------------
# Fichier .PCX (ZSoft, "version 5" -- palette 256 couleurs en fin de
# fichier) : en-tete fixe 128 o (cf. le decodeur C pour le detail des
# champs), pixels RLE (1 plan, 8 bpp), puis marqueur 0x0C + 768 o de
# palette RVB PLEINE ECHELLE (0-255/canal -- pas de conversion DAC ici,
# c'est une affaire du chargeur VGA, cf. scr.c:scr_load_hgr).
# --------------------------------------------------------------------------
def build_pcx(idx, rows_used, palette):
    """idx : uint8 (rows_used, 320). Complete a DOS_ROWS avec des lignes a
    PAL_BLACK (cf. quantize_dos) pour une image --mixed -- la fenetre de
    texte du bas (scr_gfx_mixed) doit tomber sur un fond noir coherent (texte
    blanc par-dessus, comme le mode texte plein ecran)."""
    full = np.full((DOS_ROWS, DOS_W), PAL_BLACK, dtype=np.uint8)
    full[:rows_used] = idx

    header = bytearray(128)
    header[0] = 0x0A                                      # Manufacturer (ZSoft)
    header[1] = 5                                         # Version (256 couleurs)
    header[2] = 1                                         # Encoding (RLE)
    header[3] = 8                                         # BitsPerPixel
    header[4:6]   = (0).to_bytes(2, "little")             # Xmin
    header[6:8]   = (0).to_bytes(2, "little")             # Ymin
    header[8:10]  = (DOS_W - 1).to_bytes(2, "little")     # Xmax
    header[10:12] = (DOS_ROWS - 1).to_bytes(2, "little")  # Ymax
    header[12:14] = (320).to_bytes(2, "little")           # HDpi
    header[14:16] = (200).to_bytes(2, "little")           # VDpi
    header[65] = 1                                        # NPlanes
    header[66:68] = (DOS_W).to_bytes(2, "little")         # BytesPerLine
    header[68:70] = (1).to_bytes(2, "little")             # PaletteInfo (couleur)
    header[70:72] = (DOS_W).to_bytes(2, "little")         # HscreenSize
    header[72:74] = (DOS_ROWS).to_bytes(2, "little")      # VscreenSize

    body = bytearray()
    for row in range(DOS_ROWS):
        body += encode_rle_scanline(full[row].tobytes())

    pal_bytes = bytearray(768)
    for i, rgb in enumerate(palette):
        pal_bytes[i * 3: i * 3 + 3] = bytes(rgb if rgb else (0, 0, 0))

    return bytes(header) + bytes(body) + bytes([0x0C]) + bytes(pal_bytes)


def render_preview(pcx_bytes, scale=3):
    pal_bytes = pcx_bytes[-768:]
    body = pcx_bytes[128:-769]
    palette = [tuple(pal_bytes[i * 3: i * 3 + 3]) for i in range(256)]
    idx = decode_rle(body, DOS_W, DOS_ROWS)
    lut = np.array(palette, dtype=np.uint8)
    img = lut[idx]
    out = Image.fromarray(img, "RGB")
    w = DOS_W * scale
    h = int(round(w * 3 / 4))
    return out.resize((w, h), Image.NEAREST)


def make_compare(src_path, preview):
    from PIL import ImageDraw
    w, h = preview.size
    orig = Image.open(src_path).convert("RGB")
    canvas = Image.new("RGB", (w, h), (0, 0, 0))
    ratio = min(w / orig.width, h / orig.height)
    small = orig.resize((max(1, int(orig.width * ratio)),
                         max(1, int(orig.height * ratio))), Image.LANCZOS)
    canvas.paste(small, ((w - small.width) // 2, (h - small.height) // 2))

    gap, band = 12, 22
    out = Image.new("RGB", (w * 2 + gap, h + band), (24, 24, 24))
    out.paste(canvas, (0, band))
    out.paste(preview, (w + gap, band))
    d = ImageDraw.Draw(out)
    d.text((4, 5), "source", fill=(200, 200, 200))
    d.text((w + gap + 4, 5), "DOS VGA 320x200/256c", fill=(200, 200, 200))
    return out


def main(argv=None):
    p = argparse.ArgumentParser(
        description="Convertit une image en bitmap VGA mode 13h "
                     "(.PCX, 320x200, 256 couleurs).")
    p.add_argument("input", help="image source (png, jpg, ...)")
    p.add_argument("-o", "--output", default="PIC.PCX",
                   help="fichier .PCX de sortie (defaut: PIC.PCX)")
    p.add_argument("--mixed", action="store_true",
                   help="160 lignes utiles seulement (fenetre de texte de "
                        "4 rangees en bas, cf. scr_gfx_mixed)")
    p.add_argument("--fit", choices=("stretch", "fit", "crop"),
                   default="stretch",
                   help="stretch = deforme, fit = bandes noires, crop = recadre")
    p.add_argument("--no-dither", action="store_true",
                   help="quantification sans tramage")

    g = p.add_argument_group("traitement de l'image (avant conversion)")
    g.add_argument("--preset", choices=("none",) + tuple(PRESETS),
                   default="none")
    g.add_argument("--denoise", type=int, default=None, metavar="N")
    g.add_argument("--autolevels", action="store_true", default=None)
    g.add_argument("--saturation", type=float, default=None, metavar="S")
    g.add_argument("--contrast", type=float, default=None, metavar="C")
    g.add_argument("--brightness", type=float, default=None, metavar="B")
    g.add_argument("--sharpen", type=float, default=None, metavar="S")
    g.add_argument("--gamma", type=float, default=None)

    p.add_argument("--preview", metavar="PNG", nargs="?", const="auto",
                   help="ecrit un apercu PNG redecode du .PCX produit")
    p.add_argument("--compare", metavar="PNG", nargs="?", const="auto",
                   help="ecrit un PNG source / resultat cote a cote")
    p.add_argument("--scale", type=int, default=3)
    args = p.parse_args(argv)

    rows = DOS_MIXED_ROWS if args.mixed else DOS_ROWS

    base = dict(denoise=0, autolevels=False, saturation=1.0, contrast=1.0,
                brightness=1.0, sharpen=0.0, gamma=1.0)
    if args.preset != "none":
        base.update(PRESETS[args.preset])
    for key in list(base):
        val = getattr(args, key)
        if val is not None:
            base[key] = val

    pre = dict(denoise=base["denoise"], do_autolevels=base["autolevels"],
               brightness=base["brightness"], contrast=base["contrast"],
               saturation=base["saturation"])
    arr = prepare_image(args.input, rows, fit=args.fit, gamma=base["gamma"],
                        sharpen=base["sharpen"], pre=pre)

    idx, palette = quantize_dos(arr, dither=not args.no_dither)
    pcx = build_pcx(idx, rows, palette)

    with open(args.output, "wb") as f:
        f.write(pcx)
    print("%s ecrit (%d octets, %d lignes utiles/%d)" %
          (args.output, len(pcx), rows, DOS_ROWS))

    preview = None
    if args.preview or args.compare:
        preview = render_preview(pcx, scale=args.scale)

    if args.preview:
        path = args.preview
        if path == "auto":
            path = os.path.splitext(args.output)[0] + ".png"
        preview.save(path)
        print("apercu : %s" % path)

    if args.compare:
        path = args.compare
        if path == "auto":
            path = os.path.splitext(args.output)[0] + "-compare.png"
        make_compare(args.input, preview).save(path)
        print("comparaison : %s" % path)

    return 0


if __name__ == "__main__":
    sys.exit(main())
