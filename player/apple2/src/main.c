/* main.c -- player a2adv : boucle de jeu et orchestration.
 *
 * Charge STORY.DAT, joue les sections en streaming. Les scenes specialisees
 * sont dans leurs modules : scombat.c (combat), sinput.c (saisie), simage.c
 * (images/splash), smenu.c (menu). L'en-tete de section est lu par scene.c. */

#include <stdio.h>
#include "scr.h"
#include "story.h"
#include "state.h"
#include "save.h"
#include "snd.h"
#include "ui.h"
#include "combat.h"      /* rng_seed */
#include "scene.h"
#include "simage.h"
#include "scombat.h"
#include "sinput.h"
#include "smenu.h"
#include "ramdisk.h"
#include "lang.h"
#include "game.h"
#include "z2_intro.h"

/* Choix visibles collectes pour la section courante */
static u16         v_target[MAX_CHOICES];
static u16         v_effpos[MAX_CHOICES];
static const char *v_label[MAX_CHOICES];
static u8          v_llen[MAX_CHOICES];

/* Reprise depuis une sauvegarde : sauter l'on_enter a la prochaine entree. */
static u8 g_resume;

/* Message plein ecran + attente touche. */
static void msg(const char *s)
{
    ui_clear();
    ui_status();
    scr_puts(s);
    ui_wait_key(ui_str[UI_ANYKEY]);
}

/* Sauvegarde : ferme STORY (un seul fichier ouvert), ecrit, rouvre. */
static void do_save(u16 section)
{
    signed char r;
    ui_clear();
    ui_center(ui_str[UI_SAVING], 10);        /* titre centre */
    r = save_state(section, ui_progress);     /* SAVE0.DAT ouvert en + de STORY */
    ui_center(ui_str[r == 0 ? UI_SAVED : UI_SAVE_FAIL], 14);
    scr_revers(1);
    ui_center(ui_str[UI_ANYKEY], 16);
    scr_revers(0);
    (void)scr_getkey();
}

/* Chargement : renvoie 0 et *out si ok, <0 sinon. */
static signed char do_load(u16 *out)
{
    signed char r;
    ui_clear();
    ui_center(ui_str[UI_LOADING], 10);
    r = load_state(out, ui_progress);
    if (r != 0)
        msg(ui_str[UI_NO_SAVE]);
    return r;
}

/* Confirmation de retour au menu. 1 = revenir au menu, 0 = annuler.
 * 'S' sauvegarde puis revient ; 'Q' revient sans sauver ; ESC annule. */
static u8 confirm_quit(u16 section)
{
    char c;
    ui_clear();
    scr_puts(ui_str[UI_QUIT_CONFIRM]); ui_newline(); ui_newline();
    scr_puts(ui_str[UI_QUIT_SAVE]);    ui_newline();
    scr_puts(ui_str[UI_QUIT_NOSAVE]);  ui_newline();
    scr_puts(ui_str[UI_QUIT_CANCEL]);  ui_newline();
    for (;;) {
        c = scr_getkey();
        if (c == 'S' || c == 's') {     /* sauver puis revenir au menu */
            ui_clear();
            ui_center(ui_str[UI_SAVING], 10);
            (void)save_state(section, ui_progress);
            return 1;
        }
        if (c == 'Q' || c == 'q')       /* quitter sans sauver */
            return 1;
        if (c == KEY_ESC)               /* annuler */
            return 0;
    }
}

static void ending_banner(u8 ending)
{
    if (ending == END_VICTOIRE)
        scr_puts(ui_str[UI_END_WIN]);
    else if (ending == END_DEFAITE)
        scr_puts(ui_str[UI_END_LOSE]);
    else
        scr_puts(ui_str[UI_END_GENERIC]);
}

