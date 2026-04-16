# Portabilidad — POSIX sh vs Bash

## 1. El problema — "funciona en mi máquina"

Un script que funciona perfectamente en tu terminal puede romperse en:

- **Docker**: imágenes Alpine/distroless usan `dash` o `busybox sh`, no Bash
- **cron**: puede ejecutar con `/bin/sh` en vez de `/bin/bash`
- **CI/CD**: runners con shells mínimos
- **macOS**: tiene Bash 3.2 (2007) por licencia GPL vs tu Linux con Bash 5.x
- **Sistemas embebidos**: BusyBox `sh` sin bashismos
- **Debian/Ubuntu**: `/bin/sh` es `dash` desde 2006, no Bash

```
    Tu terminal                Docker Alpine
    ┌──────────────┐           ┌──────────────┐
    │ /bin/bash    │           │ /bin/sh      │
    │ Bash 5.2     │           │ = dash 0.5   │
    │ [[ ]], arrays│           │ POSIX only   │
    │ set -o pipe  │           │ No arrays    │
    │ <() >()      │           │ No [[ ]]     │
    └──────────────┘           └──────────────┘
          ✓                         ✗
     script.sh                 script.sh
```

---

## 2. POSIX sh — el estándar mínimo

### 2.1 Qué es POSIX sh

POSIX (Portable Operating System Interface) define un estándar para shells.
Un script `#!/bin/sh` debería funcionar en **cualquier** sistema POSIX: Linux,
macOS, FreeBSD, Solaris, AIX, containers mínimos.

```bash
#!/bin/sh
# Este script funciona en CUALQUIER shell POSIX
name="Alice"
if [ "$name" = "Alice" ]; then
    echo "Hello, $name"
fi
```

### 2.2 Shells POSIX comunes

| Shell | Sistema | Velocidad | Notas |
|-------|---------|-----------|-------|
| `dash` | Debian/Ubuntu `/bin/sh` | Muy rápido | Mínimo, solo POSIX |
| `bash` (modo POSIX) | Cuando se invoca como `sh` | Medio | Desactiva bashismos |
| `ash`/`busybox sh` | Alpine, embebidos | Muy rápido | Mínimo |
| `ksh` | Solaris, AIX | Rápido | Extensiones propias |
| `zsh` (modo sh) | macOS `/bin/sh` (antiguo) | Medio | Hoy macOS usa bash 3.2 |

### 2.3 /bin/sh vs /bin/bash

```bash
# ¿Qué es /bin/sh en tu sistema?
ls -la /bin/sh
# Debian/Ubuntu: /bin/sh -> dash
# Fedora/RHEL:   /bin/sh -> bash
# Alpine:        /bin/sh -> busybox
# macOS:         /bin/sh -> bash (3.2)

# ¿Son el mismo binario?
readlink -f /bin/sh
readlink -f /bin/bash
# Si son diferentes, /bin/sh NO es bash
```

Cuando Bash se ejecuta como `/bin/sh` (o con `--posix`), **desactiva** muchas
extensiones para comportarse como POSIX sh. Pero no todas las distribuciones
usan Bash como `/bin/sh`.

---

## 3. Bashismos — features de Bash no POSIX

### 3.1 Tabla de equivalencias

| Bashismo | POSIX equivalente | Notas |
|----------|-------------------|-------|
| `[[ expr ]]` | `[ expr ]` | `[` requiere más cuidado con quoting |
| `[[ $x == pattern ]]` | `case $x in pattern) ;; esac` | POSIX no tiene glob en `[ ]` |
| `[[ $x =~ regex ]]` | `echo "$x" \| grep -qE 'regex'` | POSIX no tiene regex en test |
| `(( arith ))` | `expr` o `$(( arith ))` | `$(( ))` SÍ es POSIX |
| `declare -a arr` | No existe | POSIX sh no tiene arrays |
| `${arr[@]}` | No existe | Usar posicional `$@` o loops con IFS |
| `local var` | No garantizado | Existe en dash/ash pero no es POSIX estricto |
| `source file` | `. file` | `.` es el POSIX, `source` es bashismo |
| `function f() { }` | `f() { }` | `function` keyword no es POSIX |
| `echo -e` | `printf` | `echo` flags no son portables |
| `read -r -a arr` | No existe | `read -r` sí es POSIX, `-a` no |
| `read -p "prompt"` | `printf "prompt"; read -r` | `-p` no es POSIX |
| `${var,,}` / `${var^^}` | `echo "$var" \| tr A-Z a-z` | Case change no es POSIX |
| `${var//pat/rep}` | `echo "$var" \| sed 's/pat/rep/g'` | Global replace no es POSIX |
| `<(cmd)` / `>(cmd)` | Archivos temporales o named pipes | Process substitution no es POSIX |
| `$'...'` | No existe | ANSI-C quoting no es POSIX |
| `set -o pipefail` | No existe | No hay equivalente POSIX directo |
| `mapfile` / `readarray` | `while read` loops | Exclusivo de Bash |
| `{1..10}` | `seq 1 10` | Brace expansion no es POSIX |
| `&>file` | `>file 2>&1` | Redirección combinada no es POSIX |
| `<<<word` | `echo "word" \| cmd` | Here-string no es POSIX |
| `coproc` | Named pipes (mkfifo) | Exclusivo de Bash |
| `shopt` | No existe | Opciones Bash-only |

### 3.2 Los bashismos más peligrosos

Estos son los que rompen silenciosamente (sin error claro):

