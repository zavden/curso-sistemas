# T01 — Getopts

> **Fuentes**: README.md + README.max.md + labs/README.md
> **Regla aplicada**: 2 (`.max.md` sin `.claude.md`)

## Errata detectada en las fuentes

| # | Archivo | Línea(s) | Error | Corrección |
|---|---------|----------|-------|------------|
| 1 | max.md | 44 | Texto en chino para la descripción de `OPTERR`: `Si设为 0，抑制错误信息（默认1显示错误）` | Debe ser español: "Si vale `0`, suprime los mensajes de error (default `1`)" |
| 2 | max.md | 144 | `x::` listado como argumento opcional de `getopts` (bash 4+) | `getopts` NO soporta `::` — eso es de GNU `getopt` (comando externo). `getopts` solo acepta `:` (obligatorio) |
| 3 | md/max.md | 58/66 | Patrón básico usa OPTSTRING `"vho:"` (modo verbose) pero incluye handlers `\?)` con `$OPTARG` y `:)` | En modo verbose, `$OPTARG` no se llena para opciones inválidas y el caso `:)` nunca se ejecuta. El OPTSTRING debería ser `":vho:"` (modo silencioso) |
| 4 | max.md | 392 | Tabla: while/case manual listado como "Bash 4+ (nameref)" en portabilidad | while/case no usa namerefs — funciona en bash 3, dash, etc. Es muy portable |
| 5 | max.md | 564 | Ejercicio 4 usa `die "..."` pero la función `die` nunca se define en el script | Falta `die() { echo "error: $*" >&2; exit 1; }` |
| 6 | max.md | 724 | Comentario sugiere `./calc.sh --op div 10 3 -p 4` | El case tiene `*) break ;;` que detiene el parsing al encontrar `10`, así que `-p 4` nunca se procesa |
| 7 | max.md | 760 | `COMMAND="$*"` y luego `eval "$COMMAND"` | `$*` pierde las fronteras entre argumentos y `eval` es peligroso; mejor usar un array `CMD=("$@")` y ejecutar `"${CMD[@]}"` |
| 8 | max.md | 848-850 | La variable `SORT_BY` se valida pero nunca se usa en la lógica de sort | El sort siempre ordena por columna 1 (size), ignorando `SORT_BY` |

---

## Parsing de argumentos: el problema

Sin un parser formal, manejar flags y argumentos se vuelve frágil:

```bash
#!/bin/bash
VERBOSE=0
OUTPUT=""
FILE=""

# ¿Cómo distinguir -v de -o value de un argumento posicional?
# ¿Qué pasa si el usuario escribe -vo file en lugar de -v -o file?
# ¿Cómo manejar --help?
```

Bash incluye `getopts` para resolver esto de forma estándar.

---

## getopts — Parsing POSIX

```bash
# Sintaxis:
getopts OPTSTRING VARIABLE

# OPTSTRING define qué opciones acepta:
# ":vho:"
#   : → al inicio: modo silencioso (control total de errores)
#   v → flag -v (sin argumento)
#   h → flag -h (sin argumento)
#   o: → opción -o con argumento obligatorio (el : DESPUÉS indica que espera valor)

# VARIABLE recibe la letra de la opción actual
# $OPTARG contiene el argumento de la opción (si tiene :)
# $OPTIND contiene el índice del próximo argumento a procesar
```

### Variables especiales

| Variable | Descripción |
|---|---|
| `OPTARG` | Argumento de la opción actual. En modo silencioso: la letra inválida (para `?`) o la que falta argumento (para `:`) |
| `OPTIND` | Índice del próximo argumento a procesar (inicializa en `1`) |
| `OPTERR` | Si vale `0`, suprime los mensajes de error automáticos (default `1`) |

### OPTSTRING — Guía

| Carácter | Posición | Significado | Ejemplo |
|---|---|---|---|
| `:` | Al inicio | Modo silencioso (no imprime errores) | `":vho:"` |
| `x` | Cualquiera | Flag `-x`, sin argumento | `v` en `":vh"` |
| `x:` | Cualquiera | Opción `-x` con argumento **obligatorio** | `o:` en `":vho:"` |

> **Nota**: las fuentes listan `x::` como argumento opcional de `getopts`.
> Esto es **incorrecto** — `getopts` (builtin POSIX) NO soporta argumentos
> opcionales. Esa funcionalidad es de GNU `getopt` (comando externo,
> `/usr/bin/getopt`).

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

# IMPORTANTE: el : al inicio activa modo silencioso
while getopts ":vho:" opt; do
    case $opt in
        v) VERBOSE=1 ;;
        o) OUTPUT="$OPTARG" ;;
        h) usage; exit 0 ;;
        \?)
            # Opción desconocida: $OPTARG contiene la letra
            echo "Opción inválida: -$OPTARG" >&2
            usage >&2
            exit 1
            ;;
        :)
            # Falta argumento: $OPTARG contiene la letra
            echo "La opción -$OPTARG requiere un argumento" >&2
            exit 1
            ;;
    esac
