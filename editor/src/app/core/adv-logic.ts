/**
 * Conditions et effets : modèle + parse/sérialisation, en miroir de la
 * grammaire réelle du compilateur (`a2c/parser.py` `_parse_condition` /
 * `_parse_atom` / `_parse_effect_body`, `a2c/model.py` `Atom`/`Condition`/
 * `Effect`). Fonctions pures, sans dépendance Angular : ni l'une ni l'autre
 * n'a d'état à porter, contrairement à `AdvParser` qui, lui, gère un document
 * entier (lignes, sections, décalages).
 *
 * Comme `AdvParser`, aucune validation sémantique (stat inconnue, section
 * absente...) — seulement la syntaxe. `a2c`/`a2c.analyze` restent la
 * référence pour un contrôle réel.
 */

export const CMP_OPS = ['==', '!=', '<', '<=', '>', '>='] as const;
export type Cmp = (typeof CMP_OPS)[number];

export type AtomKind = 'flag' | 'not_flag' | 'has' | 'not_has' | 'stat';

/** Un test booléen : `flag NOM`, `not has OBJET`, `stat NOM OP N`... */
export interface ConditionAtom {
  kind: AtomKind;
  name: string;
  cmp?: Cmp; // uniquement pour kind === 'stat'
  value?: number; // uniquement pour kind === 'stat'
}

export type Connective = 'and' | 'or';

/** Un ensemble d'atomes combinés par UN SEUL connecteur (v0 du langage :
 * pas de parenthèses, jamais de mélange and/or). `atoms` vide = toujours
 * vraie (pas de condition). */
export interface AdvCondition {
  atoms: ConditionAtom[];
  connective: Connective;
}

export function emptyCondition(): AdvCondition {
  return { atoms: [], connective: 'and' };
}

export function newAtom(kind: AtomKind = 'flag'): ConditionAtom {
  return kind === 'stat' ? { kind, name: '', cmp: '==', value: 0 } : { kind, name: '' };
}

class LogicError extends Error {}

// --- Parsing : condition ----------------------------------------------------

/** `src` = le texte entre accolades (SANS les `{ }`). Chaîne vide -> condition
 * toujours vraie. Lève une `LogicError` sur toute syntaxe non reconnue —
 * à l'appelant de décider (ex. garder le texte brut) plutôt que de deviner. */
export function parseCondition(src: string): AdvCondition {
  const toks = src.trim().split(/\s+/).filter(Boolean);
  const atoms: ConditionAtom[] = [];
  let connective: Connective = 'and';
  let seenAnd = false;
  let seenOr = false;
  let i = 0;
  while (i < toks.length) {
    const { atom, next } = parseAtom(toks, i);
    atoms.push(atom);
    i = next;
    if (i < toks.length) {
      const conn = toks[i];
      if (conn === 'and') {
        seenAnd = true;
        connective = 'and';
      } else if (conn === 'or') {
        seenOr = true;
        connective = 'or';
      } else {
        throw new LogicError(`connecteur attendu 'and'/'or', reçu '${conn}'`);
      }
      i++;
      if (i >= toks.length) throw new LogicError(`condition incomplète après '${conn}'`);
    }
  }
  if (seenAnd && seenOr) throw new LogicError("mélange 'and'/'or' interdit (pas de parenthèses)");
  return { atoms, connective };
}

function parseAtom(toks: string[], i: number): { atom: ConditionAtom; next: number } {
  const t = toks[i];
  if (t === 'not') {
    if (i + 2 >= toks.length) throw new LogicError("condition 'not' incomplète");
    const kind = toks[i + 1];
    const name = toks[i + 2];
    if (kind === 'flag') return { atom: { kind: 'not_flag', name }, next: i + 3 };
    if (kind === 'has') return { atom: { kind: 'not_has', name }, next: i + 3 };
    throw new LogicError(`'not' suivi de '${kind}' invalide (attendu flag/has)`);
  }
  if (t === 'flag') {
    need(toks, i + 1, 'flag NOM');
    return { atom: { kind: 'flag', name: toks[i + 1] }, next: i + 2 };
  }
  if (t === 'has') {
    need(toks, i + 1, 'has OBJET');
    return { atom: { kind: 'has', name: toks[i + 1] }, next: i + 2 };
  }
  if (t === 'stat') {
    if (i + 3 >= toks.length) throw new LogicError("condition 'stat' incomplète (stat NOM OP N)");
    const name = toks[i + 1];
    const op = toks[i + 2];
    const val = toks[i + 3];
    if (!(CMP_OPS as readonly string[]).includes(op)) {
      throw new LogicError(`opérateur de comparaison invalide: '${op}'`);
    }
    const value = Number(val);
    if (!Number.isFinite(value)) throw new LogicError(`valeur numérique attendue, reçu '${val}'`);
    return { atom: { kind: 'stat', name, cmp: op as Cmp, value }, next: i + 4 };
  }
  throw new LogicError(`atome de condition invalide: '${t}'`);
}

