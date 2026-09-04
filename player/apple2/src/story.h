/* story.h -- ouverture de STORY.DAT, données résidentes, streaming des sections.
 *
 * Le header, le préambule et l'accès à l'index restent résidents ; le corps
 * d'une section est lu à la demande dans secbuf[] (cf. spec §7bis, §7ter).
 */
#ifndef A2ADV_STORY_H
#define A2ADV_STORY_H

#include "format.h"

/* --- Données résidentes (remplies par story_open) --------------------- */
extern u8  g_nstats, g_nitems, g_nflags;
extern u16 g_nsections;
extern u16 g_start;
extern u8  g_score_on, g_moves_on;    /* compteurs actifs ? (flags d'entete) */
extern u8  g_nfiles;                  /* nombre de fichiers STORYn.DAT */
/* 1er index de flag LOCAL : les flags [g_local_base, g_nflags) sont remis a 0
 * a chaque changement de chapitre (cf. spec §6.1, flags locaux). */
extern u8  g_local_base;
/* Masque des stats MASQUEES : bit i = la stat i ne figure pas au bandeau
 * d'etat (@stat ... hidden). Elle reste lisible par les conditions et
 * sauvegardee : c'est un compteur que l'auteur fait sentir, pas afficher. */
extern u8  g_stat_hidden;
extern u8  g_story_version;   /* version du STORY.DAT lu (pour le diagnostic) */
extern u8  g_combat_att;              /* index stat d'attaque du héros (0xFF=aucun) */
extern u8  g_combat_hp;               /* index stat de PV du héros (0xFF=aucun) */
extern u8  g_combat_basedmg;          /* dégâts du héros à mains nues */

extern u8   g_nintro;                 /* nombre de scenes d'intro */
extern u16  intro_idx[MAX_INTRO];     /* index de section de chaque scene */
#ifdef __CC65__
#define g_title ((char *)0x1478)          /* titre : 33 o : $1478-$1498 */
#else
extern char g_title[TITLE_LEN];       /* titre de l'aventure */
#endif
/* Chaines d'UI : en RAM basse sur Apple II (soulage le BSS). */
#ifdef __CC65__
#define ui_str ((char (*)[UI_STR_LEN])0x1700)   /* 46*36 = 1656 o : $1700-$1D77 */
#else
extern char ui_str[UI_COUNT][UI_STR_LEN];  /* chaines d'UI (langue de l'aventure) */
#endif

extern u8  stat_init[MAX_STATS];
extern u8  stat_min[MAX_STATS];
extern u8  stat_max[MAX_STATS];     /* max COURANT (mutable via ~ setmax, sauvegardé) */
extern u8  stat_maxdef[MAX_STATS];  /* max par défaut (préambule ; sert à state_init) */
/* Noms de stats : en RAM basse sur Apple II (soulage le BSS). */
#ifdef __CC65__
#define stat_name ((char (*)[STAT_NAME_LEN])0x13D0)   /* 8*12 = 96 o : $13D0-$142F */
#else
extern char stat_name[MAX_STATS][STAT_NAME_LEN];
#endif
/* Libelles d'objets : en RAM basse sur Apple II (soulage le BSS). */
#ifdef __CC65__
#define item_label ((char (*)[ITEM_LABEL_LEN])0x1500)   /* 24*20 = 480 o : $1500-$16DF */
#else
extern char item_label[MAX_ITEMS][ITEM_LABEL_LEN];
#endif

/* Modificateurs de combat par objet (signés) : en RAM basse (soulage le BSS). */
#ifdef __CC65__
#define item_atk   ((signed char *)0x1430)   /* 24 o : $1430-$1447 */
#define item_dmg   ((signed char *)0x1448)   /* 24 o : $1448-$145F */
#define item_armor ((signed char *)0x1460)   /* 24 o : $1460-$1477 */
#else
extern signed char item_atk[MAX_ITEMS];
extern signed char item_dmg[MAX_ITEMS];
extern signed char item_armor[MAX_ITEMS];
#endif

/* Bitsets d'état par défaut (issus du préambule) */
extern u8 item_default[(MAX_ITEMS + 7) / 8];
extern u8 flag_default[(MAX_FLAGS + 7) / 8];

/* Corps de la section couramment chargée.
 * Sur Apple II, place dans la RAM libre sous le HIRES ($0C00) pour soulager le
 * BSS (zone $80xx pinee sous __HIMEM__). Sur l'hote (gcc), vrai tableau. */
#ifdef __CC65__
#define secbuf ((u8 *)0x0C00)        /* SECTION_MAX (1024) o : $0C00-$0FFF */
#else
extern u8  secbuf[SECTION_MAX];
#endif
extern u16 seclen;

/* --- API -------------------------------------------------------------- */

/* Ouvre STORY00.DAT (au boot) : header + préambule + file_first + index local
 * du fichier 0. STORY reste ensuite ouvert en permanence. 0 = ok, <0 = erreur. */
signed char story_open(const char *path);
void story_close(void);   /* fermeture (uniquement a la sortie du programme) */

/* Charge le corps de la section idx dans secbuf[] (seek via l'index). */
signed char story_load_section(u16 idx);

/* --- Curseur de lecture dans secbuf[] --------------------------------- */
void b_reset(void);
u8   b_u8(void);
u16  b_u16(void);
u16  b_tell(void);
void b_seek(u16 pos);
/* Lit une chaîne (u8 len + octets) : renvoie le pointeur, *len = longueur. */
const char *b_str(u8 *len);

#endif /* A2ADV_STORY_H */
