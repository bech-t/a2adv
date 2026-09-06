#!/usr/bin/env python3
"""Compresse chaque image HIRES d'un dossier avec zx02, en PLUS du .HGR --
jamais a la place. Certains .HGR (BOOT00.HGR, MENU.HGR) sont des sources
versionnees dans le depot ; ce script ne supprime donc jamais un .HGR
lui-meme, seulement un .ZX2 devenu inutile (compression infructueuse, ou
source disparue depuis). C'est au Makefile de choisir, au moment de deposer
les fichiers sur la disquette, lequel des deux envoyer.

Usage : pack_images.py <chemin de l'executable zx02> <dossier .HGR/.ZX2>
"""
import subprocess
import sys
from pathlib import Path


def main() -> int:
    zx02_bin, img_dir = sys.argv[1], Path(sys.argv[2])
    hgr_names = set()

    for hgr in sorted(img_dir.glob("*.HGR")):
        hgr_names.add(hgr.stem)
        zx2 = hgr.with_suffix(".ZX2")
        orig_size = hgr.stat().st_size
        result = subprocess.run([zx02_bin, "-f", str(hgr), str(zx2)],
                                 capture_output=True)
        if result.returncode == 0 and zx2.exists() and zx2.stat().st_size < orig_size:
            comp_size = zx2.stat().st_size
            print(f"  {hgr.name} + {zx2.name}  ({orig_size} -> {comp_size} o, "
                  f"{100 * comp_size // orig_size} %, source conservee)")
        elif zx2.exists():
            zx2.unlink()   # compression infructueuse : ne pas la laisser trainer

    # .ZX2 orphelins (image renommee/supprimee depuis un build precedent) :
    # sans .HGR correspondant, plus personne ne les regenere ni ne les efface.
    for zx2 in sorted(img_dir.glob("*.ZX2")):
        if zx2.stem not in hgr_names:
            zx2.unlink()
    return 0


if __name__ == "__main__":
    sys.exit(main())
