# Traps y cleanup

## 1. El problema: recursos huérfanos

Los scripts crean recursos temporales: archivos, directorios, procesos
background, locks. Si el script muere (error, Ctrl+C, kill), esos recursos
quedan huérfanos:

```bash
# Script frágil:
tmpfile=$(mktemp)
curl -o "$tmpfile" "$url"
process "$tmpfile"
rm "$tmpfile"              # ¿Qué pasa si curl o process fallan?
                           # ¿Qué pasa si alguien presiona Ctrl+C?
                           # tmpfile queda en /tmp para siempre
```

`trap` permite ejecutar código cuando el script recibe una **señal** o
cambia de estado, garantizando limpieza sin importar cómo termine.

---

## 2. Sintaxis de trap

```bash
trap 'commands' SIGNAL [SIGNAL...]

# Ejemplos:
trap 'echo "Caught SIGINT"' INT         # Ctrl+C
trap 'rm -f "$tmpfile"' EXIT            # al terminar (cualquier causa)
trap 'echo "Error on line $LINENO"' ERR # cuando un comando falla
trap '' INT                              # IGNORAR señal (string vacío)
trap - INT                               # RESTAURAR comportamiento por defecto
```

### Señales principales

| Señal | Número | Causa | Comportamiento default |
|-------|--------|-------|----------------------|
| `EXIT` | — | Script termina (cualquier causa) | — |
| `ERR` | — | Un comando retorna exit code ≠ 0 | — |
| `DEBUG` | — | Antes de cada comando | — |
| `RETURN` | — | Función o source retorna | — |
| `INT` | 2 | Ctrl+C | Terminar |
| `TERM` | 15 | `kill` (default) | Terminar |
| `HUP` | 1 | Terminal cerrada | Terminar |
| `QUIT` | 3 | Ctrl+\ | Terminar + core dump |
| `PIPE` | 13 | Escribir a pipe roto | Terminar |
| `USR1/USR2` | 10/12 | Definidas por usuario | Terminar |

`EXIT`, `ERR`, `DEBUG`, `RETURN` son **pseudo-señales** de Bash (no POSIX
signals). Las demás son señales reales del sistema operativo.

---

## 3. trap EXIT: el patrón fundamental

`EXIT` se ejecuta **siempre** que el script termina: exit normal, error,
señal capturada. Es el mecanismo más importante:

```bash
#!/bin/bash

tmpfile=$(mktemp)
trap 'rm -f "$tmpfile"' EXIT

# No importa cómo termine el script, tmpfile se limpia:
curl -o "$tmpfile" "$url"    # si falla: EXIT trap limpia
process "$tmpfile"            # si falla: EXIT trap limpia
# Ctrl+C:                      EXIT trap limpia
# exit 0:                      EXIT trap limpia
# exit 1:                      EXIT trap limpia
```

```
Flujos posibles:

  Script exitoso → exit 0 → trap EXIT → rm tmpfile ✓
  curl falla     → exit 1 → trap EXIT → rm tmpfile ✓
  Ctrl+C         → SIGINT → trap EXIT → rm tmpfile ✓
  kill PID        → SIGTERM → trap EXIT → rm tmpfile ✓
```

### ¿Por qué EXIT y no las señales individuales?

```bash
# MAL: hay que atrapar cada señal
trap 'rm -f "$tmpfile"' INT TERM HUP QUIT

# BIEN: EXIT cubre todo
trap 'rm -f "$tmpfile"' EXIT
```

`EXIT` se ejecuta incluso después de que una señal termine el script (si
la señal es capturada o el script termina normalmente). Cubre todos los
casos de un golpe.

---

## 4. Cleanup function

Para cleanups complejos, usar una función:

```bash
#!/bin/bash
set -euo pipefail

# Variables globales para recursos
TMPDIR=""
BG_PID=""
LOCK_FILE=""

cleanup() {
    local exit_code=$?   # preservar el exit code original

    # Matar proceso background si existe
    if [[ -n "$BG_PID" ]] && kill -0 "$BG_PID" 2>/dev/null; then
        kill "$BG_PID" 2>/dev/null
        wait "$BG_PID" 2>/dev/null
    fi

    # Eliminar directorio temporal
    if [[ -d "$TMPDIR" ]]; then
        rm -rf "$TMPDIR"
    fi

    # Liberar lock
    if [[ -f "$LOCK_FILE" ]]; then
        rm -f "$LOCK_FILE"
    fi

    exit "$exit_code"   # preservar exit code
}

trap cleanup EXIT

# Crear recursos
TMPDIR=$(mktemp -d)
LOCK_FILE="/var/run/myscript.lock"
touch "$LOCK_FILE"

# Proceso background
tail -f /var/log/syslog > "$TMPDIR/monitor.log" &
BG_PID=$!

# Trabajo principal
do_work "$TMPDIR"
```

**Puntos clave**:
- Capturar `$?` al inicio de cleanup (antes de que otros comandos lo sobreescriban)
- `kill -0 PID` verifica si el proceso aún existe (sin señal)
- `2>/dev/null` en kill/wait para suprimir errores si el proceso ya murió
- `exit "$exit_code"` para que el script reporte el código correcto al caller

---

## 5. Tempfiles seguros

### 5.1 mktemp

```bash
# Archivo temporal (en /tmp o $TMPDIR)
tmpfile=$(mktemp)                    # /tmp/tmp.Xf3k9a2
tmpfile=$(mktemp /tmp/myapp.XXXXXX)  # /tmp/myapp.k9f2a1

# Directorio temporal
tmpdir=$(mktemp -d)                  # /tmp/tmp.Dk3f9a

# XXXXXX se reemplazan con caracteres aleatorios
# Mínimo 3 X (6 es la convención)
```

### 5.2 Patrón completo de tempfile seguro

```bash
#!/bin/bash
set -euo pipefail

tmpfile=$(mktemp) || exit 1
trap 'rm -f "$tmpfile"' EXIT

# Usar tmpfile con seguridad
echo "data" > "$tmpfile"
process < "$tmpfile"
```

### 5.3 Patrón con directorio temporal

```bash
#!/bin/bash
set -euo pipefail

workdir=$(mktemp -d) || exit 1
trap 'rm -rf "$workdir"' EXIT

# Múltiples archivos dentro del workdir
curl -o "$workdir/data.json" "$api_url"
jq '.items[]' "$workdir/data.json" > "$workdir/items.txt"
sort "$workdir/items.txt" > "$workdir/sorted.txt"
# Todo se limpia automáticamente al final
```

### 5.4 Por qué no usar nombres predecibles

```bash
# MAL: nombre predecible → race condition + symlink attack
tmpfile="/tmp/myapp_$$"   # PID es predecible

# Ataque:
# 1. Atacante crea symlink: /tmp/myapp_12345 → /etc/passwd
# 2. Script escribe a /tmp/myapp_12345
# 3. Realmente escribe a /etc/passwd

# BIEN: mktemp usa caracteres aleatorios impredecibles
tmpfile=$(mktemp)
```

---

## 6. trap ERR: reaccionar a errores

`ERR` se dispara cuando un comando retorna exit code ≠ 0:

```bash
#!/bin/bash
set -euo pipefail

on_error() {
    echo "ERROR on line $LINENO: command exited with $?" >&2
}

trap on_error ERR

echo "Step 1: OK"
false                    # exit code 1 → trap ERR → on_error
echo "Step 2: never reached (set -e kills the script)"
```

### ERR + set -e: combinación potente

```bash
#!/bin/bash
set -euo pipefail

trap 'echo "FAILED at line $LINENO (exit $?)" >&2' ERR

download_data
process_data
generate_report
send_email

# Si cualquier función falla:
# 1. trap ERR imprime el error con número de línea
# 2. set -e termina el script
# 3. trap EXIT (si existe) hace cleanup
```

### ERR no se dispara en:

```bash
# Comando en if/while/until condition
if false; then ...; fi       # ERR NO se dispara

# Parte de && o ||
false || echo "ok"           # ERR NO se dispara

# Comando con ! (negación explícita)
! false                       # ERR NO se dispara
```

Esto es correcto: en esos contextos, un exit code ≠ 0 es **esperado**, no
un error.

---

## 7. trap INT: manejar Ctrl+C con gracia

### 7.1 Cleanup interactivo

