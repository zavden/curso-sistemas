# T02 — Here Documents y Here Strings (Enhanced)

## Here document — <<

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
# Debe aparecer SOLO en la línea, sin espacios antes ni después
```

### Delimitador: reglas

| Regla | Ejemplo correcto | Ejemplo incorrecto |
|---|---|---|
| Solo en su propia línea | `EOF` | ` EOF` (espacio antes) |
| Sin caracteres extra | `EOF` | `EOF ` (espacio después) |
| Case-sensitive | `EOF` ≠ `Eof` | `Eof` (misma línea, contenido) |
| Sin tabulaciones si se usa `<<-` | `<<-EOF` | — |

```bash
# Ejemplo básico:
cat << EOF
Hola, este es un
texto de múltiples
líneas.
EOF
```

### Expansiones dentro de here documents

Por defecto, las here documents **sí** expanden variables y comandos:

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

### Sin expansión — <<'DELIMITADOR'

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
# Esto es fundamental para generar scripts dentro de scripts:
cat << 'EOF' > /tmp/generated-script.sh
#!/bin/bash
echo "PID: $$"
echo "User: $(whoami)"
echo "Home: $HOME"
EOF

# Sin las comillas en 'EOF', $$, $(whoami) y $HOME se expandirían
# al momento de crear el script, no al ejecutarlo
```

### Indentación — <<-

`<<-` permite indentar el contenido y el delimitador con **tabs** (no espacios):

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
# Muchos editores convierten tabs a espacios — cuidado
```

## Tabla comparativa: variantes de here document

| Variante | Sintaxis | Expande `$VAR` | Expande `$(cmd)` | Elimina tabs |
|---|---|---|---|---|
| Here doc normal | `<<EOF` | Sí | Sí | No |
| Here doc literal | `<<'EOF'` | No | No | No |
| Here doc con tabs | `<<-EOF` | Sí | Sí | Sí (tabs iniciales) |
| Here doc literal + tabs | `<<-'EOF'` | No | No | Sí (tabs iniciales) |

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
# Enviar a un comando que lee de stdin:
mysql -u root << EOF
CREATE DATABASE IF NOT EXISTS myapp;
GRANT ALL ON myapp.* TO 'appuser'@'localhost';
FLUSH PRIVILEGES;
EOF

# Con variables (sin comillas en EOF):
DB_NAME="myapp"
DB_USER="appuser"

mysql -u root << EOF
CREATE DATABASE IF NOT EXISTS $DB_NAME;
GRANT ALL ON $DB_NAME.* TO '$DB_USER'@'localhost';
EOF

# SSH remoto:
ssh user@server << 'EOF'
echo "Ejecutando en $(hostname)"
df -h /
free -h
EOF
```

### Generar scripts o configuraciones

```bash
# Script que genera otro script:
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

### Mensajes de uso (usage/help)

```bash
usage() {
    cat << EOF
Uso: $(basename "$0") [opciones] <archivo>

Descripción:
  Procesa archivos de datos y genera reportes.

Opciones:
  -v, --verbose     Modo verbose
  -o, --output FILE Archivo de salida
  -f, --format FMT  Formato: text, json, csv (default: text)
  -h, --help        Mostrar esta ayuda

Ejemplos:
  $(basename "$0") -v data.csv
  $(basename "$0") --format=json -o report.json *.csv

Versión: $VERSION
EOF
}
# $(basename "$0") SÍ se expande (sin comillas en EOF) — muestra el nombre real
```

## Asignar a una variable

```bash
# Capturar un here document en una variable:
TEMPLATE=$(cat << 'EOF'
{
    "name": "$NAME",
    "version": "$VERSION",
    "timestamp": "$TIMESTAMP"
}
EOF
)

