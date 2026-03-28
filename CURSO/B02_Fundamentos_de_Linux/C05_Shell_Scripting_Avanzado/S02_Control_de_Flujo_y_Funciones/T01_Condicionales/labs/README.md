# Lab — Condicionales

## Objetivo

Dominar las condicionales en bash: file tests (-f, -d, -L, -r, -w,
-x, -s, -nt, -ot, -ef), comparaciones de strings (-z, -n, ==, !=,
=~), comparaciones numericas (-eq, -lt, -gt, (( ))), if/elif/else,
y case con patrones, fall-through (;&) y continue (;;&).

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — File tests

### Objetivo

Usar file tests para verificar existencia, tipo y permisos de
archivos y directorios.

### Paso 1.1: Tests de existencia y tipo

```bash
docker compose exec debian-dev bash -c '
echo "=== File tests de existencia y tipo ==="
echo ""
echo "--- Preparar archivos de prueba ---"
touch /tmp/test-file.txt
mkdir -p /tmp/test-dir
ln -sf /tmp/test-file.txt /tmp/test-link
echo ""
echo "--- -f: es un archivo regular ---"
[[ -f /tmp/test-file.txt ]] && echo "-f /tmp/test-file.txt: SI"
[[ -f /tmp/test-dir ]] && echo "-f test-dir: SI" || echo "-f /tmp/test-dir: NO (es directorio)"
[[ -f /tmp/test-link ]] && echo "-f /tmp/test-link: SI (sigue el symlink)"
echo ""
echo "--- -d: es un directorio ---"
[[ -d /tmp/test-dir ]] && echo "-d /tmp/test-dir: SI"
[[ -d /tmp/test-file.txt ]] && echo "-d test-file: SI" || echo "-d /tmp/test-file.txt: NO"
echo ""
echo "--- -L / -h: es un symlink ---"
[[ -L /tmp/test-link ]] && echo "-L /tmp/test-link: SI"
[[ -L /tmp/test-file.txt ]] && echo "-L test-file: SI" || echo "-L /tmp/test-file.txt: NO"
echo ""
echo "--- -e: existe (cualquier tipo) ---"
[[ -e /tmp/test-file.txt ]] && echo "-e /tmp/test-file.txt: SI"
[[ -e /tmp/no-existe ]] && echo "-e no-existe: SI" || echo "-e /tmp/no-existe: NO"
echo ""
rm -rf /tmp/test-file.txt /tmp/test-dir /tmp/test-link
'
```

### Paso 1.2: Tests de permisos

```bash
docker compose exec debian-dev bash -c '
echo "=== File tests de permisos ==="
echo ""
echo "test" > /tmp/perm-test.txt
echo ""
echo "--- Permisos del archivo ---"
ls -l /tmp/perm-test.txt
echo ""
echo "--- -r: legible ---"
[[ -r /tmp/perm-test.txt ]] && echo "-r: SI, legible"
echo ""
echo "--- -w: escribible ---"
[[ -w /tmp/perm-test.txt ]] && echo "-w: SI, escribible"
echo ""
echo "--- -x: ejecutable ---"
[[ -x /tmp/perm-test.txt ]] && echo "-x: SI" || echo "-x: NO, no ejecutable"
chmod +x /tmp/perm-test.txt
[[ -x /tmp/perm-test.txt ]] && echo "-x (despues de chmod +x): SI"
echo ""
echo "--- -s: tamano mayor que cero ---"
[[ -s /tmp/perm-test.txt ]] && echo "-s: SI, tiene contenido"
> /tmp/perm-empty.txt
[[ -s /tmp/perm-empty.txt ]] && echo "-s empty: SI" || echo "-s empty: NO, esta vacio"
echo ""
rm -f /tmp/perm-test.txt /tmp/perm-empty.txt
'
```

