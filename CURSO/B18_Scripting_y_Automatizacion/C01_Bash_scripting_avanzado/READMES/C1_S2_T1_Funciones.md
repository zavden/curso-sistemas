# Funciones en Bash

## 1. Dos sintaxis, mismo resultado

```bash
# Sintaxis 1: con function keyword
function greet {
    echo "Hello, $1"
}

# Sintaxis 2: con paréntesis (POSIX compatible, preferida)
greet() {
    echo "Hello, $1"
}

# Ambas se llaman igual:
greet "World"    # Hello, World
```

**Preferir la sintaxis con `()`**: es compatible con POSIX `sh`, más portable,
y es la convención dominante.

Las funciones en Bash son **esencialmente aliases de bloques de código**. No
tienen firma, no declaran parámetros, no tienen tipos de retorno. Son mucho
más primitivas que funciones en C o Rust.

---

## 2. Argumentos: $1, $2, ..., $@

Los argumentos de función usan las mismas variables que los argumentos de
script, pero **locales a la invocación**:

```bash
describe() {
    echo "Name: $1"
    echo "Age: $2"
    echo "All args: $@"
    echo "Num args: $#"
    echo "Script name: $0"   # ¡CUIDADO! $0 es el script, NO la función
}

describe "Alice" 30 "extra"
# Name: Alice
# Age: 30
# All args: Alice 30 extra
# Num args: 3
# Script name: ./myscript.sh
```

```
Variables dentro de una función:
$0    → nombre del script (NO de la función)
$1    → primer argumento de la función
$2    → segundo argumento
$@    → todos los argumentos (preserva separación)
$*    → todos los argumentos (un solo string con IFS)
$#    → número de argumentos
```

### Acceder a argumentos por nombre (pseudo-named args)

```bash
create_user() {
    local name="$1"
    local email="$2"
    local role="${3:-user}"   # default: "user"

    echo "Creating $name ($email) as $role"
}

create_user "Alice" "alice@example.com" "admin"
create_user "Bob" "bob@example.com"            # role = "user" (default)
```

### Validar número de argumentos

```bash
backup() {
    if (( $# < 2 )); then
        echo "Usage: backup <source> <dest>" >&2
        return 1
    fi
    local src="$1" dest="$2"
    cp -r "$src" "$dest"
}
```

---

## 3. local: scope de variables

**Sin `local`, las variables son globales** — una de las fuentes de bugs más
comunes en Bash:

```bash
x=10

bad_function() {
    x=99    # ¡modifica la x global!
}

good_function() {
    local x=99   # solo existe dentro de esta función
}

bad_function
echo "$x"   # 99 (¡la global fue modificada!)

x=10
good_function
echo "$x"   # 10 (la global está intacta)
```

### Dynamic scoping

Bash usa **dynamic scoping**, no lexical scoping como C o Rust:

```bash
outer() {
    local x=10
    inner
}

inner() {
    echo "x from outer: $x"   # ¡PUEDE ver la x de outer!
}

outer   # x from outer: 10
```

```
Dynamic scoping:                  Lexical scoping (C/Rust):
inner() ve las locals de          inner() solo ve sus propias
quien la LLAMÓ.                   locals y las globales.

outer() { local x=10; inner; }   Esto NO pasaría en C/Rust.
inner() { echo $x; }  → 10       inner() no tendría acceso a x.
```

**Consecuencia práctica**: nombra tus variables locales de forma que no
colisionen con funciones que llames. O mejor: usa `local` en **todo**.

### local en todos los niveles

```bash
process() {
    local file="$1"
    local line_count
    local result

    line_count=$(wc -l < "$file")
    result="File $file has $line_count lines"
    echo "$result"
}
# Ninguna variable contamina el scope global
```

**Regla**: declarar **toda** variable dentro de funciones como `local`, excepto
cuando explícitamente quieres modificar una global.

---

## 4. return vs echo: dos formas de "retornar"

### 4.1 return: exit code (0-255)

```bash
is_even() {
    local n=$1
    if (( n % 2 == 0 )); then
        return 0    # success = true
    else
        return 1    # failure = false
    fi
}

if is_even 42; then
    echo "42 is even"      # ✓ esto se imprime
fi

if is_even 7; then
    echo "7 is even"
else
    echo "7 is odd"        # ✓ esto se imprime
fi
```

