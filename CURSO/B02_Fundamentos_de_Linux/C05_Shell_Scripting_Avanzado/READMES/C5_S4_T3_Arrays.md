# T03 — Arrays

> **Fuentes**: README.md + README.max.md + labs/README.md
> **Regla aplicada**: 2 (`.max.md` sin `.claude.md`)

## Errata detectada en las fuentes

| # | Archivo | Línea(s) | Error | Corrección |
|---|---------|----------|-------|------------|
| 1 | max.md | 398 | Tabla: `ARR=("${ARR[@]}")")` — paréntesis y comilla extra | Debe ser `ARR=("${ARR[@]}")` |
| 2 | max.md | 516 | `fi` cerrando un `for` loop en la función `intersection()` | Debe ser `done` (es un for, no un if) |
| 3 | md/max.md | 423/439 | `find -print0 \| sort -z \| tr '\0' '\n'` — convierte NUL de vuelta a newline | Anula la protección de `-print0`; usar `mapfile -d ''` o no usar `-print0` |
| 4 | max.md | 211 | `sort -t: -k2 -rn` pero el printf produce output con espacios | Debe ser `sort -k2 -rn` (separador por defecto = whitespace) |
| 5 | max.md | 546 | Variable `HISTORY` colisiona con el builtin de bash | `HISTORY` es una variable especial de bash (historial de comandos); renombrar |

---

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

# Desde un glob (seguro — cada archivo es un elemento):
FILES=(*.txt)

# Desde un comando (PELIGRO — word splitting rompe líneas con espacios):
LINES=($(cat file.txt))          # INCORRECTO
mapfile -t LINES < file.txt      # CORRECTO (ver sección mapfile)
```

### Acceso a elementos

```bash
FRUITS=(apple banana cherry date elderberry)

# Elemento por índice (0-based):
echo "${FRUITS[0]}"     # apple
echo "${FRUITS[2]}"     # cherry

# Último elemento (bash 4.2+):
echo "${FRUITS[-1]}"    # elderberry
echo "${FRUITS[-2]}"    # date

# IMPORTANTE: sin llaves, $FRUITS es solo el primer elemento:
echo "$FRUITS"           # apple (equivale a ${FRUITS[0]})
```

### Todos los elementos — `[@]` vs `[*]`

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

# Eliminar un elemento (deja un hueco en los índices):
unset 'FRUITS[2]'
echo "${!FRUITS[@]}"    # 0 1 3 4 5 (el índice 2 no existe)
echo "${#FRUITS[@]}"    # 5 (5 elementos)

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
echo "${FRUITS[@]:1:3}"    # banana cherry date (3 desde índice 1)
echo "${FRUITS[@]:2}"      # cherry date elderberry (desde índice 2 al final)
echo "${FRUITS[@]:(-2)}"   # date elderberry (los últimos 2)
```

---

## Arrays asociativos — `declare -A`

Los arrays asociativos (diccionarios/maps) usan strings como claves. Requieren
`declare -A` explícito (bash 4+):

```bash
# Crear:
declare -A PORTS=([http]=80 [https]=443 [ssh]=22 [dns]=53)

# O paso a paso:
declare -A PORTS
PORTS[http]=80
PORTS[https]=443

# Acceder:
echo "${PORTS[http]}"     # 80
echo "${PORTS[ssh]}"      # 22

# IMPORTANTE: sin declare -A, bash crea un array indexado normal
```

### Operaciones

```bash
declare -A CONFIG=([host]="localhost" [port]=8080 [debug]="false")

# Todas las claves (orden NO garantizado):
echo "${!CONFIG[@]}"      # host port debug (puede variar)

# Todos los valores:
echo "${CONFIG[@]}"       # localhost 8080 false

# Número de elementos:
echo "${#CONFIG[@]}"      # 3

# Verificar si una clave existe (bash 4.2+):
if [[ -v CONFIG[host] ]]; then
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
declare -A WORD_COUNT

while read -r word; do
    ((WORD_COUNT[$word]++))
done < <(tr '[:upper:]' '[:lower:]' < file.txt | tr -cs '[:alpha:]' '\n')

for word in "${!WORD_COUNT[@]}"; do
    echo "${WORD_COUNT[$word]} $word"
done | sort -rn | head -10
```

