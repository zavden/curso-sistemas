# T04 — Debugging

## set -x — Trace de ejecución

`set -x` imprime cada comando **antes de ejecutarlo**, mostrando las
expansiones ya resueltas:

```bash
#!/usr/bin/env bash
set -x

NAME="World"
echo "Hello, $NAME"
FILE_COUNT=$(ls /etc/*.conf 2>/dev/null | wc -l)
echo "Configs: $FILE_COUNT"
```

```
# Salida (stderr):
+ NAME=World
+ echo 'Hello, World'
Hello, World
++ ls /etc/adduser.conf /etc/ca-certificates.conf ...
++ wc -l
+ FILE_COUNT=12
+ echo 'Configs: 12'
Configs: 12
```

```bash
# El prefijo + indica el nivel de anidamiento:
# +   comando directo
# ++  command substitution (nivel 1)
# +++ command substitution anidada (nivel 2)
```

### Activar/desactivar selectivamente

```bash
#!/usr/bin/env bash

echo "Esta parte no se traza"

set -x
echo "Esta parte SÍ se traza"
RESULT=$((2 + 3))
set +x    # desactivar tracing

echo "Esta parte ya no se traza"
```

```bash
# Activar solo para una sección problemática:
problematic_function() {
    set -x
    # ... código que queremos depurar ...
    set +x
}
```

### Desde la línea de comandos

```bash
# Ejecutar un script con tracing sin modificarlo:
bash -x script.sh

# Combinar con otros flags:
bash -xeuo pipefail script.sh
```

## PS4 — Personalizar el prompt de trace

`PS4` define el prefijo que `set -x` muestra. Por defecto es `"+ "`:

```bash
# Agregar nombre de archivo, función y número de línea:
export PS4='+ ${BASH_SOURCE[0]}:${FUNCNAME[0]:-main}:${LINENO}: '
set -x

echo "hello"
# + script.sh:main:5: echo hello
```

```bash
# PS4 con timestamp (para medir tiempos):
export PS4='+ $(date "+%H:%M:%S.%N") ${BASH_SOURCE[0]}:${LINENO}: '
set -x

sleep 1
echo "done"
# + 14:30:01.123456 script.sh:5: sleep 1
# + 14:30:02.125000 script.sh:6: echo done
```

```bash
# PS4 compacto pero informativo:
export PS4='+[${LINENO}] '
set -x
echo "hello"
# +[3] echo hello

# Con colores (si la terminal los soporta):
export PS4=$'\033[1;34m+[${LINENO}]\033[0m '
```

### BASH_XTRACEFD — Redirigir trace a un archivo

```bash
# Por defecto, set -x imprime en stderr (fd 2)
# Esto se mezcla con los mensajes de error del script

# Redirigir trace a un archivo separado:
exec 3>/tmp/trace.log
BASH_XTRACEFD=3
set -x

echo "hello"        # stdout
echo "error" >&2    # stderr (limpio, sin trazas)
# Las trazas van a /tmp/trace.log

set +x
exec 3>&-    # cerrar el fd
```

## bash -n — Verificación de sintaxis

`bash -n` parsea el script sin ejecutarlo, detectando errores de sintaxis:

```bash
# Verificar sintaxis:
bash -n script.sh
# Si no hay errores, no imprime nada (exit 0)
# Si hay errores, muestra la línea y el error (exit != 0)

# Ejemplo con error:
cat << 'EOF' > /tmp/bad.sh
#!/bin/bash
if [[ -f /etc/passwd ]]; then
    echo "ok"
# Falta fi
EOF

bash -n /tmp/bad.sh
# /tmp/bad.sh: line 5: syntax error: unexpected end of file
```

### Limitaciones de bash -n

```bash
# bash -n solo detecta errores de SINTAXIS:
# ✓ if sin fi
# ✓ comillas sin cerrar
# ✓ paréntesis sin cerrar
# ✓ done sin do

# bash -n NO detecta:
# ✗ Variables con typos ($VARAIBLE en lugar de $VARIABLE)
# ✗ Comandos inexistentes
# ✗ Errores lógicos
# ✗ Problemas de quoting ($VAR sin comillas)
# ✗ Errores en eval o source dinámico

# Para detección más profunda, usar shellcheck
```

