/* snd.c -- moteur son haut-parleur ($C030).
 *
 * Portable : sur hote (gcc, test) les fonctions sont neutres (pas d'acces
 * materiel). Sur Apple II (cc65), on bascule le haut-parleur en boucle calibree.
 * Les valeurs de pitch/duree sont indicatives et se reglent a l'oreille.
 */

#include "snd.h"
#include "scr.h"   /* scr_idle_hook : la musique avance pendant l'attente */

/* Backend Mockingboard : optionnel, active a la compilation par
 * -DA2ADV_MOCKINGBOARD (cf. Makefile MOCKINGBOARD=1). Sinon : haut-parleur seul,
 * et snd_mb.c n'est pas linke. */
#if defined(__CC65__) && defined(A2ADV_MOCKINGBOARD)
#include "snd_mb.h"
#define MB_ENABLED 1
#else
#define MB_ENABLED 0
#endif

u8 snd_backend = 0;      /* 0 = haut-parleur (defaut), 1 = Mockingboard */
u8 snd_mb_slot = 0;      /* slot Mockingboard actif (1..7), 0 = haut-parleur */

void snd_use_mockingboard(u8 slot)
{
#if MB_ENABLED
    /* On SONDE le slot que le joueur vient de designer avant d'y ecrire pour
     * de bon. Ce n'est pas la detection automatique qu'on refuse : celle-la
     * balaierait sept slots inconnus. Ici on ne touche qu'a celui qu'il a
     * choisi, et une faute de frappe retombe sur le haut-parleur au lieu de
     * pousser des octets dans une carte quelconque.
     *
     * Le refus n'a pas besoin de message : le menu Options affiche la sortie
     * courante, qui restera "HAUT-PARLEUR". C'est le retour. */
    if (slot && mb_probe(slot)) {
        mb_init(slot);
        snd_backend = 1;
        snd_mb_slot = slot;
        /* La musique avancera desormais dans chaque attente clavier. */
        scr_idle_hook = mb_music_tick;
    } else {
        mb_music_stop();
        scr_idle_hook = 0;
        snd_backend = 0;
        snd_mb_slot = 0;
    }
#else
    (void)slot;
#endif
}

void snd_music(u8 id)
{
#if MB_ENABLED
    const MbTune *t;
    if (!snd_backend)                  /* haut-parleur : pas de musique de fond */
        return;
    t = (id == MUS_NONE) ? 0 : mb_tune(id);
    if (t) mb_music_play(t, 1);        /* en boucle */
    else   mb_music_stop();
#else
    (void)id;
#endif
}

#ifdef __CC65__

#define SPKR  (*(volatile unsigned char *)0xC030)

void snd_tone(u8 pitch, u16 dur)
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

void snd_tone(u8 pitch, u16 dur)
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

#if MB_ENABLED
    if (snd_backend) { mb_play(id); return; }   /* backend Mockingboard */
#endif
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

    default: break;
    }
}
