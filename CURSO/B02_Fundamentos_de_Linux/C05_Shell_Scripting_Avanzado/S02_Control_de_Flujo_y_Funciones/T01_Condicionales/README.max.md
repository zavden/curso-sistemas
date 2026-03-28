# T01 — Condicionales

## test, [ ] y [[ ]]

Bash tiene tres formas de evaluar condiciones. Las tres devuelven exit code 0
(verdadero) o 1 (falso):

```bash
# 1. test — comando externo (/usr/bin/test)
test -f /etc/passwd
echo $?    # 0

# 2. [ ] — sinónimo de test (es literalmente un comando llamado [)
[ -f /etc/passwd ]
echo $?    # 0
# Los espacios después de [ y antes de ] son OBLIGATORIOS
# [ es un comando, ] es su último argumento

# 3. [[ ]] — keyword de bash (NO es un comando externo)
[[ -f /etc/passwd ]]
echo $?    # 0
```

```bash
# Verificar que [ es un comando real:
type [
# [ is a shell builtin  (bash tiene un builtin, pero también existe /usr/bin/[)

type [[
# [[ is a shell keyword  (parte de la sintaxis de bash)

ls -la /usr/bin/[
# -rwxr-xr-x 1 root root ... /usr/bin/[
# Es un ejecutable real (también existe /usr/bin/test)
```

---

## Diferencias entre [ ] y [[ ]]

### Word splitting y globbing

```bash
FILE="mi archivo.txt"

# [ ] está sujeto a word splitting:
[ -f $FILE ]
# bash ve: [ -f mi archivo.txt ]
# Error: too many arguments

# [[ ]] NO hace word splitting:
[[ -f $FILE ]]
# bash ve: [[ -f mi archivo.txt ]]
# Funciona correctamente
```

### Operadores lógicos

```bash
# [ ] usa -a (AND) y -o (OR) — POSIX pero frágiles:
[ -f /etc/passwd -a -r /etc/passwd ]      # AND
[ -f /etc/passwd -o -f /etc/shadow ]       # OR

# [[ ]] usa && y || — más claros:
[[ -f /etc/passwd && -r /etc/passwd ]]     # AND
[[ -f /etc/passwd || -f /etc/shadow ]]     # OR

# Con [ ], la alternativa segura es encadenar comandos:
[ -f /etc/passwd ] && [ -r /etc/passwd ]   # AND con dos [ ]
```

### Comparación de strings

```bash
VAR="hello"

# [ ] — comparación literal:
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
| Word splitting | Sí (necesita comillas) | No |
| Globbing | Sí (peligroso) | No (pattern matching) |
| Operadores lógicos | `-a`, `-o` | `&&`, `\|\|` |
| Pattern matching | No | `==` con globs |
| Regex | No | `=~` |
| Comparación strings | `=`, `!=` | `=`, `==`, `!=` |
| POSIX compatible | Sí | No (solo bash/zsh/ksh) |
| Recomendación | Solo en `#!/bin/sh` | **Siempre en scripts bash** |

---

## Tests de archivos

```bash
# Existencia y tipo:
[[ -e FILE ]]     # existe (cualquier tipo)
[[ -f FILE ]]     # es un archivo regular
[[ -d FILE ]]     # es un directorio
[[ -L FILE ]]     # es un symlink
[[ -b FILE ]]     # es un dispositivo de bloque
[[ -c FILE ]]     # es un dispositivo de carácter
[[ -p FILE ]]     # es un named pipe (FIFO)
[[ -S FILE ]]     # es un socket

# Permisos:
[[ -r FILE ]]     # tiene permiso de lectura
[[ -w FILE ]]     # tiene permiso de escritura
[[ -x FILE ]]     # tiene permiso de ejecución
[[ -u FILE ]]     # tiene SUID
[[ -g FILE ]]     # tiene SGID
[[ -k FILE ]]     # tiene sticky bit

# Tamaño:
[[ -s FILE ]]     # existe y tamaño > 0

# Comparación:
[[ FILE1 -nt FILE2 ]]   # FILE1 es más nuevo (newer than)
[[ FILE1 -ot FILE2 ]]   # FILE1 es más viejo (older than)
[[ FILE1 -ef FILE2 ]]   # son el mismo inode (hardlink)

# Descriptores:
[[ -t FD ]]       # FD está abierto y conectado a un terminal
```

### Patrón típico de validación

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
# -f, -d, -r, etc. SIGUEN symlinks (evalúan el target):
ln -s /etc/passwd /tmp/link-passwd

[[ -f /tmp/link-passwd ]]   # true (el target es un archivo regular)
[[ -L /tmp/link-passwd ]]   # true (es un symlink)

# Symlink roto (target no existe):
ln -s /no/existe /tmp/broken-link

[[ -e /tmp/broken-link ]]   # false (el target no existe)
[[ -L /tmp/broken-link ]]   # true (el symlink sí existe)

