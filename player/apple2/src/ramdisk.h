/* ramdisk.h -- cache des STORYnn.DAT dans le volume ProDOS /RAM (cf. spec §7quater).
 *
 * Sur 128 Ko (//e / //c), ProDOS cree un volume /RAM en memoire auxiliaire
 * (~119 blocs, ~59,5 Ko). Y recopier les STORYnn.DAT supprime les acces
 * disquette pendant la partie. Tout est en "meilleur effort" : une copie qui
 * echoue (pas de /RAM, volume plein, plus de tampon ProDOS) laisse simplement
 * le fichier sur la disquette -- jamais d'erreur remontee au joueur.
 */
#ifndef A2ADV_RAMDISK_H
#define A2ADV_RAMDISK_H

#include "format.h"
#include "scr.h"

/* Remplissage maximal au boot, a partir du fichier 'from' vers les suivants.
 * A APPELER AVANT story_open() : seuls 2 tampons ProDOS sont alors necessaires
 * (source + destination). Affiche une barre de progression si cb != NULL.
 * Sans effet si /RAM est absent. */
void ram_boot_fill(u8 from, scr_progress_cb cb);

/* Entretien de la fenetre glissante : garantit que le fichier 'id' est en /RAM,
 * en evinçant au besoin le fichier cache le plus eloigne de 'id'.
 * A APPELER FICHIER FERME (cf. story.c, avant open_file). No-op si deja cache
 * ou si /RAM est absent. */
void ram_ensure(u8 id);

/* Le fichier id est-il disponible en /RAM ? */
u8 ram_has(u8 id);

/* Chemin "/RAM/STORYnn.DAT" (tampon statique, valide jusqu'au prochain appel). */
const char *ram_path(u8 id);

#endif /* A2ADV_RAMDISK_H */
