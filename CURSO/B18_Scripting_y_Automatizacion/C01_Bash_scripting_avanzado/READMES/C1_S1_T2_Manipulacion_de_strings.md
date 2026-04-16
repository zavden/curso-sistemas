# Manipulación de strings en Bash

## 1. Strings como tipo universal

En Bash **todo es un string**. Números, paths, JSON, booleanos — todo se
almacena y manipula como texto. No hay tipos. La manipulación de strings
es la operación más fundamental del lenguaje.

```bash
x=42        # string "42", no un entero
y="hello"   # string "hello"
z=true      # string "true", no un booleano

# La "aritmética" solo funciona en contextos aritméticos: (( )), let
echo $((x + 1))  # 43 — Bash interpreta "42" como número aquí
```

Bash ofrece una familia de **expansiones de parámetros** (`${var...}`) que
permiten manipular strings sin lanzar procesos externos (`sed`, `awk`, `cut`).
Son más rápidas y más portables.

---

## 2. Longitud

```bash
str="Hello, World!"

echo "${#str}"    # 13

# Útil para validaciones
password="abc"
if (( ${#password} < 8 )); then
    echo "Password too short (${#password} chars, need 8)"
fi
```

**Cuidado con multibyte**: `${#str}` cuenta **caracteres**, no bytes. En
locale UTF-8, un emoji cuenta como 1:

```bash
str="café"
echo "${#str}"    # 4 (no 5, aunque 'é' es 2 bytes en UTF-8)

str="🔥"
echo "${#str}"    # 1
```

---

## 3. Substring: ${var:offset:length}

```bash
str="Hello, World!"
#    0123456789...

echo "${str:0:5}"     # Hello
echo "${str:7:5}"     # World
echo "${str:7}"       # World! (sin length: hasta el final)
echo "${str: -6}"     # orld!  (espacio obligatorio antes del menos)
echo "${str: -6:3}"   # orl
```

```
"Hello, World!"
 ↑              ${str:0:5}  → "Hello"
        ↑       ${str:7:5}  → "World"
        ↑→→→→→  ${str:7}    → "World!"
          ↑←←←  ${str: -6}  → "orld!" (NOTA: espacio antes de -)
```

**¿Por qué el espacio en `${str: -6}`?** Sin espacio, `${str:-6}` se
interpreta como "valor por defecto si vacío" (otra expansión). El espacio
desambigua.

### Extraer extensión y nombre de archivo (sin herramientas externas)

```bash
file="/home/user/document.tar.gz"

# Métodos con substring (frágiles, mejor usar las expansiones de §4-5):
echo "${file:(-6)}"    # tar.gz (los últimos 6 caracteres — hardcodeado)
```

---

## 4. Eliminar prefijo: ${var#pattern} y ${var##pattern}

"Corta desde la izquierda" usando un patrón glob:

```bash
path="/home/user/docs/file.txt"

# # = match más CORTO desde el inicio
echo "${path#*/}"     # home/user/docs/file.txt  (elimina primer /)

# ## = match más LARGO desde el inicio
echo "${path##*/}"    # file.txt  (elimina todo hasta el último /)
```

```
/home/user/docs/file.txt
↑                          #*/   quita el match corto: /
↑→→→→→→→→→→→→→→→↑         ##*/  quita el match largo: /home/user/docs/

#  = greedy corto (elimina lo mínimo)
## = greedy largo (elimina lo máximo)
```

### Usos comunes

```bash
file="document.tar.gz"

# Extensión (eliminar todo antes del primer punto)
echo "${file#*.}"      # tar.gz

# Extensión final (eliminar todo antes del ÚLTIMO punto)
echo "${file##*.}"     # gz

# Nombre sin extensión final
echo "${file%.*}"      # document.tar  (ver §5)

# Basename (sin dirname)
filepath="/usr/local/bin/script.sh"
echo "${filepath##*/}"   # script.sh  (equivalente a basename)
```

---

## 5. Eliminar sufijo: ${var%pattern} y ${var%%pattern}

"Corta desde la derecha":

```bash
path="/home/user/docs/file.txt"

# % = match más CORTO desde el final
echo "${path%/*}"      # /home/user/docs  (elimina último /*)

# %% = match más LARGO desde el final
echo "${path%%/*}"     # (vacío — elimina todo desde el primer /)
```

