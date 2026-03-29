# sed avanzado

## 1. Qué es sed

`sed` (stream editor) procesa texto **línea por línea** aplicando comandos
de transformación. No abre el archivo en un editor — lee de stdin (o archivo),
transforma, y escribe a stdout.

```
stdin/archivo → [pattern space] → comando → stdout
                     ↑
              una línea a la vez
```

Ya conoces el uso básico (`s/old/new/`). Este tópico cubre las capacidades
avanzadas: in-place editing, multiline, hold space, y recetas de producción.

---

## 2. Anatomía de un comando sed

```bash
sed [opciones] 'dirección comando' [archivo...]
```

```
sed -n '3,10s/foo/bar/gp' file.txt
     │  │    │    │    ││
     │  │    │    │    │└─ p = print (solo líneas modificadas)
     │  │    │    │    └── g = global (todas las ocurrencias en la línea)
     │  │    │    └─────── reemplazo
     │  │    └──────────── patrón a buscar
     │  └───────────────── dirección: líneas 3 a 10
     └──────────────────── -n = no imprimir por defecto (suppress)
```

### Direcciones (seleccionar líneas)

| Dirección | Selecciona |
|-----------|-----------|
| `5` | Solo línea 5 |
| `$` | Última línea |
| `3,7` | Líneas 3 a 7 |
| `3,$` | Desde línea 3 hasta el final |
| `/regex/` | Líneas que matchean regex |
| `/start/,/end/` | Desde línea con "start" hasta línea con "end" |
| `1~2` | Líneas impares (1, 3, 5, ...) GNU sed |
| `0~3` | Cada 3 líneas (3, 6, 9, ...) GNU sed |
| `addr!` | Negar: todas las líneas EXCEPTO addr |

### Comandos principales

| Comando | Efecto |
|---------|--------|
| `s/re/rep/` | Sustituir |
| `d` | Eliminar línea |
| `p` | Imprimir línea |
| `i\texto` | Insertar antes |
| `a\texto` | Append después |
| `c\texto` | Reemplazar línea completa |
| `y/abc/ABC/` | Transliterar (como `tr`) |
| `q` | Quit (salir) |
| `=` | Imprimir número de línea |

---

## 3. Sustitución: s/// en profundidad

### 3.1 Flags

```bash
# g = global (todas las ocurrencias en la línea, no solo la primera)
echo "aaa" | sed 's/a/b/'     # baa  (solo primera)
echo "aaa" | sed 's/a/b/g'    # bbb  (todas)

# N = N-ésima ocurrencia
echo "aaa" | sed 's/a/b/2'    # aba  (solo la segunda)

# i = case insensitive (GNU sed)
echo "Hello" | sed 's/hello/hi/i'  # hi

# p = print (imprimir si hubo sustitución)
sed -n 's/error/ERROR/p' log.txt   # solo imprime líneas modificadas

# w = write a archivo
sed 's/error/ERROR/w errors.txt' log.txt  # líneas modificadas a errors.txt
```

### 3.2 Delimitadores alternativos

Cuando el patrón contiene `/`, usar otro delimitador:

```bash
# Reemplazar paths (/ en el patrón — escape hell):
sed 's/\/usr\/local\/bin/\/opt\/bin/g'    # ilegible

# Mejor: usar otro delimitador
sed 's|/usr/local/bin|/opt/bin|g'         # legible
sed 's#/usr/local/bin#/opt/bin#g'         # también válido
sed 's@/usr/local/bin@/opt/bin@g'         # cualquier carácter
```

### 3.3 Grupos de captura y backreferences

```bash
# Capturar con \( \) y referenciar con \1, \2, ...
echo "John Smith" | sed 's/\(.*\) \(.*\)/\2, \1/'
# Smith, John

# & = todo el match
echo "hello" | sed 's/[a-z]*/(&)/'
# (hello)

# Reformatear fecha
echo "2026-03-28" | sed 's/\([0-9]*\)-\([0-9]*\)-\([0-9]*\)/\3\/\2\/\1/'
# 28/03/2026

# Extended regex (-E) para no escapar paréntesis:
echo "2026-03-28" | sed -E 's/([0-9]+)-([0-9]+)-([0-9]+)/\3\/\2\/\1/'
# 28/03/2026
```

