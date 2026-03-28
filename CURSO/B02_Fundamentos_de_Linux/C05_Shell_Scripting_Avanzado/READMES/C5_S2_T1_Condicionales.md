# T01 — Condicionales

## Errata respecto a los materiales originales

| Ubicacion | Error | Correccion |
|---|---|---|
| labs:450 | "forma idomiatica" | Correcto: "forma idiomatica" |

---

## test, [ ] y [[ ]]

Bash tiene tres formas de evaluar condiciones. Las tres devuelven exit code 0
(verdadero) o 1 (falso):

```bash
# 1. test — comando (builtin de bash + /usr/bin/test)
test -f /etc/passwd
echo $?    # 0

# 2. [ ] — sinonimo de test (es literalmente un comando llamado [)
[ -f /etc/passwd ]
echo $?    # 0
# Los espacios despues de [ y antes de ] son OBLIGATORIOS
# [ es un comando, ] es su ultimo argumento

# 3. [[ ]] — keyword de bash (NO es un comando externo)
[[ -f /etc/passwd ]]
echo $?    # 0
```

```bash
# Verificar que [ es un comando real:
type [
# [ is a shell builtin  (bash tiene un builtin, pero tambien existe /usr/bin/[)

type [[
# [[ is a shell keyword  (parte de la sintaxis de bash)

ls -la /usr/bin/[
# -rwxr-xr-x 1 root root ... /usr/bin/[
```

---

## Diferencias entre [ ] y [[ ]]

### Word splitting y globbing

```bash
FILE="mi archivo.txt"

# [ ] esta sujeto a word splitting:
[ -f $FILE ]
# bash ve: [ -f mi archivo.txt ]
# Error: too many arguments

# [[ ]] NO hace word splitting:
[[ -f $FILE ]]
# Funciona correctamente
```

### Operadores logicos

```bash
# [ ] usa -a (AND) y -o (OR) — POSIX pero fragiles:
[ -f /etc/passwd -a -r /etc/passwd ]      # AND
[ -f /etc/passwd -o -f /etc/shadow ]       # OR

# [[ ]] usa && y || — mas claros:
[[ -f /etc/passwd && -r /etc/passwd ]]     # AND
[[ -f /etc/passwd || -f /etc/shadow ]]     # OR

# Con [ ], la alternativa segura es encadenar comandos:
[ -f /etc/passwd ] && [ -r /etc/passwd ]   # AND con dos [ ]
```

### Comparacion de strings

```bash
VAR="hello"

# [ ] — comparacion literal:
[ "$VAR" = "hello" ]       # igualdad (POSIX usa =, no ==)
[ "$VAR" != "hello" ]      # desigualdad

# [[ ]] — soporta pattern matching y regex:
[[ $VAR == hello ]]        # igualdad (== funciona en [[ ]])
[[ $VAR == h* ]]           # pattern matching (glob)
[[ $VAR =~ ^hel ]]         # regex (ERE)

# El glob en [[ ]] NO se expande a archivos — es pattern matching puro
```

### Resumen de diferencias

| Feature | `[ ]` (test) | `[[ ]]` (bash) |
|---|---|---|
| Tipo | Comando (builtin/externo) | Keyword del lenguaje |
| Word splitting | Si (necesita comillas) | No |
| Globbing | Si (peligroso) | No (pattern matching) |
| Operadores logicos | `-a`, `-o` | `&&`, `\|\|` |
| Pattern matching | No | `==` con globs |
| Regex | No | `=~` |
| Comparacion strings | `=`, `!=` | `=`, `==`, `!=` |
| POSIX compatible | Si | No (solo bash/zsh/ksh) |
| Recomendacion | Solo en `#!/bin/sh` | **Siempre en scripts bash** |

---

## Tests de archivos

```bash
# Existencia y tipo:
[[ -e FILE ]]     # existe (cualquier tipo)
[[ -f FILE ]]     # es un archivo regular
[[ -d FILE ]]     # es un directorio
[[ -L FILE ]]     # es un symlink
[[ -b FILE ]]     # es un dispositivo de bloque
[[ -c FILE ]]     # es un dispositivo de caracter
[[ -p FILE ]]     # es un named pipe (FIFO)
[[ -S FILE ]]     # es un socket

# Permisos:
[[ -r FILE ]]     # tiene permiso de lectura
[[ -w FILE ]]     # tiene permiso de escritura
[[ -x FILE ]]     # tiene permiso de ejecucion
[[ -u FILE ]]     # tiene SUID
[[ -g FILE ]]     # tiene SGID
[[ -k FILE ]]     # tiene sticky bit

# Tamano:
[[ -s FILE ]]     # existe y tamano > 0

# Comparacion:
[[ FILE1 -nt FILE2 ]]   # FILE1 es mas nuevo (newer than)
[[ FILE1 -ot FILE2 ]]   # FILE1 es mas viejo (older than)
[[ FILE1 -ef FILE2 ]]   # son el mismo inode (hardlink)

# Descriptores:
[[ -t FD ]]       # FD esta abierto y conectado a un terminal
```

### Patron tipico de validacion

```bash
CONFIG="/etc/myapp/config.conf"

if [[ ! -f "$CONFIG" ]]; then
    echo "Error: $CONFIG no encontrado" >&2
    exit 1
fi

if [[ ! -r "$CONFIG" ]]; then
    echo "Error: $CONFIG no es legible" >&2
    exit 1
fi
```

