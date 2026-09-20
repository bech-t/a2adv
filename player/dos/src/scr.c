/* scr.c -- pilote ecran PC DOS : mode texte BIOS 3 (80x25, ecriture directe
 * en page texte $B800, cf. apple2/src/scr.c pour le meme principe sur
 * Apple II) pour tout le texte "plein ecran", et mode graphique VGA 13h
 * (320x200, 256 couleurs, page lineaire $A000, cf. atarist/src/scr.c pour le
 * meme principe de bascule de resolution sur Atari ST) des qu'une image est
 * a l'ecran (plein ecran ou mode mixte).
 *
 * Texte en mode mixte : contrairement a l'Apple II (page texte separee) et
 * a l'Atari ST (une seule console VT52 qui dessine dans le meme framebuffer
 * que les graphismes), le mode 13h n'a AUCUNE notion de texte materiel --
 * ce fichier dessine donc ses propres glyphes pixel par pixel directement
 * en page $A000, via une police maison (font_ascii/font_latin1 plus bas,
 * cf. leur commentaire pour la provenance) : ne touche que les pixels ON du
 * glyphe, le reste de la cellule reste inchange (fond de l'image visible
 * autour des lettres). Texte normal en blanc sur le fond noir reserve par
 * l'image (cf. img2dos.py) sous la fenetre --mixed, meme rendu que le mode
 * texte plein ecran ; video inverse (cf. show_intro_image) : la cellule est
 * remplie en blanc AVANT le glyphe, dessine alors en noir -- cf.
 * put_gfx_char.
 *
 * NE PAS passer par le BIOS (INT 10h, AH=0Eh, "teletype output") pour ca,
 * meme si sa documentation le dit utilisable en mode graphique : sous
 * DOSBox, il efface tout le bloc 8x8 a la couleur 0 avant d'y dessiner le
 * glyphe (verifie par dump memoire de $A000 relu depuis l'hote) au lieu de
 * ne toucher que les pixels ON -- inutilisable pour poser du texte
 * PAR-DESSUS une image.
 *
 * Deux index de palette sont RESERVES sur toute image (cf. img2dos.py) :
 * 255 = noir, 254 = blanc -- ce sont ceux que ce fichier utilise pour le
 * texte, quelle que soit la palette propre a l'image chargee (meme
 * principe que le "blanc=0/noir=1" reserve d'img2st.py cote Atari ST). */

#include <i86.h>
#include <dos.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scr.h"

/* --- Table Latin-1 (texte de STORY.DAT/APP.LNG, cf. compiler/a2c/
 * translit.py) -> code page 437 (police ROM du BIOS), indexee par
 * (octet - 0x80). Meme esprit que latin1_to_st cote Atari ST (cf.
 * atarist/src/scr.c), valeurs CP437 au lieu du jeu de caracteres ST. '?'
 * pour tout octet de controle Latin-1 0x80-0x9F (ne devrait jamais
 * apparaitre dans du texte) et pour les lettres majuscules accentuees
 * absentes de CP437 (Ê Ë Î Ï Ô Û, notamment). */
static const u8 latin1_to_cp437[128] = {
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,  /* 0x80 */
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,  /* 0x88 */
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,  /* 0x90 */
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,  /* 0x98 */
    0x20, 0xAD, 0x3F, 0x9C, 0x3F, 0x9D, 0x3F, 0x3F,  /* 0xA0: nbsp ¡ ¢ £ ¤ ¥ ¦ § */
    0x3F, 0x3F, 0xA6, 0xAE, 0xAA, 0x2D, 0x3F, 0x3F,  /* 0xA8: ¨ © ª « ¬ shy ® ¯ */
    0xF8, 0xF1, 0xFD, 0x3F, 0x27, 0xE6, 0x3F, 0xFA,  /* 0xB0: ° ± ² ³ ´ µ ¶ · */
    0x3F, 0x3F, 0xA7, 0xAF, 0xAC, 0xAB, 0x3F, 0xA8,  /* 0xB8: ¸ ¹ º » ¼ ½ ¾ ¿ */
    0xB5, 0x41, 0x41, 0xB6, 0x8E, 0x8F, 0x92, 0x80,  /* 0xC0: À Á Â Ã Ä Å Æ Ç */
    0x45, 0x90, 0x45, 0x45, 0x49, 0x49, 0x49, 0x49,  /* 0xC8: È É Ê Ë Ì Í Î Ï */
    0x44, 0xA5, 0x4F, 0x4F, 0x4F, 0x99, 0x99, 0x3F,  /* 0xD0: Ð Ñ Ò Ó Ô Õ Ö × */
    0x4F, 0x55, 0x55, 0x55, 0x9A, 0x59, 0x54, 0xE1,  /* 0xD8: Ø Ù Ú Û Ü Ý Þ ß */
    0x85, 0xA0, 0x83, 0x61, 0x84, 0x86, 0x91, 0x87,  /* 0xE0: à á â ã ä å æ ç */
    0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B,  /* 0xE8: è é ê ë ì í î ï */
    0x6F, 0xA4, 0x95, 0xA2, 0x93, 0x6F, 0x94, 0x3F,  /* 0xF0: ð ñ ò ó ô õ ö ÷ */
    0x6F, 0x97, 0xA3, 0x96, 0x81, 0x79, 0x74, 0x98,  /* 0xF8: ø ù ú û ü ý þ ÿ */
};

