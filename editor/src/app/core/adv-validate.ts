/**
 * Vérifications "en direct", pendant l'écriture — un sous-ensemble volontaire
 * de `a2c/analyze.py` (cibles cassées, sections mortes, fins inatteignables),
 * porté ici pour ce que la structure déjà extraite par `AdvParser` permet de
 * vérifier SANS faux positif.
 *
 * Volontairement absent (et laissé à `a2c analyze`, seul à voir tout le
 * fichier) :
 *  - objets/drapeaux "jamais utilisés/jamais posés" : le vrai analyseur
 *    regarde aussi les conditions du texte narratif conditionnel, que cet
 *    éditeur ne modélise pas (texte libre) — les reprendre ici produirait de
 *    faux "jamais utilisé" sur un flag testé uniquement dans le récit ;
 *  - bornes de caractéristique jamais satisfiables : demande un calcul de
 *    plage atteignable par relaxation sur le graphe (Bellman-Ford côté
 *    compilateur) — trop de surface pour un aller-retour fiable ici.
 */

import type { AdvDocument, AdvSection, AdvEffectLine } from './adv-parser';
import { isEffectParseable, parseEffect, type AdvEffect } from './adv-logic';

export type IssueSeverity = 'error' | 'warning';

export interface ValidationIssue {
  severity: IssueSeverity;
  message: string;
  /** Section à laquelle rattacher/depuis laquelle naviguer ; absent pour un
   * problème global (ex. aucune fin victoire atteignable). */
  sectionName?: string;
}

function safeEffect(raw: string): AdvEffect | null {
  return isEffectParseable(raw) ? parseEffect(raw) : null;
}

function gotoTargets(refs: AdvEffectLine[]): { target: string; conditional: boolean }[] {
  const out: { target: string; conditional: boolean }[] = [];
  for (const ref of refs) {
    const e = safeEffect(ref.raw);
    if (e?.op === 'goto') out.push({ target: e.name, conditional: e.cond.atoms.length > 0 });
  }
  return out;
}

/** Cibles sortantes d'une section : choix, `goto` (entrée ou de choix),
 * issues de combat/ask — même liste que `a2c/analyze.py:_edges`. */
function edges(s: AdvSection): string[] {
  const out: string[] = [];
  for (const g of gotoTargets(s.entryEffects)) out.push(g.target);
  for (const c of s.choices) {
    out.push(c.target);
    for (const g of gotoTargets(c.effects)) out.push(g.target);
  }
  if (s.combat) {
    if (s.combat.win) out.push(s.combat.win.target);
    if (s.combat.lose) out.push(s.combat.lose.target);
    if (s.combat.flee) out.push(s.combat.flee.target);
  }
  if (s.ask) {
    if (s.ask.correct) out.push(s.ask.correct.target);
    if (s.ask.wrong) out.push(s.ask.wrong.target);
  }
  return out;
}

const MAX_FLAGS = 128; // cf. a2c/model.py MAX_FLAGS
const MAX_LOCAL_FLAGS = 32; // cf. a2c/model.py MAX_LOCAL_FLAGS

