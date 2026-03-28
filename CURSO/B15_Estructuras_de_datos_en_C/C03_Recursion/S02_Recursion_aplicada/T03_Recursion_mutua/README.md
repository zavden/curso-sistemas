# Recursión mutua

## Definición

En la **recursión directa** una función se llama a sí misma.  En la **recursión
mutua** (o *indirecta*) dos o más funciones se llaman entre sí formando un
ciclo:

```
A ──llama──▶ B ──llama──▶ A ──llama──▶ B ── …
```

O, en general, un ciclo de longitud $k$:

```
f₁ → f₂ → f₃ → … → fₖ → f₁
```

La recursión directa es el caso particular $k = 1$.  Aunque toda recursión
mutua puede transformarse en recursión directa (y después en iteración), la
forma mutua aparece naturalmente cuando el problema tiene **estados
alternantes** o **categorías gramaticales que se refieren entre sí**.

---

## El ejemplo canónico: is_even / is_odd

La definición matemática de paridad por inducción mutua:

$$
\text{is\_even}(n) =
\begin{cases}
\text{true}  & \text{si } n = 0 \\
\text{is\_odd}(n - 1) & \text{si } n > 0
\end{cases}
$$

$$
\text{is\_odd}(n) =
\begin{cases}
\text{false} & \text{si } n = 0 \\
\text{is\_even}(n - 1) & \text{si } n > 0
\end{cases}
$$

Cada función delega en la otra, reduciendo $n$ en 1 cada paso.  Para $n = 4$:

```
is_even(4) → is_odd(3) → is_even(2) → is_odd(1) → is_even(0) → true
```

Se generan $n + 1$ marcos en la pila: profundidad $O(n)$.  Es un ejemplo
pedagógico — nadie comprueba paridad así en producción (`n % 2` es $O(1)$) —
pero ilustra perfectamente la mecánica de la recursión mutua.

---

## Forward declarations en C

En C, el compilador procesa el archivo de arriba abajo.  Si `is_even` llama a
`is_odd` y `is_odd` llama a `is_even`, una de las dos funciones aún no ha sido
**declarada** cuando la otra la referencia.  Sin una declaración previa el
compilador emite un error (o, en C89, asume `int` — fuente de bugs).

La solución es una **forward declaration** (prototipo):

```c
#include <stdio.h>
#include <stdbool.h>

/* Forward declaration: el compilador sabe que is_odd existe
   y conoce su firma, aunque aún no tiene el cuerpo. */
bool is_odd(unsigned int n);

bool is_even(unsigned int n) {
    if (n == 0) return true;
    return is_odd(n - 1);
}

bool is_odd(unsigned int n) {
    if (n == 0) return false;
    return is_even(n - 1);
}

int main(void) {
    for (unsigned int i = 0; i <= 10; i++) {
        printf("%u: even=%d  odd=%d\n", i, is_even(i), is_odd(i));
    }
    return 0;
}
```

### Reglas del forward declaration

| Aspecto | Detalle |
|---------|---------|
| Sintaxis | Firma de la función seguida de `;` (sin cuerpo) |
| Número de prototipos | Se puede declarar una función tantas veces como se quiera, siempre que las firmas coincidan |
| Lugar habitual | Al inicio del archivo o en un `.h` (header) |
| Consecuencia de omitirlo | Error de compilación en C99+ (`implicit declaration of function`) |
| En archivos separados | Se coloca el prototipo en un header compartido (ej. `parity.h`) |

### Con header (proyecto de varios archivos)

```c
/* parity.h */
#ifndef PARITY_H
#define PARITY_H

#include <stdbool.h>

bool is_even(unsigned int n);
bool is_odd(unsigned int n);

#endif
```

```c
/* parity.c */
#include "parity.h"

bool is_even(unsigned int n) {
    if (n == 0) return true;
    return is_odd(n - 1);
}

bool is_odd(unsigned int n) {
    if (n == 0) return false;
    return is_even(n - 1);
}
```

Ambas funciones se ven mutuamente porque el `#include "parity.h"` proporciona
los prototipos antes de cualquier cuerpo.