/* Joue une section ; renvoie l'index de la suivante, ou GAME_OVER. */
static u16 play_section(u16 idx)
{
    SecHeader h;
    u8  i, n_choices, visible, sel, llen, mixed;
    u16 target, g, body_start, exit_pos;
    const char *label;

    if (story_load_section(idx) != 0) {
        ui_clear();
        scr_puts(ui_str[UI_SECTION_ERR]);
        ui_wait_key("");
        return GAME_OVER;
    }

    scene_read_header(&h);

    /* effets d'entree : appliques UNE fois ; un GOTO redirige sans afficher.
     * A la reprise d'une sauvegarde, l'etat est deja post-on_enter : on saute. */
    if (g_resume) {
        g_resume = 0;
        state_skip_effects();
    } else {
        g = state_apply_effects();
        if (g != NO_GOTO)
            return g;
        if (g_moves_on)
            ++g_moves;      /* section effectivement affichee = un mouvement */
    }

    /* effets de SORTIE : memorises puis sautes pour atteindre le corps. */
    exit_pos = b_tell();
    state_skip_effects();

    /* section de combat : ecran dedie, puis effets d'issue + de sortie. */
    if (h.has_combat) {
        u8  outcome, omlen;
        u16 fxpos;
        const char *omsg;
        body_start = b_tell();
        outcome = run_combat(h.cb_eimg != NO_IMAGE ? h.cb_eimg : h.image,
                             h.cb_att, h.cb_hp, h.cb_dmg, h.cb_armor, h.cb_name,
                             (u8)(h.cb_flee != NO_GOTO), body_start);
        if (outcome == CB_VICTOIRE)   { target = h.cb_win;  fxpos = h.cb_winfx;
                                        omsg = h.cb_winmsg;  omlen = h.cb_winmsg_len; }
        else if (outcome == CB_FUITE) { target = h.cb_flee; fxpos = h.cb_fleefx;
                                        omsg = h.cb_fleemsg; omlen = h.cb_fleemsg_len; }
        else                          { target = h.cb_lose; fxpos = h.cb_losefx;
                                        omsg = h.cb_losemsg; omlen = h.cb_losemsg_len; }
        b_seek(fxpos);   state_apply_effects();   /* effets propres a l'issue */
        b_seek(exit_pos); state_apply_effects();  /* effets de sortie */
        if (omlen) {                 /* texte d'issue defini dans la scene + pause */
            ui_clear();
            ui_status();
            ui_paragraph(omsg, omlen, 0);
            ui_wait_key(ui_str[UI_ANYKEY]);
        }
        return target;
    }

    /* section a saisie : lit une reponse, branche bonne/mauvaise + effets. */
    if (h.has_input) {
        u8  ok;
        u16 fxpos;
        body_start = b_tell();
        ok = run_input(h.inp_prompt, h.inp_maxlen, h.inp_anspos, h.inp_nans,
                       body_start);
        if (ok) { target = h.inp_correct; fxpos = h.inp_cfx; }
        else    { target = h.inp_wrong;   fxpos = h.inp_wfx; }
        b_seek(fxpos);   state_apply_effects();
        b_seek(exit_pos); state_apply_effects();
        return target;
    }

    /* image_text = scene MIXTE (image en haut + fenetre texte) ;
     * full_image = image plein ecran puis texte. */
    mixed = (u8)(h.mode == MODE_IMAGE_TEXT && h.image != NO_IMAGE);
    if (mixed)
        load_scene_image(h.image);
    else if (h.image != NO_IMAGE)
        show_image(h.image, 1);

    body_start = b_tell();   /* corps re-rendu sans re-appliquer on_enter */

    for (;;) {
        b_seek(body_start);
        if (mixed) {
            scr_gfx_mixed();          /* image en haut + fenetre texte 40 col */
            ui_col_reset();
        } else {
            ui_clear();
            ui_status();
        }

        scene_render_texts();     /* paragraphes visibles de la section */

        /* choix : on collecte les visibles */
        n_choices = b_u8();
        visible = 0;
        for (i = 0; i < n_choices; ++i) {
            u8 vis = state_eval_cond();
            u16 effpos = b_tell();
            state_skip_effects();                    /* saute l'effect_list */
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

        /* fin d'aventure ? */
        if (h.ending != END_NONE || n_choices == 0) {
            ui_newline();
            ending_banner(h.ending);
            if (h.ending == END_VICTOIRE)
                snd_play(SND_WIN);
            else if (h.ending == END_DEFAITE)
                snd_play(SND_LOSE);
            ui_wait_key(ui_str[UI_ANYKEY]);
            if (mixed) scr_gfx_off();
            return GAME_OVER;
        }
        if (visible == 0) {
            scr_puts(ui_str[UI_NO_EXIT]);
            ui_wait_key(ui_str[UI_ANYKEY]);
            if (mixed) scr_gfx_off();
            return GAME_OVER;
        }

        ui_newline();
        if (mixed)                    /* choix enchaines (fenetre 4 lignes) */
            ui_choices_flow(v_label, v_llen, visible);
        else                          /* choix un par ligne (plein ecran) */
            for (i = 0; i < visible; ++i)
                ui_choice((u8)(i + 1), v_label[i], v_llen[i]);

        sel = ui_read_choice(visible);
        if (sel == KEY_INVENTORY) {
            if (mixed) scr_gfx_off();
            ui_inventory();
            continue;               /* re-render (mixte : re-affiche l'image) */
        }
        if (sel == KEY_SAVE) {
            if (mixed) scr_gfx_off();
            do_save(idx);
            continue;
        }
        if (sel == KEY_LOAD) {
            u16 loaded;
            if (mixed) scr_gfx_off();
            if (do_load(&loaded) == 0) {
                g_resume = 1;        /* reprise : pas de re-application on_enter */
                return loaded;
            }
            continue;
        }
        if (sel == KEY_QUIT) {
            if (mixed) scr_gfx_off();
            if (confirm_quit(idx))
                return GAME_OVER;      /* retour au menu (pas sortie programme) */
            continue;
        }

        if (mixed) scr_gfx_off();
        b_seek(v_effpos[sel]);
        g = state_apply_effects();          /* effets du choix */
        target = v_target[sel];
        if (g != NO_GOTO)
            target = g;
        b_seek(exit_pos);                   /* effets de SORTIE (tout choix) */
        g = state_apply_effects();
        if (g != NO_GOTO)
            target = g;
        return target;
    }
}

/* Prepare une scene d'intro : renvoie son asset image (NO_IMAGE si aucune).
 * Si pas d'image, rend le texte de la scene ; sinon l'appelant affiche l'image. */
static u16 render_scene(u16 idx)
{
    SecHeader h;

    if (story_load_section(idx) != 0)
        return NO_IMAGE;
    scene_read_header(&h);
    state_skip_effects();    /* on_enter : ignore pour une scene d'intro */
    state_skip_effects();    /* on_exit  : ignore aussi */

    if (h.image != NO_IMAGE)
        return h.image;      /* scene image : affichee par run_intro */

    ui_clear();
    ui_newline();
    scene_render_texts();
    return NO_IMAGE;
}

/* Joue l'intro de l'aventure : une touche avance, ESC saute toute l'intro. */
static void run_intro(void)
{
    u8 i;
    char c;
    for (i = 0; i < g_nintro; ++i) {
        u16 image = render_scene(intro_idx[i]);
        if (image != NO_IMAGE) {
            c = show_intro_image(image);       /* mixte : image + invite en bas */
        } else {
            ui_newline();
            scr_revers(1);                     /* invite en video inversee */
            scr_puts(ui_str[UI_INTRO_HINT]);
            scr_revers(0);
            c = scr_getkey();
        }
        if (c == KEY_ESC)                      /* ESC saute l'intro */
            return;
    }
}

/* Barre de progression de la copie vers /RAM. L'entete n'est dessine qu'au
 * PREMIER appel : s'il n'y a rien a copier (pas de /RAM), on n'efface pas le
 * splash. APP.LNG est charge AVANT, donc le libelle est bien localise. */
static void boot_progress(u16 done, u16 total)
{
    static u8 drawn = 0;
    if (!drawn) {
        drawn = 1;
        scr_gfx_off();
        ui_clear();
        ui_center(ui_str[UI_LOADING], 10);
    }
    ui_progress(done, total);
}

int main(void)
{
    u16 cur;
    u8 act;

    ui_init();        /* detecte 40/80 col et configure l'ecran */
    run_splashes();   /* BOOTxx.HGR (avant d'ouvrir l'histoire) */
    z2_intro();       /* jingle haut-parleur : occupe l'attente sur le dernier
                       * splash pendant que /RAM et STORY0.DAT se chargent. */

    /* Cache /RAM des STORYnn.DAT (spec §7quater) : AVANT story_open, ou seuls
     * 2 tampons ProDOS sont necessaires. Sans /RAM (64 Ko) : aucun effet, et
     * le splash reste a l'ecran puisque la barre n'est jamais appelee. */
    /* Socle d'interface EN PREMIER : la barre de chargement /RAM ci-dessous
     * l'utilise deja, et story_open posera ensuite par-dessus les seules
     * chaines que l'aventure surcharge (spec §6.1). */
    lang_load();

    ram_boot_fill(0, boot_progress);

    {
        /* Messages en dur : ils precedent la lecture des donnees, donc aussi
         * celle des chaines d'UI (seule exception admise, cf. spec §6.1). */
        signed char err = story_open(STORY_FILE);
        if (err != 0) {
            scr_gfx_off();           /* quitter le splash pour un ecran texte */
            ui_clear();
            if (err == -6) {         /* disquette et player desaccordes */
                scr_puts("ERROR - STORY FORMAT V");
                scr_putc((char)('0' + g_story_version));
                scr_puts(", PLAYER NEEDS V");
                scr_putc((char)('0' + STORY_FORMAT_VERSION));
            } else {
                scr_puts("ERROR - CAN'T READ STORY00.DAT");
            }
            ui_wait_key("");
            return 1;
        }
    }

    /* Pas de detection son au boot : le backend se choisit a la main dans
     * OPTIONS (haut-parleur par defaut). */
    state_init();     /* defaults sains pour les ecrans d'etat du menu */

    for (;;) {
        act = run_menu();
        if (act == ACT_QUIT)
            break;
        if (act == ACT_NEW) {
            state_init();
            rng_seed(scr_entropy | 1u);   /* graine : temps de reaction au menu */
            run_intro();                  /* intro de l'aventure (skippable) */
            cur = g_start;
            g_resume = 0;
        } else {                          /* ACT_LOAD */
            if (do_load(&cur) != 0)
                continue;                 /* pas de sauvegarde -> retour menu */
            g_resume = 1;
        }
        while (cur != GAME_OVER)
            cur = play_section(cur);
        /* aventure terminee OU 'Q' en jeu -> retour au menu ; seul ACT_QUIT sort. */
    }

    story_close();
    return 0;         /* rend la main a ProDOS */
}
