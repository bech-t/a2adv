# Player web (Angular) : le catalogue d'aventures et le player, servis comme
# un site statique. Contrairement aux players natifs (cf. ../apple2/Makefile,
# ../atarist/Makefile, ../dos/Makefile), pas de compilation croisee ni de
# disquette : les aventures listees dans adventures.txt sont exportees par
# python3 -m a2c.site (cf. package.json:sync) avant chaque `dev` et `build`.
# Pillow est requis pour les images (pip install pillow).
#
# Prerequis (une fois) : npm install
#
#   make run                      serveur de dev, toutes les aventures de adventures.txt
#   make run ADV="combat_demo"    seulement celles-ci (plus rapide)
#   make site                     site de production dans dist/webng/browser
#   make site BASE_HREF=/a2adv-site/    site servi dans un sous-dossier
#   make test
#   CHROME=/chemin/vers/chrome make e2e    scenario hors ligne dans un vrai navigateur (apres make site)

ADV ?=
BASE_HREF ?= /

.PHONY: run site test e2e clean
run:
	ADVENTURES="$(ADV)" npm run dev

site:
	ADVENTURES="$(ADV)" npm run build -- --base-href=$(BASE_HREF)
	@echo "Site pret dans dist/webng/browser (BASE_HREF=$(BASE_HREF))"

test:
	npm test

# Scenario hors ligne (Service Worker, telechargement, sauvegardes) dans
# Chromium ; necessite un site construit par `make site` (BASE_HREF=/).
e2e:
	node tools/e2e-offline.mjs

# public/adventures et public/catalog.json sont regeneres par npm run sync,
# jamais commites (cf. .gitignore) -- sans risque a supprimer.
clean:
	rm -rf dist .angular public/adventures public/catalog.json
