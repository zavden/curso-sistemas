# T04 — grep avanzado

> **Fuentes**: README.md + README.max.md + labs/README.md
> **Regla aplicada**: 2 (`.max.md` sin `.claude.md`)

## Errata detectada en las fuentes

| # | Archivo | Línea(s) | Error | Corrección |
|---|---------|----------|-------|------------|
| 1 | max.md | 30-33 | Comentarios BRE con `)` extra: `# \| necesita \)` | Sobra el `)` — debe ser `# \| necesita \` |
| 2 | md/max.md | 133/153 | `--include='*.{c,h}'` — brace expansion no funciona en `--include` | `--include` usa fnmatch globs, no brace expansion; usar `--include='*.c' --include='*.h'` |
| 3 | md/max.md | 201/223 | `grep -P '\d{3}\.\d{3}\.\d{3}\.\d{3}'` exige exactamente 3 dígitos por octeto | Con `{3}` fijo, `192.168.1.1` no matchea (el `1` tiene 1 dígito); debe ser `\d{1,3}` |
| 4 | md/max.md | 280/309 | `grep --mmap` recomendado para rendimiento | `--mmap` fue deprecado en GNU grep 2.6 (2009) y se ignora silenciosamente; eliminado |
| 5 | max.md | 38 | `grep -E '(foo)(bar)\1'` sin notar que `\1` no es POSIX ERE | Backreferences son BRE; con `-E` funcionan solo como extensión GNU (no portable) |
| 6 | labs | 276 | `\d+(?!px)\w+` supuestamente filtra unidades que no son `px` | El backtracking de `\d+` permite que `100px` matchee como `10` + `0px`; se corrige con lookbehind negativo |

---

## Modos de regex

grep soporta tres dialectos de expresiones regulares:

```bash
# BRE — Basic Regular Expressions (default):
grep 'pattern' file
# Los metacaracteres ?, +, {}, (), | necesitan escape con \

# ERE — Extended Regular Expressions (-E):
grep -E 'pattern' file
# equivalente legacy: egrep (no usar en scripts nuevos)
# ?, +, {}, (), | funcionan sin escape

# PCRE — Perl Compatible Regular Expressions (-P):
grep -P 'pattern' file
# Lookahead, lookbehind, \d, \w, \b, non-greedy, etc.
# Solo GNU grep compilado con libpcre/libpcre2
# NO disponible en macOS (BSD grep)
```

### BRE vs ERE vs PCRE

| Característica | BRE (default) | ERE (`-E`) | PCRE (`-P`) |
|---|---|---|---|
| `+` (1 o más) | `\+` | `+` | `+` |
| `?` (0 o 1) | `\?` | `?` | `?` |
| `\|` (OR) | `\|` | `\|` | `\|` |
| `()` (grupo) | `\(\)` | `()` | `()` |
| `{n,m}` (repetición) | `\{n,m\}` | `{n,m}` | `{n,m}` |
| `\1` (backreference) | Sí (POSIX) | Solo GNU¹ | Sí |
| `\d`, `\w`, `\s` | No | Solo GNU¹ | Sí |
| `\b` (word boundary) | Solo GNU¹ | Solo GNU¹ | Sí |
| Lookahead/lookbehind | No | No | Sí |
| Non-greedy (`*?`, `+?`) | No | No | Sí |

¹ Extensión GNU, no portable a BSD grep (macOS) ni POSIX estricto.

```bash
# El mismo patrón en BRE y ERE:

# BRE: escapar metacaracteres
grep 'error\|warning' file                # | necesita \
grep 'a\{3\}' file                        # {} necesitan \
grep '\(foo\)\(bar\)' file                # () necesitan \
grep 'ab\?c' file                         # ? necesita \

# ERE: metacaracteres directos
grep -E 'error|warning' file              # | directo
grep -E 'a{3}' file                       # {} directos
grep -E '(foo)(bar)' file                 # () directos
grep -E 'ab?c' file                       # ? directo

# Recomendación: usar -E por defecto — más legible y menos propenso a errores
```

---

## Flags esenciales

### Flags de búsqueda

| Flag | Nombre | Efecto |
|---|---|---|
| `-i` | ignore-case | Case insensitive |
| `-w` | word | Palabra completa (equivale a `\bpattern\b`) |
| `-x` | line | Línea completa |
| `-v` | invert | Líneas que NO coinciden |
| `-F` | fixed | Búsqueda literal sin regex (más rápido) |

```bash
# -i — case insensitive:
grep -i 'error' file          # ERROR, Error, error, eRrOr...

# -w — word match (palabra completa):
grep -w 'log' file            # "log" pero no "login" ni "catalog"
# Equivale a: grep -P '\blog\b' o grep '\<log\>' (GNU BRE)

