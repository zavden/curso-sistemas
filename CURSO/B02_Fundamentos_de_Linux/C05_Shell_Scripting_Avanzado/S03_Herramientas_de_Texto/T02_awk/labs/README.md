# Lab — awk

## Objetivo

Dominar awk: acceso a campos ($1, $NF, $0), separadores de
entrada y salida (-F, OFS), patrones (regex, comparacion, rangos),
bloques BEGIN/END, variables (NR, NF, FNR, FILENAME), print vs
printf, arrays asociativos, funciones de string (length, substr,
split, sub, gsub, toupper), y recetas practicas.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Campos y separadores

### Objetivo

Acceder a campos de texto y controlar como awk divide las lineas.

### Paso 1.1: Campos basicos

```bash
docker compose exec debian-dev bash -c '
echo "=== Campos en awk ==="
echo ""
echo "--- \$0 = linea completa ---"
echo "hello world bash" | awk "{print \$0}"
echo ""
echo "--- \$1, \$2, ... = campos individuales ---"
echo "hello world bash" | awk "{print \$1}"
echo "hello world bash" | awk "{print \$2}"
echo "hello world bash" | awk "{print \$3}"
echo ""
echo "--- \$NF = ultimo campo ---"
echo "one two three four five" | awk "{print \$NF}"
echo ""
echo "--- \$(NF-1) = penultimo ---"
echo "one two three four five" | awk "{print \$(NF-1)}"
echo ""
echo "--- Rearranjar campos ---"
echo "nombre:edad:ciudad" | awk -F: "{print \$3, \$1, \$2}"
echo ""
echo "--- Aplicar a /etc/passwd ---"
echo "Primeros 3 usuarios:"
head -3 /etc/passwd | awk -F: "{print \$1, \"(UID:\"\$3\")\"}"
'
```

### Paso 1.2: Separadores -F y OFS

```bash
docker compose exec debian-dev bash -c '
echo "=== Separadores ==="
echo ""
echo "--- -F (input field separator) ---"
echo "campo1:campo2:campo3" | awk -F: "{print \$1, \$2}"
echo ""
echo "--- -F con regex ---"
echo "campo1::campo2:::campo3" | awk -F:+ "{print \$1, \$2, \$3}"
echo "(F:+ = uno o mas :)"
echo ""
echo "--- -F con multiples caracteres ---"
echo "campo1|campo2,campo3" | awk -F"[|,]" "{print \$1, \$2, \$3}"
echo "(F con clase de caracteres)"
echo ""
echo "--- OFS (output field separator) ---"
echo "a b c" | awk "BEGIN{OFS=\",\"} {print \$1,\$2,\$3}"
echo "a b c" | awk "BEGIN{OFS=\" → \"} {print \$1,\$2,\$3}"
echo ""
echo "--- OFS con reasignacion ---"
echo "a b c d" | awk "BEGIN{OFS=\"-\"} {\$1=\$1; print}"
echo "(\$1=\$1 fuerza la reconstruccion de \$0 con OFS)"
'
```

### Paso 1.3: print vs printf

```bash
docker compose exec debian-dev bash -c '
echo "=== print vs printf ==="
echo ""
echo "--- print: agrega ORS (newline por defecto) ---"
echo "test" | awk "{print \"Resultado:\", \$1}"
echo ""
echo "--- printf: formato controlado (sin newline automatico) ---"
head -5 /etc/passwd | awk -F: "{printf \"%-15s UID=%-5s Shell=%s\n\", \$1, \$3, \$7}"
echo ""
echo "--- Formatos de printf ---"
echo "  %s    string"
echo "  %d    entero"
echo "  %f    float"
echo "  %-15s string alineado a la izquierda, 15 chars"
echo "  %05d  entero con ceros a la izquierda, 5 digitos"
echo ""
echo "--- printf con tabulacion ---"
head -3 /etc/passwd | awk -F: "{printf \"%s\t%s\t%s\n\", \$1, \$3, \$7}"
'
```

---

## Parte 2 — Patrones y bloques

### Objetivo

