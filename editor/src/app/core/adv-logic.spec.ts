import {
  atomText,
  conditionText,
  effectBodyText,
  effectLine,
  emptyCondition,
  isConditionParseable,
  isEffectParseable,
  newAtom,
  newEffect,
  parseCondition,
  parseEffect,
  type AdvCondition,
} from './adv-logic';

describe('adv-logic: conditions', () => {
  it('parses a single atom of each kind', () => {
    expect(parseCondition('flag lead_moretti')).toEqual({
      atoms: [{ kind: 'flag', name: 'lead_moretti' }],
      connective: 'and',
    });
    expect(parseCondition('not flag deja_vu')).toEqual({
      atoms: [{ kind: 'not_flag', name: 'deja_vu' }],
      connective: 'and',
    });
    expect(parseCondition('has torche')).toEqual({ atoms: [{ kind: 'has', name: 'torche' }], connective: 'and' });
    expect(parseCondition('not has torche')).toEqual({ atoms: [{ kind: 'not_has', name: 'torche' }], connective: 'and' });
    expect(parseCondition('stat ADRESSE >= 9')).toEqual({
      atoms: [{ kind: 'stat', name: 'ADRESSE', cmp: '>=', value: 9 }],
      connective: 'and',
    });
  });

  it('accepts every comparison operator', () => {
    for (const op of ['==', '!=', '<', '<=', '>', '>=']) {
      const c = parseCondition(`stat X ${op} 3`);
      expect(c.atoms[0]).toEqual({ kind: 'stat', name: 'X', cmp: op as never, value: 3 });
    }
  });

  it('combines atoms with and / or', () => {
    expect(parseCondition('stat ADRESSE >= 9 and has machette')).toEqual({
      atoms: [
        { kind: 'stat', name: 'ADRESSE', cmp: '>=', value: 9 },
        { kind: 'has', name: 'machette' },
      ],
      connective: 'and',
    });
    expect(parseCondition('flag lead_moretti or flag lead_conservateur').connective).toBe('or');
  });

  it('rejects mixing and/or in the same condition (v0, no parentheses)', () => {
    expect(() => parseCondition('flag a and flag b or flag c')).toThrow();
    expect(isConditionParseable('flag a and flag b or flag c')).toBe(false);
  });

  it('empty string is always-true, no atoms', () => {
    const c = parseCondition('');
    expect(c.atoms).toEqual([]);
  });

  it('rejects incomplete or malformed atoms', () => {
    expect(() => parseCondition('flag')).toThrow();
    expect(() => parseCondition('stat NOM >=')).toThrow();
    expect(() => parseCondition('stat NOM ~= 3')).toThrow();
    expect(() => parseCondition('not stat NOM >= 3')).toThrow();
    expect(() => parseCondition('flag a maybe flag b')).toThrow();
  });

  it('round-trips atom -> text -> atom for every kind', () => {
    const atoms: AdvCondition['atoms'] = [
      { kind: 'flag', name: 'x' },
      { kind: 'not_flag', name: 'x' },
      { kind: 'has', name: 'x' },
      { kind: 'not_has', name: 'x' },
      { kind: 'stat', name: 'X', cmp: '>=', value: 5 },
    ];
    for (const a of atoms) {
      const text = atomText(a);
      const reparsed = parseCondition(text).atoms[0];
      expect(reparsed).toEqual(a);
    }
  });

  it('conditionText joins atoms with the connective', () => {
    const c: AdvCondition = {
      atoms: [
        { kind: 'has', name: 'torche' },
        { kind: 'not_flag', name: 'peur' },
      ],
      connective: 'or',
    };
    expect(conditionText(c)).toBe('has torche or not flag peur');
  });

  it('emptyCondition/newAtom produce sensible defaults', () => {
    expect(emptyCondition()).toEqual({ atoms: [], connective: 'and' });
    expect(newAtom('flag')).toEqual({ kind: 'flag', name: '' });
    expect(newAtom('stat')).toEqual({ kind: 'stat', name: '', cmp: '==', value: 0 });
  });
});

