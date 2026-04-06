# T02 - AFL++: el fuzzer de referencia para C

## Índice

1. [Qué es AFL++](#1-qué-es-afl)
2. [Arquitectura de AFL++](#2-arquitectura-de-afl)
3. [Instalación](#3-instalación)
4. [Verificar la instalación](#4-verificar-la-instalación)
5. [afl-cc: el compilador instrumentador](#5-afl-cc-el-compilador-instrumentador)
6. [Modos de instrumentación de afl-cc](#6-modos-de-instrumentación-de-afl-cc)
7. [Compilar un target con afl-cc](#7-compilar-un-target-con-afl-cc)
8. [afl-fuzz: el motor del fuzzer](#8-afl-fuzz-el-motor-del-fuzzer)
9. [Flags esenciales de afl-fuzz](#9-flags-esenciales-de-afl-fuzz)
10. [Preparar el corpus inicial](#10-preparar-el-corpus-inicial)
11. [Primera sesión de fuzzing](#11-primera-sesión-de-fuzzing)
12. [El dashboard de AFL++](#12-el-dashboard-de-afl)
13. [Interpretar cada sección del dashboard](#13-interpretar-cada-sección-del-dashboard)
14. [Estructura del directorio de output](#14-estructura-del-directorio-de-output)
15. [Interpretar crashes](#15-interpretar-crashes)
16. [afl-tmin: minimizar test cases](#16-afl-tmin-minimizar-test-cases)
17. [afl-cmin: minimizar el corpus](#17-afl-cmin-minimizar-el-corpus)
18. [Diccionarios en AFL++](#18-diccionarios-en-afl)
19. [Modo persistente: velocidad 10x](#19-modo-persistente-velocidad-10x)
20. [Modo deferred fork server](#20-modo-deferred-fork-server)
21. [Fuzzing en paralelo: múltiples instancias](#21-fuzzing-en-paralelo-múltiples-instancias)
22. [AFL++ con sanitizers](#22-afl-con-sanitizers)
23. [CmpLog: resolver comparaciones complejas](#23-cmplog-resolver-comparaciones-complejas)
24. [Power schedules: estrategias de mutación](#24-power-schedules-estrategias-de-mutación)
25. [afl-plot: visualizar el progreso](#25-afl-plot-visualizar-el-progreso)
26. [afl-whatsup: monitoreo de múltiples instancias](#26-afl-whatsup-monitoreo-de-múltiples-instancias)
27. [Comparación con libFuzzer y Honggfuzz](#27-comparación-con-libfuzzer-y-honggfuzz)
28. [Errores comunes](#28-errores-comunes)
29. [Ejemplo completo: fuzzear un parser INI](#29-ejemplo-completo-fuzzear-un-parser-ini)
30. [Programa de práctica: url_parser](#30-programa-de-práctica-url_parser)
31. [Ejercicios](#31-ejercicios)

---

## 1. Qué es AFL++

AFL++ (American Fuzzy Lop Plus Plus) es un fork mejorado del AFL original creado
por Michał Zalewski. Es el fuzzer coverage-guided más usado del mundo para C y C++.

```
┌──────────────────────────────────────────────────────────────────────┐
│                    AFL++ en contexto                                 │
│                                                                      │
│  2007: AFL original (Michał Zalewski / Google)                      │
│  │     • Inventó el coverage-guided fuzzing práctico                │
│  │     • Encontró bugs en libpng, libjpeg, bash, openssl, ...       │
│  │     • Último release: 2.57b (2017, proyecto pausado)             │
│  │                                                                   │
│  2019: AFL++ (fork por la comunidad)                                │
│        • Mantenido activamente                                       │
│        • Instrumentación LLVM moderna (pcguard, ctx, ngram)         │
│        • CmpLog para comparaciones complejas                        │
│        • Power schedules avanzados                                   │
│        • Modo persistente mejorado                                   │
│        • QEMU mode actualizado                                       │
│        • Frida mode (binary fuzzing sin recompilación)               │
│        • Es el estándar de facto para fuzzing de C/C++              │
│                                                                      │
│  AFL++ es el fuzzer recomendado por:                                │
│  • Google Project Zero                                               │
│  • LLVM project                                                     │
│  • OSS-Fuzz (lo usa internamente)                                   │
│  • La mayoría de CTFs y auditorías de seguridad                     │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 2. Arquitectura de AFL++

```
┌──────────────────────────────────────────────────────────────────────┐
│                  Arquitectura de AFL++                                │
│                                                                      │
│  ┌──────────┐    ┌──────────────┐    ┌──────────────────┐           │
│  │ Corpus   │───▶│  afl-fuzz    │───▶│ Target program   │           │
│  │ (seeds)  │    │  (motor)     │    │ (instrumentado)  │           │
│  └──────────┘    │              │    │                  │           │
│       ▲          │  Mutador     │    │  ┌────────────┐  │           │
│       │          │  Scheduler   │    │  │ Shared mem │  │           │
│       │          │  Evaluador   │    │  │ (bitmap)   │──┼─────┐    │
│       │          │              │    │  └────────────┘  │     │    │
│       │          └──────┬───────┘    └──────────────────┘     │    │
│       │                 │                                     │    │
│       │                 │         ┌───────────────────────┐   │    │
│       │                 │         │  Coverage bitmap      │◀──┘    │
│       │                 ▼         │  (65536 bytes)        │        │
│       │          ┌──────────┐    │  Cada byte = un edge  │        │
│       │          │ ¿Nuevo   │◀───│  del control flow     │        │
│       │          │  edge?   │    └───────────────────────┘        │
│       │          └────┬─────┘                                     │
│       │          Sí   │   No                                      │
│       │          │    │                                            │
│       │          ▼    ▼                                            │
│       └────── Guardar Descartar                                   │
│                en corpus                                          │
│                                                                      │
│  Flujo:                                                              │
│  1. afl-fuzz elige un input del corpus                              │
│  2. Lo muta (bit flips, arithmetic, havoc, etc.)                    │
│  3. Ejecuta el target con fork server                                │
│  4. Lee el coverage bitmap de shared memory                          │
│  5. Si descubrió edges nuevos → agrega al corpus                    │
│  6. Si crasheó → guarda en crashes/                                 │
│  7. Repite                                                           │
└──────────────────────────────────────────────────────────────────────┘
```

### El fork server

Una innovación clave de AFL: en lugar de `fork + exec` para cada input (lento),
AFL usa un **fork server**:

```
┌──────────────────────────────────────────────────────────────────────┐
│              Fork server de AFL                                      │
│                                                                      │
│  Sin fork server (lento):                                           │
│  Para cada input:                                                    │
│    fork() → exec(target) → inicializar todo → procesar → exit       │
│    El exec y la inicialización se repiten cada vez.                  │
│    Overhead: ~5ms por ejecución típica                               │
│                                                                      │
│  Con fork server (rápido):                                           │
│  UNA VEZ:                                                            │
│    exec(target) → inicializar todo → PARAR en el fork server        │
│                                                                      │
│  Para cada input:                                                    │
│    fork() → procesar → exit (hijo)                                   │
│    El padre espera y envía el siguiente input.                       │
│    No hay exec() ni re-inicialización.                               │
│    Overhead: ~1ms por ejecución                                      │
│                                                                      │
│  Speedup: 3-10x respecto a fork+exec por cada input.                │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 3. Instalación

### Desde paquete del sistema (más fácil)

```bash
# Fedora
sudo dnf install aflplusplus

# Ubuntu/Debian (puede ser una versión antigua)
sudo apt install afl++

# Arch Linux
sudo pacman -S afl++
```

### Compilar desde source (recomendado para última versión)

```bash
# Dependencias (Fedora)
sudo dnf install gcc gcc-c++ clang llvm-devel make python3 \
    cmake automake autoconf git

# Dependencias (Ubuntu/Debian)
sudo apt install build-essential clang llvm-dev python3 \
    cmake automake autoconf git

# Clonar y compilar
git clone https://github.com/AFLplusplus/AFLplusplus.git
cd AFLplusplus
make distrib  # compila todo: afl-cc, afl-fuzz, QEMU mode, etc.

# Instalar (opcional, o agregar al PATH)
sudo make install

# O simplemente agregar al PATH
export PATH=$PWD:$PATH
```

### Con Docker (entorno aislado)

```bash
# Imagen oficial
docker pull aflplusplus/aflplusplus

# Ejecutar con directorio compartido
docker run -ti -v $(pwd)/project:/project aflplusplus/aflplusplus

# Dentro del container, todas las herramientas están disponibles:
# afl-cc, afl-fuzz, afl-tmin, afl-cmin, afl-plot, etc.
```

---

## 4. Verificar la instalación

```bash
# Verificar que afl-cc existe
afl-cc --version
# afl-cc++4.09a by Michal Zalewski, Laszlo Szekeres, Marc Heuse - release

# Verificar que afl-fuzz existe
afl-fuzz --help 2>&1 | head -5
# afl-fuzz++4.09a based on afl by Michal Zalewski and target. ...

# Verificar herramientas auxiliares
afl-tmin --help 2>&1 | head -3
afl-cmin --help 2>&1 | head -3
afl-showmap --help 2>&1 | head -3

# Listar todas las herramientas AFL++
ls /usr/bin/afl-* 2>/dev/null || ls /usr/local/bin/afl-* 2>/dev/null
```

Herramientas principales de AFL++:

| Herramienta | Propósito |
|---|---|
| `afl-cc` | Compilador wrapper con instrumentación |
| `afl-c++` | Mismo que afl-cc para C++ |
| `afl-fuzz` | Motor del fuzzer |
| `afl-tmin` | Minimizar un test case individual |
| `afl-cmin` | Minimizar el corpus completo |
| `afl-showmap` | Mostrar el mapa de cobertura de un input |
| `afl-plot` | Generar gráficos del progreso |
| `afl-whatsup` | Estado de múltiples instancias en paralelo |
| `afl-gotcpu` | Verificar CPUs disponibles para fuzzing |
| `afl-analyze` | Analizar la estructura de un test case |

---

## 5. afl-cc: el compilador instrumentador

`afl-cc` es un wrapper de compilador que reemplaza a `gcc` o `clang`. Compila el
código normalmente pero **inyecta instrumentación** en cada edge del control flow.

### Uso básico

```bash
# En vez de:
gcc -o target target.c

# Usar:
afl-cc -o target target.c

# Para C++:
afl-c++ -o target target.cpp

# Con flags de compilación normales:
afl-cc -O2 -Wall -o target target.c library.c -lm

# Con Makefile: sustituir CC
make CC=afl-cc CXX=afl-c++

# Con CMake:
cmake -DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ ..
```

### Qué hace afl-cc internamente

```
┌──────────────────────────────────────────────────────────────────────┐
│              Qué hace afl-cc                                        │
│                                                                      │
│  tu_codigo.c                                                        │
│       │                                                              │
│       ▼                                                              │
│  ┌──────────────────────────────────┐                                │
│  │  afl-cc (wrapper)               │                                │
│  │                                  │                                │
│  │  1. Detecta el backend:          │                                │
│  │     clang/LLVM, GCC, o afl-gcc  │                                │
│  │                                  │                                │
│  │  2. Agrega flags de              │                                │
│  │     instrumentación              │                                │
│  │                                  │                                │
│  │  3. Linka con el runtime de      │                                │
│  │     AFL (fork server, shared     │                                │
│  │     memory bitmap)               │                                │
│  │                                  │                                │
│  │  4. Produce el binario           │                                │
│  │     instrumentado                │                                │
│  └──────────────────────────────────┘                                │
│       │                                                              │
│       ▼                                                              │
│  target (binario con instrumentación)                                │
│  • Cada branch tiene un contador                                     │
│  • Incluye fork server                                               │
│  • Se comunica con afl-fuzz via shared memory                        │
└──────────────────────────────────────────────────────────────────────┘
```

### Verificar la instrumentación

```bash
# Compilar un programa simple
echo 'int main() { return 0; }' > test.c
afl-cc -o test_instr test.c

# afl-cc imprime qué instrumentación usó:
# [+] Instrumented X locations (... mode, ratio 100%).

# Comparar tamaño: el instrumentado es más grande
gcc -o test_normal test.c
ls -la test_normal test_instr
# test_normal: ~16KB
# test_instr:  ~30KB (incluye fork server + instrumentación)
```

---

## 6. Modos de instrumentación de afl-cc

AFL++ soporta varios backends de instrumentación con diferentes trade-offs:

```
┌──────────────────────────────────────────────────────────────────────┐
│         Modos de instrumentación de AFL++                            │
│                                                                      │
│  Selección automática (afl-cc elige el mejor disponible):           │
│                                                                      │
│  LLVM PCGUARD (por defecto si clang/LLVM están disponibles)         │
│  ├── Más preciso y rápido                                           │
│  ├── Usa __sanitizer_cov_trace_pc_guard                             │
│  ├── Contadores de edges exactos                                    │
│  └── Recomendado para todo uso general                              │
│                                                                      │
│  LLVM CLASSIC                                                       │
│  ├── Instrumentación AFL clásica vía LLVM pass                      │
│  ├── Compatible con AFL original                                    │
│  └── Ligeramente menos preciso que PCGUARD                          │
│                                                                      │
│  GCC PLUGIN                                                         │
│  ├── Usa GCC plugin API                                             │
│  ├── Para cuando clang no está disponible                           │
│  └── Similar precisión a LLVM CLASSIC                               │
│                                                                      │
│  GCC (afl-gcc, legacy)                                              │
│  ├── Instrumentación a nivel de ensamblador                         │
│  ├── Más simple pero menos preciso                                  │
│  └── Fallback cuando no hay LLVM ni GCC plugin                     │
│                                                                      │
│  Forzar un modo específico:                                         │
│  AFL_CC_COMPILER=LTO afl-cc ...    (Link-Time Optimization)        │
│  AFL_CC_COMPILER=LLVM afl-cc ...   (LLVM PCGUARD)                  │
│  AFL_CC_COMPILER=GCC afl-cc ...    (GCC plugin)                    │
│  AFL_CC_COMPILER=GCC_PLUGIN ...    (explícito GCC plugin)          │
└──────────────────────────────────────────────────────────────────────┘
```

### LTO mode (Link-Time Optimization)

El modo LTO es el más avanzado. Instrumenta durante la fase de link, lo que permite
optimizaciones que no son posibles en otros modos:

```bash
# Compilar con LTO (requiere clang + lld)
AFL_CC_COMPILER=LTO afl-cc -o target target.c

# Ventajas del LTO:
# • Puede enumerar TODOS los edges del programa completo
# • No hay colisiones en el bitmap (cada edge tiene ID único)
# • Autodictionary: extrae tokens de comparaciones automáticamente
# • Es el modo más preciso
```

### Tabla de comparación

| Modo | Backend | Precisión | Velocidad | Requisito |
|---|---|---|---|---|
| PCGUARD | LLVM/Clang | Alta | Rápida | clang >= 6.0 |
| LTO | LLVM/Clang + lld | Muy alta | Rápida | clang + lld |
| CLASSIC | LLVM/Clang | Media | Rápida | clang |
| GCC PLUGIN | GCC | Media | Rápida | gcc + plugin |
| GCC (legacy) | GCC | Baja | Media | gcc |

**Recomendación**: usar PCGUARD (por defecto) o LTO si tienes `lld`.

---

## 7. Compilar un target con afl-cc

### Programa ejemplo

```c
/* example.c — programa simple que lee un archivo y lo procesa */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void process(const char *data, size_t len) {
    if (len < 4) return;

    if (data[0] == 'F' && data[1] == 'U' &&
        data[2] == 'Z' && data[3] == 'Z') {
        /* Ruta interesante */
        if (len > 8 && data[4] == '!') {
            /* Bug: buffer overflow */
            char buf[8];
            memcpy(buf, data, len);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *data = malloc(fsize);
    if (!data) {
        fclose(f);
        return 1;
    }

    fread(data, 1, fsize, f);
    fclose(f);

    process(data, fsize);

    free(data);
    return 0;
}
```

### Compilar

```bash
# Compilar con instrumentación
afl-cc -o example_fuzz example.c

# Verificar la instrumentación
# [+] Instrumented 12 locations (64-bit, PCGUARD mode, ratio 100%).

# Compilar con sanitizers (recomendado)
afl-cc -fsanitize=address,undefined -g -o example_fuzz_asan example.c
```

### Compilar proyectos con Makefile

```bash
# Si el proyecto usa make:
make CC=afl-cc clean all

# Con CFLAGS adicionales:
make CC=afl-cc CFLAGS="-fsanitize=address -g" clean all

# Si el proyecto usa configure:
CC=afl-cc ./configure
make
```

### Compilar proyectos con CMake

```bash
mkdir build && cd build
cmake -DCMAKE_C_COMPILER=afl-cc \
      -DCMAKE_CXX_COMPILER=afl-c++ \
      -DCMAKE_C_FLAGS="-fsanitize=address" \
      ..
make
```

---

## 8. afl-fuzz: el motor del fuzzer

`afl-fuzz` es el comando principal que ejecuta el fuzzing. Toma un corpus inicial,
muta los inputs, ejecuta el target y recolecta cobertura.

### Sintaxis básica

```bash
afl-fuzz -i <input_dir> -o <output_dir> [options] -- <target> [target_args]
```

### Anatomía del comando

```
afl-fuzz -i corpus_in -o corpus_out -- ./target @@
         ▲             ▲                ▲        ▲
         │             │                │        │
         │             │                │        └── @@ = AFL reemplaza
         │             │                │            con el nombre del
         │             │                │            archivo de input
         │             │                │
         │             │                └── programa target instrumentado
         │             │
         │             └── directorio donde AFL guarda resultados
         │                 (corpus, crashes, hangs, stats)
         │
         └── directorio con semillas iniciales
```

### Opciones de entrada del target

```bash
# El target lee de un ARCHIVO (más común):
# @@ se reemplaza con la ruta al archivo temporal con el input
afl-fuzz -i in -o out -- ./target @@

# El target lee de STDIN:
# Sin @@, AFL pasa el input por stdin
afl-fuzz -i in -o out -- ./target

# El target necesita argumentos adicionales:
afl-fuzz -i in -o out -- ./target --parse --strict @@
```

---

## 9. Flags esenciales de afl-fuzz

### Flags básicos

| Flag | Propósito | Ejemplo |
|---|---|---|
| `-i <dir>` | Directorio con semillas iniciales | `-i corpus/` |
| `-o <dir>` | Directorio de output | `-o output/` |
| `-t <ms>` | Timeout por ejecución (ms) | `-t 1000` (1 segundo) |
| `-m <MB>` | Límite de memoria (MB) | `-m 256` |
| `-x <dict>` | Archivo de diccionario | `-x json.dict` |
| `-d` | Skip deterministic stages (más rápido al inicio) | `-d` |

### Flags de control

| Flag | Propósito | Ejemplo |
|---|---|---|
| `-M <name>` | Instancia main (para fuzzing paralelo) | `-M main` |
| `-S <name>` | Instancia secondary | `-S secondary01` |
| `-p <schedule>` | Power schedule | `-p explore` |
| `-c <prog>` | Programa CmpLog (para comparaciones) | `-c ./target_cmplog` |
| `-l <level>` | Nivel de CmpLog (2 = recomendado) | `-l 2` |
| `-V <sec>` | Terminar después de N segundos | `-V 3600` (1 hora) |
| `-E <execs>` | Terminar después de N ejecuciones | `-E 1000000` |

### Flags de output

| Flag | Propósito | Ejemplo |
|---|---|---|
| `-s <seed>` | Semilla para el RNG (reproducibilidad) | `-s 42` |
| `-b <cpu>` | Fijar a un CPU específico | `-b 0` |

### Variables de entorno importantes

```bash
# Deshabilitar ASLR (mejora determinismo)
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space

# O usar AFL_NO_AFFINITY si no puedes cambiar la configuración del sistema
export AFL_NO_AFFINITY=1

# Skip la pantalla de disclaimer
export AFL_SKIP_CPUFREQ=1

# Limitar tamaño máximo de input generado
export AFL_MAX_DET_EXTRAS=200

# Habilitar modo de auto-detección de diccionario
export AFL_AUTORESUME=1

# Variables útiles para desarrollo:
export AFL_DEBUG=1             # muestra debug output
export AFL_NO_UI=1             # sin interfaz TUI (útil para CI)
export AFL_BENCH_UNTIL_CRASH=1 # para benchmarks
```

---

## 10. Preparar el corpus inicial

### Reglas del corpus para AFL++

```
┌──────────────────────────────────────────────────────────────────────┐
│            Reglas para el corpus inicial de AFL++                    │
│                                                                      │
│  1. PEQUEÑO                                                         │
│     Cada archivo de semilla debe ser lo más pequeño posible.         │
│     Ideal: < 1KB. Máximo recomendado: < 10KB.                      │
│     AFL es más eficiente mutando archivos pequeños.                  │
│                                                                      │
│  2. DIVERSO                                                         │
│     Incluir al menos un archivo que ejercite cada feature            │
│     del parser/protocolo. No 10 variantes de lo mismo.              │
│                                                                      │
│  3. VÁLIDO                                                          │
│     Las semillas deben ser inputs válidos que el programa            │
│     procese sin error. El fuzzer se encarga de romperlos.            │
│                                                                      │
│  4. DEDUPLICADO                                                     │
│     No incluir archivos que cubran los mismos caminos.              │
│     Usar afl-cmin si tienes muchos candidatos.                      │
│                                                                      │
│  5. MÍNIMO VIABLE                                                   │
│     3-10 archivos es suficiente para la mayoría de los targets.     │
│     Más no es necesariamente mejor.                                  │
└──────────────────────────────────────────────────────────────────────┘
```

### Ejemplo: preparar corpus para un parser de configuración

```bash
mkdir corpus_in

# Semilla 1: configuración mínima
cat > corpus_in/minimal.conf << 'EOF'
key=value
EOF

# Semilla 2: con sección
cat > corpus_in/section.conf << 'EOF'
[section]
key=value
EOF

# Semilla 3: con tipos de valores
cat > corpus_in/types.conf << 'EOF'
[db]
host=localhost
port=5432
enabled=true
timeout=30.5
name=my "db"
EOF

# Semilla 4: con comentarios y líneas vacías
cat > corpus_in/comments.conf << 'EOF'
# comment
[section]
; another comment

key=value
EOF

# Semilla 5: casos borde
cat > corpus_in/edge.conf << 'EOF'
=
[]=
key=
=value
EOF

# Verificar tamaños (todos < 1KB)
ls -la corpus_in/
```

### Usar afl-cmin si tienes muchas semillas

```bash
# Si tienes 500 archivos de ejemplo, reducir al mínimo:
afl-cmin -i all_seeds/ -o corpus_in/ -- ./target_fuzz @@

# afl-cmin ejecuta el target con cada archivo, registra la cobertura,
# y selecciona el subconjunto mínimo que mantiene la misma cobertura.
```

---

## 11. Primera sesión de fuzzing

### Paso a paso completo

```bash
# 1. Compilar con instrumentación y sanitizers
afl-cc -fsanitize=address,undefined -g -o target_fuzz target.c

# 2. Preparar corpus
mkdir -p corpus_in
echo "hello" > corpus_in/seed1
echo "test input" > corpus_in/seed2

# 3. Crear directorio de output
mkdir -p corpus_out

# 4. (Recomendado) Configurar el sistema
echo core | sudo tee /proc/sys/kernel/core_pattern
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# 5. Lanzar AFL++
afl-fuzz -i corpus_in -o corpus_out -- ./target_fuzz @@
```

### Problemas comunes al lanzar

```
❌ "Hmm, your system is configured to send core dump notifications to an external utility..."
✓ Solución: echo core | sudo tee /proc/sys/kernel/core_pattern

❌ "The target binary looks like a shell script..."
✓ Solución: compilar con afl-cc, no es un script

❌ "No instrumentation detected"
✓ Solución: verificar que compilaste con afl-cc, no con gcc

❌ "The input directory is empty"
✓ Solución: poner al menos un archivo en corpus_in/

❌ "Timeout while initializing fork server"
✓ Solución: el programa se cuelga al inicio, verificar que funciona:
   ./target_fuzz corpus_in/seed1
```

### Configuración del sistema para fuzzing óptimo

```bash
# Script de configuración del sistema (requiere root)
#!/bin/bash

# 1. Core pattern: AFL necesita acceso directo a cores
echo core | sudo tee /proc/sys/kernel/core_pattern

# 2. ASLR: deshabilitar para mejor determinismo
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space

# 3. CPU governor: performance mode para velocidad máxima
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance | sudo tee "$cpu"
done

# 4. (Opcional) Verificar que no hay otros fuzzers corriendo
afl-gotcpu
```

---

## 12. El dashboard de AFL++

Cuando `afl-fuzz` está corriendo, muestra un dashboard interactivo en la terminal:

```
┌──────────────────────────────────────────────────────────────────────┐
│               american fuzzy lop ++4.09a {default} (target_fuzz)    │
├─ process timing ────────────────────────────────────┬─ overall ─────┤
│        run time : 0 days, 1 hrs, 23 min, 45 sec    │  cycles done :│
│   last new find : 0 days, 0 hrs, 0 min, 32 sec     │           4   │
│ last saved crash : 0 days, 0 hrs, 15 min, 12 sec   │ corpus count :│
│  last saved hang : none yet                         │          247  │
├─ cycle progress ────────────────────┬─ map coverage ┤               │
│  now processing : 189.12 (76.5%)   │    map density : 3.42%        │
│  runs timed out : 0                │ count coverage : 5.18%        │
├─ stage progress ────────────────────┼─ findings ────┤               │
│  now trying : havoc                │ saved crashes :│               │
│ stage execs : 45.2k/131k (34.5%)  │            3   │               │
│ total execs : 5.24M                │  saved hangs :│               │
│  exec speed : 1,247/sec            │            0   │               │
├─ fuzzing strategy yields ──────────┴────────────────┤               │
│   bit flips : 12/3.2k, 4/3.2k, 2/3.1k             │               │
│  byte flips : 0/400, 1/397, 0/391                  │               │
│  arithmetics : 3/22.3k, 0/7.8k, 0/1.2k            │               │
│  known ints : 1/2.3k, 0/9.8k, 0/13.2k             │               │
│  dictionary : 0/0, 0/0, 2/412                      │               │
│havoc/splice : 224/2.8M, 8/192k                     │               │
│py/custom/rq : unused, unused, unused, unused        │               │
├─ item geometry ─────────────────────────────────────┤               │
│    levels : 7                                       │               │
│   pending : 58                                      │               │
│  pend fav : 12                                      │               │
│ own finds : 245                                     │               │
│  imported : 0                                       │               │
│ stability : 98.45%                                  │               │
└─────────────────────────────────────────────────────┘               │
```

---

## 13. Interpretar cada sección del dashboard

### Process timing

```
├─ process timing ──────────────────────────────────────────┤
│        run time : 0 days, 1 hrs, 23 min, 45 sec          │
│   last new find : 0 days, 0 hrs, 0 min, 32 sec           │
│ last saved crash : 0 days, 0 hrs, 15 min, 12 sec         │
│  last saved hang : none yet                               │
```

| Campo | Significado | Qué vigilar |
|---|---|---|
| `run time` | Tiempo total de fuzzing | Mínimo: hasta que cycles done >= 1 |
| `last new find` | Tiempo desde el último input interesante | Si > 1 hora → considerar parar |
| `last saved crash` | Tiempo desde el último crash único | Cada crash es un bug potencial |
| `last saved hang` | Tiempo desde el último timeout | Investigar si aparecen |

### Overall results

```
├─ overall results ─────────────────────────────────────────┤
│  cycles done : 4                                          │
│ corpus count : 247                                        │
│ saved crashes : 3                                         │
│  saved hangs : 0                                          │
```

| Campo | Significado | Valores buenos |
|---|---|---|
| `cycles done` | Veces que AFL recorrió todo el corpus | >= 1 mínimo, >= 5 confortable |
| `corpus count` | Total de inputs en la queue | Crece al inicio, se estabiliza |
| `saved crashes` | Crashes únicos (de-duplicados) | Cualquiera > 0 merece investigación |
| `saved hangs` | Timeouts únicos | Puede indicar complejidad algorítmica |

### Cycle progress

```
├─ cycle progress ──────────────────────────────────────────┤
│  now processing : 189.12 (76.5%)                          │
│  runs timed out : 0                                       │
```

- `now processing`: qué entrada del corpus está procesando (ID y %)
- `runs timed out`: ejecuciones que excedieron el timeout. Si > 0% del total,
  considerar aumentar `-t` o investigar el target

### Map coverage

```
├─ map coverage ────────────────────────────────────────────┤
│    map density : 3.42% / 5.18%                            │
│ count coverage : 5.18%                                    │
```

- `map density`: % del bitmap cubierto. El primer número es el promedio, el segundo
  es el máximo visto. Para programas reales, 2-10% es normal (el bitmap tiene 65536
  slots). Valores muy bajos (< 1%) sugieren poca exploración.
- `count coverage`: considera los hit counts, no solo sí/no

### Stage progress

```
├─ stage progress ──────────────────────────────────────────┤
│  now trying : havoc                                       │
│ stage execs : 45.2k/131k (34.5%)                          │
│ total execs : 5.24M                                       │
│  exec speed : 1,247/sec                                   │
```

| Campo | Significado | Valores buenos |
|---|---|---|
| `now trying` | Estrategia de mutación actual | havoc es la más productiva |
| `stage execs` | Progreso de la etapa actual | — |
| `total execs` | Total de ejecuciones desde el inicio | Cuantas más, mejor |
| `exec speed` | Ejecuciones por segundo | > 500/sec bueno, > 1000/sec excelente |

**exec speed** es la métrica más importante para la eficiencia:

| exec speed | Evaluación | Acción |
|---|---|---|
| > 1000/sec | Excelente | El target es rápido |
| 500-1000/sec | Bueno | Normal para programas medianos |
| 100-500/sec | Aceptable | Considerar modo persistente |
| 10-100/sec | Lento | Optimizar el harness |
| < 10/sec | Muy lento | Reducir el target, usar modo persistente |

### Fuzzing strategy yields

```
├─ fuzzing strategy yields ─────────────────────────────────┤
│   bit flips : 12/3.2k, 4/3.2k, 2/3.1k                   │
│  byte flips : 0/400, 1/397, 0/391                         │
│  arithmetics : 3/22.3k, 0/7.8k, 0/1.2k                   │
│  known ints : 1/2.3k, 0/9.8k, 0/13.2k                    │
│  dictionary : 0/0, 0/0, 2/412                             │
│havoc/splice : 224/2.8M, 8/192k                            │
```

Formato: `hallazgos / intentos` para cada tipo de mutación.

- `bit flips`: 1-bit, 2-bit, 4-bit flips
- `byte flips`: 1-byte, 2-byte, 4-byte flips
- `arithmetics`: sumar/restar 1-35 a bytes (8, 16, 32 bits)
- `known ints`: sustituir con valores "interesantes" (0, 1, -1, 0x7F, 0x80, etc.)
- `dictionary`: tokens del diccionario insertados/reemplazados
- `havoc/splice`: mutaciones combinadas aleatorias / splice de dos inputs

**havoc** es generalmente la estrategia más productiva a largo plazo.

### Item geometry

```
├─ item geometry ────────────────────────────────────────────┤
│    levels : 7                                              │
│   pending : 58                                             │
│  pend fav : 12                                             │
│ own finds : 245                                            │
│  imported : 0                                              │
│ stability : 98.45%                                         │
```

| Campo | Significado | Valores buenos |
|---|---|---|
| `levels` | Profundidad del "árbol" de mutaciones | Crece con el tiempo |
| `pending` | Inputs que aún no han sido procesados en este ciclo | Baja durante el ciclo |
| `pend fav` | Inputs "favoritos" pendientes (priorizados) | Baja rápidamente |
| `own finds` | Inputs descubiertos por esta instancia | Crece |
| `imported` | Inputs importados de otra instancia (paralelo) | > 0 si usas -M/-S |
| `stability` | Determinismo de la cobertura | > 90% bueno, < 80% problema |

---

## 14. Estructura del directorio de output

```
corpus_out/
└── default/                   ← nombre de la instancia
    ├── queue/                 ← EL CORPUS: inputs interesantes
    │   ├── id:000000,time:0,execs:0,orig:seed1
    │   ├── id:000001,time:0,execs:0,orig:seed2
    │   ├── id:000002,src:000000,time:1234,execs:5678,op:flip1,pos:3
    │   ├── id:000003,src:000001,time:2345,execs:12345,op:arith8,pos:7,val:+1
    │   ├── id:000004,src:000002,time:3456,execs:23456,op:havoc,rep:4
    │   └── ...
    │
    ├── crashes/               ← INPUTS QUE CAUSAN CRASHES
    │   ├── README.txt         ← instrucciones de AFL
    │   ├── id:000000,sig:11,src:000042,time:45678,execs:345678,op:havoc,rep:2
    │   ├── id:000001,sig:06,src:000123,time:78901,execs:567890,op:arith8,pos:4,val:+1
    │   └── ...
    │
    ├── hangs/                 ← INPUTS QUE CAUSAN TIMEOUT
    │   └── id:000000,src:000067,time:90123,execs:789012,op:havoc,rep:8
    │
    ├── fuzzer_stats            ← estadísticas (parseable)
    ├── plot_data               ← datos para afl-plot
    ├── cmdline                 ← comando de afl-fuzz usado
    ├── .synced/                ← sync entre instancias paralelas
    └── fuzz_bitmap             ← bitmap de cobertura acumulado
```

### Interpretar nombres de archivos

Los nombres de archivo en `queue/` y `crashes/` codifican información:

```
id:000042,sig:11,src:000023,time:45678,execs:345678,op:havoc,rep:2
│         │      │          │           │             │        │
│         │      │          │           │             │        └── repeticiones
│         │      │          │           │             └── operación de mutación
│         │      │          │           └── ejecución número
│         │      │          └── timestamp (ms desde inicio)
│         │      └── input padre (del que se mutó)
│         └── señal del crash (11 = SIGSEGV, 6 = SIGABRT)
└── ID secuencial
```

### Operaciones de mutación (campo `op`)

| op | Significado |
|---|---|
| `flip1`, `flip2`, `flip4` | Bit flip de 1, 2, 4 bits |
| `flip8`, `flip16`, `flip32` | Byte flip de 1, 2, 4 bytes |
| `arith8`, `arith16`, `arith32` | Aritmética sobre 1, 2, 4 bytes |
| `int8`, `int16`, `int32` | Sustitución con "interesting values" |
| `havoc` | Mutaciones combinadas aleatorias |
| `splice` | Combinación de dos inputs |
| `orig` | Semilla original (sin mutación) |

---

## 15. Interpretar crashes

### Reproducir un crash

```bash
# Ejecutar el target con el input del crash
./target_fuzz corpus_out/default/crashes/id:000000,sig:11,...

# Si fue compilado con ASan, el output es más informativo:
./target_fuzz_asan corpus_out/default/crashes/id:000000,...
```

### Output de ASan

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000000048 at pc 0x0000004f1a23 bp 0x7ffd12345678 sp 0x7ffd12345670
WRITE of size 50 at 0x602000000048 thread T0
    #0 0x4f1a22 in process /home/user/target.c:15:9
    #1 0x4f1b45 in main /home/user/target.c:35:5
    #2 0x7f1234567890 in __libc_start_main
    #3 0x41c289 in _start

0x602000000048 is located 0 bytes after 8-byte region [0x602000000040,0x602000000048)
allocated by thread T0 here:
    #0 0x4c1234 in malloc
    #1 0x4f1a00 in main /home/user/target.c:30:17

SUMMARY: AddressSanitizer: heap-buffer-overflow /home/user/target.c:15:9 in process
```

### Cómo leer el reporte de ASan

```
┌──────────────────────────────────────────────────────────────────────┐
│         Anatomía de un reporte AddressSanitizer                      │
│                                                                      │
│  Línea 1: tipo de bug                                               │
│  "heap-buffer-overflow" → escritura fuera de un buffer en heap       │
│  Otros tipos: stack-buffer-overflow, use-after-free, double-free,    │
│               heap-use-after-free, stack-use-after-return,           │
│               global-buffer-overflow                                 │
│                                                                      │
│  Línea 2: operación                                                  │
│  "WRITE of size 50" → intentó escribir 50 bytes                     │
│                                                                      │
│  Stack trace: dónde ocurrió                                          │
│  #0 process() en target.c:15 → la línea exacta del overflow         │
│  #1 main() en target.c:35   → quién llamó a process()              │
│                                                                      │
│  Información de memoria:                                             │
│  "0 bytes after 8-byte region" → el buffer es de 8 bytes,           │
│  la escritura empieza justo después del buffer                       │
│  "allocated ... in main target.c:30" → dónde se allocó el buffer   │
└──────────────────────────────────────────────────────────────────────┘
```

### Clasificar la severidad de un crash

| Señal / Tipo ASan | Severidad | Explotabilidad |
|---|---|---|
| heap-buffer-overflow (WRITE) | **Crítica** | Potencial RCE |
| stack-buffer-overflow (WRITE) | **Crítica** | Potencial RCE (stack smashing) |
| use-after-free | **Crítica** | Potencial RCE |
| double-free | **Alta** | Corrupción de heap |
| heap-buffer-overflow (READ) | **Media** | Information leak |
| null dereference (SIGSEGV) | **Media** | DoS |
| signed integer overflow (UBSan) | **Baja-Media** | Depende del contexto |
| SIGFPE (div by zero) | **Baja** | DoS |
| timeout (hang) | **Baja** | DoS |

### Pipeline de triaje de crashes

```bash
# 1. Listar todos los crashes
ls -la corpus_out/default/crashes/

# 2. Para cada crash, clasificar por señal
for crash in corpus_out/default/crashes/id:*; do
    echo "=== $crash ==="
    timeout 5 ./target_fuzz_asan "$crash" 2>&1 | grep "ERROR\|SUMMARY" || echo "CRASH (no ASan info)"
done

# 3. Minimizar cada crash
for crash in corpus_out/default/crashes/id:*; do
    base=$(basename "$crash")
    afl-tmin -i "$crash" -o "crashes_min/${base}" -- ./target_fuzz @@
done

# 4. Analizar crashes minimizados con GDB
gdb -batch -ex run -ex bt -ex quit --args ./target_fuzz crashes_min/id:000000,...
```

---

## 16. afl-tmin: minimizar test cases

`afl-tmin` reduce un input al mínimo necesario para reproducir el mismo crash o
la misma cobertura.

### Uso básico

```bash
# Minimizar un crash
afl-tmin -i crash_original -o crash_minimizado -- ./target_fuzz @@

# Con timeout
afl-tmin -t 5000 -i crash_original -o crash_minimizado -- ./target_fuzz @@

# Con más detalle
afl-tmin -v -i crash_original -o crash_minimizado -- ./target_fuzz @@
```

### Qué hace internamente

```
┌──────────────────────────────────────────────────────────────────────┐
│              Algoritmo de afl-tmin                                   │
│                                                                      │
│  Input original: [F U Z Z ! A B C D E F G H I J K L M N O]         │
│                   20 bytes, causa SIGSEGV                            │
│                                                                      │
│  Paso 1: Block deletion (eliminar bloques grandes)                   │
│  ¿[A B C D E F G H I J K L M N O] crashea? → No                    │
│  ¿[F U Z Z ! A B C D E] crashea? → Sí → reducir a 10 bytes        │
│                                                                      │
│  Paso 2: Block deletion más fina                                    │
│  ¿[F U Z Z ! A B C D] crashea? → Sí → 9 bytes                     │
│  ¿[F U Z Z ! A B C] crashea? → No                                  │
│  ¿[F U Z Z ! A B D] crashea? → No                                  │
│  ...                                                                 │
│                                                                      │
│  Paso 3: Byte-by-byte simplification                                │
│  ¿[F U Z Z ! A B C 0] crashea? → Sí → reemplazar último con 0     │
│  ¿[F U Z Z ! A B 0 0] crashea? → Sí → reemplazar penúltimo con 0  │
│  ...                                                                 │
│                                                                      │
│  Resultado: [F U Z Z ! 0 0 0 0]                                    │
│             9 bytes, misma señal (SIGSEGV)                          │
│             Los primeros 5 bytes son los que importan.              │
│                                                                      │
│  Reducción: 20 bytes → 9 bytes (55% más pequeño)                   │
└──────────────────────────────────────────────────────────────────────┘
```

### Cuándo usar afl-tmin

- **Siempre** antes de analizar un crash — el input minimizado es mucho más fácil
  de entender
- Antes de crear regression tests — el test case mínimo es más claro
- Cuando quieres compartir un crash con otro desarrollador

---

## 17. afl-cmin: minimizar el corpus

`afl-cmin` reduce el corpus completo al subconjunto mínimo que mantiene la misma
cobertura total.

### Uso

```bash
# Minimizar un corpus grande
afl-cmin -i corpus_grande/ -o corpus_minimizado/ -- ./target_fuzz @@

# Con timeout (para targets lentos)
afl-cmin -t 5000 -i corpus_grande/ -o corpus_minimizado/ -- ./target_fuzz @@

# Multithreaded (más rápido para corpus grandes)
AFL_THREADS=4 afl-cmin -i corpus_grande/ -o corpus_minimizado/ -- ./target_fuzz @@
```

### Ejemplo

```bash
# Antes:
ls corpus_grande/ | wc -l
# 1,847

# Después:
afl-cmin -i corpus_grande/ -o corpus_min/ -- ./target_fuzz @@
ls corpus_min/ | wc -l
# 234

# Verificar que la cobertura se mantuvo:
afl-showmap -i corpus_grande/ -o /dev/null -- ./target_fuzz @@ 2>&1 | tail -1
# Captured 2,340 tuples

afl-showmap -i corpus_min/ -o /dev/null -- ./target_fuzz @@ 2>&1 | tail -1
# Captured 2,340 tuples  ← misma cobertura con 8x menos archivos
```

### Cuándo usar afl-cmin

- Después de una sesión larga de fuzzing, antes de iniciar otra
- Cuando compartes el corpus con otro equipo
- Antes de correr regression tests sobre el corpus (menos archivos = más rápido)
- Cuando el corpus ocupa mucho disco

---

## 18. Diccionarios en AFL++

### Formato del diccionario

```
# Cada línea es un token que AFL puede insertar/reemplazar en los inputs.
# Formato: "token" o nombre="token"

# Tokens de un parser HTTP
method_get="GET"
method_post="POST"
method_put="PUT"
method_delete="DELETE"
http_version="HTTP/1.1"
header_sep="\r\n"
header_host="Host:"
header_content_type="Content-Type:"
header_content_length="Content-Length:"
content_type_json="application/json"
content_type_form="application/x-www-form-urlencoded"
path_sep="/"
query_sep="?"
query_amp="&"
query_eq="="
space=" "
colon=":"
```

### Tokens especiales con bytes no imprimibles

```
# Bytes hexadecimales
null_byte="\x00"
high_byte="\xff"
bom_utf8="\xef\xbb\xbf"

# Secuencias comunes en protocolos binarios
length_zero="\x00\x00\x00\x00"
length_max="\xff\xff\xff\xff"
magic_elf="\x7fELF"
magic_png="\x89PNG\x0d\x0a\x1a\x0a"
```

### Usar el diccionario

```bash
# Pasar con -x
afl-fuzz -i corpus/ -o output/ -x http.dict -- ./http_parser_fuzz @@

# Múltiples diccionarios: concatenarlos
cat http.dict json.dict > combined.dict
afl-fuzz -i corpus/ -o output/ -x combined.dict -- ./api_fuzz @@
```

### Autodictionary en modo LTO

```bash
# Con LTO, AFL++ extrae tokens automáticamente de las comparaciones del código
AFL_CC_COMPILER=LTO afl-cc -o target_lto target.c

# Los tokens se extraen de:
# • strcmp(input, "expected")  → "expected"
# • memcmp(input, magic, 4)   → los 4 bytes de magic
# • switch(input[0]) case 'A': case 'B': → "A", "B"

# Esto es ADICIONAL a cualquier diccionario manual
afl-fuzz -i corpus/ -o output/ -x extra.dict -- ./target_lto @@
```

### Dónde encontrar diccionarios predefinidos

```bash
# AFL++ incluye diccionarios comunes
ls /usr/share/afl/dictionaries/ 2>/dev/null
# o en el source:
ls AFLplusplus/dictionaries/

# Diccionarios disponibles:
# gif.dict, html_tags.dict, http.dict, jpeg.dict, js.dict,
# json.dict, pdf.dict, png.dict, regexp.dict, sql.dict,
# tiff.dict, xml.dict, yaml.dict, ...
```

---

## 19. Modo persistente: velocidad 10x

El modo persistente es la optimización más importante para la velocidad de AFL++.
En lugar de fork por cada input, la función target se llama múltiples veces dentro
del mismo proceso.

### Sin modo persistente (fork mode)

```
Para cada input:
  fork() → hijo ejecuta target → hijo muere → padre recoge resultado
  Overhead: ~1ms por fork (syscall + copy-on-write)
  Velocidad típica: 500-2000 exec/sec
```

### Con modo persistente

```
Un solo fork:
  fork() → hijo ejecuta target 10,000 veces → hijo muere
  Overhead: 1 fork por cada 10,000 ejecuciones
  Velocidad típica: 5,000-50,000 exec/sec
```

### Implementación en el harness

```c
/* harness_persistent.c — modo persistente de AFL++ */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* La macro __AFL_FUZZ_TESTCASE_LEN y __AFL_FUZZ_TESTCASE_BUF
   son proporcionadas por afl-cc cuando compila con modo persistente */

__AFL_FUZZ_INIT();  /* inicialización del modo persistente */

int main(int argc, char *argv[]) {
    /* Setup que solo ocurre UNA VEZ */
    /* Aquí va inicialización costosa: abrir DB, crear contexto, etc. */

    /* Señalizar a AFL que estamos en modo persistente */
    /* El loop interno se ejecuta N veces antes de un nuevo fork */

    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP(10000)) {  /* 10,000 iteraciones por fork */
        int len = __AFL_FUZZ_TESTCASE_LEN;

        /* Procesar el input del fuzzer */
        /* ¡¡ IMPORTANTE: no guardar estado entre iteraciones !! */
        process(buf, len);
    }

    return 0;
}
```

### Compilar

```bash
# afl-cc detecta __AFL_LOOP y habilita modo persistente automáticamente
afl-cc -fsanitize=address -o target_persistent harness_persistent.c target.c
```

### Restricciones del modo persistente

```
┌──────────────────────────────────────────────────────────────────────┐
│         Restricciones del modo persistente                           │
│                                                                      │
│  REQUISITO CRÍTICO: la función target NO debe:                      │
│                                                                      │
│  ✗ Modificar variables globales entre iteraciones                   │
│  ✗ Acumular memoria sin liberar (memory leaks)                      │
│  ✗ Dejar file descriptors abiertos                                  │
│  ✗ Modificar estado que afecte la siguiente iteración               │
│                                                                      │
│  Si el target tiene side effects:                                    │
│  1. Resetear el estado al inicio del loop                            │
│  2. Reducir el número de iteraciones (ej: 1000 en vez de 10000)     │
│  3. Usar fork mode en su lugar (más lento pero más seguro)          │
│                                                                      │
│  Verificar: ¿el programa da los mismos resultados con               │
│  __AFL_LOOP(1) que con __AFL_LOOP(10000)?                           │
│  Si sí → el modo persistente es seguro.                             │
│  Si no → hay state leakage, investigar.                             │
└──────────────────────────────────────────────────────────────────────┘
```

### Comparación de velocidad

```
  Velocidad de ejecución (exec/sec):
  
  Fork mode:        ████████████ 1,200/sec
  Persistent (1k):  ████████████████████████████████████ 8,500/sec
  Persistent (10k): ████████████████████████████████████████ 12,300/sec
  
  El speedup depende del overhead de inicialización del target.
  Para targets con setup costoso: 10-20x speedup.
  Para targets triviales: 3-5x speedup.
```

---

## 20. Modo deferred fork server

Si el programa tiene inicialización costosa que no depende del input, puedes decirle
a AFL que haga el fork DESPUÉS de la inicialización.

```c
/* El fork ocurre DESPUÉS de __AFL_INIT() */
#include <stdio.h>
#include <stdlib.h>

/* Inicialización costosa que no depende del input */
static DatabaseContext *db_ctx;
static ParserConfig *config;

int main(int argc, char *argv[]) {
    /* Esto se ejecuta UNA VEZ, antes de que AFL empiece a forkear */
    db_ctx = database_init("config.db");
    config = parser_config_load("parser.toml");

    /* ▼▼▼ AQUÍ AFL hace el fork ▼▼▼ */
    __AFL_INIT();
    /* ▲▲▲ A partir de aquí, cada fork es rápido ▲▲▲ */

    if (argc < 2) return 1;

    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;

    /* ... leer y procesar el input ... */

    fclose(f);
    return 0;
}
```

### Combinar deferred init + persistente

```c
int main(int argc, char *argv[]) {
    /* Setup costoso: una vez */
    ParserContext *ctx = parser_context_create();

    __AFL_INIT();  /* fork aquí */

    __AFL_FUZZ_INIT();
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP(10000)) {
        int len = __AFL_FUZZ_TESTCASE_LEN;
        parser_context_reset(ctx);  /* reset barato */
        parser_parse(ctx, buf, len);
    }

    parser_context_free(ctx);
    return 0;
}
```

---

## 21. Fuzzing en paralelo: múltiples instancias

AFL++ soporta fuzzing con múltiples instancias para aprovechar todos los CPUs.

### Arquitectura de fuzzing paralelo

```
┌──────────────────────────────────────────────────────────────────────┐
│           Fuzzing paralelo con AFL++                                 │
│                                                                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                 │
│  │  MAIN (-M)  │  │  SEC 1 (-S) │  │  SEC 2 (-S) │                 │
│  │             │  │             │  │             │                 │
│  │ Etapas      │  │ Solo havoc  │  │ Solo havoc  │                 │
│  │ deterministas│  │ + splice   │  │ + splice   │                 │
│  │ + havoc     │  │             │  │             │                 │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘                 │
│         │                │                │                         │
│         └────────┬───────┴────────┬───────┘                         │
│                  │                │                                  │
│                  ▼                ▼                                  │
│          ┌──────────────────────────┐                                │
│          │  Directorio compartido   │                                │
│          │  (output_dir/)           │                                │
│          │                         │                                │
│          │  main/queue/ ←→ sync → sec1/queue/                       │
│          │              ←→ sync → sec2/queue/                       │
│          │                         │                                │
│          │  Los inputs interesantes │                                │
│          │  se sincronizan entre    │                                │
│          │  instancias              │                                │
│          └──────────────────────────┘                                │
│                                                                      │
│  Regla: 1 instancia MAIN + N instancias SECONDARY                  │
│  La instancia MAIN ejecuta etapas deterministas (costosas).         │
│  Las SECONDARY solo ejecutan havoc (más rápido, más diverso).       │
└──────────────────────────────────────────────────────────────────────┘
```

### Lanzar fuzzing paralelo

```bash
# Terminal 1: instancia MAIN (ejecuta todas las etapas)
afl-fuzz -i corpus/ -o output/ -M main -- ./target_fuzz @@

# Terminal 2: instancia SECONDARY 1
afl-fuzz -i corpus/ -o output/ -S sec01 -- ./target_fuzz @@

# Terminal 3: instancia SECONDARY 2
afl-fuzz -i corpus/ -o output/ -S sec02 -- ./target_fuzz @@

# Terminal 4: instancia SECONDARY 3 (con diccionario)
afl-fuzz -i corpus/ -o output/ -S sec03 -x format.dict -- ./target_fuzz @@
```

### Variaciones avanzadas para múltiples instancias

```bash
# Diferentes power schedules para mayor diversidad:
afl-fuzz -i corpus/ -o output/ -M main -p fast -- ./target_fuzz @@
afl-fuzz -i corpus/ -o output/ -S sec01 -p explore -- ./target_fuzz @@
afl-fuzz -i corpus/ -o output/ -S sec02 -p coe -- ./target_fuzz @@
afl-fuzz -i corpus/ -o output/ -S sec03 -p rare -- ./target_fuzz @@

# Uno con CmpLog para resolver comparaciones
afl-fuzz -i corpus/ -o output/ -S sec_cmplog -c ./target_cmplog -- ./target_fuzz @@
```

### Cuántas instancias usar

| CPUs disponibles | Recomendación |
|---|---|
| 1 | 1 instancia (main) |
| 2-4 | 1 main + 1-3 secondary |
| 4-8 | 1 main + rest secondary con schedules variados |
| 8-16 | 1 main + 1 CmpLog + rest secondary |
| > 16 | Considerar targets diferentes o afl-fuzz en máquinas separadas |

---

## 22. AFL++ con sanitizers

### Compilar con cada sanitizer

```bash
# ASan + UBSan (RECOMENDADO para fuzzing)
afl-cc -fsanitize=address,undefined -g -o target_asan target.c

# MSan (para memoria no inicializada, NO combinar con ASan)
afl-cc -fsanitize=memory -g -o target_msan target.c

# TSan (para data races, raro en fuzzing de single-thread)
afl-cc -fsanitize=thread -g -o target_tsan target.c
```

### Estrategia multi-sanitizer

```bash
# Compilar dos versiones del target
afl-cc -fsanitize=address,undefined -g -o target_asan target.c
afl-cc -fsanitize=memory -g -o target_msan target.c

# Fuzzear con ASan (la principal)
afl-fuzz -i corpus/ -o output/ -M main -- ./target_asan @@

# Fuzzear con MSan en paralelo (encuentra bugs diferentes)
afl-fuzz -i corpus/ -o output/ -S sec_msan -- ./target_msan @@
```

### Ajustar límite de memoria con sanitizers

Los sanitizers aumentan el uso de memoria del programa. Por defecto, AFL++ limita
la memoria a 50MB, lo cual es insuficiente con ASan.

```bash
# ASan necesita más memoria (2-3x)
afl-fuzz -m 256 -i corpus/ -o output/ -- ./target_asan @@

# O deshabilitar el límite de memoria (menos seguro pero funciona)
afl-fuzz -m none -i corpus/ -o output/ -- ./target_asan @@
```

### Variables de entorno de ASan para fuzzing

```bash
# Optimizaciones de ASan para fuzzing
export ASAN_OPTIONS="abort_on_error=1:symbolize=0:detect_leaks=0"

# abort_on_error=1    → abortar inmediatamente (en vez de continuar)
# symbolize=0         → no resolver símbolos (más rápido)
# detect_leaks=0      → no detectar memory leaks (lento, ruido)

# Para análisis posterior (con símbolos):
ASAN_OPTIONS="symbolize=1" ./target_asan crash_file
```

---

## 23. CmpLog: resolver comparaciones complejas

CmpLog es una innovación de AFL++ que resuelve el problema de las comparaciones
de multi-byte. Sin CmpLog, el fuzzer tiene dificultad para descubrir inputs que
pasen comparaciones como `strcmp(input, "MAGIC_TOKEN")`.

### El problema

```c
void process(const char *data, size_t len) {
    if (len >= 12 && memcmp(data, "MAGIC_TOKEN!", 12) == 0) {
        /* El fuzzer necesita generar exactamente "MAGIC_TOKEN!"
           byte por byte — sin CmpLog, esto toma mucho tiempo
           incluso con coverage-guided fuzzing */
        vulnerable_function(data + 12, len - 12);
    }
}
```

### Cómo funciona CmpLog

```
┌──────────────────────────────────────────────────────────────────────┐
│              Cómo funciona CmpLog                                   │
│                                                                      │
│  1. Compilar una versión EXTRA del target con CmpLog:               │
│     afl-cc -fsanitize=address -o target_fuzz target.c               │
│     afl-cc -fsanitize=address -DAFL_CMPLOG -o target_cmplog target.c│
│                                                                      │
│  2. La versión CmpLog intercepta TODAS las comparaciones:           │
│     strcmp, memcmp, ==, <, >, switch, etc.                           │
│     Y registra: ¿qué comparó? ¿con qué valor?                      │
│                                                                      │
│  3. AFL++ usa esta información para "resolver" comparaciones:        │
│                                                                      │
│     Input: "ABCD_TOKEN!"                                            │
│     CmpLog captura: memcmp("ABCD_TOKEN!", "MAGIC_TOKEN!", 12)       │
│     AFL++ ve que el target esperaba "MAGIC_TOKEN!"                  │
│     → Reemplaza directamente "ABCD" con "MAGI" en el input         │
│     → El siguiente input ya pasa la comparación                     │
│                                                                      │
│  Sin CmpLog: encontrar "MAGIC_TOKEN!" por mutación aleatoria        │
│  podría tomar horas o días.                                          │
│  Con CmpLog: lo resuelve en segundos.                                │
└──────────────────────────────────────────────────────────────────────┘
```

### Uso

```bash
# Compilar dos versiones: normal + CmpLog
afl-cc -fsanitize=address -o target_fuzz target.c
afl-cc -fsanitize=address -o target_cmplog target.c
# (afl-cc detecta automáticamente si debe instrumentar con CmpLog
#  cuando se usa con -c en afl-fuzz)

# Fuzzear con CmpLog
afl-fuzz -i corpus/ -o output/ -c ./target_cmplog -- ./target_fuzz @@

# -c señala el binario con instrumentación CmpLog
# AFL++ lo ejecuta periódicamente para capturar comparaciones
# y las usa como "hints" para las mutaciones
```

### Niveles de CmpLog

```bash
# Nivel 1: solo comparaciones (default)
afl-fuzz -c ./target_cmplog -l 1 ...

# Nivel 2: comparaciones + transformaciones (recomendado)
afl-fuzz -c ./target_cmplog -l 2 ...
# Transforma los valores capturados: uppercase, lowercase, hex, etc.

# Nivel 3: más agresivo (experimental)
afl-fuzz -c ./target_cmplog -l 3 ...
```

---

## 24. Power schedules: estrategias de mutación

Los power schedules determinan cuánta "energía" (tiempo de mutación) se dedica a
cada input del corpus.

### Schedules disponibles

```bash
# Seleccionar schedule con -p
afl-fuzz -p explore ...   # default desde AFL++ 4.0
afl-fuzz -p fast ...      # favorece inputs rápidos
afl-fuzz -p coe ...       # Cut-Off Exponential
afl-fuzz -p lin ...       # lineal
afl-fuzz -p quad ...      # cuadrático
afl-fuzz -p exploit ...   # favorece inputs que encontraron nuevos edges
afl-fuzz -p rare ...      # favorece inputs que cubren edges raros
afl-fuzz -p seek ...      # balanceado
afl-fuzz -p mmopt ...     # MOpt (optimización automática)
```

### Cuál elegir

| Schedule | Mejor para | Comportamiento |
|---|---|---|
| `explore` | **General (default)** | Equilibrio exploración/explotación |
| `fast` | Targets lentos | Reduce energía a inputs lentos |
| `coe` | Corpus grandes | Reduce energía a inputs bien explorados |
| `rare` | Código con branches raros | Prioriza edges poco visitados |
| `exploit` | Correr poco tiempo | Maximiza explotación de hallazgos |
| `mmopt` | Experimentación | Optimiza automáticamente la estrategia |

### Combinar en fuzzing paralelo

La mejor estrategia es usar diferentes schedules en las instancias paralelas:

```bash
afl-fuzz -M main -p explore ...      # exploración general
afl-fuzz -S sec1 -p fast ...         # favorece velocidad
afl-fuzz -S sec2 -p rare ...         # busca edges raros
afl-fuzz -S sec3 -p coe ...          # cut-off exponential
```

---

## 25. afl-plot: visualizar el progreso

`afl-plot` genera gráficos del progreso del fuzzing a partir de los datos en
`plot_data`.

### Uso

```bash
# Generar gráficos (requiere gnuplot)
sudo dnf install gnuplot  # o apt install gnuplot

# Generar gráficos HTML
afl-plot output/default/ plot_output/

# Abrir en browser
firefox plot_output/index.html
```

### Qué gráficos genera

```
┌──────────────────────────────────────────────────────────────────────┐
│           Gráficos de afl-plot                                      │
│                                                                      │
│  1. EXECUTION SPEED                                                  │
│     exec/sec a lo largo del tiempo.                                  │
│     Ideal: constante o creciente.                                    │
│     Si decrece: el target se hace más lento con inputs más grandes.  │
│                                                                      │
│  2. CORPUS COUNT                                                     │
│     Número de inputs en el corpus a lo largo del tiempo.             │
│     Curva logarítmica: crece rápido al inicio, se aplana.           │
│     Si se aplana: el fuzzer exploró la mayor parte del código.      │
│                                                                      │
│  3. MAP COVERAGE                                                     │
│     Porcentaje del bitmap cubierto.                                  │
│     Similar a corpus count pero en % de edges.                      │
│                                                                      │
│  4. CRASHES / HANGS                                                  │
│     Número acumulado de crashes y hangs únicos.                      │
│     Escalones = momentos donde se encontró un nuevo crash.          │
└──────────────────────────────────────────────────────────────────────┘
```

### Usar los datos sin gnuplot

```bash
# El archivo plot_data es texto plano, parseable
head -5 output/default/plot_data
# unix_time, cycles_done, cur_item, corpus_count, pending_total,
# pending_favs, map_size, saved_crashes, saved_hangs, max_depth,
# execs_per_sec, total_execs, edges_found

# Gráficos con cualquier herramienta (Python, R, Excel, etc.)
```

---

## 26. afl-whatsup: monitoreo de múltiples instancias

```bash
# Ver estado de todas las instancias fuzzeando en el mismo directorio
afl-whatsup output/

# Output:
# Summary stats
# =============
#        Fuzzers alive : 4
#       Total run time : 3 days, 12 hrs
#          Total execs : 892M
#     Cumulative speed : 8,234 execs/sec
#        Total crashes : 7
#      Total corpus    : 3,847 entries
#
# main    : alive, 2.1M execs, 1,247/sec, 3 crashes, corpus 847
# sec01   : alive, 1.8M execs, 1,102/sec, 2 crashes, corpus 792
# sec02   : alive, 2.0M execs, 1,388/sec, 1 crash, corpus 810
# sec03   : alive, 1.9M execs, 1,297/sec, 1 crash, corpus 798

# Con más detalle:
afl-whatsup -s output/
```

---

## 27. Comparación con libFuzzer y Honggfuzz

| Aspecto | AFL++ | libFuzzer | Honggfuzz |
|---|---|---|---|
| **Ejecución** | Proceso externo (fork server) | In-process (linkado) | Proceso externo + Intel PT |
| **Setup** | Compilar target + preparar harness de archivos | Implementar `LLVMFuzzerTestOneInput` | Similar a AFL |
| **Velocidad (fork)** | ~1000-3000/sec | ~5000-50000/sec | ~1000-5000/sec |
| **Velocidad (persistent)** | ~5000-50000/sec | (siempre persistente) | ~5000-20000/sec |
| **Sanitizers** | ASan/MSan/UBSan/TSan | ASan/MSan/UBSan/TSan | ASan/MSan/UBSan |
| **CmpLog** | Sí (nativo) | `-use_value_profile=1` | No |
| **Diccionarios** | `-x dict` | `-dict=dict` | `--dict dict` |
| **Paralelismo** | `-M/-S` (múltiples procesos) | `-fork=N` (fork mode) | `-n N` (threads) |
| **Binary-only** | QEMU, Frida, Unicorn | No | Intel PT, Qemu |
| **Corpus merge** | `afl-cmin` | `-merge=1` | `--minimize` |
| **Test minimize** | `afl-tmin` | `-minimize_crash=1` | No nativo |
| **Dashboard** | TUI interactiva | Texto plano | Texto plano |
| **Curva de aprendizaje** | Media | Baja | Media |
| **Madurez** | Muy alta | Alta | Alta |
| **Comunidad** | Muy activa | Activa (LLVM) | Moderada |
| **Uso en CI** | Manual o con scripts | Fácil (un solo binario) | Manual |
| **Mejor para** | Targets complejos, binarios | Librerías, funciones | Hardware fuzzing |

### Cuándo elegir cada uno

```
┌──────────────────────────────────────────────────────────────────────┐
│         Cuándo elegir cada fuzzer                                    │
│                                                                      │
│  AFL++:                                                              │
│  • Fuzzing de programas completos (no solo funciones)                │
│  • Cuando necesitas CmpLog para comparaciones complejas              │
│  • Fuzzing de binarios sin código fuente (QEMU/Frida)                │
│  • Fuzzing paralelo con múltiples estrategias                        │
│  • Cuando quieres monitoreo visual (dashboard)                       │
│                                                                      │
│  libFuzzer:                                                          │
│  • Fuzzing de funciones individuales (API de librería)               │
│  • Máxima velocidad (in-process, sin fork overhead)                  │
│  • Integración con OSS-Fuzz                                         │
│  • Fuzzing de Rust (backend de cargo-fuzz)                           │
│  • Setup mínimo para librerías C/C++                                │
│                                                                      │
│  Honggfuzz:                                                          │
│  • Fuzzing con feedback de hardware (Intel PT)                       │
│  • Fuzzing de kernels y drivers                                      │
│  • Cuando AFL++ y libFuzzer no funcionan bien con el target         │
│                                                                      │
│  En la práctica: la mayoría de los proyectos usan AFL++ o libFuzzer.│
│  Para Rust: cargo-fuzz (libFuzzer backend) es el estándar.          │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 28. Errores comunes

### Error 1: "Hmm, your system is configured to send core dump notifications..."

```
❌ AFL++ se niega a iniciar.

✓ Solución:
echo core | sudo tee /proc/sys/kernel/core_pattern
```

### Error 2: exec speed es < 100/sec

```
❌ El fuzzing es demasiado lento.

✓ Diagnóstico:
1. ¿El target hace I/O de red o disco? → Fuzzear solo la función de parsing
2. ¿El target inicializa algo costoso? → Usar __AFL_INIT() (deferred)
3. ¿Está en fork mode? → Usar modo persistente (__AFL_LOOP)
4. ¿Compila con -O0? → Usar -O2
5. ¿ASan + ASLR activado? → Deshabilitar ASLR
```

### Error 3: no encuentra crashes cuando debería

```
❌ Fuzzeas por horas sin crashes, pero sabes que hay bugs.

✓ Diagnóstico:
1. ¿Compilaste con sanitizers? → Sin ASan, muchos bugs son silenciosos
2. ¿El corpus inicial es bueno? → Verificar con afl-showmap que las semillas
   cubren caminos diferentes
3. ¿Hay comparaciones complejas? → Usar CmpLog (-c)
4. ¿Usas diccionario? → Agregar tokens del formato
5. ¿El timeout es muy bajo? → El target no tiene tiempo de crashear
```

### Error 4: stability < 80%

```
❌ AFL muestra "stability: 67.2%" y el fuzzing es ineficiente.

✓ Causas:
• El target usa rand(), time(), o tiene threads
• ASLR está activo (variable randomization de direcciones)
• El target tiene estado global que afecta la cobertura

✓ Solución:
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
Mockear fuentes de no-determinismo en el harness.
```

### Error 5: corpus explota en tamaño

```
❌ Después de 24h, el corpus tiene 50,000 archivos y 2GB.

✓ Solución:
1. Parar el fuzzer
2. Minimizar: afl-cmin -i output/default/queue/ -o corpus_min/ -- ./target @@
3. Reiniciar con el corpus minimizado:
   afl-fuzz -i corpus_min/ -o output_new/ -- ./target @@
```

### Error 6: "No instrumentation detected"

```
❌ AFL se niega a iniciar porque el binario no está instrumentado.

✓ Causas:
• Compilaste con gcc en vez de afl-cc
• El Makefile usa cc hardcoded
• Un script de build sobreescribió CC

✓ Solución:
Verificar: afl-showmap -o /dev/null -- ./target < /dev/null
Si muestra 0 tuples, recompilar con afl-cc.
```

### Error 7: no convertir crashes en tests

```
❌ Encontraste 5 crashes, arreglaste los bugs, y olvidaste los crash files.
  Un mes después, un refactor reintroduce el mismo bug.

✓ Solución:
Para cada crash:
1. afl-tmin para minimizar
2. Convertir en unit test
3. Agregar al corpus como semilla
4. Commitear todo
```

### Error 8: fuzzear sin -g (debug symbols)

```
❌ El crash dice "at pc 0x4f1a23" pero no sabes qué línea de código es.

✓ Solución:
SIEMPRE compilar con -g para tener debug symbols:
afl-cc -fsanitize=address -g -o target target.c

Los símbolos no afectan la velocidad del fuzzing
(no cambian la instrumentación ni el fork server).
```

---

## 29. Ejemplo completo: fuzzear un parser INI

### El parser

```c
/* ini_parser.h */
#ifndef INI_PARSER_H
#define INI_PARSER_H

#include <stddef.h>

#define INI_MAX_SECTIONS  32
#define INI_MAX_KEYS      64
#define INI_MAX_LINE      512

typedef struct {
    char key[128];
    char value[256];
} IniEntry;

typedef struct {
    char name[128];
    IniEntry entries[INI_MAX_KEYS];
    int entry_count;
} IniSection;

typedef struct {
    IniSection sections[INI_MAX_SECTIONS];
    int section_count;
    char error_msg[256];
    int error_line;
} IniFile;

/* Parsea un buffer INI. Retorna 0 si éxito, -1 si error.
 * Los errores se almacenan en ini->error_msg y ini->error_line. */
int ini_parse(IniFile *ini, const char *data, size_t len);

/* Obtener un valor. Retorna NULL si no existe. */
const char *ini_get(const IniFile *ini, const char *section, const char *key);

#endif
```

```c
/* ini_parser.c */
#include "ini_parser.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Trim whitespace al inicio y final de un string in-place */
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

int ini_parse(IniFile *ini, const char *data, size_t len) {
    memset(ini, 0, sizeof(IniFile));

    /* Crear una copia para poder modificar (strtok) */
    char *buf = malloc(len + 1);
    if (!buf) {
        snprintf(ini->error_msg, sizeof(ini->error_msg), "out of memory");
        return -1;
    }
    memcpy(buf, data, len);
    buf[len] = '\0';

    int current_section = -1;  /* -1 = fuera de cualquier sección */
    int line_num = 0;
    char *line = strtok(buf, "\n");

    while (line != NULL) {
        line_num++;
        char *trimmed = trim(line);

        /* Línea vacía o comentario */
        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';') {
            line = strtok(NULL, "\n");
            continue;
        }

        /* Sección: [nombre] */
        if (*trimmed == '[') {
            char *end_bracket = strchr(trimmed, ']');
            if (!end_bracket) {
                snprintf(ini->error_msg, sizeof(ini->error_msg),
                         "missing ']' at line %d", line_num);
                ini->error_line = line_num;
                free(buf);
                return -1;
            }

            *end_bracket = '\0';
            char *section_name = trim(trimmed + 1);

            if (ini->section_count >= INI_MAX_SECTIONS) {
                snprintf(ini->error_msg, sizeof(ini->error_msg),
                         "too many sections at line %d", line_num);
                ini->error_line = line_num;
                free(buf);
                return -1;
            }

            current_section = ini->section_count;
            strncpy(ini->sections[current_section].name,
                    section_name, sizeof(ini->sections[0].name) - 1);
            ini->section_count++;

            line = strtok(NULL, "\n");
            continue;
        }

        /* Key=Value */
        char *eq = strchr(trimmed, '=');
        if (!eq) {
            snprintf(ini->error_msg, sizeof(ini->error_msg),
                     "expected '=' at line %d", line_num);
            ini->error_line = line_num;
            free(buf);
            return -1;
        }

        *eq = '\0';
        char *key = trim(trimmed);
        char *value = trim(eq + 1);

        if (current_section < 0) {
            /* Key fuera de sección: crear sección "" (global) */
            if (ini->section_count == 0) {
                ini->sections[0].name[0] = '\0';
                ini->section_count = 1;
            }
            current_section = 0;
        }

        IniSection *sec = &ini->sections[current_section];

        if (sec->entry_count >= INI_MAX_KEYS) {
            snprintf(ini->error_msg, sizeof(ini->error_msg),
                     "too many keys in section '%s' at line %d",
                     sec->name, line_num);
            ini->error_line = line_num;
            free(buf);
            return -1;
        }

        IniEntry *entry = &sec->entries[sec->entry_count];
        strncpy(entry->key, key, sizeof(entry->key) - 1);
        strncpy(entry->value, value, sizeof(entry->value) - 1);
        sec->entry_count++;

        line = strtok(NULL, "\n");
    }

    free(buf);
    return 0;
}

const char *ini_get(const IniFile *ini, const char *section, const char *key) {
    for (int s = 0; s < ini->section_count; s++) {
        if (strcmp(ini->sections[s].name, section) != 0) continue;
        for (int k = 0; k < ini->sections[s].entry_count; k++) {
            if (strcmp(ini->sections[s].entries[k].key, key) == 0) {
                return ini->sections[s].entries[k].value;
            }
        }
    }
    return NULL;
}
```

### El harness (modo persistente)

```c
/* fuzz_ini.c — harness para AFL++ modo persistente */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "ini_parser.h"

__AFL_FUZZ_INIT();

int main(int argc, char *argv[]) {

    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP(10000)) {
        int len = __AFL_FUZZ_TESTCASE_LEN;

        if (len < 1 || len > 65536) continue;

        IniFile ini;
        int result = ini_parse(&ini, (const char *)buf, len);

        if (result == 0) {
            /* Si el parseo fue exitoso, intentar buscar valores */
            for (int s = 0; s < ini.section_count; s++) {
                for (int k = 0; k < ini.sections[s].entry_count; k++) {
                    /* Verificar que ini_get encuentra lo que ini_parse guardó */
                    ini_get(&ini,
                            ini.sections[s].name,
                            ini.sections[s].entries[k].key);
                }
            }
        }
    }

    return 0;
}
```

### El harness para AFL++ classic (archivo)

```c
/* fuzz_ini_file.c — harness que lee de archivo (para AFL++ sin persistent mode) */
#include <stdio.h>
#include <stdlib.h>
#include "ini_parser.h"

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;

    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize > 65536 || fsize < 1) {
        fclose(f);
        return 0;
    }
    fseek(f, 0, SEEK_SET);

    char *data = malloc(fsize);
    if (!data) {
        fclose(f);
        return 1;
    }

    fread(data, 1, fsize, f);
    fclose(f);

    IniFile ini;
    int result = ini_parse(&ini, data, fsize);

    if (result == 0) {
        for (int s = 0; s < ini.section_count; s++) {
            for (int k = 0; k < ini.sections[s].entry_count; k++) {
                ini_get(&ini, ini.sections[s].name,
                        ini.sections[s].entries[k].key);
            }
        }
    }

    free(data);
    return 0;
}
```

### Diccionario

```
# ini.dict — tokens del formato INI
section_open="["
section_close="]"
comment_hash="#"
comment_semi=";"
equals="="
newline="\n"
cr_lf="\r\n"
space=" "
tab="\t"
quote="\""
true_val="true"
false_val="false"
empty=""
```

### Corpus inicial

```bash
mkdir corpus_in

# Semilla 1: mínimo
echo 'key=value' > corpus_in/minimal.ini

# Semilla 2: con sección
printf '[section]\nkey=value\n' > corpus_in/with_section.ini

# Semilla 3: múltiples secciones
printf '[db]\nhost=localhost\nport=5432\n[app]\nname=test\n' > corpus_in/multi.ini

# Semilla 4: comentarios y whitespace
printf '# comment\n[s]\n  key = value  \n; otro\n\n' > corpus_in/comments.ini

# Semilla 5: edge cases
printf '=\n[]\n=value\nkey=\n' > corpus_in/edge.ini
```

### Compilar y ejecutar

```bash
# 1. Compilar con instrumentación + ASan
afl-cc -fsanitize=address,undefined -g -o fuzz_ini fuzz_ini.c ini_parser.c

# 2. (Opcional) Compilar versión CmpLog
afl-cc -fsanitize=address,undefined -g -o fuzz_ini_cmplog fuzz_ini.c ini_parser.c

# 3. Configurar sistema
echo core | sudo tee /proc/sys/kernel/core_pattern

# 4. Lanzar fuzzing
afl-fuzz -i corpus_in -o output -x ini.dict \
    -c ./fuzz_ini_cmplog -- ./fuzz_ini

# 5. (Opcional) Paralelo en otra terminal
afl-fuzz -i corpus_in -o output -S sec01 -p rare -- ./fuzz_ini
```

### Resultados esperados

```bash
# Después de unos minutos, revisar crashes:
ls output/default/crashes/

# Reproducir cada crash:
for crash in output/default/crashes/id:*; do
    echo "=== $crash ==="
    ./fuzz_ini_file "$crash" 2>&1 | head -5
done

# Minimizar:
mkdir crashes_min
for crash in output/default/crashes/id:*; do
    base=$(basename "$crash")
    afl-tmin -i "$crash" -o "crashes_min/$base" -- ./fuzz_ini_file @@
done

# Analizar:
for crash in crashes_min/*; do
    echo "=== $(wc -c < "$crash") bytes ==="
    xxd "$crash"
    echo "---"
    ./fuzz_ini_file "$crash" 2>&1 | grep ERROR
done
```

### Crear regression tests

```c
/* test_ini_regressions.c */
#include <assert.h>
#include <string.h>
#include "ini_parser.h"

/* Regression: crash con trim en línea solo whitespace */
void test_whitespace_only_line(void) {
    const char *input = "[s]\n   \t  \nkey=val\n";
    IniFile ini;
    int result = ini_parse(&ini, input, strlen(input));
    /* No debe crashear */
    assert(result == 0);
    assert(ini.section_count == 1);
}

/* Regression: crash con = como primer carácter */
void test_equals_first_char(void) {
    const char *input = "=value\n";
    IniFile ini;
    int result = ini_parse(&ini, input, strlen(input));
    /* Debe aceptar (key vacío) o retornar error, no crashear */
    (void)result;
}

/* Regression: muchas secciones excediendo el límite */
void test_too_many_sections(void) {
    /* Generar 40 secciones (excede INI_MAX_SECTIONS = 32) */
    char input[4096];
    int pos = 0;
    for (int i = 0; i < 40 && pos < 4000; i++) {
        pos += snprintf(input + pos, sizeof(input) - pos,
                        "[section%d]\nkey=val\n", i);
    }
    IniFile ini;
    int result = ini_parse(&ini, input, pos);
    /* Debe retornar error, no crashear */
    assert(result == -1);
    assert(ini.section_count <= INI_MAX_SECTIONS);
}

int main(void) {
    test_whitespace_only_line();
    test_equals_first_char();
    test_too_many_sections();
    printf("All regression tests passed.\n");
    return 0;
}
```

---

## 30. Programa de práctica: url_parser

Construye un parser de URLs y fuzzéalo con AFL++ para encontrar bugs.

### Especificación

```
url_parser/
├── Makefile
├── url_parser.h        ← API pública
├── url_parser.c        ← implementación
├── fuzz_url.c          ← harness persistente para AFL++
├── fuzz_url_file.c     ← harness de archivo para AFL++
├── url.dict            ← diccionario de tokens de URL
├── corpus/             ← semillas iniciales
│   ├── simple.txt
│   ├── full.txt
│   ├── ipv6.txt
│   ├── encoded.txt
│   ├── noscheme.txt
│   ├── query.txt
│   └── fragment.txt
└── tests/
    └── test_url.c      ← unit tests + regression tests
```

### url_parser.h

```c
#ifndef URL_PARSER_H
#define URL_PARSER_H

#include <stddef.h>

typedef struct {
    char scheme[16];      /* http, https, ftp, ... */
    char userinfo[128];   /* user:pass */
    char host[256];       /* hostname or IP */
    int  port;            /* -1 si no especificado */
    char path[1024];      /* /path/to/resource */
    char query[2048];     /* key=val&key2=val2 (sin ?) */
    char fragment[256];   /* fragment (sin #) */
    int  is_valid;        /* 1 si parseo exitoso */
    char error[256];      /* mensaje de error si no es válido */
} Url;

/* Parsea una URL. Retorna 0 si éxito, -1 si error. */
int url_parse(Url *url, const char *input, size_t len);

/* Decodifica percent-encoding (%20 → space). In-place.
   Retorna la longitud decodificada o -1 si hay error. */
int url_decode(char *str, size_t len);

#endif
```

### Tareas

1. **Implementa `url_parser.c`** con soporte para:
   - scheme://userinfo@host:port/path?query#fragment
   - IPv4 y IPv6 (dentro de `[...]`)
   - Percent-encoding (%XX)
   - Puertos numéricos (validar rango 0-65535)
   - Introduce intencionalmente 3-4 bugs:
     - Buffer overflow en alguno de los campos
     - Integer overflow en el parsing del puerto
     - Error de off-by-one en url_decode
     - Manejo incorrecto de `%` al final del string

2. **Escribe `fuzz_url.c`** — harness persistente:
   - Usar `__AFL_FUZZ_INIT()` y `__AFL_LOOP(10000)`
   - Parsear y verificar que los campos no son basura

3. **Escribe `fuzz_url_file.c`** — harness de archivo:
   - Leer de `argv[1]`, parsear, verificar

4. **Crea `url.dict`** con tokens relevantes:
   - Schemes (http, https, ftp, file, ssh)
   - Delimitadores (:, /, @, ?, #, &, =)
   - Percent-encoding (%20, %00, %7F)
   - IPv6 ([, ], ::)

5. **Prepara el corpus** con 7+ semillas:
   - `http://example.com`
   - `https://user:pass@host:8080/path?q=1#frag`
   - `http://[::1]:80/`
   - `http://host/path%20with%20spaces`
   - URLs edge case (vacía, solo scheme, etc.)

6. **Compila y fuzzea**:
   ```bash
   afl-cc -fsanitize=address,undefined -g -o fuzz_url fuzz_url.c url_parser.c
   afl-cc -fsanitize=address,undefined -g -o fuzz_url_cmplog fuzz_url.c url_parser.c
   afl-fuzz -i corpus/ -o output/ -x url.dict -c ./fuzz_url_cmplog -- ./fuzz_url
   ```

7. **Ejecuta por al menos 15 minutos** y documenta:
   - exec speed
   - Número de crashes encontrados
   - Corpus final (total paths)
   - Tipos de bugs (analizados con ASan)

8. **Minimiza los crashes** con `afl-tmin`

9. **Corrige los bugs** y crea regression tests

10. **Fuzzea la versión corregida** otros 15 minutos y compara

---

## 31. Ejercicios

### Ejercicio 1: primera sesión de AFL++

**Objetivo**: Completar el ciclo completo de fuzzing con AFL++.

**Tareas**:

**a)** Toma el parser de CSV del tópico anterior (T01) y:
   - Compílalo con `afl-cc -fsanitize=address -g`
   - Prepara 5 semillas
   - Escribe un diccionario para CSV
   - Lanza `afl-fuzz` y déjalo correr 10 minutos

**b)** Mientras corre, toma una captura del dashboard y explica:
   - ¿Cuál es tu exec speed? ¿Es buena?
   - ¿Cuántos paths ha encontrado?
   - ¿Qué etapa de mutación ha sido más productiva?
   - ¿Qué es el stability y cuál es su valor?

**c)** Cuando encuentre crashes:
   - Minimiza cada uno con `afl-tmin`
   - Reproduce con ASan para obtener el tipo de bug exacto
   - Muestra el contenido hex del crash minimizado (`xxd crash_min`)

**d)** Arregla los bugs y verifica que el crash minimizado ya no crashea.

---

### Ejercicio 2: modo persistente vs fork mode

**Objetivo**: Medir el impacto del modo persistente en la velocidad.

**Tareas**:

**a)** Escribe dos harnesses para el parser INI:
   - `harness_fork.c`: harness clásico que lee de archivo (sin persistente)
   - `harness_persistent.c`: harness con `__AFL_LOOP(10000)`

**b)** Compila ambos con `afl-cc -fsanitize=address -g`

**c)** Ejecuta cada uno con el mismo corpus durante exactamente 2 minutos
(`-V 120`). Compara:
   - exec speed de cada uno
   - total execs de cada uno
   - Número de paths encontrados

**d)** Calcula el speedup: `exec_persistent / exec_fork = ?x`
   ¿Coincide con los 3-10x teóricos?

---

### Ejercicio 3: fuzzing paralelo

**Objetivo**: Configurar y evaluar fuzzing con múltiples instancias.

**Tareas**:

**a)** Lanza 4 instancias del fuzzer contra el parser INI:
   ```
   Terminal 1: afl-fuzz -M main -p explore -i corpus/ -o out/ -- ./fuzz @@
   Terminal 2: afl-fuzz -S sec1 -p fast -i corpus/ -o out/ -- ./fuzz @@
   Terminal 3: afl-fuzz -S sec2 -p rare -i corpus/ -o out/ -- ./fuzz @@
   Terminal 4: afl-fuzz -S sec3 -p coe -i corpus/ -o out/ -- ./fuzz @@
   ```

**b)** Después de 5 minutos, ejecuta `afl-whatsup out/` y analiza:
   - ¿Cuántas ejecuciones totales?
   - ¿Qué instancia tiene más paths?
   - ¿Hay imports entre instancias? (campo `imported`)

**c)** Compara con una sola instancia corriendo 5 minutos. ¿4 instancias
encontraron 4x más paths? ¿Por qué o por qué no?

**d)** Genera los gráficos con `afl-plot out/main/ plots/` y analiza la curva
de cobertura.

---

### Ejercicio 4: CmpLog y diccionarios

**Objetivo**: Medir el impacto de CmpLog y diccionarios en la efectividad.

**Tareas**:

**a)** Escribe un target con comparaciones de strings complejas:
   ```c
   void process(const char *data, size_t len) {
       if (len < 20) return;
       if (memcmp(data, "PROTOCOL", 8) != 0) return;
       if (memcmp(data + 8, "VERSION2", 8) != 0) return;
       uint32_t cmd = *(uint32_t *)(data + 16);
       if (cmd == 0x41424344) {  /* "ABCD" */
           char buf[4];
           memcpy(buf, data, len);  /* crash */
       }
   }
   ```

**b)** Fuzzea en 4 configuraciones durante 3 minutos cada una:
   1. Sin diccionario, sin CmpLog
   2. Con diccionario (tokens: "PROTOCOL", "VERSION2", "ABCD")
   3. Sin diccionario, con CmpLog
   4. Con diccionario + CmpLog

**c)** Compara: ¿cuántas ejecuciones necesitó cada configuración para encontrar
el crash? Ordena del más rápido al más lento.

**d)** ¿En qué casos CmpLog es más útil que un diccionario? ¿Y viceversa?

---

## Navegación

- **Anterior**: [T01 - Concepto de fuzzing](../T01_Concepto_de_fuzzing/README.md)
- **Siguiente**: [T03 - libFuzzer](../T03_libFuzzer/README.md)

---

> **Sección 1: Fuzzing en C** — Tópico 2 de 4 completado
>
> - T01: Concepto de fuzzing ✓
> - T02: AFL++ ✓
> - T03: libFuzzer (siguiente)
> - T04: Escribir harnesses
