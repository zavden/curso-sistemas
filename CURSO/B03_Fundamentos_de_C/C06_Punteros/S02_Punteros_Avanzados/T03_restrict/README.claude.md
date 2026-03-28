# T03 — `restrict`

> **Fuente**: `README.md`, `LABS.md`, `labs/README.md`, `aliasing_problem.c`, `aliasing.c`, `restrict_violation.c`, `memcpy_vs_memmove.c`

## Erratas detectadas

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| `README.md` líneas 134-140 | Dice que `copy(data + 5, data, 5)` es UB por `restrict`, pero `a[0..4]` = data[5..9] y `b[0..4]` = data[0..4] son disjuntos. El propio comentario reconoce que "no se solapan en este caso" pero concluye erróneamente que es UB | `restrict` solo prohíbe acceder al **mismo objeto** a través de punteros distintos (C11 §6.7.3.1). Si las regiones no se solapan, no hay violación. Un ejemplo correcto de UB sería `copy(data + 3, data, 5)` donde a[0..4] = data[3..7] y b[0..4] = data[0..4] se solapan en data[3..4] |

---

## 1. Qué es `restrict` — promesa de no-aliasing

`restrict` (C99) es una **promesa del programador al compilador**: este puntero es la **única forma** de acceder a los datos a los que apunta. Ningún otro puntero accede a la misma memoria:

```c
// Sin restrict:
void add(int *a, int *b, int *out, int n);

// Con restrict:
void add_fast(int *restrict a, int *restrict b, int *restrict out, int n);
```

No es una restricción que el compilador verifique — es una promesa que el programador debe cumplir. Si la promesa es falsa, el resultado es **comportamiento indefinido**.

---

## 2. El problema del aliasing

Sin `restrict`, el compilador debe asumir que dos punteros **podrían** apuntar a la misma memoria. Esto impide optimizaciones:

```c
void scale_add(double *dst, const double *src, double factor, int n) {
    for (int i = 0; i < n; i++) {
        dst[i] += src[i] * factor;
    }
}
```

¿Qué pasa si `dst == src`? Cada escritura en `dst[i]` **cambia** `src[i]`. El compilador **no puede**:
- Cargar `src[i]` una vez y reusar el valor
- Reordenar operaciones entre iteraciones
- Procesar múltiples elementos a la vez (vectorizar)

Debe generar código conservador: cargar → operar → almacenar → repetir, un elemento a la vez.

Ejemplo concreto de aliasing legítimo:

```c
double self[] = {1.0, 2.0, 3.0, 4.0};
scale_add(self, self, 1.0, 4);
// dst == src → self[i] += self[i] * 1.0 → self[i] *= 2
// Resultado: {2.0, 4.0, 6.0, 8.0}
```

Este uso es válido (sin `restrict`). El compilador debe soportarlo, lo que le impide optimizar.

---

## 3. Cómo `restrict` habilita la vectorización

Con `restrict`, el compilador **sabe** que los punteros no se solapan y puede vectorizar:

```c
void scale_add_fast(double *restrict dst, const double *restrict src,
                    double factor, int n) {
    for (int i = 0; i < n; i++) {
        dst[i] += src[i] * factor;
    }
}
```

El compilador puede:
1. Cargar **múltiples** elementos de `src` en registros SIMD
2. Multiplicar todos por `factor` en paralelo
3. Sumar a los elementos correspondientes de `dst`
4. Escribir todos los resultados de una vez

Verificable con `gcc -O2 -S`:

| Sin `restrict` | Con `restrict` |
|-----------------|----------------|
| `movsd` (1 double) | `movupd` (2 doubles) |
| `addsd` (1 suma) | `addpd` (2 sumas en paralelo) |
| `mulsd` (1 mult) | `mulpd` / `vfmadd` (2 en paralelo) |
| Loop: 1 elemento/iteración | Loop: 2-4 elementos/iteración |

Diferencia de rendimiento: **2-4x** en loops numéricos.