**Preferir `-E`** (extended regex): evita escapar `(`, `)`, `+`, `?`, `{`, `}`.

---

## 4. In-place editing: -i

`-i` modifica el archivo directamente en vez de escribir a stdout:

```bash
# GNU sed: -i sin extensión (modifica en sitio)
sed -i 's/old/new/g' file.txt

# GNU sed: -i con extensión (crea backup)
sed -i.bak 's/old/new/g' file.txt
# file.txt = modificado
# file.txt.bak = original

# macOS sed: -i requiere extensión (string vacío para no backup)
sed -i '' 's/old/new/g' file.txt

# Portable (funciona en ambos):
sed -i.bak 's/old/new/g' file.txt && rm file.txt.bak
```

### Lo que -i realmente hace

```
Sin -i:
  archivo.txt → sed → stdout (archivo intacto)

Con -i:
  1. sed crea un archivo temporal
  2. Escribe el resultado al temporal
  3. Reemplaza el original con el temporal (mv)

  archivo.txt → sed → /tmp/sedXXXXXX → mv → archivo.txt
```

**Consecuencias**:
- El inode cambia (hardlinks se rompen)
- Los permisos se preservan (GNU sed los copia)
- Si sed falla a mitad, el archivo puede corromperse con `-i` sin backup

### Recetas comunes con -i

```bash
# Eliminar líneas en blanco
sed -i '/^$/d' file.txt

# Eliminar comentarios
sed -i '/^#/d' config.txt

# Eliminar trailing whitespace
sed -i 's/[[:space:]]*$//' file.txt

# Cambiar valor en config (key=value)
sed -i 's/^PORT=.*/PORT=8080/' .env

# Insertar línea después de match
sed -i '/\[section\]/a new_key=value' config.ini

# Eliminar línea que contiene patrón
sed -i '/DEPRECATED/d' code.py
```

---

## 5. Multiline: N, P, D

Por defecto, sed procesa una línea a la vez en el **pattern space**. Los
comandos N, P, D permiten trabajar con múltiples líneas:

| Comando | Efecto |
|---------|--------|
| `N` | Append next line al pattern space (separado por `\n`) |
| `P` | Print hasta el primer `\n` del pattern space |
| `D` | Delete hasta el primer `\n`, luego restart |

### 5.1 Juntar líneas

```bash
# Unir cada par de líneas
echo -e "name: Alice\nage: 30\nname: Bob\nage: 25" | sed 'N;s/\n/ | /'
# name: Alice | age: 30
# name: Bob | age: 25
```

```
Pattern space paso a paso:

Paso 1: lee "name: Alice"
  N → append → "name: Alice\nage: 30"
  s/\n/ | / → "name: Alice | age: 30"
  print → "name: Alice | age: 30"

Paso 2: lee "name: Bob"
  N → append → "name: Bob\nage: 25"
  s/\n/ | / → "name: Bob | age: 25"
  print → "name: Bob | age: 25"
```

### 5.2 Eliminar bloque multilínea

```bash
# Eliminar un bloque de configuración
# Desde "# START BLOCK" hasta "# END BLOCK"
sed '/# START BLOCK/,/# END BLOCK/d' config.txt

# Reemplazar bloque completo
sed '/# START BLOCK/,/# END BLOCK/c\# NEW CONFIG\nkey=value' config.txt
```

### 5.3 Unir líneas de continuación (backslash)

```bash
# Input:
# CFLAGS = -Wall \
#          -Wextra \
#          -O2

# Unir líneas que terminan en \
sed -E ':a; /\\$/N; s/\\\n\s*/ /; ta' Makefile
# CFLAGS = -Wall -Wextra -O2
```

```
Desglose:
:a           → etiqueta "a" (punto de salto)
/\\$/N       → si la línea termina en \, append la siguiente
s/\\\n\s*/ / → eliminar \ + newline + espacios, reemplazar con espacio
ta           → si la sustitución tuvo efecto, saltar a "a" (loop)
```

---

## 6. Hold space

