# Lab -- Comportamiento indefinido (UB)

## Objetivo

Provocar comportamiento indefinido de forma controlada, observar como los
sanitizers lo detectan, y ver como el optimizador lo explota para transformar
el programa. Al finalizar, sabras usar `-fsanitize=undefined` para encontrar
UB y aplicar los patrones correctos para evitarlo.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `ub_showcase.c` | 4 funciones con UB clasicos: overflow, out-of-bounds, uninit, shift |
| `ub_optimizer.c` | 2 funciones donde el optimizador explota UB para cambiar el resultado |
| `ub_fixed.c` | Version corregida de ub_showcase.c con patrones seguros |

---

## Parte 1 -- UB en accion

**Objetivo**: Compilar un programa con UB de cuatro formas distintas y observar
como cada nivel de diagnostico revela (o no) los errores.

### Paso 1.1 -- Compilacion sin protecciones

```bash
gcc ub_showcase.c -o ub_showcase
```

Sin `-Wall`, sin sanitizers. GCC no emite ninguna advertencia.

```bash
./ub_showcase
```

Salida esperada (los valores de las lineas 2 y 3 varian):

```
=== UB Showcase ===

1. Signed overflow  (INT_MAX + 1): -2147483648
2. Out-of-bounds    (arr[7]):      <valor basura>
3. Uninitialized    (int x; x>0):  <0 o 1>
4. Bad shift        (1 << 32):     <valor impredecible>

All functions returned without crashing.
That does NOT mean the code is correct.
```

El programa "funciona" -- no crashea, no da error. Esto es exactamente lo que
hace peligroso al UB: el silencio.

### Paso 1.2 -- Activar warnings

```bash
gcc -Wall -Wextra ub_showcase.c -o ub_showcase_warn
```

Salida esperada (los warnings pueden variar segun version de GCC):

```
ub_showcase.c: In function 'uninitialized_use':
ub_showcase.c:33:8: warning: 'x' is used uninitialized [-Wuninitialized]
ub_showcase.c: In function 'bad_shift':
ub_showcase.c:40:15: warning: left shift count >= width of type [-Wshift-count-overflow]
```

Observa:

- GCC detecto 2 de los 4 problemas con `-Wall -Wextra`
- El signed overflow y el acceso out-of-bounds NO generan warning
- Los warnings son utiles pero **no son suficientes**

### Paso 1.3 -- Activar UBSan

```bash
gcc -fsanitize=undefined -g ub_showcase.c -o ub_showcase_ubsan
./ub_showcase_ubsan
```

Ahora el sanitizer intercepta cada UB en runtime. Salida esperada:

```
=== UB Showcase ===

ub_showcase.c:18:17: runtime error: signed integer overflow:
  2147483647 + 1 cannot be represented in type 'int'
1. Signed overflow  (INT_MAX + 1): -2147483648
ub_showcase.c:24:16: runtime error: index 7 out of bounds for type 'int [5]'
2. Out-of-bounds    (arr[7]):      <valor>
3. Uninitialized    (int x; x>0):  <valor>
ub_showcase.c:40:15: runtime error: shift exponent 32 is too large for
  32-bit type 'int'
4. Bad shift        (1 << 32):     <valor>
```

UBSan detecto 3 de los 4 UB (el acceso out-of-bounds puede requerir
`-fsanitize=address` para deteccion completa). Cada mensaje indica:

- El archivo y linea exactos
- El tipo de error
- Los valores concretos involucrados

Antes de continuar, predice: si compilas con `gcc -O0` (sin sanitizers),
el programa crasheara? Y con `gcc -O2`?

### Paso 1.4 -- Verificar la prediccion

```bash
gcc -O0 ub_showcase.c -o ub_showcase_O0
./ub_showcase_O0
echo "---"
gcc -O2 ub_showcase.c -o ub_showcase_O2
./ub_showcase_O2
```

Con `-O0`, es probable que el programa no crashee -- el compilador genera
codigo "literal" que accede a la memoria tal cual. Con `-O2`, el compilador
puede eliminar funciones enteras, cambiar valores, o provocar comportamiento
distinto. Los resultados de las lineas 2-4 pueden cambiar entre ambas
versiones.

La leccion: UB no es "crash o no crash" -- es "el compilador puede hacer
lo que quiera".

### Limpieza de la parte 1

```bash
rm -f ub_showcase ub_showcase_warn ub_showcase_ubsan ub_showcase_O0 ub_showcase_O2
```

