# Player PC DOS (FreeDOS-compatible), graphique VGA mode 13h (320x200/256c)
# -- cf. src/scr.c, EGA planaire pas encore ecrit (cf. README.md). Son laisse
# de cote pour l'instant (cf. src/snd.c) : une prochaine passe visera la
# Sound Blaster.
#
# Toolchain : Open Watcom v2 (compilation croisee Linux -> DOS 16 bits reel,
# modele COMPACT -- cf. src/diskio.c pour pourquoi). Pas de paquet apt ; a
# installer une fois (snapshot CI officiel, binaires Linux "binl/") :
#   mkdir -p ~/opt && cd ~/opt
#   curl -Lo ow-snapshot.tar.xz \
#     https://github.com/open-watcom/open-watcom-v2/releases/download/Last-CI-build/ow-snapshot.tar.xz
#   tar xJf ow-snapshot.tar.xz ./binl ./lib286 ./h
# Emplacement : ~/opt (binl/wcc, binl/wlink, h/, lib286/). Il n'est PAS dans
# le PATH : `which wcc` ne le trouve pas, c'est normal. Pour un autre dossier :
# make WATCOM=/autre/chemin.
# Emulateur de test : DOSBox -- sudo apt install dosbox.
#
# make dsk ADV=combat_demo : compile le player + les donnees de l'aventure
# dans build/, prete pour `make run`.
# make img ADV=combat_demo : idem, puis fabrique une vraie disquette
# bootable .img (FAT12 720 Ko, via mtools) dans adventures/<ADV>/build/.
#
# Images DOS-specifiques (format different d'Apple II/Atari ST, cf.
# img2dos/img2dos.py) : adventures/<ADV>/img/dos/*.PCX, si le dossier existe
# -- a cote de adventures/<ADV>/img/ (Apple II) et img/atarist/ (ST), pas
# dedans. Extension .PCX (ZSoft, format DOS standard 256 couleurs) et non
# .HGR/.PI1 -- cf. IMG_EXT dans src/platform.h, la seule reference a ne pas
# dedoubler.

# WATCOM doit etre EXPORTE (pas juste une variable make) : wlink en a besoin
# pour retrouver ses definitions systeme ("SYSTEM dos") et clibc.lib tout
# seul, sans -I explicite possible cote link (contrairement a wcc, cf.
# CPATHS plus bas).
export WATCOM ?= $(HOME)/opt
ifeq ($(wildcard $(WATCOM)/binl/wcc),)
$(error Open Watcom introuvable dans $(WATCOM) : installer le snapshot dans ~/opt (cf. en-tete) ou passer WATCOM=/chemin)
endif
CC       := $(WATCOM)/binl/wcc
LD       := $(WATCOM)/binl/wlink
INCLUDE  := $(WATCOM)/h
CFLAGS   := -0 -mc -zq -w4

ADV      ?= combat_demo
ADVBUILD := ../../adventures/$(ADV)/build
IMGDIR   := ../../adventures/$(ADV)/img/dos
BUILDDIR := build
COMMONDIR:= ../common
EXE      := $(BUILDDIR)/A2ADV.EXE

