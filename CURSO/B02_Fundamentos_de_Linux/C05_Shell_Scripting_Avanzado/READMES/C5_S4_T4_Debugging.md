# T04 — Debugging

> **Fuentes:** `README.md` + `README.max.md` + `labs/README.md`

## Erratas corregidas

| # | Ubicación | Error | Corrección |
|---|-----------|-------|------------|
| 1 | md:68, max:68 | `bash -xeuo pipefail script.sh` — `pipefail` no es un flag de un carácter | `bash -xeu -o pipefail script.sh` — las opciones largas requieren `-o` |
| 2 | md:301, max:301 | `echo "Exit code: $?"` en `on_error()` — el `echo` anterior consume `$?` (siempre vale 0) | Capturar `$?` como primera instrucción: `local exit_code=$?` |
| 3 | max:321 | Tabla dice `FUNCNAME[@]`: "main primero" | Al revés: función actual primero, `main` al final (`inner outer main`) |
| 4 | max:431 | Tabla dice `trap ERR`: "Solo en `set -e`" | `trap ERR` dispara ante cualquier retorno ≠ 0, independientemente de `set -e` |
| 5 | max:515-521 | Ej. 4: `caller $i` y luego `local line="$1"` / `local func="$2"` | `caller` escribe a stdout, no asigna positional params; parsear con `read -r line func file <<< "$(caller $i)"` |
| 6 | max:638 | `breakpoint()` usa `${BASH_SOURCE[1]}:${LINENO}` | `LINENO` dentro de una función es la línea de la función; usar `${BASH_LINENO[0]}` para la línea del llamador |
| 7 | max:684 | `time_section "Loop 100" for i in...` — `"$@"` ejecuta | `"$@"` ejecuta comandos simples, no sintaxis de shell (`for`); envolver en una función |
| 8 | max:716 | `calculate x y` pasa strings `"x"`/`"y"`, no valores | Funciona por resolución recursiva aritmética, pero `watch_vars` muestra `a=x` (confuso); pasar `"$x" "$y"` |
| 9 | md:388, max:399 | `on_error()` usa `${LINENO}` como trap handler de función | Dentro de una función trap, `LINENO` refiere a la línea de la función; usar `trap 'on_error $LINENO $?' ERR` para expandir en contexto del error |
| 10 | max:739-746 | Ej. 10: `dump_state` accede a variables locales de `inner_fail` | Variables locales (`DATA`, `CONFIG`) de `inner_fail` ya no están en scope dentro del trap handler |

---

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
# NOTA: pipefail es una opción de shell, no un flag de un carácter.
# Requiere -o:
bash -xeu -o pipefail script.sh
```

---

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

---

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

---

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

---

## Técnicas de debugging

### echo/printf para inspección

```bash
# La técnica más básica — prints estratégicos:
debug() {
    [[ "${DEBUG:-0}" == "1" ]] && echo "DEBUG: $*" >&2 || true
}

debug "variable X=$X"
debug "entrando a función process_file($1)"

# Ejecutar con: DEBUG=1 ./script.sh
```

> **Nota:** el `|| true` es necesario con `set -e`. Sin él, cuando `DEBUG=0`
> la condición `[[ ... ]] && echo ...` retorna 1 (el `&&` falla) y `set -e`
> aborta el script.

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

# IMPORTANTE: $? se sobrescribe con el exit code de cada comando.
# Si on_error() ejecuta un echo antes de capturar $?, ya vale 0.
# Solución 1: capturar $? como primera instrucción:
on_error() {
    local exit_code=$?
    echo "ERROR en ${BASH_SOURCE[1]:-$0}:${BASH_LINENO[0]}" >&2
    echo "  Comando: $BASH_COMMAND" >&2
    echo "  Exit code: $exit_code" >&2

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

> **Solución 2 (más robusta):** pasar `$LINENO` y `$?` como argumentos
> en la string del trap para expandirlos en el contexto del error:
> ```bash
> on_error() {
>     local err_line=$1 err_code=$2
>     echo "ERROR en línea $err_line, exit code $err_code" >&2
>     echo "  Comando: $BASH_COMMAND" >&2
> }
> trap 'on_error $LINENO $?' ERR
> ```
> Dentro de una función usada como trap handler, `$LINENO` refiere a la línea
> dentro de la propia función. Al pasarlo en la string del trap, se expande
> en el contexto donde ocurrió el error.

---

## Variables de diagnóstico de bash

| Variable | Descripción |
|---|---|
| `LINENO` | Número de línea actual en el script |
| `BASH_COMMAND` | Comando que se está ejecutando actualmente |
| `FUNCNAME[@]` | Array con el call stack de funciones (**función actual primero**, `main` al final) |
| `BASH_SOURCE[@]` | Array con rutas de archivos en el call stack |
| `BASH_LINENO[@]` | Array con números de línea del call stack (dónde se invocó cada `FUNCNAME[i]`) |
| `BASH_SUBSHELL` | Nivel de anidamiento de subshells |
| `SECONDS` | Segundos desde el inicio del script (reseteable con `SECONDS=0`) |

```bash
# LINENO — número de línea actual:
echo "Estoy en la línea $LINENO"

