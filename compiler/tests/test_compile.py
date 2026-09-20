"""Tests du compilateur a2c sur examples/demo.adv (round-trip encode/decode)."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from a2c import model as M              # noqa: E402
from a2c.decode import decode, _decode_section, _Reader  # noqa: E402
from a2c.encoder import VERSION, encode_assets, encode_story    # noqa: E402
from a2c.errors import A2Error          # noqa: E402
from a2c.jsonconv import (              # noqa: E402
    adv_to_json, bundle_to_dict, dict_to_bundle, dict_to_story, json_to_adv,
    render_adv, story_to_dict,
)
from a2c.encoder import encode_lang     # noqa: E402
from a2c.translit import normalize_display, to_match_key  # noqa: E402
from a2c.parser import parse, parse_lang  # noqa: E402
from a2c.symbols import resolve         # noqa: E402
from a2c.site import export_images, export_site, hgr_to_image  # noqa: E402
from a2c.webjson import load_ui_strings, resolved_to_dict  # noqa: E402

ADVENTURES_DIR = Path(__file__).resolve().parents[2] / "adventures"
LANG_DIR = Path(__file__).resolve().parents[2] / "lang"
DEMO = ADVENTURES_DIR / "demo_simple" / "demo_simple.adv"
COMBAT_DEMO = ADVENTURES_DIR / "combat_demo" / "combat_demo.adv"


def _compile():
    story = parse(DEMO.read_text(encoding="utf-8"))
    resolve(story)
    return story, decode(encode_story(story)[0])   # STORY0.DAT


def test_header_counts():
    story, d = _compile()
    h = d["header"]
    assert h.version == VERSION          # suit l'encodeur (v4 : index par fichier)
    assert h.n_sections == len(story.sections) == 7
    assert h.n_stats == 3 and h.n_items == 1 and h.n_flags == 2
    assert h.start_section == 0                      # 'lisiere' est en tête


def test_index_offsets_monotonic():
    # v4 : l'index est une liste (file_id, offset, longueur) par section, plus
    # de table d'offsets globale. Un seul fichier ici -> offsets croissants.
    _, d = _compile()
    idx = d["index"]
    assert len(idx) == d["header"].n_sections
    assert d["n_files"] == 1
    offs = [off for (_f, off, _len) in idx]
    assert offs == sorted(offs)                      # croissants
    for (_f, off, ln), (_f2, nxt, _l2) in zip(idx, idx[1:]):
        assert off + ln == nxt                       # corps jointifs


def test_default_state():
    _, d = _compile()
    # stat_table = (init, min, max) dans l'ordre VIE, OR, MORAL
    assert d["stat_table"] == [(10, 0, 10), (0, 0, 99), (5, 0, 10)]
    assert d["stat_names"] == ["VIE", "OR", "MORAL"]
    assert d["item_labels"] == ["Torche"]        # casse du source conservée
    # aucun objet/flag possédé au départ
    assert d["items_default"] == bytes([0])
    assert d["flags_default"] == bytes([0])


def test_ending_section():
    story, d = _compile()
    idx = {s.name: i for i, s in enumerate(story.sections)}
    victoire = d["sections"][idx["victoire"]]
    assert victoire.ending == int(M.Ending.WIN)
    assert victoire.choices == []                    # section terminale


def test_conditional_choice_and_effect():
    story, d = _compile()
    idx = {s.name: i for i, s in enumerate(story.sections)}
    buissons = d["sections"][idx["buissons"]]
    # 1er choix : {not has torche} [Ramasser...] -> lisiere, effets give+add
    cond, effects, target, label = buissons.choices[0]
    assert cond == [[(M.OP_NO_ITEM, 0, 0, 0)]]       # not has torche (item 0)
    assert (M.OP_GIVE_ITEM, 0, 0, 0) in effects      # give torche
    assert (M.OP_STAT_ADD, 2, 2, 0) in effects       # add MORAL(2) 2
    assert target == idx["lisiere"]
    assert label == "Ramasser la torche"         # casse du source conservée


def test_on_enter_effect():
    story, d = _compile()
    idx = {s.name: i for i, s in enumerate(story.sections)}
    gouffre = d["sections"][idx["gouffre"]]
    # @on_enter ~ sub VIE 10 ; ~ sub MORAL 5
    assert (M.OP_STAT_SUB, 0, 10, 0) in gouffre.on_enter   # VIE index 0
    assert (M.OP_STAT_SUB, 2, 5, 0) in gouffre.on_enter    # MORAL index 2


_LOCAL_SRC = """
@title T
@start a
@flag global_x
:: a
* [suite] -> b
@chapter "Un"
@flag q local
:: b
~ set q
* {flag q} [suite] -> c
@chapter "Deux"
@flag q local
@flag r local
:: c
~ set r
* {flag r and flag global_x} [fin] -> w
* {flag q} [fin] -> w
:: w
@ending win
"""


def _err(src, needle):
    try:
        resolve(parse(src))
    except A2Error as e:
        assert needle in str(e), str(e)
    else:
        raise AssertionError(f"une erreur contenant {needle!r} etait attendue")


def test_local_flags():
    # Un local se declare dans son chapitre. Les locaux de chapitres differents
    # partagent les emplacements : le budget est le maximum d'UN chapitre
    # (ici 2 : q et r), pas le total (3). Le player efface
    # [local_base, n_flags) a chaque changement de chapitre.
    story = parse(_LOCAL_SRC)
    resolve(story)
    d = decode(encode_story(story)[0])

    assert story.local_base == 1                     # un seul global : global_x
    assert story.n_flag_slots == 3                   # global_x + 2 emplacements locaux
    assert d["local_base"] == 1
    assert d["header"].n_flags == 3
    # 'q' est declare dans deux chapitres : noms qualifies, emplacement partage
    slots = story.flag_slots
    assert slots["q@1"] == slots["q@2"] == 1
    assert slots["r"] == 2
    assert story.flag_defaults() == [False, False, False]


def test_local_de_meme_nom_dans_deux_chapitres_sont_independants():
    story = parse(_LOCAL_SRC)
    resolve(story)
    by = {s.name: s for s in story.sections}
    assert by["b"].on_enter[0].name == "q@1"
    assert by["c"].choices[1].cond.atoms[0].name == "q@2"


def test_local_utilise_hors_de_son_chapitre_est_refuse():
    _err(_LOCAL_SRC.replace("@flag r local\n", "").replace("~ set r", "~ set p")
         .replace("@flag q local\n:: c", ":: c"), "non déclaré")
    _err(_LOCAL_SRC.replace("@chapter \"Deux\"\n@flag q local\n@flag r local",
                            "@chapter \"Deux\"\n@flag r local"),
         "déclaré dans le chapitre")


def test_local_dans_le_preambule_est_refuse():
    _err("@title T\n@start a\n@flag l local\n:: a\n@ending win\n",
         "à déclarer dans un chapitre")


def test_local_meme_nom_qu_un_global_ou_deux_fois_par_chapitre_est_refuse():
    _err(_LOCAL_SRC.replace("@flag q local\n@flag r local",
                            "@flag global_x local\n@flag r local"),
         "même nom qu'un flag global")
    _err(_LOCAL_SRC.replace("@flag r local", "@flag q local"), "deux fois")


def test_trop_de_locaux_dans_un_chapitre_est_refuse():
    many = "\n".join(f"@flag f{i} local" for i in range(M.MAX_LOCAL_FLAGS + 1))
    _err(f'@title T\n@start a\n@chapter "X"\n{many}\n:: a\n@ending win\n',
         "flags 'local' dans le chapitre")


def test_migrate_locals_deplace_les_declarations_dans_les_chapitres():
    from a2c.migrate_locals import migrate
    old = """@title T