Para integers, la diferencia es aún más visible:

| Sin `restrict` | Con `restrict` |
|-----------------|----------------|
| `movl` + `addl` (1 int) | `movdqu` + `paddd` (4 ints en XMM) |
| Escalar | Vectorizado |

---

## 4. Uso práctico: sintaxis y dónde aplicar

`restrict` solo se aplica a **punteros**:

```c
void foo(int *restrict p);        // OK
// int restrict x;                // ERROR — no es un puntero
```

Se usa típicamente en parámetros de funciones:

```c
void process(const float *restrict input,
             float *restrict output,
             int n) {
    for (int i = 0; i < n; i++) {
        output[i] = input[i] * 2.0f;
    }
}
```

También en variables locales (menos común):

```c
int *restrict p = malloc(100 * sizeof(int));
// p es la única forma de acceder a ese bloque de heap
```

### Cuándo usar `restrict`

Usar cuando:
1. Los punteros **realmente no se solapan**
2. La función es **performance-critical** (loops numéricos, procesamiento de señales, álgebra lineal)
3. Es una interfaz pública con documentación clara

```c
// Buenos candidatos:
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

### Cuándo NO usar `restrict`

- Los punteros **pueden** solaparse legítimamente
- El código no es performance-critical
- No estás seguro de las garantías de no-solapamiento

El riesgo: si te equivocas, introduces UB silencioso que puede manifestarse como bugs sutiles **solo con optimización alta**.

---

## 5. `restrict` en la biblioteca estándar

Muchas funciones estándar usan `restrict` en su firma:

```c
char *strcpy(char *restrict dest, const char *restrict src);
int printf(const char *restrict format, ...);
int scanf(const char *restrict format, ...);
int snprintf(char *restrict str, size_t size,
             const char *restrict format, ...);
FILE *fopen(const char *restrict filename,
            const char *restrict mode);
```

Todas asumen que sus punteros no se solapan. Pasar punteros solapados a cualquiera de estas funciones es UB, aunque compile sin errores.

El caso más conocido: `memcpy` vs `memmove` (sección 7).

---

## 6. Violación de `restrict` → UB

Cuando la promesa de `restrict` se rompe, el compilador genera código incorrecto basándose en una suposición falsa:

```c
void update(int *restrict a, int *restrict b) {
    *a = 10;
    *b = 20;
    printf("%d\n", *a);    // Con restrict, el compilador "sabe" que *a es 10
}

int x;
update(&x, &x);    // VIOLACIÓN — a y b apuntan al mismo objeto
```

| Nivel | Resultado de `*a` | Razón |
|-------|-------------------|-------|
| `-O0` | 20 | Ejecución literal: `*a=10`, `*b=20` (sobrescribe), lee `*a` = 20 |
| `-O2` | 10 | El compilador cacheó `*a=10` en un registro. La escritura a `*b` "no puede" afectar `*a` (por restrict), así que usa el valor cacheado |

El valor real en memoria es 20 en ambos casos, pero con `-O2` el `printf` usa el valor cacheado (10). **El resultado cambia según el nivel de optimización** — señal clara de UB.

### Cuándo hay violación y cuándo no

La clave es si las **regiones accedidas** se solapan, no si los punteros vienen del mismo array:

```c
void copy(int *restrict a, int *restrict b, int n) {
    for (int i = 0; i < n; i++) {
        a[i] = b[i];
    }
}

int data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

// OK — regiones disjuntas:
copy(data + 5, data, 5);
// a accede data[5..9], b accede data[0..4] — no se solapan

