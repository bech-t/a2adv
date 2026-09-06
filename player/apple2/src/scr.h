/* scr.h -- pilote texte maison 40/80 colonnes (Apple //e, CPU 6502).
 *
 * Ecrit directement la page texte. En 80 colonnes, les colonnes PAIRES vont en
 * memoire auxiliaire (via 80STORE + PAGE2), les IMPAIRES en memoire principale.
 * Detecte //e + carte 80 col ; retombe en 40 colonnes sinon.
 *
 * Remplace la conio cc65 pour l'affichage ET le clavier (aucune dependance).
 */
#ifndef A2ADV_SCR_H
#define A2ADV_SCR_H

#include "format.h"

extern u8 scr_cols;   /* largeur active : 40 ou 80 */
extern u16 scr_entropy;  /* entropie accumulee (temps de reaction clavier) */

void scr_init(void);              /* detecte + configure l'ecran, efface */
void scr_clear(void);             /* efface, curseur en haut a gauche */
void scr_putc(char c);            /* '\r' = colonne 0 ; '\n' = ligne suivante col 0 */
void scr_puts(const char *s);
void scr_revers(u8 on);           /* video inverse pour les caracteres suivants */
void scr_gotoxy(u8 x, u8 y);      /* place le curseur */
char scr_getkey(void);            /* attend une touche, renvoie l'ASCII (7 bits) */

/* Appele en boucle pendant que scr_getkey attend le joueur. Sert a faire
 * avancer un travail de fond sans interruptions -- aujourd'hui la musique
 * Mockingboard. NULL par defaut : un portage n'a rien a implementer, et un
 * player sans son n'y touche jamais.
 *
 * Contrat : la fonction doit etre BREVE et non bloquante. Elle est appelee des
 * milliers de fois par seconde. */
extern void (*scr_idle_hook)(void);
char scr_poll(void);              /* touche si pressee (efface le strobe), sinon 0 */
void scr_flush(void);             /* vide le verrou clavier (anti multi-appui) */
void scr_backspace(void);         /* efface le dernier caractere affiche */
u8   scr_readline(char *buf, u8 maxlen);  /* lit une ligne (echo + Entree) */
u8   scr_vbl(void);               /* bit de balayage vertical ($C019 & $80), ~60 Hz */

void scr_gfx_on(void);            /* affiche la page HIRES 1 ($2000) plein ecran */
void scr_gfx_mixed(void);         /* semi-graphique : image en haut, texte 40 col en bas */
void scr_gfx_off(void);           /* revient en mode texte (40/80 col) */

/* Callback de progression (octets lus, total). Appele apres chaque bloc. */
typedef void (*scr_progress_cb)(u16 done, u16 total);
/* Charge une page HIRES (8192 o) a $2000, par blocs. cb=NULL si pas de barre.
 * 0 = ok, <0 = erreur/fichier absent. */
signed char scr_load_hgr(const char *name, scr_progress_cb cb);

/* La page HIRES ou scr_load_hgr depose l'image, et sa taille. Exposees pour
 * un appelant qui doit ecrire/decompresser DIRECTEMENT dedans (cf.
 * simage.c:img_load_from_disk) plutot que passer par un fichier .HGR brut.
 * Un portage rend l'adresse de sa propre page graphique. */
#define SCR_HGR_SIZE 8192
void *scr_hgr_page(void);


#endif /* A2ADV_SCR_H */
