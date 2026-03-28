# T01 — malloc, calloc, realloc, free

## El heap

La memoria dinámica se aloca en el **heap**, una región
de memoria que crece y se encoge durante la ejecución:

```c
// Mapa de memoria de un proceso:
//
// ┌──────────────────┐  Dirección alta
// │ Stack            │  ← variables locales, crece hacia abajo
// │       ↓          │
// │                  │
// │       ↑          │
// │ Heap             │  ← malloc/free, crece hacia arriba
// ├──────────────────┤
// │ .bss             │  ← globals no inicializados (zero)
// ├──────────────────┤
// │ .data            │  ← globals inicializados
// ├──────────────────┤
// │ .rodata          │  ← string literals, constantes
// ├──────────────────┤
// │ .text            │  ← código del programa
// └──────────────────┘  Dirección baja
//
// Stack: alocación automática, tamaño limitado (~1-8 MB)
// Heap: alocación manual, tamaño limitado por RAM disponible
```

## malloc — Alocar memoria

```c
#include <stdlib.h>

// void *malloc(size_t size);
// Aloca size bytes en el heap.
// Retorna puntero a la memoria alocada, o NULL si falla.
// La memoria NO está inicializada (contiene basura).

int *p = malloc(sizeof(int));       // 1 int
int *arr = malloc(10 * sizeof(int)); // 10 ints

// SIEMPRE verificar el retorno:
if (arr == NULL) {
    perror("malloc");
    return -1;
}

// En C, el cast de void* es implícito — NO castear:
int *p = malloc(sizeof(int));          // CORRECTO en C
// int *p = (int *)malloc(sizeof(int)); // innecesario — oculta bugs
// Sin el cast, si olvidas #include <stdlib.h> el compilador
// da warning. Con el cast, no da warning y tienes UB.
```

```c
// Patrón recomendado — usar sizeof(*variable):
int *data = malloc(n * sizeof(*data));
// Si cambias el tipo de data (ej. a long*),
// sizeof(*data) se actualiza automáticamente.
// Más seguro que malloc(n * sizeof(int)).

// Verificar overflow en la multiplicación:
// Si n es muy grande, n * sizeof(int) puede hacer overflow
// y alocar menos de lo esperado.
if (n > SIZE_MAX / sizeof(int)) {
    // overflow — no alocar
    return NULL;
}
```

## calloc — Alocar e inicializar a cero

```c
// void *calloc(size_t nmemb, size_t size);
// Aloca nmemb × size bytes, inicializados a CERO.
// Retorna NULL si falla.

int *arr = calloc(10, sizeof(int));
// Aloca 10 ints, todos inicializados a 0.
// Equivalente a malloc + memset:
//   int *arr = malloc(10 * sizeof(int));
//   memset(arr, 0, 10 * sizeof(int));

if (arr == NULL) {
    perror("calloc");
    return -1;
}

// arr[0] == 0, arr[1] == 0, ..., arr[9] == 0
```

```c
// Ventaja de calloc sobre malloc + memset:
// 1. calloc verifica overflow en nmemb × size internamente.
//    malloc(n * sizeof(int)) puede hacer overflow silencioso.
//    calloc(n, sizeof(int)) detecta el overflow y retorna NULL.
//
// 2. calloc puede ser más eficiente — el SO puede proveer
//    páginas de memoria ya limpias (zero-fill-on-demand).

// Cuándo usar cada una:
// malloc → cuando vas a llenar la memoria inmediatamente
// calloc → cuando necesitas memoria inicializada a cero
//          o cuando la multiplicación podría hacer overflow
```

## realloc — Cambiar tamaño

```c
// void *realloc(void *ptr, size_t size);
// Cambia el tamaño del bloque apuntado por ptr a size bytes.
// Puede mover el bloque a otra dirección.
// Retorna la nueva dirección (puede ser diferente de ptr).
// Retorna NULL si falla (ptr original queda intacto).

int *arr = malloc(5 * sizeof(int));
for (int i = 0; i < 5; i++) arr[i] = i + 1;

// Crecer:
int *tmp = realloc(arr, 10 * sizeof(int));
if (tmp == NULL) {
    // realloc falló — arr sigue siendo válido con 5 ints
    perror("realloc");
    free(arr);
    return -1;
}
arr = tmp;
// arr tiene 10 ints: los primeros 5 conservan sus valores,
// los nuevos 5 tienen valores indeterminados.
```

```c
// ERROR CLÁSICO — perder el puntero original:
arr = realloc(arr, new_size);    // MAL
// Si realloc falla: arr = NULL → memory leak
// La memoria original se perdió.

// CORRECTO — usar variable temporal:
int *tmp = realloc(arr, new_size);
if (tmp == NULL) {
    free(arr);    // liberar lo original
    return -1;
}
arr = tmp;
```

```c
// Casos especiales de realloc:
realloc(NULL, size);     // equivale a malloc(size)
realloc(ptr, 0);         // comportamiento definido por implementación
                         // en C17: puede retornar NULL o un puntero
                         // en C23: equivale a free(ptr), retorna indeterminado
                         // NO depender de este caso — usar free() explícitamente

// realloc puede:
// 1. Extender el bloque in-place (si hay espacio después)
// 2. Alocar un bloque nuevo, copiar datos, liberar el viejo
// 3. Fallar y retornar NULL
```

