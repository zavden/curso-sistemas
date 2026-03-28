# T04 — Testing de estructuras de datos

---

## 1. Por qué las estructuras de datos necesitan testing especial

Un TAD no es una función pura: tiene **estado interno** que cambia con cada
operación. Un bug puede no manifestarse en la primera operación, sino después
de una secuencia específica de inserts y deletes que deja la estructura en un
estado corrupto.

```
Función pura:    f(x) → y           Probar una vez con cada input relevante
TAD:             op₁ → op₂ → op₃    El resultado de op₃ depende del
                                     estado acumulado por op₁ y op₂
```

Los tipos de bugs en estructuras de datos:

| Tipo | Ejemplo | Cuándo se manifiesta |
|------|---------|---------------------|
| Invariante rota | BST desordenado tras delete | Después de una secuencia específica |
| Leak | Nodos huérfanos tras remove | Al destruir la estructura |
| Corrupción | `next` apunta a nodo liberado | Operaciones posteriores crashean |
| Off-by-one | Size reporta $n$ pero hay $n-1$ nodos | Acumulativo tras múltiples ops |
| Caso borde | Insert en lista vacía no actualiza tail | Solo con la primera operación |

---

## 2. Tests unitarios para TADs en C

### Patrón básico: setup → operate → assert → teardown

```c
// test_list.c
#include <assert.h>
#include <stdio.h>
#include "list.h"

void test_create_empty(void) {
    List *list = list_create();
    assert(list != NULL);
    assert(list_is_empty(list));
    assert(list_size(list) == 0);
    list_destroy(list);
    printf("  PASS: test_create_empty\n");
}

void test_push_front_single(void) {
    List *list = list_create();
    list_push_front(list, 42);
    assert(!list_is_empty(list));
    assert(list_size(list) == 1);
    assert(list_front(list) == 42);
    list_destroy(list);
    printf("  PASS: test_push_front_single\n");
}

void test_push_pop_order(void) {
    List *list = list_create();
    list_push_front(list, 10);
    list_push_front(list, 20);
    list_push_front(list, 30);
    // Stack order: 30 → 20 → 10
    assert(list_pop_front(list) == 30);
    assert(list_pop_front(list) == 20);
    assert(list_pop_front(list) == 10);
    assert(list_is_empty(list));
    list_destroy(list);
    printf("  PASS: test_push_pop_order\n");
}

int main(void) {
    printf("List tests:\n");
    test_create_empty();
    test_push_front_single();
    test_push_pop_order();
    printf("All tests passed.\n");
    return 0;
}
```

### Compilar y ejecutar con sanitizers

```bash
gcc -g -O0 -fsanitize=address,undefined -o test_list test_list.c list.c
./test_list
```

Cada test que pasa sin que ASan reporte errores confirma dos cosas:
la lógica es correcta **y** la memoria está bien gestionada.

### El patrón `test_` + `assert` + `destroy`

```c
void test_NOMBRE(void) {
    // 1. Setup: crear la estructura
    TAD *t = tad_create();

    // 2. Operate: ejecutar las operaciones a probar
    tad_insert(t, value);

    // 3. Assert: verificar el resultado esperado
    assert(tad_contains(t, value));
    assert(tad_size(t) == 1);

    // 4. Teardown: destruir la estructura (detecta leaks con ASan)
    tad_destroy(t);
}
```

**Siempre destruir** al final del test. Si se omite, ASan/Valgrind reportan
leak. Si el test crashea antes del destroy, el crash mismo indica el bug.

---

## 3. Casos borde que siempre hay que probar

Para cualquier estructura de datos, estos son los casos borde que más
frecuentemente revelan bugs:

### La lista crítica

```c
// 1. Estructura vacía
void test_empty_operations(void) {
    List *list = list_create();
    assert(list_is_empty(list));
    assert(list_size(list) == 0);
    // pop de lista vacía — ¿retorna error o crashea?
    list_destroy(list);
}

// 2. Un solo elemento
void test_single_element(void) {
    List *list = list_create();
    list_push_front(list, 42);
    assert(list_size(list) == 1);
    assert(list_pop_front(list) == 42);
    assert(list_is_empty(list));
    // ¿La lista quedó en estado consistente después de remover el único?
    list_destroy(list);
}

// 3. Insertar y remover hasta vacío
void test_fill_and_drain(void) {
    List *list = list_create();
    for (int i = 0; i < 100; i++)
        list_push_front(list, i);
    assert(list_size(list) == 100);
    for (int i = 0; i < 100; i++)
        list_pop_front(list);
    assert(list_is_empty(list));
    assert(list_size(list) == 0);
    // ¿Se puede re-usar la lista después de drenarla?
    list_push_front(list, 999);
    assert(list_size(list) == 1);
    list_destroy(list);
}

// 4. Valores extremos
void test_extreme_values(void) {
    List *list = list_create();
    list_push_front(list, 0);
    list_push_front(list, -1);
    list_push_front(list, __INT_MAX__);
    list_push_front(list, -__INT_MAX__ - 1);  // INT_MIN
    assert(list_size(list) == 4);
    list_destroy(list);
}

// 5. Doble destrucción (si destroy es NULL-safe)
void test_double_destroy(void) {
    List *list = list_create();
    list_push_front(list, 1);
    list_destroy(list);
    // list_destroy(NULL) debería ser seguro
    list_destroy(NULL);
}
```

### Checklist universal de casos borde

| Caso | Qué probar |
|------|-----------|
| Vacío | Todas las operaciones sobre estructura vacía |
| Un elemento | Insert, remove, search del único elemento |
| Llenar y vaciar | Re-uso después de drenar completamente |
| Duplicados | Insert del mismo valor múltiples veces |
| Valores extremos | `0`, `-1`, `INT_MAX`, `INT_MIN`, `NULL` |
| Orden de ops | Insert-remove-insert, remove del primero/último/medio |
| Destrucción | Destruir con 0, 1, $n$ elementos |