```sh
#!/bin/sh

# 1. [[ ]] — error de sintaxis en dash
[[ -f /etc/passwd ]]
# dash: [[: not found

# 2. Arrays — error de sintaxis
arr=(1 2 3)
# dash: Syntax error: "(" unexpected

# 3. echo -e — se imprime literal "-e" en algunos shells
echo -e "line1\nline2"
# dash: imprime "-e line1\nline2" (literal)
# bash: imprime line1 y line2 en dos líneas

# 4. == en [ ] — puede fallar o dar resultado incorrecto
[ "$x" == "y" ]
# dash: ==: unexpected operator

# 5. Brace expansion — se pasa literal
for i in {1..5}; do echo "$i"; done
# dash: imprime literal "{1..5}"

# 6. $'...' — se pasa literal
IFS=$'\n'
# dash: IFS se setea a literal "$'\n'" (4 caracteres), no a newline
```

---

## 4. Escribir scripts POSIX portables

### 4.1 Test con `[ ]` en vez de `[[ ]]`

```sh
#!/bin/sh

# Bash: [[ permite sin comillas, glob, regex
[[ $var == *.txt ]]        # glob match
[[ $var =~ ^[0-9]+$ ]]    # regex match
[[ -z $var ]]              # sin comillas OK

# POSIX: [ ] requiere comillas y no tiene glob/regex
[ "$var" = "value" ]       # = en vez de ==, con comillas
[ -z "$var" ]              # comillas obligatorias
[ -f "$file" ]             # test de archivo

# Glob matching en POSIX — usar case
case "$var" in
  *.txt) echo "is txt" ;;
  *.log) echo "is log" ;;
  *)     echo "other" ;;
esac

# Regex matching en POSIX — usar grep
if echo "$var" | grep -qE '^[0-9]+$'; then
  echo "is numeric"
fi
```

### 4.2 Printf en vez de echo -e

```sh
#!/bin/sh

# Bash: echo -e interpreta escapes
echo -e "Name:\t$name\nAge:\t$age"

# POSIX: printf SIEMPRE interpreta escapes
printf "Name:\t%s\nAge:\t%s\n" "$name" "$age"

# printf es superior a echo en todo contexto:
# - Portable
# - No hay ambigüedad con flags
# - Format strings tipo C
# - Funciona igual en bash, dash, zsh, ksh

# Newline: echo añade \n automáticamente, printf no
echo "hello"              # hello\n
printf "hello\n"          # hello\n
printf "hello"            # hello (sin newline)
```

### 4.3 `.` en vez de `source`

```sh
#!/bin/sh

# Bash
source ./lib/utils.sh

# POSIX
. ./lib/utils.sh
```

### 4.4 Sin arrays — alternativas

```sh
#!/bin/sh

# Bash: arrays
files=("file1.txt" "file 2.txt" "file3.txt")
for f in "${files[@]}"; do
  process "$f"
done

# POSIX alternativa 1: positional parameters ($@)
set -- "file1.txt" "file 2.txt" "file3.txt"
for f in "$@"; do
  process "$f"
done

# POSIX alternativa 2: IFS + variable (sin espacios en valores)
files="file1.txt:file3.txt:file4.txt"
old_ifs="$IFS"
IFS=":"
for f in $files; do
  process "$f"
done
IFS="$old_ifs"

# POSIX alternativa 3: newline-separated (funciona con espacios)
files="file1.txt
file 2.txt
file3.txt"
echo "$files" | while IFS= read -r f; do
  process "$f"
done
```

### 4.5 Aritmética POSIX

```sh
#!/bin/sh

# Bash: (( )) para aritmética
(( count++ ))
(( x = y + z ))
if (( count > 10 )); then echo "big"; fi

# POSIX: $(( )) para expansión aritmética (SÍ es POSIX)
count=$((count + 1))
x=$((y + z))
if [ "$count" -gt 10 ]; then echo "big"; fi

# POSIX: expr para aritmética en sistemas muy antiguos
count=$(expr "$count" + 1)
# expr es más lento (fork) y más frágil — preferir $(( ))
```

### 4.6 Funciones POSIX

```sh
#!/bin/sh

# Bash: function keyword
function my_func() {
  local result="hello"
  echo "$result"
}

# POSIX: sin function keyword, local no garantizado
my_func() {
  # local existe en dash/ash pero no es POSIX estricto
  # En la práctica, funciona en todos los shells comunes
  result="hello"
  echo "$result"
}
```

### 4.7 Redirecciones POSIX

```sh
#!/bin/sh

# Bash: &> redirige stdout+stderr
cmd &> /dev/null

# POSIX: forma explícita
cmd > /dev/null 2>&1

# Bash: here-string
grep "pattern" <<< "$variable"

# POSIX: here-document o echo + pipe
echo "$variable" | grep "pattern"
# o
grep "pattern" <<EOF
$variable
EOF
```

### 4.8 Process substitution — alternativas POSIX

```sh
#!/bin/sh

# Bash: process substitution
diff <(sort file1) <(sort file2)

# POSIX: archivos temporales
tmp1=$(mktemp)
tmp2=$(mktemp)
trap 'rm -f "$tmp1" "$tmp2"' EXIT
sort file1 > "$tmp1"
sort file2 > "$tmp2"
diff "$tmp1" "$tmp2"

# POSIX: named pipes (más complejo)
fifo1=$(mktemp -u)
fifo2=$(mktemp -u)
mkfifo "$fifo1" "$fifo2"
trap 'rm -f "$fifo1" "$fifo2"' EXIT
sort file1 > "$fifo1" &
sort file2 > "$fifo2" &
diff "$fifo1" "$fifo2"
```

