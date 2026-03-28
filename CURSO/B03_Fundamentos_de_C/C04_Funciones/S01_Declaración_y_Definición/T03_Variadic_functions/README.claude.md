# T03 — Variadic functions

> **Fuentes**: `README.md`, `LABS.md`, `labs/README.md`, 3 archivos `.c` de laboratorio.
> Aplicada **Regla 3** (no existe `.max.md`).

---

Sin erratas detectadas en el material fuente.

---

## 1. Qué son las funciones variádicas

Una función variádica acepta un número **variable** de argumentos. El ejemplo
más conocido es `printf`:

```c
printf("hello\n");                         // 1 argumento
printf("x = %d\n", x);                    // 2 argumentos
printf("%d + %d = %d\n", a, b, a + b);    // 4 argumentos
```

En la declaración, `...` indica argumentos variables:

```c
int printf(const char *fmt, ...);    // al menos 1 fijo (fmt), luego 0+ extras
```

Restricción: siempre debe haber **al menos un parámetro fijo** antes de `...`:

```c
int bad(...);       // ERROR: necesita al menos un parámetro fijo
int ok(int n, ...); // OK
```

---

## 2. `<stdarg.h>` — la maquinaria

Para acceder a los argumentos variables se usan cuatro macros de `<stdarg.h>`:

| Macro | Propósito |
|-------|-----------|
| `va_list` | Tipo que almacena el estado de iteración sobre los argumentos |
| `va_start(args, last_fixed)` | Inicializa `args` a partir del último parámetro fijo |
| `va_arg(args, type)` | Obtiene el siguiente argumento interpretándolo como `type` |
| `va_end(args)` | Limpia `args`. **Siempre** llamar antes de retornar |
| `va_copy(dst, src)` | Copia el estado de un `va_list` a otro (C99) |

### Ejemplo básico

```c
#include <stdio.h>
#include <stdarg.h>

int sum(int count, ...) {
    va_list args;
    va_start(args, count);       // inicializar después del último parámetro fijo

    int total = 0;
    for (int i = 0; i < count; i++) {
        total += va_arg(args, int);   // leer el siguiente int
    }

    va_end(args);                // limpiar
    return total;
}

int main(void) {
    printf("%d\n", sum(3, 10, 20, 30));      // 60
    printf("%d\n", sum(5, 1, 2, 3, 4, 5));   // 15
    printf("%d\n", sum(0));                    // 0
    return 0;
}
```

### Traza de `sum(3, 10, 20, 30)`

```
1. count = 3, los ... son: 10, 20, 30
2. va_start(args, count)     — args apunta al primer variádico (10)
3. va_arg(args, int) → 10    — total = 10, args avanza
4. va_arg(args, int) → 20    — total = 30, args avanza
5. va_arg(args, int) → 30    — total = 60, args avanza
6. i=3, i < count es falso   — sale del loop
7. va_end(args)              — limpieza
8. return 60
```

`va_arg` no verifica nada: lee los bytes del stack ciegamente y los interpreta
como el tipo indicado. Llamar a `va_arg` más veces que argumentos pasados es **UB**.

---

## 3. Cómo saber cuántos argumentos hay

La función variádica **no sabe** cuántos argumentos recibió. Hay tres patrones:

### Patrón 1: conteo explícito

El primer parámetro indica cuántos argumentos variables hay:

```c
int sum(int count, ...) {
    va_list args;
    va_start(args, count);
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += va_arg(args, int);
    }
    va_end(args);
    return total;
}

sum(3, 10, 20, 30);    // count = 3
```

Simple y explícito, pero el caller puede equivocarse (pasar `count = 4` con solo
3 argumentos → UB).

### Patrón 2: sentinel (valor que marca el final)

El último argumento es un valor especial que la función reconoce como "fin":

