# T03 — cut, sort, uniq, tr, tee, xargs

> **Fuentes:** `README.md` (base) + `README.max.md` (ampliado) + `labs/README.md`
>
> **Erratas corregidas respecto a las fuentes:**
>
> | Ubicación | Error | Corrección |
> |---|---|---|
> | md:336, max.md:357 | `xargs -I {} sh -c '...{}'` — interpola filename en string de shell (command injection si el nombre contiene `$(cmd)`) | Patrón seguro: `sh -c '... "$1"' _ {}` pasando `{}` como argumento posicional |

Estas herramientas son los bloques de construcción de los pipelines de texto en
Unix. Cada una hace **una cosa bien** y se combinan con `|`.

---

## cut — Extraer campos o columnas

```bash
# Por delimitador y campo (como awk -F pero más simple):
cut -d: -f1 /etc/passwd           # campo 1 (usuario)
cut -d: -f1,3 /etc/passwd         # campos 1 y 3 (usuario, UID)
cut -d: -f1,3-5 /etc/passwd       # campos 1, 3, 4, 5
cut -d: -f3- /etc/passwd          # desde el campo 3 hasta el final

# -d = delimitador (un solo carácter)
# -f = field (campo)
```

```bash
# Por posición de carácter:
cut -c1-10 file                   # primeros 10 caracteres
cut -c5 file                     # solo el carácter 5
cut -c5- file                    # desde el carácter 5 hasta el final

# Por bytes (distinto de -c en locales multibyte como UTF-8):
cut -b1-10 file
```

### Limitaciones de cut

```bash
# cut NO maneja múltiples delimitadores consecutivos:
echo "a  b  c" | cut -d' ' -f2
# (vacío — el segundo campo es vacío entre los dos espacios)

# Para eso, usar awk:
echo "a  b  c" | awk '{ print $2 }'
# b

# cut NO soporta delimitadores de múltiples caracteres:
echo "a::b::c" | cut -d'::' -f2    # Error: el delimitador es un solo carácter
# Usar awk -F'::' en su lugar

# --complement — todo EXCEPTO los campos indicados:
cut -d: --complement -f2 /etc/passwd   # todos los campos excepto el 2

# --output-delimiter — cambiar el separador de salida:
cut -d: -f1,3 --output-delimiter=$'\t' /etc/passwd
# root	0
```

> **cut vs awk:** `cut -d: -f1` equivale a `awk -F: '{print $1}'`.
> Usar cut cuando el caso es simple (un delimitador, campos fijos).
> Usar awk cuando hay delimitadores múltiples, lógica condicional, o
> necesitas calcular.

---

## sort — Ordenar líneas

```bash
# Orden alfabético (default):
sort file

# Orden numérico:
sort -n file                 # 1, 2, 10, 20 (no 1, 10, 2, 20)

# Orden inverso:
sort -r file                 # descendente
sort -rn file                # numérico descendente

# Por campo específico:
sort -t: -k3,3n /etc/passwd  # ordenar por UID (campo 3, numérico)
# -t: = delimitador :
# -k3,3n = key campo 3 a 3, numérico

# Múltiples claves:
sort -t: -k4,4n -k3,3n /etc/passwd   # por GID, luego por UID

# Human-readable (K, M, G):
du -sh /* 2>/dev/null | sort -h       # ordena 1K < 1M < 1G
```

### Flags importantes

| Flag | Nombre | Efecto |
|---|---|---|
| `-n` | numeric | Ordenar numéricamente |
| `-r` | reverse | Orden descendente |
| `-u` | unique | Eliminar duplicados después de ordenar |
| `-s` | stable | Preserva orden original entre iguales |
| `-f` | fold-case | Case-insensitive |
| `-h` | human | Ordenar K, M, G correctamente |
| `-V` | version | Version sort (1.1, 1.2, 1.10) |
| `-z` | zero-terminated | Para archivos con newlines en nombres |
| `-o` | output | Escribir a archivo (seguro para in-place) |

