# Estructura de un test — Arrange-Act-Assert, un test = una verificacion, nombres descriptivos

## 1. Introduccion

En T01 vimos **como** escribir tests en C (assert, exit codes, main de prueba). Ahora vemos **como escribirlos bien**. Un test que pasa no es necesariamente un buen test. Un buen test es:

- **Legible**: puedes leer el test y entender que se esta verificando sin leer la implementacion.
- **Especifico**: cuando falla, sabes exactamente que se rompio.
- **Independiente**: no depende de otros tests ni del orden de ejecucion.
- **Repetible**: produce el mismo resultado cada vez que se ejecuta.
- **Rapido**: lo suficientemente rapido como para ejecutarlo constantemente.

Estos principios existen en todos los lenguajes. En C, donde no hay framework que te guie, son aun mas importantes — porque nadie te impide escribir un test de 200 lineas que verifica 15 cosas a la vez con variables compartidas entre tests.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     ANATOMIA DE UN BUEN TEST                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Nombre descriptivo                                                        │
│  ──────────────────                                                        │
│  test_hash_table_insert_duplicate_key_overwrites_value                     │
│       │          │        │              │                                  │
│       │          │        │              └─ que se espera                   │
│       │          │        └─ bajo que condicion                             │
│       │          └─ que operacion                                           │
│       └─ que modulo/componente                                              │
│                                                                             │
│  Cuerpo del test                                                           │
│  ───────────────                                                           │
│  ┌─── ARRANGE ────┐                                                        │
│  │ Crear datos     │  ← Preparar el escenario                              │
│  │ Inicializar     │                                                        │
│  │ Configurar      │                                                        │
│  └─────────────────┘                                                        │
│          │                                                                  │
│  ┌──── ACT ───────┐                                                        │
│  │ Llamar a la     │  ← Ejecutar la operacion que se quiere verificar      │
│  │ funcion bajo    │     (una sola operacion)                               │
│  │ test            │                                                        │
│  └─────────────────┘                                                        │
│          │                                                                  │
│  ┌─── ASSERT ─────┐                                                        │
│  │ Verificar       │  ← Comprobar que el resultado es el esperado          │
│  │ resultado       │                                                        │
│  │ Verificar       │                                                        │
│  │ efectos         │                                                        │
│  └─────────────────┘                                                        │
│                                                                             │
│  Cleanup (si hay recursos)                                                 │
│  ──────────────────────────                                                │
│  free(), fclose(), etc.                                                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Arrange-Act-Assert (AAA)

### 2.1 El patron

Arrange-Act-Assert es el patron de estructura interna de un test mas utilizado en la industria. Es simple, universal, y funciona en cualquier lenguaje:

| Fase | Que hace | En C |
|------|----------|------|
| **Arrange** | Prepara el estado necesario para el test: crea objetos, inicializa variables, configura precondiciones | `malloc`, inicializadores, llamadas a constructores |
| **Act** | Ejecuta la operacion que se quiere verificar — **una sola cosa** | La llamada a la funcion bajo test |
| **Assert** | Verifica que el resultado es el esperado | `assert()`, macros CHECK, comparaciones |

Opcionalmente hay una cuarta fase: **Cleanup** (liberar memoria, cerrar archivos). En C esto es casi siempre necesario.

### 2.2 Ejemplo basico — funcion pura

```c
// Modulo bajo test
int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}
```

```c
// Test con AAA explicito
void test_clamp_value_below_min_returns_min(void) {
    // ── Arrange ──
    int value = -10;
    int min = 0;
    int max = 100;

    // ── Act ──
    int result = clamp(value, min, max);

    // ── Assert ──
    ASSERT_EQ(result, 0);
}

void test_clamp_value_above_max_returns_max(void) {
    // ── Arrange ──
    int value = 200;
    int min = 0;
    int max = 100;

    // ── Act ──
    int result = clamp(value, min, max);

    // ── Assert ──
    ASSERT_EQ(result, 100);
}

void test_clamp_value_within_range_unchanged(void) {
    // ── Arrange ──
    int value = 50;
    int min = 0;
    int max = 100;

    // ── Act ──
    int result = clamp(value, min, max);

    // ── Assert ──
    ASSERT_EQ(result, 50);
}
```

Para funciones puras con argumentos simples, el Arrange puede parecer innecesario (podrias escribir `ASSERT_EQ(clamp(-10, 0, 100), 0)` directamente). Pero separar las fases tiene valor incluso aqui:

1. **Legibilidad**: queda claro cual es el input, cual es la operacion, y cual es la expectativa.
2. **Debugging**: si el test falla, puedes poner un breakpoint en el Act y examinar el estado del Arrange.
3. **Consistencia**: todos los tests siguen el mismo patron visual, lo que reduce la carga cognitiva al leerlos.

### 2.3 Ejemplo con estado mutable — AAA + Cleanup

```c
// Modulo bajo test: un dynamic array (vector)
typedef struct {
    int *data;
    size_t len;
    size_t cap;
} Vec;

Vec *vec_new(size_t cap);
void vec_free(Vec *v);
bool vec_push(Vec *v, int value);
bool vec_pop(Vec *v, int *out);
int  vec_get(const Vec *v, size_t index);
size_t vec_len(const Vec *v);
```

```c
void test_vec_push_increases_length(void) {
    // ── Arrange ──
    Vec *v = vec_new(4);

    // ── Act ──
    vec_push(v, 42);

    // ── Assert ──
    ASSERT_EQ(vec_len(v), 1);

    // ── Cleanup ──
    vec_free(v);
}

void test_vec_push_beyond_capacity_triggers_growth(void) {
    // ── Arrange ──
    Vec *v = vec_new(2);  // Capacidad inicial = 2
    vec_push(v, 10);
    vec_push(v, 20);      // Ahora len == cap == 2

    // ── Act ──
    bool ok = vec_push(v, 30);  // Debe triggear realloc

    // ── Assert ──
    ASSERT_TRUE(ok);
    ASSERT_EQ(vec_len(v), 3);
    ASSERT_EQ(vec_get(v, 0), 10);  // Datos preservados tras realloc
    ASSERT_EQ(vec_get(v, 1), 20);
    ASSERT_EQ(vec_get(v, 2), 30);

    // ── Cleanup ──
    vec_free(v);
}

void test_vec_pop_returns_last_element(void) {
    // ── Arrange ──
    Vec *v = vec_new(4);
    vec_push(v, 10);
    vec_push(v, 20);
    vec_push(v, 30);

    // ── Act ──
    int value;
    bool ok = vec_pop(v, &value);

    // ── Assert ──
    ASSERT_TRUE(ok);
    ASSERT_EQ(value, 30);
    ASSERT_EQ(vec_len(v), 2);

    // ── Cleanup ──
    vec_free(v);
}
```

Observa que el **Arrange** puede incluir llamadas a la misma funcion que se testea en otro test. En `test_vec_pop_returns_last_element`, usamos `vec_push` en el Arrange. Esto es normal y aceptable — el Arrange establece precondiciones, y para eso puede usar cualquier parte del API.

### 2.4 El Arrange puede ser largo — y eso esta bien

```c
void test_hash_table_resize_preserves_all_entries(void) {
    // ── Arrange ──
    // Crear tabla con capacidad pequena y llenarla
    // hasta forzar un resize
    HashTable *ht = ht_new(4);

    // Insertar suficientes entradas para superar el load factor
    // (si load factor = 0.75, con cap 4 el resize ocurre al insertar la 4ta)
    ht_insert(ht, "key1", "value1");
    ht_insert(ht, "key2", "value2");
    ht_insert(ht, "key3", "value3");

    // Guardar el estado previo para verificar post-resize
    size_t count_before = ht_count(ht);
    char *val1 = strdup(ht_get(ht, "key1"));
    char *val2 = strdup(ht_get(ht, "key2"));
    char *val3 = strdup(ht_get(ht, "key3"));

    // ── Act ──
    // Esta insercion deberia triggear el resize
    ht_insert(ht, "key4", "value4");

    // ── Assert ──
    // La tabla crecio pero todos los datos se preservaron
    ASSERT_EQ(ht_count(ht), count_before + 1);
    ASSERT_STR_EQ(ht_get(ht, "key1"), val1);
    ASSERT_STR_EQ(ht_get(ht, "key2"), val2);
    ASSERT_STR_EQ(ht_get(ht, "key3"), val3);
    ASSERT_STR_EQ(ht_get(ht, "key4"), "value4");

    // ── Cleanup ──
    free(val1);
    free(val2);
    free(val3);
    ht_free(ht);
}
```

El Arrange aqui tiene 9 lineas. Eso es perfectamente normal. Lo importante es que el **Act** sea una sola operacion y que los **Asserts** verifiquen un solo aspecto conceptual (en este caso: "todos los datos se preservan tras un resize").

### 2.5 Cuando el Act y el Assert se fusionan

A veces la operacion y la verificacion son la misma linea. Esto pasa con funciones que retornan bool (exito/fallo) o punteros (NULL/no-NULL):

```c
void test_vec_pop_on_empty_returns_false(void) {
    // ── Arrange ──
    Vec *v = vec_new(4);
    int dummy;

    // ── Act + Assert ──
    // pop en vector vacio retorna false
    ASSERT_FALSE(vec_pop(v, &dummy));

    // ── Cleanup ──
    vec_free(v);
}
```

Esto es aceptable. No necesitas forzar una variable intermedia cuando la verificacion es obvia:

```c
// Forzar la separacion cuando no aporta:
bool ok = vec_pop(v, &dummy);  // Act
ASSERT_FALSE(ok);               // Assert
// Dos lineas donde una bastaba

// Mejor — claro y directo:
ASSERT_FALSE(vec_pop(v, &dummy));  // Act + Assert en uno
```

La regla no es "siempre separar en 3 bloques de codigo" sino "pensar en 3 fases logicas". A veces dos fases colapsan en una linea, y eso esta bien.

### 2.6 AAA en otros lenguajes — misma idea, diferente nombre

