# Lab — cut, sort, uniq, tr, tee, xargs

## Objetivo

Dominar las herramientas clasicas de procesamiento de texto:
cut para extraer campos/caracteres, sort con sus opciones avanzadas
(-k, -h, -V, -t), uniq para frecuencias y duplicados, tr para
transliteracion y eliminacion de caracteres, tee para bifurcar
output, y xargs para construir comandos desde stdin.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — cut, sort, uniq

### Objetivo

Extraer campos, ordenar datos, y analizar frecuencias.

### Paso 1.1: cut — extraer campos

```bash
docker compose exec debian-dev bash -c '
echo "=== cut ==="
echo ""
echo "--- -d y -f: separador y campo ---"
echo "Usuarios con su shell:"
head -5 /etc/passwd | cut -d: -f1,7
echo ""
echo "--- Multiples campos ---"
echo "Usuario, UID, home:"
head -5 /etc/passwd | cut -d: -f1,3,6
echo ""
echo "--- Rango de campos ---"
head -3 /etc/passwd | cut -d: -f1-3
echo "(campos 1 a 3)"
echo ""
echo "--- -c: por posicion de caracter ---"
echo "Primeros 10 caracteres de cada linea:"
head -3 /etc/passwd | cut -c1-10
echo ""
echo "--- Limitacion: un solo caracter como delimitador ---"
echo "cut -d: solo acepta UN caracter"
echo "Para separadores complejos (espacios multiples, regex): usar awk"
echo ""
echo "--- Comparar cut vs awk ---"
echo "cut -d: -f1    equivale a    awk -F: '\\''{print \$1}'\\'' "
head -3 /etc/passwd | cut -d: -f1
head -3 /etc/passwd | awk -F: "{print \$1}"
'
```

### Paso 1.2: sort — opciones avanzadas

```bash
docker compose exec debian-dev bash -c '
echo "=== sort ==="
echo ""
echo "--- Default: orden alfabetico ---"
echo -e "banana\napple\ncherry\ndate" | sort
echo ""
echo "--- -n: numerico ---"
echo -e "10\n2\n30\n1\n20" | sort -n
echo ""
echo "--- -r: reverso ---"
echo -e "10\n2\n30" | sort -nr
echo ""
echo "--- -k: ordenar por campo ---"
echo "Usuarios por UID (campo 3):"
head -10 /etc/passwd | sort -t: -k3 -n | cut -d: -f1,3
echo ""
echo "--- -k con rango: campo.caracter ---"
echo -e "Alice 30 Madrid\nBob 25 Barcelona\nCarol 35 Madrid" | sort -k3,3 -k2,2n
echo "(ordenar por ciudad, luego por edad numerica)"
echo ""
echo "--- -h: human-readable (K, M, G) ---"
echo -e "10K\n5M\n1G\n500K\n2M" | sort -h
echo ""
echo "--- -V: version sort ---"
echo -e "v1.10\nv1.2\nv1.9\nv2.0\nv1.1" | sort -V
echo ""
echo "--- -u: eliminar duplicados ---"
echo -e "apple\nbanana\napple\ncherry\nbanana" | sort -u
'
```

### Paso 1.3: uniq — frecuencias y duplicados

```bash
docker compose exec debian-dev bash -c '
echo "=== uniq ==="
echo ""
echo "IMPORTANTE: uniq solo detecta duplicados ADYACENTES"
echo "Siempre usar sort | uniq"
echo ""
echo "--- -c: contar ocurrencias ---"
echo "Shells en uso (frecuencia):"
cut -d: -f7 /etc/passwd | sort | uniq -c | sort -rn
echo ""
echo "--- -d: solo duplicados ---"
echo -e "apple\nbanana\napple\ncherry\napple\nbanana" | sort | uniq -d
echo "(solo elementos que aparecen mas de una vez)"
echo ""
echo "--- -u: solo unicos ---"
echo -e "apple\nbanana\napple\ncherry\napple\nbanana" | sort | uniq -u
echo "(solo elementos que aparecen exactamente una vez)"
echo ""
echo "--- sort | uniq -c | sort -rn: top frecuencias ---"
echo "Es el patron mas comun con uniq"
echo "Equivale a un GROUP BY + COUNT + ORDER BY DESC en SQL"
'
```

---

## Parte 2 — tr y tee

### Objetivo

Transliterar caracteres con tr y bifurcar output con tee.

