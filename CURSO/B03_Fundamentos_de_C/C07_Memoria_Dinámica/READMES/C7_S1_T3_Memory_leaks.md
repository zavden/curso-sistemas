# T03 — Memory leaks

## Erratas detectadas en el material base

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| `labs/README.md` líneas 266-268 | Dice que `student_free_wrong` causa que `name` y `grades` se pierdan como "indirectly lost" | Es incorrecto. `student_free_wrong` hace `free(s)`, lo cual **libera** el struct. Tras `free(s)`, el bloque que contenía los punteros a `name` y `grades` ya no existe en el conjunto de bloques alocados — Valgrind no lo escanea como fuente de punteros. Por tanto, `name` y `grades` quedan sin ningún puntero accesible → son **"definitely lost"**, no "indirectly lost". Serían "indirectly lost" solo si el struct hubiera sido **leaked** (nunca freed), no **freed** |

---

## 1. Qué es un memory leak

Un memory leak ocurre cuando se aloca memoria con `malloc` (o `calloc`/`realloc`) y se pierde la **última referencia** a ella sin hacer `free`. La memoria queda reservada pero inaccesible — ni el programa puede usarla ni el SO puede reclamarla (hasta que el proceso termine):

```c
void leak_example(void) {
    int *p = malloc(100 * sizeof(int));
    // ... usar p ...
    // return sin free(p) → LEAK
    // p se destruye (era una variable local), pero la memoria sigue alocada
}

int main(void) {
    for (int i = 0; i < 1000; i++) {
        leak_example();  // Cada llamada pierde 400 bytes
    }
    // 400,000 bytes perdidos — nunca se pueden recuperar
    return 0;
}
```

Los leaks son **silenciosos**: el programa no falla, no hay segfault, no hay mensaje de error. El único síntoma visible es que el consumo de memoria del proceso crece continuamente (observable con `top`, `htop`, o `/proc/self/status`).

### La diferencia con otros bugs de memoria

| Bug | Síntoma | Detección |
|-----|---------|-----------|
| Use-after-free | Crash o corrupción | ASan, Valgrind |
| Double free | Crash (abort) | ASan, Valgrind |
| Buffer overflow | Corrupción silenciosa | ASan, Valgrind |
| **Memory leak** | **Ninguno inmediato** | **Solo herramientas** |

Los leaks son los más difíciles de notar porque el programa "funciona bien" — hasta que se queda sin memoria.

---

## 2. Cómo se pierde la referencia: los tres patrones clásicos

### Patrón 1: olvidar free antes de return

```c
void simple_leak(void) {
    int *data = malloc(10 * sizeof(int));
    if (!data) return;
    for (int i = 0; i < 10; i++) {
        data[i] = i * i;
    }
    printf("Computed %d squares\n", 10);
    // Falta: free(data);
}
```

### Patrón 2: sobreescribir el puntero

```c
void reassignment_leak(void) {
    char *msg = malloc(32);
    if (!msg) return;
    strcpy(msg, "first message");

    msg = malloc(64);   // LEAK: los primeros 32 bytes se perdieron
    // msg ahora apunta al nuevo bloque — no hay forma de hacer
    // free del bloque original de 32 bytes
    if (!msg) return;
    strcpy(msg, "second message");
    free(msg);          // Solo libera el segundo bloque
}
```

### Patrón 3: return temprano en path de error

```c
int error_path_leak(int trigger_error) {
    char *buffer = malloc(256);
    if (!buffer) return -1;
    strcpy(buffer, "processing data...");

    if (trigger_error) {
        return -1;      // LEAK: buffer no se liberó
    }

    free(buffer);
    return 0;
}
```

Este es el patrón más insidioso porque el happy path (sin error) sí hace `free`. El leak solo ocurre en los caminos de error, que se prueban con menos frecuencia.

---

## 3. Categorías de Valgrind

Valgrind clasifica los leaks según el estado de los punteros al terminar el programa:

### Definitely lost

No existe **ningún** puntero en memoria accesible que apunte al bloque:

```c
void definitely_lost(void) {
    int *data = malloc(64);
    data = NULL;  // Única referencia destruida → definitely lost
}
```

