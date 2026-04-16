# getopts y parsing de argumentos

## 1. El problema: interfaces de línea de comandos

Todo script no-trivial necesita aceptar opciones. Sin un parser, el código
se vuelve frágil rápidamente:

```bash
# Parsing manual (frágil, no escala):
if [[ "$1" == "-v" ]]; then
    verbose=true
    shift
fi
file="$1"
# ¿Qué pasa con -v -o output.txt file.txt?
# ¿Y si el usuario escribe -vo?
# ¿Y si olvida un argumento?
```

Bash ofrece `getopts` (builtin) para parsing robusto de flags cortos. Para
flags largos (`--verbose`) hay que extender manualmente o usar herramientas
externas.

---

## 2. getopts: sintaxis básica

```bash
while getopts "vf:o:" opt; do
    case $opt in
        v) verbose=true ;;
        f) file="$OPTARG" ;;
        o) output="$OPTARG" ;;
        ?) usage; exit 1 ;;
    esac
done
shift $((OPTIND - 1))
# "$@" ahora contiene solo los argumentos posicionales (sin flags)
```

### Anatomía del optstring

```
"vf:o:"
 │││ │
 ││└─┘── o: → -o requiere argumento (OPTARG)
 │└───── f: → -f requiere argumento (OPTARG)
 └────── v  → -v es flag booleano (sin argumento)

Los dos puntos después de una letra = "requiere argumento"
Sin dos puntos = flag booleano (on/off)
```

### Variables especiales

| Variable | Contenido |
|----------|----------|
| `$opt` | La letra de la opción actual (`v`, `f`, `o`) |
| `$OPTARG` | El argumento de la opción (solo si tiene `:`) |
| `$OPTIND` | Índice del próximo argumento a procesar |
| `?` | Se asigna a `$opt` cuando la opción es desconocida |

---

## 3. Ejemplo completo: script de backup

```bash
#!/bin/bash
set -euo pipefail

# === Defaults ===
VERBOSE=false
COMPRESS=false
EXCLUDE=""
OUTPUT_DIR="./backup"
MAX_SIZE=""

# === Usage ===
usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS] SOURCE...

Backup files to a directory.

Options:
    -o DIR    Output directory (default: ./backup)
    -c        Compress with gzip
    -e PAT    Exclude pattern (can be repeated)
    -m SIZE   Max file size (e.g., 10M)
    -v        Verbose output
    -h        Show this help

Examples:
    $(basename "$0") -cv -o /tmp/bak /etc
    $(basename "$0") -e '*.log' -e '*.tmp' -o /backup /var
EOF
}

# === Parse options ===
EXCLUDES=()

while getopts ":vcho:e:m:" opt; do
    case $opt in
        v) VERBOSE=true ;;
        c) COMPRESS=true ;;
        o) OUTPUT_DIR="$OPTARG" ;;
        e) EXCLUDES+=("$OPTARG") ;;
        m) MAX_SIZE="$OPTARG" ;;
        h) usage; exit 0 ;;
        :)
            echo "Error: -$OPTARG requires an argument" >&2
            usage >&2
            exit 1
            ;;
        ?)
            echo "Error: unknown option -$OPTARG" >&2
            usage >&2
            exit 1
            ;;
    esac
done
shift $((OPTIND - 1))

# === Validate positional args ===
if (( $# == 0 )); then
    echo "Error: at least one SOURCE required" >&2
    usage >&2
    exit 1
fi

# === Main logic ===
$VERBOSE && echo "Output: $OUTPUT_DIR"
$VERBOSE && echo "Compress: $COMPRESS"
$VERBOSE && echo "Sources: $*"

mkdir -p "$OUTPUT_DIR"

for src in "$@"; do
    $VERBOSE && echo "Backing up: $src"
    # ... backup logic ...
done
```

