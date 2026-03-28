# T04 — Punteros colgantes y wild pointers

## Dangling pointer (puntero colgante)

Un dangling pointer apunta a memoria que **ya fue liberada** o
que **ya no es válida**. Desreferenciarlo es UB:

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *p = malloc(sizeof(int));
    *p = 42;

    free(p);
    // p sigue conteniendo la dirección anterior.
    // Pero esa memoria ya no es nuestra.

    printf("%d\n", *p);    // UB — use-after-free
    *p = 100;              // UB — write-after-free

    return 0;
}
```

### Causa 1: free sin NULL

```c
int *p = malloc(sizeof(int));
*p = 42;
free(p);

// p tiene la misma dirección pero la memoria es inválida:
printf("%p\n", (void *)p);    // imprime la dirección (no crashea)
*p = 100;                      // UB — la memoria puede haber sido
                                // reasignada a otro malloc

// SOLUCIÓN: poner a NULL después de free:
free(p);
p = NULL;

// Ahora desreferenciar da segfault (detecta el bug):
// *p = 100;    // segfault — detectable
```

### Causa 2: retornar dirección de variable local

```c
int *bad_function(void) {
    int x = 42;
    return &x;    // WARNING: returning address of local variable
}

int main(void) {
    int *p = bad_function();
    // x fue destruida al salir de bad_function.
    // p apunta a un frame de stack que ya no existe.

    printf("%d\n", *p);    // UB — puede imprimir 42, basura, o crashear
    // Llamar otra función puede sobrescribir ese espacio del stack.

    return 0;
}
```

```c
// El mismo problema con arrays:
char *bad_string(void) {
    char buf[100] = "hello";
    return buf;    // WARNING — buf es local
}

// SOLUCIONES:

// 1. Retornar un string literal (está en .rodata, siempre válido):
const char *good_string(void) {
    return "hello";    // OK — string literal tiene lifetime estático
}

// 2. Alocar en el heap (el caller hace free):
char *good_string2(void) {
    return strdup("hello");    // OK — alocado en heap
}

// 3. Recibir el buffer del caller:
void good_string3(char *buf, size_t size) {
    snprintf(buf, size, "hello");
}

// 4. Usar static (cuidado: no es thread-safe):
const char *good_string4(void) {
    static char buf[100] = "hello";
    return buf;    // OK — static tiene lifetime del programa
}
```

### Causa 3: puntero a scope que terminó

```c
int *p;
{
    int x = 42;
    p = &x;
}
// x ya no existe — p es dangling
printf("%d\n", *p);    // UB

// Menos obvio con loops:
int *ptrs[10];
for (int i = 0; i < 10; i++) {
    int val = i * 10;
    ptrs[i] = &val;    // TODOS apuntan a la misma variable local
}
// Después del loop: todas las entradas son dangling.
// Incluso durante el loop, ptrs[0..i-1] apuntan a val
// que se sobrescribe cada iteración.
```

### Causa 4: realloc

```c
int *p = malloc(10 * sizeof(int));
int *alias = p;    // alias apunta a la misma memoria

p = realloc(p, 100 * sizeof(int));
// realloc puede mover el bloque a otra dirección.
// Si lo movió: alias es dangling.
// Si no lo movió: alias sigue siendo válido.
// NO se puede saber — alias es potencialmente dangling.

// SOLUCIÓN: no mantener alias a memoria que puede hacer realloc:
// Usar solo p después de realloc.
// O actualizar todos los alias.
```

## Wild pointer (puntero salvaje)

Un wild pointer es un puntero que **nunca fue inicializado**.
Contiene un valor indeterminado — puede apuntar a cualquier lugar:

```c
int *p;          // wild pointer — valor basura
*p = 42;         // UB — escribe en dirección aleatoria
// Puede crashear, corromper datos silenciosamente,
// o parecer funcionar (el peor caso).

// SOLUCIÓN: SIEMPRE inicializar:
int *p = NULL;           // si no tienes dirección aún
int *p = malloc(...);    // si vas a alocar
int *p = &x;             // si sabes a dónde apuntar
```

```c
// Variables locales NO se inicializan en C:
void foo(void) {
    int *p;       // indeterminado
    int x;        // indeterminado
    int arr[10];  // todos los elementos indeterminados

    // Variables static/global SÍ se inicializan a 0:
    static int *sp;   // NULL
    static int sy;    // 0
}
```

## Detección

### En compilación

```c
// -Wall -Wextra detectan algunos casos:

// Retornar local:
int *foo(void) {
    int x = 42;
    return &x;
    // warning: function returns address of local variable
}

// Variable no inicializada:
int *p;
if (*p) {}
// warning: 'p' is used uninitialized
// (no siempre lo detecta — depende del flujo)
```

### En runtime: ASan

```c
// gcc -fsanitize=address -g prog.c