## free — Liberar memoria

```c
// void free(void *ptr);
// Libera el bloque de memoria apuntado por ptr.
// ptr debe haber sido retornado por malloc/calloc/realloc.

int *p = malloc(sizeof(int));
*p = 42;
free(p);
// La memoria ha sido devuelta al sistema.
// p sigue apuntando a la dirección anterior (dangling pointer).
p = NULL;    // buena práctica — previene use-after-free
```

```c
// Reglas de free:

// 1. Solo liberar memoria de malloc/calloc/realloc:
int x = 42;
// free(&x);        // UB — x está en el stack

char *s = "hello";
// free(s);          // UB — string literal está en .rodata

// 2. No liberar dos veces (double free):
int *p = malloc(sizeof(int));
free(p);
// free(p);          // UB — double free → corrupción del heap

// 3. free(NULL) es seguro (no-op):
free(NULL);          // OK — no hace nada

// 4. No usar después de free:
free(p);
// *p = 42;          // UB — use-after-free
```

## Patrón: array dinámico que crece

```c
#include <stdlib.h>
#include <stdio.h>

// Array dinámico con crecimiento geométrico:
struct DynArray {
    int *data;
    size_t len;     // elementos usados
    size_t cap;     // capacidad (elementos alocados)
};

int dynarray_init(struct DynArray *da, size_t initial_cap) {
    da->data = malloc(initial_cap * sizeof(int));
    if (!da->data) return -1;
    da->len = 0;
    da->cap = initial_cap;
    return 0;
}

int dynarray_push(struct DynArray *da, int value) {
    if (da->len == da->cap) {
        size_t new_cap = da->cap * 2;    // duplicar capacidad
        int *tmp = realloc(da->data, new_cap * sizeof(int));
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

int main(void) {
    struct DynArray arr;
    dynarray_init(&arr, 4);

    for (int i = 0; i < 100; i++) {
        dynarray_push(&arr, i);
    }
    printf("len=%zu cap=%zu\n", arr.len, arr.cap);
    // len=100 cap=128

    dynarray_free(&arr);
    return 0;
}
```

## Errores comunes

```c
// 1. No verificar retorno de malloc:
int *p = malloc(1000000000 * sizeof(int));
p[0] = 42;    // si malloc retornó NULL → segfault

// 2. Calcular tamaño incorrecto:
int *p = malloc(10);           // 10 BYTES, no 10 ints
int *p = malloc(10 * 4);      // frágil — asume sizeof(int) == 4
int *p = malloc(10 * sizeof(int));   // correcto
int *p = malloc(10 * sizeof(*p));    // mejor

// 3. Olvidar free (memory leak):
void process(void) {
    int *data = malloc(1000 * sizeof(int));
    // ... procesar ...
    if (error) return;    // LEAK — data no se liberó
    free(data);
}

// 4. Usar sizeof con puntero en vez de array:
void foo(int *arr) {
    int *copy = malloc(sizeof(arr));    // INCORRECTO — sizeof(int*) = 8
    // Quería: malloc(n * sizeof(int))
}

// 5. Off-by-one en strings:
char *copy = malloc(strlen(src));      // FALTA el '\0'
char *copy = malloc(strlen(src) + 1);  // correcto
strcpy(copy, src);
```

## malloc vs stack vs static

| Aspecto | Stack | Heap (malloc) | Static/Global |
|---|---|---|---|
| Alocación | Automática | Manual (malloc) | Al inicio del programa |
| Liberación | Automática (al salir del scope) | Manual (free) | Al terminar el programa |
| Tamaño | Limitado (~1-8 MB) | Grande (RAM disponible) | Fijo en compilación |
| Velocidad | Muy rápida | Más lenta | N/A |
| Fragmentación | No | Sí | No |
| Inicialización | Indeterminada | Indeterminada (calloc → 0) | 0 |
| Riesgo | Stack overflow | Leaks, dangling, double free | Thread safety |

---

## Ejercicios

### Ejercicio 1 — Array dinámico de strings

```c
// Leer líneas de stdin hasta EOF.
// Almacenar cada línea en un array dinámico (realloc).
// Al final, imprimir todas las líneas en orden inverso.
// Liberar toda la memoria. Verificar con valgrind.
```

### Ejercicio 2 — Matriz dinámica

```c
// Implementar:
// int **matrix_create(int rows, int cols) — aloca matriz
// void matrix_fill(int **m, int rows, int cols) — llena con i*cols+j
// void matrix_print(int **m, int rows, int cols) — imprime
// void matrix_free(int **m, int rows) — libera
// Verificar con valgrind que no hay leaks.
```

### Ejercicio 3 — realloc seguro

```c
// Implementar void *safe_realloc(void *ptr, size_t size)
// que envuelva realloc y maneje el caso de falla:
// - Si realloc falla, imprimir error y retornar NULL
//   (sin hacer free del original — dejar eso al caller)
// - Si size == 0, hacer free y retornar NULL
// Probar con crecimiento de un array.
```
