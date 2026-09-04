/* format.h -- constantes du format STORY.DAT (cf. spec.md §7ter).
 * Doit rester synchronisé avec compiler/a2c/model.py.
 */
#ifndef A2ADV_FORMAT_H
#define A2ADV_FORMAT_H

#define HEADER_SIZE   20
#define NO_IMAGE      0xFFFF

/* Modes d'affichage */
#define MODE_FULL_TEXT   0
#define MODE_IMAGE_TEXT  1
#define MODE_FULL_IMAGE  2

/* Fins */
#define END_NONE      0
#define END_VICTOIRE  1
#define END_DEFAITE   2

/* Opcodes d'atomes de condition */
#define OP_FLAG_SET   0x01
#define OP_FLAG_CLR   0x02
#define OP_HAS_ITEM   0x03
#define OP_NO_ITEM    0x04
#define OP_STAT_CMP   0x05

/* Styles de paragraphe de texte (octet style, cf. compilateur model.py) */
#define STYLE_CENTER   0x01
#define STYLE_INVERSE  0x02

/* Octet-bascule inline (invisible) : inverse ON/OFF au fil du texte.
 * Produit par le compilateur a partir des marqueurs *...* du .adv. */
#define TXT_INV_TOGGLE 0x01

/* Comparateurs (STAT_CMP) */
#define CMP_EQ 0
#define CMP_NE 1
#define CMP_LT 2
#define CMP_LE 3
#define CMP_GT 4
#define CMP_GE 5

/* Opcodes d'effets */
#define OP_SET_FLAG   0x10
#define OP_CLR_FLAG   0x11
#define OP_TOG_FLAG   0x12
#define OP_GIVE_ITEM  0x13
#define OP_TAKE_ITEM  0x14
#define OP_STAT_ADD   0x15
#define OP_STAT_SUB   0x16
#define OP_STAT_SET   0x17
#define OP_GOTO       0x18
#define OP_SOUND      0x19
#define OP_SCORE_ADD  0x1A     /* ~ score N : ajoute N au compteur de points */
#define OP_STAT_MAX   0x1B     /* ~ restore STAT : valeur = max courant */
#define OP_STAT_SETMAX 0x1C    /* ~ setmax STAT N : fixe le max (borne la valeur) */

/* Bits du champ 'flags' de l'entete (offset 5). */
#define HDR_SCORE     0x01     /* compteur de points actif */
#define HDR_MOVES     0x02     /* compteur de mouvements actif */

/* Sons predefinis : ORDRE FIGE (doit correspondre a SOUND_NAMES du compilateur,
 * a2c/model.py). Declenches par l'UI et par l'effet DSL `~ sound <nom>`. */
enum {
    SND_SELECT = 0, SND_ERROR, SND_WIN, SND_LOSE,
    SND_PICKUP, SND_HIT, SND_MAGIC, SND_DOOR, SND_PAGE,
    SND_COUNT
};

/* Limites de compilation (dimensionnent les tables résidentes en RAM).
 * Calibrees au plus juste pour economiser le BSS ; le compilateur refuse une
 * aventure qui depasse. Reels actuels : 17 objets, 4 stats, 17 flags, 3 intros. */
#define MAX_STATS     8
#define MAX_ITEMS     24     /* chaque objet = 1 bit de presence + 20 o de libelle */
#define MAX_FLAGS     128    /* flags purs = 1 bit chacun (bitset) -> ~32 o au total */
#define MAX_INTRO     8      /* scenes d'intro max */
#define TITLE_LEN     33     /* titre de l'aventure (pour le menu) */

/* Versions de format attendues. DOIVENT suivre a2c/encoder.py (VERSION,
 * LANG_VERSION). Le player REFUSE un fichier d'une autre version : la
 * disposition du preambule change d'une version a l'autre, et une lecture
 * decalee ne produit pas d'erreur, juste du charabia. */
#define STORY_FORMAT_VERSION 6
#define LANG_FORMAT_VERSION  1

/* Chaines d'interface : ORDRE FIGÉ (doit correspondre a UI_KEYS du compilateur,
 * a2c/model.py). Aucun texte de langue en dur dans le player. */
enum {
    UI_MENU_NEW = 0, UI_MENU_LOAD, UI_MENU_QUIT,
    UI_INV_HUD, UI_HINTS, UI_ANYKEY, UI_INTRO_HINT,
    UI_END_WIN, UI_END_LOSE, UI_END_GENERIC,
    UI_SAVED, UI_SAVE_FAIL, UI_NO_SAVE,
    UI_INVENTORY, UI_INV_EMPTY, UI_NO_EXIT, UI_SECTION_ERR,
    UI_QUIT_CONFIRM, UI_QUIT_SAVE, UI_QUIT_NOSAVE, UI_QUIT_CANCEL,
    UI_LOADING, UI_SAVING,
    UI_SCORE, UI_MOVES,
    UI_CBT_ATK, UI_CBT_DMG, UI_CBT_ARM,
    /* ecran de combat */
    UI_CB_HP, UI_CB_DICE, UI_CB_YOU, UI_CB_PARRY, UI_CB_ATTACK, UI_CB_FLEE,
    /* menu principal & options son */
    UI_MENU_OPTIONS, UI_OPT_TITLE, UI_OPT_OUTPUT, UI_OPT_SPEAKER,
    UI_OPT_MB, UI_OPT_SLOT, UI_OPT_MB_SLOTS, UI_OPT_NO_MB,
    UI_OPT_TEST, UI_OPT_BACK, UI_SND_TITLE, UI_SND_ALL,
    UI_COUNT
};
#define ENEMY_NAME_LEN 20    /* nom d'ennemi (combat) */
#define INPUT_BUF_LEN  41    /* saisie clavier max (40 + terminateur) */
#define PROMPT_LEN     40    /* invite de saisie */

/* Resultats d'un combat */
#define CB_VICTOIRE 0
#define CB_DEFAITE  1
#define CB_FUITE    2
#define CB_CONTINUE 0xFF     /* combat en cours (round joué, personne à terre) */

#define UI_STR_LEN    36     /* longueur max d'une chaine d'UI (tronquee sinon) */
#define STAT_NAME_LEN 12
#define ITEM_LABEL_LEN 20
#define SECTION_MAX   1024   /* taille max du corps d'une section en RAM */
#define MAX_CHOICES   9      /* choix visibles simultanément (touches 1..9) */

#define NO_GOTO       0xFFFF /* valeur "pas de saut" pour apply_effects */

typedef unsigned char  u8;
typedef unsigned int   u16;   /* cc65: int = 16 bits */
typedef unsigned long  u32;   /* cc65: long = 32 bits */

#endif /* A2ADV_FORMAT_H */
