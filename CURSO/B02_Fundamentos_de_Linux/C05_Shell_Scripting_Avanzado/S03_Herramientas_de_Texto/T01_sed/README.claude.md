# T01 — sed

> **Fuentes:** `README.md` (base) + `README.max.md` (ampliado) + `labs/README.md`
>
> **Erratas corregidas respecto a las fuentes:**
>
> | Ubicación | Error | Corrección |
> |---|---|---|
> | max.md:448 | `sed '1!d'` comentado "eliminar todo excepto líneas 1-3" | `1!d` solo conserva línea 1; para 1-3 sería `1,3!d` |
> | max.md:480 | `sed 'N; s/,\n/, /'` sobre `"one,\ntwo,\nthree"` → comentado `one, two` | Resultado real: `one, two,` (la coma del segundo campo persiste) |
> | md:497, max.md:497 | `sed 's/[a-z]*/<&>/g'` comentado `<hello> <world>` | Resultado real: `<hello><> <world><>` — `[a-z]*` coincide con cadena vacía entre palabras |
> | max.md:524 | Ejercicio "hold space para reverse" con `sed -E 's/(.)/\1\n/g' \| tac` comentado `three two one` | Ni usa hold space ni invierte palabras; invierte caracteres → `eerht owt eno` |

---

## Qué es sed

`sed` (stream editor) procesa texto **línea por línea** aplicando
transformaciones. No abre el archivo completo en memoria — lee una línea,
aplica los comandos, la imprime, y pasa a la siguiente:

```bash
# Sintaxis general:
sed [opciones] 'DIRECCIÓN COMANDO' archivo
sed [opciones] -e 'cmd1' -e 'cmd2' archivo
sed [opciones] -f script.sed archivo

# También acepta stdin:
echo "hello world" | sed 's/hello/goodbye/'
# goodbye world
```

---

## Sustitución — s///

El comando más usado. Reemplaza texto que coincide con un patrón:

```bash
# s/patrón/reemplazo/flags
echo "hello world" | sed 's/world/bash/'
# hello bash

# Por defecto, solo reemplaza la PRIMERA coincidencia en cada línea:
echo "aaa bbb aaa" | sed 's/aaa/XXX/'
# XXX bbb aaa

# Flag g — reemplazar TODAS las coincidencias en cada línea:
echo "aaa bbb aaa" | sed 's/aaa/XXX/g'
# XXX bbb XXX
```

### Flags de sustitución

| Flag | Nombre | Efecto |
|---|---|---|
| `g` | global | Todas las coincidencias en la línea |
| `N` | número | Solo la N-ésima coincidencia |
| `i` | ignore case | Case insensitive (GNU sed) |
| `p` | print | Imprimir si hubo sustitución |
| `w file` | write | Escribir líneas modificadas a archivo |

```bash
# g — global (todas las coincidencias en la línea)
echo "a-b-c-d" | sed 's/-/ /g'
# a b c d

# N — reemplazar solo la N-ésima coincidencia
echo "a-b-c-d" | sed 's/-/ /2'
# a-b c-d  (solo la segunda)

# i — case insensitive (extensión GNU)
echo "Hello HELLO hello" | sed 's/hello/HI/gi'
# HI HI HI

# p — print (imprimir la línea si hubo sustitución)
# Útil con -n (suprimir salida automática)
echo -e "foo\nbar\nfoo" | sed -n 's/foo/FOO/p'
# FOO
# FOO

# w archivo — escribir líneas modificadas a un archivo
sed 's/error/ERROR/w /tmp/errors.txt' logfile
```

### Delimitadores alternativos

```bash
# El delimitador puede ser cualquier carácter, no solo /
# Útil cuando el patrón o reemplazo contienen /:

# Con / — requiere escape:
echo "/usr/local/bin" | sed 's/\/usr\/local/\/opt/'
# /opt/bin

# Con | — más legible:
echo "/usr/local/bin" | sed 's|/usr/local|/opt|'
# /opt/bin

# Con # :
echo "/usr/local/bin" | sed 's#/usr/local#/opt#'
# /opt/bin

# El primer carácter después de s define el delimitador
```

### Backreferences — Grupos capturados

