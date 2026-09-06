"""Modèle de données de l'aventure + constantes du format binaire (spec §7ter)."""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum


# --- Énumérations du format ------------------------------------------------

class Mode(IntEnum):
    FULL_TEXT = 0
    IMAGE_TEXT = 1
    FULL_IMAGE = 2


class Ending(IntEnum):
    NONE = 0
    WIN = 1
    LOSE = 2


class Cmp(IntEnum):
    EQ = 0   # ==
    NE = 1   # !=
    LT = 2   # <
    LE = 3   # <=
    GT = 4   # >
    GE = 5   # >=


CMP_FROM_TEXT = {
    "==": Cmp.EQ, "!=": Cmp.NE,
    "<": Cmp.LT, "<=": Cmp.LE,
    ">": Cmp.GT, ">=": Cmp.GE,
}


# Chaînes d'interface : ORDRE FIGÉ (doit correspondre à l'enum UI_* du player,
# format.h). Défauts en français ; surchargeables par `@ui <clé> "texte"`.
UI_KEYS = [
    ("menu_new",    "COMMENCER"),
    ("menu_load",   "CHARGER"),
    ("menu_quit",   "QUITTER"),
    ("inv_hud",     "INV:"),
    ("hints",       "I=INV S=SAUVE L=CHARGE Q=MENU"),
    ("anykey",      "APPUYEZ SUR UNE TOUCHE..."),
    ("intro_hint",  "ESPACE OU ENTREE POUR CONTINUER"),
    ("end_win",     "*** VICTOIRE ***"),
    ("end_lose",    "*** DEFAITE ***"),
    ("end_generic", "*** FIN ***"),
    ("saved",       "PARTIE SAUVEGARDEE."),
    ("save_fail",   "ECHEC DE LA SAUVEGARDE."),
    ("no_save",     "AUCUNE SAUVEGARDE TROUVEE."),
    ("inventory",   "INVENTAIRE"),
    ("inv_empty",   "(INVENTAIRE VIDE)"),
    ("no_exit",     "(AUCUNE ISSUE POSSIBLE)"),
    ("section_err", "ERREUR DE CHARGEMENT DE SECTION"),
    ("quit_confirm", "REVENIR AU MENU ?"),
    ("quit_save",    "S) SAUVEGARDER ET REVENIR"),
    ("quit_nosave",  "Q) REVENIR SANS SAUVEGARDER"),
    ("quit_cancel",  "ESC) ANNULER"),
    ("loading",      "CHARGEMENT"),
    ("saving",       "SAUVEGARDE"),
    ("score",        "SCORE"),
    ("moves",        "MVT"),
    ("cbt_atk",      "ATT"),
    ("cbt_dmg",      "DEG"),
    ("cbt_arm",      "ARM"),
    # --- ecran de combat -----------------------------------------------
    ("cb_hp",        "PV"),
    ("cb_dice",      "DES"),
    ("cb_you",       "VOUS"),
    ("cb_parry",     "PARADE"),
    ("cb_attack",    "ATTAQUER"),
    ("cb_flee",      "FUIR"),
    # --- menu principal & options son ----------------------------------
    ("menu_options", "OPTIONS"),
    ("opt_title",    "OPTIONS SON"),
    ("opt_output",   "SORTIE"),
    ("opt_speaker",  "HAUT-PARLEUR"),
    ("opt_mb",       "MOCKINGBOARD"),
    ("opt_slot",     "SLOT"),
    ("opt_mb_slots", "MOCKINGBOARD (NO DE SLOT)"),
    ("opt_no_mb",    "MOCKINGBOARD NON COMPILEE"),
    ("opt_test",     "TESTER LES SONS"),
    ("opt_back",     "RETOUR"),
    ("snd_title",    "TEST DES SONS"),
    ("snd_all",      "TOUT JOUER"),
]
UI_KEY_SET = {k for k, _ in UI_KEYS}


# Opcodes d'atomes de condition
OP_FLAG_SET = 0x01
OP_FLAG_CLR = 0x02
OP_HAS_ITEM = 0x03
OP_NO_ITEM = 0x04
OP_STAT_CMP = 0x05

