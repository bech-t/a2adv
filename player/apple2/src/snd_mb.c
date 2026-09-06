/* snd_mb.c -- pilote Mockingboard (cf. snd_mb.h).
 *
 * Materiel : 2x AY-3-8910, chacun pilote par un 6522 (VIA). VIA #1 en $Cn00
 * (AY musique), VIA #2 en $Cn80 (AY effets).
 *
 * NON teste sur materiel : a valider sur emulateur Mockingboard.
 */

#include "snd_mb.h"

/* Le pilote n'est compile QUE si le backend est demande (make MOCKINGBOARD=1,
 * defaut). Auparavant ce fichier n'etait garde que par __CC65__ : son code
 * etait lie meme avec MOCKINGBOARD=0, qui abaisse pourtant HIMEM pour
 * recuperer de la memoire. L'echappatoire ne recuperait donc rien, et le
 * binaire debordait. */
#if defined(__CC65__) && defined(A2ADV_MOCKINGBOARD)

/* --- Registres 6522 (offsets depuis la base de la VIA) ------------------- */
#define VIA_ORB   0x00     /* port B : lignes de controle AY (BDIR/BC1/RESET) */
#define VIA_ORA   0x01     /* port A : bus de donnees/adresse vers l'AY       */
#define VIA_DDRB  0x02
#define VIA_DDRA  0x03
#define VIA_T1CL  0x04     /* compteur T1 bas (la lecture acquitte le drapeau) */
#define VIA_T1CH  0x05     /* compteur T1 haut (l'ecriture arme et lance)     */
#define VIA_T1LL  0x06     /* latch T1 bas                                    */
#define VIA_T1LH  0x07     /* latch T1 haut                                   */
#define VIA_ACR   0x0B     /* auxiliary control : mode de T1                  */
#define VIA_IER   0x0E     /* interrupt enable                                */
#define VIA_IFR   0x0D     /* interrupt flags : bit 6 = T1 echu               */

#define ACR_T1_FREERUN 0x40   /* T1 recharge et repart tout seul */
#define IFR_T1         0x40

/* Commandes AY via ORB (valeurs standard Mockingboard) */
#define AY_INACTIVE 0x04
#define AY_LATCH    0x07   /* BDIR=1 BC1=1 : verrouille l'adresse de registre */
#define AY_WRITE    0x06   /* BDIR=1 BC1=0 : ecrit la donnee                  */
#define AY_RESET    0x00   /* RESET actif                                     */

/* Base de temps : T1 a 1,023 MHz. 20460 cycles ~= 50 Hz. */
#define TICK_CYCLES 20460

/* --- Etat --------------------------------------------------------------- */

static u16 via_base[2];    /* $Cn00 (AY musique) et $Cn80 (AY effets) */
static u8  mb_ready;       /* 1 si une carte a ete prise */

/* Les registres de l'AY sont en ECRITURE SEULE : impossible de relire pour
 * modifier un seul bit. On garde donc en RAM les deux seuls registres qui
 * demandent un read-modify-write. */
static u8 mixer[2];        /* registre 7 : bits 0-2 tons, 3-5 bruits (1 = coupe) */

/* Sequenceur */
static const u8 *trk[3];       /* position courante dans chaque piste */
static u8        trk_left[3];  /* ticks restants sur la note en cours */
static const MbTune *cur_tune;
static u8        music_loop;
static u8        music_on;

/* --- Acces bas niveau --------------------------------------------------- */

static void via_w(u8 ay, u8 off, u8 v)
{
    *(volatile u8 *)(via_base[ay] + off) = v;
}

static u8 via_r(u8 ay, u8 off)
{
    return *(volatile u8 *)(via_base[ay] + off);
}

/* Ecrit un registre de l'AY : on presente l'adresse, on verrouille, on
 * presente la donnee, on ecrit. Chaque etape repasse par INACTIVE. */
static void ay_w(u8 ay, u8 reg, u8 val)
{
    via_w(ay, VIA_ORA, reg);
    via_w(ay, VIA_ORB, AY_LATCH);
    via_w(ay, VIA_ORB, AY_INACTIVE);
    via_w(ay, VIA_ORA, val);
    via_w(ay, VIA_ORB, AY_WRITE);
    via_w(ay, VIA_ORB, AY_INACTIVE);
}