```bash
# Invocaciones válidas:
./backup.sh -v -c -o /tmp/bak /etc /var
./backup.sh -vc -o /tmp/bak /etc          # flags combinados: -vc = -v -c
./backup.sh -e '*.log' -e '*.tmp' /var    # -e repetido
./backup.sh -h                              # help
./backup.sh /etc                            # solo defaults
```

---

## 4. El optstring en detalle

### 4.1 Dos puntos al inicio: silent mode

```bash
# Sin : al inicio (default mode):
getopts "vf:" opt
# Opción desconocida → Bash imprime error a stderr + opt='?'
# Falta argumento   → Bash imprime error a stderr + opt='?'

# Con : al inicio (silent mode):
getopts ":vf:" opt
# Opción desconocida → opt='?', OPTARG=la letra desconocida
# Falta argumento   → opt=':', OPTARG=la letra que falta argumento
# Sin mensajes automáticos → TÚ controlas los mensajes de error
```

**Preferir silent mode** (`:` al inicio): permite mensajes de error
personalizados:

```bash
while getopts ":vf:o:" opt; do
    case $opt in
        v) verbose=true ;;
        f) file="$OPTARG" ;;
        o) output="$OPTARG" ;;
        :)
            echo "Error: -$OPTARG requires an argument" >&2
            exit 1
            ;;
        ?)
            echo "Error: unknown option -$OPTARG" >&2
            exit 1
            ;;
    esac
done
```

### 4.2 Flags combinados

`getopts` soporta flags combinados automáticamente:

```bash
# Todas estas son equivalentes:
./script.sh -v -c -f output.txt
./script.sh -vc -f output.txt
./script.sh -vcf output.txt      # -f "consume" el argumento
```

```
-vcf output.txt se parsea como:
  -v           → flag booleano
  -c           → flag booleano
  -f output.txt → opción con argumento
```

---

## 5. shift y OPTIND

Después de `getopts`, los argumentos posicionales aún contienen las flags.
`shift $((OPTIND - 1))` los elimina:

```bash
# Antes de shift:
# $1="-v" $2="-o" $3="/tmp" $4="file1" $5="file2"
# OPTIND=4 (siguiente argumento a procesar sería $4)

shift $((OPTIND - 1))

# Después de shift:
# $1="file1" $2="file2"
# "$@" ahora solo contiene argumentos posicionales
```

```
./script.sh -v -o /tmp file1.txt file2.txt

Antes de getopts:
$1    $2   $3     $4        $5
-v    -o   /tmp   file1.txt file2.txt

getopts procesa: -v, -o /tmp
OPTIND = 4 (apunta a file1.txt)

shift $((OPTIND - 1)) = shift 3

Después de shift:
$1        $2
file1.txt file2.txt
```

---

## 6. Opciones repetibles

Algunas opciones se pueden repetir (`-e pattern` múltiples veces). Usa un
array para acumularlas:

```bash
EXCLUDES=()
INCLUDES=()

while getopts ":e:i:" opt; do
    case $opt in
        e) EXCLUDES+=("$OPTARG") ;;
        i) INCLUDES+=("$OPTARG") ;;
        ?) echo "Unknown: -$OPTARG" >&2; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

echo "Excludes: ${EXCLUDES[*]}"
echo "Includes: ${INCLUDES[*]}"
```

```bash
./script.sh -e '*.log' -e '*.tmp' -i '*.conf' file.txt
# Excludes: *.log *.tmp
# Includes: *.conf
```

---

## 7. Parsing de flags largos (--verbose)

`getopts` no soporta flags largos. Tres enfoques:

### 7.1 Manual con while-case

```bash
VERBOSE=false
OUTPUT=""
FILES=()

while (( $# > 0 )); do
    case "$1" in
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -o|--output)
            if [[ -z "${2:-}" ]]; then
                echo "Error: $1 requires an argument" >&2
                exit 1
            fi
            OUTPUT="$2"
            shift 2
            ;;
        --output=*)
            OUTPUT="${1#*=}"    # extraer valor después del =
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            FILES+=("$@")      # todo después de -- son archivos
            break
            ;;
        -*)
            echo "Error: unknown option $1" >&2
            exit 1
            ;;
        *)
            FILES+=("$1")
            shift
            ;;
    esac
done
```