### Caso de uso: leer configuración key=value

```bash
declare -A CONFIG

while IFS='=' read -r key value; do
    [[ -z "$key" || "$key" == \#* ]] && continue
    key=$(echo "$key" | tr -d '[:space:]')
    value=$(echo "$value" | tr -d '[:space:]')
    CONFIG[$key]="$value"
done < config.ini

echo "Host: ${CONFIG[host]}"
echo "Port: ${CONFIG[port]}"
```

---

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

# Desde un comando (process substitution):
mapfile -t USERS < <(cut -d: -f1 /etc/passwd)
```

### Desde un comando (con espacios en nombres)

```bash
# INCORRECTO — word splitting parte las líneas con espacios:
FILES=($(find . -name "*.txt"))

# CORRECTO — mapfile con process substitution:
mapfile -t FILES < <(find . -name "*.txt")

# MÁS SEGURO — null-terminated para nombres con newlines:
mapfile -t -d '' FILES < <(find . -name "*.txt" -print0)

# ALTERNATIVA — while read con null separator:
FILES=()
while IFS= read -r -d '' f; do
    FILES+=("$f")
done < <(find . -name "*.txt" -print0)
```

---

## Pasar arrays a funciones

### Por expansión (el más común)

```bash
# Los arrays se expanden con "${array[@]}":
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
    local -n arr=$1    # arr es una referencia al array pasado
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

---

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

### Simular un stack (LIFO)

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

## Tabla comparativa

| Operación | Sintaxis | Notas |
|---|---|---|
| Crear array | `ARR=(a b c)` | Espaciado define elementos |
| Crear vacío | `declare -a ARR=()` | |
| Crear asociativo | `declare -A ARR=()` | Bash 4+ obligatorio |
| Elemento en índice | `${ARR[i]}` | 0-based |
| Último elemento | `${ARR[-1]}` | Bash 4.2+ |
| Todos (separados) | `"${ARR[@]}"` | Siempre con comillas |
| Todos (un string) | `"${ARR[*]}"` | Usa `$IFS` como separador |
| Número de elementos | `${#ARR[@]}` | |
| Longitud de elemento | `${#ARR[i]}` | Caracteres del elemento i |
| Todos los índices/claves | `${!ARR[@]}` | |
| Agregar | `ARR+=(d e)` | Al final |
| Eliminar elemento | `unset 'ARR[i]'` | Deja hueco |
| Re-indexar | `ARR=("${ARR[@]}")` | Elimina huecos |
| Slice | `${ARR[@]:offset:len}` | |
| Verificar clave | `[[ -v ARR[key] ]]` | Bash 4.2+ |

---

## Labs

### Parte 1 — Arrays indexados

#### Paso 1.1: Crear y acceder

```bash
FRUITS=(apple banana cherry date elderberry)

echo '${FRUITS[0]}' "= ${FRUITS[0]}"       # apple
echo '${FRUITS[-1]}' "= ${FRUITS[-1]}"     # elderberry
echo '$FRUITS' "= $FRUITS (solo el primero)"
echo '${#FRUITS[@]}' "= ${#FRUITS[@]} elementos"
echo '${!FRUITS[@]}' "= ${!FRUITS[@]} (índices)"
```

#### Paso 1.2: `[@]` vs `[*]`

```bash
FILES=("file one.txt" "file two.txt" "file three.txt")

echo '--- "${arr[@]}" (correcto) ---'
for f in "${FILES[@]}"; do echo "  [$f]"; done

echo '--- "${arr[*]}" (un solo string) ---'
for f in "${FILES[*]}"; do echo "  [$f]"; done

echo '--- ${arr[@]} sin comillas (ROMPE) ---'
for f in ${FILES[@]}; do echo "  [$f]"; done
```

#### Paso 1.3: Modificar arrays

```bash
ARR=(alpha bravo charlie delta echo)
echo "Original: ${ARR[*]}"

ARR+=(foxtrot golf)
echo "Después de +=: ${ARR[*]}"

ARR[1]="BRAVO"
echo "ARR[1]=BRAVO: ${ARR[*]}"

unset 'ARR[2]'
echo "unset ARR[2]: ${ARR[*]}"
echo "Índices: ${!ARR[@]} (hueco en 2)"

ARR=("${ARR[@]}")
echo "Re-indexado: ${ARR[*]}"
echo "Índices: ${!ARR[@]}"

# Slicing
FULL=(a b c d e f g)
echo '${FULL[@]:2:3}' "= ${FULL[@]:2:3}"
echo '${FULL[@]:(-2)}' "= ${FULL[@]:(-2)}"
```

