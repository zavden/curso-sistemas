# Lab — Exit codes

## Objetivo

Entender los codigos de salida ($?), las convenciones estandar
(0/1/2/126/127/128+N/130), el strict mode (set -euo pipefail),
sus excepciones, PIPESTATUS, y patrones para manejar errores
en scripts.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Exit codes y convenciones

### Objetivo

Entender como funcionan los exit codes en Linux y las convenciones
de valores.

### Paso 1.1: $? — El exit code del ultimo comando

```bash
docker compose exec debian-dev bash -c '
echo "=== \$? — exit code del ultimo comando ==="
echo ""
echo "--- Comando exitoso ---"
true
echo "Exit code de true: $?"
echo ""
echo "--- Comando fallido ---"
false
echo "Exit code de false: $?"
echo ""
echo "--- grep encuentra vs no encuentra ---"
echo "hello" | grep -q "hello"
echo "grep encontro: $?"
echo "hello" | grep -q "world"
echo "grep no encontro: $?"
echo ""
echo "Convencion: 0 = exito, distinto de 0 = error"
echo "Un exit code es un byte: 0-255"
'
```

### Paso 1.2: Convenciones de exit codes

```bash
docker compose exec debian-dev bash -c '
echo "=== Convenciones de exit codes ==="
echo ""
printf "%-8s %s\n" "Codigo" "Significado"
printf "%-8s %s\n" "-------" "-------------------------------------------"
printf "%-8s %s\n" "0" "Exito"
printf "%-8s %s\n" "1" "Error general"
printf "%-8s %s\n" "2" "Uso incorrecto (argumentos invalidos)"
printf "%-8s %s\n" "126" "Comando encontrado pero no ejecutable"
printf "%-8s %s\n" "127" "Comando no encontrado"
printf "%-8s %s\n" "128+N" "Terminado por signal N"
printf "%-8s %s\n" "130" "Terminado por Ctrl+C (128+2=SIGINT)"
printf "%-8s %s\n" "137" "Terminado por kill -9 (128+9=SIGKILL)"
printf "%-8s %s\n" "143" "Terminado por kill (128+15=SIGTERM)"
echo ""
echo "--- Demostrar 126 y 127 ---"
echo "test" > /tmp/not-executable.sh
/tmp/not-executable.sh 2>&1 || true
echo "Exit code (no ejecutable): $?"
echo ""
comando_inexistente_xyz 2>&1 || true
echo "Exit code (no encontrado): $?"
rm /tmp/not-executable.sh
'
```

### Paso 1.3: && y || — Encadenamiento condicional

```bash
docker compose exec debian-dev bash -c '
echo "=== && y || ==="
echo ""
echo "--- && ejecuta el siguiente SOLO si el anterior fue exitoso ---"
echo "true && echo ejecutado"
true && echo "Despues de true: SI se ejecuta"
false && echo "Despues de false: NO se ejecuta"
echo ""
echo "--- || ejecuta el siguiente SOLO si el anterior fallo ---"
true || echo "Despues de true: NO se ejecuta"
false || echo "Despues de false: SI se ejecuta"
echo ""
echo "--- Patron comun: || para fallback ---"
cd /directorio/inexistente 2>/dev/null || echo "No se pudo cambiar de directorio"
echo ""
echo "--- Patron: && ... || (pseudo ternario) ---"
FILE="/etc/passwd"
[[ -f "$FILE" ]] && echo "$FILE existe" || echo "$FILE no existe"
FILE="/no/existe"
[[ -f "$FILE" ]] && echo "$FILE existe" || echo "$FILE no existe"
echo ""
echo "CUIDADO: si el comando despues de && falla, || se ejecuta"
echo "  Para if/else real, usar if/then/else"
'
```

### Paso 1.4: exit y return

```bash
docker compose exec debian-dev bash -c '
echo "=== exit vs return ==="
echo ""
echo "--- exit: termina el script/subshell ---"
(
    echo "Dentro del subshell"
    exit 42
)
echo "Exit code del subshell: $?"
echo ""
echo "--- return: termina la funcion (no el script) ---"
check_file() {
    [[ -f "$1" ]] && return 0
    return 1
}
check_file /etc/passwd
echo "check_file /etc/passwd: $?"
check_file /no/existe
echo "check_file /no/existe: $?"
echo ""
echo "exit en una funcion termina TODO el script"
echo "return solo sale de la funcion"
'
```

---

## Parte 2 — set -euo pipefail

### Objetivo

Entender el strict mode de bash y sus excepciones.

### Paso 2.1: set -e — Salir en error

```bash
docker compose exec debian-dev bash -c '
echo "=== set -e ==="
echo ""
echo "--- Sin set -e (continua tras error) ---"
cat > /tmp/no-e.sh << '\''SCRIPT'\''
#!/bin/bash
echo "Paso 1"
false
echo "Paso 2 (se ejecuta aunque false fallo)"
comando_inexistente 2>/dev/null
echo "Paso 3 (tambien se ejecuta)"
SCRIPT
chmod +x /tmp/no-e.sh
/tmp/no-e.sh
echo ""
echo "--- Con set -e (sale en el primer error) ---"
cat > /tmp/with-e.sh << '\''SCRIPT'\''
#!/bin/bash
set -e
echo "Paso 1"
false
echo "Paso 2 (NUNCA se ejecuta)"
SCRIPT
chmod +x /tmp/with-e.sh
/tmp/with-e.sh || true
echo "El script salio en false (exit code: $?)"
'
```

