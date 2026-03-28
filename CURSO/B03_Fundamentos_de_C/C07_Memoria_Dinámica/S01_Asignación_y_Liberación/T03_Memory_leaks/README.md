# T03 — Memory leaks

## Qué es un memory leak

Un memory leak ocurre cuando se aloca memoria con malloc
pero **nunca se libera con free**. La memoria queda inaccesible
pero sigue reservada hasta que el proceso termina:

```c
#include <stdlib.h>

void leak_example(void) {
    int *p = malloc(100 * sizeof(int));
    // ... usar p ...
    // return sin free(p) → LEAK
}

int main(void) {
    for (int i = 0; i < 1000; i++) {
        leak_example();    // cada llamada pierde 400 bytes
    }
    // 400,000 bytes perdidos → nunca se pueden recuperar
    // (hasta que el proceso termina)
    return 0;
}
```

```c
// Un leak ocurre cuando se pierde la ÚLTIMA referencia
// a la memoria alocada:

// 1. Sobreescribir el puntero:
int *p = malloc(100);
p = malloc(200);           // LEAK — los primeros 100 bytes se perdieron

// 2. Salir del scope sin free:
void foo(void) {
    int *p = malloc(100);
}                           // LEAK — p se destruye, memoria queda

// 3. Return temprano:
int bar(void) {
    int *data = malloc(1000);
    if (error) return -1;   // LEAK — data no se liberó
    // ...
    free(data);
    return 0;
}
```

## Categorías de leaks (terminología Valgrind)

```c
// Valgrind clasifica los leaks en 4 categorías:

// 1. Definitely lost — sin ninguna referencia:
void definitely_lost(void) {
    int *p = malloc(100);
    p = NULL;    // la única referencia se perdió
}
// Severidad: ALTA — es un bug seguro.

// 2. Indirectly lost — accesible solo a través de otro leak:
void indirectly_lost(void) {
    struct Node *n = malloc(sizeof(struct Node));
    n->data = malloc(100);    // indirectly lost
    n = NULL;                  // n es definitely lost
    // n->data es indirectly lost (se accedería a través de n)
}
// Severidad: se resuelve al arreglar el definitely lost.

// 3. Possibly lost — referencia ambigua:
void possibly_lost(void) {
    int *p = malloc(100 * sizeof(int));
    p += 50;    // p apunta al MEDIO del bloque
    // ¿Es un leak o un puntero interior intencional?
}
// Severidad: usualmente es un bug.

// 4. Still reachable — aún accesible al terminar:
static int *global_data;
void still_reachable(void) {
    global_data = malloc(100);
    // Se puede acceder a global_data al terminar,
    // pero nunca se hizo free.
}
// Severidad: generalmente no es un bug.
// Es memoria que se usa durante toda la vida del programa.
// El SO la libera al terminar. Algunos la consideran aceptable.
```

## Detección con Valgrind

```bash
# Compilar con debug info:
gcc -g -O0 prog.c -o prog

# Ejecutar con Valgrind:
valgrind --leak-check=full --show-leak-kinds=all ./prog
```

```
# Salida de Valgrind para un leak:
==12345== 400 bytes in 1 blocks are definitely lost in loss record 1 of 1
==12345==    at 0x4C2AB80: malloc (vg_replace_malloc.c:299)
==12345==    at 0x40052E: leak_example (prog.c:4)
==12345==    at 0x400547: main (prog.c:10)
==12345==
==12345== LEAK SUMMARY:
==12345==    definitely lost: 400 bytes in 1 blocks
==12345==    indirectly lost: 0 bytes in 0 blocks
==12345==      possibly lost: 0 bytes in 0 blocks
==12345==    still reachable: 0 bytes in 0 blocks
==12345==         suppressed: 0 bytes in 0 blocks
```

```bash
# Opciones útiles:
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         ./prog

# --leak-check=full: detalles de cada leak
# --show-leak-kinds=all: incluir still reachable
# --track-origins=yes: rastrear origen de valores no inicializados
```

## Detección con ASan

```bash
# ASan detecta leaks al terminar el programa:
gcc -fsanitize=address -g prog.c -o prog
./prog
```

```
# Salida de ASan:
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 400 byte(s) in 1 object(s) allocated from:
    #0 0x7f... in malloc
    #1 0x40... in leak_example prog.c:4
    #2 0x40... in main prog.c:10

SUMMARY: AddressSanitizer: 400 byte(s) leaked in 1 allocation(s).
```

```bash
# Controlar el leak sanitizer:
ASAN_OPTIONS=detect_leaks=0 ./prog    # desactivar detección de leaks
ASAN_OPTIONS=detect_leaks=1 ./prog    # activar (predeterminado en Linux)
```

## Patrones de leak y cómo evitarlos

### 1. Return temprano

```c
// PROBLEMA:
int process(const char *path) {
    FILE *f = fopen(path, "r");
    char *buf = malloc(1024);

    if (!f) return -1;          // LEAK — buf no se liberó
    if (!buf) { fclose(f); return -1; }

    // ... procesar ...

    if (error) return -1;       // LEAK — buf y f no se liberaron

    free(buf);
    fclose(f);
    return 0;
}

// SOLUCIÓN — cleanup con goto:
int process(const char *path) {
    int result = -1;
    FILE *f = NULL;
    char *buf = NULL;

    f = fopen(path, "r");
    if (!f) goto cleanup;

    buf = malloc(1024);
    if (!buf) goto cleanup;

    // ... procesar ...
    if (error) goto cleanup;

    result = 0;

cleanup:
    free(buf);       // free(NULL) es seguro
    if (f) fclose(f);
    return result;
}
```

