# Debugging de scripts Bash

## 1. El problema — scripts que fallan sin explicación

Cuando un script Bash falla, el mensaje de error suele ser críptico o inexistente.
A diferencia de lenguajes compilados (C, Rust), Bash no tiene debugger integrado,
stack traces, ni información de tipos. Las herramientas de debugging son primitivas
pero sorprendentemente efectivas si se usan bien.

```
    Script falla
         │
    ¿Qué herramienta usar?
         │
    ┌────┼──────────┬──────────────┬──────────────┐
    │    │          │              │              │
    ▼    ▼          ▼              ▼              ▼
  set -x  bash -n   PS4 custom   trap DEBUG    bashdb
  trace   syntax    contexto     breakpoints   debugger
  cmds    check     en trace     condicionales completo
```

---

## 2. bash -n — verificación de sintaxis

### 2.1 Qué hace

`bash -n` parsea el script sin ejecutarlo. Detecta errores de sintaxis como
comillas sin cerrar, `if` sin `fi`, paréntesis desbalanceados, etc.

```bash
# Verificar sintaxis sin ejecutar
bash -n script.sh

# Si no hay errores: sin output, exit code 0
# Si hay errores: mensaje con línea, exit code 2

# Equivalente desde dentro del script
set -n    # activar noexec (no ejecutar, solo parsear)
```

### 2.2 Qué detecta

```bash
# Error: comillas sin cerrar
cat <<'EOF' > broken.sh
#!/bin/bash
echo "hello
echo "world"
EOF

bash -n broken.sh
# broken.sh: line 4: unexpected EOF while looking for matching `"'
# broken.sh: line 5: syntax error: unexpected end of file
```

```bash
# Error: if sin fi
cat <<'EOF' > broken2.sh
#!/bin/bash
if true; then
  echo "yes"
EOF

bash -n broken2.sh
# broken2.sh: line 4: syntax error: unexpected end of file
```

```bash
# Error: do sin done
cat <<'EOF' > broken3.sh
#!/bin/bash
for i in 1 2 3; do
  echo "$i"
EOF

bash -n broken3.sh
# broken3.sh: line 4: syntax error: unexpected end of file
```

### 2.3 Qué NO detecta

`bash -n` solo verifica **sintaxis**, no **semántica**:

```bash
cat <<'EOF' > semantic.sh
#!/bin/bash
# bash -n NO detecta estos problemas:
echo $var_sin_comillas      # word splitting (ShellCheck sí lo detecta)
cd /directorio/inexistente  # fallo en runtime
rm -rf $var_vacia/          # peligro en runtime
cat /dev/null | grep x      # UUOC (ShellCheck lo detecta)
undefined_function          # función no definida (fallo en runtime)
EOF

bash -n semantic.sh
# Sin output — "sintácticamente correcto" pero con múltiples problemas
```

### 2.4 Uso en CI/CD

```bash
# Verificar todos los scripts del proyecto
find . -name '*.sh' -print0 | while IFS= read -r -d '' f; do
  if ! bash -n "$f" 2>/dev/null; then
    echo "SYNTAX ERROR: $f" >&2
    bash -n "$f"    # mostrar el error
  fi
done

# bash -n es rápido — úsalo como primera línea de defensa antes de ShellCheck
```

```
    Pipeline de validación
    ┌──────────┐    ┌────────────┐    ┌──────────┐
    │ bash -n  │───▶│ ShellCheck │───▶│  Tests   │
    │ sintaxis │    │ semántica  │    │ runtime  │
    └──────────┘    └────────────┘    └──────────┘
      rápido         medio             lento
```

---

## 3. set -x (xtrace) — la herramienta principal

### 3.1 Qué hace

`set -x` imprime cada comando **antes de ejecutarlo**, mostrando la expansión
real de variables, globs y command substitutions. Es el equivalente de un
"printf debugging" automático para cada línea.

```bash
#!/bin/bash
set -x

name="Alice"
echo "Hello, $name"
```

```
+ name=Alice
+ echo 'Hello, Alice'
Hello, Alice
```

Cada línea con `+` es el **trace** — lo que Bash realmente ejecutó después de
expandir variables. La línea sin `+` es la salida del comando.

### 3.2 Activar y desactivar

```bash
# Opción 1: desde el shebang
#!/bin/bash -x

# Opción 2: con set
set -x          # activar xtrace
# ... comandos traceados ...
set +x          # desactivar xtrace

# Opción 3: desde la línea de comandos (sin modificar el script)
bash -x script.sh

# Opción 4: solo para una sección
#!/bin/bash
set -euo pipefail

echo "Esto NO se tracea"

set -x
echo "Esto SÍ se tracea"
problematic_function
set +x

echo "Esto tampoco se tracea"
```

### 3.3 Output a stderr

El trace va a **stderr** (`fd 2`), no a stdout. Esto permite separar la salida
del script de la información de debug:

```bash
# Salida normal a stdout, trace a stderr
bash -x script.sh > output.txt 2> trace.txt

# Ver solo el trace
bash -x script.sh > /dev/null

# Ver solo la salida normal
bash -x script.sh 2> /dev/null
```

### 3.4 Redirigir trace a un archivo

En Bash 4.1+, se puede redirigir el trace a un archivo específico usando `BASH_XTRACEFD`:

```bash
#!/bin/bash

# Abrir fd 5 para trace
exec 5> /tmp/trace.log
BASH_XTRACEFD=5

