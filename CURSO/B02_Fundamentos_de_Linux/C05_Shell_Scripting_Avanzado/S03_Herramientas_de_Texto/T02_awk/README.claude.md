# T02 — awk

> **Fuentes:** `README.md` (base) + `README.max.md` (ampliado) + `labs/README.md`
>
> **Erratas corregidas respecto a las fuentes:**
>
> | Ubicación | Error | Corrección |
> |---|---|---|
> | md:367, max.md:383 | Receta de transposición usa `a[i][NR]` — arrays multidimensionales reales de gawk 4.0+ | No funciona en mawk (default Debian/Ubuntu). Alternativa POSIX: `a[i,NR]` con SUBSEP |
> | max.md:406 | "Funciones de usuario" listadas como feature exclusiva de gawk | Las funciones definidas por el usuario (`function nombre() {}`) son parte de POSIX awk |
> | max.md:532-536 | Ejercicio CSV con `-F'","'` roto: `"Smith, John",30,New York` solo tiene un `","` como separador, `$3` queda vacío | Para CSV con comillas se necesita gawk `--csv` o un parser dedicado |

---

## Qué es awk

`awk` es un lenguaje de programación diseñado para procesar texto estructurado
por campos (columnas). A diferencia de sed que trabaja con líneas completas, awk
divide cada línea en campos y permite operar sobre ellos:

```bash
# Sintaxis general:
awk 'PATRÓN { ACCIÓN }' archivo
awk -F'SEPARADOR' 'programa' archivo

# Si se omite el PATRÓN, la acción se ejecuta en toda línea
# Si se omite la ACCIÓN, se imprime la línea completa (como grep)
```

```bash
# Ejemplo fundamental:
echo "alice 30 madrid" | awk '{ print $1 }'
# alice

echo "alice 30 madrid" | awk '{ print $3, $1 }'
# madrid alice

# $0 = la línea completa
# $1, $2, ... = campos individuales
# Por defecto, el separador es espacio/tab (cualquier cantidad)
```

---

## Campos

```bash
# $1, $2, ... $NF — campos por posición:
echo "one two three four" | awk '{ print $1 }'     # one
echo "one two three four" | awk '{ print $2 }'     # two
echo "one two three four" | awk '{ print $NF }'    # four (último campo)
echo "one two three four" | awk '{ print $(NF-1) }' # three (penúltimo)
echo "one two three four" | awk '{ print $0 }'     # one two three four

# NF = Number of Fields (número de campos en la línea actual)
```

### Separador de campos — -F

```bash
# -F define el separador de entrada:
echo "alice:30:madrid" | awk -F: '{ print $1 }'
# alice

# /etc/passwd separado por :
awk -F: '{ print $1, $3 }' /etc/passwd
# root 0
# daemon 1
# ...

# Separador de múltiples caracteres:
echo "alice::30::madrid" | awk -F'::' '{ print $2 }'
# 30

# Separador como regex:
echo "alice  30   madrid" | awk -F'[[:space:]]+' '{ print $2 }'
# 30

# Múltiples caracteres como separador (clase):
echo "campo1|campo2,campo3" | awk -F'[|,]' '{ print $1, $2, $3 }'
# campo1 campo2 campo3

# Tab como separador:
awk -F'\t' '{ print $2 }' file.tsv
```

### Separador de salida — OFS

```bash
# OFS (Output Field Separator) controla cómo se separan los campos en print:
echo "alice:30:madrid" | awk -F: '{ print $1, $3 }'
# alice madrid  (separados por espacio — OFS default)

echo "alice:30:madrid" | awk -F: -v OFS=',' '{ print $1, $3 }'
# alice,madrid  (separados por coma)

echo "alice:30:madrid" | awk -F: -v OFS='\t' '{ print $1, $3 }'
# alice	madrid  (separados por tab)

# IMPORTANTE: la coma en print usa OFS
# Sin coma, los campos se concatenan sin separador:
echo "alice:30:madrid" | awk -F: '{ print $1 $3 }'
# alicemadrid  (concatenado, sin separador)
```

> **Forzar reconstrucción con OFS:** si modificas OFS pero no cambias ningún
> campo, `$0` conserva los separadores originales. Para forzar que `$0` se
> reconstruya con el nuevo OFS, asigna a cualquier campo:
>
> ```bash
> echo "a:b:c" | awk -F: -v OFS=',' '{ $1=$1; print }'
> # a,b,c
> ```

---

## Patrones

### Patrón de texto (regex)

