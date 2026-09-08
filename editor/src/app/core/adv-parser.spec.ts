import { AdvParser, type AdvDocument } from './adv-parser';
import { newEffect } from './adv-logic';

const FIXTURE = `@title  Mon aventure
@author Moi
@start  depart
@intro  prologue

@stat ADRESSE 8 0 12
@flag porte_ouverte
@item torche "Torche" on

:: prologue
Un prologue.

* [Continuer] -> depart

@chapter "Chapitre 1"

:: depart
~ score 5
La porte du temple s'ouvre sur le noir.

* {has torche} [Entrer avec la torche] -> noir
  ~ set vu_la_porte
* [Rebrousser chemin] -> fin_lache

@on_enter
~ set toujours_vrai

:: noir
Vous n'y voyez rien.

* [Foncer] -> fin_victoire

:: fin_victoire
@ending win
Vous ressortez, le tresor sous le bras.

:: fin_lache
@ending lose
Vous ne saurez jamais.

:: combat_gobelin
@combat "Gobelin" att=6 hp=9 dmg=3 armor=0 image=gobelin
@win fin_victoire "Le gobelin s'effondre."
  ~ score 100
  ~ give tresor
@lose fin_lache
@flee depart
Le gobelin bondit !

* [Affronter] -> combat_gobelin

:: coffre
@ask "Le mot a composer ?" maxlen=20
@answer XIPE TOTEC
@answer ECORCHE
@correct fin_victoire
  ~ score 30
@wrong fin_lache
Un coffre a cadran de lettres.
`;

describe('AdvParser.parse', () => {
  let p: AdvParser;
  let doc: AdvDocument;

  beforeEach(() => {
    p = new AdvParser();
    doc = p.parse(FIXTURE);
  });

  it('reads the preamble', () => {
    expect(doc.title).toBe('Mon aventure');
    expect(doc.author).toBe('Moi');
    expect(doc.start).toBe('depart');
    expect(doc.stats).toEqual([{ line: 5, name: 'ADRESSE', init: 8, lo: 0, hi: 12, hidden: false }]);
    expect(doc.flags).toEqual([{ line: 6, name: 'porte_ouverte', on: false, local: false }]);
    expect(doc.items).toEqual([{ line: 7, name: 'torche', label: 'Torche', on: true, atk: 0, dmg: 0, armor: 0 }]);
  });

  it('splits sections into chapters at @chapter boundaries', () => {
    expect(doc.chapters.length).toBe(2);
    expect(doc.chapters[0].title).toBe('');
    expect(doc.chapters[0].sections.map((s) => s.name)).toEqual(['prologue']);
    expect(doc.chapters[1].title).toBe('Chapitre 1');
    expect(doc.chapters[1].sections.map((s) => s.name)).toContain('depart');
  });

  it('extracts choices with their condition, label and target', () => {
    const depart = doc.sections.find((s) => s.name === 'depart')!;
    expect(depart.choices.length).toBe(2);
    expect(depart.choices[0].cond).toBe('has torche');
    expect(depart.choices[0].label).toBe('Entrer avec la torche');
    expect(depart.choices[0].target).toBe('noir');
    expect(depart.choices[1].cond).toBeNull();
  });

  it('attaches entry effects (implicit and via @on_enter) separately from choice effects', () => {
    const depart = doc.sections.find((s) => s.name === 'depart')!;
    expect(depart.entryEffects.map((e) => e.raw)).toEqual(['score 5', 'set toujours_vrai']);
    expect(depart.choices[0].effects.map((e) => e.raw)).toEqual(['set vu_la_porte']);
    expect(depart.choices[1].effects).toEqual([]);
  });

  it('computes section kind flags (start, ending, combat, ask)', () => {
    const byName = (n: string) => doc.sections.find((s) => s.name === n)!;
    expect(byName('depart').kind.isStart).toBe(true);
    expect(byName('prologue').kind.isIntro).toBe(true);
    expect(byName('fin_victoire').kind.ending).toBe('win');
    expect(byName('fin_lache').kind.ending).toBe('lose');
    expect(byName('combat_gobelin').kind.hasCombat).toBe(true);
    expect(byName('coffre').kind.hasAsk).toBe(true);
  });

  it('parses @combat and routes @win/@lose/@flee effects to their own outcome, not to entry/choice', () => {
    const cb = doc.sections.find((s) => s.name === 'combat_gobelin')!;
    expect(cb.combat).toEqual(
      expect.objectContaining({
        name: 'Gobelin',
        att: 6,
        hp: 9,
        dmg: 3,
        armor: 0,
        image: 'gobelin',
      }),
    );
    expect(cb.combat!.win).toEqual(expect.objectContaining({ target: 'fin_victoire', msg: "Le gobelin s'effondre." }));
    expect(cb.combat!.win!.effects.map((e) => e.raw)).toEqual(['score 100', 'give tresor']);
    expect(cb.combat!.lose).toEqual(expect.objectContaining({ target: 'fin_lache', msg: '' }));
    expect(cb.combat!.flee).toEqual(expect.objectContaining({ target: 'depart' }));
    // les effets de @win ne doivent PAS fuiter vers les effets d'entree ou le choix
    expect(cb.entryEffects).toEqual([]);
    expect(cb.choices[0].effects).toEqual([]);
  });

  it('parses @ask/@answer/@correct/@wrong', () => {
    const coffre = doc.sections.find((s) => s.name === 'coffre')!;
    expect(coffre.ask).toEqual(
      expect.objectContaining({ prompt: 'Le mot a composer ?', maxlen: 20 }),
    );
    expect(coffre.ask!.answers.map((a) => a.text)).toEqual(['XIPE TOTEC', 'ECORCHE']);
    expect(coffre.ask!.correct).toEqual(expect.objectContaining({ target: 'fin_victoire' }));
    expect(coffre.ask!.correct!.effects.map((e) => e.raw)).toEqual(['score 30']);
    expect(coffre.ask!.wrong).toEqual(expect.objectContaining({ target: 'fin_lache' }));
  });

  it('serialize() round-trips byte for byte when nothing was edited', () => {
    expect(p.serialize(doc)).toBe(FIXTURE);
  });
});

