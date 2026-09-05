/* scr.c -- pilote texte 40/80 colonnes + affichage HIRES, par acces direct. */

#include <stdio.h>    /* fopen/fread : chargement d'une page HIRES */
#include <string.h>   /* memset : boucle STA optimisee en asm par cc65 */
#include "scr.h"

#define SCR_SPACE  0xA0   /* espace video normale ($20 | $80) */
#define HGR_PAGE   ((unsigned char *)0x2000)
#define HGR_SIZE   8192

/* --- Soft switches Apple //e ------------------------------------------- */
#define SW(a)   (*(volatile unsigned char *)(a))
#define CLR80COL  0xC000   /* 80STORE off                                   */
#define SET80COL  0xC001   /* 80STORE on  (PAGE2 banque la page texte)      */
#define RDMAINRAM 0xC054   /* PAGE2 off -> $0400-$07FF = memoire principale */
#define RDCARDRAM 0xC055   /* PAGE2 on  -> $0400-$07FF = memoire auxiliaire */
#define CLR80VID  0xC00C   /* affichage 40 colonnes                         */
#define SET80VID  0xC00D   /* affichage 80 colonnes                         */
#define CLRALTCHAR 0xC00E  /* jeu de caracteres standard (inverse correct)  */
#define SETALTCHAR 0xC00F  /* jeu de caracteres alternatif (minuscules/MouseText, non utilise) */
#define TXTCLR    0xC050   /* mode graphique                                */
#define TXTSET    0xC051   /* mode texte                                    */
#define MIXCLR    0xC052   /* plein ecran (pas de texte en bas)             */
#define MIXSET    0xC053   /* mode mixte (4 lignes de texte en bas)         */
#define HIRES_OFF 0xC056   /* basse resolution                              */
#define HIRES_ON  0xC057   /* haute resolution                             */
#define KBD       0xC000   /* clavier : bit7 = touche dispo                 */
#define KBDSTRB   0xC010   /* efface le strobe clavier                      */
#define MACHINE_ID 0xFBB3  /* $06 sur //e et suivants                       */

u8 scr_cols = 40;
u16 scr_entropy;             /* accumulateur d'entropie (attentes clavier) */

static u8 mode80;             /* 1 si 80 colonnes actives */
static u8 force40;            /* 1 : forcer l'ecriture texte en 40 col (mode mixte) */
static u8 cx, cy;            /* curseur */
static u8 inv;              /* video inverse */
static unsigned rowbase[24];

/* Adresse de base d'une ligne texte (agencement entrelace Apple II). */
static unsigned line_base(u8 r)
{
    return (unsigned)0x0400 + (unsigned)(r & 7) * 0x80 + (unsigned)(r >> 3) * 0x28;
}

/* Code ecran d'un caractere ASCII selon le mode video courant. */
static u8 encode(char c)
{
    u8 a = (u8)c;
    if (inv) {
        /* $00-$3F ne couvre que l'ASCII $20-$5F : le jeu PRIMAIRE n'a pas
         * d'inverse minuscule. Sans repli, un 'a' ($61 & 0x3F = $21) sortirait
         * en '!' inverse. On replie donc en capitale — la mise en relief reste
         * lisible, ce qui est tout ce qu'on lui demande. (L'inverse minuscule
         * n'existe que dans le jeu ALTERNATIF, en $60-$7F ; cf. scr_init.) */
        if (a >= 'a' && a <= 'z')
            a -= 'a' - 'A';
        return (u8)(a & 0x3F);        /* video inverse ($00-$3F) */
    }
    return (u8)(a | 0x80);            /* video normale : $A0-$FF = ASCII $20-$7F */
}

/* Ecrit un code ecran a (x,y), en gerant main/aux en mode 80 col. */
static void put_at(u8 x, u8 y, u8 code)
{
    unsigned addr;
    if (mode80 && !force40) {
        addr = rowbase[y] + (x >> 1);
        if ((x & 1) == 0) {           /* colonne paire -> auxiliaire */
            SW(RDCARDRAM) = 0;
            SW(addr) = code;
            SW(RDMAINRAM) = 0;
            return;
        }
    } else {
        addr = rowbase[y] + x;        /* 40 col (ou colonne impaire du 80 col) */
    }
    SW(addr) = code;                  /* memoire principale */
}