```bash
# -V — version sort:
echo -e "v1.2\nv1.10\nv1.1" | sort -V
# v1.1
# v1.2
# v1.10
# (sort alfabético daría v1.1, v1.10, v1.2 — incorrecto para versiones)

# -o — output to file (seguro para in-place):
sort -o file file             # ordena in-place (NO usar sort file > file)
# sort file > file trunca el archivo ANTES de leerlo → vacío

# -k — definición de clave completa:
# -kN,M   campo N a M
# -kN.C   campo N, carácter C
# Modificadores después: n=numérico, r=reverso, f=case-insensitive
sort -t, -k2,2nr file         # campo 2, numérico, reverso
```

---

## uniq — Filtrar duplicados

`uniq` opera sobre líneas **adyacentes** — el input debe estar **ordenado**:

```bash
# Eliminar duplicados adyacentes:
sort file | uniq

# -c — contar ocurrencias:
sort file | uniq -c
#   3 apple
#   1 banana
#   2 cherry

# -d — mostrar solo duplicados:
sort file | uniq -d

# -u — mostrar solo líneas únicas (sin duplicados):
sort file | uniq -u

# -i — case-insensitive:
sort -f file | uniq -i
```

### sort | uniq -c | sort -rn — El patrón de frecuencia

```bash
# Patrón idiomático para contar frecuencias:
awk '{ print $1 }' access.log | sort | uniq -c | sort -rn | head -10
# Top 10 IPs por número de requests

# Desglose:
# awk '{ print $1 }'  — extraer el campo de interés
# sort                 — ordenar (necesario para uniq)
# uniq -c              — contar ocurrencias
# sort -rn             — ordenar por frecuencia descendente
# head -10             — top 10

# Equivalente en SQL: SELECT ip, COUNT(*) as n FROM log GROUP BY ip ORDER BY n DESC LIMIT 10
```

### uniq con campos

```bash
# -f N — ignorar los primeros N campos al comparar:
echo -e "1 apple\n2 apple\n3 banana" | uniq -f1
# 1 apple
# 3 banana  (ignora el primer campo al comparar)

# -s N — ignorar los primeros N caracteres:
echo -e "01_hello\n02_hello\n03_world" | uniq -s3
# 01_hello
# 03_world
```

---

## tr — Translate/Delete caracteres

`tr` opera **carácter por carácter** (no strings). Lee de stdin solamente:

```bash
# Traducir (reemplazar) caracteres:
echo "hello" | tr 'a-z' 'A-Z'       # HELLO
echo "hello" | tr 'helo' 'HELO'     # HELLO
echo "hello" | tr aeiou '*'         # h*ll*

# -d — delete caracteres:
echo "hello 123 world" | tr -d '0-9'     # hello  world
echo "hello world" | tr -d ' '           # helloworld
echo "hello" | tr -d '\n'                # hello (sin newline)

# -s — squeeze (colapsar repetidos):
echo "heeellooo" | tr -s 'eo'       # helo
echo "a   b   c" | tr -s ' '        # a b c

# -c — complement (todo EXCEPTO los caracteres indicados):
echo "hello 123 world" | tr -dc '0-9\n'   # 123 (solo dígitos y newline)
echo "hello world!" | tr -dc 'a-zA-Z \n'  # hello world (solo letras y espacios)
```

### Clases de caracteres POSIX

```bash
tr '[:lower:]' '[:upper:]'     # minúsculas a mayúsculas
tr '[:upper:]' '[:lower:]'     # mayúsculas a minúsculas
tr -d '[:digit:]'              # eliminar dígitos
tr -d '[:space:]'              # eliminar espacios, tabs, newlines
tr -d '[:punct:]'              # eliminar puntuación
tr -s '[:blank:]' ' '         # colapsar tabs/espacios a un solo espacio
```

| Clase | Contenido |
|---|---|
| `[:alnum:]` | letras y dígitos |
| `[:alpha:]` | letras |
| `[:blank:]` | espacio y tab |
| `[:digit:]` | dígitos |
| `[:lower:]` | minúsculas |
| `[:upper:]` | mayúsculas |
| `[:space:]` | espacios, tabs, newlines, etc. |
| `[:punct:]` | puntuación |

### Recetas con tr