describe('AdvParser: mutating a choice line surgically', () => {
  let p: AdvParser;
  let doc: AdvDocument;

  beforeEach(() => {
    p = new AdvParser();
    doc = p.parse(FIXTURE);
  });

  it('retarget() changes only the target, keeping condition/label/comment intact', () => {
    const choice = doc.sections.find((s) => s.name === 'depart')!.choices[0];
    p.retarget(doc, choice, 'ailleurs');
    expect(choice.target).toBe('ailleurs');
    expect(doc.lines[choice.lineIndex]).toBe('* {has torche} [Entrer avec la torche] -> ailleurs');
  });

  it('relabel() changes only the label', () => {
    const choice = doc.sections.find((s) => s.name === 'depart')!.choices[0];
    p.relabel(doc, choice, 'Foncer dans le noir');
    expect(choice.label).toBe('Foncer dans le noir');
    expect(doc.lines[choice.lineIndex]).toBe('* {has torche} [Foncer dans le noir] -> noir');
  });

  it('setChoiceCond() adds braces to a choice that had none', () => {
    const choice = doc.sections.find((s) => s.name === 'depart')!.choices[1];
    expect(choice.cond).toBeNull();
    p.setChoiceCond(doc, choice, 'flag porte_ouverte');
    expect(choice.cond).toBe('flag porte_ouverte');
    expect(doc.lines[choice.lineIndex]).toBe('* {flag porte_ouverte} [Rebrousser chemin] -> fin_lache');
  });

  it('setChoiceCond() removes braces when set back to empty', () => {
    const choice = doc.sections.find((s) => s.name === 'depart')!.choices[0];
    p.setChoiceCond(doc, choice, '');
    expect(choice.cond).toBeNull();
    expect(doc.lines[choice.lineIndex]).toBe('* [Entrer avec la torche] -> noir');
  });
});