---

## 5. Docker — el caso de uso principal

### 5.1 El problema con Alpine

```dockerfile
# Alpine NO tiene bash por defecto
FROM alpine:3.19

# Esto FALLA:
COPY script.sh /app/
RUN chmod +x /app/script.sh
# Error: /bin/bash: not found (si script.sh tiene #!/bin/bash)

# Solución 1: instalar bash
RUN apk add --no-cache bash

# Solución 2: escribir el script en POSIX sh
# #!/bin/sh  (funciona con ash de Alpine)
```

### 5.2 Imágenes comunes y su shell

| Imagen base | `/bin/sh` | Bash disponible | Tamaño |
|-------------|-----------|-----------------|--------|
| `ubuntu:24.04` | dash | Sí (`/bin/bash`) | ~78 MB |
| `debian:12` | dash | Sí (`/bin/bash`) | ~117 MB |
| `alpine:3.19` | busybox ash | No (instalar con `apk`) | ~7 MB |
| `fedora:40` | bash | Sí | ~190 MB |
| `distroless` | Ninguno | No | ~2 MB |
| `busybox` | busybox ash | No | ~4 MB |
| `scratch` | Ninguno | No | 0 MB |

### 5.3 Entrypoints y scripts en Docker

```dockerfile
# MAL — asume bash
FROM alpine:3.19
COPY entrypoint.sh /
ENTRYPOINT ["/bin/bash", "/entrypoint.sh"]

# BIEN — usar sh (siempre disponible)
FROM alpine:3.19
COPY entrypoint.sh /
ENTRYPOINT ["/bin/sh", "/entrypoint.sh"]

# MEJOR — shebang correcto y ejecutable
FROM alpine:3.19
COPY entrypoint.sh /
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
# El shebang del script determina el intérprete
```

```sh
#!/bin/sh
# entrypoint.sh — portable para Alpine
set -eu    # set -o pipefail NO existe en dash

# Equivalente de pipefail para un caso específico:
if ! result=$(curl -sf "$API_URL" | jq -r '.status'); then
  echo "Failed to reach API" >&2
  exit 1
fi

exec "$@"    # pasar al CMD del Dockerfile
```

### 5.4 Patrón: detectar shell disponible

```sh
#!/bin/sh
# Script que se adapta al shell disponible

if command -v bash > /dev/null 2>&1; then
  # Re-ejecutarse con bash si está disponible
  exec bash "$0" "$@"
fi

# Fallback: modo POSIX
echo "Running in POSIX sh mode"
# ... código POSIX ...
```

---

## 6. cron — otro punto de fallo

### 6.1 Cron y /bin/sh

```bash
# cron usa /bin/sh por defecto para ejecutar comandos
# En Debian/Ubuntu, /bin/sh = dash

# crontab entry:
# 0 * * * * /scripts/backup.sh
# Se ejecuta con: /bin/sh -c '/scripts/backup.sh'
# Si backup.sh tiene #!/bin/bash, usa bash
# Si backup.sh tiene #!/bin/sh, usa dash
# Si backup.sh NO tiene shebang, cron usa /bin/sh
```

### 6.2 Forzar bash en cron

```bash
# Opción 1: shebang correcto en el script
#!/bin/bash
set -euo pipefail
# ...

# Opción 2: SHELL en crontab
SHELL=/bin/bash
0 * * * * /scripts/backup.sh

# Opción 3: invocar bash explícitamente
0 * * * * /bin/bash /scripts/backup.sh
```

### 6.3 Variables de entorno en cron

```bash
# cron NO carga .bashrc, .profile, ni .bash_profile
# Las variables de entorno son mínimas:
#   HOME, LOGNAME, SHELL, PATH (mínimo)

# Verificar qué tiene cron:
# Añadir esta línea al crontab:
* * * * * env > /tmp/cron_env.txt

# Resultado típico:
# HOME=/home/user
# LOGNAME=user
# PATH=/usr/bin:/bin
# SHELL=/bin/sh
```

---

## 7. macOS — Bash 3.2 congelado

### 7.1 El problema de la licencia

macOS incluye Bash 3.2 (2007) porque es la última versión con licencia GPLv2.
Bash 4.0+ usa GPLv3, que Apple no acepta incluir. Esto significa que scripts
con features de Bash 4+ fallan en macOS:

```bash
# Funciona en Linux (Bash 4+), falla en macOS (Bash 3.2):
declare -A assoc_array       # arrays asociativos (Bash 4.0)
${var,,}                     # lowercase (Bash 4.0)
${var^^}                     # uppercase (Bash 4.0)
mapfile -t arr < file        # mapfile (Bash 4.0)
readarray                    # alias de mapfile (Bash 4.0)
coproc                       # (Bash 4.0)
|&                           # pipe stderr (Bash 4.0)
${var@Q}                     # quoted expansion (Bash 4.4)
${arr[@]@Q}                  # (Bash 4.4)
shopt -s inherit_errexit     # (Bash 4.4)
```

### 7.2 Soluciones para macOS

```bash
# Instalar Bash moderno con Homebrew
brew install bash
# Se instala en /opt/homebrew/bin/bash (ARM) o /usr/local/bin/bash (Intel)

# Shebang portable que prefiere bash moderno:
#!/usr/bin/env bash
# env busca "bash" en PATH — si Homebrew está en PATH, usa el moderno

# Verificar versión
bash --version | head -1
# GNU bash, version 5.2.x
```