---

## 4. Verificación de invariantes

Un **invariante** es una propiedad que debe ser verdadera después de cada
operación pública del TAD. Verificarla automáticamente después de cada
operación atrapa bugs inmediatamente.

### Invariantes comunes

| Estructura | Invariante |
|-----------|-----------|
| Lista | `size` == número real de nodos, último nodo tiene `next == NULL` |
| BST | Todo nodo: `left->data < data < right->data` |
| AVL | BST + $\|balance(node)\| \leq 1$ para todo nodo |
| Heap | Todo nodo: `parent->data <= child->data` (min-heap) |
| Hash table | `size <= capacity * max_load_factor` |
| Stack | `size >= 0`, `top` es el último pushado |

### Implementar un verificador de invariantes

```c
// list_check.c — verificación de invariantes para List

#include <assert.h>
#include <stdbool.h>
#include "list.h"

// Verificar que la lista es internamente consistente
bool list_check_invariants(const List *list) {
    if (!list) return false;

    // Invariante 1: size >= 0
    if (list->size < 0) return false;

    // Invariante 2: contar nodos == size
    int count = 0;
    Node *cur = list->head;
    while (cur) {
        count++;
        if (count > list->size + 1) return false;  // protección contra ciclo
        cur = cur->next;
    }
    if (count != list->size) return false;

    // Invariante 3: si size == 0, head debe ser NULL
    if (list->size == 0 && list->head != NULL) return false;

    // Invariante 4: si size > 0, head no debe ser NULL
    if (list->size > 0 && list->head == NULL) return false;

    return true;
}
```

### Usar el verificador en tests

```c
void test_push_pop_invariants(void) {
    List *list = list_create();
    assert(list_check_invariants(list));     // vacía

    list_push_front(list, 10);
    assert(list_check_invariants(list));     // después de push

    list_push_front(list, 20);
    assert(list_check_invariants(list));     // después de segundo push

    list_pop_front(list);
    assert(list_check_invariants(list));     // después de pop

    list_pop_front(list);
    assert(list_check_invariants(list));     // vacía de nuevo

    list_destroy(list);
    printf("  PASS: test_push_pop_invariants\n");
}
```

### Verificador de invariantes para BST

```c
// Verificar que un BST está correctamente ordenado
bool bst_check_order(const TreeNode *node, int min, int max) {
    if (!node) return true;
    if (node->data <= min || node->data >= max) return false;
    return bst_check_order(node->left, min, node->data) &&
           bst_check_order(node->right, node->data, max);
}

bool bst_check_invariants(const BST *bst) {
    if (!bst) return false;
    // Invariante BST: todo nodo cumple min < data < max
    return bst_check_order(bst->root, INT_MIN, INT_MAX);
}
```

### Modo debug con verificación automática

```c
#ifdef DEBUG_INVARIANTS
#define CHECK_LIST(l) assert(list_check_invariants(l))
#else
#define CHECK_LIST(l) ((void)0)
#endif

void list_push_front(List *list, int data) {
    assert(list);
    Node *node = malloc(sizeof(Node));
    if (!node) return;
    node->data = data;
    node->next = list->head;
    list->head = node;
    list->size++;
    CHECK_LIST(list);     // verificar después de cada operación
}
```

```bash
# En desarrollo: verificación activada
gcc -g -O0 -DDEBUG_INVARIANTS -o test_list test_list.c list.c

# En producción: verificación desactivada (costo cero)
gcc -O2 -o program main.c list.c
```

---

## 5. Tests de estrés

Los tests unitarios prueban secuencias cortas y predecibles. Los tests de
estrés prueban secuencias **largas y aleatorias** para encontrar bugs que
solo aparecen con volumen o combinaciones inesperadas.

### Test de estrés básico

```c
#include <stdlib.h>
#include <time.h>

void test_stress_list(void) {
    srand(42);   // semilla fija para reproducibilidad
    List *list = list_create();

    for (int i = 0; i < 100000; i++) {
        int op = rand() % 3;
        switch (op) {
            case 0:  // push
            case 1:
                list_push_front(list, rand() % 1000);
                break;
            case 2:  // pop (si no está vacía)
                if (!list_is_empty(list))
                    list_pop_front(list);
                break;
        }
        // Verificar invariantes periódicamente (no cada iteración → lento)
        if (i % 1000 == 0)
            assert(list_check_invariants(list));
    }

    assert(list_check_invariants(list));
    list_destroy(list);
    printf("  PASS: test_stress_list (100K ops)\n");
}
```

### Test de estrés con modelo de referencia (oracle)

La técnica más poderosa: ejecutar las mismas operaciones en la estructura
bajo test **y** en una implementación de referencia confiable, y comparar
resultados:

```c
void test_stress_with_oracle(void) {
    srand(42);
    List *list = list_create();     // estructura bajo test

    // Oracle: array simple (implementación trivial pero correcta)
    int oracle[200000];
    int oracle_size = 0;

    for (int i = 0; i < 100000; i++) {
        int op = rand() % 3;
        int val = rand() % 1000;

        switch (op) {
            case 0:
            case 1: {  // push
                list_push_front(list, val);
                // Oracle: insertar al inicio (shift)
                for (int j = oracle_size; j > 0; j--)
                    oracle[j] = oracle[j-1];
                oracle[0] = val;
                oracle_size++;
                break;
            }
            case 2: {  // pop
                if (oracle_size == 0) break;
                int list_val = list_pop_front(list);
                int oracle_val = oracle[0];
                // Oracle: remover del inicio (shift)
                for (int j = 0; j < oracle_size - 1; j++)
                    oracle[j] = oracle[j+1];
                oracle_size--;
                // COMPARAR
                assert(list_val == oracle_val);
                break;
            }
        }

        // Verificar tamaño
        assert(list_size(list) == oracle_size);
    }

    list_destroy(list);
    printf("  PASS: test_stress_with_oracle (100K ops)\n");
}
```

