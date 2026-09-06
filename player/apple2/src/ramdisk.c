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

/* Bitset "le fichier n est cache" (13 o). */
static u8 cached[(RAM_MAX_FILES + 7) / 8];
#define BIT_SET(i) (cached[(i) >> 3] |= (u8)(1u << ((i) & 7)))
#define BIT_CLR(i) (cached[(i) >> 3] &= (u8)~(1u << ((i) & 7)))
#define BIT_TST(i) ((cached[(i) >> 3] >> ((i) & 7)) & 1u)

/* Second bitset : "et il est sur /RAM2 plutot que /RAM" (13 o). Deux bitsets
 * plutot qu'un octet de volume par fichier : 26 octets au lieu de 100, sur une
 * machine ou il en reste moins de deux mille. */
static u8 on_vol2[(RAM_MAX_FILES + 7) / 8];
#define V2_SET(i) (on_vol2[(i) >> 3] |= (u8)(1u << ((i) & 7)))
#define V2_CLR(i) (on_vol2[(i) >> 3] &= (u8)~(1u << ((i) & 7)))
#define V2_TST(i) ((on_vol2[(i) >> 3] >> ((i) & 7)) & 1u)

#define NVOL 2

/* Deux tampons de chemin distincts : les deux servent dans le meme appel
 * (source sur la disquette, destination en disque RAM). */
static char dpath[] = "STORY00.DAT";             /* chiffres en 5,6 */
static char gpath[6 + 14];                       /* "/RAM2/" + nom + NUL */

/* "/RAM/<nom>" ou "/RAM2/<nom>". Sert a TOUT ce qu'on cache, STORYnn.DAT
 * comme images : un seul constructeur, donc une seule facon de se tromper. */
static const char *gen_path(u8 vol, const char *name)
{
    char *p = gpath;
    *p++ = '/'; *p++ = 'R'; *p++ = 'A'; *p++ = 'M';
    if (vol) *p++ = '2';
    *p++ = '/';
    while (*name != '\0' && p < gpath + sizeof(gpath) - 1)
        *p++ = *name++;
    *p = '\0';
    return gpath;
}

static const char *disk_path(u8 id)
{
    dpath[5] = (char)('0' + id / 10);
    dpath[6] = (char)('0' + id % 10);
    return dpath;
}

const char *ram_path(u8 id)
{
    return gen_path((u8)V2_TST(id), disk_path(id));
}

u8 ram_has(u8 id)
{
    return (id < RAM_MAX_FILES) ? (u8)BIT_TST(id) : 0;
}

/* --- Disponibilite de /RAM --------------------------------------------- */

#ifdef __CC65__
/* Par volume : 0 = pas encore teste, 1 = present, 2 = absent. */
static u8 vol_state[NVOL];
#endif

/* Le volume existe-t-il et accepte-t-il l'ecriture ? On ne se fie pas a un
 * catalogue : la seule reponse fiable est d'y ecrire pour de bon. */
static u8 vol_ready(u8 vol)
{
#ifdef __CC65__
    FILE *f;
    const char *probe = vol ? "/RAM2/A2ADV.TMP" : "/RAM/A2ADV.TMP";
    if (vol_state[vol] == 0) {
        f = fopen(probe, "wb");
        if (f != NULL) {
            fclose(f);
            remove(probe);
            vol_state[vol] = 1;
        } else {
            vol_state[vol] = 2;
        }
    }
    return (u8)(vol_state[vol] == 1);
#else
    (void)vol;
    return 0;                                    /* hote : pas de disque RAM */
#endif
}

/* Au moins un volume utilisable ? */
static u8 ram_ready(void)
{
    return (u8)(vol_ready(0) || vol_ready(1));
}

/* --- Copie -------------------------------------------------------------- */