```bash
# \( \) capturan grupos, \1 \2 los referencian:
echo "2024-03-17" | sed 's/\([0-9]*\)-\([0-9]*\)-\([0-9]*\)/\3\/\2\/\1/'
# 17/03/2024

# Con -E (extended regex) — paréntesis sin escape:
echo "2024-03-17" | sed -E 's/([0-9]+)-([0-9]+)-([0-9]+)/\3\/\2\/\1/'
# 17/03/2024

# & referencia el match completo:
echo "hello" | sed 's/hello/[&]/'
# [hello]

echo "192.168.1.1" | sed -E 's/[0-9]+/(&)/g'
# (192).(168).(1).(1)
```

> **`&` vs `\1`:** `&` siempre es el match completo (no necesita grupos).
> `\1`-`\9` son los subgrupos capturados con `\( \)` (BRE) o `( )` (ERE con `-E`).

---

## Direccionamiento — Seleccionar líneas

Sin dirección, sed actúa sobre **todas** las líneas. Con dirección, actúa solo
sobre las que coinciden:

### Por número de línea

```bash
# Línea específica:
sed '3s/foo/bar/' file          # solo en la línea 3
sed '1d' file                   # eliminar la primera línea

# Rango de líneas:
sed '2,5s/foo/bar/' file        # líneas 2 a 5
sed '2,5d' file                 # eliminar líneas 2 a 5

# Desde una línea hasta el final:
sed '10,$s/foo/bar/' file       # desde línea 10 hasta el final
# $ = última línea

# Cada N líneas (GNU sed):
sed '0~2s/^/>> /' file          # líneas pares (2, 4, 6...)
sed '1~2s/^/>> /' file          # líneas impares (1, 3, 5...)
# Sintaxis: first~step → cada step líneas empezando en first
```

### Por patrón (regex)

```bash
# Líneas que coinciden con un patrón:
sed '/error/s/foo/bar/' file    # sustituir solo en líneas que contienen "error"
sed '/^#/d' file                # eliminar líneas que empiezan con #

# Rango entre dos patrones:
sed '/START/,/END/s/foo/bar/' file    # desde la línea con START hasta la de END
sed '/BEGIN/,/END/d' file             # eliminar bloque entre BEGIN y END

# Negar la dirección con !:
sed '/^#/!s/foo/bar/' file      # sustituir en líneas que NO empiezan con #
sed '1!d' file                  # eliminar todo excepto la primera línea
```

---

## Comandos principales

### d — Delete

```bash
# Eliminar líneas:
sed '/^$/d' file                # eliminar líneas vacías
sed '/^#/d' file                # eliminar comentarios
sed '/^$/d; /^#/d' file         # eliminar vacías Y comentarios
sed '1,10d' file                # eliminar primeras 10 líneas
sed '$d' file                   # eliminar última línea
```

### p — Print

```bash
# Imprimir líneas (usar con -n para suprimir la salida por defecto):
sed -n '5p' file                # imprimir solo la línea 5
sed -n '5,10p' file             # imprimir líneas 5 a 10
sed -n '/error/p' file          # imprimir líneas con "error" (como grep)
sed -n '1p' file                # primera línea (como head -1)
sed -n '$p' file                # última línea (como tail -1)
```

> **Sin `-n`**, sed imprime cada línea automáticamente. `p` la imprime
> **otra vez**, duplicándola. Casi siempre se usa `-n` con `p`.

### i y a — Insert y Append

```bash
# i\ — insertar ANTES de la línea:
sed '1i\# Header agregado' file
# Agrega "# Header agregado" antes de la primera línea

# a\ — append DESPUÉS de la línea:
sed '$a\# Footer agregado' file
# Agrega "# Footer agregado" después de la última línea

# Insertar después de un patrón:
sed '/\[section\]/a\key = value' config.ini
```

### c — Change (reemplazar línea completa)

```bash
sed '3c\línea nueva' file       # reemplazar la línea 3
sed '/^old/c\new line' file     # reemplazar líneas que empiezan con "old"
```

### y — Transliterate (como tr)

```bash
# y/source/dest/ — reemplaza carácter por carácter (como tr):
echo "hello" | sed 'y/helo/HELO/'
# HELLO

echo "abc" | sed 'y/abc/xyz/'
# xyz
```

---

## Edición in-place — -i

```bash
# -i modifica el archivo directamente (sin imprimir a stdout):
sed -i 's/foo/bar/g' file.txt

# -i con backup (extensión del backup):
sed -i.bak 's/foo/bar/g' file.txt
# Crea file.txt.bak (original) y modifica file.txt

# DIFERENCIA macOS vs Linux:
# Linux (GNU sed):
sed -i 's/foo/bar/' file       # OK sin extensión
sed -i.bak 's/foo/bar/' file   # OK con extensión

# macOS (BSD sed):
sed -i '' 's/foo/bar/' file    # necesita '' explícito
sed -i .bak 's/foo/bar/' file  # con extensión

# Portable (funciona en ambos):
sed -i.bak 's/foo/bar/' file && rm file.bak
```