```bash
# Imprimir líneas que coinciden con un patrón:
awk '/error/' /var/log/syslog          # como grep "error"
awk '/^root/' /etc/passwd              # líneas que empiezan con root
awk '!/^#/' config.ini                 # líneas que NO empiezan con #

# Patrón en un campo específico:
awk -F: '$7 ~ /bash/' /etc/passwd      # campo 7 contiene "bash"
awk -F: '$7 !~ /nologin/' /etc/passwd  # campo 7 NO contiene "nologin"
# ~ = match regex
# !~ = no match regex
```

### Patrón de comparación

```bash
# Comparaciones numéricas y de string en campos:
awk -F: '$3 >= 1000' /etc/passwd          # UID >= 1000
awk -F: '$3 == 0' /etc/passwd             # UID == 0 (root)
awk -F: '$1 == "root"' /etc/passwd        # usuario exactamente "root"
awk 'NF > 3' file                          # líneas con más de 3 campos
awk 'length($0) > 80' file                # líneas con más de 80 caracteres
```

### Rango de patrones

```bash
# Desde un patrón hasta otro (como sed):
awk '/START/,/END/' file

# Desde la línea 5 hasta la 10:
awk 'NR >= 5 && NR <= 10' file

# Combinar patrones:
awk '/error/ && /critical/' file       # ambos en la misma línea
awk '/error/ || /warning/' file        # cualquiera de los dos
```

---

## BEGIN y END

`BEGIN` se ejecuta **antes** de procesar cualquier línea. `END` se ejecuta
**después** de procesar todas:

```bash
awk 'BEGIN { print "=== Inicio ===" }
     { print $0 }
     END { print "=== Fin ===" }' file
```

```bash
# Uso típico: inicializar variables en BEGIN, resumen en END:
awk -F: '
    BEGIN { count = 0 }
    $3 >= 1000 { count++ }
    END { print "Usuarios regulares:", count }
' /etc/passwd
```

```bash
# BEGIN sin archivo de entrada (awk como calculadora):
awk 'BEGIN { print 2^32 }'
# 4294967296

awk 'BEGIN { printf "%.2f\n", 100/3 }'
# 33.33
```

---

## Variables built-in

| Variable | Significado |
|---|---|
| `NR` | Number of Records — número de línea global |
| `NF` | Number of Fields — campos en la línea actual |
| `FS` | Field Separator — separador de entrada (como -F) |
| `OFS` | Output Field Separator — separador de salida |
| `RS` | Record Separator — separador de registros (default: newline) |
| `ORS` | Output Record Separator — separador de salida de registros |
| `FNR` | File Number Record — línea dentro del archivo actual |
| `FILENAME` | Nombre del archivo actual |
| `$0` | La línea completa |
| `$N` | El campo N |

```bash
# NR — número de línea (global):
awk '{ print NR, $0 }' file
# 1 primera línea
# 2 segunda línea

# NF — número de campos:
echo -e "one\none two\none two three" | awk '{ print NF, $0 }'
# 1 one
# 2 one two
# 3 one two three

# FNR vs NR con múltiples archivos:
awk '{ print FILENAME, FNR, NR }' file1 file2
# file1 1 1    (FNR se reinicia por archivo, NR no)
# file1 2 2
# file2 1 3    ← FNR vuelve a 1, NR sigue en 3
# file2 2 4
```

```bash
# RS — Record Separator (registros separados por algo diferente a newline):
echo "alice,30|bob,25|charlie,35" | awk -F, -v RS='|' '{ print $1 }'
# alice
# bob
# charlie

# RS="" = modo "párrafo" (registros separados por líneas vacías):
awk -v RS="" '{ print "Párrafo:", NR }' file
```

---

## print vs printf

```bash
# print — simple, agrega ORS al final (default: newline):
awk '{ print $1, $2 }' file
# Separados por OFS, terminados por ORS

# printf — formateo estilo C (NO agrega newline automático):
awk '{ printf "%-20s %5d\n", $1, $3 }' file
# %-20s = string alineado a la izquierda, 20 caracteres
# %5d   = entero, 5 dígitos, alineado a la derecha
```

| Formato | Significado |
|---|---|
| `%s` | string |
| `%d` | entero decimal |
| `%f` | float |
| `%e` | notación científica |
| `%x` | hexadecimal |
| `%-Ns` | string alineado a la izquierda, N caracteres |
| `%Nd` | entero alineado a la derecha, N dígitos |
| `%.Nf` | float con N decimales |
| `%05d` | entero con ceros a la izquierda, 5 dígitos |

