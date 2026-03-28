# T03 — Funciones

> **Fuentes:** `README.md` (base) + `README.max.md` (ampliado) + `labs/README.md`
>
> **Erratas corregidas respecto a las fuentes:**
>
> | Ubicación | Error | Corrección |
> |---|---|---|
> | max.md:591 | "Funciones en библиотеки" (palabra en ruso) | "Funciones como bibliotecas" (español) |
> | labs:29 | "Sintaxis POSIX (con function keyword)" | `function` es extensión **bash**, no POSIX. POSIX es `name() { ... }` |

---

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
    local -i number=42        # -i = integer (operaciones aritméticas)
    local -a items=()         # -a = array indexado
    local -A map=()           # -A = array asociativo

    temp=$((input * 2))
    echo "$temp"
}
```

### Scope de local en funciones anidadas (dynamic scoping)

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
#
# En lexical scoping, inner() NO vería msg porque no está definida
# en su scope léxico. Bash usa dynamic scoping: la variable es visible
# en cualquier función llamada mientras la variable esté activa en el
# call stack.
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

```bash
# return sin argumento retorna el exit code del último comando:
check_root() {
    grep -q "^root:" /etc/passwd
    return    # retorna el exit code de grep (0 si encontró, 1 si no)
}
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

> **Cuidado con echo + $():** cualquier `echo` dentro de la función se
> captura, no solo el "valor de retorno". Los mensajes de debug deben ir
> a stderr (`>&2`), no a stdout:
>
> ```bash
> bad() {
>     echo "debug info"     # ¡contamina el retorno!
>     echo "valor_real"
> }
> RESULT=$(bad)    # RESULT="debug info\nvalor_real" — MAL
>
> good() {
>     echo "debug info" >&2    # stderr, no se captura
>     echo "valor_real"        # stdout, se captura con $()
> }
> RESULT=$(good)   # RESULT="valor_real" — BIEN
> ```

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

```bash
# Componer múltiples funciones-filtro con pipes:
add_prefix() {
    local prefix="$1"
    while IFS= read -r line; do
        echo "${prefix}${line}"
    done
}

number_lines() {
    local n=1
    while IFS= read -r line; do
        printf "%3d: %s\n" "$n" "$line"
        ((n++))
    done
}

echo -e "hello\nworld\nbash" | to_upper | add_prefix ">>> " | number_lines
#   1: >>> HELLO
#   2: >>> WORLD
#   3: >>> BASH
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
#
# Cada llamada recursiva con $() crea un subshell nuevo,
# lo que multiplica el costo en memoria y tiempo.

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

## Funciones como bibliotecas

```bash
# Guardar funciones reutilizables en un archivo .sh:
# --- /usr/local/lib/mylib.sh ---
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
# --- fin de mylib.sh ---

# Importar en otro script con source:
source /usr/local/lib/mylib.sh

require_command jq
is_root && echo "Eres root" || echo "No eres root"
echo "Timestamp: $(timestamp)"
```

---

## Patrones profesionales

### Die / warn / info

```bash
die()  { echo "ERROR: $*" >&2; exit 1; }
warn() { echo "WARN: $*" >&2; }
info() { echo "INFO: $*" >&2; }

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

### Main wrapper con guard de source

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

# Solo ejecutar main si el script se ejecuta directamente
# (no si se importa con source):
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi

# Ventajas:
# 1. main() al final del archivo, fácil de encontrar
# 2. Todas las funciones auxiliares definidas arriba
# 3. Se puede hacer source sin ejecutar main (para tests o reutilización)
```

> **`${BASH_SOURCE[0]}` vs `$0`:** cuando el script se ejecuta directamente,
> ambos son iguales (la ruta del script). Cuando se importa con `source`,
> `$0` es el script que hizo el `source`, pero `${BASH_SOURCE[0]}` sigue
> siendo el archivo actual. Por eso la comparación funciona como guard.

---

## Ejercicios

### Ejercicio 1 — Sintaxis y llamada básica

```bash
# Definir una función "saludar" que reciba un nombre y un saludo opcional
# (default: "Hola"). Luego llamarla de tres formas:

saludar() {
    local name="$1"
    local greeting="${2:-Hola}"
    echo "$greeting, $name!"
}

saludar "Carlos"
saludar "Ana" "Buenos días"
saludar
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué imprime <code>saludar "Carlos"</code>?</summary>

