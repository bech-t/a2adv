# --- Projet Apple II (cc65) ---------------------------------------------
# Cible : apple2 (CPU 6502, max compatibilite). Le 80 colonnes est fourni par
# un pilote texte MAISON (src/scr.c) via la memoire auxiliaire, sans la conio
# 80 col de cc65 (qui, elle, exigerait apple2enh/65C02).
TARGET   := apple2

# Nom du programme final (dans build/)
PROGRAM  := a2adv

# Outils cc65
CL65     := cl65

# AppleCommander (jar) + disquette ProDOS modele + loader, fournis dans le repo.
AC_JAR    ?= ac/ac.jar
PRODOS_TPL ?= prodos/prodos.dsk        # disquette ProDOS bootable (modele)
LOADER    ?= prodos/loader.system      # loader : charge <PROG> (nom sans .SYSTEM)
# Adresse de chargement. $4000 : 22 Ko dispo jusqu'a __HIMEM__ ($9600).
# Strategie graphismes (M4) : images en page HIRES 1 ($2000-$3FFF) uniquement
# (pas de page-flipping pour un livre-jeu) -> le code a $4000 ne la recouvre pas.
# ($6000 ne laisse que 13,5 Ko : trop juste pour le programme.)
LOADADDR  ?= 0x4000

# Dossiers
SRCDIR   := src
BUILDDIR := build

# --- Aventure a embarquer ----------------------------------------------
# Chaque aventure vit dans son propre dossier (source .adv + ses images) :
#   adventures/<ADV>/<ADV>.adv
#   adventures/<ADV>/img/*.HGR
# Sa compilation (STORYnn.DAT + ASSETS.IDX) et sa disquette vont dans
#   adventures/<ADV>/build/
# Choix de l'aventure :  make dsk ADV=chateau_hante   (defaut ci-dessous)
ADV      ?= chateau_hante
ADVDIR   := ../../adventures/$(ADV)
ADVSRC   := $(ADVDIR)/$(ADV).adv
ADVIMG   := $(ADVDIR)/img
# Images NOMMEES par leur id `@image` (ex: PENTHOUSE.HGR) : la seule source a
# maintenir a la main. Les fichiers numerotes IMGnn.HGR qu'attend le player
# sont GENERES a partir de la, jamais commits (cf. regle dsk plus bas) : plus
# jamais d'index a compter/nommer soi-meme, ni de decalage silencieux si un
# @image est insere plus tot dans le source.
ADVIMG_NAMED := $(ADVIMG)/named
DATADIR  := $(ADVDIR)/build
STORY    := STORY00.DAT
VOLNAME  := A2ADV