---

## Rust: no necesita forward declarations

El compilador de Rust resuelve nombres en **dos pasadas**: primero recopila
todas las firmas del módulo y luego compila los cuerpos.  El orden de definición
no importa:

```rust
fn is_even(n: u32) -> bool {
    if n == 0 { return true; }
    is_odd(n - 1)       // is_odd se define más abajo — no hay problema
}

fn is_odd(n: u32) -> bool {
    if n == 0 { return false; }
    is_even(n - 1)
}

fn main() {
    for i in 0..=10 {
        println!("{i}: even={}  odd={}", is_even(i), is_odd(i));
    }
}
```

### Comparación de mecanismos

| Aspecto | C | Rust |
|---------|---|------|
| Resolución de nombres | Una pasada, arriba → abajo | Dos pasadas: firmas, luego cuerpos |
| Forward declaration | Obligatorio para recursión mutua | Innecesario |
| Consecuencia de omitir | Error en C99+ o UB en C89 | N/A — siempre funciona |
| Multi-archivo | Prototipos en header `.h` | Sistema de módulos (`mod`, `pub`) |
| Orden de definición | Importa | No importa |

---

## Traza detallada

### is_even(5)

```
Llamada               Resultado parcial           Pila (profundidad)
──────────────────────────────────────────────────────────────────────
is_even(5)            → is_odd(4)                 [1]
  is_odd(4)           → is_even(3)                [2]
    is_even(3)        → is_odd(2)                 [3]
      is_odd(2)       → is_even(1)                [4]
        is_even(1)    → is_odd(0)                 [5]
          is_odd(0)   → false                     [6]  ← caso base
        is_even(1)    ← false                     [5]
      is_odd(2)       ← false                     [4]
    is_even(3)        ← false                     [3]
  is_odd(4)           ← false                     [2]
is_even(5)            ← false                     [1]

Resultado: false (5 es impar)
```

Se alternan las funciones exactamente $n$ veces hasta llegar al caso base.  La
profundidad de pila es $n + 1$ (contando la llamada inicial).

---

## Ejemplos más allá de paridad

### Secuencias de Hofstadter

Douglas Hofstadter (autor de *Gödel, Escher, Bach*) definió varias secuencias
mutuamente recursivas.  La más conocida es la **secuencia Femenina-Masculina**:

$$F(0) = 1, \quad M(0) = 0$$

$$F(n) = n - M(F(n-1)), \quad n > 0$$

$$M(n) = n - F(M(n-1)), \quad n > 0$$

Cada función llama a la otra y también se llama a sí misma indirectamente.

```c
/* Forward declarations */
int M(int n);

int F(int n) {
    if (n == 0) return 1;
    return n - M(F(n - 1));
}

int M(int n) {
    if (n == 0) return 0;
    return n - F(M(n - 1));
}
```

```rust
fn f(n: i32) -> i32 {
    if n == 0 { return 1; }
    n - m(f(n - 1))
}

fn m(n: i32) -> i32 {
    if n == 0 { return 0; }
    n - f(m(n - 1))
}
```

Primeros valores:

| $n$ | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|-----|---|---|---|---|---|---|---|---|---|
| $F(n)$ | 1 | 1 | 2 | 2 | 3 | 3 | 4 | 5 | 5 |
| $M(n)$ | 0 | 0 | 1 | 2 | 2 | 3 | 4 | 4 | 5 |

Nota: Hofstadter es también la fuente de *mutual recursion* más compleja — su
secuencia Q ($Q(n) = Q(n - Q(n-1))$) es directa pero caótica; F/M es un
ejemplo limpio de mutua.

### Máquina de estados finita (FSM)

Muchos protocolos alternan entre estados.  Una FSM con estados A y B que procesa
una cadena carácter a carácter:

```
Estado A: si el carácter es '0' → ir a B
          si el carácter es '1' → quedarse en A
          fin de cadena → aceptar

Estado B: si el carácter es '0' → ir a A
          si el carácter es '1' → quedarse en B
          fin de cadena → rechazar
```

