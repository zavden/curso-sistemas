# T04 — Profiling de memoria

> **Bloque 17 · Capítulo 4 · Sección 2 · Tópico 4**
> Dónde se asignan los bytes, cuántos, cuándo, y cuántos permanecen vivos. Heap profiling con heaptrack, DHAT, massif y jemalloc.

---

## Índice

1. [Por qué el profiling de memoria es distinto](#1-por-qué-el-profiling-de-memoria-es-distinto)
2. [Conceptos clave: allocation, peak, RSS, heap](#2-conceptos-clave-allocation-peak-rss-heap)
3. [Preguntas que responde un memory profiler](#3-preguntas-que-responde-un-memory-profiler)
4. [Panorama de herramientas](#4-panorama-de-herramientas)
5. [heaptrack: la herramienta de referencia en Linux](#5-heaptrack-la-herramienta-de-referencia-en-linux)
6. [Instalar y ejecutar heaptrack](#6-instalar-y-ejecutar-heaptrack)
7. [heaptrack GUI: vistas y qué significa cada una](#7-heaptrack-gui-vistas-y-qué-significa-cada-una)
8. [Allocation flamegraphs en heaptrack](#8-allocation-flamegraphs-en-heaptrack)
9. [Detectar temporary allocations](#9-detectar-temporary-allocations)
10. [Detectar leaks con heaptrack](#10-detectar-leaks-con-heaptrack)
11. [Comparar dos runs con heaptrack](#11-comparar-dos-runs-con-heaptrack)
12. [DHAT: Dynamic Heap Analysis Tool de Valgrind](#12-dhat-dynamic-heap-analysis-tool-de-valgrind)
13. [Métricas únicas de DHAT: access density, liveness](#13-métricas-únicas-de-dhat-access-density-liveness)
14. [Interpretar el output de DHAT](#14-interpretar-el-output-de-dhat)
15. [DHAT viewer y dhat-rs](#15-dhat-viewer-y-dhat-rs)
16. [Massif: snapshots de heap a lo largo del tiempo](#16-massif-snapshots-de-heap-a-lo-largo-del-tiempo)
17. [ms_print y massif-visualizer](#17-ms_print-y-massif-visualizer)
18. [jemalloc profiling integrado](#18-jemalloc-profiling-integrado)
19. [mimalloc: alternativa moderna con stats](#19-mimalloc-alternativa-moderna-con-stats)
20. [Profiling de memoria en Rust sin herramientas externas](#20-profiling-de-memoria-en-rust-sin-herramientas-externas)
21. [malloc_stats, mallinfo2, /proc/self/status](#21-malloc_stats-mallinfo2-procselfstatus)
22. [Peak RSS vs peak heap: la gran confusión](#22-peak-rss-vs-peak-heap-la-gran-confusión)
23. [Fragmentation: el enemigo invisible](#23-fragmentation-el-enemigo-invisible)
24. [Comparativa heaptrack vs DHAT vs massif](#24-comparativa-heaptrack-vs-dhat-vs-massif)
25. [Patrones tóxicos de memoria](#25-patrones-tóxicos-de-memoria)
26. [Errores comunes al profilear memoria](#26-errores-comunes-al-profilear-memoria)
27. [Programa de práctica: mem_hog.c](#27-programa-de-práctica-mem_hogc)
28. [Ejercicios](#28-ejercicios)
29. [Checklist de validación](#29-checklist-de-validación)
30. [Navegación](#30-navegación)

---

## 1. Por qué el profiling de memoria es distinto

El profiling de CPU (perf, callgrind, flamegraphs) responde una pregunta:
**¿dónde se gasta el tiempo?** El profiling de memoria responde una familia de
preguntas mucho más matizada:

- ¿Cuánta memoria usa el programa en su **pico máximo**?
- ¿Qué función **asigna más bytes totales**?
- ¿Qué función **asigna más veces** (aunque en bytes sea poco)?
- ¿Qué asignaciones **siguen vivas al final** (leaks o caches intencionales)?
- ¿Cuántas asignaciones son **temporales** (malloc+free casi inmediato)?
- ¿Cuánto tiempo **vive** cada allocation?
- ¿Con qué **frecuencia se accede** a cada bloque asignado?

Cada una de estas preguntas necesita instrumentación distinta. Una herramienta como
`perf` contando allocations puede responder "dónde se asigna" pero no "cuánto vive".
Una herramienta como massif muestra snapshots totales pero no granularidad por
llamada.

**Regla fundamental**: "usar más memoria" no es inherentemente malo. Usar memoria
**mal** sí lo es. El profiling de memoria existe para distinguirlo.

Ejemplos de "mal uso":

- Un servidor HTTP que asigna **1 MB por request** (debería reciclar buffers).
- Un parser que asigna **un String por token** cuando podría usar slices.
- Un loop que hace **malloc+free dentro del loop** (churn).
- Un cache sin límite que crece **linealmente con el tiempo** (leak efectivo).
- Un programa que **duplica estructuras** cuando podría compartirlas con `Rc`/`Arc`.

---

## 2. Conceptos clave: allocation, peak, RSS, heap

Antes de usar cualquier herramienta, hay que distinguir conceptos que la gente
confunde constantemente:

### Allocation (asignación)

Una llamada a `malloc`, `calloc`, `realloc`, `new`, `Box::new`, `Vec::with_capacity`,
etc. Produce un puntero a un bloque de memoria que el programa usa. Cada allocation
tiene:
- **Tamaño solicitado** (bytes pedidos por el programa).
- **Tamaño efectivo** (bytes que el allocator reserva: siempre ≥ solicitado, por
  alineación y metadata).
- **Dirección** (puntero devuelto).
- **Call stack en el momento** (quién pidió la memoria).
- **Tiempo de vida**: desde el malloc hasta el free correspondiente.

### Total bytes allocated

Suma de todos los tamaños pedidos a lo largo de la ejecución. Si haces 1000
mallocs de 1KB, son 1 MB totales, aunque solo 1KB estén vivos a la vez (si haces
free antes del siguiente malloc).

### Live bytes / live allocations

Bytes/allocations que aún no han sido liberadas **en un instante dado**. Cambia
en el tiempo.

### Peak heap / peak memory

El **máximo de live bytes** alcanzado durante la ejecución. Es lo que determina
cuánta RAM necesita tu programa para correr.

### RSS (Resident Set Size)

La memoria **física** que el SO le da al proceso. Incluye:
- Heap (lo que allocator usa).
- Stack (crece con las llamadas).
- Código y datos del binario (incluido código mapeado de librerías).
- Buffers de kernel asociados al proceso.
- Memoria mapeada con `mmap`.

`RSS ≠ live heap bytes`. RSS suele ser mayor porque:
1. El allocator retiene memoria liberada para futuras asignaciones.
2. Fragmentación: el heap tiene huecos inutilizables.
3. Código y datos del binario.
4. Librerías cargadas dinámicamente.

### Virtual memory (VSZ)

Memoria reservada por el SO para el proceso, incluyendo regiones **no residentes**
(swap, memoria asignada pero no tocada). Casi nunca es la métrica relevante.

### Visualización

```
           Total bytes allocated      Live bytes                Peak      RSS
           (acumulado, siempre sube)  (sube y baja)             (max live)
                                      │
                                      │          peak ──────────┐
               ┌─────────────────────┐│                         │
               │        ┌────┐       ││                         │
     bytes     │     ┌──┘    │       ││                         │
               │  ┌──┘       └──┐    ││                         │
               │ ─┘              └───┘│─────────────────────────┘
               │                                                 ▲
               └──────────────────────────────────────── tiempo
```

El peak es lo que limita a cuánta RAM puedes correr. El total bytes allocated es
lo que mide la **presión sobre el allocator** (churn).

---

## 3. Preguntas que responde un memory profiler

Cada herramienta responde un subconjunto distinto. Taxonomía:

| Pregunta                                                          | heaptrack | DHAT | massif | jemalloc |
|-------------------------------------------------------------------|-----------|------|--------|----------|
| ¿Cuál es el peak heap?                                            | ✔        | ✔   | ✔     | ✔       |
| ¿Qué función asignó más bytes totales?                           | ✔        | ✔   | ✔     | ✔       |
| ¿Qué función hizo más llamadas a malloc?                         | ✔        | ✔   | ✗     | ✔       |
| ¿Qué allocations siguen vivas al final?                          | ✔        | ✔   | ✗     | ✔       |
| ¿Cuántas allocations son temporales (free pronto)?               | ✔        | ✔   | ✗     | ✗       |
| ¿Cuántas veces se **lee** cada allocation?                       | ✗        | ✔   | ✗     | ✗       |
| ¿Cuánto tiempo vive cada allocation?                             | parcial  | ✔   | ✗     | ✗       |
| ¿Cómo evoluciona el heap en el tiempo?                           | ✔        | ✗   | ✔     | parcial |
| ¿Cuál fue el stack en el peak?                                   | ✔        | ✗   | ✔     | ✗       |
| ¿Cuánto overhead tiene la herramienta?                           | bajo (5-10×) | alto (20-50×) | bajo | nulo |

Ninguna responde todo. En la práctica:
- **heaptrack** para análisis general (tu herramienta #1 en Linux).
- **DHAT** para preguntas de access density y liveness.
- **massif** cuando quieres ver evolución temporal con snapshots.
- **jemalloc** cuando el programa ya usa jemalloc en producción.

---

## 4. Panorama de herramientas

```
                      MEMORY PROFILING TOOLS
                      ══════════════════════

    ┌─────────────────────────────────────────────────────────┐
    │  BINARY / DBI                  INSTRUMENTATION          │
    ├─────────────────────────────────────────────────────────┤
    │  heaptrack      LD_PRELOAD    intercepta malloc/free   │
    │  DHAT           Valgrind      simulación completa      │
    │  massif         Valgrind      simulación + snapshots   │
    │  jemalloc       built-in      profiling del allocator  │
    │  mimalloc       built-in      profiling del allocator  │
    │  dhat-rs        crate Rust    instrumenta GlobalAlloc  │
    │  bytehound      LD_PRELOAD    alt. moderna a heaptrack │
    │  memray         CPython hook  para Python              │
    └─────────────────────────────────────────────────────────┘
```

Todas tienen algo en común: interceptan las llamadas al allocator. La diferencia
está en **cómo** (LD_PRELOAD vs DBI vs built-in) y **qué registran** (solo
tamaños, o también accesos, o también tiempo).

Para C y Rust bajo Linux, los dos pilares son **heaptrack** (rápido, uso diario) y
**DHAT** (exhaustivo, análisis profundo). Los veremos en detalle.

---

## 5. heaptrack: la herramienta de referencia en Linux

**heaptrack** fue creado por Milian Wolff (KDE) como alternativa rápida y moderna
a massif. Sus características clave:

- **Intercepta malloc/free via LD_PRELOAD** → no requiere recompilación.
- **Overhead bajo**: 5-10× en CPU, factor 2-3× en memoria.
- **Guarda stack traces completos** de cada allocation.
- **GUI rica** con flamegraphs, tablas, gráficos temporales.
- **Compresión zst** de los trace files.
- **Diff entre runs** integrado.
- **Detecta temporary allocations** automáticamente.
- **Funciona con C, C++, Rust, cualquier lenguaje que use libc malloc**.

Es la herramienta **default recomendada** para profiling de memoria en Linux.

---

## 6. Instalar y ejecutar heaptrack

### Instalación

**Fedora / RHEL:**
```bash
sudo dnf install heaptrack heaptrack-gui
```

**Debian / Ubuntu:**
```bash
sudo apt install heaptrack heaptrack-gui
```

**Arch:**
```bash
sudo pacman -S heaptrack
```

### Uso básico

```bash
# 1. Compilar con símbolos
gcc -O2 -g -o program program.c

# 2. Ejecutar bajo heaptrack
heaptrack ./program

# Output típico:
# heaptrack output will be written to "/home/user/heaptrack.program.12345.zst"
# starting application, this might take some time...
# ... [programa corre, algo más lento] ...
# heaptrack stats:
#   allocations:          823,456
#   leaked allocations:     1,234
#   temporary allocations: 456,789

# 3. Analizar
heaptrack --analyze heaptrack.program.12345.zst
# Abre heaptrack_gui
```

Si no tienes GUI (servidor remoto):

```bash
heaptrack_print heaptrack.program.12345.zst | less
```

Esto muestra un reporte en texto con todas las vistas principales.

### Adjuntar a proceso existente

```bash
heaptrack --pid $(pgrep program)
```

Útil para profilear un servicio ya corriendo (aunque pierde las asignaciones
previas al attach).

### Flags útiles

```bash
# Nombre de salida personalizado
heaptrack --output my_trace.zst ./program

# Desactivar backtrace (menos overhead, menos info)
heaptrack --debug ./program

# Más profundidad en stacks (default 64)
HEAPTRACK_MAX_BACKTRACE_SIZE=128 heaptrack ./program
```

---

## 7. heaptrack GUI: vistas y qué significa cada una

`heaptrack_gui` es una aplicación Qt con varias pestañas. Cada una responde una
pregunta distinta:

### Summary

Vista de resumen con números clave:

```
Total allocations:        823,456
Temporary allocations:    456,789   (55.5%)
Peak heap memory:         124.5 MB
Peak RSS:                 189.2 MB
Total memory leaked:      12.3 MB
Bytes allocated in total: 5.7 GB    ← total acumulado, mucho mayor que peak
```

**Ratio de temporary**: si es >20%, tienes churn significativo. 55% es muy alto.

**Leak**: bytes asignados y no liberados al final. No siempre es bug (pueden ser
caches intencionales que el SO limpia al exit), pero revisar.

### Bottom-up

Lista plana de funciones ordenadas por bytes asignados (o allocations, o tiempo
vivido). Similar a `perf report` bottom-up.

Columnas:
- **Allocations**: cuántas veces llamó la función directamente a `malloc`.
- **Temporary**: cuántas de esas fueron temporary (malloc+free casi inmediato).
- **Peak**: contribución al peak.
- **Leaked**: bytes que dejó asignados al final.
- **Allocated**: total acumulado de bytes.

Click en cualquier fila para ver el call graph que lleva a ella.

### Top-down

Inverso: empieza desde `main` y expande hacia hojas. Útil para entender "¿qué
parte del programa es responsable del peak?".

### Caller/Callee

Similar a kcachegrind: selecciona una función y ve quién la llama y a quién llama.

### Flame Graph

Allocation flamegraph: x = fracción de bytes asignados, y = profundidad del stack.
**No es el mismo que un CPU flamegraph**. La anchura aquí es **bytes asignados**,
no tiempo de CPU.

Misma interpretación visual (mesetas = hotspots de asignación), pero la acción a
tomar es distinta: en lugar de optimizar velocidad, reduces allocations.

### Consumed

Gráfico temporal del **live heap** a lo largo del tiempo. Muestra:
- Curva del heap activo (sube y baja).
- Pico marcado.
- Eventos de alta actividad.

Esta es la vista para entender la **dinámica temporal** del heap. Picos, plateaus,
crecimientos sostenidos (leaks), sierra (churn).

### Allocations

Gráfico temporal del **número de allocations** vivas a lo largo del tiempo.
Diferente del anterior (que mide bytes). Puedes tener 1 M de allocations de 1 byte
cada una → pocas en bytes pero enorme presión sobre el allocator.

### Temporary Allocations

Gráfico temporal de **temporary allocations por segundo**. Picos significan
código que hace churn (malloc+free en loops).

### Sizes

Histograma de tamaños de allocation. Revela el **patrón de uso**:

- Picos en tamaños pequeños (16, 32, 64 bytes): estructuras fijas.
- Cola larga: pocas asignaciones grandes.
- Picos en potencias de 2: casi siempre buckets del allocator.

---

## 8. Allocation flamegraphs en heaptrack

El flamegraph en heaptrack tiene un selector en la parte superior que cambia **qué
métrica** se visualiza:

```
  Cost: [Peak ▼] [Allocated ▼] [Temporary ▼] [Leaked ▼] [Allocations ▼]
```

Cada modo genera un flamegraph distinto con la misma estructura de stacks:

- **Peak**: fracciones de bytes vivos en el momento del peak.
  Responde: "¿quién ocupa la memoria en el peor momento?"

- **Allocated**: total acumulado (puede ser enorme).
  Responde: "¿quién pide más bytes en total?"

- **Temporary**: solo allocations que fueron liberadas rápido.
  Responde: "¿dónde está el churn?"

- **Leaked**: allocations aún vivas al final.
  Responde: "¿de dónde vienen los leaks?"

- **Allocations**: cuenta, no bytes.
  Responde: "¿quién llama más veces a malloc?"

**Las respuestas suelen ser distintas.** Una función puede dominar "Allocations" sin
aparecer en "Peak" (muchas pequeñas temporary), o dominar "Peak" con una sola
asignación enorme sin aparecer en "Allocations".

Esta capacidad de ver la misma información bajo **cinco lentes distintas** es
invaluable y única de heaptrack entre las herramientas.

---

## 9. Detectar temporary allocations

**Temporary allocations** son malloc seguidos de free con poco trabajo en medio.
Son el síntoma clásico del **churn**:

```c
void process_items(Item *items, int n) {
    for (int i = 0; i < n; i++) {
        char *buf = malloc(256);          // ← temporary
        sprintf(buf, "Item %d", items[i].id);
        log_message(buf);
        free(buf);                         // ← liberada inmediatamente
    }
}
```

Cada iteración: malloc 256B, usa, free. El allocator lo maneja bien (reutiliza el
mismo bloque), pero hay overhead por llamada. Con 1M items, son 1M llamadas a
malloc/free solo para logging.

Cómo detectar: en heaptrack GUI, pestaña **Temporary Allocations**, vista
**Bottom-up** ordenada por "Temporary" descendente. La función culpable aparece
arriba.

Fix:
```c
void process_items(Item *items, int n) {
    char buf[256];                         // stack, sin allocation
    for (int i = 0; i < n; i++) {
        snprintf(buf, sizeof buf, "Item %d", items[i].id);
        log_message(buf);
    }
}
```

O reserva un buffer una vez fuera del loop:

```c
void process_items(Item *items, int n) {
    char *buf = malloc(256);
    for (int i = 0; i < n; i++) {
        sprintf(buf, "Item %d", items[i].id);
        log_message(buf);
    }
    free(buf);
}
```

### Criterio "temporary"

heaptrack marca como temporary cualquier allocation que se libera **antes de
cualquier otra allocation distinta**. Es una heurística imperfecta pero útil: detecta
el patrón "malloc/usa/free sin nada en medio".

Un ejemplo clásico: **Rust `.to_string()` dentro de un loop**. Cada iteración crea
un `String` temporal. Si `.to_string()` no es necesario (un `&str` bastaría), es
churn puro.

---

## 10. Detectar leaks con heaptrack

Al final del run, heaptrack reporta allocations aún vivas. No todas son leaks:

- **Caches intencionales**: el programa mantiene objetos en memoria a propósito.
- **Globals lazy**: `static` initializado una vez, nunca liberado.
- **Sub-allocators**: pools que liberan todo al exit.
- **Atexit handlers**: estructuras que deberían limpiarse pero no lo hicieron.

heaptrack no distingue estos casos automáticamente. Los reporta como "leaked" pero
es tu trabajo interpretar.

En el GUI, pestaña **Flame Graph** con Cost = **Leaked**: muestra el call graph
que creó allocations no liberadas.

Stack traces típicos en "Leaked":
```
main
 └─ initialize_cache
     └─ build_lookup_table
         └─ malloc(8MB)     ← leak o cache intencional
```

Si el 8MB es una `hash_table` creada en `initialize` y nunca liberada hasta el
exit, es intencional. Si es un buffer que se debía liberar pero alguien olvidó
`free(hash_table)` en cleanup, es leak real.

Para detectar leaks **reales** (bugs), **memcheck** de Valgrind es mejor
herramienta. heaptrack te dice "quedaron allocs vivos"; memcheck te dice "quedaron
allocs vivos Y son inalcanzables desde cualquier puntero".

---

## 11. Comparar dos runs con heaptrack

Para detectar regresiones, heaptrack soporta comparación directa:

```bash
# Run antes del cambio
heaptrack --output before.zst ./program
# Modificar código
heaptrack --output after.zst ./program
# Abrir en diff mode
heaptrack_gui --diff before.zst after.zst
```

El GUI muestra **deltas**: cuántos bytes más/menos por función. Rojo = más memoria
(peor), verde = menos (mejor).

Esto es lo que permite hacer **CI regression tests para memoria**:

```bash
#!/bin/bash
# heap-regression-test.sh

heaptrack --output current.zst ./benchmark
PEAK_CURRENT=$(heaptrack_print current.zst | grep "peak heap memory" | awk '{print $4}')
PEAK_BASELINE=$(heaptrack_print baseline.zst | grep "peak heap memory" | awk '{print $4}')

if (( PEAK_CURRENT > PEAK_BASELINE * 105 / 100 )); then
    echo "HEAP REGRESSION: $PEAK_CURRENT vs baseline $PEAK_BASELINE"
    exit 1
fi
```

---

## 12. DHAT: Dynamic Heap Analysis Tool de Valgrind

**DHAT** (Dynamic Heap Analysis Tool) es parte de Valgrind desde 3.15. Es una
herramienta profunda para análisis de memoria con información que ninguna otra
herramienta ofrece:

```bash
valgrind --tool=dhat ./program
```

DHAT registra, para cada allocation:

1. **Tamaño solicitado** (igual que heaptrack).
2. **Stack al momento del malloc** (igual que heaptrack).
3. **Stack al momento del free** (más que heaptrack).
4. **Tiempo vivido** en "instructions executed" (no segundos).
5. **Accesos de lectura** durante su vida (R).
6. **Accesos de escritura** durante su vida (W).
7. **Bytes nunca leídos ni escritos** (dead bytes).
8. **Alignment** del bloque.

Los puntos 5-7 son **únicos de DHAT**. Ninguna otra herramienta te dice cuántas
veces se tocó cada byte asignado.

### Overhead

DHAT corre bajo Valgrind → overhead similar a callgrind (30-50×). No usar en
producción. Usar en benchmarks con inputs reducidos.

---

## 13. Métricas únicas de DHAT: access density, liveness

### Access density

Para cada allocation, DHAT calcula:

```
read_rate  = bytes_read  / (size * time_alive)
write_rate = bytes_write / (size * time_alive)
```

Una allocation con alta density se **usa activamente**. Una con baja density está
"sentada sin trabajar". Extremo: density = 0 → bytes asignados pero nunca tocados
(puro desperdicio).

### Dead bytes

Bytes que el programa **nunca leyó ni escribió** durante la vida de la allocation:

```
Total bytes allocated: 1024 (estructura Foo)
Bytes read:             896
Bytes written:           768
Dead bytes:              128  ← nunca se tocaron
```

128 bytes de la estructura se asignaron y nunca se usaron. Posibles causas:
- Padding de alineación.
- Campos opcionales que en este run no se usaron.
- Over-allocation en buffers (pediste 1024 pero usaste 896).
- Estructuras inmaduras con campos legacy.

### Liveness (tiempo vivo)

Cada allocation tiene un "birth" (malloc) y "death" (free), medido en instrucciones
ejecutadas entre ambos. DHAT categoriza:

- **Short-lived**: pocos instructions → candidato a stack o pool.
- **Long-lived**: millones de instructions → probablemente estructura real del
  programa.
- **Forever-lived**: nunca murió → puede ser leak o global intencional.

Percentil típico interesante: "99.9% de las allocations viven <10K instructions" →
te dice que casi todo es temporal y un arena allocator sería óptimo.

### Peak contribution

Cada allocation contribuye al peak si estaba viva en el momento del peak. DHAT
calcula cuánto contribuyó cada stack al peak → te dice exactamente qué fracción
del peak vino de cada origen.

---

## 14. Interpretar el output de DHAT

DHAT genera `dhat.out.<PID>` en formato JSON, más un resumen en stderr:

```
==12345== Total:     5,678,901 bytes in 23,456 blocks
==12345== At t-gmax: 1,234,567 bytes in 789 blocks
==12345== At t-end:     12,345 bytes in 6 blocks
==12345== Reads:    98,765,432 bytes
==12345== Writes:   54,321,098 bytes
```

- **Total**: bytes y bloques asignados en total (acumulado).
- **At t-gmax**: en el momento del peak heap.
- **At t-end**: al final del programa (lo que queda = leaks o caches).
- **Reads/Writes**: accesos totales a bloques gestionados por malloc.

El JSON contiene todas las allocations con su stack trace y métricas. Se lee con:

```bash
valgrind --tool=dhat --dhat-out-file=dhat.json ./program

# Abrir en el viewer web
xdg-open /usr/libexec/valgrind/dh_view.html
# En la página, cargar dhat.json
```

---

## 15. DHAT viewer y dhat-rs

### DHAT viewer (C)

`dh_view.html` es una página HTML estática que viene con valgrind. Se abre con
cualquier navegador y carga el JSON localmente. Muestra:

- Árbol de call graphs con expansión.
- Para cada nodo: total bytes, peak, reads, writes, dead bytes.
- Ordenación por cualquier métrica.
- Filtrado por umbral.

Interfaz un poco cruda pero completa. No requiere servidor: es HTML+JS puros.

### dhat-rs: DHAT para Rust sin valgrind

`dhat` en crates.io es una **reimplementación del protocolo DHAT** puramente en
Rust. Se integra como `GlobalAllocator` y produce el mismo JSON que DHAT de
Valgrind, leíble con el mismo viewer.

Ventaja: **overhead mucho menor** que Valgrind. Solo registra cada
allocation/free, no simula instrucciones.

Setup:

```toml
[dev-dependencies]
dhat = "0.3"
```

```rust
#[global_allocator]
static ALLOC: dhat::Alloc = dhat::Alloc;

fn main() {
    let _profiler = dhat::Profiler::new_heap();
    // código a profilear
}
```

Ejecutar:
```bash
cargo run --release
# Genera dhat-heap.json
```

Abrir con:
```bash
# Clona el viewer de valgrind
git clone https://github.com/nnethercote/dh_view
xdg-open dh_view/dh_view.html
# Cargar dhat-heap.json
```

Limitación: dhat-rs **no** captura reads/writes (no tiene DBI). Solo bytes,
allocations, lifetimes, peak contribution. Pero es **mucho** más rápido que
Valgrind+DHAT, y suficiente para la mayoría de análisis.

Para Rust es la herramienta de elección: rápida, nativa, integrada.

---

## 16. Massif: snapshots de heap a lo largo del tiempo

**Massif** es otra herramienta de Valgrind, anterior a DHAT. Su enfoque es
distinto: en lugar de registrar cada allocation, toma **snapshots periódicos** del
estado completo del heap.

```bash
valgrind --tool=massif ./program
# Genera massif.out.<PID>
```

Opciones:

```bash
valgrind --tool=massif \
  --detailed-freq=5 \
  --threshold=0.5 \
  --massif-out-file=massif.out \
  ./program
```

- `--detailed-freq=5`: cada 5 snapshots, guarda el stack trace completo (los otros
  solo guardan el total).
- `--threshold=0.5`: funciones con <0.5% del heap no se muestran (reduce ruido).

### Qué captura cada snapshot

Por default, snapshots cuando hay cambios significativos en el heap, hasta 100 en
total. Cada snapshot tiene:

- Timestamp (en instrucciones ejecutadas).
- Tamaño total del heap.
- Tamaño por función que asignó.
- Call graph agregado (en snapshots "detailed").

### El snapshot "peak"

Uno de los snapshots se marca como "peak" (el más grande). Es el más importante:
muestra qué estaba vivo en el peor momento del heap.

---

## 17. ms_print y massif-visualizer

### ms_print (CLI)

```bash
ms_print massif.out.12345
```

Output típico:

```
    MB
8.456^                                                              #
     |                                                              #
     |                                                    ::::::::::#
     |                                          ::::::::::          #
     |                                      ::::
     |                            :::::::::::
     |                    ::::::::
     |           ::::::::::
     |    :::::::
     +----------------------------------------------------------->ki
     0                                                           500

Number of snapshots: 47
 Detailed snapshots: [3, 12, 28, 42 (peak), 46]
```

El gráfico ASCII muestra el crecimiento del heap en el tiempo. Los símbolos:
- `:` espacios de crecimiento.
- `#` marca el peak.

Debajo viene el desglose por snapshot detailed:

```
--------------------------------------------------------------------------------
  n        time(i)         total(B)   useful-heap(B) extra-heap(B)    stacks(B)
--------------------------------------------------------------------------------
 42    245,678,123        8,456,789        8,234,567       222,222            0
99.85% (8,234,567B) (heap allocations) malloc/new/new[], --alloc-fns, etc.
->45.12% (3,715,432B) in 3 places, all below massif's threshold (0.50%)
| 
->35.23% (2,901,234B) 0x4012AB: build_index (indexer.c:245)
| ->30.45% (2,508,765B) 0x401234: process_files (main.c:89)
|   ->30.45% (2,508,765B) 0x4010AB: main (main.c:42)
|     
->19.65% (1,617,901B) 0x4023BC: parse_document (parser.c:123)
  ->19.65% (1,617,901B) 0x401245: process_files (main.c:95)
    ->19.65% (1,617,901B) 0x4010AB: main (main.c:42)
```

En el snapshot 42 (peak), 8.2 MB estaban vivos. De esos:
- 45% en "lugares pequeños" agrupados.
- 35% vino de `build_index` → `process_files` → `main`.
- 19% vino de `parse_document` → `process_files` → `main`.

### massif-visualizer (GUI)

Aplicación Qt que abre directamente archivos `massif.out.*` y los muestra como
gráficos interactivos:

```bash
sudo dnf install massif-visualizer
massif-visualizer massif.out.12345
```

Mismo contenido que `ms_print` pero con zoom, navegación y exportación.

### Limitaciones de massif

- **No registra allocations individuales**: solo agregados por stack.
- **No detecta temporaries**: todo es snapshot, no hay concepto de "allocation
  vivida brevemente".
- **Overhead similar a callgrind** (20-50×).
- **Solo heap**: no ve mmap ni allocations nativas con mmap directo.

Para la mayoría de casos, **heaptrack reemplaza a massif** (más rápido, más
información, mejor GUI). Massif sigue siendo útil cuando quieres una vista **solo
temporal** simple y sin instalar paquetes adicionales (ya tienes valgrind).

---

## 18. jemalloc profiling integrado

**jemalloc** es un allocator alternativo (Facebook, Firefox, Rust opcional) con
profiling integrado. Ventaja sobre heaptrack: **no requiere herramienta externa**,
el profiling está built-in y el overhead es muy bajo.

### Compilar contra jemalloc

En C:
```bash
# Instalar
sudo dnf install jemalloc jemalloc-devel

# Compilar contra jemalloc
gcc -O2 -g -o program program.c -ljemalloc
```

O via LD_PRELOAD sin recompilar:
```bash
LD_PRELOAD=/usr/lib64/libjemalloc.so ./program
```

En Rust:
```toml
[dependencies]
tikv-jemallocator = "0.5"
```

```rust
#[global_allocator]
static GLOBAL: tikv_jemallocator::Jemalloc = tikv_jemallocator::Jemalloc;
```

### Activar profiling

Con variable de entorno:

```bash
MALLOC_CONF="prof:true,prof_active:true,prof_prefix:jeprof.out" \
  ./program
```

Opciones:
- `prof:true`: compila con prof support (requiere jemalloc compilado con
  `--enable-prof`).
- `prof_active:true`: empieza activo.
- `prof_prefix:PATH`: prefijo de los dumps.
- `lg_prof_sample:N`: sampling rate (1 cada 2^N bytes; 19 = cada 512KB).
- `prof_final:true`: dump al final del programa.

Al finalizar, jemalloc crea `jeprof.out.12345.0.f.heap` (u otros nombres según
momento del dump).

### Leer con jeprof

`jeprof` (parte de `gperftools` o jemalloc) genera visualizaciones:

```bash
# Resumen texto
jeprof --text ./program jeprof.out.12345.0.f.heap

# SVG como un pprof
jeprof --svg ./program jeprof.out.12345.0.f.heap > heap.svg

# Diff entre runs
jeprof --base=before.heap ./program after.heap
```

Output texto:

```
Total: 123.45 MB
  56.78  46.0%  46.0%  56.78  46.0% build_index
  34.56  28.0%  74.0%  91.34  74.0% process_files
  12.34  10.0%  84.0%  12.34  10.0% parse_document
```

Columnas similares a `perf report`: `flat%`, `sum%`, `cum%`.

### Cuándo usar jemalloc profiling

- Tu programa ya usa jemalloc → profiling gratuito, overhead mínimo.
- Producción: jemalloc prof es mucho más barato que heaptrack/DHAT.
- Continuous profiling: dump periódico para monitoring.

No cuándo:
- No usas jemalloc: cambiar allocator solo para profilear cambia el comportamiento.
- Necesitas temporary detection: jemalloc prof no lo tiene.

---

## 19. mimalloc: alternativa moderna con stats

**mimalloc** (Microsoft Research) es otro allocator moderno con foco en
performance. Tiene profiling más simple que jemalloc: solo stats globales, no
profiling con stack traces.

```bash
sudo dnf install mimalloc
LD_PRELOAD=/usr/lib64/libmimalloc.so ./program
```

Para stats:
```bash
MIMALLOC_SHOW_STATS=1 LD_PRELOAD=/usr/lib64/libmimalloc.so ./program
```

Output al exit:
```
heap stats:    peak      total    current    unit count   total_size
  normal         256      1024       128       128   8       1024
  huge            0         0         0         0    0          0
  total         256      1024       128

malloc requested:            4,098 kB
```

No es comparable con heaptrack en profundidad, pero sirve para ver "cuánta
presión de malloc hay" sin herramientas externas.

---

## 20. Profiling de memoria en Rust sin herramientas externas

Rust puede autoinstrumentarse sin valgrind ni heaptrack gracias a `GlobalAllocator`:

```rust
use std::alloc::{GlobalAlloc, Layout, System};
use std::sync::atomic::{AtomicUsize, Ordering};

static ALLOCATED: AtomicUsize = AtomicUsize::new(0);
static PEAK: AtomicUsize = AtomicUsize::new(0);
static COUNT: AtomicUsize = AtomicUsize::new(0);

struct TrackingAlloc;

unsafe impl GlobalAlloc for TrackingAlloc {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let ptr = System.alloc(layout);
        if !ptr.is_null() {
            let new = ALLOCATED.fetch_add(layout.size(), Ordering::Relaxed)
                    + layout.size();
            COUNT.fetch_add(1, Ordering::Relaxed);
            let mut cur_peak = PEAK.load(Ordering::Relaxed);
            while new > cur_peak {
                match PEAK.compare_exchange(cur_peak, new,
                    Ordering::Relaxed, Ordering::Relaxed) {
                    Ok(_) => break,
                    Err(p) => cur_peak = p,
                }
            }
        }
        ptr
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        System.dealloc(ptr, layout);
        ALLOCATED.fetch_sub(layout.size(), Ordering::Relaxed);
    }
}

#[global_allocator]
static GLOBAL: TrackingAlloc = TrackingAlloc;

fn main() {
    let _ = vec![0u8; 10_000_000];
    println!("Allocations:  {}", COUNT.load(Ordering::Relaxed));
    println!("Peak bytes:   {}", PEAK.load(Ordering::Relaxed));
    println!("Live at end:  {}", ALLOCATED.load(Ordering::Relaxed));
}
```

Esto da tres números: cuántas allocations hubo, pico de bytes, bytes vivos al
final. Sin stack traces, pero suficiente para assertions en tests:

```rust
#[test]
fn no_allocations_in_hot_path() {
    let before = COUNT.load(Ordering::Relaxed);
    hot_path();
    let after = COUNT.load(Ordering::Relaxed);
    assert_eq!(before, after, "hot_path allocated!");
}
```

Patrón útil en benchmarks: garantizar que una función no aloca en absoluto
(zero-alloc invariant).

Para algo más elaborado con stack traces, usa `dhat-rs` (§15). Para producción
con sampling barato, `tracing-memory` o `tracking-allocator` crates.

---

## 21. malloc_stats, mallinfo2, /proc/self/status

Sin herramientas externas en C, la libc expone APIs para introspeccionar el
allocator:

### malloc_stats (glibc)

```c
#include <malloc.h>

int main(void) {
    /* ... código ... */
    malloc_stats();  /* imprime a stderr */
    return 0;
}
```

Output:
```
Arena 0:
system bytes     =     135168
in use bytes     =      78032
Total (incl. mmap):
system bytes     =     135168
in use bytes     =      78032
max mmap regions =          0
max mmap bytes   =          0
```

### mallinfo2

API más estructurada (reemplaza la antigua `mallinfo` que tenía campos `int`):

```c
#include <malloc.h>

struct mallinfo2 mi = mallinfo2();
printf("total arena:    %zu\n", mi.arena);
printf("in-use bytes:   %zu\n", mi.uordblks);
printf("free bytes:     %zu\n", mi.fordblks);
printf("mmap regions:   %zu\n", mi.hblks);
printf("mmap bytes:     %zu\n", mi.hblkhd);
```

### /proc/self/status

El kernel expone métricas del proceso:

```c
#include <stdio.h>

void print_mem_status(void) {
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "Vm", 2) == 0) fputs(line, stderr);
    }
    fclose(f);
}
```

Campos relevantes:
- `VmPeak`: pico de VSZ.
- `VmSize`: VSZ actual.
- `VmHWM`: pico de RSS ("high water mark").
- `VmRSS`: RSS actual.
- `VmData`: data + heap.
- `VmStk`: stack.
- `RssAnon`: anónimas (heap, stack).
- `RssFile`: mapeos de ficheros (código, mmap de data).

Para monitoreo continuo sin profiler, esto es suficiente: lee `VmRSS` y `VmHWM`
periódicamente para ver evolución.

### getrusage

```c
#include <sys/resource.h>
struct rusage ru;
getrusage(RUSAGE_SELF, &ru);
printf("Peak RSS: %ld KB\n", ru.ru_maxrss);  /* en KB en Linux */
```

`ru_maxrss` es la misma métrica que `VmHWM`: pico histórico de RSS.

---

## 22. Peak RSS vs peak heap: la gran confusión

Confusión habitual:

```
heaptrack reporta: peak heap = 100 MB
ps reporta:        RSS = 180 MB
¿Qué pasa con los 80 MB faltantes?
```

Explicación:

```
┌────────────────────────────────────────────┐
│                     RSS                    │
│  ┌────────────────────────────────────┐    │
│  │  Heap (peak live)                 │    │
│  │  100 MB                           │    │
│  ├────────────────────────────────────┤    │
│  │  Heap (reservado por allocator,   │    │
│  │   liberado pero no devuelto)       │    │
│  │   30 MB                            │    │
│  ├────────────────────────────────────┤    │
│  │  Stack + TLS                       │    │
│  │   10 MB                            │    │
│  ├────────────────────────────────────┤    │
│  │  Código del binario                │    │
│  │   5 MB                             │    │
│  ├────────────────────────────────────┤    │
│  │  Librerías cargadas (libc, etc.)   │    │
│  │   20 MB                            │    │
│  ├────────────────────────────────────┤    │
│  │  mmap regions (archivos, etc.)     │    │
│  │   15 MB                            │    │
│  └────────────────────────────────────┘    │
└────────────────────────────────────────────┘
         Total RSS ≈ 180 MB
```

Heaptrack mide **exactamente** "heap live" (la parte de arriba). El SO reporta
RSS que incluye **todo lo que está en RAM físicamente**.

Implicaciones:

- Si tu programa aparentemente "usa" 180 MB según `ps` pero heaptrack dice 100 MB,
  no es error. El resto es overhead estructural.
- Si quieres reducir RSS, no basta con reducir peak heap: tienes que considerar
  también fragmentación, librerías cargadas, stack.
- `malloc_trim(0)` (glibc) devuelve memoria liberada del heap al kernel (reduce
  VmRSS). No siempre funciona: depende de si los bloques libres están al final
  del heap.

Para el usuario final, **RSS es lo que importa**: eso es la presión sobre el
sistema. Para el desarrollador, **peak heap** es lo optimizable directamente con
heaptrack.

---

## 23. Fragmentation: el enemigo invisible

Fragmentation ocurre cuando el heap tiene huecos libres pero de tamaños
inutilizables:

```
Heap layout (fragmentado):
[USED 100B][FREE 50B][USED 200B][FREE 30B][USED 80B][FREE 500B][USED 50B]

Si pides 600B, NO puedes reutilizar los 50+30+500=580 de huecos porque no son
contiguos. Tienes que pedir 600B nuevos al kernel. RSS crece.
```

Causas:
- Muchas asignaciones de tamaños muy variables entrelazadas con frees.
- Long-lived allocations en medio de short-lived.
- Liberar objetos "de en medio" del heap.

Herramientas:
- **heaptrack**: muestra la **diferencia** entre "heap used" y "heap reserved" →
  el gap es fragmentación.
- **glibc malloc_stats()**: reporta `fordblks` (free blocks) vs `uordblks` (in
  use). Alto fordblks sin malloc_trim success → fragmentación.
- **jemalloc**: el allocator en sí es mejor en fragmentación que glibc
  (arenas múltiples, bins de tamaño fino). Usar jemalloc ya mitiga este problema
  significativamente.

Mitigaciones:
1. **Usa un mejor allocator**: jemalloc o mimalloc reducen fragmentación ~30% vs
   glibc en cargas pesadas.
2. **Pool allocators** para objetos homogéneos: todos los `Foo` viven en la misma
   arena y el heap general no se ensucia.
3. **Arenas por fase**: all allocations de la fase 1 en arena A, fase 2 en arena
   B. Al acabar fase 1, destruyes arena A entera.
4. **Reducir variabilidad de tamaños**: alinear a potencias de 2.

---

## 24. Comparativa heaptrack vs DHAT vs massif

| Dimensión              | heaptrack          | DHAT                | massif             |
|------------------------|--------------------|--------------------|-----------------------|
| Mecanismo              | LD_PRELOAD         | Valgrind DBI        | Valgrind DBI       |
| Overhead CPU           | 5-10×              | 30-50×              | 20-50×             |
| Overhead memoria       | 2-3×               | 3-5×                | 2-3×               |
| Granularidad           | Allocation         | Allocation          | Snapshot agregado  |
| Stack traces           | Sí (completos)     | Sí (completos)      | Sí (agregados)     |
| Temporary detection    | Sí (automático)    | Parcial (lifetime)  | No                 |
| Read/write tracking    | No                 | Sí (único)          | No                 |
| Dead bytes detection   | No                 | Sí (único)          | No                 |
| Allocation flamegraph  | Sí (5 modos)       | Árbol               | No                 |
| Temporal evolution     | Sí (continuo)      | Parcial             | Sí (discreto)      |
| Diff entre runs        | Sí (integrado)     | Manual              | Manual             |
| GUI moderna            | Sí (Qt)            | HTML simple         | Qt (visualizer)    |
| CLI available          | Sí (heaptrack_print)| Sí (JSON)          | Sí (ms_print)      |
| Multithread            | Sí                 | Sí (serializado)    | Sí (serializado)   |
| Leak detection         | Sí (básico)        | Sí (mejor)          | Parcial            |
| Rust soporte           | Sí (via libc)      | Sí                  | Sí                 |

**Regla de oro**:

1. **Primera pasada**: heaptrack. Rápido, GUI clara, responde 80% de las
   preguntas.
2. **Si necesitas entender uso real de bytes**: DHAT. Detecta allocations
   desperdiciadas y dead bytes.
3. **Si necesitas evolución temporal fina**: heaptrack ya lo hace; massif solo
   si heaptrack no está disponible.
4. **Para Rust específicamente**: dhat-rs como GlobalAlloc. Mismo viewer que
   DHAT sin overhead de Valgrind.

---

## 25. Patrones tóxicos de memoria

Cosas a buscar cuando profileas memoria. Si ves alguno de estos patrones, tienes
trabajo:

### 25.1. Allocation churn en loop caliente

```
Heap consumed graph:
 ▓▓░░▓▓░░▓▓░░▓▓░░▓▓░░▓▓░░▓▓░░   ← sierra rápida
```

Sube y baja muchas veces por segundo. Alloc+free en cada iteración. Fix: sacar la
allocation fuera del loop, usar stack, usar arena.

### 25.2. Leak lineal

```
Heap consumed graph:
         /
        /
       /
      /
     /
    /
 __/
```

Crecimiento monótono sin bajar. Es leak real, cache sin límite, o builder que
acumula. Fix: añadir límite, implementar cleanup, o aceptar que es cache intencional.

### 25.3. Staircase (builder pattern)

```
Heap consumed graph:
       ___
      /
     /___
    /
   /___
  /
 /
```

Crecimiento por escalones. Típico de builder que reserva más espacio (`Vec::push`
con reallocs) o de lector que carga en chunks. Normal si los escalones son pocos.
Preocupante si el número de escalones escala con el input.

### 25.4. Double buffer con crecimiento

```
Heap consumed graph:
    peak ─┐
         ┌┤
         ││
         │└────
         │
       ──┘
```

Pico temporal muy alto. Causa común: `Vec::clone()` que duplica momentáneamente
antes de soltar el original. O `let new = transform(&old); drop(old);` donde new
y old coexisten brevemente. Fix: `std::mem::replace` o in-place transforms.

### 25.5. Dead bytes altos (DHAT)

```
Total bytes allocated: 1024 (struct Foo)
Bytes read:             256   ← solo 25% se leyeron
Bytes written:          128
Dead bytes:             640   ← 62% sin tocar
```

Estructura sobredimensionada para el uso real. Fix: reducir padding, convertir a
SoA, lazy fields.

### 25.6. Temporary ratio > 30%

```
Total allocations: 1_000_000
Temporary:         650_000   (65%)
```

Alto churn. Cada 2 allocations, una es temporal. Fix: pool, arena, stack
allocations, reutilización de buffers.

### 25.7. Muchas allocations pequeñas (death by thousand cuts)

```
Sizes histogram:
  16 B:  500,000 allocs   ← enorme cantidad de tiny allocs
  32 B:  300,000 allocs
  64 B:  100,000 allocs
  ...
```

Overhead de malloc por llamada es ~30-50 ns. Con 1M allocs, son 30-50 ms solo
en llamadas al allocator. Fix: object pool, arena, batch allocation.

---

## 26. Errores comunes al profilear memoria

### Error 1 — confundir peak heap con peak RSS

Ver §22. No son lo mismo. Heaptrack mide el primero, `ps` el segundo.

### Error 2 — olvidar debug symbols

Sin `-g`, los stacks son direcciones crudas. heaptrack_gui muestra `0x4012ab` en
lugar de `build_index`. Solución: siempre compilar con `-g` para profiling,
incluso en release.

### Error 3 — profilear un build debug

Igual que con CPU profiling: un build `-O0` asigna distinto a un build `-O2` (el
compilador no inlinea, no hace return value optimization, no elimina Box
innecesarios). Los hallazgos no son transferibles. Profilea siempre builds
optimizados.

### Error 4 — asumir que "leaked" = bug

Programas con static globals, caches, `OnceCell`, etc. dejan allocations vivas al
exit. Eso es normal. Para detectar leaks reales usa memcheck, no heaptrack.

### Error 5 — no considerar overhead del allocator

El allocator tiene su propio overhead: metadata por bloque (16-24 bytes en
glibc), alineación, bins de tamaño fijo. Si asignas 1M de struct de 40 bytes, el
heap usa mucho más que 40 MB: fácilmente 60-80 MB. No es "leak", es overhead del
allocator.

### Error 6 — profilear datos sintéticos irreales

Un benchmark con `vec![0u8; 1_000_000]` no refleja el comportamiento real del
programa con datos reales. Usa inputs representativos.

### Error 7 — profilear sin warmup

Las primeras asignaciones pagan overhead de inicialización del allocator
(crear arenas, bins). Si solo profileas 100 ms al inicio, verás un perfil sesgado.
Run por lo menos unos segundos.

### Error 8 — interpretar histograma de sizes ingenuamente

Un pico en 16 bytes no siempre es "muchas allocations pequeñas". Puede ser metadata
del allocator en sí, o internal allocations de libstdc++/stdlib.

### Error 9 — comparar runs con inputs distintos

"Antes tenía 100 MB peak, después 200 MB" no es regresión si el input cambió. Fija
el input antes de comparar.

### Error 10 — no mirar los stacks

heaptrack te da el bottom-up list. El primero puede ser `malloc` o
`__libc_malloc`. Eso no es información útil: expande el call graph para ver quién
pide la memoria, no quién la entrega.

---

## 27. Programa de práctica: mem_hog.c

Programa con patrones deliberados para practicar con heaptrack y DHAT:

```c
/* mem_hog.c — demostraciones de patrones de memoria
 *
 * Compilar:
 *   gcc -O2 -g -o mem_hog mem_hog.c
 *
 * Profilar con heaptrack:
 *   heaptrack ./mem_hog
 *   heaptrack_gui heaptrack.mem_hog.*.zst
 *
 * Profilar con DHAT:
 *   valgrind --tool=dhat ./mem_hog
 *   xdg-open /usr/libexec/valgrind/dh_view.html
 *   (cargar dhat.out.<PID>)
 *
 * Profilar con massif:
 *   valgrind --tool=massif ./mem_hog
 *   massif-visualizer massif.out.<PID>
 *
 * Patrones demostrados:
 *  - good_pattern:      buffer reutilizado, un solo malloc
 *  - churn_pattern:     malloc+free en cada iteración (temporary alto)
 *  - leak_pattern:      allocations no liberadas (leak)
 *  - fragment_pattern:  allocations de tamaño variable entrelazadas (frag)
 *  - oversized_struct:  estructura con mucho padding sin usar (dead bytes)
 *  - builder_pattern:   Vec push con reallocs (staircase)
 *  - double_peak:       duplicación temporal antes de transform
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define N_ITEMS 20000
#define STR_LEN 256

/* ---- good_pattern: un solo buffer reutilizado ---- */

static void good_pattern(int n) {
    char *buf = malloc(STR_LEN);
    for (int i = 0; i < n; i++) {
        snprintf(buf, STR_LEN, "item_%d", i);
        /* usar buf */
        volatile size_t len = strlen(buf);
        (void)len;
    }
    free(buf);
}

/* ---- churn_pattern: malloc por iteración ---- */

static void churn_pattern(int n) {
    for (int i = 0; i < n; i++) {
        char *buf = malloc(STR_LEN);      /* temporary */
        snprintf(buf, STR_LEN, "item_%d", i);
        volatile size_t len = strlen(buf);
        (void)len;
        free(buf);                         /* ← temporary detectada por heaptrack */
    }
}

/* ---- leak_pattern: no libera ---- */

static char **leak_pattern(int n) {
    char **leaked = malloc(sizeof(char*) * n);
    for (int i = 0; i < n; i++) {
        leaked[i] = malloc(32);             /* never freed in this function */
        snprintf(leaked[i], 32, "leak_%d", i);
    }
    return leaked;  /* caller olvidará liberar → leak */
}

/* ---- fragment_pattern: sizes variables ---- */

static void fragment_pattern(int n) {
    void *ptrs[100];
    srand(42);
    for (int round = 0; round < n / 100; round++) {
        for (int i = 0; i < 100; i++) {
            size_t sz = 16 + (rand() % 512);  /* tamaños variables */
            ptrs[i] = malloc(sz);
        }
        /* free en orden no secuencial → fragmentación */
        for (int i = 0; i < 100; i += 2) {
            free(ptrs[i]);
            ptrs[i] = NULL;
        }
        for (int i = 1; i < 100; i += 2) {
            free(ptrs[i]);
            ptrs[i] = NULL;
        }
    }
}

/* ---- oversized_struct: padding + unused fields ---- */

typedef struct {
    int  id;              /* 4 bytes usado */
    char padding1[256];   /* NUNCA se lee */
    double values[32];    /* solo el primero se usa */
    char padding2[512];   /* NUNCA se lee */
    int  flag;            /* usado */
} Oversized;

static double oversized_struct(int n) {
    Oversized *arr = malloc(sizeof(Oversized) * n);
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        arr[i].id = i;
        arr[i].values[0] = (double)i * 0.5;
        arr[i].flag = (i % 2);
        sum += arr[i].values[0];
    }
    /* solo se leen id, values[0], flag → DHAT marcará el resto como dead */
    free(arr);
    return sum;
}

/* ---- builder_pattern: Vec growing with reallocs ---- */

typedef struct {
    int *data;
    size_t len;
    size_t cap;
} IntVec;

static void ivec_push(IntVec *v, int x) {
    if (v->len == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 8;
        v->data = realloc(v->data, sizeof(int) * v->cap);
    }
    v->data[v->len++] = x;
}

static void builder_pattern(int n) {
    IntVec v = {0};
    for (int i = 0; i < n; i++) {
        ivec_push(&v, i);
    }
    /* Heap gráfico: staircase por cada realloc (doble del anterior) */
    free(v.data);
}

/* ---- double_peak: duplicación temporal ---- */

static void double_peak(int n) {
    int *original = malloc(sizeof(int) * n);
    for (int i = 0; i < n; i++) original[i] = i;

    /* Copia temporal → peak = 2 × n × sizeof(int) */
    int *copy = malloc(sizeof(int) * n);
    memcpy(copy, original, sizeof(int) * n);

    /* Transform in place */
    for (int i = 0; i < n; i++) copy[i] *= 2;

    /* Liberar original después de crear copy */
    free(original);

    /* usar copy */
    volatile long sum = 0;
    for (int i = 0; i < n; i++) sum += copy[i];
    (void)sum;

    free(copy);
}

/* ---- main ---- */

int main(void) {
    fprintf(stderr, "=== good_pattern ===\n");
    good_pattern(N_ITEMS);

    fprintf(stderr, "=== churn_pattern ===\n");
    churn_pattern(N_ITEMS);

    fprintf(stderr, "=== leak_pattern ===\n");
    char **leak = leak_pattern(500);
    (void)leak;  /* intencionalmente no freed */

    fprintf(stderr, "=== fragment_pattern ===\n");
    fragment_pattern(N_ITEMS);

    fprintf(stderr, "=== oversized_struct ===\n");
    double r = oversized_struct(1000);
    (void)r;

    fprintf(stderr, "=== builder_pattern ===\n");
    builder_pattern(N_ITEMS * 10);

    fprintf(stderr, "=== double_peak ===\n");
    double_peak(N_ITEMS * 50);

    fprintf(stderr, "=== done ===\n");
    return 0;
}
```

### Qué deberías ver

**En heaptrack (Summary)**:
- Total allocations: ~30 000
- Temporary: altísimo (cerca del 50%) por `churn_pattern`
- Peak: dominado por `double_peak` (~800 KB)
- Leaked: 500 allocations del `leak_pattern`

**En heaptrack (Flame Graph, Cost=Temporary)**:
- `churn_pattern` ocupa casi todo el ancho del modo temporary.

**En heaptrack (Flame Graph, Cost=Peak)**:
- `double_peak` dominante (el pico real).
- `builder_pattern` contribuye al peak secundario.

**En heaptrack (Flame Graph, Cost=Leaked)**:
- `leak_pattern` ocupa casi todo el modo leaked.

**En heaptrack (Consumed gráfico temporal)**:
- Meseta ancha del `good_pattern` (constante baja).
- Sierra del `churn_pattern` (up/down rápido).
- Staircase del `builder_pattern` (pasos sucesivos).
- Pico alto del `double_peak` que cae en seco.
- Leak: el nivel base sube escalones permanentes.

**En DHAT (dh_view.html)**:
- `oversized_struct` aparece con muchos **dead bytes** (>60% del total de la
  estructura nunca se lee).
- `churn_pattern` muestra lifetime ultra corto en sus allocations.
- `leak_pattern` muestra lifetime "forever" en sus allocations.

**En massif (ms_print)**:
- Gráfico con el peak marcado en el momento de `double_peak`.
- Las contribuciones de cada pattern agregadas por función.

---

## 28. Ejercicios

### Ejercicio 1 — heaptrack first run

1. Compila `mem_hog.c` con `-O2 -g`.
2. Corre bajo heaptrack: `heaptrack ./mem_hog`.
3. Abre el archivo generado con `heaptrack_gui`.
4. En la pestaña Summary, anota:
   - Total allocations
   - Temporary percentage
   - Peak heap
   - Leaked bytes
5. ¿Cuál es el temporary percentage? ¿Qué función contribuye más?

### Ejercicio 2 — explorar los 5 modos del flamegraph

1. En heaptrack_gui, pestaña Flame Graph.
2. Cambia el Cost entre Peak, Allocated, Temporary, Leaked y Allocations.
3. Para cada modo, anota qué función está dominando.
4. Responde: ¿son las mismas en los 5 modos? ¿Por qué no?

### Ejercicio 3 — reparar el churn

1. Identifica `churn_pattern` como el principal contribuidor al modo Temporary.
2. Re-escribe `churn_pattern` para usar un buffer reutilizado (como
   `good_pattern`).
3. Re-profilea y compara:
   - Total allocations debería bajar drásticamente.
   - Peak debería no cambiar mucho.
   - Temporary debería ser casi 0 para esa función.

### Ejercicio 4 — reparar el double_peak

1. `double_peak` duplica temporalmente. Re-escríbelo para transformar in-place
   sin copia intermedia:
   ```c
   static void double_peak_fixed(int n) {
       int *data = malloc(sizeof(int) * n);
       for (int i = 0; i < n; i++) data[i] = i;
       for (int i = 0; i < n; i++) data[i] *= 2;
       /* ... */
       free(data);
   }
   ```
2. Re-profilea con heaptrack y compara el peak (debería reducirse a la mitad).

### Ejercicio 5 — cg_diff (manual)

1. Profilea la versión original: `heaptrack --output before.zst ./mem_hog`.
2. Aplica las correcciones de los ejercicios 3 y 4.
3. Profilea la versión corregida: `heaptrack --output after.zst ./mem_hog_fixed`.
4. Abre ambos en heaptrack_gui con `heaptrack_gui --diff before.zst after.zst`.
5. Confirma visualmente dónde mejoró y dónde no.

### Ejercicio 6 — DHAT y dead bytes

1. Ejecuta `valgrind --tool=dhat ./mem_hog`.
2. Abre el JSON en `dh_view.html`.
3. Busca la allocation de `oversized_struct`.
4. Anota: reads, writes, dead bytes.
5. Calcula el porcentaje de dead bytes sobre el total.
6. ¿Qué modificarías en `Oversized` para eliminar dead bytes?

### Ejercicio 7 — massif temporal

1. Ejecuta `valgrind --tool=massif --massif-out-file=massif.out ./mem_hog`.
2. Abre con `ms_print massif.out` o `massif-visualizer massif.out`.
3. Identifica:
   - El snapshot del peak.
   - Qué función está dominando en ese snapshot.
4. Compara cualitativamente con lo que vio heaptrack.

### Ejercicio 8 — dhat-rs en Rust (opcional)

1. Crea un proyecto Rust que replique algunos patterns (churn, leak, builder).
2. Añade `dhat = "0.3"` como dev-dep.
3. Instrumenta con `GlobalAlloc` y `Profiler::new_heap()`.
4. Ejecuta y carga el JSON en `dh_view.html`.
5. Comparar qué responde dhat-rs que no responde heaptrack sobre Rust.

---

## 29. Checklist de validación

Antes de actuar sobre hallazgos de memory profiling:

- [ ] Compilado con `-O2 -g` (release con debug info).
- [ ] Input representativo del uso real, no synthetic mínimo.
- [ ] Ejecución suficientemente larga para pasar fase de warmup del allocator.
- [ ] Profiler correcto para la pregunta:
  - heaptrack para "dónde se asigna".
  - DHAT para "cómo se usa".
  - massif para "cómo evoluciona en el tiempo" (o heaptrack).
- [ ] Diferencia clara entre **peak heap** y **peak RSS** en la interpretación.
- [ ] "Leaked" revisado manualmente (pueden ser caches intencionales).
- [ ] Comparación con al menos 2 runs para verificar reproducibilidad.
- [ ] Si hay regresión, comparada con baseline usando diff integrado.
- [ ] Considerado overhead del allocator cuando calculas eficiencia.
- [ ] Si el programa es multithread, considerada distribución por hilo.
- [ ] Confirmado que el "hotspot" también aparece en análisis de CPU (si importa
      latencia) o solo en memoria (si importa RSS).

---

## 30. Navegación

| ← Anterior | ↑ Sección | Siguiente → |
|------------|-----------|-------------|
| [T03 — Valgrind/Callgrind](../T03_Callgrind/README.md) | [S02 — Profiling](../) | [S03 — Benchmarking Guiado por Hipótesis](../../S03_Benchmarking_guiado_por_hipotesis/T01_Formular_hipotesis/README.md) |

**Capítulo 4 — Sección 2: Profiling de Aplicaciones — Tópico 4 de 4**

**Fin de la Sección 2 — Profiling de Aplicaciones.**

Has completado el recorrido por las cuatro perspectivas del profiling:

1. **perf en Linux** (T01) — muestreo estadístico de CPU con contadores hardware.
2. **Flamegraphs** (T02) — visualización de miles de stacks en una imagen.
3. **Valgrind/Callgrind** (T03) — simulación exacta para contar instrucciones y
   cache.
4. **Profiling de memoria** (T04) — heap, allocations, peak, dead bytes, leaks.

Entre las cuatro cubres **CPU, cache, instrucciones, llamadas, memoria y
tiempo**. Cuando un programa es lento, sabes por dónde empezar. Cuando usa
demasiada memoria, sabes qué preguntar. Cuando necesitas reproducibilidad para
CI, sabes qué herramienta es determinista.

El siguiente paso (Sección 3) es la disciplina que convierte todas estas
herramientas en un método: **Benchmarking Guiado por Hipótesis**. No basta con
medir; hay que formular una predicción concreta, diseñar un experimento que la
confirme o refute, y actuar en consecuencia. Sin hipótesis, profiling es turismo.
Con hipótesis, es ingeniería.
