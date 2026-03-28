# Lab — Debugging

## Objetivo

Dominar las tecnicas de debugging de scripts bash: set -x para
tracing con PS4 personalizado, BASH_XTRACEFD para redirigir trazas,
activacion selectiva, bash -n para verificacion de sintaxis,
ShellCheck para analisis estatico, debug condicional con
variables de entorno, trap ERR/DEBUG para diagnostico automatico,
y variables como LINENO, FUNCNAME, BASH_SOURCE, y caller.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — set -x y PS4

### Objetivo

Usar set -x para tracing de ejecucion y personalizar la salida
con PS4.

### Paso 1.1: set -x basico

```bash
docker compose exec debian-dev bash -c '
echo "=== set -x — trace de ejecucion ==="
echo ""
cat > /tmp/trace-demo.sh << '\''SCRIPT'\''
#!/bin/bash
set -x

NAME="World"
GREETING="Hello, $NAME"
echo "$GREETING"
RESULT=$((2 + 3))
echo "Result: $RESULT"

set +x
echo "Tracing desactivado"
SCRIPT
chmod +x /tmp/trace-demo.sh
echo "--- Ejecutar con tracing ---"
/tmp/trace-demo.sh
echo ""
echo "Las lineas con + muestran cada comando ANTES de ejecutarse"
echo "Los valores ya estan expandidos (se ve el resultado de \$NAME)"
echo "++ indica command substitution anidada"
'
```

### Paso 1.2: PS4 personalizado

```bash
docker compose exec debian-dev bash -c '
echo "=== PS4 personalizado ==="
echo ""
cat > /tmp/ps4-demo.sh << '\''SCRIPT'\''
#!/bin/bash
export PS4="+[${BASH_SOURCE[0]##*/}:${LINENO}] "
set -x

greet() {
    local name="$1"
    echo "Hello, $name"
}

X=10
Y=20
greet "World"
echo "Sum: $((X + Y))"
SCRIPT
chmod +x /tmp/ps4-demo.sh
echo "--- Con PS4 que muestra archivo y linea ---"
/tmp/ps4-demo.sh
echo ""
echo "--- PS4 con timestamp ---"
cat > /tmp/ps4-time.sh << '\''SCRIPT'\''
#!/bin/bash
export PS4="+[\$(date +%H:%M:%S.%N | cut -c1-12)] "
set -x
sleep 0.1
echo "paso 1"
sleep 0.1
echo "paso 2"
SCRIPT
chmod +x /tmp/ps4-time.sh
/tmp/ps4-time.sh
echo ""
echo "El timestamp en PS4 ayuda a identificar pasos lentos"
'
```

### Paso 1.3: Tracing selectivo y BASH_XTRACEFD

```bash
docker compose exec debian-dev bash -c '
echo "=== Tracing selectivo ==="
echo ""
cat > /tmp/selective.sh << '\''SCRIPT'\''
#!/bin/bash

echo "Seccion 1: sin tracing"
A=10

set -x
echo "Seccion 2: CON tracing"
B=$((A + 5))
echo "B=$B"
set +x

echo "Seccion 3: sin tracing de nuevo"
echo "Resultado final: B=$B"
SCRIPT
chmod +x /tmp/selective.sh
/tmp/selective.sh
echo ""
echo "=== BASH_XTRACEFD: redirigir trace a archivo ==="
echo ""
cat > /tmp/xtracefd.sh << '\''SCRIPT'\''
#!/bin/bash
exec 3>/tmp/trace.log
BASH_XTRACEFD=3
set -x

echo "stdout limpio"
echo "error message" >&2
RESULT=$((10 + 20))
echo "Resultado: $RESULT"

set +x
exec 3>&-
echo ""
echo "Trace guardado en /tmp/trace.log:"
cat /tmp/trace.log
rm /tmp/trace.log
SCRIPT
chmod +x /tmp/xtracefd.sh
/tmp/xtracefd.sh
echo ""
echo "BASH_XTRACEFD redirige trazas a un FD separado"
echo "stdout y stderr quedan limpios"
'
```

### Paso 1.4: bash -x sin modificar el script

```bash
docker compose exec debian-dev bash -c '
echo "=== bash -x (sin modificar el script) ==="
echo ""
cat > /tmp/no-modify.sh << '\''SCRIPT'\''
#!/bin/bash
echo "Este script no tiene set -x"
X=42
echo "X=$X"
SCRIPT
chmod +x /tmp/no-modify.sh
echo "--- Ejecutar normal ---"
/tmp/no-modify.sh
echo ""
echo "--- Ejecutar con bash -x ---"
bash -x /tmp/no-modify.sh
echo ""
echo "bash -x activa tracing sin editar el script"
echo "Util para depurar scripts de terceros"
'
```

---

## Parte 2 — bash -n y ShellCheck