@start a
@flag global_x
@flag q local   # sujet abordé
@flag inutile local
@flag lu_seul local
:: a
* [suite] -> b
@chapter "Un"

:: b
~ set q
* {flag q} [suite] -> c
@chapter "Deux"

:: c
* {flag q and flag lu_seul} [fin] -> w
* {flag global_x} [fin] -> w
:: w
@ending win
"""
    new, notes = migrate(old)
    lines = new.split("\n")
    # plus de local dans le preambule ; une declaration par chapitre qui l'utilise
    assert "@flag q local   # sujet abordé" not in lines
    assert lines.count("@flag q local  # sujet abordé") == 2
    assert lines.index('@chapter "Un"') + 1 == lines.index("@flag q local  # sujet abordé")
    assert "@flag inutile local" not in new
    assert any("inutile" in n and "supprimé" in n for n in notes)
    assert any("lu_seul" in n and "sans y être jamais posé" in n for n in notes)
    # le resultat compile, et migrer deux fois ne change plus rien
    resolve(parse(new))
    assert migrate(new)[0] == new


def test_migrate_locals_refuse_un_local_utilise_avant_le_premier_chapitre():
    from a2c.migrate_locals import migrate
    try:
        migrate("@title T\n@start a\n@flag l local\n:: a\n~ set l\n@ending win\n")
    except A2Error as e:
        assert "avant le premier @chapter" in str(e)
    else:
        raise AssertionError("une erreur etait attendue")


_SPLASH_SRC = """
@title T
@start a
:: a
@splash titre 3
* [suite] -> b
:: b
@splash carte
* [suite] -> c
:: c
@splash toujours always
@image seule
@mode image_text
* [fin] -> w
:: w
@ending win
"""


def test_splash_encodage_decodage():
    story = parse(_SPLASH_SRC)
    resolve(story)
    by = {s.name: s for s in story.sections}
    assert (by["a"].splash_always, by["b"].splash_always, by["c"].splash_always) == (False, False, True)
    # `seule` est requise (un @image), `titre`/`carte`/`toujours` sont facultatives
    assert story.optional_assets == {"titre", "carte", "toujours"}
    d = decode(encode_story(story)[0])
    secs = d["sections"]
    assert (secs[0].splash_asset, secs[0].splash_secs, secs[0].splash_always) == (story.assets.index("titre"), 3, False)
    assert (secs[1].splash_asset, secs[1].splash_secs) == (story.assets.index("carte"), 0)
    assert secs[2].splash_always and secs[2].mode == M.Mode.IMAGE_TEXT
    assert secs[2].image_asset == story.assets.index("seule")
    assert secs[3].splash_asset is None                 # pas de splash : rien d'encode en plus


def test_splash_ne_coute_rien_sans_splash():
    plain = parse(_SPLASH_SRC.replace("@splash titre 3\n", "").replace("@splash carte\n", "")
                  .replace("@splash toujours always\n", ""))
    resolve(plain)
    full = parse(_SPLASH_SRC)
    resolve(full)
    assert len(encode_story(full)[0]) - len(encode_story(plain)[0]) == 3 * 3   # u16 + u8 par splash


def test_splash_erreurs():
    _err("@title T\n@start a\n:: a\n@splash\n@ending win\n", "attend un id")
    _err("@title T\n@start a\n:: a\n@splash x 99\n@ending win\n", "hors 0..31")
    _err("@title T\n@start a\n:: a\n@splash x bof\n@ending win\n", "inattendu")
    _err("@title T\n@start a\n:: a\n@splash x\n@splash y\n@ending win\n", "deux fois")


def test_splash_roundtrip_json_et_web():
    adv = _SPLASH_SRC
    assert "@splash titre 3" in json_to_adv(adv_to_json(adv))
    assert "@splash toujours always" in json_to_adv(adv_to_json(adv))
    story = parse(adv)
    resolve(story)
    _, ui_strings, _ = parse_lang((LANG_DIR / "fr.lng").read_text(encoding="utf-8"))
    web = resolved_to_dict(story, ui_strings)
    assert web["sections"][0]["splash"] == {"asset": story.assets.index("titre"), "secs": 3, "always": False}
    assert web["sections"][3]["splash"] is None


def test_images_map_marque_les_images_facultatives():
    with tempfile.TemporaryDirectory() as tmp:
        adv = Path(tmp) / "t.adv"
        adv.write_text(_SPLASH_SRC, encoding="utf-8")
        from a2c.cli import main
        assert main([str(adv), "-o", str(Path(tmp) / "out")]) == 0
        lines = (Path(tmp) / "out" / "IMAGES.MAP").read_text().splitlines()
    assert "00 titre optional" in lines
    assert "03 seule" in lines                           # requise : pas de 3e colonne


def _dnf_eval(cond, flags):
    """Evalue une condition normalisee sur un dict {flag: bool} (items = 'h_x')."""
    def atom(a):
        if a.op in ("flag", "has"):
            return flags.get(a.name, False)
        if a.op in ("not_flag", "not_has"):
            return not flags.get(a.name, False)
        raise AssertionError(a.op)
    return not cond.clauses or any(all(atom(a) for a in cl) for cl in cond.clauses)


def test_conditions_normalisees_equivalentes_a_l_expression_ecrite():
    """La forme OU de ET doit valoir exactement l'expression source, pour toutes
    les valeurs des drapeaux (python : mots-cles `and`/`or`/`not` identiques)."""
    import itertools
    from a2c.cond import parse_condition
    names = ["a", "b", "c", "d"]
    exprs = [
        "flag a", "not flag a", "flag a and has b", "flag a or flag b or has c",
        "(flag a or flag b) and has c", "not (flag a and has b)",
        "not (flag a or flag b) and flag c", "(flag a and flag b) or (flag c and not flag d)",
        "not ((flag a or flag b) and (flag c or flag d))",
        "(flag a or (flag b and flag c)) and not (has d or flag a)",
    ]
    for expr in exprs:
        cond = parse_condition(expr, 1)
        py = (expr.replace("flag ", "F.").replace("has ", "H.")).replace("not ", "not ")
        for values in itertools.product([False, True], repeat=8):
            f = dict(zip([f"{n}" for n in names], values[:4]))
            h = dict(zip([f"{n}" for n in names], values[4:]))
            # les items et les flags ont ici des espaces de noms distincts
            class NS:
                def __init__(self, d): self.__dict__.update(d)
            want = eval(py, {}, {"F": NS(f), "H": NS(h)})
            got = not cond.clauses or any(
                all((f if a.op in ("flag", "not_flag") else h).get(a.name, False)
                    == (a.op in ("flag", "has")) for a in cl) for cl in cond.clauses)
            assert got == want, (expr, values)


def test_stat_not_inverse_la_comparaison():
    from a2c.cond import parse_condition
    for op, inv in ((">=", M.Cmp.LT), ("<", M.Cmp.GE), ("==", M.Cmp.NE), ("<=", M.Cmp.GT)):
        c = parse_condition(f"not stat X {op} 3", 1)
        assert c.clauses[0][0].cmp == inv


def test_conditions_erreurs():
    from a2c.cond import parse_condition
    for src, needle in (
        ("flag a and flag b or flag c", "sans parenthèses"),
        ("(flag a or flag b", "parenthèse fermante"),
        ("flag a flag b", "inattendu"),
        ("flag a and", "incomplète"),
        ("flag a and not flag a", "toujours fausse"),
        ("else", "sans condition précédente"),
        ("(" + " or ".join(f"flag a{i}" for i in range(3)) + ") and ("
         + " or ".join(f"flag b{i}" for i in range(3)) + ") and ("
         + " or ".join(f"flag c{i}" for i in range(3)) + ")", "trop complexe"),
    ):
        try:
            parse_condition(src, 1)
        except A2Error as e:
            assert needle in str(e), (src, str(e))
        else:
            raise AssertionError(f"erreur attendue pour {src!r}")


_ELSE_SRC = """
@title T
@start a
@flag x
@flag y
@stat N 0 0 9
:: a
{flag x} Un.
{else and flag y} Deux.
{else} Trois.
Toujours.
{flag y} Encore.
* {flag x} [un] -> b
* {else and stat N >= 3} [deux] -> b
* {else} [trois] -> b
  ~ {flag x} set y
  ~ {else} set x