### 7.3 Otras diferencias macOS

```bash
# sed -i (in-place)
sed -i 's/old/new/' file        # Linux (GNU sed)
sed -i '' 's/old/new/' file     # macOS (BSD sed) — requiere '' como backup suffix

# date
date -d "2024-01-15" +%s       # Linux (GNU date)
date -j -f "%Y-%m-%d" "2024-01-15" +%s   # macOS (BSD date)

# readlink -f
readlink -f /some/link          # Linux (GNU)
# macOS: readlink -f no existe → usar realpath o brew install coreutils

# grep -P (Perl regex)
grep -P '\d+' file              # Linux (GNU grep)
grep -E '[0-9]+' file           # macOS (BSD grep, sin -P)

# find
find . -name "*.sh" -printf '%f\n'   # Linux (GNU find)
find . -name "*.sh" -exec basename {} \;  # macOS (BSD find, sin -printf)
```

---

## 8. Verificar portabilidad

### 8.1 ShellCheck (T02)

```bash
# ShellCheck detecta bashismos con #!/bin/sh
shellcheck --shell=sh script.sh

# Forzar verificación POSIX incluso con #!/bin/bash
shellcheck --shell=sh script.sh
```

### 8.2 checkbashisms (Debian)

```bash
# Herramienta específica para detectar bashismos
sudo apt install devscripts

checkbashisms script.sh
# script.sh: line 5: [[ ]] is a bashism
# script.sh: line 8: arrays are a bashism
# script.sh: line 12: source is a bashism (use . instead)
```

### 8.3 Testear con dash directamente

```bash
# Ejecutar el script con dash para verificar
dash script.sh

# O cambiar shebang temporalmente y ejecutar
#!/bin/dash
```

### 8.4 Docker como entorno de test

```bash
# Testear en Alpine (ash/busybox)
docker run --rm -v "$PWD:/scripts" alpine sh /scripts/script.sh

# Testear en Debian con dash
docker run --rm -v "$PWD:/scripts" debian dash /scripts/script.sh

# Testear en BusyBox puro
docker run --rm -v "$PWD:/scripts" busybox sh /scripts/script.sh
```

---

## 9. Cuándo usar POSIX sh vs Bash

### 9.1 Diagrama de decisión

```
    ¿Dónde se ejecutará el script?
                │
    ┌───────────┼───────────┐
    │           │           │
    Solo mi     Múltiples   Docker /
    máquina     sistemas    containers
    │           │           │
    │           │           ▼
    │           │     ¿La imagen
    │           │     tiene bash?
    │           │     │         │
    │           │    SÍ        NO
    │           │     │         │
    ▼           ▼     ▼         ▼
  #!/bin/bash  #!/bin/bash  #!/bin/sh
  usa todo     usa todo     solo POSIX
  Bash 5.x     pero evita
               Bash 4.4+
               si hay macOS
```

### 9.2 Regla general

| Contexto | Shebang | Justificación |
|----------|---------|---------------|
| Script personal / servidor controlado | `#!/bin/bash` | Control total del entorno |
| Script de equipo (Linux only) | `#!/usr/bin/env bash` | Portable entre distros Linux |
| Script cross-platform (macOS + Linux) | `#!/usr/bin/env bash` | Pero evitar features Bash 4.4+ |
| Dockerfile / entrypoint | `#!/bin/sh` | Funciona en Alpine sin instalar bash |
| CI/CD runner genérico | `#!/bin/sh` o `#!/usr/bin/env bash` | Depende del runner |
| Distribución / paquete | `#!/bin/sh` | Máxima portabilidad |
| Script de sistema (`init.d`, hooks) | `#!/bin/sh` | No depender de bash instalado |

### 9.3 Checklist antes de elegir

```
¿Necesito arrays?                    → Bash
¿Necesito [[ ]] con regex/glob?      → Bash
¿Necesito process substitution?      → Bash
¿Necesito set -o pipefail?           → Bash
¿El script va en Dockerfile Alpine?  → POSIX sh
¿El script va en paquete .deb/.rpm?  → POSIX sh
¿Solo hago if/for/case simples?      → POSIX sh
```

---

## 10. Template de script POSIX

```sh
#!/bin/sh
#
# script-name.sh — Breve descripción
#
# Uso:
#   ./script-name.sh [opciones] <argumento>

set -eu    # -e = errexit, -u = nounset (NO hay pipefail en POSIX)

# Constants
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SCRIPT_NAME="$(basename "$0")"

# Functions
usage() {
  printf "Usage: %s [options] <argument>\n" "$SCRIPT_NAME"
  printf "Options:\n"
  printf "  -v    Verbose output\n"
  printf "  -h    Show this help\n"
  exit "${1:-0}"
}

log() {
  printf "[%s] %s\n" "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >&2
}

die() {
  log "ERROR: $*"
  exit 1
}

cleanup() {
  [ -d "${tmpdir:-}" ] && rm -rf "$tmpdir"
}
trap cleanup EXIT

# Parse arguments
VERBOSE=false
while getopts "vh" opt; do
  case "$opt" in
    v) VERBOSE=true ;;
    h) usage 0 ;;
    *) usage 1 ;;
  esac
done
shift $((OPTIND - 1))

# Validate
[ $# -ge 1 ] || die "Missing required argument. Use -h for help."
TARGET="$1"

# Main
tmpdir=$(mktemp -d)

log "Starting: target=$TARGET"
$VERBOSE && log "Verbose mode enabled"

# ... lógica del script ...

log "Done"
```

