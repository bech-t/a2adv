"""Site statique du player web : les aventures publiees et leur catalogue.

    python3 -m a2c.site -o ../player/webng/public combat_demo chateau_hante

Pour chaque aventure `adventures/<nom>/<nom>.adv`, ecrit dans
`<out>/adventures/<nom>/<hash>/` :

    story.json           l'histoire resolue (cf. webjson.py)
    img/IMGnn.webp       les images `@image`, dans l'ordre de `story.assets`
                         (source : `img/web/<ID EN MAJUSCULES>.png`)
    cover.webp           la couverture du catalogue : `img/web/MENU.png`, ou
                         a defaut `img/MENU.HGR` (ecran-titre Apple II)

puis `<out>/catalog.json`, que le player lit pour lister les aventures.

Le hash est celui du contenu du dossier : une aventure corrigee change de
dossier, et un fichier d'un dossier donne ne change jamais. Le player peut donc
garder ces fichiers indefiniment en cache pour jouer hors ligne, sans risque de
servir une version perimee.

Les images passent par Pillow (redimensionnees, WebP) : c'est la seule
dependance hors bibliotheque standard du compilateur, et elle n'est requise
que pour les aventures qui ont des images.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path

from .errors import A2Error
from .webjson import compile_story

ADVENTURES_DIR = Path(__file__).resolve().parents[2] / "adventures"

CATALOG_FORMAT = 1
IMAGE_MAX_WIDTH = 960          # px ; les scenes sont au format 35:24
COVER_MAX_WIDTH = 640
WEBP_QUALITY = 80

# Palette Apple II hi-res : (bit de palette, colonne paire ?) -> couleur.
_HGR_COLORS = {
    (0, True): (255, 68, 253),     # violet
    (0, False): (20, 245, 60),     # vert
    (1, True): (20, 207, 253),     # bleu
    (1, False): (255, 106, 60),    # orange
}
_HGR_SIZE = 8192


def _pillow():
    try:
        from PIL import Image
    except ImportError:
        raise A2Error("Pillow est requis pour exporter les images du site "
                      "(pip install pillow)") from None
    return Image


def hgr_to_image(data: bytes):
    """Image Apple II hi-res (8192 octets, entrelacee) -> image RVB 280x192.
    Deux pixels allumes voisins sont blancs ; sinon la couleur depend de la
    parite de la colonne et du bit de palette de l'octet."""
    Image = _pillow()
    if len(data) < _HGR_SIZE:
        raise A2Error(f"image HGR trop courte : {len(data)} octets")
    img = Image.new("RGB", (280, 192))
    px = img.load()
    for y in range(192):
        base = ((y & 7) << 10) | (((y >> 3) & 7) << 7) | ((y >> 6) * 40)
        row = data[base:base + 40]
        bits = [(row[x // 7] >> (x % 7)) & 1 for x in range(280)]
        for x in range(280):
            if not bits[x]:
                continue
            if (x > 0 and bits[x - 1]) or (x < 279 and bits[x + 1]):
                px[x, y] = (255, 255, 255)
            else:
                px[x, y] = _HGR_COLORS[(row[x // 7] >> 7, x % 2 == 0)]
    return img


def _save_webp(img, dest: Path, max_width: int, lossless: bool = False) -> int:
    Image = _pillow()
    img = img.convert("RGB")
    if img.width > max_width:
        img = img.resize((max_width, round(img.height * max_width / img.width)),
                         Image.LANCZOS)
    dest.parent.mkdir(parents=True, exist_ok=True)
    img.save(dest, "WEBP", quality=WEBP_QUALITY, lossless=lossless)
    return dest.stat().st_size


def _export_cover(adv_dir: Path, dest: Path) -> bool:
    """Ecrit la couverture ; False si l'aventure n'a aucune image de menu."""
    Image = _pillow()
    png = adv_dir / "img" / "web" / "MENU.png"
    hgr = adv_dir / "img" / "MENU.HGR"
    if png.exists():
        _save_webp(Image.open(png), dest, COVER_MAX_WIDTH)
        return True
    if hgr.exists():
        img = hgr_to_image(hgr.read_bytes())
        img = img.resize((img.width * 3, img.height * 3), Image.NEAREST)
        _save_webp(img, dest, COVER_MAX_WIDTH * 2, lossless=True)
        return True
    return False


def export_images(assets: list[str], src_dir: Path, out_dir: Path,
                  optional: frozenset | set = frozenset()) -> tuple[int, int, list[str]]:
    """Convertit les images nommees vers `IMGnn.webp`, numerotees dans
    l'ordre de premiere apparition (`story.assets`, meme ordre qu'IMAGES.MAP
    cote natif). Source : `<src_dir>/<ID EN MAJUSCULES>.png`, a fournir a la
    main comme `img/named/<ID>.HGR` pour l'Apple II.

    Une image manquante n'est qu'un avertissement, jamais une erreur ; une
    image facultative (utilisee seulement par `@splash`, cf. `optional`) qui
    manque ne donne meme pas d'avertissement : l'aventure reste jouable en
    texte. Renvoie (images ecrites, octets, avertissements)."""
    warnings: list[str] = []
    count = size = 0
    for i, name in enumerate(assets):
        src = src_dir / f"{name.upper()}.png"
        if not src.exists():
            if name not in optional:
                warnings.append(f"image manquante pour @image {name} : {src}")
            continue
        Image = _pillow()
        size += _save_webp(Image.open(src), out_dir / f"IMG{i:02d}.webp",
                           IMAGE_MAX_WIDTH)
        count += 1
    return count, size, warnings


def _folder_digest(root: Path) -> str:
    """Hash du contenu d'un dossier (noms et octets des fichiers)."""
    h = hashlib.sha1()
    for f in sorted(p for p in root.rglob("*") if p.is_file()):
        h.update(f.relative_to(root).as_posix().encode("utf-8") + b"\0")
        h.update(f.read_bytes())
    return h.hexdigest()[:10]


def export_adventure(name: str, adventures_dir: Path,
                     out_dir: Path) -> tuple[dict, list[str]]:
    """Ecrit une aventure dans `<out_dir>/adventures/<nom>/<hash>/` ; renvoie
    son entree de catalogue et ses avertissements."""
    adv_dir = adventures_dir / name
    src = adv_dir / f"{name}.adv"
    if not src.exists():
        raise A2Error(f"aventure introuvable : {src}")
    try:
        story, data = compile_story(src)
    except A2Error as e:
        raise A2Error(f"{name}: {e}") from e

    dest = out_dir / "adventures" / name
    if dest.exists():
        shutil.rmtree(dest)
    build = dest / ".build"
    build.mkdir(parents=True)

    (build / "story.json").write_text(
        json.dumps(data, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8")
    count, _, warnings = export_images(
        story.assets, adv_dir / "img" / "web", build / "img", story.optional_assets)
    has_cover = _export_cover(adv_dir, build / "cover.webp")

    digest = _folder_digest(build)
    build.rename(dest / digest)
    base = f"adventures/{name}/{digest}"
    files = sorted(p.relative_to(out_dir).as_posix()
                   for p in (dest / digest).rglob("*") if p.is_file())

    entry = {
        "id": name,
        "title": story.title or name,
        "author": story.author,
        "version": story.version,
        "lang": story.lang,
        "description": story.description,
        "license": story.license,
        "base": base,
        "story": f"{base}/story.json",
        "cover": f"{base}/cover.webp" if has_cover else None,
        "hash": digest,
        "sections": len(story.sections),
        "images": count,
        "files": files,
        "bytes": sum((out_dir / f).stat().st_size for f in files),
    }
    return entry, [f"{name}: {w}" for w in warnings]


def export_site(names: list[str], out_dir: Path,
                adventures_dir: Path = ADVENTURES_DIR) -> tuple[dict, list[str]]:
    """Exporte les aventures `names` et ecrit `catalog.json`. Les dossiers
    d'aventures qui ne sont plus dans la liste sont supprimes, pour que le
    site corresponde exactement a la demande."""
    entries: list[dict] = []
    warnings: list[str] = []
    for name in names:
        entry, warns = export_adventure(name, adventures_dir, out_dir)
        entries.append(entry)
        warnings += warns
    root = out_dir / "adventures"
    if root.exists():
        for d in root.iterdir():
            if d.name not in names:
                shutil.rmtree(d) if d.is_dir() else d.unlink()
    catalog = {"format": CATALOG_FORMAT, "adventures": entries}
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "catalog.json").write_text(
        json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return catalog, warnings


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="a2c.site", fromfile_prefix_chars="@",
        description="Exporte des aventures et leur catalogue pour le player "
                    "web. `@liste.txt` lit les noms depuis un fichier (un "
                    "par ligne).")
    ap.add_argument("names", nargs="+", metavar="aventure",
                    help="nom d'une aventure (adventures/<nom>/<nom>.adv)")
    ap.add_argument("-o", "--out", required=True, help="dossier de sortie")
    ap.add_argument("--adventures-dir", type=Path, default=ADVENTURES_DIR,
                    help="dossier des aventures (defaut : adventures/)")
    args = ap.parse_args(argv)

    names = [n.strip() for n in args.names if n.strip()]
    try:
        catalog, warnings = export_site(names, Path(args.out), args.adventures_dir)
    except A2Error as e:
        print(f"a2c.site: {e}", file=sys.stderr)
        return 1
    for e in catalog["adventures"]:
        print(f"a2c.site: {e['id']:<20} {e['sections']:>4} sections, "
              f"{e['images']:>2} images, {e['bytes'] // 1024:>5} Ko")
    for w in warnings:
        print(f"a2c.site: attention: {w}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
