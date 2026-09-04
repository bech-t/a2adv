/* scombat.h -- ecran de combat (driver). Le coeur de resolution est combat.c. */
#ifndef A2ADV_SCOMBAT_H
#define A2ADV_SCOMBAT_H

#include "format.h"

/* Joue le combat (ecran dedie) et renvoie l'ISSUE :
 * CB_VICTOIRE / CB_DEFAITE / CB_FUITE. body_start pointe sur le texte d'accroche. */
u8 run_combat(u16 eimg, u8 att, u8 hp, u8 dmg, u8 armor,
              const char *name, u8 can_flee, u16 body_start);

#endif /* A2ADV_SCOMBAT_H */
