# T03 — cut, sort, uniq, tr, tee, xargs

Estas herramientas son los bloques de construcción de los pipelines de texto en
Unix. Cada una hace **una cosa bien** y se combinan con `|`.

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

```bash
# -u — unique (eliminar duplicados después de ordenar):
sort -u file                  # como sort | uniq pero más eficiente

# -s — stable sort (preserva orden original entre iguales):
sort -s -k2,2 file

# -f — case-insensitive (fold case):
sort -f file                  # Apple y apple se tratan igual

# -V — version sort:
echo -e "v1.2\nv1.10\nv1.1" | sort -V
# v1.1
# v1.2
# v1.10  (no v1.10, v1.1, v1.2 como haría sort normal)

# -z — null-terminated (para archivos con nombres con newlines):
find . -print0 | sort -z | xargs -0 echo

# -o — output to file (seguro para sort file -o file):
sort -o file file             # ordena in-place (NO usar sort file > file)

# -k — definición de clave completa:
# -kN,M   campo N a M
# -kN.C   campo N, carácter C
# n = numérico, r = reverso, f = case-insensitive
sort -t, -k2,2nr file         # campo 2, numérico, reverso
```

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
cat access.log | awk '{ print $1 }' | sort | uniq -c | sort -rn | head -10
# Top 10 IPs por número de requests

# Desglose:
# awk '{ print $1 }'  — extraer el campo de interés
# sort                 — ordenar (necesario para uniq)
# uniq -c              — contar ocurrencias
# sort -rn             — ordenar por frecuencia descendente
# head -10             — top 10
```

### uniq con campos

```bash
# -f N — ignorar los primeros N campos:
echo -e "1 apple\n2 apple\n3 banana" | uniq -f1
# 1 apple
# 3 banana  (ignora el primer campo al comparar)

# -s N — ignorar los primeros N caracteres:
echo -e "01_hello\n02_hello\n03_world" | uniq -s3
# 01_hello
# 03_world
```

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

# [:alnum:]   letras y dígitos
# [:alpha:]   letras
# [:blank:]   espacio y tab
# [:digit:]   dígitos
# [:lower:]   minúsculas
# [:upper:]   mayúsculas
# [:space:]   espacios, tabs, newlines, etc.
# [:punct:]   puntuación
```

### Recetas con tr

```bash
# Convertir line endings Windows → Unix:
tr -d '\r' < file.dos > file.unix

# Generar password aleatorio:
tr -dc 'a-zA-Z0-9!@#$' < /dev/urandom | head -c 20; echo

# Reemplazar newlines por comas:
echo -e "a\nb\nc" | tr '\n' ','
# a,b,c,

# Reemplazar tabs por espacios:
tr '\t' ' ' < file.tsv
```

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

# Uso con sudo (el clásico problema de redirección):
# Esto FALLA (la redirección la hace el shell del usuario, no root):
sudo echo "dato" > /etc/config

# Esto FUNCIONA (tee escribe como root):
echo "dato" | sudo tee /etc/config > /dev/null

# Append con sudo:
echo "dato" | sudo tee -a /etc/config > /dev/null
```

### tee en pipelines

```bash
# Guardar un paso intermedio del pipeline:
cat access.log | awk '{ print $1 }' | tee /tmp/ips.txt | sort | uniq -c | sort -rn
# /tmp/ips.txt contiene las IPs sin procesar
# stdout tiene el top de frecuencias

# Debug de pipelines — ver qué pasa entre pasos:
cat data.csv | tee /dev/stderr | sort -t, -k2 | tee /dev/stderr | head -5
# /dev/stderr muestra los datos intermedios sin interferir con stdout
```

## xargs — Construir comandos desde stdin

`xargs` lee items de stdin y los pasa como argumentos a un comando:

```bash
# Básico:
echo "file1 file2 file3" | xargs rm
# Ejecuta: rm file1 file2 file3

# Equivalente a:
echo "file1 file2 file3" | xargs -t rm
# rm file1 file2 file3  (-t = trace, muestra el comando)
# Elimina los tres archivos
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

# Otro ejemplo:
find . -name "*.bak" | xargs -I {} mv {} {}.old
# Renombra file.bak → file.bak.old
```

### -0 — Null-terminated

```bash
# -0 trabaja con items separados por null (\0) en lugar de newline:
# NECESARIO para archivos con espacios, newlines, etc.

# Siempre combinar find -print0 con xargs -0:
find /tmp -name "*.log" -print0 | xargs -0 rm
# Seguro con "mi archivo.log" (tiene espacio)

# Sin -0, "mi archivo.log" se partiría en "mi" y "archivo.log"
```

### -n — Máximo de argumentos por ejecución

```bash
# -n N ejecuta el comando con máximo N argumentos por vez:
echo {1..10} | xargs -n 3 echo
# echo 1 2 3
# echo 4 5 6
# echo 7 8 9
# echo 10
```

### -P — Ejecución paralela

```bash
# -P N ejecuta hasta N procesos en paralelo:
find . -name "*.jpg" -print0 | xargs -0 -P 4 -I {} convert {} -resize 50% {}
# Procesa 4 imágenes simultáneamente

# -P 0 = tantos procesos como CPUs disponibles:
echo {1..100} | xargs -n 1 -P 0 -I {} sh -c 'echo "procesando {}"'

# Descargar múltiples URLs en paralelo:
cat urls.txt | xargs -P 8 -I {} curl -sO {}
```

### xargs con comandos complejos

```bash
# Para comandos complejos, usar sh -c:
find . -name "*.txt" | xargs -I {} sh -c 'echo "Procesando {}"; wc -l "{}"'

# Verificar antes de ejecutar (-p = prompt):
find /tmp -name "*.old" | xargs -p rm
# Pregunta: rm file1.old file2.old? (y/n)
```

## Pipelines idiomáticos

### Combinar herramientas

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
# Alternativa solo con herramientas básicas:
cat -n file | sort -k2 -u | sort -n | cut -f2-

# Encontrar archivos modificados hoy y listar por tamaño:
find . -maxdepth 1 -mtime -1 -type f -print0 | xargs -0 ls -lhS
```

---

## Ejercicios

### Ejercicio 1 — Pipeline de frecuencia

```bash
# Del /etc/passwd, contar cuántos usuarios usan cada shell:
cut -d: -f7 /etc/passwd | sort | uniq -c | sort -rn
# ¿Cuál es el shell más usado?
```

### Ejercicio 2 — Combinar cut, sort y tr

```bash
# Listar todos los usuarios con UID >= 1000, en mayúsculas, separados por coma:
awk -F: '$3 >= 1000 { print $1 }' /etc/passwd | tr '[:lower:]' '[:upper:]' | sort | paste -sd,
```

### Ejercicio 3 — xargs práctico

```bash
# Crear 5 archivos de prueba y luego eliminarlos con xargs:
mkdir /tmp/xargs-test && cd /tmp/xargs-test

# Crear:
echo {1..5} | xargs -n 1 -I {} touch "file_{}.txt"
ls

# Eliminar con xargs:
ls *.txt | xargs rm

cd - && rm -rf /tmp/xargs-test
```