### 2. Olvidar liberar miembros de struct

```c
// PROBLEMA:
struct Person {
    char *name;
    char *email;
    int *scores;
};

void person_free_wrong(struct Person *p) {
    free(p);    // LEAK — name, email, scores no se liberaron
}

// SOLUCIÓN — liberar miembros primero:
void person_free(struct Person *p) {
    if (!p) return;
    free(p->name);
    free(p->email);
    free(p->scores);
    free(p);
}
```

### 3. Perder referencia en realloc

```c
// PROBLEMA:
char **lines = malloc(10 * sizeof(char *));
// ... llenar lines ...
lines = realloc(lines, 20 * sizeof(char *));
// Si realloc falla: lines = NULL → todas las líneas se perdieron

// SOLUCIÓN:
char **tmp = realloc(lines, 20 * sizeof(char *));
if (!tmp) {
    // lines sigue válido — manejar el error
    for (int i = 0; i < 10; i++) free(lines[i]);
    free(lines);
    return -1;
}
lines = tmp;
```

### 4. Leak en estructuras recursivas

```c
// PROBLEMA — liberar lista enlazada:
struct Node {
    int value;
    struct Node *next;
};

void free_list_wrong(struct Node *head) {
    free(head);    // LEAK — todos los nodos siguientes se perdieron
}

// SOLUCIÓN:
void free_list(struct Node *head) {
    while (head) {
        struct Node *next = head->next;    // guardar next ANTES de free
        free(head);
        head = next;
    }
}

// ERROR SUTIL:
void free_list_bad(struct Node *head) {
    while (head) {
        free(head);
        head = head->next;    // UB — head ya fue liberado
    }
}
```

## Patrones de ownership en C

```c
// En C, no hay ownership automático. Convenciones claras evitan leaks:

// Patrón 1: "el que aloca, libera"
char *data = malloc(100);
// ... usar data ...
free(data);

// Patrón 2: "constructor/destructor"
struct Obj *obj_create(void);    // aloca
void obj_destroy(struct Obj *);  // libera

// Patrón 3: documentar ownership
// @return: caller must free() the returned pointer
char *read_file(const char *path);

// @param data: this function takes ownership (will free)
void process_data(int *data);

// Patrón 4: init/deinit para structs en stack
struct Buffer buf;
buffer_init(&buf, 1024);
// ... usar buf ...
buffer_deinit(&buf);    // libera memoria interna
```

## Cuándo un leak es aceptable

```c
// 1. Memoria que vive toda la vida del programa:
int main(void) {
    char *config = load_config();
    // config se usa durante todo el programa
    // No hacer free — el SO libera todo al terminar.
    // Valgrind reporta "still reachable" — no es un bug.
    return 0;
}

// 2. Programas de corta duración:
// Un programa que se ejecuta, imprime un resultado y termina
// no necesita free — el SO limpia todo.
// Pero: es mala práctica. Valgrind reportará leaks.
// Mejor: siempre hacer free para tener el hábito.

// 3. NUNCA aceptable:
// Leaks en loops, funciones llamadas repetidamente,
// o servidores de larga duración.
// Estos leaks crecen sin límite → OOM eventual.
```

---

## Ejercicios

### Ejercicio 1 — Encontrar leaks

```c
// Este programa tiene 3 memory leaks. Encontrarlos y corregirlos:
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *duplicate(const char *s) {
    char *copy = malloc(strlen(s) + 1);
    strcpy(copy, s);
    return copy;
}

int main(void) {
    char *a = duplicate("hello");
    char *b = duplicate("world");
    a = duplicate("new hello");

    char **arr = malloc(3 * sizeof(char *));
    arr[0] = duplicate("one");
    arr[1] = duplicate("two");
    arr[2] = duplicate("three");
    free(arr);

    printf("%s %s\n", a, b);
    free(a);
    return 0;
}
// Compilar con -g, ejecutar con valgrind --leak-check=full.
```

### Ejercicio 2 — Cleanup con goto

```c
// Reescribir esta función usando el patrón goto cleanup
// para que no tenga leaks en ningún path de error:
int process(const char *input_path, const char *output_path) {
    FILE *in = fopen(input_path, "r");
    if (!in) return -1;

    FILE *out = fopen(output_path, "w");
    if (!out) return -1;    // LEAK: in

    char *buf = malloc(4096);
    if (!buf) return -1;     // LEAK: in, out

    // ... procesar ...
    if (error_condition) return -1;    // LEAK: in, out, buf

    free(buf);
    fclose(out);
    fclose(in);
    return 0;
}
```

### Ejercicio 3 — Constructor/destructor

```c
// Crear un struct StringList con:
// - string_list_create() → aloca la lista
// - string_list_add(list, str) → agrega una copia del string
// - string_list_print(list) → imprime todos los strings
// - string_list_destroy(list) → libera todo
// Verificar con valgrind que no hay leaks.
```
