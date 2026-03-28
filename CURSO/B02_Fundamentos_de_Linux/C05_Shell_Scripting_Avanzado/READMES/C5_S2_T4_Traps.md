# T04 — Traps

> **Fuentes:** `README.md` (base) + `README.max.md` (ampliado) + `labs/README.md`
>
> **Erratas corregidas respecto a las fuentes:**
>
> | Ubicación | Error | Corrección |
> |---|---|---|
> | md:22, max.md:22 | `trap 'cmd' ERR # cuando un comando falla (con set -e activo)` — implica que ERR requiere `set -e` | ERR dispara independientemente de `set -e`; comparten las mismas excepciones (`if`, `\|\|`, `&&`) pero son independientes |
> | max.md:732 | `trap '...' KILL` — intenta capturar SIGKILL | SIGKILL (9) no se puede capturar; ese trap nunca se ejecutará. Eliminado |
> | max.md:602-608 | Stack trace: `$frame` en el `while read` del pipe corre en subshell, no comparte valor con el loop externo | Corregido con contador independiente en el `while read` |

---

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
trap 'cmd' ERR        # cuando un comando falla (exit code != 0)
trap 'cmd' DEBUG      # antes de CADA comando (para debugging)
trap 'cmd' RETURN     # al retornar de una función o source

# Señales que NO se pueden capturar:
# SIGKILL (9) — kill -9, siempre mata
# SIGSTOP (19) — siempre suspende
```

> **Nota sobre ERR:** `trap ERR` y `set -e` son **independientes**. Ambos
> reaccionan a comandos con exit code != 0 y ambos comparten las mismas
> excepciones (comandos en `if`, `||`, `&&`, negación `!`), pero no se
> requieren mutuamente. Se usan juntos frecuentemente: `set -e` para abortar,
> `trap ERR` para diagnóstico antes de abortar.

---

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

> **Patrón clave:** capturar `$?` como primera línea de cleanup. Cualquier
> comando dentro de cleanup modifica `$?`, perdiendo el exit code original.
> Al final, `exit "$exit_code"` preserva el código con el que el script
> realmente terminó.

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

---

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

> **¿Por qué no llamar `exit` en el handler?** Si el handler llama a `exit`,
> el script termina inmediatamente, posiblemente dejando trabajo a medias.
> Poniendo `RUNNING=false`, el loop principal termina la iteración actual,
> el código post-loop ejecuta lógica de cierre, y luego el script sale
> naturalmente (disparando EXIT para cleanup).

---

## trap ERR — Capturar errores

ERR se dispara cuando un comando falla (exit code != 0), permitiendo ejecutar
código de diagnóstico:

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
    done | {
        local frame=0
        while read -r LINE FUNC FILE; do
            echo "  #$frame: $FUNC() en $FILE:$LINE" >&2
            ((frame++))
        done
    }

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
#   #0: inner() en script.sh:16
#   #1: outer() en script.sh:20
#   #2: main() en script.sh:22
# ---
```

> **`caller N`:** builtin de bash que devuelve información del frame N del
> call stack: `línea función archivo`. `caller 0` es el llamador inmediato,
> `caller 1` es quien llamó a ese, etc. Retorna 1 cuando se agota el stack.

### Diferencia entre ERR y EXIT

```bash
# ERR se dispara cuando un comando falla
# EXIT se dispara SIEMPRE al salir del script

# Usar ERR para logging/diagnóstico de errores
# Usar EXIT para cleanup de recursos

trap 'echo "Error detectado" >&2' ERR
trap 'echo "Script terminando" >&2' EXIT

# Si un comando falla con set -e:
# 1. ERR se ejecuta primero (diagnóstico)
# 2. set -e termina el script
# 3. EXIT se ejecuta al salir (cleanup)
```

---

## trap DEBUG — Debugging

DEBUG se ejecuta **antes de cada comando**:

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
# Uso práctico: logging condicional activable por variable de entorno
DEBUG=${DEBUG:-0}

if [[ $DEBUG -eq 1 ]]; then
    trap 'echo "[DEBUG] $BASH_COMMAND" >&2' DEBUG
fi