# -x — line match (línea completa):
grep -x 'exact line' file     # solo líneas que son exactamente "exact line"

# -v — invert match (líneas que NO coinciden):
grep -v '^#' config           # líneas que no son comentarios
grep -v '^$' file             # líneas no vacías

# -F — fixed strings (sin regex, literal):
grep -F '*.txt' file          # busca literal *.txt (no como regex)
grep -F '$HOME' file          # busca literal $HOME
# Más rápido que regex para búsquedas literales
```

### Flags de salida

| Flag | Nombre | Efecto |
|---|---|---|
| `-n` | number | Mostrar número de línea |
| `-c` | count | Contar líneas que coinciden |
| `-l` | files-with-matches | Listar archivos con match |
| `-L` | files-without-match | Listar archivos sin match |
| `-o` | only-matching | Solo la parte que coincide |
| `-m N` | max-count | Máximo N coincidencias |
| `-q` | quiet | Solo exit code, sin output |
| `-H` / `-h` | filename | Mostrar/ocultar nombre de archivo |

```bash
# -n — mostrar número de línea:
grep -n 'error' file
# 15:error en línea 15
# 42:otro error en línea 42

# -c — contar coincidencias (líneas que coinciden, NO total de matches):
grep -c 'error' file
# 7

# -l — listar archivos que contienen el patrón:
grep -rl 'TODO' src/
# src/main.c
# src/utils.c

# -L — listar archivos que NO contienen el patrón:
grep -rL 'TODO' src/

# -o — mostrar SOLO la parte que coincide (no la línea completa):
echo "error code: 404" | grep -oE '[0-9]+'
# 404

echo "ip: 192.168.1.1 and 10.0.0.1" | grep -oP '\d+\.\d+\.\d+\.\d+'
# 192.168.1.1
# 10.0.0.1

# -m N — máximo N coincidencias y parar:
grep -m 5 'error' huge.log    # solo las primeras 5

# -q — quiet (solo exit code, sin output):
if grep -q 'root' /etc/passwd; then
    echo "root existe"
fi
# Exit codes: 0 = encontrado, 1 = no encontrado, 2 = error
```

### Flags de contexto

```bash
# -A N — After (N líneas después del match):
grep -A 3 'error' log         # match + 3 líneas después

# -B N — Before (N líneas antes del match):
grep -B 2 'error' log         # 2 líneas antes + match

# -C N — Context (N líneas antes Y después):
grep -C 5 'segfault' kern.log # 5 antes + match + 5 después

# Las coincidencias se separan con -- entre grupos
```

### Búsqueda recursiva

```bash
# -r — recursivo (sigue symlinks solo si se nombran explícitamente):
grep -r 'pattern' /path/

# -R — recursivo (SIEMPRE sigue symlinks):
grep -R 'pattern' /path/

# --include — solo buscar en archivos que coinciden con un glob:
grep -r --include='*.py' 'import os' .

# NOTA: --include usa fnmatch globs, NO brace expansion.
# Esto NO funciona:
#   grep -r --include='*.{c,h}' 'malloc' src/    # ¡INCORRECTO!
# Usar múltiples --include:
grep -r --include='*.c' --include='*.h' 'malloc' src/

# --exclude — excluir archivos:
grep -r --exclude='*.min.js' 'function' .

# --exclude-dir — excluir directorios:
grep -r --exclude-dir='.git' --exclude-dir='node_modules' 'TODO' .

# -I — ignorar archivos binarios (equivale a --binary-files=without-match):
grep -rI 'pattern' /path/