### Paso 1.3: Tests de comparacion de archivos

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparacion de archivos ==="
echo ""
echo "first" > /tmp/old-file.txt
sleep 1
echo "second" > /tmp/new-file.txt
ln /tmp/new-file.txt /tmp/hardlink-file.txt
echo ""
echo "--- -nt: mas nuevo que ---"
[[ /tmp/new-file.txt -nt /tmp/old-file.txt ]] && echo "new-file es mas nuevo que old-file"
echo ""
echo "--- -ot: mas viejo que ---"
[[ /tmp/old-file.txt -ot /tmp/new-file.txt ]] && echo "old-file es mas viejo que new-file"
echo ""
echo "--- -ef: mismo archivo (mismo inode) ---"
[[ /tmp/new-file.txt -ef /tmp/hardlink-file.txt ]] && echo "new-file y hardlink son el mismo inode"
echo ""
ls -li /tmp/new-file.txt /tmp/hardlink-file.txt
echo "(mismo numero de inode)"
echo ""
rm -f /tmp/old-file.txt /tmp/new-file.txt /tmp/hardlink-file.txt
'
```

---

## Parte 2 — Strings y numeros

### Objetivo

Comparar strings y numeros correctamente en bash.

### Paso 2.1: Comparaciones de strings

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparaciones de strings ==="
echo ""
A="hello"
B="world"
EMPTY=""
echo "A=$A B=$B EMPTY=\"$EMPTY\""
echo ""
echo "--- Igualdad ---"
[[ "$A" == "hello" ]] && echo "A == hello: SI"
[[ "$A" == "$B" ]] && echo "A == B: SI" || echo "A == B: NO"
echo ""
echo "--- Desigualdad ---"
[[ "$A" != "$B" ]] && echo "A != B: SI"
echo ""
echo "--- Vacio / no vacio ---"
[[ -z "$EMPTY" ]] && echo "-z EMPTY: SI (esta vacia)"
[[ -z "$A" ]] && echo "-z A: SI" || echo "-z A: NO (tiene contenido)"
[[ -n "$A" ]] && echo "-n A: SI (no esta vacia)"
[[ -n "$EMPTY" ]] && echo "-n EMPTY: SI" || echo "-n EMPTY: NO"
echo ""
echo "--- Orden lexicografico ---"
[[ "apple" < "banana" ]] && echo "apple < banana: SI"
[[ "zebra" > "apple" ]] && echo "zebra > apple: SI"
echo "(< y > son lexicograficos, NO numericos)"
'
```

### Paso 2.2: Pattern matching y regex

```bash
docker compose exec debian-dev bash -c '
echo "=== Pattern matching con == en [[ ]] ==="
echo ""
FILE="report-2024.tar.gz"
echo "FILE=$FILE"
echo ""
echo "--- Glob patterns ---"
[[ "$FILE" == *.gz ]] && echo "Termina en .gz: SI"
[[ "$FILE" == report* ]] && echo "Empieza con report: SI"
[[ "$FILE" == *2024* ]] && echo "Contiene 2024: SI"
[[ "$FILE" == *.txt ]] && echo "Termina en .txt: SI" || echo "Termina en .txt: NO"
echo ""
echo "=== Regex con =~ ==="
echo ""
IP="192.168.1.100"
echo "IP=$IP"
if [[ "$IP" =~ ^([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})$ ]]; then
    echo "Es un formato de IP valido"
    echo "BASH_REMATCH[0] (completo): ${BASH_REMATCH[0]}"
    echo "BASH_REMATCH[1] (octeto 1): ${BASH_REMATCH[1]}"
    echo "BASH_REMATCH[2] (octeto 2): ${BASH_REMATCH[2]}"
    echo "BASH_REMATCH[3] (octeto 3): ${BASH_REMATCH[3]}"
    echo "BASH_REMATCH[4] (octeto 4): ${BASH_REMATCH[4]}"
fi
echo ""
echo "=~ usa regex extendida (ERE)"
echo "BASH_REMATCH[] captura los grupos ()"
'
```

