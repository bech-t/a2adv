import { ApplicationConfig, provideBrowserGlobalErrorListeners } from '@angular/core';
//import { provideAnimationsAsync } from '@angular/platform-browser/animations/async';
import { MAT_ICON_DEFAULT_OPTIONS } from '@angular/material/icon';

export const appConfig: ApplicationConfig = {
  providers: [
    provideBrowserGlobalErrorListeners(),
    // Necessaire aux transitions de mat-dialog/mat-expansion-panel/mat-tabs ;
    // absent du scaffold genere par `ng add @angular/material`.
    //provideAnimationsAsync(),
    
    // Material Symbols (jeu d'icones large) plutot que l'ancien Material
    // Icons (trop limite : pas de "swords"/"skull" pour les icones de genre
    // de section). Cf. index.html pour le lien de police correspondant.
    { provide: MAT_ICON_DEFAULT_OPTIONS, useValue: { fontSet: 'material-symbols-outlined' } },
  ],
};
