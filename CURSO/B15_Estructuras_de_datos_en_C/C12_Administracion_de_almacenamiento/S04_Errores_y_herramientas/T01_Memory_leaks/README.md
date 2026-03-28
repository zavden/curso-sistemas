# T01 — Memory leaks

## Objetivo

Comprender los **memory leaks** (fugas de memoria) como uno de los
errores más comunes en gestión manual de memoria: cómo ocurren en
listas (olvidar nodos al destruir), árboles (olvidar postorden),
grafos (ciclos), y otros patrones. Aprender a detectarlos, y
entender por qué Rust los previene en la mayoría de los casos.

---

## 1. Qué es un memory leak

### 1.1 Definición

Un **memory leak** ocurre cuando un programa asigna memoria con
`malloc` (o equivalente) pero nunca la libera con `free`. La memoria
sigue reservada pero es **inaccesible** desde el programa: ningún
puntero apunta a ella.

```
Antes del leak:
  ptr ---> [datos en heap]

Después del leak:
  ptr = NULL (o reasignado)
            [datos en heap]   <-- nadie apunta aquí
                                  memoria perdida
```

### 1.2 Consecuencias

| Consecuencia | Descripción |
|-------------|-------------|
| Crecimiento de memoria | El proceso consume más RAM con el tiempo |
| OOM (Out Of Memory) | Eventualmente el sistema niega más asignaciones |
| Degradación | Más presión en swap, peor rendimiento |
| Inestabilidad | El OOM killer (Linux) puede matar el proceso |

Para procesos de corta duración (CLI, scripts), los leaks rara vez
son problema — el OS reclama toda la memoria al terminar. Para
**servidores y daemons** que corren días/semanas, un leak de pocos
bytes por request se acumula hasta causar un crash.

### 1.3 Leak rate

Si un servidor pierde `b` bytes por request y recibe `r` requests
por segundo, el consumo de memoria crece a:

leak_rate = b × r bytes/segundo

Con b = 64 bytes y r = 1000 req/s:

64 × 1000 = 64,000 bytes/s = 62.5 KB/s ≈ 5.3 GB/día

Un leak aparentemente trivial causa OOM en horas.

---

## 2. Patrones comunes de memory leak

### 2.1 Patrón 1: Olvidar liberar en una lista enlazada

El error más clásico: liberar solo la cabeza de la lista sin
recorrer los nodos:

```c
/* INCORRECTO: solo libera la cabeza */
void list_destroy_wrong(Node *head) {
    free(head);  /* nodos 2, 3, ... perdidos */
}

/* CORRECTO: recorrer y liberar cada nodo */
void list_destroy(Node *head) {
    while (head) {
        Node *next = head->next;
        free(head);
        head = next;
    }
}
```

```
Lista: [A] -> [B] -> [C] -> NULL

list_destroy_wrong(&A):
  free(A)
  Resultado: [B] -> [C] -> NULL  (leak: B y C)

list_destroy(&A):
  free(A), free(B), free(C)
  Resultado: todo liberado
```

### 2.2 Patrón 2: Olvidar postorden en un árbol

Los árboles deben liberarse en **postorden** (hijos primero, luego
padre). Si se libera el padre primero, se pierden los hijos:

```c
/* INCORRECTO: preorden (pierde hijos) */
void tree_destroy_wrong(TreeNode *node) {
    if (!node) return;
    free(node);  /* hijos ahora inaccesibles */
    tree_destroy_wrong(node->left);   /* UB: node ya liberado */
    tree_destroy_wrong(node->right);  /* UB: use-after-free */
}

/* CORRECTO: postorden */
void tree_destroy(TreeNode *node) {
    if (!node) return;
    tree_destroy(node->left);
    tree_destroy(node->right);
    free(node);
}
```

### 2.3 Patrón 3: Ciclos en grafos

En un grafo con ciclos, un recorrido naive puede entrar en bucle
infinito o, si detecta el ciclo, puede olvidar liberar los nodos
del ciclo:

```
Grafo: A -> B -> C -> A (ciclo)

Si recorremos liberando:
  free(A), luego A->next = B
  free(B), luego B->next = C
  free(C), luego C->next = A — pero A ya fue liberado!
  (use-after-free, no leak — pero igualmente peligroso)

Con detección de visitados:
  Necesitamos un conjunto de "visitados" para evitar
  re-visitar nodos ya liberados.
```

### 2.4 Patrón 4: Reasignar puntero sin liberar

```c
char *buf = malloc(100);
/* ... usar buf ... */
buf = malloc(200);  /* LEAK: los primeros 100 bytes perdidos */
```

### 2.5 Patrón 5: Early return sin cleanup

```c
int process(void) {
    char *data = malloc(1024);
    int *result = malloc(sizeof(int) * 10);

    if (validate(data) < 0) {
        return -1;  /* LEAK: data y result no liberados */
    }

    free(data);
    free(result);
    return 0;
}
```

### 2.6 Patrón 6: Liberar struct sin liberar campos

```c
typedef struct {
    char *name;   /* malloc'd */
    int  *data;   /* malloc'd */
} Record;

void record_destroy_wrong(Record *r) {
    free(r);  /* LEAK: r->name y r->data no liberados */
}

void record_destroy(Record *r) {
    free(r->name);
    free(r->data);
    free(r);
}
```