/* --- Detection ---------------------------------------------------------- */

/* Un 6522 repond-il a cette base ? On arme T1 et on verifie qu'il DECOMPTE
 * tout seul entre deux lectures. De la RAM rendrait deux fois la meme valeur ;
 * un slot vide rend du bus flottant. Seul un vrai timer descend. */
static u8 via_present(u16 base)
{
    volatile u8 *v = (volatile u8 *)base;
    u8 a, b;
    v[VIA_ACR]  = 0x00;        /* T1 one-shot, pas de sortie sur PB7 */
    v[VIA_T1CL] = 0xFF;
    v[VIA_T1CH] = 0xFF;        /* ecrire T1CH arme et lance le decompte */
    a = v[VIA_T1CL];
    b = v[VIA_T1CL];
    return (u8)(a != b);
}

u8 mb_probe(u8 slot)
{
    u16 base;
    if (slot < 1 || slot > 7)
        return 0;
    base = (u16)(0xC000 + ((u16)slot << 8));
    /* DEUX 6522, en $Cn00 et $Cn80 : signature de la Mockingboard. Une carte
     * a 6522 unique (certaines cartes imprimante) ne passe pas. */
    return (u8)(via_present(base) && via_present((u16)(base + 0x80)));
}

/* --- Initialisation ----------------------------------------------------- */

static void ay_reset(u8 ay)
{
    via_w(ay, VIA_DDRA, 0xFF);          /* ports en sortie */
    via_w(ay, VIA_DDRB, 0xFF);
    via_w(ay, VIA_ORB, AY_INACTIVE);
    via_w(ay, VIA_ORB, AY_RESET);
    via_w(ay, VIA_ORB, AY_INACTIVE);
    mixer[ay] = 0x3F;                   /* tout coupe */
    ay_w(ay, 7, mixer[ay]);
    ay_w(ay, 8, 0); ay_w(ay, 9, 0); ay_w(ay, 10, 0);
}

void mb_init(u8 slot)
{
    via_base[0] = (u16)(0xC000 + ((u16)slot << 8));
    via_base[1] = (u16)(via_base[0] + 0x80);
    ay_reset(0);
    ay_reset(1);

    /* Base de temps : T1 de la VIA #1 en free-run, IRQ DESARMEE. On lira son
     * drapeau dans mb_music_tick au lieu de subir une interruption. */
    via_w(0, VIA_IER, 0x7F);            /* toutes interruptions desarmees */
    via_w(0, VIA_ACR, ACR_T1_FREERUN);
    via_w(0, VIA_T1LL, (u8)(TICK_CYCLES & 0xFF));
    via_w(0, VIA_T1LH, (u8)(TICK_CYCLES >> 8));
    via_w(0, VIA_T1CH, (u8)(TICK_CYCLES >> 8));   /* arme et lance */

    mb_ready = 1;
    music_on = 0;
}

void mb_reset(void)
{
    if (!mb_ready)
        return;
    music_on = 0;
    ay_reset(0);
    ay_reset(1);
}

/* --- Voies -------------------------------------------------------------- */

/* Voie 0..5 -> (AY, voie locale 0..2). */
#define AY_OF(ch)  ((u8)((ch) >= 3))
#define LOC_OF(ch) ((u8)((ch) % 3))

void mb_tone(u8 ch, u16 period)
{
    u8 ay = AY_OF(ch), c = LOC_OF(ch);
    if (!mb_ready)
        return;
    ay_w(ay, (u8)(c * 2),     (u8)(period & 0xFF));
    ay_w(ay, (u8)(c * 2 + 1), (u8)((period >> 8) & 0x0F));
}

void mb_amp(u8 ch, u8 amp)
{
    if (!mb_ready)
        return;
    ay_w(AY_OF(ch), (u8)(8 + LOC_OF(ch)), (u8)(amp & 0x1F));
}

