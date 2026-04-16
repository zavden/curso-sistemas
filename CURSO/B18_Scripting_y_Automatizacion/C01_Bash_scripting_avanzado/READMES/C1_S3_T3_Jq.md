# jq para JSON

## 1. Introducción — por qué jq

JSON es el formato de intercambio dominante en APIs, configuraciones y logs estructurados.
Procesarlo con `grep`/`sed`/`awk` es frágil porque esas herramientas operan sobre **líneas**,
mientras que JSON es un **árbol**. `jq` es un procesador de JSON en línea de comandos que
entiende la estructura del documento.

```
                   PIPELINE  jq
    ┌──────────┐              ┌──────────┐
    │  JSON    │──  filtro ──▶│  JSON    │
    │  entrada │              │  salida  │
    └──────────┘              └──────────┘
         │                         │
    stdin / archivo          stdout / archivo
```

```bash
# Instalación
sudo apt install jq          # Debian/Ubuntu
sudo dnf install jq          # Fedora
brew install jq              # macOS

# Versión
jq --version                 # jq-1.7.1
```

---

## 2. Sintaxis fundamental

### 2.1 El filtro identidad y pretty-print

```bash
# Pretty-print (filtro identidad .)
echo '{"name":"Alice","age":30}' | jq '.'
# {
#   "name": "Alice",
#   "age": 30
# }

# Desde archivo
jq '.' data.json

# Compactar salida (-c)
jq -c '.' data.json
# {"name":"Alice","age":30}

# Sin colores (para pipelines)
jq -M '.' data.json

# Raw output — sin comillas (-r)
echo '{"msg":"hello"}' | jq -r '.msg'
# hello    (no "hello")
```

### 2.2 Acceso a campos

```bash
echo '{"user":{"name":"Alice","id":42}}' | jq '.user.name'
# "Alice"

# Campo con caracteres especiales
echo '{"my-key":1}' | jq '.["my-key"]'
# 1

# Campo opcional (? evita error si no existe)
echo '{}' | jq '.missing?'
# (sin salida, sin error)

echo '{}' | jq '.missing'
# null
```

### 2.3 Acceso a arrays

```bash
data='[10,20,30,40,50]'

echo "$data" | jq '.[0]'      # 10
echo "$data" | jq '.[-1]'     # 50
echo "$data" | jq '.[1:3]'    # [20,30]
echo "$data" | jq '.[:2]'     # [10,20]
echo "$data" | jq '.[-2:]'    # [40,50]
echo "$data" | jq 'length'    # 5
```

### 2.4 Iterador `.[]`

```bash
echo '[1,2,3]' | jq '.[]'
# 1
# 2
# 3

echo '{"a":1,"b":2}' | jq '.[]'
# 1
# 2

# Obtener keys
echo '{"a":1,"b":2}' | jq 'keys'
# ["a","b"]
```

---

## 3. Pipe y construcción

### 3.1 Pipe interno `|`

`jq` tiene su propio operador pipe que encadena filtros, análogo al `|` de shell:

```bash
echo '{"users":[{"name":"Alice"},{"name":"Bob"}]}' \
  | jq '.users[] | .name'
# "Alice"
# "Bob"
```

```
    .users        .[]          .name
  ┌────────┐   ┌────────┐   ┌────────┐
  │ object ├──▶│  array ├──▶│ string │
  │  → arr │   │  → ele │   │  → val │
  └────────┘   └────────┘   └────────┘
```

### 3.2 Construir objetos y arrays

```bash
# Construir nuevo objeto
echo '{"first":"Alice","last":"Smith","age":30}' \
  | jq '{full_name: (.first + " " + .last), age}'
# {"full_name": "Alice Smith", "age": 30}

# Construir array
echo '{"a":1,"b":2,"c":3}' | jq '[.a, .c]'
# [1, 3]

# Envolver iterador en array — MUY COMÚN
echo '{"users":[{"name":"Alice","role":"admin"},{"name":"Bob","role":"user"}]}' \
  | jq '[.users[] | .name]'
# ["Alice", "Bob"]
```

### 3.3 Operador coma — múltiples salidas

```bash
echo '{"a":1,"b":2,"c":3}' | jq '.a, .c'
# 1
# 3
```

---

## 4. Filtros esenciales

### 4.1 `select` — filtrar elementos

