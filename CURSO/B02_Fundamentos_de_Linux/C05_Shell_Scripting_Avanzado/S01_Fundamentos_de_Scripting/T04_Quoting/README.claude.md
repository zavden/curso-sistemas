# T04 — Quoting

## Errata respecto a los materiales originales

| Ubicacion | Error | Correccion |
|---|---|---|
| max.md:346 | `echo $'Hello\t$NAME'` comenta `# Hello    World` | `$'...'` NO expande variables. Salida real: `Hello\t$NAME` (tab + literal $NAME) |
| max.md:467-468 | `wc -w <<< "$VAR"` comenta `# 1 (single string)` | `wc -w` cuenta palabras en el input, no argumentos shell. Ambas formas dan 2 |
| max.md:96 | "Include una comilla simple" | Correcto: "Incluir una comilla simple" (ingles mezclado) |

---

## Por que el quoting importa

El shell procesa cada linea de comando antes de ejecutarla. Ese procesamiento
incluye **word splitting** (separar por espacios) y **globbing** (expandir
patrones como `*`). El quoting controla que partes de la linea se procesan y
cuales se toman literalmente.

La mayoria de bugs sutiles en scripts bash son problemas de quoting.

```bash
# El problema fundamental:
FILE="mi archivo.txt"

# Sin comillas: word splitting parte la variable en dos palabras
rm $FILE
# bash ejecuta: rm "mi" "archivo.txt"
# Elimina DOS archivos diferentes (o falla si no existen)

# Con comillas: la variable se mantiene como una sola palabra
rm "$FILE"
# bash ejecuta: rm "mi archivo.txt"
# Correcto: elimina UN archivo
```

---

## Comillas dobles (")

Las comillas dobles permiten expansiones pero **previenen word splitting y
globbing**:

```bash
NAME="World"
FILES="*.txt"

echo "Hello $NAME"          # Hello World  (expande $NAME)
echo "Files: $FILES"        # Files: *.txt (NO expande el glob)
echo "Today: $(date)"       # Today: Mon Mar 17...  (expande $())
echo "Home: ${HOME}"        # Home: /home/dev  (expande ${})
echo "2 + 2 = $((2+2))"    # 2 + 2 = 4  (expande $(()))
```

### Que se expande dentro de comillas dobles

| Expansion | Ejemplo | Se expande? |
|---|---|---|
| Variables `$VAR` | `"$HOME"` | Si |
| Parametros `${VAR}` | `"${HOME}"` | Si |
| Comando `$(cmd)` | `"$(date)"` | Si |
| Aritmetica `$((expr))` | `"$((2+2))"` | Si |
| Backtick `` `cmd` `` | `` "`date`" `` | Si |
| Escape `\` | `"\n"` | Parcial (solo `\\`, `\"`, `\$`, `` \` ``, `\newline`) |
| Glob `*`, `?`, `[]` | `"*.txt"` | No (literal) |
| Word splitting | `"$VAR"` | No (se preserva como una palabra) |

### La regla de oro

**Siempre poner variables entre comillas dobles**, excepto en los pocos casos
donde el splitting es intencionado:

```bash
# SIEMPRE:
echo "$VAR"
cp "$SOURCE" "$DEST"
if [[ -f "$FILE" ]]; then
    "$COMMAND" "$ARG1" "$ARG2"

# Las excepciones (sin comillas a proposito):
# 1. Asignaciones (no hacen splitting)
VAR=$OTHER_VAR           # OK sin comillas
VAR=$(command)           # OK sin comillas

# 2. Dentro de [[ ]] (no hace splitting)
[[ $VAR == "value" ]]    # OK sin comillas (en [[ ]], no en [ ])

# 3. Cuando QUIERES splitting (raro, documentar por que)
for word in $SENTENCE; do   # intencionalmente separar por espacios
```

---

## Comillas simples (')

Las comillas simples son **completamente literales**. Nada se expande dentro
de ellas:

```bash
echo '$HOME'           # $HOME (literal, no se expande)
echo '$(date)'         # $(date) (literal)
echo 'Hello\nWorld'    # Hello\nWorld (literal, \n no es newline)
echo '*.txt'           # *.txt (literal)
```

### Incluir una comilla simple en comillas simples

