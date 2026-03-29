# Aritmética en Bash

## 1. Bash no tiene números

Todo en Bash es un string. La "aritmética" es un modo especial donde Bash
**interpreta** strings como enteros. No hay floats nativos, no hay tipos
numéricos, no hay overflow detection.

```bash
x=42        # string "42"
y=$((x + 1))  # Bash interpreta "42" como entero, suma 1, resultado string "43"
echo "$y"     # 43

z=3.14      # string "3.14"
w=$((z + 1))  # ERROR: Bash no puede interpretar "3.14" como entero
```

Tres contextos aritméticos:
1. `$(( ))` — expansión: calcula y retorna el resultado como string
2. `(( ))` — comando: calcula y retorna exit code (0=true si resultado ≠ 0)
3. `let` — comando: igual que `(( ))` pero con syntax diferente

---

## 2. $(( )) — Expansión aritmética

El resultado se sustituye inline como un string:

```bash
echo $((2 + 3))         # 5
echo $((10 / 3))        # 3 (entero, truncado)
echo $((10 % 3))        # 1 (módulo)
echo $((2 ** 10))       # 1024 (potencia)

# Con variables (no necesitan $ dentro de (( )))
a=10
b=3
echo $((a + b))         # 13
echo $((a * b))         # 30
echo $((a - b))         # 7

# Asignar resultado
result=$((a * b + 5))
echo "$result"          # 35
```

### Operadores disponibles

| Operador | Significado | Ejemplo | Resultado |
|----------|------------|---------|-----------|
| `+` `-` `*` | Suma, resta, multiplicación | `$((7 * 3))` | 21 |
| `/` | División entera (trunca) | `$((7 / 2))` | 3 |
| `%` | Módulo (resto) | `$((7 % 2))` | 1 |
| `**` | Potencia | `$((2 ** 8))` | 256 |
| `<<` `>>` | Shift bits | `$((1 << 4))` | 16 |
| `&` `\|` `^` | AND, OR, XOR bitwise | `$((0xFF & 0x0F))` | 15 |
| `~` | NOT bitwise | `$((~0))` | -1 |
| `!` | NOT lógico | `$((!0))` | 1 |
| `&&` `\|\|` | AND, OR lógico | `$((1 && 0))` | 0 |
| `==` `!=` `<` `>` `<=` `>=` | Comparación (retorna 0 o 1) | `$((3 > 2))` | 1 |
| `? :` | Ternario | `$((a > b ? a : b))` | max(a,b) |
| `,` | Separador (evalúa ambos) | `$((a=5, b=3, a+b))` | 8 |

### Bases numéricas

```bash
echo $((0xFF))        # 255 (hexadecimal)
echo $((077))         # 63  (octal — cuidado con ceros iniciales)
echo $((2#1010))      # 10  (binario)
echo $((8#17))        # 15  (octal explícito)
echo $((16#FF))       # 255 (hex explícito)
```

**Trampa de octal**:

```bash
# ¡CUIDADO! Ceros iniciales = octal
echo $((010))    # 8 (no 10!)
echo $((008))    # ERROR: "value too great for base (error token is 008)"
                 # 8 no es dígito válido en octal
```

---

## 3. (( )) — Comando aritmético

A diferencia de `$(( ))`, no produce output. Es un **comando** cuyo exit
code depende del resultado:

```bash
# Exit code: 0 (success) si resultado ≠ 0, 1 (failure) si resultado == 0
(( 5 > 3 ))    # exit 0 (true)
(( 3 > 5 ))    # exit 1 (false)
(( 0 ))        # exit 1 (false — resultado es 0)
(( 42 ))       # exit 0 (true — resultado ≠ 0)
```

### Uso principal: condiciones y loops