```bash
data='[
  {"name":"Alice","age":30,"role":"admin"},
  {"name":"Bob","age":25,"role":"user"},
  {"name":"Carol","age":35,"role":"admin"}
]'

# Filtrar por condición
echo "$data" | jq '.[] | select(.role == "admin")'
# {"name":"Alice","age":30,"role":"admin"}
# {"name":"Carol","age":35,"role":"admin"}

# Envolver resultado en array
echo "$data" | jq '[.[] | select(.age > 28)]'
# [{"name":"Alice",...},{"name":"Carol",...}]

# Múltiples condiciones
echo "$data" | jq '[.[] | select(.role == "admin" and .age > 32)]'
# [{"name":"Carol","age":35,"role":"admin"}]

# Negar condición
echo "$data" | jq '[.[] | select(.role != "admin")]'

# Comprobar existencia de campo
echo '[{"a":1,"b":2},{"a":3}]' | jq '.[] | select(has("b"))'
# {"a":1,"b":2}
```

### 4.2 `map` — transformar cada elemento

```bash
echo '[1,2,3,4,5]' | jq 'map(. * 2)'
# [2,4,6,8,10]

# map es azúcar sintáctico para [.[] | ...]
echo '[1,2,3]' | jq '[.[] | . + 10]'
# [11,12,13]

# map sobre objetos
echo "$data" | jq 'map({name, senior: (.age >= 30)})'
# [{"name":"Alice","senior":true},{"name":"Bob","senior":false},{"name":"Carol","senior":true}]

# map + select (filtrar + transformar)
echo "$data" | jq '[.[] | select(.role == "admin") | .name]'
# ["Alice","Carol"]
```

### 4.3 `sort_by`, `group_by`, `unique_by`

```bash
# Ordenar
echo "$data" | jq 'sort_by(.age)'
echo "$data" | jq 'sort_by(.age) | reverse'

# Agrupar
echo "$data" | jq 'group_by(.role)'
# [[admin,admin],[user]]

# Contar por grupo
echo "$data" | jq 'group_by(.role) | map({role: .[0].role, count: length})'
# [{"role":"admin","count":2},{"role":"user","count":1}]

# Únicos
echo '[1,2,2,3,3,3]' | jq 'unique'
# [1,2,3]

echo "$data" | jq '[.[] | .role] | unique'
# ["admin","user"]
```

### 4.4 `reduce` — acumular

```bash
echo '[1,2,3,4,5]' | jq 'reduce .[] as $x (0; . + $x)'
# 15

# Sintaxis: reduce EXPR as $VAR (INIT; UPDATE)
#   INIT  = valor inicial del acumulador (.)
#   UPDATE = expresión que actualiza . usando $x

# Construir objeto desde array de pares
echo '[["a",1],["b",2],["c",3]]' \
  | jq 'reduce .[] as $p ({}; . + {($p[0]): $p[1]})'
# {"a":1,"b":2,"c":3}
```

---

## 5. Variables y funciones

### 5.1 Variables con `as`

```bash
echo '{"width":3,"height":4}' \
  | jq '.width as $w | .height as $h | {area: ($w * $h), perimeter: (2*($w+$h))}'
# {"area":12,"perimeter":14}
```

### 5.2 Condicionales `if-then-else`

```bash
echo '[1,-2,3,-4,5]' | jq 'map(if . > 0 then "pos" else "neg" end)'
# ["pos","neg","pos","neg","pos"]

# elif
echo '75' | jq '
  if . >= 90 then "A"
  elif . >= 80 then "B"
  elif . >= 70 then "C"
  else "F"
  end
'
# "C"
```

### 5.3 Funciones propias (`def`)

```bash
echo '[3,1,4,1,5]' | jq '
  def square: . * .;
  def sum: reduce .[] as $x (0; . + $x);
  def mean: sum / length;
  map(square) | mean
'
# 10.4

# Función con argumento
echo '["hello","world"]' | jq '
  def wrap(tag): "<" + tag + ">" + . + "</" + tag + ">";
  map(wrap("b"))
'
# ["<b>hello</b>","<b>world</b>"]
```

### 5.4 `try-catch` y `?//`

```bash
# try evita que un error detenga el pipeline
echo '[1,"two",3]' | jq '[.[] | try (. + 1)]'
# [2, 4]    ("two" + 1 falla silenciosamente)

# Operador alternativo ?// (jq 1.7+)
echo '{"a":1}' | jq '.b // "default"'
# "default"

# // es el operador "alternative" (si null o false → usar derecha)
echo 'null' | jq '. // "fallback"'
# "fallback"
```

---

## 6. Transformaciones avanzadas

### 6.1 `to_entries`, `from_entries`, `with_entries`

```bash
echo '{"name":"Alice","age":30}' | jq 'to_entries'
# [{"key":"name","value":"Alice"},{"key":"age","value":30}]

echo '[{"key":"x","value":1}]' | jq 'from_entries'
# {"x":1}

# with_entries = to_entries | map(f) | from_entries
echo '{"name":"Alice","age":30,"role":"admin"}' \
  | jq 'with_entries(select(.key != "age"))'
# {"name":"Alice","role":"admin"}

# Prefijo en todas las keys
echo '{"host":"localhost","port":8080}' \
  | jq 'with_entries(.key = "db_" + .key)'
# {"db_host":"localhost","db_port":8080}
```

