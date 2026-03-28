# T03 — Arrays (Enhanced)

## Arrays indexados

```bash
# Crear un array:
FRUITS=(apple banana cherry)

# Crear vacío:
declare -a ITEMS=()

# Asignación individual:
COLORS[0]="red"
COLORS[1]="green"
COLORS[2]="blue"

# Desde la salida de un comando (cuidado con espacios — ver más abajo):
FILES=(*.txt)                    # glob se expande al crear el array
LINES=($(cat file.txt))          # PELIGRO: word splitting con espacios
```

### Acceso a elementos

```bash
FRUITS=(apple banana cherry date elderberry)

# Elemento por índice (0-based):
echo "${FRUITS[0]}"     # apple
echo "${FRUITS[2]}"     # cherry

# Último elemento:
echo "${FRUITS[-1]}"    # elderberry (bash 4.2+)
echo "${FRUITS[-2]}"    # date

# IMPORTANTE: sin llaves, $FRUITS es solo el primer elemento:
echo $FRUITS            # apple (equivale a ${FRUITS[0]})
echo "$FRUITS"          # apple
```

### Todos los elementos — [@] vs [*]

```bash
ITEMS=("file one.txt" "file two.txt" "file three.txt")

# "${array[@]}" — cada elemento como una palabra SEPARADA:
for item in "${ITEMS[@]}"; do
    echo "Item: $item"
done
# Item: file one.txt
# Item: file two.txt
# Item: file three.txt

# "${array[*]}" — todos los elementos como UNA SOLA cadena:
echo "${ITEMS[*]}"
# file one.txt file two.txt file three.txt

# Sin comillas — word splitting rompe todo:
for item in ${ITEMS[@]}; do
    echo "Item: $item"
done
# Item: file
# Item: one.txt
# Item: file
# Item: two.txt
# ...

# REGLA: siempre usar "${array[@]}" con comillas dobles
```

### Longitud e índices

```bash
FRUITS=(apple banana cherry date)

# Número de elementos:
echo "${#FRUITS[@]}"    # 4

# Longitud de un elemento:
echo "${#FRUITS[0]}"    # 5 (longitud de "apple")

# Todos los índices:
echo "${!FRUITS[@]}"    # 0 1 2 3

# Iterar por índice:
for i in "${!FRUITS[@]}"; do
    echo "[$i] = ${FRUITS[$i]}"
done
```

### Modificar arrays

```bash
FRUITS=(apple banana cherry)

# Agregar al final:
FRUITS+=(date elderberry)
echo "${FRUITS[@]}"     # apple banana cherry date elderberry

# Agregar un elemento:
FRUITS+=("fig")

# Reemplazar un elemento:
FRUITS[1]="blueberry"

# Eliminar un elemento (deja un hueco):
unset 'FRUITS[2]'
echo "${!FRUITS[@]}"    # 0 1 3 4 5 (el índice 2 no existe)
echo "${#FRUITS[@]}"    # 5 (5 elementos, no 6)

# Eliminar el array completo:
unset FRUITS
```

### El problema de unset y los huecos

```bash
ARR=(a b c d e)
unset 'ARR[2]'

# Los índices NO se renumeran:
echo "${!ARR[@]}"       # 0 1 3 4
echo "${ARR[2]}"        # (vacío — no existe)
echo "${ARR[3]}"        # d

# Para re-indexar, crear un nuevo array:
ARR=("${ARR[@]}")
echo "${!ARR[@]}"       # 0 1 2 3
```

### Slicing (subarray)

```bash
FRUITS=(apple banana cherry date elderberry)

# ${array[@]:offset:length}
echo "${FRUITS[@]:1:3}"    # banana cherry date (3 elementos desde el índice 1)
echo "${FRUITS[@]:2}"      # cherry date elderberry (desde el índice 2 hasta el final)
echo "${FRUITS[@]:(-2)}"   # date elderberry (los últimos 2)
```

## Arrays asociativos — declare -A

Los arrays asociativos (diccionarios/maps) usan strings como claves en lugar
de números. Requieren `declare -A` explícito (bash 4+):

```bash
# Crear:
declare -A PORTS
PORTS[http]=80
PORTS[https]=443
PORTS[ssh]=22
PORTS[dns]=53

# O en una línea:
declare -A PORTS=([http]=80 [https]=443 [ssh]=22 [dns]=53)

# Acceder:
echo "${PORTS[http]}"     # 80
echo "${PORTS[ssh]}"      # 22
```

