# set -euo pipefail

## 1. El problema — scripts silenciosamente rotos

Por defecto, Bash **ignora errores** y **continúa ejecutando** el resto del script.
Esto significa que un fallo a mitad del script puede pasar desapercibido, dejando
el sistema en un estado inconsistente:

```bash
#!/bin/bash
# SIN protección — script peligroso
rm -rf /tmp/build
mkdir /tmp/build
cd /tmp/buildd          # ← TYPO: directorio no existe
                        # cd falla, pero el script CONTINÚA
rm -rf *                # ← Se ejecuta en el directorio ACTUAL (catastrófico)
```

El `cd` falla silenciosamente (`exit code 1`), pero Bash continúa. El `rm -rf *`
se ejecuta en el directorio donde estabas antes — no en `/tmp/buildd`.

La solución estándar de la industria:

```bash
#!/bin/bash
set -euo pipefail
```

Esta línea activa tres protecciones independientes que, combinadas, convierten
Bash de un lenguaje peligrosamente permisivo a uno razonablemente seguro.

```
    Script sin protección          Script con set -euo pipefail
    ┌────────────────────┐         ┌────────────────────┐
    │ cmd1  ✓            │         │ cmd1  ✓            │
    │ cmd2  ✗ (fallo)    │         │ cmd2  ✗ (fallo)    │
    │ cmd3  ✓ ← ejecuta  │         │ ── ABORT ──        │
    │ cmd4  ✓ ← ejecuta  │         │ cmd3  (no ejecuta) │
    │ exit 0  ← "éxito"  │         │ exit 1  ← fallo    │
    └────────────────────┘         └────────────────────┘
```

---

## 2. set -e (errexit)

### 2.1 Qué hace

Termina el script inmediatamente si **cualquier comando** retorna un exit code
distinto de cero:

```bash
#!/bin/bash
set -e

echo "Paso 1"
false              # exit code 1 → script se detiene aquí
echo "Paso 2"     # NUNCA se ejecuta
```

```bash
# Equivalente a poner esto después de CADA comando:
comando || exit $?
```

### 2.2 Cuándo se activa

`set -e` aborta el script cuando un comando falla en estas situaciones:

```bash
set -e

# ✗ ABORTA: comando simple que falla
ls /no/existe

# ✗ ABORTA: último comando de un pipeline (sin pipefail)
echo "ok" | grep "nope"

# ✗ ABORTA: fallo en subshell
$(false)

# ✗ ABORTA: fallo en command substitution usado en asignación
result=$(cat /no/existe)
```

### 2.3 Cuándo NO se activa

`set -e` tiene excepciones importantes — NO aborta en estos casos:

```bash
set -e

# ✓ NO aborta: comando en condición de if
if grep -q "pattern" file.txt; then    # grep falla → if toma rama else
  echo "found"
else
  echo "not found"                     # se ejecuta normalmente
fi

# ✓ NO aborta: lado izquierdo de && o ||
false && echo "no se imprime"          # false falla, pero es parte de &&
echo "esto SÍ se ejecuta"

# ✓ NO aborta: comando negado con !
! false                                # !false = true → exit code 0

# ✓ NO aborta: comandos en pipeline (excepto el último, sin pipefail)
false | true                           # false falla pero true es el último → OK

# ✓ NO aborta: while/until condition
while ! check_ready; do               # check_ready falla → loop continúa
  sleep 1
done
```

### 2.4 La regla completa

```
set -e ABORTA si:
  1. Un comando simple falla (exit code ≠ 0)
  2. Y NO está en:
     - Condición de if/elif
     - Lado izquierdo de && o ||
     - Comando negado con !
     - Condición de while/until
     - Pipeline (excepto el último cmd, o con pipefail)
```

### 2.5 Trampas de set -e

```bash
set -e

# TRAMPA 1: fallo en subshell de asignación NO aborta si hay local
my_func() {
  local result=$(false)    # ← NO aborta — local tiene exit code 0
  echo "continúa"         # se ejecuta
}

# SOLUCIÓN: separar local de asignación
my_func_safe() {
  local result
  result=$(false)          # ← SÍ aborta — el exit code es de false
  echo "no llega aquí"
}

# TRAMPA 2: command substitution en argumentos
echo "Value: $(cat /no/existe)"    # ← SÍ aborta (falla cat)

# TRAMPA 3: funciones en condición de if
check() {
  false                    # falla
  echo "after false"       # ¿se ejecuta?
}
if check; then             # ← dentro de if, set -e se DESACTIVA para toda la función
  echo "yes"
fi
# "after false" SÍ se imprime — set -e no aplica dentro del if
```

