# Arrays indexados y asociativos en Bash

## 1. Dos tipos de array

Bash tiene dos tipos de arrays. Ningún otro dato estructurado — no hay structs,
no hay objetos. Dominar arrays es dominar la única estructura de datos del
lenguaje.

```bash
# Array indexado: índices numéricos (0, 1, 2, ...)
declare -a fruits=("apple" "banana" "cherry")

# Array asociativo: claves string (como un diccionario/map)
declare -A colors=(["red"]="#FF0000" ["green"]="#00FF00" ["blue"]="#0000FF")
```

```
Array indexado:               Array asociativo:
┌───┬────────┐               ┌───────┬─────────┐
│ 0 │ apple  │               │ red   │ #FF0000 │
│ 1 │ banana │               │ green │ #00FF00 │
│ 2 │ cherry │               │ blue  │ #0000FF │
└───┴────────┘               └───────┴─────────┘
```

**Importante**: `declare -A` es **obligatorio** para arrays asociativos.
Sin el `declare`, Bash los trata como indexados y todo falla silenciosamente.

---

## 2. Declaración y asignación

### 2.1 Arrays indexados

```bash
# Método 1: asignación directa con ()
files=("main.c" "util.c" "test.c")

# Método 2: declare explícito
declare -a files=("main.c" "util.c" "test.c")

# Método 3: asignación elemento por elemento
files[0]="main.c"
files[1]="util.c"
files[2]="test.c"

# Método 4: desde un comando (word splitting)
files=($(ls *.c))
# ⚠️ PELIGROSO: falla con espacios en nombres
# Mejor alternativa:
mapfile -t files < <(find . -name "*.c")

# Método 5: array vacío
empty=()
```

### 2.2 Arrays asociativos

```bash
# OBLIGATORIO: declare -A
declare -A config

# Asignación individual
config["host"]="localhost"
config["port"]="5432"
config["db"]="myapp"

# Asignación en bloque
declare -A config=(
    ["host"]="localhost"
    ["port"]="5432"
    ["db"]="myapp"
    ["user"]="admin"
)
```

### 2.3 La trampa de olvidar declare -A

```bash
# SIN declare -A:
oops=(["host"]="localhost" ["port"]="5432")
echo "${oops[host]}"   # → "" (vacío!)
echo "${oops[0]}"      # → "5432" (¿?)

# Bash interpretó "host" y "port" como expresiones aritméticas
# Las variables no definidas se evalúan a 0
# Ambas claves se convirtieron en índice 0 → la segunda sobreescribe la primera
```

```
Lo que quisiste:          Lo que pasó:
["host"]="localhost"      [0]="localhost"   (host=0 aritméticamente)
["port"]="5432"           [0]="5432"        (port=0, sobreescribe)

Resultado: oops = ([0]="5432")
```

---

## 3. Acceso a elementos

### 3.1 Lectura

```bash
fruits=("apple" "banana" "cherry")

# Un elemento
echo "${fruits[0]}"    # apple
echo "${fruits[1]}"    # banana
echo "${fruits[-1]}"   # cherry (último, Bash 4.3+)
echo "${fruits[-2]}"   # banana (penúltimo)

# SIN llaves: ERROR COMÚN
echo "$fruits[0]"      # → "apple[0]" (Bash expande $fruits como ${fruits[0]}, luego concatena "[0]")

# Todos los elementos (dos formas)
echo "${fruits[@]}"    # apple banana cherry (cada uno es un "word" separado)
echo "${fruits[*]}"    # apple banana cherry (todo como un solo string)
```

### 3.2 La diferencia crucial: @ vs *

```bash
fruits=("red apple" "banana split" "cherry pie")

# Con @: cada elemento es un argumento separado (preserva espacios)
for f in "${fruits[@]}"; do
    echo "[$f]"
done
# [red apple]
# [banana split]
# [cherry pie]

# Con *: todo concatenado con IFS como un solo string
for f in "${fruits[*]}"; do
    echo "[$f]"
done
# [red apple banana split cherry pie]
```

```
"${array[@]}"  →  "red apple" "banana split" "cherry pie"   (3 words)
"${array[*]}"  →  "red apple banana split cherry pie"       (1 word)

Regla: SIEMPRE usar "${array[@]}" para iterar.
       "${array[*]}" solo para join (ej: con IFS cambiado).
```