```bash
# No se puede incluir una comilla simple dentro de comillas simples
echo 'It's a test'     # Error de sintaxis

# Soluciones:
echo "It's a test"           # usar comillas dobles
echo 'It'\''s a test'        # cerrar, escapar, abrir
echo $'It\'s a test'         # $'...' (ANSI-C quoting)
```

---

## $'...' — ANSI-C Quoting

```bash
# $'...' permite secuencias de escape como en C:
echo $'Hello\nWorld'
# Hello
# World

echo $'Tab\there'
# Tab    here

echo $'It\'s a quote'
# It's a quote

# IMPORTANTE: $'...' NO expande variables
NAME="World"
echo $'Hello $NAME'     # Hello $NAME (literal)
echo $'Hello\t$NAME'    # Hello	$NAME (tab + literal $NAME)

# Secuencias disponibles:
# \n   newline
# \t   tab
# \r   carriage return
# \\   backslash literal
# \'   comilla simple
# \xHH byte hexadecimal
# \uHHHH  Unicode (bash 4.4+)
```

---

## $() vs backticks

Ambos capturan la salida de un comando, pero `$()` es superior:

```bash
# Backticks (legacy)
DATE=`date`

# $() (moderno — SIEMPRE preferir)
DATE=$(date)
```

### Por que $() es mejor

```bash
# 1. Se puede anidar sin problemas
DIR=$(dirname "$(readlink -f "$0")")

# Con backticks, anidar requiere escaping grotesco
DIR=`dirname \`readlink -f "$0"\``

# 2. Las comillas dentro son independientes
RESULT=$(echo "hello world" | grep "hello")

# Con backticks, las comillas internas son confusas
RESULT=`echo "hello world" | grep "hello"`

# 3. Es visualmente mas claro
FILES=$(find /tmp -name "*.log" -mtime +7)
```

---

## Word splitting

Cuando bash encuentra una variable **sin comillas**, la separa en palabras
usando los caracteres de `IFS` (default: espacio, tab, newline):

```bash
NAMES="alice bob charlie"

# Sin comillas: 3 palabras separadas
for name in $NAMES; do
    echo "Nombre: $name"
done
# Nombre: alice
# Nombre: bob
# Nombre: charlie

# Con comillas: 1 sola palabra
for name in "$NAMES"; do
    echo "Nombre: $name"
done
# Nombre: alice bob charlie
```

### Donde el word splitting causa bugs

```bash
FILE="mi archivo importante.txt"

# Bug 1: rm con splitting
rm $FILE
# bash ejecuta: rm mi archivo importante.txt
# Intenta eliminar 3 archivos: "mi", "archivo", "importante.txt"

# Bug 2: test/[ ] con splitting
if [ -f $FILE ]; then    # bash ve: [ -f mi archivo importante.txt ]
    echo "existe"        # Error: too many arguments
fi

# Bug 3: loops con archivos
for f in $(find . -name "*.txt"); do
    # Si un archivo se llama "mi documento.txt"
    # el loop itera "mi" y "documento.txt" por separado
    echo "$f"
done

# Correcto:
if [[ -f "$FILE" ]]; then echo "existe"; fi

# Correcto para find:
find . -name "*.txt" -print0 | while IFS= read -r -d '' f; do
    echo "$f"
done
```

---

## Globbing accidental

Sin comillas, bash expande patrones `*`, `?`, `[...]`:

```bash
# Ejemplo peligroso
MESSAGE="Archivo no encontrado: *"

echo $MESSAGE
# Si hay archivos en el directorio actual:
# Archivo no encontrado: file1.txt file2.txt script.sh
# El * se expandio a los archivos del directorio!

echo "$MESSAGE"
# Archivo no encontrado: *
# Correcto: literal
```

```bash
# En variables de configuracion
PATTERN="*.log"

# Sin comillas: glob se expande a archivos existentes
echo $PATTERN
# access.log error.log (archivos que existen)

# Con comillas: se mantiene como patron
echo "$PATTERN"
# *.log

# Para usar el glob intencionalmente con find/ls:
find /var/log -name "$PATTERN"     # find maneja el glob internamente
ls /var/log/$PATTERN               # aqui SI queremos que el shell expanda
```

---

## [ ] vs [[ ]]

```bash
# [ ] (test): comando externo, sujeto a word splitting y globbing
VAR="hello world"
[ $VAR == "hello world" ]      # Error: too many arguments
[ "$VAR" == "hello world" ]    # OK con comillas

