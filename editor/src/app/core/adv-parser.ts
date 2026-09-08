import { Injectable } from '@angular/core';
import { effectLine, type AdvEffect } from './adv-logic';

/**
 * Un choix extrait d'une section : `* {cond} [label] -> cible`.
 * `lineIndex` pointe dans `AdvDocument.lines` — modifier ce choix se fait en
 * réécrivant CETTE ligne précise, jamais en reconstruisant le texte.
 */
export interface AdvChoice {
  lineIndex: number;
  cond: string | null;
  label: string;
  target: string;
  /** Les `~ ...` qui suivent CE choix (avant le prochain choix/directive
   * d'attache) — appliqués si ce choix est pris. Texte brut, sans le `~` ni
   * un éventuel commentaire de fin de ligne : la grammaire d'effet (13
   * verbes) vit dans adv-logic.ts, pas ici — cf. `AdvSection.entryEffects`. */
  effects: AdvEffectLine[];
}

/** Un effet `~ ...` repéré par sa ligne, texte brut (sans le `~`). Comme
 * `AdvChoice.cond`, volontairement pas parsé ici : `parseEffect`/
 * `isEffectParseable` (adv-logic.ts) le font à la demande, avec repli sur du
 * texte brut si la syntaxe n'est pas reconnue. */
export interface AdvEffectLine {
  lineIndex: number;
  raw: string;
}

/** Une issue de combat (`@win`/`@lose`/`@flee`) : cible + message d'issue
 * optionnel + ses propres effets (les `~` indentés juste après). */
export interface CombatOutcome {
  line: number;
  target: string;
  msg: string;
  effects: AdvEffectLine[];
}

/** `@combat "Nom" att=N hp=N dmg=N armor=N [image=id]`. `win`/`lose` sont
 * obligatoires côté compilateur, `flee` optionnelle — mais ce parseur reste
 * volontairement tolérant : `null` si absente, sans erreur. */
export interface CombatInfo {
  line: number;
  name: string;
  att: number;
  hp: number;
  dmg: number;
  armor: number;
  image: string | null;
  win: CombatOutcome | null;
  lose: CombatOutcome | null;
  flee: CombatOutcome | null;
}

/** Une issue d'énigme (`@correct`/`@wrong`) : cible + ses propres effets. */
export interface AskOutcome {
  line: number;
  target: string;
  effects: AdvEffectLine[];
}

/** Une réponse acceptée (`@answer ...`), une par ligne. */
export interface AskAnswer {
  lineIndex: number;
  text: string;
}

/** `@ask "invite" [maxlen=N]` + ses `@answer` et ses issues. */
export interface AskInfo {
  line: number;
  prompt: string;
  maxlen: number;
  answers: AskAnswer[];
  correct: AskOutcome | null;
  wrong: AskOutcome | null;
}

/** Signaux "quel genre de section" — pour l'icône dans la hiérarchie, pas
 * pour la validation (plusieurs peuvent être vrais à la fois : un combat
 * peut aussi porter une image). */
export interface SectionKind {
  isStart: boolean;
  isIntro: boolean;
  ending: 'win' | 'lose' | null;
  hasCombat: boolean;
  hasAsk: boolean;
  hasImage: boolean;
}

/** Une section `:: nom` — ses lignes brutes (comprises), plus ses choix. */
export interface AdvSection {
  name: string;
  startLine: number;
  endLine: number; // exclusif
  choices: AdvChoice[];
  /** Effets d'entrée : les `~` avant le 1er choix (implicite), ou après un
   * `@on_enter` explicite (cf. docs/GUIDE-FORMAT-ADV.md §7). N'inclut PAS les
   * `~` attachés à `@on_exit` — ceux-là restent du texte brut non modélisé. */
  entryEffects: AdvEffectLine[];
  /** `null` si la section n'a pas de `@combat`. */
  combat: CombatInfo | null;
  /** `null` si la section n'a pas de `@ask`. */
  ask: AskInfo | null;
  kind: SectionKind;
}

/** Un chapitre = les sections entre deux `@chapter` (ou avant la 1re). */
export interface AdvChapter {
  title: string; // "" pour le prologue avant le premier @chapter
  headerLine: number | null; // ligne du `@chapter "..."` ; null pour le prologue
  sections: AdvSection[];
}

export interface StatDecl {
  line: number;
  name: string;
  init: number;
  lo: number;
  hi: number;
  hidden: boolean;
}

export interface FlagDecl {
  line: number;
  name: string;
  on: boolean;
  local: boolean;
}

export interface ItemDecl {
  line: number;
  name: string;
  label: string;
  on: boolean;
  atk: number;
  dmg: number;
  armor: number;
}

export interface AdvDocument {
  title: string;
  author: string;
  start: string;
  lines: string[];
  sections: AdvSection[];
  chapters: AdvChapter[];
  stats: StatDecl[];
  flags: FlagDecl[];
  items: ItemDecl[];
  /** Ligne du premier `::`, ou lines.length s'il n'y en a aucune : borne
   * de fin pour toute insertion dans le préambule. */
  preambleEnd: number;
}

// Reprend la regex du vrai compilateur (a2c/parser.py:_CHOICE_RE) :
// une etoile suivie d'un espace, une condition optionnelle entre accolades,
// un libelle entre crochets, puis "-> cible".
const CHOICE_RE = /^\*\s*(?:\{([^}]*)\})?\s*\[([^\]]*)\]\s*->\s*([A-Za-z_][A-Za-z0-9_]*)\s*$/;
const SECTION_RE = /^::\s*([A-Za-z_][A-Za-z0-9_]*)/;