### Paso 2.2: Excepciones de set -e

Antes de ejecutar, predice: con `set -e`, que pasa si `false`
aparece en un `if` o despues de `||`?

```bash
docker compose exec debian-dev bash -c '
echo "=== Excepciones de set -e ==="
echo ""
cat > /tmp/e-exceptions.sh << '\''SCRIPT'\''
#!/bin/bash
set -e

echo "--- if: no sale ---"
if false; then
    echo "no"
else
    echo "false en if no causa salida"
fi

echo "--- ||: no sale ---"
false || echo "false con || no causa salida"

echo "--- &&: no sale ---"
false && echo "no" || true

echo "--- ! (negacion): no sale ---"
! false
echo "!false no causa salida"

echo "--- Comando en pipeline: no sale (sin pipefail) ---"
false | true
echo "false en pipeline no causa salida"

echo "Todas las excepciones pasaron"
SCRIPT
chmod +x /tmp/e-exceptions.sh
/tmp/e-exceptions.sh
echo ""
echo "set -e NO sale cuando el error esta en:"
echo "  - if/elif/while condition"
echo "  - lado izquierdo de && o ||"
echo "  - comandos negados con !"
echo "  - comandos en un pipeline (excepto el ultimo, sin pipefail)"
'
```

### Paso 2.3: set -u — Variables no definidas

```bash
docker compose exec debian-dev bash -c '
echo "=== set -u ==="
echo ""
echo "--- Sin set -u: variables no definidas se expanden a vacio ---"
(
    echo "NOEXIST vale: [$NOEXIST]"
    echo "(vacio, sin error)"
) 2>/dev/null || true
echo ""
echo "--- Con set -u: error si la variable no esta definida ---"
cat > /tmp/with-u.sh << '\''SCRIPT'\''
#!/bin/bash
set -u
echo "Intentando usar variable no definida..."
echo "NOEXIST=$NOEXIST"
echo "Nunca llega aqui"
SCRIPT
chmod +x /tmp/with-u.sh
/tmp/with-u.sh 2>&1 || true
echo ""
echo "--- Valores por defecto con set -u ---"
echo "Para evitar el error, usar defaults:"
echo "  \${VAR:-default}   → valor por defecto"
echo "  \${VAR:-}           → string vacio como default"
echo "  \${1:-}             → primer argumento o vacio"
'
```

### Paso 2.4: set -o pipefail

```bash
docker compose exec debian-dev bash -c '
echo "=== set -o pipefail ==="
echo ""
echo "--- Sin pipefail: solo importa el ultimo comando ---"
false | true
echo "false | true → exit code: $? (0, solo mira true)"
echo ""
echo "--- Con pipefail: falla si CUALQUIER comando falla ---"
set -o pipefail
false | true
echo "false | true con pipefail → exit code: $?"
set +o pipefail
echo ""
echo "--- Ejemplo practico ---"
set -o pipefail
echo "Buscando en /etc/shadow (sin acceso)..."
cat /etc/shadow 2>/dev/null | grep "root" | wc -l
echo "Exit code: $? (fallo porque cat fallo)"
set +o pipefail
echo ""
echo "=== Strict mode completo ==="
echo "set -euo pipefail"
echo "  -e: salir en error"
echo "  -u: error en variables no definidas"
echo "  -o pipefail: fallo en cualquier comando del pipeline"
'
```

---

## Parte 3 — PIPESTATUS y patrones

### Objetivo

Usar PIPESTATUS para diagnosticar pipelines y aplicar patrones
profesionales de manejo de errores.

### Paso 3.1: PIPESTATUS

```bash
docker compose exec debian-dev bash -c '
echo "=== PIPESTATUS ==="
echo ""
echo "\$? solo guarda el exit code del ULTIMO comando"
echo "PIPESTATUS guarda el exit code de CADA comando del pipeline"
echo ""
echo "--- Pipeline exitoso ---"
echo "hello" | grep "hello" | wc -l
echo "PIPESTATUS: ${PIPESTATUS[@]}"
echo ""
echo "--- Pipeline con fallo intermedio ---"
cat /etc/shadow 2>/dev/null | grep "root" | wc -l
echo "PIPESTATUS: ${PIPESTATUS[@]}"
echo "\$? solo ve el ultimo: $?"
echo ""
echo "--- Guardar PIPESTATUS ---"
echo "IMPORTANTE: PIPESTATUS se sobreescribe en cada comando"
echo "Guardar en una variable inmediatamente:"
false | true | false
SAVED=("${PIPESTATUS[@]}")
echo "Guardado: ${SAVED[@]}"
echo "Comando 1: ${SAVED[0]}"
echo "Comando 2: ${SAVED[1]}"
echo "Comando 3: ${SAVED[2]}"
'
```