# Combinación típica:
grep -rnI --include='*.sh' --exclude-dir='.git' 'set -e' /home/
```

---

## Expresiones regulares

### Metacaracteres (ERE)

| Metacarácter | Significado |
|---|---|
| `.` | Cualquier carácter (excepto newline) |
| `^` | Inicio de línea |
| `$` | Fin de línea |
| `*` | 0 o más del anterior |
| `+` | 1 o más del anterior |
| `?` | 0 o 1 del anterior |
| `{n}` | Exactamente n repeticiones |
| `{n,}` | n o más repeticiones |
| `{n,m}` | Entre n y m repeticiones |
| `[abc]` | Cualquier carácter del conjunto |
| `[^abc]` | Cualquier carácter NO del conjunto |
| `[a-z]` | Rango |
| `\|` | Alternación (OR) |
| `()` | Grupo (captura con `\1` solo en GNU) |
| `\` | Escape |

```bash
# Ejemplos:
grep -E '^[A-Z]' file                   # líneas que empiezan con mayúscula
grep -E '[0-9]{3}-[0-9]{4}' file         # teléfono NNN-NNNN
grep -E '^.{80,}$' file                  # líneas de 80+ caracteres
grep -E '(error|warning|critical)' file  # cualquiera de las tres
grep -E '^$' file                        # líneas vacías
grep -E '^[[:space:]]*$' file            # líneas vacías o solo whitespace
```

> **Nota**: `\s`, `\w`, `\b` con `-E` son extensiones GNU — no funcionan en
> BSD grep (macOS). Para portabilidad usar clases POSIX (`[[:space:]]`) o `-P`.

### Clases POSIX

| Clase | Equivalente | Significado |
|---|---|---|
| `[[:alnum:]]` | `[a-zA-Z0-9]` | Alfanumérico |
| `[[:alpha:]]` | `[a-zA-Z]` | Letras |
| `[[:digit:]]` | `[0-9]` | Dígitos |
| `[[:lower:]]` | `[a-z]` | Minúsculas |
| `[[:upper:]]` | `[A-Z]` | Mayúsculas |
| `[[:space:]]` | `[ \t\n\r\f\v]` | Whitespace |
| `[[:blank:]]` | `[ \t]` | Espacio y tab |
| `[[:punct:]]` | — | Puntuación |

```bash
# Son más portables que rangos como [a-z] que dependen del locale
grep -E '[[:digit:]]{4}-[[:digit:]]{2}-[[:digit:]]{2}' file   # fecha YYYY-MM-DD
```

### PCRE — Extensiones de Perl (`-P`)

```bash
# Shortcuts de clase:
# \d = dígito [0-9],     \D = no dígito
# \w = word [a-zA-Z0-9_], \W = no word
# \s = whitespace,         \S = no whitespace
# \b = word boundary

grep -P '\b\w+@\w+\.\w+\b' file               # email (simple)
grep -P '\bfunction\b' file                     # palabra "function" exacta

# Lookahead — verificar qué SIGUE sin consumirlo:
grep -P '(?<=price: )\d+' file     # dígitos después de "price: "
grep -P '\d+(?= USD)' file         # dígitos antes de " USD"

# Lookbehind — verificar qué PRECEDE sin consumirlo:
grep -P '(?<!un)known' file        # "known" no precedido por "un"

# Non-greedy (lazy) — matchear lo mínimo posible:
grep -oP '".*?"' file              # match mínimo entre comillas
# Sin ?: '"abc" "def"' matchea todo '"abc" "def"'
# Con ?: matchea '"abc"' y '"def"' por separado

# \K — descartar lo matcheado hasta este punto:
grep -oP 'Port: \K\d+' file       # solo el número, no "Port: "
# Similar a lookbehind pero sin restricción de longitud fija

# Named groups (solo informativo, grep no los aprovecha mucho):
grep -oP '(?P<year>\d{4})-(?P<month>\d{2})' file
```

#### Lookahead/lookbehind: cuidado con backtracking

Los lookahead negativos con cuantificadores codiciosos pueden no funcionar como
se espera:

```bash
# INCORRECTO — intenta filtrar unidades "px":
echo "100px 200em 300px" | grep -oP '\d+(?!px)\w+'
# Resultado: 100px, 200em, 300px (matchea TODO)
# ¿Por qué? \d+ matchea "10", (?!px) ve "0p" (no "px"), pasa.
# Luego \w+ captura "0px". El backtracking derrota al lookahead.

# CORRECTO — usar lookbehind negativo al final:
echo "100px 200em 300px" | grep -oP '\d+[a-z]+(?<!px)'
# Resultado: 200em (solo unidades que NO terminan en px)
```

---

## Múltiples patrones

```bash
# -E con | (OR):
grep -E '^(root|nobody):' /etc/passwd

# -e (múltiples expresiones — equivalente a OR):
grep -e '^root' -e '^nobody' /etc/passwd

# -f (patrones desde un archivo):
echo -e "root\nnobody\ndaemon" > /tmp/patterns.txt
grep -f /tmp/patterns.txt /etc/passwd

# AND (todas las condiciones) — no hay AND nativo en grep:
# Opción 1: pipeline
grep 'root' /etc/passwd | grep 'bash'

# Opción 2: PCRE lookahead (verifica ambos en cualquier orden)
grep -P '(?=.*root)(?=.*bash)' /etc/passwd
```

---

## Patrones comunes

```bash
# IP address (IPv4):
grep -oE '([0-9]{1,3}\.){3}[0-9]{1,3}' file
# Con PCRE y word boundary:
grep -oP '\b(?:\d{1,3}\.){3}\d{1,3}\b' file
# Nota: matchea IPs inválidas como 999.999.999.999 — validar octetos
# requiere lógica adicional (PCRE complejo o post-procesamiento)

# Email:
grep -oP '[\w.+-]+@[\w.-]+\.\w{2,}' file

# URL:
grep -oP 'https?://[^\s"<>]+' file

