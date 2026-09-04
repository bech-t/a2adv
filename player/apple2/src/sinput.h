/* sinput.h -- scene a saisie clavier (mot/code compare a des reponses). */
#ifndef A2ADV_SINPUT_H
#define A2ADV_SINPUT_H

#include "format.h"

/* Affiche l'accroche + l'invite, lit une ligne, la compare aux reponses
 * (deja normalisees). Renvoie 1 si bonne reponse, 0 sinon. */
u8 run_input(const char *prompt, u8 maxlen, u16 ans_pos, u8 nans,
             u16 body_start);

#endif /* A2ADV_SINPUT_H */