```bash
# Todas estas funcionan:
./script.sh --verbose --output /tmp file.txt
./script.sh --output=/tmp file.txt
./script.sh -v -o /tmp file.txt
./script.sh -- -file-starting-with-dash.txt
```

### 7.2 Híbrido: getopts para cortos + case para largos

```bash
# Convertir flags largos a cortos antes de getopts
args=()
while (( $# > 0 )); do
    case "$1" in
        --verbose)   args+=("-v") ;;
        --output)    args+=("-o" "$2"); shift ;;
        --output=*)  args+=("-o" "${1#*=}") ;;
        --compress)  args+=("-c") ;;
        --help)      args+=("-h") ;;
        --)          args+=("--" "$@"); break ;;
        *)           args+=("$1") ;;
    esac
    shift
done
set -- "${args[@]}"    # reemplazar $@ con los args convertidos

# Ahora usar getopts normal
while getopts ":vco:h" opt; do
    # ...
done
```

### 7.3 getopt externo (GNU)

```bash
# GNU getopt (no confundir con getopts builtin)
# Disponible en Linux, no en macOS por defecto
OPTS=$(getopt -o vco:h --long verbose,compress,output:,help -n "$0" -- "$@") || exit 1
eval set -- "$OPTS"

while true; do
    case "$1" in
        -v|--verbose)  VERBOSE=true; shift ;;
        -c|--compress) COMPRESS=true; shift ;;
        -o|--output)   OUTPUT="$2"; shift 2 ;;
        -h|--help)     usage; exit 0 ;;
        --)            shift; break ;;
    esac
done
```

| Método | Flags cortos | Flags largos | Combinados (-vc) | Portabilidad |
|--------|-------------|-------------|-----------------|-------------|
| `getopts` builtin | ✓ | ✗ | ✓ | POSIX |
| Manual while-case | ✓ | ✓ | ✗ | Universal |
| Híbrido | ✓ | ✓ | ✓ | Universal |
| GNU `getopt` | ✓ | ✓ | ✓ | Solo Linux |

---

## 8. Validación de argumentos

### 8.1 Verificar que argumentos existen

```bash
while getopts ":f:n:" opt; do
    case $opt in
        f) FILE="$OPTARG" ;;
        n) COUNT="$OPTARG" ;;
        :) echo "Error: -$OPTARG requires argument" >&2; exit 1 ;;
        ?) echo "Error: unknown -$OPTARG" >&2; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

# Validar obligatorios
: "${FILE:?Error: -f FILE is required}"

# Validar tipo
if [[ -n "${COUNT:-}" ]] && ! [[ "$COUNT" =~ ^[0-9]+$ ]]; then
    echo "Error: -n must be a number, got '$COUNT'" >&2
    exit 1
fi

# Validar que archivo existe
if [[ ! -f "$FILE" ]]; then
    echo "Error: file not found: $FILE" >&2
    exit 1
fi

# Validar positional args
if (( $# < 1 )); then
    echo "Error: at least one argument required" >&2
    usage >&2
    exit 1
fi
```

### 8.2 Validadores reutilizables

```bash
require_arg() {
    local name="$1" value="$2"
    if [[ -z "$value" ]]; then
        echo "Error: $name is required" >&2
        exit 1
    fi
}

require_file() {
    local path="$1"
    if [[ ! -f "$path" ]]; then
        echo "Error: file not found: $path" >&2
        exit 1
    fi
}

require_dir() {
    local path="$1"
    if [[ ! -d "$path" ]]; then
        echo "Error: directory not found: $path" >&2
        exit 1
    fi
}

require_number() {
    local name="$1" value="$2"
    if ! [[ "$value" =~ ^[0-9]+$ ]]; then
        echo "Error: $name must be a number, got '$value'" >&2
        exit 1
    fi
}
```

---

## 9. Función usage

### 9.1 Estructura estándar

