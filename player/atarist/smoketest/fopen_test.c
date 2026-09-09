#include <stdio.h>

static void put_num(long n)
{
    char tmp[16];
    int t = 0;
    if (n < 0) { putchar('-'); n = -n; }
    if (n == 0) { putchar('0'); return; }
    while (n > 0) { tmp[t++] = (char)('0' + (n % 10)); n /= 10; }
    while (t > 0) putchar(tmp[--t]);
}

int main(void)
{
    FILE *f = fopen("STORY00.DAT", "rb");
    int r;
    unsigned char buf[4];
    size_t n;

    if (!f) { printf("FOPEN ECHOUE\r\n"); getchar(); return 0; }

    /* fseek AVANT tout fread -- isole si le bug vient d'une interaction
     * avec le tampon stdio ou de fseek() lui-meme. */
    r = fseek(f, 20L, SEEK_SET);
    printf("fseek immediat -> "); put_num((long)r); printf("\r\n");
    n = fread(buf, 1, 4, f);
    printf("fread: n="); put_num((long)n); printf(" octets=%02X %02X %02X %02X\r\n",
        buf[0], buf[1], buf[2], buf[3]);

    fclose(f);
    getchar();
    return 0;
}