```bash
#!/bin/bash

processing=true

trap 'processing=false; echo " Interrupted, finishing current item..."' INT

items=("task1" "task2" "task3" "task4" "task5")

for item in "${items[@]}"; do
    if ! $processing; then
        echo "Stopping early."
        break
    fi
    echo "Processing $item..."
    sleep 2
done

echo "Processed items before interrupt"
```

El primer Ctrl+C pone `processing=false`, y el loop termina limpiamente
después del item actual. El script no muere abruptamente.

### 7.2 Confirmar antes de abortar

```bash
trap 'echo; read -p "Really quit? (y/n) " ans; [[ "$ans" == "y" ]] && exit 1' INT
```

### 7.3 Propagar la señal correctamente

Cuando atrapas una señal, el caller (ej: otro script) no sabe que murió por
señal. Para propagar correctamente:

```bash
trap 'cleanup; trap - INT; kill -INT $$' INT
```

```
1. cleanup         → limpia recursos
2. trap - INT      → restaura handler por defecto
3. kill -INT $$    → se envía la señal a sí mismo
                   → ahora muere con el exit code correcto para SIGINT (130)
```

Esto importa para scripts llamados desde otros scripts: el padre puede
distinguir "terminó por Ctrl+C" (128+2=130) de "terminó con error" (1).

---

## 8. trap DEBUG: trace avanzado

`DEBUG` se ejecuta **antes de cada comando**. Es útil para debugging o logging:

```bash
#!/bin/bash

trap 'echo "+ [line $LINENO] $BASH_COMMAND"' DEBUG

x=10
y=$((x + 5))
echo "Result: $y"
```

```
+ [line 5] x=10
+ [line 6] y=15
+ [line 7] echo 'Result: 15'
Result: 15
```

### Trace selectivo

```bash
# Solo activar DEBUG en la parte problemática
enable_trace() { trap 'echo "DEBUG: $BASH_COMMAND"' DEBUG; }
disable_trace() { trap - DEBUG; }

normal_code
enable_trace
suspicious_code    # solo esto se traza
disable_trace
more_normal_code
```

**Rendimiento**: `trap DEBUG` es costoso — se ejecuta antes de **cada** comando.
Solo usar para debugging, nunca en producción.

---

## 9. trap RETURN: cleanup en funciones

`RETURN` se dispara cuando una función (o un `source`) termina:

```bash
with_tempfile() {
    local tmpfile
    tmpfile=$(mktemp)
    trap "rm -f '$tmpfile'" RETURN    # limpia cuando la función retorna

    echo "Working with $tmpfile"
    "$@" "$tmpfile"                    # pasar tmpfile al comando
    # tmpfile se limpia automáticamente al salir de la función
}

process_data() {
    local tmpfile="$1"
    echo "data" > "$tmpfile"
    wc -l "$tmpfile"
}

with_tempfile process_data
# tmpfile ya fue eliminado aquí
```

**Cuidado**: `trap ... RETURN` en una función **reemplaza** el trap RETURN
global (si existe). En Bash 4.4+ con `shopt -s localtraps`, los traps en
funciones son locales:

```bash
shopt -s localtraps    # los traps de función no sobreescriben los globales

outer() {
    trap 'echo "outer return"' RETURN
    inner
}

inner() {
    trap 'echo "inner return"' RETURN
    echo "inside inner"
}

outer
# inside inner
# inner return
# outer return
```

---

## 10. Múltiples traps y orden de ejecución

### 10.1 Sobreescribir traps

Cada `trap` para la misma señal **reemplaza** al anterior:

```bash
trap 'echo "first"' EXIT
trap 'echo "second"' EXIT   # reemplaza el primero

# Al terminar: solo imprime "second"
```

Para acumular acciones, incluir la anterior manualmente:

```bash
trap 'echo "first"' EXIT
trap 'echo "first"; echo "second"' EXIT   # incluir ambas
```

### 10.2 Patrón de cleanup acumulativo

```bash
CLEANUP_ACTIONS=()

add_cleanup() {
    CLEANUP_ACTIONS+=("$1")
}

run_cleanup() {
    # Ejecutar en orden INVERSO (LIFO, como atexit en C)
    local i
    for (( i=${#CLEANUP_ACTIONS[@]}-1; i>=0; i-- )); do
        eval "${CLEANUP_ACTIONS[$i]}" || true
    done
}

trap run_cleanup EXIT

# Usar durante el script:
tmpfile=$(mktemp)
add_cleanup "rm -f '$tmpfile'"

tmpdir=$(mktemp -d)
add_cleanup "rm -rf '$tmpdir'"

# Al terminar: primero rm -rf tmpdir, luego rm -f tmpfile (LIFO)
```

