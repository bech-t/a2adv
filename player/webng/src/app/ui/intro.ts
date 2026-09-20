// intro.ts -- scene d'introduction (un
// tap sur l'ecran avance, un bouton "Passer" saute).

import { Component, input } from "@angular/core";
import type { Engine, IntroView } from "../engine/engine";
import { Ui } from "../engine/format";
import { imagePath } from "../assets/loader";
import { RichText } from "./rich-text";

@Component({
  selector: "app-intro",
  imports: [RichText],
  template: `
    <div class="a2-screen a2-intro" (click)="engine().introNext()">
      @if (intro().image !== null) {
        <img
          class="a2-image"
          [src]="imagePath(assetBase(), intro().image!)"
          alt=""
          (error)="hide($event)"
        />
      }
      <div class="a2-text">
        <app-rich-text [paragraphs]="intro().texts" />
      </div>
      <div class="a2-toolbar" (click)="$event.stopPropagation()">
        <button class="a2-primary" (click)="engine().introNext()">
          {{ engine().ui(Ui.INTRO_HINT) || engine().ui(Ui.ANYKEY) }}
        </button>
        @if (intro().count > 1) {
          <button (click)="engine().introSkip()">»</button>
        }
      </div>
    </div>
  `,
})
export class IntroScreen {
  engine = input.required<Engine>();
  intro = input.required<IntroView>();
  assetBase = input.required<string>();
  protected readonly Ui = Ui;
  protected readonly imagePath = imagePath;

  protected hide(e: Event): void {
    (e.target as HTMLElement).style.display = "none";
  }
}
