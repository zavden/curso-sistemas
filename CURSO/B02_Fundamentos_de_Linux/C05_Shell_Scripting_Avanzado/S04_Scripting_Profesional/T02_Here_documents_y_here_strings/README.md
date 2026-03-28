# T02 — Here documents y here strings

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

---

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