Severidad: **alta** — es un bug seguro.

### Indirectly lost

El bloque es accesible **solo** a través de otro bloque que es "definitely lost":

```c
void indirectly_lost(void) {
    struct Record *rec = malloc(sizeof(struct Record));
    rec->name = malloc(32);      // indirectly lost
    rec->scores = malloc(20);    // indirectly lost
    // rec se pierde al salir del scope → definitely lost
    // rec->name y rec->scores son indirectly lost:
    //   existirían punteros a ellos... dentro de rec... que está perdido
}
```

Severidad: se resuelve **automáticamente** al arreglar el "definitely lost" del que dependen.

**Nota importante**: "indirectly lost" solo aplica cuando el bloque contenedor fue **leaked** (nunca freed). Si el contenedor fue **freed** (como en `student_free_wrong` que hace `free(s)` sin liberar miembros), los miembros internos pasan a ser "definitely lost", porque tras el `free` del struct, los punteros que había en él desaparecen del conjunto de memoria accesible.

### Possibly lost

Existe un puntero que apunta al **interior** del bloque, pero no a su inicio:

```c
void possibly_lost(void) {
    int *p = malloc(100 * sizeof(int));
    p += 50;    // p apunta al medio del bloque
    // El inicio del bloque no tiene ningún puntero apuntándole
    // pero p apunta a una dirección interior → ¿leak o intencional?
}
```

Severidad: usualmente es un bug.

### Still reachable

El bloque tiene un puntero accesible (típicamente global o estático) que apunta a su inicio, pero nunca se hizo `free`:

```c
static char *global_config = NULL;

void still_reachable(void) {
    global_config = malloc(128);
    strcpy(global_config, "log_level=debug");
    // global_config persiste toda la vida del programa
    // Al terminar, el puntero sigue accesible pero nunca se hizo free
}
```

Severidad: **generalmente no es un bug**. Es memoria usada durante toda la vida del programa. El SO la libera al terminar.

---

## 4. Detección con Valgrind

### Compilación y ejecución

```bash
# Compilar con debug info (INDISPENSABLE para stack traces legibles):
gcc -g -O0 prog.c -o prog

# Ejecutar con Valgrind:
valgrind --leak-check=full --show-leak-kinds=all ./prog
```

`-g` genera la tabla de símbolos (mapeo dirección → archivo:línea). Sin `-g`, Valgrind solo muestra direcciones hexadecimales. `-O0` desactiva optimizaciones para que las líneas correspondan al código fuente.

### Interpretando la salida

```
==12345== 40 bytes in 1 blocks are definitely lost in loss record 2 of 3
==12345==    at 0x4C2AB80: malloc (vg_replace_malloc.c:299)
==12345==    by 0x40052E: simple_leak (prog.c:8)
==12345==    by 0x400547: main (prog.c:49)
```

El stack trace se lee de **abajo hacia arriba**: `main` (línea 49) llamó a `simple_leak` (línea 8), que llamó a `malloc`. Esos 40 bytes nunca se liberaron.

### LEAK SUMMARY

```
LEAK SUMMARY:
   definitely lost: 400 bytes in 3 blocks
   indirectly lost: 0 bytes in 0 blocks
     possibly lost: 0 bytes in 0 blocks
   still reachable: 0 bytes in 0 blocks
        suppressed: 0 bytes in 0 blocks
```

El objetivo es alcanzar:

```
All heap blocks were freed -- no leaks are possible
```

### Opciones útiles

```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \    # Incluir still reachable
         --track-origins=yes \      # Rastrear valores no inicializados
         --verbose \
         ./prog
```

---

## 5. Detección con ASan (LeakSanitizer)

ASan incluye **LeakSanitizer** (LSan), que detecta leaks al terminar el programa. Es más rápido que Valgrind (~2× overhead vs ~20×):

```bash
gcc -fsanitize=address -g prog.c -o prog
./prog
```

```
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 400 byte(s) in 1 object(s) allocated from:
    #0 0x7f... in malloc (/usr/lib/libasan.so...)
    #1 0x40... in leak_example prog.c:4
    #2 0x40... in main prog.c:10

SUMMARY: AddressSanitizer: 400 byte(s) leaked in 1 allocation(s).
```

