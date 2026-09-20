/* sysinfo.c -- info systeme DOS pour l'ecran Options (cf. sysinfo.h).
 * Version DOS (INT 21h/AH=30h), mode video actif (INT 10h/AH=0Fh), memoire
 * conventionnelle installee (INT 12h -- meme convention BIOS que le
 * Malloc(-1L) GEMDOS utilise cote Atari ST, cf. atarist/src/sysinfo.c :
 * un fait fourni par une API standard documentee, pas une sonde materielle
 * maison, cf. player/common/platform.md point 6). Pas de detection EGA/VGA
 * ici : le mode video actif suffit a distinguer les deux a l'usage (13h =
 * VGA actif, 03h = repli texte si le mode 13h a echoue au dernier chargement
 * d'image, cf. scr.c). */

#include <i86.h>
#include "sysinfo.h"
#include "format.h"
#include "scr.h"
#include "ui.h"

static void put_u32(u32 v)
{
    char buf[11];
    u8 i = 10;
    buf[i] = '\0';
    if (v == 0) {
        buf[--i] = '0';
    } else {
        while (v > 0) {
            buf[--i] = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    scr_puts(&buf[i]);
}

void sys_info(void)
{
    union REGS r;
    u8 major, minor, mode;
    u16 ram_kb;

    r.h.ah = 0x30;
    int86(0x21, &r, &r);
    major = r.h.al;
    minor = r.h.ah;

    r.h.ah = 0x0F;
    int86(0x10, &r, &r);
    mode = r.h.al;

    int86(0x12, &r, &r);
    ram_kb = r.x.ax;

    scr_puts("MACHINE    : PC (DOS)"); ui_newline();

    scr_puts("DOS        : ");
    put_u32(major);
    scr_puts(".");
    put_u32(minor);
    ui_newline();

    scr_puts("ECRAN      : ");
    scr_puts((mode == 0x13) ? "VGA 320X200/256C" : "TEXTE 80X25");
    ui_newline();

    scr_puts("RAM (CONV.): ");
    put_u32(ram_kb);
    scr_puts(" KO");
    ui_newline();
}
