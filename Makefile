# Disquettes pretes a l'emploi dans build/, pour tester sans installer la
# chaine de build (cc65, AppleCommander, Java, Python...).
ADVENTURES := chateau_hante combat_demo orbe_de_sortis homme_costume_blanc

.PHONY: dist
dist:
	@mkdir -p build
	@for a in $(ADVENTURES); do \
	  $(MAKE) -C player/apple2 dsk ADV=$$a || exit 1; \
	  cp adventures/$$a/build/$$a.dsk build/$$a.dsk; \
	done
