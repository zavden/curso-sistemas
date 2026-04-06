# T02 — Flamegraphs

> **Bloque 17 · Capítulo 4 · Sección 2 · Tópico 2**
> Visualización jerárquica de profiling: del stack trace al rectángulo, encontrar hotspots con la vista.

---

## Índice

1. [Qué es un flamegraph](#1-qué-es-un-flamegraph)
2. [El problema que resuelve](#2-el-problema-que-resuelve)
3. [Anatomía de un flamegraph](#3-anatomía-de-un-flamegraph)
4. [Cómo se construye: el pipeline completo](#4-cómo-se-construye-el-pipeline-completo)
5. [Instalación de FlameGraph de Brendan Gregg](#5-instalación-de-flamegraph-de-brendan-gregg)
6. [Flamegraph clásico con perf + flamegraph.pl](#6-flamegraph-clásico-con-perf--flamegraphpl)
7. [Interpretar los ejes: X alfabético, Y profundidad](#7-interpretar-los-ejes-x-alfabético-y-profundidad)
8. [Colores: significado real y falsas pistas](#8-colores-significado-real-y-falsas-pistas)
9. [Patrones visuales: mesetas, torres, cerillas](#9-patrones-visuales-mesetas-torres-cerillas)
10. [Resolución de símbolos y debug info](#10-resolución-de-símbolos-y-debug-info)
11. [Frame pointers vs DWARF vs LBR](#11-frame-pointers-vs-dwarf-vs-lbr)
12. [Profilar C: desde compilación hasta SVG](#12-profilar-c-desde-compilación-hasta-svg)
13. [Profilar Rust con cargo-flamegraph](#13-profilar-rust-con-cargo-flamegraph)
14. [Profilar Rust en release sin herramientas externas](#14-profilar-rust-en-release-sin-herramientas-externas)
15. [Icicle graphs: flamegraph invertido](#15-icicle-graphs-flamegraph-invertido)
16. [Diferenciales: comparar dos runs](#16-diferenciales-comparar-dos-runs)
17. [Off-CPU flamegraphs: dónde duerme el programa](#17-off-cpu-flamegraphs-dónde-duerme-el-programa)
18. [Memory allocation flamegraphs](#18-memory-allocation-flamegraphs)
19. [Flamegraphs en lenguajes con runtime (JIT, GC)](#19-flamegraphs-en-lenguajes-con-runtime-jit-gc)
20. [speedscope: alternativa interactiva moderna](#20-speedscope-alternativa-interactiva-moderna)
21. [samply: profiler Rust con UI web](#21-samply-profiler-rust-con-ui-web)
22. [Inline functions y stack fusion](#22-inline-functions-y-stack-fusion)
23. [Errores comunes al interpretar flamegraphs](#23-errores-comunes-al-interpretar-flamegraphs)
24. [Filtrar, colapsar, limpiar la gráfica](#24-filtrar-colapsar-limpiar-la-gráfica)
25. [Integración con perf report: cuándo usar cada uno](#25-integración-con-perf-report-cuándo-usar-cada-uno)
26. [Overhead del muestreo](#26-overhead-del-muestreo)
27. [Programa de práctica: hotspot_hunter.c](#27-programa-de-práctica-hotspot_hunterc)
28. [Ejercicios](#28-ejercicios)
29. [Checklist de validación](#29-checklist-de-validación)
30. [Navegación](#30-navegación)

---

## 1. Qué es un flamegraph

Un **flamegraph** es una visualización inventada por **Brendan Gregg** en 2011 que convierte
miles (o millones) de stack traces muestreados en una imagen que se lee en segundos.

Idea central: cada stack trace es una línea de código que estaba ejecutándose en un instante
exacto. Si tomas **10 000 muestras por segundo** durante 10 segundos, tienes 100 000 stacks.
Leer 100 000 stacks en texto es imposible. Apilar 100 000 stacks y dibujar un rectángulo por
función, con anchura proporcional al número de muestras que la contienen, es **obvio**.

```
ANTES (perf report texto):                    DESPUÉS (flamegraph):

  40.12%  main                                  ┌───────────────────────────┐
  39.87%  matrix_multiply                       │          main             │
  25.01%  linear_search                         ├──────────────┬────────────┤
  20.50%  strcmp                                │ matrix_multi │ linear_    │
  15.30%  hash_compute                          │              │ search     │
  ...                                           ├──────────────┼────────────┤
  (200 líneas más)                              │    kernel    │ strcmp     │
                                                └──────────────┴────────────┘
```

En el flamegraph, de un vistazo ves:
- **Qué funciones consumen más CPU** (anchura).
- **Qué rutas de llamada las invocan** (altura).
- **Qué porcentaje del total ocupan** (fracción horizontal).

No hay que leer columnas. La vista humana detecta rectángulos anchos mucho más rápido que
números en una tabla.

---

## 2. El problema que resuelve

`perf report` (tópico anterior) muestra dos vistas:

1. **Flat profile**: cuánto tiempo se pasó en cada función (ordenado por `self`).
2. **Call graph**: árbol expandible con rutas de llamada.

Ambos son útiles, pero tienen limitaciones:

| Problema de perf report                          | Cómo lo resuelve el flamegraph                  |
|---------------------------------------------------|-------------------------------------------------|
| Tienes que expandir cada rama manualmente         | Todas las rutas visibles a la vez               |
| Difícil comparar dos funciones profundas          | Comparación visual directa por anchura          |
| Contexto de llamada se pierde en flat profile     | Stack completo explícito en cada columna        |
| El orden es alfabético o por muestras             | Orden estable (no reorganiza al expandir)       |
| Números absolutos sin intuición de proporción     | Porcentaje implícito en la anchura              |

Un flamegraph **no reemplaza** a `perf report`: reemplaza la fase de **navegación exploratoria**.
Cuando quieres saber *dónde está el problema*, abres el SVG. Cuando ya sabes dónde, usas
`perf annotate` o `perf report` para los detalles.

---

## 3. Anatomía de un flamegraph

```
       ┌─────────────────────────────────────────────────────┐
       │                      main (100%)                    │  ← Raíz
       ├───────────────────────────────┬─────────────────────┤
       │     process_batch (65%)       │  finalize (35%)     │  ← Hijas directas
       ├──────────────┬────────────────┼──────────┬──────────┤
       │ parse (30%)  │ transform (35%)│ flush(20)│ log(15%) │
       ├──────────┬───┼────────────────┼──────────┼──────────┤
       │ lex (18%)│...│  matrix_mul (35%)       │ write(12%)│
       └──────────┴───┴────────────────┴──────────┴──────────┘
       ─── X: orden alfabético (NO tiempo) ───────────────>
       │
       │ Y: profundidad del stack
       ▼
```

Elementos clave:

- **Cada rectángulo = una función** en un stack trace.
- **Anchura = frecuencia**: fracción de muestras en las que aparece esa función en esa ruta exacta.
- **Y (altura) = profundidad del stack**: la función de arriba llama a la de abajo.
  - En el flamegraph *clásico*, la raíz está abajo y el stack crece hacia arriba (como llamas).
  - En el *icicle*, la raíz está arriba y crece hacia abajo (ver §15).
- **X = orden alfabético por nombre de función**, **NO tiempo cronológico**. Dos rectángulos
  adyacentes no tienen nada que ver temporalmente. Esto es uno de los errores más comunes de
  interpretación (§23).
- **Profundidad visual = profundidad real de llamadas**.

---

## 4. Cómo se construye: el pipeline completo

El flamegraph clásico de Brendan Gregg es un pipeline de tres etapas:

```
   ┌────────────────┐    ┌──────────────────┐    ┌──────────────────┐
   │ perf record    │───▶│ stackcollapse-   │───▶│ flamegraph.pl    │
   │  (samples)     │    │ perf.pl          │    │                  │
   └────────────────┘    └──────────────────┘    └──────────────────┘
        perf.data             stacks.folded           flamegraph.svg
```

### 4.1. `perf record` — captura samples

Genera `perf.data`, un archivo binario con miles de stack traces muestreados a intervalos
regulares del PMU.

### 4.2. `perf script` + `stackcollapse-perf.pl` — colapso de stacks

`perf script` convierte el binario en texto legible:

```
main 12345 [000] 1234.567890: cycles:
    ffffffff81234567 do_syscall_64+0x23 ([kernel.kallsyms])
    7f1234567890 __libc_start_main+0xf0 (/lib64/libc.so.6)
    400100 main+0x50 (/home/user/program)
    4001a0 matrix_multiply+0x30 (/home/user/program)
    4002b0 inner_loop+0x10 (/home/user/program)
```

`stackcollapse-perf.pl` lee este texto y convierte cada stack en **una sola línea** con
formato `funcA;funcB;funcC count`:

```
main;matrix_multiply;inner_loop 1523
main;matrix_multiply;inner_loop 1523
main;linear_search;strcmp 987
main;linear_search;strcmp 987
main;hash_compute 541
```

Las stacks idénticas se suman. Este formato se llama **"folded stacks"** y es el
intermediario universal que otras herramientas (speedscope, inferno, pprof) también aceptan.

### 4.3. `flamegraph.pl` — render a SVG

Lee folded stacks y genera un SVG interactivo con:
- Tooltip al hover mostrando función y porcentaje.
- Click para zoom in a una rama.
- Búsqueda con `Ctrl+F`.
- Reset con click en "Reset Zoom".

Comando canónico end-to-end:

```bash
perf record -F 99 -g -- ./my_program
perf script | ./FlameGraph/stackcollapse-perf.pl > out.folded
./FlameGraph/flamegraph.pl out.folded > flamegraph.svg
```

O encadenado:

```bash
perf script | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > flamegraph.svg
```

Abre el SVG con un navegador (Firefox/Chromium): el JavaScript embebido habilita la
interactividad.

---

## 5. Instalación de FlameGraph de Brendan Gregg

`FlameGraph` es un repositorio de scripts Perl. No hay paquete en distros (la mayoría).
Se clona directamente:

```bash
git clone https://github.com/brendangregg/FlameGraph.git ~/tools/FlameGraph
export PATH="$HOME/tools/FlameGraph:$PATH"
```

Añade el `export` a tu `~/.zshrc` o `~/.bashrc` para que `flamegraph.pl` y
`stackcollapse-perf.pl` estén en el `PATH`.

Verificación:

```bash
which flamegraph.pl
# → /home/user/tools/FlameGraph/flamegraph.pl

which stackcollapse-perf.pl
# → /home/user/tools/FlameGraph/stackcollapse-perf.pl
```

Alternativas modernas:
- **inferno** (`cargo install inferno`): reimplementación en Rust, 10-50× más rápida.
- **samply** (`cargo install samply`): profiler con UI web embebida (§21).
- **speedscope** (`npm install -g speedscope`): visualizador alternativo (§20).

---

## 6. Flamegraph clásico con perf + flamegraph.pl

Flujo básico para un programa C compilado con frame pointers:

```bash
# 1. Compilar con frame pointers y símbolos
gcc -O2 -fno-omit-frame-pointer -g -o program program.c

# 2. Muestrear
perf record -F 99 -g --call-graph fp -- ./program

# 3. Generar flamegraph
perf script | stackcollapse-perf.pl | flamegraph.pl > program.svg

# 4. Ver
xdg-open program.svg
```

Parámetros clave de `flamegraph.pl`:

```bash
flamegraph.pl \
  --title "Mi programa - prod run" \
  --subtitle "2026-04-05, release build" \
  --width 1800 \
  --height 24 \
  --fonttype "mono" \
  --fontsize 11 \
  --colors hot \
  --bgcolors grey \
  --hash \
  out.folded > program.svg
```

- `--width 1800`: ancho en píxeles (default 1200; más es mejor para stacks profundos).
- `--colors hot`: paleta caliente (rojos/naranjas, la clásica).
- `--hash`: colores deterministas por nombre de función (mismo nombre = mismo color entre runs).
- `--bgcolors grey`: fondo gris en lugar de degradado amarillo-naranja.

---

## 7. Interpretar los ejes: X alfabético, Y profundidad

### Eje Y — profundidad del stack

De abajo hacia arriba, cada nivel es una función que fue llamada por la de abajo:

```
┌───────────────────────────┐
│  inner_loop (nivel 3)     │  ← función en ejecución (hot)
├───────────────────────────┤
│  matrix_multiply (lvl 2)  │
├───────────────────────────┤
│  main (nivel 1)           │
├───────────────────────────┤
│  _start (nivel 0, raíz)   │
└───────────────────────────┘
```

Cuanto más alta la torre, más profunda la recursión o más anidadas las llamadas. Torres muy
altas (>30 niveles) suelen indicar recursión o código muy jerárquico (parsers, compiladores).

### Eje X — orden alfabético, NO tiempo

Este es el punto más malinterpretado. Los rectángulos se ordenan **alfabéticamente por nombre
de función** dentro de cada nivel. Esto significa:

- Dos funciones adyacentes NO se ejecutaron una tras otra en el tiempo.
- NO hay "flujo temporal" de izquierda a derecha.
- Solo la **anchura** tiene significado cuantitativo.

Por qué alfabético: permite que el SVG sea **determinista** entre runs. Si fuera cronológico,
dos runs del mismo programa darían gráficas diferentes y no podrías compararlas visualmente.

Para visualización temporal existen otras herramientas (tracing, Chrome DevTools timeline,
samply). El flamegraph sacrifica el tiempo por la comparabilidad.

---

## 8. Colores: significado real y falsas pistas

Por defecto, `flamegraph.pl` usa la paleta `hot` (rojos/naranjas/amarillos), lo que da la
apariencia de "llamas" y justifica el nombre. **Los colores son aleatorios** (o semi-aleatorios
con `--hash`).

Paletas disponibles:

| Paleta   | Uso                                                    |
|----------|--------------------------------------------------------|
| `hot`    | Default, visual "llama", aleatorio                    |
| `mem`    | Azules/verdes para flamegraphs de memoria            |
| `io`     | Azules para I/O                                       |
| `java`   | Diferencia JVM/kernel/user/inlined                   |
| `js`     | JavaScript (V8 engine, JIT vs interpreted)           |
| `perl`   | Perl                                                  |
| `rust`   | Colores específicos para Rust                         |
| `python` | Python                                                |
| `wakeup` | Para off-CPU analysis                                 |

Con `--hash`, el color es una función determinista del **nombre** de la función, no aleatorio.
Esto permite comparar dos flamegraphs del mismo programa y ver si una función "cambió de
color" (indicando que su nombre cambió o se renombró).

**Falsas pistas**: los principiantes suelen pensar que "rojo = lento" o "amarillo = rápido".
**Esto es falso**. Los colores solo diferencian visualmente funciones adyacentes. La
información cuantitativa está **exclusivamente** en la anchura.

Paletas con semántica real (raras):
- `--colors java` diferencia user space (verde), kernel (naranja), JIT (amarillo), inlined
  (beige). Aquí el color **sí** tiene significado.
- Flamegraphs diferenciales (§16): rojo = más lento que baseline, azul = más rápido.

---

## 9. Patrones visuales: mesetas, torres, cerillas

Leer un flamegraph es reconocer patrones. Los tres más importantes:

### 9.1. Meseta ancha (plateau) — HOTSPOT CLÁSICO

```
┌─────────────────────────────────────────────────────┐
│                     main                            │
├─────────────────────────────────────────────────────┤
│          process_data (85% ancho)                   │  ← MESETA
└─────────────────────────────────────────────────────┘
                     ↑
           Esto es donde optimizar
```

Una función que ocupa la mayoría del ancho: **ahí está el tiempo**. Click para hacer zoom y
explorar sus hijas. Si la meseta no tiene hijas (o tiene hijas mucho más estrechas que ella),
el trabajo se hace en esa función directamente (tiempo `self`).

### 9.2. Torre alta y estrecha — recursión o pila profunda

```
     ┌──┐
     │f5│
     ├──┤
     │f4│
     ├──┤
     │f3│
     ├──┤
     │f2│
     ├──┤
     │f1│
     ├──┤
     │main│
     └──┘
```

Una columna angosta que llega muy alto: profundidad considerable pero poco tiempo. **No es
hotspot**. Es común en parsers recursivos, algoritmos de backtracking, o DFS. Si la torre es
ancha **y** alta, entonces sí importa: es un subsistema grande con rutas profundas.

### 9.3. Cerillas ("matchsticks") — polvo disperso

```
 │││││││││││││││││││││ ← muchas columnas delgadas sin meseta
 │││││││││││││││││││││
 │││││││││││││││││││││
```

Muchas funciones ocupando poco tiempo cada una, sin una dominante. Significa:
- El trabajo está **bien distribuido**, no hay cuello de botella fácil.
- Optimizar una sola función dará mejora marginal (Amdahl).
- Posiblemente el overhead está en algo estructural: allocator, cache, syscalls.

Acciones: buscar commonality. Si todas las cerillas son `malloc`, `free`, `memcpy`,
el problema es el **patrón** global, no una función.

### 9.4. Pirámide invertida — función común llamada desde todo

```
┌─┬─┬─┬─┬─┬─┬─┬─┬─┐
│a│b│c│d│e│f│g│h│i│  ← muchos padres distintos
├─┴─┴─┴─┴─┴─┴─┴─┴─┤
│   shared_func   │  ← todos llaman a la misma
└─────────────────┘
```

Una función ancha en la base cuyos padres son muchos rectángulos angostos. Esto significa que
`shared_func` se llama desde muchos sitios distintos. Si es hot, optimizarla beneficia a
todos. Si no, puede ser una utility común (ej. `memcpy`, `strlen`).

### 9.5. Torre dentada — recursión con trabajo en cada nivel

```
┌────┐
│ f1 │
├────┴───┐
│  f2    │
├────────┴──┐
│   f3      │
├───────────┴──┐
│    f4        │
└──────────────┘
```

Cada nivel es más ancho porque cada llamada hace más trabajo (o hay varias llamadas en ese
nivel). Patrón típico de `quicksort` o `mergesort` con pequeña pila pero trabajo uniforme.

---

## 10. Resolución de símbolos y debug info

Sin símbolos, los stacks muestran direcciones hexadecimales. Con símbolos, nombres legibles.

**Requisitos**:

| Tipo de código          | Requiere                                             |
|--------------------------|------------------------------------------------------|
| Tu programa C/C++/Rust  | `-g` al compilar, binario no stripped               |
| Librerías del sistema    | Paquetes `-dbg` o `-debuginfo`                      |
| Kernel                   | `/proc/kallsyms` accesible (kptr_restrict = 0)      |
| Librerías dinámicas      | `.debug` accesible (normalmente vía `debuginfod`)   |

Para Fedora/RHEL:
```bash
sudo dnf debuginfo-install glibc
```

Para Debian/Ubuntu:
```bash
sudo apt install libc6-dbg
```

Para cualquier distro con **debuginfod** (>Fedora 35, Debian 12, Arch):
```bash
export DEBUGINFOD_URLS="https://debuginfod.fedoraproject.org/"
# o en Debian:
export DEBUGINFOD_URLS="https://debuginfod.debian.net/"
```

Con debuginfod, `perf script` descarga los símbolos de librerías del sistema automáticamente
en la primera invocación.

**Verificación**: si en el flamegraph ves cajas tipo `0x7f1234567890` o `[unknown]`, te
faltan símbolos. Para C propio:
```bash
file ./program
# Debe decir: "with debug_info, not stripped"
```

Si dice `stripped`, recompila con `-g` y no uses `strip`.

---

## 11. Frame pointers vs DWARF vs LBR

Para generar stacks, `perf` necesita saber cómo navegar la pila. Tres métodos
(ver el tópico anterior T01 para detalles):

### Comparativa rápida

| Método | Coste CPU | Coste tamaño | Profundidad | Precisión | Portable |
|--------|-----------|--------------|-------------|-----------|----------|
| **fp** | Muy bajo  | Muy bajo     | Ilimitada   | Alta      | Sí       |
| **dwarf** | Alto   | Alto (8K/sample) | Alta, truncable | Media | Sí   |
| **lbr** | Bajo     | Bajo         | 16-32 niveles | Muy alta | Intel only |

Para flamegraphs en la práctica:

- **Usa `fp`** en C propio compilado con `-fno-omit-frame-pointer`. Es lo más simple y barato.
- **Usa `dwarf`** cuando no puedes recompilar (librerías externas, kernel de distro default
  que compila sin frame pointers).
- **Usa `lbr`** para call graphs cortos pero precisos en CPUs Intel.

Compilación C con frame pointers:
```bash
gcc -O2 -fno-omit-frame-pointer -g -o program program.c
```

Compilación Rust con frame pointers:
```bash
RUSTFLAGS="-C force-frame-pointers=yes -C debuginfo=2" cargo build --release
```

Para tu código propio, **siempre** compila con frame pointers cuando vayas a profilear:
el coste runtime es 1-3%, el beneficio es inmenso.

---

## 12. Profilar C: desde compilación hasta SVG

Flujo completo con un programa ejemplo:

```bash
# 1. Compilar con flags adecuados
gcc -O2 -g -fno-omit-frame-pointer -o target target.c -lm

# 2. Fijar frecuencia CPU (opcional, reduce ruido)
sudo cpupower frequency-set -g performance

# 3. Ejecutar con perf record
perf record \
    -F 997 \
    -g --call-graph fp \
    --output=perf.data \
    -- ./target

# 4. (Opcional) verificar que hay símbolos
perf report --stdio | head -30

# 5. Generar flamegraph
perf script -i perf.data \
  | stackcollapse-perf.pl --all \
  | flamegraph.pl \
      --title "target benchmark" \
      --width 1800 \
      --colors hot \
      > flamegraph.svg

# 6. Abrir en navegador
xdg-open flamegraph.svg
```

Flags relevantes:

- `-F 997`: frecuencia 997 Hz (número primo para evitar aliasing con timers).
- `-g --call-graph fp`: captura call graph usando frame pointers.
- `--all` en `stackcollapse-perf.pl`: incluye todos los eventos (default solo `cycles`).

---

## 13. Profilar Rust con cargo-flamegraph

Rust tiene una herramienta oficial que envuelve el pipeline completo:

```bash
cargo install flamegraph
```

Uso básico:

```bash
cd my_project

# Necesita frame pointers para call graphs precisos
CARGO_PROFILE_RELEASE_DEBUG=true cargo flamegraph --bin my_bin

# Para benchmarks criterion:
CARGO_PROFILE_BENCH_DEBUG=true cargo flamegraph --bench my_bench -- --bench

# Con argumentos al programa:
cargo flamegraph --bin my_bin -- arg1 arg2
```

Genera `flamegraph.svg` en el directorio actual.

`cargo flamegraph` internamente:
1. Compila en `release` con `debuginfo=1` añadido.
2. Ejecuta `perf record -F 997 -g -- target/release/my_bin`.
3. Ejecuta `perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg`.

Para forzar frame pointers reales (recomendado):

```bash
# En Cargo.toml
[profile.release]
debug = true           # info de debug completa
strip = false          # no strip
lto = false            # LTO puede eliminar símbolos

# En .cargo/config.toml del proyecto
[target.'cfg(target_os = "linux")']
rustflags = ["-C", "force-frame-pointers=yes"]
```

Luego:
```bash
cargo flamegraph --bin my_bin
```

Problema común: con `lto = true` o `strip = true`, muchas funciones se inlinean y el
flamegraph pierde granularidad. Para profilear, usa un perfil específico:

```toml
# Cargo.toml
[profile.profiling]
inherits = "release"
debug = true
strip = false
lto = false
```

```bash
cargo flamegraph --profile profiling --bin my_bin
```

---

## 14. Profilar Rust en release sin herramientas externas

Sin instalar `cargo-flamegraph`, solo con `perf` + `FlameGraph`:

```bash
# 1. Compilar con debuginfo
RUSTFLAGS="-C force-frame-pointers=yes" \
  cargo build --release

# 2. Profilear
perf record -F 997 -g -- ./target/release/my_bin

# 3. Flamegraph
perf script | stackcollapse-perf.pl | flamegraph.pl > fg.svg
```

Para **benchmarks** criterion:

```bash
# 1. Ejecutar una vez para que criterion elija los parámetros y cree el binario
cargo bench --bench my_bench -- --profile-time 10

# 2. Localizar el binario
BENCH_BIN=$(ls -t target/release/deps/my_bench-* | head -1)

# 3. Profilear
perf record -F 997 -g -- "$BENCH_BIN" --bench --profile-time 10

# 4. Flamegraph
perf script | stackcollapse-perf.pl | flamegraph.pl > bench.svg
```

`--profile-time N` le dice a criterion que corra cada función por N segundos sin estadísticas
(útil para profiling, no para medir).

---

## 15. Icicle graphs: flamegraph invertido

Un **icicle graph** (también llamado "inverted flamegraph") apila los stacks **de arriba
abajo** en lugar de abajo arriba:

```
FLAMEGRAPH CLÁSICO:               ICICLE GRAPH:

  ┌───────────┐                   ┌───────────────────────┐
  │ inner_loop│                   │        main           │
  ├───────────┤                   ├─────────────┬─────────┤
  │mat_multi  │                   │ mat_multi   │ lin_srch│
  ├───────────┤                   ├─────────────┼─────────┤
  │  main     │                   │ inner_loop  │  strcmp │
  └───────────┘                   └─────────────┴─────────┘
```

Ventaja: la función **hot** (hoja del stack) queda **arriba**, donde la vista humana suele
mirar primero. Es más intuitivo para buscar hotspots cuando hay muchas rutas diferentes.

Generación:

```bash
flamegraph.pl --inverted < out.folded > icicle.svg
```

O con `--reverse` (antiguo):

```bash
flamegraph.pl --reverse < out.folded > reversed.svg
```

`--reverse` invierte el **orden** de las funciones en el stack (la hoja queda en la base),
mientras que `--inverted` mantiene el orden pero voltea la gráfica. La diferencia es sutil
pero relevante: `--reverse` agrupa funciones hoja en la base, útil para ver "cuánto tiempo en
total se pasó en `malloc`" sumando todas las rutas que llevan a él.

---

## 16. Diferenciales: comparar dos runs

Los **differential flamegraphs** comparan dos folded stacks y muestran diferencias visuales
en color:

```bash
# Generar dos folded stacks
perf record -F 99 -g -- ./program_v1
perf script | stackcollapse-perf.pl > before.folded

perf record -F 99 -g -- ./program_v2
perf script | stackcollapse-perf.pl > after.folded

# Diferencial
difffolded.pl before.folded after.folded > diff.folded
flamegraph.pl diff.folded > diff.svg
```

Interpretación de colores:
- **Rojo**: función más lenta que en baseline (regresión).
- **Azul**: función más rápida (mejora).
- **Blanco/gris**: sin cambio significativo.

Muy útil en CI para detectar regresiones tras un cambio. También:
- Comparar distintas configuraciones de allocator (glibc vs jemalloc vs mimalloc).
- Comparar C vs Rust equivalentes.
- Comparar distintas versiones de una librería.

Variante: `difffolded.pl -n` normaliza por número de muestras (compensa si un run tiene más
muestras que el otro).

---

## 17. Off-CPU flamegraphs: dónde duerme el programa

El flamegraph tradicional muestra **on-CPU time**: tiempo de ejecución activa. Pero muchos
programas (I/O-bound, network, DB) pasan la mayor parte del tiempo **bloqueados esperando**.
Esto es **off-CPU time**.

Un **off-CPU flamegraph** muestra stacks de cuando el thread está dormido/bloqueado, no
ejecutándose. Para capturarlo hace falta tracear eventos del scheduler:

```bash
# Tracear sched_switch events (off-CPU entry)
perf record -e sched:sched_switch \
    --call-graph fp \
    -g -- ./program

# O para tiempo acumulado, BPF tools:
sudo offcputime-bpfcc -df -p $(pgrep program) 30 > off.folded
flamegraph.pl --colors=io --title="Off-CPU" < off.folded > offcpu.svg
```

`offcputime-bpfcc` (parte de `bcc-tools`) es más limpio que `perf` para esto. Si tu distro
tiene `bpftrace`:

```bash
sudo bpftrace -e '
kprobe:finish_task_switch {
    @[ustack] = count();
}' -p $(pgrep program)
```

Interpretación: anchura = **tiempo dormido**, no tiempo de CPU. Si ves un rectángulo ancho
en `futex_wait` bajo `mutex_lock`, tu programa está lock-bound. Si ves `io_schedule` bajo
`read_file`, estás I/O-bound.

**Regla**: si `top` muestra 5% CPU pero el programa tarda mucho, necesitas un off-CPU
flamegraph, no on-CPU.

---

## 18. Memory allocation flamegraphs

Muestran **dónde se asignan bytes**, no dónde se gasta CPU. Útiles para:
- Detectar funciones con allocation churn (muchos malloc/free pequeños).
- Encontrar leaks (asignaciones sin free correspondiente).
- Localizar picos de memoria.

Herramientas:

### 18.1. `heaptrack` (mejor opción para C/C++)

```bash
sudo dnf install heaptrack heaptrack-gui

heaptrack ./program
# → heaptrack.program.12345.zst

heaptrack --analyze heaptrack.program.12345.zst
# Abre GUI con flamegraph de allocations
```

En la GUI de heaptrack:
- "Consumed" flamegraph: bytes vivos en un momento dado.
- "Allocations" flamegraph: número de allocs.
- "Temporary allocations": allocs que se liberaron inmediatamente (indicador de churn).
- "Peak" flamegraph: en el momento de máximo uso.

### 18.2. `jemalloc` + `jeprof` (cualquier programa)

Enlazando con jemalloc y activando profiling:
```bash
MALLOC_CONF="prof:true,prof_prefix:jeprof.out" \
LD_PRELOAD=/usr/lib64/libjemalloc.so \
./program

jeprof --svg ./program jeprof.out.12345.0.f.heap > mem.svg
```

### 18.3. `perf` con `tracepoint kmalloc` (kernel)

```bash
perf record -e kmem:kmalloc -g --call-graph fp -- ./program
perf script | stackcollapse-perf.pl | flamegraph.pl --colors mem > kmem.svg
```

Solo captura asignaciones **kernel**, útil para drivers o código kernel-bound.

---

## 19. Flamegraphs en lenguajes con runtime (JIT, GC)

Para C/Rust, los stacks son casi directamente interpretables porque todo es código nativo
estático. Para lenguajes con runtime, hay complicaciones:

### Java
- `perf` no entiende JIT por default: ve direcciones raras sin símbolos.
- Solución: `perf-map-agent` genera `/tmp/perf-<PID>.map` que perf lee.
- O usar `async-profiler`, que es un profiler JVM con flamegraph nativo.

### Python
- CPython es interpretado: perf ve `_PyEval_EvalFrameDefault` ocupando todo el tiempo.
- Solución: `py-spy` o `pyflame` que entienden el interpreter y reconstruyen stacks Python.
- `py-spy record -o profile.svg -- python script.py` genera flamegraph directamente.

### Node.js
- V8 JIT genera código dinámico. perf con `--perf-basic-prof` genera el map file.
- O `0x` (herramienta Node con flamegraph integrado).

### Go
- `perf` funciona pero Go tiene su propio profiler más integrado: `pprof`.
- `go tool pprof -http=:8080 cpu.prof` genera flamegraph en navegador.

En este curso nos centramos en **C y Rust**, donde `perf` + flamegraph.pl es el flujo
canónico y directo.

---

## 20. speedscope: alternativa interactiva moderna

`speedscope` es un visualizador web que acepta folded stacks y ofrece **tres vistas**:

```bash
perf script | stackcollapse-perf.pl > out.folded
speedscope out.folded
# Abre http://localhost:8080 con la UI
```

Vistas disponibles:

1. **Time Order**: eje X = tiempo cronológico real (requiere capturar timestamps). Muestra
   cómo evolucionó la ejecución en el tiempo.
2. **Left Heavy**: como flamegraph clásico, pero ordenado por peso descendente (las funciones
   más anchas a la izquierda).
3. **Sandwich**: vista por función, muestra quién la llama ("callers") y a quién llama
   ("callees"), como `perf report`.

Ventajas sobre `flamegraph.pl`:
- Time order real (el clásico es alfabético).
- Zoom fluido, búsqueda instantánea.
- Profile files intercambiables (JSON, folded, pprof, Instruments trace).

Desventaja: depende de Node/navegador moderno.

Instalación:
```bash
npm install -g speedscope
# o
cargo install --git https://github.com/jlfwong/speedscope-cli
```

---

## 21. samply: profiler Rust con UI web

`samply` es un profiler moderno escrito en Rust que reemplaza `perf record` **y**
`flamegraph.pl` en un solo comando con UI web rica:

```bash
cargo install samply

samply record ./my_program
# Ejecuta, captura, y abre localhost:3000 con UI interactiva
```

Ventajas:
- No requiere clonar FlameGraph.
- Usa Firefox Profiler UI (familiar, potente).
- Soporta time-ordered view nativo.
- Stacks mergeados inteligentemente.
- Funciona en Linux y macOS.

Limitación: no tiene todos los flags exóticos de `perf`, pero para flamegraphs de profiling
de aplicación es más directo.

Uso típico:
```bash
# Compilar con debug info
RUSTFLAGS="-C force-frame-pointers=yes" cargo build --release

# Profilear
samply record ./target/release/my_bin arg1 arg2
```

Se abre el navegador con la UI. Puedes exportar a formatos intercambiables o compartir con
el equipo.

---

## 22. Inline functions y stack fusion

Cuando el compilador inlinea una función, el stack trace "pierde" ese frame: el trabajo de
la función inlineada se atribuye a su padre.

Ejemplo:
```c
static inline int helper(int x) { return x * 2 + 1; }
int caller(int x) { return helper(x) + helper(x+1); }
```

Con `-O2`, `helper` probablemente se inline en `caller`. En el flamegraph verás solo `caller`
ocupando el tiempo que *realmente* es de `helper`. Esto es bueno (menos ruido) y malo
(pierdes granularidad).

Para ver frames inlineados, usa `--inline` en `perf script`:

```bash
perf script --inline | stackcollapse-perf.pl | flamegraph.pl > fg.svg
```

`--inline` usa la información DWARF de `.debug_info` para expandir los frames inlineados
como si fueran llamadas reales. Requiere `-g` al compilar.

Sin `--inline`, funciones como `std::Vec::push` o `memcpy` inline se atribuyen a sus padres
y pueden hacer parecer que el padre es el hotspot cuando realmente es una función llamada
muchas veces desde él.

Para Rust, `--inline` es **esencial** porque gran parte de la librería estándar es código
genérico que se inlinea agresivamente. Sin él, verás funciones de usuario ocupando todo el
flamegraph y no sabrás si son realmente lentas o si es `Iterator::map` inlineado.

---

## 23. Errores comunes al interpretar flamegraphs

### Error 1 — leer el eje X como tiempo

```
"Primero se ejecuta main, luego process, luego cleanup..."
```

**Incorrecto**. X es alfabético. Para orden temporal, usa speedscope con vista "Time Order"
o herramientas de tracing.

### Error 2 — ignorar las torres estrechas altas

```
"Esta función está anidada 30 niveles, debe ser importante"
```

**Incorrecto**. Altura no es importancia. Una torre angosta es irrelevante por ancho.

### Error 3 — asumir que color = temperatura

```
"Las rojas son más lentas que las amarillas"
```

**Incorrecto** (en flamegraph clásico). El color es decoración aleatoria salvo en paletas
con semántica (java, diff).

### Error 4 — optimizar funciones altas sin ver ancho

```
"Voy a optimizar f() porque aparece en la cima de muchas torres"
```

**Incorrecto**. Solo importa la anchura total. Una función que aparece mucho pero estrecha
cada vez no es hotspot.

### Error 5 — no usar `--inline`

Ves `my_loop` ocupando el 80% y piensas "optimicemos my_loop". Pero el 75% es realmente
`SIMD vectorized memcpy` inlineado. Con `--inline` ves la verdad.

### Error 6 — usar la versión sin frame pointers

Sin `-fno-omit-frame-pointer` (C) o `force-frame-pointers=yes` (Rust), los call graphs son
basura: stacks truncados, funciones que no aparecen, atribuciones incorrectas.

### Error 7 — muestrear con frecuencia demasiado baja

`perf record -F 1` da muestras cada segundo. Para un run de 10s, tienes 10 muestras. El
flamegraph es inútil estadísticamente. Usa `-F 99` mínimo, `-F 997` preferido.

### Error 8 — profilear builds debug

Un build `-O0` tiene un perfil de ejecución completamente distinto al `-O2`/`-O3` de
producción. Profilea siempre builds optimizados (con símbolos).

### Error 9 — confundir self vs total

El flamegraph muestra **total time** (self + callees). Una función ancha puede tener 0%
self y 100% callees (solo delega). Para self, mira si sus hijas cubren todo el ancho de la
madre. Si hay "hueco" arriba, eso es self-time.

### Error 10 — profilear una sola ejecución

El jitter entre runs puede ser grande. Verifica hallazgos con 2-3 runs. Si el hotspot cambia
entre runs, el sistema tiene ruido (scheduling, cache effects, thermal throttling).

---

## 24. Filtrar, colapsar, limpiar la gráfica

Las gráficas reales suelen tener ruido: código de inicialización, shutdown, librerías del
sistema que no interesan. Herramientas de limpieza:

### Filtrado por patrón

`grep` sobre el folded output (es solo texto):

```bash
# Quedarse solo con stacks que contienen "my_app"
grep "my_app" out.folded > filtered.folded

# Excluir stacks que contengan "startup"
grep -v "startup" out.folded > filtered.folded

# Solo stacks que terminan en hot_function
grep ";hot_function " out.folded > filtered.folded
```

### Colapsar por prefijo

Si te importa el código de aplicación y todo lo de kernel debe colapsarse:

```bash
sed 's/.*\[kernel.kallsyms\].*/[kernel]/' out.folded > clean.folded
```

### Mostrar solo N niveles

`flamegraph.pl --maxdepth 20` limita la altura (útil para torres enormes que no aportan).

### Ocultar funciones triviales

```bash
flamegraph.pl --minwidth 1 < out.folded > fg.svg
```

`--minwidth N` oculta rectángulos más estrechos que N píxeles. Reduce clutter visual.

### Agrupar por librería

Paletas especiales de `flamegraph.pl`:
```bash
flamegraph.pl --colors chain < out.folded > fg.svg
```

`chain` asigna colores según el último componente del nombre (útil para ver inmediatamente
qué módulos dominan).

---

## 25. Integración con perf report: cuándo usar cada uno

Complementarios, no excluyentes:

| Tarea                                | Herramienta               |
|---------------------------------------|---------------------------|
| Detectar hotspot rápido               | **Flamegraph**            |
| Comparar dos versiones                | **Diff flamegraph**       |
| Ver call graph de una función         | `perf report` → expandir  |
| Ver instrucciones ensamblador         | `perf annotate`           |
| Ver cache misses por línea            | `perf report` con `-e cache-misses` |
| Análisis cronológico                  | `speedscope` time-order   |
| Análisis de memoria                   | `heaptrack`               |
| Off-CPU                               | Off-CPU flamegraph        |

Flujo de trabajo típico:

```
1. perf record → perf.data
2. flamegraph.svg → ver la forma general
3. Click en rectángulo ancho → zoom
4. perf report → expandir esa rama para ver callees
5. perf annotate -s funcion_hot → ver qué instrucción duele
6. Fix → repetir desde 1
```

Flamegraph es la entrada, `annotate` es la salida. `perf report` es la navegación intermedia.

---

## 26. Overhead del muestreo

Recordatorio del T01: el overhead del muestreo depende de método y frecuencia:

| Método               | Overhead típico |
|----------------------|-----------------|
| `-F 99` sin call graph | <1%           |
| `-F 997` sin cg        | ~1-2%         |
| `-F 99 --call-graph fp`  | ~1-3%       |
| `-F 997 --call-graph fp` | ~3-5%       |
| `-F 99 --call-graph dwarf` | ~10-20%   |
| `-F 997 --call-graph dwarf`| ~20-40%   |
| `-F 997 --call-graph lbr`  | ~2-5%     |

**Consecuencia**: un flamegraph generado con `dwarf` a alta frecuencia puede distorsionar el
propio perfil (Heisenberg). Si ves que el programa parece lento durante el profiling, baja
la frecuencia o cambia a `fp`.

---

## 27. Programa de práctica: hotspot_hunter.c

Programa con múltiples hotspots visibles en el flamegraph:

```c
/* hotspot_hunter.c — programa con hotspots deliberados para flamegraph
 *
 * Compilar:
 *   gcc -O2 -g -fno-omit-frame-pointer -o hotspot_hunter hotspot_hunter.c -lm
 *
 * Profilear:
 *   perf record -F 997 -g --call-graph fp -- ./hotspot_hunter
 *   perf script | stackcollapse-perf.pl | flamegraph.pl > fg.svg
 *   xdg-open fg.svg
 *
 * Estructura de llamadas esperada (aproximada en tiempo):
 *
 *   main
 *    ├─ setup_data           (~2%)
 *    │   └─ random_fill
 *    ├─ compute_phase        (~50%)
 *    │   ├─ matrix_multiply  (~35%) ← HOTSPOT PRINCIPAL (meseta ancha)
 *    │   │   └─ inner_loop
 *    │   └─ vector_norms     (~15%)
 *    │       └─ sqrt_sum
 *    ├─ search_phase         (~30%)
 *    │   ├─ linear_scan      (~20%) ← HOTSPOT SECUNDARIO
 *    │   │   └─ compare_keys
 *    │   └─ lookup_table     (~10%)
 *    └─ io_phase             (~15%)
 *        ├─ format_output    (~10%)
 *        │   └─ itoa_custom
 *        └─ write_buffer     (~5%)
 *
 * En el flamegraph:
 *   - matrix_multiply: meseta ancha clara
 *   - inner_loop: torre encima de matrix_multiply
 *   - linear_scan: segunda meseta menor
 *   - format_output: tercera meseta pequeña
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define N           512        /* tamaño matriz NxN */
#define VEC_SIZE    (1 << 16)  /* 64K elementos */
#define SEARCH_KEYS 8192
#define ITERS       3

/* ---- utilidades ---- */

static void random_fill(double *arr, size_t n) {
    for (size_t i = 0; i < n; i++) {
        arr[i] = (double)rand() / RAND_MAX;
    }
}

static void setup_data(double **A, double **B, double **C, double **v) {
    *A = malloc(sizeof(double) * N * N);
    *B = malloc(sizeof(double) * N * N);
    *C = calloc(N * N, sizeof(double));
    *v = malloc(sizeof(double) * VEC_SIZE);
    random_fill(*A, N * N);
    random_fill(*B, N * N);
    random_fill(*v, VEC_SIZE);
}

/* ---- compute phase ---- */

static void inner_loop(const double *restrict a_row,
                       const double *restrict b_col,
                       double *restrict out,
                       int n) {
    double acc = 0.0;
    for (int k = 0; k < n; k++) {
        acc += a_row[k] * b_col[k];
    }
    *out = acc;
}

static void matrix_multiply(const double *A, const double *B, double *C, int n) {
    /* B transpuesta implícitamente accedida por columnas (intencionalmente mal). */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double row_scratch[N];
            for (int k = 0; k < n; k++) row_scratch[k] = B[k * n + j];
            inner_loop(&A[i * n], row_scratch, &C[i * n + j], n);
        }
    }
}

static double sqrt_sum(const double *v, size_t n) {
    double s = 0.0;
    for (size_t i = 0; i < n; i++) {
        s += sqrt(v[i] * v[i] + 1.0);  /* sqrt intencionalmente pesado */
    }
    return s;
}

static double vector_norms(const double *v, size_t n, int rounds) {
    double total = 0.0;
    for (int r = 0; r < rounds; r++) {
        total += sqrt_sum(v, n);
    }
    return total;
}

static double compute_phase(const double *A, const double *B, double *C,
                            const double *v) {
    matrix_multiply(A, B, C, N);
    return vector_norms(v, VEC_SIZE, 32);
}

/* ---- search phase ---- */

static int compare_keys(const char *a, const char *b) {
    /* strcmp manual para que aparezca en el flamegraph */
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int linear_scan(const char *haystack[], int n, const char *needle) {
    for (int i = 0; i < n; i++) {
        if (compare_keys(haystack[i], needle) == 0) return i;
    }
    return -1;
}

static unsigned lookup_table(unsigned key) {
    /* tabla hash pobre intencionalmente */
    unsigned h = key;
    for (int i = 0; i < 16; i++) {
        h = (h * 2654435761u) ^ (h >> 7);
    }
    return h & 0xFFFF;
}

static void search_phase(void) {
    const char *dict[] = {
        "alpha","beta","gamma","delta","epsilon","zeta","eta","theta","iota",
        "kappa","lambda","mu","nu","xi","omicron","pi","rho","sigma","tau",
        "upsilon","phi","chi","psi","omega"
    };
    int nwords = sizeof(dict) / sizeof(dict[0]);

    volatile long sink = 0;
    for (int i = 0; i < SEARCH_KEYS * 4; i++) {
        int idx = linear_scan(dict, nwords, dict[i % nwords]);
        sink += idx;
    }
    for (unsigned k = 0; k < SEARCH_KEYS * 32u; k++) {
        sink ^= lookup_table(k);
    }
    (void)sink;
}

/* ---- io phase ---- */

static int itoa_custom(int v, char *buf) {
    char tmp[16];
    int n = 0;
    if (v == 0) tmp[n++] = '0';
    int neg = v < 0;
    if (neg) v = -v;
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    int o = 0;
    if (neg) buf[o++] = '-';
    for (int i = n - 1; i >= 0; i--) buf[o++] = tmp[i];
    buf[o] = 0;
    return o;
}

static size_t format_output(char *out, size_t cap) {
    size_t off = 0;
    for (int i = 0; i < 20000 && off + 32 < cap; i++) {
        off += (size_t)itoa_custom(i * 31, out + off);
        out[off++] = ' ';
    }
    out[off] = 0;
    return off;
}

static void write_buffer(const char *buf, size_t n) {
    /* simulación: sumar para evitar DCE */
    volatile unsigned long sum = 0;
    for (size_t i = 0; i < n; i++) sum += (unsigned char)buf[i];
    (void)sum;
}

static void io_phase(void) {
    static char big[1 << 20];
    for (int i = 0; i < 50; i++) {
        size_t n = format_output(big, sizeof big);
        write_buffer(big, n);
    }
}

/* ---- main ---- */

int main(void) {
    srand(42);

    double *A, *B, *C, *v;
    setup_data(&A, &B, &C, &v);

    double total = 0.0;
    for (int i = 0; i < ITERS; i++) {
        total += compute_phase(A, B, C, v);
        search_phase();
        io_phase();
    }

    printf("total=%.6f\n", total);

    free(A); free(B); free(C); free(v);
    return 0;
}
```

### Flujo de práctica

```bash
# 1. Compilar
gcc -O2 -g -fno-omit-frame-pointer -o hotspot_hunter hotspot_hunter.c -lm

# 2. Profilear con perf
perf record -F 997 -g --call-graph fp -- ./hotspot_hunter

# 3. Generar flamegraph
perf script | stackcollapse-perf.pl | flamegraph.pl \
  --title "hotspot_hunter" \
  --subtitle "perf -F 997 --call-graph fp" \
  --width 1800 \
  > hotspot_hunter.svg

# 4. Abrir
xdg-open hotspot_hunter.svg
```

### Qué debes ver

Abre el SVG y busca:

1. **Meseta ancha en `matrix_multiply`**: debería ocupar ~35% del ancho total. Click para
   zoom. Verás `inner_loop` encima como torre.
2. **Meseta media en `vector_norms` → `sqrt_sum`**: ~15% del ancho. El `sqrt` libc aparece
   encima.
3. **Meseta en `search_phase`**: mezcla de `linear_scan` (con `compare_keys` encima) y
   `lookup_table`.
4. **Meseta pequeña en `io_phase`**: `format_output` → `itoa_custom`.
5. **Sin inlining significativo** si compilaste con frame pointers.

Contrasta con:
```bash
# Sin frame pointers — call graphs rotos
gcc -O2 -g -o hotspot_hunter_nofp hotspot_hunter.c -lm
perf record -F 997 -g --call-graph fp -- ./hotspot_hunter_nofp
perf script | stackcollapse-perf.pl | flamegraph.pl > broken.svg
```

Verás stacks truncados, funciones que no aparecen, call graphs incoherentes.

### Con DWARF en lugar de fp

```bash
perf record -F 997 -g --call-graph dwarf -- ./hotspot_hunter
perf script | stackcollapse-perf.pl | flamegraph.pl > dwarf.svg
```

Debería dar un flamegraph muy similar al de fp, pero `perf record` tarda más y `perf.data`
será mucho mayor (varios MB por segundo).

---

## 28. Ejercicios

### Ejercicio 1 — Flamegraph básico

1. Compila `hotspot_hunter.c` con `-O2 -g -fno-omit-frame-pointer`.
2. Genera el flamegraph y responde:
   - ¿Qué porcentaje aproximado ocupa `matrix_multiply`?
   - ¿Aparece `inner_loop` como su hija directa?
   - ¿Cuántas "mesetas" principales ves en el nivel 1 bajo `main`?
3. Haz click en `matrix_multiply` y describe qué ves al hacer zoom.

### Ejercicio 2 — Comparar compilers y flags

1. Genera 3 versiones con distintos flags:
   - `-O0 -g -fno-omit-frame-pointer`
   - `-O2 -g -fno-omit-frame-pointer`
   - `-O3 -g -fno-omit-frame-pointer -march=native`
2. Genera un flamegraph para cada.
3. Compara:
   - ¿Cambia la **forma** (relative heights/widths)?
   - ¿Cambia el **tiempo total** de ejecución?
   - ¿Hay funciones que desaparecen por inlining en `-O3`?

### Ejercicio 3 — Diferencial

1. Modifica `matrix_multiply` para transponer B una vez antes del triple loop (en lugar
   de copiarla por columna dentro). Mide antes/después.
2. Genera un flamegraph diferencial:
   ```bash
   ./hotspot_hunter_before > /dev/null
   perf record -F 997 -g -o perf.before -- ./hotspot_hunter_before
   perf script -i perf.before | stackcollapse-perf.pl > before.folded

   ./hotspot_hunter_after > /dev/null
   perf record -F 997 -g -o perf.after -- ./hotspot_hunter_after
   perf script -i perf.after | stackcollapse-perf.pl > after.folded

   difffolded.pl before.folded after.folded | flamegraph.pl > diff.svg
   ```
3. Identifica en rojo qué empeoró y en azul qué mejoró.

### Ejercicio 4 — Rust con cargo-flamegraph

1. Instala `cargo-flamegraph`.
2. Crea un proyecto Rust con un bucle CPU-heavy similar a `matrix_multiply`.
3. Genera flamegraph con `cargo flamegraph`.
4. Añade `RUSTFLAGS="-C force-frame-pointers=yes"` a tu config.toml.
5. Añade `--inline` al pipeline de perf script:
   ```bash
   perf script --inline | stackcollapse-perf.pl | flamegraph.pl > rust.svg
   ```
6. Compara sin y con `--inline`. ¿Qué funciones nuevas aparecen?

### Ejercicio 5 — Speedscope time order

1. Instala `speedscope` (`npm install -g speedscope`).
2. Genera `out.folded` como siempre, pero con samples de alto detalle:
   ```bash
   perf record -F 997 -g --call-graph fp -- ./hotspot_hunter
   perf script --fields comm,pid,tid,time,event,ip,sym,dso | \
     stackcollapse-perf.pl > out.folded
   speedscope out.folded
   ```
3. En la UI, usa la vista "Time Order" y describe cómo cambian los hotspots a lo largo
   del tiempo. ¿Ves el orden `compute` → `search` → `io` por cada iteración?

### Ejercicio 6 — Off-CPU (opcional, requiere privilegios)

1. Añade `usleep(100000)` en `io_phase` después de cada `write_buffer`.
2. Genera un on-CPU flamegraph normal: no deberías ver el sleep.
3. Genera un off-CPU flamegraph con `bpftrace` u `offcputime-bpfcc`.
4. Compara: el off-CPU debe estar dominado por el sleep.

---

## 29. Checklist de validación

Antes de actuar sobre un flamegraph, confirma:

- [ ] Compilado en **release** con `-O2`/`-O3`, no `-O0`.
- [ ] Compilado con `-g` (o equivalente Rust `debug=true`).
- [ ] Compilado con `-fno-omit-frame-pointer` (o `force-frame-pointers=yes` en Rust).
- [ ] `perf.data` generado con `--call-graph fp` (o `dwarf` si no hay fp).
- [ ] Frecuencia de sampling ≥ 99 Hz (ideal 997).
- [ ] Duración suficiente para acumular ≥ 1000 muestras (run de 10+ segundos).
- [ ] Funciones legibles (no hex ni `[unknown]`): símbolos y debuginfo presentes.
- [ ] `--inline` usado si investigas código Rust con genéricos.
- [ ] Ruido irrelevante filtrado (startup, shutdown, kernel si no importa).
- [ ] Comparación con al menos una segunda ejecución para verificar jitter.
- [ ] Si aparece una meseta sospechosa, confirma con `perf report` y `perf annotate`.
- [ ] Si optimizas una función, mide antes/después con benchmark (no solo con flamegraph).

---

## 30. Navegación

| ← Anterior | ↑ Sección | Siguiente → |
|------------|-----------|-------------|
| [T01 — perf en Linux](../T01_Perf_en_Linux/README.md) | [S02 — Profiling](../) | [T03 — Valgrind/Callgrind](../T03_Callgrind/README.md) |

**Capítulo 4 — Sección 2: Profiling de Aplicaciones — Tópico 2 de 4**

Has aprendido a transformar miles de stack traces en una imagen interpretable en segundos,
a leerla sin caer en las trampas clásicas (ejes, colores, inlining), y a usar las variantes
(diferencial, off-CPU, memory) para casos específicos. El flamegraph es tu **primera vista
estratégica** cuando hay que optimizar. En el siguiente tópico (T03 Valgrind/Callgrind)
complementamos esta vista estadística con una vista **exacta pero lenta**: simulación del
programa para contar instrucciones y accesos a memoria uno por uno.
