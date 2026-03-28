# T01 — Getopts (Enhanced)

## Parsing de argumentos: el problema

Sin un parser formal, manejar flags y argumentos se vuelve frágil rápidamente:

```bash
# Aproximación manual (frágil):
#!/bin/bash
VERBOSE=0
OUTPUT=""
FILE=""

# ¿Cómo distinguir -v de -o value de un argumento posicional?
# ¿Qué pasa si el usuario escribe -vo file en lugar de -v -o file?
# ¿Cómo manejar --help?
```

Bash incluye `getopts` para resolver esto de forma estándar.

## getopts — Parsing POSIX

```bash
# Sintaxis:
getopts OPTSTRING VARIABLE

# OPTSTRING define qué opciones acepta:
# "vho:"
#   v → flag -v (sin argumento)
#   h → flag -h (sin argumento)
#   o: → opción -o con argumento obligatorio (el : indica que espera valor)

# VARIABLE recibe la letra de la opción actual
# $OPTARG contiene el argumento de la opción (si tiene :)
# $OPTIND contiene el índice del próximo argumento a procesar
```

### Variables especiales de getopts

| Variable | Descripción |
|---|---|
| `OPTARG` | Argumento de la opción actual (vacío si no tiene `:` en OPTSTRING) |
| `OPTIND` | Índice del próximo argumento a procesar (inicializa en `1`) |
| `OPTERR` | Si设为 `0`，抑制错误信息（默认`1`显示错误）|

### Patrón básico

```bash
#!/usr/bin/env bash
set -euo pipefail

VERBOSE=0
OUTPUT=""

usage() {
    cat << EOF
Uso: $(basename "$0") [opciones] <archivo>

Opciones:
  -v          Modo verbose
  -o ARCHIVO  Archivo de salida
  -h          Mostrar esta ayuda
EOF
}

while getopts "vho:" opt; do
    case $opt in
        v) VERBOSE=1 ;;
        o) OUTPUT="$OPTARG" ;;
        h) usage; exit 0 ;;
        \?) echo "Opción inválida: -$OPTARG" >&2; usage >&2; exit 1 ;;
        :)  echo "La opción -$OPTARG requiere un argumento" >&2; exit 1 ;;
    esac
done

# shift elimina las opciones ya procesadas, dejando solo los argumentos posicionales
shift $((OPTIND - 1))

# $@ ahora contiene solo los argumentos posicionales
if [[ $# -lt 1 ]]; then
    echo "Error: falta el archivo" >&2
    usage >&2
    exit 1
fi

FILE="$1"

echo "verbose=$VERBOSE output=$OUTPUT file=$FILE"
```

```bash
# Invocaciones válidas:
./script.sh -v -o salida.txt entrada.txt
./script.sh -vo salida.txt entrada.txt      # flags combinadas
./script.sh entrada.txt                      # sin opciones
./script.sh -h                               # ayuda
./script.sh -o salida.txt -v entrada.txt     # orden no importa
```

### Manejo de errores

```bash
# getopts tiene dos modos de error:

# 1. Default (verbose) — getopts imprime errores:
while getopts "vho:" opt; do
    # Si el usuario pasa -x (inválida), getopts imprime:
    # "script.sh: illegal option -- x"
    # $opt se pone a "?"
    case $opt in
        \?) exit 1 ;;
    esac
done

# 2. Silent — OPTSTRING empieza con : — errores silenciosos:
while getopts ":vho:" opt; do
    case $opt in
        v) VERBOSE=1 ;;
        h) usage; exit 0 ;;
        o) OUTPUT="$OPTARG" ;;
        \?)
            # Opción desconocida: $OPTARG contiene la letra
            echo "Opción desconocida: -$OPTARG" >&2
            exit 1
            ;;
        :)
            # Falta argumento: $OPTARG contiene la letra
            echo "La opción -$OPTARG requiere un argumento" >&2
            exit 1
            ;;
    esac
done

# El modo silent (:) es SIEMPRE preferible — da control total sobre los
# mensajes de error
```

