# ShellCheck

## Objetivos de aprendizaje

Al terminar este tema deberías poder:

- Ejecutar ShellCheck localmente y entender su output (`SC####`, severidad, fix).
- Corregir las advertencias más comunes de Bash (`SC2086`, `SC2164`, `SC2002`, etc.).
- Decidir cuándo silenciar una regla y documentar correctamente la justificación.
- Integrar ShellCheck en editor, pre-commit y CI/CD sin falsos positivos frecuentes.

## 1. Qué es ShellCheck

ShellCheck es un **analizador estático** para scripts de shell. Lee tu código sin
ejecutarlo y detecta bugs, malas prácticas, y problemas de portabilidad. Es el
equivalente de `clippy` (Rust) o `pylint` (Python) para Bash/sh.

```
    script.sh ──▶ ShellCheck ──▶ warnings + sugerencias
                      │
                 No ejecuta nada
                 Solo analiza el código
```

ShellCheck detecta categorías de problemas que son **invisibles** a simple vista:

```bash
# ShellCheck encuentra esto:
echo $var                  # SC2086: variable sin comillas (word splitting)
[ $x == 1 ]               # warning de portabilidad: == no es POSIX en [ ] (usar =)
cat file | grep pattern    # SC2002: uso inútil de cat (UUOC)
cd /tmp                    # SC2164: cd sin || exit (puede fallar)
rm -rf $dir/               # SC2115: $dir puede ser vacío → rm -rf /
```

---

## 2. Instalación

```bash
# Debian/Ubuntu
sudo apt install shellcheck

# Fedora/RHEL
sudo dnf install ShellCheck

# macOS
brew install shellcheck

# Arch
sudo pacman -S shellcheck

# Snap (cualquier distro)
sudo snap install shellcheck

# Con Cabal (Haskell — compilar desde fuente)
cabal install ShellCheck

# Docker (sin instalar nada)
docker run --rm -v "$PWD:/mnt" -w /mnt koalaman/shellcheck:stable script.sh

# Verificar instalación
shellcheck --version
# ShellCheck - shell script analysis tool
# version: 0.9.0
```

---

## 3. Uso básico

### 3.0 Quickstart (60 segundos)

```bash
# 1) Crear script con un bug común
cat <<'EOF' > quick.sh
#!/bin/bash
echo $1
EOF

# 2) Analizar
shellcheck quick.sh

# 3) Corregir
sed -i 's/echo $1/echo "$1"/' quick.sh

# 4) Verificar que quedó limpio
shellcheck quick.sh
```

Si el segundo `shellcheck` no muestra warnings, ya tienes el flujo base dominado:
escribir → analizar → corregir → re-analizar.

### 3.1 Analizar un script

```bash
# Analizar un archivo
shellcheck script.sh

# Analizar múltiples archivos
shellcheck *.sh

# Analizar desde stdin
echo 'echo $1' | shellcheck -

# Especificar shell (si no tiene shebang)
shellcheck --shell=bash script.sh
shellcheck --shell=sh script.sh
```

### 3.2 Output de ejemplo

```bash
cat <<'EOF' > example.sh
#!/bin/bash
echo $1
files=$(ls *.txt)
for f in $files; do
  cat $f | wc -l
done
[ $# -eq 0 ] && echo "No args"
EOF

shellcheck example.sh
```

```
In example.sh line 2:
echo $1
     ^-- SC2086 (info): Double quote to prevent globbing and word splitting.

Did you mean:
echo "$1"

In example.sh line 3:
files=$(ls *.txt)
        ^-- SC2012 (info): Use find instead of ls to better handle non-alphanumeric filenames.
           ^----^ SC2035 (info): Use ./*.txt so names with dashes won't become options.

In example.sh line 4:
for f in $files; do
         ^----^ SC2086 (info): Double quote to prevent globbing and word splitting.

In example.sh line 5:
  cat $f | wc -l
      ^-- SC2086 (info): Double quote to prevent globbing and word splitting.
  ^-- SC2002 (info): Useless use of cat. Consider 'cmd < file | ...' or 'cmd file | ...' instead.

In example.sh line 7:
[ $# -eq 0 ] && echo "No args"
  ^-- SC2086 (info): Double quote to prevent globbing and word splitting.
```

> Nota: los códigos exactos y textos pueden variar ligeramente según la versión de
> ShellCheck y el shell objetivo (`--shell=bash` vs `--shell=sh`).

Cada warning tiene:
- **Ubicación**: archivo, línea, columna
- **Código**: SC + número (ej: SC2086)
- **Severidad**: error, warning, info, style
- **Descripción**: qué está mal y por qué
- **Sugerencia**: cómo arreglarlo (cuando aplica)