```c
// Para enteros: sentinel = -1
int sum_until(int first, ...) {
    if (first == -1) return 0;

    va_list args;
    va_start(args, first);

    int total = first;
    int next;
    while ((next = va_arg(args, int)) != -1) {
        total += next;
    }

    va_end(args);
    return total;
}

sum_until(10, 20, 30, -1);    // -1 marca el final, no se suma

// Para punteros: sentinel = NULL (natural y limpio)
void print_strings(const char *first, ...) {
    va_list args;
    va_start(args, first);

    const char *s = first;
    while (s != NULL) {
        printf("%s ", s);
        s = va_arg(args, const char *);
    }
    printf("\n");

    va_end(args);
}

print_strings("hello", "world", "foo", NULL);
```

Peligro: si olvidas el sentinel, `va_arg` sigue leyendo del stack → UB. El
compilador **no puede** detectar que falta el sentinel.

### Patrón 3: format string (como printf)

El format string describe los tipos y la cantidad de argumentos:

```c
void my_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            putchar(*p);
            continue;
        }
        p++;
        switch (*p) {
            case 'd': printf("%d", va_arg(args, int)); break;
            case 's': printf("%s", va_arg(args, const char *)); break;
            case 'c': putchar(va_arg(args, int)); break;   // char → int
            case '%': putchar('%'); break;
        }
    }

    va_end(args);
}

my_printf("Name: %s, Age: %d\n", "Alice", 25);
// Name: Alice, Age: 25
```

El format string es la fuente de verdad: le dice a la función **cuántos** argumentos
leer y de **qué tipo** es cada uno. Es el patrón más poderoso pero también el más
propenso a errores si el format string no coincide con los argumentos.

---

## 4. Promoción de argumentos variádicos

Cuando los argumentos se pasan a `...`, sufren **default argument promotions**:

| Tipo pasado | Se promueve a | En `va_arg` usar... |
|-------------|---------------|---------------------|
| `char` | `int` | `int` |
| `short` | `int` | `int` |
| `float` | `double` | `double` |
| `int`, `long`, `double`, punteros | Sin cambio | El tipo original |

```c
void example(int count, ...) {
    va_list args;
    va_start(args, count);

    // Si le pasaron un char 'A':
    int c = va_arg(args, int);       // CORRECTO — 'A' se promovió a int
    // char c = va_arg(args, char);  // INCORRECTO — UB

    // Si le pasaron un float 3.14f:
    double d = va_arg(args, double);   // CORRECTO — 3.14f se promovió a double
    // float f = va_arg(args, float);  // INCORRECTO — UB

    va_end(args);
}
```

Esto es por qué `%c` en printf/my_printf usa `va_arg(args, int)` y no
`va_arg(args, char)`. El `char` ya no existe en el stack — fue promovido a `int`.

---

## 5. Pasar `va_list` a otra función

Las funciones `v*` de la biblioteca estándar aceptan un `va_list` directamente:

```c
int vprintf(const char *fmt, va_list args);
int vfprintf(FILE *stream, const char *fmt, va_list args);
int vsnprintf(char *str, size_t size, const char *fmt, va_list args);
```

Esto permite crear wrappers de logging, error reporting, etc.:

```c
#include <stdio.h>
#include <stdarg.h>

void log_message(const char *level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    printf("[%s] ", level);
    vprintf(fmt, args);       // delega el formateo a vprintf
    printf("\n");

    va_end(args);
}

log_message("INFO", "Server started on port %d", 8080);
// [INFO] Server started on port 8080
```

Sin las funciones `v*`, tendrías que reimplementar el parsing del format string.
`vprintf` ya sabe cómo hacerlo.

---

## 6. `va_copy` (C99)

`va_copy` duplica el estado de un `va_list`, permitiendo recorrer los argumentos
**dos veces**:

```c
void process(int count, ...) {
    va_list args, args_copy;
    va_start(args, count);
    va_copy(args_copy, args);    // copia el estado actual

    // Primera pasada: sumar
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += va_arg(args, int);
    }

    // Segunda pasada: imprimir (usando la copia)
    for (int i = 0; i < count; i++) {
        printf("%d ", va_arg(args_copy, int));
    }

    va_end(args_copy);    // limpiar AMBOS
    va_end(args);
}
```