# Fecha YYYY-MM-DD:
grep -oE '[0-9]{4}-[0-9]{2}-[0-9]{2}' file

# Líneas entre dos marcadores:
sed -n '/START/,/END/p' file
# O con grep (aproximado, número fijo de líneas):
grep -A 1000 'START' file | grep -B 1000 'END'

# Líneas que contienen DOS patrones (en cualquier orden):
grep 'pattern1' file | grep 'pattern2'

# Líneas que NO contienen ninguno de varios patrones:
grep -Ev 'error|warning|debug' file
```

---

## grep con otros comandos

```bash
# grep en pipelines — el truco [n]ginx:
ps aux | grep '[n]ginx'
# Los corchetes evitan que grep se encuentre a sí mismo:
# [n]ginx matchea "nginx" pero el proceso grep aparece como "grep [n]ginx"
# que NO matchea el patrón [n]ginx
# Alternativa más clara: pgrep -a nginx

# grep + xargs (cuidado con nombres de archivo con espacios):
grep -rl 'deprecated' src/ | xargs sed -i 's/deprecated/updated/g'
# Más seguro con -Z/-z:
grep -rlZ 'deprecated' src/ | xargs -0 sed -i 's/deprecated/updated/g'

# grep + wc (contar matches):
grep -r 'TODO' src/ | wc -l
# O directamente: grep -rc 'TODO' src/

# grep en logs con timestamp:
grep -E '2024-03-17 1[0-5]:' access.log    # entre 10:00 y 15:59
```

---

## Rendimiento

```bash
# -F es significativamente más rápido para búsquedas literales:
grep -F 'exact string' huge.log    # no compila regex

# LC_ALL=C evita overhead de locale en archivos ASCII:
LC_ALL=C grep 'pattern' huge.log

# -m N detiene la búsqueda después de N matches:
grep -m 1 'pattern' huge.log       # sale al primer match

# Para archivos/proyectos grandes, ripgrep (rg) es más rápido:
# rg 'pattern' directory/
# - Multi-threaded
# - Respeta .gitignore
# - Ignora binarios automáticamente
# - Recursivo por defecto
```

> **Nota**: las fuentes originales mencionan `grep --mmap` para rendimiento.
> Esta opción fue deprecada en GNU grep 2.6 (2009) y se ignora silenciosamente
> en versiones modernas.

---

## Alternativas modernas

```bash
# ripgrep (rg):
rg 'pattern'              # recursivo por defecto
rg -t py 'import'         # solo archivos Python
rg -l 'TODO'              # solo nombres de archivo
rg --pcre2 'lookahead'    # soporte PCRE2

# ag (The Silver Searcher):
ag 'pattern'              # similar a rg

# Ambos ignoran .git, node_modules, binarios
# grep sigue siendo el estándar universal:
# - Disponible en TODOS los sistemas Unix
# - No requiere instalación adicional
# - Suficiente para la mayoría de tareas
```

---

## Labs

### Parte 1 — Modos y flags

#### Paso 1.1: BRE, ERE, PCRE

```bash
# BRE (default) — metacaracteres necesitan escape:
echo "abc123def" | grep "[0-9]\+"

# ERE (-E) — metacaracteres directos:
echo "abc123def" | grep -E "[0-9]+"

# PCRE (-P) — shortcuts de clase:
echo "abc123def" | grep -P "\d+"
```

#### Paso 1.2: Flags de output

```bash
# -o: solo la parte que matchea
echo "Mi IP es 192.168.1.100 y otra es 10.0.0.1" | \
    grep -oE "[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+"

# -w: solo palabras completas
echo "cat concatenate catfish" | grep -ow "cat"
# Solo matchea "cat" como palabra, no dentro de "concatenate"

# -c: contar líneas que matchean
grep -c "nologin" /etc/passwd

# -l / -L: listar archivos con/sin match
grep -l "root" /etc/passwd /etc/group /etc/shadow 2>/dev/null
grep -L "root" /etc/passwd /etc/hostname 2>/dev/null

# -m N: detenerse después de N matches
grep -m 3 ":" /etc/passwd

# -q: silencioso (solo exit code)
if grep -q "root" /etc/passwd; then
    echo "root encontrado (grep -q retornó 0)"
fi
```

#### Paso 1.3: Flags de matching

```bash
# -i: case insensitive
echo -e "Error\nERROR\nerror\nWarning" | grep -i "error"

# -v: invertir (líneas que NO matchean)
grep -v -E "nologin|false" /etc/passwd | cut -d: -f1,7 | head -5

# -x: match en TODA la línea
echo -e "hello\nhello world\nworld" | grep -x "hello"

# -F: string fijo (sin regex)
echo "buscar [error] literalmente" | grep -F "[error]"