Comparación con el template Bash de T01:

| Elemento | Template POSIX | Template Bash |
|----------|---------------|---------------|
| Shebang | `#!/bin/sh` | `#!/bin/bash` |
| Modo estricto | `set -eu` | `set -euo pipefail` |
| Comillas en `[ ]` | Obligatorias | Opcionales con `[[ ]]` |
| Echo | `printf` | `echo` OK |
| Source | `. file` | `source file` |
| readonly | `VAR="x"` (sin readonly) | `readonly VAR="x"` |
| SCRIPT_DIR | `$(cd "$(dirname "$0")" && pwd)` | `$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)` |

---

## 11. Errores comunes

| Error | Causa | Solución |
|-------|-------|----------|
| `[[: not found` | Script con `#!/bin/sh` usa `[[ ]]` | Usar `[ ]` o cambiar a `#!/bin/bash` |
| `Syntax error: "(" unexpected` | Arrays en POSIX sh | Usar `$@` o IFS-separated strings |
| `echo -e` imprime `-e` literal | Shell no soporta `echo -e` | Usar `printf` siempre |
| `source: not found` | `source` no es POSIX | Usar `. ./file.sh` |
| `==: unexpected operator` | `==` en `[ ]` con dash | Usar `=` (POSIX) en `[ ]` |
| `{1..10}` imprime literal | Brace expansion no es POSIX | Usar `seq 1 10` o while loop |
| Script falla solo en cron | cron usa `/bin/sh` | Shebang `#!/bin/bash` o `SHELL=/bin/bash` en crontab |
| Script falla en Alpine Docker | No hay bash instalado | `apk add bash` o reescribir en POSIX sh |
| `sed -i` falla en macOS | BSD sed requiere `sed -i ''` | Usar `sed -i'' 's/...//' file` o detectar OS |
| `readlink -f` falla en macOS | BSD readlink no tiene `-f` | Usar `realpath` o `cd/pwd` trick |
| `set -o pipefail` falla en dash | pipefail no es POSIX | Capturar exit codes manualmente o usar bash |
| `local` no es POSIX estricto | Aunque funciona en dash/ash | Funciona en la práctica; puristas usan subshells |

---

## 12. Ejercicios

### Ejercicio 1 — Identificar bashismos

Identifica todos los bashismos en este script:

```bash
#!/bin/sh
set -euo pipefail

source ./config.sh

declare -a servers=("web01" "web02" "db01")
for server in "${servers[@]}"; do
  if [[ "$server" == web* ]]; then
    echo -e "Deploying to:\t$server"
    ssh "$server" 'systemctl restart app' &> /dev/null
  fi
done
echo "Done: ${#servers[@]} servers"
```

**Prediccion**: ¿Cuántos bashismos hay? ¿Cuáles romperían en dash?

<details>
<summary>Ver solución</summary>

**8 bashismos** que romperían en dash:

1. `set -o pipefail` — no existe en POSIX sh
2. `source` — POSIX usa `.`
3. `declare -a` — arrays no existen en POSIX
4. `("web01" "web02" "db01")` — sintaxis de array
5. `"${servers[@]}"` — expansión de array
6. `[[ ]]` — POSIX usa `[ ]`
7. `echo -e` — no portable (imprime `-e` literal en dash)
8. `&>` — redirección combinada no es POSIX

`${#servers[@]}` (item extra) también fallaría por depender de arrays.

Versión POSIX:

```sh
#!/bin/sh
set -eu

. ./config.sh

for server in web01 web02 db01; do
  case "$server" in
    web*)
      printf "Deploying to:\t%s\n" "$server"
      ssh "$server" 'systemctl restart app' > /dev/null 2>&1
      ;;
  esac
done
echo "Done"
```

Sin arrays, se pierde el conteo dinámico. Si necesitas contar, usa un contador
manual: `count=0; ... count=$((count + 1)); ...`
</details>

---

### Ejercicio 2 — Reescribir [[ ]] a POSIX

Reescribe estas condiciones Bash a POSIX sh:

```bash
# A
[[ -z "$var" && -f "$file" ]]

# B
[[ "$name" == *.log ]]

# C
[[ "$input" =~ ^[0-9]+$ ]]

# D
[[ "$x" > "$y" ]]
```

**Prediccion**: ¿Cuál es la más difícil de traducir y por qué?

<details>
<summary>Ver solución</summary>

```sh
# A — combinar condiciones con -a o con &&
[ -z "$var" ] && [ -f "$file" ]
# Nota: [ -z "$var" -a -f "$file" ] funciona pero -a está obsoleto en POSIX

# B — glob match con case
case "$name" in
  *.log) true ;;
  *) false ;;
esac

# C — regex con grep
echo "$input" | grep -qE '^[0-9]+$'

# D — comparación lexicográfica
[ "$x" \> "$y" ]    # \ necesario porque > es redirección en [ ]
# O usar expr:
expr "$x" \> "$y" > /dev/null
```

**C es la más difícil** porque POSIX `[ ]` no tiene regex. La solución con `grep -qE`
requiere un fork (subproceso) y pipe, mientras que `[[ =~ ]]` es interno de Bash
(sin fork). En loops con muchas iteraciones, la diferencia de performance es notable.

