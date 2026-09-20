// e2e-offline.mjs -- verifie dans un vrai navigateur que le site fonctionne
// hors ligne : installation du Service Worker, telechargement d'une aventure,
// jeu et sauvegardes serveur coupe, puis annonce d'une nouvelle version.
//
//   make site && CHROME=/chemin/vers/chrome make e2e
//
// Chromium est pilote par le protocole DevTools (WebSocket, global depuis
// Node 22) : aucune dependance a installer. Le site est servi depuis une copie
// temporaire de dist/webng/browser.

import { spawn } from "node:child_process";
import { cpSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const CHROME = process.env.CHROME;
if (!CHROME) {
  console.error("e2e: indiquer le navigateur avec CHROME=/chemin/vers/chrome");
  process.exit(2);
}
const DIST = process.argv[2] ?? "dist/webng/browser";
const PORT = 8771;
const DEBUG_PORT = 9334;
const ADV = "chateau_hante";

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let passed = 0;
let failed = 0;
function check(name, cond, detail = "") {
  cond ? passed++ : failed++;
  console.log(`${cond ? "ok  " : "FAIL"} ${name}${cond ? "" : `\n     ${detail}`}`);
}

const work = mkdtempSync(join(tmpdir(), "a2adv-e2e-"));
const site = join(work, "site");
cpSync(DIST, site, { recursive: true });

let server = null;
const startServer = () => {
  server = spawn("python3", ["-m", "http.server", String(PORT), "--bind", "127.0.0.1"], {
    cwd: site,
    stdio: "ignore",
  });
  return sleep(800);
};
const stopServer = () => {
  server.kill();
  return sleep(400);
};

const chrome = spawn(
  CHROME,
  [
    "--headless", "--no-sandbox", "--disable-gpu", "--window-size=420,900",
    `--remote-debugging-port=${DEBUG_PORT}`, `--user-data-dir=${join(work, "profile")}`,
    "about:blank",
  ],
  { stdio: "ignore" },
);

try {
  await startServer();
  await sleep(1500);
  const targets = await (await fetch(`http://127.0.0.1:${DEBUG_PORT}/json/list`)).json();
  const ws = new WebSocket(targets.find((t) => t.type === "page").webSocketDebuggerUrl);
  await new Promise((r) => (ws.onopen = r));
  let nextId = 0;
  const pending = new Map();
  ws.onmessage = (m) => {
    const d = JSON.parse(m.data);
    if (d.id && pending.has(d.id)) {
      pending.get(d.id)(d);
      pending.delete(d.id);
    }
  };
  const send = (method, params = {}) =>
    new Promise((resolve) => {
      const id = ++nextId;
      pending.set(id, resolve);
      ws.send(JSON.stringify({ id, method, params }));
    });
  const evalJs = async (expression) => {
    const r = await send("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true });
    if (r.result.exceptionDetails) throw new Error(JSON.stringify(r.result.exceptionDetails));
    return r.result.result.value;
  };
  const goto = async (url) => {
    await send("Page.navigate", { url });
    await sleep(1500);
  };
  const reload = async () => {
    await evalJs("location.reload()");
    await sleep(2200);
  };
  const text = () => evalJs("document.body.innerText");
  const waitText = async (needle, ms = 8000) => {
    for (const t0 = Date.now(); Date.now() - t0 < ms; await sleep(200)) {
      if ((await text()).includes(needle)) return true;
    }
    return false;
  };
  const click = (label) =>
    evalJs(`(() => {
      const el = [...document.querySelectorAll("button,a,label")]
        .find((e) => e.innerText.trim().startsWith(${JSON.stringify(label)}));
      if (el) el.click();
      return !!el;
    })()`);
  await send("Page.enable");
  await send("Runtime.enable");

  const base = `http://127.0.0.1:${PORT}/`;

  // 1. premiere visite : le catalogue s'affiche, le Service Worker s'installe
  await goto(base);
  check("catalogue affiché", (await waitText("Livres-jeux")) && (await waitText("Le Château Hanté")));
  await evalJs("navigator.serviceWorker.ready.then(() => true)");
  await sleep(1500);
  await goto(base);
  check("page contrôlée par le Service Worker", await evalJs("!!navigator.serviceWorker.controller"));
  const names = await evalJs("caches.keys()");
  check("cache d'application créé", names.some((k) => k.startsWith("a2adv-app-")), JSON.stringify(names));

  // 2. telechargement de l'aventure
  await goto(`${base}#/adv/${ADV}`);
  check("fiche affichée", await waitText("Télécharger pour jouer hors ligne"));
  await click("Télécharger pour jouer hors ligne");
  check("aventure disponible hors ligne", await waitText("disponible sans connexion", 10000), await text());
  await goto(base);
  check("pastille « Hors ligne » au catalogue", await waitText("Hors ligne"));

  // 3. serveur coupe : fiche et jeu se chargent depuis le cache
  await stopServer();
  await goto(`${base}#/adv/${ADV}`);
  await reload();
  check("fiche hors ligne", (await waitText("Le Château Hanté")) && (await waitText("Jouer")), await text());
  await goto(`${base}#/play/${ADV}?mode=new&slot=2`);
  await reload();
  check("jeu hors ligne", await waitText("Il y a un siècle"), await text());

  // 4. sauvegardes : ecriture dans l'emplacement 2, export, reprise, suppression
  await click("»"); // passe l'introduction
  await sleep(600);
  await click("≡");
  await sleep(300);
  await click("Sauvegarder et revenir");
  await sleep(500);
  const keys = await evalJs("Object.keys(localStorage).filter((k) => k.startsWith('a2adv:save'))");
  check("sauvegarde dans l'emplacement 2", keys.includes(`a2adv:save:${ADV}:2`), JSON.stringify(keys));
  await goto(`${base}#/adv/${ADV}`);
  await reload();
  const page = await text();
  check("liste des emplacements", page.includes("Emplacement 1") && page.includes("Emplacement 2") && page.includes("vide"), page);
  const exported = await evalJs(`(() => {
    let name = null;
    const orig = HTMLAnchorElement.prototype.click;
    HTMLAnchorElement.prototype.click = function () { name = this.download; };
    [...document.querySelectorAll("button")].find((e) => e.innerText.trim() === "Exporter").click();
    HTMLAnchorElement.prototype.click = orig;
    return name;
  })()`);
  check("export propose un fichier", exported === `a2adv-${ADV}-emplacement2.json`, String(exported));
  await goto(`${base}#/play/${ADV}?mode=resume&slot=2`);
  await reload();
  check("continuer restaure la partie", !(await text()).includes("Erreur"), await text());
  await goto(`${base}#/adv/${ADV}`);
  await reload();
  await click("Supprimer");
  await sleep(200);
  check("la suppression demande confirmation", (await text()).includes("Confirmer"));
  await click("Confirmer");
  await sleep(300);
  check("suppression effective", !(await evalJs(`localStorage.getItem("a2adv:save:${ADV}:2") !== null`)));

  // 4 bis. import d'un fichier de sauvegarde (valide, puis d'une autre aventure)
  const importFile = (body) =>
    evalJs(`(async () => {
      const input = document.querySelector('input[type="file"]');
      const data = new DataTransfer();
      data.items.add(new File([${JSON.stringify(body)}], "sauvegarde.json"));
      input.files = data.files;
      input.dispatchEvent(new Event("change", { bubbles: true }));
    })()`);
  const saveFile = (adventure) =>
    JSON.stringify({
      app: "a2adv", format: 1, adventure, savedAt: "2026-09-20T10:00:00Z", hash: "autre",
      data: { section: 0, statVal: [8], statMax: [8], itemBits: [0], flagBits: [0], score: 5, moves: 3 },
    });
  await importFile(saveFile("une_autre_aventure"));
  check("import refusé pour une autre aventure", await waitText("celle de « une_autre_aventure »"), await text());
  await importFile(saveFile(ADV));
  check("import d'une sauvegarde valide", await waitText("importée dans l'emplacement 1"), await text());
  check("l'emplacement importé signale la version différente", await waitText("autre version de l'aventure"), await text());

  // 5. nouvelle version du site : annonce, activation, menage des anciens caches
  await startServer();
  const swPath = join(site, "sw.js");
  writeFileSync(swPath, readFileSync(swPath, "utf-8").replace(/const VERSION = "[a-f0-9]+";/, 'const VERSION = "nouvelle";'));
  await goto(base);
  await evalJs("navigator.serviceWorker.getRegistration().then((r) => r.update())");
  check("annonce d'une nouvelle version", await waitText("nouvelle version est disponible"), await text());
  await click("Recharger");
  await sleep(2500);
  const after = await evalJs("caches.keys()");
  const apps = after.filter((k) => k.startsWith("a2adv-app-"));
  check(
    "ancienne version supprimée, aventures conservées",
    apps.length === 1 && apps[0] === "a2adv-app-nouvelle" && after.includes("a2adv-adv"),
    JSON.stringify(after),
  );
} finally {
  chrome.kill();
  server?.kill();
  await sleep(800);
  rmSync(work, { recursive: true, force: true });
}

console.log(`\n${passed} ok, ${failed} échec(s)`);
process.exit(failed ? 1 : 0);
