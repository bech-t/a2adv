import { Component, OnDestroy, computed, effect, inject, input, output, signal } from '@angular/core';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatChipsModule } from '@angular/material/chips';
import { MatCardModule } from '@angular/material/card';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatInputModule } from '@angular/material/input';
import {
  AdvParser,
  type AdvChoice,
  type AdvDocument,
  type AdvEffectLine,
  type AskAnswer,
  type AskInfo,
  type AskOutcome,
  type CombatInfo,
  type CombatOutcome,
} from '../../core/adv-parser';
import { CodeEditor } from '../../components/code-editor/code-editor';
import { ConditionEditor } from '../../components/condition-editor/condition-editor';
import { EffectEditor } from '../../components/effect-editor/effect-editor';
import {
  type AdvCondition,
  type AdvEffect,
  conditionText,
  isConditionParseable,
  isEffectParseable,
  newEffect,
  parseCondition,
  parseEffect,
} from '../../core/adv-logic';

function isCombatKey(k: string): k is 'win' | 'lose' | 'flee' {
  return k === 'win' || k === 'lose' || k === 'flee';
}

function isAskKey(k: string): k is 'correct' | 'wrong' {
  return k === 'correct' || k === 'wrong';
}

/** Édite le texte brut d'UNE section. Volontairement sans notion de style,
 * effet, condition ou directive — c'est du texte, l'auteur sait ce qu'il
 * écrit ; ce composant se contente de le réinjecter au bon endroit. */
@Component({
  selector: 'app-section-editor',
  imports: [
    MatButtonModule,
    MatIconModule,
    MatChipsModule,
    MatCardModule,
    MatFormFieldModule,
    MatInputModule,
    CodeEditor,
    ConditionEditor,
    EffectEditor,
  ],
  templateUrl: './section-editor.html',
  styleUrl: './section-editor.scss',
})
export class SectionEditor implements OnDestroy {
  private readonly parser = inject(AdvParser);

  readonly doc = input<AdvDocument | null>(null);
  readonly sectionName = input<string | null>(null);
  /** Porte le texte source D'AVANT le commit (pour l'historique annuler/
   * rétablir de App) : `doc` est muté EN PLACE par `commit()`, donc au
   * moment où `App` reçoit cet évènement, `doc()` reflète déjà le nouvel
   * état — impossible d'y lire l'ancien après coup, il faut le capturer
   * avant et le transporter. */
  readonly saved = output<string>();
  readonly jumpTo = output<string>();
  /** Même règle que `saved` : porte le texte D'AVANT la mutation en place
   * (retarget/relabel/effets/combat/ask...), pour que `App` puisse empiler
   * un état d'annulation correct plutôt que de relire un `doc` déjà changé. */
  readonly docTouched = output<string>();

  readonly text = signal('');
  readonly dirty = signal(false);

  readonly section = computed(() => {
    const doc = this.doc();
    const name = this.sectionName();
    return doc && name ? (doc.sections.find((s) => s.name === name) ?? null) : null;
  });

  readonly choices = computed(() => this.section()?.choices ?? []);
  readonly entryEffects = computed(() => this.section()?.entryEffects ?? []);
  readonly combat = computed(() => this.section()?.combat ?? null);
  readonly ask = computed(() => this.section()?.ask ?? null);
  readonly sectionNames = computed(() => this.doc()?.sections.map((s) => s.name) ?? []);
  readonly flagNames = computed(() => this.doc()?.flags.map((f) => f.name) ?? []);
  readonly itemNames = computed(() => this.doc()?.items.map((i) => i.name) ?? []);
  readonly statNames = computed(() => this.doc()?.stats.map((s) => s.name) ?? []);

  /** Nom de la DERNIÈRE section chargée dans `text` — sert à distinguer "on a
   * changé de section" (il faut recharger) de "le doc a changé pour une
   * autre raison pendant qu'on est sur la même section" (il ne faut SURTOUT
   * PAS écraser une frappe en cours : c'était un bug réel avant ce correctif,
   * un simple clic sur un champ structuré ailleurs dans l'écran effaçait le
   * texte narratif en train d'être tapé). */
  private lastSectionName: string | null = null;
  private commitTimer: ReturnType<typeof setTimeout> | null = null;