La alternativa sin fork para C (solo dígitos):
```sh
case "$input" in
  *[!0-9]*|"") false ;;
  *) true ;;
esac
```
</details>

---

### Ejercicio 3 — Script Docker-compatible

Reescribe este script para que funcione en Alpine sin instalar bash:

```bash
#!/bin/bash
set -euo pipefail

declare -A config
while IFS='=' read -r key value; do
  config[$key]="$value"
done < /etc/app/config.env

echo "Host: ${config[DB_HOST]}"
echo "Port: ${config[DB_PORT]}"

if [[ "${config[DEBUG]:-false}" == "true" ]]; then
  echo -e "Debug mode:\tenabled"
fi
```

**Prediccion**: ¿Cuál es la parte más compleja de reemplazar sin arrays asociativos?

<details>
<summary>Ver solución</summary>

```sh
#!/bin/sh
set -eu

# Sin arrays asociativos — usar variables dinámicas o grep
config_file="/etc/app/config.env"

# Opción 1: source directo (si el formato es compatible)
# Cada línea KEY=value se convierte en variable de shell
while IFS='=' read -r key value; do
  # Sanitizar key (solo alfanumérico y _)
  case "$key" in
    *[!A-Za-z0-9_]*) continue ;;
  esac
  eval "${key}='${value}'"
done < "$config_file"

printf "Host: %s\n" "${DB_HOST:-unset}"
printf "Port: %s\n" "${DB_PORT:-unset}"

if [ "${DEBUG:-false}" = "true" ]; then
  printf "Debug mode:\tenabled\n"
fi
```

```sh
# Opción 2: grep para extraer valores (sin eval, más seguro)
#!/bin/sh
set -eu

config_file="/etc/app/config.env"

get_config() {
  grep "^${1}=" "$config_file" | cut -d= -f2-
}

printf "Host: %s\n" "$(get_config DB_HOST)"
printf "Port: %s\n" "$(get_config DB_PORT)"

if [ "$(get_config DEBUG)" = "true" ]; then
  printf "Debug mode:\tenabled\n"
fi
```

**Los arrays asociativos** son la parte más compleja. POSIX sh no tiene nada
equivalente. Las alternativas son:
- `eval` con variables dinámicas (funcional pero riesgo de inyección si no se sanitiza)
- `grep` sobre el archivo original (más seguro, más lento por forks)
- Simplemente hacer `source`/`.` del archivo si el formato es `KEY=value`

La opción 2 (grep) es la más segura para entornos Docker donde el config viene
de fuentes no controladas.
</details>

---

### Ejercicio 4 — echo vs printf

Reemplaza todos los `echo` por `printf` para máxima portabilidad:

```bash
echo "Name: $name"
echo -e "Line1\nLine2"
echo -n "No newline"
echo ""
echo "$variable_that_might_start_with_dash"
```

**Prediccion**: ¿En qué caso `echo "$var"` puede producir output inesperado?

<details>
<summary>Ver solución</summary>

```sh
printf "Name: %s\n" "$name"
printf "Line1\nLine2\n"
printf "No newline"
printf "\n"
printf "%s\n" "$variable_that_might_start_with_dash"
```

`echo "$var"` produce output inesperado cuando `$var` empieza con `-`:

```sh
var="-e hello"
echo "$var"
# Algunas shells: interpretan -e como flag → imprimen "hello" sin "-e"
# Otras shells: imprimen "-e hello" literal
# Resultado: no es predecible entre shells

printf "%s\n" "$var"
# SIEMPRE imprime "-e hello" — el %s trata todo como dato, no como flags
```

`printf "%s\n" "$var"` es la forma **universalmente segura** de imprimir una
variable. `echo` es aceptable para strings literales sin escapes
(`echo "hello world"`), pero nunca para variables que pueden contener datos
arbitrarios.
</details>

---

### Ejercicio 5 — Portabilidad macOS

El siguiente script funciona en Linux pero falla en macOS. Identifica los
problemas y corrígelos para que funcione en ambos:

```bash
#!/usr/bin/env bash

file="data.csv"
backup="data.csv.bak"

# Backup
cp "$file" "$backup"

# Editar in-place
sed -i 's/old/new/g' "$file"

# Fecha del archivo
mod_date=$(date -d "$(stat -c '%Y' "$file")" +%Y-%m-%d)
echo "Modified: $mod_date"

# Líneas con números
grep -P '\d+' "$file" | head -5
```

**Prediccion**: ¿Cuántos comandos fallan en macOS y por qué?

<details>
<summary>Ver solución</summary>

**3 comandos** fallan en macOS:

1. **`sed -i`** — BSD sed requiere argumento de backup: `sed -i '' 's/old/new/g'`
2. **`stat -c '%Y'`** — GNU stat; BSD stat usa `stat -f '%m'`
3. **`grep -P`** — BSD grep no tiene `-P` (Perl regex)

`date -d` también fallaría si se llega a ejecutar, pero `stat` falla primero.

Versión cross-platform:

```bash
#!/usr/bin/env bash

file="data.csv"
backup="data.csv.bak"

cp "$file" "$backup"

# sed -i portable
if sed --version 2>/dev/null | grep -q GNU; then
  sed -i 's/old/new/g' "$file"
else
  sed -i '' 's/old/new/g' "$file"    # BSD (macOS)
fi

# stat portable
if stat --version 2>/dev/null | grep -q GNU; then
  mod_epoch=$(stat -c '%Y' "$file")
else
  mod_epoch=$(stat -f '%m' "$file")    # BSD (macOS)
fi
mod_date=$(date -r "$mod_epoch" +%Y-%m-%d 2>/dev/null || \
           date -d "@$mod_epoch" +%Y-%m-%d)
echo "Modified: $mod_date"

# grep sin -P (portable)
grep -E '[0-9]+' "$file" | head -5
```

