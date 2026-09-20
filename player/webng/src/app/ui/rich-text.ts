// rich-text.ts -- rendu d'un bloc de paragraphes (chaque <p> peut etre
// centre/inverse, et contenir des segments surligne coupes aux octets
// TXT_INV_TOGGLE). Le DOM fait deja le retour a la ligne.

import { Component, computed, input } from "@angular/core";
import { STYLE_CENTER, STYLE_INVERSE, TXT_INV_TOGGLE } from "../engine/format";
import type { TextParagraph } from "../engine/section";

interface Segment {
  text: string;
  on: boolean;
}

interface RenderParagraph {
  classes: string;
  segments: Segment[];
}

/** Decoupe un texte aux octets TXT_INV_TOGGLE en segments normaux/surlignes. */
function splitToggle(text: string): Segment[] {
  const parts: Segment[] = [];
  let on = false;
  let buf = "";
  for (let i = 0; i < text.length; ++i) {
    if (text.charCodeAt(i) === TXT_INV_TOGGLE) {
      if (buf) parts.push({ text: buf, on });
      buf = "";
      on = !on;
    } else {
      buf += text[i];
    }
  }
  if (buf) parts.push({ text: buf, on });
  return parts;
}

@Component({
  selector: "app-rich-text",
  template: `
    @for (p of renderParagraphs(); track $index) {
      <p [class]="p.classes">
        @for (seg of p.segments; track $index) {
          @if (seg.on) {
            <mark>{{ seg.text }}</mark>
          } @else {
            <span>{{ seg.text }}</span>
          }
        }
      </p>
    }
  `,
})
export class RichText {
  paragraphs = input.required<TextParagraph[]>();

  protected readonly renderParagraphs = computed<RenderParagraph[]>(() =>
    this.paragraphs().map((p) => {
      const classes = ["a2-p"];
      if (p.style & STYLE_CENTER) classes.push("a2-center");
      if (p.style & STYLE_INVERSE) classes.push("a2-inverse");
      return { classes: classes.join(" "), segments: splitToggle(p.text) };
    }),
  );
}