### 3.3 Formatos de salida

```bash
# Default: formato legible
shellcheck script.sh

# GCC format (para integración con editores)
shellcheck -f gcc script.sh
# script.sh:2:6: note: Double quote to prevent globbing and word splitting. [SC2086]

# JSON (para herramientas)
shellcheck -f json script.sh

# Checkstyle XML (para CI como Jenkins)
shellcheck -f checkstyle script.sh

# diff (muestra los fixes como patch)
shellcheck -f diff script.sh
# Se puede aplicar directamente:
shellcheck -f diff script.sh | git apply

# TTY (default con colores)
shellcheck -f tty script.sh

# quiet (solo exit code — para CI)
shellcheck -f quiet script.sh
echo $?    # 0 = limpio, 1 = warnings encontrados
```

---

## 4. Anatomía de un warning — códigos SC

Cada warning tiene un código `SC####` que lo identifica unívocamente. Los códigos
están documentados en la wiki de ShellCheck.

### 4.1 Rangos de códigos

| Rango | Categoría |
|-------|-----------|
| SC1000–SC1999 | **Errores de parsing** (sintaxis inválida) |
| SC2000–SC2999 | **Warnings semánticos** (código válido pero probablemente incorrecto) |
| SC3000–SC3999 | **Portabilidad** (features no POSIX usados con `#!/bin/sh`) |
| SC4000–SC4999 | **Deprecaciones** (sintaxis obsoleta) |

### 4.2 Severidades

| Severidad | Significado | Acción |
|-----------|-------------|--------|
| `error` | Bug casi seguro o sintaxis inválida | Arreglar siempre |
| `warning` | Probablemente incorrecto, riesgo alto | Arreglar salvo justificación |
| `info` | Sugerencia de mejora, riesgo medio | Arreglar idealmente |
| `style` | Estilo o preferencia cosmética | A criterio del equipo |

```bash
# Filtrar por severidad
shellcheck --severity=warning script.sh    # solo warning y error
shellcheck --severity=error script.sh      # solo errores
shellcheck --severity=style script.sh      # todo (default)
```

---

## 5. Las reglas más importantes

### 5.1 SC2086 — Variable sin comillas (la más común)

```bash
# MAL
echo $var
cp $file /dest/
rm -rf $dir/

# BIEN
echo "$var"
cp "$file" /dest/
rm -rf "$dir/"
```

**Por qué importa**: sin comillas, Bash hace **word splitting** y **globbing**.
Si `$file` es `"my file.txt"`, se parte en dos argumentos: `my` y `file.txt`.
Si `$dir` está vacío, `rm -rf $dir/` se convierte en `rm -rf /`.

```bash
# Demostración del peligro:
file="my file.txt"
touch "$file"

# Sin comillas: word splitting
ls $file
# ls: cannot access 'my': No such file or directory
# ls: cannot access 'file.txt': No such file or directory

# Con comillas: correcto
ls "$file"
# my file.txt
```

### 5.2 SC2046 — Command substitution sin comillas

```bash
# MAL
files=$(find . -name "*.txt")
for f in $files; do     # word splitting en nombres con espacios
  process "$f"
done

# BIEN — iterar directamente con find
find . -name "*.txt" -print0 | while IFS= read -r -d '' f; do
  process "$f"
done

# O con mapfile (Bash 4.4+)
mapfile -d '' files < <(find . -name "*.txt" -print0)
for f in "${files[@]}"; do
  process "$f"
done
```

### 5.3 SC2164 — cd sin verificación de error

```bash
# MAL — si cd falla, los comandos siguientes se ejecutan en directorio incorrecto
cd /some/dir
rm -rf *          # PELIGRO: puede borrar en directorio equivocado

# BIEN
cd /some/dir || exit 1
rm -rf *

# BIEN — con mensaje
cd /some/dir || { echo "ERROR: no se pudo entrar a /some/dir" >&2; exit 1; }
```

Con `set -e` (T01), el `cd` fallido aborta el script. Pero ShellCheck avisa
porque no todos los scripts usan `set -e`, y ser explícito es mejor.

### 5.4 SC2002 — Uso inútil de cat (UUOC)

```bash
# MAL — "Useless Use of Cat"
cat file.txt | grep "pattern"
cat data.csv | awk -F, '{ print $2 }'

# BIEN — el comando puede leer el archivo directamente
grep "pattern" file.txt
awk -F, '{ print $2 }' data.csv

# BIEN — redirección (cuando el comando lee stdin)
grep "pattern" < file.txt
```

`cat file | cmd` crea un proceso extra innecesario. La mayoría de comandos aceptan
archivos como argumento directamente.

### 5.5 SC2155 — Declarar y asignar en la misma línea