# Luego expandir las variables manualmente:
NAME="myapp" VERSION="1.0" TIMESTAMP=$(date -Is)
echo "$TEMPLATE" | envsubst
# envsubst reemplaza las variables de entorno en el texto
```

```bash
# Otra forma — read con here document:
read -r -d '' HELP_TEXT << 'EOF' || true
Línea 1
Línea 2
Línea 3
EOF
# || true porque read retorna 1 cuando llega a EOF sin delimitador -d ''
echo "$HELP_TEXT"
```

### Limitaciones de capturar en variable

```bash
# PROBLEMA: los newlines finales pueden perderse:
TEXT=$(cat << 'EOF'
linea1
linea2
EOF
)
echo "${#TEXT}"  # puede no incluir el trailing newline

# SOLUCIÓN: agregar un caracter marcador:
TEXT=$(cat << 'EOF'
linea1
linea2
MARKER
EOF
)
TEXT="${TEXT%MARKER}"
```

## Here string — <<<

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

echo "hello world" | read -r A B    # BUG: pipe crea subshell
read -r A B <<< "hello world"       # OK: here string no crea subshell
echo "$A $B"                         # hello world

# Variables se expanden:
NAME="World"
cat <<< "Hello, $NAME"
# Hello, World
```

### Usos prácticos

```bash
# Parsear un string en campos:
LINE="alice:30:madrid"
IFS=: read -r NAME AGE CITY <<< "$LINE"
echo "$NAME tiene $AGE años en $CITY"

# Alimentar bc para cálculos:
RESULT=$(bc <<< "scale=2; 100 / 3")
echo "$RESULT"   # 33.33

# Comparar sin pipe:
if grep -q 'error' <<< "$LOG_LINE"; then
    echo "Error encontrado"
fi

# Base64 decode:
base64 -d <<< "SGVsbG8gV29ybGQ="
# Hello World

# Alimentar bc con variables:
X=10 Y=3
bc -l <<< "scale=4; $X / $Y"
# 3.3333
```

### Here string vs pipe

```bash
# Pipe: crea un subshell (las variables no persisten):
echo "hello world" | read -r A B
echo "$A"    # (vacío — read se ejecutó en un subshell)

# Here string: no crea subshell (las variables persisten):
read -r A B <<< "hello world"
echo "$A"    # hello

# Here string con variable:
read -r FIRST REST <<< "$PATH"
# FIRST contiene todo $PATH (no se separa por : porque IFS default es espacio)

IFS=: read -r FIRST REST <<< "$PATH"
# FIRST = /usr/local/bin (primer directorio del PATH)
```

## Comparación

| Feature | Here doc `<<` | Here doc `<<'EOF'` | Here string `<<<` |
|---|---|---|---|
| Multilínea | Sí | Sí | No (una línea) |
| Expande `$VAR` | Sí | No | Sí |
| Expande `$(cmd)` | Sí | No | Sí |
| Crea subshell | No | No | No |
| Uso típico | Generar texto | Scripts literales | Pasar string a stdin |

### Here document vs echo/printf

| Criterio | Here doc | echo/printf |
|---|---|---|
| Líneas múltiples | Natural | Difícil (muchos \n) |
| Comillas | Sin conflicto | Complicado escaping |
| Legibilidad | Alta | Variable |
|Velocidad| Ligeramente más lento | Más rápido |
| Trailing newlines | Se preservan | Depende de echo |

## Ejercicios

### Ejercicio 1 — Here document con y sin expansión

```bash
NAME="DevOps"
VERSION="2.0"

# Con expansión:
cat << EOF
Proyecto: $NAME
Versión: $VERSION
Fecha: $(date +%Y-%m-%d)
EOF

# Sin expansión:
cat << 'EOF'
Proyecto: $NAME
Versión: $VERSION
Fecha: $(date +%Y-%m-%d)
EOF

# ¿Cuál es la diferencia en la salida?
```

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

### Ejercicio 3 — Here string para parsing

