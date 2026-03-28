# T02 — Loops avanzados

## Errata respecto a los materiales originales

| Ubicacion | Error | Correccion |
|---|---|---|
| max.md:554 | "Ejercicio 8 — break con etiqueta" | Bash no tiene labeled breaks. Correcto: "break con nivel N" (`break 2`, no etiquetas) |

---

## for clasico

```bash
# Iterar sobre una lista explicita:
for FRUIT in apple banana cherry; do
    echo "Fruta: $FRUIT"
done

# Iterar sobre la salida de un comando:
for USER in $(cut -d: -f1 /etc/passwd); do
    echo "Usuario: $USER"
done
# CUIDADO: esto falla con nombres que tienen espacios
# (ver "while read" mas abajo para la forma segura)

# Iterar sobre archivos (glob):
for FILE in /etc/*.conf; do
    echo "Config: $FILE"
done
# El glob se expande antes del loop — seguro con espacios en nombres
```

### for con rangos (brace expansion)

```bash
# Rango numerico:
for i in {1..5}; do
    echo "Numero: $i"
done
# 1, 2, 3, 4, 5

# Rango con paso (bash 4+):
for i in {0..20..5}; do
    echo "Numero: $i"
done
# 0, 5, 10, 15, 20

# Rango descendente:
for i in {10..1}; do
    echo "Countdown: $i"
done

# Rango con letras:
for letter in {a..f}; do
    echo "Letra: $letter"
done
# a, b, c, d, e, f

# IMPORTANTE: brace expansion NO funciona con variables:
N=5
for i in {1..$N}; do    # NO funciona — imprime literal {1..5}
    echo "$i"
done
# Razon: brace expansion ocurre ANTES de variable expansion

# Con variables, usar seq o for estilo C:
for i in $(seq 1 "$N"); do
    echo "$i"
done
```

### for estilo C

```bash
# Sintaxis identica a C/Java:
for ((i = 0; i < 10; i++)); do
    echo "i = $i"
done

# Con paso:
for ((i = 0; i <= 100; i += 10)); do
    echo "i = $i"
done

# Multiples variables:
for ((i = 0, j = 10; i < j; i++, j--)); do
    echo "i=$i j=$j"
done
# i=0 j=10, i=1 j=9, i=2 j=8, i=3 j=7, i=4 j=6

# Loop infinito:
for ((;;)); do
    echo "infinito"
    break   # sin esto, nunca termina
done

# El for estilo C es la forma correcta de iterar con variables numericas
# cuando brace expansion no funciona
```

---

## Iterar sobre arrays

```bash
# Array indexado:
COLORS=(red green blue yellow)

# Iterar valores:
for COLOR in "${COLORS[@]}"; do
    echo "Color: $COLOR"
done

# Iterar indices:
for i in "${!COLORS[@]}"; do
    echo "[$i] = ${COLORS[$i]}"
done
# [0] = red, [1] = green, [2] = blue, [3] = yellow

# Array asociativo:
declare -A PORTS
PORTS[http]=80
PORTS[https]=443
PORTS[ssh]=22

for SERVICE in "${!PORTS[@]}"; do
    echo "$SERVICE → ${PORTS[$SERVICE]}"
done
```

### "${array[@]}" vs ${array[*]}

```bash
FILES=("file one.txt" "file two.txt" "file three.txt")

# "${array[@]}" — cada elemento es una palabra separada (CORRECTO):
for f in "${FILES[@]}"; do
    echo "Archivo: $f"
done
# Archivo: file one.txt
# Archivo: file two.txt
# Archivo: file three.txt

# ${array[*]} sin comillas — word splitting rompe todo:
for f in ${FILES[*]}; do
    echo "Archivo: $f"
done
# Archivo: file
# Archivo: one.txt
# Archivo: file
# Archivo: two.txt
# ... (6 iteraciones en lugar de 3)

# REGLA: siempre usar "${array[@]}" con comillas dobles
```

---

## while

```bash
# while — ejecutar mientras la condicion sea verdadera (exit 0):
COUNT=0
while [[ $COUNT -lt 5 ]]; do
    echo "Count: $COUNT"
    ((COUNT++))
done

# while con comando:
while pgrep -x "process_name" &>/dev/null; do
    echo "Esperando a que process_name termine..."
    sleep 2
done

# Loop infinito:
while true; do
    read -rp "Comando (q para salir): " CMD
    [[ "$CMD" == "q" ]] && break
    echo "Ejecutarias: $CMD"
done
```