# [[ ]] (bash keyword): no hace word splitting ni globbing
[[ $VAR == "hello world" ]]    # OK sin comillas
[[ $VAR == hello* ]]           # pattern matching (no glob)
[[ $VAR =~ ^hello ]]           # regex

# SIEMPRE preferir [[ ]] en scripts bash
# [ ] solo si necesitas compatibilidad POSIX (#!/bin/sh)
```

### Comparacion [ vs [[

| Aspecto | `[ ]` | `[[ ]]` |
|---|---|---|
| Tipo | Comando (builtin/externo) | Keyword de bash |
| Word splitting | Si (peligro) | No |
| Glob expansion | Si (peligro) | No |
| Requiere comillas en variables | Si (siempre) | No (recomendado) |
| Pattern matching (`*`, `?`) | No | Si |
| Regex (`=~`) | No | Si |
| `&&` y `\|\|` internos | No (usar `-a`, `-o`) | Si |
| Compatibilidad POSIX | Si | No (solo bash) |

---

## Resumen de quoting

| Tipo | Sintaxis | Variables | Comandos | Glob | Splitting | Escape |
|---|---|---|---|---|---|---|
| Sin comillas | `$VAR` | Si | Si | Si | Si | Si |
| Dobles | `"$VAR"` | Si | Si | No | No | Parcial |
| Simples | `'$VAR'` | No | No | No | No | No |
| ANSI-C | `$'...'` | No | No | No | No | Si (\n, \t) |

---

## Labs

### Parte 1 — Comillas dobles y simples

#### Paso 1.1: Comillas dobles — expansion controlada

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

#### Paso 1.2: Comillas simples — literal absoluto

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

#### Paso 1.3: $'...' — ANSI-C quoting

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
NAME="World"
echo "--- \$'\''...'\'' NO expande variables ---"
echo $'\''Hello $NAME'\''
echo "(Imprime literal \$NAME, no World)"
echo ""
echo "=== Secuencias soportadas ==="
echo "\n  = newline"
echo "\t  = tab"
echo "\\\\  = backslash literal"
echo "\\'\''  = comilla simple literal"
echo "\xHH = hex (\x41 = A)"
'
```

#### Paso 1.4: Mezclar tipos de comillas

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

### Parte 2 — Word splitting y globbing

#### Paso 2.1: Word splitting

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
echo "Sin comillas: ls intenta dos archivos separados"
echo "  ls $FILE → ls mi archivo.txt (2 argumentos)"
ls $FILE 2>&1 || true
echo ""
echo "Con comillas: ls recibe un solo argumento"
touch "/tmp/$FILE"
echo "  ls \"$FILE\" → ls \"mi archivo.txt\" (1 argumento)"
ls "/tmp/$FILE"
rm -f "/tmp/$FILE"
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

#### Paso 2.2: Globbing accidental

```bash
docker compose exec debian-dev bash -c '
echo "=== Globbing accidental ==="
echo ""
echo "Bash expande * ? [...] en nombres de archivos si no estan entre comillas"
echo ""
echo "--- Demostrar el peligro ---"
cd /tmp
touch glob1.txt glob2.txt glob3.txt
MSG="Los archivos: *.txt"
echo Sin comillas: $MSG
echo "Con comillas: $MSG"
echo ""
echo "--- Caso peligroso real ---"
PATTERN="*.txt"
echo "Sin comillas:"
for f in $PATTERN; do echo "  $f"; done
echo ""
echo "Con comillas (literal):"
for f in "$PATTERN"; do echo "  $f"; done
rm -f glob1.txt glob2.txt glob3.txt
echo ""
echo "--- set -f: deshabilitar globbing ---"
echo "set -f desactiva la expansion de comodines"
echo "set +f la reactiva"
'
```

#### Paso 2.3: Donde las comillas son criticas

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

### Parte 3 — [[ ]] vs [ ]

#### Paso 3.1: [ ] es un comando

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
echo ""
echo "[ \"\$EMPTY\" == hello ] → funciona (se expande a [ \"\" == hello ])"
[ "$EMPTY" == "hello" ] 2>&1 || true
echo "Exit code: $?"
'
```

#### Paso 3.2: [[ ]] es una keyword de bash

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
fi
echo ""
echo "[[ ]] es especifico de bash — no funciona en sh/dash"
'
```

#### Paso 3.3: Comparacion completa

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

