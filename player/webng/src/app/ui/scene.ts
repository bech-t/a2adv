// scene.ts -- section normale (texte + choix). Image toujours au-dessus du texte (cf. engine/engine.ts,
// entete, pour pourquoi la distinction plein-ecran/mixte du natif n'a pas
// d'equivalent ici).

import { Component, input, signal } from "@angular/core";
import type { Engine, SceneView } from "../engine/engine";
import { Ui } from "../engine/format";
import { imagePath } from "../assets/loader";
import { RichText } from "./rich-text";
import { StatusBar } from "./status-bar";
import { Inventory } from "./inventory";
import { ConfirmDialog } from "./confirm-dialog";
import { SaveDialog } from "./save-dialog";

@Component({
  selector: "app-scene",
  imports: [RichText, StatusBar, Inventory, ConfirmDialog, SaveDialog],
  template: `
    <div class="a2-screen a2-scene">
      <app-status-bar [engine]="engine()" [status]="scene().status" />
      @if (scene().image !== null) {
        <img
          class="a2-image"
          [src]="imagePath(assetBase(), scene().image!)"
          alt=""
          (error)="hide($event)"
        />
      }
      <div class="a2-text">
        <app-rich-text [paragraphs]="scene().texts" />
      </div>

      <div class="a2-choices">
        @for (c of scene().choices; track c.index) {
          <button (click)="engine().choose(c.index)">{{ c.label }}</button>
        }
      </div>

      <div class="a2-toolbar">
        <button (click)="showInventory.set(true)">{{ engine().ui(Ui.INV_HUD) }}</button>
        <button (click)="startSave(false)">
          {{ savedFlash() ? engine().ui(Ui.SAVED) : engine().ui(Ui.SAVING) }}
        </button>
        <!-- pas de cle ui_str pour "revenir au menu" (le natif y va par un
             simple raccourci clavier Q) : pictogramme plutot qu'un mot code
             en dur dans le player. -->
        <button (click)="confirmQuit.set(true)" aria-label="Menu">≡</button>
      </div>

      @if (showInventory()) {
        <app-inventory [engine]="engine()" (close)="showInventory.set(false)" />
      }
      @if (confirmQuit()) {
        <app-confirm-dialog
          [engine]="engine()"
          (save)="startSave(true)"
          (discard)="engine().returnToMenu()"
          (cancel)="confirmQuit.set(false)"
        />
      }
      @if (pickSlot() !== null) {
        <app-save-dialog
          [engine]="engine()"
          (pick)="saveTo($event)"
          (cancel)="pickSlot.set(null)"
        />
      }
    </div>
  `,
})
export class SceneScreen {
  engine = input.required<Engine>();
  scene = input.required<SceneView>();
  assetBase = input.required<string>();
  protected readonly Ui = Ui;
  protected readonly imagePath = imagePath;

  protected readonly showInventory = signal(false);
  protected readonly confirmQuit = signal(false);
  protected readonly savedFlash = signal(false);
  protected readonly pickSlot = signal<"save" | "quit" | null>(null);

  protected hide(e: Event): void {
    (e.target as HTMLElement).style.display = "none";
  }

  /** Ouvre le choix de l'emplacement ; `thenQuit` : retour au menu ensuite. */
  protected startSave(thenQuit: boolean): void {
    if (this.engine().saveSlots().length === 0) {
      this.saveTo(0, thenQuit);
      return;
    }
    this.pickSlot.set(thenQuit ? "quit" : "save");
  }

  protected saveTo(slot: number, thenQuit = this.pickSlot() === "quit"): void {
    this.pickSlot.set(null);
    if (slot > 0) this.engine().saveTo(slot);
    else this.engine().save();
    if (thenQuit) {
      this.engine().returnToMenu();
      return;
    }
    this.savedFlash.set(true);
    setTimeout(() => this.savedFlash.set(false), 1200);
  }
}
