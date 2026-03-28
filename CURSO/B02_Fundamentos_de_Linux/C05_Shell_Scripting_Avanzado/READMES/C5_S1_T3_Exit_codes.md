# T03 — Exit codes

## Errata respecto a los materiales originales

| Ubicacion | Error | Correccion |
|---|---|---|
| max.md:529 | `exit codes en管道` (caracteres chinos) | Correcto: `exit codes en pipelines` |
| max.md:31 | `exit 300 (fuera de rango)` como ejemplo de exit code 128 | `exit 300` retorna 44 (300 mod 256), no 128. El valor 128 es la base para senales (128+N) |
| labs:68-73 | `cmd 2>&1 \|\| true` seguido de `echo "$?"` para mostrar 126/127 | `$?` siempre sera 0 tras `\|\| true`. Corregido: capturar el exit code antes del `\|\| true` |

---

## Que es un exit code

Todo comando en Linux termina con un **codigo de salida** (exit code o exit
status) — un numero entero de 0 a 255 que indica si el comando tuvo exito o
fallo:

```bash
# $? contiene el exit code del ultimo comando ejecutado
ls /etc/passwd
echo $?
# 0 (exito)

ls /archivo/inexistente
echo $?
# 2 (error — archivo no encontrado)
```

---

## Convenciones

| Exit code | Significado | Ejemplo |
|---|---|---|
| 0 | Exito | Comando exitoso |
| 1 | Error general | Fallo catch-all |
| 2 | Uso incorrecto | Argumentos invalidos |
| 126 | No ejecutable | Sin permiso de ejecucion |
| 127 | Comando no encontrado | `lss` (typo) |
| 128+N | Terminado por senal N | 137 = SIGKILL (128+9) |
| 130 | Terminado por Ctrl+C | 128+2 = SIGINT |
| 143 | Terminado por kill | 128+15 = SIGTERM |

Nota: `exit` solo acepta enteros 0-255. Si se pasa un valor mayor, bash
aplica modulo 256 (`exit 300` → exit code 44). Si se pasa un argumento no
numerico (`exit abc`), bash imprime error y sale con codigo 2.

```bash
# Verificar la convencion:
/bin/false; echo $?          # 1
comando_inexistente; echo $? # 127
touch /root/test; echo $?    # 1 (permission denied, error general)

# Senales:
sleep 100 &
kill -9 $!
wait $!; echo $?             # 137 (128 + 9 = SIGKILL)

sleep 100 &
kill -15 $!
wait $!; echo $?             # 143 (128 + 15 = SIGTERM)
```

---

## Exit en scripts

```bash
#!/bin/bash

# exit N — terminar el script con codigo N
if [[ ! -f /etc/config ]]; then
    echo "Error: config no encontrada" >&2
    exit 1
fi

# Sin argumento, exit usa el codigo del ultimo comando
exit      # sale con $? del ultimo comando

# exit 0 al final es implicito (si el ultimo comando tuvo exito)
```

### Patrones comunes

```bash
# Funcion que retorna exito/error
check_root() {
    [[ $(id -u) -eq 0 ]]    # return implicito: 0 si es root, 1 si no
}

if check_root; then
    echo "Eres root"
else
    echo "No eres root" >&2
    exit 1
fi

# Die function
die() {
    echo "ERROR: $*" >&2
    exit 1
}

[[ -f "$CONFIG" ]] || die "Config $CONFIG no encontrada"
```

### Return vs exit en funciones

```bash
# return: sale de la funcion, no del script
check_file() {
    [[ -f "$1" ]] && return 0
    return 1
}
check_file /etc/passwd
echo $?    # 0

# exit: termina TODO el script (o subshell)
fail_hard() {
    echo "ERROR critico" >&2
    exit 1
}

# En un subshell, exit solo sale del subshell
(
    exit 42
)
echo "Exit code del subshell: $?"    # 42
echo "El script principal continua"
```

---

## set -e — Salir al primer error

Sin `set -e`, un script continua ejecutando incluso si un comando falla:

```bash
#!/bin/bash
# SIN set -e:
cd /directorio/inexistente    # falla, pero el script continua
rm -rf *                       # EJECUTA en el directorio actual!
# Desastre: rm se ejecuto en el directorio equivocado
```

```bash
#!/bin/bash
set -e
# CON set -e:
cd /directorio/inexistente    # falla → el script sale inmediatamente
rm -rf *                       # nunca se ejecuta
```

