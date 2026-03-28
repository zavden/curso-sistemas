# T01 — GDB básico

## Qué es GDB

GDB (GNU Debugger) permite ejecutar un programa paso a paso,
inspeccionar variables, poner breakpoints y examinar el estado
del programa cuando crashea. Es la herramienta fundamental
de diagnóstico en C:

```bash
# Requisito: compilar con -g (información de debug):
gcc -g -O0 main.c -o main

# -g  incluye nombres de variables, líneas de código, tipos
# -O0 sin optimización (el código corresponde 1:1 con el fuente)

# Sin -g, GDB funciona pero no muestra código fuente ni nombres
# Con -O2, GDB funciona pero las variables pueden estar eliminadas
# y el código reordenado — confuso para depurar
```

## Iniciar y salir

```bash
# Iniciar GDB con un programa:
gdb ./main

# Iniciar y ejecutar directamente:
gdb -ex run ./main

# Iniciar con argumentos para el programa:
gdb --args ./main arg1 arg2

# Dentro de GDB:
(gdb) run                  # ejecutar el programa
(gdb) run arg1 arg2        # ejecutar con argumentos
(gdb) quit                 # salir (o Ctrl-D)
(gdb) q                    # abreviatura de quit
```

```bash
# GDB tiene abreviaturas para casi todo:
# break    → b
# run      → r
# continue → c
# next     → n
# step     → s
# print    → p
# backtrace → bt
# info     → i
# list     → l
# quit     → q
```

## Breakpoints

Un breakpoint pausa la ejecución en una línea específica:

```bash
(gdb) break main              # parar al inicio de main()
(gdb) break 15                # parar en la línea 15 del archivo actual
(gdb) break math.c:42         # parar en la línea 42 de math.c
(gdb) break add               # parar al inicio de la función add()
```

```bash
# Breakpoints condicionales — solo pausan si se cumple la condición:
(gdb) break 20 if i == 5      # parar en línea 20 solo cuando i vale 5
(gdb) break add if n > 100    # parar en add() solo si n > 100
```

```bash
# Gestionar breakpoints:
(gdb) info breakpoints         # listar todos los breakpoints (o: i b)
# Num  Type       Disp  Enb  Address    What
# 1    breakpoint keep  y    0x00401234 in main at main.c:10
# 2    breakpoint keep  y    0x00401280 in add at math.c:5

(gdb) disable 2               # desactivar breakpoint 2 (sin borrarlo)
(gdb) enable 2                # reactivar breakpoint 2
(gdb) delete 2                # eliminar breakpoint 2
(gdb) delete                   # eliminar todos los breakpoints
```

```bash
# Breakpoints temporales — se borran después de la primera parada:
(gdb) tbreak 30               # parar en línea 30 una sola vez

# Breakpoints en direcciones de memoria (avanzado):
(gdb) break *0x00401234       # parar en una dirección específica
```

## Ejecución paso a paso

Una vez parado en un breakpoint, hay varias formas de avanzar:

```bash
(gdb) next                    # ejecutar la línea actual, SIN entrar en funciones
(gdb) step                    # ejecutar la línea actual, ENTRANDO en funciones
(gdb) finish                  # ejecutar hasta que la función actual retorne
(gdb) continue                # continuar hasta el próximo breakpoint (o el final)
```

### Diferencia entre next y step

```c
// main.c
int add(int a, int b) {    // línea 3
    return a + b;           // línea 4
}                           // línea 5

int main(void) {            // línea 7
    int x = 5;              // línea 8
    int y = add(x, 3);      // línea 9  ← parado aquí
    printf("%d\n", y);       // línea 10
    return 0;                // línea 11
}
```

```bash
# Si estás parado en la línea 9:

(gdb) next
# Ejecuta add(x, 3) como una caja negra
# Ahora estás en la línea 10

# Si en cambio haces:
(gdb) step
# ENTRA en la función add()
# Ahora estás en la línea 4 (dentro de add)

(gdb) finish
# Ejecuta hasta que add() retorne
# Ahora estás de vuelta en la línea 9 (con el resultado)
```

```bash
# Ejecutar N líneas de una vez:
(gdb) next 5                 # ejecutar las próximas 5 líneas (sin entrar)
(gdb) step 3                 # avanzar 3 pasos (entrando en funciones)
```

```bash
# until — ejecutar hasta una línea específica:
(gdb) until 25               # ejecutar hasta la línea 25
# Útil para saltar un loop sin poner breakpoints
```

## Inspeccionar variables

```bash
(gdb) print x                # imprimir el valor de x
# $1 = 42

(gdb) print arr[3]           # imprimir el elemento 3 del array
# $2 = 7

(gdb) print *p               # desreferenciar puntero
# $3 = 42

(gdb) print &x               # dirección de memoria de x
# $4 = (int *) 0x7fffffffe4c4
```

### Formatos de impresión

