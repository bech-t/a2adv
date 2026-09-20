"""Table de symboles, validation et résolution en indices.

Vérifie les déclarations obligatoires (stats/items/flags), l'existence des
sections cibles, la cohérence des modes/images, et affecte les indices utilisés
par l'encodeur binaire.
"""

from __future__ import annotations

import re

from . import model as M
from .errors import A2Error
from .model import Atom, Condition, Effect, Mode, Section, Story
from .translit import to_match_key

_STAT_REF_RE = re.compile(r"%([A-Za-z_][A-Za-z0-9_]*)%")


class Symbols:
    def __init__(self, story: Story):
        self.story = story
        self.stats = {s.name: i for i, s in enumerate(story.stats)}
        self.items = {it.name: i for i, it in enumerate(story.items)}
        self.flags = story.flag_slots           # nom (unique) -> emplacement
        self.sections = {s.name: i for i, s in enumerate(story.sections)}
        # portee des flags locaux : chapitre -> {nom ecrit ou unique -> nom unique}
        self.local_scope: dict[int, dict[str, str]] = {}
        # nom ecrit ou unique d'un local -> chapitres qui le declarent
        self.local_where: dict[str, list[int]] = {}
        self.global_flags: set[str] = set()
        for fl in story.flags:
            if fl.is_local:
                scope = self.local_scope.setdefault(fl.chapter, {})
                scope[fl.base] = scope[fl.name] = fl.name
                for key in {fl.base, fl.name}:
                    self.local_where.setdefault(key, []).append(fl.chapter)
            else:
                self.global_flags.add(fl.name)
        self.cur_chapter = 0                    # chapitre de la section en cours de resolution

    def chapter_label(self, c: int) -> str:
        title = self.story.chapters[c] if c < len(self.story.chapters) else ""
        return f"« {title} »" if title else f"n°{c}"

    def flag_ref(self, name: str, line: int) -> str:
        """Nom unique du flag `name` tel que vu depuis le chapitre courant."""
        if name in self.global_flags:
            return name
        scope = self.local_scope.get(self.cur_chapter, {})
        if name in scope:
            return scope[name]
        elsewhere = self.local_where.get(name)
        if elsewhere:
            chaps = ", ".join(self.chapter_label(c) for c in elsewhere)
            raise A2Error(f"flag local '{name}' déclaré dans le chapitre {chaps} "
                          f"mais utilisé dans le chapitre "
                          f"{self.chapter_label(self.cur_chapter)} "
                          "(un flag local n'existe que dans son chapitre : le "
                          "déclarer ici, ou le rendre global)", line)
        raise A2Error(f"flag non déclaré: '{name}' (ajouter @flag au préambule, "
                      "ou @flag NOM local dans le chapitre)", line)


def substitute_stat_refs(text: str, sym: Symbols, line: int = 0) -> str:
    """Remplace chaque %NOM% (valeur de stat) par TXT_STAT_REF suivi de
    l'index de la stat -- deux caracteres bruts embarques dans le texte,
    meme convention que '*...*' -> TXT_INV_TOGGLE (cf. encoder.py/
    webjson.py, qui appellent cette fonction chacun de son cote juste avant
    d'encoder/serialiser, comme pour '*'). La valeur REELLE n'est connue
    qu'a l'execution : ce qui est encode ici est une REFERENCE de taille
    fixe (2 octets, quel que soit le nombre de chiffres de la valeur), pas
    la valeur elle-meme -- chaque player l'expand a l'affichage.

    Appelee aussi par `resolve()` (resultat ignore) pour valider %NOM% au
    moment de la compilation plutot qu'a l'encodage : une stat inconnue doit
    etre signalee comme n'importe quelle autre reference non declaree."""
    def repl(m: re.Match) -> str:
        name = m.group(1)
        if name not in sym.stats:
            raise A2Error(f"%{name}% : stat inconnue "
                          "(ajouter @stat au préambule)", line)
        return chr(M.TXT_STAT_REF) + chr(sym.stats[name])
    return _STAT_REF_RE.sub(repl, text)


def resolve(story: Story) -> list[str]:
    """Valide l'aventure et remplit les index. Renvoie la liste des warnings."""
    warnings: list[str] = []

    _layout_flags(story)

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
    _assign_splash(story)

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
                      (story.n_flag_slots, "flags"),
                      (len(story.assets), "images")):
        if lim > 255 and what != "images":
            raise A2Error(f"trop de {what} (> 255)")
    if len(story.assets) > 0xFFFE:
        raise A2Error("trop d'images (> 65534)")
    return warnings


