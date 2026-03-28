# Lab — Variables y expansiones

## Objetivo

Dominar las expansiones de parametros de bash: valores por defecto,
manipulacion de strings (longitud, subcadenas, patrones, reemplazo,
cambio de caso), aritmetica con $(( )), y sustitucion de comandos
con $().

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Defaults y asignacion

### Objetivo

Usar expansiones para manejar variables no definidas o vacias
de forma segura.

### Paso 1.1: ${var:-default} — valor por defecto

```bash
docker compose exec debian-dev bash -c '
echo "=== \${var:-default} ==="
echo ""
echo "Usa un valor alternativo si la variable no existe o esta vacia"
echo "La variable NO se modifica"
echo ""
unset NAME
echo "NAME no definida: ${NAME:-anonimo}"
echo "NAME sigue sin definir: ${NAME:-todavia_anonimo}"
echo ""
NAME=""
echo "NAME vacia: ${NAME:-anonimo}"
echo ""
NAME="Carlos"
echo "NAME definida: ${NAME:-anonimo}"
echo ""
echo "Uso tipico:"
echo "  LOG_LEVEL=\${LOG_LEVEL:-info}"
echo "  OUTPUT=\${1:-/dev/stdout}"
'
```

### Paso 1.2: ${var:=default} — asignar si no existe

```bash
docker compose exec debian-dev bash -c '
echo "=== \${var:=default} ==="
echo ""
echo "Como :- pero ADEMAS asigna el valor a la variable"
echo ""
unset COLOR
echo "COLOR: ${COLOR:=azul}"
echo "COLOR ahora vale: $COLOR"
echo ""
echo "Si ya tiene valor, no cambia:"
echo "COLOR: ${COLOR:=rojo}"
echo "COLOR sigue siendo: $COLOR"
echo ""
echo "NOTA: no funciona con \$1, \$2 (posicionales)"
echo "  \${1:=default}  → error"
'
```

### Paso 1.3: ${var:+alternate} — valor si existe

```bash
docker compose exec debian-dev bash -c '
echo "=== \${var:+alternate} ==="
echo ""
echo "Devuelve el valor alternativo SOLO si la variable existe y no esta vacia"
echo "Es el inverso de :-"
echo ""
unset DEBUG
echo "DEBUG no definida: [${DEBUG:+--verbose}]"
echo ""
DEBUG=1
echo "DEBUG=1: [${DEBUG:+--verbose}]"
echo ""
echo "Uso tipico: agregar flags condicionalmente"
echo "  cmd \${VERBOSE:+--verbose} \${DRY_RUN:+--dry-run} archivo.txt"
'
```

### Paso 1.4: ${var:?error} — error si no existe

```bash
docker compose exec debian-dev bash -c '
echo "=== \${var:?error} ==="
echo ""
echo "Termina el script con un mensaje de error si la variable"
echo "no esta definida o esta vacia"
echo ""
echo "--- Con variable definida ---"
DB_HOST="localhost"
echo "DB_HOST: ${DB_HOST:?Variable DB_HOST requerida}"
echo ""
echo "--- Con variable no definida (dentro de subshell para no salir) ---"
(
    unset DB_HOST
    echo "Intentando usar DB_HOST..."
    echo "${DB_HOST:?ERROR: Variable DB_HOST no definida}" 2>&1
) || true
echo ""
echo "Uso tipico al inicio de scripts:"
echo "  : \${API_KEY:?Falta API_KEY}"
echo "  : \${DB_HOST:?Falta DB_HOST}"
echo "  (: es el comando nulo, solo evalua la expansion)"
'
```

### Paso 1.5: Con y sin : (dos puntos)

```bash
docker compose exec debian-dev bash -c '
echo "=== Con : vs sin : ==="
echo ""
echo "Sin : solo verifica si esta definida (incluso vacia)"
echo "Con : verifica definida Y no vacia"
echo ""
EMPTY=""
unset NOEXIST
echo "--- \${var-default} (sin :) ---"
echo "EMPTY (vacia): [${EMPTY-default}]     → vacia (esta definida)"
echo "NOEXIST: [${NOEXIST-default}]           → default (no definida)"
echo ""
echo "--- \${var:-default} (con :) ---"
echo "EMPTY (vacia): [${EMPTY:-default}]    → default (vacia cuenta)"
echo "NOEXIST: [${NOEXIST:-default}]          → default"
echo ""
echo "Casi siempre se usa la version con : (mas segura)"
'
```

---

## Parte 2 — Manipulacion de strings

### Objetivo

Usar expansiones de bash para manipular strings sin llamar a
comandos externos como sed, awk, o cut.

### Paso 2.1: Longitud y subcadenas

