/* platform.h -- constantes de portage Apple II (cf. le pendant Atari ST dans
 * player/atarist/src/platform.h, meme nom, memes macros, valeurs propres a
 * chaque machine). Inclus par du code COMMUN (player/common/) qui a besoin
 * d'une valeur qui varie d'une machine a l'autre, sans jamais tester la
 * plateforme lui-meme. */
#ifndef A2ADV_PLATFORM_H
#define A2ADV_PLATFORM_H

/* Extension des fichiers image sur cette machine (concatenation de
 * litteraux a la compilation, cout nul -- cf. simage.c : "IMG00." IMG_EXT). */
#define IMG_EXT             "HGR"

/* Lignes du menu semi-graphique (cf. smenu.c) et de l'indice d'intro
 * (cf. simage.c). Decalees de +1 sur ST : scr_gfx_mixed() y laisse une
 * ligne d'air entre l'image et le texte, cf. atarist/src/platform.h. */
#define MENU_ROW_TITLE       20
#define MENU_ROW_CHOICES     22
#define MENU_ROW_QUIT        23
#define UI_INTRO_HINT_ROW    23

/* Musique de menu : Mockingboard verifiee au theme du titre. Cf. smenu.c. */
#define HAS_MENU_MUSIC       1

/* Taille du tampon image (cf. scr.h:SCR_HGR_SIZE) : une page HIRES brute. */
#define SCR_HGR_SIZE         8192

/* scr_frclock() (compteur VBL brut) : sans objet sur Apple II. */
#define HAS_SCR_FRCLOCK       0

#endif /* A2ADV_PLATFORM_H */
