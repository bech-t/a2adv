/* scr.c -- pilote texte Atari ST : console VT52 (GEMDOS Cconout/Cconws) plutot
 * qu'une ecriture directe en page ecran. Le ST expose nativement ce terminal
 * (visible des l'ecran de demarrage EmuTOS) ; s'appuyer dessus evite de
 * reimplementer un rendu de police, largement suffisant pour un livre-jeu au
 * tour par tour. scr_gfx_xxx et scr_load_hgr restent des bouchons -- slice
 * "texte seul" du portage, cf. spec-atarist.md §10.
 *
 * Sequences VT52 utilisees (standard sur ST, cf. doc AES/TOS) :
 *   ESC E        efface l'ecran, curseur en haut a gauche
 *   ESC Y r c    place le curseur (r, c EN CLAIR + 32, origine 0)
 *   ESC p / q    entre / sort de la video inverse
 */
#include <osbind.h>
#include "scr.h"

u8  scr_cols;
u16 scr_entropy;
void (*scr_idle_hook)(void);

/* Colonne courante -- ce fichier doit la suivre LUI-MEME, comme la version
 * Apple II (page texte directe, cx/cy). ui.c s'appuie dessus : son propre
 * compteur de colonne se remet a 0 SANS emettre de saut de ligne des qu'il
 * pense que l'ecran a "enroule" (cf. ui_wrap : `if (col >= scr_cols) col =
 * 0;`), en confiance que scr_putc() vient de le faire. Un simple Cconout()
 * qui compte sur l'auto-wrap du terminal VT52 dessynchronise les deux :
 * constate en pratique (2026-09-09) -- la fin des phrases disparaissait,
 * ecrasee sur la meme ligne au lieu de descendre. */
static u8 cx;

/* _frclock (cf. mint/sysvars.h) : compteur d'interruptions VBL reelles --
 * incremente au rythme d'affichage EFFECTIF de la machine (50 Hz PAL, 60 Hz
 * NTSC, jusqu'a 70 Hz en haute resolution monochrome). Meme nature que le bit
 * $C019 de l'Apple II (un VRAI signal materiel, pas une horloge fixee), avec
 * la meme consequence : du code partage qui compte "60 bascules = 1 seconde"
 * (wait_or_key, simage.c, jamais modifie par ce portage) tournera ~20% plus
 * lentement sur un ST PAL que sur un NTSC -- ecart reel de la plateforme, pas
 * un bug de ce fichier. A surveiller si un jour ca se voit a l'usage. */
#define FRCLOCK (*(volatile unsigned long *)0x466L)

void scr_init(void)
{
    scr_cols = (Getrez() == 0) ? 40 : 80;   /* 40 en basse resolution, 80 sinon */
    scr_idle_hook = 0;
    scr_clear();
}

void scr_clear(void)
{
    (void)Cconws("\033E");
    cx = 0;
}

static void st_newline(void)
{
    Cconout('\r');
    Cconout('\n');
    cx = 0;
}

void scr_putc(char c)
{
    if (c == '\r') {
        Cconout('\r');
        cx = 0;
        return;
    }
    if (c == '\n') {
        st_newline();
        return;
    }
    Cconout((int)(unsigned char)c);
    if (++cx >= scr_cols)
        st_newline();
}

void scr_puts(const char *s)
{
    while (*s)
        scr_putc(*s++);
}

void scr_revers(u8 on)
{
    (void)Cconws(on ? "\033p" : "\033q");
}

void scr_gotoxy(u8 x, u8 y)
{
    Cconout('\033');
    Cconout('Y');
    Cconout(32 + y);
    Cconout(32 + x);
    cx = x;
}

char scr_getkey(void)
{
    while (!Cconis()) {
        ++scr_entropy;             /* le temps de reaction humain sert d'entropie */
        if (scr_idle_hook)
            scr_idle_hook();       /* musique de fond, sans interruptions */
    }
    return (char)(Cnecin() & 0x7F);
}

char scr_poll(void)
{
    if (!Cconis())
        return 0;
    return (char)(Cnecin() & 0x7F);
}

void scr_flush(void)
{
    while (Cconis())
        (void)Cnecin();
}

void scr_backspace(void)
{
    Cconout(8);
    Cconout(' ');
    Cconout(8);
}

/* Identique a apple2/src/scr.c : aucune dependance materielle dans cette
 * fonction, seulement scr_getkey/scr_backspace/scr_putc ci-dessus. */
u8 scr_readline(char *buf, u8 maxlen)
{
    u8 n = 0;
    char c;
    for (;;) {
        c = scr_getkey();
        if (c == 13)
            break;
        if (c == 8 || c == 127) {
            if (n) { --n; scr_backspace(); }
            continue;
        }
        if (c >= 32 && n < maxlen) {
            buf[n++] = c;
            scr_putc(c);
        }
    }
    buf[n] = '\0';
    return n;
}

u8 scr_vbl(void)
{
    return (u8)(FRCLOCK & 1);
}

/* --- Bouchons graphiques (slice suivante, cf. spec-atarist.md §4/§10) ----- */

void scr_gfx_on(void)
{
}

void scr_gfx_mixed(void)
{
}

void scr_gfx_off(void)
{
}

signed char scr_load_hgr(const char *name, scr_progress_cb cb)
{
    (void)name;
    (void)cb;
    return -1;
}

void *scr_hgr_page(void)
{
    return (void *)0;
}