```bash
a=10
b=20

# En if
if (( a > b )); then
    echo "a is greater"
elif (( a == b )); then
    echo "equal"
else
    echo "b is greater"
fi
# b is greater

# En while
i=0
while (( i < 5 )); do
    echo "$i"
    (( i++ ))
done
# 0 1 2 3 4

# En for estilo C
for (( i=0; i<5; i++ )); do
    echo "$i"
done
```

### Asignación dentro de (( ))

```bash
(( x = 5 + 3 ))
echo "$x"    # 8

(( x += 10 ))
echo "$x"    # 18

(( x++ ))
echo "$x"    # 19

(( x-- ))
echo "$x"    # 18

(( x *= 2 ))
echo "$x"    # 36
```

### (( )) vs [[ ]] para comparaciones numéricas

```bash
a=10  b=20

# [[ ]] con operadores de string que actúan como numéricos
[[ $a -lt $b ]]    # true (-lt = less than)
[[ $a -eq $b ]]    # false (-eq = equal)
[[ $a -gt $b ]]    # false (-gt = greater than)

# (( )) con operadores matemáticos naturales
(( a < b ))        # true
(( a == b ))       # false
(( a > b ))        # false
```

| `[[ ]]` | `(( ))` | Significado |
|---------|---------|-------------|
| `-eq` | `==` | Igual |
| `-ne` | `!=` | No igual |
| `-lt` | `<` | Menor que |
| `-le` | `<=` | Menor o igual |
| `-gt` | `>` | Mayor que |
| `-ge` | `>=` | Mayor o igual |

**Preferir `(( ))` para comparaciones numéricas**: más legible, acepta
operadores matemáticos estándar, no requiere `$` para variables.

---

## 4. let

`let` es equivalente a `(( ))` pero con syntax de string:

```bash
let "x = 5 + 3"
echo "$x"           # 8

let "x++" "y = x * 2"
echo "$x $y"        # 9 18

# Sin comillas, los espacios son problema:
let x=5+3            # OK (sin espacios)
let x = 5 + 3        # ERROR: let interpreta cada argumento por separado
```

**En la práctica**: `(( ))` es preferido sobre `let`. Es más legible, soporta
espacios naturalmente, y no necesita comillas.

```bash
# Preferir:
(( x = 5 + 3 ))

# Sobre:
let "x = 5 + 3"
```

---

## 5. Variables no definidas en aritmética

```bash
# ¡PELIGRO! Variables no definidas se evalúan a 0 silenciosamente
unset count
echo $((count + 1))    # 1 (count = 0 implícitamente)

# Esto puede ocultar bugs:
echo $((conut + 1))    # 1 (typo: "conut" no existe → 0, sin error)
```

Con `set -u` (nounset) activado:

```bash
set -u
echo $((count + 1))    # ERROR: count: unbound variable

# Solución: inicializar siempre
count=0
echo $((count + 1))    # 1
```

**Regla**: siempre inicializar variables numéricas antes de usarlas en
aritmética. Depender del "0 implícito" es un bug esperando a ocurrir.

---

## 6. Overflow y límites

Bash usa enteros con signo de 64 bits (en plataformas de 64 bits):

```bash
# Rango: -9223372036854775808 a 9223372036854775807
echo $((2**62))              # 4611686018427387904
echo $((2**63 - 1))          # 9223372036854775807 (MAX)
echo $((2**63))              # -9223372036854775808 (¡OVERFLOW! Se envuelve)

# Overflow silencioso — no hay error ni warning
echo $((9223372036854775807 + 1))  # -9223372036854775808
```

```
Rango de enteros Bash (64-bit):
       -9223372036854775808  ←──────→  9223372036854775807
       (-(2^63))                        (2^63 - 1)

Al superar MAX: se envuelve a MIN (silencioso, como C)
```

**División por cero** sí produce error:

```bash
echo $((10 / 0))    # bash: division by 0 (error, exit code 1)
```

---

## 7. Flotantes con bc

Para aritmética con decimales, Bash necesita herramientas externas. `bc` es
la estándar:

