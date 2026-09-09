/* state.c -- état runtime, évaluation des conditions, application des effets. */

#include "state.h"
#include "story.h"
#include "snd.h"

u8 stat_val[MAX_STATS];
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

static u8 eval_atom(u8 op, u8 a0, u8 a1, u8 a2)
{
    u8 v;
    switch (op) {
    case OP_FLAG_SET: return flag_get(a0);
    case OP_FLAG_CLR: return (u8)!flag_get(a0);
    case OP_HAS_ITEM: return item_get(a0);
    case OP_NO_ITEM:  return (u8)!item_get(a0);
    case OP_STAT_CMP:
        v = stat_val[a0];
        switch (a1) {
        case CMP_EQ: return (u8)(v == a2);
        case CMP_NE: return (u8)(v != a2);
        case CMP_LT: return (u8)(v <  a2);
        case CMP_LE: return (u8)(v <= a2);
        case CMP_GT: return (u8)(v >  a2);
        case CMP_GE: return (u8)(v >= a2);
        }
        return 0;
    }
    return 0;
}

u8 state_eval_cond(void)
{
    u8 n = b_u8();
    u8 conn = b_u8();     /* 0=AND, 1=OR */
    u8 result, i, av, op, a0, a1, a2;

    if (n == 0)
        return 1;

    result = (conn == 0) ? 1 : 0;   /* AND part de vrai, OR de faux */
    for (i = 0; i < n; ++i) {
        op = b_u8(); a0 = b_u8(); a1 = b_u8(); a2 = b_u8();
        av = eval_atom(op, a0, a1, a2);
        if (conn == 0) result = (u8)(result & av);
        else           result = (u8)(result | av);
    }
    return result;
}

/* --- Effets ------------------------------------------------------------ */

static void stat_clamp_set(u8 idx, int nv)
{
    if (nv < (int)stat_min[idx]) nv = stat_min[idx];
    if (nv > (int)stat_max[idx]) nv = stat_max[idx];
    stat_val[idx] = (u8)nv;
}

void state_skip_effects(void)
{
    u8 n = b_u8();
    u8 i, na;
    for (i = 0; i < n; ++i) {
        na = b_u8();                            /* garde : n_atoms */
        (void)b_u8();                           /* connective */
        b_seek((u16)(b_tell() + (u16)na * 4));  /* atomes */
        b_seek((u16)(b_tell() + 4));            /* op + 3 operandes */
    }
}

u16 state_apply_effects(void)
{
    u8 n = b_u8();
    u8 i, op, a0, a1, ok;
    u16 target = NO_GOTO;

    for (i = 0; i < n; ++i) {
        ok = state_eval_cond();                 /* garde de l'effet */
        op = b_u8(); a0 = b_u8(); a1 = b_u8(); (void)b_u8();  /* a2 inutilisé */
        if (!ok)
            continue;                           /* garde fausse -> effet ignore */
        switch (op) {
        case OP_SET_FLAG:  flag_set(a0, 1); break;
        case OP_CLR_FLAG:  flag_set(a0, 0); break;
        case OP_TOG_FLAG:  flag_set(a0, (u8)!flag_get(a0)); break;
        case OP_GIVE_ITEM: item_set(a0, 1); break;
        case OP_TAKE_ITEM: item_set(a0, 0); break;
        case OP_STAT_ADD:  stat_clamp_set(a0, (int)stat_val[a0] + a1); break;
        case OP_STAT_SUB:  stat_clamp_set(a0, (int)stat_val[a0] - a1); break;
        case OP_STAT_SET:  stat_clamp_set(a0, (int)a1); break;
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
            stat_max[a0] = a1;
            if (stat_val[a0] > a1) stat_val[a0] = a1;
            break;
        case OP_GOTO:
            target = a0 | ((u16)a1 << 8);
            return target;   /* saut immédiat (spec §5.2) */
        }
    }
    return target;
}
