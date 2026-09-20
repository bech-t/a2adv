// play.ts -- charge une aventure du catalogue, construit le moteur puis
// delegue tout le reste a <app-game>. Deux parametres d'URL : `slot`
// (emplacement de sauvegarde, 1 par defaut) et `mode` -- "new" demarre
// directement une partie, "resume" reprend la sauvegarde de l'emplacement ;
// sans mode, le menu du jeu s'affiche.

import { Component, OnInit, inject, input, signal } from "@angular/core";
import { Title } from "@angular/platform-browser";
import { RouterLink } from "@angular/router";
import { CatalogService } from "../assets/catalog";
import { storyFetcher } from "../assets/loader";
import { SLOT_COUNT, localSaveStore } from "../assets/save";
import { playSound } from "../audio/snd";
import { Engine } from "../engine/engine";
import { loadStory } from "../engine/story";
import { Game } from "./game";

@Component({
  selector: "app-play",
  imports: [Game, RouterLink],
  template: `
    @if (error(); as err) {
      <div class="a2-boot a2-boot-error">
        <div>
          Erreur de chargement de « {{ id() }} » : {{ err }}
          <br /><a [routerLink]="['/adv', id()]">Retour à l'aventure</a>
        </div>
      </div>
    } @else if (!engine()) {
      <div class="a2-boot">Chargement…</div>
    } @else {
      <app-game [engine]="engine()!" [assetBase]="base()" />
    }
  `,
})
export class Play implements OnInit {
  id = input.required<string>();
  mode = input<string>();
  slot = input<string>();
  protected readonly engine = signal<Engine | null>(null);
  protected readonly base = signal("");
  protected readonly error = signal<string | null>(null);
  private readonly catalog = inject(CatalogService);
  private readonly title = inject(Title);

  ngOnInit(): void {
    void this.start();
  }

  private slotNumber(): number {
    const n = Number(this.slot());
    return Number.isInteger(n) && n >= 1 && n <= SLOT_COUNT ? n : 1;
  }

  private async start(): Promise<void> {
    try {
      await this.catalog.load();
      const entry = this.catalog.entry(this.id());
      if (!entry) throw new Error(this.catalog.error() ?? "aventure inconnue");
      const data = await loadStory(storyFetcher(entry.story));
      const store = localSaveStore(entry.id, this.slotNumber(), entry.hash);
      const engine = new Engine(data, store, playSound);
      this.title.setTitle(entry.title);
      if (this.mode() === "new") {
        engine.startNewGame(Date.now() & 0xffff);
      } else if (this.mode() === "resume" && !engine.continueGame()) {
        throw new Error("sauvegarde introuvable, ou écrite avec une autre version de l'aventure");
      }
      this.base.set(entry.base);
      this.engine.set(engine);
    } catch (e) {
      this.error.set(e instanceof Error ? e.message : String(e));
    }
  }
}