```bash
usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS] FILE...

Description of what the script does in one line.

Options:
    -o DIR      Output directory (default: .)
    -n COUNT    Number of iterations (default: 1)
    -f FORMAT   Output format: json, csv, text (default: text)
    -v          Verbose output
    -q          Quiet mode (suppress non-error output)
    -h          Show this help message

Arguments:
    FILE        One or more input files to process

Examples:
    $(basename "$0") -v -o /tmp data.csv
    $(basename "$0") -n 5 -f json file1.txt file2.txt
    $(basename "$0") -q *.log

Environment:
    MY_CONFIG   Path to config file (default: ~/.myapprc)
EOF
}
```

### 9.2 Convenciones

```
Estructura de usage:
1. Usage line: nombre + [OPTIONS] + argumentos posicionales
2. Descripción breve (una línea)
3. Options: alineadas, con defaults documentados
4. Arguments: qué son los posicionales
5. Examples: 2-3 invocaciones comunes
6. Environment: variables de entorno relevantes (opcional)
```

### 9.3 Mostrar usage en contexto

```bash
# En error de argumentos: usage a stderr + exit 1
echo "Error: missing file" >&2
usage >&2
exit 1

# En -h/--help: usage a stdout + exit 0
usage
exit 0
```

La diferencia importa: `script -h | less` funciona (stdout), pero los
errores van a stderr para no contaminar pipes.

---

## 10. -- (double dash): fin de opciones

`--` señala que todo lo que sigue son argumentos posicionales, no flags:

```bash
# Sin --:
./script.sh -v -file.txt    # ¿"-file.txt" es flag -f con arg "ile.txt"?

# Con --:
./script.sh -v -- -file.txt  # -file.txt es un ARCHIVO, no una flag
```

```bash
# Implementar en getopts: getopts lo maneja automáticamente
# Implementar en manual parsing:
while (( $# > 0 )); do
    case "$1" in
        --)
            shift
            break       # salir del loop, "$@" tiene el resto
            ;;
        -*)
            # procesar flag...
            ;;
        *)
            break       # primer no-flag → fin de opciones
            ;;
    esac
done
# "$@" ahora solo tiene argumentos posicionales
```

Esto es especialmente importante para scripts que procesan archivos cuyos
nombres pueden empezar con `-`:

```bash
# Crear archivo con nombre problemático:
touch -- -rf

# Borrar sin --: ¡PELIGRO!
rm -rf           # rm interpreta -rf como flags

# Borrar con --: seguro
rm -- -rf        # rm sabe que -rf es un nombre de archivo
```

---

## 11. Patrón completo: script de producción

```bash
#!/bin/bash
set -euo pipefail

# === Constants ===
readonly SCRIPT_NAME="$(basename "$0")"
readonly VERSION="1.2.0"

# === Defaults ===
VERBOSE=false
DRY_RUN=false
FORMAT="text"
OUTPUT="-"    # - = stdout

# === Functions ===
usage() {
    cat <<EOF
Usage: $SCRIPT_NAME [OPTIONS] FILE...

Process log files and generate reports.

Options:
    -f FORMAT   Output format: text, json, csv (default: text)
    -o FILE     Output file (default: stdout)
    -n          Dry run (show what would be done)
    -v          Verbose output
    -V          Show version
    -h          Show this help

Examples:
    $SCRIPT_NAME -f json -o report.json access.log
    $SCRIPT_NAME -v -n *.log
EOF
}

log() {
    $VERBOSE && echo "[INFO] $*" >&2
}

die() {
    echo "$SCRIPT_NAME: error: $*" >&2
    exit 1
}

# === Parse options ===
while getopts ":f:o:nvVh" opt; do
    case $opt in
        f) FORMAT="$OPTARG" ;;
        o) OUTPUT="$OPTARG" ;;
        n) DRY_RUN=true ;;
        v) VERBOSE=true ;;
        V) echo "$SCRIPT_NAME $VERSION"; exit 0 ;;
        h) usage; exit 0 ;;
        :) die "-$OPTARG requires an argument" ;;
        ?) die "unknown option -$OPTARG" ;;
    esac
done
shift $((OPTIND - 1))

# === Validate ===
(( $# >= 1 )) || die "at least one FILE required (see -h for help)"

case "$FORMAT" in
    text|json|csv) ;;
    *) die "invalid format '$FORMAT' (must be text, json, or csv)" ;;
esac

for f in "$@"; do
    [[ -f "$f" ]] || die "file not found: $f"
done

# === Main ===
log "Format: $FORMAT"
log "Output: $OUTPUT"
log "Dry run: $DRY_RUN"
log "Files: $*"

for file in "$@"; do
    log "Processing: $file"
    if $DRY_RUN; then
        echo "[DRY RUN] Would process: $file"
        continue
    fi
    # ... procesamiento real ...
done
```