`Hola, Carlos!` — `$1` es "Carlos", `$2` está vacío así que `${2:-Hola}` usa el default "Hola".

</details>

<details><summary>2. ¿Qué imprime <code>saludar "Ana" "Buenos días"</code>?</summary>

`Buenos días, Ana!` — `$2` es "Buenos días", sobrescribe el default.

</details>

<details><summary>3. ¿Qué imprime <code>saludar</code> (sin argumentos)?</summary>

`Hola, !` — `$1` está vacío (se expande a cadena vacía), `$2` también, así que usa default "Hola". El resultado es `Hola, !` con un nombre vacío.

</details>

---

### Ejercicio 2 — local vs global scope

```bash
COLOR="rojo"

cambiar_sin_local() {
    COLOR="azul"
}

cambiar_con_local() {
    local COLOR="verde"
    echo "Dentro: $COLOR"
}

echo "A: $COLOR"
cambiar_sin_local
echo "B: $COLOR"
cambiar_con_local
echo "C: $COLOR"
```

**Predicción:** antes de ejecutar, responde:

<details><summary>¿Qué imprimen las líneas A, B, C y "Dentro"?</summary>

```
A: rojo          # valor inicial
B: azul          # cambiar_sin_local modificó la global
Dentro: verde    # local dentro de la función
C: azul          # la global sigue siendo "azul" (local no la tocó)
```

Sin `local`, la función modifica la variable global. Con `local`, la función trabaja con una copia independiente.

</details>

---

### Ejercicio 3 — Dynamic scoping

```bash
wrapper() {
    local SECRET="clave123"
    helper
}

helper() {
    echo "SECRET=${SECRET:-<vacío>}"
}

wrapper
helper
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué imprime cuando <code>wrapper</code> llama a <code>helper</code>?</summary>

`SECRET=clave123` — por dynamic scoping, `helper` ve las variables locales de su llamador (`wrapper`). Aunque `SECRET` es `local` en `wrapper`, es visible en cualquier función invocada desde `wrapper`.

</details>

<details><summary>2. ¿Qué imprime cuando se llama <code>helper</code> directamente?</summary>

`SECRET=<vacío>` — fuera de `wrapper`, la variable `SECRET` no existe (era local a `wrapper`), así que `${SECRET:-<vacío>}` usa el default.

</details>

---

### Ejercicio 4 — return vs exit

```bash
# Archivo: test_return.sh
#!/usr/bin/env bash

validar() {
    [[ -n "$1" ]] || return 1
    [[ "$1" =~ ^[0-9]+$ ]] || return 2
    (( $1 > 0 && $1 <= 100 )) || return 3
    return 0
}

for input in "42" "" "abc" "200" "0"; do
    validar "$input"
    case $? in
        0) echo "'$input' → válido" ;;
        1) echo "'$input' → vacío" ;;
        2) echo "'$input' → no es número" ;;
        3) echo "'$input' → fuera de rango" ;;
    esac
done
```

**Predicción:** antes de ejecutar, responde:

<details><summary>¿Qué return code produce cada input y qué línea se imprime?</summary>

```
'42'  → válido          # return 0: no vacío, es número, 0 < 42 <= 100
''    → vacío           # return 1: -n "" falla
'abc' → no es número    # return 2: regex ^[0-9]+$ falla
'200' → fuera de rango  # return 3: 200 > 100
'0'   → fuera de rango  # return 3: 0 > 0 falla ($1 > 0 es falso)
```

Nota: `return` sale de la función inmediatamente — las validaciones se ejecutan en orden y la primera que falla determina el código.

</details>

---

### Ejercicio 5 — echo + $() para retornar datos

```bash
get_extension() {
    local file="$1"
    echo "Procesando: $file" >&2    # debug a stderr
    echo "${file##*.}"               # retorno a stdout
}

get_basename_no_ext() {
    local file="$1"
    local base="${file##*/}"
    echo "${base%.*}"
}

FILE="/home/user/report.tar.gz"
EXT=$(get_extension "$FILE")
BASE=$(get_basename_no_ext "$FILE")
echo "Archivo: $FILE"
echo "Extensión: $EXT"
echo "Base: $BASE"
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué valor tiene <code>EXT</code>?</summary>

`gz` — `${file##*.}` elimina el prefijo más largo hasta el último `.`, dejando solo `gz`.

</details>

<details><summary>2. ¿Qué valor tiene <code>BASE</code>?</summary>