### Diferencias con Valgrind

| Característica | Valgrind | ASan/LSan |
|---------------|----------|-----------|
| Overhead | ~20× | ~2× |
| Categorías | 4 (definitely, indirectly, possibly, still reachable) | 2 (direct, indirect) |
| Detecta en stack | Sí | Sí |
| Instalación | Programa externo | Flag del compilador |
| Recompilación | No necesaria | Sí (`-fsanitize=address`) |

### Control del leak sanitizer

```bash
# Desactivar detección de leaks (útil si solo quieres ASan para otros bugs):
ASAN_OPTIONS=detect_leaks=0 ./prog

# Solo detectar leaks (sin ASan completo):
gcc -fsanitize=leak -g prog.c -o prog
```

---

## 6. Patrón goto cleanup

El patrón `goto cleanup` centraliza la liberación de recursos en un **único punto de salida**, eliminando leaks en paths de error:

```c
// ANTES: múltiples returns, múltiples leaks potenciales
int process(const char *in_path, const char *out_path) {
    FILE *in = fopen(in_path, "r");
    if (!in) return -1;

    FILE *out = fopen(out_path, "w");
    if (!out) return -1;           // LEAK: in

    char *buf = malloc(4096);
    if (!buf) return -1;           // LEAK: in, out

    if (ferror(in)) return -1;     // LEAK: in, out, buf

    free(buf);
    fclose(out);
    fclose(in);
    return 0;
}

// DESPUÉS: un solo punto de salida con goto cleanup
int process(const char *in_path, const char *out_path) {
    int result = -1;
    FILE *in = NULL;
    FILE *out = NULL;
    char *buf = NULL;

    in = fopen(in_path, "r");
    if (!in) goto cleanup;

    out = fopen(out_path, "w");
    if (!out) goto cleanup;

    buf = malloc(4096);
    if (!buf) goto cleanup;

    if (ferror(in)) goto cleanup;

    // ... procesar ...
    result = 0;

cleanup:
    free(buf);           // free(NULL) es seguro (no-op)
    if (out) fclose(out);
    if (in) fclose(in);
    return result;
}
```

### Por qué funciona

1. Todos los punteros se inicializan a `NULL` al principio
2. `free(NULL)` es un no-op garantizado por el estándar C
3. Cualquier error salta a `cleanup`, donde se liberan todos los recursos
4. Los recursos no alocados (aún `NULL`) se manejan sin problema

### goto en C: uso legítimo

`goto` tiene mala reputación por el abuso en código espagueti. Pero `goto cleanup` es un **idioma establecido** en C, usado extensamente en el kernel de Linux. La alternativa — anidar `if`/`else` con cleanup manual en cada nivel — es más propensa a errores y menos legible.

---

## 7. Destructores correctos: structs y estructuras recursivas

### Structs con miembros alocados dinámicamente

```c
struct Person {
    char *name;     // malloc'd
    char *email;    // malloc'd
    int  *scores;   // malloc'd
};

// MAL: solo libera el struct, sus miembros quedan perdidos
void person_free_wrong(struct Person *p) {
    free(p);  // name, email, scores → definitely lost
}

// BIEN: liberar miembros PRIMERO, luego el struct
void person_free(struct Person *p) {
    if (!p) return;
    free(p->name);
    free(p->email);
    free(p->scores);
    free(p);
}
```

Regla: **de adentro hacia afuera**. Primero los miembros, luego el contenedor. Si se invierte el orden, se accede a memoria ya liberada (use-after-free).

### Listas enlazadas

```c
struct Node {
    int value;
    struct Node *next;
};

// MAL: solo libera la cabeza
void free_list_wrong(struct Node *head) {
    free(head);  // Todos los nodos siguientes → definitely lost
}

// MAL SUTIL: use-after-free
void free_list_bad(struct Node *head) {
    while (head) {
        free(head);
        head = head->next;  // UB: head ya fue liberado
    }
}

// BIEN: guardar next ANTES de free
void free_list(struct Node *head) {
    while (head) {
        struct Node *next = head->next;
        free(head);
        head = next;
    }
}
```

