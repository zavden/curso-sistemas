# Implementación completa del heap en C

## Objetivo

Unificar todo lo visto en T01–T04 en una implementación robusta de un heap
binario en C, con las siguientes características:

- Array dinámico que crece según demanda.
- API completa: `push`, `pop`, `peek`, `build`, `size`, `is_empty`.
- **Genérico**: funciona con cualquier tipo de dato mediante `void *` y un
  comparador `int (*cmp)(const void *, const void *)`.
- Configurable como max-heap o min-heap cambiando solo el comparador.

---

## API del heap

```c
typedef int (*CmpFunc)(const void *, const void *);

typedef struct {
    void **data;       // array de punteros genericos
    int size;
    int capacity;
    CmpFunc cmp;       // comparador: positivo si a debe estar arriba de b
} Heap;

Heap   *heap_create(CmpFunc cmp);
void    heap_free(Heap *h);
void    heap_push(Heap *h, void *item);
void   *heap_pop(Heap *h);
void   *heap_peek(const Heap *h);
int     heap_size(const Heap *h);
int     heap_is_empty(const Heap *h);
void    heap_build(Heap *h, void **items, int n);
```

El comparador sigue la convención de `qsort`: retorna positivo si el primer
argumento debe considerarse "mayor" (estar más arriba en el heap). Para un
max-heap de enteros, retorna `a - b`. Para un min-heap, retorna `b - a`.

---

## Implementación paso a paso

### Creación y destrucción

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 16

typedef int (*CmpFunc)(const void *, const void *);

typedef struct {
    void **data;
    int size;
    int capacity;
    CmpFunc cmp;
} Heap;

Heap *heap_create(CmpFunc cmp) {
    Heap *h = malloc(sizeof(Heap));
    h->data = malloc(INITIAL_CAPACITY * sizeof(void *));
    h->size = 0;
    h->capacity = INITIAL_CAPACITY;
    h->cmp = cmp;
    return h;
}

void heap_free(Heap *h) {
    free(h->data);
    free(h);
}
```

`data` es un array de `void *` — cada elemento es un puntero al dato real.
Esto permite almacenar cualquier tipo sin conocerlo en tiempo de compilación.

**Nota sobre ownership**: `heap_free` libera el array de punteros, pero **no**
libera los objetos apuntados. Si el usuario alocó los datos con `malloc`, es
su responsabilidad liberarlos (o usar una versión extendida con un destructor).

### Funciones auxiliares

```c
static void swap(void **a, void **b) {
    void *tmp = *a;
    *a = *b;
    *b = tmp;
}

static void grow(Heap *h) {
    h->capacity *= 2;
    h->data = realloc(h->data, h->capacity * sizeof(void *));
}
```

### Sift-up y sift-down

Ahora usan el comparador en lugar de `>` o `<`:

```c
static void sift_up(Heap *h, int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        // cmp > 0 significa que data[index] debe estar arriba de data[parent]
        if (h->cmp(h->data[index], h->data[parent]) <= 0) break;
        swap(&h->data[index], &h->data[parent]);
        index = parent;
    }
}

static void sift_down(Heap *h, int index) {
    while (1) {
        int target = index;
        int left  = 2 * index + 1;
        int right = 2 * index + 2;

        if (left < h->size &&
            h->cmp(h->data[left], h->data[target]) > 0)
            target = left;
        if (right < h->size &&
            h->cmp(h->data[right], h->data[target]) > 0)
            target = right;

        if (target == index) break;
        swap(&h->data[index], &h->data[target]);
        index = target;
    }
}
```

La condición `cmp(a, b) > 0` significa "a tiene mayor prioridad que b". Para
un max-heap de enteros, esto es `*a > *b`. Para un min-heap, `*a < *b`. El
mismo código sift funciona para ambos — el comparador decide la semántica.

### Operaciones principales

```c
void heap_push(Heap *h, void *item) {
    if (h->size == h->capacity) grow(h);
    h->data[h->size] = item;
    sift_up(h, h->size);
    h->size++;
}

void *heap_pop(Heap *h) {
    if (h->size == 0) return NULL;
    void *top = h->data[0];
    h->size--;
    h->data[0] = h->data[h->size];
    if (h->size > 0) sift_down(h, 0);
    return top;
}

void *heap_peek(const Heap *h) {
    if (h->size == 0) return NULL;
    return h->data[0];
}

int heap_size(const Heap *h) {
    return h->size;
}

