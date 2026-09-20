/* state.c -- état runtime, évaluation des conditions, application des effets. */

#include "state.h"
#include "story.h"
#include "snd.h"

u16 stat_val[MAX_STATS];
u8 item_bits[(MAX_ITEMS + 7) / 8];
u8 flag_bits[(MAX_FLAGS + 7) / 8];
u16 g_score;
u16 g_moves;

void state_init(void)
{
    u8 i;
    for (i = 0; i < g_nstats; ++i) {
        stat_val[i] = stat_init[i];
        stat_max[i] = stat_maxdef[i];   /* max courant = defaut (le perso peut le changer) */
    }
    for (i = 0; i < (g_nitems + 7) / 8; ++i)
        item_bits[i] = item_default[i];
    for (i = 0; i < (g_nflags + 7) / 8; ++i)
        flag_bits[i] = flag_default[i];
    g_score = 0;
    g_moves = 0;
}

u8 item_get(u8 i) { return (item_bits[i >> 3] >> (i & 7)) & 1; }
u8 flag_get(u8 i) { return (flag_bits[i >> 3] >> (i & 7)) & 1; }

void state_clear_locals(void)
{
    u8 i;
    for (i = g_local_base; i < g_nflags; ++i)
        flag_bits[i >> 3] &= (u8)~(1u << (i & 7));
}

/* Somme d'un modificateur de combat sur tous les objets portes. */
int gear_atk(void)
{
    u8 i; int s = 0;
    for (i = 0; i < g_nitems; ++i) if (item_get(i)) s += item_atk[i];
    return s;
}
int gear_dmg(void)
{
    u8 i; int s = 0;
    for (i = 0; i < g_nitems; ++i) if (item_get(i)) s += item_dmg[i];
    return s;
}
int gear_armor(void)
{
    u8 i; int s = 0;
    for (i = 0; i < g_nitems; ++i) if (item_get(i)) s += item_armor[i];
    return s;
}

static void item_set(u8 i, u8 v)
{
    u8 mask = 1 << (i & 7);
    if (v) item_bits[i >> 3] |= mask;
    else   item_bits[i >> 3] &= (u8)~mask;
}

static void flag_set(u8 i, u8 v)
{
    u8 mask = 1 << (i & 7);
    if (v) flag_bits[i >> 3] |= mask;
    else   flag_bits[i >> 3] &= (u8)~mask;
}

/* --- Conditions -------------------------------------------------------- */

u8 state_eval_cond(void)
{
    u8 nc = b_u8();       /* clauses (OU) ; 0 = pas de condition */
    u8 n, op, a0, a1, av, lt, eq, ok, result = 0;
    u16 a2, v;

    if (nc == 0)
        return 1;

    /* Une condition est un OU de ET (forme disjonctive, cf. a2c/cond.py) :
     * chaque clause est une suite d'atomes tous vrais. On lit TOUT, sans
     * court-circuit : le curseur doit finir apres la condition. */
    while (nc--) {
        n = b_u8();
        ok = 1;
        while (n--) {
            /* atome (op,a0,a1 = 3 o) puis a2 : 16 bits pour 'stat' (valeur
             * jusqu'a 65535), 8 bits sinon (largeur variable, cf. encoder.py).
             * Inline (un seul point d'appel) : evite le cout d'un JSR/RTS. */
            op = b_u8(); a0 = b_u8(); a1 = b_u8();
            a2 = (op == OP_STAT_CMP) ? b_u16() : b_u8();
            switch (op) {
            case OP_FLAG_SET: av = flag_get(a0); break;
            case OP_FLAG_CLR: av = (u8)!flag_get(a0); break;
            case OP_HAS_ITEM: av = item_get(a0); break;
            case OP_NO_ITEM:  av = (u8)!item_get(a0); break;
            case OP_STAT_CMP:
                /* 2 comparaisons 16 bits ('<' et '==') au lieu de 6 : sur cc65,
                 * chaque comparaison 16 bits distincte coute nettement plus
                 * qu'en 8 bits -- LE/GT/GE/NE se deduisent de lt/eq. */
                v = stat_val[a0];
                lt = (u8)(v < a2);
                eq = (u8)(v == a2);
                switch (a1) {
                case CMP_EQ: av = eq; break;
                case CMP_NE: av = (u8)!eq; break;
                case CMP_LT: av = lt; break;
                case CMP_LE: av = (u8)(lt || eq); break;
                case CMP_GT: av = (u8)(!lt && !eq); break;
                case CMP_GE: av = (u8)!lt; break;
                default: av = 0; break;
                }
                break;
            default: av = 0; break;
            }
            ok = (u8)(ok & av);
        }
        result = (u8)(result | ok);
    }
    return result;
}