/* Taille d'un fichier de la disquette, 0 si absent/illisible. */
static u32 named_size(const char *name)
{
    FILE *f;
    long n;

    f = fopen(name, "rb");
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

static u32 file_size(u8 id) { return named_size(disk_path(id)); }

/* Copie disquette -> volume 'vol'. 0 = ok (bits poses), -1 = source absente,
 * -2 = ecriture impossible (volume plein ou pas de tampon).
 * 'done'/'total' sont en octets ; la barre les recoit divises par 256. */
/* Coeur de copie : disquette -> chemin de destination. 0 = ok, -1 = source
 * absente, -2 = ecriture impossible (volume plein ou pas de tampon).
 * 'done'/'total' sont en octets ; la barre les recoit divises par 256. */
static signed char copy_stream(const char *src_name, const char *dst_path,
                               scr_progress_cb cb, u32 *done, u32 total)
{
    FILE *src, *dst;
    u16 n;

    src = fopen(src_name, "rb");
    if (src == NULL)
        return -1;
    dst = fopen(dst_path, "wb");
    if (dst == NULL) {
        fclose(src);
        return -2;
    }

    for (;;) {
        n = (u16)fread(secbuf, 1, COPY_CHUNK, src);
        if (n == 0)
            break;
        if (fwrite(secbuf, 1, n, dst) != n) {    /* volume plein en cours de route */
            fclose(dst);
            fclose(src);
            remove(dst_path);                    /* ne jamais laisser un tronque */
            return -2;
        }
        *done += n;
        if (cb != NULL)
            cb((u16)(*done >> 8), (u16)(total >> 8));
    }

    fclose(dst);
    fclose(src);
    return 0;
}

/* Copie un fichier nomme sur le PREMIER volume qui l'accepte. Renvoie le
 * volume utilise dans *used. Comme copy_stream sinon. */
static signed char copy_named_best(const char *name, u8 *used,
                                   scr_progress_cb cb, u32 *done, u32 total)
{
    u8 vol;
    signed char r = -2;

    for (vol = 0; vol < NVOL; ++vol) {
        if (!vol_ready(vol))
            continue;
        r = copy_stream(name, gen_path(vol, name), cb, done, total);
        if (r != -2) {
            *used = vol;
            return r;
        }
    }
    return r;
}

/* Un STORYnn.DAT vers le premier volume qui l'accepte, bits de cache poses. */
static signed char copy_best(u8 id, scr_progress_cb cb, u32 *done, u32 total)
{
    u8 vol = 0;
    signed char r = copy_named_best(disk_path(id), &vol, cb, done, total);

    if (r == 0) {
        if (vol) V2_SET(id); else V2_CLR(id);    /* AVANT BIT_SET : ram_path lit V2 */
        BIT_SET(id);
    }
    return r;
}

/* --- Fichiers quelconques (images) -------------------------------------- */

const char *ram_file_path(const char *name)
{
    u8 vol;
    FILE *f;

    for (vol = 0; vol < NVOL; ++vol) {
        if (!vol_ready(vol))
            continue;
        f = fopen(gen_path(vol, name), "rb");    /* pas de table a tenir a jour : */
        if (f != NULL) {                         /* l'existence du fichier EST    */
            fclose(f);                           /* l'etat du cache.              */
            return gpath;
        }
    }
    return name;
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

    /* Le chemin est calcule TANT QUE les bits tiennent : ram_path lit V2 pour
     * choisir le volume, et l'effacer d'abord nous ferait supprimer le mauvais
     * fichier. Les bits tombent ensuite, AVANT le remove : si celui-ci echoue,
     * on se contente de relire la disquette. */
    {
        const char *victim = ram_path(best);
        BIT_CLR(best);
        V2_CLR(best);
        remove(victim);
    }
    return 1;
}

/* --- API ---------------------------------------------------------------- */

/* Fichiers preccharges APRES les STORYnn.DAT : le menu, puis les images du
 * jeu. Les splashes BOOTnn.HGR en sont volontairement absents -- on ne les voit
 * qu'une fois, au demarrage ; leur place sert mieux aux images du jeu, qui
 * reviennent. Renvoie NULL au-dela de la serie. */
static const char *extra_name(u8 i)
{
    static char n[] = "IMG00.HGR";

    if (i == 0)
        return "MENU.HGR";
    --i;
    if (i >= 100)
        return 0;
    n[3] = (char)('0' + i / 10);
    n[4] = (char)('0' + i % 10);
    return n;
}

void ram_boot_fill(u8 from, scr_progress_cb cb)
{
    u8  i, vol = 0;
    u32 total = 0, done = 0;

    if (!ram_ready())
        return;

    /* Passe 1 : cumule les tailles pour l'echelle de la barre. Le premier
     * fopen qui echoue marque la fin de chaque serie. */
    for (i = from; i < RAM_MAX_FILES; ++i) {
        u32 sz = file_size(i);
        if (sz == 0)
            break;
        total += sz;
    }
    if (total == 0)
        return;
    for (i = 0; ; ++i) {
        const char *nm = extra_name(i);
        u32 sz;
        if (nm == 0)
            break;
        sz = named_size(nm);
        if (sz == 0) {
            if (i == 0)
                continue;            /* pas de MENU.HGR : les images restent */
            break;
        }
        total += sz;
    }

    /* Passe 2 : l'HISTOIRE D'ABORD -- les STORYnn.DAT sont relus a chaque
     * section, ils meritent la place avant tout le reste. -2 = plus de place
     * nulle part, -1 = fin de serie : on s'arrete, le reste demeure sur la
     * disquette. */
    for (i = from; i < RAM_MAX_FILES; ++i)
        if (copy_best(i, cb, &done, total) != 0)
            break;

    /* Puis les images, tant qu'il reste de la place. Les precharger ici plutot
     * qu'au premier affichage rend meme la PREMIERE vue instantanee, et le
     * joueur attend deja devant cette barre. */
    for (i = 0; ; ++i) {
        const char *nm = extra_name(i);
        if (nm == 0)
            break;
        if (copy_named_best(nm, &vol, cb, &done, total) == -2)
            break;                   /* plus de place : inutile d'insister */
    }
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
    while (copy_best(id, NULL, &done, total) == -2) {
        if (!evict_farthest(id))
            return;
        done = 0;
    }
}