---

## 3. Detección de leaks

### 3.1 Herramientas

| Herramienta | Tipo | Detección |
|-------------|------|-----------|
| **Valgrind memcheck** | Runtime (instrumentación) | Reporte detallado al terminar |
| **ASan (LeakSanitizer)** | Compilador (instrumentación) | Reporte al terminar |
| **mtrace** | glibc | Hook de malloc/free, analizar log |
| **Miri** | Rust (interpretador) | Detecta leaks en unsafe Rust |

### 3.2 Valgrind: reporte de leak

```
==1234== LEAK SUMMARY:
==1234==    definitely lost: 48 bytes in 3 blocks
==1234==    indirectly lost: 0 bytes in 0 blocks
==1234==      possibly lost: 0 bytes in 0 blocks
==1234==    still reachable: 0 bytes in 0 blocks
```

| Categoría | Significado |
|-----------|-------------|
| **definitely lost** | Memoria sin ningún puntero que apunte a ella |
| **indirectly lost** | Memoria apuntada solo desde bloques "definitely lost" |
| **possibly lost** | Puntero al interior del bloque (no al inicio) |
| **still reachable** | Aún hay punteros, pero no se liberó al terminar |

### 3.3 ASan (AddressSanitizer + LeakSanitizer)

```bash
gcc -fsanitize=address -g program.c -o program
./program
```

```
=================================================================
==1234==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 48 bytes in 3 object(s) allocated from:
    #0 0x... in malloc
    #1 0x... in create_node (program.c:15)
    #2 0x... in main (program.c:42)
```

---

## 4. Cómo Rust previene leaks

### 4.1 Ownership = liberación automática

En Rust, cada valor tiene un owner. Cuando el owner sale del scope,
`Drop::drop()` se llama automáticamente. No hay forma de "olvidar"
liberar:

```rust
fn process() {
    let data = vec![1, 2, 3];  // allocated
    if error {
        return;  // data dropped here — no leak
    }
    // data dropped here — no leak
}
```

### 4.2 Patrones que Rust previene automáticamente

| Patrón C (leak) | Equivalente Rust |
|-----------------|-----------------|
| Olvidar `free(list)` | `Drop` recorre y libera |
| Olvidar postorden en árbol | `Drop` recursivo automático |
| Reasignar sin liberar | Drop del valor anterior al reasignar |
| Early return sin cleanup | Drop al salir del scope |
| Liberar struct sin campos | Drop glue libera todos los campos |

### 4.3 Leaks que Rust NO previene

Rust no considera los memory leaks como **unsafe** — no causan
undefined behavior. Estos patrones pueden causar leaks en safe Rust:

1. **Ciclos con `Rc`**: como vimos en S03/T01 y T04.
2. **`std::mem::forget()`**: explícitamente "olvida" un valor sin
   llamar Drop.
3. **`Box::leak()`**: convierte un Box en referencia `'static`,
   filtrando la memoria intencionalmente.
4. **Canales (channels)**: enviar datos a un canal que nadie lee.

```rust
use std::mem;

let data = vec![1, 2, 3];
mem::forget(data);  // LEAK: Drop never called
// Safe Rust — no UB, but memory lost
```

---

## 5. Estrategias de prevención en C

### 5.1 Patrones defensivos

| Estrategia | Descripción |
|-----------|-------------|
| **Funciones destroy()** | Cada `_create()` tiene su `_destroy()` |
| **goto cleanup** | Un solo punto de salida con cleanup al final |
| **Set to NULL** | Después de `free(p)`, hacer `p = NULL` |
| **Ownership documentation** | Documentar quién es responsable de liberar |
| **Arena allocator** | Asignar en arena, liberar todo de golpe (S02/T01) |

### 5.2 Patrón goto cleanup

```c
int process(const char *filename) {
    int ret = -1;
    FILE *f = NULL;
    char *buf = NULL;
    int *data = NULL;

    f = fopen(filename, "r");
    if (!f) goto cleanup;

    buf = malloc(1024);
    if (!buf) goto cleanup;

    data = malloc(sizeof(int) * 100);
    if (!data) goto cleanup;

    /* ... work ... */
    ret = 0;

cleanup:
    free(data);   /* free(NULL) is safe (no-op) */
    free(buf);
    if (f) fclose(f);
    return ret;
}
```

---

## 6. Programa en C — Demostraciones de memory leaks

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 *  Memory Leak Demonstrations
 *
 *  Compile with: gcc -std=c11 -Wall -Wextra -g -o leaks leaks.c
 *  Run with Valgrind: valgrind --leak-check=full ./leaks
 * ============================================================ */

/* ---- Linked list ---- */

typedef struct LNode {
    int data;
    struct LNode *next;
} LNode;

LNode *lnode_new(int data) {
    LNode *n = malloc(sizeof(LNode));
    n->data = data;
    n->next = NULL;
    return n;
}

void list_push(LNode **head, int data) {
    LNode *n = lnode_new(data);
    n->next = *head;
    *head = n;
}

void list_print(LNode *head) {
    printf("  list: ");
    for (LNode *n = head; n; n = n->next)
        printf("%d -> ", n->data);
    printf("NULL\n");
}