// Detecta use-after-free:
int *p = malloc(sizeof(int));
free(p);
*p = 42;
// ERROR: AddressSanitizer: heap-use-after-free on address 0x...
// WRITE of size 4 at 0x...
// freed by thread T0 here:
//     #0 ... in free
//     #1 ... in main prog.c:3

// Detecta stack-use-after-return (con flag especial):
// ASAN_OPTIONS=detect_stack_use_after_return=1 ./prog
```

### En runtime: Valgrind

```c
// valgrind ./prog

// Detecta use-after-free:
// Invalid write of size 4
//   at 0x...: main (prog.c:4)
// Address 0x... is 0 bytes inside a block of size 4 free'd
//   at 0x...: free
//   at 0x...: main (prog.c:3)

// Detecta lectura de memoria no inicializada:
// Conditional jump or move depends on uninitialised value(s)
//   at 0x...: main (prog.c:3)
```

## Prevención

### 1. Inicializar siempre

```c
// Declarar e inicializar en la misma línea:
int *p = NULL;
int *data = malloc(n * sizeof(int));
int *elem = &array[0];

// Si declaras sin inicializar, inicializa lo antes posible:
int *p;
// ... código que calcula la dirección ...
p = get_address();
```

### 2. NULL después de free

```c
free(p);
p = NULL;

// Patrón: macro SAFE_FREE
#define SAFE_FREE(p) do { free(p); (p) = NULL; } while (0)

int *data = malloc(100);
SAFE_FREE(data);    // free + NULL
// data es NULL ahora — desreferenciar da segfault (detecta bug)

// free(NULL) es seguro (no-op), así que SAFE_FREE es idempotente:
SAFE_FREE(data);    // OK — free(NULL) no hace nada
```

### 3. No retornar direcciones locales

```c
// SIEMPRE verificar: ¿la variable sigue viva cuando
// el caller usa el puntero?

// MAL:
int *create(void) {
    int arr[10] = {0};
    return arr;           // arr muere al retornar
}

// BIEN — heap:
int *create(int n) {
    int *arr = calloc(n, sizeof(int));
    return arr;           // vive hasta free()
}

// BIEN — caller provee buffer:
void create(int *buf, int n) {
    for (int i = 0; i < n; i++) buf[i] = 0;
}
```

### 4. Limitar el scope de punteros

```c
// Cuanto menor sea el scope del puntero, menor la probabilidad
// de que se vuelva dangling:

// MAL — p vive demasiado:
int *p = malloc(sizeof(int));
*p = 42;
process(*p);
free(p);
// ... 200 líneas más de código ...
// ¿p sigue siendo válido? Difícil de saber.

// BIEN — scope limitado:
{
    int *p = malloc(sizeof(int));
    *p = 42;
    process(*p);
    free(p);
}
// p no existe fuera de las llaves
```

### 5. Ownership claro

```c
// Documentar quién es "dueño" del puntero (quién hace free):

// El caller es dueño — la función aloca, el caller libera:
// @return: caller must free() the returned pointer
char *read_file(const char *path);

// La función es dueña — el caller no debe liberar:
// @return: pointer to internal buffer, do not free
const char *get_name(int id);

// Transferencia de ownership:
// @param data: this function takes ownership, caller must not free
void process_and_free(int *data);

// En C no hay mecanismo del lenguaje para esto — solo disciplina.
// Rust resuelve esto con el sistema de ownership en el compilador.
```

## Comparación con Rust

```c
// En C:
int *p = malloc(sizeof(int));
*p = 42;
free(p);
*p = 100;    // UB — el compilador no dice nada

// En Rust:
// let p = Box::new(42);
// drop(p);
// *p = 100;    // ERROR DE COMPILACIÓN: use of moved value
// El borrow checker impide use-after-free en compilación.

// C requiere disciplina del programador + herramientas externas.
// Rust lo previene por diseño del lenguaje.
```

---

## Ejercicios

### Ejercicio 1 — Identificar bugs

```c
// Encontrar y corregir los dangling pointers en este código:
char *get_greeting(const char *name) {
    char buf[100];
    snprintf(buf, sizeof(buf), "Hello, %s!", name);
    return buf;
}

int *create_sequence(int n) {
    int arr[n];
    for (int i = 0; i < n; i++) arr[i] = i + 1;
    return arr;
}

void example(void) {
    int *p = malloc(sizeof(int));
    int *q = p;
    *p = 42;
    free(p);
    printf("%d\n", *q);
}
```

### Ejercicio 2 — SAFE_FREE

```c
// Implementar el macro SAFE_FREE(p) que hace free y pone a NULL.
// Crear un programa que aloque un array, lo use, lo libere
// con SAFE_FREE, y luego intente usarlo.
// Verificar que el acceso después de SAFE_FREE causa segfault
// (en vez de use-after-free silencioso).
```

### Ejercicio 3 — Detectar con herramientas

```c
// Crear un programa con:
// 1. Un use-after-free
// 2. Un dangling pointer a variable local
// 3. Un wild pointer
// Compilar con -fsanitize=address y con valgrind.
// Documentar qué detecta cada herramienta.
```
