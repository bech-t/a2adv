/* smenu.c -- menu principal + ecran d'options son (cf. smenu.h). */

#include <string.h>
#include "smenu.h"
#include "scr.h"
#include "ui.h"
#include "story.h"
#include "snd.h"
#include "simage.h"
#include "game.h"

/* Ecran d'options son actif seulement si la Mockingboard est compilee. */
#if defined(__CC65__) && defined(A2ADV_MOCKINGBOARD)
#define OPT_MB 1
#else
#define OPT_MB 0
#endif

/* Ecrit une chaine centree sur 40 col (fenetre mixte) a la ligne y. */
static void menu_center(const char *s, u8 y, u8 inverse)
{
    u8 len = 0;
    const char *p = s;
    while (*p++)
        ++len;
    scr_gotoxy((u8)(len < 40 ? (40 - len) / 2 : 0), y);
    if (inverse)
        scr_revers(1);
    scr_puts(s);
    if (inverse)
        scr_revers(0);
}

/* Noms des sons, dans l'ORDRE FIGE de l'enum SND_* (format.h) : le numero
 * affiche EST le code passe a snd_play(), et le nom est celui du DSL
 * `~ sound <nom>` (SOUND_NAMES, a2c/model.py). */
static const char *const snd_names[SND_COUNT] = {
    "SELECT", "ERROR", "WIN", "LOSE",
    "PICKUP", "HIT", "MAGIC", "DOOR", "PAGE"
};

/* "SORTIE : HAUT-PARLEUR" ou "SORTIE : MOCKINGBOARD (SLOT n)".
 * Partage par l'ecran Options et l'ecran de test. */
static void put_output(void)
{
    scr_puts(ui_str[UI_OPT_OUTPUT]);
    scr_puts(" : ");
    if (snd_backend) {
        scr_puts(ui_str[UI_OPT_MB]);
        scr_puts(" (");
        scr_puts(ui_str[UI_OPT_SLOT]);
        scr_putc(' ');
        scr_putc((char)('0' + snd_mb_slot));
        scr_putc(')');
    } else {
        scr_puts(ui_str[UI_OPT_SPEAKER]);
    }
}

/* --- Sous-ecran : test des sons ---------------------------------------
 * Une touche 0..8 rejoue le son de ce code, A les enchaine tous. Sert a
 * regler les tons de snd.c / snd_mb.c a l'oreille : on modifie, on
 * recompile, on rejoue le meme numero sans relancer une partie. */
static void run_sound_test(void)
{
    char c;
    u8 i;

    for (;;) {
        ui_clear();
        scr_revers(1); scr_putc(' '); scr_puts(ui_str[UI_SND_TITLE]); scr_putc(' ');
        scr_revers(0);

        scr_gotoxy(0, 2);
        put_output();

        /* Deux colonnes de 5 lignes : 0-4 a gauche, 5-8 a droite. */
        for (i = 0; i < SND_COUNT; ++i) {
            scr_gotoxy((u8)(i < 5 ? 2 : 21), (u8)(4 + (i < 5 ? i : i - 5)));
            scr_putc((char)('0' + i));
            scr_puts(") ");
            scr_puts(snd_names[i]);
        }

        scr_gotoxy(2, 10); scr_puts("A)   "); scr_puts(ui_str[UI_SND_ALL]);
        scr_gotoxy(2, 11); scr_puts("ESC) "); scr_puts(ui_str[UI_OPT_BACK]);
        scr_flush();

        c = scr_getkey();
        if (c == KEY_ESC)
            return;
        if (c >= '0' && c < (char)('0' + SND_COUNT)) {
            snd_play((u8)(c - '0'));
        } else if (c == 'A' || c == 'a') {
            for (i = 0; i < SND_COUNT; ++i) {
                scr_gotoxy(2, 13);                   /* indique le son en cours */
                scr_putc((char)('0' + i));
                scr_puts(") ");
                scr_puts(snd_names[i]);
                scr_puts("        ");                /* efface le nom precedent */
                snd_play(i);
                if (wait_or_key(1))                  /* une touche interrompt */
                    break;
            }
        }
    }
}

/* --- Ecran Options : choix du backend son (haut-parleur / Mockingboard) ---
 * Permet d'activer/desactiver la Mockingboard, de choisir son slot (1..7),
 * de relancer la detection auto et de tester le son sans recompiler. */