### Tests de archivos con symlinks

```bash
# -f, -d, -r, etc. SIGUEN symlinks (evaluan el target):
ln -s /etc/passwd /tmp/link-passwd

[[ -f /tmp/link-passwd ]]   # true (el target es un archivo regular)
[[ -L /tmp/link-passwd ]]   # true (es un symlink)

# Symlink roto (target no existe):
ln -s /no/existe /tmp/broken-link

[[ -e /tmp/broken-link ]]   # false (el target no existe)
[[ -L /tmp/broken-link ]]   # true (el symlink si existe)

# Verificar symlink roto:
[[ -L "$FILE" && ! -e "$FILE" ]]   # es symlink pero target no existe
```

---

## Comparaciones de strings

```bash
# Igualdad/desigualdad:
[[ "$VAR" == "value" ]]    # igual
[[ "$VAR" != "value" ]]    # distinto

# Vacio/no vacio:
[[ -z "$VAR" ]]     # true si VAR esta vacia (zero length)
[[ -n "$VAR" ]]     # true si VAR NO esta vacia (non-zero)

# -z y -n son las formas correctas de verificar si una variable tiene valor:
if [[ -z "$DB_HOST" ]]; then
    echo "Error: DB_HOST no esta definida" >&2
    exit 1
fi
```

### Orden lexicografico

```bash
# < y > comparan orden alfabetico (locale-dependent):
[[ "apple" < "banana" ]]    # true
[[ "banana" > "apple" ]]    # true

# CUIDADO: en [ ] hay que escapar < y >
[ "apple" \< "banana" ]     # necesita \ en [ ]
# Sin escapar, < se interpreta como redireccion
```

### Pattern matching en [[ ]]

```bash
FILE="backup-2024-03-17.tar.gz"

# Patrones glob (*, ?, [...]):
[[ $FILE == *.tar.gz ]]        # true
[[ $FILE == backup-* ]]        # true
[[ $FILE == backup-????-* ]]   # true (? = un caracter)

# NO poner el patron entre comillas — las comillas lo hacen literal:
[[ $FILE == "*.tar.gz" ]]      # false (busca literalmente *.tar.gz)
[[ $FILE == *.tar.gz ]]        # true (pattern matching)

# Uso practico:
case_insensitive() {
    local input="${1,,}"   # convertir a minusculas
    [[ $input == y* ]]     # empieza con 'y'?
}
```

### Regex en [[ ]]

```bash
# =~ usa Extended Regular Expressions (ERE):
EMAIL="user@example.com"
[[ $EMAIL =~ ^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$ ]]
echo $?   # 0 (match)

# BASH_REMATCH contiene los grupos capturados:
DATE="2024-03-17"
if [[ $DATE =~ ^([0-9]{4})-([0-9]{2})-([0-9]{2})$ ]]; then
    echo "Match completo: ${BASH_REMATCH[0]}"   # 2024-03-17
    echo "Ano: ${BASH_REMATCH[1]}"               # 2024
    echo "Mes: ${BASH_REMATCH[2]}"               # 03
    echo "Dia: ${BASH_REMATCH[3]}"               # 17
fi

# Guardar regex en una variable (para evitar problemas con quoting):
PATTERN='^v[0-9]+\.[0-9]+\.[0-9]+$'
[[ "v1.2.3" =~ $PATTERN ]]    # true (semver)
# NO poner la variable regex entre comillas:
[[ "v1.2.3" =~ "$PATTERN" ]]  # busca literal, no regex
```

---

## Comparaciones numericas

```bash
# Operadores numericos (funcionan en [ ] y [[ ]]):
[[ $A -eq $B ]]    # equal
[[ $A -ne $B ]]    # not equal
[[ $A -lt $B ]]    # less than
[[ $A -le $B ]]    # less than or equal
[[ $A -gt $B ]]    # greater than
[[ $A -ge $B ]]    # greater than or equal

# IMPORTANTE: == y != comparan STRINGS, no numeros:
[[ "01" == "1" ]]    # false (strings diferentes)
[[ "01" -eq "1" ]]   # true (numericamente iguales)

[[ " 5" == "5" ]]    # false (el espacio importa)
[[ " 5" -eq "5" ]]   # true (numericamente iguales)
```

### (( )) para aritmetica

```bash
# (( )) es mas natural para comparaciones numericas:
X=10
Y=20

if (( X > Y )); then
    echo "$X es mayor"
elif (( X < Y )); then
    echo "$Y es mayor"
else
    echo "iguales"
fi

# (( )) usa operadores de C: ==, !=, <, >, <=, >=
# Las variables NO necesitan $ dentro de (( ))
(( X == 10 ))     # true
(( X > 5 ))       # true
(( X >= 10 ))     # true

# Combinados:
(( X > 5 && X < 20 ))     # true
(( X == 10 || Y == 10 ))  # true
```

---

## if/elif/else

```bash
# Sintaxis:
if CONDICION; then
    # codigo si verdadero
elif OTRA_CONDICION; then
    # codigo si la segunda es verdadera
else
    # codigo si ninguna es verdadera
fi

# La CONDICION es cualquier comando — if evalua su exit code
# [[ ]] y (( )) son solo los mas comunes
```

