import { Component, computed, effect, input, output } from '@angular/core';
import { MatTreeModule, MatTreeNestedDataSource } from '@angular/material/tree';
import { NestedTreeControl } from '@angular/cdk/tree';
import { MatIconModule } from '@angular/material/icon';
import { MatButtonModule } from '@angular/material/button';
import { MatTooltipModule } from '@angular/material/tooltip';
import type { AdvDocument } from '../../core/adv-parser';

export interface SceneNode {
  name: string;
  viaLabel: string | null; // libelle du choix qui mene ici (null = racine)
  children: SceneNode[];
  alreadyShown: boolean; // deja developpe ailleurs dans l'arbre -> feuille
  missing: boolean; // cible sans section correspondante
}

/**
 * Arbre de scènes, enraciné sur `@start`. Ce n'est PAS un simple arbre : le
 * graphe réel boucle (carrefours, culs-de-sac récupérés) et une même section
 * peut avoir plusieurs parents. On développe chaque section une seule fois ;
 * ses réapparitions suivantes deviennent des feuilles cliquables plutôt que
 * d'être redéveloppées — sinon un simple carrefour à 5 choix qui revient sur
 * lui-même donnerait un arbre infini.
 */
@Component({
  selector: 'app-scene-tree',
  imports: [MatTreeModule, MatIconModule, MatButtonModule, MatTooltipModule],
  templateUrl: './scene-tree.html',
  styleUrl: './scene-tree.scss',
})
export class SceneTree {
  readonly doc = input<AdvDocument | null>(null);
  readonly selected = input<string | null>(null);
  readonly select = output<string>();

  readonly treeControl = new NestedTreeControl<SceneNode>((n) => n.children);
  readonly dataSource = new MatTreeNestedDataSource<SceneNode>();

  readonly roots = computed<SceneNode[]>(() => {
    const doc = this.doc();
    if (!doc || !doc.start) return [];
    const byName = new Map(doc.sections.map((s) => [s.name, s]));
    const shown = new Set<string>();

    const build = (name: string, viaLabel: string | null): SceneNode => {
      const sec = byName.get(name);
      if (!sec) return { name, viaLabel, children: [], alreadyShown: false, missing: true };
      if (shown.has(name)) {
        return { name, viaLabel, children: [], alreadyShown: true, missing: false };
      }
      shown.add(name);
      const children = sec.choices.map((c) => build(c.target, c.label || '(sans libellé)'));
      return { name, viaLabel, children, alreadyShown: false, missing: false };
    };

    return [build(doc.start, null)];
  });

  hasChild = (_: number, node: SceneNode) => node.children.length > 0;

  constructor() {
    // MatTreeNestedDataSource attend une assignation imperative : effect()
    // fait le pont depuis le signal calcule `roots`.
    effect(() => {
      const roots = this.roots();
      this.dataSource.data = roots;
      // Deploie tout par defaut : une grosse aventure se lit d'un coup d'oeil.
      const expandAll = (nodes: SceneNode[]) => {
        for (const n of nodes) {
          if (n.children.length) {
            this.treeControl.expand(n);
            expandAll(n.children);
          }
        }
      };
      expandAll(roots);
    });
  }
}
