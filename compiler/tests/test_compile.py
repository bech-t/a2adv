"""Tests du compilateur a2c sur examples/demo.adv (round-trip encode/decode)."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from a2c import model as M              # noqa: E402
from a2c.decode import decode           # noqa: E402
from a2c.encoder import VERSION, encode_story    # noqa: E402
from a2c.translit import transliterate           # noqa: E402
from a2c.parser import parse            # noqa: E402
from a2c.symbols import resolve         # noqa: E402

DEMO = Path(__file__).resolve().parents[2] / "adventures" / "demo_simple" / "demo_simple.adv"


def _compile(upper: bool = False):
    """Compile la demo. `upper` = l'option `--majuscules` du CLI."""
    story = parse(DEMO.read_text(encoding="utf-8"))
    resolve(story)
    return story, decode(encode_story(story, upper=upper)[0])   # STORY0.DAT


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


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    for fn in fns:
        fn()
        print(f"ok  {fn.__name__}")
    print(f"\n{len(fns)} tests passés.")


def test_majuscules_option():
    """`--majuscules` : rendu d'origine, tout en capitales ASCII."""
    story, d = _compile(upper=True)
    idx = {s.name: i for i, s in enumerate(story.sections)}
    assert d["item_labels"] == ["TORCHE"]
    _cond, _eff, _tgt, label = d["sections"][idx["buissons"]].choices[0]
    assert label == "RAMASSER LA TORCHE"


def test_translit_accents_toujours_retires():
    """Aucun glyphe accentué sur la machine cible : les deux modes sont ASCII."""
    src = "Où est l'Œuf, Éléphant ?"
    assert transliterate(src) == "Ou est l'Oeuf, Elephant ?"
    assert transliterate(src, upper=True) == "OU EST L'OEUF, ELEPHANT ?"
    for mode in (False, True):
        transliterate(src, mode).encode("ascii")   # ne doit jamais lever
