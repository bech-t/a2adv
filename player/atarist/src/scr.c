/* scr.c -- pilote texte Atari ST : console VT52 (GEMDOS Cconout/Cconws) plutot
 * qu'une ecriture directe en page ecran. Le ST expose nativement ce terminal
 * (visible des l'ecran de demarrage EmuTOS) ; s'appuyer dessus evite de
 * reimplementer un rendu de police, largement suffisant pour un livre-jeu au
 * tour par tour.
 *
 * Sequences VT52 utilisees (standard sur ST, cf. doc AES/TOS) :
 *   ESC E        efface l'ecran, curseur en haut a gauche
 *   ESC Y r c    place le curseur (r, c EN CLAIR + 32, origine 0)
 *   ESC p / q    entre / sort de la video inverse
 *
 * Images : texte seul en moyenne (ou haute sur mono) resolution, bascule en
 * basse resolution ST (320x200, 16 couleurs) des qu'une image est a l'ecran
 * (cf. scr_init pour le detail). Contrairement a l'Apple II (page texte
 * $400 et page HIRES $2000 = deux zones memoire separees, composees par un
 * soft-switch), la console VT52 du ST dessine ses glyphes directement dans
 * le MEME framebuffer que les graphismes (Physbase()) : pas de switch
 * materiel pour le mode "mixte", juste ne pas ecrire sur les lignes ou le
 * texte doit apparaitre (cf. scr_gfx_mixed). */
#include <osbind.h>
#include <stdio.h>    /* fopen/fread : chargement d'une image, cf. scr_load_hgr */
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

/* Texte : fond blanc, glyphes noirs -- forces explicitement plutot que de
 * garder la palette trouvee au boot (registres 0/1, communs a toutes les
 * resolutions couleur). Memes deux couleurs reservees aux memes index dans
 * les images generees par img2st.py, donc un texte dessine par-dessus une
 * image en mode mixte reste lisible sans rien faire de plus. Sans objet si
 * st_mono : un moniteur monochrome n'a pas de palette RVB.
 *
 * Poses par Setpalette() (16 registres d'un coup), pas par deux Setcolor()
 * separes : c'est le meme mecanisme que celui, deja verifie a l'ecran, que
 * scr_load_hgr() utilise pour les images. */
static const u16 text_palette[16] = {
    0x0777, 0x0000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* 1 si moniteur MONOCHROME : Getrez() vaut alors TOUJOURS 2 (haute
 * resolution) des le boot, quel que soit ce que le programme demande
 * ensuite -- le shifter video ne peut pas synchroniser un mode couleur
 * (basse/moyenne resolution) sur ce type de moniteur (frequences
 * differentes). Verifie AVANT tout Setscreen : changer de resolution sans
 * cette garde corrompt l'affichage sur un moniteur mono. Sur mono, ce
 * fichier n'exploite pas les images : scr_load_hgr echoue proprement (cf.
 * plus bas), le jeu reste jouable en texte seul. */
static u8 st_mono;

/* Objectif de resolution : TEXTE SEUL -> 80 colonnes (moyenne resolution sur
 * moniteur couleur, haute resolution sur mono) ; DES QU'UNE IMAGE EST A
 * L'ECRAN (plein ecran ou mode mixte) -> toujours la basse resolution
 * (320x200/16 couleurs, 40 colonnes), y compris pour la fenetre de texte du
 * bas en mode mixte. scr_load_hgr() bascule donc en basse resolution
 * lui-meme (avant d'ecrire le bitmap) ; scr_gfx_off() revient en moyenne
 * resolution. */
