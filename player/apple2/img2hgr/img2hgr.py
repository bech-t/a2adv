#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
img2hgr.py — convertit une image en page HIRES Apple II (8192 octets, $2000).

Deux modes :
  --mode color  (defaut) : 6 couleurs d'artefact NTSC (noir, violet, vert,
                           bleu, orange, blanc), recherche exhaustive par
                           octet + diffusion d'erreur.
  --mode mono            : noir & blanc 280x192, dithering Floyd-Steinberg,
                           bit 7 toujours a 0.

Deux hauteurs :
  plein ecran  : 192 lignes
  --mixed      : 160 lignes (les 4 lignes de texte du bas restent libres)

Sortie : fichier .bin de 8192 octets a charger tel quel a $2000 (page 1)
         ou $4000 (page 2), + un apercu PNG optionnel.

Exemples :
    python3 img2hgr.py photo.jpg -o PIC.BIN --preview pic.png
    python3 img2hgr.py logo.png  -o PIC.BIN --mode mono --mixed
"""

import argparse
import os
import sys

import numpy as np
from PIL import Image, ImageEnhance, ImageFilter

HGR_W = 280
HGR_H = 192
MIXED_H = 160
BYTES_PER_ROW = 40
PAGE_SIZE = 8192

# Palette d'artefact NTSC (valeurs classiques type AppleWin)
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
VIOLET = (255, 68, 253)
GREEN = (20, 245, 60)
BLUE = (20, 207, 253)
ORANGE = (255, 106, 60)


# --------------------------------------------------------------------------
# Adressage entrelace de la page HIRES
# --------------------------------------------------------------------------
def hgr_row_offset(y):
    """Offset (0..8191) du debut de la ligne y dans la page HIRES."""
    return 0x28 * (y // 64) + 0x80 * ((y // 8) % 8) + 0x400 * (y % 8)


# --------------------------------------------------------------------------
# Table des couleurs rendues pour chaque octet possible
# tbl[parite_x, bit_gauche, bit7, motif, pixel, canal]
# --------------------------------------------------------------------------
def build_color_table(swap_palette=False):
    if swap_palette:
        pal0 = (GREEN, VIOLET)   # (x pair, x impair), bit7 = 0
        pal1 = (ORANGE, BLUE)    # bit7 = 1
    else:
        pal0 = (VIOLET, GREEN)
        pal1 = (BLUE, ORANGE)

    tbl = np.zeros((2, 2, 2, 128, 7, 3), dtype=np.float32)
    for parity in range(2):
        for left in range(2):
            for hi in range(2):
                pal = pal1 if hi else pal0
                for pat in range(128):
                    bits = [(pat >> i) & 1 for i in range(7)]
                    for i in range(7):
                        if not bits[i]:
                            col = BLACK
                        else:
                            lb = bits[i - 1] if i > 0 else left
                            rb = bits[i + 1] if i < 6 else 0
                            # deux points voisins allumes => blanc
                            col = WHITE if (lb or rb) else pal[(parity + i) % 2]
                        tbl[parity, left, hi, pat, i] = col
    return tbl


# --------------------------------------------------------------------------
# Preparation de l'image source
# --------------------------------------------------------------------------
PRESETS = {
    "photo": dict(denoise=3, autolevels=True, saturation=1.45,
                  contrast=1.10, sharpen=1.6, gamma=0.90),
    "flat":  dict(denoise=3, autolevels=False, saturation=1.20,
                  contrast=1.05, sharpen=1.0, gamma=1.0),
    "logo":  dict(denoise=0, autolevels=True, saturation=1.0,
                  contrast=1.40, sharpen=0.0, gamma=1.0),
}


def autolevels(img, clip=0.01):
    """Etire l'histogramme sur la luminance, canaux lies (pas de derive
    de teinte). clip = fraction de pixels ecretee en haut et en bas."""
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
    """Nettoyage applique a pleine resolution, avant reduction."""
    if denoise:
        size = denoise if denoise % 2 else denoise + 1
        # median : enleve le grain et les artefacts JPEG sans noyer les
        # contours, ce qu'un flou gaussien ferait.
        img = img.filter(ImageFilter.MedianFilter(size=size))
    if do_autolevels:
        img = autolevels(img)
    if brightness != 1.0:
        img = ImageEnhance.Brightness(img).enhance(brightness)
    if contrast != 1.0:
        img = ImageEnhance.Contrast(img).enhance(contrast)
    if saturation != 1.0:
        # la trame de dithering desature le resultat : on compense avant.
        img = ImageEnhance.Color(img).enhance(saturation)
    return img


def prepare_image(path, rows, fit="stretch", gamma=1.0, sharpen=0.0,
                  pre=None, smooth=None):
    """Retourne un tableau float32 (rows, 280, 3)."""
    img = Image.open(path).convert("RGB")

    if pre:
        img = preprocess(img, **pre)

    if fit == "stretch":
        img = img.resize((HGR_W, rows), Image.LANCZOS)
    elif fit == "fit":
        # lettre-boxing : on conserve le rapport en tenant compte du pixel
        # non carre de l'Apple II (~0.75 de large pour 1 de haut a l'ecran).
        src_w, src_h = img.size
        scale = min(HGR_W / (src_w * 0.75), rows / src_h)
        new_w = max(1, int(round(src_w * 0.75 * scale)))
        new_h = max(1, int(round(src_h * scale)))
        img = img.resize((new_w, new_h), Image.LANCZOS)
        canvas = Image.new("RGB", (HGR_W, rows), (0, 0, 0))
        canvas.paste(img, ((HGR_W - new_w) // 2, (rows - new_h) // 2))
        img = canvas
    elif fit == "crop":
        src_w, src_h = img.size
        scale = max(HGR_W / (src_w * 0.75), rows / src_h)
        new_w = max(HGR_W, int(round(src_w * 0.75 * scale)))
        new_h = max(rows, int(round(src_h * scale)))
        img = img.resize((new_w, new_h), Image.LANCZOS)
        left = (new_w - HGR_W) // 2
        top = (new_h - rows) // 2
        img = img.crop((left, top, left + HGR_W, top + rows))
    else:
        raise ValueError("fit inconnu : " + fit)

    if sharpen > 0.0:
        # apres reduction : recupere le detail perdu, la resolution couleur
        # utile n'etant que de 140 points de large.
        img = img.filter(ImageFilter.UnsharpMask(
            radius=1.2, percent=int(100 * sharpen), threshold=2))

    arr = np.asarray(img, dtype=np.float32)
    if smooth:
        # sur le tableau reduit : c'est la que le fourmillement se decide,
        # et le contour reste net grace au lissage bilateral.
        arr = edge_preserving_smooth(arr, strength=smooth)
    if gamma != 1.0:
        arr = 255.0 * np.power(np.clip(arr / 255.0, 0, 1), gamma)
    return arr


def edge_preserving_smooth(arr, strength=2.0, iters=2):
    """Lissage bilateral : aplatit les aplats bruites tout en gardant les
    contours nets. C'est ce que ne fait pas un flou classique, qui baverait
    sur les bords. Applique sur le tableau reduit, juste avant conversion."""
    sigma_s = max(0.5, strength)
    sigma_r = 14.0 * strength         # tolerance couleur liee a la force
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


def _box(a, r=2):
    """Moyenne glissante (2r+1)x(2r+1) en numpy pur, bords repliques."""
    pad = np.pad(a, r, mode="edge")
    c = np.cumsum(np.cumsum(pad, axis=0), axis=1)
    c = np.pad(c, ((1, 0), (1, 0)), mode="constant")
    k = 2 * r + 1
    h, w = a.shape
    s = (c[k:k + h, k:k + w] - c[:h, k:k + w]
         - c[k:k + h, :w] + c[:h, :w])
    return s / float(k * k)


def _local_std(arr):
    """Ecart-type local de luminance (fenetre 5x5), pour reperer les aplats."""
    lum = (0.299 * arr[..., 0] + 0.587 * arr[..., 1] +
           0.114 * arr[..., 2]).astype(np.float32)
    m = _box(lum, 2)
    m2 = _box(lum * lum, 2)
    return np.sqrt(np.maximum(m2 - m * m, 0.0))


# --------------------------------------------------------------------------
# Conversion couleur : recherche exhaustive par octet + diffusion d'erreur
# --------------------------------------------------------------------------
def convert_color(arr, table, damp=0.85, clean=False, flat_thresh=8.0):
    rows = arr.shape[0]
    work = arr.copy()
    out = np.zeros((rows, BYTES_PER_ROW), dtype=np.uint8)

    # En mode "clean", on repere les zones plates : la diffusion d'erreur y
    # est coupee et l'octet est choisi sur la couleur d'origine, ce qui donne
    # un motif regulier au lieu du fourmillement habituel.
    std = _local_std(arr) if clean else None

    for y in range(rows):
        left_bit = 0
        for bx in range(BYTES_PER_ROW):
            x0 = bx * 7
            is_flat = clean and float(std[y, x0:x0 + 7].max()) < flat_thresh
            # zone plate : on vise la couleur d'origine, pas l'accumulation
            target = arr[y, x0:x0 + 7] if is_flat else work[y, x0:x0 + 7]
            cand = table[x0 & 1, left_bit]                 # (2,128,7,3)
            err = ((cand - target) ** 2).sum(axis=(2, 3))  # (2,128)
            best = int(np.argmin(err))
            hi, pat = divmod(best, 128)

            out[y, bx] = pat | (0x80 if hi else 0x00)
            left_bit = (pat >> 6) & 1

            if is_flat:
                continue                                   # aucun residu diffuse

            resid = (target - cand[hi, pat]) * damp
            for i in range(7):
                r = resid[i]
                x = x0 + i
                if i == 6 and x + 1 < HGR_W:
                    work[y, x + 1] += r * (7 / 16)
                    dw, dl, dr = 5 / 16, 3 / 16, 1 / 16
                else:
                    dw, dl, dr = 5 / 9, 3 / 9, 1 / 9
                if y + 1 < rows:
                    work[y + 1, x] += r * dw
                    if x > 0:
                        work[y + 1, x - 1] += r * dl
                    if x + 1 < HGR_W:
                        work[y + 1, x + 1] += r * dr
    return out


# --------------------------------------------------------------------------
# Conversion monochrome : Floyd-Steinberg classique
# --------------------------------------------------------------------------
def convert_mono(arr, threshold=128.0, invert=False):
    rows = arr.shape[0]
    lum = (0.299 * arr[:, :, 0] + 0.587 * arr[:, :, 1] +
           0.114 * arr[:, :, 2]).astype(np.float32)
    bits = np.zeros((rows, HGR_W), dtype=np.uint8)

    for y in range(rows):
        for x in range(HGR_W):
            old = lum[y, x]
            new = 255.0 if old >= threshold else 0.0
            bits[y, x] = 1 if new > 0 else 0
            e = old - new
            if x + 1 < HGR_W:
                lum[y, x + 1] += e * 7 / 16
            if y + 1 < rows:
                if x > 0:
                    lum[y + 1, x - 1] += e * 3 / 16
                lum[y + 1, x] += e * 5 / 16
                if x + 1 < HGR_W:
                    lum[y + 1, x + 1] += e * 1 / 16
    if invert:
        bits ^= 1

    out = np.zeros((rows, BYTES_PER_ROW), dtype=np.uint8)
    for bx in range(BYTES_PER_ROW):
        chunk = bits[:, bx * 7:bx * 7 + 7]
        val = np.zeros(rows, dtype=np.uint8)
        for i in range(7):
            val |= (chunk[:, i] << i).astype(np.uint8)
        out[:, bx] = val
    return out


# --------------------------------------------------------------------------
# Mise en page memoire + apercu
# --------------------------------------------------------------------------
def pack_page(rowbytes):
    page = bytearray(PAGE_SIZE)
    rows = rowbytes.shape[0]
    for y in range(rows):
        off = hgr_row_offset(y)
        page[off:off + BYTES_PER_ROW] = rowbytes[y].tobytes()
    return bytes(page)


def render_preview(rowbytes, swap_palette=False, scale=3, ntsc=False,
                   screen="color"):
    """Re-decode les octets choisis pour verifier le rendu."""
    if swap_palette:
        pal0, pal1 = (GREEN, VIOLET), (ORANGE, BLUE)
    else:
        pal0, pal1 = (VIOLET, GREEN), (BLUE, ORANGE)

    rows = rowbytes.shape[0]
    bits = np.zeros((rows, HGR_W), dtype=np.uint8)
    his = np.zeros((rows, HGR_W), dtype=np.uint8)
    for bx in range(BYTES_PER_ROW):
        b = rowbytes[:, bx]
        for i in range(7):
            bits[:, bx * 7 + i] = (b >> i) & 1
            his[:, bx * 7 + i] = (b >> 7) & 1

    img = np.zeros((rows, HGR_W, 3), dtype=np.uint8)
    if screen in ("mono", "green", "amber"):
        fg = {"mono": (255, 255, 255),
              "green": (120, 255, 130),
              "amber": (255, 176, 40)}[screen]
        img[bits.astype(bool)] = fg
    else:
        for y in range(rows):
            for x in range(HGR_W):
                if not bits[y, x]:
                    continue
                lb = bits[y, x - 1] if x > 0 else 0
                rb = bits[y, x + 1] if x + 1 < HGR_W else 0
                if lb or rb:
                    img[y, x] = WHITE
                else:
                    pal = pal1 if his[y, x] else pal0
                    img[y, x] = pal[x % 2]

    # En mode mixte, on complete par la zone de texte (4 lignes = 32 pixels)
    # pour retrouver les proportions reelles de l'ecran.
    if rows < HGR_H:
        pad = np.zeros((HGR_H - rows, HGR_W, 3), dtype=np.uint8)
        img = np.vstack([img, pad])

    if ntsc:
        img = ntsc_bleed(img)

    out = Image.fromarray(img, "RGB")
    # 280x192 s'affiche dans un cadre 4/3 : le pixel HIRES n'est pas carre.
    w = HGR_W * scale
    h = int(round(w * 3 / 4))
    return out.resize((w, h), Image.LANCZOS if ntsc else Image.NEAREST)


def ntsc_bleed(img, radius=2):
    """Fondu horizontal grossier des couleurs, facon signal composite."""
    a = img.astype(np.float32)
    k = np.array([1, 2, 3, 2, 1], dtype=np.float32)
    k /= k.sum()
    pad = np.pad(a, ((0, 0), (radius, radius), (0, 0)), mode="edge")
    acc = np.zeros_like(a)
    for i, w in enumerate(k):
        acc += pad[:, i:i + img.shape[1]] * w
    return np.clip(acc, 0, 255).astype(np.uint8)


def make_compare(src_path, preview, rows):
    """Original a gauche, resultat de la conversion a droite."""
    from PIL import ImageDraw

    w, h = preview.size
    orig = Image.open(src_path).convert("RGB")
    canvas = Image.new("RGB", (w, h), (0, 0, 0))
    ratio = min(w / orig.width, h / orig.height)
    small = orig.resize((max(1, int(orig.width * ratio)),
                         max(1, int(orig.height * ratio))), Image.LANCZOS)
    canvas.paste(small, ((w - small.width) // 2, (h - small.height) // 2))

    gap = 12
    band = 22
    out = Image.new("RGB", (w * 2 + gap, h + band), (24, 24, 24))
    out.paste(canvas, (0, band))
    out.paste(preview, (w + gap, band))
    d = ImageDraw.Draw(out)
    d.text((4, 5), "source", fill=(200, 200, 200))
    d.text((w + gap + 4, 5),
           "HIRES 280x%d" % rows, fill=(200, 200, 200))
    return out


# --------------------------------------------------------------------------
def main(argv=None):
    p = argparse.ArgumentParser(
        description="Convertit une image en page HIRES Apple II (8192 octets).")
    p.add_argument("input", help="image source (png, jpg, ...)")
    p.add_argument("-o", "--output", default="PIC.BIN",
                   help="fichier binaire de sortie (defaut: PIC.BIN)")
    p.add_argument("--mode", choices=("color", "mono"), default="color")
    p.add_argument("--mixed", action="store_true",
                   help="160 lignes seulement (4 lignes de texte en bas)")
    p.add_argument("--fit", choices=("stretch", "fit", "crop"),
                   default="stretch",
                   help="stretch = deforme, fit = bandes noires, crop = recadre")
    g = p.add_argument_group("traitement de l'image (avant conversion)")
    g.add_argument("--preset", choices=("none",) + tuple(PRESETS),
                   default="none",
                   help="reglages groupes : photo, flat (dessin/aplats), "
                        "logo (trait net)")
    g.add_argument("--denoise", type=int, default=None, metavar="N",
                   help="filtre median NxN : enleve grain et artefacts JPEG "
                        "que le dithering amplifierait (3 ou 5)")
    g.add_argument("--autolevels", action="store_true", default=None,
                   help="etire l'histogramme sur toute la plage disponible")
    g.add_argument("--saturation", type=float, default=None, metavar="S",
                   help="compense la desaturation due au dithering (1.3-1.6)")
    g.add_argument("--contrast", type=float, default=None, metavar="C")
    g.add_argument("--brightness", type=float, default=None, metavar="B")
    g.add_argument("--sharpen", type=float, default=None, metavar="S",
                   help="masque flou apres reduction (0.8-2.0)")
    g.add_argument("--gamma", type=float, default=None,
                   help="correction gamma avant conversion (essayer 0.8-1.2)")

    c = p.add_argument_group("fond propre (mode couleur)")
    c.add_argument("--clean", action="store_true",
                   help="diffusion d'erreur bridee : les zones uniformes "
                        "rendent un motif regulier au lieu de fourmiller")
    c.add_argument("--flat", type=float, default=8.0, metavar="T",
                   help="seuil de detection des zones plates pour --clean : "
                        "plus haut = fond plus propre mais plus aplati "
                        "(defaut 8, essayer 5 a 14)")
    c.add_argument("--smooth", type=float, default=None, metavar="S",
                   help="lissage anti-bruit qui preserve les contours, "
                        "applique juste avant conversion (essayer 1 a 3)")
    p.add_argument("--damp", type=float, default=0.85,
                   help="attenuation de la diffusion d'erreur (mode color)")
    p.add_argument("--threshold", type=float, default=128.0,
                   help="seuil du dithering (mode mono)")
    p.add_argument("--invert", action="store_true", help="inverse (mode mono)")
    p.add_argument("--swap-palette", action="store_true",
                   help="echange violet/vert et bleu/orange si le rendu reel "
                        "ne correspond pas a l'apercu")
    p.add_argument("--preview", metavar="PNG", nargs="?", const="auto",
                   help="ecrit un apercu PNG du resultat "
                        "(sans argument : nom du .bin avec l'extension .png)")
    p.add_argument("--compare", metavar="PNG", nargs="?", const="auto",
                   help="ecrit un PNG source / resultat cote a cote")
    p.add_argument("--ntsc", action="store_true",
                   help="apercu adouci facon signal composite (plus proche "
                        "d'un ecran d'epoque qu'un moniteur RVB)")
    p.add_argument("--screen", choices=("color", "mono", "green", "amber"),
                   default="color",
                   help="type de moniteur simule dans l'apercu (defaut: color)")
    p.add_argument("--scale", type=int, default=3,
                   help="facteur d'agrandissement de l'apercu (defaut: 3)")
    p.add_argument("--asm", metavar="S",
                   help="ecrit aussi un fichier .s avec des directives .byte")
    args = p.parse_args(argv)

    rows = MIXED_H if args.mixed else HGR_H

    # un preset fournit les valeurs par defaut ; toute option explicite
    # passee en ligne de commande a la priorite.
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

    if args.mode == "color":
        table = build_color_table(args.swap_palette)
        rowbytes = convert_color(arr, table, damp=args.damp,
                                 clean=args.clean, flat_thresh=args.flat)
    else:
        rowbytes = convert_mono(arr, threshold=args.threshold,
                                invert=args.invert)

    page = pack_page(rowbytes)
    with open(args.output, "wb") as f:
        f.write(page)
    print("%s ecrit (%d octets, %d lignes, mode %s)" %
          (args.output, len(page), rows, args.mode))

    if args.preview or args.compare:
        preview = render_preview(rowbytes, args.swap_palette,
                                 scale=args.scale, ntsc=args.ntsc,
                                 screen=args.screen)

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
        make_compare(args.input, preview, rows).save(path)
        print("comparaison : %s" % path)

    if args.asm:
        with open(args.asm, "w") as f:
            f.write("; page HIRES generee par img2hgr.py\n")
            f.write("; a charger a $2000 (page 1) ou $4000 (page 2)\n")
            f.write("picture:\n")
            for i in range(0, PAGE_SIZE, 16):
                chunk = page[i:i + 16]
                f.write("        .byte " +
                        ",".join("$%02X" % b for b in chunk) + "\n")
        print("source assembleur : %s" % args.asm)

    return 0


if __name__ == "__main__":
    sys.exit(main())
