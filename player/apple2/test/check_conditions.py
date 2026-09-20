#!/usr/bin/env python3
"""Verifie la sortie de hostplay sur test/conditions.adv : ce que le vrai
state_eval_cond (OU de ET, not, else) rend visible ou non.

Etat de la section b : x vrai, y faux, objet i absent, N = 5 ; section d :
idem avec l'objet i."""
import re
import sys

out = open(sys.argv[1], encoding="utf-8").read()
sections = re.split(r"== section (\d+) ==", out)
body = {int(sections[k]): sections[k + 1] for k in range(1, len(sections), 2)}


def lines(n):
    return [l.strip() for l in body[n].splitlines()
            if l.strip() and not l.startswith("    [") and not l.strip().startswith("->")]


b = lines(1)
d = lines(3)
want_b = ["B : sinon, et pas y.", "D : pas (x et y).", "E : N < 6.",
          "1) 1 : (x et non i) ou y"]
want_d = ["A : (x ou y) et i.", "1) 2 : else"]
assert b == want_b, f"section b : {b}"
assert d[:2] == want_d, f"section d : {d}"
print("conditions: OK")