### 3.3 Join con IFS y *

```bash
fruits=("apple" "banana" "cherry")

# Join con coma
IFS=","
echo "${fruits[*]}"   # apple,banana,cherry

# Para no afectar el resto del script, usar subshell:
result=$(IFS=","; echo "${fruits[*]}")
echo "$result"  # apple,banana,cherry
# IFS original no se modificó
```

---

## 4. Longitud, claves e índices

```bash
fruits=("apple" "banana" "cherry")
declare -A config=(["host"]="localhost" ["port"]="5432" ["db"]="myapp")

# Número de elementos
echo "${#fruits[@]}"    # 3
echo "${#config[@]}"    # 3

# Longitud de UN elemento (longitud del string)
echo "${#fruits[0]}"    # 5 (length of "apple")

# Índices/claves
echo "${!fruits[@]}"    # 0 1 2
echo "${!config[@]}"    # host port db (orden NO garantizado)

# Verificar si un índice existe
if [[ -v fruits[1] ]]; then
    echo "Index 1 exists"
fi

# Verificar si una clave existe en asociativo
if [[ -v config["host"] ]]; then
    echo "Key 'host' exists"
fi
```

**Nota sobre orden**: los arrays asociativos en Bash **no garantizan orden**.
Las claves pueden aparecer en cualquier secuencia.

---

## 5. Iteración

### 5.1 Iterar sobre valores

```bash
fruits=("apple" "banana" "cherry")

for fruit in "${fruits[@]}"; do
    echo "Fruit: $fruit"
done
```

### 5.2 Iterar sobre índices/claves

```bash
# Indexado: iterar con índice
for i in "${!fruits[@]}"; do
    echo "$i: ${fruits[$i]}"
done
# 0: apple
# 1: banana
# 2: cherry

# Asociativo: iterar con clave
declare -A config=(["host"]="localhost" ["port"]="5432")

for key in "${!config[@]}"; do
    echo "$key = ${config[$key]}"
done
# port = 5432
# host = localhost
```

### 5.3 Iterar sobre pares key=value (asociativo)

```bash
declare -A env_vars=(
    ["DB_HOST"]="localhost"
    ["DB_PORT"]="5432"
    ["DB_NAME"]="myapp"
)

for key in "${!env_vars[@]}"; do
    export "$key=${env_vars[$key]}"
    echo "Exported: $key=${env_vars[$key]}"
done
```

---

## 6. Slicing

Bash soporta slicing (subarray) solo para arrays indexados:

```bash
arr=("a" "b" "c" "d" "e" "f")

# ${array[@]:offset:length}
echo "${arr[@]:1:3}"    # b c d (desde índice 1, tomar 3)
echo "${arr[@]:2}"      # c d e f (desde índice 2, hasta el final)
echo "${arr[@]:(-2)}"   # e f (los últimos 2)
echo "${arr[@]:0:2}"    # a b (los primeros 2)
```

```
arr = [a, b, c, d, e, f]
       0  1  2  3  4  5

${arr[@]:1:3}  → offset=1, length=3 → [b, c, d]
${arr[@]:2}    → offset=2, hasta final → [c, d, e, f]
${arr[@]:(-2)} → últimos 2 → [e, f]
```

**Pagination** con slicing:

```bash
items=({1..20})  # 1 2 3 ... 20
page_size=5

for ((page=0; page * page_size < ${#items[@]}; page++)); do
    offset=$((page * page_size))
    echo "Page $page: ${items[@]:$offset:$page_size}"
done
# Page 0: 1 2 3 4 5
# Page 1: 6 7 8 9 10
# Page 2: 11 12 13 14 15
# Page 3: 16 17 18 19 20
```

---

## 7. Modificar arrays

### 7.1 Append

```bash
fruits=("apple" "banana")

# Append un elemento
fruits+=("cherry")
echo "${fruits[@]}"  # apple banana cherry

# Append múltiples
fruits+=("date" "elderberry")
echo "${fruits[@]}"  # apple banana cherry date elderberry

# Append en asociativo
declare -A config=(["host"]="localhost")
config+=( ["port"]="5432" ["db"]="myapp" )
# o individual:
config["user"]="admin"
```

### 7.2 Eliminar

