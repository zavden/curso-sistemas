# T01 — sed

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

# Cada N líneas:
sed '0~2s/^/>> /' file          # cada 2 líneas empezando por la 0 (pares)
sed '1~2s/^/>> /' file          # cada 2 líneas empezando por la 1 (impares)
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

## Hold space — Buffer secundario

sed tiene dos buffers:
- **Pattern space**: la línea actual (donde trabajan los comandos normales)
- **Hold space**: buffer auxiliar (vacío por defecto)

```bash
# h — copiar pattern space → hold space
# H — append pattern space → hold space (con \n)
# g — copiar hold space → pattern space
# G — append hold space → pattern space (con \n)
# x — intercambiar pattern space ↔ hold space
```

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
# Equivalente: sed -r (GNU) = sed -E (POSIX 2024, GNU, BSD)
```

## Recetas comunes

```bash
# Eliminar espacios al inicio y final de cada línea:
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

# Reemplazar solo la primera ocurrencia en todo el archivo:
sed '0,/pattern/{s/pattern/replacement/}' file    # GNU sed

# Numerar líneas (como cat -n pero personalizado):
sed = file | sed 'N; s/\n/\t/'
```

---

## Ejercicios

### Ejercicio 1 — Sustituciones básicas

```bash
echo -e "Hello World\nhello world\nHELLO WORLD" > /tmp/sed-test.txt

# 1. Reemplazar "world" por "bash" (case insensitive, todas las líneas):
sed -i 's/world/bash/gi' /tmp/sed-test.txt

# 2. Agregar ">> " al inicio de cada línea:
sed 's/^/>> /' /tmp/sed-test.txt

# 3. Reemplazar la segunda línea completa:
sed '2c\NUEVA LÍNEA' /tmp/sed-test.txt

rm /tmp/sed-test.txt
```

### Ejercicio 2 — Direccionamiento

```bash
# Del archivo /etc/passwd:
# 1. Imprimir solo las líneas 1 a 5:
sed -n '1,5p' /etc/passwd

# 2. Eliminar líneas que contienen "nologin":
sed '/nologin/d' /etc/passwd

# 3. Mostrar solo las líneas con /bin/bash:
sed -n '/\/bin\/bash/p' /etc/passwd
# O con delimitador alternativo:
sed -n '\|/bin/bash|p' /etc/passwd
```

### Ejercicio 3 — Backreferences

```bash
# Reformatear fechas de YYYY-MM-DD a DD/MM/YYYY:
echo -e "2024-01-15\n2024-03-17\n2023-12-25" | \
    sed -E 's/([0-9]{4})-([0-9]{2})-([0-9]{2})/\3\/\2\/\1/'
# 15/01/2024
# 17/03/2024
# 25/12/2023
```
