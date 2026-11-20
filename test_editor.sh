#!/bin/bash
# Script de pruebas del editor (caso individual). Uso: make test
ED=./editor
T=/tmp/ed_test_$$.txt
PASS=0; FAIL=0
rm -f "$T"

check() { # nombre, esperado, obtenido
  if [ "$2" == "$3" ]; then echo "  [OK]   $1"; PASS=$((PASS+1));
  else echo "  [FAIL] $1"; echo "     esperado: $(printf %q "$2")"; echo "     obtenido: $(printf %q "$3")"; FAIL=$((FAIL+1)); fi
}
run() { printf "$1" | $ED 2>&1 >/dev/null | head -n1; }   # solo stderr

echo "== Comandos basicos =="
printf "o $T\na uno\na dos\na tres\nq\n" | $ED >/dev/null
check "o + a crean archivo con 3 lineas" $'uno\ndos\ntres' "$(cat $T)"

out=$(printf "o $T\np 2\nq\n" | $ED | grep -v '^ed>' | grep -v abierto | sed 's/ed> //')
check "p 2 imprime la linea 2" "dos" "$(printf "o $T\np 2\nq\n" | $ED | sed 's/ed> //g' | grep -x dos)"

check "p sin parametro imprime todo" "3" "$(printf "o $T\np\nq\n" | $ED | sed 's/ed> //g' | grep -cxE 'uno|dos|tres')"

printf "o $T\nd 2\nq\n" | $ED >/dev/null
check "d 2 borra linea intermedia" $'uno\ntres' "$(cat $T)"
printf "o $T\nd 2\nq\n" | $ED >/dev/null
check "d borra la ultima linea" "uno" "$(cat $T)"
printf "o $T\nd 1\nq\n" | $ED >/dev/null
check "d borra la unica linea (archivo vacio)" "0" "$(stat -c %s $T)"

echo "== Casos borde =="
check "p linea inexistente da error" "Error: la linea 9 no existe" "$(printf "o $T\np 9\nq\n" | $ED 2>&1 >/dev/null)"
check "d en archivo vacio da error" "Error: la linea 1 no existe" "$(printf "o $T\nd 1\nq\n" | $ED 2>&1 >/dev/null)"
check "p 0 / negativo rechazado" "Uso: p [n] (n entero >= 1)" "$(printf "o $T\np -3\nq\n" | $ED 2>&1 >/dev/null)"
check "d sin parametro rechazado" "Uso: d <n> (n entero >= 1)" "$(printf "o $T\nd\nq\n" | $ED 2>&1 >/dev/null)"
check "sin archivo abierto" "Error: no hay archivo abierto. Use: o <archivo>" "$(printf "p\nq\n" | $ED 2>&1 >/dev/null)"
check "comando invalido" "Comando desconocido 'z'. Validos: o p a d q" "$(printf "z\nq\n" | $ED 2>&1 >/dev/null)"
check "open en ruta invalida (perror)" "open: No such file or directory" "$(printf "o /no/existe/x.txt\nq\n" | $ED 2>&1 >/dev/null)"

rm -f $T
printf "sin_salto_final" > $T
printf "o $T\na nueva\nq\n" | $ED >/dev/null
check "a sobre archivo sin \\n final no pega lineas" $'sin_salto_final\nnueva' "$(cat $T)"
printf "o $T\nd 2\nq\n" | $ED >/dev/null
check "d de ultima linea con contenido" "sin_salto_final" "$(cat $T)"

echo "== Archivo grande (cruza bloques de 4096 bytes) =="
rm -f $T
for i in $(seq 1 2000); do echo "linea numero $i con relleno de texto"; done > $T
printf "o $T\nd 1000\nq\n" | $ED >/dev/null
sed '1000d' <(for i in $(seq 1 2000); do echo "linea numero $i con relleno de texto"; done) > /tmp/ed_expected_$$
check "d 1000 en archivo grande coincide con sed" "$(cat /tmp/ed_expected_$$)" "$(cat $T)"
check "p 1000 tras borrar es la antigua 1001" "linea numero 1001 con relleno de texto" "$(printf "o $T\np 1000\nq\n" | $ED | sed 's/ed> //g' | grep 'linea numero')"

echo "== Reabrir otro archivo / EOF =="
check "salida por EOF (Ctrl+D) sin error" "0" "$(printf "o $T\n" | $ED >/dev/null 2>&1; echo $?)"

if command -v valgrind >/dev/null; then
  echo "== Valgrind =="
  r=$(printf "o $T\na x\np\nd 1\nq\n" | valgrind --leak-check=full --error-exitcode=9 $ED >/dev/null 2>&1; echo $?)
  check "sin fugas de memoria ni fds abiertos" "0" "$r"
fi

rm -f $T /tmp/ed_expected_$$
echo; echo "Resultado: $PASS OK, $FAIL fallidas"
[ $FAIL -eq 0 ]
