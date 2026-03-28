# Lab — Loops avanzados

## Objetivo

Dominar las variantes de loops en bash: for con listas, brace
expansion {1..10..2}, C-style for (( )), iteracion de arrays,
while read con IFS= -r, el problema del pipeline subshell,
process substitution <() >(), until, break/continue con nivel N,
find -print0 con read -d '', y globstar.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Variantes de for

### Objetivo

Conocer las distintas formas de escribir un for loop en bash.

### Paso 1.1: for clasico con listas

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

### Paso 1.2: Brace expansion

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
echo "Cuenta atras:"
for i in {5..1}; do echo -n "  $i"; done
echo " ... lanzamiento!"
echo ""
echo "--- Letras ---"
echo "Abecedario:"
for c in {a..z}; do echo -n "$c "; done
echo ""
echo ""
echo "--- Combinar ---"
echo "Archivos:"
for f in file{1..3}.{txt,log}; do echo "  $f"; done
echo ""
echo "NOTA: brace expansion ocurre ANTES de variable expansion"
echo "  N=5; {1..\$N}  NO funciona (es literal)"
echo "  Usar C-style for o seq para rangos dinamicos"
'
```

### Paso 1.3: C-style for

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

### Paso 1.4: Iterar arrays

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

---

## Parte 2 — while read y process substitution

### Objetivo

Leer input linea por linea de forma segura y entender el problema
del pipeline subshell.

### Paso 2.1: while read basico

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

### Paso 2.2: El problema del pipeline subshell

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

### Paso 2.3: Process substitution <() y >()

```bash
docker compose exec debian-dev bash -c '
echo "=== Process substitution ==="
echo ""
echo "--- <() crea un pseudo-archivo con la salida del comando ---"
echo "diff <(cmd1) <(cmd2): comparar salidas de dos comandos"
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

### Paso 2.4: until

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
echo "--- Esperar un archivo ---"
echo "Patron tipico (con timeout):"
echo "  TIMEOUT=30"
echo "  until [[ -f /tmp/ready.flag ]] || (( TIMEOUT <= 0 )); do"
echo "    sleep 1"
echo "    ((TIMEOUT--))"
echo "  done"
echo ""
echo "until se usa poco — while con condicion negada es mas comun"
'
```

---

## Parte 3 — Control de flujo avanzado

### Objetivo

Usar break/continue con niveles, find -print0 para nombres
seguros, y globstar para busqueda recursiva.

### Paso 3.1: break y continue con nivel N

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

### Paso 3.2: find -print0 con read -d ''

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
echo "--- CORRECTO: find -print0 + while read -d '\'''\'' ---"
while IFS= read -r -d "" f; do
    echo "  [$(basename "$f")]"
done < <(find /tmp/lab-find -type f -print0)
echo ""
echo "-print0: separa archivos con null (\\0) en vez de newline"
echo "-d '\'''\''':  read usa null como delimitador"
echo "IFS=: preserva espacios"
echo "-r: no interpreta backslashes"
echo ""
rm -rf /tmp/lab-find
'
```

### Paso 3.3: Globstar

```bash
docker compose exec debian-dev bash -c '
echo "=== Globstar (**) ==="
echo ""
echo "--- Sin globstar: * solo matchea en el directorio actual ---"
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

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. **Brace expansion** `{1..10..2}` genera secuencias en tiempo
   de expansion, antes de variables. Para rangos dinamicos
   usar C-style `for ((i=0; i<N; i++))`.

2. `while IFS=: read -r` es la forma correcta de leer archivos
   linea por linea. `IFS=` preserva espacios, `-r` evita
   interpretar backslashes.

3. Los **pipes crean subshells**: variables modificadas dentro
   de `cmd | while read` no persisten. La solucion es process
   substitution: `while read ... done < <(cmd)`.

4. `<()` y `>()` crean pseudo-archivos con la salida/entrada
   de comandos. `diff <(cmd1) <(cmd2)` compara salidas sin
   archivos temporales.

5. `break N` y `continue N` controlan loops anidados. N=2
   afecta al loop exterior (segundo nivel).

6. Para nombres de archivo con espacios o newlines, usar
   `find -print0 | while IFS= read -r -d '' f`. El separador
   null (\0) es el unico caracter que no puede aparecer en un
   nombre de archivo.
