// snd.ts -- effets sonores Web Audio, memes 11 ids SND_* que format.h.
// Synthese a la volee (OscillatorNode/oscillateur de bruit), meme esprit
// "chiptune" que les trois ports natifs plutot que des fichiers audio
// produits -- les frequences/durees reprennent directement celles de
// player/dos/src/snd.c (deja exprimees en Hz/ms, contrairement aux tables
// de periodes PSG d'Atari ST ou de pitch haut-parleur d'Apple II).
//
// Pas de musique de fond pour l'instant (snd_menu_music cote natif) : ce
// module ne fournit que playSound(id), le seul contrat que engine/ appelle
// (cf. engine/engine.ts:PlaySound).

import { Snd } from "../engine/format";

let ctx: AudioContext | null = null;

function ensureCtx(): AudioContext | null {
  if (typeof window === "undefined") return null; // hors navigateur (tests Node)
  if (!ctx) {
    const Ctor = window.AudioContext ?? (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
    if (!Ctor) return null;
    ctx = new Ctor();
  }
  if (ctx.state === "suspended") void ctx.resume(); // leve par le 1er geste utilisateur
  return ctx;
}

function tone(freqHz: number, ms: number, startAt: number): number {
  const c = ensureCtx();
  if (!c) return startAt;
  const osc = c.createOscillator();
  const gain = c.createGain();
  osc.type = "square";
  osc.frequency.value = freqHz;
  gain.gain.value = 0.15; // volume prudent : plusieurs sons peuvent s'empiler
  osc.connect(gain).connect(c.destination);
  const t0 = c.currentTime + startAt;
  const t1 = t0 + ms / 1000;
  gain.gain.setValueAtTime(0.15, t0);
  gain.gain.exponentialRampToValueAtTime(0.001, t1); // evite le clic de coupure nette
  osc.start(t0);
  osc.stop(t1);
  return startAt + ms / 1000;
}

function noise(ms: number, startAt: number): number {
  const c = ensureCtx();
  if (!c) return startAt;
  const n = Math.floor(c.sampleRate * (ms / 1000));
  const buf = c.createBuffer(1, Math.max(1, n), c.sampleRate);
  const data = buf.getChannelData(0);
  for (let i = 0; i < data.length; ++i) data[i] = Math.random() * 2 - 1;
  const src = c.createBufferSource();
  src.buffer = buf;
  const gain = c.createGain();
  gain.gain.value = 0.1;
  src.connect(gain).connect(c.destination);
  const t0 = c.currentTime + startAt;
  src.start(t0);
  return startAt + ms / 1000;
}

/** Sequence de tons/bruits joues bout a bout a partir de maintenant. */
function seq(...steps: Array<{ freq: number; ms: number } | { noise: number }>): void {
  let t = 0;
  for (const s of steps) {
    if ("noise" in s) t = noise(s.noise, t);
    else t = tone(s.freq, s.ms, t);
  }
}

export function playSound(id: number): void {
  switch (id) {
    case Snd.SELECT:
      seq({ freq: 440, ms: 40 });
      break;
    case Snd.ERROR:
      seq({ freq: 196, ms: 90 }, { freq: 131, ms: 130 });
      break;
    case Snd.WIN:
      seq({ freq: 392, ms: 90 }, { freq: 523, ms: 380 });
      break;
    case Snd.LOSE:
      seq({ freq: 392, ms: 80 }, { freq: 330, ms: 80 }, { freq: 131, ms: 200 });
      break;
    case Snd.PICKUP:
      seq({ freq: 659, ms: 60 }, { freq: 784, ms: 90 });
      break;
    case Snd.HIT:
      seq({ noise: 35 });
      break;
    case Snd.MAGIC:
      seq(
        { freq: 262, ms: 30 },
        { freq: 302, ms: 30 },
        { freq: 342, ms: 30 },
        { freq: 382, ms: 30 },
        { freq: 422, ms: 30 },
        { freq: 462, ms: 30 },
        { freq: 659, ms: 120 },
      );
      break;
    case Snd.DOOR:
      seq({ freq: 90, ms: 30 }, { noise: 60 });
      break;
    case Snd.PAGE:
      seq({ noise: 50 });
      break;
    case Snd.DREAD:
      seq(
        { freq: 131, ms: 45 },
        { freq: 139, ms: 45 },
        { freq: 131, ms: 45 },
        { freq: 139, ms: 45 },
        { noise: 220 },
      );
      break;
    case Snd.BONUS:
      seq({ freq: 659, ms: 60 }, { freq: 784, ms: 60 }, { freq: 988, ms: 90 });
      break;
    default:
      break;
  }
}
