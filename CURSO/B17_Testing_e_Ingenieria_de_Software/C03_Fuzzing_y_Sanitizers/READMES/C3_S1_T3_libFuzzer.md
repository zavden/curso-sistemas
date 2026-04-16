# T03 - libFuzzer: fuzzing in-process con LLVM

## Índice

1. [Qué es libFuzzer](#1-qué-es-libfuzzer)
2. [Arquitectura in-process vs fork-based](#2-arquitectura-in-process-vs-fork-based)
3. [LLVMFuzzerTestOneInput: el contrato](#3-llvmfuzzertestoneinput-el-contrato)
4. [Compilar con -fsanitize=fuzzer](#4-compilar-con--fsanitizefuzzer)
5. [Separar instrumentación de sanitizers](#5-separar-instrumentación-de-sanitizers)
6. [Primera ejecución](#6-primera-ejecución)
7. [Interpretar el output de libFuzzer](#7-interpretar-el-output-de-libfuzzer)
8. [El corpus en libFuzzer](#8-el-corpus-en-libfuzzer)
9. [Merge de corpus: -merge=1](#9-merge-de-corpus--merge1)
10. [Minimizar un crash: -minimize_crash=1](#10-minimizar-un-crash--minimize_crash1)
11. [Diccionarios en libFuzzer](#11-diccionarios-en-libfuzzer)
12. [LLVMFuzzerInitialize: setup costoso](#12-llvmfuzzerinitialize-setup-costoso)
13. [LLVMFuzzerCustomMutator: mutaciones propias](#13-llvmfuzzercustommutator-mutaciones-propias)
14. [LLVMFuzzerCustomCrossOver: combinación de inputs](#14-llvmfuzzercustomcrossover-combinación-de-inputs)
15. [Flags de ejecución esenciales](#15-flags-de-ejecución-esenciales)
16. [Value profile: resolver comparaciones](#16-value-profile-resolver-comparaciones)
17. [Fork mode: -fork=N](#17-fork-mode--forkn)
18. [Detección de memory leaks](#18-detección-de-memory-leaks)
19. [Timeouts y OOM](#19-timeouts-y-oom)
20. [Integración con sanitizers](#20-integración-con-sanitizers)
21. [Estructura de un proyecto con libFuzzer](#21-estructura-de-un-proyecto-con-libfuzzer)
22. [libFuzzer en CI/CD](#22-libfuzzer-en-cicd)
23. [Regression testing con el corpus](#23-regression-testing-con-el-corpus)
24. [Comparación detallada con AFL++](#24-comparación-detallada-con-afl)
25. [Errores comunes](#25-errores-comunes)
26. [Ejemplo completo: fuzzear un parser de expresiones](#26-ejemplo-completo-fuzzear-un-parser-de-expresiones)
27. [Programa de práctica: base64_codec](#27-programa-de-práctica-base64_codec)
28. [Ejercicios](#28-ejercicios)

---

## 1. Qué es libFuzzer

libFuzzer es un motor de fuzzing coverage-guided integrado en el proyecto LLVM.
A diferencia de AFL++ (que ejecuta el target como proceso externo), libFuzzer
se **linka directamente** con el código target — ambos viven en el mismo proceso.

```
┌──────────────────────────────────────────────────────────────────────┐
│                    libFuzzer en contexto                             │
│                                                                      │
│  Origen: LLVM project (2015, Kostya Serebryany / Google)            │
│                                                                      │
│  Filosofía:                                                          │
│  • El fuzzer ES una librería que se linka con tu código              │
│  • Tú escribes una función, libFuzzer la llama millones de veces    │
│  • Sin fork, sin exec, sin archivos temporales                      │
│  • Máxima velocidad posible                                         │
│                                                                      │
│  Usado por:                                                          │
│  • OSS-Fuzz (Google) como backend principal                         │
│  • cargo-fuzz (Rust) como backend                                   │
│  • Chromium, Firefox, OpenSSL, curl, y cientos de proyectos         │
│                                                                      │
│  Requisitos:                                                         │
│  • Compilador Clang (no funciona con GCC)                           │
│  • El código fuente del target                                      │
│  • Implementar UNA función: LLVMFuzzerTestOneInput                  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 2. Arquitectura in-process vs fork-based

```
┌──────────────────────────────────────────────────────────────────────┐
│         AFL++ (fork-based)          vs    libFuzzer (in-process)     │
│                                                                      │
│  ┌──────────────┐                    ┌──────────────────────────┐   │
│  │   afl-fuzz   │                    │  Un solo proceso:        │   │
│  │   (proceso)  │                    │                          │   │
│  │              │                    │  ┌────────────────────┐  │   │
│  │  ┌────────┐  │  fork()            │  │  libFuzzer engine  │  │   │
│  │  │ Motor  ├──┼────────┐           │  │  (mutador,         │  │   │
│  │  │ fuzzer │  │        │           │  │   evaluador,       │  │   │
│  │  └────────┘  │        ▼           │  │   corpus mgmt)     │  │   │
│  │              │  ┌───────────┐     │  └─────────┬──────────┘  │   │
│  │  ┌────────┐  │  │  Target   │     │            │             │   │
│  │  │ Shared │◀─┼──│  (hijo)   │     │            ▼             │   │
│  │  │ memory │  │  │           │     │  LLVMFuzzerTestOneInput  │   │
│  │  │ bitmap │  │  └───────────┘     │  (tu función target)     │   │
│  │  └────────┘  │                    │                          │   │
│  └──────────────┘                    └──────────────────────────┘   │
│                                                                      │
│  AFL++:                              libFuzzer:                      │
│  • fork() por cada input             • Llamada a función directa    │
│    (o cada N con persistent)         • Sin fork, sin IPC            │
│  • IPC via shared memory             • Acceso directo a bitmap      │
│  • Overhead: ~1ms/exec (fork)        • Overhead: ~0.01ms/exec      │
│  • Aislamiento de crashes            • Un crash mata el fuzzer      │
│  • exec speed: 1K-5K/sec            • exec speed: 10K-100K/sec     │
└──────────────────────────────────────────────────────────────────────┘
```

### Trade-offs

| Aspecto | Fork-based (AFL++) | In-process (libFuzzer) |
|---|---|---|
| **Velocidad** | Media-alta | Muy alta |
| **Aislamiento** | Total (proceso separado) | Ninguno (mismo proceso) |
| **Si crashea** | Solo muere el hijo, fuzzer continúa | El fuzzer muere, guarda crash y sale |
| **State leakage** | Imposible (fork limpio) | Posible (variables globales persisten) |
| **Memory leaks** | Se limpian con cada fork | Se acumulan (necesita LeakSanitizer) |
| **Setup** | Binario + corpus en disco | Una función C + compilar |
| **Debugging** | Reproducir externamente | gdb directamente |

### Por qué libFuzzer es más rápido

```
  Costo por ejecución (microsegundos):

  AFL++ fork mode:
  ├── fork()              ~500μs
  ├── copy-on-write setup ~100μs
  ├── ejecutar target     ~50μs
  ├── waitpid()           ~100μs
  ├── leer bitmap         ~50μs
  └── Total:              ~800μs → ~1,250 exec/sec

  AFL++ persistent mode:
  ├── (fork cada 10K)     ~0.08μs amortizado
  ├── ejecutar target     ~50μs
  ├── leer bitmap         ~10μs
  └── Total:              ~60μs → ~16,000 exec/sec

  libFuzzer:
  ├── llamada a función   ~0.01μs
  ├── ejecutar target     ~50μs
  ├── evaluar bitmap      ~5μs (acceso directo, no IPC)
  └── Total:              ~55μs → ~18,000 exec/sec

  Para targets triviales (~1μs), libFuzzer puede alcanzar 100K+ exec/sec.
```

---

## 3. LLVMFuzzerTestOneInput: el contrato

La única función que debes implementar. libFuzzer la llama con cada input generado.

```c
/* El contrato de libFuzzer */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* 
     * data: puntero a los bytes generados por libFuzzer
     * size: número de bytes
     *
     * REGLAS:
     * 1. SIEMPRE retornar 0 (otros valores reservados para el futuro)
     * 2. NO llamar a exit() ni _exit()
     * 3. NO modificar 'data' (es read-only)
     * 4. NO guardar estado entre llamadas (cada llamada es independiente)
     * 5. NO escribir a stdout/stderr (reduce velocidad)
     * 6. Manejar size == 0 sin crashear
     * 7. Liberar toda la memoria allocada (leak detection)
     *
     * Si la función crashea (segfault, abort, sanitizer), libFuzzer
     * guarda el input en un archivo crash-<hash> y sale.
     */

    /* Tu código aquí */
    my_parser_parse(data, size);

    return 0;
}
```

### Ejemplos de harnesses correctos

```c
/* Harness mínimo: fuzzear un parser */
#include <stdint.h>
#include <stddef.h>
#include "my_parser.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ParseResult *result = parse(data, size);
    if (result) {
        parse_result_free(result);
    }
    return 0;
}
```

```c
/* Harness con interpretación del input como string */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "my_validator.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Muchos parsers esperan strings null-terminated */
    char *str = malloc(size + 1);
    if (!str) return 0;
    memcpy(str, data, size);
    str[size] = '\0';

    validate_input(str);

    free(str);
    return 0;
}
```

```c
/* Harness que limita el tamaño del input */
#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Evitar inputs enormes que causan lentitud */
    if (size > 4096) return 0;
    if (size < 2) return 0;  /* el parser necesita al menos 2 bytes */

    process_packet(data, size);
    return 0;
}
```

### Errores comunes en harnesses

```c
/* ❌ ERROR: no retorna 0 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0) return -1;  /* ← INCORRECTO: siempre retornar 0 */
    parse(data, size);
    return 0;
}

/* ❌ ERROR: llama a exit() */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!parse(data, size)) {
        exit(1);  /* ← INCORRECTO: mata al fuzzer */
    }
    return 0;
}

/* ❌ ERROR: memory leak */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    char *copy = malloc(size);  /* ← nunca se libera */
    memcpy(copy, data, size);
    parse(copy, size);
    /* falta: free(copy) */
    return 0;
}

/* ❌ ERROR: estado global entre llamadas */
static int call_count = 0;
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    call_count++;  /* ← state leakage, el comportamiento depende de llamadas previas */
    if (call_count > 1000) reset_state();
    parse(data, size);
    return 0;
}
```

---

## 4. Compilar con -fsanitize=fuzzer

`-fsanitize=fuzzer` le dice a Clang que:
1. Instrumente el código con contadores de cobertura
2. Linke la librería libFuzzer (que contiene el `main()`)
3. El `main()` de libFuzzer llama a `LLVMFuzzerTestOneInput`

```bash
# Compilación más simple posible
clang -fsanitize=fuzzer -o fuzz_target harness.c target.c

# Con sanitizers (RECOMENDADO)
clang -fsanitize=fuzzer,address,undefined -o fuzz_target harness.c target.c

# Con debug symbols (RECOMENDADO)
clang -fsanitize=fuzzer,address,undefined -g -o fuzz_target harness.c target.c

# Con optimización (recomendado para velocidad)
clang -fsanitize=fuzzer,address,undefined -g -O1 -o fuzz_target harness.c target.c
```

### Qué hace -fsanitize=fuzzer

```
┌──────────────────────────────────────────────────────────────────────┐
│         Qué hace -fsanitize=fuzzer                                   │
│                                                                      │
│  1. INSTRUMENTACIÓN                                                  │
│     Inserta llamadas a __sanitizer_cov_trace_pc_guard()              │
│     en cada edge del control flow.                                   │
│     (Misma instrumentación que AFL++ PCGUARD)                        │
│                                                                      │
│  2. LINKADO                                                          │
│     Linka con libFuzzer.a que contiene:                              │
│     • main() — el punto de entrada real del programa                 │
│     • Motor de mutación (bit flip, arithmetic, havoc, etc.)         │
│     • Gestión de corpus                                              │
│     • Evaluación de cobertura                                        │
│     • Detección de crashes                                           │
│     • Output y estadísticas                                          │
│                                                                      │
│  3. EFECTO                                                           │
│     El programa resultante ES el fuzzer.                             │
│     No necesitas afl-fuzz ni otro programa externo.                  │
│     Solo: ./fuzz_target [corpus_dir] [options]                       │
│                                                                      │
│  IMPORTANTE: -fsanitize=fuzzer proporciona el main().                │
│  Tu código NO debe tener un main() propio.                           │
│  Si lo tiene, habrá conflicto de símbolos.                           │
└──────────────────────────────────────────────────────────────────────┘
```

### Error: "multiple definition of `main`"

```bash
# ❌ Esto falla si target.c tiene main()
clang -fsanitize=fuzzer -o fuzz target.c harness.c
# error: multiple definition of `main`

# ✓ Solución 1: compilar sin el archivo que tiene main()
clang -fsanitize=fuzzer -o fuzz harness.c parser.c validator.c
# (no incluir main.c)

# ✓ Solución 2: condicional en el código
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
/* No incluir main() cuando se compila con fuzzer */
#else
int main(int argc, char *argv[]) { ... }
#endif
```

---

## 5. Separar instrumentación de sanitizers

`-fsanitize=fuzzer` combina dos cosas: instrumentación de cobertura + el runtime
de libFuzzer. A veces necesitas separarlas.

### -fsanitize=fuzzer-no-link

```bash
# Solo instrumentación, sin linkar libFuzzer
# Útil para compilar librerías que luego se linkean con un harness
clang -fsanitize=fuzzer-no-link -c -o parser.o parser.c
clang -fsanitize=fuzzer-no-link -c -o validator.o validator.c

# El harness sí se compila con el fuzzer completo
clang -fsanitize=fuzzer,address -o fuzz_target harness.c parser.o validator.o
```

### Caso de uso: librería compartida

```bash
# Compilar la librería con instrumentación pero sin el runtime
clang -fsanitize=fuzzer-no-link,address -shared -o libtarget.so target.c

# Compilar el harness con el runtime y linkar con la librería
clang -fsanitize=fuzzer,address -o fuzz_target harness.c -L. -ltarget
```

### Caso de uso: separar archivos instrumentados y no instrumentados

```bash
# Solo instrumentar el código que quieres fuzzear
clang -fsanitize=fuzzer-no-link -c parser.c -o parser.o

# Código auxiliar sin instrumentación (más rápido)
clang -c utils.c -o utils.o  # sin -fsanitize

# Linkar todo con el runtime de libFuzzer
clang -fsanitize=fuzzer,address -o fuzz_target harness.c parser.o utils.o
```

Esto es útil cuando tienes código auxiliar grande que no quieres instrumentar
(reduce el ruido en el bitmap de cobertura y mejora la velocidad).

---

## 6. Primera ejecución

### Programa mínimo

```c
/* simple_target.c */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

void process(const uint8_t *data, size_t size) {
    if (size >= 3 && data[0] == 'F' && data[1] == 'U' && data[2] == 'Z') {
        if (size >= 6 && memcmp(data + 3, "ZME", 3) == 0) {
            /* Bug: null pointer dereference */
            char *p = NULL;
            *p = 'x';
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    process(data, size);
    return 0;
}
```

### Compilar y ejecutar

```bash
# Compilar
clang -fsanitize=fuzzer,address -g -o fuzz_simple simple_target.c

# Crear directorio de corpus (puede empezar vacío)
mkdir corpus

# Ejecutar
./fuzz_simple corpus/
```

### Output

```
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 1234567890
INFO: Loaded 1 modules   (42 inline 8-bit counters): 42 [0x...]
INFO: Loaded 1 PC tables (42 PCs): 42 [0x...]
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: A]      0 files found in corpus/
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: seed corpus: files: 0 min: 0b max: 0b total: 0b rss: 30Mb
#2      INITED cov: 3 ft: 3 corp: 1/1b exec/s: 0 rss: 30Mb
#8      NEW    cov: 4 ft: 4 corp: 2/4b lim: 4 exec/s: 0 rss: 30Mb L: 3/3 MS: 1 ...
#15     NEW    cov: 5 ft: 5 corp: 3/7b lim: 4 exec/s: 0 rss: 30Mb L: 3/3 MS: 2 ...
#128    NEW    cov: 7 ft: 7 corp: 4/13b lim: 4 exec/s: 0 rss: 31Mb L: 6/6 MS: ...
==12345==ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000
    #0 0x4f1234 in process simple_target.c:8:13
    #1 0x4f1345 in LLVMFuzzerTestOneInput simple_target.c:14:5
...
artifact_prefix='./'; Test unit written to ./crash-abc123...
```

---

## 7. Interpretar el output de libFuzzer

### Formato de cada línea

```
#128    NEW    cov: 7 ft: 7 corp: 4/13b lim: 4 exec/s: 15234 rss: 31Mb L: 6/6 MS: 3 ...
│       │      │       │     │          │       │              │        │       │
│       │      │       │     │          │       │              │        │       └── Mutaciones
│       │      │       │     │          │       │              │        └── Longitud del input
│       │      │       │     │          │       │              └── RSS memory
│       │      │       │     │          │       └── Ejecuciones/segundo
│       │      │       │     │          └── Límite de longitud
│       │      │       │     └── Corpus: archivos/bytes totales
│       │      │       └── Feature count
│       │      └── Cobertura (edges)
│       └── Tipo de evento
└── Número de ejecución
```

### Tipos de evento

| Evento | Significado |
|---|---|
| `INITED` | Inicialización completada, corpus cargado |
| `NEW` | Input que descubrió cobertura nueva → agregado al corpus |
| `REDUCE` | Se encontró un input más corto para la misma cobertura |
| `pulse` | Heartbeat periódico (sin hallazgos nuevos, solo stats) |
| `DONE` | Fuzzing terminado (por `-max_total_time` o `-runs`) |
| `BINGO` | Crash encontrado (solo en algunos modos) |

### Campos importantes

| Campo | Significado | Qué vigilar |
|---|---|---|
| `cov` | Edges de código cubiertos | Debe crecer al inicio |
| `ft` | Features (edges + hit counts) | Más fino que cov |
| `corp` | Tamaño del corpus (archivos/bytes) | Crece con NEW |
| `lim` | Max length para inputs generados | Crece automáticamente |
| `exec/s` | Ejecuciones por segundo | > 1000 bueno, > 10000 excelente |
| `rss` | Memoria RSS del proceso | Vigilar que no crezca sin control |
| `L` | Longitud del último input (actual/max) | — |
| `MS` | Mutation sequence | Qué mutaciones produjeron el input |

### Mutation sequence (MS)

```
MS: 3 ChangeByte-InsertByte-ShuffleBytes
```

Tipos de mutaciones de libFuzzer:

| Mutación | Qué hace |
|---|---|
| `EraseBytes` | Eliminar N bytes |
| `InsertByte` | Insertar un byte |
| `InsertRepeatedBytes` | Insertar N bytes repetidos |
| `ChangeByte` | Cambiar un byte |
| `ChangeBit` | Cambiar un bit |
| `ShuffleBytes` | Reordenar bytes |
| `ChangeASCIIInteger` | Cambiar un entero ASCII (ej: "42" → "43") |
| `ChangeBinaryInteger` | Cambiar un entero binario |
| `CopyPart` | Copiar una sección a otra posición |
| `CrossOver` | Combinar con otro input del corpus |
| `ManualDict` | Insertar un token del diccionario |
| `PersAutoDict` | Token del diccionario automático |

---

## 8. El corpus en libFuzzer

### Uso del corpus

```bash
# Ejecutar con directorio de corpus (se carga al inicio y se guarda durante fuzzing)
./fuzz_target corpus_dir/

# Múltiples directorios: el primero es el de escritura, los demás de lectura
./fuzz_target corpus_main/ corpus_extra/ corpus_seeds/

# El primer directorio (corpus_main/) recibe los inputs nuevos.
# Los demás se leen al inicio pero no se modifican.
```

### Corpus vacío vs con semillas

```bash
# Corpus vacío: libFuzzer empieza con un input de 0 bytes
# y genera todo desde cero. Funciona pero es más lento.
mkdir empty_corpus
./fuzz_target empty_corpus/

# Con semillas: más rápido para targets con formato
mkdir corpus
echo '{"key": "value"}' > corpus/seed1.json
echo '[]' > corpus/seed2.json
echo 'null' > corpus/seed3.json
./fuzz_target corpus/
```

### Archivos del corpus

```bash
# Durante el fuzzing, libFuzzer guarda inputs interesantes como:
ls corpus/
# seed1.json
# seed2.json
# seed3.json
# <hash1>      ← input que descubrió cobertura nueva
# <hash2>      ← otro input interesante
# ...

# Los nombres son hashes SHA1 del contenido.
# No codifican metadatos como AFL (no hay id:, src:, op:).
```

### Reusar el corpus entre sesiones

```bash
# Sesión 1: genera corpus
./fuzz_target corpus/ -max_total_time=3600  # 1 hora

# Sesión 2: continúa donde dejó (corpus ya tiene inputs)
./fuzz_target corpus/ -max_total_time=3600  # otra hora

# El corpus es acumulativo. Cada sesión empieza con los hallazgos
# de las sesiones anteriores.
```

---

## 9. Merge de corpus: -merge=1

El merge combina múltiples directorios de corpus en uno mínimo que mantiene
la misma cobertura. Equivalente a `afl-cmin`.

### Uso básico

```bash
# Merge: tomar los mejores inputs de corpus_grande/ y ponerlos en corpus_min/
mkdir corpus_min
./fuzz_target -merge=1 corpus_min/ corpus_grande/

# Output:
# MERGE-OUTER: 1500 files, 0 in the initial corpus
# MERGE-OUTER: attempt 1
# ...
# MERGE-OUTER: 234 new files with 2340 new features added
```

### Merge de múltiples fuentes

```bash
# Combinar corpus de varias sesiones/máquinas
mkdir corpus_merged
./fuzz_target -merge=1 corpus_merged/ \
    session1_corpus/ \
    session2_corpus/ \
    session3_corpus/ \
    extra_seeds/

# corpus_merged/ contiene el subconjunto mínimo que cubre
# todos los edges de todas las fuentes.
```

### Cuándo hacer merge

```
┌──────────────────────────────────────────────────────────────────────┐
│          Cuándo hacer merge                                          │
│                                                                      │
│  1. Después de una sesión larga de fuzzing                          │
│     El corpus acumula entradas redundantes.                          │
│     Merge reduce el tamaño sin perder cobertura.                    │
│                                                                      │
│  2. Antes de subir el corpus al repositorio                         │
│     Un corpus de 50MB es molesto en git.                            │
│     Después de merge podría ser 2MB.                                │
│                                                                      │
│  3. Cuando combinas corpus de varias fuentes                        │
│     Cada CI run genera un corpus. Merge los combina.                │
│                                                                      │
│  4. Periodicamente en fuzzing continuo                              │
│     Cada N horas: parar → merge → reiniciar.                       │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 10. Minimizar un crash: -minimize_crash=1

Cuando libFuzzer encuentra un crash, el input puede ser grande. `-minimize_crash=1`
lo reduce al mínimo necesario para reproducir el mismo crash.

### Uso

```bash
# libFuzzer guarda el crash en un archivo como:
# crash-<hash>

# Minimizar:
./fuzz_target -minimize_crash=1 -max_total_time=60 crash-abc123def456

# Output:
# INFO: Attempting to minimize crash 'crash-abc123def456' (1234 bytes)
# ...
# CRASH_MIN: minimizing crash 'crash-abc123def456' (1234 bytes)
# CRASH_MIN: (6 bytes) wrote to minimized-from-abc123def456
```

### Flags útiles para minimización

```bash
# Máximo tiempo de minimización (60 segundos)
./fuzz_target -minimize_crash=1 -max_total_time=60 crash-file

# Exactitud vs velocidad: más intentos = más pequeño
./fuzz_target -minimize_crash=1 -max_total_time=300 crash-file

# Minimizar con tamaño exacto de input
./fuzz_target -minimize_crash=1 -exact_artifact_path=./crash_min crash-file
```

### Diferencia con afl-tmin

| Aspecto | libFuzzer -minimize_crash | afl-tmin |
|---|---|---|
| Velocidad | Más rápido (in-process) | Más lento (fork por cada prueba) |
| Calidad | Buena (heurísticas de mutación) | Excelente (más determinista) |
| Input | Archivo de crash | Archivo de crash |
| Output | `minimized-from-<hash>` | Archivo especificado con -o |

---

## 11. Diccionarios en libFuzzer

Mismo formato que AFL++ (un token por línea).

### Uso

```bash
# Pasar diccionario con -dict=
./fuzz_target -dict=json.dict corpus/

# Verificar que se cargó:
# INFO: loaded dict from json.dict, 15 tokens
```

### Ejemplo de diccionario para XML

```
# xml.dict
tag_open="<"
tag_close=">"
tag_close_slash="</"
tag_self_close="/>"
attr_eq="="
attr_quote="\""
comment_open="<!--"
comment_close="-->"
cdata_open="<![CDATA["
cdata_close="]]>"
pi_open="<?"
pi_close="?>"
entity_amp="&amp;"
entity_lt="&lt;"
entity_gt="&gt;"
entity_quot="&quot;"
xmlns="xmlns"
xml_decl="<?xml version=\"1.0\"?>"
doctype="<!DOCTYPE"
```

### Autodictionary

libFuzzer puede extraer automáticamente tokens de las comparaciones del código
(similar a CmpLog en AFL++). Se activa automáticamente con la instrumentación
PCGUARD:

```
# En el output, verás:
# INFO: PersAutoDict: 3 entries
# Esto significa que libFuzzer encontró 3 tokens en comparaciones del código
```

---

## 12. LLVMFuzzerInitialize: setup costoso

Si tu target necesita inicialización costosa (cargar archivos, crear contexto,
parsear configuración), hazla en `LLVMFuzzerInitialize` — se ejecuta una sola
vez antes de empezar el fuzzing.

```c
#include <stdint.h>
#include <stddef.h>

/* Contexto global — inicializado una vez, usado en cada llamada */
static ParserConfig *g_config = NULL;
static ValidationRules *g_rules = NULL;

/* Se llama una sola vez antes del fuzzing */
int LLVMFuzzerInitialize(int *argc, char ***argv) {
    /* Setup costoso: una vez */
    g_config = parser_config_load("default.conf");
    g_rules = validation_rules_compile("rules.json");

    /* Puedes usar argc/argv para pasar opciones al harness */
    for (int i = 1; i < *argc; i++) {
        if (strcmp((*argv)[i], "--strict") == 0) {
            g_config->strict_mode = 1;
        }
    }

    return 0;  /* siempre retornar 0 */
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Usa g_config y g_rules sin reinicializarlos */
    ParseResult *result = parse_with_config(data, size, g_config);
    if (result) {
        validate(result, g_rules);
        parse_result_free(result);
    }
    return 0;
}
```

### Cuándo usar LLVMFuzzerInitialize

| Escenario | Usar Initialize | Razón |
|---|---|---|
| Cargar archivo de configuración | Sí | Solo se lee una vez |
| Compilar regex | Sí | Compilar regex es costoso |
| Abrir conexión de DB (para reads) | Sí | Una conexión, muchas queries |
| Allocar buffer de trabajo | Sí | Si el tamaño es fijo |
| Crear thread pool | No (evitar threads) | No determinista |
| Inicializar RNG | No | Introduce no-determinismo |

---

## 13. LLVMFuzzerCustomMutator: mutaciones propias

Para targets con formatos complejos, puedes escribir un mutador custom que genere
inputs más inteligentes que las mutaciones genéricas.

```c
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* libFuzzer llama a esta función para mutar un input.
   Si la defines, libFuzzer la usa EN LUGAR de sus mutaciones default.
   
   Parámetros:
   - data: input actual (mutable)
   - size: tamaño actual del input
   - max_size: tamaño máximo del buffer
   - seed: semilla para RNG (usar para reproducibilidad)
   
   Retorno: tamaño del input mutado */
size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                size_t max_size, unsigned int seed) {
    /* Ejemplo: mutador para un formato "CMD:DATA" */
    
    /* 50% del tiempo: usar mutaciones default de libFuzzer */
    if (seed % 2 == 0) {
        return LLVMFuzzerMutate(data, size, max_size);
    }
    
    /* 50% del tiempo: mutación custom */
    /* Insertar un comando aleatorio al inicio */
    const char *commands[] = {"GET:", "SET:", "DEL:", "LIST:", "PING:"};
    int cmd_idx = seed % 5;
    size_t cmd_len = strlen(commands[cmd_idx]);
    
    if (cmd_len + size > max_size) {
        size = max_size - cmd_len;
    }
    
    /* Mover datos existentes para hacer espacio */
    memmove(data + cmd_len, data, size);
    memcpy(data, commands[cmd_idx], cmd_len);
    
    return cmd_len + size;
}
```

### LLVMFuzzerMutate: usar las mutaciones default

Dentro de tu custom mutator, puedes llamar a `LLVMFuzzerMutate` para aplicar
las mutaciones estándar de libFuzzer sobre parte del input:

```c
size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                size_t max_size, unsigned int seed) {
    /* Preservar el header de 4 bytes, mutar solo el payload */
    if (size < 4) {
        return LLVMFuzzerMutate(data, size, max_size);
    }
    
    /* Guardar el header */
    uint8_t header[4];
    memcpy(header, data, 4);
    
    /* Mutar el payload (bytes 4 en adelante) */
    size_t new_payload_size = LLVMFuzzerMutate(
        data + 4, size - 4, max_size - 4);
    
    /* Restaurar el header */
    memcpy(data, header, 4);
    
    return 4 + new_payload_size;
}
```

### Cuándo escribir un custom mutator

```
┌──────────────────────────────────────────────────────────────────────┐
│         Cuándo vale la pena un custom mutator                        │
│                                                                      │
│  SÍ:                                                                 │
│  • Formatos con checksums (CRC, hash) — el mutador                  │
│    debe recalcular el checksum después de mutar                      │
│  • Formatos con campos de longitud — la longitud                    │
│    debe coincidir con los datos                                      │
│  • Protocolos con estructura rígida (headers fijos                   │
│    + payload variable)                                               │
│  • Cuando el diccionario no es suficiente                           │
│                                                                      │
│  NO (usar mutaciones default):                                       │
│  • Formatos de texto (JSON, XML, INI) — el diccionario basta       │
│  • Cuando no conoces bien el formato                                │
│  • Cuando las mutaciones default encuentran crashes rápido           │
│                                                                      │
│  Regla general: empezar sin custom mutator. Si después de            │
│  1 hora la cobertura no crece, considerar uno.                       │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 14. LLVMFuzzerCustomCrossOver: combinación de inputs

El cross-over combina dos inputs del corpus para producir uno nuevo. Es la
versión libFuzzer del "splice" de AFL++.

```c
/* Combina dos inputs del corpus */
size_t LLVMFuzzerCustomCrossOver(
    const uint8_t *data1, size_t size1,
    const uint8_t *data2, size_t size2,
    uint8_t *out, size_t max_out_size,
    unsigned int seed) {
    
    /* Ejemplo: tomar el header de data1 y el payload de data2 */
    size_t header_len = (size1 < 8) ? size1 : 8;
    size_t payload_len = size2;
    
    if (header_len + payload_len > max_out_size) {
        payload_len = max_out_size - header_len;
    }
    
    memcpy(out, data1, header_len);
    memcpy(out + header_len, data2, payload_len);
    
    return header_len + payload_len;
}
```

---

## 15. Flags de ejecución esenciales

### Control de tiempo y ejecución

```bash
# Máximo tiempo total de fuzzing (segundos)
./fuzz_target corpus/ -max_total_time=3600  # 1 hora

# Máximo número de ejecuciones
./fuzz_target corpus/ -runs=1000000  # 1 millón de ejecuciones

# Ejecutar 0 runs (solo cargar el corpus y salir — útil para verificar)
./fuzz_target corpus/ -runs=0
```

### Control de input

```bash
# Tamaño máximo de input generado (bytes)
./fuzz_target corpus/ -max_len=1024

# Tamaño mínimo
./fuzz_target corpus/ -len_control=100  # controla la velocidad de crecimiento

# Solo ejecutar un input específico (para reproducir)
./fuzz_target input_file
```

### Control de output

```bash
# Prefijo para archivos de artefactos (crashes, timeouts)
./fuzz_target corpus/ -artifact_prefix=./artifacts/

# Nombre exacto para el artefacto
./fuzz_target corpus/ -exact_artifact_path=./crash_out

# Imprimir el input que causó crash (como C array)
./fuzz_target corpus/ -print_final_stats=1

# Estadísticas finales
./fuzz_target corpus/ -print_final_stats=1
```

### Control de detección

```bash
# Timeout por ejecución (segundos, default: 1200)
./fuzz_target corpus/ -timeout=5

# Límite de memoria RSS (MB, default: 2048)
./fuzz_target corpus/ -rss_limit_mb=512

# Detectar memory leaks (default: si ASan está activado)
./fuzz_target corpus/ -detect_leaks=1

# Desactivar detección de leaks (más rápido, menos ruido)
./fuzz_target corpus/ -detect_leaks=0
```

### Semilla para reproducibilidad

```bash
# Fijar la semilla del RNG
./fuzz_target corpus/ -seed=42

# Sin fijar, libFuzzer elige una semilla aleatoria y la muestra:
# INFO: Seed: 1234567890
# Puedes reusar esta semilla para reproducir la sesión exacta
```

### Tabla de flags más usados

| Flag | Default | Descripción |
|---|---|---|
| `-max_total_time=N` | Infinito | Parar después de N segundos |
| `-runs=N` | Infinito | Parar después de N ejecuciones |
| `-max_len=N` | 4096 | Tamaño máximo del input |
| `-timeout=N` | 1200 | Timeout por ejecución (segundos) |
| `-rss_limit_mb=N` | 2048 | Límite de memoria RSS |
| `-dict=file` | Ninguno | Archivo de diccionario |
| `-seed=N` | Aleatorio | Semilla del RNG |
| `-jobs=N` | 0 | Número de procesos paralelos |
| `-workers=N` | (min(jobs,CPUs/2)) | Workers paralelos |
| `-fork=N` | 0 | Fork N procesos para fuzzing |
| `-merge=1` | 0 | Modo merge de corpus |
| `-minimize_crash=1` | 0 | Minimizar un crash |
| `-detect_leaks=B` | 1 (con ASan) | Detectar memory leaks |
| `-print_final_stats=1` | 0 | Estadísticas al final |
| `-artifact_prefix=DIR` | `./` | Dónde guardar crashes |
| `-use_value_profile=1` | 0 | Rastrear valores de comparaciones |
| `-only_ascii=1` | 0 | Solo generar inputs ASCII |

---

## 16. Value profile: resolver comparaciones

`-use_value_profile=1` es la respuesta de libFuzzer al CmpLog de AFL++.
Rastrea los valores de las comparaciones para guiar las mutaciones.

### Cómo funciona

```
┌──────────────────────────────────────────────────────────────────────┐
│         Value profile                                                │
│                                                                      │
│  Código:                                                             │
│  if (memcmp(data, "SECRET", 6) == 0) { ... }                       │
│                                                                      │
│  Sin value profile:                                                  │
│  Input "ABCDEF" → memcmp retorna != 0 → no pasa                    │
│  Input "SBCDEF" → memcmp retorna != 0 → no pasa                    │
│  Ambos cubren el mismo edge (la rama false del if).                  │
│  libFuzzer no sabe que "SBCDEF" está más cerca de "SECRET".         │
│                                                                      │
│  Con value profile:                                                  │
│  Input "ABCDEF" → hamming distance a "SECRET" = 5                   │
│  Input "SBCDEF" → hamming distance a "SECRET" = 4 ← ¡MÁS CERCA!   │
│  Input "SECDEF" → hamming distance = 3 ← ¡AÚN MÁS CERCA!          │
│  Input "SECREF" → hamming distance = 1                               │
│  Input "SECRET" → hamming distance = 0 → ¡PASA!                    │
│                                                                      │
│  Value profile trata cada mejora en la distancia como                │
│  "nueva cobertura", lo que guía al fuzzer hacia la comparación      │
│  correcta byte por byte.                                             │
└──────────────────────────────────────────────────────────────────────┘
```

### Uso

```bash
# Activar value profile
./fuzz_target corpus/ -use_value_profile=1

# Combinar con diccionario para máxima efectividad
./fuzz_target corpus/ -use_value_profile=1 -dict=tokens.dict
```

### Trade-offs

| Aspecto | Sin value profile | Con value profile |
|---|---|---|
| Velocidad | Más rápido | ~20-30% más lento |
| Bitmap size | Normal | ~2x más grande |
| Comparaciones multi-byte | Muy difíciles | Resueltas gradualmente |
| Falsos positivos en NEW | Mínimos | Más NEWs (no siempre útiles) |
| Recomendado | Targets simples | Targets con magic bytes/strcmp |

---

## 17. Fork mode: -fork=N

A partir de versiones recientes, libFuzzer soporta fuzzing con múltiples procesos
(similar al paralelismo de AFL++).

```bash
# Lanzar 4 procesos de fuzzing
./fuzz_target corpus/ -fork=4

# Cada proceso forkea, fuzzea independientemente, y periódicamente
# sincroniza sus hallazgos con los demás (merge automático).
```

### Cómo funciona

```
┌──────────────────────────────────────────────────────────────────────┐
│         libFuzzer fork mode                                          │
│                                                                      │
│  ./fuzz_target corpus/ -fork=4                                      │
│                                                                      │
│  ┌────────────────────────────────────────────────┐                 │
│  │  Proceso controller                             │                 │
│  │  (el que lanzaste)                              │                 │
│  │                                                  │                 │
│  │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐          │                 │
│  │  │ W1   │ │ W2   │ │ W3   │ │ W4   │          │                 │
│  │  │ fuzz │ │ fuzz │ │ fuzz │ │ fuzz │          │                 │
│  │  └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘          │                 │
│  │     │        │        │        │               │                 │
│  │     └────────┴────────┴────────┘               │                 │
│  │              │                                  │                 │
│  │         Merge periódico                         │                 │
│  │         del corpus                              │                 │
│  └────────────────────────────────────────────────┘                 │
│                                                                      │
│  Diferencia con AFL++ paralelo:                                     │
│  • AFL++: cada instancia es un proceso independiente con su TUI     │
│  • libFuzzer fork: un proceso controller coordina N workers         │
│  • libFuzzer fork: merge automático del corpus                      │
│  • libFuzzer fork: un solo comando para lanzar todo                 │
└──────────────────────────────────────────────────────────────────────┘
```

### Combinar con jobs/workers

```bash
# -jobs=N: lanzar N sesiones de fuzzing secuenciales (reinicia cuando una termina)
# -workers=N: ejecutar hasta N sesiones en paralelo

# Ejemplo: 100 sesiones cortas, 4 en paralelo
./fuzz_target corpus/ -jobs=100 -workers=4 -max_total_time=60
```

---

## 18. Detección de memory leaks

libFuzzer integra LeakSanitizer para detectar memory leaks. Esto es único
respecto a AFL++ (que no detecta leaks por defecto ya que cada fork libera
toda la memoria).

### Comportamiento

```bash
# Por defecto con ASan, leak detection está activado
clang -fsanitize=fuzzer,address -o fuzz_target harness.c target.c
./fuzz_target corpus/

# Si hay un leak, libFuzzer reporta:
# ==12345==ERROR: LeakSanitizer: detected memory leaks
# Direct leak of 1024 byte(s) in 1 object(s) allocated from:
#     #0 0x... in malloc
#     #1 0x... in parse_data target.c:42
#     ...
# artifact_prefix='./'; Test unit written to ./leak-abc123
```

### Controlar detección de leaks

```bash
# Desactivar (más rápido, menos ruido)
./fuzz_target corpus/ -detect_leaks=0

# O via variable de entorno
ASAN_OPTIONS=detect_leaks=0 ./fuzz_target corpus/
```

### Por qué los leaks importan en fuzzing in-process

```
  AFL++ (fork mode):
  Cada fork libera toda la memoria al terminar.
  Un leak no se acumula → no hay problema.

  libFuzzer (in-process):
  Cada llamada a LLVMFuzzerTestOneInput ocurre en el mismo proceso.
  Un leak de 100 bytes × 1 millón de llamadas = 100MB de memoria leakeada.
  → El proceso se queda sin memoria → OOM → falso positivo.

  Por eso libFuzzer detecta leaks: son bugs REALES en el contexto
  de un servidor que procesa millones de requests sin reiniciar.
```

---

## 19. Timeouts y OOM

### Timeout

```bash
# Default: 1200 segundos (20 minutos) por ejecución
# Para la mayoría de parsers, 1-5 segundos es suficiente

# Ajustar el timeout
./fuzz_target corpus/ -timeout=5  # 5 segundos

# Cuando libFuzzer detecta un timeout:
# ALARM: working on the last Unit for 5 seconds
# ==12345== libFuzzer: timeout after 5 seconds
# artifact_prefix='./'; Test unit written to ./timeout-abc123
```

### OOM (Out of Memory)

```bash
# Límite de RSS (default: 2048 MB)
./fuzz_target corpus/ -rss_limit_mb=512

# Cuando se excede:
# ==12345== libFuzzer: out-of-memory (used: 520Mb; limit: 512Mb)
# artifact_prefix='./'; Test unit written to ./oom-abc123
```

### Diferenciar timeout real vs target lento

```
  Timeout a los 5 segundos:
  ¿Es un bug real o es que el target es lento?

  Verificar:
  time ./fuzz_target timeout-abc123
  
  Si tarda 0.01 segundos: es un bug (loop infinito con otros inputs)
  Si tarda 4.99 segundos: el target es lento con este input
  
  Para targets lentos: aumentar -timeout
  Para loops infinitos: investigar el bug
```

---

## 20. Integración con sanitizers

### Compilación con cada sanitizer

```bash
# ASan + UBSan (el combo más común y recomendado)
clang -fsanitize=fuzzer,address,undefined -g -O1 -o fuzz_asan harness.c target.c

# MSan (NO combinar con ASan)
clang -fsanitize=fuzzer,memory -g -O1 -o fuzz_msan harness.c target.c
# IMPORTANTE: MSan requiere que TODAS las librerías estén compiladas
# con MSan. Si linkas con libc o libstdc++ sin MSan, habrá falsos positivos.

# TSan (para código con threads)
clang -fsanitize=fuzzer,thread -g -O1 -o fuzz_tsan harness.c target.c

# Solo fuzzer (sin sanitizers — para benchmarks de velocidad)
clang -fsanitize=fuzzer -g -O1 -o fuzz_plain harness.c target.c
```

### Qué detecta cada combinación

| Combinación | Detecta | Velocidad |
|---|---|---|
| `fuzzer,address,undefined` | Buffer overflow, UAF, double-free, signed overflow, null deref, shift overflow | Base × 0.5 |
| `fuzzer,memory` | Uso de memoria no inicializada | Base × 0.3 |
| `fuzzer,thread` | Data races, deadlocks | Base × 0.1 |
| `fuzzer` (solo) | Solo crashes por señales (SIGSEGV, SIGABRT) | Base × 1.0 |

### Estrategia multi-sanitizer

```bash
# Compilar tres versiones
clang -fsanitize=fuzzer,address,undefined -g -O1 -o fuzz_asan harness.c target.c
clang -fsanitize=fuzzer,memory -g -O1 -o fuzz_msan harness.c target.c
clang -fsanitize=fuzzer -g -O1 -o fuzz_plain harness.c target.c

# Fuzzear con ASan (principal — encuentra los bugs más comunes)
./fuzz_asan corpus_asan/ -max_total_time=3600

# Fuzzear con MSan (complementario — encuentra bugs que ASan no ve)
./fuzz_msan corpus_msan/ -max_total_time=3600

# Fuzzear sin sanitizer (rápido — para maximizar cobertura)
./fuzz_plain corpus_plain/ -max_total_time=3600

# Merge de todos los corpus para la siguiente sesión
./fuzz_asan -merge=1 corpus_merged/ corpus_asan/ corpus_msan/ corpus_plain/
```

---

## 21. Estructura de un proyecto con libFuzzer

### Layout recomendado

```
my_library/
├── Makefile
├── include/
│   └── my_parser.h          ← API pública
├── src/
│   ├── parser.c              ← implementación
│   ├── validator.c
│   └── utils.c
├── fuzz/
│   ├── fuzz_parse.c          ← harness 1: fuzzear parse()
│   ├── fuzz_validate.c       ← harness 2: fuzzear validate()
│   ├── fuzz_roundtrip.c      ← harness 3: parse → serialize → parse
│   ├── corpus_parse/         ← corpus para parse
│   ├── corpus_validate/      ← corpus para validate
│   ├── corpus_roundtrip/     ← corpus para roundtrip
│   └── dict/
│       └── format.dict       ← diccionario compartido
├── tests/
│   ├── test_parser.c         ← unit tests
│   └── regression/           ← crashes minimizados como test cases
│       ├── crash_parse_001
│       ├── crash_parse_002
│       └── crash_validate_001
└── scripts/
    └── fuzz.sh               ← script para lanzar fuzzing
```

### Makefile para fuzzing

```makefile
CC = clang
CFLAGS = -g -O1 -I include
FUZZ_FLAGS = -fsanitize=fuzzer,address,undefined
SRC = src/parser.c src/validator.c src/utils.c

# Targets de fuzzing
fuzz_parse: fuzz/fuzz_parse.c $(SRC)
	$(CC) $(CFLAGS) $(FUZZ_FLAGS) -o $@ $^

fuzz_validate: fuzz/fuzz_validate.c $(SRC)
	$(CC) $(CFLAGS) $(FUZZ_FLAGS) -o $@ $^

fuzz_roundtrip: fuzz/fuzz_roundtrip.c $(SRC)
	$(CC) $(CFLAGS) $(FUZZ_FLAGS) -o $@ $^

# Ejecutar fuzzing
run_fuzz_parse: fuzz_parse
	mkdir -p fuzz/corpus_parse
	./fuzz_parse fuzz/corpus_parse/ -dict=fuzz/dict/format.dict \
		-max_total_time=3600 -artifact_prefix=./fuzz/crashes/

# Merge corpus
merge_parse: fuzz_parse
	mkdir -p fuzz/corpus_parse_min
	./fuzz_parse -merge=1 fuzz/corpus_parse_min/ fuzz/corpus_parse/

# Regression tests: ejecutar todos los crashes guardados como tests
regression: fuzz_parse
	@for f in tests/regression/crash_parse_*; do \
		echo "Testing $$f..."; \
		./fuzz_parse $$f || exit 1; \
	done
	@echo "All regression tests passed."

# Ejecutar corpus como tests (verificar que ningún input crashea)
corpus_test: fuzz_parse
	./fuzz_parse fuzz/corpus_parse/ -runs=0

.PHONY: run_fuzz_parse merge_parse regression corpus_test
```

---

## 22. libFuzzer en CI/CD

### GitHub Actions

```yaml
name: Fuzzing
on:
  push:
    branches: [main]
  pull_request:
  schedule:
    - cron: '0 2 * * *'  # Cada noche a las 2am

jobs:
  fuzz:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        target: [fuzz_parse, fuzz_validate, fuzz_roundtrip]
    steps:
      - uses: actions/checkout@v4

      - name: Install Clang
        run: sudo apt-get install -y clang

      - name: Build fuzzer
        run: make ${{ matrix.target }}

      - name: Download corpus (from previous runs)
        uses: actions/cache@v3
        with:
          path: fuzz/corpus_${{ matrix.target }}/
          key: corpus-${{ matrix.target }}-${{ github.sha }}
          restore-keys: corpus-${{ matrix.target }}-

      - name: Run fuzzer (5 minutes)
        run: |
          mkdir -p fuzz/corpus_${{ matrix.target }}
          ./${{ matrix.target }} \
            fuzz/corpus_${{ matrix.target }}/ \
            -dict=fuzz/dict/format.dict \
            -max_total_time=300 \
            -print_final_stats=1

      - name: Merge corpus
        run: |
          mkdir -p fuzz/corpus_min
          ./${{ matrix.target }} -merge=1 \
            fuzz/corpus_min/ \
            fuzz/corpus_${{ matrix.target }}/
          rm -rf fuzz/corpus_${{ matrix.target }}
          mv fuzz/corpus_min fuzz/corpus_${{ matrix.target }}

      - name: Check for crashes
        run: |
          if ls crash-* timeout-* oom-* 2>/dev/null; then
            echo "::error::Fuzzer found crashes!"
            for f in crash-* timeout-* oom-*; do
              echo "=== $f ==="
              xxd "$f" | head -20
            done
            exit 1
          fi

      - name: Upload crash artifacts
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: fuzzer-crashes-${{ matrix.target }}
          path: |
            crash-*
            timeout-*
            oom-*
```

### Regression testing en CI

```yaml
  regression:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build fuzzer
        run: make fuzz_parse
      - name: Run regression tests
        run: |
          for f in tests/regression/crash_*; do
            echo "Testing $f..."
            ./fuzz_parse "$f" || exit 1
          done
          echo "All regression tests passed."
```

---

## 23. Regression testing con el corpus

El corpus generado por el fuzzer se puede reusar como tests de regresión.

### Ejecutar el corpus como test suite

```bash
# -runs=0 significa: ejecutar cada archivo del corpus UNA vez, sin fuzzear
./fuzz_target corpus/ -runs=0

# Si algún archivo crashea, libFuzzer lo reporta y sale con error.
# Esto es equivalente a correr un test case por cada archivo del corpus.

# Útil en CI para verificar que no se introdujeron regresiones
```

### Convertir un crash en test permanente

```bash
# 1. Minimizar el crash
./fuzz_target -minimize_crash=1 -exact_artifact_path=tests/regression/crash_001 crash-hash

# 2. Verificar que el crash se reproduce
./fuzz_target tests/regression/crash_001
# Debe crashear

# 3. Arreglar el bug en el código

# 4. Verificar que el crash ya no se reproduce
./fuzz_target tests/regression/crash_001
# No debe crashear

# 5. Commitear el crash file al repositorio
git add tests/regression/crash_001
git commit -m "Add regression test for buffer overflow in parse()"

# 6. En CI: verificar todos los regression tests
./fuzz_target tests/regression/ -runs=0
```

### Relación corpus → unit tests

```
┌──────────────────────────────────────────────────────────────────────┐
│         Corpus como tests                                            │
│                                                                      │
│  Corpus de fuzzing:                                                  │
│  • Generado automáticamente                                         │
│  • Cientos/miles de inputs                                          │
│  • Cubre muchos edges del código                                    │
│  • Solo verifica "no crashea"                                        │
│  • Se ejecuta con: ./fuzz_target corpus/ -runs=0                    │
│                                                                      │
│  Unit tests:                                                         │
│  • Escritos manualmente                                             │
│  • Decenas de inputs                                                 │
│  • Verifican corrección (assert)                                    │
│  • Más lentos de escribir pero más precisos                         │
│                                                                      │
│  Regression tests (crashes):                                         │
│  • Derivados de crashes del fuzzer                                  │
│  • Archivos binarios en tests/regression/                           │
│  • Verifican que bugs corregidos no reaparecen                      │
│  • Se ejecutan con: ./fuzz_target tests/regression/ -runs=0         │
│                                                                      │
│  Las tres capas son complementarias:                                 │
│  Unit tests: corrección funcional                                   │
│  Corpus tests: robustez (no crashea)                                │
│  Regression tests: no-regresión de bugs encontrados                 │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 24. Comparación detallada con AFL++

| Aspecto | libFuzzer | AFL++ |
|---|---|---|
| **Modelo** | In-process (librería) | Fork-based (proceso externo) |
| **Punto de entrada** | `LLVMFuzzerTestOneInput()` | Programa completo (main con archivo/stdin) |
| **Compilador** | Solo Clang | Clang o GCC |
| **Velocidad base** | 10K-100K exec/s | 1K-3K exec/s (fork), 5K-50K (persistent) |
| **Dashboard** | Texto plano (stdout) | TUI interactiva (ncurses) |
| **Crash recovery** | El proceso muere, guarda crash y sale | El hijo muere, el padre continúa |
| **Memory leaks** | Detecta (LeakSanitizer) | No detecta (fork limpia memoria) |
| **Custom mutator** | `LLVMFuzzerCustomMutator()` | Custom mutator API + Python module |
| **Comparaciones** | `-use_value_profile=1` | CmpLog (`-c`) |
| **Minimizar test** | `-minimize_crash=1` | `afl-tmin` |
| **Minimizar corpus** | `-merge=1` | `afl-cmin` |
| **Paralelismo** | `-fork=N` | `-M/-S` (múltiples instancias) |
| **Binary-only** | No | QEMU, Frida, Unicorn |
| **Diccionario** | `-dict=file` | `-x file` o autodictionary (LTO) |
| **Setup** | Una función + compilar | Compilar target + preparar archivos |
| **CI/CD** | Fácil (un binario) | Más complejo (script wrapper) |
| **Rust integration** | cargo-fuzz (backend) | Posible pero no nativo |
| **Go integration** | No | No (Go tiene su fuzzer nativo) |
| **OSS-Fuzz** | Backend principal | Soportado también |

### Cuándo elegir libFuzzer sobre AFL++

```
  Elige libFuzzer cuando:
  ✓ Fuzzeas una función o API de librería (no un programa completo)
  ✓ Necesitas máxima velocidad (sin overhead de fork)
  ✓ Quieres detectar memory leaks
  ✓ Estás fuzzeando para OSS-Fuzz
  ✓ Quieres un setup simple (una función + compilar)
  ✓ Vas a fuzzear Rust (cargo-fuzz usa libFuzzer)

  Elige AFL++ cuando:
  ✓ Fuzzeas un programa completo (lee archivo/stdin)
  ✓ Necesitas binary-only fuzzing (sin código fuente)
  ✓ Quieres CmpLog para comparaciones complejas
  ✓ Prefieres monitoreo visual (dashboard TUI)
  ✓ Necesitas fuzzing paralelo sofisticado (-M/-S con schedules)
  ✓ El target tiene state leakage (fork lo aísla)
```

---

## 25. Errores comunes

### Error 1: "multiple definition of `main`"

```
❌ El código target tiene su propio main().
  -fsanitize=fuzzer linka libFuzzer que tiene su propio main().

✓ Soluciones:
  a) No compilar el archivo con main():
     clang -fsanitize=fuzzer harness.c parser.c  # no incluir main.c

  b) Compilación condicional:
     #ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
     int main(int argc, char *argv[]) { ... }
     #endif
     
     # Y compilar con:
     clang -fsanitize=fuzzer -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION ...
```

### Error 2: exec/s es 0 o muy bajo

```
❌ libFuzzer muestra exec/s: 0 o un número muy bajo.

✓ Causas:
  • El target es lento (hace I/O, sleep, red)
  • El target se cuelga en la primera ejecución
  • ASan causa mucho overhead en targets con mucha memoria

✓ Soluciones:
  • Verificar: time ./fuzz_target seed_file (debe ser < 10ms)
  • Reducir el scope del target (solo la función de parsing)
  • -timeout=2 (detectar hangs más rápido)
  • -rss_limit_mb=256 (detectar OOM más rápido)
```

### Error 3: false positives con LeakSanitizer

```
❌ libFuzzer reporta leaks en código que no es tuyo (libc, libstdc++).

✓ Solución:
  # Desactivar leak detection
  ./fuzz_target corpus/ -detect_leaks=0

  # O suprimir leaks específicos
  cat > lsan_suppressions.txt << 'EOF'
  leak:libstdc++
  leak:libc
  EOF
  LSAN_OPTIONS=suppressions=lsan_suppressions.txt ./fuzz_target corpus/
```

### Error 4: el corpus crece sin control

```
❌ Después de horas, el corpus tiene 100K+ archivos.

✓ Solución:
  # Merge periódico
  mkdir corpus_new
  ./fuzz_target -merge=1 corpus_new/ corpus/
  rm -rf corpus
  mv corpus_new corpus
```

### Error 5: compilar con GCC

```
❌ gcc -fsanitize=fuzzer -o fuzz harness.c
   error: unrecognized argument to '-fsanitize=' option: 'fuzzer'

✓ Solución:
  libFuzzer solo funciona con Clang.
  Instalar clang: sudo apt install clang
  Usar: clang -fsanitize=fuzzer ...
```

### Error 6: no usar -g (debug symbols)

```
❌ El crash report muestra:
   #0 0x4f1234 in ??? (fuzz_target+0x4f1234)
   No dice qué función ni qué línea.

✓ Solución:
  SIEMPRE compilar con -g:
  clang -fsanitize=fuzzer,address -g -o fuzz_target ...
  
  Con -g:
  #0 0x4f1234 in parse_field target.c:47:9
```

### Error 7: no guardar crashes como regression tests

```
❌ Encontraste crashes, arreglaste los bugs, borraste los archivos.
   Meses después, un refactor reintroduce el mismo bug.

✓ Solución:
  Para cada crash:
  1. Minimizar: ./fuzz_target -minimize_crash=1 -exact_artifact_path=tests/regression/crash_N crash-hash
  2. Arreglar el bug
  3. Verificar: ./fuzz_target tests/regression/crash_N
  4. Commitear: git add tests/regression/crash_N
  5. En CI: ./fuzz_target tests/regression/ -runs=0
```

---

## 26. Ejemplo completo: fuzzear un parser de expresiones

### El parser

```c
/* expr_parser.h */
#ifndef EXPR_PARSER_H
#define EXPR_PARSER_H

typedef enum {
    EXPR_NUM,
    EXPR_ADD,
    EXPR_SUB,
    EXPR_MUL,
    EXPR_DIV,
} ExprType;

typedef struct Expr Expr;
struct Expr {
    ExprType type;
    union {
        double number;      /* EXPR_NUM */
        struct {
            Expr *left;
            Expr *right;
        } binop;            /* EXPR_ADD, SUB, MUL, DIV */
    };
};

/* Parsea una expresión como "2 + 3 * (4 - 1)".
 * Retorna NULL si hay error de sintaxis.
 * El llamador debe liberar con expr_free(). */
Expr *expr_parse(const char *input, int len);

/* Evalúa una expresión y retorna el resultado. */
double expr_eval(const Expr *expr);

/* Libera una expresión y todos sus nodos. */
void expr_free(Expr *expr);

#endif
```

```c
/* expr_parser.c */
#include "expr_parser.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

typedef struct {
    const char *input;
    int pos;
    int len;
} Parser;

static char peek(Parser *p) {
    while (p->pos < p->len && isspace((unsigned char)p->input[p->pos]))
        p->pos++;
    if (p->pos >= p->len) return '\0';
    return p->input[p->pos];
}

static char advance(Parser *p) {
    char c = peek(p);
    if (c != '\0') p->pos++;
    return c;
}

static Expr *make_num(double val) {
    Expr *e = malloc(sizeof(Expr));
    if (!e) return NULL;
    e->type = EXPR_NUM;
    e->number = val;
    return e;
}

static Expr *make_binop(ExprType type, Expr *left, Expr *right) {
    Expr *e = malloc(sizeof(Expr));
    if (!e) {
        expr_free(left);
        expr_free(right);
        return NULL;
    }
    e->type = type;
    e->binop.left = left;
    e->binop.right = right;
    return e;
}

/* Forward declarations */
static Expr *parse_expr(Parser *p);
static Expr *parse_term(Parser *p);
static Expr *parse_factor(Parser *p);

static Expr *parse_factor(Parser *p) {
    char c = peek(p);

    if (c == '(') {
        advance(p);  /* consume '(' */
        Expr *inner = parse_expr(p);
        if (!inner) return NULL;
        if (peek(p) != ')') {
            expr_free(inner);
            return NULL;  /* missing ')' */
        }
        advance(p);  /* consume ')' */
        return inner;
    }

    if (c == '-') {
        advance(p);
        Expr *operand = parse_factor(p);
        if (!operand) return NULL;
        return make_binop(EXPR_SUB, make_num(0), operand);
    }

    if (isdigit((unsigned char)c) || c == '.') {
        double val = 0;
        int has_dot = 0;
        double frac = 0.1;

        while (p->pos < p->len) {
            c = p->input[p->pos];
            if (isdigit((unsigned char)c)) {
                if (has_dot) {
                    val += (c - '0') * frac;
                    frac *= 0.1;
                } else {
                    val = val * 10 + (c - '0');
                }
                p->pos++;
            } else if (c == '.' && !has_dot) {
                has_dot = 1;
                p->pos++;
            } else {
                break;
            }
        }
        return make_num(val);
    }

    return NULL;  /* unexpected character */
}

static Expr *parse_term(Parser *p) {
    Expr *left = parse_factor(p);
    if (!left) return NULL;

    while (1) {
        char c = peek(p);
        if (c == '*') {
            advance(p);
            Expr *right = parse_factor(p);
            if (!right) { expr_free(left); return NULL; }
            left = make_binop(EXPR_MUL, left, right);
        } else if (c == '/') {
            advance(p);
            Expr *right = parse_factor(p);
            if (!right) { expr_free(left); return NULL; }
            left = make_binop(EXPR_DIV, left, right);
        } else {
            break;
        }
    }
    return left;
}

static Expr *parse_expr(Parser *p) {
    Expr *left = parse_term(p);
    if (!left) return NULL;

    while (1) {
        char c = peek(p);
        if (c == '+') {
            advance(p);
            Expr *right = parse_term(p);
            if (!right) { expr_free(left); return NULL; }
            left = make_binop(EXPR_ADD, left, right);
        } else if (c == '-') {
            advance(p);
            Expr *right = parse_term(p);
            if (!right) { expr_free(left); return NULL; }
            left = make_binop(EXPR_SUB, left, right);
        } else {
            break;
        }
    }
    return left;
}

Expr *expr_parse(const char *input, int len) {
    Parser p = { .input = input, .pos = 0, .len = len };
    Expr *result = parse_expr(&p);

    /* Verificar que consumimos todo el input */
    if (result && peek(&p) != '\0') {
        expr_free(result);
        return NULL;
    }
    return result;
}

double expr_eval(const Expr *expr) {
    if (!expr) return 0.0;

    switch (expr->type) {
    case EXPR_NUM:
        return expr->number;
    case EXPR_ADD:
        return expr_eval(expr->binop.left) + expr_eval(expr->binop.right);
    case EXPR_SUB:
        return expr_eval(expr->binop.left) - expr_eval(expr->binop.right);
    case EXPR_MUL:
        return expr_eval(expr->binop.left) * expr_eval(expr->binop.right);
    case EXPR_DIV: {
        double divisor = expr_eval(expr->binop.right);
        if (divisor == 0.0) return 0.0;  /* evitar div by zero */
        return expr_eval(expr->binop.left) / divisor;
    }
    default:
        return 0.0;
    }
}

void expr_free(Expr *expr) {
    if (!expr) return;
    switch (expr->type) {
    case EXPR_ADD:
    case EXPR_SUB:
    case EXPR_MUL:
    case EXPR_DIV:
        expr_free(expr->binop.left);
        expr_free(expr->binop.right);
        break;
    case EXPR_NUM:
        break;
    }
    free(expr);
}
```

### El harness

```c
/* fuzz_expr.c */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "expr_parser.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Limitar tamaño para evitar recursión excesiva */
    if (size > 256) return 0;

    /* Crear string null-terminated */
    char *str = malloc(size + 1);
    if (!str) return 0;
    memcpy(str, data, size);
    str[size] = '\0';

    /* Parsear */
    Expr *expr = expr_parse(str, size);

    if (expr) {
        /* Evaluar */
        double result = expr_eval(expr);

        /* Verificar que el resultado no es NaN ni infinito */
        /* (esto es un assert del harness, no del código) */
        (void)result;  /* no hace nada, pero podríamos verificar propiedades */

        /* Liberar */
        expr_free(expr);
    }

    free(str);
    return 0;
}
```

### Diccionario

```
# expr.dict
digit_0="0"
digit_1="1"
digit_9="9"
plus="+"
minus="-"
star="*"
slash="/"
paren_open="("
paren_close=")"
dot="."
space=" "
big_num="99999999999999"
neg_num="-1"
zero="0.0"
nested="((("
```

### Corpus inicial

```bash
mkdir corpus

echo -n '1' > corpus/single_num
echo -n '2+3' > corpus/simple_add
echo -n '2*3+4' > corpus/precedence
echo -n '(2+3)*4' > corpus/parens
echo -n '-5' > corpus/negative
echo -n '1.5+2.5' > corpus/decimals
echo -n '10/3' > corpus/division
```

### Compilar y ejecutar

```bash
# Compilar
clang -fsanitize=fuzzer,address,undefined -g -O1 \
    -o fuzz_expr fuzz_expr.c expr_parser.c

# Ejecutar (5 minutos)
./fuzz_expr corpus/ -dict=expr.dict -max_total_time=300

# Si encuentra crashes:
# artifact_prefix='./'; Test unit written to ./crash-<hash>
```

### Bugs potenciales que el fuzzer puede encontrar

```
1. STACK OVERFLOW por recursión excesiva:
   Input: "((((((((((((((((((((((((1))))))))))))))))))))))))))"
   parse_factor → parse_expr → parse_term → parse_factor → ...
   El parser no tiene límite de profundidad de recursión.
   
   Fix: agregar contador de profundidad
   static Expr *parse_factor(Parser *p) {
       if (p->depth > MAX_DEPTH) return NULL;
       p->depth++;
       ...
       p->depth--;
   }

2. OOM por input con muchos operadores:
   Input: "1+1+1+1+1+...+1" (miles de sumas)
   Cada + crea un nodo Expr en heap.
   Con 10K sumas: 10K mallocs × ~24 bytes = ~240KB.
   No es un crash, pero con inputs más grandes puede ser.

3. UNDEFINED BEHAVIOR: signed integer overflow en val * 10:
   Input: "99999999999999999999999999999"
   val = val * 10 + digit → overflow de double → INF
   Con UBSan: detecta overflow si usáramos int en vez de double.

4. DIVIDE BY ZERO: ya manejado (retorna 0.0)
   Pero el fuzzer verificará que el manejo es correcto.
```

### Crear regression test

```bash
# Si encontró stack overflow:
./fuzz_expr -minimize_crash=1 \
    -exact_artifact_path=tests/regression/crash_recursion \
    crash-<hash>

# Verificar el contenido minimizado
cat tests/regression/crash_recursion
# Probablemente: "((((((((((((((1))))))))))))))" o similar
```

---

## 27. Programa de práctica: base64_codec

Construye un codec Base64 (encode + decode) y fuzzéalo con libFuzzer.

### Especificación

```
base64_codec/
├── Makefile
├── base64.h            ← API: encode y decode
├── base64.c            ← implementación
├── fuzz_decode.c       ← harness 1: fuzzear decode
├── fuzz_roundtrip.c    ← harness 2: encode(decode(x)) == x?
├── corpus_decode/      ← semillas para decode
├── corpus_roundtrip/   ← semillas para roundtrip
├── base64.dict         ← diccionario
└── tests/
    ├── test_base64.c   ← unit tests
    └── regression/     ← crashes como tests
```

### base64.h

```c
#ifndef BASE64_H
#define BASE64_H

#include <stddef.h>

/* Encode binary data to Base64 string.
 * out_buf must be at least ((in_len + 2) / 3) * 4 + 1 bytes.
 * Returns the length of the encoded string (without null terminator).
 * Returns -1 on error. */
int base64_encode(const unsigned char *in, size_t in_len,
                  char *out, size_t out_buf_size);

/* Decode Base64 string to binary data.
 * out_buf must be at least (in_len / 4) * 3 bytes.
 * Returns the length of the decoded data.
 * Returns -1 on error (invalid Base64). */
int base64_decode(const char *in, size_t in_len,
                  unsigned char *out, size_t out_buf_size);

#endif
```

### Tareas

1. **Implementa `base64.c`** con soporte para:
   - Caracteres estándar: A-Z, a-z, 0-9, +, /
   - Padding con `=`
   - Whitespace ignorado durante decode
   - Introduce intencionalmente 2-3 bugs:
     - Buffer overflow si el output buffer es demasiado pequeño
     - Off-by-one en el cálculo del padding
     - Aceptar caracteres inválidos sin reportar error

2. **Escribe `fuzz_decode.c`**:
   ```c
   int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
       // Decodificar los bytes del fuzzer como si fueran Base64
       // Verificar que no crashea
   }
   ```

3. **Escribe `fuzz_roundtrip.c`**:
   ```c
   int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
       // Encode los bytes del fuzzer
       // Decode el resultado
       // Verificar que el decode == input original (propiedad!)
       // Si no coincide → abort() (bug detectado por el fuzzer)
   }
   ```

4. **Prepara semillas** para decode:
   - `""` (vacío)
   - `"aGVsbG8="` ("hello")
   - `"AQID"` (bytes 1, 2, 3)
   - `"////"`
   - `"====" `(solo padding)

5. **Escribe `base64.dict`** con tokens relevantes

6. **Compila y fuzzea**:
   ```bash
   clang -fsanitize=fuzzer,address,undefined -g -o fuzz_decode fuzz_decode.c base64.c
   clang -fsanitize=fuzzer,address,undefined -g -o fuzz_roundtrip fuzz_roundtrip.c base64.c
   
   ./fuzz_decode corpus_decode/ -dict=base64.dict -max_total_time=300
   ./fuzz_roundtrip corpus_roundtrip/ -max_total_time=300
   ```

7. **El harness de roundtrip** debería encontrar bugs incluso sin ASan — si
   `decode(encode(x)) != x`, el `abort()` en el harness lo reporta.

8. **Documenta los crashes**, minimiza, corrige, y crea regression tests.

---

## 28. Ejercicios

### Ejercicio 1: primer fuzzer con libFuzzer

**Objetivo**: Completar el ciclo completo de fuzzing con libFuzzer.

**Tareas**:

**a)** Toma esta función con un bug:
   ```c
   void parse_tag(const char *data, size_t len) {
       if (len < 1 || data[0] != '<') return;
       
       char tag_name[32];
       size_t i = 1;
       size_t j = 0;
       
       while (i < len && data[i] != '>' && data[i] != ' ') {
           tag_name[j++] = data[i++];  /* bug: no verifica j < 32 */
       }
       tag_name[j] = '\0';
       
       /* ... procesar tag_name ... */
   }
   ```

   Escribe un harness `LLVMFuzzerTestOneInput` para esta función.

**b)** Compila con `clang -fsanitize=fuzzer,address -g` y ejecuta.
   - ¿Cuántos segundos tarda en encontrar el crash?
   - ¿Qué tipo de bug reporta ASan?
   - ¿Cuál es el input que causa el crash?

**c)** Minimiza el crash con `-minimize_crash=1`.
   - ¿De cuántos bytes se redujo?
   - ¿Cuál es el contenido del crash minimizado?

**d)** Arregla el bug y verifica que el crash minimizado ya no crashea.

---

### Ejercicio 2: roundtrip fuzzing

**Objetivo**: Usar fuzzing para verificar propiedades (encode/decode roundtrip).

**Tareas**:

**a)** Implementa funciones simples de hex encode/decode:
   ```c
   // hex_encode: bytes → "4F6B" (2 chars por byte)
   int hex_encode(const uint8_t *in, size_t in_len, char *out, size_t out_size);
   
   // hex_decode: "4F6B" → bytes (2 chars por byte)
   int hex_decode(const char *in, size_t in_len, uint8_t *out, size_t out_size);
   ```

**b)** Escribe un harness de roundtrip que verifique:
   ```c
   int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
       // 1. encode(data) → hex_string
       // 2. decode(hex_string) → decoded
       // 3. assert: decoded == data (memcmp)
       // Si no son iguales → abort()
   }
   ```

**c)** Introduce un bug intencional en hex_decode (por ejemplo, confundir
mayúsculas con minúsculas en 'A'-'F'). Fuzzea y verifica que el harness
de roundtrip lo encuentra.

**d)** ¿Cuánto tiempo tarda el fuzzer en encontrar el bug del roundtrip
vs un bug de crash (buffer overflow)? ¿Por qué la diferencia?

---

### Ejercicio 3: custom mutator

**Objetivo**: Escribir un mutador custom para un formato con estructura.

**Tareas**:

**a)** Escribe un parser para un protocolo simple:
   ```
   Formato: [2 bytes: longitud][N bytes: payload][4 bytes: checksum CRC32]
   ```

**b)** Sin custom mutator, fuzzea el parser 5 minutos. ¿Cuánta cobertura
alcanza? El fuzzer probablemente nunca pasará la validación de checksum.

**c)** Escribe un `LLVMFuzzerCustomMutator` que:
   1. Mute solo el payload (no el header de longitud ni el checksum)
   2. Recalcule la longitud en el header
   3. Recalcule el checksum CRC32 al final

**d)** Con el custom mutator, fuzzea 5 minutos. Compara:
   - ¿Cuánta más cobertura alcanza?
   - ¿Encuentra crashes que antes no encontraba?

---

### Ejercicio 4: libFuzzer en CI

**Objetivo**: Configurar fuzzing continuo con GitHub Actions.

**Tareas**:

**a)** Toma el parser de expresiones (sección 26) y configura:
   1. Un workflow que corra el fuzzer 5 minutos en cada push a main
   2. Cache del corpus entre runs (para que crezca con el tiempo)
   3. Upload de crashes como artefactos si se encuentran
   4. Regression testing del corpus existente en cada PR

**b)** Escribe el archivo `.github/workflows/fuzz.yml` completo.

**c)** Verifica que el workflow:
   - Falla si el fuzzer encuentra un crash
   - Pasa si no hay crashes
   - El corpus se preserva entre runs (cache hit en la segunda ejecución)

**d)** Después de 5 runs exitosos, haz merge del corpus:
   ```bash
   ./fuzz_expr -merge=1 corpus_min/ corpus/
   ```
   ¿Cuántos archivos se eliminaron? ¿La cobertura se mantuvo?

---

## Navegación

- **Anterior**: [T02 - AFL++](../T02_AFL/README.md)
- **Siguiente**: [T04 - Escribir harnesses](../T04_Escribir_harnesses/README.md)

---

> **Sección 1: Fuzzing en C** — Tópico 3 de 4 completado
>
> - T01: Concepto de fuzzing ✓
> - T02: AFL++ ✓
> - T03: libFuzzer ✓
> - T04: Escribir harnesses (siguiente)
