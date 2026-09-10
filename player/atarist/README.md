# Player Atari ST (portage — en cours)

Ce dossier réutilise le cœur C partagé avec `player/apple2/` (`story.c`,
`state.c`, `combat.c`, ..., et le format binaire `STORY.DAT`/`APP.LNG`) sans
rien y changer ; seuls l'affichage (`scr.c`), le son et le disque sont
spécifiques à l'Atari ST.

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

## État actuel (2026-09-10)

Le cœur portable + la couche UI compilent et se lient sans avertissement
(`make all`), et une aventure texte (`combat_demo`) **charge et se joue**
sous Hatari+EmuTOS (menu, choix, accents à l'écran). `fseek()` de `mintlib`
est cassé sous l'émulation GEMDOS-HDD de Hatari (confirmé et isolé via
`smoketest/fseek_test.c`) ; contourné dans la copie ST de `story.c` en
chargeant chaque `STORYnn.DAT` entièrement en RAM (64 Ko max, garanti par
le compilateur) plutôt qu'en cherchant dedans.

Mode graphique écrit (`scr_gfx_*`/`scr_load_hgr`, basse résolution 320×200
16 couleurs pour les images, bascule automatique depuis le texte 80
colonnes) et convertisseur PNG/JPG → `.PI1` (`img2st/img2st.py`) — pas
encore vérifié à l'écran sous Hatari/matériel réel.

Son écrit (`snd.c`, YM2149 natif du ST accédé directement en $FF8800/
$FF8802, même modèle de registres que le Mockingboard Apple II mais deux
voies dédiées à la musique + une aux effets, un seul chip oblige) — pas
encore vérifié à l'oreille sous Hatari/matériel réel non plus.