```bash
# Convertir line endings Windows → Unix:
tr -d '\r' < file.dos > file.unix

# Generar password aleatorio:
tr -dc 'a-zA-Z0-9!@#$' < /dev/urandom | head -c 20; echo

# Reemplazar newlines por comas:
echo -e "a\nb\nc" | tr '\n' ','
# a,b,c,  (nota: coma final — tr reemplaza el último \n también)

# Reemplazar tabs por espacios:
tr '\t' ' ' < file.tsv

# ROT13:
echo "Hello World" | tr 'A-Za-z' 'N-ZA-Mn-za-m'
# Uryyb Jbeyq
# Aplicar dos veces devuelve el original

# Separar $PATH en líneas:
echo "$PATH" | tr ':' '\n'
```

---

## tee — Bifurcar la salida

`tee` escribe la entrada a stdout **y** a uno o más archivos simultáneamente:

```bash
# Guardar en archivo Y ver en pantalla:
ls -la | tee /tmp/listing.txt
# La salida se muestra en pantalla Y se guarda en el archivo

# Append (no sobrescribir):
echo "nueva línea" | tee -a /tmp/log.txt

# Múltiples archivos:
echo "dato" | tee file1.txt file2.txt file3.txt
```

### tee y sudo

```bash
# Uso con sudo (el clásico problema de redirección):
# Esto FALLA (la redirección la hace el shell del usuario, no root):
sudo echo "dato" > /etc/config    # Error: Permission denied

# Esto FUNCIONA (tee escribe como root):
echo "dato" | sudo tee /etc/config > /dev/null

# Append con sudo:
echo "dato" | sudo tee -a /etc/config > /dev/null
```

> **¿Por qué falla `sudo echo > /etc/config`?** El shell evalúa la
> redirección `>` **antes** de ejecutar `sudo`. Así que es el shell del
> usuario (sin privilegios) el que intenta abrir `/etc/config` para
> escritura. `sudo` solo eleva `echo`, no la redirección. Con `sudo tee`,
> es `tee` el que abre el archivo, y `tee` corre como root.

### tee en pipelines

```bash
# Guardar un paso intermedio del pipeline:
awk '{ print $1 }' access.log | tee /tmp/ips.txt | sort | uniq -c | sort -rn
# /tmp/ips.txt contiene las IPs sin procesar
# stdout tiene el top de frecuencias

# Debug de pipelines — ver qué pasa entre pasos:
cat data.csv | tee /dev/stderr | sort -t, -k2 | tee /dev/stderr | head -5
# /dev/stderr muestra los datos intermedios sin interferir con stdout
```

---

## xargs — Construir comandos desde stdin

`xargs` lee items de stdin y los pasa como argumentos a un comando:

```bash
# Básico:
echo "file1 file2 file3" | xargs rm
# Ejecuta: rm file1 file2 file3

# -t = trace (muestra el comando antes de ejecutar):
echo "file1 file2 file3" | xargs -t rm
```

### -I {} — Placeholder

```bash
# -I {} define un placeholder que se reemplaza por cada item:
echo -e "file1\nfile2\nfile3" | xargs -I {} cp {} /backup/
# Ejecuta:
# cp file1 /backup/
# cp file2 /backup/
# cp file3 /backup/

# -I hace que xargs ejecute el comando UNA VEZ POR LÍNEA
# Sin -I, xargs pasa todos los items juntos
```

### -0 — Null-terminated (seguridad con nombres)

```bash
# -0 trabaja con items separados por null (\0) en lugar de newline:
# NECESARIO para archivos con espacios, newlines, comillas, etc.

# Siempre combinar find -print0 con xargs -0:
find /tmp -name "*.log" -print0 | xargs -0 rm
# Seguro con "mi archivo.log" (tiene espacio)

# Sin -0, "mi archivo.log" se partiría en "mi" y "archivo.log"
```

### -n — Máximo de argumentos por ejecución

```bash
# -n N ejecuta el comando con máximo N argumentos por vez:
echo {1..10} | xargs -n 3 echo
# 1 2 3
# 4 5 6
# 7 8 9
# 10
```

### -P — Ejecución paralela

```bash
# -P N ejecuta hasta N procesos en paralelo:
find . -name "*.jpg" -print0 | xargs -0 -P 4 -I {} convert {} -resize 50% {}
# Procesa 4 imágenes simultáneamente

# -P 0 = tantos procesos como CPUs disponibles

# Descargar múltiples URLs en paralelo:
cat urls.txt | xargs -P 8 -I {} curl -sO {}
```