```bash
# Tabla formateada:
awk -F: 'BEGIN { printf "%-20s %6s %s\n", "USER", "UID", "SHELL";
                 printf "%-20s %6s %s\n", "----", "---", "-----" }
         $3 >= 1000 { printf "%-20s %6d %s\n", $1, $3, $7 }' /etc/passwd
```

---

## Acciones — Variables, condicionales, loops

awk es un lenguaje completo con variables, condicionales y loops:

### Variables

```bash
# Las variables no necesitan declaración ni $:
awk '{ total += $1 } END { print "Total:", total }' file
# total se inicializa automáticamente a 0 (numérico) o "" (string)

# Asignar desde la línea de comandos con -v:
awk -v threshold=100 '$3 > threshold { print $0 }' file
```

### Condicionales

```bash
awk -F: '{
    if ($3 == 0)
        print $1, "es root"
    else if ($3 < 1000)
        print $1, "es usuario de sistema"
    else
        print $1, "es usuario regular"
}' /etc/passwd

# Ternario:
awk -F: '{ print $1, ($3 >= 1000 ? "regular" : "sistema") }' /etc/passwd
```

### Loops

```bash
# for:
awk 'BEGIN { for (i = 1; i <= 5; i++) print i }'

# while:
awk 'BEGIN { i = 1; while (i <= 5) { print i; i++ } }'

# Iterar campos de una línea:
echo "10 20 30 40 50" | awk '{
    sum = 0
    for (i = 1; i <= NF; i++) sum += $i
    print "Suma:", sum
}'
# Suma: 150
```

### Arrays asociativos

```bash
# awk tiene arrays asociativos nativos (como diccionarios):
awk -F: '{ shells[$7]++ }
    END { for (sh in shells) print sh, shells[sh] }
' /etc/passwd
# /bin/bash 3
# /usr/sbin/nologin 25
# /bin/false 2

# Contar ocurrencias de IPs en un log:
awk '{ ips[$1]++ } END { for (ip in ips) print ips[ip], ip }' access.log | sort -rn
```

> **`for (key in array)`:** el orden de iteración es **indefinido** (no es
> el orden de inserción ni alfabético). Para salida ordenada, pipar a `sort`.

> **Eliminar entrada de un array:** `delete array[key]`. Para vaciar todo:
> `delete array` (gawk) o iterar y borrar cada clave (POSIX).

---

## Funciones built-in

### Strings

```bash
length(s)                # longitud
substr(s, start, len)    # substring (1-indexed)
index(s, target)         # posición de target en s (0 si no está)
split(s, arr, sep)       # dividir s en array arr por sep
sub(regex, repl, target) # reemplazar primera coincidencia
gsub(regex, repl, target) # reemplazar todas las coincidencias
match(s, regex)          # posición del match (0 si no)
tolower(s)               # a minúsculas
toupper(s)               # a mayúsculas
sprintf(fmt, ...)        # formatear string (como printf pero retorna)
```

```bash
# Ejemplos:
echo "hello world" | awk '{ print length($0) }'       # 11
echo "hello world" | awk '{ print substr($0, 7) }'    # world
echo "hello world" | awk '{ print toupper($0) }'      # HELLO WORLD

# sub y gsub:
echo "aaa bbb aaa" | awk '{ sub(/aaa/, "XXX"); print }'   # XXX bbb aaa
echo "aaa bbb aaa" | awk '{ gsub(/aaa/, "XXX"); print }'  # XXX bbb XXX

# split:
echo "a,b,c,d" | awk '{ n = split($0, arr, ","); for (i=1; i<=n; i++) print arr[i] }'
# a
# b
# c
# d
```

> **`sub`/`gsub` sin tercer argumento:** operan sobre `$0` (la línea completa).
> Con tercer argumento, operan sobre esa variable: `gsub(/old/, "new", $3)`
> modifica solo el campo 3.

### Matemáticas

```bash
int(x)      # parte entera
sqrt(x)     # raíz cuadrada
sin(x)      # seno
cos(x)      # coseno
log(x)      # logaritmo natural
exp(x)      # e^x
rand()      # número aleatorio [0, 1)
srand(seed) # semilla para rand
```

---

## Funciones definidas por el usuario

```bash
# POSIX awk soporta funciones (no es exclusivo de gawk):
awk '
function max(a, b) {
    return (a > b) ? a : b
}
function min(a, b) {
    return (a < b) ? a : b
}
{
    print "max(" $1 "," $2 ") =", max($1, $2)
    print "min(" $1 "," $2 ") =", min($1, $2)
}' << 'EOF'
10 20
5 3
EOF
```

