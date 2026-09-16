"""Tests du compilateur a2c sur examples/demo.adv (round-trip encode/decode)."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from a2c import model as M              # noqa: E402
from a2c.decode import decode, _decode_section, _Reader  # noqa: E402
from a2c.encoder import VERSION, encode_assets, encode_story    # noqa: E402
from a2c.jsonconv import (              # noqa: E402
    adv_to_json, bundle_to_dict, dict_to_bundle, dict_to_story, json_to_adv,
    render_adv, story_to_dict,
)
from a2c.encoder import encode_lang     # noqa: E402
from a2c.translit import normalize_display, to_match_key  # noqa: E402
from a2c.parser import parse, parse_lang  # noqa: E402
from a2c.symbols import resolve         # noqa: E402
from a2c.webjson import resolved_to_dict  # noqa: E402

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
    assert cond == [(M.OP_NO_ITEM, 0, 0, 0)]         # not has torche (item 0)
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


def test_local_flags():
    # Les flags `local` sont repousses en fin de table et l'en-tete porte
    # local_base (= nb de globaux). Le player efface [local_base, n_flags).
    src = DEMO.read_text(encoding="utf-8").replace(
        "@flag a_fouille", "@flag q_topic local\n@flag a_fouille", 1)
    story = parse(src)
    resolve(story)
    d = decode(encode_story(story)[0])

    names = [fl.name for fl in story.flags]
    assert names[-1] == "q_topic"                    # le local passe en dernier
    assert story.local_base == len(names) - 1
    assert d["local_base"] == story.local_base
    assert d["header"].n_flags == len(names)


def test_local_flag_rejects_on():
    src = DEMO.read_text(encoding="utf-8").replace(
        "@flag a_fouille", "@flag mauvais on local\n@flag a_fouille", 1)
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
        return [(a["op"], a["a0"], a["a1"], a["a2"]) for a in c["atoms"]]

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


def test_normalize_display_conserve_accents_et_casse():
    """Le texte affiché garde accents et casse : c'est scr.c qui adapte."""
    src = "Où est l'Œuf, Éléphant ?"
    assert normalize_display(src) == "Où est l'Oeuf, Éléphant ?"
    normalize_display(src).encode("latin-1")   # ne doit jamais lever


def test_to_match_key_toujours_ascii_majuscule():
    """Un clavier Apple II ne tape pas d'accent : la clé de comparaison l'est."""
    src = "Où est l'Œuf, Éléphant ?"
    assert to_match_key(src) == "OU EST L'OEUF, ELEPHANT ?"


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    for fn in fns:
        fn()
        print(f"ok  {fn.__name__}")
    print(f"\n{len(fns)} tests passés.")