| Lenguaje/Framework | Nombre del patron | Arrange | Act | Assert |
|--------------------|--------------------|---------|-----|--------|
| General | AAA | Arrange | Act | Assert |
| BDD (Behavior-Driven) | GWT | **G**iven | **W**hen | **T**hen |
| Rust (convencion) | — | setup | call | assert! |
| Go (convencion) | — | setup | call | if got != want |
| JUnit/xUnit | AAA | @Before + setup | method call | assertEquals |

El patron BDD "Given-When-Then" es exactamente AAA con otro vocabulario:

```
Given a hash table with 3 entries at 75% load factor  ← Arrange
When  a 4th entry is inserted                          ← Act
Then  all 4 entries are retrievable                    ← Assert
```

En C no usamos estos nombres formalmente, pero el concepto es el mismo.

---

## 3. Un test = una verificacion

### 3.1 El principio

Cada funcion de test debe verificar **una sola cosa**. No "una sola linea de assert", sino un solo comportamiento logico. Si el test falla, debe ser inmediatamente obvio **que se rompio**.

### 3.2 Antipatron: el test que verifica todo

```c
// MALO — un test que verifica 5 cosas diferentes
void test_vec(void) {
    Vec *v = vec_new(4);

    // Verifica creacion
    ASSERT(v != NULL);
    ASSERT_EQ(vec_len(v), 0);

    // Verifica push
    vec_push(v, 10);
    ASSERT_EQ(vec_len(v), 1);
    ASSERT_EQ(vec_get(v, 0), 10);

    // Verifica push multiple
    vec_push(v, 20);
    vec_push(v, 30);
    ASSERT_EQ(vec_len(v), 3);

    // Verifica pop
    int val;
    vec_pop(v, &val);
    ASSERT_EQ(val, 30);
    ASSERT_EQ(vec_len(v), 2);

    // Verifica get fuera de rango
    // (supongamos que retorna -1 o algun sentinel)
    ASSERT_EQ(vec_get(v, 99), -1);

    vec_free(v);
}
```

Problemas de este test:

1. **Si falla en la linea 5, no sabes nada sobre pop ni get** — las verificaciones posteriores nunca se ejecutan (a menos que uses soft-asserts).
2. **El nombre "test_vec" no dice nada** — ¿que fallo? ¿Push? ¿Pop? ¿Get? ¿Creacion?
3. **Debugging dificil** — si falla en "ASSERT_EQ(vec_len(v), 3)", ¿es porque push no funciona, o porque pop se ejecuto prematuramente, o porque len calcula mal?
4. **Fragilidad acumulada** — cada verificacion depende del estado dejado por la anterior. Si cambias algo en el medio, todo lo de abajo puede romperse.

### 3.3 Refactorizacion: un test por comportamiento

```c
// BIEN — cada test verifica un solo comportamiento

void test_vec_new_creates_empty_vector(void) {
    Vec *v = vec_new(4);
    ASSERT(v != NULL);
    ASSERT_EQ(vec_len(v), 0);
    vec_free(v);
}

void test_vec_push_stores_element(void) {
    Vec *v = vec_new(4);
    vec_push(v, 10);
    ASSERT_EQ(vec_get(v, 0), 10);
    vec_free(v);
}

void test_vec_push_increases_length(void) {
    Vec *v = vec_new(4);
    vec_push(v, 10);
    ASSERT_EQ(vec_len(v), 1);
    vec_push(v, 20);
    ASSERT_EQ(vec_len(v), 2);
    vec_free(v);
}

void test_vec_pop_returns_last_and_decreases_length(void) {
    Vec *v = vec_new(4);
    vec_push(v, 10);
    vec_push(v, 20);
    vec_push(v, 30);

    int val;
    vec_pop(v, &val);

    ASSERT_EQ(val, 30);
    ASSERT_EQ(vec_len(v), 2);
    vec_free(v);
}

void test_vec_get_out_of_bounds_returns_sentinel(void) {
    Vec *v = vec_new(4);
    vec_push(v, 10);
    ASSERT_EQ(vec_get(v, 99), -1);
    vec_free(v);
}
```

Ahora cada test:
- Tiene un **nombre que describe exactamente que verifica**
- Es **independiente** — no depende del estado de otro test
- Cuando falla, **el nombre te dice que se rompio** sin leer el codigo
- Se puede ejecutar **en cualquier orden** con el mismo resultado

### 3.4 Multiples asserts estan bien — si verifican la misma cosa

"Un test = una verificacion" no significa "un test = un assert". A veces un solo comportamiento requiere verificar multiples propiedades:

```c
void test_vec_push_beyond_capacity_preserves_all_data(void) {
    Vec *v = vec_new(2);
    vec_push(v, 10);
    vec_push(v, 20);

    // Act — este push fuerza realloc
    vec_push(v, 30);

    // Assert — verificamos que TODOS los datos se preservaron
    // Estos 4 asserts verifican UNA cosa: "el realloc no perdio datos"
    ASSERT_EQ(vec_len(v), 3);
    ASSERT_EQ(vec_get(v, 0), 10);
    ASSERT_EQ(vec_get(v, 1), 20);
    ASSERT_EQ(vec_get(v, 2), 30);

    vec_free(v);
}
```

Los 4 asserts estan verificando un solo comportamiento: "push beyond capacity preserves all data". Eso es correcto. Lo incorrecto seria meter en este test una verificacion de pop, o de get con indice negativo — eso es otro comportamiento.

### 3.5 Regla practica: si necesitas "and" en el nombre, son dos tests

```
test_vec_push_stores_element_AND_increases_length
                                ^^^
                        Esto deberian ser dos tests:
                        - test_vec_push_stores_element
                        - test_vec_push_increases_length
```

```
test_hash_table_insert_updates_count_AND_get_retrieves_value
                                     ^^^
                        Dos tests:
                        - test_hash_table_insert_updates_count
                        - test_hash_table_get_retrieves_inserted_value
```

La excepcion es cuando el "and" describe facetas del mismo comportamiento:

```
test_vec_pop_returns_last_element_and_decreases_length
```

Esto es aceptable porque pop **siempre** hace ambas cosas — son facetas inseparables del mismo comportamiento, no dos comportamientos independientes.

---

## 4. Nombres descriptivos

### 4.1 Por que importa el nombre

Cuando un test falla en CI, lo primero que ves es el nombre. Si el nombre es `test_7` o `test_parser`, no sabes nada. Si el nombre es `test_parser_rejects_unterminated_string_literal`, sabes exactamente que se rompio sin abrir el archivo.

```bash
# Output con nombres malos
$ ./test_parser
  [FAIL] test_1
  [FAIL] test_5
  [PASS] test_2
  [PASS] test_3
  [PASS] test_4
# ¿Que fallo? No tengo idea. Tengo que abrir el archivo y buscar test_1 y test_5.

# Output con nombres buenos
$ ./test_parser
  [FAIL] test_parser_rejects_unterminated_string
  [FAIL] test_parser_handles_nested_brackets_depth_limit
  [PASS] test_parser_accepts_empty_input
  [PASS] test_parser_preserves_whitespace_in_strings
  [PASS] test_parser_parses_integer_literals
# Sin abrir ningun archivo ya se que: tengo un bug con strings sin terminar
# y otro con la profundidad de anidamiento de corchetes.
```

### 4.2 Convenciones de nombrado

No hay un estandar universal en C, pero hay patrones comunes que se han consolidado en la industria:

**Patron 1: modulo_operacion_condicion_resultado**

```c
// test_[modulo]_[operacion]_[condicion]_[resultado_esperado]
int test_parser_parse_empty_input_returns_null(void);
int test_parser_parse_valid_json_returns_tree(void);
int test_parser_parse_nested_arrays_depth_3(void);
int test_parser_parse_unterminated_string_returns_error(void);

int test_hash_table_insert_new_key_increments_count(void);
int test_hash_table_insert_duplicate_key_overwrites_value(void);
int test_hash_table_delete_existing_key_decrements_count(void);
int test_hash_table_delete_nonexistent_key_returns_false(void);
int test_hash_table_get_after_delete_returns_null(void);
```

**Patron 2: modulo_debe_comportamiento (BDD-style)**

```c
// test_[modulo]_should_[comportamiento]
int test_parser_should_accept_empty_input(void);
int test_parser_should_reject_unterminated_string(void);
int test_parser_should_handle_nested_brackets(void);

int test_hash_table_should_overwrite_on_duplicate_key(void);
int test_hash_table_should_return_null_for_missing_key(void);
```

**Patron 3: modulo_cuando_condicion (Given-When-Then)**

```c
// test_[modulo]_when_[condicion]_[resultado]
int test_parser_when_input_is_empty_returns_null(void);
int test_parser_when_string_is_unterminated_returns_error(void);

int test_hash_table_when_key_exists_overwrites_value(void);
int test_hash_table_when_key_missing_returns_null(void);
```

### 4.3 Comparacion de estilos de nombrado

| Patron | Ventaja | Desventaja |
|--------|---------|------------|
| `modulo_op_condicion_resultado` | Muy explicito, facil de buscar con grep | Nombres largos |
| `modulo_should_behavior` | Legible como frase en ingles | "should" es redundante (un test siempre describe lo esperado) |
| `modulo_when_condicion` | Enfasis en la precondicion | Puede ser ambiguo si no incluye el resultado |

Mi recomendacion: usa el patron 1 (`modulo_op_condicion_resultado`) porque es el mas explicito y el mas comun en proyectos C de codigo abierto.

### 4.4 Nombres en C — limitaciones practicas

En C los nombres de funciones no pueden contener espacios, puntos, ni la mayoria de caracteres especiales. Los underscores son tu unica herramienta de separacion. Ademas, los nombres largos pueden ser molestos en editores con columnas limitadas:

```c
// Demasiado largo — dificil de leer
int test_binary_search_tree_insert_when_tree_is_empty_should_set_root_to_new_node(void);

// Mas razonable — abrevia el modulo
int test_bst_insert_empty_tree_sets_root(void);

// Tambien aceptable — usa acronimos conocidos en el proyecto
int test_ht_get_missing_key_returns_null(void);  // ht = hash table
```

Reglas practicas:

1. **El modulo puede abreviarse** si la abreviatura es obvia en el contexto del proyecto: `ht` para hash table, `rb` para ring buffer, `ll` para linked list.
2. **La condicion y el resultado no se abrevian** — son la parte mas importante del nombre.
3. **Si el nombre supera ~60 caracteres**, busca formas de acortarlo sin perder claridad.
4. **Prefija siempre con `test_`** — es la convencion universal y permite buscar todos los tests con `grep "^.*test_"`.

