# T01 — Getopts

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

## getopts vs getopt

```bash
# getopts (builtin de bash):
# + POSIX, portable
# + Disponible en todos los shells
# - Solo opciones cortas (-v, -o)
# - No soporta opciones largas (--verbose)

# getopt (comando externo, /usr/bin/getopt):
# + Soporta opciones largas (--verbose)
# + Maneja argumentos opcionales
# - No es POSIX (GNU extension)
# - Sintaxis más compleja
# - La versión de macOS (BSD) es limitada

# Ejemplo con getopt:
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

---

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