set -x
echo "Esto aparece en stdout"
name="world"
echo "Hello, $name"
set +x

# Cerrar fd
exec 5>&-
```

```bash
# stdout (pantalla):
# Esto aparece en stdout
# Hello, world

# /tmp/trace.log:
# + echo 'Esto aparece en stdout'
# + name=world
# + echo 'Hello, world'
# + set +x
```

Esto es especialmente útil en scripts que producen output formateado (tablas,
JSON, reportes) donde mezclar trace arruinaría la salida.

---

## 4. PS4 — personalizar el prompt de trace

### 4.1 El problema con el + default

El `+` por defecto no dice **dónde** ni **cuándo** se ejecutó un comando:

```
+ mkdir /tmp/build
+ cd /tmp/build
+ rm -rf .
```

Sin contexto, no sabes qué archivo, qué función, ni qué línea produjo cada comando.

### 4.2 PS4 con información útil

`PS4` es la variable que controla el prefijo de cada línea de trace:

```bash
# Formato recomendado: archivo:línea:función
export PS4='+${BASH_SOURCE[0]}:${LINENO}:${FUNCNAME[0]:+${FUNCNAME[0]}(): }'

set -x
echo "hello"
# +script.sh:5:: echo hello
```

### 4.3 Variables disponibles en PS4

| Variable | Contenido | Ejemplo |
|----------|-----------|---------|
| `$LINENO` | Número de línea actual | `42` |
| `${BASH_SOURCE[0]}` | Archivo actual | `script.sh` |
| `${FUNCNAME[0]}` | Función actual | `my_func` |
| `${FUNCNAME[0]:+...}` | Solo si está en una función | `my_func(): ` o vacío |
| `$SECONDS` | Segundos desde inicio del script | `3` |
| `$BASHPID` | PID del proceso actual | `12345` |
| `$(date +%T)` | Hora actual | `14:30:05` |

### 4.4 Configuraciones de PS4 recomendadas

```bash
# Mínimo útil: archivo y línea
PS4='+${BASH_SOURCE[0]##*/}:${LINENO}: '
# +script.sh:42: echo "hello"

# Con función (si aplica)
PS4='+${BASH_SOURCE[0]##*/}:${LINENO}:${FUNCNAME[0]:+${FUNCNAME[0]}(): }'
# +script.sh:42:process_file(): echo "hello"

# Con timestamp (útil para scripts lentos)
PS4='+[${SECONDS}s] ${BASH_SOURCE[0]##*/}:${LINENO}: '
# +[3s] script.sh:42: curl -s "https://..."

# Con PID (útil con subshells/background)
PS4='+[$$:$BASHPID] ${BASH_SOURCE[0]##*/}:${LINENO}: '
# +[1234:5678] script.sh:42: echo "hello"

# Completo — para debugging serio
PS4='+$(date +%H:%M:%S) ${BASH_SOURCE[0]##*/}:${LINENO}:${FUNCNAME[0]:-main}: '
# +14:30:05 script.sh:42:main: echo "hello"
```

### 4.5 PS4 con profundidad de subshell

El `+` se multiplica por la profundidad de subshell. Con PS4 default:

```bash
set -x
echo "level 0"
( echo "level 1"
  ( echo "level 2" ) )
```

```
+ echo 'level 0'
++ echo 'level 1'
+++ echo 'level 2'
```

Los `++` indican subshell nivel 1, `+++` nivel 2, etc. Esto ayuda a identificar
cuándo un comando se ejecuta en una subshell (pipelines, `$(...)`, `(...)`).

---

## 5. Xtrace selectivo

### 5.1 Tracear solo funciones específicas

```bash
#!/bin/bash
set -euo pipefail

process_file() {
  set -x
  local file="$1"
  wc -l < "$file"
  grep -c "ERROR" "$file" || true
  set +x
}