### 6.2 `paths`, `getpath`, `setpath`

```bash
echo '{"a":{"b":1,"c":{"d":2}}}' | jq '[paths]'
# [["a"],["a","b"],["a","c"],["a","c","d"]]

echo '{"a":{"b":1}}' | jq 'getpath(["a","b"])'
# 1

echo '{"a":{"b":1}}' | jq 'setpath(["a","c"]; 99)'
# {"a":{"b":1,"c":99}}
```

### 6.3 `@base64`, `@uri`, `@csv`, `@tsv`, `@html`

```bash
echo '"hello world"' | jq '@uri'
# "hello%20world"

echo '"<script>"' | jq '@html'
# "&lt;script&gt;"

echo '[[1,"a"],[2,"b"]]' | jq '.[] | @csv'
# "1,\"a\""
# "2,\"b\""

# Base64
echo '"secret"' | jq '@base64'
# "c2VjcmV0"

echo '"c2VjcmV0"' | jq '@base64d'
# "secret"
```

### 6.4 `flatten`, `add`, `any`, `all`

```bash
echo '[[1,2],[3,[4,5]]]' | jq 'flatten'
# [1,2,3,4,5]

echo '[[1,2],[3,4]]' | jq 'flatten(1)'
# [1,2,3,4]   (un solo nivel)

echo '["hello"," ","world"]' | jq 'add'
# "hello world"

echo '[1,2,3]' | jq 'add'
# 6

echo '[1,2,3]' | jq 'any(. > 2)'
# true

echo '[1,2,3]' | jq 'all(. > 0)'
# true
```

---

## 7. Integración con shell

### 7.1 Variables de shell en jq

```bash
# --arg: pasar string
name="Alice"
jq --arg n "$name" '.[] | select(.name == $n)' users.json

# --argjson: pasar número/booleano/null
min_age=25
jq --argjson age "$min_age" '.[] | select(.age >= $age)' users.json

# --slurpfile: cargar archivo JSON como variable
jq --slurpfile allowed roles.json '
  .[] | select(.role == $allowed[0].admin_role)
' users.json

# $ENV — acceder a variables de entorno
export API_KEY="abc123"
jq -n '$ENV.API_KEY'
# "abc123"

jq -n 'env.HOME'
# "/home/user"
```

### 7.2 Leer salida de jq en variables Bash

```bash
# Una variable
user_name=$(jq -r '.name' config.json)

# Múltiples variables (evitar subshells múltiples)
read -r name age role < <(
  jq -r '[.name, (.age|tostring), .role] | join("\t")' config.json
)
echo "Name=$name Age=$age Role=$role"

# Iterar sobre array
jq -r '.users[] | .name' data.json | while IFS= read -r name; do
  echo "Processing: $name"
done

# Iterar con múltiples campos — @tsv
jq -r '.users[] | [.name, .email] | @tsv' data.json \
  | while IFS=$'\t' read -r name email; do
    echo "User: $name <$email>"
  done

# Array Bash desde JSON array
mapfile -t names < <(jq -r '.users[].name' data.json)
echo "${names[0]}"
```

### 7.3 Modificar archivos JSON

```bash
# jq no tiene -i como sed — usar patrón sponge o temporal
# Opción 1: moreutils sponge
jq '.version = "2.0"' config.json | sponge config.json

# Opción 2: archivo temporal (más portable)
tmp=$(mktemp)
jq '.version = "2.0"' config.json > "$tmp" && mv "$tmp" config.json

# Opción 3: variable intermedia (archivos pequeños)
updated=$(jq '.version = "2.0"' config.json)
echo "$updated" > config.json

# Añadir elemento a array
jq '.users += [{"name":"Dave","age":28}]' data.json > "$tmp" && mv "$tmp" data.json

# Eliminar campo
jq 'del(.deprecated_field)' config.json | sponge config.json

# Eliminar de array por condición
jq '.users |= map(select(.active))' data.json | sponge config.json
```

---

## 8. Integración con APIs (curl + jq)

### 8.1 Patrón básico

```bash
# GET + extraer campos
curl -s "https://api.github.com/users/torvalds" \
  | jq '{name, public_repos, followers}'

# Listar repos ordenados por estrellas
curl -s "https://api.github.com/users/torvalds/repos?per_page=100" \
  | jq 'sort_by(.stargazers_count) | reverse | .[:5] | .[] | {name, stars: .stargazers_count}'
```