#### Paso 3.4: Errores comunes

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
echo "4. rm sin comillas con contenido peligroso:"
echo "   FILE=\"-rf /\"  → rm \$FILE se vuelve rm -rf /"
echo "   rm \"\$FILE\"    → intenta borrar el archivo literalmente"
echo "   Mejor aun: rm -- \"\$FILE\" (-- termina las opciones)"
echo ""
echo "REGLA: en caso de duda, SIEMPRE poner comillas dobles"
'
```

---

## Ejercicios

### Ejercicio 1 — Word splitting en accion

Predice que pasa en cada caso:

```bash
mkdir /tmp/quoting-test && cd /tmp/quoting-test
touch "archivo uno.txt" "archivo dos.txt"

VAR="archivo uno.txt"

# Caso A
ls -la $VAR 2>&1

# Caso B
ls -la "$VAR" 2>&1

cd - && rm -rf /tmp/quoting-test
```

<details><summary>Prediccion</summary>

Caso A: `ls` recibe 2 argumentos: `archivo` y `uno.txt`. Ninguno de los dos
existe como archivo separado → error `No such file or directory` para ambos.

Caso B: `ls` recibe 1 argumento: `archivo uno.txt`. El archivo existe
(fue creado con `touch`) → muestra sus detalles.

El word splitting ocurre porque `$VAR` sin comillas se separa por espacios.
Con `"$VAR"`, bash trata todo el valor como una sola palabra.

</details>

### Ejercicio 2 — Globbing accidental

Predice la salida de cada echo:

```bash
mkdir /tmp/glob-test && cd /tmp/glob-test
touch a.log b.log c.txt

MSG="Archivos: *"
echo $MSG
echo "$MSG"

cd - && rm -rf /tmp/glob-test
```

<details><summary>Prediccion</summary>

- `echo $MSG` → `Archivos: a.log b.log c.txt` — sin comillas, bash expande
  el `*` a todos los archivos del directorio actual. Ademas, los multiples
  espacios entre "Archivos:" y los nombres se colapsan.

- `echo "$MSG"` → `Archivos: *` — con comillas, el `*` permanece literal.
  No se hace globbing ni word splitting.

</details>

### Ejercicio 3 — Comparar tipos de comillas

Predice que imprime cada linea:

```bash
NAME="World"
echo "Hello $NAME"
echo 'Hello $NAME'
echo $'Hello\t$NAME'
echo "Command: $(whoami)"
echo 'Command: $(whoami)'
```

<details><summary>Prediccion</summary>

1. `"Hello $NAME"` → `Hello World` — comillas dobles expanden `$NAME`
2. `'Hello $NAME'` → `Hello $NAME` — comillas simples, todo literal
3. `$'Hello\t$NAME'` → `Hello	$NAME` — ANSI-C quoting: `\t` se convierte en tab, pero `$NAME` permanece literal (las variables NO se expanden en `$'...'`)
4. `"Command: $(whoami)"` → `Command: dev` (o tu usuario) — `$()` se expande dentro de comillas dobles
5. `'Command: $(whoami)'` → `Command: $(whoami)` — literal

Punto clave: `$'...'` interpreta secuencias de escape (`\n`, `\t`, `\'`) pero
NO expande variables ni comandos. Es distinto de las comillas dobles.

</details>

### Ejercicio 4 — ANSI-C quoting

Predice la salida:

```bash
echo $'Linea 1\nLinea 2'
echo $'Col1\tCol2\tCol3'
echo $'It\'s working'
echo $'\x48\x65\x6c\x6c\x6f'
```

<details><summary>Prediccion</summary>

```
Linea 1
Linea 2
```
(dos lineas — `\n` se convierte en newline)

```
Col1	Col2	Col3
```
(separado por tabs — `\t` se convierte en tab)

```
It's working
```
(`\'` se convierte en comilla simple literal)

```
Hello
```
(`\x48`=H, `\x65`=e, `\x6c`=l, `\x6c`=l, `\x6f`=o — codigos hexadecimales ASCII)

</details>

### Ejercicio 5 — [ ] vs [[ ]] con variables

Predice que pasa en cada caso:

```bash
VAR="hello world"
EMPTY=""

# Caso A
[ $VAR == "hello world" ] 2>&1; echo "A: $?"

# Caso B
[[ $VAR == "hello world" ]]; echo "B: $?"

# Caso C
[ -n $EMPTY ]; echo "C: $?"

# Caso D
[ -n "$EMPTY" ]; echo "D: $?"

# Caso E
[[ -n $EMPTY ]]; echo "E: $?"
```

