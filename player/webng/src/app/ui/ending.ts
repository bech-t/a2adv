// ending.ts -- banniere de fin / impasse.

import { Component, input } from "@angular/core";
import type { Engine, EndingView } from "../engine/engine";
import { END_LOSE, END_WIN, Ui } from "../engine/format";

@Component({
  selector: "app-ending",
  template: `
    <div class="a2-screen a2-ending">
      <p class="a2-ending-banner">{{ text() }}</p>
      <button class="a2-primary" (click)="engine().acknowledgeEnding()">
        {{ engine().ui(Ui.ANYKEY) }}
      </button>
    </div>
  `,
})
export class EndingScreen {
  engine = input.required<Engine>();
  ending = input.required<EndingView>();
  protected readonly Ui = Ui;

  protected text(): string {
    const ending = this.ending();
    if (ending.deadEnd) return this.engine().ui(Ui.NO_EXIT);
    if (ending.ending === END_WIN) return this.engine().ui(Ui.END_WIN);
    if (ending.ending === END_LOSE) return this.engine().ui(Ui.END_LOSE);
    return this.engine().ui(Ui.END_GENERIC);
  }
}