`va_copy` es necesario porque `va_arg` modifica el `va_list` al avanzar.
Sin la copia, no podrías volver al inicio. Siempre llamar `va_end` en ambos.

---

## 7. `__attribute__((format))` — verificación de GCC

Por defecto, GCC no puede verificar los argumentos de funciones variádicas propias.
Pero con `__attribute__((format))`, le dices que el format string sigue las
convenciones de printf:

```c
void my_log(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
// Parámetros del atributo:
//   printf  — estilo de formato
//   1       — el formato está en el parámetro 1 (fmt)
//   2       — los argumentos variádicos empiezan en el parámetro 2
```

Ahora GCC verifica los argumentos:
```c
my_log("x = %d\n", "hello");
// warning: format '%d' expects argument of type 'int',
//          but argument 2 has type 'char *'
```

La declaración real de `printf` en glibc incluye este atributo, por eso
`-Wall -Wformat` detecta errores en llamadas a `printf`.

---

## 8. Peligros de las funciones variádicas

```c
// 1. No hay verificación de tipos (sin __attribute__):
sum(3, 10, 20, "hello");   // "hello" se interpreta como int → UB

// 2. Leer más argumentos de los pasados:
sum(5, 10, 20);            // pide 5 pero solo pasó 2 → UB en el 3ro

// 3. Tipo incorrecto en va_arg:
int x = va_arg(args, double);  // si el arg era int → UB
// va_arg lee bytes ciegamente del stack

// 4. Olvidar va_end:
// Puede causar resource leaks en algunos sistemas

// 5. Olvidar el sentinel:
sum_until(10, 20, 30);     // sin -1 al final → UB
```

El compilador no puede detectar ninguno de estos errores (excepto con
`__attribute__((format))` para el caso de format strings). Las funciones variádicas
son inherentemente inseguras — el programador es responsable de la corrección.

---

## Ejercicios

### Ejercicio 1 — sum variádica con conteo

```c
#include <stdio.h>
#include <stdarg.h>

int sum(int count, ...) {
    va_list args;
    va_start(args, count);
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += va_arg(args, int);
    }
    va_end(args);
    return total;
}

int main(void) {
    printf("sum(3, 10, 20, 30) = %d\n", sum(3, 10, 20, 30));
    printf("sum(5, 1, 2, 3, 4, 5) = %d\n", sum(5, 1, 2, 3, 4, 5));
    printf("sum(1, 42) = %d\n", sum(1, 42));
    printf("sum(0) = %d\n", sum(0));
    return 0;
}
```

<details><summary>Predicción</summary>

```
sum(3, 10, 20, 30) = 60
sum(5, 1, 2, 3, 4, 5) = 15
sum(1, 42) = 42
sum(0) = 0
```

- `sum(3, ...)`: lee 3 ints → 10 + 20 + 30 = 60.
- `sum(5, ...)`: lee 5 ints → 1 + 2 + 3 + 4 + 5 = 15.
- `sum(1, ...)`: lee 1 int → 42.
- `sum(0)`: el loop no ejecuta → total queda en 0. `va_arg` nunca se llama.

</details>

---

### Ejercicio 2 — Sentinel con enteros

```c
#include <stdio.h>
#include <stdarg.h>

int sum_until(int first, ...) {
    if (first == -1) return 0;

    va_list args;
    va_start(args, first);

    int total = first;
    int next;
    while ((next = va_arg(args, int)) != -1) {
        total += next;
    }

    va_end(args);
    return total;
}

int main(void) {
    printf("sum_until(10, 20, 30, -1) = %d\n", sum_until(10, 20, 30, -1));
    printf("sum_until(5, -1) = %d\n", sum_until(5, -1));
    printf("sum_until(-1) = %d\n", sum_until(-1));
    printf("sum_until(100, -1) = %d\n", sum_until(100, -1));
    return 0;
}
```

<details><summary>Predicción</summary>

