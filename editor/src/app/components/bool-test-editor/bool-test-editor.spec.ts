import { TestBed } from '@angular/core/testing';
import { BoolTestEditor } from './bool-test-editor';
import type { ConditionAtom } from '../../core/adv-logic';

describe('BoolTestEditor', () => {
  async function setup(atom: ConditionAtom) {
    const fixture = TestBed.createComponent(BoolTestEditor);
    fixture.componentRef.setInput('atom', atom);
    fixture.componentRef.setInput('flags', ['porte_ouverte']);
    fixture.componentRef.setInput('items', ['torche']);
    fixture.componentRef.setInput('stats', ['ADRESSE']);
    await fixture.whenStable();
    return fixture;
  }

  it('renders the kind, name and (for stat) cmp/value fields', async () => {
    const fixture = await setup({ kind: 'stat', name: 'ADRESSE', cmp: '>=', value: 9 });
    const el = fixture.nativeElement as HTMLElement;
    expect(el.querySelectorAll('.f-cmp').length).toBe(1);
    expect(el.querySelectorAll('.f-value').length).toBe(1);
  });

  it('hides cmp/value fields for non-stat kinds', async () => {
    const fixture = await setup({ kind: 'flag', name: 'porte_ouverte' });
    const el = fixture.nativeElement as HTMLElement;
    expect(el.querySelectorAll('.f-cmp').length).toBe(0);
    expect(el.querySelectorAll('.f-value').length).toBe(0);
  });

  it('setKind() to stat adds cmp/value with sensible defaults, keeping the name', async () => {
    const fixture = await setup({ kind: 'flag', name: 'porte_ouverte' });
    let emitted: ConditionAtom | undefined;
    fixture.componentInstance.atomChange.subscribe((a) => (emitted = a));
    fixture.componentInstance.setKind('stat');
    expect(emitted).toEqual({ kind: 'stat', name: 'porte_ouverte', cmp: '==', value: 0 });
  });

  it('setKind() away from stat drops cmp/value but keeps the name', async () => {
    const fixture = await setup({ kind: 'stat', name: 'ADRESSE', cmp: '>=', value: 9 });
    let emitted: ConditionAtom | undefined;
    fixture.componentInstance.atomChange.subscribe((a) => (emitted = a));
    fixture.componentInstance.setKind('has');
    expect(emitted).toEqual({ kind: 'has', name: 'ADRESSE' });
  });

  it('setName()/setCmp()/setValue() patch only their own field', async () => {
    const fixture = await setup({ kind: 'stat', name: 'ADRESSE', cmp: '>=', value: 9 });
    let emitted: ConditionAtom | undefined;
    fixture.componentInstance.atomChange.subscribe((a) => (emitted = a));

    fixture.componentInstance.setName('AUTRE');
    expect(emitted).toEqual({ kind: 'stat', name: 'AUTRE', cmp: '>=', value: 9 });

    fixture.componentInstance.setCmp('<');
    expect(emitted).toEqual({ kind: 'stat', name: 'ADRESSE', cmp: '<', value: 9 });

    fixture.componentInstance.setValue('12');
    expect(emitted).toEqual({ kind: 'stat', name: 'ADRESSE', cmp: '>=', value: 12 });
  });

  it('setValue() falls back to 0 on non-numeric input', async () => {
    const fixture = await setup({ kind: 'stat', name: 'ADRESSE', cmp: '>=', value: 9 });
    let emitted: ConditionAtom | undefined;
    fixture.componentInstance.atomChange.subscribe((a) => (emitted = a));
    fixture.componentInstance.setValue('pas un nombre');
    expect(emitted!.value).toBe(0);
  });

  it('nameOptions()/namePlaceholder() pick the right list per kind', async () => {
    const fixture = await setup({ kind: 'has', name: 'torche' });
    expect(fixture.componentInstance.nameOptions).toEqual(['torche']);
    expect(fixture.componentInstance.namePlaceholder).toBe('objet');
  });
});
