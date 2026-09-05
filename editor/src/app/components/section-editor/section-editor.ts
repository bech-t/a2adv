import { Component, computed, effect, inject, input, output, signal } from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatChipsModule } from '@angular/material/chips';
import { MatCardModule } from '@angular/material/card';
import { AdvParser, type AdvDocument } from '../../core/adv-parser';
import { CodeEditor } from '../code-editor/code-editor';

/** Édite le texte brut d'UNE section. Volontairement sans notion de style,
 * effet, condition ou directive — c'est du texte, l'auteur sait ce qu'il
 * écrit ; ce composant se contente de le réinjecter au bon endroit. */
@Component({
  selector: 'app-section-editor',
  imports: [MatButtonModule, MatIconModule, MatChipsModule, MatCardModule, CodeEditor],
  templateUrl: './section-editor.html',
  styleUrl: './section-editor.scss',
})
export class SectionEditor {
  private readonly parser = inject(AdvParser);

  readonly doc = input<AdvDocument | null>(null);
  readonly sectionName = input<string | null>(null);
  readonly saved = output<void>();
  readonly jumpTo = output<string>();

  readonly text = signal('');
  readonly dirty = signal(false);

  readonly section = computed(() => {
    const doc = this.doc();
    const name = this.sectionName();
    return doc && name ? (doc.sections.find((s) => s.name === name) ?? null) : null;
  });

  readonly choices = computed(() => this.section()?.choices ?? []);

  constructor() {
    effect(() => {
      const doc = this.doc();
      const sec = this.section();
      this.text.set(doc && sec ? this.parser.sectionText(doc, sec) : '');
      this.dirty.set(false);
    });
  }

  onEdit(value: string): void {
    this.text.set(value);
    this.dirty.set(true);
  }

  save(): void {
    const doc = this.doc();
    const sec = this.section();
    if (!doc || !sec) return;
    this.parser.setSectionText(doc, sec, this.text());
    this.dirty.set(false);
    this.saved.emit();
  }
}