### OPTSTRING — Guía de caracteres

| Carácter | Significado | Ejemplo |
|---|---|---|
| `x` | Flag `-x`, sin argumento | `getopts "vh" opt` |
| `x:` | Opción `-x` con argumento obligatorio | `getopts "o:" opt` → `-o file` |
| `x::` | Opción `-x` con argumento opcional (bash 4+, no portátil) | `getopts "v::" opt` → `-v` o `-v arg` |
| `:x` | Modo silencioso, opción `-x` con argumento | `getopts ":o:" opt` |

## shift — Desplazar argumentos

`shift` elimina argumentos posicionales del inicio:

```bash
# shift N — eliminar los primeros N argumentos ($1..$N)
# Los restantes se renumeran: $N+1 → $1, $N+2 → $2, etc.

echo "Antes: $1 $2 $3"    # uno dos tres
shift
echo "Después: $1 $2"      # dos tres

shift 2
echo "Después: $1"          # (nada, si solo había 3)
```

```bash
# Patrón con getopts:
while getopts ":vf:" opt; do
    case $opt in
        v) VERBOSE=1 ;;
        f) FILE="$OPTARG" ;;
        \?) echo "Opción inválida: -$OPTARG" >&2; exit 1 ;;
    esac
done
shift $((OPTIND - 1))
# OPTIND apunta al primer argumento que NO es una opción
# Después del shift, $@ contiene solo argumentos posicionales

# Ejemplo:
# ./script.sh -v -f config.txt arg1 arg2
# OPTIND=5 (apunta a arg1, el 5to token)
# shift 4: $1=arg1, $2=arg2
```

### Parsing manual con shift (sin getopts)

Para opciones largas (`--verbose`, `--output=file`), getopts no sirve. Se
usa un `while/case` manual:

```bash
#!/usr/bin/env bash
set -euo pipefail

VERBOSE=0
OUTPUT=""
DRY_RUN=0
FILES=()

usage() {
    cat << EOF
Uso: $(basename "$0") [opciones] <archivos...>

Opciones:
  -v, --verbose          Modo verbose
  -o, --output ARCHIVO   Archivo de salida
  -n, --dry-run          Simular sin ejecutar
  -h, --help             Mostrar esta ayuda
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -o|--output)
            [[ $# -lt 2 ]] && { echo "Error: --output requiere un argumento" >&2; exit 1; }
            OUTPUT="$2"
            shift 2
            ;;
        --output=*)
            OUTPUT="${1#*=}"
            shift
            ;;
        -n|--dry-run)
            DRY_RUN=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            FILES+=("$@")
            break
            ;;
        -*)
            echo "Opción desconocida: $1" >&2
            usage >&2
            exit 1
            ;;
        *)
            FILES+=("$1")
            shift
            ;;
    esac
done

echo "verbose=$VERBOSE output=$OUTPUT dry_run=$DRY_RUN"
echo "files=${FILES[*]}"
```

```bash
# -- indica "fin de opciones":
./script.sh --verbose -- -archivo-con-guion.txt
# Sin --, "-archivo-con-guion.txt" se interpretaría como una opción
# Con --, se trata como argumento posicional
```

## Validación de input

```bash
# Validar que un argumento es un número:
validate_port() {
    local port="$1"
    if ! [[ "$port" =~ ^[0-9]+$ ]] || (( port < 1 || port > 65535 )); then
        echo "Error: puerto inválido: $port (debe ser 1-65535)" >&2
        exit 1
    fi
}

# Validar que un archivo existe:
validate_file() {
    local file="$1"
    if [[ ! -f "$file" ]]; then
        echo "Error: archivo no encontrado: $file" >&2
        exit 1
    fi
    if [[ ! -r "$file" ]]; then
        echo "Error: sin permiso de lectura: $file" >&2
        exit 1
    fi
}

# Validar que un directorio de salida existe:
validate_output_dir() {
    local dir
    dir=$(dirname "$1")
    if [[ ! -d "$dir" ]]; then
        echo "Error: directorio no existe: $dir" >&2
        exit 1
    fi
}
```