En la práctica, si necesitas portabilidad macOS/Linux frecuente, instala
GNU coreutils con `brew install coreutils` y usa `gstat`, `gsed`, `gdate`.
</details>

---

### Ejercicio 6 — set -o pipefail sin pipefail

Escribe un equivalente POSIX de este pipeline con `pipefail`:

```bash
#!/bin/bash
set -eo pipefail
data=$(curl -sf "$url" | jq '.result')
echo "Data: $data"
```

**Prediccion**: ¿Cómo detectas que `curl` falló si `jq` tiene éxito?

<details>
<summary>Ver solución</summary>

```sh
#!/bin/sh
set -eu

# Opción 1: archivo temporal (más simple)
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

if ! curl -sf "$url" > "$tmp"; then
  echo "curl failed" >&2
  exit 1
fi

data=$(jq '.result' "$tmp")
echo "Data: $data"
```

```sh
# Opción 2: verificar cada etapa con named pipe
#!/bin/sh
set -eu

fifo=$(mktemp -u)
mkfifo "$fifo"
trap 'rm -f "$fifo"' EXIT

curl -sf "$url" > "$fifo" &
curl_pid=$!

data=$(jq '.result' < "$fifo")
wait "$curl_pid" || { echo "curl failed" >&2; exit 1; }

echo "Data: $data"
```

Sin `pipefail`, la única forma de detectar que `curl` falló en un pipeline
es **no usar pipeline**: separar los comandos en pasos secuenciales con
archivos temporales, o usar background + wait para capturar el exit code.

La opción 1 (archivo temporal) es la más clara y robusta. La opción 2 (named
pipe) preserva el streaming pero es más compleja.
</details>

---

### Ejercicio 7 — Detectar el shell actual

Escribe una función que detecte qué shell está ejecutando el script y muestre
sus capacidades:

**Prediccion**: ¿`$BASH_VERSION` existe en dash? ¿Y `$0`?

<details>
<summary>Ver solución</summary>

```sh
#!/bin/sh

detect_shell() {
  if [ -n "${BASH_VERSION:-}" ]; then
    printf "Shell: bash %s\n" "$BASH_VERSION"
    printf "  Arrays: yes\n"
    printf "  [[ ]]: yes\n"
    printf "  pipefail: yes\n"
  elif [ -n "${ZSH_VERSION:-}" ]; then
    printf "Shell: zsh %s\n" "$ZSH_VERSION"
    printf "  Arrays: yes\n"
    printf "  [[ ]]: yes\n"
    printf "  pipefail: yes\n"
  elif [ -n "${KSH_VERSION:-}" ]; then
    printf "Shell: ksh %s\n" "$KSH_VERSION"
  else
    # Probably dash, ash, or busybox sh
    shell_path="$(readlink -f /proc/$$/exe 2>/dev/null || echo "unknown")"
    printf "Shell: POSIX sh (%s)\n" "$shell_path"
    printf "  Arrays: no\n"
    printf "  [[ ]]: no\n"
    printf "  pipefail: no\n"
  fi

  printf "  PID: %s\n" "$$"
  printf "  Script: %s\n" "$0"
}

detect_shell
```

`$BASH_VERSION` **no existe** en dash (está undefined). Por eso se usa
`${BASH_VERSION:-}` con el default vacío — sin eso, `set -u` abortaría.

`$0` **sí existe** en todos los shells POSIX — es el nombre del script o shell.
Es una de las pocas variables universales.

Variables de detección por shell:
- Bash: `$BASH_VERSION`, `$BASH`
- Zsh: `$ZSH_VERSION`
- Ksh: `$KSH_VERSION`
- Dash/ash: ninguna específica — detectar por exclusión
</details>

---

### Ejercicio 8 — Migrar script completo

Convierte este script Bash a POSIX sh:

```bash
#!/bin/bash
set -euo pipefail

readonly LOG_DIR="/var/log/myapp"
declare -a log_files=()

mapfile -t log_files < <(find "$LOG_DIR" -name "*.log" -mtime -7)

total_errors=0
for f in "${log_files[@]}"; do
  count=$(grep -c "ERROR" "$f" 2>/dev/null || true)
  (( total_errors += count ))
  [[ $count -gt 0 ]] && echo "$f: $count errors"
done

echo "Total errors (last 7 days): $total_errors"
```

**Prediccion**: ¿Cuál es el mayor riesgo al migrar de `mapfile` a un `while read` loop?

<details>
<summary>Ver solución</summary>

```sh
#!/bin/sh
set -eu

LOG_DIR="/var/log/myapp"

total_errors=0
find "$LOG_DIR" -name "*.log" -mtime -7 | while IFS= read -r f; do
  count=$(grep -c "ERROR" "$f" 2>/dev/null || echo "0")
  total_errors=$((total_errors + count))
  [ "$count" -gt 0 ] && printf "%s: %s errors\n" "$f" "$count"
done

printf "Total errors (last 7 days): %s\n" "$total_errors"
```

