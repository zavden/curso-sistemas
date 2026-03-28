# T02 — Here documents y here strings

> **Fuentes**: README.md + README.max.md + labs/README.md
> **Regla aplicada**: 2 (`.max.md` sin `.claude.md`)

## Errata detectada en las fuentes

| # | Archivo | Línea(s) | Error | Corrección |
|---|---------|----------|-------|------------|
| 1 | max.md | 451 | Comilla sin cerrar: `&>> "$LOG_FILE &` | Debe ser `&>> "$LOG_FILE" &` |
| 2 | max.md | 27 | Tabla: "Sin tabulaciones si se usa `<<-`" | `<<-` es precisamente para USAR tabs (los elimina del output); la regla está invertida |
| 3 | max.md | 534-538 | `passwd <<< "password"` y `chsh <<< "/bin/zsh"` | `passwd` no lee de stdin por defecto; usar `chpasswd <<< "user:pass"` o `passwd --stdin` (Red Hat) |
| 4 | max.md | 547-561 | `envsubst` con placeholders `{{NAME}}` | `envsubst` solo reemplaza `$VAR`/`${VAR}`, NO `{{VAR}}`; la plantilla no se sustituye |

---

## Here document — `<<`

Un here document permite pasar un bloque de texto multilínea como stdin de un
comando:

```bash
# Sintaxis:
COMANDO << DELIMITADOR
línea 1
línea 2
...
DELIMITADOR

# El DELIMITADOR puede ser cualquier palabra (EOF, END, HEREDOC, etc.)
# Debe aparecer SOLO en su línea, sin espacios ni caracteres extra
```

### Reglas del delimitador

| Regla | Ejemplo correcto | Error |
|---|---|---|
| Solo en su propia línea | `EOF` | ` EOF` (espacio antes) |
| Sin caracteres extra | `EOF` | `EOF ` (espacio después) |
| Case-sensitive | `EOF` ≠ `Eof` | — |

```bash
# Ejemplo básico:
cat << EOF
Hola, este es un
texto de múltiples
líneas.
EOF
```

### Expansiones dentro de here documents

Por defecto, los here documents **sí** expanden variables y comandos:

```bash
NAME="World"
DATE=$(date +%Y-%m-%d)

cat << EOF
Hola, $NAME.
Hoy es $DATE.
Tu home es $HOME.
Resultado: $((2 + 2))
EOF

# Salida:
# Hola, World.
# Hoy es 2024-03-17.
# Tu home es /home/dev.
# Resultado: 4
```

### Sin expansión — `<<'DELIMITADOR'`

Poniendo el delimitador entre comillas simples, **nada** se expande:

```bash
cat << 'EOF'
Hola, $NAME.
Hoy es $(date).
Tu home es $HOME.
Resultado: $((2 + 2))
EOF

# Salida (literal):
# Hola, $NAME.
# Hoy es $(date).
# Tu home es $HOME.
# Resultado: $((2 + 2))
```

```bash
# Fundamental para generar scripts dentro de scripts:
cat << 'EOF' > /tmp/generated-script.sh
#!/bin/bash
echo "PID: $$"
echo "User: $(whoami)"
echo "Home: $HOME"
EOF

# Sin las comillas en 'EOF', $$, $(whoami) y $HOME se expandirían
# al momento de crear el archivo, no al ejecutarlo
```

### Indentación — `<<-`

`<<-` elimina **tabs** iniciales del contenido y el delimitador (no espacios):

```bash
# Sin <<-: el delimitador debe estar al inicio de la línea
if true; then
    cat << EOF
Este texto no está indentado
aunque el código sí
EOF
fi

# Con <<-: los TABS al inicio se eliminan
if true; then
    cat <<- EOF
		Este texto puede estar indentado con tabs
		y el delimitador también
	EOF
fi
# IMPORTANTE: la indentación debe ser TABS, no espacios
# Muchos editores convierten tabs a espacios — verificar la configuración
```

### Tabla: variantes de here document

| Variante | Sintaxis | Expande `$VAR` | Expande `$(cmd)` | Elimina tabs |
|---|---|---|---|---|
| Normal | `<< EOF` | Sí | Sí | No |
| Literal | `<< 'EOF'` | No | No | No |
| Con tabs | `<<- EOF` | Sí | Sí | Sí (tabs iniciales) |
| Literal + tabs | `<<- 'EOF'` | No | No | Sí (tabs iniciales) |