```bash
# MAL — el exit code de local oculta el de la command substitution
local result=$(some_command)    # exit code siempre 0 (de local)

# BIEN — separar declaración y asignación
local result
result=$(some_command)          # exit code refleja some_command
```

Esto se conecta directamente con `set -e` (T01) — la trampa del `local`.

### 5.6 SC2012 — Parsear la salida de ls

```bash
# MAL — ls no maneja bien nombres especiales
for f in $(ls *.txt); do
  echo "$f"
done

# BIEN — glob directo
for f in ./*.txt; do
  [[ -e "$f" ]] || continue    # manejar caso sin matches
  echo "$f"
done

# BIEN — find para recursivo
find . -name "*.txt" -exec echo {} \;
```

`ls` produce output diseñado para humanos, no para parsing programático. Nombres
con espacios, newlines, o caracteres especiales lo rompen.

### 5.7 SC2162 — read sin -r

```bash
# MAL — sin -r, backslash se interpreta como escape
read line

# BIEN — -r desactiva interpretación de backslash
read -r line

# BIEN — patrón completo para leer líneas
while IFS= read -r line; do
  echo "$line"
done < file.txt
```

### 5.8 SC2034 — Variable asignada pero no usada

```bash
# ShellCheck avisa:
unused_var="hello"    # SC2034: unused_var appears unused

# Si la variable se usa en otro script (sourced), silenciar:
# shellcheck disable=SC2034
exported_config="value"
```

### 5.9 SC2115 — rm con variable que puede ser vacía

```bash
# MAL — si $dir está vacío: rm -rf /
rm -rf "$dir/"

# BIEN — verificar que no esté vacío
rm -rf "${dir:?ERROR: dir is empty}/"

# BIEN — verificar primero
[[ -n "$dir" ]] && rm -rf "$dir/"
```

### 5.10 SC3039/SC3010 — Features de Bash con shebang sh

```bash
#!/bin/sh
# SC3039: [[ ]] no es POSIX
[[ -f "$file" ]] && echo "exists"

# SC3010: == en [ ] no es POSIX
[ "$x" == "y" ]

# Correcciones POSIX:
[ -f "$file" ] && echo "exists"
[ "$x" = "y" ]

# O cambiar el shebang a bash:
#!/bin/bash
```

---

## 6. Directivas — controlar ShellCheck

### 6.1 Desactivar un warning específico

```bash
# Desactivar para la siguiente línea
# shellcheck disable=SC2086
echo $var_that_i_know_is_safe

# Desactivar múltiples códigos
# shellcheck disable=SC2086,SC2002
cat $file | process

# Desactivar para todo el archivo (poner justo después del shebang)
#!/bin/bash
# shellcheck disable=SC2086
echo $var1
echo $var2
```

### 6.2 Directivas inline

```bash
# Especificar shell para el archivo
# shellcheck shell=bash

# Indicar que un archivo es sourced (no necesita shebang)
# shellcheck shell=bash

# Especificar archivos que se sourcean
# shellcheck source=./lib/utils.sh
source "$SCRIPT_DIR/lib/utils.sh"

# Indicar que source es dinámico (no verificable)
# shellcheck source=/dev/null
source "$config_file"

# external-sources: seguir sources a archivos externos
# shellcheck external-sources=true
```

### 6.3 Archivo de configuración .shellcheckrc

Crear `.shellcheckrc` en la raíz del proyecto o en `$HOME`:

```bash
# .shellcheckrc

# Desactivar globalmente (para todo el proyecto)
disable=SC2059    # no exigir format string en printf
disable=SC1091    # no seguir source de archivos no encontrados

# Shell por defecto
shell=bash

# Severidad mínima
severity=style

# Habilitar extensiones
enable=all
```

```bash
# Ubicaciones que ShellCheck busca (en orden):
# 1. .shellcheckrc en el directorio del script
# 2. .shellcheckrc en directorios padre (hasta /)
# 3. $HOME/.shellcheckrc
# 4. $XDG_CONFIG_HOME/shellcheckrc
```

### 6.4 Cuándo silenciar vs cuándo arreglar

```
    ¿ShellCheck reporta un warning?
                │
                ▼
    ¿Es un falso positivo?
         │            │
        SÍ           NO
         │            │
         ▼            ▼
    Silenciar     ¿Es fácil de arreglar?
    con disable        │            │
    + comentario      SÍ           NO
    explicando         │            │
                       ▼            ▼
                  Arreglar     ¿Hay riesgo real?
                                │           │
                               SÍ          NO
                                │           │
                                ▼           ▼
                           Arreglar    Silenciar
                                       + comentario
```

**Regla**: nunca silenciar sin un comentario explicando por qué. El próximo
desarrollador (o tú en 6 meses) necesita saber que fue una decisión deliberada.

