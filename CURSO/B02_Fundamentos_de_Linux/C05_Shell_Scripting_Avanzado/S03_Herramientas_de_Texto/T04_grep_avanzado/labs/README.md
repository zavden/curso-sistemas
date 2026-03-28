# Lab — grep avanzado

## Objetivo

Dominar grep a nivel profesional: modos de regex (BRE, ERE, PCRE),
flags avanzados (-o, -w, -q, -c, -l, -L, -m), contexto (-A, -B,
-C), busqueda recursiva con filtros (--include, --exclude-dir),
PCRE con lookahead/lookbehind/non-greedy, patrones comunes (IP,
email, URL), y optimizacion de rendimiento.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Modos y flags

### Objetivo

Entender los tres modos de regex de grep y los flags mas utiles.

### Paso 1.1: BRE, ERE, PCRE

```bash
docker compose exec debian-dev bash -c '
echo "=== Modos de regex en grep ==="
echo ""
echo "--- BRE (Basic Regular Expressions) — default ---"
echo "abc123def" | grep "[0-9]\+"
echo "Metacaracteres que necesitan escape en BRE: + ? | { } ( )"
echo ""
echo "--- ERE (Extended) — grep -E o egrep ---"
echo "abc123def" | grep -E "[0-9]+"
echo "En ERE, + ? | { } ( ) NO necesitan escape"
echo ""
echo "--- PCRE (Perl Compatible) — grep -P ---"
echo "abc123def" | grep -P "\d+"
echo "PCRE soporta: \\d \\w \\s \\b lookahead lookbehind"
echo ""
echo "--- Comparacion ---"
printf "%-12s %-18s %-18s %-18s\n" "Feature" "BRE (default)" "ERE (-E)" "PCRE (-P)"
printf "%-12s %-18s %-18s %-18s\n" "-----------" "-----------------" "-----------------" "-----------------"
printf "%-12s %-18s %-18s %-18s\n" "+" "\\+" "+" "+"
printf "%-12s %-18s %-18s %-18s\n" "?" "\\?" "?" "?"
printf "%-12s %-18s %-18s %-18s\n" "|" "\\|" "|" "|"
printf "%-12s %-18s %-18s %-18s\n" "()" "\\(\\)" "()" "()"
printf "%-12s %-18s %-18s %-18s\n" "\\d" "No" "No" "Si"
printf "%-12s %-18s %-18s %-18s\n" "Lookahead" "No" "No" "Si"
echo ""
echo "Recomendacion: usar -E por defecto, -P cuando se necesite \\d o lookahead"
'
```

### Paso 1.2: Flags de output

```bash
docker compose exec debian-dev bash -c '
echo "=== Flags de output ==="
echo ""
echo "--- -o: solo la parte que matchea ---"
echo "Mi IP es 192.168.1.100 y otra es 10.0.0.1" | grep -oE "[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+"
echo ""
echo "--- -w: solo palabras completas ---"
echo "cat concatenate catfish" | grep -ow "cat"
echo "(solo matchea cat como palabra, no dentro de concatenate)"
echo ""
echo "--- -c: contar lineas que matchean ---"
grep -c "nologin" /etc/passwd
echo "lineas con nologin en /etc/passwd"
echo ""
echo "--- -l: solo nombres de archivo ---"
grep -l "root" /etc/passwd /etc/group /etc/shadow 2>/dev/null
echo "(archivos que contienen root)"
echo ""
echo "--- -L: archivos que NO matchean ---"
grep -L "root" /etc/passwd /etc/hostname 2>/dev/null
echo "(archivos que NO contienen root)"
echo ""
echo "--- -m N: detenerse despues de N matches ---"
grep -m 3 ":" /etc/passwd
echo "(solo primeras 3 lineas que matchean)"
echo ""
echo "--- -q: silencioso (solo exit code) ---"
if grep -q "root" /etc/passwd; then
    echo "root encontrado (grep -q retorno 0)"
fi
'
```

### Paso 1.3: Flags de matching

```bash
docker compose exec debian-dev bash -c '
echo "=== Flags de matching ==="
echo ""
echo "--- -i: case insensitive ---"
echo -e "Error\nERROR\nerror\nWarning" | grep -i "error"
echo ""
echo "--- -v: invertir (lineas que NO matchean) ---"
echo "Usuarios con login (sin nologin ni false):"
grep -v -E "nologin|false" /etc/passwd | cut -d: -f1,7 | head -5
echo ""
echo "--- -x: match en TODA la linea ---"
echo -e "hello\nhello world\nworld" | grep -x "hello"
echo "(solo lineas que son exactamente hello)"
echo ""
echo "--- -F: string fijo (no regex) ---"
echo "buscar [error] literalmente" | grep -F "[error]"
echo "Sin -F, [ ] se interpretan como clase de caracteres"
echo ""
echo "--- -n: numeros de linea ---"
grep -n "root" /etc/passwd
'
```

