# FreeDOS -- fichiers de boot minimaux

`KERNEL.SYS`, `COMMAND.COM` et `BOOTSECT.BIN` (secteur de boot FAT12, extrait
avec `dd`) proviennent tels quels de la disquette d'installation officielle
**FreeDOS 1.3 "Floppy edition"** (image 720 Ko), disponible sur
https://www.freedos.org/download/ -- FreeDOS est un DOS libre (GNU GPL),
concu pour etre redistribue tel quel.

Le Makefile (`../Makefile`, cible `dsk`) part de ce secteur de boot (via
`mformat -B`) pour fabriquer une disquette FAT12 fraiche, y copie ces deux
fichiers puis nos propres `CONFIG.SYS`/`AUTOEXEC.BAT` (cf. `../boot/`) et le
joueur/les donnees de l'aventure -- aucun des fichiers d'installateur FreeDOS
(menu de langue, FDISK, FORMAT...) n'est conserve : seul le minimum pour
booter et lancer `A2ADV.EXE` directement.
