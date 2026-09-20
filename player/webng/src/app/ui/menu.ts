// menu.ts -- menu principal.

import { Component, input } from "@angular/core";
import { RouterLink } from "@angular/router";
import type { Engine, View } from "../engine/engine";
import { Ui } from "../engine/format";

@Component({
  selector: "app-menu",
  imports: [RouterLink],
  template: `
    <div class="a2-screen a2-menu">
      <h1>{{ view().title }}</h1>
      @if (view().version) {
        <p class="a2-version">V{{ view().version }}</p>
      }
      <div class="a2-menu-actions">
        <button class="a2-primary" (click)="engine().startNewGame(seed())">
          {{ engine().ui(Ui.MENU_NEW) }}
        </button>
        <button [disabled]="!view().canContinue" (click)="engine().continueGame()">
          {{ engine().ui(Ui.MENU_LOAD) }}
        </button>
        <a class="a2-button" routerLink="/">Catalogue</a>
      </div>
    </div>
  `,
})
export class MenuScreen {
  engine = input.required<Engine>();
  view = input.required<Extract<View, { kind: "menu" }>>();
  protected readonly Ui = Ui;

  protected seed(): number {
    return Date.now() & 0xffff;
  }
}
