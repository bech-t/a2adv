/* snd.c -- pilote Sound Blaster (DSP 8 bits + DMA), cf. snd.h.
 *
 * Detection : variable d'environnement BLASTER ("A220 I5 D1 T4", etc.),
 * exposee a l'identique par DOSBox et par les pilotes SB d'origine --
 * A<port E/S en HEX> I<IRQ> D<canal DMA 8 bits> [H<canal DMA 16 bits>]
 * T<type de carte>. Aucune sonde materielle a l'aveugle : IRQ/H/T sont
 * ignores (pas besoin de savoir le type de carte pour du 8 bits basique,
 * pas d'IRQ utilisee ici, cf. plus bas).
 *
 * Portee volontairement reduite (premier jet) : uniquement des bruitages
 * courts synthetises a la volee (onde carree ou bruit 8 bits, sortie DMA
 * "single-cycle" bloquante, cf. snd_play/snd_intro) -- pas de musique de
 * fond (snd_menu_music reste un bouchon) : un flux qui boucle sans bloquer
 * le jeu demanderait une lecture DMA auto-init pilotee par l'IRQ du DSP,
 * hors de portee ici. Meme esprit que atarist/src/snd.c : chaque
 * plateforme programme son propre materiel a la main, aucune librairie
 * externe (aucune ne convient a ce modele memoire/format de toute facon).
 *
 * ATTENTION : DSP + DMA non verifies au son (ni carte reelle, ni capture
 * audio DOSBox possible dans cet environnement de developpement -- meme
 * limite que l'absence de capture d'ecran, cf. player/dos/README.md) :
 * seuls la detection BLASTER et le buffer genere ont ete relus. A
 * confirmer a l'oreille avant de considerer ce pilote acquis. */

#include <i86.h>
#include <dos.h>
#include <conio.h>
#include <stdlib.h>
#include "snd.h"
#include "scr.h"      /* scr_frclock() : base de temps des pauses bloquantes */

/* --- Detection BLASTER ---------------------------------------------------- */

static u16 dsp_port;
static u8  dsp_dma;

static u8 hexval(char c)
{
    if (c >= '0' && c <= '9') return (u8)(c - '0');
    if (c >= 'A' && c <= 'F') return (u8)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (u8)(c - 'a' + 10);
    return 0xFF;
}

/* Ne retient que A (port, en HEX) et D (canal DMA 8 bits, en decimal) --
 * seuls champs necessaires a une sortie DMA simple sans IRQ. Les autres
 * champs (I/H/T) sont juste sautes jusqu'au prochain espace. */
static u8 parse_blaster(u16 *port, u8 *dma)
{
    char *b = getenv("BLASTER");
    u8 have_port = 0, have_dma = 0;

    if (!b)
        return 0;
    while (*b) {
        while (*b == ' ')
            ++b;
        if (*b == 'A') {
            u16 v = 0;
            u8 h;
            ++b;
            while ((h = hexval(*b)) != 0xFF) { v = (u16)(v * 16 + h); ++b; }
            *port = v;
            have_port = 1;
        } else if (*b == 'D') {
            u16 v = 0;
            ++b;
            while (*b >= '0' && *b <= '9') { v = (u16)(v * 10 + (*b - '0')); ++b; }
            *dma = (u8)v;
            have_dma = 1;
        } else if (*b) {
            ++b;
            while (*b && *b != ' ')
                ++b;
        }
    }
    return (u8)(have_port && have_dma);
}

/* --- DSP (registres E/S base+0x6/0xA/0xC/0xE, cf. doc Creative) ---------- */

static void io_delay(u8 n)          /* port 0x80 : diagnostic POST libre, ~1us/acces */
{
    while (n--)
        (void)inp(0x80);
}

/* Bouclages BORNES (pas de while(1) sur un port materiel) : une carte
 * absente ou un BLASTER errone ne doivent jamais figer le jeu, juste
 * echouer la detection. */
static u8 dsp_out(u16 port, u8 val)
{
    u16 i;
    for (i = 0; i < 0xFFFF; ++i)
        if (!(inp((u16)(port + 0xC)) & 0x80)) {
            outp((u16)(port + 0xC), val);
            return 1;
        }
    return 0;
}

static u8 dsp_in(u16 port, u8 *val)
{
    u16 i;
    for (i = 0; i < 0xFFFF; ++i)
        if (inp((u16)(port + 0xE)) & 0x80) {
            *val = (u8)inp((u16)(port + 0xA));
            return 1;
        }
    return 0;
}

static u8 dsp_reset(u16 port)
{
    u8 v;
    outp((u16)(port + 0x6), 1);
    io_delay(4);                    /* >= 3us exiges par le DSP avant relachement */
    outp((u16)(port + 0x6), 0);
    return dsp_in(port, &v) && v == 0xAA;
}

