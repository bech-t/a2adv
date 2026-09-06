# zx02 — compresseur d'images (outil de build)

Sources vendues telles quelles depuis [dmsc/zx02](https://github.com/dmsc/zx02)
(licence MIT, `LICENSE`), une variation 6502 du format ZX0 d'Einar Saukas
(licence BSD-3-Clause, `LICENSE.zx0`). Compile en un outil hôte (`zx02`) par
`make dsk` — jamais embarqué dans le player.

Le **décodeur** (le seul morceau qui tourne sur l'Apple II) n'est pas ici :
c'est `player/apple2/src/zx02.s`, un port de `6502/zx02-optim.asm` du même
projet — voir l'en-tête de ce fichier pour le détail de l'adaptation et sa
vérification.