Si hay una discrepancia entre la lista y el oracle, el `assert` falla y
la semilla (`srand(42)`) permite reproducir exactamente la secuencia que
causó el bug.

### Tips para tests de estrés

| Práctica | Razón |
|----------|-------|
| Semilla fija (`srand(42)`) | Reproducibilidad: el mismo seed produce la misma secuencia |
| Operaciones mixtas | Más probabilidad de encontrar bugs de interacción |
| Verificar invariantes | Atrapar corrupción temprano |
| Más pushes que pops | Evitar que la estructura esté vacía todo el tiempo |
| Probar también con la estructura llena | Detectar bugs de realloc/resize |
| Compilar con ASan | Detectar problemas de memoria durante el stress |

---

## 6. Tests unitarios en Rust

Rust tiene un framework de testing integrado. No se necesitan bibliotecas
externas para tests básicos:

```rust
// src/lib.rs

pub struct Stack<T> {
    data: Vec<T>,
}

impl<T> Stack<T> {
    pub fn new() -> Self {
        Stack { data: Vec::new() }
    }

    pub fn push(&mut self, value: T) {
        self.data.push(value);
    }

    pub fn pop(&mut self) -> Option<T> {
        self.data.pop()
    }

    pub fn peek(&self) -> Option<&T> {
        self.data.last()
    }

    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    pub fn len(&self) -> usize {
        self.data.len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new_is_empty() {
        let s: Stack<i32> = Stack::new();
        assert!(s.is_empty());
        assert_eq!(s.len(), 0);
    }

    #[test]
    fn test_push_pop_single() {
        let mut s = Stack::new();
        s.push(42);
        assert!(!s.is_empty());
        assert_eq!(s.len(), 1);
        assert_eq!(s.pop(), Some(42));
        assert!(s.is_empty());
    }

    #[test]
    fn test_lifo_order() {
        let mut s = Stack::new();
        s.push(1);
        s.push(2);
        s.push(3);
        assert_eq!(s.pop(), Some(3));
        assert_eq!(s.pop(), Some(2));
        assert_eq!(s.pop(), Some(1));
        assert_eq!(s.pop(), None);
    }

    #[test]
    fn test_peek_does_not_remove() {
        let mut s = Stack::new();
        s.push(10);
        assert_eq!(s.peek(), Some(&10));
        assert_eq!(s.len(), 1);    // peek no remueve
        assert_eq!(s.pop(), Some(10));
    }

    #[test]
    fn test_pop_empty() {
        let mut s: Stack<i32> = Stack::new();
        assert_eq!(s.pop(), None);
    }
}
```

```bash
cargo test
```

```
running 5 tests
test tests::test_new_is_empty ... ok
test tests::test_push_pop_single ... ok
test tests::test_lifo_order ... ok
test tests::test_peek_does_not_remove ... ok
test tests::test_pop_empty ... ok

test result: ok. 5 passed; 0 filtered out
```

### Assertions útiles en Rust

```rust
assert!(condition);                        // panics si false
assert_eq!(left, right);                   // panics si left != right
assert_ne!(left, right);                   // panics si left == right
assert!(cond, "msg: {}", detail);          // con mensaje personalizado
assert_eq!(v, expected, "after inserting {}", val);
```

---

## 7. Property-based testing con proptest

Los tests unitarios prueban valores específicos. **Property-based testing**
genera valores aleatorios y verifica que una **propiedad** se cumpla para
todos ellos.

### Concepto

```
Test unitario:     push(42); pop() == 42    → "funciona para 42"
Property test:     ∀x: push(x); pop() == x  → "funciona para TODOS los x"
```

### Proptest en Rust

```toml
# Cargo.toml
[dev-dependencies]
proptest = "1"
```

```rust
#[cfg(test)]
mod proptests {
    use super::*;
    use proptest::prelude::*;

    // Propiedad 1: push(x) seguido de pop() devuelve x
    proptest! {
        #[test]
        fn push_pop_identity(x in any::<i32>()) {
            let mut s = Stack::new();
            s.push(x);
            assert_eq!(s.pop(), Some(x));
        }
    }

    // Propiedad 2: después de n pushes, len() == n
    proptest! {
        #[test]
        fn push_increases_len(values in prop::collection::vec(any::<i32>(), 0..100)) {
            let mut s = Stack::new();
            for (i, &v) in values.iter().enumerate() {
                s.push(v);
                assert_eq!(s.len(), i + 1);
            }
        }
    }

    // Propiedad 3: LIFO — pop devuelve en orden inverso al push
    proptest! {
        #[test]
        fn lifo_order(values in prop::collection::vec(any::<i32>(), 0..100)) {
            let mut s = Stack::new();
            for &v in &values {
                s.push(v);
            }
            for &v in values.iter().rev() {
                assert_eq!(s.pop(), Some(v));
            }
            assert_eq!(s.pop(), None);
        }
    }

    // Propiedad 4: push luego drain deja el stack vacío
    proptest! {
        #[test]
        fn drain_leaves_empty(values in prop::collection::vec(any::<i32>(), 0..100)) {
            let mut s = Stack::new();
            for &v in &values {
                s.push(v);
            }
            for _ in 0..values.len() {
                s.pop();
            }
            assert!(s.is_empty());
        }
    }
}
```

### Qué pasa cuando falla

Si proptest encuentra un contraejemplo, lo **reduce** (shrinking) al caso
más simple que reproduce el bug:

```
test proptests::lifo_order ... FAILED

thread 'proptests::lifo_order' panicked at 'assertion failed:
`(left == right)`
  left: `Some(0)`,
 right: `Some(1)`'

proptest: Minimal failing input: values = [0, 1]
```

Proptest probó cientos de vectores aleatorios, encontró uno que falla, y lo
redujo a `[0, 1]` — el caso más simple que reproduce el bug.

### Propiedades comunes para estructuras de datos

| Propiedad | Aplica a |
|-----------|---------|
| Push-pop identity: `push(x); pop() == x` | Stack, queue, deque |
| LIFO/FIFO order | Stack (LIFO), queue (FIFO) |
| Insert-contains: `insert(x); contains(x) == true` | Set, map, BST |
| Remove-not-contains: `remove(x); contains(x) == false` | Set, map, BST |
| Size consistency: `n inserts → size == n` | Todos |
| Idempotencia: `insert(x); insert(x); size no cambia` | Set (no multiset) |
| Equivalencia con oracle: `ops(DS) == ops(Vec)` | Todos |

---

## 8. Test de modelo (oracle) en Rust con proptest

La técnica más poderosa: comparar la estructura bajo test con una
implementación de referencia trivial:

```rust
use proptest::prelude::*;
use std::collections::HashSet;

// Nuestra implementación de Set (bajo test)
pub struct MySet {
    data: Vec<i32>,
}

impl MySet {
    pub fn new() -> Self { MySet { data: Vec::new() } }

    pub fn insert(&mut self, val: i32) -> bool {
        if self.data.contains(&val) { return false; }
        self.data.push(val);
        true
    }

    pub fn contains(&self, val: &i32) -> bool {
        self.data.contains(val)
    }

    pub fn remove(&mut self, val: &i32) -> bool {
        if let Some(pos) = self.data.iter().position(|x| x == val) {
            self.data.swap_remove(pos);
            true
        } else {
            false
        }
    }

    pub fn len(&self) -> usize { self.data.len() }
}

// Operaciones que se pueden aplicar a un Set
#[derive(Debug, Clone)]
enum SetOp {
    Insert(i32),
    Remove(i32),
    Contains(i32),
}

prop_compose! {
    fn arb_set_op()(op in 0..3i32, val in -100..100i32) -> SetOp {
        match op {
            0 => SetOp::Insert(val),
            1 => SetOp::Remove(val),
            _ => SetOp::Contains(val),
        }
    }
}

proptest! {
    #[test]
    fn set_matches_oracle(ops in prop::collection::vec(arb_set_op(), 0..200)) {
        let mut my_set = MySet::new();
        let mut oracle = HashSet::new();   // implementación de referencia

        for op in &ops {
            match op {
                SetOp::Insert(v) => {
                    let my_result = my_set.insert(*v);
                    let oracle_result = oracle.insert(*v);
                    assert_eq!(my_result, oracle_result,
                        "insert({}) diverged", v);
                }
                SetOp::Remove(v) => {
                    let my_result = my_set.remove(v);
                    let oracle_result = oracle.remove(v);
                    assert_eq!(my_result, oracle_result,
                        "remove({}) diverged", v);
                }
                SetOp::Contains(v) => {
                    let my_result = my_set.contains(v);
                    let oracle_result = oracle.contains(v);
                    assert_eq!(my_result, oracle_result,
                        "contains({}) diverged", v);
                }
            }
            // Invariante: mismos tamaños siempre
            assert_eq!(my_set.len(), oracle.len(),
                "size diverged after {:?}", op);
        }
    }
}
```

Si proptest encuentra una secuencia de operaciones donde `MySet` y
`HashSet` divergen, la reduce al caso mínimo. Esto ha encontrado bugs
en implementaciones que pasaron cientos de tests unitarios.

---

## 9. Organizar un suite de tests en C

### Estructura de archivos

```
project/
├── src/
│   ├── list.h
│   ├── list.c
│   ├── bst.h
│   └── bst.c
├── tests/
│   ├── test_list.c
│   ├── test_bst.c
│   └── test_main.c
└── Makefile
```

### Framework mínimo de testing

```c
// tests/test_framework.h
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  %-40s ", #name); \
    name(); \
    tests_passed++; \
    printf("PASS\n"); \
} while (0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while (0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAIL\n    %s:%d: %s == %s (%d != %d)\n", \
               __FILE__, __LINE__, #a, #b, (int)(a), (int)(b)); \
        tests_failed++; \
        return; \
    } \
} while (0)

#define TEST_SUMMARY() do { \
    printf("\n%d/%d tests passed", tests_passed, tests_run); \
    if (tests_failed) printf(", %d FAILED", tests_failed); \
    printf("\n"); \
} while (0)

#endif
```

### Uso del framework

```c
// tests/test_list.c
#include "test_framework.h"
#include "list.h"

void test_create(void) {
    List *l = list_create();
    ASSERT(l != NULL);
    ASSERT_EQ(list_size(l), 0);
    list_destroy(l);
}

void test_push_pop(void) {
    List *l = list_create();
    list_push_front(l, 10);
    list_push_front(l, 20);
    ASSERT_EQ(list_pop_front(l), 20);
    ASSERT_EQ(list_pop_front(l), 10);
    ASSERT(list_is_empty(l));
    list_destroy(l);
}

void run_list_tests(void) {
    printf("List tests:\n");
    TEST(test_create);
    TEST(test_push_pop);
}
```

```c
// tests/test_main.c
#include "test_framework.h"

extern void run_list_tests(void);
extern void run_bst_tests(void);

int main(void) {
    run_list_tests();
    run_bst_tests();
    TEST_SUMMARY();
    return tests_failed > 0 ? 1 : 0;
}
```

### Makefile