### Operaciones

```bash
declare -A CONFIG=([host]="localhost" [port]=8080 [debug]="false")

# Todas las claves:
echo "${!CONFIG[@]}"      # host port debug (orden no garantizado)

# Todos los valores:
echo "${CONFIG[@]}"       # localhost 8080 false

# Número de elementos:
echo "${#CONFIG[@]}"      # 3

# Verificar si una clave existe:
if [[ -v CONFIG[host] ]]; then    # -v (bash 4.2+)
    echo "host está definido"
fi

# Alternativa sin -v:
if [[ -n "${CONFIG[host]+set}" ]]; then
    echo "host está definido"
fi

# Eliminar una clave:
unset 'CONFIG[debug]'

# Iterar:
for key in "${!CONFIG[@]}"; do
    echo "$key = ${CONFIG[$key]}"
done
```

### Caso de uso: conteo de frecuencias

```bash
# Contar palabras en un archivo:
declare -A WORD_COUNT

while read -r word; do
    ((WORD_COUNT[$word]++))
done < <(tr '[:upper:]' '[:lower:]' < file.txt | tr -cs '[:alpha:]' '\n')

for word in "${!WORD_COUNT[@]}"; do
    echo "${WORD_COUNT[$word]} $word"
done | sort -rn | head -10
```

### Caso de uso: configuración

```bash
# Leer un archivo key=value en un array asociativo:
declare -A CONFIG

while IFS='=' read -r key value; do
    [[ -z "$key" || "$key" == \#* ]] && continue
    # Eliminar espacios:
    key=$(echo "$key" | tr -d '[:space:]')
    value=$(echo "$value" | tr -d '[:space:]')
    CONFIG[$key]="$value"
done < config.ini

echo "Host: ${CONFIG[host]}"
echo "Port: ${CONFIG[port]}"
```

## Leer arrays de forma segura

### Desde un archivo (línea por línea)

```bash
# mapfile/readarray (bash 4+) — la forma correcta:
mapfile -t LINES < file.txt
# -t elimina el newline de cada línea
# Cada línea es un elemento del array

echo "Líneas: ${#LINES[@]}"
echo "Primera: ${LINES[0]}"
echo "Última: ${LINES[-1]}"

# readarray es sinónimo de mapfile:
readarray -t LINES < file.txt

# Desde un comando:
mapfile -t USERS < <(cut -d: -f1 /etc/passwd)

# Con callback cada N líneas:
mapfile -t -C 'echo "Leyendo..."' -c 100 LINES < huge.log
```

### Desde un comando (con espacios)

```bash
# INCORRECTO — word splitting parte las líneas con espacios:
FILES=($(find . -name "*.txt"))

# CORRECTO — mapfile con process substitution:
mapfile -t FILES < <(find . -name "*.txt")

# CORRECTO — null-terminated para nombres con newlines:
mapfile -t -d '' FILES < <(find . -name "*.txt" -print0)

# CORRECTO — while read:
FILES=()
while IFS= read -r -d '' f; do
    FILES+=("$f")
done < <(find . -name "*.txt" -print0)
```

## Pasar arrays a funciones

```bash
# Los arrays NO se pasan como un solo argumento
# Se expanden con "${array[@]}":
process_files() {
    echo "Recibí $# archivos"
    for f in "$@"; do
        echo "  - $f"
    done
}

FILES=("file one.txt" "file two.txt")
process_files "${FILES[@]}"
# Recibí 2 archivos
#   - file one.txt
#   - file two.txt
```

### Por referencia — nameref (bash 4.3+)

```bash
# local -n crea una referencia a la variable nombrada:
add_to_array() {
    local -n arr=$1    # arr es una referencia
    local item="$2"
    arr+=("$item")
}

declare -a ITEMS=()
add_to_array ITEMS "first"     # pasar el NOMBRE, no el valor
add_to_array ITEMS "second"
echo "${ITEMS[@]}"              # first second

# También funciona con arrays asociativos:
get_value() {
    local -n map=$1
    local key=$2
    echo "${map[$key]}"
}

declare -A PORTS=([http]=80 [ssh]=22)
get_value PORTS http    # 80
```

## Patrones comunes

### Acumular resultados