/* WRONG: only frees head */
void list_destroy_wrong(LNode *head) {
    free(head);
}

/* CORRECT: free all nodes */
void list_destroy(LNode *head) {
    while (head) {
        LNode *next = head->next;
        free(head);
        head = next;
    }
}

int list_count(LNode *head) {
    int c = 0;
    for (LNode *n = head; n; n = n->next) c++;
    return c;
}

/* ---- Binary tree ---- */

typedef struct TNode {
    int data;
    struct TNode *left;
    struct TNode *right;
} TNode;

TNode *tnode_new(int data) {
    TNode *n = malloc(sizeof(TNode));
    n->data = data;
    n->left = n->right = NULL;
    return n;
}

/* WRONG: preorder free (use-after-free + leak) */
void tree_destroy_wrong(TNode *node) {
    if (!node) return;
    TNode *l = node->left;
    TNode *r = node->right;
    free(node);
    /* l and r saved before free — avoids UB but still wrong
       pattern to teach: in real code, people often write:
       free(node); tree_destroy_wrong(node->left); // UB! */
    (void)l; (void)r;
    /* NOT recursing = leak of subtrees */
}

/* CORRECT: postorder free */
void tree_destroy(TNode *node) {
    if (!node) return;
    tree_destroy(node->left);
    tree_destroy(node->right);
    free(node);
}

int tree_count(TNode *node) {
    if (!node) return 0;
    return 1 + tree_count(node->left) + tree_count(node->right);
}

/* ---- Record with owned fields ---- */

typedef struct {
    char *name;
    int  *scores;
    int   num_scores;
} Record;

Record *record_new(const char *name, int n) {
    Record *r = malloc(sizeof(Record));
    r->name = malloc(strlen(name) + 1);
    strcpy(r->name, name);
    r->scores = malloc(sizeof(int) * n);
    r->num_scores = n;
    for (int i = 0; i < n; i++)
        r->scores[i] = (i + 1) * 10;
    return r;
}

/* WRONG: free struct without fields */
void record_destroy_wrong(Record *r) {
    free(r);  /* r->name and r->scores leaked */
}

/* CORRECT: free fields then struct */
void record_destroy(Record *r) {
    free(r->name);
    free(r->scores);
    free(r);
}

/* ============================================================
 *  Demo 1: List leak — freeing only head
 * ============================================================ */
void demo1(void) {
    printf("\n=== Demo 1: List leak — freeing only head ===\n");

    LNode *list = NULL;
    for (int i = 5; i >= 1; i--)
        list_push(&list, i);
    list_print(list);

    int count = list_count(list);
    printf("  Nodes: %d\n", count);

    printf("\n  --- WRONG: list_destroy_wrong (free only head) ---\n");
    printf("  Leaks %d nodes (%lu bytes)\n",
           count - 1, (unsigned long)(count - 1) * sizeof(LNode));
    printf("  (Not actually calling it to avoid UB in later demos)\n");

    printf("\n  --- CORRECT: list_destroy (free all) ---\n");
    list_destroy(list);
    printf("  All %d nodes freed.\n", count);
}

/* ============================================================
 *  Demo 2: Tree leak — not using postorder
 * ============================================================ */
void demo2(void) {
    printf("\n=== Demo 2: Tree leak — wrong traversal ===\n");

    /*        10
     *       /  \
     *      5    15
     *     / \
     *    3   7
     */
    TNode *root = tnode_new(10);
    root->left = tnode_new(5);
    root->right = tnode_new(15);
    root->left->left = tnode_new(3);
    root->left->right = tnode_new(7);

    int count = tree_count(root);
    printf("  Tree nodes: %d\n", count);

    printf("\n  --- WRONG: tree_destroy_wrong (only frees root) ---\n");
    printf("  Would leak %d nodes (%lu bytes)\n",
           count - 1, (unsigned long)(count - 1) * sizeof(TNode));

    printf("\n  --- CORRECT: tree_destroy (postorder) ---\n");
    tree_destroy(root);
    printf("  All %d nodes freed in postorder.\n", count);
}

/* ============================================================
 *  Demo 3: Record leak — struct without fields
 * ============================================================ */
void demo3(void) {
    printf("\n=== Demo 3: Record leak — struct without fields ===\n");

    Record *r = record_new("Alice", 5);
    printf("  Record: name=%s, scores=[", r->name);
    for (int i = 0; i < r->num_scores; i++) {
        if (i) printf(",");
        printf("%d", r->scores[i]);
    }
    printf("]\n");

    size_t field_bytes = strlen(r->name) + 1 + sizeof(int) * r->num_scores;
    printf("\n  --- WRONG: record_destroy_wrong ---\n");
    printf("  Would leak name (%lu bytes) + scores (%lu bytes) = %lu bytes\n",
           (unsigned long)(strlen(r->name) + 1),
           (unsigned long)(sizeof(int) * r->num_scores),
           (unsigned long)field_bytes);

    printf("\n  --- CORRECT: record_destroy ---\n");
    record_destroy(r);
    printf("  Struct + all fields freed.\n");
}

/* ============================================================
 *  Demo 4: Pointer reassignment leak
 * ============================================================ */