### xargs con comandos complejos (forma segura)

```bash
# INSEGURO — {} se interpola en la string de sh -c:
# find . -name "*.txt" | xargs -I {} sh -c 'echo "{}"; wc -l "{}"'
# Si un archivo se llama $(rm -rf /).txt, se ejecutaría el comando

# SEGURO — pasar {} como argumento posicional:
find . -name "*.txt" -print0 | xargs -0 -I {} sh -c 'echo "$1"; wc -l "$1"' _ {}
# _ es un placeholder para $0 (nombre del script), {} va a $1

# -p = prompt (pedir confirmación antes de ejecutar):
find /tmp -name "*.old" | xargs -p rm
# Pregunta: rm file1.old file2.old? (y/n)

# -r = no ejecutar si stdin está vacío (evita errores):
echo "" | xargs -r rm    # no ejecuta rm sin argumentos
```

---

## Pipelines idiomáticos

```bash
# Top 10 procesos por memoria:
ps aux --no-headers | sort -k4 -rn | head -10 | awk '{ printf "%-10s %5s%% %s\n", $1, $4, $11 }'

# Contar extensiones de archivo:
find . -type f | sed 's/.*\.//' | sort | uniq -c | sort -rn

# Usuarios con shell interactivo, ordenados por UID:
grep -v nologin /etc/passwd | cut -d: -f1,3,7 | sort -t: -k2 -n

# Espacio por directorio, ordenado:
du -sh /var/*/ 2>/dev/null | sort -h

# Eliminar líneas duplicadas preservando orden:
awk '!seen[$0]++' file

# Encontrar archivos modificados hoy y listar por tamaño:
find . -maxdepth 1 -mtime -1 -type f -print0 | xargs -0 ls -lhS

# Palabras más comunes en un archivo:
tr '[:upper:]' '[:lower:]' < file | tr -cs '[:alpha:]' '\n' | sort | uniq -c | sort -rn | head -10
```

---

## Ejercicios

### Ejercicio 1 — cut: extraer campos

```bash
# a) Extraer usuario y shell de /etc/passwd:
cut -d: -f1,7 /etc/passwd | head -5

# b) Extraer solo los primeros 8 caracteres de cada línea:
cut -c1-8 /etc/passwd | head -5

# c) Todos los campos EXCEPTO el password (campo 2):
cut -d: --complement -f2 /etc/passwd | head -3
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué separador usa <code>cut -d: -f1,7</code> en la salida?</summary>

El mismo delimitador de entrada (`:`). Resultado: `root:/bin/bash`. Para cambiar el separador de salida, usar `--output-delimiter`.

</details>

<details><summary>2. ¿Qué diferencia hay entre <code>-c1-8</code> y <code>-b1-8</code>?</summary>

Con texto ASCII son idénticos. Con caracteres multibyte (UTF-8), `-c` cuenta caracteres (un emoji = 1) y `-b` cuenta bytes (un emoji = 3-4 bytes). En la práctica, la implementación de `-c` varía entre sistemas.

</details>

---

### Ejercicio 2 — sort: múltiples claves

```bash
cat << 'EOF' | sort -t: -k3,3 -k1,1n
Madrid:Juan:ventas
Barcelona:Ana:ventas
Madrid:Pedro:compras
Barcelona:Luis:compras
Madrid:Ana:ventas
EOF
```

**Predicción:** antes de ejecutar, responde:

<details><summary>¿En qué orden salen las líneas?</summary>

Primero ordena por campo 3 (departamento, alfabético), luego por campo 1 (ciudad, numérico — pero son strings, así que se compara como texto):

```
Barcelona:Luis:compras
Madrid:Pedro:compras
Barcelona:Ana:ventas
Madrid:Ana:ventas
Madrid:Juan:ventas
```

"compras" < "ventas" alfabéticamente. Dentro de cada departamento, "Barcelona" < "Madrid".

Nota: `-k1,1n` intenta ordenar numéricamente, pero "Barcelona" y "Madrid" no son números (se tratan como 0). Debería ser `-k1,1` sin `n` para orden alfabético.

</details>

---

### Ejercicio 3 — uniq: patrón de frecuencia

```bash
echo -e "error\ninfo\nerror\nwarn\ninfo\ninfo\nerror\nwarn" | sort | uniq -c | sort -rn
```

**Predicción:** antes de ejecutar, responde:

<details><summary>¿Qué produce y por qué el <code>sort</code> intermedio es necesario?</summary>

```
      3 error
      3 info
      2 warn
