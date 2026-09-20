// app.ts -- composant racine : un simple aiguillage de routes (cf.
// app.routes.ts), l'annonce d'une nouvelle version du site et
// l'enregistrement du Service Worker. Les anciens liens `?adv=<nom>` (avant le
// catalogue) menent toujours au jeu.

import { Component, inject } from "@angular/core";
import { Router, RouterOutlet } from "@angular/router";
import { Offline } from "./assets/offline";

@Component({
  selector: "app-root",
  imports: [RouterOutlet],
  templateUrl: "./app.html",
})
export class App {
  protected readonly offline = inject(Offline);

  constructor() {
    this.offline.register();
    const adv = new URLSearchParams(window.location.search).get("adv");
    if (adv) {
      history.replaceState(null, "", window.location.pathname);
      void inject(Router).navigate(["/play", adv]);
    }
  }
}
