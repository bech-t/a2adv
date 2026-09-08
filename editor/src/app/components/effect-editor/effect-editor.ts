import { Component, input, output } from '@angular/core';
import { MatSelectModule } from '@angular/material/select';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatInputModule } from '@angular/material/input';
import { MatIconModule } from '@angular/material/icon';
import { MatButtonModule } from '@angular/material/button';
import { ConditionEditor } from '../condition-editor/condition-editor';
import { EFFECT_SHAPE, SOUND_NAMES, type AdvCondition, type AdvEffect, type EffectOp } from '../../core/adv-logic';

let nextId = 0;

const VERB_LABELS: { op: EffectOp; label: string }[] = [
  { op: 'set', label: 'set — activer un drapeau' },
  { op: 'clear', label: 'clear — désactiver un drapeau' },
  { op: 'toggle', label: 'toggle — inverser un drapeau' },
  { op: 'give', label: 'give — donner un objet' },
  { op: 'take', label: 'take — retirer un objet' },
  { op: 'setstat', label: 'set — fixer une caractéristique' },
  { op: 'add', label: 'add — ajouter à une caractéristique' },
  { op: 'sub', label: 'sub — retrancher d’une caractéristique' },
  { op: 'setmax', label: 'setmax — changer le plafond' },
  { op: 'restore', label: 'restore — soin complet (= plafond)' },
  { op: 'score', label: 'score — points de score' },
  { op: 'sound', label: 'sound — effet sonore' },
  { op: 'goto', label: 'goto — sauter vers une section' },
];

/**
 * Un effet (`~ ...`) unique, garde `{condition}` optionnelle comprise.
 * Composant "contrôlé" (cf. CodeEditor/BoolTestEditor) : `effect()` en
 * entrée, `effectChange` en sortie, jamais de mutation en place.
 */
@Component({
  selector: 'app-effect-editor',
  imports: [
    MatSelectModule,
    MatFormFieldModule,
    MatInputModule,
    MatIconModule,
    MatButtonModule,
    ConditionEditor,
  ],
  templateUrl: './effect-editor.html',
  styleUrl: './effect-editor.scss',
})
export class EffectEditor {
  readonly effect = input.required<AdvEffect>();
  readonly flags = input<readonly string[]>([]);
  readonly items = input<readonly string[]>([]);
  readonly stats = input<readonly string[]>([]);
  readonly sections = input<readonly string[]>([]);
  readonly removable = input(true);

  readonly effectChange = output<AdvEffect>();
  readonly remove = output<void>();

  readonly listId = `effect-names-${nextId++}`;
  readonly verbs = VERB_LABELS;
  readonly soundNames = SOUND_NAMES;

  get shape() {
    return EFFECT_SHAPE[this.effect().op];
  }

  get nameOptions(): readonly string[] {
    switch (this.shape.name) {
      case 'flag':
        return this.flags();
      case 'item':
        return this.items();
      case 'stat':
        return this.stats();
      case 'section':
        return this.sections();
      default:
        return [];
    }
  }

  get valueLabel(): string {
    return this.effect().op === 'score' ? 'points (+/-)' : 'valeur';
  }

  setOp(op: EffectOp): void {
    const shape = EFFECT_SHAPE[op];
    this.effectChange.emit({
      op,
      name: shape.name === 'none' ? '' : this.effect().name,
      value: shape.value ? (this.effect().value ?? 0) : undefined,
      cond: this.effect().cond,
    });
  }

  setName(name: string): void {
    this.effectChange.emit({ ...this.effect(), name });
  }

  setValue(raw: string): void {
    const n = Number(raw);
    this.effectChange.emit({ ...this.effect(), value: Number.isFinite(n) ? n : 0 });
  }

  setGuard(cond: AdvCondition): void {
    this.effectChange.emit({ ...this.effect(), cond });
  }
}