### 4.5 Nombres de archivos de test

El nombre del archivo de test debe reflejar el modulo que testea:

```
src/parser.c          → tests/test_parser.c
src/hash_table.c      → tests/test_hash_table.c
src/ring_buffer.c     → tests/test_ring_buffer.c
src/http/request.c    → tests/test_http_request.c
```

Para modulos grandes, puedes dividir los tests en multiples archivos por area:

```
tests/
├── test_parser_basics.c        ← parsing de tipos simples
├── test_parser_errors.c        ← manejo de errores
├── test_parser_edge_cases.c    ← edge cases (Unicode, archivos enormes)
└── test_parser_regression.c    ← tests para bugs encontrados y corregidos
```

---

## 5. Independencia de tests

### 5.1 Por que importa

Tests independientes significan que:
- Puedes ejecutar **cualquier test individual** sin ejecutar los demas.
- Puedes ejecutar los tests en **cualquier orden** y obtener el mismo resultado.
- Un fallo en un test **no causa fallos en cascada** en tests posteriores.
- Puedes ejecutar tests **en paralelo** (con Make `-j`).

### 5.2 Antipatron: estado compartido entre tests

```c
// MALO — estado global compartido entre tests
static Vec *shared_vec = NULL;

void test_push(void) {
    shared_vec = vec_new(4);
    vec_push(shared_vec, 10);
    ASSERT_EQ(vec_len(shared_vec), 1);
    // NO hace free — lo deja para el siguiente test
}

void test_push_more(void) {
    // Depende de que test_push se haya ejecutado primero
    // y haya dejado shared_vec con un elemento
    vec_push(shared_vec, 20);
    ASSERT_EQ(vec_len(shared_vec), 2);  // Falla si test_push no se ejecuto
}

void test_pop(void) {
    // Depende de que test_push_more se haya ejecutado
    int val;
    vec_pop(shared_vec, &val);
    ASSERT_EQ(val, 20);  // Falla si el orden cambia
    vec_free(shared_vec);
    shared_vec = NULL;
}
```

Este codigo tiene tres problemas graves:

1. **Dependencia de orden**: `test_pop` falla si se ejecuta antes de `test_push_more`.
2. **Fallo en cascada**: si `test_push` falla, todos los demas tambien fallan.
3. **Memory leak**: si `test_push_more` falla, `vec_free` nunca se llama.

### 5.3 Solucion: cada test crea y destruye su propio estado

```c
// BIEN — cada test es autocontenido

void test_push_single_element(void) {
    Vec *v = vec_new(4);
    vec_push(v, 10);
    ASSERT_EQ(vec_len(v), 1);
    vec_free(v);
}

void test_push_two_elements(void) {
    Vec *v = vec_new(4);
    vec_push(v, 10);
    vec_push(v, 20);
    ASSERT_EQ(vec_len(v), 2);
    vec_free(v);
}

void test_pop_returns_last_pushed(void) {
    Vec *v = vec_new(4);
    vec_push(v, 10);
    vec_push(v, 20);

    int val;
    vec_pop(v, &val);

    ASSERT_EQ(val, 20);
    vec_free(v);
}
```

Si, hay **duplicacion** en el Arrange (cada test crea su propio Vec). Eso es **intencional y correcto**. La duplicacion en tests es mucho menos peligrosa que la dependencia entre tests. En la seccion 7 veremos como reducir esa duplicacion de forma segura con helpers.

### 5.4 Estado global en C — la trampa

En C es facil tener estado global accidental. Variables `static` a nivel de archivo, variables globales en headers, o incluso `errno`:

```c
// Trampa 1: errno como estado global
#include <errno.h>

void test_open_nonexistent_file(void) {
    FILE *f = fopen("/no/existe", "r");
    ASSERT(f == NULL);
    ASSERT_EQ(errno, ENOENT);
    // errno queda con valor ENOENT despues de este test
}

void test_something_unrelated(void) {
    // Si este test llama a una funcion de la libc que no resetea errno,
    // podria leer ENOENT de un test anterior
    some_function();
    if (errno != 0) {
        // ¿Es por some_function() o por el test anterior?
    }
}
```

```c
// Solucion: resetear errno al inicio del test
void test_open_nonexistent_file(void) {
    errno = 0;  // Resetear antes de usar
    FILE *f = fopen("/no/existe", "r");
    ASSERT(f == NULL);
    ASSERT_EQ(errno, ENOENT);
}
```

```c
// Trampa 2: variables static que persisten entre tests
// en el modulo bajo test

// cache.c
static int cache_hits = 0;   // Esta variable persiste entre tests!

int cache_lookup(const char *key) {
    // ...
    cache_hits++;
    // ...
}

int cache_get_hits(void) {
    return cache_hits;
}
```

```c
// test_cache.c
void test_cache_hit_increments_counter(void) {
    // Si otro test llamo a cache_lookup antes,
    // cache_hits ya tiene un valor != 0
    cache_lookup("key1");
    ASSERT_EQ(cache_get_hits(), 1);  // Puede fallar: cache_hits podria ser 5
}

// Solucion: la API debe exponer una funcion de reset
// cache.h: void cache_reset(void);
// Llamar cache_reset() en el Arrange de cada test
```

### 5.5 Test con archivos temporales — aislamiento del filesystem

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// Helper: crear un directorio temporal unico por test
static char *make_temp_dir(void) {
    char template[] = "/tmp/test_XXXXXX";
    char *dir = mkdtemp(template);
    if (dir == NULL) return NULL;
    return strdup(dir);
}

// Helper: eliminar directorio y su contenido
static void remove_temp_dir(const char *dir) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
    system(cmd);
}

void test_write_config_creates_file(void) {
    // ── Arrange ──
    char *tmpdir = make_temp_dir();
    ASSERT(tmpdir != NULL);

    char path[512];
    snprintf(path, sizeof(path), "%s/config.ini", tmpdir);

    // ── Act ──
    int result = write_config(path, "key=value\n");

    // ── Assert ──
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(access(path, F_OK) == 0);  // El archivo existe

    // Verificar contenido
    FILE *f = fopen(path, "r");
    ASSERT(f != NULL);
    char buf[256];
    fgets(buf, sizeof(buf), f);
    fclose(f);
    ASSERT_STR_EQ(buf, "key=value\n");

    // ── Cleanup ──
    remove_temp_dir(tmpdir);
    free(tmpdir);
}
```

Cada test usa su propio directorio temporal (`/tmp/test_XXXXXX` con sufijo aleatorio), asi que no hay conflictos entre tests ejecutados en paralelo.

---

## 6. Tests que documentan

### 6.1 Un test es documentacion ejecutable

Un test bien escrito funciona como documentacion del API que **nunca se desactualiza** — porque si el comportamiento cambia, el test falla. Esto es particularmente valioso en C, donde la documentacion formal suele ser escasa.

```c
// Este test documenta el contrato de ht_insert:
void test_ht_insert_duplicate_key_overwrites_value(void) {
    HashTable *ht = ht_new(16);

    ht_insert(ht, "name", "Alice");
    ht_insert(ht, "name", "Bob");    // Misma key, diferente valor

    // El contrato es: la segunda insercion REEMPLAZA el valor
    // (no inserta un duplicado ni falla)
    ASSERT_STR_EQ(ht_get(ht, "name"), "Bob");
    ASSERT_EQ(ht_count(ht), 1);  // Sigue siendo UNA entrada

    ht_free(ht);
}

// Este test documenta un edge case que no es obvio del header:
void test_ht_insert_null_value_is_allowed(void) {
    HashTable *ht = ht_new(16);

    // ¿Se puede insertar NULL como valor?
    // SI — la tabla distingue "key no existe" de "key existe con valor NULL"
    bool ok = ht_insert(ht, "key", NULL);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(ht_contains(ht, "key"));  // La key existe
    ASSERT(ht_get(ht, "key") == NULL);    // El valor es NULL

    ht_free(ht);
}

// Este test documenta una decision de diseno:
void test_ht_get_nonexistent_key_returns_null(void) {
    HashTable *ht = ht_new(16);

    // get() retorna NULL para keys que no existen
    // (no aborta, no retorna un sentinel, no modifica errno)
    ASSERT(ht_get(ht, "nonexistent") == NULL);

    ht_free(ht);
}
```

Observa como los **comentarios** en estos tests no explican el codigo — explican el **contrato** del API. Esto es diferente a los comentarios en codigo de produccion, que deben explicar el "por que", no el "que".

### 6.2 Regression tests — documentar bugs encontrados

Cuando encuentras y corriges un bug, escribe un test que reproduce el bug **antes** de corregirlo (para confirmar que falla) y dejalo permanentemente (para asegurar que no vuelva):

```c
// Bug #47: ht_delete no manejaba colisiones correctamente.
// Si key_a y key_b colisionaban en el mismo bucket, borrar key_a
// hacia que key_b fuera inaccesible.
// Corregido en commit abc1234.
void test_ht_delete_with_collision_preserves_other_keys(void) {
    // Arrange — forzar colision usando keys que hashean al mismo bucket
    // Con una tabla de capacidad 4 y hash simple:
    //   "aa" y "ea" podrian colisionar (depende del hash)
    HashTable *ht = ht_new(4);

    // Insertar dos keys que colisionan
    ht_insert(ht, "aa", "value_aa");
    ht_insert(ht, "ea", "value_ea");

    // Act — borrar la primera key de la cadena de colision
    ht_delete(ht, "aa");

    // Assert — la segunda key debe seguir accesible
    ASSERT(ht_get(ht, "aa") == NULL);           // Borrada
    ASSERT_STR_EQ(ht_get(ht, "ea"), "value_ea"); // Preservada

    ht_free(ht);
}
```

El comentario al inicio del test referencia el bug y el commit. Esto crea una cadena de trazabilidad: bug report → test → fix → commit.

---

## 7. Helpers y reduccion de boilerplate

### 7.1 El problema de la repeticion

Despues de aplicar "un test = una verificacion", es comun tener mucha repeticion en el Arrange:

```c
void test_vec_push_single(void) {
    Vec *v = vec_new(4);           // repetido
    vec_push(v, 42);
    ASSERT_EQ(vec_get(v, 0), 42);
    vec_free(v);                   // repetido
}

