# T01 — GDB básico

> **Errata del material base**
>
> Sin erratas detectadas. El material teórico y los laboratorios son factualmente correctos.

---

## 1. Qué es GDB

GDB (GNU Debugger) permite ejecutar un programa paso a paso, inspeccionar variables, poner breakpoints y examinar el estado del programa cuando crashea:

```bash
# Requisito: compilar con -g (información de debug):
gcc -g -O0 main.c -o main

# -g   incluye nombres de variables, líneas de código, tipos
# -O0  sin optimización (el código corresponde 1:1 con el fuente)

# Sin -g: GDB funciona pero no muestra código fuente ni nombres
# Con -O2: GDB funciona pero variables pueden estar eliminadas
#          y el código reordenado — confuso para depurar
```

---

## 2. Iniciar, ejecutar y salir

```bash
gdb ./main                   # iniciar GDB con un programa
gdb -ex run ./main           # iniciar y ejecutar directamente
gdb --args ./main arg1 arg2  # iniciar con argumentos para el programa
```

```bash
# Dentro de GDB:
(gdb) run                    # ejecutar el programa
(gdb) run arg1 arg2          # ejecutar con argumentos
(gdb) run < input.txt        # redirigir stdin
(gdb) set environment DEBUG=1  # variable de entorno
(gdb) quit                   # salir (o Ctrl-D)
```

### Abreviaturas

GDB tiene abreviaturas para casi todo:

| Comando | Abreviatura |
|---------|-------------|
| `break` | `b` |
| `run` | `r` |
| `continue` | `c` |
| `next` | `n` |
| `step` | `s` |
| `finish` | `fin` |
| `print` | `p` |
| `backtrace` | `bt` |
| `info` | `i` |
| `list` | `l` |
| `quit` | `q` |
| `frame` | `f` |
| `until` | `u` |
| `delete` | `d` |

---

## 3. Breakpoints

Un breakpoint pausa la ejecución en una línea específica:

```bash
(gdb) break main              # al inicio de main()
(gdb) break 15                # en la línea 15 del archivo actual
(gdb) break math.c:42         # en la línea 42 de math.c
(gdb) break add               # al inicio de la función add()
```

### Breakpoints condicionales

```bash
(gdb) break 20 if i == 5      # solo cuando i vale 5
(gdb) break add if n > 100    # solo si n > 100
```

### Gestionar breakpoints

```bash
(gdb) info breakpoints         # listar todos (abrev: i b)
(gdb) disable 2               # desactivar breakpoint 2 (sin borrarlo)
(gdb) enable 2                # reactivar
(gdb) delete 2                # eliminar breakpoint 2
(gdb) delete                   # eliminar todos
```

### Breakpoints temporales

```bash
(gdb) tbreak 30               # parar en línea 30 una sola vez
(gdb) break *0x00401234       # breakpoint en dirección de memoria
```

---

## 4. Ejecución paso a paso

Una vez parado en un breakpoint:

```bash
(gdb) next                    # ejecutar línea actual SIN entrar en funciones
(gdb) step                    # ejecutar línea actual ENTRANDO en funciones
(gdb) finish                  # ejecutar hasta que la función actual retorne
(gdb) continue                # continuar hasta el próximo breakpoint (o final)
(gdb) until 25                # ejecutar hasta la línea 25 (útil para saltar loops)
(gdb) next 5                  # ejecutar 5 líneas de una vez (sin entrar)
```

### next vs step — la diferencia clave

```c
// debug_basic.c (labs/)
int factorial(int n) {         // línea 12
    int result = 1;            // línea 13
    for (int i = 1; i <= n; i++) {
        result *= i;           // línea 15
    }
    return result;             // línea 17
}

int main(void) {               // línea 28
    int num = 6;               // línea 29
    int fact = factorial(num); // línea 30  ← parado aquí
    ...
}
```

- **`next`** en línea 30 → ejecuta `factorial()` como caja negra → paras en línea 31
- **`step`** en línea 30 → **entras** en `factorial()` → paras en línea 13 (dentro de factorial)
- **`finish`** dentro de factorial → ejecuta hasta que retorne → vuelves a línea 30 con el resultado

---

## 5. Inspeccionar variables

```bash
(gdb) print x                 # valor de x (decimal)
(gdb) print arr[3]            # elemento 3 del array
(gdb) print *p                # desreferenciar puntero
(gdb) print &x                # dirección de memoria de x
```

### Formatos de impresión

```bash
(gdb) print/x x               # hexadecimal
(gdb) print/o x               # octal
(gdb) print/t x               # binario
(gdb) print/c x               # como carácter
(gdb) print/f x               # como float
(gdb) print/s str             # como string
```

