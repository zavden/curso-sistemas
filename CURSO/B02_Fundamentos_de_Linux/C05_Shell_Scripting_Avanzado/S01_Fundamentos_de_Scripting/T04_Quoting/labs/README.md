# Lab — Quoting

## Objetivo

Dominar el quoting en bash: cuando usar comillas dobles (expansion
de variables), comillas simples (literal), $'...' (ANSI-C quoting),
entender el word splitting y globbing accidental, y las diferencias
fundamentales entre [[ ]] y [ ].

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Comillas dobles y simples

### Objetivo

Entender que se expande dentro de cada tipo de comillas y cuando
usar cada una.

### Paso 1.1: Comillas dobles — expansion controlada

```bash
docker compose exec debian-dev bash -c '
echo "=== Comillas dobles ==="
echo ""
NAME="World"
echo "--- Dentro de comillas dobles se expanden ---"
echo "  Variables: Hello, $NAME"
echo "  Comandos: $(date +%H:%M)"
echo "  Aritmetica: $((2 + 3))"
echo "  Backslash escapes: \" \$ \\"
echo ""
echo "--- NO se expande ---"
echo "  Globbing: *.txt (literal, no se expande)"
echo "  Word splitting: \"hello   world\" (espacios preservados)"
echo ""
echo "--- Verificar con un archivo ---"
echo "hello   world   spaces" > /tmp/test-spaces.txt
CONTENT=$(cat /tmp/test-spaces.txt)
echo "Sin comillas: $CONTENT"
echo "Con comillas: \"$CONTENT\""
rm /tmp/test-spaces.txt
'
```

### Paso 1.2: Comillas simples — literal absoluto

```bash
docker compose exec debian-dev bash -c '
echo "=== Comillas simples ==="
echo ""
NAME="World"
echo "--- NADA se expande dentro de comillas simples ---"
echo '\''$NAME $(date) $((2+3)) *.txt'\''
echo ""
echo "Todo es literal: $, (, *, \\ — nada especial"
echo ""
echo "--- Cuando usar comillas simples ---"
echo "  grep '\''error|warning'\'' log.txt"
echo "  awk '\''{print $1}'\'' file.txt"
echo "  sed '\''s/old/new/g'\'' file.txt"
echo ""
echo "En awk/sed/grep, las comillas simples evitan que"
echo "bash interprete $ y otros caracteres especiales"
'
```

### Paso 1.3: $'...' — ANSI-C quoting

```bash
docker compose exec debian-dev bash -c '
echo "=== ANSI-C quoting ==="
echo ""
echo "--- Caracteres especiales con \$'\''...'\'' ---"
echo $'\''Linea 1\nLinea 2\nLinea 3'\''
echo ""
echo "Tab entre columnas:"
echo $'\''Col1\tCol2\tCol3'\''
echo ""
echo "=== Secuencias soportadas ==="
echo "\n  = newline"
echo "\t  = tab"
echo "\r  = carriage return"
echo "\\\\  = backslash literal"
echo "\\'\''  = comilla simple literal"
echo "\a  = bell (alerta)"
echo "\x41 = hex (A)"
echo "\101 = octal (A)"
echo ""
echo "--- Uso practico: comilla simple dentro de string ---"
echo $'\''It'\''s a test'\''
echo ""
echo "--- Comparar ---"
echo "echo \"tab: \t\"  → literal \\t (no funciona en echo sin -e)"
echo "echo \$'\''tab: \t'\'' → tab real"
'
```

### Paso 1.4: Mezclar tipos de comillas

```bash
docker compose exec debian-dev bash -c '
echo "=== Mezclar comillas ==="
echo ""
NAME="admin"
echo "--- Concatenar dobles y simples ---"
echo "El usuario '"'"'$NAME'"'"' tiene acceso"
echo "El usuario '\''$NAME'\'' tiene acceso"
echo ""
echo "--- Forma practica: solo dobles con escape ---"
echo "El usuario \"\$NAME\" literal: \$NAME"
echo "El usuario \"$NAME\" expandido: $NAME"
echo ""
echo "--- \$() dentro de comillas dobles ---"
echo "Son $(wc -l < /etc/passwd) usuarios"
echo ""
echo "--- Variable con espacios ---"
FILE="mi archivo.txt"
echo "Siempre entre comillas: \"$FILE\""
echo "Sin comillas se rompe"
'
```

---

## Parte 2 — Word splitting y globbing

### Objetivo

Entender los dos peligros principales de no poner comillas:
word splitting (separacion por espacios) y globbing (expansion
de comodines).

### Paso 2.1: Word splitting

Antes de ejecutar, predice: si `FILE="mi archivo.txt"`, cuantos
argumentos recibe `ls` con `ls $FILE` vs `ls "$FILE"`?

