# T03 — Exit codes

## Qué es un exit code

Todo comando en Linux termina con un **código de salida** (exit code o exit
status) — un número entero de 0 a 255 que indica si el comando tuvo éxito o
falló:

```bash
# $? contiene el exit code del último comando ejecutado
ls /etc/passwd
echo $?
# 0 (éxito)

ls /archivo/inexistente
echo $?
# 2 (error — archivo no encontrado)
```

---

## Convenciones

| Exit code | Significado | Ejemplo |
|---|---|---|
| 0 | Éxito | Comando exitoso |
| 1 | Error general | Fallo catch-all |
| 2 | Uso incorrecto | Argumentos inválidos |
| 126 | No ejecutable | Sin permiso de ejecución |
| 127 | Comando no encontrado | `ls` mal escrito |
| 128 | Argumento inválido para exit | `exit 300` ( fuera de rango) |
| 128+N | Terminado por señal N | 137 = SIGKILL (128+9) |
| 130 | Terminado por Ctrl+C | 128+2 = SIGINT |
| 255 | Exit code fuera de rango | |

```bash
# Verificar la convención:
/bin/false; echo $?          # 1
comando_inexistente; echo $? # 127
touch /root/test; echo $?    # 1 (permission denied, error general)

# Señales:
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

# exit N — terminar el script con código N
if [[ ! -f /etc/config ]]; then
    echo "Error: config no encontrada" >&2
    exit 1
fi

# Sin argumento, exit usa el código del último comando
exit      # sale con $? del último comando

# exit 0 al final es implícito (si el último comando tuvo éxito)
```

### Patrones comunes

```bash
# Función que retorna éxito/error
check_root() {
    [[ $(id -u) -eq 0 ]]    # return implícito: 0 si es root, 1 si no
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

---

## set -e — Salir al primer error

Sin `set -e`, un script continúa ejecutando incluso si un comando falla:

```bash
#!/bin/bash
# SIN set -e:
cd /directorio/inexistente    # falla, pero el script continúa
rm -rf *                       # ¡EJECUTA en el directorio actual!
# Desastre: rm se ejecutó en el directorio equivocado
```

```bash
#!/bin/bash
set -e
# CON set -e:
cd /directorio/inexistente    # falla → el script sale inmediatamente
rm -rf *                       # nunca se ejecuta
```

### Cuándo set -e NO sale

`set -e` tiene excepciones que son fuente de confusión:

| Situación | ¿Sale con set -e? | Razón |
|---|---|---|
| Comando en `if/while/until` | No | Es el resultado de la condición |
| Comando con `\|\|` | No | El operador maneja el error |
| Comando con `&&` | No | El operador maneja el resultado |
| Pipeline (sin pipefail) | No | Solo el último comando importa |
| Comando negado con `!` | No | El neg invertió el resultado |
| `false` directo | **Sí** | Falla sin manejo |
| `$(false)` | **Sí** | Command substitution que falla |

```bash
set -e

# NO sale si el comando fallido está en un if/while/until
if grep -q "patron" file; then     # grep falla → no sale (es parte de if)
    echo "encontrado"
fi

# NO sale si el comando tiene || o &&
false || echo "no sale"            # false falla pero || lo maneja
false && echo "no sale"            # false falla pero && lo maneja

# NO sale en el lado izquierdo de un pipe (sin pipefail)
false | echo "no sale"             # false falla pero el pipe continúa

# NO sale si el comando está negado con !
! false                            # negación invierte el resultado

# SÍ sale en estos casos:
false                              # falla → sale
$(false)                           # command substitution que falla → sale
```

### set -e en la práctica

```bash
#!/bin/bash
set -e

# Comandos que ESPERAS que fallen necesitan manejo explícito:
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
echo $VARIABLE_TYPO    # (vacío, sin error — el typo pasa desapercibido)
rm -rf "$DIR/"*         # Si DIR no está definida: rm -rf /* ← DESASTRE

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
# Solución: usar defaults
FILE=${1:-""}

# Arrays vacíos generan error con ${arr[@]}
# Solución: usar ${arr[@]+"${arr[@]}"}
declare -a ARGS=()
some_command ${ARGS[@]+"${ARGS[@]}"}
```

---

## set -o pipefail — Fallar en pipes

Sin pipefail, el exit code de un pipeline es el del **último** comando:

```bash
# SIN pipefail:
false | true
echo $?
# 0  ← true tuvo éxito, se ignora que false falló