int heap_is_empty(const Heap *h) {
    return h->size == 0;
}
```

`heap_pop` retorna el puntero al dato pero **no** lo libera. El usuario
decide qué hacer con él.

### Build (heapify)

```c
void heap_build(Heap *h, void **items, int n) {
    // asegurar capacidad
    while (h->capacity < n) h->capacity *= 2;
    h->data = realloc(h->data, h->capacity * sizeof(void *));

    // copiar punteros
    memcpy(h->data, items, n * sizeof(void *));
    h->size = n;

    // heapify bottom-up
    for (int i = n / 2 - 1; i >= 0; i--) {
        sift_down(h, i);
    }
}
```

---

## Comparadores

### Para enteros (almacenados como punteros)

```c
// max-heap de enteros
int cmp_int_max(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

// min-heap de enteros
int cmp_int_min(const void *a, const void *b) {
    return *(const int *)b - *(const int *)a;
}
```

**Cuidado con overflow**: si los enteros pueden ser muy grandes (cercanos a
`INT_MAX`), la resta puede desbordar. Versión segura:

```c
int cmp_int_max_safe(const void *a, const void *b) {
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}
```

### Para doubles

```c
int cmp_double_max(const void *a, const void *b) {
    double x = *(const double *)a;
    double y = *(const double *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}
```

### Para strings

```c
int cmp_str_min(const void *a, const void *b) {
    // min-heap lexicografico: menor string tiene prioridad
    return -strcmp((const char *)a, (const char *)b);
}
```

### Para structs (ejemplo: procesos con prioridad)

```c
typedef struct {
    char name[32];
    int priority;
} Process;

int cmp_process(const void *a, const void *b) {
    const Process *pa = (const Process *)a;
    const Process *pb = (const Process *)b;
    return pa->priority - pb->priority;  // max-heap por prioridad
}
```

---

## Versión alternativa: almacenar datos por valor

La versión con `void *` es flexible pero tiene indirección (cada acceso
desreferencia un puntero). Para tipos simples como `int`, es más eficiente
almacenar los valores directamente:

```c
typedef struct {
    int *data;
    int size;
    int capacity;
} IntHeap;

IntHeap *int_heap_create(void) {
    IntHeap *h = malloc(sizeof(IntHeap));
    h->data = malloc(INITIAL_CAPACITY * sizeof(int));
    h->size = 0;
    h->capacity = INITIAL_CAPACITY;
    return h;
}

void int_heap_push(IntHeap *h, int value) {
    if (h->size == h->capacity) {
        h->capacity *= 2;
        h->data = realloc(h->data, h->capacity * sizeof(int));
    }
    h->data[h->size] = value;

    // sift-up inline
    int i = h->size;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h->data[p] >= h->data[i]) break;
        int tmp = h->data[p]; h->data[p] = h->data[i]; h->data[i] = tmp;
        i = p;
    }
    h->size++;
}

int int_heap_pop(IntHeap *h) {
    int top = h->data[0];
    h->size--;
    h->data[0] = h->data[h->size];

    // sift-down inline
    int i = 0;
    while (1) {
        int largest = i;
        int l = 2*i+1, r = 2*i+2;
        if (l < h->size && h->data[l] > h->data[largest]) largest = l;
        if (r < h->size && h->data[r] > h->data[largest]) largest = r;
        if (largest == i) break;
        int tmp = h->data[i]; h->data[i] = h->data[largest]; h->data[largest] = tmp;
        i = largest;
    }
    return top;
}
```

La versión por valor es ~2× más rápida que la versión `void *` para enteros,
porque:
- No hay indirección de punteros.
- Mejor localidad de caché (los ints están contiguos, no dispersos en el heap
  de memoria).
- No hay overhead de malloc para cada elemento.

---

## Patrón: cola de prioridad con heap

El heap es la implementación estándar de una cola de prioridad:

```c
// Cola de prioridad = Heap con interfaz renombrada
typedef Heap PriorityQueue;

PriorityQueue *pq_create(CmpFunc cmp) {
    return heap_create(cmp);
}

void pq_enqueue(PriorityQueue *pq, void *item) {
    heap_push(pq, item);
}

void *pq_dequeue(PriorityQueue *pq) {
    return heap_pop(pq);
}