```

`uniq` solo detecta duplicados **adyacentes**. Sin `sort` previo, `error`, `info`, `error` no se detectarían como duplicados porque no están juntos. `sort` agrupa todas las líneas iguales de forma consecutiva, permitiendo que `uniq -c` las cuente correctamente.

</details>

---

### Ejercicio 4 — tr: transliteración y limpieza

```bash
# a) Mayúsculas:
echo "Hello World 123!" | tr '[:lower:]' '[:upper:]'

# b) Solo dígitos:
echo "Hello World 123!" | tr -dc '[:digit:]\n'

# c) Colapsar espacios:
echo "uno   dos     tres" | tr -s ' '

# d) Eliminar puntuación:
echo "Hello, World! How's it going?" | tr -d '[:punct:]'
```

**Predicción:** antes de ejecutar, responde:

<details><summary>¿Qué produce cada comando?</summary>

```
a) HELLO WORLD 123!      — solo letras minúsculas → mayúsculas, resto intacto
b) 123                    — elimina todo excepto dígitos y newline
c) uno dos tres           — múltiples espacios → uno solo
d) Hello World Hows it going  — elimina , ! ' ?
```

Nota en (d): la apóstrofo en `How's` también es puntuación y se elimina.

</details>

---

### Ejercicio 5 — tee: bifurcar pipeline

```bash
echo -e "3\n1\n4\n1\n5" | tee /tmp/original.txt | sort -n | tee /tmp/sorted.txt | tail -1
echo "--- original ---"
cat /tmp/original.txt
echo "--- sorted ---"
cat /tmp/sorted.txt
rm /tmp/original.txt /tmp/sorted.txt
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué imprime <code>tail -1</code>?</summary>

`5` — es el último número después de ordenar numéricamente (el mayor).

</details>

<details><summary>2. ¿Qué contiene cada archivo?</summary>

`original.txt`: los números sin ordenar (3, 1, 4, 1, 5).
`sorted.txt`: los números ordenados (1, 1, 3, 4, 5).

Cada `tee` captura el estado del pipeline en ese punto.

</details>

---

### Ejercicio 6 — tee con sudo

```bash
# ¿Por qué falla esto?
# sudo echo "nameserver 8.8.8.8" > /etc/resolv.conf

# ¿Y por qué funciona esto?
# echo "nameserver 8.8.8.8" | sudo tee /etc/resolv.conf > /dev/null
```

**Predicción:** antes de ejecutar (mentalmente), responde:

<details><summary>Explica la diferencia</summary>

En `sudo echo "..." > /etc/resolv.conf`:
1. El shell actual (sin privilegios) evalúa `>` y intenta abrir `/etc/resolv.conf` para escritura
2. Falla con "Permission denied" porque **la redirección no es parte de sudo**
3. `sudo` solo eleva `echo`, no la redirección

En `echo "..." | sudo tee /etc/resolv.conf`:
1. `echo` corre como usuario normal (OK — solo escribe a stdout)
2. `sudo tee` corre como root y **es tee quien abre el archivo**
3. `> /dev/null` suprime la salida de tee a terminal (tee escribe a stdout Y al archivo)

</details>

---

### Ejercicio 7 — xargs: básico y -I

```bash
echo -e "alice\nbob\ncharlie" | xargs echo "Usuarios:"
echo "---"
echo -e "alice\nbob\ncharlie" | xargs -I {} echo "Usuario: {}"
```

**Predicción:** antes de ejecutar, responde:

<details><summary>¿Cuál es la diferencia entre ambos?</summary>

Sin `-I`:
```
Usuarios: alice bob charlie
```
Un solo `echo` con todos los argumentos juntos.

Con `-I {}`:
```
Usuario: alice
Usuario: bob
Usuario: charlie
```
Un `echo` por línea — `-I` hace que xargs ejecute el comando **una vez por cada item**.

</details>

---

### Ejercicio 8 — xargs -0: seguridad con espacios

```bash
mkdir -p /tmp/xtest
touch "/tmp/xtest/normal.txt" "/tmp/xtest/con espacio.txt" "/tmp/xtest/otro archivo.txt"