### 8.2 Paginación

```bash
#!/bin/bash
page=1
while true; do
  response=$(curl -s "https://api.example.com/items?page=$page&per_page=50")
  count=$(echo "$response" | jq 'length')

  echo "$response" | jq -r '.[] | .name'

  (( count < 50 )) && break
  (( page++ ))
done
```

### 8.3 Construir JSON para POST

```bash
# Construir payload con --arg
payload=$(jq -n \
  --arg name "$USER_NAME" \
  --arg email "$USER_EMAIL" \
  --argjson age "$USER_AGE" \
  '{name: $name, email: $email, age: $age}'
)

curl -s -X POST "https://api.example.com/users" \
  -H "Content-Type: application/json" \
  -d "$payload"

# Alternativa: jq como template
jq -n '{
  name: "Alice",
  settings: {
    theme: "dark",
    notifications: true,
    tags: ["admin","active"]
  }
}' | curl -s -X POST "https://api.example.com/users" \
  -H "Content-Type: application/json" \
  -d @-
```

### 8.4 Procesar respuestas con errores

```bash
response=$(curl -s -w "\n%{http_code}" "https://api.example.com/data")
http_code=$(echo "$response" | tail -1)
body=$(echo "$response" | sed '$d')

if (( http_code >= 200 && http_code < 300 )); then
  echo "$body" | jq '.result'
else
  echo "$body" | jq -r '.error.message // "Unknown error"' >&2
  exit 1
fi
```

---

## 9. Recetas de producción

### 9.1 Merge de archivos JSON

```bash
# Merge superficial (segundo gana en conflictos)
jq -s '.[0] * .[1]' defaults.json overrides.json

# Merge profundo
jq -s '
  def deep_merge:
    reduce .[] as $obj ({}; . as $base |
      $obj | to_entries | reduce .[] as $e ($base;
        if ($e.value | type) == "object" and (.[$e.key] | type) == "object"
        then .[$e.key] = ([.[$e.key], $e.value] | deep_merge)
        else .[$e.key] = $e.value
        end
      )
    );
  deep_merge
' defaults.json overrides.json
```

### 9.2 JSON Lines (NDJSON)

```bash
# Leer NDJSON — cada línea es un JSON independiente
# --slurp (-s) convierte líneas en un array
cat logs.jsonl | jq -s 'map(select(.level == "error"))'

# Emitir NDJSON — -c compacta cada salida en una línea
jq -c '.users[]' data.json > users.jsonl

# Streaming de NDJSON (sin cargar todo en memoria)
while IFS= read -r line; do
  echo "$line" | jq -r '.timestamp + " " + .message'
done < logs.jsonl
```

### 9.3 Comparar dos JSON

```bash
# Diferencias en keys
diff <(jq -S 'keys' a.json) <(jq -S 'keys' b.json)

# Diferencia completa (ordenar keys primero)
diff <(jq -S '.' a.json) <(jq -S '.' b.json)

# ¿Son equivalentes? (ignorando orden de keys)
if jq -e -S '.' a.json | diff - <(jq -S '.' b.json) > /dev/null 2>&1; then
  echo "Iguales"
else
  echo "Diferentes"
fi
```

### 9.4 Validar JSON

```bash
# Validar sintaxis
if jq empty file.json 2>/dev/null; then
  echo "JSON válido"
else
  echo "JSON inválido"
fi

# Validar esquema básico (campos requeridos)
jq '
  def validate:
    if (.name | type) != "string" then error("name must be string")
    elif (.age | type) != "number" then error("age must be number")
    elif .age < 0 then error("age must be positive")
    else .
    end;
  validate
' input.json
```

### 9.5 Generar reportes

```bash
# Tabla desde JSON
jq -r '
  ["NAME","AGE","ROLE"],
  (.[] | [.name, (.age|tostring), .role])
  | @tsv
' users.json | column -t

# Salida:
# NAME    AGE  ROLE
# Alice   30   admin
# Bob     25   user
# Carol   35   admin

# CSV con cabeceras
jq -r '
  (.[0] | keys_unsorted) as $keys
  | $keys, (.[] | [.[$keys[]]])
  | @csv
' users.json
```

---

## 10. Diagrama de flujo de filtros