---

## while read — Leer linea por linea

Esta es la forma **correcta** de procesar texto linea por linea:

```bash
# Leer linea por linea de un archivo:
while IFS= read -r LINE; do
    echo "Linea: $LINE"
done < /etc/hostname

# Desglose:
# IFS=        → no hacer word splitting (preservar espacios al inicio/final)
# read -r     → no interpretar backslashes como escapes
# LINE        → variable donde se guarda cada linea
# < archivo   → redireccion de stdin
```

### Por que IFS= y -r

```bash
# Sin IFS=: se eliminan espacios al inicio y final
echo "  hello  " | while read LINE; do echo "|$LINE|"; done
# |hello|  (espacios eliminados)

echo "  hello  " | while IFS= read LINE; do echo "|$LINE|"; done
# |  hello  |  (espacios preservados)

# Sin -r: backslashes se interpretan como escapes
echo 'path\to\file' | while read LINE; do echo "$LINE"; done
# pathtofile  (\ se interpreto como escape)

echo 'path\to\file' | while read -r LINE; do echo "$LINE"; done
# path\to\file  (literal)
```

### Separar en campos

```bash
# IFS=: para separar por dos puntos (como /etc/passwd):
while IFS=: read -r USER _ UID GID _ HOME SHELL; do
    if [[ $UID -ge 1000 && $SHELL != */nologin ]]; then
        echo "$USER (UID=$UID) → $HOME [$SHELL]"
    fi
done < /etc/passwd
# _ es una convencion para campos que se descartan

# IFS=, para CSV simple:
while IFS=, read -r NAME AGE CITY; do
    echo "$NAME tiene $AGE anos y vive en $CITY"
done << 'EOF'
Alice,30,Madrid
Bob,25,Barcelona
Charlie,35,Valencia
EOF
```

### while read con pipes vs redireccion

```bash
# CUIDADO: pipe crea un subshell — las variables NO persisten:
COUNT=0
cat /etc/passwd | while IFS= read -r LINE; do
    ((COUNT++))
done
echo "Lineas: $COUNT"   # 0! — COUNT se modifico en el subshell

# CORRECTO: usar redireccion (no crea subshell):
COUNT=0
while IFS= read -r LINE; do
    ((COUNT++))
done < /etc/passwd
echo "Lineas: $COUNT"   # valor correcto

# ALTERNATIVA: lastpipe (bash 4.2+, solo en scripts, no interactive)
shopt -s lastpipe
COUNT=0
cat /etc/passwd | while IFS= read -r LINE; do
    ((COUNT++))
done
echo "Lineas: $COUNT"   # funciona con lastpipe
```

### Leer desde un comando

```bash
# Redirigir la salida de un comando al while:
while IFS= read -r LINE; do
    echo "Proceso: $LINE"
done < <(ps aux --no-headers)
# <() es process substitution — ver mas abajo

# Procesar archivos con espacios (find + read):
while IFS= read -r -d '' FILE; do
    echo "Archivo: $FILE"
done < <(find /tmp -name "*.log" -print0)
# -print0 y -d '' usan null como delimitador en lugar de newline
# Esto es SEGURO con archivos que tienen espacios, newlines, etc.
```

### Manejar archivos sin newline final

```bash
# Un archivo sin newline al final pierde la ultima linea con read simple:
while IFS= read -r LINE || [[ -n "$LINE" ]]; do
    echo "Linea: $LINE"
done < /etc/hostname
# || [[ -n "$LINE" ]] captura la ultima linea aunque no termine en newline
```

---

## until

```bash
# until — ejecutar mientras la condicion sea FALSA (exit != 0):
# (inverso de while)

COUNT=0
until [[ $COUNT -ge 5 ]]; do
    echo "Count: $COUNT"
    ((COUNT++))
done

# Esperar a que un servicio este disponible:
until curl -sf http://localhost:8080/health &>/dev/null; do
    echo "Esperando servicio..."
    sleep 2
done
echo "Servicio disponible"
```

---

## break y continue