void *pq_front(const PriorityQueue *pq) {
    return heap_peek(pq);
}
```

Es un typedef — la cola de prioridad **es** el heap, solo con nombres más
descriptivos para el contexto de uso.

---

## Programa completo

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 16

typedef int (*CmpFunc)(const void *, const void *);

typedef struct {
    void **data;
    int size;
    int capacity;
    CmpFunc cmp;
} Heap;

static void swap_ptr(void **a, void **b) {
    void *t = *a; *a = *b; *b = t;
}

Heap *heap_create(CmpFunc cmp) {
    Heap *h = malloc(sizeof(Heap));
    h->data = malloc(INITIAL_CAPACITY * sizeof(void *));
    h->size = 0;
    h->capacity = INITIAL_CAPACITY;
    h->cmp = cmp;
    return h;
}

void heap_free(Heap *h) {
    free(h->data);
    free(h);
}

static void sift_up(Heap *h, int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (h->cmp(h->data[index], h->data[parent]) <= 0) break;
        swap_ptr(&h->data[index], &h->data[parent]);
        index = parent;
    }
}

static void sift_down(Heap *h, int index) {
    while (1) {
        int target = index;
        int l = 2 * index + 1, r = 2 * index + 2;
        if (l < h->size && h->cmp(h->data[l], h->data[target]) > 0)
            target = l;
        if (r < h->size && h->cmp(h->data[r], h->data[target]) > 0)
            target = r;
        if (target == index) break;
        swap_ptr(&h->data[index], &h->data[target]);
        index = target;
    }
}

void heap_push(Heap *h, void *item) {
    if (h->size == h->capacity) {
        h->capacity *= 2;
        h->data = realloc(h->data, h->capacity * sizeof(void *));
    }
    h->data[h->size] = item;
    sift_up(h, h->size);
    h->size++;
}

void *heap_pop(Heap *h) {
    if (h->size == 0) return NULL;
    void *top = h->data[0];
    h->size--;
    h->data[0] = h->data[h->size];
    if (h->size > 0) sift_down(h, 0);
    return top;
}

void *heap_peek(const Heap *h) {
    return h->size > 0 ? h->data[0] : NULL;
}

int heap_size(const Heap *h) { return h->size; }
int heap_is_empty(const Heap *h) { return h->size == 0; }

void heap_build(Heap *h, void **items, int n) {
    while (h->capacity < n) h->capacity *= 2;
    h->data = realloc(h->data, h->capacity * sizeof(void *));
    memcpy(h->data, items, n * sizeof(void *));
    h->size = n;
    for (int i = n / 2 - 1; i >= 0; i--)
        sift_down(h, i);
}

// ======== Comparadores ========

int cmp_int_max(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

int cmp_int_min(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (y > x) - (y < x);
}

int cmp_str_alpha(const void *a, const void *b) {
    return -strcmp((const char *)a, (const char *)b);  // min = primero alfa
}

typedef struct {
    char name[32];
    int priority;
    int arrival;
} Task;

int cmp_task(const void *a, const void *b) {
    const Task *ta = (const Task *)a;
    const Task *tb = (const Task *)b;
    if (ta->priority != tb->priority)
        return ta->priority - tb->priority;
    // misma prioridad: el que llego primero tiene preferencia
    return tb->arrival - ta->arrival;
}

// ======== Demos ========

void demo_int_max_heap(void) {
    printf("=== Max-heap de enteros ===\n\n");

    int values[] = {40, 10, 70, 50, 30, 85, 20, 60};
    int n = sizeof(values) / sizeof(values[0]);

    Heap *h = heap_create(cmp_int_max);

    for (int i = 0; i < n; i++) {
        heap_push(h, &values[i]);
        printf("push(%d) -> peek = %d, size = %d\n",
               values[i], *(int *)heap_peek(h), heap_size(h));
    }

    printf("\nExtraer en orden:\n");
    while (!heap_is_empty(h)) {
        int *val = (int *)heap_pop(h);
        printf("  pop() = %d\n", *val);
    }

    heap_free(h);
}

void demo_int_min_heap(void) {
    printf("\n=== Min-heap de enteros ===\n\n");

    int values[] = {40, 10, 70, 50, 30};
    int n = sizeof(values) / sizeof(values[0]);

    Heap *h = heap_create(cmp_int_min);
    for (int i = 0; i < n; i++)
        heap_push(h, &values[i]);

    printf("Extraer en orden:\n");
    while (!heap_is_empty(h)) {
        int *val = (int *)heap_pop(h);
        printf("  pop() = %d\n", *val);
    }

    heap_free(h);
}

void demo_string_heap(void) {
    printf("\n=== Min-heap de strings (orden alfabetico) ===\n\n");

    const char *words[] = {"mango", "apple", "cherry", "banana", "date"};
    int n = 5;

    Heap *h = heap_create(cmp_str_alpha);
    for (int i = 0; i < n; i++) {
        heap_push(h, (void *)words[i]);
        printf("push(\"%s\")\n", words[i]);
    }

    printf("\nExtraer en orden alfabetico:\n");
    while (!heap_is_empty(h)) {
        const char *w = (const char *)heap_pop(h);
        printf("  pop() = \"%s\"\n", w);
    }

    heap_free(h);
}

void demo_task_queue(void) {
    printf("\n=== Cola de prioridad de tareas ===\n\n");

    Task tasks[] = {
        {"backup",   2, 0},
        {"deploy",   5, 1},
        {"logs",     1, 2},
        {"migrate",  5, 3},
        {"alert",    9, 4},
        {"cleanup",  3, 5},
    };
    int n = sizeof(tasks) / sizeof(tasks[0]);

    Heap *pq = heap_create(cmp_task);
    for (int i = 0; i < n; i++) {
        heap_push(pq, &tasks[i]);
        printf("enqueue: %-10s (prioridad %d)\n", tasks[i].name, tasks[i].priority);
    }

    printf("\nEjecutar por prioridad:\n");
    while (!heap_is_empty(pq)) {
        Task *t = (Task *)heap_pop(pq);
        printf("  ejecutar: %-10s (prioridad %d, llegada %d)\n",
               t->name, t->priority, t->arrival);
    }

    heap_free(pq);
}

void demo_build(void) {
    printf("\n=== Build (heapify) ===\n\n");

    int values[] = {4, 10, 3, 5, 1, 8, 7};
    int n = sizeof(values) / sizeof(values[0]);

    void *ptrs[7];
    for (int i = 0; i < n; i++) ptrs[i] = &values[i];

    Heap *h = heap_create(cmp_int_max);
    heap_build(h, ptrs, n);

    printf("Array original: ");
    for (int i = 0; i < n; i++) printf("%d ", values[i]);
    printf("\n");

    // los valores fueron reordenados in-place via los punteros
    printf("Heap (via pop): ");
    while (!heap_is_empty(h)) {
        int *val = (int *)heap_pop(h);
        printf("%d ", *val);
    }
    printf("\n");

    heap_free(h);
}

void demo_top_k(void) {
    printf("\n=== Top-3 de 10 elementos ===\n\n");

    int values[] = {42, 17, 93, 8, 55, 71, 29, 64, 36, 81};
    int n = 10, k = 3;

    printf("Array: ");
    for (int i = 0; i < n; i++) printf("%d ", values[i]);
    printf("\n");

    // min-heap de tamano k
    Heap *h = heap_create(cmp_int_min);
    for (int i = 0; i < n; i++) {
        if (heap_size(h) < k) {
            heap_push(h, &values[i]);
        } else if (*(int *)heap_peek(h) < values[i]) {
            heap_pop(h);
            heap_push(h, &values[i]);
        }
    }

    printf("Top %d: ", k);
    while (!heap_is_empty(h)) {
        printf("%d ", *(int *)heap_pop(h));
    }
    printf("\n");

    heap_free(h);
}

int main(void) {
    demo_int_max_heap();
    demo_int_min_heap();
    demo_string_heap();
    demo_task_queue();
    demo_build();
    demo_top_k();
    return 0;
}
```