Esta FSM acepta cadenas binarias con un número par de ceros.

```c
#include <stdbool.h>

bool state_b(const char *s);  /* forward declaration */

bool state_a(const char *s) {
    if (*s == '\0') return true;   /* aceptar */
    if (*s == '0') return state_b(s + 1);
    return state_a(s + 1);        /* '1': quedarse en A */
}

bool state_b(const char *s) {
    if (*s == '\0') return false;  /* rechazar */
    if (*s == '0') return state_a(s + 1);
    return state_b(s + 1);        /* '1': quedarse en B */
}
```

```rust
fn state_a(s: &[u8]) -> bool {
    match s.first() {
        None       => true,                    // aceptar
        Some(b'0') => state_b(&s[1..]),
        _          => state_a(&s[1..]),        // '1': quedarse en A
    }
}

fn state_b(s: &[u8]) -> bool {
    match s.first() {
        None       => false,                   // rechazar
        Some(b'0') => state_a(&s[1..]),
        _          => state_b(&s[1..]),        // '1': quedarse en B
    }
}

fn accepts(input: &str) -> bool {
    state_a(input.as_bytes())
}
```

Cada estado es una función; las transiciones son llamadas.  Este patrón escala a
FSMs complejas — compiladores, protocolos de red, lexers.

### Descenso recursivo (preview)

Un parser de descenso recursivo para una gramática como:

```
expr   → term (('+' | '-') term)*
term   → factor (('*' | '/') factor)*
factor → NUMBER | '(' expr ')'
```

`factor` llama a `expr` (por los paréntesis) y `expr` llama a `term` que llama
a `factor` — un ciclo de longitud 3.  En C esto requiere forward declarations
de `expr` y `term` antes de `factor`.  En Rust no.  Profundizaremos en parsers
en bloques posteriores, pero la recursión mutua es el fundamento.

---

## Conversión a recursión directa

Toda recursión mutua entre dos funciones $A$ y $B$ puede eliminarse
**inlining** una dentro de la otra.  Para is_even/is_odd:

1. En `is_even`, sustituir la llamada a `is_odd(n-1)` por el cuerpo de
   `is_odd` evaluado en `n-1`:

```
is_even(n) =
    si n == 0 → true
    si n-1 == 0 → false          (caso base de is_odd)
    sino → is_even((n-1) - 1)    (is_odd llama a is_even)
```

Simplificando:

```
is_even(n) =
    si n == 0 → true
    si n == 1 → false
    sino → is_even(n - 2)
```

Ahora es recursión directa con paso -2.

```c
bool is_even_direct(unsigned int n) {
    if (n == 0) return true;
    if (n == 1) return false;
    return is_even_direct(n - 2);
}
```

```rust
fn is_even_direct(n: u32) -> bool {
    match n {
        0 => true,
        1 => false,
        _ => is_even_direct(n - 2),
    }
}
```

### Cuándo es viable el inlining

| Caso | Viable | Razón |
|------|--------|-------|
| Ciclo de 2 funciones simples | Sí | Sustitución directa |
| Ciclo de 3+ funciones | En teoría sí, en la práctica explota el tamaño | Cada inline puede duplicar el código |
| Funciones con efectos laterales distintos | Con cuidado | Los efectos deben mantenerse en orden |
| FSM con muchos estados | No práctico | Mejor usar tabla de transiciones |

---

## Conversión a iteración

### Método directo (para is_even/is_odd)

Después de convertir a recursión directa (`is_even(n-2)`), la conversión a
iteración es trivial:

```c
bool is_even_iter(unsigned int n) {
    return (n % 2) == 0;    /* la solución obvia */
}
```

Pero si queremos preservar la estructura alternante (útil para FSMs):

```c
bool is_even_loop(unsigned int n) {
    bool in_even = true;  /* empezamos en is_even */
    while (n > 0) {
        in_even = !in_even;
        n--;
    }
    return in_even;
}
```

El patrón general: una variable de **estado** indica en cuál función estamos, y
un bucle despacha según ese estado.

