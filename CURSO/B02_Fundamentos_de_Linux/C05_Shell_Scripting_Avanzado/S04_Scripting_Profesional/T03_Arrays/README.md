# T03 — Arrays

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

---

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