```bash
# Básico
echo "3.14 * 2" | bc           # 6.28

# scale = número de decimales
echo "scale=2; 10 / 3" | bc    # 3.33
echo "scale=4; 22 / 7" | bc    # 3.1428

# Asignar a variable
pi=$(echo "scale=10; 4 * a(1)" | bc -l)   # a() = arctan, -l = math library
echo "$pi"    # 3.1415926532

# Comparaciones (bc retorna 0 o 1)
if (( $(echo "3.14 > 2.71" | bc -l) )); then
    echo "pi > e"
fi
```

### Funciones de bc -l (math library)

| Función | Significado |
|---------|-------------|
| `s(x)` | seno |
| `c(x)` | coseno |
| `a(x)` | arcotangente |
| `l(x)` | logaritmo natural |
| `e(x)` | e^x |
| `sqrt(x)` | raíz cuadrada (sin -l) |

### Cálculos multi-línea

```bash
result=$(bc -l <<'EOF'
scale=2
tax_rate = 0.21
price = 49.99
tax = price * tax_rate
total = price + tax
total
EOF
)
echo "Total: $result"  # Total: 60.48
```

---

## 8. Flotantes con awk (alternativa rápida)

`awk` puede ser más conveniente que `bc` para cálculos inline:

```bash
# Cálculo simple
awk "BEGIN { printf \"%.2f\n\", 10/3 }"     # 3.33

# Con variables de Bash
price=49.99
tax=0.21
total=$(awk "BEGIN { printf \"%.2f\n\", $price * (1 + $tax) }")
echo "$total"   # 60.49

# Comparaciones
if awk "BEGIN { exit !($price > 40) }"; then
    echo "Expensive"
fi
```

### bc vs awk para flotantes

| Aspecto | bc | awk |
|---------|-----|-----|
| Precisión | Arbitraria (`scale=100`) | Double (15-16 dígitos) |
| Funciones math | `-l` (limitadas) | Completas (sin, cos, exp, log...) |
| Disponibilidad | POSIX, casi universal | POSIX, universal |
| Syntax | Su propio lenguaje | C-like |
| Para un cálculo | `echo "..." \| bc` | `awk "BEGIN{...}"` |
| Para procesar datos | No adecuado | Ideal |

**Regla práctica**: `bc` para precisión arbitraria, `awk` para todo lo demás.

---

## 9. Operador ternario y expresiones complejas

```bash
a=15  b=20

# Ternario: condición ? valor_si_true : valor_si_false
max=$(( a > b ? a : b ))
echo "Max: $max"     # 20

# Clamp (limitar a rango)
value=150
min_val=0
max_val=100
clamped=$(( value > max_val ? max_val : (value < min_val ? min_val : value) ))
echo "Clamped: $clamped"   # 100

# Absolute value
x=-42
abs=$(( x < 0 ? -x : x ))
echo "Abs: $abs"    # 42

# Sign function
sign=$(( x > 0 ? 1 : (x < 0 ? -1 : 0) ))
echo "Sign: $sign"  # -1
```

### Múltiples operaciones con coma

```bash
# El operador coma evalúa de izquierda a derecha, retorna el último
result=$(( a=5, b=a*2, c=a+b, c ))
echo "$result"     # 15
echo "$a $b $c"    # 5 10 15
```

---

## 10. Patrones comunes

### 10.1 Contadores

```bash
count=0
while read -r line; do
    (( count++ ))
done < file.txt
echo "Lines: $count"

# O más simple:
count=$(wc -l < file.txt)
```

### 10.2 Acumuladores

```bash
# Sumar una columna de números
total=0
while read -r value; do
    (( total += value ))
done <<EOF
10
20
30
40
EOF
echo "Total: $total"  # 100
```

### 10.3 Formateo con printf