/* --- Detection de la memoire auxiliaire (carte 80 col) ---------------- */
static u8 detect_aux(void)
{
    u8 mainv, auxv, ok;

    /* On ne teste PAS l'ID machine ($FBB3) : sous ProDOS la ROM F8 est banquee
     * par la carte langage, donc illisible. Le test d'ecriture en aux ci-dessous
     * est le vrai test de presence (renvoie faux sur ][+/64 Ko sans aux). */
    SW(SET80COL) = 0;                 /* 80STORE : PAGE2 banque la page texte */
    SW(RDMAINRAM) = 0;                /* etat connu : PAGE2 off -> main */
    mainv = SW(0x0478);               /* $0478 = "trou d'ecran", non affiche */
    SW(RDCARDRAM) = 0;                /* -> aux */
    auxv = SW(0x0478);
    SW(0x0478) = 0x5A;                /* marque cote aux */
    SW(RDMAINRAM) = 0;                /* -> main */
    SW(0x0478) = 0xA5;                /* valeur differente cote main */
    SW(RDCARDRAM) = 0;                /* -> aux */
    ok = (SW(0x0478) == 0x5A);        /* aux independante de main ? */
    SW(0x0478) = auxv;                /* restaure aux */
    SW(RDMAINRAM) = 0;                /* -> main */
    SW(0x0478) = mainv;               /* restaure main */
    SW(CLR80COL) = 0;                 /* 80STORE off en attendant */
    return ok;
}

/* --- API --------------------------------------------------------------- */

void scr_init(void)
{
    u8 r;
    for (r = 0; r < 24; ++r)
        rowbase[r] = line_base(r);

    /* Jeu de caracteres PRIMAIRE, jamais l'alternatif, et fixe EXPLICITEMENT :
     * on ne suppose jamais l'etat de ce registre au demarrage. Une carte 80
     * colonnes peut laisser l'alternatif actif, ce qui avait rendu la video
     * inverse illisible sur //e.
     *
     * Le primaire suffit a tout ce qu'on affiche : $A0-$FF couvre l'ASCII
     * $20-$7F, minuscules comprises. Seul lui manque l'inverse minuscule
     * (present en $60-$7F dans l'alternatif) ; encode() replie la casse. */
    SW(CLRALTCHAR) = 0;

    mode80 = detect_aux();
    if (mode80) {
        SW(SET80VID) = 0;             /* affichage 80 colonnes */
        SW(SET80COL) = 0;             /* 80STORE on */
        scr_cols = 80;
    } else {
        SW(CLR80VID) = 0;             /* affichage 40 colonnes */
        SW(CLR80COL) = 0;
        scr_cols = 40;
    }
    SW(TXTSET) = 0;                   /* mode texte */
    inv = 0;
    scr_clear();
}

void scr_clear(void)
{
    u8 y;
    /* Dans une rangee, les 40 octets sont contigus -> memset par ligne.
     * On ne bascule aux/main qu'UNE fois (au lieu d'une fois par cellule). */
    if (mode80) {
        SW(RDCARDRAM) = 0;                 /* aux : colonnes paires */
        for (y = 0; y < 24; ++y)
            memset((void *)rowbase[y], SCR_SPACE, 40);
        SW(RDMAINRAM) = 0;                 /* main : colonnes impaires */
    }
    for (y = 0; y < 24; ++y)               /* memoire principale (40 ou 80 col) */
        memset((void *)rowbase[y], SCR_SPACE, 40);
    cx = 0;
    cy = 0;
}

static void newline(void)
{
    cx = 0;
    if (cy < 23)
        ++cy;                         /* pas de scroll : les sections tiennent */
}

void scr_putc(char c)
{
    if (c == '\r') { cx = 0; return; }
    if (c == '\n') { newline(); return; }
    put_at(cx, cy, encode(c));
    if (++cx >= scr_cols)
        newline();
}

void scr_puts(const char *s)
{
    while (*s)
        scr_putc(*s++);
}

void scr_revers(u8 on)
{
    inv = on;
}

void scr_gotoxy(u8 x, u8 y)
{
    cx = x;
    cy = y;
}

char scr_getkey(void)
{
    char c;
    while ((SW(KBD) & 0x80) == 0)
        ++scr_entropy;                /* le temps de reaction humain sert d'entropie */
    c = (char)(SW(KBD) & 0x7F);
    SW(KBDSTRB) = 0;                  /* efface le strobe */
    return c;
}

char scr_poll(void)
{
    char c;
    if ((SW(KBD) & 0x80) == 0)
        return 0;                     /* aucune touche */
    c = (char)(SW(KBD) & 0x7F);
    SW(KBDSTRB) = 0;
    return c;
}