## Patrón completo profesional

```bash
#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_NAME=$(basename "$0")
readonly VERSION="1.0.0"

# --- Defaults ---
VERBOSE=0
OUTPUT="/dev/stdout"
FORMAT="text"

# --- Funciones ---
die()   { echo "$SCRIPT_NAME: error: $*" >&2; exit 1; }
warn()  { echo "$SCRIPT_NAME: warning: $*" >&2; }
info()  { (( VERBOSE )) && echo "$SCRIPT_NAME: $*" >&2 || true; }

usage() {
    cat << EOF
Uso: $SCRIPT_NAME [opciones] <archivo> [archivo...]

Procesa archivos y genera un reporte.

Opciones:
  -v, --verbose          Modo verbose
  -o, --output ARCHIVO   Archivo de salida (default: stdout)
  -f, --format FORMATO   Formato: text, json, csv (default: text)
  -h, --help             Mostrar esta ayuda
  -V, --version          Mostrar versión

Ejemplos:
  $SCRIPT_NAME -v data.txt
  $SCRIPT_NAME --format=json -o report.json *.csv
  $SCRIPT_NAME --verbose -- -file-with-dash.txt
EOF
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -v|--verbose)   VERBOSE=1; shift ;;
            -o|--output)    [[ $# -lt 2 ]] && die "--output requiere argumento"
                            OUTPUT="$2"; shift 2 ;;
            --output=*)     OUTPUT="${1#*=}"; shift ;;
            -f|--format)    [[ $# -lt 2 ]] && die "--format requiere argumento"
                            FORMAT="$2"; shift 2 ;;
            --format=*)     FORMAT="${1#*=}"; shift ;;
            -h|--help)      usage; exit 0 ;;
            -V|--version)   echo "$SCRIPT_NAME $VERSION"; exit 0 ;;
            --)             shift; FILES+=("$@"); break ;;
            -*)             die "opción desconocida: $1" ;;
            *)              FILES+=("$1"); shift ;;
        esac
    done
}

validate_args() {
    [[ ${#FILES[@]} -gt 0 ]] || die "falta al menos un archivo (ver --help)"

    for f in "${FILES[@]}"; do
        [[ -f "$f" ]] || die "archivo no encontrado: $f"
        [[ -r "$f" ]] || die "sin permiso de lectura: $f"
    done

    case "$FORMAT" in
        text|json|csv) ;;
        *) die "formato inválido: $FORMAT (usar text, json o csv)" ;;
    esac
}

process() {
    info "Procesando ${#FILES[@]} archivo(s) en formato $FORMAT"

    for f in "${FILES[@]}"; do
        info "Procesando: $f"
        # ... lógica real ...
    done

    info "Resultado escrito en $OUTPUT"
}

# --- Main ---
FILES=()
parse_args "$@"
validate_args
process
```

## getopts vs getopt vs while/case

| Característica | `getopts` (builtin) | `getopt` (externo) | `while/case` manual |
|---|---|---|---|
| Opciones cortas (`-v`) | Sí | Sí | Sí |
| Opciones largas (`--verbose`) | No | Sí | Sí |
| Argumentos opcionales | Bash 4+ (no portátil) | Sí | Manual |
| Portabilidad | POSIX (todos los shells) | GNU-only | Bash 4+ (nameref) |
| Manejo de `--` | Manual | Automático | Manual |
| Disponibilidad | Siempre disponible | Requiere GNU getopt | Bash 4+ para algunas features |
| Complejidad | Baja | Media | Media |