### 10.3 Orden: señal → EXIT

Cuando el script recibe una señal atrapada:

```bash
trap 'echo "INT handler"' INT
trap 'echo "EXIT handler"' EXIT

# Ctrl+C produce:
# INT handler
# EXIT handler
```

El handler de la señal se ejecuta primero, luego EXIT. EXIT siempre es el último.

---

## 11. Lock files

Un patrón común: asegurar que solo una instancia del script se ejecute:

### 11.1 Lock simple

```bash
#!/bin/bash
LOCK_FILE="/var/run/myscript.lock"

# Intentar crear lock
if ! mkdir "$LOCK_FILE" 2>/dev/null; then
    echo "Another instance is already running" >&2
    exit 1
fi

trap 'rmdir "$LOCK_FILE"' EXIT

# Script principal aquí...
echo "Running with lock"
sleep 10
```

`mkdir` es **atómico** en el filesystem: o lo crea o falla, sin race condition.
Es más seguro que verificar-luego-crear con `if [[ -f lock ]]; then ...`.

### 11.2 Lock con PID (stale lock detection)

```bash
#!/bin/bash
LOCK_FILE="/var/run/myscript.lock"

acquire_lock() {
    if mkdir "$LOCK_FILE" 2>/dev/null; then
        echo $$ > "$LOCK_FILE/pid"
        return 0
    fi

    # Lock existe — ¿el proceso aún vive?
    local old_pid
    old_pid=$(cat "$LOCK_FILE/pid" 2>/dev/null) || old_pid=""

    if [[ -n "$old_pid" ]] && ! kill -0 "$old_pid" 2>/dev/null; then
        echo "Removing stale lock (PID $old_pid is dead)" >&2
        rm -rf "$LOCK_FILE"
        mkdir "$LOCK_FILE"
        echo $$ > "$LOCK_FILE/pid"
        return 0
    fi

    echo "Another instance is running (PID $old_pid)" >&2
    return 1
}

release_lock() {
    rm -rf "$LOCK_FILE"
}

acquire_lock || exit 1
trap release_lock EXIT

# Script principal...
```

### 11.3 flock (Linux)

La herramienta `flock` es la forma más robusta:

```bash
#!/bin/bash
LOCK_FILE="/var/run/myscript.lock"

exec 200>"$LOCK_FILE"
if ! flock -n 200; then
    echo "Another instance is running" >&2
    exit 1
fi
trap 'rm -f "$LOCK_FILE"' EXIT

# Script principal...
# El lock se libera automáticamente al cerrar fd 200 (o al terminar)
```

```
flock ventajas sobre mkdir:
- Se libera automáticamente si el proceso muere (sin stale locks)
- Es un advisory lock del kernel, no un truco del filesystem
- -n = non-blocking (falla inmediatamente si no puede adquirir)
- Sin -n = blocking (espera hasta que el lock esté disponible)
```

---

## 12. Patrones de cleanup reales

### 12.1 Build script con artifacts

```bash
#!/bin/bash
set -euo pipefail

BUILD_DIR=""
cleanup() {
    if [[ -d "$BUILD_DIR" ]]; then
        echo "Cleaning build artifacts..." >&2
        rm -rf "$BUILD_DIR"
    fi
}
trap cleanup EXIT

BUILD_DIR=$(mktemp -d)
echo "Building in $BUILD_DIR"

git archive HEAD | tar -x -C "$BUILD_DIR"
make -C "$BUILD_DIR"
cp "$BUILD_DIR/output" ./result

echo "Build successful"
```

### 12.2 Script que lanza servicios temporales

```bash
#!/bin/bash
set -euo pipefail

PIDS=()

cleanup() {
    echo "Shutting down services..." >&2
    for pid in "${PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null
            wait "$pid" 2>/dev/null || true
        fi
    done
}
trap cleanup EXIT

# Lanzar servicios
python3 -m http.server 8080 &
PIDS+=($!)

redis-server --port 6380 &
PIDS+=($!)

echo "Services running (PIDs: ${PIDS[*]})"
echo "Press Ctrl+C to stop"

# Esperar a que todos terminen (o hasta Ctrl+C)
wait
```

