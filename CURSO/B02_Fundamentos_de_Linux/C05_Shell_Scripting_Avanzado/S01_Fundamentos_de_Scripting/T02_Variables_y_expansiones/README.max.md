# T02 — Variables y expansiones

## Variables básicas

```bash
# Asignación (SIN espacios alrededor del =)
NAME="valor"
COUNT=42
FILE_PATH=/etc/passwd

# INCORRECTO — bash interpreta el = como argumentos del comando NAME
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
# o
MY_VAR="exportada"
export MY_VAR

# Ver todas las variables de entorno
env
# o
printenv

# Ver una específica
printenv HOME
echo $HOME

# Crear variable de entorno solo para un comando
LANG=C sort file.txt
# LANG=C es efectivo solo durante la ejecución de sort
```

---

## Expansiones de parámetros

Las expansiones `${...}` permiten manipular el valor de variables directamente
en bash, sin necesidad de herramientas externas como `sed` o `awk`.

### Valores por defecto

```bash
# ${var:-default} — usar default si var está vacía o no definida
echo ${NAME:-"anónimo"}
# Si NAME tiene valor: muestra el valor
# Si NAME está vacía o no existe: muestra "anónimo"
# NO modifica NAME

# ${var:=default} — asignar default si var está vacía o no definida
echo ${NAME:="anónimo"}
# Igual que :- pero SÍ asigna "anónimo" a NAME

# ${var:+alternate} — usar alternate si var TIENE valor
echo ${NAME:+"tiene nombre"}
# Si NAME tiene valor: muestra "tiene nombre"
# Si NAME está vacía: muestra nada (cadena vacía)

# ${var:?message} — error si var está vacía o no definida
echo ${NAME:?"ERROR: NAME no está definida"}
# Si NAME tiene valor: muestra el valor
# Si NAME está vacía: imprime el mensaje y sale con error
```

### La diferencia del : (colon)

```bash
# Sin colon: solo verifica si la variable está definida
# Con colon: verifica si está definida Y no vacía

VAR=""

echo ${VAR-default}    # (vacío — VAR está definida, aunque vacía)
echo ${VAR:-default}   # default  (VAR está vacía)

unset VAR
echo ${VAR-default}    # default  (VAR no está definida)
echo ${VAR:-default}   # default  (VAR no está definida)
```

### Tabla completa de expansiones

| Expansión | Variable no definida | Variable vacía ("") | Variable con valor |
|---|---|---|---|
| `${var-default}` | default | (vacío) | valor |
| `${var:-default}` | default | **default** | valor |
| `${var=default}` | default (y asigna) | (vacío) | valor |
| `${var:=default}` | default (y asigna) | **default (y asigna)** | valor |
| `${var+alt}` | (vacío) | alt | alt |
| `${var:+alt}` | (vacío) | (vacío) | **alt** |
| `${var?error}` | error y sale | (vacío) | valor |
| `${var:?error}` | error y sale | **error y sale** | valor |

### Patrones comunes con defaults

```bash
# Valor por defecto para parámetros de script
PORT=${1:-8080}
HOST=${2:-"localhost"}

# Variables de configuración con fallback
LOG_DIR=${LOG_DIR:-"/var/log/myapp"}
DEBUG=${DEBUG:-0}

# Validar que una variable requerida existe
DB_HOST=${DB_HOST:?"Falta la variable DB_HOST"}
```

---

## Manipulación de strings

### Longitud

```bash
NAME="hello world"
echo ${#NAME}
# 11

# Útil en validaciones
if [[ ${#PASSWORD} -lt 8 ]]; then
    echo "Contraseña demasiado corta"
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

# Offset negativo (desde el final, necesita espacio antes del -)
echo ${STR: -6}
# orld!
```

### Eliminar patrones (prefix y suffix)

```bash
FILE="/home/dev/project/main.tar.gz"

# ${var#pattern} — eliminar el prefijo más CORTO que coincida
echo ${FILE#*/}
# home/dev/project/main.tar.gz  (eliminó el primer /)

# ${var##pattern} — eliminar el prefijo más LARGO que coincida
echo ${FILE##*/}
# main.tar.gz  (eliminó todo hasta el último /)
# Equivale a: basename "$FILE"

# ${var%pattern} — eliminar el sufijo más CORTO que coincida
echo ${FILE%.*}
# /home/dev/project/main.tar  (eliminó .gz)

# ${var%%pattern} — eliminar el sufijo más LARGO que coincida
echo ${FILE%%.*}
# /home/dev/project/main  (eliminó .tar.gz)
```