void test_vec_push_updates_length(void) {
    Vec *v = vec_new(4);           // repetido
    vec_push(v, 42);
    ASSERT_EQ(vec_len(v), 1);
    vec_free(v);                   // repetido
}

void test_vec_pop_on_empty(void) {
    Vec *v = vec_new(4);           // repetido
    int val;
    ASSERT_FALSE(vec_pop(v, &val));
    vec_free(v);                   // repetido
}
```

### 7.2 Solucion 1: Funcion helper para crear estado

```c
// Helper — crea un Vec con datos precargados
static Vec *make_test_vec(int *values, size_t count) {
    Vec *v = vec_new(count > 4 ? count * 2 : 4);
    if (v == NULL) return NULL;
    for (size_t i = 0; i < count; i++) {
        vec_push(v, values[i]);
    }
    return v;
}

void test_vec_pop_returns_last_of_three(void) {
    int data[] = {10, 20, 30};
    Vec *v = make_test_vec(data, 3);

    int val;
    vec_pop(v, &val);

    ASSERT_EQ(val, 30);
    vec_free(v);
}

void test_vec_len_after_three_pushes(void) {
    int data[] = {10, 20, 30};
    Vec *v = make_test_vec(data, 3);

    ASSERT_EQ(vec_len(v), 3);

    vec_free(v);
}
```

### 7.3 Solucion 2: Macro wrapper con cleanup automatico

En C no hay RAII ni defer. Pero puedes usar macros para garantizar cleanup:

```c
// Macro que ejecuta un bloque con un Vec y hace free al final
#define WITH_VEC(name, cap, body) \
    do { \
        Vec *name = vec_new(cap); \
        if (name == NULL) { \
            ASSERT(name != NULL); \
            break; \
        } \
        body \
        vec_free(name); \
    } while (0)

void test_vec_push_single(void) {
    WITH_VEC(v, 4, {
        vec_push(v, 42);
        ASSERT_EQ(vec_get(v, 0), 42);
    });
}

void test_vec_is_empty_initially(void) {
    WITH_VEC(v, 4, {
        ASSERT_TRUE(vec_is_empty(v));
    });
}
```

Este patron tiene limitaciones (no puedes hacer `return` desde dentro del bloque porque saltaria el `vec_free`), pero es util para tests simples.

### 7.4 Solucion 3: Struct de contexto (test fixture manual)

Para tests que necesitan multiples objetos en el Arrange:

```c
// Fixture — todo el estado necesario para los tests de OrderService
typedef struct {
    HashTable *catalog;      // catalogo de productos
    Vec       *cart;         // carrito de compras
    Logger    *logger;       // logger (puede ser un mock)
} OrderTestCtx;

static OrderTestCtx setup_order_test(void) {
    OrderTestCtx ctx = {0};
    ctx.catalog = ht_new(64);
    ctx.cart = vec_new(16);
    ctx.logger = logger_new_null();  // logger que descarta todo

    // Cargar datos de prueba
    ht_insert(ctx.catalog, "SKU001", &(Product){.price = 1000});
    ht_insert(ctx.catalog, "SKU002", &(Product){.price = 2500});
    ht_insert(ctx.catalog, "SKU003", &(Product){.price = 500});

    return ctx;
}

static void teardown_order_test(OrderTestCtx *ctx) {
    ht_free(ctx->catalog);
    vec_free(ctx->cart);
    logger_free(ctx->logger);
}

void test_order_total_single_item(void) {
    OrderTestCtx ctx = setup_order_test();

    add_to_cart(ctx.cart, "SKU001", 1);
    int total = calculate_total(ctx.cart, ctx.catalog);

    ASSERT_EQ(total, 1000);

    teardown_order_test(&ctx);
}

void test_order_total_multiple_items(void) {
    OrderTestCtx ctx = setup_order_test();

    add_to_cart(ctx.cart, "SKU001", 2);
    add_to_cart(ctx.cart, "SKU002", 1);
    int total = calculate_total(ctx.cart, ctx.catalog);

    ASSERT_EQ(total, 4500);  // 2*1000 + 1*2500

    teardown_order_test(&ctx);
}

void test_order_total_empty_cart(void) {
    OrderTestCtx ctx = setup_order_test();

    int total = calculate_total(ctx.cart, ctx.catalog);

    ASSERT_EQ(total, 0);

    teardown_order_test(&ctx);
}
```

Este es exactamente el patron que frameworks como Unity y CMocka formalizan con `setUp`/`tearDown`. La diferencia es que aqui lo haces manualmente — pero la estructura es identica.

### 7.5 Cuando el helper es demasiado

Un helper que esconde demasiada logica puede hacer que el test sea opaco:

```c
// MALO — ¿que hace run_order_test? ¿Que verifica? Misterio.
void test_order_scenario_1(void) {
    run_order_test("SKU001", 2, "SKU002", 1, EXPECT_TOTAL, 4500);
}

// Hay que leer la implementacion de run_order_test para entender el test.
// El test dejo de ser documentacion legible.
```

```c
// BIEN — el helper hace el setup, pero el test es explicito
void test_order_total_two_items(void) {
    OrderTestCtx ctx = setup_order_test();

    add_to_cart(ctx.cart, "SKU001", 2);
    add_to_cart(ctx.cart, "SKU002", 1);
    int total = calculate_total(ctx.cart, ctx.catalog);

    ASSERT_EQ(total, 4500);

    teardown_order_test(&ctx);
}
// Puedo leer el test y entender exactamente que pasa sin buscar helpers.
```

Regla: **el helper maneja infraestructura (crear/destruir objetos), el test expresa la logica (que se prueba y que se espera)**.

---

## 8. Estructura interna de archivos de test

### 8.1 Layout recomendado

```c
// test_hash_table.c — layout completo de un archivo de test

// ═══════════════════════════════════════════════════════════════
// 1. Includes
// ═══════════════════════════════════════════════════════════════
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash_table.h"           // Modulo bajo test
#include "test_helpers.h"          // Macros de assert (ASSERT, RUN, etc.)

// ═══════════════════════════════════════════════════════════════
// 2. Helpers y fixtures (static — privados a este archivo)
// ═══════════════════════════════════════════════════════════════

// Fixture
typedef struct {
    HashTable *ht;
} HtTestCtx;

static HtTestCtx setup(void) {
    HtTestCtx ctx = { .ht = ht_new(16) };
    return ctx;
}

static void teardown(HtTestCtx *ctx) {
    ht_free(ctx->ht);
}

// Helper para crear una tabla con N entries precargadas
static HashTable *make_populated_ht(size_t count) {
    HashTable *ht = ht_new(count * 2);
    char key[32], val[32];
    for (size_t i = 0; i < count; i++) {
        snprintf(key, sizeof(key), "key_%zu", i);
        snprintf(val, sizeof(val), "val_%zu", i);
        ht_insert(ht, key, strdup(val));
    }
    return ht;
}

// ═══════════════════════════════════════════════════════════════
// 3. Tests — agrupados por funcionalidad
// ═══════════════════════════════════════════════════════════════

// ─── Insert ──────────────────────────────────────────────────

void test_ht_insert_new_key(void) {
    HtTestCtx ctx = setup();

    bool ok = ht_insert(ctx.ht, "name", "Alice");

    ASSERT_TRUE(ok);
    ASSERT_EQ(ht_count(ctx.ht), 1);

    teardown(&ctx);
}

void test_ht_insert_duplicate_key_overwrites(void) {
    HtTestCtx ctx = setup();

    ht_insert(ctx.ht, "name", "Alice");
    ht_insert(ctx.ht, "name", "Bob");

    ASSERT_STR_EQ(ht_get(ctx.ht, "name"), "Bob");
    ASSERT_EQ(ht_count(ctx.ht), 1);

    teardown(&ctx);
}

void test_ht_insert_null_key_returns_false(void) {
    HtTestCtx ctx = setup();

    ASSERT_FALSE(ht_insert(ctx.ht, NULL, "value"));

    teardown(&ctx);
}

// ─── Get ─────────────────────────────────────────────────────

void test_ht_get_existing_key(void) {
    HtTestCtx ctx = setup();
    ht_insert(ctx.ht, "name", "Alice");

    const char *val = ht_get(ctx.ht, "name");

    ASSERT_STR_EQ(val, "Alice");

    teardown(&ctx);
}

void test_ht_get_missing_key_returns_null(void) {
    HtTestCtx ctx = setup();

    ASSERT(ht_get(ctx.ht, "nonexistent") == NULL);

    teardown(&ctx);
}

// ─── Delete ──────────────────────────────────────────────────

void test_ht_delete_existing_key(void) {
    HtTestCtx ctx = setup();
    ht_insert(ctx.ht, "name", "Alice");

    bool ok = ht_delete(ctx.ht, "name");

    ASSERT_TRUE(ok);
    ASSERT(ht_get(ctx.ht, "name") == NULL);
    ASSERT_EQ(ht_count(ctx.ht), 0);

    teardown(&ctx);
}

void test_ht_delete_missing_key_returns_false(void) {
    HtTestCtx ctx = setup();

    ASSERT_FALSE(ht_delete(ctx.ht, "ghost"));

    teardown(&ctx);
}

// ─── Resize ──────────────────────────────────────────────────

void test_ht_resize_preserves_data(void) {
    // Usar tabla muy pequena para forzar resize
    HashTable *ht = ht_new(2);
    ht_insert(ht, "a", "1");
    ht_insert(ht, "b", "2");

    ht_insert(ht, "c", "3");  // Deberia triggear resize

    ASSERT_STR_EQ(ht_get(ht, "a"), "1");
    ASSERT_STR_EQ(ht_get(ht, "b"), "2");
    ASSERT_STR_EQ(ht_get(ht, "c"), "3");

    ht_free(ht);
}

// ─── Edge cases ──────────────────────────────────────────────