static char to_screen(char c)
{
    u8 a = (u8)c;
    if (a >= 0x80)
        a = latin1_to_cp437[a - 0x80];
    return (char)a;
}

/* Replie un code clavier accentue (CP437, PAS Latin-1 -- meme role que
 * fold_accent cote Atari ST, cf. atarist/src/scr.c) en sa lettre ASCII nue
 * majuscule : le compilateur compare toujours une reponse @ask en ASCII
 * majuscule sans accent (cf. compiler/a2c/translit.py, to_match_key). Utile
 * seulement si un pilote clavier francais (KEYB FR) est charge -- absent de
 * la disquette minimale de ce portage (cf. Makefile), le clavier reste en
 * disposition US par defaut et ne produit alors jamais ces codes ; conserve
 * quand meme pour le jour ou KEYB FR sera ajoute. 0 = code non reconnu ici. */
static char fold_accent(u8 c)
{
    switch (c) {
    case 0x82: case 0x8A: case 0x88: case 0x89: case 0x90:
        return 'E';
    case 0x85: case 0x83: case 0x84: case 0x91: case 0x8F: case 0x8E:
        return 'A';
    case 0x87: case 0x80:
        return 'C';
    case 0x97: case 0x96: case 0x81: case 0x9A:
        return 'U';
    case 0x8C: case 0x8B: case 0xD8: case 0xDE:
        return 'I';
    case 0x93: case 0x94: case 0x99:
        return 'O';
    case 0x98:
        return 'Y';
    case 0xA4: case 0xA5:
        return 'N';
    default:
        return 0;
    }
}

u8  scr_cols;
u16 scr_entropy;
void (*scr_idle_hook)(void);

static u8 cx, cy;
#define SCR_MAX_ROW 24     /* derniere rangee valide (25 rangees, 0-24) */

static u8 gfx_active;      /* 0 = mode texte 80 col, 1 = mode 13h (overlay 40 col) */

#define TEXT_SEG          0xB800
#define GFX_SEG           0xA000
#define TEXT_ATTR_NORMAL  0x07
#define TEXT_ATTR_REVERS  0x70

#define PAL_WHITE  254
#define PAL_BLACK  255

static void bios_setmode(u8 mode)
{
    union REGS r;
    r.h.ah = 0x00;
    r.h.al = mode;
    int86(0x10, &r, &r);
}

static void bios_gotoxy(u8 x, u8 y)
{
    union REGS r;
    r.h.ah = 0x02;
    r.h.bh = 0;
    r.h.dh = y;
    r.h.dl = x;
    int86(0x10, &r, &r);
}

/* --- Mode texte (page $B800, ecriture directe) -------------------------- */
static void text_putat(u8 x, u8 y, char c, u8 attr)
{
    unsigned char *p = (unsigned char *)MK_FP(TEXT_SEG, ((u16)y * 80 + x) * 2);
    p[0] = (unsigned char)c;
    p[1] = attr;
}

/* --- Police maison 8x8 (mode graphique uniquement, cf. entete du fichier)
 * -- domaine public, issue de font8x8 (Daniel Hepper, base sur les polices
 * VGA IBM/Marcel Sondaar, "Public Domain"), https://github.com/dhepper/
 * font8x8 -- reduite a l'ASCII imprimable (font_ascii, 0x20-0x7E, index
 * direct) et au bloc Latin-1 accentue (font_latin1, U+00A0-U+00FF ==
 * 0xA0-0xFF en Latin-1, meme encodage que le texte de STORY.DAT -- cf.
 * compiler/a2c/translit.py) : index direct par l'octet Latin-1, PAS par le
 * code page 437 utilise cote mode texte (cf. to_screen plus haut) -- deux
 * chemins de rendu independants, chacun sa police source. Un octet dans
 * l'ecart 0x7F-0x9F (jamais produit par translit.py) retombe sur '?'.
 * Chaque glyphe : 8 octets, un par rangee, bit 0 = pixel de GAUCHE (verifie
 * par rendu + relecture, cf. le commentaire au-dessus de put_gfx_char). */