---

## Usos comunes

### Crear archivos

```bash
# Crear un archivo con contenido:
cat << 'EOF' > /tmp/config.conf
# Configuración generada automáticamente
server_host=localhost
server_port=8080
log_level=info
EOF

# Append a un archivo:
cat << 'EOF' >> /tmp/config.conf
# Sección adicional
debug=false
EOF
```

### Pasar texto a comandos

```bash
# Enviar SQL a mysql:
DB_NAME="myapp"
DB_USER="appuser"

mysql -u root << EOF
CREATE DATABASE IF NOT EXISTS $DB_NAME;
GRANT ALL ON $DB_NAME.* TO '$DB_USER'@'localhost';
EOF

# SSH remoto — usar 'EOF' para que los comandos se ejecuten en el remoto:
ssh user@server << 'EOF'
echo "Ejecutando en $(hostname)"
df -h /
free -h
EOF
```

### Generar scripts

```bash
# Script que genera otro script con expansión parcial:
generate_init_script() {
    local service_name="$1"
    local exec_path="$2"

    cat << EOF
#!/bin/bash
# Auto-generated init script for $service_name

start() {
    echo "Starting $service_name..."
    $exec_path &
    echo \$! > /var/run/${service_name}.pid
}

stop() {
    echo "Stopping $service_name..."
    kill \$(cat /var/run/${service_name}.pid)
    rm -f /var/run/${service_name}.pid
}

case "\$1" in
    start) start ;;
    stop)  stop ;;
    *)     echo "Usage: \$0 {start|stop}" ;;
esac
EOF
}

generate_init_script "myapp" "/usr/local/bin/myapp" > /tmp/myapp-init.sh
# Nota: \$! \$( \$1 \$0 tienen \ para que NO se expandan al generar
# sino al EJECUTAR el script generado
```

> **Cuándo usar `<< EOF` vs `<< 'EOF'` para generar scripts**:
> - `<< 'EOF'` (literal): cuando TODO el contenido debe ser literal. Simple.
> - `<< EOF` (con expansión): cuando necesitas inyectar valores del script
>   generador (como `$service_name` arriba), pero debes escapar con `\` todo
>   lo que debe ser literal en el script generado (`\$!`, `\$(cmd)`, `\$1`).

### Mensajes de uso (usage/help)

```bash
usage() {
    cat << EOF
Uso: $(basename "$0") [opciones] <archivo>

Opciones:
  -v, --verbose     Modo verbose
  -o, --output FILE Archivo de salida
  -h, --help        Mostrar esta ayuda

Versión: $VERSION
EOF
}
# $(basename "$0") SÍ se expande — muestra el nombre real del script
```

---

## Asignar a una variable

```bash
# Capturar un here document en una variable con $(cat ...):
TEMPLATE=$(cat << 'EOF'
{
    "name": "$NAME",
    "version": "$VERSION",
    "timestamp": "$TIMESTAMP"
}
EOF
)

# Luego expandir con envsubst:
export NAME="myapp" VERSION="1.0" TIMESTAMP=$(date -Is)
echo "$TEMPLATE" | envsubst
# envsubst reemplaza $VAR y ${VAR} — las variables DEBEN estar exportadas
```

```bash
# Alternativa — read con here document:
read -r -d '' HELP_TEXT << 'EOF' || true
Línea 1
Línea 2
Línea 3
EOF
# || true porque read retorna 1 cuando llega a EOF sin encontrar el
# delimitador -d '' (NUL byte). El texto se lee correctamente.
echo "$HELP_TEXT"
```

> **Nota sobre trailing newlines**: `$()` (command substitution) elimina TODOS
> los newlines finales del output — esto es comportamiento POSIX, no un bug.
> Si necesitas preservarlos, agrega un carácter marcador al final y luego
> quítalo con `${VAR%marcador}`.

---

## Here string — `<<<`

Un here string pasa un **string** como stdin de un comando (sin crear un
bloque multilínea):

```bash
# Sintaxis:
COMANDO <<< "string"
```

```bash
# Equivalencias:
echo "hello" | grep -o 'ell'    # pipe
grep -o 'ell' <<< "hello"       # here string (más eficiente, sin subshell)

# Variables se expanden:
NAME="World"
cat <<< "Hello, $NAME"
# Hello, World
```

### Here string vs pipe: el problema de las subshells

```bash
# Pipe: crea un subshell para el lado derecho — las variables NO persisten:
echo "hello world" | read -r A B
echo "$A"    # (vacío — read se ejecutó en un subshell)