# Ejecutar con: DEBUG=1 ./script.sh
```

> **`$BASH_COMMAND`:** contiene el texto del comando que está a punto de
> ejecutarse. Disponible en cualquier trap, pero especialmente útil con
> DEBUG (antes del comando) y ERR (qué comando falló).

---

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

---

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

### Resetear, ignorar, manejar

```bash
# Tres estados posibles para una señal:
trap 'cmd' INT  # MANEJAR — Ctrl+C ejecuta cmd
trap '' INT     # IGNORAR — Ctrl+C no hace nada (silencioso)
trap - INT      # RESETEAR — Ctrl+C mata el script (comportamiento default)

# CUIDADO: '' (string vacío) IGNORA la señal, no la resetea
# Es una diferencia crucial:
trap '' INT     # → Ctrl+C no tiene efecto alguno
trap - INT      # → Ctrl+C termina el script normalmente
```

### Redefinir traps

```bash
# Cada nuevo trap para la misma señal REEMPLAZA el anterior:
trap 'echo "trap 1"' EXIT
trap 'echo "trap 2"' EXIT    # reemplaza el anterior
# Al salir solo se ejecuta "trap 2"

# Para acumular acciones, usar una sola función cleanup
# que maneje todo, en vez de múltiples traps para EXIT
```

---

## Traps y subshells

```bash
# Los traps NO se heredan en subshells:
trap 'echo "parent trap"' EXIT

(
    # Este subshell NO tiene el trap EXIT del padre
    echo "subshell"
)
# "subshell" — sin "parent trap"

# Command substitution crea un subshell — sin traps:
RESULT=$(
    # aquí no hay traps del padre
    echo "value"
)

# EXCEPCIÓN: trap '' SIG (ignorar señal) SÍ se hereda en subshells
# Si el padre ignora SIGINT, los subshells también lo ignoran
```

---

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
    # durante la espera (sleep bloquea la entrega de señales)
done
```

> **`sleep &; wait $!` vs `sleep` directo:** `sleep 5` bloquea el shell
> y las señales se encolan hasta que `sleep` termine. Con `sleep 5 &; wait $!`,
> el `wait` es un builtin de bash que se interrumpe inmediatamente al recibir
> una señal, permitiendo que el handler se ejecute sin esperar los 5 segundos.

### Lock file con cleanup

```bash
#!/usr/bin/env bash
set -euo pipefail

LOCKFILE="/tmp/myscript.lock"

die() { echo "ERROR: $*" >&2; exit 1; }

acquire_lock() {
    if ! (set -o noclobber; echo $$ > "$LOCKFILE") 2>/dev/null; then
        local pid
        pid=$(cat "$LOCKFILE" 2>/dev/null || echo "?")
        die "Otra instancia está corriendo (PID $pid)"
    fi
    trap 'rm -f "$LOCKFILE"' EXIT
}

acquire_lock
echo "Lock adquirido, ejecutando..."
sleep 10    # simular trabajo
echo "Terminado"
# EXIT trap elimina el lockfile
```

> **`set -o noclobber`:** impide que `>` sobrescriba archivos existentes.
> Si el lockfile ya existe, `echo $$ > "$LOCKFILE"` falla. Se ejecuta en
> un subshell `(...)` para no afectar al script. Este enfoque es atómico
> (race-condition-safe) comparado con `if [[ -f lock ]]; then ...`.

---

## Ejercicios

### Ejercicio 1 — Cleanup básico con EXIT

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
echo "Creado: $TMPFILE"
echo "datos de prueba" > "$TMPFILE"
cat "$TMPFILE"

echo "Saliendo con exit 0..."
exit 0
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Se ejecuta cleanup aunque <code>exit 0</code> indica éxito?</summary>

Sí. `trap EXIT` se ejecuta **siempre** al salir del script, sin importar si el exit code es 0 (éxito) o cualquier otro valor. El nombre "EXIT" significa "al salir", no "al fallar".

</details>

<details><summary>2. ¿Qué pasa si eliminas <code>exit 0</code> y dejas que el script termine solo?</summary>

El mismo resultado. Cuando el script llega al final sin un `exit` explícito, sale con el exit code del último comando ejecutado (en este caso `cat`, que será 0). EXIT se dispara igualmente.

</details>

---

### Ejercicio 2 — EXIT con error (set -e)

