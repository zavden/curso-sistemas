# T03 — restrict

## Qué es restrict

`restrict` (C99) es una promesa del programador al compilador:
**este puntero es la única forma de acceder a este dato**.
Ningún otro puntero apunta a la misma memoria:

```c
// Sin restrict:
void add(int *a, int *b, int *out, int n) {
    for (int i = 0; i < n; i++) {
        out[i] = a[i] + b[i];
    }
}

// Con restrict:
void add_fast(int *restrict a, int *restrict b, int *restrict out, int n) {
    for (int i = 0; i < n; i++) {
        out[i] = a[i] + b[i];
    }
}
```

```c
// ¿Qué cambia?
// Sin restrict, el compilador no puede asumir que a, b y out
// apuntan a memoria diferente. Tal vez out == a. Entonces:
//
//   out[i] = a[i] + b[i];
//
// Cada vez que escribe en out[i], podría cambiar a[i] o b[i].
// El compilador DEBE releer a[i] y b[i] de memoria cada iteración.
//
// Con restrict, el compilador SABE que no se solapan.
// Puede cargar a[] y b[] en registros, vectorizar el loop,
// y escribir out[] sin preocuparse de alias.
```

## Por qué importa para el rendimiento

```c
// Ejemplo: multiplicar un array por un escalar y sumar a destino:

// Sin restrict — el compilador no puede optimizar:
void scale_add(double *dest, const double *src, double factor, int n) {
    for (int i = 0; i < n; i++) {
        dest[i] += src[i] * factor;
    }
    // Si dest == src, escribir en dest[i] cambia src[i].
    // El compilador genera: load src[i], multiply, load dest[i], add, store.
    // Cada iteración accede a memoria.
}

// Con restrict — el compilador puede vectorizar:
void scale_add_fast(double *restrict dest, const double *restrict src,
                    double factor, int n) {
    for (int i = 0; i < n; i++) {
        dest[i] += src[i] * factor;
    }
    // El compilador sabe que dest y src no se solapan.
    // Puede usar instrucciones SIMD (SSE/AVX) para procesar
    // 4 doubles a la vez. Diferencia: 2-4x más rápido.
}
```

```c
// Verificar con gcc -O2 -S:
// Sin restrict: el loop tiene loads/stores individuales
// Con restrict: el loop usa instrucciones vectoriales (vmovapd, vaddpd)

// memcpy vs memmove es el ejemplo clásico:
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
// memcpy PROMETE que no se solapan → puede copiar en cualquier orden

void *memmove(void *dest, const void *src, size_t n);
// memmove NO promete → debe verificar dirección y copiar byte a byte
// si hay solapamiento

// Por eso memcpy es más rápido que memmove.
```

## Cómo usar restrict

```c
// restrict solo se aplica a PUNTEROS:
void foo(int *restrict p);        // OK
// int restrict x;                // ERROR — no es un puntero

// Típicamente en parámetros de funciones:
void process(const float *restrict input,
             float *restrict output,
             int n) {
    for (int i = 0; i < n; i++) {
        output[i] = input[i] * 2.0f;
    }
}

// En variables locales (menos común):
int *restrict p = malloc(100 * sizeof(int));
```

```c
// restrict en la biblioteca estándar:
// Muchas funciones usan restrict para indicar no-solapamiento:

char *strcpy(char *restrict dest, const char *restrict src);
int printf(const char *restrict format, ...);
int scanf(const char *restrict format, ...);
int snprintf(char *restrict str, size_t size,
             const char *restrict format, ...);
FILE *fopen(const char *restrict filename,
            const char *restrict mode);

// Estas funciones asumen que los punteros no se solapan.
// Violar esa restricción es UB.
```

## Cuándo la promesa se rompe → UB

```c
void copy(int *restrict a, int *restrict b, int n) {
    for (int i = 0; i < n; i++) {
        a[i] = b[i];
    }
}

int data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

// CORRECTO — arrays separados:
int dest[10];
copy(dest, data, 10);    // OK — no se solapan

// UB — mismo array:
copy(data + 5, data, 5);    // UB — a y b se solapan
// a = &data[5], b = &data[0]
// a[0..4] y b[0..4] no se solapan en este caso,
// pero la promesa de restrict dice que a y b no apuntan
// a la misma "región" — el compilador puede reordenar
// las operaciones y producir resultados incorrectos.
```

```c
// Ejemplo de cómo restrict permite optimización que rompe con alias:

void update(int *restrict a, int *restrict b) {
    *a = 1;
    *b = 2;
    // El compilador puede reordenar: primero *b = 2, luego *a = 1
    // porque restrict dice que a y b no se solapan.
    printf("%d\n", *a);    // DEBE ser 1 con restrict
}

int x;
update(&x, &x);    // UB — a y b apuntan al mismo objeto
// Sin restrict: printf imprime 2 (b sobrescribió a)
// Con restrict: printf puede imprimir 1 (reordenamiento) o 2
// Es comportamiento indefinido — cualquier resultado es válido.
```

## Cuándo usar restrict

```c
// USAR restrict cuando:
// 1. Los punteros realmente no se solapan
// 2. La función es performance-critical (loops numéricos)
// 3. Es una interfaz pública con documentación clara

// Ejemplos buenos:
void matrix_multiply(const double *restrict A,
                     const double *restrict B,
                     double *restrict C,
                     int m, int n, int p);

void fft(const double *restrict input,
         double *restrict output, int n);

void image_blend(const uint8_t *restrict src1,
                 const uint8_t *restrict src2,
                 uint8_t *restrict dest,
                 int width, int height);
```

```c
// NO usar restrict cuando:
// 1. Los punteros PUEDEN solaparse legítimamente
// 2. El código no es performance-critical
// 3. No estás seguro de las garantías de no-solapamiento

// El riesgo de restrict es que si te equivocas,
// introduces UB silencioso que puede manifestarse
// como bugs sutiles solo con optimización alta.
```

## restrict vs const

```c
// const y restrict son ortogonales:
// const = no modifico el dato
// restrict = nadie más accede al dato

void foo(const int *restrict p);
// No modifico *p, y nadie más lo está accediendo.
// El compilador puede cachear *p en un registro
// y nunca releerlo de memoria.

void bar(const int *p);
// No modifico *p, pero otro puntero podría.
// El compilador debe releer *p si alguien más pudo cambiarlo.
```

## restrict solo existe en C

```c
// C99: restrict es un keyword
// C++: NO tiene restrict (la semántica de aliasing es diferente)
// GCC/Clang en C++: __restrict__ como extensión

// En C++, se usa __restrict__ (no estándar):
// void foo(int *__restrict__ a, int *__restrict__ b);
```

---

## Ejercicios

### Ejercicio 1 — Comparar assembly

```bash
# Escribir una función que sume dos arrays de doubles:
# 1. Sin restrict
# 2. Con restrict
# Compilar ambas con gcc -O2 -S y comparar el assembly.
# ¿Qué instrucciones SIMD aparecen con restrict?
```

### Ejercicio 2 — Uso correcto

```c
// Agregar restrict donde sea apropiado en estas funciones:
void vector_add(float *out, const float *a, const float *b, int n);
void string_copy(char *dest, const char *src);
void swap(int *a, int *b);
// ¿En cuál NO se debe usar restrict y por qué?
```

### Ejercicio 3 — Demostrar UB

```c
// Crear una función void copy(int *restrict a, int *restrict b, int n)
// que copie b a a.
// 1. Llamar con arrays separados — verificar resultado correcto.
// 2. Llamar con arrays solapados — ¿qué pasa con -O0 vs -O2?
// Documentar las diferencias.
```
