/* editor.h - Editor de texto CLI basado en system calls POSIX (sin stdio para el archivo) */
#ifndef EDITOR_H
#define EDITOR_H

#include <sys/types.h>

#define ED_CHUNK 4096

typedef struct {
    int   fd;        /* File descriptor del archivo abierto (-1 si ninguno) */
    char *path;      /* Ruta (memoria dinamica, se libera en ed_close) */
} Editor;

void ed_init(Editor *e);
int  ed_open(Editor *e, const char *path);          /* comando o */
int  ed_print(Editor *e, long n);                   /* comando p (n=0 => todo) */
int  ed_append(Editor *e, const char *text);        /* comando a */
int  ed_delete(Editor *e, long n);                  /* comando d */
void ed_close(Editor *e);                           /* comando q (libera todo) */

#endif