### Salida esperada

```
=== Max-heap de enteros ===

push(40) -> peek = 40, size = 1
push(10) -> peek = 40, size = 2
push(70) -> peek = 70, size = 3
push(50) -> peek = 70, size = 4
push(30) -> peek = 70, size = 5
push(85) -> peek = 85, size = 6
push(20) -> peek = 85, size = 7
push(60) -> peek = 85, size = 8

Extraer en orden:
  pop() = 85
  pop() = 70
  pop() = 60
  pop() = 50
  pop() = 40
  pop() = 30
  pop() = 20
  pop() = 10

=== Min-heap de enteros ===

Extraer en orden:
  pop() = 10
  pop() = 30
  pop() = 40
  pop() = 50
  pop() = 70

=== Min-heap de strings (orden alfabetico) ===

push("mango")
push("apple")
push("cherry")
push("banana")
push("date")

Extraer en orden alfabetico:
  pop() = "apple"
  pop() = "banana"
  pop() = "cherry"
  pop() = "date"
  pop() = "mango"

=== Cola de prioridad de tareas ===

enqueue: backup     (prioridad 2)
enqueue: deploy     (prioridad 5)
enqueue: logs       (prioridad 1)
enqueue: migrate    (prioridad 5)
enqueue: alert      (prioridad 9)
enqueue: cleanup    (prioridad 3)

Ejecutar por prioridad:
  ejecutar: alert      (prioridad 9, llegada 4)
  ejecutar: deploy     (prioridad 5, llegada 1)
  ejecutar: migrate    (prioridad 5, llegada 3)
  ejecutar: cleanup    (prioridad 3, llegada 5)
  ejecutar: backup     (prioridad 2, llegada 0)
  ejecutar: logs       (prioridad 1, llegada 2)

=== Build (heapify) ===

Array original: 4 10 3 5 1 8 7
Heap (via pop): 10 8 7 5 4 3 1

=== Top-3 de 10 elementos ===

Array: 42 17 93 8 55 71 29 64 36 81
Top 3: 71 81 93
```