```bash
# if con comando directo (sin [ ] ni [[ ]]):
if grep -q "root" /etc/passwd; then
    echo "root existe"
fi

if ping -c 1 -W 2 8.8.8.8 &>/dev/null; then
    echo "hay conectividad"
fi

if command -v docker &>/dev/null; then
    echo "Docker esta instalado"
fi
```

### Patron de validacion de argumentos

```bash
#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Uso: $(basename "$0") <archivo>" >&2
    exit 1
fi

FILE="$1"

if [[ ! -f "$FILE" ]]; then
    echo "Error: '$FILE' no es un archivo" >&2
    exit 1
fi

if [[ ! -r "$FILE" ]]; then
    echo "Error: '$FILE' no es legible" >&2
    exit 1
fi

echo "Procesando $FILE..."
```

---

## case

`case` es mas legible que cadenas de `if/elif` cuando se compara una variable
contra multiples patrones:

```bash
case "$1" in
    start)
        echo "Iniciando..."
        ;;
    stop)
        echo "Deteniendo..."
        ;;
    restart|reload)           # | = OR
        echo "Reiniciando..."
        ;;
    status)
        echo "Estado..."
        ;;
    *)                        # default (catch-all)
        echo "Uso: $0 {start|stop|restart|status}" >&2
        exit 1
        ;;
esac
```

### Patrones en case

```bash
# case usa pattern matching (como globs):
FILE="report.tar.gz"

case "$FILE" in
    *.tar.gz|*.tgz)
        echo "Archivo tar comprimido con gzip"
        ;;
    *.tar.bz2|*.tbz2)
        echo "Archivo tar comprimido con bzip2"
        ;;
    *.tar.xz|*.txz)
        echo "Archivo tar comprimido con xz"
        ;;
    *.zip)
        echo "Archivo zip"
        ;;
    *)
        echo "Formato desconocido"
        ;;
esac
```

```bash
# case con confirmacion del usuario:
read -rp "Continuar? [y/N] " REPLY
case "${REPLY,,}" in    # ${REPLY,,} convierte a minusculas
    y|yes|si)
        echo "Continuando..."
        ;;
    *)
        echo "Cancelado."
        exit 0
        ;;
esac
```

### ;& y ;;& (bash 4+)

```bash
# ;; = break (sale del case)
# ;& = fall-through (ejecuta el siguiente bloque sin evaluar su patron)
# ;;& = continue (evalua el siguiente patron)

LEVEL=3
case $LEVEL in
    5)  echo "Nivel 5" ;&     # fall-through
    4)  echo "Nivel 4" ;&     # fall-through
    3)  echo "Nivel 3" ;&     # fall-through
    2)  echo "Nivel 2" ;&     # fall-through
    1)  echo "Nivel 1" ;;
esac
# Imprime: Nivel 3, Nivel 2, Nivel 1 (fall-through desde 3)

# ;;& evalua cada patron (como if sin elif):
FILE="script.sh"
case "$FILE" in
    *.sh)   echo "Shell script" ;;&
    *.*)    echo "Tiene extension" ;;&
    s*)     echo "Empieza con s" ;;
esac
# Imprime las tres lineas (todos los patrones coinciden)
```

---

## Errores comunes

### Olvidar comillas en [ ]

```bash
VAR=""

# BUG: variable vacia desaparece
[ $VAR = "hello" ]
# bash ve: [ = "hello" ]  → error: unary operator expected

# CORRECTO:
[ "$VAR" = "hello" ]
# bash ve: [ "" = "hello" ]  → false (correcto)

# Con [[ ]] no hay problema:
[[ $VAR == "hello" ]]
# bash ve: [[ "" == "hello" ]]  → false (correcto)
```

### Confundir = y == y -eq

```bash
# String comparison:
[[ "$A" == "$B" ]]    # compara TEXTO
[ "$A" = "$B" ]       # compara TEXTO (POSIX usa =)

# Numeric comparison:
[[ "$A" -eq "$B" ]]   # compara NUMEROS

# Ejemplo de bug:
A="01"
B="1"
[[ "$A" == "$B" ]]    # false (strings diferentes)
[[ "$A" -eq "$B" ]]   # true (numericamente iguales)
```

### Usar < > sin escapar en [ ]

```bash
# BUG: < es redireccion, no comparacion
[ "a" < "b" ]
# Crea un archivo llamado "b" y falla

# CORRECTO en [ ]:
[ "a" \< "b" ]

# CORRECTO en [[ ]] (no necesita escape):
[[ "a" < "b" ]]
```

### Espacio faltante

```bash
# BUG: falta espacio despues de [ y antes de ]
[-f /etc/passwd]
# bash: [-f: command not found

# CORRECTO:
[ -f /etc/passwd ]
```

---

## Labs

### Parte 1 — File tests

#### Paso 1.1: Tests de existencia y tipo

