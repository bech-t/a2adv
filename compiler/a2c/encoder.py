"""Encodeur binaire : Story -> STORY.DAT + ASSETS.IDX (spec §7ter).

Tous les champs multi-octets sont little-endian (natif 6502). Les opcodes de
condition et d'effet font 4 octets fixes.
"""

from __future__ import annotations

import struct

from . import model as M
from .errors import A2Error
from .symbols import Symbols
from .translit import transliterate

MAGIC_STORY = b"A2AD"
MAGIC_ASSETS = b"A2IX"
MAGIC_LANG = b"A2LG"
VERSION = 6                 # v6 : masque des stats masquees (@stat ... hidden)
                            # v5 : socle d'UI dans APP.LNG, l'aventure ne porte
                            #      plus que ses surcharges (v4 : index par fichier)
LANG_VERSION = 1
NO_IMAGE = 0xFFFF
HEADER_SIZE = 20
DEFAULT_MAX_FILE = 0xFC00  # taille max d'un STORYn.DAT (offsets tenables en u16)
MAX_FILES = 100           # noms STORY00.DAT..STORY99.DAT (2 chiffres decimaux)
SECTION_MAX = 1024        # doit rester <= SECTION_MAX du player (secbuf)

_ATOM_OP = {
    "flag": M.OP_FLAG_SET, "not_flag": M.OP_FLAG_CLR,
    "has": M.OP_HAS_ITEM, "not_has": M.OP_NO_ITEM, "stat": M.OP_STAT_CMP,
}
_EFFECT_OP = {
    "set": M.OP_SET_FLAG, "clear": M.OP_CLR_FLAG, "toggle": M.OP_TOG_FLAG,
    "give": M.OP_GIVE_ITEM, "take": M.OP_TAKE_ITEM,
    "add": M.OP_STAT_ADD, "sub": M.OP_STAT_SUB, "setstat": M.OP_STAT_SET,
    "goto": M.OP_GOTO, "sound": M.OP_SOUND, "score": M.OP_SCORE_ADD,
    "restore": M.OP_STAT_MAX, "setmax": M.OP_STAT_SETMAX,
}


def _ascii(text: str) -> bytes:
    return transliterate(text).encode("ascii")