# Opcodes d'effets
OP_SET_FLAG = 0x10
OP_CLR_FLAG = 0x11
OP_TOG_FLAG = 0x12
OP_GIVE_ITEM = 0x13
OP_TAKE_ITEM = 0x14
OP_STAT_ADD = 0x15
OP_STAT_SUB = 0x16
OP_STAT_SET = 0x17
OP_GOTO = 0x18
OP_SOUND = 0x19
OP_SCORE_ADD = 0x1A     # ~ score N  (ajoute N au compteur de points)
OP_STAT_MAX = 0x1B      # ~ restore STAT  (valeur = max courant)
OP_STAT_SETMAX = 0x1C   # ~ setmax STAT N (fixe le max ; borne la valeur)

# Bits du champ 'flags' de l'entete STORY0.DAT
HDR_SCORE = 0x01        # compteur de points actif
HDR_MOVES = 0x02        # compteur de mouvements actif

# Styles de paragraphe de texte (champ style, cf. format.h)
STYLE_CENTER = 0x01
STYLE_INVERSE = 0x02

# Octet-bascule inline (invisible) : les marqueurs *...* du texte deviennent
# cet octet, que le player interprete comme "inverse ON/OFF".
TXT_INV_TOGGLE = 0x01

# Sons predefinis : ORDRE FIGE (doit correspondre a l'enum SND_* du player,
# format.h). Reference par l'effet DSL `~ sound <nom>`.
SOUND_NAMES = ["select", "error", "win", "lose",
               "pickup", "hit", "magic", "door", "page"]
SOUND_INDEX = {n: i for i, n in enumerate(SOUND_NAMES)}

# Limites du player (doivent correspondre a format.h). Le player REFUSE une
# aventure qui les depasse : mieux vaut echouer a la compilation.
MAX_FLAGS = 128           # total globaux + locaux
MAX_LOCAL_FLAGS = 32      # part reservee aux flags `local`


# --- Déclarations du préambule ---------------------------------------------

@dataclass
class StatDecl:
    name: str
    init: int
    lo: int = 0
    hi: int = 255
    line: int = 0
    hidden: bool = False    # utilisable en condition, absente du bandeau d'etat


@dataclass
class ItemDecl:
    name: str
    label: str
    default_on: bool = False
    line: int = 0
    # modificateurs de COMBAT, actifs tant que l'objet est porté (signés -128..127)
    atk: int = 0       # bonus a l'attaque (2d6 + ATT)
    dmg: int = 0       # bonus aux degats
    armor: int = 0     # reduction des degats subis


@dataclass
class FlagDecl:
    name: str
    default_on: bool = False
    line: int = 0
    is_local: bool = False    # remis a 0 a chaque changement de chapitre


# --- Conditions & effets (noms symboliques ; résolus en indices à l'encodage) -

@dataclass
class Atom:
    op: str            # 'flag' | 'not_flag' | 'has' | 'not_has' | 'stat'
    name: str          # nom de flag / item / stat
    cmp: Cmp | None = None
    value: int = 0
    line: int = 0


@dataclass
class Condition:
    atoms: list[Atom] = field(default_factory=list)
    connective: int = 0   # 0=AND, 1=OR
    line: int = 0

    @property
    def always(self) -> bool:
        return not self.atoms


@dataclass
class Effect:
    op: str            # 'set'|'clear'|'toggle'|'give'|'take'|'add'|'sub'|'setstat'|'goto'|'sound'
    name: str          # flag/item/stat/section/son
    value: int = 0
    cond: "Condition" = field(default_factory=lambda: Condition())  # garde optionnelle
    line: int = 0


# --- Sections & choix -------------------------------------------------------

@dataclass
class TextSegment:
    text: str
    cond: Condition
    style: int = 0            # STYLE_CENTER | STYLE_INVERSE
    line: int = 0


