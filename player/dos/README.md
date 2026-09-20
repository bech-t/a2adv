# Player PC DOS (FreeDOS-compatible) -- portage (en cours)

Ce dossier reutilise le cœur C partage avec `player/apple2/` et
`player/atarist/` (`story.c`, `state.c`, `combat.c`, ..., et le format
binaire `STORY.DAT`/`APP.LNG`) sans rien y changer ; seuls l'affichage
(`scr.c`), le son et le disque sont specifiques a cette machine.

Cible : un PC 386/486 (ou compatible) reel-mode avec carte **VGA**
(mode 13h, 320x200/256 couleurs) ; sur une machine **EGA seule** (pas de
mode 13h), le chargement d'image echoue proprement et le jeu reste jouable
en texte seul -- meme repli que le moniteur monochrome sur Atari ST (cf.
`atarist/src/scr.c:st_mono`). Un vrai mode graphique EGA planaire (16
couleurs) reste a ecrire, cf. "Restant" plus bas.

## Installer la chaine de build

Compilateur croise **Open Watcom v2** (snapshot CI officiel -- pas de
paquet apt, binaires Linux precompiles) :

```bash
mkdir -p ~/opt && cd ~/opt
curl -Lo ow-snapshot.tar.xz \
  https://github.com/open-watcom/open-watcom-v2/releases/download/Last-CI-build/ow-snapshot.tar.xz
tar xJf ow-snapshot.tar.xz ./binl ./lib286 ./lib386 ./h
```

`WATCOM` doit pointer vers ce dossier (`~/opt` par defaut, cf. `Makefile`,
surchargeable : `make WATCOM=/autre/chemin`).

Emulateur de test (DOSBox) :

```bash
sudo apt install dosbox mtools
```

`mtools` sert a fabriquer la disquette `.img` (deja installe sur la plupart
des distributions Ubuntu/WSL).

Verifier l'installation :

```bash
~/opt/binl/wcc -0 -mc -zq
dosbox --version
```

## État actuel (2026-09-15)

Le cœur portable + la couche UI compilent et se lient sans erreur
(`make all`, modele **compact** -- code near, donnees far, cf.
`src/diskio.c` pour pourquoi le modele small ne suffit pas ici) ; le binaire
resultant **boote et tourne sous DOSBox** depuis une vraie disquette FreeDOS
(secteur de boot + `KERNEL.SYS`/`COMMAND.COM` officiels, cf.
`freedos/README.md`) jusqu'au menu du jeu, verifie par un test de non-crash
(le programme reste bloque en attente clavier au lieu de rendre la main a
DOS).

Mode texte (`scr.c`, page `$B800` en ecriture directe, 80 colonnes) et mode
graphique VGA 13h (page `$A000`, avec overlay texte 40 colonnes via une
police maison 8x8, cf. `src/font8x8.h`) **verifies a l'octet/au pixel pres**
en relisant la memoire video depuis l'hote apres execution sous DOSBox (pas
de capture d'ecran possible dans cet environnement de developpement --
WSLg/Wayland empeche `x11grab` de voir le contenu reel de la fenetre DOSBox,
confirme a l'essai) : texte, accents (CP437), video inverse, positionnement
curseur et rendu de police tous corrects sur cette base, chargement d'une
vraie image `.PCX` (RLE + palette) inclus. Clavier (`kbhit()`/`getch()`, cf.
"Notes techniques") non verifie par ce moyen -- une lecture memoire ne
capture pas une frappe, seul un humain devant l'ecran peut le confirmer.

