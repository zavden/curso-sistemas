# T01 — `malloc`, `calloc`, `realloc`, `free`

> **Fuente**: `README.md`, `LABS.md`, `labs/README.md`, `malloc_basic.c`, `calloc_vs_malloc.c`, `realloc_grow.c`, `heap_map.c`
>
> Sin erratas detectadas en el material fuente.

---

## 1. El heap — mapa de memoria de un proceso

La memoria dinámica se aloca en el **heap**, una región de memoria que crece y se encoge durante la ejecución:

```
┌──────────────────┐  Dirección alta
│ Stack            │  ← variables locales, crece hacia abajo
│       ↓          │
│                  │
│       ↑          │
│ Heap             │  ← malloc/free, crece hacia arriba
├──────────────────┤
│ .bss             │  ← globals no inicializados (zero)
├──────────────────┤
│ .data            │  ← globals inicializados
├──────────────────┤
│ .rodata          │  ← string literals, constantes
├──────────────────┤
│ .text            │  ← código del programa
└──────────────────┘  Dirección baja
```

El heap y el stack crecen en **direcciones opuestas**. Verificable con `/proc/self/maps` o `pmap`:

```bash
cat /proc/<PID>/maps    # muestra todas las regiones
pmap <PID>              # formato más legible con tamaños
```

Las direcciones del stack están en la zona alta (~`0x7f...`), las del heap mucho más abajo, y `.text`/`.data`/`.bss` en la zona baja.

| Región | Contenido | Tamaño típico |
|--------|-----------|---------------|
| Stack | Variables locales, frames de función | ~1-8 MB (limitado) |
| Heap | Memoria de `malloc`/`calloc`/`realloc` | Limitado por RAM disponible |
| `.bss` | Globals no inicializados (a 0) | Fijo en compilación |
| `.data` | Globals inicializados | Fijo en compilación |
| `.rodata` | String literals, constantes | Fijo en compilación |
| `.text` | Código del programa | Fijo en compilación |

---

## 2. `malloc` — alocar memoria

```c
#include <stdlib.h>

// void *malloc(size_t size);
// Aloca size bytes en el heap.
// Retorna puntero a la memoria alocada, o NULL si falla.
// La memoria NO está inicializada (contiene basura).

int *p = malloc(sizeof(int));        // 1 int
int *arr = malloc(10 * sizeof(int)); // 10 ints
```

### Regla 1: SIEMPRE verificar el retorno

```c
int *p = malloc(sizeof(*p));
if (p == NULL) {
    perror("malloc");
    return -1;
}
```

Si `malloc` falla (memoria insuficiente), retorna `NULL`. Sin verificar, `*NULL = 42` es segfault.

### Regla 2: NO castear en C

```c
int *p = malloc(sizeof(int));           // CORRECTO en C
// int *p = (int *)malloc(sizeof(int)); // innecesario en C
```

En C, `void *` se convierte implícitamente a cualquier tipo de puntero. El cast es innecesario y en compiladores pre-C99 podía ocultar el olvido de `#include <stdlib.h>` (declaración implícita de función). En C99+ (incluido C11), la declaración implícita es un error, así que este argumento es menos relevante — pero sigue siendo buena práctica no castear porque es ruido visual innecesario.

En C++, el cast sí es obligatorio (C++ no permite conversión implícita de `void *`). Pero en C++ se usa `new`, no `malloc`.

### Regla 3: usar `sizeof(*variable)`

```c
int *data = malloc(n * sizeof(*data));
// Si cambias el tipo de data (ej. a long*),
// sizeof(*data) se actualiza automáticamente.
// Más seguro que malloc(n * sizeof(int)).
```

---

## 3. `calloc` — alocar e inicializar a cero

```c
// void *calloc(size_t nmemb, size_t size);
// Aloca nmemb × size bytes, inicializados a CERO.
// Retorna NULL si falla.

int *arr = calloc(10, sizeof(int));
if (arr == NULL) {
    perror("calloc");
    return -1;
}
// arr[0] == 0, arr[1] == 0, ..., arr[9] == 0
```

### Ventajas sobre `malloc` + `memset`