void mb_mix(u8 ch, u8 tone_on, u8 noise_on)
{
    u8 ay = AY_OF(ch), c = LOC_OF(ch);
    if (!mb_ready)
        return;
    /* Bits a 1 = COUPE, sur l'AY. On part du masque courant garde en RAM. */
    if (tone_on)  mixer[ay] &= (u8)~(1 << c);
    else          mixer[ay] |= (u8)(1 << c);
    if (noise_on) mixer[ay] &= (u8)~(8 << c);
    else          mixer[ay] |= (u8)(8 << c);
    ay_w(ay, 7, mixer[ay]);
}

void mb_noise(u8 ch, u8 period)
{
    if (!mb_ready)
        return;
    ay_w(AY_OF(ch), 6, (u8)(period & 0x1F));
}

void mb_env(u8 ch, u16 period, u8 shape)
{
    u8 ay = AY_OF(ch);
    if (!mb_ready)
        return;
    ay_w(ay, 11, (u8)(period & 0xFF));
    ay_w(ay, 12, (u8)(period >> 8));
    ay_w(ay, 13, (u8)(shape & 0x0F));   /* ecrire r13 REDECLENCHE l'enveloppe */
}

/* --- Notes -------------------------------------------------------------- */

/* AY cadence a 1 MHz : periode = 62500 / frequence. Trois octaves. */
static const u16 note_period[MB_NOTE_MAX] = {
     478,  451,  426,  402,  379,  358,  338,  319,  301,  284,  268,  253,
    /*    C3   Cd3    D3   Dd3    E3    F3   Fd3    G3   Gd3    A3   Ad3    B3 */
     239,  225,  213,  201,  190,  179,  169,  159,  150,  142,  134,  127,
    /*    C4   Cd4    D4   Dd4    E4    F4   Fd4    G4   Gd4    A4   Ad4    B4 */
     119,  113,  106,  100,   95,   89,   84,   80,   75,   71,   67,   63
    /*    C5   Cd5    D5   Dd5    E5    F5   Fd5    G5   Gd5    A5   Ad5    B5 */
};

u16 mb_note_period(u8 note)
{
    return (note < MB_NOTE_MAX) ? note_period[note] : 0;
}

/* --- Morceaux ----------------------------------------------------------- */
/*
 * Flux de deux octets : [note][duree en ticks], MB_END pour finir, MB_REST
 * pour un silence. A 50 Hz, 12 ticks font ~0,24 s. Les trois pistes durent
 * 204 ticks chacune, pour reboucler ensemble.
 *
 * Theme du menu : la mineur, lent, une melodie sur un cheminement de basse.
 */
static const u8 title_mel[] = {
     9,12,  12,12,  16,12,  14,12,       /* A3  C4  E4  D4 */
    12,12,  11,12,   9,24,  MB_REST,12,  /* C4  B3  A3  -- */
    16,12,  17,12,  16,12,  14,12,       /* E4  F4  E4  D4 */
    12,12,   9,24,  MB_REST,24,
    MB_END
};
static const u8 title_harm[] = {
    MB_REST,24,  16,24,  MB_REST,24,  19,24,   /* --  E4  --  G4 */
    MB_REST,24,  16,24,  MB_REST,24,  12,36,   /* --  E4  --  C4 */
    MB_END
};
static const u8 title_bass[] = {
     9,51,   5,51,   7,51,   9,51,       /* A3  F3  G3  A3 */
    MB_END
};

static const MbTune tune_title = {
    { title_mel, title_harm, title_bass },
    MB_ENV_DECAY,   /* chaque note attaque puis s'eteint */
    800             /* ~0,2 s de declin (256 * 800 / 1 MHz) */
};

const MbTune *mb_tune(u8 id)
{
    switch (id) {
    case MB_TUNE_TITLE: return &tune_title;
    default:            return 0;
    }
}

/* --- Sequenceur --------------------------------------------------------- */

void mb_music_stop(void)
{
    u8 c;
    music_on = 0;
    if (!mb_ready)
        return;
    for (c = 0; c < 3; ++c) {
        mb_amp(c, 0);
        mb_mix(c, 0, 0);
    }
}

u8 mb_music_active(void) { return music_on; }

