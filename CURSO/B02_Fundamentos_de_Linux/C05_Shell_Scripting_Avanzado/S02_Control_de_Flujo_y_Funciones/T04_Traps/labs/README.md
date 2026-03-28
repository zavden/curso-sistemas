# Lab — Traps

## Objetivo

Dominar traps en bash: trap EXIT para cleanup garantizado, trap
INT/TERM para manejo de senales, trap ERR para diagnostico con
LINENO y stack trace, trap DEBUG para tracing, gestion de traps
(trap -p, trap '', trap -), herencia en subshells, y el patron
de lockfile.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — trap EXIT

### Objetivo

Usar trap EXIT para garantizar limpieza de recursos temporales
sin importar como termine el script.

### Paso 1.1: Cleanup basico con EXIT

```bash
docker compose exec debian-dev bash -c '
echo "=== trap EXIT ==="
echo ""
echo "trap EXIT se ejecuta SIEMPRE al salir del script:"
echo "  - exit normal (exit 0)"
echo "  - exit por error (exit 1)"
echo "  - exit por set -e"
echo "  - SIGTERM (kill)"
echo "  NO se ejecuta con SIGKILL (kill -9)"
echo ""
cat > /tmp/trap-exit.sh << '\''SCRIPT'\''
#!/bin/bash
TMPDIR=$(mktemp -d)
TMPFILE=$(mktemp)

cleanup() {
    echo "  cleanup: eliminando $TMPDIR y $TMPFILE"
    rm -rf "$TMPDIR" "$TMPFILE"
}
trap cleanup EXIT

echo "Archivos temporales creados:"
echo "  Dir:  $TMPDIR"
echo "  File: $TMPFILE"
echo "Trabajando..."

# Simular trabajo
echo "datos" > "$TMPFILE"
touch "$TMPDIR/output.txt"

echo "Terminando (cleanup se ejecuta automaticamente)"
SCRIPT
chmod +x /tmp/trap-exit.sh
/tmp/trap-exit.sh
echo ""
echo "El cleanup se ejecuto al salir"
'
```

### Paso 1.2: EXIT se ejecuta incluso con errores

```bash
docker compose exec debian-dev bash -c '
echo "=== EXIT con errores ==="
echo ""
cat > /tmp/trap-error-exit.sh << '\''SCRIPT'\''
#!/bin/bash
set -e

cleanup() {
    echo "  cleanup ejecutado (exit code: $?)"
}
trap cleanup EXIT

echo "Paso 1: OK"
echo "Paso 2: OK"
echo "Paso 3: forzar error..."
false
echo "Paso 4: nunca llega"
SCRIPT
chmod +x /tmp/trap-error-exit.sh
/tmp/trap-error-exit.sh 2>&1 || true
echo ""
echo "Aunque set -e termino el script en false,"
echo "cleanup se ejecuto de todas formas"
'
```

### Paso 1.3: Patron de archivos temporales

```bash
docker compose exec debian-dev bash -c '
echo "=== Patron profesional: temp files ==="
echo ""
cat > /tmp/tempfile-pattern.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

# Crear directorio temporal al inicio
readonly WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/myapp.XXXXXXXXXX")

# Registrar cleanup INMEDIATAMENTE despues de crear
cleanup() {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

# Ahora usar WORK_DIR libremente
echo "Directorio de trabajo: $WORK_DIR"
echo "datos de entrada" > "$WORK_DIR/input.txt"
echo "resultado" > "$WORK_DIR/output.txt"

echo "Archivos en WORK_DIR:"
ls -la "$WORK_DIR/"

echo ""
echo "Al terminar, cleanup elimina todo automaticamente"
SCRIPT
chmod +x /tmp/tempfile-pattern.sh
/tmp/tempfile-pattern.sh
echo ""
echo "Patron: mktemp + trap cleanup EXIT"
echo "  1. Crear directorio temporal con mktemp -d"
echo "  2. Registrar trap EXIT inmediatamente"
echo "  3. Usar el directorio libremente"
echo "  4. Cleanup se ejecuta automaticamente"
'
```

---

## Parte 2 — trap INT/TERM/ERR

### Objetivo

Manejar senales de interrupcion y errores con diagnostico.

### Paso 2.1: trap INT y TERM

