# AWK como lenguaje

## 1. AWK no es solo un comando

AWK es un **lenguaje de programación** completo, diseñado para procesar texto
estructurado. Tiene variables, arrays, funciones, control de flujo, y printf.
Donde sed opera línea por línea con regex, AWK opera **campo por campo** con
lógica programática.

```bash
# sed: regex sobre la línea completa
sed -n '/error/p' log.txt

# AWK: acceso a campos, condiciones, cálculos
awk '$3 > 500 { printf "%-15s %d\n", $1, $3 }' data.txt
```

```
AWK procesa cada línea como un "record" con "fields":

Línea:     "Alice  Engineering  75000"
            $1       $2          $3
            ↑        ↑           ↑
          campo 1  campo 2    campo 3
$0 = toda la línea
NF = número de campos (3)
NR = número de línea actual
```

---

## 2. Estructura de un programa AWK

```
awk 'pattern { action }' file
```

```bash
# pattern = cuándo ejecutar
# action  = qué hacer

awk '/error/ { print $0 }'           # pattern = regex
awk 'NR > 5 { print $1 }'            # pattern = condición
awk '$3 > 100 { sum += $3 }'         # pattern = comparación de campo
awk '{ print $1, $3 }'               # sin pattern = todas las líneas
awk '/start/,/end/ { print }'        # pattern = rango
```

### BEGIN y END

```bash
awk '
    BEGIN { print "=== Report ===" }     # antes de procesar cualquier línea
    { total += $3 }                      # para cada línea
    END { print "Total:", total }        # después de procesar todas
' data.txt
```

```
BEGIN:  se ejecuta UNA vez, antes del input
        → inicializar variables, imprimir headers

body:   se ejecuta para CADA línea
        → procesar datos

END:    se ejecuta UNA vez, después de todo el input
        → imprimir resúmenes, totales
```

---

## 3. Campos y separadores

### 3.1 Field separator (-F)

```bash
# Default: whitespace (espacios/tabs)
echo "Alice 30 Engineer" | awk '{ print $2 }'    # 30

# CSV: separador coma
echo "Alice,30,Engineer" | awk -F, '{ print $2 }'  # 30

# Archivo /etc/passwd (separador :)
awk -F: '{ print $1, $7 }' /etc/passwd   # usuario y shell

# Múltiples caracteres como separador
echo "a::b::c" | awk -F'::' '{ print $2 }'   # b

# Regex como separador
echo "a1b2c3d" | awk -F'[0-9]' '{ print $1, $3 }'  # a c
```

### 3.2 Output field separator (OFS)

```bash
# OFS controla qué se imprime entre campos en print
awk -F: -v OFS=',' '{ print $1, $3, $7 }' /etc/passwd
# root,0,/bin/bash
# daemon,1,/usr/sbin/nologin

# Setear en BEGIN
awk 'BEGIN { FS=":"; OFS="\t" } { print $1, $3 }' /etc/passwd
# root    0
# daemon  1
```

### 3.3 Variables de campo especiales

| Variable | Significado |
|----------|-----------|
| `$0` | Toda la línea |
| `$1`, `$2`... | Campo N |
| `$NF` | Último campo |
| `$(NF-1)` | Penúltimo campo |
| `NF` | Número de campos en la línea actual |
| `NR` | Número de record (línea) global |
| `FNR` | Número de record en el archivo actual |
| `FS` | Field separator (input) |
| `OFS` | Output field separator |
| `RS` | Record separator (default: newline) |
| `ORS` | Output record separator |
| `FILENAME` | Nombre del archivo actual |

```bash
# Último campo
echo "a b c d e" | awk '{ print $NF }'      # e

# Penúltimo
echo "a b c d e" | awk '{ print $(NF-1) }'  # d

# Número de campos
echo "a b c" | awk '{ print NF }'            # 3
```

---

## 4. Patterns (condiciones)

