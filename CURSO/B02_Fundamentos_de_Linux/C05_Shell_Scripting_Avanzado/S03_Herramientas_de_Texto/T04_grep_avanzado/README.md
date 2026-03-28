# T04 — grep avanzado

## Modos de regex

grep soporta tres dialectos de expresiones regulares:

```bash
# BRE — Basic Regular Expressions (default):
grep 'pattern' file
# Los metacaracteres ?, +, {}, (), | necesitan escape con \

# ERE — Extended Regular Expressions (-E):
grep -E 'pattern' file
# equivalente: egrep 'pattern' file (legacy)
# ?, +, {}, (), | funcionan sin escape

# PCRE — Perl Compatible Regular Expressions (-P):
grep -P 'pattern' file
# Lookahead, lookbehind, \d, \w, non-greedy, etc.
# Solo disponible con GNU grep compilado con libpcre
# NO disponible en todos los sistemas (macOS no lo tiene)
```

### BRE vs ERE en la práctica

```bash
# El mismo patrón en los tres modos:

# BRE: escapar metacaracteres
grep 'error\|warning' file                # | necesita \
grep 'a\{3\}' file                        # {} necesitan \
grep '\(foo\)\(bar\)\1' file              # () necesitan \
grep 'ab\?c' file                         # ? necesita \

# ERE: metacaracteres directos
grep -E 'error|warning' file              # | directo
grep -E 'a{3}' file                       # {} directos
grep -E '(foo)(bar)\1' file               # () directos
grep -E 'ab?c' file                       # ? directo

# SIEMPRE usar -E — es más legible y menos propenso a errores
```

## Flags esenciales

### Flags de búsqueda

```bash
# -i — case insensitive:
grep -i 'error' file          # ERROR, Error, error, eRrOr...

# -w — word match (palabra completa):
grep -w 'log' file            # "log" pero no "login" ni "catalog"
# Equivale a: grep '\blog\b' con -P o grep '\<log\>' con BRE

# -x — line match (línea completa):
grep -x 'exact line' file     # solo líneas que son exactamente "exact line"

# -v — invert match (líneas que NO coinciden):
grep -v '^#' config           # líneas que no son comentarios
grep -v '^$' file             # líneas no vacías

# -F — fixed strings (sin regex, literal):
grep -F '*.txt' file          # busca literal *.txt (no como glob)
grep -F '$HOME' file          # busca literal $HOME
# Más rápido que regex para búsquedas literales
# equivalente legacy: fgrep
```

### Flags de salida

```bash
# -n — mostrar número de línea:
grep -n 'error' file
# 15:error en línea 15
# 42:otro error en línea 42

# -c — contar coincidencias (líneas que coinciden):
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
# -r — recursivo (sigue symlinks si se apuntan explícitamente):
grep -r 'pattern' /path/

# -R — recursivo (SIEMPRE sigue symlinks):
grep -R 'pattern' /path/

# --include — solo buscar en archivos que coinciden con un glob:
grep -r --include='*.py' 'import os' .
grep -r --include='*.{c,h}' 'malloc' src/

# --exclude — excluir archivos:
grep -r --exclude='*.min.js' 'function' .

# --exclude-dir — excluir directorios:
grep -r --exclude-dir='.git' --exclude-dir='node_modules' 'TODO' .

# Combinados:
grep -rn --include='*.sh' --exclude-dir='.git' 'set -e' /home/
```

## Expresiones regulares

### Metacaracteres básicos (ERE)

```bash
.        # cualquier carácter (excepto newline)
^        # inicio de línea
$        # fin de línea
*        # 0 o más del anterior
+        # 1 o más del anterior
?        # 0 o 1 del anterior
{n}      # exactamente n repeticiones
{n,}     # n o más repeticiones
{n,m}    # entre n y m repeticiones
[abc]    # cualquier carácter del conjunto
[^abc]   # cualquier carácter NO del conjunto
[a-z]    # rango
|        # alternación (OR)
()       # grupo de captura
\        # escape
```

```bash
# Ejemplos:
grep -E '^[A-Z]' file                # líneas que empiezan con mayúscula
grep -E '[0-9]{3}-[0-9]{4}' file     # teléfono NNN-NNNN
grep -E '^.{80,}$' file              # líneas de 80+ caracteres
grep -E '(error|warning|critical)' file  # cualquiera de las tres
grep -E '^$' file                     # líneas vacías
grep -E '^\s*$' file                  # líneas vacías o solo whitespace
```

### Clases POSIX

```bash
[[:alnum:]]    # alfanumérico [a-zA-Z0-9]
[[:alpha:]]    # letras [a-zA-Z]
[[:digit:]]    # dígitos [0-9]
[[:lower:]]    # minúsculas [a-z]
[[:upper:]]    # mayúsculas [A-Z]
[[:space:]]    # espacio, tab, newline
[[:blank:]]    # espacio y tab
[[:punct:]]    # puntuación

# Son más portables que rangos como [a-z] que dependen del locale
grep -E '[[:digit:]]{4}-[[:digit:]]{2}-[[:digit:]]{2}' file   # fecha YYYY-MM-DD
```

### PCRE — Extensiones de Perl (-P)