1. **Overflow check interno**: `calloc(n, sizeof(int))` verifica que `n × sizeof(int)` no desborde. `malloc(n * sizeof(int))` puede hacer overflow silencioso si `n` es muy grande, alocando menos memoria de la esperada.

2. **Potencialmente más eficiente**: el SO puede proveer páginas de memoria ya limpias (zero-fill-on-demand) sin necesidad de recorrer toda la memoria.

### Cuándo usar cada una

| Función | Inicializa | Overflow check | Usar cuando... |
|---------|:---:|:---:|---|
| `malloc` | No | No | Vas a llenar la memoria inmediatamente |
| `calloc` | Sí (a 0) | Sí | Necesitas ceros o `nmemb × size` podría desbordar |

**Nota**: en la primera ejecución, `malloc` también puede devolver ceros (el SO limpia las páginas por seguridad). Pero esto **no está garantizado** — en un programa que aloca y libera repetidamente, `malloc` devolvería residuos de usos anteriores.

---

## 4. `realloc` — cambiar tamaño

```c
// void *realloc(void *ptr, size_t size);
// Cambia el tamaño del bloque apuntado por ptr a size bytes.
// Puede mover el bloque a otra dirección.
// Retorna la nueva dirección, o NULL si falla (ptr queda intacto).

int *arr = malloc(5 * sizeof(int));
for (int i = 0; i < 5; i++) arr[i] = i + 1;

// Crecer:
int *tmp = realloc(arr, 10 * sizeof(int));
if (tmp == NULL) {
    perror("realloc");
    free(arr);    // arr sigue siendo válido — liberarlo
    return -1;
}
arr = tmp;
// Los primeros 5 valores se conservan.
// Los nuevos 5 tienen valores indeterminados.
```

### Error clásico: perder el puntero original

```c
// MAL — memory leak si realloc falla:
arr = realloc(arr, new_size);
// Si retorna NULL: arr = NULL → la memoria original se perdió

// CORRECTO — puntero temporal:
int *tmp = realloc(arr, new_size);
if (tmp == NULL) {
    free(arr);       // liberar la memoria original
    return -1;
}
arr = tmp;           // solo actualizar si tuvo éxito
```

### `realloc` internamente puede hacer tres cosas

1. **Extender in-place** — si hay espacio contiguo después del bloque (rápido)
2. **Mover** — alocar bloque nuevo, copiar datos, liberar el viejo (lento para bloques grandes)
3. **Fallar** — retornar `NULL` (el bloque original queda intacto)

No se puede predecir cuál ocurrirá — depende del estado del heap.

### Casos especiales

```c
realloc(NULL, size);     // equivale a malloc(size)
realloc(ptr, 0);         // comportamiento definido por implementación
                         // NO depender de este caso — usar free() explícitamente
```

---

## 5. `free` — liberar memoria

```c
// void free(void *ptr);
// Libera el bloque de memoria apuntado por ptr.
// ptr debe haber sido retornado por malloc/calloc/realloc.

int *p = malloc(sizeof(int));
*p = 42;
free(p);
p = NULL;    // buena práctica — previene use-after-free
```

### Reglas de `free`

```c
// 1. Solo liberar memoria de malloc/calloc/realloc:
int x = 42;
// free(&x);         // UB — x está en el stack

char *s = "hello";
// free(s);           // UB — string literal está en .rodata

// 2. No liberar dos veces (double free → corrupción del heap):
int *p = malloc(sizeof(int));
free(p);
// free(p);           // UB — double free

// 3. free(NULL) es seguro (no-op):
free(NULL);           // OK — no hace nada

// 4. No usar después de free (use-after-free):
free(p);
// *p = 42;           // UB
```

---

## 6. Patrón `sizeof(*ptr)` y overflow check

### `sizeof(*ptr)` — independiente del tipo

```c
// Si cambias de int* a long*, sizeof se actualiza:
int *data = malloc(n * sizeof(*data));    // sizeof(*data) = sizeof(int) = 4
long *data = malloc(n * sizeof(*data));   // sizeof(*data) = sizeof(long) = 8
```