```makefile
CC = gcc
CFLAGS = -g -O0 -Wall -Wextra -Itests -Isrc
ASAN_FLAGS = -fsanitize=address,undefined

SRC = src/list.c src/bst.c
TEST_SRC = tests/test_list.c tests/test_bst.c tests/test_main.c

test: test_runner
	./test_runner

test_runner: $(SRC) $(TEST_SRC)
	$(CC) $(CFLAGS) $(ASAN_FLAGS) -o $@ $^

test-valgrind: test_runner_nosan
	valgrind --leak-check=full --error-exitcode=1 ./test_runner_nosan

test_runner_nosan: $(SRC) $(TEST_SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f test_runner test_runner_nosan
```

---

## 10. Qué testear según la estructura

### Checklist por estructura

**Lista enlazada:**
- [ ] Create vacía → size 0, is_empty true
- [ ] Push front/back → size incrementa, element accesible
- [ ] Pop front/back → retorna correcto, size decrementa
- [ ] Remove medio → reconexión correcta
- [ ] Remove head → head se actualiza
- [ ] Remove tail → tail se actualiza (si aplica)
- [ ] Operaciones en lista vacía → no crashea
- [ ] Fill → drain → re-fill → funciona
- [ ] Stress: 100K operaciones mixtas + invariantes

**BST:**
- [ ] Insert → contains devuelve true
- [ ] Insert duplicado → comportamiento esperado
- [ ] Remove hoja, nodo con 1 hijo, nodo con 2 hijos
- [ ] Remove raíz
- [ ] Invariante BST después de cada operación
- [ ] In-order traversal produce secuencia ordenada
- [ ] Stress: 10K inserts aleatorios + verificar orden

**Hash table:**
- [ ] Insert → get devuelve el valor
- [ ] Insert con misma key → update del valor
- [ ] Remove → get devuelve NULL/None
- [ ] Resize/rehash mantiene todos los elementos
- [ ] Load factor no excede umbral
- [ ] Stress: 100K inserts + verificar todos accesibles

**Stack/Queue:**
- [ ] LIFO (stack) o FIFO (queue) order
- [ ] Push + pop identity
- [ ] Pop de vacío → None/error
- [ ] Stress: equivalencia con oracle (`Vec` o `VecDeque`)

---

## Ejercicios

### Ejercicio 1 — Tests unitarios básicos

Escribe 5 tests unitarios para una lista enlazada en C usando `assert`.
Cubre: crear vacía, push uno, push varios + pop en orden, fill + drain,
y operación en lista vacía.

**Prediccion**: ¿Cuál de los 5 tests es más probable que revele un bug
en una implementación nueva?

<details><summary>Respuesta</summary>

```c
#include <assert.h>
#include <stdio.h>
#include "list.h"

void test_create_empty(void) {
    List *l = list_create();
    assert(l != NULL);
    assert(list_is_empty(l));
    assert(list_size(l) == 0);
    list_destroy(l);
}

void test_push_one(void) {
    List *l = list_create();
    list_push_front(l, 42);
    assert(!list_is_empty(l));
    assert(list_size(l) == 1);
    assert(list_front(l) == 42);
    list_destroy(l);
}

void test_push_pop_order(void) {
    List *l = list_create();
    list_push_front(l, 1);
    list_push_front(l, 2);
    list_push_front(l, 3);
    assert(list_pop_front(l) == 3);
    assert(list_pop_front(l) == 2);
    assert(list_pop_front(l) == 1);
    list_destroy(l);
}

void test_fill_drain(void) {
    List *l = list_create();
    for (int i = 0; i < 1000; i++)
        list_push_front(l, i);
    assert(list_size(l) == 1000);
    for (int i = 0; i < 1000; i++)
        list_pop_front(l);
    assert(list_is_empty(l));
    list_push_front(l, 999);
    assert(list_size(l) == 1);
    list_destroy(l);
}

void test_pop_empty(void) {
    List *l = list_create();
    // Dependiendo del API: retorna -1, o assert falla
    // Probar que no crashea sin assert
    assert(list_is_empty(l));
    list_destroy(l);
}

int main(void) {
    test_create_empty();
    test_push_one();
    test_push_pop_order();
    test_fill_drain();
    test_pop_empty();
    printf("All tests passed.\n");
    return 0;
}
```

El test más probable de revelar un bug: **fill + drain + re-fill**.
Muchas implementaciones fallan al re-usar una estructura que fue vaciada
completamente (head no se resetea a NULL, o size queda inconsistente).

</details>

---

### Ejercicio 2 — Verificador de invariantes para BST

Escribe `bst_check_invariants` que verifique:
1. Propiedad BST (todo left < parent < right)
2. Size coincide con número real de nodos

```c
typedef struct TreeNode {
    int data;
    struct TreeNode *left, *right;
} TreeNode;

typedef struct {
    TreeNode *root;
    int size;
} BST;
```

**Prediccion**: ¿Necesitas pasar `min` y `max` como parámetros para
verificar la propiedad BST recursivamente?

<details><summary>Respuesta</summary>

Sí — sin `min`/`max`, solo verificar `left < parent && parent < right`
no es suficiente. Un nodo podría cumplir con su padre pero violar la
restricción de un ancestro más arriba.

```c
#include <limits.h>

// Contar nodos reales
static int count_nodes(const TreeNode *node) {
    if (!node) return 0;
    return 1 + count_nodes(node->left) + count_nodes(node->right);
}

// Verificar propiedad BST con rango permitido
static bool check_bst_range(const TreeNode *node, int min, int max) {
    if (!node) return true;
    if (node->data <= min || node->data >= max) return false;
    return check_bst_range(node->left, min, node->data) &&
           check_bst_range(node->right, node->data, max);
}

bool bst_check_invariants(const BST *bst) {
    if (!bst) return false;

    // Invariante 1: propiedad BST
    if (!check_bst_range(bst->root, INT_MIN, INT_MAX)) return false;

    // Invariante 2: size coincide con nodos reales
    int real_count = count_nodes(bst->root);
    if (real_count != bst->size) return false;

    // Invariante 3: coherencia de estado
    if (bst->size == 0 && bst->root != NULL) return false;
    if (bst->size > 0 && bst->root == NULL) return false;

    return true;
}
```