/* Vide le verrou clavier (jette une touche en attente). A appeler juste avant
 * une attente de touche, apres une operation lente (chargement d'image) : evite
 * qu'une touche pressee par impatience ne saute la scene des son affichage. */
void scr_flush(void)
{
    SW(KBDSTRB) = 0;
}

/* Efface le dernier caractere affiche (recule d'une colonne, ecrit un espace). */
void scr_backspace(void)
{
    if (cx > 0) {
        --cx;
        put_at(cx, cy, encode(' '));
    }
}

/* Lit une ligne au clavier (echo, retour arriere) jusqu'a Entree.
 * Ecrit la chaine terminee par 0 dans buf ; renvoie sa longueur. */
u8 scr_readline(char *buf, u8 maxlen)
{
    u8 n = 0;
    char c;
    for (;;) {
        c = scr_getkey();
        if (c == 13)                  /* Entree (CR) */
            break;
        if (c == 8 || c == 127) {     /* retour arriere / suppr */
            if (n) { --n; scr_backspace(); }
            continue;
        }
        if (c >= 32 && n < maxlen) {  /* caractere imprimable */
            buf[n++] = c;
            scr_putc(c);
        }
    }
    buf[n] = '\0';
    return n;
}

u8 scr_vbl(void)
{
    return (u8)(SW(0xC019) & 0x80);   /* RDVBLBAR : bascule a ~60 Hz sur //e */
}

/* --- Affichage graphique HIRES ---------------------------------------- */

void scr_gfx_on(void)
{
    SW(CLR80COL) = 0;    /* 80STORE off : PAGE1 selectionne bien $2000 en main */
    SW(TXTCLR)  = 0;     /* graphique */
    SW(MIXCLR)  = 0;     /* plein ecran */
    SW(RDMAINRAM) = 0;   /* $C054 = page graphique 1 */
    SW(HIRES_ON) = 0;    /* haute resolution */
}

/* Mode semi-graphique : image HIRES en haut, fenetre texte 40 col (4 lignes)
 * en bas. Le texte s'ecrit en 40 col dans les lignes 20-23. */
void scr_gfx_mixed(void)
{
    u8 y;
    SW(CLR80COL) = 0;    /* 80STORE off : hires page1 = main, texte bas = 40 col */
    SW(CLR80VID) = 0;    /* affichage texte 40 col (fenetre du bas) */
    force40 = 1;
    for (y = 20; y < 24; ++y)        /* efface les 4 lignes de texte du bas */
        memset((void *)rowbase[y], SCR_SPACE, 40);
    SW(TXTCLR)  = 0;     /* graphique */
    SW(MIXSET)  = 0;     /* 4 lignes de texte en bas */
    SW(RDMAINRAM) = 0;   /* page graphique 1 */
    SW(HIRES_ON) = 0;
    scr_cols = 40;       /* la fenetre du bas est en 40 col */
    cx = 0;
    cy = 20;             /* le texte demarre a la 1ere des 4 lignes */
}

void scr_gfx_off(void)
{
    force40 = 0;
    if (mode80) {                    /* restaure le mode texte du player */
        SW(SET80VID) = 0;
        SW(SET80COL) = 0;
    } else {
        SW(CLR80VID) = 0;
        SW(CLR80COL) = 0;
    }
    scr_cols = mode80 ? 80 : 40;      /* restaure la largeur (quittee du mixte 40) */
    /* Efface la page texte AVANT de la rallumer : sinon l'ancien contenu
     * (menu, section precedente...) reapparait un instant ("effet memoire"). */
    scr_clear();
    SW(HIRES_OFF) = 0;
    SW(MIXCLR) = 0;      /* quitte le mode mixte si on y etait */
    SW(TXTSET) = 0;      /* bascule affichage texte -> ecran deja vide */
}

#define HGR_BLK  512
#define HGR_NBLK  (HGR_SIZE / HGR_BLK)   /* 16 blocs de 512 o */

signed char scr_load_hgr(const char *name, scr_progress_cb cb)
{
    FILE *f = fopen(name, "rb");
    unsigned char *p = HGR_PAGE;
    u8 i;
    if (f == NULL)
        return -1;
    for (i = 0; i < HGR_NBLK; ++i) {
        if (fread(p, 1, HGR_BLK, f) != HGR_BLK) {
            fclose(f);
            return -2;
        }
        p += HGR_BLK;
        if (cb != NULL)
            cb((u16)((i + 1) * HGR_BLK), HGR_SIZE);
    }
    fclose(f);
    return 0;
}
