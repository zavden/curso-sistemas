# Lab — Arrays

## Objetivo

Dominar arrays en bash: arrays indexados (crear, acceder, ${arr[@]}
vs ${arr[*]}, longitud, indices, modificar, unset, slicing), arrays
asociativos (declare -A, claves, -v para verificar existencia),
mapfile/readarray para lectura segura, pasar arrays a funciones
con nameref, y patrones comunes (join/split, stack, acumular
errores).

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Arrays indexados

### Objetivo

Crear, acceder, modificar y recorrer arrays indexados.

### Paso 1.1: Crear y acceder

```bash
docker compose exec debian-dev bash -c '
echo "=== Arrays indexados ==="
echo ""
FRUITS=(apple banana cherry date elderberry)
echo "--- Acceso por indice (0-based) ---"
echo "\${FRUITS[0]} = ${FRUITS[0]}"
echo "\${FRUITS[2]} = ${FRUITS[2]}"
echo "\${FRUITS[-1]} = ${FRUITS[-1]} (ultimo, bash 4.2+)"
echo ""
echo "--- Sin llaves: solo el primer elemento ---"
echo "\$FRUITS = $FRUITS (equivale a \${FRUITS[0]})"
echo ""
echo "--- Todos los elementos ---"
echo "\${FRUITS[@]} = ${FRUITS[@]}"
echo ""
echo "--- Longitud ---"
echo "\${#FRUITS[@]} = ${#FRUITS[@]} elementos"
echo "\${#FRUITS[0]} = ${#FRUITS[0]} caracteres (longitud de apple)"
echo ""
echo "--- Indices ---"
echo "\${!FRUITS[@]} = ${!FRUITS[@]}"
'
```

### Paso 1.2: [@] vs [*]

Antes de ejecutar, predice: con elementos que tienen espacios,
que diferencia hay entre `"${arr[@]}"` y `${arr[@]}`?

```bash
docker compose exec debian-dev bash -c '
echo "=== [@] vs [*] ==="
echo ""
FILES=("file one.txt" "file two.txt" "file three.txt")
echo "Array con espacios en elementos"
echo ""
echo "--- \"\${arr[@]}\" (correcto: cada elemento separado) ---"
for f in "${FILES[@]}"; do
    echo "  [$f]"
done
echo ""
echo "--- \"\${arr[*]}\" (un solo string) ---"
for f in "${FILES[*]}"; do
    echo "  [$f]"
done
echo ""
echo "--- \${arr[@]} sin comillas (ROMPE con espacios) ---"
for f in ${FILES[@]}; do
    echo "  [$f]"
done
echo ""
echo "REGLA: siempre usar \"\${arr[@]}\" con comillas"
'
```

### Paso 1.3: Modificar arrays

```bash
docker compose exec debian-dev bash -c '
echo "=== Modificar arrays ==="
echo ""
ARR=(alpha bravo charlie delta echo)
echo "Original: ${ARR[*]}"
echo ""
echo "--- Agregar al final ---"
ARR+=(foxtrot golf)
echo "Despues de +=: ${ARR[*]}"
echo ""
echo "--- Reemplazar ---"
ARR[1]="BRAVO"
echo "Despues de ARR[1]=BRAVO: ${ARR[*]}"
echo ""
echo "--- Eliminar (deja hueco) ---"
unset "ARR[2]"
echo "Despues de unset ARR[2]: ${ARR[*]}"
echo "Indices: ${!ARR[@]}"
echo "(indice 2 no existe — hay un hueco)"
echo ""
echo "--- Re-indexar ---"
ARR=("${ARR[@]}")
echo "Re-indexado: ${ARR[*]}"
echo "Indices: ${!ARR[@]}"
echo "(indices consecutivos de nuevo)"
echo ""
echo "--- Slicing ---"
FULL=(a b c d e f g)
echo "Original: ${FULL[*]}"
echo "\${FULL[@]:2:3} = ${FULL[@]:2:3} (3 desde indice 2)"
echo "\${FULL[@]:(-2)} = ${FULL[@]:(-2)} (ultimos 2)"
'
```

---

## Parte 2 — Arrays asociativos

### Objetivo

Usar arrays asociativos (declare -A) para mapas clave-valor.

### Paso 2.1: declare -A

