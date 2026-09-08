import { AdvParser } from './adv-parser';
import { validateDocument } from './adv-validate';

const p = new AdvParser();

const DEMO = `@title  Nouvelle aventure
@author Vous
@start  depart

:: depart
La porte du temple s'ouvre sur le noir.

* [Entrer sans lumiere] -> noir
* [Rebrousser chemin] -> fin_lache

:: noir
Vous n'y voyez rien. Un bruit, tout pres.

* [Foncer] -> fin_victoire
* [Reculer] -> depart

:: fin_victoire
@ending win
Vous ressortez, le tresor sous le bras.

:: fin_lache
@ending lose
Vous ne saurez jamais ce qu'il y avait derriere cette porte.
`;

function messages(doc: ReturnType<typeof p.parse>): string[] {
  return validateDocument(doc).map((i) => i.message);
}

describe('validateDocument: graph checks', () => {
  it('finds nothing wrong in a clean, fully-reachable adventure', () => {
    expect(validateDocument(p.parse(DEMO))).toEqual([]);
  });

  it('flags a choice target that does not exist, and its knock-on effects', () => {
    const broken = DEMO.replace('-> fin_victoire', '-> fin_victoiree');
    const issues = validateDocument(p.parse(broken));
    expect(issues).toEqual([
      { severity: 'error', sectionName: 'noir', message: 'Le choix « Foncer » pointe vers une section inexistante : « fin_victoiree »' },
      { severity: 'warning', sectionName: 'fin_victoire', message: "Section inatteignable depuis @start (ou une scène d'intro)" },
      { severity: 'error', message: 'Aucune fin VICTOIRE atteignable' },
    ]);
  });

  it('flags an orphan section unreachable from @start', () => {
    const withOrphan = `${DEMO}\n:: orpheline\nJamais visitee.\n\n* [Retour] -> depart\n`;
    expect(messages(p.parse(withOrphan))).toEqual(["Section inatteignable depuis @start (ou une scène d'intro)"]);
  });

  it('flags a dead-end: no choices and no @ending', () => {
    const deadEnd = DEMO.replace('@ending lose\n', '');
    expect(messages(p.parse(deadEnd))).toContain('Cul-de-sac : aucun choix et pas de fin (@ending)');
  });

  it('flags a section with only conditional gotos and no unconditional fallback', () => {
    const leaky = DEMO.replace('-> fin_lache', '-> aiguillage').replace(
      ':: fin_lache\n@ending lose\n',
      ':: aiguillage\n~ {flag jamais_pose} goto fin_lache\n\n:: fin_lache\n@ending lose\n',
    );
    expect(messages(p.parse(leaky))).toContain(
      "Aiguillage sans goto inconditionnel : si aucune garde n'est vraie, le joueur reste bloqué",
    );
  });

  it('an unconditional goto in an otherwise choice-less section is NOT a dead-end', () => {
    const routed = DEMO.replace('-> fin_lache', '-> aiguillage').replace(
      ':: fin_lache\n@ending lose\n',
      ':: aiguillage\n~ goto fin_lache\n\n:: fin_lache\n@ending lose\n',
    );
    expect(messages(p.parse(routed))).toEqual([]);
  });

  it('flags the absence of any reachable victory ending', () => {
    const noWin = DEMO.replace('@ending win\n', '');
    const issues = validateDocument(p.parse(noWin));
    expect(issues.map((i) => i.message)).toContain('Aucune fin VICTOIRE atteignable');
  });
});

describe('validateDocument: combat/ask completeness', () => {
  const COMBAT_OK = `@title T
@start depart
:: depart
* [Aller] -> combat_gobelin
:: combat_gobelin
@combat "Gobelin" att=6 hp=9 dmg=3 armor=0
@win sortie "GG"
@lose mort
:: sortie
@ending win
Fin victoire.
:: mort
@ending lose
Fin defaite.
`;

  it('a complete combat section has no issues', () => {
    expect(validateDocument(p.parse(COMBAT_OK))).toEqual([]);
  });

  it('flags a @combat missing its mandatory @lose, and the section it would have reached becomes unreachable', () => {
    const missingLose = COMBAT_OK.replace('@lose mort\n', '');
    const issues = validateDocument(p.parse(missingLose));
    expect(issues).toEqual([
      { severity: 'error', sectionName: 'combat_gobelin', message: '@combat sans @lose (obligatoire)' },
      { severity: 'warning', sectionName: 'mort', message: "Section inatteignable depuis @start (ou une scène d'intro)" },
    ]);
  });

  it('flags @ask missing @answer/@correct/@wrong', () => {
    const ask = `@title T
@start coffre
:: coffre
@ask "Le mot ?"
Un coffre.
`;
    const issues = validateDocument(p.parse(ask)).map((i) => i.message);
    expect(issues).toContain('@ask sans @answer (au moins une réponse obligatoire)');
    expect(issues).toContain('@ask sans @correct (obligatoire)');
    expect(issues).toContain('@ask sans @wrong (obligatoire)');
  });
});

describe('validateDocument: numeric bounds (mirrors a2c exactly, no graph needed)', () => {
  const BASE = `@title T
@start depart
:: depart
Texte.
`;

  it('flags a @stat outside [0,255] or with min > max', () => {
    expect(messages(p.parse(`@stat X 300 0 255\n${BASE}`))).toContain('@stat X : valeurs hors [0,255]');
    expect(messages(p.parse(`@stat X 5 10 5\n${BASE}`))).toContain('@stat X : min (10) > max (5)');
  });

  it('flags an @item modifier outside [-128,127]', () => {
    expect(messages(p.parse(`@item x "X" atk=200\n${BASE}`))).toContain('@item x : modificateur hors [-128,127]');
  });

  it('flags too many flags or too many local flags', () => {
    const manyFlags = Array.from({ length: 129 }, (_, i) => `@flag f${i}`).join('\n');
    expect(messages(p.parse(`${manyFlags}\n${BASE}`))).toContain('129 drapeaux déclarés (maximum 128)');

    const manyLocal = Array.from({ length: 33 }, (_, i) => `@flag f${i} local`).join('\n');
    expect(messages(p.parse(`${manyLocal}\n${BASE}`))).toContain("33 drapeaux 'local' (maximum 32) : en rendre quelques-uns globaux");
  });

  it('flags @combat values outside their bounds (att/dmg/armor 0..255, hp 1..255)', () => {
    const zeroHp = `@title T
@start depart
:: depart
* [Aller] -> c
:: c
@combat "X" att=6 hp=0 dmg=3 armor=0
@win depart
@lose depart
`;
    expect(messages(p.parse(zeroHp))).toContain('@combat : valeurs hors bornes (att/dmg/armor 0..255, hp 1..255)');
  });

  it('flags @ask maxlen outside [1,40]', () => {
    const bad = `@title T
@start c
:: c
@ask "Q ?" maxlen=41
@answer x
@correct c
@wrong c
`;
    expect(messages(p.parse(bad))).toContain('@ask maxlen=41 hors bornes (1 à 40)');
  });
});
