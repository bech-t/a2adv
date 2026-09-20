// confirm-dialog.ts -- confirmation de retour au menu (S = sauver puis revenir,
// Q = revenir sans sauver, ESC = annuler cote natif -- ici trois boutons
// plutot qu'une invite clavier).

import { Component, input, output } from "@angular/core";
import type { Engine } from "../engine/engine";
import { Ui } from "../engine/format";

@Component({
  selector: "app-confirm-dialog",
  template: `
    <div class="a2-modal-backdrop" (click)="cancel.emit()">
      <div class="a2-modal" (click)="$event.stopPropagation()">
        <p>{{ engine().ui(Ui.QUIT_CONFIRM) }}</p>
        <div class="a2-modal-actions">
          <button (click)="save.emit()">{{ engine().ui(Ui.QUIT_SAVE) }}</button>
          <button (click)="discard.emit()">{{ engine().ui(Ui.QUIT_NOSAVE) }}</button>
          <button (click)="cancel.emit()">{{ engine().ui(Ui.QUIT_CANCEL) }}</button>
        </div>
      </div>
    </div>
  `,
})
export class ConfirmDialog {
  engine = input.required<Engine>();
  save = output<void>();
  discard = output<void>();
  cancel = output<void>();
  protected readonly Ui = Ui;
}
