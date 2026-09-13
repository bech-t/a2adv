/* simage.c -- scenes graphiques (cf. simage.h).
 * Le chargement/affichage HIRES est dans scr.c ; ici l'orchestration. */

#include "simage.h"
#include "assetcache.h"
#include "platform.h"
#include "scene.h"
#include "scr.h"
#include "story.h"
#include "state.h"
#include "ui.h"
#include "game.h"

/* Nom de fichier d'un asset image : "IMGnn.<ext>" (index sur 2 chiffres,
 * extension IMG_EXT propre a la machine, cf. platform.h). */
static const char *img_name(u16 asset)
{
    static char name[] = "IMG00." IMG_EXT;
    name[3] = (char)('0' + asset / 10);
    name[4] = (char)('0' + asset % 10);
    return name;
}

/* Charge `hgr_name` (ex: "IMG00.HGR" sur Apple II, "IMG00.PI1" sur ST -- cf.
 * IMG_EXT dans platform.h) en page graphique, en preferant une eventuelle
 * variante compressee (cf. assetcache.h:cache_load_compressed -- format et
 * existence meme du codec au choix du backend, ce fichier n'en sait rien)
 * a `hgr_name` telle quelle -- une aventure compilee avant l'introduction
 * de la compression, ou une plateforme qui n'en a pas, reste lisible sans
 * rien changer.
 *
 * Ne touche PAS au cache d'assets (cf. assetcache.h:cache_asset_path) :
 * c'est a l'appelant de decider s'il faut y ecrire le resultat
 * (cache_boot_fill) ou non (affichage direct, cf. img_load). Renvoie comme
 * scr_load_hgr : 0 = image chargee. */
signed char img_load_from_disk(const char *hgr_name)
{
    if (cache_load_compressed(hgr_name, scr_hgr_page()) == 0)
        return 0;
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

/* Charge une image en preferant sa copie en cache (cf. assetcache.h, deja
 * decompressee par cache_boot_fill : lecture directe, sans decodage). Si
 * elle n'a pas tenu en memoire (ou sur une plateforme sans cache), on
 * decompresse la disquette a la volee (img_load_from_disk) -- plus lent
 * qu'un cache, mais toujours correct : c'est le meme "meilleur effort" que
 * le cache lui-meme. */
signed char img_load(const char *name)
{
    const char *cached = cache_asset_path(name);
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
        ui_center(ui_str[UI_INTRO_HINT], UI_INTRO_HINT_ROW);  /* invite en derniere ligne */
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
    char name[] = "BOOT00." IMG_EXT;
    u8 i;
    for (i = 0; i < 100; ++i) {
        name[4] = (char)('0' + i / 10);
        name[5] = (char)('0' + i % 10);
        /* Pas de img_load ici : un splash n'est affiche qu'une fois, au boot,
         * donc pas de mise en cache -- la place sert mieux aux images du jeu,
         * qui reviennent. img_load_from_disk decompresse tout de meme une
         * eventuelle variante compressee : ca profite aussi aux splashes,
         * uniquement pour l'espace disque, sans rien coder de plus ici. */
        if (img_load_from_disk(name) != 0)
            break;                    /* plus de splash */
        scr_gfx_on();
        wait_or_key(SPLASH_SECS);
        /* pas de scr_gfx_off : le dernier splash reste a l'ecran pendant
         * l'ouverture de STORY0.DAT et le chargement du menu. */
    }
}