**Riesgo principal**: el `while read` con pipe (`find | while`) ejecuta el loop
en una **subshell**. `total_errors` se modifica en la subshell y el valor se
pierde al volver al proceso padre. El `printf` final imprime `0`.

Corrección — evitar el pipe subshell:

```sh
#!/bin/sh
set -eu

LOG_DIR="/var/log/myapp"

# Guardar lista en archivo temporal para evitar pipe subshell
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT
find "$LOG_DIR" -name "*.log" -mtime -7 > "$tmp"

total_errors=0
while IFS= read -r f; do
  count=$(grep -c "ERROR" "$f" 2>/dev/null || echo "0")
  total_errors=$((total_errors + count))
  [ "$count" -gt 0 ] && printf "%s: %s errors\n" "$f" "$count"
done < "$tmp"

printf "Total errors (last 7 days): %s\n" "$total_errors"
```

En POSIX sh no hay `< <(cmd)` (process substitution), así que el archivo
temporal es la alternativa más limpia. El redirect `< "$tmp"` mantiene el
`while` en el proceso principal, preservando la variable `total_errors`.
</details>

---

### Ejercicio 9 — Entrypoint Docker portable

Escribe un entrypoint.sh para Docker que:
1. Funcione en Alpine (sin bash)
2. Espere a que un servicio esté disponible (health check)
3. Configure variables de entorno con defaults
4. Pase el control al CMD (`exec "$@"`)

**Prediccion**: ¿Por qué se usa `exec "$@"` y no simplemente `"$@"`?

<details>
<summary>Ver solución</summary>

```sh
#!/bin/sh
set -eu

# Defaults
DB_HOST="${DB_HOST:-localhost}"
DB_PORT="${DB_PORT:-5432}"
MAX_RETRIES="${MAX_RETRIES:-30}"
RETRY_INTERVAL="${RETRY_INTERVAL:-2}"

# Logging
log() {
  printf "[entrypoint] %s\n" "$*" >&2
}

# Health check — esperar a que DB esté disponible
wait_for_db() {
  local retries=0
  log "Waiting for $DB_HOST:$DB_PORT..."

  while ! nc -z "$DB_HOST" "$DB_PORT" 2>/dev/null; do
    retries=$((retries + 1))
    if [ "$retries" -ge "$MAX_RETRIES" ]; then
      log "ERROR: timeout waiting for $DB_HOST:$DB_PORT"
      exit 1
    fi
    log "  retry $retries/$MAX_RETRIES..."
    sleep "$RETRY_INTERVAL"
  done

  log "Database available at $DB_HOST:$DB_PORT"
}

# Main
log "Starting with DB_HOST=$DB_HOST DB_PORT=$DB_PORT"
wait_for_db

# Pasar control al CMD del Dockerfile
log "Executing: $*"
exec "$@"
```

`exec "$@"` **reemplaza** el proceso del shell con el comando CMD. Sin `exec`,
el CMD se ejecutaría como hijo del shell, y las señales (SIGTERM de Docker) irían
al shell padre en vez del proceso de la aplicación. Con `exec`:
- El proceso CMD tiene PID 1 (recibe señales directamente)
- No queda un shell zombie como padre
- Docker puede enviar SIGTERM/SIGKILL al proceso correcto para shutdown graceful

`"$@"` sin `exec` también funciona, pero el proceso de la app sería PID > 1 y
Docker enviaría SIGTERM al shell (PID 1), que podría no propagarlo correctamente.
</details>

---

### Ejercicio 10 — Test de portabilidad

Escribe un script que verifique si otro script es portable, ejecutándolo
en múltiples shells y comparando resultados:

**Prediccion**: ¿Todos los shells POSIX producen exactamente el mismo output
para el mismo script POSIX?

<details>
<summary>Ver solución</summary>

```sh
#!/bin/sh
set -eu

script="${1:?Usage: $0 <script.sh> [args...]}"
shift

if [ ! -f "$script" ]; then
  printf "ERROR: %s not found\n" "$script" >&2
  exit 1
fi

# Shells a testear (solo los que existan)
passed=0
failed=0
total=0

for shell in sh dash bash ash; do
  shell_path="$(command -v "$shell" 2>/dev/null || true)"
  [ -z "$shell_path" ] && continue

  total=$((total + 1))
  printf "Testing with %-8s (%s)... " "$shell" "$shell_path"

  output_file=$(mktemp)
  if "$shell_path" "$script" "$@" > "$output_file" 2>&1; then
    printf "PASS\n"
    passed=$((passed + 1))
  else
    printf "FAIL (exit code: %s)\n" "$?"
    printf "  Output: %s\n" "$(head -3 "$output_file")"
    failed=$((failed + 1))
  fi
  rm -f "$output_file"
done

printf "\nResults: %s/%s passed" "$passed" "$total"
[ "$failed" -gt 0 ] && printf " (%s failed)" "$failed"
printf "\n"

[ "$failed" -eq 0 ]
```

**No**, los shells POSIX no garantizan output idéntico. POSIX define la
**semántica** (qué debe hacer), no la implementación exacta. Diferencias
posibles:

- `echo` puede manejar escapes distinto (`\n`, `-e`)
- Formato de mensajes de error varía entre shells
- Orden de expansión en edge cases puede diferir
- `printf` con locale puede afectar formato numérico
- Whitespace en output de comandos como `wc` varía

Para tests de portabilidad robustos, comparar exit codes y output normalizado
(trim whitespace, ignorar stderr) es más confiable que comparar byte a byte.
</details>