```bash
docker compose exec debian-dev bash -c '
echo "=== Longitud y subcadenas ==="
echo ""
FILE="/home/user/documents/report.tar.gz"
echo "FILE=$FILE"
echo ""
echo "--- Longitud ---"
echo "\${#FILE} = ${#FILE}"
echo ""
echo "--- Subcadenas \${var:offset:length} ---"
echo "\${FILE:0:5}  = ${FILE:0:5}"
echo "\${FILE:6:4}  = ${FILE:6:4}"
echo "\${FILE:(-6)} = ${FILE:(-6)}"
echo ""
echo "Los parentesis en (-6) evitan confusion con :- (default)"
'
```

### Paso 2.2: Eliminar prefijos y sufijos

Antes de ejecutar, predice: dado `FILE="/home/user/docs/report.tar.gz"`,
que devuelve `${FILE##*/}` y `${FILE%%/*}`?

```bash
docker compose exec debian-dev bash -c '
echo "=== Eliminar prefijos y sufijos ==="
echo ""
FILE="/home/user/docs/report.tar.gz"
echo "FILE=$FILE"
echo ""
echo "--- Eliminar prefijo (desde el inicio) ---"
echo "\${FILE#*/}   = ${FILE#*/}"
echo "  (# = el match mas corto desde el inicio)"
echo "\${FILE##*/}  = ${FILE##*/}"
echo "  (## = el match mas largo desde el inicio → basename)"
echo ""
echo "--- Eliminar sufijo (desde el final) ---"
echo "\${FILE%.*}   = ${FILE%.*}"
echo "  (% = el match mas corto desde el final → quita .gz)"
echo "\${FILE%%.*}  = ${FILE%%.*}"
echo "  (%% = el match mas largo desde el final → quita todo desde el primer .)"
echo ""
echo "=== Patrones utiles ==="
echo "Basename: \${FILE##*/}  = ${FILE##*/}"
echo "Dirname:  \${FILE%/*}   = ${FILE%/*}"
echo "Extension: \${FILE##*.} = ${FILE##*.}"
echo "Sin ext:   \${FILE%.*}  = ${FILE%.*}"
'
```

### Paso 2.3: Reemplazo de patrones

```bash
docker compose exec debian-dev bash -c '
echo "=== Reemplazo \${var/patron/reemplazo} ==="
echo ""
TEXT="hello world hello bash"
echo "TEXT=$TEXT"
echo ""
echo "--- Primera ocurrencia ---"
echo "\${TEXT/hello/HOLA} = ${TEXT/hello/HOLA}"
echo ""
echo "--- Todas las ocurrencias ---"
echo "\${TEXT//hello/HOLA} = ${TEXT//hello/HOLA}"
echo ""
echo "--- Reemplazar al inicio ---"
echo "\${TEXT/#hello/HOLA} = ${TEXT/#hello/HOLA}"
echo ""
echo "--- Reemplazar al final ---"
echo "\${TEXT/%bash/SHELL} = ${TEXT/%bash/SHELL}"
echo ""
echo "--- Eliminar (reemplazar con nada) ---"
echo "\${TEXT// /} = ${TEXT// /}"
echo ""
echo "=== Uso practico ==="
PATH_WIN="C:\\Users\\admin\\docs"
echo "Windows: $PATH_WIN"
echo "Linux:   ${PATH_WIN//\\\\//}"
'
```

### Paso 2.4: Cambio de caso

```bash
docker compose exec debian-dev bash -c '
echo "=== Cambio de caso (bash 4+) ==="
echo ""
NAME="Hello World"
echo "NAME=$NAME"
echo ""
echo "--- A minusculas ---"
echo "\${NAME,}  = ${NAME,}"
echo "  (, = solo el primer caracter)"
echo "\${NAME,,} = ${NAME,,}"
echo "  (,, = todos los caracteres)"
echo ""
echo "--- A mayusculas ---"
echo "\${NAME^}  = ${NAME^}"
echo "  (^ = solo el primer caracter)"
echo "\${NAME^^} = ${NAME^^}"
echo "  (^^ = todos los caracteres)"
echo ""
echo "=== Uso practico ==="
INPUT="yes"
echo "Normalizar input del usuario:"
echo "  INPUT=yes → \${INPUT,,} = ${INPUT,,}"
INPUT="YES"
echo "  INPUT=YES → \${INPUT,,} = ${INPUT,,}"
INPUT="Yes"
echo "  INPUT=Yes → \${INPUT,,} = ${INPUT,,}"
echo "  Comparar siempre en minusculas: [[ \"\${INPUT,,}\" == \"yes\" ]]"
'
```

---

## Parte 3 — Aritmetica y sustitucion

### Objetivo

Usar $(( )) para aritmetica, $() para sustitucion de comandos,
y entender la diferencia entre $@ y $*.

### Paso 3.1: Aritmetica con $(( ))

