import { TestBed } from '@angular/core/testing';
import { EffectEditor } from './effect-editor';
import { emptyCondition, newEffect, type AdvEffect } from '../../core/adv-logic';

describe('EffectEditor', () => {
  async function setup(effect: AdvEffect) {
    const fixture = TestBed.createComponent(EffectEditor);
    fixture.componentRef.setInput('effect', effect);
    fixture.componentRef.setInput('flags', ['porte_ouverte']);
    fixture.componentRef.setInput('items', ['torche']);
    fixture.componentRef.setInput('stats', ['ADRESSE']);
    fixture.componentRef.setInput('sections', ['depart']);
    await fixture.whenStable();
    return fixture;
  }

  it('shows a name field for verbs that need one, and a value field only when relevant', async () => {
    const fixture = await setup({ op: 'add', name: 'ADRESSE', value: 1, cond: emptyCondition() });
    const el = fixture.nativeElement as HTMLElement;
    expect(el.querySelectorAll('.f-name').length).toBe(1);
    expect(el.querySelectorAll('.f-value').length).toBe(1);
  });

  it('hides the name field for "score" (value only)', async () => {
    const fixture = await setup({ op: 'score', name: '', value: 5, cond: emptyCondition() });
    const el = fixture.nativeElement as HTMLElement;
    expect(el.querySelectorAll('.f-name').length).toBe(0);
    expect(el.querySelectorAll('.f-value').length).toBe(1);
  });

  it('shows the guard condition editor even when the guard is empty (always-true)', async () => {
    const fixture = await setup(newEffect('set'));
    const el = fixture.nativeElement as HTMLElement;
    expect(el.querySelector('app-condition-editor')).toBeTruthy();
  });

  it('setOp() resets name/value to fit the new verb\'s shape, keeps the guard', async () => {
    const cond = { atoms: [{ kind: 'has' as const, name: 'torche' }], connective: 'and' as const };
    const fixture = await setup({ op: 'set', name: 'FLAG', cond });
    let emitted: AdvEffect | undefined;
    fixture.componentInstance.effectChange.subscribe((e) => (emitted = e));
    fixture.componentInstance.setOp('score');
    expect(emitted).toEqual({ op: 'score', name: '', value: 0, cond });
  });

  it('setName()/setValue() patch only their own field', async () => {
    const fixture = await setup({ op: 'give', name: 'torche', cond: emptyCondition() });
    let emitted: AdvEffect | undefined;
    fixture.componentInstance.effectChange.subscribe((e) => (emitted = e));
    fixture.componentInstance.setName('clef');
    expect(emitted).toEqual({ op: 'give', name: 'clef', cond: emptyCondition() });

    const fixture2 = await setup({ op: 'score', name: '', value: 5, cond: emptyCondition() });
    fixture2.componentInstance.effectChange.subscribe((e) => (emitted = e));
    fixture2.componentInstance.setValue('-3');
    expect(emitted).toEqual({ op: 'score', name: '', value: -3, cond: emptyCondition() });
  });

  it('setGuard() replaces the guard condition without touching op/name/value', async () => {
    const fixture = await setup({ op: 'give', name: 'torche', cond: emptyCondition() });
    let emitted: AdvEffect | undefined;
    fixture.componentInstance.effectChange.subscribe((e) => (emitted = e));
    const newGuard = { atoms: [{ kind: 'flag' as const, name: 'x' }], connective: 'and' as const };
    fixture.componentInstance.setGuard(newGuard);
    expect(emitted).toEqual({ op: 'give', name: 'torche', cond: newGuard });
  });

  it('valueLabel() is specific to "score"', async () => {
    const fixture = await setup({ op: 'score', name: '', value: 0, cond: emptyCondition() });
    expect(fixture.componentInstance.valueLabel).toBe('points (+/-)');
    const fixture2 = await setup({ op: 'add', name: 'X', value: 0, cond: emptyCondition() });
    expect(fixture2.componentInstance.valueLabel).toBe('valeur');
  });
});