# Verificar symlink roto:
[[ -L "$FILE" && ! -e "$FILE" ]]   # es symlink pero target no existe
```

---

## Comparaciones de strings

```bash
# Igualdad/desigualdad:
[[ "$VAR" == "value" ]]    # igual
[[ "$VAR" != "value" ]]    # distinto

# Vacío/no vacío:
[[ -z "$VAR" ]]     # true si VAR está vacía (zero length)
[[ -n "$VAR" ]]     # true si VAR NO está vacía (non-zero)

# -z y -n son las formas correctas de verificar si una variable tiene valor:
if [[ -z "$DB_HOST" ]]; then
    echo "Error: DB_HOST no está definida" >&2
    exit 1
fi
```

### Orden lexicográfico

```bash
# < y > comparan orden alfabético (locale-dependent):
[[ "apple" < "banana" ]]    # true
[[ "banana" > "apple" ]]    # true

# CUIDADO: en [ ] hay que escapar < y >
[ "apple" \< "banana" ]     # necesita \ en [ ]
# Sin escapar, < se interpreta como redirección
```

### Pattern matching en [[ ]]

```bash
FILE="backup-2024-03-17.tar.gz"

# Patrones glob (*, ?, [...]):
[[ $FILE == *.tar.gz ]]        # true
[[ $FILE == backup-* ]]         # true
[[ $FILE == backup-????-* ]]   # true (? = un carácter)

# NO poner el patrón entre comillas — las comillas lo hacen literal:
[[ $FILE == "*.tar.gz" ]]      # false (busca literalmente *.tar.gz)
[[ $FILE == *.tar.gz ]]         # true (pattern matching)