---

## 12. Subcomandos (estilo git)

Algunos scripts tienen subcomandos: `./app.sh deploy ...`, `./app.sh status`:

```bash
#!/bin/bash
set -euo pipefail

SCRIPT_NAME="$(basename "$0")"

usage() {
    cat <<EOF
Usage: $SCRIPT_NAME <command> [OPTIONS] [ARGS]

Commands:
    deploy      Deploy the application
    status      Show current status
    rollback    Rollback to previous version
    help        Show help for a command

Run '$SCRIPT_NAME help <command>' for command-specific help.
EOF
}

cmd_deploy() {
    local env="staging"
    local tag="latest"

    while getopts ":e:t:h" opt; do
        case $opt in
            e) env="$OPTARG" ;;
            t) tag="$OPTARG" ;;
            h) echo "Usage: $SCRIPT_NAME deploy [-e ENV] [-t TAG]"; exit 0 ;;
            :) echo "Error: -$OPTARG requires argument" >&2; exit 1 ;;
            ?) echo "Error: unknown -$OPTARG" >&2; exit 1 ;;
        esac
    done
    shift $((OPTIND - 1))

    echo "Deploying tag=$tag to env=$env"
}

cmd_status() {
    echo "Current status: running"
}

cmd_rollback() {
    local steps=1
    while getopts ":n:" opt; do
        case $opt in
            n) steps="$OPTARG" ;;
        esac
    done
    echo "Rolling back $steps version(s)"
}

# === Dispatch ===
command="${1:-}"
shift || true

case "$command" in
    deploy)   cmd_deploy "$@" ;;
    status)   cmd_status "$@" ;;
    rollback) cmd_rollback "$@" ;;
    help)
        if (( $# > 0 )) && declare -f "cmd_$1" > /dev/null 2>&1; then
            "cmd_$1" -h
        else
            usage
        fi
        ;;
    ""|*)
        usage >&2
        exit 1
        ;;
esac
```

```bash
./app.sh deploy -e production -t v2.1
./app.sh status
./app.sh rollback -n 3
./app.sh help deploy
```

**Detalle importante**: cada subcomando tiene su propio `getopts` con su
propio `OPTIND`. Antes de llamar a `getopts` en un subcomando, resetear
`OPTIND`:

```bash
cmd_deploy() {
    OPTIND=1    # reset para que getopts empiece desde $1 del subcomando
    while getopts ":e:t:" opt; do
        # ...
    done
}
```

---

## 13. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| Olvidar `shift $((OPTIND-1))` | `$@` aún contiene las flags | Siempre shift después de getopts |
| Sin `:` al inicio del optstring | Bash imprime errores genéricos | Usar `":..."` para mensajes custom |
| `getopts` con `--long` | No soportado, se ignora | Manual parsing o GNU getopt |
| No resetear OPTIND en subcomandos | getopts no procesa args del subcomando | `OPTIND=1` al inicio del subcomando |
| `-o` sin argumento no detectado | Sin `:` en optstring después de `o` | `"o:"` — el `:` indica que requiere arg |
| usage a stdout en errores | Contamina pipes (`script 2>&1 \| less` funciona raro) | `usage >&2` en errores, `usage` en -h |
| No validar valores de opciones | `-f banana` aceptado cuando solo text/json/csv | `case` para validar valores permitidos |
| `$OPTARG` sin comillas | Word splitting si el argumento tiene espacios | `"$OPTARG"` siempre |
| No manejar `--` | Archivos con `-` en nombre causan errores | getopts maneja `--` automáticamente |
| Flags obligatorios sin verificar | Script continúa sin datos necesarios | `${VAR:?msg}` o verificación explícita post-parse |

