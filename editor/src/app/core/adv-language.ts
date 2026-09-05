import { StreamLanguage } from '@codemirror/language';

/**
 * Coloration syntaxique du `.adv`, sur le même principe que le format :
 * le premier caractère non blanc d'une ligne décide de son type
 * (cf. docs/GUIDE-FORMAT-ADV.md §1). Volontairement approximative — c'est une
 * aide à la lecture, pas une validation ; `a2c` reste la référence.
 */
type LineMode = 'text' | 'section' | 'directive' | 'choice' | 'effect' | 'comment';

interface AdvState {
  mode: LineMode;
}

function classifyLine(line: string): LineMode {
  const t = line.replace(/^\s+/, '');
  if (!t) return 'text';
  const c0 = t[0];
  if (c0 === '#') return 'comment';
  if (t.startsWith('::')) return 'section';
  if (c0 === '@') return 'directive';
  if (c0 === '~') return 'effect';
  if (c0 === '*' && (t.length === 1 || t[1] === ' ')) return 'choice';
  return 'text';
}

export const advLanguage = StreamLanguage.define<AdvState>({
  startState: () => ({ mode: 'text' }),

  token(stream, state) {
    if (stream.sol()) {
      state.mode = classifyLine(stream.string);
    }

    // Condition entre accolades : partout, quel que soit le type de ligne.
    if (stream.match(/^\{[^}]*\}/)) return 'atom';
    // Chaine entre guillemets : partout (titres, invites, reponses...).
    if (stream.match(/^"[^"]*"/)) return 'string';
    // Commentaire de fin de ligne : uniquement sur les lignes structurelles
    // (@, ::, *, ~) — un '#' en texte narratif est litteral (cf. §1).
    if (state.mode !== 'text' && stream.peek() === '#') {
      stream.skipToEnd();
      return 'comment';
    }

    switch (state.mode) {
      case 'comment':
        stream.skipToEnd();
        return 'comment';

      case 'section':
        if (stream.match('::')) return 'keyword';
        if (stream.match(/^[A-Za-z_][A-Za-z0-9_]*/)) return 'typeName';
        stream.next();
        return null;

      case 'directive':
        if (stream.match(/^@[A-Za-z_]+/)) return 'keyword';
        stream.next();
        return null;

      case 'choice':
        if (stream.match('*')) return 'keyword';
        if (stream.match(/^\[[^\]]*\]/)) return 'string';
        if (stream.match('->')) return 'operator';
        if (stream.match(/^[A-Za-z_][A-Za-z0-9_]*/)) return 'variableName';
        stream.next();
        return null;

      case 'effect':
        if (stream.match('~')) return 'keyword';
        if (stream.match(/^[A-Za-z_][A-Za-z0-9_]*/)) return 'propertyName';
        stream.next();
        return null;

      case 'text':
      default:
        // mise en relief *mot(s)* et titres "= "/"! "/"=! " (cf. §2)
        if (stream.match(/^\*[^*]*\*/)) return 'emphasis';
        if (stream.sol() && stream.match(/^[=!]+ /)) return 'heading';
        stream.next();
        return null;
    }
  },
});