```bash
# Regex (match en $0)
awk '/error/' log.txt                    # líneas con "error"
awk '!/comment/' code.txt               # líneas SIN "comment"

# Regex en campo específico
awk '$2 ~ /^admin/' users.txt           # campo 2 empieza con "admin"
awk '$1 !~ /^#/' config.txt             # campo 1 no empieza con #

# Comparaciones
awk '$3 > 1000' data.txt                # campo 3 mayor que 1000
awk '$1 == "root"' /etc/passwd          # campo 1 es "root" (FS default)
awk 'NR >= 10 && NR <= 20' file.txt     # líneas 10-20
awk 'NF > 0' file.txt                   # líneas no vacías (tienen campos)
awk 'length($0) > 80' code.c            # líneas con más de 80 caracteres

# Rangos
awk '/START/,/END/' file.txt            # desde START hasta END (inclusive)
awk 'NR==5,NR==10' file.txt             # líneas 5-10
```

---

## 5. Variables y tipos

AWK tiene tipado dinámico. Las variables se crean al usarlas, inicializadas
a `""` (string) o `0` (número), según contexto:

```bash
awk '
    {
        count++           # inicia en 0, se incrementa
        total += $3       # acumula campo 3
        names = names " " $1   # concatenar strings
    }
    END {
        print "Count:", count
        print "Total:", total
        print "Avg:", total / count
        print "Names:", names
    }
' data.txt
```

### Pasar variables desde Bash

```bash
threshold=1000
awk -v limit="$threshold" '$3 > limit { print $1, $3 }' data.txt

# Múltiples variables
awk -v min=10 -v max=100 '$1 >= min && $1 <= max' numbers.txt

# Variable de entorno
awk -v user="$USER" '$1 == user { print }' users.txt
```

**Siempre usar `-v`** para pasar variables. No interpolar Bash directamente
en el programa AWK:

```bash
# MAL: inyección si $threshold contiene caracteres especiales
awk "\$3 > $threshold { print }" data.txt

# BIEN: -v es seguro
awk -v limit="$threshold" '$3 > limit { print }' data.txt
```

---

## 6. Arrays asociativos

AWK tiene arrays asociativos nativos (como `declare -A` en Bash, pero más
naturales):

```bash
# Contar ocurrencias
awk '{ count[$1]++ } END { for (k in count) print k, count[k] }' access.log

# Sumar por categoría
awk -F, '{ sum[$1] += $2 } END { for (cat in sum) print cat, sum[cat] }' sales.csv
```

### Operaciones con arrays

```bash
awk '
BEGIN {
    # Asignar
    color["red"] = "#FF0000"
    color["green"] = "#00FF00"
    color["blue"] = "#0000FF"

    # Iterar
    for (key in color) {
        print key, "=", color[key]
    }

    # Verificar existencia (sin crear la entrada)
    if ("red" in color) print "red exists"

    # Eliminar
    delete color["green"]

    # Longitud (GNU AWK)
    print "Count:", length(color)
}
'
```

### Frecuencia de palabras

```bash
# Contar cada palabra en un texto
awk '
{
    for (i = 1; i <= NF; i++) {
        word = tolower($i)
        gsub(/[^a-z]/, "", word)    # eliminar puntuación
        if (word != "") freq[word]++
    }
}
END {
    for (w in freq) printf "%6d %s\n", freq[w], w
}
' textfile.txt | sort -rn | head -20
```

### Arrays multidimensionales (simulados)

```bash
# AWK no tiene arrays 2D, pero usa SUBSEP para simular
awk '
{
    # grid[fila,columna] = valor
    grid[$1, $2] = $3
}
END {
    # Acceder
    print grid["A", "1"]
}
' <<'EOF'
A 1 100
A 2 200
B 1 300
B 2 400
EOF
```

---

## 7. Control de flujo