La trampa 3 es la más peligrosa: cuando una función se usa como condición de `if`,
`set -e` se desactiva **dentro de toda la función**, no solo para el `if`.

---

## 3. set -u (nounset)

### 3.1 Qué hace

Trata el uso de **variables no definidas** como un error:

```bash
#!/bin/bash
set -u

echo "$NOMBRE"     # ERROR: NOMBRE: unbound variable
```

Sin `set -u`, `$NOMBRE` se expandiría silenciosamente a cadena vacía `""`.

### 3.2 Por qué importa

```bash
# Sin set -u — bug silencioso
#!/bin/bash
data_dir="$DTAA_DIR"    # TYPO: DTAA_DIR no existe → data_dir=""
rm -rf "$data_dir/"     # rm -rf "/"  ← CATASTRÓFICO (dependiendo del shell/rm)
```

Con `set -u`, el script aborta inmediatamente al intentar usar `$DTAA_DIR`.

### 3.3 Variables con valor por defecto

`set -u` no prohíbe que una variable sea vacía — prohíbe que **no esté definida**.
Para manejar variables opcionales, usa expansiones de parámetros:

```bash
set -u

# ${var:-default} → usa default si var no está definida O está vacía
echo "${OPTIONAL_VAR:-default_value}"

# ${var-default} → usa default solo si var no está definida (vacía está OK)
echo "${MAYBE_EMPTY-default_value}"

# ${var:?message} → error con mensaje si no definida o vacía
echo "${REQUIRED_VAR:?REQUIRED_VAR must be set}"

# ${var+value} → usa value si var ESTÁ definida (existe)
echo "${DEBUG+--verbose}"    # si DEBUG existe → "--verbose", si no → ""
```

### 3.4 Casos especiales

```bash
set -u

# $@ y $* son seguros incluso sin argumentos
my_func() {
  echo "Args: $@"     # OK, incluso si no hay argumentos
}
my_func               # no aborta

# Arrays vacíos — CUIDADO (Bash < 4.4)
declare -a my_array=()
echo "${my_array[@]}"            # ERROR en Bash < 4.4: unbound variable

# Solución para Bash < 4.4:
echo "${my_array[@]+"${my_array[@]}"}"   # feo pero funciona

# En Bash >= 4.4: arrays vacíos funcionan correctamente con set -u

# Verificar si variable está definida (sin triggear set -u)
if [[ -v MY_VAR ]]; then        # -v = "is variable set?" (Bash 4.2+)
  echo "MY_VAR = $MY_VAR"
else
  echo "MY_VAR not set"
fi

# Alternativa para Bash más antiguo:
if [[ "${MY_VAR+x}" == "x" ]]; then
  echo "MY_VAR está definida"
fi
```

### 3.5 Tabla de expansiones con set -u

| Expansión | Var no definida | Var vacía (`""`) | Var con valor |
|-----------|----------------|-----------------|---------------|
| `${var:-default}` | `default` | `default` | `valor` |
| `${var-default}` | `default` | `""` | `valor` |
| `${var:=default}` | asigna `default` | asigna `default` | `valor` |
| `${var=default}` | asigna `default` | `""` | `valor` |
| `${var:?err}` | **ERROR** | **ERROR** | `valor` |
| `${var?err}` | **ERROR** | `""` | `valor` |
| `${var:+alt}` | `""` | `""` | `alt` |
| `${var+alt}` | `""` | `alt` | `alt` |

---

## 4. set -o pipefail

### 4.1 Qué hace

Sin `pipefail`, el exit code de un pipeline es el exit code del **último comando**:

```bash
# Sin pipefail
false | true
echo $?    # 0 — ¡el fallo de false se perdió!

# Con pipefail
set -o pipefail
false | true
echo $?    # 1 — el fallo de false se propaga
```

**Regla con pipefail**: el exit code del pipeline es el del **último comando que
falló** (el más a la derecha que retornó ≠ 0), o 0 si todos tuvieron éxito.

### 4.2 Por qué importa

```bash
# Caso real: grep en pipeline
set -e       # errexit activado, pipefail NO

# ¿curl falló? No lo sabremos...
curl -s "https://api.example.com/data" | jq '.result'
echo "Continuando..."    # se ejecuta incluso si curl falló

# Con pipefail:
set -eo pipefail
curl -s "https://api.example.com/data" | jq '.result'
# Si curl falla → pipeline falla → script aborta
```

### 4.3 Comportamiento detallado