`return` solo puede retornar un entero 0-255. Es un **exit code**, no un
valor de retorno como en C/Rust.

```bash
# ¡ERROR LÓGICO!
add() {
    return $(( $1 + $2 ))
}
add 200 100    # return 300 → truncado a 300 % 256 = 44
echo $?        # 44 (¡no 300!)
```

### 4.2 echo: retornar datos via stdout

```bash
add() {
    echo $(( $1 + $2 ))
}

result=$(add 200 100)
echo "$result"    # 300 ✓
```

La forma idiomática de "retornar un valor" en Bash:
1. La función **imprime** el resultado a stdout con `echo`/`printf`
2. El caller **captura** con `$(...)`

```bash
get_extension() {
    local file="$1"
    echo "${file##*.}"
}

ext=$(get_extension "report.tar.gz")
echo "Extension: $ext"    # gz
```

### 4.3 Combinar ambos: dato + status

```bash
find_user() {
    local name="$1"
    local result
    result=$(grep "^$name:" /etc/passwd 2>/dev/null)

    if [[ -z "$result" ]]; then
        return 1    # not found (status)
    fi

    echo "$result"  # user data (stdout)
    return 0        # found (status)
}

if user_data=$(find_user "root"); then
    echo "Found: $user_data"
else
    echo "User not found"
fi
```

**Patrón**: `echo` para datos, `return` para success/failure. El caller
usa `if var=$(func); then ...` para capturar ambos.

---

## 5. Retornar múltiples valores

Bash no tiene tuplas ni structs. Estrategias para retornar múltiples valores:

### 5.1 Stdout con delimitador

```bash
get_dimensions() {
    local file="$1"
    # Simulado:
    echo "1920 1080"
}

read -r width height <<< "$(get_dimensions "image.png")"
echo "Width: $width, Height: $height"    # Width: 1920, Height: 1080
```

### 5.2 Variables globales (explícitas)

```bash
parse_url() {
    local url="$1"
    # Asignar a variables globales con nombres documentados
    PARSE_HOST="${url#*://}"
    PARSE_HOST="${PARSE_HOST%%/*}"
    PARSE_PATH="/${url#*://*/}"
}

parse_url "https://example.com/api/users"
echo "Host: $PARSE_HOST"   # example.com
echo "Path: $PARSE_PATH"   # /api/users
```

### 5.3 Nameref para output parameters (Bash 4.3+)

```bash
divide() {
    local dividend=$1
    local divisor=$2
    local -n out_quotient=$3    # nameref: alias al nombre pasado
    local -n out_remainder=$4

    out_quotient=$(( dividend / divisor ))
    out_remainder=$(( dividend % divisor ))
}

divide 17 5 q r
echo "17 / 5 = $q remainder $r"    # 17 / 5 = 3 remainder 2
```

**Cuidado con namerefs**: si el nombre del parámetro coincide con la variable
local de la función, se crea una referencia circular:

```bash
bad() {
    local -n result=$1
    local temp=42
    result=$temp
}

bad result    # ¡"result" es tanto el nameref como la variable externa!
              # Bash 4: se confunde. Bash 5: warning.
```

Solución: usar nombres internos que no colisionen (prefijo `_` o `__`):

```bash
safe() {
    local -n _result=$1
    local _temp=42
    _result=$_temp
}
```

---

## 6. Pasar arrays a funciones

### 6.1 Por expansión (como argumentos)

```bash
sum_all() {
    local total=0
    for n in "$@"; do
        (( total += n ))
    done
    echo "$total"
}

numbers=(10 20 30 40)
result=$(sum_all "${numbers[@]}")
echo "Sum: $result"    # Sum: 100
```

El array se expande en argumentos posicionales. La función no sabe que era
un array — solo ve `$1=10, $2=20, $3=30, $4=40`.

### 6.2 Por nameref

```bash
print_array() {
    local -n arr=$1
    echo "Length: ${#arr[@]}"
    for item in "${arr[@]}"; do
        echo "  - $item"
    done
}

fruits=("apple" "banana" "cherry")
print_array fruits     # nombre, sin $ ni [@]
```

