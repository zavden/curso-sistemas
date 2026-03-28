# Lab — if/else, switch

## Objetivo

Practicar branching con if/else y switch, observar como el compilador genera
codigo distinto para cada uno, y reconocer los errores clasicos de control de
flujo. Al finalizar, sabras cuando usar if vs switch, como funciona el
fall-through, y como los warnings de GCC te protegen de bugs comunes.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `branching.c` | if/else basico, condiciones encadenadas, truthiness |
| `switch_basic.c` | switch con break, fall-through, agrupacion de cases |
| `if_vs_switch.c` | Misma logica implementada con if y con switch |
| `dangling_else.c` | Bug de dangling else y version corregida |
| `switch_nobreak.c` | Bug de switch sin break y version corregida |
| `assign_vs_equal.c` | Bug de = vs == en condiciones |

---

## Parte 1 — if/else if/else: branching basico

**Objetivo**: Entender branching con if/else, condiciones encadenadas, y como
C evalua verdadero/falso.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat branching.c
```

Observa tres bloques:

- Un if/else simple (pass/fail)
- Una cadena if/else if/else (grades A-F)
- Un loop que prueba truthiness con valores 42, 0, -1, 1

Antes de compilar, predice:

- Que grade recibira un score de 75?
- De los valores {42, 0, -1, 1}, cuales evaluara C como true?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic branching.c -o branching
./branching
```

Salida esperada:

```
Passed
Grade: C
42 is true
0 is false
-1 is true
1 is true
```

Verifica tus predicciones:

- 75 >= 70, asi que entra en el tercer `else if` y recibe Grade C. Las
  condiciones se evaluan de arriba hacia abajo y la primera que se cumple gana.
- En C, **cualquier valor distinto de cero es true**. Solo 0 es false. Esto
  incluye numeros negativos como -1.

### Paso 1.3 — Observar los warnings

Nota que la compilacion no produjo ningun warning. Esto confirma que el codigo
es limpio segun `-Wall -Wextra -Wpedantic`.

### Limpieza de la parte 1

```bash
rm -f branching
```

---

## Parte 2 — switch: break, fall-through y default

**Objetivo**: Entender la mecanica de switch, el rol de break, y como usar
fall-through de forma intencional.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat switch_basic.c
```

Observa tres bloques:

1. switch basico con break en cada case (dias de la semana)
2. Fall-through para agrupar cases (vocales vs consonantes)
3. Fall-through acumulativo con `__attribute__((fallthrough))` (niveles de
   acceso)

Antes de compilar, predice:

- Con `day = 3`, que dia imprimira?
- Con `c = 'e'`, que rama tomara el switch de vocales?
- Con `level = 2`, que accesos se imprimiran? (pista: no hay break entre
  case 2 y case 1)

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic switch_basic.c -o switch_basic
./switch_basic
```

Salida esperada:

```
Day 3: Wednesday
'e' is a vowel
Access level 2 grants:
  - write access
  - read access
```

Verifica tus predicciones:

- `day = 3` entra en `case 3` e imprime Wednesday. El `break` evita que caiga
  al case 4.
- `c = 'e'` cae por los cases vacios `'a'`, `'e'`, `'i'`, `'o'`, `'u'` hasta
  llegar al `printf("vowel")`. Esta agrupacion de cases es el uso idoneo del
  fall-through.
- `level = 2` entra en case 2, imprime "write access", y como no hay break,
  cae a case 1 e imprime tambien "read access". El `__attribute__((fallthrough))`
  le dice a GCC que esto es intencional.

### Paso 2.3 — Verificar que no hay warnings

La compilacion no produjo warnings porque el fall-through intencional esta
marcado con `__attribute__((fallthrough))`. Sin esa marca, GCC daria un
warning de `-Wimplicit-fallthrough`.

### Limpieza de la parte 2

```bash
rm -f switch_basic
```

---

## Parte 3 — Comparacion if vs switch: assembly generado

**Objetivo**: Ver como el compilador traduce if/else if y switch a assembly,
y como la optimizacion cambia radicalmente el codigo generado.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat if_vs_switch.c
```

Observa que `classify_if()` y `classify_switch()` implementan **exactamente
la misma logica** — clasificar un codigo entero en una cadena. Una usa
if/else if, la otra usa switch.

### Paso 3.2 — Compilar y verificar que ambas son equivalentes

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic if_vs_switch.c -o if_vs_switch
./if_vs_switch
```

Salida esperada:

```
code 0: if=INVALID, switch=INVALID
code 1: if=OK, switch=OK
code 2: if=WARNING, switch=WARNING
code 3: if=ERROR, switch=ERROR
code 4: if=CRITICAL, switch=CRITICAL
code 5: if=FATAL, switch=FATAL
code 6: if=UNKNOWN, switch=UNKNOWN
code 7: if=TIMEOUT, switch=TIMEOUT
code 8: if=RETRY, switch=RETRY
code 9: if=INVALID, switch=INVALID
```