```bash
# Padding con ceros
for i in {1..5}; do
    printf "file_%03d.txt\n" "$i"
done
# file_001.txt  file_002.txt  file_003.txt ...

# Conversión de base
printf "Decimal: %d\n" 0xFF     # 255
printf "Hex: %x\n" 255          # ff
printf "Octal: %o\n" 255        # 377

# Tabla alineada
printf "%-15s %8s %8s\n" "Name" "Size" "Count"
printf "%-15s %8d %8d\n" "documents" 1024 42
printf "%-15s %8d %8d\n" "images" 50000 7
# Name                Size    Count
# documents           1024       42
# images             50000        7
```

### 10.4 Aritmética en arrays

```bash
values=(10 20 30 40 50)

# Suma
sum=0
for v in "${values[@]}"; do
    (( sum += v ))
done
echo "Sum: $sum"      # 150

# Promedio (entero)
avg=$(( sum / ${#values[@]} ))
echo "Avg: $avg"      # 30

# Promedio (flotante con bc)
avg=$(echo "scale=2; $sum / ${#values[@]}" | bc)
echo "Avg: $avg"      # 30.00

# Min/Max
min=${values[0]}
max=${values[0]}
for v in "${values[@]}"; do
    (( v < min )) && min=$v
    (( v > max )) && max=$v
done
echo "Min: $min, Max: $max"  # Min: 10, Max: 50
```

### 10.5 Conversión de tamaños

```bash
bytes_to_human() {
    local bytes=$1
    if (( bytes >= 1073741824 )); then
        echo "$(echo "scale=1; $bytes/1073741824" | bc)G"
    elif (( bytes >= 1048576 )); then
        echo "$(echo "scale=1; $bytes/1048576" | bc)M"
    elif (( bytes >= 1024 )); then
        echo "$(echo "scale=1; $bytes/1024" | bc)K"
    else
        echo "${bytes}B"
    fi
}

bytes_to_human 1536         # 1.5K
bytes_to_human 2621440      # 2.5M
bytes_to_human 5368709120   # 5.0G
```

---

## 11. Aritmética y set -e

`(( ))` retorna exit code 1 cuando el resultado es 0. Con `set -e`
(errexit), esto **mata el script**:

```bash
set -e

count=0
(( count++ ))    # count pasa de 0 a 1, pero el VALOR ANTERIOR era 0
                 # x++ retorna el valor antes del incremento = 0
                 # exit code = 1 → set -e MATA el script

echo "Never reached"
```

```
¿Por qué? Post-incremento retorna el valor ANTERIOR:

count=0
(( count++ ))   → valor retornado: 0 (el valor antes de incrementar)
                → exit code: 1 (porque resultado == 0)
                → set -e: "exit code no-zero! Abortar!"

count ya vale 1, pero es tarde — el script ya murió.
```

### Soluciones

```bash
# Solución 1: pre-incremento (retorna valor DESPUÉS)
(( ++count ))     # retorna 1, exit code 0 ✓

# Solución 2: asegurar resultado no-zero
(( count++ )) || true

# Solución 3: usar asignación en expansión
count=$((count + 1))   # $(( )) no afecta exit code del script

# Solución 4: prefijo con :
: $(( count++ ))       # : siempre retorna 0
```

**Recomendación**: para incrementos simples, usar `count=$((count + 1))` o
`(( ++count ))`. Evitar `(( count++ ))` con `set -e`.

---

## 12. $RANDOM

Bash tiene un generador de números pseudo-aleatorios:

```bash
echo $RANDOM            # 0-32767 (15 bits)

# Rango personalizado: 1-100
echo $(( RANDOM % 100 + 1 ))

# Rango sin sesgo (para rangos que no dividen 32768 limpiamente):
# El módulo tiene sesgo si 32768 % rango != 0
# Para scripts no-criptográficos, el sesgo es despreciable

# Semilla (para resultados reproducibles)
RANDOM=42
echo $RANDOM    # siempre el mismo valor para la misma semilla

# Mejor aleatoriedad: /dev/urandom
echo $(( $(od -An -tu4 -N4 /dev/urandom) % 100 + 1 ))
```