---

## 14. Ejercicios

### Ejercicio 1 — getopts básico

**Predicción**: ¿qué valores tienen las variables después del parsing?

```bash
verbose=false
output=""
count=1

while getopts ":vo:n:" opt; do
    case $opt in
        v) verbose=true ;;
        o) output="$OPTARG" ;;
        n) count="$OPTARG" ;;
    esac
done
shift $((OPTIND - 1))
```

Con la invocación: `./script.sh -v -n 5 -o result.txt file1 file2`

<details>
<summary>Respuesta</summary>

```
verbose = true
output  = "result.txt"
count   = "5"
$1      = "file1"
$2      = "file2"
$#      = 2
```

getopts procesa `-v`, `-n 5`, `-o result.txt`. OPTIND apunta al primer
argumento no-opción (`file1`). Después de `shift $((OPTIND - 1))`,
solo quedan `file1` y `file2` en `$@`.
</details>

---

### Ejercicio 2 — Flags combinados

**Predicción**: ¿qué valores producen estas invocaciones?

```bash
# optstring: ":vcf:"
# Invocación A:
./script.sh -vc -f output.txt data.csv

# Invocación B:
./script.sh -vcf output.txt data.csv

# Invocación C:
./script.sh -fcv output.txt data.csv
```

<details>
<summary>Respuesta</summary>

**Invocación A**: `-vc` → v=true, c=true. `-f output.txt` → f="output.txt". Posicional: data.csv. ✓

**Invocación B**: `-vcf` → v=true, c=true, f="output.txt" (el argumento de -f es lo que sigue). Posicional: data.csv. ✓

**Invocación C**: `-fcv` → f="cv" (¡"cv" es el argumento de -f, no flags!). "output.txt" y "data.csv" son posicionales. ✗ Probablemente no es lo que el usuario quiso.

```
-vc -f output.txt → v, c, f="output.txt"    ✓
-vcf output.txt   → v, c, f="output.txt"    ✓
-fcv output.txt   → f="cv"                  ✗ (sorpresa!)
```

Cuando una opción con argumento (`:`) aparece en un grupo combinado, todo
lo que sigue EN ESE grupo se convierte en su argumento. Por eso `-f`
siempre debe ir **último** en flags combinados.
</details>

---

### Ejercicio 3 — Silent mode

**Predicción**: ¿qué imprime cada caso?

```bash
# Con : al inicio (silent mode)
while getopts ":f:v" opt; do
    case $opt in
        f) echo "file=$OPTARG" ;;
        v) echo "verbose" ;;
        :) echo "MISSING ARG for -$OPTARG" ;;
        ?) echo "UNKNOWN: -$OPTARG" ;;
    esac
done
```

```bash
# Caso A: opción desconocida
./script.sh -x

# Caso B: falta argumento
./script.sh -f
```

<details>
<summary>Respuesta</summary>

**Caso A**: `UNKNOWN: -x`
- `-x` no está en el optstring
- En silent mode: `opt='?'`, `OPTARG='x'`

**Caso B**: `MISSING ARG for -f`
- `-f` requiere argumento (tiene `:`), pero no hay nada después
- En silent mode: `opt=':'`, `OPTARG='f'`

Sin el `:` al inicio (default mode), Bash imprimiría:
- Caso A: `./script.sh: illegal option -- x` (mensaje genérico)
- Caso B: `./script.sh: option requires an argument -- f`