def _layout_flags(story: Story) -> None:
    """Valide les declarations de flags et leur attribue des emplacements.

    Un flag global occupe un emplacement pour toute la partie. Un flag local
    n'existe que dans son chapitre : les locaux de chapitres differents
    partagent les memes emplacements (le player efface la plage
    [local_base, n_flag_slots) a chaque changement de chapitre). Le budget est
    donc « au plus MAX_LOCAL_FLAGS locaux dans un meme chapitre ».

    Un nom local declare dans plusieurs chapitres est qualifie (`nom@chapitre`)
    pour rester unique dans le reste de la chaine (analyse, simulation, JSON) ;
    `FlagDecl.base` garde le nom ecrit dans la source."""
    for fl in story.flags:
        if not fl.base:
            fl.base = fl.name
    story.flags.sort(key=lambda fl: fl.is_local)      # globaux d'abord (stable)

    globals_ = [fl for fl in story.flags if not fl.is_local]
    locals_ = [fl for fl in story.flags if fl.is_local]

    seen: dict[str, int] = {}
    for fl in globals_:
        if fl.base in seen:
            raise A2Error(f"flag '{fl.base}' déclaré deux fois", fl.line)
        seen[fl.base] = fl.line
    per_chapter: dict[int, dict[str, int]] = {}
    for fl in locals_:
        if fl.base in seen:
            raise A2Error(f"flag local '{fl.base}' : porte le même nom qu'un "
                          "flag global", fl.line)
        chap = per_chapter.setdefault(fl.chapter, {})
        if fl.base in chap:
            raise A2Error(f"flag local '{fl.base}' déclaré deux fois dans le "
                          "même chapitre", fl.line)
        chap[fl.base] = fl.line

    # noms uniques : on qualifie un local des que son nom apparait dans
    # plusieurs chapitres
    chapters_of: dict[str, set[int]] = {}
    for fl in locals_:
        chapters_of.setdefault(fl.base, set()).add(fl.chapter)
    for fl in locals_:
        fl.name = f"{fl.base}@{fl.chapter}" if len(chapters_of[fl.base]) > 1 else fl.base

    slots: dict[str, int] = {fl.name: i for i, fl in enumerate(globals_)}
    story.local_base = len(globals_)
    widest = 0
    for chap_no in sorted(per_chapter):
        in_chapter = [fl for fl in locals_ if fl.chapter == chap_no]
        widest = max(widest, len(in_chapter))
        if len(in_chapter) > M.MAX_LOCAL_FLAGS:
            title = story.chapters[chap_no] if chap_no < len(story.chapters) else ""
            raise A2Error(f"{len(in_chapter)} flags 'local' dans le chapitre "
                          f"« {title} » (> {M.MAX_LOCAL_FLAGS}) : en rendre "
                          "quelques-uns globaux")
        for k, fl in enumerate(in_chapter):
            slots[fl.name] = story.local_base + k
    story.flag_slots = slots
    story.n_flag_slots = story.local_base + widest
    if story.n_flag_slots > M.MAX_FLAGS:
        raise A2Error(f"{story.n_flag_slots} emplacements de flags "
                      f"(> {M.MAX_FLAGS}) : {story.local_base} globaux + "
                      f"{widest} locaux au plus dans un chapitre")


def _assign_splash(story: Story) -> None:
    """Images purement facultatives : celles que seul un @splash utilise."""
    required: set[str] = set()
    for sec in story.sections:
        if sec.image is not None:
            required.add(sec.image)
        if sec.combat is not None and sec.combat.image is not None:
            required.add(sec.combat.image)
    story.optional_assets = {a for a in story.assets if a not in required}


def _check_unique(story: Story) -> None:
    for coll, what in ((story.stats, "stat"), (story.items, "item"),
                       (story.sections, "section")):
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
    if sec.splash is not None:
        sec.splash_asset = asset_index.setdefault(sec.splash, len(asset_index))
    if sec.image is not None:
        if sec.mode == Mode.FULL_TEXT:
            warnings.append(f"ligne {sec.line}: @image ignoré en mode full_text "
                            f"(section '{sec.name}')")
        sec.image_asset = asset_index.setdefault(sec.image, len(asset_index))

    sym.cur_chapter = sec.chapter
    for e in sec.on_enter:
        _resolve_effect(e, sym)
    for e in sec.on_exit:
        _resolve_effect(e, sym)
    for t in sec.texts:
        _resolve_condition(t.cond, sym)
        substitute_stat_refs(t.text, sym, t.line)   # valide %NOM% (résultat ignoré ici)
    for c in sec.choices:
        _resolve_condition(c.cond, sym)
        for e in c.effects:
            _resolve_effect(e, sym)
        if c.target not in sym.sections:
            raise A2Error(f"choix vers une section inconnue: '{c.target}'", c.line)
        c.target_index = sym.sections[c.target]
        substitute_stat_refs(c.label, sym, c.line)

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
        for msg in (cb.win_msg, cb.lose_msg, cb.flee_msg):
            substitute_stat_refs(msg, sym, cb.line)
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
        # de bord — indépendamment de la casse/accentuation affichée. Le
        # player compare à une saisie que `norm_input` (sinput.c) met elle
        # aussi en capitales ASCII (un clavier Apple II ne tape pas d'accent) :
        # la casse/accentuation d'affichage ne doit pas décider si une
        # réponse est acceptée.
        ip.answers = [to_match_key(a).strip() for a in ip.answers]
        substitute_stat_refs(ip.prompt, sym, ip.line)
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
        a.name = sym.flag_ref(a.name, a.line)
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
        e.name = sym.flag_ref(e.name, e.line)
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