`report.tar` — primero `${file##*/}` elimina la ruta → `report.tar.gz`, luego `${base%.*}` elimina el sufijo más corto desde el último `.` → `report.tar`.

</details>

<details><summary>3. ¿El mensaje "Procesando: ..." aparece en <code>EXT</code>?</summary>

No. El `echo "Procesando: ..." >&2` va a stderr, no a stdout. `$()` solo captura stdout. El mensaje se verá en la terminal pero no contamina la variable.

</details>

---

### Ejercicio 6 — Nameref (local -n)

```bash
swap() {
    local -n ref_a=$1
    local -n ref_b=$2
    local tmp="$ref_a"
    ref_a="$ref_b"
    ref_b="$tmp"
}

X="primero"
Y="segundo"
echo "Antes: X=$X, Y=$Y"
swap X Y
echo "Después: X=$X, Y=$Y"
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué valores tienen <code>X</code> e <code>Y</code> después de <code>swap</code>?</summary>

`X=segundo`, `Y=primero` — los valores se intercambiaron. `local -n ref_a=X` hace que `ref_a` sea un alias de `X`, así que asignar a `ref_a` modifica `X` directamente.

</details>

<details><summary>2. ¿Por qué se pasa <code>X Y</code> y no <code>$X $Y</code>?</summary>

Porque `local -n` necesita el **nombre** de la variable, no su valor. Si pasaras `$X`, recibiría `"primero"` y trataría de crear una referencia a una variable llamada `primero`, que no existe.

</details>

---

### Ejercicio 7 — Funciones como filtros en pipeline

```bash
remove_blanks() {
    while IFS= read -r line; do
        [[ -n "$line" ]] && echo "$line"
    done
}

