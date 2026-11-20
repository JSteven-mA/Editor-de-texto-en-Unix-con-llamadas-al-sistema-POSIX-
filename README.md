# Editor de texto en Unix integrado en eafitOS

Evaluación práctica (primer parcial) de **Sistemas Operativos**, Universidad EAFIT.

**Autor:** Jaisson Steven Merchán Ardila (trabajo individual)

## Descripción

Editor de texto interactivo por línea de comandos, escrito en C para Linux. Lee, modifica y guarda archivos planos usando únicamente llamadas al sistema POSIX (`open`, `read`, `write`, `lseek`, `ftruncate`, `fstat`, `close`), sin `fopen`, `fread`, `fwrite` ni `fclose`.

Está integrado en el shell educativo **eafitOS** mediante una categoría nueva, `editor`, con el comando `e_edit`. El shell lanza el editor como proceso hijo (`fork` + `execv` + `waitpid`).

## Archivos

| Archivo | Contenido |
|---|---|
| `editor.h`, `editor.c` | Lógica del editor (comandos `o`, `p`, `a`, `d`, `q`) |
| `editor_main.c` | Ciclo interactivo del editor |
| `cat_editor.c` | Categoría nueva del shell: comando `e_edit` |
| `main.c`, `shell.h` | Shell eafitOS (con el comando `e_edit` registrado) |
| `cat_datos.c`, `cat_memoria.c`, `cat_monitoreo.c`, `cat_util.c` | Resto de categorías del shell |
| `Makefile` | Compilación (`all` y `clean`) |
| `test_editor.sh` | Pruebas automáticas del editor |
| `demo_shell.sh` | Pruebas de la integración shell + editor |

## Compilar

```bash
sudo apt install build-essential   # solo la primera vez
make clean && make
```

Al terminar, `make` abre el shell. Escribe `exit` para salir.

## Uso

```bash
./eafitOS
```

Dentro del shell:

```
help editor
e_edit prueba.txt
```

Comandos del editor:

| Comando | Descripción |
|---|---|
| `o [archivo]` | Abre un archivo (lo crea si no existe) |
| `p [n]` | Imprime la línea `n`; sin número, imprime todo el archivo |
| `a [texto]` | Añade una línea al final |
| `d n` | Borra la línea `n` |
| `q` | Cierra el archivo y vuelve al shell |

Las líneas se numeran desde 1. Los cambios se escriben directamente en disco.

## Pruebas

Con el shell y el editor ya compilados:

```bash
./test_editor.sh    # pruebas del editor
./demo_shell.sh     # pruebas de integración con el shell
```

Si los scripts no tienen permiso de ejecución: `chmod +x test_editor.sh demo_shell.sh`.
