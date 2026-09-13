/* sinput.c -- scene a saisie clavier (cf. sinput.h). */

#include "sinput.h"
#include "scene.h"
#include "scr.h"
#include "ui.h"
#include "story.h"
#include "state.h"

/* Normalise une saisie : majuscules + suppression des espaces de bord. */
static void norm_input(char *s)
{
    u8 i, a, b;
    for (i = 0; s[i]; ++i)
        if (s[i] >= 'a' && s[i] <= 'z') s[i] = (char)(s[i] - 32);
    i = 0; while (s[i]) ++i;
    while (i && s[i - 1] == ' ') s[--i] = '\0';
    a = 0; while (s[a] == ' ') ++a;
    if (a) { b = 0; while (s[a]) s[b++] = s[a++]; s[b] = '\0'; }
}

u8 run_input(const char *prompt, u8 maxlen, u16 ans_pos, u8 nans,
             u16 body_start)
{
    char buf[INPUT_BUF_LEN];
    u8 i, blen, match = 0;

    ui_clear();
    ui_status();
    b_seek(body_start);               /* intro : paragraphes de la section */
    scene_render_texts();
    ui_newline();
    scr_puts(prompt);
    ui_newline();
    scr_puts("> ");
    if (maxlen > INPUT_BUF_LEN - 1)
        maxlen = INPUT_BUF_LEN - 1;
    scr_flush();                      /* jette une touche parasite avant la saisie */
    scr_readline(buf, maxlen);
    norm_input(buf);
    blen = 0; while (buf[blen]) ++blen;

    b_seek(ans_pos);
    for (i = 0; i < nans; ++i) {
        u8 al, k, same = 1;
        const char *a = b_str(&al);
        if (al == blen) {
            for (k = 0; k < al; ++k)
                if (a[k] != buf[k]) { same = 0; break; }
            if (same) match = 1;
        }
    }
    return match;
}