# FUNCNAME — stack de funciones (actual → ... → main):
inner() { echo "${FUNCNAME[@]}"; }
outer() { inner; }
outer
# inner outer main
# FUNCNAME[0]=inner (actual), FUNCNAME[1]=outer, FUNCNAME[2]=main

# BASH_SOURCE — stack de archivos:
echo "${BASH_SOURCE[0]}"    # archivo actual
echo "${BASH_SOURCE[@]}"    # todos los archivos en el stack

# BASH_COMMAND — comando actual:
trap 'echo "Ejecutando: $BASH_COMMAND"' DEBUG

# caller — información del llamador (escribe a stdout, NO a $1/$2/$3):
show_caller() {
    local line func file
    read -r line func file <<< "$(caller 0)"
    echo "Llamado desde $func() en $file:$line"
}
# caller N devuelve: LINENO FUNCTION FILENAME
# (una sola línea a stdout, separada por espacios)

# BASH_LINENO — array complementario a FUNCNAME:
# BASH_LINENO[0] = línea donde se llamó a FUNCNAME[0]
# BASH_LINENO[1] = línea donde se llamó a FUNCNAME[1]
```

---

## Debugging interactivo

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

---

## Tabla: Herramientas de debugging

| Herramienta | Qué detecta | Tipo | Limitación |
|---|---|---|---|
| `set -x` | Flujo de ejecución | Runtime | Solo trace, puede ser muy verboso |
| `bash -n` | Errores de sintaxis | Estático | No detecta lógica, typos ni quoting |
| `shellcheck` | Bugs, malas prácticas, portabilidad | Estático | No entiende intención del código |
| `trap DEBUG` | Cada comando antes de ejecutar | Runtime | Overhead alto, ralentiza ejecución |
| `trap ERR` | Errores y stack trace | Runtime | Funciona ante retorno ≠ 0 (no requiere `set -e`) |
| `$DEBUG` var | Prints condicionales | Runtime | Requiere instrumentar el código |

---

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
    local err_line=$1 err_code=$2
    error "Fallo en ${BASH_SOURCE[0]}:${err_line} (exit $err_code)"
    error "  Comando: $BASH_COMMAND"
}
trap 'on_error $LINENO $?' ERR

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

## Lab — Debugging

### Parte 1 — set -x y PS4

#### Paso 1.1: set -x básico

Crea y ejecuta este script:

```bash
#!/bin/bash
set -x

NAME="World"
GREETING="Hello, $NAME"
echo "$GREETING"
RESULT=$((2 + 3))
echo "Result: $RESULT"

set +x
echo "Tracing desactivado"
```

Observa:
- Las líneas con `+` muestran cada comando ANTES de ejecutarse
- Los valores ya están expandidos (se ve `World`, no `$NAME`)
- `++` indica command substitution anidada

#### Paso 1.2: PS4 personalizado

```bash
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
```

Luego prueba con timestamp:

```bash
#!/bin/bash
export PS4="+[\$(date +%H:%M:%S.%N | cut -c1-12)] "
set -x
sleep 0.1
echo "paso 1"
sleep 0.1
echo "paso 2"
```

El timestamp en PS4 ayuda a identificar pasos lentos.

#### Paso 1.3: Tracing selectivo y BASH_XTRACEFD

```bash
#!/bin/bash