### Structs y arrays

```bash
(gdb) print p                  # struct completo: {x = 10, y = 20}
(gdb) print p.x                # campo individual
(gdb) print *arr@5             # 5 elementos consecutivos desde arr
(gdb) print *ptr@10            # 10 elementos desde un puntero
```

### Modificar variables en runtime

```bash
(gdb) set var x = 100          # cambiar valor sin recompilar
(gdb) set var n = 3            # útil para probar con valores diferentes
```

---

## 6. Examinar memoria (comando `x`)

```bash
# Formato: x/NFU dirección
# N = cuántas unidades
# F = formato (x=hex, d=decimal, s=string, i=instrucciones)
# U = tamaño (b=byte, h=halfword/2B, w=word/4B, g=giant/8B)

(gdb) x/4xw &x                # 4 words en hex desde &x
(gdb) x/s str                 # como string
(gdb) x/10xb &x               # 10 bytes en hex desde &x
(gdb) x/5i main               # 5 instrucciones desde main
```

---

## 7. Backtrace (call stack)

Cuando el programa crashea o está parado en un breakpoint, `backtrace` muestra la cadena de llamadas:

```bash
(gdb) backtrace                # o bt
# #0  access_data (p=0x0) at debug_crash.c:15
# #1  process (data=0x0) at debug_crash.c:20
# #2  caller () at debug_crash.c:26
# #3  main () at debug_crash.c:32

# Frame #0 es donde estás ahora (la función que crasheó)
# Frame #3 es main (el inicio de la cadena)
# Se lee de abajo (main) hacia arriba (crash)
```

```bash
# Navegar entre frames:
(gdb) frame 1                  # moverse al frame de process()
(gdb) print data               # ver variables de process()
(gdb) frame 0                  # volver al frame actual

# Variantes:
(gdb) bt 3                     # solo los 3 frames más recientes
(gdb) bt full                  # backtrace con todas las variables locales
```

Ver `labs/debug_crash.c` — cadena `main()→caller()→process()→access_data()` con NULL pointer para practicar backtrace.

---

## 8. Watchpoints

Un watchpoint pausa la ejecución cuando una variable **cambia de valor**:

```bash
(gdb) watch x                  # parar cuando x cambie
# Hardware watchpoint 1: x

(gdb) continue
# Hardware watchpoint 1: x
# Old value = 5
# New value = 10
```

```bash
# Variantes:
(gdb) rwatch x                 # parar cuando se LEA la variable
(gdb) awatch x                 # parar cuando se lea O escriba
(gdb) watch x if x > 100      # solo pausar si x supera 100
```

Útiles para: encontrar quién modifica una variable, detectar corrupción de memoria, depurar loops que no terminan.

Ver `labs/debug_watch.c` — bug donde `counter` se vuelve negativo en un nested loop.

---

## 9. Ver código fuente y estado

```bash
(gdb) list                     # 10 líneas alrededor de la posición actual
(gdb) list 20                  # alrededor de la línea 20
(gdb) list main                # inicio de main()
(gdb) list math.c:10           # línea 10 de math.c
(gdb) list 1,30                # líneas 1 a 30
```

```bash
# Información del estado:
(gdb) info locals              # variables locales del frame actual
(gdb) info args                # argumentos de la función actual
(gdb) info breakpoints         # todos los breakpoints
(gdb) info watchpoints         # todos los watchpoints
(gdb) info registers           # registros del CPU
(gdb) info threads             # threads del programa
```

---

## 10. Core dumps

Cuando un programa crashea, el sistema puede guardar un "core dump" — imagen de la memoria del proceso en el momento del crash:

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
# Muestra exactamente dónde crasheó, como si lo hubieras
# ejecutado dentro de GDB
```

Útiles cuando: el crash ocurre en producción, el crash es intermitente, o se necesita análisis post-mortem.

---

## 11. TUI (Text User Interface)

```bash
(gdb) tui enable               # o Ctrl-X A — código fuente arriba, prompt abajo
(gdb) tui disable              # volver al modo normal