### 6.3 Modificar un array via nameref

```bash
append_if_missing() {
    local -n target=$1
    local value="$2"

    for existing in "${target[@]}"; do
        [[ "$existing" == "$value" ]] && return 0
    done

    target+=("$value")
}

tags=("linux" "bash")
append_if_missing tags "python"
append_if_missing tags "bash"        # ya existe, no añade
echo "${tags[@]}"   # linux bash python
```

---

## 7. Funciones como librería: source

`source` (o `.`) ejecuta un archivo en el **contexto actual del shell**,
haciendo disponibles sus funciones y variables:

### 7.1 Crear una librería

```bash
# lib/utils.sh
#!/bin/bash
# No ejecutar directamente — diseñado para ser sourced

log_info() {
    echo "[INFO] $(date +%H:%M:%S) $*" >&2
}

log_error() {
    echo "[ERROR] $(date +%H:%M:%S) $*" >&2
}

is_root() {
    (( EUID == 0 ))
}

require_root() {
    if ! is_root; then
        log_error "This script must be run as root"
        exit 1
    fi
}
```

### 7.2 Usar la librería

```bash
#!/bin/bash
# deploy.sh

# Ubicar la librería relativa al script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/lib/utils.sh"

log_info "Starting deployment"

require_root

log_info "Deploying..."
# ...
log_info "Done"
```

### 7.3 Guard contra ejecución directa

```bash
# lib/utils.sh
# Si se ejecuta directamente (no sourced), mostrar ayuda
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    echo "This file is meant to be sourced, not executed."
    echo "Usage: source ${BASH_SOURCE[0]}"
    exit 1
fi
```

```
${BASH_SOURCE[0]} = el archivo donde está ESTA línea
$0                = el script que se está ejecutando

Si son iguales → se ejecutó directamente (./lib/utils.sh)
Si difieren    → fue sourced desde otro script
```

### 7.4 Patrón: source condicional

```bash
# Solo cargar si no está ya cargado (evitar re-source)
if ! declare -f log_info >/dev/null 2>&1; then
    source "$SCRIPT_DIR/lib/utils.sh"
fi
```

---

## 8. Recursión

Bash soporta recursión, pero sin optimización de tail call y con un stack
limitado:

### 8.1 Factorial

```bash
factorial() {
    local n=$1
    if (( n <= 1 )); then
        echo 1
    else
        local sub
        sub=$(factorial $((n - 1)))
        echo $(( n * sub ))
    fi
}

factorial 5    # 120
factorial 10   # 3628800
```

### 8.2 Recorrer directorios recursivamente

```bash
find_files() {
    local dir="$1"
    local pattern="$2"
    local indent="${3:-}"

    for entry in "$dir"/*; do
        [[ -e "$entry" ]] || continue    # skip si glob no expandió

        local name="${entry##*/}"

        if [[ -d "$entry" ]]; then
            echo "${indent}[DIR] $name/"
            find_files "$entry" "$pattern" "  $indent"
        elif [[ "$name" == $pattern ]]; then
            echo "${indent}[FILE] $name"
        fi
    done
}

find_files /etc "*.conf" ""
```

### 8.3 Límites de recursión

```bash
# Bash tiene un límite de recursión (por defecto ~1000 niveles)
# Depende del tamaño del stack del proceso

recurse() {
    local depth=$1
    echo "Depth: $depth"
    recurse $((depth + 1))
}

# recurse 0    # Eventualmente: segfault o "maximum recursion depth exceeded"
```

**Regla práctica**: si la recursión puede exceder ~100 niveles, convertir a
un loop con un stack explícito (array como stack). Los mismos principios del
tópico de iteración sobre composites (B16 C03 S03 T03) aplican aquí.

```bash
# Iterativo con stack (equivalente a find_files recursivo):
find_files_iter() {
    local pattern="$2"
    local stack=("$1")

    while (( ${#stack[@]} > 0 )); do
        local dir="${stack[-1]}"
        unset 'stack[-1]'

        for entry in "$dir"/*; do
            [[ -e "$entry" ]] || continue
            if [[ -d "$entry" ]]; then
                stack+=("$entry")
            elif [[ "${entry##*/}" == $pattern ]]; then
                echo "$entry"
            fi
        done
    done
}
```