### Uso práctico: selección aleatoria

```bash
# Elegir un elemento aleatorio de un array
options=("deploy" "rollback" "restart" "status")
choice="${options[RANDOM % ${#options[@]}]}"
echo "Random action: $choice"

# Sleep aleatorio (jitter para evitar thundering herd)
max_jitter=30
sleep $(( RANDOM % max_jitter ))
```

---

## 13. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| `$((3.14 + 1))` | Bash no soporta floats | Usar `bc` o `awk` |
| `$((010))` = 8 | Ceros iniciales = octal | Eliminar ceros: `$((10#010))` |
| `(( count++ ))` con `set -e` | Post-incremento de 0 mata el script | `(( ++count ))` o `count=$((count+1))` |
| `$((var + 1))` con var no definida | Silenciosamente usa 0 | Inicializar: `var=0` |
| `$((2**63))` overflow | Se envuelve sin warning | Verificar rango o usar `bc` |
| `echo $((10 / 0))` | Error fatal | Verificar divisor antes |
| `let x = 5 + 3` | Espacios rompen el parsing | `let "x = 5 + 3"` o `(( x = 5+3 ))` |
| `[[ $a > $b ]]` para números | Comparación lexicográfica, no numérica | `(( a > b ))` o `[[ $a -gt $b ]]` |
| `(( a == 05 ))` | 05 es octal (= 5 decimal, coincide) | Ojo con inputs con padding |
| Mezclar `$` y no-`$` en `(( ))` | Ambos funcionan pero inconsistente | Sin `$` dentro de `(( ))` |

---

## 14. Ejercicios

### Ejercicio 1 — Expansión aritmética básica

**Predicción**: ¿qué imprime cada línea?

```bash
echo $((7 / 2))
echo $((7 % 2))
echo $((2 ** 10))
echo $((0xFF))
echo $((2#1100))
```

<details>
<summary>Respuesta</summary>

```
3
1
1024
255
12
```

- `7 / 2` → 3 (división entera, trunca el .5)
- `7 % 2` → 1 (resto: 7 = 3*2 + 1)
- `2 ** 10` → 1024 (potencia)
- `0xFF` → 255 (hexadecimal)
- `2#1100` → 12 (binario: 8+4=12)
</details>

---

### Ejercicio 2 — Trampa del octal

**Predicción**: ¿qué imprime?

```bash
echo $((10))
echo $((010))
echo $((0x10))
```

<details>
<summary>Respuesta</summary>

```
10
8
16
```

- `10` → decimal: 10
- `010` → octal (cero inicial): 1×8 + 0 = 8
- `0x10` → hexadecimal: 1×16 + 0 = 16

Es la misma trampa que en C. Los ceros iniciales activan interpretación
octal. Esto es especialmente problemático al procesar fechas o horas:

```bash
hour="08"
(( hour + 1 ))   # ERROR: "08" no es octal válido (8 no existe en octal)
```

Solución: forzar base 10 con `10#`:
```bash
(( 10#$hour + 1 ))  # 9 ✓
```
</details>

---

### Ejercicio 3 — (( )) como condición

**Predicción**: ¿qué imprime?

```bash
x=0
if (( x )); then
    echo "truthy"
else
    echo "falsy"
fi

x=42
if (( x )); then
    echo "truthy"
else
    echo "falsy"
fi

if (( x > 100 )); then
    echo "big"
else
    echo "small"
fi
```

<details>
<summary>Respuesta</summary>

```
falsy
truthy
small
```

- `(( 0 ))` → resultado es 0 → exit code 1 → falso
- `(( 42 ))` → resultado ≠ 0 → exit code 0 → verdadero
- `(( 42 > 100 ))` → 42 > 100 es falso → resultado 0 → exit code 1

`(( ))` funciona como C: 0 es falso, cualquier otro valor es verdadero.
El exit code es el **inverso**: 0 = success (true), 1 = failure (false).
</details>

