# Makefile racine : compile les aventures (.adv -> STORYnn.DAT/APP.LNG/
# ASSETS.IDX/IMAGES.MAP) avec le compilateur a2c. Etape neutre vis-a-vis de la
# plateforme cible -- ce Makefile ignore tout des players. La compilation du
# player et la fabrication des disquettes/images vivent dans
# player/apple2/Makefile et player/atarist/Makefile, qui appellent celui-ci
# (make -C .. compile ADV=...) pour obtenir leurs donnees.

# Aventures detectees automatiquement : tout dossier adventures/<nom>/ qui
# contient un <nom>.adv (ignore les fichiers isoles comme adventures/boot.hgr).
ADV_DIRS   := $(patsubst adventures/%/,%,$(wildcard adventures/*/))
ADVENTURES := $(foreach a,$(ADV_DIRS),$(if $(wildcard adventures/$a/$a.adv),$a))

ADV      ?= chateau_hante
ADVDIR   := adventures/$(ADV)
ADVSRC   := $(ADVDIR)/$(ADV).adv
DATADIR  := $(ADVDIR)/build

LNGSRC   := $(wildcard lang/*.lng)
A2CSRC   := $(wildcard compiler/a2c/*.py)
# Options supplementaires pour a2c (--max-file, --summary...).
A2CFLAGS ?=
DATAOUT  := $(DATADIR)/STORY00.DAT $(DATADIR)/APP.LNG $(DATADIR)/ASSETS.IDX $(DATADIR)/IMAGES.MAP

.PHONY: compile compile-all
compile: $(DATAOUT)

# Cible groupee (&:, GNU Make >= 4.3) : un seul appel a2c produit STORYnn.DAT,
# ASSETS.IDX, APP.LNG et IMAGES.MAP ensemble. Sinon un fichier manquant ne
# declenche pas de recompilation tant que les autres restent plus recents que
# le .adv. Le COMPILATEUR (A2CSRC) est un prerequis au meme titre que la
# source : sans ca, changer une regle d'encodage dans a2c ne refait rien, les
# STORYnn.DAT restant plus recents que le .adv (deja arrive avec le passage de
# la casse).
$(DATAOUT) &: $(ADVSRC) $(LNGSRC) $(A2CSRC)
	( cd compiler && python3 -m a2c ../$(ADVSRC) -o ../$(DATADIR) $(A2CFLAGS) )

# Compile toutes les aventures detectees -- utilise par les cibles `dist` des
# players (cf. player/apple2/Makefile, player/atarist/Makefile) pour ne pas
# reimplementer la detection ni l'appel a2c chacun de leur cote.
compile-all:
	@for a in $(ADVENTURES); do $(MAKE) compile ADV=$$a || exit 1; done

# Liste des aventures detectees, pour les cibles `dist` des players (evite de
# redupliquer la detection ADV_DIRS/ADVENTURES dans player/apple2/Makefile et
# player/atarist/Makefile).
.PHONY: print-adventures
print-adventures:
	@echo $(ADVENTURES)
