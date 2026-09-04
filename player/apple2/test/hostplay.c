/* hostplay.c -- banc d'essai HÔTE (gcc) du cœur du player.
 *
 * Réutilise les modules PORTABLES du player (story.c + state.c) pour rejouer un
 * vrai STORY.DAT, en simulant les choix passés en arguments. Vérifie le contrat
 * compilateur<->player (parsing binaire, conditions, effets, navigation) sans
 * matériel Apple II. Ce fichier ne fait pas partie du binaire Apple II.
 *
 *   gcc -I src test/hostplay.c src/story.c src/state.c -o /tmp/hostplay
 *   /tmp/hostplay build/STORY.DAT 2 1 1 1 1
 */

#include <stdio.h>
#include <stdlib.h>
#include "story.h"
#include "state.h"
#include "combat.h"

#define GAME_OVER 0xFFFF

static const char *script[32];   /* arguments bruts : choix (nombre) ou saisie (texte) */
static int  n_script, s_idx;

static void print_state(void)
{
    u8 i;
    printf("    [");
    for (i = 0; i < g_nstats; ++i)
        printf("%s%s=%u", i ? " " : "", stat_name[i], stat_val[i]);
    printf(" | sac:");
    for (i = 0; i < g_nitems; ++i)
        if (item_get(i)) printf(" %s", item_label[i]);
    if (g_score_on) printf(" | score=%u", g_score);
    if (g_moves_on) printf(" | moves=%u", g_moves);
    printf("]\n");
}

static void print_text(const char *s, u16 len)
{
    u16 i;
    printf("  ");
    for (i = 0; i < len; ++i) putchar(s[i]);
    putchar('\n');
}