void mb_music_play(const MbTune *tune, u8 loop)
{
    u8 c;
    if (!mb_ready || tune == 0)
        return;
    cur_tune   = tune;
    music_loop = loop;
    for (c = 0; c < 3; ++c) {
        trk[c]      = tune->track[c];
        trk_left[c] = 0;                /* 0 = il faut lire le prochain evenement */
    }
    mb_env(MB_CH_MUSIC, tune->env_period, tune->env_shape);
    music_on = 1;
}

/* Avance une piste d'un tick. Renvoie 1 tant qu'elle a de la matiere. */
static u8 track_tick(u8 c)
{
    u8 note, dur;

    if (trk[c] == 0)
        return 0;
    if (trk_left[c] > 0) {              /* note en cours : on laisse sonner */
        --trk_left[c];
        return 1;
    }
    note = *trk[c]++;
    if (note == MB_END) {
        trk[c] = 0;
        mb_amp(c, 0);
        mb_mix(c, 0, 0);
        return 0;
    }
    dur = *trk[c]++;
    trk_left[c] = (u8)(dur ? dur - 1 : 0);

    if (note == MB_REST) {
        mb_amp(c, 0);
        mb_mix(c, 0, 0);
    } else {
        mb_tone(c, mb_note_period(note));
        mb_mix(c, 1, 0);
        /* L'amplitude suit l'enveloppe : chaque note est reattaquee en
         * reecrivant r13, sinon toutes partageraient un seul declin. */
        mb_amp(c, MB_AMP_ENV);
        ay_w(0, 13, (u8)(cur_tune->env_shape & 0x0F));
    }
    return 1;
}

/* Avance le sequenceur d'UN tick, sans regarder le timer. Separe de
 * mb_music_tick parce que l'attente des effets consomme elle aussi le drapeau
 * T1 : si elle ne relayait pas, la musique serait affamee pendant chaque
 * bruitage. Les deux appelants detectent le tick, celui-ci l'applique. */
static void music_advance(void)
{
    u8 c, alive = 0;

    if (!music_on)
        return;
    for (c = 0; c < 3; ++c)
        alive |= track_tick(c);

    if (!alive) {
        if (music_loop)
            mb_music_play(cur_tune, 1);
        else
            mb_music_stop();
    }
}

/* Le tick est echu ? Lire T1CL acquitte le drapeau ; T1 etant en free-run, il
 * a deja recharge tout seul, donc on ne derive pas meme si l'appelant est
 * irregulier -- ce qu'une boucle d'attente clavier est par nature. */
static u8 tick_due(void)
{
    if ((via_r(0, VIA_IFR) & IFR_T1) == 0)
        return 0;
    (void)via_r(0, VIA_T1CL);
    return 1;
}

void mb_music_tick(void)
{
    if (!mb_ready || !music_on)
        return;
    if (tick_due())
        music_advance();
}

/* --- Effets ------------------------------------------------------------- */
/* Sur l'AY #2, pour ne jamais couper la musique. Bloquants et courts : on
 * scrute le meme tick T1 plutot qu'une boucle calibree a l'aveugle, donc les
 * durees sont de vraies durees -- et la musique continue d'avancer pendant
 * l'attente, puisqu'on l'appelle depuis la boucle. */

static void fx_wait(u8 ticks)
{
    while (ticks--) {
        while (!tick_due())
            ;
        music_advance();          /* la musique continue pendant le bruitage */
    }
}

/* Un ton sur la voie d'effets, avec enveloppe percussive. */
static void fx_note(u8 note, u8 amp, u8 ticks)
{
    mb_tone(MB_CH_FX, mb_note_period(note));
    mb_amp(MB_CH_FX, amp);
    mb_mix(MB_CH_FX, 1, 0);
    fx_wait(ticks);
    mb_amp(MB_CH_FX, 0);
}

/* Un souffle de bruit blanc a amplitude constante. */
static void fx_noise(u8 period, u8 amp, u8 ticks)
{
    mb_noise(MB_CH_FX, period);
    mb_amp(MB_CH_FX, amp);
    mb_mix(MB_CH_FX, 0, 1);
    fx_wait(ticks);
    mb_amp(MB_CH_FX, 0);
    mb_mix(MB_CH_FX, 0, 0);
}

