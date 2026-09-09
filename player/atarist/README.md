# Player Atari ST (portage — en cours)

Voir `spec-atarist.md` à la racine du dépôt (document local, non versionné)
pour le détail du portage : ce qui est déjà réutilisable côté `player/apple2/`,
ce qui change (graphismes, son, disque), et les inconnues restantes.

## Installer la chaîne de build

Compilateur croisé `m68k-atari-mint-gcc` (+ `libcmini`), via la PPA de
Vincent Rivière (Ubuntu/WSL) :

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
