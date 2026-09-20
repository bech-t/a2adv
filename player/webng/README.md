# a2adv — player web (Angular)

Le portage navigateur d'a2adv (avec l'Apple II, l'Atari ST et le DOS) : le
même jeu, la même architecture de fond, pensé pour le mobile (lecture hors
ligne, sauvegarde locale, « ajouter à l'écran d'accueil »). Angular 22,
composants autonomes et signaux.

## Le site : catalogue et jeu

Le player est un site statique, sans serveur applicatif. Sa page d'accueil
liste les aventures publiées ; chacune a sa fiche, puis se joue dans la page.

| adresse (routes en `#`) | écran |
|---|---|
| `#/` | le catalogue : une carte par aventure (couverture, titre, auteur, description, pastille « Partie en cours ») |
| `#/adv/<id>` | la fiche : présentation, emplacements de sauvegarde, téléchargement hors ligne |
| `#/play/<id>` | le jeu ; `?mode=new` démarre directement, `?mode=resume` reprend l'emplacement, `?slot=1..3` choisit l'emplacement |

Les routes en `#` marchent sur n'importe quel hébergement statique, sans règle
de réécriture. Les chemins des fichiers du site sont relatifs à la page
(`<base href>`) : il peut être servi à la racine d'un domaine ou dans un
sous-dossier (`make site BASE_HREF=/a2adv-site/`). L'ancien lien `?adv=<id>`
mène toujours au jeu.

## De `.adv` à l'écran

```
adventures/<nom>/<nom>.adv          (texte, écrit par l'auteur)
        │
        │  python3 -m a2c.site      (compiler/, en Python ; cf. adventures.txt)
        ▼
public/catalog.json                 (la liste : titre, auteur, description…)
public/adventures/<nom>/<hash>/
    story.json                      (résolu : noms → indices)
    img/IMGnn.webp, cover.webp      (images converties, redimensionnées)
        │
        │  fetch() + JSON.parse     (src/app/assets/catalog.ts, engine/story.ts)
        ▼
Engine                              (machine à états : quelle vue afficher,
                                      quelles actions le joueur peut faire —
                                      AUCUN import Angular ici)
        │
        │  engine.subscribe → signal (src/app/ui/game.ts)
        ▼
Composants Angular (ui/)            (ce que le joueur voit et touche)
```

Les aventures publiées sont listées dans `adventures.txt` (un nom par ligne).
`public/catalog.json` et `public/adventures/` sont générés par `npm run sync`
(lancé avant `dev` et `build`), jamais commités. Une aventure a besoin de sa
description (`@description` dans le `.adv`), de ses images (`img/web/*.png`) et
d'une couverture (`img/web/MENU.png`, ou à défaut son `img/MENU.HGR` Apple II)
pour un catalogue complet ; sans elles la carte affiche simplement son titre.

## Jouer hors ligne

Le Service Worker (`public/sw.js`, écrit à la main) traite trois familles de
fichiers, chacune avec sa stratégie :

| fichiers | stratégie |
|---|---|
| l'application (`index.html`, scripts, styles) | préchargée à l'installation dans un cache propre à la version (`a2adv-app-<version>`), servie cache d'abord |
| les aventures (`adventures/<id>/<hash>/…`) | cache d'abord dans `a2adv-adv` : le dossier porte le hash de son contenu, un fichier n'y change jamais |
| `catalog.json` | réseau d'abord, repli sur la dernière copie hors ligne |

`tools/precache.mjs` (lancé après `ng build`, cf. `postbuild`) écrit dans
`sw.js` la liste des fichiers de l'application et une version calculée sur
leur contenu. Une nouvelle version s'installe **en attente** : la page
annonce « Une nouvelle version est disponible » et ne l'active qu'au clic sur
*Recharger*, pour qu'une page ouverte ne mélange jamais des fichiers de deux
versions. L'ancien cache d'application est alors supprimé, les aventures
téléchargées restent.

La fiche d'une aventure propose *Télécharger pour jouer hors ligne* : la page
(`src/app/assets/offline.ts`) récupère tous les fichiers du dossier de
l'aventure (liste `files` du catalogue) et les range dans le cache. Jouer en
ligne en garde aussi au fil de la lecture, mais seul le téléchargement
complet donne la pastille « Hors ligne ». Les dossiers de versions qui ne
figurent plus au catalogue sont supprimés du cache.

