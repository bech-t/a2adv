/* combat.c -- coeur portable du combat (cf. combat.h).
 *
 * Resolution v1 (livre-jeu) : chaque round, heros et ennemi lancent 2d6 + leur
 * attaque ; le plus haut score touche et inflige (degats - armure adverse),
 * minimum 1 (garantit la fin du combat). Les PV du heros sont la stat de PV
 * declaree (@combat_hp) ; l'arme/armure portee modifie att/degats/armure. */

#include "combat.h"
#include "story.h"
#include "state.h"

/* --- PRNG xorshift 16 bits --------------------------------------------- */
static u16 rng_state = 0xACE1;

void rng_seed(u16 s) { rng_state = s ? s : 0xACE1; }

static u16 rng_next(void)
{
    u16 x = rng_state;
    x ^= (u16)(x << 7);
    x ^= (u16)(x >> 9);
    x ^= (u16)(x << 8);
    rng_state = x;
    return x;
}

u8 rng_d6(void) { return (u8)(rng_next() % 6) + 1; }

/* --- Etat --------------------------------------------------------------- */
u16 cb_enemy_hp;
u8  cb_e_att, cb_e_dmg, cb_e_armor;
u8  cb_pscore, cb_escore, cb_last_dmg, cb_last_to;

void combat_begin(u8 att, u8 hp, u8 dmg, u8 armor)
{
    cb_e_att = att;
    cb_enemy_hp = hp;
    cb_e_dmg = dmg;
    cb_e_armor = armor;
    cb_pscore = cb_escore = cb_last_dmg = 0;
    cb_last_to = 2;
}

u8 combat_hero_hp(void)
{
    return (g_combat_hp == 0xFF) ? 0 : stat_val[g_combat_hp];
}

static void hero_take(u8 d)
{
    u8 h;
    if (g_combat_hp == 0xFF)
        return;
    h = stat_val[g_combat_hp];
    stat_val[g_combat_hp] = (d >= h) ? 0 : (u8)(h - d);
}

/* degats effectifs = max(1, base - armure) */
static u8 damage(int base, int armor)
{
    int d = base - armor;
    return (d < 1) ? 1 : (u8)d;
}

u8 combat_attack(void)
{
    int patt = (g_combat_att == 0xFF ? 0 : stat_val[g_combat_att]) + gear_atk();
    int pdmg = (int)g_combat_basedmg + gear_dmg();
    int parmor = gear_armor();
    int ps, es;

    if (patt < 0) patt = 0;
    ps = (int)rng_d6() + rng_d6() + patt;
    es = (int)rng_d6() + rng_d6() + cb_e_att;
    cb_pscore = (u8)(ps > 255 ? 255 : ps);
    cb_escore = (u8)(es > 255 ? 255 : es);

    if (ps > es) {                       /* le heros touche */
        u8 d = damage(pdmg, cb_e_armor);
        cb_enemy_hp = (d >= cb_enemy_hp) ? 0 : (u16)(cb_enemy_hp - d);
        cb_last_dmg = d;
        cb_last_to = 0;
    } else if (es > ps) {                /* l'ennemi touche */
        u8 d = damage(cb_e_dmg, parmor);
        hero_take(d);
        cb_last_dmg = d;
        cb_last_to = 1;
    } else {                            /* egalite : personne */
        cb_last_dmg = 0;
        cb_last_to = 2;
    }

    if (cb_enemy_hp == 0)
        return CB_VICTOIRE;
    if (combat_hero_hp() == 0)
        return CB_DEFAITE;
    return CB_CONTINUE;
}

u8 combat_flee(void)
{
    u8 d = damage(cb_e_dmg, gear_armor());   /* coup gratuit de l'ennemi */
    hero_take(d);
    cb_last_dmg = d;
    cb_last_to = 1;
    return (combat_hero_hp() == 0) ? CB_DEFAITE : CB_FUITE;
}