<details><summary>Prediccion</summary>

- A: Error `too many arguments`, exit 2 — `$VAR` sin comillas en `[ ]` se
  convierte en `[ hello world == "hello world" ]` (4 argumentos donde se
  esperaban 3).

- B: Exit 0 (true) — `[[ ]]` no hace word splitting, `$VAR` se mantiene
  como una sola palabra. La comparacion es correcta.

- C: Exit 0 (true, BUG!) — `$EMPTY` sin comillas desaparece, `[ -n ]`
  se evalua como `[ -n ]` que bash interpreta como "el string `-n` no esta
  vacio" → true. Un falso positivo peligroso.

- D: Exit 1 (false, correcto) — `"$EMPTY"` se expande a `""`,
  `[ -n "" ]` → string vacio → false.

- E: Exit 1 (false, correcto) — `[[ ]]` maneja variables vacias
  correctamente sin necesitar comillas.

</details>

### Ejercicio 6 — find seguro con espacios

Predice que pasa con cada metodo:

```bash
mkdir -p /tmp/space-test
touch "/tmp/space-test/archivo uno.txt" "/tmp/space-test/archivo dos.txt"

# Metodo A: for + $()
for f in $(find /tmp/space-test -name "*.txt"); do
    echo "A: [$f]"
done

# Metodo B: pipe + read
find /tmp/space-test -name "*.txt" | while IFS= read -r f; do
    echo "B: [$f]"
done

# Metodo C: -print0 + read -d ''
find /tmp/space-test -name "*.txt" -print0 | while IFS= read -r -d '' f; do
    echo "C: [$f]"
done

rm -rf /tmp/space-test
```

<details><summary>Prediccion</summary>

Metodo A (roto):
```
A: [/tmp/space-test/archivo]
A: [uno.txt]
A: [/tmp/space-test/archivo]
A: [dos.txt]
```
El `$()` captura la salida de find, luego word splitting parte cada nombre
en los espacios. Recibe 4 "archivos" en vez de 2.

Metodo B (funciona):
```
B: [/tmp/space-test/archivo uno.txt]
B: [/tmp/space-test/archivo dos.txt]
```
`read -r` lee linea por linea. `IFS=` evita que trim whitespace al inicio/final.
Funciona con espacios pero falla si un nombre tiene newlines.

Metodo C (mas robusto):
```
C: [/tmp/space-test/archivo uno.txt]
C: [/tmp/space-test/archivo dos.txt]
```
`-print0` usa null (`\0`) como delimitador. `read -d ''` lee hasta null.
Funciona con espacios, tabs, y newlines en nombres de archivos.

</details>

### Ejercicio 7 — Quoting en asignaciones

Predice que pasa:

```bash
# Caso A
A=hello
echo "A: $A"

# Caso B
B="hello world"
echo "B: $B"

# Caso C
C=hello world
echo "C: $C"
```

<details><summary>Prediccion</summary>

- A: `A: hello` — asignacion simple, funciona sin comillas.

- B: `B: hello world` — las comillas encierran el valor completo incluyendo
  el espacio.

- C: Bash interpreta `C=hello` como una variable temporal y `world` como
  un comando a ejecutar (con `C=hello` en su entorno). Si `world` no existe
  como comando, da error `world: command not found`. `C` no queda asignada
  en el shell actual.

  Esto no es word splitting — es como bash parsea las lineas de comando.
  `VAR=valor comando` es sintaxis para "ejecutar comando con esta variable
  de entorno temporal".

</details>

### Ejercicio 8 — Pattern matching en [[ ]]

Predice cada resultado:

```bash
FILE="report.2024-03-17.tar.gz"

[[ "$FILE" == *.gz ]]     && echo "A: es gzip"
[[ "$FILE" == *.tar.* ]]  && echo "B: es tar"
[[ "$FILE" == report* ]]  && echo "C: empieza con report"
[[ "$FILE" == *2024* ]]   && echo "D: contiene 2024"
[[ "$FILE" =~ \.tar\. ]]  && echo "E: regex match .tar."
[[ "$FILE" =~ ^[a-z]+\. ]] && echo "F: empieza con letras"
```

<details><summary>Prediccion</summary>