```bash
fruits=("apple" "banana" "cherry" "date")

# Eliminar por índice
unset 'fruits[1]'
echo "${fruits[@]}"    # apple cherry date
echo "${!fruits[@]}"   # 0 2 3  ← ¡los índices NO se reindexan!

# ¡CUIDADO! Ahora fruits[1] no existe:
echo "${fruits[1]}"    # "" (vacío)
echo "${fruits[2]}"    # cherry
```

```
Antes de unset 'fruits[1]':
[0]=apple  [1]=banana  [2]=cherry  [3]=date

Después de unset 'fruits[1]':
[0]=apple  [2]=cherry  [3]=date
           ↑ hueco! El array tiene "agujeros"

Esto es un SPARSE array. Los índices no se reasignan.
```

### 7.3 Reindexar después de eliminar

```bash
# Recrear el array para eliminar huecos
fruits=("${fruits[@]}")
echo "${!fruits[@]}"   # 0 1 2 (reindexado)
```

### 7.4 Eliminar en asociativo

```bash
declare -A config=(["host"]="localhost" ["port"]="5432" ["db"]="myapp")

unset 'config["port"]'
echo "${!config[@]}"  # host db
# Sin problemas de sparse: no hay índices numéricos
```

### 7.5 Filtrar elementos

```bash
# Crear nuevo array sin el elemento a eliminar
arr=("apple" "banana" "cherry" "banana" "date")

# Eliminar TODAS las ocurrencias de "banana"
new_arr=()
for item in "${arr[@]}"; do
    if [[ "$item" != "banana" ]]; then
        new_arr+=("$item")
    fi
done
echo "${new_arr[@]}"  # apple cherry date
```

---

## 8. Pasar arrays a funciones

### 8.1 Pasar por nombre (nameref, Bash 4.3+)

```bash
print_array() {
    local -n arr_ref=$1   # nameref: arr_ref es un alias
    for item in "${arr_ref[@]}"; do
        echo "  $item"
    done
}

fruits=("apple" "banana" "cherry")
print_array fruits   # sin $, sin [@], solo el nombre
#   apple
#   banana
#   cherry
```

### 8.2 Pasar como argumentos (clásico)

```bash
print_all() {
    for item in "$@"; do    # $@ = todos los argumentos
        echo "  $item"
    done
}

fruits=("apple" "banana" "cherry")
print_all "${fruits[@]}"   # se expande a 3 argumentos separados
```

### 8.3 Retornar un array desde función

```bash
# Bash no puede "retornar" arrays. Solución: imprimir y capturar.
get_files() {
    local dir=$1
    find "$dir" -maxdepth 1 -type f -name "*.sh" -print0
}

# Capturar con mapfile (NUL-separated para nombres con espacios)
mapfile -d '' -t scripts < <(get_files "/usr/local/bin")
echo "Found ${#scripts[@]} scripts"
```

---

## 9. mapfile / readarray

`mapfile` (alias: `readarray`) lee líneas de stdin a un array:

```bash
# Leer archivo a array (una línea = un elemento)
mapfile -t lines < /etc/hostname
echo "${lines[0]}"

# -t elimina el trailing newline de cada línea
# Sin -t: lines[0] = "myhost\n"  (incluye newline)
# Con -t: lines[0] = "myhost"    (limpio)

# Leer desde un comando
mapfile -t users < <(cut -d: -f1 /etc/passwd | head -5)
echo "${users[@]}"

# Leer con delimitador personalizado (NUL para nombres con espacios)
mapfile -d '' -t files < <(find /tmp -type f -print0)

# Leer solo N líneas
mapfile -t first_10 -n 10 < /var/log/syslog

# Saltar N líneas iniciales
mapfile -t rest -s 5 < data.txt   # skip primeras 5 líneas
```

### mapfile vs for loop

```bash
# MAL: word splitting rompe líneas con espacios
for line in $(cat file.txt); do    # cada PALABRA es un item
    echo "$line"
done

# BIEN: mapfile preserva líneas
mapfile -t lines < file.txt
for line in "${lines[@]}"; do      # cada LÍNEA es un item
    echo "$line"
done

# BIEN alternativo: while read
while IFS= read -r line; do
    echo "$line"
done < file.txt
```

---

## 10. Patrones comunes

