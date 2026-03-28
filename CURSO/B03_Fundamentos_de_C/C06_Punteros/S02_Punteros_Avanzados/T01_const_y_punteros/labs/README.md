# Lab — const y punteros

## Objetivo

Practicar las 4 combinaciones de `const` con punteros, usar la regla de
lectura derecha a izquierda para interpretar declaraciones, y aplicar const
correctness en APIs de funciones. Al finalizar, podras predecir que lineas
compilan y cuales dan error, leer cualquier declaracion `const`/puntero, y
disenar firmas de funciones con `const` correcto.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `four_combos.c` | Las 4 combinaciones de const con punteros |
| `const_errors.c` | Errores de compilacion intencionales para analizar |
| `read_right_to_left.c` | Regla de lectura derecha a izquierda |
| `const_api.c` | const correctness en funciones y APIs |
| `cast_away.c` | Cast away const y comportamiento indefinido |

---

## Parte 1 — Las 4 combinaciones

**Objetivo**: Entender que permite y que prohibe cada combinacion de `const`
con punteros, y verificarlo con el compilador.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat four_combos.c
```

Observa las 4 secciones del programa. Cada una usa una combinacion diferente:

1. `int *p1` -- puntero mutable a dato mutable
2. `const int *p2` -- puntero mutable a dato protegido
3. `int *const p3` -- puntero fijo a dato mutable
4. `const int *const p4` -- puntero fijo a dato protegido

Antes de compilar, predice para cada combinacion:

- Puede hacer `*p = valor`? (modificar el dato)
- Puede hacer `p = &otra_var`? (redirigir el puntero)

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic four_combos.c -o four_combos
./four_combos
```

Salida esperada:

```
combo 1 (int *):            x=100 y=200
combo 2 (const int *):      *p2=10 -> after redirect: *p2=20
combo 3 (int *const):       x=100 (via *p3)
combo 4 (const int *const): *p4=20 (read only)
```

Verifica tu prediccion:

| Combinacion | `*p = valor` | `p = &otra` |
|-------------|--------------|-------------|
| `int *p` | Si | Si |
| `const int *p` | No | Si |
| `int *const p` | Si | No |
| `const int *const p` | No | No |

### Paso 1.3 — Provocar los errores

Ahora examina `const_errors.c`:

```bash
cat const_errors.c
```

Este archivo tiene 4 errores intencionales y 1 warning. Antes de compilar,
identifica mentalmente cuales lineas fallaran y por que.

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.4 — Verificar con el compilador

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic const_errors.c -o const_errors 2>&1
```

Salida esperada (4 errores, 1 warning):

```
const_errors.c:15:9: error: assignment of read-only location '*p2'
const_errors.c:17:9: error: assignment of read-only location '*(const int *)p4'
const_errors.c:22:8: error: assignment of read-only variable 'p3'
const_errors.c:23:8: error: assignment of read-only variable 'p4'
const_errors.c:26:15: warning: initialization discards 'const' qualifier ...
```

Analisis de cada error:

- **Linea 15** (`*p2 = 100`): `p2` es `const int *` -- el dato es read-only
- **Linea 17** (`*p4 = 100`): `p4` es `const int *const` -- el dato es read-only
- **Linea 22** (`p3 = &y`): `p3` es `int *const` -- el puntero es read-only
- **Linea 23** (`p4 = &y`): `p4` es `const int *const` -- el puntero es read-only
- **Linea 26** (`int *p5 = &cx`): `cx` es `const int`, pero `p5` es `int *` --
  se pierde la proteccion `const` (warning, no error)

Observa el patron: el compilador distingue entre "read-only location" (el dato)
y "read-only variable" (el puntero). Esto corresponde a donde esta el `const`
en la declaracion.

### Paso 1.5 — Conversion implicita int* a const int*

Observa la linea 26 de `const_errors.c` con atencion. El compilador da
**warning** (no error) porque se esta quitando `const`. En cambio, la
conversion inversa es segura:

```bash
cat > implicit_conv.c << 'EOF'
#include <stdio.h>

int main(void) {
    int x = 42;
    int *p = &x;

    /* Adding const is safe — implicit conversion */
    const int *cp = p;
    printf("*cp = %d\n", *cp);

    /* Removing const requires explicit cast */
    /* int *q = cp; */  /* WARNING: discards const */

    return 0;
}
EOF
gcc -std=c11 -Wall -Wextra -Wpedantic implicit_conv.c -o implicit_conv
./implicit_conv
```

Salida esperada:

```
*cp = 42
```

Agregar `const` es seguro (se agregan restricciones). Quitar `const` es
peligroso (se eliminan restricciones). Por eso el compilador permite lo primero
sin quejarse.

### Limpieza de la parte 1

```bash
rm -f four_combos const_errors implicit_conv implicit_conv.c
```

---

## Parte 2 — Regla de lectura derecha a izquierda

**Objetivo**: Aplicar la regla de lectura derecha a izquierda para interpretar
declaraciones de punteros con `const` y verificar la comprension con codigo
real.

### Paso 2.1 — Examinar el codigo

```bash
cat read_right_to_left.c
```

Cada declaracion tiene un comentario que muestra como leerla de derecha a
izquierda. Observa el patron:

```
const int *p    -->  p is a pointer (*) to int that is const
                     ←─────────────────────────────────────
