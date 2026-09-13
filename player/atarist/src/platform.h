/* platform.h -- constantes de portage Atari ST (cf. le pendant Apple II dans
 * player/apple2/src/platform.h, meme nom, memes macros, valeurs propres a
 * chaque machine). Inclus par du code COMMUN (player/common/) qui a besoin
 * d'une valeur qui varie d'une machine a l'autre, sans jamais tester la
 * plateforme lui-meme. */
#ifndef A2ADV_PLATFORM_H
#define A2ADV_PLATFORM_H

/* Extension des fichiers image sur cette machine : de vrais bitmaps Degas
 * .PI1 (cf. img2st/img2st.py), jamais des .HGR Apple II -- concatenation de
 * litteraux a la compilation, cout nul (cf. simage.c : "IMG00." IMG_EXT). */
#define IMG_EXT             "PI1"

/* Lignes du menu semi-graphique (cf. smenu.c) et de l'indice d'intro
 * (cf. simage.c). Decalees de +1 par rapport a l'Apple II (cf.
 * apple2/src/platform.h) : scr_gfx_mixed() laisse ici une ligne d'air entre
 * l'image et le texte. */
#define MENU_ROW_TITLE       21
#define MENU_ROW_CHOICES     23
#define MENU_ROW_QUIT        24
#define UI_INTRO_HINT_ROW    24

/* Musique de menu : pas encore activee sur ST (divergence deliberee, cf.
 * player/atarist/README.md -- son YM2149 ecrit mais pas encore verifie a
 * l'oreille). A remettre a 1 une fois verifiee. */
#define HAS_MENU_MUSIC       0

/* Taille du tampon image (cf. scr.h:SCR_HGR_SIZE) : bitmap .PI1 seul (hors
 * resolution/palette, 320x200/4bpp planaire = 32000 o). */
#define SCR_HGR_SIZE         32000

/* scr_frclock() (compteur VBL brut _frclock, base de temps du sequenceur
 * son -- cf. snd.c) : reel sur Atari ST. */
#define HAS_SCR_FRCLOCK       1

/* Compression ZX02 des images (cf. player/apple2/src/zx02.s + tools/zx02/) :
 * sans objet ici -- la disquette DD 720 Ko a assez de marge (cf. `st` dans
 * le Makefile), et le decodeur est ecrit en assembleur 6502, inutilisable
 * sur 68000. A ecrire pour de vrai le jour ou la compression devient utile
 * sur cette machine (cf. l'ancien atarist/src/zx02_unpack.c, un bouchon
 * vide desormais retire -- ce define desactive completement le chemin). */
#define HAS_ZX02              0

#endif /* A2ADV_PLATFORM_H */
