/* ramdisk.h -- cache des STORYnn.DAT en disque RAM ProDOS (cf. spec §7quater).
 *
 * Sur 128 Ko (//e / //c), ProDOS cree un volume /RAM en memoire auxiliaire
 * (~119 blocs, ~59,5 Ko). Y recopier les STORYnn.DAT supprime les acces
 * disquette pendant la partie.
 *
 * DEUX volumes sont utilises quand ils existent : /RAM d'abord, puis /RAM2 --
 * ce dernier apparait sur les //e a carte memoire etendue, dont le pilote
 * publie un volume supplementaire. Une aventure qui debordait de /RAM tient
 * alors entierement en memoire. Rien d'autre dans le player n'a a le savoir :
 * ram_path() rend le bon chemin selon l'endroit ou le fichier a atterri.
 *
 * Tout reste en "meilleur effort" : une copie qui echoue (aucun volume, tous
 * pleins, plus de tampon ProDOS) laisse simplement le fichier sur la
 * disquette -- jamais d'erreur remontee au joueur.
 */
#ifndef A2ADV_RAMDISK_H
#define A2ADV_RAMDISK_H

#include "format.h"
#include "scr.h"

/* Remplissage maximal au boot, en enchainant /RAM puis /RAM2 :
 *   1. les STORYnn.DAT a partir de 'from' -- relus a chaque section, ils
 *      passent avant tout le reste ;
 *   2. puis MENU.HGR et les IMGnn.HGR, tant qu'il reste de la place.
 * S'arrete des qu'aucun volume n'accepte plus ; le reste demeure sur la
 * disquette. Les splashes BOOTnn.HGR sont exclus : vus une seule fois au
 * demarrage, leur place sert mieux aux images du jeu.
 * A APPELER AVANT story_open() : seuls 2 tampons ProDOS sont alors necessaires
 * (source + destination). Affiche une barre de progression si cb != NULL.
 * Sans effet si /RAM est absent. */
void ram_boot_fill(u8 from, scr_progress_cb cb);

/* Entretien de la fenetre glissante : garantit que le fichier 'id' est en /RAM,
 * en evinçant au besoin le fichier cache le plus eloigne de 'id'.
 * A APPELER FICHIER FERME (cf. story.c, avant open_file). No-op si deja cache
 * ou si /RAM est absent. */
void ram_ensure(u8 id);

/* Le fichier id est-il disponible en disque RAM (quel que soit le volume) ? */
u8 ram_has(u8 id);

/* --- Fichiers quelconques (images) -------------------------------------- */
/*
 * Les images sont le vrai poste d'attente : 8 192 o piece, contre ~1 Ko pour
 * une section. Elles sont donc cachees elles aussi, mais APRES les STORYnn.DAT
 * -- ceux-la sont relus a chaque section, ils passent d'abord -- et seulement
 * tant qu'il reste de la place. Le remplissage est opportuniste : une image
 * qui ne tient pas reste sur la disquette, sans que rien ne le signale.
 */

/* Chemin a ouvrir pour ce fichier : sa copie en disque RAM si elle existe,
 * sinon `name` tel quel, donc la disquette. */
const char *ram_file_path(const char *name);


/* Chemin du fichier cache, "/RAM/..." ou "/RAM2/..." selon ou il a ete copie
 * (tampon statique, valide jusqu'au prochain appel). */
const char *ram_path(u8 id);

#endif /* A2ADV_RAMDISK_H */