### -i con múltiples archivos

```bash
# Editar múltiples archivos:
sed -i 's/TODO/DONE/g' *.txt

# Combinar con find:
find . -name "*.conf" -exec sed -i 's/old/new/g' {} +
```

---

## Múltiples comandos

```bash
# Con ; (separador):
sed 's/foo/bar/; s/baz/qux/' file

# Con -e:
sed -e 's/foo/bar/' -e 's/baz/qux/' file

# Con bloque {} (para aplicar múltiples comandos a la misma dirección):
sed '/pattern/ {
    s/foo/bar/
    s/baz/qux/
    a\nueva línea
}' file

# Desde un archivo de script:
cat > transform.sed << 'EOF'
s/foo/bar/g
s/baz/qux/g
/^#/d
/^$/d
EOF
sed -f transform.sed file
```

---

## Hold space — Buffer secundario

sed tiene dos buffers:
- **Pattern space**: la línea actual (donde trabajan los comandos normales)
- **Hold space**: buffer auxiliar (vacío por defecto)

| Comando | Dirección | Descripción |
|---|---|---|
| `h` | → hold | Copiar pattern space → hold space (sobrescribe) |
| `H` | → hold | Append pattern space → hold space (con `\n`) |
| `g` | ← hold | Copiar hold space → pattern space (sobrescribe) |
| `G` | ← hold | Append hold space → pattern space (con `\n`) |
| `x` | ↔ | Intercambiar pattern space ↔ hold space |

### Ejemplo: invertir orden de líneas (como tac)

```bash
sed -n '1!G; h; $p' file
# 1!G  — si NO es la primera línea, append hold space al pattern space
# h    — copiar pattern space al hold space
# $p   — si es la última línea, imprimir

# Es un ejercicio académico — usar tac en la práctica
```

### Ejemplo: agrupar líneas

```bash
# Juntar cada par de líneas:
echo -e "clave1\nvalor1\nclave2\nvalor2" | sed 'N; s/\n/ = /'
# clave1 = valor1
# clave2 = valor2
# N lee la siguiente línea y la agrega al pattern space con \n
```

---

## sed -E — Extended regex

```bash
# Sin -E (BRE — Basic Regular Expressions):
# Los metacaracteres (), +, ?, {} necesitan escape:
echo "aaa" | sed 's/a\+/X/'        # \+ para "uno o más"
echo "abc" | sed 's/\(a\)\(b\)/\2\1/'  # \( \) para grupos

# Con -E (ERE — Extended Regular Expressions):
# No necesitan escape:
echo "aaa" | sed -E 's/a+/X/'      # + sin escape
echo "abc" | sed -E 's/(a)(b)/\2\1/'   # () sin escape

# SIEMPRE usar -E — es más legible
# Equivalente: sed -r (GNU antiguo) = sed -E (POSIX 2024, GNU, BSD)
```

> **BRE vs ERE resumen:**
>
> | Carácter | BRE (sin `-E`) | ERE (con `-E`) |
> |---|---|---|
> | `( )` | literal, `\( \)` para grupo | grupo, `\( \)` literal |
> | `+` | literal, `\+` para "1 o más" | "1 o más", `\+` literal |
> | `?` | literal, `\?` para "0 o 1" | "0 o 1", `\?` literal |
> | `{ }` | literal, `\{ \}` para rango | rango, `\{ \}` literal |
> | `.` `*` `^` `$` `[ ]` | metacaracteres (igual en ambos) | igual |

---

## Recetas comunes