```bash
set -o pipefail

# Exit code = último fallo (más a la derecha)
false | false | true
echo $?    # 1 (del segundo false — el más a la derecha que falló)

true | false | true
echo $?    # 1 (del false en el medio)

false | true | false
echo $?    # 1 (del último false)

true | true | true
echo $?    # 0 (todos exitosos)
```

### 4.4 Interacción con set -e

`pipefail` + `errexit` juntos hacen que un pipeline con **cualquier fallo** aborte
el script:

```bash
set -eo pipefail

# grep retorna 1 si no encuentra nada
cat access.log | grep "ERROR" | wc -l
# Si no hay errores en el log:
#   cat → 0 (ok)
#   grep → 1 (no encontró nada)
#   wc → 0 (recibió stdin vacío, imprime "0")
#   Pipeline exit code = 1 (por grep) → SCRIPT ABORTA
```

Este es el caso más común donde `pipefail` causa problemas inesperados. La solución
está en la sección 5 (cuándo relajar).

### 4.5 PIPESTATUS

Bash guarda el exit code de **cada comando** del pipeline en el array `$PIPESTATUS`:

```bash
set -o pipefail

false | true | false
echo "${PIPESTATUS[@]}"    # 1 0 1
echo "${PIPESTATUS[0]}"    # 1 (primer comando)
echo "${PIPESTATUS[1]}"    # 0 (segundo)
echo "${PIPESTATUS[2]}"    # 1 (tercero)

# Uso práctico: verificar qué parte del pipeline falló
curl -s "$url" | jq '.data'
if (( PIPESTATUS[0] != 0 )); then
  echo "curl falló" >&2
elif (( PIPESTATUS[1] != 0 )); then
  echo "jq falló (¿JSON inválido?)" >&2
fi
```

---

## 5. Cuándo relajar — manejo controlado de fallos

`set -euo pipefail` es la configuración **por defecto** para todo script serio.
Pero hay situaciones legítimas donde un fallo es esperado. En esos casos, no
desactivas la protección — la relajas de forma controlada.

### 5.1 `|| true` — ignorar fallo de un comando específico

```bash
set -euo pipefail

# grep retorna 1 si no encuentra nada — es comportamiento normal, no error
count=$(grep -c "ERROR" log.txt || true)
echo "Errores: ${count:-0}"

# rm de algo que puede no existir
rm -f /tmp/lockfile || true    # -f ya maneja esto, pero el patrón es útil

# Comando que puede fallar legítimamente
docker stop my-container 2>/dev/null || true
```

### 5.2 `if` para capturar el resultado

```bash
set -euo pipefail

# En vez de || true, usar if cuando necesitas el resultado
if output=$(grep "PATTERN" file.txt 2>/dev/null); then
  echo "Encontrado: $output"
else
  echo "No encontrado"
fi

# Verificar si comando existe
if command -v docker &>/dev/null; then
  echo "Docker disponible"
else
  echo "Docker no instalado" >&2
  exit 1
fi
```

### 5.3 Desactivar temporalmente set -e

```bash
set -euo pipefail

# Desactivar para una sección
set +e    # desactivar errexit
risky_command
result=$?
set -e    # reactivar errexit

if (( result != 0 )); then
  echo "risky_command falló con código $result" >&2
fi
```

**Usar con moderación** — cada `set +e` es un punto donde errores pueden pasar
desapercibidos. Preferir `|| true` o `if` cuando sea posible.

### 5.4 Pipeline con grep — el caso más común

```bash
set -eo pipefail

# PROBLEMA: grep retorna 1 cuando no hay match → pipeline falla → script aborta
error_count=$(cat log.txt | grep -c "ERROR")    # aborta si no hay errores

# SOLUCIÓN 1: || true en el grep
error_count=$(grep -c "ERROR" log.txt || true)

# SOLUCIÓN 2: usar if
if error_count=$(grep -c "ERROR" log.txt); then
  echo "Hay $error_count errores"
else
  echo "No hay errores"
fi

# SOLUCIÓN 3: || echo "0" para default
error_count=$(grep -c "ERROR" log.txt || echo "0")

# SOLUCIÓN 4: awk (nunca falla por no encontrar match)
error_count=$(awk '/ERROR/ { c++ } END { print c+0 }' log.txt)
```

### 5.5 Patrón: cleanup con trap

