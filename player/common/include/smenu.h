/* smenu.h -- menu principal (semi-graphique si MENU.HGR, sinon texte). */
#ifndef A2ADV_SMENU_H
#define A2ADV_SMENU_H

#include "format.h"

/* Renvoie ACT_NEW / ACT_LOAD / ACT_QUIT (cf. game.h). */
u8 run_menu(void);

#endif /* A2ADV_SMENU_H */
