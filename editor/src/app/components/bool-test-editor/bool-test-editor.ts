import { Component, input, output } from '@angular/core';
import { MatSelectModule } from '@angular/material/select';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatInputModule } from '@angular/material/input';
import { MatIconModule } from '@angular/material/icon';
import { MatButtonModule } from '@angular/material/button';
import { MatTooltipModule } from '@angular/material/tooltip';
import { CMP_OPS, type Cmp, type AtomKind, type ConditionAtom } from '../../core/adv-logic';

let nextId = 0;

/**
 * Un test booléen unique : `flag NOM`, `not has OBJET`, `stat NOM OP N`...
 * Composant "contrôlé" (cf. CodeEditor) : la valeur vient de `atom()`, toute
 * modification émet un NOUVEL objet via `atomChange` — jamais de mutation en
 * place, c'est au parent (ConditionEditor) de décider comment le réintégrer.
 */
@Component({
  selector: 'app-bool-test-editor',
  imports: [MatSelectModule, MatFormFieldModule, MatInputModule, MatIconModule, MatButtonModule, MatTooltipModule],
  templateUrl: './bool-test-editor.html',
  styleUrl: './bool-test-editor.scss',
})
export class BoolTestEditor {
  readonly atom = input.required<ConditionAtom>();
  /** Noms connus, pour suggestion (`<datalist>`) — pas une contrainte : rien
   * n'empêche de taper un nom qui n'existe pas encore ailleurs. */
  readonly flags = input<readonly string[]>([]);
  readonly items = input<readonly string[]>([]);
  readonly stats = input<readonly string[]>([]);
  readonly removable = input(true);

  readonly atomChange = output<ConditionAtom>();
  readonly remove = output<void>();

  readonly listId = `bool-test-names-${nextId++}`;
  readonly cmpOps = CMP_OPS;
  readonly kinds: { value: AtomKind; label: string }[] = [
    { value: 'flag', label: 'drapeau actif' },
    { value: 'not_flag', label: 'drapeau inactif' },
    { value: 'has', label: 'objet possédé' },
    { value: 'not_has', label: 'objet absent' },
    { value: 'stat', label: 'caractéristique' },
  ];

  get nameOptions(): readonly string[] {
    const kind = this.atom().kind;
    if (kind === 'flag' || kind === 'not_flag') return this.flags();
    if (kind === 'has' || kind === 'not_has') return this.items();
    return this.stats();
  }

  get namePlaceholder(): string {
    const kind = this.atom().kind;
    if (kind === 'flag' || kind === 'not_flag') return 'drapeau';
    if (kind === 'has' || kind === 'not_has') return 'objet';
    return 'caractéristique';
  }

  setKind(kind: AtomKind): void {
    const a = this.atom();
    // Le nom reste (un drapeau qui devient "inactif" garde le meme nom) ;
    // cmp/value n'ont de sens que pour 'stat', donc réinitialisés ailleurs.
    this.atomChange.emit(kind === 'stat' ? { kind, name: a.name, cmp: a.cmp ?? '==', value: a.value ?? 0 } : { kind, name: a.name });
  }

  setName(name: string): void {
    this.atomChange.emit({ ...this.atom(), name });
  }

  setCmp(cmp: Cmp): void {
    this.atomChange.emit({ ...this.atom(), cmp });
  }

  setValue(raw: string): void {
    const n = Number(raw);
    this.atomChange.emit({ ...this.atom(), value: Number.isFinite(n) ? n : 0 });
  }
}