void scr_init(void)
{
    (void)Super(0L);   /* mode superviseur : necessaire pour lire FRCLOCK (scr_frclock) */
    st_mono = (Getrez() == 2);
    if (!st_mono) {
        void *scr = Physbase();
        Setscreen((long)scr, (long)scr, 1);   /* moyenne resolution (texte 80 col) */
        Setpalette((long)text_palette);
    }
    scr_cols = 80;                    /* 80 colonnes dans les deux cas, texte seul */
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

u32 scr_frclock(void)
{
    return FRCLOCK;
}

/* --- Affichage graphique basse resolution (320x200, 16 couleurs) ------- */

/* scr_load_hgr() a deja bascule en basse resolution et pose l'image (cf.
 * plus bas) : rien de plus a activer ici. */
void scr_gfx_on(void)
{
}

/* Image en haut (160 lignes, cf. ST_MIXED_ROWS) + fenetre texte 40 col
 * (4 lignes) en bas -- memes proportions que apple2/src/scr.c (cy = 20 sur
 * 24 lignes). Pas de switch materiel ICI : deja fait par scr_load_hgr()
 * (bascule de resolution + palette) ; la zone de texte fait deja partie de
 * l'image chargee (le convertisseur y met du noir, cf. player/atarist/
 * img2st/img2st.py --mixed), scr_putc()/Cconout() n'a plus qu'a dessiner
 * ses glyphes par-dessus. */
void scr_gfx_mixed(void)
{
    scr_gotoxy(0, 20);
}

/* Revient au texte seul : moyenne resolution (80 col) sur moniteur couleur,
 * inchange sur mono (jamais quitte la haute resolution, cf. scr_init). */
void scr_gfx_off(void)
{
    if (!st_mono) {
        void *scr = Physbase();
        Setscreen((long)scr, (long)scr, 1);
        Setpalette((long)text_palette);
        scr_cols = 80;
    }
    scr_clear();                          /* efface AVANT de revenir : pas d'effet memoire */
}

/* Bitmap basse resolution ST : 4 plans entrelaces par mot (MSB = pixel de
 * gauche), 200 lignes de 160 o (20 mots/plan x 4 plans x 2 o) = 32000 o.
 * Format sur disque = Degas .PI1 reel (2 o resolution + 32 o palette + 32000
 * o bitmap, cf. img2st.py) : lisible tel quel par n'importe quel outil ST,
 * pas invente pour l'occasion. */
#define ST_BITMAP_SIZE 32000
#define ST_BLK          2000
#define ST_NBLK         (ST_BITMAP_SIZE / ST_BLK)   /* 16 blocs, comme apple2 */

void *scr_hgr_page(void)
{
    return Physbase();
}

signed char scr_load_hgr(const char *name, scr_progress_cb cb)
{
    FILE *f;
    u16 rez;
    u16 pal[16];
    unsigned char *p;
    u8 i;

    if (st_mono)          /* pas d'images sur moniteur mono, cf. scr_init */
        return -1;

    f = fopen(name, "rb");
    if (f == NULL)
        return -1;
    if (fread(&rez, 1, 2, f) != 2 || rez != 0) {   /* doit etre basse resolution */
        fclose(f);
        return -2;
    }
    if (fread(pal, 1, 32, f) != 32) {
        fclose(f);
        return -2;
    }
    /* En-tete valide : bascule en basse resolution MAINTENANT (cf. objectif
     * de resolution, scr_init), avant d'ecrire le bitmap -- pas avant, pour
     * ne jamais laisser l'ecran en basse resolution si le fichier est
     * absent/tronque. Adresses logique/physique explicitement synchronisees
     * (pas -1L/-1L, "ne pas changer") : c'est l'adresse logique que relit la
     * console VT52 pour le texte (cf. scr_gfx_mixed) apres reinitialisation
     * par Setscreen -- la forcer sur Physbase() evite tout doute. */
    {
        void *scr = Physbase();
        Setscreen((long)scr, (long)scr, 0);
    }
    scr_cols = 40;
    Setpalette((long)pal);

    p = (unsigned char *)Physbase();
    for (i = 0; i < ST_NBLK; ++i) {
        if (fread(p, 1, ST_BLK, f) != ST_BLK) {
            fclose(f);
            return -2;
        }
        p += ST_BLK;
        if (cb != NULL)
            cb((u16)((i + 1) * ST_BLK), ST_BITMAP_SIZE);
    }
    fclose(f);
    return 0;
}