```bash
docker compose exec debian-dev bash -c '
echo "=== Arrays asociativos ==="
echo ""
echo "--- Crear ---"
declare -A PORTS=([http]=80 [https]=443 [ssh]=22 [dns]=53)
echo ""
echo "--- Acceder ---"
echo "PORTS[http] = ${PORTS[http]}"
echo "PORTS[ssh] = ${PORTS[ssh]}"
echo ""
echo "--- Todas las claves ---"
echo "\${!PORTS[@]} = ${!PORTS[@]}"
echo "(el orden NO esta garantizado)"
echo ""
echo "--- Todos los valores ---"
echo "\${PORTS[@]} = ${PORTS[@]}"
echo ""
echo "--- Iterar ---"
for key in "${!PORTS[@]}"; do
    echo "  $key → ${PORTS[$key]}"
done
echo ""
echo "--- Numero de elementos ---"
echo "\${#PORTS[@]} = ${#PORTS[@]}"
echo ""
echo "IMPORTANTE: declare -A es OBLIGATORIO"
echo "Sin declare -A, bash crea un array indexado"
'
```

### Paso 2.2: Verificar existencia y eliminar

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar y eliminar ==="
echo ""
declare -A CONFIG=([host]="localhost" [port]="8080" [debug]="false")
echo ""
echo "--- -v: verificar si una clave existe (bash 4.2+) ---"
if [[ -v CONFIG[host] ]]; then
    echo "CONFIG[host] existe: ${CONFIG[host]}"
fi
if [[ -v CONFIG[timeout] ]]; then
    echo "CONFIG[timeout] existe"
else
    echo "CONFIG[timeout] NO existe"
fi
echo ""
echo "--- Alternativa sin -v ---"
if [[ -n "${CONFIG[host]+set}" ]]; then
    echo "CONFIG[host] esta definido"
fi
echo ""
echo "--- Eliminar una clave ---"
unset "CONFIG[debug]"
echo "Despues de unset CONFIG[debug]:"
for k in "${!CONFIG[@]}"; do echo "  $k = ${CONFIG[$k]}"; done
echo ""
echo "--- Agregar ---"
CONFIG[timeout]="30"
CONFIG[log_level]="info"
echo "Despues de agregar:"
for k in "${!CONFIG[@]}"; do echo "  $k = ${CONFIG[$k]}"; done
'
```

### Paso 2.3: Conteo de frecuencias

```bash
docker compose exec debian-dev bash -c '
echo "=== Conteo de frecuencias ==="
echo ""
echo "--- Shells en /etc/passwd ---"
declare -A SHELL_COUNT
while IFS=: read -r _ _ _ _ _ _ shell; do
    ((SHELL_COUNT[$shell]++))
done < /etc/passwd

echo "Frecuencia de shells:"
for shell in "${!SHELL_COUNT[@]}"; do
    printf "  %-25s %d\n" "$shell" "${SHELL_COUNT[$shell]}"