Filtrar lineas con patrones y usar BEGIN/END para inicializacion
y resumen.

### Paso 2.1: Patrones

```bash
docker compose exec debian-dev bash -c '
echo "=== Patrones en awk ==="
echo ""
echo "--- Regex ---"
head -10 /etc/passwd | awk -F: "/^root|^nobody/ {print \$1, \$3}"
echo "(lineas que empiezan con root o nobody)"
echo ""
echo "--- Comparacion de campos ---"
head -20 /etc/passwd | awk -F: "\$3 >= 1000 {print \$1, \"UID=\"\$3}"
echo "(usuarios con UID >= 1000)"
echo ""
echo "--- Patron en campo especifico ---"
head -10 /etc/passwd | awk -F: "\$7 ~ /bash/ {print \$1, \$7}"
echo "(\$7 matchea /bash/)"
echo ""
echo "--- Negacion ---"
head -10 /etc/passwd | awk -F: "\$7 !~ /nologin/ {print \$1, \$7}"
echo "(\$7 NO matchea /nologin/)"
echo ""
echo "--- Rango ---"
echo -e "START\ndata1\ndata2\nEND\nignored" | awk "/START/,/END/ {print}"
echo "(desde START hasta END, inclusive)"
'
```

### Paso 2.2: BEGIN y END

```bash
docker compose exec debian-dev bash -c '
echo "=== BEGIN y END ==="
echo ""
echo "--- BEGIN: se ejecuta ANTES de leer la primera linea ---"
echo "--- END: se ejecuta DESPUES de leer la ultima linea ---"
echo ""
echo "--- Contar usuarios por shell ---"
awk -F: "
BEGIN {
    printf \"%-20s %s\n\", \"Shell\", \"Usuarios\"
    printf \"%-20s %s\n\", \"-------------------\", \"--------\"
}
{
    shells[\$7]++
}
END {
    for (s in shells)
        printf \"%-20s %d\n\", s, shells[s]
}" /etc/passwd
echo ""
echo "--- Calcular promedio ---"
echo -e "10\n20\n30\n40\n50" | awk "
{
    sum += \$1
    count++
}
END {
    printf \"Sum: %d, Count: %d, Avg: %.1f\n\", sum, count, sum/count
}"
'
```

### Paso 2.3: NR, NF, FNR

```bash
docker compose exec debian-dev bash -c '
echo "=== Variables internas ==="
echo ""
echo "--- NR: numero de registro (linea global) ---"
head -5 /etc/passwd | awk -F: "{print NR, \$1}"
echo ""
echo "--- NF: numero de campos en la linea actual ---"
echo -e "a b c\nd e\nf g h i" | awk "{print \"Linea\", NR, \"tiene\", NF, \"campos\"}"
echo ""
echo "--- Filtrar por NR ---"
echo "Primeras 3 lineas:"
awk "NR <= 3" /etc/passwd
echo ""
echo "Lineas 5 a 7:"
awk "NR >= 5 && NR <= 7" /etc/passwd
echo ""
echo "--- Saltar header ---"
echo -e "Name,Age\nAlice,30\nBob,25" | awk -F, "NR > 1 {print \$1, \"tiene\", \$2, \"anos\"}"
'
```

---

## Parte 3 — Arrays y funciones

### Objetivo

Usar arrays asociativos y funciones de string para procesamiento
avanzado.

### Paso 3.1: Arrays asociativos

```bash
docker compose exec debian-dev bash -c '
echo "=== Arrays asociativos ==="
echo ""
echo "--- Conteo de frecuencias ---"
echo "En /etc/passwd, contar usuarios por shell:"
awk -F: "{count[\$7]++} END {for (k in count) printf \"%3d %s\n\", count[k], k}" /etc/passwd | sort -rn
echo ""
echo "--- Detectar duplicados ---"
echo -e "apple\nbanana\napple\ncherry\nbanana\napple" | awk "
seen[\$0]++ == 1 {print \"Duplicado:\", \$0}
"
echo ""
echo "--- Sumar por categoria ---"
echo -e "ventas 100\ncompras 50\nventas 200\ncompras 75\nventas 300" | awk "
{
    total[\$1] += \$2
    count[\$1]++
}
END {
    for (cat in total)
        printf \"%s: total=%d, count=%d, avg=%.1f\n\", cat, total[cat], count[cat], total[cat]/count[cat]
}"
'
```