```bash
# Parsear una línea CSV con here string:
CSV_LINE="alice,30,madrid,dev"
IFS=, read -r NAME AGE CITY ROLE <<< "$CSV_LINE"
echo "Nombre: $NAME, Edad: $AGE, Ciudad: $CITY, Rol: $ROLE"
```

### Ejercicio 4 — Generar script executable con variables de shell

```bash
#!/usr/bin/env bash

SCRIPT_NAME="myservice"
USER="www-data"
PID_FILE="/var/run/myservice.pid"
LOG_FILE="/var/log/myservice.log"

cat << 'EOF' > /tmp/generate_init.sh
#!/bin/bash
# Script generado automáticamente — NO editar manualmente

NAME="{{SCRIPT_NAME}}"
USER="{{USER}}"
PID_FILE="{{PID_FILE}}"
LOG_FILE="{{LOG_FILE}}"

start() {
    echo "Starting $NAME..."
    /usr/local/bin/$NAME &>> "$LOG_FILE &
    echo $! > "$PID_FILE"
}

stop() {
    echo "Stopping $NAME..."
    kill $(cat "$PID_FILE") 2>/dev/null
    rm -f "$PID_FILE"
}

status() {
    if [[ -f "$PID_FILE" ]]; then
        echo "$NAME is running (PID: $(cat $PID_FILE))"
    else
        echo "$NAME is not running"
    fi
}

case "$1" in
    start) start ;;
    stop)  stop ;;
    status) status ;;
    *)     echo "Usage: $0 {start|stop|status}" ;;
esac
EOF

sed -i "s/{{SCRIPT_NAME}}/$SCRIPT_NAME/g; s/{{USER}}/$USER/g; s|{{PID_FILE}}|$PID_FILE|g; s|{{LOG_FILE}}|$LOG_FILE|g" /tmp/generate_init.sh
chmod +x /tmp/generate_init.sh

# Probar:
# cat /tmp/generate_init.sh  # ver el script generado
```

### Ejercicio 5 — Multi-línea con indentación (<<-)

```bash
#!/bin/bash

generate_nginx_config() {
    local server_name="$1"
    local port="$2"
    local root="$3"

    cat <<-EOF
		server {
		    listen       $port;
		    server_name  $server_name;
		    root         $root;
		    access_log   /var/log/nginx/\${server_name}_access.log;
		    error_log    /var/log/nginx/\${server_name}_error.log;

		    location / {
		        index index.html index.htm;
		    }
		}
	EOF
}

# Probar (los tabs se eliminan en la salida):
generate_nginx_config "miweb.local" "8080" "/var/www/miweb"
# Los tabs iniciales de cada línea se eliminan con <<-
```

### Ejercicio 6 — Here string para alimentar comandos interactivos

```bash
#!/bin/bash

# Alimentar comandos a un programa interactivo via here string:
# echo "comandos" | ftp host  (equivalent a:)
# ftp host <<< "comandos\n..."

# Ejemplo con expect (simplificado):
# Esperar un prompt y responder:
# expect << 'EOF'
# spawn ssh user@host
# expect "password:"
# send "mypassword\r"
# expect "$ "
# send "exit\r"
# EOF

# Here string para responder a prompts:
passwd <<< "nueva_password
nueva_password"

# Cambiar shell a un usuario:
chsh <<< "/bin/zsh"
```

### Ejercicio 7 — Template engine simple con envsubst

```bash
#!/bin/bash

cat << 'TEMPLATE' > /tmp/email_template.txt
Estimado/a {{NAME}},

Su pedido #{{ORDER_ID}} ha sido {{STATUS}}.

Monto total: {{AMOUNT}}
Fecha: {{DATE}}

Saludos,
El equipo de {{COMPANY}}
TEMPLATE

NAME="Ana García" ORDER_ID="12345" STATUS="enviado"
AMOUNT="€149.99" DATE=$(date +%Y-%m-%d) COMPANY="MiTienda"

envsubst < /tmp/email_template.txt

# Probar:
# ./email_gen.sh
```