# Layouts:
(gdb) layout src               # solo código fuente
(gdb) layout asm               # solo assembly
(gdb) layout split             # fuente + assembly
(gdb) layout regs              # registros + fuente
```

---

## 12. Tabla de comandos

| Comando | Abrev. | Qué hace |
|---------|--------|----------|
| `run [args]` | `r` | Ejecutar el programa |
| `break lugar` | `b` | Poner breakpoint |
| `tbreak lugar` | `tb` | Breakpoint temporal (una sola vez) |
| `continue` | `c` | Continuar hasta siguiente breakpoint |
| `next` | `n` | Siguiente línea (sin entrar en funciones) |
| `step` | `s` | Siguiente línea (entrando en funciones) |
| `finish` | `fin` | Ejecutar hasta que retorne la función |
| `until N` | `u N` | Ejecutar hasta la línea N |
| `print var` | `p` | Imprimir valor de variable |
| `print/x var` | `p/x` | Imprimir en hexadecimal |
| `x/NFU addr` | | Examinar memoria |
| `backtrace` | `bt` | Mostrar call stack |
| `frame N` | `f N` | Cambiar al frame N |
| `watch var` | | Watchpoint (pausar cuando cambie) |
| `info locals` | `i lo` | Variables locales |
| `info args` | `i ar` | Argumentos de función |
| `info breakpoints` | `i b` | Listar breakpoints |
| `list` | `l` | Mostrar código fuente |
| `set var X = V` | | Cambiar valor de variable |
| `delete N` | `d N` | Eliminar breakpoint N |
| `quit` | `q` | Salir de GDB |

---

## 13. Archivos del laboratorio

| Archivo | Descripción |
|---------|-------------|
| `labs/debug_basic.c` | `factorial()` y `process_array()` — breakpoints, step/next, print |
| `labs/debug_crash.c` | Crash por NULL pointer: `main→caller→process→access_data` — backtrace |
| `labs/debug_watch.c` | Nested loop con bug: `counter` se vuelve negativo — watchpoints |

---

## Ejercicios

### Ejercicio 1 — Primer breakpoint y run

Compila `debug_basic.c` con símbolos de debug, ábrelo en GDB, pon un breakpoint en `main` y ejecuta:

```bash
cd labs/
gcc -g -O0 debug_basic.c -o debug_basic
gdb ./debug_basic
```

```
(gdb) break main
(gdb) run
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿En qué línea se detendrá GDB? ¿Se habrá ejecutado alguna instrucción de main()?</summary>

GDB se detiene en la **línea 29**: `int num = 6;` — la primera línea ejecutable de `main()`.

```
Breakpoint 1, main () at debug_basic.c:29
29	    int num = 6;
```

El breakpoint en una función para en su primera línea ejecutable. La línea 29 **no se ha ejecutado aún** — `num` todavía no tiene el valor 6. Si haces `print num` ahora, mostrará un valor indeterminado (posiblemente 0 en debug mode).

</details>

---

### Ejercicio 2 — next vs step: entrar en factorial

Desde el breakpoint en main (línea 29), avanza con `next` hasta la línea 30 (`int fact = factorial(num);`).

**Predicción**: Antes de ejecutar, responde:

<details><summary>Si haces step en la línea 30, ¿dónde terminas? ¿Y si haces next?</summary>

- **`step`** → entras **dentro** de `factorial()`. GDB para en la línea 13: `int result = 1;`, mostrando `factorial (n=6)`. Puedes inspeccionar `n`, `result`, etc.

- **`next`** → ejecuta `factorial()` como caja negra. GDB para en la línea 31 (la línea siguiente). `factorial` ya se ejecutó completamente y `fact` tiene el valor 720.

Para salir de `factorial()` después de `step`, usa **`finish`** — ejecuta hasta que la función retorne y muestra el valor devuelto (`Value returned is $N = 720`).

</details>

Prueba ambas opciones (tendrás que reiniciar con `run` para probar la segunda).

---

### Ejercicio 3 — Inspeccionar el loop de factorial

Entra en `factorial()` con `step` y avanza con `next` por el loop. En cada iteración, imprime `i` y `result`:

```
(gdb) step        # entrar en factorial
(gdb) next        # ejecutar int result = 1
(gdb) next        # entrar al for
(gdb) next        # ejecutar result *= i
(gdb) print i
(gdb) print result
```

**Predicción**: Antes de inspeccionar, responde:

<details><summary>¿Qué valores tendrán i y result tras ejecutar result *= i en las primeras 3 iteraciones?</summary>

| Iteración | `i` | `result` antes | `result *= i` | `result` después |
|-----------|-----|----------------|----------------|------------------|
| 1 | 1 | 1 | 1 * 1 | 1 |
| 2 | 2 | 1 | 1 * 2 | 2 |
| 3 | 3 | 2 | 2 * 3 | 6 |

Después de la iteración 3, `result = 6` (que es 3!). El loop continúa hasta i=6, donde `result = 720` (6!).

Si haces `finish` desde cualquier punto dentro de factorial, GDB ejecuta el resto de la función y muestra `Value returned is $N = 720`.