---

## Análisis de la implementación genérica

### Costo del void *

| Aspecto | int directo | void * genérico |
|---------|------------|----------------|
| Tamaño por elemento | 4 bytes | 8 bytes (puntero) + dato |
| Indirección | Ninguna | 1 dereferencia por comparación |
| Localidad de caché | Excelente | Pobre (datos dispersos) |
| Flexibilidad | Solo int | Cualquier tipo |
| Overhead de malloc | Ninguno | 1 malloc por elemento (si se aloca) |

Para aplicaciones de alto rendimiento con tipos simples, la versión por valor
es preferible. Para aplicaciones donde la flexibilidad importa más (e.g.,
bibliotecas genéricas), `void *` es la opción estándar en C.

### Comparación con `qsort` de stdlib

Nuestra API sigue el mismo patrón que `qsort`:

```c
// qsort
void qsort(void *base, size_t nmemb, size_t size,
            int (*compar)(const void *, const void *));

// nuestro heap
Heap *heap_create(int (*cmp)(const void *, const void *));
```

El comparador tiene la misma firma. Esto permite reusar comparadores entre
`qsort` y nuestro heap.

---

## Errores comunes

### 1. Olvidar que void * no almacena el valor

```c
// INCORRECTO: el puntero apunta a una variable local que cambia
for (int i = 0; i < n; i++) {
    int x = values[i];
    heap_push(h, &x);  // todos apuntan a la misma variable 'x'!
}

// CORRECTO: apuntar directamente al array original
for (int i = 0; i < n; i++) {
    heap_push(h, &values[i]);  // cada puntero apunta a un elemento distinto
}

// CORRECTO: alocar cada valor individualmente
for (int i = 0; i < n; i++) {
    int *p = malloc(sizeof(int));
    *p = values[i];
    heap_push(h, p);
}
```

### 2. Comparador con signo incorrecto

```c
// INCORRECTO para max-heap: retorna negativo cuando a > b
int cmp_wrong(const void *a, const void *b) {
    return *(int *)b - *(int *)a;  // esto es min-heap!
}
```

### 3. No verificar heap vacío antes de pop/peek

```c
// PELIGROSO: puede retornar NULL
int val = *(int *)heap_pop(h);  // si h está vacío, dereferencia NULL

// SEGURO:
if (!heap_is_empty(h)) {
    int val = *(int *)heap_pop(h);
}
```

---

## Ejercicios

### Ejercicio 1 — Max-heap con build

Usa `heap_build` para construir un max-heap desde `{20, 5, 15, 10, 25, 30}`.
Luego extrae todos los elementos. ¿En qué orden salen?

<details>
<summary>Verificar</summary>

Heapify bottom-up de `[20, 5, 15, 10, 25, 30]`:

- i=2 (15): hijos 5 (30). 15 < 30 → swap. `[20, 5, 30, 10, 25, 15]`.
- i=1 (5): hijos 3 (10), 4 (25). Mayor=25. 5 < 25 → swap. `[20, 25, 30, 10, 5, 15]`.
- i=0 (20): hijos 1 (25), 2 (30). Mayor=30. 20 < 30 → swap. `[30, 25, 20, 10, 5, 15]`.
  Continuar: i=2, hijos 5 (15). 20 > 15 → stop.

Heap: `[30, 25, 20, 10, 5, 15]`.

Extraer todo: **30, 25, 20, 15, 10, 5** (descendente).
</details>

### Ejercicio 2 — Comparador para doubles