```bash
#!/usr/bin/env bash
set -euo pipefail

cleanup() {
    echo "cleanup ejecutado (exit code del script: $?)" >&2
}
trap cleanup EXIT

echo "Paso 1: OK"
echo "Paso 2: OK"
false
echo "Paso 3: ¿llega aquí?"
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Se ejecuta "Paso 3"?</summary>

No. `false` retorna exit code 1, y con `set -e` activo, el script se aborta inmediatamente.

</details>

<details><summary>2. ¿Se ejecuta cleanup? ¿Con qué <code>$?</code>?</summary>

Sí, cleanup se ejecuta. `$?` es 1 (el exit code de `false`). `trap EXIT` se dispara incluso cuando `set -e` termina el script.

</details>

---

### Ejercicio 3 — Capturar $? antes de perderlo

```bash
#!/usr/bin/env bash
set -euo pipefail

bad_cleanup() {
    echo "Limpiando..." >&2    # este echo cambia $?
    echo "Exit code: $?" >&2
}

good_cleanup() {
    local exit_code=$?         # capturar primero
    echo "Limpiando..." >&2
    echo "Exit code: $exit_code" >&2
    exit "$exit_code"
}

# Probar con bad_cleanup:
trap bad_cleanup EXIT
false
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué exit code reporta <code>bad_cleanup</code>?</summary>

0 — porque `echo "Limpiando..."` tiene éxito (exit code 0), y eso sobrescribe `$?` antes de que se imprima. El exit code original (1 de `false`) se pierde.

</details>

<details><summary>2. Si cambias a <code>trap good_cleanup EXIT</code>, ¿qué reporta?</summary>

1 — porque `local exit_code=$?` captura el valor original (1) antes de que cualquier otro comando lo modifique. Luego `exit "$exit_code"` asegura que el script termine con el código correcto.

</details>

---

### Ejercicio 4 — Señales INT y TERM

```bash
#!/usr/bin/env bash

trap 'echo -e "\nRecibido SIGINT"; exit 130' INT
trap 'echo "Recibido SIGTERM"; exit 143' TERM
trap 'echo "Script finalizado con $?"' EXIT

echo "PID: $$"
echo "Ejecuta en otra terminal: kill -TERM $$"
echo "O presiona Ctrl+C"

while true; do
    echo -n "."
    sleep 1
done
```

**Predicción:** antes de ejecutar y presionar Ctrl+C, responde:

<details><summary>1. ¿Qué traps se ejecutan al presionar Ctrl+C?</summary>

Dos traps en orden:
1. **INT** se ejecuta primero: imprime "Recibido SIGINT" y llama `exit 130`
2. **EXIT** se ejecuta después (porque `exit 130` sale del script): imprime "Script finalizado con 130"

El trap EXIT se dispara **siempre** al salir, incluso si otro trap llamó a `exit`.

</details>

<details><summary>2. ¿Por qué 130 y 143 como exit codes?</summary>

Convención Unix: cuando un proceso termina por una señal, su exit code es `128 + número_de_señal`. SIGINT = 2 → 128+2 = 130. SIGTERM = 15 → 128+15 = 143. Esto permite al llamador distinguir entre error normal y terminación por señal.

</details>

---

### Ejercicio 5 — trap ERR con diagnóstico

```bash
#!/usr/bin/env bash
set -euo pipefail

on_error() {
    echo "ERROR en línea $1: '$BASH_COMMAND' falló con código $?" >&2
}
trap 'on_error $LINENO' ERR

step_one() {
    echo "step_one: OK"
}

step_two() {
    grep -q "string_inexistente" /etc/hostname
}

echo "Ejecutando..."
step_one
step_two
echo "Nunca llega aquí"
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué variable contiene el comando que falló?</summary>

`$BASH_COMMAND` — contiene el texto literal del comando que acaba de fallar: `grep -q "string_inexistente" /etc/hostname`.

</details>

<details><summary>2. ¿Qué <code>$LINENO</code> reporta el trap? ¿La línea de <code>grep</code> o la línea de <code>step_two</code>?</summary>

`$LINENO` en el contexto del trap apunta a la línea donde se llamó a `step_two` en el código principal (no la línea del `grep` dentro de la función). Para ver la línea exacta dentro de funciones anidadas, se necesita `caller` para obtener el stack trace completo.

</details>

---

### Ejercicio 6 — Stack trace completo

```bash
#!/usr/bin/env bash
set -euo pipefail

stacktrace() {
    local exit_code=$?
    echo "=== ERROR (exit code: $exit_code) ===" >&2
    echo "Comando: $BASH_COMMAND" >&2
    echo "Stack trace:" >&2

    local i=0
    while caller $i 2>/dev/null; do
        ((i++))
    done | {
        local frame=0
        while read -r line func file; do
            printf "  #%d  %s()  en %s:%s\n" "$frame" "$func" "$file" "$line" >&2
            ((frame++))
        done
    }

    echo "===" >&2
}
trap stacktrace ERR