### El patrón trampolín

Para recursión mutua genérica donde el inlining no es práctico, existe la
técnica del **trampolín** (*trampoline*).  En lugar de llamar directamente a la
otra función (añadiendo un marco a la pila), cada función retorna una
**instrucción** indicando qué función llamar a continuación y con qué
argumentos.  Un bucle exterior (el trampolín) ejecuta esas instrucciones:

```c
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    enum { DONE, CALL_EVEN, CALL_ODD } tag;
    union {
        bool result;           /* si tag == DONE */
        unsigned int arg;      /* si tag == CALL_EVEN o CALL_ODD */
    };
} Bounce;

Bounce even_bounce(unsigned int n) {
    if (n == 0) return (Bounce){ .tag = DONE, .result = true };
    return (Bounce){ .tag = CALL_ODD, .arg = n - 1 };
}

Bounce odd_bounce(unsigned int n) {
    if (n == 0) return (Bounce){ .tag = DONE, .result = false };
    return (Bounce){ .tag = CALL_EVEN, .arg = n - 1 };
}

bool trampoline(unsigned int n) {
    Bounce b = { .tag = CALL_EVEN, .arg = n };
    while (b.tag != DONE) {
        if (b.tag == CALL_EVEN) {
            b = even_bounce(b.arg);
        } else {
            b = odd_bounce(b.arg);
        }
    }
    return b.result;
}
```

En Rust, el enum con datos hace esto más elegante:

```rust
enum Bounce {
    Done(bool),
    CallEven(u32),
    CallOdd(u32),
}

fn even_bounce(n: u32) -> Bounce {
    if n == 0 { Bounce::Done(true) }
    else { Bounce::CallOdd(n - 1) }
}

fn odd_bounce(n: u32) -> Bounce {
    if n == 0 { Bounce::Done(false) }
    else { Bounce::CallEven(n - 1) }
}

fn trampoline(n: u32) -> bool {
    let mut bounce = Bounce::CallEven(n);
    loop {
        match bounce {
            Bounce::Done(result) => return result,
            Bounce::CallEven(n)  => bounce = even_bounce(n),
            Bounce::CallOdd(n)   => bounce = odd_bounce(n),
        }
    }
}
```

### Ventajas del trampolín

| Aspecto | Recursión mutua directa | Trampolín |
|---------|------------------------|-----------|
| Profundidad de pila | $O(n)$ | $O(1)$ — siempre 1 marco |
| Velocidad | Ligeramente más rápida (llamadas directas) | Overhead del bucle y match |
| Stack overflow | Posible para $n$ grande | Imposible |
| Claridad | Más legible para funciones simples | Mejor para FSMs complejas |

El trampolín es especialmente valioso en lenguajes sin TCO garantizada (C, Rust,
Java, Python) cuando la profundidad puede ser grande.

---

## TCO y recursión mutua en cola

Observa que en is_even/is_odd, la llamada mutua es la **última operación** de
cada función — es recursión de cola.  Pero aquí hay un matiz importante:

- **TCO de recursión directa**: el compilador reemplaza `call` por `jmp` a la
  misma función.  GCC lo hace con `-O2`.
- **TCO de recursión mutua** (*sibling call optimization*, SCO): el compilador
  reemplaza `call` por `jmp` a **otra** función.  GCC también lo hace con
  `-O2`, pero es menos predecible.

```
; Sin SCO (recursión mutua):
is_even:
    ...
    call is_odd      ; nuevo marco en la pila
    ret

; Con SCO (recursión mutua optimizada):
is_even:
    ...
    jmp is_odd       ; reutiliza el marco actual
```

Para verificar con GCC:

```bash
gcc -O2 -S parity.c -o parity.s
grep -E 'call|jmp' parity.s | grep -E 'is_even|is_odd'
```

Si aparece `jmp is_odd` en lugar de `call is_odd`, GCC está aplicando SCO.

### Estado de SCO por compilador

