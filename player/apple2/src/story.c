/* story.c -- chargement de STORY.DAT et streaming des sections. */

#include <stdio.h>
#include <string.h>
#include "story.h"
#include "state.h"
#include "ramdisk.h"

u8  g_nstats, g_nitems, g_nflags;
u16 g_nsections;
u16 g_start;
u8  g_score_on, g_moves_on;
u8  g_nfiles;
u8  g_local_base;
u8  g_stat_hidden;
u8  g_story_version;     /* version lue dans l'en-tete (diagnostic si refus) */
u8  g_combat_att, g_combat_hp, g_combat_basedmg;

u8   g_nintro;
u16  intro_idx[MAX_INTRO];
#ifndef __CC65__
char g_title[TITLE_LEN];
char ui_str[UI_COUNT][UI_STR_LEN];   /* hote : vrais tableaux (cf. story.h) */
#endif

u8  stat_init[MAX_STATS];
u8  stat_min[MAX_STATS];
u8  stat_max[MAX_STATS];
u8  stat_maxdef[MAX_STATS];
#ifndef __CC65__
char stat_name[MAX_STATS][STAT_NAME_LEN];     /* hote : vrais tableaux (cf. story.h) */
char item_label[MAX_ITEMS][ITEM_LABEL_LEN];
signed char item_atk[MAX_ITEMS];
signed char item_dmg[MAX_ITEMS];
signed char item_armor[MAX_ITEMS];
#endif

u8 item_default[(MAX_ITEMS + 7) / 8];
u8 flag_default[(MAX_FLAGS + 7) / 8];

#ifndef __CC65__
u8  secbuf[SECTION_MAX];              /* hote : vrai tableau (cf. story.h) */
#endif
u16 seclen;

static FILE *fp;
static u32   index_offset;   /* -> table file_first dans STORY00.DAT */
static u32   local0_off;     /* -> index local de STORY00 (apres file_first) */
static u16   cpos;           /* curseur dans secbuf[] */
static u16   cur_count;      /* nb de sections de l'index local courant */

/* Multi-fichiers : chemin de STORY00.DAT avec les 2 chiffres de fichier remplacables. */
static char g_path[64];
static u8   g_digit;     /* position du chiffre "n" (avant ".DAT") dans g_path */
static u8   cur_file;    /* fichier STORYn actuellement ouvert (0xFF = aucun) */

/* Index PAR FICHIER (format v4). Resident :
 *   - file_first[n_files+1] : 1re section globale de chaque fichier (+ sentinelle) ;
 *   - local_off[]           : offsets internes des corps du FICHIER COURANT
 *                             (recharge quand on change de STORYnn.DAT).
 * Ne garde donc en RAM que l'index du fichier ouvert -> passe a l'echelle.
 *
 * Carte RAM basse ($0C00-$1FFF), remaniee quand ui_str est passe de 28 a 46
 * chaines (1008 -> 1656 o) : a son ancienne adresse $1A00 il aurait recouvert
 * la page HIRES a $2000. Cette zone est de la RAM SOUS le programme (qui
 * demarre a $4000) : elle ne coute rien au budget code.
 *   secbuf     $0C00 (1024)  -> $0C00-$0FFF
 *   local_off  $1000 ( 514)  -> $1000-$1201
 *   file_first $1300 ( 202)  -> $1300-$13C9
 *   stat_name  $13D0 (  96)  -> $13D0-$142F
 *   item_atk   $1430 (  24)  -> $1430-$1447
 *   item_dmg   $1448 (  24)  -> $1448-$145F
 *   item_armor $1460 (  24)  -> $1460-$1477
 *   g_title    $1478 (  33)  -> $1478-$1498
 *   [libre $1499-$14FF]
 *   item_label $1500 ( 480)  -> $1500-$16DF
 *   ui_str     $1700 (1656)  -> $1700-$1D77
 *   [libre $1D78-$1FFF, 648 o]
 *
 * item_atk/item_dmg/item_armor sont arrives apres coup et avaient pris
 * $1400/$1418/$1430 SANS relire cette carte : ils recouvraient stat_name (des
 * 5 stats declarees) et g_title en entier. Tout ajout ici DOIT verifier les
 * plages ci-dessus avant de choisir une adresse.                            */