Escribe un comparador para un min-heap de `double`. Prueba con
`{3.14, 1.41, 2.72, 0.58, 1.73}`.

<details>
<summary>Verificar</summary>

```c
int cmp_double_min(const void *a, const void *b) {
    double x = *(const double *)a;
    double y = *(const double *)b;
    if (y < x) return -1;  // b tiene prioridad (mas chico)
    if (y > x) return 1;
    return 0;
}
```

No usar resta (`y - x`) porque se truncaría a `int` (e.g., 0.5 - 0.3 = 0.2
→ truncado a 0, no detecta la diferencia).

Extracción: **0.58, 1.41, 1.73, 2.72, 3.14**.
</details>

### Ejercicio 3 — Heap de structs con dos criterios

Define un struct `Student { char name[32]; float gpa; int year; }` y un
comparador que ordene por GPA descendente, desempatando por año ascendente
(estudiantes más antiguos primero).

<details>
<summary>Verificar</summary>

```c
typedef struct {
    char name[32];
    float gpa;
    int year;
} Student;

int cmp_student(const void *a, const void *b) {
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;

    // mayor GPA tiene prioridad
    if (sa->gpa > sb->gpa) return 1;
    if (sa->gpa < sb->gpa) return -1;

    // mismo GPA: menor year tiene prioridad (mas antiguo)
    if (sa->year < sb->year) return 1;
    if (sa->year > sb->year) return -1;

    return 0;
}
```

Ejemplo: estudiantes {("Ana", 3.9, 2022), ("Bob", 3.9, 2021), ("Cat", 4.0, 2023)}.
Orden de extracción: Cat (4.0), Bob (3.9 año 2021), Ana (3.9 año 2022).
</details>

### Ejercicio 4 — Top-k con min-heap

Implementa `top_k(int arr[], int n, int k)` que encuentre los $k$ mayores
elementos de un array de $n$ en $O(n \log k)$, usando un min-heap de tamaño $k$.

<details>
<summary>Verificar</summary>

```c
void top_k(int arr[], int n, int k) {
    Heap *h = heap_create(cmp_int_min);

    for (int i = 0; i < n; i++) {
        if (heap_size(h) < k) {
            heap_push(h, &arr[i]);
        } else if (*(int *)heap_peek(h) < arr[i]) {
            heap_pop(h);
            heap_push(h, &arr[i]);
        }
    }

    printf("Top %d: ", k);
    // extraer del min-heap (saldran del menor al mayor)
    int results[k];
    int idx = 0;
    while (!heap_is_empty(h)) {
        results[idx++] = *(int *)heap_pop(h);
    }
    // imprimir en orden descendente
    for (int i = idx - 1; i >= 0; i--) {
        printf("%d ", results[i]);
    }
    printf("\n");

    heap_free(h);
}
```

¿Por qué min-heap y no max-heap? Porque el min-heap de tamaño $k$ mantiene
el menor de los $k$ mayores en la raíz. Cuando llega un nuevo elemento mayor
que la raíz, reemplazamos la raíz (el más pequeño de los top-k). Así siempre
mantenemos los $k$ mayores.

Con max-heap necesitaríamos insertar todo ($O(n \log n)$) y extraer $k$ veces.
Con min-heap de tamaño $k$: $O(n \log k)$, que para $k \ll n$ es mucho mejor.
</details>

### Ejercicio 5 — Merge de k arrays ordenados

Dados $k$ arrays ordenados, combínalos en un solo array ordenado usando un
min-heap de tamaño $k$.

<details>
<summary>Verificar</summary>

```c
typedef struct {
    int value;
    int array_idx;
    int elem_idx;
} HeapEntry;

int cmp_entry(const void *a, const void *b) {
    const HeapEntry *ea = (const HeapEntry *)a;
    const HeapEntry *eb = (const HeapEntry *)b;
    return (eb->value > ea->value) - (eb->value < ea->value);  // min-heap
}

void merge_k_sorted(int **arrays, int *sizes, int k, int *output) {
    Heap *h = heap_create(cmp_entry);
    HeapEntry *entries = malloc(k * sizeof(HeapEntry));

    // insertar primer elemento de cada array
    for (int i = 0; i < k; i++) {
        if (sizes[i] > 0) {
            entries[i] = (HeapEntry){arrays[i][0], i, 0};
            heap_push(h, &entries[i]);
        }
    }

    int out_idx = 0;
    // pool de entries para nuevas inserciones
    int pool_size = 0;
    for (int i = 0; i < k; i++) pool_size += sizes[i];
    HeapEntry *pool = malloc(pool_size * sizeof(HeapEntry));
    int pool_idx = 0;

    while (!heap_is_empty(h)) {
        HeapEntry *min = (HeapEntry *)heap_pop(h);
        output[out_idx++] = min->value;

        int ai = min->array_idx;
        int ei = min->elem_idx + 1;
        if (ei < sizes[ai]) {
            pool[pool_idx] = (HeapEntry){arrays[ai][ei], ai, ei};
            heap_push(h, &pool[pool_idx]);
            pool_idx++;
        }
    }

    free(entries);
    free(pool);
    heap_free(h);
}
```