```bash
docker compose exec debian-dev bash -c '
echo "=== Aritmetica \$(( )) ==="
echo ""
A=10
B=3
echo "A=$A B=$B"
echo ""
echo "Suma:      \$((A + B))  = $((A + B))"
echo "Resta:     \$((A - B))  = $((A - B))"
echo "Multiplic: \$((A * B))  = $((A * B))"
echo "Division:  \$((A / B))  = $((A / B)) (entera, trunca)"
echo "Modulo:    \$((A % B))  = $((A % B))"
echo "Potencia:  \$((A ** 2)) = $((A ** 2))"
echo ""
echo "--- Incremento/decremento ---"
X=5
echo "X=$X"
echo "\$((X++)) = $((X++)) (post: devuelve 5, luego incrementa)"
echo "X ahora = $X"
echo "\$((++X)) = $((++X)) (pre: incrementa primero, devuelve 7)"
echo ""
echo "--- No necesita \$ dentro de (( )) ---"
echo "\$((A + B)) es igual que \$((\$A + \$B))"
echo "Dentro de (( )), bash resuelve los nombres automaticamente"
'
```

### Paso 3.2: Sustitucion de comandos

```bash
docker compose exec debian-dev bash -c '
echo "=== Sustitucion de comandos \$() ==="
echo ""
echo "--- Capturar salida de un comando ---"
TODAY=$(date +%Y-%m-%d)
echo "Hoy es: $TODAY"
echo ""
USERS=$(wc -l < /etc/passwd)
echo "Usuarios en /etc/passwd: $USERS"
echo ""
echo "--- Anidar ---"
echo "Directorio de bash: $(dirname $(which bash))"
echo ""
echo "--- En asignaciones ---"
HOSTNAME_UPPER=$(hostname | tr "[:lower:]" "[:upper:]")
echo "Hostname en mayusculas: $HOSTNAME_UPPER"
echo ""
echo "--- Backticks (obsoleto) ---"
echo "Antes: DATE=\`date\`"
echo "Ahora: DATE=\$(date)"
echo "Backticks no se anidan facilmente y son menos legibles"
'
```

### Paso 3.3: $@ vs $*

```bash
docker compose exec debian-dev bash -c '
echo "=== \$@ vs \$* ==="
echo ""
cat > /tmp/args-test.sh << '\''SCRIPT'\''
#!/bin/bash
echo "=== Con \"\$@\" (cada argumento separado) ==="
i=1
for arg in "$@"; do
    echo "  [$i]: $arg"
    ((i++))
done

echo ""
echo "=== Con \"\$*\" (todo como un solo string) ==="
i=1
for arg in "$*"; do
    echo "  [$i]: $arg"
    ((i++))
done

echo ""
echo "=== Sin comillas \$@ (word splitting!) ==="
i=1
for arg in $@; do
    echo "  [$i]: $arg"
    ((i++))
done
SCRIPT
chmod +x /tmp/args-test.sh
echo "--- Ejecutar con argumentos que tienen espacios ---"
echo "  /tmp/args-test.sh \"hello world\" \"foo bar\" baz"
echo ""
/tmp/args-test.sh "hello world" "foo bar" "baz"
echo ""
echo "REGLA: siempre usar \"\$@\" (con comillas)"
echo "  \"\$@\" preserva los argumentos como fueron pasados"
echo "  \"\$*\" los concatena en un solo string"
echo "  \$@ sin comillas rompe argumentos con espacios"
'
```

### Paso 3.4: Variables especiales

```bash
docker compose exec debian-dev bash -c '
echo "=== Variables especiales ==="
echo ""
echo "\$\$    PID del shell actual:     $$"
echo "\$!    PID del ultimo background: ${!:-ninguno}"
echo "\$0    Nombre del script:        $0"
echo "\$#    Numero de argumentos:     $#"
echo "\$?    Exit code anterior:       $?"
echo ""
echo "--- \$RANDOM ---"
echo "Numero aleatorio: $RANDOM (0-32767)"
echo "Dado (1-6): $(( RANDOM % 6 + 1 ))"
echo ""
echo "--- \$SECONDS ---"
echo "Segundos desde inicio del shell: $SECONDS"
echo ""
echo "--- \$LINENO ---"
echo "Linea actual del script: $LINENO"
echo "(en la terminal interactiva, cuenta lineas del input)"
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/args-test.sh
echo "Archivos temporales eliminados"
'
```

---

## Conceptos reforzados

1. `${var:-default}` provee un valor alternativo sin modificar la
   variable. `${var:=default}` ademas asigna. Son fundamentales
   para valores por defecto en scripts.

2. `${var:?error}` termina el script si la variable no esta
   definida — util para validar variables requeridas al inicio.

3. `${var##*/}` (basename) y `${var%.*}` (quitar extension) son
   expansiones de patron que reemplazan comandos externos como
   `basename` y `dirname`.

4. `${var//patron/reemplazo}` reemplaza todas las ocurrencias en
   un string. `${var,,}` y `${var^^}` cambian caso — utiles para
   normalizar input del usuario.

5. `$(( ))` hace aritmetica entera. No necesita `$` dentro de los
   parentesis. La division trunca (no hay punto flotante en bash).

6. Siempre usar `"$@"` (con comillas) para pasar argumentos.
   `"$*"` concatena todo en un string. Sin comillas, el word
   splitting rompe argumentos con espacios.
