import { Component, input, output } from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatButtonToggleModule } from '@angular/material/button-toggle';
import { MatTooltipModule } from '@angular/material/tooltip';
import { BoolTestEditor } from '../bool-test-editor/bool-test-editor';
import { type AdvCondition, type Connective, type ConditionAtom, newAtom } from '../../core/adv-logic';

/**
 * Une condition entière : une liste de tests booléens combinés par UN SEUL
 * connecteur ('and' OU 'or', jamais les deux — c'est une règle du langage,
 * pas une limite de cet éditeur, cf. docs/GUIDE-FORMAT-ADV.md §6). Le
 * sélecteur ET/OU est donc désactivé tant qu'il n'y a pas au moins deux
 * tests : avec un seul, le connecteur ne s'écrit nulle part.
 */
@Component({
  selector: 'app-condition-editor',
  imports: [MatButtonModule, MatIconModule, MatButtonToggleModule, MatTooltipModule, BoolTestEditor],
  templateUrl: './condition-editor.html',
  styleUrl: './condition-editor.scss',
})
export class ConditionEditor {
  readonly condition = input.required<AdvCondition>();
  readonly flags = input<readonly string[]>([]);
  readonly items = input<readonly string[]>([]);
  readonly stats = input<readonly string[]>([]);

  readonly conditionChange = output<AdvCondition>();

  addAtom(): void {
    const c = this.condition();
    this.conditionChange.emit({ ...c, atoms: [...c.atoms, newAtom('flag')] });
  }

  updateAtom(index: number, atom: ConditionAtom): void {
    const c = this.condition();
    const atoms = c.atoms.slice();
    atoms[index] = atom;
    this.conditionChange.emit({ ...c, atoms });
  }

  removeAtom(index: number): void {
    const c = this.condition();
    this.conditionChange.emit({ ...c, atoms: c.atoms.filter((_, i) => i !== index) });
  }

  setConnective(connective: Connective): void {
    this.conditionChange.emit({ ...this.condition(), connective });
  }
}