### Regla mnemotécnica

```
# está a la IZQUIERDA del $ en el teclado → elimina PREFIJO (izquierda)
% está a la DERECHA del $ en el teclado → elimina SUFIJO (derecha)

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

# Obtener la extensión
echo ${FILE##*.}          # gz (última extensión)

# Obtener el nombre sin extensión
BASENAME=${FILE##*/}
echo ${BASENAME%.*}       # document.tar (quita .gz)
echo ${BASENAME%%.*}      # document (quita .tar.gz)

# Cambiar extensión
echo ${FILE%.gz}.bz2      # /path/to/document.tar.bz2
```

### Búsqueda y reemplazo

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
# HelloWorldHello  (eliminó todos los espacios)
```

### Cambiar mayúsculas/minúsculas (bash 4+)

```bash
STR="Hello World"

echo ${STR,,}     # hello world (todo minúsculas)
echo ${STR^^}     # HELLO WORLD (todo mayúsculas)
echo ${STR,}      # hello World (primera letra a minúscula)
echo ${STR^}      # Hello World (primera letra a mayúscula)

# También funciona en reasignación
echo ${STR,,}
echo $STR          # No modifica la variable original

STR=${STR,,}      # Ahora sí modifica
echo $STR          # hello world
```

---

## Variables especiales del shell

| Variable | Descripción |
|---|---|
| `$0` | Nombre del script |
| `$1` ... `$9` | Parámetros posicionales |
| `${10}` | Parámetro 10+ (necesita llaves) |
| `$#` | Número de parámetros |
| `$@` | Todos los parámetros (como palabras separadas) |
| `$*` | Todos los parámetros (como una sola cadena) |
| `$$` | PID del shell actual |
| `$!` | PID del último proceso en background |
| `$?` | Exit code del último comando |
| `$_` | Último argumento del comando anterior |
| `$-` | Flags activos del shell (himBHs) |

### $@ vs $*

```bash
# Con archivos que tienen espacios:
set -- "file one.txt" "file two.txt" "file three.txt"

# $@ preserva las separaciones (3 argumentos)
for f in "$@"; do echo "archivo: $f"; done
# archivo: file one.txt
# archivo: file two.txt
# archivo: file three.txt

# $* junta todo en una cadena (1 argumento)
for f in "$*"; do echo "archivo: $f"; done
# archivo: file one.txt file two.txt file three.txt

# SIEMPRE usar "$@" (con comillas) para iterar parámetros
```

---

## Expansión aritmética

```bash
# $(( expresión ))
echo $((2 + 3))       # 5
echo $((10 / 3))      # 3 (entero, sin decimales)
echo $((10 % 3))      # 1 (módulo)
echo $((2 ** 10))     # 1024 (potencia)

# Con variables (no necesitan $)
X=5
echo $((X * 2))       # 10
echo $((X++))         # 5 (post-incremento, X ahora es 6)
echo $X               # 6

# Asignación
((COUNT++))
((TOTAL = X + Y))

# Operadores disponibles: + - * / % ** & | ^ ~ << >>
```

---

## Expansión de comandos

```bash
# $() — capturar la salida de un comando
FILES=$(ls /etc/*.conf)
DATE=$(date +%Y-%m-%d)
CPUS=$(nproc)

# Backticks (legacy, evitar)
FILES=`ls /etc/*.conf`
# Los backticks no se anidan bien y son menos legibles
# SIEMPRE preferir $()

# Se pueden anidar
DIR=$(dirname $(readlink -f $0))
```

---

## Ejercicios

### Ejercicio 1 — Defaults y validación

```bash
#!/bin/bash
PORT=${1:-8080}
HOST=${2:-"0.0.0.0"}
MODE=${3:?"Falta el modo (dev|prod)"}

echo "Servidor en $HOST:$PORT modo $MODE"

# Probar:
# bash /tmp/test-defaults.sh                     → error: MODE no definida
# bash /tmp/test-defaults.sh 3000 localhost dev   → Servidor en localhost:3000 modo dev
# bash /tmp/test-defaults.sh 3000                 → error: MODE no definida
```

---

### Ejercicio 2 — Manipulación de strings

```bash
FILE="/var/log/nginx/access.log.2024-03-17.gz"

echo "Directorio: ${FILE%/*}"
echo "Nombre: ${FILE##*/}"
echo "Sin extensión gz: ${FILE%.gz}"
echo "Solo extensión final: ${FILE##*.}"

