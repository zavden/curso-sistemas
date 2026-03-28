# T03 — Funciones

## Sintaxis

Bash soporta dos sintaxis para definir funciones:

```bash
# Sintaxis 1 — POSIX (portable):
mi_funcion() {
    echo "Hola desde la función"
}

# Sintaxis 2 — bash (con keyword function):
function mi_funcion {
    echo "Hola desde la función"
}

# Sintaxis 3 — combinada (redundante pero válida):
function mi_funcion() {
    echo "Hola desde la función"
}

# Preferir la sintaxis 1 (POSIX) — es la más portable y la convención
# más común en scripts profesionales
```

```bash
# Llamar a una función (sin paréntesis, como un comando):
mi_funcion
mi_funcion arg1 arg2    # con argumentos

# Las funciones deben definirse ANTES de ser llamadas
# (bash procesa el script de arriba a abajo)
```

---

## Argumentos

Las funciones reciben argumentos igual que los scripts — mediante `$1`, `$2`,
etc.:

```bash
greet() {
    local name="$1"
    local greeting="${2:-Hello}"
    echo "$greeting, $name!"
}

greet "World"           # Hello, World!
greet "World" "Hola"    # Hola, World!
```

```bash
# $@ y $# dentro de una función se refieren a los argumentos de LA FUNCIÓN,
# no del script:
show_args() {
    echo "La función recibió $# argumentos:"
    for arg in "$@"; do
        echo "  - $arg"
    done
}

show_args "uno" "dos" "tres"
# La función recibió 3 argumentos:
#   - uno
#   - dos
#   - tres
```

### Pasar arrays a funciones

```bash
# Los arrays NO se pasan directamente — se expanden como argumentos individuales:
process_items() {
    for item in "$@"; do
        echo "Procesando: $item"
    done
}

ITEMS=("file one.txt" "file two.txt" "file three.txt")
process_items "${ITEMS[@]}"
# Procesando: file one.txt
# Procesando: file two.txt
# Procesando: file three.txt

# Pasar un array por nombre (bash 4.3+ con nameref):
process_array() {
    local -n arr=$1    # nameref: arr es una referencia a la variable nombrada
    for item in "${arr[@]}"; do
        echo "Item: $item"
    done
}

COLORS=(red green blue)
process_array COLORS    # pasar el NOMBRE, no el valor
# Item: red
# Item: green
# Item: blue
```

---

## Variables locales

Por defecto, las variables en funciones son **globales** — modifican el scope
del llamador:

```bash
# SIN local — la variable contamina el scope global:
bad_function() {
    RESULT="valor de la función"    # modifica el scope global
}

RESULT="valor original"
bad_function
echo "$RESULT"    # "valor de la función" — ¡se sobrescribió!
```

```bash
# CON local — la variable solo existe dentro de la función:
good_function() {
    local result="valor de la función"
    echo "$result"
}

RESULT="valor original"
good_function              # imprime "valor de la función"
echo "$RESULT"             # "valor original" — no se modificó
```

### Regla: siempre usar local

```bash
calculate() {
    local input="$1"
    local temp                 # declarar sin asignar
    local -r CONST="fijo"     # -r = readonly (no se puede reasignar)
    local -i number=42         # -i = integer (operaciones aritméticas)
    local -a items=()         # -a = array indexado
    local -A map=()           # -A = array asociativo

    temp=$((input * 2))
    echo "$temp"
}
```

### Scope de local en funciones anidadas

```bash
# local es visible en funciones llamadas desde la función:
outer() {
    local msg="desde outer"
    inner
}

inner() {
    echo "$msg"    # "desde outer" — local de outer es visible en inner
}

outer    # imprime "desde outer"
inner    # (vacío) — msg no existe fuera de outer

# Esto se llama "dynamic scoping" — diferente de la mayoría de lenguajes
# que usan "lexical scoping"
```

---

## return vs exit

```bash
# return — sale de la FUNCIÓN con un exit code (0-255):
check_file() {
    [[ -f "$1" ]] && return 0 || return 1
}

if check_file /etc/passwd; then
    echo "Existe"
fi

# exit — sale del SCRIPT ENTERO (o del subshell):
validate() {
    if [[ -z "$1" ]]; then
        echo "Error: argumento vacío" >&2
        exit 1    # ¡TERMINA EL SCRIPT, no solo la función!
    fi
}
```

```bash
# Patrón seguro: return en la función, exit en el llamador
validate_input() {
    [[ -n "$1" ]] || return 1
    [[ "$1" =~ ^[0-9]+$ ]] || return 2
    return 0
}

if ! validate_input "$1"; then
    echo "Input inválido" >&2
    exit 1
fi
```

### Retornar valores (no solo exit codes)

El `return` solo devuelve un número 0-255 (exit code). Para devolver datos:

```bash
# Método 1: echo + command substitution (el más común):
get_hostname() {
    local host
    host=$(hostname -f 2>/dev/null || hostname)
    echo "$host"    # "retorna" imprimiendo a stdout
}
RESULT=$(get_hostname)    # captura la salida
echo "Host: $RESULT"

# Método 2: variable global por convención:
get_config() {
    # Por convención, la función escribe en una variable conocida
    REPLY=$(grep "^$1=" /etc/config 2>/dev/null | cut -d= -f2-)
}
get_config "database_host"
echo "DB: $REPLY"

# Método 3: nameref (bash 4.3+):
get_timestamp() {
    local -n result=$1     # referencia al nombre pasado
    result=$(date +%s)
}
get_timestamp MY_TIME
echo "Timestamp: $MY_TIME"
```

### Retornar múltiples valores

```bash
# Método 1: múltiples líneas en stdout
get_dimensions() {
    echo "1920"
    echo "1080"
}
read -r WIDTH HEIGHT < <(get_dimensions | paste - -)

# Método 2: separador conocido
get_user_info() {
    echo "alice:30:/home/alice"
}
IFS=: read -r NAME AGE HOME_DIR < <(get_user_info)

# Método 3: namerefs múltiples (bash 4.3+)
split_path() {
    local -n dir=$1
    local -n base=$2
    local path=$3
    dir="${path%/*}"
    base="${path##*/}"
}
split_path DIR BASE "/var/log/syslog"
echo "Dir: $DIR, Base: $BASE"
```

---

## Funciones como comandos

Las funciones se comportan como comandos — se pueden usar con pipes, redirección,
y en cualquier lugar donde iría un comando:

```bash
to_upper() {
    tr '[:lower:]' '[:upper:]'
}

# En un pipeline:
echo "hello world" | to_upper
# HELLO WORLD

# Con redirección:
to_upper < /etc/hostname

# En command substitution:
UPPER=$(echo "test" | to_upper)
```

```bash
# Función que filtra — lee de stdin, escribe a stdout:
skip_comments() {
    while IFS= read -r line; do
        [[ "$line" == \#* || -z "$line" ]] && continue
        echo "$line"
    done
}

skip_comments < /etc/fstab
cat config.ini | skip_comments
```

---

## Funciones recursivas

Bash soporta recursión, pero con limitaciones prácticas:

```bash
# Factorial:
factorial() {
    local n=$1
    if (( n <= 1 )); then
        echo 1
    else
        local sub
        sub=$(factorial $((n - 1)))
        echo $((n * sub))
    fi
}
factorial 10    # 3628800

# Listar directorio recursivamente (como tree):
list_tree() {
    local dir="$1"
    local prefix="${2:-}"

    local entries=("$dir"/*)
    local count=${#entries[@]}
    local i=0

    for entry in "${entries[@]}"; do
        ((i++))
        local name="${entry##*/}"
        if [[ $i -eq $count ]]; then
            echo "${prefix}└── $name"
            [[ -d "$entry" ]] && list_tree "$entry" "${prefix}    "
        else
            echo "${prefix}├── $name"
            [[ -d "$entry" ]] && list_tree "$entry" "${prefix}│   "
        fi
    done
}
list_tree /etc/apt
```

### Límites de recursión

```bash
# Bash no tiene tail-call optimization
# La profundidad de recursión está limitada por:
# 1. El stack del proceso (ulimit -s, típicamente 8MB)
# 2. El overhead de cada subshell en $(...)

# Para recursión profunda, preferir loops:
factorial_loop() {
    local n=$1
    local result=1
    for ((i = 2; i <= n; i++)); do
        ((result *= i))
    done
    echo $result
}
```

---

## Patrones profesionales

### Die function

```bash
die() {
    echo "ERROR: $*" >&2
    exit 1
}

[[ -f "$CONFIG" ]] || die "Config $CONFIG no encontrada"
[[ -n "$DB_HOST" ]] || die "DB_HOST no definida"
```

### Log function

```bash
log() {
    local level="${1:-INFO}"
    shift
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$level] $*" >&2
}

log INFO "Iniciando proceso"
log WARN "Disco al 85%"
log ERROR "Conexión rechazada"
```

### Require command

```bash
require_cmd() {
    command -v "$1" &>/dev/null || die "'$1' no está instalado"
}

require_cmd docker
require_cmd jq
require_cmd curl
```

### Cleanup con trap

```bash
# Patrón completo de script profesional:
TMPDIR=""

cleanup() {
    [[ -n "$TMPDIR" ]] && rm -rf "$TMPDIR"
}
trap cleanup EXIT    # se ejecuta al salir (ver T04_Traps)

main() {
    TMPDIR=$(mktemp -d)
    # ... toda la lógica aquí ...
}

main "$@"
```

### Wrapper de main