sed tiene dos buffers:
- **Pattern space**: la línea actual (donde trabajan la mayoría de comandos)
- **Hold space**: un buffer auxiliar (vacío por defecto)

```
                ┌─────────────┐
 input ───────→ │ Pattern     │ ───────→ output
                │ space       │
                └──────┬──────┘
                       │ h/H/g/G/x
                ┌──────▼──────┐
                │ Hold        │
                │ space       │
                └─────────────┘
```

| Comando | Efecto |
|---------|--------|
| `h` | Copy pattern → hold (sobreescribe) |
| `H` | Append pattern → hold |
| `g` | Copy hold → pattern (sobreescribe) |
| `G` | Append hold → pattern |
| `x` | Exchange (swap pattern ↔ hold) |

### 6.1 Invertir líneas (tac)

```bash
echo -e "1\n2\n3" | sed -n '1!G;h;$p'
# 3
# 2
# 1
```

```
Línea 1: pattern="1"
  1!G  → no ejecutar G (es línea 1)
  h    → hold="1"
  $p   → no (no es última línea)

Línea 2: pattern="2"
  1!G  → G: pattern="2\n1" (append hold al pattern)
  h    → hold="2\n1"
  $p   → no

Línea 3: pattern="3"
  1!G  → G: pattern="3\n2\n1"
  h    → hold="3\n2\n1"
  $p   → sí: imprime "3\n2\n1"
```

### 6.2 Imprimir línea anterior al match

```bash
# Mostrar la línea ANTES de cada match
echo -e "aaa\nbbb\nccc\nbbb\nddd" | sed -n '/bbb/{x;p;d}; x'
# aaa
# ccc
```

Cada línea se guarda en hold space con `x`. Cuando se encuentra "bbb",
el hold space (que contiene la línea anterior) se imprime.

---

## 7. Branches y labels

sed tiene control de flujo básico con labels y saltos:

```bash
# :label  → definir label
# b label → branch (saltar) a label
# t label → branch si la última s/// fue exitosa
# T label → branch si la última s/// NO fue exitosa (GNU)
```

### Loop: repetir hasta que no haya más matches

```bash
# Eliminar espacios duplicados
echo "a   b    c" | sed -E ':loop; s/  / /; t loop'
# a b c
```

```
Paso 1: "a   b    c" → s/  / / → "a  b    c" (1 par reemplazado)
  t loop → sustitución exitosa → saltar a :loop
Paso 2: "a  b    c" → s/  / / → "a b    c"
  t loop → saltar
Paso 3: "a b    c" → s/  / / → "a b   c"
  ...continúa hasta que no quedan dobles espacios...
Paso N: "a b c" → s/  / / → no match
  t loop → no hubo sustitución → no saltar → print
```

### Condicional: ejecutar comando solo si match

```bash
# Si la línea contiene ERROR, añadir prefijo >>>
sed '/ERROR/{ s/^/>>> /; }' log.txt

# Si NO contiene #, wrap en comillas
sed '/^#/!{ s/.*/"&"/; }' config.txt
```

---

## 8. Recetas útiles de producción

### 8.1 Configuración (key=value)

```bash
# Cambiar valor existente
sed -i 's/^DATABASE_URL=.*/DATABASE_URL=postgres:\/\/prod/' .env

# Añadir si no existe, cambiar si existe
grep -q '^PORT=' .env \
    && sed -i 's/^PORT=.*/PORT=8080/' .env \
    || echo 'PORT=8080' >> .env

# Comentar una línea
sed -i 's/^dangerous_option/#&/' config.txt

# Descomentar
sed -i 's/^#\(dangerous_option\)/\1/' config.txt
```

### 8.2 Procesamiento de logs

```bash
# Extraer timestamps
sed -n 's/^\[\([0-9-]* [0-9:]*\)\].*/\1/p' app.log

# Anonimizar IPs
sed -E 's/[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+/X.X.X.X/g' access.log

# Extraer entre delimitadores
sed -n 's/.*token="\([^"]*\)".*/\1/p' response.xml

# Eliminar ANSI color codes
sed -E 's/\x1b\[[0-9;]*m//g' colored_output.txt
```

### 8.3 Código fuente