Ejemplo de bug que solo `min`/`max` detecta:

```
        10
       /  \
      5    15
     / \
    3   12     ← 12 > 10, viola la propiedad BST
                 (12 debería estar en el subárbol derecho de 10)

check left < parent:  12 > 5? Sí, pero...
check min/max:        min=5, max=10, 12 >= 10? → FALLA ✓
```

</details>

---

### Ejercicio 3 — Test de estrés con oracle

Escribe un test de estrés en C que compare una implementación de stack
(list-based) con un array simple como oracle. Ejecuta 50,000 operaciones
aleatorias (push/pop) y verifica que cada pop devuelve el mismo valor.

**Prediccion**: ¿Cuál es el beneficio de usar `srand(42)` en vez de
`srand(time(NULL))`?

<details><summary>Respuesta</summary>

```c
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "list.h"

void test_stress_oracle(void) {
    srand(42);     // semilla FIJA para reproducibilidad

    List *stack = list_create();

    // Oracle: array con top pointer
    int oracle[200000];
    int oracle_top = -1;

    for (int i = 0; i < 50000; i++) {
        int op = rand() % 3;   // 2/3 push, 1/3 pop
        if (op != 2) {
            // Push
            int val = rand() % 10000;
            list_push_front(stack, val);
            oracle[++oracle_top] = val;
        } else {
            // Pop (si no vacío)
            if (oracle_top >= 0) {
                int stack_val = list_pop_front(stack);
                int oracle_val = oracle[oracle_top--];
                assert(stack_val == oracle_val);
            }
        }
        assert(list_size(stack) == oracle_top + 1);
    }

    list_destroy(stack);
    printf("  PASS: test_stress_oracle (50K ops)\n");
}
```

Beneficio de `srand(42)`: **reproducibilidad**. Si el test falla:
- Con `srand(42)`: volver a ejecutar reproduce exactamente la misma
  secuencia → se puede depurar con GDB
- Con `srand(time(NULL))`: cada ejecución produce una secuencia distinta
  → el bug puede no reaparecer

Estrategia recomendada: desarrollar con semilla fija. En CI, probar con
varias semillas fijas (42, 123, 999) para mayor cobertura sin perder
reproducibilidad.

</details>

---

### Ejercicio 4 — Tests en Rust con `#[test]`

Escribe tests para un `Queue<T>` en Rust. Cubre: new, enqueue/dequeue
FIFO, peek, empty dequeue retorna `None`, y fill + drain.

```rust
pub struct Queue<T> {
    data: Vec<T>,
}

impl<T> Queue<T> {
    pub fn new() -> Self { Queue { data: Vec::new() } }
    pub fn enqueue(&mut self, val: T) { self.data.push(val); }
    pub fn dequeue(&mut self) -> Option<T> {
        if self.data.is_empty() { None }
        else { Some(self.data.remove(0)) }
    }
    pub fn peek(&self) -> Option<&T> { self.data.first() }
    pub fn len(&self) -> usize { self.data.len() }
    pub fn is_empty(&self) -> bool { self.data.is_empty() }
}
```

**Prediccion**: ¿Cuál es la complejidad de `dequeue` en esta implementación?
¿Afecta al testing?

<details><summary>Respuesta</summary>

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new() {
        let q: Queue<i32> = Queue::new();
        assert!(q.is_empty());
        assert_eq!(q.len(), 0);
    }

    #[test]
    fn test_fifo_order() {
        let mut q = Queue::new();
        q.enqueue(1);
        q.enqueue(2);
        q.enqueue(3);
        assert_eq!(q.dequeue(), Some(1));   // FIFO: primero en entrar
        assert_eq!(q.dequeue(), Some(2));
        assert_eq!(q.dequeue(), Some(3));
        assert_eq!(q.dequeue(), None);
    }

    #[test]
    fn test_peek() {
        let mut q = Queue::new();
        q.enqueue(42);
        assert_eq!(q.peek(), Some(&42));
        assert_eq!(q.len(), 1);   // peek no remueve
    }

    #[test]
    fn test_dequeue_empty() {
        let mut q: Queue<i32> = Queue::new();
        assert_eq!(q.dequeue(), None);
    }

    #[test]
    fn test_fill_drain() {
        let mut q = Queue::new();
        for i in 0..100 {
            q.enqueue(i);
        }
        assert_eq!(q.len(), 100);
        for i in 0..100 {
            assert_eq!(q.dequeue(), Some(i));
        }
        assert!(q.is_empty());
        q.enqueue(999);
        assert_eq!(q.len(), 1);
    }
}
```

`dequeue` usa `Vec::remove(0)` que es $O(n)$ — shift de todos los elementos.
Para testing no importa (los tests usan $n$ pequeño), pero para producción
se usaría `VecDeque` que tiene dequeue $O(1)$.

El testing no se ve afectado por la complejidad porque testea **correctitud**,
no rendimiento. Un test de estrés con $n = 100{,}000$ sí notaría la
diferencia: $O(n)$ dequeue haría el test $O(n^2)$ total.

</details>

---

### Ejercicio 5 — Property-based test con proptest

Escribe propiedades para un `Set<i32>` que verifiquen:
1. Insert + contains = true
2. Remove + contains = false
3. Insert duplicado no cambia el tamaño
4. Equivalencia con `HashSet` como oracle

**Prediccion**: ¿Cuántos casos generará proptest por default?

<details><summary>Respuesta</summary>

```rust
use proptest::prelude::*;
use std::collections::HashSet;