```bash
# break — salir del loop:
for i in {1..100}; do
    if [[ $i -eq 5 ]]; then
        break
    fi
    echo "$i"
done
# 1, 2, 3, 4

# continue — saltar a la siguiente iteracion:
for i in {1..5}; do
    if [[ $i -eq 3 ]]; then
        continue
    fi
    echo "$i"
done
# 1, 2, 4, 5

# break N — salir de N niveles de loops anidados:
for i in {1..3}; do
    for j in {1..3}; do
        if [[ $j -eq 2 ]]; then
            break 2    # sale de AMBOS loops
        fi
        echo "$i.$j"
    done
done
# 1.1 (solo una iteracion)

# continue N — saltar al siguiente ciclo del loop N niveles arriba:
for i in {1..3}; do
    for j in {1..3}; do
        (( j == 2 )) && continue 2    # salta al siguiente i
        echo "i=$i j=$j"
    done
done
# i=1 j=1, i=2 j=1, i=3 j=1
```

---

## Process substitution <( ) y >( )

Process substitution permite usar la salida de un comando como si fuera un
archivo:

```bash
# <(command) — la salida del comando se presenta como un archivo:
diff <(sort file1.txt) <(sort file2.txt)
# diff necesita dos archivos — <() "simula" archivos temporales
# Internamente, bash crea un /dev/fd/N o un named pipe

# Verificar que es:
echo <(echo hello)
# /dev/fd/63  (o similar)
```

### Casos de uso practicos

```bash
# Comparar salida de dos comandos:
diff <(ssh server1 'cat /etc/config') <(ssh server2 'cat /etc/config')

# Leer de un comando sin crear subshell (vs pipe):
while IFS= read -r LINE; do
    # Este while NO esta en un subshell
    # Las variables modificadas aqui persisten
    echo "$LINE"
done < <(find . -name "*.sh")

# comm requiere archivos ordenados:
comm <(sort list1.txt) <(sort list2.txt)

# paste para combinar columnas:
paste <(cut -d: -f1 /etc/passwd) <(cut -d: -f7 /etc/passwd)
```

### >( ) — Process substitution de escritura

```bash
# >() permite escribir a un comando como si fuera un archivo:

# Enviar salida a multiples destinos:
echo "log message" | tee >(logger -t myapp) > output.log

# Comprimir mientras se copia:
tar cf >(gzip > backup.tar.gz) /important/data
```

---

## Patrones avanzados

### Procesar archivos de configuracion

```bash
# Leer un archivo .env ignorando comentarios y lineas vacias:
while IFS= read -r LINE; do
    # Ignorar comentarios y lineas vacias
    [[ -z "$LINE" || "$LINE" == \#* ]] && continue

    # Exportar la variable
    export "$LINE"
done < .env
```

### Retry con backoff exponencial

```bash
MAX_RETRIES=5
RETRY=0

until curl -sf http://localhost:8080/health &>/dev/null; do
    ((RETRY++))
    if [[ $RETRY -ge $MAX_RETRIES ]]; then
        echo "Error: servicio no disponible despues de $MAX_RETRIES intentos" >&2
        exit 1
    fi
    WAIT=$((2 ** RETRY))   # exponential backoff: 2, 4, 8, 16, 32
    echo "Intento $RETRY/$MAX_RETRIES fallido, esperando ${WAIT}s..."
    sleep "$WAIT"
done
```

### Iterar archivos de forma segura

```bash
# INCORRECTO: for con command substitution — falla con espacios
for f in $(find . -name "*.txt"); do
    echo "$f"   # "mi documento.txt" se parte en dos iteraciones
done

# CORRECTO: find con -print0 y while read -d ''
while IFS= read -r -d '' f; do
    echo "$f"
done < <(find . -name "*.txt" -print0)

# CORRECTO: glob (si no necesitas recursion):
for f in *.txt; do
    [[ -e "$f" ]] || continue   # si no hay matches, el glob queda literal
    echo "$f"
done

# CORRECTO: globstar para recursion (bash 4+):
shopt -s globstar
for f in **/*.txt; do
    echo "$f"
done
```

---

## Labs

### Parte 1 — Variantes de for

#### Paso 1.1: for clasico con listas

```bash
docker compose exec debian-dev bash -c '
echo "=== for con listas ==="
echo ""
echo "--- Lista literal ---"
for fruit in apple banana cherry; do
    echo "  Fruta: $fruit"
done
echo ""
echo "--- Glob ---"
for f in /etc/*.conf; do
    [[ -f "$f" ]] && echo "  Config: $(basename "$f")"
done | head -5
echo "  ..."
echo ""
echo "--- Salida de comando ---"
for user in $(cut -d: -f1 /etc/passwd | head -5); do
    echo "  Usuario: $user"
done
echo ""
echo "CUIDADO: for con \$(comando) rompe lineas con espacios"
echo "Para lineas con espacios, usar while read"
'
```