### Parte 2 — Arrays asociativos

#### Paso 2.1: `declare -A`

```bash
declare -A PORTS=([http]=80 [https]=443 [ssh]=22 [dns]=53)

echo "PORTS[http] = ${PORTS[http]}"
echo "PORTS[ssh] = ${PORTS[ssh]}"
echo "Claves: ${!PORTS[@]} (orden no garantizado)"
echo "Valores: ${PORTS[@]}"
echo "Total: ${#PORTS[@]}"

for key in "${!PORTS[@]}"; do
    echo "  $key → ${PORTS[$key]}"
done
```

#### Paso 2.2: Verificar existencia y eliminar

```bash
declare -A CONFIG=([host]="localhost" [port]="8080" [debug]="false")

# -v verifica si la clave existe
[[ -v CONFIG[host] ]] && echo "host existe: ${CONFIG[host]}"
[[ -v CONFIG[timeout] ]] || echo "timeout NO existe"

# Eliminar
unset 'CONFIG[debug]'
echo "Después de unset debug:"
for k in "${!CONFIG[@]}"; do echo "  $k = ${CONFIG[$k]}"; done

# Agregar
CONFIG[timeout]="30"
echo "Después de agregar timeout:"
for k in "${!CONFIG[@]}"; do echo "  $k = ${CONFIG[$k]}"; done
```

#### Paso 2.3: Conteo de frecuencias

```bash
declare -A SHELL_COUNT
while IFS=: read -r _ _ _ _ _ _ shell; do
    ((SHELL_COUNT[$shell]++))
done < /etc/passwd

echo "Frecuencia de shells:"
for shell in "${!SHELL_COUNT[@]}"; do
    printf "  %-25s %d\n" "$shell" "${SHELL_COUNT[$shell]}"
done | sort -k2 -rn
```

### Parte 3 — mapfile y patrones

#### Paso 3.1: mapfile

```bash
# Desde archivo
mapfile -t LINES < /etc/hostname
echo "Líneas: ${#LINES[@]}, Primera: ${LINES[0]}"

# Desde comando
mapfile -t USERS < <(cut -d: -f1 /etc/passwd | head -5)
echo "Usuarios:"
for u in "${USERS[@]}"; do echo "  $u"; done

# Seguro con espacios en nombres
touch "/tmp/test file.txt" "/tmp/normal.txt"
mapfile -t FOUND < <(find /tmp -maxdepth 1 -name "*.txt" -type f)
echo "Encontrados:"
for f in "${FOUND[@]}"; do echo "  $f"; done
rm -f "/tmp/test file.txt" "/tmp/normal.txt"
```

#### Paso 3.2: Join y split

```bash
# Split
IFS="," read -ra ITEMS <<< "apple,banana,cherry,date"
echo "Split de CSV:"
for item in "${ITEMS[@]}"; do echo "  $item"; done

# Join
join_by() {
    local IFS="$1"
    shift
    echo "$*"
}
TAGS=(linux bash scripting)
echo "Join con coma: $(join_by ',' "${TAGS[@]}")"

# Split del PATH
IFS=":" read -ra PATH_DIRS <<< "$PATH"
echo "Primeros 3 dirs:"
for ((i=0; i<3 && i<${#PATH_DIRS[@]}; i++)); do
    echo "  ${PATH_DIRS[$i]}"
done
```

---

## Ejercicios

### Ejercicio 1 — Operaciones básicas

Crea un array, modifícalo y observa el comportamiento de `unset`.

```bash
ITEMS=(alpha bravo charlie delta echo)
ITEMS+=(foxtrot golf)
unset 'ITEMS[2]'

echo "Antes de re-indexar:"
echo "  Índices: ${!ITEMS[@]}"
echo "  Elementos: ${ITEMS[*]}"

ITEMS=("${ITEMS[@]}")

echo "Después de re-indexar:"
for i in "${!ITEMS[@]}"; do
    echo "  [$i] ${ITEMS[$i]}"
done
```