```bash
# Ejemplo con getopt (GNU):
OPTS=$(getopt -o 'vho:' --long 'verbose,help,output:' -n "$0" -- "$@") || exit 1
eval set -- "$OPTS"

while true; do
    case "$1" in
        -v|--verbose) VERBOSE=1; shift ;;
        -o|--output)  OUTPUT="$2"; shift 2 ;;
        -h|--help)    usage; exit 0 ;;
        --)           shift; break ;;
    esac
done

# En la práctica, el while/case manual (sin getopt) es lo más común
# para scripts que necesitan opciones largas
```

## Ejercicios

### Ejercicio 1 — getopts básico

```bash
#!/usr/bin/env bash
set -euo pipefail

COUNT=1
UPPER=0

while getopts ":c:uh" opt; do
    case $opt in
        c) COUNT="$OPTARG" ;;
        u) UPPER=1 ;;
        h) echo "Uso: $(basename "$0") [-c N] [-u] [-h] nombre"; exit 0 ;;
        \?) echo "Opción inválida: -$OPTARG" >&2; exit 1 ;;
        :)  echo "-$OPTARG requiere argumento" >&2; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

NAME="${1:?Falta el nombre}"

for ((i = 0; i < COUNT; i++)); do
    MSG="Hola, $NAME"
    (( UPPER )) && MSG="${MSG^^}"
    echo "$MSG"
done

# Probar:
# ./script.sh -c 3 -u mundo
# ./script.sh mundo
# ./script.sh -c           → ¿qué error da?
# ./script.sh -x           → ¿qué error da?
```

### Ejercicio 2 — Opciones largas con while/case

```bash
#!/usr/bin/env bash
set -euo pipefail

DRY_RUN=0
TARGET=""
FILES=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -n|--dry-run) DRY_RUN=1; shift ;;
        -t|--target)  TARGET="$2"; shift 2 ;;
        --target=*)   TARGET="${1#*=}"; shift ;;
        -h|--help)    echo "Uso: $0 [-n] [-t DIR] archivos..."; exit 0 ;;
        --)           shift; FILES+=("$@"); break ;;
        -*)           echo "Opción desconocida: $1" >&2; exit 1 ;;
        *)            FILES+=("$1"); shift ;;
    esac
done

echo "dry_run=$DRY_RUN target=$TARGET files=(${FILES[*]})"

# Probar:
# ./script.sh --dry-run --target=/tmp file1 file2
# ./script.sh -n -t /tmp -- --weird-filename
```

### Ejercicio 3 — Validación

```bash
# Agregar validación al ejercicio anterior:
# 1. TARGET debe ser un directorio existente (si se especifica)
# 2. Al menos un archivo es obligatorio
# 3. Cada archivo debe existir y ser legible

[[ -n "$TARGET" && ! -d "$TARGET" ]] && { echo "Error: $TARGET no es un directorio" >&2; exit 1; }
[[ ${#FILES[@]} -eq 0 ]] && { echo "Error: falta al menos un archivo" >&2; exit 1; }
for f in "${FILES[@]}"; do
    [[ -f "$f" && -r "$f" ]] || { echo "Error: $f no existe o no es legible" >&2; exit 1; }
done
```

### Ejercicio 4 — Gestor de contactos CLI

