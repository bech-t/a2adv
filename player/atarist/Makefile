# Player Atari ST. Texte + images (scr_gfx_*/scr_load_hgr, cf.
# player/atarist/src/scr.c) + son YM2149 (snd.c) -- ce dernier pas encore
# verifie sur emulateur/materiel.
#
# make dsk ADV=combat_demo : compile le player + copie STORY00.DAT/APP.LNG
# depuis adventures/<ADV>/build/ (deja produits par le Makefile Apple II --
# le compilateur a2c produit un format binaire neutre vis-a-vis de la
# plateforme cible) dans build/AUTO/, prete pour `make run`.
#
# Images ST-specifiques (format different de l'Apple II, cf. img2st.py) :
# adventures/<ADV>/img/atarist/*.HGR, si le dossier existe -- a cote de
# adventures/<ADV>/img/ (Apple II), pas dedans, pour ne rien lui melanger.

CC := m68k-atari-mint-gcc
CFLAGS := -mshort -O2 -Wall

ADV      ?= combat_demo
ADVDIR   := ../../adventures/$(ADV)/build
IMGDIR   := ../../adventures/$(ADV)/img/atarist
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
# (le compilateur a2c ne connait pas la plateforme cible). A la racine du
# lecteur C: (pas dans AUTO/) : le programme auto-lance depuis
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
	@# Splash du player, pas de l'aventure : contrairement a l'Apple II (ou
	@# BOOT00.HGR est toujours fourni par l'aventure, cf. adventures/*/img/),
	@# ce BOOT00.HGR-la vit a la racine du player ST et s'affiche avant TOUTE
	@# aventure qui n'en fournit pas le sien -- easter egg assume, pas un
	@# reste de test.
	@test -f BOOT00.HGR && cp BOOT00.HGR $(BUILDDIR)/ || true
	@test -d $(IMGDIR) && cp $(IMGDIR)/*.HGR $(BUILDDIR)/ || true

# HFLAGS : options Hatari, par defaut un moniteur monochrome (--monitor mono
# force la haute resolution, Getrez()=2 -- `--tos-res med` seul (RGB, moyenne
# resolution) ne suffit pas avec cette version de Hatari, verifie le
# 2026-09-09 ; a reessayer si Hatari est mis a jour). Sur mono, jamais
# d'image (impossible materiellement), cf. scr.c. Surchargeable : `make run
# HFLAGS=` pour un moniteur couleur (texte 80 col, images en 40 col/16
# couleurs des qu'une scene en affiche une, cf. scr_load_hgr).
# HFLAGS ?= --monitor mono

HFLAGS=

run: dsk
	cd $(BUILDDIR) && hatari --tos ../$(TOS) -d "$$(pwd)" $(HFLAGS)

clean:
	rm -rf $(BUILDDIR)
