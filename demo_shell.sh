#!/bin/bash
# demo_shell.sh - Pruebas de integracion shell (eafitOS) + editor (comando e_edit).
# Uso (con ./eafitOS y ./editor ya compilados):  ./demo_shell.sh
SH=./eafitOS
D=$(mktemp -d /tmp/demo_shell.XXXXXX)
PASS=0; FAIL=0
trap 'rm -rf "$D"' EXIT

[ -x "$SH" ] && [ -x ./editor ] || { echo "Compila primero con 'make' (faltan $SH o ./editor)"; exit 1; }

# Ejecuta el shell con la entrada dada ($1, estilo printf), sin colores ANSI, stdout+stderr juntos.
run() { printf "$1" | $SH 2>&1 | sed 's/\x1b\[[0-9;]*[A-Za-z]//g'; }

ok()   { echo "  [OK]   $1"; PASS=$((PASS+1)); }
bad()  { echo "  [FAIL] $1"; echo "         $2"; FAIL=$((FAIL+1)); }
has()  { if grep -qF -- "$3" <<<"$2"; then ok "$1"; else bad "$1" "no aparece: $3"; fi; }
eq()   { if [ "$2" == "$3" ]; then ok "$1"; else bad "$1" "esperado=$(printf %q "$2") obtenido=$(printf %q "$3")"; fi; }

echo "== 1. Registro del comando en el shell =="
out=$(run 'help\nhelp editor\nhelp e_edit\nexit\n')
has "help lista la categoria 'editor'"           "$out" "Aplicaciones interactivas en primer plano"
has "help editor muestra e_edit"                 "$out" "e_edit"
has "help e_edit muestra el uso"                 "$out" "e_edit [archivo]"
has "help e_edit muestra las syscalls"           "$out" "fork(2), execv(3), waitpid(2)"

echo "== 2. Flujo normal: shell -> editor -> shell =="
F="$D/a.txt"
out=$(run "e_edit $F\na uno\na dos\na tres\nd 2\np\nq\nhora\nexit\n")
eq  "el archivo quedo con las lineas 1 y 3"      $'uno\ntres' "$(cat $F)"
has "se traza la syscall fork"       "$out" "[syscall] fork()"
has "el editor termina con codigo 0"             "$out" "Editor finalizado (código 0)"
has "el shell sigue vivo tras 'q' (comando hora)" "$out" "Hora actual:"

echo "== 3. Abrir sin argumento y usar 'o' dentro del editor =="
G="$D/b.txt"
run "e_edit\no $G\na desde_o\nq\nexit\n" >/dev/null
eq  "e_edit sin archivo + o crea el archivo"      "desde_o" "$(cat $G)"

echo "== 4. Dos sesiones seguidas del editor =="
H="$D/c.txt"
run "e_edit $H\na primera\nq\ne_edit $H\na segunda\nq\nexit\n" >/dev/null
eq  "la segunda sesion agrega al mismo archivo"   $'primera\nsegunda' "$(cat $H)"

echo "== 5. Errores: el shell nunca debe caerse =="
out=$(run "e_edit uno dos\nhora\nexit\n")
has "demasiados argumentos -> mensaje de uso"     "$out" "Uso: e_edit [archivo]"
has "...y el shell sigue vivo"                    "$out" "Hora actual:"

out=$(run "e_edit /no/existe/x.txt\nq\nhora\nexit\n")
has "ruta invalida -> perror de open()"           "$out" "open: No such file or directory"
has "...y el shell sigue vivo"                    "$out" "Hora actual:"

out=$(run "e_edit $F\np 99\nd 99\np 0\nd\nz\nq\nhora\nexit\n")
has "linea inexistente (p 99) reportada"          "$out" "la linea 99 no existe"
has "argumento invalido reportado"                "$out" "Uso: p [n]"
has "comando desconocido reportado"               "$out" "Comando desconocido 'z'"
has "...y el shell sigue vivo"                    "$out" "Hora actual:"

echo "== 6. Editor ausente (execv falla) =="
mkdir "$D/sin_editor" && cp "$SH" "$D/sin_editor/eafitOS"
out=$(printf "e_edit x.txt\nhora\nexit\n" | "$D/sin_editor/eafitOS" 2>&1 | sed 's/\x1b\[[0-9;]*[A-Za-z]//g')
has "execv fallido informado"                     "$out" "execv("
has "codigo de salida 127 propagado"              "$out" "código 127"
has "...y el shell sigue vivo"                    "$out" "Hora actual:"

echo "== 7. Salida por EOF dentro del editor =="
out=$(printf "e_edit $F\na eof\n" | $SH 2>&1 | sed 's/\x1b\[[0-9;]*[A-Za-z]//g')
has "Ctrl+D en el editor devuelve el control"     "$out" "Editor finalizado (código 0)"
eq  "...y lo escrito antes quedo guardado"        "eof" "$(tail -n1 $F)"

if command -v strace >/dev/null 2>&1; then
  echo "== 8. Evidencia con strace (editor: solo syscalls, sin stdio para el archivo) =="
  T="$D/s.txt"
  tr=$(printf "a hola\na mundo\nd 1\nq\n" | strace -f -e trace=openat,read,write,lseek,ftruncate,close ./editor "$T" 2>&1 >/dev/null)
  has "usa openat sobre el archivo"               "$tr" "openat(AT_FDCWD, \"$T\", O_RDWR|O_CREAT"
  has "usa lseek"                                 "$tr" "lseek("
  has "usa ftruncate al borrar"                   "$tr" "ftruncate("
  has "cierra el descriptor (close)"              "$tr" "close(3)"
else
  echo "== 8. (strace no instalado: omitida; sudo apt install strace) =="
fi

echo; echo "Resultado: $PASS OK, $FAIL fallidas"
[ $FAIL -eq 0 ]
