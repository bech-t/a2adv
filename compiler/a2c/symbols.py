"""Table de symboles, validation et résolution en indices (spec §6.1, §7ter).

Vérifie les déclarations obligatoires (stats/items/flags), l'existence des
sections cibles, la cohérence des modes/images, et affecte les indices utilisés
par l'encodeur binaire.
"""

from __future__ import annotations

from . import model as M
from .errors import A2Error
from .model import Atom, Condition, Effect, Mode, Section, Story
from .translit import transliterate


class Symbols:
    def __init__(self, story: Story):
        self.story = story
        self.stats = {s.name: i for i, s in enumerate(story.stats)}
        self.items = {it.name: i for i, it in enumerate(story.items)}
        self.flags = {fl.name: i for i, fl in enumerate(story.flags)}
        self.sections = {s.name: i for i, s in enumerate(story.sections)}


def resolve(story: Story) -> list[str]:
    """Valide l'aventure et remplit les index. Renvoie la liste des warnings."""
    warnings: list[str] = []

    # Flags LOCAUX en fin de table : leurs index forment une plage contigue
    # [local_base, n_flags) que le player efface a chaque changement de
    # chapitre. L'ordre relatif de chaque groupe est preserve.
    story.flags.sort(key=lambda fl: fl.is_local)
    story.local_base = sum(1 for fl in story.flags if not fl.is_local)
    n_local = len(story.flags) - story.local_base
    if n_local > M.MAX_LOCAL_FLAGS:
        raise A2Error(f"{n_local} flags 'local' (> {M.MAX_LOCAL_FLAGS}) : "
                      "en rendre quelques-uns globaux")
    if len(story.flags) > M.MAX_FLAGS:
        raise A2Error(f"{len(story.flags)} flags (> {M.MAX_FLAGS})")

    sym = Symbols(story)

    _check_unique(story)

    if not story.start:
        raise A2Error("@start manquant dans le préambule")
    if story.start not in sym.sections:
        raise A2Error(f"@start référence une section inconnue: '{story.start}'")
    story.start_index = sym.sections[story.start]

    # table d'assets (images) : ordre de première apparition
    asset_index: dict[str, int] = {}

    for sec in story.sections:
        _resolve_section(sec, sym, asset_index, warnings)

    story.assets = list(asset_index.keys())

    # config de combat : noms de stats -> index (0xFF si non défini)
    for name, attr in ((story.combat_attack, "combat_attack_index"),
                       (story.combat_hp, "combat_hp_index")):
        if name:
            if name not in sym.stats:
                raise A2Error(f"@combat_attack/@combat_hp : stat inconnue '{name}'")
            setattr(story, attr, sym.stats[name])

    # scènes d'intro -> index de sections (doivent exister)
    story.intro_index = []
    for name in story.intro:
        if name not in sym.sections:
            raise A2Error(f"@intro référence une section inconnue: '{name}'")
        story.intro_index.append(sym.sections[name])

    if len(story.sections) > 0xFFFF:
        raise A2Error("trop de sections (> 65535)")
    for lim, what in ((len(story.stats), "stats"),
                      (len(story.items), "items"),
                      (len(story.flags), "flags"),
                      (len(story.assets), "images")):
        if lim > 255 and what != "images":
            raise A2Error(f"trop de {what} (> 255)")
    if len(story.assets) > 0xFFFE:
        raise A2Error("trop d'images (> 65534)")
    return warnings


def _check_unique(story: Story) -> None:
    for coll, what in ((story.stats, "stat"), (story.items, "item"),
                       (story.flags, "flag"), (story.sections, "section")):
        seen: dict[str, int] = {}
        for d in coll:
            if d.name in seen:
                raise A2Error(f"{what} '{d.name}' déclaré deux fois", d.line)
            seen[d.name] = d.line


