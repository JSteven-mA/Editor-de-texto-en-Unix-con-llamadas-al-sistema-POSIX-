/* editor.c - Implementacion: solo open/read/write/lseek/ftruncate/close/fstat */
#include "editor.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>     /* solo perror/fprintf para mensajes de error */
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---------- utilidades de bajo nivel ---------- */

/* write() puede escribir menos bytes de los pedidos: se repite hasta completar. */
static int write_all(int fd, const char *buf, size_t len)
{
    size_t done = 0;
    while (done < len) {
        ssize_t w = write(fd, buf + done, len - done);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        done += (size_t)w;
    }
    return 0;
}

/* read() con reintento ante EINTR. */
static ssize_t read_retry(int fd, char *buf, size_t len)
{
    ssize_t r;
    do { r = read(fd, buf, len); } while (r < 0 && errno == EINTR);
    return r;
}

static int require_open(const Editor *e)
{
    if (e->fd < 0) {
        fprintf(stderr, "Error: no hay archivo abierto. Use: o <archivo>\n");
        return -1;
    }
    return 0;
}

static off_t file_size(int fd)
{
    struct stat st;
    if (fstat(fd, &st) < 0) return -1;
    return st.st_size;
}

/*
 * Localiza la linea n (1-based). Recorre el archivo byte a byte (por bloques)
 * hasta encontrar los '\n'. *start = offset del primer byte de la linea,
 * *end = offset justo despues del '\n' (o EOF si la ultima linea no lo tiene).
 * Retorna 0 si existe, 1 si la linea no existe, -1 si hubo error de I/O.
 */
static int line_bounds(int fd, long n, off_t *start, off_t *end)
{
    char *buf = malloc(ED_CHUNK);
    if (!buf) { perror("malloc"); return -1; }
    if (lseek(fd, 0, SEEK_SET) < 0) { perror("lseek"); free(buf); return -1; }

    long cur = 1;
    off_t pos = 0, line_start = 0;
    int found = 0;
    ssize_t r;

    while (!found && (r = read_retry(fd, buf, ED_CHUNK)) > 0) {
        for (ssize_t i = 0; i < r; i++) {
            pos++;
            if (buf[i] == '\n') {
                if (cur == n) { *start = line_start; *end = pos; found = 1; break; }
                cur++;
                line_start = pos;
            }
        }
    }
    if (r < 0 && !found) { perror("read"); free(buf); return -1; }

    /* Ultima linea sin '\n' final */
    if (!found && cur == n && pos > line_start) {
        *start = line_start; *end = pos; found = 1;
    }
    free(buf);
    return found ? 0 : 1;
}

/* ---------- API del editor ---------- */

void ed_init(Editor *e) { e->fd = -1; e->path = NULL; }

void ed_close(Editor *e)
{
    if (e->fd >= 0 && close(e->fd) < 0) perror("close");
    e->fd = -1;
    free(e->path);
    e->path = NULL;
}

int ed_open(Editor *e, const char *path)
{
    if (!path || !*path) {
        fprintf(stderr, "Uso: o <archivo>\n");
        return -1;
    }
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) { perror("open"); return -1; }

    char *copy = strdup(path);
    if (!copy) { perror("strdup"); close(fd); return -1; }

    ed_close(e);               /* cierra el anterior (si existia) sin fugas */
    e->fd = fd;
    e->path = copy;
    printf("Archivo '%s' abierto (fd=%d)\n", e->path, e->fd);
    return 0;
}

