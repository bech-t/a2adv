/* sysinfo.c -- info systeme Apple II pour l'ecran Options (cf. sysinfo.h).
 * Modele (get_ostype, fiable -- cf. scr.c ou la meme fonction distingue deja
 * le clavier majuscule seul du II/II+), colonnes actives, disque RAM
 * detecte. Pas de taille RAM totale : aucune detection materielle fiable et
 * verifiee n'existe pour ca a ce jour (cf. player/common/platform.md). */

#include <apple2.h>
#include "sysinfo.h"
#include "format.h"
#include "scr.h"
#include "ui.h"
#include "ramdisk.h"

static const char *model_name(void)
{
    switch (get_ostype()) {
    case APPLE_II:      return "APPLE ][";
    case APPLE_IIPLUS:  return "APPLE ][+";
    case APPLE_IIE:     return "APPLE //E";
    case APPLE_IIEENH:  return "APPLE //E (ENHANCED)";
    case APPLE_IIC:     return "APPLE //C";
    case APPLE_IIC35:   return "APPLE //C (ROM 3.5)";
    case APPLE_IICEXP:  return "APPLE //C (MEM. EXP.)";
    case APPLE_IICREV:  return "APPLE //C (REV. MEM.)";
    case APPLE_IICPLUS: return "APPLE //C+";
    case APPLE_IIGS:
    case APPLE_IIGS1:
    case APPLE_IIGS3:   return "APPLE IIGS";
    default:            return "APPLE II (MODELE INCONNU)";
    }
}

void sys_info(void)
{
    scr_puts("MACHINE    : "); scr_puts(model_name());                       ui_newline();
    scr_puts("ECRAN      : "); scr_puts(scr_cols == 80 ? "80 COLONNES"
                                                        : "40 COLONNES");     ui_newline();
    scr_puts("DISQUE RAM : "); scr_puts(ram_ready() ? "DETECTE" : "ABSENT"); ui_newline();
}