# Sources / objets
SOURCES  := $(wildcard $(SRCDIR)/*.c $(SRCDIR)/*.s)

# Options de compilation.
# Pile C reduite a 1 Ko (defaut 2 Ko) : pas de recursion profonde ici.
#
# -Cl : variables locales en STATIQUE plutot que sur la pile logicielle.
#   Mesure sur ce projet : 25016 -> 24238 octets, soit -778 (-3,1 %), et du code
#   plus rapide (pas de manipulation de pile a chaque acces a une locale).
#   /!\ INTERDIT DES QU'UNE FONCTION EST RECURSIVE OU REENTRANTE : ses locales
#   sont alors partagees entre les appels imbriques, et la corruption est
#   silencieuse. Aucune recursion dans les sources a ce jour ; a re-verifier
#   avant d'en introduire une (y compris indirecte, via un pointeur de fonction
#   comme scr_progress_cb).
#   Ne PAS ajouter -Oris : mesure a 26616 octets, soit +1600. Ni --codesize 40 :
#   pire encore, le binaire ne rentre plus sous __HIMEM__.
CFLAGS   := -t $(TARGET) -O -Cl
LDFLAGS  := -t $(TARGET) --start-addr $(LOADADDR) -Wl -D,__STACKSIZE__=0x0400

# --- Place code --------------------------------------------------------
# Le player ne tient plus sous le __HIMEM__ par defaut de la cible apple2 : on
# le releve a $B000 DANS TOUS LES CAS. C'etait auparavant conditionne a
# MOCKINGBOARD=1, si bien que MOCKINGBOARD=0 ne liait plus -- le binaire
# debordait de ~900 octets.
# A $B000 il reste 3 buffers ProDOS ($B000-$BF00), suffisant : STORY ouvert (1)
# + sauvegarde transitoire (1).
# Chaque Ko rendu au code retire un tampon ProDOS ($B000 -> 3, $B400 -> 2).
#
# Pourquoi les tampons sont AU-DESSUS de HIMEM, et pas dans le tas C : la carte
# du lieur place le tas entre la fin du BSS et le bas de la pile, ce qui ne
# faisait que ~109 octets dans la version d'origine -- moins qu'UN tampon de
# 1 Ko. Le jeu ouvrait pourtant ses fichiers. Les tampons occupent donc les
# pages libres jusqu'a la page globale ProDOS ($BF00), et relever HIMEM en
# retire bien un.
#
# DEUX suffisent, et c'est verifie et non suppose : tous les fopen du player
# ont ete recenses, le maximum simultane est de 2 --
#   STORY (story.c, ouvert en permanence) + image (scr_load_hgr) ;
#   STORY + SAVE0.DAT (cf. main.c, "ouvert en + de STORY") ;
#   source + destination pendant une copie en disque RAM, et la story.c ferme
#   STORY exprès avant d'appeler ram_ensure.
#
# On est donc AU RAS du besoin : toute fonction future qui ouvrirait un
# TROISIEME fichier en meme temps casserait ce reglage. Le symptome serait un
# fopen qui echoue, pas un plantage -- mais il faudrait alors repasser a $B000
# et rendre de la place ailleurs.
HIMEM    ?= 0xB400
LDFLAGS  += -Wl -D,__HIMEM__=$(HIMEM)

# --- Son Mockingboard --------------------------------------------------
# Active par defaut. Desactivation : make dsk MOCKINGBOARD=0 -- snd_mb.c n'est
# alors ni compile ni lie, et seul le haut-parleur reste.
#
# Le slot se choisit A LA MAIN dans le menu Options : il n'y a AUCUNE detection
# automatique, un balayage ecrirait dans sept cartes inconnues. Le slot designe
# est en revanche verifie avant usage (cf. mb_probe).
MOCKINGBOARD ?= 1
ifeq ($(MOCKINGBOARD),1)
  CFLAGS  += -DA2ADV_MOCKINGBOARD
else
  SOURCES := $(filter-out $(SRCDIR)/snd_mb.c,$(SOURCES))
endif

OBJECTS  := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,\
              $(patsubst $(SRCDIR)/%.s,$(BUILDDIR)/%.o,$(SOURCES)))

# Binaire Apple II generique (partage par toutes les aventures)
BIN      := $(BUILDDIR)/$(PROGRAM).bin
# Disquette : specifique a l'aventure -> dans son dossier build/
DSK      := $(DATADIR)/$(ADV).dsk

# --- Regles -------------------------------------------------------------
.PHONY: all clean dsk run hosttest data

all: $(BIN)

# Compile l'aventure selectionnee (ADV) -> adventures/<ADV>/build/
# Un seul appel a2c produit STORYnn.DAT, ASSETS.IDX et APP.LNG : cible groupee
# (&:, GNU Make >= 4.3) pour que make les considere tous produits ensemble.
# Sinon un APP.LNG manquant ne declencherait pas la recompilation tant que les
# STORYnn.DAT restent plus recents que le .adv.
# Les .lng sont en prerequis : traduire l'interface refait la disquette.
LNGSRC   := $(wildcard ../../lang/*.lng)
# Le COMPILATEUR est un prerequis, au meme titre que la source. Sans ca, changer
# une regle d'encodage dans a2c ne refait rien : les STORYnn.DAT restent plus
# recents que le .adv, make les croit a jour, et la disquette garde en silence
# le rendu de l'ancien compilateur. C'est arrive avec le passage de la casse.
A2CSRC   := $(wildcard ../../compiler/a2c/*.py)
# Options supplementaires pour a2c. Ex. pour une disquette Apple II / II+, qui
# n'ont aucun glyphe minuscule :  make dsk ADV=... A2CFLAGS=--majuscules
A2CFLAGS ?=
DATAOUT  := $(DATADIR)/$(STORY) $(DATADIR)/APP.LNG $(DATADIR)/ASSETS.IDX $(DATADIR)/IMAGES.MAP

data: $(DATAOUT)
$(DATAOUT) &: $(ADVSRC) $(LNGSRC) $(A2CSRC)
	( cd ../../compiler && python3 -m a2c ../adventures/$(ADV)/$(ADV).adv \
	    -o ../adventures/$(ADV)/build $(A2CFLAGS) )

# Edition de liens -> binaire Apple II
$(BIN): $(OBJECTS)
	$(CL65) $(LDFLAGS) -o $@ $(OBJECTS)

# Compilation C -> objet
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CL65) $(CFLAGS) --create-dep $(BUILDDIR)/$*.d -c -o $@ $<

# Assemblage .s -> objet
$(BUILDDIR)/%.o: $(SRCDIR)/%.s | $(BUILDDIR)
	$(CL65) -t $(TARGET) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Test hote : verifie le coeur du player (parsing + interpreteur) sur PC via gcc,
# en rejouant le vrai STORY.DAT. Ne teste pas l'affichage Apple II (ui.c).
HOSTBIN := $(BUILDDIR)/hostplay
DEMO_DATA := /tmp/a2adv_demo
hosttest:
	( cd ../../compiler && python3 -m a2c ../adventures/demo_simple/demo_simple.adv -o $(DEMO_DATA) )
	gcc -std=c99 -Wall -I$(SRCDIR) test/hostplay.c \
	    $(SRCDIR)/story.c $(SRCDIR)/state.c $(SRCDIR)/snd.c $(SRCDIR)/combat.c \
	    $(SRCDIR)/ramdisk.c \
	    -o $(HOSTBIN)
	@echo "== parcours victoire =="; $(HOSTBIN) $(DEMO_DATA)/STORY00.DAT 2 1 1 1 1 | tail -2
	@echo "== parcours defaite  =="; $(HOSTBIN) $(DEMO_DATA)/STORY00.DAT 1 1 1     | tail -2

# Fabrique une disquette ProDOS bootable a partir du modele fourni :
#   - loader          -> $(PROGRAM).SYSTEM (lance au boot ; charge $(PROGRAM))
#   - binaire cc65    -> $(PROGRAM)        (type BIN, adresse LOADADDR)
#   - STORY.DAT / ASSETS.IDX (donnees du compilateur)
# AppleCommander -as : depose le binaire cc65 en lisant son exehdr (adresse de
# chargement extraite automatiquement, exehdr strippe). Le loader saute a cette
# adresse -> l'octet 0 pose est bien le code. Idiomatique pour cc65.
dsk: $(DSK)
# Les images sont des PREREQUIS, pas seulement des fichiers copies par la
# recette : sans ca, remplacer un .HGR ne declenche aucun rebuild et la
# disquette garde l'ancienne image sans rien signaler. Les fichiers NOMMES
# (img/named/) en font partie : c'est desormais la vraie source, les IMGnn.HGR
# de $(ADVIMG) n'etant qu'une regeneration.
ADVIMGS  := $(wildcard $(ADVIMG)/*.HGR) $(wildcard $(ADVIMG_NAMED)/*.HGR)

$(DSK): $(BIN) $(DATAOUT) $(ADVIMGS) $(LOADER) $(PRODOS_TPL) | $(BUILDDIR)
	mkdir -p $(DATADIR)
	@# IMGnn.HGR : toujours REGENERES depuis les fichiers NOMMES (img/named/),
	@# jamais a la main. IMAGES.MAP (produit par a2c) donne l'index reel de
	@# chaque id `@image` ; le nom nomme ne bouge pas si l'ordre des @image
	@# change dans le source, contrairement a l'ancien index fige a la main.
	@while read -r idx name; do \
	  up=$$(printf '%s' "$$name" | tr '[:lower:]' '[:upper:]'); \
	  src=$(ADVIMG_NAMED)/$$up.HGR; \
	  if [ ! -e "$$src" ]; then \
	    echo "ERREUR: @image $$name attend $$src (introuvable)"; \
	    exit 1; \
	  fi; \
	  cp "$$src" $(ADVIMG)/IMG$$idx.HGR; \
	done < $(DATADIR)/IMAGES.MAP
	@# Budget disque, verifie AVANT AppleCommander : sans ca, un depassement se
	@# traduit par "Unable to locate a free block in the Volume Bitmap!" en
	@# pleine ecriture (disquette a moitie remplie, message illisible). On
	@# mesure la place libre du MODELE (vierge) plutot qu'un total fige : ca
	@# suit automatiquement un futur modele 800 Ko sans rien changer ici.
	@# L'estimation arrondit chaque fichier au bloc de 512 o superieur, et
	@# ajoute un bloc d'index pour tout fichier > 512 o (fichier "Sapling" en
	@# ProDOS) : legerement pessimiste, jamais l'inverse.
	@budget=$$(java -jar $(AC_JAR) -i $(PRODOS_TPL) | awk -F': ' '/Free Space \(bytes\)/{print $$2}'); \
	need=0; \
	for f in $(BIN) $(LOADER) $(DATADIR)/STORY*.DAT $(DATADIR)/ASSETS.IDX $(DATADIR)/APP.LNG $(ADVIMG)/*.HGR; do \
	  [ -e "$$f" ] || continue; \
	  sz=$$(stat -c%s "$$f"); \
	  blocks=$$(( (sz + 511) / 512 )); \
	  [ "$$sz" -gt 512 ] && blocks=$$((blocks + 1)); \
	  need=$$((need + blocks * 512)); \
	done; \
	if [ "$$need" -gt "$$budget" ]; then \
	  echo "ERREUR: cette disquette necessiterait ~$$((need/1024)) Ko pour" \
	       "$$((budget/1024)) Ko disponibles sur $(notdir $(PRODOS_TPL))" \
	       "($$(( (need-budget+1023)/1024 )) Ko de trop)."; \
	  echo "  -> retirez des images/du texte, ou passez a un support plus" \
	       "grand (voir spec, disque dur / sous-dossier ProDOS)."; \
	  exit 1; \
	fi
	cp $(PRODOS_TPL) $(DSK)
	java -jar $(AC_JAR) -n  $(DSK) $(VOLNAME)
	java -jar $(AC_JAR) -as $(DSK) $(PROGRAM)             < $(BIN)
	java -jar $(AC_JAR) -p  $(DSK) $(PROGRAM).SYSTEM sys  < $(LOADER)
	@for s in $(DATADIR)/STORY*.DAT; do \
	  [ -e "$$s" ] || continue; \
	  n=$$(basename $$s); \
	  echo "  + $$n"; \
	  java -jar $(AC_JAR) -p $(DSK) $$n bin 0 < $$s; \
	done
	java -jar $(AC_JAR) -p  $(DSK) ASSETS.IDX bin 0       < $(DATADIR)/ASSETS.IDX
	@# APP.LNG : socle de chaines d'interface. Sans lui le player est muet ;
	@# c'est une garantie de fabrication, pas un cas d'execution (spec §6.1).
	@[ -e $(DATADIR)/APP.LNG ] || { echo "ERREUR: $(DATADIR)/APP.LNG absent"; exit 1; }
	java -jar $(AC_JAR) -p  $(DSK) APP.LNG bin 0          < $(DATADIR)/APP.LNG
	@for h in $(ADVIMG)/*.HGR; do \
	  [ -e "$$h" ] || continue; \
	  n=$$(basename $$h); \
	  echo "  + $$n"; \
	  java -jar $(AC_JAR) -p $(DSK) $$n bin 0x2000 < $$h; \
	done
	@echo "Disquette prete : $(DSK)  (volume /$(VOLNAME)/)"
	java -jar $(AC_JAR) -l $(DSK)

run: $(DSK)
	@echo "Bootez $(DSK) dans un emulateur Apple II (ProDOS)."

clean:
	rm -rf $(BUILDDIR)/*

# Dependances auto-generees
-include $(wildcard $(BUILDDIR)/*.d)