---

## Recetas comunes

```bash
# Sumar una columna:
awk '{ sum += $1 } END { print sum }' file

# Promedio de una columna:
awk '{ sum += $3; count++ } END { print sum/count }' file

# Máximo de una columna:
awk 'NR == 1 || $1 > max { max = $1 } END { print max }' file

# Imprimir campos del 3 al último:
awk '{ for (i=3; i<=NF; i++) printf "%s ", $i; print "" }' file

# Eliminar duplicados preservando orden (como sort -u sin reordenar):
awk '!seen[$0]++' file

# Imprimir línea anterior a un match:
awk '/pattern/ { print prev } { prev = $0 }' file

# Saltar header (primera línea):
awk 'NR > 1 { print $0 }' file

# Top 10 IPs de un access log:
awk '{ ips[$1]++ } END { for (ip in ips) print ips[ip], ip }' access.log | sort -rn | head -10

# CSV a formato tabla:
awk -F, '{ printf "%-15s %-10s %s\n", $1, $2, $3 }' data.csv

# Extraer IPs de ip addr:
ip addr | awk '/inet / { split($2, a, "/"); print a[1] }'

# Sumar por categoría:
awk '{ total[$1] += $2 } END { for (k in total) print k, total[k] }' file
```

### `!seen[$0]++` explicado

```bash
# Esta expresión es un modismo clásico de awk:
awk '!seen[$0]++' file

# Desglose:
# seen[$0]   → busca la línea actual en el array seen
# seen[$0]++ → post-incremento: retorna el valor actual, LUEGO incrementa
#              - primera vez: retorna 0 (no existía), luego seen[$0]=1
#              - segunda vez: retorna 1, luego seen[$0]=2
# !          → negación: !0 = verdadero (imprimir), !1 = falso (no imprimir)
#
# Resultado: solo imprime la primera aparición de cada línea
```

---

## gawk vs mawk

```bash
# gawk (GNU awk) — el más completo:
# - Arrays multidimensionales reales: a[i][j]
# - @include para importar archivos
# - --csv para parsear CSV con comillas
# - Extensiones con -e

# mawk — rápido pero básico:
# - Más rápido que gawk para tareas simples
# - Default en Debian/Ubuntu (awk → mawk)
# - No soporta: a[i][j], @include, --csv
# - Sí soporta: funciones de usuario, tolower/toupper (desde mawk 1.3.4+)

# En Debian/Ubuntu:
ls -la /usr/bin/awk
# /usr/bin/awk -> /etc/alternatives/awk -> /usr/bin/mawk

# Instalar gawk si necesitas sus extensiones:
# apt install gawk

# En RHEL/Fedora:
# awk → gawk (siempre)
```

---

## Ejercicios

### Ejercicio 1 — Campos y separadores

```bash
# De /etc/passwd, mostrar usuario, UID y shell separados por tab:
awk -F: -v OFS='\t' '{ print $1, $3, $7 }' /etc/passwd | head -5
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué hace <code>-F:</code>?</summary>

Define `:` como separador de campos de entrada. `/etc/passwd` usa `:` para separar sus 7 campos: usuario:password:UID:GID:gecos:home:shell.

</details>

<details><summary>2. ¿Qué pasa si quitas <code>-v OFS='\t'</code>?</summary>

Los campos se separan por espacio (OFS default). Con `OFS='\t'` se separan por tab, creando columnas alineadas.

</details>

<details><summary>3. ¿Qué pasa si usas <code>print $1 $3 $7</code> (sin comas)?</summary>

Los tres campos se concatenan sin separador alguno: `root0/bin/bash`. Las comas en `print` son las que insertan el OFS entre campos.

</details>

---

### Ejercicio 2 — Patrones de filtrado

```bash
# a) Usuarios con UID >= 1000:
awk -F: '$3 >= 1000 { print $1, $3 }' /etc/passwd

# b) Líneas que contienen "bash" en el campo 7:
awk -F: '$7 ~ /bash/ { print $1, $7 }' /etc/passwd

# c) Líneas con más de 3 campos (archivo genérico):
echo -e "a\na b\na b c\na b c d" | awk 'NF > 3'
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Cuál es la diferencia entre <code>$7 ~ /bash/</code> y <code>$7 == "/bin/bash"</code>?</summary>