proptest! {
    #[test]
    fn insert_then_contains(x in any::<i32>()) {
        let mut s = MySet::new();
        s.insert(x);
        assert!(s.contains(&x));
    }

    #[test]
    fn remove_then_not_contains(x in any::<i32>()) {
        let mut s = MySet::new();
        s.insert(x);
        s.remove(&x);
        assert!(!s.contains(&x));
    }

    #[test]
    fn insert_duplicate_no_size_change(x in any::<i32>()) {
        let mut s = MySet::new();
        s.insert(x);
        let size_after_first = s.len();
        s.insert(x);
        assert_eq!(s.len(), size_after_first);
    }

    #[test]
    fn matches_hashset_oracle(
        ops in prop::collection::vec(
            (0..3i32, -100..100i32), 0..200
        )
    ) {
        let mut my_set = MySet::new();
        let mut oracle = HashSet::new();

        for (op, val) in &ops {
            match op % 3 {
                0 => { my_set.insert(*val); oracle.insert(*val); }
                1 => { my_set.remove(val); oracle.remove(val); }
                _ => {
                    assert_eq!(my_set.contains(val), oracle.contains(val));
                }
            }
            assert_eq!(my_set.len(), oracle.len());
        }
    }
}
```

Proptest genera **256 casos por default** por cada `proptest!` block.
Se puede configurar:

```rust
proptest! {
    #![proptest_config(ProptestConfig::with_cases(1000))]
    #[test]
    fn my_test(x in any::<i32>()) { /* ... */ }
}
```

O con variable de entorno:

```bash
PROPTEST_CASES=10000 cargo test
```

</details>

---

### Ejercicio 6 — Encontrar un bug con test de estrés

Esta implementación de `list_remove` tiene un bug sutil. Escribe un test
de estrés que lo encuentre:

```c
bool list_remove(List *list, int value) {
    if (!list || !list->head) return false;

    if (list->head->data == value) {
        Node *old = list->head;
        list->head = old->next;
        free(old);
        list->size--;
        return true;
    }

    Node *prev = list->head;
    while (prev->next) {
        if (prev->next->data == value) {
            Node *old = prev->next;
            prev->next = old->next;
            free(old);
            // BUG: ¿qué falta aquí?
            return true;
        }
        prev = prev->next;
    }
    return false;
}
```

**Prediccion**: ¿Cuál es el bug? ¿Qué tipo de test lo detectaría: unitario
o de estrés?

<details><summary>Respuesta</summary>

**Bug**: falta `list->size--` en el caso de remover del medio/final.
Solo se decrementa `size` cuando se remueve la cabeza.

Un test unitario simple puede detectarlo:

```c
void test_remove_middle(void) {
    List *l = list_create();
    list_push_front(l, 1);
    list_push_front(l, 2);
    list_push_front(l, 3);
    list_remove(l, 2);
    assert(list_size(l) == 2);   // FALLA: size es 3
    list_destroy(l);
}
```

Pero un test de estrés con verificación de invariantes lo encuentra
automáticamente:

```c
void test_stress_remove(void) {
    srand(42);
    List *l = list_create();
    int oracle_size = 0;

    for (int i = 0; i < 10000; i++) {
        int op = rand() % 3;
        int val = rand() % 100;
        if (op < 2) {
            list_push_front(l, val);
            oracle_size++;
        } else {
            if (list_remove(l, val)) oracle_size--;
        }
        assert(list_size(l) == oracle_size);   // FALLA eventualmente
    }
    list_destroy(l);
}
```

El test de estrés falla la primera vez que `list_remove` encuentra un
nodo que no es la cabeza. El assert `list_size(l) == oracle_size` detecta
la discrepancia inmediatamente.

**Ambos** tests lo detectan, pero el de estrés es más general: no necesitas
saber *dónde* está el bug para que lo encuentre.

</details>

---

### Ejercicio 7 — Testing con ASan + Valgrind pipeline

Escribe un Makefile que ejecute los tests de una lista enlazada en 3 modos:
normal, ASan, y Valgrind. Si cualquiera falla, el pipeline se detiene.

**Prediccion**: ¿Por qué no se puede ejecutar ASan y Valgrind sobre el
mismo binario?

<details><summary>Respuesta</summary>

ASan instrumenta el binario durante la compilación (inserta zonas rojas,
shadow memory). Valgrind ejecuta el binario en un emulador de CPU. Si se
ejecuta un binario instrumentado por ASan bajo Valgrind, ambos intentan
interceptar `malloc`/`free` simultáneamente → conflictos y falsos positivos.

```makefile
CC = gcc
CFLAGS = -g -O0 -Wall -Wextra
SRC = list.c
TEST = test_list.c

# Binario normal (para Valgrind)
test_normal: $(SRC) $(TEST)
	$(CC) $(CFLAGS) -o $@ $^

# Binario con ASan (para ASan)
test_asan: $(SRC) $(TEST)
	$(CC) $(CFLAGS) -fsanitize=address,undefined -o $@ $^

# Pipeline completo
test: test_normal test_asan
	@echo "=== ASan ==="
	./test_asan
	@echo "=== Valgrind ==="
	valgrind --leak-check=full --error-exitcode=1 ./test_normal
	@echo "=== All passed ==="

clean:
	rm -f test_normal test_asan