echo "Sección 1: sin tracing"
A=10

set -x
echo "Sección 2: CON tracing"
B=$((A + 5))
echo "B=$B"
set +x

echo "Sección 3: sin tracing de nuevo"
echo "Resultado final: B=$B"
```

Ahora separa las trazas de stderr:

```bash
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
```

`BASH_XTRACEFD` redirige trazas a un FD separado — stdout y stderr quedan limpios.

#### Paso 1.4: bash -x sin modificar el script

```bash
cat > /tmp/no-modify.sh << 'SCRIPT'
#!/bin/bash
echo "Este script no tiene set -x"
X=42
echo "X=$X"
SCRIPT
chmod +x /tmp/no-modify.sh

# Ejecutar normal:
/tmp/no-modify.sh

# Ejecutar con bash -x:
bash -x /tmp/no-modify.sh
```

Útil para depurar scripts de terceros sin editarlos.

### Parte 2 — bash -n y ShellCheck

#### Paso 2.1: bash -n — verificación de sintaxis

```bash
# Script correcto:
cat > /tmp/good.sh << 'SCRIPT'
#!/bin/bash
if [[ -f /etc/passwd ]]; then
    echo "ok"
fi
SCRIPT
bash -n /tmp/good.sh && echo "Sintaxis OK (exit 0)"

# Script con error de sintaxis:
cat > /tmp/bad.sh << 'SCRIPT'
#!/bin/bash
if [[ -f /etc/passwd ]]; then
    echo "ok"
SCRIPT
bash -n /tmp/bad.sh 2>&1 || true
```

`bash -n` es rápido pero limitado: solo detecta errores de sintaxis pura.

#### Paso 2.2: ShellCheck

```bash
cat > /tmp/buggy.sh << 'SCRIPT'
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

shellcheck /tmp/buggy.sh
```

ShellCheck encontrará: SC2086 (sin comillas), SC2006 (backticks),
SC2164 (cd sin error), SC2044 (for con find).

#### Paso 2.3: Directivas de ShellCheck

```bash
# Deshabilitar un warning en una línea:
# shellcheck disable=SC2086
echo $VAR

# Para todo el archivo (al inicio):
# shellcheck disable=SC2086,SC2034

# Niveles de severidad en CI/CD:
shellcheck --severity=error scripts/*.sh || exit 1
# --severity: error | warning | info | style

# Formatos de salida:
shellcheck --format=gcc script.sh       # para vim/emacs
shellcheck --format=json script.sh      # para procesamiento
```

### Parte 3 — Debug condicional y variables de diagnóstico

#### Paso 3.1: Patrón DEBUG=1

```bash
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
    debug "Líneas encontradas: $lines"
    echo "$file: $lines líneas"
}

debug "Inicio del script"
process_file /etc/passwd
process_file /etc/hostname
debug "Fin del script"
```

```bash
# Normal:
./script.sh

# Con debug:
DEBUG=1 ./script.sh
```

Los mensajes de debug van a stderr — se activan con `DEBUG=1`.

#### Paso 3.2: Variables de diagnóstico

```bash
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
```

Observar:
- `FUNCNAME[@]` muestra: `inner outer main` (actual primero, `main` al final)
- `caller N` imprime a stdout: `LÍNEA FUNCIÓN ARCHIVO`

---

## Ejercicios

### Ejercicio 1 — set -x y PS4 básico

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
```

Ejecuta el script. ¿Qué muestra cada línea de trace?

<details><summary>Predicción</summary>

```
+[4] NAME=World
+[5] GREETING='Hello, World'
+[6] echo 'Hello, World'
Hello, World
+[7] RESULT=5
+[8] echo 'Result: 5'
Result: 5
+[10] set +x
Tracing desactivado
```

Cada línea `+[N]` muestra el número de línea y el comando con las expansiones
ya resueltas. `set +x` se traza a sí mismo (es el último comando trazado).
La última línea sale sin `+` porque el tracing ya se desactivó.

</details>

### Ejercicio 2 — bash -n vs ShellCheck

Crea este script con errores intencionales:

```bash
cat << 'SCRIPT' > /tmp/buggy.sh
#!/bin/bash
FILE=mi archivo.txt
if [ -f $FILE ]; then
    echo "existe"
    DATE=`date`
    echo $DATE
SCRIPT
```

1. Ejecuta `bash -n /tmp/buggy.sh`
2. Ejecuta `shellcheck /tmp/buggy.sh` (si está instalado)

¿Cuántos problemas encuentra cada herramienta?

<details><summary>Predicción</summary>

`bash -n` encuentra **1 problema**: `syntax error: unexpected end of file`
(falta `fi` para cerrar el `if`).

`shellcheck` encuentra **múltiples problemas** (tras agregar `fi`):
- SC2086: `$FILE` sin comillas (word splitting)
- SC2086: `$DATE` sin comillas
- SC2006: backticks `` `date` `` obsoletos, usar `$(date)`
- La asignación `FILE=mi archivo.txt` falla: sin comillas, `archivo.txt`
  se interpreta como un comando

`bash -n` solo detecta sintaxis; ShellCheck detecta bugs semánticos.

</details>

### Ejercicio 3 — BASH_XTRACEFD: separar traces de stderr

```bash
#!/usr/bin/env bash
set -euo pipefail

TRACE_FILE="/tmp/trace_$$.log"
exec 3>"$TRACE_FILE"
BASH_XTRACEFD=3

export PS4='+[${BASH_SOURCE[0]##*/}:${LINENO}] '