```bash
ERRORS=()
WARNINGS=()

check_file() {
    local file="$1"
    if [[ ! -f "$file" ]]; then
        ERRORS+=("$file: no encontrado")
    elif [[ ! -r "$file" ]]; then
        WARNINGS+=("$file: sin permiso de lectura")
    fi
}

for f in config.ini data.csv missing.txt; do
    check_file "$f"
done

if [[ ${#ERRORS[@]} -gt 0 ]]; then
    echo "Errores:"
    printf '  - %s\n' "${ERRORS[@]}"
    exit 1
fi
```

### Simular un stack

```bash
declare -a STACK=()

push() { STACK+=("$1"); }
pop()  {
    local -n result=$1
    result="${STACK[-1]}"
    unset 'STACK[-1]'
}
peek() { echo "${STACK[-1]}"; }

push "first"
push "second"
push "third"
pop VALUE
echo "$VALUE"    # third
peek             # second
```

### Join y split

```bash
# Join array con delimitador:
join_by() {
    local IFS="$1"
    shift
    echo "$*"
}
ITEMS=(apple banana cherry)
join_by ',' "${ITEMS[@]}"
# apple,banana,cherry

# Split string en array:
IFS=',' read -ra ITEMS <<< "apple,banana,cherry"
echo "${ITEMS[@]}"    # apple banana cherry
echo "${#ITEMS[@]}"   # 3
```

## Tabla comparativa: operaciones con arrays

| Operación | Sintaxis | Notas |
|---|---|---|
| Crear array | `ARR=(a b c)` | Espaciado define elementos |
| Crear vacío | `declare -a ARR=()` | Con `declare` |
| Crear asociativo | `declare -A ARR=()` | Bash 4+ |
| Elemento en índice | `${ARR[i]}` | i es 0-based |
| Último elemento | `${ARR[-1]}` | Bash 4.2+ |
| Todos los elementos | `${ARR[@]}` | Con comillas dobles SIEMPRE |
| Número de elementos | `${#ARR[@]}` | |
| Todos los índices | `${!ARR[@]}` | Para sparse arrays |
| Agregar elementos | `ARR+=(d e)` | Al final |
| Eliminar elemento | `unset 'ARR[i]'` | Deja hueco |
| Re-indexar | `ARR=("${ARR[@]}")") | Elimina huecos |
| Eliminar array | `unset ARR` | |
|Slice | `${ARR[@]:offset:len}` | Bash 3+ |

## Ejercicios

### Ejercicio 1 — Operaciones básicas con arrays

```bash
# Crear un array con 5 elementos, agregar 2 más, eliminar el tercero,
# e imprimir el resultado:
ITEMS=(alpha bravo charlie delta echo)
ITEMS+=(foxtrot golf)
unset 'ITEMS[2]'
ITEMS=("${ITEMS[@]}")    # re-indexar

for i in "${!ITEMS[@]}"; do
    echo "[$i] ${ITEMS[$i]}"
done
```

### Ejercicio 2 — Array asociativo

```bash
# Crear un mapa de código HTTP → descripción:
declare -A HTTP_STATUS=(
    [200]="OK"
    [301]="Moved Permanently"
    [404]="Not Found"
    [500]="Internal Server Error"
)

CODE="${1:-200}"
echo "$CODE: ${HTTP_STATUS[$CODE]:-Unknown Status}"
```

### Ejercicio 3 — Leer archivos en un array de forma segura

```bash
# Leer todos los .txt del directorio actual en un array,
# manejar nombres con espacios:
mapfile -t TXT_FILES < <(find . -maxdepth 1 -name "*.txt" -print0 | sort -z | tr '\0' '\n')

echo "Encontrados: ${#TXT_FILES[@]} archivos"
for f in "${TXT_FILES[@]}"; do
    echo "  - $f ($(wc -l < "$f") líneas)"
done
```

### Ejercicio 4 — Frecuencia de palabras en texto

```bash
#!/usr/bin/env bash
set -euo pipefail

TEXT="${1:-The quick brown fox jumps over the lazy dog fox}"

declare -A WORDS
for word in $TEXT; do
    ((WORDS[${word,,}]++))
done

echo "Conteo de palabras:"
for word in "${!WORDS[@]}"; do
    printf "%-10s %d\n" "$word" "${WORDS[$word]}"
done | sort

# Probar:
# ./word_count.sh "hello world hello"
# ./word_count.sh "$(cat /etc/passwd | head -3)"
```

### Ejercicio 5 — Set (conjunto) con arrays

```bash
#!/usr/bin/env bash
set -euo pipefail

declare -a SET=()

add_to_set() {
    local elem="$1"
    for existing in "${SET[@]}"; do
        [[ "$existing" == "$elem" ]] && return 0
    done
    SET+=("$elem")
}

