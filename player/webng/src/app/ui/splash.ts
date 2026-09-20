// splash.ts -- @splash : image plein
// ecran avant le texte de la section (simage.c:show_splash). Un clic ou une
// touche passe ; une duree non nulle passe d'elle-meme ; image introuvable :
// on saute directement au texte.

import { Component, DestroyRef, OnInit, inject, input } from "@angular/core";
import type { Engine, SplashView } from "../engine/engine";
import { imagePath } from "../assets/loader";

@Component({
  selector: "app-splash",
  template: `
    <div class="a2-screen a2-splash" (click)="finish()">
      <img
        class="a2-image"
        [src]="imagePath(assetBase(), splash().image)"
        alt=""
        (error)="finish()"
      />
    </div>
  `,
})
export class SplashScreen implements OnInit {
  engine = input.required<Engine>();
  splash = input.required<SplashView>();
  assetBase = input.required<string>();
  protected readonly imagePath = imagePath;

  private readonly destroyRef = inject(DestroyRef);
  private done = false;

  ngOnInit(): void {
    const onKey = () => this.finish();
    window.addEventListener("keydown", onKey);
    const secs = this.splash().secs;
    const timer = secs > 0 ? setTimeout(() => this.finish(), secs * 1000) : null;
    this.destroyRef.onDestroy(() => {
      window.removeEventListener("keydown", onKey);
      if (timer !== null) clearTimeout(timer);
    });
  }

  /** Un seul passage, meme si clic, delai et erreur d'image se croisent. */
  protected finish(): void {
    if (this.done) return;
    this.done = true;
    this.engine().splashDone();
  }
}
