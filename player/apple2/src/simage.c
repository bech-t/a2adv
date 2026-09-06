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
#include "zx02_getbyte.h"

/* Nom de fichier d'un asset image : "IMGnn.HGR" (index sur 2 chiffres). */
static const char *img_name(u16 asset)
{
    static char name[] = "IMG00.HGR";
    name[3] = (char)('0' + asset / 10);
    name[4] = (char)('0' + asset % 10);
    return name;
}

/* Charge `hgr_name` (ex: "IMG00.HGR") en page HIRES, en preferant sa version
 * compressee sur la disquette : "<meme base>.ZX2" (produite par le Makefile,
 * cf. tools/zx02) si elle existe, sinon `hgr_name` telle quelle -- une
 * aventure compilee avant l'introduction de la compression, ou une image que
 * compresser n'aidait pas, reste lisible sans rien changer.
 *
 * Decompression EN FLUX : zx_getbyte lit le fichier .ZX2 par blocs de 256 o
 * (cf. zx02_getbyte.c) ; le flux compresse entier n'a donc jamais besoin de
 * tenir en RAM, quelle que soit la taille de l'image. C'est ce choix qui
 * remplace la decompression EN PLACE envisagee d'abord (testee, puis
 * abandonnee : elle corrompt les donnees des que la sortie devient
 * nettement plus grosse que l'entree, ce qui est systematiquement le cas
 * pour des images qui compressent bien).
 *
 * Ne touche PAS au cache /RAM : c'est a l'appelant de decider s'il faut y
 * ecrire le resultat (ram_boot_fill, cf. ramdisk.c) ou non (affichage direct,
 * cf. img_load). Renvoie comme scr_load_hgr : 0 = image en page HIRES. */
signed char img_load_from_disk(const char *hgr_name)
{
    char zx_name[11];        /* "BOOT00.ZX2" (le plus long) + NUL = 11 o */
    FILE *f;
    u8 i;

    for (i = 0; hgr_name[i] != '\0' && hgr_name[i] != '.' && i < 6; ++i)
        zx_name[i] = hgr_name[i];
    zx_name[i++] = '.'; zx_name[i++] = 'Z'; zx_name[i++] = 'X'; zx_name[i++] = '2';
    zx_name[i] = '\0';

    f = fopen(zx_name, "rb");
    if (f != NULL) {
        zx_getbyte_init(f);
        zx02_unpack(scr_hgr_page());
        fclose(f);
        return 0;
    }
    return scr_load_hgr(hgr_name, 0);      /* repli : pas de version compressee */
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

/* Charge une image en preferant sa copie en disque RAM (deja decompressee
 * par ram_boot_fill : lecture directe, sans decodage). Si elle n'a pas tenu
 * en memoire, on decompresse la disquette a la volee (img_load_from_disk) --
 * plus lent qu'un cache, mais toujours correct : c'est le meme "meilleur
 * effort" que le cache lui-meme. */
signed char img_load(const char *name)
{
    const char *cached = ram_file_path(name);
    if (cached != name)
        return scr_load_hgr(cached, 0);
    return img_load_from_disk(name);
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
        /* Pas de img_load ici : un splash n'est affiche qu'une fois, au boot,
         * donc pas de mise en cache /RAM -- la place sert mieux aux images du
         * jeu, qui reviennent. img_load_from_disk decompresse tout de meme
         * si un .ZX2 existe : la compression profite aussi aux splashes,
         * uniquement pour l'espace disque, sans rien coder de plus ici. */
        if (img_load_from_disk(name) != 0)
            break;                    /* plus de splash */
        scr_gfx_on();
        wait_or_key(SPLASH_SECS);
        /* pas de scr_gfx_off : le dernier splash reste a l'ecran pendant
         * l'ouverture de STORY0.DAT et le chargement du menu. */
    }
}