:: b
@ending win
"""


def test_else_enchaine_les_conditions_voisines():
    story = parse(_ELSE_SRC)
    resolve(story)
    a = story.sections[0]
    t = [tx.cond for tx in a.texts]
    for x in (False, True):
        for y in (False, True):
            f = {"x": x, "y": y}
            got = [_dnf_eval(c, f) for c in t[:3]]
            assert got == [x, (not x) and y, (not x) and (not y)], f
    # un texte sans condition interrompt la suite : `{flag y}` recommence
    assert _dnf_eval(t[4], {"y": True}) and not _dnf_eval(t[4], {"y": False})
    # choix : le 3e est « ni x, ni (non x et N >= 3) »
    c3 = a.choices[2].cond
    assert len(c3.clauses) == 1 and {(q.op, q.name) for q in c3.clauses[0]} == {("not_flag", "x"), ("stat", "N")}
    # effets d'un meme choix : `{else}` complete `{flag x}`
    fx = a.choices[2].effects
    assert [e.cond.clauses[0][0].op for e in fx] == ["flag", "not_flag"]
    _err(_ELSE_SRC.replace("{flag x} Un.", "Un."), "sans condition précédente")


def test_condition_vide_coute_un_octet_et_un_ou_est_encode_en_clauses():
    from a2c.encoder import _encode_cond
    from a2c.cond import parse_condition
    story = parse("@title T\n@start a\n@flag a\n@flag b\n@item i\n:: a\n@ending win\n")
    resolve(story)
    from a2c.symbols import Symbols
    sym = Symbols(story)
    assert _encode_cond(M.Condition(), sym) == b"\x00"
    et = _encode_cond(parse_condition("flag a and has i", 1), sym)
    ou = _encode_cond(parse_condition("flag a or flag b", 1), sym)
    assert et[:2] == b"\x01\x02"                     # 1 clause de 2 atomes
    assert ou[:2] == b"\x02\x01" and len(ou) == 1 + 2 * (1 + 4)   # 2 clauses d'1 atome


def test_conditions_composees_aller_retour_json_binaire_web():
    src = """@title T
