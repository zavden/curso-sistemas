# T04 — Quoting

## Por qué el quoting importa

El shell procesa cada línea de comando antes de ejecutarla. Ese procesamiento
incluye **word splitting** (separar por espacios) y **globbing** (expandir
patrones como `*`). El quoting controla qué partes de la línea se procesan y
cuáles se toman literalmente.

La mayoría de bugs sutiles en scripts bash son problemas de quoting.

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

### Qué se expande dentro de comillas dobles

| Expansión | Ejemplo | ¿Se expande? |
|---|---|---|
| Variables `$VAR` | `"$HOME"` | Sí |
| Parámetros `${VAR}` | `"${HOME}"` | Sí |
| Comando `$(cmd)` | `"$(date)"` | Sí |
| Aritmética `$((expr))` | `"$((2+2))"` | Sí |
| Backtick `` `cmd` `` | `` "`date`" `` | Sí |
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

# Las excepciones (sin comillas a propósito):
# 1. Asignaciones (no hacen splitting)
VAR=$OTHER_VAR           # OK sin comillas
VAR=$(command)           # OK sin comillas

# 2. Dentro de [[ ]] (no hace splitting)
[[ $VAR == "value" ]]    # OK sin comillas (en [[ ]], no en [ ])

# 3. Cuando QUIERES splitting (raro, documentar por qué)
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

### Include una comilla simple en comillas simples

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

### Por qué $() es mejor

```bash
# 1. Se puede anidar sin problemas
DIR=$(dirname $(readlink -f "$0"))

# Con backticks, anidar requiere escapes grotesco
DIR=`dirname \`readlink -f "$0"\``

# 2. Las comillas dentro son independientes
RESULT=$(echo "hello world" | grep "hello")

# Con backticks, las comillas internas son confusas
RESULT=`echo "hello world" | grep "hello"`

# 3. Es visualmente más claro
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
# ¡El * se expandió a los archivos del directorio!

echo "$MESSAGE"
# Archivo no encontrado: *
# Correcto: literal
```

```bash
# En variables de configuración
PATTERN="*.log"

# Sin comillas: glob se expande a archivos existentes
echo $PATTERN
# access.log error.log (archivos que existen)

# Con comillas: se mantiene como patrón
echo "$PATTERN"
# *.log

# Para usar el glob intencionalmente con find/ls:
find /var/log -name "$PATTERN"     # find maneja el glob internamente
ls /var/log/$PATTERN               # aquí SÍ queremos que el shell expanda
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

### Comparación [ vs [[

| Aspecto | `[ ]` | `[[ ]]` |
|---|---|---|
| Word splitting | Sí | No |
| Glob expansion | Sí | No |
| Requiere comillas en variables | Sí | No |
| Pattern matching (`*`, `?`) | No | Sí |
| Regex (`=~`) | No | Sí |
| Compatibilidad POSIX | Sí | No |

---

## Resumen de quoting

| Tipo | Sintaxis | Variables | Comandos | Glob | Splitting | Escape |
|---|---|---|---|---|---|---|
| Sin comillas | `$VAR` | Sí | Sí | Sí | Sí | Sí |
| Dobles | `"$VAR"` | Sí | Sí | No | No | Parcial |
| Simples | `'$VAR'` | No | No | No | No | No |
| ANSI-C | `$'...'` | No | No | No | No | Sí (\n, \t) |

---

## Ejercicios

### Ejercicio 1 — Demostrar word splitting

```bash
# Crear archivos de prueba
mkdir /tmp/quoting-test && cd /tmp/quoting-test
touch "archivo uno.txt" "archivo dos.txt"

VAR="archivo uno.txt"

# Sin comillas: ls recibe 3 argumentos
ls -la $VAR 2>&1

# Con comillas: ls recibe 1 argumento
ls -la "$VAR" 2>&1

cd - && rm -rf /tmp/quoting-test
```

---

### Ejercicio 2 — Globbing accidental

```bash
mkdir /tmp/glob-test && cd /tmp/glob-test
touch a.log b.log c.txt

MSG="Archivos: *"
echo $MSG       # Se expande a los archivos
echo "$MSG"     # Literal asterisco

cd - && rm -rf /tmp/glob-test
```

---

