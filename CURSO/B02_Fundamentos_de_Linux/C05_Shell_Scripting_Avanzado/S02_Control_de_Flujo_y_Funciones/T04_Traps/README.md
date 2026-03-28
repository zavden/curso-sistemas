# T04 — Traps

## Qué es trap

`trap` permite ejecutar un comando cuando el shell recibe una **señal** o
cuando ocurre un **evento** del shell:

```bash
trap 'COMANDO' SEÑAL [SEÑAL...]

# Ejemplo básico:
trap 'echo "Ctrl+C interceptado"' INT
# Ahora Ctrl+C no mata el script — ejecuta el echo
```

```bash
# Las señales más usadas con trap:
trap 'cmd' EXIT       # al salir del script (por cualquier motivo)
trap 'cmd' INT        # al recibir SIGINT (Ctrl+C)
trap 'cmd' TERM       # al recibir SIGTERM (kill sin argumentos)
trap 'cmd' HUP        # al recibir SIGHUP (terminal cerrada)
trap 'cmd' ERR        # cuando un comando falla (con set -e activo)
trap 'cmd' DEBUG      # antes de CADA comando (para debugging)
trap 'cmd' RETURN     # al retornar de una función o source

# Señales que NO se pueden capturar:
# SIGKILL (9) — kill -9, siempre mata
# SIGSTOP (19) — siempre suspende
```

## trap EXIT — Cleanup al salir

Este es el uso más importante de `trap`. Garantiza que el código de limpieza se
ejecute sin importar cómo termine el script:

```bash
#!/usr/bin/env bash
set -euo pipefail

TMPFILE=""

cleanup() {
    echo "Limpiando..." >&2
    [[ -n "$TMPFILE" && -f "$TMPFILE" ]] && rm -f "$TMPFILE"
}
trap cleanup EXIT

TMPFILE=$(mktemp)
echo "Trabajando con $TMPFILE"
# ... hacer trabajo ...
# Al salir (exit 0, exit 1, set -e, Ctrl+C) → cleanup se ejecuta
```

### EXIT se ejecuta siempre

```bash
# EXIT se dispara en TODOS estos casos:
# 1. El script llega al final (exit implícito)
# 2. exit N explícito
# 3. set -e mata el script por un error
# 4. Ctrl+C (SIGINT)
# 5. SIGTERM (kill)
#
# NO se dispara con:
# - kill -9 (SIGKILL) — no se puede interceptar
# - El sistema se apaga abruptamente (power loss)
```

### Cleanup avanzado con directorio temporal

```bash
#!/usr/bin/env bash
set -euo pipefail

TMPDIR=""

cleanup() {
    local exit_code=$?    # capturar ANTES de que otro comando lo cambie
    if [[ -n "$TMPDIR" ]]; then
        rm -rf "$TMPDIR"
        echo "Directorio temporal eliminado: $TMPDIR" >&2
    fi
    exit "$exit_code"     # preservar el exit code original
}
trap cleanup EXIT

TMPDIR=$(mktemp -d)
echo "Directorio temporal: $TMPDIR"

# Crear archivos de trabajo en TMPDIR
cp /etc/passwd "$TMPDIR/data.txt"
# ... procesar ...
# Al salir, TMPDIR se elimina automáticamente
```

### Múltiples recursos

```bash
TMPFILE=""
LOCKFILE=""
PID_BG=""

cleanup() {
    # Eliminar archivos temporales
    [[ -n "$TMPFILE" ]] && rm -f "$TMPFILE"

    # Liberar lock
    [[ -n "$LOCKFILE" ]] && rm -f "$LOCKFILE"

    # Matar proceso en background
    [[ -n "$PID_BG" ]] && kill "$PID_BG" 2>/dev/null || true
}
trap cleanup EXIT
```

## trap INT y TERM — Manejo de interrupciones

```bash
#!/usr/bin/env bash
set -euo pipefail

# Manejar Ctrl+C de forma grácil:
trap 'echo -e "\nInterrumpido por el usuario"; exit 130' INT
# 130 = 128 + 2 (SIGINT) — convención para "terminado por señal"

# Manejar SIGTERM (lo que envía kill, systemd, Docker stop):
trap 'echo "Terminando..."; exit 143' TERM
# 143 = 128 + 15 (SIGTERM)

echo "PID: $$, ejecutando..."
while true; do
    sleep 1
done
```

### Patrón profesional: separar cleanup de señales