```bash
# Añadir header de licencia (insertar antes de línea 1)
sed -i '1i\// Copyright 2026 MyCompany. All rights reserved.' src/*.c

# Eliminar trailing whitespace en todo el proyecto
find . -name '*.py' -exec sed -i 's/[[:space:]]*$//' {} +

# Reemplazar import
sed -i 's/from old_module import/from new_module import/g' *.py

# Añadir línea después de match
sed -i '/^import os$/a import sys' script.py
```

### 8.4 Extraer secciones

```bash
# Todo entre dos marcadores (inclusive)
sed -n '/BEGIN_DATA/,/END_DATA/p' file.txt

# Sin los marcadores (exclusive)
sed -n '/BEGIN_DATA/,/END_DATA/{//!p}' file.txt

# Primeras N líneas (como head)
sed '10q' file.txt    # imprime líneas 1-10 y sale (más rápido que head para stdin)

# Línea específica
sed -n '42p' file.txt  # solo línea 42

# Rango
sed -n '10,20p' file.txt  # líneas 10-20
```

---

## 9. sed vs otras herramientas

| Tarea | sed | Alternativa | Cuándo preferir alternativa |
|-------|-----|-------------|---------------------------|
| Sustitución simple | `sed 's/a/b/'` | `${var//a/b}` | Variable en Bash, sin fork |
| Sustitución en archivo | `sed -i 's/a/b/' f` | — | sed es la herramienta correcta |
| Extraer campo | `sed 's/.*://'` | `cut -d: -f2` | Datos tabulares, cut es más claro |
| Filtrar líneas | `sed -n '/pat/p'` | `grep 'pat'` | grep es más rápido y claro |
| Transformación compleja | `sed` con hold space | `awk` | Lógica con estado, campos |
| JSON/YAML | No usar sed | `jq` / `yq` | Datos estructurados |
| Operaciones numéricas | No usar sed | `awk` | Cualquier cálculo |

**Regla**: si necesitas hold space, branches, o el script sed tiene más de
~3 comandos, probablemente `awk` es mejor opción.

---

## 10. Múltiples comandos

```bash
# Con -e (cada -e es un comando)
sed -e 's/foo/bar/' -e 's/baz/qux/' file.txt

# Con ; (separador)
sed 's/foo/bar/; s/baz/qux/' file.txt

# Con bloque multilinea (script file o heredoc)
sed -f script.sed file.txt

# heredoc:
sed '
    s/foo/bar/g
    s/baz/qux/g
    /^#/d
    /^$/d
' file.txt
```

### Script sed en archivo

```bash
# cleanup.sed
# Eliminar comentarios y líneas vacías
/^#/d
/^$/d
# Trim trailing whitespace
s/[[:space:]]*$//
# Normalizar separadores
s/[[:space:]]*=[[:space:]]*/=/
```

```bash
sed -f cleanup.sed config.txt
```

---

## 11. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| `sed 's/path/to/new/'` | `/` en el patrón rompe el delimitador | `sed 's\|path/to\|new\|'` |
| `sed -i` sin backup | Archivo corrupto si falla | `sed -i.bak` o verificar primero |
| `sed 's/\t/ /'` | `\t` no funciona en todos los sed | `sed 's/'$'\t''/ /'` o `sed -E` |
| Regex greedy no controlado | `s/".*/"/` matchea demasiado | Usar `[^"]*` para no-greedy |
| Hold space no inicializado | Contiene `\n` extra al inicio | Usar `h` antes de `G` |
| `-E` no portable | BSD/GNU difieren ligeramente | Testar en target, o usar escapes `\(` |
| `sed` en binarios | Corrompe archivos binarios | Solo usar en texto |
| `s/\n//` no funciona | `\n` no está en pattern space por defecto | Usar `N` primero, o `tr -d '\n'` |
| `sed -i ''` en GNU | Interpreta `''` como nombre de archivo | GNU: `sed -i`, macOS: `sed -i ''` |
| Multiline con `N` en última línea | `N` falla si no hay siguiente línea | Verificar con `$!N` |

---

## 12. Ejercicios

### Ejercicio 1 — Sustitución básica

**Predicción**: ¿qué imprime cada comando?