</details>

---

### Ejercicio 4 — Inspeccionar un array con print *arr@N

Pon un breakpoint en `process_array` y ejecuta:

```
(gdb) break process_array
(gdb) run
```

Cuando pare en `process_array`, examina los argumentos:

```
(gdb) info args
(gdb) print *arr@5
(gdb) print/x arr[0]
```

**Predicción**: Antes de inspeccionar, responde:

<details><summary>¿Qué mostrará info args? ¿Y print *arr@5?</summary>

`info args` mostrará:
```
arr = 0x7fffffffd... (dirección del array)
n = 5
```

`arr` es un **puntero** — `info args` muestra la dirección, no el contenido. Para ver los elementos hay que desreferenciar.

`print *arr@5` mostrará:
```
$1 = {10, 20, 30, 40, 50}
```

La sintaxis `*arr@5` significa: desreferenciar `arr` y mostrar 5 elementos consecutivos. Es la forma de GDB de imprimir un array pasado como puntero.

`print/x arr[0]` mostrará `$2 = 0xa` (10 en hex).

</details>

---

### Ejercicio 5 — Modificar una variable con set var

Estando parado en `process_array` (tras el ejercicio anterior), cambia `n` de 5 a 3 y continúa:

```
(gdb) print n
(gdb) set var n = 3
(gdb) print n
(gdb) continue
```

**Predicción**: Antes de continuar, responde:

<details><summary>¿Qué imprimirá el programa como "sum of array"?</summary>

**60** en vez de 150.

Con `n = 3`, el loop `for (int i = 0; i < n; i++)` solo itera 3 veces, sumando `arr[0] + arr[1] + arr[2] = 10 + 20 + 30 = 60`.

```
sum of array = 60
done
```

`set var` modifica el valor **en runtime** sin recompilar. Es útil para probar hipótesis rápidamente: "¿qué pasa si n fuera 3?" sin cambiar el código fuente.

</details>

---

### Ejercicio 6 — Depurar un segfault con backtrace

Compila y ejecuta `debug_crash.c` dentro de GDB:

```bash
gcc -g -O0 debug_crash.c -o debug_crash
gdb ./debug_crash
```

```
(gdb) run
```

**Predicción**: Antes de ejecutar, lee el código de `debug_crash.c` (cadena: `main→caller→process→access_data`). Responde:

<details><summary>¿En qué función y línea crasheará? ¿Qué mostrará bt?</summary>

Crashea en **`access_data()`**, línea 15: `int value = *p;` — desreferencia de NULL.

GDB muestra:
```
Program received signal SIGSEGV, Segmentation fault.
0x... in access_data (p=0x0) at debug_crash.c:15
```

El backtrace (`bt`) muestra la cadena completa:
```
#0  access_data (p=0x0) at debug_crash.c:15
#1  process (data=0x0) at debug_crash.c:20
#2  caller () at debug_crash.c:26
#3  main () at debug_crash.c:32
```

Se lee de abajo hacia arriba: `main()` llamó a `caller()` (línea 32), que llamó a `process(ptr)` (línea 26), que llamó a `access_data(data)` (línea 20), que crasheó al desreferenciar `p` (línea 15).

La causa raíz está en `caller()` (línea 25): `int *ptr = NULL;` — declaró un puntero NULL y lo pasó sin verificar.

</details>

---

### Ejercicio 7 — Navegar frames para rastrear el NULL

Después del crash del ejercicio anterior, usa `frame` para subir por la cadena:

```
(gdb) frame 0
(gdb) print p
(gdb) frame 1
(gdb) print data
(gdb) frame 2
(gdb) print ptr
```

**Predicción**: Antes de ejecutar, responde:

<details><summary>¿Qué valor mostrará print en cada frame? ¿En qué frame se origina el NULL?</summary>

- **Frame 0** (`access_data`): `p = (int *) 0x0` — el puntero que causó el crash
- **Frame 1** (`process`): `data = (int *) 0x0` — recibió NULL de caller y lo pasó sin verificar
- **Frame 2** (`caller`): `ptr = (int *) 0x0` — **aquí se origina el NULL** (línea 25: `int *ptr = NULL;`)

El NULL se propaga hacia abajo: `caller` lo crea → `process` lo recibe como `data` → `access_data` lo recibe como `p` y crashea al hacer `*p`.

Con tres comandos (`bt`, `frame`, `print`) se identificó dónde crasheó, por qué, y quién originó el problema. Este es el flujo fundamental de depuración con GDB.

</details>

---