---

## 9. Funciones y subshells

Un error conceptual común: `$(...)` ejecuta en un **subshell**. Los cambios
dentro no afectan al shell padre:

```bash
counter=0

increment() {
    (( counter++ ))
}

# Esto funciona (mismo proceso):
increment
echo "$counter"    # 1

# Esto NO funciona (subshell):
echo "hello" | while read -r line; do
    increment
done
echo "$counter"    # 1 (¡no 2! el while en pipe corre en subshell)
```

```
Shell principal:  counter=0  →  increment  →  counter=1
                                                  │
Pipe crea subshell: ─────────────────────────────┐
│ counter=1 (copia)  →  increment  →  counter=2  │
│ (este 2 se descarta cuando el subshell muere)   │
└─────────────────────────────────────────────────┘
                                                  │
Shell principal:  counter=1  (sin cambio)    ←────┘
```

### Soluciones

```bash
# Solución 1: process substitution en vez de pipe
counter=0
while read -r line; do
    (( counter++ ))
done < <(echo -e "a\nb\nc")
echo "$counter"    # 3 ✓ (while corre en shell principal)

# Solución 2: lastpipe (Bash 4.2+, solo en scripts no-interactivos)
shopt -s lastpipe
counter=0
echo -e "a\nb\nc" | while read -r line; do
    (( counter++ ))
done
echo "$counter"    # 3 ✓ (último comando del pipe corre en shell actual)
```

---

## 10. Funciones y redirección

Las funciones se comportan como comandos — puedes redirigir su I/O:

```bash
generate_report() {
    echo "=== Report ==="
    echo "Date: $(date)"
    echo "User: $USER"
    echo "Host: $(hostname)"
}

# Redirigir output a archivo
generate_report > /tmp/report.txt

# Redirigir solo stderr
log_info() {
    echo "[INFO] $*" >&2   # explícitamente a stderr
}

# Redirigir la función entera
verbose_operation() {
    echo "stdout output"
    echo "stderr output" >&2
} 2>/dev/null    # solo suprime stderr de esta función
```

### Función como filtro (stdin → stdout)

```bash
to_upper() {
    while IFS= read -r line; do
        echo "${line^^}"
    done
}

echo "hello world" | to_upper          # HELLO WORLD
cat file.txt | to_upper > upper.txt    # archivo completo a mayúsculas
```

---

## 11. Patrones avanzados

### 11.1 Función con cleanup (usando trap local)

```bash
safe_download() {
    local url="$1"
    local dest="$2"
    local tmpfile

    tmpfile=$(mktemp) || return 1
    # Cleanup: borrar tmpfile cuando la función termine
    trap "rm -f '$tmpfile'" RETURN

    if curl -sS -o "$tmpfile" "$url"; then
        mv "$tmpfile" "$dest"
        return 0
    else
        return 1   # trap RETURN limpia el tmpfile automáticamente
    fi
}
```

`trap ... RETURN` se ejecuta cuando la función retorna, por cualquier camino.

### 11.2 Dispatch table (funciones como first-class)

```bash
cmd_start() { echo "Starting service..."; }
cmd_stop() { echo "Stopping service..."; }
cmd_restart() { cmd_stop; cmd_start; }
cmd_status() { echo "Service is running"; }

main() {
    local command="${1:-}"
    local func="cmd_$command"

    # Verificar que la función existe
    if declare -f "$func" > /dev/null 2>&1; then
        "$func" "${@:2}"   # llamar la función, pasar args restantes
    else
        echo "Unknown command: $command" >&2
        echo "Available: start, stop, restart, status" >&2
        return 1
    fi
}

main "$@"
```

```bash
./service.sh start     # Starting service...
./service.sh status    # Service is running
./service.sh foo       # Unknown command: foo
```

### 11.3 Higher-order: funciones que reciben funciones