```bash
#!/usr/bin/env bash
set -euo pipefail

readonly PROGRAM="${0##*/}"
CONTACT_FILE="${CONTACT_FILE:-$HOME/.contacts}"

show_help() {
    cat << EOF
Uso: $PROGRAM [opciones] [comando]

Gestiona una libreta de contactos.

Comandos:
  add NOMBRE EMAIL [TEL]   Agrega un contacto
  list                    Lista todos los contactos
  find PATRÓN             Busca contactos
  rm NOMBRE               Elimina un contacto

Opciones:
  -f, --file ARCHIVO      Archivo de contactos (default: ~/.contacts)
  -h, --help              Muestra esta ayuda

Ejemplos:
  $PROGRAM add "Ana" "ana@email.com" "555-1234"
  $PROGRAM find "ana"
  $PROGRAM list
EOF
}

# Modo silencioso con getopts
while getopts ":f:h" opt; do
    case $opt in
        f) CONTACT_FILE="$OPTARG" ;;
        h) show_help; exit 0 ;;
        \?) echo "$PROGRAM: opción inválida: -$OPTARG" >&2; exit 1 ;;
        :)  echo "$PROGRAM: -$OPTARG requiere argumento" >&2; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

CMD="${1:-}"
[[ -z "$CMD" ]] && { show_help; exit 1; }
shift || true

add_contact() {
    local name="$1" email="$2" tel="${3:-N/A}"
    echo "$name:$email:$tel" >> "$CONTACT_FILE"
    echo "Contacto agregado: $name"
}

list_contacts() {
    [[ -f "$CONTACT_FILE" ]] || { echo "No hay contactos"; return 0; }
    printf "%-20s %-30s %-15s\n" "NOMBRE" "EMAIL" "TELÉFONO"
    printf "%-20s %-30s %-15s\n" "------" "-----" "--------"
    cut -d: -f1,2,3 "$CONTACT_FILE" 2>/dev/null | while IFS=: read -r n e t; do
        printf "%-20s %-30s %-15s\n" "$n" "$e" "$t"
    done
}

find_contact() {
    local pattern="$1"
    grep -i "$pattern" "$CONTACT_FILE" 2>/dev/null || echo "No encontrado"
}

case "$CMD" in
    add)  [[ $# -lt 2 ]] && die "Usage: add NOMBRE EMAIL [TEL]"
          add_contact "$1" "$2" "${3:-}";;
    list) list_contacts ;;
    find) [[ $# -lt 1 ]] && die "Usage: find PATRÓN"; find_contact "$1" ;;
    rm)   [[ $# -lt 1 ]] && die "Usage: rm NOMBRE"
          grep -v "^${1}:" "$CONTACT_FILE" > "${CONTACT_FILE}.tmp" \
          && mv "${CONTACT_FILE}.tmp" "$CONTACT_FILE"
          echo "Eliminado: $1";;
    *)    show_help; exit 1 ;;
esac

# Probar:
# CONTACT_FILE=/tmp/test-contacts.sh $PROGRAM add "Ana" "ana@x.com" "555"
# CONTACT_FILE=/tmp/test-contacts.sh $PROGRAM list
# CONTACT_FILE=/tmp/test-contacts.sh $PROGRAM find "ana"
# CONTACT_FILE=/tmp/test-contacts.sh $PROGRAM rm "Ana"
```

### Ejercicio 5 — Script de backup con múltiples flags