```bash
#!/bin/bash
set -euo pipefail

tmpdir=$(mktemp -d)

# Limpiar aunque el script falle (trap se ejecuta con set -e)
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT    # EXIT se ejecuta siempre: éxito, error, o señal

# El script puede fallar en cualquier punto — cleanup siempre se ejecuta
curl -o "$tmpdir/data.json" "https://api.example.com/data"
jq '.results' "$tmpdir/data.json" > "$tmpdir/processed.json"
cp "$tmpdir/processed.json" /final/destination/
```

### 5.6 Patrón: error handler personalizado

```bash
#!/bin/bash
set -euo pipefail

on_error() {
  local exit_code=$?
  local line_no=$1
  echo "ERROR: script falló en línea $line_no con código $exit_code" >&2
  # Opcional: logging, notificación, cleanup
}
trap 'on_error $LINENO' ERR    # ERR trap: solo en errores, no en exit normal

echo "Paso 1: preparar"
false                          # falla → trap ERR → mensaje con número de línea
echo "Paso 2: nunca llega"
```

```
$ ./script.sh
Paso 1: preparar
ERROR: script falló en línea 12 con código 1
```

---

## 6. El template completo para scripts profesionales

```bash
#!/bin/bash
#
# script-name.sh — Breve descripción de lo que hace
#
# Uso:
#   ./script-name.sh [opciones] <argumento>
#
# Opciones:
#   -v    Verbose
#   -d    Dry run
#   -h    Help
#

set -euo pipefail

# ─── Constants ──────────────────────────────────────────
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly SCRIPT_NAME="$(basename "${BASH_SOURCE[0]}")"

# ─── Defaults ───────────────────────────────────────────
VERBOSE=false
DRY_RUN=false

# ─── Functions ──────────────────────────────────────────
usage() {
  cat <<EOF
Usage: $SCRIPT_NAME [options] <argument>

Options:
  -v    Verbose output
  -d    Dry run (no changes)
  -h    Show this help
EOF
  exit "${1:-0}"
}

log() {
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >&2
}

die() {
  log "ERROR: $*"
  exit 1
}

cleanup() {
  # Liberar recursos temporales
  [[ -d "${tmpdir:-}" ]] && rm -rf "$tmpdir"
}

# ─── Traps ──────────────────────────────────────────────
trap cleanup EXIT

# ─── Parse arguments ────────────────────────────────────
while getopts ":vdh" opt; do
  case $opt in
    v) VERBOSE=true ;;
    d) DRY_RUN=true ;;
    h) usage 0 ;;
    :) die "Option -$OPTARG requires an argument" ;;
    *) die "Unknown option: -$OPTARG" ;;
  esac
done
shift $((OPTIND - 1))

# ─── Validate ──────────────────────────────────────────
(( $# >= 1 )) || die "Missing required argument. Use -h for help."
readonly TARGET="$1"

# ─── Main ──────────────────────────────────────────────
tmpdir=$(mktemp -d)

log "Starting: target=$TARGET"
$VERBOSE && log "Verbose mode enabled"
$DRY_RUN && log "Dry run mode — no changes will be made"

# ... lógica del script ...

log "Done"
```

```
¿Qué protege cada línea?

set -e         →  script aborta si un comando falla
set -u         →  script aborta si usas variable no definida
set -o pipefail →  fallos en pipelines no se ocultan
trap EXIT      →  cleanup siempre se ejecuta
readonly       →  constantes no se pueden modificar accidentalmente
die()          →  manejo de errores consistente con log
```

---

## 7. Comparación: con y sin protección

### Script sin protección

```bash
#!/bin/bash
echo "Descargando..."
curl -o /tmp/data.csv "$URL"
echo "Procesando..."
sort /tmp/data.csv > /tmp/sorted.csv
echo "Subiendo..."
scp /tmp/sorted.csv server:/data/
echo "¡Listo!"
rm /tmp/data.csv /tmp/sorted.csv
```

**Problemas**:
- Si `$URL` no está definida → `curl` descarga de URL vacía → silencioso
- Si `curl` falla → `sort` procesa archivo vacío o inexistente → resultado basura
- Si `scp` falla → se imprime "¡Listo!" y se borran los archivos → datos perdidos
- Exit code 0 siempre → CI/CD cree que todo fue bien

### Script con protección

```bash
#!/bin/bash
set -euo pipefail
trap 'rm -f /tmp/data.csv /tmp/sorted.csv' EXIT

: "${URL:?URL is required}"

echo "Descargando..."
curl -f -o /tmp/data.csv "$URL"     # -f = fail on HTTP errors

echo "Procesando..."
sort /tmp/data.csv > /tmp/sorted.csv

echo "Subiendo..."
scp /tmp/sorted.csv server:/data/

echo "¡Listo!"
```

