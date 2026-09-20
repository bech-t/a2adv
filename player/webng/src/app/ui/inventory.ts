// inventory.ts -- ecran d'inventaire.
// Purement local a l'UI : pas d'action moteur dediee, juste une lecture de
// engine.state/engine.story.

import { Component, computed, input, output } from "@angular/core";
import type { Engine } from "../engine/engine";
import { Ui } from "../engine/format";

interface OwnedItem {
  i: number;
  label: string;
  mods: string;
}

@Component({
  selector: "app-inventory",
  template: `
    <div class="a2-modal-backdrop" (click)="close.emit()">
      <div class="a2-modal" (click)="$event.stopPropagation()">
        <h2>{{ engine().ui(Ui.INVENTORY) }}</h2>
        @if (owned().length === 0) {
          <p>{{ engine().ui(Ui.INV_EMPTY) }}</p>
        } @else {
          <ul class="a2-inventory-list">
            @for (it of owned(); track it.i) {
              <li>
                {{ it.label }}
                @if (it.mods) {
                  <span class="a2-item-mods"> ({{ it.mods }})</span>
                }
              </li>
            }
          </ul>
        }
        <button (click)="close.emit()">{{ engine().ui(Ui.OPT_BACK) }}</button>
      </div>
    </div>
  `,
})
export class Inventory {
  engine = input.required<Engine>();
  close = output<void>();
  protected readonly Ui = Ui;

  protected readonly owned = computed<OwnedItem[]>(() => {
    const engine = this.engine();
    const story = engine.story;
    const attLabel =
      story.combatAtt !== 0xff && story.statName[story.combatAtt]
        ? story.statName[story.combatAtt]
        : engine.ui(Ui.CBT_ATK);
    return story.itemLabel
      .map((label, i) => ({ label, i }))
      .filter(({ i }) => engine.state.itemGet(i))
      .map(({ label, i }) => {
        const atk = story.itemAtk[i];
        const dmg = story.itemDmg[i];
        const armor = story.itemArmor[i];
        const mods = [
          atk ? `${attLabel} ${atk > 0 ? "+" : ""}${atk}` : null,
          dmg ? `${engine.ui(Ui.CBT_DMG)} ${dmg > 0 ? "+" : ""}${dmg}` : null,
          armor ? `${engine.ui(Ui.CBT_ARM)} ${armor > 0 ? "+" : ""}${armor}` : null,
        ].filter((m): m is string => m !== null);
        return { i, label, mods: mods.join(", ") };
      });
  });
}