### Paso 3.2: Funciones de string

```bash
docker compose exec debian-dev bash -c '
echo "=== Funciones de string ==="
echo ""
echo "--- length ---"
echo "Hello World" | awk "{print \"Longitud:\", length(\$0)}"
echo ""
echo "--- substr(string, start, length) ---"
echo "Hello World" | awk "{print substr(\$0, 7)}"
echo "Hello World" | awk "{print substr(\$0, 1, 5)}"
echo ""
echo "--- split(string, array, separator) ---"
echo "192.168.1.100" | awk "{
    n = split(\$0, octets, \".\")
    for (i = 1; i <= n; i++)
        printf \"Octeto %d: %s\n\", i, octets[i]
}"
echo ""
echo "--- sub y gsub ---"
echo "hello world hello" | awk "{sub(/hello/, \"HOLA\"); print}"
echo "(sub: reemplaza la primera)"
echo "hello world hello" | awk "{gsub(/hello/, \"HOLA\"); print}"
echo "(gsub: reemplaza todas)"
echo ""
echo "--- toupper / tolower (gawk) ---"
echo "Hello World" | awk "{print toupper(\$0)}"
echo "Hello World" | awk "{print tolower(\$0)}"
'
```

### Paso 3.3: Recetas utiles

Antes de ejecutar, predice: como extraer solo las IPs de la
salida de `ip addr` usando awk?

```bash
docker compose exec debian-dev bash -c '
echo "=== Recetas utiles de awk ==="
echo ""
echo "--- Sumar una columna ---"
echo -e "item1 10\nitem2 20\nitem3 30" | awk "{sum += \$2} END {print \"Total:\", sum}"
echo ""
echo "--- Eliminar duplicados (como uniq pero sin necesitar sort) ---"
echo -e "a\nb\na\nc\nb\na" | awk "!seen[\$0]++"
echo ""
echo "--- Extraer IPs ---"
ip addr 2>/dev/null | awk "/inet / {split(\$2, a, \"/\"); print a[1]}" | head -5
echo ""
echo "--- CSV a formato legible ---"
echo -e "nombre,edad,ciudad\nAlice,30,Madrid\nBob,25,Barcelona" | awk -F, "
NR == 1 {
    for (i=1; i<=NF; i++) header[i] = \$i
    next
}
{
    for (i=1; i<=NF; i++)
        printf \"%s: %s  \", header[i], \$i
    print \"\"
}"
echo ""
echo "--- Top N procesos por memoria ---"
ps aux --no-header 2>/dev/null | awk "{mem[\$11] += \$6} END {for (p in mem) printf \"%8d KB  %s\n\", mem[p], p}" | sort -rn | head -5
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `$1`-`$NF` acceden a campos individuales, `$0` es la linea
   completa. `-F` define el separador de entrada (por defecto
   espacios/tabs). `OFS` define el separador de salida.

2. **BEGIN** se ejecuta antes de leer datos (inicializacion),
   **END** despues de leer todos (resumen). Son ideales para
   headers y totales.

3. `NR` es el numero de linea global, `NF` el numero de campos.
   `NR > 1` salta headers, `$NF` accede al ultimo campo.

4. Los **arrays asociativos** de awk son la herramienta principal
   para conteos (`count[$key]++`), sumas (`total[$key] += $value`),
   y deteccion de duplicados (`!seen[$0]++`).

5. `sub()` reemplaza la primera ocurrencia, `gsub()` todas.
   `split()` divide un string en un array. `substr()` extrae
   subcadenas. `toupper()`/`tolower()` son de gawk.

6. awk combina filtrado (patrones), transformacion (acciones),
   y agregacion (arrays + END) en un solo comando. Para muchos
   casos reemplaza combinaciones de grep + sed + sort + uniq.
