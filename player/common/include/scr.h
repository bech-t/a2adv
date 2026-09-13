/* scr.h -- contrat d'ecran commun aux players (pilote texte + graphique),
 * un backend par plateforme (cf. player/apple2/src/scr.c pour le pilote
 * texte maison 40/80 colonnes de l'Apple //e -- ecriture directe en page
 * texte, memoire auxiliaire pour les colonnes paires en 80 col -- et
 * player/atarist/src/scr.c pour la console VT52 native de l'Atari ST, plus
 * simple et suffisante pour un livre-jeu au tour par tour).
 *
 * Le reste du moteur (ui.c, scene.c, simage.c, smenu.c, sinput.c, scombat.c)
 * s'y branche sans savoir a quelle machine il parle. Remplace toute conio
 * (cc65 ou autre) pour l'affichage ET le clavier : aucune dependance. */
#ifndef A2ADV_SCR_H
#define A2ADV_SCR_H

#include "format.h"
#include "platform.h"

extern u8 scr_cols;      /* largeur active en colonnes (40 ou 80) */
extern u16 scr_entropy;  /* entropie accumulee (temps de reaction clavier) */

void scr_init(void);              /* detecte/configure l'ecran, efface */
void scr_clear(void);             /* efface, curseur en haut a gauche */
void scr_putc(char c);            /* '\r' = colonne 0 ; '\n' = ligne suivante col 0 */
void scr_puts(const char *s);
void scr_revers(u8 on);           /* video inverse pour les caracteres suivants */
void scr_gotoxy(u8 x, u8 y);      /* place le curseur */
char scr_getkey(void);            /* attend une touche, renvoie l'ASCII (7 bits) */

/* Appele en boucle pendant que scr_getkey attend le joueur. Sert a faire
 * avancer un travail de fond sans interruptions -- musique de fond (cf.
 * snd.h:snd_music). NULL par defaut : un portage n'a rien a implementer, et
 * un player sans musique de fond n'y touche jamais.
 *
 * Contrat : la fonction doit etre BREVE et non bloquante. Elle est appelee
 * des milliers de fois par seconde. */
extern void (*scr_idle_hook)(void);

char scr_poll(void);              /* touche si pressee (efface le strobe), sinon 0 */
void scr_flush(void);             /* vide le verrou clavier (anti multi-appui) */
void scr_backspace(void);         /* efface le dernier caractere affiche */
u8   scr_readline(char *buf, u8 maxlen);  /* lit une ligne (echo + Entree) */
u8   scr_vbl(void);                /* bit togglant a ~60 Hz (VBL/NTSC-PAL selon la machine) */

#if HAS_SCR_FRCLOCK
u32  scr_frclock(void);            /* compteur VBL brut -- base de temps du sequenceur son */
#endif

void scr_gfx_on(void);            /* image plein ecran */
void scr_gfx_mixed(void);         /* semi-graphique : image en haut, texte en bas */
void scr_gfx_off(void);           /* revient en mode texte (40/80 col) */

/* Callback de progression (octets lus, total). Appele apres chaque bloc. */
typedef void (*scr_progress_cb)(u16 done, u16 total);

/* Charge une image bitmap plein ecran, par blocs. cb=NULL si pas de barre.
 * 0 = ok, <0 = erreur/fichier absent/tronque. */
signed char scr_load_hgr(const char *path, scr_progress_cb cb);

/* La page ou scr_load_hgr depose l'image, et sa taille (IMG_EXT/SCR_HGR_SIZE
 * cf. platform.h). Exposees pour un appelant qui doit ecrire/decompresser
 * DIRECTEMENT dedans (cf. simage.c:img_load_from_disk) plutot que passer par
 * un fichier image brut. Un portage rend l'adresse de sa propre page
 * graphique. */
void *scr_hgr_page(void);

#endif /* A2ADV_SCR_H */