```
/home/user/docs/file.txt
               ↑→→→→→→→→  %/*   quita match corto desde la derecha: /file.txt
↑→→→→→→→→→→→→→→→→→→→→→→→  %%/*  quita match largo desde la derecha: todo
```

### Mnemotécnico

```
#  → corta desde la izquierda  (# está a la izquierda en el teclado US)
%  → corta desde la derecha    (% está a la derecha en el teclado US)
Una vez (#/%) = match corto
Dos veces (##/%%) = match largo
```

### Combinaciones prácticas para paths

```bash
filepath="/home/user/docs/report.tar.gz"

echo "${filepath##*/}"     # report.tar.gz     (basename)
echo "${filepath%/*}"      # /home/user/docs   (dirname)
echo "${filepath##*.}"     # gz                (extensión final)
echo "${filepath%.*}"      # /home/user/docs/report.tar  (sin ext final)
echo "${filepath%%.*}"     # /home/user/docs/report      (sin TODAS las ext)

# Extraer solo el nombre sin path ni extensiones:
name="${filepath##*/}"     # report.tar.gz
echo "${name%%.*}"         # report
```

```
Operación encadenada:

"/home/user/docs/report.tar.gz"
         ##*/  → "report.tar.gz"
         %%.*  → "report"
```

---

## 6. Buscar y reemplazar: ${var/pattern/replacement}

```bash
str="hello world, hello bash"

# Primera ocurrencia
echo "${str/hello/hi}"      # hi world, hello bash

# TODAS las ocurrencias (doble /)
echo "${str//hello/hi}"     # hi world, hi bash

# Eliminar (reemplazar con nada)
echo "${str//hello}"        # world,  bash
echo "${str// /-}"          # hello-world,-hello-bash

# Solo al INICIO (# como ancla)
echo "${str/#hello/hi}"     # hi world, hello bash

# Solo al FINAL (% como ancla)
echo "${str/%bash/zsh}"     # hello world, hello zsh
```

### Patrones glob en reemplazo

```bash
files="main.c util.c test.c"

# Reemplazar extensión .c → .o
echo "${files//.c/.o}"     # main.o util.o test.o

# Glob: ? = un carácter, * = cualquier secuencia
str="file001.txt file002.txt file003.txt"
echo "${str//file???/doc}"  # doc.txt doc.txt doc.txt
```

### Casos de uso reales

```bash
# Limpiar string para usar como nombre de archivo
title="My Document (Draft #3)"
safe="${title// /_}"           # My_Document_(Draft_#3)
safe="${safe//[^a-zA-Z0-9_]}"  # No funciona — ver nota

# Para eliminar caracteres especiales, usar tr o sed:
safe=$(echo "$title" | tr -cd 'a-zA-Z0-9_')  # MyDocumentDraft3

# URL encoding básico
url="hello world&foo=bar"
echo "${url// /%20}"    # hello%20world&foo=bar
```

---

## 7. Mayúsculas y minúsculas (Bash 4.0+)

```bash
str="Hello World"

# A mayúsculas
echo "${str^^}"     # HELLO WORLD  (todo)
echo "${str^}"      # Hello World  (solo primer carácter)

# A minúsculas
echo "${str,,}"     # hello world  (todo)
echo "${str,}"      # hello World  (solo primer carácter)

# Solo ciertos caracteres
echo "${str^^[aeiou]}"  # HEllO WOrld  (solo vocales a mayúscula)
```

```
str="Hello World"
${str^^}   → HELLO WORLD    (uppercase all)
${str,,}   → hello world    (lowercase all)
${str^}    → Hello World    (capitalize first)
${str,}    → hello World    (lowercase first)
```

### Uso práctico: comparación case-insensitive

```bash
input="Yes"

# Normalizar a minúsculas para comparar
if [[ "${input,,}" == "yes" ]]; then
    echo "Confirmed"
fi

# Equivalente a: if [[ "$input" =~ ^[Yy][Ee][Ss]$ ]]
# Pero mucho más legible
```

---

## 8. Valores por defecto y validación

```bash
# ${var:-default} — usar default si var vacía o no definida
name="${1:-anonymous}"
echo "Hello, $name"

# ${var:=default} — asignar default si var vacía o no definida
: "${LOG_DIR:=/var/log}"   # : es no-op, solo fuerza la expansión
echo "$LOG_DIR"            # /var/log (o lo que tuviera antes)

# ${var:+alternate} — usar alternate SI var está definida y no vacía
token="abc123"
header="${token:+Authorization: Bearer $token}"
echo "$header"   # Authorization: Bearer abc123
# Si token está vacío:
token=""
header="${token:+Authorization: Bearer $token}"
echo "$header"   # (vacío)

# ${var:?message} — error si var vacía o no definida
: "${DB_HOST:?DB_HOST must be set}"
# Si DB_HOST no está definida → stderr: "DB_HOST must be set", exit 1
```