### Cuando set -e NO sale

`set -e` tiene excepciones que son fuente de confusion:

| Situacion | Sale con set -e? | Razon |
|---|---|---|
| Comando en `if/while/until` | No | Es el resultado de la condicion |
| Comando con `\|\|` | No | El operador maneja el error |
| Comando con `&&` | No | El operador maneja el resultado |
| Pipeline (sin pipefail) | No | Solo el ultimo comando importa |
| Comando negado con `!` | No | La negacion invirtio el resultado |
| `false` directo | **Si** | Falla sin manejo |
| `$(false)` | **Si** | Command substitution que falla |

```bash
set -e

# NO sale si el comando fallido esta en un if/while/until
if grep -q "patron" file; then     # grep falla → no sale (es parte de if)
    echo "encontrado"
fi

# NO sale si el comando tiene || o &&
false || echo "no sale"            # false falla pero || lo maneja
false && echo "no sale"            # false falla pero && lo maneja

# NO sale en el lado izquierdo de un pipe (sin pipefail)
false | echo "no sale"             # false falla pero el pipe continua

# NO sale si el comando esta negado con !
! false                            # negacion invierte el resultado

# SI sale en estos casos:
false                              # falla → sale
$(false)                           # command substitution que falla → sale
```

### set -e en la practica

```bash
#!/bin/bash
set -e

# Comandos que ESPERAS que fallen necesitan manejo explicito:
grep -q "patron" file || true      # ignorar si grep no encuentra nada
rm -f /tmp/lock 2>/dev/null || true  # ignorar si no existe

# O usar if:
if ! grep -q "patron" file; then
    echo "No encontrado (esto es OK)"
fi
```

---

## set -u — Error en variables no definidas

```bash
#!/bin/bash
# SIN set -u:
echo $VARIABLE_TYPO    # (vacio, sin error — el typo pasa desapercibido)
rm -rf "$DIR/"*         # Si DIR no esta definida: rm -rf /* ← DESASTRE

# CON set -u:
set -u
echo $VARIABLE_TYPO
# bash: VARIABLE_TYPO: unbound variable
# El script sale con error
```

### Variables que necesitan tratamiento especial con -u

```bash
set -u

# $1, $2... generan error si no se pasaron
# Solucion: usar defaults
FILE=${1:-""}

# Arrays vacios generan error con ${arr[@]}
# Solucion: usar ${arr[@]+"${arr[@]}"}
declare -a ARGS=()
some_command ${ARGS[@]+"${ARGS[@]}"}
```

---

## set -o pipefail — Fallar en pipes

Sin pipefail, el exit code de un pipeline es el del **ultimo** comando:

```bash
# SIN pipefail:
false | true
echo $?
# 0  ← true tuvo exito, se ignora que false fallo

# CON pipefail:
set -o pipefail
false | true
echo $?
# 1  ← false fallo, pipefail lo reporta
```

### Pipeline real donde importa

```bash
# Sin pipefail: no detectas el error
curl -f https://example.com/data.json | jq '.users'
echo $?
# 0 si jq tuvo exito, INCLUSO si curl fallo y jq proceso basura

# Con pipefail: el error de curl se detecta
set -o pipefail
curl -f https://example.com/data.json | jq '.users'
echo $?
# El exit code refleja el primer fallo en el pipeline
```

---

## El header estandar de scripts robustos

```bash
#!/usr/bin/env bash
set -euo pipefail

# set -e: salir al primer error
# set -u: error si variable no definida
# set -o pipefail: el pipeline falla si cualquier parte falla

# Este header es el estandar de facto para scripts bash profesionales
# Conocido como "bash strict mode"
```

### IFS y el strict mode extendido

Algunas guias agregan la redefinicion de IFS:

```bash
#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

# IFS=$'\n\t' cambia el separador de palabras:
# Default:  IFS=' \t\n' (espacio, tab, newline)
# Estricto: IFS='\n\t'  (solo tab y newline — el espacio no separa)
#
# Esto evita problemas con archivos que tienen espacios en el nombre
# Pero puede romper scripts que dependen del splitting por espacios
# Usar con cuidado
```

---

## PIPESTATUS — Exit codes de todo el pipeline