### Árboles (recursivo)

```c
struct TreeNode {
    int value;
    struct TreeNode *left;
    struct TreeNode *right;
};

void tree_free(struct TreeNode *node) {
    if (!node) return;
    tree_free(node->left);   // Liberar subárbol izquierdo
    tree_free(node->right);  // Liberar subárbol derecho
    free(node);              // Liberar este nodo
}
```

---

## 8. Patrones de ownership en C

C no tiene ownership automático. La responsabilidad de hacer `free` se establece por **convención**:

### Patrón 1: "el que aloca, libera"

```c
void process(void) {
    char *data = malloc(100);
    // ... usar data ...
    free(data);  // Mismo scope, misma función
}
```

El caso más simple y seguro. La función que hizo `malloc` también hace `free`.

### Patrón 2: constructor / destructor

```c
// Interfaz pública:
struct Buffer *buffer_create(size_t capacity);
void buffer_destroy(struct Buffer *buf);

// Uso:
struct Buffer *buf = buffer_create(1024);
// ... usar buf ...
buffer_destroy(buf);
```

Naming convention: `xxx_create` / `xxx_destroy`, o `xxx_new` / `xxx_free`, o `xxx_alloc` / `xxx_dealloc`. Lo importante es que el par sea obvio.

### Patrón 3: init / deinit (struct en stack)

```c
struct Buffer {
    char *data;
    size_t len;
    size_t cap;
};

void buffer_init(struct Buffer *buf, size_t cap) {
    buf->data = malloc(cap);
    buf->len = 0;
    buf->cap = cap;
}

void buffer_deinit(struct Buffer *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->len = buf->cap = 0;
}

// Uso (struct en stack, datos internos en heap):
struct Buffer buf;
buffer_init(&buf, 1024);
// ... usar buf ...
buffer_deinit(&buf);
```

### Patrón 4: documentar transferencia de ownership

```c
// @return: caller must free() the returned pointer
char *read_file(const char *path);

// @param data: this function takes ownership (will free)
void process_and_free(int *data);
```

Cuando una función retorna un puntero alocado, documentar que el **caller** es responsable de liberarlo. Cuando una función recibe un puntero y lo libera internamente, documentar la **transferencia de ownership**.

---

## 9. Cuándo un leak es aceptable (y cuándo no)

### Generalmente aceptable

```c
// Memoria que vive toda la vida del programa:
int main(void) {
    struct Config *config = load_config();
    // config se usa durante todo el programa
    // El SO libera TODA la memoria del proceso al terminar
    // Valgrind reporta "still reachable" — no es un bug funcional
    return 0;
}
```

El SO reclama toda la memoria del proceso al terminar (`exit`/`_exit`), incluyendo el heap completo. Así que la memoria "still reachable" no se pierde realmente — se libera junto con el proceso.

### NUNCA aceptable

```c
// Leaks en funciones llamadas repetidamente:
void handle_request(const char *url) {
    char *response = fetch(url);
    // Procesar response...
    // Olvidar free(response) → leak por request
}

// Un servidor que procesa 1000 requests/segundo:
// 1000 × ~4KB = 4 MB/s de leak
// En 1 hora: 14.4 GB → OOM
```

Leaks en **loops**, funciones llamadas repetidamente, o **daemons/servidores** de larga duración crecen sin límite y eventualmente agotan la memoria (OOM killer).

### Zona gris

```c
// Programa CLI de corta duración:
int main(int argc, char *argv[]) {
    char *result = process(argv[1]);
    printf("%s\n", result);
    // ¿Hacer free(result) antes de return?
    // Técnicamente no es necesario — el SO limpia todo.
    // Pero hacer free es buena práctica:
    // 1. Permite verificar con Valgrind que no hay otros leaks
    // 2. Si la función se reutiliza en un servidor, ya no hay leak
    // 3. Establece el hábito de ownership disciplinado
    free(result);
    return 0;
}
```

---

## 10. Comparación con Rust

