# T02 — Pila de llamadas

## Índice

1. [La pila de llamadas en la práctica](#1-la-pila-de-llamadas-en-la-práctica)
2. [Visualización con GDB](#2-visualización-con-gdb)
3. [Stack overflow](#3-stack-overflow)
4. [ulimit y configuración del stack](#4-ulimit-y-configuración-del-stack)
5. [Stack overflow en Rust](#5-stack-overflow-en-rust)
6. [Layout de memoria de un proceso](#6-layout-de-memoria-de-un-proceso)
7. [Relación con la pila como estructura de datos](#7-relación-con-la-pila-como-estructura-de-datos)
8. [Herramientas de diagnóstico](#8-herramientas-de-diagnóstico)
9. [Ejercicios](#9-ejercicios)

---

## 1. La pila de llamadas en la práctica

En T01 vimos que cada llamada a función crea un **stack frame**. Ahora
exploramos cómo observar, medir, y controlar este mecanismo en un
sistema real.

### Registros del procesador involucrados (x86-64)

| Registro | Nombre | Función |
|:--------:|--------|---------|
| `RSP` | Stack Pointer | Apunta al tope actual del stack |
| `RBP` | Base Pointer | Apunta a la base del frame actual |
| `RIP` | Instruction Pointer | Dirección de la instrucción actual |

Cuando se llama a una función:

1. Se empuja la **dirección de retorno** (valor actual de `RIP`) al stack.
2. Se empuja el **RBP** antiguo (para restaurarlo al retornar).
3. `RBP` se actualiza al nuevo tope (`RSP`).
4. `RSP` se decrementa para reservar espacio para variables locales.

Cuando la función retorna:

1. `RSP` se restaura a `RBP` (descarta variables locales).
2. Se hace pop de `RBP` (restaura el frame anterior).
3. Se hace pop de la dirección de retorno y se salta a ella.

### Programa de ejemplo

```c
/* callstack_demo.c */
#include <stdio.h>

int multiply(int a, int b) {
    int result = a * b;     /* variable local */
    return result;           /* breakpoint here */
}

int add_and_multiply(int x, int y, int z) {
    int sum = x + y;
    return multiply(sum, z);
}

int main(void) {
    int answer = add_and_multiply(3, 4, 5);
    printf("Answer: %d\n", answer);
    return 0;
}
```

Compilar con información de debug:

```bash
gcc -g -O0 -o callstack_demo callstack_demo.c
```

El flag `-O0` desactiva optimizaciones para que los frames no se
eliminen. Con `-O2`, el compilador puede hacer **inlining** (copiar el
cuerpo de la función en el punto de llamada, eliminando el frame).

---

## 2. Visualización con GDB

### Sesión básica

```bash
gdb ./callstack_demo
```

```
(gdb) break multiply
Breakpoint 1 at 0x...: file callstack_demo.c, line 4.

(gdb) run
Starting program: ./callstack_demo
Breakpoint 1, multiply (a=7, b=5) at callstack_demo.c:4

(gdb) backtrace
#0  multiply (a=7, b=5) at callstack_demo.c:4
#1  0x... in add_and_multiply (x=3, y=4, z=5) at callstack_demo.c:10
#2  0x... in main () at callstack_demo.c:14
```

### Anatomía del backtrace

El comando `backtrace` (o `bt`) muestra la **pila de llamadas** actual:

```
#0  multiply (a=7, b=5)           ← frame actual (tope de la pila)
#1  add_and_multiply (x=3, y=4, z=5)  ← quien llamó a multiply
#2  main ()                       ← quien llamó a add_and_multiply
```

Cada línea es un **stack frame**. El frame `#0` es el tope (la función
ejecutándose actualmente). Los números crecientes van hacia la base de la
pila (hacia `main`).

### Comandos GDB para inspeccionar el stack

| Comando | Abreviatura | Función |
|---------|:-----------:|---------|
| `backtrace` | `bt` | Mostrar la pila de llamadas completa |
| `backtrace 5` | `bt 5` | Mostrar solo los 5 frames superiores |
| `frame N` | `f N` | Cambiar al frame número N |
| `info locals` | — | Mostrar variables locales del frame actual |
| `info args` | — | Mostrar argumentos del frame actual |
| `up` | — | Subir un frame (hacia main) |
| `down` | — | Bajar un frame (hacia el tope) |
| `info frame` | — | Información detallada del frame actual |

### Ejemplo con recursión

```c
/* fib_debug.c */
#include <stdio.h>

int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

int main(void) {
    printf("%d\n", fib(6));
    return 0;
}
```

```bash
gcc -g -O0 -o fib_debug fib_debug.c
gdb ./fib_debug
```

```
(gdb) break fib if n == 0
Breakpoint 1 at 0x...: file fib_debug.c, line 4.

(gdb) run

(gdb) backtrace
#0  fib (n=0) at fib_debug.c:4
#1  fib (n=2) at fib_debug.c:5
#2  fib (n=3) at fib_debug.c:5
#3  fib (n=4) at fib_debug.c:5
#4  fib (n=5) at fib_debug.c:5
#5  fib (n=6) at fib_debug.c:5
#6  main () at fib_debug.c:9
```

Este backtrace muestra **7 frames simultáneos**: `main` + 6 niveles de
recursión (`fib(6)` → `fib(5)` → `fib(4)` → `fib(3)` → `fib(2)` →
`fib(0)`). Cada frame `fib` tiene su propia copia de `n`.

### Breakpoint condicional

El `break fib if n == 0` es un **breakpoint condicional**: solo se activa
cuando la condición es verdadera. Sin la condición, GDB se detendría en
cada una de las 25 llamadas a `fib` para `fib(6)`. La condición permite
capturar exactamente el momento de máxima profundidad.

---

## 3. Stack overflow

### Qué es

El stack tiene un tamaño fijo asignado al inicio del proceso (típicamente
8 MB en Linux). Cuando la recursión consume todo el espacio disponible, el
programa intenta escribir más allá del límite del stack → el sistema
operativo lanza una señal **SIGSEGV** (segmentation fault).

### Ejemplo en C

```c
/* overflow.c */
#include <stdio.h>

void infinite_recurse(int depth) {
    char buffer[1024];   /* 1 KB per frame */
    buffer[0] = 'x';    /* prevent optimization */
    printf("Depth: %d\n", depth);
    infinite_recurse(depth + 1);
}

int main(void) {
    infinite_recurse(1);
    return 0;
}
```

```bash
gcc -g -O0 -o overflow overflow.c
./overflow
```

Salida (aproximada):

```
Depth: 1
Depth: 2
...
Depth: 7800
Segmentation fault (core dumped)
```

Con 1 KB de buffer local por frame y 8 MB de stack: $8192 / 1 \approx 8000$
niveles. El número exacto varía porque el frame incluye overhead adicional
(dirección de retorno, registros salvados, padding).

### Ejemplo en Rust

```rust
// overflow.rs
fn infinite_recurse(depth: u64) {
    let _buffer = [0u8; 1024];   // 1 KB per frame
    println!("Depth: {}", depth);
    infinite_recurse(depth + 1);
}

fn main() {
    infinite_recurse(1);
}
```

```bash
rustc -g -o overflow overflow.rs
./overflow
```

Rust produce un mensaje más informativo que C:

```
thread 'main' panicked at 'thread has overflowed its stack',
overflow.rs:4
```

En modo debug, Rust inserta **stack probes** — verificaciones periódicas
de que queda espacio en el stack. Esto convierte el SIGSEGV silencioso de
C en un panic con mensaje de error legible. En modo release (`--release`),
las probes siguen activas pero el mensaje puede variar.

### Stack overflow no es undefined behavior en Rust

En C, un stack overflow es **undefined behavior**: el programa puede
corromperse silenciosamente, ejecutar código arbitrario, o crashear con
un mensaje críptico. En Rust, el runtime detecta el overflow y produce
un abort controlado (no un UB).

---

## 4. ulimit y configuración del stack

### Consultar el tamaño actual

```bash
ulimit -s
# 8192    (KB, es decir, 8 MB)
```

### Cambiar el tamaño (solo para la sesión actual)

```bash
ulimit -s 16384    # 16 MB
ulimit -s unlimited  # sin límite (usa toda la memoria disponible)
```

`unlimited` no significa infinito — significa que el stack puede crecer
hasta que se agote la memoria virtual. En la práctica, suele funcionar
para recursiones profundas pero no es recomendable en producción.

### Hacer el cambio permanente

En `/etc/security/limits.conf` (requiere root):

```
*    soft    stack    16384
*    hard    stack    65536
```

O en el shell de inicio (`~/.bashrc` o `~/.zshrc`):

```bash
ulimit -s 16384
```

### Verificar el efecto

```bash
# Before:
ulimit -s
# 8192

ulimit -s 32768

# Run the overflow program again:
./overflow
# Depth reaches ~32000 before crashing
```

Duplicar el stack duplica la profundidad máxima de recursión.

### Tamaño del stack por thread

`ulimit` solo afecta al thread principal. Los threads creados con
`pthread_create` tienen su propio stack, configurado independientemente:

```c
#include <pthread.h>

pthread_attr_t attr;
pthread_attr_init(&attr);
pthread_attr_setstacksize(&attr, 16 * 1024 * 1024);  /* 16 MB */

pthread_t thread;
pthread_create(&thread, &attr, thread_func, NULL);
```

En Rust:

```rust
std::thread::Builder::new()
    .stack_size(16 * 1024 * 1024)
    .spawn(|| { /* ... */ })
    .unwrap();
```

---

## 5. Stack overflow en Rust

### Protecciones del runtime

Rust tiene dos niveles de protección contra stack overflow:

**1. Stack probes (compilador)**: El compilador inserta código al inicio de
funciones con frames grandes que verifica que queda espacio. Si no queda,
el programa aborta con un mensaje en lugar de corromper memoria.

**2. Guard page (OS)**: El sistema operativo coloca una página de memoria
protegida (sin permisos de escritura) al final del stack. Cualquier
escritura en ella genera SIGSEGV inmediatamente, antes de que el programa
pueda sobrescribir otras estructuras de datos.

### Recursión profunda segura en Rust

Para algoritmos que requieren recursión profunda legítima (por ejemplo,
recorrido de árboles muy desbalanceados), las opciones son:

**Opción 1: Thread con stack grande**

```rust
fn deep_recursion() {
    let handle = std::thread::Builder::new()
        .stack_size(64 * 1024 * 1024)  // 64 MB
        .spawn(|| {
            recursive_algorithm(large_input);
        })
        .unwrap();
    handle.join().unwrap();
}
```

**Opción 2: Convertir a iteración con pila explícita** (se verá en T03)

```rust
fn iterative_algorithm(input: &Data) {
    let mut stack = Vec::new();
    stack.push(initial_state);
    while let Some(state) = stack.pop() {
        // process state, push new states
    }
}
```

La opción 2 es generalmente preferible porque el `Vec` crece
dinámicamente sin un límite fijo.

---

## 6. Layout de memoria de un proceso

El stack es una de varias regiones de memoria de un proceso:

```
Direcciones altas
┌──────────────────────┐
│     Kernel space      │  (no accesible por el programa)
├──────────────────────┤
│       Stack           │  ← crece hacia abajo ↓
│  (variables locales,  │
│   frames, retornos)   │
│          ↓            │
├ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┤
│    (espacio libre)    │
├ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┤
│          ↑            │
│        Heap           │  ← crece hacia arriba ↑
│  (malloc, Box, Vec)   │
├──────────────────────┤
│   BSS (uninit data)  │  (variables globales sin inicializar)
├──────────────────────┤
│   Data (init data)   │  (variables globales inicializadas)
├──────────────────────┤
│       Text (code)     │  (instrucciones del programa)
└──────────────────────┘
Direcciones bajas
```

### Stack vs Heap

| Propiedad | Stack | Heap |
|-----------|:-----:|:----:|
| Gestión | Automática (push/pop frames) | Manual (`malloc`/`free`, `Box`) |
| Velocidad | Muy rápida (solo mover RSP) | Más lenta (buscar bloque libre) |
| Tamaño | Fijo (8 MB típico) | Dinámico (GBs disponibles) |
| Fragmentación | No | Sí |
| Orden de liberación | LIFO estricto | Cualquier orden |
| Uso | Variables locales, parámetros | Datos dinámicos, estructuras grandes |

### Por qué el stack crece hacia abajo

Es una convención histórica de la arquitectura x86. El stack empieza en
direcciones altas y crece decrementando `RSP`. El heap empieza en
direcciones bajas y crece incrementando `brk`. Ambos crecen uno hacia el
otro, maximizando el espacio libre entre ellos.

Si el stack crece lo suficiente y alcanza el heap (o viceversa), se
produce corrupción de memoria. La guard page previene esto para el stack,
y `mmap` con protección previene esto para el heap.

---

## 7. Relación con la pila como estructura de datos

La pila de llamadas **es** una pila. No es una analogía: es una pila
implementada en hardware (registros RSP/RBP) y memoria real.

### Correspondencia exacta

| Pila (TAD, C02) | Pila de llamadas |
|:----------------:|:----------------:|
| `push(x)` | Llamar a función (crear frame) |
| `pop()` | Retornar de función (destruir frame) |
| `peek()` | Acceder a variables del frame actual |
| `is_empty()` | Solo queda `main` (o `_start`) |
| `size()` | Profundidad de recursión actual |
| Overflow | Stack overflow (recursión demasiado profunda) |
| LIFO | La última función llamada retorna primera |

### Demostración: recursión como operaciones de pila

```c
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
```

Ejecutar `factorial(4)` es equivalente a:

```
push frame{n=4}      // call factorial(4)
  push frame{n=3}    // call factorial(3)
    push frame{n=2}  // call factorial(2)
      push frame{n=1}  // call factorial(1)
      pop → return 1    // return from factorial(1)
    pop → return 2*1=2  // return from factorial(2)
  pop → return 3*2=6    // return from factorial(3)
pop → return 4*6=24     // return from factorial(4)
```

Esto es exactamente el comportamiento de la pila que implementamos
en C02 con arrays y listas enlazadas — excepto que aquí la gestiona el
hardware.

### Implicación práctica

Cualquier algoritmo recursivo puede reescribirse con una pila explícita
porque la recursión **ya usa una pila** (la del sistema). La conversión
de recursión a iteración con pila explícita simplemente hace explícito lo
que el compilador hace implícitamente. Se explorará esto en T03.

---

## 8. Herramientas de diagnóstico

### GDB: comandos esenciales para recursión

```bash
# Compilar con debug
gcc -g -O0 -o prog prog.c

# Iniciar GDB
gdb ./prog

# Breakpoint en función recursiva con condición
(gdb) break fib if n == 1

# Ejecutar
(gdb) run

# Ver pila completa
(gdb) bt

# Ver solo los 10 frames superiores
(gdb) bt 10

# Ver frame específico
(gdb) frame 3
(gdb) info locals
(gdb) info args

# Contar profundidad (en la pila de fib(30)):
(gdb) bt
# GDB muestra todos los frames — contar la profundidad máxima
```

### Valgrind: detectar stack overflow

Valgrind ejecuta el programa en un entorno instrumentado. Aunque no
detecta stack overflow directamente (ya que no intercepta el crecimiento
del stack), puede detectar **accesos inválidos** a memoria cercana al
stack.

Para detectar problemas de stack en producción, la combinación
`-fsanitize=address` (AddressSanitizer) es más efectiva:

```bash
gcc -g -O0 -fsanitize=address -o prog prog.c
./prog
```

AddressSanitizer detecta stack buffer overflows, use-after-return, y
reporta con trazas detalladas.

### Medir la profundidad del stack en C

```c
#include <stdio.h>

/* approximate stack depth by comparing addresses */
static const char *stack_base = NULL;

void measure_depth(int n) {
    char marker;  /* local variable — lives on the stack */
    if (stack_base == NULL) stack_base = &marker;

    long depth_bytes = stack_base - &marker;

    if (n <= 1) {
        printf("Max depth: %d calls, ~%ld bytes of stack\n",
               n, depth_bytes);
        return;
    }
    measure_depth(n - 1);
}
```

Esta técnica compara la dirección de una variable local en el frame
actual con una en el frame inicial. La diferencia es el número de bytes
de stack consumidos. Es una aproximación (el compilador puede reordenar
variables) pero da una idea útil.

### Medir en Rust

Rust no expone direcciones de variables locales tan fácilmente (las
referencias no son punteros crudos). Sin embargo, se puede usar la misma
técnica dentro de un bloque `unsafe`:

```rust
fn measure_depth(n: u64, base: *const u8) {
    let marker: u8 = 0;
    let current = &marker as *const u8;
    let depth = unsafe { base.offset_from(current) };

    if n <= 1 {
        println!("Max depth: {} calls, ~{} bytes", n, depth);
        return;
    }
    measure_depth(n - 1, base);
}

fn main() {
    let base: u8 = 0;
    measure_depth(1000, &base as *const u8);
}
```

---

## 9. Ejercicios

---

### Ejercicio 1 — Sesión GDB con factorial

Compila `factorial.c` (de T01) con `-g -O0`. En GDB:

1. Pon un breakpoint en `factorial` con condición `n == 1`.
2. Ejecuta con argumento `factorial(6)`.
3. Cuando se detenga, ejecuta `bt` y anota toda la traza.
4. Usa `frame 3` para moverte al frame de `factorial(4)` y ejecuta
   `info args`.

<details>
<summary>¿Cuántos frames muestra el backtrace y qué dice `info args` en frame 3?</summary>

El backtrace muestra **7 frames**:

```
#0  factorial (n=1) at factorial.c:4
#1  factorial (n=2) at factorial.c:5
#2  factorial (n=3) at factorial.c:5
#3  factorial (n=4) at factorial.c:5
#4  factorial (n=5) at factorial.c:5
#5  factorial (n=6) at factorial.c:5
#6  main () at factorial.c:9
```

`frame 3` cambia al frame de `factorial(4)`. `info args` muestra:

```
n = 4
```

Cada frame tiene su propia copia de `n`. El frame #3 "recuerda" que fue
llamado con `n=4`, mientras que el frame #0 (actual) tiene `n=1`. Esto
demuestra que las variables locales son privadas a cada invocación.

</details>

---

### Ejercicio 2 — Provocar stack overflow

Escribe un programa en C con una función recursiva que tenga un array
local de 4096 bytes. Calcula teóricamente cuántas llamadas caben en el
stack por defecto (8 MB). Luego ejecútalo y compara.

<details>
<summary>¿Coincide la predicción teórica con la profundidad real?</summary>

```c
#include <stdio.h>

void overflow_test(int depth) {
    char buffer[4096];
    buffer[0] = (char)depth;  /* prevent optimization */
    if (depth % 100 == 0) {
        printf("Depth: %d\n", depth);
    }
    overflow_test(depth + 1);
}

int main(void) {
    overflow_test(1);
    return 0;
}
```

Predicción teórica: cada frame necesita ~4096 bytes (buffer) + ~32 bytes
(overhead) ≈ 4128 bytes. Con 8 MB = 8,388,608 bytes:
$8388608 / 4128 \approx 2032$ llamadas.

En la práctica (compilado con `-O0`):

```
Depth: 1
Depth: 100
...
Depth: 1900
Segmentation fault
```

La profundidad real es cercana a 1900-2000, consistente con la predicción.
La diferencia se debe a que `main` y las funciones de `printf` también
consumen stack, y el overhead por frame puede ser mayor que 32 bytes
(alignment, registros salvados).

Con `-O2`, el compilador puede eliminar el buffer si detecta que no se
usa de forma observable, aumentando drásticamente la profundidad. El
`buffer[0] = (char)depth` dificulta esta optimización.

</details>

---

### Ejercicio 3 — ulimit en acción

Ejecuta los siguientes comandos y anota los resultados:

```bash
ulimit -s              # tamaño actual
ulimit -s 2048         # reducir a 2 MB
./overflow_test        # ¿a qué profundidad crashea?
ulimit -s 32768        # aumentar a 32 MB
./overflow_test        # ¿a qué profundidad crashea?
```

<details>
<summary>¿La profundidad es proporcional al tamaño del stack?</summary>

Con el programa del ejercicio 2:

```
ulimit -s 2048  (2 MB):   ~480 llamadas antes del crash
ulimit -s 8192  (8 MB):   ~1950 llamadas
ulimit -s 32768 (32 MB):  ~7900 llamadas
```

La relación es **aproximadamente proporcional**:

| Stack | Profundidad | Ratio |
|:-----:|:-----------:|:-----:|
| 2 MB | ~480 | 1× |
| 8 MB | ~1950 | ~4× |
| 32 MB | ~7900 | ~16× |

$2 \times 4 = 8$ MB y $480 \times 4 \approx 1920$ — casi exacto. La
proporcionalidad confirma que el factor limitante es el tamaño total del
stack dividido por el tamaño de cada frame.

Nota: `ulimit -s` solo cambia el límite para el shell actual y sus
procesos hijos. Abrir otra terminal mostrará el valor por defecto.

</details>

---

### Ejercicio 4 — Backtrace de Fibonacci en GDB

Ejecuta `fib(10)` en GDB con un breakpoint condicional `break fib if n == 0`.
Cuando se detenga por primera vez, ejecuta `bt`. Anota la profundidad.
Luego ejecuta `continue` para llegar al siguiente `n == 0` y ejecuta `bt`
de nuevo.

<details>
<summary>¿Los dos backtraces son iguales? ¿Por qué?</summary>

No, son diferentes. Las dos primeras paradas muestran:

**Primera parada** (primer `fib(0)` alcanzado):
```
#0  fib (n=0)
#1  fib (n=2)
#2  fib (n=3)
#3  fib (n=4)
#4  fib (n=5)
#5  fib (n=6)
#6  fib (n=7)
#7  fib (n=8)
#8  fib (n=9)
#9  fib (n=10)
#10 main ()
```

Profundidad: 11 frames. Este es el camino `10→9→8→7→6→5→4→3→2→0`
(siempre tomando la rama `fib(n-1)` primero, luego `fib(n-2)` al final).

**Segunda parada** (segundo `fib(0)` alcanzado):
```
#0  fib (n=0)
#1  fib (n=2)
#2  fib (n=3)
#3  fib (n=4)
#4  fib (n=5)
#5  fib (n=6)
#6  fib (n=7)
#7  fib (n=8)
#8  fib (n=9)
#9  fib (n=10)
#10 main ()
```

Este backtrace se ve similar pero el camino completo es diferente: el
primer `fib(2)` ya retornó, y este es el `fib(0)` de otro `fib(2)` en
otra rama del árbol. Los backtraces tienen la misma profundidad porque
`fib(0)` siempre está a la misma distancia del tope en esta secuencia.

Para ver la diferencia, usar `break fib if n == 0` con conteo:
`(gdb) ignore 1 5` ignorará las primeras 5 paradas y se detendrá en la 6ª,
mostrando un backtrace con diferente estructura.

</details>

---

### Ejercicio 5 — Medir bytes de stack por llamada

Usa la técnica de la sección 8 (comparar direcciones de variables locales)
para medir cuántos bytes de stack consume cada llamada a `factorial` y a
`fib`. Compila con `-O0` y luego con `-O2`.

<details>
<summary>¿Cambia el tamaño del frame entre -O0 y -O2?</summary>

Código de medición:

```c
#include <stdio.h>

static const char *base_addr = NULL;

unsigned long factorial_measure(int n) {
    char marker;
    if (base_addr == NULL) base_addr = &marker;

    if (n <= 1) {
        printf("factorial: %ld bytes for %d frames\n",
               (long)(base_addr - &marker),
               n);
        return 1;
    }
    return n * factorial_measure(n - 1);
}
```

Resultados típicos en x86-64 Linux:

| Función | `-O0` | `-O2` |
|---------|:-----:|:-----:|
| factorial | ~48 bytes/frame | ~16 bytes/frame o 0 (inlined/TCO) |
| fib | ~64 bytes/frame | ~32 bytes/frame |

Con `-O2`:
- **factorial** puede ser optimizado por TCO (tail call optimization) si
  el compilador lo detecta como recursión de cola — en ese caso usa 0
  bytes adicionales de stack (reutiliza el mismo frame). Pero factorial
  como `n * factorial(n-1)` no es de cola (la multiplicación ocurre
  después), así que el compilador puede o no optimizarlo.
- **fib** no puede ser TCO (dos llamadas recursivas) pero el compilador
  puede reducir el tamaño del frame eliminando padding y registros
  innecesarios.

</details>

---

### Ejercicio 6 — Stack overflow controlado

Escribe un programa en C que determine experimentalmente la profundidad
máxima de recursión **sin crashear**. Usa `setjmp`/`longjmp` para
recuperarse del overflow, o simplemente cuenta los niveles hasta que un
valor conocido cambie. Alernativa más simple: usa un contador y para
antes de alcanzar el límite.

<details>
<summary>¿Se puede detectar el overflow antes de que ocurra?</summary>

La forma más portable de limitar la profundidad es con un contador:

```c
#include <stdio.h>

#define MAX_DEPTH 10000

int safe_recurse(int depth) {
    if (depth >= MAX_DEPTH) {
        printf("Stopped at depth %d to prevent overflow\n", depth);
        return depth;
    }
    return safe_recurse(depth + 1);
}
```

Para determinar el límite experimentalmente sin crashear:

```c
#include <stdio.h>
#include <sys/resource.h>

long get_stack_size(void) {
    struct rlimit rl;
    getrlimit(RLIMIT_STACK, &rl);
    return (long)rl.rlim_cur;
}

int main(void) {
    long stack_bytes = get_stack_size();
    /* estimate ~128 bytes per frame for a typical function */
    long estimated_max = stack_bytes / 128;
    /* use 80% as safety margin */
    long safe_max = estimated_max * 80 / 100;

    printf("Stack: %ld bytes\n", stack_bytes);
    printf("Estimated max depth: %ld\n", estimated_max);
    printf("Safe max depth: %ld\n", safe_max);

    return 0;
}
```

`getrlimit` consulta el límite del stack programáticamente (equivalente a
`ulimit -s` pero desde código C). Dividir por un tamaño estimado de frame
da la profundidad aproximada. El margen del 80% deja espacio para `main`,
`printf`, y otras funciones en la pila.

</details>

---

### Ejercicio 7 — Threads con stacks diferentes en Rust

Crea dos threads en Rust: uno con stack de 1 MB y otro con 32 MB. Cada
uno ejecuta la misma función recursiva y reporta la profundidad máxima
alcanzada antes de panic.

<details>
<summary>¿Cómo capturas el panic de stack overflow en un thread?</summary>

No se puede capturar directamente con `catch_unwind` porque un stack
overflow en Rust produce un **abort** (no un panic recuperable). Pero se
puede estimar la profundidad antes de llegar al límite:

```rust
use std::thread;

fn deep_recurse(depth: u64) -> u64 {
    let _pad = [0u8; 512]; // 512 bytes per frame
    if depth % 1000 == 0 {
        eprintln!("depth: {}", depth);
    }
    deep_recurse(depth + 1)
}

fn main() {
    let small = thread::Builder::new()
        .name("small-stack".into())
        .stack_size(1 * 1024 * 1024) // 1 MB
        .spawn(|| deep_recurse(1));

    let big = thread::Builder::new()
        .name("big-stack".into())
        .stack_size(32 * 1024 * 1024) // 32 MB
        .spawn(|| deep_recurse(1));

    // These will abort when they overflow;
    // run them separately and observe the last "depth:" printed
    let _ = small.unwrap().join();
    // let _ = big.unwrap().join();
}
```

Ejecutar cada uno por separado:

- 1 MB con 512 bytes/frame: ~1900 niveles
- 32 MB con 512 bytes/frame: ~62000 niveles

La proporción 32× en stack da ~32× en profundidad, confirmando la
relación lineal entre tamaño del stack y profundidad máxima.

</details>

---

### Ejercicio 8 — Comparar -O0 vs -O2

Compila `fib(40)` con `-O0` y con `-O2`. Mide el tiempo de ejecución con
`time`. Luego, compila `factorial(20)` con `-O2` y verifica con GDB si el
compilador aplicó alguna optimización al stack.

<details>
<summary>¿Qué optimizaciones aplica -O2 a estas funciones?</summary>

Medición:

```bash
# fib(40)
gcc -O0 -o fib0 fib.c && time ./fib0    # ~2-5 seconds
gcc -O2 -o fib2 fib.c && time ./fib2    # ~0.5-1 seconds
```

Para `fib`, `-O2` no puede eliminar las llamadas recursivas (la
estructura del árbol es inherente), pero puede:
- Reducir el tamaño de cada frame (menos spills a memoria)
- Usar registros más eficientemente
- Inline la comparación `n <= 1`

El speedup típico es 2-5×, no exponencial.

Para `factorial`:

```bash
gcc -O2 -g -o fact2 factorial.c
gdb ./fact2
(gdb) disassemble factorial
```

Con `-O2`, el compilador puede:
1. **Eliminar la recursión completamente** (convertir a loop)
2. **Computar el resultado en tiempo de compilación** si el argumento es
   constante (`factorial(20)` → constante `2432902008176640000`)

Verificar con `(gdb) info locals` — si no hay frame de recursión, el
compilador convirtió la recursión a iteración. Esto demuestra que el
compilador puede hacer automáticamente la conversión que estudiaremos
manualmente en T03.

</details>

---

### Ejercicio 9 — Layout de memoria

Escribe un programa en C que imprima las direcciones de:
- Una variable global
- Una variable local en `main`
- Una variable local en una función llamada por `main`
- Un bloque de `malloc`

Verifica que las direcciones son consistentes con el layout de la
sección 6 (text < data < heap < ... < stack).

<details>
<summary>¿El stack tiene direcciones más altas que el heap?</summary>

```c
#include <stdio.h>
#include <stdlib.h>

int global_var = 42;

void callee(void) {
    int callee_local = 0;
    printf("callee local:  %p\n", (void *)&callee_local);
}

int main(void) {
    int main_local = 0;
    int *heap_var = malloc(sizeof(int));

    printf("global:        %p\n", (void *)&global_var);
    printf("main local:    %p\n", (void *)&main_local);
    callee();
    printf("heap (malloc): %p\n", (void *)heap_var);
    printf("main func:     %p\n", (void *)main);

    free(heap_var);
    return 0;
}
```

Salida típica en Linux x86-64:

```
global:        0x404040          (sección data, dirección baja)
main local:    0x7ffd12345670    (stack, dirección alta)
callee local:  0x7ffd12345640    (stack, más abajo que main)
heap (malloc): 0x5555556032a0    (heap, entre data y stack)
main func:     0x401136          (sección text, dirección baja)
```

Confirmaciones:
- text (`0x40...`) < data (`0x40...`) < heap (`0x55...`) < stack (`0x7f...`)
- `callee_local` tiene dirección **menor** que `main_local` (stack crece
  hacia abajo)
- `heap_var` está entre data y stack

Con ASLR (Address Space Layout Randomization), las direcciones exactas
cambian en cada ejecución, pero las **relaciones de orden** se mantienen.

</details>
