import { Component, computed, input, output } from '@angular/core';
import { MatIconModule } from '@angular/material/icon';
import { MatButtonModule } from '@angular/material/button';
import { type ValidationIssue } from '../../core/adv-validate';

/**
 * Liste des problèmes détectés par `validateDocument` (adv-validate.ts),
 * erreurs d'abord. Lecture seule — cliquer une ligne rattachée à une section
 * y bascule (même geste que le Graphe, cf. `onGraphSelect` dans app.ts).
 */
@Component({
  selector: 'app-validation-panel',
  imports: [MatIconModule, MatButtonModule],
  templateUrl: './validation-panel.html',
  styleUrl: './validation-panel.scss',
})
export class ValidationPanel {
  readonly issues = input<ValidationIssue[]>([]);
  readonly select = output<string>();

  readonly errors = computed(() => this.issues().filter((i) => i.severity === 'error'));
  readonly warnings = computed(() => this.issues().filter((i) => i.severity === 'warning'));
}