```bash
awk '{
    # if-else
    if ($3 > 1000)
        print $1, "HIGH"
    else if ($3 > 500)
        print $1, "MEDIUM"
    else
        print $1, "LOW"
}' data.txt

# for loop
awk '{ for (i = 1; i <= NF; i++) print NR ":" i, $i }' file.txt

# while loop
awk '{
    i = 1
    while (i <= NF) {
        if ($i ~ /error/) print "Found at field", i
        i++
    }
}' log.txt

# do-while
awk 'BEGIN {
    i = 1
    do {
        print i
        i *= 2
    } while (i <= 100)
}'
```

### next y exit

```bash
# next: saltar a la siguiente línea (como continue para líneas)
awk '/^#/ { next } { print }' config.txt    # skip comentarios

# exit: terminar inmediatamente (ejecuta END primero)
awk '{ print; if (NR >= 10) exit }' file.txt  # primeras 10 líneas
```

---

## 8. Funciones built-in

### 8.1 String

```bash
# length
awk '{ print length($0) }' file.txt       # longitud de cada línea

# substr(string, start, length)
echo "Hello World" | awk '{ print substr($0, 7) }'      # World
echo "Hello World" | awk '{ print substr($0, 1, 5) }'   # Hello

# index(string, target) — posición de primera ocurrencia
echo "Hello World" | awk '{ print index($0, "World") }'  # 7

# split(string, array, separator)
echo "a:b:c:d" | awk '{ n = split($0, parts, ":"); print parts[2], n }'  # b 4

# sub(regex, replacement, target) — primera ocurrencia
echo "foo bar foo" | awk '{ sub(/foo/, "FOO"); print }'   # FOO bar foo

# gsub(regex, replacement, target) — todas las ocurrencias
echo "foo bar foo" | awk '{ gsub(/foo/, "FOO"); print }'  # FOO bar FOO

# match(string, regex) — retorna posición, sets RSTART y RLENGTH
echo "error code 404" | awk '{ if (match($0, /[0-9]+/)) print substr($0, RSTART, RLENGTH) }'  # 404

# tolower / toupper
echo "Hello" | awk '{ print toupper($0) }'   # HELLO

# sprintf (formatea sin imprimir)
awk '{ s = sprintf("%05d", NR); print s, $0 }' file.txt
```

### 8.2 Matemáticas

```bash
awk 'BEGIN {
    print sin(3.14159/2)    # 1
    print cos(0)            # 1
    print exp(1)            # 2.71828
    print log(2.71828)      # ~1
    print sqrt(144)         # 12
    print int(3.7)          # 3 (truncar)
    print int(-3.7)         # -3

    # Random
    srand()                 # seed con tiempo actual
    print rand()            # 0 <= x < 1
    print int(rand() * 100) # 0-99
}'
```

---

## 9. Funciones definidas por el usuario

```bash
awk '
function max(a, b) {
    return a > b ? a : b
}

function min(a, b) {
    return a < b ? a : b
}

function clamp(val, lo, hi) {
    return max(lo, min(hi, val))
}

function human_size(bytes,    units, i) {
    # Variables locales: parámetros extra después de una separación
    split("B KB MB GB TB", units, " ")
    i = 1
    while (bytes >= 1024 && i < 5) {
        bytes /= 1024
        i++
    }
    return sprintf("%.1f %s", bytes, units[i])
}

{
    print $1, human_size($2)
}
' <<'EOF'
file1 1536
file2 2621440
file3 5368709120
EOF
# file1 1.5 KB
# file2 2.5 MB
# file3 5.0 GB
```

**Variables locales en AWK**: no hay `local`. La convención es declarar
variables locales como parámetros extra de la función, separados por
espacios:

```bash
function myfunc(param1, param2,    local1, local2) {
    #                              ↑ convención: espacios antes de locales
    local1 = "only exists here"
    local2 = param1 + param2
    return local2
}
```

---

## 10. Reportes tabulares

AWK brilla generando reportes formateados:

### 10.1 Tabla con headers y alineación