def _bitset(count: int, is_on) -> bytes:
    out = bytearray((count + 7) // 8)
    for i in range(count):
        if is_on(i):
            out[i >> 3] |= 1 << (i & 7)
    return bytes(out)


# --- STORY.DAT -------------------------------------------------------------

def encode_story(story: M.Story, max_file: int = DEFAULT_MAX_FILE) -> list[bytes]:
    """Encode l'aventure en 1..N fichiers STORYn.DAT (format v4).

    Découpage par chapitre (+ sous-découpage si > max_file). Chaque STORYn.DAT
    porte SON PROPRE index local (offsets internes des corps). STORY0.DAT porte
    en plus header + preambule + la table `file_first` (1re section globale de
    chaque fichier). Le player ne garde resident que `file_first` + l'index du
    fichier courant."""
    sym = Symbols(story)

    preamble = _encode_preamble(story)
    bodies = [_encode_section(sec, sym) for sec in story.sections]
    n = len(bodies)

    # --- Pass 1 : repartir les sections en fichiers (par chapitre + taille) ---
    file_bodies = []        # liste de corps par fichier
    file_first = []         # index global de la 1re section de chaque fichier
    cur_chapter = None
    cur_file = -1
    cur_bytes = 0
    for i, sec in enumerate(story.sections):
        blen = len(bodies[i])
        if blen > SECTION_MAX:
            raise A2Error(f"section '{sec.name}' trop grosse ({blen} > "
                          f"{SECTION_MAX} o)", sec.line)
        new_chapter = cur_file < 0 or sec.chapter != cur_chapter
        # marge de 1 Ko pour l'entete/index (offsets tenables en u16)
        overflow = cur_file >= 0 and (cur_bytes + blen) > (max_file - 1024)
        if new_chapter or overflow:
            file_bodies.append([])
            file_first.append(i)
            cur_file += 1
            cur_bytes = 0
            cur_chapter = sec.chapter
        file_bodies[cur_file].append(bodies[i])
        cur_bytes += blen
    n_files = len(file_bodies)
    file_first.append(n)    # sentinelle
    if n_files > MAX_FILES:
        raise A2Error(f"trop de fichiers ({n_files} > {MAX_FILES}) : "
                      f"regroupe des chapitres (1 fichier/chapitre)")

    flags = 0
    if story.score_on:
        flags |= M.HDR_SCORE
    if story.moves_on:
        flags |= M.HDR_MOVES

    header = (
        MAGIC_STORY
        + struct.pack("<BB", VERSION, flags)
        + struct.pack("<H", n)
        + struct.pack("<BBBB", len(story.stats), len(story.items),
                      len(story.flags), len(story.intro_index))
        + struct.pack("<H", story.start_index)
        + struct.pack("<I", HEADER_SIZE + len(preamble))   # -> table file_first
        # offset 18 : n_files ; offset 19 : 1er index de flag LOCAL
        + struct.pack("<BB", n_files, story.local_base)
    )
    assert len(header) == HEADER_SIZE, len(header)

    ff_bytes = struct.pack("<%dH" % (n_files + 1), *file_first)

    # --- Pass 2 : index local de chaque fichier + corps ---
    out_files = []
    for f in range(n_files):
        bods = file_bodies[f]
        count = len(bods)
        if count > 256:                            # = LOCAL_IDX_MAX du player
            raise A2Error(f"fichier STORY{f} : {count} sections (> 256) ; "
                          f"decoupe le chapitre")
        lidx_size = 2 + (count + 1) * 2            # u16 count + (count+1) offsets
        base = lidx_size
        if f == 0:
            base += HEADER_SIZE + len(preamble) + len(ff_bytes)
        offs, pos = [], base
        for b in bods:
            offs.append(pos)
            pos += len(b)
        offs.append(pos)                           # sentinelle = fin des corps
        if pos > 0xFFFF:
            raise A2Error(f"fichier STORY{f} trop gros ({pos} o > 64 Ko)")
        local_index = struct.pack("<H", count) \
            + b"".join(struct.pack("<H", o) for o in offs)
        blob = b"".join(bods)
        if f == 0:
            out_files.append(header + preamble + ff_bytes + local_index + blob)
        else:
            out_files.append(local_index + blob)
    return out_files


def _encode_preamble(story: M.Story) -> bytes:
    out = bytearray()
    for s in story.stats:
        out += struct.pack("<BBB", s.init, s.lo, s.hi)
    # v6 : masque des stats MASQUEES (bit i = stat i absente du bandeau d'etat).
    # Un seul octet suffit (MAX_STATS = 8), plutot qu'un octet par stat.
    hidden = 0
    for i, s in enumerate(story.stats):
        if s.hidden:
            hidden |= 1 << i
    out += struct.pack("<B", hidden)
    out += _bitset(len(story.items), lambda i: story.items[i].default_on)
    out += _bitset(len(story.flags), lambda i: story.flags[i].default_on)
    for s in story.stats:
        out += _lenstr(s.name)
    for it in story.items:
        out += _lenstr(it.label if it.label else it.name)
    out += _lenstr(story.title)                       # titre (pour le menu)
    for idx in story.intro_index:                     # scènes d'intro
        out += struct.pack("<H", idx)
    # Chaînes d'UI : SEULES LES SURCHARGES (v5). Le socle des 28 chaînes vit
    # dans APP.LNG (cf. spec §6.1) ; ici, des couples (index de clé, texte)
    # appliqués par-dessus. Une aventure sans `@ui` n'emporte donc rien.
    over = [(i, story.ui[k]) for i, (k, _d) in enumerate(M.UI_KEYS)
            if k in story.ui]
    out += struct.pack("<B", len(over))
    for i, text in over:
        out += struct.pack("<B", i) + _lenstr(text)
    # attributs de combat par objet (atk, dmg, armor) — signés, en fin de préambule
    for it in story.items:
        out += struct.pack("<bbb", it.atk, it.dmg, it.armor)
    # config de combat : stat d'attaque, stat de PV (0xFF si absent), dégâts de base
    out += struct.pack("<BBB", story.combat_attack_index & 0xFF,
                       story.combat_hp_index & 0xFF, story.combat_base_dmg & 0xFF)
    return bytes(out)


def _lenstr(text: str) -> bytes:
    b = _ascii(text)
    if len(b) > 255:
        raise A2Error(f"chaîne trop longue (>255): '{text[:20]}...'")
    return struct.pack("<B", len(b)) + b


def _encode_section(sec: M.Section, sym: Symbols) -> bytes:
    out = bytearray()
    out += struct.pack("<BB", int(sec.mode), int(sec.ending))
    out += struct.pack("<H", sec.image_asset if sec.image else NO_IMAGE)
    # bloc combat optionnel (u8 présent + données ennemi + cibles)
    if sec.combat is None:
        out += struct.pack("<B", 0)
    else:
        cb = sec.combat
        out += struct.pack("<B", 1)
        out += struct.pack("<BBBB", cb.att, cb.hp, cb.dmg, cb.armor)
        out += struct.pack("<H", cb.image_asset if cb.image else NO_IMAGE)
        out += struct.pack("<HHH", cb.win_index, cb.lose_index, cb.flee_index)
        out += _lenstr(cb.name)
        out += _encode_effects(cb.win_effects, sym)    # effets par issue
        out += _encode_effects(cb.lose_effects, sym)
        out += _encode_effects(cb.flee_effects, sym)
        out += _lenstr(cb.win_msg)     # textes d'issue (vides = aucun ecran)
        out += _lenstr(cb.lose_msg)
        out += _lenstr(cb.flee_msg)
    # bloc saisie optionnel (u8 present + invite + reponses + cibles + effets)
    if sec.input is None:
        out += struct.pack("<B", 0)
    else:
        ip = sec.input
        out += struct.pack("<B", 1)
        out += _lenstr(ip.prompt)
        out += struct.pack("<B", ip.maxlen)
        out += struct.pack("<B", len(ip.answers))
        for a in ip.answers:
            out += _lenstr(a)
        out += struct.pack("<HH", ip.correct_index, ip.wrong_index)
        out += _encode_effects(ip.correct_effects, sym)
        out += _encode_effects(ip.wrong_effects, sym)
    out += _encode_effects(sec.on_enter, sym)
    out += _encode_effects(sec.on_exit, sym)          # effets de sortie

    out += struct.pack("<B", len(sec.texts))
    for t in sec.texts:
        out += _encode_cond(t.cond, sym)
        out += struct.pack("<B", t.style)        # style du paragraphe
        # marqueurs inline *...* -> octet bascule inverse (invisible)
        body = _ascii(t.text).replace(b"*", bytes([M.TXT_INV_TOGGLE]))
        if len(body) > 0xFFFF:
            raise A2Error("segment de texte trop long (>65535)", t.line)
        out += struct.pack("<H", len(body)) + body

    out += struct.pack("<B", len(sec.choices))
    for c in sec.choices:
        out += _encode_cond(c.cond, sym)
        out += _encode_effects(c.effects, sym)
        out += struct.pack("<H", c.target_index)
        out += _lenstr(c.label)
    return bytes(out)


def _encode_cond(cond: M.Condition, sym: Symbols) -> bytes:
    out = bytearray(struct.pack("<BB", len(cond.atoms), cond.connective))
    for a in cond.atoms:
        out += _encode_atom(a, sym)
    return bytes(out)


def _encode_atom(a: M.Atom, sym: Symbols) -> bytes:
    op = _ATOM_OP[a.op]
    if a.op in ("flag", "not_flag"):
        return struct.pack("<BBBB", op, sym.flags[a.name], 0, 0)
    if a.op in ("has", "not_has"):
        return struct.pack("<BBBB", op, sym.items[a.name], 0, 0)
    # stat
    return struct.pack("<BBBB", op, sym.stats[a.name], int(a.cmp), a.value)


def _encode_effects(effects: list[M.Effect], sym: Symbols) -> bytes:
    out = bytearray(struct.pack("<B", len(effects)))
    for e in effects:
        out += _encode_cond(e.cond, sym)     # garde (n_atoms=0 si inconditionnel)
        out += _encode_effect(e, sym)
    return bytes(out)


def _encode_effect(e: M.Effect, sym: Symbols) -> bytes:
    op = _EFFECT_OP[e.op]
    if e.op in ("set", "clear", "toggle"):
        return struct.pack("<BBBB", op, sym.flags[e.name], 0, 0)
    if e.op in ("give", "take"):
        return struct.pack("<BBBB", op, sym.items[e.name], 0, 0)
    if e.op in ("add", "sub", "setstat", "setmax"):
        return struct.pack("<BBBB", op, sym.stats[e.name], e.value, 0)
    if e.op == "restore":                       # valeur = max courant (a1 inutile)
        return struct.pack("<BBBB", op, sym.stats[e.name], 0, 0)
    if e.op == "sound":
        return struct.pack("<BBBB", op, M.SOUND_INDEX[e.name], 0, 0)
    if e.op == "score":
        return struct.pack("<BBBB", op, e.value & 0xFF, 0, 0)
    # goto : indice de section sur 16 bits (a0=lo, a1=hi)
    idx = sym.sections[e.name]
    return struct.pack("<BBBB", op, idx & 0xFF, (idx >> 8) & 0xFF, 0)


# --- ASSETS.IDX ------------------------------------------------------------

def encode_lang(lang: str, strings: dict[str, str]) -> bytes:
    """Socle de chaines d'interface -> APP.LNG (cf. spec §6.1).

    Positionnel : les 28 chaines dans l'ordre figé de UI_KEYS. Toutes sont
    exigées — un socle incomplet laisserait le player muet sur une clé.
    """
    missing = [k for k, _d in M.UI_KEYS if k not in strings]
    if missing:
        raise A2Error(f"fichier de langue incomplet, {len(missing)} clé(s) "
                      f"manquante(s) : {', '.join(missing[:5])}"
                      + (" ..." if len(missing) > 5 else ""))
    code = _ascii(lang).ljust(2)[:2]
    out = bytearray(MAGIC_LANG)
    out += struct.pack("<B", LANG_VERSION) + code
    out += struct.pack("<B", len(M.UI_KEYS))
    for key, _default in M.UI_KEYS:
        out += _lenstr(strings[key])
    return bytes(out)


def encode_assets(story: M.Story) -> bytes:
    out = bytearray(MAGIC_ASSETS)
    out += struct.pack("<BB", VERSION, 0)
    out += struct.pack("<H", len(story.assets))
    for img in story.assets:
        path = f"/A2ADV/IMG/{img.upper()}.HGR".encode("ascii")
        # disque=0, offset=0, length=0 (rempli à M4 quand les images existent)
        out += struct.pack("<BII", 0, 0, 0)
        out += struct.pack("<B", len(path)) + path
    return bytes(out)