```bash
#!/usr/bin/env bash
set -euo pipefail

BACKUP_DIR="/tmp/backups"
VERBOSE=0
COMPRESS=0
ENCRYPT=0
SOURCE=""

usage() {
    cat << EOF
Uso: $(basename "$0") -s SOURCE [-d DIR] [-c] [-e] [-v] [-h]

Opciones:
  -s SOURCE   Directorio a respaldar (obligatorio)
  -d DIR      Directorio de destino (default: /tmp/backups)
  -c          Comprimir con gzip
  -e          Cifrar con openssl (AES-256)
  -v          Modo verboso
  -h          Mostrar ayuda

Ejemplos:
  $(basename "$0") -s /home/user/data -d /mnt/backup
  $(basename "$0") -s /home/user/data -c -e -v
EOF
}

while getopts ":s:d:cevh" opt; do
    case $opt in
        s) SOURCE="$OPTARG" ;;
        d) BACKUP_DIR="$OPTARG" ;;
        c) COMPRESS=1 ;;
        e) ENCRYPT=1 ;;
        v) VERBOSE=1 ;;
        h) usage; exit 0 ;;
        \?) echo "Opción inválida: -$OPTARG" >&2; usage; exit 1 ;;
        :)  echo "La opción -$OPTARG requiere un argumento" >&2; usage; exit 1 ;;
    esac
done

[[ -n "$SOURCE" ]] || { usage; exit 1; }
[[ -d "$SOURCE" ]] || { echo "Error: $SOURCE no es un directorio" >&2; exit 1; }
mkdir -p "$BACKUP_DIR"

TS=$(date +%Y%m%d_%H%M%S)
BACKUP_NAME="backup_${TS}.tar"
BACKUP_PATH="${BACKUP_DIR}/${BACKUP_NAME}"

[[ $VERBOSE -eq 1 ]] && echo "Respaldando $SOURCE → $BACKUP_PATH"
tar -cf "$BACKUP_PATH" -C "$(dirname "$SOURCE")" "$(basename "$SOURCE")"

[[ $COMPRESS -eq 1 ]] && {
    [[ $VERBOSE -eq 1 ]] && echo "Comprimiendo..."
    gzip "$BACKUP_PATH"
    BACKUP_PATH="${BACKUP_PATH}.gz"
}

[[ $ENCRYPT -eq 1 ]] && {
    [[ $VERBOSE -eq 1 ]] && echo "Cifrando..."
    openssl enc -aes-256-cbc -pbkdf2 -salt -in "$BACKUP_PATH" -out "${BACKUP_PATH}.enc"
    rm -f "$BACKUP_PATH"
    BACKUP_PATH="${BACKUP_PATH}.enc"
}

[[ $VERBOSE -eq 1 ]] && echo "Backup creado: $BACKUP_PATH"
echo "$BACKUP_PATH"

# Probar (crear directorio de prueba primero):
# mkdir -p /tmp/testdata && echo "test" > /tmp/testdata/file.txt
# ./backup.sh -s /tmp/testdata -d /tmp -v
```

### Ejercicio 6 — Calculadora CLI con validación

```bash
#!/usr/bin/env bash
set -euo pipefail

OPERATION="add"
PRECISION=2

usage() {
    cat << EOF
Uso: $(basename "$0") [opciones] NUM1 NUM2

Calculadora simple de línea de comandos.

Operaciones:
  add, sub, mul, div

Opciones:
  -o, --op OP     Operación: add|sub|mul|div (default: add)
  -p, --prec N    Decimales de precisión (default: 2)

Ejemplos:
  $(basename "$0") 10 5           # 10 + 5 = 15
  $(basename "$0") -o mul 3 4    # 3 * 4 = 12
  $(basename "$0") --op div 10 3 # 10 / 3 = 3.33
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -o|--op)    OPERATION="$2"; shift 2 ;;
        -p|--prec)  PRECISION="$2"; shift 2 ;;
        -h|--help)  usage; exit 0 ;;
        -*)         echo "Opción desconocida: $1" >&2; exit 1 ;;
        *)          break ;;
    esac
done

[[ $# -lt 2 ]] && { echo "Error: se requieren 2 números" >&2; usage; exit 1; }

validate_number() {
    [[ "$1" =~ ^-?[0-9]+\.?[0-9]*$ ]]
}

validate_number "$1" || { echo "Error: '$1' no es un número" >&2; exit 1; }
validate_number "$2" || { echo "Error: '$2' no es un número" >&2; exit 1; }

A="$1"; B="$2"

case "$OPERATION" in
    add) RESULT=$(echo "scale=$PRECISION; $A + $B" | bc) ;;
    sub) RESULT=$(echo "scale=$PRECISION; $A - $B" | bc) ;;
    mul) RESULT=$(echo "scale=$PRECISION; $A * $B" | bc) ;;
    div)
        [[ "$B" == "0" ]] && { echo "Error: división por cero" >&2; exit 1; }
        RESULT=$(echo "scale=$PRECISION; $A / $B" | bc)
        ;;
    *) echo "Error: operación '$OPERATION' no válida" >&2; exit 1 ;;
esac

echo "$RESULT"

# Probar:
# ./calc.sh 10 5
# ./calc.sh -o mul 3 4
# ./calc.sh --op div 10 3 -p 4
# ./calc.sh 10 0   # división por cero
```