```bash
awk -F: '
BEGIN {
    printf "%-20s %6s %s\n", "USER", "UID", "SHELL"
    printf "%-20s %6s %s\n", "----", "---", "-----"
}
$3 >= 1000 && $7 !~ /nologin/ {
    printf "%-20s %6d %s\n", $1, $3, $7
}
END {
    printf "\n%d users found\n", count
}
' /etc/passwd
```

```
USER                    UID SHELL
----                    --- -----
alice                  1000 /bin/bash
bob                    1001 /bin/zsh
charlie                1002 /bin/bash

3 users found
```

### 10.2 Resumen de disco

```bash
df -h | awk '
NR == 1 { next }     # skip header de df
{
    gsub(/%/, "", $5)  # eliminar % de Use%
    use = $5 + 0       # convertir a número
    if (use > 80)
        status = "CRITICAL"
    else if (use > 60)
        status = "WARNING"
    else
        status = "OK"
    printf "%-30s %3d%% [%s]\n", $6, use, status
}
'
```

### 10.3 Procesar CSV y generar reporte

```bash
cat <<'EOF' | awk -F, '
NR == 1 { next }  # skip header
{
    dept = $2
    salary = $3
    dept_total[dept] += salary
    dept_count[dept]++
    grand_total += salary
}
END {
    printf "%-15s %8s %6s %8s\n", "Department", "Total", "Count", "Average"
    printf "%-15s %8s %6s %8s\n", "----------", "-----", "-----", "-------"
    for (d in dept_total) {
        avg = dept_total[d] / dept_count[d]
        printf "%-15s %8d %6d %8.0f\n", d, dept_total[d], dept_count[d], avg
    }
    printf "\n%-15s %8d\n", "GRAND TOTAL", grand_total
}
'
Name,Department,Salary
Alice,Engineering,85000
Bob,Engineering,92000
Charlie,Sales,65000
Diana,Engineering,88000
Eve,Sales,70000
Frank,Marketing,75000
EOF
```

---

## 11. AWK multi-archivo

```bash
# FNR vs NR
# NR  = record number global (continúa entre archivos)
# FNR = record number del archivo actual (se resetea)

awk '
FNR == 1 { print "--- " FILENAME " ---" }
{ print FNR": "$0 }
' file1.txt file2.txt
# --- file1.txt ---
# 1: first line of file1
# 2: second line of file1
# --- file2.txt ---
# 1: first line of file2
# 2: second line of file2
```

### Join de dos archivos

```bash
# Lookup: file1 tiene ID→nombre, file2 tiene ID→datos
# file1.txt: 101 Alice
# file2.txt: 101 85 90 78

awk '
NR == FNR { name[$1] = $2; next }     # primer archivo: cargar lookup
{ print name[$1], $2, $3, $4 }         # segundo archivo: usar lookup
' file1.txt file2.txt
# Alice 85 90 78
```

```
Truco NR == FNR:
  NR  = record global (nunca se resetea)
  FNR = record del archivo actual (se resetea para cada archivo)

  NR == FNR es true SOLO para el primer archivo
  (porque para el segundo, NR > FNR)

  Primer archivo:  NR=1 FNR=1 → true → cargar
                   NR=2 FNR=2 → true → cargar
  Segundo archivo: NR=3 FNR=1 → false → procesar
                   NR=4 FNR=2 → false → procesar
```

---

## 12. AWK one-liners esenciales

```bash
# Sumar una columna
awk '{ sum += $1 } END { print sum }' numbers.txt

# Promedio
awk '{ sum += $1; n++ } END { print sum/n }' numbers.txt

# Max y min
awk 'NR==1 || $1>max { max=$1 } NR==1 || $1<min { min=$1 } END { print min, max }' numbers.txt

# Contar líneas (como wc -l)
awk 'END { print NR }' file.txt

# Imprimir líneas únicas (como uniq, pero no necesita sort)
awk '!seen[$0]++' file.txt

# Transponer columnas a filas
awk '{ for (i=1; i<=NF; i++) col[i] = col[i] " " $i } END { for (i=1; i<=NF; i++) print col[i] }' file.txt

# Eliminar duplicados preservando orden
awk '!seen[$0]++' file.txt

# Imprimir entre dos patrones (sin los patrones)
awk '/START/{f=1; next} /END/{f=0} f' file.txt

# Reverse fields
awk '{ for (i=NF; i>=1; i--) printf "%s ", $i; print "" }' file.txt

# Histograma de longitud de líneas
awk '{ len[int(length/10)*10]++ } END { for (l in len) printf "%3d-%3d: %d\n", l, l+9, len[l] }' file.txt | sort -n
```

