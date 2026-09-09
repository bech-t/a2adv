#include <osbind.h>

int main(void)
{
    Cconws("HELLO A2ADV SUR ATARI ST\r\n");
    Cconws("ACCENTS: \202\212\205\207\r\n"); /* codes CP437/ST : e' e` a` c-cedille */
    Cconws("APPUYEZ SUR UNE TOUCHE\r\n");
    Cnecin();
    return 0;
}
