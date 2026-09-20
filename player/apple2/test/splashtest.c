/* splashtest.c -- test hote de show_splash (common/src/simage.c) : quand le
 * splash s'affiche, quand il ne se rejoue pas, image absente, duree.
 * Les fonctions ecran/disque appelees par simage.c sont remplacees par des
 * compteurs. Lance par `make hosttest`. */
#include <stdio.h>
#include <string.h>
#include "simage.h"
#include "scene.h"
#include "assetcache.h"
#include "scr.h"
#include "platform.h"
#include "ui.h"
#include "game.h"

static int shown = 0, keyed = 0, exists = 1;
/* remplacants minimaux de tout ce que simage.c appelle */
signed char cache_load_compressed(const char *n, void *d){ (void)n;(void)d; return exists?0:-1; }
const char *cache_asset_path(const char *n){ return n; }
signed char scr_load_hgr(const char *n, scr_progress_cb cb){ (void)n;(void)cb; return exists?0:-1; }
void *scr_hgr_page(void){ static char b[8]; return b; }
void scr_gfx_on(void){ ++shown; } void scr_gfx_off(void){} void scr_gfx_mixed(void){}
u8 scr_vbl(void){ static u8 v; return v^=1; }
char scr_poll(void){ return 0; } void scr_flush(void){}
char scr_getkey(void){ ++keyed; return ' '; }
void scr_revers(u8 on){(void)on;} void ui_col_reset(void){} void scene_render_texts(void){}
void ui_center(const char *s,u8 r){(void)s;(void)r;}
const char *ui_str[64];
int main(void)
{
    SecHeader h; memset(&h,0,sizeof h);
    h.splash = 2; h.splash_secs = 0; h.splash_always = 0;
    splash_reset();
    show_splash(5,&h); if(shown!=1||keyed!=1){puts("FAIL 1");return 1;}     /* premiere arrivee, attend une touche */
    show_splash(5,&h); if(shown!=1){puts("FAIL 2 rejoue");return 1;}        /* meme section : pas rejoue */
    show_splash(6,&h); if(shown!=2){puts("FAIL 3");return 1;}               /* autre section avec splash */
    show_splash(5,&h); if(shown!=3){puts("FAIL 4");return 1;}               /* retour apres un autre splash : rejoue */
    h.splash_always = 1; show_splash(5,&h); if(shown!=4){puts("FAIL 5 always");return 1;}
    h.splash = NO_IMAGE; show_splash(9,&h); if(shown!=4){puts("FAIL 6 sans splash");return 1;}
    h.splash = 2; h.splash_always=0; exists=0; show_splash(7,&h); if(shown!=4){puts("FAIL 7 absente");return 1;}   /* image absente : rien */
    exists=1; h.splash_secs = 2; keyed=0; show_splash(8,&h); if(keyed!=0){puts("FAIL 8 duree");return 1;}          /* duree : pas d'attente clavier bloquante */
    splash_reset(); show_splash(8,&h); if(shown!=6){puts("FAIL 9 reset");return 1;}
    puts("splash: OK"); return 0;
}