@start a
@flag a
@flag b
@flag c
@item i
:: a
{(flag a or flag b) and has i} Un.
{else} Deux.
* {not (flag a and flag b)} [go] -> w
:: w
@ending win
"""
    text = json_to_adv(adv_to_json(src))
    assert "{(flag a or flag b) and has i} Un." in text
    assert "{else} Deux." in text
    assert "{not (flag a and flag b)}" in text
    story = parse(src)
    resolve(story)
    d = decode(encode_story(story)[0])
    conds = [t[0] for t in d["sections"][0].texts]
    assert len(conds[0]) == 2 and len(conds[1]) == 2     # (a et i) ou (b et i) ; else : non i, ou (non a et non b)
    _, ui_strings, _ = parse_lang((LANG_DIR / "fr.lng").read_text(encoding="utf-8"))
    web = resolved_to_dict(story, ui_strings)
    assert _json_section_to_tuple(web["sections"][0])[5][0][0] == conds[0]


def test_local_flag_rejects_on():
    src = _LOCAL_SRC.replace("@flag r local", "@flag r on local")
    try:
        parse(src)
    except Exception as e:
        assert "local" in str(e)
    else:
        raise AssertionError("un flag 'local on' aurait du etre refuse")


def test_jsonconv_roundtrip_toutes_les_aventures():
    """adv -> json -> adv -> compile doit reproduire le meme binaire, pour
    chaque aventure du depot (pas seulement demo_simple)."""
    for adv in sorted(ADVENTURES_DIR.glob("*/*.adv")):
        src = adv.read_text(encoding="utf-8")
        story1 = parse(src)
        resolve(story1)
        bin1 = encode_story(story1)
        assets1 = encode_assets(story1)

        data = story_to_dict(story1)
        story2 = dict_to_story(data)
        story3 = parse(render_adv(story2))
        resolve(story3)

        assert encode_story(story3) == bin1, adv
        assert encode_assets(story3) == assets1, adv


def test_jsonconv_json_est_un_point_fixe():
    """json -> adv -> json doit redonner exactement le meme dict : le JSON
    n'a pas de perte propre (au-dela de ce que le DSL texte perd deja)."""
    src = DEMO.read_text(encoding="utf-8")
    data = adv_to_json(src)
    assert adv_to_json(json_to_adv(data)) == data


def test_jsonconv_conserve_les_commentaires():
    """Les commentaires '#' (tete de fichier, en-tete de @stat/@item/@flag,
    de section, de choix, et commentaires de fin de ligne) survivent a un
    aller-retour adv -> json -> adv -> json, pour toutes les aventures."""
    for adv in sorted(ADVENTURES_DIR.glob("*/*.adv")):
        src = adv.read_text(encoding="utf-8")
        data = adv_to_json(src)
        assert adv_to_json(json_to_adv(data)) == data, adv

    src = COMBAT_DEMO.read_text(encoding="utf-8")
    data = adv_to_json(src)
    assert data["comments"]["title"]["lead"] == [
        "Démo de combat (v1) : montre le PRNG, l'écran de combat, l'arme portée.",
        "Prose originale.",
    ]
    assert data["comments"]["combat_basedmg"]["trail"] == "degats du heros a mains nues"
    stat = next(s for s in data["stats"] if s["name"] == "HABILETE")
    assert stat["trail"] == "sert d'attaque au combat (2d6 + HABILETE)"


def test_jsonconv_bundle_global_adv_et_lang():
    """Le paquet 'global' (--bundle) embarque le socle .lng complet ; son
    aller-retour reproduit exactement le meme STORY.DAT et le meme APP.LNG
    que la compilation directe des fichiers d'origine."""
    story1 = parse(COMBAT_DEMO.read_text(encoding="utf-8"))
    resolve(story1)
    bin1 = encode_story(story1)
    code1, ui1, _ = parse_lang((LANG_DIR / "fr.lng").read_text(encoding="utf-8"))
    lng1 = encode_lang(code1, ui1)

    bundle = bundle_to_dict(COMBAT_DEMO.read_text(encoding="utf-8"), lang_dir=LANG_DIR)
    assert bundle["lang"]["code"] == "fr"
    assert len(bundle["lang"]["ui"]) == 48

    adv_text, lang_text = dict_to_bundle(bundle)
    story2 = parse(adv_text)
    resolve(story2)
    assert encode_story(story2) == bin1

    code2, ui2, _ = parse_lang(lang_text)
    assert encode_lang(code2, ui2) == lng1