```bash
docker compose exec debian-dev bash -c '
echo "=== File tests de existencia y tipo ==="
echo ""
echo "--- Preparar archivos de prueba ---"
touch /tmp/test-file.txt
mkdir -p /tmp/test-dir
ln -sf /tmp/test-file.txt /tmp/test-link
echo ""
echo "--- -f: es un archivo regular ---"
[[ -f /tmp/test-file.txt ]] && echo "-f /tmp/test-file.txt: SI"
[[ -f /tmp/test-dir ]] && echo "-f test-dir: SI" || echo "-f /tmp/test-dir: NO (es directorio)"
[[ -f /tmp/test-link ]] && echo "-f /tmp/test-link: SI (sigue el symlink)"
echo ""
echo "--- -d: es un directorio ---"
[[ -d /tmp/test-dir ]] && echo "-d /tmp/test-dir: SI"
[[ -d /tmp/test-file.txt ]] && echo "-d test-file: SI" || echo "-d /tmp/test-file.txt: NO"
echo ""
echo "--- -L / -h: es un symlink ---"
[[ -L /tmp/test-link ]] && echo "-L /tmp/test-link: SI"
[[ -L /tmp/test-file.txt ]] && echo "-L test-file: SI" || echo "-L /tmp/test-file.txt: NO"
echo ""
echo "--- -e: existe (cualquier tipo) ---"
[[ -e /tmp/test-file.txt ]] && echo "-e /tmp/test-file.txt: SI"
[[ -e /tmp/no-existe ]] && echo "-e no-existe: SI" || echo "-e /tmp/no-existe: NO"
echo ""
rm -rf /tmp/test-file.txt /tmp/test-dir /tmp/test-link
'
```

#### Paso 1.2: Tests de permisos

```bash
docker compose exec debian-dev bash -c '
echo "=== File tests de permisos ==="
echo ""
echo "test" > /tmp/perm-test.txt
echo ""
echo "--- Permisos del archivo ---"
ls -l /tmp/perm-test.txt
echo ""
echo "--- -r: legible ---"
[[ -r /tmp/perm-test.txt ]] && echo "-r: SI, legible"
echo ""
echo "--- -w: escribible ---"
[[ -w /tmp/perm-test.txt ]] && echo "-w: SI, escribible"
echo ""
echo "--- -x: ejecutable ---"
[[ -x /tmp/perm-test.txt ]] && echo "-x: SI" || echo "-x: NO, no ejecutable"
chmod +x /tmp/perm-test.txt
[[ -x /tmp/perm-test.txt ]] && echo "-x (despues de chmod +x): SI"
echo ""
echo "--- -s: tamano mayor que cero ---"
[[ -s /tmp/perm-test.txt ]] && echo "-s: SI, tiene contenido"
> /tmp/perm-empty.txt
[[ -s /tmp/perm-empty.txt ]] && echo "-s empty: SI" || echo "-s empty: NO, esta vacio"
echo ""
rm -f /tmp/perm-test.txt /tmp/perm-empty.txt
'
```

#### Paso 1.3: Tests de comparacion de archivos

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparacion de archivos ==="
echo ""
echo "first" > /tmp/old-file.txt
sleep 1
echo "second" > /tmp/new-file.txt
ln /tmp/new-file.txt /tmp/hardlink-file.txt
echo ""
echo "--- -nt: mas nuevo que ---"
[[ /tmp/new-file.txt -nt /tmp/old-file.txt ]] && echo "new-file es mas nuevo que old-file"
echo ""
echo "--- -ot: mas viejo que ---"
[[ /tmp/old-file.txt -ot /tmp/new-file.txt ]] && echo "old-file es mas viejo que new-file"
echo ""
echo "--- -ef: mismo archivo (mismo inode) ---"
[[ /tmp/new-file.txt -ef /tmp/hardlink-file.txt ]] && echo "new-file y hardlink son el mismo inode"
echo ""
ls -li /tmp/new-file.txt /tmp/hardlink-file.txt
echo "(mismo numero de inode)"
echo ""
rm -f /tmp/old-file.txt /tmp/new-file.txt /tmp/hardlink-file.txt
'
```

### Parte 2 — Strings y numeros

#### Paso 2.1: Comparaciones de strings

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparaciones de strings ==="
echo ""
A="hello"
B="world"
EMPTY=""
echo "A=$A B=$B EMPTY=\"$EMPTY\""
echo ""
echo "--- Igualdad ---"
[[ "$A" == "hello" ]] && echo "A == hello: SI"
[[ "$A" == "$B" ]] && echo "A == B: SI" || echo "A == B: NO"
echo ""
echo "--- Desigualdad ---"
[[ "$A" != "$B" ]] && echo "A != B: SI"
echo ""
echo "--- Vacio / no vacio ---"
[[ -z "$EMPTY" ]] && echo "-z EMPTY: SI (esta vacia)"
[[ -z "$A" ]] && echo "-z A: SI" || echo "-z A: NO (tiene contenido)"
[[ -n "$A" ]] && echo "-n A: SI (no esta vacia)"
[[ -n "$EMPTY" ]] && echo "-n EMPTY: SI" || echo "-n EMPTY: NO"
echo ""
echo "--- Orden lexicografico ---"
[[ "apple" < "banana" ]] && echo "apple < banana: SI"
[[ "zebra" > "apple" ]] && echo "zebra > apple: SI"
echo "(< y > son lexicograficos, NO numericos)"
'
```

#### Paso 2.2: Pattern matching y regex