```bash
set -o pipefail

false | true | false
echo "Pipeline exit: $?"              # 1 (el ultimo fallo)
echo "Individual: ${PIPESTATUS[@]}"   # 1 0 1 (cada comando)

# PIPESTATUS es un array con el exit code de cada comando del pipeline
# Solo disponible inmediatamente despues del pipeline
# Se sobreescribe con el siguiente comando
```

### Ejemplo practico

```bash
cmd1 | cmd2 | cmd3
status=("${PIPESTATUS[@]}")

if [[ ${status[0]} -ne 0 ]]; then
    echo "cmd1 fallo"
elif [[ ${status[1]} -ne 0 ]]; then
    echo "cmd2 fallo"
elif [[ ${status[2]} -ne 0 ]]; then
    echo "cmd3 fallo"
fi
```

---

## Operadores condicionales

```bash
# && — ejecutar si el anterior tuvo exito (exit 0)
make && make install
# make install solo se ejecuta si make tuvo exito

# || — ejecutar si el anterior fallo (exit != 0)
cd /dir || exit 1
# exit 1 solo si cd fallo

# Combinados (patron "do or die"):
cd /dir || { echo "Error: no puedo entrar a /dir" >&2; exit 1; }

# Encadenar multiples comandos
apt update && apt upgrade -y && apt autoremove -y || echo "Algo fallo"
```

---

## true y false

```bash
# true: siempre exit 0
true; echo $?    # 0

# false: siempre exit 1
false; echo $?   # 1

# Utiles en scripts:
while true; do     # loop infinito
    check_status
    sleep 60
done

# Ignorar errores explicitamente con set -e activo
rm /tmp/lock 2>/dev/null || true    # no falla si el archivo no existe
```

---

## Trap ERR para diagnostico

```bash
#!/usr/bin/env bash
set -euo pipefail

on_error() {
    echo "ERROR en linea $LINENO: $BASH_COMMAND" >&2
    echo "Exit code: $?" >&2
}
trap on_error ERR

echo "Paso 1: OK"
echo "Paso 2: OK"
false    # trap se ejecuta antes de salir
echo "Nunca llega aqui"
```

`trap ERR` permite ejecutar diagnosticos (linea, comando, exit code) cuando
ocurre un error, antes de que el script termine.

---

## Labs

### Parte 1 — Exit codes y convenciones

#### Paso 1.1: $? — El exit code del ultimo comando

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

#### Paso 1.2: Convenciones de exit codes

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
/tmp/not-executable.sh 2>&1; RC=$?
echo "Exit code (no ejecutable): $RC"
echo ""
comando_inexistente_xyz 2>&1; RC=$?
echo "Exit code (no encontrado): $RC"
rm /tmp/not-executable.sh
'
```

#### Paso 1.3: && y || — Encadenamiento condicional

```bash
docker compose exec debian-dev bash -c '
echo "=== && y || ==="
echo ""
echo "--- && ejecuta el siguiente SOLO si el anterior fue exitoso ---"
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

#### Paso 1.4: exit y return

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

### Parte 2 — set -euo pipefail

#### Paso 2.1: set -e — Salir en error

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
echo "El script salio en false"
'
```

#### Paso 2.2: Excepciones de set -e

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

#### Paso 2.3: set -u — Variables no definidas

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

#### Paso 2.4: set -o pipefail

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
echo "=== Strict mode completo ==="
echo "set -euo pipefail"
echo "  -e: salir en error"
echo "  -u: error en variables no definidas"
echo "  -o pipefail: fallo en cualquier comando del pipeline"
'
```

### Parte 3 — PIPESTATUS y patrones

#### Paso 3.1: PIPESTATUS

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

#### Paso 3.2: Patron strict mode

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

#### Paso 3.3: Patron die/warn

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

#### Paso 3.4: Trap ERR para diagnostico

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