```bash
docker compose exec debian-dev bash -c '
echo "=== Word splitting ==="
echo ""
echo "Bash divide strings sin comillas por IFS (espacio, tab, newline)"
echo ""
echo "--- El bug clasico ---"
FILE="mi archivo.txt"
touch "/tmp/mi" "/tmp/archivo.txt" 2>/dev/null || true
echo "Sin comillas: ls intenta dos archivos separados"
echo "  ls $FILE → ls mi archivo.txt (2 argumentos)"
ls $FILE 2>&1 || true
echo ""
echo "Con comillas: ls recibe un solo argumento"
touch "/tmp/$FILE"
echo "  ls \"$FILE\" → ls \"mi archivo.txt\" (1 argumento)"
ls "/tmp/$FILE"
rm -f "/tmp/mi" "/tmp/archivo.txt" "/tmp/$FILE"
echo ""
echo "--- IFS controla la separacion ---"
DATA="uno:dos:tres"
echo "IFS default (espacio):"
for word in $DATA; do echo "  [$word]"; done
echo ""
echo "IFS=: (separar por :):"
IFS=: read -ra PARTS <<< "$DATA"
for word in "${PARTS[@]}"; do echo "  [$word]"; done
'
```

### Paso 2.2: Globbing accidental

```bash
docker compose exec debian-dev bash -c '
echo "=== Globbing accidental ==="
echo ""
echo "Bash expande * ? [...] en nombres de archivos si no estan entre comillas"
echo ""
echo "--- Demostrar el peligro ---"
cd /tmp
touch file1.txt file2.txt file3.txt
MSG="Los archivos *.txt estan listos"
echo "Sin comillas: $MSG"
echo "  *.txt se expandio a los archivos reales!"
echo ""
echo "Con comillas: \"$MSG\""
echo "  (NO se expande — pero esto es porque ya se expandio en la asignacion)"
echo ""
echo "--- Caso peligroso real ---"
PATTERN="*.txt"
echo "Sin comillas:"
for f in $PATTERN; do echo "  $f"; done
echo ""
echo "Con comillas (literal):"
for f in "$PATTERN"; do echo "  $f"; done
rm -f file1.txt file2.txt file3.txt
echo ""
echo "--- set -f: deshabilitar globbing ---"
echo "set -f desactiva la expansion de comodines"
echo "set +f la reactiva"
'
```

### Paso 2.3: Donde las comillas son criticas

```bash
docker compose exec debian-dev bash -c '
echo "=== Donde siempre usar comillas ==="
echo ""
echo "1. Variables en argumentos:"
echo "   rm \$FILE      → peligroso"
echo "   rm \"\$FILE\"    → correcto"
echo ""
echo "2. Variables en condiciones:"
echo "   [ -f \$FILE ]   → falla si FILE tiene espacios"
echo "   [ -f \"\$FILE\" ] → correcto"
echo ""
echo "3. Sustitucion de comandos:"
echo "   for f in \$(find . -name \"*.txt\")  → rompe con espacios"
echo "   while read -r f; do ...            → correcto"
echo ""
echo "4. Arrays:"
echo "   for item in \${arr[@]}   → word splitting"
echo "   for item in \"\${arr[@]}\" → correcto"
echo ""
echo "=== Donde NO poner comillas ==="
echo "  Globs intencionales: for f in *.txt"
echo "  Pattern en case:     case \$x in *.txt)"
echo "  Asignaciones:        VAR=\$(cmd)  (no necesita comillas)"
echo "  Dentro de [[ ]]:     [[ \$x == pattern* ]]"
'
```

---

## Parte 3 — [[ ]] vs [ ]

### Objetivo

Entender las diferencias entre el comando test ([ ]) y la
keyword de bash [[ ]], y cuando usar cada una.

### Paso 3.1: [ ] es un comando

```bash
docker compose exec debian-dev bash -c '
echo "=== [ ] es un comando externo/builtin ==="
echo ""
echo "--- [ es un programa real ---"
which [ 2>/dev/null || true
ls -la /usr/bin/[ 2>/dev/null || true
type [
echo ""
echo "[ es equivalente a test:"
echo "  [ -f /etc/passwd ] es lo mismo que test -f /etc/passwd"
echo "  El ] final es obligatorio (es un argumento de [)"
echo ""
echo "--- Problemas de [ con variables vacias ---"
EMPTY=""
echo "[ \$EMPTY == hello ]  → falla (se expande a [ == hello ])"
[ $EMPTY == "hello" ] 2>&1 || true
echo "Exit code: $?"
echo ""
echo "[ \"\$EMPTY\" == hello ] → funciona (se expande a [ \"\" == hello ])"
[ "$EMPTY" == "hello" ] 2>&1 || true
echo "Exit code: $?"
'
```

### Paso 3.2: [[ ]] es una keyword de bash