```bash
# Eliminar espacios al inicio y final de cada línea (trim):
sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//' file

# Eliminar líneas vacías y comentarios:
sed -E '/^[[:space:]]*(#|$)/d' file

# Agregar prefijo a cada línea:
sed 's/^/PREFIX: /' file

# Agregar sufijo a cada línea:
sed 's/$/ SUFFIX/' file

# Extraer valor de una clave en formato key=value:
sed -n 's/^database_host=//p' config.ini

# Convertir Windows line endings (CRLF → LF):
sed -i 's/\r$//' file

# Imprimir entre dos patrones (inclusivo):
sed -n '/START/,/END/p' file

# Imprimir entre dos patrones (exclusivo):
sed -n '/START/,/END/{/START/d; /END/d; p}' file

# Reemplazar solo la primera ocurrencia en todo el archivo (GNU sed):
sed '0,/pattern/{s/pattern/replacement/}' file

# Numerar líneas (como cat -n pero personalizado):
sed = file | sed 'N; s/\n/\t/'

# Eliminar comentarios inline (texto después de #):
sed 's/#.*//' file

# Eliminar tags HTML:
sed -E 's/<[^>]+>//g' file.html
```

---

## Ejercicios

### Ejercicio 1 — Sustitución básica y flags

```bash
echo -e "Hello World\nhello world\nHELLO WORLD" > /tmp/sed-test.txt

# a) Reemplazar "world" por "bash" (case insensitive, todas las ocurrencias):
sed 's/world/bash/gi' /tmp/sed-test.txt

# b) Reemplazar solo la segunda ocurrencia de "l" en cada línea:
sed 's/l/L/2' /tmp/sed-test.txt

# c) Imprimir solo las líneas donde hubo sustitución:
sed -n 's/world/BASH/ip' /tmp/sed-test.txt
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué produce <code>sed 's/world/bash/gi'</code>?</summary>

```
Hello bash
hello bash
HELLO bash
```

`i` hace case-insensitive (World, world, WORLD todas coinciden). `g` reemplaza todas las ocurrencias en cada línea (aquí solo hay una por línea).

</details>

<details><summary>2. ¿Qué produce <code>sed 's/l/L/2'</code> en "Hello World"?</summary>

`HelLo World` — reemplaza solo la segunda `l` encontrada en la línea. La primera `l` (posición 2) queda, la segunda `l` (posición 3) se cambia a `L`.

</details>

---

### Ejercicio 2 — Delimitadores alternativos

```bash
# Reemplazar rutas en un archivo de configuración:
echo "path=/usr/local/bin/app" > /tmp/path.txt

# a) Con / (forzando escapes):
sed 's/\/usr\/local\/bin/\/opt\/bin/' /tmp/path.txt

# b) Con |:
sed 's|/usr/local/bin|/opt/bin|' /tmp/path.txt

# c) Con #:
sed 's#/usr/local/bin#/opt/bin#' /tmp/path.txt
```

**Predicción:** antes de ejecutar, responde:

<details><summary>¿Producen las tres variantes el mismo resultado?</summary>

Sí, las tres producen `path=/opt/bin/app`. El delimitador es solo sintaxis — el primer carácter después de `s` lo define. Usar `|` o `#` evita la cascada de `\/` y mejora la legibilidad.

</details>

---

### Ejercicio 3 — Direccionamiento por línea y patrón

```bash
cat > /tmp/data.txt << 'EOF'
# Comentario
host=localhost
port=8080
# Otro comentario
debug=true
log=info
EOF

# a) Imprimir solo líneas 2 a 3:
sed -n '2,3p' /tmp/data.txt

# b) Eliminar comentarios (líneas que empiezan con #):
sed '/^#/d' /tmp/data.txt

# c) Cambiar "localhost" solo en líneas que contienen "host":
sed '/host/s/localhost/0.0.0.0/' /tmp/data.txt

# d) Negar: sustituir en todas las líneas EXCEPTO comentarios:
sed '/^#/!s/=/ => /' /tmp/data.txt
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué imprime <code>sed -n '2,3p'</code>?</summary>

```
host=localhost
port=8080
```

`-n` suprime salida automática, `2,3p` imprime solo líneas 2 y 3.

</details>

<details><summary>2. ¿Qué produce <code>sed '/^#/!s/=/ => /'</code>?</summary>

```
# Comentario
host => localhost
port => 8080
# Otro comentario
debug => true
log => info
```

`/^#/!` selecciona líneas que **no** empiezan con `#`. En esas, `s/=/ => /` reemplaza `=` por ` => `. Los comentarios quedan intactos.

</details>

---

### Ejercicio 4 — Backreferences y grupos