### Sin los dos puntos: solo "no definida" (vacío está OK)

```bash
unset var
echo "${var-default}"    # default (no definida)

var=""
echo "${var-default}"    # (vacío — está definida, aunque vacía)
echo "${var:-default}"   # default (con :, vacío cuenta como "no definida")
```

```
Resumen:
${var:-val}  → val si var vacía O no definida
${var-val}   → val si var no definida (vacía está OK)
${var:=val}  → asignar val si var vacía O no definida
${var:+val}  → val si var NO vacía Y definida
${var:?msg}  → error si var vacía O no definida
```

---

## 9. Expansión indirecta: ${!var}

```bash
color="red"
red="#FF0000"
green="#00FF00"

# ${!var} expande el NOMBRE que contiene var
echo "${!color}"    # #FF0000  (expande $red, porque color="red")

# Equivalente a:
eval "echo \$$color"  # #FF0000 — pero eval es peligroso, preferir ${!}
```

### Uso práctico: variables dinámicas

```bash
# Leer configuración con prefijo
DB_HOST="localhost"
DB_PORT="5432"
DB_NAME="myapp"

for key in HOST PORT NAME; do
    varname="DB_$key"
    echo "$key = ${!varname}"
done
# HOST = localhost
# PORT = 5432
# NAME = myapp
```

**Nota**: para la mayoría de casos donde querrías variables dinámicas, un
**array asociativo** es mejor solución. `${!var}` es útil cuando las variables
vienen del entorno o de un source.

---

## 10. Expansiones combinadas

Las expansiones se pueden encadenar asignando a variables intermedias:

```bash
# Extraer nombre de archivo, sin extensión, en mayúsculas
filepath="/var/log/app/error.log"

filename="${filepath##*/}"      # error.log
name="${filename%.*}"           # error
upper="${name^^}"               # ERROR

echo "$upper"  # ERROR

# NO se pueden anidar directamente:
# echo "${${filepath##*/}%.*}"   ← SYNTAX ERROR
# Cada expansión necesita su propia variable o línea
```

### Pipeline de transformación

```bash
transform_filename() {
    local path="$1"
    local file="${path##*/}"       # basename
    local name="${file%.*}"        # sin extensión
    local ext="${file##*.}"        # extensión
    local clean="${name// /_}"     # espacios → underscores
    local lower="${clean,,}"       # minúsculas
    echo "${lower}.${ext}"
}

transform_filename "/home/user/My Report (v2).TXT"
# my_report_(v2).txt
```

---

## 11. Comparación: expansión vs herramientas externas

```bash
filepath="/home/user/docs/report.tar.gz"

# basename / dirname (proceso externo)
basename "$filepath"          # report.tar.gz
dirname "$filepath"           # /home/user/docs

# Expansión de parámetros (sin proceso externo)
echo "${filepath##*/}"        # report.tar.gz
echo "${filepath%/*}"         # /home/user/docs
```

| Operación | Expansión Bash | Herramienta externa |
|-----------|---------------|-------------------|
| Basename | `${var##*/}` | `basename "$var"` |
| Dirname | `${var%/*}` | `dirname "$var"` |
| Extensión | `${var##*.}` | — |
| Sin extensión | `${var%.*}` | — |
| Reemplazo | `${var//old/new}` | `echo "$var" \| sed 's/old/new/g'` |
| Uppercase | `${var^^}` | `echo "$var" \| tr a-z A-Z` |
| Lowercase | `${var,,}` | `echo "$var" \| tr A-Z a-z` |
| Substring | `${var:off:len}` | `echo "$var" \| cut -c$((off+1))-$((off+len))` |

**Rendimiento**: en un loop de 10,000 iteraciones, la expansión es ~100x más
rápida que lanzar un proceso externo por cada string. Dentro de un loop,
**siempre** preferir expansiones. Fuera de loops, la diferencia es negligible.

---

## 12. Regex matching con =~

Bash puede hacer match de regex (ERE) en `[[ ... ]]`:

```bash
str="Error: file not found (code 404)"

if [[ "$str" =~ ^Error:\ (.+)\ \(code\ ([0-9]+)\)$ ]]; then
    echo "Message: ${BASH_REMATCH[1]}"  # file not found
    echo "Code: ${BASH_REMATCH[2]}"     # 404
fi
```

`BASH_REMATCH` es un array especial:
- `[0]` = todo el match
- `[1]` = primer grupo de captura `(...)`
- `[2]` = segundo grupo, etc.

```bash
# Validar formato de email (simplificado)
email="user@example.com"
if [[ "$email" =~ ^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$ ]]; then
    echo "Valid email"
fi

# Extraer partes de una fecha
date="2026-03-28"
if [[ "$date" =~ ^([0-9]{4})-([0-9]{2})-([0-9]{2})$ ]]; then
    year="${BASH_REMATCH[1]}"
    month="${BASH_REMATCH[2]}"
    day="${BASH_REMATCH[3]}"
    echo "$day/$month/$year"  # 28/03/2026
fi
```

**Nota**: los patrones regex NO se deben encerrar en comillas. Las comillas
desactivan los caracteres especiales de regex:

```bash
pattern='^[0-9]+$'

# CORRECTO: sin comillas en el patrón
[[ "123" =~ $pattern ]]     # match ✓

# INCORRECTO: con comillas, se trata como string literal
[[ "123" =~ "$pattern" ]]   # no match ✗ (busca literal ^[0-9]+$)
```

---

## 13. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| `${#arr}` en vez de `${#arr[@]}` | Da length del string `arr[0]`, no del array | `${#arr[@]}` para contar elementos |
| `${str:-6}` para últimos 6 chars | Se interpreta como "default si vacío" | `${str: -6}` con espacio |
| Anidar expansiones `${${x}...}` | Syntax error | Variable intermedia |
| `${var/regex/rep}` | El patrón es glob, NO regex | Usar `sed` para regex, o `=~` |
| Comillas en `=~` pattern | Desactiva los metacaracteres regex | Sin comillas: `[[ $s =~ $pat ]]` |
| `echo $var` sin comillas | Word splitting y globbing | `echo "$var"` siempre |
| `${var##*.}` en archivo sin extensión | Retorna el nombre completo | Verificar si contiene `.` primero |
| `${var%.*}` en `.bashrc` | Retorna string vacío (el `.` es el primero) | Tratar dotfiles como caso especial |
| `${str^^}` en Bash < 4.0 | Syntax error | Usar `tr` como fallback |
| Olvidar `local` en funciones | La variable modifica el scope global | `local result="${str##*/}"` |

---

## 14. Ejercicios

### Ejercicio 1 — Longitud y substring

**Predicción**: ¿qué imprime cada línea?

```bash
str="Bash scripting"
echo "${#str}"
echo "${str:5}"
echo "${str:0:4}"
echo "${str: -9}"
```

<details>
<summary>Respuesta</summary>

```
14
scripting
Bash
scripting
```

- `${#str}` → 14 caracteres (incluyendo el espacio)
- `${str:5}` → desde índice 5 hasta el final: `scripting`
- `${str:0:4}` → desde 0, tomar 4: `Bash`
- `${str: -9}` → últimos 9 caracteres: `scripting` (el espacio antes del `-` es obligatorio)
</details>

---

### Ejercicio 2 — Prefijo y sufijo

**Predicción**: ¿qué imprime cada línea?

```bash
path="/var/log/nginx/access.log"
echo "${path#*/}"
echo "${path##*/}"
echo "${path%/*}"
echo "${path%%/*}"
```

<details>
<summary>Respuesta</summary>

```
var/log/nginx/access.log
access.log
/var/log/nginx

```

- `${path#*/}` → match corto desde inicio: quita `/` → `var/log/nginx/access.log`
- `${path##*/}` → match largo desde inicio: quita `/var/log/nginx/` → `access.log`
- `${path%/*}` → match corto desde final: quita `/access.log` → `/var/log/nginx`
- `${path%%/*}` → match largo desde final: quita `/var/log/nginx/access.log` → `""` (vacío)

La última da vacío porque el path empieza con `/`, así que `/*` desde la
derecha abarca todo el string desde el primer `/`.
</details>

---

### Ejercicio 3 — Extensiones dobles

**Predicción**: ¿qué imprime para un archivo `.tar.gz`?