### Ejercicio 3 — Comparar tipos de comillas

```bash
NAME="World"
echo "Hello $NAME"           # Hello World
echo 'Hello $NAME'           # Hello $NAME (literal)
echo $'Hello\t$NAME'         # Hello    World (tab expansion)
echo "Command: $(whoami)"    # Command: username
echo 'Command: $(whoami)'    # Command: $(whoami) (literal)
```

---

### Ejercicio 4 — ANSI-C quoting

```bash
# Newlines
echo $'Línea 1\nLínea 2'

# Tabs
echo $'Col1\tCol2\tCol3'

# Comillas simples dentro
echo $'It\'s working'

# Hex bytes
echo $'\x48\x65\x6c\x6c\x6f'  # Hello
```

---

### Ejercicio 5 — Globbing vs pattern matching

```bash
FILE="test.txt"

# Sin comillas: glob se expande
ls -la $FILE 2>/dev/null || echo "No hay archivos test.txt"

# Con glob pattern
[[ "test.txt" == *.txt ]] && echo "Es un texto"
[[ "test.txt" == test.* ]] && echo "Empieza con test"

# En [[ ]], el glob no se expande a archivos
PATTERN="*.txt"
[[ "test.txt" == $PATTERN ]] && echo "Match con variable"
```

---

### Ejercicio 6 — Evitar bugs comunes

```bash
# Bug: archivos con espacios
mkdir /tmp/space-test
touch "/tmp/space-test/archivo con espacios.txt"

# INCORRECTO
# find /tmp/space-test -name "*.txt" | while read f; do
#     echo "$f"  # Funciona en este caso
# done

# CORRECTO: usar delimitador nulo
find /tmp/space-test -name "*.txt" -print0 | while IFS= read -r -d '' f; do
    echo "Encontrado: $f"
done

# También correcto:IFS modificado
find /tmp/space-test -name "*.txt" | while IFS= read -r f; do
    echo "Encontrado: $f"
done

rm -rf /tmp/space-test
```

---

### Ejercicio 7 — Quoting en asignaciones

```bash
# Asignación sin comillas (generalmente OK)
A=hello
B="hello world"
C=$(date)

# Pero hay casos donde las comillas importan:
PATH_SIN_COMILLAS=/home/user/my app
# Error: el espacio causa separación

PATH_CON_COMILLAS="/home/user/my app"
# OK

# Arrays sí necesitan paréntesis
ARRAY=(one two three)
```

---

### Ejercicio 8 — eval y quoting

```bash
# eval trata el string como si fuera entrada de shell
VAR="hello world"
eval echo $VAR              # Expande dos veces
eval echo "$VAR"           # Una vez

# Peligro de eval
USER_INPUT='"; rm -rf /; echo "'
# safe_var="$USER_INPUT"   # Asignación normal
# eval echo "$safe_var"    # ¡PELIGRO! Executa el rm
```

---

### Ejercicio 9 — here strings con quoting

```bash
# Here string: <<< palabra
wc -w <<< "hello world"
# 2

# With quoting
wc -w <<< 'hello world'
# 2

# Difference
VAR="hello    world"
wc -w <<< $VAR      # 2 (splits)
wc -w <<< "$VAR"    # 1 (single string)
```

---

### Ejercicio 10 — Case statement y quoting

```bash
case "$1" in
    "start")
        echo "Iniciando..."
        ;;
    "stop")
        echo "Deteniendo..."
        ;;
    "")
        echo "Uso: $0 {start|stop}"
        exit 1
        ;;
    *)
        echo "Comando desconocido: $1"
        exit 1
        ;;
esac
```

---

## Errores comunes a evitar

```bash
# ERROR: sin comillas en if
if [ $VAR == "value" ]; then  # Falla si VAR tiene espacios

# CORRECTO:
if [[ $VAR == "value" ]]; then  # [[ no hace splitting

# ERROR: glob en variable
FILES=$PATTERN  # Se expande inmediatamente

# CORRECTO:
FILES="$PATTERN"  # Se mantiene como literal

# ERROR: find con output splitting
for f in $(find . -name "*.txt"); do  # Falla con espacios

# CORRECTO:
while IFS= read -r -d '' f; do
    echo "$f"
done < <(find . -name "*.txt" -print0)
```