```bash
# MAL — silenciar sin explicación
# shellcheck disable=SC2086
cmd $args

# BIEN — con justificación
# $args está pre-validado y requiere word splitting para pasar múltiples flags
# shellcheck disable=SC2086
cmd $args
```

---

## 7. Integración con editores

### 7.1 VS Code

```
1. Instalar extensión: "ShellCheck" (timonwong.shellcheck)
2. Instalar shellcheck en el sistema
3. Los warnings aparecen automáticamente como squiggly lines
```

Configuración en `settings.json`:

```json
{
  "shellcheck.enable": true,
  "shellcheck.executablePath": "shellcheck",
  "shellcheck.run": "onSave",
  "shellcheck.customArgs": [
    "--severity=style"
  ],
  "shellcheck.exclude": [
    "SC1091"
  ]
}
```

### 7.2 Vim/Neovim

```vim
" Con ALE (Asynchronous Lint Engine)
" Plug 'dense-analysis/ale'
let g:ale_linters = {'sh': ['shellcheck']}
let g:ale_sh_shellcheck_options = '--severity=style'

" Con Neovim + null-ls
" ShellCheck funciona automáticamente si está instalado

" Con Neovim nativo (0.5+)
" Usando nvim-lint:
" require('lint').linters_by_ft = { sh = {'shellcheck'} }
```

### 7.3 Emacs

```elisp
;; Con flycheck
(require 'flycheck)
(add-hook 'sh-mode-hook 'flycheck-mode)
;; flycheck detecta shellcheck automáticamente
```

### 7.4 Cualquier editor con LSP

```bash
# bash-language-server incluye ShellCheck
npm install -g bash-language-server

# Integra automáticamente ShellCheck si está instalado
# Funciona con cualquier editor que soporte LSP
```

---

## 8. Integración con CI/CD

### 8.1 GitHub Actions

```yaml
# .github/workflows/shellcheck.yaml
name: ShellCheck
on: [push, pull_request]

jobs:
  shellcheck:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install ShellCheck
        run: |
          sudo apt-get update
          sudo apt-get install -y shellcheck

      - name: Run ShellCheck
        run: |
          find ./scripts -name '*.sh' -print0 \
            | xargs -0 shellcheck --severity=warning
```

### 8.2 GitLab CI

```yaml
# .gitlab-ci.yml
shellcheck:
  image: koalaman/shellcheck-alpine:stable
  stage: lint
  script:
    - find . -name '*.sh' -print0 | xargs -0 shellcheck --severity=warning
  allow_failure: false
```

### 8.3 Pre-commit hook

```yaml
# .pre-commit-config.yaml
repos:
  - repo: https://github.com/koalaman/shellcheck-precommit
    rev: v0.9.0
    hooks:
      - id: shellcheck
        args: ['--severity=warning']
```

```bash
# O manualmente en .git/hooks/pre-commit:
#!/bin/bash
set -euo pipefail

mapfile -d '' -t files < <(
  git diff --cached --name-only --diff-filter=ACM -z -- '*.sh'
)

if (( ${#files[@]} > 0 )); then
  echo "Running ShellCheck..."
  shellcheck "${files[@]}"
fi
```

### 8.4 Makefile

```makefile
.PHONY: lint
lint:
	find . -name '*.sh' -not -path './vendor/*' -print0 | xargs -0 shellcheck --severity=warning

.PHONY: lint-fix
lint-fix:
	find . -name '*.sh' -not -path './vendor/*' -print0 | xargs -0 shellcheck -f diff | git apply
```

### 8.5 Script de CI genérico

```bash
#!/bin/bash
# ci-shellcheck.sh — Ejecutar ShellCheck en todos los scripts del repo
set -euo pipefail

severity="${1:-warning}"
errors=0

while IFS= read -r -d '' file; do
  if ! shellcheck --severity="$severity" "$file"; then
    (( errors++ )) || true    # || true: evita abort por set -e cuando errors=0 (T01)
  fi
done < <(find . -name '*.sh' -not -path './.git/*' -print0)

if (( errors > 0 )); then
  echo "ShellCheck: $errors file(s) with issues" >&2
  exit 1
fi

echo "ShellCheck: all files clean"
```

---

## 9. Aplicar fixes automáticamente

ShellCheck puede generar diffs que se aplican directamente:

```bash
# Ver diff sugerido
shellcheck -f diff script.sh

# Aplicar automáticamente con git apply
shellcheck -f diff script.sh | git apply

# Aplicar con patch (sin git)
shellcheck -f diff script.sh | patch -p1

# Verificar antes de aplicar (dry-run)
shellcheck -f diff script.sh | git apply --check

# Fix + verificar en un solo paso
fix_shellcheck() {
  local file="$1"
  local diff
  diff=$(shellcheck -f diff "$file" 2>/dev/null) || true
  if [[ -n "$diff" ]]; then
    echo "$diff" | git apply
    echo "Fixed: $file"
  fi
}
```

**Nota**: no todos los warnings tienen auto-fix. Los que sí lo tienen son
principalmente los de quoting (SC2086) y estilo.

---

## 10. Tabla de reglas más frecuentes

| Código | Severidad | Qué detecta | Fix |
|--------|-----------|-------------|-----|
| SC2086 | info | Variable sin comillas | `"$var"` |
| SC2046 | warning | Command substitution sin comillas | `"$(cmd)"` |
| SC2002 | style | Uso inútil de cat | `cmd < file` o `cmd file` |
| SC2155 | warning | `local var=$(cmd)` oculta exit code | Separar `local` y asignación |
| SC2164 | warning | `cd` sin verificar error | `cd dir \|\| exit 1` |
| SC2012 | info | Parsear `ls` | Usar globs o `find` |
| SC2162 | info | `read` sin `-r` | `read -r` |
| SC2034 | warning | Variable no usada | Eliminar o `# shellcheck disable` |
| SC2115 | warning | `rm` con variable posiblemente vacía | `${var:?msg}` |
| SC2029 | info | Expansión en argumento de `ssh` | Comillas simples para remoto |
| SC2035 | info | Glob sin `./` prefijo | `rm ./*.txt` |
| SC2044 | warning | `for f in $(find)` | `find -exec` o `while read` |
| SC2059 | info | Variable como format string de printf | `printf '%s' "$var"` |
| SC2068 | error | Array sin comillas | `"${arr[@]}"` |
| SC2076 | warning | Comillas en `=~` de regex | Sin comillas: `[[ $x =~ $regex ]]` |
| SC2091 | warning | `$(cmd)` sin nada (ejecuta output) | Verificar intención |
| SC2143 | style | `if [ $(grep) ]` | `if grep -q` |
| SC2181 | style | Verificar `$?` en vez de usar `if cmd` | `if cmd; then` |
| SC2206 | warning | Array desde string sin comillas | `read -ra arr <<< "$str"` |
| SC2230 | warning | `which` no es portable | `command -v` |
| SC1091 | info | Source no encontrado | `# shellcheck source=path` |
| SC3039 | warning | Feature Bash con shebang sh | Cambiar a `#!/bin/bash` o usar POSIX |

---

## 11. Errores comunes con ShellCheck

| Error | Causa | Solución |
|-------|-------|----------|
| `shellcheck: command not found` | No instalado | `apt install shellcheck` |
| No detecta nada en archivo sin shebang | ShellCheck no sabe qué shell | Añadir `#!/bin/bash` o `--shell=bash` |
| Warning en variable intencional sin comillas | ShellCheck no sabe tu intención | `# shellcheck disable=SC2086` con comentario |
| Avisa sobre source no encontrado (SC1091) | Archivo sourced no existe en tiempo de análisis | `# shellcheck source=./real/path.sh` |
| Reporta problemas en código generado | Código no editable manualmente | Excluir archivo en CI o `disable` global |
| No reconoce funciones de archivo sourced | No sigue sources por defecto | `# shellcheck source=./lib.sh` o `external-sources=true` |
| Demasiados warnings al empezar | Proyecto legacy sin ShellCheck | Empezar con `--severity=error`, bajar gradualmente |
| Auto-fix rompe el script | Diff no cubre todos los casos | Revisar diff antes de aplicar, testear después |

---

## 12. Workflow recomendado

```
    1. Escribir script
           │
           ▼
    2. ShellCheck en editor     ← feedback inmediato
       (squiggly lines)
           │
           ▼
    3. Arreglar o silenciar     ← con justificación
       cada warning
           │
           ▼
    4. Pre-commit hook          ← último check local
       shellcheck *.sh
           │
           ▼
    5. CI pipeline              ← gate de calidad
       shellcheck --severity=warning
           │
      ┌────┴────┐
      │ PASS    │ FAIL
      │         │
      ▼         ▼
    Merge    Fix + retry
```

```bash
# Flujo práctico:
# 1. Escribir
vim script.sh

# 2. Check rápido
shellcheck script.sh

# 3. Auto-fix lo que se pueda
shellcheck -f diff script.sh | git apply

# 4. Check de nuevo (arreglar lo que quede manualmente)
shellcheck script.sh

# 5. Commit
git add script.sh
git commit -m "Add script with clean ShellCheck"
```

---

## 13. Ejercicios

### Ejercicio 1 — Identificar warnings