Rust previene la mayoría de los leaks a nivel de lenguaje mediante el sistema de ownership:

```rust
// El ownership system garantiza free automático al salir del scope:
fn example() {
    let data = vec![1, 2, 3, 4, 5];
    // data se aloca en heap (Vec usa malloc internamente)

    // Al salir del scope, Rust inserta drop(data) automáticamente
    // → equivalente a free(data.ptr) + free del Vec
}
// No es posible "olvidar" el free — el compilador lo inserta

// Equivalente al patrón constructor/destructor de C,
// pero con destrucción automática (trait Drop):
struct Buffer {
    data: Vec<u8>,
}

impl Drop for Buffer {
    fn drop(&mut self) {
        // Cleanup personalizado (Vec se libera automáticamente)
        println!("Buffer dropped");
    }
}

fn use_buffer() {
    let buf = Buffer { data: vec![0; 1024] };
    // ... usar buf ...
}   // drop(buf) se llama automáticamente aquí

// El patrón goto cleanup no es necesario — RAII maneja todo:
fn process(path: &str) -> Result<(), std::io::Error> {
    let file = std::fs::File::open(path)?;  // ? retorna temprano si error
    let data = std::fs::read_to_string(path)?;
    // Si ? retorna, file se dropea automáticamente
    // No hay leak posible en ningún path de error
    Ok(())
}

// ¿Puede haber leaks en Rust? Sí, pero es muy difícil:
// 1. std::mem::forget(value) — suprime el drop intencionalmente
// 2. Rc<T> con ciclos — referencia circular, ningún Rc llega a 0
//    Solución: Weak<T> para romper ciclos
// 3. Box::leak(value) — leak intencional para datos con lifetime 'static
```

La diferencia fundamental: en C, cada `malloc` requiere disciplina humana para recordar el `free` correspondiente. En Rust, el compilador inserta el `free` (drop) automáticamente y rechaza el código que violaría las reglas de ownership.

---

## Ejercicios

### Ejercicio 1 — Detectar el leak simple

```c
#include <stdio.h>
#include <stdlib.h>

void compute(void) {
    int *data = malloc(10 * sizeof(int));
    if (!data) return;
    for (int i = 0; i < 10; i++) {
        data[i] = i * i;
    }
    printf("data[9] = %d\n", data[9]);
}

int main(void) {
    compute();
    return 0;
}
```

Compila con `gcc -g -O0 leak1.c -o leak1` y ejecuta con `valgrind --leak-check=full ./leak1`.

**Predicción**: ¿Cuántos bytes reportará Valgrind como "definitely lost"? ¿En qué línea del código fuente aparecerá el `malloc` en el stack trace?

<details><summary>Respuesta</summary>

40 bytes (10 × `sizeof(int)` = 10 × 4) en 1 bloque, "definitely lost". El stack trace apuntará a la línea del `malloc` dentro de `compute`. La corrección: agregar `free(data);` antes del cierre de la función.

</details>

### Ejercicio 2 — Leak por reasignación

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char *msg = malloc(32);
    if (!msg) return 1;
    strcpy(msg, "hello");
    printf("1: %s\n", msg);

    msg = malloc(64);
    if (!msg) return 1;
    strcpy(msg, "world");
    printf("2: %s\n", msg);

    free(msg);
    return 0;
}
```

**Predicción**: ¿Cuántos bloques se pierden? ¿Cuántos bytes? ¿Valgrind dirá "definitely lost" o "still reachable"?

<details><summary>Respuesta</summary>

1 bloque de 32 bytes, "definitely lost". El segundo `msg = malloc(64)` sobreescribe el puntero al primer bloque de 32 bytes sin hacer `free`. El `free(msg)` final solo libera el segundo bloque (64 bytes). Corrección: agregar `free(msg);` antes de `msg = malloc(64);`.

</details>

### Ejercicio 3 — Leak en error path

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int process(const char *input) {
    char *buf = malloc(256);
    if (!buf) return -1;

    if (strlen(input) > 200) {
        printf("Input too long\n");
        return -1;
    }

    strcpy(buf, input);
    printf("Processed: %s\n", buf);
    free(buf);
    return 0;
}

int main(void) {
    process("short input");

    char long_input[300];
    memset(long_input, 'A', 299);
    long_input[299] = '\0';
    process(long_input);

    return 0;
}
```