```bash
retry() {
    local max_attempts=$1
    shift   # ahora "$@" contiene el comando a reintentar

    local attempt=1
    while (( attempt <= max_attempts )); do
        echo "Attempt $attempt/$max_attempts..."
        if "$@"; then
            return 0
        fi
        (( attempt++ ))
        sleep 1
    done
    return 1
}

flaky_operation() {
    (( RANDOM % 3 == 0 ))   # falla 2/3 de las veces
}

retry 5 flaky_operation
```

---

## 12. FUNCNAME, BASH_SOURCE, BASH_LINENO

Bash mantiene arrays especiales para introspección del call stack:

```bash
debug_info() {
    echo "Function: ${FUNCNAME[0]}"         # la función actual
    echo "Called by: ${FUNCNAME[1]}"         # quien la llamó
    echo "Source file: ${BASH_SOURCE[0]}"    # archivo de la función
    echo "Line: ${BASH_LINENO[0]}"          # línea donde fue llamada
}

wrapper() {
    debug_info
}

wrapper
# Function: debug_info
# Called by: wrapper
# Source file: ./script.sh
# Line: 8 (aprox)
```

### Stack trace para debugging

```bash
stacktrace() {
    local i=0
    echo "Stack trace:"
    while caller $i; do
        (( i++ ))
    done
}

# O más detallado:
full_trace() {
    echo "=== Stack Trace ==="
    for (( i=0; i < ${#FUNCNAME[@]}; i++ )); do
        echo "  ${FUNCNAME[$i]}() at ${BASH_SOURCE[$i]}:${BASH_LINENO[$i]}"
    done
}
```

---

## 13. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| Olvidar `local` | Variables contaminan scope global | `local` en toda variable de función |
| `return` con valor > 255 | Truncamiento silencioso (módulo 256) | `echo` para datos, `return` solo 0/1 |
| Capturar output + exit code mal | `$()` crea subshell, `$?` cambia | `if result=$(func); then` |
| Recursión sin caso base | Stack overflow → segfault | Siempre verificar condición de parada |
| Modificar variable en pipe subshell | Cambios se pierden | Process substitution `< <(cmd)` |
| Nameref colisión de nombre | Referencia circular, comportamiento raro | Prefijo `_` en variables internas |
| `$0` para nombre de función | `$0` siempre es el script | `${FUNCNAME[0]}` |
| `source` archivo inexistente | Error silencioso (sin `set -e`) | `source file \|\| exit 1` |
| Función y comando mismo nombre | La función tiene prioridad | `command cmd` fuerza el comando externo |
| `echo` dentro de función que retorna datos | Output mezclado | Solo `echo` el resultado, `>&2` para logs |

---

## 14. Ejercicios

### Ejercicio 1 — Scope de variables

**Predicción**: ¿qué imprime?

```bash
x=global

modify_bad() {
    x=modified
}

modify_good() {
    local x=local_only
    echo "Inside: $x"
}

modify_bad
echo "After bad: $x"

modify_good
echo "After good: $x"
```

<details>
<summary>Respuesta</summary>

```
After bad: modified
Inside: local_only
After good: modified
```

- `modify_bad` cambia `x` global porque no usa `local` → `x=modified`
- `modify_good` crea `x` local, imprime "local_only", pero no toca la global
- Después de `modify_good`, la global sigue siendo "modified" (lo que dejó `modify_bad`)

Sin `local`, toda asignación en una función modifica el scope del caller
(o el global). Es uno de los bugs más comunes en Bash.
</details>

---

### Ejercicio 2 — return vs echo

**Predicción**: ¿qué imprime?

```bash
bad_add() {
    return $(( $1 + $2 ))
}

good_add() {
    echo $(( $1 + $2 ))
}

bad_add 200 100
echo "Bad: $?"

result=$(good_add 200 100)
echo "Good: $result"
```

<details>
<summary>Respuesta</summary>

```
Bad: 44
Good: 300
```

- `bad_add`: return 300 → truncado a 300 % 256 = 44 (return solo acepta 0-255)
- `good_add`: echo "300" → capturado por `$(...)` → result=300

`return` es un exit code, no un valor de retorno. Para retornar datos,
siempre usar `echo`/`printf` y capturar con `$()`.
</details>

---

### Ejercicio 3 — Dynamic scoping

**Predicción**: ¿qué imprime?