### 10.1 Array como stack (LIFO)

```bash
stack=()

# Push
stack+=("first")
stack+=("second")
stack+=("third")

# Pop (tomar y eliminar el último)
top="${stack[-1]}"
unset 'stack[-1]'
echo "Popped: $top"        # third
echo "Remaining: ${stack[@]}"  # first second
```

### 10.2 Array como set (sin duplicados)

```bash
# Usando array asociativo como set
declare -A seen
input=("apple" "banana" "apple" "cherry" "banana")

unique=()
for item in "${input[@]}"; do
    if [[ ! -v seen["$item"] ]]; then
        seen["$item"]=1
        unique+=("$item")
    fi
done
echo "${unique[@]}"  # apple banana cherry
```

### 10.3 Contador de frecuencias

```bash
declare -A freq

words=("the" "cat" "sat" "on" "the" "mat" "the" "cat")

for word in "${words[@]}"; do
    ((freq["$word"]++))
done

for word in "${!freq[@]}"; do
    echo "$word: ${freq[$word]}"
done
# on: 1
# sat: 1
# mat: 1
# cat: 2
# the: 3
```

### 10.4 Lookup table

```bash
declare -A http_status=(
    [200]="OK"
    [301]="Moved Permanently"
    [404]="Not Found"
    [500]="Internal Server Error"
)

code=404
echo "HTTP $code: ${http_status[$code]}"
# HTTP 404: Not Found

# Con valor por defecto:
code=418
msg="${http_status[$code]:-Unknown Status}"
echo "HTTP $code: $msg"
# HTTP 418: Unknown Status
```

### 10.5 Parsear CSV a arrays

```bash
# Leer un CSV simple (sin quotes ni escapes)
declare -A user

while IFS=, read -r name email role; do
    echo "Name: $name, Email: $email, Role: $role"
done < users.csv

# Almacenar con clave:
declare -A emails
while IFS=, read -r name email role; do
    emails["$name"]="$email"
done < users.csv

echo "Alice's email: ${emails[Alice]}"
```

---

## 11. Sparse arrays y trampas

### 11.1 Indices no consecutivos

```bash
arr=()
arr[0]="zero"
arr[5]="five"
arr[10]="ten"

echo "${#arr[@]}"      # 3 (tres elementos, no 11)
echo "${!arr[@]}"      # 0 5 10

# Iterar con for-in ignora los huecos:
for val in "${arr[@]}"; do
    echo "$val"        # zero, five, ten
done

# Iterar con índice numérico es PELIGROSO con sparse arrays:
for ((i=0; i<${#arr[@]}; i++)); do
    echo "$i: ${arr[$i]}"   # 0: zero, 1: (vacío!), 2: (vacío!)
done
# ¡Solo itera 3 veces pero accede a índices 0,1,2 — no 0,5,10!

# CORRECTO: iterar sobre los índices reales
for i in "${!arr[@]}"; do
    echo "$i: ${arr[$i]}"   # 0: zero, 5: five, 10: ten
done
```

### 11.2 Variables como índice en asociativo

```bash
declare -A data

key="my key"
data["$key"]="value"

# SIN comillas: falla
echo "${data[$key]}"     # Puede funcionar, pero es frágil
# CON comillas: seguro
echo "${data["$key"]}"   # value

# En un for, las comillas son obligatorias si las claves tienen espacios
```

---

## 12. Diferencias con arrays en C y Rust

| Aspecto | Bash array | C array | Rust Vec |
|---------|-----------|---------|----------|
| Tipado | Solo strings | Un tipo fijo | Un tipo fijo |
| Tamaño | Dinámico | Fijo (stack) | Dinámico (heap) |
| Sparse | Sí (huecos) | No | No |
| Índice negativo | Sí (`[-1]`) | Undefined behavior | Panic |
| Asociativo | `declare -A` | No (usar struct) | `HashMap` |
| Pasar a función | Por nombre o expansión | Puntero + size | `&[T]` o `&Vec<T>` |
| Rendimiento | Lento (interpretado) | Rápido (stack/heap) | Rápido (heap) |
| Uso principal | Listas de archivos, args | Datos numéricos | Colecciones generales |

**Regla práctica**: si necesitas arrays de más de ~100 elementos o procesamiento
intensivo, Bash no es la herramienta correcta. Usa Python, AWK, o un lenguaje
compilado.