```
    Entrada JSON
         │
         ▼
    ┌─────────┐     Acceso       ┌──────────┐
    │    .     │────────────────▶│ .field    │
    │ identidad│                  │ .[n]     │
    └─────────┘                  │ .[]      │
         │                       └──────────┘
         │ pipe (|)                   │
         ▼                            ▼
    ┌─────────┐     Filtrar      ┌──────────┐
    │ select  │◀────────────────│ elementos │
    │ has     │                  └──────────┘
    │ type    │
    └─────────┘
         │
         ▼
    ┌─────────┐     Transformar  ┌──────────┐
    │  map    │────────────────▶│ resultado │
    │ reduce  │                  └──────────┘
    │ sort_by │
    │group_by │
    └─────────┘
         │
         ▼
    ┌─────────┐     Construir    ┌──────────┐
    │ {k: v}  │────────────────▶│  salida   │
    │ [...]   │                  │  JSON     │
    └─────────┘                  └──────────┘
```

---

## 11. Tabla de referencia rápida

| Filtro | Acción | Ejemplo |
|--------|--------|---------|
| `.` | Identidad / pretty-print | `jq '.'` |
| `.field` | Acceder a campo | `jq '.name'` |
| `.[n]` | Índice de array | `jq '.[0]'` |
| `.[]` | Iterar todos | `jq '.[]'` |
| `\|` | Pipe interno | `jq '.[] \| .name'` |
| `select(cond)` | Filtrar | `select(.age > 18)` |
| `map(f)` | Transformar array | `map(. + 1)` |
| `sort_by(f)` | Ordenar | `sort_by(.name)` |
| `group_by(f)` | Agrupar | `group_by(.role)` |
| `unique` / `unique_by` | Deduplicar | `unique_by(.id)` |
| `reduce` | Acumular | `reduce .[] as $x (0; .+$x)` |
| `keys` / `values` | Extraer keys/values | `jq 'keys'` |
| `length` | Tamaño | `jq 'length'` |
| `type` | Tipo del valor | `jq 'type'` |
| `to_entries` | Obj → array de {key,value} | `to_entries` |
| `from_entries` | Array de {key,value} → obj | `from_entries` |
| `with_entries(f)` | Transformar entries | `with_entries(.key\|="prefix_"+.)` |
| `has(k)` | ¿Tiene key? | `has("name")` |
| `in(obj)` | ¿Key existe en obj? | `"name" \| in({"name":1})` |
| `del(path)` | Eliminar | `del(.tmp)` |
| `flatten` | Aplanar arrays anidados | `flatten` |
| `add` | Sumar/concatenar array | `add` |
| `any(cond)` / `all(cond)` | Cuantificadores | `any(. > 5)` |
| `limit(n; f)` | Primeros n resultados | `limit(3; .[])` |
| `first(f)` / `last(f)` | Primero/último | `first(.[] \| select(.ok))` |
| `ascii_downcase` | Minúsculas | `map(ascii_downcase)` |
| `split(s)` / `join(s)` | Partir/unir strings | `split(",")` |
| `test(re)` | Regex match (bool) | `select(.name \| test("^A"))` |
| `capture(re)` | Regex con grupos nombrados | `capture("(?<y>\\d{4})")` |
| `@csv` / `@tsv` | Formato tabular | `.[] \| @csv` |
| `@base64` / `@base64d` | Codificar/decodificar | `@base64` |
| `@uri` / `@html` | URL-encode / HTML-escape | `@uri` |
| `--arg k v` | Inyectar string desde shell | `--arg name "$n"` |
| `--argjson k v` | Inyectar JSON desde shell | `--argjson age 30` |
| `-r` | Raw output (sin comillas) | `jq -r '.name'` |
| `-c` | Compacto (una línea) | `jq -c '.'` |
| `-s` | Slurp (array de entradas) | `jq -s '.'` |
| `-e` | Exit code según resultado | `jq -e '.ok'` |
| `-S` | Ordenar keys en salida | `jq -S '.'` |
| `-n` | Null input (no lee stdin) | `jq -n '{}'` |

---

## 12. Errores comunes

| Error | Causa | Solución |
|-------|-------|----------|
| `Cannot iterate over null` | `.field[]` cuando `.field` no existe | Usar `.field[]?` o `(.field // [])[]` |
| Variable shell no se expande | `'$var'` dentro de comillas simples | Usar `--arg v "$var"` y `$v` dentro de jq |
| `string and number cannot be added` | Concatenar tipos distintos | Convertir: `(.age\|tostring)` |
| Salida con comillas `"hello"` | Falta `-r` | Añadir `-r` para raw output |
| Archivo vacío como salida | Redirigir al mismo archivo | Usar `sponge` o archivo temporal |
| `Cannot index string with string` | Tratar string como objeto | Verificar `type` del dato |
| `null` inesperado | Campo no existe / typo | Verificar con `keys` la estructura real |
| `parse error` en shell | Comillas mal escapadas | Usar comillas simples para filtro jq |
| jq no lee pipe | Falta `-n` cuando no hay stdin | Usar `-n` o proveer entrada |
| `map` devuelve error | Input no es array | Envolver en `[...]` o usar `.[]` directamente |