```
sum_until(10, 20, 30, -1) = 60
sum_until(5, -1) = 5
sum_until(-1) = 0
sum_until(100, -1) = 100
```

- `sum_until(10, 20, 30, -1)`: first=10, lee 20 y 30, encuentra -1 → 10+20+30 = 60.
- `sum_until(5, -1)`: first=5, lee -1 inmediatamente → total = 5.
- `sum_until(-1)`: first == -1 → retorna 0 sin iniciar `va_start`.
- `sum_until(100, -1)`: first=100, lee -1 → total = 100.

El sentinel -1 **no se suma**. Cuando `first == -1`, la función retorna 0 directamente.
Limitación: no se puede sumar el valor -1 porque se confundiría con el sentinel.

</details>

---

### Ejercicio 3 — Sentinel con punteros (NULL)

```c
#include <stdio.h>
#include <stdarg.h>

void print_strings(const char *first, ...) {
    va_list args;
    va_start(args, first);

    const char *s = first;
    while (s != NULL) {
        printf("%s ", s);
        s = va_arg(args, const char *);
    }
    printf("\n");

    va_end(args);
}

int main(void) {
    print_strings("hello", "world", "foo", NULL);
    print_strings("one", NULL);
    print_strings("a", "b", "c", "d", "e", NULL);
    return 0;
}
```

<details><summary>Predicción</summary>

```
hello world foo
one
a b c d e
```

- Cada string se imprime seguido de un espacio.
- `NULL` termina el loop — no se imprime.
- `first` se procesa antes de `va_start`, como parte del while.

NULL es el sentinel natural para punteros: es un valor que nunca sería un string
válido. A diferencia de -1 para enteros, NULL no restringe los datos útiles.

</details>

---

### Ejercicio 4 — Promoción de argumentos

```c
#include <stdio.h>
#include <stdarg.h>

void show_types(int count, ...) {
    va_list args;
    va_start(args, count);

    for (int i = 0; i < count; i++) {
        // Todos se leen como int (por la promoción):
        int val = va_arg(args, int);
        printf("arg %d: %d\n", i, val);
    }

    va_end(args);
}

int main(void) {
    char c = 'A';           // 65
    short s = 1000;
    int n = 42;

    show_types(3, c, s, n);
    return 0;
}
```

<details><summary>Predicción</summary>

```
arg 0: 65
arg 1: 1000
arg 2: 42
```

- `char c = 'A'` se promueve a `int` (65) al pasarse a `...`.
- `short s = 1000` se promueve a `int` (1000) al pasarse a `...`.
- `int n = 42` no cambia (ya es `int`).

Los tres se leen con `va_arg(args, int)` porque tras la promoción, todos son `int`.
Usar `va_arg(args, char)` o `va_arg(args, short)` sería UB.

</details>

---

### Ejercicio 5 — Mini-printf

```c
#include <stdio.h>
#include <stdarg.h>

void my_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') { putchar(*p); continue; }
        p++;
        switch (*p) {
            case 'd': printf("%d", va_arg(args, int)); break;
            case 's': printf("%s", va_arg(args, const char *)); break;
            case 'c': putchar(va_arg(args, int)); break;
            case '%': putchar('%'); break;
            default:  putchar('%'); putchar(*p); break;
        }
    }

    va_end(args);
}

int main(void) {
    my_printf("Hello %s, age %d\n", "Alice", 25);
    my_printf("Grade: %c\n", 'A');
    my_printf("100%% done\n");
    my_printf("%d + %d = %d\n", 3, 4, 7);
    return 0;
}
```

<details><summary>Predicción</summary>

```
Hello Alice, age 25
Grade: A
100% done
3 + 4 = 7
```

- `%s` → `va_arg(args, const char *)` → "Alice".
- `%d` → `va_arg(args, int)` → 25.
- `%c` → `va_arg(args, int)` → 'A' (65, promovido a int). `putchar(65)` imprime 'A'.
- `%%` → imprime un `%` literal (no consume argumento).
- La última línea consume 3 argumentos int consecutivos.