**Predicción**: ¿Cuántos warnings de ShellCheck tiene este script? ¿Cuáles son?

```bash
#!/bin/bash
echo $1
name=$2
if [ $name == "admin" ]; then
  echo "Welcome, " $name
fi
```

<details>
<summary>Ver solución</summary>

**Advertencias principales** (el conteo exacto puede variar por versión/configuración, típicamente ~5):

1. **SC2086** línea 2: `echo $1` → falta comillas → `echo "$1"`
2. **SC2086** línea 4: `[ $name == ... ]` → falta comillas → `[ "$name" == ... ]`
3. **SC2086** línea 4: `$name` sin comillas (dentro de `[ ]`)
4. **SC2086** línea 5: `$name` sin comillas en `echo`

La mayoría son SC2086 (quoting). Es la regla más frecuente.

**Nota**: `==` en `[ ]` es válido en Bash y ShellCheck **no lo reporta** con
`#!/bin/bash`. Solo lo reportaría (SC3014) si el shebang fuera `#!/bin/sh`.
Aun así, usar `[[ ]]` es preferible en Bash. Script corregido:

```bash
#!/bin/bash
echo "$1"
name="$2"
if [[ "$name" == "admin" ]]; then
  echo "Welcome, $name"
fi
```
</details>

---

### Ejercicio 2 — UUOC y quoting

**Predicción**: ¿Qué warnings produce ShellCheck?

```bash
#!/bin/bash
result=$(cat config.txt | grep "PORT" | cut -d= -f2)
echo "Port is: " $result
```

<details>
<summary>Ver solución</summary>

1. **SC2002** (style): `cat config.txt | grep` → Useless Use of Cat. `grep` puede leer el archivo directamente.
2. **SC2086** (info): `$result` sin comillas en `echo`.

Corregido:

```bash
#!/bin/bash
result=$(grep "PORT" config.txt | cut -d= -f2)
echo "Port is: $result"
```

O más robusto:

```bash
#!/bin/bash
result=$(grep "^PORT=" config.txt | cut -d= -f2) || true
echo "Port is: ${result:-not set}"
```

`cat file | cmd` crea un proceso extra innecesario. `grep` (y la mayoría de comandos
Unix) acepta archivos como argumento directo.
</details>

---

### Ejercicio 3 — La trampa del local

**Predicción**: ¿Qué warning genera ShellCheck y por qué es importante?

```bash
#!/bin/bash
set -e

get_config() {
  cat /etc/myapp/config.json
}

process() {
  local data=$(get_config)
  echo "Config: $data"
}

process
```

<details>
<summary>Ver solución</summary>

**SC2155** (warning): `local data=$(get_config)` — Declare and assign separately
to avoid masking return values.

Esto es crítico con `set -e`: si `get_config` falla (archivo no existe), el exit
code del fallo es **ocultado** por `local`, que siempre retorna 0. El script
continúa con `$data` vacío, sin detectar el error.

Corregido:

```bash
process() {
  local data
  data=$(get_config)    # ahora set -e detecta el fallo de get_config
  echo "Config: $data"
}
```

ShellCheck es la herramienta que detecta esta trampa automáticamente — uno de los
bugs más sutiles de Bash (cubierto en T01).
</details>

---

### Ejercicio 4 — cd sin protección

**Predicción**: ¿Qué warning da ShellCheck y cuál es el riesgo real?

```bash
#!/bin/bash
cd /opt/myapp/build
rm -rf *
make install
```

<details>
<summary>Ver solución</summary>

**SC2164** (warning): `cd /opt/myapp/build` — Use `cd ... || exit` in case cd fails.

El riesgo: si `/opt/myapp/build` no existe, `cd` falla pero el script continúa en
el directorio actual. `rm -rf *` borra **todo el contenido del directorio donde
estabas** — potencialmente tu home, el repo, o peor.

Corregido:

```bash
#!/bin/bash
set -euo pipefail   # set -e también protege aquí

cd /opt/myapp/build || { echo "Cannot cd to build dir" >&2; exit 1; }
rm -rf ./*          # ./* para que nombres con - no sean interpretados como flags
make install
```

Notar también el cambio de `*` a `./*` — ShellCheck reportaría **SC2035** si
hubiera archivos que empiezan con `-`, ya que serían interpretados como flags de `rm`.
</details>

---

### Ejercicio 5 — Parsear ls

**Predicción**: ¿Qué warnings genera ShellCheck y cómo se corrige?

```bash
#!/bin/bash
for file in $(ls /var/log/*.log); do
  lines=$(wc -l $file | awk '{print $1}')
  echo "$file: $lines lines"
done
```

<details>
<summary>Ver solución</summary>

