import { TestBed } from '@angular/core/testing';
import { ConditionEditor } from './condition-editor';
import type { AdvCondition } from '../../core/adv-logic';

describe('ConditionEditor', () => {
  async function setup(condition: AdvCondition) {
    const fixture = TestBed.createComponent(ConditionEditor);
    fixture.componentRef.setInput('condition', condition);
    await fixture.whenStable();
    return fixture;
  }

  it('shows the "always true" empty state when there are no atoms', async () => {
    const fixture = await setup({ atoms: [], connective: 'and' });
    const el = fixture.nativeElement as HTMLElement;
    expect(el.querySelector('.empty')?.textContent).toContain('Toujours vraie');
    expect(el.querySelectorAll('app-bool-test-editor').length).toBe(0);
  });

  it('renders one app-bool-test-editor per atom', async () => {
    const fixture = await setup({
      atoms: [
        { kind: 'has', name: 'torche' },
        { kind: 'flag', name: 'x' },
      ],
      connective: 'and',
    });
    const el = fixture.nativeElement as HTMLElement;
    expect(el.querySelectorAll('app-bool-test-editor').length).toBe(2);
  });

  it('hides the ET/OU toggle with fewer than two atoms', async () => {
    const fixture = await setup({ atoms: [{ kind: 'has', name: 'torche' }], connective: 'and' });
    const el = fixture.nativeElement as HTMLElement;
    expect(el.querySelector('.connective')).toBeNull();
  });

  it('addAtom() appends a new flag atom, keeping existing ones and the connective', async () => {
    const fixture = await setup({ atoms: [{ kind: 'has', name: 'torche' }], connective: 'or' });
    let emitted: AdvCondition | undefined;
    fixture.componentInstance.conditionChange.subscribe((c) => (emitted = c));
    fixture.componentInstance.addAtom();
    expect(emitted).toEqual({ atoms: [{ kind: 'has', name: 'torche' }, { kind: 'flag', name: '' }], connective: 'or' });
  });

  it('updateAtom() replaces only the targeted index', async () => {
    const fixture = await setup({
      atoms: [
        { kind: 'has', name: 'torche' },
        { kind: 'flag', name: 'x' },
      ],
      connective: 'and',
    });
    let emitted: AdvCondition | undefined;
    fixture.componentInstance.conditionChange.subscribe((c) => (emitted = c));
    fixture.componentInstance.updateAtom(1, { kind: 'not_flag', name: 'x' });
    expect(emitted!.atoms).toEqual([{ kind: 'has', name: 'torche' }, { kind: 'not_flag', name: 'x' }]);
  });

  it('removeAtom() removes only the targeted index', async () => {
    const fixture = await setup({
      atoms: [
        { kind: 'has', name: 'torche' },
        { kind: 'flag', name: 'x' },
      ],
      connective: 'and',
    });
    let emitted: AdvCondition | undefined;
    fixture.componentInstance.conditionChange.subscribe((c) => (emitted = c));
    fixture.componentInstance.removeAtom(0);
    expect(emitted!.atoms).toEqual([{ kind: 'flag', name: 'x' }]);
  });

  it('setConnective() switches and/or without touching the atoms', async () => {
    const atoms: AdvCondition['atoms'] = [{ kind: 'has', name: 'torche' }, { kind: 'flag', name: 'x' }];
    const fixture = await setup({ atoms, connective: 'and' });
    let emitted: AdvCondition | undefined;
    fixture.componentInstance.conditionChange.subscribe((c) => (emitted = c));
    fixture.componentInstance.setConnective('or');
    expect(emitted).toEqual({ atoms, connective: 'or' });
  });
});
