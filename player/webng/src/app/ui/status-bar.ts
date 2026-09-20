// status-bar.ts -- bandeau de stats/score/mouvements (Flexbox gere
// l'alignement).

import { Component, computed, input } from "@angular/core";
import type { Engine, StatusInfo } from "../engine/engine";
import { Ui } from "../engine/format";

@Component({
  selector: "app-status-bar",
  template: `
    <div class="a2-status">
      <div class="a2-status-stats">
        @for (s of visibleStats(); track s.name) {
          <span class="a2-stat"><span class="a2-stat-name">{{ s.name }}</span> {{ s.value }}</span>
        }
      </div>
      @if (status().score !== null || status().moves !== null) {
        <div class="a2-status-counters">
          @if (status().score !== null) {
            <span>{{ engine().ui(Ui.SCORE) }} {{ status().score }}</span>
          }
          @if (status().moves !== null) {
            <span>{{ engine().ui(Ui.MOVES) }} {{ status().moves }}</span>
          }
        </div>
      }
    </div>
  `,
})
export class StatusBar {
  engine = input.required<Engine>();
  status = input.required<StatusInfo>();
  protected readonly Ui = Ui;

  protected readonly visibleStats = computed(() => this.status().stats.filter((s) => !s.hidden));
}
