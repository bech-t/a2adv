/* ui.c -- couche d'affichage, au-dessus du pilote texte maison (scr.c).
 * S'adapte a 40 ou 80 colonnes via scr_cols. */

#include "scr.h"
#include "ui.h"
#include "story.h"
#include "state.h"

static u8 col;   /* colonne courante suivie pour la césure */

void ui_init(void)
{
    scr_init();   /* detecte 40/80 col, configure l'ecran */
}

void ui_clear(void)
{
    scr_clear();
    col = 0;
}

void ui_newline(void)
{
    scr_putc('\n');   /* le pilote gere CR+LF */
    col = 0;
}

/* Imprime une chaîne (len octets) avec retour à la ligne aux espaces.
 * Les octets TXT_INV_TOGGLE (issus des marqueurs *...*) basculent l'inverse.
 * Le separateur entre deux mots est imprime AVANT d'appliquer les bascules du
 * mot suivant : ainsi l'espace precedant un mot en surbrillance reste en video
 * normale (tandis qu'un espace INTERNE a une surbrillance reste, lui, inverse). */
void ui_wrap(const char *s, u16 len)
{
    u16 i = 0, ws, we, k;
    u8 wlen, inv_on = 0;

    while (i < len) {
        while (i < len && s[i] == ' ')          /* saute les espaces */
            ++i;
        if (i >= len)
            break;
        /* mot [ws, we) : jusqu'au prochain espace (bascules incluses) */
        ws = i;
        while (i < len && s[i] != ' ')
            ++i;
        we = i;
        wlen = 0;                                /* largeur visible (hors bascules) */
        for (k = ws; k < we; ++k)
            if (s[k] != TXT_INV_TOGGLE) ++wlen;

        if (col != 0) {                          /* separateur, en video COURANTE */
            if (col + 1 + wlen > scr_cols) {
                ui_newline();
            } else {
                scr_putc(' ');
                ++col;
            }
        }
        for (k = ws; k < we; ++k) {              /* le mot (bascules appliquees ici) */
            if (s[k] == TXT_INV_TOGGLE) {
                inv_on = (u8)!inv_on;
                scr_revers(inv_on);
            } else {
                scr_putc(s[k]);
                ++col;
                if (col >= scr_cols) col = 0;    /* l'écran a enroulé */
            }
        }
    }
    if (inv_on)
        scr_revers(0);                           /* securite : on ne laisse pas l'inverse */
    ui_newline();
}

/* Imprime un paragraphe avec style : centre (sans cesure) et/ou inverse.
 * Sans style : justifie aux espaces comme ui_wrap. */
void ui_paragraph(const char *s, u16 len, u8 style)
{
    if (style & STYLE_CENTER) {
        u16 i;
        u8 vis = 0, pad, inv_on = (style & STYLE_INVERSE) ? 1 : 0;
        for (i = 0; i < len; ++i)        /* largeur = caracteres visibles */
            if (s[i] != TXT_INV_TOGGLE) ++vis;
        pad = (vis < scr_cols) ? (u8)((scr_cols - vis) / 2) : 0;
        while (pad--)                    /* remplissage centrage (video normale) */
            scr_putc(' ');
        if (inv_on)
            scr_revers(1);
        for (i = 0; i < len; ++i) {
            if (s[i] == TXT_INV_TOGGLE) {
                inv_on = (u8)!inv_on;
                scr_revers(inv_on);
            } else {
                scr_putc(s[i]);
            }
        }
        scr_revers(0);
        ui_newline();
    } else {
        if (style & STYLE_INVERSE)
            scr_revers(1);
        ui_wrap(s, len);                 /* justifie ; ui_wrap termine la ligne */
        scr_revers(0);
    }
}

/* Imprime un entier 0..255 sans zeros de tete. */
static void put_num(u8 v)
{
    if (v >= 100) scr_putc((char)('0' + v / 100));
    if (v >= 10)  scr_putc((char)('0' + (v / 10) % 10));
    scr_putc((char)('0' + v % 10));
}

