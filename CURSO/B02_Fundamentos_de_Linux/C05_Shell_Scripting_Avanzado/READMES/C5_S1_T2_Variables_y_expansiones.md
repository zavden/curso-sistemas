# T02 — Variables y expansiones

## Errata respecto a los materiales originales

| Ubicacion | Error | Correccion |
|---|---|---|
| README.md:149, max.md:155 | `${STR: -6}` sobre `"Hello, World!"` comenta `# orld!` | Correcto: `# World!` (13 chars, indice -6 = posicion 7 = W) |
| README.md:317, max.md:338 | `DIR=$(dirname $(readlink -f $0))` sin quoting | Correcto: `DIR=$(dirname "$(readlink -f "$0")")` — falla con espacios en rutas |

---

## Variables basicas

```bash
# Asignacion (SIN espacios alrededor del =)
NAME="valor"
COUNT=42
FILE_PATH=/etc/passwd

# INCORRECTO — bash interpreta NAME como comando
NAME = "valor"     # bash: NAME: command not found

# Acceder al valor
echo $NAME
echo ${NAME}       # equivalente, con llaves para delimitar

# Las llaves son necesarias cuando el nombre se junta con texto:
echo "$NAME_suffix"    # bash busca la variable NAME_suffix (no existe)
echo "${NAME}_suffix"  # correcto: valor_suffix
```

### Variables de entorno vs variables de shell

```bash
# Variable de shell (solo visible en este shell)
MY_VAR="local"

# Variable de entorno (visible para procesos hijos)
export MY_VAR="exportada"
# o en dos pasos:
MY_VAR="exportada"
export MY_VAR

# Ver todas las variables de entorno
env
printenv

# Ver una especifica
printenv HOME
echo $HOME

# Crear variable de entorno solo para un comando
LANG=C sort file.txt
# LANG=C es efectivo solo durante la ejecucion de sort
```

---

## Expansiones de parametros

Las expansiones `${...}` permiten manipular el valor de variables directamente
en bash, sin necesidad de herramientas externas como `sed` o `awk`.

### Valores por defecto

```bash
# ${var:-default} — usar default si var esta vacia o no definida
echo ${NAME:-"anonimo"}
# Si NAME tiene valor: muestra el valor
# Si NAME esta vacia o no existe: muestra "anonimo"
# NO modifica NAME

# ${var:=default} — asignar default si var esta vacia o no definida
echo ${NAME:="anonimo"}
# Igual que :- pero SI asigna "anonimo" a NAME
# NOTA: no funciona con $1, $2 (posicionales)

# ${var:+alternate} — usar alternate si var TIENE valor
echo ${NAME:+"tiene nombre"}
# Si NAME tiene valor: muestra "tiene nombre"
# Si NAME esta vacia: muestra nada (cadena vacia)

# ${var:?message} — error si var esta vacia o no definida
echo ${NAME:?"ERROR: NAME no esta definida"}
# Si NAME tiene valor: muestra el valor
# Si NAME esta vacia: imprime el mensaje y sale con error
```

### La diferencia del : (colon)

```bash
# Sin colon: solo verifica si la variable esta definida
# Con colon: verifica si esta definida Y no vacia

VAR=""

echo ${VAR-default}    # (vacio — VAR esta definida, aunque vacia)
echo ${VAR:-default}   # default  (VAR esta vacia)

unset VAR
echo ${VAR-default}    # default  (VAR no esta definida)
echo ${VAR:-default}   # default  (VAR no esta definida)
```

### Tabla completa de expansiones

| Expansion | Variable no definida | Variable vacia ("") | Variable con valor |
|---|---|---|---|
| `${var-default}` | default | (vacio) | valor |
| `${var:-default}` | default | **default** | valor |
| `${var=default}` | default (y asigna) | (vacio) | valor |
| `${var:=default}` | default (y asigna) | **default (y asigna)** | valor |
| `${var+alt}` | (vacio) | alt | alt |
| `${var:+alt}` | (vacio) | (vacio) | **alt** |
| `${var?error}` | error y sale | (vacio) | valor |
| `${var:?error}` | error y sale | **error y sale** | valor |

La columna en **negrita** es donde el `:` hace la diferencia. Sin `:`, una
variable vacia (`""`) se trata como "definida". Con `:`, se trata como ausente.

### Patrones comunes con defaults

```bash
# Valor por defecto para parametros de script
PORT=${1:-8080}
HOST=${2:-"localhost"}

# Variables de configuracion con fallback
LOG_DIR=${LOG_DIR:-"/var/log/myapp"}
DEBUG=${DEBUG:-0}

# Validar que una variable requerida existe
# : es el comando nulo — solo evalua la expansion sin hacer nada mas
: ${DB_HOST:?"Falta la variable DB_HOST"}
: ${API_KEY:?"Falta la variable API_KEY"}
```

