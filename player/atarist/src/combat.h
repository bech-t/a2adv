/* combat.h -- coeur PORTABLE du combat (RNG + resolution d'un round).
 * Aucune dependance a l'affichage : le pilote (main.c / hostplay.c) gere
 * l'ecran et les entrees, et appelle ces fonctions. */
#ifndef A2ADV_COMBAT_H
#define A2ADV_COMBAT_H

#include "format.h"

/* --- PRNG (xorshift 16 bits) ------------------------------------------ */
void rng_seed(u16 s);
u8   rng_d6(void);            /* 1..6 */

/* --- Etat du combat en cours ------------------------------------------ */
extern u16 cb_enemy_hp;       /* PV restants de l'ennemi */
extern u8  cb_e_att, cb_e_dmg, cb_e_armor;
/* Infos du dernier round (pour l'affichage) */
extern u8  cb_pscore, cb_escore;   /* scores 2d6 + attaque */
extern u8  cb_last_dmg;            /* degats infliges au dernier round */
extern u8  cb_last_to;             /* 0=ennemi touche, 1=heros touche, 2=aucun */

/* Prepare un combat contre un ennemi (att, PV, degats, armure). */
void combat_begin(u8 att, u8 hp, u8 dmg, u8 armor);

/* Joue UN round (le heros frappe, l'ennemi riposte selon les scores).
 * Renvoie CB_CONTINUE, CB_WIN ou CB_LOSE. */
u8 combat_attack(void);

/* Fuite : l'ennemi place un coup gratuit (penalite).
 * Renvoie CB_FLEE, ou CB_LOSE si le coup est fatal. */
u8 combat_flee(void);

/* PV courants du heros (stat de PV de combat), 0 si non configuree. */
u8 combat_hero_hp(void);

#endif /* A2ADV_COMBAT_H */