### El truco de `!seen[$0]++`

```bash
awk '!seen[$0]++'
```

```
Desglose paso a paso:
  seen[$0]     → acceder array con la línea como clave
                  (primer acceso: 0, segundo acceso: 1, etc.)
  seen[$0]++   → post-incremento: retorna valor ANTES de incrementar
                  primera vez: retorna 0 (luego se vuelve 1)
                  segunda vez: retorna 1 (luego se vuelve 2)
  !seen[$0]++  → negar:
                  primera vez: !0 = true → imprimir
                  segunda vez: !1 = false → no imprimir

Resultado: cada línea se imprime solo la primera vez que aparece.
```

---

## 13. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| `awk { print $1 }` sin comillas | Bash interpreta `{` y `$1` | `awk '{ print $1 }'` con comillas simples |
| Interpolar `$variable` Bash en AWK | Bash expande `$1` antes de que AWK lo vea | `-v var="$bash_var"` |
| `$0 = $0` no reconstruye con OFS | Necesitas tocar un campo | `$1 = $1` fuerza reconstrucción |
| Arrays sin `for (k in arr)` | `for (i=0; i<length; i++)` no funciona | AWK arrays no son indexados secuencialmente |
| `==` para regex | `$1 == /pattern/` no funciona | `$1 ~ /pattern/` |
| Variables locales accidentalmente globales | AWK no tiene `local` | Parámetros extra en funciones |
| Olvidar `next` en multi-archivo | Segundo archivo ejecuta reglas del primero | `NR==FNR { ...; next }` |
| print sin paréntesis con `>` | `print $1 > "file"` redirige (correcto), `print $1 > $2` compara | Paréntesis: `print ($1 > $2)` |
| Olvidar `-F` para CSV | Campos se splitean por espacio | `awk -F,` |
| `getline` mal usado | Comportamiento confuso, difícil de debuggear | Preferir `NR==FNR` pattern |

---

## 14. Ejercicios

### Ejercicio 1 — Campos básicos

**Predicción**: ¿qué imprime?

```bash
echo "Alice 30 Engineer 85000" | awk '{ print $1, $NF }'
echo "a:b:c:d:e" | awk -F: '{ print $2, $(NF-1) }'
echo "hello world" | awk '{ print NF, length($0) }'
```

<details>
<summary>Respuesta</summary>

```
Alice 85000
b d
2 11
```

- `$1` = "Alice", `$NF` = último campo = "85000" (NF=4)
- `-F:` separa por `:`. `$2` = "b", `$(NF-1)` = penúltimo = "d" (NF=5)
- `NF` = 2 campos ("hello" y "world"), `length($0)` = 11 caracteres
</details>

---

### Ejercicio 2 — BEGIN y END

**Predicción**: ¿qué imprime?

```bash
echo -e "10\n20\n30" | awk '
    BEGIN { print "Start"; sum = 0 }
    { sum += $1; print "Line:", $1 }
    END { print "Total:", sum }
'
```

<details>
<summary>Respuesta</summary>

```
Start
Line: 10
Line: 20
Line: 30
Total: 60
```

- BEGIN: imprime "Start" antes de leer cualquier línea
- Body: para cada línea, acumula y imprime
- END: imprime el total después de todas las líneas

BEGIN se ejecuta incluso sin input. END se ejecuta después de que todo el
input se haya procesado.
</details>

---

### Ejercicio 3 — Condiciones

