# Lab — Here documents y here strings

## Objetivo

Dominar here documents (<< con expansion de variables, <<'EOF'
sin expansion, <<- con tabs), here strings (<<<), sus usos
comunes (crear archivos, generar scripts, mensajes de uso),
asignacion a variables, y la comparacion entre las distintas
formas.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Here documents

### Objetivo

Usar here documents para texto multilinea con y sin expansion.

### Paso 1.1: << con expansion

```bash
docker compose exec debian-dev bash -c '
echo "=== Here document con expansion ==="
echo ""
NAME="DevOps"
VERSION="2.0"
echo "--- Sin comillas en el delimitador: se expanden variables ---"
cat << EOF
Proyecto: $NAME
Version: $VERSION
Fecha: $(date +%Y-%m-%d)
Hostname: $(hostname)
Shell: $SHELL
EOF
echo ""
echo "Variables (\$NAME) y comandos (\$(date)) se expanden"
echo "porque EOF no tiene comillas"
'
```

### Paso 1.2: <<'EOF' sin expansion

Antes de ejecutar, predice: que se imprime si el delimitador
esta entre comillas simples?

```bash
docker compose exec debian-dev bash -c '
echo "=== Here document sin expansion ==="
echo ""
NAME="DevOps"
echo "--- Con comillas en el delimitador: TODO literal ---"
cat << '\''EOF'\''
Proyecto: $NAME
Version: $(cat /etc/os-release | head -1)
PID: $$
Aritmetica: $((2 + 2))
EOF
echo ""
echo "Con '\''EOF'\'', nada se expande:"
echo "  \$NAME es literal"
echo "  \$(cmd) es literal"
echo "  \$\$ es literal"
echo ""
echo "Usar '\''EOF'\'' para:"
echo "  - Generar scripts (donde \$ debe ser literal)"
echo "  - Templates con placeholders"
echo "  - Texto que contiene variables bash"
'
```

### Paso 1.3: Generar scripts con here document

```bash
docker compose exec debian-dev bash -c '
echo "=== Generar scripts ==="
echo ""
echo "--- Script con expansion parcial ---"
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
echo "Script generado:"
cat /tmp/generated.sh
echo ""
echo "--- Ejecutar ---"
chmod +x /tmp/generated.sh
/tmp/generated.sh
echo ""
echo "Nota: \\\$\\\$ y \\\$(cmd) tienen \\\\ para que NO se expandan"
echo "al generar, sino al EJECUTAR el script"
rm /tmp/generated.sh
'
```

### Paso 1.4: Crear archivos de configuracion

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear archivos de configuracion ==="
echo ""
DB_HOST="db.example.com"
DB_PORT=5432
DB_NAME="myapp"

cat << EOF > /tmp/config.ini
[database]
host = $DB_HOST
port = $DB_PORT
name = $DB_NAME

[logging]
level = info
file = /var/log/$DB_NAME.log
EOF
echo "--- Config generada ---"
cat /tmp/config.ini
echo ""
echo "--- Append con >> ---"
cat << EOF >> /tmp/config.ini

[cache]
enabled = true
ttl = 300
EOF
echo "--- Config con append ---"
cat /tmp/config.ini
rm /tmp/config.ini
'
```

---

## Parte 2 — Usos avanzados

### Objetivo

Usar here documents para mensajes de uso, asignar a variables,
y redireccionar a comandos.

### Paso 2.1: Mensajes de uso (usage)

```bash
docker compose exec debian-dev bash -c '
echo "=== Usage con here document ==="
echo ""
cat > /tmp/usage-demo.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_NAME=$(basename "$0")

usage() {
    cat << EOF
Uso: $SCRIPT_NAME [opciones] <archivo>

Descripcion:
  Procesa archivos de datos y genera reportes.

Opciones:
  -v, --verbose     Modo verbose
  -o, --output FILE Archivo de salida
  -h, --help        Mostrar esta ayuda

Ejemplos:
  $SCRIPT_NAME -v data.csv
  $SCRIPT_NAME --output report.txt *.csv

Version: 1.0.0
EOF
}