remove_comments() {
    while IFS= read -r line; do
        [[ "$line" != \#* ]] && echo "$line"
    done
}

to_upper() {
    tr '[:lower:]' '[:upper:]'
}

# Crear un archivo de prueba:
cat << 'EOF' > /tmp/config.txt
# Server config
host=localhost
port=8080

# Database
db_name=myapp

EOF

# Aplicar pipeline de filtros:
cat /tmp/config.txt | remove_comments | remove_blanks | to_upper
```

**Predicción:** antes de ejecutar, responde:

<details><summary>¿Qué produce el pipeline final?</summary>

```
HOST=LOCALHOST
PORT=8080
DB_NAME=MYAPP
```

El pipeline: (1) `remove_comments` elimina las líneas que empiezan con `#`, (2) `remove_blanks` elimina las líneas vacías, (3) `to_upper` convierte todo a mayúsculas. Las tres funciones actúan como filtros stdin→stdout.

</details>

---

### Ejercicio 8 — BASH_SOURCE guard

```bash
# Archivo: /tmp/mylib.sh
cat > /tmp/mylib.sh << 'EOF'
#!/usr/bin/env bash

greet() { echo "Hola, $1!"; }
farewell() { echo "Adiós, $1!"; }

main() {
    greet "mundo"
    farewell "mundo"
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
EOF
chmod +x /tmp/mylib.sh

# Caso A: ejecutar directamente
/tmp/mylib.sh

# Caso B: importar con source
source /tmp/mylib.sh
greet "bash"
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué imprime el Caso A?</summary>

```
Hola, mundo!
Adiós, mundo!
```

Al ejecutar directamente, `${BASH_SOURCE[0]}` y `$0` son ambos `/tmp/mylib.sh`, así que el guard es verdadero y `main` se ejecuta.

</details>

<details><summary>2. ¿Qué imprime el Caso B?</summary>

```
Hola, bash!
```

Al hacer `source`, `$0` es el script que llamó (o `bash` si es interactivo), pero `${BASH_SOURCE[0]}` es `/tmp/mylib.sh`. Como no son iguales, `main` NO se ejecuta. Solo se cargan las definiciones de funciones, y luego `greet "bash"` funciona porque la función ya está definida.

</details>

---

### Ejercicio 9 — Pasar arrays: expansión vs nameref

```bash
# Método 1: expansión con "$@"
print_via_args() {
    echo "Recibidos: $# argumentos"
    for item in "$@"; do
        echo "  [$item]"
    done
}

# Método 2: nameref
print_via_nameref() {
    local -n arr=$1
    echo "Recibidos: ${#arr[@]} elementos"
    for item in "${arr[@]}"; do
        echo "  [$item]"
    done
}

FILES=("mi archivo.txt" "otro doc.pdf" "script.sh")

echo "=== Vía \"\$@\" ==="
print_via_args "${FILES[@]}"

echo "=== Vía nameref ==="
print_via_nameref FILES

echo "=== Error común: sin comillas ==="
print_via_args ${FILES[@]}
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Cuántos argumentos recibe <code>print_via_args</code> con <code>"${FILES[@]}"</code>?</summary>

3 argumentos, preservando los espacios: `[mi archivo.txt]`, `[otro doc.pdf]`, `[script.sh]`. Las comillas dobles con `[@]` expanden cada elemento como un argumento separado, respetando espacios internos.

</details>

<details><summary>2. ¿Cuántos elementos ve <code>print_via_nameref</code>?</summary>

3 elementos — idéntico resultado. El nameref accede al array original directamente por nombre sin necesidad de expandirlo. Se pasa `FILES` (el nombre), no `"${FILES[@]}"`.

</details>

<details><summary>3. ¿Qué pasa sin comillas (<code>${FILES[@]}</code>)?</summary>

5 argumentos: `[mi]`, `[archivo.txt]`, `[otro]`, `[doc.pdf]`, `[script.sh]`. Sin comillas, word splitting divide cada elemento por espacios, rompiendo los nombres con espacios.

</details>

---

### Ejercicio 10 — Panorama: script profesional completo

```bash
#!/usr/bin/env bash
set -euo pipefail

# --- Funciones utilitarias ---
readonly PROG=$(basename "$0")

die()  { echo "$PROG: error: $*" >&2; exit 1; }
info() { echo "$PROG: $*" >&2; }

require_cmd() {
    command -v "$1" &>/dev/null || die "'$1' no está instalado"
}

# --- Funciones de negocio ---
count_lines() {
    local file="$1"
    local -n out=$2
    [[ -f "$file" ]] || { out=0; return 1; }
    out=$(wc -l < "$file")
}

format_report() {
    local -n files_ref=$1
    local total=0
    local lines

    for f in "${files_ref[@]}"; do
        if count_lines "$f" lines; then
            printf "  %-30s %5d líneas\n" "$f" "$lines"
            ((total += lines))
        else
            printf "  %-30s %s\n" "$f" "(no encontrado)"
        fi
    done
    echo "  ---"
    printf "  %-30s %5d líneas\n" "TOTAL" "$total"
}

# --- Cleanup ---
TMPFILE=""
cleanup() { [[ -n "$TMPFILE" ]] && rm -f "$TMPFILE"; }
trap cleanup EXIT

# --- Main ---
main() {
    [[ $# -ge 1 ]] || die "Uso: $PROG <archivo> [archivo...]"
    require_cmd wc

    info "Analizando $# archivo(s)"
    local -a targets=("$@")

    TMPFILE=$(mktemp)
    format_report targets > "$TMPFILE"

    echo "=== Reporte ==="
    cat "$TMPFILE"
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
```

**Predicción:** antes de ejecutar con `./script.sh /etc/passwd /etc/hostname /no/existe`, responde:

<details><summary>1. ¿Qué patrones profesionales usa este script? (nombra al menos 5)</summary>

1. **Strict mode:** `set -euo pipefail`
2. **die/info:** mensajes a stderr con nombre del programa
3. **require_cmd:** validación de dependencias al inicio
4. **cleanup + trap EXIT:** limpieza automática de archivos temporales
5. **main wrapper:** toda la lógica en `main()`, funciones auxiliares arriba
6. **BASH_SOURCE guard:** permite `source` sin ejecutar `main`
7. **Nameref:** `count_lines` retorna por referencia con `local -n`
8. **local:** todas las variables internas son locales

</details>

<details><summary>2. ¿Qué pasa con <code>/no/existe</code>?</summary>

`count_lines` detecta que no es un archivo (`[[ -f "$file" ]]` falla), asigna `out=0`, retorna 1 (fallo). `format_report` imprime `(no encontrado)` para ese archivo. El script NO falla porque `count_lines` usa `return 1`, no `exit 1`, y el `if` en `format_report` maneja el fallo.

</details>

<details><summary>3. ¿Por qué <code>count_lines</code> no causa que <code>set -e</code> termine el script?</summary>

Porque se llama dentro de un `if count_lines ...`. El `set -e` no termina el script cuando un comando falla dentro de una condición `if`, `while`, `||`, o `&&`. Esta es una excepción documentada de `set -e`.

</details>