done | sort -t: -k2 -rn
echo ""
echo "--- Leer config key=value ---"
declare -A APP_CONFIG
while IFS="=" read -r key value; do
    [[ -z "$key" || "$key" == \#* ]] && continue
    APP_CONFIG[$key]="$value"
done << '\''EOF'\''
# Configuracion
host=localhost
port=8080
debug=true
# Fin
EOF

echo "Config leida:"
for k in "${!APP_CONFIG[@]}"; do
    echo "  $k = ${APP_CONFIG[$k]}"
done
'
```

---

## Parte 3 — mapfile y patrones

### Objetivo

Leer arrays de forma segura con mapfile y aplicar patrones
comunes.

### Paso 3.1: mapfile / readarray

```bash
docker compose exec debian-dev bash -c '
echo "=== mapfile (readarray) ==="
echo ""
echo "--- Leer archivo linea por linea ---"
mapfile -t LINES < /etc/hostname
echo "Lineas: ${#LINES[@]}"
echo "Primera: ${LINES[0]}"
echo ""
echo "--- Desde un comando ---"
mapfile -t USERS < <(cut -d: -f1 /etc/passwd | head -5)
echo "Usuarios:"
for u in "${USERS[@]}"; do echo "  $u"; done
echo ""
echo "--- INCORRECTO: \$(command) con word splitting ---"
echo "FILES=(\$(find /tmp -name \"*.txt\"))  → ROMPE con espacios"
echo ""
echo "--- CORRECTO: mapfile ---"
touch "/tmp/test file.txt" "/tmp/normal.txt"
mapfile -t FOUND < <(find /tmp -maxdepth 1 -name "*.txt" -type f)
echo "Encontrados:"
for f in "${FOUND[@]}"; do echo "  $f"; done
rm -f "/tmp/test file.txt" "/tmp/normal.txt"
echo ""
echo "-t: eliminar newline de cada elemento"
echo "readarray es sinonimo de mapfile"
'
```

### Paso 3.2: Join y split

```bash
docker compose exec debian-dev bash -c '
echo "=== Join y split ==="
echo ""
echo "--- Split: string a array ---"
IFS="," read -ra ITEMS <<< "apple,banana,cherry,date"
echo "Split de CSV:"
for item in "${ITEMS[@]}"; do echo "  $item"; done
echo "Total: ${#ITEMS[@]}"
echo ""
echo "--- Join: array a string ---"
join_by() {
    local IFS="$1"
    shift
    echo "$*"
}
TAGS=(linux bash scripting)
echo "Join con coma: $(join_by "," "${TAGS[@]}")"
echo "Join con pipe: $(join_by "|" "${TAGS[@]}")"
echo ""
echo "--- Split por otro delimitador ---"
IFS=":" read -ra PATH_DIRS <<< "$PATH"
echo "Primeros 3 dirs del PATH:"
for ((i=0; i<3 && i<${#PATH_DIRS[@]}; i++)); do
    echo "  ${PATH_DIRS[$i]}"
done
'
```

### Paso 3.3: Patrones con arrays

```bash
docker compose exec debian-dev bash -c '
echo "=== Patrones con arrays ==="
echo ""
echo "--- Acumular errores ---"
ERRORS=()
WARNINGS=()

check() {
    local item="$1"
    if [[ ! -f "$item" ]]; then
        ERRORS+=("$item: no encontrado")
    elif [[ ! -r "$item" ]]; then
        WARNINGS+=("$item: sin permiso")
    fi
}

check /etc/passwd
check /no/existe
check /etc/shadow

echo "Errores: ${#ERRORS[@]}"
for e in "${ERRORS[@]}"; do echo "  ERROR: $e"; done
echo "Warnings: ${#WARNINGS[@]}"
for w in "${WARNINGS[@]}"; do echo "  WARN: $w"; done
echo ""
echo "--- Stack (LIFO) ---"
declare -a STACK=()
push() { STACK+=("$1"); }
pop() { local -n ref=$1; ref="${STACK[-1]}"; unset "STACK[-1]"; }

push "primero"
push "segundo"
push "tercero"
echo "Stack: ${STACK[*]}"
pop VALUE
echo "Pop: $VALUE"
echo "Stack: ${STACK[*]}"
echo ""
echo "--- Pasar a funciones con nameref ---"
fill() {
    local -n arr=$1
    arr+=("x" "y" "z")
}
declare -a RESULT=()
fill RESULT
echo "Resultado via nameref: ${RESULT[*]}"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Arrays indexados se crean con `()` o `declare -a`. El acceso
   requiere `${}`: `${arr[0]}`. Sin llaves, `$arr` es solo
   el primer elemento.

2. `"${arr[@]}"` expande cada elemento como una palabra separada
   (correcto). `"${arr[*]}"` concatena todo en un string.
   Sin comillas, word splitting rompe elementos con espacios.

3. `unset 'arr[N]'` deja un hueco en los indices. Re-indexar
   con `arr=("${arr[@]}")`. `${arr[@]:offset:length}` para
   slicing.

4. Arrays **asociativos** requieren `declare -A` obligatoriamente.
   Las claves son strings, el orden no esta garantizado. `-v`
   verifica existencia de clave.

5. **mapfile** (readarray) lee lineas de forma segura en un array.
   `mapfile -t LINES < <(command)` es la alternativa correcta a
   `LINES=($(command))` que rompe con espacios.

6. Patrones comunes: `IFS=, read -ra ARR <<< "csv"` para split,
   `join_by() { local IFS="$1"; shift; echo "$*"; }` para join,
   arrays para acumular errores/warnings, y `local -n` (nameref)
   para modificar arrays por referencia.
