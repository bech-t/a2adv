/* ui.h -- affichage texte 40 colonnes (conio). */
#ifndef A2ADV_UI_H
#define A2ADV_UI_H

#include "format.h"

#define SCREEN_W 40              /* largeur par defaut (repli si screensize echoue) */
#define KEY_INVENTORY 0xFE       /* ui_read_choice : demande d'inventaire (I) */
#define KEY_SAVE      0xFD       /* demande de sauvegarde (S) */
#define KEY_LOAD      0xFC       /* demande de chargement (L) */
#define KEY_QUIT      0xFB       /* demande de quitter (Q) */

void ui_init(void);                      /* lit la largeur ecran reelle (40 ou 80) */
void ui_clear(void);
void ui_status(void);                    /* HUD 2 lignes : stats + compte de l'inventaire */
void ui_inventory(void);                 /* ecran plein : liste des objets portes */
void ui_wrap(const char *s, u16 len);    /* imprime avec césure aux espaces */
void ui_paragraph(const char *s, u16 len, u8 style);  /* wrap + styles (centre/inverse) */
void ui_newline(void);
void ui_choice(u8 num, const char *label, u8 len);   /* "1) LIBELLE" (une par ligne) */
void ui_choices_flow(const char *const *labels, const u8 *lens, u8 count); /* enchaines */
void ui_col_reset(void);                 /* remet le suivi de colonne a 0 */
u8   ui_read_choice(u8 count);           /* '1'..count -> 0..count-1 ; 'I' -> KEY_INVENTORY */
void ui_wait_key(const char *msg);
void ui_center(const char *s, u8 y);     /* texte centre horizontalement sur la ligne y */
void ui_progress(u16 done, u16 total);   /* barre [####----] centree a l'ecran */

#endif /* A2ADV_UI_H */