---

## Manipulacion de strings

### Longitud

```bash
NAME="hello world"
echo ${#NAME}
# 11

# Util en validaciones
if [[ ${#PASSWORD} -lt 8 ]]; then
    echo "Password demasiado corta"
fi
```

### Substring

```bash
STR="Hello, World!"

# ${var:offset} — desde offset hasta el final
echo ${STR:7}
# World!

# ${var:offset:length} — desde offset, N caracteres
echo ${STR:0:5}
# Hello

# Offset negativo (desde el final)
# Necesita espacio o parentesis antes del - para no confundir con :-
echo ${STR: -6}
# World!  (6 caracteres desde el final: indices 7..12)

echo ${STR:(-6)}
# World!  (equivalente, parentesis evitan ambiguedad)
```

### Eliminar patrones (prefix y suffix)

```bash
FILE="/home/dev/project/main.tar.gz"

# ${var#pattern} — eliminar el prefijo mas CORTO que coincida
echo ${FILE#*/}
# home/dev/project/main.tar.gz  (elimino el primer /)

# ${var##pattern} — eliminar el prefijo mas LARGO que coincida
echo ${FILE##*/}
# main.tar.gz  (elimino todo hasta el ultimo /)
# Equivale a: basename "$FILE"

# ${var%pattern} — eliminar el sufijo mas CORTO que coincida
echo ${FILE%.*}
# /home/dev/project/main.tar  (elimino .gz)

# ${var%%pattern} — eliminar el sufijo mas LARGO que coincida
echo ${FILE%%.*}
# /home/dev/project/main  (elimino .tar.gz)
```

### Regla mnemotecnica

```
# esta a la IZQUIERDA del $ en el teclado → elimina PREFIJO (izquierda)
% esta a la DERECHA del $ en el teclado → elimina SUFIJO (derecha)

#  = corto (un solo #)
## = largo (doble ##)
%  = corto
%% = largo
```

### Patrones comunes con archivos

```bash
FILE="/path/to/document.tar.gz"

# Obtener el nombre del archivo (sin ruta)
echo ${FILE##*/}          # document.tar.gz (como basename)

# Obtener el directorio (sin nombre)
echo ${FILE%/*}           # /path/to (como dirname)

# Obtener la extension
echo ${FILE##*.}          # gz (ultima extension)

# Obtener el nombre sin extension
BASENAME=${FILE##*/}
echo ${BASENAME%.*}       # document.tar (quita .gz)
echo ${BASENAME%%.*}      # document (quita .tar.gz)

# Cambiar extension
echo ${FILE%.gz}.bz2      # /path/to/document.tar.bz2
```

### Busqueda y reemplazo

```bash
STR="Hello World Hello"

# ${var/pattern/replacement} — reemplazar la PRIMERA coincidencia
echo ${STR/Hello/Goodbye}
# Goodbye World Hello

# ${var//pattern/replacement} — reemplazar TODAS las coincidencias
echo ${STR//Hello/Goodbye}
# Goodbye World Goodbye

# ${var/#pattern/replacement} — reemplazar solo al INICIO
echo ${STR/#Hello/Goodbye}
# Goodbye World Hello

# ${var/%pattern/replacement} — reemplazar solo al FINAL
echo ${STR/%Hello/Goodbye}
# Hello World Goodbye

# Eliminar (reemplazar con nada)
echo ${STR// /}
# HelloWorldHello  (elimino todos los espacios)
```

### Cambiar mayusculas/minusculas (bash 4+)

```bash
STR="Hello World"

echo ${STR,,}     # hello world (todo minusculas)
echo ${STR^^}     # HELLO WORLD (todo mayusculas)
echo ${STR,}      # hello World (primera letra a minuscula)
echo ${STR^}      # Hello World (primera letra a mayuscula)

# No modifica la variable original
echo ${STR,,}     # hello world
echo $STR         # Hello World (sin cambio)

# Para modificar, reasignar:
STR=${STR,,}      # ahora STR = "hello world"

# Uso practico: normalizar input del usuario
INPUT="YES"
if [[ "${INPUT,,}" == "yes" ]]; then
    echo "Confirmado"
fi
```

---

## Variables especiales del shell