Ambas funciones producen los mismos resultados. Ahora veamos como difiere el
assembly.

### Paso 3.3 — Assembly sin optimizacion (-O0)

```bash
gcc -std=c11 -S -O0 if_vs_switch.c -o if_vs_switch_O0.s
```

Cuenta las instrucciones de comparacion/salto en cada funcion:

```bash
echo "=== classify_if (comparaciones y saltos) ==="
sed -n '/^classify_if:/,/^\.size.*classify_if/p' if_vs_switch_O0.s | grep -c "cmp\|je\|jne\|jmp"

echo "=== classify_switch (comparaciones y saltos) ==="
sed -n '/^classify_switch:/,/^\.size.*classify_switch/p' if_vs_switch_O0.s | grep -c "cmp\|je\|jne\|jmp"
```

Salida esperada (los numeros pueden variar):

```
=== classify_if (comparaciones y saltos) ===
~57
=== classify_switch (comparaciones y saltos) ===
~33
```

Sin optimizacion, `classify_if` tiene mas instrucciones de comparacion y salto
que `classify_switch`. Ambas usan una cadena de comparaciones secuenciales,
pero la estructura del if/else if genera mas saltos condicionales porque cada
`else if` implica un salto adicional si la condicion anterior fallo.

### Paso 3.4 — Assembly con optimizacion (-O2)

```bash
gcc -std=c11 -S -O2 if_vs_switch.c -o if_vs_switch_O2.s
```

Antes de examinar, predice:

- Con -O2, el compilador reconocera que ambas funciones hacen lo mismo?
- Cuantas instrucciones de comparacion/salto tendra cada funcion?

```bash
echo "=== classify_if (O2) ==="
sed -n '/^classify_if:/,/^\.size.*classify_if/p' if_vs_switch_O2.s | grep -c "cmp\|je\|jne\|jmp\|ja\|jb"

echo "=== classify_switch (O2) ==="
sed -n '/^classify_switch:/,/^\.size.*classify_switch/p' if_vs_switch_O2.s | grep -c "cmp\|je\|jne\|jmp\|ja\|jb"
```

Salida esperada:

```
=== classify_if (O2) ===
~2
=== classify_switch (O2) ===
~2
```

Con `-O2`, el compilador optimizo **ambas** funciones a una lookup table (una
tabla de punteros en `.rodata`). En lugar de hacer 8 comparaciones, hace una
sola comparacion de rango y un salto indirecto a la tabla.

### Paso 3.5 — Buscar la lookup table en el assembly

```bash
grep "CSWTCH" if_vs_switch_O2.s
```

Veras referencias a `CSWTCH` — una tabla de datos generada por el compilador
donde cada entrada es un puntero a la cadena correspondiente. El acceso es
`O(1)` en lugar de `O(n)`.

### Paso 3.6 — Comparar tamanos totales

```bash
wc -l if_vs_switch_O0.s if_vs_switch_O2.s
```

Salida esperada:

```
 ~200 if_vs_switch_O0.s
 ~120 if_vs_switch_O2.s
```

La version optimizada es significativamente mas corta.

La leccion clave: con un compilador moderno y optimizaciones habilitadas, la
diferencia de rendimiento entre if/else if y switch es **minima**. El
compilador es capaz de reconocer patrones y optimizar ambas formas. Elige la
estructura que sea mas **legible** para tu caso — switch cuando comparas una
variable contra constantes, if/else cuando las condiciones son expresiones
complejas.

### Limpieza de la parte 3

```bash
rm -f if_vs_switch if_vs_switch_O0.s if_vs_switch_O2.s
```

---

## Parte 4 — Errores comunes

**Objetivo**: Reconocer tres bugs clasicos de control de flujo en C y ver como
los warnings de GCC ayudan a detectarlos.

### Paso 4.1 — Dangling else

```bash
cat dangling_else.c
```

Observa `test_dangling()`: la indentacion sugiere que el `else` pertenece al
`if (a > 0)` exterior, pero en C el else siempre se asocia al if **mas
cercano**.

Antes de compilar, predice la salida para estas entradas:

- `a=1, b=1`
- `a=1, b=-1`
- `a=-1, b=1`
- `a=-1, b=-1`

