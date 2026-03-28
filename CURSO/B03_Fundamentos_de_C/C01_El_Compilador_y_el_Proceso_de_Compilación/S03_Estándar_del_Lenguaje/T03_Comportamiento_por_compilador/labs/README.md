# Lab -- Comportamiento por compilador

## Objetivo

Comprobar empiricamente como el estandar seleccionado con `-std` y el flag
`-Wpedantic` afectan lo que GCC acepta, advierte o rechaza. Al finalizar,
sabras por que el default de GCC permite codigo no portable sin avisarte, que
cambia entre C89, C99 y C11, y por que `-std=c11 -Wpedantic` es la base del
curso.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `extensions.c` | Programa con 5 extensiones GNU: statement expression, nested function, binary literal, zero-length array, case range |
| `c99_features.c` | Programa con 6 features de C99: // comments, mixed declarations, VLA, for-loop declarations, designated initializers, stdint.h |
| `c11_features.c` | Programa con 5 features de C11: _Static_assert, _Noreturn, _Generic, anonymous struct/union, _Alignof |

---

## Parte 1 -- Extensiones GNU vs C estandar

**Objetivo**: Demostrar que GCC acepta extensiones no portables por defecto
y que `-Wpedantic` es necesario para detectarlas.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat extensions.c
```

Lee el codigo y localiza las 5 extensiones GNU. Cada una tiene un comentario
que la identifica. Antes de continuar, predice:

- Si compilas con `gcc extensions.c`, sin flags adicionales, dara algun
  warning o error?
- Cuales de las 5 features crees que NO son parte del estandar C11?

### Paso 1.2 -- Compilar con gcc por defecto

```bash
gcc extensions.c -o extensions_default 2>&1
```

Salida esperada: ninguna. Cero warnings, cero errores. El programa compila
limpio.

```bash
./extensions_default
```

Salida esperada:

```
hello from nested function
lowercase letter
max  = 5
mask = 0xF0
sizeof(struct Packet) = 4
```

El default de GCC es `-std=gnu17`, que es C17 mas todas las extensiones GNU.
No avisa sobre ninguna extension.

### Paso 1.3 -- Compilar con -std=c11

```bash
gcc -std=c11 extensions.c -o extensions_c11 2>&1
```

Predice: al cambiar a `-std=c11` (C11 estricto, sin extensiones GNU),
aparecen warnings?

### Paso 1.4 -- Verificar la prediccion

GCC con `-std=c11` pero **sin** `-Wpedantic` sigue aceptando las extensiones
silenciosamente. El flag `-std=c11` solo cambia que macros se definen y que
headers exponen, pero no avisa sobre extensiones por si solo.

```bash
./extensions_c11
```

Mismo resultado que antes. Para que GCC avise, falta `-Wpedantic`.

### Paso 1.5 -- Compilar con -std=c11 -Wpedantic

```bash
gcc -std=c11 -Wpedantic extensions.c -o extensions_pedantic 2>&1
```

Salida esperada (5 warnings):

```
extensions.c: In function 'main':
extensions.c:5:15: warning: ISO C forbids braced-groups within expressions [-Wpedantic]
extensions.c:8:5: warning: ISO C forbids nested functions [-Wpedantic]
extensions.c:14:16: warning: binary constants are a C23 feature or GCC extension [-Wpedantic]
extensions.c:19:14: warning: ISO C forbids zero-size array 'data' [-Wpedantic]
extensions.c:25:9: warning: ISO C does not support range expressions in switch statements before C2Y [-Wpedantic]
```

Cuenta los warnings: exactamente 5, uno por cada extension. Solo con
`-Wpedantic` GCC avisa sobre codigo que no es C estandar.

### Paso 1.6 -- Verificar que el programa sigue funcionando

```bash
./extensions_pedantic
```

Los warnings no impiden la compilacion. El programa funciona igual. Pero ahora
sabes que este codigo no es portable -- depende de extensiones de GCC.

### Paso 1.7 -- Leccion clave

Sin `-Wpedantic`, GCC acepta codigo no portable sin avisarte. El default
(`-std=gnu17`) esta disenado para maxima compatibilidad con codigo existente,
no para portabilidad. Si quieres saber que tu codigo compila en cualquier
compilador que soporte C11, necesitas `-std=c11 -Wpedantic`.

### Limpieza de la parte 1

```bash
rm -f extensions_default extensions_c11 extensions_pedantic
```

---

## Parte 2 -- Diferencias entre estandares (C89 vs C99 vs C11)

**Objetivo**: Ver que features se agregaron en C99 y C11, y como GCC las
rechaza o advierte cuando se compila con un estandar anterior.

### Paso 2.1 -- C99 features con -std=c89

```bash
cat c99_features.c
```

Este archivo usa 6 features que se introdujeron en C99. Predice: cuantos
errores o warnings producira compilar con `-std=c89 -Wpedantic`?

### Paso 2.2 -- Compilar con -std=c89 -Wpedantic

```bash
gcc -std=c89 -Wpedantic c99_features.c -o c99_as_c89 2>&1
```

Salida esperada (errores y warnings):

```
c99_features.c:5:5: error: C++ style comments are not allowed in ISO C90
c99_features.c:9:5: warning: ISO C90 forbids mixed declarations and code
c99_features.c:13:5: warning: ISO C90 forbids variable length array 'vla'
c99_features.c:14:5: error: 'for' loop initial declarations are only allowed in C99 or C11 mode
c99_features.c:19:20: warning: ISO C90 forbids specifying subobject to initialize
c99_features.c:27:5: error: 'for' loop initial declarations are only allowed in C99 or C11 mode
```

La compilacion falla. Los `//` comments y las declaraciones en `for` son
errores, no warnings -- C89 no los acepta de ninguna forma.