---

## 13. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| `echo $arr` sin `[@]` | Solo imprime `arr[0]` | `echo "${arr[@]}"` |
| `echo ${arr[@]}` sin comillas | Word splitting rompe elementos con espacios | `"${arr[@]}"` con comillas |
| Olvidar `declare -A` | Asociativo se trata como indexado, falla silenciosamente | Siempre `declare -A` |
| `for i in ${!arr[@]}` sin comillas | Falla si claves tienen espacios | `"${!arr[@]}"` |
| `${#arr}` sin `[@]` | Longitud de `arr[0]` (string length), no del array | `${#arr[@]}` |
| `arr=($(cmd))` | Word splitting + globbing en output | `mapfile -t arr < <(cmd)` |
| Iterar con `((i=0; i<len; i++))` en sparse | Accede a índices inexistentes | `for i in "${!arr[@]}"` |
| `unset arr[i]` sin comillas ni quotes | Glob expansion en `[i]` | `unset 'arr[i]'` |
| Confundir `@` y `*` | `*` une todo en un string, `@` preserva separación | `@` para iterar, `*` para join |
| Asignar array con `=` en vez de `+=` | Sobreescribe todo el array | `+=` para append |

---

## 14. Ejercicios

### Ejercicio 1 — Declaración e indexación

**Predicción**: ¿qué imprime cada línea?

```bash
arr=("one" "two" "three" "four" "five")
echo "${arr[0]}"
echo "${arr[-1]}"
echo "${#arr[@]}"
echo "${arr[@]:1:2}"
```

<details>
<summary>Respuesta</summary>

```
one
five
5
two three
```

- `arr[0]` → primer elemento: `one`
- `arr[-1]` → último elemento: `five`
- `${#arr[@]}` → número de elementos: 5
- `${arr[@]:1:2}` → desde índice 1, tomar 2 elementos: `two three`
</details>

---

### Ejercicio 2 — La trampa de declare -A

**Predicción**: ¿qué imprime este script?

```bash
oops=(["name"]="Alice" ["age"]="30")
echo "Length: ${#oops[@]}"
echo "name: ${oops["name"]}"
echo "Index 0: ${oops[0]}"
```

<details>
<summary>Respuesta</summary>

```
Length: 1
name:
Index 0: 30
```

Sin `declare -A`, Bash trata esto como array **indexado**. Las claves `name`
y `age` se evalúan aritméticamente:
- `name` → variable no definida → valor 0
- `age` → variable no definida → valor 0

Ambas se asignan a índice 0. La segunda (`"30"`) sobreescribe la primera (`"Alice"`).
Resultado: un array con 1 elemento en índice 0.

Correcto:
```bash
declare -A oops=(["name"]="Alice" ["age"]="30")
```
</details>

---

### Ejercicio 3 — @ vs * en for

**Predicción**: ¿cuántas iteraciones hace cada bucle?

```bash
data=("hello world" "foo bar" "baz")

echo "--- Loop A ---"
for item in "${data[@]}"; do echo "[$item]"; done

echo "--- Loop B ---"
for item in "${data[*]}"; do echo "[$item]"; done

echo "--- Loop C ---"
for item in ${data[@]}; do echo "[$item]"; done
```

<details>
<summary>Respuesta</summary>

```
--- Loop A ---
[hello world]
[foo bar]
[baz]
--- Loop B ---
[hello world foo bar baz]
--- Loop C ---
[hello]
[world]
[foo]
[bar]
[baz]
```

- **Loop A** (3 iteraciones): `"${data[@]}"` preserva cada elemento como un
  word separado. Correcto.
- **Loop B** (1 iteración): `"${data[*]}"` une todo con el primer carácter de
  IFS (espacio). Es un solo string gigante.
- **Loop C** (5 iteraciones): `${data[@]}` sin comillas. Word splitting rompe
  cada elemento en sus palabras. "hello world" se convierte en dos items.

Regla: **siempre** `"${array[@]}"` para iterar.
</details>

---

### Ejercicio 4 — Sparse arrays

**Predicción**: ¿qué imprime este script?