### Paso 2.3: Comparaciones numericas

Antes de ejecutar, predice: que pasa con `[[ "9" > "10" ]]`
(comparacion de string) vs `(( 9 > 10 ))` (aritmetica)?

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparaciones numericas ==="
echo ""
A=42
B=100
echo "A=$A B=$B"
echo ""
echo "--- Con [[ ]] (operadores de string para numeros) ---"
[[ $A -eq 42 ]] && echo "A -eq 42: SI"
[[ $A -lt $B ]] && echo "A -lt B: SI ($A < $B)"
[[ $A -gt $B ]] && echo "A -gt B: SI" || echo "A -gt B: NO"
[[ $A -le 42 ]] && echo "A -le 42: SI (<=)"
[[ $A -ge 42 ]] && echo "A -ge 42: SI (>=)"
[[ $A -ne $B ]] && echo "A -ne B: SI (!=)"
echo ""
echo "--- Con (( )) (C-style, mas legible) ---"
(( A == 42 )) && echo "A == 42: SI"
(( A < B )) && echo "A < B: SI"
(( A > 0 && A < 100 )) && echo "A entre 1 y 99: SI"
echo ""
echo "--- PELIGRO: < > con strings vs numeros ---"
[[ "9" > "10" ]] && echo '\"9\" > \"10\" (string): SI (9 > 1 lexicograficamente)'
(( 9 > 10 )) && echo "9 > 10 (numerico): SI" || echo "9 > 10 (numerico): NO"
echo ""
echo "REGLA: para numeros, siempre usar -eq/-lt/-gt o (( ))"
echo "       NUNCA usar < > == para comparar numeros en [[ ]]"
'
```

### Paso 2.4: if/elif/else

```bash
docker compose exec debian-dev bash -c '
echo "=== if/elif/else ==="
echo ""
check_age() {
    local age=$1
    if (( age < 0 )); then
        echo "Edad $age: invalida"
    elif (( age < 13 )); then
        echo "Edad $age: nino"
    elif (( age < 18 )); then
        echo "Edad $age: adolescente"
    elif (( age < 65 )); then
        echo "Edad $age: adulto"
    else
        echo "Edad $age: senior"
    fi
}

check_age -1
check_age 5
check_age 15
check_age 30
check_age 70
echo ""
echo "--- if con comandos (no solo [[ ]]) ---"
if grep -q "root" /etc/passwd; then
    echo "El usuario root existe en /etc/passwd"
fi
echo ""
echo "if evalua el EXIT CODE del comando"
echo "No solo [[ ]] — cualquier comando funciona como condicion"
'
```

---

## Parte 3 — case

### Objetivo

Usar case para multiple branching con patrones, incluyendo
fall-through (;&) y continue testing (;;&).

### Paso 3.1: case basico

```bash
docker compose exec debian-dev bash -c '
echo "=== case basico ==="
echo ""
classify_file() {
    local file="$1"
    case "$file" in
        *.tar.gz|*.tgz)
            echo "$file: archivo tar comprimido con gzip"
            ;;
        *.tar.bz2|*.tbz2)
            echo "$file: archivo tar comprimido con bzip2"
            ;;
        *.tar.xz|*.txz)
            echo "$file: archivo tar comprimido con xz"
            ;;
        *.zip)
            echo "$file: archivo zip"
            ;;
        *.sh)
            echo "$file: script de shell"
            ;;
        *)
            echo "$file: tipo desconocido"
            ;;
    esac
}