```bash
docker compose exec debian-dev bash -c '
echo "=== Pattern matching con == en [[ ]] ==="
echo ""
FILE="report-2024.tar.gz"
echo "FILE=$FILE"
echo ""
echo "--- Glob patterns ---"
[[ "$FILE" == *.gz ]] && echo "Termina en .gz: SI"
[[ "$FILE" == report* ]] && echo "Empieza con report: SI"
[[ "$FILE" == *2024* ]] && echo "Contiene 2024: SI"
[[ "$FILE" == *.txt ]] && echo "Termina en .txt: SI" || echo "Termina en .txt: NO"
echo ""
echo "=== Regex con =~ ==="
echo ""
IP="192.168.1.100"
echo "IP=$IP"
if [[ "$IP" =~ ^([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})$ ]]; then
    echo "Es un formato de IP valido"
    echo "BASH_REMATCH[0] (completo): ${BASH_REMATCH[0]}"
    echo "BASH_REMATCH[1] (octeto 1): ${BASH_REMATCH[1]}"
    echo "BASH_REMATCH[2] (octeto 2): ${BASH_REMATCH[2]}"
    echo "BASH_REMATCH[3] (octeto 3): ${BASH_REMATCH[3]}"
    echo "BASH_REMATCH[4] (octeto 4): ${BASH_REMATCH[4]}"
fi
echo ""
echo "=~ usa regex extendida (ERE)"
echo "BASH_REMATCH[] captura los grupos ()"
'
```

#### Paso 2.3: Comparaciones numericas

Antes de ejecutar, predice: que pasa con `[[ "9" > "10" ]]`
(comparacion de string) vs `(( 9 > 10 ))` (aritmetica)?

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparaciones numericas ==="
echo ""
A=42
B=100
echo "A=$A B=$B"
echo ""
echo "--- Con [[ ]] (operadores numericos) ---"
[[ $A -eq 42 ]] && echo "A -eq 42: SI"
[[ $A -lt $B ]] && echo "A -lt B: SI ($A < $B)"
[[ $A -gt $B ]] && echo "A -gt B: SI" || echo "A -gt B: NO"
[[ $A -le 42 ]] && echo "A -le 42: SI (<=)"
[[ $A -ge 42 ]] && echo "A -ge 42: SI (>=)"
[[ $A -ne $B ]] && echo "A -ne B: SI (!=)"
echo ""
echo "--- Con (( )) (C-style, mas legible) ---"
(( A == 42 )) && echo "A == 42: SI"
(( A < B )) && echo "A < B: SI"
(( A > 0 && A < 100 )) && echo "A entre 1 y 99: SI"
echo ""
echo "--- PELIGRO: < > con strings vs numeros ---"
[[ "9" > "10" ]] && echo "\"9\" > \"10\" (string): SI (9 > 1 lexicograficamente)"
(( 9 > 10 )) && echo "9 > 10 (numerico): SI" || echo "9 > 10 (numerico): NO"
echo ""
echo "REGLA: para numeros, siempre usar -eq/-lt/-gt o (( ))"
echo "       NUNCA usar < > == para comparar numeros en [[ ]]"
'
```

#### Paso 2.4: if/elif/else

```bash
docker compose exec debian-dev bash -c '
echo "=== if/elif/else ==="
echo ""
check_age() {
    local age=$1
    if (( age < 0 )); then
        echo "Edad $age: invalida"
    elif (( age < 13 )); then
        echo "Edad $age: nino"
    elif (( age < 18 )); then
        echo "Edad $age: adolescente"
    elif (( age < 65 )); then
        echo "Edad $age: adulto"
    else
        echo "Edad $age: senior"
    fi
}

check_age -1
check_age 5
check_age 15
check_age 30
check_age 70
echo ""
echo "--- if con comandos (no solo [[ ]]) ---"
if grep -q "root" /etc/passwd; then
    echo "El usuario root existe en /etc/passwd"
fi
echo ""
echo "if evalua el EXIT CODE del comando"
echo "No solo [[ ]] — cualquier comando funciona como condicion"
'
```

### Parte 3 — case

#### Paso 3.1: case basico

```bash
docker compose exec debian-dev bash -c '
echo "=== case basico ==="
echo ""
classify_file() {
    local file="$1"
    case "$file" in
        *.tar.gz|*.tgz)
            echo "$file: archivo tar comprimido con gzip"
            ;;
        *.tar.bz2|*.tbz2)
            echo "$file: archivo tar comprimido con bzip2"
            ;;
        *.tar.xz|*.txz)
            echo "$file: archivo tar comprimido con xz"
            ;;
        *.zip)
            echo "$file: archivo zip"
            ;;
        *.sh)
            echo "$file: script de shell"
            ;;
        *)
            echo "$file: tipo desconocido"
            ;;
    esac
}

classify_file "backup.tar.gz"
classify_file "data.tar.xz"
classify_file "docs.zip"
classify_file "deploy.sh"
classify_file "image.png"
echo ""
echo "Cada patron puede usar | para alternativas"
echo ";; termina el bloque (no cae al siguiente)"
'
```

#### Paso 3.2: Patrones avanzados en case

```bash
docker compose exec debian-dev bash -c '
echo "=== Patrones avanzados en case ==="
echo ""
process_input() {
    local input="$1"
    case "$input" in
        [Yy]|[Yy][Ee][Ss])
            echo "\"$input\" → respuesta afirmativa"
            ;;
        [Nn]|[Nn][Oo])
            echo "\"$input\" → respuesta negativa"
            ;;
        [0-9])
            echo "\"$input\" → un digito"
            ;;
        [0-9][0-9])
            echo "\"$input\" → dos digitos"
            ;;
        "")
            echo "(vacio) → sin input"
            ;;
        *)
            echo "\"$input\" → no reconocido"
            ;;
    esac
}