#define LOCAL_IDX_MAX 256    /* sections max par fichier (index local resident) */
#define FILE_FIRST_MAX 100   /* = MAX_FILES du format */
#ifdef __CC65__
#define local_off  ((u16 *)0x1000)   /* (256+1)*2 = 514 o : $1000-$1201 */
#define file_first ((u16 *)0x1300)   /* (100+1)*2 = 202 o : $1300-$13C9 */
#else
static u16 local_off[LOCAL_IDX_MAX + 1];
static u16 file_first[FILE_FIRST_MAX + 1];
#endif

/* --- Petites lectures depuis le fichier -------------------------------- */

/* Mise en tampon.
 *
 * ATTENTION a ne pas se tromper de gain : ProDOS met DEJA en cache le bloc
 * courant de 512 o dans le tampon de 1 Ko de chaque fichier ouvert. Le manuel
 * technique est explicite : "Neither a READ nor a WRITE call necessarily
 * causes a disk access. It is only when a read or write crosses a 512-byte
 * (block) boundary that a disk access occurs." Lire le preambule octet par
 * octet ne coute donc PAS des centaines d'acces disque : il y en a autant dans
 * les deux cas (un par bloc traverse).
 *
 * Ce qu'on economise, c'est le SURCOUT D'APPEL : la stdio de cc65 n'etant pas
 * tamponnee, chaque fgetc() descend en read() puis en appel MLI (bloc de
 * parametres, JSR $BF00, dispatch, commutation de banque) meme quand la donnee
 * sort du cache ProDOS. ~900 appels au demarrage, a quelques centaines de
 * cycles piece, valent de l'ordre de 0,2 a 0,4 s a 1 MHz.
 *
 * On charge donc d'un bloc dans secbuf — libre a ces moments-la — et f_u8() y
 * puise tant qu'il reste quelque chose. Repli automatique sur la lecture
 * directe si le bloc ne tient pas dans secbuf. */
static u16 pbuf_pos;
static u16 pbuf_left;

/* Charge n octets a la position courante du fichier dans secbuf. 0 = ok (les
 * lectures suivantes viennent du tampon), -1 = trop gros ou lecture courte
 * (on reste en lecture directe, plus lente mais correcte). */
static signed char buf_fill(u16 n)
{
    pbuf_pos = 0;
    pbuf_left = 0;
    if (n == 0 || n > SECTION_MAX)
        return -1;
    if (fread(secbuf, 1, n, fp) != n)
        return -1;
    pbuf_left = n;
    return 0;
}

static u8 f_u8(void)
{
    if (pbuf_left) {
        --pbuf_left;
        return secbuf[pbuf_pos++];
    }
    return (u8)fgetc(fp);
}

static u16 f_u16(void)
{
    u16 lo = f_u8();
    return lo | ((u16)f_u8() << 8);
}

static u32 f_u32(void)
{
    u32 v = f_u16();
    return v | ((u32)f_u16() << 16);
}

/* Lit une chaîne préfixée (u8 len + octets) vers dst (tronquée à cap-1). */
static void f_lenstr(char *dst, u8 cap)
{
    u8 len = f_u8();
    u8 i, keep;
    keep = (len < cap - 1) ? len : (u8)(cap - 1);
    for (i = 0; i < len; ++i) {
        u8 c = f_u8();
        if (i < keep)
            dst[i] = (char)c;
    }
    dst[keep] = '\0';
}

/* --- Gestion des fichiers STORYn.DAT ---------------------------------- */

/* Memorise le chemin de base (STORY00.DAT) et repere le 1er des 2 chiffres de
 * fichier, situes juste avant ".DAT" (positions len-6 et len-5). */
static void set_path(const char *path)
{
    u8 i = 0;
    while (path[i] && i < (u8)(sizeof(g_path) - 1)) {
        g_path[i] = path[i];
        ++i;
    }
    g_path[i] = '\0';
    g_digit = (i >= 6) ? (u8)(i - 6) : 0;
}

/* Ouvre le fichier STORYnn.DAT (remplace les 2 chiffres dans g_path).
 * Cache /RAM d'abord (cf. spec §7quater.6), repli disquette sinon. */