# -n: números de línea
grep -n "root" /etc/passwd
```

### Parte 2 — Contexto y recursión

#### Paso 2.1: Contexto -A, -B, -C

```bash
cat > /tmp/logfile.txt << 'EOF'
2024-03-17 10:00:01 INFO Starting service
2024-03-17 10:00:02 INFO Loading config
2024-03-17 10:00:03 ERROR Failed to connect to database
2024-03-17 10:00:03 ERROR Connection refused on port 5432
2024-03-17 10:00:04 INFO Retrying in 5 seconds
2024-03-17 10:00:09 INFO Connection established
2024-03-17 10:00:10 WARNING Slow query detected
2024-03-17 10:00:11 INFO Processing complete
EOF

# -A N: N líneas después del match
grep -A 1 "ERROR" /tmp/logfile.txt

# -B N: N líneas antes del match
grep -B 1 "ERROR" /tmp/logfile.txt

# -C N: N líneas antes Y después
grep -C 1 "WARNING" /tmp/logfile.txt

# El separador -- indica límites entre grupos de matches
rm /tmp/logfile.txt
```

#### Paso 2.2: Búsqueda recursiva

```bash
# -r: buscar en todos los archivos recursivamente
grep -r "nameserver" /etc/ 2>/dev/null | head -5

# --include: solo en archivos que matchean un glob
grep -r --include="*.conf" "listen" /etc/ 2>/dev/null | head -5

# Combinación típica para desarrollo:
# grep -rnI --include="*.sh" --exclude-dir=.git "set -e" .
#   -r  recursivo
#   -n  números de línea
#   -I  ignorar binarios
```

#### Paso 2.3: Múltiples patrones

```bash
# -E con | (OR)
grep -E "^(root|nobody):" /etc/passwd

# -e (múltiples expresiones)
grep -e "^root" -e "^nobody" /etc/passwd

# AND con pipeline
grep "root" /etc/passwd | grep "bash"

# AND con PCRE lookahead
grep -P '(?=.*root)(?=.*bash)' /etc/passwd
```

### Parte 3 — PCRE y patrones comunes

#### Paso 3.1: PCRE — clases y boundaries

```bash
# \d: dígitos
echo "abc 123 def_456" | grep -oP '\d+'
# 123
# 456

# \w: word characters
echo "abc 123 def_456" | grep -oP '\w+'
# abc
# 123
# def_456

# \b: word boundary
echo "cat concatenate catfish" | grep -oP '\bcat\b'
# cat

# Non-greedy con ?
echo '<b>bold</b> and <i>italic</i>' | grep -oP '<.+>'
# <b>bold</b> and <i>italic</i>     (greedy: primer < hasta último >)

echo '<b>bold</b> and <i>italic</i>' | grep -oP '<.+?>'
# <b>
# </b>
# <i>
# </i>
# (non-greedy: bloque más corto posible)
```

#### Paso 3.2: Lookahead y lookbehind

```bash
# (?=...) Lookahead positivo
echo "Precio: 100 USD y 200 EUR" | grep -oP '\d+(?= USD)'
# 100   (números seguidos de " USD", sin incluir " USD")

# (?!...) Lookahead negativo
echo "Precio: 100 USD y 200 EUR" | grep -oP '\b\d+\b(?! USD)'
# 200   (números que NO están seguidos de " USD")
# Nota: \b\d+\b ancla el match a la palabra completa, evitando
# que el backtracking matchee parcialmente "10" de "100"

# (?<=...) Lookbehind positivo
echo 'Total: $100 y $200' | grep -oP '(?<=\$)\d+'
# 100
# 200   (números precedidos por $, sin incluir $)

# \K — alternativa a lookbehind sin restricción de longitud:
echo "Port: 8080" | grep -oP 'Port: \K\d+'
# 8080
```

#### Paso 3.3: Patrones comunes

```bash
cat > /tmp/test-data.txt << 'EOF'
user@example.com is valid
not-an-email is invalid
IP: 192.168.1.100
IP: 10.0.0.1
IP: 999.999.999.999
URL: https://example.com/path?q=1
URL: http://test.org
Port: 8080
Port: 99999
EOF

# Extraer emails
grep -oP '[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}' /tmp/test-data.txt

# Extraer IPs
grep -oP '\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b' /tmp/test-data.txt
# Nota: matchea 999.999.999.999 — validar octetos requiere lógica extra

# Extraer URLs
grep -oP 'https?://[^\s]+' /tmp/test-data.txt

# Extraer puertos (\K descarta "Port: " del match)
grep -oP 'Port: \K\d+' /tmp/test-data.txt

rm /tmp/test-data.txt
```

---

## Ejercicios

### Ejercicio 1 — BRE vs ERE

Escribe el mismo patrón que matchee líneas con `error` seguido de uno o más
dígitos, primero en BRE y luego en ERE.

```bash
# Archivo de prueba:
echo -e "error404\nerror\nwarning123\nerror99 found" > /tmp/modes.txt