| Compilador | Recursión directa (TCO) | Recursión mutua (SCO) |
|-----------|--------------------------|------------------------|
| GCC `-O2` | Sí, confiable | Sí, usualmente |
| Clang `-O2` | Sí, confiable | Sí, usualmente |
| rustc (release) | A veces | Rara vez |
| MSVC | Limitado | Muy limitado |

En Rust, la recomendación sigue siendo la misma que para TCO directa: no
depender de ella.  Usar `loop` con variable de estado si la profundidad es un
problema.

---

## Recursión mutua con más de dos funciones

El concepto se extiende a ciclos de longitud $k > 2$.  Ejemplo: un clasificador
que alterna entre tres estados para determinar si un número es divisible entre 3:

$$
\text{mod3\_0}(n) =
\begin{cases}
\text{true}  & n = 0 \\
\text{mod3\_1}(n-1)
\end{cases}
\quad
\text{mod3\_1}(n) =
\begin{cases}
\text{false} & n = 0 \\
\text{mod3\_2}(n-1)
\end{cases}
\quad
\text{mod3\_2}(n) =
\begin{cases}
\text{false} & n = 0 \\
\text{mod3\_0}(n-1)
\end{cases}
$$

```c
bool mod3_1(unsigned int n);
bool mod3_2(unsigned int n);

bool mod3_0(unsigned int n) {
    if (n == 0) return true;
    return mod3_1(n - 1);
}

bool mod3_1(unsigned int n) {
    if (n == 0) return false;
    return mod3_2(n - 1);
}

bool mod3_2(unsigned int n) {
    if (n == 0) return false;
    return mod3_0(n - 1);
}
```

Profundidad de pila: $O(n)$.  Después del inlining: `mod3_0(n) → mod3_0(n-3)`,
recursión directa con paso -3, trivialmente convertible a `n % 3 == 0`.

El patrón se generaliza: un ciclo de $k$ funciones donde cada una decrementa en
1 equivale a comprobar `n % k == r` para distintos restos $r$.

---

## Comparación: recursión mutua vs directa

| Aspecto | Directa | Mutua |
|---------|---------|-------|
| Funciones involucradas | 1 | 2 o más |
| Forward declarations (C) | No necesarias | Al menos $k-1$ prototipos |
| Legibilidad | Una función contiene toda la lógica | Lógica distribuida entre funciones |
| Depuración | Un breakpoint basta | Necesitas breakpoints en cada función |
| TCO/SCO | Bien soportada | Menos predecible |
| Inlining a directa | N/A | Posible pero puede explotar el tamaño |
| Caso de uso natural | Reducción uniforme | Estados alternantes, gramáticas |

---

## Cuándo usar recursión mutua

### Usar cuando

1. **El problema tiene estados alternantes naturales** — FSMs, protocolos de
   turno (jugador A / jugador B), parsers de gramáticas con producciones
   mutuamente referentes.

2. **La profundidad es acotada** — gramáticas con anidamiento limitado
   ($O(\log n)$ o constante), FSMs sobre entradas de tamaño razonable.

3. **La claridad compensa** — cada función representa un concepto distinto
   (estado, categoría gramatical) y separarlas mejora la legibilidad.

### Evitar cuando

1. **La profundidad es $O(n)$ con $n$ grande** — como is_even(1000000).
   Convertir a iteración con variable de estado o trampolín.

2. **El ciclo es artificial** — si las funciones no representan conceptos
   distintos sino que se separaron arbitrariamente, unificar en una sola.

3. **El lenguaje no soporta SCO** — en entornos sin TCO de sibling calls,
   la recursión mutua profunda desborda la pila.

---

## Implementación completa: FSM para cadenas binarias

Un ejemplo completo que combina todo: forward declarations en C, enums en Rust,
y versión iterativa con trampolín.

### Problema

Determinar si una cadena binaria contiene un número par de ceros **y** un
número par de unos.  Esto requiere 4 estados (combinaciones de paridad de 0s y
1s):

| Estado | Ceros vistos | Unos vistos | Aceptar |
|--------|-------------|-------------|---------|
| S00 | par | par | Sí |
| S01 | par | impar | No |
| S10 | impar | par | No |
| S11 | impar | impar | No |