Warnings:
1. **SC2012** (info): Use `find` instead of `ls` to better handle non-alphanumeric filenames.
2. **SC2046** (warning): `$(ls ...)` sin comillas — word splitting.
3. **SC2035** (info): Use `./*.log` — nombres con `-` pueden ser opciones.
4. **SC2086** (info): `$file` sin comillas en `wc -l $file`.

Corregido:

```bash
#!/bin/bash
for file in /var/log/*.log; do
  [[ -e "$file" ]] || continue    # manejar caso sin matches
  lines=$(wc -l < "$file")        # < file evita que wc imprima el nombre
  echo "$file: $lines lines"
done
```

Cambios:
- Glob directo en vez de `$(ls ...)` — más seguro y eficiente
- `[[ -e "$file" ]]` — manejar caso donde no hay archivos `.log`
- `wc -l < "$file"` — redirigir para obtener solo el número (sin nombre de archivo)
- Todas las variables con comillas
</details>

---

### Ejercicio 6 — Silenciar correctamente

**Predicción**: ¿Cuál de estos silenciamientos es correcto y cuál es peligroso?

```bash
#!/bin/bash
# Caso A:
# shellcheck disable=SC2086
cmd $deliberately_unquoted_args

# Caso B:
# shellcheck disable=SC2086
rm -rf $user_input/

# Caso C:
# shellcheck disable=SC2002
cat very_large_file.bin | process_binary --stdin
```

<details>
<summary>Ver solución</summary>

- **Caso A**: **Aceptable** (con comentario explicando por qué). A veces necesitas
  word splitting intencional (pasar múltiples flags almacenados en una variable).
  Pero es preferible usar un array: `"${args[@]}"`.

- **Caso B**: **PELIGROSO**. Silenciar SC2086 en una variable que contiene input de
  usuario es una vulnerabilidad. Si `$user_input` está vacío: `rm -rf /`. Si contiene
  espacios o `; malicious_command`: ejecución arbitraria. **Nunca silenciar SC2086
  en variables con input externo.**

- **Caso C**: **Aceptable**. Hay casos legítimos donde `cat file | cmd` es necesario:
  cuando el comando solo acepta stdin, o por claridad en un pipeline largo. El UUOC
  es style, no un bug. Silenciarlo está bien con justificación.

**Regla**: silenciar warnings de **estilo** (SC2002) es seguro. Silenciar warnings
de **seguridad** (SC2086 en input no confiable, SC2115) es peligroso. Siempre
evaluar el contexto antes de silenciar.
</details>

---

### Ejercicio 7 — Portabilidad sh vs bash

**Predicción**: ¿Qué warnings genera ShellCheck con `#!/bin/sh`?

```bash
#!/bin/sh
name="Alice"
if [[ "$name" == "Alice" ]]; then
  echo "Hello, ${name}!"
  arr=(1 2 3)
  echo "${arr[@]}"
fi
```

<details>
<summary>Ver solución</summary>

Con `#!/bin/sh`, ShellCheck reporta bashismos (los códigos pueden variar según versión):

1. `[[ ... ]]` no es POSIX (`sh` espera `[ ... ]`).
2. Operadores/comparaciones estilo Bash reducen portabilidad en `sh` (en POSIX usar `[ "$a" = "$b" ]`).
3. Arrays no existen en POSIX sh — `arr=(1 2 3)` es bashismo.

Versión POSIX:

```sh
#!/bin/sh
name="Alice"
if [ "$name" = "Alice" ]; then
  echo "Hello, ${name}!"
  # No hay arrays en POSIX sh — usar otros patrones
fi
```

Versión Bash (cambiar shebang):

```bash
#!/bin/bash
name="Alice"
if [[ "$name" == "Alice" ]]; then
  echo "Hello, ${name}!"
  arr=(1 2 3)
  echo "${arr[@]}"
fi
```

**ShellCheck valida contra el shell del shebang**. Con `#!/bin/sh`, aplica reglas
POSIX. Con `#!/bin/bash`, permite bashismos. Esto es crucial en Docker (muchas
imágenes usan `dash` como `/bin/sh`) y cron (que puede usar `sh` por defecto).
</details>

---

### Ejercicio 8 — Arreglar script completo

El siguiente script tiene **múltiples problemas** que ShellCheck detectaría.
Corrígelo:

```bash
#!/bin/bash
DIR=$1
if [ -z $DIR ]; then
  echo "Usage: $0 <directory>"
  exit 1
fi

cd $DIR
for f in $(ls *.csv); do
  lines=$(cat $f | wc -l)
  echo $f has $lines lines
  if [ $lines -gt 100 ]; then
    cp $f /archive/
  fi
done
echo Done processing $DIR
```