def _decode_all_sections(files: list[bytes]) -> list:
    """Decode TOUTES les sections de 1..N STORYnn.DAT dans l'ordre global.
    decode.decode() ne sait lire qu'un seul fichier (les sections des autres
    fichiers reviennent a None) ; ce helper de test suit file_first pour
    parcourir chaque fichier a son tour, comme le ferait un vrai player
    multi-fichiers (ou story.ts cote web)."""
    d0 = decode(files[0])
    n_files = d0["n_files"]
    r = _Reader(files[0], d0["header"].index_offset)
    file_first = [r.u16() for _ in range(n_files + 1)]

    sections = []
    for f in range(n_files):
        buf = files[f]
        base = r.pos if f == 0 else 0   # position apres file_first, fichier 0 seulement
        rr = _Reader(buf, base)
        count = rr.u16()
        offs = [rr.u16() for _ in range(count + 1)]
        for k in range(count):
            sections.append(_decode_section(_Reader(buf, offs[k])))
    assert len(sections) == d0["header"].n_sections
    return sections


def _json_section_to_tuple(sj: dict) -> tuple:
    """Convertit une section du JSON web (dicts) vers la meme forme (tuples)
    que decode.decode() renvoie, pour pouvoir les comparer terme a terme."""
    def cond(c):
        return [[(a["op"], a["a0"], a["a1"], a["a2"]) for a in clause]
                for clause in c["clauses"]]

    def fx(effects):
        return [(e["op"], e["a0"], e["a1"], e["a2"]) for e in effects]

    # normalize_display() : seule difference volontaire vs le binaire (cf.
    # webjson.py) -- le JSON garde la typographie d'origine (ligatures,
    # guillemets courbes), le DOM sait l'afficher sans aide. On la reapplique
    # a chaque chaine affichee pour comparer le reste a l'identique. Les
    # reponses @ask (answers) n'en ont pas besoin : deja normalisees en
    # ASCII majuscule par resolve() (to_match_key), en amont des deux sorties.
    nd = normalize_display
    combat = None
    if sj["combat"]:
        cb = sj["combat"]
        combat = dict(name=nd(cb["name"]), att=cb["att"], hp=cb["hp"], dmg=cb["dmg"],
                     armor=cb["armor"], image=cb["eimg"], win=cb["win"],
                     lose=cb["lose"], flee=cb["flee"],
                     win_fx=fx(cb["winFx"]), lose_fx=fx(cb["loseFx"]),
                     flee_fx=fx(cb["fleeFx"]), win_msg=nd(cb["winMsg"]),
                     lose_msg=nd(cb["loseMsg"]), flee_msg=nd(cb["fleeMsg"]))
    inp = None
    if sj["input"]:
        ip = sj["input"]
        inp = dict(prompt=nd(ip["prompt"]), maxlen=ip["maxlen"], answers=ip["answers"],
                  correct=ip["correct"], wrong=ip["wrong"],
                  correct_fx=fx(ip["correctFx"]), wrong_fx=fx(ip["wrongFx"]))
    texts = [(cond(t["cond"]), t["style"], nd(t["text"])) for t in sj["texts"]]
    choices = [(cond(c["cond"]), fx(c["effects"]), c["target"], nd(c["label"]))
              for c in sj["choices"]]
    return (sj["mode"], sj["ending"], sj["image"], fx(sj["onEnter"]),
           fx(sj["onExit"]), texts, choices, combat, inp)


def test_webjson_identique_au_binaire_toutes_aventures():
    """resolved_to_dict (webjson.py) doit contenir EXACTEMENT la meme
    information que le STORY.DAT compile -- section par section, sur toutes
    les aventures du depot (y compris multi-fichiers/multi-chapitres) :
    c'est la garantie que le player web (JSON) et les players natifs
    (binaire) jouent la meme partie."""
    _, ui_strings, _ = parse_lang((LANG_DIR / "fr.lng").read_text(encoding="utf-8"))
    for adv in sorted(ADVENTURES_DIR.glob("*/*.adv")):
        story = parse(adv.read_text(encoding="utf-8"))
        resolve(story)
        files = encode_story(story)
        bin_sections = _decode_all_sections(files)

        web = resolved_to_dict(story, ui_strings)
        assert web["start"] == story.start_index, adv
        assert web["localBase"] == story.local_base, adv
        assert len(web["sections"]) == len(bin_sections), adv
        d0 = decode(files[0])
        assert normalize_display(web["title"]) == d0["title"], adv
        assert [normalize_display(s["name"]) for s in web["stats"]] == d0["stat_names"], adv
        assert [normalize_display(it["label"]) for it in web["items"]] == d0["item_labels"], adv

        for i, (bin_sec, web_sec) in enumerate(zip(bin_sections, web["sections"])):
            got = _json_section_to_tuple(web_sec)
            want = (bin_sec.mode, bin_sec.ending, bin_sec.image_asset,
                   bin_sec.on_enter, bin_sec.on_exit, bin_sec.texts,
                   bin_sec.choices, bin_sec.combat, bin_sec.input)
            assert got == want, f"{adv} section {i} ({story.sections[i].name})"
            sp = web_sec["splash"]
            if bin_sec.splash_asset is None:
                assert sp is None, adv
            else:
                assert sp == {"asset": bin_sec.splash_asset, "secs": bin_sec.splash_secs,
                              "always": bin_sec.splash_always}, adv


def test_normalize_display_conserve_accents_et_casse():
    """Le texte affiché garde accents et casse : c'est scr.c qui adapte."""
    src = "Où est l'Œuf, Éléphant ?"
    assert normalize_display(src) == "Où est l'Oeuf, Éléphant ?"
    normalize_display(src).encode("latin-1")   # ne doit jamais lever


def test_to_match_key_toujours_ascii_majuscule():
    """Un clavier Apple II ne tape pas d'accent : la clé de comparaison l'est."""
    src = "Où est l'Œuf, Éléphant ?"
    assert to_match_key(src) == "OU EST L'OEUF, ELEPHANT ?"


