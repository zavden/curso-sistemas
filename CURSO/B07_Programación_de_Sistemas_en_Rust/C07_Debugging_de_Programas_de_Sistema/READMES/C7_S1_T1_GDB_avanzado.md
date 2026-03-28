# GDB avanzado

## Índice

1. [GDB más allá de lo básico](#gdb-más-allá-de-lo-básico)
2. [Conditional breakpoints](#conditional-breakpoints)
3. [Watchpoints: vigilar cambios en memoria](#watchpoints-vigilar-cambios-en-memoria)
4. [Catchpoints: capturar eventos del sistema](#catchpoints-capturar-eventos-del-sistema)
5. [Breakpoint commands: automatizar acciones](#breakpoint-commands-automatizar-acciones)
6. [Examinar memoria: el comando x](#examinar-memoria-el-comando-x)
7. [Core dumps: análisis post-mortem](#core-dumps-análisis-post-mortem)
8. [Remote debugging: depurar otro proceso/máquina](#remote-debugging-depurar-otro-procesomáquina)
9. [GDB con Rust](#gdb-con-rust)
10. [GDB scripting con Python](#gdb-scripting-con-python)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## GDB más allá de lo básico

Este tema asume que conoces los comandos básicos de GDB (`break`, `run`, `next`, `step`, `print`, `continue`, `backtrace`). Nos enfocamos en técnicas avanzadas para depurar programas de sistema.

### Preparar el binario

```bash
# C: compilar con símbolos de debug y sin optimización
gcc -g -O0 -o program program.c

# C con warnings completos
gcc -g -O0 -Wall -Wextra -o program program.c

# Rust: debug build (por defecto incluye símbolos)
cargo build
# El binario está en target/debug/nombre_proyecto

# Rust: release con símbolos de debug
cargo build --release
# Añadir en Cargo.toml:
# [profile.release]
# debug = true
```

### Iniciar GDB con comodidades

```bash
# Iniciar con un programa
gdb ./program

# Iniciar y ejecutar inmediatamente con argumentos
gdb --args ./program arg1 arg2

# Conectar a un proceso en ejecución
gdb -p <PID>

# Interfaz TUI (curses) — muestra código fuente en tiempo real
gdb -tui ./program

# Modo batch: ejecutar comandos y salir
gdb -batch -ex "run" -ex "bt" ./program

# Cargar comandos desde archivo
gdb -x commands.gdb ./program
```

### Layout TUI

```
┌──────────────────────────────────────────────┐
│  (gdb) layout src     ← solo código fuente   │
│  (gdb) layout asm     ← solo ensamblador     │
│  (gdb) layout split   ← código + ensamblador │
│  (gdb) layout regs    ← registros            │
│  (gdb) focus cmd      ← foco al prompt       │
│  (gdb) focus src      ← foco al código       │
│  Ctrl+X, A            ← toggle TUI on/off    │
└──────────────────────────────────────────────┘
```

---

## Conditional breakpoints

Un breakpoint condicional solo se detiene cuando una expresión es verdadera. Invaluable para bugs que solo ocurren en la iteración 9999 de un loop.

### Crear breakpoint condicional

```
# Sintaxis: break LOCATION if CONDICIÓN
(gdb) break process_item if item_count == 500
Breakpoint 1 at 0x4011a0: file main.c, line 42.

# Condición sobre variable local
(gdb) break parser.c:120 if strlen(token) > 100

# Condición sobre puntero
(gdb) break insert_node if node == NULL

# Condición compuesta
(gdb) break main.c:50 if (x > 0 && y < 100)
```

### Añadir condición a breakpoint existente

```
(gdb) break main.c:30
Breakpoint 1 at 0x401150

# Añadir condición después
(gdb) condition 1 i == 42

# Eliminar la condición (breakpoint siempre se activa)
(gdb) condition 1
```

### Ejemplo práctico: encontrar cuándo un valor se corrompe

```c
// bug.c — algún lugar en el loop, data[5] se corrompe
#include <stdio.h>

void process(int *data, int n) {
    for (int i = 0; i < n; i++) {
        data[i] = i * 2;
        if (i == 7) {
            data[5] = -999;  // bug: escribe fuera de su índice
        }
    }
}

int main() {
    int data[10] = {0};
    process(data, 10);
    printf("data[5] = %d\n", data[5]);  // -999, ¿por qué?
    return 0;
}
```

```
$ gcc -g -O0 -o bug bug.c
$ gdb ./bug

(gdb) break process if data[5] != 0 && i > 5
(gdb) run
# Se detiene en la iteración donde data[5] cambia
(gdb) print i
$1 = 8        ← la iteración DESPUÉS del cambio
(gdb) print data[5]
$2 = -999
```

Mejor aún: usar un watchpoint (siguiente sección).

---

## Watchpoints: vigilar cambios en memoria

Un watchpoint detiene el programa cuando una dirección de memoria **cambia de valor**. Es la herramienta perfecta para "¿quién modificó esta variable?".

### Tipos de watchpoints

```
# watch: se detiene cuando el valor CAMBIA (hardware si posible)
(gdb) watch variable
(gdb) watch *0x7ffd1234       # dirección específica
(gdb) watch array[5]          # elemento de array

# rwatch: se detiene cuando se LEE
(gdb) rwatch variable

# awatch: se detiene cuando se lee O escribe
(gdb) awatch variable
```

### Ejemplo: encontrar quién modifica una variable

```
$ gdb ./bug

# Primero, poner breakpoint donde la variable existe
(gdb) break process
(gdb) run

# Ahora que estamos en el scope, poner watchpoint
(gdb) watch data[5]
Hardware watchpoint 2: data[5]

(gdb) continue
# GDB se detiene EXACTAMENTE en la línea que modifica data[5]:

Hardware watchpoint 2: data[5]
Old value = 0
New value = 10         ← primer cambio legítimo (i=5, data[5]=5*2)

(gdb) continue
Hardware watchpoint 2: data[5]
Old value = 10
New value = -999       ← ¡aquí está el bug!

(gdb) bt
#0  process (data=0x7ffd..., n=10) at bug.c:7
(gdb) print i
$1 = 7                 ← la iteración causante
```

### Hardware vs software watchpoints

```
┌─────────────────────────────────────────────────────┐
│  Hardware watchpoints (preferidos):                  │
│  - Usa registros de debug del CPU (DR0-DR3 en x86)  │
│  - Sin overhead de performance                      │
│  - Limitados: típicamente 4 watchpoints simultáneos │
│  - Solo monitorean direcciones fijas                │
│                                                     │
│  Software watchpoints (fallback):                   │
│  - GDB ejecuta step-by-step y compara              │
│  - MUY lento (100-1000x más lento)                 │
│  - Sin límite de cantidad                           │
│  - GDB los usa automáticamente cuando no hay HW    │
│                                                     │
│  (gdb) show can-use-hw-watchpoints                  │
│  (gdb) set can-use-hw-watchpoints 0  ← forzar SW   │
└─────────────────────────────────────────────────────┘
```

### Watchpoint condicional

```
# Solo detenerse si data[5] cambia a un valor negativo
(gdb) watch data[5] if data[5] < 0
```

---

## Catchpoints: capturar eventos del sistema

Los catchpoints detienen el programa en eventos del sistema, no en líneas de código.

```
# Capturar todas las llamadas a fork/exec
(gdb) catch fork
(gdb) catch exec

# Capturar señales
(gdb) catch signal SIGSEGV
(gdb) catch signal SIGFPE

# Capturar excepciones C++ (throw/catch)
(gdb) catch throw
(gdb) catch catch

# Capturar syscalls específicas
(gdb) catch syscall write
(gdb) catch syscall open close

# Capturar carga de librerías dinámicas
(gdb) catch load
(gdb) catch load libc.so

# Listar todos los catchpoints
(gdb) info breakpoints
```

### Ejemplo: capturar SIGSEGV antes de que mate el proceso

```
(gdb) catch signal SIGSEGV
(gdb) run

# Cuando ocurre segfault, GDB se detiene ANTES de que
# el proceso muera, permitiendo examinar el estado:
(gdb) bt
(gdb) info registers
(gdb) print *ptr    ← examinar el puntero problemático
```

---

## Breakpoint commands: automatizar acciones

Puedes ejecutar comandos automáticamente cuando un breakpoint se activa:

### Sintaxis básica

```
(gdb) break main.c:50
Breakpoint 1

(gdb) commands 1
> print x
> print y
> continue
> end
```

Ahora, cada vez que el breakpoint 1 se active, GDB imprime `x` e `y` y continúa automáticamente — funciona como un `printf` sin modificar el código.

### Logging: trazar ejecución sin detener

```
(gdb) break process_request
(gdb) commands
> silent
> printf "request: id=%d, type=%s\n", req->id, req->type
> continue
> end
```

`silent` suprime el mensaje "Breakpoint hit...". El resultado es un log de todas las llamadas a `process_request` sin detener el programa.

### Ejemplo: contar llamadas a una función

```
(gdb) set $call_count = 0
(gdb) break malloc
(gdb) commands
> silent
> set $call_count = $call_count + 1
> continue
> end

(gdb) run
# ...programa termina...
(gdb) print $call_count
$1 = 1234    ← malloc fue llamada 1234 veces
```

### Guardar breakpoints

```
# Guardar todos los breakpoints a un archivo
(gdb) save breakpoints bp.gdb

# Cargar breakpoints en otra sesión
(gdb) source bp.gdb
```

---

## Examinar memoria: el comando x

El comando `x` (examine) muestra el contenido raw de memoria. Esencial para depurar corrupción de memoria y entender layouts de datos.

### Sintaxis: x/NFU DIRECCIÓN

```
N = número de unidades a mostrar
F = formato: x(hex), d(decimal), o(octal), t(binario),
             s(string), i(instrucción), c(char), f(float)
U = tamaño: b(byte), h(halfword/2B), w(word/4B), g(giant/8B)
```

### Ejemplos prácticos

```
# Ver 16 bytes en hexadecimal
(gdb) x/16xb &data
0x7ffd1230: 0x0a 0x00 0x00 0x00 0x14 0x00 0x00 0x00
0x7ffd1238: 0x1e 0x00 0x00 0x00 0x28 0x00 0x00 0x00

# Ver 4 words (i32) en decimal
(gdb) x/4wd &data
0x7ffd1230: 10    20    30    40

# Ver como string
(gdb) x/s name
0x402010: "hello world"

# Ver 5 instrucciones desde el PC actual
(gdb) x/5i $pc
0x401150: mov    %edi,%ebx
0x401152: call   0x401040 <puts@plt>
...

# Ver un struct en memoria
(gdb) x/8xw &my_struct

# Ver stack frames raw
(gdb) x/20xg $rsp
```

### Ejemplo: inspeccionar layout de un struct

```c
struct Point {
    int x;
    int y;
    char label[8];
};

struct Point p = {100, 200, "origin"};
```

```
(gdb) print p
$1 = {x = 100, y = 200, label = "origin\000\000"}

(gdb) x/16xb &p
0x7ffd1230: 0x64 0x00 0x00 0x00   ← x = 100 (0x64)
0x7ffd1234: 0xc8 0x00 0x00 0x00   ← y = 200 (0xc8)
0x7ffd1238: 0x6f 0x72 0x69 0x67   ← "orig"
0x7ffd123c: 0x69 0x6e 0x00 0x00   ← "in\0\0"
```

---

## Core dumps: análisis post-mortem

Un core dump es una foto de la memoria del proceso en el momento del crash. GDB puede analizarlo **sin necesidad de reproducir el bug**.

### Habilitar core dumps

```bash
# Verificar límite actual
ulimit -c
# 0 = deshabilitado

# Habilitar (unlimited)
ulimit -c unlimited

# En systemd, los core dumps van a coredumpctl
coredumpctl list
```

### Analizar un core dump

```bash
# Abrir core dump con GDB
gdb ./program core

# O con coredumpctl (systemd)
coredumpctl gdb <PID>
coredumpctl gdb ./program
```

```
(gdb) bt
#0  0x0000000000401135 in process (data=0x0, n=10) at main.c:5
#1  0x0000000000401180 in main () at main.c:12

# El crash fue en process(), data es NULL
(gdb) frame 0
(gdb) print data
$1 = (int *) 0x0     ← NULL pointer dereference!

(gdb) info locals
data = 0x0
n = 10
i = 0

(gdb) info registers
rdi            0x0     ← primer argumento era NULL
```

### Información disponible en un core dump

```
┌────────────────────────────────────────────────┐
│  Qué SÍ hay en un core dump:                  │
│  ├─ Stack completo de todos los threads        │
│  ├─ Registros del CPU                          │
│  ├─ Variables locales y globales               │
│  ├─ Heap (memoria dinámica)                    │
│  ├─ Memory mappings                            │
│  └─ Señal que causó el crash                   │
│                                                │
│  Qué NO hay:                                   │
│  ├─ Estado de archivos abiertos                │
│  ├─ Estado de la red                           │
│  ├─ Contenido de pipes/sockets                 │
│  └─ Variables de entorno (a veces sí)          │
└────────────────────────────────────────────────┘
```

---

## Remote debugging: depurar otro proceso/máquina

### Attach a proceso en ejecución

```bash
# Encontrar el PID
pidof my_program
# o
pgrep my_program

# Attach
gdb -p <PID>

# Dentro de GDB:
(gdb) attach <PID>
```

```
# Después de attach, el proceso está pausado:
(gdb) bt           ← ver qué está haciendo
(gdb) info threads ← ver todos los threads
(gdb) thread 2     ← cambiar al thread 2
(gdb) bt           ← backtrace del thread 2

# Continuar la ejecución
(gdb) detach       ← soltar el proceso (sigue corriendo)
# o
(gdb) continue     ← seguir depurando
```

### gdbserver: depurar en otra máquina

```bash
# En la máquina remota (target):
gdbserver :9999 ./program
# o attach a proceso existente:
gdbserver :9999 --attach <PID>

# En tu máquina (host):
gdb ./program
(gdb) target remote 192.168.1.100:9999
(gdb) break main
(gdb) continue
```

```
┌──────────────┐    TCP :9999    ┌──────────────┐
│  Tu máquina  │ ──────────────→ │   Servidor   │
│  (GDB)       │                 │ (gdbserver)  │
│              │ ←────────────── │ + programa   │
│  - símbolos  │   resultados    │ - sin GUI    │
│  - TUI       │                 │ - mínimo     │
└──────────────┘                 └──────────────┘
```

### Depurar múltiples threads

```
(gdb) info threads
  Id   Target Id          Frame
* 1    Thread 0x7f... "main" in main () at main.c:20
  2    Thread 0x7f... "worker" in process () at worker.c:15
  3    Thread 0x7f... "worker" in wait_for_data () at worker.c:30

# Cambiar a thread 2
(gdb) thread 2
(gdb) bt

# Aplicar comando a todos los threads
(gdb) thread apply all bt

# Breakpoint solo en un thread específico
(gdb) break worker.c:20 thread 2
```

---

## GDB con Rust

GDB funciona con binarios de Rust. Los nombres están decorados (mangled), pero GDB entiende la estructura.

### Debugging de un binario Rust

```bash
# Compilar en modo debug (default)
cargo build

# Depurar
gdb target/debug/my_project

# O con rust-gdb que carga pretty-printers
rust-gdb target/debug/my_project
```

### Pretty-printers de Rust

`rust-gdb` carga automáticamente pretty-printers para tipos de Rust:

```
# Sin pretty-printers:
(gdb) print my_vec
$1 = alloc::vec::Vec<i32> {buf: alloc::raw_vec::RawVec<i32, alloc::alloc::Global> {ptr: core::ptr::unique::Unique<i32> {pointer: 0x5555557a4b40, _marker: core::marker::PhantomData<i32>}, cap: 4, alloc: alloc::alloc::Global}, len: 3}

# Con pretty-printers (rust-gdb):
(gdb) print my_vec
$1 = Vec(size=3) = {1, 2, 3}
```

### Breakpoints en funciones Rust

```
# Por nombre de función (usa el path completo del módulo)
(gdb) break my_crate::my_module::my_function

# Por archivo:línea
(gdb) break src/main.rs:42

# Buscar funciones
(gdb) info functions process
# Lista todas las funciones que contienen "process"

# Breakpoint en método de trait
(gdb) break <my_crate::MyStruct as my_crate::MyTrait>::method_name
```

### Imprimir tipos de Rust

```
# Strings
(gdb) print my_string
$1 = "hello world"

# Option
(gdb) print my_option
$1 = Some(42)

# Result
(gdb) print my_result
$1 = Ok("success")

# HashMap (con pretty-printer)
(gdb) print my_map
$1 = HashMap(size=2) = {["key1"] = 100, ["key2"] = 200}

# Enum
(gdb) print my_enum
$1 = MyEnum::Variant2(42, "hello")
```

---

## GDB scripting con Python

GDB tiene un intérprete Python integrado para automatización avanzada.

### Comandos Python inline

```
(gdb) python print("Hello from Python in GDB")
Hello from Python in GDB

(gdb) python
> import gdb
> val = gdb.parse_and_eval("x")
> print(f"x = {val}, type = {val.type}")
> end
```

### Script: encontrar el máximo en un array

```python
# max_finder.py — cargar con: (gdb) source max_finder.py
import gdb

class FindMax(gdb.Command):
    """Find maximum value in an int array: find_max ARRAY LEN"""

    def __init__(self):
        super().__init__("find_max", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        args = arg.split()
        array_name = args[0]
        length = int(args[1])

        max_val = None
        max_idx = 0

        for i in range(length):
            val = int(gdb.parse_and_eval(f"{array_name}[{i}]"))
            if max_val is None or val > max_val:
                max_val = val
                max_idx = i

        print(f"Max value: {max_val} at index {max_idx}")

FindMax()
```

```
(gdb) source max_finder.py
(gdb) find_max data 10
Max value: 999 at index 7
```

### Script: dump de una lista enlazada

```python
# linkedlist.py
import gdb

class WalkList(gdb.Command):
    """Walk a linked list: walk_list HEAD_PTR"""

    def __init__(self):
        super().__init__("walk_list", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        node = gdb.parse_and_eval(arg)
        count = 0

        while node != 0:
            val = node["value"]
            print(f"  [{count}] value = {val}")
            node = node["next"]
            count += 1
            if count > 1000:
                print("  (stopped after 1000 nodes — possible cycle)")
                break

        print(f"Total: {count} nodes")

WalkList()
```

---

## Errores comunes

### 1. Compilar con optimización y esperar debugging útil

```bash
# INCORRECTO: optimizaciones reordenan y eliminan código
gcc -O2 -g -o program program.c
# GDB: "value optimized out", variables inaccesibles,
# líneas ejecutadas en orden no lineal

# CORRECTO: sin optimización para debugging
gcc -O0 -g -o program program.c

# En Rust, usar debug build (ya es -O0):
cargo build  # no --release
```

### 2. Olvidar habilitar core dumps

```bash
# El programa crasheó pero no hay core dump:
$ ./program
Segmentation fault

# Verificar:
$ ulimit -c
0    ← deshabilitado!

# Habilitar:
$ ulimit -c unlimited
$ ./program
Segmentation fault (core dumped)    ← ahora sí
```

### 3. Poner watchpoints fuera del scope de la variable

```
(gdb) run
# Paramos en main()

(gdb) watch local_in_other_function
No symbol "local_in_other_function" in current context.

# Los watchpoints sobre variables locales requieren estar
# en el scope donde la variable existe.
# Solución: poner un breakpoint en esa función primero,
# y después el watchpoint.
```

### 4. No usar thread apply all bt en programas multi-thread

```
(gdb) bt
# Solo muestra el backtrace del thread actual
# Los otros threads podrían tener el bug

# CORRECTO: ver TODOS los threads
(gdb) thread apply all bt
# o más conciso:
(gdb) thread apply all bt 5   ← solo 5 frames por thread
```

### 5. No usar rust-gdb para programas Rust

```bash
# INCORRECTO: GDB sin pretty-printers
gdb target/debug/my_program
(gdb) print my_vec
$1 = {buf = {ptr = {pointer = 0x5555...}, cap = 10}, len = 3}
# Ilegible

# CORRECTO: rust-gdb con pretty-printers
rust-gdb target/debug/my_program
(gdb) print my_vec
$1 = Vec(size=3) = {1, 2, 3}
# Legible
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║               GDB AVANZADO — REFERENCIA RÁPIDA           ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  INICIAR                                                  ║
║  ───────                                                  ║
║  gdb ./program                  iniciar                   ║
║  gdb --args ./prog a b          con argumentos            ║
║  gdb -p PID                     attach a proceso          ║
║  gdb -tui ./program             interfaz TUI              ║
║  rust-gdb target/debug/prog     Rust con pretty-printers  ║
║                                                           ║
║  CONDITIONAL BREAKPOINTS                                  ║
║  ───────────────────────                                  ║
║  break LOC if COND              crear condicional         ║
║  condition N COND               añadir condición a #N     ║
║  condition N                    eliminar condición        ║
║                                                           ║
║  WATCHPOINTS                                              ║
║  ───────────                                              ║
║  watch VAR                      detenerse al cambiar      ║
║  watch VAR if COND              con condición             ║
║  rwatch VAR                     detenerse al leer         ║
║  awatch VAR                     leer o escribir           ║
║  watch *0xADDR                  dirección específica       ║
║                                                           ║
║  CATCHPOINTS                                              ║
║  ───────────                                              ║
║  catch fork/exec/signal/syscall/load                      ║
║                                                           ║
║  BREAKPOINT COMMANDS                                      ║
║  ────────────────────                                     ║
║  commands N                                               ║
║  > silent                       suprimir mensaje          ║
║  > printf "x=%d\n", x          imprimir                  ║
║  > continue                     no detenerse              ║
║  > end                                                    ║
║                                                           ║
║  EXAMINAR MEMORIA                                         ║
║  ────────────────                                         ║
║  x/Nfu ADDR                    N=count, f=format, u=unit ║
║  x/16xb &var                   16 bytes hex              ║
║  x/4wd &arr                    4 words decimal            ║
║  x/s str_ptr                   string                     ║
║  x/5i $pc                      instrucciones              ║
║                                                           ║
║  CORE DUMPS                                               ║
║  ──────────                                               ║
║  ulimit -c unlimited            habilitar                 ║
║  gdb ./prog core                analizar                  ║
║  coredumpctl gdb                systemd                   ║
║                                                           ║
║  THREADS                                                  ║
║  ───────                                                  ║
║  info threads                   listar threads            ║
║  thread N                       cambiar a thread N        ║
║  thread apply all bt            bt de todos los threads   ║
║  break LOC thread N             bp solo en thread N       ║
║                                                           ║
║  REMOTE                                                   ║
║  ──────                                                   ║
║  gdbserver :PORT ./prog         en el target              ║
║  target remote HOST:PORT        desde el host             ║
║  attach PID / detach            conectar/soltar proceso   ║
║                                                           ║
║  PYTHON                                                   ║
║  ──────                                                   ║
║  python CÓDIGO                  ejecutar Python inline    ║
║  source script.py               cargar script             ║
║  gdb.parse_and_eval("expr")    evaluar expresión C       ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Encontrar corrupción de memoria con watchpoints

Compila y depura este programa. Usa un watchpoint para encontrar exactamente qué línea corrompe el array:

```c
// corruption.c
#include <stdio.h>
#include <string.h>

void init_buffer(int *buf, int size) {
    for (int i = 0; i <= size; i++) {  // ¿ves el bug?
        buf[i] = i * 10;
    }
}

void process(int *data, int n) {
    int temp[8];
    init_buffer(temp, 8);
    // temp tiene 8 elementos, pero init_buffer escribe 9 (off-by-one)
    // El byte extra puede pisar variables en el stack

    for (int i = 0; i < n; i++) {
        data[i] += temp[i % 8];
    }
}

int main() {
    int values[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    process(values, 8);

    for (int i = 0; i < 8; i++) {
        printf("values[%d] = %d\n", i, values[i]);
    }

    return 0;
}
```

Pasos:
1. Compila con `gcc -g -O0 -o corruption corruption.c`.
2. En GDB, pon un breakpoint en `init_buffer`.
3. Pon un watchpoint en `temp[8]` (fuera de bounds).
4. Continúa y observa cuándo se escribe fuera del array.
5. Examina el stack con `x/20xw $rsp` para ver el daño.

**Pregunta de reflexión**: ¿Por qué este bug podría no causar un crash visible? ¿Qué variable del stack se sobreescribe y cómo depende del layout decidido por el compilador?

---

### Ejercicio 2: Tracing automático con breakpoint commands

Usa breakpoint commands para crear un log de ejecución de este programa sin modificar el código fuente:

```c
// server_sim.c
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int id;
    const char *method;
    const char *path;
    int status;
} Request;

Request* create_request(int id, const char *method, const char *path) {
    Request *r = malloc(sizeof(Request));
    r->id = id;
    r->method = method;
    r->path = path;
    r->status = 0;
    return r;
}

void handle_request(Request *r) {
    if (r->path[0] != '/') {
        r->status = 400;
    } else if (strcmp(r->method, "GET") == 0) {
        r->status = 200;
    } else if (strcmp(r->method, "POST") == 0) {
        r->status = 201;
    } else {
        r->status = 405;
    }
}

void log_request(Request *r) {
    printf("[%d] %s %s → %d\n", r->id, r->method, r->path, r->status);
}

int main() {
    const char *methods[] = {"GET", "POST", "DELETE", "GET", "GET"};
    const char *paths[] = {"/index", "/users", "/admin", "invalid", "/data"};

    for (int i = 0; i < 5; i++) {
        Request *r = create_request(i, methods[i], paths[i]);
        handle_request(r);
        log_request(r);
        free(r);
    }

    return 0;
}
```

Configura GDB para que imprima automáticamente:
1. En `create_request`: el id, method y path de cada request creada.
2. En `handle_request` (al salir): el status asignado.
3. Cuenta cuántas veces se llama a `malloc` y `free`.

Todo sin detener la ejecución (usa `silent` + `continue`).

**Pregunta de reflexión**: ¿En qué situaciones es más útil este "tracing con GDB" que añadir `printf` al código? Piensa en código que no puedes modificar (librería, producción, binario sin fuente).

---

### Ejercicio 3: Debugging multi-thread

Compila y depura este programa que tiene un data race:

```c
// race.c
#include <stdio.h>
#include <pthread.h>

int shared_counter = 0;

void* increment(void *arg) {
    int id = *(int*)arg;
    for (int i = 0; i < 100000; i++) {
        shared_counter++;  // data race
    }
    printf("Thread %d finished, counter = %d\n", id, shared_counter);
    return NULL;
}

int main() {
    pthread_t threads[4];
    int ids[4] = {0, 1, 2, 3};

    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, increment, &ids[i]);
    }

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Final counter: %d (expected 400000)\n", shared_counter);
    return 0;
}
```

Pasos:
1. Compila: `gcc -g -O0 -pthread -o race race.c`.
2. Ejecuta varias veces y observa que el resultado varía.
3. En GDB:
   - Usa `info threads` para ver los threads.
   - Pon un watchpoint en `shared_counter`.
   - Usa `thread apply all bt` cuando se detenga.
   - Usa `set scheduler-locking on` para avanzar solo un thread.
4. Identifica dónde ocurre la race condition a nivel de instrucciones (`layout asm`).

**Pregunta de reflexión**: ¿Por qué `shared_counter++` no es atómico a nivel de instrucciones? ¿Cuántas instrucciones de CPU se ejecutan realmente? Usa `disassemble increment` en GDB para verlo.