#### Paso 1.2: Brace expansion

```bash
docker compose exec debian-dev bash -c '
echo "=== Brace expansion ==="
echo ""
echo "--- Secuencia numerica ---"
echo "Numeros 1 a 5:"
for i in {1..5}; do echo -n "  $i"; done
echo ""
echo ""
echo "--- Con paso ---"
echo "Impares 1 a 10:"
for i in {1..10..2}; do echo -n "  $i"; done
echo ""
echo ""
echo "--- Cuenta regresiva ---"
for i in {5..1}; do echo -n "  $i"; done
echo " ... lanzamiento!"
echo ""
echo "--- Letras ---"
for c in {a..z}; do echo -n "$c "; done
echo ""
echo ""
echo "--- Combinar ---"
for f in file{1..3}.{txt,log}; do echo "  $f"; done
echo ""
echo "NOTA: brace expansion ocurre ANTES de variable expansion"
echo "  N=5; {1..\$N}  NO funciona (es literal)"
echo "  Usar C-style for o seq para rangos dinamicos"
'
```

#### Paso 1.3: C-style for

```bash
docker compose exec debian-dev bash -c '
echo "=== C-style for (( )) ==="
echo ""
echo "--- Rango dinamico ---"
N=5
for ((i = 1; i <= N; i++)); do
    echo -n "  $i"
done
echo ""
echo ""
echo "--- Cuenta regresiva ---"
for ((i = 3; i > 0; i--)); do
    echo "  T-$i"
done
echo "  Despegue!"
echo ""
echo "--- Paso variable ---"
for ((i = 0; i < 20; i += 5)); do
    echo -n "  $i"
done
echo ""
echo ""
echo "--- Multiples variables ---"
for ((i = 0, j = 10; i < j; i++, j--)); do
    echo "  i=$i j=$j"
done
echo ""
echo "C-style for es la unica forma de usar variables en el rango"
'
```

#### Paso 1.4: Iterar arrays

```bash
docker compose exec debian-dev bash -c '
echo "=== Iterar arrays ==="
echo ""
SERVERS=("web-01" "web-02" "db-01" "cache-01")
echo "--- Por valor ---"
for server in "${SERVERS[@]}"; do
    echo "  Server: $server"
done
echo ""
echo "--- Por indice ---"
for i in "${!SERVERS[@]}"; do
    echo "  [$i] = ${SERVERS[$i]}"
done
echo ""
echo "--- Con espacios en elementos ---"
FILES=("mi archivo.txt" "otro archivo.log" "datos finales.csv")
echo "Correcto (con comillas):"
for f in "${FILES[@]}"; do
    echo "  \"$f\""
done
echo ""
echo "Incorrecto (sin comillas):"
for f in ${FILES[@]}; do
    echo "  \"$f\""
done
echo "(los archivos con espacios se rompen)"
'
```

### Parte 2 — while read y process substitution

#### Paso 2.1: while read basico

```bash
docker compose exec debian-dev bash -c '
echo "=== while read ==="
echo ""
echo "--- Leer /etc/passwd linea por linea ---"
COUNT=0
while IFS=: read -r user _ uid gid _ home shell; do
    (( uid >= 1000 )) || continue
    echo "  $user (UID $uid): $home → $shell"
    ((COUNT++))
done < /etc/passwd
echo "  Usuarios con UID >= 1000: $COUNT"
echo ""
echo "--- Desglose ---"
echo "  IFS=:     → separar campos por :"
echo "  read -r   → no interpretar backslashes"
echo "  _         → descartar campos que no interesan"
echo "  < archivo → redirigir archivo como stdin del while"
echo ""
echo "--- Leer con espacios preservados ---"
echo "  IFS= read -r line  → lee la linea COMPLETA"
echo "  Sin IFS=, se eliminan espacios al inicio y final"
'
```

#### Paso 2.2: El problema del pipeline subshell

Antes de ejecutar, predice: despues de `echo "hello" | read VAR`,
que valor tiene $VAR en el shell actual?

