/* scr.h -- pilote texte Atari ST (68000, GEMDOS/BIOS via <osbind.h>).
 *
 * Meme contrat que player/apple2/src/scr.h : le reste du moteur (ui.c,
 * scene.c, simage.c, smenu.c, sinput.c, scombat.c) s'y branche sans savoir
 * a quelle machine il parle. L'implementation ST (scr.c) passe par des
 * sequences VT52 (le terminal texte natif du ST, cf. console EmuTOS) plutot
 * que par une ecriture directe en page ecran -- plus simple, largement
 * suffisant pour un livre-jeu au tour par tour.
 *
 * Images : basse resolution ST (320x200, 16 couleurs), activee a la volee
 * par scr_load_hgr() le temps d'une image puis rendue par scr_gfx_off()
 * (cf. scr_init pour la resolution texte par defaut). scr_gfx_xxx/
 * scr_load_hgr chargent un bitmap planaire + sa palette (format Degas
 * .PI1, cf. player/atarist/img2st/img2st.py).
 */
#ifndef A2ADV_SCR_H
#define A2ADV_SCR_H

#include "format.h"

extern u8 scr_cols;      /* 40 en resolution basse (320x200), 80 sinon -- cf. scr_init */
extern u16 scr_entropy;  /* entropie accumulee (temps de reaction clavier) */

void scr_init(void);              /* configure la console VT52, efface */
void scr_clear(void);             /* efface, curseur en haut a gauche */
void scr_putc(char c);            /* '\r' = colonne 0 ; '\n' = ligne suivante col 0 */
void scr_puts(const char *s);
void scr_revers(u8 on);           /* video inverse pour les caracteres suivants */
void scr_gotoxy(u8 x, u8 y);      /* place le curseur */
char scr_getkey(void);            /* attend une touche, renvoie l'ASCII (accents repliés, cf. scr.c) */

/* Cf. apple2/src/scr.h : hook d'avancement pendant scr_getkey (musique de
 * fond). NULL par defaut -- rien a implementer tant que snd.c ST ne gere
 * pas de musique. */
extern void (*scr_idle_hook)(void);
char scr_poll(void);              /* touche si pressee, sinon 0 */
void scr_flush(void);             /* vide le clavier en attente */
void scr_backspace(void);         /* efface le dernier caractere affiche */
u8   scr_readline(char *buf, u8 maxlen);  /* lit une ligne (echo + Entree) */
u8   scr_vbl(void);                /* bit togglant a ~60 Hz, cf. scr.c : NTSC vs PAL */
u32  scr_frclock(void);            /* compteur VBL brut (_frclock) -- base de
                                    * temps du sequenceur son, cf. snd.c */

void scr_gfx_on(void);            /* image plein ecran */
void scr_gfx_mixed(void);         /* image + fenetre texte (160 lignes + 4 rangees texte) */
void scr_gfx_off(void);           /* retour texte : restaure la palette du boot */

typedef void (*scr_progress_cb)(u16 done, u16 total);
/* Charge un .PI1 (resolution + palette 16 coul. + bitmap) en page ecran.
 * 0 = ok, -1 = fichier absent, -2 = tronque ou pas en basse resolution. */
signed char scr_load_hgr(const char *name, scr_progress_cb cb);

#define SCR_HGR_SIZE 32000   /* bitmap seul (hors resolution/palette) */
void *scr_hgr_page(void);

#endif /* A2ADV_SCR_H */
