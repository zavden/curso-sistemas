# Compilacion condicional para tests — #ifdef TEST, test targets en Makefile, separar codigo de produccion y test

## 1. Introduccion

En T01 y T02 aprendimos a escribir tests como programas independientes (`test_math.c` con su propio `main()`). Eso funciona, pero tiene una limitacion: **solo puedes testear la interfaz publica** — las funciones declaradas en el `.h`. Las funciones `static` (internas al `.c`) y las variables internas son invisibles desde otro archivo.

La compilacion condicional resuelve esto y mas: permite **incrustar codigo de test dentro del mismo archivo fuente**, activandolo solo cuando compilas en modo test.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│            DOS ESTRATEGIAS DE TESTING EN C                                      │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Estrategia 1: Archivos separados (T01/T02)                                    │
│  ──────────────────────────────────────────                                      │
│                                                                                  │
│  math.c ──┐                                                                     │
│  math.h ──┼──→ gcc -o test_math test_math.c math.c                             │
│  test_math.c ─┘                                                                 │
│                                                                                  │
│  ✓ Separacion clara                                                             │
│  ✓ El test no contamina el codigo fuente                                        │
│  ✗ No puede testear funciones static                                            │
│  ✗ No puede acceder a estado interno                                            │
│                                                                                  │
│  Estrategia 2: Compilacion condicional (este topico)                            │
│  ──────────────────────────────────────────────────                              │
│                                                                                  │
│  math.c ──→ gcc -o math math.c              (produccion: #ifdef TEST = false)   │
│         └─→ gcc -DTEST -o test_math math.c   (test: #ifdef TEST = true)         │
│                                                                                  │
│  ✓ Puede testear funciones static                                               │
│  ✓ Puede acceder a estado interno                                               │
│  ✓ Test junto al codigo que testea                                              │
│  ✗ El archivo fuente es mas largo                                               │
│  ✗ Mezcla dos preocupaciones en un archivo                                      │
│                                                                                  │
│  En la practica: se usan AMBAS segun el caso                                    │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 1.1 Que es la compilacion condicional

El preprocesador de C (`cpp`) puede incluir o excluir bloques de codigo basandose en macros definidas. Esto ocurre **antes** de la compilacion propiamente dicha — el compilador nunca ve el codigo excluido.

```c
#ifdef TEST
    // Este codigo SOLO existe cuando compilas con -DTEST
    // Si compilas sin -DTEST, es como si estas lineas no existieran
#endif
```

```bash
# Compilacion normal — el preprocesador elimina todo lo que esta dentro de #ifdef TEST
$ gcc -o program program.c

# Compilacion para test — el preprocesador INCLUYE todo lo que esta dentro de #ifdef TEST
$ gcc -DTEST -o test_program program.c
```

`-DTEST` es equivalente a poner `#define TEST` al inicio del archivo. La `D` viene de "Define".

### 1.2 Como funciona internamente

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  Archivo fuente: math.c                                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  int sum(int a, int b) {                                                   │
│      return a + b;                                                         │
│  }                                                                          │
│                                                                             │
│  #ifdef TEST                                                               │
│  #include <assert.h>                                                       │
│  int main(void) {                                                          │
│      assert(sum(2, 3) == 5);                                               │
│      return 0;                                                             │
│  }                                                                          │
│  #endif                                                                    │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  gcc math.c -o math          gcc -DTEST math.c -o test_math                │
│  ────────────────────        ─────────────────────────────                  │
│  Preprocesador ve:           Preprocesador ve:                              │
│                                                                             │
│  int sum(int a, int b) {     int sum(int a, int b) {                       │
│      return a + b;               return a + b;                             │
│  }                           }                                              │
│                                                                             │
│  (el #ifdef TEST se          #include <assert.h>                           │
│   descarta completo)         int main(void) {                              │
│                                  assert(sum(2, 3) == 5);                   │
│                                  return 0;                                  │
│                              }                                              │
│                                                                             │
│  → Error: no tiene main()   → Compilacion exitosa, ejecutable de test      │
│    (es una libreria, se                                                     │
│     linkea con otro main)                                                   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Directivas del preprocesador relevantes

### 2.1 Tabla completa

| Directiva | Significado | Ejemplo |
|-----------|-------------|---------|
| `#define MACRO` | Define una macro (sin valor) | `#define TEST` |
| `#define MACRO valor` | Define una macro con valor | `#define VERSION 3` |
| `#ifdef MACRO` | Verdadero si MACRO esta definida | `#ifdef TEST` |
| `#ifndef MACRO` | Verdadero si MACRO NO esta definida | `#ifndef NDEBUG` |
| `#if expr` | Verdadero si la expresion constante es != 0 | `#if DEBUG_LEVEL > 2` |
| `#elif expr` | Else if | `#elif defined(MOCK_FS)` |
| `#else` | Else | `#else` |
| `#endif` | Cierra el bloque condicional | `#endif` |
| `#undef MACRO` | Elimina la definicion de MACRO | `#undef NDEBUG` |
| `defined(MACRO)` | Operador — 1 si esta definida, 0 si no | `#if defined(TEST) && !defined(NDEBUG)` |

### 2.2 Formas de definir la macro

```bash
# 1. Desde la linea de comandos (lo mas comun)
gcc -DTEST program.c              # define TEST (sin valor, pero #ifdef TEST es true)
gcc -DTEST=1 program.c            # define TEST con valor 1
gcc -DTEST_LEVEL=3 program.c      # define TEST_LEVEL con valor 3

# 2. Desde el Makefile (lo mas practico)
CFLAGS_TEST = -DTEST
gcc $(CFLAGS_TEST) program.c

# 3. Desde el codigo fuente (raro para testing, pero valido)
#define TEST
```

### 2.3 ifdef vs if defined — diferencia sutil

```c
// #ifdef solo puede verificar UNA macro
#ifdef TEST
    // ...
#endif

// #if defined() puede combinar condiciones con && y ||
#if defined(TEST) && !defined(NDEBUG)
    // Solo en modo test Y con asserts habilitados
#endif

#if defined(TEST) || defined(DEBUG)
    // En modo test O en modo debug
#endif

// #ifdef NO puede hacer esto:
#ifdef TEST && !NDEBUG     // ERROR de sintaxis
```

---

## 3. Patron basico — Tests al final del archivo

### 3.1 El patron

El patron mas simple y comun es poner los tests al final del archivo `.c`, protegidos por `#ifdef TEST`:

```c
// calculator.c
#include <stdbool.h>
#include <limits.h>

// ══════════════════════════════════════════════════════════════
// Codigo de produccion
// ══════════════════════════════════════════════════════════════

// Funcion static — INTERNA, no visible desde otros archivos
static bool would_overflow_add(int a, int b) {
    if (b > 0 && a > INT_MAX - b) return true;
    if (b < 0 && a < INT_MIN - b) return true;
    return false;
}

// Funcion static — INTERNA
static bool would_overflow_mul(int a, int b) {
    if (a == 0 || b == 0) return false;
    if (a > 0 && b > 0 && a > INT_MAX / b) return true;
    if (a > 0 && b < 0 && b < INT_MIN / a) return true;
    if (a < 0 && b > 0 && a < INT_MIN / b) return true;
    if (a < 0 && b < 0 && a < INT_MAX / b) return true;
    return false;
}

// Funcion publica
int safe_add(int a, int b, bool *overflow) {
    if (would_overflow_add(a, b)) {
        if (overflow) *overflow = true;
        return 0;
    }
    if (overflow) *overflow = false;
    return a + b;
}

// Funcion publica
int safe_mul(int a, int b, bool *overflow) {
    if (would_overflow_mul(a, b)) {
        if (overflow) *overflow = true;
        return 0;
    }
    if (overflow) *overflow = false;
    return a * b;
}

// ══════════════════════════════════════════════════════════════
// Tests (solo se compilan con -DTEST)
// ══════════════════════════════════════════════════════════════

#ifdef TEST
#include <stdio.h>
#include <assert.h>

// ¡Podemos testear would_overflow_add aunque sea static!
// Porque estamos en el MISMO translation unit

static void test_would_overflow_add_positive(void) {
    assert(would_overflow_add(INT_MAX, 1) == true);
    assert(would_overflow_add(INT_MAX - 10, 11) == true);
    assert(would_overflow_add(INT_MAX - 10, 10) == false);
    assert(would_overflow_add(INT_MAX, 0) == false);
}

static void test_would_overflow_add_negative(void) {
    assert(would_overflow_add(INT_MIN, -1) == true);
    assert(would_overflow_add(INT_MIN + 10, -11) == true);
    assert(would_overflow_add(INT_MIN + 10, -10) == false);
    assert(would_overflow_add(INT_MIN, 0) == false);
}

static void test_would_overflow_add_mixed(void) {
    // Positivo + negativo nunca overflow
    assert(would_overflow_add(INT_MAX, INT_MIN) == false);
    assert(would_overflow_add(100, -50) == false);
}

static void test_would_overflow_mul_basic(void) {
    assert(would_overflow_mul(INT_MAX, 2) == true);
    assert(would_overflow_mul(INT_MAX, 1) == false);
    assert(would_overflow_mul(INT_MAX, 0) == false);
    assert(would_overflow_mul(0, INT_MAX) == false);
}

static void test_safe_add_no_overflow(void) {
    bool ov;
    int result = safe_add(10, 20, &ov);
    assert(result == 30);
    assert(ov == false);
}

static void test_safe_add_overflow(void) {
    bool ov;
    int result = safe_add(INT_MAX, 1, &ov);
    assert(result == 0);
    assert(ov == true);
}

static void test_safe_add_null_overflow_ptr(void) {
    // Pasar NULL como overflow pointer — no debe crashear
    int result = safe_add(10, 20, NULL);
    assert(result == 30);
}

static void test_safe_mul_no_overflow(void) {
    bool ov;
    int result = safe_mul(100, 200, &ov);
    assert(result == 20000);
    assert(ov == false);
}

static void test_safe_mul_overflow(void) {
    bool ov;
    int result = safe_mul(INT_MAX, 2, &ov);
    assert(result == 0);
    assert(ov == true);
}

int main(void) {
    printf("--- would_overflow_add ---\n");
    test_would_overflow_add_positive();
    printf("  [PASS] positive overflow\n");
    test_would_overflow_add_negative();
    printf("  [PASS] negative overflow\n");
    test_would_overflow_add_mixed();
    printf("  [PASS] mixed signs\n");

    printf("--- would_overflow_mul ---\n");
    test_would_overflow_mul_basic();
    printf("  [PASS] basic cases\n");

    printf("--- safe_add ---\n");
    test_safe_add_no_overflow();
    printf("  [PASS] no overflow\n");
    test_safe_add_overflow();
    printf("  [PASS] overflow detected\n");
    test_safe_add_null_overflow_ptr();
    printf("  [PASS] null overflow ptr\n");

    printf("--- safe_mul ---\n");
    test_safe_mul_no_overflow();
    printf("  [PASS] no overflow\n");
    test_safe_mul_overflow();
    printf("  [PASS] overflow detected\n");

    printf("\nAll tests passed!\n");
    return 0;
}

#endif /* TEST */
```

```bash
# Compilar como libreria (para linkear con el programa real)
$ gcc -Wall -Wextra -std=c11 -c calculator.c -o calculator.o
# calculator.o NO tiene main() — es un object file para linkear

# Compilar como test
$ gcc -Wall -Wextra -std=c11 -DTEST calculator.c -o test_calculator
$ ./test_calculator
--- would_overflow_add ---
  [PASS] positive overflow
  [PASS] negative overflow
  [PASS] mixed signs
--- would_overflow_mul ---
  [PASS] basic cases
--- safe_add ---
  [PASS] no overflow
  [PASS] overflow detected
  [PASS] null overflow ptr
--- safe_mul ---
  [PASS] no overflow
  [PASS] overflow detected

All tests passed!
```

### 3.2 Por que esto funciona — translation units

En C, cada archivo `.c` se compila de forma independiente como una **translation unit**. Las funciones `static` son visibles en toda su translation unit, pero no fuera de ella. Cuando pones los tests **dentro** del mismo archivo:

```
┌─────────────────────────────────────────────────────────┐
│  calculator.c (una translation unit)                    │
│                                                         │
│  ┌──────────────────────────────────────────────────┐   │
│  │  static would_overflow_add()  ← visible aqui    │   │
│  │  static would_overflow_mul()  ← visible aqui    │   │
│  │  safe_add()                   ← visible aqui    │   │
│  │  safe_mul()                   ← visible aqui    │   │
│  └──────────────────────────────────────────────────┘   │
│                                                         │
│  #ifdef TEST                                            │
│  ┌──────────────────────────────────────────────────┐   │
│  │  test_would_overflow_add()  ← puede llamar      │   │
│  │                                a todas las       │   │
│  │  test_safe_add()            ← funciones de       │   │
│  │                                arriba, incluso   │   │
│  │  main()                     ← las static         │   │
│  └──────────────────────────────────────────────────┘   │
│  #endif                                                 │
│                                                         │
└─────────────────────────────────────────────────────────┘

│  test_external.c (otra translation unit)                │
│                                                         │
│  ┌──────────────────────────────────────────────────┐   │
│  │  Solo puede ver:                                 │   │
│  │  - safe_add()  (declarada en calculator.h)       │   │
│  │  - safe_mul()  (declarada en calculator.h)       │   │
│  │                                                  │   │
│  │  NO puede ver:                                   │   │
│  │  - would_overflow_add()  ← es static             │   │
│  │  - would_overflow_mul()  ← es static             │   │
│  └──────────────────────────────────────────────────┘   │
```

### 3.3 Analogia con Rust

Rust tiene este patron built-in y formalizado:

```rust
// calculator.rs

// Funcion privada (equivalente a static en C)
fn would_overflow_add(a: i32, b: i32) -> bool {
    a.checked_add(b).is_none()
}

// Funcion publica
pub fn safe_add(a: i32, b: i32) -> Option<i32> {
    a.checked_add(b)
}

// Tests en el mismo archivo — exactamente el mismo patron que #ifdef TEST
#[cfg(test)]
mod tests {
    use super::*;  // Importa todo del modulo padre, incluyendo funciones privadas

    #[test]
    fn test_would_overflow_add() {
        assert!(would_overflow_add(i32::MAX, 1));
        assert!(!would_overflow_add(100, 200));
    }

    #[test]
    fn test_safe_add_no_overflow() {
        assert_eq!(safe_add(10, 20), Some(30));
    }
}
```

| Aspecto | C con `#ifdef TEST` | Rust con `#[cfg(test)]` |
|---------|--------------------|-----------------------|
| Macro de compilacion | `-DTEST` | `--test` (automatico con `cargo test`) |
| Acceso a funciones privadas | Si (mismo translation unit) | Si (`use super::*`) |
| Separacion del binario | Manual (dos comandos de compilacion) | Automatico (`cargo build` vs `cargo test`) |
| Autodiscovery de tests | No (necesitas `main()` manual) | Si (`#[test]`) |

---

## 4. Variantes del patron

### 4.1 Variante: macro TEST con nivel

A veces quieres diferentes niveles de test: rapidos (unit) y lentos (integration):

```c
// network.c

// ... codigo de produccion ...

#if defined(TEST) && TEST >= 2
// Tests de integracion (solo con -DTEST=2 o mayor)
// Estos tests hacen I/O real y pueden ser lentos

static void test_tcp_connect_to_localhost(void) {
    int fd = tcp_connect("127.0.0.1", 8080);
    assert(fd >= 0);
    close(fd);
}

#endif

#if defined(TEST) && TEST >= 1
// Tests unitarios (con -DTEST=1 o mayor)
// Estos tests son rapidos y no hacen I/O

static void test_parse_address_ipv4(void) {
    Addr addr;
    assert(parse_address("192.168.1.1:8080", &addr) == 0);
    assert(addr.port == 8080);
}

static void test_parse_address_invalid(void) {
    Addr addr;
    assert(parse_address("not-an-ip", &addr) != 0);
}

int main(void) {
    // Siempre ejecutar unit tests
    test_parse_address_ipv4();
    printf("[PASS] parse_address ipv4\n");
    test_parse_address_invalid();
    printf("[PASS] parse_address invalid\n");

    #if TEST >= 2
    // Solo ejecutar integration tests si TEST >= 2
    test_tcp_connect_to_localhost();
    printf("[PASS] tcp connect\n");
    #endif

    printf("All tests passed (level %d)\n", TEST);
    return 0;
}

#endif
```

```bash
# Solo unit tests (rapidos)
$ gcc -DTEST=1 network.c -o test_network && ./test_network
[PASS] parse_address ipv4
[PASS] parse_address invalid
All tests passed (level 1)

# Unit + integration tests (mas lentos)
$ gcc -DTEST=2 network.c -o test_network && ./test_network
[PASS] parse_address ipv4
[PASS] parse_address invalid
[PASS] tcp connect
All tests passed (level 2)
```

### 4.2 Variante: macros separadas por funcionalidad

```c
// database.c

// ... codigo de produccion ...

// Tests que necesitan mocking
#ifdef TEST_MOCK
#include "mock_stdlib.h"  // Override malloc/free para simular fallos
static void test_db_open_malloc_fails(void) {
    mock_malloc_fail_next();
    Database *db = db_open("test.db");
    assert(db == NULL);
    assert(get_last_error() == ERR_OUT_OF_MEMORY);
}
#endif

// Tests unitarios puros (no necesitan mocks)
#ifdef TEST
static void test_db_parse_query(void) {
    Query *q = parse_query("SELECT * FROM users");
    assert(q != NULL);
    assert(q->type == QUERY_SELECT);
    query_free(q);
}

int main(void) {
    test_db_parse_query();
    printf("[PASS] parse_query\n");

    #ifdef TEST_MOCK
    test_db_open_malloc_fails();
    printf("[PASS] malloc failure handled\n");
    #endif

    return 0;
}
#endif
```

```bash
# Solo tests sin mock
$ gcc -DTEST -o test_db database.c
$ ./test_db
[PASS] parse_query

# Tests con mock
$ gcc -DTEST -DTEST_MOCK -o test_db database.c mock_stdlib.c
$ ./test_db
[PASS] parse_query
[PASS] malloc failure handled
```

### 4.3 Variante: #include del archivo fuente en el archivo de test

Una tecnica poco conocida pero poderosa: en lugar de poner los tests dentro del `.c`, puedes **incluir** el `.c` dentro del archivo de test:

```c
// test_calculator.c
#include "calculator.c"   // ← incluye el ARCHIVO FUENTE completo, no el header

// Ahora todas las funciones static de calculator.c son visibles aqui
// porque estamos en la misma translation unit

#include <stdio.h>

void test_would_overflow_add(void) {
    // would_overflow_add es static en calculator.c
    // pero es accesible porque lo incluimos directamente
    assert(would_overflow_add(INT_MAX, 1) == true);
    assert(would_overflow_add(100, 200) == false);
}

int main(void) {
    test_would_overflow_add();
    printf("[PASS] would_overflow_add\n");
    return 0;
}
```

```bash
# IMPORTANTE: NO linkear calculator.c — ya esta incluido via #include
$ gcc -Wall -Wextra -std=c11 -o test_calculator test_calculator.c
# NO: gcc test_calculator.c calculator.c  ← duplicate symbols!
```

```
┌─────────────────────────────────────────────────────────────────────────┐
│  #include "calculator.c" — VENTAJAS Y PELIGROS                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ✓ Ventajas:                                                           │
│  • Acceso a funciones static sin modificar el archivo fuente            │
│  • Los tests estan separados en su propio archivo                       │
│  • El archivo fuente no tiene #ifdef TEST — queda limpio               │
│                                                                         │
│  ✗ Peligros:                                                           │
│  • Si incluyes calculator.c Y ademas lo linkeas → duplicate symbols    │
│  • El Makefile debe saber que test_calculator.c NO necesita             │
│    calculator.o — esto es facil de olvidar                              │
│  • Herramientas de analisis pueden confundirse (lint, IDE)              │
│  • Si calculator.c tiene side effects globales (variables static        │
│    con constructores), se ejecutan en el contexto del test              │
│                                                                         │
│  Recomendacion: usar con cuidado, documentar en el Makefile            │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Test targets en Makefile

### 5.1 Makefile basico con separacion produccion/test

```makefile
# Makefile
CC     = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g

# ── Produccion ────────────────────────────────────────────
SRC    = calculator.c parser.c network.c
OBJ    = $(SRC:.c=.o)

program: main.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Tests (compilacion condicional) ───────────────────────
# Cada .c se compila como ejecutable de test con -DTEST
TEST_SRC  = calculator.c parser.c network.c
TEST_BINS = $(TEST_SRC:.c=_test)

# Patron: foo_test se genera de foo.c con -DTEST
%_test: %.c
	$(CC) $(CFLAGS) -DTEST -o $@ $<

# Construir todos los test binaries
build-tests: $(TEST_BINS)

# Ejecutar todos los tests
test: $(TEST_BINS)
	@pass=0; fail=0; \
	for t in $(TEST_BINS); do \
		echo "--- $$t ---"; \
		./$$t; \
		if [ $$? -eq 0 ]; then \
			pass=$$((pass + 1)); \
		else \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "$$pass passed, $$fail failed"; \
	[ $$fail -eq 0 ]

clean:
	rm -f *.o program $(TEST_BINS)

.PHONY: test build-tests clean
```

```bash
$ make test
--- calculator_test ---
  [PASS] would_overflow_add positive
  [PASS] would_overflow_add negative
  [PASS] safe_add
All tests passed!
--- parser_test ---
  [PASS] parse_int
  [PASS] parse_string
All tests passed!
--- network_test ---
  [PASS] parse_address
All tests passed!

3 passed, 0 failed
```

### 5.2 Makefile avanzado — tests con sanitizers, cobertura, y tests externos

```makefile
# Makefile avanzado
CC       = gcc
CFLAGS   = -Wall -Wextra -Wpedantic -std=c11 -g
SRCDIR   = src
TESTDIR  = tests
BUILDDIR = build

# ── Fuentes ──────────────────────────────────────────────────
# Archivos que tienen tests inline (#ifdef TEST)
INLINE_TEST_SRC = $(SRCDIR)/calculator.c \
                  $(SRCDIR)/parser.c \
                  $(SRCDIR)/tokenizer.c

# Archivos de test externo (test_*.c con su propio main)
EXTERN_TEST_SRC = $(wildcard $(TESTDIR)/test_*.c)

# Archivos fuente de produccion (todos los .c en src/)
PROD_SRC = $(wildcard $(SRCDIR)/*.c)
PROD_OBJ = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(PROD_SRC))

# ── Binarios de test ────────────────────────────────────────
INLINE_TEST_BINS = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%_test, $(INLINE_TEST_SRC))
EXTERN_TEST_BINS = $(patsubst $(TESTDIR)/test_%.c, $(BUILDDIR)/test_%, $(EXTERN_TEST_SRC))
ALL_TEST_BINS    = $(INLINE_TEST_BINS) $(EXTERN_TEST_BINS)

# ── Produccion ───────────────────────────────────────────────

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/program: $(SRCDIR)/main.c $(PROD_OBJ) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^

# ── Tests inline (compilacion condicional) ───────────────────

# calculator.c → build/calculator_test
$(BUILDDIR)/%_test: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -DTEST -o $@ $<

# ── Tests externos ───────────────────────────────────────────

# tests/test_hash_table.c → build/test_hash_table
# Necesita linkear con los .o de produccion (excepto main.o)
PROD_OBJ_NO_MAIN = $(filter-out $(BUILDDIR)/main.o, $(PROD_OBJ))

$(BUILDDIR)/test_%: $(TESTDIR)/test_%.c $(PROD_OBJ_NO_MAIN) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $^

# ── Test runners ─────────────────────────────────────────────

.PHONY: test test-inline test-extern test-asan test-ubsan test-coverage clean

# Ejecutar TODOS los tests
test: $(ALL_TEST_BINS)
	@echo "Running $(words $(ALL_TEST_BINS)) test suites...\n"
	@pass=0; fail=0; \
	for t in $(ALL_TEST_BINS); do \
		name=$$(basename $$t); \
		./$$t > /dev/null 2>&1; \
		rc=$$?; \
		if [ $$rc -eq 0 ]; then \
			printf "  \033[32m[PASS]\033[0m %s\n" "$$name"; \
			pass=$$((pass + 1)); \
		else \
			printf "  \033[31m[FAIL]\033[0m %s (exit %d)\n" "$$name" "$$rc"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo ""; \
	echo "Results: $$pass passed, $$fail failed"; \
	[ $$fail -eq 0 ]

# Solo tests inline
test-inline: $(INLINE_TEST_BINS)
	@for t in $(INLINE_TEST_BINS); do ./$$t || exit 1; done

# Solo tests externos
test-extern: $(EXTERN_TEST_BINS)
	@for t in $(EXTERN_TEST_BINS); do ./$$t || exit 1; done

# Tests con AddressSanitizer
test-asan: CFLAGS += -fsanitize=address -fno-omit-frame-pointer
test-asan: clean test

# Tests con UndefinedBehaviorSanitizer
test-ubsan: CFLAGS += -fsanitize=undefined
test-ubsan: clean test

# Tests con cobertura
test-coverage: CFLAGS += -fprofile-arcs -ftest-coverage
test-coverage: clean test
	@echo "\nGenerating coverage report..."
	gcov $(INLINE_TEST_SRC) $(PROD_SRC) 2>/dev/null | grep -E "^(File|Lines)" 
	@echo "Detailed reports: *.gcov files"

clean:
	rm -rf $(BUILDDIR) *.gcov *.gcda *.gcno
```

```bash
$ make test
Running 6 test suites...

  [PASS] calculator_test
  [PASS] parser_test
  [PASS] tokenizer_test
  [PASS] test_hash_table
  [PASS] test_linked_list
  [FAIL] test_network (exit 1)

Results: 5 passed, 1 failed
make: *** [Makefile:65: test] Error 1

$ make test-asan    # Recompila todo con ASan y ejecuta
$ make test-ubsan   # Recompila todo con UBSan y ejecuta
$ make test-coverage # Recompila con coverage y genera reporte
```

### 5.3 Makefile con regla generica para tests inline

Si quieres que **cualquier** `.c` en `src/` sea testeable con `-DTEST` sin listarlo explicitamente:

```makefile
# Regla generica: si src/foo.c tiene #ifdef TEST,
# puedes hacer "make build/foo_test" automaticamente
$(BUILDDIR)/%_test: $(SRCDIR)/%.c | $(BUILDDIR)
	@if grep -q '#ifdef TEST\|#if.*defined(TEST)' $<; then \
		echo "Building $@"; \
		$(CC) $(CFLAGS) -DTEST -o $@ $<; \
	else \
		echo "SKIP: $< has no #ifdef TEST block"; \
	fi

# Descubrir automaticamente cuales .c tienen tests inline
INLINE_TEST_SRC = $(shell grep -rl '#ifdef TEST\|#if.*defined(TEST)' $(SRCDIR)/*.c 2>/dev/null)
INLINE_TEST_BINS = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%_test, $(INLINE_TEST_SRC))
```

---

## 6. Separar codigo de produccion y test — estrategias de proyecto

### 6.1 Estructura de directorios

```
proyecto/
├── src/                          ← Codigo de produccion
│   ├── main.c                    ← El main() del programa real
│   ├── calculator.c              ← Tiene #ifdef TEST al final
│   ├── calculator.h
│   ├── parser.c                  ← Tiene #ifdef TEST al final
│   ├── parser.h
│   ├── hash_table.c              ← NO tiene #ifdef TEST (testeado externamente)
│   └── hash_table.h
├── tests/                        ← Tests externos (archivos separados)
│   ├── test_hash_table.c         ← Test con su propio main()
│   ├── test_integration.c        ← Tests de integracion
│   └── testdata/                 ← Datos de prueba
│       ├── valid.json
│       └── invalid.json
├── include/                      ← Headers internos de test (opcional)
│   └── test_helpers.h            ← Macros ASSERT, RUN, etc.
├── Makefile
└── README.md
```

### 6.2 Cuando usar cada estrategia

```
┌───────────────────────────────────────────────────────────────────────────────┐
│              ¿DONDE PONER LOS TESTS?                                         │
├───────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│  Usar #ifdef TEST (inline) cuando:                                           │
│  ────────────────────────────────                                            │
│  • Necesitas testear funciones static (internas)                             │
│  • El modulo es autocontenido (no necesita linkear con otros .c)             │
│  • Los tests son rapidos y no hacen I/O                                      │
│  • Quieres que el test sea un "compañero" del codigo                        │
│  • Ejemplos: funciones de parsing, algoritmos, utilidades matematicas        │
│                                                                               │
│  Usar archivos externos (tests/test_*.c) cuando:                            │
│  ─────────────────────────────────────────────                               │
│  • Solo necesitas testear la interfaz publica                                │
│  • El modulo depende de otros modulos (necesita linkear con otros .o)        │
│  • Los tests son de integracion (multiples modulos juntos)                   │
│  • Los tests hacen I/O (archivos, red, base de datos)                        │
│  • Quieres ejecutar tests en paralelo (cada test es un proceso)             │
│  • Ejemplos: APIs, flujos end-to-end, interaccion entre modulos             │
│                                                                               │
│  Usar ambos:                                                                 │
│  ───────────                                                                 │
│  • Tests inline para funciones static (algoritmo interno)                    │
│  • Tests externos para la interfaz publica (comportamiento observable)       │
│  • Es la combinacion mas comun en proyectos C profesionales                 │
│                                                                               │
└───────────────────────────────────────────────────────────────────────────────┘
```

### 6.3 El problema del doble main()

Cuando un `.c` tiene `#ifdef TEST` con su propio `main()`, **no puede compilarse como test y a la vez linkearse con otro `main()`** en la misma invocacion:

```bash
# Esto falla — dos definiciones de main()
$ gcc -DTEST main.c calculator.c -o program
# error: multiple definition of 'main'
```

Soluciones:

**Solucion A**: compilar en modo test sin main.c:

```bash
# Solo compilar calculator.c como test (tiene su propio main)
$ gcc -DTEST calculator.c -o calculator_test
```

**Solucion B**: proteger el main() del programa:

```c
// main.c
#ifndef TEST
int main(int argc, char *argv[]) {
    // ... programa real ...
}
#endif
```

**Solucion C**: usar un main() centralizado para tests (mas complejo pero mas flexible):

```c
// test_runner.c — el unico main() para tests
#define TEST
#include "calculator.c"  // Incluye las funciones de test de calculator
#include "parser.c"      // Incluye las funciones de test de parser

// Pero ahora necesitas que los tests en cada .c NO tengan su propio main()
// Solucion: usar una macro TEST_MAIN

// calculator.c:
// #ifdef TEST
// void test_calculator_run(void) { ... }
// #ifdef TEST_MAIN
// int main(void) { test_calculator_run(); return 0; }
// #endif
// #endif
```

La solucion A es la mas simple y la recomendada. No la compliques.

---

## 7. Macros utiles para testing condicional

### 7.1 #undef NDEBUG — asegurar que assert funciona en tests

Recuerda de T01 que `assert()` se desactiva con `NDEBUG`. Cuando compilas en modo test, **siempre** quieres asserts activos:

```c
#ifdef TEST
// FORZAR que assert() funcione, incluso si alguien paso -DNDEBUG
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

// Ahora assert() SIEMPRE funciona en tests
static void test_something(void) {
    assert(1 + 1 == 2);  // Garantizado activo
}
#endif
```

```bash
# Escenario peligroso: compilar con NDEBUG Y TEST
$ gcc -DTEST -DNDEBUG calculator.c -o test_calculator
# Sin el #undef NDEBUG, los assert dentro de #ifdef TEST serian no-ops!
# El test pasaria sin verificar nada. False positive.
```

### 7.2 Macros para testing interno de estado

```c
// hash_table.c

typedef struct {
    Entry *buckets;
    size_t count;
    size_t capacity;
    float  load_factor_threshold;
} HashTable;

// ... codigo de produccion ...

#ifdef TEST

// Funciones de "inspeccion" — solo existen en modo test
// Permiten verificar el estado interno sin exponerlo en el .h publico

static size_t test_get_capacity(const HashTable *ht) {
    return ht->capacity;
}

static float test_get_load_factor(const HashTable *ht) {
    return (float)ht->count / (float)ht->capacity;
}

static size_t test_count_empty_buckets(const HashTable *ht) {
    size_t empty = 0;
    for (size_t i = 0; i < ht->capacity; i++) {
        if (ht->buckets[i].state == EMPTY) empty++;
    }
    return empty;
}

// Ahora los tests pueden verificar invariantes internas
static void test_ht_load_factor_stays_below_threshold(void) {
    HashTable *ht = ht_new(4);

    for (int i = 0; i < 100; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        ht_insert(ht, key, "value");

        // Invariante: el load factor nunca excede el threshold
        // (la tabla debe hacer resize antes de que eso pase)
        float lf = test_get_load_factor(ht);
        assert(lf <= ht->load_factor_threshold);
    }

    ht_free(ht);
}

static void test_ht_capacity_is_power_of_two(void) {
    HashTable *ht = ht_new(4);

    for (int i = 0; i < 50; i++) {
        char key[16];
        snprintf(key, sizeof(key), "key_%d", i);
        ht_insert(ht, key, "value");

        // Invariante: la capacidad siempre es potencia de 2
        size_t cap = test_get_capacity(ht);
        assert((cap & (cap - 1)) == 0);  // Truco: N & (N-1) == 0 si N es potencia de 2
    }

    ht_free(ht);
}

#endif
```

### 7.3 Compilacion condicional para inyectar dependencias de test

Este patron es mas avanzado — usa `#ifdef TEST` para reemplazar una dependencia con una version de test:

```c
// file_processor.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// En produccion, usamos las funciones reales de I/O
// En test, podemos reemplazarlas con versiones fake

#ifdef TEST
// ── Fake filesystem para tests ──────────────────────────────
typedef struct {
    const char *path;
    const char *content;
} FakeFile;

static FakeFile fake_files[16];
static int fake_file_count = 0;

static void fake_fs_reset(void) {
    fake_file_count = 0;
}

static void fake_fs_add(const char *path, const char *content) {
    fake_files[fake_file_count].path = path;
    fake_files[fake_file_count].content = content;
    fake_file_count++;
}

// Reemplazar read_file con una version que usa el fake filesystem
static char *read_file(const char *path) {
    for (int i = 0; i < fake_file_count; i++) {
        if (strcmp(fake_files[i].path, path) == 0) {
            return strdup(fake_files[i].content);
        }
    }
    return NULL;  // Archivo no encontrado
}

#else
// ── Implementacion real ─────────────────────────────────────
static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if (buf == NULL) { fclose(f); return NULL; }

    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}
#endif

// ── Codigo que usa read_file (identico en test y produccion) ──

typedef struct {
    char *key;
    char *value;
} ConfigEntry;

int parse_config(const char *path, ConfigEntry *entries, int max_entries) {
    char *content = read_file(path);  // Usa la version real o fake
    if (content == NULL) return -1;

    int count = 0;
    char *line = strtok(content, "\n");
    while (line && count < max_entries) {
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            entries[count].key = strdup(line);
            entries[count].value = strdup(eq + 1);
            count++;
        }
        line = strtok(NULL, "\n");
    }

    free(content);
    return count;
}

#ifdef TEST
#include <assert.h>
#include <stdio.h>

static void test_parse_config_basic(void) {
    fake_fs_reset();
    fake_fs_add("/etc/app.conf", "host=localhost\nport=8080\ndebug=true");

    ConfigEntry entries[10];
    int count = parse_config("/etc/app.conf", entries, 10);

    assert(count == 3);
    assert(strcmp(entries[0].key, "host") == 0);
    assert(strcmp(entries[0].value, "localhost") == 0);
    assert(strcmp(entries[1].key, "port") == 0);
    assert(strcmp(entries[1].value, "8080") == 0);
    assert(strcmp(entries[2].key, "debug") == 0);
    assert(strcmp(entries[2].value, "true") == 0);

    for (int i = 0; i < count; i++) {
        free(entries[i].key);
        free(entries[i].value);
    }
}

static void test_parse_config_empty_file(void) {
    fake_fs_reset();
    fake_fs_add("/etc/empty.conf", "");

    ConfigEntry entries[10];
    int count = parse_config("/etc/empty.conf", entries, 10);

    assert(count == 0);
}

static void test_parse_config_file_not_found(void) {
    fake_fs_reset();
    // No agregar ningun archivo al fake filesystem

    ConfigEntry entries[10];
    int count = parse_config("/no/existe", entries, 10);

    assert(count == -1);
}

static void test_parse_config_malformed_lines(void) {
    fake_fs_reset();
    fake_fs_add("/etc/bad.conf", "no_equals_sign\nvalid=entry\n=no_key");

    ConfigEntry entries[10];
    int count = parse_config("/etc/bad.conf", entries, 10);

    // Solo la linea "valid=entry" es valida
    // "=no_key" tambien tiene un '=', depende de la implementacion
    // si lo acepta o no (key vacia). Segun nuestra impl, count = 2
    assert(count == 2);
    assert(strcmp(entries[0].key, "valid") == 0);

    for (int i = 0; i < count; i++) {
        free(entries[i].key);
        free(entries[i].value);
    }
}

int main(void) {
    test_parse_config_basic();
    printf("[PASS] parse_config basic\n");

    test_parse_config_empty_file();
    printf("[PASS] parse_config empty\n");

    test_parse_config_file_not_found();
    printf("[PASS] parse_config not found\n");

    test_parse_config_malformed_lines();
    printf("[PASS] parse_config malformed\n");

    printf("\nAll tests passed!\n");
    return 0;
}

#endif
```

```bash
# Test — usa fake filesystem, no toca el disco
$ gcc -DTEST -Wall -Wextra -std=c11 -o test_config file_processor.c
$ ./test_config
[PASS] parse_config basic
[PASS] parse_config empty
[PASS] parse_config not found
[PASS] parse_config malformed

All tests passed!

# Produccion — usa filesystem real
$ gcc -Wall -Wextra -std=c11 -c file_processor.c -o file_processor.o
```

La clave es que `parse_config()` llama a `read_file()` sin saber si es la version real o la fake. El preprocesador decide cual version existe segun `-DTEST`.

---

## 8. Integracion con CMake

### 8.1 CMakeLists.txt con soporte para tests inline

```cmake
cmake_minimum_required(VERSION 3.14)
project(mi_proyecto C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Produccion ──────────────────────────────────────────
add_library(mylib
    src/calculator.c
    src/parser.c
    src/hash_table.c
)
target_include_directories(mylib PUBLIC src)

add_executable(program src/main.c)
target_link_libraries(program mylib)

# ── Tests ───────────────────────────────────────────────
enable_testing()

# Funcion: crear un test inline desde un .c con #ifdef TEST
function(add_inline_test source_file)
    # Extraer nombre base: src/calculator.c → calculator_test
    get_filename_component(base_name ${source_file} NAME_WE)
    set(test_name "${base_name}_test")

    add_executable(${test_name} ${source_file})
    target_compile_definitions(${test_name} PRIVATE TEST)
    target_include_directories(${test_name} PRIVATE src)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# Registrar tests inline
add_inline_test(src/calculator.c)
add_inline_test(src/parser.c)

# Funcion: crear un test externo que linkea con la libreria
function(add_external_test test_source)
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} mylib)
    target_include_directories(${test_name} PRIVATE src)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# Registrar tests externos
add_external_test(tests/test_hash_table.c)
add_external_test(tests/test_linked_list.c)

# ── Tests con sanitizers (opcional) ─────────────────────
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()
```

```bash
# Build y test normal
$ mkdir build && cd build
$ cmake ..
$ cmake --build .
$ ctest --output-on-failure

# Build y test con sanitizers
$ cmake -DENABLE_ASAN=ON -DENABLE_UBSAN=ON ..
$ cmake --build .
$ ctest --output-on-failure

# Ejecutar un test especifico
$ ctest -R calculator_test
```

### 8.2 Nota sobre target_compile_definitions

`target_compile_definitions(target PRIVATE TEST)` es el equivalente CMake de `-DTEST`. El `PRIVATE` significa que la definicion solo aplica a ese target, no se propaga a otros targets que dependan de el.

| Visibilidad | Significado |
|-------------|-------------|
| `PRIVATE` | Solo este target ve la definicion |
| `PUBLIC` | Este target Y todos los que dependan de el |
| `INTERFACE` | Solo los que dependan de este target (no el target mismo) |

Para tests, siempre usa `PRIVATE` — no quieres que `-DTEST` se propague.

---

## 9. Patrones avanzados

### 9.1 Test helpers condicionales — funciones que solo existen en test mode

```c
// hash_table.c

// ... struct HashTable, funciones de produccion ...

// ── API de testing (solo visible en modo test) ──────────────

#ifdef TEST

// Exponer la capacidad interna (normalmente es un detalle de implementacion)
size_t ht_test_get_capacity(const HashTable *ht) {
    return ht->capacity;
}

// Exponer la distribucion de buckets (para verificar calidad del hash)
void ht_test_bucket_stats(const HashTable *ht, size_t *empty, size_t *single, size_t *collisions) {
    *empty = *single = *collisions = 0;
    for (size_t i = 0; i < ht->capacity; i++) {
        size_t chain_len = 0;
        for (Entry *e = ht->buckets[i]; e != NULL; e = e->next) {
            chain_len++;
        }
        if (chain_len == 0) (*empty)++;
        else if (chain_len == 1) (*single)++;
        else (*collisions)++;
    }
}

// Forzar un resize (normalmente es automatico)
void ht_test_force_resize(HashTable *ht, size_t new_capacity) {
    ht_resize_internal(ht, new_capacity);
}

#endif
```

Tambien puedes declarar estas funciones en un **header separado** para tests:

```c
// hash_table_test_api.h — solo para usar en tests
#ifndef HASH_TABLE_TEST_API_H
#define HASH_TABLE_TEST_API_H

#include "hash_table.h"

// Estas funciones solo existen cuando hash_table.c se compila con -DTEST
size_t ht_test_get_capacity(const HashTable *ht);
void   ht_test_bucket_stats(const HashTable *ht, size_t *empty, size_t *single, size_t *collisions);
void   ht_test_force_resize(HashTable *ht, size_t new_capacity);

#endif
```

### 9.2 Compilacion condicional para elegir implementacion de I/O

```c
// logger.c

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

// ── Abstraer el output ──────────────────────────────────────

#ifdef TEST

// En tests: capturar en buffer en vez de escribir a stderr
static char log_buffer[4096];
static size_t log_buffer_pos = 0;

static void log_reset(void) {
    log_buffer[0] = '\0';
    log_buffer_pos = 0;
}

static const char *log_get_output(void) {
    return log_buffer;
}

static void log_write(const char *msg) {
    size_t len = strlen(msg);
    if (log_buffer_pos + len < sizeof(log_buffer)) {
        memcpy(log_buffer + log_buffer_pos, msg, len);
        log_buffer_pos += len;
        log_buffer[log_buffer_pos] = '\0';
    }
}

#else

// En produccion: escribir a stderr
static void log_write(const char *msg) {
    fputs(msg, stderr);
}

#endif

// ── API publica (igual en test y produccion) ────────────────

void log_info(const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);

    // Timestamp
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int offset = snprintf(buf, sizeof(buf), "[%04d-%02d-%02d %02d:%02d:%02d] [INFO] ",
                          tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                          tm->tm_hour, tm->tm_min, tm->tm_sec);

    vsnprintf(buf + offset, sizeof(buf) - offset, fmt, args);
    va_end(args);

    strncat(buf, "\n", sizeof(buf) - strlen(buf) - 1);
    log_write(buf);  // Usa la implementacion correcta segun #ifdef TEST
}

void log_error(const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int offset = snprintf(buf, sizeof(buf), "[%04d-%02d-%02d %02d:%02d:%02d] [ERROR] ",
                          tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                          tm->tm_hour, tm->tm_min, tm->tm_sec);

    vsnprintf(buf + offset, sizeof(buf) - offset, fmt, args);
    va_end(args);

    strncat(buf, "\n", sizeof(buf) - strlen(buf) - 1);
    log_write(buf);
}

// ── Tests ───────────────────────────────────────────────────

#ifdef TEST
#include <assert.h>

static void test_log_info_contains_info_tag(void) {
    log_reset();

    log_info("hello %s", "world");

    const char *output = log_get_output();
    assert(strstr(output, "[INFO]") != NULL);
    assert(strstr(output, "hello world") != NULL);
}

static void test_log_error_contains_error_tag(void) {
    log_reset();

    log_error("disk full");

    const char *output = log_get_output();
    assert(strstr(output, "[ERROR]") != NULL);
    assert(strstr(output, "disk full") != NULL);
}

static void test_log_multiple_messages(void) {
    log_reset();

    log_info("first");
    log_info("second");

    const char *output = log_get_output();
    // Ambos mensajes deben estar en el buffer
    assert(strstr(output, "first") != NULL);
    assert(strstr(output, "second") != NULL);
    // "second" debe aparecer despues de "first"
    assert(strstr(output, "first") < strstr(output, "second"));
}

static void test_log_info_has_timestamp(void) {
    log_reset();

    log_info("test");

    const char *output = log_get_output();
    // El timestamp tiene formato [YYYY-MM-DD HH:MM:SS]
    // Verificar que empieza con [20 (anio 20xx)
    assert(output[0] == '[');
    assert(output[1] == '2');
    assert(output[2] == '0');
}

int main(void) {
    test_log_info_contains_info_tag();
    printf("[PASS] log_info contains [INFO]\n");

    test_log_error_contains_error_tag();
    printf("[PASS] log_error contains [ERROR]\n");

    test_log_multiple_messages();
    printf("[PASS] multiple messages captured\n");

    test_log_info_has_timestamp();
    printf("[PASS] timestamp present\n");

    printf("\nAll logger tests passed!\n");
    return 0;
}
#endif
```

### 9.3 Compilacion condicional para inyectar funciones de tiempo

El tiempo (`time()`, `clock_gettime()`) es una de las dependencias mas dificiles de testear. La compilacion condicional lo resuelve elegantemente:

```c
// cache.c

#include <time.h>
#include <stdbool.h>

// ── Abstraer el tiempo ──────────────────────────────────────

#ifdef TEST
// En tests: tiempo controlable
static time_t fake_time = 0;
static void test_set_time(time_t t) { fake_time = t; }
static time_t get_current_time(void) { return fake_time; }
#else
// En produccion: tiempo real
static time_t get_current_time(void) { return time(NULL); }
#endif

// ── Codigo de produccion ────────────────────────────────────

typedef struct {
    char *key;
    char *value;
    time_t expires_at;
} CacheEntry;

typedef struct {
    CacheEntry *entries;
    size_t count;
    size_t capacity;
} Cache;

// ... cache_new, cache_free, etc. ...

bool cache_set(Cache *c, const char *key, const char *value, int ttl_seconds) {
    // ... encontrar o crear entry ...
    entry->expires_at = get_current_time() + ttl_seconds;
    // ...
    return true;
}

const char *cache_get(Cache *c, const char *key) {
    CacheEntry *entry = find_entry(c, key);
    if (entry == NULL) return NULL;

    // Verificar expiracion
    if (get_current_time() > entry->expires_at) {
        remove_entry(c, key);
        return NULL;  // Expirado
    }

    return entry->value;
}

// ── Tests ───────────────────────────────────────────────────

#ifdef TEST
#include <assert.h>
#include <stdio.h>

static void test_cache_entry_expires_after_ttl(void) {
    Cache *c = cache_new(16);

    test_set_time(1000);                     // Tiempo: 1000
    cache_set(c, "token", "abc123", 60);     // Expira en 1060

    test_set_time(1030);                     // Tiempo: 1030 (30s despues)
    assert(cache_get(c, "token") != NULL);   // Aun valido (30 < 60)

    test_set_time(1061);                     // Tiempo: 1061 (61s despues)
    assert(cache_get(c, "token") == NULL);   // Expirado (61 > 60)

    cache_free(c);
}

static void test_cache_entry_valid_exactly_at_ttl(void) {
    Cache *c = cache_new(16);

    test_set_time(1000);
    cache_set(c, "key", "val", 60);

    test_set_time(1060);                     // Exactamente al limite
    // ¿Es valido o expirado? Depende de si usas > o >=
    // Con > (como en nuestra impl): aun valido en el segundo exacto
    assert(cache_get(c, "key") != NULL);

    test_set_time(1061);
    assert(cache_get(c, "key") == NULL);     // Un segundo despues: expirado

    cache_free(c);
}

int main(void) {
    test_cache_entry_expires_after_ttl();
    printf("[PASS] entry expires after TTL\n");

    test_cache_entry_valid_exactly_at_ttl();
    printf("[PASS] entry valid at exact TTL boundary\n");

    printf("\nAll cache tests passed!\n");
    return 0;
}
#endif
```

Sin `#ifdef TEST` y el tiempo fake, tendrias que usar `sleep(61)` en el test — haciendolo inutilmente lento. Con la compilacion condicional, el test es instantaneo.

---

## 10. Comparacion con Rust y Go

### 10.1 Compilacion condicional para tests en tres lenguajes

**C:**

```c
// calculator.c

static int internal_helper(int x) { return x * 2; }

int public_function(int x) { return internal_helper(x) + 1; }

#ifdef TEST
#include <assert.h>
int main(void) {
    assert(internal_helper(5) == 10);   // Puede testear la funcion static
    assert(public_function(5) == 11);
    return 0;
}
#endif
```

```bash
gcc -DTEST calculator.c -o test      # Test
gcc -c calculator.c -o calculator.o  # Produccion
```

**Rust:**

```rust
// calculator.rs

fn internal_helper(x: i32) -> i32 { x * 2 }

pub fn public_function(x: i32) -> i32 { internal_helper(x) + 1 }

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_internal_helper() {
        assert_eq!(internal_helper(5), 10);  // Puede testear fn privada
    }
    
    #[test]
    fn test_public_function() {
        assert_eq!(public_function(5), 11);
    }
}
```

```bash
cargo test       # Test (automatico)
cargo build      # Produccion (automatico)
```

**Go:**

```go
// calculator.go
package calculator

func internalHelper(x int) int { return x * 2 }

func PublicFunction(x int) int { return internalHelper(x) + 1 }
```

```go
// calculator_test.go (archivo SEPARADO pero MISMO paquete)
package calculator

import "testing"

func TestInternalHelper(t *testing.T) {
    // Puede testear la funcion no exportada (minuscula)
    // porque esta en el mismo paquete
    if got := internalHelper(5); got != 10 {
        t.Errorf("got %d, want 10", got)
    }
}

func TestPublicFunction(t *testing.T) {
    if got := PublicFunction(5); got != 11 {
        t.Errorf("got %d, want 11", got)
    }
}
```

```bash
go test ./...     # Test (automatico)
go build          # Produccion (automatico)
```

### 10.2 Tabla comparativa

| Aspecto | C (`#ifdef TEST`) | Rust (`#[cfg(test)]`) | Go (`_test.go`) |
|---------|-------------------|-----------------------|-----------------|
| **Ubicacion** | Mismo archivo | Mismo archivo | Archivo separado (mismo paquete) |
| **Acceso a privados** | Si (mismo translation unit) | Si (`use super::*`) | Si (mismo paquete) |
| **Compilacion separada** | Manual (`-DTEST` vs sin) | Automatica (`cargo test` vs `cargo build`) | Automatica (`go test` vs `go build`) |
| **Entra en el binario de produccion** | No (preprocesador lo elimina) | No (`cfg(test)` lo excluye) | No (`_test.go` excluido de build) |
| **Puede tener su propio main** | Si | No (el runner lo maneja) | No (el runner lo maneja) |
| **Macro para activar** | `-DTEST` | `--test` flag (automatico) | No aplica (deteccion por nombre de archivo) |
| **Necesita framework** | No (manual) | No (built-in) | No (built-in) |

### 10.3 Lo que C te hace hacer manualmente

En Rust y Go, la separacion produccion/test es **automatica**:
- `cargo build` nunca compila `#[cfg(test)]`
- `go build` nunca compila `*_test.go`

En C, **tu** eres responsable de que el codigo de test no entre en el binario de produccion. Si olvidas `-DTEST` o si tu Makefile tiene un error, puedes terminar con:

1. **main duplicado**: tu programa tiene el main() de produccion Y el main() de test
2. **Codigo de test en produccion**: funciones de test, asserts, fake implementations en el binario final
3. **Dependencias de test en produccion**: `#include <assert.h>` o libs de mock linkeadas en el release

El Makefile es tu red de seguridad. Si esta bien hecho, estos problemas no ocurren.

---

## 11. Errores comunes y trampas

### 11.1 Olvidar cerrar el #endif

```c
#ifdef TEST
void test_something(void) {
    assert(1 == 1);
}

int main(void) {
    test_something();
    return 0;
}
// ← ¡¡FALTA #endif!!
// Todo el codigo despues de este punto se incluye solo en modo TEST
// Si hay otro .c que lo incluye, puede causar errores misteriosos
```

Prevencion: siempre agregar un comentario al `#endif`:

```c
#ifdef TEST
// ... tests ...
#endif /* TEST */
```

### 11.2 Compilar el .c como test Y linkearlo como objeto

```bash
# INCORRECTO — produce duplicate symbol errors
$ gcc -DTEST calculator.c main.c -o program
# error: multiple definition of 'main'

# CORRECTO — compilar solo el archivo que tiene el test main
$ gcc -DTEST calculator.c -o test_calculator
```

### 11.3 Efectos secundarios de includes condicionales

```c
// PELIGROSO
#ifdef TEST
#include <assert.h>  // Define la macro assert()
#endif

void production_function(void) {
    // Si alguien usa assert() aqui SIN #ifdef TEST,
    // compila en modo test pero falla en produccion
    // (assert no esta definido)
    assert(ptr != NULL);  // Error: implicit declaration of function 'assert'
}
```

Solucion: los `#include` para testing deben estar **dentro** del bloque `#ifdef TEST`, y el codigo de produccion debe usar sus propios includes:

```c
#include <stdlib.h>  // Produccion: siempre incluido

// Codigo de produccion — NO usa assert()
void production_function(void) {
    if (ptr == NULL) {
        // Manejar error explicitamente
        return;
    }
}

#ifdef TEST
#include <assert.h>  // Solo para tests
#include <stdio.h>

static void test_production_function(void) {
    // Aqui si podemos usar assert
    assert(some_condition);
}
#endif
```

### 11.4 Variables static en modo test que afectan re-ejecucion

```c
// TRAMPA: estado que persiste entre tests (en el mismo proceso)
static int call_count = 0;

void tracked_function(void) {
    call_count++;
    // ...
}

#ifdef TEST
static void test_tracked_function_increments_counter(void) {
    tracked_function();
    assert(call_count == 1);  // FALLA si otro test llamo a tracked_function antes
}
#endif
```

Solucion: resetear el estado al inicio de cada test, o exponer una funcion de reset en modo test:

```c
#ifdef TEST
static void reset_tracking(void) {
    call_count = 0;
}

static void test_tracked_function_increments_counter(void) {
    reset_tracking();  // Garantizar estado limpio
    tracked_function();
    assert(call_count == 1);
}
#endif
```

### 11.5 Macros que se expanden diferente en test y produccion

```c
// PELIGRO SUTIL: el comportamiento del codigo cambia segun TEST

#ifdef TEST
#define LOG(msg) log_to_buffer(msg)    // Captura en buffer
#else
#define LOG(msg) fprintf(stderr, msg)  // Escribe a stderr
#endif

void process(Data *d) {
    LOG("processing...\n");  // ¿Que hace exactamente? Depende de TEST.
    // Si el bug solo se manifiesta cuando LOG va a stderr
    // (ej: buffer de stderr afecta timing), no lo encontraras en tests.
}
```

Esto no es inherentemente malo — es la esencia de la compilacion condicional. Pero **debes ser consciente** de que tu test ejecuta codigo ligeramente diferente al de produccion. Minimiza las diferencias: el fake debe comportarse **lo mas parecido posible** al real.

---

## 12. Ejemplo completo — Proyecto con ambas estrategias

Un proyecto real que usa tests inline para funciones internas y tests externos para la interfaz publica:

### 12.1 Estructura

```
proyecto/
├── src/
│   ├── main.c
│   ├── tokenizer.c        ← tiene #ifdef TEST (funciones static de parsing)
│   ├── tokenizer.h
│   ├── evaluator.c        ← tiene #ifdef TEST (funciones static de evaluacion)
│   ├── evaluator.h
│   ├── calculator.c       ← NO tiene tests inline (solo API publica)
│   └── calculator.h       ← API publica: calculate(const char *expr) → double
├── tests/
│   ├── test_calculator.c  ← tests de integracion (tokenizer + evaluator juntos)
│   └── test_helpers.h     ← macros compartidas
├── Makefile
└── README.md
```

### 12.2 tokenizer.c con tests inline

```c
// tokenizer.c
#include "tokenizer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// ── Funciones static (internas) ─────────────────────────────

static bool is_operator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/';
}

static int operator_precedence(char op) {
    switch (op) {
        case '+': case '-': return 1;
        case '*': case '/': return 2;
        default: return 0;
    }
}

static double parse_number(const char *str, const char **end) {
    double result = strtod(str, (char **)end);
    return result;
}

// ── Funciones publicas ──────────────────────────────────────

Token *tokenize(const char *expr, size_t *count) {
    // ... implementacion que usa is_operator, parse_number, etc. ...
    return tokens;
}

void tokens_free(Token *tokens, size_t count) {
    free(tokens);
}

// ── Tests inline ────────────────────────────────────────────

#ifdef TEST
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

// Tests de funciones STATIC — imposible desde otro archivo

static void test_is_operator(void) {
    assert(is_operator('+') == true);
    assert(is_operator('-') == true);
    assert(is_operator('*') == true);
    assert(is_operator('/') == true);
    assert(is_operator('x') == false);
    assert(is_operator('0') == false);
    assert(is_operator(' ') == false);
}

static void test_operator_precedence(void) {
    assert(operator_precedence('+') == 1);
    assert(operator_precedence('-') == 1);
    assert(operator_precedence('*') == 2);
    assert(operator_precedence('/') == 2);
    assert(operator_precedence('x') == 0);  // Desconocido
    // Verificar que multiplicacion tiene mayor precedencia que suma
    assert(operator_precedence('*') > operator_precedence('+'));
}

static void test_parse_number_integer(void) {
    const char *end;
    double val = parse_number("42rest", &end);
    assert(val == 42.0);
    assert(*end == 'r');  // Se detuvo en 'r'
}

static void test_parse_number_float(void) {
    const char *end;
    double val = parse_number("3.14!", &end);
    assert(val > 3.13 && val < 3.15);
    assert(*end == '!');
}

static void test_parse_number_negative(void) {
    const char *end;
    double val = parse_number("-7.5x", &end);
    assert(val == -7.5);
    assert(*end == 'x');
}

// Tests de funciones PUBLICAS tambien (por conveniencia)

static void test_tokenize_simple_addition(void) {
    size_t count;
    Token *tokens = tokenize("2 + 3", &count);
    assert(count == 3);
    assert(tokens[0].type == TOKEN_NUMBER);
    assert(tokens[0].value == 2.0);
    assert(tokens[1].type == TOKEN_OPERATOR);
    assert(tokens[1].op == '+');
    assert(tokens[2].type == TOKEN_NUMBER);
    assert(tokens[2].value == 3.0);
    tokens_free(tokens, count);
}

int main(void) {
    printf("--- Tokenizer internal tests ---\n");
    test_is_operator();          printf("[PASS] is_operator\n");
    test_operator_precedence();  printf("[PASS] operator_precedence\n");
    test_parse_number_integer(); printf("[PASS] parse_number integer\n");
    test_parse_number_float();   printf("[PASS] parse_number float\n");
    test_parse_number_negative(); printf("[PASS] parse_number negative\n");

    printf("\n--- Tokenizer public tests ---\n");
    test_tokenize_simple_addition(); printf("[PASS] tokenize simple addition\n");

    printf("\nAll tokenizer tests passed!\n");
    return 0;
}
#endif /* TEST */
```

### 12.3 test_calculator.c — test externo de integracion

```c
// tests/test_calculator.c — testea la API publica de calculator
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "test_helpers.h"
#include "calculator.h"

// Solo puede ver la API publica: calculate(const char *expr)
// NO puede ver is_operator, parse_number, etc.

void test_calc_simple_addition(void) {
    double result;
    ASSERT_EQ(calculate("2 + 3", &result), 0);
    ASSERT_DOUBLE_EQ(result, 5.0);
}

void test_calc_operator_precedence(void) {
    double result;
    ASSERT_EQ(calculate("2 + 3 * 4", &result), 0);
    ASSERT_DOUBLE_EQ(result, 14.0);  // 3*4=12, 2+12=14 (no 5*4=20)
}

void test_calc_parentheses(void) {
    double result;
    ASSERT_EQ(calculate("(2 + 3) * 4", &result), 0);
    ASSERT_DOUBLE_EQ(result, 20.0);
}

void test_calc_invalid_expression(void) {
    double result;
    ASSERT_NE(calculate("2 + + 3", &result), 0);  // Debe retornar error
}

void test_calc_division_by_zero(void) {
    double result;
    int rc = calculate("10 / 0", &result);
    // Puede retornar error o Inf — depende del contrato
    // Nuestro contrato: retorna error
    ASSERT_NE(rc, 0);
}

void test_calc_empty_expression(void) {
    double result;
    ASSERT_NE(calculate("", &result), 0);
}

int main(void) {
    printf("=== Calculator Integration Tests ===\n\n");

    RUN(test_calc_simple_addition);
    RUN(test_calc_operator_precedence);
    RUN(test_calc_parentheses);
    RUN(test_calc_invalid_expression);
    RUN(test_calc_division_by_zero);
    RUN(test_calc_empty_expression);

    SUMMARY();
    return test_failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
```

### 12.4 Makefile

```makefile
CC     = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g
SRCDIR = src
TESTDIR = tests
BUILD  = build

# Produccion
SRC = $(SRCDIR)/tokenizer.c $(SRCDIR)/evaluator.c $(SRCDIR)/calculator.c
OBJ = $(patsubst $(SRCDIR)/%.c, $(BUILD)/%.o, $(SRC))

$(BUILD)/program: $(SRCDIR)/main.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $^

$(BUILD)/%.o: $(SRCDIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

# Tests inline
$(BUILD)/tokenizer_test: $(SRCDIR)/tokenizer.c | $(BUILD)
	$(CC) $(CFLAGS) -DTEST -o $@ $<

$(BUILD)/evaluator_test: $(SRCDIR)/evaluator.c | $(BUILD)
	$(CC) $(CFLAGS) -DTEST -o $@ $<

# Tests externos
$(BUILD)/test_calculator: $(TESTDIR)/test_calculator.c $(OBJ) | $(BUILD)
	$(CC) $(CFLAGS) -I$(SRCDIR) -I$(TESTDIR) -o $@ $^

# Runner
ALL_TESTS = $(BUILD)/tokenizer_test $(BUILD)/evaluator_test $(BUILD)/test_calculator

test: $(ALL_TESTS)
	@pass=0; fail=0; \
	for t in $(ALL_TESTS); do \
		name=$$(basename $$t); \
		./$$t > /dev/null 2>&1; \
		if [ $$? -eq 0 ]; then \
			printf "  [PASS] %s\n" "$$name"; pass=$$((pass+1)); \
		else \
			printf "  [FAIL] %s\n" "$$name"; fail=$$((fail+1)); \
		fi; \
	done; \
	echo ""; echo "$$pass passed, $$fail failed"; [ $$fail -eq 0 ]

clean:
	rm -rf $(BUILD)

.PHONY: test clean
```

---

## 13. Programa de practica

### Objetivo

Implementar un **evaluador de expresiones aritmeticas** simple (suma, resta, multiplicacion, division de enteros) usando compilacion condicional para los tests. El evaluador tiene funciones internas (`static`) que necesitan ser testeadas.

### Especificacion

```c
// expr.h — API publica
#ifndef EXPR_H
#define EXPR_H

typedef enum {
    EXPR_OK = 0,
    EXPR_ERR_EMPTY,
    EXPR_ERR_SYNTAX,
    EXPR_ERR_DIV_ZERO,
    EXPR_ERR_OVERFLOW
} ExprError;

// Evalua una expresion aritmetica.
// Soporta: +, -, *, /, parentesis, enteros positivos y negativos.
// Ejemplo: "2 + 3 * (4 - 1)" → result = 11
ExprError expr_eval(const char *input, long *result);

// Retorna una descripcion legible del error
const char *expr_error_str(ExprError err);

#endif
```

```c
// expr.c — implementacion con tests inline

// Funciones internas (static) que necesitan tests:
static bool is_digit(char c);
static long parse_integer(const char **cursor);
static long eval_factor(const char **cursor, ExprError *err);  // numero o (expr)
static long eval_term(const char **cursor, ExprError *err);    // factor * factor
static long eval_expression(const char **cursor, ExprError *err); // term + term

// API publica
ExprError expr_eval(const char *input, long *result) {
    // ... wrapper que llama a eval_expression ...
}

#ifdef TEST
// Tests de las funciones static:
// - test_is_digit
// - test_parse_integer_positive
// - test_parse_integer_negative
// - test_parse_integer_leading_zeros
// - test_eval_factor_number
// - test_eval_factor_parenthesized
// - test_eval_factor_nested_parens
// - test_eval_term_multiplication
// - test_eval_term_division
// - test_eval_term_division_by_zero
// - test_eval_expression_addition
// - test_eval_expression_precedence
// - test_expr_eval_complex  (API publica)
// - test_expr_eval_errors   (API publica)
int main(void) { /* ... */ }
#endif
```

### Makefile

```makefile
# Produccion: gcc -c expr.c -o expr.o (sin main, sin tests)
# Test:       gcc -DTEST expr.c -o test_expr (con main de test)
# Asan:       gcc -DTEST -fsanitize=address,undefined expr.c -o test_expr
```

### Que verificar

1. Cada funcion `static` tiene al menos 3 tests
2. Los tests cubren happy path, edge cases, y errores
3. El `main()` de test esta dentro de `#ifdef TEST`
4. Compilar **sin** `-DTEST` produce un object file (`.o`) sin `main()`
5. Compilar **con** `-DTEST` produce un ejecutable que pasa todos los tests
6. El Makefile tiene targets `test`, `test-asan`, y `clean`

---

## 14. Ejercicios

### Ejercicio 1: Agregar tests inline a un archivo existente

Toma tu implementacion del ring buffer del T01 (o la del string builder del T02) y agrega un bloque `#ifdef TEST` al final con tests para las funciones internas. Si no tienes funciones `static`, crea al menos una (por ejemplo, una funcion helper que calcula el siguiente indice con wrap-around).

### Ejercicio 2: #include del .c

Crea un archivo `test_via_include.c` que haga `#include "ring_buffer.c"` y testee las funciones `static` desde un archivo separado. Asegurate de que el Makefile no linkee `ring_buffer.o` cuando compila este test.

### Ejercicio 3: Fake time con compilacion condicional

Implementa un modulo `rate_limiter.c` que limite las llamadas a una funcion a N por segundo. Usa `#ifdef TEST` para inyectar un reloj falso (`fake_time`) y escribe tests que verifiquen:
- Se permiten exactamente N llamadas en un segundo
- La llamada N+1 se rechaza
- Despues de avanzar el tiempo 1 segundo, se permiten N llamadas mas

Sin el reloj falso, este test requeriria `sleep()` y seria lento e indeterminista.

### Ejercicio 4: Makefile completo

Crea un Makefile para un proyecto con 3 modulos (`math.c`, `string_utils.c`, `buffer.c`) donde:
- `math.c` y `buffer.c` tienen tests inline (`#ifdef TEST`)
- `string_utils.c` se testea externamente (`tests/test_string_utils.c`)
- El target `test` ejecuta los 3 suites de test
- El target `test-asan` recompila todo con ASan y ejecuta
- El target `test-verbose` muestra el output completo de cada test
- Autodescubrimiento: el Makefile detecta cuales `.c` tienen `#ifdef TEST` con `grep`

---

## 15. Resumen

```
┌────────────────────────────────────────────────────────────────────────────────┐
│           COMPILACION CONDICIONAL PARA TESTS — MAPA MENTAL                   │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  ¿Que es?                                                                     │
│  └─ Usar #ifdef TEST para incluir/excluir codigo de test en el mismo .c      │
│                                                                                │
│  ¿Por que?                                                                    │
│  ├─ Testear funciones static (inaccesibles desde otro archivo)               │
│  ├─ Testear estado interno (struct fields, variables static)                 │
│  └─ Inyectar dependencias fake (filesystem, tiempo, I/O)                     │
│                                                                                │
│  ¿Como?                                                                       │
│  ├─ #ifdef TEST ... #endif /* TEST */ al final del .c                        │
│  ├─ Compilar: gcc -DTEST foo.c -o foo_test                                   │
│  ├─ Produccion: gcc -c foo.c -o foo.o (sin -DTEST)                           │
│  └─ #undef NDEBUG dentro de #ifdef TEST (asegurar asserts activos)           │
│                                                                                │
│  Variantes:                                                                   │
│  ├─ -DTEST=1/2/3 para niveles (unit, integration, etc.)                      │
│  ├─ Macros separadas: -DTEST_MOCK, -DTEST_FS, etc.                           │
│  ├─ #include "foo.c" desde test_foo.c (acceso a static sin inline tests)     │
│  └─ Fake implementations (#ifdef TEST para reemplazar I/O, time, etc.)       │
│                                                                                │
│  Makefile:                                                                    │
│  ├─ Target %_test: %.c — regla generica para tests inline                    │
│  ├─ Target test: ejecuta todos los tests                                     │
│  ├─ Target test-asan: recompila con sanitizers                               │
│  └─ Autodescubrimiento con grep '#ifdef TEST'                                │
│                                                                                │
│  Comparacion:                                                                 │
│  ├─ C: #ifdef TEST → manual, flexible, peligroso si se olvida #endif        │
│  ├─ Rust: #[cfg(test)] → automatico, mismo patron, mas seguro               │
│  └─ Go: _test.go → archivo separado, mismo paquete, acceso a privados       │
│                                                                                │
│  Trampas:                                                                     │
│  ├─ Olvidar #endif                                                           │
│  ├─ Linkear .c como test Y como objeto → duplicate symbols                   │
│  ├─ Includes de test fuera del bloque #ifdef TEST                            │
│  ├─ Estado static que persiste entre tests                                   │
│  └─ Comportamiento diferente test/produccion por macros condicionales        │
│                                                                                │
│  Proximo topico: T04 — Fixtures y setup/teardown                              │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

**Navegacion**:
← Anterior: T02 — Estructura de un test | Siguiente: T04 — Fixtures y setup/teardown →

---

*Bloque 17 — Testing e Ingenieria de Software > Capitulo 1 — Testing en C > Seccion 1 — Fundamentos > Topico 3 — Compilacion condicional para tests*