`%c` usa `va_arg(args, int)` porque `char` se promueve a `int` en argumentos
variádicos. Si usáramos `va_arg(args, char)`, sería UB.

</details>

---

### Ejercicio 6 — log_message con vprintf

```c
#include <stdio.h>
#include <stdarg.h>

void log_msg(const char *level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

void log_msg(const char *level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[%s] ", level);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

int main(void) {
    log_msg("INFO",  "Server started on port %d", 8080);
    log_msg("WARN",  "Disk usage at %d%%", 85);
    log_msg("ERROR", "Failed to open '%s'", "/tmp/data.txt");
    return 0;
}
```

<details><summary>Predicción</summary>

```
[INFO] Server started on port 8080
[WARN] Disk usage at 85%
[ERROR] Failed to open '/tmp/data.txt'
```

- `vprintf(fmt, args)` formatea la string usando el `va_list` directamente.
  No necesitamos parsear el format string manualmente.
- El `__attribute__((format(printf, 2, 3)))` le dice a GCC:
  - Parámetro 2 (`fmt`) es el format string estilo printf.
  - Parámetro 3 es donde empiezan los argumentos variádicos.
- Con este atributo, `log_msg("INFO", "%d", "hello")` daría warning de compilación.
- Sin él, el error pasaría desapercibido.

Nota: `%d%%` imprime el número seguido de un `%` literal. El `%%` lo maneja `vprintf`.

</details>

---

### Ejercicio 7 — max variádico

```c
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>

int max_of(int count, ...) {
    va_list args;
    va_start(args, count);

    int max = INT_MIN;
    for (int i = 0; i < count; i++) {
        int val = va_arg(args, int);
        if (val > max) max = val;
    }

    va_end(args);
    return max;
}

int main(void) {
    printf("max_of(3, 10, 50, 30) = %d\n", max_of(3, 10, 50, 30));
    printf("max_of(5, -1, -5, -3, -2, -4) = %d\n",
           max_of(5, -1, -5, -3, -2, -4));
    printf("max_of(1, 42) = %d\n", max_of(1, 42));
    return 0;
}
```

<details><summary>Predicción</summary>

```
max_of(3, 10, 50, 30) = 50
max_of(5, -1, -5, -3, -2, -4) = -1
max_of(1, 42) = 42
```

- max empieza en `INT_MIN` (-2147483648 en 32-bit), así que cualquier valor pasado
  será mayor.
- Con todos negativos, -1 es el mayor.
- Con un solo argumento, ese argumento es el máximo.

`max_of(0)` retornaría `INT_MIN` — el loop no ejecuta. Esto podría considerarse
un error; una alternativa sería usar el primer argumento como valor inicial.

</details>

---

### Ejercicio 8 — va_copy para doble pasada

```c
#include <stdio.h>
#include <stdarg.h>

void avg_and_print(int count, ...) {
    va_list args, copy;
    va_start(args, count);
    va_copy(copy, args);

    // Primera pasada: calcular promedio
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += va_arg(args, int);
    }
    double avg = (count > 0) ? (double)total / count : 0.0;

    // Segunda pasada: imprimir valores
    printf("Values: ");
    for (int i = 0; i < count; i++) {
        printf("%d ", va_arg(copy, int));
    }
    printf("\nAverage: %.1f\n", avg);

    va_end(copy);
    va_end(args);
}

int main(void) {
    avg_and_print(4, 10, 20, 30, 40);
    avg_and_print(3, 7, 8, 9);
    return 0;
}
```

<details><summary>Predicción</summary>

```
Values: 10 20 30 40
Average: 25.0
Values: 7 8 9
Average: 8.0
```

- Primera pasada (con `args`): suma 10+20+30+40 = 100, promedio = 25.0.
- Segunda pasada (con `copy`): imprime cada valor. `copy` fue creada con `va_copy`
  antes de consumir `args`, así que empieza desde el primer argumento.