### Paso 4.2 — Compilar y ejecutar dangling_else

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic dangling_else.c -o dangling_else
```

Observa el warning:

```
dangling_else.c: warning: suggest explicit braces to avoid ambiguous 'else' [-Wdangling-else]
```

GCC detecta la ambiguedad y recomienda usar llaves. Ahora ejecuta:

```bash
./dangling_else
```

Salida esperada:

```
=== Dangling else (buggy) ===
a=1, b=1: both positive
a=1, b=-1: this looks like it handles a<=0, but it doesn't
a=-1, b=1: a=-1, b=-1:
=== Fixed with braces ===
a=1, b=1: both positive
a=1, b=-1: a=-1, b=1: a is not positive
a=-1, b=-1: a is not positive
```

Analisis de la version con bug:

- `a=1, b=-1`: el else se ejecuta porque `b > 0` es falso. Pero el mensaje
  dice "handles a<=0", lo cual es incorrecto — `a` es 1.
- `a=-1, b=1`: ni el if interno ni su else se ejecutan porque el if externo
  `a > 0` ya es falso. No se imprime nada despues de los dos puntos.
- `a=-1, b=-1`: mismo caso — el if externo es falso, no se ejecuta nada.

En la version corregida con llaves, el else pertenece claramente al if
externo, y el comportamiento es el esperado.

### Paso 4.3 — Switch sin break

```bash
cat switch_nobreak.c
```

Observa la primera version: los cases 1, 2 y 3 no tienen break. Predice que
imprimira con `command = 1`.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic switch_nobreak.c -o switch_nobreak
```

Observa los warnings:

```
switch_nobreak.c: warning: this statement may fall through [-Wimplicit-fallthrough=]
```

GCC detecta el fall-through no marcado. Ejecuta:

```bash
./switch_nobreak
```

Salida esperada:

```
command = 1
Executing: START STOP RESET

Fixed version:
command = 1
Executing: START
```

Con `command = 1`, la version con bug ejecuta START, luego cae a STOP, y luego
cae a RESET. Solo cuando encuentra el `break` de case 3 se detiene. La version
corregida ejecuta solo START.

### Paso 4.4 — Asignacion vs comparacion (= vs ==)

```bash
cat assign_vs_equal.c
```

Observa el `if (x = 5)` — esto **asigna** 5 a x y luego evalua 5 como true.
Predice:

- Se ejecutara la rama true o la false?
- Cual sera el valor de x despues del if?

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic assign_vs_equal.c -o assign_vs_equal
```

Observa el warning:

```
assign_vs_equal.c: warning: suggest parentheses around assignment used as truth value [-Wparentheses]
```

GCC detecta que una asignacion se usa como condicion booleana. Ejecuta:

```bash
./assign_vs_equal
```

Salida esperada:

```
=== Bug: = instead of == ===
x before: 0
x is 5? x = 5 (branch taken)
x after: 5 (x was modified!)

=== Correct: == ===
x before: 0
x is not 5 (branch taken)
x after: 0 (x unchanged)
```

Con `=`, la condicion siempre es true (5 es no-cero) y x queda modificado.
Con `==`, la condicion evalua correctamente y x no se modifica.

Leccion: siempre compilar con `-Wall -Wextra`. Estos tres bugs (dangling else,
switch sin break, = vs ==) son detectados por warnings de GCC.

### Limpieza de la parte 4

```bash
rm -f dangling_else switch_nobreak assign_vs_equal
```

---

## Limpieza final

```bash
rm -f branching switch_basic if_vs_switch dangling_else switch_nobreak assign_vs_equal
rm -f if_vs_switch_O0.s if_vs_switch_O2.s
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  assign_vs_equal.c  branching.c  dangling_else.c  if_vs_switch.c  switch_basic.c  switch_nobreak.c
```

---

## Conceptos reforzados

1. En C, cualquier valor distinto de cero es true y solo 0 es false. Esto
   incluye numeros negativos, punteros no-NULL y caracteres distintos de `'\0'`.

2. Las condiciones de un if/else if se evaluan de arriba hacia abajo. La
   primera que se cumple gana y las demas se saltan.

3. Cada `case` de un switch necesita un `break` explicito. Sin break, la
   ejecucion cae al siguiente case (fall-through).

4. El fall-through es util para agrupar cases con el mismo comportamiento
   (vocales, meses con 31 dias) o para acumular acciones (niveles de acceso).
   Debe marcarse con `__attribute__((fallthrough))` para silenciar warnings.

5. Con `-O0`, el compilador genera comparaciones secuenciales para ambas
   estructuras. Con `-O2`, puede optimizar tanto if/else if como switch a
   lookup tables, haciendo la diferencia de rendimiento despreciable.

6. El bug del dangling else ocurre porque en C el else se asocia al if mas
   cercano, no al que la indentacion sugiere. Usar llaves siempre elimina la
   ambiguedad.

7. Olvidar break en un switch causa fall-through accidental — el case ejecuta
   su codigo y tambien el del case siguiente. `-Wimplicit-fallthrough` detecta
   este error.

8. Usar `=` (asignacion) en lugar de `==` (comparacion) dentro de un if es
   un bug que siempre evalua como true (si el valor asignado es no-cero) y
   modifica la variable. `-Wparentheses` lo detecta.