### Paso 2.3 -- Compilar con -std=c99

```bash
gcc -std=c99 c99_features.c -o c99_ok 2>&1
./c99_ok
```

Salida esperada:

```
=== C99 features ===
x     = 10
vla   = {0, 1, 4, 9, 16}
arr   = {0, 0, 42, 0, 99}
fixed = 12345
byte  = 255
__STDC_VERSION__ = 199901L
```

Compila limpio. Todas las features son parte de C99. Observa el valor de
`__STDC_VERSION__`: `199901L` indica C99.

### Paso 2.4 -- C11 features con -std=c99 -Wpedantic

```bash
cat c11_features.c
```

Este archivo usa 5 features de C11. Predice: compilara con `-std=c99`?
Y con `-std=c99 -Wpedantic`?

### Paso 2.5 -- Compilar con -std=c99 -Wpedantic

```bash
gcc -std=c99 -Wpedantic c11_features.c -o c11_as_c99 2>&1
```

Salida esperada (warnings, no errores):

```
c11_features.c:10:15: warning: ISO C99 does not support '_Noreturn'
c11_features.c:28:38: warning: ISO C99 doesn't support unnamed structs/unions
c11_features.c:30:10: warning: ISO C99 doesn't support unnamed structs/unions
c11_features.c:16:22: warning: ISO C99 does not support '_Generic'
    (aparece una vez por cada uso de type_name)
c11_features.c:46:40: warning: ISO C99 does not support '_Alignof'
    (aparece una vez por cada uso de _Alignof)
```

GCC 15 acepta las features de C11 como extensiones incluso en modo C99, pero
`-Wpedantic` advierte que no son parte de C99. Observa: sin `-Wpedantic`, la
compilacion es silenciosa incluso con `-std=c99`.

### Paso 2.6 -- Compilar con -std=c11

```bash
gcc -std=c11 c11_features.c -o c11_ok 2>&1
./c11_ok
```

Salida esperada:

```
=== C11 features ===
type of i: int
type of f: float
type of d: double
_Alignof(int)    = 4
_Alignof(double) = 8
_Alignof(char)   = 1
v.x = 1.0, v.y = 2.0, v.z = 3.0
v.components[0] = 1.0
__STDC_VERSION__ = 201112L
```

Compila limpio. Todas las features son parte de C11. `__STDC_VERSION__` ahora
es `201112L`.

### Paso 2.7 -- Comparar __STDC_VERSION__ entre estandares

Observa los valores que imprimieron los programas:

| Flag | __STDC_VERSION__ | Estandar |
|------|------------------|----------|
| `-std=c89` | No definido | C89/C90 (ANSI C) |
| `-std=c99` | `199901L` | C99 |
| `-std=c11` | `201112L` | C11 |
| (default) | `202311L` | gnu17/C23 (GCC 15) |

El valor de `__STDC_VERSION__` permite que el codigo detecte en tiempo de
compilacion con que estandar se esta compilando.

### Limpieza de la parte 2

```bash
rm -f c99_as_c89 c99_ok c11_as_c99 c11_ok
```

---

## Parte 3 -- El estandar del curso: -std=c11