---

## Parte 2 -- El optimizador explota UB

**Objetivo**: Demostrar que el compilador no ignora el UB -- lo **usa** como
informacion para optimizar. El mismo codigo fuente produce resultados distintos
segun el nivel de optimizacion.

### Paso 2.1 -- Compilar con -O0 y -O2

```bash
gcc -O0 ub_optimizer.c -o ub_opt_O0
gcc -O2 ub_optimizer.c -o ub_opt_O2
```

Antes de ejecutar, predice:

- La funcion `overflow_check(INT_MAX)` retornara 0 o 1 con `-O0`?
- Y con `-O2`?

Pista: con `-O0`, `INT_MAX + 1` wrappea a un numero negativo en la mayoria
de plataformas (complemento a dos). Con `-O2`, el compilador razona
diferente.

### Paso 2.2 -- Ejecutar y comparar

```bash
echo "=== -O0 ==="
./ub_opt_O0
echo ""
echo "=== -O2 ==="
./ub_opt_O2
```

Salida esperada con -O0:

```
=== Optimizer exploiting UB ===

1. null_check_after_deref(&value): 43
   (The NULL check is silently removed at -O2)

2. overflow_check(INT_MAX): 0
   Expected with -O0: 0  (wrapping makes x+1 negative)
   Expected with -O2: 1  (compiler assumes no overflow)
```

Salida esperada con -O2:

```
=== Optimizer exploiting UB ===

1. null_check_after_deref(&value): 43
   (The NULL check is silently removed at -O2)

2. overflow_check(INT_MAX): 1
   Expected with -O0: 0  (wrapping makes x+1 negative)
   Expected with -O2: 1  (compiler assumes no overflow)
```

El resultado de `overflow_check` cambio:

- `-O0`: `INT_MAX + 1` wrappea a `-2147483648`. Comparar `-2147483648 > INT_MAX`
  es false, por lo tanto retorna 0.
- `-O2`: el compilador razona "signed overflow es UB, por lo tanto no puede
  ocurrir. Si no hay overflow, entonces `x + 1 > x` es siempre true." Reduce
  la funcion entera a `return 1`.

### Paso 2.3 -- Ver la diferencia en el assembly

```bash
gcc -S -O0 ub_optimizer.c -o ub_opt_O0.s
gcc -S -O2 ub_optimizer.c -o ub_opt_O2.s
```

Examina la funcion `overflow_check` en ambos:

```bash
echo "=== -O0: overflow_check ==="
sed -n '/overflow_check/,/\.size.*overflow_check/p' ub_opt_O0.s

echo ""
echo "=== -O2: overflow_check ==="
sed -n '/overflow_check/,/\.size.*overflow_check/p' ub_opt_O2.s
```

Con `-O0`, veras instrucciones de comparacion (`cmp`, `jle` o similar).
Con `-O2`, la funcion probablemente se reduce a:

```asm
overflow_check:
    movl    $1, %eax
    ret
```

El compilador elimino toda la logica de comparacion. Esto no es un bug del
compilador -- es una optimizacion valida segun el estandar de C.

### Paso 2.4 -- Examinar la eliminacion del null check

```bash
echo "=== -O0: null_check_after_deref ==="
sed -n '/null_check_after_deref/,/\.size.*null_check_after_deref/p' ub_opt_O0.s | head -30

echo ""
echo "=== -O2: null_check_after_deref ==="
sed -n '/null_check_after_deref/,/\.size.*null_check_after_deref/p' ub_opt_O2.s | head -30
```

Con `-O0`, veras la comparacion con cero (`test`, `je` o similar) -- el null
check esta presente. Con `-O2`, el compilador razona: "p fue desreferenciado en
`int x = *p`, por lo tanto p no puede ser NULL. El `if (p == NULL)` es codigo
muerto." La comparacion desaparece del assembly.

### Limpieza de la parte 2

```bash
rm -f ub_opt_O0 ub_opt_O2 ub_opt_O0.s ub_opt_O2.s
```

---

## Parte 3 -- Corregir UB

**Objetivo**: Aplicar los patrones seguros que reemplazan cada UB, y verificar
con sanitizers que el codigo corregido no tiene errores.

### Paso 3.1 -- Examinar el codigo corregido

```bash
cat ub_fixed.c
```

Observa los cuatro cambios respecto a `ub_showcase.c`:

| UB original | Correccion aplicada |
|-------------|---------------------|
| `INT_MAX + 1` (signed overflow) | Usar `unsigned int` (wrapping definido) |
| `arr[7]` (out-of-bounds) | Verificar `index >= 0 && index < size` |
| `int x;` sin inicializar | Inicializar `int x = 0;` |
| `1 << 32` (shift invalido) | Verificar `shift < sizeof(int) * 8` |

### Paso 3.2 -- Compilar con sanitizers completos

```bash
gcc -Wall -Wextra -fsanitize=address,undefined -g ub_fixed.c -o ub_fixed
./ub_fixed
```

Salida esperada:

```
=== UB Fixed ===

  Index 7 out of bounds [0, 5)
  Shift 32 exceeds int width (32 bits)
1. Safe wrapping   (UINT_MAX + 1): 0
2. Safe access     (index 7):      -1
3. Safe init       (x = 0; x>0):   0
4. Safe shift      (shift 32):     0

All functions use defined behavior.
Sanitizers report zero errors.
```

Cero errores de sanitizer. Los casos "problematicos" (indice fuera de rango,
shift invalido) ahora se manejan con un mensaje explicito y un valor de retorno
seguro en lugar de provocar UB silencioso.

### Paso 3.3 -- Comparar directamente

Para ver el contraste, ejecuta ambas versiones con UBSan:

```bash
gcc -fsanitize=undefined -g ub_showcase.c -o ub_showcase_check
gcc -fsanitize=undefined -g ub_fixed.c -o ub_fixed_check

echo "=== ub_showcase (con UB) ==="
./ub_showcase_check 2>&1 | grep "runtime error" | wc -l
echo "errores de UBSan"

echo ""
echo "=== ub_fixed (corregido) ==="
./ub_fixed_check 2>&1 | grep "runtime error" | wc -l
echo "errores de UBSan"
```

Salida esperada:

```
=== ub_showcase (con UB) ===
3
errores de UBSan
=== ub_fixed (corregido) ===
0
errores de UBSan
```

### Paso 3.4 -- El set de compilacion para desarrollo

El patron que debes usar siempre que desarrolles en C:

```bash
gcc -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g ub_fixed.c -o ub_fixed_dev
./ub_fixed_dev
```

Este comando activa:

| Flag | Que hace |
|------|----------|
| `-Wall -Wextra -Wpedantic` | Maximos warnings en compilacion |
| `-fsanitize=address` | Detecta errores de memoria (out-of-bounds, use-after-free) |
| `-fsanitize=undefined` | Detecta UB (overflow, shift, null deref) |
| `-g` | Incluye info de debug para que los sanitizers muestren lineas exactas |

Para produccion, se usan flags distintos (`-O2`, sin sanitizers). Pero en
desarrollo, los sanitizers son tu primera linea de defensa.

### Limpieza de la parte 3

```bash
rm -f ub_fixed ub_fixed_check ub_fixed_dev ub_showcase_check
```

---

## Limpieza final

```bash
rm -f ub_showcase* ub_opt_* ub_fixed* *.s
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  ub_fixed.c  ub_optimizer.c  ub_showcase.c
```

---

## Conceptos reforzados

1. Compilar sin `-Wall -Wextra` no reporta la mayoria de UB. Los warnings
   detectan algunos casos (variable sin inicializar, shift invalido) pero
   no todos (signed overflow, acceso out-of-bounds).

2. `-fsanitize=undefined` detecta UB en runtime: reporta el archivo, la linea,
   y los valores concretos involucrados. Requiere que el codigo se ejecute --
   no analiza caminos que no se recorren.

3. El mismo codigo fuente produce resultados distintos con `-O0` y `-O2` cuando
   contiene UB. Esto no es un bug del compilador: el estandar de C permite
   cualquier resultado, y el optimizador elige el mas eficiente.

4. El compilador usa UB como **informacion para optimizar**: si `x + 1 > x`
   pudiera ser false (por overflow), el compilador no podria simplificar
   la expresion. Como el overflow es UB, asume que no ocurre y reduce
   `x + 1 > x` a `true`.

5. Los patrones de correccion son concretos: `unsigned` para wrapping,
   bounds-check antes de acceder, inicializar variables, validar shift amount.
   Cada uno tiene un costo minimo en runtime pero elimina una categoria
   completa de bugs.

6. El set de compilacion para desarrollo es
   `gcc -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g`.
   Usar este comando sistematicamente atrapa la mayoria de errores de memoria
   y UB antes de que lleguen a produccion.