echo "stdout: este mensaje va a stdout"
echo "stderr: este es un error" >&2

set -x
NAME="debug_test"
RESULT=$((3 + 4))
echo "La suma de 3+4 es $RESULT"
set +x

exec 3>&-
echo ""
echo "=== Trace guardado en $TRACE_FILE ==="
cat "$TRACE_FILE"
rm "$TRACE_FILE"
```

¿Qué aparece en stdout, qué en stderr y qué en el archivo de trace?

<details><summary>Predicción</summary>

**stdout:**
```
stdout: este mensaje va a stdout
La suma de 3+4 es 7

=== Trace guardado en /tmp/trace_XXXX.log ===
+[script.sh:14] NAME=debug_test
+[script.sh:15] RESULT=7
+[script.sh:16] echo 'La suma de 3+4 es 7'
+[script.sh:17] set +x
```

**stderr:**
```
stderr: este es un error
```

**archivo de trace** (`/tmp/trace_XXXX.log`):
```
+[script.sh:14] NAME=debug_test
+[script.sh:15] RESULT=7
+[script.sh:16] echo 'La suma de 3+4 es 7'
+[script.sh:17] set +x
```

Los tres flujos están completamente separados: stdout limpio para datos,
stderr limpio para errores reales, y el trace en su propio archivo.
`$$` en el nombre evita colisiones entre instancias.

</details>

### Ejercicio 4 — Debug condicional con niveles

```bash
#!/usr/bin/env bash
set -euo pipefail

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

process() {
    local file="$1"
    debug "Procesando: $file"
    local lines
    lines=$(wc -l < "$file")
    debug "Líneas: $lines"
    echo "$file: $lines líneas"
}

info "Inicio"
process /etc/passwd
process /etc/hostname
info "Fin"
```

Ejecuta con tres modos y compara la salida:
1. `./script.sh`
2. `DEBUG=1 ./script.sh`
3. `TRACE=1 ./script.sh`

<details><summary>Predicción</summary>

**Modo 1 — Normal:**
```
[HH:MM:SS] [INFO] Inicio
/etc/passwd: NN líneas
/etc/hostname: 1 líneas
[HH:MM:SS] [INFO] Fin
```
Solo mensajes `info` (stderr) y la salida normal (stdout).

**Modo 2 — DEBUG=1:**
```
[HH:MM:SS] [INFO] Inicio
[HH:MM:SS] [DEBUG] Procesando: /etc/passwd
[HH:MM:SS] [DEBUG] Líneas: NN
/etc/passwd: NN líneas
[HH:MM:SS] [DEBUG] Procesando: /etc/hostname
[HH:MM:SS] [DEBUG] Líneas: 1
/etc/hostname: 1 líneas
[HH:MM:SS] [INFO] Fin
```
Se agregan mensajes `[DEBUG]` mostrando el flujo interno.

**Modo 3 — TRACE=1:**
Toda la salida anterior más las líneas `+[script.sh:NN]` de `set -x`
para cada comando ejecutado. Muy verboso — muestra cada asignación,
expansión y llamada.

Tres niveles de detalle para tres escenarios: producción, investigación
de lógica, inspección línea a línea.

</details>

### Ejercicio 5 — Stack trace con caller

```bash
#!/usr/bin/env bash
set -euo pipefail