# Here string: no crea subshell — las variables SÍ persisten:
read -r A B <<< "hello world"
echo "$A"    # hello
```

### Usos prácticos

```bash
# Parsear un string en campos con IFS:
LINE="alice:30:madrid"
IFS=: read -r NAME AGE CITY <<< "$LINE"
echo "$NAME tiene $AGE años en $CITY"

# Primer componente del PATH:
IFS=: read -r FIRST REST <<< "$PATH"
echo "Primer directorio: $FIRST"

# Alimentar bc para cálculos:
RESULT=$(bc <<< "scale=2; 100 / 3")
echo "$RESULT"   # 33.33

# Condición sin pipe:
if grep -q 'error' <<< "$LOG_LINE"; then
    echo "Error encontrado"
fi

# Base64 decode:
base64 -d <<< "SGVsbG8gV29ybGQ="
# Hello World
```

---

## Comparación general

| Feature | Here doc `<<` | Here doc `<<'EOF'` | Here string `<<<` | Pipe `echo \| cmd` |
|---|---|---|---|---|
| Multilínea | Sí | Sí | Típicamente no¹ | No |
| Expande `$VAR` | Sí | No | Sí | Sí |
| Crea subshell | No | No | No | Sí (lado derecho) |
| Uso típico | Generar texto | Scripts literales | Pasar string a stdin | Evitar² |

¹ Técnicamente `<<<` acepta strings con newlines embebidos, pero no es su uso
habitual.
² Preferir here string sobre pipe cuando se alimenta un solo comando.

### Here document vs echo/printf

| Criterio | Here doc | echo/printf |
|---|---|---|
| Líneas múltiples | Natural | Difícil (muchos `\n`) |
| Comillas en contenido | Sin conflicto | Escaping complicado |
| Legibilidad | Alta | Variable |
| Rendimiento | Ligeramente más lento | Más rápido |

---

## Labs

### Parte 1 — Here documents

#### Paso 1.1: `<<` con expansión

```bash
NAME="DevOps"
VERSION="2.0"

cat << EOF
Proyecto: $NAME
Versión: $VERSION
Fecha: $(date +%Y-%m-%d)
Hostname: $(hostname)
Shell: $SHELL
EOF
# Variables y comandos se expanden porque EOF no tiene comillas
```

#### Paso 1.2: `<<'EOF'` sin expansión

```bash
NAME="DevOps"

cat << 'EOF'
Proyecto: $NAME
Versión: $(cat /etc/os-release | head -1)
PID: $$
Aritmética: $((2 + 2))
EOF
# TODO se imprime literal: $NAME, $(cmd), $$, $((expr))
```

#### Paso 1.3: Generar scripts

```bash
SERVICE="myapp"
PORT=8080

cat << EOF > /tmp/generated.sh
#!/bin/bash
# Script generado para $SERVICE en puerto $PORT
echo "Iniciando $SERVICE en puerto $PORT"
echo "PID: \$\$"
echo "Usuario: \$(whoami)"
echo "Fecha: \$(date)"
EOF

cat /tmp/generated.sh    # ver el script generado
chmod +x /tmp/generated.sh
/tmp/generated.sh        # ejecutar
rm /tmp/generated.sh

# $SERVICE y $PORT se expanden al GENERAR (valores inyectados)
# \$\$ y \$(cmd) se expanden al EJECUTAR (escapados con \)
```

#### Paso 1.4: Crear archivos de configuración

```bash
DB_HOST="db.example.com"
DB_PORT=5432
DB_NAME="myapp"

# Crear con >
cat << EOF > /tmp/config.ini
[database]
host = $DB_HOST
port = $DB_PORT
name = $DB_NAME

[logging]
level = info
file = /var/log/$DB_NAME.log
EOF

# Append con >>
cat << EOF >> /tmp/config.ini

[cache]
enabled = true
ttl = 300
EOF

cat /tmp/config.ini
rm /tmp/config.ini
```

### Parte 2 — Usos avanzados

#### Paso 2.1: Mensajes de uso (usage)

```bash
cat > /tmp/usage-demo.sh << 'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_NAME=$(basename "$0")