`~ /bash/` es un match de regex — coincide si el campo **contiene** "bash" en cualquier parte (e.g., `/bin/bash`, `/usr/bin/bash`). `== "/bin/bash"` es comparación exacta — solo coincide con esa cadena literal.

</details>

<details><summary>2. ¿Qué imprime <code>awk 'NF > 3'</code> sobre las 4 líneas?</summary>

Solo `a b c d` — es la única línea con más de 3 campos (tiene 4). Las demás tienen 1, 2, y 3 campos respectivamente.

</details>

---

### Ejercicio 3 — BEGIN/END y agregación

```bash
awk -F: '
    BEGIN { count = 0; printf "%-20s %6s\n", "USUARIO", "UID" }
    $3 >= 1000 { printf "%-20s %6d\n", $1, $3; count++ }
    END { printf "\nTotal usuarios regulares: %d\n", count }
' /etc/passwd
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Es necesario <code>BEGIN { count = 0 }</code>?</summary>

No estrictamente — awk inicializa variables a 0 automáticamente. Pero explicitarlo en BEGIN documenta la intención y es buena práctica para legibilidad.

</details>

<details><summary>2. ¿Cuándo se ejecuta el bloque <code>END</code>?</summary>

Después de procesar **todas** las líneas del archivo. En ese momento `count` tiene el total acumulado. Si el archivo estuviera vacío, `END` se ejecuta igualmente (con count=0).

</details>

---

### Ejercicio 4 — Arrays asociativos: conteo de frecuencias

```bash
# Contar usuarios por shell y mostrar ordenado:
awk -F: '{ shells[$7]++ }
    END { for (sh in shells) printf "%3d  %s\n", shells[sh], sh }
' /etc/passwd | sort -rn
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué hace <code>shells[$7]++</code>?</summary>

Para cada línea, usa el campo 7 (shell) como clave del array `shells` e incrementa su contador. Si la clave no existía, se crea con valor 0 y luego se incrementa a 1.

</details>

<details><summary>2. ¿Por qué se necesita <code>| sort -rn</code>?</summary>

Porque `for (key in array)` en awk itera en orden **indefinido** (no alfabético, no de inserción). El `sort -rn` ordena numéricamente en orden descendente para mostrar los shells más usados primero.

</details>

---

### Ejercicio 5 — Sumar por categoría

```bash
cat << 'EOF' | awk '{ total[$1] += $2; count[$1]++ }
    END { for (cat in total) printf "%-10s total=%d  promedio=%.1f\n", cat, total[cat], total[cat]/count[cat] }'
ventas 100
compras 50
ventas 200
compras 75
ventas 300
EOF
```

**Predicción:** antes de ejecutar, responde:

<details><summary>¿Qué totales y promedios produce?</summary>

```
ventas     total=600  promedio=200.0
compras    total=125  promedio=62.5
```

(El orden puede variar — `for in` no garantiza orden.)

`total["ventas"]` = 100+200+300 = 600, `count["ventas"]` = 3, promedio = 200.0.
`total["compras"]` = 50+75 = 125, `count["compras"]` = 2, promedio = 62.5.

</details>

---

### Ejercicio 6 — NR, NF y FNR

```bash
echo -e "a b c\nd e\nf" | awk '{ print "NR=" NR, "NF=" NF, "->", $0 }'
```

**Predicción:** antes de ejecutar, responde:

<details><summary>¿Qué imprime cada línea?</summary>

```
NR=1 NF=3 -> a b c
NR=2 NF=2 -> d e
NR=3 NF=1 -> f
```

`NR` se incrementa con cada línea (1, 2, 3). `NF` varía según cuántos campos tiene cada línea (3, 2, 1). `$0` es la línea completa.

</details>

---

### Ejercicio 7 — printf vs print

```bash
awk -F: '
    BEGIN { printf "%-15s %6s  %s\n", "USUARIO", "UID", "HOME" }
    $3 >= 1000 && $7 !~ /nologin/ {
        printf "%-15s %6d  %s\n", $1, $3, $6
    }
' /etc/passwd
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué hace <code>%-15s</code> vs <code>%6d</code>?</summary>

`%-15s`: string de 15 caracteres alineado a la **izquierda** (el `-` indica alineación izquierda).
`%6d`: entero de 6 dígitos alineado a la **derecha** (sin `-`, default es derecha).

Esto crea columnas alineadas tipo tabla.

</details>

<details><summary>2. ¿Por qué <code>printf</code> necesita <code>\n</code> pero <code>print</code> no?</summary>

`print` agrega automáticamente ORS (newline por defecto) al final. `printf` no agrega nada — tienes control total del formato. Sin `\n` en printf, todo se imprime en una sola línea.

</details>

---

### Ejercicio 8 — Funciones de string

```bash
echo "192.168.1.100" | awk '{
    n = split($0, octets, ".")
    for (i = 1; i <= n; i++)
        printf "Octeto %d: %s\n", i, octets[i]
    print "Total octetos:", n
}'