// UB — regiones solapadas:
copy(data + 3, data, 5);
// a accede data[3..7], b accede data[0..4] — se solapan en data[3..4]
// El mismo objeto (data[3], data[4]) se accede a través de a Y b
```

`restrict` prohíbe que el mismo **objeto** se acceda a través de punteros `restrict` distintos (C11 §6.7.3.1). Si las regiones no se solapan, no hay violación aunque ambos punteros apunten al mismo array subyacente.

---

## 7. `memcpy` vs `memmove` — el ejemplo canónico

```c
void *memcpy(void *restrict dst, const void *restrict src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
```

La **única** diferencia en la firma: `memcpy` tiene `restrict`, `memmove` no.

### Sin solapamiento — ambas son equivalentes

```c
int src[] = {1, 2, 3, 4, 5};
int dst[5];
memcpy(dst, src, 5 * sizeof(int));    // OK
memmove(dst, src, 5 * sizeof(int));   // OK — mismo resultado
```

### Con solapamiento — solo `memmove` es segura

```c
int data[] = {1, 2, 3, 4, 5, 6, 7, 8};

// Copiar data[0..4] → data[2..6] (solapamiento hacia adelante):
memmove(data + 2, data, 5 * sizeof(int));
// Resultado correcto: {1, 2, 1, 2, 3, 4, 5, 8}
// memmove detecta que dst > src y copia de atrás hacia adelante

memcpy(data + 2, data, 5 * sizeof(int));
// UB — violación de restrict. Puede dar resultado correcto o no,
// dependiendo de la implementación de libc.
```

### Por qué `memcpy` es más rápida

- `memcpy` no necesita comprobar solapamiento — puede copiar en **cualquier orden** (adelante, bloques SIMD de 64 bytes con AVX-512)
- `memmove` debe comparar `dst` y `src`, y si se solapan, elegir la dirección correcta

En la práctica, la diferencia es pequeña para copias pequeñas. Para megabytes, `memcpy` puede ser mediblemente más rápida.

**Regla simple**: si sabes que no se solapan → `memcpy`. Si podrían solaparse → `memmove`.

---

## 8. `restrict` vs `const` — ortogonales

`const` y `restrict` son calificadores independientes que pueden combinarse:

```c
void foo(const int *restrict p);
// No modifico *p (const), y nadie más lo está accediendo (restrict).
// El compilador puede cachear *p en un registro y nunca releerlo.

void bar(const int *p);
// No modifico *p (const), pero otro puntero podría modificar el dato.
// El compilador debe releer *p si otro puntero pudo cambiarlo.
```

| Calificador | Promesa | Beneficio |
|-------------|---------|-----------|
| `const` | "Yo no modifico esto" | Documentación + error si intento modificar |
| `restrict` | "Nadie más accede a esto" | Permite vectorización y caching |
| Ambos | "Yo no modifico, y nadie más accede" | Máxima optimización posible |

---

## 9. `restrict` solo existe en C

```c
// C99:    restrict es un keyword
// C++:    NO tiene restrict
// GCC/Clang en C++: __restrict__ como extensión (no estándar)

// En C++:
void foo(int *__restrict__ a, int *__restrict__ b);   // extensión GCC/Clang
```

C++ no tiene `restrict` porque su modelo de aliasing y optimización funciona diferente. Las extensiones `__restrict__` son útiles en código numérico de alto rendimiento compilado como C++, pero no son portables.

---

## 10. Comparación con Rust — ownership resuelve aliasing

En Rust, el borrow checker **garantiza en compilación** que no exista aliasing mutable:

```rust
fn add(a: &[i32], b: &[i32], out: &mut [i32]) {
    for i in 0..out.len() {
        out[i] = a[i] + b[i];
    }
}
```

Las reglas de Rust:
- Puedes tener múltiples `&T` (lecturas) **o** un único `&mut T` (escritura), **nunca ambos**
- Esto elimina el aliasing mutable **por diseño**

```rust
let mut data = vec![1, 2, 3, 4, 5];

// Esto NO compila — no puedes tener &data y &mut data a la vez:
// add(&data, &data, &mut data);

// Esto sí compila — slices disjuntos:
let (left, right) = data.split_at_mut(3);
// left y right son &mut [i32] a regiones disjuntas — el compilador lo verifica
```

| C con `restrict` | Rust |
|------------------|------|
| Promesa del programador | Garantía del compilador |
| Violación = UB silencioso | Violación = error de compilación |
| Solo en parámetros de funciones | Automático en todo el código |
| El programador es responsable | El borrow checker verifica |

El compilador de Rust (LLVM) internamente marca las referencias como `noalias` (equivalente a `restrict`) — obtiene las mismas optimizaciones sin que el programador tenga que recordar poner `restrict`.

---

## Ejercicios

### Ejercicio 1 — Ver el aliasing en acción

```c
// Escribe scale_add(double *dst, const double *src, double factor, int n)
// que haga dst[i] += src[i] * factor.
// Llámala dos veces:
//   a) Con arrays separados: dst = a, src = b
//   b) Con el mismo array: dst = self, src = self, factor = 1.0
// ¿Qué resultado da (b)? ¿Por qué self[i] se duplica?
```

<details><summary>Predicción</summary>

(a) Con arrays separados: `a[i] += b[i] * factor`, resultado normal. (b) Con `self == self`: `self[i] += self[i] * 1.0` equivale a `self[i] *= 2`. Cada elemento se duplica porque `dst` y `src` son el mismo array — la escritura en `dst[i]` modifica `src[i]` pero como `+= val * 1.0` lee y escribe el mismo índice en la misma iteración, el resultado es determinista. Esto demuestra aliasing legítimo que el compilador debe soportar sin `restrict`.

</details>

### Ejercicio 2 — Comparar assembly con y sin `restrict`

```c
// Escribe dos funciones idénticas de suma de arrays de int (n = 8):
//   void sum_no(int *a, int *b, int *out, int n);
//   void sum_yes(int *restrict a, int *restrict b, int *restrict out, int n);
// Compila con gcc -O2 -S y examina el assembly de cada una.
// Busca instrucciones paddd (4 ints en paralelo) vs addl (1 int).
// ¿Cuántas instrucciones SIMD tiene cada versión?
```

<details><summary>Predicción</summary>

`sum_no` usará instrucciones escalares (`movl`, `addl`) — un int por iteración. El compilador no puede vectorizar porque `out` podría solaparse con `a` o `b`. `sum_yes` usará instrucciones vectoriales (`movdqu` para cargar 4 ints, `paddd` para sumar 4 en paralelo, `movups` para almacenar 4). El loop de `sum_yes` procesará 4 ints por iteración. Ambas funciones producen el mismo resultado con arrays no solapados.

</details>

### Ejercicio 3 — Violación observable

```c
// Escribe void update(int *restrict a, int *restrict b) que haga:
//   *a = 10;
//   *b = 20;
//   printf("*a = %d\n", *a);
// Llama con update(&x, &x) — violación de restrict.
// Compila con -O0 y con -O2. ¿Qué valor imprime en cada caso?
// ¿Por qué difiere?
```

<details><summary>Predicción</summary>

Con `-O0`: imprime 20. Ejecución literal: `*a=10`, luego `*b=20` (sobrescribe porque `a == b`), luego lee `*a` que ahora es 20. Con `-O2`: imprime 10. El compilador, confiando en `restrict`, cachea `*a = 10` en un registro. La escritura `*b = 20` "no puede" afectar `*a`, así que `printf` usa el valor cacheado 10. El valor real en memoria es 20 en ambos casos, pero el optimizador no relee la memoria. Esto es UB — el resultado cambia con el nivel de optimización.

</details>

### Ejercicio 4 — ¿Violación o no?

```c
// Dada void copy(int *restrict dst, int *restrict src, int n);
// que copia src[0..n-1] a dst[0..n-1].
// Para cada llamada, di si viola restrict o no:
//   a) int a[10], b[10]; copy(a, b, 10);
//   b) int data[10]; copy(data + 5, data, 5);
//   c) int data[10]; copy(data + 3, data, 5);
//   d) int data[10]; copy(data, data, 5);
//   e) int data[10]; copy(data + 5, data, 6);
// Justifica cada respuesta con las regiones accedidas.
```

<details><summary>Predicción</summary>

(a) **No viola** — `a` y `b` son arrays completamente separados. (b) **No viola** — dst accede data[5..9], src accede data[0..4], disjuntos. (c) **Viola** — dst accede data[3..7], src accede data[0..4], se solapan en data[3..4]. (d) **Viola** — dst y src acceden exactamente la misma memoria data[0..4]. (e) **Viola** — dst accede data[5..10] (fuera de bounds, UB de por sí), src accede data[0..5], se solapan en data[5].

</details>

### Ejercicio 5 — `memcpy` vs `memmove`

```c
// Crea int data[] = {1,2,3,4,5,6,7,8}.
// a) Copia data[0..4] → data[2..6] con memmove. ¿Resultado?
// b) Restaura data. Copia data[2..6] → data[0..4] con memmove. ¿Resultado?
// c) ¿Por qué memmove copia hacia atrás en (a) y hacia adelante en (b)?
// d) Repite (a) con memcpy. ¿El resultado es correcto en tu sistema?
//    ¿Puedes confiar en que siempre lo sea?
```

<details><summary>Predicción</summary>

(a) `memmove(data+2, data, 5*sizeof(int))`: copia data[0..4] a data[2..6]. src < dst y se solapan, así que memmove copia de atrás hacia adelante. Resultado: {1, 2, 1, 2, 3, 4, 5, 8}. (b) `memmove(data, data+2, 5*sizeof(int))`: copia data[2..6] a data[0..4]. dst < src, copia hacia adelante. Resultado: {3, 4, 5, 6, 7, 6, 7, 8}. (c) Cuando dst > src y se solapan, copiar adelante corrompería los datos fuente. Copiar atrás preserva los originales. Cuando dst < src, copiar adelante es seguro. (d) `memcpy` con solapamiento es UB. En glibc moderna puede dar resultado correcto por coincidencia (misma implementación interna para bloques pequeños), pero no se puede confiar — en otra plataforma o tamaño, puede fallar.

</details>

### Ejercicio 6 — Agregar `restrict` correctamente

```c
// Agrega restrict donde sea apropiado:
// a) void vector_add(float *out, const float *a, const float *b, int n);
// b) void string_copy(char *dest, const char *src);
// c) void swap(int *a, int *b);
// d) int dot_product(const int *a, const int *b, int n);
// e) void self_scale(double *arr, double factor, int n);
// ¿En cuáles NO se debe usar restrict y por qué?
```

<details><summary>Predicción</summary>

(a) `float *restrict out, const float *restrict a, const float *restrict b` — tres punteros a regiones independientes. (b) `char *restrict dest, const char *restrict src` — igual que `strcpy` en la stdlib. (c) **No usar restrict** — `swap(&x, &y)` espera que `a` y `b` apunten a objetos diferentes, pero `swap(&x, &x)` sería UB con restrict (y aunque sea un no-op, es legítimo). (d) `const int *restrict a, const int *restrict b` — ambos solo leen, pero restrict permite al compilador asumir independencia y vectorizar la multiplicación. (e) **Un solo puntero** — no tiene sentido poner restrict (no hay otro puntero con el que aliasear). Se puede poner pero no aporta nada en ausencia de otros punteros.

</details>

### Ejercicio 7 — `restrict` y `const` combinados

```c
// Escribe una función que calcule la media de un array:
//   double mean(const double *restrict arr, int n);
// ¿Qué beneficio da restrict aquí si solo hay un puntero?
// Ahora escribe:
//   void normalize(double *restrict arr, const double *restrict mean_ptr, int n);
// que reste *mean_ptr a cada elemento.
// ¿Por qué restrict en mean_ptr es útil aquí?
```

<details><summary>Predicción</summary>

En `mean`: `restrict` con un solo puntero no aporta optimización significativa — no hay otro puntero con el que pueda aliasear. En `normalize`: `restrict` en `mean_ptr` es crucial. Sin restrict, el compilador debe considerar que `mean_ptr` podría apuntar dentro de `arr`. Cada vez que escribe `arr[i] -= *mean_ptr`, debe releer `*mean_ptr` porque la escritura en `arr[i]` podría haberlo cambiado. Con restrict, el compilador cachea `*mean_ptr` en un registro una vez y lo reutiliza en todas las iteraciones.

</details>

### Ejercicio 8 — Benchmark simple

```c
// Escribe un benchmark que mida el tiempo de sum_no_restrict vs sum_restrict
// sobre un array grande (1 millón de ints). Usa clock() antes y después.
// Compila con gcc -O2. ¿Hay diferencia medible?
// Repite con gcc -O0. ¿La diferencia desaparece?
// Nota: con -O0 restrict no tiene efecto porque no hay optimización.
```

<details><summary>Predicción</summary>

Con `-O2`: `sum_restrict` será 2-4x más rápida que `sum_no_restrict` porque el loop se vectoriza (procesa 4 ints por iteración vs 1). Con `-O0`: ambas tendrán rendimiento similar — el compilador no optimiza, así que `restrict` no tiene efecto. La diferencia exacta depende de la CPU y del soporte SIMD. En CPUs con AVX2, la diferencia puede ser mayor (8 ints por iteración).

</details>

### Ejercicio 9 — `restrict` con structs

```c
// Dado struct Vec3 { float x, y, z; };
// Escribe void vec3_add(struct Vec3 *restrict out,
//                       const struct Vec3 *restrict a,
//                       const struct Vec3 *restrict b);
// Prueba con dos llamadas:
//   a) vec3_add(&result, &v1, &v2);     // tres objetos distintos
//   b) vec3_add(&v1, &v1, &v2);         // out == a — ¿es UB?
// ¿Qué pasa si out y a son el mismo struct?
```

<details><summary>Predicción</summary>

(a) Tres objetos distintos — no hay violación, funciona correctamente. (b) `out == a` — **UB**: el mismo objeto (`v1`) se accede como escritura a través de `out` y como lectura a través de `a`. Con restrict, el compilador puede leer `a->x` antes o después de escribir `out->x`. Si reordena la escritura antes de la lectura, `a->x` ya tendría el valor nuevo, no el original. En la práctica, con structs pequeños el compilador suele generar el mismo código, pero formalmente es UB.

</details>

### Ejercicio 10 — Implementar `my_memcpy` y `my_memmove`

```c
// Implementa:
//   void *my_memcpy(void *restrict dst, const void *restrict src, size_t n);
//   void *my_memmove(void *dst, const void *src, size_t n);
//
// my_memcpy: copia byte a byte hacia adelante (simple, con restrict).
// my_memmove: comprueba si dst > src; si es así, copia hacia atrás.
//
// Prueba ambas con:
//   a) Regiones separadas — ambas correctas
//   b) Solapamiento hacia adelante — solo memmove correcta
//   c) Solapamiento hacia atrás — ambas correctas (¿por qué?)
```

<details><summary>Predicción</summary>

`my_memcpy` casta a `unsigned char *restrict` y copia `dst[i] = src[i]` de 0 a n-1. `my_memmove` compara `dst` y `src`: si `dst > src` (solapamiento hacia adelante), copia de n-1 a 0; si no, copia de 0 a n-1. (a) Sin solapamiento: ambas dan el mismo resultado. (b) Solapamiento hacia adelante (src < dst): `my_memcpy` corrompre datos porque sobrescribe fuente antes de leerla; `my_memmove` copia hacia atrás, preservando los originales. (c) Solapamiento hacia atrás (dst < src): `my_memcpy` funciona porque la copia hacia adelante lee cada byte antes de sobrescribirlo; `my_memmove` detecta que no necesita invertir la dirección. Ambas correctas, pero `my_memcpy` con solapamiento sigue siendo UB formal.

</details>