usage() {
    cat << EOF
Uso: $SCRIPT_NAME [opciones] <archivo>

Descripción:
  Procesa archivos de datos y genera reportes.

Opciones:
  -v, --verbose     Modo verbose
  -o, --output FILE Archivo de salida
  -h, --help        Mostrar esta ayuda

Ejemplos:
  $SCRIPT_NAME -v data.csv
  $SCRIPT_NAME --output report.txt *.csv

Versión: 1.0.0
EOF
}

[[ "${1:-}" == "-h" || "${1:-}" == "--help" || $# -eq 0 ]] && { usage; exit 0; }
echo "Procesando: $@"
SCRIPT
chmod +x /tmp/usage-demo.sh

/tmp/usage-demo.sh --help
# $(basename "$0") se expande al nombre real del script
rm /tmp/usage-demo.sh
```

#### Paso 2.2: Asignar a variable

```bash
# Con $(cat << 'EOF' ... EOF)
TEMPLATE=$(cat << 'EOF'
{
    "name": "$NAME",
    "version": "$VERSION",
    "date": "$DATE"
}
EOF
)
echo "Template guardado:"
echo "$TEMPLATE"

# Con read -r -d ''
read -r -d '' JSON_TEMPLATE << 'EOF' || true
{
    "server": "localhost",
    "port": 8080,
    "debug": true
}
EOF
echo "JSON via read:"
echo "$JSON_TEMPLATE"
```

#### Paso 2.3: Pasar a comandos

```bash
# grep con here document:
grep "error" << 'EOF'
info: todo bien
error: algo falló
warning: atención
error: otro fallo
EOF

# while read con here document:
while IFS=: read -r key value; do
    echo "  $key = $value"
done << 'EOF'
host:localhost
port:8080
debug:true
EOF
```

### Parte 3 — Here strings

#### Paso 3.1: `<<<` básico

```bash
# Pasar string como stdin:
grep -o "world" <<< "hello world"

# Variables se expanden:
NAME="bash"
cat <<< "Hola, $NAME"
```

#### Paso 3.2: Parsing con here string

```bash
# Separar campos con IFS:
LINE="alice:30:madrid:dev"
IFS=: read -r name age city role <<< "$LINE"
echo "Nombre: $name, Edad: $age, Ciudad: $city, Rol: $role"

# CSV:
CSV="producto,100,USD"
IFS=, read -r item price currency <<< "$CSV"
echo "$item cuesta $price $currency"

# El problema del pipe vs here string:
echo "hello world" | read -r A B
echo "Con pipe: A='$A' B='$B'"     # vacío (subshell)

read -r A B <<< "hello world"
echo "Con <<<:  A='$A' B='$B'"     # hello world (mismo shell)

# Cálculos con bc:
RESULT=$(bc <<< "scale=2; 100 / 3")
echo "100 / 3 = $RESULT"
```

---

## Ejercicios

### Ejercicio 1 — Expansión vs literal

```bash
NAME="DevOps"
VERSION="2.0"

echo "=== Con expansión ==="
cat << EOF
Proyecto: $NAME
Versión: $VERSION
Fecha: $(date +%Y-%m-%d)
EOF

echo ""
echo "=== Sin expansión ==="
cat << 'EOF'
Proyecto: $NAME
Versión: $VERSION
Fecha: $(date +%Y-%m-%d)
EOF
```

<details><summary>Predicción</summary>

```
=== Con expansión ===
Proyecto: DevOps
Versión: 2.0
Fecha: 2026-03-25

=== Sin expansión ===
Proyecto: $NAME
Versión: $VERSION
Fecha: $(date +%Y-%m-%d)
```

Sin comillas en `EOF`: `$NAME`, `$VERSION`, `$(date)` se expanden a sus
valores. Con comillas en `'EOF'`: todo se imprime literal, incluyendo los
signos `$` y `$(...)`.

</details>

### Ejercicio 2 — Generar un archivo de configuración

```bash
HOST="db.example.com"
PORT=5432
DB="myapp_prod"

cat << EOF > /tmp/db-config.ini
[database]
host = $HOST
port = $PORT
name = $DB
user = app_$(echo "$DB" | tr '-' '_')
EOF

cat /tmp/db-config.ini
rm /tmp/db-config.ini
```

<details><summary>Predicción</summary>

```ini
[database]
host = db.example.com
port = 5432
name = myapp_prod
user = app_myapp_prod
```

`$HOST`, `$PORT`, `$DB` se expanden a sus valores. `$(echo "$DB" | tr '-' '_')`
ejecuta el command substitution: toma `myapp_prod`, reemplaza `-` por `_`
(pero no hay guiones, así que queda igual). Resultado: `app_myapp_prod`.

</details>

### Ejercicio 3 — Here string: parsing con IFS

```bash
CSV_LINE="alice,30,madrid,dev"
IFS=, read -r NAME AGE CITY ROLE <<< "$CSV_LINE"
echo "Nombre: $NAME, Edad: $AGE, Ciudad: $CITY, Rol: $ROLE"

# Ahora con otro delimitador:
LOG_ENTRY="2024-03-17|ERROR|disk full|server01"
IFS='|' read -r DATE LEVEL MSG HOST <<< "$LOG_ENTRY"
echo "[$DATE] $LEVEL en $HOST: $MSG"
```

<details><summary>Predicción</summary>

```
Nombre: alice, Edad: 30, Ciudad: madrid, Rol: dev
[2024-03-17] ERROR en server01: disk full
```

`IFS=,` frente a `read` solo cambia IFS para ese comando (no modifica el
IFS global). Cada campo se asigna a la variable correspondiente. El segundo
ejemplo usa `|` como delimitador — funciona igual.

</details>

### Ejercicio 4 — Here string vs pipe: subshell

```bash
# Con pipe:
echo "hello world" | read -r A B
echo "Pipe: A='$A' B='$B'"

# Con here string:
read -r A B <<< "hello world"
echo "Here string: A='$A' B='$B'"

# Predice: ¿cuántos procesos crea cada uno?
```

<details><summary>Predicción</summary>

```
Pipe: A='' B=''
Here string: A='hello' B='world'
```

Con pipe, `read` se ejecuta en una **subshell** (el lado derecho del pipe).
Las variables `A` y `B` se asignan dentro de la subshell y se pierden al
regresar al shell padre. Resultado: vacío.

Con here string, `read` se ejecuta en el **shell actual**. Las variables
persisten. Resultado: `hello` y `world`.

Procesos: el pipe crea al menos 2 procesos (uno para `echo`, otro para la
subshell de `read`). El here string no crea procesos adicionales — `read`
es un builtin que se ejecuta in-place.

</details>

### Ejercicio 5 — Generar un script con expansión parcial

```bash
SERVICE="myapp"
PORT=8080

cat << EOF > /tmp/service.sh
#!/bin/bash
# Script generado para $SERVICE

echo "Iniciando $SERVICE en puerto $PORT"
echo "PID actual: \$\$"
echo "Ejecutado por: \$(whoami)"
echo "Argumentos: \$@"
EOF

echo "=== Script generado ==="
cat /tmp/service.sh

echo ""
echo "=== Ejecutando ==="
chmod +x /tmp/service.sh
/tmp/service.sh arg1 arg2

rm /tmp/service.sh
```

<details><summary>Predicción</summary>

Script generado:
```bash
#!/bin/bash
# Script generado para myapp

echo "Iniciando myapp en puerto 8080"
echo "PID actual: $$"
echo "Ejecutado por: $(whoami)"
echo "Argumentos: $@"
```

`$SERVICE` y `$PORT` se expanden al GENERAR (inyectan valores). `\$\$`,
`\$(whoami)`, `\$@` tienen `\` que se consume en la expansión, dejando
`$$`, `$(whoami)`, `$@` literales en el script generado.

Al ejecutar:
```
Iniciando myapp en puerto 8080
PID actual: 12345
Ejecutado por: zavden
Argumentos: arg1 arg2
```
(PID varía)

</details>

### Ejercicio 6 — `<<'EOF'` para generar script completamente literal

```bash
cat << 'EOF' > /tmp/monitor.sh
#!/bin/bash
echo "=== System Monitor ==="
echo "Hostname: $(hostname)"
echo "Uptime: $(uptime -p)"
echo "Users: $(who | wc -l)"
echo "Disk: $(df -h / | tail -1 | awk '{print $5}') used"
echo "Memory: $(free -h | awk '/Mem/{print $3}') used"
echo "PID: $$"
EOF

echo "=== Script generado ==="
cat /tmp/monitor.sh

echo ""
echo "=== Ejecutando ==="
chmod +x /tmp/monitor.sh
bash /tmp/monitor.sh

rm /tmp/monitor.sh
```

<details><summary>Predicción</summary>

Script generado (idéntico al contenido — nada se expande):
```bash
#!/bin/bash
echo "=== System Monitor ==="
echo "Hostname: $(hostname)"
echo "Uptime: $(uptime -p)"
...
echo "PID: $$"
```

Al ejecutar, cada `$(cmd)` se ejecuta en ese momento y `$$` es el PID del
proceso bash. La salida muestra los valores reales del sistema.

La diferencia con el Ejercicio 5: aquí no se necesita escapar nada con `\`
porque `'EOF'` hace que TODO sea literal. Es más limpio cuando no necesitas
inyectar valores del script generador.

</details>

### Ejercicio 7 — Here document a variable + envsubst

```bash
# Template con $VAR (no {{VAR}} — envsubst solo entiende $VAR)
TEMPLATE=$(cat << 'EOF'
Estimado/a $NAME,

Su pedido #$ORDER_ID ha sido $STATUS.

Monto total: $AMOUNT
Fecha: $DATE

Saludos,
El equipo de $COMPANY
EOF
)

# envsubst necesita variables EXPORTADAS
export NAME="Ana García"
export ORDER_ID="12345"
export STATUS="enviado"
export AMOUNT="€149.99"
export DATE=$(date +%Y-%m-%d)
export COMPANY="MiTienda"

echo "$TEMPLATE" | envsubst
```

<details><summary>Predicción</summary>

```
Estimado/a Ana García,

Su pedido #12345 ha sido enviado.

Monto total: €149.99
Fecha: 2026-03-25

Saludos,
El equipo de MiTienda
```

El template se captura literal (`'EOF'`) — los `$NAME` etc. no se expanden
al crear la variable. Luego `envsubst` lee el texto y reemplaza cada `$VAR`
con el valor de la variable de entorno correspondiente. Las variables DEBEN
estar exportadas (`export`) para que `envsubst` las vea.

</details>

### Ejercicio 8 — `read -d ''` con here document

```bash
# Capturar un bloque JSON en una variable:
read -r -d '' CONFIG << 'EOF' || true
{
    "server": "localhost",
    "port": 8080,
    "debug": true,
    "workers": 4
}
EOF

echo "Contenido de CONFIG:"
echo "$CONFIG"
echo ""
echo "Longitud: ${#CONFIG} caracteres"

# Extraer un valor con grep:
PORT=$(grep -oP '"port": \K\d+' <<< "$CONFIG")
echo "Puerto extraído: $PORT"
```

<details><summary>Predicción</summary>

```
Contenido de CONFIG:
{
    "server": "localhost",
    "port": 8080,
    "debug": true,
    "workers": 4
}

Longitud: 82 caracteres
Puerto extraído: 8080
```

`read -r -d ''` lee hasta encontrar un byte NUL (`\0`). Como el here document
no contiene NUL, `read` llega al final y retorna exit code 1 (por eso el
`|| true` para que no falle con `set -e`). El texto se lee correctamente en
`$CONFIG`. Luego `grep -oP '"port": \K\d+'` usa `\K` para descartar
`"port": ` y extraer solo `8080`.

</details>

### Ejercicio 9 — `<<-` con tabs (indentación)

```bash
#!/usr/bin/env bash

generate_nginx_config() {
    local server_name="$1"
    local port="$2"

    # <<- elimina tabs iniciales del output
    cat <<-EOF
		server {
		    listen       $port;
		    server_name  $server_name;

		    location / {
		        index index.html;
		    }
		}
	EOF
}

echo "=== Config generada ==="
generate_nginx_config "miweb.local" "8080"
```

<details><summary>Predicción</summary>

```
=== Config generada ===
server {
    listen       8080;
    server_name  miweb.local;

    location / {
        index index.html;
    }
}
```

`<<-` elimina todos los **tabs iniciales** de cada línea y del delimitador.
Esto permite indentar el here document dentro de la función sin que los tabs
aparezcan en el output. `$port` y `$server_name` se expanden porque `EOF`
no tiene comillas.

**Cuidado**: solo tabs, NO espacios. Si el editor convierte tabs a espacios,
`<<-` no funciona y el delimitador no se reconoce (error de "unexpected EOF").

</details>

### Ejercicio 10 — Panorama: script profesional con here documents

Un script que usa múltiples formas de here documents y here strings.

```bash
#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_NAME=$(basename "$0")

# --- 1. Usage con here document (expansión de $SCRIPT_NAME) ---
usage() {
    cat << EOF
Uso: $SCRIPT_NAME [-f formato] [-o archivo] datos.csv

Genera un reporte a partir de un CSV.

Opciones:
  -f FMT   Formato: text, html (default: text)
  -o FILE  Archivo de salida (default: stdout)
  -h       Ayuda
EOF
}

FORMAT="text"
OUTPUT=""

while getopts ":f:o:h" opt; do
    case $opt in
        f) FORMAT="$OPTARG" ;;
        o) OUTPUT="$OPTARG" ;;
        h) usage; exit 0 ;;
        \?) echo "$SCRIPT_NAME: opción inválida: -$OPTARG" >&2; exit 1 ;;
        :)  echo "$SCRIPT_NAME: -$OPTARG requiere argumento" >&2; exit 1 ;;
    esac