done

# shift elimina las opciones ya procesadas
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

### Modo verbose vs silencioso

```bash
# Modo VERBOSE (sin : al inicio) — getopts imprime errores él mismo:
while getopts "vho:" opt; do
    # Opción inválida -x → getopts imprime:
    #   "script.sh: illegal option -- x"
    # $opt = "?", $OPTARG NO se llena
    # Caso ":" NUNCA se ejecuta (getopts maneja falta de argumento como ?)
    case $opt in
        \?) exit 1 ;;    # $OPTARG vacío en este modo
    esac
done

# Modo SILENCIOSO (: al inicio) — control total:
while getopts ":vho:" opt; do
    case $opt in
        v) VERBOSE=1 ;;
        h) usage; exit 0 ;;
        o) OUTPUT="$OPTARG" ;;
        \?)
            # Opción desconocida: $OPTARG = la letra inválida
            echo "Opción desconocida: -$OPTARG" >&2
            exit 1
            ;;
        :)
            # Falta argumento: $OPTARG = la letra que lo necesita
            echo "La opción -$OPTARG requiere un argumento" >&2
            exit 1
            ;;
    esac
done

# SIEMPRE usar modo silencioso — da control total sobre los mensajes de error
```

> **Punto clave**: en modo verbose, `$OPTARG` NO contiene la letra de la opción
> inválida y el caso `:)` nunca se ejecuta. Si necesitas mensajes de error
> personalizados (y siempre los necesitas), usa modo silencioso.

---

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

---

## Opciones largas con while/case

`getopts` solo soporta opciones cortas (`-v`, `-o`). Para opciones largas
(`--verbose`, `--output=file`), se usa un `while/case` manual:

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
            OUTPUT="${1#*=}"       # elimina todo hasta e incluyendo '='
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
            FILES+=("$@")          # todo lo que queda son argumentos
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

### El separador `--`

```bash
# -- indica "fin de opciones":
./script.sh --verbose -- -archivo-con-guion.txt
# Sin --, "-archivo-con-guion.txt" se interpretaría como una opción
# Con --, se trata como argumento posicional
```

### `${1#*=}` — Extraer valor de `--flag=valor`

```bash
# ${variable#pattern} — elimina el prefijo más corto que matchee pattern
--output=/tmp/result.txt
${1#*=}    # → /tmp/result.txt (elimina todo hasta e incluyendo el primer =)
```

---

## Validación de input

```bash
# Validar que un argumento es un número (puerto):
validate_port() {
    local port="$1"
    if ! [[ "$port" =~ ^[0-9]+$ ]] || (( port < 1 || port > 65535 )); then
        echo "Error: puerto inválido: $port (debe ser 1-65535)" >&2
        exit 1
    fi
}

# Validar que un archivo existe y es legible:
validate_file() {
    local file="$1"
    [[ -f "$file" ]] || { echo "Error: archivo no encontrado: $file" >&2; exit 1; }
    [[ -r "$file" ]] || { echo "Error: sin permiso de lectura: $file" >&2; exit 1; }
}

# Validar que un directorio de salida existe:
validate_output_dir() {
    local dir
    dir=$(dirname "$1")
    [[ -d "$dir" ]] || { echo "Error: directorio no existe: $dir" >&2; exit 1; }
}
```

---

## Patrón profesional completo

```bash
#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_NAME=$(basename "$0")
readonly VERSION="1.0.0"

# --- Defaults ---
VERBOSE=0
OUTPUT="/dev/stdout"
FORMAT="text"

# --- Funciones de utilidad ---
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

### Estructura del patrón

1. **Shebang + strict mode** — `set -euo pipefail`
2. **Constantes** — `readonly SCRIPT_NAME`, `VERSION`
3. **Defaults** — valores por defecto de todas las opciones
4. **Utilidades** — `die()`, `warn()`, `info()`
5. **`usage()`** — ayuda con ejemplos
6. **`parse_args()`** — parsing con while/case
7. **`validate_args()`** — validar después de parsear
8. **`process()`** — lógica principal
9. **Main** — llamar en orden: parse → validate → process

---

## getopts vs getopt vs while/case

| Característica | `getopts` (builtin) | `getopt` (externo) | `while/case` manual |
|---|---|---|---|
| Opciones cortas (`-v`) | Sí | Sí | Sí |
| Combinar flags (`-vo`) | Sí (automático) | Sí | No |
| Opciones largas (`--verbose`) | No | Sí | Sí |
| `--flag=value` | No | Sí | Sí (con `${1#*=}`) |
| Argumentos opcionales | No | Sí (`::`) | Manual |
| Manejo de `--` | Manual (`shift`) | Automático | Manual |
| Portabilidad | POSIX (todos los shells) | GNU only | Muy portable (cualquier shell) |
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

# En la práctica:
# - Scripts simples con solo -v -h -o → getopts
# - Scripts con --verbose --output → while/case manual
# - getopt tiene pocas ventajas sobre while/case manual
```

---

## Labs

### Parte 1 — getopts POSIX

#### Paso 1.1: getopts básico

```bash
cat > /tmp/getopts-basic.sh << 'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail

VERBOSE=0
OUTPUT=""

while getopts ":vho:" opt; do
    case $opt in
        v) VERBOSE=1 ;;
        o) OUTPUT="$OPTARG" ;;
        h) echo "Uso: $(basename "$0") [-v] [-o archivo] [-h] <input>"; exit 0 ;;
        \?) echo "Opción inválida: -$OPTARG" >&2; exit 1 ;;
        :)  echo "-$OPTARG requiere un argumento" >&2; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