**Predicción**: ¿cuántas líneas imprime?

```bash
cat <<'EOF' | awk '$3 > 70000 && $2 != "Sales"'
Alice Engineering 85000
Bob Sales 65000
Charlie Engineering 92000
Diana Marketing 75000
Eve Sales 70000
EOF
```

<details>
<summary>Respuesta</summary>

Imprime **3 líneas**:

```
Alice Engineering 85000
Charlie Engineering 92000
Diana Marketing 75000
```

- Alice: 85000 > 70000 Y Engineering != Sales → ✓
- Bob: 65000 > 70000 → ✗ (falla primera condición)
- Charlie: 92000 > 70000 Y Engineering != Sales → ✓
- Diana: 75000 > 70000 Y Marketing != Sales → ✓
- Eve: 70000 > 70000 → ✗ (no es estrictamente mayor)

Cuando no hay `{ action }`, AWK usa la acción por defecto: `{ print $0 }`.
</details>

---

### Ejercicio 4 — Array asociativo

**Predicción**: ¿qué imprime?

```bash
echo -e "a\nb\na\nc\nb\na" | awk '
    { count[$1]++ }
    END {
        for (k in count)
            print k, count[k]
    }
'
```

<details>
<summary>Respuesta</summary>

```
a 3
b 2
c 1
```

(El orden de las líneas puede variar — `for (k in count)` no garantiza orden.)

- "a" aparece 3 veces → count["a"] = 3
- "b" aparece 2 veces → count["b"] = 2
- "c" aparece 1 vez → count["c"] = 1

Para orden garantizado: pipe a `sort`:
```bash
... | awk '...' | sort -k2 -rn    # ordenar por frecuencia descendente
```
</details>

---

### Ejercicio 5 — El truco !seen[$0]++

**Predicción**: ¿qué imprime?

```bash
echo -e "banana\napple\nbanana\ncherry\napple\ndate" | awk '!seen[$0]++'
```

<details>
<summary>Respuesta</summary>

```
banana
apple
cherry
date
```

Primera aparición de cada línea, en orden original:

```
"banana"  → seen["banana"] = 0 → !0 = true → PRINT, luego ++ → 1
"apple"   → seen["apple"]  = 0 → !0 = true → PRINT, luego ++ → 1
"banana"  → seen["banana"] = 1 → !1 = false → no print, luego ++ → 2
"cherry"  → seen["cherry"] = 0 → !0 = true → PRINT, luego ++ → 1
"apple"   → seen["apple"]  = 1 → !1 = false → no print
"date"    → seen["date"]   = 0 → !0 = true → PRINT
```

A diferencia de `sort -u`, preserva el orden original y no necesita
que el input esté ordenado.
</details>

---

### Ejercicio 6 — Multi-archivo con NR==FNR

**Predicción**: ¿qué imprime?

```bash
awk '
NR == FNR { lookup[$1] = $2; next }
$1 in lookup { print $0, lookup[$1] }
' <(echo -e "101 Alice\n102 Bob\n103 Charlie") \
  <(echo -e "101 85\n103 92\n104 78")
```

<details>
<summary>Respuesta</summary>

```
101 85 Alice
103 92 Charlie
```

Primer archivo (NR==FNR true):
- lookup["101"] = "Alice"
- lookup["102"] = "Bob"
- lookup["103"] = "Charlie"

Segundo archivo (NR≠FNR):
- "101": $1="101", está en lookup → print "101 85 Alice"
- "103": $1="103", está en lookup → print "103 92 Charlie"
- "104": $1="104", NO está en lookup → skip

ID 102 está en lookup pero no en el segundo archivo, así que no aparece.
ID 104 está en el segundo archivo pero no en lookup, así que se filtra.
Es un inner join por campo 1.
</details>

---

### Ejercicio 7 — printf formateado

**Predicción**: ¿qué imprime?