### Limpieza

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/no-e.sh /tmp/with-e.sh /tmp/e-exceptions.sh
rm -f /tmp/with-u.sh /tmp/strict.sh /tmp/die-pattern.sh
rm -f /tmp/trap-err.sh /tmp/not-executable.sh
echo "Archivos temporales eliminados"
'
```

---

## Ejercicios

### Ejercicio 1 — Observar exit codes basicos

Predice el exit code de cada comando antes de ejecutar:

```bash
true; echo "true: $?"
false; echo "false: $?"
ls /etc/passwd; echo "ls existente: $?"
ls /no/existe 2>/dev/null; echo "ls inexistente: $?"
grep -q root /etc/passwd; echo "grep encontrado: $?"
grep -q zzzzz /etc/passwd; echo "grep no encontrado: $?"
```

<details><summary>Prediccion</summary>

- `true` → 0 (siempre exito)
- `false` → 1 (siempre error)
- `ls /etc/passwd` → 0 (archivo existe)
- `ls /no/existe` → 2 (archivo no encontrado)
- `grep -q root /etc/passwd` → 0 (patron encontrado)
- `grep -q zzzzz /etc/passwd` → 1 (patron no encontrado)

Convencion: 0 = exito, 1 = error general, 2 = uso incorrecto / no encontrado.
Cada comando puede usar codigos distintos (ver `man comando`, seccion EXIT STATUS).

</details>

### Ejercicio 2 — Exit codes especiales: 126, 127, senales

Predice que exit code produce cada caso:

```bash
# Caso A: archivo sin permiso de ejecucion
echo "echo hola" > /tmp/no-exec.sh
/tmp/no-exec.sh 2>/dev/null; echo "A: $?"

# Caso B: comando inexistente
comando_que_no_existe_xyz 2>/dev/null; echo "B: $?"

# Caso C: proceso terminado con SIGKILL
sleep 100 &
kill -9 $!
wait $! 2>/dev/null; echo "C: $?"

# Caso D: proceso terminado con SIGTERM
sleep 100 &
kill -15 $!
wait $! 2>/dev/null; echo "D: $?"

rm /tmp/no-exec.sh
```

<details><summary>Prediccion</summary>

- A: `126` — archivo encontrado pero sin permiso de ejecucion (falta `chmod +x`)
- B: `127` — comando no encontrado en PATH
- C: `137` — 128 + 9 (SIGKILL)
- D: `143` — 128 + 15 (SIGTERM)

Regla: 126 = "existe pero no ejecutable", 127 = "no existe".
Para senales: 128 + numero de senal.

</details>

### Ejercicio 3 — set -e en accion

Predice que lineas se imprimen en cada caso:

```bash
# Caso 1: sin set -e
bash -c '
    echo "Paso 1"
    false
    echo "Paso 2"
    comando_inexistente 2>/dev/null
    echo "Paso 3"
'

# Caso 2: con set -e
bash -c '
    set -e
    echo "Paso 1"
    false
    echo "Paso 2"
' 2>&1; echo "Exit: $?"
```

<details><summary>Prediccion</summary>

Caso 1 (sin set -e):
```
Paso 1
Paso 2
Paso 3
```
Todos los pasos se ejecutan — los errores se ignoran.

Caso 2 (con set -e):
```
Paso 1
Exit: 1
```
Solo "Paso 1" se imprime. `false` falla → set -e termina el script
inmediatamente. "Paso 2" nunca se ejecuta. El exit code es 1.

</details>

### Ejercicio 4 — Excepciones de set -e

Predice que imprime este script:

```bash
bash -c '
set -e

if false; then echo "A"; else echo "B"; fi

false || echo "C"

false && echo "D"

! false
echo "E"

false | true
echo "F"

echo "G"
false
echo "H"
'
```

<details><summary>Prediccion</summary>

```
B
C
E
F
G
```

- `if false` → no sale (excepcion: condicion de if) → imprime "B"
- `false || echo "C"` → no sale (excepcion: ||) → imprime "C"
- `false && echo "D"` → no sale (excepcion: &&), "D" no se imprime porque && requiere exito
- `! false` → no sale (excepcion: negacion) → imprime "E"
- `false | true` → no sale (excepcion: pipeline sin pipefail) → imprime "F"
- `echo "G"` → se imprime normalmente
- `false` → SI sale (error directo, sin manejo) → "H" nunca se imprime

</details>

### Ejercicio 5 — pipefail y PIPESTATUS

Predice el exit code y PIPESTATUS en cada caso:

```bash
# Caso A: sin pipefail
bash -c '
false | true | echo "ok"
echo "Exit: $?"
echo "PIPESTATUS: ${PIPESTATUS[@]}"
'

