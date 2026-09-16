/* platform.h -- constantes de portage PC DOS (cf. les pendants Apple II et
 * Atari ST dans player/apple2/src/platform.h et player/atarist/src/platform.h,
 * meme nom, memes macros, valeurs propres a chaque machine). Inclus par du
 * code COMMUN (player/common/) qui a besoin d'une valeur qui varie d'une
 * machine a l'autre, sans jamais tester la plateforme lui-meme. */
#ifndef A2ADV_PLATFORM_H
#define A2ADV_PLATFORM_H

/* Extension des fichiers image sur cette machine : de vrais .PCX (ZSoft,
 * RLE, palette 256 couleurs -- format DOS standard de l'epoque, cf.
 * img2dos/img2dos.py), jamais un format invente pour l'occasion -- meme
 * principe que le Degas .PI1 reel cote Atari ST (cf. atarist/src/
 * platform.h). Un seul binaire cible VGA (mode 13h) ; sur une machine EGA
 * seule (pas de mode 13h), scr_load_hgr echoue proprement (cf. scr.c) et le
 * jeu reste jouable en texte seul -- meme repli que le moniteur
 * monochrome sur Atari ST (cf. atarist/src/scr.c:st_mono). Un vrai mode
 * graphique EGA planaire reste a ecrire (cf. player/dos/README.md). */
#define IMG_EXT             "PCX"

/* Lignes du menu semi-graphique (cf. smenu.c) et de l'indice d'intro
 * (cf. simage.c). Memes valeurs qu'Atari ST : meme geometrie (320x200,
 * police 8x8 -> 25 rangees, scr_gfx_mixed laisse une ligne d'air entre
 * l'image et le texte, cf. scr.c:scr_gfx_mixed). */
#define MENU_ROW_TITLE       20
#define MENU_ROW_CHOICES     22
#define MENU_ROW_QUIT        23
#define UI_INTRO_HINT_ROW    23

/* Taille du tampon image (cf. scr.h:SCR_HGR_SIZE) : bitmap seul (hors
 * palette, cf. scr_load_hgr), mode 13h lineaire 320x200x1 octet/pixel. */
#define SCR_HGR_SIZE         64000UL

/* scr_frclock() (compteur d'horloge brut, base de temps pour un futur
 * sequenceur son -- cf. snd.h) : reel sur PC, via le compteur BIOS
 * 0040:006C (cf. scr.c). */
#define HAS_SCR_FRCLOCK       1

#endif /* A2ADV_PLATFORM_H */