remove_from_set() {
    local elem="$1"
    local new_set=()
    for existing in "${SET[@]}"; do
        [[ "$existing" != "$elem" ]] && new_set+=("$existing")
    done
    SET=("${new_set[@]}")
}

is_in_set() {
    local elem="$1"
    for existing in "${SET[@]}"; do
        [[ "$existing" == "$elem" ]] && return 0
    done
    return 1
}

union() {
    local -a other=("$@")
    for elem in "${other[@]}"; do
        add_to_set "$elem"
    done
}

intersection() {
    local -a other=("$@")
    local -a result=()
    for elem in "${SET[@]}"; do
        for other_elem in "${other[@]}"; do
            [[ "$elem" == "$other_elem" ]] && result+=("$elem") && break
        fi
    done
    SET=("${result[@]}")
}

echo "=== Demo Set ==="
add_to_set "apple"
add_to_set "banana"
add_to_set "apple"
add_to_set "cherry"
echo "Set: ${SET[@]}"
echo "Tamaño: ${#SET[@]}"

is_in_set "banana" && echo "banana está en el set" || echo "banana NO está"
is_in_set "grape" && echo "grape está en el set" || echo "grape NO está"

remove_from_set "banana"
echo "Tras remove banana: ${SET[@]}"

# Probar:
# ./set_demo.sh
```

### Ejercicio 6 — Historial de comandos con array

```bash
#!/usr/bin/env bash
set -euo pipefail

readonly MAX_HISTORY=100
declare -a HISTORY=()