**Protecciones activas**:
- `set -u` → `$URL` no definida aborta inmediatamente con mensaje claro
- `${URL:?}` → doble verificación con mensaje custom
- `set -e` → si `curl`, `sort`, o `scp` fallan → script aborta
- `curl -f` → falla en errores HTTP (4xx, 5xx)
- `trap EXIT` → archivos temporales se borran siempre (éxito o fallo)
- Exit code ≠ 0 en caso de fallo → CI/CD detecta el error

---

## 8. Opciones adicionales útiles

### 8.1 IFS — Internal Field Separator

```bash
# Setear IFS estricto — previene problemas con word splitting
IFS=$'\n\t'

# Default IFS = space + tab + newline
# Con solo newline + tab, los espacios en nombres de archivo no rompen loops:
for file in $(find . -name "*.txt"); do
  echo "$file"    # funciona con "my file.txt" si IFS no tiene espacio
done

# Nota: mejor usar find -print0 | while read -r -d '' de todas formas
```

### 8.2 shopt -s failglob

```bash
# Sin failglob: glob sin match se pasa literal
echo *.xyz       # si no hay .xyz: imprime "*.xyz" (literal)

# Con failglob: glob sin match es un error
shopt -s failglob
echo *.xyz       # ERROR: no match: *.xyz

# Útil para prevenir errores como:
rm *.tmp         # sin archivos .tmp → rm recibe "*.tmp" literal → error confuso
```

### 8.3 shopt -s inherit_errexit (Bash 4.4+)

```bash
# Sin inherit_errexit: command substitution ignora set -e
set -e
result=$(false; echo "no debería ejecutarse")
echo "$result"    # imprime "no debería ejecutarse"

# Con inherit_errexit: set -e se hereda en subshells
shopt -s inherit_errexit
set -e
result=$(false; echo "no debería ejecutarse")
# → script aborta en false dentro de $()
```

### 8.4 Combinación recomendada para Bash moderno (≥ 4.4)

```bash
#!/bin/bash
set -euo pipefail
shopt -s inherit_errexit failglob
```

---

## 9. Errores comunes

| Error | Causa | Solución |
|-------|-------|----------|
| Script aborta en `grep` | `grep` retorna 1 si no hay match | `grep ... \|\| true` o usar `if` |
| `local var=$(cmd)` no detecta fallo | `local` siempre retorna 0 | Separar: `local var; var=$(cmd)` |
| Variable vacía no detectada | `set -u` solo detecta **no definidas** | Usar `${var:?msg}` para rechazar vacías |
| `set -e` no funciona dentro de `if` | `if` desactiva `set -e` en la condición | Usar `\|\|` en vez de `if` para check, o capturar exit code |
| Pipeline falla por `head`/`grep` | `pipefail` propaga fallos del pipe | `\|\| true` en el comando que puede fallar |
| `$@` falla con `set -u` sin args | Bash < 4.4: `$@` sin args es unbound | Usar `"${@}"` o actualizar Bash |
| Array vacío falla con `set -u` | Bash < 4.4: array vacío es unbound | `${arr[@]+"${arr[@]}"}` o Bash ≥ 4.4 |
| Trap no se ejecuta | Trap definido después del error | Definir traps al inicio del script |
| `set -e` no aborta en función | Función llamada desde `if` o `\|\|` | Reestructurar: no usar funciones como condición de `if` |
| `read` retorna 1 al final | `read` retorna 1 en EOF | `read -r line \|\| true` o usar `while IFS= read -r` |

---

## 10. Diagrama de decisión

```
    ¿El comando puede fallar legítimamente?
                    │
          ┌─── NO ──┴── SÍ ───┐
          │                    │
    Dejar que set -e      ¿Necesitas el resultado?
    lo maneje                  │
                     ┌── NO ──┴── SÍ ──┐
                     │                  │
               cmd || true         if cmd; then
               (ignorar fallo)       usar resultado
                                   else
                                     manejar fallo
                                   fi

    ¿La variable puede no existir?
                    │
          ┌─── NO ──┴── SÍ ───┐
          │                    │
    Dejar que set -u      ${var:-default}
    lo detecte            ${var:?error msg}
                          [[ -v var ]]
```

---

## 11. Ejercicios

### Ejercicio 1 — Predecir el comportamiento

**Predicción**: ¿Qué imprime cada bloque?

```bash
#!/bin/bash
set -e
echo "A"
false
echo "B"
```

```bash
#!/bin/bash
echo "A"
false
echo "B"
```

<details>
<summary>Ver solución</summary>