```bash
docker compose exec debian-dev bash -c '
echo "=== Pipeline subshell problem ==="
echo ""
echo "--- El bug ---"
COUNT=0
echo -e "line1\nline2\nline3" | while read -r line; do
    ((COUNT++))
done
echo "COUNT despues del pipe: $COUNT (0! el while corrio en subshell)"
echo ""
echo "El pipe (|) crea un subshell para el lado derecho"
echo "Las variables modificadas en el subshell NO persisten"
echo ""
echo "--- Solucion 1: process substitution ---"
COUNT=0
while read -r line; do
    ((COUNT++))
done < <(echo -e "line1\nline2\nline3")
echo "COUNT con < <(): $COUNT (3, correcto)"
echo ""
echo "--- Solucion 2: here string ---"
COUNT=0
while read -r line; do
    ((COUNT++))
done <<< "$(echo -e "line1\nline2\nline3")"
echo "COUNT con <<<: $COUNT (3, correcto)"
echo ""
echo "< <(cmd) redirige la salida de cmd como un archivo"
echo "El while corre en el shell actual, no en subshell"
'
```

#### Paso 2.3: Process substitution <() y >()

```bash
docker compose exec debian-dev bash -c '
echo "=== Process substitution ==="
echo ""
echo "--- <() crea un pseudo-archivo con la salida del comando ---"
echo ""
echo "--- Comparar usuarios en dos archivos ---"
echo -e "root\nalice\nbob" > /tmp/users-old.txt
echo -e "root\nbob\ncharlie" > /tmp/users-new.txt
echo "Usuarios antiguos: root alice bob"
echo "Usuarios nuevos:   root bob charlie"
echo ""
echo "--- diff con process substitution ---"
diff <(sort /tmp/users-old.txt) <(sort /tmp/users-new.txt) || true
echo ""
echo "--- comm: encontrar diferencias ---"
echo "Solo en old (eliminados):"
comm -23 <(sort /tmp/users-old.txt) <(sort /tmp/users-new.txt)
echo "Solo en new (agregados):"
comm -13 <(sort /tmp/users-old.txt) <(sort /tmp/users-new.txt)
echo "En ambos:"
comm -12 <(sort /tmp/users-old.txt) <(sort /tmp/users-new.txt)
echo ""
rm -f /tmp/users-old.txt /tmp/users-new.txt
echo ""
echo "<() crea /dev/fd/N que se comporta como un archivo"
echo "Util para comandos que esperan archivos, no pipes"
'
```

#### Paso 2.4: until

```bash
docker compose exec debian-dev bash -c '
echo "=== until ==="
echo ""
echo "until es el inverso de while:"
echo "  while: repite MIENTRAS la condicion sea true"
echo "  until: repite HASTA QUE la condicion sea true"
echo ""
echo "--- Cuenta regresiva ---"
N=5
until (( N == 0 )); do
    echo -n "  $N"
    ((N--))
done
echo "  ... despegue!"
echo ""
echo "--- Uso tipico: esperar un recurso ---"
echo "  until curl -sf http://host/health; do"
echo "    sleep 2"
echo "  done"
'
```

### Parte 3 — Control de flujo avanzado

#### Paso 3.1: break y continue con nivel N

```bash
docker compose exec debian-dev bash -c '
echo "=== break N y continue N ==="
echo ""
echo "--- break (sale del loop actual) ---"
for i in {1..5}; do
    (( i == 3 )) && break
    echo "  i=$i"
done
echo "  (salio en i=3)"
echo ""
echo "--- break 2 (sale de 2 niveles de loops) ---"
for i in {1..3}; do
    for j in {1..3}; do
        (( i == 2 && j == 2 )) && break 2
        echo "  i=$i j=$j"
    done
done
echo "  (salio de ambos loops en i=2,j=2)"
echo ""
echo "--- continue (salta a la siguiente iteracion) ---"
for i in {1..5}; do
    (( i == 3 )) && continue
    echo -n "  $i"
done
echo ""
echo "  (salto i=3)"
echo ""
echo "--- continue 2 ---"
for i in {1..3}; do
    for j in {1..3}; do
        (( j == 2 )) && continue 2
        echo "  i=$i j=$j"
    done
done
echo "  (cuando j=2, salta al siguiente i)"
'
```

#### Paso 3.2: find -print0 con read -d ''