**Objetivo**: Verificar que `-std=c11 -Wpedantic` es el punto justo para el
curso: acepta todo C99 y C11, pero advierte sobre extensiones GNU.

### Paso 3.1 -- Compilar los tres archivos con -std=c11 -Wpedantic

Predice antes de compilar: de los tres archivos, cuales compilaran sin
warnings y cual tendra warnings?

```bash
echo "=== extensions.c ==="
gcc -std=c11 -Wpedantic extensions.c -o ext_test 2>&1

echo ""
echo "=== c99_features.c ==="
gcc -std=c11 -Wpedantic c99_features.c -o c99_test 2>&1

echo ""
echo "=== c11_features.c ==="
gcc -std=c11 -Wpedantic c11_features.c -o c11_test 2>&1
```

Resultado:

- `extensions.c` -- 5 warnings (extensiones GNU no son C11)
- `c99_features.c` -- 0 warnings (C99 es un subconjunto de C11)
- `c11_features.c` -- 0 warnings (C11 features son C11)

Solo el codigo que usa extensiones fuera del estandar produce warnings.
Codigo C99 y C11 puro compila limpio.

### Paso 3.2 -- Los flags recomendados del curso

```bash
echo "=== c99_features.c con flags del curso ==="
gcc -Wall -Wextra -Wpedantic -std=c11 -g c99_features.c -o c99_curso 2>&1

echo ""
echo "=== c11_features.c con flags del curso ==="
gcc -Wall -Wextra -Wpedantic -std=c11 -g c11_features.c -o c11_curso 2>&1
```

Salida esperada: cero warnings en ambos. El set completo del curso es:

```
-Wall -Wextra -Wpedantic -std=c11 -g
```

Que hace cada flag:

| Flag | Funcion |
|------|---------|
| `-Wall` | Warnings principales (variables sin usar, sin inicializar, formato incorrecto) |
| `-Wextra` | Warnings adicionales (parametros no usados, missing initializers) |
| `-Wpedantic` | Advierte sobre extensiones GNU que no son C estandar |
| `-std=c11` | Compila en modo C11 estricto |
| `-g` | Incluye informacion de debug para GDB |

### Paso 3.3 -- Verificar que las extensiones se detectan

```bash
echo "=== extensions.c con flags del curso ==="
gcc -Wall -Wextra -Wpedantic -std=c11 -g extensions.c -o ext_curso 2>&1
```

Salida esperada: 5 warnings. Este set de flags detecta codigo no portable
tempranamente. Si un programa compila limpio con estos flags, compila en
cualquier compilador que soporte C11.

### Paso 3.4 -- Ejecutar los programas

```bash
./c99_curso
echo ""
./c11_curso
```

Los programas funcionan correctamente. Los flags del curso no limitan
funcionalidad -- solo exponen dependencias no portables.

### Limpieza de la parte 3

```bash
rm -f ext_test c99_test c11_test c99_curso c11_curso ext_curso
```

---

## Limpieza final

```bash
rm -f extensions_default extensions_c11 extensions_pedantic
rm -f c99_as_c89 c99_ok c11_as_c99 c11_ok
rm -f ext_test c99_test c11_test c99_curso c11_curso ext_curso
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  c11_features.c  c99_features.c  extensions.c
```

---

## Conceptos reforzados

1. El default de GCC (`-std=gnu17`) acepta extensiones GNU sin avisar. Un
   programa que compila limpio con `gcc main.c` puede contener codigo no
   portable que no compila en otros compiladores.

2. `-Wpedantic` es el unico flag que advierte sobre extensiones GNU. Sin el,
   ni `-Wall` ni `-Wextra` detectan uso de features no estandar.

3. C99 agrego sobre C89: `//` comments, mixed declarations and code, VLA,
   declaraciones en `for`, designated initializers, `stdint.h`.
   Compilar con `-std=c89 -Wpedantic` rechaza todas estas features.

4. C11 agrego sobre C99: `_Static_assert`, `_Generic`, anonymous
   struct/union, `_Alignof`, `_Noreturn`. GCC 15 las acepta como extensiones
   en C99, pero `-Wpedantic` advierte que no son C99 estandar.

5. `__STDC_VERSION__` indica el estandar activo: no definido (C89),
   `199901L` (C99), `201112L` (C11). Es util para codigo que necesita
   adaptarse al estandar de compilacion.

6. Con `-std=c11 -Wpedantic`, codigo C99 y C11 puro compila sin warnings.
   Solo las extensiones GNU producen warnings. Este es el criterio del curso
   para codigo portable.
