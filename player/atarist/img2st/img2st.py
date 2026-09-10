#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
img2st.py -- convertit une image en bitmap basse resolution Atari ST
(320x200, 16 couleurs, format Degas .PI1 reel : 2 o resolution + 32 o
palette + 32000 o bitmap planaire) charge par scr_load_hgr (player/atarist/
src/scr.c).

Contrairement a l'Apple II (6 couleurs d'artefact NTSC fixes, cf.
player/apple2/img2hgr/img2hgr.py), le ST a une vraie palette RGB : chaque
image choisit 14 couleurs par quantification + tramage Floyd-Steinberg
(Pillow). Les couleurs 0 (noir) et 1 (blanc) sont TOUJOURS reservees (cf.
quantize_st) : ce sont celles que scr.c force pour le texte, donc un texte
dessine par-dessus l'image en mode --mixed reste lisible (fond noir,
texte blanc) quelle que soit la palette propre a l'image.

Sortie : un .PI1 lisible tel quel par n'importe quel outil/visionneuse
Atari ST -- pas un format invente pour l'occasion.

Exemples :
    python3 img2st.py photo.jpg -o PIC.PI1 --preview pic.png
    python3 img2st.py logo.png  -o PIC.PI1 --mixed --preset logo
"""

import argparse
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageEnhance, ImageFilter

ST_W = 320
ST_ROWS = 200
ST_MIXED_ROWS = 160         # memes proportions que MIXED_H d'img2hgr.py (160/192)
WORDS_PER_PLANE = ST_W // 16    # 20
ROW_BYTES = WORDS_PER_PLANE * 4 * 2   # 160 o/ligne (4 plans, mot = 2 o)
BITMAP_SIZE = ROW_BYTES * ST_ROWS     # 32000 o
N_COLORS = 16


# --------------------------------------------------------------------------
# Preparation de l'image source (identique en esprit a img2hgr.py, juste la
# geometrie ST : pixel LEGEREMENT plus haut que large en basse resolution,
# cf. 320x200 affiche en 4:3 -> facteur de largeur 0.833 au lieu des 0.75
# de l'Apple II)
# --------------------------------------------------------------------------
ST_PIXEL_WFACTOR = 0.833

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
                   pre=None, smooth=None):
    """Retourne un tableau float32 (rows, 320, 3)."""
    img = Image.open(path).convert("RGB")

    if pre:
        img = preprocess(img, **pre)

    if fit == "stretch":
        img = img.resize((ST_W, rows), Image.LANCZOS)
    elif fit == "fit":
        src_w, src_h = img.size
        scale = min(ST_W / (src_w * ST_PIXEL_WFACTOR), rows / src_h)
        new_w = max(1, int(round(src_w * ST_PIXEL_WFACTOR * scale)))
        new_h = max(1, int(round(src_h * scale)))
        img = img.resize((new_w, new_h), Image.LANCZOS)
        canvas = Image.new("RGB", (ST_W, rows), (0, 0, 0))
        canvas.paste(img, ((ST_W - new_w) // 2, (rows - new_h) // 2))
        img = canvas
    elif fit == "crop":
        src_w, src_h = img.size
        scale = max(ST_W / (src_w * ST_PIXEL_WFACTOR), rows / src_h)
        new_w = max(ST_W, int(round(src_w * ST_PIXEL_WFACTOR * scale)))
        new_h = max(rows, int(round(src_h * scale)))
        img = img.resize((new_w, new_h), Image.LANCZOS)
        left = (new_w - ST_W) // 2
        top = (new_h - rows) // 2
        img = img.crop((left, top, left + ST_W, top + rows))
    else:
        raise ValueError("fit inconnu : " + fit)

    if sharpen > 0.0:
        img = img.filter(ImageFilter.UnsharpMask(
            radius=1.2, percent=int(100 * sharpen), threshold=2))

    arr = np.asarray(img, dtype=np.float32)
    if smooth:
        arr = edge_preserving_smooth(arr, strength=smooth)
    if gamma != 1.0:
        arr = 255.0 * np.power(np.clip(arr / 255.0, 0, 1), gamma)
    return arr


def edge_preserving_smooth(arr, strength=2.0, iters=2):
    sigma_s = max(0.5, strength)
    sigma_r = 14.0 * strength
    out = arr.copy()
    rad = max(1, int(round(sigma_s * 2)))
    sw = np.exp(-(np.arange(-rad, rad + 1) ** 2) / (2 * sigma_s ** 2))
    for _ in range(int(max(1, iters))):
        for axis in (0, 1):
            acc = np.zeros_like(out)
            wsum = np.zeros(out.shape[:2], dtype=np.float32)
            for k, d in enumerate(range(-rad, rad + 1)):
                s = np.roll(out, d, axis=axis)
                diff = ((s - out) ** 2).sum(axis=-1)
                w = sw[k] * np.exp(-diff / (2 * sigma_r ** 2))
                acc += s * w[..., None]
                wsum += w
            out = acc / wsum[..., None]
    return out


# --------------------------------------------------------------------------
# Quantification 14 couleurs adaptatives + 2 reservees (index 0 = noir,
# index 1 = blanc) + tramage. Ces deux index sont ceux que scr.c force pour
# le texte (fond noir, texte blanc) : les reserver ici garantit qu'un texte
# dessine par-dessus l'image en mode mixte tombe sur les memes deux couleurs,
# quelle que soit la palette propre a l'image. Noir a l'index 0 aussi parce
# que c'est la couleur des lignes non couvertes par l'image en mode --mixed
# (cf. pack_planes : completees a 0) -- se fond avec le fond noir du texte.
# --------------------------------------------------------------------------
def quantize_st(arr, dither=True):
    """arr : float32 (rows, 320, 3). Renvoie (idx uint8 (rows,320) valeurs
    0..15, palette liste de 16 tuples RGB 0..255)."""
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB")
    q = img.quantize(
        colors=N_COLORS - 2, method=Image.MEDIANCUT,
        dither=Image.FLOYDSTEINBERG if dither else Image.NONE)
    pal_flat = q.getpalette()[: (N_COLORS - 2) * 3]
    idx = np.asarray(q, dtype=np.uint8) + 2     # decale : 0 et 1 deviennent libres
    palette = [(0, 0, 0), (255, 255, 255)] + [
        tuple(pal_flat[i * 3: i * 3 + 3]) for i in range(N_COLORS - 2)
    ]
    return idx, palette


def rgb_to_st_word(rgb):
    r, g, b = rgb
    r3 = min(7, int(round(r * 7 / 255)))
    g3 = min(7, int(round(g * 7 / 255)))
    b3 = min(7, int(round(b * 7 / 255)))
    return (r3 << 8) | (g3 << 4) | b3


def st_word_to_rgb(word):
    r3 = (word >> 8) & 7
    g3 = (word >> 4) & 7
    b3 = word & 7
    return (r3 * 255 // 7, g3 * 255 // 7, b3 * 255 // 7)


# --------------------------------------------------------------------------
# Empaquetage planaire (4 plans entrelaces par mot, MSB = pixel de gauche)
# --------------------------------------------------------------------------
def pack_planes(idx, rows_used):
    """idx : uint8 (rows_used, 320), valeurs 0..15. Complete a ST_ROWS avec
    des lignes a 0 (noir, cf. quantize_st) pour une image --mixed."""
    full = np.zeros((ST_ROWS, ST_W), dtype=np.uint16)
    full[:rows_used] = idx

    weights = (1 << np.arange(15, -1, -1)).astype(np.uint16)
    planes = []
    for p in range(4):
        bits = ((full >> p) & 1).reshape(ST_ROWS, WORDS_PER_PLANE, 16)
        words = (bits * weights).sum(axis=2).astype(np.uint16)   # (200,20)
        planes.append(words)
    # (200,20,4) -> par ligne : pour chaque groupe de 16 px, plan0..plan3
    stacked = np.stack(planes, axis=2).reshape(ST_ROWS, WORDS_PER_PLANE * 4)
    return stacked.astype(">u2").tobytes()


def build_pi1(idx, rows_used, palette):
    header = struct.pack(">H", 0)             # 0 = basse resolution
    pal_words = [rgb_to_st_word(c) for c in palette]
    pal_bytes = struct.pack(">16H", *pal_words)
    bitmap = pack_planes(idx, rows_used)
    return header + pal_bytes + bitmap


# --------------------------------------------------------------------------
# Apercu : redecode le .PI1 produit (verifie l'empaquetage, pas juste ce
# que Pillow a genere avant conversion)
# --------------------------------------------------------------------------
def render_preview(pi1_bytes, scale=3):
    pal_words = struct.unpack(">16H", pi1_bytes[2:34])
    palette = [st_word_to_rgb(w) for w in pal_words]
    bitmap = pi1_bytes[34:]
    words = np.frombuffer(bitmap, dtype=">u2").astype(np.uint16)
    words = words.reshape(ST_ROWS, WORDS_PER_PLANE, 4)

    idx = np.zeros((ST_ROWS, ST_W), dtype=np.uint8)
    for p in range(4):
        w = words[:, :, p]                                   # (200,20)
        for bit in range(16):
            mask = ((w >> (15 - bit)) & 1).astype(np.uint8)
            idx[:, bit::16] |= (mask << p)

    lut = np.array(palette, dtype=np.uint8)                  # (16,3)
    img = lut[idx]                                           # (200,320,3)

    out = Image.fromarray(img, "RGB")
    w = ST_W * scale
    h = int(round(w * 3 / 4))                                 # 4:3 comme img2hgr.py
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
    d.text((w + gap + 4, 5), "ST 320x200/16c", fill=(200, 200, 200))
    return out


# --------------------------------------------------------------------------
def main(argv=None):
    p = argparse.ArgumentParser(
        description="Convertit une image en bitmap basse resolution "
                     "Atari ST (.PI1, 320x200, 16 couleurs).")
    p.add_argument("input", help="image source (png, jpg, ...)")
    p.add_argument("-o", "--output", default="PIC.PI1",
                   help="fichier .PI1 de sortie (defaut: PIC.PI1)")
    p.add_argument("--mixed", action="store_true",
                   help="160 lignes utiles seulement (fenetre de texte de "
                        "4 rangees en bas, cf. scr_gfx_mixed)")
    p.add_argument("--fit", choices=("stretch", "fit", "crop"),
                   default="stretch",
                   help="stretch = deforme, fit = bandes noires, crop = recadre")
    p.add_argument("--no-dither", action="store_true",
                   help="quantification sans tramage (aplats plus nets, "
                        "plus de bandes de posterisation)")

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
    g.add_argument("--smooth", type=float, default=None, metavar="S")

    p.add_argument("--preview", metavar="PNG", nargs="?", const="auto",
                   help="ecrit un apercu PNG redecode du .PI1 produit")
    p.add_argument("--compare", metavar="PNG", nargs="?", const="auto",
                   help="ecrit un PNG source / resultat cote a cote")
    p.add_argument("--scale", type=int, default=3)
    args = p.parse_args(argv)

    rows = ST_MIXED_ROWS if args.mixed else ST_ROWS

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
                        sharpen=base["sharpen"], pre=pre, smooth=args.smooth)

    idx, palette = quantize_st(arr, dither=not args.no_dither)
    pi1 = build_pi1(idx, rows, palette)

    with open(args.output, "wb") as f:
        f.write(pi1)
    print("%s ecrit (%d octets, %d lignes utiles/%d)" %
          (args.output, len(pi1), rows, ST_ROWS))

    preview = None
    if args.preview or args.compare:
        preview = render_preview(pi1, scale=args.scale)

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
