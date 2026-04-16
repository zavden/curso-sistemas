# T01 - AddressSanitizer (ASan): buffer overflow, use-after-free, double-free, stack overflow — en C y Rust unsafe

## Índice

1. [Qué es AddressSanitizer](#1-qué-es-addresssanitizer)
2. [Historia y contexto](#2-historia-y-contexto)
3. [Arquitectura de ASan](#3-arquitectura-de-asan)
4. [Shadow memory: el mecanismo central](#4-shadow-memory-el-mecanismo-central)
5. [Mapeo de shadow memory](#5-mapeo-de-shadow-memory)
6. [Instrumentación del compilador](#6-instrumentación-del-compilador)
7. [Runtime library: el otro componente](#7-runtime-library-el-otro-componente)
8. [Compilar con ASan en C](#8-compilar-con-asan-en-c)
9. [Compilar con ASan en Rust](#9-compilar-con-asan-en-rust)
10. [Bug 1: heap buffer overflow](#10-bug-1-heap-buffer-overflow)
11. [Bug 2: stack buffer overflow](#11-bug-2-stack-buffer-overflow)
12. [Bug 3: global buffer overflow](#12-bug-3-global-buffer-overflow)
13. [Bug 4: use-after-free](#13-bug-4-use-after-free)
14. [Bug 5: double-free](#14-bug-5-double-free)
15. [Bug 6: use-after-return](#15-bug-6-use-after-return)
16. [Bug 7: use-after-scope](#16-bug-7-use-after-scope)
17. [Bug 8: stack overflow (stack exhaustion)](#17-bug-8-stack-overflow-stack-exhaustion)
18. [Bug 9: memory leaks (LeakSanitizer)](#18-bug-9-memory-leaks-leaksanitizer)
19. [Anatomía completa de un reporte ASan](#19-anatomía-completa-de-un-reporte-asan)
20. [Leer el stack trace](#20-leer-el-stack-trace)
21. [Leer la sección de memoria](#21-leer-la-sección-de-memoria)
22. [Shadow bytes legend](#22-shadow-bytes-legend)
23. [ASan en Rust unsafe: los mismos bugs](#23-asan-en-rust-unsafe-los-mismos-bugs)
24. [Ejemplo completo en C: string_buffer](#24-ejemplo-completo-en-c-string_buffer)
25. [Ejemplo completo en Rust unsafe: raw_vec](#25-ejemplo-completo-en-rust-unsafe-raw_vec)
26. [ASan con tests unitarios](#26-asan-con-tests-unitarios)
27. [ASan con fuzzing (repaso integrado)](#27-asan-con-fuzzing-repaso-integrado)
28. [ASAN_OPTIONS: configuración completa](#28-asan_options-configuración-completa)
29. [Impacto en rendimiento detallado](#29-impacto-en-rendimiento-detallado)
30. [Falsos positivos y cómo manejarlos](#30-falsos-positivos-y-cómo-manejarlos)
31. [Suppressions](#31-suppressions)
32. [ASan vs Valgrind](#32-asan-vs-valgrind)
33. [Limitaciones de ASan](#33-limitaciones-de-asan)
34. [Errores comunes](#34-errores-comunes)
35. [Programa de práctica: memory_pool](#35-programa-de-práctica-memory_pool)
36. [Ejercicios](#36-ejercicios)

---

## 1. Qué es AddressSanitizer

AddressSanitizer (ASan) es una herramienta de detección de errores de memoria implementada como instrumentación del compilador + biblioteca de runtime. Detecta bugs que de otra forma serían silenciosos y explotables.

```
┌──────────────────────────────────────────────────────────────────┐
│                    AddressSanitizer (ASan)                        │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Componentes:                                                    │
│  ┌──────────────────────────┐  ┌──────────────────────────────┐  │
│  │ Instrumentación          │  │ Runtime library              │  │
│  │ (compilador)             │  │ (libasan)                    │  │
│  │                          │  │                              │  │
│  │ - Insertar checks antes  │  │ - Reemplazar malloc/free     │  │
│  │   de cada load/store     │  │ - Gestionar shadow memory    │  │
│  │ - Envenenar redzones     │  │ - Quarantine para freed mem  │  │
│  │   en stack frames        │  │ - Imprimir reportes          │  │
│  └──────────────────────────┘  └──────────────────────────────┘  │
│                                                                  │
│  Detecta:                                                        │
│  ✓ Heap buffer overflow/underflow                                │
│  ✓ Stack buffer overflow/underflow                               │
│  ✓ Global buffer overflow                                        │
│  ✓ Use-after-free (heap)                                         │
│  ✓ Use-after-return (stack)                                      │
│  ✓ Use-after-scope (stack)                                       │
│  ✓ Double-free                                                   │
│  ✓ Invalid free (free de dirección no-malloc)                    │
│  ✓ Memory leaks (vía LeakSanitizer integrado)                   │
│  ✓ Stack overflow (stack exhaustion)                             │
│                                                                  │
│  NO detecta:                                                     │
│  ✗ Lectura de memoria no inicializada (→ MSan)                   │
│  ✗ Data races (→ TSan)                                           │
│  ✗ Integer overflow, UB aritmético (→ UBSan)                     │
│  ✗ Acceso dentro de bounds pero a campo incorrecto               │
│  ✗ Errores lógicos                                               │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 2. Historia y contexto

```
2011: Desarrollado por Konstantin Serebryany y otros en Google
      Publicado como paper: "AddressSanitizer: A Fast Memory Error Detector"
      Integrado en Clang/LLVM 3.1

2012: Integrado en GCC 4.8
      Empieza a usarse en Chromium, Firefox, Linux kernel testing

2013: LeakSanitizer (LSan) se integra como parte de ASan

2015-: Se convierte en herramienta estándar de la industria
       Usado en desarrollo de: Chrome, Firefox, Android, LLVM, GCC,
       OpenSSL, curl, y cientos de proyectos más

2020+: Soporte en Rust (nightly) a través de -Zsanitizer=address

Hoy: ASan ha encontrado miles de bugs de seguridad en software real.
     Es la herramienta más importante para seguridad de memoria en C/C++.
```

### El paper original

El paper de Serebryany et al. (2012) estableció que ASan es:
- **Rápido**: ~2x slowdown (vs 20-50x de Valgrind)
- **Preciso**: sin falsos positivos en práctica
- **Comprensivo**: detecta heap, stack, y global overflows
- **Práctico**: funciona con código real, no solo benchmarks

---

## 3. Arquitectura de ASan

```
┌─────────────────────────────────────────────────────────────────┐
│                   Arquitectura de ASan                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  COMPILACIÓN (clang -fsanitize=address / rustc -Zsanitizer=...) │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                                                           │  │
│  │  Código fuente                                            │  │
│  │       │                                                   │  │
│  │       ▼                                                   │  │
│  │  ┌─────────────────┐                                      │  │
│  │  │  Frontend        │  Parsear C/Rust → IR (LLVM IR)      │  │
│  │  └────────┬────────┘                                      │  │
│  │           │                                               │  │
│  │           ▼                                               │  │
│  │  ┌─────────────────┐                                      │  │
│  │  │  ASan pass       │  Para cada load/store:              │  │
│  │  │  (instrumentar)  │    1. Calcular shadow address       │  │
│  │  │                  │    2. Leer shadow byte              │  │
│  │  │                  │    3. If != 0: reportar error        │  │
│  │  │                  │                                     │  │
│  │  │                  │  Para stack frames:                  │  │
│  │  │                  │    - Insertar redzones alrededor     │  │
│  │  │                  │      de variables locales            │  │
│  │  │                  │    - Envenenar redzones al entrar    │  │
│  │  │                  │    - Desenvenenar al salir           │  │
│  │  └────────┬────────┘                                      │  │
│  │           │                                               │  │
│  │           ▼                                               │  │
│  │  ┌─────────────────┐                                      │  │
│  │  │  Linker          │  Enlazar con libasan               │  │
│  │  └─────────────────┘                                      │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│  EJECUCIÓN                                                      │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                                                           │  │
│  │  libasan (runtime):                                       │  │
│  │  ┌─────────────────────────────────────────────────────┐  │  │
│  │  │ 1. Reservar shadow memory al inicio                 │  │  │
│  │  │ 2. Reemplazar malloc → __asan_malloc                │  │  │
│  │  │    - Allocar con redzones                           │  │  │
│  │  │    - Envenenar redzones en shadow                   │  │  │
│  │  │ 3. Reemplazar free → __asan_free                    │  │  │
│  │  │    - Envenenar toda la región                       │  │  │
│  │  │    - Poner en quarantine (no reusar inmediatamente) │  │  │
│  │  │ 4. Cuando un check falla: __asan_report_error       │  │  │
│  │  │    - Imprimir stack trace                           │  │  │
│  │  │    - Imprimir contexto de memoria                   │  │  │
│  │  │    - Abortar el programa                            │  │  │
│  │  └─────────────────────────────────────────────────────┘  │  │
│  │                                                           │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4. Shadow memory: el mecanismo central

### Concepto

Shadow memory es una región de la memoria del proceso que **mapea** la memoria real. Cada byte de shadow memory describe el estado de 8 bytes de memoria real.

```
Memoria real (application memory):
┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐
│  byte 0-7 (8 bytes)    │  byte 8-15 (8 bytes)   │
└──┬──┴──┴──┴──┴──┴──┴──┴──┬──┴──┴──┴──┴──┴──┴──┘
   │                        │
   │ mapea 8:1              │ mapea 8:1
   │                        │
   ▼                        ▼
┌──────┐                ┌──────┐
│ 0x00 │                │ 0x05 │
└──────┘                └──────┘
Shadow byte 0           Shadow byte 1
```

### Valores del shadow byte

| Shadow byte | Significado |
|---|---|
| `0x00` | Los 8 bytes de app memory son accesibles |
| `0x01` - `0x07` | Solo los primeros 1-7 bytes son accesibles (allocación parcial) |
| `0xfa` | Redzone de stack (envenenado por instrumentación) |
| `0xf1` | Left redzone de stack (antes de la variable) |
| `0xf3` | Right redzone de stack (después de la variable) |
| `0xf5` | Stack frame después de return (use-after-return) |
| `0xf8` | Stack variable fuera de scope (use-after-scope) |
| `0xfd` | Freed memory (quarantined) |
| `0xfe` | Left redzone de heap |
| `0xfa` | Right redzone de heap |
| `0xfb` | Freed heap left redzone |
| `0xfc` | Freed heap right redzone |

### Cómo se verifica un acceso

```c
// Código original:
*ptr = 42;

// Con ASan (pseudocódigo de la instrumentación):
shadow_addr = (ptr >> 3) + SHADOW_OFFSET;
shadow_value = *shadow_addr;
if (shadow_value != 0) {
    // Verificación adicional para allocaciones parciales
    if (shadow_value > 0) {
        // Los primeros shadow_value bytes son accesibles
        last_accessed = (ptr & 7) + access_size;
        if (last_accessed > shadow_value) {
            __asan_report_store(ptr, access_size);
        }
    } else {
        // Completamente envenenado
        __asan_report_store(ptr, access_size);
    }
}
*ptr = 42;  // acceso original
```

---

## 5. Mapeo de shadow memory

### La fórmula de mapeo

```
shadow_address = (application_address >> 3) + SHADOW_OFFSET

Donde:
  >> 3            = dividir por 8 (cada shadow byte cubre 8 app bytes)
  SHADOW_OFFSET   = constante que depende de la plataforma

En Linux x86_64:
  SHADOW_OFFSET = 0x7fff8000

Ejemplo:
  app address:    0x7ffe12345678
  shadow address: 0x7ffe12345678 >> 3 + 0x7fff8000
                = 0x0ffffc2468ACF + 0x7fff8000
                = 0x10007fc2468ACF
```

### Mapa de memoria del proceso con ASan

```
┌─────────────────────────────┐  0xFFFFFFFFFFFF
│ Kernel space                │
├─────────────────────────────┤
│ High shadow                 │  Shadow de la zona alta
├─────────────────────────────┤
│ High application memory     │  Zona alta de app (stack, etc.)
├─────────────────────────────┤
│ Shadow gap (protected)      │  No accesible (SEGV si se toca)
├─────────────────────────────┤
│ Low shadow                  │  Shadow de la zona baja
├─────────────────────────────┤
│ Low application memory      │  Zona baja de app (heap, globals)
├─────────────────────────────┤
│ Bad (protected)             │  No accesible
└─────────────────────────────┘  0x000000000000
```

### Costo de shadow memory

```
Shadow memory ocupa 1/8 del espacio de direcciones virtual.

En un proceso de 64 bits:
  Teóricamente: 2^64 / 8 = 2^61 bytes de shadow
  En práctica:  solo las páginas accedidas se mapean físicamente
  
  Proceso típico:  100 MB de app memory → ~13 MB de shadow
  Proceso grande: 1 GB de app memory → ~128 MB de shadow

El costo de RAM es ~2x-3x del programa original.
```

---

## 6. Instrumentación del compilador

### Qué inserta el compilador

Para **cada acceso a memoria** (load/store), el compilador inserta un check:

```c
// Código original:
int x = array[i];

// Código instrumentado (pseudocódigo):
{
    void *addr = &array[i];
    size_t size = sizeof(int);  // 4

    // Calcular shadow address
    char *shadow = (char *)((uintptr_t)addr >> 3) + SHADOW_OFFSET;
    char shadow_value = *shadow;

    if (shadow_value) {
        // Verificar acceso parcial
        char last_byte = ((uintptr_t)addr & 7) + size - 1;
        if (last_byte >= shadow_value) {
            __asan_report_load4(addr);  // ERROR!
        }
    }
}
int x = array[i];  // acceso original
```

### Para stack frames

```c
void my_function() {
    int a[10];    // 40 bytes
    char b[20];   // 20 bytes
    float c;      // 4 bytes

    // ASan transforma el stack frame a:
    // [redzone_left][a (40)][redzone][b (20)][redzone][c (4)][redzone_right]
    // [  32 bytes  ]       [ 32 bytes]       [32 bytes]      [  32 bytes   ]

    // Al entrar a la función:
    //   Envenenar todos los redzones en shadow memory

    // Al salir de la función:
    //   Desenvenenar todo el frame

    // Si accedes a a[10] (1 byte después de a):
    //   → Estás en el redzone → shadow byte = 0xf1 → ERROR
}
```

### Overhead de la instrumentación

```
Cada acceso a memoria:
  Antes: 1 instrucción (mov)
  Después: ~7-12 instrucciones (calcular shadow, leer, comparar, branch)

Pero el branch "no hay error" se predice correctamente 99.99%+ del tiempo,
así que el pipeline del CPU no se estanca significativamente.

Overhead de código: binario ~2x más grande
Overhead de CPU:    ~2x más lento
```

---

## 7. Runtime library: el otro componente

### Reemplazo de malloc/free

ASan reemplaza las funciones del sistema de allocación con sus propias versiones:

```
malloc(10) original:
  ┌──────────┐
  │ 10 bytes │
  └──────────┘

__asan_malloc(10):
  ┌──────────┬──────────┬──────────┐
  │ left     │ 10 bytes │ right    │
  │ redzone  │ (tu data)│ redzone  │
  │ (poison) │          │ (poison) │
  │ 16 bytes │          │ 16 bytes │
  └──────────┴──────────┴──────────┘

Shadow:
  [0xfe][0x00 0x02][0xfa]
  ^^^^               ^^^^
  left redzone       right redzone
  (envenenado)       (envenenado)

  0x02 = solo los primeros 2 de los últimos 8 bytes son accesibles
  (10 bytes = 8 + 2, así que el segundo shadow byte es 0x02)
```

### Quarantine para freed memory

Cuando llamas `free(ptr)`:

```
free(ptr) en ASan:
  1. NO retornar la memoria al OS inmediatamente
  2. Envenenar toda la región en shadow memory (0xfd)
  3. Poner el bloque en una cola FIFO (quarantine)
  4. Solo reciclar la memoria cuando la quarantine se llena

Quarantine:
  ┌────────────────────────────────────────────────┐
  │ [freed 3][freed 2][freed 1]  ← nuevos         │
  │                              freed 0 ← sale    │
  │                              (se recicla)      │
  └────────────────────────────────────────────────┘
  Tamaño default: 256 MB

Efecto:
  Si haces use-after-free, la memoria sigue envenenada
  → ASan lo detecta

  Sin quarantine:
  free(ptr) → malloc(10) → reutiliza la misma memoria
  *ptr = 42   → sin error (la memoria ya fue reasignada)
  → Bug SILENCIOSO

  Con quarantine:
  free(ptr) → la memoria sigue envenenada por un rato
  *ptr = 42   → shadow = 0xfd → ERROR: use-after-free
```

---

## 8. Compilar con ASan en C

### Con Clang

```bash
# Compilar
clang -fsanitize=address -fno-omit-frame-pointer -g -O1 -o programa programa.c

# Flags explicados:
# -fsanitize=address       → activar ASan
# -fno-omit-frame-pointer  → stack traces legibles
# -g                       → símbolos de debug (líneas de código en reportes)
# -O1                      → optimización leve (recomendado: no -O0 ni -O3)
```

### Con GCC

```bash
gcc -fsanitize=address -fno-omit-frame-pointer -g -O1 -o programa programa.c
```

### ¿Qué nivel de optimización?

| Nivel | Recomendado | Por qué |
|---|---|---|
| `-O0` | No | Demasiado lento, ASan ya es 2x más lento |
| `-O1` | Sí | Buen balance velocidad/debuggeabilidad |
| `-O2` | A veces | Más rápido, pero algunos bugs se optimizan away |
| `-O3` | No | El optimizador puede eliminar código que tiene bugs |
| `-Og` | Sí | Optimizado para debug, buen compromiso |

### Ejemplo mínimo en C

```c
// overflow.c
#include <stdlib.h>

int main() {
    int *p = (int *)malloc(10 * sizeof(int));  // 40 bytes
    p[10] = 42;  // ← heap-buffer-overflow (escribe 4 bytes después del buffer)
    free(p);
    return 0;
}
```

```bash
clang -fsanitize=address -g -O1 -o overflow overflow.c
./overflow
```

---

## 9. Compilar con ASan en Rust

### Con cargo (proyecto)

```bash
RUSTFLAGS="-Zsanitizer=address" cargo +nightly build --release
```

### Con rustc directamente

```bash
rustup run nightly rustc -Zsanitizer=address -Cdebuginfo=2 programa.rs
```

### Con cargo-fuzz (ya visto en S02)

```bash
# ASan es el default
cargo fuzz run my_target
# Equivalente a:
cargo fuzz run my_target --sanitizer=address
```

### Con cargo test

```bash
# Ejecutar tests unitarios con ASan
RUSTFLAGS="-Zsanitizer=address" cargo +nightly test
```

### Ejemplo mínimo en Rust

```rust
// src/main.rs
fn main() {
    unsafe {
        let layout = std::alloc::Layout::from_size_align(40, 4).unwrap();
        let ptr = std::alloc::alloc(layout) as *mut i32;

        // heap-buffer-overflow: escribir después del buffer
        *ptr.add(10) = 42;  // offset 10 * 4 = 40 bytes, pero solo hay 40
                             // → escribe en byte 40-43, fuera del buffer

        std::alloc::dealloc(ptr as *mut u8, layout);
    }
}
```

```bash
RUSTFLAGS="-Zsanitizer=address" cargo +nightly run
```

---

## 10. Bug 1: heap buffer overflow

### Descripción

Acceder a memoria **más allá del final** (o antes del inicio) de un buffer allocado en el heap.

### En C

```c
#include <stdlib.h>
#include <string.h>

void copy_data(const char *input, size_t len) {
    char *buf = (char *)malloc(64);  // 64 bytes

    // BUG: no verificar que len <= 64
    memcpy(buf, input, len);  // Si len > 64 → heap-buffer-overflow

    // Procesar buf...
    free(buf);
}

int main() {
    char data[100];
    memset(data, 'A', 100);
    copy_data(data, 100);  // 100 > 64 → overflow
    return 0;
}
```

### En Rust unsafe

```rust
unsafe fn copy_data(input: &[u8]) {
    let layout = std::alloc::Layout::from_size_align(64, 1).unwrap();
    let buf = std::alloc::alloc(layout);

    // BUG: no verificar que input.len() <= 64
    std::ptr::copy_nonoverlapping(input.as_ptr(), buf, input.len());

    std::alloc::dealloc(buf, layout);
}

fn main() {
    let data = vec![0x41u8; 100];
    unsafe { copy_data(&data); }  // 100 > 64 → overflow
}
```

### Reporte de ASan

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000000070
WRITE of size 36 at 0x602000000070 thread T0
    #0 0x55a3... in copy_data src/main.rs:6:5
    #1 0x55a3... in main src/main.rs:11:14

0x602000000070 is located 0 bytes after 64-byte region [0x602000000030,0x602000000070)
allocated by thread T0 here:
    #0 0x55a3... in __rust_alloc
    #1 0x55a3... in copy_data src/main.rs:3:15

SUMMARY: AddressSanitizer: heap-buffer-overflow src/main.rs:6:5 in copy_data
Shadow bytes around the buggy address:
  0x0c047fff7fb0: 00 00 00 00 00 00 00 00 fa fa fa fa fa fa fa fa
  0x0c047fff7fc0: 00 00 00 00 00 00 00 00 fa fa fa fa fa fa fa fa
=>0x0c047fff7fd0: 00 00 00 00 00 00 00 00[fa]fa fa fa fa fa fa fa
                                          ^^
                                          right redzone (envenenado)
```

### Severidad

```
Heap buffer overflow: CRÍTICA

- Lectura OOB: información sensible (heartbleed, etc.)
- Escritura OOB: control de flujo (RCE potencial)
  - Sobrescribir metadata de malloc → arbitrary write
  - Sobrescribir punteros de función → ejecución de código
  - Sobrescribir datos de control → bypass de seguridad
```

---

## 11. Bug 2: stack buffer overflow

### Descripción

Acceder a memoria más allá de un buffer declarado como variable local (en el stack).

### En C

```c
#include <string.h>

void vulnerable(const char *input) {
    char buf[64];

    // BUG: strcpy no limita la copia al tamaño de buf
    strcpy(buf, input);  // Si strlen(input) > 63 → stack-buffer-overflow

    // Procesar buf...
}

int main() {
    char data[200];
    memset(data, 'A', 199);
    data[199] = '\0';
    vulnerable(data);  // 199 > 63 → overflow
    return 0;
}
```

### En Rust unsafe

```rust
unsafe fn vulnerable(input: &[u8]) {
    let mut buf: [u8; 64] = [0; 64];

    // BUG: copiar sin verificar tamaño
    std::ptr::copy_nonoverlapping(
        input.as_ptr(),
        buf.as_mut_ptr(),
        input.len(),  // Si input.len() > 64 → stack-buffer-overflow
    );
}

fn main() {
    let data = vec![0x41u8; 200];
    unsafe { vulnerable(&data); }
}
```

### Reporte de ASan

```
==12345==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7ffd12345700
WRITE of size 200 at 0x7ffd12345700 thread T0
    #0 0x55a3... in vulnerable src/main.rs:7:5
    #1 0x55a3... in main src/main.rs:14:14

Address 0x7ffd12345700 is located in stack of thread T0 at offset 96 in frame
    #0 0x55a3... in vulnerable src/main.rs:2

  This frame has 1 object(s):
    [32, 96) 'buf' (line 3)    <== Memory access at offset 96 overflows this variable
```

### Severidad

```
Stack buffer overflow: CRÍTICA

Aún más peligroso que heap overflow porque:
- Sobrescribe la dirección de retorno → control de flujo directo
- Classic "return-to-libc" o ROP chains
- Posible bypass de ASLR + DEP con gadgets

Históricamente es el tipo de bug más explotado en C/C++.
```

---

## 12. Bug 3: global buffer overflow

### Descripción

Acceder más allá de un buffer global (variable global o estática).

### En C

```c
#include <stdio.h>

// Global buffer
char global_buf[10] = "hello";

void read_global(int index) {
    // BUG: no verificar bounds
    printf("char at %d: %c\n", index, global_buf[index]);
}

int main() {
    read_global(15);  // 15 > 9 → global-buffer-overflow
    return 0;
}
```

### En Rust unsafe

```rust
static mut GLOBAL_BUF: [u8; 10] = [0; 10];

unsafe fn read_global(index: usize) -> u8 {
    // BUG: sin bounds check
    *GLOBAL_BUF.as_ptr().add(index)
}

fn main() {
    let val = unsafe { read_global(15) };
    println!("val: {}", val);
}
```

### Reporte de ASan

```
==12345==ERROR: AddressSanitizer: global-buffer-overflow on address 0x55a3b2c4d00f
READ of size 1 at 0x55a3b2c4d00f thread T0
    #0 0x55a3... in read_global src/main.rs:5:5

0x55a3b2c4d00f is located 5 bytes after global variable 'GLOBAL_BUF'
    defined in 'src/main.rs:1:1' (0x55a3b2c4d000) of size 10
```

---

## 13. Bug 4: use-after-free

### Descripción

Acceder a memoria que ya fue liberada con `free()` (C) o `drop`/`dealloc` (Rust).

### En C

```c
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} User;

int main() {
    User *user = (User *)malloc(sizeof(User));
    user->id = 42;
    snprintf(user->name, 32, "Alice");

    free(user);  // Liberar

    // BUG: usar después de liberar
    printf("User: %d %s\n", user->id, user->name);
    // ↑ use-after-free: user apunta a memoria liberada

    return 0;
}
```

### En Rust unsafe

```rust
fn main() {
    unsafe {
        let user = Box::new((42i32, "Alice"));
        let ptr = Box::into_raw(user);

        // Liberar
        drop(Box::from_raw(ptr));

        // BUG: usar después de liberar
        let id = (*ptr).0;  // use-after-free
        println!("id: {}", id);
    }
}
```

### Reporte de ASan

```
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x602000000010
READ of size 4 at 0x602000000010 thread T0
    #0 0x55a3... in main src/main.rs:10:18

0x602000000010 is located 0 bytes inside of 40-byte region [0x602000000010,0x602000000038)
freed by thread T0 here:
    #0 0x55a3... in __rust_dealloc
    #1 0x55a3... in main src/main.rs:7:14

previously allocated by thread T0 here:
    #0 0x55a3... in __rust_alloc
    #1 0x55a3... in main src/main.rs:3:24
```

### Severidad

```
Use-after-free: CRÍTICA

- La memoria liberada puede ser reasignada a otro objeto
- El atacante puede controlar el contenido del nuevo objeto
- Al leer: información de otro objeto (info leak)
- Al escribir: corromper otro objeto (type confusion → RCE)

CVE más frecuente en browsers (Chrome, Firefox, Safari).
```

---

## 14. Bug 5: double-free

### Descripción

Llamar `free()` dos veces sobre el mismo puntero.

### En C

```c
#include <stdlib.h>

int main() {
    char *buf = (char *)malloc(100);

    free(buf);
    free(buf);  // BUG: double-free

    return 0;
}
```

### En Rust unsafe

```rust
fn main() {
    unsafe {
        let layout = std::alloc::Layout::from_size_align(100, 1).unwrap();
        let ptr = std::alloc::alloc(layout);

        std::alloc::dealloc(ptr, layout);
        std::alloc::dealloc(ptr, layout);  // BUG: double-free
    }
}
```

### Reporte de ASan

```
==12345==ERROR: AddressSanitizer: attempting double-free on 0x602000000010 in thread T0:
    #0 0x55a3... in __rust_dealloc
    #1 0x55a3... in main src/main.rs:7:9

0x602000000010 is located 0 bytes inside of 100-byte region [0x602000000010,0x602000000074)
freed by thread T0 here:
    #0 0x55a3... in __rust_dealloc
    #1 0x55a3... in main src/main.rs:6:9

previously allocated by thread T0 here:
    #0 0x55a3... in __rust_alloc
    #1 0x55a3... in main src/main.rs:4:20
```

### Severidad

```
Double-free: ALTA a CRÍTICA

- Corrompe las estructuras internas del allocator
- Puede convertir la siguiente malloc() en un arbitrary write
- Explotable para RCE en muchos allocators (ptmalloc, jemalloc)
```

---

## 15. Bug 6: use-after-return

### Descripción

Usar un puntero a una variable local después de que la función retornó.

### En C

```c
int *get_ptr() {
    int local = 42;
    return &local;  // ← retorna puntero a variable local
}

int main() {
    int *p = get_ptr();
    // Stack frame de get_ptr ya fue destruido
    printf("value: %d\n", *p);  // BUG: use-after-return
    return 0;
}
```

### Detección

Use-after-return no se detecta por defecto en ASan. Necesitas activarlo:

```bash
# Opción 1: variable de entorno
ASAN_OPTIONS=detect_stack_use_after_return=1 ./programa

# Opción 2: flag de compilación (Clang 15+)
clang -fsanitize=address -fsanitize-address-use-after-return=always ...
```

### Por qué no está activo por defecto

```
La detección de use-after-return funciona así:
1. Cada variable local se alloca en el HEAP en vez del stack
2. Al retornar, la región del heap se envenena
3. Si se accede → error

Costo:
- Cada llamada a función hace malloc() para sus locales
- Overhead de 2x-4x adicional
- No práctico para producción ni fuzzing largo
```

---

## 16. Bug 7: use-after-scope

### Descripción

Usar un puntero a una variable que salió de su scope (pero la función no retornó aún).

### En C

```c
#include <stdio.h>

int main() {
    int *p;

    {
        int x = 42;
        p = &x;
    }  // x sale de scope aquí

    // BUG: x ya no existe, pero p todavía apunta a su ubicación
    printf("value: %d\n", *p);  // use-after-scope

    return 0;
}
```

### Detección

```bash
# Activo por defecto con ASan en Clang 9+
clang -fsanitize=address -fno-omit-frame-pointer -g programa.c
```

### Reporte

```
==12345==ERROR: AddressSanitizer: stack-use-after-scope on address 0x7ffd12345abc
READ of size 4 at 0x7ffd12345abc thread T0
    #0 0x55a3... in main programa.c:11:28

Address 0x7ffd12345abc is located in stack of thread T0 at offset 32 in frame
    #0 0x55a3... in main programa.c:3

  This frame has 1 object(s):
    [32, 36) 'x' (line 6) <== Memory access at offset 32 is inside this variable
```

---

## 17. Bug 8: stack overflow (stack exhaustion)

### Descripción

Agotar el stack por recursión profunda. No es un "buffer overflow en el stack", sino agotar el **espacio total del stack**.

### En C

```c
void recursive(int n) {
    char buf[4096];  // 4 KB por frame
    buf[0] = n;
    recursive(n + 1);  // Recursión infinita → stack exhaustion
}

int main() {
    recursive(0);
    return 0;
}
```

### En Rust

```rust
fn recursive(n: u32) {
    let buf = [0u8; 4096];
    let _ = buf[0];
    recursive(n + 1);  // En debug: stack overflow
}

fn main() {
    recursive(0);
}
```

### Reporte de ASan

```
==12345==ERROR: AddressSanitizer: stack-overflow on address 0x7ffc00000000
    #0 0x55a3... in recursive src/main.rs:1:1
    #1 0x55a3... in recursive src/main.rs:4:5
    #2 0x55a3... in recursive src/main.rs:4:5
    ...
    #253 0x55a3... in recursive src/main.rs:4:5

SUMMARY: AddressSanitizer: stack-overflow src/main.rs:1:1 in recursive
```

### Nota

ASan detecta stack overflow, pero Rust safe también lo detecta (como panic). La ventaja de ASan es que proporciona un stack trace más detallado.

---

## 18. Bug 9: memory leaks (LeakSanitizer)

### Descripción

Memoria allocada que nunca se libera. ASan incluye LeakSanitizer (LSan) que se ejecuta al final del programa.

### En C

```c
#include <stdlib.h>

void create_and_leak() {
    int *p = (int *)malloc(100 * sizeof(int));
    // BUG: nunca se llama free(p)
    // p sale de scope → la memoria es inalcanzable → leak
}

int main() {
    for (int i = 0; i < 1000; i++) {
        create_and_leak();  // Leak de 400 bytes por iteración
    }
    return 0;
    // Al salir: LSan reporta 400,000 bytes leaked
}
```

### En Rust unsafe

```rust
fn create_and_leak() {
    let v = vec![0u8; 1000];
    let ptr = Box::into_raw(v.into_boxed_slice());
    // BUG: nunca se llama Box::from_raw(ptr)
    // ptr se pierde → memoria inalcanzable → leak
}

fn main() {
    for _ in 0..100 {
        create_and_leak();
    }
}
```

### Reporte de LSan

```
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 400000 byte(s) in 1000 object(s) allocated from:
    #0 0x55a3... in malloc
    #1 0x55a3... in create_and_leak programa.c:4:16

SUMMARY: AddressSanitizer: 400000 byte(s) leaked in 1000 allocation(s).
```

### Desactivar LSan

```bash
# Si los leaks no son relevantes para tu investigación:
ASAN_OPTIONS=detect_leaks=0 ./programa
```

---

## 19. Anatomía completa de un reporte ASan

Un reporte ASan tiene varias secciones. Veamos cada una en detalle:

```
=================================================================   ← separador
==12345==ERROR: AddressSanitizer: heap-buffer-overflow              ← línea 1: QUÉ
               on address 0x602000000054                            ← DÓNDE (dirección)
WRITE of size 4 at 0x602000000054 thread T0                        ← línea 2: TIPO DE ACCESO

    #0 0x55a3b2c4d5e6 in my_function src/lib.rs:42:15              ← stack trace del acceso
    #1 0x55a3b2c4d5e7 in caller src/lib.rs:30:5                    ← (de más reciente
    #2 0x55a3b2c4d5e8 in main src/main.rs:10:5                     ←  a más antigua)

0x602000000054 is located 4 bytes after 16-byte region              ← CONTEXTO
    [0x602000000040,0x602000000050)                                 ← de la allocación
allocated by thread T0 here:                                        ← DÓNDE se allocó
    #0 0x55a3b2c4d5e9 in __rust_alloc
    #1 0x55a3b2c4d5ea in my_function src/lib.rs:38:20

Shadow bytes around the buggy address:                              ← MAPA DE SHADOW
  0x0c047fff7fa0: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x0c047fff7fb0: fa fa fa fa fa fa fa fa 00 00[fa]fa fa fa fa fa   ← [] = byte buggy
                                               ^^
                                         right redzone
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07
  Heap left redzone:       fa
  Freed heap region:       fd
  ...

SUMMARY: AddressSanitizer: heap-buffer-overflow src/lib.rs:42:15   ← RESUMEN
==12345==ABORTING                                                   ← ACCIÓN
```

---

## 20. Leer el stack trace

### Formato de cada línea

```
#N 0xADDRESS in FUNCTION FILE:LINE:COLUMN
│   │             │        │    │    │
│   │             │        │    │    └── Columna en el archivo
│   │             │        │    └── Línea en el archivo
│   │             │        └── Archivo fuente
│   │             └── Nombre de la función
│   └── Dirección de la instrucción
└── Número de frame (0 = más reciente)
```

### Stack traces de ASan en Rust

```
    #0 0x55a3... in playground::process::h1234567890abcdef  src/lib.rs:42:15
                    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                    Nombre mangled de Rust
                    (playground::process con hash)

Para desmanglar: usa rustfilt o RUST_BACKTRACE=1
```

### Múltiples stack traces

```
Stack trace 1: "WRITE of size 4 at ..."
  → DÓNDE ocurrió el acceso ilegal
  → Leer de arriba a abajo: la función más cercana al bug está arriba

Stack trace 2: "allocated by thread T0 here:"
  → DÓNDE se allocó la memoria
  → Ayuda a entender qué objeto se desbordó

Stack trace 3 (solo en use-after-free): "freed by thread T0 here:"
  → DÓNDE se liberó la memoria
  → Ayuda a entender por qué el puntero es inválido
```

---

## 21. Leer la sección de memoria

### "is located X bytes after/before N-byte region"

```
"0x602000000054 is located 4 bytes after 16-byte region
    [0x602000000040,0x602000000050)"

Interpretación:
  Tu buffer: bytes 0x40 a 0x4F (16 bytes)
  Tu acceso: byte 0x54 (4 bytes después del final)

  0x40  0x44  0x48  0x4C  0x50  0x54
  [──── tu buffer (16 bytes) ────]
                                  ↑
                                  4 bytes después: AQUÍ escribiste
```

### Variaciones comunes

```
"0 bytes after 40-byte region"
  → Acceso JUSTO después del buffer (off-by-one clásico)

"4 bytes before 16-byte region"
  → Underflow: acceso ANTES del inicio del buffer

"0 bytes inside 40-byte region ... freed by"
  → Use-after-free: acceso a memoria ya liberada

"inside global variable 'BUFFER'"
  → Overflow de buffer global
```

---

## 22. Shadow bytes legend

Al final del reporte, ASan muestra los shadow bytes alrededor de la dirección del bug:

```
Shadow bytes around the buggy address:
  0x0c047fff7fa0: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x0c047fff7fb0: fa fa 00 00 00 00 00 fa fa fa 00 00 00 00 00 04
=>0x0c047fff7fc0: fa fa fa fa[fd]fd fd fd fd fa fa fa 00 00 00 00
                              ^^
                    Aquí está el byte problemático
```

### Leyenda

```
00          Completamente accesible (8 de 8 bytes)
01-07       Parcialmente accesible (1-7 de 8 bytes)
fa          Heap redzone (inserado por ASan alrededor de allocaciones)
fd          Freed memory (en quarantine)
f1          Stack left redzone
f2          Stack mid redzone
f3          Stack right redzone
f5          Stack after return
f8          Stack after scope
cb          Right alloca redzone
ca          Left alloca redzone
```

### Cómo leer el mapa

```
Ejemplo:
  ... 00 00 00 00 04 fa fa fa fd fd fd fd fd fa ...
      ↑──── buffer ────↑  ↑↑    ↑─── freed ────↑

  00 00 00 00: 4 × 8 = 32 bytes completamente accesibles
  04:          solo 4 de los últimos 8 bytes son accesibles
               Total del buffer: 32 + 4 = 36 bytes
  fa:          right redzone (envenenado)
  fd:          memoria que fue freed (en quarantine)
```

---

## 23. ASan en Rust unsafe: los mismos bugs

### Safe Rust previene estos bugs

```rust
// Safe Rust: IMPOSIBLE tener los bugs que ASan detecta

// Buffer overflow: bounds checking
let v = vec![1, 2, 3];
// v[10]; → panic (no UB), ASan no aporta nada

// Use-after-free: ownership
let s = String::from("hello");
let ptr = &s;
drop(s);
// println!("{}", ptr); → error de compilación: borrow checker

// Double-free: ownership
let b = Box::new(42);
drop(b);
// drop(b); → error de compilación: value moved

// Data race: Send/Sync
// No compila si intentas compartir sin sincronización
```

### Unsafe Rust: exactamente los mismos bugs que C

```rust
unsafe {
    // Buffer overflow: sin bounds check
    let ptr = std::alloc::alloc(layout);
    *ptr.add(beyond) = 42;  // ← ASan detecta

    // Use-after-free: raw pointers
    let ptr = Box::into_raw(b);
    drop(Box::from_raw(ptr));
    *ptr = 42;  // ← ASan detecta

    // Double-free: manual dealloc
    std::alloc::dealloc(ptr, layout);
    std::alloc::dealloc(ptr, layout);  // ← ASan detecta
}
```

### Patrón común en Rust: bug en la frontera safe/unsafe

```rust
pub struct MyVec {
    ptr: *mut u8,
    len: usize,
    cap: usize,
}

impl MyVec {
    // API safe que internamente usa unsafe
    pub fn push(&mut self, byte: u8) {
        if self.len >= self.cap {
            // BUG: olvidó hacer grow
            // self.grow();  ← FALTA
        }
        unsafe {
            // Si len >= cap, esto es buffer overflow
            *self.ptr.add(self.len) = byte;
        }
        self.len += 1;
    }
}

// El bug está en la LÓGICA del safe code,
// pero se manifiesta como UB en el unsafe block.
// ASan detecta el overflow.
```

---

## 24. Ejemplo completo en C: string_buffer

### Código con múltiples bugs

```c
/* string_buffer.h */
#ifndef STRING_BUFFER_H
#define STRING_BUFFER_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StringBuffer;

StringBuffer *sb_create(size_t initial_cap);
void sb_destroy(StringBuffer *sb);
void sb_append(StringBuffer *sb, const char *str);
void sb_append_char(StringBuffer *sb, char c);
char *sb_to_string(StringBuffer *sb);
void sb_clear(StringBuffer *sb);
char sb_char_at(StringBuffer *sb, size_t index);

#endif
```

```c
/* string_buffer.c */
#include "string_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

StringBuffer *sb_create(size_t initial_cap) {
    StringBuffer *sb = (StringBuffer *)malloc(sizeof(StringBuffer));
    if (!sb) return NULL;

    sb->data = (char *)malloc(initial_cap);
    if (!sb->data) {
        free(sb);
        return NULL;
    }

    sb->len = 0;
    sb->cap = initial_cap;
    sb->data[0] = '\0';
    return sb;
}

void sb_destroy(StringBuffer *sb) {
    if (!sb) return;
    free(sb->data);
    free(sb);
    /* BUG 1: no pone sb->data = NULL después de free.
       Si alguien usa sb después de destroy → use-after-free.
       Normalmente free(sb) haría que sb sea inválido,
       pero si alguien guardó una copia del puntero... */
}

void sb_append(StringBuffer *sb, const char *str) {
    size_t str_len = strlen(str);

    /* BUG 2: la condición de grow es off-by-one.
       Debería ser >= en vez de >
       sb->len + str_len + 1 >= sb->cap (necesita espacio para '\0')
       pero usa: */
    if (sb->len + str_len > sb->cap) {
        size_t new_cap = (sb->cap * 2 > sb->len + str_len + 1)
                         ? sb->cap * 2
                         : sb->len + str_len + 1;
        char *new_data = (char *)realloc(sb->data, new_cap);
        if (!new_data) return;  /* silently fail */
        sb->data = new_data;
        sb->cap = new_cap;
    }

    memcpy(sb->data + sb->len, str, str_len);
    sb->len += str_len;
    sb->data[sb->len] = '\0';
    /* Cuando sb->len + str_len == sb->cap exactamente,
       la condición > es false, no se hace realloc,
       pero sb->data[sb->len] escribe 1 byte más allá del buffer.
       → heap-buffer-overflow (off-by-one) */
}

void sb_append_char(StringBuffer *sb, char c) {
    char str[2] = {c, '\0'};
    sb_append(sb, str);
}

char *sb_to_string(StringBuffer *sb) {
    /* BUG 3: retorna el puntero interno.
       Si el caller hace free() del resultado,
       sb->data queda como dangling pointer → use-after-free.
       Y si sb_destroy() después hace free(sb->data) → double-free */
    return sb->data;
}

void sb_clear(StringBuffer *sb) {
    sb->len = 0;
    sb->data[0] = '\0';
}

char sb_char_at(StringBuffer *sb, size_t index) {
    /* BUG 4: no verifica que index < len */
    return sb->data[index];
    /* Si index >= cap → heap-buffer-overflow (lectura) */
}
```

### Test que activa los bugs

```c
/* test_string_buffer.c */
#include "string_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void test_bug2_off_by_one() {
    /* Crear buffer con capacidad exacta para "hello" + '\0' = 6 */
    StringBuffer *sb = sb_create(6);
    sb_append(sb, "hello");  /* len=5, cap=6, data[5]='\0' → OK */
    sb_append(sb, "!");      /* len=5+1=6, 5+1 > 6 es FALSE → no realloc
                                data[6] = '\0' → OVERFLOW (escribe en byte 6, cap=6) */
    sb_destroy(sb);
}

void test_bug3_double_free() {
    StringBuffer *sb = sb_create(64);
    sb_append(sb, "hello world");

    char *str = sb_to_string(sb);  /* retorna sb->data directamente */
    printf("String: %s\n", str);

    free(str);       /* free del puntero interno */
    sb_destroy(sb);  /* sb_destroy llama free(sb->data) de nuevo → DOUBLE-FREE */
}

void test_bug4_oob_read() {
    StringBuffer *sb = sb_create(8);
    sb_append(sb, "hi");  /* len=2 */

    char c = sb_char_at(sb, 100);  /* 100 >> cap → heap-buffer-overflow READ */
    printf("char: %c\n", c);

    sb_destroy(sb);
}

int main() {
    printf("=== Test bug 2: off-by-one ===\n");
    test_bug2_off_by_one();

    printf("=== Test bug 3: double-free ===\n");
    test_bug3_double_free();

    printf("=== Test bug 4: OOB read ===\n");
    test_bug4_oob_read();

    return 0;
}
```

### Compilar y ejecutar

```bash
clang -fsanitize=address -g -O1 -o test_sb \
    string_buffer.c test_string_buffer.c

./test_sb
# ASan reportará el primer bug encontrado y abortará
```

---

## 25. Ejemplo completo en Rust unsafe: raw_vec

### Código con bugs

```rust
// src/lib.rs
use std::alloc::{alloc, dealloc, realloc, Layout};
use std::ptr;

pub struct RawVec<T> {
    ptr: *mut T,
    len: usize,
    cap: usize,
}

impl<T: Copy> RawVec<T> {
    pub fn new() -> Self {
        RawVec {
            ptr: ptr::null_mut(),
            len: 0,
            cap: 0,
        }
    }

    pub fn with_capacity(cap: usize) -> Self {
        if cap == 0 {
            return Self::new();
        }
        let layout = Layout::array::<T>(cap).unwrap();
        let ptr = unsafe { alloc(layout) as *mut T };
        if ptr.is_null() {
            std::alloc::handle_alloc_error(layout);
        }
        RawVec { ptr, len: 0, cap }
    }

    pub fn push(&mut self, value: T) {
        if self.len == self.cap {
            self.grow();
        }
        unsafe {
            ptr::write(self.ptr.add(self.len), value);
        }
        self.len += 1;
    }

    pub fn pop(&mut self) -> Option<T> {
        if self.len == 0 {
            return None;
        }
        self.len -= 1;
        unsafe {
            Some(ptr::read(self.ptr.add(self.len)))
        }
    }

    pub fn get(&self, index: usize) -> Option<&T> {
        if index >= self.len {
            return None;
        }
        unsafe {
            Some(&*self.ptr.add(index))
        }
    }

    // BUG 1: get_unchecked sin verificar bounds
    pub unsafe fn get_unchecked(&self, index: usize) -> &T {
        &*self.ptr.add(index)  // Si index >= cap → heap-buffer-overflow
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    fn grow(&mut self) {
        let new_cap = if self.cap == 0 { 4 } else { self.cap * 2 };
        let new_layout = Layout::array::<T>(new_cap).unwrap();

        let new_ptr = if self.cap == 0 {
            unsafe { alloc(new_layout) as *mut T }
        } else {
            let old_layout = Layout::array::<T>(self.cap).unwrap();
            unsafe {
                realloc(self.ptr as *mut u8, old_layout, new_layout.size()) as *mut T
            }
        };

        if new_ptr.is_null() {
            std::alloc::handle_alloc_error(new_layout);
        }

        self.ptr = new_ptr;
        self.cap = new_cap;
    }

    // BUG 2: as_ptr retorna puntero que puede quedar dangling
    pub fn as_ptr(&self) -> *const T {
        self.ptr
    }

    // BUG 3: into_raw_parts consume el vec pero no previene double-free
    pub fn into_raw_parts(self) -> (*mut T, usize, usize) {
        let parts = (self.ptr, self.len, self.cap);
        std::mem::forget(self);  // No ejecutar Drop
        parts
    }
}

impl<T> Drop for RawVec<T> {
    fn drop(&mut self) {
        if !self.ptr.is_null() && self.cap > 0 {
            let layout = Layout::array::<T>(self.cap).unwrap();
            unsafe {
                dealloc(self.ptr as *mut u8, layout);
            }
        }
    }
}

// BUG 4: Clone que no alloca nueva memoria
impl<T: Copy> Clone for RawVec<T> {
    fn clone(&self) -> Self {
        // INCORRECTO: copia el puntero en vez de los datos
        // → double-free cuando ambos se dropean
        RawVec {
            ptr: self.ptr,
            len: self.len,
            cap: self.cap,
        }
    }
}
```

### Tests con ASan

```rust
// tests/asan_tests.rs

#[test]
fn test_bug1_get_unchecked_oob() {
    let mut v = raw_vec::RawVec::<u32>::with_capacity(4);
    v.push(1);
    v.push(2);
    // len=2, cap=4
    unsafe {
        let _ = v.get_unchecked(10);  // 10 > cap → heap-buffer-overflow
    }
}

#[test]
fn test_bug2_dangling_ptr() {
    let mut v = raw_vec::RawVec::<u32>::with_capacity(2);
    v.push(1);
    let ptr = v.as_ptr();

    // Grow realloca → ptr es dangling
    v.push(2);
    v.push(3);  // grow() realloca

    unsafe {
        let _ = *ptr;  // use-after-free: ptr apunta a memoria ya reallocada
    }
}

#[test]
fn test_bug4_clone_double_free() {
    let mut v = raw_vec::RawVec::<u32>::with_capacity(4);
    v.push(1);
    v.push(2);

    let _clone = v.clone();
    // Al final del scope: v.drop() + _clone.drop()
    // Ambos llaman dealloc con el mismo puntero → double-free
}
```

### Ejecutar con ASan

```bash
RUSTFLAGS="-Zsanitizer=address" cargo +nightly test
```

---

## 26. ASan con tests unitarios

### Por qué ejecutar tests con ASan

```
Tests unitarios normales detectan: errores lógicos, panics
Tests unitarios con ASan detectan:  TODO lo anterior + bugs de memoria

Si tu código tiene unsafe, ejecutar tests con ASan es gratuito
(solo más lento) y puede encontrar bugs que los tests solos no ven.
```

### En C

```bash
# Compilar tests con ASan
clang -fsanitize=address -g -O1 -o test_runner \
    tests/*.c src/*.c -I include/

# Ejecutar
./test_runner
```

### En Rust

```bash
# Ejecutar TODOS los tests con ASan
RUSTFLAGS="-Zsanitizer=address" cargo +nightly test

# Ejecutar un test específico con ASan
RUSTFLAGS="-Zsanitizer=address" cargo +nightly test test_name

# Con backtrace
RUST_BACKTRACE=1 RUSTFLAGS="-Zsanitizer=address" cargo +nightly test
```

### Integrar en CI

```yaml
# .github/workflows/asan-tests.yml
name: Tests with ASan

on: [push, pull_request]

jobs:
  asan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@nightly
      - name: Run tests with ASan
        run: |
          RUSTFLAGS="-Zsanitizer=address" \
          cargo +nightly test --target x86_64-unknown-linux-gnu
```

---

## 27. ASan con fuzzing (repaso integrado)

### En C (AFL++)

```bash
# Compilar con ASan + AFL++
AFL_USE_ASAN=1 afl-cc -o fuzz_target harness.c target.c

# O manualmente:
afl-cc -fsanitize=address -o fuzz_target harness.c target.c

# Ejecutar
afl-fuzz -i corpus -o output -- ./fuzz_target @@
```

### En C (libFuzzer)

```bash
# Compilar con ASan + libFuzzer
clang -fsanitize=address,fuzzer -g -O1 -o fuzz_target \
    harness.c target.c

# Ejecutar
./fuzz_target corpus/
```

### En Rust (cargo-fuzz)

```bash
# ASan es el default
cargo fuzz run my_target

# Explícito:
cargo fuzz run my_target --sanitizer=address
```

### La sinergia fuzzer + ASan

```
Sin ASan:
  El fuzzer genera input → buffer overflow silencioso
  → el programa continúa con datos corruptos
  → quizás crashea más tarde por SEGV (en un lugar irrelevante)
  → o no crashea nunca

Con ASan:
  El fuzzer genera input → buffer overflow
  → ASan: "HEAP-BUFFER-OVERFLOW en parse.c:42"
  → crash inmediato con stack trace preciso
  → input guardado como artifact
  → bug encontrado con ubicación exacta
```

---

## 28. ASAN_OPTIONS: configuración completa

### Referencia de opciones

| Opción | Default | Descripción |
|---|---|---|
| `detect_leaks` | 1 (Linux) | Activar detección de leaks (LSan) |
| `halt_on_error` | 1 | Abortar en el primer error |
| `abort_on_error` | 0 | Usar abort() en vez de _exit() |
| `symbolize` | 1 | Simbolizar stack traces |
| `print_stats` | 0 | Imprimir estadísticas de memoria al final |
| `quarantine_size_mb` | 256 | Tamaño de quarantine para freed mem |
| `malloc_context_size` | 30 | Profundidad de stack traces en malloc/free |
| `detect_stack_use_after_return` | 0 | Detectar use-after-return (costoso) |
| `check_initialization_order` | 0 | Verificar orden de inicialización de globals |
| `strict_init_order` | 0 | Abortar si hay init order violation |
| `detect_odr_violation` | 2 | Detectar One Definition Rule violations |
| `max_malloc_fill_size` | 0 | Llenar allocaciones con 0xbe (para debug) |
| `sleep_before_dying` | 0 | Segundos antes de abortar (para attach debugger) |
| `exitcode` | 1 | Código de salida en error |
| `log_path` | stderr | Redirigir reportes a archivo |
| `suppressions` | "" | Archivo de suppressions |
| `verbosity` | 0 | Nivel de detalle (0-2) |
| `fast_unwind_on_fatal` | 0 | Unwinding rápido pero menos preciso |
| `fast_unwind_on_malloc` | 1 | Unwinding rápido para stack traces de malloc |

### Configuraciones recomendadas

```bash
# Para desarrollo local:
ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:symbolize=1" ./programa

# Para fuzzing:
ASAN_OPTIONS="detect_leaks=1:abort_on_error=1:symbolize=1" \
    cargo fuzz run my_target

# Para debugging un crash específico:
ASAN_OPTIONS="halt_on_error=0:sleep_before_dying=30" ./programa crash_input
# → Puedes attachar gdb durante los 30 segundos

# Para CI (mínimo output):
ASAN_OPTIONS="detect_leaks=1:log_path=/tmp/asan.log" cargo +nightly test

# Para reducir uso de RAM:
ASAN_OPTIONS="quarantine_size_mb=64:malloc_context_size=10" ./programa
```

---

## 29. Impacto en rendimiento detallado

### CPU

```
Overhead de CPU por tipo de operación:

Load/Store instrumentado:  ~2-3 instrucciones extra por acceso
  → Overhead promedio: 73% más lento (paper original)
  → En práctica: 1.5x a 3x dependiendo del código

malloc/free:
  → malloc: ~20% más lento (por redzones y shadow)
  → free:   ~30% más lento (por quarantine y envenenar shadow)
  → Código con muchas allocaciones: más overhead

Stack operations:
  → Envenenar/desenvenenar redzones al entrar/salir de funciones
  → Overhead: ~10% adicional
```

### RAM

```
Overhead de RAM:

Shadow memory:         1/8 del espacio de direcciones mapeado
  → En práctica: ~2x-3x la RAM del programa

Redzones:
  Heap: 16 bytes antes + 16 bytes después de cada allocación
  Stack: 32 bytes de redzone entre cada variable local
  → Si tienes muchas allocaciones pequeñas: overhead significativo

Quarantine:
  Default: 256 MB de memoria freed retenida
  → Ajustar con quarantine_size_mb si causa OOM

Ejemplo práctico:
  Programa usa 100 MB sin ASan
  → Con ASan: ~250-350 MB
```

### Tabla resumen

| Métrica | Sin ASan | Con ASan | Ratio |
|---|---|---|---|
| CPU time | 1.0x | 1.5-3.0x | ~2x promedio |
| Peak RAM | 1.0x | 2.0-3.5x | ~2.5x promedio |
| Binario | 1.0x | 2.0-3.0x | Más código de check |
| Stack frame | 1.0x | 1.5-2.0x | Redzones |
| malloc(8) | 8 bytes | 40+ bytes | Redzones + metadata |

---

## 30. Falsos positivos y cómo manejarlos

### ASan tiene falsos positivos MUY raros

A diferencia de otros sanitizers, ASan tiene una tasa de falsos positivos extremadamente baja. Si ASan reporta un error, **casi siempre es un bug real**.

### Los raros falsos positivos

| Situación | Causa | Solución |
|---|---|---|
| Custom allocator | ASan no sabe de tu allocator | `__asan_poison_memory_region` / `__asan_unpoison_memory_region` |
| Memory-mapped I/O | Acceso a direcciones especiales | Suppression |
| Assembly inline | ASan no instrumenta asm | Suppression |
| Allocator mismatch | malloc de una lib, free de otra | Investigar la causa |
| Signal handlers | Acceso durante signal handling | Configurar signal-safe |

### Funciones de ASan para custom allocators

```c
#include <sanitizer/asan_interface.h>

void *my_alloc(size_t size) {
    void *ptr = internal_alloc(size + 2 * REDZONE);

    // Decirle a ASan: esta región NO es accesible
    __asan_poison_memory_region(ptr, REDZONE);
    __asan_poison_memory_region(ptr + REDZONE + size, REDZONE);

    // Decirle a ASan: esta región SÍ es accesible
    __asan_unpoison_memory_region(ptr + REDZONE, size);

    return ptr + REDZONE;
}

void my_free(void *ptr) {
    void *real_ptr = ptr - REDZONE;

    // Envenenar toda la región
    __asan_poison_memory_region(real_ptr, actual_size);

    internal_free(real_ptr);
}
```

---

## 31. Suppressions

### Cuándo usar suppressions

```
Suprimir cuando:
  - Falso positivo confirmado
  - Bug en dependencia que no controlas
  - Bug conocido con fix pendiente upstream
  - Bug en runtime/stdlib que no es tuyo

NUNCA suprimir:
  - Sin investigar primero
  - Bugs en tu código
  - "Para que el CI pase"
```

### Archivo de suppression

```
# asan_suppressions.txt

# Leak en la inicialización del runtime
leak:__libc_start_main
leak:std::rt::init

# Bug conocido en librería de terceros
heap-buffer-overflow:third_party_lib::parse_internal

# Falso positivo con custom allocator
alloc-dealloc-mismatch:my_custom_allocator
```

### Usar suppressions

```bash
# Para leaks (LSAN_OPTIONS):
LSAN_OPTIONS="suppressions=lsan_suppressions.txt" ./programa

# Para otros errores de ASan:
# No hay ASAN_OPTIONS para suppressions de bugs no-leak.
# En cambio, usar __attribute__((no_sanitize("address"))) en el código:
```

```c
// Desactivar ASan para una función específica (C):
__attribute__((no_sanitize("address")))
void function_with_known_false_positive() {
    // ...
}
```

```rust
// En Rust (nightly):
#[cfg_attr(sanitize = "address", no_sanitize(address))]
fn function_with_known_issue() {
    // ...
}
```

---

## 32. ASan vs Valgrind

### Tabla comparativa

| Aspecto | ASan | Valgrind (Memcheck) |
|---|---|---|
| **Mecanismo** | Instrumentación del compilador | Emulación binaria (DBI) |
| **Recompilación** | Sí (necesita -fsanitize) | No (ejecutable sin modificar) |
| **Overhead CPU** | ~2x | ~20-50x |
| **Overhead RAM** | ~2-3x | ~2x |
| **Heap overflow** | Sí | Sí |
| **Stack overflow** | Sí | No |
| **Global overflow** | Sí | No |
| **Use-after-free** | Sí | Sí |
| **Double-free** | Sí | Sí |
| **Uninitialized read** | No (→ MSan) | Sí |
| **Memory leaks** | Sí (LSan) | Sí |
| **Precisión** | Muy alta | Muy alta |
| **Falsos positivos** | Casi nulos | Raros |
| **Plataformas** | Linux, macOS, Windows | Linux, macOS |
| **Lenguajes** | C, C++, Rust, Go | Cualquiera |
| **Integración fuzzer** | Nativa (-fsanitize=fuzzer) | No directa |
| **CI** | Fácil (recompilar) | Fácil (no recompilar) |

### Cuándo usar cada uno

```
Usar ASan cuando:
  ✓ Puedes recompilar
  ✓ Necesitas velocidad (testing, fuzzing, CI)
  ✓ Necesitas detectar stack/global overflows
  ✓ Integras con fuzzer (libFuzzer, AFL++)

Usar Valgrind cuando:
  ✓ No puedes recompilar (binario de terceros)
  ✓ Necesitas detectar reads no inicializados (Memcheck)
  ✓ Necesitas profiling de memoria (Massif)
  ✓ Necesitas análisis de cache (Cachegrind)
  ✓ La velocidad no importa (ejecución manual)
```

### ¿Pueden complementarse?

Sí: ASan encuentra bugs que Valgrind no (stack/global overflow), y Valgrind encuentra bugs que ASan no (reads no inicializados). Ejecutar ambos en tu CI da la mejor cobertura.

---

## 33. Limitaciones de ASan

### Qué NO detecta ASan

```
┌──────────────────────────────────────────────────────────────┐
│                  Limitaciones de ASan                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  1. Overflow DENTRO de un struct                             │
│     struct { int a[10]; int b; };                            │
│     a[10] = 42;  // Escribe en b, no en redzone             │
│     → ASan NO lo detecta (no hay redzone entre a y b)       │
│                                                              │
│  2. Overflow que salta el redzone                            │
│     ptr[20] = 42;  // Si 20 salta el redzone de 16 bytes    │
│     → Puede caer en otra allocación accesible                │
│     → ASan NO lo detecta                                    │
│     (raro, pero teóricamente posible)                        │
│                                                              │
│  3. Lectura de memoria no inicializada                       │
│     → Usar MSan para esto                                   │
│                                                              │
│  4. Data races                                               │
│     → Usar TSan para esto                                    │
│                                                              │
│  5. Integer overflow y UB aritmético                         │
│     → Usar UBSan para esto (C)                              │
│     → Rust debug_assertions para overflow                    │
│                                                              │
│  6. Logic bugs                                               │
│     → ASan solo detecta bugs de MEMORIA                     │
│     → Para lógica: tests unitarios, fuzzing con assertions   │
│                                                              │
│  7. Use-after-free si quarantine ya recicló                  │
│     → Si mucho tiempo pasó entre free y use,                 │
│       la quarantine puede haber reciclado la memoria         │
│     → Aumentar quarantine_size_mb para más detección         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 34. Errores comunes

### Error 1: compilar sin -g

```bash
# ❌ Sin debug info: stack traces sin archivos/líneas
clang -fsanitize=address -O1 -o programa programa.c
# Reporte: #0 0x55a3b2c4d5e6 in ?? ← INÚTIL

# ✓ Con debug info:
clang -fsanitize=address -g -O1 -o programa programa.c
# Reporte: #0 0x55a3b2c4d5e6 in my_func src/lib.c:42:15 ← ÚTIL
```

### Error 2: mezclar código con y sin ASan

```bash
# ❌ Compilar la librería sin ASan pero el ejecutable con ASan
clang -c -O2 lib.c -o lib.o                    # SIN ASan
clang -fsanitize=address main.c lib.o -o prog   # CON ASan

# Resultado: bugs en lib.c NO se detectan
# Las funciones de lib.c no tienen instrumentación

# ✓ Todo con ASan:
clang -fsanitize=address -c -O2 lib.c -o lib.o
clang -fsanitize=address main.c lib.o -o prog
```

### Error 3: ignorar el reporte porque "el programa funciona"

```
"El programa produce la salida correcta, así que el reporte de ASan
es un falso positivo"

INCORRECTO. Un heap-buffer-overflow puede:
- Leer datos correctos POR CASUALIDAD (la memoria adyacente tiene el valor esperado)
- Funcionar en tu máquina pero crashear en producción
- Ser explotable por un atacante

Si ASan reporta un error → ES UN BUG. Siempre.
```

### Error 4: -O3 con ASan

```bash
# ❌ Optimización agresiva puede eliminar código con bugs
clang -fsanitize=address -O3 programa.c

# El optimizador puede:
# - Eliminar stores que considera "muertos" (pero que causan overflow)
# - Reordenar accesos a memoria
# - Combinar accesos adyacentes

# ✓ Usar -O1 o -Og
clang -fsanitize=address -O1 programa.c
```

### Error 5: no usar -fno-omit-frame-pointer

```bash
# ❌ Sin frame pointers: stack traces incompletos
clang -fsanitize=address -g -O2 programa.c

# Reporte puede tener:
# #0 0x55a3... in func1
# #1 0x55a3... in ??       ← frame perdido
# #2 0x55a3... in main

# ✓ Con frame pointers:
clang -fsanitize=address -g -O1 -fno-omit-frame-pointer programa.c
```

### Error 6: ASan en producción

```
NO usar ASan en producción:
  - 2-3x más lento
  - 2-3x más RAM
  - Shadow memory ocupa espacio de direcciones
  - Quarantine crece continuamente

ASan es para:
  - Desarrollo local
  - Tests (cargo test con ASan)
  - Fuzzing
  - CI

Para producción: usar allocators hardened (como scudo)
o hardware features (MTE en ARM).
```

---

## 35. Programa de práctica: memory_pool

### Objetivo

Implementar un memory pool (allocator de bloques fijos) en C y Rust unsafe, y usar ASan para encontrar todos los bugs.

### Especificación

```
memory_pool/
├── c/
│   ├── pool.h            ← API del pool
│   ├── pool.c            ← implementación con bugs
│   ├── test_pool.c       ← tests que activan los bugs
│   └── Makefile
└── rust/
    ├── Cargo.toml
    ├── src/
    │   └── lib.rs         ← implementación con bugs
    └── tests/
        └── asan_tests.rs  ← tests que activan los bugs
```

### pool.h (C)

```c
#ifndef POOL_H
#define POOL_H

#include <stddef.h>
#include <stdint.h>

typedef struct MemoryPool MemoryPool;

/* Crear un pool con 'count' bloques de 'block_size' bytes. */
MemoryPool *pool_create(size_t block_size, size_t count);

/* Destruir el pool y liberar toda la memoria. */
void pool_destroy(MemoryPool *pool);

/* Allocar un bloque del pool. Retorna NULL si no hay bloques libres. */
void *pool_alloc(MemoryPool *pool);

/* Retornar un bloque al pool. */
void pool_free(MemoryPool *pool, void *ptr);

/* Cuántos bloques están en uso. */
size_t pool_used(const MemoryPool *pool);

/* Cuántos bloques están disponibles. */
size_t pool_available(const MemoryPool *pool);

/* Total de bloques. */
size_t pool_capacity(const MemoryPool *pool);

#endif
```

### pool.c (C con bugs)

```c
#include "pool.h"
#include <stdlib.h>
#include <string.h>

struct MemoryPool {
    uint8_t *memory;       /* bloque de memoria contiguo */
    uint8_t *free_list;    /* lista enlazada de bloques libres */
    size_t block_size;
    size_t count;
    size_t used;
};

MemoryPool *pool_create(size_t block_size, size_t count) {
    if (block_size < sizeof(void *)) {
        block_size = sizeof(void *);  /* mínimo para almacenar next ptr */
    }

    MemoryPool *pool = (MemoryPool *)malloc(sizeof(MemoryPool));
    if (!pool) return NULL;

    pool->memory = (uint8_t *)malloc(block_size * count);
    if (!pool->memory) {
        free(pool);
        return NULL;
    }

    pool->block_size = block_size;
    pool->count = count;
    pool->used = 0;

    /* Inicializar free list: cada bloque apunta al siguiente */
    pool->free_list = pool->memory;
    for (size_t i = 0; i < count - 1; i++) {
        uint8_t *current = pool->memory + i * block_size;
        uint8_t *next = pool->memory + (i + 1) * block_size;
        *(uint8_t **)current = next;
    }
    /* Último bloque apunta a NULL */
    uint8_t *last = pool->memory + (count - 1) * block_size;
    *(uint8_t **)last = NULL;

    return pool;
}

void pool_destroy(MemoryPool *pool) {
    if (!pool) return;
    free(pool->memory);
    free(pool);
    /* BUG 1: no pone pool->memory = NULL.
       Si alguien tiene una copia del puntero pool
       y llama pool_alloc → use-after-free */
}

void *pool_alloc(MemoryPool *pool) {
    if (!pool->free_list) return NULL;

    uint8_t *block = pool->free_list;
    pool->free_list = *(uint8_t **)block;
    pool->used++;

    return block;
}

void pool_free(MemoryPool *pool, void *ptr) {
    /* BUG 2: no verifica que ptr pertenece al pool.
       Si ptr no está en [pool->memory, pool->memory + block_size * count):
       → escritura a dirección arbitraria (escribe next ptr) */

    /* BUG 3: no verifica double-free.
       Si ptr ya está en la free list y se libera de nuevo:
       → la free list se corrompe (loop o nodo duplicado) */

    uint8_t *block = (uint8_t *)ptr;
    *(uint8_t **)block = pool->free_list;
    pool->free_list = block;
    pool->used--;
    /* BUG 4: used puede ir a underflow si double-free
       (wrapping underflow en size_t) */
}

size_t pool_used(const MemoryPool *pool) {
    return pool->used;
}

size_t pool_available(const MemoryPool *pool) {
    return pool->count - pool->used;
}

size_t pool_capacity(const MemoryPool *pool) {
    return pool->count;
}
```

### Bugs intencionales resumidos

```
Bug 1: pool_destroy no previene use-after-free
Bug 2: pool_free no valida que ptr pertenezca al pool
       → escritura a dirección arbitraria
Bug 3: pool_free no detecta double-free
       → free list corrupta
Bug 4: pool_free decrementa used sin verificar
       → underflow de size_t
```

### Tareas

1. **Implementa en C**: compila con ASan y escribe tests que activen cada bug

2. **Implementa en Rust unsafe**: la misma API con los mismos bugs

3. **Para cada bug en C**:
   ```bash
   clang -fsanitize=address -g -O1 -o test_pool pool.c test_pool.c
   ./test_pool
   ```
   - ¿ASan detecta cada bug? ¿Qué tipo de reporte genera?

4. **Para cada bug en Rust**:
   ```bash
   RUSTFLAGS="-Zsanitizer=address" cargo +nightly test
   ```

5. **Corrige** cada bug y verifica que ASan ya no reporta

6. **Bonus**: escribe un harness de fuzzing (cargo-fuzz) con secuencias de alloc/free

---

## 36. Ejercicios

### Ejercicio 1: interpretar reportes ASan

**Objetivo**: Aprender a leer y diagnosticar reportes ASan.

**Tareas**:

**a)** Dado el siguiente reporte, responde:
   - ¿Qué tipo de bug es?
   - ¿Dónde ocurrió el acceso ilegal?
   - ¿Cuántos bytes se excedieron?
   - ¿Dónde se allocó la memoria?

```
==99==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x60200000001a
WRITE of size 1 at 0x60200000001a thread T0
    #0 0x4c3e5f in process_input src/parser.c:87:23
    #1 0x4c4a12 in main src/main.c:15:5

0x60200000001a is located 2 bytes after 8-byte region
    [0x602000000010,0x602000000018)
allocated by thread T0 here:
    #0 0x493d2d in malloc
    #1 0x4c3d80 in process_input src/parser.c:82:18
```

**b)** Dado este reporte, responde:
   - ¿Cuántas veces se liberó la memoria?
   - ¿Dónde fue cada free?
   - ¿Dónde se allocó originalmente?

```
==99==ERROR: AddressSanitizer: attempting double-free on 0x602000000010
    #0 0x493b5d in free
    #1 0x4c3f10 in cleanup src/manager.c:45:5
    #2 0x4c4b00 in main src/main.c:22:5

0x602000000010 is located 0 bytes inside of 32-byte region
freed by thread T0 here:
    #0 0x493b5d in free
    #1 0x4c3e80 in process src/manager.c:30:5
    #2 0x4c4a50 in main src/main.c:18:5

previously allocated by thread T0 here:
    #0 0x493d2d in malloc
    #1 0x4c3d00 in init src/manager.c:10:18
    #2 0x4c4a20 in main src/main.c:12:5
```

---

### Ejercicio 2: encontrar bugs con ASan

**Objetivo**: Usar ASan para encontrar todos los bugs en un programa dado.

**Tareas**:

**a)** Compila y ejecuta el siguiente programa con ASan:

```c
#include <stdlib.h>
#include <string.h>

char *duplicate(const char *src) {
    size_t len = strlen(src);
    char *dst = (char *)malloc(len);  /* Bug: falta +1 para '\0' */
    strcpy(dst, src);                  /* Overflow por 1 byte */
    return dst;
}

char *concat(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    char *result = (char *)malloc(la + lb + 1);
    memcpy(result, a, la);
    memcpy(result + la, b, lb + 1);
    return result;
}

int main() {
    char *s1 = duplicate("hello");
    char *s2 = concat(s1, " world");

    free(s1);
    printf("Result: %s\n", s2);  /* ¿Hay bug aquí? */

    free(s2);
    free(s1);  /* ¿Hay bug aquí? */

    return 0;
}
```

**b)** ¿Cuántos bugs hay? Lista cada uno con el tipo de error ASan.

**c)** Corrige todos los bugs. Verifica con ASan que ya no hay errores.

---

### Ejercicio 3: ASan en Rust unsafe

**Objetivo**: Escribir y fuzzear código unsafe con ASan.

**Tareas**:

**a)** Implementa un `FixedString` en Rust unsafe:
   ```rust
   pub struct FixedString {
       buf: *mut u8,
       len: usize,
       cap: usize,  // siempre fijo, no crece
   }
   ```
   Con operaciones: `new(cap)`, `push_str(&str)`, `as_str()`, `clear()`, `drop`.

**b)** Introduce 2 bugs:
   - `push_str` no verifica que hay espacio suficiente
   - `drop` no pone `buf` a null (permite use-after-drop)

**c)** Escribe tests unitarios que activen cada bug.

**d)** Ejecuta con ASan:
   ```bash
   RUSTFLAGS="-Zsanitizer=address" cargo +nightly test
   ```

**e)** Escribe un harness de fuzzing con Arbitrary y ejecútalo con ASan.

---

### Ejercicio 4: comparar ASan en C y Rust

**Objetivo**: Experimentar las diferencias de experiencia.

**Tareas**:

**a)** Implementa el MISMO linked list en C y Rust unsafe:
   ```
   Operaciones: create, push_front, pop_front, peek, destroy
   ```

**b)** Introduce el MISMO bug en ambos: use-after-free en `pop_front` (retorna puntero al nodo después de liberarlo).

**c)** Compila ambos con ASan y ejecuta.

**d)** Compara los reportes:
   - ¿Son igualmente informativos?
   - ¿El stack trace de Rust es más difícil de leer? (name mangling)
   - ¿ASan detecta el bug en ambos de la misma forma?

**e)** Corrige el bug en ambos. ¿Cuál fue más fácil de corregir?

---

## Navegación

- **Anterior**: [S02/T04 - Integrar fuzzing en workflow](../../S02_Fuzzing_en_Rust/T04_Integrar_fuzzing_en_workflow/README.md)
- **Siguiente**: [T02 - MemorySanitizer (MSan)](../T02_MemorySanitizer/README.md)

---

> **Sección 3: Sanitizers como Red de Seguridad** — Tópico 1 de 4 completado