_STAT_REF_ADV = """\
@title Test
@start intro
@stat JOURS 5 0 10

:: intro
Il vous reste %JOURS% jours.
* [Attendre %JOURS% jours restants] -> intro
"""


def test_stat_ref_resolu_identique_en_binaire_et_en_json():
    """%NOM% (texte narratif ET libelle de choix) s'encode en TXT_STAT_REF +
    index de stat, meme octets des deux cotes (STORY.DAT et story.json) --
    la valeur reelle n'est jamais connue a la compilation, seule une
    reference de taille fixe l'est (cf. symbols.substitute_stat_refs)."""
    story = parse(_STAT_REF_ADV)
    resolve(story)

    d = decode(encode_story(story)[0])
    sec = d["sections"][0]
    bin_text = sec.texts[0][2]
    bin_label = sec.choices[0][3]
    expected_text = "Il vous reste " + chr(M.TXT_STAT_REF) + chr(0) + " jours."
    expected_label = "Attendre " + chr(M.TXT_STAT_REF) + chr(0) + " jours restants"
    assert bin_text == expected_text
    assert bin_label == expected_label

    ui = {k: v for k, v in M.UI_KEYS}
    j = resolved_to_dict(story, ui)
    assert j["sections"][0]["texts"][0]["text"] == expected_text
    assert j["sections"][0]["choices"][0]["label"] == expected_label


def test_stat_ref_inconnue_leve_une_erreur_a_la_resolution():
    bad = _STAT_REF_ADV.replace("%JOURS%", "%NEXISTEPAS%", 1)
    story = parse(bad)
    try:
        resolve(story)
        assert False, "aurait du lever A2Error"
    except A2Error as e:
        assert "NEXISTEPAS" in str(e)


def test_stat_ref_dans_le_prompt_ask_et_ignore_dans_les_reponses():
    """%NOM% fonctionne dans le prompt @ask (affiche au joueur) mais n'a
    aucun sens dans une reponse @answer (jamais affichee, sert seulement a
    comparer la saisie du joueur) : jamais substitue, jamais valide -- une
    reponse peut donc legitimement contenir un '%' litteral sans que ce
    soit une erreur."""
    adv = """\
@title Test
@start q
@stat JOURS 5 0 10

:: q
Question.
@ask "Il reste %JOURS% jours. Combien font 10% de 50 ?"
@answer 5
@answer 5%
@correct q
@wrong q
"""
    story = parse(adv)
    resolve(story)   # ne doit pas lever, meme si '%' litteral dans une reponse

    ui = {k: v for k, v in M.UI_KEYS}
    j = resolved_to_dict(story, ui)
    prompt = j["sections"][0]["input"]["prompt"]
    # seul %JOURS% (nom valide) est substitue ; le '%' isole de "10% de 50"
    # ne matche pas %IDENT% et reste tel quel, sans echappement necessaire.
    assert prompt == "Il reste " + chr(M.TXT_STAT_REF) + chr(0) + " jours. Combien font 10% de 50 ?"
    assert j["sections"][0]["input"]["answers"] == ["5", "5%"]


def _png(path: Path, size=(1400, 960), color=(200, 60, 20)) -> None:
    from PIL import Image
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.new("RGB", size, color).save(path)


def _has_pillow() -> bool:
    try:
        import PIL  # noqa: F401
    except ImportError:
        return False
    return True


def test_export_images_convertit_les_trouvees_et_signale_les_manquantes():
    """IMGnn.webp numerotees par ordre de story.assets, redimensionnees ; une
    image absente n'est qu'un avertissement (jamais une exception)."""
    if not _has_pillow():
        return
    from PIL import Image
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        src_dir = tmp / "web"
        _png(src_dir / "URNE.png")
        out_dir = tmp / "out" / "img"

        count, size, warnings = export_images(["urne", "musee", "penthouse"],
                                              src_dir, out_dir, {"penthouse"})

        assert count == 1 and size == (out_dir / "IMG00.webp").stat().st_size
        assert Image.open(out_dir / "IMG00.webp").size == (960, 658)
        assert not (out_dir / "IMG01.webp").exists()
        assert len(warnings) == 1 and "musee" in warnings[0]   # penthouse : facultative


def test_export_images_rien_a_faire_sans_assets():
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        out_dir = tmp / "out" / "img"

        assert export_images([], tmp / "web", out_dir) == (0, 0, [])
        assert not out_dir.exists()  # jamais cree si l'aventure n'a aucune image


def test_hgr_couleurs():
    if not _has_pillow():
        return
    data = bytearray(8192)
    data[0] = 0b0000001        # x=0, palette 0, colonne paire  : violet
    data[1] = 0b0000100        # x=9, palette 0, colonne impaire : vert
    data[2] = 0b10000001       # x=14, palette 1, colonne paire  : bleu
    data[3] = 0b10000010       # x=22, palette 1, colonne paire  : bleu
    data[4] = 0b0000110        # x=29 et x=30 voisins : blancs
    img = hgr_to_image(bytes(data))
    assert img.size == (280, 192)
    assert img.getpixel((0, 0)) == (255, 68, 253)
    assert img.getpixel((9, 0)) == (20, 245, 60)
    assert img.getpixel((14, 0)) == (20, 207, 253)
    assert img.getpixel((29, 0)) == img.getpixel((30, 0)) == (255, 255, 255)
    assert img.getpixel((1, 0)) == (0, 0, 0)
    # 2e ligne de l'ecran : adresse entrelacee 0x400
    data2 = bytearray(8192)
    data2[0x400] = 0b0000001
    assert hgr_to_image(bytes(data2)).getpixel((0, 1)) == (255, 68, 253)