### 12.3 Database migration con rollback

```bash
#!/bin/bash
set -euo pipefail

BACKUP_FILE=""

cleanup() {
    local exit_code=$?
    if (( exit_code != 0 )) && [[ -f "$BACKUP_FILE" ]]; then
        echo "Migration failed! Rolling back..." >&2
        pg_restore -d mydb "$BACKUP_FILE"
        echo "Rollback complete" >&2
    fi
    rm -f "$BACKUP_FILE"
}
trap cleanup EXIT

# Backup antes de migrar
BACKUP_FILE=$(mktemp)
pg_dump mydb > "$BACKUP_FILE"

# Migrar (si falla, cleanup hace rollback)
psql mydb < migration.sql

echo "Migration successful"
```

---

## 13. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| Trap solo INT, no EXIT | Ctrl+C limpia, pero errores no | Usar EXIT (cubre todo) |
| `trap 'rm $tmpfile'` sin comillas en var | `$tmpfile` se expande al definir el trap, no al ejecutar | Comillas simples: `trap 'rm "$tmpfile"'` |
| No preservar exit code en cleanup | cleanup retorna 0, ocultando el error | `local ec=$?; ...; exit $ec` |
| Lock file sin cleanup | Script muere → stale lock → no puede re-ejecutar | `trap 'rm -f "$LOCK"' EXIT` |
| `trap '' INT` (ignorar) vs `trap - INT` (reset) | Ignorar ≠ restaurar | `''` ignora la señal, `-` restaura default |
| trap en subshell | Subshells no heredan traps | Redefinir trap dentro del subshell |
| mktemp falla silenciosamente | Sin `set -e`, continúa con tmpfile vacío | `tmpfile=$(mktemp) \|\| exit 1` |
| `rm -rf "$var"` con var vacía | `rm -rf /` si var está vacía y concatenas | `rm -rf "${var:?}"` (falla si vacía) |
| ERR trap sin set -e | ERR se dispara pero el script continúa | Combinar `trap ... ERR` + `set -e` |
| DEBUG trap en producción | Overhead masivo (ejecuta antes de cada comando) | Solo para debugging temporal |

---

## 14. Ejercicios

### Ejercicio 1 — trap EXIT básico

**Predicción**: ¿qué imprime este script al ejecutar normalmente?

```bash
#!/bin/bash
trap 'echo "Cleanup!"' EXIT

echo "Working..."
echo "Done."
```

<details>
<summary>Respuesta</summary>

```
Working...
Done.
Cleanup!
```

`trap EXIT` se ejecuta al terminar el script, incluso si termina
normalmente con éxito. La secuencia es: ejecutar todo el script, luego
el trap EXIT al final.
</details>

---

### Ejercicio 2 — EXIT con error

**Predicción**: ¿qué imprime si `false` causa exit por `set -e`?

```bash
#!/bin/bash
set -e
trap 'echo "Exit code: $?"' EXIT

echo "Before"
false
echo "After"
```

<details>
<summary>Respuesta</summary>

```
Before
Exit code: 1
```

1. "Before" se imprime normalmente
2. `false` retorna exit code 1
3. `set -e` termina el script inmediatamente
4. "After" nunca se ejecuta
5. trap EXIT se ejecuta con `$?` = 1 (el exit code que causó la terminación)

EXIT se ejecuta **siempre**, sin importar la causa de terminación. `$?`
dentro del trap refleja el exit code del script.
</details>

---

### Ejercicio 3 — Comillas en trap

**Predicción**: ¿cuál es la diferencia?

```bash
file="original.txt"

trap "echo Cleaning $file" EXIT    # Trap A
file="changed.txt"
exit 0

# vs

file="original.txt"
trap 'echo Cleaning $file' EXIT    # Trap B
file="changed.txt"
exit 0
```

<details>
<summary>Respuesta</summary>

**Trap A** (comillas dobles): imprime `Cleaning original.txt`
- `$file` se expande **cuando se define el trap**
- El trap almacena: `echo Cleaning original.txt`

