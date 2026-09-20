import type { Routes } from "@angular/router";
import { AdventurePage } from "./ui/adventure-page";
import { Catalog } from "./ui/catalog";

// Routes en `#` (withHashLocation, cf. app.config.ts) : le site se sert tel
// quel depuis n'importe quel hebergement statique, sans regle de reecriture.
export const routes: Routes = [
  { path: "", component: Catalog, title: "a2adv" },
  { path: "adv/:id", component: AdventurePage },
  {
    path: "play/:id",
    loadComponent: () => import("./ui/play").then((m) => m.Play),
  },
  { path: "**", redirectTo: "" },
];