```bash
# \d = dígito, \D = no dígito
# \w = word char [a-zA-Z0-9_], \W = no word
# \s = whitespace, \S = no whitespace
# \b = word boundary

grep -P '\d{3}\.\d{3}\.\d{3}\.\d{3}' file    # IP (aproximada)
grep -P '\b\w+@\w+\.\w+\b' file               # email (simple)
grep -P '\bfunction\b' file                    # palabra "function" exacta

# Lookahead y lookbehind:
grep -P '(?<=price: )\d+' file     # dígitos después de "price: "
grep -P '\d+(?= USD)' file         # dígitos antes de " USD"
grep -P '(?<!un)known' file        # "known" no precedido por "un"

# Non-greedy (lazy):
grep -oP '".*?"' file              # match mínimo entre comillas
# Sin ?: '"abc" "def"' matchea todo '"abc" "def"'
# Con ?: '"abc" "def"' matchea '"abc"' y '"def"' por separado

# Named groups:
grep -oP '(?P<year>\d{4})-(?P<month>\d{2})' file
```

## Patrones comunes

```bash
# IP address (IPv4):
grep -oE '([0-9]{1,3}\.){3}[0-9]{1,3}' file
# Más preciso con -P:
grep -oP '\b(?:\d{1,3}\.){3}\d{1,3}\b' file

# Email:
grep -oP '[\w.+-]+@[\w.-]+\.\w{2,}' file

# URL:
grep -oP 'https?://[^\s"<>]+' file

# Fecha YYYY-MM-DD:
grep -oE '[0-9]{4}-[0-9]{2}-[0-9]{2}' file

# Líneas entre dos marcadores:
grep -A 1000 'START' file | grep -B 1000 'END'
# O con sed: sed -n '/START/,/END/p' file

# Líneas que contienen DOS patrones (en cualquier orden):
grep -E 'pattern1.*pattern2|pattern2.*pattern1' file
# O con dos greps:
grep 'pattern1' file | grep 'pattern2'

# Líneas que NO contienen ninguno de varios patrones:
grep -Ev 'error|warning|debug' file
```

## grep con otros comandos

```bash
# grep en pipelines:
ps aux | grep '[n]ginx'
# Los corchetes evitan que grep se encuentre a sí mismo:
# [n]ginx matchea "nginx" pero el proceso grep aparece como "grep [n]ginx"
# que NO matchea [n]ginx

# Alternativa más clara:
pgrep -a nginx

# grep + xargs:
grep -rl 'deprecated' src/ | xargs sed -i 's/deprecated/updated/g'

# grep + wc:
grep -r 'TODO' src/ | wc -l

# grep en logs con timestamp:
grep -E '2024-03-17 1[0-5]:' access.log    # entre 10:00 y 15:59
```

## Rendimiento

```bash
# -F es significativamente más rápido para búsquedas literales:
grep -F 'exact string' huge.log    # no compila regex

# LC_ALL=C evita overhead de locale en archivos ASCII:
LC_ALL=C grep 'pattern' huge.log

# --mmap puede mejorar rendimiento en archivos grandes (GNU grep):
grep --mmap 'pattern' huge.log

# Para archivos muy grandes, ripgrep (rg) es más rápido que GNU grep:
# rg 'pattern' directory/
# rg es multi-threaded, respeta .gitignore, y tiene output más legible
```

## Alternativas modernas

```bash
# ripgrep (rg) — más rápido, respeta .gitignore:
# rg 'pattern'              # recursivo por defecto
# rg -t py 'import'         # solo archivos Python
# rg -l 'TODO'              # solo nombres de archivo
# rg --pcre2 'lookahead'    # soporte PCRE2

# ag (The Silver Searcher):
# ag 'pattern'              # similar a rg

# Ambos son significativamente más rápidos que grep -r en proyectos grandes
# porque ignoran .git, node_modules, binarios, etc.

# Sin embargo, grep sigue siendo la herramienta estándar:
# - Disponible en TODOS los sistemas Unix
# - No requiere instalación
# - Suficiente para la mayoría de tareas
```

---

## Ejercicios

### Ejercicio 1 — Flags de búsqueda

```bash
# 1. Contar líneas con "root" (case insensitive) en /etc/passwd:
grep -ic 'root' /etc/passwd

# 2. Mostrar líneas con "bash" como palabra completa (no "rbash"):
grep -w 'bash' /etc/passwd

# 3. Listar archivos .conf en /etc que contienen "Listen":
grep -rl --include='*.conf' 'Listen' /etc/ 2>/dev/null
```

### Ejercicio 2 — Expresiones regulares

```bash
# 1. Extraer todas las IPs de un texto:
echo "Server 192.168.1.1 responded, backup at 10.0.0.5" | \
    grep -oE '([0-9]{1,3}\.){3}[0-9]{1,3}'
# 192.168.1.1
# 10.0.0.5

# 2. Encontrar líneas que empiezan con un número de 4 dígitos:
grep -E '^[0-9]{4}\b' file

# 3. Líneas que contienen "error" O "fail" (case insensitive):
grep -Ei 'error|fail' file
```

### Ejercicio 3 — grep en pipelines

```bash
# Encontrar los 5 procesos que más memoria usan, excluyendo headers:
ps aux --no-headers | sort -k4 -rn | head -5 | grep -oE '[^ ]+$'
# Muestra solo el nombre del comando (último campo)

# Contar cuántos archivos .sh hay recursivamente en /etc:
grep -rl --include='*.sh' '' /etc/ 2>/dev/null | wc -l
# O más eficiente:
find /etc -name '*.sh' 2>/dev/null | wc -l
```