```bash
(gdb) print x                # decimal (por defecto)
(gdb) print/x x              # hexadecimal
(gdb) print/o x              # octal
(gdb) print/t x              # binario
(gdb) print/c x              # como carácter
(gdb) print/f x              # como float
(gdb) print/s str            # como string

# Ejemplo:
(gdb) print x
# $1 = 65
(gdb) print/x x
# $2 = 0x41
(gdb) print/c x
# $3 = 65 'A'
(gdb) print/t x
# $4 = 1000001
```

### Inspeccionar structs y arrays

```c
struct Point { int x; int y; };
struct Point p = {10, 20};
int arr[5] = {1, 2, 3, 4, 5};
```

```bash
(gdb) print p                 # imprimir struct completo
# $1 = {x = 10, y = 20}

(gdb) print p.x               # campo individual
# $2 = 10

(gdb) print arr               # imprimir array completo
# $3 = {1, 2, 3, 4, 5}

(gdb) print *arr@5            # imprimir 5 elementos desde arr
# $4 = {1, 2, 3, 4, 5}

# Para un puntero a varios elementos:
(gdb) print *ptr@10           # imprimir 10 elementos desde ptr
```

### Modificar variables en runtime

```bash
(gdb) set var x = 100         # cambiar el valor de x
(gdb) print x
# $1 = 100

# Útil para probar qué pasa con valores diferentes
# sin recompilar
```

## Examinar memoria

```bash
# x (examine) muestra memoria raw:
(gdb) x/4xw &x               # 4 words en hexadecimal desde &x
# 0x7fffffffe4c4: 0x0000002a 0x00000003 0x00000000 0x00000000

# Formato: x/NFU dirección
# N = cuántas unidades
# F = formato (x=hex, d=decimal, s=string, i=instrucciones)
# U = tamaño (b=byte, h=halfword/2bytes, w=word/4bytes, g=giant/8bytes)
```

```bash
(gdb) x/s str                 # imprimir como string
# 0x404010: "Hello, world!"

(gdb) x/10xb &x               # 10 bytes en hex desde &x
# 0x7fffffffe4c4: 0x2a 0x00 0x00 0x00 0x03 0x00 0x00 0x00 ...

(gdb) x/5i main               # 5 instrucciones desde main
# 0x401136 <main>:     push   %rbp
# 0x401137 <main+1>:   mov    %rsp,%rbp
# ...
```

## Backtrace (call stack)

Cuando un programa crashea o está parado en un breakpoint,
`backtrace` muestra la cadena de llamadas que llegó ahí:

```bash
(gdb) backtrace               # o bt
# #0  add (a=5, b=3) at math.c:4
# #1  0x00401180 in process (x=5) at main.c:15
# #2  0x004011a0 in main () at main.c:22

# Frame #0 es donde estás ahora (add)
# Frame #1 es quien llamó a add (process)
# Frame #2 es quien llamó a process (main)
```

```bash
# Navegar entre frames:
(gdb) frame 1                 # moverse al frame de process()
(gdb) print x                 # ver variables de process()
(gdb) frame 0                 # volver al frame actual
```

```bash
# Backtrace limitado:
(gdb) bt 3                    # solo los 3 frames más recientes
(gdb) bt full                 # backtrace con todas las variables locales
```

### Depurar un segfault

```c
// crash.c
#include <stdio.h>

void bad_function(int *p) {
    printf("%d\n", *p);       // crashea si p es NULL
}

void caller(void) {
    int *ptr = NULL;
    bad_function(ptr);
}

int main(void) {
    caller();
    return 0;
}
```

```bash
gcc -g -O0 crash.c -o crash
gdb ./crash

(gdb) run
# Program received signal SIGSEGV, Segmentation fault.
# 0x0000000000401140 in bad_function (p=0x0) at crash.c:4
# 4	    printf("%d\n", *p);

(gdb) bt
# #0  0x0000000000401140 in bad_function (p=0x0) at crash.c:4
# #1  0x0000000000401160 in caller () at crash.c:9
# #2  0x0000000000401170 in main () at crash.c:13

(gdb) print p
# $1 = (int *) 0x0              ← p es NULL

# Ahora sabes:
# - Dónde crasheó (crash.c:4)
# - Por qué (p es NULL)
# - Quién pasó el NULL (caller, línea 9)
```

## Watchpoints

Un watchpoint pausa la ejecución cuando una variable **cambia
de valor**:

```bash
(gdb) watch x                 # parar cuando x cambie
# Hardware watchpoint 1: x

(gdb) continue
# Hardware watchpoint 1: x
# Old value = 5
# New value = 10
# 0x00401150 in main () at main.c:12

# GDB muestra el valor anterior y el nuevo
```

```bash
# Watchpoint de lectura — parar cuando se LEE la variable:
(gdb) rwatch x

# Watchpoint de acceso — parar cuando se lee O escribe:
(gdb) awatch x

# Watchpoint condicional:
(gdb) watch x if x > 100     # solo pausar si x supera 100
```

```bash
# Watchpoints son útiles para:
# - Encontrar quién modifica una variable global
# - Detectar corrupción de memoria (watch en la dirección)
# - Depurar loops que no terminan (watch en la variable del loop)
```

## Ver código fuente

