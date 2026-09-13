/* smenu.c -- menu principal + ecran d'options son (cf. smenu.h). */

#include <string.h>
#include "smenu.h"
#include "scr.h"
#include "ui.h"
#include "story.h"
#include "snd.h"
#include "simage.h"
#include "game.h"
#include "platform.h"
#include "sysinfo.h"

/* Ecrit une chaine centree sur `width` colonnes a la ligne y. `width` = 40
 * pour le menu semi-graphique (fenetre mixte, toujours 40 col qu'importe
 * scr_cols) ; `width` = scr_cols pour le repli texte plein ecran -- sans
 * quoi ce dernier reste cale sur 40 meme en 80 colonnes (bug reel, constate
 * en pratique le 2026-09-09 en portant sur Atari ST, corrige ici aussi). */
static void menu_center(const char *s, u8 y, u8 inverse, u8 width)
{
    u8 len = 0;
    const char *p = s;
    while (*p++)
        ++len;
    scr_gotoxy((u8)(len < width ? (width - len) / 2 : 0), y);
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

/* --- Sous-ecran : test des sons ---------------------------------------
 * Une touche 0..8 rejoue le son de ce code, A les enchaine tous. Sert a
 * regler les tons de snd.c a l'oreille : on modifie, on recompile, on
 * rejoue le meme numero sans relancer une partie. */
static void run_sound_test(void)
{
    char c;
    u8 i;

    for (;;) {
        ui_clear();
        scr_revers(1); scr_putc(' '); scr_puts(ui_str[UI_SND_TITLE]); scr_putc(' ');
        scr_revers(0);

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

/* --- Sous-ecran : info systeme (cf. sysinfo.h) -------------------------
 * sys_info() ecrit son propre nombre de lignes, au choix de la plateforme :
 * ce fichier ne connait que le titre et le retour, comme run_sound_test. */
static void run_sys_info(void)
{
    ui_clear();
    scr_revers(1); scr_putc(' '); scr_puts(ui_str[UI_SYSINFO_TITLE]); scr_putc(' ');
    scr_revers(0);
    ui_newline(); ui_newline();

    sys_info();

    ui_newline();
    scr_puts("ESC) "); scr_puts(ui_str[UI_OPT_BACK]); ui_newline();
    scr_flush();

    while (scr_getkey() != KEY_ESC)
        ;
}

/* --- Ecran Options : test des sons + info systeme ----------------------
 * Aucune des deux plateformes actuelles n'offre de backend son a choisir a
 * la main (Apple II : haut-parleur seul ; Atari ST : YM2149 toujours actif). */
static void run_options(void)
{
    char c;
    for (;;) {
        ui_clear();
        scr_revers(1); scr_putc(' '); scr_puts(ui_str[UI_OPT_TITLE]); scr_putc(' ');
        scr_revers(0);
        ui_newline(); ui_newline();

        scr_puts("T)   "); scr_puts(ui_str[UI_OPT_TEST]);     ui_newline();
        scr_puts("I)   "); scr_puts(ui_str[UI_OPT_INFO]);     ui_newline();
        scr_puts("ESC) "); scr_puts(ui_str[UI_OPT_BACK]);     ui_newline();
        scr_flush();

        c = scr_getkey();
        if (c == KEY_ESC)
            return;
        if (c == 'T' || c == 't') { run_sound_test(); continue; }
        if (c == 'I' || c == 'i') { run_sys_info(); continue; }
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
        /* (Re)lance le theme a CHAQUE dessin du menu, pas seulement a la
         * premiere entree -- no-op sur une plateforme sans musique de fond
         * (cf. snd.h), les appelants n'ont donc aucun test a faire. */
        snd_menu_music(1);

        /* --- menu semi-graphique (image MENU + titre + choix en bas) --- */
        if (img_load("MENU." IMG_EXT) == 0) {
            scr_gfx_mixed();                   /* image en haut, 4 lignes en bas */
            menu_center(g_title, MENU_ROW_TITLE, 1, 40);   /* titre */
            build_choices(line);
            menu_center(line, MENU_ROW_CHOICES, 0, 40);    /* 1) 2) 3) */
            line[0] = '\0';
            strcat(line, "Q) "); strcat(line, ui_str[UI_MENU_QUIT]);
            menu_center(line, MENU_ROW_QUIT, 0, 40);       /* Q) quitter, ligne centree */
            scr_flush();               /* le chargement de l'image peut etre long */
            for (;;) {
                c = scr_getkey();
                if (c == '1') { scr_gfx_off(); return ACT_NEW; }
                if (c == '2') { scr_gfx_off(); return ACT_LOAD; }
                if (c == 'Q' || c == 'q') { scr_gfx_off(); return ACT_QUIT; }
                if (c == '3') { scr_gfx_off(); run_options(); break; }  /* -> redraw */
            }
            continue;                  /* revient dessiner le menu */
        }

        /* --- menu texte de repli (pas d'image MENU) --- */
        scr_gfx_off();
        for (;;) {
            ui_clear();
            menu_center(g_title, 3, 1, scr_cols);
            build_choices(line);
            menu_center(line, 6, 0, scr_cols);
            line[0] = '\0';
            strcat(line, "Q) "); strcat(line, ui_str[UI_MENU_QUIT]);
            menu_center(line, 8, 0, scr_cols);
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
    u8 act = menu_loop();     /* le theme est (re)lance a chaque dessin, dedans */
    snd_menu_music(0);        /* silence des qu'on entre dans l'aventure */
    return act;
}
