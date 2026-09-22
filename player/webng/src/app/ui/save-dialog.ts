// save-dialog.ts -- choix de l'emplacement où sauvegarder.

import { Component, input, output } from "@angular/core";
import type { Engine } from "../engine/engine";
import { Ui } from "../engine/format";

@Component({
  selector: "app-save-dialog",
  template: `
    <div class="a2-modal-backdrop" (click)="cancel.emit()">
      <div class="a2-modal" (click)="$event.stopPropagation()">
        <p>{{ engine().ui(Ui.SAVING) }}</p>
        <div class="a2-modal-actions">
          @for (s of engine().saveSlots(); track s.slot) {
            <button [class.a2-primary]="s.current" (click)="pick.emit(s.slot)">
              <strong>{{ s.slot }}</strong> · {{ s.summary || "vide" }}
            </button>
          }
          <button (click)="cancel.emit()">{{ engine().ui(Ui.QUIT_CANCEL) }}</button>
        </div>
      </div>
    </div>
  `,
})
export class SaveDialog {
  engine = input.required<Engine>();
  pick = output<number>();
  cancel = output<void>();
  protected readonly Ui = Ui;
}