# CON pipefail:
set -o pipefail
false | true
echo $?
# 1  ← false falló, pipefail lo reporta
```

### Pipeline real donde importa

```bash
# Sin pipefail: no detectas el error
curl -f https://example.com/data.json | jq '.users'
echo $?
# 0 si jq tuvo éxito, INCLUSO si curl falló y jq procesó basura

# Con pipefail: el error de curl se detecta
set -o pipefail
curl -f https://example.com/data.json | jq '.users'
echo $?
# El exit code refleja el primer fallo en el pipeline
```

---

## El header estándar de scripts robustos

```bash
#!/usr/bin/env bash
set -euo pipefail

# set -e: salir al primer error
# set -u: error si variable no definida
# set -o pipefail: el pipeline falla si cualquier parte falla

# Este header es el estándar de facto para scripts bash profesionales
# Conocido como "bash strict mode"
```

### IFS y el strict mode extendido

Algunos guías agregan la redefinición de IFS:

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
echo "Pipeline exit: $?"              # 1 (el último fallo)
echo "Individual: ${PIPESTATUS[@]}"   # 1 0 1 (cada comando)

# PIPESTATUS es un array con el exit code de cada comando del pipeline
# Solo disponible inmediatamente después del pipeline
# Se sobrescribe con el siguiente comando
```

### Ejemplo práctico

```bash
cmd1 | cmd2 | cmd3
status=("${PIPESTATUS[@]}")

if [[ ${status[0]} -ne 0 ]]; then
    echo "cmd1 falló"
elif [[ ${status[1]} -ne 0 ]]; then
    echo "cmd2 falló"
elif [[ ${status[2]} -ne 0 ]]; then
    echo "cmd3 falló"
fi
```

---

## Operadores condicionales

```bash
# && — ejecutar si el anterior tuvo éxito (exit 0)
make && make install
# make install solo se ejecuta si make tuvo éxito

# || — ejecutar si el anterior falló (exit != 0)
cd /dir || exit 1
# exit 1 solo si cd falló

# Combinados (patrón "do or die"):
cd /dir || { echo "Error: no puedo entrar a /dir" >&2; exit 1; }

# Encadenar múltiples comandos
apt update && apt upgrade -y && apt autoremove -y || echo "Algo falló"
```

---

## true y false

```bash
# true: siempre exit 0
true; echo $?    # 0

# false: siempre exit 1
false; echo $?   # 1

# Útiles en scripts:
while true; do     # loop infinito
    check_status
    sleep 60
done

# Ignorar errores explícitamente con set -e activo
rm /tmp/lock 2>/dev/null || true    # no falla si el archivo no existe
```

---

## Ejercicios

### Ejercicio 1 — Observar exit codes

```bash
true; echo "true: $?"
false; echo "false: $?"
ls /etc/passwd; echo "ls existente: $?"
ls /no/existe 2>/dev/null; echo "ls inexistente: $?"
grep -q root /etc/passwd; echo "grep encontrado: $?"
grep -q zzzzz /etc/passwd; echo "grep no encontrado: $?"

# Comandos con señales
sleep 100 &
PID=$!
kill -9 $PID 2>/dev/null
wait $PID 2>/dev/null
echo "SIGKILL: $?"
```

---

### Ejercicio 2 — set -e en acción

```bash
# Script sin set -e
bash -c '
    echo "Paso 1"
    false
    echo "Paso 2 (se ejecuta aunque false falló)"
'

# Script con set -e
bash -c '
    set -e
    echo "Paso 1"
    false
    echo "Paso 2 (no se ejecuta)"
' 2>&1 || echo "Script terminó con error"
```

---

### Ejercicio 3 — pipefail

```bash
# Sin pipefail
bash -c '
    false | echo "ok"
    echo "Exit: $?"
'

# Con pipefail
bash -c '
    set -o pipefail
    false | echo "ok"
    echo "Exit: $?"
'

# Ejemplo real: curl + jq
bash -c '
    set -o pipefail
    curl -f https://httpbin.org/status/500 2>/dev/null | jq .
    echo "curl/jq exit: $?"
'
```

---

### Ejercicio 4 — PIPESTATUS

```bash
# Guardar PIPESTATUS antes de que cambie
true | false | true
STATUS=("${PIPESTATUS[@]}")
echo "PIPESTATUS: ${STATUS[@]}"
echo "cmd1: ${STATUS[0]}, cmd2: ${STATUS[1]}, cmd3: ${STATUS[2]}"

# Aplicación: verificar cada paso del pipeline
bash -c '
    set -o pipefail
    make | tee build.log | grep -E "(error|warning)"
    PIPESTATUS=("${PIPESTATUS[@]}")
    if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
        echo "make falló"
    fi
'
```