```bash
docker compose exec debian-dev bash -c '
echo "=== find -print0 + read -d (nombres con espacios) ==="
echo ""
echo "--- Crear archivos con nombres problematicos ---"
mkdir -p /tmp/lab-find
touch "/tmp/lab-find/normal.txt"
touch "/tmp/lab-find/con espacio.txt"
touch "/tmp/lab-find/con
newline.txt"
echo ""
echo "--- INCORRECTO: for con \$(find) ---"
echo "for f in \$(find ...); rompe con espacios y newlines:"
for f in $(find /tmp/lab-find -type f); do
    echo "  [$f]"
done
echo ""
echo "--- CORRECTO: find -print0 + while read -d ---"
while IFS= read -r -d "" f; do
    echo "  [$(basename "$f")]"
done < <(find /tmp/lab-find -type f -print0)
echo ""
echo "-print0: separa archivos con null en vez de newline"
echo "read -d: usa null como delimitador"
echo "IFS=: preserva espacios"
echo "-r: no interpreta backslashes"
echo ""
rm -rf /tmp/lab-find
'
```

#### Paso 3.3: Globstar

```bash
docker compose exec debian-dev bash -c '
echo "=== Globstar (**) ==="
echo ""
mkdir -p /tmp/lab-glob/a/b/c
touch /tmp/lab-glob/root.txt
touch /tmp/lab-glob/a/mid.txt
touch /tmp/lab-glob/a/b/deep.txt
touch /tmp/lab-glob/a/b/c/deepest.txt
echo ""
echo "Sin globstar:"
shopt -u globstar 2>/dev/null || true
for f in /tmp/lab-glob/*.txt; do
    echo "  $f"
done
echo "  (solo archivos en el nivel raiz)"
echo ""
echo "--- Con globstar: ** matchea recursivamente ---"
shopt -s globstar
for f in /tmp/lab-glob/**/*.txt; do
    echo "  $f"
done
echo "  (todos los .txt en cualquier nivel)"
echo ""
echo "shopt -s globstar  → activar"
echo "shopt -u globstar  → desactivar"
echo "Es especifico de bash 4+"
echo ""
rm -rf /tmp/lab-glob
'
```

---

## Ejercicios

### Ejercicio 1 — Brace expansion vs C-style

Predice la salida de cada caso:

```bash
# Caso A
for i in {2..10..2}; do echo -n "$i "; done
echo ""

# Caso B
N=10
for i in {2..$N..2}; do echo -n "$i "; done
echo ""

# Caso C
N=10
for ((i = 2; i <= N; i += 2)); do echo -n "$i "; done
echo ""
```

<details><summary>Prediccion</summary>

- A: `2 4 6 8 10` — brace expansion con paso 2, funciona.
- B: `{2..10..2}` (literal) — brace expansion ocurre ANTES de variable
  expansion. `$N` no se ha expandido cuando bash evalua la brace, asi que
  no reconoce `{2..$N..2}` como un rango valido.
- C: `2 4 6 8 10` — C-style for si funciona con variables. Es la alternativa
  correcta cuando el rango es dinamico.

</details>

### Ejercicio 2 — Arrays con espacios

Predice cuantas iteraciones hay en cada caso:

```bash
FILES=("mi archivo.txt" "otro doc.log" "datos.csv")

# Caso A
for f in "${FILES[@]}"; do echo "A: [$f]"; done

# Caso B
for f in ${FILES[@]}; do echo "B: [$f]"; done

# Caso C
for f in "${FILES[*]}"; do echo "C: [$f]"; done
```

<details><summary>Prediccion</summary>

Caso A: **3 iteraciones** (correcto)
```
A: [mi archivo.txt]
A: [otro doc.log]
A: [datos.csv]
```
`"${FILES[@]}"` preserva cada elemento como una palabra separada.

Caso B: **6 iteraciones** (roto)
```
B: [mi]
B: [archivo.txt]
B: [otro]
B: [doc.log]
B: [datos.csv]
```
Sin comillas, word splitting parte los elementos con espacios. `datos.csv`
no se rompe porque no tiene espacios.

Caso C: **1 iteracion** (probablemente no deseado)
```
C: [mi archivo.txt otro doc.log datos.csv]
```
`"${FILES[*]}"` une todos los elementos en un solo string (separados por
el primer caracter de IFS, default espacio).

</details>

### Ejercicio 3 — Pipeline subshell

Predice el valor de COUNT en cada caso:

```bash
# Caso A
COUNT=0
echo -e "a\nb\nc" | while read -r line; do ((COUNT++)); done
echo "A: $COUNT"

# Caso B
COUNT=0
while read -r line; do ((COUNT++)); done < <(echo -e "a\nb\nc")
echo "B: $COUNT"

# Caso C
COUNT=0
while read -r line; do ((COUNT++)); done <<< "$(echo -e "a\nb\nc")"
echo "C: $COUNT"
```

