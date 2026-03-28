# T02 — Loops avanzados

## for clásico

```bash
# Iterar sobre una lista explícita:
for FRUIT in apple banana cherry; do
    echo "Fruta: $FRUIT"
done

# Iterar sobre la salida de un comando:
for USER in $(cut -d: -f1 /etc/passwd); do
    echo "Usuario: $USER"
done
# CUIDADO: esto falla con nombres que tienen espacios
# (ver "while read" más abajo para la forma segura)

# Iterar sobre archivos (glob):
for FILE in /etc/*.conf; do
    echo "Config: $FILE"
done
# El glob se expande antes del loop — seguro con espacios en nombres
```

### for con rangos (brace expansion)

```bash
# Rango numérico:
for i in {1..5}; do
    echo "Número: $i"
done
# 1, 2, 3, 4, 5

# Rango con paso (bash 4+):
for i in {0..20..5}; do
    echo "Número: $i"
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

# Con variables, usar seq o for estilo C:
for i in $(seq 1 "$N"); do
    echo "$i"
done
```

### for estilo C

```bash
# Sintaxis idéntica a C/Java:
for ((i = 0; i < 10; i++)); do
    echo "i = $i"
done

# Con paso:
for ((i = 0; i <= 100; i += 10)); do
    echo "i = $i"
done

# Múltiples variables:
for ((i = 0, j = 10; i < j; i++, j--)); do
    echo "i=$i j=$j"
done
# i=0 j=10, i=1 j=9, i=2 j=8, i=3 j=7, i=4 j=6

# Loop infinito:
for ((;;)); do
    echo "infinito"
    break   # sin esto, nunca termina
done

# El for estilo C es la forma correcta de iterar con variables numéricas
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

# Iterar índices:
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
# while — ejecutar mientras la condición sea verdadera (exit 0):
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
    eval "$CMD" 2>&1 || true
done
```

---

## while read — Leer línea por línea

Esta es la forma **correcta** de procesar texto línea por línea:

```bash
# Leer línea por línea de un archivo:
while IFS= read -r LINE; do
    echo "Línea: $LINE"
done < /etc/hostname

# Desglose:
# IFS=        → no hacer word splitting (preservar espacios al inicio/final)
# read -r     → no interpretar backslashes como escapes
# LINE        → variable donde se guarda cada línea
# < archivo   → redirección de stdin
```

### Por qué IFS= y -r

```bash
# Sin IFS=: se eliminan espacios al inicio y final
echo "  hello  " | while read LINE; do echo "|$LINE|"; done
# |hello|  (espacios eliminados)

echo "  hello  " | while IFS= read LINE; do echo "|$LINE|"; done
# |  hello  |  (espacios preservados)

# Sin -r: backslashes se interpretan como escapes
echo 'path\to\file' | while read LINE; do echo "$LINE"; done
# pathtofile  (\ se interpretó como escape)

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
# _ es una convención para campos que se descartan

# IFS=, para CSV simple:
while IFS=, read -r NAME AGE CITY; do
    echo "$NAME tiene $AGE años y vive en $CITY"
done << 'EOF'
Alice,30,Madrid
Bob,25,Barcelona
Charlie,35,Valencia
EOF
```

### while read con pipes vs redirección

```bash
# CUIDADO: pipe crea un subshell — las variables NO persisten:
COUNT=0
cat /etc/passwd | while IFS= read -r LINE; do
    ((COUNT++))
done
echo "Líneas: $COUNT"   # ¡0! — COUNT se modificó en el subshell

# CORRECTO: usar redirección (no crea subshell):
COUNT=0
while IFS= read -r LINE; do
    ((COUNT++))
done < /etc/passwd
echo "Líneas: $COUNT"   # valor correcto

# ALTERNATIVA: lastpipe (bash 4.2+)
shopt -s lastpipe
COUNT=0
cat /etc/passwd | while IFS= read -r LINE; do
    ((COUNT++))
done
echo "Líneas: $COUNT"   # funciona con lastpipe
```

### Leer desde un comando

```bash
# Redirigir la salida de un comando al while:
while IFS= read -r LINE; do
    echo "Proceso: $LINE"
done < <(ps aux --no-headers)
# <() es process substitution — ver más abajo

# Procesar archivos con espacios (find + read):
while IFS= read -r -d '' FILE; do
    echo "Archivo: $FILE"
done < <(find /tmp -name "*.log" -print0)
# -print0 y -d '' usan null como delimitador en lugar de newline
# Esto es SEGURO con archivos que tienen espacios, newlines, etc.
```

---

## until

```bash
# until — ejecutar mientras la condición sea FALSA (exit != 0):
# (inverso de while)

COUNT=0
until [[ $COUNT -ge 5 ]]; do
    echo "Count: $COUNT"
    ((COUNT++))
done

# Esperar a que un servicio esté disponible:
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

# continue — saltar a la siguiente iteración:
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
# 1.1 (solo una iteración)
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

# Verificar qué es:
echo <(echo hello)
# /dev/fd/63  (o similar)
```

### Casos de uso prácticos

