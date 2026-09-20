/* snd.c -- moteur son haut-parleur ($C030).
 *
 * Portable : sur hote (gcc, test) les fonctions sont neutres (pas d'acces
 * materiel). Sur Apple II (cc65), on bascule le haut-parleur en boucle calibree.
 * Les valeurs de pitch/duree sont indicatives et se reglent a l'oreille.
 *
 * Pas de musique de fond possible sur ce materiel (un haut-parleur 1 bit
 * bloquerait le jeu le temps de la jouer) : snd_menu_music() ne fait rien.
 * L'intro (snd_intro) est en assembleur, cf. snd_intro.s. */

#include "snd.h"

void snd_menu_music(u8 on)
{
    (void)on;             /* haut-parleur 1 bit : pas de musique de fond possible */
}

/* snd_intro (cf. snd.h) : definie en assembleur sur cc65 (snd_intro.s), en
 * bouchon C ci-dessous sur l'hote. */

#ifdef __CC65__

#define SPKR  (*(volatile unsigned char *)0xC030)

static void snd_tone(u8 pitch, u16 dur)
{
    u16 i;
    volatile u8 d;
    for (i = 0; i < dur; ++i) {
        SPKR = 0;                   /* acces $C030 -> bascule la membrane.
                                     * Ecriture (pas lecture) : jamais optimisee. */
        for (d = 0; d < pitch; ++d) /* demi-periode -> hauteur du ton */
            ;
    }
}

/* Silence de meme duree que snd_tone(pitch, dur) : meme boucle, sans toucher
 * au haut-parleur. Sert a detacher deux notes (sinon elles se collent). */
static void snd_rest(u8 pitch, u16 dur)
{
    u16 i;
    volatile u8 d;
    for (i = 0; i < dur; ++i)
        for (d = 0; d < pitch; ++d)
            ;
}

#else  /* hote : pas de materiel */

void snd_intro(void)
{
}

static void snd_tone(u8 pitch, u16 dur)
{
    (void)pitch;
    (void)dur;
}

static void snd_rest(u8 pitch, u16 dur)
{
    (void)pitch;
    (void)dur;
}

#endif

/* Reperes de reglage (Apple II a 1,023 MHz, mesures a la boucle ci-dessus) :
 *   frequence ~= 1023000 / (40*pitch + 60)   -> pitch grand = grave
 *   duree(ms) ~= dur * (40*pitch + 60) / 2046
 * Quelques notes :  C4=96  E4=76  G4=64  A4=57  C5=47  E5=37  G5=31  C6=23
 */
void snd_play(u8 id)
{
    u8 p;

    switch (id) {
    case SND_SELECT: snd_tone(40, 40); break;
    case SND_ERROR:  snd_tone(150, 60); snd_tone(190, 70); break;

    /* "TA-DAA" : quarte montante G4 -> C5, la seconde note tenue. */
    case SND_WIN:    snd_tone(64, 71);          /* ta  : G4, ~90 ms  */
                     snd_rest(47, 36);          /* respiration       */
                     snd_tone(47, 439);         /* daa : C5, ~420 ms */
                     break;

    case SND_LOSE:   snd_tone(55, 45); snd_tone(85, 45); snd_tone(140, 90); break;
    case SND_PICKUP: snd_tone(55, 25); snd_tone(35, 35); break;
    case SND_HIT:    snd_tone(200, 45); break;

    /* Scintillement : glissando montant rapide + eclat aigu tenu. */
    case SND_MAGIC:  for (p = 96; p > 26; p = (u8)(p - 7))
                         snd_tone(p, 14);
                     snd_tone(23, 90);          /* C6 final */
                     break;

    /* Porte : un battant qui claque. Deux impulsions tres graves collees --
     * c'est tout ce qu'un haut-parleur 1 bit peut faire passer pour un choc.
     * Le grincement descendant d'avant durait trop et sonnait faux. */
    case SND_DOOR:   snd_tone(230, 12); snd_tone(255, 30); break;

    /* Page : froissement bref (3 clics descendants, ~50 ms au total). */
    case SND_PAGE:   snd_tone(40, 14); snd_tone(64, 12); snd_tone(100, 10); break;

    /* Pressentiment : tremolo grave (deux tons graves proches, alternes
     * vite) qui s'installe en une tenue plus grave encore -- un frisson,
     * pas un choc (SND_HIT) ni un sortilege (SND_MAGIC). */
    case SND_DREAD:  for (p = 0; p < 4; ++p) { snd_tone(150, 8); snd_tone(170, 8); }
                     snd_tone(190, 60);
                     break;

    /* Bonus : petit arpege ascendant a trois notes, plus court et plus
     * scintillant que SND_PICKUP (deux notes) -- gain de stat/ressource
     * abstrait, sans objet physique a ramasser. */
    case SND_BONUS:  snd_tone(60, 16); snd_tone(45, 16); snd_tone(30, 28); break;

    default: break;
    }
}