void demo4(void) {
    printf("\n=== Demo 4: Pointer reassignment leak ===\n");

    printf("  --- WRONG pattern ---\n");
    char *buf = malloc(100);
    snprintf(buf, 100, "first allocation");
    printf("  buf = \"%s\" (%p)\n", buf, (void *)buf);

    char *old = buf;  /* save pointer before reassignment */

    buf = malloc(200);  /* LEAK if we don't free old */
    snprintf(buf, 200, "second allocation");
    printf("  buf = \"%s\" (%p) — first allocation lost!\n",
           buf, (void *)buf);

    printf("\n  --- FIX: free before reassign ---\n");
    free(old);   /* free the saved pointer */
    printf("  old pointer freed (saved before reassignment)\n");
    free(buf);
    printf("  current buf freed.\n");
}

/* ============================================================
 *  Demo 5: Early return leak + goto cleanup fix
 * ============================================================ */
int process_wrong(int fail_point) {
    char *step1 = malloc(64);
    strcpy(step1, "step1_data");

    if (fail_point == 1) {
        printf("    Fail at point 1: step1 leaked (%lu bytes)\n",
               (unsigned long)64);
        return -1;  /* LEAK: step1 not freed */
    }

    char *step2 = malloc(128);
    strcpy(step2, "step2_data");

    if (fail_point == 2) {
        printf("    Fail at point 2: step1+step2 leaked (%lu bytes)\n",
               (unsigned long)(64 + 128));
        return -1;  /* LEAK: step1 and step2 not freed */
    }

    free(step2);
    free(step1);
    return 0;
}

int process_correct(int fail_point) {
    int ret = -1;
    char *step1 = NULL;
    char *step2 = NULL;

    step1 = malloc(64);
    strcpy(step1, "step1_data");

    if (fail_point == 1) {
        printf("    Fail at point 1: goto cleanup\n");
        goto cleanup;
    }

    step2 = malloc(128);
    strcpy(step2, "step2_data");

    if (fail_point == 2) {
        printf("    Fail at point 2: goto cleanup\n");
        goto cleanup;
    }

    ret = 0;

cleanup:
    free(step2);  /* free(NULL) is safe */
    free(step1);
    printf("    All resources freed via cleanup label.\n");
    return ret;
}

void demo5(void) {
    printf("\n=== Demo 5: Early return leak + goto cleanup ===\n");

    printf("  --- WRONG: early return without free ---\n");
    process_wrong(1);
    process_wrong(2);

    printf("\n  --- CORRECT: goto cleanup pattern ---\n");
    process_correct(1);
    process_correct(2);
    process_correct(0);
}

/* ============================================================
 *  Demo 6: Leak accumulation simulation
 *  Simulates a server leaking bytes per request.
 * ============================================================ */
void demo6(void) {
    printf("\n=== Demo 6: Leak accumulation over time ===\n");

    int leak_per_request = 64;  /* bytes */
    int requests_per_sec = 1000;
    long total_leaked = 0;

    printf("  Simulating server: %d bytes leaked per request, "
           "%d req/s\n", leak_per_request, requests_per_sec);
    printf("\n  %-10s  %-15s  %-15s\n",
           "Time", "Requests", "Leaked");
    printf("  %-10s  %-15s  %-15s\n",
           "----", "--------", "------");

    int intervals[] = {1, 10, 60, 3600, 86400};  /* seconds */
    const char *labels[] = {"1 sec", "10 sec", "1 min",
                            "1 hour", "1 day"};

    for (int i = 0; i < 5; i++) {
        long reqs = (long)requests_per_sec * intervals[i];
        total_leaked = (long)leak_per_request * reqs;

        char leaked_str[32];
        if (total_leaked < 1024)
            snprintf(leaked_str, 32, "%ld B", total_leaked);
        else if (total_leaked < 1024 * 1024)
            snprintf(leaked_str, 32, "%.1f KB",
                     total_leaked / 1024.0);
        else if (total_leaked < 1024L * 1024 * 1024)
            snprintf(leaked_str, 32, "%.1f MB",
                     total_leaked / (1024.0 * 1024));
        else
            snprintf(leaked_str, 32, "%.1f GB",
                     total_leaked / (1024.0 * 1024 * 1024));

        printf("  %-10s  %-15ld  %-15s\n", labels[i], reqs, leaked_str);
    }

    printf("\n  64 bytes/request seems tiny, but at 1000 req/s:\n");
    printf("  -> %.1f GB leaked per day -> OOM in hours.\n",
           (double)leak_per_request * requests_per_sec * 86400
           / (1024.0 * 1024 * 1024));
}