Complejidad: $O(N \log k)$ donde $N$ es el total de elementos. Cada elemento
entra y sale del heap una vez ($O(\log k)$ cada operación). Esto es el
algoritmo detrás de la fase de merge en merge sort externo.
</details>

### Ejercicio 6 — Mediana en stream

Usa dos heaps (un max-heap para la mitad inferior y un min-heap para la mitad
superior) para calcular la mediana de un stream de números.

<details>
<summary>Verificar</summary>

```c
typedef struct {
    Heap *lower;  // max-heap: mitad inferior
    Heap *upper;  // min-heap: mitad superior
} MedianFinder;

MedianFinder *mf_create(void) {
    MedianFinder *mf = malloc(sizeof(MedianFinder));
    mf->lower = heap_create(cmp_int_max);
    mf->upper = heap_create(cmp_int_min);
    return mf;
}

void mf_add(MedianFinder *mf, int *val) {
    if (heap_is_empty(mf->lower) || *val <= *(int *)heap_peek(mf->lower)) {
        heap_push(mf->lower, val);
    } else {
        heap_push(mf->upper, val);
    }

    // balancear: lower puede tener a lo sumo 1 mas que upper
    if (heap_size(mf->lower) > heap_size(mf->upper) + 1) {
        heap_push(mf->upper, heap_pop(mf->lower));
    } else if (heap_size(mf->upper) > heap_size(mf->lower)) {
        heap_push(mf->lower, heap_pop(mf->upper));
    }
}

double mf_median(MedianFinder *mf) {
    if (heap_size(mf->lower) > heap_size(mf->upper)) {
        return *(int *)heap_peek(mf->lower);
    }
    return (*(int *)heap_peek(mf->lower) + *(int *)heap_peek(mf->upper)) / 2.0;
}
```

Ejemplo: stream 5, 2, 8, 1, 9.
- add(5): lower=[5], upper=[]. Mediana=5.
- add(2): lower=[5,2], upper=[]. Rebalancear: lower=[2], upper=[5]. Mediana=3.5.
- add(8): lower=[2], upper=[5,8]. Rebalancear: lower=[5,2], upper=[8]. Mediana=5.
- add(1): lower=[5,2,1], upper=[8]. Rebalancear: lower=[2,1], upper=[5,8]. Mediana=3.5.
- add(9): lower=[2,1], upper=[5,8,9]. Rebalancear: lower=[5,2,1], upper=[8,9]. Mediana=5.

Complejidad: $O(\log n)$ por inserción, $O(1)$ para consultar la mediana.
</details>

### Ejercicio 7 — heap_delete genérico

Implementa `heap_delete(Heap *h, int index)` que elimine el elemento en una
posición arbitraria.

<details>
<summary>Verificar</summary>

```c
void *heap_delete(Heap *h, int index) {
    if (index < 0 || index >= h->size) return NULL;

    void *removed = h->data[index];
    h->size--;
    h->data[index] = h->data[h->size];

    if (index < h->size) {
        // puede necesitar subir o bajar
        sift_up(h, index);
        sift_down(h, index);
    }

    return removed;
}
```

Ambos sift son seguros de llamar: si el nuevo valor es mayor que el padre,
sift-up lo sube; si es menor que un hijo, sift-down lo baja; si está en su
lugar, ninguno hace nada.

Complejidad: $O(\log n)$.

Nota: encontrar **qué** índice tiene un valor particular requiere búsqueda
lineal $O(n)$. Para soportar `delete_by_value` eficientemente, se necesita
un mapa auxiliar valor→índice (usado en decrease-key para Dijkstra).
</details>

### Ejercicio 8 — Comparar rendimiento lista vs heap