export function validateDocument(doc: AdvDocument): ValidationIssue[] {
  const issues: ValidationIssue[] = [];
  const sectionNames = new Set(doc.sections.map((s) => s.name));

  // --- bornes numeriques, miroir exact de ce que le compilateur rejette ---
  // (a2c/parser.py _parse_stat/_parse_item/_parse_ask, symbols.py resolve/
  // _resolve_section) : aucune de ces bornes ne depend du graphe, donc
  // aucun faux positif possible ici, contrairement aux verifs exclues plus
  // haut.
  for (const s of doc.stats) {
    if (s.init < 0 || s.init > 255 || s.lo < 0 || s.lo > 255 || s.hi < 0 || s.hi > 255) {
      issues.push({ severity: 'error', message: `@stat ${s.name} : valeurs hors [0,255]` });
    } else if (s.lo > s.hi) {
      issues.push({ severity: 'error', message: `@stat ${s.name} : min (${s.lo}) > max (${s.hi})` });
    }
  }
  for (const it of doc.items) {
    if (it.atk < -128 || it.atk > 127 || it.dmg < -128 || it.dmg > 127 || it.armor < -128 || it.armor > 127) {
      issues.push({ severity: 'error', message: `@item ${it.name} : modificateur hors [-128,127]` });
    }
  }
  if (doc.flags.length > MAX_FLAGS) {
    issues.push({ severity: 'error', message: `${doc.flags.length} drapeaux déclarés (maximum ${MAX_FLAGS})` });
  }
  const localFlags = doc.flags.filter((f) => f.local).length;
  if (localFlags > MAX_LOCAL_FLAGS) {
    issues.push({ severity: 'error', message: `${localFlags} drapeaux 'local' (maximum ${MAX_LOCAL_FLAGS}) : en rendre quelques-uns globaux` });
  }
  for (const s of doc.sections) {
    if (s.combat) {
      const cb = s.combat;
      if (!(cb.att >= 0 && cb.att <= 255 && cb.hp >= 1 && cb.hp <= 255 && cb.dmg >= 0 && cb.dmg <= 255 && cb.armor >= 0 && cb.armor <= 255)) {
        issues.push({ severity: 'error', sectionName: s.name, message: '@combat : valeurs hors bornes (att/dmg/armor 0..255, hp 1..255)' });
      }
    }
    if (s.ask && (s.ask.maxlen < 1 || s.ask.maxlen > 40)) {
      issues.push({ severity: 'error', sectionName: s.name, message: `@ask maxlen=${s.ask.maxlen} hors bornes (1 à 40)` });
    }
  }

  const checkTarget = (target: string, where: string, sectionName: string) => {
    if (target && !sectionNames.has(target)) {
      issues.push({ severity: 'error', sectionName, message: `${where} pointe vers une section inexistante : « ${target} »` });
    }
  };

  for (const s of doc.sections) {
    for (const c of s.choices) {
      checkTarget(c.target, `Le choix « ${c.label || '(sans libellé)'} »`, s.name);
      for (const g of gotoTargets(c.effects)) checkTarget(g.target, `Un effet (goto) du choix « ${c.label || '(sans libellé)'} »`, s.name);
    }
    for (const g of gotoTargets(s.entryEffects)) checkTarget(g.target, "Un effet d'entrée (goto)", s.name);

    if (s.combat) {
      if (s.combat.win) checkTarget(s.combat.win.target, '@win', s.name);
      else issues.push({ severity: 'error', sectionName: s.name, message: '@combat sans @win (obligatoire)' });
      if (s.combat.lose) checkTarget(s.combat.lose.target, '@lose', s.name);
      else issues.push({ severity: 'error', sectionName: s.name, message: '@combat sans @lose (obligatoire)' });
      if (s.combat.flee) checkTarget(s.combat.flee.target, '@flee', s.name);
    }
    if (s.ask) {
      if (s.ask.correct) checkTarget(s.ask.correct.target, '@correct', s.name);
      else issues.push({ severity: 'error', sectionName: s.name, message: '@ask sans @correct (obligatoire)' });
      if (s.ask.wrong) checkTarget(s.ask.wrong.target, '@wrong', s.name);
      else issues.push({ severity: 'error', sectionName: s.name, message: '@ask sans @wrong (obligatoire)' });
      if (!s.ask.answers.length) issues.push({ severity: 'error', sectionName: s.name, message: '@ask sans @answer (au moins une réponse obligatoire)' });
    }
  }

  // --- reachabilité depuis @start (+ scènes d'intro) ---
  const byName = new Map(doc.sections.map((s) => [s.name, s]));
  const introNames = new Set(doc.sections.filter((s) => s.kind.isIntro).map((s) => s.name));
  const seen = new Set<string>();
  const queue: string[] = doc.start ? [doc.start, ...introNames] : [...introNames];
  while (queue.length) {
    const name = queue.shift()!;
    if (seen.has(name)) continue;
    const sec = byName.get(name);
    if (!sec) continue;
    seen.add(name);
    for (const t of edges(sec)) if (sectionNames.has(t)) queue.push(t);
  }
  for (const s of doc.sections) {
    if (!seen.has(s.name)) {
      issues.push({ severity: 'warning', sectionName: s.name, message: "Section inatteignable depuis @start (ou une scène d'intro)" });
    }
  }

  // --- culs-de-sac / aiguillages sans goto inconditionnel ---
  for (const s of doc.sections) {
    const bare = s.choices.length === 0 && !s.kind.ending && !s.combat && !s.ask && !introNames.has(s.name);
    if (!bare) continue;
    const gotos = gotoTargets(s.entryEffects);
    if (gotos.length === 0) {
      issues.push({ severity: 'error', sectionName: s.name, message: 'Cul-de-sac : aucun choix et pas de fin (@ending)' });
    } else if (gotos.every((g) => g.conditional)) {
      issues.push({
        severity: 'warning',
        sectionName: s.name,
        message: "Aiguillage sans goto inconditionnel : si aucune garde n'est vraie, le joueur reste bloqué",
      });
    }
  }

  // --- fin victoire atteignable ---
  const winReachable = doc.sections.some((s) => s.kind.ending === 'win' && seen.has(s.name));
  if (!winReachable) issues.push({ severity: 'error', message: 'Aucune fin VICTOIRE atteignable' });

  return issues;
}