**Predicción**: ¿Cuántas llamadas a `process` producen leak? ¿Por qué la primera no pierde memoria pero la segunda sí?

<details><summary>Respuesta</summary>

Solo la segunda llamada (con `long_input`) produce leak. En la primera, `strlen("short input")` ≤ 200, así que se ejecuta el happy path con `free(buf)`. En la segunda, `strlen(long_input)` = 299 > 200, así que se ejecuta `return -1` sin `free(buf)` → 256 bytes "definitely lost". Corrección: usar el patrón `goto cleanup` o agregar `free(buf)` antes del `return -1`.

</details>

### Ejercicio 4 — Categorías de Valgrind

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Item {
    char *name;
    int  *data;
};

static int *global_ptr = NULL;

int main(void) {
    // Caso A
    int *a = malloc(100);
    a = NULL;

    // Caso B
    struct Item *item = malloc(sizeof(struct Item));
    item->name = malloc(20);
    strcpy(item->name, "test");
    item->data = malloc(40);
    // No liberar nada — item se pierde al salir

    // Caso C
    global_ptr = malloc(200);

    return 0;
}
```

**Predicción**: Para cada caso (A, B, C), ¿qué categoría de Valgrind asignará a cada bloque? ¿Cuántos bytes en cada categoría en el LEAK SUMMARY?

<details><summary>Respuesta</summary>

- **Caso A**: 100 bytes "definitely lost" (1 bloque). `a = NULL` destruye la referencia.
- **Caso B**: `item` (probablemente 16 bytes en 64-bit) es "definitely lost". `item->name` (20 bytes) e `item->data` (40 bytes) son "indirectly lost" — son accesibles solo a través de `item`, que está perdido (nunca freed, solo leaked al salir del scope).
- **Caso C**: 200 bytes "still reachable" (1 bloque). `global_ptr` es una variable global que persiste hasta el final del programa — Valgrind puede encontrar el puntero.

LEAK SUMMARY: definitely lost: ~116 bytes (100 + 16) en 2 bloques; indirectly lost: 60 bytes (20 + 40) en 2 bloques; still reachable: 200 bytes en 1 bloque.

</details>

### Ejercicio 5 — Corregir con goto cleanup

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int process(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char *line = malloc(256);
    if (!line) return -1;          // LEAK: f

    char *result = malloc(1024);
    if (!result) return -1;        // LEAK: f, line

    // Simular error
    if (ferror(f)) return -1;      // LEAK: f, line, result

    free(result);
    free(line);
    fclose(f);
    return 0;
}

int main(void) {
    process("/dev/null");
    return 0;
}
```

Reescribe `process` usando el patrón `goto cleanup`.

**Predicción**: ¿Cuántas etiquetas `cleanup` necesitas? ¿Qué pasa si `fopen` falla — se ejecuta `free(NULL)` para `line` y `result`?

<details><summary>Respuesta</summary>

Solo 1 etiqueta `cleanup`. Inicializar `f = NULL`, `line = NULL`, `result = NULL` al principio. Si `fopen` falla, `goto cleanup` ejecuta `free(NULL)` para `line` y `result` (no-op seguro) y `if (f) fclose(f)` con `f = NULL` (no ejecuta fclose). Solución:

```c
int process(const char *path) {
    int ret = -1;
    FILE *f = NULL;
    char *line = NULL;
    char *result = NULL;

    f = fopen(path, "r");
    if (!f) goto cleanup;

    line = malloc(256);
    if (!line) goto cleanup;

    result = malloc(1024);
    if (!result) goto cleanup;

    if (ferror(f)) goto cleanup;

    ret = 0;
cleanup:
    free(result);
    free(line);
    if (f) fclose(f);
    return ret;
}
```

</details>

### Ejercicio 6 — Destructor incompleto

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Student {
    char *name;
    int  *grades;
    int   grade_count;
};