print_stack() {
    local i=0
    local line func file
    echo "=== Stack Trace ===" >&2
    while read -r line func file <<< "$(caller $i 2>/dev/null)" && [[ -n "$line" ]]; do
        printf "  #%d %s() at %s line %s\n" "$i" "$func" "$file" "$line" >&2
        ((i++))
    done
}

level3() {
    echo "En level3"
    print_stack
}

level2() {
    echo "En level2"
    level3
}

level1() {
    echo "En level1"
    level2
}

echo "Llamando level1..."
level1
```

¿Qué muestra el stack trace?

<details><summary>Predicción</summary>

```
Llamando level1...
En level1
En level2
En level3
=== Stack Trace ===
  #0 level3() at ./script.sh line 18
  #1 level2() at ./script.sh line 23
  #2 level1() at ./script.sh line 28
  #3 main() at ./script.sh line 31
```

(Los números de línea dependen del archivo real.)

Puntos clave:
- `caller 0` devuelve dónde se llamó a `print_stack` (dentro de `level3`)
- `caller 1` devuelve dónde se llamó a `level3` (dentro de `level2`)
- Así sucesivamente hasta `main`
- `caller` escribe a **stdout** (`LÍNEA FUNCIÓN ARCHIVO`), por eso se
  parsea con `read -r line func file <<< "$(caller $i)"`
- Cuando `caller $i` falla (no hay más frames), devuelve exit code 1
  y salida vacía; `$line` queda vacío y el `[[ -n "$line" ]]` detiene el loop

</details>

### Ejercicio 6 — trap ERR con exit code correcto

```bash
#!/usr/bin/env bash
set -euo pipefail

on_error() {
    local err_line=$1 err_code=$2
    echo "=== ERROR ===" >&2
    echo "  Línea:     $err_line" >&2
    echo "  Comando:   $BASH_COMMAND" >&2
    echo "  Exit code: $err_code" >&2
    echo "  Stack:" >&2
    local i=0
    while caller $i 2>/dev/null; do
        ((i++))
    done | while read -r line func file; do
        echo "    $func() en $file:$line" >&2
    done
}
trap 'on_error $LINENO $?' ERR

risky_operation() {
    echo "Intentando operación..."
    ls /directorio/que/no/existe
}

echo "Inicio"
risky_operation
echo "Esto no se ejecuta"
```

¿Qué información muestra el error handler y por qué el exit code es correcto?

<details><summary>Predicción</summary>

```
Inicio
Intentando operación...
ls: cannot access '/directorio/que/no/existe': No such file or directory
=== ERROR ===
  Línea:     22
  Comando:   ls /directorio/que/no/existe
  Exit code: 2
  Stack:
    risky_operation() en ./script.sh:22
    main() en ./script.sh:26
```

Puntos clave:
- `$LINENO` y `$?` se pasan como argumentos en la string del trap:
  `trap 'on_error $LINENO $?' ERR`
- Se **expanden en el contexto del error** (antes de entrar a la función),
  así `$LINENO` es la línea del `ls` que falló y `$?` es 2
- Si usáramos `$?` directamente dentro de `on_error()`, el primer `echo`
  (que tiene éxito) lo sobrescribiría con 0
- `$BASH_COMMAND` SÍ funciona dentro de la función — bash lo preserva
- `set -e` termina el script tras el error; "Esto no se ejecuta" nunca aparece

</details>

### Ejercicio 7 — Breakpoint interactivo

```bash
#!/usr/bin/env bash
set -euo pipefail