```bash
docker compose exec debian-dev bash -c '
echo "=== trap INT y TERM ==="
echo ""
echo "--- Senales comunes ---"
printf "%-10s %-8s %s\n" "Senal" "Numero" "Cuando se recibe"
printf "%-10s %-8s %s\n" "---------" "-------" "----------------------------"
printf "%-10s %-8s %s\n" "INT" "2" "Ctrl+C"
printf "%-10s %-8s %s\n" "TERM" "15" "kill PID (default)"
printf "%-10s %-8s %s\n" "HUP" "1" "Terminal cerrada"
printf "%-10s %-8s %s\n" "KILL" "9" "kill -9 (NO se puede capturar)"
echo ""
cat > /tmp/trap-signal.sh << '\''SCRIPT'\''
#!/bin/bash
RUNNING=true

on_signal() {
    local sig="$1"
    echo ""
    echo "  Recibida senal: $sig"
    echo "  Deteniendo de forma limpia..."
    RUNNING=false
}
trap "on_signal INT" INT
trap "on_signal TERM" TERM

echo "Script corriendo (PID: $$)"
echo "Enviando SIGTERM a si mismo en 1 segundo..."
(sleep 1; kill -TERM $$) &

while $RUNNING; do
    echo -n "."
    sleep 0.3
done

echo "  Script terminado limpiamente"
SCRIPT
chmod +x /tmp/trap-signal.sh
/tmp/trap-signal.sh
echo ""
echo "El trap convirtio una terminacion abrupta en un shutdown limpio"
'
```

### Paso 2.2: trap ERR con diagnostico

```bash
docker compose exec debian-dev bash -c '
echo "=== trap ERR con diagnostico ==="
echo ""
cat > /tmp/trap-err-diag.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

on_error() {
    local exit_code=$?
    echo "" >&2
    echo "=== ERROR ===" >&2
    echo "  Archivo:  ${BASH_SOURCE[1]:-$0}" >&2
    echo "  Linea:    $LINENO" >&2
    echo "  Comando:  $BASH_COMMAND" >&2
    echo "  Exit:     $exit_code" >&2
    echo "" >&2

    # Stack trace
    echo "  Stack trace:" >&2
    local i=0
    while caller $i 2>/dev/null; do
        ((i++))
    done | while read -r line func file; do
        echo "    $func() en $file:$line" >&2
    done
    echo "=============" >&2
}
trap on_error ERR

step_one() {
    echo "  step_one: OK"
}

step_two() {
    echo "  step_two: ejecutando comando que falla..."
    grep -q "string_inexistente" /etc/hostname
}

step_three() {
    echo "  step_three: nunca llega"
}

echo "Ejecutando pasos..."
step_one
step_two
step_three
SCRIPT
chmod +x /tmp/trap-err-diag.sh
/tmp/trap-err-diag.sh 2>&1 || true
echo ""
echo "trap ERR muestra exactamente donde y por que fallo"
echo "caller proporciona el stack trace completo"
'
```

### Paso 2.3: Combinar EXIT + ERR

```bash
docker compose exec debian-dev bash -c '
echo "=== Combinar EXIT + ERR ==="
echo ""
cat > /tmp/combined-traps.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

TMPFILE=$(mktemp)
EXIT_CODE=0

on_error() {
    EXIT_CODE=$?
    echo "ERROR en linea $LINENO: $BASH_COMMAND (exit: $EXIT_CODE)" >&2
}

cleanup() {
    rm -f "$TMPFILE"
    if (( EXIT_CODE != 0 )); then
        echo "Script fallo (exit: $EXIT_CODE)" >&2
    else
        echo "Script completado exitosamente"
    fi
}

trap on_error ERR
trap cleanup EXIT

echo "Archivo temporal: $TMPFILE"
echo "datos" > "$TMPFILE"
echo "Trabajo completado"
SCRIPT
chmod +x /tmp/combined-traps.sh
echo "--- Ejecucion exitosa ---"
/tmp/combined-traps.sh
echo ""
echo "EXIT y ERR se pueden combinar:"
echo "  ERR: diagnostico del error"
echo "  EXIT: cleanup garantizado (siempre se ejecuta)"
'
```

---

## Parte 3 — trap DEBUG y avanzado

### Objetivo

Usar trap DEBUG para tracing, entender la herencia en subshells,
y aplicar el patron de lockfile.

### Paso 3.1: trap DEBUG

```bash
docker compose exec debian-dev bash -c '
echo "=== trap DEBUG ==="
echo ""
echo "trap DEBUG se ejecuta ANTES de cada comando"
echo ""
cat > /tmp/trap-debug.sh << '\''SCRIPT'\''
#!/bin/bash
trace() {
    echo "  [TRACE ${FUNCNAME[1]:-main}:${BASH_LINENO[0]}] $BASH_COMMAND" >&2
}
trap trace DEBUG

A=10
B=20
SUM=$((A + B))
echo "Resultado: $SUM"

# Desactivar
trap - DEBUG
echo "Tracing desactivado"
SCRIPT
chmod +x /tmp/trap-debug.sh
/tmp/trap-debug.sh
echo ""
echo "Cada linea de [TRACE] muestra el comando ANTES de ejecutarse"
echo "util para debugging sin modificar el codigo"
'
```

### Paso 3.2: Gestion de traps