add_history() {
    local cmd="$1"
    [[ -z "$cmd" ]] && return
    HISTORY+=("$cmd")
    [[ ${#HISTORY[@]} -gt $MAX_HISTORY ]] && HISTORY=("${HISTORY[@]:1}")
}

show_history() {
    local i=0
    for cmd in "${HISTORY[@]}"; do
        ((i++))
        printf "%4d  %s\n" "$i" "$cmd"
    done
}

search_history() {
    local pattern="$1"
    local i=0
    for cmd in "${HISTORY[@]}"; do
        ((i++))
        if [[ "$cmd" =~ $pattern ]]; then
            printf "%4d  %s\n" "$i" "$cmd"
        fi
    done
}

add_history "ls -la"
add_history "cat /etc/passwd"
add_history "grep root /etc/passwd"
add_history "ls /tmp"
add_history "echo hello"

echo "=== Historial completo ==="
show_history

echo ""
echo "=== Búsqueda 'ls' ==="
search_history "^ls"

# Probar:
# ./history_demo.sh
```

### Ejercicio 7 — Matriz (array 2D simulada)

```bash
#!/bin/bash

declare -a MATRIX=()
readonly ROWS=3 COLS=3

set_cell() {
    local row=$1 col=$2 val=$3
    local idx=$((row * COLS + col))
    MATRIX[idx]="$val"
}

get_cell() {
    local row=$1 col=$2
    local idx=$((row * COLS + col))
    echo "${MATRIX[idx]:-0}"
}

print_matrix() {
    for ((r=0; r<ROWS; r++)); do
        local row=""
        for ((c=0; c<COLS; c++)); do
            row+=$(printf "%4s" "$(get_cell $r $c)")
        done
        echo "$row"
    done
}

for ((r=0; r<ROWS; r++)); do
    for ((c=0; c<COLS; c++)); do
        set_cell $r $c $((r * COLS + c + 1))
    done
done

echo "Matriz 3x3:"
print_matrix

# Sumar diagonal:
diag_sum=0
for ((i=0; i<ROWS; i++)); do
    diag_sum=$((diag_sum + $(get_cell $i $i)))
done
echo "Suma diagonal: $diag_sum"

# Probar:
# ./matrix_demo.sh
```

### Ejercicio 8 — Parsear CSV a array

```bash
#!/bin/bash
set -euo pipefail

parse_csv() {
    local line="$1"
    local -a fields=()
    IFS=',' read -ra fields <<< "$line"
    printf '%s\n' "${fields[@]}"
}

declare -a HEADER
read -ra HEADER < <(parse_csv "nombre,edad,ciudad,pais")
echo "Campos: ${HEADER[@]}"
echo "Total: ${#HEADER[@]} campos"

while IFS= read -r line; do
    echo "--- Registro ---"
    fields=()
    IFS=',' read -ra fields <<< "$line"
    for i in "${!fields[@]}"; do
        echo "  ${HEADER[$i]}: ${fields[$i]}"
    done
done <<< $'Ana Garcia,30,Madrid,España\nBob Smith,25,London,UK\nCarol Wu,35,Beijing,China'

# Probar:
# ./parse_csv.sh
```

### Ejercicio 9 — Arguments como array (argc/argv)

```bash
#!/bin/bash
set -euo pipefail

echo "Número de argumentos: $#"
echo "Todos los argumentos: $*"
echo "Argumento 0 (script): $0"
echo "Argumento 1: ${1:-<no existe>}"
echo "Argumento 2: ${2:-<no existe>}"

# Convertir $@ a array:
declare -a ARGV=("$@")
echo "ARGV[@]: ${ARGV[@]}"
echo "ARGV[0]: ${ARGV[0]}"
echo "ARGV[-1]: ${ARGV[-1]}"

# Slice: argumentos del 2 en adelante:
echo "Args[2:]: ${ARGV[@]:2}"

# Probar:
# ./argv_demo.sh uno dos tres cuatro cinco
```

### Ejercicio 10 — Cache en memoria con array asociativo

```bash
#!/bin/bash
set -euo pipefail

declare -A CACHE=()
readonly CACHE_MAX=100
cache_hits=0
cache_misses=0

cache_get() {
    local key="$1"
    if [[ -v CACHE[$key] ]]; then
        ((cache_hits++))
        echo "${CACHE[$key]}"
        return 0
    else
        ((cache_misses++))
        return 1
    fi
}

cache_set() {
    local key="$1" value="$2"
    [[ ${#CACHE[@]} -ge $CACHE_MAX ]] && CACHE=()
    CACHE[$key]="$value"
}

cache_stats() {
    local total=$((cache_hits + cache_misses))
    local hit_rate=0
    [[ $total -gt 0 ]] && hit_rate=$((cache_hits * 100 / total))
    echo "Cache stats: hits=$cache_hits misses=$cache_misses hit_rate=${hit_rate}%"
}

simulate_expensive_op() {
    local key="$1"
    local result
    if result=$(cache_get "$key"); then
        echo "[CACHE HIT] key=$key value=$result"
    else
        sleep 0.1
        result="computed_${key}_$(date +%S)"
        cache_set "$key" "$result"
        echo "[CACHE MISS] key=$key computed=$result"
    fi
}

for key in A B A C B D A; do
    simulate_expensive_op "$key"
done

cache_stats

# Probar:
# ./cache_demo.sh
```

### Ejercicio 11 — Diff entre dos arrays

```bash
#!/bin/bash
set -euo pipefail

declare -a LEFT=("apple" "banana" "cherry" "date")
declare -a RIGHT=("banana" "date" "elderberry" "fig")

declare -a ONLY_LEFT=()
declare -a ONLY_RIGHT=()
declare -a IN_BOTH=()

for l in "${LEFT[@]}"; do
    found=0
    for r in "${RIGHT[@]}"; do
        [[ "$l" == "$r" ]] && found=1 && break
    done
    if [[ $found -eq 1 ]]; then
        IN_BOTH+=("$l")
    else
        ONLY_LEFT+=("$l")
    fi
done

for r in "${RIGHT[@]}"; do
    found=0
    for l in "${LEFT[@]}"; do
        [[ "$r" == "$l" ]] && found=1 && break
    done
    [[ $found -eq 0 ]] && ONLY_RIGHT+=("$r")
done

echo "Solo izquierda: ${ONLY_LEFT[*]}"
echo "Solo derecha:   ${ONLY_RIGHT[*]}"
echo "En ambos:       ${IN_BOTH[*]}"

# Probar:
# ./array_diff.sh
```

### Ejercicio 12 — Rotación de array (circular shift)

```bash
#!/bin/bash

rotate_left() {
    local -n arr=$1
    local first="${arr[0]}"
    arr=("${arr[@]:1}" "$first")
}

rotate_right() {
    local -n arr=$1
    local last="${arr[-1]}"
    arr=("$last" "${arr[@]:0:${#arr[@]}-1}")
}

ITEMS=(alpha bravo charlie delta echo)
echo "Original: ${ITEMS[*]}"

rotate_left ITEMS
echo "Rotado izquierda: ${ITEMS[*]}"   # bravo charlie delta echo alpha

rotate_right ITEMS
echo "Rotado derecha: ${ITEMS[*]}"      # alpha bravo charlie delta echo

# Probar:
# ./rotate_array.sh
```
