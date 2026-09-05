import { Component, inject, input, output } from '@angular/core';
import { MatListModule } from '@angular/material/list';
import { MatIconModule } from '@angular/material/icon';
import { MatButtonModule } from '@angular/material/button';
import { MatTooltipModule } from '@angular/material/tooltip';
import { MatExpansionModule } from '@angular/material/expansion';
import { MatDialog } from '@angular/material/dialog';
import { AdvParser, type AdvDocument, type AdvSection } from '../../core/adv-parser';
import { ConfirmDialog } from '../confirm-dialog/confirm-dialog';
import { PromptDialog } from '../prompt-dialog/prompt-dialog';

/** Icône + libellé court pour le genre de section — priorité du plus
 * spécifique au plus générique (une section peut cumuler plusieurs signaux,
 * un seul pictogramme est affiché). */
function iconFor(s: AdvSection): { icon: string; label: string; cls: string } {
  if (s.kind.isStart) return { icon: 'play_circle', label: 'Départ', cls: 'k-start' };
  if (s.kind.ending === 'victoire') return { icon: 'emoji_events', label: 'Fin — victoire', cls: 'k-victoire' };
  if (s.kind.ending === 'defaite') return { icon: 'skull', label: 'Fin — défaite', cls: 'k-defaite' };
  if (s.kind.hasCombat) return { icon: 'swords', label: 'Combat', cls: 'k-combat' };
  if (s.kind.hasAsk) return { icon: 'quiz', label: 'Énigme (saisie)', cls: 'k-ask' };
  if (s.kind.hasImage) return { icon: 'image', label: 'Image', cls: 'k-image' };
  if (s.kind.isIntro) return { icon: 'star', label: 'Scène d’intro', cls: 'k-intro' };
  return { icon: 'article', label: 'Section', cls: 'k-normal' };
}

/**
 * Hiérarchie Aventure → Chapitre → Section, sur le modèle des `@chapter` du
 * source — un vrai arbre, sans cycle (contrairement au graphe des choix,
 * qui vit dans son propre onglet). C'est la navigation principale pendant
 * l'édition, et le point d'entrée pour ajouter/retirer chapitres et sections.
 */
@Component({
  selector: 'app-adventure-tree',
  imports: [MatListModule, MatIconModule, MatButtonModule, MatTooltipModule, MatExpansionModule],
  templateUrl: './adventure-tree.html',
  styleUrl: './adventure-tree.scss',
})
export class AdventureTree {
  private readonly parser = inject(AdvParser);
  private readonly dialog = inject(MatDialog);

  readonly doc = input.required<AdvDocument>();
  readonly selected = input<string | null>(null);
  readonly selectedIsPreamble = input(false);
  readonly select = output<string>();
  readonly selectPreamble = output<void>();
  readonly docChanged = output<AdvDocument>();

  readonly iconFor = iconFor;

  chapterLabel(index: number, title: string): string {
    return title || (index === 0 ? 'Prologue' : `Chapitre ${index}`);
  }

  addSection(chapterIndex: number, event: Event): void {
    event.stopPropagation();
    this.docChanged.emit(this.parser.addSection(this.doc(), chapterIndex));
  }

  removeSection(section: AdvSection, event: Event): void {
    event.stopPropagation();
    const refs = this.parser.countReferences(this.doc(), section.name);
    this.dialog
      .open(ConfirmDialog, {
        data: {
          title: 'Supprimer cette section ?',
          message: `« ${section.name} » et tout son texte seront retirés du document.`,
          warning: refs > 0 ? `${refs} choix pointe(nt) encore vers cette section ailleurs.` : undefined,
          confirmLabel: 'Supprimer',
          danger: true,
        },
      })
      .afterClosed()
      .subscribe((ok) => {
        if (ok) this.docChanged.emit(this.parser.removeSection(this.doc(), section));
      });
  }

  addChapter(): void {
    this.dialog
      .open(PromptDialog, { data: { title: 'Nouveau chapitre', label: 'Titre du chapitre' } })
      .afterClosed()
      .subscribe((title: string | null) => {
        if (title !== null) this.docChanged.emit(this.parser.addChapter(this.doc(), title));
      });
  }

  mergeChapter(chapterIndex: number, event: Event): void {
    event.stopPropagation();
    const title = this.chapterLabel(chapterIndex, this.doc().chapters[chapterIndex].title);
    this.dialog
      .open(ConfirmDialog, {
        data: {
          title: 'Fusionner ce chapitre ?',
          message: `« ${title} » sera fusionné avec le chapitre précédent. Ses sections sont conservées.`,
          confirmLabel: 'Fusionner',
        },
      })
      .afterClosed()
      .subscribe((ok) => {
        if (ok) this.docChanged.emit(this.parser.mergeChapterWithPrevious(this.doc(), chapterIndex));
      });
  }
}