```bash
arr=("a" "b" "c" "d" "e")
unset 'arr[1]'
unset 'arr[3]'

echo "Length: ${#arr[@]}"
echo "Indices: ${!arr[@]}"
echo "Values: ${arr[@]}"
echo "arr[1]: '${arr[1]}'"
echo "arr[3]: '${arr[3]}'"
```

<details>
<summary>Respuesta</summary>

```
Length: 3
Indices: 0 2 4
Values: a c e
arr[1]: ''
arr[3]: ''
```

Después de `unset 'arr[1]'` y `unset 'arr[3]'`:
```
Antes: [0]=a [1]=b [2]=c [3]=d [4]=e  (5 elementos)
Después: [0]=a [2]=c [4]=e            (3 elementos, sparse)
```

Los índices 1 y 3 ya no existen. `${arr[1]}` retorna string vacío (no error).
`${#arr[@]}` cuenta los elementos que existen (3), no el índice máximo + 1.
Los arrays Bash son **sparse** por naturaleza después de eliminar elementos.
</details>

---

### Ejercicio 5 — Contador de frecuencias

**Predicción**: ¿qué valores tiene `freq` después de ejecutar?

```bash
declare -A freq
words=("bash" "bash" "bash" "python" "python" "c")

for w in "${words[@]}"; do
    ((freq["$w"]++))
done
```

<details>
<summary>Respuesta</summary>

```
freq["bash"]=3
freq["python"]=2
freq["c"]=1
```

`((freq["$w"]++))` usa expansión aritmética. La primera vez que se accede
a una clave inexistente, su valor es 0 (Bash trata strings vacíos como 0
en contexto aritmético). Luego `++` incrementa.

Ejecución paso a paso:
```
"bash"   → freq["bash"]   = 0+1 = 1
"bash"   → freq["bash"]   = 1+1 = 2
"bash"   → freq["bash"]   = 2+1 = 3
"python" → freq["python"] = 0+1 = 1
"python" → freq["python"] = 1+1 = 2
"c"      → freq["c"]      = 0+1 = 1
```
</details>

---

### Ejercicio 6 — Pasar array a función

**Predicción**: ¿qué imprime cada función?

```bash
func_args() {
    echo "Count: $#"
    for item in "$@"; do echo "  $item"; done
}

func_nameref() {
    local -n ref=$1
    echo "Count: ${#ref[@]}"
    for item in "${ref[@]}"; do echo "  $item"; done
}

data=("hello world" "foo" "bar baz")
echo "=== By args ==="
func_args "${data[@]}"
echo "=== By nameref ==="
func_nameref data
```

<details>
<summary>Respuesta</summary>

```
=== By args ===
Count: 3
  hello world
  foo
  bar baz
=== By nameref ===
Count: 3
  hello world
  foo
  bar baz
```

Ambas producen el mismo resultado, pero de forma diferente:

- **func_args**: recibe 3 argumentos posicionales. `$#` = 3. Los espacios
  dentro de "hello world" se preservan porque usamos `"${data[@]}"`.
- **func_nameref**: `local -n ref=data` crea un alias a la variable `data`.
  `${ref[@]}` accede directamente al array original.

La diferencia importa al **modificar**: con nameref puedes cambiar el array
original. Con args, solo recibes una copia (los argumentos posicionales).
</details>

---

### Ejercicio 7 — mapfile

**Predicción**: ¿cuál es la diferencia entre estas dos formas de leer?

```bash
# Forma A
arr_a=($(echo -e "hello world\nfoo bar\nbaz"))

# Forma B
mapfile -t arr_b < <(echo -e "hello world\nfoo bar\nbaz")

echo "A length: ${#arr_a[@]}"
echo "B length: ${#arr_b[@]}"
```

<details>
<summary>Respuesta</summary>

```
A length: 5
B length: 3
```

- **Forma A**: `$(...)` produce output, luego word splitting lo divide por
  espacios y newlines. Resultado: 5 words (`hello`, `world`, `foo`, `bar`, `baz`).
- **Forma B**: `mapfile -t` lee línea por línea. Resultado: 3 líneas
  (`hello world`, `foo bar`, `baz`).

```
Forma A: word splitting
"hello world\nfoo bar\nbaz" → [hello] [world] [foo] [bar] [baz]

Forma B: line splitting
"hello world\nfoo bar\nbaz" → [hello world] [foo bar] [baz]
```

