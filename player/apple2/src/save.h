/* save.h -- sauvegarde/chargement de l'etat joueur dans un fichier ProDOS.
 *
 * Bloc sauve (cf. spec §9) : section_courante (2 o) + stats[] + bitset objets
 * + bitset flags. L'appelant doit fermer STORY0.DAT avant (un seul fichier
 * ouvert a la fois) puis le rouvrir apres.
 */
#ifndef A2ADV_SAVE_H
#define A2ADV_SAVE_H

#include "format.h"
#include "scr.h"   /* scr_progress_cb */

/* Ecrit l'etat courant + la section donnee. cb=NULL si pas de barre. 0=ok. */
signed char save_state(u16 section, scr_progress_cb cb);

/* Relit l'etat ; *out_section = section sauvee. cb=NULL si pas de barre.
 * 0 = ok, <0 = pas de sauvegarde. */
signed char load_state(u16 *out_section, scr_progress_cb cb);

#endif /* A2ADV_SAVE_H */