/* --- Effets ------------------------------------------------------------ */

static void stat_clamp_set(u8 idx, u16 nv)
{
    if (nv < stat_min[idx]) nv = stat_min[idx];
    if (nv > stat_max[idx]) nv = stat_max[idx];
    stat_val[idx] = nv;
}

void state_skip_effects(void)
{
    u8 n = b_u8();
    u8 i;
    for (i = 0; i < n; ++i) {
        /* garde de l'effet : largeur d'atome variable selon op (cf.
         * state_eval_cond) -- reevaluer et jeter le resultat evite de
         * dupliquer cette logique ici juste pour avancer le curseur. */
        (void)state_eval_cond();
        b_seek((u16)(b_tell() + 4));            /* op + 3 operandes (effet) */
    }
}

u16 state_apply_effects(void)
{
    u8 n = b_u8();
    u8 i, op, a0, a1, a2, ok;
    u16 target = NO_GOTO;
    u16 val, nv, cur;

    for (i = 0; i < n; ++i) {
        ok = state_eval_cond();                 /* garde de l'effet */
        op = b_u8(); a0 = b_u8(); a1 = b_u8(); a2 = b_u8();
        if (!ok)
            continue;                           /* garde fausse -> effet ignore */
        /* valeur 16 bits des effets 'stat' : a1=octet faible, a2=octet fort
         * (cf. encoder.py:_encode_effect). Inutilise ailleurs. */
        val = (u16)(a1 | ((u16)a2 << 8));
        switch (op) {
        case OP_SET_FLAG:  flag_set(a0, 1); break;
        case OP_CLR_FLAG:  flag_set(a0, 0); break;
        case OP_TOG_FLAG:  flag_set(a0, (u8)!flag_get(a0)); break;
        case OP_GIVE_ITEM: item_set(a0, 1); break;
        case OP_TAKE_ITEM: item_set(a0, 0); break;
        case OP_STAT_ADD:
            cur = stat_val[a0];
            nv = (u16)(cur + val);
            if (nv < cur) nv = 0xFFFF;   /* debordement 16 bits -> sature haut */
            stat_clamp_set(a0, nv);
            break;
        case OP_STAT_SUB:
            /* val > courant : sature bas direct (pas de soustraction, pas
             * de debordement possible) */
            cur = stat_val[a0];
            nv = (val > cur) ? 0 : (u16)(cur - val);
            stat_clamp_set(a0, nv);
            break;
        case OP_STAT_SET:  stat_clamp_set(a0, val); break;
        case OP_SOUND:
            snd_play(a0);
            break;
        case OP_SCORE_ADD:
            if (g_score_on) g_score += a0;
            break;
        case OP_STAT_MAX:                   /* ~ restore STAT : au max courant */
            stat_val[a0] = stat_max[a0];
            break;
        case OP_STAT_SETMAX:                /* ~ setmax STAT N : fixe le max */
            stat_max[a0] = val;
            if (stat_val[a0] > val) stat_val[a0] = val;
            break;
        case OP_GOTO:
            target = a0 | ((u16)a1 << 8);
            return target;   /* saut immédiat */
        }
    }
    return target;
}