@dataclass
class Choice:
    label: str
    target: str                        # nom de section (résolu en index)
    cond: Condition
    effects: list[Effect] = field(default_factory=list)
    line: int = 0
    target_index: int = -1             # rempli à la résolution


@dataclass
class Combat:
    name: str                          # nom de l'ennemi (affiché)
    att: int = 0                       # attaque de l'ennemi (2d6 + att)
    hp: int = 1                        # points de vie de l'ennemi
    dmg: int = 2                       # dégâts de l'ennemi
    armor: int = 0                     # armure de l'ennemi
    image: str | None = None           # portrait (id d'asset, optionnel)
    win: str = ""                      # section si victoire
    lose: str = ""                     # section si défaite
    flee: str | None = None            # section si fuite (optionnel)
    win_effects: list = field(default_factory=list)    # effets appliqués sur victoire
    lose_effects: list = field(default_factory=list)   # ... sur défaite
    flee_effects: list = field(default_factory=list)   # ... sur fuite
    win_msg: str = ""                  # texte affiché sur victoire (optionnel)
    lose_msg: str = ""                 # ... sur défaite
    flee_msg: str = ""                 # ... sur fuite
    line: int = 0
    # résolus
    image_asset: int = 0xFFFF
    win_index: int = 0
    lose_index: int = 0
    flee_index: int = 0xFFFF


@dataclass
class Input:
    prompt: str = ""                   # invite affichée avant la saisie
    answers: list = field(default_factory=list)   # réponses acceptées (normalisées)
    maxlen: int = 20                   # longueur max saisie
    correct: str = ""                  # section si bonne réponse
    wrong: str = ""                    # section si mauvaise réponse
    correct_effects: list = field(default_factory=list)
    wrong_effects: list = field(default_factory=list)
    line: int = 0
    correct_index: int = 0
    wrong_index: int = 0


@dataclass
class Section:
    name: str
    line: int = 0
    mode: Mode = Mode.FULL_TEXT
    image: str | None = None
    ending: Ending = Ending.NONE
    on_enter: list[Effect] = field(default_factory=list)
    on_exit: list[Effect] = field(default_factory=list)   # effets appliqués en sortie
    texts: list[TextSegment] = field(default_factory=list)
    choices: list[Choice] = field(default_factory=list)
    image_asset: int = 0xFFFF          # rempli à la résolution
    chapter: int = 0                   # index de chapitre (pilote le decoupage fichier)
    combat: "Combat | None" = None     # section de combat (@combat) sinon None
    input: "Input | None" = None       # section a saisie (@ask) sinon None


@dataclass
class Story:
    title: str = ""
    author: str = ""
    start: str = ""
    stats: list[StatDecl] = field(default_factory=list)
    items: list[ItemDecl] = field(default_factory=list)
    flags: list[FlagDecl] = field(default_factory=list)
    sections: list[Section] = field(default_factory=list)
    intro: list[str] = field(default_factory=list)      # noms des scènes d'intro
    ui: dict = field(default_factory=dict)              # surcharges de chaînes d'UI
    score_on: bool = True     # compteur de points (désactivable via @score off)
    moves_on: bool = True     # compteur de mouvements (désactivable via @moves off)
    chapters: list[str] = field(default_factory=lambda: [""])  # titres, index=chapitre
    # combat : quelles stats jouent l'attaque et les PV du héros (par nom -> index)
    combat_attack: str = ""   # @combat_attack STAT
    combat_hp: str = ""       # @combat_hp STAT
    combat_base_dmg: int = 2  # dégâts du héros à mains nues (@combat_basedmg N)
    combat_attack_index: int = 0xFF   # résolus (0xFF = non défini)
    combat_hp_index: int = 0xFF
    # tables d'index (remplies à la résolution)
    assets: list[str] = field(default_factory=list)     # ids d'images, ordre = index
    intro_index: list[int] = field(default_factory=list)  # scènes d'intro -> index section
    start_index: int = 0
    local_base: int = 0     # 1er index de flag LOCAL (= nb de flags globaux)
    lang: str = "fr"        # socle d'interface : lang/<code>.lng -> APP.LNG
