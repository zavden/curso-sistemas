# Lab — sed

## Objetivo

Dominar sed (stream editor): sustitucion con s/// y sus flags
(g, i, p, N), delimitadores alternativos, direcciones (lineas,
regex, rangos, !), comandos (d, p, i, a, c, y), edicion in-place
con -i, backreferences (\1, &, -E), hold space (h/H/g/G/x), y
recetas practicas.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Sustitucion y flags

### Objetivo

Usar s/// con sus flags para reemplazar texto de forma precisa.

### Paso 1.1: s/// basico

```bash
docker compose exec debian-dev bash -c '
echo "=== sed s/// ==="
echo ""
echo "--- Sustitucion basica ---"
echo "Hello World" | sed "s/World/Bash/"
echo ""
echo "--- Solo reemplaza la PRIMERA ocurrencia por linea ---"
echo "one two one two one" | sed "s/one/ONE/"
echo ""
echo "--- Flag g: todas las ocurrencias ---"
echo "one two one two one" | sed "s/one/ONE/g"
echo ""
echo "--- Flag i: case insensitive ---"
echo "Hello HELLO hello" | sed "s/hello/HOLA/gi"
echo ""
echo "--- Flag p: imprimir la linea modificada ---"
echo -e "error: fallo\ninfo: ok\nerror: otro" | sed -n "s/error/ERROR/p"
echo "(con -n, solo imprime lineas que matchean)"
echo ""
echo "--- Flag N: reemplazar la N-esima ocurrencia ---"
echo "a-b-c-d-e" | sed "s/-/=/2"
echo "(solo reemplaza el segundo guion)"
'
```

### Paso 1.2: Delimitadores alternativos

```bash
docker compose exec debian-dev bash -c '
echo "=== Delimitadores alternativos ==="
echo ""
echo "--- El problema con / en rutas ---"
echo "/home/user/docs" | sed "s/\/home\/user/\/opt/g"
echo "(muchos escapes — dificil de leer)"
echo ""
echo "--- Solucion: usar otro delimitador ---"
echo "/home/user/docs" | sed "s|/home/user|/opt|g"
echo "/home/user/docs" | sed "s#/home/user#/opt#g"
echo "(cualquier caracter sirve como delimitador)"
echo ""
echo "--- Regla practica ---"
echo "  Texto normal: s/old/new/"
echo "  Rutas:        s|old/path|new/path|"
echo "  URLs:         s#http://old#https://new#"
'
```