process_input "yes"
process_input "Y"
process_input "NO"
process_input "n"
process_input "5"
process_input "42"
process_input ""
process_input "maybe"
echo ""
echo "[Yy] = Y o y"
echo "[0-9] = cualquier digito"
echo "| = alternativa (OR)"
'
```

#### Paso 3.3: ;& (fall-through) y ;;& (continue testing)

Antes de ejecutar, predice: con `;&`, si el primer patron matchea,
que pasa con los siguientes bloques?

```bash
docker compose exec debian-dev bash -c '
echo "=== ;& (fall-through) y ;;& (continue testing) ==="
echo ""
echo "--- ;; (normal): sale despues del match ---"
echo "--- ;& (fall-through): ejecuta el SIGUIENTE bloque sin testear ---"
echo "--- ;;& (continue): sigue testeando los siguientes patrones ---"
echo ""
echo "=== ;& ejemplo: niveles acumulativos ==="
log_level() {
    local level="$1"
    echo "Con nivel $level se registra:"
    case "$level" in
        debug)
            echo "  - DEBUG"
            ;&
        info)
            echo "  - INFO"
            ;&
        warning)
            echo "  - WARNING"
            ;&
        error)
            echo "  - ERROR"
            ;;
    esac
}

log_level "debug"
echo ""
log_level "warning"
echo ""
log_level "error"
echo ""
echo "=== ;;& ejemplo: multiples categorias ==="
categorize() {
    local char="$1"
    echo -n "\"$char\" es: "
    case "$char" in
        [0-9])
            echo -n "digito "
            ;;&
        [a-z])
            echo -n "minuscula "
            ;;&
        [A-Z])
            echo -n "mayuscula "
            ;;&
        [aeiouAEIOU])
            echo -n "vocal "
            ;;&
        [a-zA-Z0-9])
            echo -n "alfanumerico "
            ;;
    esac
    echo ""
}

categorize "a"
categorize "B"
categorize "5"
categorize "z"
'
```

---

## Ejercicios

### Ejercicio 1 — [ ] vs [[ ]] con espacios

Predice que pasa en cada caso:

```bash
FILE="mi archivo.txt"
touch "/tmp/$FILE"

[ -f /tmp/$FILE ] 2>&1; echo "A: $?"
[[ -f /tmp/$FILE ]]; echo "B: $?"

rm "/tmp/$FILE"
```

<details><summary>Prediccion</summary>

- A: Error `too many arguments`, exit 2 — `[ ]` es un comando, `$FILE` sin
  comillas se convierte en `[ -f /tmp/mi archivo.txt ]` (4 argumentos en
  vez de 3).

- B: Exit 0 (true) — `[[ ]]` es una keyword de bash que no hace word
  splitting. Trata `/tmp/mi archivo.txt` como una sola ruta.

</details>

### Ejercicio 2 — File tests encadenados

Predice el resultado de este script sobre `/etc/passwd`, `/tmp`, y
`/no/existe`:

```bash
check_file() {
    local f="$1"
    if [[ ! -e "$f" ]]; then
        echo "$f: no existe"
    elif [[ -d "$f" ]]; then
        echo "$f: directorio"
    elif [[ -f "$f" && -r "$f" ]]; then
        echo "$f: archivo regular legible"
    elif [[ -f "$f" ]]; then
        echo "$f: archivo regular (no legible)"
    else
        echo "$f: otro tipo"
    fi
}

check_file /etc/passwd
check_file /tmp
check_file /no/existe
```

<details><summary>Prediccion</summary>

- `/etc/passwd` → `archivo regular legible` — es un archivo regular (`-f`) y
  tiene permiso de lectura (`-r`).
- `/tmp` → `directorio` — es un directorio, se detecta antes de los tests
  de archivo regular.
- `/no/existe` → `no existe` — `-e` es false.

El orden de los `elif` importa: `-d` se evalua antes que `-f` para que
los directorios no caigan en "archivo regular".

</details>

### Ejercicio 3 — Symlinks rotos

Predice la salida:

```bash
ln -sf /etc/passwd /tmp/good-link
ln -sf /no/existe /tmp/broken-link

[[ -e /tmp/good-link ]] && echo "A: existe" || echo "A: no existe"
[[ -L /tmp/good-link ]] && echo "B: es symlink"
[[ -f /tmp/good-link ]] && echo "C: es archivo regular (target)"

[[ -e /tmp/broken-link ]] && echo "D: existe" || echo "D: no existe"
[[ -L /tmp/broken-link ]] && echo "E: es symlink"
[[ -f /tmp/broken-link ]] && echo "F: es archivo" || echo "F: no es archivo"

