# Test de fumee du toolchain ST -- pas le vrai player, juste de quoi
# verifier que m68k-atari-mint-gcc + Hatari + EmuTOS fonctionnent ensemble
# avant d'ecrire scr.c/snd.c pour de vrai (cf. spec-atarist.md §10).

CC := m68k-atari-mint-gcc
CFLAGS := -mshort -O2

AUTO := AUTO
PRG  := $(AUTO)/HELLO.PRG

# etos512fr.img (EmuTOS francais) n'est pas versionne : a recuperer sur
# https://emutos.sourceforge.io/ (GPL) et a placer a cote de ce Makefile.
TOS := etos512fr.img

.PHONY: all run clean
all: $(PRG)

$(PRG): hello.c | $(AUTO)
	$(CC) $(CFLAGS) -o $@ $<

$(AUTO):
	mkdir -p $(AUTO)

# Boote sous Hatari : le dossier courant devient le lecteur C:, EmuTOS lance
# tout seul ce qu'il trouve dans AUTO/ au demarrage (pas besoin de --auto).
run: $(PRG)
	hatari --tos $(TOS) -d "$$(pwd)"

clean:
	rm -rf $(AUTO)
