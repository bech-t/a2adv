// combat.ts -- ecran de combat. Un tap
// sur "ATTAQUER" = n'importe quelle touche cote natif ; le bouton FUIR
// n'apparait que si @flee existe (canFlee).

import { Component, input } from "@angular/core";
import type { Engine, CombatView } from "../engine/engine";
import { Ui } from "../engine/format";
import { imagePath } from "../assets/loader";
import { RichText } from "./rich-text";
import { StatusBar } from "./status-bar";

@Component({
  selector: "app-combat",
  imports: [RichText, StatusBar],
  template: `
    <div class="a2-screen a2-combat">
      <app-status-bar [engine]="engine()" [status]="combat().status" />
      @if (combat().enemyImage !== null) {
        <img
          class="a2-image"
          [src]="imagePath(assetBase(), combat().enemyImage!)"
          alt=""
          (error)="hide($event)"
        />
      }
      <div class="a2-combat-header">
        <span class="a2-enemy-name">{{ combat().enemyName }}</span>
        <span>{{ engine().ui(Ui.CB_HP) }} {{ combat().enemyHp }}</span>
      </div>

      @if (combat().intro) {
        <div class="a2-text">
          <app-rich-text [paragraphs]="combat().intro!" />
        </div>
      }

      @if (combat().last && !combat().intro) {
        <div class="a2-combat-log">
          <p>{{ engine().ui(Ui.CB_DICE) }} {{ combat().last!.pscore }} / {{ combat().last!.escore }}</p>
          @if (combat().last!.lastTo === 0) {
            <p>{{ combat().enemyName }} -{{ combat().last!.lastDmg }}</p>
          }
          @if (combat().last!.lastTo === 1) {
            <p>{{ engine().ui(Ui.CB_YOU) }} -{{ combat().last!.lastDmg }}</p>
          }
          @if (combat().last!.lastTo === 2) {
            <p>{{ engine().ui(Ui.CB_PARRY) }}</p>
          }
        </div>
      }

      @if (combat().outcomeMessage) {
        <div class="a2-text">
          <p>{{ combat().outcomeMessage }}</p>
        </div>
      }

      @if (combat().outcome !== null) {
        <button class="a2-primary" (click)="engine().continueAfterCombat()">
          {{ engine().ui(Ui.ANYKEY) }}
        </button>
      } @else {
        <div class="a2-choices">
          <button class="a2-primary" (click)="engine().combatAttack()">
            {{ engine().ui(Ui.CB_ATTACK) }}
          </button>
          @if (combat().canFlee) {
            <button (click)="engine().combatFlee()">{{ engine().ui(Ui.CB_FLEE) }}</button>
          }
        </div>
      }
    </div>
  `,
})
export class CombatScreen {
  engine = input.required<Engine>();
  combat = input.required<CombatView>();
  assetBase = input.required<string>();
  protected readonly Ui = Ui;
  protected readonly imagePath = imagePath;

  protected hide(e: Event): void {
    (e.target as HTMLElement).style.display = "none";
  }
}