### Overflow en `n * sizeof(T)`

Si `n` es muy grande, `n * sizeof(int)` puede hacer overflow de `size_t` y alocar un bloque mucho más pequeño de lo esperado:

```c
// Check manual antes de malloc:
if (n > SIZE_MAX / sizeof(int)) {
    // overflow — no alocar
    return NULL;
}
int *arr = malloc(n * sizeof(int));

// O usar calloc que hace el check internamente:
int *arr = calloc(n, sizeof(int));    // retorna NULL si n × sizeof(int) desborda
```

---

## 7. Array dinámico con crecimiento geométrico

El patrón fundamental de estructura de datos dinámica en C:

```c
struct DynArray {
    int *data;
    size_t len;     // elementos usados
    size_t cap;     // capacidad (elementos alocados)
};

int dynarray_init(struct DynArray *da, size_t initial_cap) {
    da->data = malloc(initial_cap * sizeof(*da->data));
    if (!da->data) return -1;
    da->len = 0;
    da->cap = initial_cap;
    return 0;
}

int dynarray_push(struct DynArray *da, int value) {
    if (da->len == da->cap) {
        size_t new_cap = da->cap * 2;    // duplicar capacidad
        int *tmp = realloc(da->data, new_cap * sizeof(*tmp));
        if (!tmp) return -1;
        da->data = tmp;
        da->cap = new_cap;
    }
    da->data[da->len++] = value;
    return 0;
}

void dynarray_free(struct DynArray *da) {
    free(da->data);
    da->data = NULL;
    da->len = 0;
    da->cap = 0;
}
```

**Crecimiento geométrico** (duplicar): cada `realloc` es caro (puede copiar todo el bloque), pero al duplicar la capacidad, el número de reallocs es logarítmico. Con cap inicial 4 y 100 elementos: solo 5 reallocs (4→8→16→32→64→128), no 96.

El factor 2 es el más simple. Algunos usan 1.5 (`new_cap = cap + cap / 2`) para reducir el pico de memoria. `Vec<T>` de Rust usa exactamente este patrón internamente.

---

## 8. Errores comunes

### 1. No verificar retorno de `malloc`

```c
int *p = malloc(1000000000 * sizeof(int));
p[0] = 42;    // si malloc retornó NULL → segfault
```

### 2. Calcular tamaño incorrecto

```c
int *p = malloc(10);               // 10 BYTES, no 10 ints (2.5 ints)
int *p = malloc(10 * 4);           // frágil — asume sizeof(int) == 4
int *p = malloc(10 * sizeof(int)); // correcto
int *p = malloc(10 * sizeof(*p));  // mejor — independiente del tipo
```

### 3. Olvidar `free` (memory leak)

```c
void process(void) {
    int *data = malloc(1000 * sizeof(int));
    // ... procesar ...
    if (error) return;    // LEAK — data no se liberó
    free(data);
}
// Solución: un solo punto de salida, o goto cleanup
```

### 4. `sizeof(puntero)` en vez de `sizeof(array)`

```c
void foo(int *arr, int n) {
    int *copy = malloc(sizeof(arr));    // BUG: sizeof(int*) = 8, no n*sizeof(int)
    // Correcto:
    int *copy = malloc(n * sizeof(*copy));
}
```

### 5. Off-by-one en strings

```c
char *copy = malloc(strlen(src));      // FALTA el '\0' — buffer overflow en strcpy
char *copy = malloc(strlen(src) + 1);  // correcto — espacio para el terminador
strcpy(copy, src);
// O más simple: char *copy = strdup(src);
```

---

## 9. Stack vs heap vs static

| Aspecto | Stack | Heap (`malloc`) | Static/Global |
|---------|-------|-----------------|---------------|
| Alocación | Automática | Manual (`malloc`) | Al inicio del programa |
| Liberación | Automática (al salir del scope) | Manual (`free`) | Al terminar el programa |
| Tamaño | Limitado (~1-8 MB) | Grande (RAM disponible) | Fijo en compilación |
| Velocidad | Muy rápida (mover stack pointer) | Más lenta (buscar bloque libre) | N/A |
| Fragmentación | No | Sí | No |
| Inicialización | Indeterminada | Indeterminada (`calloc` → 0) | 0 |
| Riesgo | Stack overflow | Leaks, dangling, double free | Thread safety |
| Lifetime | Hasta el fin del scope | Hasta `free()` | Todo el programa |