/* --- Tampon DMA-safe -------------------------------------------------------
 * Le controleur 8237 ne franchit jamais une frontiere physique de 64 Ko en
 * cours de transfert (le registre de page ne s'incremente pas) : le tampon
 * doit donc commencer PILE sur une telle frontiere pour qu'un transfert de
 * MAX_SAMPLES octets (tous < 64 Ko) ne puisse jamais la franchir. Alloue
 * via DOS (INT 21h/48h, qui rend un segment quelconque) avec une marge de
 * 64 Ko : la frontiere suivante tombe forcement dans le bloc obtenu, on n'a
 * plus qu'a la trouver (segment multiple de 0x1000 <=> adresse physique
 * multiple de 0x10000) -- jamais libere, comme le tampon STORY.DAT
 * (cf. diskio.c), le jeu tourne seul jusqu'a la sortie. */
#define MAX_SAMPLES     4096U
#define SLACK_PARAS     0x1000U     /* 64 Ko de marge, en paragraphes de 16 o */

static unsigned char *dma_buf;
static u16 dma_seg;

static u8 dma_alloc(u16 bytes)
{
    union REGS r;
    u16 paras = (u16)(((bytes + 15) / 16) + SLACK_PARAS);

    r.h.ah = 0x48;
    r.x.bx = paras;
    int86(0x21, &r, &r);
    if (r.x.cflag)
        return 0;
    dma_seg = r.x.ax;
    if (dma_seg & (u16)(SLACK_PARAS - 1))
        dma_seg = (u16)((dma_seg + SLACK_PARAS) & ~(u16)(SLACK_PARAS - 1));
    dma_buf = (unsigned char *)MK_FP(dma_seg, 0);
    return 1;
}

/* --- Canal DMA 8 bits (controleur 1, ports 0x00-0x0F/0x81-0x87) ---------- */

static const u16 dma_addr_port[4]  = {0x00, 0x02, 0x04, 0x06};
static const u16 dma_count_port[4] = {0x01, 0x03, 0x05, 0x07};
static const u16 dma_page_port[4]  = {0x87, 0x83, 0x81, 0x82};

static void dma_program(u16 len)
{
    u32 phys = (u32)dma_seg << 4;   /* offset 0 dans dma_buf : toujours multiple de 64 Ko */
    u8  chan = (u8)(dsp_dma & 3);

    outp(0x0A, (u8)(4 | chan));               /* masque le canal pendant la reprogrammation */
    outp(0x0C, 0);                            /* reinitialise le bascule (flip-flop) adresse/octets */
    outp(0x0B, (u8)(0x48 | chan));            /* cycle simple, lecture memoire -> peripherique */
    outp(dma_addr_port[chan],  (u8)(phys & 0xFF));
    outp(dma_addr_port[chan],  (u8)((phys >> 8) & 0xFF));
    outp(dma_page_port[chan],  (u8)((phys >> 16) & 0xFF));
    outp(dma_count_port[chan], (u8)((len - 1) & 0xFF));
    outp(dma_count_port[chan], (u8)(((len - 1) >> 8) & 0xFF));
    outp(0x0A, chan);                         /* demasque : le transfert peut commencer */
}

/* --- Lecture DSP 8 bits, cycle simple (commande 0x14, toutes versions SB) */

#define SAMPLE_RATE  8000U

static void dsp_play(u16 len)
{
    u8 tc = (u8)(256 - (1000000UL / SAMPLE_RATE));   /* constante de temps DSP */

    dma_program(len);
    dsp_out(dsp_port, 0x40); dsp_out(dsp_port, tc);
    dsp_out(dsp_port, 0x14);
    dsp_out(dsp_port, (u8)((len - 1) & 0xFF));
    dsp_out(dsp_port, (u8)(((len - 1) >> 8) & 0xFF));
}

/* --- Generation des echantillons (PCM 8 bits non signe, silence = 0x80) - */

static u16 gen_tone(u16 freq_hz, u16 ms)     /* onde carree ; freq_hz=0 -> silence */
{
    u16 n = (u16)(((u32)SAMPLE_RATE * ms) / 1000UL);
    u16 i;

    if (n > MAX_SAMPLES)
        n = MAX_SAMPLES;
    if (!freq_hz) {
        for (i = 0; i < n; ++i)
            dma_buf[i] = 0x80;
    } else {
        u16 half = (u16)(SAMPLE_RATE / (2U * freq_hz));
        u8  level = 0xC0;
        u16 cnt = 0;
        if (!half)
            half = 1;
        for (i = 0; i < n; ++i) {
            dma_buf[i] = level;
            if (++cnt >= half) {
                cnt = 0;
                level = (u8)(level == 0xC0 ? 0x40 : 0xC0);
            }
        }
    }
    return n;
}