#include "font8x8.h"

static const u8 *glyph_for(char c)
{
    u8 a = (u8)c;
    if (a >= 0x20 && a <= 0x7E)
        return font_ascii[a - 0x20];
    if (a >= 0xA0)
        return font_latin1[a - 0xA0];
    return font_ascii['?' - 0x20];
}

/* --- Mode graphique (page $A000, overlay texte 40 col) -------------------
 * put_gfx_char dessine un caractere 8x8 a la cellule (x,y) PIXEL PAR PIXEL,
 * directement en page $A000 -- ne touche que les pixels ON du glyphe (fond
 * de l'image intact autour des lettres en video normale), cf. entete du
 * fichier pour pourquoi ce n'est PAS fait via le BIOS. Video inverse :
 * cellule remplie en blanc D'ABORD (gfx_fillcell), glyphe dessine en noir
 * par-dessus -- normal : pas de remplissage, glyphe en blanc directement sur
 * le fond noir reserve par l'image (cf. img2dos.py). */
static void gfx_fillcell(u8 x, u8 y, u8 color)
{
    unsigned char *p = (unsigned char *)MK_FP(GFX_SEG, (u32)y * 8 * 320 + (u32)x * 8);
    u8 row;
    for (row = 0; row < 8; ++row) {
        memset(p, color, 8);
        p += 320;
    }
}

static u8 revers_on;

static void put_gfx_char(u8 x, u8 y, char c, u8 rev)
{
    const u8 *g = glyph_for(c);
    unsigned char *p = (unsigned char *)MK_FP(GFX_SEG, (u32)y * 8 * 320 + (u32)x * 8);
    u8 ink = rev ? PAL_BLACK : PAL_WHITE;
    u8 row, col, bits;

    if (rev)
        gfx_fillcell(x, y, PAL_WHITE);
    for (row = 0; row < 8; ++row) {
        bits = g[row];
        for (col = 0; col < 8; ++col)
            if (bits & (1 << col))
                p[col] = ink;
        p += 320;
    }
}

/* --- API commune (cf. scr.h) --------------------------------------------- */

void scr_init(void)
{
    bios_setmode(0x03);
    scr_cols = 80;
    scr_idle_hook = 0;
    gfx_active = 0;
    revers_on = 0;
    scr_clear();
}

void scr_clear(void)
{
    if (!gfx_active) {
        unsigned char *p = (unsigned char *)MK_FP(TEXT_SEG, 0);
        u16 i;
        for (i = 0; i < 80 * 25; ++i) {
            p[i * 2]     = ' ';
            p[i * 2 + 1] = TEXT_ATTR_NORMAL;
        }
    }
    cx = 0;
    cy = 0;
    bios_gotoxy(0, 0);
}

static void newline(void)
{
    cx = 0;
    if (cy < SCR_MAX_ROW) {
        ++cy;
        if (!gfx_active)
            bios_gotoxy(cx, cy);
    }
}

void scr_putc(char c)
{
    if (c == '\r') {
        cx = 0;
        if (!gfx_active)
            bios_gotoxy(cx, cy);
        return;
    }
    if (c == '\n') {
        newline();
        return;
    }
    if (gfx_active)
        put_gfx_char(cx, cy, c, revers_on);
    else
        text_putat(cx, cy, to_screen(c), revers_on ? TEXT_ATTR_REVERS : TEXT_ATTR_NORMAL);
    if (++cx >= scr_cols)
        newline();
    else if (!gfx_active)
        bios_gotoxy(cx, cy);
}

void scr_puts(const char *s)
{
    while (*s)
        scr_putc(*s++);
}

void scr_revers(u8 on)
{
    revers_on = on;
}

void scr_gotoxy(u8 x, u8 y)
{
    cx = x;
    cy = y;
    if (!gfx_active)
        bios_gotoxy(x, y);
}