echo "verbose=$VERBOSE"
echo "output=${OUTPUT:-stdout}"
echo "args restantes: $@"
SCRIPT
chmod +x /tmp/getopts-basic.sh

# Sin opciones:
/tmp/getopts-basic.sh archivo.txt

# Con opciones:
/tmp/getopts-basic.sh -v -o salida.txt archivo.txt

# Flags combinados (-vo = -v -o):
/tmp/getopts-basic.sh -vo salida.txt archivo.txt
```

#### Paso 1.2: Errores en getopts

Predice: ¿qué pasa con `-x` (opción inválida) y con `-o` sin argumento?

```bash
cat > /tmp/getopts-errors.sh << 'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail

while getopts ":vho:" opt; do
    case $opt in
        v) echo "  → flag -v activado" ;;
        h) echo "  → help" ;;
        o) echo "  → output: $OPTARG" ;;
        \?) echo "  → ERROR: opción inválida: -$OPTARG" >&2 ;;
        :)  echo "  → ERROR: -$OPTARG requiere argumento" >&2 ;;
    esac
done
SCRIPT
chmod +x /tmp/getopts-errors.sh

echo "--- Opción inválida ---"
/tmp/getopts-errors.sh -v -x -o out.txt 2>&1 || true

echo "--- Falta argumento ---"
/tmp/getopts-errors.sh -v -o 2>&1 || true
```

### Parte 2 — shift y opciones largas

#### Paso 2.1: shift explicado

```bash
cat > /tmp/shift-demo.sh << 'SCRIPT'
#!/bin/bash
echo "Antes del shift:"
echo "  \$# = $#, \$@ = $@"

shift
echo "Después de shift 1:"
echo "  \$# = $#, \$1 = $1, \$@ = $@"

shift 2
echo "Después de shift 2:"
echo "  \$# = $#, \$@ = $@"
SCRIPT
chmod +x /tmp/shift-demo.sh
/tmp/shift-demo.sh uno dos tres cuatro cinco
```

#### Paso 2.2: Opciones largas con while/case

```bash
cat > /tmp/long-opts.sh << 'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail

VERBOSE=0
OUTPUT=""
DRY_RUN=0
FILES=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -v|--verbose) VERBOSE=1; shift ;;
        -o|--output)
            [[ $# -lt 2 ]] && { echo "ERROR: --output requiere argumento" >&2; exit 1; }
            OUTPUT="$2"; shift 2
            ;;
        --output=*) OUTPUT="${1#*=}"; shift ;;
        -n|--dry-run) DRY_RUN=1; shift ;;
        -h|--help) echo "Uso: $(basename "$0") [-v] [-o FILE] [-n] [-h] archivos..."; exit 0 ;;
        --) shift; FILES+=("$@"); break ;;
        -*) echo "Opción desconocida: $1" >&2; exit 1 ;;
        *) FILES+=("$1"); shift ;;
    esac
done

echo "verbose=$VERBOSE output=${OUTPUT:-stdout} dry_run=$DRY_RUN"
echo "files=(${FILES[*]:-none})"
SCRIPT
chmod +x /tmp/long-opts.sh

# Opciones largas:
/tmp/long-opts.sh --verbose --output=report.txt data.csv

# Mezclar cortas y largas:
/tmp/long-opts.sh -v --output report.txt -n file1 file2

# Separador --:
/tmp/long-opts.sh -v -- --este-no-es-flag.txt
```

### Parte 3 — Validación y patrón completo

#### Paso 3.1: Validación de input

```bash
cat > /tmp/validate.sh << 'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

validate_port() {
    local port="$1"
    [[ "$port" =~ ^[0-9]+$ ]] || die "puerto inválido: $port (debe ser numérico)"
    (( port >= 1 && port <= 65535 )) || die "puerto fuera de rango: $port (1-65535)"
}

validate_file() {
    local file="$1"
    [[ -f "$file" ]] || die "archivo no encontrado: $file"
    [[ -r "$file" ]] || die "sin permiso de lectura: $file"
}