struct Student *student_create(const char *name, int n) {
    struct Student *s = malloc(sizeof(*s));
    if (!s) return NULL;
    s->name = malloc(strlen(name) + 1);
    if (!s->name) { free(s); return NULL; }
    strcpy(s->name, name);
    s->grades = malloc((size_t)n * sizeof(int));
    if (!s->grades) { free(s->name); free(s); return NULL; }
    s->grade_count = n;
    return s;
}

void student_free(struct Student *s) {
    free(s);
}

int main(void) {
    struct Student *s = student_create("Ana", 3);
    if (s) {
        s->grades[0] = 90;
        s->grades[1] = 85;
        s->grades[2] = 95;
        printf("%s: %d, %d, %d\n", s->name, s->grades[0], s->grades[1], s->grades[2]);
        student_free(s);
    }
    return 0;
}
```

**Predicción**: Tras ejecutar con Valgrind, ¿cuántos bloques se reportan como perdidos? ¿Serán "definitely lost" o "indirectly lost"? ¿Por qué?

<details><summary>Respuesta</summary>

2 bloques: `s->name` y `s->grades`, ambos **"definitely lost"** (no "indirectly lost"). El struct `s` fue **freed** por `student_free`, así que ya no existe en el conjunto de bloques alocados. Valgrind no escanea bloques liberados como fuente de punteros. Los punteros a `name` y `grades` que estaban dentro del struct desaparecieron con el `free(s)`, así que no hay ningún puntero apuntando a esos bloques → "definitely lost". Corrección:

```c
void student_free(struct Student *s) {
    if (!s) return;
    free(s->name);
    free(s->grades);
    free(s);
}
```

</details>

### Ejercicio 7 — Liberar lista enlazada correctamente

```c
#include <stdio.h>
#include <stdlib.h>

struct Node {
    int value;
    struct Node *next;
};

struct Node *list_prepend(struct Node *head, int val) {
    struct Node *n = malloc(sizeof(*n));
    if (!n) return head;
    n->value = val;
    n->next = head;
    return n;
}

int main(void) {
    struct Node *list = NULL;
    for (int i = 0; i < 5; i++) {
        list = list_prepend(list, i * 10);
    }

    // Imprimir
    for (struct Node *n = list; n; n = n->next) {
        printf("%d -> ", n->value);
    }
    printf("NULL\n");

    // INCORRECTO: solo libera la cabeza
    free(list);

    return 0;
}
```

**Predicción**: ¿Cuántos bloques reportará Valgrind como perdidos? ¿Serán "definitely lost" o "indirectly lost"?

<details><summary>Respuesta</summary>

4 bloques perdidos. El `free(list)` libera la cabeza (nodo con valor 40). Los 4 nodos restantes (30→20→10→0) quedan sin liberar. El nodo con valor 30 es **"definitely lost"** (el puntero `list->next` que apuntaba a él desapareció con el `free(list)`). Los nodos 20, 10 y 0 son **"definitely lost"** también, porque cada uno dependía transitivamente del nodo 30 que fue perdido — aunque en la práctica Valgrind puede clasificar los nodos intermedios como "indirectly lost" si la memoria del nodo 30 (freed como parte de la cabeza... no, la cabeza y el nodo 30 son bloques separados). Corrección, cada nodo es un bloque separado. `free(list)` libera nodo-40. `nodo-40->next` apuntaba a nodo-30, pero ese puntero estaba en la memoria de nodo-40, que fue freed. Entonces nodo-30 es "definitely lost", nodo-20 es "indirectly lost" (accesible solo via nodo-30), nodo-10 es "indirectly lost" (accesible solo via nodo-20 via nodo-30), nodo-0 también "indirectly lost". En resumen: 1 "definitely lost", 3 "indirectly lost". Corrección:

```c
struct Node *n = list;
while (n) {
    struct Node *next = n->next;
    free(n);
    n = next;
}
```

</details>

### Ejercicio 8 — Leak acumulativo en loop

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *format_line(int n) {
    char *buf = malloc(64);
    if (!buf) return NULL;
    snprintf(buf, 64, "Line %d: data here", n);
    return buf;
}

int main(void) {
    for (int i = 0; i < 100; i++) {
        char *line = format_line(i);
        if (line) {
            printf("%s\n", line);
        }
        // Falta: free(line);
    }
    return 0;
}
```

