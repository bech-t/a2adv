/* lang.c -- lecture de APP.LNG (cf. lang.h, spec §6.1).
 *
 * Disposition du fichier (offsets dans secbuf apres la lecture) :
 *   0-3  "A2LG"
 *   4    version
 *   5-6  code de langue ("fr", "en", ...) — informatif, non utilise ici
 *   7    n_keys
 *   8+   n_keys x (u8 longueur + octets), dans l'ordre fige de l'enum UI_*
 */

#include <stdio.h>
#include "lang.h"
#include "story.h"      /* ui_str */

#define LANG_FILE "APP.LNG"

signed char lang_load(void)
{
    FILE *f;
    u16   n, p;
    u8    nkeys, i;

    /* Le fichier tient dans secbuf (1 Ko), libre a ce stade : aucune section
     * n'est chargee et ram_boot_fill n'a pas encore commence. Une lecture au
     * lieu de ~650 appels MLI — ProDOS aurait de toute facon servi la plupart
     * depuis son propre cache de bloc, on economise le surcout d'appel, pas
     * des acces disque (cf. la note de story.c). */
    f = fopen(LANG_FILE, "rb");
    if (f == NULL)
        return -1;
    n = (u16)fread(secbuf, 1, SECTION_MAX, f);
    fclose(f);

    if (n < 8 ||
        secbuf[0] != 'A' || secbuf[1] != '2' ||
        secbuf[2] != 'L' || secbuf[3] != 'G')
        return -2;
    if (secbuf[4] != LANG_FORMAT_VERSION)
        return -3;                  /* mieux vaut muet que lu de travers */

    nkeys = secbuf[7];              /* 5-6 : code de langue (informatif) */
    p = 8;
    for (i = 0; i < nkeys; ++i) {
        u8 len, k, keep;
        if (p >= n)                 /* fichier tronque : on garde l'acquis */
            return -4;
        len = secbuf[p++];
        keep = (len < UI_STR_LEN - 1) ? len : (u8)(UI_STR_LEN - 1);
        for (k = 0; k < len && p < n; ++k, ++p)
            if (i < UI_COUNT && k < keep)
                ui_str[i][k] = (char)secbuf[p];
        if (i < UI_COUNT)
            ui_str[i][keep] = '\0';
    }
    return 0;
}
