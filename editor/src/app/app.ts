import { Component, HostListener, computed, effect, inject, signal, viewChild } from '@angular/core';
import { MatToolbarModule } from '@angular/material/toolbar';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatTabsModule } from '@angular/material/tabs';
import { MatSidenavModule } from '@angular/material/sidenav';
import { MatSnackBar, MatSnackBarModule } from '@angular/material/snack-bar';
import { MatDialog, MatDialogModule } from '@angular/material/dialog';
import { MatTooltipModule } from '@angular/material/tooltip';
import { AdvParser, type AdvDocument } from './core/adv-parser';
import { validateDocument } from './core/adv-validate';
import { SceneTree } from './pages/scene-tree/scene-tree';
import { SectionEditor } from './pages/section-editor/section-editor';
import { AdventureTree } from './components/adventure-tree/adventure-tree';
import { PreambleEditor } from './pages/preamble-editor/preamble-editor';
import { ValidationPanel } from './pages/validation-panel/validation-panel';
import { ConfirmDialog } from './components/confirm-dialog/confirm-dialog';

const AUTOSAVE_KEY = 'a2adv-editor:autosave';
const MAX_HISTORY = 100;

interface Autosave {
  fileName: string;
  text: string;
}

/** Lu une seule fois, à l'initialisation des signaux (cf. `App.autosave`) —
 * best effort comme tout ce qui touche au stockage navigateur : stockage
 * bloqué, navigation privée ou JSON corrompu retombent sur `DEMO`, jamais
 * une erreur visible (même philosophie que le "meilleur effort" du player,
 * cf. ramdisk.c côté Apple II). */
function loadAutosave(): Autosave | null {
  try {
    const raw = localStorage.getItem(AUTOSAVE_KEY);
    if (!raw) return null;
    const parsed = JSON.parse(raw);
    if (typeof parsed?.text === 'string' && typeof parsed?.fileName === 'string') return parsed;
  } catch {
    /* stockage indisponible ou corrompu : on démarre sur DEMO */
  }
  return null;
}

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
@ending win
Vous ressortez, le tresor sous le bras.

:: fin_lache
@ending lose
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
    MatDialogModule,
    MatTooltipModule,
    SceneTree,
    SectionEditor,
    AdventureTree,
    PreambleEditor,
    ValidationPanel,
  ],
  templateUrl: './app.html',
  styleUrl: './app.scss',
})
export class App {
  private readonly parser = inject(AdvParser);
  private readonly snackBar = inject(MatSnackBar);
  private readonly dialog = inject(MatDialog);

  private readonly autosave = loadAutosave();
  private autosaveTimer: ReturnType<typeof setTimeout> | null = null;
  /** Present seulement quand l'onglet Editeur affiche une section (pas le
   * preambule) : sert a flush le texte narratif non encore commis avant de
   * changer de section/onglet/export, cf. `SectionEditor.commitDraft`. */
  private readonly sectionEditorView = viewChild(SectionEditor);

  readonly fileName = signal(this.autosave?.fileName ?? 'nouvelle-aventure.adv');
  readonly doc = signal<AdvDocument>(this.parser.parse(this.autosave?.text ?? DEMO));
  readonly selectedSection = signal<string | null>(this.doc().start || this.doc().sections[0]?.name || null);
  readonly selectedIsPreamble = signal(false);
  readonly activeTab = signal(0); // 0 = Editeur, 1 = Graphe, 2 = Problemes

  readonly sectionCount = computed(() => this.doc().sections.length);
  readonly issues = computed(() => validateDocument(this.doc()));
  readonly errorCount = computed(() => this.issues().filter((i) => i.severity === 'error').length);

  // --- Annuler/rétablir --------------------------------------------------
  //
  // Pile de textes SOURCE (pas de AdvDocument : plus simple a empiler, et un
  // retour arriere passe de toute facon par une re-analyse complete). Chaque
  // point d'entree de mutation (onDocChanged/onDocTouched/onSaved) empile le
  // texte D'AVANT avant d'appliquer le changement -- jamais un doc() relu
  // apres coup, qui peut deja refleter la mutation (cf. commentaires sur
  // `SectionEditor.docTouched`/`saved`).
  private undoStack: string[] = [];
  private redoStack: string[] = [];
  readonly canUndo = signal(false);
  readonly canRedo = signal(false);

  constructor() {
    if (this.autosave) {
      this.snackBar.open('Brouillon restauré depuis votre dernier passage.', 'OK', { duration: 4000 });
    }
    // Ecrit a chaque changement de doc/nom de fichier -- debounce court pour
    // eviter une ecriture par frappe sur les champs structures (choix, etc).
    effect(() => {
      const text = this.parser.serialize(this.doc());
      const fileName = this.fileName();
      if (this.autosaveTimer) clearTimeout(this.autosaveTimer);
      this.autosaveTimer = setTimeout(() => {
        try {
          localStorage.setItem(AUTOSAVE_KEY, JSON.stringify({ fileName, text } satisfies Autosave));
        } catch {
          /* quota depasse, navigation privee... le brouillon reste en memoire seulement */
        }
      }, 500);
    });
  }