**Cuándo usar cada una**:
- **Stack**: datos pequeños y temporales, tamaño conocido en compilación
- **Heap**: datos grandes, tamaño desconocido hasta runtime, datos que sobreviven al scope
- **Static**: configuración global, tablas de lookup, contadores persistentes

---

## 10. Comparación con Rust

Rust elimina toda la gestión manual de memoria con **ownership**:

```rust
// Equivalente a malloc + uso + free:
let data = vec![1, 2, 3, 4, 5];    // aloca en heap
// ... usar data ...
// drop(data) se llama automáticamente al salir del scope

// Equivalente a realloc:
let mut data = Vec::new();
data.push(1);   // crece automáticamente (realloc interno)
data.push(2);
// La capacidad se duplica automáticamente — mismo patrón geométrico
```

| C | Rust |
|---|------|
| `malloc(n * sizeof(int))` | `Vec::with_capacity(n)` o `Box::new(...)` |
| `calloc(n, sizeof(int))` | `vec![0; n]` |
| `realloc(ptr, new_size)` | `vec.reserve(additional)` (automático en `push`) |
| `free(ptr); ptr = NULL;` | Automático al salir del scope (`Drop` trait) |
| Memory leak si olvidas `free` | Imposible — `Drop` siempre se ejecuta |
| Double free si `free` dos veces | Imposible — ownership se mueve, no se copia |
| Use-after-free | Imposible — borrow checker lo previene |

En Rust, el único costo es la curva de aprendizaje del borrow checker. En C, el costo es la disciplina constante del programador y las herramientas externas (Valgrind, ASan).

---

## Ejercicios

### Ejercicio 1 — `malloc` y `free` básicos

```c
// a) Aloca un int con malloc, asígnale 42, imprímelo, libera.
// b) Aloca un array de 10 ints, llénalos con i*i, imprímelos, libera.
// c) Usa sizeof(*ptr) en ambos casos.
// d) Verifica el retorno de malloc en ambos casos.
// Compila y ejecuta con valgrind para verificar 0 leaks.
```

<details><summary>Predicción</summary>

(a) `int *p = malloc(sizeof(*p)); *p = 42; printf("%d\n", *p); free(p); p = NULL;`. (b) `int *arr = malloc(10 * sizeof(*arr)); for(i=0;i<10;i++) arr[i] = i*i;`. Valgrind reporta: "All heap blocks were freed -- no leaks are possible". Las direcciones de `p` y `arr` estarán en la zona del heap (distinto del stack). Con `sizeof(*ptr)`, si se cambia `int *` a `long *`, el tamaño se actualiza automáticamente.

</details>

### Ejercicio 2 — `calloc` vs `malloc`

```c
// a) Aloca 5 ints con malloc. Imprime los valores SIN inicializarlos.
// b) Aloca 5 ints con calloc. Imprime los valores.
// c) ¿Los valores de (a) son cero? ¿Por qué podrían serlo?
// d) Libera ambos. Ahora aloca con malloc de nuevo e imprime.
//    ¿Los valores cambiaron respecto a (a)?
```

<details><summary>Predicción</summary>

(a) Los valores de `malloc` podrían ser 0 en la primera ejecución (el SO entrega páginas limpias), pero esto no está garantizado. (b) `calloc` siempre da 0 — garantizado por el estándar. (c) Pueden ser cero porque el SO limpia las páginas nuevas por seguridad (no exponer datos de otros procesos). (d) Después de free + malloc, la segunda alocación puede devolver el mismo bloque. Si fue modificado entre medias, contendrá residuos del uso anterior. En un programa simple puede seguir mostrando ceros.

</details>

### Ejercicio 3 — `realloc` con patrón seguro