### Paso 2.1: tr — transliterar

```bash
docker compose exec debian-dev bash -c '
echo "=== tr ==="
echo ""
echo "--- Transliterar (reemplazar caracter por caracter) ---"
echo "Hello World" | tr "a-z" "A-Z"
echo "HELLO WORLD" | tr "A-Z" "a-z"
echo ""
echo "--- -d: eliminar caracteres ---"
echo "Hello, World! 123" | tr -d "0-9"
echo "(eliminar digitos)"
echo "Hello, World! 123" | tr -d "[:punct:]"
echo "(eliminar puntuacion)"
echo ""
echo "--- -s: squeeze (colapsar repetidos) ---"
echo "hello     world" | tr -s " "
echo "(multiples espacios → uno solo)"
echo ""
echo -e "line1\n\n\n\nline2\n\n\nline3" | tr -s "\n"
echo "(multiples newlines → uno solo)"
echo ""
echo "--- -c: complemento (todos EXCEPTO) ---"
echo "Hello World 123!@#" | tr -cd "a-zA-Z "
echo ""
echo "(eliminar todo EXCEPTO letras y espacios)"
echo ""
echo "--- Clases POSIX ---"
echo "  [:alpha:]  letras"
echo "  [:digit:]  digitos"
echo "  [:alnum:]  alfanumericos"
echo "  [:space:]  espacios/tabs/newlines"
echo "  [:punct:]  puntuacion"
echo "  [:upper:]  mayusculas"
echo "  [:lower:]  minusculas"
'
```

### Paso 2.2: tr — usos practicos

```bash
docker compose exec debian-dev bash -c '
echo "=== Usos practicos de tr ==="
echo ""
echo "--- Convertir delimitadores ---"
echo "PATH: $PATH" | tr ":" "\n" | head -5
echo ""
echo "--- Generar contrasena aleatoria ---"
tr -dc "A-Za-z0-9" < /dev/urandom | head -c 16
echo ""
echo ""
echo "--- Limpiar input de Windows (\\r\\n → \\n) ---"
printf "linea1\r\nlinea2\r\n" | tr -d "\r" | cat -A
echo "(sin ^M al final)"
echo ""
echo "--- ROT13 ---"
echo "Hello World" | tr "A-Za-z" "N-ZA-Mn-za-m"
echo "Uryyb Jbeyq" | tr "A-Za-z" "N-ZA-Mn-za-m"
echo "(cifrado/descifrado ROT13)"
'
```

### Paso 2.3: tee — bifurcar output

```bash
docker compose exec debian-dev bash -c '
echo "=== tee ==="
echo ""
echo "--- tee: escribir a stdout Y a un archivo ---"
echo "datos importantes" | tee /tmp/tee-output.txt
echo ""
echo "Archivo creado:"
cat /tmp/tee-output.txt
echo ""
echo "--- tee -a: append ---"
echo "mas datos" | tee -a /tmp/tee-output.txt
echo ""
echo "Archivo con append:"
cat /tmp/tee-output.txt
echo ""
echo "--- tee a multiples archivos ---"
echo "broadcast" | tee /tmp/file1.txt /tmp/file2.txt > /dev/null
echo "file1: $(cat /tmp/file1.txt)"
echo "file2: $(cat /tmp/file2.txt)"
echo ""
echo "--- Patron sudo: escribir a archivo protegido ---"
echo "echo \"data\" | sudo tee /etc/myconfig.conf > /dev/null"
echo "(echo | sudo tee es la alternativa a sudo echo > file)"
echo "(porque la redireccion > corre como usuario, no como root)"
echo ""
echo "--- tee en pipelines para debug ---"
echo -e "3\n1\n2" | tee /dev/stderr | sort | tee /dev/stderr | head -1
echo "(ver el estado intermedio del pipeline)"
rm -f /tmp/tee-output.txt /tmp/file1.txt /tmp/file2.txt
'
```

---

## Parte 3 — xargs y pipelines

### Objetivo

Construir comandos desde stdin con xargs y componer pipelines
complejos.

### Paso 3.1: xargs basico

