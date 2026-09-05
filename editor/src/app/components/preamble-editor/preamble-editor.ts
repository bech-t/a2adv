import { Component, inject, input, output } from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatInputModule } from '@angular/material/input';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatCheckboxModule } from '@angular/material/checkbox';
import { MatTooltipModule } from '@angular/material/tooltip';
import { MatCardModule } from '@angular/material/card';
import { AdvParser, type AdvDocument, type FlagDecl, type ItemDecl, type StatDecl } from '../../core/adv-parser';

/**
 * Le "global" de l'aventure : titre/auteur/départ, caractéristiques,
 * drapeaux, objets. Contrairement à l'éditeur de section (texte brut), ici
 * chaque ligne a une forme connue — on édite des champs, pas du texte.
 */
@Component({
  selector: 'app-preamble-editor',
  imports: [
    MatButtonModule,
    MatIconModule,
    MatInputModule,
    MatFormFieldModule,
    MatCheckboxModule,
    MatTooltipModule,
    MatCardModule,
  ],
  templateUrl: './preamble-editor.html',
  styleUrl: './preamble-editor.scss',
})
export class PreambleEditor {
  private readonly parser = inject(AdvParser);

  readonly doc = input.required<AdvDocument>();
  readonly changed = output<AdvDocument>();

  onMeta(field: 'title' | 'author' | 'start', value: string): void {
    this.changed.emit(this.parser.updateMeta(this.doc(), { [field]: value }));
  }

  addStat(): void {
    this.changed.emit(this.parser.addStat(this.doc()));
  }
  updateStat(stat: StatDecl, patch: Partial<Omit<StatDecl, 'line'>>): void {
    this.changed.emit(this.parser.updateStat(this.doc(), stat, patch));
  }
  removeStat(stat: StatDecl): void {
    this.changed.emit(this.parser.removeStat(this.doc(), stat));
  }

  addFlag(): void {
    this.changed.emit(this.parser.addFlag(this.doc()));
  }
  updateFlag(flag: FlagDecl, patch: Partial<Omit<FlagDecl, 'line'>>): void {
    this.changed.emit(this.parser.updateFlag(this.doc(), flag, patch));
  }
  removeFlag(flag: FlagDecl): void {
    this.changed.emit(this.parser.removeFlag(this.doc(), flag));
  }

  addItem(): void {
    this.changed.emit(this.parser.addItem(this.doc()));
  }
  updateItem(item: ItemDecl, patch: Partial<Omit<ItemDecl, 'line'>>): void {
    this.changed.emit(this.parser.updateItem(this.doc(), item, patch));
  }
  removeItem(item: ItemDecl): void {
    this.changed.emit(this.parser.removeItem(this.doc(), item));
  }

  toNumber(value: string): number {
    const n = Number(value);
    return Number.isFinite(n) ? n : 0;
  }
}
