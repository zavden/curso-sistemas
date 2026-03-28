# Lab — Getopts

## Objetivo

Dominar el parsing de argumentos en bash: getopts para opciones
cortas POSIX (OPTSTRING, OPTARG, OPTIND), modo silencioso para
control de errores, shift para acceder a argumentos posicionales,
while/case manual para opciones largas (--verbose, --output=file),
validacion de input, y el patron profesional completo.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — getopts POSIX

### Objetivo

Usar el builtin getopts para parsing de opciones cortas.

### Paso 1.1: getopts basico

```bash
docker compose exec debian-dev bash -c '
echo "=== getopts basico ==="
echo ""
cat > /tmp/getopts-basic.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

VERBOSE=0
OUTPUT=""

while getopts ":vho:" opt; do
    case $opt in
        v) VERBOSE=1 ;;
        o) OUTPUT="$OPTARG" ;;
        h) echo "Uso: $(basename "$0") [-v] [-o archivo] [-h] <input>"; exit 0 ;;
        \?) echo "Opcion invalida: -$OPTARG" >&2; exit 1 ;;
        :)  echo "-$OPTARG requiere un argumento" >&2; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

echo "verbose=$VERBOSE"
echo "output=${OUTPUT:-stdout}"
echo "args restantes: $@"
SCRIPT
chmod +x /tmp/getopts-basic.sh
echo "--- Sin opciones ---"
/tmp/getopts-basic.sh archivo.txt
echo ""
echo "--- Con opciones ---"
/tmp/getopts-basic.sh -v -o salida.txt archivo.txt
echo ""
echo "--- Flags combinados ---"
/tmp/getopts-basic.sh -vo salida.txt archivo.txt
echo "(v + o combinados en -vo)"
'
```

### Paso 1.2: OPTSTRING explicado

```bash
docker compose exec debian-dev bash -c '
echo "=== OPTSTRING ==="
echo ""
echo "La cadena define que opciones acepta getopts:"
echo ""
echo "  \":vho:\""
echo "   ^         : al inicio = modo silencioso"
echo "    v        = flag -v (sin argumento)"
echo "     h       = flag -h (sin argumento)"
echo "      o:     = opcion -o con argumento obligatorio"
echo ""
echo "--- Modo silencioso (: al inicio) ---"
echo "Sin : → getopts imprime errores automaticamente"
echo "Con : → el script controla los mensajes de error"
echo ""
echo "--- OPTARG ---"
echo "Contiene el argumento de opciones con :"
echo "En \\?, contiene la letra de la opcion invalida"
echo "En :, contiene la letra que falta argumento"
echo ""
echo "--- OPTIND ---"
echo "Indice del proximo argumento a procesar"
echo "Despues del loop: shift \$((OPTIND - 1))"
echo "→ \$@ contiene solo los argumentos posicionales"
'
```

### Paso 1.3: Errores en getopts

Antes de ejecutar, predice: que pasa con `-x` (opcion invalida)
y con `-o` sin argumento?

```bash
docker compose exec debian-dev bash -c '
echo "=== Manejo de errores ==="
echo ""
cat > /tmp/getopts-errors.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

while getopts ":vho:" opt; do
    case $opt in
        v) echo "  → flag -v activado" ;;
        h) echo "  → help" ;;
        o) echo "  → output: $OPTARG" ;;
        \?) echo "  → ERROR: opcion invalida: -$OPTARG" >&2 ;;
        :)  echo "  → ERROR: -$OPTARG requiere argumento" >&2 ;;
    esac
done
SCRIPT
chmod +x /tmp/getopts-errors.sh
echo "--- Opcion invalida ---"
/tmp/getopts-errors.sh -v -x -o out.txt 2>&1 || true
echo ""
echo "--- Falta argumento ---"
/tmp/getopts-errors.sh -v -o 2>&1 || true
echo ""
echo "Los : en OPTSTRING dan control total sobre los errores"
'
```

---

## Parte 2 — shift y opciones largas

### Objetivo

Usar shift para argumentos posicionales y while/case para
soportar opciones largas.

### Paso 2.1: shift explicado