```

```
int *const p    -->  p is a const pointer (*) to int
                     ←──────────────────────────────
```

La clave: lo que esta **justo a la izquierda** del `*` es lo protegido.

### Paso 2.2 — Compilar y verificar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic read_right_to_left.c -o read_right_to_left
./read_right_to_left
```

Salida esperada:

```
All four read x=42 correctly:
  const int *p1       = 42
  int const *p2       = 42
  int *const p3       = 42
  const int *const p4 = 42
```

Nota que `const int *p1` y `int const *p2` son equivalentes. Ambas protegen
el dato, no el puntero. La posicion de `const` respecto al tipo (`int`) no
importa -- lo que importa es su posicion respecto al `*`.

### Paso 2.3 — Ejercicio de lectura

Lee las siguientes declaraciones de derecha a izquierda y predice que
protege cada una. No compiles todavia:

```c
const char *msg;
char *const buffer;
const char *const label;
const int **pp;
int *const *pp2;
```

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4 — Verificar con el compilador

```bash
cat > read_exercise.c << 'EOF'
#include <stdio.h>

int main(void) {
    char hello[] = "Hello";
    char world[] = "World";

    /* msg: pointer to const char — can redirect, cannot modify chars */
    const char *msg = hello;
    /* msg[0] = 'h'; */  /* ERROR: read-only */
    msg = world;          /* OK */
    printf("msg = %s\n", msg);

    /* buffer: const pointer to char — cannot redirect, can modify chars */
    char *const buffer = hello;
    buffer[0] = 'h';     /* OK */
    /* buffer = world; */  /* ERROR: read-only variable */
    printf("buffer = %s\n", buffer);

    /* label: const pointer to const char — cannot do anything */
    const char *const label = "Linux";
    /* label[0] = 'l'; */  /* ERROR */
    /* label = world; */    /* ERROR */
    printf("label = %s\n", label);

    return 0;
}
EOF
gcc -std=c11 -Wall -Wextra -Wpedantic read_exercise.c -o read_exercise
./read_exercise
```

Salida esperada:

```
msg = World
buffer = hello
label = Linux
```

Observa:

- `msg` pudo redirigirse a `world` pero no modificar los caracteres
- `buffer` pudo cambiar `'H'` a `'h'` pero no redirigirse
- `label` no puede hacer ninguna de las dos cosas

### Limpieza de la parte 2

```bash
rm -f read_right_to_left read_exercise read_exercise.c
```

---

## Parte 3 — const correctness en APIs

**Objetivo**: Disenar APIs de funciones con `const` correctamente aplicado,
entender la conversion implicita de `int*` a `const int*`, y observar las
consecuencias de quitar `const` con cast.

### Paso 3.1 — Examinar el API

```bash
cat const_api.c
```

Observa las 4 funciones y como usan `const`:

| Funcion | Parametros | Por que |
|---------|-----------|---------|
| `sum()` | `const int *arr` | Solo lee el array, no lo modifica |
| `fill()` | `int *arr` | Modifica el array, no puede ser const |
| `find_char()` | `const char *str` | Solo lee el string |
| `get_error_message()` | retorna `const char *` | Retorna puntero a string literal |

Antes de compilar, predice: la llamada `sum(data, n)` donde `data` es `int[]`
(no `const int[]`), dara error o compilara?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic const_api.c -o const_api
./const_api
```

Salida esperada:

```
sum = 150
after fill: data[0]=99 data[4]=99
found 'W' at position 7: "World!"
code 0: Success
code 1: File not found
code 2: Permission denied
code 3: Out of memory
code 4: Unknown error
```

La llamada `sum(data, n)` compila sin problemas porque `int *` se convierte
implicitamente a `const int *`. Agregar restricciones es seguro.

### Paso 3.3 — Observar la tabla de mensajes

Mira la declaracion de `messages` dentro de `get_error_message()`:

```c
static const char *const messages[] = { ... };
```

Lee de derecha a izquierda:

- `messages` es un array de...
- `const` punteros a...
- `const char`

Cada puntero del array es fijo (no se puede redirigir) y apunta a datos que
no se pueden modificar. Doble proteccion.

### Paso 3.4 — Cast away const

```bash
cat cast_away.c
```

Este programa tiene dos casos:

1. **Caso 1**: `const int x = 42` -- el objeto original ES const. Quitar
   `const` con cast y modificar es **comportamiento indefinido**.
2. **Caso 2**: `int y = 42` visto a traves de `const int *` -- el objeto
   original NO es const. Quitar `const` con cast y modificar es valido.

Antes de compilar, predice: en el Caso 1, despues de `*bad_ptr = 100`, que
imprimira `x`? Y `*bad_ptr`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.5 — Compilar sin optimizacion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic cast_away.c -o cast_away
./cast_away
```