# BRE:
grep 'error[0-9]\+' /tmp/modes.txt

# ERE:
grep -E 'error[0-9]+' /tmp/modes.txt

rm /tmp/modes.txt
```

<details><summary>Predicción</summary>

Ambos producen la misma salida — las líneas que contienen `error` seguido de
al menos un dígito:

```
error404
error99 found
```

La línea `error` sola no matchea (no tiene dígitos después). La línea
`warning123` no matchea (no empieza con `error`). En BRE `+` requiere `\+`;
en ERE `+` funciona directo.

</details>

### Ejercicio 2 — Flags de búsqueda: -w, -i, -v, -F

```bash
echo -e "cat\ncatalog\nCat\nthe cat sat\n*.txt files" > /tmp/flags.txt

# 1. Buscar "cat" como palabra completa (case sensitive):
grep -w 'cat' /tmp/flags.txt

# 2. Buscar "cat" como palabra completa, case insensitive:
grep -wi 'cat' /tmp/flags.txt

# 3. Líneas que NO contienen "cat" (sin -w, substring):
grep -v 'cat' /tmp/flags.txt

# 4. Buscar el literal "*.txt" (sin interpretar como regex):
grep -F '*.txt' /tmp/flags.txt

rm /tmp/flags.txt
```

<details><summary>Predicción</summary>

1. `-w 'cat'`: matchea `cat` y `the cat sat` (palabra completa "cat"). No
   matchea `catalog` (no es palabra completa) ni `Cat` (diferente case).
   ```
   cat
   the cat sat
   ```

2. `-wi 'cat'`: añade case insensitive → `cat`, `Cat`, `the cat sat`.
   `catalog` sigue sin matchear (no es palabra completa).
   ```
   cat
   Cat
   the cat sat
   ```

3. `-v 'cat'`: líneas que NO contienen el substring "cat" → `Cat`
   (mayúscula, `-v` es case sensitive) y `*.txt files`.
   ```
   Cat
   *.txt files
   ```

4. `-F '*.txt'`: busca la cadena literal `*.txt` → matchea la línea
   `*.txt files`. Sin `-F`, `*` sería un cuantificador regex (0 o más del
   carácter anterior).
   ```
   *.txt files
   ```

</details>

### Ejercicio 3 — Flags de salida: -o, -c, -l, -n

```bash
echo -e "error 404\nwarning 200\nerror 500\ninfo ok" > /tmp/out.txt

# 1. Extraer solo los números:
grep -oE '[0-9]+' /tmp/out.txt

# 2. Contar cuántas líneas contienen "error":
grep -c 'error' /tmp/out.txt

# 3. Mostrar las líneas con "error" con número de línea:
grep -n 'error' /tmp/out.txt

rm /tmp/out.txt
```

<details><summary>Predicción</summary>

1. `-oE '[0-9]+'`: extrae cada secuencia de dígitos como match independiente.
   Cada match se imprime en su propia línea:
   ```
   404
   200
   500
   ```
   La línea `info ok` no tiene dígitos, no produce output.

2. `-c 'error'`: cuenta **líneas** que contienen "error", no ocurrencias
   totales. Resultado: `2` (líneas 1 y 3).

3. `-n 'error'`: prefija el número de línea con `:`:
   ```
   1:error 404
   3:error 500
   ```

</details>

### Ejercicio 4 — Contexto: -A, -B, -C

```bash
cat > /tmp/ctx.log << 'EOF'
2024-01-01 INFO boot
2024-01-01 INFO loading
2024-01-01 ERROR disk full
2024-01-01 INFO retry
2024-01-01 INFO ok
2024-01-01 WARNING slow
2024-01-01 INFO done
EOF

# 1. Mostrar ERROR con 1 línea antes y 1 después:
grep -C 1 'ERROR' /tmp/ctx.log

# 2. Mostrar WARNING con 2 líneas antes pero ninguna después:
grep -B 2 'WARNING' /tmp/ctx.log

rm /tmp/ctx.log
```

<details><summary>Predicción</summary>

1. `-C 1 'ERROR'`: 1 línea antes, el match, 1 línea después:
   ```
   2024-01-01 INFO loading
   2024-01-01 ERROR disk full
   2024-01-01 INFO retry
   ```

2. `-B 2 'WARNING'`: 2 líneas antes del match y el match:
   ```
   2024-01-01 INFO retry
   2024-01-01 INFO ok
   2024-01-01 WARNING slow
   ```

</details>

### Ejercicio 5 — Búsqueda recursiva con --include

```bash
# Crear estructura de prueba:
mkdir -p /tmp/proj/src /tmp/proj/docs
echo 'TODO: fix bug' > /tmp/proj/src/main.c
echo 'TODO: add tests' > /tmp/proj/src/test.py
echo 'TODO: update docs' > /tmp/proj/docs/guide.md
echo 'all done' > /tmp/proj/src/utils.c