# El resto del script NO se tracea
for f in /var/log/*.log; do
  [[ -e "$f" ]] || continue
  echo "Processing: $f"
  count=$(process_file "$f")
  echo "  Lines: $count"
done
```

### 5.2 Tracear por condición (variable de entorno)

```bash
#!/bin/bash
set -euo pipefail

# Activar trace solo si DEBUG está definida
if [[ "${DEBUG:-}" == "1" ]]; then
  set -x
fi

# Uso:
# ./script.sh           → sin trace
# DEBUG=1 ./script.sh   → con trace
```

Patrón más sofisticado:

```bash
#!/bin/bash
set -euo pipefail

# Niveles de debug
debug_level="${DEBUG:-0}"

debug() {
  if (( debug_level >= 1 )); then
    echo "[DEBUG] $*" >&2
  fi
}

trace() {
  if (( debug_level >= 2 )); then
    echo "[TRACE] $*" >&2
  fi
}

# Uso:
# DEBUG=0 ./script.sh   → sin debug
# DEBUG=1 ./script.sh   → mensajes debug
# DEBUG=2 ./script.sh   → mensajes debug + trace
```

### 5.3 Xtrace solo para pipelines problemáticos

```bash
#!/bin/bash
set -euo pipefail

# Solo tracear la sección problemática
echo "Preparando datos..."

set -x
curl -sf "https://api.example.com/data" \
  | jq '.results[]' \
  | while IFS= read -r item; do
      echo "$item" >> processed.json
    done
set +x

echo "Datos procesados"
```

---

## 6. trap DEBUG — breakpoints y tracing avanzado

### 6.1 Ejecutar código en cada comando

`trap '...' DEBUG` ejecuta el código especificado **antes de cada comando simple**:

```bash
#!/bin/bash

trap 'echo "[LINE $LINENO] next: $BASH_COMMAND"' DEBUG

x=10
y=20
echo "sum = $(( x + y ))"
```

```
[LINE 3] next: x=10
[LINE 4] next: y=20
[LINE 5] next: echo "sum = 30"
sum = 30
```

`$BASH_COMMAND` contiene el comando que está **a punto de ejecutarse**.

### 6.2 Breakpoints condicionales

```bash
#!/bin/bash

# Pausar cuando una variable llega a cierto valor
trap '
  if [[ "$BASH_COMMAND" =~ count= ]] && (( count > 5 )); then
    echo "BREAK: count=$count at line $LINENO" >&2
    echo "Press Enter to continue..." >&2
    read -r
  fi
' DEBUG

count=0
for i in {1..10}; do
  count=$i
  echo "Processing item $i"
done
```

### 6.3 Logging automático de cada comando

```bash
#!/bin/bash

# Log cada comando con timestamp a un archivo
log_file="/tmp/script_debug.log"
trap 'echo "$(date +%H:%M:%S.%N) [L$LINENO] $BASH_COMMAND" >> "$log_file"' DEBUG

echo "Starting..."
sleep 1
echo "Done"
```

```
# /tmp/script_debug.log:
# 14:30:05.123456 [L5] echo "Starting..."
# 14:30:05.124789 [L6] sleep 1
# 14:30:06.128012 [L7] echo "Done"
```

### 6.4 Comparación: set -x vs trap DEBUG

| Aspecto | `set -x` | `trap DEBUG` |
|---------|----------|-------------|
| Muestra el comando | Sí (expandido) | Sí (sin expandir, en `$BASH_COMMAND`) |
| Se ejecuta | Después de expansión, antes de ejecución | Antes de cada comando simple |
| Personalizable | Solo via PS4 | Código Bash arbitrario |
| Performance | Bajo overhead | Mayor overhead (ejecuta trap cada vez) |
| Breakpoints | No | Sí (con `read`) |
| Variables | Ve valores expandidos | Puede inspeccionar cualquier variable |
| Uso típico | Tracing general | Debugging específico |

---

## 7. Variables de debugging

Bash expone varias variables útiles para debugging:

```bash
# Dentro de un script:
echo "Script: ${BASH_SOURCE[0]}"     # archivo actual
echo "Función: ${FUNCNAME[0]}"       # función actual (vacío si main)
echo "Línea: $LINENO"               # línea actual
echo "Comando: $BASH_COMMAND"        # último comando (útil en traps)
echo "Subshell level: $BASH_SUBSHELL" # 0=main, 1=subshell, 2=sub-subshell
echo "PID: $$, BASHPID: $BASHPID"   # $$ = main, BASHPID = actual

# Call stack (en una función)
show_stack() {
  local i
  echo "=== Call Stack ===" >&2
  for (( i=0; i < ${#FUNCNAME[@]}; i++ )); do
    echo "  [$i] ${FUNCNAME[$i]}() at ${BASH_SOURCE[$i+1]:-main}:${BASH_LINENO[$i]}" >&2
  done
}
```

### 7.1 Call stack manual

```bash
#!/bin/bash

stacktrace() {
  local i
  echo "--- Stacktrace ---" >&2
  for (( i=1; i < ${#FUNCNAME[@]}; i++ )); do
    echo "  ${BASH_SOURCE[$i]}:${BASH_LINENO[$i-1]} in ${FUNCNAME[$i]}()" >&2
  done
}

inner() {
  stacktrace
  echo "inner executing"
}

middle() {
  inner
}

outer() {
  middle
}

outer
```

```
--- Stacktrace ---
  script.sh:16 in inner()
  script.sh:20 in middle()
  script.sh:24 in outer()
  script.sh:27 in main()
inner executing
```

### 7.2 Error handler con stacktrace

```bash
#!/bin/bash
set -euo pipefail

on_error() {
  local exit_code=$?
  echo "ERROR: exit code $exit_code" >&2
  echo "  Command: $BASH_COMMAND" >&2
  echo "  Location: ${BASH_SOURCE[1]}:${BASH_LINENO[0]}" >&2
  local i
  for (( i=1; i < ${#FUNCNAME[@]}; i++ )); do
    echo "  called from: ${BASH_SOURCE[$i+1]:-?}:${BASH_LINENO[$i]} ${FUNCNAME[$i]}()" >&2
  done
}
trap on_error ERR

process() {
  cat /no/existe    # falla aquí
}

main() {
  process
}

main
```

```
cat: /no/existe: No such file or directory
ERROR: exit code 1
  Command: cat /no/existe
  Location: script.sh:18
  called from: script.sh:22 main()
  called from: ?:25 main()
```

---

## 8. Técnicas de debugging prácticas

### 8.1 Echo debugging (lo más básico)

```bash
#!/bin/bash
set -euo pipefail

echo "DEBUG: starting, args=$*" >&2

config_file="${1:?Usage: $0 <config>}"
echo "DEBUG: config_file=$config_file" >&2

if [[ -f "$config_file" ]]; then
  echo "DEBUG: file exists, reading..." >&2
  source "$config_file"
  echo "DEBUG: after source, DB_HOST=${DB_HOST:-undefined}" >&2
else
  echo "DEBUG: file NOT found" >&2
  exit 1
fi
```

Reglas de echo debugging:
- Siempre a **stderr** (`>&2`) para no contaminar stdout
- Prefijo `DEBUG:` para identificar rápidamente
- Incluir **nombre** y **valor** de la variable, no solo el valor
- Eliminar o convertir a función debug() antes de hacer commit

### 8.2 Patrón: debug() condicional

```bash
#!/bin/bash
set -euo pipefail

# Función debug que solo imprime si DEBUG está activo
debug() {
  [[ "${DEBUG:-0}" == "1" ]] && echo "[DEBUG] $*" >&2 || true
}

debug "Script started with args: $*"

config="${1:?}"
debug "Loading config: $config"

source "$config"
debug "Config loaded, DB_HOST=${DB_HOST:-unset}"
```

```bash
# Uso normal — sin output de debug
./script.sh config.sh

# Con debug — ver mensajes internos
DEBUG=1 ./script.sh config.sh
```

### 8.3 Comparar archivos antes/después

```bash
#!/bin/bash
set -euo pipefail

# Guardar estado antes de una operación
cp important_file.conf important_file.conf.debug_before

# Operación que modifica el archivo
process_config important_file.conf

# Comparar
diff important_file.conf.debug_before important_file.conf || true
```

### 8.4 Logging con timestamps

```bash
#!/bin/bash
set -euo pipefail

LOG_FILE="/tmp/script_debug_$(date +%Y%m%d_%H%M%S).log"

log() {
  local level="$1"; shift
  echo "[$(date +%H:%M:%S)] [$level] $*" | tee -a "$LOG_FILE" >&2
}

log INFO "Script started"
log DEBUG "Processing file: $1"

if ! curl -sf "$url" -o "$tmpfile"; then
  log ERROR "curl failed for $url"
  exit 1
fi

log INFO "Script completed"
```

---

## 9. bashdb — debugger interactivo

Para debugging complejo, existe `bashdb`, un debugger interactivo similar a `gdb`:

```bash
# Instalación
sudo apt install bashdb        # Debian/Ubuntu
sudo dnf install bashdb        # Fedora

# Uso básico
bashdb script.sh
```

### 9.1 Comandos de bashdb

```
bashdb<0> help
  s, step          — ejecutar siguiente línea (entrar en funciones)
  n, next          — ejecutar siguiente línea (saltar funciones)
  c, continue      — continuar hasta siguiente breakpoint
  b, break LINE    — poner breakpoint en línea
  delete N         — eliminar breakpoint N
  p EXPR           — imprimir expresión
  x EXPR           — examinar variable
  l, list          — listar código fuente alrededor
  where, bt        — mostrar call stack
  q, quit          — salir del debugger
```

### 9.2 Sesión de ejemplo

```bash
$ bashdb my_script.sh arg1 arg2
bash debugger, bashdb, release 5.0
Copyright 2002-2019 Rocky Bernstein
...
(/path/to/my_script.sh:3):
3:  config="${1:?Usage: $0 <config>}"

bashdb<0> n          # siguiente línea
(/path/to/my_script.sh:5):
5:  source "$config"

bashdb<1> p $config  # imprimir variable
arg1

bashdb<2> b 15       # breakpoint en línea 15
Breakpoint 1 set in file /path/to/my_script.sh, line 15

bashdb<3> c          # continuar hasta breakpoint
...
```

### 9.3 Cuándo usar bashdb vs set -x

```
    ¿Qué tipo de problema?
            │
    ┌───────┼───────┐
    │       │       │
    ▼       ▼       ▼
  Simple  Flujo   Complejo
    │     raro      │
    │       │       │
    ▼       ▼       ▼
  set -x  trap    bashdb
  PS4     DEBUG
```

- **set -x**: problemas de expansión de variables, orden de ejecución, pipes
- **trap DEBUG**: necesitas inspeccionar estado en puntos específicos
- **bashdb**: lógica compleja, múltiples funciones, necesitas step-through

En la práctica, `set -x` con `PS4` resuelve el 90% de los problemas. `bashdb`
es para los casos donde necesitas un debugger real.

---

## 10. Recetas de debugging

### 10.1 "¿Por qué no entra al if?"

```bash
#!/bin/bash
set -euo pipefail

var="hello"

# El if no se ejecuta como esperas — ¿por qué?
if [[ $var == "hello" ]]; then
  echo "match"
fi

# Debug: ver qué vale var realmente
echo "DEBUG: var=[$var] (length=${#var})" >&2

# Causas comunes:
# 1. Trailing whitespace/newline: var="hello " o var=$'hello\n'
# 2. Variable viene de command substitution con caracteres invisibles
# 3. La variable no está definida (con set -u aborta antes)

# Diagnóstico: hexdump para ver caracteres invisibles
echo -n "$var" | xxd >&2
```

### 10.2 "¿Por qué el pipe pierde datos?"

```bash
#!/bin/bash
set -euo pipefail

# Problema: la variable count no se actualiza
count=0
cat data.txt | while IFS= read -r line; do
  (( count++ )) || true
done
echo "Lines: $count"    # siempre 0 — ¿por qué?

# Debug: el pipe crea subshell — count se modifica en la subshell y se pierde
# Solución: redirigir en vez de pipe
count=0
while IFS= read -r line; do
  (( count++ )) || true
done < data.txt
echo "Lines: $count"    # ahora funciona
```

### 10.3 "El script funciona manual pero no en cron"

```bash
# Causas más comunes:
# 1. PATH diferente
# 2. Variables de entorno no definidas
# 3. Directorio de trabajo diferente
# 4. Shell diferente (/bin/sh vs /bin/bash)
# 5. No hay terminal (tty)

# Debug: al inicio del script de cron
#!/bin/bash
exec > /tmp/cron_debug.log 2>&1
set -x
echo "PATH=$PATH"
echo "HOME=$HOME"
echo "PWD=$PWD"
echo "SHELL=$SHELL"
env | sort

# Solucion tipica: definir PATH explícitamente
#!/bin/bash
set -euo pipefail
export PATH="/usr/local/bin:/usr/bin:/bin"
cd "$(dirname "${BASH_SOURCE[0]}")"
```

### 10.4 "¿Por qué tarda tanto?"

```bash
#!/bin/bash
set -euo pipefail

# Profiling simple con timestamps
PS4='+[$(date +%s.%N)] '
set -x

step1     # operación
step2     # operación lenta — lo verás por la diferencia de timestamps
step3     # operación

set +x
```

```bash
# Profiling más preciso
#!/bin/bash
TIMEFORMAT='  → %Rs elapsed'

echo "Step 1: download"
time curl -sf "$url" -o data.json

echo "Step 2: process"
time jq '.results' data.json > processed.json

echo "Step 3: upload"
time scp processed.json server:/data/
```

### 10.5 "La función recibe argumentos incorrectos"

```bash
#!/bin/bash
set -euo pipefail

process() {
  # Imprimir exactamente qué recibió la función
  echo "DEBUG process(): $# args:" >&2
  local i=0
  for arg in "$@"; do
    echo "  [$i]='$arg'" >&2
    (( i++ )) || true
  done

  # ... lógica de la función ...
}

# Llamada — ¿los argumentos se pasan correctamente?
process "file name.txt" "--flag" "value with spaces"
```

```
DEBUG process(): 3 args:
  [0]='file name.txt'
  [1]='--flag'
  [2]='value with spaces'
```

---

## 11. Diagrama de decisión

```
    El script falla. ¿Qué hacer?
                │
    ¿El error es de sintaxis?
         │            │
        SÍ           NO
         │            │
    bash -n       ¿Sabes dónde falla?
                     │            │
                    SÍ           NO
                     │            │
                set -x en     set -x global
                esa zona      + PS4 con línea
                     │            │
                ¿Necesitas        │
                inspeccionar      │
                variables?        │
                │         │       │
               SÍ        NO      │
                │         │       │
           trap DEBUG  Leer el    │
           o bashdb    trace      │
                              ¿Funciona en
                              terminal pero
                              no en cron?
                                  │
                              env + PATH
                              + shell check
```

---

## 12. Errores comunes

| Error | Causa | Solución |
|-------|-------|----------|
| Trace mezclado con output | `set -x` va a stderr, script a stdout | Redirigir: `2>trace.log` o usar `BASH_XTRACEFD` |
| `bash -n` no detecta bug | Es error semántico, no sintáctico | Usar ShellCheck + tests |
| PS4 no muestra función | No estás dentro de una función | `${FUNCNAME[0]:-main}` como fallback |
| `set +x` aparece en el trace | Es normal — el propio `set +x` se tracea | Ignorar o redirigir trace a archivo |
| trap DEBUG no se hereda en funciones | Falta `set -T` (functrace) | Añadir `set -T` o `shopt -s extdebug` |
| `$BASH_COMMAND` vacío en trap | Trap incorrecto o shell no es Bash | Verificar shebang `#!/bin/bash` |
| Trace demasiado verboso | Script con muchas iteraciones | Tracear solo la sección problemática |
| `LINENO` incorrecto en functions | Bug conocido en Bash < 4.3 con source | Actualizar Bash o usar `caller` |
| `bashdb` no disponible | No instalado o no en PATH | `apt install bashdb` o usar `set -x` |
| Script se comporta diferente con `-x` | Timing change (race conditions) | Redirigir trace a archivo para menor impacto |

---

## 13. Ejercicios

### Ejercicio 1 — bash -n

El siguiente script tiene un error de sintaxis. Encuéntralo **sin ejecutar** el script.

```bash
#!/bin/bash
set -euo pipefail

process() {
  local file="$1"
  if [[ -f "$file" ]]; then
    wc -l < "$file"
  else
    echo "not found" >&2
  fi

main() {
  for f in /var/log/*.log; do
    process "$f"
  done
}

main
```

**Prediccion**: ¿Qué reporta `bash -n` y en qué línea?

<details>
<summary>Ver solución</summary>

```bash
bash -n broken.sh
# broken.sh: line 12: syntax error near unexpected token `main'
# broken.sh: line 12: `main() {'
```

Falta el `fi` que cierra el `if` dentro de `process()`. Sin el `fi`, Bash cree
que `process()` nunca se cerró, y cuando encuentra `main()` interpreta que estás
intentando definir otra función dentro de la primera — error de sintaxis.

Corregido:

```bash
process() {
  local file="$1"
  if [[ -f "$file" ]]; then
    wc -l < "$file"
  else
    echo "not found" >&2
  fi    # ← faltaba
}
```

`bash -n` detecta esto en un instante. Sin él, tendrías que ejecutar el script
y deducir el problema del error de runtime, que podría ser más críptico.
</details>

---

### Ejercicio 2 — Leer trace de set -x

Dado este trace de `set -x`, reconstruye qué pasó:

```
+ config_file=/etc/app/config.yaml
+ '[' -f /etc/app/config.yaml ']'
+ source /etc/app/config.yaml
++ DB_HOST=localhost
++ DB_PORT=5432
+ echo 'Connecting to localhost:5432'
Connecting to localhost:5432
+ curl -sf https://localhost:5432/health
+ echo 'Health check passed'
```

**Prediccion**: ¿Cuántos comandos realmente se ejecutaron? ¿Qué indican los `++`?

<details>
<summary>Ver solución</summary>

Se ejecutaron **6 comandos** (las líneas con `+`):
1. Asignación de `config_file`
2. Test `[ -f ... ]` (condición de un `if`, retornó true)
3. `source` del archivo de configuración
4. `echo` del mensaje de conexión
5. `curl` al endpoint de salud
6. `echo` de resultado

Las líneas con `++` (`DB_HOST=localhost`, `DB_PORT=5432`) son comandos ejecutados
**dentro de la subshell del `source`**. El doble `+` indica un nivel de profundidad:
el script principal está en `+`, y los comandos del archivo sourceado en `++`.

Notar que `curl` aparentemente tuvo éxito (el trace no muestra error y el `echo`
siguiente se ejecutó). Si `curl` hubiera fallado con `set -e`, el script habría
abortado antes del último `echo`.
</details>

---

### Ejercicio 3 — PS4 personalizado

Escribe un PS4 que muestre: timestamp en formato `HH:MM:SS`, nombre del archivo
(sin path), número de línea, y nombre de función (o "main" si no está en una función).

**Prediccion**: ¿Por qué se usa `${BASH_SOURCE[0]##*/}` en vez de `$BASH_SOURCE`?

<details>
<summary>Ver solución</summary>

```bash
PS4='+$(date +%H:%M:%S) ${BASH_SOURCE[0]##*/}:${LINENO}:${FUNCNAME[0]:-main}: '
```

Ejemplo de output:
```
+14:30:05 script.sh:10:main: echo "hello"
+14:30:05 script.sh:15:process: local file=/tmp/data.txt
+14:30:05 script.sh:16:process: wc -l /tmp/data.txt
```

`${BASH_SOURCE[0]##*/}` usa parameter expansion `##*/` para eliminar todo hasta
el último `/` — equivale a `basename`. Se usa en vez de `$BASH_SOURCE` por dos razones:

1. **Legibilidad**: `script.sh:42` es más legible que `/home/user/projects/myapp/scripts/script.sh:42`
2. **Performance**: la expansión `##*/` es interna de Bash (no fork), mientras
   que `$(basename "$BASH_SOURCE")` haría un fork por cada línea de trace
   (devastador para performance en scripts con muchas líneas).
</details>

---

### Ejercicio 4 — Xtrace selectivo

Dado un script con un bug en la función `transform`, activa `set -x` solo
para esa función sin tracear el resto:

```bash
#!/bin/bash
set -euo pipefail

load_data() {
  cat "$1"
}

transform() {
  local data="$1"
  echo "$data" | tr '[:lower:]' '[:upper:]' | sed 's/  */ /g'
}

save_result() {
  echo "$1" > "$2"
}

main() {
  local input="${1:?}"
  local output="${2:?}"
  local data
  data=$(load_data "$input")
  local result
  result=$(transform "$data")
  save_result "$result" "$output"
  echo "Done"
}

main "$@"
```

**Prediccion**: Si pones `set -x` dentro de `transform`, ¿el trace aparece
cuando `transform` se llama desde `result=$(transform "$data")`?

<details>
<summary>Ver solución</summary>

```bash
transform() {
  set -x
  local data="$1"
  echo "$data" | tr '[:lower:]' '[:upper:]' | sed 's/  */ /g'
  set +x
}
```

Sí, el trace aparece, pero con una particularidad: `result=$(transform "$data")`
ejecuta `transform` en una **subshell** (por el `$()`). El `set -x` dentro de
la subshell activa el trace ahí, y el `set +x` lo desactiva ahí. El trace va
a stderr (que no se captura por `$()`), así que sí se muestra en pantalla.

El `set -x` de la subshell **no afecta** al script principal — cuando la subshell
termina, el estado de xtrace del script padre se mantiene sin cambios.

Output del trace (solo la función):
```
++ set -x
++ local data='hello world'
++ echo 'hello world'
++ tr '[:lower:]' '[:upper:]'
++ sed 's/  */ /g'
++ set +x
```

Los `++` indican que estamos en subshell (un nivel de profundidad).
</details>

---

### Ejercicio 5 — trap DEBUG

Escribe un trap DEBUG que imprima el valor de la variable `count` cada vez que
cambia, sin modificar el resto del script:

```bash
#!/bin/bash

count=0
for i in {1..5}; do
  count=$((count + i))
  echo "Step $i"
done
echo "Total: $count"
```

**Prediccion**: ¿Cuántas veces se ejecuta el trap DEBUG en este script?

<details>
<summary>Ver solución</summary>

```bash
#!/bin/bash

prev_count=""
trap '
  if [[ "${count:-}" != "$prev_count" ]]; then
    echo "[DEBUG] count changed: $prev_count → ${count:-}" >&2
    prev_count="${count:-}"
  fi
' DEBUG

count=0
for i in {1..5}; do
  count=$((count + i))
  echo "Step $i"
done
echo "Total: $count"
```

Output:
```
[DEBUG] count changed:  → 0
Step 1
[DEBUG] count changed: 0 → 1
Step 2
[DEBUG] count changed: 1 → 3
Step 3
[DEBUG] count changed: 3 → 6
Step 4
[DEBUG] count changed: 6 → 10
Step 5
[DEBUG] count changed: 10 → 15
Total: 15
```

El trap DEBUG se ejecuta **antes de cada comando simple**, incluyendo:
- Cada asignación (`count=0`, `count=$((count + i))`)
- Cada `echo`
- Cada iteración del `for`
- La evaluación de la condición del `for`

Se ejecuta significativamente más veces de lo que hay líneas de código. El trap
incluye sus propios comandos, pero el trap DEBUG **no se dispara recursivamente**
(los comandos dentro del trap no disparan otro trap DEBUG).
</details>

---

### Ejercicio 6 — Debugging un fallo real

El siguiente script debería contar líneas con "ERROR" en logs, pero siempre
reporta 0. Usa debugging para encontrar el bug:

```bash
#!/bin/bash
set -euo pipefail

count=0
find /var/log -name "*.log" -print0 | while IFS= read -r -d '' log; do
  errors=$(grep -c "ERROR" "$log" 2>/dev/null || true)
  count=$((count + errors))
done
echo "Total errors: $count"
```

**Prediccion**: ¿Qué técnica de debugging usarías y qué revelaría?

<details>
<summary>Ver solución</summary>

Usando `set -x`:
```
+ count=0
+ find /var/log -name '*.log' -print0
+ IFS= read -r -d '' log
+ grep -c ERROR /var/log/syslog
+ errors=5
+ count=5
+ IFS= read -r -d '' log
...
+ echo 'Total errors: 0'
Total errors: 0
```

El trace revela que `count` se modifica correctamente **dentro del loop** (llega a 5,
luego más), pero el `echo` final imprime `0`. El bug es el **pipe subshell**: el `|`
antes del `while` crea una subshell. `count` se modifica en la subshell y se pierde
cuando la subshell termina (cubierto en S02/T01 y en la receta 10.2).

Corrección — usar process substitution para evitar la subshell:

```bash
count=0
while IFS= read -r -d '' log; do
  errors=$(grep -c "ERROR" "$log" 2>/dev/null || true)
  count=$((count + errors))
done < <(find /var/log -name "*.log" -print0)
echo "Total errors: $count"
```

Ahora `while` corre en el proceso principal y `count` se preserva.
</details>

---

### Ejercicio 7 — BASH_XTRACEFD

Modifica el siguiente script para que el trace de `set -x` vaya a
`/tmp/trace.log` sin mezclarse con la salida estándar del script:

```bash
#!/bin/bash
set -euo pipefail
set -x

echo "Processing..."
result=$(curl -sf "https://api.example.com/data" | jq '.count')
echo "Result: $result"
```

**Prediccion**: ¿Qué pasa si olvidas cerrar el fd al final del script?

<details>
<summary>Ver solución</summary>

```bash
#!/bin/bash
set -euo pipefail

# Abrir fd 3 para trace
exec 3>/tmp/trace.log
BASH_XTRACEFD=3

set -x

echo "Processing..."
result=$(curl -sf "https://api.example.com/data" | jq '.count')
echo "Result: $result"

set +x
exec 3>&-    # cerrar fd
```

Stdout (pantalla):
```
Processing...
Result: 42
```

/tmp/trace.log:
```
+ echo Processing...
+ curl -sf https://api.example.com/data
+ jq .count
+ result=42
+ echo 'Result: 42'
+ set +x
```

Si olvidas cerrar el fd: en scripts cortos, no pasa nada grave — el fd se cierra
automáticamente cuando el proceso termina. Pero en scripts largos o que se
ejecutan muchas veces, podrías acumular fds abiertos. La buena práctica es
cerrar explícitamente con `exec 3>&-`, especialmente si el script continúa
ejecutando más código después de la sección de debug.
</details>

---

### Ejercicio 8 — Error handler con stacktrace

Escribe un error handler con `trap ERR` que muestre: exit code, comando que
falló, archivo, línea, y call stack completo.

**Prediccion**: ¿Necesitas `set -E` (`errtrace`) para que el trap ERR funcione
dentro de funciones?

<details>
<summary>Ver solución</summary>

```bash
#!/bin/bash
set -euo pipefail
set -E    # errtrace: ERR trap se hereda en funciones

on_error() {
  local exit_code=$?
  echo "========== ERROR ==========" >&2
  echo "Exit code: $exit_code" >&2
  echo "Command:   $BASH_COMMAND" >&2
  echo "--- Call Stack ---" >&2
  local i
  for (( i=0; i < ${#FUNCNAME[@]}-1; i++ )); do
    echo "  ${BASH_SOURCE[$i+1]}:${BASH_LINENO[$i]} in ${FUNCNAME[$i+1]}()" >&2
  done
  echo "===========================" >&2
}
trap on_error ERR
```

Sí, `set -E` (o `set -o errtrace`) es necesario. Sin él, el trap ERR solo se
dispara en el scope del nivel principal — si un comando falla dentro de una
función, el trap ERR **no se ejecuta**. Con `set -E`, el trap ERR se hereda
por subshells y funciones.

Análogamente, `set -T` (`functrace`) hace lo mismo para trap DEBUG y RETURN.

| Flag | Hereda trap en funciones |
|------|--------------------------|
| `set -E` (errtrace) | ERR |
| `set -T` (functrace) | DEBUG y RETURN |
</details>

---

### Ejercicio 9 — Script de cron que falla

El siguiente script funciona perfectamente ejecutado manualmente pero falla
cuando lo pones en cron. Diagnostica el problema:

```bash
#!/bin/bash
set -euo pipefail

backup_dir="/backups/$(date +%Y%m%d)"
mkdir -p "$backup_dir"

pg_dump production > "$backup_dir/db.sql"
gzip "$backup_dir/db.sql"

echo "Backup complete: $backup_dir" | mail -s "Backup OK" admin@example.com
```

**Prediccion**: ¿Cuáles son las 3 causas más probables de fallo en cron?

<details>
<summary>Ver solución</summary>

Las 3 causas más probables:

1. **PATH**: cron tiene un PATH mínimo (`/usr/bin:/bin`). `pg_dump`, `gzip`, y
   `mail` podrían no estar en ese PATH. En terminal funciona porque tu `.bashrc`
   configura PATH más completo.

2. **Variables de entorno**: `pg_dump` necesita `PGHOST`, `PGUSER`, `PGPASSWORD`
   o un `.pgpass`. En tu terminal, estas están configuradas. En cron, no.

3. **Directorio de trabajo**: cron ejecuta desde `$HOME` del usuario. Si el script
   depende de estar en un directorio específico, puede fallar.

Script corregido para cron:

```bash
#!/bin/bash
set -euo pipefail

# Diagnóstico: redirigir todo a log
exec >> /var/log/backup.log 2>&1

# PATH explícito
export PATH="/usr/local/bin:/usr/bin:/bin"

# Directorio del script
cd "$(dirname "${BASH_SOURCE[0]}")"

# Variables de entorno para pg_dump
export PGHOST="db.example.com"
export PGUSER="backup_user"
# PGPASSWORD debería venir de .pgpass o pg_service.conf, no hardcoded

echo "[$(date)] Starting backup"

backup_dir="/backups/$(date +%Y%m%d)"
mkdir -p "$backup_dir"

pg_dump production > "$backup_dir/db.sql"
gzip "$backup_dir/db.sql"

echo "[$(date)] Backup complete: $backup_dir"
# mail con path completo por si acaso
/usr/bin/mail -s "Backup OK" admin@example.com <<< "Backup at $backup_dir"
```

Técnica de diagnóstico: añadir al inicio del script de cron:
```bash
exec > /tmp/cron_debug.log 2>&1
set -x
env | sort
```
Esto captura todo el output + trace + variables de entorno a un archivo que
puedes examinar después del fallo.
</details>

---

### Ejercicio 10 — Debugging con set -x y PS4

El siguiente script tiene un bug sutil. Usa `set -x` con un PS4 personalizado
para encontrarlo:

```bash
#!/bin/bash
set -euo pipefail

count_files() {
  local dir="$1"
  local ext="$2"
  find "$dir" -name "*.$ext" | wc -l
}

main() {
  local src_count
  src_count=$(count_files "/usr/share" "conf")
  echo "Config files: $src_count"

  local log_count
  log_count=$(count_files "/var/log" "log")
  echo "Log files: $log_count"

  echo "Total: $(( src_count + log_count ))"
}

main
```

**Prediccion**: ¿El PS4 mostrará correctamente el nombre de `count_files` cuando
se llama desde `$()`?

<details>
<summary>Ver solución</summary>

```bash
PS4='+${BASH_SOURCE[0]##*/}:${LINENO}:${FUNCNAME[0]:-main}: '
set -x
```

El trace revelaría que `count_files` funciona, pero `wc -l` incluye **espacios
en blanco** al inicio del output. Por ejemplo, `wc -l` podría retornar `"   42"`
en vez de `"42"`. Esto hace que la suma aritmética `$(( src_count + log_count ))`
funcione correctamente (Bash ignora whitespace en aritmética), pero si `src_count`
se usa en una comparación de string o se imprime, los espacios pueden causar
comportamiento inesperado.

Si quisiéramos asegurarnos de que el valor es limpio:

```bash
count_files() {
  local dir="$1"
  local ext="$2"
  find "$dir" -name "*.$ext" | wc -l | tr -d ' '
}
```

O con redirección (que evita el espacio):

```bash
count_files() {
  local dir="$1"
  local ext="$2"
  local count
  count=$(find "$dir" -name "*.$ext" -print0 | tr -cd '\0' | wc -c)
  echo "$count"
}
```

Sí, PS4 muestra correctamente `${FUNCNAME[0]}` como `count_files` incluso
dentro de `$()`. La subshell hereda el stack de funciones. El trace mostraría
las líneas con `++` (doble plus) indicando el nivel de subshell:

```
++script.sh:6:count_files: local dir=/usr/share
++script.sh:7:count_files: local ext=conf
++script.sh:8:count_files: find /usr/share -name '*.conf'
++script.sh:8:count_files: wc -l
```
</details>