**Trap B** (comillas simples): imprime `Cleaning changed.txt`
- `$file` NO se expande al definir (comillas simples protegen)
- Se expande **cuando se ejecuta el trap** (al salir)
- En ese momento, `file="changed.txt"`

```
Comillas dobles: expansión al DEFINIR (early binding)
Comillas simples: expansión al EJECUTAR (late binding)
```

Generalmente quieres comillas simples — el valor de la variable al momento
del cleanup suele ser el correcto (ej: `$tmpfile` se asignó después del trap).
</details>

---

### Ejercicio 4 — Orden de señales

**Predicción**: si el usuario presiona Ctrl+C, ¿en qué orden se imprimen?

```bash
#!/bin/bash
trap 'echo "INT"' INT
trap 'echo "EXIT"' EXIT

echo "Waiting..."
sleep 100
```

<details>
<summary>Respuesta</summary>

```
Waiting...
^CINT
EXIT
```

1. "Waiting..." se imprime
2. `sleep 100` espera
3. Ctrl+C envía SIGINT
4. `sleep` muere por SIGINT
5. Trap INT se ejecuta → "INT"
6. El script termina (SIGINT sale del sleep, script continúa después de sleep, llega al final)
7. Trap EXIT se ejecuta → "EXIT"

Orden: primero el handler de la señal específica, luego EXIT. EXIT siempre
es el último. Esto permite que el handler INT haga algo específico (ej: log)
y EXIT haga la limpieza general.
</details>

---

### Ejercicio 5 — Tempfile seguro

**Predicción**: ¿hay algún problema con este script?

```bash
#!/bin/bash
tmpfile="/tmp/myapp_data"
trap 'rm -f "$tmpfile"' EXIT

echo "sensitive data" > "$tmpfile"
process "$tmpfile"
```

<details>
<summary>Respuesta</summary>

Dos problemas:

1. **Nombre predecible**: `/tmp/myapp_data` es siempre el mismo. Un atacante
   puede crear un symlink: `ln -s /etc/shadow /tmp/myapp_data`. Cuando el script
   escribe a `/tmp/myapp_data`, realmente escribe a `/etc/shadow`.

2. **Race condition**: entre scripts. Si dos instancias corren simultáneamente,
   ambas escriben al mismo archivo, corrompiendo los datos.

Solución:
```bash
tmpfile=$(mktemp /tmp/myapp.XXXXXX) || exit 1
trap 'rm -f "$tmpfile"' EXIT
```

`mktemp` genera un nombre aleatorio impredecible y crea el archivo
atómicamente con permisos 0600 (solo el dueño).
</details>

---

### Ejercicio 6 — Lock con mkdir

**Predicción**: ¿por qué `mkdir` es más seguro que este approach?

```bash
# Approach inseguro:
if [[ ! -f /tmp/myapp.lock ]]; then
    touch /tmp/myapp.lock
    # ... trabajo ...
    rm /tmp/myapp.lock
fi
```

<details>
<summary>Respuesta</summary>

**Race condition** (TOCTOU — Time Of Check To Time Of Use):

```
Tiempo   Proceso A                    Proceso B
─────────────────────────────────────────────────
  t1     if [[ ! -f lock ]] → true
  t2                                  if [[ ! -f lock ]] → true
  t3     touch lock
  t4                                  touch lock
  t5     # Ambos creen tener el lock!
```

Entre verificar (`[[ ! -f lock ]]`) y crear (`touch lock`), otro proceso
puede colarse. Son dos operaciones separadas, no atómicas.

`mkdir` es **atómico** en el kernel:
```bash
if mkdir /tmp/myapp.lock 2>/dev/null; then
    # Solo un proceso llega aquí
fi
```

`mkdir` falla si el directorio ya existe — verificación y creación en
una sola operación, sin ventana de race condition.
</details>

---

### Ejercicio 7 — Cleanup con PID

**Predicción**: ¿qué pasa si el proceso background muere antes del cleanup?

```bash
#!/bin/bash
trap 'kill $BG_PID 2>/dev/null' EXIT

some_server &
BG_PID=$!

sleep 5
# some_server ya terminó naturalmente antes de los 5 segundos
```

<details>
<summary>Respuesta</summary>

No pasa nada malo — el `kill` falla silenciosamente gracias a `2>/dev/null`.