static signed char open_file(u8 id)
{
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
    if (ram_has(id))
        fp = fopen(ram_path(id), "rb");
    if (fp == NULL) {                              /* repli : la disquette */
        g_path[g_digit]     = (char)('0' + id / 10);   /* 2 chiffres : 00..99 */
        g_path[g_digit + 1] = (char)('0' + id % 10);
        fp = fopen(g_path, "rb");
    }
    if (fp == NULL) {
        cur_file = 0xFF;
        return -1;
    }
    cur_file = id;
    return 0;
}

/* Fichier contenant la section globale idx (via file_first). */
static u8 file_of(u16 idx)
{
    u8 f;
    for (f = 0; f < g_nfiles; ++f)
        if (idx >= file_first[f] && idx < file_first[f + 1])
            return f;
    return 0;
}

/* Charge l'index LOCAL du fichier f (deja ouvert) : u16 count + (count+1) offsets.
 * STORY00 : l'index local est apres file_first (local0_off) ; les autres : a 0. */
static signed char load_local(u8 f)
{
    u16 k;
    if (fseek(fp, (f == 0) ? (long)local0_off : 0L, SEEK_SET) != 0)
        return -1;
    pbuf_left = 0;
    cur_count = f_u16();
    if (cur_count > LOCAL_IDX_MAX)
        return -2;
    /* (count+1) offsets d'un bloc. Gain modeste — 10 a 52 octets en pratique,
     * soit ~20 ms par changement de chapitre — mais secbuf est libre ici (le
     * corps de section n'est lu qu'apres) et ca ne coute que ces trois lignes. */
    (void)buf_fill((u16)((cur_count + 1) * 2));
    for (k = 0; k <= cur_count; ++k)
        local_off[k] = f_u16();
    pbuf_left = 0;
    return 0;
}

/* --- Ouverture / préambule -------------------------------------------- */

signed char story_open(const char *path)
{
    u8 i;
    char magic[4];

    set_path(path);
    if (open_file(0) != 0)
        return -1;

    /* En-tete : 20 octets en UNE lecture, puis on les consomme depuis secbuf. */
    if (buf_fill(HEADER_SIZE) != 0)
        return -2;
    for (i = 0; i < 4; ++i)
        magic[i] = (char)f_u8();
    if (magic[0] != 'A' || magic[1] != '2' ||
        magic[2] != 'A' || magic[3] != 'D')
        return -2;

    g_story_version = f_u8();
    if (g_story_version != STORY_FORMAT_VERSION)
        return -6;                /* format incompatible : ne PAS lire la suite */
    {
        u8 hf = f_u8();           /* flags   */
        g_score_on = (hf & HDR_SCORE) ? 1 : 0;
        g_moves_on = (hf & HDR_MOVES) ? 1 : 0;
    }
    g_nsections = f_u16();
    g_nstats = f_u8();
    g_nitems = f_u8();
    g_nflags = f_u8();
    g_nintro = f_u8();            /* nb de scenes d'intro */
    g_start = f_u16();
    index_offset = f_u32();
    g_nfiles = f_u8();            /* offset 18 : nombre de fichiers STORYn.DAT */
    g_local_base = f_u8();        /* offset 19 : 1er index de flag LOCAL */

    /* Preambule : longueur connue (index_offset le termine). Un seul fread
     * remplace ici plusieurs centaines d'appels MLI. Au-dela de secbuf, on
     * retombe sur la lecture directe — correcte, juste plus lente. */
    if (index_offset > HEADER_SIZE)
        (void)buf_fill((u16)(index_offset - HEADER_SIZE));

    if (g_nstats > MAX_STATS || g_nitems > MAX_ITEMS ||
        g_nflags > MAX_FLAGS || g_nintro > MAX_INTRO ||
        g_nfiles > FILE_FIRST_MAX)
        return -3;

    /* préambule : stat_table[init,min,max] ; le max lu est le DEFAUT (mutable
     * ensuite via ~ setmax) -> stat_maxdef ; state_init en fera stat_max. */
    for (i = 0; i < g_nstats; ++i) {
        stat_init[i]   = f_u8();
        stat_min[i]    = f_u8();
        stat_maxdef[i] = f_u8();
    }
    g_stat_hidden = f_u8();       /* v6 : bit i = stat i absente du bandeau */
    for (i = 0; i < (g_nitems + 7) / 8; ++i)
        item_default[i] = f_u8();
    for (i = 0; i < (g_nflags + 7) / 8; ++i)
        flag_default[i] = f_u8();
    for (i = 0; i < g_nstats; ++i)
        f_lenstr(stat_name[i], STAT_NAME_LEN);
    for (i = 0; i < g_nitems; ++i)
        f_lenstr(item_label[i], ITEM_LABEL_LEN);
    f_lenstr(g_title, TITLE_LEN);
    for (i = 0; i < g_nintro; ++i)
        intro_idx[i] = f_u16();

    /* SURCHARGES de chaines d'UI (v5) : couples (index de cle, texte), poses
     * par-dessus le socle deja charge depuis APP.LNG (cf. spec §6.1). Une
     * aventure qui ne surcharge rien n'a rien ici. */
    {
        u8 nover = f_u8();
        for (i = 0; i < nover; ++i) {
            u8 k = f_u8();
            if (k < UI_COUNT) {
                f_lenstr(ui_str[k], UI_STR_LEN);
            } else {
                u8 sl = f_u8();      /* cle inconnue : saute la chaine */
                while (sl--) (void)f_u8();
            }
        }
    }

    /* attributs de combat par objet (atk, dmg, armor) — signés */
    for (i = 0; i < g_nitems; ++i) {
        item_atk[i]   = (signed char)f_u8();
        item_dmg[i]   = (signed char)f_u8();
        item_armor[i] = (signed char)f_u8();
    }
    /* config de combat : stat d'attaque, stat de PV, dégâts de base */
    g_combat_att     = f_u8();
    g_combat_hp      = f_u8();
    g_combat_basedmg = f_u8();

    /* v4 : table file_first (resident) puis index local de STORY00 (courant) */
    {
        u16 k;
        if (fseek(fp, (long)index_offset, SEEK_SET) != 0)
            return -4;
        pbuf_left = 0;                /* le tampon du preambule est epuise */
        (void)buf_fill((u16)((g_nfiles + 1) * 2));   /* jusqu'a 202 octets */
        for (k = 0; k <= g_nfiles; ++k)
            file_first[k] = f_u16();
        pbuf_left = 0;
    }
    local0_off = index_offset + (u32)(g_nfiles + 1) * 2;
    if (load_local(0) != 0)
        return -5;
    cur_file = 0;

    return 0;
}