[[ "${1:-}" == "-h" || "${1:-}" == "--help" || $# -eq 0 ]] && { usage; exit 0; }
echo "Procesando: $@"
SCRIPT
chmod +x /tmp/usage-demo.sh
/tmp/usage-demo.sh --help
echo ""
echo "\$(basename \"\$0\") se expande al nombre real del script"
echo "(EOF sin comillas para que se expanda)"
rm /tmp/usage-demo.sh
'
```

### Paso 2.2: Asignar a variable

```bash
docker compose exec debian-dev bash -c '
echo "=== Here document en variable ==="
echo ""
echo "--- Con \$(cat << EOF) ---"
TEMPLATE=$(cat << '\''EOF'\''
{
    "name": "$NAME",
    "version": "$VERSION",
    "date": "$DATE"
}
EOF
)
echo "Template guardado en variable:"
echo "$TEMPLATE"
echo ""
echo "--- read -r -d con here document ---"
read -r -d "" JSON_TEMPLATE << '\''EOF'\'' || true
{
    "server": "localhost",
    "port": 8080,
    "debug": true
}
EOF
echo "JSON via read:"
echo "$JSON_TEMPLATE"
echo ""
echo "\$() con cat es la forma mas comun"
echo "read -d '\'''\'' es alternativa (necesita || true)"
'
```

### Paso 2.3: Pasar a comandos

```bash
docker compose exec debian-dev bash -c '
echo "=== Here document como stdin de comandos ==="
echo ""
echo "--- Pasar a grep ---"
grep "error" << '\''EOF'\''
info: todo bien
error: algo fallo
warning: atencion
error: otro fallo
EOF
echo ""
echo "--- Pasar a wc ---"
wc -l << '\''EOF'\''
linea 1
linea 2
linea 3
EOF
echo ""
echo "--- Pasar a while read ---"
while IFS=: read -r key value; do
    echo "  $key = $value"
done << '\''EOF'\''
host:localhost
port:8080
debug:true
EOF
'
```

---

## Parte 3 — Here strings

### Objetivo

Usar here strings (<<<) como alternativa a pipes para pasar
strings a comandos.

### Paso 3.1: <<< basico

```bash
docker compose exec debian-dev bash -c '
echo "=== Here string <<< ==="
echo ""
echo "--- Basico: pasar string como stdin ---"
grep -o "world" <<< "hello world"
echo ""
echo "--- Variables se expanden ---"
NAME="bash"
cat <<< "Hola, $NAME"
echo ""
echo "--- Comparar con pipe ---"
echo "Pipe:        echo \"hello\" | grep \"hello\""
echo "Here string: grep \"hello\" <<< \"hello\""
echo ""
echo "Here string es mas eficiente:"
echo "  - No crea subshell (como el pipe)"
echo "  - Las variables persisten despues"
'
```

### Paso 3.2: Parsing con here string

```bash
docker compose exec debian-dev bash -c '
echo "=== Parsing con here string ==="
echo ""
echo "--- Separar campos con IFS ---"
LINE="alice:30:madrid:dev"
IFS=: read -r name age city role <<< "$LINE"
echo "Nombre: $name"
echo "Edad: $age"
echo "Ciudad: $city"
echo "Rol: $role"
echo ""
echo "--- CSV ---"
CSV="producto,100,USD"
IFS=, read -r item price currency <<< "$CSV"
echo "$item cuesta $price $currency"
echo ""
echo "--- El problema del pipe ---"
echo "hello world" | read -r A B
echo "Con pipe: A=$A B=$B (vacio — subshell!)"
echo ""
read -r A B <<< "hello world"
echo "Con <<<: A=$A B=$B (correcto — mismo shell)"
echo ""
echo "--- Calculos con bc ---"
RESULT=$(bc <<< "scale=2; 100 / 3")
echo "100 / 3 = $RESULT"
'
```

### Paso 3.3: Comparacion completa

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparacion ==="
echo ""
printf "%-22s %-10s %-10s %-12s\n" "Forma" "Multilinea" "Expansion" "Subshell"
printf "%-22s %-10s %-10s %-12s\n" "---------------------" "---------" "---------" "-----------"
printf "%-22s %-10s %-10s %-12s\n" "<< EOF (sin comillas)" "Si" "Si" "No"
printf "%-22s %-10s %-10s %-12s\n" "<< '\''EOF'\'' (con comillas)" "Si" "No" "No"
printf "%-22s %-10s %-10s %-12s\n" "<<< \"string\"" "No" "Si" "No"
printf "%-22s %-10s %-10s %-12s\n" "echo | cmd (pipe)" "No" "Si" "Si"
echo ""
echo "=== Cuando usar cada uno ==="
echo ""
echo "  << EOF:        generar texto con variables expandidas"
echo "  << '\''EOF'\'':      generar scripts/templates literales"
echo "  <<<:           pasar un string corto a un comando"
echo "  echo | cmd:    evitar (here string es mejor)"
echo ""
echo "  << para multilinea, <<< para una linea"
echo "  Comillas en delimitador = literal"
echo "  Sin comillas = expansion"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `<< EOF` (sin comillas) expande variables y comandos dentro
   del here document. `<< 'EOF'` (con comillas) es completamente
   literal — nada se expande.

2. Para generar scripts dentro de scripts, usar `<< 'EOF'` para
   que `$$`, `$(cmd)`, y `$VAR` se mantengan literales y se
   expandan al ejecutar, no al generar.

3. `<< EOF > archivo` crea archivos con contenido. Es la forma
   idiomatica de crear configuraciones, scripts, y templates
   desde bash.

4. `<<<` (here string) pasa un string como stdin sin crear
   subshell. `read -r A B <<< "hello world"` preserva las
   variables, a diferencia de `echo | read` (subshell).

5. `IFS=: read -r ... <<< "$LINE"` es el patron para parsear
   strings delimitados sin subshell. Funciona con cualquier
   delimitador (`:`, `,`, `|`, etc.).

6. `$(cat << 'EOF' ... EOF)` captura un here document en una
   variable. Util para templates JSON, SQL, o configuraciones
   que se procesan mas tarde.
