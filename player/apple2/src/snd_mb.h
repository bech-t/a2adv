/* snd_mb.h -- pilote Mockingboard : 2x AY-3-8910 via 6522, effets + musique.
 *
 * ATTENTION : materiel NON teste ici. A valider sur emulateur Mockingboard
 * (AppleWin, MAME, Virtual II). L'API reste neutre sur l'hote.
 *
 * ---------------------------------------------------------------------------
 * Trois decisions structurent ce pilote.
 *
 * 1. AUCUNE DETECTION AUTOMATIQUE. Un balayage des slots ecrirait dans sept
 *    cartes inconnues. Le slot se choisit A LA MAIN dans le menu Options
 *    (snd_use_mockingboard). Il est en revanche VERIFIE avant usage : mb_probe
 *    exige deux 6522, en $Cn00 et $Cn80. On ne sonde donc que la ou on allait
 *    ecrire de toute facon.
 *
 * 2. AUCUNE INTERRUPTION. La base de temps est le timer T1 du 6522 en mode
 *    free-run, mais on SCRUTE son drapeau (IFR bit 6) au lieu d'armer l'IRQ.
 *    On obtient un tick regulier a 50 Hz sans toucher aux vecteurs ProDOS ni
 *    a la commutation main/aux du 80 colonnes -- exactement les deux endroits
 *    ou une IRQ mal maitrisee ferait tomber la machine.
 *
 * 3. LA MUSIQUE AVANCE PENDANT L'ATTENTE CLAVIER. mb_music_tick() est appele
 *    depuis la boucle d'attente de scr_getkey() (via scr_idle_hook). Dans un
 *    livre-jeu, la machine passe l'essentiel de son temps a attendre le
 *    joueur : c'est donc suffisant, et ca ne coute pas une ligne d'IRQ.
 *
 * Repartition des voies : la MUSIQUE occupe l'AY #1 (voies 0-2), les EFFETS
 * l'AY #2 (voies 3-5). Un bruitage ne coupe donc jamais la musique.
 * ---------------------------------------------------------------------------
 */
#ifndef A2ADV_SND_MB_H
#define A2ADV_SND_MB_H

#include "format.h"

/* --- Detection et initialisation ---------------------------------------- */

/* Y a-t-il une Mockingboard dans ce slot (1..7) ? Renvoie 1 si DEUX 6522
 * repondent, en $Cn00 et $Cn80 : c'est la signature de la carte, que ne
 * presente pas une carte a 6522 unique. Ne balaie rien. */
u8 mb_probe(u8 slot);

/* Prend la carte du slot donne : ports en sortie, les deux AY reset, silence,
 * et T1 arme en free-run comme base de temps. */
void mb_init(u8 slot);

/* Coupe tout : les six voies, les deux enveloppes, et la musique en cours. */
void mb_reset(void);

/* --- Voies -------------------------------------------------------------- */
/* Les voies 0..2 sont l'AY #1 (musique), 3..5 l'AY #2 (effets). */

#define MB_CH_MUSIC   0    /* premiere voie de musique */
#define MB_CH_FX      3    /* premiere voie d'effets   */
#define MB_AMP_ENV    0x10 /* amplitude "suivre l'enveloppe" (bit 4) */

void mb_tone(u8 ch, u16 period);              /* periode 12 bits (grand = grave) */
void mb_amp(u8 ch, u8 amp);                   /* 0..15, ou MB_AMP_ENV */
void mb_mix(u8 ch, u8 tone_on, u8 noise_on);  /* routage ton/bruit de la voie */
void mb_noise(u8 ch, u8 period);              /* periode de bruit 0..31 (par AY) */

/* Enveloppe materielle de l'AY portant cette voie. `shape` : voir MB_ENV_*.
 * Une voie mise a MB_AMP_ENV suit alors cette enveloppe au lieu d'une
 * amplitude fixe -- c'est ce qui donne une attaque et un declin, au lieu d'un
 * creneau qui demarre et s'arrete net. */
#define MB_ENV_DECAY   0x00   /* \___  percussif : attaque puis extinction */
#define MB_ENV_ATTACK  0x0C   /* /|/|  montee repetee */
#define MB_ENV_SWELL   0x0D   /* /---  montee puis tenue */
#define MB_ENV_TRI     0x0E   /* /\/\  triangle continu */
void mb_env(u8 ch, u16 period, u8 shape);

/* --- Notes -------------------------------------------------------------- */
/* Index de note : 0 = C3, 35 = B5 (trois octaves chromatiques). MB_REST dans
 * un flux de musique = silence. */
#define MB_NOTE_MAX  36
#define MB_REST      0xFE
#define MB_END       0xFF

u16 mb_note_period(u8 note);   /* index -> periode AY ; 0 si hors table */

/* --- Musique ------------------------------------------------------------ */
/*
 * Une piste est un flux d'evenements de DEUX octets : [note][duree en ticks].
 * `note` vaut un index 0..35, MB_REST pour un silence, MB_END pour finir.
 * A 50 Hz, une duree de 25 fait une demi-seconde.
 *
 * Trois pistes au plus (les trois voies de l'AY #1). Une piste absente est un
 * pointeur nul. Format volontairement plat : pas de compression, pas de
 * repetition -- une melodie de trente notes tient dans 60 octets, ce qui ne
 * justifie pas d'encodeur.
 */
typedef struct {
    const u8 *track[3];   /* flux par voie ; NULL = voie inutilisee */
    u8        env_shape;  /* enveloppe appliquee aux notes (MB_ENV_*) */
    u16       env_period; /* periode d'enveloppe : grand = declin lent */
} MbTune;

/* Morceaux embarques dans le player. Le jour ou une aventure portera ses
 * propres musiques, elles arriveront par la meme structure MbTune -- seule la
 * source des octets changera. */
#define MB_TUNE_TITLE 1
const MbTune *mb_tune(u8 id);   /* 0 si l'id est inconnu */

void mb_music_play(const MbTune *tune, u8 loop);  /* loop != 0 : reboucle */
void mb_music_stop(void);
u8   mb_music_active(void);

/* Fait avancer la musique si le tick T1 est echu. Appel FREQUENT et non
 * bloquant : depuis la boucle d'attente clavier. Ne fait rien si aucune
 * musique n'est en cours ou si la carte est absente. */
void mb_music_tick(void);

/* --- Effets ------------------------------------------------------------- */

/* Joue un effet predefini (memes ids que snd_play : SND_*) sur l'AY #2.
 * Bloquant et court, mais la musique de l'AY #1 continue pendant ce temps. */
void mb_play(u8 id);

#endif /* A2ADV_SND_MB_H */