```bash
echo "foo bar foo baz foo" | sed 's/foo/FOO/'
echo "foo bar foo baz foo" | sed 's/foo/FOO/g'
echo "foo bar foo baz foo" | sed 's/foo/FOO/2'
```

<details>
<summary>Respuesta</summary>

```
FOO bar foo baz foo
FOO bar FOO baz FOO
foo bar FOO baz foo
```

- Sin flag: reemplaza solo la **primera** ocurrencia → `FOO bar foo baz foo`
- `g`: reemplaza **todas** → `FOO bar FOO baz FOO`
- `2`: reemplaza solo la **segunda** ocurrencia → `foo bar FOO baz foo`
</details>

---

### Ejercicio 2 — Direcciones

**Predicción**: ¿qué imprime?

```bash
echo -e "alpha\nbeta\ngamma\ndelta\nepsilon" | sed -n '2,4p'
echo -e "alpha\nbeta\ngamma\ndelta\nepsilon" | sed '3d'
echo -e "alpha\nbeta\ngamma\ndelta\nepsilon" | sed -n '/a/p'
```

<details>
<summary>Respuesta</summary>

```
beta
gamma
delta
```

```
alpha
beta
delta
epsilon
```

```
alpha
gamma
delta
```

- `'2,4p'` con `-n`: imprime solo líneas 2 a 4 (beta, gamma, delta)
- `'3d'`: elimina línea 3 (gamma), imprime el resto
- `'/a/p'` con `-n`: imprime líneas que contienen "a" (alpha, gamma, delta)
</details>

---

### Ejercicio 3 — Backreferences

**Predicción**: ¿qué imprime?

```bash
echo "2026-03-28" | sed -E 's/([0-9]{4})-([0-9]{2})-([0-9]{2})/\3.\2.\1/'
echo "John Smith" | sed -E 's/(\w+) (\w+)/\2_\1/'
echo "abc" | sed 's/./(&)/g'
```

<details>
<summary>Respuesta</summary>

```
28.03.2026
Smith_John
(a)(b)(c)
```

- Primera: tres grupos de captura → reordenados como `\3.\2.\1`
- Segunda: `\w+` matchea palabras → `\2_\1` invierte
- Tercera: `.` matchea cada carácter, `&` es todo el match → cada char entre paréntesis

Nota: `\w` (word character) funciona en GNU sed con `-E`. No es portable a
todos los sed.
</details>

---

### Ejercicio 4 — In-place

**Predicción**: después de ejecutar, ¿qué contiene file.txt?

```bash
echo -e "PORT=3000\nHOST=localhost\nDEBUG=true" > file.txt
sed -i 's/^PORT=.*/PORT=8080/' file.txt
sed -i '/^DEBUG/d' file.txt
```

<details>
<summary>Respuesta</summary>

```
PORT=8080
HOST=localhost
```

- Primera `-i`: reemplaza la línea que empieza con `PORT=` → `PORT=8080`
- Segunda `-i`: elimina la línea que empieza con `DEBUG`

`HOST=localhost` no fue afectado por ninguno de los dos comandos.
Cada `sed -i` lee el archivo, lo transforma, y lo reescribe.
</details>

---

### Ejercicio 5 — Extraer con -n y p

**Predicción**: ¿qué imprime?

```bash
echo -e 'url="https://example.com/api"\nname="test"\ntoken="abc123"' | \
    sed -n 's/.*token="\([^"]*\)".*/\1/p'
```

<details>
<summary>Respuesta</summary>

```
abc123
```

Desglose del regex:
- `.*token="` → matchea todo hasta `token="`
- `\([^"]*\)` → grupo de captura: todo hasta la siguiente comilla
- `".*` → matchea la comilla y el resto

Solo la línea con `token=` matchea. `-n` suprime output por defecto, `p`
imprime solo la línea donde la sustitución tuvo efecto. El resultado es
solo el contenido del grupo de captura: `abc123`.

Las otras dos líneas (url, name) no matchean `token=`, así que no se imprimen.
</details>

---

### Ejercicio 6 — Multiline con N

**Predicción**: ¿qué imprime?