```bash
docker compose exec debian-dev bash -c '
echo "=== shift ==="
echo ""
cat > /tmp/shift-demo.sh << '\''SCRIPT'\''
#!/bin/bash
echo "Antes del shift:"
echo "  \$# = $#"
echo "  \$1 = $1"
echo "  \$2 = $2"
echo "  \$3 = $3"
echo "  \$@ = $@"
echo ""
shift
echo "Despues de shift 1:"
echo "  \$# = $#"
echo "  \$1 = $1"
echo "  \$2 = $2"
echo "  \$@ = $@"
echo ""
shift 2
echo "Despues de shift 2:"
echo "  \$# = $#"
echo "  \$1 = ${1:-nada}"
echo "  \$@ = $@"
SCRIPT
chmod +x /tmp/shift-demo.sh
/tmp/shift-demo.sh uno dos tres cuatro cinco
echo ""
echo "shift N elimina los primeros N argumentos"
echo "Los restantes se renumeran: \$N+1 → \$1"
'
```

### Paso 2.2: Opciones largas con while/case

```bash
docker compose exec debian-dev bash -c '
echo "=== Opciones largas ==="
echo ""
cat > /tmp/long-opts.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

VERBOSE=0
OUTPUT=""
DRY_RUN=0
FILES=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -o|--output)
            [[ $# -lt 2 ]] && { echo "ERROR: --output requiere argumento" >&2; exit 1; }
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
            echo "Uso: $(basename "$0") [-v] [-o FILE] [-n] [-h] archivos..."
            exit 0
            ;;
        --)
            shift
            FILES+=("$@")
            break
            ;;
        -*)
            echo "Opcion desconocida: $1" >&2
            exit 1
            ;;
        *)
            FILES+=("$1")
            shift
            ;;
    esac
done

echo "verbose=$VERBOSE output=${OUTPUT:-stdout} dry_run=$DRY_RUN"
echo "files=(${FILES[*]:-none})"
SCRIPT
chmod +x /tmp/long-opts.sh
echo "--- Opciones largas ---"
/tmp/long-opts.sh --verbose --output=report.txt data.csv
echo ""
echo "--- Mezclar cortas y largas ---"
/tmp/long-opts.sh -v --output report.txt -n file1 file2
echo ""
echo "--- Separador -- ---"
/tmp/long-opts.sh -v -- --este-no-es-flag.txt
echo "  (-- indica fin de opciones; lo que sigue son argumentos)"
'
```

### Paso 2.3: getopts vs while/case

```bash
docker compose exec debian-dev bash -c '
echo "=== getopts vs while/case ==="
echo ""
printf "%-25s %-15s %-15s\n" "Caracteristica" "getopts" "while/case"
printf "%-25s %-15s %-15s\n" "------------------------" "--------------" "--------------"
printf "%-25s %-15s %-15s\n" "Opciones cortas (-v)" "Si" "Si"
printf "%-25s %-15s %-15s\n" "Opciones largas (--verb)" "No" "Si"
printf "%-25s %-15s %-15s\n" "--flag=value" "No" "Si"
printf "%-25s %-15s %-15s\n" "Combinar flags (-vo)" "Si" "No"
printf "%-25s %-15s %-15s\n" "POSIX compatible" "Si" "No (bash)"
printf "%-25s %-15s %-15s\n" "Complejidad" "Baja" "Media"
echo ""
echo "Recomendacion:"
echo "  Scripts simples con solo -v -h -o: getopts"
echo "  Scripts con --verbose --output: while/case"
echo "  La mayoria de scripts modernos usa while/case"
'
```

---

## Parte 3 — Validacion y patron completo

### Objetivo

Validar argumentos y construir un script profesional con
parsing completo.

### Paso 3.1: Validacion de input