describe('AdvParser: effect mutators', () => {
  let p: AdvParser;
  let doc: AdvDocument;

  beforeEach(() => {
    p = new AdvParser();
    doc = p.parse(FIXTURE);
  });

  it('updateEffectLine() rewrites the line and preserves a trailing comment', () => {
    doc.lines[doc.sections.find((s) => s.name === 'depart')!.entryEffects[0].lineIndex] = '~ score 5   # bonus initial';
    doc = p.parse(p.serialize(doc));
    const ref = doc.sections.find((s) => s.name === 'depart')!.entryEffects[0];
    p.updateEffectLine(doc, ref, { ...newEffect('score'), value: 42 });
    expect(doc.lines[ref.lineIndex]).toBe('~ score 42  # bonus initial');
  });

  it('setEffectRaw() falls back to raw text without going through the grammar', () => {
    const ref = doc.sections.find((s) => s.name === 'depart')!.entryEffects[0];
    p.setEffectRaw(doc, ref, 'un truc pas encore reconnu');
    expect(doc.lines[ref.lineIndex]).toBe('~ un truc pas encore reconnu');
    expect(ref.raw).toBe('un truc pas encore reconnu');
  });

  it('addEntryEffect() appends after the last existing entry effect and reparses', () => {
    const sec = doc.sections.find((s) => s.name === 'depart')!;
    p.addEntryEffect(doc, sec, { ...newEffect('give'), name: 'clef' });
    const sec2 = doc.sections.find((s) => s.name === 'depart')!;
    expect(sec2.entryEffects.map((e) => e.raw)).toEqual(['score 5', 'set toujours_vrai', 'give clef']);
  });

  it('addChoiceEffect() appends to the right choice only', () => {
    const sec = doc.sections.find((s) => s.name === 'depart')!;
    const choice = sec.choices[1]; // celui qui n'a pas encore d'effet
    p.addChoiceEffect(doc, choice, { ...newEffect('score'), value: -1 });
    const sec2 = doc.sections.find((s) => s.name === 'depart')!;
    expect(sec2.choices[0].effects.map((e) => e.raw)).toEqual(['set vu_la_porte']);
    expect(sec2.choices[1].effects.map((e) => e.raw)).toEqual(['score -1']);
  });

  it('addOutcomeEffect() appends to a combat outcome', () => {
    const cb = doc.sections.find((s) => s.name === 'combat_gobelin')!.combat!;
    p.addOutcomeEffect(doc, cb.lose!, { ...newEffect('sound'), name: 'lose' });
    const cb2 = doc.sections.find((s) => s.name === 'combat_gobelin')!.combat!;
    expect(cb2.lose!.effects.map((e) => e.raw)).toEqual(['sound lose']);
  });

  it('removeEffectLine() removes exactly one effect and shifts nothing else incorrectly', () => {
    const sec = doc.sections.find((s) => s.name === 'depart')!;
    p.removeEffectLine(doc, sec.entryEffects[0]); // "score 5"
    const sec2 = doc.sections.find((s) => s.name === 'depart')!;
    expect(sec2.entryEffects.map((e) => e.raw)).toEqual(['set toujours_vrai']);
    expect(sec2.choices[0].effects.map((e) => e.raw)).toEqual(['set vu_la_porte']);
  });
});

describe('AdvParser: combat block add/remove', () => {
  let p: AdvParser;
  let doc: AdvDocument;

  beforeEach(() => {
    p = new AdvParser();
    doc = p.parse(FIXTURE);
  });

  it('addCombat() inserts @combat + @win + @lose with a placeholder target, and is a no-op if already present', () => {
    const sec = doc.sections.find((s) => s.name === 'noir')!;
    p.addCombat(doc, sec);
    const sec2 = doc.sections.find((s) => s.name === 'noir')!;
    expect(sec2.combat).toBeTruthy();
    expect(sec2.combat!.win!.target).toBe('A_COMPLETER');
    expect(sec2.combat!.lose!.target).toBe('A_COMPLETER');
    expect(sec2.combat!.flee).toBeNull();
    expect(sec2.choices.length).toBe(1); // le choix existant n'est pas touche

    const before = p.serialize(doc);
    p.addCombat(doc, sec2); // deja present : sans effet
    expect(p.serialize(doc)).toBe(before);
  });

  it('addCombatOutcome() adds only the missing outcome', () => {
    const cb = doc.sections.find((s) => s.name === 'combat_gobelin')!.combat!;
    expect(cb.flee).toBeTruthy(); // deja present dans la fixture
    p.addCombatOutcome(doc, cb, 'flee'); // no-op, deja la
    const cb2 = doc.sections.find((s) => s.name === 'combat_gobelin')!.combat!;
    expect(cb2.flee!.target).toBe('depart'); // inchange, pas ecrase par le placeholder
  });

  it('removeCombat() removes @combat and all its outcomes/effects, restoring plain narrative text', () => {
    const sec = doc.sections.find((s) => s.name === 'combat_gobelin')!;
    p.removeCombat(doc, sec);
    const sec2 = doc.sections.find((s) => s.name === 'combat_gobelin')!;
    expect(sec2.combat).toBeNull();
    expect(sec2.kind.hasCombat).toBe(false);
    expect(sec2.choices.length).toBe(1); // le choix "Affronter" reste
    expect(p.sectionText(doc, sec2)).toContain('Le gobelin bondit !');
    expect(p.sectionText(doc, sec2)).not.toContain('@combat');
  });

  it('removeCombatOutcome() removes only that outcome and its own effects', () => {
    const cb = doc.sections.find((s) => s.name === 'combat_gobelin')!.combat!;
    p.removeCombatOutcome(doc, cb, 'win');
    const cb2 = doc.sections.find((s) => s.name === 'combat_gobelin')!.combat!;
    expect(cb2.win).toBeNull();
    expect(cb2.lose).toBeTruthy();
    expect(cb2.flee).toBeTruthy();
  });

  it('updateCombat()/updateCombatOutcome() rewrite their line in place', () => {
    const cb = doc.sections.find((s) => s.name === 'combat_gobelin')!.combat!;
    p.updateCombat(doc, cb, { hp: 20, image: null });
    expect(doc.lines[cb.line]).toBe('@combat "Gobelin" att=6 hp=20 dmg=3 armor=0');
    p.updateCombatOutcome(doc, cb.win!, { target: 'ailleurs', msg: '' });
    expect(doc.lines[cb.win!.line]).toBe('@win ailleurs');
  });
});