echo "--- Puerto válido ---"
validate_port 8080 && echo "8080: OK"

echo "--- Puerto inválido ---"
validate_port "abc" 2>&1 || true
validate_port 99999 2>&1 || true

echo "--- Archivo ---"
validate_file /etc/passwd && echo "/etc/passwd: OK"
validate_file /no/existe 2>&1 || true
SCRIPT
chmod +x /tmp/validate.sh
/tmp/validate.sh
```

#### Paso 3.2: Script profesional completo

```bash
cat > /tmp/professional.sh << 'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_NAME=$(basename "$0")
readonly VERSION="1.0.0"

VERBOSE=0
OUTPUT="/dev/stdout"
FORMAT="text"
FILES=()

die()  { echo "$SCRIPT_NAME: error: $*" >&2; exit 1; }
info() { (( VERBOSE )) && echo "$SCRIPT_NAME: $*" >&2 || true; }

usage() {
    cat << EOF
Uso: $SCRIPT_NAME [opciones] <archivo> [archivo...]

Opciones:
  -v, --verbose         Modo verbose
  -o, --output ARCHIVO  Archivo de salida (default: stdout)
  -f, --format FMT      Formato: text, json, csv (default: text)
  -h, --help            Mostrar ayuda
  -V, --version         Mostrar versión
EOF
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -v|--verbose)  VERBOSE=1; shift ;;
            -o|--output)   [[ $# -lt 2 ]] && die "--output requiere argumento"
                           OUTPUT="$2"; shift 2 ;;
            --output=*)    OUTPUT="${1#*=}"; shift ;;
            -f|--format)   [[ $# -lt 2 ]] && die "--format requiere argumento"
                           FORMAT="$2"; shift 2 ;;
            --format=*)    FORMAT="${1#*=}"; shift ;;
            -h|--help)     usage; exit 0 ;;
            -V|--version)  echo "$SCRIPT_NAME $VERSION"; exit 0 ;;
            --)            shift; FILES+=("$@"); break ;;
            -*)            die "opción desconocida: $1 (ver --help)" ;;
            *)             FILES+=("$1"); shift ;;
        esac
    done
}

validate() {
    (( ${#FILES[@]} > 0 )) || die "falta al menos un archivo (ver --help)"
    for f in "${FILES[@]}"; do
        [[ -f "$f" ]] || die "archivo no encontrado: $f"
        [[ -r "$f" ]] || die "sin permiso de lectura: $f"
    done
    case "$FORMAT" in
        text|json|csv) ;;
        *) die "formato inválido: $FORMAT (usar text, json, csv)" ;;
    esac
}

process() {
    info "Procesando ${#FILES[@]} archivo(s) en formato $FORMAT"
    for f in "${FILES[@]}"; do
        local lines
        lines=$(wc -l < "$f")
        info "  $f: $lines líneas"
        echo "$f: $lines líneas"
    done
}

parse_args "$@"
validate
process
SCRIPT
chmod +x /tmp/professional.sh

/tmp/professional.sh --help
/tmp/professional.sh -v /etc/passwd /etc/hostname
/tmp/professional.sh --version
/tmp/professional.sh 2>&1 || true                          # sin archivos
/tmp/professional.sh --format=xml /etc/passwd 2>&1 || true # formato inválido
```

### Limpieza

```bash
rm -f /tmp/getopts-basic.sh /tmp/getopts-errors.sh
rm -f /tmp/shift-demo.sh /tmp/long-opts.sh
rm -f /tmp/validate.sh /tmp/professional.sh
```

---

## Ejercicios

### Ejercicio 1 — getopts básico: OPTSTRING y OPTARG

Escribe un script con `getopts` que acepte `-c N` (count), `-u` (uppercase)
y `-h` (help), seguidos de un nombre posicional.

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

<details><summary>Predicción</summary>

- `./script.sh -c 3 -u mundo`:
  ```
  HOLA, MUNDO
  HOLA, MUNDO
  HOLA, MUNDO
  ```
  `-c 3` → COUNT=3, `-u` → UPPER=1, shift deja "mundo" como `$1`.
  `${MSG^^}` convierte a mayúsculas.

- `./script.sh mundo`:
  ```
  Hola, mundo
  ```
  Defaults: COUNT=1, UPPER=0.

- `./script.sh -c` → `-c requiere argumento` (caso `:` del OPTSTRING,
  `$OPTARG` = `c`).

- `./script.sh -x` → `Opción inválida: -x` (caso `\?`, `$OPTARG` = `x`).

</details>

### Ejercicio 2 — Modo verbose vs silencioso

Compara el comportamiento de getopts con y sin `:` al inicio del OPTSTRING.

```bash
#!/usr/bin/env bash

echo "=== Modo VERBOSE (sin : al inicio) ==="
# Nota: quitamos 'set -u' porque OPTARG estará unset en modo verbose
OPTIND=1
while getopts "vo:" opt; do
    case $opt in
        v) echo "  -v activado" ;;
        o) echo "  -o con argumento: $OPTARG" ;;
        \?) echo "  Caso \\?: OPTARG='${OPTARG:-<unset>}'" ;;
    esac
