// game.ts -- aiguillage vue -> composant. Engine est un store externe (hors
// Angular) : un signal local (`view`) est mis a jour par `engine.subscribe`,
// desabonne via DestroyRef quand le composant est detruit.

import { Component, DestroyRef, OnInit, inject, input, signal } from "@angular/core";
import type { Engine, View } from "../engine/engine";
import { MenuScreen } from "./menu";
import { IntroScreen } from "./intro";
import { SplashScreen } from "./splash";
import { SceneScreen } from "./scene";
import { CombatScreen } from "./combat";
import { InputScreen } from "./ask-input";
import { EndingScreen } from "./ending";

@Component({
  selector: "app-game",
  imports: [MenuScreen, IntroScreen, SplashScreen, SceneScreen, CombatScreen, InputScreen, EndingScreen],
  template: `
    @switch (view()?.kind) {
      @case ("menu") {
        <app-menu [engine]="engine()" [view]="menuView()!" />
      }
      @case ("intro") {
        <app-intro [engine]="engine()" [intro]="introView()!" [assetBase]="assetBase()" />
      }
      @case ("splash") {
        <app-splash [engine]="engine()" [splash]="splashView()!" [assetBase]="assetBase()" />
      }
      @case ("scene") {
        <app-scene [engine]="engine()" [scene]="sceneView()!" [assetBase]="assetBase()" />
      }
      @case ("combat") {
        <app-combat [engine]="engine()" [combat]="combatView()!" [assetBase]="assetBase()" />
      }
      @case ("input") {
        <app-ask-input [engine]="engine()" [inputView]="inputViewData()!" />
      }
      @case ("ending") {
        <app-ending [engine]="engine()" [ending]="endingView()!" />
      }
      @case ("error") {
        <div class="a2-boot a2-boot-error">{{ errorMessage() }}</div>
      }
    }
  `,
})
export class Game implements OnInit {
  engine = input.required<Engine>();
  assetBase = input.required<string>();

  private readonly destroyRef = inject(DestroyRef);
  protected readonly view = signal<View | null>(null);

  ngOnInit(): void {
    const engine = this.engine();
    this.view.set(engine.getView());
    const unsubscribe = engine.subscribe(() => this.view.set(engine.getView()));
    this.destroyRef.onDestroy(unsubscribe);
  }

  protected menuView() {
    const v = this.view();
    return v?.kind === "menu" ? v : null;
  }
  protected introView() {
    const v = this.view();
    return v?.kind === "intro" ? v.intro : null;
  }
  protected splashView() {
    const v = this.view();
    return v?.kind === "splash" ? v.splash : null;
  }
  protected sceneView() {
    const v = this.view();
    return v?.kind === "scene" ? v.scene : null;
  }
  protected combatView() {
    const v = this.view();
    return v?.kind === "combat" ? v.combat : null;
  }
  protected inputViewData() {
    const v = this.view();
    return v?.kind === "input" ? v.input : null;
  }
  protected endingView() {
    const v = this.view();
    return v?.kind === "ending" ? v.ending : null;
  }
  protected errorMessage() {
    const v = this.view();
    return v?.kind === "error" ? v.message : "";
  }
}