```bash
file="backup.tar.gz"
echo "${file#*.}"
echo "${file##*.}"
echo "${file%.*}"
echo "${file%%.*}"
```

<details>
<summary>Respuesta</summary>

```
tar.gz
gz
backup.tar
backup
```

- `${file#*.}` → quita match corto hasta primer `.`: `tar.gz`
- `${file##*.}` → quita match largo hasta último `.`: `gz`
- `${file%.*}` → quita match corto desde último `.`: `backup.tar`
- `${file%%.*}` → quita match largo desde primer `.`: `backup`

```
"backup.tar.gz"
 ↑     ↑   ↑
 |     |   ##*.  → "gz"      (después del último punto)
 |     #*.      → "tar.gz"   (después del primer punto)
 %%.*           → "backup"   (antes del primer punto)
       %.*     → "backup.tar" (antes del último punto)
```
</details>

---

### Ejercicio 4 — Buscar y reemplazar

**Predicción**: ¿qué imprime cada línea?

```bash
str="foo-bar-baz-foo-bar"
echo "${str/foo/FOO}"
echo "${str//foo/FOO}"
echo "${str/#foo/FOO}"
echo "${str/%bar/BAR}"
echo "${str//bar}"
```

<details>
<summary>Respuesta</summary>

```
FOO-bar-baz-foo-bar
FOO-bar-baz-FOO-bar
FOO-bar-baz-foo-bar
foo-bar-baz-foo-BAR
foo--baz-foo-
```

- `/foo/FOO` → primera ocurrencia: `FOO-bar-baz-foo-bar`
- `//foo/FOO` → todas las ocurrencias: `FOO-bar-baz-FOO-bar`
- `/#foo/FOO` → solo al inicio: `FOO-bar-baz-foo-bar`
- `/%bar/BAR` → solo al final: `foo-bar-baz-foo-BAR`
- `//bar` → eliminar todas las ocurrencias de "bar": `foo--baz-foo-`
</details>

---

### Ejercicio 5 — Mayúsculas y minúsculas

**Predicción**: ¿qué imprime?

```bash
str="hello WORLD"
echo "${str^^}"
echo "${str,,}"
echo "${str^}"
echo "${str,}"
```

<details>
<summary>Respuesta</summary>

```
HELLO WORLD
hello world
Hello WORLD
hello WORLD
```

- `^^` → todo a mayúsculas: `HELLO WORLD`
- `,,` → todo a minúsculas: `hello world`
- `^` → solo primer carácter a mayúscula: `Hello WORLD` (la h → H)
- `,` → solo primer carácter a minúscula: `hello WORLD` (la h ya era minúscula, pero WORLD mantiene sus mayúsculas intactas)

Nota: `${str,}` solo afecta al **primer** carácter. El resto del string
queda exactamente como estaba.
</details>

---

### Ejercicio 6 — Valores por defecto

**Predicción**: ¿qué imprime cada bloque?

```bash
unset var
echo "A: ${var:-fallback}"
echo "B: $var"

echo "C: ${var:=assigned}"
echo "D: $var"

var="exists"
echo "E: ${var:+alternate}"

var=""
echo "F: ${var:+alternate}"
echo "G: ${var:-fallback}"
```

<details>
<summary>Respuesta</summary>

```
A: fallback
B:
C: assigned
D: assigned
E: alternate
F:
G: fallback
```

- **A**: `var` no definida → usa fallback. Pero `:-` NO asigna.
- **B**: `var` sigue sin definir → vacío.
- **C**: `var` no definida → `:=` asigna "assigned" Y lo expande.
- **D**: `var` ahora vale "assigned" (fue asignado por `:=`).
- **E**: `var` está definida y no vacía → `:+` retorna alternate.
- **F**: `var` está vacía → `:+` retorna vacío (la condición "no vacía" falla).
- **G**: `var` está vacía → `:-` retorna fallback.
</details>

---

### Ejercicio 7 — Transformación de path

**Predicción**: ¿qué imprime la función?

```bash
info() {
    local path="$1"
    local dir="${path%/*}"
    local file="${path##*/}"
    local name="${file%.*}"
    local ext="${file##*.}"
    echo "Dir:  $dir"
    echo "File: $file"
    echo "Name: $name"
    echo "Ext:  $ext"
}
info "/etc/nginx/nginx.conf"
```

<details>
<summary>Respuesta</summary>

