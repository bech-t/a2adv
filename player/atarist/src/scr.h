/* scr.h -- pilote texte Atari ST (68000, GEMDOS/BIOS via <osbind.h>).
 *
 * Meme contrat que player/apple2/src/scr.h (cf. spec.md §9ter, spec-atarist.md) :
 * ui.c/scene.c/simage.c/smenu.c/sinput.c/scombat.c s'y branchent sans savoir
 * a quelle machine ils parlent. L'implementation ST (scr.c) passe par des
 * sequences VT52 (le terminal texte natif du ST, cf. console EmuTOS) plutot
 * que par une ecriture directe en page ecran -- plus simple, largement
 * suffisant pour un livre-jeu au tour par tour.
 *
 * Slice "texte seul" (cf. spec-atarist.md §10) : scr_gfx_xxx et scr_load_hgr
 * restent des bouchons tant que le mode graphique ST n'est pas ecrit.
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
char scr_getkey(void);            /* attend une touche, renvoie l'ASCII (7 bits) */

/* Cf. apple2/src/scr.h : hook d'avancement pendant scr_getkey (musique de
 * fond). NULL par defaut -- rien a implementer tant que snd.c ST ne gere
 * pas de musique. */
extern void (*scr_idle_hook)(void);
char scr_poll(void);              /* touche si pressee, sinon 0 */
void scr_flush(void);             /* vide le clavier en attente */
void scr_backspace(void);         /* efface le dernier caractere affiche */
u8   scr_readline(char *buf, u8 maxlen);  /* lit une ligne (echo + Entree) */
u8   scr_vbl(void);                /* bit togglant a ~60 Hz, cf. scr.c : NTSC vs PAL */

void scr_gfx_on(void);            /* [bouchon] image plein ecran */
void scr_gfx_mixed(void);         /* [bouchon] image + fenetre texte */
void scr_gfx_off(void);           /* retour texte (fonctionne des maintenant) */

typedef void (*scr_progress_cb)(u16 done, u16 total);
/* [bouchon] toujours en erreur (-1) tant que le format d'image ST n'existe pas. */
signed char scr_load_hgr(const char *name, scr_progress_cb cb);

#define SCR_HGR_SIZE 8192
void *scr_hgr_page(void);

#endif /* A2ADV_SCR_H */
