import {
  AfterViewInit,
  Component,
  ElementRef,
  OnDestroy,
  effect,
  input,
  output,
  viewChild,
} from '@angular/core';
import { EditorState } from '@codemirror/state';
import { EditorView, keymap, lineNumbers, highlightActiveLine } from '@codemirror/view';
import { defaultKeymap, history, historyKeymap } from '@codemirror/commands';
import { defaultHighlightStyle, syntaxHighlighting, indentOnInput } from '@codemirror/language';
import { advLanguage } from '../../core/adv-language';

/**
 * Éditeur de texte CodeMirror pour le corps d'une section — coloration
 * syntaxique du `.adv` (voir `adv-language.ts`). Composant "contrôlé" : la
 * valeur vient de `value()`, toute frappe émet `valueChange`, sans état
 * caché ailleurs que dans le document CodeMirror lui-même.
 */
@Component({
  selector: 'app-code-editor',
  template: `<div #host class="cm-host"></div>`,
  styles: [
    `
      :host {
        display: block;
        height: 100%;
      }
      .cm-host {
        height: 100%;
      }
      .cm-host :global(.cm-editor) {
        height: 100%;
        font-size: 0.85rem;
      }
      .cm-host :global(.cm-scroller) {
        font-family: 'JetBrains Mono', 'Cascadia Code', ui-monospace, monospace;
        line-height: 1.5;
      }
    `,
  ],
})
export class CodeEditor implements AfterViewInit, OnDestroy {
  readonly value = input('');
  readonly valueChange = output<string>();

  private readonly host = viewChild.required<ElementRef<HTMLDivElement>>('host');
  private view: EditorView | null = null;
  private lastEmitted = '';

  constructor() {
    // Resynchronise le contenu quand `value()` change POUR UNE AUTRE RAISON
    // que notre propre frappe (ex : changement de section sélectionnée).
    effect(() => {
      const v = this.value();
      if (this.view && v !== this.lastEmitted && v !== this.view.state.doc.toString()) {
        this.view.dispatch({
          changes: { from: 0, to: this.view.state.doc.length, insert: v },
        });
      }
    });
  }

  ngAfterViewInit(): void {
    const initial = this.value();
    this.lastEmitted = initial;
    this.view = new EditorView({
      parent: this.host().nativeElement,
      state: EditorState.create({
        doc: initial,
        extensions: [
          lineNumbers(),
          history(),
          highlightActiveLine(),
          indentOnInput(),
          advLanguage,
          syntaxHighlighting(defaultHighlightStyle, { fallback: true }),
          keymap.of([...defaultKeymap, ...historyKeymap]),
          EditorView.lineWrapping,
          EditorView.updateListener.of((update) => {
            if (update.docChanged) {
              this.lastEmitted = update.state.doc.toString();
              this.valueChange.emit(this.lastEmitted);
            }
          }),
        ],
      }),
    });
  }

  ngOnDestroy(): void {
    this.view?.destroy();
  }
}