```bash
outer() {
    local msg="from outer"
    middle
}

middle() {
    inner
}

inner() {
    echo "$msg"
}

msg="global"
outer
inner
```

<details>
<summary>Respuesta</summary>

```
from outer
global
```

- Primera llamada: `outer` declara `local msg="from outer"`, llama `middle`,
  que llama `inner`. `inner` busca `msg` en su scope → no tiene → busca en
  `middle` → no tiene → busca en `outer` → `"from outer"` ✓

- Segunda llamada: `inner` directamente. Busca `msg` en su scope → no tiene →
  busca en el scope global → `"global"` ✓

Esto es **dynamic scoping**: la resolución de variables sigue el **call stack**,
no el lugar donde la función está definida (eso sería lexical scoping).
</details>

---

### Ejercicio 4 — Pipe y subshell

**Predicción**: ¿qué imprime?

```bash
count=0

echo -e "a\nb\nc" | while read -r line; do
    (( count++ ))
done
echo "Pipe: $count"

count=0
while read -r line; do
    (( count++ ))
done < <(echo -e "a\nb\nc")
echo "Proc sub: $count"
```

<details>
<summary>Respuesta</summary>

```
Pipe: 0
Proc sub: 3
```

- **Pipe**: el `while` corre en un **subshell**. `count` se incrementa
  dentro del subshell (copia), pero esos cambios se pierden al terminar.
  La `count` del shell principal sigue en 0.

- **Process substitution**: `< <(...)` alimenta el stdin del `while` que
  corre en el **shell principal**. `count` se modifica directamente.
  Al terminar, count = 3.

Regla: si necesitas que un while-read modifique variables del script,
usar `< <(command)` en vez de `command | while`.
</details>

---

### Ejercicio 5 — Source guard

**Predicción**: ¿qué pasa al ejecutar y al sourcear?

```bash
# lib.sh
say_hello() {
    echo "Hello from lib!"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    echo "Don't run me directly!"
    exit 1
fi
```

```bash
# Caso A:
./lib.sh

# Caso B:
source lib.sh
say_hello
```

<details>
<summary>Respuesta</summary>

**Caso A** (ejecución directa):
```
Don't run me directly!
```
Y el script termina con exit code 1.
- `BASH_SOURCE[0]` = `./lib.sh`, `$0` = `./lib.sh` → son iguales → entra al if

**Caso B** (source):
```
Hello from lib!
```
- `BASH_SOURCE[0]` = `lib.sh`, `$0` = `./main_script.sh` (o `bash`) → difieren → no entra al if
- La función `say_hello` queda disponible en el shell actual
- Se llama y funciona correctamente

Este patrón es el equivalente Bash de `if __name__ == "__main__"` en Python.
</details>

---

### Ejercicio 6 — Dispatch table

**Predicción**: ¿qué imprime cada invocación?

```bash
cmd_hello() { echo "Hello, ${1:-World}!"; }
cmd_bye()   { echo "Goodbye!"; }

dispatch() {
    local func="cmd_$1"
    if declare -f "$func" > /dev/null 2>&1; then
        "$func" "${@:2}"
    else
        echo "Unknown: $1" >&2
        return 1
    fi
}

dispatch hello
dispatch hello "Alice"
dispatch bye
dispatch unknown
```

<details>
<summary>Respuesta</summary>

```
Hello, World!
Hello, Alice!
Goodbye!
Unknown: unknown
```

- `dispatch hello` → busca `cmd_hello` → existe → `cmd_hello` sin args → default "World"
- `dispatch hello "Alice"` → `cmd_hello "Alice"` → `$1=Alice`
- `dispatch bye` → `cmd_bye` → imprime "Goodbye!"
- `dispatch unknown` → busca `cmd_unknown` → `declare -f` falla → "Unknown: unknown" a stderr

`${@:2}` es slicing de `$@`: todos los argumentos desde el segundo en adelante.
`declare -f name` retorna 0 si la función existe, 1 si no.
</details>

---

### Ejercicio 7 — Nameref para output

**Predicción**: ¿qué imprime?