---

## 13. Ejercicios

### Ejercicio 1 — Extracción básica

Dado el siguiente archivo `servers.json`:

```json
[
  {"hostname": "web01", "ip": "10.0.1.1", "role": "frontend", "cpu": 4, "ram_gb": 8},
  {"hostname": "web02", "ip": "10.0.1.2", "role": "frontend", "cpu": 2, "ram_gb": 4},
  {"hostname": "db01",  "ip": "10.0.2.1", "role": "database", "cpu": 8, "ram_gb": 32},
  {"hostname": "db02",  "ip": "10.0.2.2", "role": "database", "cpu": 8, "ram_gb": 32},
  {"hostname": "cache01","ip": "10.0.3.1", "role": "cache",   "cpu": 2, "ram_gb": 16}
]
```

Escribe un comando `jq` que extraiga solo los hostnames de todos los servidores.

**Predicción**: ¿Qué diferencia hay entre usar `jq '.[] | .hostname'` y `jq '[.[] | .hostname]'`?

<details>
<summary>Ver solución</summary>

```bash
# Versión que produce strings separados (uno por línea)
jq -r '.[] | .hostname' servers.json
# web01
# web02
# db01
# db02
# cache01

# Versión que produce un array JSON
jq '[.[] | .hostname]' servers.json
# ["web01","web02","db01","db02","cache01"]

# También equivalente:
jq 'map(.hostname)' servers.json
```

La primera produce **múltiples salidas** (una por línea), útil para iterar en Bash.
La segunda produce **un array JSON**, útil para encadenar con más filtros jq o guardar como JSON válido.
</details>

---

### Ejercicio 2 — Filtrar con select

Usando `servers.json`, extrae los servidores cuyo rol es `"database"` y que tengan más de 16 GB de RAM.

**Predicción**: Si usas `select(.role == "database") | select(.ram_gb > 16)` vs `select(.role == "database" and .ram_gb > 16)`, ¿producen el mismo resultado?

<details>
<summary>Ver solución</summary>

```bash
jq '[.[] | select(.role == "database" and .ram_gb > 16)]' servers.json
# [
#   {"hostname":"db01","ip":"10.0.2.1","role":"database","cpu":8,"ram_gb":32},
#   {"hostname":"db02","ip":"10.0.2.2","role":"database","cpu":8,"ram_gb":32}
# ]
```

Sí, ambas formas producen el mismo resultado. `select` en cadena es como `AND` lógico porque `select` solo pasa al siguiente filtro los elementos que cumplen la condición. La forma con `and` es más explícita y ligeramente más eficiente (un solo select).
</details>

---

### Ejercicio 3 — Transformar y construir

A partir de `servers.json`, genera un nuevo JSON donde cada servidor tenga solo `hostname` y un campo calculado `total_memory_mb` (ram_gb × 1024).

**Predicción**: ¿Qué pasa si escribes `{hostname, total_memory_mb: .ram_gb * 1024}` sin el punto antes de `hostname`?

<details>
<summary>Ver solución</summary>

```bash
jq 'map({hostname, total_memory_mb: (.ram_gb * 1024)})' servers.json
# [
#   {"hostname":"web01","total_memory_mb":8192},
#   {"hostname":"web02","total_memory_mb":4096},
#   {"hostname":"db01","total_memory_mb":32768},
#   {"hostname":"db02","total_memory_mb":32768},
#   {"hostname":"cache01","total_memory_mb":16384}
# ]
```

`{hostname}` es azúcar sintáctico de jq para `{hostname: .hostname}`. Funciona perfectamente sin el punto — jq infiere que quieres `.hostname` como valor cuando la key coincide con un campo del input.
</details>

---

### Ejercicio 4 — group_by y estadísticas

Agrupa los servidores por `role` y para cada grupo calcula: número de servidores, total de CPUs y total de RAM.

**Predicción**: ¿Qué estructura devuelve `group_by(.role)` antes de aplicar el `map`?

<details>
<summary>Ver solución</summary>

```bash
jq '
  group_by(.role) | map({
    role: .[0].role,
    count: length,
    total_cpu: (map(.cpu) | add),
    total_ram_gb: (map(.ram_gb) | add)
  })
' servers.json
# [
#   {"role":"cache","count":1,"total_cpu":2,"total_ram_gb":16},
#   {"role":"database","count":2,"total_cpu":16,"total_ram_gb":64},
#   {"role":"frontend","count":2,"total_cpu":6,"total_ram_gb":12}
# ]
```