# 1. Buscar TODO recursivamente solo en archivos .c:
grep -rn --include='*.c' 'TODO' /tmp/proj/

# 2. Listar archivos que NO contienen TODO:
grep -rL 'TODO' /tmp/proj/

rm -rf /tmp/proj
```

<details><summary>Predicción</summary>

1. `--include='*.c'`: solo busca en archivos `.c`:
   ```
   /tmp/proj/src/main.c:1:TODO: fix bug
   ```
   `test.py` y `guide.md` se ignoran por el filtro.

2. `-rL 'TODO'`: lista archivos que NO contienen "TODO":
   ```
   /tmp/proj/src/utils.c
   ```
   Los otros tres archivos contienen "TODO".

</details>

### Ejercicio 6 — Clases POSIX y rangos

```bash
echo -e "abc123\nABC\n123\n!@#\nhello world\n2024-03-17" > /tmp/classes.txt

# 1. Líneas que contienen solo letras y/o dígitos (de inicio a fin):
grep -Ex '[[:alnum:]]+' /tmp/classes.txt

# 2. Líneas que empiezan con un dígito:
grep -E '^[[:digit:]]' /tmp/classes.txt

# 3. Extraer una fecha YYYY-MM-DD:
grep -oE '[[:digit:]]{4}-[[:digit:]]{2}-[[:digit:]]{2}' /tmp/classes.txt

rm /tmp/classes.txt
```

<details><summary>Predicción</summary>

1. `-Ex '[[:alnum:]]+'`: `-x` exige que TODA la línea matchee. Solo líneas
   compuestas enteramente de letras/dígitos:
   ```
   abc123
   ABC
   123
   ```
   `!@#` tiene puntuación. `hello world` tiene espacio. `2024-03-17` tiene `-`.

2. `^[[:digit:]]`: líneas que empiezan con un dígito:
   ```
   123
   2024-03-17
   ```

3. Extrae la fecha:
   ```
   2024-03-17
   ```

</details>

### Ejercicio 7 — PCRE: lookahead y lookbehind

```bash
echo -e 'price: 100 USD\nprice: 200 EUR\nprice: 50 USD\ntotal: 350' > /tmp/pcre.txt

# 1. Extraer números seguidos de " USD" (sin incluir " USD"):
grep -oP '\d+(?= USD)' /tmp/pcre.txt

# 2. Extraer números precedidos de "price: " (sin incluir "price: "):
grep -oP '(?<=price: )\d+' /tmp/pcre.txt

# 3. Usando \K (equivalente al lookbehind pero más flexible):
grep -oP 'total: \K\d+' /tmp/pcre.txt

rm /tmp/pcre.txt
```

<details><summary>Predicción</summary>

1. `\d+(?= USD)`: números que tienen ` USD` adelante, sin incluirlo:
   ```
   100
   50
   ```
   `200` está seguido de ` EUR`, no matchea. `350` no tiene ` USD`.

2. `(?<=price: )\d+`: números precedidos por `price: `:
   ```
   100
   200
   50
   ```
   `350` no está precedido por `price: ` sino por `total: `.

3. `total: \K\d+`: `\K` descarta `total: ` del resultado:
   ```
   350
   ```

</details>

### Ejercicio 8 — Non-greedy y el truco [n]ginx

```bash
# 1. Non-greedy: extraer cada tag HTML individualmente
echo '<div><span>text</span></div>' | grep -oP '<.+?>'

# 2. vs greedy:
echo '<div><span>text</span></div>' | grep -oP '<.+>'

# 3. El truco [p]attern para excluir grep de su propia salida:
# Predice la diferencia entre estas dos líneas:
ps aux | grep 'bash'
ps aux | grep '[b]ash'
```

<details><summary>Predicción</summary>

1. Non-greedy `<.+?>`: matchea el tag más corto posible:
   ```
   <div>
   <span>
   </span>
   </div>
   ```

2. Greedy `<.+>`: matchea desde el primer `<` hasta el último `>`:
   ```
   <div><span>text</span></div>
   ```
   Todo en una sola línea de output.

3. `grep 'bash'` muestra procesos con "bash" en la línea **incluyendo el
   propio proceso grep** (que aparece como `grep bash`). `grep '[b]ash'`
   produce el mismo match de "bash" pero el proceso grep aparece como
   `grep [b]ash` que NO contiene la cadena "bash" (contiene `[b]ash`), así
   que se excluye a sí mismo. Resultado: solo procesos bash reales.