Bloque 1 (con `set -e`):
```
A
```
`false` retorna exit code 1 → `set -e` aborta → "B" nunca se imprime.

Bloque 2 (sin `set -e`):
```
A
B
```
`false` retorna 1, pero Bash lo ignora y continúa → "B" se imprime.

El exit code del script 1 es `1` (fallo). El del script 2 es `0` (éxito, porque
el último comando `echo "B"` tuvo éxito).
</details>

---

### Ejercicio 2 — La trampa del if

**Predicción**: ¿Qué imprime?

```bash
#!/bin/bash
set -e

check() {
  false
  echo "después de false"
}

if check; then
  echo "check tuvo éxito"
else
  echo "check falló"
fi
```

<details>
<summary>Ver solución</summary>

```
después de false
check falló
```

Cuando una función se usa como condición de `if`, `set -e` se **desactiva dentro
de toda la función**. Por eso `false` no aborta — `echo "después de false"` se
ejecuta. La función `check` retorna el exit code del último comando (`echo` = 0),
pero como `false` no abortó, la función completa retorna 0... No. Espera.

Más precisamente: el exit code de la función es el del último comando ejecutado.
`echo "después de false"` retorna 0. Pero `check` está en un `if`, y el exit code
que evalúa el `if` es el de la función completa.

En realidad, `false` retorna 1, `echo "después de false"` retorna 0. El exit code
de la función es 0 (último comando). Entonces `if check` evalúa como true... pero
no. Hay que probarlo:

**Resultado real**:
```
después de false
check tuvo éxito
```

`false` no aborta (dentro de `if`, `set -e` está desactivado). `echo` se ejecuta y
retorna 0. La función retorna 0 (del último `echo`). El `if` toma la rama `then`.

**La lección**: `set -e` dentro de `if` está desactivado, lo que puede causar que
funciones parezcan exitosas cuando en realidad tuvieron fallos internos. Es la
trampa más peligrosa de `set -e`.
</details>

---

### Ejercicio 3 — set -u con variables

**Predicción**: ¿Cuáles de estos fallan con `set -u`?

```bash
#!/bin/bash
set -u

# Caso A
echo "${UNDEFINED_VAR:-default}"

# Caso B
echo "$UNDEFINED_VAR"

# Caso C
DEFINED_VAR=""
echo "$DEFINED_VAR"

# Caso D
unset REMOVED_VAR
echo "${REMOVED_VAR}"
```

<details>
<summary>Ver solución</summary>

- **Caso A**: **NO falla** — `${UNDEFINED_VAR:-default}` imprime `default`. La expansión `:-` maneja variables no definidas.
- **Caso B**: **FALLA** — `UNDEFINED_VAR: unbound variable`. Uso directo de variable no definida.
- **Caso C**: **NO falla** — `DEFINED_VAR` está definida (aunque vacía). `set -u` solo detecta no definidas, no vacías. Imprime línea vacía.
- **Caso D**: **FALLA** — `REMOVED_VAR: unbound variable`. `unset` elimina la variable, haciéndola no definida.

El script se detiene en **Caso B** (nunca llega a C ni D). El output real es:
```
default
./script.sh: line 7: UNDEFINED_VAR: unbound variable
```
</details>

---

### Ejercicio 4 — pipefail

**Predicción**: ¿Cuál es `$?` después de cada pipeline?

```bash
set -o pipefail

# A
true | false | true
echo "A: $?"

# B
false | true | true
echo "B: $?"

# C
true | true | true
echo "C: $?"

# D
false | false | true
echo "D: $?"
```

<details>
<summary>Ver solución</summary>

**Nota**: con `set -e` activado, el script abortaría en A. Estos ejemplos asumen
solo `pipefail` sin `errexit`, para ilustrar los exit codes.

```
A: 1
```
Pipeline A falla en `false` (posición 2). Con `pipefail`, el exit code es 1 (del
`false`). Con `set -e` activado, el script abortaría aquí.

Sin `set -e`:
- **A: 1** — `false` en posición 2 retorna 1, es el fallo más a la derecha.
- **B: 1** — `false` en posición 1 retorna 1.
- **C: 0** — todos exitosos.
- **D: 1** — ambos `false` retornan 1. El más a la derecha que falló (posición 2)
  determina el exit code.

`PIPESTATUS` para D sería: `(1 1 0)`.

La regla: el exit code del pipeline es el del **último comando (más a la derecha)
que tuvo exit code ≠ 0**. Si todos son 0, el pipeline retorna 0.
</details>

---