```
Dir:  /etc/nginx
File: nginx.conf
Name: nginx
Ext:  conf
```

Paso a paso:
```
path = "/etc/nginx/nginx.conf"
dir  = "${path%/*}"    → quita "/*" corto desde final → "/etc/nginx"
file = "${path##*/}"   → quita "*/" largo desde inicio → "nginx.conf"
name = "${file%.*}"    → quita ".*" corto desde final → "nginx"
ext  = "${file##*.}"   → quita "*." largo desde inicio → "conf"
```

Este patrón reemplaza cuatro comandos externos (`dirname`, `basename`,
y dos invocaciones de `sed`/`cut`) con cuatro expansiones inline.
</details>

---

### Ejercicio 8 — Regex con BASH_REMATCH

**Predicción**: ¿qué imprime?

```bash
log='2026-03-28 14:30:22 [ERROR] Connection refused (port 5432)'

if [[ "$log" =~ ^([0-9-]+)\ ([0-9:]+)\ \[([A-Z]+)\]\ (.+)$ ]]; then
    echo "Date: ${BASH_REMATCH[1]}"
    echo "Time: ${BASH_REMATCH[2]}"
    echo "Level: ${BASH_REMATCH[3]}"
    echo "Message: ${BASH_REMATCH[4]}"
else
    echo "No match"
fi
```

<details>
<summary>Respuesta</summary>

```
Date: 2026-03-28
Time: 14:30:22
Level: ERROR
Message: Connection refused (port 5432)
```

El regex tiene 4 grupos de captura:
1. `([0-9-]+)` → `2026-03-28` (fecha)
2. `([0-9:]+)` → `14:30:22` (hora)
3. `([A-Z]+)` → `ERROR` (nivel)
4. `(.+)` → `Connection refused (port 5432)` (resto del mensaje)

`BASH_REMATCH[0]` contendría toda la línea (el match completo).
Cada `\ ` en el patrón es un espacio literal escapado.
</details>

---

### Ejercicio 9 — Expansión indirecta

**Predicción**: ¿qué imprime?

```bash
ENV="production"

production_DB="prod-db.example.com"
staging_DB="staging-db.local"
development_DB="localhost"

varname="${ENV}_DB"
echo "Connecting to: ${!varname}"
```

<details>
<summary>Respuesta</summary>

```
Connecting to: prod-db.example.com
```

Paso a paso:
1. `ENV="production"`
2. `varname="${ENV}_DB"` → `varname="production_DB"`
3. `${!varname}` → expande el contenido de la variable cuyo **nombre** es
   el valor de `varname` → `${production_DB}` → `"prod-db.example.com"`

```
varname = "production_DB"
${!varname} busca la variable llamada "production_DB"
production_DB = "prod-db.example.com"
```

Es una indirección de un nivel. Útil para configuración por entorno, pero
un array asociativo `declare -A DB=(["production"]="..." ["staging"]="...")`
sería más idiomático.
</details>

---

### Ejercicio 10 — Pipeline de transformación

**Predicción**: ¿qué retorna la función?

```bash
slugify() {
    local str="$1"
    str="${str,,}"            # paso 1
    str="${str// /-}"         # paso 2
    str="${str//[^a-z0-9-]}"  # paso 3 — ¿funciona?
    echo "$str"
}
slugify "Hello World! (2026)"
```

<details>
<summary>Respuesta</summary>

```
hello-world!-(2026)
```

Los pasos 1 y 2 funcionan como esperado:
1. `${str,,}` → `"hello world! (2026)"`
2. `${str// /-}` → `"hello-world!-(2026)"`
3. `${str//[^a-z0-9-]}` → **NO elimina los caracteres especiales**

¿Por qué? El patrón `[^a-z0-9-]` se interpreta como **glob**, no como regex.
Los character classes con negación (`[^...]`) no funcionan en expansión de
parámetros de Bash de la misma manera que en regex.

Para slugificar correctamente:
```bash
slugify() {
    local str="$1"
    str="${str,,}"
    str="${str// /-}"
    str=$(echo "$str" | tr -cd 'a-z0-9-')  # herramienta externa para esto
    str="${str//--/-}"  # colapsar dobles guiones
    echo "$str"
}
slugify "Hello World! (2026)"
# hello-world-2026
```

Lección: las expansiones de parámetros usan **glob patterns**, no regex.
Para eliminación de clases de caracteres complejas, `tr` o `sed` son
la herramienta correcta.
</details>