```bash
split_path() {
    local path="$1"
    local -n _dir=$2
    local -n _file=$3

    _dir="${path%/*}"
    _file="${path##*/}"
}

split_path "/var/log/syslog" my_dir my_file
echo "Dir: $my_dir"
echo "File: $my_file"
```

<details>
<summary>Respuesta</summary>

```
Dir: /var/log
File: syslog
```

- `local -n _dir=$2` → `_dir` es alias de `my_dir` (la variable del caller)
- `local -n _file=$3` → `_file` es alias de `my_file`
- `_dir="${path%/*}"` → asigna "/var/log" directamente a `my_dir`
- `_file="${path##*/}"` → asigna "syslog" directamente a `my_file`

Es como pasar punteros a variables en C. El prefijo `_` evita colisiones
entre los namerefs y las variables del caller.
</details>

---

### Ejercicio 8 — Retry con función

**Predicción**: ¿cuál es el output posible de este script?

```bash
attempt=0

maybe_fail() {
    (( attempt++ ))
    if (( attempt < 3 )); then
        echo "Attempt $attempt: failed" >&2
        return 1
    fi
    echo "Success on attempt $attempt"
    return 0
}

retry() {
    local max=$1; shift
    local i
    for (( i=1; i<=max; i++ )); do
        if "$@"; then
            return 0
        fi
        sleep 0.1
    done
    return 1
}

retry 5 maybe_fail
```

<details>
<summary>Respuesta</summary>

```
Attempt 1: failed
Attempt 2: failed
Success on attempt 3
```

`retry` llama `maybe_fail` hasta 5 veces. `maybe_fail` usa la variable
global `attempt` (no local) como contador:

- Intento 1: attempt=1, < 3 → fail → return 1 → retry continúa
- Intento 2: attempt=2, < 3 → fail → return 1 → retry continúa
- Intento 3: attempt=3, no < 3 → echo "Success..." → return 0 → retry retorna 0

Los mensajes de error van a `>&2` (stderr), el resultado a stdout.
`retry` solo ve el exit code de `maybe_fail`, no su output.

Nota: `attempt` es global intencionalmente — `maybe_fail` necesita persistir
estado entre llamadas. Si fuera `local`, se reiniciaría en cada invocación.
</details>

---

### Ejercicio 9 — Función como filtro

**Predicción**: ¿qué imprime?

```bash
number_lines() {
    local n=1
    while IFS= read -r line; do
        printf "%3d: %s\n" "$n" "$line"
        (( n++ ))
    done
}

echo -e "hello\nworld\nfoo" | number_lines
```

<details>
<summary>Respuesta</summary>

```
  1: hello
  2: world
  3: foo
```

`number_lines` lee de stdin (pipe), procesa línea por línea, y escribe a
stdout con número de línea formateado. Es una función que actúa como filtro
Unix (como `cat -n` simplificado).

- `IFS=` evita que se eliminen espacios iniciales/finales
- `read -r` evita que backslashes se interpreten como escapes
- `printf "%3d"` formatea el número con padding de 3 caracteres

Este patrón (while-read + transformación + printf/echo) es la forma de
escribir filtros de texto en Bash.
</details>

---

### Ejercicio 10 — Función en subshell vs shell actual

**Predicción**: ¿qué imprime?

```bash
items=()

add_item() {
    items+=("$1")
}

add_item "direct"
echo "After direct: ${#items[@]}"

echo "trigger" | while read -r _; do
    add_item "in_pipe"
done
echo "After pipe: ${#items[@]}"

while read -r _; do
    add_item "in_procsub"
done < <(echo "trigger")
echo "After procsub: ${#items[@]}"
```

<details>
<summary>Respuesta</summary>

```
After direct: 1
After pipe: 1
After procsub: 2
```

- **direct**: `add_item "direct"` corre en el shell principal → items tiene 1 elemento
- **pipe**: el `while` corre en subshell → `add_item "in_pipe"` modifica una
  **copia** de items → la copia se descarta → items sigue con 1 elemento
- **procsub**: el `while` corre en el shell principal → `add_item "in_procsub"`
  modifica items directamente → items tiene 2 elementos: "direct" e "in_procsub"

"in_pipe" se añadió a la copia del subshell y se perdió. Este es el bug más
sutil y frecuente en Bash scripting.
</details>