### Objetivo

Verificar sintaxis y detectar bugs comunes antes de ejecutar.

### Paso 2.1: bash -n — verificacion de sintaxis

```bash
docker compose exec debian-dev bash -c '
echo "=== bash -n ==="
echo ""
echo "--- Script correcto ---"
cat > /tmp/good.sh << '\''SCRIPT'\''
#!/bin/bash
if [[ -f /etc/passwd ]]; then
    echo "ok"
fi
SCRIPT
bash -n /tmp/good.sh && echo "Sintaxis OK (exit 0)"
echo ""
echo "--- Script con error de sintaxis ---"
cat > /tmp/bad.sh << '\''SCRIPT'\''
#!/bin/bash
if [[ -f /etc/passwd ]]; then
    echo "ok"
SCRIPT
bash -n /tmp/bad.sh 2>&1 || true
echo ""
echo "--- Que detecta ---"
echo "  Si: if sin fi, comillas sin cerrar, done sin do"
echo "  No: typos en variables, comandos inexistentes, errores logicos"
echo ""
echo "bash -n es rapido pero limitado"
echo "Para analisis profundo: ShellCheck"
rm /tmp/good.sh /tmp/bad.sh
'
```

### Paso 2.2: ShellCheck

```bash
docker compose exec debian-dev bash -c '
echo "=== ShellCheck ==="
echo ""
which shellcheck &>/dev/null || {
    echo "ShellCheck no instalado"
    echo "Instalar con: apt install shellcheck"
    echo ""
    echo "Mostrando ejemplos de lo que detecta:"
    echo ""
}
echo "--- Errores comunes que ShellCheck detecta ---"
echo ""
cat > /tmp/buggy.sh << '\''SCRIPT'\''
#!/bin/bash
FILE=mi archivo.txt
if [ -f $FILE ]; then
    echo "existe"
fi
DATE=`date`
echo $DATE
cd /some/dir
for f in $(find . -name "*.txt"); do
    cat $f
done
SCRIPT
echo "Script con bugs:"
cat -n /tmp/buggy.sh
echo ""
if which shellcheck &>/dev/null; then
    echo "--- Analisis de ShellCheck ---"
    shellcheck /tmp/buggy.sh 2>&1 || true
else
    echo "--- Bugs que ShellCheck encontraria ---"
    echo "Linea 2: SC2155 — asignacion con espacios falla"
    echo "Linea 3: SC2086 — \$FILE sin comillas (word splitting)"
    echo "Linea 6: SC2006 — backticks obsoletos, usar \$()"
    echo "Linea 7: SC2086 — \$DATE sin comillas"
    echo "Linea 8: SC2164 — cd sin manejo de error"
    echo "Linea 9: SC2044 — for con find, usar while read"
    echo "Linea 10: SC2086 — \$f sin comillas"
fi
rm /tmp/buggy.sh
'
```

### Paso 2.3: Directivas de ShellCheck

```bash
docker compose exec debian-dev bash -c '
echo "=== Directivas de ShellCheck ==="
echo ""
echo "--- Deshabilitar un warning en una linea ---"
echo "# shellcheck disable=SC2086"
echo "echo \$VAR"
echo ""
echo "--- Deshabilitar para todo el archivo ---"
echo "# shellcheck disable=SC2086,SC2034"
echo "(al inicio del archivo)"
echo ""
echo "--- Especificar shell ---"
echo "# shellcheck shell=bash"
echo "(si no hay shebang)"
echo ""
echo "--- Niveles de severidad ---"
echo "  --severity=error    solo errores"
echo "  --severity=warning  errores + warnings"
echo "  --severity=info     + info"
echo "  --severity=style    todo (default)"
echo ""
echo "--- En CI/CD ---"
echo "  shellcheck --severity=error scripts/*.sh || exit 1"
echo ""
echo "--- Formatos de salida ---"
echo "  --format=gcc    para vim/emacs"
echo "  --format=json   para procesamiento"
echo "  --format=diff   para auto-fix"
'
```

---

## Parte 3 — Debug condicional

### Objetivo

Implementar debugging que se activa con variables de entorno.

### Paso 3.1: Patron DEBUG=1

```bash
docker compose exec debian-dev bash -c '
echo "=== Patron DEBUG=1 ==="
echo ""
cat > /tmp/debug-pattern.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

DEBUG="${DEBUG:-0}"

debug() {
    [[ "$DEBUG" == "1" ]] && echo "[DEBUG] $*" >&2 || true
}

process_file() {
    local file="$1"
    debug "Procesando: $file"
    local lines
    lines=$(wc -l < "$file")
    debug "Lineas encontradas: $lines"
    echo "$file: $lines lineas"
}

debug "Inicio del script"
process_file /etc/passwd
process_file /etc/hostname
debug "Fin del script"
SCRIPT
chmod +x /tmp/debug-pattern.sh
echo "--- Ejecucion normal ---"
/tmp/debug-pattern.sh
echo ""
echo "--- Con DEBUG=1 ---"
DEBUG=1 /tmp/debug-pattern.sh
echo ""
echo "Los mensajes de debug van a stderr"
echo "Se activan con DEBUG=1 antes del comando"
'
```