**Predicción**: ¿Cuántos bytes totales reportará Valgrind como perdidos? ¿Cuántos bloques? Si el loop fuera de 1,000,000 iteraciones en un servidor, ¿cuánta memoria se perdería?

<details><summary>Respuesta</summary>

100 bloques, cada uno de 64 bytes solicitados (pero `malloc_usable_size` podría dar ~72). Valgrind reportará ~6400 bytes "definitely lost" en 100 bloques. Con 1,000,000 iteraciones: 64 × 1,000,000 = ~64 MB de leak (más overhead del allocator). Corrección: agregar `free(line);` después del `printf` dentro del loop.

</details>

### Ejercicio 9 — still reachable vs definitely lost

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *app_name = NULL;
static int  *cache    = NULL;

void init(void) {
    app_name = malloc(32);
    if (app_name) strcpy(app_name, "MyApp");

    cache = malloc(1024);
}

void cleanup(void) {
    free(cache);
    cache = NULL;
    // Olvidó free(app_name)
}

int main(void) {
    init();
    printf("Running %s...\n", app_name);
    cleanup();
    return 0;
}
```

Ejecuta con `valgrind --leak-check=full --show-leak-kinds=all ./prog`.

**Predicción**: ¿`app_name` será "definitely lost" o "still reachable"? ¿Y `cache`?

<details><summary>Respuesta</summary>

`app_name` es **"still reachable"** — la variable global `app_name` sigue existiendo al terminar el programa y apunta al bloque alocado. Valgrind puede encontrar ese puntero escaneando el segmento de datos globales. `cache` no genera leak porque `cleanup()` hace `free(cache)`. Si `cleanup()` no hubiera puesto `cache = NULL` pero sí hiciera `free(cache)`, Valgrind no reportaría leak de `cache` (está freed, no leaked).

</details>

### Ejercicio 10 — Constructor/destructor completo

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct StringList {
    char **items;
    int len;
    int cap;
};

struct StringList *slist_create(int initial_cap) {
    struct StringList *sl = malloc(sizeof(*sl));
    if (!sl) return NULL;
    sl->items = malloc((size_t)initial_cap * sizeof(char *));
    if (!sl->items) { free(sl); return NULL; }
    sl->len = 0;
    sl->cap = initial_cap;
    return sl;
}

int slist_add(struct StringList *sl, const char *str) {
    if (sl->len >= sl->cap) return -1;
    sl->items[sl->len] = malloc(strlen(str) + 1);
    if (!sl->items[sl->len]) return -1;
    strcpy(sl->items[sl->len], str);
    sl->len++;
    return 0;
}

// INCOMPLETO: ¿qué falta?
void slist_destroy(struct StringList *sl) {
    free(sl->items);
    free(sl);
}

int main(void) {
    struct StringList *sl = slist_create(10);
    if (!sl) return 1;

    slist_add(sl, "alpha");
    slist_add(sl, "beta");
    slist_add(sl, "gamma");

    for (int i = 0; i < sl->len; i++) {
        printf("[%d] %s\n", i, sl->items[i]);
    }

    slist_destroy(sl);
    return 0;
}
```

**Predicción**: ¿Cuántos bloques reportará Valgrind como perdidos? ¿Qué falta en `slist_destroy`?

<details><summary>Respuesta</summary>

3 bloques perdidos: los tres strings individuales ("alpha", "beta", "gamma") alocados por `slist_add` con `malloc(strlen(str) + 1)`. `slist_destroy` libera el array de punteros (`items`) y el struct (`sl`), pero no libera cada string individual apuntado por `items[0]`, `items[1]`, `items[2]`. Serán "definitely lost" (el array `items` que contenía los punteros fue freed). Corrección:

```c
void slist_destroy(struct StringList *sl) {
    if (!sl) return;
    for (int i = 0; i < sl->len; i++) {
        free(sl->items[i]);
    }
    free(sl->items);
    free(sl);
}
```

</details>