El silent mode da control total sobre los mensajes de error.
</details>

---

### Ejercicio 4 — shift y OPTIND

**Predicción**: ¿qué vale `$1` después del shift?

```bash
while getopts ":v" opt; do
    case $opt in v) ;; esac
done
shift $((OPTIND - 1))
echo "First arg: $1"
```

```bash
# Caso A:
./script.sh -v file.txt

# Caso B:
./script.sh file.txt

# Caso C:
./script.sh -v -v -v file.txt
```

<details>
<summary>Respuesta</summary>

Todos imprimen `First arg: file.txt`.

- **Caso A**: OPTIND=2 (procesó `-v`), shift 1 → `$1="file.txt"`
- **Caso B**: OPTIND=1 (no procesó nada), shift 0 → `$1="file.txt"`
- **Caso C**: OPTIND=4 (procesó `-v -v -v`), shift 3 → `$1="file.txt"`

getopts procesa todas las opciones repetidas. OPTIND siempre apunta al
primer argumento no-opción. `shift $((OPTIND - 1))` elimina exactamente
las opciones procesadas.

Nota: `-v` tres veces no es un error — simplemente la variable `verbose`
se asigna a true tres veces. Algunos scripts usan repetición para
niveles de verbosidad (`-vvv` = nivel 3).
</details>

---

### Ejercicio 5 — Opciones repetibles

**Predicción**: ¿qué contiene `${tags[@]}`?

```bash
tags=()
while getopts ":t:" opt; do
    case $opt in
        t) tags+=("$OPTARG") ;;
    esac
done

# ./script.sh -t linux -t bash -t docker
```

<details>
<summary>Respuesta</summary>

```
tags = ("linux" "bash" "docker")
```

Cada vez que getopts encuentra `-t`, ejecuta `tags+=("$OPTARG")`, que
añade el argumento al array. getopts itera sobre TODAS las opciones
en orden, así que `-t linux -t bash -t docker` produce tres iteraciones
con `OPTARG` = "linux", "bash", "docker" respectivamente.

Esto es el patrón correcto para opciones que se pueden repetir.
No necesitas ningún manejo especial — el `while getopts` ya itera múltiples veces.
</details>

---

### Ejercicio 6 — Double dash

**Predicción**: ¿qué argumentos posicionales tiene `$@` después del parsing?

```bash
while getopts ":v" opt; do
    case $opt in v) ;; esac
done
shift $((OPTIND - 1))

# ./script.sh -v -- -strange-file.txt normal.txt
```

<details>
<summary>Respuesta</summary>

```
$1 = "-strange-file.txt"
$2 = "normal.txt"
```

getopts reconoce `--` como fin de opciones. Deja de parsear y OPTIND
apunta al argumento después de `--`. Después del shift:
- `-v` fue procesado y eliminado
- `--` fue consumido por getopts
- `-strange-file.txt` y `normal.txt` son argumentos posicionales

Sin `--`, getopts intentaría interpretar `-strange-file.txt` como la
opción `-s` (o un error si `s` no está en el optstring).
</details>

---

### Ejercicio 7 — Manual parsing de flags largos

**Predicción**: ¿qué valores produce este parsing?

```bash
verbose=false
output=""
files=()

while (( $# > 0 )); do
    case "$1" in
        -v|--verbose) verbose=true; shift ;;
        --output=*)   output="${1#*=}"; shift ;;
        --output)     output="$2"; shift 2 ;;
        --)           shift; files+=("$@"); break ;;
        -*)           echo "Unknown: $1" >&2; exit 1 ;;
        *)            files+=("$1"); shift ;;
    esac
done

# ./script.sh --verbose --output=/tmp/out data.csv
```

<details>
<summary>Respuesta</summary>

```
verbose = true
output  = "/tmp/out"
files   = ("data.csv")
```

Paso a paso:
1. `--verbose` → match `-v|--verbose` → verbose=true, shift
2. `--output=/tmp/out` → match `--output=*` → output="${1#*=}" = "/tmp/out", shift
3. `data.csv` → match `*` (default) → files+=("data.csv"), shift
4. `$# = 0` → loop termina