```bash
(gdb) list                    # mostrar 10 líneas alrededor de la posición actual
(gdb) list 20                 # mostrar alrededor de la línea 20
(gdb) list main               # mostrar el inicio de main()
(gdb) list math.c:10          # mostrar alrededor de la línea 10 de math.c
(gdb) list 1,30               # mostrar líneas 1 a 30
```

## info — Información del estado

```bash
(gdb) info locals             # variables locales del frame actual
# x = 42
# y = 7
# p = 0x7fffffffe4c4

(gdb) info args               # argumentos de la función actual
# a = 5
# b = 3

(gdb) info breakpoints        # todos los breakpoints
(gdb) info watchpoints        # todos los watchpoints
(gdb) info registers          # registros del CPU
(gdb) info threads            # threads del programa
```

## Core dumps

Cuando un programa crashea, el sistema puede guardar un "core
dump" — una imagen de la memoria del proceso en el momento
del crash:

```bash
# Habilitar core dumps:
ulimit -c unlimited

# Ejecutar el programa (que crashea):
./crash
# Segmentation fault (core dumped)

# Analizar el core dump:
gdb ./crash core
# O en sistemas con systemd:
coredumpctl gdb crash

(gdb) bt
# Muestra exactamente dónde crasheó, igual que si lo hubieras
# ejecutado dentro de GDB
```

```bash
# Core dumps son útiles cuando:
# - El crash ocurre en producción (no puedes correr GDB interactivamente)
# - El crash es intermitente (difícil de reproducir)
# - Necesitas analizar el crash después del hecho
```

## Depurar con argumentos y entrada

```bash
# Pasar argumentos al programa:
(gdb) run arg1 arg2

# Redirigir stdin:
(gdb) run < input.txt

# Variables de entorno:
(gdb) set environment DEBUG=1
(gdb) run
```

## Comandos de TUI (Text User Interface)

```bash
# Activar modo TUI — muestra código fuente en una ventana:
(gdb) tui enable              # o Ctrl-X A

# En TUI se ve el código fuente arriba y el prompt abajo.
# La línea actual está resaltada.
# Los breakpoints se marcan con B+.

(gdb) tui disable             # volver al modo normal

# Layouts de TUI:
(gdb) layout src              # solo código fuente
(gdb) layout asm              # solo assembly
(gdb) layout split            # fuente + assembly
(gdb) layout regs             # registros + fuente
```

## Tabla de comandos

| Comando | Abrev. | Qué hace |
|---|---|---|
| run [args] | r | Ejecutar el programa |
| break lugar | b | Poner breakpoint |
| continue | c | Continuar hasta siguiente breakpoint |
| next | n | Siguiente línea (sin entrar en funciones) |
| step | s | Siguiente línea (entrando en funciones) |
| finish | fin | Ejecutar hasta que retorne la función |
| until N | u N | Ejecutar hasta la línea N |
| print var | p | Imprimir valor de variable |
| print/x var | p/x | Imprimir en hexadecimal |
| x/NFU addr | | Examinar memoria |
| backtrace | bt | Mostrar call stack |
| frame N | f N | Cambiar al frame N |
| watch var | | Watchpoint (pausar cuando cambie) |
| info locals | i lo | Variables locales |
| info args | i ar | Argumentos de función |
| info breakpoints | i b | Listar breakpoints |
| list | l | Mostrar código fuente |
| set var X = V | | Cambiar valor de variable |
| delete N | d N | Eliminar breakpoint N |
| quit | q | Salir de GDB |

---

## Ejercicios

### Ejercicio 1 — Breakpoints y step

```c
// Compilar con -g -O0, ejecutar en GDB:
// 1. Poner breakpoint en main
// 2. Ejecutar, avanzar con next hasta la llamada a add
// 3. Usar step para entrar en add
// 4. Imprimir los argumentos con print
// 5. Usar finish para volver a main

#include <stdio.h>

int add(int a, int b) {
    int result = a + b;
    return result;
}

int main(void) {
    int x = 10;
    int y = 20;
    int z = add(x, y);
    printf("%d + %d = %d\n", x, y, z);
    return 0;
}
```

### Ejercicio 2 — Depurar un segfault

```c
// Este programa crashea. Usar GDB para encontrar:
// 1. ¿En qué línea crashea?
// 2. ¿Qué variable causa el crash?
// 3. ¿Quién pasó el valor incorrecto?

#include <stdio.h>
#include <stdlib.h>

void process(int *data, int n) {
    for (int i = 0; i <= n; i++) {
        data[i] = i * 2;
    }
}

int main(void) {
    int *arr = malloc(5 * sizeof(int));
    process(arr, 1000);
    free(arr);
    return 0;
}
```

### Ejercicio 3 — Watchpoints

```c
// Usar un watchpoint para encontrar en qué iteración
// la variable total supera 100.

#include <stdio.h>

int main(void) {
    int total = 0;
    for (int i = 1; i <= 50; i++) {
        total += i;
    }
    printf("Total: %d\n", total);
    return 0;
}

// (gdb) break main
// (gdb) run
// (gdb) watch total if total > 100
// (gdb) continue
// ¿En qué iteración para?
```