## ShellCheck — Análisis estático

ShellCheck es un linter que detecta bugs, malas prácticas y problemas de
portabilidad en scripts bash:

```bash
# Instalar:
# Debian/Ubuntu:
apt install shellcheck

# RHEL/Fedora:
dnf install ShellCheck

# Uso básico:
shellcheck script.sh
```

### Qué detecta ShellCheck

```bash
# SC2086 — Variable sin comillas (word splitting):
# echo $VAR  →  echo "$VAR"
FILE="mi archivo.txt"
rm $FILE                  # ShellCheck: SC2086
rm "$FILE"                # correcto

# SC2046 — Command substitution sin comillas:
rm $(find . -name "*.tmp")    # SC2046
# Sugerencia: usar while read o comillas

# SC2006 — Backticks en lugar de $():
DATE=`date`               # SC2006
DATE=$(date)               # correcto

# SC2004 — $ innecesario en aritmética:
echo $(($X + 1))          # SC2004
echo $((X + 1))           # correcto (no necesita $ dentro de (( )))

# SC2034 — Variable asignada pero nunca usada:
UNUSED="value"             # SC2034

# SC2155 — declare y asignación en la misma línea (enmascara exit code):
local VAR=$(command)       # SC2155
local VAR                  # correcto
VAR=$(command)             #

# SC2164 — cd sin manejo de error:
cd /some/dir               # SC2164
cd /some/dir || exit 1     # correcto

# SC2162 — read sin -r:
read LINE                  # SC2162
read -r LINE               # correcto (no interpreta backslashes)
```

### Directivas inline

```bash
# Deshabilitar un warning específico en una línea:
# shellcheck disable=SC2086
echo $VAR

# Deshabilitar para todo el archivo (al inicio):
# shellcheck disable=SC2086,SC2034

# Deshabilitar para un bloque:
# shellcheck disable=SC2086
for f in $FILES; do
    echo "$f"
done

# Especificar el shell (si no hay shebang):
# shellcheck shell=bash
```

### Integración

```bash
# En CI/CD:
shellcheck --severity=error scripts/*.sh || exit 1
# --severity: error, warning, info, style

# Formato para editores:
shellcheck --format=gcc script.sh       # formato gcc (para vim/emacs)
shellcheck --format=json script.sh      # JSON

# Con diff (solo verificar archivos cambiados):
git diff --name-only HEAD~1 -- '*.sh' | xargs shellcheck

# En pre-commit hook:
# .git/hooks/pre-commit:
#!/bin/bash
git diff --cached --name-only --diff-filter=ACM -- '*.sh' | xargs -r shellcheck
```

## Técnicas de debugging

### echo/printf para inspección

```bash
# La técnica más básica — prints estratégicos:
debug() {
    [[ "${DEBUG:-0}" == "1" ]] && echo "DEBUG: $*" >&2
}

debug "variable X=$X"
debug "entrando a función process_file($1)"

# Ejecutar con: DEBUG=1 ./script.sh
```

### trap DEBUG para tracing condicional

```bash
# Ejecutar una función antes de cada comando:
trace() {
    echo "[TRACE] $BASH_COMMAND" >&2
}
trap trace DEBUG

# Solo cuando DEBUG está activo:
if [[ "${DEBUG:-0}" == "1" ]]; then
    trap 'echo "[${FUNCNAME[0]:-main}:${LINENO}] $BASH_COMMAND" >&2' DEBUG
fi
```

### trap ERR para diagnóstico de errores

```bash
#!/usr/bin/env bash
set -euo pipefail

on_error() {
    echo "ERROR en ${BASH_SOURCE[0]}:${LINENO}" >&2
    echo "  Comando: $BASH_COMMAND" >&2
    echo "  Exit code: $?" >&2

    # Stack trace:
    local i=0
    echo "  Stack:" >&2
    while caller $i 2>/dev/null; do
        ((i++))
    done | while read -r line func file; do
        echo "    $func() en $file:$line" >&2
    done
}
trap on_error ERR
```

### Variables de diagnóstico de bash

