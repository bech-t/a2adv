# Player Atari ST -- slice "texte seul" (cf. spec-atarist.md §10). Pas encore
# de mode graphique ni de son reel (scr_gfx_*/scr_load_hgr sont des bouchons,
# snd.c ne fait rien hors __CC65__, cf. player/atarist/src/scr.c/snd.c).
#
# make dsk ADV=combat_demo : compile le player + copie STORY00.DAT/APP.LNG
# depuis adventures/<ADV>/build/ (deja produits par le Makefile Apple II --
# le format est partage, cf. spec.md §9ter) dans build/AUTO/, prete pour
# `make run`.

CC := m68k-atari-mint-gcc
CFLAGS := -mshort -O2 -Wall

ADV      ?= combat_demo
ADVDIR   := ../../adventures/$(ADV)/build
BUILDDIR := build
AUTO     := $(BUILDDIR)/AUTO
PRG      := $(AUTO)/A2ADV.PRG

TOS := etos512fr.img

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=$(BUILDDIR)/%.o)

.PHONY: all dsk run clean
all: $(PRG)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(PRG): $(OBJ) | $(AUTO)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

$(BUILDDIR) $(AUTO):
	mkdir -p $@

# STORY00.DAT/APP.LNG : produits par `make dsk ADV=...` cote player/apple2
# (le compilateur a2c ne connait pas la plateforme cible, cf. spec.md §9ter).
# A la racine du lecteur C: (pas dans AUTO/) : le programme auto-lance depuis
# AUTO/ mais son repertoire courant GEMDOS reste la racine du volume.
#
# TODO (note du 2026-09-09, a reflechir plus tard, pas maintenant) : appeler
# `make -C ../apple2` d'ici est un raccourci qui couple ce Makefile au dossier
# Apple II, alors que compiler un .adv en STORY.DAT est neutre vis-a-vis de la
# plateforme. A separer proprement un jour entre make de player (ce fichier),
# make de disque/image, et make d'aventure (.adv -> STORY.DAT, qui ne devrait
# dependre d'aucun player).
# Nettoie les STORY*.DAT AVANT de copier : sans ca, enchainer `make dsk
# ADV=x` puis `make run` tout seul (ADV retombe au defaut combat_demo) melange
# les STORYnn.DAT de deux aventures differentes dans build/ -- constate en
# pratique le 2026-09-09.
dsk: $(PRG)
	@test -f $(ADVDIR)/STORY00.DAT || { echo "ERREUR: $(ADVDIR)/STORY00.DAT absent -- lancer d'abord: make -C ../apple2 dsk ADV=$(ADV)"; exit 1; }
	rm -f $(BUILDDIR)/STORY*.DAT
	cp $(ADVDIR)/STORY*.DAT $(ADVDIR)/APP.LNG $(BUILDDIR)/

# HFLAGS : options Hatari, par defaut le 80 colonnes (--monitor mono force la
# haute resolution monochrome, Getrez()=2 -- `--tos-res med` seul (RGB,
# moyenne resolution, 80 col aussi) ne suffit pas avec cette version de
# Hatari, verifie le 2026-09-09 ; a reessayer si Hatari est mis a jour).
# Surchargeable : `make run HFLAGS=` pour le 40 col par defaut (basse res).
HFLAGS ?= --monitor mono
run: dsk
	cd $(BUILDDIR) && hatari --tos ../$(TOS) -d "$$(pwd)" $(HFLAGS)

clean:
	rm -rf $(BUILDDIR)