function need(toks: string[], i: number, what: string): void {
  if (i >= toks.length) throw new LogicError(`condition incomplète (attendu ${what})`);
}

/** true si `src` (contenu d'un `{...}`) est reconnu par `parseCondition` sans
 * lever — pour décider, à l'affichage, si on montre l'éditeur structuré ou le
 * texte brut en repli. */
export function isConditionParseable(src: string): boolean {
  try {
    parseCondition(src);
    return true;
  } catch {
    return false;
  }
}

// --- Sérialisation : condition ----------------------------------------------

export function atomText(a: ConditionAtom): string {
  switch (a.kind) {
    case 'flag':
      return `flag ${a.name}`;
    case 'not_flag':
      return `not flag ${a.name}`;
    case 'has':
      return `has ${a.name}`;
    case 'not_has':
      return `not has ${a.name}`;
    case 'stat':
      return `stat ${a.name} ${a.cmp} ${a.value}`;
  }
}

/** Texte à remettre entre accolades. Chaîne vide pour une condition sans
 * atome (le rendu `{}` autour n'est pas de son ressort — cf. `EffectEditor`
 * et le futur éditeur de choix, qui décident SI des accolades apparaissent). */
export function conditionText(c: AdvCondition): string {
  return c.atoms.map(atomText).join(` ${c.connective} `);
}

// --- Effets ------------------------------------------------------------------

// Verbe SOURCE ('set' couvre a la fois drapeau et stat, distingués par le
// nombre d'arguments) -> ops internes distincts, comme dans a2c/model.py.
export type EffectOp =
  | 'set'
  | 'clear'
  | 'toggle'
  | 'give'
  | 'take'
  | 'add'
  | 'sub'
  | 'setstat'
  | 'setmax'
  | 'restore'
  | 'goto'
  | 'sound'
  | 'score';

/** Ce que chaque op attend, pour piloter l'affichage du formulaire :
 * 'flag' | 'item' | 'stat' | 'section' | 'sound' -> un champ nom de ce type ;
 * `value` -> un champ numérique en plus (ou à la place, pour 'score'). */
export const EFFECT_SHAPE: Record<EffectOp, { name: 'flag' | 'item' | 'stat' | 'section' | 'sound' | 'none'; value: boolean }> = {
  set: { name: 'flag', value: false },
  clear: { name: 'flag', value: false },
  toggle: { name: 'flag', value: false },
  give: { name: 'item', value: false },
  take: { name: 'item', value: false },
  add: { name: 'stat', value: true },
  sub: { name: 'stat', value: true },
  setstat: { name: 'stat', value: true },
  setmax: { name: 'stat', value: true },
  restore: { name: 'stat', value: false },
  goto: { name: 'section', value: false },
  sound: { name: 'sound', value: false },
  score: { name: 'none', value: true },
};

/** Les 9 sons prédéfinis (cf. docs/GUIDE-FORMAT-ADV.md §9) — aucun import de
 * son personnalisé n'existe, la liste est donc fermée et fixe. */
export const SOUND_NAMES = ['select', 'error', 'win', 'lose', 'pickup', 'hit', 'magic', 'door', 'page'] as const;

export interface AdvEffect {
  op: EffectOp;
  name: string; // vide pour 'score'
  value?: number; // present selon EFFECT_SHAPE[op].value
  cond: AdvCondition; // garde optionnelle ; emptyCondition() = toujours applique
}

/** Nom qui n'existe forcément pas — même rôle que `AdvParser`'s cible
 * `A_COMPLETER` pour un `@win`/`@correct` fraîchement ajouté : un effet créé
 * vide (`set FLAG` sans FLAG) ne serait même pas reconnu par `parseEffect`
 * (round-trip cassé dès la création), et resterait invisible en texte brut
 * au lieu du formulaire structuré. */