```bash
docker compose exec debian-dev bash -c '
echo "=== xargs ==="
echo ""
echo "xargs lee argumentos de stdin y los pasa a un comando"
echo ""
echo "--- Basico: pasar como argumentos ---"
echo "file1.txt file2.txt file3.txt" | xargs echo "Archivos:"
echo ""
echo "--- -I {}: placeholder ---"
echo -e "alice\nbob\ncharlie" | xargs -I{} echo "Usuario: {}"
echo ""
echo "--- -n N: N argumentos por ejecucion ---"
echo "a b c d e f" | xargs -n 2 echo "Par:"
echo ""
echo "--- Ejemplo practico: buscar y procesar ---"
echo "Archivos .conf en /etc (primeros 5):"
find /etc -maxdepth 1 -name "*.conf" -type f 2>/dev/null | head -5 | xargs -I{} bash -c "echo \"{}: \$(wc -l < {}) lineas\""
'
```

### Paso 3.2: xargs -0 y -P

```bash
docker compose exec debian-dev bash -c '
echo "=== xargs -0 y -P ==="
echo ""
echo "--- -0: separador null (para nombres con espacios) ---"
mkdir -p /tmp/xargs-test
touch "/tmp/xargs-test/normal.txt"
touch "/tmp/xargs-test/con espacio.txt"
echo ""
echo "find -print0 | xargs -0: seguro con espacios"
find /tmp/xargs-test -name "*.txt" -print0 | xargs -0 ls -la
echo ""
echo "--- -P N: ejecutar N procesos en paralelo ---"
echo "Procesando en paralelo (P 3):"
echo -e "1\n2\n3\n4\n5\n6" | xargs -P 3 -I{} bash -c "sleep 0.2; echo \"  Proceso {} completado\""
echo ""
echo "Opciones importantes:"
echo "  -I {}   placeholder para el argumento"
echo "  -0      input separado por null"
echo "  -n N    N argumentos por ejecucion"
echo "  -P N    N procesos en paralelo"
echo "  -r      no ejecutar si stdin esta vacio"
echo ""
rm -rf /tmp/xargs-test
'
```

### Paso 3.3: Pipelines compuestos

Antes de ejecutar, predice: que hace este pipeline completo?
`cut -d: -f7 /etc/passwd | sort | uniq -c | sort -rn | head -5`

```bash
docker compose exec debian-dev bash -c '
echo "=== Pipelines compuestos ==="
echo ""
echo "--- Top 5 shells mas usados ---"
cut -d: -f7 /etc/passwd | sort | uniq -c | sort -rn | head -5
echo ""
echo "Desglose:"
echo "  cut -d: -f7   → extraer campo shell"
echo "  sort           → ordenar (requerido por uniq)"
echo "  uniq -c        → contar ocurrencias"
echo "  sort -rn       → ordenar por frecuencia (mayor primero)"
echo "  head -5        → solo los 5 primeros"
echo ""
echo "--- Archivos mas grandes en /etc ---"
find /etc -maxdepth 1 -type f -exec wc -c {} \; 2>/dev/null | sort -rn | head -5
echo ""
echo "--- Procesos unicos por usuario ---"
ps aux --no-header 2>/dev/null | awk "{print \$1}" | sort | uniq -c | sort -rn | head -5
echo ""
echo "--- Palabras mas comunes en un archivo ---"
tr "[:upper:]" "[:lower:]" < /etc/services | tr -cs "[:alpha:]" "\n" | sort | uniq -c | sort -rn | head -5
echo "(normalizar → separar palabras → contar → top 5)"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. **cut** extrae campos (`-d` `-f`) o caracteres (`-c`).
   Limitacion: solo acepta un caracter como delimitador. Para
   separadores complejos, usar awk.

2. **sort** ordena con `-n` (numerico), `-h` (human K/M/G),
   `-V` (versiones), `-k` (por campo), `-u` (unico), `-t`
   (delimitador). `-k3,3n` ordena por campo 3 numericamente.

3. **uniq** requiere input ordenado. `-c` cuenta frecuencias,
   `-d` muestra solo duplicados, `-u` solo unicos. El patron
   `sort | uniq -c | sort -rn` es equivalente a GROUP BY.

4. **tr** transliterar caracter por caracter. `-d` elimina,
   `-s` colapsa repetidos, `-c` complemento. Opera sobre
   caracteres individuales, no strings.

5. **tee** bifurca stdout a archivo(s) y terminal. `echo | sudo
   tee` es el patron para escribir a archivos protegidos.
   `-a` hace append.

6. **xargs** construye comandos desde stdin. `-I {}` como
   placeholder, `-0` para null-separated, `-P N` para
   ejecucion paralela. Esencial con find.