Salida esperada:

```
before: x = 42, *bad_ptr = 42
after:  x = 100, *bad_ptr = 100

y = 200 (modified through cast, original was not const)
```

Sin optimizacion, `x` parece cambiar a 100. Pero esto es **UB** -- el resultado
no es confiable. Ahora compila con optimizacion para ver la diferencia.

### Paso 3.6 — Compilar con optimizacion

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 cast_away.c -o cast_away_opt
./cast_away_opt
```

Salida esperada:

```
before: x = 42, *bad_ptr = 42
after:  x = 42, *bad_ptr = 100

y = 200 (modified through cast, original was not const)
```

Ahora `x` sigue mostrando 42 aunque `*bad_ptr` es 100. El compilador, al ver
que `x` fue declarado `const`, reemplazo `x` por 42 en tiempo de compilacion
(constant folding). La escritura a traves de `bad_ptr` modifico la memoria,
pero `printf("x = %d", x)` ya fue optimizado a `printf("x = %d", 42)`.

Esta es la esencia del UB: el resultado cambia segun el nivel de optimizacion.
El Caso 2 (`y`) funciona correctamente en ambos casos porque `y` nunca fue
declarado `const`.

### Paso 3.7 — Ejercicio: disenar firmas

Escribe en un archivo las firmas correctas (con `const` donde corresponda)
para estas funciones:

```bash
cat > signatures.c << 'EOF'
#include <stdio.h>
#include <string.h>

/* 1. Count: counts how many times 'target' appears in 'arr' of size 'n'.
 *    Does not modify arr. */
int count(/* complete the parameters */) {
    int c = 0;
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) c++;
    }
    return c;
}

/* 2. Swap: swaps the values of two integers.
 *    Modifies both. */
void swap(/* complete the parameters */) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

/* 3. Greeting: returns a string literal based on hour.
 *    Does not modify anything. */
/* complete the return type */ greeting(int hour) {
    if (hour < 12) return "Good morning";
    if (hour < 18) return "Good afternoon";
    return "Good evening";
}

int main(void) {
    int data[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3};
    printf("count of 1: %d\n", count(data, 10, 1));

    int a = 10, b = 20;
    swap(&a, &b);
    printf("after swap: a=%d b=%d\n", a, b);

    printf("%s\n", greeting(9));
    printf("%s\n", greeting(15));

    return 0;
}
EOF
```

Completa las firmas y verifica que compila sin warnings:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic signatures.c -o signatures
./signatures
```

Salida esperada (despues de completar correctamente):

```
count of 1: 2
after swap: a=20 b=10
Good morning
Good afternoon
```

Las firmas correctas son:

- `count(const int *arr, int n, int target)` -- solo lee `arr`
- `swap(int *a, int *b)` -- modifica ambos, no puede ser `const`
- `const char *greeting(int hour)` -- retorna puntero a string literal

### Limpieza de la parte 3

```bash
rm -f const_api cast_away cast_away_opt signatures signatures.c
```

---

## Limpieza final

```bash
rm -f *.o signatures.c implicit_conv.c read_exercise.c
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  cast_away.c  const_api.c  const_errors.c  four_combos.c  read_right_to_left.c
```

---

## Conceptos reforzados

1. `const int *p` protege el **dato** apuntado -- no se puede hacer `*p = x`.
   El puntero se puede redirigir con `p = &y`.

2. `int *const p` protege el **puntero** -- no se puede hacer `p = &y`. El
   dato se puede modificar con `*p = x`.

3. `const int *const p` protege **ambos** -- ni el dato ni el puntero se
   pueden modificar. Solo se puede leer.

4. La regla de lectura derecha a izquierda permite interpretar cualquier
   declaracion: lo que esta justo a la izquierda de `*` determina que se
   protege.

5. `const int *p` e `int const *p` son equivalentes -- la posicion de `const`
   respecto al tipo no importa, solo respecto al `*`.

6. La conversion de `int *` a `const int *` es implicita y segura (agregar
   restricciones). La conversion inversa requiere cast explicito y puede
   causar UB.

7. Quitar `const` con cast y modificar el objeto es UB si el objeto fue
   declarado como `const`. Es valido solo si el objeto original no era `const`.

8. El UB por cast away const se manifiesta de formas diferentes segun el nivel
   de optimizacion: sin `-O2` puede parecer que funciona, con `-O2` el
   compilador reemplaza la variable const por su valor en tiempo de compilacion.

9. En APIs de funciones, `const` en parametros puntero documenta la intencion
   (solo lectura) y el compilador lo verifica. Las funciones de la biblioteca
   estandar siguen este patron: `strlen(const char *s)`,
   `strcpy(char *dest, const char *src)`.