_SITE_ADV = """\
@title Aventure de test
@author Quelqu'un
@description "Une courte presentation."
@version 1.2
@start a

:: a
@image scene
Un texte.
* [Fin] -> b

:: b
@ending win
Fin.
"""


def test_export_site_catalogue_et_fichiers():
    if not _has_pillow():
        return
    import json
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        advs = tmp / "advs"
        (advs / "test").mkdir(parents=True)
        (advs / "test" / "test.adv").write_text(_SITE_ADV, encoding="utf-8")
        _png(advs / "test" / "img" / "web" / "SCENE.png")
        _png(advs / "test" / "img" / "web" / "MENU.png")
        out = tmp / "site"
        (out / "adventures" / "vieille").mkdir(parents=True)   # dossier perime

        catalog, warnings = export_site(["test"], out, advs)

        assert warnings == []
        (e,) = catalog["adventures"]
        assert e["id"] == "test" and e["title"] == "Aventure de test"
        assert e["author"] == "Quelqu'un" and e["version"] == "1.2"
        assert e["description"] == "Une courte presentation."
        assert e["sections"] == 2 and e["images"] == 1
        base = f"adventures/test/{e['hash']}"
        assert e["base"] == base and e["story"] == f"{base}/story.json"
        assert e["cover"] == f"{base}/cover.webp"
        assert e["files"] == [f"{base}/cover.webp", f"{base}/img/IMG00.webp",
                              f"{base}/story.json"]
        assert all((out / f).exists() for f in e["files"])
        assert e["bytes"] == sum((out / f).stat().st_size for f in e["files"])
        assert json.loads((out / "catalog.json").read_text(encoding="utf-8")) == catalog
        assert not (out / "adventures" / "vieille").exists()
        # relance : meme contenu, donc meme dossier ; l'ancien dossier disparait
        stale = out / "adventures" / "test" / "0000000000"
        stale.mkdir()
        catalog2, _ = export_site(["test"], out, advs)
        assert catalog2 == catalog and not stale.exists()
        # un changement de source change le hash, donc le dossier
        (advs / "test" / "test.adv").write_text(
            _SITE_ADV.replace("Un texte.", "Un autre texte."), encoding="utf-8")
        catalog3, _ = export_site(["test"], out, advs)
        e3 = catalog3["adventures"][0]
        assert e3["hash"] != e["hash"] and not (out / e["base"]).exists()


def test_description_directive_et_aller_retour_json():
    story = parse(_SITE_ADV)
    assert story.description == "Une courte presentation."
    back = dict_to_story(story_to_dict(story))
    assert back.description == story.description
    assert '@description "Une courte presentation."' in render_adv(back)
    assert parse("@title T\n@start a\n:: a\nx\n").description == ""


def test_lang_web_surcharge_le_socle():
    web = load_ui_strings("fr")
    base = parse_lang((LANG_DIR / "fr.lng").read_text(encoding="utf-8"))[1]
    assert web["menu_new"] == "Nouvelle partie" and base["menu_new"] == "COMMENCER"
    assert set(web) == set(base)                     # aucune cle perdue
    assert web["opt_speaker"] == base["opt_speaker"]  # cle non surchargee : socle
    with tempfile.TemporaryDirectory() as tmp:       # sans dossier web/ : socle seul
        d = Path(tmp)
        (d / "fr.lng").write_text((LANG_DIR / "fr.lng").read_text(encoding="utf-8"),
                                  encoding="utf-8")
        assert load_ui_strings("fr", d)["menu_new"] == "COMMENCER"


_STAT16_ADV = """\
@title Test
@start q
@stat ARGENT 0 0 65535

:: q
Vous avez %ARGENT% pieces.
* {stat ARGENT > 300} [Grosse fortune] -> q
  ~ add ARGENT 20000
* [Petit pecule] -> q
  ~ setmax ARGENT 40000
"""


def test_stat_16bits_valeurs_au_dela_de_255_binaire_et_json():
    """@stat ... 0 0 65535, ~ add ARGENT 20000 et {stat ARGENT > 300} doivent
    survivre a l'encodage binaire (atome de condition sur 5 o, effet 'add'
    avec valeur scindee bas/haut sur a1/a2, cf. encoder.py) et redonner
    exactement la meme valeur cote JSON (webjson.py, qui ne scinde rien --
    un seul champ JS, cf. decode.py:_decode_effect_atom qui recombine pour
    comparer les deux a armes egales)."""
    story = parse(_STAT16_ADV)
    resolve(story)

    d = decode(encode_story(story)[0])
    assert d["stat_table"] == [(0, 0, 65535)]
    sec = d["sections"][0]

    cond, effects, _target, _label = sec.choices[0]
    assert cond == [[(M.OP_STAT_CMP, 0, int(M.Cmp.GT), 300)]]
    assert (M.OP_STAT_ADD, 0, 20000, 0) in effects

    _cond2, effects2, _t2, _l2 = sec.choices[1]
    assert (M.OP_STAT_SETMAX, 0, 40000, 0) in effects2

    ui = {k: v for k, v in M.UI_KEYS}
    web = resolved_to_dict(story, ui)
    wcond = web["sections"][0]["choices"][0]["cond"]["clauses"][0][0]
    assert (wcond["op"], wcond["a0"], wcond["a1"], wcond["a2"]) == (M.OP_STAT_CMP, 0, int(M.Cmp.GT), 300)
    wfx = web["sections"][0]["choices"][0]["effects"][0]
    assert (wfx["op"], wfx["a0"], wfx["a1"]) == (M.OP_STAT_ADD, 0, 20000)


def test_stat_hors_0_65535_leve_une_erreur():
    bad = _STAT16_ADV.replace("@stat ARGENT 0 0 65535", "@stat ARGENT 0 0 70000")
    try:
        parse(bad)
        assert False, "aurait du lever A2Error"
    except A2Error as e:
        assert "65535" in str(e)


