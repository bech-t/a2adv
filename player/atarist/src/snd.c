/* snd.c -- pilote YM2149 (cf. snd.h). Portage du MODELE de programmation
 * par registres de player/apple2/src/snd_mb.c (AY-3-8910, registre-compatible
 * avec le YM2149) -- pas du code, qui est specifique au bus 6522/6502.
 *
 * ATTENTION : materiel NON teste (comme snd_mb.c en son temps). A valider
 * sous Hatari puis sur ST reel.
 *
 * Acces bas niveau : $FF8800 (selection de registre) / $FF8802 (donnee),
 * les deux ports memoire-mappes standard du PSG sur ST. Necessite le mode
 * superviseur (cf. scr_init : Super(0L)), comme tout acces materiel direct
 * sur ST.
 *
 * Registre 7 (mixeur) initialise a une valeur FIXE (0xFF, cf. st_init)
 * plutot que lu puis modifie : ses bits 6-7 sont la direction des ports
 * IOA/IOB du chip (IOA cable sur le ST au selecteur de face disquette et au
 * signal STROBE Centronics), et ce chip ne supporte pas la relecture de
 * registre (comme l'AY-3-8910 du Mockingboard). 0xFF met IOA/IOB en sortie
 * (ce que TOS attend) et coupe les trois voies ; seuls les bits 0-5
 * changent ensuite, via mixer_shadow. */

#include "snd.h"
#include "scr.h"      /* scr_idle_hook, scr_frclock : la musique avance pendant l'attente */

#define PSG_SELECT (*(volatile u8 *)0xFFFF8800L)
#define PSG_DATA   (*(volatile u8 *)0xFFFF8802L)

static void ym_w(u8 reg, u8 val)
{
    PSG_SELECT = reg;
    PSG_DATA = val;
}

/* Actif par defaut : contrairement au Mockingboard (carte optionnelle dont
 * le slot doit etre choisi a la main avant tout risque d'y ecrire), le
 * YM2149 est toujours la -- rien ne justifie de demarrer en silence. */
u8 snd_backend = 1;   /* 0 = silence, 1 = YM2149 actif */
u8 snd_mb_slot;        /* sans objet sur ST, cf. snd.h */

static u8 ready;
static u8 mixer_shadow;   /* bits 0-5 = tons/bruits ; bits 6-7 = IOA/IOB, jamais modifies */

/* --- Voies ---------------------------------------------------------------
 * A=0 B=1 C=2. A/B = musique (amplitude fixe, pas d'enveloppe) ; C = effets
 * (seule voie a utiliser l'enveloppe materielle, cf. snd.h). */
#define CH_MUSIC0  0
#define CH_FX      2
#define AMP_ENV    0x10   /* amplitude "suit l'enveloppe" (bit 4, registres 8-10) */

static void st_tone(u8 ch, u16 period)
{
    ym_w((u8)(ch * 2),     (u8)(period & 0xFF));
    ym_w((u8)(ch * 2 + 1), (u8)((period >> 8) & 0x0F));
}

static void st_amp(u8 ch, u8 amp)
{
    ym_w((u8)(8 + ch), (u8)(amp & 0x1F));
}

static void st_mix(u8 ch, u8 tone_on, u8 noise_on)
{
    if (tone_on)  mixer_shadow &= (u8)~(1 << ch);
    else          mixer_shadow |= (u8)(1 << ch);
    if (noise_on) mixer_shadow &= (u8)~(8 << ch);
    else          mixer_shadow |= (u8)(8 << ch);
    ym_w(7, mixer_shadow);
}

static void st_noise(u8 period)   /* generateur de bruit UNIQUE, partage par les 3 voies */
{
    ym_w(6, (u8)(period & 0x1F));
}

static void st_env(u16 period, u8 shape)   /* generateur d'enveloppe UNIQUE : reserve a C */
{
    ym_w(11, (u8)(period & 0xFF));
    ym_w(12, (u8)(period >> 8));
    ym_w(13, (u8)(shape & 0x0F));   /* ecrire r13 REDECLENCHE l'enveloppe */
}

static void st_init(void)
{
    mixer_shadow = 0xFF;   /* IOA/IOB en sortie, tons/bruits tous coupes -- cf. entete */
    ym_w(7, mixer_shadow);
    ym_w(8, 0); ym_w(9, 0); ym_w(10, 0);        /* silence les trois voies */
    ready = 1;
}