<details><summary>Predicción</summary>

```
Antes de re-indexar:
  Índices: 0 1 3 4 5 6
  Elementos: alpha bravo delta echo foxtrot golf
```

`unset 'ITEMS[2]'` elimina `charlie` (índice 2) pero no renumera los demás.
El índice 2 desaparece, queda un hueco.

```
Después de re-indexar:
  [0] alpha
  [1] bravo
  [2] delta
  [3] echo
  [4] foxtrot
  [5] golf
```

`ITEMS=("${ITEMS[@]}")` crea un nuevo array con los valores existentes,
re-indexando consecutivamente desde 0. 6 elementos (7 originales - 1 eliminado).

</details>

### Ejercicio 2 — `[@]` vs `[*]` con espacios

```bash
FILES=("my doc.txt" "your file.pdf" "the data.csv")

echo "=== Con [@] y comillas ==="
count=0
for f in "${FILES[@]}"; do
    ((count++))
    echo "  $count: [$f]"
done

echo ""
echo "=== Sin comillas ==="
count=0
for f in ${FILES[@]}; do
    ((count++))
    echo "  $count: [$f]"
done
```

<details><summary>Predicción</summary>

```
=== Con [@] y comillas ===
  1: [my doc.txt]
  2: [your file.pdf]
  3: [the data.csv]
```

`"${FILES[@]}"` preserva cada elemento como unidad. 3 iteraciones.

```
=== Sin comillas ===
  1: [my]
  2: [doc.txt]
  3: [your]
  4: [file.pdf]
  5: [the]
  6: [data.csv]
```

Sin comillas, word splitting divide por espacios. 6 iteraciones en lugar de 3.
Por eso la regla: **siempre** usar `"${array[@]}"` con comillas dobles.

</details>

### Ejercicio 3 — Array asociativo: mapa HTTP

```bash
declare -A HTTP_STATUS=(
    [200]="OK"
    [301]="Moved Permanently"
    [404]="Not Found"
    [500]="Internal Server Error"
)

for code in 200 301 404 500 418; do
    echo "$code: ${HTTP_STATUS[$code]:-Unknown Status}"
done
```

<details><summary>Predicción</summary>

```
200: OK
301: Moved Permanently
404: Not Found
500: Internal Server Error
418: Unknown Status
```