rm /tmp/good-link /tmp/broken-link
```

<details><summary>Prediccion</summary>

- A: `existe` — `-e` sigue el symlink, el target `/etc/passwd` existe
- B: `es symlink` — `-L` prueba el symlink mismo
- C: `es archivo regular (target)` — `-f` sigue el symlink, el target es regular
- D: `no existe` — `-e` sigue el symlink, el target `/no/existe` no existe
- E: `es symlink` — `-L` prueba el symlink mismo, que si existe
- F: `no es archivo` — `-f` sigue el symlink, el target no existe

Para detectar un symlink roto: `[[ -L "$f" && ! -e "$f" ]]`.

</details>

### Ejercicio 4 — String vs numerico

Predice true o false para cada uno:

```bash
[[ "10" == "10" ]]; echo "A: $?"
[[ "010" == "10" ]]; echo "B: $?"
[[ "010" -eq "10" ]]; echo "C: $?"
[[ "9" > "10" ]]; echo "D: $?"
(( 9 > 10 )); echo "E: $?"
[[ " 5" -eq "5" ]]; echo "F: $?"
[[ " 5" == "5" ]]; echo "G: $?"
```

<details><summary>Prediccion</summary>

- A: `0` (true) — string "10" == string "10"
- B: `1` (false) — string "010" != string "10" (distintas)
- C: `0` (true) — numerico 010 == 10 (bash interpreta como decimal, no octal aqui)
- D: `0` (true) — lexicografico: '9' (ASCII 57) > '1' (ASCII 49), primer caracter decide
- E: `1` (false) — aritmetico: 9 no es mayor que 10
- F: `0` (true) — numerico: bash ignora espacios al convertir a numero
- G: `1` (false) — string " 5" != string "5" (el espacio importa)

Regla: para numeros, siempre usar `-eq`/`-lt`/`-gt` o `(( ))`. `==`, `<`, `>`
son comparaciones de string.

</details>

### Ejercicio 5 — Pattern matching vs regex

Predice cuales matchean:

```bash
FILE="backup-2024-03-17.tar.gz"

[[ $FILE == *.gz ]] && echo "A: match"
[[ $FILE == backup* ]] && echo "B: match"
[[ $FILE == "*.gz" ]] && echo "C: match" || echo "C: no match"
[[ $FILE =~ \.tar\.gz$ ]] && echo "D: match"
[[ $FILE =~ ^backup-[0-9]{4} ]] && echo "E: match"
[[ $FILE =~ "\.tar" ]] && echo "F: match" || echo "F: no match"
```

<details><summary>Prediccion</summary>

- A: `match` — `*.gz` glob pattern, el archivo termina en .gz
- B: `match` — `backup*` glob pattern, el archivo empieza con backup
- C: `no match` — `"*.gz"` entre comillas es literal, busca exactamente `*.gz`
- D: `match` — regex `\.tar\.gz$`, el archivo termina en .tar.gz
- E: `match` — regex `^backup-[0-9]{4}`, empieza con backup- seguido de 4 digitos
- F: `no match` — regex entre comillas `"\.tar"` busca literal `\.tar` (con backslash), no `.tar`

Regla: no poner entre comillas el patron glob ni la variable regex.
Las comillas desactivan los caracteres especiales.

</details>

### Ejercicio 6 — BASH_REMATCH

Predice que captura este regex:

```bash
LOG="2024-03-17 14:30:45 ERROR Connection timeout"

if [[ $LOG =~ ^([0-9]{4})-([0-9]{2})-([0-9]{2})\ ([0-9]{2}):([0-9]{2}):([0-9]{2})\ ([A-Z]+)\ (.*)$ ]]; then
    echo "Fecha: ${BASH_REMATCH[1]}-${BASH_REMATCH[2]}-${BASH_REMATCH[3]}"
    echo "Hora: ${BASH_REMATCH[4]}:${BASH_REMATCH[5]}:${BASH_REMATCH[6]}"
    echo "Nivel: ${BASH_REMATCH[7]}"
    echo "Mensaje: ${BASH_REMATCH[8]}"
fi
```

<details><summary>Prediccion</summary>

```
Fecha: 2024-03-17
Hora: 14:30:45
Nivel: ERROR
Mensaje: Connection timeout
```

Los grupos capturados:
- `BASH_REMATCH[0]` = linea completa (match total)
- `BASH_REMATCH[1]` = `2024` (ano)
- `BASH_REMATCH[2]` = `03` (mes)
- `BASH_REMATCH[3]` = `17` (dia)
- `BASH_REMATCH[4]` = `14` (hora)
- `BASH_REMATCH[5]` = `30` (minuto)
- `BASH_REMATCH[6]` = `45` (segundo)
- `BASH_REMATCH[7]` = `ERROR` (nivel)
- `BASH_REMATCH[8]` = `Connection timeout` (mensaje, `.*` captura el resto)

`\ ` en el regex representa un espacio literal.

</details>

### Ejercicio 7 — case con tipos de archivo

Predice la salida para cada archivo:

```bash
classify() {
    case "$1" in
        *.tar.gz|*.tgz) echo "$1 → tar+gzip" ;;
        *.tar.bz2)      echo "$1 → tar+bzip2" ;;
        *.gz)           echo "$1 → gzip solo" ;;
        *.sh)           echo "$1 → shell script" ;;
        *)              echo "$1 → desconocido" ;;
    esac
}