### Ejercicio 5 — La trampa del local

**Predicción**: ¿Qué imprime?

```bash
#!/bin/bash
set -euo pipefail

get_data() {
  cat /no/existe   # falla
}

# Versión A
func_a() {
  local result=$(get_data)
  echo "A llegó aquí: result=$result"
}

# Versión B
func_b() {
  local result
  result=$(get_data)
  echo "B llegó aquí: result=$result"
}

echo "Probando A..."
func_a
echo "Después de A"

echo "Probando B..."
func_b
echo "Después de B"
```

<details>
<summary>Ver solución</summary>

```
Probando A...
cat: /no/existe: No such file or directory
A llegó aquí: result=
Después de A
Probando B...
cat: /no/existe: No such file or directory
```

**Versión A**: `local result=$(get_data)` — el exit code de la línea es el de
`local`, que siempre es 0. El fallo de `get_data` (cat) se pierde. `result` queda
vacío, y el script continúa.

**Versión B**: `result=$(get_data)` — el exit code es el de `get_data` (1 por cat).
`set -e` detecta el fallo → aborta. "B llegó aquí" nunca se imprime, ni
"Después de B".

**Lección**: siempre separar `local` de la asignación cuando usas command
substitution con `set -e`. Esto es uno de los bugs más sutiles de Bash scripting.
</details>

---

### Ejercicio 6 — Grep en pipeline

**Predicción**: ¿Qué pasa con cada versión?

```bash
#!/bin/bash
set -euo pipefail

# Archivo de log sin errores
echo -e "INFO: todo bien\nINFO: perfecto" > /tmp/test.log

# Versión A
error_count=$(grep -c "ERROR" /tmp/test.log)
echo "A: $error_count errores"

# Versión B
error_count=$(grep -c "ERROR" /tmp/test.log || true)
echo "B: $error_count errores"

# Versión C
if error_count=$(grep -c "ERROR" /tmp/test.log); then
  echo "C: $error_count errores encontrados"
else
  echo "C: sin errores"
fi
```

<details>
<summary>Ver solución</summary>

**Versión A**: el script **aborta**. `grep -c "ERROR"` no encuentra nada → retorna 1.
Con `set -e`, exit code 1 = error fatal. "A: ..." nunca se imprime.

**Versión B**: imprime `B: 0 errores`. `grep -c` retorna 1, pero `|| true` convierte
el exit code a 0. `grep -c` cuando no encuentra match imprime `0` a stdout, que se
captura en `error_count`.

**Versión C**: imprime `C: sin errores`. `grep` falla (exit code 1) → el `if` toma
la rama `else`. Dentro de `if`, `set -e` no aborta. `error_count` contiene `0`
(output de `grep -c`).

Las tres versiones son correctas dependiendo del contexto:
- **A**: si la ausencia de "ERROR" es inesperada y debería detener el script
- **B**: si quieres continuar con el conteo (posiblemente 0)
- **C**: si quieres distinguir explícitamente entre "hay errores" y "no hay"
</details>

---

### Ejercicio 7 — Escribir un script robusto

Escribe un script que:
1. Reciba un URL como argumento obligatorio
2. Descargue el contenido a un archivo temporal
3. Cuente las líneas y muestre el resultado
4. Limpie el archivo temporal (incluso si algo falla)

**Predicción**: ¿Qué componentes de `set -euo pipefail` protegen cada paso?

<details>
<summary>Ver solución</summary>

```bash
#!/bin/bash
set -euo pipefail

# set -u protege aquí: si no pasamos argumento, $1 no existe
url="${1:?Usage: $0 <url>}"

tmpfile=$(mktemp)

# trap EXIT: limpieza siempre, incluso si curl o wc fallan (set -e aborta)
trap 'rm -f "$tmpfile"' EXIT

# set -e protege aquí: si curl falla, el script aborta
echo "Descargando: $url" >&2
curl -sf -o "$tmpfile" "$url"

# set -e protege aquí: si wc falla (improbable), aborta
lines=$(wc -l < "$tmpfile")

echo "Líneas descargadas: $lines"
```

Protecciones por componente:
- **`set -u`**: Si olvidamos pasar `$1`, error inmediato. Si hay typo en `$tmpfile`, error.
- **`set -e`**: Si `curl` falla (URL inválida, red caída) → aborta antes de contar.
- **`set -o pipefail`**: No aplica directamente aquí (no hay pipelines), pero si
  refactorizáramos a `curl ... | wc -l`, protegería el fallo de curl.
