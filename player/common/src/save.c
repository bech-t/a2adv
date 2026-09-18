/* save.c -- serialisation de l'etat joueur vers un fichier ProDOS. */

#include <stdio.h>
#include "save.h"
#include "story.h"
#include "state.h"

#define SAVE_NAME  "SAVE0.DAT"
/* bloc : section(2) + stat_val[n]*2 + stat_max[n]*2 + items + flags + score(2) + moves(2)
 * (stat_val/stat_max sont u16 : 2 octets chacun) */
#define STATE_MAX  (2 + 4 * MAX_STATS + (MAX_ITEMS + 7) / 8 + (MAX_FLAGS + 7) / 8 + 4)

static u8 state_len(void)
{
    return (u8)(2 + 4 * g_nstats + (g_nitems + 7) / 8 + (g_nflags + 7) / 8 + 4);
}

#define STATE_STEP  4    /* octets par bloc d'I/O (anime la barre) */

/* save_state/load_state ne s'executent jamais en meme temps : un seul
 * tampon statique partage, plutot qu'un par fonction (sous -Cl, une locale
 * "buf[STATE_MAX]" est de toute facon en BSS -- ne pas la dupliquer). */
static u8 g_state_buf[STATE_MAX];

/* petit/grand octet groupes ici (au lieu d'etre repetes a chaque champ 16
 * bits) : le code duplique 5 fois (section, stat_val[], stat_max[], score,
 * moves) coutait plus cher en code que le JSR/RTS d'un appel partage. */
static void put16(u8 *buf, u8 *n, u16 v)
{
    buf[(*n)++] = (u8)(v & 0xFF);
    buf[(*n)++] = (u8)(v >> 8);
}

signed char save_state(u16 section, scr_progress_cb cb)
{
    FILE *f;
    u8 n = 0, i, off, c;

    f = fopen(SAVE_NAME, "wb");
    if (f == NULL)
        return -1;

    put16(g_state_buf, &n, section);
    for (i = 0; i < g_nstats; ++i)
        put16(g_state_buf, &n, stat_val[i]);
    for (i = 0; i < g_nstats; ++i)       /* max courant (peut avoir change) */
        put16(g_state_buf, &n, stat_max[i]);
    for (i = 0; i < (g_nitems + 7) / 8; ++i)
        g_state_buf[n++] = item_bits[i];
    for (i = 0; i < (g_nflags + 7) / 8; ++i)
        g_state_buf[n++] = flag_bits[i];
    put16(g_state_buf, &n, g_score);
    put16(g_state_buf, &n, g_moves);

    for (off = 0; off < n; off += c) {
        c = (u8)(n - off < STATE_STEP ? n - off : STATE_STEP);
        if (fwrite(g_state_buf + off, 1, c, f) != c) {
            fclose(f);
            return -1;
        }
        if (cb != NULL)
            cb((u16)(off + c), n);
    }
    fclose(f);
    return 0;
}

static u16 get16(const u8 *buf, u8 *k)
{
    u16 v = (u16)(buf[*k] | ((u16)buf[*k + 1] << 8));
    *k = (u8)(*k + 2);
    return v;
}

signed char load_state(u16 *out_section, scr_progress_cb cb)
{
    FILE *f;
    u8 n, i, k, off, c;

    f = fopen(SAVE_NAME, "rb");
    if (f == NULL)
        return -1;

    n = state_len();
    for (off = 0; off < n; off += c) {
        c = (u8)(n - off < STATE_STEP ? n - off : STATE_STEP);
        if (fread(g_state_buf + off, 1, c, f) != c) {
            fclose(f);
            return -1;
        }
        if (cb != NULL)
            cb((u16)(off + c), n);
    }
    fclose(f);

    k = 0;
    *out_section = get16(g_state_buf, &k);
    for (i = 0; i < g_nstats; ++i)
        stat_val[i] = get16(g_state_buf, &k);
    for (i = 0; i < g_nstats; ++i)
        stat_max[i] = get16(g_state_buf, &k);
    for (i = 0; i < (g_nitems + 7) / 8; ++i)
        item_bits[i] = g_state_buf[k++];
    for (i = 0; i < (g_nflags + 7) / 8; ++i)
        flag_bits[i] = g_state_buf[k++];
    g_score = get16(g_state_buf, &k);
    g_moves = get16(g_state_buf, &k);
    return 0;
}