**Predicción**: ¿Cuántos warnings distintos genera ShellCheck?

<details>
<summary>Ver solución</summary>

Warnings (al menos **10 instancias** de varios códigos):

- SC2086 × 8: `$DIR`, `$f`, `$lines` sin comillas (múltiples ocurrencias)
- SC2164 × 1: `cd $DIR` sin verificación
- SC2012 × 1: `$(ls *.csv)` — parsear ls
- SC2002 × 1: `cat $f | wc -l` — UUOC
- SC2035 × 1: `*.csv` sin `./` prefijo

Script corregido:

```bash
#!/bin/bash
set -euo pipefail

dir="${1:?Usage: $0 <directory>}"

cd "$dir" || { echo "ERROR: cannot cd to $dir" >&2; exit 1; }

for f in ./*.csv; do
  [[ -e "$f" ]] || continue
  lines=$(wc -l < "$f")
  echo "$f has $lines lines"
  if (( lines > 100 )); then
    cp "$f" /archive/
  fi
done

echo "Done processing $dir"
```

Cambios:
- `set -euo pipefail` — protección global
- `${1:?msg}` — validación con mensaje en vez de `[ -z ]`
- `"$dir"` — todas las variables con comillas
- `cd "$dir" || exit` — verificar fallo de cd
- `./*.csv` — glob directo, con `./` prefijo
- `[[ -e "$f" ]]` — manejar caso sin archivos csv
- `wc -l < "$f"` — sin cat, sin nombre en output
- `(( lines > 100 ))` — aritmética Bash en vez de `[ -gt ]`
</details>

---

### Ejercicio 9 — .shellcheckrc

Escribe un `.shellcheckrc` para un proyecto que:
1. Usa Bash (no sh)
2. Tiene scripts que se sourcean mutuamente
3. Quiere ignorar UUOC (el equipo lo prefiere por legibilidad)
4. Quiere detectar todo desde severity `style`

**Predicción**: ¿Dónde debe colocarse el archivo para que aplique a todo el proyecto?

<details>
<summary>Ver solución</summary>

```bash
# .shellcheckrc
# Proyecto: mi-proyecto

# Shell por defecto
shell=bash

# Severidad: reportar todo
severity=style

# Ignorar UUOC — el equipo prefiere cat file | cmd por legibilidad
disable=SC2002

# No reportar source no encontrado (scripts se sourcean dinámicamente)
disable=SC1091

# Seguir sources de archivos externos
external-sources=true
```

El archivo debe colocarse en la **raíz del proyecto** (junto al `.git/`).
ShellCheck busca `.shellcheckrc` empezando desde el directorio del script
analizado y subiendo hasta `/`. El primero que encuentre es el que usa.

```
mi-proyecto/
├── .shellcheckrc        ← aquí
├── .git/
├── scripts/
│   ├── deploy.sh
│   └── lib/
│       └── utils.sh
└── Makefile
```

También se puede tener un `.shellcheckrc` global en `$HOME/.shellcheckrc` como
fallback para proyectos que no tengan uno propio.
</details>

---

### Ejercicio 10 — Pipeline CI

Escribe un paso de CI que ejecute ShellCheck en todos los `.sh` del repo, falle
si hay warnings de severidad `warning` o superior, pero permita `info` y `style`.

**Predicción**: ¿Qué exit code retorna ShellCheck cuando hay solo `info` pero se
filtra con `--severity=warning`?

<details>
<summary>Ver solución</summary>

```yaml
# GitHub Actions
- name: ShellCheck
  run: |
    find . -name '*.sh' -not -path './.git/*' -print0 \
      | xargs -0 shellcheck --severity=warning
```

```bash
# Script genérico para cualquier CI:
#!/bin/bash
set -euo pipefail

echo "=== ShellCheck (severity: warning) ==="

files=()
while IFS= read -r -d '' f; do
  files+=("$f")
done < <(find . -name '*.sh' -not -path './.git/*' -print0)

if (( ${#files[@]} == 0 )); then
  echo "No .sh files found"
  exit 0
fi

shellcheck --severity=warning "${files[@]}"
echo "ShellCheck: all clean"
```

ShellCheck retorna **exit code 0** cuando no hay warnings en o por encima de la
severidad especificada. Con `--severity=warning`, los `info` y `style` se **ignoran
completamente** — ni se muestran ni afectan el exit code.

Exit codes de ShellCheck:
- `0` — sin problemas (en la severidad filtrada)
- `1` — problemas encontrados
- `2` — error de ShellCheck mismo (archivo no encontrado, etc.)

Esto permite una política gradual: empezar con `--severity=error` en proyectos
legacy, e ir bajando a `warning` y luego `style` conforme se limpie el código.
</details>