### Ejercicio 8 — Watchpoint simple en counter

Compila y abre `debug_watch.c` en GDB:

```bash
gcc -g -O0 debug_watch.c -o debug_watch
gdb ./debug_watch
```

```
(gdb) break main
(gdb) run
(gdb) watch counter
(gdb) continue
```

**Predicción**: Antes de continuar, lee el código. El loop externo hace `counter += 3` y el interno resta 7 cuando `(i + j) % 5 == 0`. Responde:

<details><summary>¿Cuáles serán los primeros 5 cambios de counter que GDB reportará?</summary>

El watchpoint reporta **cada** cambio de `counter`:

| # | Old → New | Causa | `i` |
|---|-----------|-------|-----|
| 1 | 0 → 3 | `counter += 3` | 0 |
| 2 | 3 → 6 | `counter += 3` | 1 |
| 3 | 6 → 9 | `counter += 3` | 2 |
| 4 | 9 → 12 | `counter += 3` | 3 |
| 5 | 12 → 5 | `counter -= 7` | 3 (j=2, (3+2)%5==0) |

El cambio #5 es la primera resta: en i=3, j=2, la condición `(3+2) % 5 == 0` es verdadera, así que se ejecuta `counter -= 7` (12 - 7 = 5).

Para ver todos los cambios sin parar en cada uno, usa `continue` repetidamente.

</details>

---

### Ejercicio 9 — Watchpoint condicional: encontrar el bug

Reinicia el programa y pon un watchpoint condicional para ir directo al primer valor negativo:

```
(gdb) delete                   # eliminar todos los watchpoints
(gdb) break main
(gdb) run                      # responde y para reiniciar
(gdb) watch counter if counter < 0
(gdb) continue
```

**Predicción**: Antes de continuar, traza mentalmente el programa. Responde:

<details><summary>¿En qué iteración (valor de i y j) counter se vuelve negativo por primera vez? ¿De qué valor a cuál?</summary>

Traza completa de `counter`:

| `i` | Después de `+= 3` | Restas internas (`-= 7`) | Final de iteración |
|-----|-------|---------|---------|
| 0 | 3 | ninguna (j<0) | 3 |
| 1 | 6 | ninguna | 6 |
| 2 | 9 | ninguna | 9 |
| 3 | 12 | j=2: (3+2)%5==0 → 12-7=5 | 5 |
| 4 | 8 | j=1: (4+1)%5==0 → 8-7=1 | 1 |
| 5 | **4** | **j=0: (5+0)%5==0 → 4-7=-3** | **-3** |

La primera transición a negativo ocurre en **i=5, j=0**: `counter` pasa de **4 a -3**.

GDB reporta:
```
Hardware watchpoint: counter
Old value = 4
New value = -3
```

Después puedes hacer `print i` → 5 y `print j` → 0 para confirmar.

</details>

---

### Ejercicio 10 — Depurar un segfault con core dump

Habilita core dumps, ejecuta `debug_crash` fuera de GDB, y analiza el core:

```bash
ulimit -c unlimited
./debug_crash
# Segmentation fault (core dumped)
```

Luego abre el core dump (en Fedora con systemd, usa `coredumpctl`):

```bash
# Opción A — core dump tradicional:
gdb ./debug_crash core

# Opción B — Fedora/systemd:
coredumpctl gdb debug_crash
```

Dentro de GDB:

```
(gdb) bt
(gdb) frame 2
(gdb) info locals
```

**Predicción**: Antes de analizar, responde:

<details><summary>¿Mostrará bt la misma información que cuando ejecutaste dentro de GDB? ¿Qué mostrará info locals en frame 2?</summary>

**Sí**, `bt` muestra exactamente la misma cadena:
```
#0  access_data (p=0x0) at debug_crash.c:15
#1  process (data=0x0) at debug_crash.c:20
#2  caller () at debug_crash.c:26
#3  main () at debug_crash.c:32
```

El core dump es una "fotografía" del estado del proceso al momento del crash — contiene la misma información de stack, variables y memoria que tendría GDB interactivo.

`info locals` en frame 2 (`caller`) mostrará:
```
ptr = (int *) 0x0
answer = <valor indeterminado>
```

`ptr` es NULL (la causa raíz). `answer` nunca se asignó porque `process()` crasheó antes de retornar.

Los core dumps son esenciales para depuración **post-mortem**: cuando el crash ocurre en producción, es intermitente, o no puedes ejecutar GDB interactivamente. Compilar siempre con `-g` permite análisis posterior sin recompilar.

</details>

Limpieza final:

```bash
rm -f debug_basic debug_crash debug_watch
```
