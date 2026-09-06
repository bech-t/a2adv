/* simage.c -- scenes graphiques (cf. simage.h).
 * Le chargement/affichage HIRES est dans scr.c ; ici l'orchestration. */

#include "simage.h"
#include "ramdisk.h"
#include "scene.h"
#include "scr.h"
#include "story.h"
#include "state.h"
#include "ui.h"
#include "game.h"

/* Nom de fichier d'un asset image : "IMGnn.HGR" (index sur 2 chiffres). */
static const char *img_name(u16 asset)
{
    static char name[] = "IMG00.HGR";
    name[3] = (char)('0' + asset / 10);
    name[4] = (char)('0' + asset % 10);
    return name;
}

char wait_or_key(u8 secs)
{
    u16 frames = (u16)secs * 60;
    u16 f = 0;
    unsigned long guard = 0;
    unsigned long guard_max = 60000UL * secs;
    u8 prev = scr_vbl();
    char c;

    scr_flush();                  /* ignore une touche pressee avant l'affichage */
    while (f < frames) {
        u8 cur = scr_vbl();
        if (cur != prev) {
            prev = cur;
            if (cur)                  /* un front montant par trame */
                ++f;
        }
        c = scr_poll();
        if (c)
            return c;
        if (++guard >= guard_max)     /* securite anti-blocage */
            return 0;
    }
    return 0;
}

/* Charge une image en preferant sa copie en disque RAM. Le prechargement a
 * lieu au boot (ram_boot_fill) : ici on se contente de choisir le chemin. Si
 * l'image n'a pas tenu en memoire, on lit la disquette, comme avant. */
signed char img_load(const char *name)
{
    return scr_load_hgr(ram_file_path(name), 0);
}

/* STORY reste ouvert : scr_load_hgr ouvre le HGR comme 2e fichier (FOPEN_MAX=8),
 * ce qui preserve le tampon/cache de STORY et evite un OPEN repete. */
char show_image(u16 asset, u8 timed)
{
    char c = 0;
    if (asset >= 100)
        return 0;
    if (img_load(img_name(asset)) == 0) {
        scr_gfx_on();
        c = timed ? wait_or_key(SPLASH_SECS) : scr_getkey();
        scr_gfx_off();
    }
    return c;
}

char show_intro_image(u16 asset)
{
    char c = 0;
    if (asset >= 100)
        return 0;
    if (img_load(img_name(asset)) == 0) {
        scr_gfx_mixed();                 /* image + fenetre texte 40 col en bas */
        ui_col_reset();
        scene_render_texts();
        scr_revers(1);
        ui_center(ui_str[UI_INTRO_HINT], 23);   /* invite sur la derniere ligne */
        scr_revers(0);
        scr_flush();
        c = scr_getkey();
        scr_gfx_off();
    }
    return c;
}

void load_scene_image(u16 asset)
{
    if (asset >= 100)
        return;
    img_load(img_name(asset));
}

void run_splashes(void)
{
    char name[] = "BOOT00.HGR";
    u8 i;
    for (i = 0; i < 100; ++i) {
        name[4] = (char)('0' + i / 10);
        name[5] = (char)('0' + i % 10);
        /* Pas de img_load ici : un splash n'est affiche qu'une fois, au boot.
         * Le cacher prendrait la place des images du jeu, qui reviennent. */
        if (scr_load_hgr(name, 0) != 0)
            break;                    /* plus de splash */
        scr_gfx_on();
        wait_or_key(SPLASH_SECS);
        /* pas de scr_gfx_off : le dernier splash reste a l'ecran pendant
         * l'ouverture de STORY0.DAT et le chargement du menu. */
    }
}