void test_ht_operations_on_null_table(void) {
    ASSERT_FALSE(ht_insert(NULL, "key", "val"));
    ASSERT(ht_get(NULL, "key") == NULL);
    ASSERT_FALSE(ht_delete(NULL, "key"));
    ASSERT_EQ(ht_count(NULL), 0);
    ht_free(NULL);  // No crash
}

void test_ht_empty_string_key(void) {
    HtTestCtx ctx = setup();

    ht_insert(ctx.ht, "", "empty_key_value");
    ASSERT_STR_EQ(ht_get(ctx.ht, ""), "empty_key_value");

    teardown(&ctx);
}

// ═══════════════════════════════════════════════════════════════
// 4. main — registrar y ejecutar todos los tests
// ═══════════════════════════════════════════════════════════════

int main(void) {
    printf("=== Hash Table Tests ===\n\n");

    printf("-- Insert --\n");
    RUN(test_ht_insert_new_key);
    RUN(test_ht_insert_duplicate_key_overwrites);
    RUN(test_ht_insert_null_key_returns_false);

    printf("\n-- Get --\n");
    RUN(test_ht_get_existing_key);
    RUN(test_ht_get_missing_key_returns_null);

    printf("\n-- Delete --\n");
    RUN(test_ht_delete_existing_key);
    RUN(test_ht_delete_missing_key_returns_false);

    printf("\n-- Resize --\n");
    RUN(test_ht_resize_preserves_data);

    printf("\n-- Edge cases --\n");
    RUN(test_ht_operations_on_null_table);
    RUN(test_ht_empty_string_key);

    SUMMARY();
    return test_failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
```

```bash
$ ./test_hash_table
=== Hash Table Tests ===

-- Insert --
  [PASS] test_ht_insert_new_key
  [PASS] test_ht_insert_duplicate_key_overwrites
  [PASS] test_ht_insert_null_key_returns_false

-- Get --
  [PASS] test_ht_get_existing_key
  [PASS] test_ht_get_missing_key_returns_null

-- Delete --
  [PASS] test_ht_delete_existing_key
  [PASS] test_ht_delete_missing_key_returns_false

-- Resize --
  [PASS] test_ht_resize_preserves_data

-- Edge cases --
  [PASS] test_ht_operations_on_null_table
  [PASS] test_ht_empty_string_key

─────────────────────────────────────────
10 passed, 0 failed
OK
```

### 8.2 Orden de los tests en main()

El orden en que listas los tests en `main()` deberia seguir una progresion logica:

```
┌────────────────────────────────────────────────────────────────────────┐
│              ORDEN RECOMENDADO DE TESTS EN main()                     │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  1. Creacion / inicializacion                                         │
│     test_new, test_create, test_init                                  │
│     → Si esto falla, nada mas tiene sentido                           │
│                                                                        │
│  2. Operaciones basicas (happy path)                                  │
│     test_insert, test_push, test_get                                  │
│     → Las operaciones fundamentales funcionan?                        │
│                                                                        │
│  3. Operaciones avanzadas                                             │
│     test_delete, test_resize, test_iterate                            │
│     → Funcionalidad mas compleja                                      │
│                                                                        │
│  4. Edge cases                                                        │
│     test_empty, test_null, test_max_size                              │
│     → Casos borde y limites                                           │
│                                                                        │
│  5. Error handling                                                    │
│     test_invalid_input, test_overflow, test_null_safety               │
│     → Manejo de errores                                               │
│                                                                        │
│  6. Regression tests                                                  │
│     test_bug_47_collision_delete                                      │
│     → Bugs historicos que no deben volver                             │
│                                                                        │
│  7. Destruccion / cleanup                                             │
│     test_free, test_destroy, test_double_free                         │
│     → Verificar que la limpieza funciona                              │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

Este orden no afecta la correccion (los tests son independientes), pero facilita la lectura y el debugging: si los tests de creacion fallan, puedes parar de leer — todo lo demas fallara por la misma razon.

---

## 9. El test como especificacion — Test-First Thinking

### 9.1 Escribir el test antes del codigo

No necesitas practicar TDD (Test-Driven Development) formalmente para beneficiarte de "pensar en tests primero". Antes de implementar una funcion, escribe los tests que deberia pasar. Esto te fuerza a:

1. **Definir el contrato** antes de implementarlo.
2. **Pensar en edge cases** antes de que el codigo exista.
3. **Disenar la API** desde la perspectiva del usuario, no del implementador.

```c
// Paso 1: escribir los tests ANTES de implementar str_split()
// Esto te obliga a decidir:
//   - Que retorna? Un array de strings? Cuantas?
//   - Quien libera la memoria?
//   - Que pasa con delimitadores consecutivos?
//   - Que pasa con string vacia?

void test_str_split_basic(void) {
    char **parts = NULL;
    size_t count = 0;

    int rc = str_split("a,b,c", ',', &parts, &count);

    ASSERT_EQ(rc, 0);
    ASSERT_EQ(count, 3);
    ASSERT_STR_EQ(parts[0], "a");
    ASSERT_STR_EQ(parts[1], "b");
    ASSERT_STR_EQ(parts[2], "c");

    str_split_free(parts, count);
}

void test_str_split_no_delimiter(void) {
    char **parts = NULL;
    size_t count = 0;

    str_split("hello", ',', &parts, &count);

    ASSERT_EQ(count, 1);
    ASSERT_STR_EQ(parts[0], "hello");

    str_split_free(parts, count);
}

void test_str_split_consecutive_delimiters(void) {
    char **parts = NULL;
    size_t count = 0;

    // Decision de diseno: ¿"a,,b" produce ["a", "", "b"] o ["a", "b"]?
    // El test DOCUMENTA la decision:
    str_split("a,,b", ',', &parts, &count);

    ASSERT_EQ(count, 3);           // Produce ["a", "", "b"]
    ASSERT_STR_EQ(parts[0], "a");
    ASSERT_STR_EQ(parts[1], "");   // String vacia entre delimitadores
    ASSERT_STR_EQ(parts[2], "b");

    str_split_free(parts, count);
}

void test_str_split_empty_string(void) {
    char **parts = NULL;
    size_t count = 0;

    str_split("", ',', &parts, &count);

    ASSERT_EQ(count, 1);
    ASSERT_STR_EQ(parts[0], "");

    str_split_free(parts, count);
}

void test_str_split_null_input(void) {
    char **parts = NULL;
    size_t count = 0;

    int rc = str_split(NULL, ',', &parts, &count);

    ASSERT_EQ(rc, -1);  // Error
    ASSERT(parts == NULL);
    ASSERT_EQ(count, 0);
}

void test_str_split_only_delimiters(void) {
    char **parts = NULL;
    size_t count = 0;

    str_split(",,,", ',', &parts, &count);

    ASSERT_EQ(count, 4);  // ["", "", "", ""]
    for (size_t i = 0; i < count; i++) {
        ASSERT_STR_EQ(parts[i], "");
    }

    str_split_free(parts, count);
}
```

Escribir estos tests **antes** de implementar `str_split` te forzo a decidir:
- El prototipo: `int str_split(const char *str, char delim, char ***out, size_t *count)`
- Retorno: 0 = exito, -1 = error
- Delimitadores consecutivos producen strings vacias (como `strsep`, no como `strtok`)
- String vacia produce un array de 1 elemento (string vacia)
- NULL input retorna error, no crash
- La memoria se libera con `str_split_free(parts, count)`

### 9.2 El test como contrato

Los tests escritos asi son un **contrato formal**. Si alguien cambia el comportamiento de `str_split` para tratar delimitadores consecutivos de forma diferente, el test `test_str_split_consecutive_delimiters` fallara inmediatamente. Eso es exactamente lo que quieres — el test protege la decision de diseno.

---

## 10. Comparacion con Rust y Go

### 10.1 AAA en tres lenguajes

**C:**

```c
void test_vec_push_increases_length(void) {
    // Arrange
    Vec *v = vec_new(4);

    // Act
    vec_push(v, 42);

    // Assert
    ASSERT_EQ(vec_len(v), 1);

    // Cleanup (manual, obligatorio)
    vec_free(v);
}
```

**Rust:**

```rust
#[test]
fn test_vec_push_increases_length() {
    // Arrange
    let mut v = Vec::new();

    // Act
    v.push(42);

    // Assert
    assert_eq!(v.len(), 1);

    // Cleanup: automatico (Drop trait)
}
```

**Go:**

```go
func TestVecPushIncreasesLength(t *testing.T) {
    // Arrange
    v := NewVec(4)

    // Act
    v.Push(42)

    // Assert
    if v.Len() != 1 {
        t.Errorf("Len() = %d, want 1", v.Len())
    }

    // Cleanup: GC se encarga (o t.Cleanup para recursos)
}
```

### 10.2 Diferencias clave

| Aspecto | C | Rust | Go |
|---------|---|------|----|
| **Cleanup** | Manual (`free`, `fclose`) — el programador **debe** recordar | Automatico (Drop) — imposible olvidar | GC para memoria; `t.Cleanup()` para recursos |
| **Aislamiento** | Mismo proceso, mismo address space — un crash mata todos | Cada test en su thread, panic atrapado por el runner | Cada test en su goroutine, t.Fatal no mata el proceso |
| **Nombre del test** | Funcion C — `test_mod_op_cond` (underscore_case) | Funcion Rust — `test_op_cond` (snake_case, igual pero con `#[test]`) | Funcion Go — `TestOpCond` (CamelCase, debe empezar con mayuscula) |
| **Subtests** | No built-in — funciones separadas | Closure o macro | `t.Run("nombre", func)` — primera clase |
| **Helper output** | `__FILE__:__LINE__` via macros | `#[track_caller]` (estable desde Rust 1.46) | `t.Helper()` — el error reporta la linea del caller |
| **Asserts** | Macros custom o `assert.h` | `assert!`, `assert_eq!`, `assert_ne!` | No tiene — `if got != want { t.Errorf(...) }` |

### 10.3 Subtests — C vs Rust vs Go

En Rust y Go, los subtests permiten agrupar verificaciones relacionadas bajo un test padre:

**Go (subtests nativos):**

```go
func TestClamp(t *testing.T) {
    t.Run("below min", func(t *testing.T) {
        if got := Clamp(-10, 0, 100); got != 0 {
            t.Errorf("got %d, want 0", got)
        }
    })
    t.Run("above max", func(t *testing.T) {
        if got := Clamp(200, 0, 100); got != 100 {
            t.Errorf("got %d, want 100", got)
        }
    })
    t.Run("within range", func(t *testing.T) {
        if got := Clamp(50, 0, 100); got != 50 {
            t.Errorf("got %d, want 50", got)
        }
    })
}
```

**C (simulacion con funciones):**

```c
// En C no hay subtests, pero puedes simularlos con estructura:
void test_clamp_below_min(void) {
    ASSERT_EQ(clamp(-10, 0, 100), 0);
}

void test_clamp_above_max(void) {
    ASSERT_EQ(clamp(200, 0, 100), 100);
}

void test_clamp_within_range(void) {
    ASSERT_EQ(clamp(50, 0, 100), 50);
}

// Agrupacion en main():
int main(void) {
    printf("-- clamp --\n");
    RUN(test_clamp_below_min);
    RUN(test_clamp_above_max);
    RUN(test_clamp_within_range);
    // ...
}
```

La ventaja de los subtests en Go/Rust es que puedes filtrar ejecucion por nombre: `go test -run TestClamp/below_min`. En C, cada test es una funcion independiente — no hay filtering a menos que lo implementes manualmente.

---

## 11. Ejemplo completo — Linked List con tests estructurados

### 11.1 API de la linked list

```c
// linked_list.h
#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Node {
    int data;
    struct Node *next;
} Node;

typedef struct {
    Node *head;
    size_t len;
} LinkedList;

LinkedList *ll_new(void);
void        ll_free(LinkedList *list);

// Insercion
void ll_push_front(LinkedList *list, int value);
void ll_push_back(LinkedList *list, int value);
bool ll_insert_at(LinkedList *list, size_t index, int value);

// Eliminacion
bool ll_pop_front(LinkedList *list, int *out);
bool ll_pop_back(LinkedList *list, int *out);
bool ll_remove_at(LinkedList *list, size_t index, int *out);

// Consulta
bool   ll_get(const LinkedList *list, size_t index, int *out);
size_t ll_len(const LinkedList *list);
bool   ll_is_empty(const LinkedList *list);
bool   ll_contains(const LinkedList *list, int value);
int    ll_find(const LinkedList *list, int value);  // retorna indice o -1

// Operaciones
void ll_reverse(LinkedList *list);
void ll_clear(LinkedList *list);  // vacia sin destruir la lista

#endif
```

### 11.2 Tests completos

```c
// test_linked_list.c
#include <stdio.h>
#include <stdlib.h>
#include "linked_list.h"

// ═══════════════════════════════════════════════════════════════
// Micro-framework (reutilizable entre archivos de test)
// ═══════════════════════════════════════════════════════════════

static int _pass = 0, _fail = 0;

#define ASSERT(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr); \
        _fail++; return; \
    } _pass++; \
} while (0)

#define ASSERT_EQ(a, b) do { \
    long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { \
        fprintf(stderr, "  FAIL [%s:%d] expected %ld, got %ld\n", \
                __FILE__, __LINE__, _b, _a); \
        _fail++; return; \
    } _pass++; \
} while (0)

#define ASSERT_TRUE(e)  ASSERT(e)
#define ASSERT_FALSE(e) ASSERT(!(e))

#define RUN(fn) do { \
    int _bf = _fail; fn(); \
    printf("  [%s] %s\n", _fail == _bf ? "PASS" : "FAIL", #fn); \
} while (0)

// ═══════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════

// Crea una lista con los valores dados
static LinkedList *make_list(int *values, size_t count) {
    LinkedList *list = ll_new();
    for (size_t i = 0; i < count; i++) {
        ll_push_back(list, values[i]);
    }
    return list;
}

// Verifica que la lista tiene exactamente los valores dados, en orden
static void assert_list_contents(LinkedList *list, int *expected, size_t count,
                                  const char *file, int line) {
    if (ll_len(list) != count) {
        fprintf(stderr, "  FAIL [%s:%d] list len = %zu, expected %zu\n",
                file, line, ll_len(list), count);
        _fail++;
        return;
    }
    for (size_t i = 0; i < count; i++) {
        int val;
        if (!ll_get(list, i, &val) || val != expected[i]) {
            fprintf(stderr, "  FAIL [%s:%d] list[%zu] = %d, expected %d\n",
                    file, line, i, val, expected[i]);
            _fail++;
            return;
        }
    }
    _pass++;
}

#define ASSERT_LIST(list, ...) do { \
    int _exp[] = { __VA_ARGS__ }; \
    assert_list_contents(list, _exp, sizeof(_exp)/sizeof(_exp[0]), \
                         __FILE__, __LINE__); \
} while (0)

// ═══════════════════════════════════════════════════════════════
// Tests: Creacion
// ═══════════════════════════════════════════════════════════════

void test_ll_new_creates_empty_list(void) {
    LinkedList *list = ll_new();

    ASSERT(list != NULL);
    ASSERT_EQ(ll_len(list), 0);
    ASSERT_TRUE(ll_is_empty(list));

    ll_free(list);
}

// ═══════════════════════════════════════════════════════════════
// Tests: push_front
// ═══════════════════════════════════════════════════════════════

void test_ll_push_front_on_empty(void) {
    LinkedList *list = ll_new();

    ll_push_front(list, 42);

    ASSERT_EQ(ll_len(list), 1);
    int val;
    ASSERT_TRUE(ll_get(list, 0, &val));
    ASSERT_EQ(val, 42);

    ll_free(list);
}

void test_ll_push_front_prepends(void) {
    LinkedList *list = ll_new();

    ll_push_front(list, 30);
    ll_push_front(list, 20);
    ll_push_front(list, 10);

    // El ultimo push_front esta al indice 0
    ASSERT_LIST(list, 10, 20, 30);

    ll_free(list);
}

// ═══════════════════════════════════════════════════════════════
// Tests: push_back
// ═══════════════════════════════════════════════════════════════

void test_ll_push_back_on_empty(void) {
    LinkedList *list = ll_new();

    ll_push_back(list, 42);

    ASSERT_EQ(ll_len(list), 1);
    int val;
    ASSERT_TRUE(ll_get(list, 0, &val));
    ASSERT_EQ(val, 42);

    ll_free(list);
}

void test_ll_push_back_appends(void) {
    LinkedList *list = ll_new();

    ll_push_back(list, 10);
    ll_push_back(list, 20);
    ll_push_back(list, 30);

    ASSERT_LIST(list, 10, 20, 30);

    ll_free(list);
}

// ═══════════════════════════════════════════════════════════════
// Tests: insert_at
// ═══════════════════════════════════════════════════════════════

void test_ll_insert_at_beginning(void) {
    int data[] = {20, 30};
    LinkedList *list = make_list(data, 2);

    ll_insert_at(list, 0, 10);

    ASSERT_LIST(list, 10, 20, 30);

    ll_free(list);
}

void test_ll_insert_at_middle(void) {
    int data[] = {10, 30};
    LinkedList *list = make_list(data, 2);

    ll_insert_at(list, 1, 20);

    ASSERT_LIST(list, 10, 20, 30);

    ll_free(list);
}

void test_ll_insert_at_end(void) {
    int data[] = {10, 20};
    LinkedList *list = make_list(data, 2);

    ll_insert_at(list, 2, 30);

    ASSERT_LIST(list, 10, 20, 30);

    ll_free(list);
}

void test_ll_insert_at_invalid_index_returns_false(void) {
    int data[] = {10, 20};
    LinkedList *list = make_list(data, 2);

    ASSERT_FALSE(ll_insert_at(list, 5, 99));
    ASSERT_EQ(ll_len(list), 2);  // Lista no modificada

    ll_free(list);
}

// ═══════════════════════════════════════════════════════════════
// Tests: pop_front
// ═══════════════════════════════════════════════════════════════

void test_ll_pop_front_returns_first(void) {
    int data[] = {10, 20, 30};
    LinkedList *list = make_list(data, 3);

    int val;
    ASSERT_TRUE(ll_pop_front(list, &val));
    ASSERT_EQ(val, 10);
    ASSERT_EQ(ll_len(list), 2);

    ll_free(list);
}

void test_ll_pop_front_empty_returns_false(void) {
    LinkedList *list = ll_new();

    int val = -1;
    ASSERT_FALSE(ll_pop_front(list, &val));
    ASSERT_EQ(val, -1);  // No modificado

    ll_free(list);
}

void test_ll_pop_front_single_element_leaves_empty(void) {
    LinkedList *list = ll_new();
    ll_push_back(list, 42);

    int val;
    ASSERT_TRUE(ll_pop_front(list, &val));
    ASSERT_EQ(val, 42);
    ASSERT_TRUE(ll_is_empty(list));

    ll_free(list);
}

// ═══════════════════════════════════════════════════════════════
// Tests: pop_back
// ═══════════════════════════════════════════════════════════════

void test_ll_pop_back_returns_last(void) {
    int data[] = {10, 20, 30};
    LinkedList *list = make_list(data, 3);

    int val;
    ASSERT_TRUE(ll_pop_back(list, &val));
    ASSERT_EQ(val, 30);
    ASSERT_EQ(ll_len(list), 2);

    ll_free(list);
}

void test_ll_pop_back_empty_returns_false(void) {
    LinkedList *list = ll_new();

    ASSERT_FALSE(ll_pop_back(list, NULL));

    ll_free(list);
}

// ═══════════════════════════════════════════════════════════════
// Tests: remove_at
// ═══════════════════════════════════════════════════════════════

void test_ll_remove_at_middle(void) {
    int data[] = {10, 20, 30};
    LinkedList *list = make_list(data, 3);

    int val;
    ASSERT_TRUE(ll_remove_at(list, 1, &val));
    ASSERT_EQ(val, 20);
    ASSERT_LIST(list, 10, 30);

    ll_free(list);
}

void test_ll_remove_at_invalid_index(void) {
    int data[] = {10, 20};
    LinkedList *list = make_list(data, 2);

    ASSERT_FALSE(ll_remove_at(list, 5, NULL));
    ASSERT_EQ(ll_len(list), 2);

    ll_free(list);
}

// ═══════════════════════════════════════════════════════════════
// Tests: Consulta
// ═══════════════════════════════════════════════════════════════

void test_ll_get_valid_index(void) {
    int data[] = {10, 20, 30};
    LinkedList *list = make_list(data, 3);

    int val;
    ASSERT_TRUE(ll_get(list, 1, &val));
    ASSERT_EQ(val, 20);

    ll_free(list);
}

void test_ll_get_invalid_index(void) {
    int data[] = {10};
    LinkedList *list = make_list(data, 1);

    ASSERT_FALSE(ll_get(list, 5, NULL));

    ll_free(list);
}

void test_ll_contains_existing_value(void) {
    int data[] = {10, 20, 30};
    LinkedList *list = make_list(data, 3);

    ASSERT_TRUE(ll_contains(list, 20));

    ll_free(list);
}

void test_ll_contains_missing_value(void) {
    int data[] = {10, 20, 30};
    LinkedList *list = make_list(data, 3);

    ASSERT_FALSE(ll_contains(list, 99));

    ll_free(list);
}

void test_ll_find_existing_returns_index(void) {
    int data[] = {10, 20, 30};
    LinkedList *list = make_list(data, 3);

    ASSERT_EQ(ll_find(list, 20), 1);

    ll_free(list);
}

void test_ll_find_missing_returns_negative(void) {
    int data[] = {10, 20, 30};
    LinkedList *list = make_list(data, 3);

    ASSERT_EQ(ll_find(list, 99), -1);

    ll_free(list);
}

// ═══════════════════════════════════════════════════════════════
// Tests: reverse
// ═══════════════════════════════════════════════════════════════

void test_ll_reverse_three_elements(void) {
    int data[] = {10, 20, 30};
    LinkedList *list = make_list(data, 3);

    ll_reverse(list);

    ASSERT_LIST(list, 30, 20, 10);

    ll_free(list);
}

void test_ll_reverse_single_element(void) {
    LinkedList *list = ll_new();
    ll_push_back(list, 42);

    ll_reverse(list);

    int val;
    ASSERT_TRUE(ll_get(list, 0, &val));
    ASSERT_EQ(val, 42);

    ll_free(list);
}

void test_ll_reverse_empty(void) {
    LinkedList *list = ll_new();

    ll_reverse(list);  // No crash

    ASSERT_TRUE(ll_is_empty(list));

    ll_free(list);
}

void test_ll_reverse_twice_restores_original(void) {
    int data[] = {10, 20, 30, 40, 50};
    LinkedList *list = make_list(data, 5);

    ll_reverse(list);
    ll_reverse(list);

    ASSERT_LIST(list, 10, 20, 30, 40, 50);

    ll_free(list);
}

// ═══════════════════════════════════════════════════════════════
// Tests: clear
// ═══════════════════════════════════════════════════════════════

void test_ll_clear_makes_empty(void) {
    int data[] = {10, 20, 30};
    LinkedList *list = make_list(data, 3);

    ll_clear(list);

    ASSERT_TRUE(ll_is_empty(list));
    ASSERT_EQ(ll_len(list), 0);

    ll_free(list);
}

void test_ll_clear_then_reuse(void) {
    int data[] = {10, 20, 30};
    LinkedList *list = make_list(data, 3);

    ll_clear(list);
    ll_push_back(list, 99);

    ASSERT_EQ(ll_len(list), 1);
    int val;
    ll_get(list, 0, &val);
    ASSERT_EQ(val, 99);

    ll_free(list);
}

// ═══════════════════════════════════════════════════════════════
// Tests: Null safety
// ═══════════════════════════════════════════════════════════════

void test_ll_null_safety(void) {
    // Ninguna de estas debe crashear
    ll_push_front(NULL, 1);
    ll_push_back(NULL, 1);
    ASSERT_FALSE(ll_insert_at(NULL, 0, 1));
    ASSERT_FALSE(ll_pop_front(NULL, NULL));
    ASSERT_FALSE(ll_pop_back(NULL, NULL));
    ASSERT_FALSE(ll_remove_at(NULL, 0, NULL));
    ASSERT_FALSE(ll_get(NULL, 0, NULL));
    ASSERT_EQ(ll_len(NULL), 0);
    ASSERT_TRUE(ll_is_empty(NULL));
    ASSERT_FALSE(ll_contains(NULL, 1));
    ASSERT_EQ(ll_find(NULL, 1), -1);
    ll_reverse(NULL);
    ll_clear(NULL);
    ll_free(NULL);
}

// ═══════════════════════════════════════════════════════════════
// main
// ═══════════════════════════════════════════════════════════════

int main(void) {
    printf("=== Linked List Tests ===\n\n");

    printf("-- Creation --\n");
    RUN(test_ll_new_creates_empty_list);

    printf("\n-- push_front --\n");
    RUN(test_ll_push_front_on_empty);
    RUN(test_ll_push_front_prepends);

    printf("\n-- push_back --\n");
    RUN(test_ll_push_back_on_empty);
    RUN(test_ll_push_back_appends);

    printf("\n-- insert_at --\n");
    RUN(test_ll_insert_at_beginning);
    RUN(test_ll_insert_at_middle);
    RUN(test_ll_insert_at_end);
    RUN(test_ll_insert_at_invalid_index_returns_false);

    printf("\n-- pop_front --\n");
    RUN(test_ll_pop_front_returns_first);
    RUN(test_ll_pop_front_empty_returns_false);
    RUN(test_ll_pop_front_single_element_leaves_empty);

    printf("\n-- pop_back --\n");
    RUN(test_ll_pop_back_returns_last);
    RUN(test_ll_pop_back_empty_returns_false);

    printf("\n-- remove_at --\n");
    RUN(test_ll_remove_at_middle);
    RUN(test_ll_remove_at_invalid_index);

    printf("\n-- Consulta --\n");
    RUN(test_ll_get_valid_index);
    RUN(test_ll_get_invalid_index);
    RUN(test_ll_contains_existing_value);
    RUN(test_ll_contains_missing_value);
    RUN(test_ll_find_existing_returns_index);
    RUN(test_ll_find_missing_returns_negative);

    printf("\n-- reverse --\n");
    RUN(test_ll_reverse_three_elements);
    RUN(test_ll_reverse_single_element);
    RUN(test_ll_reverse_empty);
    RUN(test_ll_reverse_twice_restores_original);

    printf("\n-- clear --\n");
    RUN(test_ll_clear_makes_empty);
    RUN(test_ll_clear_then_reuse);

    printf("\n-- Null safety --\n");
    RUN(test_ll_null_safety);

    printf("\n═════════════════════════════════════════\n");
    printf("Assertions: %d passed, %d failed\n", _pass, _fail);
    return _fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
```

```bash
$ gcc -Wall -Wextra -std=c11 -g -fsanitize=address,undefined \
      -o test_ll test_linked_list.c linked_list.c
$ ./test_ll
=== Linked List Tests ===

-- Creation --
  [PASS] test_ll_new_creates_empty_list

-- push_front --
  [PASS] test_ll_push_front_on_empty
  [PASS] test_ll_push_front_prepends

-- push_back --
  [PASS] test_ll_push_back_on_empty
  [PASS] test_ll_push_back_appends

-- insert_at --
  [PASS] test_ll_insert_at_beginning
  [PASS] test_ll_insert_at_middle
  [PASS] test_ll_insert_at_end
  [PASS] test_ll_insert_at_invalid_index_returns_false

-- pop_front --
  [PASS] test_ll_pop_front_returns_first
  [PASS] test_ll_pop_front_empty_returns_false
  [PASS] test_ll_pop_front_single_element_leaves_empty

-- pop_back --
  [PASS] test_ll_pop_back_returns_last
  [PASS] test_ll_pop_back_empty_returns_false

-- remove_at --
  [PASS] test_ll_remove_at_middle
  [PASS] test_ll_remove_at_invalid_index

-- Consulta --
  [PASS] test_ll_get_valid_index
  [PASS] test_ll_get_invalid_index
  [PASS] test_ll_contains_existing_value
  [PASS] test_ll_contains_missing_value
  [PASS] test_ll_find_existing_returns_index
  [PASS] test_ll_find_missing_returns_negative

-- reverse --
  [PASS] test_ll_reverse_three_elements
  [PASS] test_ll_reverse_single_element
  [PASS] test_ll_reverse_empty
  [PASS] test_ll_reverse_twice_restores_original

-- clear --
  [PASS] test_ll_clear_makes_empty
  [PASS] test_ll_clear_then_reuse

-- Null safety --
  [PASS] test_ll_null_safety

═════════════════════════════════════════
Assertions: 52 passed, 0 failed
OK
```

Observa la estructura del archivo de test:
1. **Micro-framework** (macros reutilizables)
2. **Helpers** (`make_list`, `ASSERT_LIST`)
3. **Tests agrupados por operacion** (push_front, push_back, insert_at, etc.)
4. **Progresion logica** (creacion → insercion → eliminacion → consulta → operaciones → edge cases)
5. **Cada test es independiente** — crea su lista, opera, verifica, y libera

---

## 12. Errores comunes

### 12.1 Test que depende de la implementacion, no del contrato

```c
// MALO — acopla el test a la implementacion interna
void test_vec_push_doubles_capacity(void) {
    Vec *v = vec_new(4);
    for (int i = 0; i < 5; i++) vec_push(v, i);

    // ¿Por que el test sabe que la capacidad se duplica?
    // Si cambias la estrategia de crecimiento (1.5x, grow by 8, etc.)
    // este test se rompe sin que haya un bug.
    ASSERT_EQ(v->cap, 8);  // Accede a campo interno

    vec_free(v);
}

// BIEN — verifica el comportamiento observable
void test_vec_push_beyond_capacity_succeeds(void) {
    Vec *v = vec_new(4);
    for (int i = 0; i < 100; i++) {
        ASSERT_TRUE(vec_push(v, i));
    }
    ASSERT_EQ(vec_len(v), 100);

    // Verificar que los datos estan correctos
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(vec_get(v, i), i);
    }

    vec_free(v);
}
```

### 12.2 Test sin Assert (false positive)

```c
// MALO — este test siempre pasa, nunca falla
void test_parser_processes_input(void) {
    Parser *p = parser_new();
    parser_feed(p, "some input");
    parser_run(p);
    // ... y? No verifica nada.
    // Si parser_run() segfaultea, falla por crash.
    // Si parser_run() produce resultados incorrectos, pasa igual.
    parser_free(p);
}

// BIEN — verifica el resultado
void test_parser_produces_expected_ast(void) {
    Parser *p = parser_new();
    parser_feed(p, "2 + 3");

    AstNode *ast = parser_run(p);

    ASSERT(ast != NULL);
    ASSERT_EQ(ast->type, NODE_BINOP);
    ASSERT_EQ(ast->op, OP_ADD);
    ASSERT_EQ(ast->left->value, 2);
    ASSERT_EQ(ast->right->value, 3);

    parser_free(p);
}
```

### 12.3 Arrange demasiado complejo que oscurece el test

```c
// MALO — 20 lineas de setup para verificar una cosa simple
void test_user_can_login(void) {
    Database *db = db_connect("localhost", 5432, "test_db");
    db_migrate(db);
    User *admin = user_create(db, "admin", "admin@test.com");
    user_set_role(admin, ROLE_ADMIN);
    user_save(db, admin);
    Config *config = config_load("test.ini");
    config_set(config, "auth.method", "password");
    AuthService *auth = auth_new(db, config);
    Session *session = session_new();
    Request *req = request_new();
    request_set_header(req, "Content-Type", "application/json");
    request_set_body(req, "{\"user\":\"admin\",\"pass\":\"secret\"}");

    // Ya perdi de vista que se esta testeando
    bool ok = auth_login(auth, req, session);

    ASSERT_TRUE(ok);
    // ... cleanup de 10 lineas ...
}

// BIEN — extraer el Arrange a un helper
typedef struct {
    Database *db;
    AuthService *auth;
    Session *session;
} AuthTestCtx;

static AuthTestCtx setup_auth_test(void) {
    // ... todo el setup complejo va aqui ...
}
static void teardown_auth_test(AuthTestCtx *ctx) {
    // ... cleanup ...
}

void test_user_can_login_with_valid_credentials(void) {
    AuthTestCtx ctx = setup_auth_test();

    Request *req = make_login_request("admin", "secret");
    bool ok = auth_login(ctx.auth, req, ctx.session);

    ASSERT_TRUE(ok);

    request_free(req);
    teardown_auth_test(&ctx);
}
```

---

## 13. Programa de practica

### Objetivo

Implementar y testear un **string builder** — una estructura para construir strings por concatenacion eficiente (evitando multiples `realloc` en cada append).

### Especificacion

```c
// string_builder.h
#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char  *data;
    size_t len;    // caracteres escritos (sin contar '\0')
    size_t cap;    // capacidad total del buffer
} StringBuilder;

StringBuilder *sb_new(size_t initial_cap);
void           sb_free(StringBuilder *sb);

// Append operations
bool sb_append_char(StringBuilder *sb, char c);
bool sb_append_str(StringBuilder *sb, const char *str);
bool sb_append_fmt(StringBuilder *sb, const char *fmt, ...);
bool sb_append_int(StringBuilder *sb, int value);

// Consulta
size_t      sb_len(const StringBuilder *sb);
const char *sb_str(const StringBuilder *sb);  // retorna puntero al string interno (NUL-terminated)
bool        sb_is_empty(const StringBuilder *sb);

// Operaciones
char *sb_to_string(StringBuilder *sb);  // retorna copia (caller hace free), resetea el builder
void  sb_clear(StringBuilder *sb);      // vacia sin liberar

#endif
```

### Tests que escribir (siguiendo los principios de este topico)

Escribe tests siguiendo estos principios:
1. **AAA**: cada test tiene Arrange, Act, Assert, Cleanup claramente separados
2. **Un test = un comportamiento**: nombres descriptivos, sin AND
3. **Independientes**: cada test crea y destruye su propio StringBuilder
4. **Helpers**: crea un helper `make_sb_with(const char *initial)` para reducir boilerplate

Tests sugeridos (como minimo):

| Grupo | Tests |
|-------|-------|
| Creacion | `test_sb_new_creates_empty_builder`, `test_sb_new_zero_cap_uses_default` |
| append_char | `test_sb_append_char_single`, `test_sb_append_char_multiple_builds_string`, `test_sb_append_char_triggers_growth` |
| append_str | `test_sb_append_str_basic`, `test_sb_append_str_empty_string_no_effect`, `test_sb_append_str_null_returns_false`, `test_sb_append_str_concatenates` |
| append_fmt | `test_sb_append_fmt_integer`, `test_sb_append_fmt_string_and_int`, `test_sb_append_fmt_multiple_calls` |
| append_int | `test_sb_append_int_positive`, `test_sb_append_int_negative`, `test_sb_append_int_zero` |
| Consulta | `test_sb_len_after_appends`, `test_sb_str_is_nul_terminated`, `test_sb_is_empty_on_new` |
| to_string | `test_sb_to_string_returns_copy`, `test_sb_to_string_resets_builder` |
| clear | `test_sb_clear_resets_to_empty`, `test_sb_clear_then_reuse` |
| Null safety | `test_sb_null_safety` |
| Growth | `test_sb_append_large_string_triggers_multiple_reallocs` |

---

## 14. Ejercicios

### Ejercicio 1: Refactorizar tests malos

Dado este test monolitico, refactorizalo en tests individuales con nombres descriptivos, AAA, y independencia total:

```c
void test_stack(void) {
    Stack *s = stack_new(2);
    assert(s != NULL);
    assert(stack_is_empty(s));
    stack_push(s, 10);
    stack_push(s, 20);
    assert(!stack_is_empty(s));
    assert(stack_len(s) == 2);
    stack_push(s, 30);  // triggers realloc
    assert(stack_len(s) == 3);
    int val;
    stack_pop(s, &val);
    assert(val == 30);
    stack_pop(s, &val);
    assert(val == 20);
    stack_pop(s, &val);
    assert(val == 10);
    assert(stack_is_empty(s));
    assert(!stack_pop(s, &val));
    stack_free(s);
}
```

Deberia resultar en al menos 6 funciones de test separadas.

### Ejercicio 2: Nombrado

Para cada uno de estos tests con nombres malos, escribe un nombre mejor:

```
test_1                     → ???
test_insert                → ???
test_error                 → ???
test_hash_table            → ???
test_boundary              → ???
test_it_works              → ???
test_new_test              → ???
```

Pista: necesitas inventar el contexto (que modulo, que operacion, que condicion) porque los nombres no te dan informacion.

### Ejercicio 3: Identificar violaciones

Lee este archivo de tests e identifica todas las violaciones de los principios cubiertos en este topico (independencia, AAA, un test = una cosa, nombres):

```c
static char *shared_buffer = NULL;

void test_1(void) {
    shared_buffer = malloc(100);
    strcpy(shared_buffer, "hello");
    assert(strlen(shared_buffer) == 5);
}

void test_2(void) {
    strcat(shared_buffer, " world");
    assert(strlen(shared_buffer) == 11);
    assert(strcmp(shared_buffer, "hello world") == 0);
}

void test_3(void) {
    free(shared_buffer);
    shared_buffer = NULL;
}
```

Para cada violacion, explica el problema y muestra como corregirlo.

### Ejercicio 4: Test-first

Antes de implementar `sb_append_fmt` del programa de practica, escribe al menos 5 tests que definan completamente su comportamiento. Los tests deben cubrir:
- Formato basico con `%d` y `%s`
- Multiples llamadas consecutivas a `sb_append_fmt`
- Formato con string vacio
- NULL como argumento de formato (comportamiento de error)
- Formato largo que excede la capacidad actual

---

## 15. Resumen

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                 ESTRUCTURA DE UN TEST — MAPA MENTAL                           │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  Arrange-Act-Assert (AAA)                                                     │
│  ├─ Arrange: preparar estado, crear objetos, configurar precondiciones        │
│  ├─ Act: ejecutar UNA operacion (la que se quiere verificar)                  │
│  ├─ Assert: comprobar resultado y efectos                                     │
│  └─ Cleanup: free(), fclose() — en C casi siempre necesario                  │
│                                                                                │
│  Un test = una verificacion                                                   │
│  ├─ Cada funcion de test verifica UN comportamiento                           │
│  ├─ Si necesitas "and" en el nombre → probablemente son dos tests            │
│  ├─ Multiples asserts estan bien si verifican facetas del MISMO               │
│  │   comportamiento                                                           │
│  └─ El test monolitico es el antipatron mas comun                            │
│                                                                                │
│  Nombres descriptivos                                                         │
│  ├─ Patron: test_modulo_operacion_condicion_resultado                         │
│  ├─ Un nombre bueno te dice que fallo sin abrir el archivo                   │
│  ├─ Abreviar el modulo, nunca la condicion ni el resultado                   │
│  └─ Prefijo test_ siempre                                                    │
│                                                                                │
│  Independencia                                                                │
│  ├─ Sin estado global compartido entre tests                                 │
│  ├─ Cada test crea y destruye su propio estado                               │
│  ├─ Ejecutable en cualquier orden                                             │
│  ├─ Cuidado con: errno, variables static, singletons, archivos temporales    │
│  └─ La duplicacion en tests es preferible a la dependencia                   │
│                                                                                │
│  Helpers                                                                      │
│  ├─ setup()/teardown() — fixture manual                                       │
│  ├─ make_xxx() — helpers de creacion                                          │
│  ├─ El helper maneja infraestructura, el test expresa logica                 │
│  └─ Un helper que esconde la logica del test es peor que la duplicacion      │
│                                                                                │
│  El test como documentacion                                                   │
│  ├─ Documenta el contrato del API — ejecutable y siempre actualizada         │
│  ├─ Regression tests: documentan bugs corregidos con referencia al issue      │
│  └─ Test-first: escribir tests antes del codigo fuerza diseno del contrato   │
│                                                                                │
│  Proximo topico: T03 — Compilacion condicional para tests (#ifdef TEST)       │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

**Navegacion**:
← Anterior: T01 — Test sin framework | Siguiente: T03 — Compilacion condicional para tests →

---

*Bloque 17 — Testing e Ingenieria de Software > Capitulo 1 — Testing en C > Seccion 1 — Fundamentos > Topico 2 — Estructura de un test*