```bash
# LINENO — número de línea actual:
echo "Estoy en la línea $LINENO"

# FUNCNAME — stack de funciones:
inner() { echo "${FUNCNAME[@]}"; }
outer() { inner; }
outer
# inner outer main

# BASH_SOURCE — stack de archivos:
echo "${BASH_SOURCE[0]}"    # archivo actual
echo "${BASH_SOURCE[@]}"    # todos los archivos en el stack

# BASH_COMMAND — comando actual:
trap 'echo "Ejecutando: $BASH_COMMAND"' DEBUG

# caller — información del llamador:
show_caller() {
    local line func file
    read -r line func file <<< "$(caller 0)"
    echo "Llamado desde $func() en $file:$line"
}

# BASH_LINENO — array de números de línea del call stack
# BASH_ARGC — array de conteos de argumentos del call stack
```

### Debugging interactivo

```bash
# Parar en un punto específico (breakpoint manual):
echo "Estado: X=$X, Y=$Y"
echo "Presiona Enter para continuar..."
read -r

# O con un shell interactivo en el punto:
echo "Entrando en modo debug (exit para continuar)"
bash    # abre un subshell interactivo
echo "Continuando..."

# Ejecutar paso a paso con trap:
trap 'read -p "[$LINENO] $BASH_COMMAND → "' DEBUG
# Presionar Enter para ejecutar cada línea
```

## Patrón completo de script debuggeable

```bash
#!/usr/bin/env bash
set -euo pipefail

# --- Debug infrastructure ---
readonly DEBUG="${DEBUG:-0}"
readonly TRACE="${TRACE:-0}"

if [[ "$TRACE" == "1" ]]; then
    export PS4='+[${BASH_SOURCE[0]##*/}:${LINENO}] '
    set -x
fi

log() {
    local level="$1"; shift
    echo "[$(date '+%H:%M:%S')] [$level] $*" >&2
}
debug() { [[ "$DEBUG" == "1" ]] && log DEBUG "$@" || true; }
info()  { log INFO "$@"; }
error() { log ERROR "$@"; }
die()   { error "$@"; exit 1; }

on_error() {
    error "Fallo en ${BASH_SOURCE[1]:-$0}:${LINENO}"
    error "  Comando: $BASH_COMMAND"
}
trap on_error ERR

# --- Logic ---
main() {
    debug "Argumentos: $*"
    info "Inicio"

    # ... lógica del script ...

    info "Fin"
}

main "$@"

# Uso:
# ./script.sh                  # normal
# DEBUG=1 ./script.sh          # con mensajes debug
# TRACE=1 ./script.sh          # con set -x
# TRACE=1 DEBUG=1 ./script.sh  # ambos
```

---

## Ejercicios

### Ejercicio 1 — set -x y PS4

```bash
#!/usr/bin/env bash
export PS4='+[${LINENO}] '
set -x

NAME="World"
GREETING="Hello, $NAME"
echo "$GREETING"
RESULT=$((2 + 3))
echo "Result: $RESULT"

set +x
echo "Tracing desactivado"

# Observar la salida y entender qué muestra cada línea de trace
```

### Ejercicio 2 — bash -n y shellcheck

```bash
# Crear un script con errores intencionales:
cat << 'SCRIPT' > /tmp/buggy.sh
#!/bin/bash
FILE=mi archivo.txt
if [ -f $FILE ]; then
    echo "existe"
    DATE=`date`
    echo $DATE
SCRIPT

# 1. Verificar sintaxis:
bash -n /tmp/buggy.sh

# 2. Si tienes shellcheck instalado:
shellcheck /tmp/buggy.sh

# ¿Cuántos problemas encuentra cada herramienta?
rm /tmp/buggy.sh
```

### Ejercicio 3 — Debug condicional

```bash
#!/usr/bin/env bash
set -euo pipefail

DEBUG="${DEBUG:-0}"

debug() { [[ "$DEBUG" == "1" ]] && echo "[DEBUG] $*" >&2 || true; }

process() {
    local file="$1"
    debug "Procesando: $file"
    local lines
    lines=$(wc -l < "$file")
    debug "Líneas: $lines"
    echo "$file: $lines líneas"
}

for f in /etc/hostname /etc/os-release; do
    process "$f"
done

# Ejecutar:
# ./script.sh               → solo la salida normal
# DEBUG=1 ./script.sh       → con mensajes de debug
```