```bash
# Comparar salida de dos comandos:
diff <(ssh server1 'cat /etc/config') <(ssh server2 'cat /etc/config')

# Leer de un comando sin crear subshell (vs pipe):
while IFS= read -r LINE; do
    # Este while NO está en un subshell
    # Las variables modificadas aquí persisten
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

# Enviar salida a múltiples destinos:
echo "log message" | tee >(logger -t myapp) >(mail -s "alert" admin@example.com)

# Comprimir mientras se copia:
tar cf >(gzip > backup.tar.gz) /important/data
```

---

## Patrones avanzados

### Procesar archivos de configuración

```bash
# Leer un archivo .env ignorando comentarios y líneas vacías:
while IFS= read -r LINE; do
    # Ignorar comentarios y líneas vacías
    [[ -z "$LINE" || "$LINE" == \#* ]] && continue

    # Exportar la variable
    export "$LINE"
done < .env
```

### Retry con backoff

```bash
MAX_RETRIES=5
RETRY=0

until curl -sf http://localhost:8080/health &>/dev/null; do
    ((RETRY++))
    if [[ $RETRY -ge $MAX_RETRIES ]]; then
        echo "Error: servicio no disponible después de $MAX_RETRIES intentos" >&2
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

# CORRECTO: glob (si no necesitas recursión):
for f in *.txt; do
    [[ -e "$f" ]] || continue   # si no hay matches, el glob queda literal
    echo "$f"
done

# CORRECTO: globstar para recursión (bash 4+):
shopt -s globstar
for f in **/*.txt; do
    echo "$f"
done
```

---

## Ejercicios

### Ejercicio 1 — for con rangos y arrays

```bash
# 1. Imprimir los números pares de 2 a 20:
for i in {2..20..2}; do echo "$i"; done

# 2. Crear un array con 5 nombres y recorrerlo con índices:
NAMES=(alice bob charlie dave eve)
for i in "${!NAMES[@]}"; do
    echo "[$i] ${NAMES[$i]}"
done
```

---

### Ejercicio 2 — while read

```bash
# Contar cuántos usuarios tienen /bin/bash como shell:
COUNT=0
while IFS=: read -r _ _ _ _ _ _ SHELL; do
    [[ "$SHELL" == "/bin/bash" ]] && ((COUNT++))
done < /etc/passwd
echo "Usuarios con bash: $COUNT"
```

---

### Ejercicio 3 — Process substitution

```bash
# Comparar líneas que difieren entre dos archivos:
diff <(echo -e "a\nb\nc\nd") <(echo -e "a\nc\nd\ne")

# Ver todos los archivos en /tmp y su tamaño total:
TOTAL=0
while IFS= read -r -d '' FILE; do
    SIZE=$(stat -c%s "$FILE" 2>/dev/null || echo 0)
    ((TOTAL += SIZE))
done < <(find /tmp -maxdepth 1 -type f -print0)
echo "Tamaño total: $TOTAL bytes"
```

---

### Ejercicio 4 — while con procesamiento condicional

```bash
# Extraer solo líneas no vacías y no comentadas de un archivo:
while IFS= read -r LINE; do
    [[ -z "$LINE" || "$LINE" =~ ^[[:space:]]*# ]] && continue
    echo "PROCESAR: $LINE"
done < /etc/fstab
```

---

### Ejercicio 5 — for estilo C con múltiples variables

```bash
# Fibonacci con for estilo C:
a=0
b=1
LIMIT=100

echo "Fibonacci hasta $LIMIT:"
echo -n "$a "
for ((i=0; b<=LIMIT; i++)); do
    echo -n "$b "
    ((tmp=a+b, a=b, b=tmp))
done
echo
```

---

### Ejercicio 6 — until para esperar algo

```bash
# Esperar a que un archivo aparezca:
FILE=/tmp/ready
until [[ -f "$FILE" ]]; do
    echo "Esperando archivo..."
    sleep 1
done
echo "Archivo encontrado"
```

---

### Ejercicio 7 — Iterar arrays asociativos

```bash
declare -A CAPITALS
CAPITALS[FR]="Paris"
CAPITALS[DE]="Berlin"
CAPITALS[IT]="Rome"
CAPITALS[ES]="Madrid"

# Iterar keys:
for country in "${!CAPITALS[@]}"; do
    echo "$country: ${CAPITALS[$country]}"
done | sort
```

---

### Ejercicio 8 — break con etiqueta

```bash
# Loop anidado con break etiquetado:
for i in {1..3}; do
    for j in {1..3}; do
        if [[ $j -eq 2 ]]; then
            break  # solo sale del loop interno
        fi
        echo "i=$i j=$j"
    done
done
echo "---"
for i in {1..3}; do
    for j in {1..3}; do
        if [[ $j -eq 2 ]]; then
            break 2  # sale de ambos loops
        fi
        echo "i=$i j=$j"
    done
done
```

---

### Ejercicio 9 — Manejo de EOF en while read

```bash
# Procesar archivo y detectar EOF:
while IFS= read -r LINE || [[ -n "$LINE" ]]; do
    echo "Línea: $LINE"
done < /etc/hostname
# El || [[ -n "$LINE" ]] maneja archivos sin newline al final
```

---

### Ejercicio 10 — Transformar datos con while read

```bash
# Convertir /etc/passwd a formato legible:
while IFS=: read -r USER PASS UID GID DESC HOME SHELL; do
    printf "%-10s UID:%-5s Home:%s\n" "$USER" "$UID" "$HOME"
done < /etc/passwd | head -10
```