`group_by(.role)` devuelve un **array de arrays**: `[[cache servers], [database servers], [frontend servers]]`. Cada sub-array contiene los objetos que comparten el mismo `role`. Los grupos están ordenados alfabéticamente por el valor del campo.
</details>

---

### Ejercicio 5 — Variables shell y --arg

Escribe un script que reciba un rol como argumento (`$1`) y muestre los hostnames de ese rol.

**Predicción**: ¿Qué pasa si usas `select(.role == "$1")` directamente dentro de las comillas simples de jq?

<details>
<summary>Ver solución</summary>

```bash
#!/bin/bash
# usage: ./find_role.sh database

role="${1:?Usage: $0 <role>}"

jq -r --arg r "$role" '.[] | select(.role == $r) | .hostname' servers.json
```

Si escribes `select(.role == "$1")` dentro de comillas simples, `$1` **no se expande** — Bash no interpreta variables dentro de `'...'`. jq lo interpretaría como el string literal `$1`, que no está definido en jq. Siempre se debe usar `--arg` para inyectar variables de shell en filtros jq.
</details>

---

### Ejercicio 6 — reduce para acumular

Dado un array de transacciones:

```json
[
  {"type":"income","amount":1000},
  {"type":"expense","amount":300},
  {"type":"income","amount":500},
  {"type":"expense","amount":200},
  {"type":"expense","amount":150}
]
```

Usa `reduce` para calcular el balance final (ingresos - gastos).

**Predicción**: En `reduce .[] as $t (0; BODY)`, ¿qué representa `.` dentro del BODY?

<details>
<summary>Ver solución</summary>

```bash
echo '[
  {"type":"income","amount":1000},
  {"type":"expense","amount":300},
  {"type":"income","amount":500},
  {"type":"expense","amount":200},
  {"type":"expense","amount":150}
]' | jq '
  reduce .[] as $t (0;
    if $t.type == "income" then . + $t.amount
    else . - $t.amount
    end
  )
'
# 850
```

Dentro del body de `reduce`, `.` es el **acumulador**, no el input original. Empieza valiendo `0` (el valor inicial) y en cada iteración se actualiza con el resultado de la expresión. `$t` es el elemento actual del array.
</details>

---

### Ejercicio 7 — with_entries y transformación de keys

Dado un objeto de configuración con keys en `camelCase`, conviértelas a `snake_case` (solo el primer nivel):

```json
{"serverName":"web01","maxRetries":3,"baseUrl":"https://api.example.com","timeoutMs":5000}
```

**Predicción**: ¿Qué hace `with_entries(.key |= ...)` internamente paso a paso?

<details>
<summary>Ver solución</summary>

```bash
echo '{"serverName":"web01","maxRetries":3,"baseUrl":"https://api.example.com","timeoutMs":5000}' \
  | jq '
    with_entries(.key |= gsub("(?<a>[A-Z])"; "_" + (.a | ascii_downcase)))
  '
# {
#   "server_name": "web01",
#   "max_retries": 3,
#   "base_url": "https://api.example.com",
#   "timeout_ms": 5000
# }
```

`with_entries(f)` internamente:
1. Convierte `{"serverName":"web01",...}` en `[{"key":"serverName","value":"web01"},...]` (to_entries)
2. Aplica `f` a cada entry — aquí reemplaza mayúsculas por `_` + minúscula en `.key`
3. Convierte de vuelta a objeto (from_entries)

Es la composición `to_entries | map(f) | from_entries`.
</details>

---

### Ejercicio 8 — Procesar NDJSON de logs

Dado un archivo `app.log` en formato JSON Lines:

```
{"ts":"2024-01-15T10:00:00Z","level":"info","msg":"Server started","port":8080}
{"ts":"2024-01-15T10:00:05Z","level":"error","msg":"DB connection failed","code":500}
{"ts":"2024-01-15T10:00:10Z","level":"info","msg":"Retry succeeded"}
{"ts":"2024-01-15T10:00:15Z","level":"error","msg":"Timeout","code":504}
{"ts":"2024-01-15T10:00:20Z","level":"warn","msg":"High memory usage","pct":92}
```

Extrae solo los errores y muestra timestamp y mensaje en formato legible.

**Predicción**: ¿Qué diferencia hay entre usar `jq -s` y procesar línea por línea con `jq`?

<details>
<summary>Ver solución</summary>

```bash
# Opción 1: slurp y filtrar (carga todo en memoria)
jq -s 'map(select(.level == "error")) | .[] | "\(.ts) ERROR: \(.msg)"' app.log

# Opción 2: sin slurp — procesa línea por línea (mejor para archivos grandes)
jq -r 'select(.level == "error") | "\(.ts) ERROR: \(.msg)"' app.log
# 2024-01-15T10:00:05Z ERROR: DB connection failed
# 2024-01-15T10:00:15Z ERROR: Timeout

# Opción 3: resumen por nivel
jq -s 'group_by(.level) | map({level: .[0].level, count: length})' app.log
# [{"level":"error","count":2},{"level":"info","count":2},{"level":"warn","count":1}]
```

