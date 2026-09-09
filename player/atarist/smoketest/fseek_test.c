/* Test de diagnostic (garde volontairement) : `fseek()` de la libc mintlib
 * (paquet cross-mint-essential, jammy) echoue sur un fichier ouvert par
 * fopen() sous l'emulation GEMDOS-HDD de Hatari -- confirme en pratique le
 * 2026-09-09 (cf. spec-atarist.md §9/§11). L'appel GEMDOS brut Fseek(), lui,
 * fonctionne parfaitement : le bug est dans la couche stdio de mintlib, pas
 * dans GEMDOS/Hatari. story.c (copie ST) devra contourner fseek() -- via le
 * handle GEMDOS brut plutot que fseek() -- avant que le chargement d'une
 * aventure ne marche.
 *
 * A relancer si mintlib est mis a jour, ou sur une vraie image disquette
 * FAT12 plutot qu'un dossier hote monte en GEMDOS-HDD (cf. §3), pour voir si
 * le probleme persiste ailleurs qu'ici. */
#include <osbind.h>

static void put_num(long n)
{
    char tmp[16];
    int t = 0;
    if (n < 0) { Cconout('-'); n = -n; }
    if (n == 0) { Cconout('0'); return; }
    while (n > 0) { tmp[t++] = (char)('0' + (n % 10)); n /= 10; }
    while (t > 0) Cconout(tmp[--t]);
}

int main(void)
{
    long h = Fopen("STORY00.DAT", 0); /* GEMDOS brut : marche */
    long r;
    unsigned char buf[4];

    if (h < 0) { Cconws("Fopen (brut) ECHOUE\r\n"); Cnecin(); return 0; }

    r = Fseek(20L, h, 0); /* 0 = SEEK_SET */
    Cconws("Fseek brut(20, SEEK_SET) -> "); put_num(r); Cconws("\r\n");

    r = Fread(h, 4L, buf);
    Cconws("Fread apres seek: n="); put_num(r); Cconws(" octets=");
    {
        int i;
        static const char *H = "0123456789ABCDEF";
        for (i = 0; i < (int)r; ++i) {
            Cconout(H[(buf[i] >> 4) & 0xF]);
            Cconout(H[buf[i] & 0xF]);
            Cconout(' ');
        }
    }
    Cconws("\r\n(attendu si STORY00.DAT = combat_demo : 08 00 0C 0E)\r\n");

    Fclose(h);
    Cnecin();
    return 0;
}