/* --- Clavier (conio.h : kbhit()/getch(), PAS d'appel BIOS/DOS a la main) --
 * Marche identiquement quel que soit le mode video (texte ou graphique) --
 * ce sont des fonctions clavier, sans rapport avec l'ecran. Fonctions de
 * bibliotheque Watcom deja eprouvees : pas de raison de les reimplementer
 * via int86() -- en particulier, INT 16h/AH=01h ("touche disponible ?") ne
 * doit PAS etre teste par "AX != 0" en repli faute d'acces au fanion Z
 * (Open Watcom n'expose que cflag, la retenue, via union REGS) : rien ne
 * garantit que le BIOS remette AX a 0 quand aucune touche n'attend (Ralf
 * Brown's Interrupt List), et fixer AH avant l'appel (necessaire pour
 * selectionner la fonction) rend alors ce test pratiquement toujours vrai,
 * y compris sans touche pressee. */

/* getch() rend 0 sur le PREMIER appel pour une touche etendue (fleches,
 * F1...), le code reel venant d'un SECOND appel a consommer -- protocole a
 * deux appels, differe de la lecture BIOS brute (AH=00h) qui rend les deux
 * octets (AL/AH) en un seul appel. Touches etendues non utilisees par ce
 * moteur : le second getch() est juste consomme puis ignore. */
char scr_getkey(void)
{
    int c;
    while (!kbhit()) {
        ++scr_entropy;
        if (scr_idle_hook)
            scr_idle_hook();
    }
    c = getch();
    if (c == 0) {
        (void)getch();
        return scr_getkey();
    }
    return (char)c;
}

char scr_poll(void)
{
    int c;
    if (!kbhit())
        return 0;
    c = getch();
    if (c == 0) {
        (void)getch();
        return scr_poll();
    }
    return (char)c;
}

void scr_flush(void)
{
    while (kbhit())
        (void)getch();
}

void scr_backspace(void)
{
    if (cx == 0)
        return;
    --cx;
    if (gfx_active) {
        gfx_fillcell(cx, cy, PAL_BLACK);
    } else {
        text_putat(cx, cy, ' ', TEXT_ATTR_NORMAL);
        bios_gotoxy(cx, cy);
    }
}

/* Diverge du mode texte pur : un octet CP437 accentue (cf. entete) est
 * replie en ASCII nu AVANT d'entrer dans buf, comme fold_accent cote Atari
 * ST -- sans effet tant qu'aucun pilote clavier francais n'est charge (cf.
 * fold_accent), mais sans cout non plus. */
u8 scr_readline(char *buf, u8 maxlen)
{
    u8 n = 0;
    char c;
    u8 uc;
    for (;;) {
        c = scr_getkey();
        uc = (u8)c;
        if (uc >= 0x80) {
            char f = fold_accent(uc);
            if (!f)
                continue;
            c = f;
            uc = (u8)c;
        }
        if (c == 13)
            break;
        if (c == 8 || c == 127) {
            if (n) { --n; scr_backspace(); }
            continue;
        }
        if (uc >= 32 && n < maxlen) {
            buf[n++] = c;
            scr_putc(c);
        }
    }
    buf[n] = '\0';
    return n;
}

/* --- Horloge / VBL --------------------------------------------------------
 * Registre d'etat CRTC $3DA, bit 3 : retrace verticale en cours -- signal
 * materiel reel (cf. le pendant $C019 Apple II / FRCLOCK Atari ST), pas une
 * horloge fixee. scr_vbl() en extrait un bit qui bascule a chaque retrace.
 * scr_frclock() : compteur BIOS 0000:046C, incremente a ~18.2 Hz par
 * l'horloge du PIT (interruption 08h) -- cf. dos.h/RBIL, adresse standard,
 * documentee. */
#define TICK_COUNT (*(volatile u32 *)MK_FP(0x0040, 0x006C))

u8 scr_vbl(void)
{
    static u8 state;
    if (inp(0x3DA) & 0x08)
        state ^= 1;
    return state;
}

u32 scr_frclock(void)
{
    return TICK_COUNT;
}

/* --- Graphique (mode 13h, 320x200, 256 couleurs) ------------------------- */

void scr_gfx_on(void)
{
    /* deja bascule par scr_load_hgr (cf. plus bas), rien de plus ici */
}

/* Image en haut (160 lignes = 20 rangees de 8px), fenetre texte 40 col en
 * bas (rangees 21-24, une rangee d'air sous l'image -- meme geometrie
 * qu'Atari ST, cf. atarist/src/scr.c:scr_gfx_mixed). */
void scr_gfx_mixed(void)
{
    scr_gotoxy(0, 21);
}

void scr_gfx_off(void)
{
    if (gfx_active) {
        gfx_active = 0;
        bios_setmode(0x03);
        scr_cols = 80;
    }
    scr_clear();
}

void *scr_hgr_page(void)
{
    return (void *)MK_FP(GFX_SEG, 0);
}