int main(void) {
    demo1();
    demo2();
    demo3();
    demo4();
    demo5();
    demo6();
    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -g -o leaks leaks.c
./leaks
```

Para detectar leaks con Valgrind:

```bash
valgrind --leak-check=full ./leaks
```

### Salida esperada (extracto)

```
=== Demo 1: List leak — freeing only head ===
  list: 1 -> 2 -> 3 -> 4 -> 5 -> NULL
  Nodes: 5

  --- WRONG: list_destroy_wrong (free only head) ---
  Leaks 4 nodes (64 bytes)

  --- CORRECT: list_destroy (free all) ---
  All 5 nodes freed.

=== Demo 5: Early return leak + goto cleanup ===
  --- WRONG: early return without free ---
    Fail at point 1: step1 leaked (64 bytes)
    Fail at point 2: step1+step2 leaked (192 bytes)

  --- CORRECT: goto cleanup pattern ---
    Fail at point 1: goto cleanup
    All resources freed via cleanup label.

=== Demo 6: Leak accumulation over time ===
  Simulating server: 64 bytes leaked per request, 1000 req/s

  Time        Requests         Leaked
  ----        --------         ------
  1 sec       1000             62.5 KB
  10 sec      10000            625.0 KB
  1 min       60000            3.7 MB
  1 hour      3600000          219.7 MB
  1 day       86400000         5.1 GB
```

---

## 7. Programa en Rust — Rust previene (casi) todos los leaks

```rust
use std::cell::RefCell;
use std::rc::Rc;

/* ============================================================
 *  Memory Leak Prevention in Rust
 * ============================================================ */

/* ---- Traced type to show Drop ---- */

struct Traced {
    name: String,
}

impl Traced {
    fn new(name: &str) -> Self {
        println!("  [new]  {}", name);
        Traced { name: name.to_string() }
    }
}

impl Drop for Traced {
    fn drop(&mut self) {
        println!("  [drop] {}", self.name);
    }
}

/* ============================================================
 *  Demo 1: List — automatic Drop for all nodes
 * ============================================================ */
fn demo1() {
    println!("\n=== Demo 1: List — Drop frees all nodes ===");

    // In C: forgetting to walk the list = leak
    // In Rust: Vec drops all elements automatically
    {
        let list: Vec<Traced> = vec![
            Traced::new("node_1"),
            Traced::new("node_2"),
            Traced::new("node_3"),
            Traced::new("node_4"),
            Traced::new("node_5"),
        ];
        println!("  List has {} nodes", list.len());
        println!("  -- end of scope --");
    }
    println!("  All nodes dropped — impossible to forget in Rust.");
}

/* ============================================================
 *  Demo 2: Tree — recursive Drop (postorder automatic)
 * ============================================================ */
fn demo2() {
    println!("\n=== Demo 2: Tree — recursive Drop ===");

    struct TreeNode {
        data:  i32,
        left:  Option<Box<TreeNode>>,
        right: Option<Box<TreeNode>>,
    }

    impl Drop for TreeNode {
        fn drop(&mut self) {
            println!("  [drop] TreeNode({})", self.data);
            // left and right are dropped automatically after this
            // (postorder: children dropped after parent's drop() body)
        }
    }

    {
        let tree = Box::new(TreeNode {
            data: 10,
            left: Some(Box::new(TreeNode {
                data: 5,
                left: Some(Box::new(TreeNode {
                    data: 3, left: None, right: None,
                })),
                right: Some(Box::new(TreeNode {
                    data: 7, left: None, right: None,
                })),
            })),
            right: Some(Box::new(TreeNode {
                data: 15, left: None, right: None,
            })),
        });

        println!("  Tree root: {}", tree.data);
        println!("  -- end of scope --");
    }
    println!("  All nodes dropped — Rust does postorder automatically.");
}

/* ============================================================
 *  Demo 3: Struct with owned fields — Drop glue
 * ============================================================ */
fn demo3() {
    println!("\n=== Demo 3: Struct fields — automatic cleanup ===");

    struct Record {
        name:   String,
        scores: Vec<i32>,
    }

    impl Drop for Record {
        fn drop(&mut self) {
            println!("  [drop] Record({}, {} scores)",
                     self.name, self.scores.len());
        }
    }

    {
        let r = Record {
            name: "Alice".to_string(),
            scores: vec![10, 20, 30, 40, 50],
        };
        println!("  Record: name={}, scores={:?}", r.name, r.scores);
        println!("  -- end of scope --");
    }
    println!("  In C: forgetting free(r->name) or free(r->scores) = leak.");
    println!("  In Rust: Drop glue frees name (String) and scores (Vec).");
}

/* ============================================================
 *  Demo 4: Reassignment — old value dropped automatically
 * ============================================================ */
fn demo4() {
    println!("\n=== Demo 4: Reassignment drops old value ===");

    let mut buf = Traced::new("first_alloc");
    println!("  buf = {}", buf.name);

    buf = Traced::new("second_alloc");
    // "first_alloc" is dropped here automatically!
    println!("  buf = {} (first_alloc was auto-dropped)", buf.name);

    println!("  -- end of scope --");
    // "second_alloc" dropped here
}

/* ============================================================
 *  Demo 5: Early return — Drop runs on all paths
 * ============================================================ */
fn demo5() {
    println!("\n=== Demo 5: Early return — no leak ===");

    fn process(fail: bool) -> Result<(), &'static str> {
        let _step1 = Traced::new("step1");
        let _step2 = Traced::new("step2");

        if fail {
            println!("    Early return!");
            return Err("failed");
            // step1 and step2 dropped here — no goto needed
        }

        println!("    Success path");
        Ok(())
        // step1 and step2 dropped here too
    }

    println!("  Call with fail=true:");
    let _ = process(true);

    println!("\n  Call with fail=false:");
    let _ = process(false);

    println!("\n  Both paths clean up automatically — no goto cleanup.");
}