# Caso B: con pipefail
bash -c '
set -o pipefail
false | true | echo "ok"
echo "Exit: $?"
'
```

<details><summary>Prediccion</summary>

Caso A (sin pipefail):
```
ok
Exit: 0
PIPESTATUS: 0 0 0
```
Atencion: PIPESTATUS despues de `echo "Exit: $?"` refleja el pipeline del
`echo`, no del `false | true | echo`. Para capturar PIPESTATUS del pipeline
original, hay que guardarlo inmediatamente despues.

Correccion — el codigo tal como esta no captura bien PIPESTATUS. La linea
`echo "Exit: $?"` sobreescribe PIPESTATUS. Para hacerlo bien:
```bash
false | true | echo "ok"
saved=("${PIPESTATUS[@]}")
echo "Exit: $?"    # sera 0 del saved= assignment
echo "PIPESTATUS: ${saved[@]}"    # 1 0 0
```

Caso B (con pipefail):
```
ok
Exit: 1
```
`echo "ok"` se ejecuta (los comandos del pipeline corren en paralelo).
pipefail hace que el exit code sea 1 (de `false`).

</details>

### Ejercicio 6 — set -u y defaults

Predice la salida de cada caso:

```bash
# Caso A: sin set -u
bash -c '
echo "NOEXIST=[$NOEXIST]"
echo "Continua"
'

# Caso B: con set -u, sin proteccion
bash -c '
set -u
echo "NOEXIST=[$NOEXIST]"
echo "Continua"
' 2>&1; echo "Exit: $?"

# Caso C: con set -u, con default
bash -c '
set -u
VALUE=${NOEXIST:-"fallback"}
echo "VALUE=[$VALUE]"
echo "Continua"
'
```

<details><summary>Prediccion</summary>

Caso A: `NOEXIST=[]` + `Continua` — la variable se expande a vacio sin error.

Caso B: Error `bash: NOEXIST: unbound variable` + `Exit: 1` — set -u detecta
la variable no definida y aborta. "Continua" nunca se imprime.

Caso C: `VALUE=[fallback]` + `Continua` — `${NOEXIST:-"fallback"}` provee un
default, set -u no se activa porque la expansion `:-` maneja el caso.

</details>

### Ejercicio 7 — Die pattern

Crea este script y predice que pasa con cada invocacion:

```bash
cat > /tmp/die-test.sh << 'EOF'
#!/usr/bin/env bash
set -euo pipefail

die() { echo "ERROR: $*" >&2; exit 1; }

