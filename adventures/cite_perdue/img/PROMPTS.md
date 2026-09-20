# La Cité perdue : prompts d'illustration

Prompts prêts à coller dans un générateur d'images (Midjourney, DALL·E,
Copilot, Flux...). Ils sont en anglais, car les générateurs le comprennent
mieux. Chaque prompt se compose de **la scène** suivie du **bloc de style**
commun (à coller tel quel à la fin).

## Contraintes HGR (à respecter dès la génération)

- Toutes les images (écrans fixes et scènes `@splash`) : **280 x 192**, plein écran,
  ratio 35:24, par exemple 1400 x 960.
- **Une seule famille de couleurs par image** : soit *violet + vert*, soit *bleu + orange*
  (toujours avec noir et blanc). Ne jamais mélanger les deux familles.
- Beaucoup de noir, aplats francs, dégradés tramés (dithering), traits épais.
  Les traits fins d'un pixel doivent être noirs ou blancs.

### Bloc de style à ajouter à la fin de chaque prompt

Famille **bleu + orange** :

```
Style: 1980s Apple II hi-res graphics, retro adventure game illustration in the
spirit of a Franco-Belgian comic (ligne claire), bold black outlines, flat
color areas, ordered dithering instead of gradients, large readable shapes,
lots of solid black. Strictly limited palette: pure black, pure white,
bright blue and bright orange only. No other colors, no anti-aliasing,
no text, no lettering, no frame. Pixel art look, 280x192 resolution.
```

Famille **violet + vert** : même bloc, en remplaçant la palette par
`bright violet and bright green only`.

### Conversion

```
cd player/apple2/img2hgr
.venv/bin/python img2hgr.py ../../../adventures/cite_perdue/img/src/ID.png \
    -o ../../../adventures/cite_perdue/img/named/ID.HGR --preview /tmp/apercu.png
```

Le menu se convertit vers `img/MENU.HGR` et le boot vers `img/BOOT00.HGR`.
Ajouter `--preset flat` si le rendu paraît trop contrasté.

---

## 1. Écrans fixes

### `BOOT00` : écran de démarrage (plein écran, bleu + orange)

```
A ruined stone gate lost in a jungle of giant ferns, seen from below at dusk;
above the lintel three carved signs: a triangle, a radiant sun disc and three
wavy lines. A single small explorer silhouette with a hat and a lantern stands
at the foot of the steps, dwarfed by the monument. Huge orange sky with a low
black mountain ridge. Dramatic, mysterious, symmetrical composition.
```

### `MENU` : écran-titre (plein écran, bleu + orange)

Laisser le tiers inférieur assez sombre : le menu à quatre entrées s'y superpose.

```
The emblem of a lost civilization, centered: a sun with three long rays
resting on three wavy water lines, carved in massive stone, glowing orange
against a black background. Around it, the silhouette of a hidden valley
ringed by jagged peaks, and a thin waterfall on the right. Empty dark area at
the bottom third of the picture for menu text. Monumental, symmetric,
mysterious, very graphic.
```

---

## 2. Scènes (`@splash`, plein écran)

Pour chaque scène : l'identifiant du fichier `img/named/ID.HGR`, la ou les
sections où elle s'accorde le mieux, et la famille de couleurs conseillée.

### Chapitre 1 : Paris

#### `bureau` : le bureau du professeur Delorme (violet + vert) : `c1_bureau`

```
Cluttered study of a 1920s archaeologist at night: a big wooden desk with an
open empty safe in the wall behind it, a torn map in three pieces on the
desk with a magnifying glass, shelves crowded with statuettes, a window with
its latch torn off and a single leather glove on the carpet. A desk lamp
casts a hard triangular light. Noir mystery mood.
```

#### `symbole` : le symbole du journal (bleu + orange) : `c1_phrase`

```
Extreme close-up of an old worn notebook cover and a brass magnifying glass
lens revealing a tiny engraving: a sun with three rays sitting on three wavy
lines. The rest of the page is dark. The lens shows the symbol enlarged in
bright orange, everything else is black and blue.
```

### Chapitre 2 : Puerto Vidal

#### `quai` : l'arrivée d'Albion (bleu + orange) : `c2_bw_arrive`

```
A busy tropical harbor quay in the 1920s: fishermen with nets, crates,
seagulls, and in the background an elegant white steam yacht arriving with a
tall man in a light suit and a bowler hat on the gangway, followed by a
huge man in a dark jacket. A small explorer in the foreground looks at them.
Late afternoon sun, orange sky, long shadows.
```

### Chapitre 3 : le fleuve

#### `poste` : le poste abandonné (violet + vert) : `c3b_hub`

```
A half-collapsed log cabin overgrown by lianas on a pale sand riverbank at
dusk, a rotten wooden jetty, a broad dark river vanishing into a wall of
jungle, two small wooden crosses in front of the hut, fireflies. Silent,
abandoned, slightly eerie.
```

### Chapitre 4 : le territoire inconnu

#### `marais` : le marais et le jaguar (violet + vert) : `c4_marais2`

```
A misty swamp path between tall trees with aerial roots, notches carved in
a trunk in the foreground (two vertical cuts), and in the distance two
glowing eyes of a black jaguar between the ferns. A hand holding an oil lamp
enters the frame from the left. Tense, dark, lots of black.
```

#### `cascade` : la cascade et la porte scellée (bleu + orange) : `c4b_derriere`

```
Seen from inside a cave behind a huge waterfall: a curtain of white water in
the background lit by orange light, and in the foreground a perfectly smooth
stone door with a lintel carved with three signs (a triangle, a sun disc, three
waves) and, beside them, a sun with three rays over three waves. A small
figure holds up a lantern in front of it.
```