/* ============================================================
 *  Demo 6: Rc cycle — the one leak Rust CAN'T prevent
 * ============================================================ */
fn demo6() {
    println!("\n=== Demo 6: Rc cycle — leak in safe Rust ===");

    struct CycleNode {
        name: String,
        next: RefCell<Option<Rc<CycleNode>>>,
    }

    impl Drop for CycleNode {
        fn drop(&mut self) {
            println!("  [drop] CycleNode({})", self.name);
        }
    }

    // Create a cycle: A -> B -> A
    let a = Rc::new(CycleNode {
        name: "A".to_string(),
        next: RefCell::new(None),
    });
    let b = Rc::new(CycleNode {
        name: "B".to_string(),
        next: RefCell::new(Some(Rc::clone(&a))),
    });
    *a.next.borrow_mut() = Some(Rc::clone(&b));

    println!("  A strong_count={}", Rc::strong_count(&a));
    println!("  B strong_count={}", Rc::strong_count(&b));

    println!("  -- dropping local variables a, b --");
    drop(a);
    drop(b);

    println!("  Notice: NO [drop] messages! A and B leaked.");
    println!("  Rc cycle: each holds the other alive (strong_count=1).");
    println!("  This is the ONE type of leak safe Rust allows.");

    println!("\n  Fix: use Weak for one direction of the cycle.");
}

/* ============================================================
 *  Demo 7: Rc cycle fixed with Weak
 * ============================================================ */
fn demo7() {
    println!("\n=== Demo 7: Rc cycle fixed with Weak ===");

    use std::rc::Weak;

    struct SafeNode {
        name: String,
        next: RefCell<Option<Weak<SafeNode>>>,  // Weak, not Rc!
    }

    impl Drop for SafeNode {
        fn drop(&mut self) {
            println!("  [drop] SafeNode({})", self.name);
        }
    }

    let a = Rc::new(SafeNode {
        name: "A".to_string(),
        next: RefCell::new(None),
    });
    let b = Rc::new(SafeNode {
        name: "B".to_string(),
        next: RefCell::new(Some(Rc::downgrade(&a))),  // weak ref to A
    });
    *a.next.borrow_mut() = Some(Rc::downgrade(&b));   // weak ref to B

    println!("  A strong={} weak={}", Rc::strong_count(&a),
             Rc::weak_count(&a));
    println!("  B strong={} weak={}", Rc::strong_count(&b),
             Rc::weak_count(&b));

    println!("  -- end of scope --");
    // a dropped (strong_count 1->0), b dropped (strong_count 1->0)
    // Weak references don't prevent drop!
}

/* ============================================================
 *  Demo 8: Leak accumulation — why Rust servers don't leak
 * ============================================================ */
fn demo8() {
    println!("\n=== Demo 8: Why Rust servers don't leak ===");

    // Simulate request handling
    fn handle_request(id: u32) {
        let _header = format!("Request-{}", id);
        let _body = vec![0u8; 64];
        let _parsed: Vec<&str> = _header.split('-').collect();
        // ALL dropped at end of function — zero leak per request
    }

    let num_requests = 1000;
    for i in 0..num_requests {
        handle_request(i);
    }

    println!("  Processed {} requests.", num_requests);
    println!("  Memory leaked per request: 0 bytes");
    println!("  Total leaked after {} requests: 0 bytes", num_requests);
    println!();

    // C equivalent would leak if any allocation missed a free()
    println!("  In C (with the bug from Demo 6 section 2):");
    let leak_per_req = 64;
    println!("  {} requests * {} bytes/leak = {} bytes leaked",
             num_requests, leak_per_req,
             num_requests * leak_per_req);
    println!();

    println!("  Summary of leak prevention:");
    println!("  +----------------------------+-------+-------+");
    println!("  | Pattern                    |   C   | Rust  |");
    println!("  +----------------------------+-------+-------+");
    println!("  | List destroy               | leak  | safe  |");
    println!("  | Tree destroy               | leak  | safe  |");
    println!("  | Struct field destroy        | leak  | safe  |");
    println!("  | Pointer reassignment       | leak  | safe  |");
    println!("  | Early return               | leak  | safe  |");
    println!("  | Rc/Arc cycles              | N/A   | LEAK  |");
    println!("  | mem::forget()              | N/A   | LEAK  |");
    println!("  +----------------------------+-------+-------+");
}

fn main() {
    demo1();
    demo2();
    demo3();
    demo4();
    demo5();
    demo6();
    demo7();
    demo8();
}
```

### Compilar y ejecutar

```bash
rustc -o leaks_rs leaks.rs
./leaks_rs
```

### Salida esperada (extracto)

```
=== Demo 4: Reassignment drops old value ===
  [new]  first_alloc
  buf = first_alloc
  [new]  second_alloc
  [drop] first_alloc
  buf = second_alloc (first_alloc was auto-dropped)
  -- end of scope --
  [drop] second_alloc

=== Demo 6: Rc cycle — leak in safe Rust ===
  A strong_count=2
  B strong_count=2
  -- dropping local variables a, b --
  Notice: NO [drop] messages! A and B leaked.