### Paso 3.2: Script debuggeable completo

```bash
docker compose exec debian-dev bash -c '
echo "=== Script debuggeable ==="
echo ""
cat > /tmp/debuggeable.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

readonly DEBUG="${DEBUG:-0}"
readonly TRACE="${TRACE:-0}"

# Activar tracing si se pide
if [[ "$TRACE" == "1" ]]; then
    export PS4="+[${BASH_SOURCE[0]##*/}:${LINENO}] "
    set -x
fi

# Funciones de logging
log()   { echo "[$(date +%H:%M:%S)] [$1] ${*:2}" >&2; }
debug() { [[ "$DEBUG" == "1" ]] && log DEBUG "$@" || true; }
info()  { log INFO "$@"; }
error() { log ERROR "$@"; }
die()   { error "$@"; exit 1; }

# Trap para errores
on_error() {
    error "Fallo en linea $LINENO: $BASH_COMMAND"
}
trap on_error ERR

# Logica
main() {
    debug "Argumentos: $*"
    info "Inicio"

    local total=0
    for f in /etc/passwd /etc/hostname; do
        local lines
        lines=$(wc -l < "$f")
        debug "$f: $lines lineas"
        ((total += lines))
    done

    info "Total: $total lineas"
    info "Fin"
}

main "$@"
SCRIPT
chmod +x /tmp/debuggeable.sh
echo "--- Normal ---"
/tmp/debuggeable.sh
echo ""
echo "--- DEBUG=1 ---"
DEBUG=1 /tmp/debuggeable.sh
echo ""
echo "--- TRACE=1 ---"
TRACE=1 /tmp/debuggeable.sh 2>&1 | head -10
echo "..."
echo ""
echo "3 niveles de detalle:"
echo "  Normal:  solo info"
echo "  DEBUG=1: info + debug"
echo "  TRACE=1: info + debug + set -x"
'
```

### Paso 3.3: Variables de diagnostico

```bash
docker compose exec debian-dev bash -c '
echo "=== Variables de diagnostico ==="
echo ""
cat > /tmp/diag-vars.sh << '\''SCRIPT'\''
#!/bin/bash

inner() {
    echo "=== Dentro de inner() ==="
    echo "LINENO:      $LINENO"
    echo "FUNCNAME:    ${FUNCNAME[@]}"
    echo "BASH_SOURCE: ${BASH_SOURCE[@]}"
    echo ""
    echo "--- caller (stack trace) ---"
    local i=0
    while caller $i; do
        ((i++))
    done
}

outer() {
    echo "Llamando a inner desde outer..."
    inner
}

outer
SCRIPT
chmod +x /tmp/diag-vars.sh
/tmp/diag-vars.sh
echo ""
echo "LINENO: linea actual"
echo "FUNCNAME[]: stack de funciones (inner outer main)"
echo "BASH_SOURCE[]: stack de archivos"
echo "caller N: linea, funcion, archivo del llamador N"
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/trace-demo.sh /tmp/ps4-demo.sh /tmp/ps4-time.sh
rm -f /tmp/selective.sh /tmp/xtracefd.sh /tmp/no-modify.sh
rm -f /tmp/debug-pattern.sh /tmp/debuggeable.sh /tmp/diag-vars.sh
echo "Archivos temporales eliminados"
'
```

---

## Conceptos reforzados

1. `set -x` imprime cada comando antes de ejecutarse con
   expansiones resueltas. `set +x` lo desactiva. `bash -x`
   activa tracing sin modificar el script.

2. `PS4` personaliza el prefijo de trace. `+[file:line]` con
   `${BASH_SOURCE[0]}` y `${LINENO}` es el patron mas util.
   Agregar timestamp para detectar pasos lentos.

3. `BASH_XTRACEFD` redirige trazas a un file descriptor
   separado, manteniendo stdout y stderr limpios.

4. `bash -n` verifica sintaxis sin ejecutar. ShellCheck
   detecta bugs mas profundos: variables sin comillas,
   backticks obsoletos, cd sin manejo de error.

5. El patron `DEBUG=1 ./script.sh` con funcion `debug()`
   que va a stderr permite activar diagnosticos sin modificar
   el script. Tres niveles: normal, DEBUG, TRACE.

6. `LINENO`, `FUNCNAME[]`, `BASH_SOURCE[]`, y `caller`
   permiten construir stack traces automaticos en `trap ERR`.
