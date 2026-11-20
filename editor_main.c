/* main.c - Ciclo interactivo (REPL). stdio solo para comandos (stdin) y consola (stdout). */
#include "editor.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *skip_ws(char *s) { while (*s == ' ' || *s == '\t') s++; return s; }

/* Convierte a entero positivo; retorna -1 si no es un numero valido. */
static long parse_num(const char *s)
{
    char *endp;
    errno = 0;
    long v = strtol(s, &endp, 10);
    if (errno || endp == s) return -1;
    endp = (char *)skip_ws(endp);
    if (*endp != '\0' || v < 1) return -1;
    return v;
}

int main(int argc, char *argv[])
{
    Editor ed;
    ed_init(&ed);

    /* stdin sin búfer: al salir con 'q' no dejamos consumidas las líneas que
     * pertenecen al shell que nos lanzó (importante con scripts/pipes). */
    setvbuf(stdin, NULL, _IONBF, 0);

    if (argc > 1) ed_open(&ed, argv[1]);      /* permite: ./editor archivo.txt */

    char line[4096];
    for (;;) {
        printf("ed> ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;      /* EOF (Ctrl+D) */

        line[strcspn(line, "\r\n")] = '\0';
        char *p = skip_ws(line);
        if (*p == '\0') continue;

        char cmd = *p;
        char *arg = p + 1;
        /* el comando debe ir solo o seguido de espacio */
        if (*arg != '\0' && *arg != ' ' && *arg != '\t') {
            fprintf(stderr, "Comando desconocido: '%s'\n", p);
            continue;
        }
        arg = skip_ws(arg);

        switch (cmd) {
        case 'o':
            ed_open(&ed, arg);
            break;
        case 'p':
            if (*arg == '\0') ed_print(&ed, 0);
            else {
                long n = parse_num(arg);
                if (n < 0) fprintf(stderr, "Uso: p [n] (n entero >= 1)\n");
                else ed_print(&ed, n);
            }
            break;
        case 'a':
            ed_append(&ed, arg);
            break;
        case 'd': {
            long n = parse_num(arg);
            if (n < 0) fprintf(stderr, "Uso: d <n> (n entero >= 1)\n");
            else ed_delete(&ed, n);
            break;
        }
        case 'q':
            ed_close(&ed);
            return 0;
        default:
            fprintf(stderr, "Comando desconocido '%c'. Validos: o p a d q\n", cmd);
        }
    }
    ed_close(&ed);       /* salida por EOF tambien sin fugas */
    return 0;
}