done
shift $((OPTIND - 1))

INPUT="${1:?Falta archivo de entrada (ver $SCRIPT_NAME -h)}"
[[ -f "$INPUT" ]] || { echo "$SCRIPT_NAME: $INPUT no encontrado" >&2; exit 1; }

# --- 2. Datos de prueba con here document literal ---
if [[ "$INPUT" == "--demo" ]]; then
    read -r -d '' DEMO_DATA << 'EOF' || true
nombre,edad,ciudad
Alice,30,Madrid
Bob,25,Barcelona
Carol,35,Valencia
EOF
    INPUT="/tmp/demo-data.csv"
    echo "$DEMO_DATA" > "$INPUT"
fi

# --- 3. Generar reporte ---
{
    case "$FORMAT" in
        text)
            echo "=== Reporte ==="
            echo "Archivo: $INPUT"
            echo "Fecha: $(date +%Y-%m-%d)"
            echo ""
            # --- 4. Here string para parsear el header ---
            IFS=, read -r -a HEADERS <<< "$(head -1 "$INPUT")"
            echo "Columnas: ${HEADERS[*]}"
            echo "Registros: $(($(wc -l < "$INPUT") - 1))"
            echo ""
            column -t -s, "$INPUT"
            ;;
        html)
            # --- 5. Here document para HTML ---
            cat << HTMLEOF
