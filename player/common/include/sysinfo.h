/* sysinfo.h -- info systeme optionnelle (ecran Options, diagnostic), un
 * backend par plateforme (cf. apple2/src/sysinfo.c, atarist/src/sysinfo.c).
 * Meme motif que scr.h/snd.h : le format exact (quels faits, combien de
 * lignes) est laisse a chaque plateforme, qui affiche ce qu'elle sait
 * determiner de facon sure (modele, mode ecran, memoire...). */
#ifndef A2ADV_SYSINFO_H
#define A2ADV_SYSINFO_H

/* Affiche quelques lignes d'info systeme via scr_puts()/ui_newline(), a la
 * position courante du curseur. L'appelant (smenu.c) se charge du titre et
 * du retour. */
void sys_info(void);

#endif /* A2ADV_SYSINFO_H */