---

### Ejercicio 4 — Post-incremento y set -e

**Predicción**: ¿qué imprime este script?

```bash
#!/bin/bash
set -e

a=0
(( a++ ))
echo "a = $a"

b=1
(( b++ ))
echo "b = $b"
```

<details>
<summary>Respuesta</summary>

El script muere en la línea `(( a++ ))` y no imprime nada.

```
a=0
(( a++ ))
  → post-incremento: retorna valor ANTERIOR (0)
  → resultado = 0
  → exit code = 1
  → set -e: exit code no-zero → ABORT

"a = $a" nunca se ejecuta.
```

Si `a` fuera 1 en lugar de 0:
```
a=1
(( a++ ))
  → retorna 1 (valor anterior)
  → exit code = 0
  → OK, continúa
```

La línea `(( b++ ))` sí funcionaría porque `b=1`, y `b++` retorna 1.

Solución: usar `(( ++a ))` (pre-incremento retorna el nuevo valor: 1).
</details>

---

### Ejercicio 5 — Ternario

**Predicción**: ¿qué imprime?

```bash
a=15  b=27  c=8

max=$(( a > b ? a : b ))
max=$(( max > c ? max : c ))
echo "Max: $max"

sign=$(( a - b ))
result=$(( sign > 0 ? 1 : (sign < 0 ? -1 : 0) ))
echo "Sign: $result"
```

<details>
<summary>Respuesta</summary>

```
Max: 27
Sign: -1
```

Primer ternario:
- `a > b` → 15 > 27 → false → `max = b` = 27
- `max > c` → 27 > 8 → true → `max = max` = 27

Segundo:
- `sign = a - b` = 15 - 27 = -12
- `sign > 0` → false → evalúa la rama else
- `sign < 0` → -12 < 0 → true → result = -1

El ternario anidado `(( cond ? val : (cond2 ? val2 : val3) ))` es el
equivalente Bash de un if-elif-else inline.
</details>

---

### Ejercicio 6 — bc para flotantes

**Predicción**: ¿qué imprime cada línea?

```bash
echo "scale=2; 10 / 3" | bc
echo "scale=0; 10 / 3" | bc
echo "10 / 3" | bc
echo "scale=4; 22 / 7" | bc
```

<details>
<summary>Respuesta</summary>

```
3.33
3
3
3.1428
```

- `scale=2; 10/3` → 3.33 (2 decimales)
- `scale=0; 10/3` → 3 (0 decimales = entero)
- `10/3` sin scale → 3 (scale por defecto es 0)
- `scale=4; 22/7` → 3.1428 (4 decimales)

`scale` controla cuántos decimales se calculan. Sin especificar, `bc` hace
división entera (scale=0). Con `bc -l`, el scale por defecto es 20.
</details>

---

### Ejercicio 7 — Comparación numérica vs lexicográfica

**Predicción**: ¿cuál es el resultado de cada comparación?

```bash
a=9  b=10

# Test A: [[ ]] con string comparison
if [[ "$a" > "$b" ]]; then echo "A: a > b"; else echo "A: a <= b"; fi

# Test B: [[ ]] con -gt
if [[ "$a" -gt "$b" ]]; then echo "B: a > b"; else echo "B: a <= b"; fi

# Test C: (( ))
if (( a > b )); then echo "C: a > b"; else echo "C: a <= b"; fi
```

<details>
<summary>Respuesta</summary>

```
A: a > b
B: a <= b
C: a <= b
```

- **Test A**: `[[ "9" > "10" ]]` es comparación **lexicográfica** (de strings).
  "9" > "10" porque el carácter '9' (ASCII 57) > '1' (ASCII 49). ¡Incorrecto
  numéricamente!

- **Test B**: `[[ "9" -gt "10" ]]` usa `-gt` que fuerza comparación **numérica**.
  9 > 10 es falso. Correcto.

- **Test C**: `(( 9 > 10 ))` es contexto aritmético. 9 > 10 es falso. Correcto.