static void ensure_ready(void)
{
    if (!ready)
        st_init();
}

void snd_use_mockingboard(u8 slot)
{
    if (slot) {
        ensure_ready();
        snd_backend = 1;
        scr_idle_hook = 0;   /* pose par snd_music() seulement si un morceau tourne */
    } else {
        snd_backend = 0;
        scr_idle_hook = 0;
        if (ready) {
            ym_w(8, 0); ym_w(9, 0); ym_w(10, 0);
            mixer_shadow |= 0x3F;
            ym_w(7, mixer_shadow);
        }
    }
    snd_mb_slot = 0;   /* sans objet sur ST */
}

void snd_tone(u8 pitch, u16 dur)
{
    (void)pitch;
    (void)dur;
}

/* --- Notes -----------------------------------------------------------
 * YM2149 cadence a 2 MHz sur ST (contre ~1 MHz assume pour l'AY-3-8910 du
 * Mockingboard) : periode = 125000 / frequence (formule identique a
 * snd_mb.c, cf. son commentaire "periode = 62500 / frequence" a 1 MHz --
 * ici deux fois plus vite, donc deux fois la periode a frequence egale).
 * Meme accord (index 0 = C3, La4 = 440 Hz a l'index 21) que le Mockingboard,
 * juste recalcule pour ce chip -- PAS une simple copie de sa table. */
#define NOTE_MAX  36
static const u16 note_period[NOTE_MAX] = {
    956, 902, 851, 804, 758, 716, 676, 638, 602, 568, 536, 506,
    /*  C3  Cd3   D3  Dd3   E3   F3  Fd3   G3  Gd3   A3  Ad3   B3 */
    478, 451, 426, 402, 379, 358, 338, 319, 301, 284, 268, 253,
    /*  C4  Cd4   D4  Dd4   E4   F4  Fd4   G4  Gd4   A4  Ad4   B4 */
    239, 225, 213, 201, 190, 179, 169, 159, 150, 142, 134, 127
    /*  C5  Cd5   D5  Dd5   E5   F5  Fd5   G5  Gd5   A5  Ad5   B5 */
};

#define REST 0xFE
#define ENDT 0xFF

static u16 note_period_of(u8 note)
{
    return (note < NOTE_MAX) ? note_period[note] : 0;
}

/* --- Morceaux --------------------------------------------------------
 * Meme flux [note][duree en VBL] que MbTune (cf. snd_mb.c), sur DEUX voix
 * au lieu de trois (melodie + basse -- l'harmonie, plus creuse, est celle
 * qui manque le moins avec une seule voie de moins). A ~50 Hz (PAL), 12
 * ticks font ~0,24 s -- des VBL reels (cf. scr_frclock), donc ~20 % plus
 * vite en NTSC, meme ecart deja documente pour wait_or_key (cf. scr.c). */
static const u8 title_mel[] = {
     9,12,  12,12,  16,12,  14,12,       /* A3  C4  E4  D4 */
    12,12,  11,12,   9,24,  REST,12,     /* C4  B3  A3  -- */
    16,12,  17,12,  16,12,  14,12,       /* E4  F4  E4  D4 */
    12,12,   9,24,  REST,24,
    ENDT
};
static const u8 title_bass[] = {
     9,51,   5,51,   7,51,   9,51,       /* A3  F3  G3  A3 */
    ENDT
};

#define MUSIC_AMP 10   /* amplitude fixe (pas d'enveloppe, cf. snd.h) */

static const u8 *trk[2];
static u8        trk_left[2];
static u8        music_on;
static u8        music_loop;
static const u8 *tune_track[2];

static void track_stop(u8 c)
{
    st_amp((u8)(CH_MUSIC0 + c), 0);
    st_mix((u8)(CH_MUSIC0 + c), 0, 0);
}

static u8 track_tick(u8 c)
{
    u8 note, dur;

    if (trk[c] == 0)
        return 0;
    if (trk_left[c] > 0) {
        --trk_left[c];
        return 1;
    }
    note = *trk[c]++;
    if (note == ENDT) {
        trk[c] = 0;
        track_stop(c);
        return 0;
    }
    dur = *trk[c]++;
    trk_left[c] = (u8)(dur ? dur - 1 : 0);

    if (note == REST) {
        track_stop(c);
    } else {
        st_tone((u8)(CH_MUSIC0 + c), note_period_of(note));
        st_amp((u8)(CH_MUSIC0 + c), MUSIC_AMP);
        st_mix((u8)(CH_MUSIC0 + c), 1, 0);
    }
    return 1;
}