# Uso práctico:
case_insensitive() {
    local input="${1,,}"   # convertir a minúsculas
    [[ $input == y* ]]     # ¿empieza con 'y'?
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
    echo "Año: ${BASH_REMATCH[1]}"               # 2024
    echo "Mes: ${BASH_REMATCH[2]}"               # 03
    echo "Día: ${BASH_REMATCH[3]}"               # 17
fi

# Guardar regex en una variable (para evitar problemas con quoting):
PATTERN='^v[0-9]+\.[0-9]+\.[0-9]+$'
[[ "v1.2.3" =~ $PATTERN ]]    # true (semver)
# NO poner la variable regex entre comillas:
[[ "v1.2.3" =~ "$PATTERN" ]]  # busca literal, no regex
```

---

## Comparaciones numéricas

```bash
# Operadores numéricos (funcionan en [ ] y [[ ]]):
[[ $A -eq $B ]]    # equal
[[ $A -ne $B ]]    # not equal
[[ $A -lt $B ]]    # less than
[[ $A -le $B ]]    # less than or equal
[[ $A -gt $B ]]    # greater than
[[ $A -ge $B ]]    # greater than or equal

# IMPORTANTE: == y != comparan STRINGS, no números:
[[ "01" == "1" ]]    # false (strings diferentes)
[[ "01" -eq "1" ]]   # true (numéricamente iguales)

[[ " 5" == "5" ]]    # false (el espacio importa)
[[ " 5" -eq "5" ]]   # true (numéricamente iguales)
```

### (( )) para aritmética

```bash
# (( )) es más natural para comparaciones numéricas:
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
(( X > 5 && X < 20 ))   # true
(( X == 10 || Y == 10 ))  # true
```

---

## if/elif/else

```bash
# Sintaxis:
if CONDICIÓN; then
    # código si verdadero
elif OTRA_CONDICIÓN; then
    # código si la segunda es verdadera
else
    # código si ninguna es verdadera
fi

# La CONDICIÓN es cualquier comando — if evalúa su exit code
# [[ ]] y (( )) son solo los más comunes
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
    echo "Docker está instalado"
fi
```

### Patrón de validación de argumentos

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

`case` es más legible que cadenas de `if/elif` cuando se compara una variable
contra múltiples patrones:

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
# case con confirmación del usuario:
read -rp "¿Continuar? [y/N] " REPLY
case "${REPLY,,}" in    # ${REPLY,,} convierte a minúsculas
    y|yes|si|sí)
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
# ;& = fall-through (ejecuta el siguiente bloque sin evaluar su patrón)
# ;;& = continue (evalúa el siguiente patrón)

LEVEL=3
case $LEVEL in
    5)  echo "Nivel 5" ;&     # fall-through
    4)  echo "Nivel 4" ;&     # fall-through
    3)  echo "Nivel 3" ;&     # fall-through
    2)  echo "Nivel 2" ;&     # fall-through
    1)  echo "Nivel 1" ;;
esac
# Imprime: Nivel 3, Nivel 2, Nivel 1 (fall-through desde 3)

# ;;& evalúa cada patrón (como if sin elif):
FILE="script.sh"
case "$FILE" in
    *.sh)   echo "Shell script" ;;&
    *.*)    echo "Tiene extensión" ;;&
    s*)     echo "Empieza con s" ;;
esac
# Imprime las tres líneas (todos los patrones coinciden)
```

---

## Errores comunes

### Olvidar comillas en [ ]

```bash
VAR=""

# BUG: variable vacía desaparece
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
[[ "$A" -eq "$B" ]]   # compara NÚMEROS

# Ejemplo de bug:
A="01"
B="1"
[[ "$A" == "$B" ]]    # false (strings diferentes)
[[ "$A" -eq "$B" ]]   # true (numéricamente iguales)
```

### Usar < > sin escapar en [ ]

```bash
# BUG: < es redirección, no comparación
[ "a" < "b" ]
# Crea un archivo llamado "b" y falla

# CORRECTO en [ ]:
[ "a" \< "b" ]

# CORRECTO en [[ ]] (no necesita escape):
[[ "a" < "b" ]]
```

---

## Ejercicios

### Ejercicio 1 — Diferencia entre [ ] y [[ ]]

```bash
FILE="mi archivo.txt"
touch "/tmp/$FILE"

# ¿Cuál funciona y cuál falla?
[ -f /tmp/$FILE ] 2>&1
[[ -f /tmp/$FILE ]]

# Verificar:
echo "[ ] exit: $([ -f /tmp/$FILE ] 2>/dev/null; echo $?)"
echo "[[ ]] exit: $([[ -f /tmp/$FILE ]]; echo $?)"

rm "/tmp/$FILE"
```

---

### Ejercicio 2 — Validar un número

```bash
INPUT="${1:-abc}"

if [[ $INPUT =~ ^-?[0-9]+$ ]]; then
    echo "$INPUT es un número entero"
else
    echo "$INPUT NO es un número entero"
fi

# Probar con: 42, -5, 3.14, abc, "", "1 2"
```

---

### Ejercicio 3 — case con tipos de archivo

```bash
FILE="${1:-test.txt}"

case "$FILE" in
    *.tar.gz|*.tgz)  echo "tar+gzip" ;;
    *.tar.bz2)       echo "tar+bzip2" ;;
    *.tar.xz)        echo "tar+xz" ;;
    *.gz)            echo "gzip (sin tar)" ;;
    *.zip)           echo "zip" ;;
    *.sh)            echo "shell script" ;;
    *.py)            echo "python" ;;
    *)               echo "desconocido" ;;
esac
```

---

### Ejercicio 4 — Regex para validación

```bash
#!/bin/bash
validate_email() {
    local email="$1"
    if [[ "$email" =~ ^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$ ]]; then
        echo "Email válido"
    else
        echo "Email inválido"
    fi
}

validate_email "user@example.com"
validate_email "invalid.email"
validate_email "@example.com"
```

---

### Ejercicio 5 — Tests de archivos en cadena

```bash
FILE="${1:-/etc/passwd}"

# Verificar existencia, tipo y permisos
if [[ ! -e "$FILE" ]]; then
    echo "El archivo no existe"
elif [[ ! -f "$FILE" ]]; then
    echo "No es un archivo regular"
elif [[ ! -r "$FILE" ]]; then
    echo "No es legible"
else
    echo "OK: archivo regular legible"
fi
```

---

### Ejercicio 6 — Comparar números correctamente

```bash
A="42"
B="42"

# String comparison (incorrecto para números)
[[ "$A" == "$B" ]] && echo "String: iguales" || echo "String: diferentes"

# Numeric comparison (correcto)
[[ "$A" -eq "$B" ]] && echo "Numérico: iguales" || echo "Numérico: diferentes"

# Con (( ))
(( A == B )) && echo "Aritmético: iguales"
```

---

### Ejercicio 7 — BASH_REMATCH con grupos

```bash
LOG="2024-03-17 14:30:45 ERROR Connection timeout"

if [[ $LOG =~ ^([0-9]{4})-([0-9]{2})-([0-9]{2})\ ([0-9]{2}):([0-9]{2}):([0-9]{2})\ ([A-Z]+)\ (.*)$ ]]; then
    echo "Fecha: ${BASH_REMATCH[1]}-${BASH_REMATCH[2]}-${BASH_REMATCH[3]}"
    echo "Hora: ${BASH_REMATCH[4]}:${BASH_REMATCH[5]}:${BASH_REMATCH[6]}"
    echo "Nivel: ${BASH_REMATCH[7]}"
    echo "Mensaje: ${BASH_REMATCH[8]}"
fi
```

---

### Ejercicio 8 —case con expansión de parámetros

```bash
#!/bin/bash
#case con expansiones para normalizar input
INPUT="${1:-}"

case "${INPUT,,}" in  # Convertir a minúsculas
    y|yes|si)
        echo "Afirmativo"
        ;;
    n|no)
        echo "Negativo"
        ;;
    q|quit)
        echo "Salir"
        ;;
    *)
        echo "Opción desconocida: $INPUT"
        ;;
esac
```
