import { Component, computed, inject, signal } from '@angular/core';
import { MatToolbarModule } from '@angular/material/toolbar';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatTabsModule } from '@angular/material/tabs';
import { MatSidenavModule } from '@angular/material/sidenav';
import { MatSnackBar, MatSnackBarModule } from '@angular/material/snack-bar';
import { AdvParser, type AdvDocument } from './core/adv-parser';
import { SceneTree } from './components/scene-tree/scene-tree';
import { SectionEditor } from './components/section-editor/section-editor';
import { AdventureTree } from './components/adventure-tree/adventure-tree';
import { PreambleEditor } from './components/preamble-editor/preamble-editor';

const DEMO = `@title  Nouvelle aventure
@author Vous
@start  depart

:: depart
La porte du temple s'ouvre sur le noir.

* [Entrer sans lumiere] -> noir
* [Rebrousser chemin] -> fin_lache

:: noir
Vous n'y voyez rien. Un bruit, tout pres.

* [Foncer] -> fin_victoire
* [Reculer] -> depart

:: fin_victoire
@ending victoire
Vous ressortez, le tresor sous le bras.

:: fin_lache
@ending defaite
Vous ne saurez jamais ce qu'il y avait derriere cette porte.
`;

@Component({
  selector: 'app-root',
  imports: [
    MatToolbarModule,
    MatButtonModule,
    MatIconModule,
    MatTabsModule,
    MatSidenavModule,
    MatSnackBarModule,
    SceneTree,
    SectionEditor,
    AdventureTree,
    PreambleEditor,
  ],
  templateUrl: './app.html',
  styleUrl: './app.scss',
})
export class App {
  private readonly parser = inject(AdvParser);
  private readonly snackBar = inject(MatSnackBar);

  readonly fileName = signal('nouvelle-aventure.adv');
  readonly doc = signal<AdvDocument>(this.parser.parse(DEMO));
  readonly selectedSection = signal<string | null>('depart');
  readonly selectedIsPreamble = signal(false);
  readonly activeTab = signal(0); // 0 = Editeur, 1 = Graphe

  readonly sectionCount = computed(() => this.doc().sections.length);

  /** Depuis l'onglet Graphe (lecture seule) : selectionner ET revenir a
   * l'onglet Editeur pour travailler sur cette section. */
  onGraphSelect(name: string): void {
    this.selectedSection.set(name);
    this.selectedIsPreamble.set(false);
    this.activeTab.set(0);
  }

  onSelectPreamble(): void {
    this.selectedIsPreamble.set(true);
  }

  /** Mutations structurelles (ajout/suppression de stat/flag/objet/section/
   * chapitre) : le service renvoie un document FRAIS (re-analyse complete),
   * on remplace juste la reference. */
  onDocChanged(next: AdvDocument): void {
    this.doc.set(next);
  }

  onImport(event: Event): void {
    const input = event.target as HTMLInputElement;
    const file = input.files?.[0];
    if (!file) return;
    file.text().then((text) => {
      const parsed = this.parser.parse(text);
      if (!parsed.start || parsed.sections.length === 0) {
        this.snackBar.open(
          "Aucune section trouvee (attendu : '::' en debut de ligne). Fichier ouvert quand meme.",
          'OK',
          { duration: 5000 },
        );
      }
      this.doc.set(parsed);
      this.fileName.set(file.name);
      this.selectedSection.set(parsed.start || parsed.sections[0]?.name || null);
      this.selectedIsPreamble.set(false);
    });
    input.value = '';
  }

  onExport(): void {
    const text = this.parser.serialize(this.doc());
    const blob = new Blob([text], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = this.fileName();
    a.click();
    URL.revokeObjectURL(url);
  }

  onSelect(name: string): void {
    this.selectedSection.set(name);
    this.selectedIsPreamble.set(false);
  }

  onSaved(): void {
    // Le service mute doc() en place ; on force une nouvelle reference pour
    // que les signaux derives (arbre, editeur) se recalculent.
    this.doc.set({ ...this.doc() });
    this.snackBar.open('Section enregistree.', undefined, { duration: 1500 });
  }
}