def test_effet_stat_hors_0_65535_leve_une_erreur():
    bad = _STAT16_ADV.replace("~ add ARGENT 20000", "~ add ARGENT 70000")
    try:
        parse(bad)
        assert False, "aurait du lever A2Error"
    except A2Error as e:
        assert "65535" in str(e)


# --- modeles de sections (@template / @use) --------------------------------

_TPL_HEAD = """@title Modeles
@start hub
@stat FORCE 5
"""

_TPL_ADV = _TPL_HEAD + """
@template carnet(ret, nom)
:: carnet_@ret
Le carnet de @nom est ouvert.
* [Fermer le carnet] -> @ret
@end

:: hub
Un carrefour.
* [Ouvrir le carnet] -> carnet_hub
* [Aller plus loin] -> autre

@use carnet(hub, "Elise Martin")

:: autre
Un autre lieu.
* [Ouvrir le carnet] -> carnet_autre
* [Revenir] -> hub

@use carnet(autre, Paul)
"""

_TPL_FLAT = _TPL_HEAD + """
:: hub
Un carrefour.
* [Ouvrir le carnet] -> carnet_hub
* [Aller plus loin] -> autre

:: carnet_hub
Le carnet de Elise Martin est ouvert.
* [Fermer le carnet] -> hub

:: autre
Un autre lieu.
* [Ouvrir le carnet] -> carnet_autre
* [Revenir] -> hub

:: carnet_autre
Le carnet de Paul est ouvert.
* [Fermer le carnet] -> autre
"""


def _tpl_error(src):
    try:
        parse(src)
    except A2Error as e:
        return e
    assert False, "aurait du lever A2Error"


def test_template_equivaut_au_texte_developpe():
    a = parse(_TPL_ADV)
    b = parse(_TPL_FLAT)
    resolve(a)
    resolve(b)
    assert [s.name for s in a.sections] == [s.name for s in b.sections]
    assert encode_story(a)[0] == encode_story(b)[0]


def test_template_sans_parametre_et_nom_colle_avec_accolades():
    src = _TPL_HEAD + """
@template piege
:: piege
Une dalle.
* [Fuir] -> hub
@end

@template etage(n)
:: etage_@{n}_bis
Etage @n.
* [Sortir] -> hub
@end

:: hub
Debut.
* [Piege] -> piege
* [Etage] -> etage_3_bis

@use piege
@use etage(3)
"""
    story = parse(src)
    assert [s.name for s in story.sections] == ["hub", "piege", "etage_3_bis"]
    assert story.sections[2].texts[0].text == "Etage 3."


def test_template_utilisable_avant_sa_definition_et_imbrique():
    src = _TPL_HEAD + """
:: hub
Debut.
* [Aide] -> aide_hub

@use aide(hub)

@template aide(ret)
:: aide_@ret
@use ligne(@ret)
* [Retour] -> @ret
@end

@template ligne(x)
Aide pour @x.
@end
"""
    # ligne() n'a pas de section : le corps de aide contient un texte issu de ligne
    story = parse(src)
    assert story.sections[1].name == "aide_hub"
    assert story.sections[1].texts[0].text == "Aide pour hub."


def test_template_chapitres_locaux_par_instance():
    src = _TPL_HEAD.replace("@start hub", "@start a") + """
@template lieu(id)
:: @id
{flag vu} Deja vu.
~ set vu
* [Suite] -> @id
@end

@chapter "Un"
@flag vu local
@use lieu(a)
@chapter "Deux"
@flag vu local
@use lieu(b)
"""
    story = parse(src)
    assert [s.chapter for s in story.sections] == [1, 2]
    resolve(story)          # la reference a 'vu' se resout dans chaque chapitre


def test_template_erreurs():
    e = _tpl_error(_TPL_HEAD + "@use inconnu(a)\n:: hub\nx\n")
    assert "inconnu" in e.message and e.line == 4

    e = _tpl_error(_TPL_ADV.replace("@use carnet(autre, Paul)", "@use carnet(autre)"))
    assert "2 attendu" in e.message

    e = _tpl_error(_TPL_HEAD + "@template t(a)\n:: x\nfoo\n@end\n")
    assert "jamais utilisé" in e.message and e.line == 4

    e = _tpl_error(_TPL_HEAD + "@template t\n:: x\nfoo\n")
    assert "sans @end" in e.message

    e = _tpl_error(_TPL_HEAD + "@end\n:: x\nfoo\n")
    assert "sans @template" in e.message

    e = _tpl_error(_TPL_HEAD + "@template t\n@template u\n@end\n@end\n")
    assert "imbriqué" in e.message

    e = _tpl_error(_TPL_HEAD + "@template t(image)\n:: x\n@image\n@end\n")
    assert "directive" in e.message

    e = _tpl_error(_TPL_HEAD + "@template t\n@chapter \"x\"\n:: x\n@end\n")
    assert "@chapter" in e.message

    e = _tpl_error(_TPL_HEAD + "@template t\n:: x\ny\n@end\n@template t\n@end\n")
    assert "déjà défini" in e.message

    e = _tpl_error(_TPL_HEAD + "@template t(a)\n@use t(@a)\n:: x\n@end\n@use t(1)\n")
    assert "récursif" in e.message


def test_template_erreur_dans_un_modele_pointe_l_use():
    src = _TPL_HEAD + """
@template t(dest)
:: s_@dest
* [Va] -> @dest %%
@end

@use t(hub)
"""
    e = _tpl_error(src)
    assert e.line == 10
    assert "modèle 't', ligne 7" in e.message


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    for fn in fns:
        fn()
        print(f"ok  {fn.__name__}")
    print(f"\n{len(fns)} tests passés.")