`${HTTP_STATUS[$code]:-Unknown Status}` usa el valor default `Unknown Status`
cuando la clave no existe. `418` (I'm a teapot) no está en el mapa, así que
usa el default. El orden de iteración del for aquí es determinístico porque
los códigos están en el for, no en `${!HTTP_STATUS[@]}` (que sería no
garantizado).

</details>

### Ejercicio 4 — Frecuencia de palabras

```bash
declare -A WORDS
TEXT="the quick brown fox jumps over the lazy dog the fox"

for word in $TEXT; do
    ((WORDS[${word,,}]++))
done

echo "Conteo de palabras:"
for word in "${!WORDS[@]}"; do
    printf "  %-10s %d\n" "$word" "${WORDS[$word]}"
done | sort -k2 -rn
```

<details><summary>Predicción</summary>

```
Conteo de palabras:
  the        3
  fox        2
  brown      1
  dog        1
  jumps      1
  lazy       1
  over       1
  quick      1
```

`${word,,}` convierte a minúsculas (no importa aquí, ya lo son).
`((WORDS[$word]++))` incrementa el contador. `the` aparece 3 veces, `fox` 2,
el resto 1. `sort -k2 -rn` ordena por la segunda columna numérica descendente.
El orden exacto de las palabras con count=1 puede variar (iteración de array
asociativo no es determinística).

</details>

### Ejercicio 5 — mapfile: lectura segura

```bash
# Crear archivos de prueba
echo -e "line one\nline two\nline three" > /tmp/test.txt

# Incorrecto:
BAD=($(cat /tmp/test.txt))
echo "Incorrecto (${#BAD[@]} elementos):"
for b in "${BAD[@]}"; do echo "  [$b]"; done

# Correcto:
mapfile -t GOOD < /tmp/test.txt
echo ""
echo "Correcto (${#GOOD[@]} elementos):"
for g in "${GOOD[@]}"; do echo "  [$g]"; done

rm /tmp/test.txt
```

<details><summary>Predicción</summary>

```
Incorrecto (6 elementos):
  [line]
  [one]
  [line]
  [two]
  [line]
  [three]
```

`$(cat ...)` aplica word splitting: cada palabra se convierte en un elemento
separado. 6 palabras = 6 elementos.

```
Correcto (3 elementos):
  [line one]
  [line two]
  [line three]
```

`mapfile -t` lee línea por línea. Cada línea es un elemento, preservando los
espacios internos. 3 líneas = 3 elementos. `-t` elimina el `\n` final de
cada línea.

</details>

### Ejercicio 6 — Nameref: modificar array por referencia

```bash
fill_array() {
    local -n arr=$1
    arr+=("x" "y" "z")
}

append_to() {
    local -n arr=$1
    shift
    arr+=("$@")
}

declare -a RESULT=()
fill_array RESULT
echo "Después de fill: ${RESULT[*]}"

append_to RESULT "a" "b"
echo "Después de append: ${RESULT[*]}"
echo "Total: ${#RESULT[@]}"
```

<details><summary>Predicción</summary>

```
Después de fill: x y z
Después de append: x y z a b
Total: 5
```

`local -n arr=$1` crea una referencia al array cuyo **nombre** se pasó como
primer argumento. `arr+=("x" "y" "z")` modifica `RESULT` directamente porque
`arr` es un alias. La segunda llamada agrega `a` y `b` al array ya existente.

Nota: pasar el **nombre** (`RESULT`), no el **contenido** (`"${RESULT[@]}"`).

</details>

### Ejercicio 7 — Slicing y subarray

```bash
NUMS=(0 1 2 3 4 5 6 7 8 9)

echo "Original:     ${NUMS[*]}"
echo "Desde 3:      ${NUMS[*]:3}"
echo "3 desde 2:    ${NUMS[*]:2:3}"
echo "Últimos 3:    ${NUMS[*]:(-3)}"
echo "Primeros 4:   ${NUMS[*]:0:4}"

# Simular "quitar primer elemento":
REST=("${NUMS[@]:1}")
echo "Sin primero:  ${REST[*]}"
echo "Tamaño orig:  ${#NUMS[@]}"
echo "Tamaño rest:  ${#REST[@]}"
```

<details><summary>Predicción</summary>

```
Original:     0 1 2 3 4 5 6 7 8 9
Desde 3:      3 4 5 6 7 8 9
3 desde 2:    2 3 4
Últimos 3:    7 8 9
Primeros 4:   0 1 2 3
Sin primero:  1 2 3 4 5 6 7 8 9
Tamaño orig:  10
Tamaño rest:  9
```

`${NUMS[*]:3}` = desde índice 3 al final. `${NUMS[*]:2:3}` = 3 elementos
empezando en índice 2. `${NUMS[*]:(-3)}` = los últimos 3 (el paréntesis es
necesario para que `-3` no se interprete como default value).

</details>

### Ejercicio 8 — Join y split

```bash
# Split
IFS=',' read -ra FIELDS <<< "alice,30,madrid,dev"
echo "Campos: ${#FIELDS[@]}"
for i in "${!FIELDS[@]}"; do
    echo "  [$i] = ${FIELDS[$i]}"
done

# Join
join_by() {
    local IFS="$1"
    shift
    echo "$*"
}

TAGS=(linux bash docker kubernetes)
echo ""
echo "Con coma: $(join_by ',' "${TAGS[@]}")"
echo "Con pipe: $(join_by '|' "${TAGS[@]}")"
echo "Con dash: $(join_by '-' "${TAGS[@]}")"
```

<details><summary>Predicción</summary>

```
Campos: 4
  [0] = alice
  [1] = 30
  [2] = madrid
  [3] = dev
```

`IFS=','` delante de `read` cambia IFS solo para ese comando. Cada campo
separado por coma se asigna a un elemento del array.

```
Con coma: linux,bash,docker,kubernetes
Con pipe: linux|bash|docker|kubernetes
Con dash: linux-bash-docker-kubernetes
```

`join_by` funciona porque `"$*"` concatena todos los argumentos usando el
primer carácter de `$IFS` como separador. `local IFS="$1"` solo afecta dentro
de la función.

</details>

### Ejercicio 9 — Stack con arrays

```bash
declare -a STACK=()

push() { STACK+=("$1"); }
pop() {
    local -n result=$1
    result="${STACK[-1]}"
    unset 'STACK[-1]'
}
peek() { echo "${STACK[-1]}"; }
size() { echo "${#STACK[@]}"; }

push "primero"
push "segundo"
push "tercero"
echo "Stack: ${STACK[*]} (size=$(size))"

pop VAL
echo "Pop: $VAL"
echo "Stack: ${STACK[*]} (size=$(size))"
echo "Peek: $(peek)"

pop VAL
echo "Pop: $VAL"
echo "Stack: ${STACK[*]} (size=$(size))"
```

<details><summary>Predicción</summary>

```
Stack: primero segundo tercero (size=3)
Pop: tercero
Stack: primero segundo (size=2)
Peek: segundo
Pop: segundo
Stack: primero (size=1)
```

LIFO (Last In, First Out): `push` agrega al final con `+=`, `pop` toma el
último con `${STACK[-1]}` y lo elimina con `unset 'STACK[-1]'`. `peek` muestra
el tope sin eliminarlo.

</details>

### Ejercicio 10 — Panorama: script con arrays

Un script que procesa argumentos, acumula resultados en arrays, y genera
un reporte.

```bash
#!/usr/bin/env bash
set -euo pipefail

declare -a VALID_FILES=()
declare -a ERRORS=()
declare -A STATS=()

# Procesar argumentos como array
ARGS=("$@")
[[ ${#ARGS[@]} -eq 0 ]] && ARGS=(/etc/passwd /etc/hostname /no/existe /etc/group)

for file in "${ARGS[@]}"; do
    if [[ ! -f "$file" ]]; then
        ERRORS+=("$file: no encontrado")
        continue
    fi

    VALID_FILES+=("$file")
    lines=$(wc -l < "$file")
    STATS[$file]=$lines
done

echo "=== Resultados ==="
echo "Archivos procesados: ${#VALID_FILES[@]}"
echo "Errores: ${#ERRORS[@]}"

if [[ ${#VALID_FILES[@]} -gt 0 ]]; then
    echo ""
    echo "--- Estadísticas ---"
    for f in "${VALID_FILES[@]}"; do
        printf "  %-30s %5d líneas\n" "$f" "${STATS[$f]}"
    done

    # Total usando array slice y loop
    total=0
    for f in "${VALID_FILES[@]}"; do
        ((total += STATS[$f]))
    done
    echo "  Total: $total líneas"

    # Top 3 por líneas (join para mostrar)
    echo ""
    echo "--- Top archivos ---"
    for f in "${VALID_FILES[@]}"; do
        echo "${STATS[$f]} $f"
    done | sort -rn | head -3
fi

if [[ ${#ERRORS[@]} -gt 0 ]]; then
    echo ""
    echo "--- Errores ---"
    printf '  - %s\n' "${ERRORS[@]}"
fi

# Probar:
# ./report.sh /etc/passwd /etc/hostname /no/existe /etc/group
# ./report.sh $(find /etc -maxdepth 1 -name "*.conf" 2>/dev/null | head -5)
```

<details><summary>Predicción</summary>

Con los defaults (`/etc/passwd /etc/hostname /no/existe /etc/group`):

```
=== Resultados ===
Archivos procesados: 3
Errores: 1

--- Estadísticas ---
  /etc/passwd                    N líneas
  /etc/hostname                  1 líneas
  /etc/group                     M líneas
  Total: X líneas

--- Top archivos ---
N /etc/passwd
M /etc/group
1 /etc/hostname

--- Errores ---
  - /no/existe: no encontrado
```

(N, M, X dependen del sistema)

El script demuestra:
1. **Array indexado** `VALID_FILES` para acumular archivos válidos
2. **Array indexado** `ERRORS` para acumular errores
3. **Array asociativo** `STATS` para mapear archivo → número de líneas
4. **`$@` a array** con `ARGS=("$@")` para poder verificar `${#ARGS[@]}`
5. **`printf` con arrays** para formatear el reporte
6. **Patrón acumular-y-reportar**: procesar todo primero, luego reportar

</details>