```

Dos compilaciones, dos binarios, dos ejecuciones — cada herramienta sobre
su binario correspondiente.

</details>

---

### Ejercicio 8 — Propiedad que no deberías testear

¿Cuáles de estas propiedades tiene sentido testear con proptest y cuáles no?

```
a) push(x); pop() == x
b) El puntero head no es NULL después de push
c) push no tarda más de 1ms
d) push(x); push(y); pop() == y && pop() == x
e) sizeof(Stack) == 24
f) Para todo x: contains(x) == false en set vacío
```

**Prediccion**: ¿Cuáles son propiedades del API y cuáles son detalles de
implementación?

<details><summary>Respuesta</summary>

| Propiedad | ¿Testear? | Razón |
|-----------|----------|-------|
| a) push-pop identity | **Sí** | Propiedad fundamental del API |
| b) head != NULL | **No** | Detalle de implementación — el usuario no ve `head` |
| c) Tiempo < 1ms | **No** | Los tests de propiedades verifican correctitud, no rendimiento. Además, el tiempo depende de la máquina |
| d) LIFO order | **Sí** | Propiedad fundamental del API |
| e) sizeof == 24 | **No** | Detalle de implementación, depende de la plataforma |
| f) contains vacío | **Sí** | Propiedad del API — trivial pero vale como test |

Regla general: testear propiedades del **contrato público** (lo que el
usuario del TAD espera), no propiedades de la **implementación interna**
(que pueden cambiar sin romper el contrato).

Excepción: los verificadores de invariantes (sección 4) sí acceden a la
implementación interna, pero su propósito es **depuración durante desarrollo**,
no tests de API.

</details>

---

### Ejercicio 9 — Test que detecta memory leak

Escribe un test en C que, compilado con ASan, detecte un leak en esta
implementación de `list_remove_all`:

```c
// Supuestamente remueve TODOS los nodos con valor == target
void list_remove_all(List *list, int target) {
    Node *prev = NULL;
    Node *cur = list->head;
    while (cur) {
        if (cur->data == target) {
            if (prev)
                prev->next = cur->next;
            else
                list->head = cur->next;
            Node *to_free = cur;
            cur = cur->next;
            free(to_free);
            list->size--;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
}
```

**Prediccion**: ¿Hay un leak? ¿O la implementación es correcta?

<details><summary>Respuesta</summary>

La implementación es **correcta** — no hay leak. Cada nodo encontrado se
desconecta, se guarda en `to_free`, y se libera. `cur` avanza a `cur->next`
*antes* del free (porque `to_free` guarda la referencia).

El test para verificar:

```c
void test_remove_all(void) {
    List *l = list_create();
    list_push_front(l, 5);
    list_push_front(l, 3);
    list_push_front(l, 5);
    list_push_front(l, 5);
    list_push_front(l, 7);
    // Lista: 7 → 5 → 5 → 3 → 5

    list_remove_all(l, 5);
    assert(list_size(l) == 2);    // quedan 7 y 3
    assert(list_front(l) == 7);

    list_destroy(l);
    printf("  PASS: test_remove_all\n");
}
```

Compilar con ASan:

```bash
gcc -g -O0 -fsanitize=address -o test test.c list.c
./test
```

Si ASan no reporta nada y el assert pasa, la implementación es correcta.

Este ejercicio es deliberadamente "limpio" para practicar la lectura de
código correcto — no todo tiene bug. Parte de testear es **confirmar** que
el código funciona.

</details>

---

### Ejercicio 10 — Diseñar suite de tests para BST

Diseña (no implementes, solo lista) una suite de tests completa para un BST
con operaciones: `insert`, `contains`, `remove`, `min`, `max`, `inorder`.
Agrupa los tests en: unitarios, casos borde, invariantes, y estrés.

**Prediccion**: ¿Cuántos tests necesitarías como mínimo para tener confianza
en la implementación?

<details><summary>Respuesta</summary>

**Tests unitarios (correctitud básica):**
1. `test_create_empty` — BST vacío, size 0
2. `test_insert_one` — insert + contains true
3. `test_insert_three_balanced` — insert 2,1,3 → in-order = [1,2,3]
4. `test_contains_absent` — contains de valor no insertado → false
5. `test_min_max` — verificar mínimo y máximo
6. `test_remove_leaf` — remover hoja
7. `test_remove_one_child` — remover nodo con 1 hijo
8. `test_remove_two_children` — remover nodo con 2 hijos
9. `test_remove_root` — remover la raíz
10. `test_inorder_sorted` — in-order produce secuencia ordenada

**Tests de casos borde:**
11. `test_insert_duplicate` — comportamiento con duplicados
12. `test_remove_absent` — remover valor que no existe
13. `test_remove_from_empty` — remover de BST vacío
14. `test_min_max_single` — min/max con un solo elemento
15. `test_insert_sorted_sequence` — 1,2,3,4,5 (degenerado a lista)
16. `test_insert_reverse_sorted` — 5,4,3,2,1 (degenerado a lista)
17. `test_fill_drain_refill` — vaciar completamente y re-usar

**Tests de invariantes:**
18. `test_bst_property_after_inserts` — check BST order después de $n$ inserts
19. `test_bst_property_after_removes` — check BST order después de removes
20. `test_size_consistency` — size == count_nodes siempre

**Tests de estrés:**
21. `test_stress_10K_random_ops` — 10K insert/remove/contains aleatorios
22. `test_stress_oracle` — comparar con `std::set` o array ordenado
23. `test_stress_sorted_input` — insert secuencial 1..10K (peor caso)

**Mínimo para tener confianza razonable: ~15 tests** (los unitarios + bordes
más críticos + al menos 1 estrés con oracle). Los 23 listados dan confianza
alta.

El test de **remove con 2 hijos** (#8) es históricamente el que más bugs
revela — la lógica del sucesor in-order tiene muchos casos sutiles.

</details>

Limpieza:

```bash
rm -f test_list test_runner test_runner_nosan test_normal test_asan
```