/* Fichier .PCX (ZSoft, format DOS standard -- cf. platform.h:IMG_EXT et
 * img2dos.py) : en-tete fixe 128 o, puis les pixels (1 plan, 8 bpp) encodes
 * en RLE ligne par ligne, puis -- convention PCX "version 5" pour une
 * palette 256 couleurs -- un octet marqueur 0x0C suivi de 768 o de palette
 * RVB PLEINE ECHELLE (0-255 par canal, PAS les 0-63 des registres DAC VGA :
 * la conversion se fait ici, au chargement, cf. plus bas).
 *
 * Codage RLE (symetrique de encode_rle_scanline, img2dos.py) : un octet dont
 * les 2 bits de poids fort valent 11 (>= 0xC0) est un compteur de repetition
 * (6 bits bas, 1-63), suivi de l'octet a repeter ; sinon l'octet EST le
 * pixel (repetition implicite de 1).
 *
 * Le fichier est charge ENTIER en RAM avant decodage (cf. diskio.c pour le
 * meme choix sur STORYnn.DAT : lecteur de disquette lent, RAM conventionnelle
 * large) -- au plus 128 + 64000 + 769 = 64897 o non compresses, tient
 * largement dans un seul bloc alloue (modele COMPACT, cf. entete du
 * Makefile). Decoder depuis un tampon en RAM plutot que fread() octet par
 * octet evite aussi 64000+ appels stdio individuels. */
#define PCX_HEADER_SIZE 128
#define PCX_PAL_BYTES   768

signed char scr_load_hgr(const char *name, scr_progress_cb cb)
{
    FILE *f;
    unsigned char *buf, *p, *pend;
    unsigned char *vga;
    static unsigned char dac[PCX_PAL_BYTES];
    long fsize;
    u16 got, i;
    union REGS r;
    struct SREGS sr;

    f = fopen(name, "rb");
    if (f == NULL)
        return -1;
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize < (long)(PCX_HEADER_SIZE + 1 + PCX_PAL_BYTES)) {
        fclose(f);
        return -2;
    }
    buf = malloc((u16)fsize);
    if (buf == NULL) {
        fclose(f);
        return -2;
    }
    if (fread(buf, 1, (u16)fsize, f) != (u16)fsize) {
        fclose(f);
        free(buf);
        return -2;
    }
    fclose(f);

    /* Manufacturer=0x0A (ZSoft), BitsPerPixel=8, NPlanes=1 -- seule
     * combinaison ecrite par img2dos.py, cf. son entete. */
    if (buf[0] != 0x0A || buf[3] != 8 || buf[65] != 1) {
        free(buf);
        return -2;
    }

    bios_setmode(0x13);
    gfx_active = 1;
    scr_cols = 40;

    vga = (unsigned char *)MK_FP(GFX_SEG, 0);
    p = buf + PCX_HEADER_SIZE;
    pend = buf + fsize;
    got = 0;
    while (got < SCR_HGR_SIZE && p < pend) {
        unsigned char b = *p++;
        u16 run;
        unsigned char val;
        if ((b & 0xC0) == 0xC0) {
            run = (u16)(b & 0x3F);
            if (p >= pend) break;
            val = *p++;
        } else {
            run = 1;
            val = b;
        }
        while (run-- > 0 && got < SCR_HGR_SIZE)
            vga[got++] = val;
        if (cb != NULL && (got & 0x0FFF) == 0)
            cb(got, (u16)SCR_HGR_SIZE);
    }
    if (got < SCR_HGR_SIZE) {          /* flux RLE tronque */
        free(buf);
        return -2;
    }

    /* Palette : 769 DERNIERS octets du fichier (marqueur + 768 o RVB),
     * jamais suppose immediatement apres le flux RLE -- cf. entete. */
    p = buf + fsize - PCX_PAL_BYTES;
    if (p[-1] != 0x0C) {
        free(buf);
        return -2;
    }
    for (i = 0; i < PCX_PAL_BYTES; ++i)
        dac[i] = (unsigned char)(p[i] >> 2);      /* 0-255 -> 0-63 (DAC VGA) */
    free(buf);

    r.h.ah = 0x10;
    r.h.al = 0x12;
    r.x.bx = 0;
    r.x.cx = 256;
    r.x.dx = FP_OFF(dac);
    segread(&sr);
    sr.es = FP_SEG(dac);
    int86x(0x10, &r, &r, &sr);

    if (cb != NULL)
        cb((u16)SCR_HGR_SIZE, (u16)SCR_HGR_SIZE);
    return 0;
}