```bash
docker compose exec debian-dev bash -c '
echo "=== [[ ]] — keyword de bash ==="
echo ""
echo "--- No necesita comillas en variables ---"
EMPTY=""
[[ $EMPTY == "hello" ]] && echo "si" || echo "no (sin error)"
echo "No falla aunque EMPTY este vacio — [[ ]] maneja esto internamente"
echo ""
echo "--- Soporta && y || dentro ---"
X=5
if [[ $X -gt 0 && $X -lt 10 ]]; then
    echo "$X esta entre 1 y 9"
fi
echo ""
echo "Con [ ] necesitarias:"
echo "  [ \$X -gt 0 ] && [ \$X -lt 10 ]"
echo "  o: [ \$X -gt 0 -a \$X -lt 10 ] (obsoleto)"
echo ""
echo "--- Pattern matching con == ---"
FILE="report.tar.gz"
if [[ "$FILE" == *.gz ]]; then
    echo "$FILE es un archivo gzip"
fi
echo ""
echo "--- Regex con =~ ---"
EMAIL="user@example.com"
if [[ "$EMAIL" =~ ^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$ ]]; then
    echo "$EMAIL es un email valido"
    echo "Dominio: ${BASH_REMATCH[0]#*@}"
fi
echo ""
echo "[[ ]] es especifico de bash — no funciona en sh/dash"
'
```

### Paso 3.3: Comparacion completa

```bash
docker compose exec debian-dev bash -c '
echo "=== [ ] vs [[ ]] ==="
echo ""
printf "%-28s %-16s %-16s\n" "Caracteristica" "[ ] / test" "[[ ]]"
printf "%-28s %-16s %-16s\n" "---------------------------" "---------------" "---------------"
printf "%-28s %-16s %-16s\n" "Tipo" "Comando" "Keyword bash"
printf "%-28s %-16s %-16s\n" "POSIX" "Si" "No (solo bash)"
printf "%-28s %-16s %-16s\n" "Comillas necesarias" "Si (siempre)" "No (recomendado)"
printf "%-28s %-16s %-16s\n" "&& y ||" "Externo" "Interno"
printf "%-28s %-16s %-16s\n" "Pattern matching" "No" "Si (==, !=)"
printf "%-28s %-16s %-16s\n" "Regex" "No" "Si (=~)"
printf "%-28s %-16s %-16s\n" "Word splitting" "Si (peligro)" "No"
printf "%-28s %-16s %-16s\n" "Globbing en vars" "Si (peligro)" "No"
echo ""
echo "--- Recomendacion ---"
echo "  Scripts bash: usar [[ ]] siempre"
echo "  Scripts POSIX (#!/bin/sh): usar [ ] con comillas"
echo ""
echo "--- (( )) para aritmetica ---"
X=10
if (( X > 5 && X < 20 )); then
    echo "$X esta entre 6 y 19"
fi
echo "(( )) es mejor que [[ \$X -gt 5 ]] para comparaciones numericas"
'
```

### Paso 3.4: Errores comunes

```bash
docker compose exec debian-dev bash -c '
echo "=== Errores comunes de quoting ==="
echo ""
echo "1. Asignaciones con espacio:"
echo "   VAR = value   → error (bash piensa que VAR es un comando)"
echo "   VAR=value     → correcto"
VAR=value
echo "   Correcto: VAR=$VAR"
echo ""
echo "2. Comparacion con espacio:"
echo "   [ \$X==\"hello\" ] → error (un solo argumento para [)"
echo "   [ \"\$X\" == \"hello\" ] → correcto"
echo ""
echo "3. String vacio en [ ]:"
echo "   [ -n \$EMPTY ]  → true! (sin comillas, se vuelve [ -n ])"
echo "   [ -n \"\$EMPTY\" ] → false (correcto)"
EMPTY=""
[ -n $EMPTY ] && echo "   Sin comillas: true (BUG)" || true
[ -n "$EMPTY" ] && echo "   true" || echo "   Con comillas: false (correcto)"
echo ""
echo "4. rm sin comillas:"
echo "   FILE=\"-rf /\"  → rm \$FILE se vuelve rm -rf /"
echo "   rm \"\$FILE\"    → intenta borrar el archivo literalmente"
echo ""
echo "REGLA: en caso de duda, SIEMPRE poner comillas dobles"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Las **comillas dobles** expanden variables y comandos pero
   previenen word splitting y globbing. Son la opcion por defecto
   para casi todo uso de variables.

2. Las **comillas simples** son completamente literales — nada
   se expande. Ideales para patrones de grep/sed/awk.

3. `$'...'` (ANSI-C quoting) soporta `\n`, `\t`, `\'` y otros
   escapes. Util para strings con caracteres especiales.

4. Sin comillas, bash aplica **word splitting** (separa por
   IFS) y **globbing** (expande `*`, `?`, `[...]`). Estos son
   la causa de la mayoria de bugs en scripts.

5. `[[ ]]` es una keyword de bash que no necesita comillas,
   soporta `&&`/`||` internos, pattern matching con `==`, y
   regex con `=~`. `[ ]` es un comando POSIX que requiere
   comillas siempre.

6. Siempre usar comillas dobles en variables excepto en globs
   intencionales (`for f in *.txt`) y dentro de `[[ ]]` en
   el lado derecho de `==` para pattern matching.