**Lección**: nunca usar `>`, `<` en `[[ ]]` para comparar números. Usar
`-gt`, `-lt` o `(( ))`.
</details>

---

### Ejercicio 8 — Acumulador con array

**Predicción**: ¿qué imprime?

```bash
scores=(85 92 78 95 88)
sum=0
count=${#scores[@]}

for s in "${scores[@]}"; do
    (( sum += s ))
done

avg=$(( sum / count ))
echo "Sum: $sum"
echo "Avg (int): $avg"
echo "Avg (float): $(echo "scale=1; $sum / $count" | bc)"
```

<details>
<summary>Respuesta</summary>

```
Sum: 438
Avg (int): 87
Avg (float): 87.6
```

- Suma: 85 + 92 + 78 + 95 + 88 = 438
- Promedio entero: 438 / 5 = 87 (truncado, no redondeado)
- Promedio float: 438 / 5 = 87.6 (bc con scale=1)

Nota: la división entera de Bash **trunca** (hacia cero), no redondea.
438/5 = 87.6 → Bash retorna 87, no 88.

Para redondear al entero más cercano:
```bash
avg=$(( (sum + count/2) / count ))  # 87 (funciona para positivos)
```
</details>

---

### Ejercicio 9 — Conversión de tamaños

**Predicción**: ¿qué imprime para cada input?

```bash
to_bytes() {
    local input="$1"
    local num="${input%[KMG]}"
    local suffix="${input: -1}"
    case "$suffix" in
        K) echo $(( num * 1024 )) ;;
        M) echo $(( num * 1024 * 1024 )) ;;
        G) echo $(( num * 1024 * 1024 * 1024 )) ;;
        *) echo "$input" ;;
    esac
}

to_bytes "4K"
to_bytes "2M"
to_bytes "1G"
to_bytes "512"
```

<details>
<summary>Respuesta</summary>

```
4096
2097152
1073741824
512
```

- `"4K"`: num=4, suffix=K → 4 × 1024 = 4096
- `"2M"`: num=2, suffix=M → 2 × 1024 × 1024 = 2097152
- `"1G"`: num=1, suffix=G → 1 × 1024 × 1024 × 1024 = 1073741824
- `"512"`: suffix=2 (último char), no coincide con K/M/G → caso `*` → 512

Las expansiones usadas:
- `${input%[KMG]}` → elimina sufijo K, M, o G → deja solo el número
- `${input: -1}` → último carácter (el espacio es obligatorio antes del `-`)
</details>

---

### Ejercicio 10 — $RANDOM y distribución

**Predicción**: ¿cuál es el rango posible de cada expresión?

```bash
echo $(( RANDOM % 6 + 1 ))
echo $(( RANDOM % 100 ))
echo $(( RANDOM * RANDOM ))
```

<details>
<summary>Respuesta</summary>

- `RANDOM % 6 + 1` → rango: **1 a 6** (dado)
  - RANDOM = 0..32767, módulo 6 = 0..5, +1 = 1..6

- `RANDOM % 100` → rango: **0 a 99**
  - RANDOM = 0..32767, módulo 100 = 0..99
  - Nota: hay un ligero sesgo porque 32768 % 100 ≠ 0
    (los valores 0..67 aparecen 328 veces, 68..99 aparecen 327)
    Para scripts no-criptográficos, este sesgo es despreciable.

- `RANDOM * RANDOM` → rango: **0 a 1,073,676,289** (32767²)
  - Mínimo: 0 × 0 = 0
  - Máximo: 32767 × 32767 = 1,073,676,289
  - La distribución NO es uniforme: valores cercanos a 0 y al máximo son
    mucho menos probables que los del medio.
  - Para ampliar el rango de $RANDOM, mejor usar:
    `$(( RANDOM * 32768 + RANDOM ))` → rango 0..1,073,741,823 (30 bits, uniforme)
</details>