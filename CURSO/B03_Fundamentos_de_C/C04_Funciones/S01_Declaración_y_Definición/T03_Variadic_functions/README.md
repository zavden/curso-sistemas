# T03 — Variadic functions

## Qué son las funciones variádicas

Una función variádica acepta un número **variable** de argumentos.
El ejemplo más conocido es `printf`:

```c
printf("hello\n");              // 1 argumento
printf("x = %d\n", x);         // 2 argumentos
printf("%d + %d = %d\n", a, b, a+b);  // 4 argumentos
```

```c
// Declaración: los ... indican argumentos variables:
int printf(const char *fmt, ...);
// Al menos un parámetro fijo (fmt), luego 0 o más extras

// No se puede tener una función solo con ...:
// int bad(...);    // ERROR: necesita al menos un parámetro fijo
```

## stdarg.h — La maquinaria

Para acceder a los argumentos variables, se usan las macros
de `<stdarg.h>`:

```c
#include <stdarg.h>

// va_list    — tipo que almacena el estado de iteración
// va_start   — inicializa va_list a partir del último parámetro fijo
// va_arg     — obtiene el siguiente argumento con un tipo dado
// va_end     — limpia va_list
// va_copy    — copia va_list (C99)
```

### Ejemplo básico

```c
#include <stdio.h>
#include <stdarg.h>

// Sumar n enteros:
int sum(int count, ...) {
    va_list args;
    va_start(args, count);       // inicializar después de count

    int total = 0;
    for (int i = 0; i < count; i++) {
        total += va_arg(args, int);   // obtener el siguiente int
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

### Paso a paso

```c
int sum(int count, ...) {
    // count = 3, los ... son: 10, 20, 30

    va_list args;
    // args es una variable que "apunta" a los argumentos variables

    va_start(args, count);
    // Inicializa args. El segundo parámetro es el ÚLTIMO
    // parámetro fijo (count). Los argumentos variables
    // empiezan DESPUÉS de count.

    int a = va_arg(args, int);   // a = 10 (primer variádico)
    int b = va_arg(args, int);   // b = 20 (segundo)
    int c = va_arg(args, int);   // c = 30 (tercero)

    va_end(args);
    // Limpia args. SIEMPRE llamar va_end antes de retornar.
    // Algunos sistemas necesitan esto para liberar recursos.

    return a + b + c;
}
```

## Cómo saber cuántos argumentos hay

La función variádica **no sabe** cuántos argumentos recibió.
Hay tres patrones para resolver esto:

### 1. Pasar el conteo explícitamente

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

### 2. Usar un sentinela (valor que marca el final)

```c
// El último argumento es un valor especial (ej. -1 o NULL):
int sum_sentinel(int first, ...) {
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

sum_sentinel(10, 20, 30, -1);    // -1 marca el final
```

```c
// Con punteros, NULL es el sentinela natural:
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

### 3. Format string (como printf)

```c
// El format string describe los argumentos:
void my_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            putchar(*p);
            continue;
        }
        p++;    // avanzar después del %
        switch (*p) {
            case 'd': {
                int val = va_arg(args, int);
                printf("%d", val);
                break;
            }
            case 's': {
                const char *val = va_arg(args, const char *);
                printf("%s", val);
                break;
            }
            case 'f': {
                double val = va_arg(args, double);
                printf("%f", val);
                break;
            }
            case '%':
                putchar('%');
                break;
        }
    }

    va_end(args);
}

my_printf("Name: %s, Age: %d, GPA: %f\n", "Alice", 25, 3.9);
```

## Promoción de argumentos variádicos

Los argumentos variádicos sufren **default argument promotions**:

```c
// Cuando se pasan a ..., los tipos se promueven:
// char, short → int
// float → double

// Por eso en va_arg:
// va_arg(args, char)   ← INCORRECTO (se promovió a int)
// va_arg(args, int)    ← CORRECTO

// va_arg(args, float)  ← INCORRECTO (se promovió a double)
// va_arg(args, double) ← CORRECTO

void example(int count, ...) {
    va_list args;
    va_start(args, count);

    // Si le pasaron un char 'A':
    int c = va_arg(args, int);    // usar int, no char

    // Si le pasaron un float 3.14f:
    double d = va_arg(args, double);  // usar double, no float

    va_end(args);
}
```

## Cómo funciona printf internamente

```c
// printf simplificado (concepto):
int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int count = 0;

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            putchar(*p);
            count++;
            continue;
        }
        p++;
        switch (*p) {
            case 'd':
                count += print_int(va_arg(args, int));
                break;
            case 'f':
                count += print_double(va_arg(args, double));
                break;
            case 's':
                count += print_string(va_arg(args, const char *));
                break;
            case 'c':
                putchar(va_arg(args, int));   // char → int promotion
                count++;
                break;
            // ... muchos más formatos
        }
    }

    va_end(args);
    return count;    // printf retorna el número de caracteres escritos
}
```

```c
// ¿Por qué printf con formato incorrecto es UB?
printf("%s\n", 42);
// printf lee va_arg(args, const char*) — interpreta 42 como puntero
// → acceso a dirección 0x2A → crash o basura

// ¿Por qué -Wall -Wformat lo detecta?
// GCC tiene soporte especial para printf.
// __attribute__((format(printf, 1, 2))) le dice al compilador
// que verifique los argumentos contra el format string.

// La declaración real de printf en glibc:
// int printf(const char *restrict fmt, ...)
//     __attribute__((format(printf, 1, 2)));
```

## va_copy (C99)

```c
// va_copy copia el estado de un va_list:
void example(int count, ...) {
    va_list args, args_copy;
    va_start(args, count);
    va_copy(args_copy, args);    // copiar el estado

    // Primera pasada: contar
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += va_arg(args, int);
    }

    // Segunda pasada: imprimir (usando la copia)
    for (int i = 0; i < count; i++) {
        printf("%d ", va_arg(args_copy, int));
    }

    va_end(args_copy);    // limpiar la copia también
    va_end(args);
}
```

## Pasar va_list a otra función

```c
// vprintf, vfprintf, vsprintf aceptan va_list directamente:

#include <stdio.h>
#include <stdarg.h>

void log_message(const char *level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    printf("[%s] ", level);
    vprintf(fmt, args);        // vprintf acepta va_list
    printf("\n");

    va_end(args);
}

log_message("INFO", "Server started on port %d", 8080);
// [INFO] Server started on port 8080

log_message("ERROR", "Failed to open %s: %s", path, strerror(errno));
// [ERROR] Failed to open /tmp/data: No such file or directory
```

```c
// Funciones v* de la biblioteca estándar:
int vprintf(const char *fmt, va_list args);
int vfprintf(FILE *stream, const char *fmt, va_list args);
int vsprintf(char *str, const char *fmt, va_list args);
int vsnprintf(char *str, size_t size, const char *fmt, va_list args);
```

## Peligros

```c
// 1. No hay verificación de tipos:
sum(3, 10, 20, "hello");   // el "hello" se interpreta como int → UB
// El compilador no puede verificar (a menos que use __attribute__)

// 2. Leer más argumentos de los pasados:
sum(2, 10, 20);
// Si la función lee 3 argumentos con va_arg: UB

// 3. Tipo incorrecto en va_arg:
int x = va_arg(args, double);  // si el argumento era int → UB
// va_arg no verifica el tipo — lee bytes ciegamente

// 4. Olvidar va_end:
// Puede causar resource leaks en algunos sistemas
```

## __attribute__((format)) para verificación

```c
// GCC puede verificar argumentos variádicos si le dices el formato:

void my_log(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
// Le dice a GCC:
// - El formato tipo printf está en el parámetro 1 (fmt)
// - Los argumentos variádicos empiezan en el parámetro 2

// Ahora GCC verifica:
my_log("x = %d\n", "hello");
// warning: format '%d' expects argument of type 'int',
//          but argument 2 has type 'char *'
```

---

## Ejercicios

### Ejercicio 1 — sum variádica

```c
// Implementar int sum(int count, ...) que sume count enteros.
// Probar: sum(3, 10, 20, 30) → 60
//         sum(1, 42) → 42
//         sum(0) → 0
```

### Ejercicio 2 — print con formato

```c
// Implementar void my_print(const char *fmt, ...)
// que soporte %d (int), %s (string) y %% (literal %).
// Probar: my_print("Hello %s, you are %d years old%%\n", "Alice", 25);
```

### Ejercicio 3 — log_message

```c
// Implementar void log_msg(const char *level, const char *fmt, ...)
// que imprima: [LEVEL] mensaje
// Usar vprintf internamente.
// Agregar __attribute__((format)) para verificación.
```