echo "Hello World Hello" | awk '{
    gsub(/Hello/, "Hola")
    print
}'

echo "Hello World" | awk '{ print substr($0, 7, 5) }'
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué produce el <code>split</code>?</summary>

```
Octeto 1: 192
Octeto 2: 168
Octeto 3: 1
Octeto 4: 100
Total octetos: 4
```

`split($0, octets, ".")` divide `"192.168.1.100"` por `.` en el array `octets`. Retorna el número de elementos (4).

</details>

<details><summary>2. ¿Qué produce <code>substr($0, 7, 5)</code>?</summary>

`World` — extrae 5 caracteres empezando en la posición 7. Recuerda: awk usa índices **1-based** (no 0-based como C).

</details>

---

### Ejercicio 9 — Eliminar duplicados preservando orden

```bash
echo -e "banana\napple\ncherry\napple\nbanana\ndate\ncherry" | awk '!seen[$0]++'
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Qué líneas imprime y en qué orden?</summary>

```
banana
apple
cherry
date
```

Primera aparición de cada línea, en el orden original. Las repeticiones (apple, banana, cherry) se suprimen.

</details>

<details><summary>2. Traza del array <code>seen</code> para las primeras 4 líneas</summary>

| Línea | `$0` | `seen[$0]` antes | `seen[$0]++` retorna | `!` | ¿Imprime? |
|---|---|---|---|---|---|
| 1 | banana | (no existe → 0) | 0 → luego 1 | !0=true | sí |
| 2 | apple | 0 | 0 → 1 | !0=true | sí |
| 3 | cherry | 0 | 0 → 1 | !0=true | sí |
| 4 | apple | 1 | 1 → 2 | !1=false | no |

El post-incremento `++` retorna el valor **antes** de incrementar. La primera vez es 0 (falsy), negado con `!` da true → imprime.

</details>

---

### Ejercicio 10 — Panorama: análisis de log

```bash
cat > /tmp/access.log << 'EOF'
192.168.1.10 GET /index.html 200 1024
192.168.1.20 GET /api/users 200 2048
192.168.1.10 POST /api/login 401 128
192.168.1.30 GET /index.html 200 1024
192.168.1.20 GET /api/users 500 64
192.168.1.10 GET /favicon.ico 404 0
192.168.1.20 GET /api/users 200 4096
192.168.1.10 GET /index.html 200 1024
EOF

# a) Top IPs por número de requests:
awk '{ ips[$1]++ } END { for (ip in ips) printf "%3d  %s\n", ips[ip], ip }' /tmp/access.log | sort -rn

# b) Requests con error (status >= 400):
awk '$4 >= 400 { printf "%-15s %-4s %-20s %s\n", $1, $2, $3, $4 }' /tmp/access.log

# c) Total de bytes transferidos por IP:
awk '{ bytes[$1] += $5 }
    END { for (ip in bytes) printf "%-15s %8d bytes\n", ip, bytes[ip] }
' /tmp/access.log

# d) Resumen completo:
awk '
    { total++; bytes += $5; status[$4]++; ips[$1]++ }
    END {
        printf "Total requests: %d\n", total
        printf "Total bytes:    %d\n", bytes
        printf "\nPor status:\n"
        for (s in status) printf "  %s: %d\n", s, status[s]
        printf "\nPor IP:\n"
        for (ip in ips) printf "  %-15s %d requests\n", ip, ips[ip]
    }
' /tmp/access.log

rm /tmp/access.log
```

**Predicción:** antes de ejecutar, responde:

<details><summary>1. ¿Cuál es la IP con más requests?</summary>

`192.168.1.10` con 4 requests (líneas 1, 3, 6, 8).

</details>

<details><summary>2. ¿Cuántos requests tienen status >= 400?</summary>

2 requests: la línea con status 401 (POST /api/login) y la línea con status 404 (/favicon.ico). El status 500 también es >= 400, así que son **3** requests con error.

</details>

<details><summary>3. ¿Cuántos bytes totales transfirió 192.168.1.20?</summary>

2048 + 64 + 4096 = 6208 bytes (sus 3 requests).

</details>
