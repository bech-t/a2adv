"""Décodeur de STORY.DAT — vérification de l'encodeur et référence pour le player.

Relit un STORY.DAT en structures Python lisibles. Sert de test (round-trip) et de
spécification exécutable du format §7ter pour l'implémentation 6502.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field

from . import model as M


class _Reader:
    def __init__(self, buf: bytes, pos: int = 0):
        self.buf, self.pos = buf, pos

    def u8(self) -> int:
        v = self.buf[self.pos]; self.pos += 1; return v

    def u16(self) -> int:
        v = struct.unpack_from("<H", self.buf, self.pos)[0]; self.pos += 2; return v

    def u32(self) -> int:
        v = struct.unpack_from("<I", self.buf, self.pos)[0]; self.pos += 4; return v

    def take(self, n: int) -> bytes:
        v = self.buf[self.pos:self.pos + n]; self.pos += n; return v

    def lenstr(self) -> str:
        return self.take(self.u8()).decode("ascii")


@dataclass
class DecHeader:
    version: int
    n_sections: int
    n_stats: int
    n_items: int
    n_flags: int
    start_section: int
    index_offset: int
    flags: int = 0


@dataclass
class DecSection:
    mode: int
    ending: int
    image_asset: int
    on_enter: list = field(default_factory=list)
    on_exit: list = field(default_factory=list)
    texts: list = field(default_factory=list)
    choices: list = field(default_factory=list)
    combat: dict = None
    input: dict = None


def decode(buf: bytes) -> dict:
    r = _Reader(buf)
    if r.take(4) != b"A2AD":
        raise ValueError("magic STORY.DAT invalide")
    version, hdr_flags = r.u8(), r.u8()
    n_sections = r.u16()
    n_stats, n_items, n_flags, n_intro = r.u8(), r.u8(), r.u8(), r.u8()
    start_section = r.u16()
    index_offset = r.u32()
    n_files = r.u8()          # offset 18
    local_base = r.u8()       # offset 19 : 1er index de flag LOCAL
    header = DecHeader(version, n_sections, n_stats, n_items, n_flags,
                       start_section, index_offset, hdr_flags)

    stat_table = [tuple(r.take(3)) for _ in range(n_stats)]
    stat_hidden = r.u8()          # v6 : bit i = stat i masquee au bandeau
    items_default = r.take((n_items + 7) // 8)
    flags_default = r.take((n_flags + 7) // 8)
    stat_names = [r.lenstr() for _ in range(n_stats)]
    item_labels = [r.lenstr() for _ in range(n_items)]
    title = r.lenstr()
    intro = [r.u16() for _ in range(n_intro)]
    n_ui = r.u8()
    ui = [r.lenstr() for _ in range(n_ui)]

    # attributs de combat par objet (atk, dmg, armor) — signés
    def _s8(b):
        return b - 256 if b >= 128 else b
    item_combat = [tuple(_s8(x) for x in r.take(3)) for _ in range(n_items)]

    # config de combat : stat attaque, stat PV (0xFF=absent), dégâts de base
    combat_att, combat_hp, combat_basedmg = r.u8(), r.u8(), r.u8()

    # v4 : table file_first (1re section globale de chaque fichier) + index
    # local de STORY0. Les corps des autres fichiers ne sont pas dans ce buffer.
    fr = _Reader(buf, index_offset)
    file_first = [fr.u16() for _ in range(n_files + 1)]
    count0 = fr.u16()
    local0 = [fr.u16() for _ in range(count0 + 1)]

    def _file_of(idx):
        for f in range(n_files):
            if file_first[f] <= idx < file_first[f + 1]:
                return f
        return 0

    entries, sections = [], []
    for idx in range(n_sections):
        f = _file_of(idx)
        if f == 0:
            slot = idx - file_first[0]
            off = local0[slot]
            entries.append((0, off, local0[slot + 1] - off))
            sections.append(_decode_section(_Reader(buf, off)))
        else:
            entries.append((f, 0, 0))
            sections.append(None)

    return {
        "header": header,
        "n_files": n_files,
        "local_base": local_base,
        "stat_table": stat_table,
        "stat_hidden": stat_hidden,
        "items_default": items_default,
        "flags_default": flags_default,
        "stat_names": stat_names,
        "item_labels": item_labels,
        "title": title,
        "intro": intro,
        "ui": ui,
        "item_combat": item_combat,
        "index": entries,
        "sections": sections,
    }


def _decode_cond(r: _Reader) -> list:
    n = r.u8(); r.u8()  # connective
    return [tuple(r.take(4)) for _ in range(n)]


def _decode_effects(r: _Reader) -> list:
    n = r.u8()
    out = []
    for _ in range(n):
        _decode_cond(r)                 # garde de l'effet (ignoree ici)
        out.append(tuple(r.take(4)))
    return out


def _decode_section(r: _Reader) -> DecSection:
    mode = r.u8()
    ending = r.u8()
    image_asset = r.u16()
    combat = None
    if r.u8():                         # bloc combat présent ?
        att, hp, dmg, armor = r.u8(), r.u8(), r.u8(), r.u8()
        cimg = r.u16()
        win, lose, flee = r.u16(), r.u16(), r.u16()
        cname = r.lenstr()
        win_fx = _decode_effects(r)
        lose_fx = _decode_effects(r)
        flee_fx = _decode_effects(r)
        combat = dict(name=cname, att=att, hp=hp, dmg=dmg, armor=armor,
                      image=cimg, win=win, lose=lose, flee=flee,
                      win_fx=win_fx, lose_fx=lose_fx, flee_fx=flee_fx)
    inp = None
    if r.u8():                          # bloc saisie présent ?
        prompt = r.lenstr()
        maxlen = r.u8()
        answers = [r.lenstr() for _ in range(r.u8())]
        icorrect, iwrong = r.u16(), r.u16()
        cfx = _decode_effects(r)
        wfx = _decode_effects(r)
        inp = dict(prompt=prompt, maxlen=maxlen, answers=answers,
                   correct=icorrect, wrong=iwrong, correct_fx=cfx, wrong_fx=wfx)
    on_enter = _decode_effects(r)
    on_exit = _decode_effects(r)
    texts = []
    for _ in range(r.u8()):
        cond = _decode_cond(r)
        style = r.u8()
        text = r.take(r.u16()).decode("ascii")
        texts.append((cond, style, text))
    choices = []
    for _ in range(r.u8()):
        cond = _decode_cond(r)
        effects = _decode_effects(r)
        target = r.u16()
        label = r.lenstr()
        choices.append((cond, effects, target, label))
    return DecSection(mode, ending, image_asset, on_enter, on_exit, texts,
                      choices, combat, inp)


def _dump(path: str) -> None:
    d = decode(open(path, "rb").read())
    h = d["header"]
    print(f"STORY.DAT v{h.version} : {h.n_sections} sections, "
          f"{d['n_files']} fichier(s), "
          f"{h.n_stats} stats, {h.n_items} items, {h.n_flags} flags, "
          f"start={h.start_section} "
          f"score={'on' if h.flags & 0x01 else 'off'} "
          f"moves={'on' if h.flags & 0x02 else 'off'}")
    print("stats :", list(zip(d["stat_names"], d["stat_table"])))
    print("items :", d["item_labels"],
          "default=", d["items_default"].hex())
    for i, s in enumerate(d["sections"]):
        fid, off, ln = d["index"][i]
        if s is None:
            print(f"\n[{i}] (corps dans STORY{fid:02d}.DAT, off={off} len={ln})")
            continue
        print(f"\n[{i}] file={fid} mode={s.mode} ending={s.ending} "
              f"image={s.image_asset:#06x} on_enter={s.on_enter}")
        for cond, style, text in s.texts:
            g = f"{{{cond}}} " if cond else ""
            st = f"[style {style}] " if style else ""
            print(f"    T: {g}{st}{text!r}")
        for cond, eff, tgt, label in s.choices:
            g = f"{{{cond}}} " if cond else ""
            print(f"    * {g}[{label}] -> {tgt}  eff={eff}")


if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("usage: python -m a2c.decode <STORY0.DAT>", file=sys.stderr)
        raise SystemExit(2)
    _dump(sys.argv[1])