/**
 * Parseur "structurel" volontairement minimal : il ne comprend que ce dont
 * l'arbre de scènes et l'export ont besoin (sections, choix). Tout le reste
 * (texte, effets, directives) reste du texte brut, réémis tel quel — un
 * aller-retour sans édition est donc TOUJOURS identique au fichier d'origine.
 *
 * Ce n'est pas le compilateur : aucune validation, aucune résolution de
 * conditions. Pour tout contrôle réel (sections mortes, bornes de stats...),
 * `a2c`/`a2c.analyze` restent la référence.
 */
@Injectable({ providedIn: 'root' })
export class AdvParser {
  parse(text: string): AdvDocument {
    const lines = text.split('\n');
    const sections: AdvSection[] = [];
    const chapters: AdvChapter[] = [{ title: '', headerLine: null, sections: [] }];
    const stats: StatDecl[] = [];
    const flags: FlagDecl[] = [];
    const items: ItemDecl[] = [];
    const introNames = new Set<string>();
    let title = '';
    let author = '';
    let start = '';
    let preambleEnd = lines.length;
    let current: AdvSection | null = null;
    // Cible des '~' qui suivent : entryEffects (implicite en entrant dans une
    // section, ou après un @on_enter explicite), effets d'un choix ou d'une
    // issue de combat/ask (win/lose/flee/correct/wrong), ou null (attaché à
    // @on_exit — pas modélisé ici, cf. AdvSection.entryEffects). Miroir de
    // l'"attache" séquentielle du vrai compilateur (a2c/parser.py:77).
    let attach: AdvEffectLine[] | null = null;
    const OTHER_ANCHORS = new Set(['on_exit']);

    for (let i = 0; i < lines.length; i++) {
      const raw = lines[i];
      const trimmed = raw.trim();

      if (!current) {
        const t = trimmed.match(/^@title\s+(.*)$/);
        if (t) title = this.stripComment(t[1]).trim();
        const au = trimmed.match(/^@author\s+(.*)$/);
        if (au) author = this.stripComment(au[1]).trim();
        const s = trimmed.match(/^@start\s+(\S+)/);
        if (s) start = s[1];
        const intro = trimmed.match(/^@intro\s+(.*)$/);
        if (intro) for (const n of this.stripComment(intro[1]).trim().split(/\s+/)) introNames.add(n);

        const stat = trimmed.match(/^@stat\s+(.*)$/);
        if (stat) {
          const parsed = this.parseStatLine(this.stripComment(stat[1]));
          if (parsed) stats.push({ line: i, ...parsed });
        }
        const flag = trimmed.match(/^@flag\s+(.*)$/);
        if (flag) {
          const parsed = this.parseFlagLine(this.stripComment(flag[1]));
          if (parsed) flags.push({ line: i, ...parsed });
        }
        const item = trimmed.match(/^@item\s+(.*)$/);
        if (item) {
          const parsed = this.parseItemLine(this.stripComment(item[1]));
          if (parsed) items.push({ line: i, ...parsed });
        }
      }

      const chap = trimmed.match(/^@chapter\b(.*)$/);
      if (chap) {
        // Une section finit AUSSI a la frontiere de chapitre suivante, pas
        // seulement au prochain '::' — sinon son intervalle de lignes avale
        // le `@chapter` (et la 1re section du chapitre suivant se retrouve
        // rattachee au mauvais chapitre ; suppression = frontiere emportee
        // avec la section, en silence).
        if (current) {
          current.endLine = i;
          current = null;
          attach = null;
        }
        const q = this.stripComment(chap[1]).match(/"([^"]*)"/);
        chapters.push({ title: q ? q[1] : '', headerLine: i, sections: [] });
        continue;
      }

      const sec = trimmed.match(SECTION_RE);
      if (sec) {
        if (current) current.endLine = i;
        else preambleEnd = i;
        current = {
          name: sec[1],
          startLine: i,
          endLine: lines.length,
          choices: [],
          entryEffects: [],
          combat: null,
          ask: null,
          kind: { isStart: false, isIntro: false, ending: null, hasCombat: false, hasAsk: false, hasImage: false },
        };
        sections.push(current);
        chapters[chapters.length - 1].sections.push(current);
        attach = current.entryEffects; // par défaut, les '~' avant le 1er choix sont des effets d'entrée
        continue;
      }

      if (current && trimmed.startsWith('@')) {
        const stripped = this.stripComment(trimmed).slice(1); // sans '@', sans commentaire
        const key = stripped.match(/^[A-Za-z_]+/)?.[0] ?? '';
        const rest = stripped.slice(key.length).trim();

        if (key === 'on_enter') {
          attach = current.entryEffects;
        } else if (key === 'combat') {
          current.combat = this.parseCombatLine(rest, i);
        } else if (key === 'win' || key === 'lose' || key === 'flee') {
          const outcome = current.combat ? this.parseOutcomeLine(rest, i) : null;
          if (current.combat && outcome) {
            if (key === 'win') current.combat.win = outcome;
            else if (key === 'lose') current.combat.lose = outcome;
            else current.combat.flee = outcome;
            attach = outcome.effects;
          } else {
            attach = null;
          }
        } else if (key === 'ask') {
          current.ask = this.parseAskLine(rest, i);
        } else if (key === 'answer' && current.ask) {
          current.ask.answers.push({ lineIndex: i, text: this.parseAnswerLine(rest) });
        } else if (key === 'correct' || key === 'wrong') {
          const target = rest.split(/\s+/)[0] || '';
          if (current.ask && target) {
            const outcome: AskOutcome = { line: i, target, effects: [] };
            if (key === 'correct') current.ask.correct = outcome;
            else current.ask.wrong = outcome;
            attach = outcome.effects;
          } else {
            attach = null;
          }
        } else if (OTHER_ANCHORS.has(key)) {
          attach = null;
        }
        // les autres directives (@mode, @image, @ending...) ne changent pas
        // l'attache courante — même règle que le compilateur.
      }

      if (current && trimmed.startsWith('*') && (trimmed.length === 1 || trimmed[1] === ' ')) {
        const m = this.stripComment(trimmed).match(CHOICE_RE);
        if (m) {
          const choice: AdvChoice = {
            lineIndex: i,
            cond: m[1] ?? null,
            label: m[2].trim(),
            target: m[3],
            effects: [],
          };
          current.choices.push(choice);
          attach = choice.effects;
        }
        continue;
      }

      if (current && trimmed.startsWith('~')) {
        if (attach) attach.push({ lineIndex: i, raw: this.stripComment(trimmed).slice(1).trim() });
        continue;
      }
    }