</details>

### Ejercicio 9 — Grep + pipeline: frecuencia de errores

```bash
cat > /tmp/app.log << 'EOF'
2024-01-01 ERROR timeout
2024-01-01 ERROR disk full
2024-01-01 INFO ok
2024-01-01 ERROR timeout
2024-01-01 WARNING slow
2024-01-01 ERROR timeout
2024-01-01 ERROR disk full
2024-01-01 ERROR auth failed
EOF

# Extraer solo el mensaje de error y contar frecuencia:
grep 'ERROR' /tmp/app.log | sed 's/.*ERROR //' | sort | uniq -c | sort -rn

rm /tmp/app.log
```

<details><summary>Predicción</summary>

1. `grep 'ERROR'` filtra solo las 5 líneas con ERROR.
2. `sed 's/.*ERROR //'` elimina todo hasta e incluyendo "ERROR ", dejando solo
   el mensaje: `timeout`, `disk full`, `timeout`, `timeout`, `disk full`,
   `auth failed`.
3. `sort` agrupa iguales: `auth failed`, `disk full`, `disk full`, `timeout`,
   `timeout`, `timeout`.
4. `uniq -c` cuenta consecutivos.
5. `sort -rn` ordena por frecuencia descendente.

Resultado:
```
      3 timeout
      2 disk full
      1 auth failed
```

</details>

### Ejercicio 10 — Panorama: análisis de datos con grep

Un script que analiza un archivo CSV simulado usando múltiples técnicas de grep.

```bash
cat > /tmp/access.csv << 'EOF'
timestamp,ip,method,path,status,size
2024-03-17 10:00:01,192.168.1.10,GET,/index.html,200,1024
2024-03-17 10:00:02,10.0.0.5,POST,/api/login,200,512
2024-03-17 10:00:03,192.168.1.10,GET,/style.css,200,2048
2024-03-17 10:00:04,172.16.0.1,GET,/admin,403,128
2024-03-17 10:00:05,10.0.0.5,POST,/api/data,500,64
2024-03-17 10:00:06,192.168.1.10,GET,/index.html,200,1024
2024-03-17 10:00:07,10.0.0.5,GET,/api/status,200,256
2024-03-17 10:00:08,172.16.0.1,POST,/api/login,401,128
2024-03-17 10:00:09,192.168.1.20,GET,/index.html,200,1024
2024-03-17 10:00:10,10.0.0.5,DELETE,/api/data,500,32
EOF

echo "=== 1. Requests con errores (status 4xx o 5xx) ==="
grep -E ',[45][0-9]{2},[0-9]+$' /tmp/access.csv

echo ""
echo "=== 2. IPs únicas ==="
grep -oP '\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b' /tmp/access.csv | sort -u

echo ""
echo "=== 3. Requests por IP (frecuencia) ==="
grep -oP '\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b' /tmp/access.csv | \
    sort | uniq -c | sort -rn

echo ""
echo "=== 4. Solo métodos POST (excluyendo el header) ==="
grep -v '^timestamp' /tmp/access.csv | grep ',POST,'

echo ""
echo "=== 5. Paths de /api/ con status 500 ==="
grep -P ',/api/\w+,500,' /tmp/access.csv | grep -oP '/api/\w+'

rm /tmp/access.csv
```

<details><summary>Predicción</summary>

1. **Requests con errores**: matchea líneas donde el penúltimo campo (status)
   empieza con 4 o 5:
   ```
   2024-03-17 10:00:04,172.16.0.1,GET,/admin,403,128
   2024-03-17 10:00:05,10.0.0.5,POST,/api/data,500,64
   2024-03-17 10:00:08,172.16.0.1,POST,/api/login,401,128
   2024-03-17 10:00:10,10.0.0.5,DELETE,/api/data,500,32
   ```

2. **IPs únicas** (`sort -u` elimina duplicados):
   ```
   10.0.0.5
   172.16.0.1
   192.168.1.10
   192.168.1.20
   ```

3. **Requests por IP** (frecuencia descendente):
   ```
         4 10.0.0.5
         3 192.168.1.10
         2 172.16.0.1
         1 192.168.1.20
   ```

4. **Solo POST** (excluyendo header):
   ```
   2024-03-17 10:00:02,10.0.0.5,POST,/api/login,200,512
   2024-03-17 10:00:05,10.0.0.5,POST,/api/data,500,64
   2024-03-17 10:00:08,172.16.0.1,POST,/api/login,401,128
   ```

5. **Paths /api/ con status 500**: primero filtra las líneas con `/api/` y
   `500`, luego extrae solo el path:
   ```
   /api/data
   /api/data
   ```
   Dos resultados porque hay dos requests a `/api/data` con status 500 (POST y DELETE).

</details>