breakpoint() {
    echo "=== BREAKPOINT en ${BASH_SOURCE[1]:-$0}:${BASH_LINENO[0]} ===" >&2
    echo "  Variables:" >&2
    local var
    for var in "$@"; do
        echo "    $var=${!var:-<unset>}" >&2
    done
    echo "  Presiona Enter para continuar o 'q' para salir..." >&2
    read -r answer
    [[ "$answer" == "q" ]] && exit 1 || true
}

x="foo"
y="bar"
z=0

echo "Inicio"
breakpoint x y z

x="modified"
z=$((z + 1))
breakpoint x y z

echo "Fin: x=$x, z=$z"
```

¿Qué muestra cada breakpoint y qué línea reporta?

<details><summary>Predicción</summary>

**Primer breakpoint:**
```
Inicio
=== BREAKPOINT en ./script.sh:20 ===
  Variables:
    x=foo
    y=bar
    z=0
  Presiona Enter para continuar o 'q' para salir...
```

**Segundo breakpoint** (tras presionar Enter):
```
=== BREAKPOINT en ./script.sh:24 ===
  Variables:
    x=modified
    y=bar
    z=1
  Presiona Enter para continuar o 'q' para salir...
```

Puntos clave:
- `${BASH_LINENO[0]}` muestra la línea donde se **llamó** a `breakpoint`,
  no la línea dentro de la función (ese sería el error de usar `$LINENO`)
- `${!var}` hace expansión indirecta: dado `var="x"`, `${!var}` = valor de `$x`
- Se reciben los **nombres** de las variables como argumentos para
  inspeccionar variables diferentes en cada breakpoint

</details>

### Ejercicio 8 — Medir tiempo de ejecución por secciones

```bash
#!/usr/bin/env bash
set -euo pipefail

time_section() {
    local label="$1"; shift
    local start end elapsed
    start=$(date +%s%N)
    "$@"
    end=$(date +%s%N)
    elapsed=$(( (end - start) / 1000000 ))
    echo "=== $label: ${elapsed}ms ===" >&2
}

count_lines() {
    wc -l /etc/passwd /etc/group /etc/hosts 2>/dev/null | tail -1
}

loop_100() {
    local i
    for i in $(seq 1 100); do :; done
}

echo "Midiendo tiempos..."
time_section "Sleep 0.5s" sleep 0.5
time_section "Count lines" count_lines
time_section "Echo simple" echo "hello"
time_section "Loop 100" loop_100
```

¿Por qué `loop_100` se envuelve en una función en lugar de escribir el
`for` directamente en la llamada a `time_section`?

<details><summary>Predicción</summary>

Salida aproximada:
```
Midiendo tiempos...
=== Sleep 0.5s: ~500ms ===
  NN total
=== Count lines: ~Nms ===
hello
=== Echo simple: ~0ms ===
=== Loop 100: ~Nms ===
```

`time_section` usa `"$@"` para ejecutar el comando recibido. Esto funciona
para **comandos simples** (`sleep 0.5`, `echo "hello"`) y **funciones**
(`count_lines`, `loop_100`).

Pero `for i in...; do...; done` es **sintaxis de shell**, no un comando
ejecutable. Si escribiéramos:
```bash
time_section "Loop" for i in $(seq 1 100); do :; done
```
`"$@"` intentaría ejecutar `for` como un programa externo y fallaría.

Soluciones:
1. **Envolver en una función** (como `loop_100`) — la más limpia
2. Usar `eval "$@"` — funciona pero inseguro
3. `$SECONDS` para mediciones simples: `SECONDS=0; ...; echo "$SECONDS s"`

</details>

### Ejercicio 9 — Watch variables con expansión indirecta

```bash
#!/usr/bin/env bash
set -euo pipefail

watch_vars() {
    local output=""
    local var
    for var in "$@"; do
        output+=" $var=${!var:-<unset>}"
    done
    echo "[WATCH]${output}" >&2
}

calculate() {
    local a=$1 b=$2
    local result
    watch_vars a b
    result=$((a * b + a - b))
    watch_vars a b result
    result=$((result / 2))
    watch_vars a b result
    echo "$result"
}

