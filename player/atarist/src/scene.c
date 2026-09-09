/* scene.c -- lecture de l'en-tete d'une section (cf. scene.h). */

#include "scene.h"
#include "story.h"
#include "state.h"
#include "ui.h"

void scene_read_header(SecHeader *h)
{
    u8 i;

    h->mode = b_u8();
    h->ending = b_u8();
    h->image = b_u16();

    /* bloc combat optionnel */
    h->has_combat = b_u8();
    if (h->has_combat) {
        const char *nm;
        u8 nlen;
        h->cb_att = b_u8(); h->cb_hp = b_u8();
        h->cb_dmg = b_u8(); h->cb_armor = b_u8();
        h->cb_eimg = b_u16();
        h->cb_win = b_u16(); h->cb_lose = b_u16(); h->cb_flee = b_u16();
        nm = b_str(&nlen);
        if (nlen >= ENEMY_NAME_LEN) nlen = ENEMY_NAME_LEN - 1;
        for (i = 0; i < nlen; ++i) h->cb_name[i] = nm[i];
        h->cb_name[nlen] = '\0';
        h->cb_winfx = b_tell();  state_skip_effects();
        h->cb_losefx = b_tell(); state_skip_effects();
        h->cb_fleefx = b_tell(); state_skip_effects();
        h->cb_winmsg  = b_str(&h->cb_winmsg_len);   /* textes d'issue (dans secbuf) */
        h->cb_losemsg = b_str(&h->cb_losemsg_len);
        h->cb_fleemsg = b_str(&h->cb_fleemsg_len);
    }

    /* bloc saisie optionnel */
    h->has_input = b_u8();
    if (h->has_input) {
        const char *p;
        u8 plen;
        p = b_str(&plen);
        if (plen > PROMPT_LEN) plen = PROMPT_LEN;
        for (i = 0; i < plen; ++i) h->inp_prompt[i] = p[i];
        h->inp_prompt[plen] = '\0';
        h->inp_maxlen = b_u8();
        h->inp_nans = b_u8();
        h->inp_anspos = b_tell();
        for (i = 0; i < h->inp_nans; ++i) { u8 al; (void)b_str(&al); }
        h->inp_correct = b_u16();
        h->inp_wrong = b_u16();
        h->inp_cfx = b_tell(); state_skip_effects();
        h->inp_wfx = b_tell(); state_skip_effects();
    }
}

void scene_render_texts(void)
{
    u8 n = b_u8(), i, printed = 0;
    for (i = 0; i < n; ++i) {
        u8 vis = state_eval_cond();
        u8 style = b_u8();
        u16 len = b_u16();
        u16 pos = b_tell();
        if (vis) {
            if (printed)
                ui_newline();                /* ligne vide entre paragraphes */
            ui_paragraph((const char *)&secbuf[pos], len, style);
            printed = 1;
        }
        b_seek((u16)(pos + len));
    }
}