```bash
docker compose exec debian-dev bash -c '
echo "=== Validacion de input ==="
echo ""
cat > /tmp/validate.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

validate_port() {
    local port="$1"
    [[ "$port" =~ ^[0-9]+$ ]] || die "puerto invalido: $port (debe ser numerico)"
    (( port >= 1 && port <= 65535 )) || die "puerto fuera de rango: $port (1-65535)"
}

validate_file() {
    local file="$1"
    [[ -f "$file" ]] || die "archivo no encontrado: $file"
    [[ -r "$file" ]] || die "sin permiso de lectura: $file"
}

validate_dir() {
    local dir="$1"
    [[ -d "$dir" ]] || die "directorio no existe: $dir"
    [[ -w "$dir" ]] || die "sin permiso de escritura: $dir"
}

echo "--- Puerto valido ---"
validate_port 8080 && echo "8080: OK"
echo ""
echo "--- Puerto invalido ---"
validate_port "abc" 2>&1 || true
validate_port 99999 2>&1 || true
echo ""
echo "--- Archivo ---"
validate_file /etc/passwd && echo "/etc/passwd: OK"
validate_file /no/existe 2>&1 || true
echo ""
echo "--- Directorio ---"
validate_dir /tmp && echo "/tmp: OK"
validate_dir /no/existe 2>&1 || true
SCRIPT
chmod +x /tmp/validate.sh
/tmp/validate.sh
'
```

### Paso 3.2: Script profesional completo

```bash
docker compose exec debian-dev bash -c '
echo "=== Script profesional ==="
echo ""
cat > /tmp/professional.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_NAME=$(basename "$0")
readonly VERSION="1.0.0"

# --- Defaults ---
VERBOSE=0
OUTPUT="/dev/stdout"
FORMAT="text"
FILES=()

# --- Utilidades ---
die()  { echo "$SCRIPT_NAME: error: $*" >&2; exit 1; }
info() { (( VERBOSE )) && echo "$SCRIPT_NAME: $*" >&2 || true; }

usage() {
    cat << EOF
Uso: $SCRIPT_NAME [opciones] <archivo> [archivo...]

Procesa archivos y genera un reporte.

Opciones:
  -v, --verbose         Modo verbose
  -o, --output ARCHIVO  Archivo de salida (default: stdout)
  -f, --format FMT      Formato: text, json, csv (default: text)
  -h, --help            Mostrar ayuda
  -V, --version         Mostrar version

Ejemplos:
  $SCRIPT_NAME -v data.txt
  $SCRIPT_NAME --format=csv -o report.csv *.txt
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
            -*)            die "opcion desconocida: $1 (ver --help)" ;;
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
        *) die "formato invalido: $FORMAT (usar text, json, csv)" ;;
    esac
}

process() {
    info "Procesando ${#FILES[@]} archivo(s) en formato $FORMAT"
    for f in "${FILES[@]}"; do
        local lines
        lines=$(wc -l < "$f")
        info "  $f: $lines lineas"
        echo "$f: $lines lineas"
    done
}

# --- Main ---
parse_args "$@"
validate
process
SCRIPT
chmod +x /tmp/professional.sh
echo "--- Help ---"
/tmp/professional.sh --help
echo ""
echo "--- Uso normal ---"
/tmp/professional.sh -v /etc/passwd /etc/hostname
echo ""
echo "--- Version ---"
/tmp/professional.sh --version
echo ""
echo "--- Error: sin archivos ---"
/tmp/professional.sh 2>&1 || true
echo ""
echo "--- Error: formato invalido ---"
/tmp/professional.sh --format=xml /etc/passwd 2>&1 || true
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/getopts-basic.sh /tmp/getopts-errors.sh
rm -f /tmp/shift-demo.sh /tmp/long-opts.sh
rm -f /tmp/validate.sh /tmp/professional.sh
echo "Archivos temporales eliminados"
'
```

---

## Conceptos reforzados

1. **getopts** usa OPTSTRING para definir opciones: letra sola
   = flag, letra con `:` = opcion con argumento. `:` al inicio
   activa modo silencioso para control total de errores.

2. `shift $((OPTIND - 1))` despues de getopts deja en `$@` solo
   los argumentos posicionales. `OPTARG` contiene el valor de
   opciones con argumento.

3. Para **opciones largas** (`--verbose`, `--output=file`), usar
   `while/case` manual con `shift`. `--` indica fin de opciones.

4. `${1#*=}` extrae el valor de `--flag=valor` eliminando todo
   hasta el `=`. Es el patron estandar para `--key=value`.

5. Siempre **validar** argumentos despues de parsear: verificar
   existencia de archivos, rangos de puertos, valores de enums.
   `die()` centraliza los mensajes de error.

6. El script profesional separa `parse_args()`, `validate()`, y
   `process()`. Soporta `-h`/`--help`, `-V`/`--version`, y
   mensajes de error claros con el nombre del script.
