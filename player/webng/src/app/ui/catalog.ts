// catalog.ts -- page d'accueil : une carte par aventure du catalogue.

import { Component, OnInit, computed, inject } from "@angular/core";
import { RouterLink } from "@angular/router";
import { CatalogService } from "../assets/catalog";
import { Offline } from "../assets/offline";
import { hasAnySave } from "../assets/save";

@Component({
  selector: "app-catalog",
  imports: [RouterLink],
  template: `
    <main class="a2-page">
      <header class="a2-page-head">
        <h1>Livres-jeux</h1>
        <p class="a2-muted">Choisissez une aventure. Elle se joue dans le navigateur, hors ligne une fois chargée.</p>
      </header>
      @if (service.error(); as err) {
        <p class="a2-boot-error">Catalogue indisponible : {{ err }}</p>
      } @else if (!service.catalog()) {
        <p class="a2-muted">Chargement…</p>
      } @else {
        <ul class="a2-cards">
          @for (a of service.catalog()!.adventures; track a.id) {
            <li>
              <a class="a2-card" [routerLink]="['/adv', a.id]">
                <span class="a2-card-cover">
                  @if (a.cover) {
                    <img [src]="a.cover" alt="" loading="lazy" />
                  } @else {
                    <span class="a2-card-nocover">{{ a.title }}</span>
                  }
                </span>
                <span class="a2-card-body">
                  <strong>{{ a.title }}</strong>
                  @if (a.author) {
                    <span class="a2-muted">{{ a.author }}</span>
                  }
                  @if (a.description) {
                    <span class="a2-card-desc">{{ a.description }}</span>
                  }
                  <span class="a2-badges">
                    @if (saved().has(a.id)) {
                      <span class="a2-badge">Partie en cours</span>
                    }
                    @if (offline.isAvailable(a.id)) {
                      <span class="a2-badge a2-badge-alt">Hors ligne</span>
                    }
                  </span>
                </span>
              </a>
            </li>
          }
        </ul>
      }
    </main>
  `,
})
export class Catalog implements OnInit {
  protected readonly service = inject(CatalogService);
  protected readonly offline = inject(Offline);
  protected readonly saved = computed(
    () =>
      new Set(
        (this.service.catalog()?.adventures ?? [])
          .filter((a) => hasAnySave(a.id))
          .map((a) => a.id),
      ),
  );

  ngOnInit(): void {
    void this.service.load().then(() => this.offline.refresh(this.service.catalog()?.adventures ?? []));
  }
}