[[ $# -ge 1 ]] || die "uso: $0 <archivo>"
[[ -f "$1" ]]   || die "no existe: $1"
[[ -r "$1" ]]   || die "no legible: $1"

echo "Procesando $1: $(wc -l < "$1") lineas"
EOF
chmod +x /tmp/die-test.sh
```

Prueba:
```bash
/tmp/die-test.sh
/tmp/die-test.sh /no/existe
/tmp/die-test.sh /etc/passwd
```

<details><summary>Prediccion</summary>

1. `/tmp/die-test.sh` → stderr: `ERROR: uso: /tmp/die-test.sh <archivo>`, exit 1
   ($# es 0, la condicion `$# -ge 1` es falsa, || ejecuta die)

2. `/tmp/die-test.sh /no/existe` → stderr: `ERROR: no existe: /no/existe`, exit 1
   ($# es 1, pasa la primera validacion. `-f /no/existe` es falso → die)

3. `/tmp/die-test.sh /etc/passwd` → stdout: `Procesando /etc/passwd: N lineas`, exit 0
   (todas las validaciones pasan, wc -l cuenta las lineas)

Patron: las validaciones con `|| die` actuan como guardias al inicio del script.
Si alguna falla, die() imprime a stderr y sale con exit 1.

</details>

### Ejercicio 8 — && y || combinados

Predice que imprime cada linea:

```bash
true  && echo "A"
false && echo "B"
true  || echo "C"
false || echo "D"
true  && echo "E" || echo "F"
false && echo "G" || echo "H"
```

<details><summary>Prediccion</summary>

- `true && echo "A"` → `A` (true exito → && ejecuta echo)
- `false && echo "B"` → nada (false fallo → && no ejecuta echo)
- `true || echo "C"` → nada (true exito → || no ejecuta echo)
- `false || echo "D"` → `D` (false fallo → || ejecuta echo)
- `true && echo "E" || echo "F"` → `E` (true → && ejecuta echo "E", exito → || no ejecuta)
- `false && echo "G" || echo "H"` → `H` (false → && no ejecuta "G", fallo → || ejecuta "H")

El patron `cmd && ok || fail` funciona como un pseudo-ternario, pero cuidado:
si el comando despues de `&&` falla, `||` tambien se ejecuta. Para logica
if/else real, usar `if/then/else/fi`.

</details>

### Ejercicio 9 — Trap ERR

Predice que imprime este script:

```bash
bash -c '
set -euo pipefail

on_error() {
    echo "TRAP: fallo en linea $LINENO: $BASH_COMMAND (exit $?)" >&2
}
trap on_error ERR

echo "Paso 1"
echo "Paso 2"
ls /archivo/inexistente 2>/dev/null
echo "Paso 3"
' 2>&1; echo "Script exit: $?"
```

<details><summary>Prediccion</summary>

```
Paso 1
Paso 2
TRAP: fallo en linea N: ls /archivo/inexistente 2>/dev/null (exit 2)
Script exit: 2
```

- "Paso 1" y "Paso 2" se imprimen normalmente
- `ls /archivo/inexistente` falla con exit 2 (archivo no encontrado)
- `set -e` detecta el error → activa trap ERR → ejecuta `on_error`
- trap muestra la linea, el comando que fallo, y el exit code
- El script termina con exit 2 (el exit code de ls)
- "Paso 3" nunca se imprime

Nota: `$LINENO` dentro del trap apunta a la linea dentro del `bash -c`, no
la linea del shell exterior. `$BASH_COMMAND` contiene el comando exacto que fallo.

</details>

### Ejercicio 10 — Panorama: strict mode completo

Analiza este script y predice que pasa en cada escenario de ejecucion:

```bash
cat > /tmp/strict-demo.sh << 'EOF'
#!/usr/bin/env bash
set -euo pipefail

die() { echo "FATAL: $*" >&2; exit 1; }

# Variables con defaults
OUTPUT_DIR=${OUTPUT_DIR:-/tmp/output}
FORMAT=${FORMAT:-json}
VERBOSE=${VERBOSE:-0}

# Validacion obligatoria
: ${INPUT_FILE:?"INPUT_FILE no definida"}

# Crear directorio de salida
mkdir -p "$OUTPUT_DIR" || die "No puedo crear $OUTPUT_DIR"

# Verificar archivo de entrada
[[ -f "$INPUT_FILE" ]] || die "No existe: $INPUT_FILE"

# Procesar
LINES=$(wc -l < "$INPUT_FILE")
echo "Procesando $INPUT_FILE ($LINES lineas) → $OUTPUT_DIR (formato: $FORMAT)"

[[ "$VERBOSE" -eq 1 ]] && echo "Modo verbose activado"

echo "Completado"
EOF
chmod +x /tmp/strict-demo.sh
```

Predice la salida de:
```bash
/tmp/strict-demo.sh
INPUT_FILE=/etc/passwd /tmp/strict-demo.sh
INPUT_FILE=/etc/passwd FORMAT=csv VERBOSE=1 /tmp/strict-demo.sh
INPUT_FILE=/no/existe /tmp/strict-demo.sh
```

<details><summary>Prediccion</summary>

1. `/tmp/strict-demo.sh` → Error: `INPUT_FILE: INPUT_FILE no definida`, exit 1.
   La linea `: ${INPUT_FILE:?...}` falla porque INPUT_FILE no esta en el entorno.

2. `INPUT_FILE=/etc/passwd /tmp/strict-demo.sh` →
   ```
   Procesando /etc/passwd (N lineas) → /tmp/output (formato: json)
   Completado
   ```
   INPUT_FILE definida, defaults aplican para OUTPUT_DIR, FORMAT, VERBOSE.
   VERBOSE=0, la linea del && no imprime nada.

3. `INPUT_FILE=/etc/passwd FORMAT=csv VERBOSE=1 /tmp/strict-demo.sh` →
   ```
   Procesando /etc/passwd (N lineas) → /tmp/output (formato: csv)
   Modo verbose activado
   Completado
   ```
   FORMAT y VERBOSE sobreescriben los defaults.

4. `INPUT_FILE=/no/existe /tmp/strict-demo.sh` →
   ```
   FATAL: No existe: /no/existe
   ```
   INPUT_FILE pasa la validacion `:?` (esta definida), mkdir funciona,
   pero `-f /no/existe` es falso → `|| die` ejecuta die().

Patron completo: `:?` para obligatorias, `:-` para opcionales,
`die()` para errores, `set -euo pipefail` para robustez.

</details>