Sin `va_copy`, la segunda pasada leería después del último argumento (UB)
porque `args` ya fue consumida por la primera pasada.

Ambos `va_list` necesitan `va_end`.

</details>

---

### Ejercicio 9 — Concatenar strings

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Concatena todas las strings hasta NULL.
// El caller debe hacer free() del resultado.
char *concat(const char *first, ...) {
    // Primera pasada: calcular longitud total
    va_list args, copy;
    va_start(args, first);
    va_copy(copy, args);

    size_t total = strlen(first);
    const char *s;
    while ((s = va_arg(args, const char *)) != NULL) {
        total += strlen(s);
    }

    // Segunda pasada: copiar
    char *result = malloc(total + 1);
    if (!result) { va_end(copy); va_end(args); return NULL; }

    strcpy(result, first);
    while ((s = va_arg(copy, const char *)) != NULL) {
        strcat(result, s);
    }

    va_end(copy);
    va_end(args);
    return result;
}

int main(void) {
    char *s = concat("Hello", ", ", "World", "!", NULL);
    if (s) {
        printf("'%s' (len=%zu)\n", s, strlen(s));
        free(s);
    }

    char *t = concat("one", NULL);
    if (t) {
        printf("'%s'\n", t);
        free(t);
    }
    return 0;
}
```

<details><summary>Predicción</summary>

```
'Hello, World!' (len=13)
'one'
```

- `concat("Hello", ", ", "World", "!", NULL)`:
  - Primera pasada: strlen("Hello")=5 + strlen(", ")=2 + strlen("World")=5 + strlen("!")=1 = 13.
  - malloc(14). Segunda pasada: strcpy "Hello", strcat ", ", strcat "World", strcat "!".
  - Resultado: "Hello, World!" (13 chars + null terminator).
- `concat("one", NULL)`: solo `first`, sin argumentos variables → "one".

`va_copy` es necesario para hacer dos pasadas: una para medir, otra para copiar.
El caller es responsable de `free()`.

</details>

---

### Ejercicio 10 — Formato incorrecto: predecir el desastre

```c
#include <stdio.h>

int main(void) {
    // ¿Qué pasa con cada una de estas llamadas?
    // NO ejecutar — son UB. Solo predecir.

    // 1. Tipo incorrecto:
    // printf("%d\n", 3.14);

    // 2. Tipo incorrecto (inverso):
    // printf("%f\n", 42);

    // 3. Argumento faltante:
    // printf("%d %d\n", 10);

    // 4. String donde hay int:
    // printf("%s\n", 42);

    // Para verificación segura, usar -Wall -Wformat:
    printf("Safe: %d %.2f %s\n", 42, 3.14, "hello");
    return 0;
}
```

<details><summary>Predicción</summary>

Salida (la línea segura):
```
Safe: 42 3.14 hello
```

Análisis de las líneas comentadas (todas son UB):

1. `printf("%d\n", 3.14)` — `%d` lee `va_arg(args, int)`, pero 3.14 es un `double`
   (8 bytes). Lee 4 bytes de un double → basura. Podría imprimir 0 o algún número
   grande dependiente de la representación IEEE 754.

2. `printf("%f\n", 42)` — `%f` lee `va_arg(args, double)` (8 bytes), pero 42 es un
   `int` (4 bytes). Lee 4 bytes extra del stack → basura. Podría imprimir 0.000000
   o un número enorme.

3. `printf("%d %d\n", 10)` — El segundo `%d` lee un `int` del stack más allá de lo
   pasado → basura del stack. Podría imprimir "10 0" o "10 32767" o cualquier cosa.

4. `printf("%s\n", 42)` — `%s` lee `va_arg(args, const char *)`, interpreta 42
   como dirección de memoria (0x2A). Intenta leer un string en esa dirección →
   segfault probable.

GCC con `-Wall -Wformat` detecta los 4 casos y emite warnings. Esto funciona
porque la declaración de `printf` en glibc tiene `__attribute__((format(printf, 1, 2)))`.

</details>