describe('AdvParser: ask block add/remove', () => {
  let p: AdvParser;
  let doc: AdvDocument;

  beforeEach(() => {
    p = new AdvParser();
    doc = p.parse(FIXTURE);
  });

  it('addAsk() inserts @ask + @answer + @correct + @wrong, and is a no-op if already present', () => {
    const sec = doc.sections.find((s) => s.name === 'noir')!;
    p.addAsk(doc, sec);
    const sec2 = doc.sections.find((s) => s.name === 'noir')!;
    expect(sec2.ask).toBeTruthy();
    expect(sec2.ask!.answers.length).toBe(1);
    expect(sec2.ask!.correct!.target).toBe('A_COMPLETER');
    expect(sec2.ask!.wrong!.target).toBe('A_COMPLETER');

    const before = p.serialize(doc);
    p.addAsk(doc, sec2);
    expect(p.serialize(doc)).toBe(before);
  });

  it('removeAsk() removes @ask and everything under it', () => {
    const sec = doc.sections.find((s) => s.name === 'coffre')!;
    p.removeAsk(doc, sec);
    const sec2 = doc.sections.find((s) => s.name === 'coffre')!;
    expect(sec2.ask).toBeNull();
    expect(sec2.kind.hasAsk).toBe(false);
    expect(p.sectionText(doc, sec2)).toContain('Un coffre a cadran de lettres.');
  });

  it('addAnswer()/removeAnswer()/updateAnswer() manage the answer list', () => {
    const ask = doc.sections.find((s) => s.name === 'coffre')!.ask!;
    p.addAnswer(doc, ask, 'TROISIEME');
    let ask2 = doc.sections.find((s) => s.name === 'coffre')!.ask!;
    expect(ask2.answers.map((a) => a.text)).toEqual(['XIPE TOTEC', 'ECORCHE', 'TROISIEME']);

    p.updateAnswer(doc, ask2.answers[1], 'CHANGEE');
    ask2 = doc.sections.find((s) => s.name === 'coffre')!.ask!;
    expect(ask2.answers.map((a) => a.text)).toEqual(['XIPE TOTEC', 'CHANGEE', 'TROISIEME']);

    p.removeAnswer(doc, ask2.answers[0]);
    ask2 = doc.sections.find((s) => s.name === 'coffre')!.ask!;
    expect(ask2.answers.map((a) => a.text)).toEqual(['CHANGEE', 'TROISIEME']);
  });

  it('updateAsk()/updateAskOutcome() rewrite their line in place', () => {
    const ask = doc.sections.find((s) => s.name === 'coffre')!.ask!;
    p.updateAsk(doc, ask, { prompt: 'Nouvelle invite ?', maxlen: 10 });
    expect(doc.lines[ask.line]).toBe('@ask "Nouvelle invite ?" maxlen=10');
    p.updateAskOutcome(doc, ask.wrong!, 'ailleurs');
    expect(doc.lines[ask.wrong!.line]).toBe('@wrong ailleurs');
  });
});