<!DOCTYPE html>
<html>
<head><title>Reporte — $INPUT</title></head>
<body>
<h1>Reporte</h1>
<p>Generado: $(date +%Y-%m-%d)</p>
<table border="1">
HTMLEOF
            while IFS=, read -r -a FIELDS; do
                echo "<tr>"
                for f in "${FIELDS[@]}"; do
                    echo "  <td>$f</td>"
                done
                echo "</tr>"
            done < "$INPUT"
            cat << 'HTMLEOF'
</table>
</body>
</html>
HTMLEOF
            ;;
    esac
} > "${OUTPUT:-/dev/stdout}"

[[ -n "$OUTPUT" ]] && echo "$SCRIPT_NAME: reporte escrito en $OUTPUT" >&2

# Probar:
# echo -e "nombre,edad,ciudad\nAlice,30,Madrid\nBob,25,Barcelona" > /tmp/data.csv
# ./report.sh /tmp/data.csv
# ./report.sh -f html -o /tmp/report.html /tmp/data.csv
```

<details><summary>Predicción</summary>

Con `/tmp/data.csv` conteniendo `nombre,edad,ciudad\nAlice,30,Madrid\nBob,25,Barcelona`:

**Formato text** (`./report.sh /tmp/data.csv`):
```
=== Reporte ===
Archivo: /tmp/data.csv
Fecha: 2026-03-25

Columnas: nombre edad ciudad
Registros: 2

nombre  edad  ciudad
Alice   30    Madrid
Bob     25    Barcelona
```

**Formato HTML** (`./report.sh -f html /tmp/data.csv`):
```html
<!DOCTYPE html>
<html>
<head><title>Reporte — /tmp/data.csv</title></head>
<body>
<h1>Reporte</h1>
<p>Generado: 2026-03-25</p>
<table border="1">
<tr>
  <td>nombre</td>
  <td>edad</td>
  <td>ciudad</td>
</tr>
<tr>
  <td>Alice</td>
  <td>30</td>
  <td>Madrid</td>
</tr>
...
```

El script demuestra:
1. `<< EOF` con expansión para usage y HTML (inyecta `$SCRIPT_NAME`, `$(date)`)
2. `<< 'EOF'` literal para datos de demo y cierre HTML
3. `<<<` here string para parsear el header CSV en un array
4. `read -r -d ''` para capturar bloque multilínea en variable

</details>