- **`trap EXIT`**: Si `curl` falla y `set -e` aborta, el trap borra `$tmpfile`.
- **`${1:?msg}`**: Doble protección — mensaje de error claro además de `set -u`.
- **`curl -f`**: Falla en errores HTTP (sin esto, curl retorna 0 en 404).
</details>

---

### Ejercicio 8 — Desactivación temporal

**Predicción**: ¿Qué imprime?

```bash
#!/bin/bash
set -euo pipefail

echo "Inicio"

set +e
false
echo "Después de false (set +e)"
set -e

echo "set -e reactivado"
false
echo "¿Se imprime esto?"
```

<details>
<summary>Ver solución</summary>

```
Inicio
Después de false (set +e)
set -e reactivado
```

- `set +e` desactiva `errexit` → `false` no aborta → "Después de false" se imprime.
- `set -e` reactiva `errexit`.
- El segundo `false` ocurre con `set -e` activo → script aborta.
- "¿Se imprime esto?" **nunca se imprime**.

El exit code del script es `1` (por el segundo `false`).

**Patrón correcto**: limitar el scope de `set +e` al mínimo necesario y siempre
reactivar `set -e` inmediatamente después.
</details>

---

### Ejercicio 9 — PIPESTATUS

**Predicción**: ¿Qué imprime?

```bash
#!/bin/bash
set -o pipefail
# (sin set -e para poder ver los resultados)

false | grep "x" | true
echo "Exit: $?"
echo "PIPESTATUS: ${PIPESTATUS[@]}"
```

<details>
<summary>Ver solución</summary>

```
Exit: 1
PIPESTATUS: 1 1 0
```

- `false` → exit code 1
- `grep "x"` → recibe stdin vacío (de false), no encuentra "x" → exit code 1
- `true` → exit code 0

Con `pipefail`, el exit code del pipeline es el del **último comando que falló**
(más a la derecha con exit ≠ 0). Ese es `grep` (posición 2, exit code 1).

`PIPESTATUS` guarda los tres exit codes: `(1 1 0)`.

Sin `pipefail`, el exit code sería `0` (de `true`, el último comando), y los
fallos de `false` y `grep` pasarían desapercibidos.
</details>

---

### Ejercicio 10 — Auditar un script

El siguiente script tiene **múltiples problemas** de robustez. Identifícalos y
corrígelos:

```bash
#!/bin/bash

BACKUP_DIR=$BKUP_DIR
DB_NAME=production

echo "Starting backup of $DB_NAME"
pg_dump $DB_NAME > /tmp/backup.sql
gzip /tmp/backup.sql
mv /tmp/backup.sql.gz $BACKUP_DIR/
echo "Backup complete"
```

**Predicción**: ¿Cuántos problemas puedes encontrar antes de ver la solución?

<details>
<summary>Ver solución</summary>

Problemas encontrados:

1. **No tiene `set -euo pipefail`** — errores pasan silenciosamente
2. **`$BKUP_DIR` puede no existir** — `BACKUP_DIR` queda vacío → `mv` a destino incorrecto
3. **Variables sin comillas** — si `BACKUP_DIR` tiene espacios, se rompe
4. **No hay `trap` para cleanup** — `/tmp/backup.sql` queda si algo falla
5. **Archivo temporal con nombre fijo** — otro proceso podría colisionar
6. **No verifica que `pg_dump` exista o tenga permisos**
7. **Exit code siempre 0** — CI/CD no detectaría fallos
8. **`mv` puede fallar** si `$BACKUP_DIR` no existe como directorio

Versión corregida:

```bash
#!/bin/bash
set -euo pipefail

# Validación: set -u detecta si BKUP_DIR no existe,
# :? da mensaje claro si está vacía
readonly BACKUP_DIR="${BKUP_DIR:?BKUP_DIR must be set}"
readonly DB_NAME="production"

# Verificar que el directorio destino existe
[[ -d "$BACKUP_DIR" ]] || { echo "ERROR: $BACKUP_DIR no existe" >&2; exit 1; }

# Archivo temporal seguro
tmpfile=$(mktemp /tmp/backup-XXXXXX.sql)
trap 'rm -f "$tmpfile" "${tmpfile}.gz"' EXIT

echo "Starting backup of $DB_NAME" >&2

# pg_dump con verificación (set -e aborta si falla)
pg_dump "$DB_NAME" > "$tmpfile"

gzip "$tmpfile"
mv "${tmpfile}.gz" "$BACKUP_DIR/"

echo "Backup complete: $BACKUP_DIR/$(basename "${tmpfile}.gz")" >&2
```
</details>