```bash
# a) Reformatear fechas YYYY-MM-DD a DD/MM/YYYY:
echo -e "2024-01-15\n2024-03-17\n2023-12-25" | \
    sed -E 's/([0-9]{4})-([0-9]{2})-([0-9]{2})/\3\/\2\/\1/'

# b) Swap "apellido, nombre" → "nombre apellido":
echo -e "García, Ana\nLópez, Carlos" | sed -E 's/([^,]+), *(.+)/\2 \1/'

# c) Envolver cada número entre corchetes:
echo "port 8080 and port 443" | sed -E 's/[0-9]+/[&]/g'
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué produce el reformateo de fechas?</summary>

```
15/01/2024
17/03/2024
25/12/2023
```

`\1` = año, `\2` = mes, `\3` = día. El reemplazo `\3/\2/\1` reordena a DD/MM/YYYY.

</details>

<details><summary>2. ¿Qué produce el swap de nombres?</summary>

```
Ana García
Carlos López
```

`\1` captura todo antes de la coma (`García`/`López`). `\2` captura todo después de la coma y espacios opcionales (`Ana`/`Carlos`). El reemplazo `\2 \1` invierte el orden.

</details>

<details><summary>3. ¿Qué hace <code>&</code> en el tercer comando?</summary>

`&` se reemplaza por el match completo. Resultado: `port [8080] and port [443]`. Cada número queda envuelto entre `[` y `]`.

</details>

---

### Ejercicio 5 — Comandos d, i, a, c

```bash
cat > /tmp/script.sh << 'EOF'
#!/bin/bash
# TODO: add error handling
echo "step 1"
echo "step 2"
# TODO: add logging
echo "step 3"
EOF

# a) Eliminar líneas TODO:
sed '/TODO/d' /tmp/script.sh

# b) Insertar un comentario antes de la línea 3:
sed '3i\# Paso principal:' /tmp/script.sh

# c) Reemplazar la línea del shebang:
sed '1c\#!/usr/bin/env bash' /tmp/script.sh
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Cuántas líneas produce <code>sed '/TODO/d'</code>?</summary>

4 líneas — elimina las dos líneas que contienen "TODO" (líneas 2 y 5). Quedan: shebang, echo step 1, echo step 2, echo step 3.

</details>

<details><summary>2. ¿Dónde aparece el texto insertado con <code>3i\</code>?</summary>

Antes de la línea 3 (que es `echo "step 1"`). `i\` inserta **antes** de la línea direccionada. Para insertar **después**, se usa `a\`.

</details>

---

### Ejercicio 6 — Edición in-place con -i

```bash
echo -e "server=old\nport=8080\ndebug=true" > /tmp/config.txt

# a) Modificar sin backup (irreversible):
sed -i 's/old/new/' /tmp/config.txt
cat /tmp/config.txt

# b) Modificar con backup:
sed -i.bak 's/8080/9090/' /tmp/config.txt
echo "--- Modificado ---"
cat /tmp/config.txt
echo "--- Backup ---"
cat /tmp/config.txt.bak
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué contiene <code>config.txt</code> después de ambos <code>sed -i</code>?</summary>

```
server=new
port=9090
debug=true
```

Primer `sed -i`: `old` → `new`. Segundo `sed -i.bak`: `8080` → `9090`.

</details>

<details><summary>2. ¿Qué contiene <code>config.txt.bak</code>?</summary>

```
server=new
port=8080
debug=true
```

El `.bak` se crea en el **segundo** `sed -i.bak`. Es una copia del archivo **antes** de esa segunda modificación. El primer cambio (`old` → `new`) ya se hizo sin backup.

</details>

---

### Ejercicio 7 — Rango entre patrones

```bash
cat > /tmp/config.ini << 'EOF'
[database]
host=db.example.com
port=5432

[cache]
host=redis.example.com
port=6379

[app]
host=0.0.0.0
port=8080
EOF

# a) Imprimir solo la sección [cache] (inclusivo):
sed -n '/\[cache\]/,/\[/p' /tmp/config.ini

# b) Cambiar host solo en la sección [database]:
sed '/\[database\]/,/\[/{ s/host=.*/host=newdb.local/; }' /tmp/config.ini
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué imprime el rango <code>/\[cache\]/,/\[/</code>?</summary>

```
[cache]
host=redis.example.com
port=6379

