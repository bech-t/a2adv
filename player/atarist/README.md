# Player Atari ST (portage — en cours)

Voir `spec-atarist.md` à la racine du dépôt (document local, non versionné)
pour le détail du portage : ce qui est déjà réutilisable côté `player/apple2/`,
ce qui change (graphismes, son, disque), et les inconnues restantes.

## Installer la chaîne de build

Compilateur croisé `m68k-atari-mint-gcc` (+ `mintlib`), via la PPA de
Vincent Rivière (Ubuntu/WSL) — `libcmini` n'est PAS dans cette PPA,
contrairement à ce qu'une première version de ce fichier disait :

```
sudo add-apt-repository ppa:vriviere/ppa
sudo apt update
sudo apt install cross-mint-essential
```

Émulateur de test (Hatari — peut monter un dossier hôte comme lecteur ST,
pratique pour itérer sans regénérer une image disque à chaque build) :

```
sudo apt install hatari
```

Vérifier l'installation :

```
m68k-atari-mint-gcc --version
hatari --version
```

## État actuel (2026-09-09)

Le cœur portable + la couche UI compilent et se lient sans avertissement
(`make all`), et une aventure texte (`combat_demo`) **charge et se joue**
sous Hatari+EmuTOS (menu, choix, accents à l'écran). `fseek()` de `mintlib`
est cassé sous l'émulation GEMDOS-HDD de Hatari (confirmé, cf.
`spec-atarist.md` §9 et `smoketest/fseek_test.c`) ; contourné dans la copie
ST de `story.c` en chargeant chaque `STORYnn.DAT` entièrement en RAM
(64 Ko max, garanti par le compilateur) plutôt qu'en cherchant dedans.

Prochain jalon : le mode graphique ST (`scr_gfx_*`/`scr_load_hgr` restent
des bouchons), puis le son YM2149 — cf. `spec-atarist.md` §4/§5/§11.