### Paso 3.2: Patron strict mode

```bash
docker compose exec debian-dev bash -c '
echo "=== Patron: strict mode ==="
echo ""
cat > /tmp/strict.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

# Uso correcto de defaults con set -u
LOG_LEVEL="${LOG_LEVEL:-info}"
DEBUG="${DEBUG:-0}"

echo "LOG_LEVEL=$LOG_LEVEL"
echo "DEBUG=$DEBUG"

# Funcion que puede fallar
check_service() {
    local service="$1"
    if systemctl is-active "$service" &>/dev/null; then
        echo "$service: activo"
        return 0
    else
        echo "$service: inactivo"
        return 1
    fi
}

# Con set -e, usar || para manejar el fallo
check_service "ssh" || echo "(continuando de todos modos)"

echo "Script completo"
SCRIPT
chmod +x /tmp/strict.sh
echo "--- Ejecutar ---"
/tmp/strict.sh
echo ""
echo "Patron recomendado para todo script:"
echo "  #!/usr/bin/env bash"
echo "  set -euo pipefail"
echo "  Usar \${VAR:-default} para variables opcionales"
echo "  Usar || para comandos que pueden fallar legitimamente"
'
```

### Paso 3.3: Patron die/warn

```bash
docker compose exec debian-dev bash -c '
echo "=== Patron die/warn ==="
echo ""
cat > /tmp/die-pattern.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

readonly SCRIPT_NAME=$(basename "$0")

die()  { echo "$SCRIPT_NAME: error: $*" >&2; exit 1; }
warn() { echo "$SCRIPT_NAME: warning: $*" >&2; }

# Validar argumentos
[[ $# -ge 1 ]] || die "falta argumento (uso: $SCRIPT_NAME <archivo>)"

FILE="$1"
[[ -f "$FILE" ]] || die "archivo no encontrado: $FILE"
[[ -r "$FILE" ]] || die "sin permiso de lectura: $FILE"

LINES=$(wc -l < "$FILE")
(( LINES > 0 )) || warn "archivo vacio: $FILE"

echo "$FILE: $LINES lineas"
SCRIPT
chmod +x /tmp/die-pattern.sh
echo "--- Sin argumentos ---"
/tmp/die-pattern.sh 2>&1 || true
echo ""
echo "--- Archivo inexistente ---"
/tmp/die-pattern.sh /no/existe 2>&1 || true
echo ""
echo "--- Archivo valido ---"
/tmp/die-pattern.sh /etc/passwd
echo ""
echo "die() envia el error a stderr y sale con exit 1"
echo "warn() envia advertencia a stderr pero continua"
'
```

### Paso 3.4: Trap ERR para diagnostico

```bash
docker compose exec debian-dev bash -c '
echo "=== trap ERR ==="
echo ""
cat > /tmp/trap-err.sh << '\''SCRIPT'\''
#!/usr/bin/env bash
set -euo pipefail

on_error() {
    echo "ERROR en linea $LINENO: $BASH_COMMAND" >&2
    echo "Exit code: $?" >&2
}
trap on_error ERR

echo "Paso 1: OK"
echo "Paso 2: OK"
echo "Paso 3: forzar error..."
false
echo "Nunca llega aqui"
SCRIPT
chmod +x /tmp/trap-err.sh
echo "--- Ejecutar ---"
/tmp/trap-err.sh 2>&1 || true
echo ""
echo "trap ERR captura el error y muestra diagnostico"
echo "antes de que el script termine"
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/no-e.sh /tmp/with-e.sh /tmp/e-exceptions.sh
rm -f /tmp/with-u.sh /tmp/strict.sh /tmp/die-pattern.sh
rm -f /tmp/trap-err.sh /tmp/not-executable.sh
echo "Archivos temporales eliminados"
'
```

---

## Conceptos reforzados

1. `$?` contiene el exit code del ultimo comando. Convencion:
   **0 = exito**, cualquier otro valor = error. Los codigos
   126 (no ejecutable) y 127 (no encontrado) son del shell.

2. `set -e` termina el script en el primer error, pero tiene
   excepciones: condiciones de `if`, lado izquierdo de `&&`/`||`,
   comandos negados con `!`, y pipelines (sin pipefail).

3. `set -u` detecta variables no definidas. Usar `${VAR:-}`
   o `${VAR:-default}` para evitar errores con variables
   opcionales.

4. `set -o pipefail` reporta el fallo de CUALQUIER comando
   en un pipeline, no solo el ultimo. `PIPESTATUS` guarda
   el exit code de cada comando individual.

5. El patron `die()` centraliza mensajes de error con el
   nombre del script, salida a stderr, y exit 1. `warn()`
   es similar pero no termina el script.

6. `trap ERR` permite ejecutar diagnosticos (linea, comando,
   exit code) cuando ocurre un error, antes de que el script
   termine.
