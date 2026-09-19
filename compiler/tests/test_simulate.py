"""Tests du simulateur de parties (a2c.simulate) sur de petites aventures."""

from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from a2c.parser import parse                        # noqa: E402
from a2c.simulate import (                          # noqa: E402
    DEAD_END, END_LOSE, END_WIN, LOOP, Simulator, make_picker,
)
from a2c.symbols import resolve                     # noqa: E402

ADVENTURES_DIR = Path(__file__).resolve().parents[2] / "adventures"


def _sim(source: str, p_correct: float = 0.5) -> Simulator:
    story = parse(source)
    resolve(story)
    return Simulator(story, p_correct)


def _play(sim: Simulator, seed: int = 1, strategy: str = "random"):
    return sim.play(make_picker(strategy, []), random.Random(seed))


def test_impasse_when_every_choice_is_conditional_and_false():
    sim = _sim("""
@title T
@start a
@flag f
:: a
* {flag f} [aller] -> b
:: b
""")
    outcome, section, _, _ = _play(sim)
    assert outcome == DEAD_END
    assert section == "a"


def test_loop_is_detected():
    sim = _sim("""
@title T
@start a
:: a
* [encore] -> a
""")
    outcome, _, _, _ = sim.play(make_picker("random", []), random.Random(1),
                                max_steps=50)
    assert outcome == LOOP


def test_endings_are_reported():
    sim = _sim("""
@title T
@start a
:: a
* [gagner] -> w
* [perdre] -> l
:: w
@ending win
:: l
@ending lose
""")
    outcomes = {_play(sim, seed)[0] for seed in range(30)}
    assert outcomes == {END_WIN, END_LOSE}


def test_stat_effects_are_clamped_and_conditions_follow_them():
    sim = _sim("""
@title T
@start a
@stat X 0 0 3
:: a
* [monter] -> b
  ~ add X 10
:: b
* {stat X == 3} [plafonné] -> w
:: w
@ending win
""")
    outcome, _, state, _ = _play(sim)
    assert outcome == END_WIN
    assert state.value["X"] == 3


def test_local_flags_reset_at_chapter_change():
    sim = _sim("""
@title T
@start a
@flag l local
:: a
~ set l
* [suite] -> b
@chapter "Deux"
:: b
* {flag l} [ne doit pas apparaître] -> w
:: w
@ending win
""")
    outcome, section, _, _ = _play(sim)
    assert outcome == DEAD_END
    assert section == "b"


def test_same_seed_same_games():
    sim = _sim((ADVENTURES_DIR / "chateau_hante" / "chateau_hante.adv")
               .read_text(encoding="utf-8"))
    runs = [[_play(sim, seed, "explorer")[0] for seed in range(20)]
            for _ in range(2)]
    assert runs[0] == runs[1]


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    for fn in fns:
        fn()
        print(f"ok  {fn.__name__}")
    print(f"\n{len(fns)} tests passés.")