# Le gros du moteur vit dans $(COMMONDIR), partage avec les autres players
# (cf. ../apple2/Makefile, ../atarist/Makefile) ; ne reste ici que ce qui est
# reellement specifique DOS (scr.c, snd.c, diskio.c/assetcache.c/sysinfo.c --
# backends de player/common/include/).
CPATHS   := -I=src -I=$(COMMONDIR)/include -I=$(INCLUDE)
SRC      := $(wildcard src/*.c) $(wildcard $(COMMONDIR)/src/*.c)
OBJ      := $(patsubst src/%.c,$(BUILDDIR)/%.o,\
              $(patsubst $(COMMONDIR)/src/%.c,$(BUILDDIR)/%.o,$(SRC)))

.PHONY: all dsk img run dist hosttest clean
all: $(EXE)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(CPATHS) -fo=$@ $<

$(BUILDDIR)/%.o: $(COMMONDIR)/src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(CPATHS) -fo=$@ $<

# @wlink.lnk (charge explicitement, cf. sa presence dans binl/) : definit les
# "SYSTEM" connus de wlink (dont "dos"), PAS charge automatiquement quand
# wlink est invoque par un chemin absolu (constate en pratique) -- seul un
# invocation par simple nom trouve via PATH semble le faire toute seule.
$(EXE): $(OBJ)
	$(LD) @$(WATCOM)/binl/wlink.lnk NAME $@ SYSTEM dos FILE { $(OBJ) }

$(BUILDDIR):
	mkdir -p $@

# STORY00.DAT/APP.LNG : compiles depuis le .adv par le Makefile racine (cf.
# ../../Makefile, cible `compile`) -- neutre vis-a-vis de la plateforme.
# Nettoie les STORY*.DAT AVANT de copier, meme raison qu'Atari ST (cf.
# ../atarist/Makefile) : eviter de melanger deux aventures dans build/.
dsk: $(EXE)
	$(MAKE) -C ../.. compile ADV=$(ADV)
	rm -f $(BUILDDIR)/STORY*.DAT
	cp $(ADVBUILD)/STORY*.DAT $(ADVBUILD)/APP.LNG $(BUILDDIR)/
	@test -f BOOT00.PCX && cp BOOT00.PCX $(BUILDDIR)/ || true
	@test -d $(IMGDIR) && cp $(IMGDIR)/*.PCX $(BUILDDIR)/ || true

run: dsk
	cd $(BUILDDIR) && dosbox -c "mount c ." -c "c:" -c "A2ADV.EXE" -c "exit"

# --- Image disquette .img (mtools) ---------------------------------------
# Repart du secteur de boot FreeDOS officiel (cf. freedos/README.md) a
# chaque fois plutot que de modifier une image existante : mformat -B copie
# le code de demarrage MAIS reecrit le BPB (geometrie), donc toujours
# repartir d'un volume FRAIS -- jamais de KERNEL.SYS/COMMAND.COM deplaces
# entre deux versions du contenu. Necessite mtools (mformat/mcopy), aucun
# montage ni droit root : `sudo apt install mtools`.
IMG_FILE := $(ADVBUILD)/$(ADV).img
FREEDOS  := freedos
BOOTFILES:= boot

img: $(IMG_FILE)

$(IMG_FILE): dsk
	rm -f $@
	mformat -C -B $(FREEDOS)/BOOTSECT.BIN -f 720 -i $@ ::
	mcopy -i $@ $(FREEDOS)/KERNEL.SYS $(FREEDOS)/COMMAND.COM ::
	mcopy -i $@ $(BOOTFILES)/CONFIG.SYS $(BOOTFILES)/AUTOEXEC.BAT ::
	mcopy -i $@ $(EXE) ::
	mcopy -i $@ $(BUILDDIR)/STORY*.DAT $(BUILDDIR)/APP.LNG ::
	@test -f $(BUILDDIR)/BOOT00.PCX && mcopy -i $@ $(BUILDDIR)/BOOT00.PCX :: || true
	@for f in $(BUILDDIR)/*.PCX; do \
	  [ -e "$$f" ] && [ "$$(basename $$f)" != "BOOT00.PCX" ] && mcopy -i $@ "$$f" :: || true; \
	done
	@echo "Image disquette prete : $@"

# Images .img pretes a l'emploi pour TOUTES les aventures detectees (meme
# convention que `dist` Apple II/Atari ST, cf. ../apple2/Makefile).
DISTDIR := ../../build
dist:
	@mkdir -p $(DISTDIR)
	@failed=""; \
	for a in $$($(MAKE) -s -C ../.. print-adventures); do \
	  if $(MAKE) img ADV=$$a; then \
	    cp ../../adventures/$$a/build/$$a.img $(DISTDIR)/$$a.img; \
	  else \
	    echo "ATTENTION: $$a ignoree (echec de construction)"; \
	    failed="$$failed $$a"; \
	  fi; \
	done; \
	[ -z "$$failed" ] || echo "Aventures ignorees :$$failed"

# Rejoue le coeur du moteur (story/state/combat) avec gcc HOTE, sans DOS ni
# Watcom ni emulateur -- valide tout common/ d'un coup (cf. ../apple2/
# Makefile:hosttest). Les backends DOS diskio.c/assetcache.c/snd.c sont du C
# portable (pas d'en-tete DOS) : reutilises tels quels. scr.c/sysinfo.c, eux,
# dependent de <i86.h>/<dos.h> -- exclus, remplaces par le stub hostplay.c
# deja ecrit pour Apple II (meme contrat scr.h minimal).
DEMO_DATA := $(BUILDDIR)/hosttest-data
HOSTBIN   := $(BUILDDIR)/hosttest
APPLE2DIR := ../apple2
hosttest:
	mkdir -p $(DEMO_DATA)
	( cd ../../compiler && python3 -m a2c ../adventures/demo_simple/demo_simple.adv -o ../player/dos/$(DEMO_DATA) )
	gcc -std=c99 -Wall -Isrc -I$(COMMONDIR)/include -I$(APPLE2DIR)/src \
	    $(APPLE2DIR)/test/hostplay.c \
	    $(COMMONDIR)/src/story.c $(COMMONDIR)/src/state.c $(COMMONDIR)/src/combat.c \
	    src/snd.c src/diskio.c src/assetcache.c \
	    -o $(HOSTBIN)
	@echo "== parcours victoire =="; $(HOSTBIN) $(DEMO_DATA)/STORY00.DAT 2 1 1 1 1 | tail -2
	@echo "== parcours defaite  =="; $(HOSTBIN) $(DEMO_DATA)/STORY00.DAT 1 1 1     | tail -2

clean:
	rm -rf $(BUILDDIR)