describe('adv-logic: effects', () => {
  it('parses every verb shape from the guide', () => {
    expect(parseEffect('set FLAG')).toEqual({ op: 'set', name: 'FLAG', cond: emptyCondition() });
    expect(parseEffect('clear FLAG')).toEqual({ op: 'clear', name: 'FLAG', cond: emptyCondition() });
    expect(parseEffect('toggle FLAG')).toEqual({ op: 'toggle', name: 'FLAG', cond: emptyCondition() });
    expect(parseEffect('give OBJET')).toEqual({ op: 'give', name: 'OBJET', cond: emptyCondition() });
    expect(parseEffect('take OBJET')).toEqual({ op: 'take', name: 'OBJET', cond: emptyCondition() });
    expect(parseEffect('add STAT 3')).toEqual({ op: 'add', name: 'STAT', value: 3, cond: emptyCondition() });
    expect(parseEffect('sub STAT 3')).toEqual({ op: 'sub', name: 'STAT', value: 3, cond: emptyCondition() });
    expect(parseEffect('set STAT 12')).toEqual({ op: 'setstat', name: 'STAT', value: 12, cond: emptyCondition() });
    expect(parseEffect('setmax STAT 12')).toEqual({ op: 'setmax', name: 'STAT', value: 12, cond: emptyCondition() });
    expect(parseEffect('restore STAT')).toEqual({ op: 'restore', name: 'STAT', cond: emptyCondition() });
    expect(parseEffect('score -5')).toEqual({ op: 'score', name: '', value: -5, cond: emptyCondition() });
    expect(parseEffect('sound magic')).toEqual({ op: 'sound', name: 'magic', cond: emptyCondition() });
    expect(parseEffect('goto ailleurs')).toEqual({ op: 'goto', name: 'ailleurs', cond: emptyCondition() });
  });

  it('disambiguates "set" by argument count', () => {
    expect(parseEffect('set FLAG').op).toBe('set');
    expect(parseEffect('set STAT 1').op).toBe('setstat');
    expect(() => parseEffect('set STAT 1 2')).toThrow();
  });

  it('parses an optional guard condition', () => {
    const e = parseEffect('{not flag deja_vu} score 10');
    expect(e.op).toBe('score');
    expect(e.value).toBe(10);
    expect(e.cond.atoms).toEqual([{ kind: 'not_flag', name: 'deja_vu' }]);
  });

  it('rejects malformed / unknown effects', () => {
    expect(() => parseEffect('')).toThrow();
    expect(() => parseEffect('nawak FLAG')).toThrow();
    expect(() => parseEffect('add STAT')).toThrow();
    expect(() => parseEffect('score')).toThrow();
    expect(() => parseEffect('score 1 2')).toThrow();
    expect(() => parseEffect('{cond unclosed score 1')).toThrow();
    expect(isEffectParseable('nawak FLAG')).toBe(false);
  });

  it('round-trips every verb through effectLine -> parseEffect', () => {
    const effects = [
      { ...newEffect('set'), name: 'FLAG' },
      { ...newEffect('clear'), name: 'FLAG' },
      { ...newEffect('toggle'), name: 'FLAG' },
      { ...newEffect('give'), name: 'OBJET' },
      { ...newEffect('take'), name: 'OBJET' },
      { ...newEffect('add'), name: 'STAT', value: 2 },
      { ...newEffect('sub'), name: 'STAT', value: 2 },
      { ...newEffect('setstat'), name: 'STAT', value: 12 },
      { ...newEffect('setmax'), name: 'STAT', value: 12 },
      { ...newEffect('restore'), name: 'STAT' },
      { ...newEffect('score'), value: -3 },
      { ...newEffect('sound'), name: 'magic' },
      { ...newEffect('goto'), name: 'ailleurs' },
    ];
    for (const e of effects) {
      const line = effectLine(e);
      expect(line.startsWith('~ ')).toBe(true);
      const reparsed = parseEffect(line.slice(2));
      expect(reparsed).toEqual(e);
    }
  });

  it('effectLine prefixes the guard condition in braces when present', () => {
    const e = { ...newEffect('score'), value: 10, cond: parseCondition('not flag deja_vu') };
    expect(effectLine(e)).toBe('~ {not flag deja_vu} score 10');
    expect(effectBodyText(e)).toBe('score 10');
  });

  it('effectLine has no braces when the guard is empty', () => {
    const e = newEffect('set');
    e.name = 'FLAG';
    expect(effectLine(e)).toBe('~ set FLAG');
  });

  it('newEffect() is always immediately parseable for every op, even before the author fills a name in', () => {
    const ops: Parameters<typeof newEffect>[0][] = [
      'set',
      'clear',
      'toggle',
      'give',
      'take',
      'add',
      'sub',
      'setstat',
      'setmax',
      'restore',
      'goto',
      'sound',
      'score',
    ];
    for (const op of ops) {
      const line = effectLine(newEffect(op));
      expect([op, line, isEffectParseable(line.slice(2))]).toEqual([op, line, true]);
    }
  });
});