func_c() { false; }
func_b() { func_c; }
func_a() { func_b; }

func_a
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Cuántos frames muestra el stack trace?</summary>

3 frames:
- `#0  func_c()` — donde ocurrió `false`
- `#1  func_b()` — que llamó a `func_c`
- `#2  func_a()` — que llamó a `func_b`
(o posiblemente 4 si incluye `main` del top-level)

`caller N` recorre el call stack empezando desde 0 (llamador inmediato) hasta que se agota.

</details>

<details><summary>2. ¿Por qué se usa <code>{ ... }</code> en vez de un pipe simple a <code>while read</code>?</summary>

Para poder usar una variable local `frame` como contador. Si el `while read` estuviera en el pipe directamente (`| while read ...`), correría en un subshell donde cualquier variable sería local al subshell. Con `{ ... }` (group command), también corre en subshell en un pipe, pero es la forma correcta de encapsular el bloque. La clave es que `frame` se usa solo dentro de ese bloque, así que funciona correctamente.

</details>

---

### Ejercicio 7 — DEBUG trap condicional

```bash
#!/usr/bin/env bash

DEBUG=${DEBUG:-0}

[[ $DEBUG -eq 1 ]] && trap 'echo "[DEBUG] $BASH_COMMAND" >&2' DEBUG

for i in 1 2 3; do
    echo "Iteración $i"
done

echo "Fin"
```

**Predicción:** antes de ejecutar de dos formas, responde:

<details><summary>1. ¿Qué sale con <code>./script.sh</code> (sin DEBUG)?</summary>

```
Iteración 1
Iteración 2
Iteración 3
Fin
```

Sin `DEBUG=1`, el trap DEBUG nunca se registra. Solo stdout normal.

</details>

<details><summary>2. ¿Qué sale con <code>DEBUG=1 ./script.sh</code>?</summary>

Cada comando se muestra en stderr antes de ejecutarse:
```
[DEBUG] for i in 1 2 3
[DEBUG] echo "Iteración $i"
Iteración 1
[DEBUG] echo "Iteración $i"
Iteración 2
[DEBUG] echo "Iteración $i"
Iteración 3
[DEBUG] echo "Fin"
Fin
```

El trap DEBUG se ejecuta **antes** de cada comando, mostrando `$BASH_COMMAND` (el comando a punto de ejecutarse, sin expandir variables en todos los casos).

</details>

---

### Ejercicio 8 — Traps y subshells

```bash
#!/usr/bin/env bash

trap 'echo "EXIT padre"' EXIT

echo "=== Subshell () ==="
(
    trap -p EXIT
    echo "  (sin trap — no se hereda)"
)

echo "=== Command substitution ==="
RESULT=$(trap -p EXIT; echo "valor")
echo "  Resultado: $RESULT"

echo "=== Ignorar señal sí se hereda ==="
trap '' INT    # ignorar SIGINT
(
    trap -p INT
    echo "  (trap '' INT SÍ se hereda en subshells)"
)

trap - INT     # restaurar default
echo "Saliendo..."
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿El subshell <code>()</code> hereda el trap EXIT?</summary>

No. Los traps con handler (función o comando) **no se heredan** en subshells. `trap -p EXIT` dentro del subshell no mostrará nada.

</details>

<details><summary>2. ¿El subshell hereda <code>trap '' INT</code>?</summary>

Sí. `trap '' SIG` (ignorar señal) es la **excepción**: sí se hereda en subshells. `trap -p INT` dentro del subshell mostrará `trap -- '' INT`. Esto es por diseño POSIX — si el padre decide ignorar una señal, los hijos deben respetarlo.

</details>

<details><summary>3. ¿Qué contiene <code>$RESULT</code>?</summary>

Solo `"valor"`. `trap -p EXIT` no imprime nada (el `$()` crea un subshell sin traps), así que solo queda el `echo "valor"`.

</details>

---

### Ejercicio 9 — Lock file con noclobber

```bash
#!/usr/bin/env bash
set -euo pipefail

LOCKFILE="/tmp/test-lock-$$"

acquire_lock() {
    if ! (set -o noclobber; echo $$ > "$LOCKFILE") 2>/dev/null; then
        echo "Lock ya existe" >&2
        return 1
    fi
    trap 'rm -f "$LOCKFILE"' EXIT
    echo "Lock adquirido: $LOCKFILE"
}