```bash
#!/usr/bin/env bash
set -euo pipefail

# Todas las funciones auxiliares arriba de main:
validate_args() { ... }
process_file() { ... }
generate_report() { ... }

# main contiene la lógica principal:
main() {
    validate_args "$@"
    for file in "$@"; do
        process_file "$file"
    done
    generate_report
}

# Llamar a main con todos los argumentos del script:
main "$@"

# Ventaja: main() puede estar al final del archivo,
# donde es fácil de encontrar, y todas las funciones
# auxiliares están definidas arriba
```

---

## Ejercicios

### Ejercicio 1 — Función con local y return

```bash
is_number() {
    local input="$1"
    [[ "$input" =~ ^-?[0-9]+$ ]]
}

# Probar:
is_number 42 && echo "sí" || echo "no"     # sí
is_number -7 && echo "sí" || echo "no"     # sí
is_number 3.14 && echo "sí" || echo "no"   # no
is_number abc && echo "sí" || echo "no"    # no
```

---

### Ejercicio 2 — Función que retorna datos

```bash
split_path() {
    local path="$1"
    echo "${path%/*}"     # directorio
    echo "${path##*/}"    # nombre
}

read -r DIR NAME < <(split_path "/var/log/syslog" | paste - -)
echo "Dir: $DIR"
echo "Name: $NAME"
```

---

### Ejercicio 3 — Patrón main con funciones auxiliares

```bash
#!/usr/bin/env bash
set -euo pipefail

die() { echo "ERROR: $*" >&2; exit 1; }
log() { echo "[$(date '+%H:%M:%S')] $*" >&2; }

validate() {
    [[ $# -ge 1 ]] || die "Uso: $(basename "$0") <directorio>"
    [[ -d "$1" ]] || die "'$1' no es un directorio"
}

count_files() {
    local dir="$1"
    local count
    count=$(find "$dir" -type f | wc -l)
    echo "$count"
}

main() {
    validate "$@"
    local dir="$1"
    log "Contando archivos en $dir"
    local total
    total=$(count_files "$dir")
    echo "Total archivos: $total"
}

main "$@"
```

---

### Ejercicio 4 — Función con array asociativo

```bash
declare -A TRANSLATIONS
TRANSLATIONS[hello]=hola
TRANSLATIONS[world]=mundo
TRANSLATIONS[yes]=si
TRANSLATIONS[no]=no

translate() {
    local -n dict=$1
    local word="${2,,}"
    echo "${dict[$word]:-unknown}"
}

translate TRANSLATIONS hello
translate TRANSLATIONS WORLD
translate TRANSLATIONS maybe
```

---

### Ejercicio 5 — Function factory

```bash
#!/bin/bash
# Función que crea funciones dinámicamente

create_greeter() {
    local name="$1"
    local greeting="${2:-Hello}"

    eval "${name}() { echo '${greeting}, ${name}!'; }"
}

create_greeter alice "Buenos días"
create_greeter bob "Guten Tag"

alice  # Buenos días, alice!
bob    # Guten Tag, bob!
```

---

### Ejercicio 6 — nameref para retornar array

```bash
get_files() {
    local -n result=$1
    local dir="${2:-.}"
    result=()
    while IFS= read -r -d '' f; do
        result+=("$f")
    done < <(find "$dir" -type f -name "*.txt" -print0)
}

declare -a TEXT_FILES
get_files TEXT_FILES "/etc"

echo "Archivos encontrados: ${#TEXT_FILES[@]}"
for f in "${TEXT_FILES[@]}"; do
    echo "  $f"
done
```

---

### Ejercicio 7 — Funciones en библиотеки

```bash
# Guardar funciones en un archivo de biblioteca:
cat > /tmp/mylib.sh << 'EOF'
#!/bin/bash
# Biblioteca de funciones útiles

has_command() {
    command -v "$1" &>/dev/null
}

require_command() {
    has_command "$1" || { echo "ERROR: '$1' no encontrado" >&2; exit 1; }
}

is_root() {
    [[ $(id -u) -eq 0 ]]
}

timestamp() {
    date '+%Y-%m-%d %H:%M:%S'
}
EOF

# Usar la biblioteca:
source /tmp/mylib.sh

require_command jq
is_root && echo "Eres root" || echo "No eres root"
echo "Timestamp: $(timestamp)"

rm /tmp/mylib.sh
```

---

### Ejercicio 8 — Recursión vs iteración

```bash
#!/bin/bash
# Comparar factorial recursivo vs iterativo

factorial_recursive() {
    local n=$1
    if (( n <= 1 )); then
        echo 1
    else
        echo $((n * $(factorial_recursive $((n - 1)))))
    fi
}

factorial_iterative() {
    local n=$1
    local result=1
    for ((i = 2; i <= n; i++)); do
        ((result *= i))
    done
    echo $result
}

echo "Recursivo 10!: $(factorial_recursive 10)"
echo "Iterativo 10!: $(factorial_iterative 10)"

# Medir tiempo para números grandes
time factorial_recursive 15
time factorial_iterative 15
```