/* Imprime un entier 16 bits sans zeros de tete. */
static void put_num16(u16 v)
{
    char buf[6];
    u8 n = 0;
    if (v == 0) { scr_putc('0'); return; }
    while (v) { buf[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) scr_putc(buf[--n]);
}

static u8 slen(const char *s)
{
    u8 n = 0;
    while (s[n]) ++n;
    return n;
}

/* Nombre de chiffres d'un entier 16 bits (>=1). */
static u8 nw16(u16 v)
{
    u8 n = 1;
    while (v >= 10) { v /= 10; ++n; }
    return n;
}

/* Bandeau d'etat : 1re ligne INVERSE sur toute la largeur.
 * Stats a gauche ; score/moves (si actifs) alignes a droite. */
void ui_status(void)
{
    u8 i, count = 0, c = 0, rightw = 0;

    if (g_score_on)
        rightw = (u8)(slen(ui_str[UI_SCORE]) + 1 + nw16(g_score));
    if (g_moves_on) {
        if (rightw) rightw = (u8)(rightw + 2);
        rightw = (u8)(rightw + slen(ui_str[UI_MOVES]) + 1 + nw16(g_moves));
    }

    scr_revers(1);
    for (i = 0; i < g_nstats; ++i) {          /* bloc gauche : stats */
        if ((g_stat_hidden >> i) & 1)         /* @stat ... hidden : non affichee */
            continue;
        scr_puts(stat_name[i]); c = (u8)(c + slen(stat_name[i]));
        scr_putc(' ');          ++c;
        put_num16(stat_val[i]); c = (u8)(c + nw16(stat_val[i]));
        scr_putc(' ');          ++c;
    }
    if (rightw && (u8)(rightw + 1) <= scr_cols &&
        c <= (u8)(scr_cols - rightw)) {       /* bloc droit aligne a droite */
        while (c < (u8)(scr_cols - rightw)) { scr_putc(' '); ++c; }
        if (g_score_on) {
            scr_puts(ui_str[UI_SCORE]); scr_putc(' '); put_num16(g_score);
        }
        if (g_moves_on) {
            if (g_score_on) { scr_putc(' '); scr_putc(' '); }
            scr_puts(ui_str[UI_MOVES]); scr_putc(' '); put_num16(g_moves);
        }
    } else {                                  /* pas de compteurs (ou trop etroit) */
        while (c < scr_cols) { scr_putc(' '); ++c; }
    }
    scr_revers(0);
    /* la ligne fait exactement scr_cols : scr_putc a deja enroule -> pas de newline */
    col = 0;

    for (i = 0; i < g_nitems; ++i)
        if (item_get(i)) ++count;
    scr_puts(ui_str[UI_INV_HUD]);
    put_num(count);
    scr_puts("  ");
    scr_puts(ui_str[UI_HINTS]);
    ui_newline();
    ui_newline();
    col = 0;
}

/* Affiche un modificateur de combat signé s'il est non nul : " ATT+1". */
static void put_mod(const char *lbl, signed char v)
{
    if (v == 0)
        return;
    scr_putc(' ');
    scr_puts(lbl);
    scr_putc(v < 0 ? '-' : '+');
    put_num((u8)(v < 0 ? -v : v));
}

/* Ecran plein : liste des objets portes, un par ligne, avec leurs attributs
 * de combat (ATT/DEG/ARM) s'ils en ont. */
void ui_inventory(void)
{
    u8 i, any = 0;

    ui_clear();
    scr_revers(1);
    scr_putc(' ');
    scr_puts(ui_str[UI_INVENTORY]);
    scr_putc(' ');
    scr_revers(0);
    ui_newline();
    ui_newline();
    for (i = 0; i < g_nitems; ++i) {
        if (item_get(i)) {
            scr_puts("- ");
            scr_puts(item_label[i]);
            /* si attaque ou autre, on presente */
            if ((item_atk[i] + item_dmg[i] + item_armor[i]) > 0) { 
                scr_puts("     ");
                /* l'attaque modifie la stat d'attaque -> on affiche son nom */
                put_mod(g_combat_att < g_nstats ? stat_name[g_combat_att]
                                            : ui_str[UI_CBT_ATK], item_atk[i]);
                put_mod(ui_str[UI_CBT_DMG], item_dmg[i]);
                put_mod(ui_str[UI_CBT_ARM], item_armor[i]);;
            }
            ui_newline();
            any = 1;
        }
    }
    if (!any) {
        scr_puts(ui_str[UI_INV_EMPTY]);
        ui_newline();
    }
    ui_wait_key(ui_str[UI_ANYKEY]);
}

void ui_choice(u8 num, const char *label, u8 len)
{
    u8 k;
    scr_putc((char)('0' + num));
    scr_putc(')');
    scr_putc(' ');
    col = 3;
    for (k = 0; k < len; ++k) {
        scr_putc(label[k]);
        if (++col >= scr_cols) col = 0;
    }
    ui_newline();
}

void ui_col_reset(void)
{
    col = 0;
}

/* Choix ENCHAINES sur la/les ligne(s) : "1) A  2) B  3) C", retour a la ligne
 * automatique quand ca ne tient pas. Utile en fenetre mixte (4 lignes / 40 col). */
void ui_choices_flow(const char *const *labels, const u8 *lens, u8 count)
{
    u8 i, k, w;
    for (i = 0; i < count; ++i) {
        w = (u8)(3 + lens[i]);            /* "N) " + libelle */
        if (col != 0) {
            if (col + 2 + w > scr_cols)
                ui_newline();             /* passe a la ligne */
            else {
                scr_putc(' '); scr_putc(' ');
                col = (u8)(col + 2);
            }
        }
        scr_putc((char)('0' + i + 1));
        scr_putc(')');
        scr_putc(' ');
        col = (u8)(col + 3);
        for (k = 0; k < lens[i]; ++k) {
            scr_putc(labels[i][k]);
            if (++col >= scr_cols) col = 0;
        }
    }
    ui_newline();
}

u8 ui_read_choice(u8 count)
{
    char c;
    scr_flush();                /* jette une touche pressee pendant le rendu */
    for (;;) {
        c = scr_getkey();
        if (c == 'I' || c == 'i') return KEY_INVENTORY;
        if (c == 'S' || c == 's') return KEY_SAVE;
        if (c == 'L' || c == 'l') return KEY_LOAD;
        if (c == 'Q' || c == 'q') return KEY_QUIT;
        if (c >= '1' && c < (char)('1' + count))
            return (u8)(c - '1');
    }
}

void ui_wait_key(const char *msg)
{
    ui_newline();
    scr_revers(1);          /* invite en video inversee */
    scr_puts(msg);
    scr_revers(0);
    scr_flush();
    (void)scr_getkey();
}

/* Texte centre horizontalement sur la ligne y. */
void ui_center(const char *s, u8 y)
{
    u8 len = 0;
    while (s[len])
        ++len;
    scr_gotoxy((u8)(len < scr_cols ? (scr_cols - len) / 2 : 0), y);
    scr_puts(s);
}

/* Barre de progression centree a l'ecran (H + V), reecrite sur place. */
void ui_progress(u16 done, u16 total)
{
    u8 width = 20, filled, k;
    if (total == 0)
        total = 1;
    filled = (u8)((unsigned long)done * width / total);
    scr_gotoxy((u8)(scr_cols > width + 2 ? (scr_cols - (width + 2)) / 2 : 0), 12);
    //scr_putc('-');
    for (k = 0; k < width; ++k)
        scr_putc(k < filled ? '#' : '.');
    //scr_putc('-');
}