`${1#*=}` elimina todo hasta e incluyendo el primer `=`:
`--output=/tmp/out` → `#*=` → `/tmp/out`
</details>

---

### Ejercicio 8 — Validación post-parse

**Predicción**: ¿qué error produce este script?

```bash
FORMAT=""
FILE=""

while getopts ":f:i:" opt; do
    case $opt in
        f) FORMAT="$OPTARG" ;;
        i) FILE="$OPTARG" ;;
    esac
done

: "${FILE:?-i FILE is required}"

case "$FORMAT" in
    json|csv|text) ;;
    "") FORMAT="text" ;;       # default
    *) echo "Invalid format: $FORMAT" >&2; exit 1 ;;
esac

# ./script.sh -f yaml -i data.csv
```

<details>
<summary>Respuesta</summary>

Imprime: `Invalid format: yaml` y termina con exit 1.

Paso a paso:
1. getopts parsea `-f yaml` → FORMAT="yaml" y `-i data.csv` → FILE="data.csv"
2. `${FILE:?...}` → FILE no está vacío → no falla
3. `case "yaml"` → no coincide con json, csv, text, ni "" → cae en `*`
4. Imprime error y sale

Si la invocación fuera `./script.sh -f json` (sin -i):
El paso 2 fallaría: `bash: FILE: -i FILE is required`

La validación post-parse es donde se verifican las reglas de negocio
que getopts no puede manejar (valores permitidos, combinaciones, etc.).
</details>

---

### Ejercicio 9 — Subcomando con getopts propio

**Predicción**: ¿funciona correctamente?

```bash
cmd_greet() {
    local name="World"
    while getopts ":n:" opt; do
        case $opt in n) name="$OPTARG" ;; esac
    done
    echo "Hello, $name!"
}

command="$1"; shift
case "$command" in
    greet) cmd_greet "$@" ;;
esac

# ./script.sh greet -n Alice
```

<details>
<summary>Respuesta</summary>

Imprime: `Hello, Alice!`

Funciona correctamente porque:
1. `$1` = "greet", `shift` → `$@` = "-n Alice"
2. `cmd_greet "-n" "Alice"` se llama con los args restantes
3. Dentro de `cmd_greet`, getopts procesa `-n Alice` → name="Alice"

**Pero hay una trampa**: si se llama `cmd_greet` una segunda vez en el mismo
script, OPTIND no se reseteó. Solución robusta:

```bash
cmd_greet() {
    local OPTIND=1    # reset — local para no afectar el caller
    local name="World"
    while getopts ":n:" opt; do
        # ...
    done
}
```

`local OPTIND=1` es la forma correcta de resetear OPTIND dentro de una
función: es local (no contamina el global) y empieza en 1.
</details>

---

### Ejercicio 10 — Verbosidad incremental

**Predicción**: ¿qué valor tiene `verbosity`?

```bash
verbosity=0

while getopts ":v" opt; do
    case $opt in
        v) (( verbosity++ )) ;;
    esac
done

# ./script.sh -v -v -v
# o equivalente: ./script.sh -vvv
```

<details>
<summary>Respuesta</summary>

`verbosity = 3`

Cada `-v` (o cada `v` en `-vvv`) incrementa el contador. getopts procesa
cada letra individualmente, incluso en flags combinados.

```
-v -v -v  → 3 iteraciones del loop, cada una con opt='v'
-vvv      → 3 iteraciones del loop (getopts descompone automáticamente)
```

Uso práctico:
```bash
case $verbosity in
    0) LOG_LEVEL="error" ;;
    1) LOG_LEVEL="warn" ;;
    2) LOG_LEVEL="info" ;;
    *) LOG_LEVEL="debug" ;;
esac
```

Este patrón es común en herramientas Unix: `ssh -vvv`, `curl -vvv`,
`ansible-playbook -vvvv`.
</details>