### Chapitre 5 : les montagnes

#### `col` : l'aube au col des Trois Croix (bleu + orange) : `c5b_col_aube`

```
A high mountain pass at dawn with three weathered stone crosses covered in
lichen; a single narrow ray of sunlight passes through a slit in the rock and
lights up a smooth cliff face covered in carved signs: a barred sack and an
open hand reaching for three waves. Cold blue shadows, one orange ray.
```

#### `vallee` : la vallée cachée vue depuis l'escalier (bleu + orange) : `c5b_escalier`

```
Panoramic view from a rocky terrace high above a hidden circular green
valley surrounded by sheer peaks: terraced fields, shining canals, a blond
stone city in the middle, tiny lights. Two explorers seen from behind in the
foreground holding an unfolded map. Sunset sky, breathtaking scale.
```

### Chapitre 6 : la civilisation isolée

#### `porte` : la porte monumentale (bleu + orange) : `c6_start`

```
A monumental stone gate ten meters high with a carved lintel of three signs,
open doors, two guards in wool tunics with bronze spears blocking the way, and
a tall gray-eyed man with a tattoo and a bronze pendant walking toward a group
of tired explorers who have laid their weapons on the ground. Evening light,
terraced fields behind the gate.
```

### Chapitre 7 : le conflit

#### `trone` : le palais et le Conseil (violet + vert) : `c7_start`

```
Long stone throne hall with tall pillars decorated with five-ray suns,
an old thin king in a red cloak sitting on a plain stone seat, a massive
general in leather armor standing on his left with cold eyes, a tall gray-eyed
man on his right. Light falls from high openings. Tense political scene.
```

### Chapitre 8 : les ruines anciennes

#### `statue` : la statue de la place (bleu + orange) : `c8_statue`

```
A vast ruined plaza with a colossal stone guardian statue, bare-chested, one
arm stretched out horizontally, standing on a circular base with a lever. Broken
columns, moss, a dome-topped observatory tower on the left, a library on
the right, a huge closed door at the back. Bright morning light from the
right, long shadows.
```

### Chapitre 9 : la cité perdue

#### `cite` : la caverne aux puits de lumière (bleu + orange) : `c9_start`

Image phare de l'aventure : à soigner particulièrement.

```
A gigantic underground cavern like a cathedral, holding a perfectly
preserved ancient city: a wide black-paved avenue, white marble palace with
columns on the left, a stepped temple at the end. Dozens of golden light
beams fall from shafts in the roof and bounce from mirror to mirror down the
avenue. Tiny explorers in the middle of the avenue for scale. Silence and awe.
```

### Chapitre 10 : la salle secrète

#### `miroirs` : la salle des miroirs (bleu + orange) : `c10_miroirs`

```
A square room lined with polished bronze, a single vertical beam of light
falling from a ceiling shaft onto a first pedestal with a mirror, then a
zigzag of bright orange beams jumping between three mirrors on three pedestals
arranged in an L shape, ending on a glyph on the wall that glows: a sun with
three rays over three waves. Black everywhere else.
```

### Chapitre 11 : le trésor

#### `coeur` : le Coeur d'Uruvan (bleu + orange) : `c11_coeur`

```
A round vaulted chamber full of piles of gold, masks, gems and clay tablets
on stone shelves; in the center on a black stone pedestal a sphere of crystal
and bronze surrounded by movable rings, projecting a starry sky with seven
connected points onto the ceiling. A kneeling man in a light suit stares at it.
```

### Chapitre 12 : le piège

#### `piege` : le couloir effondré (violet + vert) : `c12_couloir`

```
A collapsing ancient stone corridor: falling blocks, clouds of dust, a
four-meter chasm cutting the floor, a stone pillar with a rope tied to it,
explorers rappelling across in panic and a man in a dark jacket with a
revolver on the far edge holding a heavy bag. Dark, chaotic, dramatic.
```

### Chapitre 13 : l'évasion

#### `sortie` : la sortie derrière la cascade (bleu + orange) : `c13_cascade`

```
A stone door swinging open behind a huge waterfall, a rocky ledge, mist and
orange sunset light pouring in, exhausted explorers stepping out; on the bank
beyond the veil of water three silhouettes wait: a tall man standing straight,
a young girl with braids and a man in a red felt hat.
```

#### `carte` : la seconde destination (violet + vert) : `c13_epilogue_gardien`

```
Top view of an old worn map spread on a wooden table at sunset next to a
small black notebook; a second circle drawn in ink lights up far to the
south-east of the map, a faint line appearing at the bottom of the notebook
page. Warm light, a hand holding the map corner. Quiet, hopeful, mysterious.
```

---

## 3. Intégration dans l'aventure

Chaque image est déjà branchée par un `@splash` dans les sections indiquées
plus haut : l'image s'affiche en plein écran avant le texte, une touche passe
(ou quelques secondes, selon la section). Il suffit de **déposer le fichier
converti** pour que l'image apparaisse. Sans fichier, la section commence
directement par son texte : c'est le cas de l'Apple II, où la disquette est
pleine.

| Plateforme | Où déposer l'image |
|---|---|
| Apple II | `img/named/<ID EN MAJUSCULES>.HGR` (ex. `img/named/CITE.HGR`) |
| Atari ST | `img/atarist/IMGnn.PI1` (`nn` : ordre dans `build/IMAGES.MAP`) |
| PC DOS | `img/dos/IMGnn.PCX` |
| Web | `img/web/<ID EN MAJUSCULES>.png` (ex. `img/web/CITE.png`) |

`BOOT00` et `MENU` sont les écrans fixes (`img/BOOT00.HGR`, `img/MENU.HGR`),
sans `@splash`.
