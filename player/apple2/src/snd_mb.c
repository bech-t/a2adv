/* snd_mb.c -- backend son Mockingboard (cf. snd_mb.h).
 *
 * Materiel : carte Mockingboard = 2x AY-3-8910 pilotes chacun par un 6522 (VIA).
 * On n'utilise QUE la VIA #1 / l'AY "gauche" (offset 0 du slot) : suffisant pour
 * de petits bruitages mono. Les sons sont BLOQUANTS (pose des registres + delai
 * + coupe), ce qui convient a un livre-jeu au tour par tour.
 *
 * NON teste sur cette machine : a valider sur emulateur Mockingboard.
 */

#include "snd_mb.h"

#ifdef __CC65__

/* --- Registres 6522 (offsets depuis la base de slot $Cn00) --------------- */
#define VIA_ORB   0x00     /* port B : lignes de controle AY (BDIR/BC1/RESET) */
#define VIA_ORA   0x01     /* port A : bus de donnees/adresse vers l'AY       */
#define VIA_DDRB  0x02
#define VIA_DDRA  0x03
#define VIA_T1CL  0x04
#define VIA_T1CH  0x05
#define VIA_ACR   0x0B

/* Commandes AY via ORB (valeurs standard Mockingboard) */
#define AY_INACTIVE 0x04
#define AY_LATCH    0x07   /* BDIR=1 BC1=1 : verrouille l'adresse de registre */
#define AY_WRITE    0x06   /* BDIR=1 BC1=0 : ecrit la donnee                  */
#define AY_RESET    0x00   /* RESET actif                                     */

static u16 mb_base;        /* $Cn00 de la VIA #1 */

static void via_w(u8 off, u8 v) { *(volatile u8 *)(mb_base + off) = v; }

/* Ecrit une valeur dans un registre de l'AY (latch adresse puis write). */
static void ay_w(u8 reg, u8 val)
{
    via_w(VIA_ORA, reg);
    via_w(VIA_ORB, AY_LATCH);
    via_w(VIA_ORB, AY_INACTIVE);
    via_w(VIA_ORA, val);
    via_w(VIA_ORB, AY_WRITE);
    via_w(VIA_ORB, AY_INACTIVE);
}

/* Petit delai bloquant (unites ~ centaines de ms selon 'ticks'). */
static void mb_delay(u16 ticks)
{
    u16 i;
    volatile u16 j;
    for (i = 0; i < ticks; ++i)
        for (j = 0; j < 1200; ++j)
            ;
}

static void mb_silence(void)
{
    ay_w(8, 0); ay_w(9, 0); ay_w(10, 0);   /* amplitudes A/B/C = 0 */
    ay_w(7, 0x3F);                          /* mixer : tous tons+bruits coupes */
}

/* Un ton carre sur la voie A : period = 12 bits (grand = grave). */
static void mb_note(u16 period, u8 ampl, u16 dur)
{
    ay_w(0, (u8)(period & 0xFF));           /* voie A : periode fine */
    ay_w(1, (u8)((period >> 8) & 0x0F));    /* voie A : periode grossiere */
    ay_w(8, (u8)(ampl & 0x0F));             /* amplitude A */
    ay_w(7, 0x3E);                          /* mixer : ton A actif (bit0=0) */
    mb_delay(dur);
    ay_w(8, 0);                             /* coupe A */
}

/* Un souffle de bruit blanc sur la voie A (percussif). */
static void mb_noise(u16 dur)
{
    ay_w(6, 0x0F);                          /* periode de bruit */
    ay_w(8, 0x0C);                          /* amplitude A */
    ay_w(7, 0x37);                          /* mixer : bruit A actif (bit3=0) */
    mb_delay(dur);
    ay_w(8, 0);
}

void mb_init(u8 slot)
{
    mb_base = (u16)(0xC000 + ((u16)slot << 8));
    via_w(VIA_DDRA, 0xFF);      /* port A en sortie */
    via_w(VIA_DDRB, 0xFF);      /* port B en sortie */
    via_w(VIA_ORB, AY_INACTIVE);
    via_w(VIA_ORB, AY_RESET);   /* reset AY */
    via_w(VIA_ORB, AY_INACTIVE);
    mb_silence();
}

/* Reperes de reglage (AY cadence a 1 MHz) : freq ~= 62500 / period, et
 * mb_delay(1) ~= 30 ms. Quelques notes :
 *   C4=239  E4=189  G4=159  A4=142  C5=120  E5=95  G5=80  C6=60
 */
void mb_play(u8 id)
{
    u16 pr;

    switch (id) {
    case SND_SELECT: mb_note(145, 10, 1); break;
    case SND_ERROR:  mb_note(290, 12, 2); mb_note(360, 12, 3); break;

    /* "TA-DAA" : quarte montante G4 -> C5, la seconde note tenue. */
    case SND_WIN:    mb_note(159, 13, 3);       /* ta  : G4  */
                     mb_delay(1);               /* respiration */
                     mb_note(120, 13, 12);      /* daa : C5 tenu */
                     break;

    case SND_LOSE:   mb_note(145, 13, 2); mb_note(217, 13, 2); mb_note(360, 13, 4); break;
    case SND_PICKUP: mb_note(200, 12, 1); mb_note(120, 12, 2); break;
    case SND_HIT:    mb_noise(3); break;

    /* Scintillement : glissando montant + eclat aigu tenu. */
    case SND_MAGIC:  for (pr = 300; pr > 70; pr = (u16)(pr - 30))
                         mb_note(pr, 11, 1);
                     mb_note(60, 12, 3);        /* C6 final */
                     break;

    /* Porte : grincement (glissando descendant) puis battant (bruit). */
    case SND_DOOR:   for (pr = 500; pr < 900; pr = (u16)(pr + 80))
                         mb_note(pr, 10, 1);
                     mb_noise(3);
                     break;

    /* Page : froissement bref et discret (amplitude faible). */
    case SND_PAGE:   mb_note(90, 7, 1); mb_note(140, 6, 1); break;

    default: break;
    }
    mb_silence();
}

#else  /* hote : pas de materiel */

void mb_init(u8 slot)     { (void)slot; }
void mb_play(u8 id)       { (void)id; }

#endif