static u16 gen_noise(u16 ms)                 /* LFSR 16 bits (Galois, poly 0xB400) */
{
    static u16 seed = 0xACE1;
    u16 n = (u16)(((u32)SAMPLE_RATE * ms) / 1000UL);
    u16 i;

    if (n > MAX_SAMPLES)
        n = MAX_SAMPLES;
    for (i = 0; i < n; ++i) {
        seed = (u16)((seed >> 1) ^ (u16)(-(seed & 1) & 0xB400));
        dma_buf[i] = (u8)(0x80 + (u8)(seed & 0x3F) - 0x20);
    }
    return n;
}

/* --- Effets bloquants : generer, lancer, attendre la duree jouee -------- */

#define TICK_MS  55U   /* compteur BIOS 0040:006C, ~18.2 Hz (cf. scr_frclock) */

static void wait_ms(u16 ms)
{
    u32 target = scr_frclock() + (u32)(ms / TICK_MS) + 1;   /* +1 tick de marge */
    while (scr_frclock() < target)
        ;
}

static u8 sb_ready;

static void fx_tone(u16 freq_hz, u16 ms)
{
    u16 n;
    if (!sb_ready)
        return;
    n = gen_tone(freq_hz, ms);
    if (!n)
        return;
    dsp_play(n);
    wait_ms(ms);
}

static void fx_noise(u16 ms)
{
    u16 n;
    if (!sb_ready)
        return;
    n = gen_noise(ms);
    if (!n)
        return;
    dsp_play(n);
    wait_ms(ms);
}

static void ensure_ready(void)
{
    static u8 tried;
    if (tried)
        return;
    tried = 1;
    if (!parse_blaster(&dsp_port, &dsp_dma))
        return;
    if (!dsp_reset(dsp_port))
        return;
    if (!dma_alloc(MAX_SAMPLES))
        return;
    sb_ready = 1;
}

/* --- API commune (cf. snd.h) ---------------------------------------------- */

void snd_intro(void)
{
    ensure_ready();
    fx_tone(262, 120);   /* C4 E4 G4 -- arpege montant */
    fx_tone(330, 120);
    fx_tone(392, 120);
    fx_tone(523, 500);   /* C5 tenu */
}

/* Pas de musique de fond : demanderait une lecture DMA auto-init pilotee
 * par l'IRQ du DSP (flux qui boucle sans bloquer scr_getkey/le jeu) --
 * hors de portee de ce premier pilote, cf. entete du fichier. */
void snd_menu_music(u8 on)
{
    (void)on;
}

void snd_play(u8 id)
{
    u8 n;

    ensure_ready();
    if (!sb_ready)
        return;

    switch (id) {
    case SND_SELECT: fx_tone(440, 40); break;                        /* A4 */
    case SND_ERROR:  fx_tone(196, 90); fx_tone(131, 130); break;      /* G3 C3 */

    /* "TA-DAA" : quarte montante G4 -> C5, la seconde note tenue. */
    case SND_WIN:    fx_tone(392, 90); fx_tone(523, 380); break;

    case SND_LOSE:   fx_tone(392, 80); fx_tone(330, 80); fx_tone(131, 200); break;
    case SND_PICKUP: fx_tone(659, 60); fx_tone(784, 90); break;
    case SND_HIT:    fx_noise(35); break;

    /* Scintillement : glissando montant rapide + eclat aigu tenu. */
    case SND_MAGIC:  for (n = 0; n < 6; ++n)
                         fx_tone((u16)(262 + n * 40), 30);
                     fx_tone(659, 120);
                     break;

    case SND_DOOR:   fx_tone(90, 30); fx_noise(60); break;    /* choc grave + battant */
    case SND_PAGE:   fx_noise(50); break;                     /* froissement bref */

    /* Pressentiment : tremolo grave (deux tons voisins alternes) puis un
     * grondement de bruit -- un frisson qui s'installe, pas un choc
     * (SND_HIT) ni un sortilege (SND_MAGIC). */
    case SND_DREAD:  fx_tone(131, 45); fx_tone(139, 45);
                     fx_tone(131, 45); fx_tone(139, 45);
                     fx_noise(220);
                     break;

    /* Bonus : arpege ascendant a trois notes, plus court et plus clair que
     * SND_PICKUP (deux notes) -- gain de stat/ressource abstrait, sans
     * objet physique a ramasser. */
    case SND_BONUS:  fx_tone(659, 60); fx_tone(784, 60); fx_tone(988, 90); break;

    default: break;
    }
}
