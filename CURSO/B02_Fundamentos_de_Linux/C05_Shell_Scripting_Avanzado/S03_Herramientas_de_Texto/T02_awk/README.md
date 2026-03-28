# T02 — awk

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
# ...

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

## print vs printf

```bash
# print — simple, agrega ORS al final (default: newline):
awk '{ print $1, $2 }' file
# Separados por OFS, terminados por ORS

# printf — formateo estilo C (NO agrega newline automático):
awk '{ printf "%-20s %5d\n", $1, $3 }' file
# %-20s = string alineado a la izquierda, 20 caracteres
# %5d   = entero, 5 dígitos, alineado a la derecha

# Formatos de printf:
# %s    string
# %d    entero decimal
# %f    float
# %e    notación científica
# %x    hexadecimal
# %-Ns  string alineado a la izquierda, N caracteres
# %Nd   entero alineado a la derecha, N dígitos
# %.Nf  float con N decimales
```

```bash
# Tabla formateada:
awk -F: 'BEGIN { printf "%-20s %6s %s\n", "USER", "UID", "SHELL";
                 printf "%-20s %6s %s\n", "----", "---", "-----" }
         $3 >= 1000 { printf "%-20s %6d %s\n", $1, $3, $7 }' /etc/passwd
```

## Acciones — Variables, condicionales, loops

awk es un lenguaje completo con variables, condicionales y loops:

### Variables

```bash
# Las variables no necesitan declaración ni $:
awk '{ total += $1 } END { print "Total:", total }' file
# total se inicializa automáticamente a 0

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

### Matemáticas

```bash
int(x)      # parte entera
sqrt(x)     # raíz cuadrada
sin(x)      # seno
cos(x)      # coseno
log(x)      # logaritmo natural
exp(x)      # e^x
rand()      # número aleatorio 0-1
srand(seed) # semilla para rand
```

## Recetas comunes

```bash
# Sumar una columna:
awk '{ sum += $1 } END { print sum }' file

# Promedio de una columna:
awk '{ sum += $3; count++ } END { print sum/count }' file

# Máximo de una columna:
awk 'BEGIN { max = -999999 } $1 > max { max = $1 } END { print max }' file

# Imprimir campos del 3 al último:
awk '{ for (i=3; i<=NF; i++) printf "%s ", $i; print "" }' file

# Eliminar duplicados preservando orden (como sort -u pero sin reordenar):
awk '!seen[$0]++' file

# Transponer filas ↔ columnas:
awk '{ for (i=1; i<=NF; i++) a[i][NR]=$i }
     END { for (i=1; i<=NF; i++) { for (j=1; j<=NR; j++) printf "%s ", a[i][j]; print "" } }' file

# Imprimir línea anterior a un match:
awk '/pattern/ { print prev } { prev = $0 }' file

# Top 10 IPs de un access log:
awk '{ print $1 }' access.log | sort | uniq -c | sort -rn | head -10
# O todo en awk:
awk '{ ips[$1]++ } END { for (ip in ips) print ips[ip], ip }' access.log | sort -rn | head -10

# CSV a formato tabla:
awk -F, '{ printf "%-15s %-10s %s\n", $1, $2, $3 }' data.csv
```

## gawk vs mawk vs nawk

```bash
# gawk (GNU awk) — el más completo:
# - Arrays multidimensionales
# - Funciones de usuario
# - Regex avanzado con @include
# - Extensiones con -e, --csv
# Instalado por defecto en la mayoría de distribuciones Linux

# mawk — rápido pero básico:
# - Más rápido que gawk para tareas simples
# - Default en Debian/Ubuntu (awk → mawk)
# - No soporta algunas extensiones de gawk

# En Debian/Ubuntu:
ls -la /usr/bin/awk
# /usr/bin/awk -> /etc/alternatives/awk -> /usr/bin/mawk

# Si necesitas gawk en Debian:
# apt install gawk

# En RHEL/Fedora:
# awk → gawk (siempre)
```

---

## Ejercicios

### Ejercicio 1 — Campos básicos

```bash
# Del archivo /etc/passwd, mostrar usuario y shell separados por tab:
awk -F: -v OFS='\t' '{ print $1, $7 }' /etc/passwd

# Mostrar solo usuarios con UID >= 1000:
awk -F: '$3 >= 1000 { print $1, $3 }' /etc/passwd
```

### Ejercicio 2 — Conteo y agregación

```bash
# Contar cuántos usuarios usan cada shell:
awk -F: '{ shells[$7]++ }
    END { for (sh in shells) printf "%-25s %d\n", sh, shells[sh] }
' /etc/passwd
```

### Ejercicio 3 — Formateo de tabla

```bash
# Crear una tabla formateada con usuario, UID y home:
awk -F: 'BEGIN { printf "%-15s %6s  %-25s\n", "USUARIO", "UID", "HOME";
                 printf "%-15s %6s  %-25s\n", "-------", "---", "----" }
         $3 >= 1000 && $7 !~ /nologin/ {
             printf "%-15s %6d  %-25s\n", $1, $3, $6
         }' /etc/passwd
```