# Cambiar la extensión .gz por .bz2
echo "Con bz2: ${FILE%.gz}.bz2"
```

---

### Ejercicio 3 — Buscar y reemplazar

```bash
URL="https://api.example.com/v1/users?page=1&limit=10"

# Extraer el dominio (entre // y /)
TEMP=${URL#*//}
DOMAIN=${TEMP%%/*}
echo "Dominio: $DOMAIN"

# Reemplazar https por http
echo ${URL/https/http}

# Eliminar los query params (todo después de ?)
echo ${URL%%\?*}
```

---

### Ejercicio 4 — Mayúsculas y minúsculas

```bash
NAME="Juan Pérez"

# Convertir a minúsculas
echo ${NAME,,}

# Convertir a mayúsculas
echo ${NAME^^}

# Solo primera letra
echo ${NAME^}

# En una asignación
LOWER_NAME=${NAME,,}
echo "Lower: $LOWER_NAME"
```

---

### Ejercicio 5 — Extracción de paths

```bash
FULL_PATH="/home/dev/projects/my-app/src/main.c"

# Directorio padre
echo "Directorio: ${FULL_PATH%/*}"

# Nombre del archivo
echo "Archivo: ${FULL_PATH##*/}"

# Múltiples operaciones
BASE=${FULL_PATH##*/}           # main.c
BASE_NO_EXT=${BASE%.*}         # main
EXT=${FULL_PATH##*.}           # c

echo "Base: $BASE_NO_EXT, Ext: $EXT"
```

---

### Ejercicio 6 — Longitud y substring

```bash
TEXT="abcdefghij"

# Longitud
echo "Longitud: ${#TEXT}"

# Primeros 4 caracteres
echo "Primeros 4: ${TEXT:0:4}"

# Últimos 4 caracteres
echo "Últimos 4: ${TEXT: -4}"

# Del 5 en adelante
echo "Del 5 en adelante: ${TEXT:4}"

# Invertir (hack)
echo "Invertido: $(echo $TEXT | rev)"
```

---

### Ejercicio 7 — Arrays y expansiones

```bash
# bash tiene arrays (no POSIX)
ARR=(uno dos tres cuatro)

# Longitud del array
echo "Elementos: ${#ARR[@]}"

# Todos los elementos
echo "Todos: ${ARR[@]}"
echo "Todos con *: ${ARR[*]}"

# Slice: primeros 3
echo "Primeros 3: ${ARR[@]:0:3}"

# Expandir índices
echo "Índices: ${!ARR[@]}"
```

---

### Ejercicio 8 — Expansión en strings complejos

```bash
USER="admin"
HOST="server1"
PORT="22"

# Construcción de strings
SSH="ssh ${USER}@${HOST} -p ${PORT}"
echo $SSH

# URL builder
PROTOCOL="https"
DOMAIN="api.example.com"
VERSION="v2"
ENDPOINT="users"

URL="${PROTOCOL}://${DOMAIN}/${VERSION}/${ENDPOINT}"
echo $URL
```

---

### Ejercicio 9 — Default con variables de entorno

```bash
#!/bin/bash
# typical environment config pattern

DB_HOST=${DB_HOST:-localhost}
DB_PORT=${DB_PORT:-5432}
DB_NAME=${DB_NAME:-myapp}
DB_USER=${DB_USER:-postgres}

echo "Conectando a ${DB_HOST}:${DB_PORT}/${DB_NAME} como ${DB_USER}"

# Si DB_PASS no está definida, abortar
DB_PASS=${DB_PASS:?"ERROR: DB_PASS debe estar definida"}

echo "DB_PASS está configurada (no se muestra por seguridad)"
```

---

### Ejercicio 10 — Pipeline de procesamiento

```bash
FILENAME="/tmp/data.old.backup.tar.gz"

# Encontrar la extensión real
EXT1=${FILENAME##*.}              # gz
BASENAME=${FILENAME##*/}           # data.old.backup.tar.gz
NAME_WITHOUT_EXT=${BASENAME%.*}    # data.old.backup.tar
EXT2=${NAME_WITHOUT_EXT##*.}       # tar
NAME_WITHOUT_EXT2=${NAME_WITHOUT_EXT%.*}  # data.old.backup

echo "Archivo: $BASENAME"
echo "Extensión 1: $EXT1"
echo "Extensión 2: $EXT2"
echo "Nombre base: $NAME_WITHOUT_EXT2"

# Determinar tipo de archivo
case "$EXT1" in
    gz|bz2|xz) echo "Comprimido con $EXT1" ;;
    tar) echo "Archivo tar sin comprimir" ;;
    *) echo "Formato desconocido" ;;
esac
```