```bash
docker compose exec debian-dev bash -c '
echo "=== Gestion de traps ==="
echo ""
echo "--- trap -p: ver traps activos ---"
trap "echo cleanup" EXIT
trap "echo interrupted" INT
echo "Traps activos:"
trap -p
echo ""
echo "--- trap - SIGNAL: eliminar un trap ---"
trap - INT
echo "Despues de trap - INT:"
trap -p
echo ""
echo "--- trap '\'''\'' SIGNAL: ignorar la senal ---"
echo "trap '\'''\'' INT → Ctrl+C se ignora completamente"
echo "trap - INT → restaurar comportamiento default"
echo ""
echo "--- Reemplazar un trap ---"
trap "echo primer_cleanup" EXIT
trap "echo segundo_cleanup" EXIT
echo "Solo se ejecuta el ultimo trap registrado:"
# Limpiar para no afectar al shell
trap - EXIT
'
```

### Paso 3.3: Herencia en subshells

Antes de ejecutar, predice: un trap definido en el shell padre
se hereda en un subshell ()?

```bash
docker compose exec debian-dev bash -c '
echo "=== Traps y subshells ==="
echo ""
trap "echo parent_exit" EXIT

echo "--- Subshell () ---"
(
    echo "  En subshell: los traps del padre se resetean"
    trap -p EXIT
    echo "  (vacio — el trap EXIT del padre NO se hereda)"
)

echo ""
echo "--- Command substitution ---"
RESULT=$(
    trap -p EXIT
    echo "sin_trap"
)
echo "En \$(): $RESULT"
echo ""
echo "Los subshells NO heredan traps"
echo "Cada subshell empieza con traps limpios"
echo "Excepto trap '\'''\'' (ignorar senal) — ESO si se hereda"
trap - EXIT
'
```

### Paso 3.4: Patron lockfile

```bash
docker compose exec debian-dev bash -c '
echo "=== Patron lockfile ==="
echo ""
cat > /tmp/lockfile-demo.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

LOCKFILE="/tmp/myapp.lock"

cleanup() {
    rm -f "$LOCKFILE"
}

# Verificar si ya hay una instancia corriendo
if [[ -f "$LOCKFILE" ]]; then
    OTHER_PID=$(cat "$LOCKFILE")
    if kill -0 "$OTHER_PID" 2>/dev/null; then
        echo "ERROR: otra instancia corriendo (PID $OTHER_PID)" >&2
        exit 1
    else
        echo "Stale lockfile encontrado, eliminando..."
        rm -f "$LOCKFILE"
    fi
fi

# Crear lockfile y registrar cleanup
echo $$ > "$LOCKFILE"
trap cleanup EXIT

echo "Instancia corriendo (PID: $$, lockfile: $LOCKFILE)"
echo "Trabajando..."
sleep 0.5
echo "Terminado (lockfile se elimina automaticamente)"
SCRIPT
chmod +x /tmp/lockfile-demo.sh
echo "--- Primera instancia ---"
/tmp/lockfile-demo.sh
echo ""
echo "--- Lockfile eliminado por trap EXIT ---"
ls -la /tmp/myapp.lock 2>&1 || echo "/tmp/myapp.lock: no existe (correcto)"
echo ""
echo "El patron lockfile previene ejecucion simultanea"
echo "trap EXIT garantiza que el lockfile se elimina al salir"
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/trap-exit.sh /tmp/trap-error-exit.sh /tmp/tempfile-pattern.sh
rm -f /tmp/trap-signal.sh /tmp/trap-err-diag.sh /tmp/combined-traps.sh
rm -f /tmp/trap-debug.sh /tmp/lockfile-demo.sh
rm -f /tmp/myapp.lock
echo "Archivos temporales eliminados"
'
```

---

## Conceptos reforzados

1. `trap cleanup EXIT` se ejecuta **siempre** al terminar el
   script (exit normal, error, set -e, SIGTERM). Solo SIGKILL
   lo evita. Es la forma correcta de limpiar archivos temporales.

2. `trap handler INT TERM` permite hacer **shutdown limpio**
   ante Ctrl+C o kill. Patron: poner `RUNNING=false` y dejar
   que el loop principal termine naturalmente.

3. `trap on_error ERR` captura errores con acceso a `$LINENO`,
   `$BASH_COMMAND`, y `caller` para stack traces. Se combina
   con EXIT para diagnostico + cleanup.

4. `trap handler DEBUG` ejecuta codigo **antes de cada comando**.
   Util para tracing sin modificar el script. Se activa/desactiva
   con `trap trace DEBUG` / `trap - DEBUG`.

5. Los subshells **no heredan traps**, excepto `trap '' SIGNAL`
   (ignorar senal). Cada subshell empieza con traps limpios.

6. El **patron lockfile** (`echo $$ > lock; trap cleanup EXIT`)
   previene ejecucion simultanea y garantiza limpieza del lock
   al terminar.