### Paso 1.3: Multiples sustituciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Multiples sustituciones ==="
echo ""
echo "--- Con -e (multiples expresiones) ---"
echo "Hello World 2024" | sed -e "s/Hello/Hola/" -e "s/World/Mundo/"
echo ""
echo "--- Con ; (separador) ---"
echo "Hello World 2024" | sed "s/Hello/Hola/; s/World/Mundo/"
echo ""
echo "--- Aplicar a un archivo ---"
cat > /tmp/sed-test.txt << '\''EOF'\''
server_host = localhost
server_port = 8080
debug = true
log_level = info
EOF
echo "Original:"
cat /tmp/sed-test.txt
echo ""
echo "Modificado:"
sed -e "s/localhost/0.0.0.0/" -e "s/8080/9090/" -e "s/true/false/" /tmp/sed-test.txt
echo ""
echo "(el archivo original no cambia — sed imprime a stdout)"
rm /tmp/sed-test.txt
'
```

---

## Parte 2 — Direcciones y comandos

### Objetivo

Aplicar comandos sed a lineas especificas usando direcciones.

### Paso 2.1: Direcciones de linea

```bash
docker compose exec debian-dev bash -c '
echo "=== Direcciones de linea ==="
echo ""
cat > /tmp/lines.txt << '\''EOF'\''
linea 1: primera
linea 2: segunda
linea 3: tercera
linea 4: cuarta
linea 5: quinta
EOF
echo "--- Linea especifica ---"
sed -n "3p" /tmp/lines.txt
echo "(3p: imprimir linea 3)"
echo ""
echo "--- Rango de lineas ---"
sed -n "2,4p" /tmp/lines.txt
echo "(2,4p: imprimir lineas 2 a 4)"
echo ""
echo "--- Desde linea N hasta el final ---"
sed -n "3,\$p" /tmp/lines.txt
echo "(3,\$p: desde linea 3 hasta el final)"
echo ""
echo "--- Cada N lineas ---"
sed -n "1~2p" /tmp/lines.txt
echo "(1~2p: desde la 1, cada 2 → impares)"
echo ""
echo "--- Ultima linea ---"
sed -n "\$p" /tmp/lines.txt
rm /tmp/lines.txt
'
```

### Paso 2.2: Direcciones regex

```bash
docker compose exec debian-dev bash -c '
echo "=== Direcciones regex ==="
echo ""
cat > /tmp/config.txt << '\''EOF'\''
# Database config
db_host = localhost
db_port = 5432
# Cache config
cache_host = localhost
cache_port = 6379
# App config
app_port = 8080
EOF
echo "--- Solo lineas que matchean un patron ---"
sed -n "/cache/p" /tmp/config.txt
echo ""
echo "--- Rango entre dos patrones ---"
sed -n "/Database/,/Cache/p" /tmp/config.txt
echo "(desde Database hasta Cache, inclusive)"
echo ""
echo "--- Negacion con ! ---"
echo "Lineas que NO son comentarios:"
sed -n "/^#/!p" /tmp/config.txt
echo ""
echo "--- Sustituir solo en lineas que matchean ---"
echo "Cambiar localhost solo en lineas de cache:"
sed "/cache/s/localhost/redis-server/" /tmp/config.txt
rm /tmp/config.txt
'
```

### Paso 2.3: Comandos d, i, a, c

```bash
docker compose exec debian-dev bash -c '
echo "=== Comandos sed ==="
echo ""
cat > /tmp/data.txt << '\''EOF'\''
header line
data line 1
data line 2
empty:
data line 3
footer line
EOF
echo "Original:"
cat /tmp/data.txt
echo ""
echo "--- d: eliminar lineas ---"
echo "Sin comentarios ni lineas vacias:"
echo -e "# comment\ndata\n\n# otro\nmore data" | sed "/^#/d; /^$/d"
echo ""
echo "--- i: insertar ANTES ---"
sed "3i\\INSERTADO ANTES DE LINEA 3" /tmp/data.txt | head -4
echo ""
echo "--- a: agregar DESPUES ---"
sed "2a\\AGREGADO DESPUES DE LINEA 2" /tmp/data.txt | head -4
echo ""
echo "--- c: reemplazar linea completa ---"
sed "/empty/c\\replaced: no longer empty" /tmp/data.txt
echo ""
echo "--- y: transliterar (como tr) ---"
echo "Hello World" | sed "y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/"
rm /tmp/data.txt
'
```

### Paso 2.4: -i — Edicion in-place

```bash
docker compose exec debian-dev bash -c '
echo "=== sed -i (in-place) ==="
echo ""
echo "server=old" > /tmp/inplace.txt
echo "port=8080" >> /tmp/inplace.txt
echo ""
echo "--- Antes ---"
cat /tmp/inplace.txt
echo ""
echo "--- sed -i (modifica el archivo directamente) ---"
sed -i "s/old/new/" /tmp/inplace.txt
echo "--- Despues ---"
cat /tmp/inplace.txt
echo ""
echo "--- sed -i.bak (crea backup) ---"
sed -i.bak "s/8080/9090/" /tmp/inplace.txt
echo "Archivo modificado:"
cat /tmp/inplace.txt
echo "Backup:"
cat /tmp/inplace.txt.bak
echo ""
echo "CUIDADO: -i sin .bak es irreversible"
echo "En macOS: sed -i '\'''\'' (string vacio obligatorio)"
echo "En Linux: sed -i (sin argumento)"
rm /tmp/inplace.txt /tmp/inplace.txt.bak
'
```

---

## Parte 3 — Backreferences y hold space

### Objetivo

Usar backreferences para capturas avanzadas y el hold space
para operaciones multilinea.

### Paso 3.1: Backreferences

```bash
docker compose exec debian-dev bash -c '
echo "=== Backreferences ==="
echo ""
echo "--- & (todo el match) ---"
echo "hello world" | sed "s/[a-z]*/(&)/g"
echo "(& se reemplaza por lo que matcheo)"
echo ""
echo "--- \\1, \\2 (grupos capturados) ---"
echo "2024-03-17" | sed "s/\([0-9]*\)-\([0-9]*\)-\([0-9]*\)/\3\/\2\/\1/"
echo "(\\1=2024, \\2=03, \\3=17 → dd/mm/yyyy)"
echo ""
echo "--- -E (extended regex, sin escapar parentesis) ---"
echo "2024-03-17" | sed -E "s/([0-9]+)-([0-9]+)-([0-9]+)/\3\/\2\/\1/"
echo "(mismo resultado, mas legible con -E)"
echo ""
echo "--- Ejemplo: extraer valores ---"
echo "name=John age=30 city=Madrid" | sed -E "s/name=([^ ]+).*/\1/"
echo ""
echo "--- Duplicar palabras ---"
echo "hello world" | sed -E "s/([a-z]+)/\1 \1/g"
echo ""
echo "--- Swap dos campos ---"
echo "apellido, nombre" | sed -E "s/([^,]+), *(.+)/\2 \1/"
'
```

### Paso 3.2: Hold space

```bash
docker compose exec debian-dev bash -c '
echo "=== Hold space ==="
echo ""
echo "sed tiene dos buffers:"
echo "  Pattern space: la linea actual (donde trabajan los comandos)"
echo "  Hold space:    buffer auxiliar (vacio por defecto)"
echo ""
echo "Comandos:"
echo "  h → copiar pattern → hold (overwrite)"
echo "  H → append pattern → hold"
echo "  g → copiar hold → pattern (overwrite)"
echo "  G → append hold → pattern"
echo "  x → swap pattern ↔ hold"
echo ""
echo "--- Ejemplo: invertir lineas ---"
echo -e "1\n2\n3\n4\n5" | sed -n "1!G; h; \$p"
echo "(almacena en hold en orden inverso, imprime al final)"
echo ""
echo "--- Ejemplo: unir cada par de lineas ---"
echo -e "clave1\nvalor1\nclave2\nvalor2" | sed "N; s/\n/ = /"
echo "(N: append siguiente linea al pattern space)"
echo ""
echo "El hold space es avanzado — para la mayoria de casos,"
echo "awk o un while read son mas legibles"
'
```

### Paso 3.3: Recetas utiles

```bash
docker compose exec debian-dev bash -c '
echo "=== Recetas utiles de sed ==="
echo ""
echo "--- Eliminar lineas en blanco ---"
echo -e "line1\n\n\nline2\n\nline3" | sed "/^$/d"
echo ""
echo "--- Eliminar espacios al inicio y final (trim) ---"
echo "   hello world   " | sed "s/^[[:space:]]*//; s/[[:space:]]*$//"
echo ""
echo "--- Eliminar comentarios ---"
echo -e "data  # comment\n# full comment\nmore data" | sed "s/#.*//; /^[[:space:]]*$/d"
echo ""
echo "--- Numerar lineas ---"
echo -e "alpha\nbeta\ngamma" | sed "=" | sed "N; s/\n/\t/"
echo ""
echo "--- Insertar linea despues de un patron ---"
echo -e "[section]\nkey1=val1\nkey2=val2" | sed "/\[section\]/a\\new_key=new_val"
echo ""
echo "--- Cambiar entre la linea N y M ---"
echo -e "a\nb\nc\nd\ne" | sed "2,4s/.*/REPLACED/"
echo ""
echo "--- Imprimir solo lineas entre dos patrones ---"
echo -e "START\ndata1\ndata2\nEND\nignored" | sed -n "/START/,/END/p"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `s/old/new/` sustituye la primera ocurrencia. `g` reemplaza
   todas, `i` ignora caso, `p` imprime la linea modificada,
   `N` reemplaza la N-esima ocurrencia.

2. Los **delimitadores** pueden ser cualquier caracter (`|`, `#`,
   `@`). Usar alternativas cuando el patron contiene `/`.

3. Las **direcciones** seleccionan lineas: numero (`3`), rango
   (`2,5`), regex (`/patron/`), rango regex (`/ini/,/fin/`),
   ultima (`$`), y negacion (`!`).

4. `-i` edita el archivo directamente (irreversible sin `.bak`).
   `-i.bak` crea un backup antes de modificar.

5. **Backreferences**: `&` es el match completo, `\1`-`\9` son
   grupos capturados. Con `-E` no hay que escapar `()`.

6. El **hold space** (h/H/g/G/x) permite operaciones multilinea
   pero es complejo. Para la mayoria de casos, awk o while read
   son mas legibles.