  constructor() {
    effect(() => {
      const doc = this.doc();
      const sec = this.section();
      const name = sec?.name ?? null;
      if (name !== this.lastSectionName) {
        this.lastSectionName = name;
        this.text.set(doc && sec ? this.parser.sectionText(doc, sec) : '');
        this.dirty.set(false);
      }
    });
  }

  ngOnDestroy(): void {
    if (this.commitTimer) clearTimeout(this.commitTimer);
  }

  onEdit(value: string): void {
    this.text.set(value);
    this.dirty.set(true);
    // Auto-commit apres une pause de frappe : le texte narratif suit la meme
    // regle que tous les autres champs (s'applique sans clic), avec une
    // marge courte plutot qu'a chaque caractere pour ne pas re-analyser tout
    // le document en continu. `commitDraft` reste dispo pour un flush
    // immediat (changement de section, export...).
    if (this.commitTimer) clearTimeout(this.commitTimer);
    this.commitTimer = setTimeout(() => this.commitDraft(), 1500);
  }

  /** Capture le texte source AVANT `fn`, l'applique, puis émet `docTouched`
   * avec ce "avant" — jamais avec `doc()` lu après coup, puisque `fn` mute
   * `doc` en place (cf. commentaire sur `docTouched`). */
  private mutate(fn: (doc: AdvDocument) => void): void {
    const doc = this.doc();
    if (!doc) return;
    const before = this.parser.serialize(doc);
    fn(doc);
    this.docTouched.emit(before);
  }

  private commit(): string | null {
    const doc = this.doc();
    const sec = this.section();
    if (!doc || !sec) return null;
    const before = this.parser.serialize(doc);
    this.parser.setSectionText(doc, sec, this.text());
    this.dirty.set(false);
    return before;
  }

  /** Flush immédiat, sans attendre le délai normal — à appeler avant de
   * changer de section, d'exporter, ou de fermer l'onglet. Renvoie true si
   * quelque chose a réellement été commis (le parent doit alors rafraîchir
   * sa référence `doc`, ce que fait `docTouched` ici même). */
  commitDraft(): boolean {
    if (this.commitTimer) {
      clearTimeout(this.commitTimer);
      this.commitTimer = null;
    }
    if (!this.dirty()) return false;
    const before = this.commit();
    if (before !== null) this.docTouched.emit(before);
    return before !== null;
  }

  save(): void {
    if (this.commitTimer) {
      clearTimeout(this.commitTimer);
      this.commitTimer = null;
    }
    const before = this.commit();
    if (before !== null) this.saved.emit(before);
  }

  /** Condition d'un choix, structurée — `null` si le texte entre `{}` n'est
   * pas reconnu par la grammaire (auteur ayant tapé autre chose à la main) :
   * le gabarit retombe alors sur un champ texte brut plutôt que de deviner. */
  choiceCondition(choice: AdvChoice): AdvCondition | null {
    const src = choice.cond ?? '';
    return isConditionParseable(src) ? parseCondition(src) : null;
  }

  setChoiceLabel(choice: AdvChoice, label: string): void {
    this.mutate((doc) => this.parser.relabel(doc, choice, label));
  }

  setChoiceTarget(choice: AdvChoice, target: string): void {
    this.mutate((doc) => this.parser.retarget(doc, choice, target));
  }

  setChoiceCondition(choice: AdvChoice, cond: AdvCondition): void {
    this.mutate((doc) => this.parser.setChoiceCond(doc, choice, conditionText(cond)));
  }

  setChoiceCondText(choice: AdvChoice, raw: string): void {
    this.mutate((doc) => this.parser.setChoiceCond(doc, choice, raw));
  }

  /** Effet structuré — `null` si sa syntaxe n'est pas reconnue (repli texte
   * brut), même principe que `choiceCondition`. */
  effectFor(ref: AdvEffectLine): AdvEffect | null {
    return isEffectParseable(ref.raw) ? parseEffect(ref.raw) : null;
  }

  setEffect(ref: AdvEffectLine, effect: AdvEffect): void {
    this.mutate((doc) => this.parser.updateEffectLine(doc, ref, effect));
  }