- A: `es gzip` — `*.gz` coincide (termina en .gz)
- B: `es tar` — `*.tar.*` coincide (tiene .tar. en medio)
- C: `empieza con report` — `report*` coincide (empieza con report)
- D: `contiene 2024` — `*2024*` coincide (2024 esta en el nombre)
- E: `regex match .tar.` — `\.tar\.` coincide con la subcadena literal `.tar.`
- F: `empieza con letras` — `^[a-z]+\.` coincide con `report.`

En `[[ ]]`: el lado derecho de `==` es un glob pattern, el de `=~` es
una regex extendida. Sin comillas en el pattern para que funcione como glob.

</details>

### Ejercicio 9 — Here strings y quoting

Predice la salida:

```bash
VAR="hello    world"

# Caso A
cat <<< $VAR

# Caso B
cat <<< "$VAR"

# Caso C
wc -w <<< $VAR

# Caso D
wc -w <<< "$VAR"
```

<details><summary>Prediccion</summary>

Caso A: `hello world` — sin comillas, word splitting colapsa los espacios
multiples a uno solo antes de pasarlo como here string.

Caso B: `hello    world` — con comillas, los 4 espacios se preservan.

Caso C: `2` — aunque los espacios se colapsan, el input sigue teniendo
2 palabras: "hello" y "world". `wc -w` cuenta palabras en el texto, no
argumentos shell.

Caso D: `2` — los espacios se preservan pero siguen siendo 2 palabras.
`wc -w` separa por whitespace al contar, asi que 4 espacios o 1 espacio
da igual: hay 2 palabras.

La diferencia entre comillas y sin comillas en here strings afecta los
**espacios** (se colapsan vs se preservan), pero no el **conteo de
palabras** de `wc`.

</details>

### Ejercicio 10 — Panorama de quoting

Analiza este script y predice que pasa:

```bash
#!/usr/bin/env bash
set -euo pipefail

# Variables de ejemplo
DIR="/tmp/mi directorio"
FILES="*.txt"
NAME='$USER'
MSG=$'Linea1\nLinea2'

# Pregunta 1: cuantos directorios crea mkdir?
mkdir -p "$DIR"

# Pregunta 2: que contiene NAME?
echo "NAME es: $NAME"

# Pregunta 3: cuantas lineas imprime echo?
echo "$MSG"

# Pregunta 4: que pasa con este glob?
touch "$DIR/a.txt" "$DIR/b.txt"
echo "Sin comillas: " $FILES
echo "Con comillas: $FILES"
echo "En directorio: " "$DIR"/$FILES

# Pregunta 5: funciona [[ ]] sin comillas?
[[ $DIR == */mi* ]] && echo "Contiene 'mi'"

rm -rf "$DIR"
```

<details><summary>Prediccion</summary>

1. `mkdir -p "$DIR"` crea **1** directorio: `/tmp/mi directorio` (con espacio).
   Sin comillas crearia dos: `/tmp/mi` y `directorio`.

2. `NAME es: $USER` — NAME contiene el literal `$USER` porque fue asignada
   con comillas simples (`NAME='$USER'`). Pero dentro de `echo "$NAME"`, las
   comillas dobles expanden `$NAME` al literal `$USER`, que luego NO se
   re-expande. Bash solo hace una ronda de expansion.

3. Imprime **2 lineas**: `Linea1` y `Linea2`. `$'...'` convirtio `\n` en
   un newline real al momento de la asignacion. `echo "$MSG"` preserva
   ese newline.

4. - `echo "Sin comillas: " $FILES` → `Sin comillas:  a.txt b.txt ...` — el
     glob `$FILES` se expande a archivos del directorio **actual** (no de
     `$DIR`), o se mantiene literal si no hay .txt en el directorio actual.
   - `echo "Con comillas: $FILES"` → `Con comillas: *.txt` — el glob no
     se expande dentro de comillas dobles.
   - `echo "En directorio: " "$DIR"/$FILES` → lista los archivos .txt de
     `/tmp/mi directorio/` — el glob se expande porque `$FILES` esta sin
     comillas, pero `"$DIR"/` protege el espacio en la ruta.

5. Si, `[[ $DIR == */mi* ]]` funciona sin comillas. `[[ ]]` no hace word
   splitting, y el `*` en el lado derecho funciona como pattern matching.
   Imprime `Contiene 'mi'`.

</details>
