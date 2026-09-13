# Player Atari ST. Texte + images (scr_gfx_*/scr_load_hgr, cf.
# player/atarist/src/scr.c) + son YM2149 (snd.c) -- ce dernier pas encore
# verifie sur emulateur/materiel.
#
# make dsk ADV=combat_demo : compile le player + les donnees de l'aventure
# (delegue au Makefile racine, neutre vis-a-vis de la plateforme -- cf.
# ../../Makefile) dans build/AUTO/, prete pour `make run`.
# make st ADV=combat_demo : idem, puis fabrique une vraie image disquette
# .ST (FAT12, via mtools) dans adventures/<ADV>/build/.
#
# Images ST-specifiques (format different de l'Apple II, cf. img2st.py) :
# adventures/<ADV>/img/atarist/*.PI1, si le dossier existe -- a cote de
# adventures/<ADV>/img/ (Apple II), pas dedans, pour ne rien lui melanger.
# Extension .PI1 (Degas reel, cf. img2st.py) et non .HGR (Apple II) : cf.
# IMG_EXT dans src/platform.h, la seule reference a ne pas dedoubler.

CC := m68k-atari-mint-gcc
CFLAGS := -mshort -O2 -Wall

ADV      ?= combat_demo
ADVBUILD := ../../adventures/$(ADV)/build
IMGDIR   := ../../adventures/$(ADV)/img/atarist
BUILDDIR := build
AUTO     := $(BUILDDIR)/AUTO
PRG      := $(AUTO)/A2ADV.PRG
COMMONDIR:= ../common

TOS := etos512fr.img

# Le gros du moteur (parsing, streaming, UI, menu...) vit dans $(COMMONDIR),
# partage avec le player Apple II (cf. ../apple2/Makefile) ; ne reste ici que
# ce qui est reellement specifique Atari ST (scr.c, snd.c, diskio.c/
# assetcache.c -- backends de player/common/include/).
CFLAGS += -I src -I $(COMMONDIR)/include
SRC := $(wildcard src/*.c) $(wildcard $(COMMONDIR)/src/*.c)
OBJ := $(patsubst src/%.c,$(BUILDDIR)/%.o,\
         $(patsubst $(COMMONDIR)/src/%.c,$(BUILDDIR)/%.o,$(SRC)))

.PHONY: all dsk st dist run clean
all: $(PRG)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/%.o: $(COMMONDIR)/src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(PRG): $(OBJ) | $(AUTO)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

$(BUILDDIR) $(AUTO):
	mkdir -p $@

# STORY00.DAT/APP.LNG : compiles depuis le .adv par le Makefile racine (le
# compilateur a2c ne connait pas la plateforme cible -- cf. ../../Makefile,
# cible `compile`). A la racine du lecteur C: (pas dans AUTO/) : le programme
# auto-lance depuis AUTO/ mais son repertoire courant GEMDOS reste la racine
# du volume.
#
# Nettoie les STORY*.DAT AVANT de copier : sans ca, enchainer `make dsk
# ADV=x` puis `make run` tout seul (ADV retombe au defaut combat_demo) melange
# les STORYnn.DAT de deux aventures differentes dans build/ -- constate en
# pratique le 2026-09-09.
dsk: $(PRG)
	$(MAKE) -C ../.. compile ADV=$(ADV)
	rm -f $(BUILDDIR)/STORY*.DAT
	cp $(ADVBUILD)/STORY*.DAT $(ADVBUILD)/APP.LNG $(BUILDDIR)/
	@# Splash du player, pas de l'aventure : contrairement a l'Apple II (ou
	@# BOOT00.HGR est toujours fourni par l'aventure, cf. adventures/*/img/),
	@# ce BOOT00.PI1-la vit a la racine du player ST et s'affiche avant TOUTE
	@# aventure qui n'en fournit pas le sien -- easter egg assume, pas un
	@# reste de test.
	@test -f BOOT00.PI1 && cp BOOT00.PI1 $(BUILDDIR)/ || true
	@test -d $(IMGDIR) && cp $(IMGDIR)/*.PI1 $(BUILDDIR)/ || true

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

# --- Image disquette .ST (mtools) ---------------------------------------
# `run` monte $(BUILDDIR) comme disque dur GEMDOS via Hatari (-d) : pratique
# pour iterer, mais ne produit aucun fichier a distribuer. `st` fabrique une
# VRAIE image disquette Atari ST -- format FAT12 720 Ko, la disquette DD
# standard, lisible par n'importe quel Atari ST/emulateur -- a partir du
# meme contenu que `dsk`. Necessite mtools (mformat/mcopy/mmd), aucun montage
# ni droit root : `sudo apt install mtools`.
ST_IMG := $(ADVBUILD)/$(ADV).st

st: $(ST_IMG)

$(ST_IMG): dsk
	rm -f $@
	mformat -C -f 720 -i $@ ::
	mmd -i $@ ::AUTO
	mcopy -i $@ $(AUTO)/A2ADV.PRG ::AUTO/
	mcopy -i $@ $(BUILDDIR)/STORY*.DAT $(BUILDDIR)/APP.LNG ::
	@test -f $(BUILDDIR)/BOOT00.PI1 && mcopy -i $@ $(BUILDDIR)/BOOT00.PI1 :: || true
	@for f in $(BUILDDIR)/*.PI1; do \
	  [ -e "$$f" ] && [ "$$(basename $$f)" != "BOOT00.PI1" ] && mcopy -i $@ "$$f" :: || true; \
	done
	@echo "Image disquette prete : $@"

# Images .ST pretes a l'emploi pour TOUTES les aventures detectees, dans
# build/ a la racine du depot (meme convention que le `dist` Apple II, cf.
# player/apple2/Makefile) -- liste fournie par le Makefile racine (cf.
# print-adventures) : pas de duplication. Une aventure qui echoue (ex.
# fixture de test incomplete, comme demo_simple) est signalee et IGNOREE
# plutot que d'interrompre les autres.
DISTDIR := ../../build
dist:
	@mkdir -p $(DISTDIR)
	@failed=""; \
	for a in $$($(MAKE) -s -C ../.. print-adventures); do \
	  if $(MAKE) st ADV=$$a; then \
	    cp ../../adventures/$$a/build/$$a.st $(DISTDIR)/$$a.st; \
	  else \
	    echo "ATTENTION: $$a ignoree (echec de construction)"; \
	    failed="$$failed $$a"; \
	  fi; \
	done; \
	[ -z "$$failed" ] || echo "Aventures ignorees :$$failed"

clean:
	rm -rf $(BUILDDIR)