int ed_print(Editor *e, long n)
{
    if (require_open(e) < 0) return -1;

    char *buf = malloc(ED_CHUNK);
    if (!buf) { perror("malloc"); return -1; }
    int rc = 0;
    off_t size = file_size(e->fd);
    if (size < 0) { perror("fstat"); free(buf); return -1; }

    if (n == 0) {                              /* imprimir todo */
        if (lseek(e->fd, 0, SEEK_SET) < 0) { perror("lseek"); free(buf); return -1; }
        ssize_t r;
        char last = '\n';
        while ((r = read_retry(e->fd, buf, ED_CHUNK)) > 0) {
            if (write_all(STDOUT_FILENO, buf, (size_t)r) < 0) { perror("write"); rc = -1; break; }
            last = buf[r - 1];
        }
        if (r < 0) { perror("read"); rc = -1; }
        if (rc == 0 && size > 0 && last != '\n') write_all(STDOUT_FILENO, "\n", 1);
    } else {                                   /* imprimir linea n */
        off_t s, en;
        int f = line_bounds(e->fd, n, &s, &en);
        if (f < 0) rc = -1;
        else if (f == 1) { fprintf(stderr, "Error: la linea %ld no existe\n", n); rc = -1; }
        else {
            if (lseek(e->fd, s, SEEK_SET) < 0) { perror("lseek"); rc = -1; }
            else {
                off_t left = en - s;
                char lastc = '\n';
                while (left > 0) {
                    size_t want = left > ED_CHUNK ? ED_CHUNK : (size_t)left;
                    ssize_t r = read_retry(e->fd, buf, want);
                    if (r <= 0) { if (r < 0) perror("read"); rc = -1; break; }
                    if (write_all(STDOUT_FILENO, buf, (size_t)r) < 0) { perror("write"); rc = -1; break; }
                    lastc = buf[r - 1];
                    left -= r;
                }
                if (rc == 0 && lastc != '\n') write_all(STDOUT_FILENO, "\n", 1);
            }
        }
    }
    free(buf);
    return rc;
}

int ed_append(Editor *e, const char *text)
{
    if (require_open(e) < 0) return -1;
    if (!text) text = "";

    off_t size = file_size(e->fd);
    if (size < 0) { perror("fstat"); return -1; }

    /* Si el archivo no termina en '\n', lo agregamos para no pegar lineas */
    if (size > 0) {
        char c;
        if (lseek(e->fd, -1, SEEK_END) < 0) { perror("lseek"); return -1; }
        if (read_retry(e->fd, &c, 1) != 1) { perror("read"); return -1; }
        if (c != '\n' && write_all(e->fd, "\n", 1) < 0) { perror("write"); return -1; }
    }
    if (lseek(e->fd, 0, SEEK_END) < 0) { perror("lseek"); return -1; }

    size_t len = strlen(text);
    char *line = malloc(len + 1);
    if (!line) { perror("malloc"); return -1; }
    memcpy(line, text, len);
    line[len] = '\n';
    int rc = write_all(e->fd, line, len + 1);   /* una sola escritura por linea */
    if (rc < 0) perror("write");
    free(line);
    return rc;
}

int ed_delete(Editor *e, long n)
{
    if (require_open(e) < 0) return -1;
    if (n < 1) { fprintf(stderr, "Uso: d <n> (n >= 1)\n"); return -1; }

    off_t s, en;
    int f = line_bounds(e->fd, n, &s, &en);
    if (f < 0) return -1;
    if (f == 1) { fprintf(stderr, "Error: la linea %ld no existe\n", n); return -1; }

    off_t size = file_size(e->fd);
    if (size < 0) { perror("fstat"); return -1; }

    char *buf = malloc(ED_CHUNK);
    if (!buf) { perror("malloc"); return -1; }

    /* Desplazar hacia atras los bytes posteriores a la linea, por bloques */
    off_t src = en, dst = s;
    while (src < size) {
        size_t want = (size - src) > ED_CHUNK ? ED_CHUNK : (size_t)(size - src);
        if (lseek(e->fd, src, SEEK_SET) < 0) { perror("lseek"); free(buf); return -1; }
        ssize_t r = read_retry(e->fd, buf, want);
        if (r <= 0) { if (r < 0) perror("read"); free(buf); return -1; }
        if (lseek(e->fd, dst, SEEK_SET) < 0) { perror("lseek"); free(buf); return -1; }
        if (write_all(e->fd, buf, (size_t)r) < 0) { perror("write"); free(buf); return -1; }
        src += r;
        dst += r;
    }
    free(buf);

    if (ftruncate(e->fd, size - (en - s)) < 0) { perror("ftruncate"); return -1; }
    return 0;
}