# Primera adquisición:
acquire_lock
echo "Trabajando..."

# Intentar segunda adquisición (simular):
if ! (set -o noclobber; echo "otro" > "$LOCKFILE") 2>/dev/null; then
    echo "Segunda adquisición falló (esperado)"
fi

echo "Terminando..."
# EXIT trap elimina el lockfile
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Por qué <code>set -o noclobber</code> se ejecuta en un subshell <code>(...)</code>?</summary>

Porque `noclobber` modifica el comportamiento de `>` en todo el shell. Si se pusiera sin subshell, el script ya no podría usar `>` para sobrescribir archivos. El subshell aísla el efecto: `noclobber` solo aplica dentro de `(...)`.

</details>

<details><summary>2. ¿Por qué <code>noclobber</code> es mejor que <code>[[ -f lock ]] && die</code>?</summary>

Atomicidad. Con `[[ -f ]] && die`, hay un intervalo entre el check y la creación donde otro proceso podría crear el archivo (race condition / TOCTOU). Con `noclobber`, el check y la creación son una sola operación atómica del kernel — no hay ventana de race condition.

</details>

---

### Ejercicio 10 — Panorama: script profesional con traps

```bash
#!/usr/bin/env bash
set -euo pipefail

readonly PROG=$(basename "$0")
TMPDIR=""
EXIT_CODE=0

# --- Infraestructura ---
die()  { echo "$PROG: ERROR: $*" >&2; exit 1; }
log()  { echo "[$PROG $(date +%H:%M:%S)] $*" >&2; }

on_error() {
    EXIT_CODE=$?
    log "Fallo en línea $1: '$BASH_COMMAND' (exit: $EXIT_CODE)"
}
trap 'on_error $LINENO' ERR

cleanup() {
    local code=${EXIT_CODE:-$?}
    [[ -n "$TMPDIR" ]] && rm -rf "$TMPDIR"
    if (( code != 0 )); then
        log "Abortado con código $code"
    else
        log "Completado exitosamente"
    fi
}
trap cleanup EXIT

trap 'log "Interrumpido por usuario"; EXIT_CODE=130; exit 130' INT
trap 'log "Terminado por señal"; EXIT_CODE=143; exit 143' TERM

# --- Lógica ---
main() {
    [[ $# -ge 1 ]] || die "Uso: $PROG <archivo>"

    TMPDIR=$(mktemp -d)
    log "Inicio (tmpdir=$TMPDIR)"

    local file="$1"
    [[ -f "$file" ]] || die "'$file' no existe"

    cp "$file" "$TMPDIR/work.txt"
    local lines
    lines=$(wc -l < "$TMPDIR/work.txt")
    log "Procesado: $file ($lines líneas)"
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
```

**Predicción:** antes de ejecutar con `./script.sh /etc/passwd`, responde:

<details><summary>1. ¿Qué traps están registrados y para qué sirve cada uno?</summary>

| Trap | Propósito |
|---|---|
| `ERR` | Diagnóstico: captura exit code y registra qué comando falló, en qué línea |
| `EXIT` | Cleanup: elimina `TMPDIR`, reporta éxito o fallo |
| `INT` | Ctrl+C: log + exit 130 (convención SIGINT) |
| `TERM` | kill: log + exit 143 (convención SIGTERM) |

</details>

<details><summary>2. Si el archivo no existe, ¿qué traps se ejecutan y en qué orden?</summary>

1. `die` imprime error a stderr y llama `exit 1`
2. **EXIT** se dispara: cleanup elimina TMPDIR y reporta "Abortado con código 1"

`ERR` no se dispara porque `die` llama a `exit`, no porque un comando falle. Y la condición `[[ -f "$file" ]] || die` — el `die` se ejecuta en el lado derecho de `||`, que es una excepción de ERR.

</details>

<details><summary>3. ¿Por qué se usa <code>EXIT_CODE</code> como variable global en vez de confiar solo en <code>$?</code>?</summary>

Porque entre el error original y la ejecución de `cleanup`, pueden ejecutarse otros comandos (como los del trap INT/TERM o el propio `on_error`) que modifican `$?`. La variable `EXIT_CODE` se captura en `on_error` y se mantiene intacta hasta que `cleanup` la lea. Es más robusto que depender de `$?` en un trap que se ejecuta al final.

</details>