classify_file "backup.tar.gz"
classify_file "data.tar.xz"
classify_file "docs.zip"
classify_file "deploy.sh"
classify_file "image.png"
echo ""
echo "Cada patron puede usar | para alternativas"
echo ";; termina el bloque (no cae al siguiente)"
'
```

### Paso 3.2: Patrones en case

```bash
docker compose exec debian-dev bash -c '
echo "=== Patrones avanzados en case ==="
echo ""
process_input() {
    local input="$1"
    case "$input" in
        [Yy]|[Yy][Ee][Ss])
            echo "\"$input\" → respuesta afirmativa"
            ;;
        [Nn]|[Nn][Oo])
            echo "\"$input\" → respuesta negativa"
            ;;
        [0-9])
            echo "\"$input\" → un digito"
            ;;
        [0-9][0-9])
            echo "\"$input\" → dos digitos"
            ;;
        "")
            echo "(vacio) → sin input"
            ;;
        *)
            echo "\"$input\" → no reconocido"
            ;;
    esac
}

process_input "yes"
process_input "Y"
process_input "NO"
process_input "n"
process_input "5"
process_input "42"
process_input ""
process_input "maybe"
echo ""
echo "[Yy] = Y o y"
echo "[0-9] = cualquier digito"
echo "| = alternativa (OR)"
'
```

### Paso 3.3: ;& (fall-through) y ;;& (continue testing)

Antes de ejecutar, predice: con `;&`, si el primer patron matchea,
que pasa con los siguientes bloques?

```bash
docker compose exec debian-dev bash -c '
echo "=== ;& (fall-through) y ;;& (continue testing) ==="
echo ""
echo "--- ;; (normal): sale despues del match ---"
echo "--- ;& (fall-through): ejecuta el SIGUIENTE bloque sin testear ---"
echo "--- ;;& (continue): sigue testeando los siguientes patrones ---"
echo ""
echo "=== ;& ejemplo: niveles acumulativos ==="
log_level() {
    local level="$1"
    echo "Con nivel $level se registra:"
    case "$level" in
        debug)
            echo "  - DEBUG"
            ;&
        info)
            echo "  - INFO"
            ;&
        warning)
            echo "  - WARNING"
            ;&
        error)
            echo "  - ERROR"
            ;;
    esac
}

log_level "debug"
echo ""
log_level "warning"
echo ""
log_level "error"
echo ""
echo "=== ;;& ejemplo: multiples categorias ==="
categorize() {
    local char="$1"
    echo -n "\"$char\" es: "
    case "$char" in
        [0-9])
            echo -n "digito "
            ;;&
        [a-z])
            echo -n "minuscula "
            ;;&
        [A-Z])
            echo -n "mayuscula "
            ;;&
        [aeiouAEIOU])
            echo -n "vocal "
            ;;&
        [a-zA-Z0-9])
            echo -n "alfanumerico "
            ;;
    esac
    echo ""
}

categorize "a"
categorize "B"
categorize "5"
categorize "z"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Los **file tests** verifican existencia (`-e`), tipo
   (`-f`, `-d`, `-L`), permisos (`-r`, `-w`, `-x`), contenido
   (`-s`), y comparacion temporal (`-nt`, `-ot`, `-ef`).
   `-f` y `-e` siguen symlinks; `-L` prueba el symlink mismo.

2. En `[[ ]]`, las **strings** se comparan con `==`/`!=`
   (pattern matching), `<`/`>` (lexicografico), `-z`/`-n`
   (vacio/no vacio), y `=~` (regex con BASH_REMATCH).

3. Para **numeros**, usar `-eq`/`-lt`/`-gt` en `[[ ]]` o
   preferiblemente `(( ))` para C-style. `<`/`>` en `[[ ]]`
   son lexicograficos — `"9" > "10"` es true.

4. `if` evalua el **exit code** de cualquier comando, no solo
   `[[ ]]`. `grep -q`, `command -v`, funciones — todo sirve
   como condicion.

5. `case` soporta patrones glob (`*`, `?`, `[...]`), alternativas
   con `|`, y es la forma idomiatica de hacer multiple branching.

6. `;&` (fall-through) ejecuta el siguiente bloque sin testear.
   `;;&` (continue testing) sigue evaluando los siguientes
   patrones. Ambos son especificos de bash 4+.