=== Demo 7: Rc cycle fixed with Weak ===
  A strong=1 weak=1
  B strong=1 weak=1
  -- end of scope --
  [drop] SafeNode(B)
  [drop] SafeNode(A)
```

---

## 8. Ejercicios

### Ejercicio 1: Contar bytes leakeados

Dada esta lista enlazada en C, cuántos bytes se pierden si solo
se libera la cabeza?

```c
typedef struct Node { int data; struct Node *next; } Node;
// Lista: 1 -> 2 -> 3 -> 4 -> 5 -> NULL
// sizeof(Node) = 16 bytes (en 64-bit con padding)
free(head);
```

<details><summary>Prediccion</summary>

La lista tiene 5 nodos. `free(head)` libera solo el primer nodo (1).
Los nodos 2, 3, 4 y 5 se pierden.

Bytes leakeados: 4 nodos × 16 bytes = **64 bytes**.

Además, `malloc` almacena metadata por bloque (típicamente 8-16 bytes
adicionales). El leak real incluyendo metadata de malloc podría ser
4 × (16 + 16) = **128 bytes**.

Con Valgrind veriamos:
```
definitely lost: 64 bytes in 4 blocks
```

</details>

### Ejercicio 2: Árbol — preorden vs postorden

Dado este árbol, qué pasa si se libera en preorden?

```
       A
      / \
     B   C
    /
   D
```

<details><summary>Prediccion</summary>

**Preorden** (A, B, D, C): `free(A)` primero.

Problema: después de `free(A)`, acceder a `A->left` (para encontrar B)
y `A->right` (para encontrar C) es **use-after-free** (undefined
behavior). Los punteros a B y C se pierden.

Si guardamos los punteros antes de liberar A:
```c
TNode *l = A->left, *r = A->right;
free(A);
```
Esto evita el UB pero es frágil y propenso a errores.

**Postorden** (D, B, C, A): libera hojas primero, luego padres. Cuando
se libera cada nodo, sus hijos ya fueron liberados. No hay
use-after-free ni leaks.

Bytes leakeados con preorden naive (sin guardar punteros): 3 nodos
(B, C, D) × sizeof(TNode).

</details>

### Ejercicio 3: Detectar el leak

Identifica el leak en este código:

```c
char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char *buf = malloc(4096);
    if (!buf) return NULL;  // <-- aquí
    fread(buf, 1, 4096, f);
    fclose(f);
    return buf;
}
```

<details><summary>Prediccion</summary>

Si `malloc(4096)` falla (retorna NULL), la función retorna NULL
**sin cerrar el archivo** `f`. El `FILE *f` queda abierto — es un
**resource leak** (file descriptor leak).

Fix:
```c
if (!buf) {
    fclose(f);
    return NULL;
}
```

O con el patrón goto cleanup:
```c
char *buf = NULL;
FILE *f = fopen(path, "r");
if (!f) goto cleanup;
buf = malloc(4096);
if (!buf) goto cleanup;
fread(buf, 1, 4096, f);
cleanup:
    if (f) fclose(f);
    return buf;
```

En Rust, `File` implementa Drop, así que se cerraría automáticamente
al salir del scope, sin importar el path de ejecución.

</details>

### Ejercicio 4: Goto cleanup order

En el patrón goto cleanup, por qué se inicializan los punteros a NULL
y se liberan en **orden inverso**?

<details><summary>Prediccion</summary>

**Inicializar a NULL**: porque `free(NULL)` es un no-op seguro según
el estándar C. Si un `goto cleanup` salta antes de que un puntero
sea asignado, intentar `free()` de una variable no inicializada
causaría **undefined behavior**. Con NULL, el free es seguro.

**Orden inverso**: los recursos se liberan en LIFO (último
adquirido, primero liberado) porque recursos posteriores pueden
depender de los anteriores. Ejemplo:
```c
f = fopen(...);
buf = malloc(...);
// buf puede contener datos leídos de f
// liberar f antes de buf podría ser problemático
// si buf aún referencia datos de f
```

Liberar en orden inverso es el equivalente manual del orden de Drop
en Rust (LIFO) y del stack unwinding.

</details>

### Ejercicio 5: Rust reassignment

```rust
let mut v = vec![1, 2, 3];
v = vec![4, 5, 6];
v = vec![7, 8, 9];
```

Cuántas veces se llama Drop? Sobre cuáles valores?

<details><summary>Prediccion</summary>

**Tres veces** en total:

1. Cuando `v = vec![4, 5, 6]`: el Vec `[1,2,3]` es droppeado.
2. Cuando `v = vec![7, 8, 9]`: el Vec `[4,5,6]` es droppeado.
3. Al final del scope: el Vec `[7,8,9]` es droppeado.

Cada reasignación droppea el valor anterior automáticamente. En C,
cada reasignación de puntero sin `free()` previo sería un leak.
Rust hace el `free` implícito.

</details>

### Ejercicio 6: mem::forget

```rust
let v = vec![1, 2, 3, 4, 5];
std::mem::forget(v);
```

Es unsafe? Cuánta memoria se pierde?

<details><summary>Prediccion</summary>

`mem::forget()` es **safe** — no es `unsafe`. Los leaks no son
undefined behavior en Rust (no causan dangling pointers, data races,
ni corrupción de memoria).

Memoria perdida:
- Heap: 5 × 4 bytes (i32) = **20 bytes** de datos.
- Heap: metadata de la asignación de malloc (implementation-defined).
- Stack: la estructura Vec (ptr, len, capacity = 24 bytes) se "olvida"
  sin liberar el heap al que apunta.

El Vec en el stack desaparece (sin Drop), pero los 20 bytes en el
heap nunca se liberan. Es un leak intencional.

Uso legítimo de `mem::forget`: interfacing con FFI donde un sistema
externo toma ownership de la memoria (le pasamos el puntero a C y C
se encargará de liberarlo).

</details>

### Ejercicio 7: Indirectly lost

Valgrind reporta:

```
definitely lost: 16 bytes in 1 blocks
indirectly lost: 48 bytes in 3 blocks
```

Qué estructura de datos podría causar esto?

<details><summary>Prediccion</summary>

**definitely lost**: 1 bloque de 16 bytes cuyo puntero se perdió.
**indirectly lost**: 3 bloques de 16 bytes cada uno que son accesibles
**solo** desde el bloque definitely lost.

Estructura probable: una **lista enlazada** de 4 nodos (16 bytes cada
uno) donde se perdió el puntero a la cabeza:

```
head (perdido) -> [N1: 16B] -> [N2: 16B] -> [N3: 16B] -> [N4: 16B]
                   ^^^^^^^^     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                   definitely    indirectly lost (solo accesibles
                   lost          desde N1, que ya está lost)