<details><summary>Prediccion</summary>

- A: `0` — el pipe crea un subshell para el while. COUNT se incrementa en
  el subshell pero ese subshell termina, y el COUNT del shell padre sigue
  siendo 0.

- B: `3` — process substitution `< <(cmd)` redirige la salida como un
  archivo. El while corre en el shell actual, no en un subshell.

- C: `3` — here string `<<<` tambien mantiene el while en el shell actual.

Para que variables persistan despues de un while read, usar redireccion
(`< archivo` o `< <(cmd)`) en vez de pipe (`cmd | while`).

</details>

### Ejercicio 4 — while read con IFS

Predice la salida:

```bash
while IFS=: read -r user _ uid gid _ home shell; do
    (( uid == 0 )) && echo "$user: $home ($shell)"
done < /etc/passwd
```

<details><summary>Prediccion</summary>

```
root: /root (/bin/bash)
```

- `IFS=:` separa cada linea por `:` (formato de /etc/passwd)
- `_` descarta campos (password, GECOS)
- `(( uid == 0 ))` filtra solo root (UID 0)
- Los campos de `/etc/passwd` son: user:pass:uid:gid:gecos:home:shell

Solo root tiene UID 0, asi que solo imprime una linea.

</details>

### Ejercicio 5 — break N en loops anidados

Predice la salida de cada caso:

```bash
# Caso A: break (sin numero)
for i in {1..3}; do
    for j in {1..3}; do
        (( j == 2 )) && break
        echo "A: i=$i j=$j"
    done
done

# Caso B: break 2
for i in {1..3}; do
    for j in {1..3}; do
        (( j == 2 )) && break 2
        echo "B: i=$i j=$j"
    done
done
```

<details><summary>Prediccion</summary>

Caso A (break sin numero = break 1, sale del loop interno):
```
A: i=1 j=1
A: i=2 j=1
A: i=3 j=1
```
Cuando j=2, sale del loop de j, pero el loop de i continua. Cada iteracion
de i solo ejecuta j=1 antes de que break salga del loop interno.

Caso B (break 2, sale de ambos loops):
```
B: i=1 j=1
```
En la primera iteracion de i, cuando j=2, break 2 sale de AMBOS loops.
Solo se imprime i=1 j=1.

</details>

### Ejercicio 6 — find -print0 vs $(find)

Predice cuantos elementos imprime cada metodo:

```bash
mkdir -p /tmp/test-find
touch "/tmp/test-find/normal.txt"
touch "/tmp/test-find/con espacio.txt"

# Metodo A
for f in $(find /tmp/test-find -name "*.txt"); do
    echo "A: [$f]"
done

# Metodo B
while IFS= read -r -d '' f; do
    echo "B: [$f]"
done < <(find /tmp/test-find -name "*.txt" -print0)

rm -rf /tmp/test-find
```

<details><summary>Prediccion</summary>

Metodo A: **3 lineas** (roto)
```
A: [/tmp/test-find/normal.txt]
A: [/tmp/test-find/con]
A: [espacio.txt]
```
`$(find ...)` captura la salida y word splitting la parte por espacios.
"con espacio.txt" se convierte en dos iteraciones.

Metodo B: **2 lineas** (correcto)
```
B: [/tmp/test-find/normal.txt]
B: [/tmp/test-find/con espacio.txt]
```
`-print0` separa por null (`\0`), `read -d ''` lee hasta null. Los espacios
en nombres se preservan.

</details>

### Ejercicio 7 — Process substitution con diff

Predice la salida:

```bash
diff <(echo -e "apple\nbanana\ncherry") <(echo -e "apple\nblueberry\ncherry")
```

<details><summary>Prediccion</summary>

```
2c2
< banana
---
> blueberry
```

- Linea 1 ("apple") es igual en ambos
- Linea 2 difiere: "banana" (izquierda) vs "blueberry" (derecha)
- Linea 3 ("cherry") es igual
- `2c2` = linea 2 cambiada

diff recibe dos "archivos" (pseudo-archivos via process substitution)
y muestra las diferencias. No se necesitan archivos temporales.

</details>

### Ejercicio 8 — Retry con until

Analiza este patron y predice que pasa si el servicio nunca responde:

```bash
MAX_RETRIES=3
RETRY=0

until curl -sf http://localhost:9999/health &>/dev/null; do
    ((RETRY++))
    if (( RETRY >= MAX_RETRIES )); then
        echo "Fallo despues de $MAX_RETRIES intentos" >&2
        exit 1
    fi
    WAIT=$((2 ** RETRY))
    echo "Intento $RETRY/$MAX_RETRIES, esperando ${WAIT}s..."
    sleep "$WAIT"
done
echo "Servicio OK"
```

<details><summary>Prediccion</summary>

Si el servicio nunca responde:
```
Intento 1/3, esperando 2s...
Intento 2/3, esperando 4s...
Intento 3/3, esperando 8s...
```
Wait, la logica es: RETRY se incrementa ANTES del check. Veamos:
- Iteracion 1: RETRY=1, 1 >= 3? No. WAIT=2. Sleep 2.
- Iteracion 2: RETRY=2, 2 >= 3? No. WAIT=4. Sleep 4.
- Iteracion 3: RETRY=3, 3 >= 3? Si. Imprime "Fallo", exit 1.

Salida:
```
Intento 1/3, esperando 2s...
Intento 2/3, esperando 4s...
Fallo despues de 3 intentos
```
El tercer intento detecta que RETRY >= MAX_RETRIES y sale antes de esperar.
Total de espera: 6 segundos (2 + 4). Backoff exponencial: 2^1, 2^2.

</details>

### Ejercicio 9 — Globstar

Predice que imprime con y sin globstar:

```bash
mkdir -p /tmp/gs/a/b
touch /tmp/gs/top.sh /tmp/gs/a/mid.sh /tmp/gs/a/b/deep.sh

# Sin globstar
shopt -u globstar
echo "Sin globstar:"
for f in /tmp/gs/**/*.sh; do echo "  $f"; done

# Con globstar
shopt -s globstar
echo "Con globstar:"
for f in /tmp/gs/**/*.sh; do echo "  $f"; done

rm -rf /tmp/gs
```

<details><summary>Prediccion</summary>

Sin globstar:
```
Sin globstar:
  /tmp/gs/a/mid.sh
```
Sin globstar, `**` se comporta como `*` (un solo nivel). `/tmp/gs/*/*.sh`
matchea solo archivos .sh dentro de subdirectorios directos de /tmp/gs.

Con globstar:
```
Con globstar:
  /tmp/gs/a/mid.sh
  /tmp/gs/a/b/deep.sh
```
Con globstar, `**` matchea recursivamente a cualquier profundidad.
`top.sh` no aparece porque `**/*.sh` requiere al menos un subdirectorio
(el `**` matchea paths, y `/*.sh` busca archivos en esos paths).

Nota: para incluir `top.sh`, usar `/tmp/gs/**/*.sh /tmp/gs/*.sh` o
el patron `/tmp/gs/{,**/}*.sh`.

</details>

### Ejercicio 10 — Panorama de loops

Analiza este script y predice la salida:

```bash
#!/usr/bin/env bash
set -euo pipefail

# Simular datos
DATA="name:age:city
Alice:30:Madrid
Bob:25:Barcelona
#Charlie:35:Valencia
Eve:28:Sevilla"

COUNT=0
TOTAL_AGE=0

while IFS=: read -r name age city; do
    # Saltar header y comentarios
    [[ "$name" == "name" || "$name" == \#* ]] && continue

    ((COUNT++))
    ((TOTAL_AGE += age))
    echo "$COUNT. $name ($age) - $city"
done <<< "$DATA"

echo "---"
echo "Total: $COUNT personas"
(( COUNT > 0 )) && echo "Edad promedio: $((TOTAL_AGE / COUNT))"
```

<details><summary>Prediccion</summary>

```
1. Alice (30) - Madrid
2. Bob (25) - Barcelona
3. Eve (28) - Sevilla
---
Total: 3 personas
Edad promedio: 27
```

- "name:age:city" se salta (header: `name == "name"`)
- "#Charlie:35:Valencia" se salta (`name == "#Charlie"`, matchea `\#*`)
- Alice, Bob, Eve se procesan (COUNT=3, TOTAL_AGE=83)
- Edad promedio: 83 / 3 = 27 (aritmetica entera, trunca 27.66)

El here string `<<<` pasa la variable como input sin crear subshell,
asi que COUNT y TOTAL_AGE persisten despues del while.

</details>
