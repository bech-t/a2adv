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

/* Table de correspondance Latin-1 (texte de STORY.DAT/APP.LNG, cf.
 * compiler/a2c/translit.py) -> jeu de caracteres Atari ST, indexee par
 * (octet - 0x80). Contrairement a l'Apple II (aucun glyphe accentue en
 * ROM, cf. player/apple2/src/scr.c : accent_fold, qui replie tout en
 * ASCII nu), le generateur de caracteres du ST a les lettres francaises
 * courantes aux codes 0x80-0xFF ("ST high", proche de CP437) -- confirme
 * a l'ecran (cf. smoketest/hello.c : 0x82=e', 0x8A=e`, 0x85=a`, 0x87=c
 * cedille, qui retrouvent ici les memes valeurs). Repli en lettre nue
 * uniquement quand le glyphe MAJUSCULE manque du jeu ST (È Ê Ë Î Ï Ô Û Ù
 * notamment, present en minuscule mais pas en capitale) ; '?' pour les
 * octets de controle Latin-1 0x80-0x9F, qui ne devraient jamais
 * apparaitre dans du texte. */
static const u8 latin1_to_st[128] = {
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,  /* 0x80 */
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,  /* 0x88 */
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,  /* 0x90 */
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,  /* 0x98 */
    0x20, 0xAD, 0x9B, 0x9C, 0x3F, 0x9D, 0x3F, 0xDD,  /* 0xA0: nbsp ¡ ¢ £ ¤ ¥ ¦ § */
    0xB9, 0xBD, 0xA6, 0xAE, 0xAA, 0x2D, 0xBE, 0xFF,  /* 0xA8: ¨ © ª « ¬ shy ® ¯ */
    0xF8, 0xF1, 0xFD, 0xFE, 0xBA, 0xE6, 0xBC, 0xFA,  /* 0xB0: ° ± ² ³ ´ µ ¶ · */
    0x3F, 0x31, 0xA7, 0xAF, 0xAC, 0xAB, 0x3F, 0xA8,  /* 0xB8: ¸ ¹ º » ¼ ½ ¾ ¿ */
    0xB6, 0x41, 0x41, 0xB7, 0x8E, 0x8F, 0x92, 0x80,  /* 0xC0: À Á Â Ã Ä Å Æ Ç */
    0x45, 0x90, 0x45, 0x45, 0x49, 0x49, 0x49, 0x49,  /* 0xC8: È É Ê Ë Ì Í Î Ï */
    0x44, 0xA5, 0x4F, 0x4F, 0x4F, 0xB8, 0x99, 0x3F,  /* 0xD0: Ð Ñ Ò Ó Ô Õ Ö × */
    0xB2, 0x55, 0x55, 0x55, 0x9A, 0x59, 0x54, 0x9E,  /* 0xD8: Ø Ù Ú Û Ü Ý Þ ß */
    0x85, 0xA0, 0x83, 0xB0, 0x84, 0x86, 0x91, 0x87,  /* 0xE0: à á â ã ä å æ ç */
    0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B,  /* 0xE8: è é ê ë ì í î ï */
    0x64, 0xA4, 0x95, 0xA2, 0x93, 0xB1, 0x94, 0xF6,  /* 0xF0: ð ñ ò ó ô õ ö ÷ */
    0xB3, 0x97, 0xA3, 0x96, 0x81, 0x79, 0x74, 0x98,  /* 0xF8: ø ù ú û ü ý þ ÿ */
};

/* Ramene un octet Latin-1 au code du jeu de caracteres ST courant. */
static char to_screen_st(char c)
{
    u8 a = (u8)c;
    if (a >= 0x80)
        a = latin1_to_st[a - 0x80];
    return (char)a;
}

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
    Cconout((int)(unsigned char)to_screen_st(c));
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