---

## Parte 2 — Contexto y recursion

### Objetivo

Mostrar lineas de contexto alrededor de los matches y buscar
recursivamente en directorios.

### Paso 2.1: Contexto -A, -B, -C

```bash
docker compose exec debian-dev bash -c '
echo "=== Contexto ==="
echo ""
cat > /tmp/logfile.txt << '\''EOF'\''
2024-03-17 10:00:01 INFO Starting service
2024-03-17 10:00:02 INFO Loading config
2024-03-17 10:00:03 ERROR Failed to connect to database
2024-03-17 10:00:03 ERROR Connection refused on port 5432
2024-03-17 10:00:04 INFO Retrying in 5 seconds
2024-03-17 10:00:09 INFO Connection established
2024-03-17 10:00:10 WARNING Slow query detected
2024-03-17 10:00:11 INFO Processing complete
EOF
echo "--- -A N: N lineas despues del match ---"
grep -A 1 "ERROR" /tmp/logfile.txt
echo ""
echo "--- -B N: N lineas antes del match ---"
grep -B 1 "ERROR" /tmp/logfile.txt
echo ""
echo "--- -C N: N lineas antes Y despues ---"
grep -C 1 "WARNING" /tmp/logfile.txt
echo ""
echo "El separador -- indica limites entre grupos de matches"
rm /tmp/logfile.txt
'
```

### Paso 2.2: Busqueda recursiva

```bash
docker compose exec debian-dev bash -c '
echo "=== Busqueda recursiva ==="
echo ""
echo "--- -r: buscar en todos los archivos recursivamente ---"
grep -r "nameserver" /etc/ 2>/dev/null | head -5
echo ""
echo "--- --include: solo en archivos que matchean un glob ---"
grep -r --include="*.conf" "listen" /etc/ 2>/dev/null | head -5
echo "(solo en archivos .conf)"
echo ""
echo "--- --exclude-dir: saltar directorios ---"
echo "grep -r --exclude-dir=.git --exclude-dir=node_modules \"TODO\" ."
echo "(comun en proyectos de desarrollo)"
echo ""
echo "--- --exclude: saltar archivos ---"
echo "grep -r --exclude=\"*.log\" --exclude=\"*.tmp\" \"error\" /var/"
echo ""
echo "--- Combinacion tipica ---"
echo "grep -rnI --include=\"*.sh\" --exclude-dir=.git \"set -e\" ."
echo "  -r  recursivo"
echo "  -n  numeros de linea"
echo "  -I  ignorar binarios"
echo ""
echo "--- ripgrep (rg): alternativa moderna ---"
echo "rg ignora .git, binarios y .gitignore automaticamente"
echo "Es mucho mas rapido que grep -r en proyectos grandes"
which rg 2>/dev/null && echo "rg: instalado" || echo "rg: no instalado"
'
```

### Paso 2.3: Multiples patrones

```bash
docker compose exec debian-dev bash -c '
echo "=== Multiples patrones ==="
echo ""
echo "--- -E con | (OR) ---"
echo "Usuarios root o nobody:"
grep -E "^(root|nobody):" /etc/passwd
echo ""
echo "--- -e (multiples expresiones) ---"
grep -e "^root" -e "^nobody" /etc/passwd
echo "(equivalente al anterior)"
echo ""
echo "--- -f: patrones desde un archivo ---"
echo -e "root\nnobody\ndaemon" > /tmp/patterns.txt
grep -f /tmp/patterns.txt /etc/passwd | head -5
rm /tmp/patterns.txt
echo ""
echo "--- AND (todas las condiciones) ---"
echo "No hay AND nativo en grep. Soluciones:"
echo "  grep A file | grep B          → pipeline"
echo "  grep -P '(?=.*A)(?=.*B)' file → PCRE lookahead"
echo ""
echo "Ejemplo pipeline:"
grep "root" /etc/passwd | grep "bash"
echo "(lineas con root Y bash)"
'
```

---

## Parte 3 — PCRE y patrones comunes

### Objetivo

Usar PCRE para busquedas avanzadas y aplicar patrones del mundo
real.

### Paso 3.1: PCRE — clases y boundaries