| Variable | Descripcion |
|---|---|
| `$0` | Nombre del script |
| `$1` ... `$9` | Parametros posicionales |
| `${10}` | Parametro 10+ (necesita llaves) |
| `$#` | Numero de parametros |
| `$@` | Todos los parametros (como palabras separadas) |
| `$*` | Todos los parametros (como una sola cadena, unida por IFS) |
| `$$` | PID del shell actual |
| `$!` | PID del ultimo proceso en background |
| `$?` | Exit code del ultimo comando |
| `$_` | Ultimo argumento del comando anterior |
| `$-` | Flags activos del shell (himBHs) |
| `$RANDOM` | Numero aleatorio 0-32767 |
| `$SECONDS` | Segundos desde inicio del shell |
| `$LINENO` | Numero de linea actual en el script |

### $@ vs $*

```bash
# Con archivos que tienen espacios:
set -- "file one.txt" "file two.txt" "file three.txt"

# "$@" preserva las separaciones (3 argumentos)
for f in "$@"; do echo "archivo: $f"; done
# archivo: file one.txt
# archivo: file two.txt
# archivo: file three.txt

# "$*" junta todo en una cadena (1 argumento, unido por primer char de IFS)
for f in "$*"; do echo "archivo: $f"; done
# archivo: file one.txt file two.txt file three.txt

# $@ SIN comillas — word splitting rompe argumentos con espacios
for f in $@; do echo "archivo: $f"; done
# archivo: file
# archivo: one.txt
# archivo: file
# archivo: two.txt
# archivo: file
# archivo: three.txt

# REGLA: SIEMPRE usar "$@" (con comillas) para iterar parametros
```

---

## Expansion aritmetica

```bash
# $(( expresion ))
echo $((2 + 3))       # 5
echo $((10 / 3))      # 3 (entero, sin decimales — trunca)
echo $((10 % 3))      # 1 (modulo)
echo $((2 ** 10))     # 1024 (potencia)

# Con variables (no necesitan $ dentro de (( )))
X=5
echo $((X * 2))       # 10
echo $((X++))         # 5 (post-incremento, X ahora es 6)
echo $X               # 6

# Asignacion directa
((COUNT++))
((TOTAL = X + Y))

# Operadores disponibles: + - * / % ** & | ^ ~ << >>
# (incluye bitwise: AND, OR, XOR, NOT, shifts)
```

---

## Expansion de comandos