const PLACEHOLDER_NAME = 'A_COMPLETER';

export function newEffect(op: EffectOp = 'set'): AdvEffect {
  const shape = EFFECT_SHAPE[op];
  return { op, name: shape.name === 'none' ? '' : PLACEHOLDER_NAME, value: shape.value ? 0 : undefined, cond: emptyCondition() };
}

// --- Parsing : effet ---------------------------------------------------------

/** `src` = le texte après le `~` (SANS le `~` lui-même), garde `{...}`
 * optionnelle en tête comprise. */
export function parseEffect(src: string): AdvEffect {
  let body = src.trim();
  let cond = emptyCondition();
  if (body.startsWith('{')) {
    const end = body.indexOf('}');
    if (end < 0) throw new LogicError("condition d'effet non fermée (manque '}')");
    cond = parseCondition(body.slice(1, end));
    body = body.slice(end + 1).trim();
  }
  const eff = parseEffectBody(body);
  eff.cond = cond;
  return eff;
}

function parseEffectBody(src: string): AdvEffect {
  const toks = src.split(/\s+/).filter(Boolean);
  if (!toks.length) throw new LogicError('effet vide');
  const verb = toks[0];
  const a = toks.slice(1);
  const int = (s: string): number => {
    const n = Number(s);
    if (!Number.isInteger(n)) throw new LogicError(`entier attendu, reçu '${s}'`);
    return n;
  };
  const need1 = (): string => {
    if (a.length !== 1) throw new LogicError(`effet '${verb}' attend un seul argument`);
    return a[0];
  };

  if (verb === 'clear' || verb === 'toggle') return { op: verb, name: need1(), cond: emptyCondition() };
  if (verb === 'set') {
    if (a.length === 1) return { op: 'set', name: a[0], cond: emptyCondition() };
    if (a.length === 2) return { op: 'setstat', name: a[0], value: int(a[1]), cond: emptyCondition() };
    throw new LogicError("effet 'set' invalide (set FLAG | set STAT N)");
  }
  if (verb === 'give' || verb === 'take') return { op: verb, name: need1(), cond: emptyCondition() };
  if (verb === 'add' || verb === 'sub') {
    if (a.length !== 2) throw new LogicError(`effet '${verb}' attend STAT N`);
    return { op: verb, name: a[0], value: int(a[1]), cond: emptyCondition() };
  }
  if (verb === 'goto') return { op: 'goto', name: need1(), cond: emptyCondition() };
  if (verb === 'sound') return { op: 'sound', name: need1(), cond: emptyCondition() };
  if (verb === 'score') {
    if (a.length !== 1) throw new LogicError("effet 'score' attend N (points a ajouter)");
    return { op: 'score', name: '', value: int(a[0]), cond: emptyCondition() };
  }
  if (verb === 'restore') return { op: 'restore', name: need1(), cond: emptyCondition() };
  if (verb === 'setmax') {
    if (a.length !== 2) throw new LogicError("effet 'setmax' attend STAT N");
    return { op: 'setmax', name: a[0], value: int(a[1]), cond: emptyCondition() };
  }
  throw new LogicError(`effet inconnu: '${verb}'`);
}

export function isEffectParseable(src: string): boolean {
  try {
    parseEffect(src);
    return true;
  } catch {
    return false;
  }
}

// --- Sérialisation : effet -----------------------------------------------

/** Verbe + arguments SOURCE (sans le `~` ni la garde) : `set STAT 12`,
 * `restore SANG_FROID`, `score -5`... */
export function effectBodyText(e: AdvEffect): string {
  const shape = EFFECT_SHAPE[e.op];
  const verb = e.op === 'setstat' ? 'set' : e.op;
  if (shape.name === 'none') return `${verb} ${e.value ?? 0}`;
  if (shape.value) return `${verb} ${e.name} ${e.value ?? 0}`;
  return `${verb} ${e.name}`;
}

/** Ligne complète, `~` compris : `~ {cond} verbe args` ou `~ verbe args`. */
export function effectLine(e: AdvEffect): string {
  const guard = e.cond.atoms.length ? `{${conditionText(e.cond)}} ` : '';
  return `~ ${guard}${effectBodyText(e)}`;
}
