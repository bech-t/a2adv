// adventure-page.ts -- fiche d'une aventure : presentation, emplacements de
// sauvegarde (reprendre, nouvelle partie, exporter, importer, supprimer) et
// telechargement pour jouer hors ligne.

import { Component, OnInit, computed, inject, input, signal } from "@angular/core";
import { RouterLink } from "@angular/router";
import { CatalogService } from "../assets/catalog";
import { Offline } from "../assets/offline";
import {
  clearSlot,
  listSlots,
  parseSaveFile,
  saveFileName,
  slotSummary,
  toSaveFile,
  writeSlot,
  type SaveSlot,
} from "../assets/save";

@Component({
  selector: "app-adventure-page",
  imports: [RouterLink],
  template: `
    <main class="a2-page">
      <a class="a2-back" routerLink="/">‹ Catalogue</a>
      @if (entry(); as a) {
        @if (a.cover) {
          <img class="a2-image" [src]="a.cover" alt="" />
        }
        <h1>{{ a.title }}</h1>
        <p class="a2-muted">
          @if (a.author) {
            {{ a.author }} ·
          }
          @if (a.version) {
            V{{ a.version }} ·
          }
          {{ a.sections }} sections · {{ size() }}
          @if (a.license) {
            · Licence {{ a.license }}
          }
        </p>
        @if (a.description) {
          <p>{{ a.description }}</p>
        }

        @if (!anySave()) {
          <div class="a2-actions">
            <a
              class="a2-button a2-primary"
              [routerLink]="['/play', a.id]"
              [queryParams]="{ mode: 'new', slot: 1 }"
              >Jouer</a
            >
            <label class="a2-button">
              Importer une sauvegarde
              <input type="file" accept=".json,application/json" hidden (change)="importFile($event, 1)" />
            </label>
          </div>
        } @else {
          <h2>Sauvegardes</h2>
          <ul class="a2-slots">
            @for (s of slots(); track $index) {
              <li class="a2-slot">
                <div class="a2-slot-head">
                  <strong>Emplacement {{ $index + 1 }}</strong>
                  <span class="a2-muted">{{ summary(s) }}</span>
                </div>
                @if (s && s.hash && s.hash !== a.hash) {
                  <p class="a2-warn">
                    Écrite avec une autre version de l'aventure : elle peut ne plus se charger.
                  </p>
                }
                <div class="a2-slot-actions">
                  @if (s) {
                    <a
                      class="a2-button a2-primary"
                      [routerLink]="['/play', a.id]"
                      [queryParams]="{ mode: 'resume', slot: $index + 1 }"
                      >Continuer</a
                    >
                  }
                  <a
                    class="a2-button"
                    [routerLink]="['/play', a.id]"
                    [queryParams]="{ mode: 'new', slot: $index + 1 }"
                    >Nouvelle partie</a
                  >
                  @if (s) {
                    <button (click)="exportSlot($index + 1, s)">Exporter</button>
                  }
                  <label class="a2-button">
                    Importer
                    <input
                      type="file"
                      accept=".json,application/json"
                      hidden
                      (change)="importFile($event, $index + 1)"
                    />
                  </label>
                  @if (s) {
                    @if (confirmDelete() === $index + 1) {
                      <button class="a2-danger" (click)="deleteSlot($index + 1)">Confirmer</button>
                      <button (click)="confirmDelete.set(null)">Annuler</button>
                    } @else {
                      <button (click)="confirmDelete.set($index + 1)">Supprimer</button>
                    }
                  }
                </div>
              </li>
            }
          </ul>
        }
        @if (message(); as m) {
          <p [class]="m.error ? 'a2-boot-error' : 'a2-muted'">{{ m.text }}</p>
        }

        @if (offline.supported) {
          <h2>Hors ligne</h2>
          @if (offline.progress()[a.id]; as p) {
            <p class="a2-muted">Téléchargement… {{ p.done }} / {{ p.total }} fichiers</p>
          } @else if (offline.isAvailable(a.id)) {
            <p class="a2-muted">Cette aventure est disponible sans connexion.</p>
            <button (click)="removeOffline()">Supprimer les fichiers hors ligne</button>
          } @else {
            <p class="a2-muted">Téléchargez l'aventure pour y jouer sans connexion.</p>
            <button (click)="download()">Télécharger pour jouer hors ligne ({{ size() }})</button>
          }
        }
      } @else if (service.error(); as err) {
        <p class="a2-boot-error">Catalogue indisponible : {{ err }}</p>
      } @else if (!service.catalog()) {
        <p class="a2-muted">Chargement…</p>
      } @else {
        <p class="a2-boot-error">Aventure inconnue : « {{ id() }} ».</p>
      }
    </main>
  `,
})
export class AdventurePage implements OnInit {
  id = input.required<string>();
  protected readonly service = inject(CatalogService);
  protected readonly offline = inject(Offline);

  // localStorage n'est pas reactif : `version` relance le calcul des emplacements.
  private readonly version = signal(0);
  protected readonly confirmDelete = signal<number | null>(null);
  protected readonly message = signal<{ text: string; error: boolean } | null>(null);

  protected readonly entry = computed(() => {
    this.service.catalog();
    return this.service.entry(this.id());
  });
  protected readonly slots = computed(() => {
    this.version();
    return listSlots(this.id());
  });
  protected readonly anySave = computed(() => this.slots().some((s) => s !== null));
  protected readonly size = computed(() => {
    const kb = Math.max(1, Math.round((this.entry()?.bytes ?? 0) / 1024));
    return kb >= 1024 ? `${(kb / 1024).toFixed(1)} Mo` : `${kb} Ko`;
  });

  ngOnInit(): void {
    void this.service.load().then(() => this.offline.refresh(this.entries()));
  }

  private entries() {
    return this.service.catalog()?.adventures ?? [];
  }

  protected summary(slot: SaveSlot | null): string {
    return slotSummary(slot) || "vide";
  }

  protected exportSlot(n: number, slot: SaveSlot): void {
    const text = JSON.stringify(toSaveFile(this.id(), slot), null, 2);
    const url = URL.createObjectURL(new Blob([text], { type: "application/json" }));
    const link = document.createElement("a");
    link.href = url;
    link.download = saveFileName(this.id(), n);
    link.click();
    setTimeout(() => URL.revokeObjectURL(url));
  }

  protected async importFile(event: Event, n: number): Promise<void> {
    const input = event.target as HTMLInputElement;
    const file = input.files?.[0];
    input.value = ""; // permet de reimporter le meme fichier
    if (!file) return;
    try {
      const slot = parseSaveFile(await file.text(), this.id());
      if (!writeSlot(this.id(), n, slot)) throw new Error("le navigateur refuse d'écrire");
      this.message.set({ text: `Sauvegarde importée dans l'emplacement ${n}.`, error: false });
      this.version.update((v) => v + 1);
    } catch (e) {
      const why = e instanceof Error ? e.message : String(e);
      this.message.set({ text: `Import impossible : ${why}.`, error: true });
    }
  }

  protected deleteSlot(n: number): void {
    clearSlot(this.id(), n);
    this.confirmDelete.set(null);
    this.message.set(null);
    this.version.update((v) => v + 1);
  }

  protected async download(): Promise<void> {
    const entry = this.entry();
    if (!entry) return;
    this.message.set(null);
    try {
      await this.offline.download(entry, this.entries());
    } catch (e) {
      this.message.set({ text: e instanceof Error ? e.message : String(e), error: true });
    }
  }

  protected async removeOffline(): Promise<void> {
    const entry = this.entry();
    if (entry) await this.offline.remove(entry, this.entries());
  }
}
