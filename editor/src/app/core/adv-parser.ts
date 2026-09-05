import { Injectable } from '@angular/core';

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
}

/** Signaux "quel genre de section" — pour l'icône dans la hiérarchie, pas
 * pour la validation (plusieurs peuvent être vrais à la fois : un combat
 * peut aussi porter une image). */
export interface SectionKind {
  isStart: boolean;
  isIntro: boolean;
  ending: 'victoire' | 'defaite' | null;
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
          kind: { isStart: false, isIntro: false, ending: null, hasCombat: false, hasAsk: false, hasImage: false },
        };
        sections.push(current);
        chapters[chapters.length - 1].sections.push(current);
        continue;
      }

      if (current && trimmed.startsWith('*') && (trimmed.length === 1 || trimmed[1] === ' ')) {
        const m = this.stripComment(trimmed).match(CHOICE_RE);
        if (m) {
          current.choices.push({
            lineIndex: i,
            cond: m[1] ?? null,
            label: m[2].trim(),
            target: m[3],
          });
        }
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
      if (/^@ending\s+victoire/.test(t)) kind.ending = 'victoire';
      else if (/^@ending\s+defaite/.test(t)) kind.ending = 'defaite';
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

  /** Texte brut d'une section (pour l'éditeur de détail), lignes jointes. */
  sectionText(doc: AdvDocument, section: AdvSection): string {
    return doc.lines.slice(section.startLine, section.endLine).join('\n');
  }

  /** Réécrit le texte brut d'une section, puis ré-extrait ses choix. Le
   * nombre de lignes peut changer : les sections suivantes sont décalées. */
  setSectionText(doc: AdvDocument, section: AdvSection, text: string): void {
    const newLines = text.split('\n');
    const delta = newLines.length - (section.endLine - section.startLine);
    doc.lines.splice(section.startLine, section.endLine - section.startLine, ...newLines);
    section.endLine = section.startLine + newLines.length;
    if (delta !== 0) {
      for (const s of doc.sections) {
        if (s !== section && s.startLine > section.startLine) {
          s.startLine += delta;
          s.endLine += delta;
          for (const c of s.choices) c.lineIndex += delta;
        }
      }
    }
    section.choices = [];
    for (let i = section.startLine; i < section.endLine; i++) {
      const trimmed = doc.lines[i].trim();
      if (trimmed.startsWith('*') && (trimmed.length === 1 || trimmed[1] === ' ')) {
        const m = this.stripComment(trimmed).match(CHOICE_RE);
        if (m) {
          section.choices.push({
            lineIndex: i,
            cond: m[1] ?? null,
            label: m[2].trim(),
            target: m[3],
          });
        }
      }
    }
    section.kind = this.computeKind(doc.lines, section, section.kind.isStart, section.kind.isIntro);
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