### C — recursión mutua con 4 funciones

```c
#include <stdio.h>
#include <stdbool.h>

/* Forward declarations */
bool s01(const char *s);
bool s10(const char *s);
bool s11(const char *s);

bool s00(const char *s) {
    if (*s == '\0') return true;
    if (*s == '0')  return s10(s + 1);   /* 0: cambia paridad de ceros */
    return s01(s + 1);                   /* 1: cambia paridad de unos  */
}

bool s01(const char *s) {
    if (*s == '\0') return false;
    if (*s == '0')  return s11(s + 1);
    return s00(s + 1);
}

bool s10(const char *s) {
    if (*s == '\0') return false;
    if (*s == '0')  return s00(s + 1);
    return s11(s + 1);
}

bool s11(const char *s) {
    if (*s == '\0') return false;
    if (*s == '0')  return s01(s + 1);
    return s10(s + 1);
}

int main(void) {
    const char *tests[] = {"", "0", "00", "01", "0011", "0110", "111", NULL};
    for (const char **t = tests; *t != NULL; t++) {
        printf("\"%s\" -> %s\n", *t, s00(*t) ? "accept" : "reject");
    }
    return 0;
}
```

Salida esperada:

```
"" -> accept         (0 ceros, 0 unos: par, par)
"0" -> reject        (1 cero: impar)
"00" -> reject       (2 ceros, 0 unos: par, par? — sí, accept) ... Corrección:
"00" -> accept       (2 ceros par, 0 unos par)
"01" -> reject       (1 cero impar, 1 uno impar)
"0011" -> accept     (2 ceros par, 2 unos par)
"0110" -> accept     (2 ceros par, 2 unos par)
"111" -> reject      (0 ceros par, 3 unos impar)
```

### Rust — sin forward declarations

```rust
fn s00(s: &[u8]) -> bool {
    match s.first() {
        None       => true,
        Some(b'0') => s10(&s[1..]),
        _          => s01(&s[1..]),
    }
}

fn s01(s: &[u8]) -> bool {
    match s.first() {
        None       => false,
        Some(b'0') => s11(&s[1..]),
        _          => s00(&s[1..]),
    }
}

fn s10(s: &[u8]) -> bool {
    match s.first() {
        None       => false,
        Some(b'0') => s00(&s[1..]),
        _          => s11(&s[1..]),
    }
}

fn s11(s: &[u8]) -> bool {
    match s.first() {
        None       => false,
        Some(b'0') => s01(&s[1..]),
        _          => s10(&s[1..]),
    }
}

fn accepts(input: &str) -> bool {
    s00(input.as_bytes())
}

fn main() {
    let tests = ["", "0", "00", "01", "0011", "0110", "111"];
    for t in &tests {
        println!("\"{}\" -> {}", t, if accepts(t) { "accept" } else { "reject" });
    }
}
```

### Versión iterativa (tabla de transiciones)

```c
#include <stdio.h>
#include <stdbool.h>

bool fsm_accepts(const char *s) {
    /*         '0'  '1'
       S00  ->  S10  S01
       S01  ->  S11  S00
       S10  ->  S00  S11
       S11  ->  S01  S10  */
    enum { S00, S01, S10, S11 } state = S00;

    while (*s != '\0') {
        int input = (*s == '0') ? 0 : 1;
        static const int table[4][2] = {
            /* S00 */ { S10, S01 },
            /* S01 */ { S11, S00 },
            /* S10 */ { S00, S11 },
            /* S11 */ { S01, S10 },
        };
        state = table[state][input];
        s++;
    }
    return state == S00;
}
```

```rust
fn fsm_accepts(input: &str) -> bool {
    #[derive(Clone, Copy, PartialEq)]
    enum State { S00, S01, S10, S11 }

    let mut state = State::S00;
    for byte in input.bytes() {
        state = match (state, byte) {
            (State::S00, b'0') => State::S10,
            (State::S00, _)    => State::S01,
            (State::S01, b'0') => State::S11,
            (State::S01, _)    => State::S00,
            (State::S10, b'0') => State::S00,
            (State::S10, _)    => State::S11,
            (State::S11, b'0') => State::S01,
            (State::S11, _)    => State::S10,
        };
    }
    state == State::S00
}
```