static void run_options(void)
{
    char c;
    for (;;) {
        ui_clear();
        scr_revers(1); scr_putc(' '); scr_puts(ui_str[UI_OPT_TITLE]); scr_putc(' ');
        scr_revers(0);
        ui_newline(); ui_newline();

        put_output();
        ui_newline(); ui_newline();

#if OPT_MB
        scr_puts("H)   "); scr_puts(ui_str[UI_OPT_SPEAKER]);  ui_newline();
        scr_puts("1-7) "); scr_puts(ui_str[UI_OPT_MB_SLOTS]); ui_newline();
#else
        scr_puts(ui_str[UI_OPT_NO_MB]);                       ui_newline();
#endif
        scr_puts("T)   "); scr_puts(ui_str[UI_OPT_TEST]);     ui_newline();
        scr_puts("ESC) "); scr_puts(ui_str[UI_OPT_BACK]);     ui_newline();
        scr_flush();

        c = scr_getkey();
        if (c == KEY_ESC)
            return;
        if (c == 'T' || c == 't') { run_sound_test(); continue; }
#if OPT_MB
        /* Choix MANUEL du backend : plus de detection automatique. */
        if (c == 'H' || c == 'h')            snd_use_mockingboard(0);
        else if (c >= '1' && c <= '7')       snd_use_mockingboard((u8)(c - '0'));
#endif
    }
}

/* Construit "1) NEW  2) LOAD  3) OPTIONS" dans dst (entierement localise). */
static void build_choices(char *dst)
{
    dst[0] = '\0';
    strcat(dst, "1) "); strcat(dst, ui_str[UI_MENU_NEW]);
    strcat(dst, "  2) "); strcat(dst, ui_str[UI_MENU_LOAD]);
    strcat(dst, "  3) "); strcat(dst, ui_str[UI_MENU_OPTIONS]);
}

/* Boucle du menu. Encadree par run_menu, qui lui pose la musique autour --
 * plus sur que de dupliquer un arret sur chacune de ses six sorties. */
static u8 menu_loop(void)
{
    char c;
    char line[64];

    for (;;) {                 /* boucle : redessine apres un retour d'Options */
        /* --- menu semi-graphique (image MENU.HGR + titre + choix en bas) --- */
        if (scr_load_hgr("MENU.HGR", 0) == 0) {
            scr_gfx_mixed();                   /* image en haut, 4 lignes en bas */
            menu_center(g_title, 20, 1);       /* titre */
            build_choices(line);
            menu_center(line, 22, 0);          /* 1) 2) 3) */
            line[0] = '\0';
            strcat(line, "Q) "); strcat(line, ui_str[UI_MENU_QUIT]);
            menu_center(line, 23, 0);          /* Q) quitter, ligne centree */
            scr_flush();               /* le chargement de MENU.HGR peut etre long */
            for (;;) {
                c = scr_getkey();
                if (c == '1') { scr_gfx_off(); return ACT_NEW; }
                if (c == '2') { scr_gfx_off(); return ACT_LOAD; }
                if (c == 'Q' || c == 'q') { scr_gfx_off(); return ACT_QUIT; }
                if (c == '3') { scr_gfx_off(); run_options(); break; }  /* -> redraw */
            }
            continue;                  /* revient dessiner le menu */
        }

        /* --- menu texte de repli (pas de MENU.HGR) --- */
        scr_gfx_off();
        for (;;) {
            ui_clear();
            menu_center(g_title, 3, 1);
            build_choices(line);
            menu_center(line, 6, 0);
            line[0] = '\0';
            strcat(line, "Q) "); strcat(line, ui_str[UI_MENU_QUIT]);
            menu_center(line, 8, 0);
            scr_flush();
            c = scr_getkey();
            if (c == '1') return ACT_NEW;
            if (c == '2') return ACT_LOAD;
            if (c == 'Q' || c == 'q') return ACT_QUIT;
            if (c == '3') { run_options(); break; }   /* -> redraw (boucle externe) */
        }
    }
}

u8 run_menu(void)
{
    u8 act;
    snd_music(MUS_TITLE);     /* no-op si le backend est le haut-parleur */
    act = menu_loop();
    snd_music(MUS_NONE);      /* silence des qu'on entre dans l'aventure */
    return act;
}
