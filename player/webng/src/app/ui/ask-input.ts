// ask-input.ts -- scene a saisie libre (@ask). Champ nomme `inputView` (pas `input`) pour ne pas
// masquer la fonction `input()` d'Angular importee dans ce fichier.

import { Component, input, signal } from "@angular/core";
import type { Engine, InputView } from "../engine/engine";
import { RichText } from "./rich-text";
import { StatusBar } from "./status-bar";

@Component({
  selector: "app-ask-input",
  imports: [RichText, StatusBar],
  template: `
    <div class="a2-screen a2-input">
      <app-status-bar [engine]="engine()" [status]="inputView().status" />
      <div class="a2-text">
        <app-rich-text [paragraphs]="inputView().texts" />
      </div>
      <p class="a2-prompt">{{ inputView().prompt }}</p>
      <form class="a2-input-form" (submit)="onSubmit($event)">
        <input
          type="text"
          autofocus
          [attr.maxlength]="inputView().maxlen"
          [value]="value()"
          (input)="value.set($any($event.target).value)"
        />
        <button type="submit">→</button>
      </form>
    </div>
  `,
})
export class InputScreen {
  engine = input.required<Engine>();
  inputView = input.required<InputView>();
  protected readonly value = signal("");

  protected onSubmit(e: Event): void {
    e.preventDefault();
    this.engine().submitAnswer(this.value());
    this.value.set("");
  }
}