### Ejercicio 8 — Here document con matrices (arrays)

```bash
#!/bin/bash

declare -a SERVERS=("web01" "web02" "web03")
declare -a PORTS=(80 443 8080)

cat << EOF
Configuración de servidores:
$(for i in "${!SERVERS[@]}"; do
    echo "  ${SERVERS[$i]}: Puerto ${PORTS[$i]}"
done)
EOF

# Salida incluye los valores actuales de los arrays
```

### Ejercicio 9 — Read con here document para datos estructurados

```bash
#!/bin/bash

# Parsear configuración INI usando read con here document:
parse_ini() {
    local file="$1"
    declare -A SECTION_KEYS
    local current_section=""

    while IFS= read -r line; do
        # Saltar comentarios y líneas vacías:
        [[ "$line" =~ ^[[:space:]]*# ]] && continue
        [[ -z "${line// }" ]] && continue

        # Nueva sección:
        if [[ "$line" =~ ^\[([^\]]+)\] ]]; then
            current_section="${BASH_REMATCH[1]}"
        # Asignación key=value:
        elif [[ "$line" =~ ^([^=]+)=(.*) ]]; then
            local key="${BASH_REMATCH[1]}" value="${BASH_REMATCH[2]}"
            key=$(echo "$key" | tr -d '[:space:]')
            value=$(echo "$value" | tr -d '[:space:]')
            SECTION_KEYS["${current_section}:${key}"]="$value"
        fi
    done <<< "$(cat "$file")"

    # Imprimir resultado:
    for key in "${!SECTION_KEYS[@]}"; do
        echo "$key = ${SECTION_KEYS[$key]}"
    done
}

cat << 'INI' > /tmp/test.ini
[database]
host=localhost
port=5432
name=myapp

[server]
host=0.0.0.0
port=8080
INI

echo "=== Parseo INI ==="
parse_ini /tmp/test.ini

# Probar:
# ./parse_ini.sh
```

### Ejercicio 10 — Here document vs printf para logs multi-línea

```bash
#!/bin/bash

# Método 1: here document (limpio para múltiples líneas)
log_info() {
    cat << EOF
[$(date '+%Y-%m-%d %H:%M:%S')] [INFO]
  Usuario: ${USER:-root}
  PID: $$
  Args: $*
--
EOF
}

# Método 2: printf (mejor para una sola línea formateada)
log_info_simple() {
    printf '[%s] [INFO] Usuario: %s | PID: %d | Args: %s\n' \
        "$(date '+%Y-%m-%d %H:%M:%S')" \
        "${USER:-root}" \
        "$$" \
        "$*"
}

# Comparar salida:
echo "=== Log con here doc ==="
log_info "iniciando proceso" "argumento2"

echo ""
echo "=== Log con printf ==="
log_info_simple "iniciando proceso" "argumento2"

# Probar:
# ./log_comparison.sh
```

### Ejercicio 11 — Here string con xargs (ejecución paralela simulada)

```bash
#!/bin/bash

# Generar comandos para xargs con here string:
echo -e "file1.txt\nfile2.txt\nfile3.txt" | while IFS= read -r f; do
    echo "Procesando: $f"
done

# Equivalente con here string y process substitution:
while IFS= read -r file; do
    echo "Size of $file: $(wc -c < "$file") bytes"
done < <(find . -maxdepth 1 -name "*.md" -type f)

# Aquí strings para alimentar xargs con argumentos:
ITEMS=("alpha" "beta" "gamma")
printf '%s\n' "${ITEMS[@]}" | xargs -I {} echo "Item: {}"

# Aquí string para dry-run con confirmación:
cmd="rm -rf /tmp/test-dir"
[[ -n "$cmd" ]] && echo "Ejecutar: $cmd" || echo "Nada que ejecutar"
```