# INSEGURO (rompe con espacios):
find /tmp/xtest -name "*.txt" | xargs ls -la 2>&1

# SEGURO:
find /tmp/xtest -name "*.txt" -print0 | xargs -0 ls -la

rm -rf /tmp/xtest
```

**Predicción:** antes de ejecutar, responde:

<details><summary>¿Qué pasa con la versión insegura?</summary>

xargs interpreta espacios como separadores de argumentos. `"con espacio.txt"` se divide en `"con"` y `"espacio.txt"`, ambos inexistentes → errores "No such file or directory". Lo mismo con "otro archivo.txt".

Con `-print0` + `-0`, el separador es el byte null (`\0`), que no puede aparecer en nombres de archivo, así que cada nombre se trata como una unidad completa.

</details>

---

### Ejercicio 9 — Pipeline completo: análisis de frecuencia

```bash
# Palabras más comunes en /etc/services:
tr '[:upper:]' '[:lower:]' < /etc/services | tr -cs '[:alpha:]' '\n' | sort | uniq -c | sort -rn | head -5
```

**Predicción:** antes de ejecutar, traza cada paso del pipeline:

<details><summary>¿Qué hace cada paso?</summary>

| Paso | Comando | Efecto |
|---|---|---|
| 1 | `tr '[:upper:]' '[:lower:]'` | Normalizar a minúsculas |
| 2 | `tr -cs '[:alpha:]' '\n'` | Reemplazar todo lo que NO es letra por newline, y colapsar newlines repetidos → una palabra por línea |
| 3 | `sort` | Ordenar (necesario para uniq) |
| 4 | `uniq -c` | Contar ocurrencias de cada palabra |
| 5 | `sort -rn` | Ordenar por frecuencia descendente |
| 6 | `head -5` | Top 5 palabras |

El paso 2 es clave: `-c` es complemento (todo excepto `[:alpha:]`), `-s` colapsa repetidos. Así `"tcp  80"` se convierte en `tcp\n80\n` → pero 80 no es alpha, se convierte a newline. Resultado: `tcp\n\n` → con `-s`: `tcp\n`.

Espera — los dígitos no son `[:alpha:]`, así que `80` se reemplaza por newlines. El resultado son solo palabras alfabéticas.

</details>

---

### Ejercicio 10 — Panorama: combinar todas las herramientas

```bash
cat > /tmp/data.csv << 'EOF'
nombre,departamento,salario
Alice,Engineering,75000
Bob,Marketing,55000
Charlie,Engineering,82000
Diana,Marketing,60000
Eve,Engineering,90000
Frank,HR,50000
Grace,HR,52000
EOF

# a) Salarios por departamento (sin header):
tail -n +2 /tmp/data.csv | cut -d, -f2,3 | sort -t, -k1,1

# b) Total de empleados por departamento:
tail -n +2 /tmp/data.csv | cut -d, -f2 | sort | uniq -c | sort -rn

# c) Salario más alto (con nombre):
tail -n +2 /tmp/data.csv | sort -t, -k3,3nr | head -1

# d) Nombres en mayúsculas, separados por " | ":
tail -n +2 /tmp/data.csv | cut -d, -f1 | tr '[:lower:]' '[:upper:]' | sort | paste -sd'|'

rm /tmp/data.csv
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué produce el comando (b)?</summary>

```
      3 Engineering
      2 HR
      2 Marketing
```

`cut -d, -f2` extrae departamento, `sort | uniq -c` cuenta, `sort -rn` ordena por frecuencia.

</details>

<details><summary>2. ¿Qué produce (c) — salario más alto?</summary>

`Eve,Engineering,90000` — `sort -t, -k3,3nr` ordena por campo 3 numérico descendente, `head -1` toma el primero.

</details>

<details><summary>3. ¿Por qué <code>tail -n +2</code> en todos los comandos?</summary>

Para **saltar el header** (`nombre,departamento,salario`). `tail -n +2` imprime desde la línea 2 en adelante. Sin esto, "nombre" se contaría como un departamento, "salario" corrompería el ordenamiento numérico, etc.

</details>