```bash
echo -e "Name: Alice\nAge: 30\nName: Bob\nAge: 25" | sed 'N; s/\n/ — /'
```

<details>
<summary>Respuesta</summary>

```
Name: Alice — Age: 30
Name: Bob — Age: 25
```

`N` añade la siguiente línea al pattern space (separada por `\n`).
Luego `s/\n/ — /` reemplaza el newline con ` — `.

```
Paso 1: pattern = "Name: Alice"
  N → pattern = "Name: Alice\nAge: 30"
  s/\n/ — / → "Name: Alice — Age: 30"
  print

Paso 2: pattern = "Name: Bob"
  N → pattern = "Name: Bob\nAge: 25"
  s/\n/ — / → "Name: Bob — Age: 25"
  print
```
</details>

---

### Ejercicio 7 — Rango con regex

**Predicción**: ¿qué imprime?

```bash
cat <<'EOF' | sed -n '/START/,/END/p'
before
START
data 1
data 2
END
after
EOF
```

<details>
<summary>Respuesta</summary>

```
START
data 1
data 2
END
```

`/START/,/END/` selecciona desde la línea que contiene START hasta la que
contiene END, **inclusive**. Con `-n` y `p`, solo se imprimen esas líneas.

"before" y "after" están fuera del rango y no se imprimen.

Para excluir los marcadores (solo el contenido):
```bash
sed -n '/START/,/END/{//!p}'
# // repite el último regex usado, ! niega → "no imprimir las de los marcadores"
```
</details>

---

### Ejercicio 8 — Eliminar sección

**Predicción**: ¿qué queda después de esto?

```bash
cat <<'EOF' | sed '/# BEGIN GENERATED/,/# END GENERATED/d'
# config.conf
setting1=value1
# BEGIN GENERATED
auto_generated=true
timestamp=12345
# END GENERATED
setting2=value2
EOF
```

<details>
<summary>Respuesta</summary>

```
# config.conf
setting1=value1
setting2=value2
```

`/# BEGIN GENERATED/,/# END GENERATED/d` elimina todas las líneas desde
el marcador de inicio hasta el de fin, incluyendo los propios marcadores.

Esto es un patrón común para gestionar secciones auto-generadas en archivos
de configuración: regenerar eliminando la sección vieja y añadiendo la nueva.
</details>

---

### Ejercicio 9 — Loop con branch

**Predicción**: ¿qué imprime?

```bash
echo "a    b     c" | sed -E ':loop; s/  / /; t loop'
```

<details>
<summary>Respuesta</summary>

```
a b c
```

El loop reemplaza pares de espacios por un espacio, repitiendo hasta que
no quedan dobles espacios:

```
"a    b     c" → s/  / / → "a   b    c" (1 reemplazo) → t loop ✓
"a   b    c"   → s/  / / → "a  b   c"  → t loop ✓
"a  b   c"     → s/  / / → "a b  c"    → t loop ✓
"a b  c"       → s/  / / → "a b c"     → t loop ✓
"a b c"        → s/  / / → no match    → t loop ✗ → print
```

`t loop` solo salta si la sustitución anterior tuvo efecto. Cuando no hay
más dobles espacios, el loop termina.

Alternativa sin loop: `tr -s ' '` (squeeze spaces).
</details>

---

### Ejercicio 10 — Hold space

**Predicción**: ¿qué imprime?

```bash
echo -e "1\n2\n3" | sed -n '1!G; h; $p'
```

<details>
<summary>Respuesta</summary>

```
3
2
1
```

Es el patrón de `tac` (reversa de líneas):

```
Línea 1: pattern="1"
  1!G → no (es línea 1)
  h   → hold="1"
  $p  → no

Línea 2: pattern="2"
  1!G → G: pattern="2\n1" (append hold)
  h   → hold="2\n1"
  $p  → no

Línea 3: pattern="3"
  1!G → G: pattern="3\n2\n1"
  h   → hold="3\n2\n1"
  $p  → sí: imprime "3\n2\n1"
```

`G` appende el hold space al pattern space. Al acumular cada línea nueva
al frente y el hold al final, se invierte el orden. `$p` solo imprime
en la última línea, cuando todo está acumulado.
</details>