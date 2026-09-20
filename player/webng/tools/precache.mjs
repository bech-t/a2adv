// precache.mjs -- apres `ng build`, ecrit dans le sw.js du site la liste des
// fichiers de l'application et une VERSION qui en depend (cf. public/sw.js).
//
//   node tools/precache.mjs dist/webng/browser
//
// Les aventures et le catalogue n'y figurent pas : le Service Worker les
// gere lui-meme (cf. sw.js).

import { createHash } from "node:crypto";
import { readdirSync, readFileSync, writeFileSync } from "node:fs";
import { join, relative, sep } from "node:path";

const root = process.argv[2];
if (!root) {
  console.error("usage: node tools/precache.mjs <dossier du site>");
  process.exit(2);
}

const EXCLUDED = ["sw.js", "catalog.json", "3rdpartylicenses.txt", "prerendered-routes.json"];

function walk(dir) {
  return readdirSync(dir, { withFileTypes: true }).flatMap((e) =>
    e.isDirectory() ? walk(join(dir, e.name)) : [join(dir, e.name)],
  );
}

const files = walk(root)
  .map((f) => relative(root, f).split(sep).join("/"))
  .filter((f) => !f.startsWith("adventures/") && !EXCLUDED.includes(f))
  .sort();

const hash = createHash("sha1");
for (const f of files) hash.update(f).update(readFileSync(join(root, f)));
const version = hash.digest("hex").slice(0, 10);

const swPath = join(root, "sw.js");
let sw = readFileSync(swPath, "utf-8");
const versionRe = /^const VERSION = ".*";$/m;
const precacheRe = /^const PRECACHE = \[.*\];$/m;
if (!versionRe.test(sw) || !precacheRe.test(sw)) {
  console.error("precache: VERSION ou PRECACHE introuvable dans sw.js");
  process.exit(1);
}
sw = sw
  .replace(versionRe, `const VERSION = "${version}";`)
  .replace(precacheRe, `const PRECACHE = ${JSON.stringify(files)};`);
writeFileSync(swPath, sw);
console.log(`precache: ${files.length} fichiers, version ${version}`);