```

Otra posibilidad: un struct de 16 bytes con tres campos apuntando
a bloques de 16 bytes cada uno (struct sin liberar campos).

"Indirectly lost" significa: "si liberas el bloque definitely lost
y sigues sus punteros, encontrarás estos bloques."

</details>

### Ejercicio 8: Servidor con leak gradual

Un servidor procesa 500 req/s. Cada request leak 128 bytes. El
servidor tiene 8 GB de RAM. Cuánto tiempo hasta OOM?

<details><summary>Prediccion</summary>

Leak rate: 500 × 128 = 64,000 bytes/s = 62.5 KB/s.

62.5 KB/s × 60 = 3,750 KB/min ≈ 3.66 MB/min.

3.66 MB/min × 60 = 219.7 MB/hora.

8 GB / 219.7 MB/hora ≈ **37.3 horas** ≈ 1.5 días.

El servidor crashea en menos de 2 días. Si el proceso ya usa 2 GB
de memoria base, queda 6 GB: 6000 / 219.7 ≈ **27.3 horas**.

En producción, el OOM killer de Linux probablemente mate el proceso
antes de alcanzar 8 GB completos. Con monitoring, se vería un
crecimiento lineal de RSS (Resident Set Size) — señal clásica de leak.

</details>

### Ejercicio 9: Rc cycle detection

Cómo detectarías un Rc cycle leak en un programa Rust?

<details><summary>Prediccion</summary>

Opciones:

1. **Observar strong_count**: si después de drop de las variables
   locales, el `strong_count` no llega a 0, hay un ciclo. Pero no
   puedes verificar después del drop (la variable ya no existe).

2. **Custom Drop con log**: implementar Drop y verificar que se
   llama para cada objeto. Si Drop no se ejecuta, hay leak.

3. **Valgrind/ASan**: correr el programa bajo Valgrind o con
   LeakSanitizer. Reportará los bloques no liberados.

4. **Herramientas de heap profiling**: `heaptrack`, `massif`
   (Valgrind) muestran el crecimiento de memoria over time.

5. **Diseño preventivo**: evitar `Rc` cycles por construcción. Usar
   `Weak` para back-edges. Mejor aún, usar arenas o indices
   (generational indices de `slotmap`) en lugar de `Rc` para grafos.

Rust no tiene un detector de Rc cycles en la stdlib. La prevención
por diseño es la estrategia recomendada.

</details>

### Ejercicio 10: C vs Rust — quién necesita más disciplina?

Para un programa que maneja 20 tipos de structs con campos dinámicos,
cuántas funciones `_destroy()` necesitas en C? Cuántas en Rust?

<details><summary>Prediccion</summary>

**En C**: necesitas **20 funciones `_destroy()`**, una por tipo. Cada
una debe liberar todos los campos dinámicos del struct en el orden
correcto. Si un struct contiene otro struct con campos dinámicos,
la destroy del padre debe llamar la destroy del hijo.

Además necesitas documentar la ownership de cada puntero (quién es
responsable de liberarlo) y verificar manualmente que cada path de
ejecución (incluyendo error paths) llame a la destroy correcta.

**En Rust**: necesitas **0 funciones Drop** en la mayoría de los
casos. El compilador genera **drop glue** automáticamente para cada
struct, liberando todos los campos que implementan Drop (String, Vec,
Box, etc.).

Solo necesitas implementar `Drop` manualmente si el struct tiene
lógica de cleanup especial (cerrar conexiones, flush buffers,
logging, etc.). Para la pura liberación de memoria, Rust lo hace
automáticamente.

20 funciones manuales vs 0 automáticas: cada función manual es una
oportunidad para un bug. Esto escala — un proyecto grande con 200
tipos necesita 200 destructores manuales en C, todos correctos.

</details>