  setEffectRaw(ref: AdvEffectLine, raw: string): void {
    this.mutate((doc) => this.parser.setEffectRaw(doc, ref, raw));
  }

  removeEffect(ref: AdvEffectLine): void {
    this.mutate((doc) => this.parser.removeEffectLine(doc, ref));
  }

  addEntryEffect(): void {
    const sec = this.section();
    if (!sec) return;
    this.mutate((doc) => this.parser.addEntryEffect(doc, sec, newEffect()));
  }

  addChoiceEffect(choice: AdvChoice): void {
    this.mutate((doc) => this.parser.addChoiceEffect(doc, choice, newEffect()));
  }

  toNum(raw: string): number {
    const n = Number(raw);
    return Number.isFinite(n) ? n : 0;
  }

  setCombat(patch: Partial<Pick<CombatInfo, 'name' | 'att' | 'hp' | 'dmg' | 'armor' | 'image'>>): void {
    const combat = this.combat();
    if (!combat) return;
    this.mutate((doc) => this.parser.updateCombat(doc, combat, patch));
  }

  setCombatOutcome(outcome: CombatOutcome, patch: Partial<Pick<CombatOutcome, 'target' | 'msg'>>): void {
    this.mutate((doc) => this.parser.updateCombatOutcome(doc, outcome, patch));
  }

  setAsk(patch: Partial<Pick<AskInfo, 'prompt' | 'maxlen'>>): void {
    const ask = this.ask();
    if (!ask) return;
    this.mutate((doc) => this.parser.updateAsk(doc, ask, patch));
  }

  setAskOutcome(outcome: AskOutcome, target: string): void {
    this.mutate((doc) => this.parser.updateAskOutcome(doc, outcome, target));
  }

  setAnswer(answer: AskAnswer, text: string): void {
    this.mutate((doc) => this.parser.updateAnswer(doc, answer, text));
  }

  addAnswer(): void {
    const ask = this.ask();
    if (!ask) return;
    this.mutate((doc) => this.parser.addAnswer(doc, ask));
  }

  removeAnswer(answer: AskAnswer): void {
    this.mutate((doc) => this.parser.removeAnswer(doc, answer));
  }

  addOutcomeEffect(outcome: CombatOutcome | AskOutcome): void {
    this.mutate((doc) => this.parser.addOutcomeEffect(doc, outcome, newEffect()));
  }

  addCombat(): void {
    const sec = this.section();
    if (!sec) return;
    this.mutate((doc) => this.parser.addCombat(doc, sec));
  }

  removeCombat(): void {
    const sec = this.section();
    if (!sec) return;
    this.mutate((doc) => this.parser.removeCombat(doc, sec));
  }

  // `key` en string : les paires { key, title, outcome } du gabarit sont un
  // tableau littéral, TS n'y infère pas le littéral 'win'|'lose'|'flee' --
  // revérifié ici plutôt que d'imposer `as const` dans le template.
  addCombatOutcome(key: string): void {
    const combat = this.combat();
    if (!combat || !isCombatKey(key)) return;
    this.mutate((doc) => this.parser.addCombatOutcome(doc, combat, key));
  }

  removeCombatOutcome(key: string): void {
    const combat = this.combat();
    if (!combat || !isCombatKey(key)) return;
    this.mutate((doc) => this.parser.removeCombatOutcome(doc, combat, key));
  }

  addAsk(): void {
    const sec = this.section();
    if (!sec) return;
    this.mutate((doc) => this.parser.addAsk(doc, sec));
  }

  removeAsk(): void {
    const sec = this.section();
    if (!sec) return;
    this.mutate((doc) => this.parser.removeAsk(doc, sec));
  }

  addAskOutcome(key: string): void {
    const ask = this.ask();
    if (!ask || !isAskKey(key)) return;
    this.mutate((doc) => this.parser.addAskOutcome(doc, ask, key));
  }

  removeAskOutcome(key: string): void {
    const ask = this.ask();
    if (!ask || !isAskKey(key)) return;
    this.mutate((doc) => this.parser.removeAskOutcome(doc, ask, key));
  }
}