/* Un coup SEC : bruit attaque a fond puis eteint par l'enveloppe materielle.
 * C'est l'enveloppe qui fait le "sec" -- a amplitude constante on obtient un
 * souffle plat qui s'arrete net, ce qui sonne comme une coupure, pas comme un
 * choc. `period` grave le bruit (0..31, grand = sourd), `decay` regle la
 * vitesse d'extinction (grand = plus long).
 *
 * L'enveloppe est celle de l'AY #2 : la musique de l'AY #1 n'est pas touchee. */
static void fx_knock(u8 period, u16 decay, u8 ticks)
{
    mb_noise(MB_CH_FX, period);
    mb_env(MB_CH_FX, decay, MB_ENV_DECAY);
    mb_amp(MB_CH_FX, MB_AMP_ENV);
    mb_mix(MB_CH_FX, 0, 1);
    fx_wait(ticks);
    mb_amp(MB_CH_FX, 0);
    mb_mix(MB_CH_FX, 0, 0);
}

/* Indices de notes (0 = C3) : C4=12 E4=16 G4=19 C5=24 E5=28 G5=31 */
#define N_C4 12
#define N_E4 16
#define N_G4 19
#define N_A4 21
#define N_C5 24
#define N_E5 28
#define N_G5 31
#define N_C3  0
#define N_G3  7

void mb_play(u8 id)
{
    u8 n;

    if (!mb_ready)
        return;

    switch (id) {
    case SND_SELECT: fx_note(N_A4, 10, 2); break;
    case SND_ERROR:  fx_note(N_G3, 12, 4); fx_note(N_C3, 12, 6); break;

    /* "TA-DAA" : quarte montante G4 -> C5, la seconde tenue. */
    case SND_WIN:    fx_note(N_G4, 13, 5);
                     fx_note(N_C5, 13, 18);
                     break;

    case SND_LOSE:   fx_note(N_G4, 13, 4); fx_note(N_E4, 13, 4);
                     fx_note(N_C3, 13, 10); break;
    case SND_PICKUP: fx_note(N_E5, 12, 3); fx_note(N_G5, 12, 5); break;
    case SND_HIT:    fx_knock(6, 250, 3); break;   /* claquement bref et clair */

    /* Scintillement : arpege montant + eclat aigu tenu. */
    case SND_MAGIC:  for (n = N_C4; n < N_C5; n = (u8)(n + 4))
                         fx_note(n, 11, 2);
                     fx_note(N_C5 + 11, 12, 6);
                     break;

    /* Porte : un battant qui claque. Un coup sourd et sec, plus juste que le
     * grincement descendant d'avant -- lequel bouclait sans fin, `n` etant
     * non signe : 1 - 2 valait 255, toujours > 0. */
    case SND_DOOR:   fx_knock(24, 900, 6); break;

    /* Page : froissement bref et discret. */
    case SND_PAGE:   fx_noise(24, 6, 2); break;

    default: break;
    }
    mb_mix(MB_CH_FX, 0, 0);
}

#else  /* hote, ou backend Mockingboard non compile */

u8   mb_probe(u8 slot)                  { (void)slot; return 0; }
void mb_init(u8 slot)                   { (void)slot; }
void mb_reset(void)                     { }
void mb_tone(u8 c, u16 p)               { (void)c; (void)p; }
void mb_amp(u8 c, u8 a)                 { (void)c; (void)a; }
void mb_mix(u8 c, u8 t, u8 n)           { (void)c; (void)t; (void)n; }
void mb_noise(u8 c, u8 p)               { (void)c; (void)p; }
void mb_env(u8 c, u16 p, u8 s)          { (void)c; (void)p; (void)s; }
u16  mb_note_period(u8 n)               { (void)n; return 0; }
const MbTune *mb_tune(u8 id)            { (void)id; return 0; }
void mb_music_play(const MbTune *t, u8 l) { (void)t; (void)l; }
void mb_music_stop(void)                { }
u8   mb_music_active(void)              { return 0; }
void mb_music_tick(void)                { }
void mb_play(u8 id)                     { (void)id; }

#endif
