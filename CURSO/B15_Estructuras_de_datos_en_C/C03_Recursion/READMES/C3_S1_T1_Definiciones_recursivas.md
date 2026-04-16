# T01 — Definiciones recursivas

## Índice

1. [Qué es la recursión](#1-qué-es-la-recursión)
2. [Caso base y caso recursivo](#2-caso-base-y-caso-recursivo)
3. [Conexión con la inducción matemática](#3-conexión-con-la-inducción-matemática)
4. [Factorial](#4-factorial)
5. [Fibonacci](#5-fibonacci)
6. [Cómo la máquina ejecuta recursión](#6-cómo-la-máquina-ejecuta-recursión)
7. [Stack frames en detalle](#7-stack-frames-en-detalle)
8. [Errores comunes](#8-errores-comunes)
9. [Más ejemplos de definiciones recursivas](#9-más-ejemplos-de-definiciones-recursivas)
10. [Ejercicios](#10-ejercicios)

---

## 1. Qué es la recursión

Una función es **recursiva** cuando se llama a sí misma, directa o
indirectamente, para resolver un sub-problema de menor tamaño.

La idea central: si un problema de tamaño $n$ se puede expresar en
términos del **mismo problema** de tamaño menor, entonces la función
puede llamarse a sí misma con la entrada reducida hasta llegar a un caso
trivial.

```
Para resolver problema(n):
    Si n es trivial:
        Retornar solución directa         ← caso base
    Si no:
        Retornar combinar(
            problema(sub-problema),       ← llamada recursiva
            parte_restante
        )
```

### Recursión vs repetición

Un bucle repite la misma operación cambiando un estado (variable de
control). La recursión repite la misma operación **creando una nueva
instancia** de la función con parámetros diferentes. Cada instancia tiene
sus propias variables locales, almacenadas en su propio **stack frame** en
la pila de llamadas.

---

## 2. Caso base y caso recursivo

Toda definición recursiva correcta tiene dos componentes:

### Caso base

La condición de terminación. Resuelve el problema directamente, sin
recursión. Sin caso base, la recursión sería infinita.

### Caso recursivo

Reduce el problema a una instancia **más pequeña** y llama a la función
con esa instancia. La reducción debe garantizar que eventualmente se
alcance el caso base.

### Analogía

Imagina una escalera descendente:
- **Caso recursivo**: "para bajar $n$ escalones, baja uno y luego baja
  $n-1$ escalones."
- **Caso base**: "si estás en el suelo ($n = 0$), detente."

Sin el caso base, nunca dejarías de bajar (stack overflow). Sin la
reducción (si dijeras "baja $n$ escalones" en lugar de $n-1$), tampoco
terminarías (recursión infinita sin progreso).

### Requisitos formales

Una función recursiva $f$ es correcta si:

1. **Existe al menos un caso base** donde $f$ retorna sin llamarse.
2. **Cada llamada recursiva reduce el problema** hacia el caso base.
3. **La combinación de los resultados parciales produce el resultado
   correcto** del problema original.

---

## 3. Conexión con la inducción matemática

La recursión es la contraparte computacional de la **inducción matemática**.
La estructura es idéntica:

| Inducción | Recursión |
|-----------|-----------|
| Caso base: probar $P(0)$ | Caso base: resolver $f(0)$ directamente |
| Paso inductivo: si $P(k)$ entonces $P(k+1)$ | Caso recursivo: $f(n)$ usa $f(n-1)$ |
| Conclusión: $P(n)$ vale $\forall n \geq 0$ | Conclusión: $f(n)$ termina $\forall n \geq 0$ |

### Ejemplo con suma

Propiedad: la suma de los primeros $n$ naturales es $\frac{n(n+1)}{2}$.

**Inducción**:
- Base: $P(0)$: suma $= 0 = \frac{0 \cdot 1}{2}$ ✓
- Paso: si $\sum_{i=1}^{k} i = \frac{k(k+1)}{2}$, entonces
  $\sum_{i=1}^{k+1} i = \frac{k(k+1)}{2} + (k+1) = \frac{(k+1)(k+2)}{2}$ ✓

**Recursión**:

```c
int sum(int n) {
    if (n == 0) return 0;               /* base */
    return n + sum(n - 1);              /* f(n) = n + f(n-1) */
}
```

La función `sum(n)` computa exactamente lo que la inducción prueba: si
`sum(n-1)` es correcto (hipótesis inductiva), entonces `n + sum(n-1)` es
correcto (paso inductivo).

---

## 4. Factorial

El factorial es el ejemplo canónico de recursión:

$$n! = \begin{cases} 1 & \text{si } n = 0 \\ n \times (n-1)! & \text{si } n > 0 \end{cases}$$

### Implementación en C

```c
/* factorial.c */
#include <stdio.h>

unsigned long factorial(int n) {
    if (n <= 1) return 1;           /* base case */
    return n * factorial(n - 1);    /* recursive case */
}

int main(void) {
    for (int i = 0; i <= 12; i++) {
        printf("%2d! = %lu\n", i, factorial(i));
    }
    return 0;
}
```

### Implementación en Rust

```rust
fn factorial(n: u64) -> u64 {
    if n <= 1 { return 1; }         // base case
    n * factorial(n - 1)            // recursive case
}

fn main() {
    for i in 0..=20 {
        println!("{:>2}! = {}", i, factorial(i));
    }
}
```

### Traza de ejecución: `factorial(4)`

```
factorial(4)
│ return 4 * factorial(3)
│              │ return 3 * factorial(2)
│              │              │ return 2 * factorial(1)
│              │              │              │ return 1  ← base
│              │              │ return 2 * 1 = 2
│              │ return 3 * 2 = 6
│ return 4 * 6 = 24
```

La recursión **desciende** hasta el caso base y luego los resultados
**ascienden** de vuelta, multiplicándose en cada nivel. Esto se llama
la fase de **ida** (descenso) y la fase de **vuelta** (combinación).

### Límite práctico

| Tipo | Máximo $n$ | Razón |
|------|:----------:|-------|
| `unsigned long` (C, 64 bits) | 20 | $21! > 2^{64}$ (overflow) |
| `u64` (Rust) | 20 | Mismo rango |
| `u128` (Rust) | 34 | $35! > 2^{128}$ |

Rust con `u64` en modo debug detecta overflow con panic. C con
`unsigned long` produce truncamiento silencioso (wrap around).

---

## 5. Fibonacci

La secuencia de Fibonacci se define recursivamente:

$$F(n) = \begin{cases} 0 & \text{si } n = 0 \\ 1 & \text{si } n = 1 \\ F(n-1) + F(n-2) & \text{si } n > 1 \end{cases}$$

Secuencia: 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, ...

### Implementación directa (ineficiente)

```c
unsigned long fib(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fib(n - 1) + fib(n - 2);
}
```

```rust
fn fib(n: u32) -> u64 {
    match n {
        0 => 0,
        1 => 1,
        _ => fib(n - 1) + fib(n - 2),
    }
}
```

### Árbol de llamadas: `fib(5)`

```
                       fib(5)
                      /      \
                 fib(4)       fib(3)
                /     \       /     \
           fib(3)   fib(2)  fib(2)  fib(1)
           /   \    /   \   /   \      |
       fib(2) fib(1) f(1) f(0) f(1) f(0)  1
       /   \    |    |    |    |    |
     f(1) f(0)  1    1    0    1    0
      |    |
      1    0
```

### El problema: crecimiento exponencial

Para calcular `fib(5)` se realizan **15 llamadas**. El número de llamadas
crece exponencialmente:

| $n$ | Llamadas | $F(n)$ |
|:---:|:--------:|:------:|
| 5 | 15 | 5 |
| 10 | 177 | 55 |
| 20 | 21,891 | 6,765 |
| 30 | 2,692,537 | 832,040 |
| 40 | ~331 millones | 102,334,155 |

El número de llamadas es aproximadamente $O(\varphi^n)$ donde
$\varphi = \frac{1+\sqrt{5}}{2} \approx 1.618$ (proporción áurea). Esto
es catastrófico: `fib(40)` tarda varios segundos, `fib(50)` varios
minutos.

La causa es la **re-computación**: `fib(3)` se calcula 2 veces, `fib(2)` se
calcula 3 veces, etc. Cada sub-problema se resuelve desde cero cada vez que
se necesita.

### Versión iterativa (eficiente)

```c
unsigned long fib_iter(int n) {
    if (n <= 0) return 0;
    unsigned long prev = 0, curr = 1;
    for (int i = 2; i <= n; i++) {
        unsigned long next = prev + curr;
        prev = curr;
        curr = next;
    }
    return curr;
}
```

```rust
fn fib_iter(n: u32) -> u64 {
    if n == 0 { return 0; }
    let (mut prev, mut curr) = (0u64, 1u64);
    for _ in 2..=n {
        let next = prev + curr;
        prev = curr;
        curr = next;
    }
    curr
}
```

La versión iterativa es $O(n)$ en tiempo y $O(1)$ en espacio. La
recursiva es $O(\varphi^n)$ en tiempo y $O(n)$ en espacio (profundidad
de la pila). La diferencia es abismal.

### Lección

Que un problema tenga una definición recursiva elegante **no implica** que
la implementación recursiva directa sea eficiente. Fibonacci es el ejemplo
clásico de recursión ineficiente. Las soluciones son:

1. **Iteración** (como arriba).
2. **Memoización** (cachear resultados ya calculados).
3. **Programación dinámica** (llenar una tabla bottom-up).

Estas técnicas se explorarán en contextos futuros. Por ahora, el punto
es que la recursión debe evaluarse tanto por **corrección** (¿produce el
resultado correcto?) como por **eficiencia** (¿en cuánto tiempo?).

---

## 6. Cómo la máquina ejecuta recursión

Cuando una función se llama a sí misma, la máquina no tiene noción
especial de "recursión". Para el procesador, es simplemente una función
que llama a otra función — que resulta ser la misma. El mecanismo que lo
hace posible es la **pila de llamadas** (call stack).

### La pila de llamadas

Cada programa tiene una región de memoria llamada **stack** que crece
hacia direcciones bajas. Cada llamada a función crea un **stack frame**
(marco de pila) que contiene:

1. **Dirección de retorno**: a dónde volver cuando la función termine.
2. **Parámetros**: valores pasados a la función.
3. **Variables locales**: variables declaradas dentro de la función.
4. **Frame pointer** (registros salvados): para restaurar el contexto.

```
Memoria (direcciones altas arriba):

┌──────────────────────┐
│    main()            │  frame de main
│    n = 4             │
├──────────────────────┤
│    factorial(4)      │  frame de factorial(4)
│    n = 4             │
│    return addr → main│
├──────────────────────┤
│    factorial(3)      │  frame de factorial(3)
│    n = 3             │
│    return addr → f(4)│
├──────────────────────┤
│    factorial(2)      │  frame de factorial(2)
│    n = 2             │
│    return addr → f(3)│
├──────────────────────┤
│    factorial(1)      │  frame de factorial(1)  ← tope
│    n = 1             │
│    return addr → f(2)│
└──────────────────────┘
         ↓ crece hacia abajo
```

### Conexión con C02

En C02 estudiamos la pila como estructura de datos. Ahora vemos que el
sistema operativo y el procesador usan una pila real (en memoria) para
gestionar las llamadas a función. La recursión funciona porque cada
llamada push un frame nuevo y cada retorno pop ese frame.

| Pila (estructura de datos) | Pila de llamadas |
|:-------------------------:|:----------------:|
| push(elemento) | Llamar a función (crear frame) |
| pop() | Retornar de función (destruir frame) |
| peek() | Acceder a variables del frame actual |
| LIFO | La última función llamada es la primera en retornar |
| Stack overflow (capacidad) | Stack overflow (profundidad de recursión) |

---

## 7. Stack frames en detalle

### En C

Cada stack frame de `factorial(n)` contiene:

```
┌─────────────────────────────┐
│  Saved RBP (frame pointer)  │  8 bytes
├─────────────────────────────┤
│  Return address             │  8 bytes
├─────────────────────────────┤
│  Parameter: n               │  4 bytes (int)
├─────────────────────────────┤
│  (padding/alignment)        │  variable
└─────────────────────────────┘
  Total: ~32 bytes típicos (con alignment)
```

El tamaño exacto depende del compilador, la arquitectura, y el nivel de
optimización. Un frame de `factorial` es pequeño (~32 bytes), pero para
una función con muchas variables locales o arrays, puede ser mucho mayor.

### Tamaño del stack

| Sistema | Tamaño por defecto |
|---------|--------------------|
| Linux | 8 MB (`ulimit -s`) |
| macOS | 8 MB |
| Windows | 1 MB |
| Thread secundario (típico) | 2 MB |

Con frames de ~32 bytes y un stack de 8 MB, se pueden anidar
aproximadamente 250,000 llamadas recursivas. Pero una función con un array
local de 4 KB permite solo ~2,000 niveles antes de stack overflow.

### En Rust

Rust usa el mismo mecanismo de stack frames que C (ambos compilan a código
nativo). El tamaño del stack por defecto del thread principal es
generalmente 8 MB, y los threads creados con `std::thread::spawn` tienen
un stack de 2 MB (configurable con `Builder::stack_size`).

```rust
// thread with 16 MB stack
std::thread::Builder::new()
    .stack_size(16 * 1024 * 1024)
    .spawn(|| {
        // deep recursion here
    })
    .unwrap()
    .join()
    .unwrap();
```

---

## 8. Errores comunes

### 8.1 Sin caso base

```c
int bad_factorial(int n) {
    return n * bad_factorial(n - 1);  /* never stops */
}
```

La función se llama infinitamente hasta que la pila se agota →
**stack overflow** (segmentation fault en C, stack overflow panic en Rust
en modo debug).

### 8.2 Caso base inalcanzable

```c
int bad_fib(int n) {
    if (n == 1) return 1;              /* base case */
    return bad_fib(n - 1) + bad_fib(n - 2);
}
```

Si se llama con `bad_fib(0)`: $0 \neq 1$ → llama `bad_fib(-1)` y
`bad_fib(-2)` → los valores nunca llegan a 1 → recursión infinita.

El caso base debe cubrir **todos los valores que pueden alcanzarse** por
reducción. Aquí falta `if (n <= 0) return 0`.

### 8.3 Sin reducción

```c
int bad_sum(int n) {
    if (n == 0) return 0;
    return 1 + bad_sum(n);  /* should be n-1, not n */
}
```

`bad_sum(5)` llama `bad_sum(5)` llama `bad_sum(5)`... El parámetro no
se reduce → nunca alcanza el caso base.

### 8.4 Confundir fase de ida y vuelta

Un error conceptual común: pensar que la recursión ejecuta todo "hacia
abajo" y no entender que los resultados se combinan "hacia arriba":

```c
void confused_print(int n) {
    if (n == 0) return;
    printf("%d ", n);              /* before recursive call */
    confused_print(n - 1);
    printf("%d ", n);              /* after recursive call */
}
```

`confused_print(3)` imprime: `3 2 1 1 2 3`. La primera mitad se imprime
en la fase de ida (descendente); la segunda mitad en la fase de vuelta
(ascendente). Esto es correcto pero a menudo sorprende a principiantes.

### 8.5 Variables locales vs estado compartido

Cada llamada recursiva tiene sus **propias variables locales**. Un error
es tratar de usar una variable global para acumular resultados:

```c
int total = 0;  /* global — shared across all calls */

void bad_sum_global(int n) {
    if (n == 0) return;
    total += n;
    bad_sum_global(n - 1);
}
```

Esto funciona para una llamada, pero si se llama dos veces sin resetear
`total`, los resultados se acumulan incorrectamente. La versión recursiva
pura retorna el resultado sin estado compartido.

---

## 9. Más ejemplos de definiciones recursivas

### Suma de dígitos

$$S(n) = \begin{cases} n & \text{si } n < 10 \\ (n \bmod 10) + S(n / 10) & \text{si } n \geq 10 \end{cases}$$

```c
int digit_sum(int n) {
    if (n < 10) return n;
    return (n % 10) + digit_sum(n / 10);
}
/* digit_sum(1234) = 4 + digit_sum(123) = 4 + 3 + digit_sum(12)
                   = 4 + 3 + 2 + digit_sum(1) = 4 + 3 + 2 + 1 = 10 */
```

### Potencia

$$a^n = \begin{cases} 1 & \text{si } n = 0 \\ a \times a^{n-1} & \text{si } n > 0 \end{cases}$$

```rust
fn power(a: f64, n: u32) -> f64 {
    if n == 0 { return 1.0; }
    a * power(a, n - 1)
}
```

Existe una versión más eficiente (**exponenciación rápida**) que reduce la
complejidad de $O(n)$ a $O(\log n)$:

$$a^n = \begin{cases} 1 & \text{si } n = 0 \\ (a^{n/2})^2 & \text{si } n \text{ es par} \\ a \times a^{n-1} & \text{si } n \text{ es impar} \end{cases}$$

```rust
fn fast_power(a: f64, n: u32) -> f64 {
    if n == 0 { return 1.0; }
    if n % 2 == 0 {
        let half = fast_power(a, n / 2);
        half * half
    } else {
        a * fast_power(a, n - 1)
    }
}
```

### MCD (Máximo Común Divisor) — Algoritmo de Euclides

$$\gcd(a, b) = \begin{cases} a & \text{si } b = 0 \\ \gcd(b, a \bmod b) & \text{si } b > 0 \end{cases}$$

```c
int gcd(int a, int b) {
    if (b == 0) return a;
    return gcd(b, a % b);
}
/* gcd(48, 18) → gcd(18, 12) → gcd(12, 6) → gcd(6, 0) → 6 */
```

Este es un ejemplo donde la recursión es extremadamente natural: la
definición matemática y el código son prácticamente idénticos.

---

## 10. Ejercicios

---

### Ejercicio 1 — Identificar componentes

Para cada función, identifica: (a) el caso base, (b) el caso recursivo,
(c) la reducción, y (d) si la función es correcta:

```c
/* Función A */
int mystery_a(int n) {
    if (n == 0) return 0;
    return n + mystery_a(n - 2);
}

/* Función B */
int mystery_b(int n) {
    if (n <= 1) return n;
    return mystery_b(n - 1) + mystery_b(n - 2);
}

/* Función C */
int mystery_c(int n) {
    return 1 + mystery_c(n - 1);
}
```

<details>
<summary>¿Cuál de las tres funciones tiene un error fatal?</summary>

**Función A**:
- Base: `n == 0` → retorna 0
- Recursivo: `n + mystery_a(n - 2)` (reduce en 2)
- Reducción: $n \to n-2$
- **Incorrecta para $n$ impar**: `mystery_a(3)` → `3 + mystery_a(1)` →
  `3 + 1 + mystery_a(-1)` → recursión infinita (nunca llega a 0). Falta
  caso base para `n == 1` o usar `n <= 0`.

**Función B**:
- Base: `n <= 1` → retorna `n`
- Recursivo: `mystery_b(n-1) + mystery_b(n-2)` (Fibonacci)
- Reducción: $n \to n-1$ y $n \to n-2$
- **Correcta**: el caso base cubre 0 y 1, y la reducción garantiza
  convergencia para todo $n \geq 0$.

**Función C**:
- Base: **no tiene** ← error fatal
- Recursivo: `1 + mystery_c(n - 1)`
- **Fatalmente incorrecta**: stack overflow para cualquier entrada. Falta
  el caso base (por ejemplo `if (n == 0) return 0`).

</details>

---

### Ejercicio 2 — Traza de factorial

Dibuja la traza completa de `factorial(5)`, mostrando:
- Cada llamada con su parámetro
- La fase de ida (descenso)
- El valor retornado en cada nivel (fase de vuelta)

<details>
<summary>¿Cuántas multiplicaciones se realizan en total?</summary>

```
factorial(5)    → 5 * factorial(4)          pendiente: 5 * ?
  factorial(4)  → 4 * factorial(3)          pendiente: 4 * ?
    factorial(3) → 3 * factorial(2)         pendiente: 3 * ?
      factorial(2) → 2 * factorial(1)       pendiente: 2 * ?
        factorial(1) → return 1             ← base (0 multiplicaciones)

      factorial(2) → return 2 * 1 = 2      ← multiplicación 1
    factorial(3) → return 3 * 2 = 6         ← multiplicación 2
  factorial(4) → return 4 * 6 = 24          ← multiplicación 3
factorial(5) → return 5 * 24 = 120          ← multiplicación 4
```

Se realizan **4 multiplicaciones** para `factorial(5)`. En general,
`factorial(n)` realiza $n - 1$ multiplicaciones (una por cada nivel
excepto el caso base).

La profundidad máxima de la pila es 5 (5 frames simultáneos). Cada frame
se destruye al retornar, así que el espacio total es $O(n)$.

</details>

---

### Ejercicio 3 — Conteo de llamadas en Fibonacci

Sin ejecutar el código, calcula cuántas veces se llama `fib(n)` para
$n = 6$. Dibuja el árbol de llamadas.

<details>
<summary>¿Cuántas llamadas totales hay? ¿Cuántas son a fib(1) y fib(0)?</summary>

```
                           fib(6)
                        /          \
                   fib(5)           fib(4)
                  /     \          /     \
              fib(4)   fib(3)   fib(3)  fib(2)
              /   \    /   \    /   \    /   \
          fib(3) f(2) f(2) f(1) f(2) f(1) f(1) f(0)
          /   \  / \  / \       / \
       f(2) f(1) f(1) f(0) f(1) f(0) f(1) f(0)
       / \
    f(1) f(0)
```

Conteo por nivel:

| Función | Veces llamada |
|:-------:|:-------------:|
| fib(6) | 1 |
| fib(5) | 1 |
| fib(4) | 2 |
| fib(3) | 3 |
| fib(2) | 5 |
| fib(1) | 8 |
| fib(0) | 5 |
| **Total** | **25** |

Las llamadas a `fib(1)` y `fib(0)` suman **13** — más de la mitad del
total. Observa que el número de llamadas a `fib(k)` es $F(n-k+1)$
(otro número de Fibonacci). El total de llamadas para `fib(n)` es
$2 \times F(n+1) - 1$.

</details>

---

### Ejercicio 4 — Suma recursiva de array

Implementa en C y Rust una función que calcule la suma de un array de
enteros recursivamente. El caso base es un array de longitud 0. El caso
recursivo suma el primer elemento más la suma del resto.

<details>
<summary>¿Cómo pasas "el resto del array" en C y en Rust?</summary>

**En C** — aritmética de punteros:

```c
int array_sum(const int *arr, int len) {
    if (len == 0) return 0;
    return arr[0] + array_sum(arr + 1, len - 1);
}
/* array_sum({1,2,3,4}, 4) = 1 + array_sum({2,3,4}, 3)
                            = 1 + 2 + array_sum({3,4}, 2)
                            = 1 + 2 + 3 + array_sum({4}, 1)
                            = 1 + 2 + 3 + 4 + array_sum({}, 0)
                            = 1 + 2 + 3 + 4 + 0 = 10 */
```

`arr + 1` avanza el puntero un elemento; `len - 1` reduce el tamaño.

**En Rust** — slices:

```rust
fn array_sum(arr: &[i32]) -> i32 {
    if arr.is_empty() { return 0; }
    arr[0] + array_sum(&arr[1..])
}
```

`&arr[1..]` crea un sub-slice que empieza en el segundo elemento. En Rust,
el slice ya lleva su longitud, así que no se necesita un parámetro `len`
separado.

Ambas versiones son $O(n)$ en tiempo y espacio (pila). Para arrays
grandes ($n > 100,000$), la versión iterativa es preferible para evitar
stack overflow.

</details>

---

### Ejercicio 5 — Potencia con traza

Implementa `power(base, exp)` en Rust. Añade `println!` al inicio de la
función para imprimir los parámetros. Ejecuta `power(2, 10)` y observa
la traza. Luego implementa `fast_power` y compara el número de llamadas.

<details>
<summary>¿Cuántas llamadas hace cada versión para $2^{10}$?</summary>

**`power(2, 10)`** — versión lineal:

```
power(2, 10) → power(2, 9) → power(2, 8) → ... → power(2, 0)
```

**11 llamadas** (de $n=10$ a $n=0$, inclusive). Complejidad: $O(n)$.

**`fast_power(2, 10)`** — versión logarítmica:

```
fast_power(2, 10) → 10 par → fast_power(2, 5)
  fast_power(2, 5) → 5 impar → fast_power(2, 4)
    fast_power(2, 4) → 4 par → fast_power(2, 2)
      fast_power(2, 2) → 2 par → fast_power(2, 1)
        fast_power(2, 1) → 1 impar → fast_power(2, 0)
          fast_power(2, 0) → return 1
```

**6 llamadas**. Complejidad: $O(\log n)$. Para $n = 1000$, la versión
lineal hace 1001 llamadas vs ~20 para la logarítmica.

</details>

---

### Ejercicio 6 — Stack frames de GCD

El algoritmo de Euclides `gcd(48, 18)` produce la secuencia:
`gcd(48,18) → gcd(18,12) → gcd(12,6) → gcd(6,0)`.

Dibuja los 4 stack frames simultáneos en el punto de máxima profundidad,
indicando para cada uno: parámetros `a` y `b`, y dirección de retorno.

<details>
<summary>¿Cuál es la profundidad máxima y cuántos bytes de stack consume?</summary>

```
┌────────────────────────────┐
│ main()                     │
│   calls gcd(48, 18)        │
├────────────────────────────┤
│ gcd(a=48, b=18)            │
│   return addr → main       │
│   18 ≠ 0 → gcd(18, 12)    │
├────────────────────────────┤
│ gcd(a=18, b=12)            │
│   return addr → gcd(48,18) │
│   12 ≠ 0 → gcd(12, 6)     │
├────────────────────────────┤
│ gcd(a=12, b=6)             │
│   return addr → gcd(18,12) │
│   6 ≠ 0 → gcd(6, 0)       │
├────────────────────────────┤
│ gcd(a=6, b=0)              │  ← máxima profundidad
│   return addr → gcd(12,6)  │
│   b == 0 → return 6        │
└────────────────────────────┘
```

Profundidad máxima: **5 frames** (main + 4 llamadas a gcd). Con ~32 bytes
por frame, consume ~160 bytes de stack. El algoritmo de Euclides es muy
eficiente en profundidad: para entradas de hasta $2^{64}$, nunca supera
~93 niveles (proporcional a $\log_\varphi(n)$).

</details>

---

### Ejercicio 7 — Verificar recursión correcta

Para cada función, predice si termina para la entrada dada y qué retorna:

```rust
fn f1(n: i32) -> i32 {
    if n <= 0 { return 0; }
    f1(n - 3) + 1
}

fn f2(n: i32) -> i32 {
    if n == 1 { return 0; }
    if n % 2 == 0 { f2(n / 2) + 1 }
    else { f2(3 * n + 1) + 1 }
}
```

Llamadas: `f1(7)`, `f1(8)`, `f2(6)`, `f2(27)`.

<details>
<summary>¿Todas terminan? ¿Cuál tiene terminación no demostrada?</summary>

**f1(7)**: 7→4→1→-2→ base (≤0) → retorna 0+1+1+1 = **3**. Termina.

**f1(8)**: 8→5→2→-1→ base (≤0) → retorna 0+1+1+1 = **3**. Termina.

**f2(6)**: 6→3→10→5→16→8→4→2→1 → base → retorna **8** (8 pasos). Termina.

**f2(27)**: Esta es la **conjetura de Collatz**. Para $n=27$, la secuencia
alcanza un máximo de 9232 antes de descender a 1 después de 111 pasos.
Retorna **111**. Termina (empíricamente).

La función `f2` implementa la conjetura de Collatz: "para todo entero
positivo $n$, la secuencia eventualmente llega a 1." Esta conjetura está
**sin demostrar** desde 1937. No se puede probar que `f2` termina para
todo $n$, aunque nunca se ha encontrado un contraejemplo.

</details>

---

### Ejercicio 8 — Fase de ida vs vuelta

¿Qué imprime cada función para `mystery(4)`?

```c
void mystery_down(int n) {
    if (n == 0) return;
    printf("%d ", n);
    mystery_down(n - 1);
}

void mystery_up(int n) {
    if (n == 0) return;
    mystery_up(n - 1);
    printf("%d ", n);
}

void mystery_both(int n) {
    if (n == 0) return;
    printf("(%d ", n);
    mystery_both(n - 1);
    printf("%d) ", n);
}
```

<details>
<summary>¿Cuál imprime en orden ascendente y cuál en descendente?</summary>

**`mystery_down(4)`**: imprime **antes** de la llamada recursiva (fase de ida).
```
4 3 2 1
```
Orden descendente: cada nivel imprime su `n` antes de recurrir con `n-1`.

**`mystery_up(4)`**: imprime **después** de la llamada recursiva (fase de vuelta).
```
1 2 3 4
```
Orden ascendente: la función desciende hasta `n=1`, y al retornar de cada
nivel imprime su `n`.

**`mystery_both(4)`**: imprime **antes y después** de la llamada recursiva.
```
(4 (3 (2 (1 1) 2) 3) 4)
```
Produce un patrón de paréntesis anidados. La primera mitad es la ida
(descendente), la segunda mitad es la vuelta (ascendente).

Este ejercicio demuestra que el código antes de la llamada recursiva se
ejecuta en orden descendente y el código después se ejecuta en orden
ascendente. Es el mismo principio que push (ida) y pop (vuelta) en una pila.

</details>

---

### Ejercicio 9 — Recursión sobre strings

Implementa en Rust una función recursiva `reverse(s: &str) -> String` que
invierta una cadena. Caso base: cadena vacía o de un carácter. Caso
recursivo: el último carácter seguido de la inversión del resto.

<details>
<summary>¿Es eficiente esta implementación? ¿Cuál es su complejidad?</summary>

```rust
fn reverse(s: &str) -> String {
    if s.len() <= 1 {
        return s.to_string();
    }
    let last = &s[s.len()-1..];
    let rest = &s[..s.len()-1];
    format!("{}{}", last, reverse(rest))
}
```

**No es eficiente**. Cada llamada crea un nuevo `String` con `format!`,
que copia todos los caracteres. La complejidad es:

- Profundidad: $O(n)$ llamadas
- Cada nivel crea un string de tamaño creciente
- Total de caracteres copiados: $1 + 2 + 3 + \ldots + n = O(n^2)$

La versión iterativa con `s.chars().rev().collect::<String>()` es $O(n)$.

Nota: esta implementación asume ASCII. Para UTF-8 general, operar byte por
byte con `s[..s.len()-1]` puede cortar un carácter multibyte por la mitad.
La versión correcta para UTF-8 usaría `s.chars()`.

La recursión aquí es didáctica, no práctica.

</details>

---

### Ejercicio 10 — Recursión y pila explícita

Reimplementa `factorial` usando una **pila explícita** (sin llamada
recursiva, pero simulando los stack frames con un `Vec` en Rust o un
array en C). Compara la estructura del código con la versión recursiva.

<details>
<summary>¿La pila explícita almacena los mismos datos que los stack frames reales?</summary>

```rust
fn factorial_stack(n: u64) -> u64 {
    let mut stack: Vec<u64> = Vec::new();

    // Phase 1: "descent" — push pending multiplications
    let mut current = n;
    while current > 1 {
        stack.push(current);
        current -= 1;
    }

    // Phase 2: "ascent" — pop and multiply
    let mut result: u64 = 1;
    while let Some(val) = stack.pop() {
        result *= val;
    }

    result
}
```

La pila explícita almacena **menos** que los stack frames reales. Los
frames reales almacenan dirección de retorno, frame pointer, y todas las
variables locales. La pila explícita solo almacena el dato relevante (el
valor de `n` en cada nivel).

Para factorial, la pila explícita es innecesaria (la versión iterativa
simple `for i in 2..=n { result *= i }` es mejor). Pero para problemas
con estructura de árbol (recorridos, backtracking), la pila explícita es
la forma estándar de convertir recursión a iteración, como se verá en T03.

Comparación:

| Aspecto | Recursiva | Pila explícita | Iterativa pura |
|---------|:---------:|:--------------:|:--------------:|
| Claridad | Alta | Media | Alta |
| Espacio | $O(n)$ stack | $O(n)$ heap | $O(1)$ |
| Overhead | Frames completos | Solo datos | Ninguno |
| Generalizable | Sí | Sí | Solo para recursión lineal |

</details>
