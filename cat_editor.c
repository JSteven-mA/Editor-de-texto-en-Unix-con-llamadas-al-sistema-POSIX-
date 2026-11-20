#include "shell.h"
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * ====================================================================================
 * CATEGORÍA: editor  (aplicaciones interactivas en primer plano)
 * ====================================================================================
 * Por qué una categoría NUEVA y no "monitoreo" (p_exec) ni "datos":
 *  - p_exec es un lanzador GENÉRICO de demostración (fork + execvp + waitpid) dentro de la
 *    categoría "monitoreo" (procesos, señales, recursos). Técnicamente el mecanismo es el
 *    mismo que usa e_edit, y un hijo hereda STDIN/STDOUT, así que 'p_exec ./editor f.txt'
 *    también funcionaría. La diferencia es de DISEÑO, no de mecanismo:
 *      1) p_exec obliga al usuario a conocer la ruta del binario (se resuelve contra el
 *         directorio actual); e_edit es un comando propio que ubica 'editor' junto al
 *         shell con readlink("/proc/self/exe"), sin importar desde dónde se ejecute.
 *      2) e_edit valida sus argumentos, informa fallos de execv y propaga el código de
 *         salida del editor; p_exec es deliberadamente genérico.
 *      3) El editor es una aplicación interactiva con su propia sesión de comandos
 *         (o, p, a, d, q) que toma STDIN/STDOUT durante toda su vida. Esa clase de
 *         programas (editores, pagers) merece su propia categoría para documentar y
 *         tratar su contrato con la terminal (p. ej. stdin sin búfer, ver main.c).
 *      4) Organización: "monitoreo" agrupa observación de procesos y recursos; el editor
 *         es una herramienta de trabajo con archivos, no de monitoreo.
 *  - Corre como proceso aparte: si el editor falla, el shell sigue vivo.
 *
 * COMANDO: e_edit [archivo]
 * Syscalls explicadas:
 *  1. readlink(2) sobre /proc/self/exe: localiza el binario del shell para hallar 'editor'
 *     en la misma carpeta (funciona sin importar desde dónde se ejecute el shell).
 *  2. fork(2): clona el proceso. Retorna 0 en el hijo y el PID del hijo en el padre.
 *  3. execv(3): reemplaza la imagen del hijo por el editor (hereda STDIN/STDOUT).
 *  4. waitpid(2): el shell se bloquea hasta que el editor termina y obtiene su estado.
 */
int cmd_e_edit(int argc, char **argv) {
    if (argc > 2) {
        printf(COLOR_ERROR "Uso: e_edit [archivo]\n" COLOR_RESET);
        return 1;
    }

    /* Ruta absoluta del editor: <carpeta del shell>/editor */
    char exe[PATH_MAX], editor_path[PATH_MAX + 16];
    LOG_SYSCALL("readlink", "\"/proc/self/exe\"");
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n < 0) {
        LOG_SYSCALL_ERROR(strerror(errno));
        snprintf(editor_path, sizeof(editor_path), "./editor");   /* plan B */
    } else {
        exe[n] = '\0';
        LOG_SYSCALL_RESULT(n);
        snprintf(editor_path, sizeof(editor_path), "%s/editor", dirname(exe));
    }

    char *ed_argv[3] = { "editor", argc == 2 ? argv[1] : NULL, NULL };

    LOG_SYSCALL("fork", "");
    /* Vaciar stdout ANTES del fork: si no, el hijo hereda una copia del buffer
     * y el texto pendiente podría imprimirse dos veces. */
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) {
        LOG_SYSCALL_ERROR(strerror(errno));
        return 1;
    }

    if (pid == 0) {                                   /* ---- HIJO ---- */
        execv(editor_path, ed_argv);
        fprintf(stderr, COLOR_ERROR "execv(%s): %s\n" COLOR_RESET, editor_path, strerror(errno));
        _exit(127);                                   /* solo si execv falló */
    }

    LOG_SYSCALL_RESULT(pid);                          /* ---- PADRE ---- */
    LOG_SYSCALL("waitpid", "%d, &status, 0", (int)pid);
    fflush(stdout);

    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            LOG_SYSCALL_ERROR(strerror(errno));
            return 1;
        }
    }
    LOG_SYSCALL_RESULT(pid);

    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code == 0) printf(COLOR_INFO "Editor finalizado (código 0).\n" COLOR_RESET);
        else           printf(COLOR_ERROR "Editor finalizó con código %d.\n" COLOR_RESET, code);
        return code;
    }
    printf(COLOR_ERROR "El editor terminó por una señal.\n" COLOR_RESET);
    return 1;
}
