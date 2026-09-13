/* sysinfo.c -- info systeme Atari ST pour l'ecran Options (cf. sysinfo.h).
 * Version TOS (Sversion, GEMDOS), moniteur (Getrez, XBIOS -- meme appel que
 * scr_init pour detecter le mono, cf. scr.c, celui-la bien verifie a
 * l'ecran), RAM libre (Malloc(-1L), convention GEMDOS standard : renvoie la
 * taille du plus grand bloc libre sans rien allouer). Pas de detection de
 * modele (ST/STE/TT/Falcon) : pas tentee ici, cf. player/common/platform.md.
 *
 * ATTENTION : ce fichier entier n'est PAS ENCORE VERIFIE a l'ecran sous
 * Hatari ni sur materiel reel -- Sversion()/Malloc() reposent sur une
 * convention GEMDOS documentee, mais rien ici n'a ete rejoue pour de vrai. */

#include <osbind.h>
#include "sysinfo.h"
#include "format.h"
#include "scr.h"
#include "ui.h"

/* Ecrit v en decimal (pas de sprintf : coherent avec le reste du player,
 * cf. story.c/ramdisk.c qui evitent aussi la stdio lourde). */
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
    /* Sversion() (GEMDOS $30) : octet bas = version majeure, octet haut =
     * version mineure (ex. TOS 1.04 -> 0x0401) -- convention GEMDOS standard,
     * documentee, mais PAS ENCORE VERIFIEE a l'ecran sous Hatari/EmuTOS ni
     * sur ST reel (cf. player/atarist/README.md pour l'etat du portage). */
    short v = Sversion();
    u8 major = (u8)(v & 0xFF);
    u8 minor = (u8)(v >> 8);
    long free_ram = Malloc(-1L);

    scr_puts("MACHINE    : ATARI ST"); ui_newline();

    scr_puts("TOS        : ");
    put_u32(major);
    scr_puts(".");
    if (minor < 10)
        scr_puts("0");
    put_u32(minor);
    ui_newline();

    scr_puts("ECRAN      : ");
    scr_puts((Getrez() == 2) ? "MONOCHROME" : "COULEUR");
    ui_newline();

    scr_puts("RAM LIBRE  : ");
    put_u32((u32)(free_ram / 1024));
    scr_puts(" KO");
    ui_newline();
}