```bash
#!/usr/bin/env bash
set -euo pipefail

RUNNING=true
TMPDIR=""

cleanup() {
    [[ -n "$TMPDIR" ]] && rm -rf "$TMPDIR"
}

handle_signal() {
    echo -e "\nSeñal recibida, terminando..." >&2
    RUNNING=false
    # No llamar exit aquí — dejar que el loop termine limpiamente
}

trap cleanup EXIT           # cleanup siempre al salir
trap handle_signal INT TERM # señales ponen RUNNING=false

TMPDIR=$(mktemp -d)

while $RUNNING; do
    echo "Trabajando..."
    sleep 2
done

echo "Terminado limpiamente"
# EXIT trap ejecuta cleanup automáticamente
```

## trap ERR — Capturar errores

`ERR` se dispara cuando un comando falla (exit code != 0), similar a `set -e`
pero permite ejecutar código antes de salir:

```bash
#!/usr/bin/env bash
set -euo pipefail

on_error() {
    local exit_code=$?
    local line_no=$1
    echo "ERROR en línea $line_no: comando falló con código $exit_code" >&2
}
trap 'on_error $LINENO' ERR

echo "Paso 1: OK"
echo "Paso 2: OK"
false                      # falla aquí
echo "Paso 3: nunca llega"

# Salida:
# Paso 1: OK
# Paso 2: OK
# ERROR en línea 10: comando falló con código 1
```

### ERR con stack trace

```bash
#!/usr/bin/env bash
set -euo pipefail

stacktrace() {
    local exit_code=$?
    echo "--- Stack trace ---" >&2
    echo "Error: exit code $exit_code" >&2

    local i=0
    while caller $i 2>/dev/null; do
        ((i++))
    done | while read -r LINE FUNC FILE; do
        echo "  en $FUNC() ($FILE:$LINE)" >&2
    done

    echo "---" >&2
}
trap stacktrace ERR

inner() {
    false    # error aquí
}

outer() {
    inner
}

outer
# --- Stack trace ---
# Error: exit code 1
#   en inner() (script.sh:16)
#   en outer() (script.sh:20)
#   en main (script.sh:22)
# ---
```

### Diferencia entre ERR y EXIT

```bash
# ERR se dispara cuando un comando falla
# EXIT se dispara SIEMPRE al salir del script

# Usar ERR para logging/diagnóstico de errores
# Usar EXIT para cleanup de recursos

trap 'echo "Error detectado" >&2' ERR
trap 'echo "Script terminando" >&2' EXIT

# Si un comando falla:
# 1. ERR se ejecuta primero
# 2. Si set -e está activo, el script sale
# 3. EXIT se ejecuta al salir
```

## trap DEBUG — Debugging

`DEBUG` se ejecuta **antes de cada comando**:

```bash
#!/usr/bin/env bash

# Tracing manual (alternativa a set -x):
trap 'echo "+ $BASH_COMMAND" >&2' DEBUG

A=1
B=2
C=$((A + B))
echo "$C"

# Salida en stderr:
# + A=1
# + B=2
# + C=3
# + echo 3
# Salida en stdout:
# 3
```

```bash
# Uso práctico: logging condicional
DEBUG=${DEBUG:-0}

if [[ $DEBUG -eq 1 ]]; then
    trap 'echo "[DEBUG] $BASH_COMMAND" >&2' DEBUG
fi

# Ejecutar con: DEBUG=1 ./script.sh
```

## trap RETURN

Se ejecuta cada vez que una función retorna o un `source` termina:

```bash
trace_returns() {
    echo "Función retornó con código $?" >&2
}
trap trace_returns RETURN

my_function() {
    echo "dentro de la función"
    return 0
}

my_function
# dentro de la función
# Función retornó con código 0
```

## Operaciones con trap

### Listar traps activos

```bash
# Ver todos los traps configurados:
trap -p
# trap -- 'cleanup' EXIT
# trap -- 'handle_signal' INT TERM

# Ver un trap específico:
trap -p EXIT
# trap -- 'cleanup' EXIT
```

### Resetear un trap

```bash
# Restaurar comportamiento por defecto:
trap - INT          # Ctrl+C vuelve a matar el script
trap - EXIT         # no ejecutar nada al salir

# Ignorar una señal:
trap '' INT         # Ctrl+C se ignora completamente
trap '' TERM        # kill (sin -9) se ignora
# CUIDADO: '' (string vacío) IGNORA la señal, no la resetea
# Usar - para resetear

# Diferencia:
trap '' INT     # IGNORAR — Ctrl+C no hace nada
trap - INT      # RESETEAR — Ctrl+C mata el script (default)
trap 'cmd' INT  # MANEJAR — Ctrl+C ejecuta cmd
```

### Redefinir traps

```bash
# Cada nuevo trap para la misma señal REEMPLAZA el anterior:
trap 'echo "trap 1"' EXIT
trap 'echo "trap 2"' EXIT    # reemplaza el anterior
# Al salir solo se ejecuta "trap 2"

# Para acumular acciones, definir una función que crezca:
# (o usar una sola función que haga todo)
```

