# T04 — Recursión de cola

## Índice

1. [Definición](#1-definición)
2. [Recursión de cola vs no-cola](#2-recursión-de-cola-vs-no-cola)
3. [Tail Call Optimization (TCO)](#3-tail-call-optimization-tco)
4. [TCO en GCC](#4-tco-en-gcc)
5. [Rust y TCO](#5-rust-y-tco)
6. [Transformación a recursión de cola](#6-transformación-a-recursión-de-cola)
7. [loop en Rust como reemplazo idiomático](#7-loop-en-rust-como-reemplazo-idiomático)
8. [Limitaciones de TCO](#8-limitaciones-de-tco)
9. [Ejercicios](#9-ejercicios)

---

## 1. Definición

Una función tiene **recursión de cola** (tail recursion) cuando la llamada
recursiva es la **última operación** antes de retornar. No hay trabajo
pendiente después de la llamada — el resultado de la llamada recursiva
**es** el resultado de la función.

```c
/* Recursión de cola: la llamada recursiva ES el return */
int gcd(int a, int b) {
    if (b == 0) return a;
    return gcd(b, a % b);    /* nothing happens after gcd() returns */
}

/* NO es recursión de cola: hay multiplicación DESPUÉS de la llamada */
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);  /* must multiply by n after return */
}
```

En `gcd`, cuando `gcd(b, a % b)` retorna, su resultado se retorna
directamente, sin modificarlo. En `factorial`, el resultado de
`factorial(n-1)` debe multiplicarse por `n` — hay trabajo pendiente, así
que **no es de cola**.

### Definición formal

Una llamada a función `f(args)` está en **posición de cola** (tail
position) si es la última instrucción ejecutada antes del `return` de la
función que la contiene. Una función es **tail-recursive** si todas sus
llamadas recursivas están en posición de cola.

---

## 2. Recursión de cola vs no-cola

### Comparación visual

**No-cola** (factorial clásico):

```
factorial(4)
  4 * factorial(3)             ← debe recordar "multiplicar por 4"
        3 * factorial(2)       ← debe recordar "multiplicar por 3"
              2 * factorial(1) ← debe recordar "multiplicar por 2"
                    return 1
              return 2 * 1 = 2       ← trabajo pendiente
        return 3 * 2 = 6            ← trabajo pendiente
  return 4 * 6 = 24                 ← trabajo pendiente
```

Cada frame debe mantenerse vivo para hacer la multiplicación cuando la
llamada recursiva retorne. Se necesitan $n$ frames simultáneos.

**De cola** (factorial con acumulador):

```
factorial_tail(4, 1)
  factorial_tail(3, 4)         ← no hay trabajo pendiente
    factorial_tail(2, 12)      ← no hay trabajo pendiente
      factorial_tail(1, 24)    ← no hay trabajo pendiente
        return 24
      return 24                ← solo propaga el resultado
    return 24                  ← solo propaga el resultado
  return 24                   ← solo propaga el resultado
```

Cada frame solo necesita propagar el resultado — no modifica nada al
retornar. Esto significa que **el frame anterior ya no se necesita** cuando
se hace la llamada recursiva.

### La clave: frames innecesarios

En la versión de cola, cuando `factorial_tail(4, 1)` llama a
`factorial_tail(3, 4)`, el frame de `(4, 1)` ya no tiene trabajo
que hacer. Solo esperará el resultado para retornarlo sin cambios. Si
el compilador detecta esto, puede **reutilizar el mismo frame** en
lugar de crear uno nuevo.

---

## 3. Tail Call Optimization (TCO)

TCO es una optimización del compilador que transforma una llamada en
posición de cola en un **salto** (jump) en lugar de una llamada a
función (call). El efecto:

| Sin TCO | Con TCO |
|:-------:|:-------:|
| Crear nuevo frame | Reutilizar el frame actual |
| $O(n)$ espacio de stack | $O(1)$ espacio de stack |
| Stack overflow para $n$ grande | Nunca stack overflow |
| `call` instruction | `jmp` instruction |

### Cómo funciona internamente

Sin TCO:

```asm
factorial_tail:
    cmp edi, 1          ; if n <= 1
    jle .base
    imul esi, edi       ; acc = acc * n
    dec edi             ; n = n - 1
    call factorial_tail ; CREATE new frame, push return address
    ret                 ; return from this frame

.base:
    mov eax, esi        ; return acc
    ret
```

Con TCO:

```asm
factorial_tail:
    cmp edi, 1          ; if n <= 1
    jle .base
    imul esi, edi       ; acc = acc * n
    dec edi             ; n = n - 1
    jmp factorial_tail  ; REUSE frame, just jump back to start
                        ; no ret needed — caller's ret will suffice

.base:
    mov eax, esi        ; return acc
    ret
```

La diferencia es una instrucción: `call` → `jmp`. Pero el efecto es
profundo: `call` empuja la dirección de retorno y crea un frame; `jmp`
simplemente salta sin tocar el stack. El resultado es que la recursión
se convierte en un **bucle** a nivel de código máquina.

---

## 4. TCO en GCC

### GCC aplica TCO con -O2

```c
/* tail_factorial.c */
#include <stdio.h>

unsigned long factorial_tail(int n, unsigned long acc) {
    if (n <= 1) return acc;
    return factorial_tail(n - 1, acc * n);
}

unsigned long factorial(int n) {
    return factorial_tail(n, 1);
}

int main(void) {
    printf("%lu\n", factorial(20));
    return 0;
}
```

Compilar y verificar:

```bash
# Sin optimización: la recursión se mantiene
gcc -O0 -S -o tail_O0.s tail_factorial.c
grep "call.*factorial_tail" tail_O0.s
# factorial_tail aparece como call → NO hay TCO

# Con optimización: la recursión se elimina
gcc -O2 -S -o tail_O2.s tail_factorial.c
grep "call.*factorial_tail" tail_O2.s
# NO aparece call → SÍ hay TCO (convertido a jmp o loop)
```

El flag `-S` produce código ensamblador. Buscar `call factorial_tail`
revela si hay llamada recursiva. Con `-O2`, GCC la reemplaza por un salto.

### Verificar con GDB

```bash
gcc -g -O2 -o tail_fact tail_factorial.c
gdb ./tail_fact
```

```
(gdb) break factorial_tail
(gdb) run
(gdb) bt
#0  factorial_tail (n=20, acc=1) at tail_factorial.c:4
#1  main () at tail_factorial.c:10
```

Con TCO, el backtrace solo muestra **2 frames** (main + factorial_tail),
sin importar el valor de $n$. Sin TCO (`-O0`), mostraría $n + 1$ frames.

### gcc vs clang

Tanto GCC como Clang aplican TCO con `-O2`. Pero no hay ningún estándar
de C que **obligue** al compilador a hacerlo. Es una optimización
opcional que el compilador puede elegir no aplicar en ciertos casos.

---

## 5. Rust y TCO

### Rust NO garantiza TCO

El compilador de Rust (rustc, que usa LLVM como backend) **puede** aplicar
TCO en algunos casos, pero **no lo garantiza**. Esto es una decisión
deliberada del equipo de Rust:

1. **Backtraces legibles**: TCO elimina frames del stack, haciendo que los
   backtraces de debug sean incompletos y difíciles de interpretar.
2. **Semántica de Drop**: si hay destructores pendientes al final de la
   función, el compilador no puede reutilizar el frame (los destructores
   deben ejecutarse antes de la "vuelta").
3. **Sin especificación**: no hay sintaxis ni atributo para indicar al
   compilador "esta función debe usar TCO".

### Verificar empíricamente

```rust
// tail_test.rs
fn factorial_tail(n: u64, acc: u64) -> u64 {
    if n <= 1 { return acc; }
    factorial_tail(n - 1, acc * n)
}

fn main() {
    // This will stack overflow WITHOUT TCO
    // 10 million is way beyond stack limits
    println!("{}", factorial_tail(10_000_000, 1));
}
```

```bash
# Debug mode: no TCO, stack overflow
rustc tail_test.rs -o tail_debug
./tail_debug
# thread 'main' panicked at 'thread has overflowed its stack'

# Release mode: MIGHT apply TCO
rustc -O tail_test.rs -o tail_release
./tail_release
# may work (TCO applied) or may still overflow (TCO not applied)
```

El resultado en release depende de la versión de LLVM y de la complejidad
de la función. **No se puede depender de esto.**

### La postura oficial de Rust

La recomendación del equipo de Rust es: **no escribir recursión de cola
esperando TCO**. En su lugar, usar iteración explícita. Si la función es
tail-recursive, la conversión a bucle es trivial (se mostrará en la
sección 7).

---

## 6. Transformación a recursión de cola

Muchas funciones recursivas que no son de cola se pueden transformar
usando un **parámetro acumulador** que arrastra el resultado parcial.

### Patrón general

```
/* No-cola: trabajo pendiente después de la llamada */
f(n):
    if base: return base_value
    return COMBINE(n, f(reduce(n)))

/* De cola: el acumulador lleva el resultado parcial */
f_tail(n, acc):
    if base: return acc
    return f_tail(reduce(n), COMBINE(n, acc))

/* Wrapper para la interfaz pública */
f(n):
    return f_tail(n, initial_acc)
```

La idea: mover la operación `COMBINE` de **después** de la llamada
recursiva (no-cola) a **antes** de la llamada recursiva, acumulándola
en un parámetro extra.

### Factorial

```c
/* No-cola */
unsigned long factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);    /* multiply AFTER return */
}

/* De cola con acumulador */
unsigned long factorial_tail(int n, unsigned long acc) {
    if (n <= 1) return acc;
    return factorial_tail(n - 1, acc * n);  /* multiply BEFORE call */
}

unsigned long factorial(int n) {
    return factorial_tail(n, 1);  /* initial acc = identity for * */
}
```

El acumulador `acc` empieza en 1 (identidad de la multiplicación) y va
acumulando el producto en cada paso. Cuando se alcanza el caso base, `acc`
ya contiene el resultado final.

### Fibonacci

Fibonacci con dos llamadas recursivas no es de cola. Pero se puede
transformar con **dos acumuladores**:

```c
/* No-cola (exponencial) */
unsigned long fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

/* De cola con dos acumuladores (lineal) */
unsigned long fib_tail(int n, unsigned long a, unsigned long b) {
    if (n == 0) return a;
    return fib_tail(n - 1, b, a + b);
}

unsigned long fib(int n) {
    return fib_tail(n, 0, 1);  /* F(0)=0, F(1)=1 */
}
```

`a` es $F(k)$ y `b` es $F(k+1)$ en cada paso. Al decrementar `n` y
avanzar `a = b`, `b = a + b`, la función computa Fibonacci en $O(n)$
tiempo y $O(1)$ espacio (con TCO). Esto transforma una recursión de
árbol exponencial en una recursión de cola lineal.

### Suma de lista

```rust
// No-cola
fn sum(arr: &[i32]) -> i32 {
    if arr.is_empty() { return 0; }
    arr[0] + sum(&arr[1..])
}

// De cola
fn sum_tail(arr: &[i32], acc: i32) -> i32 {
    if arr.is_empty() { return acc; }
    sum_tail(&arr[1..], acc + arr[0])
}

fn sum(arr: &[i32]) -> i32 {
    sum_tail(arr, 0)
}
```

### Cuándo NO es posible

La transformación a recursión de cola **no es posible** cuando:

- Hay **múltiples llamadas recursivas** que no se pueden colapsar (por
  ejemplo, recorrido de árbol donde se necesita visitar ambos hijos).
- El resultado de la recursión se usa de forma no-lineal (por ejemplo,
  como índice para acceder a un array).
- La función necesita hacer trabajo **después** de la llamada que
  depende del contexto del frame actual (por ejemplo, backtracking).

---

## 7. loop en Rust como reemplazo idiomático

Dado que Rust no garantiza TCO, la práctica idiomática es convertir
recursión de cola a un `loop` explícito. La conversión es mecánica:

### Regla de conversión

```rust
// Recursión de cola
fn f(param1: T1, param2: T2) -> R {
    if base_condition {
        return base_value;
    }
    f(new_param1, new_param2)   // tail call
}

// Equivalente con loop
fn f(mut param1: T1, mut param2: T2) -> R {
    loop {
        if base_condition {
            return base_value;
        }
        // "tail call" becomes parameter reassignment
        let next1 = new_param1;  // compute new values
        let next2 = new_param2;
        param1 = next1;
        param2 = next2;
        // loop back to the top (implicit "jmp")
    }
}
```

La llamada recursiva se reemplaza por **reasignación de parámetros** y
el bucle vuelve al inicio. Esto es exactamente lo que TCO haría a nivel
de código máquina — pero escrito explícitamente en el código fuente.

### Factorial

```rust
// Recursión de cola
fn factorial_tail(n: u64, acc: u64) -> u64 {
    if n <= 1 { return acc; }
    factorial_tail(n - 1, acc * n)
}

// Conversión a loop
fn factorial(n: u64) -> u64 {
    let mut n = n;
    let mut acc: u64 = 1;
    loop {
        if n <= 1 { return acc; }
        acc *= n;
        n -= 1;
    }
}
```

O de forma más idiomática con `while`:

```rust
fn factorial(mut n: u64) -> u64 {
    let mut acc: u64 = 1;
    while n > 1 {
        acc *= n;
        n -= 1;
    }
    acc
}
```

### Fibonacci

```rust
// Recursión de cola
fn fib_tail(n: u64, a: u64, b: u64) -> u64 {
    if n == 0 { return a; }
    fib_tail(n - 1, b, a + b)
}

// Conversión a loop
fn fib(n: u64) -> u64 {
    let mut n = n;
    let mut a: u64 = 0;
    let mut b: u64 = 1;
    loop {
        if n == 0 { return a; }
        let next_a = b;
        let next_b = a + b;
        a = next_a;
        b = next_b;
        n -= 1;
    }
}
```

Las variables temporales `next_a`, `next_b` son necesarias porque `a` y
`b` se usan en ambas asignaciones. Si se asignara `a = b` primero, el
valor original de `a` se perdería antes de calcular `a + b`. En Rust se
puede usar asignación por tupla para evitar temporales:

```rust
fn fib(mut n: u64) -> u64 {
    let (mut a, mut b) = (0u64, 1u64);
    while n > 0 {
        (a, b) = (b, a + b);
        n -= 1;
    }
    a
}
```

La asignación por tupla `(a, b) = (b, a + b)` evalúa el lado derecho
completo antes de asignar — equivale a un swap simultáneo.

### GCD

```rust
// Recursión de cola (directo)
fn gcd_rec(a: u64, b: u64) -> u64 {
    if b == 0 { return a; }
    gcd_rec(b, a % b)
}

// Loop (trivial)
fn gcd(mut a: u64, mut b: u64) -> u64 {
    while b != 0 {
        (a, b) = (b, a % b);
    }
    a
}
```

La conversión de GCD es tan natural que la versión con `loop` es la que
la mayoría de programadores escribiría directamente.

---

## 8. Limitaciones de TCO

### Lenguajes que garantizan TCO

| Lenguaje | TCO garantizada | Mecanismo |
|----------|:---------------:|-----------|
| Scheme | Sí (por especificación) | Obligatoria para implementaciones conformes |
| Haskell | Sí (efectivamente) | Evaluación perezosa + thunks |
| Erlang/Elixir | Sí | Diseñado para procesos de larga duración |
| Scala | Parcial (`@tailrec`) | Anotación que fuerza error si no es de cola |
| C (GCC/Clang) | No garantizada | Optimización opcional con `-O2` |
| Rust | No garantizada | LLVM puede aplicarla pero no se promete |
| Java | No | La JVM no lo soporta nativamente |
| Python | No | Decisión deliberada de Guido van Rossum |

### Por qué C y Rust no la garantizan

**C**: El estándar C no menciona TCO. Los compiladores pueden aplicarla
como optimización, pero el programador no puede depender de ella. Depende
del nivel de optimización, la complejidad de la función, y el compilador
específico.

**Rust**: Razones específicas:

1. **Drop semántica**: Si una variable local implementa `Drop`, su
   destructor debe ejecutarse al final del scope. Esto puede impedir que
   el compilador reutilice el frame:

   ```rust
   fn process(data: Vec<i32>, n: usize) -> i32 {
       if n == 0 { return data[0]; }
       let new_data = transform(&data);
       process(new_data, n - 1)
       // `data` must be dropped HERE, after the call returns
       // → cannot be tail-optimized
   }
   ```

2. **Backtraces**: Eliminar frames hace que `panic!` y herramientas de
   profiling muestren trazas incompletas. El equipo de Rust prioriza la
   depurabilidad.

3. **Propuesta `become`**: Existe una propuesta (RFC) para una keyword
   `become` que garantice TCO:

   ```rust
   fn factorial_tail(n: u64, acc: u64) -> u64 {
       if n <= 1 { return acc; }
       become factorial_tail(n - 1, acc * n);  // guaranteed TCO
   }
   ```

   Esta propuesta lleva años en discusión y no ha sido estabilizada.

### Consejo práctico

En C: si la recursión de cola es correcta y `-O2` la optimiza, es
aceptable usarla en código que siempre se compilará con optimizaciones
(producción). Pero para código que pueda compilarse con `-O0` (debug),
no confiar en TCO.

En Rust: **siempre convertir manualmente a `loop`**. La conversión es
trivial para recursión de cola, y el resultado es idiomático, predecible,
y no depende del compilador.

---

## 9. Ejercicios

---

### Ejercicio 1 — Identificar posición de cola

¿Cuáles de estas funciones tienen la llamada recursiva en posición de
cola?

```c
int f1(int n) { return n <= 0 ? 0 : 1 + f1(n - 1); }
int f2(int n, int acc) { return n <= 0 ? acc : f2(n - 1, acc + 1); }
int f3(int n) { if (n <= 1) return n; int x = f3(n-1); return x + f3(n-2); }
int f4(int a, int b) { return b == 0 ? a : f4(b, a % b); }
void f5(int n) { if (n > 0) { printf("%d", n); f5(n - 1); } }
```

<details>
<summary>¿Cuáles son de cola y cuáles no?</summary>

```
f1: NO de cola. "1 + f1(n-1)" — la suma ocurre DESPUÉS de que f1 retorne.
    El resultado de la llamada recursiva se modifica antes de retornarlo.

f2: SÍ de cola. "f2(n-1, acc+1)" es la última operación. acc+1 se computa
    ANTES de la llamada, no después.

f3: NO de cola. Hay DOS llamadas recursivas. "x + f3(n-2)" — la segunda
    llamada no está en posición de cola porque su resultado se suma con x.
    Tampoco la primera (f3(n-1)) porque su resultado se almacena para
    sumarlo después.

f4: SÍ de cola. "f4(b, a % b)" es la última operación. Es el algoritmo de
    Euclides, clásico ejemplo de recursión de cola.

f5: SÍ de cola. printf se ejecuta ANTES de la llamada recursiva.
    f5(n-1) es la última instrucción ejecutada. El void return es
    implícito después de la llamada.
```

De cola: **f2, f4, f5**. No de cola: **f1, f3**.

</details>

---

### Ejercicio 2 — Transformar a cola con acumulador

Transforma `f1` del ejercicio 1 (`1 + f1(n-1)`, que cuenta de $n$ a 0)
a recursión de cola con un acumulador. Luego conviértela a `loop` en Rust.

<details>
<summary>¿Cuál es el valor inicial del acumulador?</summary>

La función original cuenta cuántos pasos hay de $n$ a 0, es decir, retorna
$n$ (para $n \geq 0$).

```c
/* De cola */
int f1_tail(int n, int acc) {
    if (n <= 0) return acc;
    return f1_tail(n - 1, acc + 1);
}

int f1(int n) {
    return f1_tail(n, 0);  /* acc starts at 0: nothing counted yet */
}
```

El acumulador empieza en **0** (identidad de la suma) y acumula los `1 +`
que la versión original añadía en la fase de vuelta.

Conversión a `loop` en Rust:

```rust
fn f1(mut n: i32) -> i32 {
    let mut acc = 0;
    while n > 0 {
        acc += 1;
        n -= 1;
    }
    acc
}
```

Que es simplemente `n` — pero el ejercicio demuestra el patrón mecánico
de conversión.

</details>

---

### Ejercicio 3 — Verificar TCO con GCC

Compila el `factorial_tail` de la sección 4 con `-O0` y con `-O2`.
Para cada caso:

1. Ejecuta con $n = 1,000,000$.
2. En GDB, pon un breakpoint en `factorial_tail` con condición `n == 1`
   y revisa el backtrace.

<details>
<summary>¿Cuántos frames muestra cada backtrace?</summary>

```bash
# Sin TCO
gcc -g -O0 -o fact_O0 tail_factorial.c
gdb ./fact_O0
(gdb) break factorial_tail if n == 1
(gdb) run
# Stack overflow before reaching n==1 (1 million frames > 8MB)
```

Con `-O0` y $n = 1,000,000$: **stack overflow** antes de llegar al
breakpoint.

```bash
# Con TCO
gcc -g -O2 -o fact_O2 tail_factorial.c
gdb ./fact_O2
(gdb) break factorial_tail if n == 1
(gdb) run
(gdb) bt
```

Con `-O2`: el backtrace muestra solo **2-3 frames** (main + factorial +
posiblemente factorial_tail inlined). El compilador convirtió la recursión
en un bucle, así que solo hay un frame de `factorial_tail` reutilizado.

Nota: con `-O2 -g`, el debugger puede tener dificultades para poner
breakpoints en funciones optimizadas. Si el breakpoint no se activa,
el compilador probablemente inlineó la función completamente.

</details>

---

### Ejercicio 4 — Fibonacci de cola

Implementa `fib_tail` en Rust con dos acumuladores como en la sección 6.
Calcula `fib(90)` con la versión de cola y con la versión ingenua. ¿Cuál
termina?

<details>
<summary>¿Cuál es fib(90) y cuánto tarda cada versión?</summary>

```rust
fn fib_tail(n: u64, a: u64, b: u64) -> u64 {
    if n == 0 { return a; }
    fib_tail(n - 1, b, a + b)
}

fn fib_naive(n: u64) -> u64 {
    if n <= 1 { return n; }
    fib_naive(n - 1) + fib_naive(n - 2)
}

fn main() {
    // This finishes instantly
    println!("fib_tail(90) = {}", fib_tail(90, 0, 1));

    // This would take millions of years
    // println!("fib_naive(90) = {}", fib_naive(90));
}
```

$F(90) = 2,880,067,194,370,816,120$

- `fib_tail(90)`: **instantáneo** (~90 iteraciones).
- `fib_naive(90)`: **nunca termina**. El número de llamadas es
  $\approx \varphi^{90} \approx 4.6 \times 10^{18}$. A $10^9$ llamadas
  por segundo, tomaría ~146 años.

La transformación de recursión de árbol a recursión de cola con
acumuladores cambió la complejidad de $O(\varphi^n)$ a $O(n)$ — una
mejora exponencial, no solo de constante.

Nota: `fib_tail(93)` produciría overflow en `u64` ($F(93) > 2^{64}$).

</details>

---

### Ejercicio 5 — Loop idiomático

Convierte cada una de estas funciones recursivas de cola a un `loop` o
`while` en Rust:

```rust
fn power_tail(base: f64, exp: u32, acc: f64) -> f64 {
    if exp == 0 { return acc; }
    power_tail(base, exp - 1, acc * base)
}

fn sum_digits_tail(n: u64, acc: u64) -> u64 {
    if n == 0 { return acc; }
    sum_digits_tail(n / 10, acc + n % 10)
}

fn reverse_tail(s: &[u8], acc: Vec<u8>) -> Vec<u8> {
    if s.is_empty() { return acc; }
    let mut new_acc = acc;
    new_acc.push(s[s.len() - 1]);
    reverse_tail(&s[..s.len() - 1], new_acc)
}
```

<details>
<summary>¿Cuál requiere atención especial con el ownership del acumulador?</summary>

```rust
fn power(base: f64, mut exp: u32) -> f64 {
    let mut acc = 1.0;
    while exp > 0 {
        acc *= base;
        exp -= 1;
    }
    acc
}

fn sum_digits(mut n: u64) -> u64 {
    let mut acc = 0u64;
    while n > 0 {
        acc += n % 10;
        n /= 10;
    }
    acc
}

fn reverse(s: &[u8]) -> Vec<u8> {
    let mut acc = Vec::with_capacity(s.len());
    let mut remaining = s;
    while !remaining.is_empty() {
        acc.push(remaining[remaining.len() - 1]);
        remaining = &remaining[..remaining.len() - 1];
    }
    acc
}
```

`reverse_tail` requiere atención: en la versión recursiva, `new_acc`
se mueve (ownership transfer) a la siguiente llamada. En la versión con
`loop`, `acc` es `mut` y se modifica in-place — no hay transferencia de
ownership. Esto es más eficiente porque el `Vec` no se mueve entre frames.

Por cierto, la forma idiomática en Rust sería `s.iter().rev().copied().collect()`
— pero el ejercicio demuestra el patrón de conversión mecánica.

</details>

---

### Ejercicio 6 — Recursión no transformable

Explica por qué el recorrido inorder de un árbol binario **no se puede
transformar** a recursión de cola, ni siquiera con acumuladores.

```c
void inorder(Node *node) {
    if (node == NULL) return;
    inorder(node->left);
    printf("%d ", node->data);
    inorder(node->right);    /* this IS in tail position... */
}
```

<details>
<summary>¿La segunda llamada está en posición de cola? ¿Eso basta?</summary>

La segunda llamada `inorder(node->right)` **sí** está en posición de cola.
El compilador puede aplicar TCO a esa llamada. Pero la primera llamada
`inorder(node->left)` **no** está en posición de cola (hay un `printf`
y otra llamada después).

Esto significa que:
- La rama derecha se puede recorrer con $O(1)$ stack (TCO).
- La rama izquierda **sigue necesitando** stack frames.

El resultado: el uso de stack es $O(h)$ donde $h$ es la profundidad
máxima de ramas **izquierdas** — mejor que $O(h)$ total, pero no
eliminable.

No hay forma de poner **ambas** llamadas en posición de cola porque hay
trabajo entre ellas (`printf`). Para eliminiar toda recursión, se necesita
la pila explícita (como se vio en T03).

Los árboles son inherentemente no-transformables a recursión de cola
completa porque tienen **dos ramas** y trabajo intermedio. Solo las
estructuras lineales (listas, secuencias) permiten transformación a cola.

</details>

---

### Ejercicio 7 — @tailrec mental

Si Rust tuviera un atributo `#[tailrec]` como Scala, ¿cuáles de estas
funciones compilarían con él? (El atributo da error si la función no es
de cola.)

```rust
fn a(n: u64) -> u64 { if n == 0 { 1 } else { n * a(n-1) } }
fn b(n: u64, acc: u64) -> u64 { if n == 0 { acc } else { b(n-1, acc*n) } }
fn c(s: &str) -> usize { if s.is_empty() { 0 } else { 1 + c(&s[1..]) } }
fn d(v: &[i32], t: i32) -> bool {
    if v.is_empty() { return false; }
    if v[0] == t { return true; }
    d(&v[1..], t)
}
```

<details>
<summary>¿Cuáles son de cola y cuáles no?</summary>

```
a: NO. "n * a(n-1)" — multiplicación después de la llamada.
   #[tailrec] daría error de compilación.

b: SÍ. "b(n-1, acc*n)" — la llamada es la última operación.
   #[tailrec] compilaría correctamente.

c: NO. "1 + c(&s[1..])" — suma después de la llamada.
   #[tailrec] daría error. Necesita acumulador: c_tail(s, 0).

d: SÍ. "d(&v[1..], t)" — la llamada es la última operación.
   Los return tempranos (false, true) son casos base, no llamadas.
   #[tailrec] compilaría correctamente.
```

Compilarían con `#[tailrec]`: **b** y **d**.

En Scala real:

```scala
@tailrec
def factorial(n: Int, acc: Int = 1): Int = {
  if (n <= 1) acc
  else factorial(n - 1, acc * n)  // compiler verifies this is tail
}
```

Si la función no es de cola, el compilador de Scala emite un error en
lugar de generar recursión silenciosa con riesgo de stack overflow. Rust
no tiene este mecanismo — la responsabilidad recae en el programador.

</details>

---

### Ejercicio 8 — Drop y TCO

Explica por qué esta función en Rust probablemente **no** recibiría TCO
incluso con un compilador que lo soporte:

```rust
fn process(data: Vec<i32>, depth: usize) -> i32 {
    if depth == 0 {
        return data.iter().sum();
    }
    let filtered: Vec<i32> = data.into_iter().filter(|&x| x > 0).collect();
    process(filtered, depth - 1)
}
```

<details>
<summary>¿Qué destructor interfiere?</summary>

Aunque `process(filtered, depth - 1)` está en posición de cola
sintácticamente, hay un problema semántico: `data` se consume por
`into_iter()` dentro de la creación de `filtered`, así que **`data` ya no
existe** en el punto de la llamada recursiva.

En este caso concreto, el compilador **podría** aplicar TCO porque `data`
se movió (consumió) antes de la llamada. El `Vec` original ya no necesita
Drop.

Pero si el código fuera:

```rust
fn process(data: Vec<i32>, depth: usize) -> i32 {
    if depth == 0 {
        return data.iter().sum();
    }
    let filtered: Vec<i32> = data.iter().filter(|&&x| x > 0).copied().collect();
    process(filtered, depth - 1)
    // `data` must be DROPPED here — after the call returns
    // → prevents tail call optimization
}
```

Aquí `data.iter()` toma una referencia prestada, no consume `data`. El
`Vec` original sigue vivo y su destructor debe ejecutarse **después** de
que `process` retorne. El frame no se puede reutilizar porque contiene el
`data` pendiente de destrucción.

La diferencia sutil entre `.into_iter()` (consume, permite TCO) y
`.iter()` (presta, bloquea TCO) es un ejemplo de cómo el ownership de
Rust interactúa con las optimizaciones del compilador.

</details>

---

### Ejercicio 9 — Benchmark: recursión de cola vs loop

Implementa `sum(1..n)` de tres formas en Rust: recursión no-cola,
recursión de cola, y `loop`. Mide el tiempo para $n = 1,000,000$
compilando con `--release`.

<details>
<summary>¿Las tres versiones tienen el mismo rendimiento en release?</summary>

```rust
use std::time::Instant;

fn sum_no_tail(n: u64) -> u64 {
    if n == 0 { return 0; }
    n + sum_no_tail(n - 1)
}

fn sum_tail(n: u64, acc: u64) -> u64 {
    if n == 0 { return acc; }
    sum_tail(n - 1, acc + n)
}

fn sum_loop(n: u64) -> u64 {
    let mut acc = 0u64;
    let mut i = n;
    while i > 0 {
        acc += i;
        i -= 1;
    }
    acc
}
```

Resultados típicos con `--release` y $n = 1,000,000$:

| Versión | Tiempo |
|---------|:------:|
| sum_no_tail | ~3 ms (si LLVM aplica TCO) o stack overflow (si no) |
| sum_tail | ~1 ms |
| sum_loop | ~1 ms |

Con `--release`, LLVM **suele** optimizar ambas versiones recursivas a
bucles, dando tiempos similares. Pero:

- `sum_no_tail` en debug: **stack overflow** (1M frames).
- `sum_tail` en debug: **stack overflow** (1M frames — TCO no aplica en
  debug).
- `sum_loop` en debug: **funciona** (~5 ms, más lento sin optimizaciones
  pero sin riesgo de overflow).

La conclusión: `loop` es el único que funciona **en todos los modos de
compilación**. Las versiones recursivas dependen de optimizaciones que
no están garantizadas.

</details>
