/* scombat.c -- ecran de combat texte (cf. scombat.h). */

#include "scombat.h"
#include "scene.h"
#include "combat.h"
#include "scr.h"
#include "ui.h"
#include "story.h"
#include "state.h"
#include "snd.h"
#include "simage.h"

/* Petit afficheur d'entier 16 bits (sans zeros de tete). */
static void mput_num(u16 v)
{
    char buf[6];
    u8 k = 0;
    if (v == 0) { scr_putc('0'); return; }
    while (v) { buf[k++] = (char)('0' + v % 10); v /= 10; }
    while (k) scr_putc(buf[--k]);
}

u8 run_combat(u16 eimg, u8 att, u8 hp, u8 dmg, u8 armor,
              const char *name, u8 can_flee, u16 body_start)
{
    u8 r, first = 1;
    char c;

    if (eimg != NO_IMAGE)
        show_image(eimg, 1);              /* portrait de l'ennemi (~3 s ou touche) */

    combat_begin(att, hp, dmg, armor);
    for (;;) {
        ui_clear();
        ui_status();                      /* stats du heros (dont ses PV) */
        scr_revers(1);
        scr_putc(' '); scr_puts(name); scr_putc(' ');
        scr_revers(0);
        scr_puts("  "); scr_puts(ui_str[UI_CB_HP]); scr_putc(' ');
        mput_num(cb_enemy_hp);
        ui_newline(); ui_newline();

        if (first) {                      /* intro : paragraphes de la section */
            b_seek(body_start);
            scene_render_texts();
            first = 0;
        } else {                          /* log du dernier round */
            scr_puts(ui_str[UI_CB_DICE]); scr_putc(' '); mput_num(cb_pscore);
            scr_puts(" / ");  mput_num(cb_escore);
            ui_newline();
            if (cb_last_to == 0) { scr_puts(name); scr_puts(" -"); mput_num(cb_last_dmg); }
            else if (cb_last_to == 1) { scr_puts(ui_str[UI_CB_YOU]); scr_puts(" -");
                                        mput_num(cb_last_dmg); }
            else scr_puts(ui_str[UI_CB_PARRY]);
            ui_newline();
        }
        ui_newline();
        scr_puts("1) "); scr_puts(ui_str[UI_CB_ATTACK]);
        if (can_flee) { scr_puts("   3) "); scr_puts(ui_str[UI_CB_FLEE]); }
        ui_newline();

        scr_flush();                  /* anti double-appui entre les tours */
        c = scr_getkey();
        if (c == '3' && can_flee) {
            r = combat_flee();
            if (r == CB_LOSE) snd_play(SND_LOSE);  /* coup de fuite fatal */
            return r;                                  /* CB_FLEE ou CB_LOSE */
        }
        r = combat_attack();              /* toute autre touche = attaquer */
        if (r == CB_WIN) { snd_play(SND_WIN);  return CB_WIN; }
        if (r == CB_LOSE)  { snd_play(SND_LOSE); return CB_LOSE; }
    }
}