Escribe un programa que mida el tiempo de $n$ operaciones `push` + `pop`
alternadas, comparando un heap vs una lista ordenada, para $n = 10^4$ y $10^5$.

<details>
<summary>Verificar</summary>

```c
#include <time.h>

// Lista ordenada (insercion O(n) por busqueda binaria + shift)
void sorted_insert(int arr[], int *size, int val) {
    int i = *size - 1;
    while (i >= 0 && arr[i] > val) {
        arr[i + 1] = arr[i];
        i--;
    }
    arr[i + 1] = val;
    (*size)++;
}

int sorted_pop(int arr[], int *size) {
    (*size)--;
    return arr[*size];  // max esta al final en lista ascendente
}

int main(void) {
    for (int n = 10000; n <= 100000; n *= 10) {
        // Heap
        clock_t start = clock();
        IntHeap *h = int_heap_create();
        for (int i = 0; i < n; i++) {
            int_heap_push(h, rand());
            if (i % 2 == 1) int_heap_pop(h);
        }
        while (h->size > 0) int_heap_pop(h);
        double heap_time = (double)(clock() - start) / CLOCKS_PER_SEC;

        // Lista ordenada
        start = clock();
        int *list = malloc(n * sizeof(int));
        int list_size = 0;
        for (int i = 0; i < n; i++) {
            sorted_insert(list, &list_size, rand());
            if (i % 2 == 1) sorted_pop(list, &list_size);
        }
        double list_time = (double)(clock() - start) / CLOCKS_PER_SEC;

        printf("n=%6d: heap=%.4fs, lista=%.4fs, ratio=%.1fx\n",
               n, heap_time, list_time, list_time / heap_time);
        free(list);
    }
}
```

Resultado típico:

```
n= 10000: heap=0.0005s, lista=0.0120s, ratio=24.0x
n=100000: heap=0.0060s, lista=1.2000s, ratio=200.0x
```

El heap es ~200× más rápido para $n = 10^5$. La lista tiene inserción
$O(n)$ que domina el costo; el heap tiene $O(\log n)$ para ambas operaciones.
</details>

### Ejercicio 9 — Destructor personalizado

Extiende la implementación del heap para soportar un **destructor** que libere
los datos al hacer `heap_free`.

<details>
<summary>Verificar</summary>

```c
typedef void (*FreeFunc)(void *);

typedef struct {
    void **data;
    int size;
    int capacity;
    CmpFunc cmp;
    FreeFunc free_item;  // NULL si no se necesita
} HeapEx;

void heapex_free(HeapEx *h) {
    if (h->free_item) {
        for (int i = 0; i < h->size; i++) {
            h->free_item(h->data[i]);
        }
    }
    free(h->data);
    free(h);
}

// Uso con datos alocados:
HeapEx *h = heapex_create(cmp_int_max, free);  // free() como destructor
for (int i = 0; i < n; i++) {
    int *p = malloc(sizeof(int));
    *p = values[i];
    heapex_push(h, p);
}
heapex_free(h);  // libera cada int* y luego el array y el struct
```

Patrón común en C para contenedores genéricos con ownership. En Rust, `Drop`
maneja esto automáticamente.
</details>

### Ejercicio 10 — Decrease-key

Implementa `heap_decrease_key(Heap *h, int index, void *new_value)` para un
min-heap. Esta operación es esencial para el algoritmo de Dijkstra.

<details>
<summary>Verificar</summary>

```c
// Para min-heap: decrease_key reduce el valor y sube el nodo
void heap_decrease_key(Heap *h, int index, void *new_value) {
    // el nuevo valor debe ser menor (para min-heap)
    // si el comparador retorna > 0 cuando a tiene mas prioridad que b
    // (en min-heap, menor = mas prioridad), verificar:
    if (h->cmp(new_value, h->data[index]) <= 0) {
        return;  // nuevo valor no es mejor, ignorar
    }

    h->data[index] = new_value;
    sift_up(h, index);
}
```

Para usarlo eficientemente en Dijkstra, necesitamos un mapa auxiliar que
indique en qué índice del heap está cada nodo del grafo:

```c
int position[MAX_NODES];  // position[node_id] = indice en el heap

// actualizar positions durante cada swap en sift_up/sift_down
```

Esto transforma el heap en un **indexed priority queue** — un tema avanzado
que se profundiza en C11 (grafos). Complejidad: $O(\log n)$ para decrease-key.

Sin la posición conocida, buscar el nodo para decrease-key sería $O(n)$,
anulando la ventaja del heap.
</details>