`mapfile` preserva las líneas intactas. `$(...)` en contexto de array rompe
por cualquier whitespace (espacio, tab, newline). Siempre preferir `mapfile`.
</details>

---

### Ejercicio 8 — Set con array asociativo

**Predicción**: ¿qué imprime?

```bash
declare -A seen
input=("banana" "apple" "cherry" "apple" "banana" "date" "cherry" "cherry")
unique=()

for item in "${input[@]}"; do
    if [[ ! -v seen["$item"] ]]; then
        seen["$item"]=1
        unique+=("$item")
    fi
done

echo "Count: ${#unique[@]}"
echo "Items: ${unique[@]}"
```

<details>
<summary>Respuesta</summary>

```
Count: 4
Items: banana apple cherry date
```

El array asociativo `seen` actúa como un set. `-v seen["$item"]` verifica
si la clave existe (Bash 4.2+). Solo la primera ocurrencia de cada valor
se añade a `unique`.

```
"banana" → seen no tiene "banana" → añadir, seen["banana"]=1
"apple"  → seen no tiene "apple"  → añadir, seen["apple"]=1
"cherry" → seen no tiene "cherry" → añadir, seen["cherry"]=1
"apple"  → seen YA tiene "apple"  → skip
"banana" → seen YA tiene "banana" → skip
"date"   → seen no tiene "date"   → añadir, seen["date"]=1
"cherry" → seen YA tiene "cherry" → skip
"cherry" → seen YA tiene "cherry" → skip
```

Nota: el orden de `unique` es el orden de primera aparición, no alfabético.
</details>

---

### Ejercicio 9 — Stack con array

**Predicción**: ¿qué imprime este script que simula undo/redo?

```bash
undo_stack=()
redo_stack=()

do_action() {
    undo_stack+=("$1")
    redo_stack=()  # limpiar redo al hacer nueva acción
    echo "Did: $1"
}

undo() {
    if [[ ${#undo_stack[@]} -eq 0 ]]; then echo "Nothing to undo"; return; fi
    local action="${undo_stack[-1]}"
    unset 'undo_stack[-1]'
    redo_stack+=("$action")
    echo "Undid: $action"
}

do_action "write A"
do_action "write B"
do_action "write C"
undo
undo
echo "Undo stack: ${undo_stack[@]}"
echo "Redo stack: ${redo_stack[@]}"
```

<details>
<summary>Respuesta</summary>

```
Did: write A
Did: write B
Did: write C
Undid: write C
Undid: write B
Undo stack: write A
Redo stack: write C write B
```

Estado paso a paso:
```
do_action "write A" → undo=[write A], redo=[]
do_action "write B" → undo=[write A, write B], redo=[]
do_action "write C" → undo=[write A, write B, write C], redo=[]
undo                → pop "write C", undo=[write A, write B], redo=[write C]
undo                → pop "write B", undo=[write A], redo=[write C, write B]
```

El array Bash funciona como stack con `+=` (push) y `unset 'arr[-1]'` (pop).
`${arr[-1]}` accede al último elemento (Bash 4.3+).
</details>

---

### Ejercicio 10 — Lookup table con default

**Predicción**: ¿qué imprime este script?

```bash
declare -A exit_msg=(
    [0]="Success"
    [1]="General error"
    [2]="Misuse of shell builtin"
    [126]="Permission denied"
    [127]="Command not found"
    [130]="Terminated by Ctrl+C"
)

for code in 0 1 42 127 130; do
    msg="${exit_msg[$code]:-Unknown exit code}"
    printf "Exit %3d: %s\n" "$code" "$msg"
done
```

<details>
<summary>Respuesta</summary>

```
Exit   0: Success
Exit   1: General error
Exit  42: Unknown exit code
Exit 127: Command not found
Exit 130: Terminated by Ctrl+C
```

`${exit_msg[$code]:-Unknown exit code}` usa expansión con default:
- Si la clave existe y tiene valor → retorna el valor
- Si la clave no existe o está vacía → retorna "Unknown exit code"

El código 42 no está en la tabla, así que se usa el default. Todos los demás
tienen entradas definidas.

Este patrón (asociativo como lookup table + `:-` para default) es la forma
idiomática de Bash para mapeos clave→valor con fallback.
</details>