Esta versión iterativa es $O(n)$ en tiempo, $O(1)$ en espacio, y no tiene
riesgo de stack overflow.  Pero la versión recursiva mutua refleja directamente
la definición de la FSM — útil durante el diseño y la depuración.

---

## Ejercicios

### Ejercicio 1 — Traza manual de is_even

Traza la ejecución de `is_even(7)` mostrando cada llamada, la función invocada
y la profundidad de pila.

<details>
<summary>Predice el resultado antes de trazar</summary>

Resultado esperado: `false` (7 es impar).  Se generan 8 llamadas (profundidad
8): is_even(7) → is_odd(6) → is_even(5) → is_odd(4) → is_even(3) → is_odd(2)
→ is_even(1) → is_odd(0) → false.
</details>

### Ejercicio 2 — Forward declarations

El siguiente código no compila.  Identifica el error y corrígelo:

```c
#include <stdbool.h>

bool is_even(unsigned int n) {
    if (n == 0) return true;
    return is_odd(n - 1);
}

bool is_odd(unsigned int n) {
    if (n == 0) return false;
    return is_even(n - 1);
}
```

<details>
<summary>Pista y solución</summary>

Error: `is_even` llama a `is_odd` que aún no ha sido declarada.  Solución:
añadir `bool is_odd(unsigned int n);` antes de `is_even`.  En Rust esto no
ocurre — el compilador resuelve nombres en dos pasadas.
</details>

### Ejercicio 3 — Inlining a recursión directa

Transforma las funciones `is_odd` / `is_even` mutuamente recursivas en una
única función `is_odd_direct` que use recursión directa (sin llamar a ninguna
otra función).  Implementa en C y Rust.

<details>
<summary>Predice la estructura</summary>

`is_odd(n)`: si $n = 0$ → false, si $n = 1$ → true, sino → `is_odd(n - 2)`.
Es el mismo patrón que `is_even_direct` pero con los casos base invertidos.
La recursión reduce $n$ de 2 en 2 hasta llegar a 0 o 1.
</details>

### Ejercicio 4 — Secuencia de Hofstadter

Implementa las secuencias $F(n)$ y $M(n)$ de Hofstadter en C (con forward
declaration) y en Rust.  Genera los primeros 20 valores de cada una y verifica
que coinciden con la tabla del tópico.

<details>
<summary>Pista sobre la complejidad</summary>

Estas funciones tienen recursión mutua **y** directa anidada: $F(n) = n -
M(F(n-1))$.  La llamada a $F(n-1)$ es recursión directa, y el resultado se
pasa a $M$, que es mutua.  La profundidad real es difícil de analizar — para
valores pequeños ($n < 100$) no hay problema, pero crece rápido.  Usa
memoización (array de resultados previos) si quieres calcular valores grandes.
</details>

### Ejercicio 5 — Trampolín para paridad

Implementa el trampolín completo para is_even/is_odd en C y Rust (puedes partir
del código del tópico).  Verifica que funciona para $n = 1\,000\,000$ sin stack
overflow, y compara el tiempo contra la versión recursiva directa (que
probablemente desborde la pila para ese valor).

<details>
<summary>Predice qué ocurre con la recursión directa para n = 1000000</summary>

Con la recursión mutua directa (sin TCO), cada llamada consume un marco (~32-64
bytes).  Para $n = 10^6$: $\approx 32$ MB, probablemente excede el stack por
defecto (8 MB).  Resultado: SIGSEGV en C o abort en Rust.  El trampolín usa
$O(1)$ de pila — funciona sin problema.
</details>

### Ejercicio 6 — FSM de 3 estados

Diseña e implementa una FSM con recursión mutua que acepte cadenas sobre el
alfabeto `{a, b}` donde el número de caracteres `a` sea divisible entre 3.
Necesitarás 3 estados (mod3_0, mod3_1, mod3_2).  Implementa en C con forward
declarations y en Rust.

