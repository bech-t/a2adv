/* ramdisk.c -- cache des STORYnn.DAT dans /RAM (cf. spec §7quater).
 *
 * Politique (mono-disquette) :
 *   - au boot, AVANT story_open() : remplissage maximal vers l'avant, barre de
 *     progression, seuls 2 tampons ProDOS necessaires ;
 *   - en jeu : au plus UN fichier a la fois (celui qu'on entre), avec eviction
 *     du fichier cache le plus eloigne du courant.
 *
 * Tout est en meilleur effort : /RAM absent, volume plein ou tampon ProDOS
 * indisponible -> le fichier reste sur la disquette, sans erreur visible.
 */

#include <stdio.h>
#include "ramdisk.h"
#include "story.h"

#define RAM_MAX_FILES 100                        /* = MAX_FILES du format */
#define COPY_CHUNK    SECTION_MAX                /* 1 Ko : on reutilise secbuf */

/* Bitset "le fichier n est en /RAM" (13 o). */
static u8 cached[(RAM_MAX_FILES + 7) / 8];
#define BIT_SET(i) (cached[(i) >> 3] |= (u8)(1u << ((i) & 7)))
#define BIT_CLR(i) (cached[(i) >> 3] &= (u8)~(1u << ((i) & 7)))
#define BIT_TST(i) ((cached[(i) >> 3] >> ((i) & 7)) & 1u)

/* Chemins : deux tampons distincts (les deux servent dans le meme appel). */
static char dpath[] = "STORY00.DAT";             /* chiffres en 5,6   */
static char rpath[] = "/RAM/STORY00.DAT";        /* chiffres en 10,11 */

static const char *disk_path(u8 id)
{
    dpath[5] = (char)('0' + id / 10);
    dpath[6] = (char)('0' + id % 10);
    return dpath;
}

const char *ram_path(u8 id)
{
    rpath[10] = (char)('0' + id / 10);
    rpath[11] = (char)('0' + id % 10);
    return rpath;
}

u8 ram_has(u8 id)
{
    return (id < RAM_MAX_FILES) ? (u8)BIT_TST(id) : 0;
}

/* --- Disponibilite de /RAM --------------------------------------------- */

#ifdef __CC65__
/* 0 = pas encore teste, 1 = present, 2 = absent. */
static u8 ram_state;
#endif

static u8 ram_ready(void)
{
#ifdef __CC65__
    FILE *f;
    if (ram_state == 0) {
        f = fopen("/RAM/A2ADV.TMP", "wb");       /* seul test fiable : ecrire */
        if (f != NULL) {
            fclose(f);
            remove("/RAM/A2ADV.TMP");
            ram_state = 1;
        } else {
            ram_state = 2;
        }
    }
    return (u8)(ram_state == 1);
#else
    return 0;                                    /* hote : pas de /RAM */
#endif
}

/* --- Copie -------------------------------------------------------------- */

/* Taille du STORYnn.DAT sur disquette, 0 si absent/illisible. */
static u32 file_size(u8 id)
{
    FILE *f;
    long n;

    f = fopen(disk_path(id), "rb");
    if (f == NULL)
        return 0;
    if (fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    n = ftell(f);
    fclose(f);
    return (n > 0) ? (u32)n : 0;
}

/* Copie disquette -> /RAM. 0 = ok (et bit pose), -1 = source absente,
 * -2 = ecriture impossible (/RAM plein ou pas de tampon).
 * 'done'/'total' sont en octets ; la barre les recoit divises par 256. */
static signed char copy_one(u8 id, scr_progress_cb cb, u32 *done, u32 total)
{
    FILE *src, *dst;
    u16 n;

    src = fopen(disk_path(id), "rb");
    if (src == NULL)
        return -1;
    dst = fopen(ram_path(id), "wb");
    if (dst == NULL) {
        fclose(src);
        return -2;
    }

    for (;;) {
        n = (u16)fread(secbuf, 1, COPY_CHUNK, src);
        if (n == 0)
            break;
        if (fwrite(secbuf, 1, n, dst) != n) {    /* /RAM plein en cours de route */
            fclose(dst);
            fclose(src);
            remove(ram_path(id));                /* ne jamais laisser un tronque */
            return -2;
        }
        *done += n;
        if (cb != NULL)
            cb((u16)(*done >> 8), (u16)(total >> 8));
    }

    fclose(dst);
    fclose(src);
    BIT_SET(id);
    return 0;
}

/* --- Eviction ----------------------------------------------------------- */

/* Libere le fichier cache le plus eloigne de 'from' (a distance egale, celui
 * qui est DERRIERE : on revient plus souvent en arriere qu'on ne saute loin
 * devant). Ne touche jamais a 'from'. 1 = un fichier libere, 0 = plus rien. */
static u8 evict_farthest(u8 from)
{
    u8 i, best = 0xFF, bestd = 0, d;

    for (i = 0; i < RAM_MAX_FILES; ++i) {
        if (i == from || !BIT_TST(i))
            continue;
        d = (u8)(i > from ? i - from : from - i);
        if (d > bestd || (d == bestd && i < from)) {
            bestd = d;
            best = i;
        }
    }
    if (best == 0xFF)
        return 0;

    BIT_CLR(best);                    /* le bit tombe AVANT le remove : si le  */
    remove(ram_path(best));           /* remove echoue, on relit la disquette. */
    return 1;
}

/* --- API ---------------------------------------------------------------- */

void ram_boot_fill(u8 from, scr_progress_cb cb)
{
    u8  i;
    u32 total = 0, done = 0;

    if (!ram_ready())
        return;

    /* Passe 1 : compte les fichiers et cumule leur taille (echelle de la barre).
     * Le premier fopen qui echoue marque la fin de la serie STORYnn. */
    for (i = from; i < RAM_MAX_FILES; ++i) {
        u32 s = file_size(i);
        if (s == 0)
            break;
        total += s;
    }
    if (total == 0)
        return;

    /* Passe 2 : copie tant que /RAM accepte (-2 = plein, -1 = fin de serie). */
    for (i = from; i < RAM_MAX_FILES; ++i)
        if (copy_one(i, cb, &done, total) != 0)
            break;
}

void ram_ensure(u8 id)
{
    u32 total, done = 0;

    if (id >= RAM_MAX_FILES || !ram_ready() || BIT_TST(id))
        return;

    total = file_size(id);
    if (total == 0)
        return;                       /* pas sur la disquette : rien a faire */

    /* Plein : on libere le plus eloigne et on reessaie, jusqu'a n'avoir plus
     * rien a evincer (on reste alors sur la disquette). */
    while (copy_one(id, NULL, &done, total) == -2) {
        if (!evict_farthest(id))
            return;
        done = 0;
    }
}
