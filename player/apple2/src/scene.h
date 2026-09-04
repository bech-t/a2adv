/* scene.h -- lecture factorisee de l'en-tete d'une section.
 *
 * Le corps d'une section commence par un en-tete a blocs optionnels
 * (mode, ending, image, [combat], [saisie]) avant les effets/textes. Cette
 * fonction le lit UNE fois pour tous les consommateurs (play_section,
 * render_scene) afin d'eviter toute desynchronisation quand le format evolue. */
#ifndef A2ADV_SCENE_H
#define A2ADV_SCENE_H

#include "format.h"

typedef struct {
    u8  mode, ending;
    u16 image;
    /* combat (has_combat=0 si absent) */
    u8  has_combat, cb_att, cb_hp, cb_dmg, cb_armor;
    u16 cb_eimg, cb_win, cb_lose, cb_flee, cb_winfx, cb_losefx, cb_fleefx;
    char cb_name[ENEMY_NAME_LEN];
    /* textes d'issue (pointent dans secbuf ; *_len == 0 = aucun) */
    const char *cb_winmsg, *cb_losemsg, *cb_fleemsg;
    u8  cb_winmsg_len, cb_losemsg_len, cb_fleemsg_len;
    /* saisie (has_input=0 si absent) */
    u8  has_input, inp_maxlen, inp_nans;
    u16 inp_anspos, inp_correct, inp_wrong, inp_cfx, inp_wfx;
    char inp_prompt[PROMPT_LEN + 1];
} SecHeader;

/* Section deja chargee (curseur secbuf au debut) : lit l'en-tete dans *h et
 * laisse le curseur positionne sur l'effect_list @on_enter. */
void scene_read_header(SecHeader *h);

/* Rend le bloc de paragraphes (curseur sur le compteur n_texts) : n'affiche que
 * les paragraphes dont la condition est vraie, une ligne vide entre eux.
 * Avance le curseur au-dela du bloc. Utilise par toutes les scenes (texte,
 * combat, saisie, intro). */
void scene_render_texts(void);

#endif /* A2ADV_SCENE_H */