---

### Ejercicio 5 — set -u

```bash
# Sin set -u
bash -c '
    echo "Valor de TYPO: $VARIABLE_TYPO"
    echo "El script continúa"
'

# Con set -u
bash -c '
    set -u
    echo "Valor de TYPO: $VARIABLE_TYPO"
' 2>&1 || echo "Script falló"

# Proteger con default
bash -c '
    set -u
    VALUE=${VARIABLE_TYPO:-"default"}
    echo "VALUE: $VALUE"
'
```

---

### Ejercicio 6 — Die function pattern

```bash
cat > /tmp/test-die.sh << 'EOF'
#!/bin/bash
set -euo pipefail

die() {
    echo "ERROR: $*" >&2
    exit 1
}

# Validaciones
[[ -f "$1" ]] || die "Archivo $1 no encontrado"
[[ -r "$1" ]] || die "Archivo $1 no es legible"

echo "Procesando $1..."
head "$1"
EOF
chmod +x /tmp/test-die.sh

# Probar con archivo inexistente
/tmp/test-die.sh /no/existe 2>&1 || echo "Falló como esperado"

# Crear archivo y probar
echo "test" > /tmp/test.txt
/tmp/test-die.sh /tmp/test.txt

rm /tmp/test-die.sh /tmp/test.txt
```

---

### Ejercicio 7 — Return vs exit en funciones

```bash
#!/bin/bash

# Función con return (NO sale del script)
check() {
    if [[ $1 -gt 10 ]]; then
        echo "$1 es mayor que 10"
        return 0
    else
        echo "$1 es menor o igual a 10"
        return 1
    fi
}

check 5; echo "check(5) exit: $?"
check 15; echo "check(15) exit: $?"

# Función con exit (SÍ sale del script)
fail_on_error() {
    if [[ $1 -ne 0 ]]; then
        echo "ERROR: código $1" >&2
        exit $1
    fi
}

echo "Antes de fail_on_error"
fail_on_error 0
echo "Después de fail_on_error con 0"
fail_on_error 1
echo "Esto nunca se imprime"
```

---

### Ejercicio 8 — exit codes en管道

```bash
#!/bin/bash
set -euo pipefail

# Un script que procesa y falla si cualquier paso falla
INPUT_FILE=${1:-/etc/passwd}

# Verificar que existe
[[ -f "$INPUT_FILE" ]] || { echo "No existe: $INPUT_FILE" >&2; exit 1; }

# Procesar línea por línea
while IFS= read -r line; do
    echo "Línea: $line"
done < "$INPUT_FILE"

echo "Procesamiento completado"
```

---

### Ejercicio 9 — Combinación set -e + manejo de errores

```bash
#!/bin/bash
set -euo pipefail

# Función que puede fallar pero no debe terminar el script
try_grep() {
    if grep -q "$1" "$2" 2>/dev/null; then
        return 0
    else
        return 1
    fi
}

# Uso
if try_grep "root" /etc/passwd; then
    echo "root existe"
else
    echo "root no encontrado"
fi

# Con operador || para ignorar
set -e
try_grep "nobody" /etc/passwd || true
echo "Continuó después del grep que pudo fallar"
```

---

### Ejercicio 10 — Verificar exit code de script completo

```bash
cat > /tmp/test-script.sh << 'EOF'
#!/bin/bash
set -euo pipefail
echo "Inicio"
false
echo "Fin"
EOF
chmod +x /tmp/test-script.sh

# Ejecutar y verificar
/tmp/test-script.sh 2>&1
echo "Exit code: $?"

# Con set -e removido
cat > /tmp/test-script2.sh << 'EOF'
#!/bin/bash
echo "Inicio"
false
echo "Fin"
EOF
chmod +x /tmp/test-script2.sh

/tmp/test-script2.sh 2>&1
echo "Exit code: $?"

rm /tmp/test-script.sh /tmp/test-script2.sh
```

---

## Cheat sheet rápido

```bash
# Ver exit code
echo $?

# Probar comando y mostrar código
ls /etc/passwd; echo "Exit: $?"

# Salir con error
exit 1

# Salir con código de señal (128 + N)
# 137 = kill -9 = SIGKILL
# 143 = kill -15 = SIGTERM
# 130 = Ctrl+C = SIGINT

# set -e: morir al primer error
set -e

# set -u: morir en variable indefinida
set -u

# set -o pipefail: morir si cualquier pipe falla
set -o pipefail

# Combinar
set -euo pipefail
```