static u16 play_section(u16 idx)
{
    u8  mode, ending, i, n_texts, n_choices, visible, llen;
    u16 image, len16, pos, target, g, exit_pos;
    u16 v_target[MAX_CHOICES], v_effpos[MAX_CHOICES];
    const char *v_label[MAX_CHOICES];
    u8 v_llen[MAX_CHOICES];
    const char *label;
    int sel;
    u8  has_cb = 0, cb_att = 0, cb_hp = 0, cb_dmg = 0, cb_armor = 0, cb_namelen = 0;
    u16 cb_win = 0, cb_lose = 0, cb_flee = 0;
    u16 cb_winfx = 0, cb_losefx = 0, cb_fleefx = 0;
    const char *cb_name = NULL;
    u8  has_ip = 0, ip_maxlen = 0, ip_nans = 0, ip_plen = 0;
    u16 ip_anspos = 0, ip_correct = 0, ip_wrong = 0, ip_cfx = 0, ip_wfx = 0;
    const char *ip_prompt = NULL;

    if (story_load_section(idx) != 0) {
        printf("!! erreur chargement section %u\n", idx);
        return GAME_OVER;
    }

    mode = b_u8(); ending = b_u8(); image = b_u16();
    (void)mode; (void)image;

    /* bloc combat optionnel */
    {
        u8 has_combat = b_u8();
        if (has_combat) {
            cb_att = b_u8(); cb_hp = b_u8(); cb_dmg = b_u8(); cb_armor = b_u8();
            (void)b_u16();                     /* image ennemi */
            cb_win = b_u16(); cb_lose = b_u16(); cb_flee = b_u16();
            cb_name = b_str(&cb_namelen);
            cb_winfx = b_tell();  state_skip_effects();
            cb_losefx = b_tell(); state_skip_effects();
            cb_fleefx = b_tell(); state_skip_effects();
            has_cb = 1;
        } else {
            has_cb = 0;
        }
    }

    /* bloc saisie optionnel */
    {
        u8 has_input = b_u8();
        if (has_input) {
            ip_prompt = b_str(&ip_plen);
            ip_maxlen = b_u8();
            ip_nans = b_u8();
            ip_anspos = b_tell();
            for (i = 0; i < ip_nans; ++i) { u8 al; (void)b_str(&al); }
            ip_correct = b_u16(); ip_wrong = b_u16();
            ip_cfx = b_tell(); state_skip_effects();
            ip_wfx = b_tell(); state_skip_effects();
            (void)ip_maxlen;              /* non borne cote hote */
            has_ip = 1;
        } else {
            has_ip = 0;
        }
    }

    g = state_apply_effects();          /* on_enter */
    if (g == NO_GOTO && g_moves_on) ++g_moves;
    printf("== section %u ==\n", idx);
    print_state();
    if (g != NO_GOTO) {
        printf("    (on_enter goto -> %u)\n", g);
        return g;
    }

    exit_pos = b_tell();                 /* on_exit : memorise puis saute */
    state_skip_effects();

    n_texts = b_u8();
    for (i = 0; i < n_texts; ++i) {
        u8 vis = state_eval_cond();
        u8 style = b_u8();               /* octet de style du paragraphe */
        (void)style;
        len16 = b_u16();
        pos = b_tell();
        if (vis) print_text((const char *)&secbuf[pos], len16);
        b_seek((u16)(pos + len16));
    }

    /* combat : deroule automatique (attaque chaque round), affiche les tours */
    if (has_cb) {
        u8 r;
        printf("  --- COMBAT : ");
        for (i = 0; i < cb_namelen; ++i) putchar(cb_name[i]);
        printf(" (att=%u pv=%u dmg=%u arm=%u) ---\n", cb_att, cb_hp, cb_dmg, cb_armor);
        combat_begin(cb_att, cb_hp, cb_dmg, cb_armor);
        for (;;) {
            r = combat_attack();
            printf("    round: heros=%u ennemi=%u -> %s %u dgts | PV heros=%u ennemi=%u\n",
                   cb_pscore, cb_escore,
                   cb_last_to == 0 ? "ennemi -" : (cb_last_to == 1 ? "heros -" : "aucun"),
                   cb_last_dmg, combat_hero_hp(), cb_enemy_hp);
            if (r != CB_CONTINUE) break;
        }
        if (r == CB_VICTOIRE) {
            b_seek(cb_winfx); state_apply_effects();     /* effets de victoire */
            printf("  >>> VICTOIRE -> %u\n", cb_win);
            return cb_win;
        }
        b_seek(cb_losefx); state_apply_effects();        /* effets de defaite */
        printf("  >>> DEFAITE -> %u\n", cb_lose);
        (void)cb_flee; (void)cb_fleefx;
        return cb_lose;
    }

    /* saisie : la reponse vient du script (arg brut), normalisee puis comparee */
    if (has_ip) {
        char ans[64];
        u8 i2, blen = 0, match = 0;
        const char *raw = (s_idx < n_script) ? script[s_idx++] : "";
        for (i2 = 0; raw[i2] && i2 < 63; ++i2) {
            char c = raw[i2];
            if (c >= 'a' && c <= 'z') c = (char)(c - 32);
            ans[i2] = c;
        }
        ans[i2] = '\0';
        blen = i2;
        while (blen && ans[blen - 1] == ' ') ans[--blen] = '\0';
        printf("  ? %.*s\n  > %s\n", (int)ip_plen, ip_prompt, ans);
        b_seek(ip_anspos);
        for (i2 = 0; i2 < ip_nans; ++i2) {
            u8 al, k, same = 1;
            const char *a = b_str(&al);
            if (al == blen) {
                for (k = 0; k < al; ++k) if (a[k] != ans[k]) { same = 0; break; }
                if (same) match = 1;
            }
        }
        if (match) {
            b_seek(ip_cfx); state_apply_effects();
            printf("  [saisie acceptee -> %u]\n", ip_correct);
            return ip_correct;
        }
        b_seek(ip_wfx); state_apply_effects();
        printf("  [saisie refusee -> %u]\n", ip_wrong);
        return ip_wrong;
    }

    n_choices = b_u8();
    visible = 0;
    for (i = 0; i < n_choices; ++i) {
        u8 vis = state_eval_cond();
        u16 effpos = b_tell();
        state_skip_effects();            /* saute l'effect_list (cond+op) */
        target = b_u16();
        label = b_str(&llen);
        if (vis && visible < MAX_CHOICES) {
            v_effpos[visible] = effpos;
            v_target[visible] = target;
            v_label[visible]  = label;
            v_llen[visible]   = llen;
            ++visible;
        }
    }

    if (ending != END_NONE || n_choices == 0) {
        printf("  >>> FIN (ending=%u)\n", ending);
        return GAME_OVER;
    }
    if (visible == 0) {
        printf("  >>> impasse\n");
        return GAME_OVER;
    }

    for (i = 0; i < visible; ++i) {
        u8 k;
        printf("  %u) ", i + 1);
        for (k = 0; k < v_llen[i]; ++k) putchar(v_label[i][k]);
        putchar('\n');
    }

    if (s_idx >= n_script) {
        printf("  (script épuisé)\n");
        return GAME_OVER;
    }
    sel = atoi(script[s_idx++]) - 1;
    if (sel < 0 || sel >= visible) {
        printf("  !! choix %d invalide (%u visibles)\n", sel + 1, visible);
        return GAME_OVER;
    }
    printf("  -> choix %d\n", sel + 1);

    b_seek(v_effpos[sel]);
    g = state_apply_effects();          /* effets du choix */
    target = v_target[sel];
    if (g != NO_GOTO) target = g;
    b_seek(exit_pos);                   /* effets de sortie */
    g = state_apply_effects();
    if (g != NO_GOTO) target = g;
    return target;
}

int main(int argc, char **argv)
{
    u16 cur;
    int i;

    if (argc < 2) {
        fprintf(stderr, "usage: %s STORY.DAT [choix...]\n", argv[0]);
        return 2;
    }
    for (i = 2; i < argc && n_script < 32; ++i)
        script[n_script++] = argv[i];

    if (story_open(argv[1]) != 0) {
        fprintf(stderr, "impossible d'ouvrir %s\n", argv[1]);
        return 1;
    }

    rng_seed(0x1234);        /* graine fixe : combats reproductibles en test */
    state_init();
    cur = g_start;
    while (cur != GAME_OVER)
        cur = play_section(cur);

    story_close();
    return 0;
}