## Traps y subshells

```bash
# Los traps NO se heredan en subshells:
trap 'echo "parent trap"' EXIT

(
    # Este subshell NO tiene el trap EXIT del padre
    echo "subshell"
)
# "subshell" — sin "parent trap"

# Los traps SÍ se heredan con source:
# source script.sh  → hereda los traps del shell actual

# Command substitution crea un subshell — sin traps:
RESULT=$(
    # aquí no hay traps del padre
    echo "value"
)
```

## Patrones completos

### Script con cleanup completo

```bash
#!/usr/bin/env bash
set -euo pipefail

# --- Infraestructura ---
readonly SCRIPT_NAME=$(basename "$0")
TMPDIR=""

die()  { echo "$SCRIPT_NAME: ERROR: $*" >&2; exit 1; }
log()  { echo "$SCRIPT_NAME: $*" >&2; }

cleanup() {
    local exit_code=$?
    [[ -n "$TMPDIR" ]] && rm -rf "$TMPDIR"
    (( exit_code != 0 )) && log "Falló con código $exit_code"
    return "$exit_code"
}

trap cleanup EXIT
trap 'log "Interrumpido"; exit 130' INT
trap 'log "Terminado"; exit 143' TERM

# --- Lógica ---
main() {
    TMPDIR=$(mktemp -d)
    log "Inicio (tmpdir=$TMPDIR)"

    # ... trabajo real ...

    log "Fin"
}

main "$@"
```

### Servidor/daemon con shutdown grácil

```bash
#!/usr/bin/env bash
set -euo pipefail

RUNNING=true
PIDFILE="/tmp/myservice.pid"

shutdown() {
    echo "Shutdown iniciado..."
    RUNNING=false
    rm -f "$PIDFILE"
    # Esperar a que trabajos en background terminen
    wait
    echo "Shutdown completo"
}

trap shutdown INT TERM
trap 'rm -f "$PIDFILE"' EXIT

echo $$ > "$PIDFILE"
echo "Servicio iniciado (PID $$)"

while $RUNNING; do
    # Simular trabajo
    echo "$(date): procesando..."
    sleep 5 &
    wait $! 2>/dev/null || true
    # wait en lugar de sleep directo permite que las señales se procesen
    # durante la espera (sleep bloquea las señales en algunos shells)
done
```

### Lock file con cleanup

```bash
#!/usr/bin/env bash
set -euo pipefail

LOCKFILE="/tmp/myscript.lock"

acquire_lock() {
    if ! (set -o noclobber; echo $$ > "$LOCKFILE") 2>/dev/null; then
        local pid
        pid=$(cat "$LOCKFILE" 2>/dev/null || echo "?")
        die "Otra instancia está corriendo (PID $pid)"
    fi
    trap 'rm -f "$LOCKFILE"' EXIT
}

die() { echo "ERROR: $*" >&2; exit 1; }

acquire_lock
echo "Lock adquirido, ejecutando..."
sleep 10    # simular trabajo
echo "Terminado"
# EXIT trap elimina el lockfile
```

---

## Ejercicios

### Ejercicio 1 — Cleanup básico

```bash
#!/usr/bin/env bash
set -euo pipefail

TMPFILE=""

cleanup() {
    echo "Limpiando..." >&2
    [[ -n "$TMPFILE" ]] && rm -f "$TMPFILE"
}
trap cleanup EXIT

TMPFILE=$(mktemp)
echo "Archivo: $TMPFILE"
echo "datos de prueba" > "$TMPFILE"

# Verificar que el archivo existe:
ls -la "$TMPFILE"

# Salir (cleanup se ejecuta automáticamente):
exit 0
# Verificar que el archivo fue eliminado:
# ls -la "$TMPFILE"  → No such file or directory
```

### Ejercicio 2 — Interceptar Ctrl+C

```bash
#!/usr/bin/env bash

trap 'echo -e "\nCtrl+C detectado, ¿seguro que quieres salir? (y/n)"; read -r REPLY; [[ "$REPLY" == "y" ]] && exit 0' INT

echo "Presiona Ctrl+C para probar (o espera 30s)"
for i in {1..30}; do
    echo "Segundo $i..."
    sleep 1
done
```

### Ejercicio 3 — ERR con información de diagnóstico

```bash
#!/usr/bin/env bash
set -euo pipefail

on_error() {
    echo "FALLO en línea $1: '$BASH_COMMAND' retornó $?" >&2
}
trap 'on_error $LINENO' ERR

echo "Paso 1: listando /etc"
ls /etc > /dev/null

echo "Paso 2: listando directorio inexistente"
ls /directorio/que/no/existe > /dev/null

echo "Paso 3: nunca se ejecuta"
```