classify "backup.tar.gz"
classify "data.gz"
classify "deploy.sh"
classify "image.png"
classify "archive.tgz"
```

<details><summary>Prediccion</summary>

- `backup.tar.gz` → `tar+gzip` — matchea `*.tar.gz`
- `data.gz` → `gzip solo` — NO matchea `*.tar.gz` (no tiene .tar), matchea `*.gz`
- `deploy.sh` → `shell script` — matchea `*.sh`
- `image.png` → `desconocido` — no matchea ningun patron, cae en `*`
- `archive.tgz` → `tar+gzip` — matchea `*.tgz` (alternativa con `|`)

Nota: el orden importa. `*.gz` esta DESPUES de `*.tar.gz`. Si estuviera
antes, `backup.tar.gz` matchearia `*.gz` en vez de `*.tar.gz`, porque case
usa el primer patron que coincide.

</details>

### Ejercicio 8 — ;& fall-through

Predice que imprime `log_level "info"`:

```bash
log_level() {
    echo "Niveles activos:"
    case "$1" in
        debug)   echo "  DEBUG"   ;&
        info)    echo "  INFO"    ;&
        warning) echo "  WARNING" ;&
        error)   echo "  ERROR"   ;;
    esac
}

log_level "debug"
echo "---"
log_level "info"
echo "---"
log_level "error"
```

<details><summary>Prediccion</summary>

```
Niveles activos:
  DEBUG
  INFO
  WARNING
  ERROR
---
Niveles activos:
  INFO
  WARNING
  ERROR
---
Niveles activos:
  ERROR
```

- `debug`: matchea el primer patron, luego `;&` cae al siguiente bloque
  sin evaluar el patron. Cae sucesivamente hasta `;;` en `error`.
- `info`: matchea el segundo patron, cae a `WARNING` y `ERROR`.
- `error`: matchea directamente, solo imprime `ERROR` y sale con `;;`.

`;&` es como el `switch` sin `break` de C/Java. Ejecuta bloques subsiguientes
sin evaluar sus patrones.

</details>

### Ejercicio 9 — ;;& continue testing

Predice que imprime `categorize "A"` y `categorize "3"`:

```bash
categorize() {
    echo -n "\"$1\": "
    case "$1" in
        [0-9])       echo -n "digito " ;;&
        [a-z])       echo -n "minuscula " ;;&
        [A-Z])       echo -n "mayuscula " ;;&
        [aeiouAEIOU]) echo -n "vocal " ;;&
        [a-zA-Z0-9]) echo -n "alfanumerico " ;;
    esac
    echo ""
}

categorize "a"
categorize "A"
categorize "3"
categorize "z"
```

<details><summary>Prediccion</summary>

- `"a"`: minuscula, vocal, alfanumerico
  (matchea `[a-z]`, `[aeiouAEIOU]`, `[a-zA-Z0-9]`)
- `"A"`: mayuscula, vocal, alfanumerico
  (matchea `[A-Z]`, `[aeiouAEIOU]`, `[a-zA-Z0-9]`)
- `"3"`: digito, alfanumerico
  (matchea `[0-9]`, `[a-zA-Z0-9]`; no matchea letras ni vocales)
- `"z"`: minuscula, alfanumerico
  (matchea `[a-z]`, `[a-zA-Z0-9]`; z no es vocal)

`;;&` sigue evaluando los patrones restantes (como if/if/if en vez de
if/elif/elif). Cada patron se evalua independientemente.

</details>

### Ejercicio 10 — Panorama de condicionales

Analiza este script y predice que pasa con cada invocacion:

```bash
#!/usr/bin/env bash
set -euo pipefail

die() { echo "ERROR: $*" >&2; exit 1; }

[[ $# -ge 1 ]] || die "uso: $0 <archivo>"

FILE="$1"
[[ -e "$FILE" ]] || die "no existe: $FILE"

if [[ -d "$FILE" ]]; then
    echo "$FILE es un directorio con $(ls -1 "$FILE" | wc -l) entradas"
elif [[ -f "$FILE" ]]; then
    LINES=$(wc -l < "$FILE")
    SIZE=$(stat --format=%s "$FILE")

    case "$FILE" in
        *.sh)    echo "Script shell: $LINES lineas" ;;
        *.py)    echo "Script Python: $LINES lineas" ;;
        *.tar.*) echo "Archivo comprimido: $SIZE bytes" ;;
        *)       echo "Archivo: $LINES lineas, $SIZE bytes" ;;
    esac

    if (( SIZE > 1048576 )); then
        echo "ADVERTENCIA: archivo mayor a 1MB"
    fi
elif [[ -L "$FILE" ]]; then
    echo "$FILE es un symlink a $(readlink "$FILE")"
else
    echo "$FILE es de tipo especial"
fi
```

Predice para: `script.sh` (10 lineas), `/tmp` (3 archivos), `/no/existe`

<details><summary>Prediccion</summary>

1. `script.sh` (asumiendo 10 lineas, < 1MB) → `Script shell: 10 lineas`
   Entra en `-f`, case matchea `*.sh`.

2. `/tmp` → `/tmp es un directorio con N entradas`
   Entra en `-d`, cuenta entradas con ls.

3. `/no/existe` → `ERROR: no existe: /no/existe`, exit 1
   La linea `[[ -e "$FILE" ]] || die` detecta que no existe.

Nota: el elif `[[ -L "$FILE" ]]` despues de `[[ -f "$FILE" ]]` nunca se
alcanza para symlinks que apuntan a archivos regulares, porque `-f` sigue
symlinks y seria true. Solo aplica para symlinks rotos o a tipos especiales.

</details>