done

echo ""
echo "=== Modo SILENCIOSO (: al inicio) ==="
OPTIND=1
while getopts ":vo:" opt; do
    case $opt in
        v) echo "  -v activado" ;;
        o) echo "  -o con argumento: $OPTARG" ;;
        \?) echo "  Caso \\?: OPTARG='$OPTARG'" ;;
        :)  echo "  Caso :: OPTARG='$OPTARG'" ;;
    esac
done

# Ejecutar con: ./script.sh -v -x -o
```

<details><summary>Predicción</summary>

Con los argumentos `-v -x -o`:

**Modo VERBOSE** (OPTSTRING `"vo:"`):
```
  -v activado
./script.sh: illegal option -- x       ← getopts imprime esto
  Caso \?: OPTARG='<unset>'            ← OPTARG NO se llena
./script.sh: option requires an argument -- o  ← getopts imprime esto
  Caso \?: OPTARG='<unset>'            ← missing arg también da ? (no :)
```
En modo verbose, getopts maneja los mensajes de error. `$OPTARG` queda vacío
para ambos tipos de error. El caso `:)` nunca se ejecuta.

**Modo SILENCIOSO** (OPTSTRING `":vo:"`):
```
  -v activado
  Caso \?: OPTARG='x'                  ← OPTARG = la letra inválida
  Caso :: OPTARG='o'                   ← OPTARG = la letra sin argumento
```
En modo silencioso, el script tiene control total. `$OPTARG` se llena
correctamente y el caso `:)` sí funciona.

</details>

### Ejercicio 3 — shift y OPTIND

Predice el valor de `$1` después de cada `shift`.

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "Argumentos originales: $@"
echo "\$1=$1, \$2=$2, \$3=$3, \$4=$4, \$5=$5"

while getopts ":vf:" opt; do
    case $opt in
        v) echo "  -v activado" ;;
        f) echo "  -f = $OPTARG" ;;
        \?) echo "  inválida: -$OPTARG" >&2; exit 1 ;;
    esac
done

echo "OPTIND=$OPTIND"
shift $((OPTIND - 1))
echo "Después del shift: \$1=${1:-<vacío>}, \$2=${2:-<vacío>}"
echo "Argumentos restantes: $@"

# Ejecutar con: ./script.sh -v -f config.txt arg1 arg2
```

<details><summary>Predicción</summary>

Con `-v -f config.txt arg1 arg2`:

```
Argumentos originales: -v -f config.txt arg1 arg2
$1=-v, $2=-f, $3=config.txt, $4=arg1, $5=arg2
  -v activado
  -f = config.txt
OPTIND=4
Después del shift: $1=arg1, $2=arg2
Argumentos restantes: arg1 arg2
```

`OPTIND=4` porque: `$1`=-v (procesado), `$2`=-f (procesado), `$3`=config.txt
(argumento de -f, procesado), `$4`=arg1 (primer NO-opción → OPTIND apunta
aquí). `shift $((4-1))` = `shift 3` elimina los 3 primeros, dejando arg1 arg2.

</details>

### Ejercicio 4 — Opciones largas: while/case

