/* state.h -- état runtime du joueur + interprétation conditions/effets. */
#ifndef A2ADV_STATE_H
#define A2ADV_STATE_H

#include "format.h"

extern u8 stat_val[MAX_STATS];
extern u8 item_bits[(MAX_ITEMS + 7) / 8];
extern u8 flag_bits[(MAX_FLAGS + 7) / 8];

extern u16 g_score;    /* compteur de points   (~ score N) */
extern u16 g_moves;    /* compteur de mouvements (auto, +1 par section) */

/* Initialise l'état depuis les valeurs par défaut du préambule. */
void state_init(void);

u8 item_get(u8 i);
u8 flag_get(u8 i);

/* Remet a 0 les flags LOCAUX ([g_local_base, g_nflags)). Appele a chaque
 * changement de chapitre : c'est ce qui leur donne leur duree de vie. */
void state_clear_locals(void);

/* Sommes des modificateurs de combat de tous les objets PORTES (signées). */
int gear_atk(void);
int gear_dmg(void);
int gear_armor(void);

/* Lit un cond_block au curseur secbuf et renvoie 1 si vrai, 0 sinon.
 * Le curseur est avancé jusqu'après le bloc dans tous les cas. */
u8 state_eval_cond(void);

/* Lit un effect_list au curseur, applique les effets.
 * Renvoie l'index de section d'un GOTO rencontré, ou NO_GOTO. */
u16 state_apply_effects(void);

/* Lit un effect_list au curseur SANS l'appliquer (avance juste le curseur).
 * Utilisé à la reprise d'une sauvegarde (l'état est déjà post-on_enter). */
void state_skip_effects(void);

#endif /* A2ADV_STATE_H */