<details>
<summary>Estructura esperada</summary>

```
mod3_0(s): fin → accept; 'a' → mod3_1(s+1); 'b' → mod3_0(s+1)
mod3_1(s): fin → reject; 'a' → mod3_2(s+1); 'b' → mod3_1(s+1)
mod3_2(s): fin → reject; 'a' → mod3_0(s+1); 'b' → mod3_2(s+1)
```

Solo las 'a' provocan transición de estado; las 'b' son ignoradas (permanecen
en el estado actual).
</details>

### Ejercicio 7 — Verificar SCO con GCC

Compila el ejemplo is_even/is_odd con `gcc -O2 -S` y examina el ensamblador
generado.  Responde:

1. ¿Aparece `call` o `jmp` para las llamadas mutuas?
2. ¿Cuántos marcos de pila consume is_even(1000000) con `-O2`?
3. ¿Qué ocurre si compilas con `-O0`?

<details>
<summary>Resultado esperado</summary>

1. Con `-O2`, GCC suele generar `jmp` (SCO aplicada).  2. Con SCO: $O(1)$
marcos — la función se ejecuta en un bucle.  3. Con `-O0`: `call` sin
optimización — desborda la pila para $n$ grande.  Algunos compiladores incluso
reducen is_even/is_odd a `n % 2 == 0` con `-O2`.
</details>

### Ejercicio 8 — Conversión de FSM a tabla

Toma la FSM de 4 estados (par de ceros y unos) implementada con recursión mutua
y conviértela a una versión iterativa con tabla de transiciones (como el ejemplo
del tópico).  Luego generaliza: escribe una función `fsm_run` que reciba la
tabla como parámetro (en C como array 2D, en Rust como `&[[usize]]`).

<details>
<summary>Firma sugerida</summary>

```c
bool fsm_run(const int table[][2], int num_states,
             int accept_state, const char *input);
```

```rust
fn fsm_run(table: &[[usize; 2]], accept_state: usize, input: &str) -> bool;
```

La tabla codifica `table[estado][simbolo] = siguiente_estado`.  El bucle
reemplaza toda la recursión mutua.
</details>

### Ejercicio 9 — Recursión mutua con acumulador

Modifica is_even/is_odd para que lleven un **contador de profundidad** como
parámetro adicional y retornen tanto el resultado booleano como la profundidad
alcanzada.  En C usa un `struct`; en Rust retorna `(bool, u32)`.  Verifica que
la profundidad es siempre $n + 1$.

<details>
<summary>Pista para el struct en C</summary>

```c
typedef struct {
    bool result;
    unsigned int depth;
} ParityResult;
```

Cada función incrementa `depth` en 1 antes de pasarla a la otra.  El caso base
retorna `{resultado, depth}` sin incrementar más.
</details>

### Ejercicio 10 — Descenso recursivo simple

Implementa un parser mutuamente recursivo para la gramática:

```
S → '(' L ')' | 'a'
L → S (',' S)*
```

Esta gramática acepta: `a`, `(a)`, `(a,a)`, `(a,(a,a))`, etc.  Implementa
funciones `parse_s` y `parse_l` que se llamen mutuamente.  En C necesitarás un
forward declaration; en Rust no.  La función debe retornar si la cadena es
válida.

<details>
<summary>Pistas de implementación</summary>

Usa un índice (o puntero) que avanza por la cadena:

```c
bool parse_l(const char *s, int *pos);

bool parse_s(const char *s, int *pos) {
    if (s[*pos] == 'a') { (*pos)++; return true; }
    if (s[*pos] != '(') return false;
    (*pos)++;  /* consumir '(' */
    if (!parse_l(s, pos)) return false;
    if (s[*pos] != ')') return false;
    (*pos)++;  /* consumir ')' */
    return true;
}
```

En Rust, usa `&[u8]` y un `&mut usize` para el índice, o retorna un slice
reducido.
</details>