`jq -s` (slurp) lee **todas las líneas** y las empaqueta en un array — necesario para `group_by`, `sort_by`, etc., pero consume más memoria. Sin `-s`, jq procesa **cada línea independientemente** — ideal para filtrar o transformar sin cargar todo en memoria, y funciona como streaming para archivos enormes.
</details>

---

### Ejercicio 9 — Construir payload para API

Escribe un script que lea variables de entorno (`DB_HOST`, `DB_PORT`, `DB_NAME`, `DB_USER`) y construya un JSON de configuración con validación.

**Predicción**: ¿Por qué se usa `--arg` en vez de interpolar con `\(env.DB_HOST)` o `$ENV.DB_HOST`?

<details>
<summary>Ver solución</summary>

```bash
#!/bin/bash
set -euo pipefail

: "${DB_HOST:?DB_HOST is required}"
: "${DB_PORT:?DB_PORT is required}"
: "${DB_NAME:?DB_NAME is required}"
: "${DB_USER:?DB_USER is required}"

jq -n \
  --arg host "$DB_HOST" \
  --argjson port "$DB_PORT" \
  --arg name "$DB_NAME" \
  --arg user "$DB_USER" \
  --arg pass "${DB_PASS:-}" \
  '{
    database: {
      host: $host,
      port: $port,
      name: $name,
      credentials: {
        user: $user,
        password: (if $pass == "" then null else $pass end)
      },
      options: {
        ssl: ($host | test("prod") // false),
        pool_size: (if $port == 5432 then 10 else 5 end)
      }
    },
    generated_at: (now | strftime("%Y-%m-%dT%H:%M:%SZ"))
  }'
```

`--arg` es preferible porque:
1. **Seguridad**: escapa correctamente caracteres especiales (comillas, backslash) — no hay riesgo de inyección JSON
2. **Portabilidad**: funciona sin que las variables estén exportadas (a diferencia de `$ENV`)
3. **Tipos**: `--argjson` permite pasar números/booleanos como su tipo JSON real, no como strings
4. `$ENV` requiere que la variable esté `export`ada — `--arg` funciona con variables locales de shell
</details>

---

### Ejercicio 10 — Pipeline completo

Combina `curl`, `jq` y Bash para crear un script que:
1. Consulte la API de GitHub para obtener los repos de un usuario
2. Filtre solo los que no son forks
3. Ordene por estrellas descendente
4. Muestre una tabla con nombre, estrellas y lenguaje

**Predicción**: ¿Por qué se usa `@tsv` con `column -t` en lugar de generar la tabla directamente con jq?

<details>
<summary>Ver solución</summary>

```bash
#!/bin/bash
set -euo pipefail

user="${1:?Usage: $0 <github_username>}"

# Obtener repos (hasta 100)
repos=$(curl -s "https://api.github.com/users/${user}/repos?per_page=100&type=owner")

# Verificar respuesta válida
if ! echo "$repos" | jq empty 2>/dev/null; then
  echo "Error: respuesta no es JSON válido" >&2
  exit 1
fi

# Verificar que no sea un error de la API
if echo "$repos" | jq -e 'type == "object" and has("message")' > /dev/null 2>&1; then
  echo "Error: $(echo "$repos" | jq -r '.message')" >&2
  exit 1
fi

# Filtrar, ordenar, formatear
echo "$repos" | jq -r '
  map(select(.fork | not))
  | sort_by(.stargazers_count) | reverse
  | .[:20]
  | ["REPO","STARS","LANGUAGE"],
    ["----","-----","--------"],
    (.[] | [.name, (.stargazers_count|tostring), (.language // "N/A")])
  | @tsv
' | column -ts $'\t'

# Estadísticas
echo ""
echo "--- Resumen ---"
echo "$repos" | jq -r '
  map(select(.fork | not)) as $own |
  "Total repos propios: \($own | length)",
  "Estrellas totales:   \([$own[].stargazers_count] | add // 0)",
  "Lenguajes:           \([$own[].language // empty] | unique | join(", "))"
'
```

`@tsv` produce campos separados por tabulador, y `column -t` alinea las columnas automáticamente según el ancho real de los datos. jq no tiene funcionalidad nativa para alinear columnas con padding — su `@tsv`/`@csv` solo serializa, no formatea visualmente. La combinación `@tsv | column -t` delega la presentación al programa especializado.
</details>