def _resolve_section(sec: Section, sym: Symbols,
                     asset_index: dict[str, int], warnings: list[str]) -> None:
    # mode / image
    if sec.mode in (Mode.IMAGE_TEXT, Mode.FULL_IMAGE):
        if sec.image is None:
            raise A2Error(f"section '{sec.name}': mode graphique sans @image",
                          sec.line)
    if sec.image is not None:
        if sec.mode == Mode.FULL_TEXT:
            warnings.append(f"ligne {sec.line}: @image ignoré en mode full_text "
                            f"(section '{sec.name}')")
        sec.image_asset = asset_index.setdefault(sec.image, len(asset_index))

    for e in sec.on_enter:
        _resolve_effect(e, sym)
    for e in sec.on_exit:
        _resolve_effect(e, sym)
    for t in sec.texts:
        _resolve_condition(t.cond, sym)
    for c in sec.choices:
        _resolve_condition(c.cond, sym)
        for e in c.effects:
            _resolve_effect(e, sym)
        if c.target not in sym.sections:
            raise A2Error(f"choix vers une section inconnue: '{c.target}'", c.line)
        c.target_index = sym.sections[c.target]

    if sec.combat is not None:
        cb = sec.combat
        if not cb.win or not cb.lose:
            raise A2Error(f"@combat dans '{sec.name}': @win et @lose "
                          "obligatoires", cb.line)
        for nm, attr in ((cb.win, "win_index"), (cb.lose, "lose_index")):
            if nm not in sym.sections:
                raise A2Error(f"@combat: section inconnue '{nm}'", cb.line)
            setattr(cb, attr, sym.sections[nm])
        if cb.flee:
            if cb.flee not in sym.sections:
                raise A2Error(f"@combat: section de fuite inconnue '{cb.flee}'",
                              cb.line)
            cb.flee_index = sym.sections[cb.flee]
        if cb.image is not None:
            cb.image_asset = asset_index.setdefault(cb.image, len(asset_index))
        for e in cb.win_effects:
            _resolve_effect(e, sym)
        for e in cb.lose_effects:
            _resolve_effect(e, sym)
        for e in cb.flee_effects:
            _resolve_effect(e, sym)
        if not (0 <= cb.att <= 255 and 1 <= cb.hp <= 255 and
                0 <= cb.dmg <= 255 and 0 <= cb.armor <= 255):
            raise A2Error(f"@combat dans '{sec.name}': valeurs hors bornes "
                          "(att/dmg/armor 0..255, hp 1..255)", cb.line)

    if sec.input is not None:
        ip = sec.input
        if not ip.correct or not ip.wrong:
            raise A2Error(f"@ask dans '{sec.name}': @correct et @wrong requis",
                          ip.line)
        if not ip.answers:
            raise A2Error(f"@ask dans '{sec.name}': au moins un @answer requis",
                          ip.line)
        for nm, attr in ((ip.correct, "correct_index"), (ip.wrong, "wrong_index")):
            if nm not in sym.sections:
                raise A2Error(f"@ask: section inconnue '{nm}'", ip.line)
            setattr(ip, attr, sym.sections[nm])
        # Normalisation des réponses : sans accents, MAJUSCULES, sans espaces
        # de bord — indépendamment de --minuscules. Le player compare à une
        # saisie que `norm_input` (sinput.c) met elle aussi en capitales : la
        # casse d'affichage ne doit pas décider si une réponse est acceptée.
        ip.answers = [transliterate(a, upper=True).strip() for a in ip.answers]
        for e in ip.correct_effects:
            _resolve_effect(e, sym)
        for e in ip.wrong_effects:
            _resolve_effect(e, sym)

    # avertissement de place en mode image_text (~4 lignes de texte + choix)
    if sec.mode == Mode.IMAGE_TEXT:
        approx = sum(max(1, -(-len(t.text) // 40)) for t in sec.texts) \
            + len(sec.choices)
        if approx > 4:
            warnings.append(f"ligne {sec.line}: section '{sec.name}' en "
                            f"image_text : ~{approx} lignes pour ~4 dispo "
                            "(envisager full_text)")


def _resolve_condition(cond: Condition, sym: Symbols) -> None:
    for a in cond.atoms:
        _resolve_atom(a, sym)


def _resolve_atom(a: Atom, sym: Symbols) -> None:
    if a.op in ("flag", "not_flag"):
        if a.name not in sym.flags:
            raise A2Error(f"flag non déclaré: '{a.name}' "
                          "(ajouter @flag au préambule)", a.line)
    elif a.op in ("has", "not_has"):
        if a.name not in sym.items:
            raise A2Error(f"objet non déclaré: '{a.name}' "
                          "(ajouter @item au préambule)", a.line)
    elif a.op == "stat":
        if a.name not in sym.stats:
            raise A2Error(f"stat non déclarée: '{a.name}' "
                          "(ajouter @stat au préambule)", a.line)


def _resolve_effect(e: Effect, sym: Symbols) -> None:
    _resolve_condition(e.cond, sym)          # garde optionnelle de l'effet
    if e.op in ("set", "clear", "toggle"):
        if e.name not in sym.flags:
            raise A2Error(f"flag non déclaré: '{e.name}' "
                          "(ajouter @flag au préambule)", e.line)
    elif e.op in ("give", "take"):
        if e.name not in sym.items:
            raise A2Error(f"objet non déclaré: '{e.name}' "
                          "(ajouter @item au préambule)", e.line)
    elif e.op in ("add", "sub", "setstat", "restore", "setmax"):
        if e.name not in sym.stats:
            raise A2Error(f"stat non déclarée: '{e.name}' "
                          "(ajouter @stat au préambule)", e.line)
    elif e.op == "goto":
        if e.name not in sym.sections:
            raise A2Error(f"goto vers une section inconnue: '{e.name}'", e.line)
    elif e.op == "sound":
        from .model import SOUND_INDEX
        if e.name not in SOUND_INDEX:
            raise A2Error(f"son inconnu: '{e.name}' "
                          f"(connus: {', '.join(SOUND_INDEX)})", e.line)