## Sauvegardes

Trois emplacements par aventure, dans le `localStorage` du navigateur
(`a2adv:save:<id>:<n>`). Chaque emplacement garde la date et le hash de
l'aventure qui l'a écrit ; si l'aventure a été modifiée depuis, la fiche le
signale. Le moteur refuse de charger une sauvegarde dont la forme ne
correspond plus (autre nombre de sections, de caractéristiques, d'objets ou de
drapeaux) au lieu de se bloquer. Une ancienne sauvegarde unique devient
l'emplacement 1.

*Exporter* télécharge l'emplacement dans un fichier `a2adv-<id>-emplacement<n>.json` ;
*Importer* le relit dans l'emplacement de son choix, après avoir vérifié qu'il
s'agit bien d'une sauvegarde de cette aventure. C'est la façon de passer une
partie d'un appareil à un autre : le `localStorage` ne sort pas du navigateur.
*Nouvelle partie* n'efface pas l'emplacement : l'ancienne sauvegarde n'est
remplacée qu'à la prochaine sauvegarde.

`src/app/engine/` ne dépend d'aucun framework : ce sont les règles du jeu
(combat, inventaire, conditions, effets), testées en Node sans navigateur
(`src/app/engine/hostplay.spec.ts`). `src/app/assets/` (chargement,
sauvegarde `localStorage`) et `src/app/audio/` (sons synthétisés en Web
Audio) sont aussi indépendants d'Angular. Seule la couche d'affichage
(`src/app/ui/`, `src/app/app.ts`) utilise Angular.

## Comment Angular est utilisé ici

| besoin | mécanisme |
|---|---|
| composant | classe décorée `@Component`, template séparé (`.html`) ou inline (`template: ...`) |
| état local | `signal(...)`, lu avec `maValeur()`, écrit avec `maValeur.set(...)` |
| dérivé | `computed(() => ...)`, mémoïsé automatiquement |
| props | champs `input.required<T>()` (ou `input(défaut)`), lus avec `maProp()` |
| callback vers le parent | `output<T>()`, le parent écoute avec `(close)="..."` |
| s'abonner à `Engine` (hors Angular) | `ngOnInit` + `engine.subscribe(...)` + un `signal`, désabonné via `DestroyRef` (cf. `ui/game.ts`) |
| conditionnelle / boucle dans le template | `@if`/`@else`, `@for (... track ...)`, `@switch`/`@case` |
| union discriminée (`View`) dans le template | le compilateur de template ne narrowe pas les getters : `ui/game.ts` a une petite méthode par branche (`sceneView()`, `combatView()`...) qui fait le cast |

Un piège rencontré dans `ui/ask-input.ts` : la fonction `input()` (pour
déclarer une prop) et un champ qu'on aurait naturellement appelé `input` (la
vue `InputView`) se marchent dessus à la lecture. Le champ s'appelle
`inputView`.

## Lancer / tester

```bash
npm install
npm run dev             # http://localhost:4200 (ng serve), toutes les aventures de adventures.txt
ADVENTURES="combat_demo" npm run dev   # seulement celles-ci (plus rapide)
npm test                # rejoue combat_demo en Node (ng test / Vitest)
make site               # site de production dans dist/webng/browser
CHROME=/chemin/vers/chrome make e2e   # scénario hors ligne dans un vrai navigateur
```

`npm run sync` (appelé automatiquement avant `dev`/`build`, cf. `package.json`)
invoque `python3 -m a2c.site` : Python 3 est requis, et Pillow
(`pip install pillow`) pour convertir les images.

`make e2e` (après `make site`) lance `tools/e2e-offline.mjs` : Chromium piloté
par le protocole DevTools, sans dépendance à installer, installe le site,
télécharge une aventure, coupe le serveur, joue, sauvegarde, exporte, importe,
puis vérifie l'annonce d'une nouvelle version. Il faut un Chromium ou Chrome,
indiqué par `CHROME`.

Le fichier de langue `lang/web/fr.lng` surcharge les chaînes du socle
`lang/fr.lng` avec des libellés adaptés au navigateur (« Nouvelle partie »,
« Annuler »…) ; les chaînes propres au catalogue (« Livres-jeux », « Jouer »…)
sont écrites dans les composants.