  /** Touche clavier globale (Ctrl/Cmd+Z, Ctrl/Cmd+Maj+Z ou +Y) -- ignorée si
   * le focus est dans un champ de saisie ou l'éditeur CodeMirror : ceux-là
   * ont déjà leur propre undo local (texte en cours de frappe), qu'il ne
   * faut pas court-circuiter avec l'historique global du document. */
  @HostListener('window:keydown', ['$event'])
  onKeydown(event: KeyboardEvent): void {
    const target = event.target as HTMLElement | null;
    if (target?.closest('.cm-editor, input, textarea, [contenteditable="true"]')) return;
    if (!(event.ctrlKey || event.metaKey)) return;
    const key = event.key.toLowerCase();
    if (key === 'z' && !event.shiftKey) {
      event.preventDefault();
      this.undo();
    } else if (key === 'y' || (key === 'z' && event.shiftKey)) {
      event.preventDefault();
      this.redo();
    }
  }

  private pushHistory(beforeText: string): void {
    this.undoStack.push(beforeText);
    if (this.undoStack.length > MAX_HISTORY) this.undoStack.shift();
    this.redoStack = [];
    this.canUndo.set(true);
    this.canRedo.set(false);
  }

  undo(): void {
    if (!this.undoStack.length) return;
    const current = this.parser.serialize(this.doc());
    const prev = this.undoStack.pop()!;
    this.redoStack.push(current);
    this.doc.set(this.parser.parse(prev));
    this.reconcileSelection();
    this.canUndo.set(this.undoStack.length > 0);
    this.canRedo.set(true);
  }

  redo(): void {
    if (!this.redoStack.length) return;
    const current = this.parser.serialize(this.doc());
    const next = this.redoStack.pop()!;
    this.undoStack.push(current);
    this.doc.set(this.parser.parse(next));
    this.reconcileSelection();
    this.canUndo.set(true);
    this.canRedo.set(this.redoStack.length > 0);
  }

  /** Après un annuler/rétablir, la section affichée peut avoir disparu
   * (annulation de son ajout, par ex.) : retombe sur @start plutôt que sur
   * un écran vide. */
  private reconcileSelection(): void {
    if (this.selectedIsPreamble()) return;
    const name = this.selectedSection();
    const doc = this.doc();
    if (name && !doc.sections.some((s) => s.name === name)) {
      this.selectedSection.set(doc.start || doc.sections[0]?.name || null);
    }
  }

  onNew(): void {
    this.dialog
      .open(ConfirmDialog, {
        data: {
          title: 'Nouvelle aventure ?',
          message: 'Le brouillon actuel sera remplacé par le modèle de départ.',
          warning: "Pensez à l'exporter avant si vous voulez le garder.",
          confirmLabel: 'Nouvelle aventure',
          danger: true,
        },
      })
      .afterClosed()
      .subscribe((ok: boolean) => {
        if (!ok) return;
        try {
          localStorage.removeItem(AUTOSAVE_KEY);
        } catch {
          /* rien a faire de plus : best effort */
        }
        const parsed = this.parser.parse(DEMO);
        this.doc.set(parsed);
        this.fileName.set('nouvelle-aventure.adv');
        this.selectedSection.set(parsed.start || parsed.sections[0]?.name || null);
        this.selectedIsPreamble.set(false);
        this.activeTab.set(0);
        this.clearHistory();
      });
  }

  /** Efface l'historique annuler/rétablir : un nouveau document sans rapport
   * avec le précédent (import, nouvelle aventure) rendrait un retour arrière
   * dedans incohérent. */
  private clearHistory(): void {
    this.undoStack = [];
    this.redoStack = [];
    this.canUndo.set(false);
    this.canRedo.set(false);
  }

  /** Depuis l'onglet Graphe (lecture seule) : selectionner ET revenir a
   * l'onglet Editeur pour travailler sur cette section. */
  onGraphSelect(name: string): void {
    this.sectionEditorView()?.commitDraft();
    this.selectedSection.set(name);
    this.selectedIsPreamble.set(false);
    this.activeTab.set(0);
  }

  onSelectPreamble(): void {
    this.sectionEditorView()?.commitDraft();
    this.selectedIsPreamble.set(true);
  }

  /** Mutations structurelles (ajout/suppression de stat/flag/objet/section/
   * chapitre) : le service renvoie un document FRAIS (re-analyse complete),
   * on remplace juste la reference. `doc()` est encore l'ANCIEN etat ici
   * (ce chemin ne mute jamais en place), donc sur de l'empiler tel quel. */
  onDocChanged(next: AdvDocument): void {
    this.pushHistory(this.parser.serialize(this.doc()));
    this.doc.set(next);
  }

  /** Choix/effet/combat/ask édités depuis `section-editor` : `doc` a déjà
   * été muté en place, `before` porte le texte tel qu'il était avant. */
  onDocTouched(before: string): void {
    this.pushHistory(before);
    this.doc.set({ ...this.doc() });
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
      this.clearHistory();
    });
    input.value = '';
  }

  onExport(): void {
    this.sectionEditorView()?.commitDraft();
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
    this.sectionEditorView()?.commitDraft();
    this.selectedSection.set(name);
    this.selectedIsPreamble.set(false);
  }

  onSaved(before: string): void {
    // `commit()` mute doc() en place ; on force une nouvelle reference pour
    // que les signaux derives (arbre, editeur) se recalculent.
    this.pushHistory(before);
    this.doc.set({ ...this.doc() });
    this.snackBar.open('Section enregistree.', undefined, { duration: 1500 });
  }
}