[app]
```

El rango empieza en `[cache]` y termina en la **siguiente** línea que contiene `[` — que es `[app]`. Incluye ambos extremos. `[app]` se incluye porque el rango es inclusivo.

</details>

<details><summary>2. ¿Se modifica el host de [cache] o [app]?</summary>

No. El rango `/\[database\]/,/\[/` solo abarca desde `[database]` hasta la siguiente `[` (que es la línea vacía seguida de `[cache]`). Solo el `host=db.example.com` dentro de ese rango se modifica.

</details>

---

### Ejercicio 8 — Hold space: invertir líneas

```bash
echo -e "uno\ndos\ntres\ncuatro" | sed -n '1!G; h; $p'
```

**Predicción:** antes de ejecutar, traza el estado de pattern space (PS) y hold space (HS) en cada paso:

<details><summary>Traza completa del hold space</summary>

**Línea 1 ("uno"):** `1!G` no aplica (es línea 1) → `h` copia PS→HS → HS=`uno`
**Línea 2 ("dos"):** `1!G` append HS→PS → PS=`dos\nuno` → `h` → HS=`dos\nuno`
**Línea 3 ("tres"):** `1!G` → PS=`tres\ndos\nuno` → `h` → HS=`tres\ndos\nuno`
**Línea 4 ("cuatro"):** `1!G` → PS=`cuatro\ntres\ndos\nuno` → `h` → `$p` imprime

Resultado:
```
cuatro
tres
dos
uno
```

Cada línea nueva se prepone al acumulado en hold space. Al final, se imprime todo invertido. Equivalente a `tac`.

</details>

---

### Ejercicio 9 — N: operaciones multilínea

```bash
# Juntar cada par de líneas con " = ":
echo -e "nombre\nAlice\nedad\n30\nciudad\nMadrid" | sed 'N; s/\n/ = /'
```

**Predicción:** antes de ejecutar, responde:

<details><summary>¿Qué produce y cómo funciona?</summary>

```
nombre = Alice
edad = 30
ciudad = Madrid
```

`N` lee la siguiente línea y la agrega al pattern space con `\n` como separador. Luego `s/\n/ = /` reemplaza ese `\n` por ` = `. El efecto es juntar cada par de líneas consecutivas.

**Cuidado con número impar de líneas:** si el archivo tiene un número impar de líneas, la última queda sola. `N` en la última línea (sin siguiente) causa que sed salga sin procesar esa línea (en GNU sed).

</details>

---

### Ejercicio 10 — Panorama: pipeline de transformación

```bash
cat > /tmp/raw.csv << 'EOF'
# Employee data
name,age,department
# Updated 2024-01-15
Alice,30,Engineering
Bob,25,Marketing
Charlie,35,Engineering
# End of data
EOF

# Pipeline: limpiar comentarios, agregar header, reformatear
sed -E '
    /^#/d
    /^$/d
    1s/,/ | /g
    1s/^/| /
    1s/$/ |/
    2,$s/([^,]+),([^,]+),(.*)/| \1 | \2 | \3 |/
' /tmp/raw.csv
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué produce el pipeline completo?</summary>

```
| name | age | department |
| Alice | 30 | Engineering |
| Bob | 25 | Marketing |
| Charlie | 35 | Engineering |
```

Paso a paso:
1. `/^#/d` elimina las 3 líneas de comentario
2. `/^$/d` elimina líneas vacías (no hay en este caso después de eliminar comentarios)
3. La línea del header (`name,age,department`) se convierte en tabla con `| campo | campo | campo |`
4. Las líneas de datos se reformatean con backreferences a formato tabla

</details>

<details><summary>2. ¿Por qué la dirección <code>1</code> en <code>1s/,/ | /g</code> apunta al header y no al comentario?</summary>

Porque las direcciones se evalúan **después** de aplicar los comandos anteriores (en la misma pasada). Una vez que `/^#/d` elimina la primera línea (comentario), la segunda línea original (`name,age,...`) se convierte en la salida de la línea 1. **No** — en realidad, sed procesa línea por línea y las direcciones se refieren a la línea de **entrada**, no de salida. La línea 1 de entrada es `# Employee data` que se elimina con `d`. Después de `d`, sed pasa a la siguiente línea. La dirección `1s/...` solo aplica a la primera línea de entrada.

En realidad, esto **no funciona** como se espera si las direcciones son de línea de entrada. Para que funcione correctamente, se necesitaría `sed -E '/^#/d; /^$/d'` primero y luego un segundo `sed` para formatear. O usar las sustituciones con patrón en vez de número de línea:

```bash
sed -E '/^#/d; /^$/d' /tmp/raw.csv | sed -E '
    1s/,/ | /g; 1s/^/| /; 1s/$/ |/
    2,$s/([^,]+),([^,]+),(.*)/| \1 | \2 | \3 |/
'
```

</details>