Convertisseur d'image PNG/JPG -> `.PCX` ecrit et verifie visuellement
(`img2dos/img2dos.py`, RLE + palette 254 couleurs adaptatives + 2 reservees
pour le texte -- format DOS standard de l'epoque (ZSoft), pas invente pour
l'occasion, meme principe que le Degas `.PI1` reel cote Atari ST). Portage
autonome : chaque plateforme quantifie depuis les PNG source, aucune ne
depend de la sortie d'une autre. Les 4 images de *L'Homme en Costume Blanc*
(`adventures/homme_costume_blanc/img/dos/*.PCX`) en sont issues -- le DAC
VGA (256 couleurs, 6 bits/canal) rend nettement plus vif que la palette ST
(16 couleurs, 3 bits/canal) sur les visuels qui ont reellement de la
couleur (penthouse, menu) ; `urne`/`musee` restent proches du niveau de
gris, fidelement, parce que leurs PNG source le sont deja (ecart R-B max
mesure : 5 a 15 sur 255, verifie avant conversion, pas du tout un bug de
quantification).

Un second outil, `img2dos/pi1_to_pcx.py`, reconditionne directement un
`.PI1` Atari ST deja converti en `.PCX` (memes pixels/palette, juste un
autre conteneur) -- garde pour les cas ou aucun PNG source n'existe plus :
`BOOT00.PCX` (splash du player, l'elephant SEDP) n'a pas de source PNG
retrouvee dans le depot, seul son `.PI1` (`player/atarist/BOOT00.PI1`)
existe encore -- exception assumee, pas l'usage par defaut.

Son laisse de cote pour l'instant (`src/snd.c`, bouchon complet) : une
prochaine passe visera la Sound Blaster (detection DSP + DMA), plutot que le
haut-parleur PC (choix du 2026-09-14, decision deliberee).

## Notes techniques propres a ce portage

- **Modele memoire compact (`-mc`)**, pas small : `diskio.c` charge le
  `STORYnn.DAT` courant entier en RAM (jusqu'a 62,5 Ko, cf. son entete) --
  depasse a lui seul un segment de donnees "near" (64 Ko DGROUP, partage
  avec TOUTES les variables globales du moteur en modele small).
- **`wlink` a besoin de `@$(WATCOM)/binl/wlink.lnk`** explicitement passe en
  ligne de commande (cf. Makefile) pour connaitre `SYSTEM dos` -- ce fichier
  n'est PAS charge tout seul quand `wlink` est invoque par un chemin absolu
  (constate en pratique), contrairement a une invocation par simple nom
  trouve via `PATH`.
- **Overlay texte en mode graphique (mode mixte/image)** : le BIOS (INT 10h,
  AH=0Eh, "teletype output") est **documente** comme fonctionnant en mode
  graphique, mais sous DOSBox il efface tout le bloc 8x8 a la couleur 0
  avant d'y dessiner le glyphe (verifie par dump memoire) -- inutilisable
  pour poser du texte PAR-DESSUS une image. `scr.c` dessine donc ses propres
  glyphes pixel par pixel, avec une police maison 8x8 domaine public
  (`src/font8x8.h`, cf. son entete pour la provenance).
- **Clavier : `kbhit()`/`getch()` (`conio.h`), pas d'appel BIOS/DOS a la
  main.** Marchent identiquement en mode texte ET graphique (fonctions
  clavier, sans rapport avec l'ecran) -- pas de raison de reimplementer une
  fonction de bibliotheque deja eprouvee. En particulier, `INT 16h/AH=01h`
  ("touche disponible ?") ne doit PAS etre teste via `int86()` par
  `AX != 0` en repli faute d'acces au fanion Z (Open Watcom n'expose que
  `cflag`, la retenue, via `union REGS`) : rien ne garantit que le BIOS
  remette `AX` a 0 quand aucune touche n'attend (Ralf Brown's Interrupt
  List), et fixer `AH` avant l'appel (necessaire pour selectionner la
  fonction) rend alors ce test pratiquement toujours vrai, y compris sans
  touche pressee -- `scr_flush()`/`scr_poll()` deviendraient faussement
  bloquants (cf. `src/scr.c`, section clavier).
- **Disquette bootable** : repart du secteur de boot FreeDOS 1.3 officiel
  (`freedos/BOOTSECT.BIN`) via `mformat -B` a chaque fabrication d'image,
  plutot que de modifier une image existante -- un volume toujours frais,
  jamais de fichier deplace entre deux versions du contenu.

## Utilisation

```bash
cd player/dos
make run ADV=combat_demo     # compile + lance sous DOSBox (mount direct, rapide a iterer)
make img ADV=combat_demo     # fabrique une vraie disquette .img bootable (720 Ko, FAT12)
# -> adventures/combat_demo/build/combat_demo.img
make hosttest                 # rejoue le coeur du moteur avec gcc HOTE, sans DOS ni Watcom
```

## Restant

- Mode graphique **EGA** (320x200/16 couleurs, planaire) pour les machines
  sans VGA -- aujourd'hui, ces machines jouent en texte seul (repli
  automatique, cf. plus haut).
- Son **Sound Blaster** (`src/snd.c`, bouchon complet pour l'instant).
- Rejoue humaine sur DOSBox/materiel reel (clavier physique, lisibilite a
  l'ecran) -- tout ce document verifie est verifie par relecture d'octets,
  jamais par un œil humain devant l'ecran.
- Clavier francais (`KEYB FR`) : absent de la disquette minimale
  aujourd'hui, donc `fold_accent` (cf. `src/scr.c`) reste sans effet pratique
  tant qu'il n'est pas ajoute.