```bash
docker compose exec debian-dev bash -c '
echo "=== PCRE: clases y boundaries ==="
echo ""
echo "--- \\d \\w \\s (shortcuts de clase) ---"
echo "abc 123 def_456" | grep -oP "\\d+"
echo "(\\d = digito, equivale a [0-9])"
echo ""
echo "abc 123 def_456" | grep -oP "\\w+"
echo "(\\w = word char, equivale a [a-zA-Z0-9_])"
echo ""
echo "--- \\b (word boundary) ---"
echo "cat concatenate catfish" | grep -oP "\\bcat\\b"
echo "(\\bcat\\b = cat como palabra completa)"
echo ""
echo "--- Non-greedy con ? ---"
echo "<b>bold</b> and <i>italic</i>" | grep -oP "<.+>"
echo "(greedy: matchea todo desde el primer < hasta el ultimo >)"
echo ""
echo "<b>bold</b> and <i>italic</i>" | grep -oP "<.+?>"
echo "(non-greedy: matchea el bloque mas corto posible)"
'
```

### Paso 3.2: Lookahead y lookbehind

Antes de ejecutar, predice: que diferencia hay entre
`grep -P '\d+(?= USD)'` y `grep -P '(?<=\$)\d+'`?

```bash
docker compose exec debian-dev bash -c '
echo "=== Lookahead y lookbehind ==="
echo ""
echo "--- (?=...) Lookahead positivo ---"
echo "Precio: 100 USD y 200 EUR" | grep -oP "\\d+(?= USD)"
echo "(numeros seguidos de USD, sin incluir USD en el match)"
echo ""
echo "--- (?!...) Lookahead negativo ---"
echo "Precio: 100 USD y 200 EUR" | grep -oP "\\d+(?! USD)\\b"
echo "(numeros que NO estan seguidos de USD)"
echo ""
echo "--- (?<=...) Lookbehind positivo ---"
echo "Total: \$100 y \$200" | grep -oP "(?<=\\$)\\d+"
echo "(numeros precedidos por \$, sin incluir \$ en el match)"
echo ""
echo "--- (?<!...) Lookbehind negativo ---"
echo "100px 200em 300px" | grep -oP "\\d+(?!px)\\w+"
echo "(unidades que NO son px)"
echo ""
echo "Lookahead/lookbehind NO consumen caracteres del match"
echo "Solo verifican que la condicion se cumple"
'
```

### Paso 3.3: Patrones comunes

```bash
docker compose exec debian-dev bash -c '
echo "=== Patrones comunes ==="
echo ""
cat > /tmp/test-data.txt << '\''EOF'\''
user@example.com is valid
not-an-email is invalid
IP: 192.168.1.100
IP: 10.0.0.1
IP: 999.999.999.999
URL: https://example.com/path?q=1
URL: http://test.org
Port: 8080
Port: 99999
EOF
echo "--- Extraer emails ---"
grep -oP "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}" /tmp/test-data.txt
echo ""
echo "--- Extraer IPs ---"
grep -oP "\\b\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\b" /tmp/test-data.txt
echo "(nota: matchea 999.999.999.999 — validar octetos requiere logica extra)"
echo ""
echo "--- Extraer URLs ---"
grep -oP "https?://[^\\s]+" /tmp/test-data.txt
echo ""
echo "--- Extraer puertos validos (1-65535) ---"
grep -oP "Port: \\K\\d+" /tmp/test-data.txt
echo "(\\K descarta lo que esta antes del match — solo muestra el numero)"
echo ""
rm /tmp/test-data.txt
echo ""
echo "--- Rendimiento ---"
echo "  grep -F:     string fijo (mas rapido)"
echo "  LC_ALL=C:    locale C (mas rapido que UTF-8)"
echo "  ripgrep (rg): optimizado para busqueda en codigo"
echo "  -m N:        detenerse despues de N matches"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. **BRE** (default) necesita escapar `+`, `?`, `|`. **ERE**
   (`-E`) no. **PCRE** (`-P`) soporta `\d`, `\w`, `\b`,
   lookahead/lookbehind. Usar `-E` por defecto, `-P` para
   patrones avanzados.

2. `-o` extrae solo el match, `-w` matchea palabras completas,
   `-q` retorna solo exit code (ideal en `if`), `-c` cuenta
   matches, `-l`/`-L` listan archivos con/sin match.

3. Contexto: `-A N` (after), `-B N` (before), `-C N` (context).
   Fundamentales para revisar logs donde un error necesita
   las lineas circundantes.

4. `-r` busca recursivamente. `--include="*.ext"` filtra por
   tipo, `--exclude-dir=.git` salta directorios. `-I` ignora
   binarios.

5. PCRE **lookahead** `(?=...)` y **lookbehind** `(?<=...)`
   verifican condiciones sin consumir caracteres. `\K` descarta
   el match previo. `?` despues de cuantificador = non-greedy.

6. Para AND logico usar pipeline (`grep A | grep B`) o PCRE
   (`grep -P '(?=.*A)(?=.*B)'`). Para rendimiento en archivos
   grandes: `-F` (literal), `LC_ALL=C`, o ripgrep.