    for (const s of sections) {
      s.kind = this.computeKind(lines, s, s.name === start, introNames.has(s.name));
    }

    return { title, author, start, lines, sections, chapters, stats, flags, items, preambleEnd };
  }

  // --- Analyse d'une ligne @stat/@flag/@item (miroir de a2c/parser.py) ----

  private parseStatLine(body: string): Omit<StatDecl, 'line'> | null {
    const args = body.trim().split(/\s+/).filter(Boolean);
    let hidden = false;
    if (args.length && args[args.length - 1] === 'hidden') {
      hidden = true;
      args.pop();
    }
    if (args.length !== 2 && args.length !== 4) return null;
    const name = args[0];
    const init = Number(args[1]);
    const lo = args.length === 4 ? Number(args[2]) : 0;
    const hi = args.length === 4 ? Number(args[3]) : 255;
    if (!name || Number.isNaN(init) || Number.isNaN(lo) || Number.isNaN(hi)) return null;
    return { name, init, lo, hi, hidden };
  }

  private parseFlagLine(body: string): Omit<FlagDecl, 'line'> | null {
    const args = body.trim().split(/\s+/).filter(Boolean);
    let local = false;
    if (args.length && args[args.length - 1] === 'local') {
      local = true;
      args.pop();
    }
    if (args.length !== 1 && args.length !== 2) return null;
    const name = args[0];
    const on = args[1] === 'on';
    if (!name) return null;
    return { name, on, local };
  }

  private parseItemLine(body: string): Omit<ItemDecl, 'line'> | null {
    const args = body.trim().split(/\s+/).filter(Boolean);
    if (!args.length) return null;
    const name = args[0];
    const qm = body.match(/"([^"]*)"/);
    const label = qm ? qm[1] : name;
    let on = false;
    let atk = 0;
    let dmg = 0;
    let armor = 0;
    // tokens hors id et libellé entre guillemets
    const tail = body.slice(body.indexOf(name) + name.length).replace(/"[^"]*"/, '');
    for (const tok of tail.trim().split(/\s+/).filter(Boolean)) {
      if (tok === 'on') on = true;
      else if (tok === 'off') on = false;
      else if (tok.startsWith('atk=')) atk = Number(tok.slice(4)) || 0;
      else if (tok.startsWith('dmg=')) dmg = Number(tok.slice(4)) || 0;
      else if (tok.startsWith('armor=')) armor = Number(tok.slice(6)) || 0;
    }
    return { name, label, on, atk, dmg, armor };
  }

  // --- Analyse combat/ask (miroir de a2c/parser.py _parse_combat/_parse_ask) -

  /** `body` = texte après `@combat` (sans le mot-clé). */
  private parseCombatLine(body: string, line: number): CombatInfo | null {
    const m = body.match(/"([^"]*)"/);
    if (!m || m.index === undefined) return null;
    const info: CombatInfo = { line, name: m[1], att: 0, hp: 0, dmg: 0, armor: 0, image: null, win: null, lose: null, flee: null };
    for (const tok of body.slice(m.index + m[0].length).trim().split(/\s+/).filter(Boolean)) {
      const eq = tok.indexOf('=');
      if (eq < 0) continue;
      const key2 = tok.slice(0, eq);
      const val = tok.slice(eq + 1);
      if (key2 === 'image') info.image = val;
      else if (key2 === 'att') info.att = Number(val) || 0;
      else if (key2 === 'hp') info.hp = Number(val) || 0;
      else if (key2 === 'dmg') info.dmg = Number(val) || 0;
      else if (key2 === 'armor') info.armor = Number(val) || 0;
    }
    return info;
  }

  /** `body` = texte après `@win`/`@lose`/`@flee` : `cible ["message"]`. */
  private parseOutcomeLine(body: string, line: number): CombatOutcome | null {
    const mq = body.match(/"([^"]*)"/);
    const msg = mq ? mq[1] : '';
    const head = mq && mq.index !== undefined ? body.slice(0, mq.index) : body;
    const targs = head.trim().split(/\s+/).filter(Boolean);
    return targs.length === 1 ? { line, target: targs[0], msg, effects: [] } : null;
  }

  /** `body` = texte après `@ask` (sans le mot-clé). */
  private parseAskLine(body: string, line: number): AskInfo | null {
    const m = body.match(/"([^"]*)"/);
    if (!m || m.index === undefined) return null;
    const info: AskInfo = { line, prompt: m[1], maxlen: 20, answers: [], correct: null, wrong: null };
    for (const tok of body.slice(m.index + m[0].length).trim().split(/\s+/).filter(Boolean)) {
      if (tok.startsWith('maxlen=')) info.maxlen = Number(tok.slice(7)) || 20;
    }
    return info;
  }

  /** `body` = texte après `@answer` : avec ou sans guillemets, équivalents. */
  private parseAnswerLine(body: string): string {
    const m = body.match(/"([^"]*)"/);
    return m ? m[1] : body.trim();
  }

  // --- Sérialisation d'une déclaration vers sa ligne .adv -----------------

  private statLine(s: Omit<StatDecl, 'line'>): string {
    return `@stat ${s.name} ${s.init} ${s.lo} ${s.hi}${s.hidden ? ' hidden' : ''}`;
  }

  private flagLine(f: Omit<FlagDecl, 'line'>): string {
    return `@flag ${f.name}${f.on ? ' on' : ''}${f.local ? ' local' : ''}`;
  }

  private itemLine(it: Omit<ItemDecl, 'line'>): string {
    const parts = [`@item ${it.name}`, `"${it.label}"`];
    if (it.on) parts.push('on');
    if (it.atk) parts.push(`atk=${it.atk}`);
    if (it.dmg) parts.push(`dmg=${it.dmg}`);
    if (it.armor) parts.push(`armor=${it.armor}`);
    return parts.join(' ');
  }

  // --- Mutations structurelles (ajout/suppression) -------------------------
  //
  // Contrairement a `setSectionText`/`retarget` (appelees a chaque frappe,
  // ou le decalage incremental des index vaut la peine), ces operations sont
  // rares (un clic). Le plus SUR est de modifier les lignes brutes puis de
  // RE-ANALYSER tout le document — aucun index a decaler a la main, donc
  // aucune classe de bug a ce sujet (cf. la vraie collision memoire trouvee
  // dans le player Apple II cette meme session : jamais deux fois la
  // meme logique de decalage, ecrite a la main, a deux endroits).

  private reparseAfterSplice(lines: string[]): AdvDocument {
    return this.parse(lines.join('\n'));
  }

  private uniqueName(base: string, existing: Set<string>): string {
    if (!existing.has(base)) return base;
    let i = 2;
    while (existing.has(`${base}_${i}`)) i++;
    return `${base}_${i}`;
  }

  private computeKind(lines: string[], section: AdvSection, isStart: boolean, isIntro: boolean): SectionKind {
    const kind: SectionKind = { isStart, isIntro, ending: null, hasCombat: false, hasAsk: false, hasImage: false };
    for (let i = section.startLine; i < section.endLine; i++) {
      const t = this.stripComment(lines[i].trim());
      if (/^@ending\s+win\b/.test(t)) kind.ending = 'win';
      else if (/^@ending\s+lose\b/.test(t)) kind.ending = 'lose';
      else if (/^@combat\b/.test(t)) kind.hasCombat = true;
      else if (/^@ask\b/.test(t)) kind.hasAsk = true;
      else if (/^@mode\s+(image_text|full_image)/.test(t)) kind.hasImage = true;
    }
    return kind;
  }

  /** Reconstruit le texte source. Sans édition depuis `parse()`, c'est un
   * aller-retour identique octet pour octet. */
  serialize(doc: AdvDocument): string {
    return doc.lines.join('\n');
  }

  /** Change la cible d'un choix, en ne touchant QUE le `-> ancienne_cible`
   * de sa ligne — le reste de la ligne (condition, libellé) est préservé. */
  retarget(doc: AdvDocument, choice: AdvChoice, newTarget: string): void {
    const line = doc.lines[choice.lineIndex];
    const updated = line.replace(/->\s*[A-Za-z_][A-Za-z0-9_]*\s*$/, `-> ${newTarget}`);
    doc.lines[choice.lineIndex] = updated;
    choice.target = newTarget;
  }

  /** Change le libellé d'un choix, en ne touchant QUE le texte entre crochets
   * — même logique chirurgicale que `retarget` (condition et commentaire de
   * fin de ligne éventuels préservés). */
  relabel(doc: AdvDocument, choice: AdvChoice, label: string): void {
    const line = doc.lines[choice.lineIndex];
    doc.lines[choice.lineIndex] = line.replace(/\[[^\]]*\]/, `[${label}]`);
    choice.label = label;
  }

  /** Change la condition d'un choix. `condText` est le texte SANS les
   * accolades (cf. `conditionText`/`parseCondition` dans adv-logic.ts) ;
   * chaîne vide = retire la condition. Ajoute ou retire les `{...}` selon le
   * cas, sans toucher au reste de la ligne. */
  setChoiceCond(doc: AdvDocument, choice: AdvChoice, condText: string): void {
    const line = doc.lines[choice.lineIndex];
    const hasBraces = /\{[^}]*\}/.test(line);
    let updated: string;
    if (hasBraces) {
      updated = condText ? line.replace(/\{[^}]*\}/, `{${condText}}`) : line.replace(/\{[^}]*\}\s*/, '');
    } else if (condText) {
      updated = line.replace(/^(\s*\*\s*)/, `$1{${condText}} `);
    } else {
      updated = line;
    }
    doc.lines[choice.lineIndex] = updated;
    choice.cond = condText || null;
  }

  // --- Effets (~) -----------------------------------------------------------
  //
  // Éditer un effet EXISTANT (verbe/nom/valeur/garde) réécrit sa ligne en
  // place, comme retarget/relabel/setChoiceCond — sans changer le nombre de
  // lignes, aucune classe de bug de décalage à écrire à la main. En ajouter
  // ou en retirer un, en revanche, DOIT décaler tout ce qui suit (autres
  // sections, choix, effets) : on retombe alors sur le "clic rare -> on
  // re-analyse tout" (cf. addStat et suivants), mais en gardant la MEME
  // référence `doc` — `reparseInPlace` recopie juste les champs recalculés
  // dessus, pour rester compatible avec le chemin "mutation en place" déjà
  // utilisé pour les choix (un seul évènement `docTouched`, jamais un doc de
  // rechange à faire remonter).

  private reparseInPlace(doc: AdvDocument): void {
    Object.assign(doc, this.parse(doc.lines.join('\n')));
  }

  /** Le `# commentaire` de fin de ligne, s'il y en a un (guillemets exclus),
   * `#` compris — pour le préserver quand une ligne d'effet est réécrite. */
  private trailingComment(line: string): string {
    let inStr = false;
    for (let i = 0; i < line.length; i++) {
      const ch = line[i];
      if (ch === '"') inStr = !inStr;
      if (ch === '#' && !inStr) return line.slice(i);
    }
    return '';
  }

  /** Réécrit intégralement la ligne d'un effet existant (verbe, nom, valeur,
   * garde) — contrairement aux choix, un effet EST toute sa ligne, une
   * reconstruction complète ne perd donc rien d'autre qu'un commentaire de
   * fin de ligne, explicitement préservé ici. */
  updateEffectLine(doc: AdvDocument, ref: AdvEffectLine, effect: AdvEffect): void {
    const comment = this.trailingComment(doc.lines[ref.lineIndex]);
    const body = effectLine(effect); // "~ ..."
    doc.lines[ref.lineIndex] = comment ? `${body}  ${comment}` : body;
    ref.raw = body.slice(2);
  }

  /** Repli texte brut, pour un effet dont la syntaxe n'est pas reconnue par
   * `parseEffect` (cf. `isEffectParseable`) : réécrit `raw` tel quel, sans
   * passer par la grammaire d'`AdvEffect`. */
  setEffectRaw(doc: AdvDocument, ref: AdvEffectLine, raw: string): void {
    const comment = this.trailingComment(doc.lines[ref.lineIndex]);
    const body = `~ ${raw}`;
    doc.lines[ref.lineIndex] = comment ? `${body}  ${comment}` : body;
    ref.raw = raw;
  }

  /** Ajoute un effet d'entrée en fin de liste (après le dernier existant, ou
   * juste après `:: nom` s'il n'y en a aucun). */
  addEntryEffect(doc: AdvDocument, section: AdvSection, effect: AdvEffect): void {
    const at = section.entryEffects.length
      ? section.entryEffects[section.entryEffects.length - 1].lineIndex + 1
      : section.startLine + 1;
    doc.lines.splice(at, 0, effectLine(effect));
    this.reparseInPlace(doc);
  }

  /** Ajoute un effet à un choix (appliqué si ce choix est pris), en fin de
   * sa propre liste. */
  addChoiceEffect(doc: AdvDocument, choice: AdvChoice, effect: AdvEffect): void {
    const at = choice.effects.length ? choice.effects[choice.effects.length - 1].lineIndex + 1 : choice.lineIndex + 1;
    doc.lines.splice(at, 0, effectLine(effect));
    this.reparseInPlace(doc);
  }

  /** Ajoute un effet à une issue de combat/ask (win/lose/flee/correct/wrong :
   * même forme `{line, effects}`), en fin de sa propre liste. */
  addOutcomeEffect(doc: AdvDocument, outcome: { line: number; effects: AdvEffectLine[] }, effect: AdvEffect): void {
    const at = outcome.effects.length ? outcome.effects[outcome.effects.length - 1].lineIndex + 1 : outcome.line + 1;
    doc.lines.splice(at, 0, effectLine(effect));
    this.reparseInPlace(doc);
  }

  /** Retire un effet (d'entrée ou de choix, peu importe : sa ligne suffit). */
  removeEffectLine(doc: AdvDocument, ref: AdvEffectLine): void {
    doc.lines.splice(ref.lineIndex, 1);
    this.reparseInPlace(doc);
  }

  // --- Combat (@combat/@win/@lose/@flee) -------------------------------------
  //
  // Chaque directive tient sur UNE ligne : comme pour les effets, l'éditer
  // réécrit la ligne en place (pas de reparse, pas de décalage). Ajouter ou
  // retirer tout un bloc (@combat, une issue) change le nombre de lignes ->
  // reparse, comme les effets.

  /** Nom de section syntaxiquement valide mais garanti absent : cible de
   * secours pour un `@win`/`@lose`/.../`@correct` fraîchement ajouté, pour
   * que le validateur le signale (cible inconnue) plutôt qu'il ne reste
   * invisible avec une cible vide (que ce parseur ne reconnaîtrait même
   * pas : `parseOutcomeLine` exige exactement un token). */
  private static readonly PLACEHOLDER_TARGET = 'A_COMPLETER';

  /** Transforme une section ordinaire en section de combat : ajoute
   * `@combat`, `@win` et `@lose` (les deux obligatoires côté compilateur)
   * juste après les effets d'entrée déjà présents — jamais avant, pour ne
   * pas leur voler leur attache (cf. `reparseInPlace`). Sans effet si la
   * section a déjà un `@combat`. */
  addCombat(doc: AdvDocument, section: AdvSection): void {
    if (section.combat) return;
    const at = section.entryEffects.length
      ? section.entryEffects[section.entryEffects.length - 1].lineIndex + 1
      : section.startLine + 1;
    const t = AdvParser.PLACEHOLDER_TARGET;
    doc.lines.splice(at, 0, '@combat "Adversaire" att=6 hp=6 dmg=2 armor=0', `@win ${t}`, `@lose ${t}`);
    this.reparseInPlace(doc);
  }

  /** Retire `@combat` et tout ce qui en dépend (issues + leurs effets). */
  removeCombat(doc: AdvDocument, section: AdvSection): void {
    const cb = section.combat;
    if (!cb) return;
    const lines = [cb.line, cb.win?.line, cb.lose?.line, cb.flee?.line]
      .concat(cb.win?.effects.map((e) => e.lineIndex) ?? [])
      .concat(cb.lose?.effects.map((e) => e.lineIndex) ?? [])
      .concat(cb.flee?.effects.map((e) => e.lineIndex) ?? [])
      .filter((n): n is number => n !== undefined)
      .sort((a, b) => b - a); // decroissant : jamais decaler un index pas encore traite
    for (const l of lines) doc.lines.splice(l, 1);
    this.reparseInPlace(doc);
  }

  /** Ajoute l'issue manquante (`@win`/`@lose`/`@flee`), cible de secours a
   * completer. Sans effet si elle existe deja. */
  addCombatOutcome(doc: AdvDocument, combat: CombatInfo, key: 'win' | 'lose' | 'flee'): void {
    if (combat[key]) return;
    doc.lines.splice(combat.line + 1, 0, `@${key} ${AdvParser.PLACEHOLDER_TARGET}`);
    this.reparseInPlace(doc);
  }

  /** Retire une issue de combat (et ses effets). Autorisé même pour win/lose
   * (obligatoires côté compilateur) : ce parseur reste tolérant, c'est
   * l'onglet Problèmes qui signale l'incohérence resultante. */
  removeCombatOutcome(doc: AdvDocument, combat: CombatInfo, key: 'win' | 'lose' | 'flee'): void {
    const outcome = combat[key];
    if (!outcome) return;
    const lines = [outcome.line, ...outcome.effects.map((e) => e.lineIndex)].sort((a, b) => b - a);
    for (const l of lines) doc.lines.splice(l, 1);
    this.reparseInPlace(doc);
  }

  private combatLine(c: Pick<CombatInfo, 'name' | 'att' | 'hp' | 'dmg' | 'armor' | 'image'>): string {
    const parts = [`@combat "${c.name}"`, `att=${c.att}`, `hp=${c.hp}`, `dmg=${c.dmg}`, `armor=${c.armor}`];
    if (c.image) parts.push(`image=${c.image}`);
    return parts.join(' ');
  }

  updateCombat(doc: AdvDocument, combat: CombatInfo, patch: Partial<Pick<CombatInfo, 'name' | 'att' | 'hp' | 'dmg' | 'armor' | 'image'>>): void {
    const merged = { ...combat, ...patch };
    const comment = this.trailingComment(doc.lines[combat.line]);
    const body = this.combatLine(merged);
    doc.lines[combat.line] = comment ? `${body}  ${comment}` : body;
    Object.assign(combat, patch);
  }

  private outcomeLine(key: 'win' | 'lose' | 'flee' | 'correct' | 'wrong', target: string, msg?: string): string {
    return msg ? `@${key} ${target} "${msg}"` : `@${key} ${target}`;
  }

  updateCombatOutcome(doc: AdvDocument, outcome: CombatOutcome, patch: Partial<Pick<CombatOutcome, 'target' | 'msg'>>): void {
    const merged = { ...outcome, ...patch };
    const key = doc.lines[outcome.line].trim().match(/^@(win|lose|flee)\b/)?.[1] as 'win' | 'lose' | 'flee' | undefined;
    if (!key) return;
    const comment = this.trailingComment(doc.lines[outcome.line]);
    const body = this.outcomeLine(key, merged.target, merged.msg);
    doc.lines[outcome.line] = comment ? `${body}  ${comment}` : body;
    Object.assign(outcome, patch);
  }

  // --- Ask (@ask/@answer/@correct/@wrong) ------------------------------------

  private askLine(a: Pick<AskInfo, 'prompt' | 'maxlen'>): string {
    return a.maxlen !== 20 ? `@ask "${a.prompt}" maxlen=${a.maxlen}` : `@ask "${a.prompt}"`;
  }

  updateAsk(doc: AdvDocument, ask: AskInfo, patch: Partial<Pick<AskInfo, 'prompt' | 'maxlen'>>): void {
    const merged = { ...ask, ...patch };
    const comment = this.trailingComment(doc.lines[ask.line]);
    const body = this.askLine(merged);
    doc.lines[ask.line] = comment ? `${body}  ${comment}` : body;
    Object.assign(ask, patch);
  }

  updateAskOutcome(doc: AdvDocument, outcome: AskOutcome, target: string): void {
    const key = doc.lines[outcome.line].trim().match(/^@(correct|wrong)\b/)?.[1] as 'correct' | 'wrong' | undefined;
    if (!key) return;
    const comment = this.trailingComment(doc.lines[outcome.line]);
    const body = this.outcomeLine(key, target);
    doc.lines[outcome.line] = comment ? `${body}  ${comment}` : body;
    outcome.target = target;
  }

  updateAnswer(doc: AdvDocument, answer: AskAnswer, text: string): void {
    const comment = this.trailingComment(doc.lines[answer.lineIndex]);
    const body = `@answer ${text}`;
    doc.lines[answer.lineIndex] = comment ? `${body}  ${comment}` : body;
    answer.text = text;
  }

  /** Ajoute une réponse acceptée en fin de liste (ou juste après `@ask` s'il
   * n'y en a aucune). Change le nombre de lignes -> reparse. */
  addAnswer(doc: AdvDocument, ask: AskInfo, text = 'reponse'): void {
    const at = ask.answers.length ? ask.answers[ask.answers.length - 1].lineIndex + 1 : ask.line + 1;
    doc.lines.splice(at, 0, `@answer ${text}`);
    this.reparseInPlace(doc);
  }

  removeAnswer(doc: AdvDocument, answer: AskAnswer): void {
    doc.lines.splice(answer.lineIndex, 1);
    this.reparseInPlace(doc);
  }

  /** Transforme une section ordinaire en énigme à saisie : ajoute `@ask`,
   * une `@answer` et les deux issues `@correct`/`@wrong` (obligatoires côté
   * compilateur), même règle de position que `addCombat`. Sans effet si la
   * section a déjà un `@ask`. */
  addAsk(doc: AdvDocument, section: AdvSection): void {
    if (section.ask) return;
    const at = section.entryEffects.length
      ? section.entryEffects[section.entryEffects.length - 1].lineIndex + 1
      : section.startLine + 1;
    const t = AdvParser.PLACEHOLDER_TARGET;
    doc.lines.splice(at, 0, '@ask "Question ?"', `@answer ${t}`, `@correct ${t}`, `@wrong ${t}`);
    this.reparseInPlace(doc);
  }

  /** Retire `@ask` et tout ce qui en dépend (réponses, issues, leurs effets). */
  removeAsk(doc: AdvDocument, section: AdvSection): void {
    const ak = section.ask;
    if (!ak) return;
    const lines = [ak.line, ak.correct?.line, ak.wrong?.line]
      .concat(ak.answers.map((a) => a.lineIndex))
      .concat(ak.correct?.effects.map((e) => e.lineIndex) ?? [])
      .concat(ak.wrong?.effects.map((e) => e.lineIndex) ?? [])
      .filter((n): n is number => n !== undefined)
      .sort((a, b) => b - a);
    for (const l of lines) doc.lines.splice(l, 1);
    this.reparseInPlace(doc);
  }

  /** Ajoute l'issue manquante (`@correct`/`@wrong`), cible de secours a
   * completer. Sans effet si elle existe deja. */
  addAskOutcome(doc: AdvDocument, ask: AskInfo, key: 'correct' | 'wrong'): void {
    if (ask[key]) return;
    doc.lines.splice(ask.line + 1, 0, `@${key} ${AdvParser.PLACEHOLDER_TARGET}`);
    this.reparseInPlace(doc);
  }

  removeAskOutcome(doc: AdvDocument, ask: AskInfo, key: 'correct' | 'wrong'): void {
    const outcome = ask[key];
    if (!outcome) return;
    const lines = [outcome.line, ...outcome.effects.map((e) => e.lineIndex)].sort((a, b) => b - a);
    for (const l of lines) doc.lines.splice(l, 1);
    this.reparseInPlace(doc);
  }

  /** Texte brut d'une section (pour l'éditeur de détail), lignes jointes. */
  sectionText(doc: AdvDocument, section: AdvSection): string {
    return doc.lines.slice(section.startLine, section.endLine).join('\n');
  }

  /** Réécrit le texte brut d'une section. Le nombre de lignes peut changer :
   * plutôt que décaler à la main choix/effets/sections suivantes (une classe
   * de bug à elle seule si on la réécrit deux fois, cf. `reparseInPlace`), on
   * ré-analyse tout le document sur la même référence `doc`. */
  setSectionText(doc: AdvDocument, section: AdvSection, text: string): void {
    const newLines = text.split('\n');
    doc.lines.splice(section.startLine, section.endLine - section.startLine, ...newLines);
    this.reparseInPlace(doc);
  }

  // --- Préambule : titre / auteur / départ --------------------------------

  updateMeta(doc: AdvDocument, patch: { title?: string; author?: string; start?: string }): AdvDocument {
    const lines = [...doc.lines];
    const set = (directive: string, value: string) => {
      const re = new RegExp(`^\\s*@${directive}\\b`);
      const idx = lines.slice(0, doc.preambleEnd).findIndex((l) => re.test(l));
      const text = `@${directive}  ${value}`;
      if (idx >= 0) lines[idx] = text;
      else lines.splice(doc.preambleEnd, 0, text);
    };
    if (patch.title !== undefined) set('title', patch.title);
    if (patch.author !== undefined) set('author', patch.author);
    if (patch.start !== undefined) set('start', patch.start);
    return this.reparseAfterSplice(lines);
  }

  // --- Caractéristiques (@stat) --------------------------------------------

  addStat(doc: AdvDocument): AdvDocument {
    const name = this.uniqueName('NOUVELLE_STAT', new Set(doc.stats.map((s) => s.name)));
    const lines = [...doc.lines];
    const at = doc.stats.length ? doc.stats[doc.stats.length - 1].line + 1 : doc.preambleEnd;
    lines.splice(at, 0, this.statLine({ name, init: 0, lo: 0, hi: 10, hidden: false }));
    return this.reparseAfterSplice(lines);
  }

  updateStat(doc: AdvDocument, stat: StatDecl, patch: Partial<Omit<StatDecl, 'line'>>): AdvDocument {
    const lines = [...doc.lines];
    lines[stat.line] = this.statLine({ ...stat, ...patch });
    return this.reparseAfterSplice(lines);
  }

  removeStat(doc: AdvDocument, stat: StatDecl): AdvDocument {
    const lines = [...doc.lines];
    lines.splice(stat.line, 1);
    return this.reparseAfterSplice(lines);
  }

  // --- Drapeaux (@flag) -----------------------------------------------------

  addFlag(doc: AdvDocument): AdvDocument {
    const name = this.uniqueName('nouveau_drapeau', new Set(doc.flags.map((f) => f.name)));
    const lines = [...doc.lines];
    const at = doc.flags.length ? doc.flags[doc.flags.length - 1].line + 1 : doc.preambleEnd;
    lines.splice(at, 0, this.flagLine({ name, on: false, local: false }));
    return this.reparseAfterSplice(lines);
  }

  updateFlag(doc: AdvDocument, flag: FlagDecl, patch: Partial<Omit<FlagDecl, 'line'>>): AdvDocument {
    const merged = { ...flag, ...patch };
    if (merged.local) merged.on = false; // un flag local demarre toujours a off (regle a2c)
    const lines = [...doc.lines];
    lines[flag.line] = this.flagLine(merged);
    return this.reparseAfterSplice(lines);
  }

  removeFlag(doc: AdvDocument, flag: FlagDecl): AdvDocument {
    const lines = [...doc.lines];
    lines.splice(flag.line, 1);
    return this.reparseAfterSplice(lines);
  }

  // --- Objets (@item) --------------------------------------------------------

  addItem(doc: AdvDocument): AdvDocument {
    const name = this.uniqueName('nouvel_objet', new Set(doc.items.map((i) => i.name)));
    const lines = [...doc.lines];
    const at = doc.items.length ? doc.items[doc.items.length - 1].line + 1 : doc.preambleEnd;
    lines.splice(at, 0, this.itemLine({ name, label: 'Nouvel objet', on: false, atk: 0, dmg: 0, armor: 0 }));
    return this.reparseAfterSplice(lines);
  }

  updateItem(doc: AdvDocument, item: ItemDecl, patch: Partial<Omit<ItemDecl, 'line'>>): AdvDocument {
    const lines = [...doc.lines];
    lines[item.line] = this.itemLine({ ...item, ...patch });
    return this.reparseAfterSplice(lines);
  }

  removeItem(doc: AdvDocument, item: ItemDecl): AdvDocument {
    const lines = [...doc.lines];
    lines.splice(item.line, 1);
    return this.reparseAfterSplice(lines);
  }

  // --- Sections et chapitres -------------------------------------------------

  /** Nombre de choix, dans TOUTE l'aventure, qui ciblent cette section —
   * pour avertir avant suppression (pas une garantie : les `~ goto` bruts
   * ne sont pas suivis, volontairement, comme partout ailleurs ici). */
  countReferences(doc: AdvDocument, sectionName: string): number {
    let n = 0;
    for (const s of doc.sections) for (const c of s.choices) if (c.target === sectionName) n++;
    return n;
  }

  addSection(doc: AdvDocument, chapterIndex: number): AdvDocument {
    const chapter = doc.chapters[chapterIndex];
    const name = this.uniqueName('nouvelle_section', new Set(doc.sections.map((s) => s.name)));
    const at =
      chapter.sections.length > 0
        ? chapter.sections[chapter.sections.length - 1].endLine
        : chapter.headerLine !== null
          ? chapter.headerLine + 1
          : doc.preambleEnd;
    const lines = [...doc.lines];
    lines.splice(at, 0, '', `:: ${name}`, 'Texte à écrire.', '');
    return this.reparseAfterSplice(lines);
  }

  /** Suppression réelle : retire la section et tout son texte. Les choix
   * d'autres sections qui la ciblaient ne sont PAS réécrits — comptez
   * `countReferences` avant, pour prévenir plutôt que corriger. */
  removeSection(doc: AdvDocument, section: AdvSection): AdvDocument {
    const lines = [...doc.lines];
    // Emporte aussi la ligne vide juste avant, si elle y est (celle qui la
    // separait de ce qui precede) : sinon un ajout puis un retrait immediats
    // laissent une ligne vide en trop.
    let from = section.startLine;
    if (from > 0 && lines[from - 1].trim() === '') from--;
    lines.splice(from, section.endLine - from);
    return this.reparseAfterSplice(lines);
  }

  /** Ajoute un chapitre en fin de document, avec une première section. */
  addChapter(doc: AdvDocument, title: string): AdvDocument {
    const name = this.uniqueName('nouvelle_section', new Set(doc.sections.map((s) => s.name)));
    const lines = [...doc.lines];
    lines.push('', `@chapter "${title}"`, '', `:: ${name}`, 'Texte à écrire.', '');
    return this.reparseAfterSplice(lines);
  }

  /** Retire la frontière de chapitre (fusion avec le précédent) — les
   * sections restent, intactes ; seul le découpage en chapitres change. */
  mergeChapterWithPrevious(doc: AdvDocument, chapterIndex: number): AdvDocument {
    const chapter = doc.chapters[chapterIndex];
    if (chapter.headerLine === null) return doc; // le prologue n'a pas de frontière à retirer
    const lines = [...doc.lines];
    lines.splice(chapter.headerLine, 1);
    return this.reparseAfterSplice(lines);
  }

  /** Retire un commentaire de fin de ligne hors guillemets (cf. a2c/parser.py). */
  private stripComment(line: string): string {
    let out = '';
    let inStr = false;
    for (const ch of line) {
      if (ch === '"') inStr = !inStr;
      if (ch === '#' && !inStr) break;
      out += ch;
    }
    return out.trimEnd();
  }
}
