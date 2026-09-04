/* lang.h -- socle de chaines d'interface, charge depuis APP.LNG (spec §6.1).
 *
 * Le player ne contient AUCUN texte de langue. APP.LNG porte les UI_COUNT
 * chaines dans l'ordre fige de l'enum UI_* ; l'aventure ne fournit ensuite que
 * ses surcharges (lues par story_open). Traduire l'interface = fournir un autre
 * APP.LNG, sans toucher a une seule aventure.
 */
#ifndef A2ADV_LANG_H
#define A2ADV_LANG_H

#include "format.h"

/* Charge APP.LNG dans ui_str[]. A APPELER AVANT story_open() : les surcharges
 * de l'aventure se posent par-dessus. 0 = ok, <0 = absent ou illisible
 * (le player reste alors muet : APP.LNG est une garantie de fabrication,
 * cf. la regle du Makefile). */
signed char lang_load(void);

#endif /* A2ADV_LANG_H */
