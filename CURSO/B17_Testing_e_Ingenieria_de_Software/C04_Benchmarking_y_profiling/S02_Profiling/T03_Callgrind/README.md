# T03 — Valgrind / Callgrind

> **Bloque 17 · Capítulo 4 · Sección 2 · Tópico 3**
> Profiling determinista por simulación: contar cada instrucción, cada cache miss, cada llamada. Lento pero exacto.

---

## Índice

1. [Valgrind: qué es realmente](#1-valgrind-qué-es-realmente)
2. [La familia de herramientas Valgrind](#2-la-familia-de-herramientas-valgrind)
3. [Sampling vs simulación: las dos filosofías](#3-sampling-vs-simulación-las-dos-filosofías)
4. [Callgrind: contar todo lo que pasa](#4-callgrind-contar-todo-lo-que-pasa)
5. [Instalación y disponibilidad](#5-instalación-y-disponibilidad)
6. [Primera ejecución: callgrind básico](#6-primera-ejecución-callgrind-básico)
7. [Leer la salida con callgrind_annotate](#7-leer-la-salida-con-callgrind_annotate)
8. [Eventos que callgrind puede contar](#8-eventos-que-callgrind-puede-contar)
9. [Simulación de cache con --cache-sim](#9-simulación-de-cache-con---cache-sim)
10. [Simulación de branch predictor con --branch-sim](#10-simulación-de-branch-predictor-con---branch-sim)
11. [Jerarquía L1/LL: qué modela y qué no](#11-jerarquía-l1ll-qué-modela-y-qué-no)
12. [Overhead: por qué callgrind es 20-100x más lento](#12-overhead-por-qué-callgrind-es-20-100x-más-lento)
13. [KCachegrind: visualización interactiva](#13-kcachegrind-visualización-interactiva)
14. [Navegar KCachegrind: callers, callees, source, map](#14-navegar-kcachegrind-callers-callees-source-map)
15. [Cachegrind: solo simulación de cache](#15-cachegrind-solo-simulación-de-cache)
16. [Cachegrind vs Callgrind: cuándo usar cuál](#16-cachegrind-vs-callgrind-cuándo-usar-cuál)
17. [Cachegrind + cg_diff: comparar dos runs](#17-cachegrind--cg_diff-comparar-dos-runs)
18. [Perfilar secciones específicas con client requests](#18-perfilar-secciones-específicas-con-client-requests)
19. [Controlar inicio/paro desde línea de comandos](#19-controlar-iniciopparo-desde-línea-de-comandos)
20. [Filtrar por función, archivo o thread](#20-filtrar-por-función-archivo-o-thread)
21. [Programas multihilo: --separate-threads](#21-programas-multihilo---separate-threads)
22. [Rust bajo Valgrind: cosas a saber](#22-rust-bajo-valgrind-cosas-a-saber)
23. [Iai: benchmarking Rust basado en callgrind](#23-iai-benchmarking-rust-basado-en-callgrind)
24. [Comparación: perf vs callgrind vs flamegraph](#24-comparación-perf-vs-callgrind-vs-flamegraph)
25. [Errores comunes con callgrind](#25-errores-comunes-con-callgrind)
26. [Programa de práctica: cache_victim.c](#26-programa-de-práctica-cache_victimc)
27. [Ejercicios](#27-ejercicios)
28. [Checklist de validación](#28-checklist-de-validación)
29. [Navegación](#29-navegación)

---

## 1. Valgrind: qué es realmente

**Valgrind** no es un profiler ni un detector de leaks. Es un **framework de
instrumentación binaria dinámica** (DBI — Dynamic Binary Instrumentation). Lo que
hace por debajo es mucho más radical que `perf`:

1. Lee las instrucciones máquina del programa (`x86-64`, `aarch64`, etc.).
2. Las traduce a un lenguaje intermedio llamado **VEX IR**.
3. Añade al VEX IR las instrucciones de instrumentación que pide la herramienta
   (contadores, checks de memoria, simulación de cache...).
4. Compila el VEX IR instrumentado de vuelta a código máquina y lo ejecuta en un
   **procesador virtual sintético** (no en la CPU real directamente).

Es decir: tu programa **nunca se ejecuta directamente sobre la CPU real**. Corre dentro
de una VM ligera que valgrind implementa. Cada instrucción pasa por el traductor.

```
                     ┌──────────────┐
                     │  Tu binario  │
                     │  (x86-64)    │
                     └──────┬───────┘
                            │
                            ▼
                     ┌──────────────┐
                     │  VEX IR      │   ◄── traducción
                     └──────┬───────┘
                            │
                            ▼
             ┌──────────────────────────┐
             │ VEX IR + instrumentación │   ◄── la herramienta añade contadores
             └──────┬───────────────────┘
                    │
                    ▼
             ┌──────────────┐
             │ Traducción   │   ◄── vuelta a x86-64
             └──────┬───────┘
                    │
                    ▼
             ┌──────────────┐
             │ CPU real     │   ◄── ejecuta el código instrumentado
             └──────────────┘
```

**Consecuencias**:

- **Lento**: cada instrucción real se traduce y ejecuta con overhead (20-100×).
- **Determinista**: cuenta exactamente lo que ocurre, sin muestreo ni estadística.
- **No depende del PMU**: funciona sin contadores hardware, en VMs, containers, CI.
- **Limitado a ciertas instrucciones**: SSE/AVX antiguos sí, AVX-512 soporte parcial,
  TSX, extensiones muy nuevas pueden dar errores.
- **No mide tiempo real**: mide trabajo realizado (instrucciones, accesos).

Esto es profundamente distinto de `perf` (sampling) y del flamegraph de muestreo del
T02. Callgrind no te dice "la CPU pasó 40% del tiempo aquí". Te dice "aquí se
ejecutaron exactamente 1 834 872 019 instrucciones, de las cuales X se fueron en
cache misses L1".

---

## 2. La familia de herramientas Valgrind

Valgrind es un paraguas que incluye varias herramientas sobre el mismo framework DBI:

| Herramienta   | Función                                                            |
|----------------|--------------------------------------------------------------------|
| **memcheck** (default) | Detector de errores de memoria: uninit reads, OOB, leaks, UAF |
| **callgrind** | Profiler de llamadas + simulación cache + branch predictor        |
| **cachegrind**| Solo simulación cache (más ligero que callgrind)                 |
| **helgrind**  | Detector de data races en pthreads                                |
| **drd**       | Otro detector de data races, enfoque distinto                     |
| **massif**    | Profiler de memoria (heap snapshots)                              |
| **dhat**      | Heap profiler de nueva generación, **lo veremos en T04**         |
| **exp-bbv**   | Basic block vector (experimental)                                 |

Se invocan con `--tool=NOMBRE`:

```bash
valgrind --tool=memcheck  ./program   # default, equivale a solo 'valgrind ./program'
valgrind --tool=callgrind ./program
valgrind --tool=cachegrind ./program
valgrind --tool=massif    ./program
```

En este tópico nos centramos en **callgrind** y **cachegrind**. Memcheck y helgrind los
vimos en capítulos anteriores (debugging, concurrencia). DHAT lo vemos en el próximo
tópico (T04 profiling de memoria).

---

## 3. Sampling vs simulación: las dos filosofías

```
                 SAMPLING (perf)                    SIMULACIÓN (callgrind)
              ═══════════════════                ═══════════════════════════

              ┌─────┬─────┬─────┐              ┌─────────────────────────┐
              │  ↑  │  ↑  │  ↑  │              │ INSTR INSTR INSTR INSTR │
              │ samp│ samp│ samp│              │ INSTR INSTR INSTR INSTR │
              │     │     │     │              │ (todas contadas)        │
              └─────┴─────┴─────┘              └─────────────────────────┘
              Toma fotos periódicas            Cuenta cada cosa que pasa
              Estadística                       Determinista
              Overhead 1-5%                    Overhead 20-100×
              Ruido de muestreo                Sin ruido
              Tiempo real                       Tiempo virtual (instrucciones)
```

Cuándo usar cada uno:

| Situación                                 | Mejor herramienta                |
|--------------------------------------------|----------------------------------|
| Profilear producción en vivo              | `perf` (overhead bajo)           |
| Contar instrucciones exactas              | `callgrind` (determinista)       |
| Detectar cache misses del L1/L2/L3       | `cachegrind` o `perf` con `-e`   |
| Medir tiempo wall clock                   | `perf` (callgrind no mide tiempo)|
| Run muy corto (<100ms)                    | `callgrind` (perf no muestrea bien) |
| Runs reproducibles para CI                | `callgrind` (sin ruido)          |
| Detectar hotspot global                   | `perf` + flamegraph (rápido)     |
| Refinar una función concreta              | `callgrind` (precisión instrucción) |

Callgrind brilla en **runs cortos** y **reproducibilidad**. Un benchmark de 50 ms en
perf daría 5 muestras inútiles. En callgrind da una cuenta exacta. Por eso existe
**iai** en Rust: benchmarks de regresión sin ruido (§23).

---

## 4. Callgrind: contar todo lo que pasa

Callgrind es, en esencia, **cachegrind + call graph**. Mientras simula el programa:

- Cuenta **instrucciones ejecutadas** (`Ir` — instructions retired).
- Cuenta **llamadas a función** (con número de calls por par caller→callee).
- Opcionalmente simula **cache L1/LL** y cuenta misses (`D1mr`, `D1mw`, `DLmr`, `DLmw`).
- Opcionalmente simula **branch predictor** y cuenta branch mispredicts (`Bc`, `Bcm`,
  `Bi`, `Bim`).
- Asocia cada evento al **par fuente** `(file:line, función)` exacto.

El output es `callgrind.out.<PID>`, un archivo de texto formato propio con la
información por función y por línea. No se lee a mano: se procesa con
`callgrind_annotate` (CLI) o `kcachegrind` (GUI).

---

## 5. Instalación y disponibilidad

### Fedora / RHEL
```bash
sudo dnf install valgrind valgrind-tools
sudo dnf install kcachegrind        # GUI
```

### Debian / Ubuntu
```bash
sudo apt install valgrind
sudo apt install kcachegrind
```

### Arch
```bash
sudo pacman -S valgrind
sudo pacman -S kcachegrind
```

### Alpine / minimal images
```bash
apk add valgrind
```

**Nota**: valgrind a veces se retrasa en soportar instrucciones CPU muy nuevas
(AVX-512, AMX). Si tu binario usa `-march=native` en una Zen 5 o Sapphire Rapids,
valgrind puede fallar con:

```
vex amd64: unhandled instruction ...
```

Soluciones:
- Compilar sin `-march=native`: usar `-march=x86-64-v3` o inferior.
- Actualizar valgrind a la última versión (puede requerir compilar desde fuente).
- Para callgrind, compilar con `-mno-avx512f` si tu binario no necesita AVX-512.

Verifica la versión:
```bash
valgrind --version
# valgrind-3.22.0 o superior para soporte moderno
```

---

## 6. Primera ejecución: callgrind básico

Flujo mínimo:

```bash
# Compilar con debug info y optimización moderada
gcc -O2 -g -o program program.c

# Ejecutar bajo callgrind (lento, ~20-50× más lento)
valgrind --tool=callgrind ./program

# Genera: callgrind.out.<PID>
ls callgrind.out.*
```

Ejemplo de salida en stderr durante la ejecución:

```
==12345== Callgrind, a call-graph generating cache profiler
==12345== Copyright (C) 2002-2023, and GNU GPL'd, by Josef Weidendorfer et al.
==12345== Using Valgrind-3.22.0 and LibVEX; rerun with -h for copyright info
==12345== Command: ./program
==12345==
==12345== For interactive control, run 'callgrind_control -h'.
==12345==
... [programa corriendo, muy despacio] ...
==12345==
==12345== Events    : Ir
==12345== Collected : 1834872019
==12345==
==12345== I   refs:      1,834,872,019
```

El número "I refs" es el conteo total de instrucciones ejecutadas. Es **determinista**:
si ejecutas el mismo binario con el mismo input dos veces, obtienes exactamente el mismo
número. Esto es lo que hace callgrind valioso: **reproducibilidad absoluta**.

Para una salida con archivo nombrado:
```bash
valgrind --tool=callgrind \
  --callgrind-out-file=program.callgrind \
  ./program
```

---

## 7. Leer la salida con callgrind_annotate

`callgrind_annotate` procesa el archivo `callgrind.out.*` y produce un reporte texto:

```bash
callgrind_annotate callgrind.out.12345
```

Salida típica:

```
--------------------------------------------------------------------------------
Profile data file 'callgrind.out.12345' (creator: callgrind-3.22.0)
--------------------------------------------------------------------------------
I1 cache:
D1 cache:
LL cache:
Timerange: Basic block 0 - 1283719
Trigger: Program termination
Profiled target:  ./program
Events recorded:  Ir
Events shown:     Ir
Event sort order: Ir
Thresholds:       99
Include dirs:
User annotated:
Auto-annotation:  off

--------------------------------------------------------------------------------
            Ir
--------------------------------------------------------------------------------
1,834,872,019 (100.0%)  PROGRAM TOTALS

--------------------------------------------------------------------------------
           Ir  file:function
--------------------------------------------------------------------------------
  712,345,018 (38.8%)  program.c:matrix_multiply [/home/user/program]
  321,987,654 (17.5%)  program.c:inner_loop      [/home/user/program]
  245,612,301 (13.4%)  ???:sqrt                  [/usr/lib64/libm.so.6]
  187,432,198 (10.2%)  program.c:vector_norms    [/home/user/program]
  123,456,789 ( 6.7%)  ???:rand                  [/usr/lib64/libc.so.6]
   ...
```

Columnas:
- **Ir**: Instruction reads (instrucciones ejecutadas).
- **%**: porcentaje del total.
- **file:function**: archivo fuente y función.
- **[ruta del binario]**: de qué objeto proviene.

Para ver anotación por línea fuente (muy útil):

```bash
callgrind_annotate --auto=yes callgrind.out.12345
```

Output extra: al final muestra cada función con el source interleaved y el conteo por
línea:

```
-- Auto-annotated source: /home/user/program.c
--------------------------------------------------------------------------------
Ir

          .  static void inner_loop(double *a, double *b, double *out, int n) {
       32  .      double acc = 0.0;
      384  .      for (int k = 0; k < n; k++) {
262,144  27.5%      acc += a[k] * b[k];       ← línea caliente
      384  .      }
       32  .      *out = acc;
        .      }
```

La línea `acc += a[k] * b[k]` ejecutó 262 144 instrucciones, el 27.5% del total. Con
esto sabes **exactamente** qué línea cuesta.

Opciones útiles de `callgrind_annotate`:

```bash
# Solo las 10 funciones más caras
callgrind_annotate --threshold=90 callgrind.out.12345

# Incluir archivos de include específicos
callgrind_annotate -I/usr/include/c++ callgrind.out.12345

# Mostrar tree (equivalente a call graph)
callgrind_annotate --tree=both callgrind.out.12345
```

---

## 8. Eventos que callgrind puede contar

Por default, callgrind cuenta solo `Ir` (instrucciones). Con flags adicionales cuenta
muchos más:

| Flag                      | Eventos añadidos                          |
|---------------------------|--------------------------------------------|
| (default)                 | `Ir` (instruction reads)                  |
| `--cache-sim=yes`         | `I1mr`, `ILmr`, `Dr`, `Dw`, `D1mr`, `D1mw`, `DLmr`, `DLmw` |
| `--branch-sim=yes`        | `Bc`, `Bcm`, `Bi`, `Bim`                  |
| `--collect-jumps=yes`     | Contadores de saltos por línea             |
| `--collect-systime=yes`   | Tiempo de sistema por función              |

Significado:

- **Ir**: Instruction reads. Total de instrucciones ejecutadas.
- **Dr / Dw**: Data reads / writes. Cuántas lecturas/escrituras de memoria.
- **I1mr**: L1 Instruction cache miss (read).
- **ILmr**: Last-level (LL) Instruction cache miss (read).
- **D1mr / D1mw**: L1 Data cache miss read/write.
- **DLmr / DLmw**: LL Data cache miss read/write.
- **Bc / Bcm**: Conditional branches / mispredicted conditional branches.
- **Bi / Bim**: Indirect branches / mispredicted indirect branches.

Con `--cache-sim=yes --branch-sim=yes` tienes un perfil completísimo, pero el overhead
pasa de ~30× a ~80-100×.

Para activar todo:

```bash
valgrind --tool=callgrind \
  --cache-sim=yes \
  --branch-sim=yes \
  --callgrind-out-file=full.callgrind \
  ./program
```

Y leerlo:

```bash
callgrind_annotate --show-percs=yes \
  --event-sort=D1mr:100,DLmr:10,Ir:1 \
  full.callgrind
```

`--event-sort` permite definir un criterio compuesto de ordenación (D1mr tiene peso 100,
DLmr peso 10, Ir peso 1). Así las funciones con más cache misses L1 aparecen primero,
luego las que tengan más LL misses, y como desempate las que tengan más instrucciones.

---

## 9. Simulación de cache con --cache-sim

Callgrind modela una jerarquía de cache **simplificada de 2 niveles**: L1 (separados
instruction/data) y LL (last-level, unificado). No modela L2 como un nivel intermedio
separado.

Por default, callgrind **auto-detecta** los parámetros de tu CPU:

```bash
valgrind --tool=callgrind --cache-sim=yes ./program 2>&1 | head -10
# ==12345== I1 cache: 32768 B, 64 B, 8-way associative
# ==12345== D1 cache: 32768 B, 64 B, 12-way associative
# ==12345== LL cache: 25165824 B, 64 B, 12-way associative
```

Para forzar parámetros específicos (útil para modelar otra máquina):

```bash
valgrind --tool=callgrind \
  --cache-sim=yes \
  --I1=32768,8,64 \
  --D1=32768,8,64 \
  --LL=8388608,16,64 \
  ./program
```

Formato: `<tamaño_bytes>,<asociatividad>,<tamaño_línea>`.

### Salida típica con --cache-sim

```
==12345== I   refs:      1,834,872,019
==12345== I1  misses:        1,234,567     ( 0.07%)
==12345== LLi misses:          234,567     ( 0.01%)
==12345==
==12345== D   refs:        812,345,678    (534,123,456 rd   + 278,222,222 wr)
==12345== D1  misses:       23,456,789    ( 21,345,678 rd   +   2,111,111 wr)
==12345== LLd misses:        3,456,789    (  3,234,567 rd   +     222,222 wr)
==12345==
==12345== LL refs:          24,691,356    ( 22,580,245 rd   +   2,111,111 wr)
==12345== LL misses:         3,691,356    (  3,469,134 rd   +     222,222 wr)
==12345== LL miss rate:         0.1%      (      0.1%       +        0.0% )
```

Interpretación:
- **1 834M instrucciones** ejecutadas.
- **812M** accesos a datos (534M lecturas, 278M escrituras).
- **D1 miss rate**: 23.4M / 812M ≈ 2.9%. Razonable para código con buen locality.
- **LLd miss rate**: 3.4M / 812M ≈ 0.4%. Eso es acceso a RAM. Estos son los caros.

Si ves D1 miss rate > 10%, tu código sufre **cache thrashing**. Si LLd miss rate >
1%, estás **memory-bound** y saturando el bus.

### Limitaciones del modelo

- **No modela prefetcher**: la CPU real prefetchea accesos secuenciales; callgrind
  asume "miss si no está en la cache modelada". Esto **sobreestima** los misses en
  accesos secuenciales.
- **No modela TLB**.
- **No modela out-of-order execution**: asume ejecución in-order, sin pipelining.
- **No modela SMT/hyperthreading**: cada thread tiene su cache dedicada.
- **No modela NUMA**.

Por estas limitaciones, los números de callgrind son una **aproximación guiada**, no
una predicción exacta de rendimiento en CPU real. Pero para **comparar dos versiones
del mismo código**, el ruido es determinista y las diferencias son significativas.

---

## 10. Simulación de branch predictor con --branch-sim

Modelo simple de branch predictor de 2 niveles: una tabla de historia global + un
contador 2-bit por entrada. Es mucho más primitivo que predictores modernos (TAGE,
perceptron) pero sirve como **proxy**.

```bash
valgrind --tool=callgrind \
  --branch-sim=yes \
  ./program
```

Salida:

```
==12345== Branches:        123,456,789    ( 98,765,432 cond +  24,691,357 ind)
==12345== Mispredicts:      12,345,678    (  8,765,432 cond +   3,580,246 ind)
==12345== Mispred rate:         10.0%     (      8.9%       +       14.5% )
```

- **Conditional branches**: `if`, `while`, `for` con condiciones.
- **Indirect branches**: punteros a función, virtual methods, switch statements grandes.

**Mispred rate > 5% conditional**: probablemente tienes ramas imprevisibles (datos
aleatorios). Considera branchless code o sort previo de datos.

**Mispred rate > 20% indirect**: muchas llamadas virtuales impredecibles. Considera
devirtualización (tipos concretos en lugar de punteros a interface).

---

## 11. Jerarquía L1/LL: qué modela y qué no

```
REALIDAD (Intel Core, Zen moderno):     CALLGRIND MODEL:

 ┌───────────────────────┐              ┌─────────────┐
 │ L1i 32KB (per core)   │              │ L1i (I1)    │
 ├───────────────────────┤              ├─────────────┤
 │ L1d 48KB (per core)   │              │ L1d (D1)    │
 ├───────────────────────┤              ├─────────────┤
 │ L2  1MB (per core)    │   ←─ NO      │ (ausente)   │
 ├───────────────────────┤              ├─────────────┤
 │ L3  30MB (shared)     │              │ LL          │
 ├───────────────────────┤              ├─────────────┤
 │ DRAM (per socket)     │              │ (miss LL)   │
 └───────────────────────┘              └─────────────┘
```

Callgrind colapsa L2+L3 en un solo "LL". Un programa que cabe en L2 pero no en L1
mostrará miss en L1 pero hit en LL, lo cual es correcto. Un programa que cabe en L3
pero no en L2 NO muestra ningún nivel intermedio: pasa directamente de "L1 miss" a
"LL hit".

Esto es una **simplificación intencional** para mantener el modelo tractable. En la
práctica:

- Para distinguir L1 vs L2/L3: `callgrind --cache-sim=yes` es suficiente.
- Para distinguir L2 vs L3 con precisión: usa `perf stat -e L2-hits,L2-misses,
  LLC-hits,LLC-misses` en hardware real.

---

## 12. Overhead: por qué callgrind es 20-100x más lento

Contribuciones al overhead:

1. **Traducción VEX**: cada bloque básico se traduce una vez (caché de bloques).
2. **Instrumentación**: contadores atómicos por instrucción.
3. **Simulación de cache** (si activada): lookup en estructuras de datos simuladas
   por cada acceso.
4. **Memoria virtual**: el programa entero corre en una VM de valgrind, con todas
   las syscalls interceptadas.

Resultado:

| Herramienta                         | Slowdown típico |
|--------------------------------------|-----------------|
| `perf record` (sampling)            | 1.01-1.05×     |
| `valgrind --tool=none` (sin nada)   | 3-5×           |
| `valgrind --tool=callgrind`         | 20-50×         |
| `valgrind --tool=callgrind --cache-sim=yes` | 50-100×  |
| `valgrind --tool=callgrind --cache-sim=yes --branch-sim=yes` | 80-150× |
| `valgrind --tool=memcheck`          | 20-30×         |

Un programa que corre en 10s normalmente tarda 5-15 minutos con callgrind completo.

**Implicaciones prácticas**:

1. **Usa inputs pequeños pero representativos**. Si tu bench procesa 1GB normalmente,
   con callgrind dale 10MB. Los porcentajes relativos serán casi iguales.
2. **Desactiva animaciones/GUI/sleeps**. Un programa con `usleep(100ms)` dentro del
   loop dará 100ms × 50 = 5 segundos de sleep real durante callgrind, sin overhead.
3. **No profilees código que depende de timing real**: callgrind distorsiona los
   timings; código con `clock_gettime` o timeouts comporta distinto.

---

## 13. KCachegrind: visualización interactiva

`kcachegrind` (KDE) es la GUI más madura para leer output de callgrind. Para
entornos sin KDE:
- `qcachegrind` (mismo código, sin dependencias KDE).
- `callgrind_annotate` en CLI (siempre disponible).

```bash
kcachegrind callgrind.out.12345
```

La interfaz tiene 4 paneles principales:

```
┌─────────────────────────┬─────────────────────────┐
│  Flat Profile           │   Source / Assembly     │
│  (lista funciones)      │   (código anotado)      │
│                         │                         │
├─────────────────────────┼─────────────────────────┤
│  Call Graph             │   Callers / Callees     │
│  (grafo visual)         │   (tablas)              │
└─────────────────────────┴─────────────────────────┘
```

Arriba seleccionas una función. Los paneles de la derecha y abajo se actualizan para
mostrar su contexto.

---

## 14. Navegar KCachegrind: callers, callees, source, map

### Flat profile (izquierda arriba)

Lista todas las funciones ordenadas por el evento actualmente seleccionado (Ir por
defecto, pero puedes cambiar a D1mr, LLmiss, etc. desde el menú de eventos).

Dos columnas clave:
- **Incl.** (inclusive): evento total de la función + todas sus llamadas.
- **Self**: evento solo de la función (sin llamadas).

Ejemplo:
```
Incl.    Self    Called   Function
38.8%    0.1%    1        matrix_multiply
17.5%   17.2%    262144   inner_loop
13.4%   13.4%    512      sqrt
```

`matrix_multiply` tiene 38.8% inclusive pero solo 0.1% self → delega todo a sus
hijas. `inner_loop` tiene casi todo como self → ahí se hace el trabajo.

### Source (derecha arriba)

Muestra el código fuente de la función seleccionada con anotación lateral del
conteo por línea. Alternativamente puedes ver ensamblador.

### Call graph (izquierda abajo)

Grafo interactivo con nodos como funciones y aristas como llamadas. Cada arista
muestra cuántas veces se llamó. Los nodos más grandes son los más calientes.

Click derecho → "Zoom to function" centra el grafo en una función específica.

### Callers/callees (derecha abajo)

Dos tablas:
- **Callers**: qué funciones llaman a la seleccionada.
- **Callees**: qué funciones llama la seleccionada.

Clickando en una de ellas navegas: útil para explorar un call chain.

### Atajos útiles

- `Ctrl+F`: búsqueda por nombre de función.
- `Ctrl+G`: go to function by name.
- `Ctrl+1..9`: cambiar evento mostrado (1=Ir, 2=D1mr, etc.).
- Click en columna → reordenar.
- Flechas arriba/abajo en flat profile: navegar.

### Cambiar el evento visualizado

Si activaste `--cache-sim` y `--branch-sim`, tienes múltiples eventos. En el menú
superior "Cost Type", elige:
- `Ir`: ordena por instrucciones.
- `D1mr`: ordena por L1 data cache read misses (¡hotspots de memoria!).
- `Bcm`: ordena por mispredictions condicionales.

Esto permite hacer preguntas distintas al mismo perfil: "¿qué función tiene más
instrucciones?" vs "¿qué función tiene más cache misses?" pueden ser funciones
distintas.

---

## 15. Cachegrind: solo simulación de cache

**Cachegrind** (`--tool=cachegrind`) es una versión reducida de callgrind: solo
hace simulación de cache, **sin** tracking de call graph. Por tanto:

- Más rápido (~40-60× en lugar de 80-100×).
- Menos información (no tienes árbol de llamadas).
- Output a `cachegrind.out.<PID>`.

```bash
valgrind --tool=cachegrind ./program
```

Opciones similares a callgrind:
- `--I1=<size,assoc,line>`
- `--D1=<size,assoc,line>`
- `--LL=<size,assoc,line>`
- `--branch-sim=yes`

Para leer:

```bash
cg_annotate cachegrind.out.12345
```

`cg_annotate` es similar a `callgrind_annotate` pero sin árbol de llamadas.

Útil cuando:
- Solo quieres entender el comportamiento de cache de un programa.
- El call graph es demasiado lento.
- Quieres comparar dos versiones con `cg_diff` (ver §17).

---

## 16. Cachegrind vs Callgrind: cuándo usar cuál

| Necesidad                              | Herramienta |
|----------------------------------------|-------------|
| Árbol de llamadas (callers/callees)   | Callgrind   |
| Solo cache behavior                    | Cachegrind  |
| Máxima velocidad                       | Cachegrind  |
| Visualización con kcachegrind          | Callgrind   |
| Diff entre runs para regression tests | Cachegrind (mejor soporte) |
| Contar instrucciones solo (sin cache) | Callgrind sin `--cache-sim` |
| Line-by-line hotspots exactos          | Cualquiera  |
| Benchmark determinista (iai)           | Callgrind   |

Regla: **cachegrind** si solo te importa qué líneas cuestan y cómo se comportan las
caches. **callgrind** si además necesitas entender el call graph.

---

## 17. Cachegrind + cg_diff: comparar dos runs

`cg_diff` toma dos archivos cachegrind y produce uno con las diferencias. Útil en CI
para detectar regresiones:

```bash
# Run antes del cambio
valgrind --tool=cachegrind --cachegrind-out-file=before.cg ./program

# Hacer el cambio, recompilar
# ...

# Run después
valgrind --tool=cachegrind --cachegrind-out-file=after.cg ./program

# Diff
cg_diff before.cg after.cg > diff.cg
cg_annotate diff.cg
```

Output similar al normal pero con **delta**:
```
          Ir       D1mr  file:function
 +2,345,678     +12,345  program.c:matrix_multiply
   -987,654        -123  program.c:vector_norms
```

Positivos: el cambio **añadió** instrucciones/misses (empeoró).
Negativos: el cambio **redujo** (mejoró).

Para normalizar inputs (en caso de programas con randomness):
```bash
cg_diff --mod-filename='s/_v[0-9]+//' before.cg after.cg
```

### Uso en CI

Patrón común:

```bash
#!/bin/bash
# ci-regression-test.sh

# Build actual
make
valgrind --tool=cachegrind --cachegrind-out-file=current.cg ./benchmark

# Comparar con baseline comprometido
cg_diff baseline.cg current.cg > diff.cg

# Extraer métricas del diff
INSTR_DIFF=$(cg_annotate diff.cg | grep "PROGRAM TOTALS" | awk '{print $1}')

# Abortar si aumenta más del 1%
if [ "$INSTR_DIFF" -gt "10000000" ]; then
    echo "REGRESSION: +$INSTR_DIFF instructions"
    exit 1
fi
```

Esto es el núcleo de cómo **iai** (§23) implementa regression testing determinista.

---

## 18. Perfilar secciones específicas con client requests

A veces no quieres profilear todo el programa, solo una función específica. Valgrind
expone **client requests**: macros que tu código inserta para comunicar con valgrind
en runtime.

```c
#include <valgrind/callgrind.h>

int main(void) {
    setup_data();            // NO queremos profilear esto

    CALLGRIND_START_INSTRUMENTATION;
    CALLGRIND_TOGGLE_COLLECT;

    hot_function();          // SÍ queremos profilear esto

    CALLGRIND_TOGGLE_COLLECT;
    CALLGRIND_STOP_INSTRUMENTATION;

    cleanup();               // NO queremos profilear esto
    return 0;
}
```

Compila con:
```bash
gcc -O2 -g -o program program.c
```

Y ejecuta desactivando la colección al inicio:

```bash
valgrind --tool=callgrind \
    --instr-atstart=no \
    --collect-atstart=no \
    ./program
```

- `--instr-atstart=no`: no instrumenta hasta que el programa lo pida.
- `--collect-atstart=no`: no recoge eventos hasta que el programa lo pida.

Las macros:
- `CALLGRIND_START_INSTRUMENTATION`: activa instrumentación.
- `CALLGRIND_STOP_INSTRUMENTATION`: desactiva (el programa sigue corriendo pero sin
  overhead).
- `CALLGRIND_TOGGLE_COLLECT`: toggle del flag de colección.
- `CALLGRIND_DUMP_STATS`: fuerza volcado del snapshot actual.
- `CALLGRIND_ZERO_STATS`: resetea contadores.

**Importante**: para Rust necesitas usar `crabgrind` o FFI manual, porque estas
macros son C. El crate `crabgrind` encapsula todos los client requests:

```rust
use crabgrind as cg;

fn main() {
    setup_data();
    cg::callgrind::start_instrumentation();
    hot_function();
    cg::callgrind::stop_instrumentation();
    cleanup();
}
```

---

## 19. Controlar inicio/paro desde línea de comandos

Alternativa a modificar el código: `callgrind_control` permite mandar comandos a un
proceso callgrind en ejecución.

```bash
# Terminal 1
valgrind --tool=callgrind --instr-atstart=no ./program

# Terminal 2
callgrind_control -i on            # activa instrumentación
# ... esperar a fase interesante ...
callgrind_control -i off           # desactiva
callgrind_control -d               # dump del estado actual
callgrind_control -z               # zero stats
callgrind_control -k               # kill
```

También `callgrind_control -b` imprime el backtrace actual del programa vivo (útil
si parece colgado).

---

## 20. Filtrar por función, archivo o thread

### Excluir bibliotecas del sistema

```bash
valgrind --tool=callgrind \
  --fn-skip=__libc_* \
  --fn-skip=_dl_* \
  ./program
```

Las funciones que matchean el patrón (glob-style) no se instrumentan: no aparecen en
el output y contribuyen menos overhead.

### Skip de objetos enteros

```bash
valgrind --tool=callgrind \
  --obj-skip=/lib64/libc.so.6 \
  ./program
```

Todas las funciones de libc se saltan.

### Incluir solo ciertas funciones

Inverso: solo incluye lo que matchea:

```bash
valgrind --tool=callgrind \
  --fn-recursion=20 \
  --fn-recursion-my_function=1 \
  ./program
```

`--fn-recursion` controla cuánta recursión se contabiliza por función (default ilimitado).
Esto es un proxy: si quieres "solo mi_funcion", pasa `--separate-recs=my_function`.

---

## 21. Programas multihilo: --separate-threads

Por default, callgrind suma datos de todos los threads en un único profile. Para
tener profiles separados por thread:

```bash
valgrind --tool=callgrind \
  --separate-threads=yes \
  ./multithreaded_program
```

Genera varios archivos:
```
callgrind.out.12345-01   ← thread 1
callgrind.out.12345-02   ← thread 2
callgrind.out.12345-03   ← thread 3
```

Cada archivo es independiente y se abre con `callgrind_annotate` o `kcachegrind`
normalmente.

**Limitación**: valgrind serializa la ejecución de threads (solo un thread corriendo
a la vez dentro del simulador). Por tanto:
- Los conteos de instrucciones son correctos.
- Los conteos de cache misses son **por thread, sin interferencia con otros threads**.
- **No modela false sharing** entre threads (cada thread "tiene su cache").
- No modela contention de locks con precisión temporal.

Para análisis de contention/races, usa `helgrind` o `drd`. Para false sharing, usa
`perf c2c` en hardware real.

---

## 22. Rust bajo Valgrind: cosas a saber

Rust funciona bajo valgrind con algunas caveats:

### Compilación con debug info
```toml
# Cargo.toml
[profile.bench]
debug = true

[profile.release]
debug = 1  # line tables suficientes
```

### Leak detection
Memcheck de Rust puede reportar "leaks" que son en realidad asignaciones globales de
stdlib (lazily initialized). Supresiones típicas:

```bash
valgrind --tool=memcheck \
  --suppressions=/path/to/rust.supp \
  ./target/release/my_bin
```

El repositorio de cargo tiene un archivo `.valgrindrc` de ejemplo.

### Callgrind en Rust
Callgrind funciona sin problemas especiales. Flujo típico:

```bash
cargo build --release
valgrind --tool=callgrind \
  --callgrind-out-file=cg.out \
  target/release/my_bin

callgrind_annotate cg.out
kcachegrind cg.out
```

### Inlining
Igual que con `perf` + flamegraph, el inlining oculta funciones. Para Rust es
agresivo. Si quieres ver todas las funciones, compila con:

```toml
[profile.bench]
debug = true
lto = false
codegen-units = 256   # limita inlining
```

Aunque esto da un perfil menos representativo del binario final.

### Assembly no soportado

Algunos binarios Rust con `-C target-cpu=native` en CPUs modernas usan AVX-512. Si
valgrind de tu distro no soporta esas instrucciones:

```bash
RUSTFLAGS="-C target-cpu=x86-64-v3" cargo build --release
```

Usa una ISA más conservadora compatible con valgrind.

---

## 23. Iai: benchmarking Rust basado en callgrind

**iai** es un framework de benchmarking Rust que usa **callgrind** en lugar de
medición real. La filosofía:

- No mide tiempo wall-clock (ruidoso).
- Cuenta instrucciones, L1 accesses, LL accesses, RAM accesses → valor determinista.
- Perfecto para regression benchmarks en CI.

Instalación en `Cargo.toml`:

```toml
[dev-dependencies]
iai = "0.1"

[[bench]]
name = "my_bench"
harness = false
```

Bench ejemplo:

```rust
use iai::{black_box, main};

fn fibonacci(n: u64) -> u64 {
    match n {
        0 => 1,
        1 => 1,
        n => fibonacci(n - 1) + fibonacci(n - 2),
    }
}

fn iai_fibonacci_short() {
    fibonacci(black_box(10));
}

fn iai_fibonacci_long() {
    fibonacci(black_box(30));
}

main!(iai_fibonacci_short, iai_fibonacci_long);
```

Ejecución:

```bash
cargo bench --bench my_bench
```

Output:

```
iai_fibonacci_short
  Instructions:                 1735
  L1 Accesses:                  2364
  L2 Accesses:                     1
  RAM Accesses:                    1
  Estimated Cycles:             2404

iai_fibonacci_long
  Instructions:              26865563
  L1 Accesses:               36599327
  L2 Accesses:                      2
  RAM Accesses:                     0
  Estimated Cycles:          36599349
```

Los números son **exactos**. Corre 100 veces y son idénticos.

### Cycles estimados

iai aproxima el ciclo total como:
```
cycles ≈ L1_accesses + 5 × L2_accesses + 35 × RAM_accesses
```

(Los factores 5 y 35 son latencias típicas de L2 y RAM en ciclos.) No es una
predicción exacta, pero correlaciona bien con tiempo real cuando el programa es
memory-bound.

### Baseline + regression

iai guarda el último run como baseline automáticamente. El siguiente run compara:

```
iai_fibonacci_long
  Instructions:              26865563  (No change)
  L1 Accesses:               36599327  (No change)
```

Si cambias el código:

```
iai_fibonacci_long
  Instructions:              26865570  (+0.00003%)
  L1 Accesses:               36599340  (+0.00004%)
```

Y si algo se rompe catastróficamente:

```
iai_fibonacci_long
  Instructions:             50123456  (+86.5%)  ← flag en CI
```

### Cuándo usar iai en lugar de criterion

| Caso                            | Herramienta |
|----------------------------------|-------------|
| Medir tiempo real en producción | Criterion   |
| Detectar regresiones en CI       | Iai         |
| Comparar algoritmos en abstracto | Iai         |
| Benchmarks con I/O real          | Criterion   |
| Runs muy cortos (<1 ms)          | Iai         |
| Necesitas cache behavior         | Iai         |

Iai y criterion son **complementarios**, no excluyentes. Muchos proyectos usan
criterion en local y iai en CI.

`iai-callgrind` (fork moderno) tiene más features: librerías, setup/teardown,
múltiples grupos, output en formato JSON para integración.

---

## 24. Comparación: perf vs callgrind vs flamegraph

Tres técnicas de profiling complementarias:

| Dimensión             | perf (sampling)  | callgrind (sim)   | flamegraph     |
|------------------------|------------------|-------------------|----------------|
| Principio             | Muestreo PMU     | Instrumentación DBI| Visualización de samples |
| Overhead              | 1-5%             | 20-100×           | Hereda de perf |
| Determinismo          | Estadístico      | Exacto            | Estadístico    |
| Resolución temporal   | Tiempo real      | Virtual (instrucciones) | Tiempo real |
| Granularidad          | Función/línea    | Instrucción exacta| Función        |
| Multithread real      | Sí               | Serializado       | Sí             |
| Cache behavior        | Requiere PMU real| Modelo simulado   | No directamente|
| Call graph            | Sí (con -g)      | Sí nativo         | Sí (imagen)    |
| Regresiones CI        | Difícil (ruido)  | Fácil (exacto)    | Visual         |
| Producción viva       | Sí               | No (overhead)     | Sí             |
| Runs cortos (<100ms)  | No (pocas samples)| Sí               | No             |
| Hardware necesario    | PMU access       | Ninguno           | PMU access     |
| Funciona en VM/CI     | A veces          | Siempre           | A veces        |

**Flujo pragmático recomendado**:

1. **Primera vista**: flamegraph (§T02) para detectar hotspots rápidamente.
2. **Validación**: `perf report` + `perf annotate` para ver detalles de ensamblador.
3. **Refinamiento fino**: callgrind + kcachegrind para contar exactamente líneas.
4. **CI regression**: cachegrind + `cg_diff` o iai para benchmarks reproducibles.

Cada herramienta tiene su nicho. **Ninguna es "mejor"**. Son las tres caras de un
mismo trabajo: encontrar dónde se va el tiempo y por qué.

---

## 25. Errores comunes con callgrind

### Error 1 — creer que callgrind mide tiempo

```
"Mi función tarda 245 millones de Ir. ¿Cuánto es eso en segundos?"
```

No directamente. Ir son instrucciones, no tiempo. Una instrucción puede tardar 0.3
ciclos (superescalar paralelo) o 300 ciclos (miss DRAM). Para tiempo, usa `perf`.

### Error 2 — profilear con `-O0`

Igual que con perf: un build sin optimización tiene perfil irreal. Compila con
`-O2 -g` como mínimo.

### Error 3 — olvidar `--cache-sim=yes`

Si quieres ver misses y no pones el flag, callgrind solo cuenta instrucciones. Al
leerlo en kcachegrind, los eventos de cache no aparecen.

### Error 4 — usar en producción

Callgrind es 20-100× más lento. Nunca en producción. Solo desarrollo y CI con inputs
reducidos.

### Error 5 — comparar números absolutos entre máquinas

Callgrind auto-detecta la cache, así que dos máquinas con distintas caches darán
números de misses distintos. Para comparaciones entre máquinas, fija los parámetros
con `--I1=... --D1=... --LL=...`.

### Error 6 — ignorar el inlining

Código Rust con iteradores genéricos: todo se inlinea dentro de la función del
usuario. Ves `my_function` con 90% self-time y crees "esa función es cara", pero
realmente el 80% es `iter.map.filter.collect` inlineado. Valgrind no tiene opción
equivalente a `perf script --inline`, lo mejor que puedes hacer es compilar con
`lto = false` y `codegen-units = 256`.

### Error 7 — confundir Ir con tiempo de CPU real

```
"Optimizé para reducir Ir un 50% pero el programa no es más rápido."
```

Posible causa: las instrucciones eliminadas eran "baratas" (ALU simples) y las
restantes son caras (dependencias, misses). La reducción de Ir no implica reducción
de ciclos reales. Valida **siempre** con un benchmark real tras optimizar guiado por
callgrind.

### Error 8 — no usar suppressions con Rust

Rust stdlib tiene asignaciones globales "leak" intencionales (thread-local storage,
lazy statics). Sin suppressions, memcheck reporta estos leaks como reales.

### Error 9 — AVX-512 / instrucciones nuevas

Valgrind 3.20 y anteriores fallan con AVX-512F en Intel y AMD modernos. Solución:
actualizar valgrind, o compilar con `-mno-avx512f`.

### Error 10 — profilear todo el programa cuando solo importa una función

Perder horas esperando que callgrind termine un programa entero en vez de usar
client requests (`CALLGRIND_START_INSTRUMENTATION`) para acotar.

---

## 26. Programa de práctica: cache_victim.c

Programa diseñado para mostrar claramente efectos de cache en callgrind:

```c
/* cache_victim.c — demostración de efectos de cache para callgrind
 *
 * Compilar:
 *   gcc -O2 -g -o cache_victim cache_victim.c
 *
 * Profilar:
 *   valgrind --tool=callgrind \
 *     --cache-sim=yes --branch-sim=yes \
 *     --callgrind-out-file=cv.callgrind \
 *     ./cache_victim
 *
 *   callgrind_annotate --auto=yes cv.callgrind > report.txt
 *   kcachegrind cv.callgrind
 *
 * Qué esperar en callgrind:
 *   - sum_row_major:  D1mr muy bajo, buen locality
 *   - sum_col_major:  D1mr alto, mal locality (stride grande)
 *   - linked_walk:    D1mr moderado/alto, accesos aleatorios
 *   - stride_walk_*:  D1mr aumenta con stride
 *   - branchy_count:  Bcm alto si los datos son aleatorios
 *   - branchless_count: Bcm casi cero
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define N          1024     /* matriz NxN */
#define LIST_LEN   4096
#define DATA_LEN   (1 << 20)

/* ---- locality: row major vs col major ---- */

static double sum_row_major(const double *A, int n) {
    double s = 0.0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            s += A[i * n + j];
    return s;
}

static double sum_col_major(const double *A, int n) {
    double s = 0.0;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++)
            s += A[i * n + j];
    return s;
}

/* ---- linked list vs array ---- */

typedef struct Node {
    int value;
    struct Node *next;
} Node;

static Node *make_random_list(int n) {
    /* indices aleatorios para que los nodos estén dispersos en el heap */
    Node **nodes = malloc(sizeof(*nodes) * n);
    for (int i = 0; i < n; i++) {
        nodes[i] = malloc(sizeof(Node));
        nodes[i]->value = i;
    }
    /* shuffle */
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Node *tmp = nodes[i]; nodes[i] = nodes[j]; nodes[j] = tmp;
    }
    /* encadenar */
    for (int i = 0; i < n - 1; i++) nodes[i]->next = nodes[i + 1];
    nodes[n - 1]->next = NULL;
    Node *head = nodes[0];
    free(nodes);
    return head;
}

static long linked_walk(Node *head) {
    long s = 0;
    while (head) {
        s += head->value;
        head = head->next;
    }
    return s;
}

/* ---- stride access ---- */

static long stride_walk(const int *data, int n, int stride) {
    long s = 0;
    for (int i = 0; i < n; i += stride) s += data[i];
    return s;
}

/* ---- branches: predecibles vs aleatorias ---- */

static long branchy_count(const int *data, int n, int threshold) {
    long c = 0;
    for (int i = 0; i < n; i++) {
        if (data[i] > threshold) c++;    /* branch dependiente de datos */
    }
    return c;
}

static long branchless_count(const int *data, int n, int threshold) {
    long c = 0;
    for (int i = 0; i < n; i++) {
        c += (data[i] > threshold);      /* sin branch explícita */
    }
    return c;
}

/* ---- free list ---- */

static void free_list(Node *head) {
    while (head) {
        Node *next = head->next;
        free(head);
        head = next;
    }
}

int main(void) {
    srand(42);

    /* matrix */
    double *A = malloc(sizeof(double) * N * N);
    for (int i = 0; i < N * N; i++) A[i] = (double)i;

    /* data arrays */
    int *data_sorted = malloc(sizeof(int) * DATA_LEN);
    int *data_random = malloc(sizeof(int) * DATA_LEN);
    for (int i = 0; i < DATA_LEN; i++) {
        data_sorted[i] = i;
        data_random[i] = rand();
    }

    /* linked list */
    Node *list = make_random_list(LIST_LEN);

    /* ---- ejecutar y acumular para evitar DCE ---- */
    volatile double sink_d = 0.0;
    volatile long   sink_l = 0;

    sink_d += sum_row_major(A, N);
    sink_d += sum_col_major(A, N);

    for (int r = 0; r < 16; r++)
        sink_l += linked_walk(list);

    sink_l += stride_walk(data_sorted, DATA_LEN, 1);
    sink_l += stride_walk(data_sorted, DATA_LEN, 8);
    sink_l += stride_walk(data_sorted, DATA_LEN, 64);
    sink_l += stride_walk(data_sorted, DATA_LEN, 512);

    /* branches aleatorias → mucha mispredict */
    sink_l += branchy_count(data_random, DATA_LEN, RAND_MAX / 2);
    sink_l += branchless_count(data_random, DATA_LEN, RAND_MAX / 2);

    /* branches predecibles → casi cero mispredict */
    sink_l += branchy_count(data_sorted, DATA_LEN, DATA_LEN / 2);
    sink_l += branchless_count(data_sorted, DATA_LEN, DATA_LEN / 2);

    printf("sinks: %.2f %ld\n", sink_d, sink_l);

    free(A);
    free(data_sorted);
    free(data_random);
    free_list(list);
    return 0;
}
```

### Ejecución

```bash
# 1. Compilar
gcc -O2 -g -o cache_victim cache_victim.c

# 2. Profilar con cache + branch sim
valgrind --tool=callgrind \
    --cache-sim=yes \
    --branch-sim=yes \
    --callgrind-out-file=cv.callgrind \
    ./cache_victim

# 3. Ver reporte texto
callgrind_annotate --auto=yes cv.callgrind | less

# 4. Ver GUI
kcachegrind cv.callgrind
```

### Observaciones esperadas

En `callgrind_annotate` ordenando por distintos eventos:

Por **Ir** (instrucciones): las funciones con más loops dominan.
```
Ir                  function
850M  stride_walk (todas variantes)
400M  sum_row_major
400M  sum_col_major
200M  branchy_count + branchless_count
...
```

Por **D1mr** (L1 data cache miss reads) — cambia el ordering:
```
D1mr                function
2.5M  sum_col_major        ← pésimo locality
1.8M  linked_walk          ← nodos dispersos
0.8M  stride_walk (stride=512)
0.1M  sum_row_major        ← locality perfecto
0.05M stride_walk (stride=1)
```

Por **Bcm** (conditional branch mispredicts):
```
Bcm                 function
1.2M  branchy_count (data_random)   ← 50% mispredict, peor caso
0.6M  branchless_count (data_random) ← branchless, casi cero
0.0K  branchy_count (data_sorted)    ← predictor aprende
```

### Interpretación

- **sum_row_major vs sum_col_major**: mismo número de Ir (mismo loop), pero
  `col_major` tiene ~10-20× más D1mr porque salta 1024 elementos (8KB) entre
  accesos, saltando líneas de cache. Esto es **el ejemplo canónico** de mal
  locality.

- **linked_walk**: los nodos están en direcciones aleatorias del heap, cada acceso
  es miss. Si comparas con una array de los mismos valores, el array tiene
  10-50× menos misses. Esto justifica estructuras cache-friendly.

- **stride_walk**: los misses **crecen con el stride** hasta saturar cuando cada
  acceso es a una línea nueva. Stride 1 es perfecto. Stride > 16 (línea de cache)
  es miss por cada acceso.

- **branchy_count con data_random**: el predictor no puede aprender → 40-50%
  mispredict rate. Cada mispredict cuesta 15-20 ciclos reales. Con `data_sorted`,
  el predictor ve el mismo resultado durante mucho tiempo y acierta.

- **branchless_count**: sin ramas dependientes del dato, no hay mispredicts
  posibles. En hardware real, suele ser 2-3× más rápido que `branchy` con datos
  aleatorios, pero puede ser más lento con datos ordenados (el CPU adivina
  perfecto el branch).

---

## 27. Ejercicios

### Ejercicio 1 — First call

1. Compila `cache_victim.c` con `-O2 -g`.
2. Ejecuta con callgrind **sin** `--cache-sim` (solo Ir).
3. Abre el resultado con `callgrind_annotate --auto=yes`.
4. Pregunta: ¿aparece diferencia entre `sum_row_major` y `sum_col_major`?
   (Respuesta esperada: casi no, porque Ir es igual; la diferencia está en cache.)

### Ejercicio 2 — Cache simulation

1. Repite el run con `--cache-sim=yes`.
2. Abre con `kcachegrind`.
3. Selecciona `D1mr` como Cost Type desde el menú.
4. Identifica las 3 funciones con peor D1 miss rate.
5. Calcula a mano: `D1 miss rate = D1mr / Dr` para cada una.

### Ejercicio 3 — Branches

1. Con el mismo run (`--branch-sim=yes`), selecciona `Bcm` como Cost Type.
2. ¿Qué invocación de `branchy_count` tiene más mispredictions? ¿Por qué?
3. Compara `branchy_count(data_random)` vs `branchless_count(data_random)`:
   ¿Tienen el mismo Ir? ¿Tienen los mismos Bcm?

### Ejercicio 4 — Comparar con perf

1. Ejecuta el mismo binario con `perf stat -d -d ./cache_victim`.
2. Compara:
   - `L1-dcache-load-misses` de perf vs `D1mr` total de callgrind.
   - `branch-misses` de perf vs `Bcm` total de callgrind.
3. Deberían ser del mismo orden de magnitud pero no idénticos. ¿Por qué?
   (Respuesta: prefetcher hw en perf, modelo simplificado en callgrind.)

### Ejercicio 5 — Optimizar con callgrind

1. Cambia `sum_col_major` para que sea igual que `sum_row_major` pero iterando en
   el orden correcto. (Transponer la lógica, no el array.)
2. Re-profilea y compara los D1mr antes/después.
3. Usa `cg_diff`:
   ```bash
   cg_diff before.callgrind after.callgrind > diff.callgrind
   callgrind_annotate diff.callgrind
   ```
4. Verifica que las instrucciones (Ir) son casi iguales pero D1mr ha bajado
   drásticamente.

### Ejercicio 6 — Client requests en C

1. Modifica `cache_victim.c` para instrumentar **solo** la sección de
   `sum_col_major`:
   ```c
   #include <valgrind/callgrind.h>

   CALLGRIND_START_INSTRUMENTATION;
   CALLGRIND_TOGGLE_COLLECT;
   sink_d += sum_col_major(A, N);
   CALLGRIND_TOGGLE_COLLECT;
   CALLGRIND_STOP_INSTRUMENTATION;
   ```
2. Ejecuta con `--instr-atstart=no --collect-atstart=no`.
3. Compara el total de Ir con la versión completa.
4. Observa cuánto overhead total se reduce.

### Ejercicio 7 — iai en Rust (opcional)

1. Crea un proyecto Rust con iai:
   ```bash
   cargo new rust_bench
   cd rust_bench
   ```
2. Añade iai y un bench simple (fibonacci o suma).
3. Ejecuta `cargo bench` y observa los Instructions.
4. Cambia el algoritmo (recursivo → iterativo) y vuelve a correr.
5. iai automáticamente muestra el diff.

---

## 28. Checklist de validación

Antes de confiar en los números de callgrind:

- [ ] Compilado con `-O2 -g` (release con debug info, no `-O0`).
- [ ] `--cache-sim=yes` si quieres ver cache misses.
- [ ] `--branch-sim=yes` si quieres ver mispredictions.
- [ ] Parámetros de cache auto-detectados (o fijados con `--I1 --D1 --LL`).
- [ ] Binario contiene símbolos (`file ./bin` dice "not stripped").
- [ ] Si el programa es grande, usar client requests para acotar.
- [ ] `callgrind_annotate --auto=yes` para ver source anotado.
- [ ] Validar hallazgos con un benchmark real (callgrind no mide tiempo).
- [ ] Si comparas runs, usar `cg_diff` en lugar de comparar números a ojo.
- [ ] Si es multihilo, `--separate-threads=yes` y analizar cada hilo por
      separado.
- [ ] Recordar que callgrind no modela prefetcher, OOO, NUMA: los números son
      proxies útiles, no realidad exacta.

---

## 29. Navegación

| ← Anterior | ↑ Sección | Siguiente → |
|------------|-----------|-------------|
| [T02 — Flamegraphs](../T02_Flamegraphs/README.md) | [S02 — Profiling](../) | [T04 — Profiling de memoria](../T04_Profiling_de_memoria/README.md) |

**Capítulo 4 — Sección 2: Profiling de Aplicaciones — Tópico 3 de 4**

Has aprendido la filosofía de profiling por **simulación** vs por **muestreo**, cómo
callgrind cuenta cada instrucción y simula cache/branch predictor, cuándo merece la
pena el 50× de overhead para obtener números deterministas, cómo usar kcachegrind
para navegar visualmente los resultados, y cómo herramientas como iai aprovechan
callgrind para benchmarks reproducibles en CI. En el siguiente tópico (T04 Profiling
de memoria) cambiamos el foco de CPU a **heap**: cuánta memoria asigna tu programa,
dónde, cuándo, y cuánto permanece viva. Herramientas como `heaptrack`, `massif` y
`DHAT` nos darán una perspectiva completamente nueva del mismo problema: no solo
cuánto tiempo, sino cuántos bytes.
