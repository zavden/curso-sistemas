# Lab — Funciones

## Objetivo

Dominar funciones en bash: definicion y sintaxis, argumentos
($1, $@, $#), scope con local, return vs exit, patrones de retorno
(echo + $(), nameref con local -n), y funciones profesionales
(die/log/require_cmd, main wrapper).

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Definicion y argumentos

### Objetivo

Crear funciones, pasar argumentos, y entender el scope de
variables.

### Paso 1.1: Sintaxis basica

```bash
docker compose exec debian-dev bash -c '
echo "=== Definicion de funciones ==="
echo ""
echo "--- Sintaxis bash ---"
greet() {
    echo "Hola, $1"
}

echo "--- Sintaxis POSIX (con function keyword) ---"
# Ambas son equivalentes en bash:
# greet() { ... }
# function greet { ... }
# function greet() { ... }   ← menos comun

greet "mundo"
greet "bash"
echo ""
echo "--- Argumentos: \$1, \$2, ..., \$@, \$# ---"
show_args() {
    echo "  Numero de argumentos: $#"
    echo "  Primer argumento: $1"
    echo "  Segundo argumento: $2"
    echo "  Todos: $@"
}

show_args "uno" "dos" "tres"
echo ""
echo "Los argumentos de la funcion son independientes"
echo "de los argumentos del script (\$1 del script != \$1 de la funcion)"
'
```

### Paso 1.2: local — scope de variables

Antes de ejecutar, predice: si una funcion modifica una variable
global sin `local`, que pasa con el valor en el shell?

```bash
docker compose exec debian-dev bash -c '
echo "=== local — scope de variables ==="
echo ""
echo "--- Sin local: modifica la variable global ---"
X="global"
modify_global() {
    X="modificado por funcion"
}
echo "Antes: X=$X"
modify_global
echo "Despues: X=$X (cambio!)"
echo ""
echo "--- Con local: no afecta al exterior ---"
Y="global"
modify_local() {
    local Y="solo en la funcion"
    echo "  Dentro: Y=$Y"
}
echo "Antes: Y=$Y"
modify_local
echo "Despues: Y=$Y (no cambio)"
echo ""
echo "--- REGLA: siempre usar local ---"
proper_function() {
    local result=""
    local i
    for i in "$@"; do
        result+="$i "
    done
    echo "$result"
}
proper_function "a" "b" "c"
echo ""
echo "SIEMPRE declarar variables con local dentro de funciones"
echo "Excepto cuando intencionalmente se quiere modificar el scope exterior"
'
```

### Paso 1.3: Pasar arrays a funciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Pasar arrays a funciones ==="
echo ""
echo "--- Los arrays se pasan expandidos como argumentos ---"
print_items() {
    echo "  Recibidos: $# argumentos"
    for item in "$@"; do
        echo "    - $item"
    done
}

FRUITS=("red apple" "green banana" "yellow cherry")
echo "Array con elementos con espacios:"
print_items "${FRUITS[@]}"
echo ""
echo "--- Pasar como argumentos separados: \"\${arr[@]}\" ---"
echo "--- NO pasar como: \${arr[@]} (rompe con espacios) ---"
echo "--- NO pasar como: \"\${arr[*]}\" (un solo string) ---"
'
```

---

## Parte 2 — Retorno y nameref

### Objetivo

Entender las formas de retornar valores desde funciones.

### Paso 2.1: return vs exit

```bash
docker compose exec debian-dev bash -c '
echo "=== return vs exit ==="
echo ""
echo "--- return: sale de la funcion con un exit code (0-255) ---"
is_even() {
    (( $1 % 2 == 0 )) && return 0
    return 1
}

for n in 2 3 4 7; do
    if is_even "$n"; then
        echo "  $n es par"
    else
        echo "  $n es impar"
    fi
done
echo ""
echo "--- exit: sale del SCRIPT completo ---"
echo "NUNCA usar exit dentro de una funcion a menos que"
echo "sea intencionalmente para terminar todo el script"
echo ""
echo "--- return sin argumento = exit code del ultimo comando ---"
last_command_status() {
    grep -q "root" /etc/passwd
    return   # retorna el exit code de grep
}
last_command_status
echo "Exit code: $?"
'
```

### Paso 2.2: Patron echo + $() para retornar strings

```bash
docker compose exec debian-dev bash -c '
echo "=== echo + \$() para retornar strings ==="
echo ""
echo "return solo retorna numeros (0-255)"
echo "Para strings, la funcion hace echo y el caller captura con \$()"
echo ""
get_extension() {
    local file="$1"
    echo "${file##*.}"
}

get_basename() {
    local file="$1"
    local base="${file##*/}"
    echo "${base%.*}"
}

FILE="/home/user/report.tar.gz"
EXT=$(get_extension "$FILE")
BASE=$(get_basename "$FILE")
echo "Archivo: $FILE"
echo "Extension: $EXT"
echo "Basename: $BASE"
echo ""
echo "--- CUIDADO con echo dentro de la funcion ---"
bad_function() {
    echo "Mensaje de debug"    # esto contamina el retorno!
    echo "valor_real"
}
RESULT=$(bad_function)
echo "Resultado: $RESULT"
echo "(incluye el mensaje de debug — MAL)"
echo ""
echo "Solucion: debug a stderr, retorno a stdout"
good_function() {
    echo "Mensaje de debug" >&2    # stderr, no se captura
    echo "valor_real"              # stdout, se captura con $()
}
RESULT=$(good_function)
echo "Resultado: $RESULT"
echo "(solo el valor real — BIEN)"
'
```

### Paso 2.3: Nameref — local -n

```bash
docker compose exec debian-dev bash -c '
echo "=== Nameref — local -n (bash 4.3+) ==="
echo ""
echo "local -n crea una referencia a otra variable"
echo "Permite que la funcion modifique una variable del caller"
echo ""
get_file_info() {
    local file="$1"
    local -n out_lines=$2      # referencia a la variable del caller
    local -n out_size=$3

    out_lines=$(wc -l < "$file")
    out_size=$(wc -c < "$file")
}

LINES=0
SIZE=0
get_file_info /etc/passwd LINES SIZE
echo "/etc/passwd: $LINES lineas, $SIZE bytes"
echo ""
echo "--- Tambien para arrays ---"
fill_array() {
    local -n arr=$1
    arr+=("primero")
    arr+=("segundo")
    arr+=("tercero")
}

declare -a ITEMS=()
fill_array ITEMS
echo "Items: ${ITEMS[*]}"
echo ""
echo "Nota: se pasa el NOMBRE de la variable (sin \$)"
echo "  fill_array ITEMS   ← nombre"
echo "  fill_array \$ITEMS  ← valor (INCORRECTO)"
'
```

---

## Parte 3 — Patrones profesionales

### Objetivo

Aplicar patrones de funciones usados en scripts de produccion.

### Paso 3.1: die / log / require_cmd

```bash
docker compose exec debian-dev bash -c '
echo "=== Patrones: die, log, require_cmd ==="
echo ""
cat > /tmp/patterns.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_NAME=$(basename "$0")

# --- Funciones utilitarias ---
die()  { echo "$SCRIPT_NAME: error: $*" >&2; exit 1; }
warn() { echo "$SCRIPT_NAME: warning: $*" >&2; }
info() { echo "$SCRIPT_NAME: $*" >&2; }

require_cmd() {
    local cmd="$1"
    command -v "$cmd" &>/dev/null || die "$cmd no encontrado (instalar con: $2)"
}

# --- Validaciones ---
require_cmd "bash" "apt install bash"
require_cmd "grep" "apt install grep"
require_cmd "programa_inexistente" "apt install programa_inexistente" 2>&1 || true

info "Todas las dependencias verificadas"
SCRIPT
chmod +x /tmp/patterns.sh
/tmp/patterns.sh 2>&1 || true
echo ""
echo "die(): error fatal → stderr + exit 1"
echo "warn(): advertencia → stderr, no sale"
echo "info(): informativo → stderr"
echo "require_cmd(): verifica que un comando existe"
'
```

### Paso 3.2: Main wrapper

```bash
docker compose exec debian-dev bash -c '
echo "=== Main wrapper ==="
echo ""
cat > /tmp/main-wrapper.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

setup() {
    echo "  setup: preparando entorno"
}

process() {
    local file="$1"
    echo "  process: procesando $file"
}

cleanup() {
    echo "  cleanup: limpiando"
}

main() {
    echo "Inicio"
    setup
    for f in "$@"; do
        process "$f"
    done
    cleanup
    echo "Fin"
}

# Solo ejecutar main si el script se ejecuta directamente
# (no si se importa con source)
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
SCRIPT
chmod +x /tmp/main-wrapper.sh
echo "--- Ejecutar directamente ---"
/tmp/main-wrapper.sh archivo1.txt archivo2.txt
echo ""
echo "--- Source (no ejecuta main) ---"
source /tmp/main-wrapper.sh
echo "Despues de source: las funciones estan disponibles"
echo "pero main no se ejecuto"
echo ""
echo "Este patron permite:"
echo "  1. Testear funciones individuales con source"
echo "  2. Reusar funciones en otros scripts"
echo "  3. Ejecutar normalmente con ./script.sh"
'
```

### Paso 3.3: Funciones con pipeline y redireccion

```bash
docker compose exec debian-dev bash -c '
echo "=== Funciones en pipelines ==="
echo ""
to_upper() {
    tr "[:lower:]" "[:upper:]"
}

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

echo "--- Componer funciones con pipes ---"
echo -e "hello\nworld\nbash" | to_upper | add_prefix ">>> " | number_lines
echo ""
echo "--- Funcion como filtro ---"
echo "Las funciones que leen de stdin y escriben a stdout"
echo "se componen naturalmente con pipes"
echo ""
echo "--- Redirigir salida de una funcion ---"
generate_report() {
    echo "Hostname: $(hostname)"
    echo "Date: $(date)"
    echo "Users: $(wc -l < /etc/passwd)"
}
generate_report > /tmp/report.txt
echo "Reporte guardado:"
cat /tmp/report.txt
rm /tmp/report.txt
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/patterns.sh /tmp/main-wrapper.sh
echo "Archivos temporales eliminados"
'
```

---

## Conceptos reforzados

1. Las funciones reciben argumentos en `$1`, `$2`, `$@`, `$#` —
   independientes de los argumentos del script. Siempre usar
   `"$@"` (con comillas) para pasar argumentos a otros comandos.

2. **Siempre usar `local`** para variables dentro de funciones.
   Sin `local`, las variables son globales y pueden causar bugs
   dificiles de diagnosticar.

3. `return` sale de la funcion con un exit code (0-255). `exit`
   sale del **script completo**. Para retornar strings, usar
   `echo` y capturar con `$()`.

4. **Nameref** (`local -n ref=$1`) permite que una funcion
   modifique variables del caller por referencia. Se pasa el
   **nombre** de la variable, no su valor.

5. `die()`, `warn()`, `require_cmd()` son patrones estandar
   para scripts profesionales. `die` va a stderr y termina;
   `require_cmd` valida dependencias al inicio.

6. El **main wrapper** (`if [[ "${BASH_SOURCE[0]}" == "${0}" ]]`)
   permite que un script sea importable con `source` sin
   ejecutar la logica principal.