Escribe un parser de opciones largas con `--target=DIR`, `--dry-run`, y `--`.

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
# ./script.sh --target=/var/log *.txt
```

<details><summary>Predicción</summary>

- `--dry-run --target=/tmp file1 file2`:
  ```
  dry_run=1 target=/tmp files=(file1 file2)
  ```
  `--target=/tmp` matchea `--target=*`, `${1#*=}` extrae `/tmp`.

- `-n -t /tmp -- --weird-filename`:
  ```
  dry_run=1 target=/tmp files=(--weird-filename)
  ```
  `-n` = dry-run. `-t /tmp` consume 2 argumentos (shift 2). `--` hace break.
  `--weird-filename` entra como archivo, no como opción.

- `--target=/var/log *.txt`: el shell expande `*.txt` antes de que el script
  lo vea. Cada archivo matchea `*)` y se agrega a FILES.

</details>

### Ejercicio 5 — Validación de argumentos

Agrega validación al parser del ejercicio anterior.

```bash
#!/usr/bin/env bash
set -euo pipefail

die() { echo "$(basename "$0"): error: $*" >&2; exit 1; }

DRY_RUN=0
TARGET=""
FILES=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -n|--dry-run) DRY_RUN=1; shift ;;
        -t|--target)
            [[ $# -lt 2 ]] && die "--target requiere un argumento"
            TARGET="$2"; shift 2
            ;;
        --target=*)   TARGET="${1#*=}"; shift ;;
        -h|--help)    echo "Uso: $0 [-n] [-t DIR] archivos..."; exit 0 ;;
        --)           shift; FILES+=("$@"); break ;;
        -*)           die "opción desconocida: $1" ;;
        *)            FILES+=("$1"); shift ;;
    esac
done

# Validación
[[ ${#FILES[@]} -gt 0 ]]              || die "falta al menos un archivo"
[[ -z "$TARGET" || -d "$TARGET" ]]    || die "$TARGET no es un directorio"
for f in "${FILES[@]}"; do
    [[ -f "$f" && -r "$f" ]]          || die "$f no existe o no es legible"
done

echo "Todo válido: dry_run=$DRY_RUN target=${TARGET:-<ninguno>} files=(${FILES[*]})"

# Probar:
# ./script.sh -t /tmp /etc/passwd /etc/hostname    → OK
# ./script.sh -t /noexiste /etc/passwd             → error target
# ./script.sh                                       → error: falta archivo
# ./script.sh /noexiste                             → error: archivo no existe
```

<details><summary>Predicción</summary>

- `-t /tmp /etc/passwd /etc/hostname` → `Todo válido: dry_run=0 target=/tmp files=(/etc/passwd /etc/hostname)`. `/tmp` existe, ambos archivos existen y son legibles.

- `-t /noexiste /etc/passwd` → `error: /noexiste no es un directorio`. La validación de TARGET falla antes de llegar a los archivos.

- Sin argumentos → `error: falta al menos un archivo`. FILES está vacío.

- `/noexiste` → `error: /noexiste no existe o no es legible`. El archivo no existe.

</details>

### Ejercicio 6 — Flags combinados con getopts

Observa cómo getopts maneja automáticamente flags combinados.

```bash
#!/usr/bin/env bash
set -euo pipefail

VERBOSE=0
FORCE=0
RECURSIVE=0
OUTPUT=""

while getopts ":vfro:" opt; do
    case $opt in
        v) VERBOSE=1 ;;
        f) FORCE=1 ;;
        r) RECURSIVE=1 ;;
        o) OUTPUT="$OPTARG" ;;
        \?) echo "Opción inválida: -$OPTARG" >&2; exit 1 ;;
        :)  echo "-$OPTARG requiere argumento" >&2; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

echo "v=$VERBOSE f=$FORCE r=$RECURSIVE o=${OUTPUT:-<none>} args=$*"

# Probar estas 4 invocaciones y predecir si producen el mismo resultado:
# ./script.sh -v -f -r -o out.txt file.txt
# ./script.sh -vfr -o out.txt file.txt
# ./script.sh -vfro out.txt file.txt
# ./script.sh -vrfo out.txt file.txt
```

<details><summary>Predicción</summary>

Las 4 invocaciones producen **exactamente el mismo resultado**:
```
v=1 f=1 r=1 o=out.txt args=file.txt
```

getopts maneja flags combinados automáticamente:
- `-vfr` = `-v -f -r` (tres flags sin argumento)
- `-vfro out.txt` = `-v -f -r -o out.txt` (el último flag `o` requiere
  argumento, que se toma del siguiente token)
- `-vrfo out.txt` = misma lógica, diferente orden de v, r, f

La clave: cuando getopts encuentra una opción con `:` (como `o:`) dentro de
un grupo combinado, toma el **resto del grupo** como argumento si hay más
caracteres, o el **siguiente token** si es el último. Ejemplo:
- `-vfoOUT` → v=1, f=1, o=OUT (el "OUT" restante es el argumento de -o)

</details>

### Ejercicio 7 — El separador `--`

```bash
#!/usr/bin/env bash
set -euo pipefail

VERBOSE=0
FILES=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -v|--verbose) VERBOSE=1; shift ;;
        --) shift; FILES+=("$@"); break ;;
        -*) echo "Opción desconocida: $1" >&2; exit 1 ;;
        *) FILES+=("$1"); shift ;;
    esac
done

echo "verbose=$VERBOSE"
printf "archivo: %s\n" "${FILES[@]}"

# Predice la diferencia entre estas invocaciones:
# ./script.sh -v file1.txt file2.txt
# ./script.sh -v -- -file-with-dash.txt
# ./script.sh -- -v file1.txt
```

<details><summary>Predicción</summary>

- `./script.sh -v file1.txt file2.txt`:
  ```
  verbose=1
  archivo: file1.txt
  archivo: file2.txt
  ```
  `-v` es opción, el resto son archivos.

- `./script.sh -v -- -file-with-dash.txt`:
  ```
  verbose=1
  archivo: -file-with-dash.txt
  ```
  `-v` es opción, `--` señala fin de opciones, `-file-with-dash.txt` se
  trata como archivo (no como opción inválida).

- `./script.sh -- -v file1.txt`:
  ```
  verbose=0
  archivo: -v
  archivo: file1.txt
  ```
  `--` aparece antes de `-v`, así que TODO lo que sigue (incluyendo `-v`)
  se toma como archivo. verbose queda en 0.

</details>

### Ejercicio 8 — Calculadora CLI

```bash
#!/usr/bin/env bash
set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

OPERATION="add"
PRECISION=2

usage() {
    cat << EOF
Uso: $(basename "$0") [-o OP] [-p N] NUM1 NUM2

Operaciones: add, sub, mul, div (default: add)
-p N: decimales de precisión (default: 2)
EOF
}

while getopts ":o:p:h" opt; do
    case $opt in
        o) OPERATION="$OPTARG" ;;
        p) PRECISION="$OPTARG" ;;
        h) usage; exit 0 ;;
        \?) die "opción inválida: -$OPTARG" ;;
        :)  die "-$OPTARG requiere argumento" ;;
    esac
done
shift $((OPTIND - 1))

[[ $# -ge 2 ]] || die "se requieren 2 números (ver -h)"

[[ "$1" =~ ^-?[0-9]+\.?[0-9]*$ ]] || die "'$1' no es un número"
[[ "$2" =~ ^-?[0-9]+\.?[0-9]*$ ]] || die "'$2' no es un número"

A="$1" B="$2"

case "$OPERATION" in
    add) RESULT=$(echo "scale=$PRECISION; $A + $B" | bc) ;;
    sub) RESULT=$(echo "scale=$PRECISION; $A - $B" | bc) ;;
    mul) RESULT=$(echo "scale=$PRECISION; $A * $B" | bc) ;;
    div) [[ "$B" != "0" ]] || die "división por cero"
         RESULT=$(echo "scale=$PRECISION; $A / $B" | bc) ;;
    *)   die "operación '$OPERATION' no válida (usar add|sub|mul|div)" ;;
esac

echo "$RESULT"

# Probar:
# ./calc.sh 10 5             → 15
# ./calc.sh -o mul 3 4       → 12
# ./calc.sh -o div -p 4 10 3 → 3.3333
# ./calc.sh -o div 10 0      → error: división por cero
```

<details><summary>Predicción</summary>

- `./calc.sh 10 5` → `15` (operación default: add, scale=2, sin decimales
  porque la suma es entera).

- `./calc.sh -o mul 3 4` → `12` (3 × 4).

- `./calc.sh -o div -p 4 10 3` → `3.3333` (10 ÷ 3 con 4 decimales).

- `./calc.sh -o div 10 0` → `error: división por cero` (la validación
  `[[ "$B" != "0" ]]` falla antes de llegar a `bc`).

Nota: se usa `getopts` en vez de while/case porque las opciones son solo
cortas (`-o`, `-p`). Las opciones van ANTES de los números posicionales.

</details>

### Ejercicio 9 — Wrapper seguro de comandos

Ejecuta un comando con opción de dry-run, pero sin usar `eval`.

```bash
#!/usr/bin/env bash
set -euo pipefail

DRY_RUN=0
VERBOSE=0
CMD=()

usage() {
    cat << EOF
Uso: $(basename "$0") [-n] [-v] [-h] -- COMANDO [ARGS...]

  -n  Dry-run (muestra sin ejecutar)
  -v  Verbose (muestra antes de ejecutar)
  -h  Ayuda
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -n|--dry-run) DRY_RUN=1; shift ;;
        -v|--verbose) VERBOSE=1; shift ;;
        -h|--help)    usage; exit 0 ;;
        --)           shift; CMD=("$@"); break ;;
        -*)           echo "Opción desconocida: $1" >&2; exit 1 ;;
        *)            echo "Usar -- para separar opciones del comando" >&2; exit 1 ;;
    esac
done

[[ ${#CMD[@]} -gt 0 ]] || { usage; exit 1; }

if (( DRY_RUN )); then
    echo "[DRY-RUN] ${CMD[*]}"
else
    (( VERBOSE )) && echo "[EXEC] ${CMD[*]}" >&2
    "${CMD[@]}"    # ejecuta preservando fronteras de argumentos
fi

# Probar:
# ./run.sh -n -- ls -la /tmp
# ./run.sh -v -- echo "hello world"
# ./run.sh -- ls -la /tmp
```

<details><summary>Predicción</summary>

- `./run.sh -n -- ls -la /tmp`:
  ```
  [DRY-RUN] ls -la /tmp
  ```
  Muestra el comando sin ejecutarlo.

- `./run.sh -v -- echo "hello world"`:
  ```
  [EXEC] echo hello world       ← en stderr
  hello world                    ← en stdout
  ```
  Verbose muestra el comando en stderr, luego lo ejecuta. `"${CMD[@]}"`
  preserva las fronteras: `CMD=("echo" "hello world")`, así que `echo`
  recibe un solo argumento "hello world".

- `./run.sh -- ls -la /tmp`: ejecuta `ls -la /tmp` sin output adicional.

**Punto clave**: `"${CMD[@]}"` (array) es seguro. `eval "$COMMAND"` (string)
es peligroso porque re-interpreta metacaracteres del shell.

</details>

### Ejercicio 10 — Panorama: script profesional con getopts + while/case

Un script que combina getopts para opciones cortas y validación completa.

```bash
#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_NAME=$(basename "$0")
readonly VERSION="1.0.0"

# --- Defaults ---
VERBOSE=0
OUTPUT=""
FORMAT="text"
FILES=()

# --- Utilidades ---
die()  { echo "$SCRIPT_NAME: error: $*" >&2; exit 1; }
info() { (( VERBOSE )) && echo "$SCRIPT_NAME: $*" >&2 || true; }

usage() {
    cat << EOF
Uso: $SCRIPT_NAME [opciones] <archivo> [archivo...]

Analiza archivos de texto y genera estadísticas.

Opciones:
  -v          Modo verbose
  -o ARCHIVO  Archivo de salida (default: stdout)
  -f FORMATO  Formato: text, csv (default: text)
  -h          Mostrar esta ayuda
  -V          Mostrar versión

Ejemplos:
  $SCRIPT_NAME -v /etc/passwd
  $SCRIPT_NAME -f csv -o stats.csv *.txt
EOF
}

# --- Parsing con getopts ---
while getopts ":vo:f:hV" opt; do
    case $opt in
        v) VERBOSE=1 ;;
        o) OUTPUT="$OPTARG" ;;
        f) FORMAT="$OPTARG" ;;
        h) usage; exit 0 ;;
        V) echo "$SCRIPT_NAME $VERSION"; exit 0 ;;
        \?) die "opción inválida: -$OPTARG (ver -h)" ;;
        :)  die "-$OPTARG requiere un argumento" ;;
    esac
done
shift $((OPTIND - 1))

# --- Argumentos posicionales ---
FILES=("$@")

# --- Validación ---
[[ ${#FILES[@]} -gt 0 ]] || die "falta al menos un archivo (ver -h)"

for f in "${FILES[@]}"; do
    [[ -f "$f" ]] || die "archivo no encontrado: $f"
    [[ -r "$f" ]] || die "sin permiso de lectura: $f"
done

case "$FORMAT" in
    text|csv) ;;
    *) die "formato inválido: $FORMAT (usar text o csv)" ;;
esac

# --- Procesamiento ---
info "Procesando ${#FILES[@]} archivo(s) en formato $FORMAT"

report() {
    local file="$1"
    local lines words chars
    read -r lines words chars _ < <(wc "$file")
    info "  Analizando: $file"

    case "$FORMAT" in
        text) printf "%-30s  %6d líneas  %8d palabras  %10d bytes\n" \
                  "$file" "$lines" "$words" "$chars" ;;
        csv)  echo "$file,$lines,$words,$chars" ;;
    esac
}

{
    [[ "$FORMAT" == "csv" ]] && echo "archivo,líneas,palabras,bytes"
    for f in "${FILES[@]}"; do
        report "$f"
    done
} > "${OUTPUT:-/dev/stdout}"

info "Resultado escrito en ${OUTPUT:-stdout}"

# Probar:
# ./stats.sh -v /etc/passwd /etc/hostname
# ./stats.sh -f csv -o /tmp/stats.csv /etc/passwd /etc/group
# ./stats.sh -V
# ./stats.sh -h
# ./stats.sh                  → error: falta archivo
# ./stats.sh -f xml /etc/passwd → error: formato inválido
```

<details><summary>Predicción</summary>

- `./stats.sh -v /etc/passwd /etc/hostname`:
  ```
  stats.sh: Procesando 2 archivo(s) en formato text          ← stderr (verbose)
  stats.sh:   Analizando: /etc/passwd                        ← stderr
  /etc/passwd                      N líneas    M palabras     K bytes
  stats.sh:   Analizando: /etc/hostname                      ← stderr
  /etc/hostname                    1 líneas    1 palabras     L bytes
  stats.sh: Resultado escrito en stdout                      ← stderr
  ```
  Los mensajes `info` van a stderr, las estadísticas a stdout. Los valores
  N, M, K, L dependen del sistema.

- `./stats.sh -f csv -o /tmp/stats.csv /etc/passwd /etc/group`:
  No produce output en terminal (va a archivo). El archivo contiene:
  ```
  archivo,líneas,palabras,bytes
  /etc/passwd,N,M,K
  /etc/group,N2,M2,K2
  ```

- `./stats.sh -V` → `stats.sh 1.0.0`

- `./stats.sh` → `stats.sh: error: falta al menos un archivo (ver -h)`

- `./stats.sh -f xml /etc/passwd` → `stats.sh: error: formato inválido: xml (usar text o csv)`

</details>