```c
// Crea un array con malloc(4 * sizeof(int)). Llénalo con {10, 20, 30, 40}.
// Crece a 8 elementos con realloc. Usa puntero temporal.
// Imprime la dirección antes y después — ¿cambió?
// Llena los 4 nuevos elementos con {50, 60, 70, 80}.
// Imprime los 8 elementos. ¿Los primeros 4 se conservaron?
// Libera. Verifica con valgrind.
```

<details><summary>Predicción</summary>

La dirección puede cambiar o no — depende del estado del heap. Si hay espacio contiguo, `realloc` extiende in-place (misma dirección). Si no, mueve el bloque (dirección diferente). Los primeros 4 elementos ({10, 20, 30, 40}) siempre se conservan — `realloc` copia los datos existentes si mueve el bloque. Los 4 nuevos tienen valores indeterminados hasta que los inicializamos. Valgrind: 0 leaks, 0 errors.

</details>

### Ejercicio 4 — Error clásico de `realloc`

```c
// Demuestra el error: arr = realloc(arr, new_size);
// a) Simula un fallo: usa un tamaño absurdamente grande para que realloc falle.
//    ¿arr se convirtió en NULL? ¿La memoria original se perdió?
// b) Reescribe con el patrón seguro (puntero temporal).
//    ¿arr sigue válido después del fallo?
```

<details><summary>Predicción</summary>

(a) Con `arr = realloc(arr, SIZE_MAX)`, `realloc` falla y retorna `NULL`. `arr` ahora es `NULL` — la memoria original se perdió (memory leak). No se puede ni liberar ni usar. (b) Con `int *tmp = realloc(arr, SIZE_MAX); if(!tmp) { /* arr sigue válido */ }`, `arr` conserva la memoria original. Se puede seguir usando o liberar con `free(arr)`.

</details>

### Ejercicio 5 — Mapa de memoria

```c
// Declara: una global inicializada, una global no inicializada,
// un string literal, una variable local, y un malloc.
// Imprime la dirección de cada una con %p.
// Ordena las direcciones de menor a mayor.
// ¿Coincide con el modelo stack/heap/.bss/.data/.rodata?
// Imprime /proc/self/maps filtrando [heap] y [stack].
```

<details><summary>Predicción</summary>

De menor a mayor: `.rodata` (string literal) ≈ `.text`, `.data` (global inicializada) ≈ `.bss` (global no inicializada), heap (malloc), ... gran espacio ..., stack (variable local ~`0x7f...`). Las direcciones del stack están en la zona más alta. El heap está entre las secciones de datos y el stack. `/proc/self/maps` mostrará `[heap]` con permisos `rw-p` y `[stack]` con permisos `rw-p` en rangos que contienen las direcciones impresas.

</details>

### Ejercicio 6 — Array dinámico completo

```c
// Implementa struct DynArray con:
//   dynarray_init(da, initial_cap)
//   dynarray_push(da, value)      — crece ×2 si está lleno
//   dynarray_get(da, index)       — retorna valor o -1 si fuera de rango
//   dynarray_pop(da)              — retorna y elimina el último
//   dynarray_print(da)
//   dynarray_free(da)
// Push 20 elementos. Pop 5. Print. Free.
// Verifica con valgrind: 0 leaks, 0 errors.
```

<details><summary>Predicción</summary>

Con cap inicial 4 y 20 pushes: 4 reallocs (4→8→16→32). `len` = 20, `cap` = 32. Después de 5 pops: `len` = 15, `cap` = 32 (no se reduce automáticamente — shrink-to-fit requiere otro realloc explícito). `dynarray_get` verifica `index < da->len` antes de acceder. `dynarray_pop` decrementa `len` y retorna `da->data[da->len]` (el elemento que acaba de "eliminarse" — sigue en memoria pero ya no está "activo"). Valgrind: una única alocación al final (la última realloc), y un free.

</details>

### Ejercicio 7 — Matriz dinámica 2D

```c
// Implementa:
//   int **matrix_create(int rows, int cols)  — aloca filas + cada fila
//   void matrix_fill(int **m, int rows, int cols)  — m[i][j] = i*cols+j
//   void matrix_print(int **m, int rows, int cols)
//   void matrix_free(int **m, int rows)  — libera cada fila, luego el array
// Crea una matriz 3×4, llénala, imprímela, libérala.
// ¿Cuántos malloc se necesitan? ¿Cuántos free?
// Verifica con valgrind.
```