x=10
y=5
echo "Antes: x=$x y=$y"
final=$(calculate "$x" "$y")
echo "Resultado: $final"
```

¿Qué valores muestra `watch_vars` en cada llamada?

<details><summary>Predicción</summary>

```
Antes: x=10 y=5
[WATCH] a=10 b=5
[WATCH] a=10 b=5 result=55
[WATCH] a=10 b=5 result=27
Resultado: 27
```

Cálculo: `10 * 5 + 10 - 5 = 55`, luego `55 / 2 = 27` (truncamiento entero).

Puntos clave:
- Se pasa `"$x" "$y"` (valores `10` y `5`), no los nombres `x` e `y`.
  Así `a=10` y no `a=x` (que funcionaría por resolución recursiva
  aritmética de bash, pero sería confuso en `watch_vars`)
- `${!var}` hace expansión indirecta: dado `var="a"`, `${!var}` = valor de `$a`
- `${!var:-<unset>}` muestra `<unset>` si la variable no existe (pero aquí
  la primera `watch_vars` no incluye `result` porque aún no está definida)
- `watch_vars` escribe a stderr para no interferir con el `echo "$result"`
  que captura `$(calculate ...)`

</details>

### Ejercicio 10 — Script debuggeable completo

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
    local err_line=$1 err_code=$2
    error "Fallo en línea $err_line (exit $err_code)"
    error "  Comando: $BASH_COMMAND"
    local i=0
    while caller $i 2>/dev/null; do
        ((i++))
    done | while read -r line func file; do
        error "  #$i $func() en $file:$line"
    done
}
trap 'on_error $LINENO $?' ERR

# --- Logic ---
count_config_files() {
    local dir="${1:?Uso: count_config_files DIR}"
    debug "Buscando .conf en $dir"
    local count
    count=$(find "$dir" -maxdepth 1 -name "*.conf" 2>/dev/null | wc -l)
    debug "Encontrados: $count"
    echo "$count"
}

main() {
    debug "Argumentos: $*"
    info "Inicio"

    local dirs=(/etc /tmp /var)
    local total=0

    for dir in "${dirs[@]}"; do
        local n
        n=$(count_config_files "$dir")
        info "$dir: $n archivos .conf"
        ((total += n)) || true
    done

    info "Total: $total archivos .conf"

    if [[ "$total" -eq 0 ]]; then
        die "No se encontraron archivos de configuración"
    fi

    info "Fin"
}

main "$@"
```

Ejecuta con los tres modos (`./script.sh`, `DEBUG=1`, `TRACE=1`). Luego
cambia `find` por `ls "$dir"/*.conf` (sin `2>/dev/null`) y observa cómo
responde el error handler cuando `/tmp` no tiene archivos `.conf`.

<details><summary>Predicción</summary>

**Modo normal** (`./script.sh`):
```
[HH:MM:SS] [INFO] Inicio
[HH:MM:SS] [INFO] /etc: N archivos .conf
[HH:MM:SS] [INFO] /tmp: 0 archivos .conf
[HH:MM:SS] [INFO] /var: 0 archivos .conf
[HH:MM:SS] [INFO] Total: N archivos .conf
[HH:MM:SS] [INFO] Fin
```

**Modo DEBUG=1:** agrega `[DEBUG] Buscando .conf en /etc`, `[DEBUG] Encontrados: N`
para cada directorio.

**Modo TRACE=1:** todo lo anterior más `+[script.sh:NN]` para cada línea.

**Con `ls` en lugar de `find`** (sin `2>/dev/null`): cuando `/tmp` no tiene
archivos `.conf`, el glob `*.conf` no se expande y `ls` falla con exit ≠ 0.
El trap ERR muestra:
```
[HH:MM:SS] [ERROR] Fallo en línea NN (exit 2)
[HH:MM:SS] [ERROR]   Comando: ls /tmp/*.conf
```

Este patrón (DEBUG/TRACE/on_error con inline trap) es la base profesional
para scripts bash diagnosticables. Los tres niveles (normal → debug → trace)
permiten investigar problemas sin modificar el código.

</details>
