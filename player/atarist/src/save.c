/* save.c -- serialisation de l'etat joueur vers un fichier ProDOS. */

#include <stdio.h>
#include "save.h"
#include "story.h"
#include "state.h"

#define SAVE_NAME  "SAVE0.DAT"
/* bloc : section(2) + stat_val[n] + stat_max[n] + items + flags + score(2) + moves(2) */
#define STATE_MAX  (2 + 2 * MAX_STATS + (MAX_ITEMS + 7) / 8 + (MAX_FLAGS + 7) / 8 + 4)

static u8 state_len(void)
{
    return (u8)(2 + 2 * g_nstats + (g_nitems + 7) / 8 + (g_nflags + 7) / 8 + 4);
}

#define STATE_STEP  4    /* octets par bloc d'I/O (anime la barre) */

signed char save_state(u16 section, scr_progress_cb cb)
{
    FILE *f;
    u8 buf[STATE_MAX];
    u8 n = 0, i, off, c;

    f = fopen(SAVE_NAME, "wb");
    if (f == NULL)
        return -1;

    buf[n++] = (u8)(section & 0xFF);
    buf[n++] = (u8)(section >> 8);
    for (i = 0; i < g_nstats; ++i)
        buf[n++] = stat_val[i];
    for (i = 0; i < g_nstats; ++i)
        buf[n++] = stat_max[i];         /* max courant (peut avoir change) */
    for (i = 0; i < (g_nitems + 7) / 8; ++i)
        buf[n++] = item_bits[i];
    for (i = 0; i < (g_nflags + 7) / 8; ++i)
        buf[n++] = flag_bits[i];
    buf[n++] = (u8)(g_score & 0xFF);
    buf[n++] = (u8)(g_score >> 8);
    buf[n++] = (u8)(g_moves & 0xFF);
    buf[n++] = (u8)(g_moves >> 8);

    for (off = 0; off < n; off += c) {
        c = (u8)(n - off < STATE_STEP ? n - off : STATE_STEP);
        if (fwrite(buf + off, 1, c, f) != c) {
            fclose(f);
            return -1;
        }
        if (cb != NULL)
            cb((u16)(off + c), n);
    }
    fclose(f);
    return 0;
}

signed char load_state(u16 *out_section, scr_progress_cb cb)
{
    FILE *f;
    u8 buf[STATE_MAX];
    u8 n, i, k, off, c;

    f = fopen(SAVE_NAME, "rb");
    if (f == NULL)
        return -1;

    n = state_len();
    for (off = 0; off < n; off += c) {
        c = (u8)(n - off < STATE_STEP ? n - off : STATE_STEP);
        if (fread(buf + off, 1, c, f) != c) {
            fclose(f);
            return -1;
        }
        if (cb != NULL)
            cb((u16)(off + c), n);
    }
    fclose(f);

    *out_section = (u16)(buf[0] | ((u16)buf[1] << 8));
    k = 2;
    for (i = 0; i < g_nstats; ++i)
        stat_val[i] = buf[k++];
    for (i = 0; i < g_nstats; ++i)
        stat_max[i] = buf[k++];
    for (i = 0; i < (g_nitems + 7) / 8; ++i)
        item_bits[i] = buf[k++];
    for (i = 0; i < (g_nflags + 7) / 8; ++i)
        flag_bits[i] = buf[k++];
    g_score = (u16)(buf[k] | ((u16)buf[k + 1] << 8)); k = (u8)(k + 2);
    g_moves = (u16)(buf[k] | ((u16)buf[k + 1] << 8)); k = (u8)(k + 2);
    return 0;
}