```
Secuencia:
1. some_server se lanza en background, PID guardado
2. sleep 5 espera
3. some_server termina solo (antes de los 5s)
4. Script llega al final
5. trap EXIT: kill $BG_PID → proceso ya no existe → kill falla
6. 2>/dev/null → el error se suprime
```

Esto es correcto: el cleanup es **defensivo**. Intenta matar el proceso, pero
si ya murió, no importa.

Para ser más robusto, verificar primero:
```bash
cleanup() {
    if kill -0 "$BG_PID" 2>/dev/null; then
        kill "$BG_PID"
        wait "$BG_PID" 2>/dev/null || true
    fi
}
```

`kill -0` envía señal 0 (no hace nada) para verificar si el proceso existe.
</details>

---

### Ejercicio 8 — ERR trap scope

**Predicción**: ¿cuántas veces se imprime "ERR"?

```bash
#!/bin/bash

trap 'echo "ERR"' ERR

false                          # línea A
if false; then echo "yes"; fi  # línea B
false || true                  # línea C
! false                        # línea D
```

<details>
<summary>Respuesta</summary>

Se imprime **1 vez** (solo por la línea A).

- **Línea A**: `false` → exit code 1 → ERR se dispara ✓
- **Línea B**: `false` en condición de `if` → ERR **no** se dispara
  (fallar en una condición es esperado, no es un error)
- **Línea C**: `false || true` → false falla, pero `||` lo captura → ERR no se dispara
- **Línea D**: `! false` → negación explícita → ERR no se dispara

ERR solo se dispara cuando un comando falla **inesperadamente** — es decir,
cuando su exit code no es manejado por `if`, `||`, `&&`, `while`, `until`,
o `!`.
</details>

---

### Ejercicio 9 — Cleanup acumulativo

**Predicción**: ¿en qué orden se imprime el cleanup?

```bash
CLEANUP=()
add_cleanup() { CLEANUP+=("$1"); }
run_cleanup() {
    for (( i=${#CLEANUP[@]}-1; i>=0; i-- )); do
        echo "Cleanup: ${CLEANUP[$i]}"
    done
}
trap run_cleanup EXIT

add_cleanup "remove tmpfile"
add_cleanup "stop server"
add_cleanup "release lock"

exit 0
```

<details>
<summary>Respuesta</summary>

```
Cleanup: release lock
Cleanup: stop server
Cleanup: remove tmpfile
```

Orden LIFO (Last In, First Out): el último recurso adquirido se libera
primero. Esto es igual que:
- `atexit` en C (LIFO)
- `Drop` en Rust (orden inverso a la creación)
- Destructores en C++ (inverso a construcción)

El loop itera el array en reversa (`i` decrece). Esto es el orden correcto
porque los recursos adquiridos después pueden depender de los anteriores
(ej: el server usa el tmpfile, así que el server debe morir antes de
borrar el tmpfile).
</details>

---

### Ejercicio 10 — Preservar exit code

**Predicción**: ¿cuál es el exit code final del script?

```bash
#!/bin/bash

# Versión A:
cleanup_bad() {
    rm -f /tmp/somefile    # esto retorna 0
}
trap cleanup_bad EXIT

false    # exit code 1

# Versión B:
cleanup_good() {
    local ec=$?
    rm -f /tmp/somefile
    exit $ec
}
trap cleanup_good EXIT

false    # exit code 1
```

<details>
<summary>Respuesta</summary>

- **Versión A**: exit code = **0**. El cleanup ejecuta `rm -f` que retorna 0.
  Como el trap EXIT es lo último que se ejecuta, su exit code se convierte
  en el exit code del script. El `false` original (exit 1) se pierde.

- **Versión B**: exit code = **1**. El cleanup captura `$?` (= 1) al inicio,
  hace la limpieza, y luego `exit $ec` restaura el exit code original.

```
Versión A:
  false → $? = 1 → trap EXIT → rm (exit 0) → script exit = 0 ✗

Versión B:
  false → $? = 1 → trap EXIT → ec=1, rm, exit 1 → script exit = 1 ✓
```

**Siempre** capturar `$?` al inicio del cleanup y re-emitirlo al final.
De lo contrario, un script que falló puede reportar éxito al caller.
</details>