```bash
# $() — capturar la salida de un comando
FILES=$(ls /etc/*.conf)
DATE=$(date +%Y-%m-%d)
CPUS=$(nproc)

# Backticks (legacy, evitar)
FILES=`ls /etc/*.conf`
# Los backticks no se anidan bien y son menos legibles
# SIEMPRE preferir $()

# Se pueden anidar (con quoting correcto)
DIR=$(dirname "$(readlink -f "$0")")
```

---

## Labs

### Parte 1 — Defaults y asignacion

#### Paso 1.1: ${var:-default} — valor por defecto

```bash
docker compose exec debian-dev bash -c '
echo "=== \${var:-default} ==="
echo ""
echo "Usa un valor alternativo si la variable no existe o esta vacia"
echo "La variable NO se modifica"
echo ""
unset NAME
echo "NAME no definida: ${NAME:-anonimo}"
echo "NAME sigue sin definir: ${NAME:-todavia_anonimo}"
echo ""
NAME=""
echo "NAME vacia: ${NAME:-anonimo}"
echo ""
NAME="Carlos"
echo "NAME definida: ${NAME:-anonimo}"
echo ""
echo "Uso tipico:"
echo "  LOG_LEVEL=\${LOG_LEVEL:-info}"
echo "  OUTPUT=\${1:-/dev/stdout}"
'
```

#### Paso 1.2: ${var:=default} — asignar si no existe

```bash
docker compose exec debian-dev bash -c '
echo "=== \${var:=default} ==="
echo ""
echo "Como :- pero ADEMAS asigna el valor a la variable"
echo ""
unset COLOR
echo "COLOR: ${COLOR:=azul}"
echo "COLOR ahora vale: $COLOR"
echo ""
echo "Si ya tiene valor, no cambia:"
echo "COLOR: ${COLOR:=rojo}"
echo "COLOR sigue siendo: $COLOR"
echo ""
echo "NOTA: no funciona con \$1, \$2 (posicionales)"
echo "  \${1:=default}  → error"
'
```

#### Paso 1.3: ${var:+alternate} — valor si existe

```bash
docker compose exec debian-dev bash -c '
echo "=== \${var:+alternate} ==="
echo ""
echo "Devuelve el valor alternativo SOLO si la variable existe y no esta vacia"
echo "Es el inverso de :-"
echo ""
unset DEBUG
echo "DEBUG no definida: [${DEBUG:+--verbose}]"
echo ""
DEBUG=1
echo "DEBUG=1: [${DEBUG:+--verbose}]"
echo ""
echo "Uso tipico: agregar flags condicionalmente"
echo "  cmd \${VERBOSE:+--verbose} \${DRY_RUN:+--dry-run} archivo.txt"
'
```

#### Paso 1.4: ${var:?error} — error si no existe

```bash
docker compose exec debian-dev bash -c '
echo "=== \${var:?error} ==="
echo ""
echo "Termina el script con un mensaje de error si la variable"
echo "no esta definida o esta vacia"
echo ""
echo "--- Con variable definida ---"
DB_HOST="localhost"
echo "DB_HOST: ${DB_HOST:?Variable DB_HOST requerida}"
echo ""
echo "--- Con variable no definida (dentro de subshell para no salir) ---"
(
    unset DB_HOST
    echo "Intentando usar DB_HOST..."
    echo "${DB_HOST:?ERROR: Variable DB_HOST no definida}" 2>&1
) || true
echo ""
echo "Uso tipico al inicio de scripts:"
echo "  : \${API_KEY:?Falta API_KEY}"
echo "  : \${DB_HOST:?Falta DB_HOST}"
echo "  (: es el comando nulo, solo evalua la expansion)"
'
```

#### Paso 1.5: Con y sin : (dos puntos)

```bash
docker compose exec debian-dev bash -c '
echo "=== Con : vs sin : ==="
echo ""
echo "Sin : solo verifica si esta definida (incluso vacia)"
echo "Con : verifica definida Y no vacia"
echo ""
EMPTY=""
unset NOEXIST
echo "--- \${var-default} (sin :) ---"
echo "EMPTY (vacia): [${EMPTY-default}]     → vacia (esta definida)"
echo "NOEXIST: [${NOEXIST-default}]           → default (no definida)"
echo ""
echo "--- \${var:-default} (con :) ---"
echo "EMPTY (vacia): [${EMPTY:-default}]    → default (vacia cuenta)"
echo "NOEXIST: [${NOEXIST:-default}]          → default"
echo ""
echo "Casi siempre se usa la version con : (mas segura)"
'
```

### Parte 2 — Manipulacion de strings

#### Paso 2.1: Longitud y subcadenas

```bash
docker compose exec debian-dev bash -c '
echo "=== Longitud y subcadenas ==="
echo ""
FILE="/home/user/documents/report.tar.gz"
echo "FILE=$FILE"
echo ""
echo "--- Longitud ---"
echo "\${#FILE} = ${#FILE}"
echo ""
echo "--- Subcadenas \${var:offset:length} ---"
echo "\${FILE:0:5}  = ${FILE:0:5}"
echo "\${FILE:6:4}  = ${FILE:6:4}"
echo "\${FILE:(-6)} = ${FILE:(-6)}"
echo ""
echo "Los parentesis en (-6) evitan confusion con :- (default)"
echo "Tambien sirve un espacio: \${FILE: -6}"
'
```

#### Paso 2.2: Eliminar prefijos y sufijos

Antes de ejecutar, predice: dado `FILE="/home/user/docs/report.tar.gz"`,
que devuelve `${FILE##*/}` y `${FILE%%/*}`?

```bash
docker compose exec debian-dev bash -c '
echo "=== Eliminar prefijos y sufijos ==="
echo ""
FILE="/home/user/docs/report.tar.gz"
echo "FILE=$FILE"
echo ""
echo "--- Eliminar prefijo (desde el inicio) ---"
echo "\${FILE#*/}   = ${FILE#*/}"
echo "  (# = el match mas corto desde el inicio)"
echo "\${FILE##*/}  = ${FILE##*/}"
echo "  (## = el match mas largo desde el inicio → basename)"
echo ""
echo "--- Eliminar sufijo (desde el final) ---"
echo "\${FILE%.*}   = ${FILE%.*}"
echo "  (% = el match mas corto desde el final → quita .gz)"
echo "\${FILE%%.*}  = ${FILE%%.*}"
echo "  (%% = el match mas largo desde el final → quita todo desde el primer .)"
echo ""
echo "=== Patrones utiles ==="
echo "Basename: \${FILE##*/}  = ${FILE##*/}"
echo "Dirname:  \${FILE%/*}   = ${FILE%/*}"
echo "Extension: \${FILE##*.} = ${FILE##*.}"
echo "Sin ext:   \${FILE%.*}  = ${FILE%.*}"
'
```

#### Paso 2.3: Reemplazo de patrones

```bash
docker compose exec debian-dev bash -c '
echo "=== Reemplazo \${var/patron/reemplazo} ==="
echo ""
TEXT="hello world hello bash"
echo "TEXT=$TEXT"
echo ""
echo "--- Primera ocurrencia ---"
echo "\${TEXT/hello/HOLA} = ${TEXT/hello/HOLA}"
echo ""
echo "--- Todas las ocurrencias ---"
echo "\${TEXT//hello/HOLA} = ${TEXT//hello/HOLA}"
echo ""
echo "--- Reemplazar al inicio ---"
echo "\${TEXT/#hello/HOLA} = ${TEXT/#hello/HOLA}"
echo ""
echo "--- Reemplazar al final ---"
echo "\${TEXT/%bash/SHELL} = ${TEXT/%bash/SHELL}"
echo ""
echo "--- Eliminar (reemplazar con nada) ---"
echo "\${TEXT// /} = ${TEXT// /}"
'
```

#### Paso 2.4: Cambio de caso

```bash
docker compose exec debian-dev bash -c '
echo "=== Cambio de caso (bash 4+) ==="
echo ""
NAME="Hello World"
echo "NAME=$NAME"
echo ""
echo "--- A minusculas ---"
echo "\${NAME,}  = ${NAME,}"
echo "  (, = solo el primer caracter)"
echo "\${NAME,,} = ${NAME,,}"
echo "  (,, = todos los caracteres)"
echo ""
echo "--- A mayusculas ---"
echo "\${NAME^}  = ${NAME^}"
echo "  (^ = solo el primer caracter)"
echo "\${NAME^^} = ${NAME^^}"
echo "  (^^ = todos los caracteres)"
echo ""
echo "=== Uso practico ==="
INPUT="yes"
echo "Normalizar input del usuario:"
echo "  INPUT=yes → \${INPUT,,} = ${INPUT,,}"
INPUT="YES"
echo "  INPUT=YES → \${INPUT,,} = ${INPUT,,}"
INPUT="Yes"
echo "  INPUT=Yes → \${INPUT,,} = ${INPUT,,}"
echo "  Comparar siempre en minusculas: [[ \"\${INPUT,,}\" == \"yes\" ]]"
'
```

### Parte 3 — Aritmetica y sustitucion

#### Paso 3.1: Aritmetica con $(( ))

```bash
docker compose exec debian-dev bash -c '
echo "=== Aritmetica \$(( )) ==="
echo ""
A=10
B=3
echo "A=$A B=$B"
echo ""
echo "Suma:      \$((A + B))  = $((A + B))"
echo "Resta:     \$((A - B))  = $((A - B))"
echo "Multiplic: \$((A * B))  = $((A * B))"
echo "Division:  \$((A / B))  = $((A / B)) (entera, trunca)"
echo "Modulo:    \$((A % B))  = $((A % B))"
echo "Potencia:  \$((A ** 2)) = $((A ** 2))"
echo ""
echo "--- Incremento/decremento ---"
X=5
echo "X=$X"
echo "\$((X++)) = $((X++)) (post: devuelve 5, luego incrementa)"
echo "X ahora = $X"
echo "\$((++X)) = $((++X)) (pre: incrementa primero, devuelve 7)"
echo ""
echo "--- No necesita \$ dentro de (( )) ---"
echo "\$((A + B)) es igual que \$((\$A + \$B))"
echo "Dentro de (( )), bash resuelve los nombres automaticamente"
'
```

#### Paso 3.2: Sustitucion de comandos

```bash
docker compose exec debian-dev bash -c '
echo "=== Sustitucion de comandos \$() ==="
echo ""
echo "--- Capturar salida de un comando ---"
TODAY=$(date +%Y-%m-%d)
echo "Hoy es: $TODAY"
echo ""
USERS=$(wc -l < /etc/passwd)
echo "Usuarios en /etc/passwd: $USERS"
echo ""
echo "--- Anidar (con quoting correcto) ---"
echo "Directorio de bash: $(dirname "$(which bash)")"
echo ""
echo "--- En asignaciones ---"
HOSTNAME_UPPER=$(hostname | tr "[:lower:]" "[:upper:]")
echo "Hostname en mayusculas: $HOSTNAME_UPPER"
echo ""
echo "--- Backticks (obsoleto, evitar) ---"
echo "Antes: DATE=\`date\`"
echo "Ahora: DATE=\$(date)"
echo "Backticks no se anidan facilmente y son menos legibles"
'
```

#### Paso 3.3: $@ vs $*

```bash
docker compose exec debian-dev bash -c '
cat > /tmp/args-test.sh << '\''SCRIPT'\''
#!/bin/bash
echo "=== Con \"$@\" (cada argumento separado) ==="
i=1
for arg in "$@"; do
    echo "  [$i]: $arg"
    ((i++))
done

echo ""
echo "=== Con \"$*\" (todo como un solo string) ==="
i=1
for arg in "$*"; do
    echo "  [$i]: $arg"
    ((i++))
done

echo ""
echo "=== Sin comillas $@ (word splitting!) ==="
i=1
for arg in $@; do
    echo "  [$i]: $arg"
    ((i++))
done
SCRIPT
chmod +x /tmp/args-test.sh
echo "--- Ejecutar con argumentos que tienen espacios ---"
echo "  /tmp/args-test.sh \"hello world\" \"foo bar\" baz"
echo ""
/tmp/args-test.sh "hello world" "foo bar" "baz"
echo ""
echo "REGLA: siempre usar \"\$@\" (con comillas)"
'
```

#### Paso 3.4: Variables especiales

```bash
docker compose exec debian-dev bash -c '
echo "=== Variables especiales ==="
echo ""
echo "\$\$    PID del shell actual:     $$"
echo "\$!    PID del ultimo background: ${!:-ninguno}"
echo "\$0    Nombre del script:        $0"
echo "\$#    Numero de argumentos:     $#"
echo "\$?    Exit code anterior:       $?"
echo ""
echo "--- \$RANDOM ---"
echo "Numero aleatorio: $RANDOM (0-32767)"
echo "Dado (1-6): $(( RANDOM % 6 + 1 ))"
echo ""
echo "--- \$SECONDS ---"
echo "Segundos desde inicio del shell: $SECONDS"
echo ""
echo "--- \$LINENO ---"
echo "Linea actual del script: $LINENO"
'
```

### Limpieza

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/args-test.sh
echo "Archivos temporales eliminados"
'
```

---

## Ejercicios

### Ejercicio 1 — Defaults en cascada

Crea un script que reciba 3 parametros: HOST, PORT, MODE. Usa `${:-}` para
defaults de HOST y PORT, y `${:?}` para hacer MODE obligatorio.

```bash
#!/bin/bash
PORT=${1:-8080}
HOST=${2:-"0.0.0.0"}
MODE=${3:?"Falta el modo (dev|prod)"}

echo "Servidor en $HOST:$PORT modo $MODE"
```

Prueba con estos tres casos y predice el resultado antes de ejecutar:

```bash
bash /tmp/test-defaults.sh
bash /tmp/test-defaults.sh 3000 localhost dev
bash /tmp/test-defaults.sh 3000
```

<details><summary>Prediccion</summary>

1. `bash /tmp/test-defaults.sh` → Error: `Falta el modo (dev|prod)` (PORT=8080, HOST=0.0.0.0, pero MODE no definido → `:?` aborta)
2. `bash /tmp/test-defaults.sh 3000 localhost dev` → `Servidor en localhost:3000 modo dev`
3. `bash /tmp/test-defaults.sh 3000` → Error: `Falta el modo (dev|prod)` (PORT=3000, HOST=0.0.0.0, MODE falta)

</details>

### Ejercicio 2 — La tabla del colon

Sin ejecutar, predice que imprime cada linea:

```bash
VAR=""
echo "A: [${VAR-default}]"
echo "B: [${VAR:-default}]"
echo "C: [${VAR+alt}]"
echo "D: [${VAR:+alt}]"

unset VAR
echo "E: [${VAR-default}]"
echo "F: [${VAR:-default}]"
echo "G: [${VAR+alt}]"
echo "H: [${VAR:+alt}]"
```

<details><summary>Prediccion</summary>

Con `VAR=""` (definida pero vacia):
- A: `[]` — sin `:`, VAR esta definida → no usa default
- B: `[default]` — con `:`, VAR esta vacia → usa default
- C: `[alt]` — sin `:`, VAR esta definida → usa alt
- D: `[]` — con `:`, VAR esta vacia → no usa alt

Con `unset VAR` (no definida):
- E: `[default]` — no definida → usa default
- F: `[default]` — no definida → usa default
- G: `[]` — no definida → no usa alt
- H: `[]` — no definida → no usa alt

Regla: sin `:` solo importa si esta *definida*. Con `:` importa que este
*definida y no vacia*.

</details>

### Ejercicio 3 — Diseccion de rutas

Dado `FILE="/var/log/nginx/access.log.2024-03-17.gz"`, predice el resultado
de cada expansion y luego verifica:

```bash
FILE="/var/log/nginx/access.log.2024-03-17.gz"

echo "1: ${FILE%/*}"
echo "2: ${FILE##*/}"
echo "3: ${FILE%.gz}"
echo "4: ${FILE##*.}"
echo "5: ${FILE%%.*}"
echo "6: ${FILE#*/}"
```

<details><summary>Prediccion</summary>

1. `/var/log/nginx` — `%/*` elimina el sufijo mas corto que coincida con `/*` (el ultimo `/` + nombre)
2. `access.log.2024-03-17.gz` — `##*/` elimina el prefijo mas largo hasta el ultimo `/`
3. `/var/log/nginx/access.log.2024-03-17` — `%.gz` elimina el sufijo `.gz`
4. `gz` — `##*.` elimina el prefijo mas largo hasta el ultimo `.`
5. `/var/log/nginx/access` — `%%.*` elimina el sufijo mas largo desde el primer `.`
6. `var/log/nginx/access.log.2024-03-17.gz` — `#*/` elimina el prefijo mas corto (solo el primer `/`)

</details>

### Ejercicio 4 — Busqueda y reemplazo

```bash
URL="https://api.example.com/v1/users?page=1&limit=10"
```

Sin usar `sed`, `awk` ni `cut`, extrae:
- El dominio (`api.example.com`)
- La URL sin query params
- La URL cambiando `https` por `http`
- La URL cambiando `v1` por `v2`

<details><summary>Prediccion</summary>

```bash
# Dominio — dos pasos:
TEMP=${URL#*//}           # api.example.com/v1/users?page=1&limit=10
DOMAIN=${TEMP%%/*}        # api.example.com

# Sin query params:
echo ${URL%%\?*}          # https://api.example.com/v1/users

# https → http:
echo ${URL/https/http}    # http://api.example.com/v1/users?page=1&limit=10

# v1 → v2:
echo ${URL/v1/v2}         # https://api.example.com/v2/users?page=1&limit=10
```

Nota: `?` necesita escaparse con `\` en el patron `%%` porque `?` es un
glob que coincide con un caracter cualquiera.

</details>

### Ejercicio 5 — Mayusculas, minusculas y normalizacion

Predice la salida y luego verifica:

```bash
INPUT="Hello World"

echo "A: ${INPUT,,}"
echo "B: ${INPUT^^}"
echo "C: ${INPUT,}"
echo "D: ${INPUT^}"

# Normalizar comparacion
ANSWER="YES"
if [[ "${ANSWER,,}" == "yes" ]]; then
    echo "Confirmado"
else
    echo "No confirmado"
fi
```

<details><summary>Prediccion</summary>

- A: `hello world` — `,,` convierte todos a minuscula
- B: `HELLO WORLD` — `^^` convierte todos a mayuscula
- C: `hello World` — `,` solo la primera letra a minuscula
- D: `Hello World` — `^` solo la primera letra a mayuscula (ya lo esta)

El `if` imprime `Confirmado` porque `${ANSWER,,}` convierte "YES" a "yes".

</details>

### Ejercicio 6 — $@ vs $* en la practica

Crea este script y predice la salida antes de ejecutar:

```bash
#!/bin/bash
# Guardar como /tmp/args-demo.sh

echo "=== \"\$@\" ==="
for arg in "$@"; do echo "  [$arg]"; done

echo "=== \"\$*\" ==="
for arg in "$*"; do echo "  [$arg]"; done

echo "=== \$@ sin comillas ==="
for arg in $@; do echo "  [$arg]"; done
```

Ejecutar con: `bash /tmp/args-demo.sh "uno dos" tres "cuatro cinco"`

<details><summary>Prediccion</summary>

```
=== "$@" ===
  [uno dos]
  [tres]
  [cuatro cinco]
=== "$*" ===
  [uno dos tres cuatro cinco]
=== $@ sin comillas ===
  [uno]
  [dos]
  [tres]
  [cuatro]
  [cinco]
```

- `"$@"`: preserva 3 argumentos tal como fueron pasados
- `"$*"`: une todo en un solo string (separado por primer char de IFS, default espacio)
- `$@` sin comillas: word splitting rompe "uno dos" en dos palabras

</details>

### Ejercicio 7 — Aritmetica y variables

Predice cada valor y ejecuta para verificar:

```bash
X=10
echo "A: $((X + 5))"
echo "B: $((X / 3))"
echo "C: $((X % 3))"
echo "D: $((X ** 2))"
echo "E: $((X++))"
echo "F: $X"
echo "G: $((++X))"
echo "H: $X"
```

<details><summary>Prediccion</summary>

- A: `15` — 10 + 5
- B: `3` — 10 / 3 = 3.33, trunca a 3 (aritmetica entera)
- C: `1` — 10 % 3 = 1 (resto)
- D: `100` — 10^2
- E: `10` — post-incremento: devuelve 10, LUEGO X pasa a 11
- F: `11` — X ya fue incrementado
- G: `12` — pre-incremento: PRIMERO incrementa a 12, luego devuelve 12
- H: `12` — X quedo en 12

</details>

### Ejercicio 8 — Sustitucion de comandos y arrays

Ejecuta y analiza:

```bash
# Arrays con expansiones (bash, no POSIX)
ARR=(uno dos tres cuatro cinco)

echo "Elementos: ${#ARR[@]}"
echo "Todos: ${ARR[@]}"
echo "Primeros 3: ${ARR[@]:0:3}"
echo "Indices: ${!ARR[@]}"
echo "Longitud de ARR[2]: ${#ARR[2]}"
```

Predice los valores antes de ejecutar.

<details><summary>Prediccion</summary>

- `${#ARR[@]}` → `5` — numero de elementos
- `${ARR[@]}` → `uno dos tres cuatro cinco` — todos los elementos
- `${ARR[@]:0:3}` → `uno dos tres` — slice de 3 elementos desde indice 0
- `${!ARR[@]}` → `0 1 2 3 4` — los indices del array
- `${#ARR[2]}` → `4` — longitud del string "tres" (4 caracteres)

Nota: `${#ARR[@]}` cuenta elementos del array, `${#ARR[2]}` cuenta
caracteres del elemento. El `#` opera sobre lo que este a la derecha.

</details>

### Ejercicio 9 — Config con variables de entorno

Crea un script que simule conexion a una base de datos usando defaults
y validacion:

```bash
#!/bin/bash
# Guardar como /tmp/db-config.sh

DB_HOST=${DB_HOST:-localhost}
DB_PORT=${DB_PORT:-5432}
DB_NAME=${DB_NAME:-myapp}
DB_USER=${DB_USER:-postgres}

echo "Conectando a ${DB_HOST}:${DB_PORT}/${DB_NAME} como ${DB_USER}"

# Si DB_PASS no esta definida, abortar
: ${DB_PASS:?"ERROR: DB_PASS debe estar definida"}

echo "Conexion configurada correctamente"
```

Predice que pasa con cada ejecucion:

```bash
bash /tmp/db-config.sh
DB_PASS=secret bash /tmp/db-config.sh
DB_HOST=prod.db.com DB_PASS=secret bash /tmp/db-config.sh
```

<details><summary>Prediccion</summary>

1. `bash /tmp/db-config.sh` → Imprime "Conectando a localhost:5432/myapp como postgres", luego error: `ERROR: DB_PASS debe estar definida` y aborta (exit 1). No llega al "Conexion configurada".

2. `DB_PASS=secret bash /tmp/db-config.sh` → "Conectando a localhost:5432/myapp como postgres" + "Conexion configurada correctamente". DB_PASS tiene valor, pasa la validacion.

3. `DB_HOST=prod.db.com DB_PASS=secret bash /tmp/db-config.sh` → "Conectando a prod.db.com:5432/myapp como postgres" + "Conexion configurada correctamente". DB_HOST sobreescribe el default.

Patron clave: `VAR=valor comando` crea una variable de entorno solo para
ese comando. Combinado con `${:-}`, permite configurar scripts desde fuera
sin modificar el codigo.

</details>

### Ejercicio 10 — Panorama de expansiones

Dado este pipeline, predice el resultado de CADA linea:

```bash
FILENAME="/tmp/data.old.backup.tar.gz"

BASENAME=${FILENAME##*/}
DIR=${FILENAME%/*}
EXT=${BASENAME##*.}
NO_EXT=${BASENAME%.*}
DOUBLE_NO_EXT=${BASENAME%%.*}
UPPER=${BASENAME^^}
REPLACED=${BASENAME//./—}

echo "Original:  $FILENAME"
echo "Basename:  $BASENAME"
echo "Directory: $DIR"
echo "Extension: $EXT"
echo "Sin .gz:   $NO_EXT"
echo "Sin todo:  $DOUBLE_NO_EXT"
echo "Mayusc:    $UPPER"
echo "Dots→dash: $REPLACED"
echo "Longitud:  ${#BASENAME}"
```

<details><summary>Prediccion</summary>

- `BASENAME` = `data.old.backup.tar.gz` — `##*/` elimina hasta el ultimo `/`
- `DIR` = `/tmp` — `%/*` elimina desde el ultimo `/`
- `EXT` = `gz` — `##*.` elimina hasta el ultimo `.`
- `NO_EXT` = `data.old.backup.tar` — `%.*` elimina la ultima extension
- `DOUBLE_NO_EXT` = `data` — `%%.*` elimina desde el primer `.`
- `UPPER` = `DATA.OLD.BACKUP.TAR.GZ` — `^^` todo a mayuscula
- `REPLACED` = `data—old—backup—tar—gz` — `//./—` reemplaza cada `.` por `—`
- `${#BASENAME}` = `24` — longitud de `data.old.backup.tar.gz`

Longitud: d(1)a(2)t(3)a(4).(5)o(6)l(7)d(8).(9)b(10)a(11)c(12)k(13)u(14)p(15).(16)t(17)a(18)r(19).(20)g(21)z(22) = 22 caracteres.

Correccion: son 22, no 24.

</details>