<details><summary>Predicción</summary>

Se necesitan `1 + rows` mallocs: 1 para el array de punteros `int **m = malloc(rows * sizeof(int *))`, y `rows` para cada fila `m[i] = malloc(cols * sizeof(int))`. Total: 4 mallocs para 3×4. Correspondientes 4 frees: primero las filas (internas), luego el array (externo). Orden de free es crucial: si liberas `m` primero, pierdes los punteros a las filas → memory leak. Valgrind: "All heap blocks were freed" si el orden es correcto.

</details>

### Ejercicio 8 — `strdup` manual

```c
// Implementa char *my_strdup(const char *s) que:
//   1. Calcula strlen(s) + 1
//   2. Aloca con malloc
//   3. Copia con memcpy (o strcpy)
//   4. Retorna el nuevo string (caller es responsable de free)
// Prueba con varios strings, incluyendo "" (vacío).
// ¿Qué retorna my_strdup("") — NULL o un puntero válido?
// Verifica con valgrind.
```

<details><summary>Predicción</summary>

`my_strdup("")` retorna un puntero válido a un bloque de 1 byte que contiene solo `'\0'`. No retorna `NULL` — `malloc(1)` es válido. `strlen("") + 1 = 1`. El caller debe hacer `free()` del resultado. Para strings normales: `strlen("hello") + 1 = 6` bytes, `memcpy` copia "hello\0". Valgrind: 0 leaks si se hace free de cada strdup.

</details>

### Ejercicio 9 — Overflow check

```c
// a) Intenta calloc(SIZE_MAX, sizeof(int)). ¿Retorna NULL?
// b) Intenta malloc(SIZE_MAX / 2 * sizeof(int)). ¿Retorna NULL?
// c) Calcula manualmente: si n = SIZE_MAX / sizeof(int) + 1,
//    ¿cuánto vale n * sizeof(int)? ¿Hay overflow?
// d) Implementa void *safe_malloc(size_t nmemb, size_t size) que
//    verifique overflow antes de alocar.
```

<details><summary>Predicción</summary>

(a) `calloc(SIZE_MAX, sizeof(int))` retorna `NULL` — calloc detecta internamente que `SIZE_MAX × 4` desborda. (b) `malloc(SIZE_MAX / 2 * sizeof(int))` probablemente retorna `NULL` — el sistema no tiene tanta memoria. Pero la multiplicación puede desbordar silenciosamente. (c) `n = SIZE_MAX/4 + 1`, `n * 4` = `(SIZE_MAX/4 + 1) * 4` = `SIZE_MAX + 4 - (SIZE_MAX % 4)` que desborda a un valor pequeño. (d) `safe_malloc` verifica `if (size != 0 && nmemb > SIZE_MAX / size) return NULL;` antes de `malloc(nmemb * size)`.

</details>

### Ejercicio 10 — Array dinámico de strings

```c
// Lee líneas de stdin (con fgets) hasta EOF (Ctrl+D).
// Almacena cada línea en un array dinámico de char* (con strdup).
// Usa realloc para crecer el array de punteros cuando sea necesario.
// Al final, imprime todas las líneas en orden inverso.
// Libera toda la memoria: cada strdup + el array de punteros.
// Verifica con valgrind: 0 leaks, 0 errors.
```

<details><summary>Predicción</summary>

Se necesitan dos niveles de alocación: (1) el array de `char *` punteros (crece con realloc, patrón geométrico), y (2) cada línea individual (con `strdup`). Para liberar: primero `free(lines[i])` para cada línea (lo interno), luego `free(lines)` para el array de punteros (lo externo). Orden crucial: interno antes que externo. `fgets` incluye el `\n` — se puede eliminar con `line[strcspn(line, "\n")] = '\0'` antes de `strdup`. Con 5 líneas de entrada: 1 malloc + N reallocs para el array, 5 strdups para las líneas. Valgrind debe mostrar todos los bloques liberados.

</details>