void story_close(void)
{
    if (fp != NULL) {
        fclose(fp);
        fp = NULL;
    }
    cur_file = 0xFF;
}

/* --- Chargement d'une section ----------------------------------------- */

signed char story_load_section(u16 idx)
{
    u8  f = file_of(idx);
    u16 slot, off;

    if (f != cur_file) {                 /* changement de fichier : recharge l'index local */
        if (fp != NULL) {                /* on ferme AVANT ram_ensure : la copie */
            fclose(fp);                  /* a besoin de 2 tampons ProDOS libres  */
            fp = NULL;
        }
        ram_ensure(f);                   /* fenetre glissante (cf. spec §7quater.4) */
        if (open_file(f) != 0)
            return -1;
        if (load_local(f) != 0)
            return -2;
        state_clear_locals();            /* 1 fichier = 1 chapitre : les flags   */
    }                                    /* LOCAUX ne survivent pas au passage.  */
    slot = (u16)(idx - file_first[f]);
    off = local_off[slot];
    seclen = (u16)(local_off[slot + 1] - off);
    if (seclen > SECTION_MAX)
        return -3;
    if (fseek(fp, (long)off, SEEK_SET) != 0)
        return -4;
    if (fread(secbuf, 1, seclen, fp) != seclen)
        return -5;

    cpos = 0;
    return 0;
}

/* --- Curseur secbuf[] -------------------------------------------------- */

void b_reset(void) { cpos = 0; }
u16  b_tell(void)  { return cpos; }
void b_seek(u16 p) { cpos = p; }

u8 b_u8(void)
{
    return secbuf[cpos++];
}

u16 b_u16(void)
{
    u16 v = secbuf[cpos] | ((u16)secbuf[cpos + 1] << 8);
    cpos += 2;
    return v;
}

const char *b_str(u8 *len)
{
    u8 n = secbuf[cpos++];
    const char *p = (const char *)&secbuf[cpos];
    cpos += n;
    *len = n;
    return p;
}