static void music_start(void)
{
    u8 c;
    for (c = 0; c < 2; ++c) {
        trk[c] = tune_track[c];
        trk_left[c] = 0;
    }
    music_on = 1;
}

static void music_advance(void)
{
    u8 c, alive = 0;
    if (!music_on)
        return;
    for (c = 0; c < 2; ++c)
        alive |= track_tick(c);
    if (!alive) {
        if (music_loop)
            music_start();
        else
            music_on = 0;
    }
}

static u32 last_frclock;

static u8 tick_due(void)
{
    u32 now = scr_frclock();
    if (now == last_frclock)
        return 0;
    last_frclock = now;
    return 1;
}

static void snd_music_tick(void)
{
    if (!ready || !music_on)
        return;
    if (tick_due())
        music_advance();
}

void snd_music(u8 id)
{
    ensure_ready();
    if (!snd_backend) {
        music_on = 0;
        return;
    }
    if (id == MUS_NONE) {
        music_on = 0;
        track_stop(0);
        track_stop(1);
        scr_idle_hook = 0;
        return;
    }
    if (id != MUS_TITLE)
        return;
    tune_track[0] = title_mel;
    tune_track[1] = title_bass;
    music_loop = 1;
    last_frclock = scr_frclock();
    music_start();
    scr_idle_hook = snd_music_tick;
}

/* --- Effets (voie C, cf. snd.h) -------------------------------------- */

static void fx_wait(u8 ticks)
{
    while (ticks--) {
        while (!tick_due())
            ;
        music_advance();      /* la musique continue pendant le bruitage */
    }
}

static void fx_note(u8 note, u8 amp, u8 ticks)
{
    st_tone(CH_FX, note_period_of(note));
    st_amp(CH_FX, amp);
    st_mix(CH_FX, 1, 0);
    fx_wait(ticks);
    st_amp(CH_FX, 0);
}

static void fx_noise(u8 period, u8 amp, u8 ticks)
{
    st_noise(period);
    st_amp(CH_FX, amp);
    st_mix(CH_FX, 0, 1);
    fx_wait(ticks);
    st_amp(CH_FX, 0);
    st_mix(CH_FX, 0, 0);
}

static void fx_knock(u8 period, u16 decay, u8 ticks)
{
    st_noise(period);
    st_env(decay, 0x00);        /* MB_ENV_DECAY : attaque puis extinction */
    st_amp(CH_FX, AMP_ENV);
    st_mix(CH_FX, 0, 1);
    fx_wait(ticks);
    st_amp(CH_FX, 0);
    st_mix(CH_FX, 0, 0);
}

#define N_C3  0
#define N_G3  7
#define N_C4 12
#define N_E4 16
#define N_G4 19
#define N_A4 21
#define N_C5 24
#define N_E5 28
#define N_G5 31

/* Memes notes/amplitudes/durees que mb_play (snd_mb.c) : c'est le meme
 * chip, le meme tick ~50 Hz -- seule la voie change (C au lieu de l'AY #2). */
void snd_play(u8 id)
{
    u8 n;

    ensure_ready();
    if (!snd_backend)
        return;

    switch (id) {
    case SND_SELECT: fx_note(N_A4, 10, 2); break;
    case SND_ERROR:  fx_note(N_G3, 12, 4); fx_note(N_C3, 12, 6); break;

    case SND_WIN:    fx_note(N_G4, 13, 5);
                     fx_note(N_C5, 13, 18);
                     break;

    case SND_LOSE:   fx_note(N_G4, 13, 4); fx_note(N_E4, 13, 4);
                     fx_note(N_C3, 13, 10); break;
    case SND_PICKUP: fx_note(N_E5, 12, 3); fx_note(N_G5, 12, 5); break;
    case SND_HIT:    fx_knock(6, 250, 3); break;

    case SND_MAGIC:  for (n = N_C4; n < N_C5; n = (u8)(n + 4))
                         fx_note(n, 11, 2);
                     fx_note(N_C5 + 11, 12, 6);
                     break;

    case SND_DOOR:   fx_knock(24, 900, 6); break;
    case SND_PAGE:   fx_noise(24, 6, 2); break;

    default: break;
    }
    st_mix(CH_FX, 0, 0);
}