```bash
echo -e "Alice 85000\nBob 92000\nCharlie 65000" | awk '
BEGIN { printf "%-10s %10s %8s\n", "Name", "Salary", "Monthly" }
{
    monthly = $2 / 12
    printf "%-10s %10d %8.2f\n", $1, $2, monthly
}
'
```

<details>
<summary>Respuesta</summary>

```
Name           Salary  Monthly
Alice           85000  7083.33
Bob             92000  7666.67
Charlie         65000  5416.67
```

- `%-10s`: string alineado a la izquierda, 10 caracteres
- `%10d`: entero alineado a la derecha, 10 caracteres
- `%8.2f`: float, 8 caracteres total, 2 decimales

AWK maneja floats nativamente (a diferencia de Bash que necesita `bc`).
La división `$2 / 12` produce un float directamente.
</details>

---

### Ejercicio 8 — Función definida por usuario

**Predicción**: ¿qué imprime?

```bash
echo -e "1536\n2097152\n5368709120" | awk '
function human(bytes,    i, units) {
    split("B KB MB GB TB", units, " ")
    i = 1
    while (bytes >= 1024 && i < 5) {
        bytes /= 1024
        i++
    }
    return sprintf("%.1f %s", bytes, units[i])
}
{ print human($1) }
'
```

<details>
<summary>Respuesta</summary>

```
1.5 KB
2.0 MB
5.0 GB
```

- 1536: 1536/1024 = 1.5 → "1.5 KB"
- 2097152: /1024 = 2048 → /1024 = 2.0 → "2.0 MB"
- 5368709120: /1024³ = 5.0 → "5.0 GB"

Las variables `i` y `units` después de la coma en la firma son variables
locales (convención AWK). Sin esto, serían globales y podrían interferir
con otros usos.
</details>

---

### Ejercicio 9 — Procesar CSV

**Predicción**: ¿qué imprime?

```bash
cat <<'EOF' | awk -F, '
NR == 1 { next }
{ dept[$2] += $3; count[$2]++ }
END {
    for (d in dept)
        printf "%s: avg=%.0f (n=%d)\n", d, dept[d]/count[d], count[d]
}
'
Name,Dept,Score
Alice,Eng,85
Bob,Eng,92
Charlie,Sales,78
Diana,Eng,88
Eve,Sales,82
EOF
```

<details>
<summary>Respuesta</summary>

```
Eng: avg=88 (n=3)
Sales: avg=80 (n=2)
```

(Orden de Eng/Sales puede variar.)

- `NR == 1 { next }` — salta el header
- Engineering: (85+92+88)/3 = 265/3 = 88.33 → `%.0f` = 88
- Sales: (78+82)/2 = 160/2 = 80.00 → `%.0f` = 80

`-F,` separa por coma. `$2` es el departamento, `$3` es el score.
Los arrays `dept` y `count` acumulan por clave (departamento).
</details>

---

### Ejercicio 10 — Reporte completo

**Predicción**: ¿qué produce este one-liner sobre la salida de `ls -l`?

```bash
ls -l /etc/*.conf 2>/dev/null | awk '
NR <= 3 {   # solo los primeros 3 archivos como ejemplo
    size = $5
    name = $NF
    total += size
    printf "%-30s %8d bytes\n", name, size
}
END {
    printf "%-30s %8d bytes\n", "---TOTAL---", total
}
'
```

<details>
<summary>Respuesta</summary>

Ejemplo de output (depende del sistema):

```
/etc/adduser.conf                  3028 bytes
/etc/debconf.conf                  2969 bytes
/etc/deluser.conf                   604 bytes
---TOTAL---                        6601 bytes
```

- `$5` es la columna de tamaño en `ls -l`
- `$NF` es el último campo (nombre del archivo)
- `NR <= 3` limita a las primeras 3 líneas de output de `ls`
- END imprime el total acumulado

Nota: parsear `ls` no es recomendable en producción (nombres con espacios
rompen el parsing). Usar `stat` o `find -printf` es más robusto. Pero
para reportes rápidos ad-hoc, este patrón es muy común.
</details>