### Ejercicio 7 — Wrapper de comandos con dry-run

```bash
#!/usr/bin/env bash
set -euo pipefail

DRY_RUN=0
VERBOSE=0
COMMAND=""

usage() {
    cat << EOF
Uso: $(basename "$0") [opciones] -- COMANDO [ARG...]

Envuelve un comando con opciones de dry-run y verbose.

Opciones:
  -n, --dry-run   Muestra el comando sin ejecutarlo
  -v, --verbose  Muestra el comando antes de ejecutarlo
  -h, --help     Muestra esta ayuda

Ejemplos:
  $(basename "$0") -n -- rm -rf /tmp/test
  $(basename "$0") -v -- cp file1.txt file2.txt
  $(basename "$0") --dry-run -- mv *.log /tmp
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -n|--dry-run) DRY_RUN=1; shift ;;
        -v|--verbose) VERBOSE=1; shift ;;
        -h|--help)    usage; exit 0 ;;
        --)           shift; COMMAND="$*"; break ;;
        -*)           echo "Opción desconocida: $1" >&2; exit 1 ;;
        *)            echo "Usar -- para separar opciones del comando" >&2; exit 1 ;;
    esac
done

[[ -z "$COMMAND" ]] && { usage; exit 1; }

if [[ $DRY_RUN -eq 1 ]]; then
    echo "[DRY-RUN] $COMMAND"
elif [[ $VERBOSE -eq 1 ]]; then
    echo "[EXEC] $COMMAND"
    eval "$COMMAND"
else
    eval "$COMMAND"
fi

# Probar:
# ./run.sh -n -- ls -la
# ./run.sh -v -- echo "hello world"
# ./run.sh -- ls -la /tmp
```

### Ejercicio 8 — Catálogo de archivos con filtros

```bash
#!/usr/bin/env bash
set -euo pipefail

SORT_BY="name"
REVERSE=0
MIN_SIZE=0
SHOW_HIDDEN=0
DIRECTORY="."

usage() {
    cat << EOF
Uso: $(basename "$0") [opciones] [DIR]

Catálogo de archivos con filtros.

Opciones:
  -s, --sort CRITERIO   Ordenar por: name|size|date (default: name)
  -r, --reverse         Invertir orden
  -m, --min-size N      Mostrar solo archivos >= N bytes
  -a, --all             Mostrar archivos ocultos
  -h, --help            Mostrar esta ayuda

Ejemplos:
  $(basename "$0") /home/user
  $(basename "$0") -s size -r /var/log
  $(basename "$0") -m 1024 --all /tmp
EOF
}

while getopts ":s:rm:ah" opt; do
    case $opt in
        s) SORT_BY="$OPTARG" ;;
        r) REVERSE=1 ;;
        m) MIN_SIZE="$OPTARG" ;;
        a) SHOW_HIDDEN=1 ;;
        h) usage; exit 0 ;;
        \?) echo "Opción inválida: -$OPTARG" >&2; exit 1 ;;
        :)  echo "La opción -$OPTARG requiere argumento" >&2; exit 1 ;;
    esac
done
shift $((OPTIND - 1))
[[ -n "${1:-}" ]] && DIRECTORY="$1"

[[ -d "$DIRECTORY" ]] || { echo "Error: $DIRECTORY no es un directorio" >&2; exit 1; }

[[ "$SORT_BY" =~ ^(name|size|date)$ ]] || { echo "Error: -s debe ser name|size|date" >&2; exit 1; }

mapfile -t FILES < <(
    if [[ $SHOW_HIDDEN -eq 1 ]]; then
        find "$DIRECTORY" -maxdepth 1 -type f -printf "%s %T@ %f\n"
    else
        find "$DIRECTORY" -maxdepth 1 -type f ! -name '.*' -printf "%s %T@ %f\n"
    fi
)

for entry in "${FILES[@]}"; do
    size="${entry%% *}"
    rest="${entry#* }"
    mtime="${rest%% *}"
    name="${rest#* }"
    [[ $size -ge $MIN_SIZE ]] || continue
    printf "%10d %s %s\n" "$size" "$(date -d "@${mtime}" '+%Y-%m-%d %H:%M')" "$name"
done | sort -k3 | {
    [[ $REVERSE -eq 1 ]] && sort -rk1 || sort -k1
}

# Probar:
# ./catalog.sh /etc
# ./catalog.sh -s size -r /tmp
# ./catalog.sh -m 1024 -a ~
```

