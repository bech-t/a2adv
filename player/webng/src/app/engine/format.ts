// format.ts -- constantes SEMANTIQUES du format (opcodes, sentinelles,
// enums), miroir de player/common/include/format.h. Garder synchronise a la
// main avec ce fichier et avec compiler/a2c/model.py (meme discipline que
// les ports natifs : ce sont deux implementations independantes du meme
// contrat). Ne contient PLUS les constantes de mise en page BINAIRE
// (tailles d'en-tete, versions de format...) : depuis que ce player charge
// un JSON deja resolu (cf. engine/story.ts, compiler/a2c/webjson.py) plutot
// que STORY.DAT/APP.LNG, ces details d'empaquetage n'existent plus cote web.

export const NO_IMAGE = 0xffff;
export const NO_GOTO = 0xffff;
export const GAME_OVER = 0xffff;

export const MODE_FULL_TEXT = 0;
export const MODE_IMAGE_TEXT = 1;
export const MODE_FULL_IMAGE = 2;

export const END_NONE = 0;
export const END_WIN = 1;
export const END_LOSE = 2;

// Opcodes d'atomes de condition.
export const OP_FLAG_SET = 0x01;
export const OP_FLAG_CLR = 0x02;
export const OP_HAS_ITEM = 0x03;
export const OP_NO_ITEM = 0x04;
export const OP_STAT_CMP = 0x05;

export const STYLE_CENTER = 0x01;
export const STYLE_INVERSE = 0x02;

// Octet-bascule inline (invisible) : inverse ON/OFF au fil du texte,
// produit par le compilateur a partir des marqueurs *...* du .adv.
export const TXT_INV_TOGGLE = 0x01;

// Reference de stat inline (invisible), suivie d'un caractere d'index --
// produit par le compilateur a partir des marqueurs %NOM% du .adv (cf.
// compiler/a2c/symbols.py:substitute_stat_refs). Expandee par
// Engine.substituteStatRefs (engine.ts) avant que l'UI ne voie le texte :
// contrairement a TXT_INV_TOGGLE, jamais vue par rich-text.ts.
export const TXT_STAT_REF = 0x02;

export const CMP_EQ = 0;
export const CMP_NE = 1;
export const CMP_LT = 2;
export const CMP_LE = 3;
export const CMP_GT = 4;
export const CMP_GE = 5;

// Opcodes d'effets.
export const OP_SET_FLAG = 0x10;
export const OP_CLR_FLAG = 0x11;
export const OP_TOG_FLAG = 0x12;
export const OP_GIVE_ITEM = 0x13;
export const OP_TAKE_ITEM = 0x14;
export const OP_STAT_ADD = 0x15;
export const OP_STAT_SUB = 0x16;
export const OP_STAT_SET = 0x17;
export const OP_GOTO = 0x18;
export const OP_SOUND = 0x19;
export const OP_SCORE_ADD = 0x1a;
export const OP_STAT_MAX = 0x1b;
export const OP_STAT_SETMAX = 0x1c;

export const HDR_SCORE = 0x01;
export const HDR_MOVES = 0x02;

// Sons predefinis -- ORDRE FIGE, doit correspondre a SOUND_NAMES
// (compiler/a2c/model.py) et a l'enum SND_* de format.h. Objet + union de
// litteraux plutot qu'un `enum` TypeScript : `enum` genere du code a
// l'execution (une IIFE), incompatible avec `erasableSyntaxOnly` (cf.
// tsconfig.app.json) -- ce fichier reste alors purement des DECLARATIONS DE
// TYPE, effacees au build, comme tout le reste de engine/.
export const Snd = {
  SELECT: 0,
  ERROR: 1,
  WIN: 2,
  LOSE: 3,
  PICKUP: 4,
  HIT: 5,
  MAGIC: 6,
  DOOR: 7,
  PAGE: 8,
  DREAD: 9,
  BONUS: 10,
} as const;
export type Snd = (typeof Snd)[keyof typeof Snd];

// Chaines d'interface -- ORDRE FIGE, doit correspondre a UI_KEYS
// (compiler/a2c/model.py) et a l'enum UI_* de format.h. Meme raison qu'au-
// dessus (Snd) : objet + union de litteraux, pas un `enum` TypeScript.
export const Ui = {
  MENU_NEW: 0,
  MENU_LOAD: 1,
  MENU_QUIT: 2,
  INV_HUD: 3,
  HINTS: 4,
  ANYKEY: 5,
  INTRO_HINT: 6,
  END_WIN: 7,
  END_LOSE: 8,
  END_GENERIC: 9,
  SAVED: 10,
  SAVE_FAIL: 11,
  NO_SAVE: 12,
  INVENTORY: 13,
  INV_EMPTY: 14,
  NO_EXIT: 15,
  SECTION_ERR: 16,
  QUIT_CONFIRM: 17,
  QUIT_SAVE: 18,
  QUIT_NOSAVE: 19,
  QUIT_CANCEL: 20,
  LOADING: 21,
  SAVING: 22,
  SCORE: 23,
  MOVES: 24,
  CBT_ATK: 25,
  CBT_DMG: 26,
  CBT_ARM: 27,
  CB_HP: 28,
  CB_DICE: 29,
  CB_YOU: 30,
  CB_PARRY: 31,
  CB_ATTACK: 32,
  CB_FLEE: 33,
  MENU_OPTIONS: 34,
  OPT_TITLE: 35,
  OPT_OUTPUT: 36,
  OPT_SPEAKER: 37,
  OPT_MB: 38,
  OPT_SLOT: 39,
  OPT_MB_SLOTS: 40,
  OPT_NO_MB: 41,
  OPT_TEST: 42,
  OPT_BACK: 43,
  SND_TITLE: 44,
  SND_ALL: 45,
  OPT_INFO: 46,
  SYSINFO_TITLE: 47,
} as const;
export type Ui = (typeof Ui)[keyof typeof Ui];
export const UI_COUNT = 48;

// Resultats d'un combat.
export const CB_WIN = 0;
export const CB_LOSE = 1;
export const CB_FLEE = 2;
export const CB_CONTINUE = 0xff;