describe('AdvParser: section text and structure', () => {
  let p: AdvParser;
  let doc: AdvDocument;

  beforeEach(() => {
    p = new AdvParser();
    doc = p.parse(FIXTURE);
  });

  it('setSectionText() re-parses without corrupting later sections when the line count changes', () => {
    const sec = doc.sections.find((s) => s.name === 'depart')!;
    const originalNoirStart = doc.sections.find((s) => s.name === 'noir')!.startLine;
    p.setSectionText(doc, sec, `${p.sectionText(doc, sec)}\nUne ligne de plus.\nEt une autre.`);
    const noir = doc.sections.find((s) => s.name === 'noir')!;
    expect(noir.startLine).toBe(originalNoirStart + 2);
    expect(noir.choices[0].target).toBe('fin_victoire'); // toujours coherent
  });

  it('countReferences() counts choices targeting a section, across all sections', () => {
    expect(p.countReferences(doc, 'fin_lache')).toBe(1);
    expect(p.countReferences(doc, 'combat_gobelin')).toBe(1); // le choix "Affronter" se cible lui-meme
    expect(p.countReferences(doc, 'inexistante')).toBe(0);
  });

  it('addSection()/removeSection() add and remove a section from its chapter', () => {
    const before = doc.sections.length;
    let next = p.addSection(doc, 1);
    expect(next.sections.length).toBe(before + 1);
    const added = next.sections.find((s) => s.name === 'nouvelle_section')!;
    expect(added).toBeTruthy();

    next = p.removeSection(next, added);
    expect(next.sections.length).toBe(before);
    expect(next.sections.some((s) => s.name === 'nouvelle_section')).toBe(false);
  });

  it('addChapter()/mergeChapterWithPrevious() manage chapter boundaries without touching sections', () => {
    let next = p.addChapter(doc, 'Chapitre 2');
    expect(next.chapters.length).toBe(3);
    const totalSectionsAfterAdd = next.sections.length;

    next = p.mergeChapterWithPrevious(next, 2);
    expect(next.chapters.length).toBe(2);
    expect(next.sections.length).toBe(totalSectionsAfterAdd); // aucune section perdue
  });
});

describe('AdvParser: preamble mutators', () => {
  let p: AdvParser;
  let doc: AdvDocument;

  beforeEach(() => {
    p = new AdvParser();
    doc = p.parse(FIXTURE);
  });

  it('updateMeta() rewrites title/author/start', () => {
    const next = p.updateMeta(doc, { title: 'Nouveau titre' });
    expect(next.title).toBe('Nouveau titre');
    expect(next.author).toBe('Moi'); // inchange
  });

  it('addStat()/updateStat()/removeStat() round-trip', () => {
    let next = p.addStat(doc);
    expect(next.stats.length).toBe(2);
    const stat = next.stats[1];
    next = p.updateStat(next, stat, { name: 'ENDURANCE', init: 10, lo: 0, hi: 20 });
    expect(next.stats[1]).toEqual(expect.objectContaining({ name: 'ENDURANCE', init: 10, lo: 0, hi: 20 }));
    next = p.removeStat(next, next.stats[1]);
    expect(next.stats.length).toBe(1);
  });

  it('addFlag()/updateFlag()/removeFlag() round-trip, local implies off', () => {
    let next = p.addFlag(doc);
    const flag = next.flags[next.flags.length - 1];
    next = p.updateFlag(next, flag, { local: true, on: true });
    const updated = next.flags[next.flags.length - 1];
    expect(updated.local).toBe(true);
    expect(updated.on).toBe(false); // un flag local ne demarre jamais a on
    next = p.removeFlag(next, updated);
    expect(next.flags.length).toBe(1);
  });

  it('addItem()/updateItem()/removeItem() round-trip', () => {
    let next = p.addItem(doc);
    const item = next.items[next.items.length - 1];
    next = p.updateItem(next, item, { name: 'dague', label: 'Dague', atk: 1, dmg: 3, armor: 0 });
    const updated = next.items[next.items.length - 1];
    expect(updated).toEqual(expect.objectContaining({ name: 'dague', label: 'Dague', atk: 1, dmg: 3 }));
    next = p.removeItem(next, updated);
    expect(next.items.length).toBe(1);
  });
});