### Ejercicio 9 — Parser de flags combinados

```bash
#!/usr/bin/env bash
set -euo pipefail

VERBOSE=0
DRY_RUN=0
FORCE=0
RECURSIVE=0
TARGET=""

usage() {
    cat << EOF
Uso: $(basename "$0") [banderas combinadas] <target>

Banderas:
  -v  Modo verbose
  -n  Dry-run
  -f  Forzar
  -r  Recursivo
  -h  Mostrar ayuda

Las banderas pueden combinarse: -vnf, -rvf, etc.

Ejemplos:
  $(basename "$0") -vnf archivo.txt
  $(basename "$0") -r directorio
EOF
}

OPTSTRING=":vnfrh"
while getopts "$OPTSTRING" opt; do
    case $opt in
        v) VERBOSE=1 ;;
        n) DRY_RUN=1 ;;
        f) FORCE=1 ;;
        r) RECURSIVE=1 ;;
        h) usage; exit 0 ;;
        \?) echo "Opción inválida: -$OPTARG" >&2; exit 1 ;;
        :)  echo "La opción -$OPTARG requiere argumento" >&2; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

TARGET="${1:-}"

[[ -n "$TARGET" ]] || { usage; exit 1; }

echo "verbose=$VERBOSE dry_run=$DRY_RUN force=$FORCE recursive=$RECURSIVE target=$TARGET"

# Probar:
# ./flags.sh -vnf archivo.txt
# ./flags.sh -rvf directorio
# ./flags.sh -h
```

### Ejercicio 10 — Tabla de opciones conocidas

```bash
#!/usr/bin/env bash
set -euo pipefail

FORMAT="table"
QUIET=0
COUNT=0

while getopts ":f:qh" opt; do
    case $opt in
        f) FORMAT="$OPTARG" ;;
        q) QUIET=1 ;;
        h) echo "Ayuda: $(basename "$0") [-f FORMAT] [-q] [-h]"; exit 0 ;;
        \?) echo "Opción inválida: -$OPTARG" >&2; exit 1 ;;
        :)  echo "La opción -$OPTARG requiere argumento" >&2; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

# Mostrar tabla de valores procesados
if [[ $QUIET -eq 0 ]]; then
    case "$FORMAT" in
        table)
            printf "| %-10s | %-10s |\n" "Variable" "Valor"
            printf "|------------|------------|\n"
            printf "| %-10s | %-10s |\n" "FORMAT" "$FORMAT"
            printf "| %-10s | %-10s |\n" "QUIET" "$QUIET"
            printf "| %-10s | %-10s |\n" "COUNT" "$COUNT"
            ;;
        json)
            echo "{"
            echo "  \"format\": \"$FORMAT\","
            echo "  \"quiet\": $QUIET,"
            echo "  \"count\": $COUNT"
            echo "}"
            ;;
        csv)
            echo "variable,valor"
            echo "FORMAT,$FORMAT"
            echo "QUIET,$QUIET"
            echo "COUNT,$COUNT"
            ;;
        *) echo "Formato desconocido: $FORMAT" >&2; exit 1 ;;
    esac
fi

# Probar:
# ./opts.sh -f table
# ./opts.sh -f json -q
# ./opts.sh -f csv | column -t -s,
```
