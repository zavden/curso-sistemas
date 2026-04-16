# T02 - MemorySanitizer (MSan): lectura de memoria no inicializada, solo Clang/nightly Rust

## Índice

1. [Qué es MemorySanitizer](#1-qué-es-memorysanitizer)
2. [Historia y contexto](#2-historia-y-contexto)
3. [El problema de la memoria no inicializada](#3-el-problema-de-la-memoria-no-inicializada)
4. [Arquitectura de MSan](#4-arquitectura-de-msan)
5. [Shadow memory en MSan: mapeo 1:1 bit-preciso](#5-shadow-memory-en-msan-mapeo-11-bit-preciso)
6. [Propagación de estado: la clave de MSan](#6-propagación-de-estado-la-clave-de-msan)
7. [Origin tracking: de dónde viene el problema](#7-origin-tracking-de-dónde-viene-el-problema)
8. [Compilar con MSan en C](#8-compilar-con-msan-en-c)
9. [El requisito de -Zbuild-std en Rust](#9-el-requisito-de--zbuild-std-en-rust)
10. [Compilar con MSan en Rust](#10-compilar-con-msan-en-rust)
11. [Bug 1: variable local no inicializada](#11-bug-1-variable-local-no-inicializada)
12. [Bug 2: campo de struct no inicializado](#12-bug-2-campo-de-struct-no-inicializado)
13. [Bug 3: malloc sin inicializar](#13-bug-3-malloc-sin-inicializar)
14. [Bug 4: lectura parcial de buffer](#14-bug-4-lectura-parcial-de-buffer)
15. [Bug 5: uso condicional basado en valor no inicializado](#15-bug-5-uso-condicional-basado-en-valor-no-inicializado)
16. [Bug 6: pasar valor no inicializado a función](#16-bug-6-pasar-valor-no-inicializado-a-función)
17. [Bug 7: retornar valor no inicializado](#17-bug-7-retornar-valor-no-inicializado)
18. [Bug 8: no inicializado en operación aritmética](#18-bug-8-no-inicializado-en-operación-aritmética)
19. [Bug 9: memoria no inicializada vía syscall](#19-bug-9-memoria-no-inicializada-vía-syscall)
20. [Anatomía completa de un reporte MSan](#20-anatomía-completa-de-un-reporte-msan)
21. [Leer el origin trace](#21-leer-el-origin-trace)
22. [MSan en Rust unsafe: MaybeUninit y ptr](#22-msan-en-rust-unsafe-maybeuninit-y-ptr)
23. [Ejemplo completo en C: config_parser](#23-ejemplo-completo-en-c-config_parser)
24. [Ejemplo completo en Rust unsafe: sparse_array](#24-ejemplo-completo-en-rust-unsafe-sparse_array)
25. [MSan con tests unitarios](#25-msan-con-tests-unitarios)
26. [MSan con fuzzing (cargo-fuzz)](#26-msan-con-fuzzing-cargo-fuzz)
27. [MSAN_OPTIONS: configuración completa](#27-msan_options-configuración-completa)
28. [Impacto en rendimiento detallado](#28-impacto-en-rendimiento-detallado)
29. [La complicación de la stdlib](#29-la-complicación-de-la-stdlib)
30. [MSan y bibliotecas externas](#30-msan-y-bibliotecas-externas)
31. [Falsos positivos en MSan](#31-falsos-positivos-en-msan)
32. [MSan vs Valgrind (Memcheck)](#32-msan-vs-valgrind-memcheck)
33. [Limitaciones de MSan](#33-limitaciones-de-msan)
34. [Errores comunes](#34-errores-comunes)
35. [Programa de práctica: packet_decoder](#35-programa-de-práctica-packet_decoder)
36. [Ejercicios](#36-ejercicios)

---

## 1. Qué es MemorySanitizer

MemorySanitizer (MSan) detecta lecturas de memoria no inicializada. Cuando un programa usa un valor que nunca fue asignado, el comportamiento resultante es **undefined behavior** en C/C++ y un error lógico grave en cualquier lenguaje.

```
┌──────────────────────────────────────────────────────────────────┐
│                    MemorySanitizer (MSan)                         │
│                                                                  │
│  Detecta: uso de memoria NO INICIALIZADA                         │
│                                                                  │
│  ┌─────────────┐    ┌──────────────┐    ┌──────────────────┐     │
│  │  Compilador  │    │   Runtime     │    │  Shadow memory   │     │
│  │  (Clang)     │───▶│   library     │───▶│  (bit-precise)   │     │
│  └─────────────┘    └──────────────┘    └──────────────────┘     │
│                                                                  │
│  NO detecta:                                                     │
│  - buffer overflows (eso es ASan)                                │
│  - use-after-free (eso es ASan)                                  │
│  - data races (eso es TSan)                                      │
│  - undefined behavior aritmético (eso es UBSan)                  │
│                                                                  │
│  SÍ detecta:                                                     │
│  - Variables no inicializadas usadas en condicionales             │
│  - Campos de struct nunca escritos                                │
│  - malloc() sin memset/calloc                                    │
│  - Propagación de datos no inicializados                         │
│  - Valores parcialmente inicializados                            │
│  - Retorno de valores no inicializados                           │
│  - Paso de valores no inicializados a funciones                  │
└──────────────────────────────────────────────────────────────────┘
```

La diferencia fundamental con otros sanitizers: MSan **no** detecta accesos fuera de límites ni uso de memoria liberada. Su **único** objetivo es rastrear si cada byte (y cada bit) ha sido inicializado antes de ser usado en una operación que lo requiere.

---

## 2. Historia y contexto

```
Línea temporal de MSan:

2012    Evgeniy Stepanov (Google) presenta MemorySanitizer
        Basado en investigación de shadow memory bit-precise
        Implementado en LLVM/Clang

2013    MSan entra en LLVM trunk
        Solo soporta Linux x86_64 (y sigue siendo así en 2025)

2014    Origin tracking añadido (-fsanitize-memory-track-origins)
        Permite rastrear DÓNDE se creó la memoria no inicializada

2015    Soporte para origin tracking con profundidad configurable
        -fsanitize-memory-track-origins=2 (default mejorado)

2017    Google ChromeOS integra MSan en fuzzing continuo
        Descubre cientos de bugs de inicialización

2019    Rust nightly añade soporte experimental para MSan
        Requiere -Zbuild-std para reinstrumentar la stdlib

2023    MSan maduro pero sigue limitado a:
        - Solo Clang (no GCC, no MSVC)
        - Solo Linux x86_64 (parcial: AArch64, NetBSD)
        - Requiere recompilar TODAS las dependencias
```

MSan es el sanitizer más restrictivo en cuanto a requisitos de compilación. A diferencia de ASan (que funciona con GCC, Clang, y parcialmente MSVC), MSan **solo funciona con Clang** y **solo en Linux x86_64**. Esta restricción existe porque la técnica de shadow memory bit-precise es extremadamente dependiente del layout de memoria del compilador.

---

## 3. El problema de la memoria no inicializada

### ¿Por qué es un problema?

En C, las variables locales y la memoria de `malloc()` **no** se inicializan automáticamente:

```c
// ¿Qué valor tiene x?
int x;            // Indeterminado: lo que había en la stack
printf("%d\n", x); // UB: cualquier cosa puede pasar

// ¿Qué contiene buf?
char *buf = malloc(100);  // Indeterminado: basura del heap
if (buf[0] == 'A') {      // UB: decisión basada en basura
    // ...
}
```

### El peligro: no siempre crashea

A diferencia de un null pointer dereference (que crashea inmediatamente) o un buffer overflow (que corrompe memoria adyacente), usar memoria no inicializada **puede funcionar la mayoría de las veces**:

```
┌─────────────────────────────────────────────────────────────────┐
│              Espectro de bugs de memoria                         │
│                                                                  │
│  NULL deref      Buffer overflow    Use-after-free    UNINIT     │
│  ───────────────────────────────────────────────────────────────  │
│  Siempre crashea  Suele corromper   Puede crashear   SILENCIOSO │
│  Fácil de ver     Detectable        A veces ok       Invisible  │
│                                                                  │
│  ◄── Más visible ──────────────────────── Más peligroso ──►     │
│                                                                  │
│  Un bug de memoria no inicializada puede:                        │
│  • Funcionar 999/1000 veces (la memoria "casualmente" es 0)     │
│  • Fallar solo en producción (layout de memoria diferente)       │
│  • Causar vulnerabilidades de seguridad (info leak)              │
│  • Producir resultados sutilmente incorrectos                    │
└─────────────────────────────────────────────────────────────────┘
```

### Consecuencias de seguridad reales

| Vulnerabilidad | CVE | Impacto |
|---|---|---|
| OpenSSL Heartbleed (parcialmente) | CVE-2014-0160 | Leak de memoria del servidor |
| Linux kernel info leaks | Múltiples | Fuga de datos del kernel al userspace |
| WebKit uninitialized read | CVE-2019-8644 | Ejecución remota de código |
| FreeBSD kernel | CVE-2020-7454 | Leak de datos del kernel |

### En Rust: ¿no es seguro por defecto?

Rust **inicializa** toda variable en safe code. No puedes tener una variable no inicializada sin `unsafe`:

```rust
// Safe Rust: IMPOSIBLE tener memoria no inicializada
let x: i32;
println!("{}", x); // Error de compilación: use of possibly-uninitialized `x`

let v = vec![0u8; 100]; // Siempre inicializado a 0
```

Pero en `unsafe` Rust, es **totalmente posible**:

```rust
// Unsafe Rust: memoria no inicializada ES posible
unsafe {
    let x: i32 = std::mem::uninitialized(); // Deprecated, UB instantáneo
    let x = std::mem::MaybeUninit::<i32>::uninit(); // Correcto: explícitamente no inicializado
    let val = x.assume_init(); // UB si no se inicializó

    let layout = std::alloc::Layout::new::<[u8; 100]>();
    let ptr = std::alloc::alloc(layout); // No inicializado (como malloc)
    let first = *ptr; // UB: lectura de memoria no inicializada
}
```

MSan es esencial cuando trabajas con:
- Código `unsafe` de Rust que usa `MaybeUninit` o `alloc`
- FFI con bibliotecas C que usan `malloc` sin `memset`
- Buffers pre-allocados para rendimiento

---

## 4. Arquitectura de MSan

```
┌─────────────────────────────────────────────────────────────────────┐
│                     Arquitectura de MSan                             │
│                                                                     │
│  ┌────────────────────┐     ┌────────────────────────────────────┐  │
│  │  Código fuente (.c) │     │  Instrumentación del compilador    │  │
│  │                     │────▶│                                    │  │
│  │  int x;             │     │  Para CADA carga/almacenamiento:  │  │
│  │  if (x > 0) {...}   │     │  1. Verifica shadow del valor     │  │
│  │                     │     │  2. Si no inicializado Y se usa   │  │
│  └────────────────────┘     │     en branch/syscall → REPORTAR  │  │
│                              │  3. Si no inicializado pero solo   │  │
│                              │     se copia → PROPAGAR shadow    │  │
│                              └────────────────────────────────────┘  │
│                                          │                           │
│                                          ▼                           │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │                    Runtime library                              │  │
│  │                                                                │  │
│  │  • Intercepta malloc/calloc/realloc/free                       │  │
│  │    - malloc: marca shadow como NO inicializado (0xFF)          │  │
│  │    - calloc: marca shadow como inicializado (0x00)             │  │
│  │    - free: marca como no inicializado de nuevo                 │  │
│  │                                                                │  │
│  │  • Intercepta memcpy/memmove/memset                            │  │
│  │    - Propaga shadow bit a bit                                  │  │
│  │    - memset: marca como inicializado                           │  │
│  │                                                                │  │
│  │  • Intercepta syscalls (read, recv, etc.)                      │  │
│  │    - Marca buffers de salida como inicializados                │  │
│  │                                                                │  │
│  │  • Gestiona origin storage (si -track-origins)                 │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                                          │                           │
│                                          ▼                           │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │                    Shadow memory (1:1)                          │  │
│  │                                                                │  │
│  │  Cada byte de memoria de la app tiene 1 byte de shadow.       │  │
│  │  Cada BIT del shadow indica si el bit correspondiente         │  │
│  │  está inicializado (0) o no inicializado (1).                 │  │
│  │                                                                │  │
│  │  App memory:     [0x10000000 ... 0x1FFFFFFF]                   │  │
│  │  Shadow memory:  [0x20000000 ... 0x2FFFFFFF]  (misma forma)   │  │
│  │  Origin memory:  [0x30000000 ... 0x3FFFFFFF]  (4 bytes/4app)  │  │
│  └────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

### Diferencia fundamental con ASan

| Aspecto | ASan | MSan |
|---|---|---|
| Shadow ratio | 8:1 (8 bytes app → 1 byte shadow) | **1:1** (1 byte app → 1 byte shadow) |
| Granularidad | Byte | **Bit** |
| Qué rastrea | Accesibilidad (¿se puede leer?) | **Inicialización** (¿tiene valor?) |
| Shadow = 0 | Todos accesibles | **Todos inicializados** |
| Shadow = 0xFF | No accesible | **Todos no inicializados** |
| Cuándo reporta | Al acceder memoria inaccesible | Al **usar** valor no inicializado |

La distinción "al usar" es crucial: MSan **no** reporta cuando lees memoria no inicializada a una variable. Solo reporta cuando esa variable se **usa** en una operación que depende de su valor (condicional, syscall, retorno observable).

---

## 5. Shadow memory en MSan: mapeo 1:1 bit-preciso

### El mapeo

```
┌─────────────────────────────────────────────────────────────────┐
│            Shadow memory de MSan (Linux x86_64)                  │
│                                                                  │
│  Fórmula:  shadow_addr = app_addr XOR 0x500000000000             │
│                                                                  │
│  Ejemplo:                                                        │
│  App byte en:    0x7fff12340001                                  │
│  Shadow byte:    0x7fff12340001 XOR 0x500000000000               │
│               =  0x2fff12340001                                  │
│                                                                  │
│  Layout de memoria virtual:                                      │
│                                                                  │
│  0x000000000000 ┌───────────────────┐                            │
│                 │   (no mapeado)     │                            │
│  0x100000000000 ├───────────────────┤                            │
│                 │   Origin memory    │ (si -track-origins)       │
│  0x200000000000 ├───────────────────┤                            │
│                 │   Shadow memory    │ 1:1 con app               │
│  0x500000000000 ├───────────────────┤                            │
│                 │   (no mapeado)     │                            │
│  0x600000000000 ├───────────────────┤                            │
│                 │   App memory       │ programa + heap + stack   │
│  0x800000000000 └───────────────────┘                            │
└─────────────────────────────────────────────────────────────────┘
```

### Granularidad bit-preciso

A diferencia de ASan que trabaja a nivel de byte, MSan rastrea cada **bit** individualmente:

```
App memory (1 byte):    [b7 b6 b5 b4 b3 b2 b1 b0]
Shadow memory (1 byte): [s7 s6 s5 s4 s3 s2 s1 s0]

Si s_i = 0 → bit b_i está inicializado
Si s_i = 1 → bit b_i NO está inicializado

Ejemplo: struct con bitfields
struct flags {
    unsigned valid : 1;   // Inicializado → shadow bit = 0
    unsigned type  : 3;   // NO inicializado → shadow bits = 1,1,1
    unsigned id    : 4;   // Inicializado → shadow bits = 0,0,0,0
};

Shadow byte para esta struct: 0b00001110 = 0x0E
                                    ^^^^
                                    type (3 bits no inicializados)
```

### Valores del shadow byte

```
┌──────────────┬─────────────────────────────────────────────────┐
│ Shadow byte  │ Significado                                     │
├──────────────┼─────────────────────────────────────────────────┤
│ 0x00         │ Byte completamente inicializado                 │
│ 0xFF         │ Byte completamente NO inicializado              │
│ 0x0F         │ 4 bits altos inicializados, 4 bajos no          │
│ 0xF0         │ 4 bits altos no inicializados, 4 bajos sí       │
│ 0x01         │ Solo el bit 0 no inicializado                   │
│ 0x80         │ Solo el bit 7 no inicializado                   │
│ cualquier    │ Cada bit indica independientemente su estado    │
└──────────────┴─────────────────────────────────────────────────┘
```

---

## 6. Propagación de estado: la clave de MSan

El aspecto más sofisticado de MSan es cómo **propaga** el estado de inicialización a través de operaciones. MSan no simplemente dice "¿este byte fue escrito?". Rastrea cómo fluye la "no-inicialización" por el programa:

### Reglas de propagación

```
┌─────────────────────────────────────────────────────────────────┐
│              Reglas de propagación de shadow                     │
│                                                                  │
│  Operación              │ Shadow resultado                      │
│  ───────────────────────┼─────────────────────────────────────── │
│  a = b                  │ shadow(a) = shadow(b)                 │
│  a = b + c              │ shadow(a) = shadow(b) | shadow(c)     │
│  a = b & c              │ shadow(a) = (shadow(b) & c) |         │
│                         │            (b & shadow(c)) |          │
│                         │            (shadow(b) & shadow(c))    │
│  a = b | c              │ similar a AND pero con OR             │
│  a = b << n             │ shadow(a) = shadow(b) << n            │
│  a = ~b                 │ shadow(a) = shadow(b)                 │
│  a = (tipo)b            │ shadow(a) se extiende/trunca          │
│  memcpy(d,s,n)          │ copiar shadow de s a shadow de d      │
│  memset(d,v,n)          │ shadow(d) = 0 (todo inicializado)     │
│  if (x) {...}           │ Si shadow(x) ≠ 0 → ERROR             │
│  syscall(..., buf)      │ Si shadow(buf) ≠ 0 → ERROR           │
│  return x               │ Si shadow(x) ≠ 0 → propagar al call  │
└─────────────────────────────────────────────────────────────────┘
```

### Ejemplo de propagación

```c
// MSan rastrea la propagación paso a paso:

int a;                      // shadow(a) = 0xFFFFFFFF (no inicializado)
int b = 42;                 // shadow(b) = 0x00000000 (inicializado)
int c = a + b;              // shadow(c) = shadow(a) | shadow(b)
                            //           = 0xFFFFFFFF | 0x00000000
                            //           = 0xFFFFFFFF (no inicializado!)

int d = c * 2;              // shadow(d) = shadow(c) = 0xFFFFFFFF
                            // La "contaminación" se propaga

// Hasta aquí MSan NO reporta nada. Solo propaga.

if (d > 0) {               // AQUÍ reporta: uso de valor no inicializado
    // ...                  // en una decisión de control de flujo
}
```

### ¿Por qué propagar en vez de reportar inmediatamente?

```
┌─────────────────────────────────────────────────────────────────┐
│         ¿Por qué MSan no reporta al primer read?                │
│                                                                  │
│  Caso 1: Copiar memoria no inicializada (LEGÍTIMO)              │
│                                                                  │
│    struct Packet pkt;          // No inicializado                │
│    pkt.header = read_header(); // Inicializa .header             │
│    pkt.payload = data;         // Inicializa .payload            │
│    // pkt.padding NO se inicializa (no importa si no se lee)     │
│    send_packet(pkt);           // Copia todo, incluido padding   │
│                                                                  │
│    MSan PROPAGA el shadow de padding pero no reporta             │
│    mientras el receptor no USE el padding.                        │
│                                                                  │
│  Caso 2: Inicialización diferida (LEGÍTIMO)                      │
│                                                                  │
│    int buf[100];               // No inicializado                │
│    int n = read(fd, buf, 400); // Kernel inicializa n bytes      │
│    // MSan marca buf[0..n] como inicializado                     │
│    // buf[n..100] sigue como no inicializado                     │
│    for (int i = 0; i < n/4; i++)                                │
│        process(buf[i]);        // OK, solo lee lo inicializado   │
│                                                                  │
│  Si MSan reportara al primer read de buf[], daría falso          │
│  positivo en ambos casos.                                        │
└─────────────────────────────────────────────────────────────────┘
```

---

## 7. Origin tracking: de dónde viene el problema

Origin tracking es la funcionalidad que hace a MSan **usable en la práctica**. Sin ella, MSan solo dice "usaste un valor no inicializado en la línea X". Con ella, dice "el valor no inicializado se creó en la línea Y".

### Activar origin tracking

```bash
# Sin origin tracking (poco útil):
clang -fsanitize=memory -g -O1 program.c

# Con origin tracking (SIEMPRE usar):
clang -fsanitize=memory -fsanitize-memory-track-origins=2 -g -O1 program.c
```

### Niveles de origin tracking

```
┌─────────────────────────────────────────────────────────────────┐
│            Niveles de origin tracking                            │
│                                                                  │
│  Nivel 0: sin tracking (default sin flag)                        │
│    "Use of uninitialized value"                                  │
│    → Solo te dice DÓNDE se usó, no DÓNDE se creó                │
│    → Prácticamente inútil para debugging                         │
│                                                                  │
│  Nivel 1: -fsanitize-memory-track-origins                        │
│    "Use of uninitialized value"                                  │
│    "  Uninitialized value was created by heap allocation"        │
│    "    #0 in malloc ..."                                        │
│    → Te dice la allocación original                              │
│    → Overhead: ~1.5x sobre MSan base                             │
│                                                                  │
│  Nivel 2: -fsanitize-memory-track-origins=2 (RECOMENDADO)        │
│    "Use of uninitialized value"                                  │
│    "  Uninitialized value was stored to memory at"               │
│    "    #0 in copy_config config.c:45"                           │
│    "  Uninitialized value was created by heap allocation"        │
│    "    #0 in malloc ..."                                        │
│    → Te dice la cadena COMPLETA de propagación                   │
│    → Overhead: ~2x sobre MSan base                               │
│    → Vale la pena: sin esto no puedes debuggear eficazmente     │
└─────────────────────────────────────────────────────────────────┘
```

### Almacenamiento de origins

```
┌─────────────────────────────────────────────────────────────────┐
│              Origin memory layout                                │
│                                                                  │
│  App memory:    [byte0][byte1][byte2][byte3][byte4]...           │
│  Shadow memory: [shad0][shad1][shad2][shad3][shad4]...           │
│  Origin memory: [    origin_id_32bit     ][  origin_id  ]...     │
│                  ◄──── 4 bytes ───────────►                      │
│                                                                  │
│  Cada 4 bytes de app memory comparten 1 origin_id (32 bits).    │
│  El origin_id es un índice en una tabla interna que almacena:    │
│  - Stack trace de la creación original                           │
│  - Stack trace de cada store intermedio (nivel 2)                │
│  - Tipo de creación (stack alloc, heap alloc, etc.)              │
│                                                                  │
│  Ratio total con origins:                                        │
│  1 byte app → 1 byte shadow + 1 byte origin = 2 bytes overhead  │
│  (realmente 1.25 bytes origin por el alineamiento a 4)           │
│  Total: ~3x uso de memoria con origin tracking nivel 2           │
└─────────────────────────────────────────────────────────────────┘
```

---

## 8. Compilar con MSan en C

### Flags de compilación

```bash
# Mínimo (NO recomendado, sin origin tracking):
clang -fsanitize=memory -g program.c -o program

# Recomendado:
clang -fsanitize=memory \
      -fsanitize-memory-track-origins=2 \
      -fno-omit-frame-pointer \
      -g \
      -O1 \
      program.c -o program

# Desglose de cada flag:
# -fsanitize=memory          → Activa MSan
# -fsanitize-memory-track-origins=2  → Origin tracking completo
# -fno-omit-frame-pointer    → Stack traces legibles
# -g                         → Símbolos de debug (nombres de funciones, líneas)
# -O1                        → Optimización mínima (recomendado por MSan)
```

### ¿Por qué -O1?

```
┌─────────────────────────────────────────────────────────────────┐
│            ¿Qué nivel de optimización usar con MSan?             │
│                                                                  │
│  -O0: NO recomendado                                             │
│    • El compilador no elimina variables temporales               │
│    • Genera muchos falsos positivos por stores/loads redundantes │
│    • MSan ve "contaminación" en copias intermedias que -O1       │
│      eliminaría                                                  │
│                                                                  │
│  -O1: RECOMENDADO                                                │
│    • Elimina copias redundantes que causarían falso positivos    │
│    • Mantiene suficiente información de debug                    │
│    • Balance entre precisión y debuggeabilidad                   │
│                                                                  │
│  -O2/-O3: Posible pero con precaución                            │
│    • El optimizador puede eliminar reads que serían bugs         │
│    • El inlining agresivo complica los stack traces              │
│    • Útil para testing de rendimiento con MSan                   │
└─────────────────────────────────────────────────────────────────┘
```

### Compilar un proyecto multi-archivo

```bash
# CRUCIAL: TODOS los archivos deben compilarse con MSan
clang -fsanitize=memory -fsanitize-memory-track-origins=2 \
      -fno-omit-frame-pointer -g -O1 \
      -c src/main.c -o build/main.o

clang -fsanitize=memory -fsanitize-memory-track-origins=2 \
      -fno-omit-frame-pointer -g -O1 \
      -c src/parser.c -o build/parser.o

clang -fsanitize=memory -fsanitize-memory-track-origins=2 \
      -fno-omit-frame-pointer -g -O1 \
      build/main.o build/parser.o -o program

# Con Makefile:
CC = clang
CFLAGS = -fsanitize=memory -fsanitize-memory-track-origins=2 \
         -fno-omit-frame-pointer -g -O1
LDFLAGS = -fsanitize=memory
```

### ¿Qué pasa si mezclo archivos con y sin MSan?

```
┌─────────────────────────────────────────────────────────────────┐
│     PELIGRO: mezclar código instrumentado y no instrumentado     │
│                                                                  │
│  archivo1.c (con MSan) ──┐                                       │
│                           ├──▶ programa                          │
│  archivo2.c (sin MSan) ──┘                                       │
│                                                                  │
│  Resultado: FALSOS POSITIVOS masivos                             │
│                                                                  │
│  ¿Por qué?                                                       │
│  archivo2.c escribe datos sin actualizar shadow memory.          │
│  Cuando archivo1.c lee esos datos, MSan cree que no están        │
│  inicializados porque el shadow sigue en 0xFF.                   │
│                                                                  │
│  Esto incluye bibliotecas del sistema (libc, libm, etc.)         │
│  MSan intercepta las funciones comunes (malloc, memcpy, etc.)    │
│  pero no TODAS las funciones de libc.                            │
│                                                                  │
│  Solución: recompilar TODA dependencia con MSan                  │
│  (ver sección 29: La complicación de la stdlib)                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 9. El requisito de -Zbuild-std en Rust

Este es el aspecto más complicado de usar MSan con Rust. La stdlib de Rust viene precompilada sin instrumentación MSan. Si la usas directamente, cualquier dato que pase por la stdlib será marcado como "no inicializado" desde la perspectiva de MSan.

### El problema

```
┌─────────────────────────────────────────────────────────────────┐
│         ¿Por qué MSan necesita -Zbuild-std en Rust?              │
│                                                                  │
│  Sin -Zbuild-std:                                                │
│                                                                  │
│  Tu código (instrumentado) ──▶ Vec::new() (NO instrumentado)    │
│                                                                  │
│  Vec::new() alloca memoria y la inicializa internamente.         │
│  Pero como Vec no fue compilado con MSan, NO actualiza shadow.  │
│  → MSan cree que el contenido del Vec no está inicializado.     │
│  → Falso positivo al leer v[0].                                  │
│                                                                  │
│  Con -Zbuild-std:                                                │
│                                                                  │
│  Tu código (instrumentado) ──▶ Vec::new() (INSTRUMENTADO)       │
│                                                                  │
│  Vec::new() compila con MSan, actualiza shadow correctamente.   │
│  → MSan sabe que el contenido fue inicializado.                  │
│  → Sin falso positivo.                                           │
│                                                                  │
│  -Zbuild-std recompila core, alloc, std con instrumentación     │
│  MSan. Esto añade ~2-5 minutos de compilación inicial.           │
└─────────────────────────────────────────────────────────────────┘
```

### Requisitos previos

```bash
# 1. Instalar el componente rust-src (código fuente de la stdlib)
rustup component add rust-src --toolchain nightly

# 2. Verificar que está instalado
rustup component list --toolchain nightly | grep rust-src
# rust-src (installed)

# 3. Verificar que target está disponible
rustup target list --toolchain nightly | grep x86_64-unknown-linux-gnu
# x86_64-unknown-linux-gnu (installed)
```

---

## 10. Compilar con MSan en Rust

### Comando básico

```bash
# Compilar un programa:
RUSTFLAGS="-Zsanitizer=memory" \
    cargo +nightly run -Zbuild-std \
    --target x86_64-unknown-linux-gnu

# Ejecutar tests:
RUSTFLAGS="-Zsanitizer=memory" \
    cargo +nightly test -Zbuild-std \
    --target x86_64-unknown-linux-gnu

# Desglose:
# RUSTFLAGS="-Zsanitizer=memory"     → Activa MSan
# cargo +nightly                      → Requiere nightly (features inestables)
# -Zbuild-std                         → Recompila la stdlib con MSan
# --target x86_64-unknown-linux-gnu   → Requerido con -Zbuild-std
```

### Con origin tracking

```bash
# Añadir origin tracking (RECOMENDADO):
RUSTFLAGS="-Zsanitizer=memory \
           -Zsanitizer-memory-track-origins=2" \
    cargo +nightly run -Zbuild-std \
    --target x86_64-unknown-linux-gnu
```

### En Cargo.toml (configuración persistente)

```toml
# .cargo/config.toml (en la raíz del proyecto)

# Perfil para MSan
[target.x86_64-unknown-linux-gnu]
rustflags = ["-Zsanitizer=memory", "-Zsanitizer-memory-track-origins=2"]

[build]
# build-std no se puede poner en config.toml directamente,
# hay que pasarlo en la línea de comandos.
# Pero sí podemos definir el target:
target = "x86_64-unknown-linux-gnu"
```

```bash
# Con la config anterior, solo necesitas:
cargo +nightly run -Zbuild-std
cargo +nightly test -Zbuild-std
```

### Ejemplo mínimo funcional

```rust
// src/main.rs
use std::mem::MaybeUninit;

fn main() {
    // Bug: leer valor no inicializado
    unsafe {
        let x: MaybeUninit<i32> = MaybeUninit::uninit();
        let val = x.assume_init(); // UB: no inicializado
        if val > 0 {               // MSan detecta aquí
            println!("positive");
        }
    }
}
```

```bash
# Compilar y ejecutar:
RUSTFLAGS="-Zsanitizer=memory -Zsanitizer-memory-track-origins=2" \
    cargo +nightly run -Zbuild-std --target x86_64-unknown-linux-gnu

# Salida:
# ==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
#     #0 0x... in main::main src/main.rs:7:12
#
#   Uninitialized value was created in the stack frame of function 'main::main'
#     #0 0x... in main::main src/main.rs:5:1
```

---

## 11. Bug 1: variable local no inicializada

### En C

```c
// uninit_local.c
#include <stdio.h>

int process(int mode) {
    int result;  // ← NUNCA se inicializa en la rama else

    if (mode == 1) {
        result = 42;
    } else if (mode == 2) {
        result = 99;
    }
    // Si mode != 1 && mode != 2, result queda sin inicializar

    return result;  // Bug: puede retornar basura
}

int main() {
    int val = process(3);  // mode=3, cae en el caso no cubierto
    printf("Result: %d\n", val);  // Usa valor no inicializado
    return 0;
}
```

```bash
$ clang -fsanitize=memory -fsanitize-memory-track-origins=2 \
        -fno-omit-frame-pointer -g -O1 uninit_local.c -o uninit_local
$ ./uninit_local

==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x4c3e80 in main uninit_local.c:16:5
    #1 0x7f... in __libc_start_main

  Uninitialized value was created in the stack frame of function 'process'
    #0 0x4c3d00 in process uninit_local.c:4:5
```

### En Rust unsafe

```rust
// No se puede hacer directamente en safe Rust (compilador lo impide).
// En unsafe:
use std::mem::MaybeUninit;

fn process(mode: i32) -> i32 {
    unsafe {
        let mut result: MaybeUninit<i32> = MaybeUninit::uninit();

        if mode == 1 {
            result = MaybeUninit::new(42);
        } else if mode == 2 {
            result = MaybeUninit::new(99);
        }
        // Si mode != 1 && mode != 2, result sigue uninit

        result.assume_init()  // Bug: UB si no se inicializó
    }
}

fn main() {
    let val = process(3);
    if val > 0 {  // MSan detecta aquí
        println!("Positive: {}", val);
    }
}
```

---

## 12. Bug 2: campo de struct no inicializado

### En C

```c
// uninit_field.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int id;
    int priority;
    char name[32];
    int flags;      // ← Este campo puede quedar sin inicializar
} Task;

Task *create_task(int id, const char *name) {
    Task *t = (Task *)malloc(sizeof(Task));
    t->id = id;
    strncpy(t->name, name, 31);
    t->name[31] = '\0';
    // Bug: NO inicializa t->priority ni t->flags
    return t;
}

void print_task(Task *t) {
    printf("Task %d: %s\n", t->id, t->name);

    // Usa campos no inicializados:
    if (t->priority > 5) {          // Bug: priority no inicializado
        printf("  HIGH PRIORITY\n");
    }
    if (t->flags & 0x01) {          // Bug: flags no inicializado
        printf("  FLAGGED\n");
    }
}

int main() {
    Task *t = create_task(1, "compile");
    print_task(t);
    free(t);
    return 0;
}
```

```bash
$ clang -fsanitize=memory -fsanitize-memory-track-origins=2 \
        -fno-omit-frame-pointer -g -O1 uninit_field.c -o uninit_field
$ ./uninit_field

==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x4c3f20 in print_task uninit_field.c:25:9
    #1 0x4c4100 in main uninit_field.c:33:5

  Uninitialized value was stored to memory at
    #0 0x4c3d80 in create_task uninit_field.c:15:14

  Uninitialized value was created by a heap allocation
    #0 0x... in malloc
    #1 0x4c3d60 in create_task uninit_field.c:14:20
```

### En Rust unsafe

```rust
use std::alloc::{alloc, dealloc, Layout};
use std::ptr;

#[repr(C)]
struct Task {
    id: i32,
    priority: i32,
    name: [u8; 32],
    flags: i32,
}

unsafe fn create_task(id: i32, name: &str) -> *mut Task {
    let layout = Layout::new::<Task>();
    let ptr = alloc(layout) as *mut Task;

    (*ptr).id = id;
    // Copiar nombre
    let name_bytes = name.as_bytes();
    let copy_len = name_bytes.len().min(31);
    ptr::copy_nonoverlapping(
        name_bytes.as_ptr(),
        (*ptr).name.as_mut_ptr(),
        copy_len,
    );
    (*ptr).name[copy_len] = 0;

    // Bug: NO inicializa priority ni flags
    ptr
}

unsafe fn print_task(t: *const Task) {
    println!("Task {}", (*t).id);
    if (*t).priority > 5 {    // Bug: priority no inicializado
        println!("  HIGH PRIORITY");
    }
    if (*t).flags & 0x01 != 0 {  // Bug: flags no inicializado
        println!("  FLAGGED");
    }
}

fn main() {
    unsafe {
        let t = create_task(1, "compile");
        print_task(t);
        dealloc(t as *mut u8, Layout::new::<Task>());
    }
}
```

---

## 13. Bug 3: malloc sin inicializar

### En C

```c
// uninit_malloc.c
#include <stdio.h>
#include <stdlib.h>

int sum_array(const int *arr, size_t n) {
    int total = 0;
    for (size_t i = 0; i < n; i++) {
        total += arr[i];  // Bug si arr[i] no fue inicializado
    }
    return total;
}

int main() {
    size_t n = 10;

    // malloc NO inicializa la memoria
    int *data = (int *)malloc(n * sizeof(int));

    // Solo inicializamos los primeros 5 elementos
    for (size_t i = 0; i < 5; i++) {
        data[i] = (int)(i * 10);
    }

    // Bug: sumamos los 10, pero data[5..9] son basura
    int total = sum_array(data, n);
    printf("Sum: %d\n", total);

    free(data);
    return 0;
}
```

```bash
$ clang -fsanitize=memory -fsanitize-memory-track-origins=2 \
        -fno-omit-frame-pointer -g -O1 uninit_malloc.c -o uninit_malloc
$ ./uninit_malloc

==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x4c3e00 in sum_array uninit_malloc.c:7:16
    #1 0x4c3f80 in main uninit_malloc.c:22:17

  Uninitialized value was created by a heap allocation
    #0 0x... in malloc
    #1 0x4c3f00 in main uninit_malloc.c:14:26
```

**Corrección**: usar `calloc` o `memset`:

```c
// Opción 1: calloc (inicializa a 0)
int *data = (int *)calloc(n, sizeof(int));

// Opción 2: memset después de malloc
int *data = (int *)malloc(n * sizeof(int));
memset(data, 0, n * sizeof(int));

// Opción 3: inicializar todos los elementos
for (size_t i = 0; i < n; i++) {
    data[i] = 0;
}
```

### En Rust unsafe

```rust
use std::alloc::{alloc, alloc_zeroed, Layout};

fn main() {
    unsafe {
        let layout = Layout::array::<i32>(10).unwrap();

        // alloc NO inicializa (equivale a malloc)
        let ptr = alloc(layout) as *mut i32;

        // Solo inicializamos 5 elementos
        for i in 0..5 {
            ptr.add(i).write(i as i32 * 10);
        }

        // Bug: leemos los 10
        let mut total = 0i32;
        for i in 0..10 {
            total += *ptr.add(i);  // Bug en i=5..9: no inicializado
        }
        println!("Sum: {}", total);

        std::alloc::dealloc(ptr as *mut u8, layout);
    }
}
```

**Corrección en Rust**:

```rust
// Opción 1: alloc_zeroed (equivale a calloc)
let ptr = alloc_zeroed(layout) as *mut i32;

// Opción 2: write_bytes (equivale a memset)
let ptr = alloc(layout) as *mut i32;
ptr.write_bytes(0, 10);

// Opción 3: safe Rust (lo mejor)
let mut data = vec![0i32; 10];
```

---

## 14. Bug 4: lectura parcial de buffer

### En C

```c
// partial_init.c
#include <stdio.h>
#include <string.h>

typedef struct {
    char header[8];   // 8 bytes
    int length;       // 4 bytes
    char data[64];    // 64 bytes
} Message;

void send_message(const Message *msg) {
    // Envía TODO el struct, incluido lo no inicializado
    // MSan detecta cuando se lee msg->data más allá de length
    for (int i = 0; i < 64; i++) {
        if (msg->data[i] != 0) {   // Bug: lee más allá de lo inicializado
            printf("data[%d] = %c\n", i, msg->data[i]);
        }
    }
}

int main() {
    Message msg;
    memcpy(msg.header, "MSG\x01\x00\x00\x00\x00", 8);
    msg.length = 5;
    memcpy(msg.data, "Hello", 5);  // Solo inicializa 5 de 64 bytes

    send_message(&msg);  // Lee los 64 bytes de data
    return 0;
}
```

```bash
==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x4c3e80 in send_message partial_init.c:15:9
    #1 0x4c4020 in main partial_init.c:24:5

  Uninitialized value was created in the stack frame of function 'main'
    #0 0x4c3f00 in main partial_init.c:19:5
```

### En Rust unsafe

```rust
use std::ptr;

#[repr(C)]
struct Message {
    header: [u8; 8],
    length: i32,
    data: [u8; 64],
}

unsafe fn send_message(msg: *const Message) {
    let data = &(*msg).data;
    for i in 0..64 {
        if data[i] != 0 {  // Bug: lee más allá de lo inicializado
            println!("data[{}] = {}", i, data[i] as char);
        }
    }
}

fn main() {
    unsafe {
        let layout = std::alloc::Layout::new::<Message>();
        let msg = std::alloc::alloc(layout) as *mut Message;

        ptr::copy_nonoverlapping(
            b"MSG\x01\x00\x00\x00\x00".as_ptr(),
            (*msg).header.as_mut_ptr(),
            8,
        );
        (*msg).length = 5;
        ptr::copy_nonoverlapping(
            b"Hello".as_ptr(),
            (*msg).data.as_mut_ptr(),
            5,
        );
        // Solo 5 de 64 bytes inicializados

        send_message(msg);
        std::alloc::dealloc(msg as *mut u8, layout);
    }
}
```

---

## 15. Bug 5: uso condicional basado en valor no inicializado

Este es el caso que MSan está **diseñado específicamente** para detectar. Usar un valor no inicializado en un `if`, `while`, `switch`, o `?:` es particularmente peligroso porque el flujo de control del programa depende de basura.

### En C

```c
// uninit_branch.c
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int type;
    int value;
    int cached;     // ← Debería ser flag booleano, pero no se inicializa
    int cache_val;
} Entry;

Entry *new_entry(int type, int value) {
    Entry *e = (Entry *)malloc(sizeof(Entry));
    e->type = type;
    e->value = value;
    // Bug: no inicializa cached ni cache_val
    return e;
}

int get_computed_value(Entry *e) {
    // Bug: decisión basada en campo no inicializado
    if (e->cached) {            // ← MSan detecta aquí
        return e->cache_val;    // También no inicializado
    }

    // Computar valor
    int result = e->value * e->value;
    e->cached = 1;
    e->cache_val = result;
    return result;
}

int main() {
    Entry *e = new_entry(1, 7);
    int v = get_computed_value(e);
    printf("Value: %d\n", v);
    free(e);
    return 0;
}
```

```bash
==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x4c3e40 in get_computed_value uninit_branch.c:22:9
    #1 0x4c4020 in main uninit_branch.c:33:13

  Uninitialized value was stored to memory at
    #0 0x4c3d80 in new_entry uninit_branch.c:13:14

  Uninitialized value was created by a heap allocation
    #0 0x... in malloc
    #1 0x4c3d60 in new_entry uninit_branch.c:12:22
```

### En Rust unsafe

```rust
use std::alloc::{alloc, dealloc, Layout};

#[repr(C)]
struct Entry {
    entry_type: i32,
    value: i32,
    cached: i32,     // No inicializado
    cache_val: i32,  // No inicializado
}

unsafe fn new_entry(entry_type: i32, value: i32) -> *mut Entry {
    let layout = Layout::new::<Entry>();
    let ptr = alloc(layout) as *mut Entry;
    (*ptr).entry_type = entry_type;
    (*ptr).value = value;
    // Bug: no inicializa cached ni cache_val
    ptr
}

unsafe fn get_computed_value(e: *mut Entry) -> i32 {
    if (*e).cached != 0 {       // Bug: decisión con valor no inicializado
        return (*e).cache_val;
    }
    let result = (*e).value * (*e).value;
    (*e).cached = 1;
    (*e).cache_val = result;
    result
}

fn main() {
    unsafe {
        let e = new_entry(1, 7);
        let v = get_computed_value(e);
        println!("Value: {}", v);
        dealloc(e as *mut u8, Layout::new::<Entry>());
    }
}
```

---

## 16. Bug 6: pasar valor no inicializado a función

### En C

```c
// uninit_param.c
#include <stdio.h>
#include <math.h>

void process_coordinates(double x, double y, double z) {
    // z podría no estar inicializado
    double distance = sqrt(x*x + y*y + z*z);  // Usa z no inicializado
    printf("Distance: %f\n", distance);
}

int main() {
    double x = 1.0, y = 2.0;
    double z;  // Bug: no inicializado

    // El desarrollador olvidó asignar z
    process_coordinates(x, y, z);  // MSan detecta al evaluar z*z
    return 0;
}
```

```bash
==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x4c3e00 in process_coordinates uninit_param.c:6:39
    #1 0x4c3f80 in main uninit_param.c:14:5

  Uninitialized value was created in the stack frame of function 'main'
    #0 0x4c3f00 in main uninit_param.c:10:5
```

### En Rust unsafe

```rust
use std::mem::MaybeUninit;

fn process_coordinates(x: f64, y: f64, z: f64) {
    let distance = (x*x + y*y + z*z).sqrt();
    println!("Distance: {}", distance);
}

fn main() {
    let x = 1.0f64;
    let y = 2.0f64;
    let z: f64 = unsafe {
        MaybeUninit::uninit().assume_init()  // Bug: UB
    };
    process_coordinates(x, y, z);  // MSan detecta al usar z
}
```

---

## 17. Bug 7: retornar valor no inicializado

### En C

```c
// uninit_return.c
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int status;
    int error_code;
} Result;

Result do_operation(int input) {
    Result r;  // No inicializado

    if (input > 0) {
        r.status = 1;
        r.error_code = 0;
    } else if (input < 0) {
        r.status = 0;
        r.error_code = -1;
    }
    // Bug: si input == 0, r queda completamente sin inicializar

    return r;  // MSan propaga el shadow
}

int main() {
    Result r = do_operation(0);  // Todos los campos sin inicializar

    // MSan detecta al usar los campos:
    if (r.status) {              // Bug: status no inicializado
        printf("Success\n");
    } else {
        printf("Error: %d\n", r.error_code);  // Bug: error_code no inicializado
    }

    return 0;
}
```

```bash
==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x4c3f80 in main uninit_return.c:27:9

  Uninitialized value was stored to memory at
    #0 0x4c3e80 in do_operation uninit_return.c:21:5

  Uninitialized value was created in the stack frame of function 'do_operation'
    #0 0x4c3d00 in do_operation uninit_return.c:10:5
```

Nota cómo origin tracking nivel 2 muestra la **cadena**: creado en `do_operation:10`, almacenado en `do_operation:21` (el return), usado en `main:27`.

### En Rust unsafe

```rust
use std::mem::MaybeUninit;

#[repr(C)]
struct OpResult {
    status: i32,
    error_code: i32,
}

fn do_operation(input: i32) -> OpResult {
    unsafe {
        let mut r: MaybeUninit<OpResult> = MaybeUninit::uninit();
        let ptr = r.as_mut_ptr();

        if input > 0 {
            (*ptr).status = 1;
            (*ptr).error_code = 0;
        } else if input < 0 {
            (*ptr).status = 0;
            (*ptr).error_code = -1;
        }
        // Bug: input == 0 deja todo sin inicializar

        r.assume_init()  // UB si no se inicializó
    }
}

fn main() {
    let r = do_operation(0);
    if r.status != 0 {  // Bug: status no inicializado
        println!("Success");
    }
}
```

---

## 18. Bug 8: no inicializado en operación aritmética

### En C

```c
// uninit_arith.c
#include <stdio.h>
#include <stdlib.h>

void compute_stats(const int *data, size_t n,
                   int *out_min, int *out_max, int *out_avg) {
    if (n == 0) return;  // Bug: no asigna nada a los punteros de salida

    int min = data[0], max = data[0];
    long sum = data[0];

    for (size_t i = 1; i < n; i++) {
        if (data[i] < min) min = data[i];
        if (data[i] > max) max = data[i];
        sum += data[i];
    }

    *out_min = min;
    *out_max = max;
    *out_avg = (int)(sum / (long)n);
}

int main() {
    int min, max, avg;

    // Caso con n=0: no asigna nada
    compute_stats(NULL, 0, &min, &max, &avg);

    // Bug: min, max, avg no fueron inicializados
    printf("Min: %d, Max: %d, Avg: %d\n", min, max, avg);

    // Operación aritmética con valores no inicializados:
    int range = max - min;   // MSan detecta aquí
    printf("Range: %d\n", range);

    return 0;
}
```

### En Rust unsafe

```rust
use std::mem::MaybeUninit;

unsafe fn compute_stats(
    data: *const i32,
    n: usize,
    out_min: *mut i32,
    out_max: *mut i32,
    out_avg: *mut i32,
) {
    if n == 0 {
        return;  // Bug: no escribe en los punteros de salida
    }

    let mut min = *data;
    let mut max = *data;
    let mut sum = *data as i64;

    for i in 1..n {
        let val = *data.add(i);
        if val < min { min = val; }
        if val > max { max = val; }
        sum += val as i64;
    }

    *out_min = min;
    *out_max = max;
    *out_avg = (sum / n as i64) as i32;
}

fn main() {
    unsafe {
        let mut min = MaybeUninit::<i32>::uninit();
        let mut max = MaybeUninit::<i32>::uninit();
        let mut avg = MaybeUninit::<i32>::uninit();

        compute_stats(
            std::ptr::null(), 0,
            min.as_mut_ptr(), max.as_mut_ptr(), avg.as_mut_ptr(),
        );

        // Bug: assume_init en valores que nunca fueron escritos
        let range = max.assume_init() - min.assume_init();
        println!("Range: {}", range);
    }
}
```

---

## 19. Bug 9: memoria no inicializada vía syscall

### En C

```c
// uninit_syscall.c
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
    char buf[256];  // No inicializado

    int fd = open("/dev/null", O_RDONLY);
    if (fd < 0) return 1;

    // read retorna 0 bytes desde /dev/null
    ssize_t n = read(fd, buf, sizeof(buf));
    close(fd);

    // n == 0, pero intentamos procesar buf como si tuviera datos
    if (n >= 0) {
        // Bug: procesamos buf entero, pero solo n=0 bytes fueron escritos
        for (int i = 0; i < 256; i++) {
            if (buf[i] == '\n') {  // Bug: buf no inicializado
                printf("Found newline at %d\n", i);
            }
        }
    }

    return 0;
}
```

```bash
==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x4c3f00 in main uninit_syscall.c:19:17

  Uninitialized value was created in the stack frame of function 'main'
    #0 0x4c3d00 in main uninit_syscall.c:6:5
```

MSan **sabe** que `read()` inicializa exactamente `n` bytes del buffer. Así que el shadow de `buf[0..n]` se marca como inicializado, y `buf[n..256]` permanece como no inicializado.

### Corrección

```c
// Procesar solo los bytes que read() realmente escribió:
for (ssize_t i = 0; i < n; i++) {
    if (buf[i] == '\n') {
        printf("Found newline at %zd\n", i);
    }
}
```

---

## 20. Anatomía completa de un reporte MSan

```
┌──────────────────────────────────────────────────────────────────────┐
│  REPORTE MSAN COMPLETO (con -fsanitize-memory-track-origins=2)       │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ SECCIÓN 1: Resumen del error                                   │  │
│  │                                                                │  │
│  │ ==PID==WARNING: MemorySanitizer: use-of-uninitialized-value    │  │
│  │                                                                │  │
│  │ "use-of-uninitialized-value" es el ÚNICO tipo de error MSan   │  │
│  │ (a diferencia de ASan que tiene heap-buffer-overflow,          │  │
│  │  use-after-free, etc.)                                        │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ SECCIÓN 2: Stack trace del USO (dónde se detectó)              │  │
│  │                                                                │  │
│  │     #0 0x4c3f20 in check_status src/validator.c:45:9           │  │
│  │     #1 0x4c4100 in process src/main.c:22:15                    │  │
│  │     #2 0x4c4200 in main src/main.c:38:5                       │  │
│  │                                                                │  │
│  │ ← Lee de abajo a arriba: main llamó process llamó check_status│  │
│  │   El uso fue en validator.c línea 45, columna 9               │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ SECCIÓN 3: Origin chain (de dónde viene, nivel 2)              │  │
│  │                                                                │  │
│  │   Uninitialized value was stored to memory at                  │  │
│  │     #0 0x4c3d80 in copy_config src/config.c:67:5               │  │
│  │     #1 0x4c3e00 in init_system src/init.c:30:10                │  │
│  │                                                                │  │
│  │ ← Intermedio: el valor no inicializado fue COPIADO aquí       │  │
│  │   (puede haber múltiples niveles de propagación)               │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ SECCIÓN 4: Creación original                                   │  │
│  │                                                                │  │
│  │   Uninitialized value was created by a heap allocation         │  │
│  │     #0 0x... in malloc                                         │  │
│  │     #1 0x4c3c00 in new_config src/config.c:12:25               │  │
│  │                                                                │  │
│  │ ← Origen: la memoria se allocó en config.c línea 12           │  │
│  │   "heap allocation" = malloc/realloc sin inicializar           │  │
│  │   También puede decir:                                        │  │
│  │   - "in the stack frame of function 'X'" = variable local     │  │
│  │   - "by a stack allocation" = array en stack                   │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ SECCIÓN 5: Resumen final                                       │  │
│  │                                                                │  │
│  │ SUMMARY: MemorySanitizer: use-of-uninitialized-value           │  │
│  │   src/validator.c:45:9 in check_status                         │  │
│  │ Exiting                                                        │  │
│  └────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

### Tipos de creación que reporta MSan

| Mensaje | Significado |
|---|---|
| `created by a heap allocation` | `malloc()` sin `memset`/`calloc` |
| `created in the stack frame of function 'X'` | Variable local no inicializada |
| `created by a stack allocation` | Array/struct en stack sin inicializar |
| `Uninitialized value was stored to memory at` | Propagación intermedia (nivel 2) |

---

## 21. Leer el origin trace

### Ejemplo real con propagación multinivel

```c
// propagation.c
#include <stdlib.h>
#include <string.h>

typedef struct { int x, y, z; } Point;
typedef struct { Point origin; Point target; } Line;

Point *make_point(int x, int y) {
    Point *p = (Point *)malloc(sizeof(Point));
    p->x = x;
    p->y = y;
    // Bug: p->z no inicializado
    return p;
}

Line *make_line(Point *a, Point *b) {
    Line *l = (Line *)malloc(sizeof(Line));
    memcpy(&l->origin, a, sizeof(Point));  // Copia incluyendo z no inicializado
    memcpy(&l->target, b, sizeof(Point));
    return l;
}

double line_length(const Line *l) {
    int dx = l->target.x - l->origin.x;
    int dy = l->target.y - l->origin.y;
    int dz = l->target.z - l->origin.z;  // Bug: usa z no inicializado
    return dx*dx + dy*dy + dz*dz;
}
```

```
==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x4c4200 in line_length propagation.c:25:32      ← USO: l->origin.z
    #1 0x4c4400 in main propagation.c:35:18

  Uninitialized value was stored to memory at            ← PROPAGACIÓN: memcpy
    #0 0x... in memcpy
    #1 0x4c4100 in make_line propagation.c:18:5

  Uninitialized value was stored to memory at            ← PROPAGACIÓN: campo z
    #0 0x4c3e00 in make_point propagation.c:10:5

  Uninitialized value was created by a heap allocation   ← ORIGEN: malloc
    #0 0x... in malloc
    #1 0x4c3d80 in make_point propagation.c:9:22
```

Lectura del trace:
1. **Origen** (abajo): `malloc` en `make_point:9` — se allocó memoria
2. **Propagación 1**: en `make_point:10` — se almacenó sin inicializar `.z`
3. **Propagación 2**: `memcpy` en `make_line:18` — se copió el Point (incluyendo `.z` no inicializado)
4. **Uso** (arriba): `line_length:25` — se usó `l->origin.z` en operación aritmética

---

## 22. MSan en Rust unsafe: MaybeUninit y ptr

### Patrones comunes que MSan detecta

```
┌─────────────────────────────────────────────────────────────────┐
│        Fuentes de memoria no inicializada en Rust unsafe         │
│                                                                  │
│  1. MaybeUninit::uninit().assume_init()                          │
│     → Uso directo de valor no inicializado                       │
│                                                                  │
│  2. std::alloc::alloc(layout)                                    │
│     → Equivale a malloc: memoria sin inicializar                 │
│                                                                  │
│  3. std::mem::uninitialized() [deprecated]                       │
│     → UB instantáneo, MSan lo detecta al usar el valor           │
│                                                                  │
│  4. Vec::set_len() sin inicializar el rango nuevo                │
│     → Los elementos "nuevos" son basura                          │
│                                                                  │
│  5. ptr::read() de memoria nunca escrita                         │
│     → Lee basura                                                 │
│                                                                  │
│  6. FFI: función C retorna struct parcialmente inicializado      │
│     → Rust no sabe qué campos están inicializados                │
│                                                                  │
│  Safe Rust: NINGUNA de estas es posible sin unsafe               │
└─────────────────────────────────────────────────────────────────┘
```

### Patrón 1: MaybeUninit para optimización

```rust
use std::mem::MaybeUninit;

// CORRECTO: inicializar antes de assume_init
fn make_array_correct() -> [i32; 100] {
    let mut arr: [MaybeUninit<i32>; 100] = unsafe {
        MaybeUninit::uninit().assume_init()
        // Esto es OK: [MaybeUninit<T>] puede estar uninit
    };

    for i in 0..100 {
        arr[i] = MaybeUninit::new(i as i32);
    }

    // AHORA sí es seguro:
    unsafe {
        // transmute de [MaybeUninit<i32>; 100] a [i32; 100]
        std::mem::transmute::<_, [i32; 100]>(arr)
    }
}

// BUG: assume_init sin inicializar todo
fn make_array_buggy() -> [i32; 100] {
    let mut arr: [MaybeUninit<i32>; 100] = unsafe {
        MaybeUninit::uninit().assume_init()
    };

    for i in 0..50 {  // Solo inicializa la mitad
        arr[i] = MaybeUninit::new(i as i32);
    }

    unsafe {
        std::mem::transmute::<_, [i32; 100]>(arr)  // Bug: arr[50..99] es basura
    }
}
```

### Patrón 2: Vec::set_len

```rust
// BUG CLÁSICO: set_len sin inicializar
fn read_data_buggy() -> Vec<u8> {
    let mut buf = Vec::with_capacity(1024);
    unsafe {
        buf.set_len(1024);  // Bug: los 1024 bytes son basura
    }
    // MSan detectará al leer cualquier elemento
    buf
}

// CORRECTO: resize inicializa a 0
fn read_data_correct() -> Vec<u8> {
    let mut buf = vec![0u8; 1024];  // Inicializado a 0
    buf
}

// CORRECTO con set_len: llenar antes
fn read_data_correct2(reader: &mut impl std::io::Read) -> Vec<u8> {
    let mut buf = Vec::with_capacity(1024);
    unsafe {
        let n = reader.read(
            std::slice::from_raw_parts_mut(buf.as_mut_ptr(), 1024)
        ).unwrap();
        buf.set_len(n);  // OK: solo "revela" los bytes que read() escribió
    }
    buf
}
```

### Patrón 3: FFI con structs parcialmente inicializados

```rust
// Binding a una función C que llena parcialmente un struct
#[repr(C)]
struct CResult {
    status: i32,
    value: f64,
    error_msg: [u8; 256],
}

extern "C" {
    fn c_compute(input: i32, result: *mut CResult) -> i32;
}

fn call_c_function(input: i32) -> Result<f64, String> {
    unsafe {
        let mut result = MaybeUninit::<CResult>::uninit();
        let ret = c_compute(input, result.as_mut_ptr());

        let result = result.assume_init();
        // Bug potencial: si c_compute no inicializa TODOS los campos
        // para todos los códigos de retorno, MSan lo detectará

        if ret == 0 {
            Ok(result.value)  // ¿Está value inicializado?
        } else {
            // ¿Está error_msg inicializado?
            let msg = std::str::from_utf8(&result.error_msg)
                .unwrap_or("unknown");
            Err(msg.to_string())
        }
    }
}
```

---

## 23. Ejemplo completo en C: config_parser

Un parser de configuración con múltiples bugs de inicialización:

```c
// config_parser.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENTRIES 64
#define MAX_KEY_LEN 32
#define MAX_VAL_LEN 128

typedef struct {
    char key[MAX_KEY_LEN];
    char value[MAX_VAL_LEN];
    int type;       // 0=string, 1=int, 2=float, 3=bool
    int int_val;
    double float_val;
    int bool_val;
} ConfigEntry;

typedef struct {
    ConfigEntry entries[MAX_ENTRIES];
    int count;
    int version;    // Bug 1: nunca se inicializa
    char source[256];
} Config;

// ────────────────────────────────────────────────────────────────
// Bug 1: version nunca se inicializa
// ────────────────────────────────────────────────────────────────
Config *config_new(const char *source) {
    Config *cfg = (Config *)malloc(sizeof(Config));
    cfg->count = 0;
    strncpy(cfg->source, source, 255);
    cfg->source[255] = '\0';
    // Bug 1: cfg->version NO se inicializa
    return cfg;
}

// ────────────────────────────────────────────────────────────────
// Bug 2: solo inicializa ALGUNOS campos del tipo correcto
// ────────────────────────────────────────────────────────────────
int config_set(Config *cfg, const char *key, const char *value) {
    if (cfg->count >= MAX_ENTRIES) return -1;

    ConfigEntry *e = &cfg->entries[cfg->count];
    strncpy(e->key, key, MAX_KEY_LEN - 1);
    e->key[MAX_KEY_LEN - 1] = '\0';
    strncpy(e->value, value, MAX_VAL_LEN - 1);
    e->value[MAX_VAL_LEN - 1] = '\0';

    // Detectar tipo automáticamente
    char *endptr;
    long lval = strtol(value, &endptr, 10);
    if (*endptr == '\0' && endptr != value) {
        e->type = 1;       // int
        e->int_val = (int)lval;
        // Bug 2: NO inicializa float_val ni bool_val
    } else {
        double dval = strtod(value, &endptr);
        if (*endptr == '\0' && endptr != value) {
            e->type = 2;   // float
            e->float_val = dval;
            // Bug 2: NO inicializa int_val ni bool_val
        } else if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0) {
            e->type = 3;   // bool
            e->bool_val = (strcmp(value, "true") == 0);
            // Bug 2: NO inicializa int_val ni float_val
        } else {
            e->type = 0;   // string
            // Bug 2: NO inicializa int_val, float_val, ni bool_val
        }
    }

    cfg->count++;
    return 0;
}

// ────────────────────────────────────────────────────────────────
// Bug 3: accede al tipo incorrecto sin verificar
// ────────────────────────────────────────────────────────────────
int config_get_int(const Config *cfg, const char *key) {
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entries[i].key, key) == 0) {
            // Bug 3: retorna int_val sin verificar que type == 1
            return cfg->entries[i].int_val;  // Puede ser no inicializado
        }
    }
    // Bug 4: retorna sin valor si no encuentra la key
    int not_found;
    return not_found;  // No inicializado
}

double config_get_float(const Config *cfg, const char *key) {
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entries[i].key, key) == 0) {
            return cfg->entries[i].float_val;  // Puede ser no inicializado
        }
    }
    double not_found;
    return not_found;  // Bug 4: no inicializado
}

// ────────────────────────────────────────────────────────────────
// Función que expone Bug 1 y Bug 3/4
// ────────────────────────────────────────────────────────────────
void config_print_summary(const Config *cfg) {
    printf("Config v%d from %s:\n", cfg->version, cfg->source);
    //                     ↑ Bug 1: version no inicializado

    for (int i = 0; i < cfg->count; i++) {
        const ConfigEntry *e = &cfg->entries[i];
        printf("  %s = ", e->key);

        switch (e->type) {
            case 0: printf("\"%s\" (string)\n", e->value); break;
            case 1: printf("%d (int)\n", e->int_val); break;
            case 2: printf("%f (float)\n", e->float_val); break;
            case 3: printf("%s (bool)\n", e->bool_val ? "true" : "false"); break;
        }
    }
}

// ────────────────────────────────────────────────────────────────
// main: activa todos los bugs
// ────────────────────────────────────────────────────────────────
int main() {
    Config *cfg = config_new("test.conf");

    config_set(cfg, "port", "8080");
    config_set(cfg, "ratio", "3.14");
    config_set(cfg, "debug", "true");
    config_set(cfg, "name", "myservice");

    // Bug 1: imprime version no inicializado
    config_print_summary(cfg);

    // Bug 3: pide int_val de una entry que es float
    int ratio_int = config_get_int(cfg, "ratio");
    printf("Ratio as int: %d\n", ratio_int);  // int_val no inicializado

    // Bug 4: pide una key que no existe
    double missing = config_get_float(cfg, "timeout");
    printf("Timeout: %f\n", missing);  // Retorna basura

    free(cfg);
    return 0;
}
```

### Compilar y ejecutar

```bash
$ clang -fsanitize=memory -fsanitize-memory-track-origins=2 \
        -fno-omit-frame-pointer -g -O1 \
        config_parser.c -o config_parser
$ ./config_parser

==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x... in config_print_summary config_parser.c:92:5
    #1 0x... in main config_parser.c:112:5

  Uninitialized value was stored to memory at
    #0 0x... in config_new config_parser.c:31:26

  Uninitialized value was created by a heap allocation
    #0 0x... in malloc
    #1 0x... in config_new config_parser.c:30:24
```

### Corrección de todos los bugs

```c
// Bug 1: inicializar version
Config *config_new(const char *source) {
    Config *cfg = (Config *)calloc(1, sizeof(Config));  // calloc inicializa todo
    strncpy(cfg->source, source, 255);
    cfg->source[255] = '\0';
    cfg->version = 1;  // Explícito
    return cfg;
}

// Bug 2: inicializar TODOS los campos numéricos
int config_set(Config *cfg, const char *key, const char *value) {
    ConfigEntry *e = &cfg->entries[cfg->count];
    memset(e, 0, sizeof(ConfigEntry));  // Inicializar todo a 0
    // ... resto igual
}

// Bug 3: verificar tipo antes de acceder
int config_get_int(const Config *cfg, const char *key) {
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entries[i].key, key) == 0) {
            if (cfg->entries[i].type != 1) return 0;  // Tipo incorrecto
            return cfg->entries[i].int_val;
        }
    }
    return 0;  // Bug 4: valor por defecto en vez de basura
}
```

---

## 24. Ejemplo completo en Rust unsafe: sparse_array

Un array disperso implementado con punteros raw:

```rust
// sparse_array.rs
use std::alloc::{alloc, dealloc, Layout};
use std::ptr;

/// Array disperso: solo almacena valores en posiciones usadas.
/// Internamente usa un array denso de capacity elementos,
/// pero solo inicializa los que se asignan.
pub struct SparseArray<T: Copy> {
    data: *mut T,          // Buffer de datos
    present: *mut bool,    // Bitmap: ¿posición i tiene valor?
    capacity: usize,
    count: usize,          // Cantidad de posiciones con valor
}

impl<T: Copy> SparseArray<T> {
    pub fn new(capacity: usize) -> Self {
        let data_layout = Layout::array::<T>(capacity).unwrap();
        let present_layout = Layout::array::<bool>(capacity).unwrap();

        unsafe {
            let data = alloc(data_layout) as *mut T;
            let present = alloc(present_layout) as *mut bool;

            // Bug 1: NO inicializa present a false
            // Debería hacer: ptr::write_bytes(present, 0, capacity);
            // Sin inicializar, present[i] puede ser "true" para posiciones vacías

            SparseArray { data, present, capacity, count: 0 }
        }
    }

    pub fn set(&mut self, index: usize, value: T) {
        assert!(index < self.capacity, "index out of bounds");
        unsafe {
            if !*self.present.add(index) {
                self.count += 1;
            }
            ptr::write(self.data.add(index), value);
            ptr::write(self.present.add(index), true);
        }
    }

    pub fn get(&self, index: usize) -> Option<T> {
        assert!(index < self.capacity, "index out of bounds");
        unsafe {
            // Bug 1 se manifiesta aquí: present[index] puede ser
            // "true" (basura) para una posición nunca escrita
            if *self.present.add(index) {
                Some(ptr::read(self.data.add(index)))
                // Bug 2: si present era basura "true",
                // lee data[index] que no está inicializado
            } else {
                None
            }
        }
    }

    // ────────────────────────────────────────────────────────────────
    // Bug 3: sum_all lee data de posiciones donde present es basura
    // ────────────────────────────────────────────────────────────────
    pub fn sum_all(&self) -> T
    where
        T: std::ops::Add<Output = T> + Default,
    {
        let mut total = T::default();
        unsafe {
            for i in 0..self.capacity {
                if *self.present.add(i) {  // Bug: present no inicializado
                    total = total + ptr::read(self.data.add(i));
                    // Bug: data[i] no inicializado para posiciones falsas
                }
            }
        }
        total
    }

    // ────────────────────────────────────────────────────────────────
    // Bug 4: to_vec incluye posiciones "fantasma"
    // ────────────────────────────────────────────────────────────────
    pub fn to_vec(&self) -> Vec<(usize, T)> {
        let mut result = Vec::new();
        unsafe {
            for i in 0..self.capacity {
                if *self.present.add(i) {  // Bug: present no inicializado
                    result.push((i, ptr::read(self.data.add(i))));
                }
            }
        }
        result
    }
}

impl<T: Copy> Drop for SparseArray<T> {
    fn drop(&mut self) {
        unsafe {
            let data_layout = Layout::array::<T>(self.capacity).unwrap();
            let present_layout = Layout::array::<bool>(self.capacity).unwrap();
            dealloc(self.data as *mut u8, data_layout);
            dealloc(self.present as *mut u8, present_layout);
        }
    }
}

fn main() {
    let mut arr = SparseArray::<i32>::new(100);

    // Solo asignamos 3 posiciones
    arr.set(0, 10);
    arr.set(50, 20);
    arr.set(99, 30);

    // Bug 1+2: get de posiciones no asignadas puede devolver Some(basura)
    for i in 0..100 {
        if let Some(val) = arr.get(i) {
            println!("[{}] = {}", i, val);
            // Imprime posiciones fantasma con valores basura
        }
    }

    // Bug 3: suma incluye valores basura
    let total: i32 = arr.sum_all();
    println!("Total: {}", total);  // Debería ser 60, pero será basura

    // Bug 4: vec incluye posiciones fantasma
    let entries = arr.to_vec();
    println!("Entries: {}", entries.len());  // Debería ser 3
}
```

### Compilar y ejecutar con MSan

```bash
$ RUSTFLAGS="-Zsanitizer=memory -Zsanitizer-memory-track-origins=2" \
    cargo +nightly run -Zbuild-std --target x86_64-unknown-linux-gnu

==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x... in sparse_array::SparseArray<T>::get::... src/main.rs:52:16
    #1 0x... in sparse_array::main src/main.rs:102:28

  Uninitialized value was created by a heap allocation
    #0 0x... in alloc::alloc::alloc
    #1 0x... in sparse_array::SparseArray<T>::new src/main.rs:22:29
```

### Corrección

```rust
pub fn new(capacity: usize) -> Self {
    let data_layout = Layout::array::<T>(capacity).unwrap();
    let present_layout = Layout::array::<bool>(capacity).unwrap();

    unsafe {
        let data = alloc(data_layout) as *mut T;
        let present = alloc(present_layout) as *mut bool;

        // FIX: inicializar present a false (0)
        ptr::write_bytes(present, 0, capacity);

        SparseArray { data, present, capacity, count: 0 }
    }
}
```

---

## 25. MSan con tests unitarios

### En C

```bash
# Compilar tests con MSan:
clang -fsanitize=memory -fsanitize-memory-track-origins=2 \
      -fno-omit-frame-pointer -g -O1 \
      tests/test_config.c src/config.c -o test_config

# Ejecutar:
./test_config
# MSan reportará en el primer test que use un valor no inicializado
```

### En Rust

```bash
# Ejecutar tests con MSan:
RUSTFLAGS="-Zsanitizer=memory -Zsanitizer-memory-track-origins=2" \
    cargo +nightly test -Zbuild-std \
    --target x86_64-unknown-linux-gnu

# Solo un test específico:
RUSTFLAGS="-Zsanitizer=memory -Zsanitizer-memory-track-origins=2" \
    cargo +nightly test -Zbuild-std \
    --target x86_64-unknown-linux-gnu \
    -- test_sparse_get
```

### Estructura de test para MSan

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sparse_get_unset_returns_none() {
        let arr = SparseArray::<i32>::new(10);
        // Si present no está inicializado, esto puede devolver Some(basura)
        // MSan detectará la lectura de present[5] no inicializado
        assert_eq!(arr.get(5), None);
    }

    #[test]
    fn test_sparse_set_then_get() {
        let mut arr = SparseArray::<i32>::new(10);
        arr.set(3, 42);
        // Esto debería funcionar sin MSan error
        assert_eq!(arr.get(3), Some(42));
    }

    #[test]
    fn test_sparse_sum_only_set_values() {
        let mut arr = SparseArray::<i32>::new(100);
        arr.set(0, 10);
        arr.set(50, 20);
        arr.set(99, 30);
        // MSan detectará si sum_all lee posiciones no inicializadas
        assert_eq!(arr.sum_all(), 60);
    }
}
```

---

## 26. MSan con fuzzing (cargo-fuzz)

### Configurar cargo-fuzz con MSan

```bash
# Ejecutar fuzzing con MSan:
RUSTFLAGS="-Zsanitizer=memory -Zsanitizer-memory-track-origins=2" \
    cargo +nightly fuzz run fuzz_target \
    -Zbuild-std --target x86_64-unknown-linux-gnu \
    -- -max_len=1024
```

### Harness ejemplo para sparse_array

```rust
// fuzz/fuzz_targets/fuzz_sparse.rs
#![no_main]

use libfuzzer_sys::fuzz_target;
use arbitrary::Arbitrary;

#[derive(Arbitrary, Debug)]
enum SparseOp {
    Set { index: u8, value: i32 },
    Get { index: u8 },
    SumAll,
    ToVec,
}

fuzz_target!(|ops: Vec<SparseOp>| {
    let capacity = 64;
    let mut arr = sparse_array::SparseArray::<i32>::new(capacity);

    for op in ops {
        match op {
            SparseOp::Set { index, value } => {
                let idx = (index as usize) % capacity;
                arr.set(idx, value);
            }
            SparseOp::Get { index } => {
                let idx = (index as usize) % capacity;
                let _ = arr.get(idx);
                // MSan detectará si get() lee presente no inicializado
            }
            SparseOp::SumAll => {
                let _ = arr.sum_all();
                // MSan detectará si sum_all lee datos no inicializados
            }
            SparseOp::ToVec => {
                let _ = arr.to_vec();
            }
        }
    }
});
```

### MSan con fuzzing: consideraciones

```
┌─────────────────────────────────────────────────────────────────┐
│           MSan + fuzzing: consideraciones especiales             │
│                                                                  │
│  1. COMPILACIÓN MUY LENTA                                        │
│     -Zbuild-std recompila toda la stdlib                         │
│     Primera compilación: 5-15 minutos                            │
│     Recompilaciones: 1-3 minutos                                 │
│                                                                  │
│  2. OVERHEAD DE EJECUCIÓN                                        │
│     MSan es ~3x más lento que sin sanitizer                      │
│     Con origin tracking: ~5x más lento                           │
│     → Menos iteraciones de fuzzing por segundo                   │
│                                                                  │
│  3. NO COMPATIBLE CON ASan                                       │
│     No puedes usar MSan y ASan simultáneamente                   │
│     Estrategia: fuzzear con ASan primero, luego con MSan         │
│                                                                  │
│  4. FALSOS POSITIVOS POSIBLES                                    │
│     Si alguna dependencia no se compiló con MSan                 │
│     libFuzzer internamente está instrumentado (ok)               │
│     Pero bibliotecas C enlazadas pueden no estarlo               │
│                                                                  │
│  5. STRATEGY                                                     │
│     Paso 1: cargo fuzz run target (sin sanitizer, buscar panics) │
│     Paso 2: cargo fuzz run target con ASan (buscar memory bugs)  │
│     Paso 3: cargo fuzz run target con MSan (buscar uninit bugs)  │
│     Cada paso usa el corpus del anterior                          │
└─────────────────────────────────────────────────────────────────┘
```

---

## 27. MSAN_OPTIONS: configuración completa

MSan se configura via la variable de entorno `MSAN_OPTIONS`:

```bash
# Sintaxis: clave=valor separados por ':'
MSAN_OPTIONS="option1=value1:option2=value2" ./program
```

### Opciones principales

```
┌───────────────────────────────────┬─────────┬──────────────────────────────┐
│ Opción                            │ Default │ Descripción                   │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ halt_on_error                     │ 0       │ 1=abortar en primer error,   │
│                                   │         │ 0=continuar (solo MSan)      │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ print_stats                       │ 0       │ 1=imprimir estadísticas al   │
│                                   │         │ finalizar                    │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ report_umrs                       │ 1       │ 1=reportar usos de memoria   │
│                                   │         │ no inicializada              │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ wrap_signals                      │ 1       │ 1=interceptar señales        │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ poison_in_dtor                    │ 0       │ 1=marcar como no inicializado│
│                                   │         │ en destructores C++          │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ poison_in_free                    │ 1       │ 1=marcar como no inicializado│
│                                   │         │ al liberar con free()        │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ keep_going                        │ 0       │ Alias de halt_on_error=0     │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ exit_code                         │ 77      │ Código de salida en error    │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ symbolize                         │ 1       │ 1=resolver símbolos en       │
│                                   │         │ stack traces                 │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ external_symbolizer_path          │ ""      │ Ruta a llvm-symbolizer       │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ log_path                          │ stderr  │ Archivo para los reportes    │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ verbosity                         │ 0       │ Nivel de verbosidad (0-3)    │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ strip_path_prefix                 │ ""      │ Prefijo a eliminar de paths  │
│                                   │         │ en reportes                  │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ history_size                      │ 7       │ Log2 del tamaño de la tabla  │
│                                   │         │ de origins (2^7=128 entries) │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ origin_history_size               │ 0       │ Tamaño del historial de      │
│                                   │         │ origins por allocación       │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ origin_history_per_stack_limit    │ 20000   │ Límite de stack frames en    │
│                                   │         │ historial de origins         │
├───────────────────────────────────┼─────────┼──────────────────────────────┤
│ store_context_size                │ 0       │ Número de frames a guardar   │
│                                   │         │ en origin stores (nivel 2)   │
└───────────────────────────────────┴─────────┴──────────────────────────────┘
```

### Ejemplos de uso

```bash
# Encontrar TODOS los errores (no parar en el primero):
MSAN_OPTIONS="halt_on_error=0" ./program

# Debugging verbose:
MSAN_OPTIONS="verbosity=2:print_stats=1" ./program

# Log a archivo:
MSAN_OPTIONS="log_path=/tmp/msan.log" ./program

# Limpiar paths en reportes:
MSAN_OPTIONS="strip_path_prefix=/home/user/project/" ./program

# Para fuzzing (halt inmediato para que libFuzzer capture el crash):
MSAN_OPTIONS="halt_on_error=1:symbolize=1:exit_code=77" ./fuzz_target
```

---

## 28. Impacto en rendimiento detallado

```
┌──────────────────────────────────────────────────────────────────────┐
│              Impacto de MSan en rendimiento                           │
│                                                                      │
│  Recurso          │ Sin MSan  │ MSan base │ MSan+origins │ MSan+orig2│
│  ─────────────────┼───────────┼───────────┼──────────────┼───────────│
│  CPU (tiempo)     │ 1x        │ ~3x       │ ~4x          │ ~5x       │
│  RAM              │ 1x        │ ~2x       │ ~2.5x        │ ~3x       │
│  Tamaño binario   │ 1x        │ ~2-3x     │ ~3-4x        │ ~3-4x    │
│  Velocidad build  │ 1x        │ ~1.5x     │ ~1.5x        │ ~1.5x    │
│  Build con -Zbuild│ 1x        │ ~5-10x    │ ~5-10x       │ ~5-10x   │
│  -std (Rust)      │           │ (primera) │ (primera)    │(primera)  │
│                                                                      │
│  Comparación con otros sanitizers:                                   │
│                                                                      │
│  ASan:   CPU ~2x,   RAM ~2-3x                                       │
│  MSan:   CPU ~3-5x, RAM ~2-3x                                       │
│  TSan:   CPU ~5-15x, RAM ~5-10x                                     │
│  UBSan:  CPU ~1.2x, RAM ~1x  (el más ligero)                        │
│                                                                      │
│  MSan es más lento que ASan porque:                                  │
│  1. Shadow 1:1 (ASan usa 8:1) → más shadow memory que gestionar    │
│  2. Propagación bit-precise → más chequeos por operación            │
│  3. Origin tracking → almacena stack traces en cada store           │
│  4. -Zbuild-std (Rust) → la stdlib también es más lenta            │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 29. La complicación de la stdlib

### El problema central

MSan requiere que **todo** el código que toca la memoria de tu programa esté instrumentado. Si una biblioteca no instrumentada escribe datos, MSan no actualiza el shadow memory, y cuando tu código lee esos datos, MSan cree que no están inicializados.

```
┌─────────────────────────────────────────────────────────────────┐
│         MSan y bibliotecas no instrumentadas                     │
│                                                                  │
│  ┌──────────────────────┐                                        │
│  │  Tu código (con MSan) │                                       │
│  │  int *p = malloc(4); │──▶ malloc interceptado por MSan ✓     │
│  │  lib_fill(p);        │──▶ función de libexternal              │
│  │  printf("%d", *p);   │                                       │
│  └──────────────────────┘                                        │
│           │                                                      │
│           ▼                                                      │
│  ┌──────────────────────┐                                        │
│  │  libexternal (sin MSan│                                       │
│  │  void lib_fill(int*p)│                                       │
│  │  { *p = 42; }        │ ← Este store NO actualiza shadow     │
│  └──────────────────────┘                                        │
│           │                                                      │
│           ▼                                                      │
│  Shadow de *p sigue siendo 0xFF (no inicializado)                │
│  Tu código lee *p → MSan reporta FALSO POSITIVO                 │
└─────────────────────────────────────────────────────────────────┘
```

### Solución: recompilar dependencias

```bash
# En C: recompilar TODAS las bibliotecas con MSan
clang -fsanitize=memory -fsanitize-memory-track-origins=2 \
      -c libexternal/src/*.c -o libexternal/lib.o

# En Rust: -Zbuild-std recompila core/alloc/std
# Pero dependencias de crates.io NO se recompilan automáticamente con MSan
# (sí se recompilan como cualquier dependencia Rust, así que
#  si no tienen código C enlazado, están instrumentadas)
```

### ¿Qué funciones intercepta MSan del runtime?

MSan intercepta (~250 funciones de libc):

```
┌─────────────────────────────────────────────────────────────────┐
│  Funciones de libc interceptadas por MSan (selección)            │
│                                                                  │
│  Memoria:  malloc, calloc, realloc, free, mmap, munmap           │
│            memcpy, memmove, memset, memcmp, bzero                │
│                                                                  │
│  String:   strlen, strcpy, strncpy, strcat, strcmp, strncmp       │
│            strchr, strrchr, strstr, strtol, strtod                │
│                                                                  │
│  I/O:      read, write, pread, pwrite, readv, writev             │
│            recv, send, recvmsg, sendmsg                          │
│            fread, fwrite, fgets, fputs                            │
│                                                                  │
│  System:   stat, fstat, lstat, open, close, pipe                 │
│            socket, bind, listen, accept, connect                 │
│            gettimeofday, clock_gettime                           │
│                                                                  │
│  Cada interceptor actualiza el shadow memory correctamente:      │
│  - read(fd, buf, n) marca buf[0..retval] como inicializado      │
│  - calloc(n, size) marca como inicializado (a diferencia de     │
│    malloc que deja como no inicializado)                          │
│  - memset(p, v, n) marca p[0..n] como inicializado              │
└─────────────────────────────────────────────────────────────────┘
```

---

## 30. MSan y bibliotecas externas

### El problema con dependencias C en Rust

```
┌─────────────────────────────────────────────────────────────────┐
│  Caso: crate Rust que enlaza biblioteca C vía FFI                │
│                                                                  │
│  Tu código Rust (con MSan)                                       │
│       │                                                          │
│       ├──▶ crate "zip" (Rust, se recompila con MSan ✓)          │
│       │       │                                                  │
│       │       └──▶ libz-sys (C, enlaza libz.a precompilada)     │
│       │                ↑                                         │
│       │                └── NO instrumentada con MSan ✗           │
│       │                                                          │
│       └──▶ crate "openssl" (Rust ✓)                             │
│               │                                                  │
│               └──▶ openssl-sys (C, enlaza libssl.so)            │
│                        ↑                                         │
│                        └── NO instrumentada con MSan ✗           │
│                                                                  │
│  Resultado: falsos positivos masivos con cualquier dato          │
│  que pase por libz o libssl                                      │
└─────────────────────────────────────────────────────────────────┘
```

### Soluciones

```bash
# Solución 1: Recompilar la biblioteca C con MSan
# (requiere acceso al código fuente de la dependencia)
CC=clang CFLAGS="-fsanitize=memory -fsanitize-memory-track-origins=2" \
    cargo +nightly build -Zbuild-std --target x86_64-unknown-linux-gnu

# Solución 2: Usar la variante pure-Rust si existe
# En Cargo.toml:
[dependencies]
# En vez de openssl, usar rustls (pure Rust, sin C)
rustls = "0.21"
# En vez de libz-sys, usar miniz_oxide (pure Rust)
flate2 = { version = "1.0", default-features = false, features = ["rust_backend"] }

# Solución 3: Marcar la biblioteca con __msan_unpoison
# En el wrapper FFI:
extern "C" {
    fn external_fill(buf: *mut u8, len: usize);
}

fn safe_external_fill(buf: &mut [u8]) {
    unsafe {
        external_fill(buf.as_mut_ptr(), buf.len());
        // Decirle a MSan que la biblioteca inicializó el buffer:
        // __msan_unpoison(buf.as_ptr() as *const _, buf.len());
    }
}
```

### __msan_unpoison: la válvula de escape

```c
// En C, incluir el header de MSan:
#include <sanitizer/msan_interface.h>

void use_external_lib(void) {
    char buf[256];

    // Función de biblioteca externa (no instrumentada)
    external_lib_fill(buf, 256);

    // Decirle a MSan que buf está inicializado:
    __msan_unpoison(buf, 256);

    // Ahora MSan no reportará falso positivo
    process(buf);
}
```

```rust
// En Rust:
extern "C" {
    fn __msan_unpoison(addr: *const std::ffi::c_void, size: usize);
}

unsafe fn use_external_lib() {
    let mut buf = [0u8; 256];

    // Función C externa no instrumentada
    external_fill(buf.as_mut_ptr(), buf.len());

    // Marcar como inicializado para MSan
    __msan_unpoison(
        buf.as_ptr() as *const std::ffi::c_void,
        buf.len(),
    );
}
```

**ADVERTENCIA**: `__msan_unpoison` suprime la detección. Si la biblioteca realmente NO inicializó el buffer, estarás ocultando un bug real. Úsalo **solo** cuando estés seguro de que la biblioteca sí inicializa la memoria.

---

## 31. Falsos positivos en MSan

MSan tiene una tasa de falsos positivos más alta que ASan debido a la dependencia de que todo el código esté instrumentado.

### Causas comunes de falsos positivos

```
┌──────────────────────────────────────────────────────────────────────┐
│             Falsos positivos en MSan                                  │
│                                                                      │
│  1. Biblioteca no instrumentada                                      │
│     Causa: función externa escribe datos sin actualizar shadow       │
│     Solución: recompilar con MSan o usar __msan_unpoison             │
│                                                                      │
│  2. Inline assembly                                                  │
│     Causa: MSan no puede rastrear lo que hace el asm                 │
│     Solución: __msan_unpoison en los outputs del asm                 │
│                                                                      │
│  3. Memory-mapped I/O                                                │
│     Causa: hardware escribe en memoria, MSan no lo sabe              │
│     Solución: __msan_unpoison después de cada lectura MMIO           │
│                                                                      │
│  4. Compilación parcial                                              │
│     Causa: solo algunos .o tienen MSan                               │
│     Solución: TODOS los archivos con -fsanitize=memory               │
│                                                                      │
│  5. Optimizaciones agresivas (-O2/-O3)                               │
│     Causa: el optimizador reordena operaciones de forma que          │
│            confunde al tracking de MSan                              │
│     Solución: usar -O1                                               │
│                                                                      │
│  6. Uso legítimo de padding                                          │
│     Causa: struct tiene padding entre campos (por alignment)         │
│            y se copia con memcpy toda la struct                      │
│     Solución: memset la struct entera antes de llenar campos         │
│              o usar __msan_unpoison en el padding                    │
└──────────────────────────────────────────────────────────────────────┘
```

### El caso del padding

```c
// Falso positivo clásico: padding de struct

struct Example {
    char a;         // offset 0, size 1
    // 3 bytes de padding (offset 1-3)
    int b;          // offset 4, size 4
};

struct Example e;
e.a = 'X';
e.b = 42;
// El padding entre a y b NO está inicializado

// Si mandamos la struct por red:
write(fd, &e, sizeof(e));  // MSan reporta: padding no inicializado

// Esto es técnicamente correcto (información leak en el padding)
// pero a veces es un falso positivo funcional.

// Solución defensiva (también evita info leak):
memset(&e, 0, sizeof(e));
e.a = 'X';
e.b = 42;
write(fd, &e, sizeof(e));  // OK, padding es 0
```

---

## 32. MSan vs Valgrind (Memcheck)

Valgrind Memcheck también detecta lecturas de memoria no inicializada. Comparación:

```
┌──────────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto                  │ MSan                 │ Valgrind Memcheck    │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Mecanismo                │ Instrumentación en   │ Emulación de CPU     │
│                          │ compilación          │ (binary translation) │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Velocidad                │ ~3-5x overhead       │ ~20-50x overhead     │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Requiere recompilar      │ SÍ (todo)            │ NO                   │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Compilador requerido     │ Solo Clang           │ Cualquiera           │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Plataformas              │ Linux x86_64         │ Linux, macOS,        │
│                          │ (parcial AArch64)    │ FreeBSD, Solaris     │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Granularidad             │ Bit-preciso          │ Bit-preciso (V-bits) │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Propagación              │ SÍ                   │ SÍ                   │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Origin tracking          │ Sí (-track-origins)  │ Sí (--track-origins) │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Falsos positivos         │ Más (por libs no     │ Menos (ve todo el    │
│                          │ instrumentadas)      │ binario)             │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ También detecta          │ SOLO uninit          │ uninit + leaks +     │
│                          │                      │ overflows + UAF      │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Con fuzzing              │ SÍ (integrado)       │ Muy lento            │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Recomendación            │ CI, fuzzing, testing  │ Debugging puntual,   │
│                          │ continuo              │ binarios sin fuente  │
└──────────────────────────┴──────────────────────┴──────────────────────┘
```

### ¿Cuándo usar cada uno?

```
┌─────────────────────────────────────────────────────────────────┐
│  Usa MSan cuando:                                                │
│  • Puedes recompilar TODO con Clang                              │
│  • Necesitas integrar en CI (velocidad importa)                  │
│  • Quieres combinar con fuzzing (cargo-fuzz, libFuzzer)          │
│  • Estás en Linux x86_64                                         │
│                                                                  │
│  Usa Valgrind cuando:                                            │
│  • No puedes recompilar (binario cerrado)                        │
│  • Quieres detección de uninit + leaks + overflows juntos        │
│  • Estás en macOS o en una plataforma que MSan no soporta        │
│  • Tienes dependencias que no puedes recompilar con MSan         │
│  • Solo necesitas un análisis puntual (no CI continuo)           │
│                                                                  │
│  Usa AMBOS cuando:                                               │
│  • MSan en CI (rápido, automatizado)                             │
│  • Valgrind para verificación extra pre-release                  │
│  • MSan puede tener falso negativo si un bug solo se             │
│    manifiesta con una biblioteca no instrumentada                │
└─────────────────────────────────────────────────────────────────┘
```

---

## 33. Limitaciones de MSan

```
┌──────────────────────────────────────────────────────────────────────┐
│                   Limitaciones de MSan                                │
│                                                                      │
│  1. SOLO CLANG                                                       │
│     No hay soporte en GCC, MSVC, ni otros compiladores.              │
│     En Rust: solo nightly con -Zsanitizer=memory.                    │
│                                                                      │
│  2. SOLO LINUX x86_64                                                │
│     Soporte parcial en AArch64 y NetBSD.                             │
│     NO funciona en macOS, Windows, ni otras plataformas.             │
│     → Si tu CI es en macOS, no puedes usar MSan.                    │
│                                                                      │
│  3. REQUIERE RECOMPILAR TODO                                         │
│     Toda dependencia debe compilarse con MSan.                       │
│     Si no, falsos positivos garantizados.                            │
│     En Rust: -Zbuild-std recompila la stdlib (~5-15 min inicial).   │
│                                                                      │
│  4. NO COMPATIBLE CON OTROS SANITIZERS                               │
│     MSan y ASan son incompatibles (shadow memory conflicta).         │
│     MSan y TSan son incompatibles.                                   │
│     Solo MSan + UBSan pueden combinarse.                             │
│                                                                      │
│  5. NO DETECTA OVERFLOWS NI UAF                                      │
│     MSan SOLO detecta uso de valores no inicializados.               │
│     Para otros bugs, necesitas ASan (overflows, UAF)                 │
│     o TSan (data races).                                             │
│                                                                      │
│  6. OVERHEAD DE MEMORIA ALTO                                         │
│     Shadow 1:1 + origin tracking = ~3x RAM.                         │
│     Puede ser prohibitivo para programas que usan mucha memoria.     │
│                                                                      │
│  7. NO DETECTA UNINIT EN REGISTROS x87                               │
│     Operaciones de punto flotante x87 (legacy) no rastreadas.        │
│     SSE/AVX sí están soportados.                                    │
│                                                                      │
│  8. PUEDE PERDER BUGS POR OPTIMIZACIÓN                               │
│     -O2/-O3 pueden eliminar reads que serían bugs.                   │
│     El compilador elimina el read "optimizando" y MSan no lo ve.    │
│                                                                      │
│  9. INLINE ASSEMBLY NO RASTREADO                                     │
│     MSan no puede instrumentar asm volátil.                          │
│     Necesitas __msan_unpoison manual en los outputs.                 │
│                                                                      │
│  10. TAMAÑO DE ADDRESS SPACE                                         │
│      MSan reserva regiones de virtual memory para shadow.            │
│      En sistemas con ASLR agresivo, puede haber conflictos.         │
└──────────────────────────────────────────────────────────────────────┘
```

### Incompatibilidades entre sanitizers

```
┌────────────────┬──────┬──────┬──────┬──────┐
│                │ ASan │ MSan │ TSan │ UBSan│
├────────────────┼──────┼──────┼──────┼──────┤
│ ASan           │  -   │  ✗   │  ✗   │  ✓   │
│ MSan           │  ✗   │  -   │  ✗   │  ✓   │
│ TSan           │  ✗   │  ✗   │  -   │  ✓   │
│ UBSan          │  ✓   │  ✓   │  ✓   │  -   │
└────────────────┴──────┴──────┴──────┴──────┘

✓ = compatibles, pueden usarse juntos
✗ = incompatibles, NO pueden usarse juntos

UBSan es el único que combina con todos.
ASan, MSan y TSan son mutuamente exclusivos
(usan regiones de shadow memory que conflictan).
```

---

## 34. Errores comunes

### Error 1: olvidar -Zbuild-std en Rust

```bash
# ERROR: ejecutar sin -Zbuild-std
RUSTFLAGS="-Zsanitizer=memory" cargo +nightly run
# Resultado: falsos positivos masivos por stdlib no instrumentada

# Síntoma típico:
# ==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
#     #0 in std::io::stdio::print ...
# ← Falso positivo: println! usa datos "no inicializados"
#   porque la stdlib no tiene instrumentación MSan
```

### Error 2: olvidar --target con -Zbuild-std

```bash
# ERROR: -Zbuild-std sin --target
RUSTFLAGS="-Zsanitizer=memory" cargo +nightly run -Zbuild-std
# error: --target must be specified with -Zbuild-std

# CORRECTO:
RUSTFLAGS="-Zsanitizer=memory" \
    cargo +nightly run -Zbuild-std \
    --target x86_64-unknown-linux-gnu
```

### Error 3: enlazar con biblioteca C no instrumentada

```bash
# En build.rs:
# println!("cargo:rustc-link-lib=external_lib");
#
# Si external_lib no se compiló con MSan, habrá falsos positivos.
#
# Solución: recompilar external_lib con MSan en build.rs:
# cc::Build::new()
#     .file("external_lib/src/lib.c")
#     .flag("-fsanitize=memory")
#     .flag("-fsanitize-memory-track-origins=2")
#     .compile("external_lib");
```

### Error 4: ignorar origin tracking

```bash
# Sin origin tracking:
==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x4c3f20 in process src/main.c:45:9
# ← ¿De dónde viene el valor no inicializado? No se sabe.

# Con origin tracking nivel 2:
==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x4c3f20 in process src/main.c:45:9

  Uninitialized value was stored to memory at
    #0 0x4c3d80 in copy_data src/io.c:67:5

  Uninitialized value was created by a heap allocation
    #0 0x... in malloc
    #1 0x4c3c00 in init src/setup.c:12:25
# ← Ahora sabes exactamente la cadena de propagación
```

### Error 5: usar MSan en macOS o Windows

```bash
# MSan NO funciona en macOS:
$ clang -fsanitize=memory test.c
# clang: error: unsupported option '-fsanitize=memory' for target 'x86_64-apple-darwin'

# MSan NO funciona en Windows:
# MSVC no soporta MSan en absoluto
# Clang on Windows tampoco

# Solución: usar Docker con Linux o VM
# docker run -v $(pwd):/code -w /code ubuntu:22.04 bash
# apt install clang && clang -fsanitize=memory ...
```

### Error 6: mezclar MSan con ASan

```bash
# ERROR: usar ambos
clang -fsanitize=memory,address test.c
# clang: error: invalid argument '-fsanitize=address' not allowed with
#        '-fsanitize=memory'

# En Rust:
RUSTFLAGS="-Zsanitizer=memory -Zsanitizer=address" cargo +nightly build
# error: only one sanitizer is supported at a time

# Solución: ejecutar en builds separados
# Build 1: ASan para overflows y UAF
# Build 2: MSan para uninit
```

---

## 35. Programa de práctica: packet_decoder

### Descripción

Un decodificador de paquetes binarios que tiene múltiples bugs de inicialización. Implementa un protocolo simple donde los paquetes tienen header, payload, y checksum.

### Versión en C

```c
// packet_decoder.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════
//  Protocolo de paquetes
// ═══════════════════════════════════════════════════════════════
//
//  Formato de paquete:
//  ┌──────────┬──────────┬──────────┬──────────┬──────────────┐
//  │ magic(2) │ type(1)  │ flags(1) │ len(2)   │ payload(len) │
//  └──────────┴──────────┴──────────┴──────────┴──────────────┘
//  │ checksum(4)                                               │
//  └───────────────────────────────────────────────────────────┘
//
//  magic: 0xCAFE
//  type: 0=data, 1=ack, 2=error, 3=keepalive
//  flags: bit 0=compressed, bit 1=encrypted, bits 2-7=reserved
//  len: payload length (big-endian)
//  payload: len bytes of data
//  checksum: CRC32 of header + payload

#define PACKET_MAGIC 0xCAFE
#define MAX_PAYLOAD 1024

typedef struct {
    uint16_t magic;
    uint8_t type;
    uint8_t flags;
    uint16_t length;
} PacketHeader;

typedef struct {
    PacketHeader header;
    uint8_t *payload;
    uint32_t checksum;
    int valid;          // ← Campo de estado de validación
    char error_msg[64]; // ← Mensaje de error si no es válido
} Packet;

// ────────────────────────────────────────────────────────────────
// Bug 1: decode_header no inicializa TODOS los campos del Packet
// ────────────────────────────────────────────────────────────────
Packet *packet_decode(const uint8_t *data, size_t len) {
    Packet *pkt = (Packet *)malloc(sizeof(Packet));

    if (len < sizeof(PacketHeader)) {
        pkt->valid = 0;
        strncpy(pkt->error_msg, "too short", 63);
        pkt->error_msg[63] = '\0';
        // Bug 1: NO inicializa pkt->header, pkt->payload, pkt->checksum
        return pkt;
    }

    // Decodificar header
    pkt->header.magic = (data[0] << 8) | data[1];
    pkt->header.type = data[2];
    pkt->header.flags = data[3];
    pkt->header.length = (data[4] << 8) | data[5];

    // Verificar magic
    if (pkt->header.magic != PACKET_MAGIC) {
        pkt->valid = 0;
        snprintf(pkt->error_msg, 64, "bad magic: 0x%04X", pkt->header.magic);
        // Bug 1 variante: NO inicializa pkt->payload ni pkt->checksum
        return pkt;
    }

    // Verificar longitud
    size_t expected = sizeof(PacketHeader) + pkt->header.length + 4;
    if (len < expected) {
        pkt->valid = 0;
        snprintf(pkt->error_msg, 64, "truncated: need %zu, got %zu",
                 expected, len);
        // Bug 1: payload y checksum no inicializados
        return pkt;
    }

    // ────────────────────────────────────────────────────────────────
    // Bug 2: allocar payload sin inicializar los bytes restantes
    // ────────────────────────────────────────────────────────────────
    pkt->payload = (uint8_t *)malloc(MAX_PAYLOAD);
    // Solo copia header.length bytes, pero aloca MAX_PAYLOAD
    memcpy(pkt->payload, data + sizeof(PacketHeader), pkt->header.length);
    // Bug 2: payload[header.length .. MAX_PAYLOAD-1] no inicializado

    // Decodificar checksum (últimos 4 bytes)
    size_t cksum_offset = sizeof(PacketHeader) + pkt->header.length;
    pkt->checksum = (data[cksum_offset] << 24) |
                    (data[cksum_offset + 1] << 16) |
                    (data[cksum_offset + 2] << 8) |
                    data[cksum_offset + 3];

    pkt->valid = 1;
    // Bug 3: NO inicializa error_msg cuando el paquete es válido
    return pkt;
}

// ────────────────────────────────────────────────────────────────
// Bug 3: accede a error_msg sin verificar valid
// ────────────────────────────────────────────────────────────────
void packet_print(const Packet *pkt) {
    if (!pkt->valid) {
        printf("Invalid packet: %s\n", pkt->error_msg);
        // Acceder a header cuando valid=0:
        printf("  Magic was: 0x%04X\n", pkt->header.magic);
        // Bug 1: header.magic puede no estar inicializado
        //         (si el paquete era "too short")
        return;
    }

    printf("Packet type=%d flags=0x%02X len=%d\n",
           pkt->header.type, pkt->header.flags, pkt->header.length);

    // Imprimir payload como hex
    printf("Payload: ");
    for (int i = 0; i < MAX_PAYLOAD && i < 32; i++) {
        printf("%02X ", pkt->payload[i]);
        // Bug 2: imprime bytes no inicializados más allá de length
    }
    printf("\n");

    // Imprimir error_msg (debería estar vacío para paquetes válidos)
    printf("Status: %s\n", pkt->valid ? "OK" :  pkt->error_msg);
    // Si valid=1, no accede a error_msg (OK gracias al ternario)
}

// ────────────────────────────────────────────────────────────────
// Bug 4: copiar struct con padding no inicializado
// ────────────────────────────────────────────────────────────────
typedef struct {
    uint8_t type;        // 1 byte
    // 3 bytes de padding
    uint32_t count;      // 4 bytes
    uint16_t id;         // 2 bytes
    // 2 bytes de padding
} PacketStats;

PacketStats compute_stats(const Packet *packets[], int n) {
    PacketStats stats;
    // Bug 4: no inicializa padding
    stats.type = 0;
    stats.count = (uint32_t)n;
    stats.id = 0;

    for (int i = 0; i < n; i++) {
        if (packets[i]->valid) {
            stats.id++;
        }
    }

    return stats;  // Retorna struct con padding no inicializado
}

void send_stats(const PacketStats *stats) {
    // Bug 4: envía struct con padding no inicializado
    // Esto es un information leak potencial
    write(1, stats, sizeof(PacketStats));
    // MSan detecta: padding bytes no inicializados
}

// ────────────────────────────────────────────────────────────────
void packet_free(Packet *pkt) {
    if (pkt->payload) {  // Bug potencial si payload no fue inicializado
        free(pkt->payload);
    }
    free(pkt);
}

int main() {
    // Paquete válido
    uint8_t valid_data[] = {
        0xCA, 0xFE,   // magic
        0x00,          // type=data
        0x01,          // flags=compressed
        0x00, 0x05,    // length=5
        'H', 'e', 'l', 'l', 'o',  // payload
        0x12, 0x34, 0x56, 0x78     // checksum
    };

    // Paquete demasiado corto
    uint8_t short_data[] = { 0xCA, 0xFE };

    // Paquete con magic incorrecto
    uint8_t bad_magic[] = {
        0xDE, 0xAD,
        0x00, 0x00, 0x00, 0x03,
        'a', 'b', 'c',
        0x00, 0x00, 0x00, 0x00
    };

    Packet *p1 = packet_decode(valid_data, sizeof(valid_data));
    Packet *p2 = packet_decode(short_data, sizeof(short_data));
    Packet *p3 = packet_decode(bad_magic, sizeof(bad_magic));

    printf("=== Packet 1 (valid) ===\n");
    packet_print(p1);

    printf("\n=== Packet 2 (too short) ===\n");
    packet_print(p2);

    printf("\n=== Packet 3 (bad magic) ===\n");
    packet_print(p3);

    // Stats
    const Packet *all[] = {p1, p2, p3};
    PacketStats stats = compute_stats(all, 3);
    printf("\n=== Stats ===\n");
    send_stats(&stats);

    packet_free(p1);
    packet_free(p2);
    packet_free(p3);

    return 0;
}
```

### Versión en Rust unsafe

```rust
// src/main.rs
use std::alloc::{alloc, alloc_zeroed, dealloc, Layout};
use std::ptr;

const PACKET_MAGIC: u16 = 0xCAFE;
const MAX_PAYLOAD: usize = 1024;

#[repr(C)]
struct PacketHeader {
    magic: u16,
    pkt_type: u8,
    flags: u8,
    length: u16,
}

struct Packet {
    header: PacketHeader,
    payload: *mut u8,      // Buffer de datos
    checksum: u32,
    valid: bool,
    error_msg: [u8; 64],
}

// ────────────────────────────────────────────────────────────────
// Bug 1: campos no inicializados en paths de error
// ────────────────────────────────────────────────────────────────
unsafe fn packet_decode(data: &[u8]) -> *mut Packet {
    let layout = Layout::new::<Packet>();
    let pkt = alloc(layout) as *mut Packet;

    if data.len() < std::mem::size_of::<PacketHeader>() {
        (*pkt).valid = false;
        let msg = b"too short";
        ptr::copy_nonoverlapping(
            msg.as_ptr(), (*pkt).error_msg.as_mut_ptr(), msg.len(),
        );
        (*pkt).error_msg[msg.len()] = 0;
        // Bug 1: header, payload, checksum NO inicializados
        return pkt;
    }

    // Decodificar header
    (*pkt).header.magic = (data[0] as u16) << 8 | data[1] as u16;
    (*pkt).header.pkt_type = data[2];
    (*pkt).header.flags = data[3];
    (*pkt).header.length = (data[4] as u16) << 8 | data[5] as u16;

    if (*pkt).header.magic != PACKET_MAGIC {
        (*pkt).valid = false;
        let msg = b"bad magic";
        ptr::copy_nonoverlapping(
            msg.as_ptr(), (*pkt).error_msg.as_mut_ptr(), msg.len(),
        );
        (*pkt).error_msg[msg.len()] = 0;
        // Bug 1: payload, checksum NO inicializados
        return pkt;
    }

    let hdr_size = std::mem::size_of::<PacketHeader>();
    let payload_len = (*pkt).header.length as usize;
    let expected = hdr_size + payload_len + 4;

    if data.len() < expected {
        (*pkt).valid = false;
        let msg = b"truncated";
        ptr::copy_nonoverlapping(
            msg.as_ptr(), (*pkt).error_msg.as_mut_ptr(), msg.len(),
        );
        (*pkt).error_msg[msg.len()] = 0;
        return pkt;
    }

    // ────────────────────────────────────────────────────────────
    // Bug 2: allocar MAX_PAYLOAD pero solo copiar payload_len
    // ────────────────────────────────────────────────────────────
    let payload_layout = Layout::array::<u8>(MAX_PAYLOAD).unwrap();
    (*pkt).payload = alloc(payload_layout);
    ptr::copy_nonoverlapping(
        data[hdr_size..].as_ptr(),
        (*pkt).payload,
        payload_len,
    );
    // Bug 2: payload[payload_len..MAX_PAYLOAD] no inicializado

    // Checksum
    let ck_off = hdr_size + payload_len;
    (*pkt).checksum = (data[ck_off] as u32) << 24
        | (data[ck_off + 1] as u32) << 16
        | (data[ck_off + 2] as u32) << 8
        | data[ck_off + 3] as u32;

    (*pkt).valid = true;
    // Bug 3: error_msg no inicializado cuando valid=true
    pkt
}

// ────────────────────────────────────────────────────────────────
// Bug 3: accede a campos no inicializados
// ────────────────────────────────────────────────────────────────
unsafe fn packet_print(pkt: *const Packet) {
    if !(*pkt).valid {
        let msg_len = (*pkt).error_msg.iter()
            .position(|&b| b == 0)
            .unwrap_or(64);
        let msg = std::str::from_utf8_unchecked(&(*pkt).error_msg[..msg_len]);
        println!("Invalid: {}", msg);
        // Bug 1: accede a header.magic que puede no estar inicializado
        println!("  Magic was: 0x{:04X}", (*pkt).header.magic);
        return;
    }

    println!(
        "Packet type={} flags=0x{:02X} len={}",
        (*pkt).header.pkt_type, (*pkt).header.flags, (*pkt).header.length,
    );

    // Bug 2: imprime más allá de length
    print!("Payload: ");
    for i in 0..32.min(MAX_PAYLOAD) {
        print!("{:02X} ", *(*pkt).payload.add(i));
    }
    println!();
}

// ────────────────────────────────────────────────────────────────
// Bug 4: struct con padding
// ────────────────────────────────────────────────────────────────
#[repr(C)]
struct PacketStats {
    pkt_type: u8,      // 1 byte + 3 padding
    count: u32,        // 4 bytes
    valid_count: u16,  // 2 bytes + 2 padding
}

unsafe fn compute_stats(packets: &[*const Packet]) -> PacketStats {
    let layout = Layout::new::<PacketStats>();
    let stats_ptr = alloc(layout) as *mut PacketStats;
    // Bug 4: padding no inicializado

    (*stats_ptr).pkt_type = 0;
    (*stats_ptr).count = packets.len() as u32;
    (*stats_ptr).valid_count = 0;

    for &pkt in packets {
        if (*pkt).valid {
            (*stats_ptr).valid_count += 1;
        }
    }

    ptr::read(stats_ptr)
    // Bug 4: retorna struct con padding no inicializado
}

unsafe fn send_stats(stats: &PacketStats) {
    // Bug 4: escribe struct con padding no inicializado a stdout
    let bytes = std::slice::from_raw_parts(
        stats as *const PacketStats as *const u8,
        std::mem::size_of::<PacketStats>(),
    );
    // MSan detecta los bytes de padding no inicializados
    for b in bytes {
        print!("{:02X} ", b);
    }
    println!();
}

unsafe fn packet_free(pkt: *mut Packet) {
    if !(*pkt).payload.is_null() {
        let payload_layout = Layout::array::<u8>(MAX_PAYLOAD).unwrap();
        dealloc((*pkt).payload, payload_layout);
    }
    dealloc(pkt as *mut u8, Layout::new::<Packet>());
}

fn main() {
    let valid_data: &[u8] = &[
        0xCA, 0xFE,
        0x00, 0x01,
        0x00, 0x05,
        b'H', b'e', b'l', b'l', b'o',
        0x12, 0x34, 0x56, 0x78,
    ];

    let short_data: &[u8] = &[0xCA, 0xFE];

    let bad_magic: &[u8] = &[
        0xDE, 0xAD,
        0x00, 0x00, 0x00, 0x03,
        b'a', b'b', b'c',
        0x00, 0x00, 0x00, 0x00,
    ];

    unsafe {
        let p1 = packet_decode(valid_data);
        let p2 = packet_decode(short_data);
        let p3 = packet_decode(bad_magic);

        println!("=== Packet 1 (valid) ===");
        packet_print(p1);

        println!("\n=== Packet 2 (too short) ===");
        packet_print(p2);

        println!("\n=== Packet 3 (bad magic) ===");
        packet_print(p3);

        let all = [p1 as *const Packet, p2 as *const Packet, p3 as *const Packet];
        let stats = compute_stats(&all);
        println!("\n=== Stats ===");
        send_stats(&stats);

        packet_free(p1);
        packet_free(p2);
        packet_free(p3);
    }
}
```

### Resumen de bugs en packet_decoder

```
┌─────┬──────────────────────────────────────┬──────────────────────────────┐
│ Bug │ Descripción                           │ MSan reportará               │
├─────┼──────────────────────────────────────┼──────────────────────────────┤
│  1  │ Campos no inicializados en paths     │ use-of-uninitialized-value   │
│     │ de error (header, payload, checksum) │ al acceder header.magic en   │
│     │                                      │ packet_print para pkt inválido│
├─────┼──────────────────────────────────────┼──────────────────────────────┤
│  2  │ payload: aloca MAX_PAYLOAD pero      │ use-of-uninitialized-value   │
│     │ solo copia header.length bytes       │ al imprimir payload[i] para  │
│     │                                      │ i >= header.length           │
├─────┼──────────────────────────────────────┼──────────────────────────────┤
│  3  │ error_msg no inicializado cuando     │ (no se activa en este main   │
│     │ valid=true                           │ porque el ternario evita     │
│     │                                      │ leerlo, pero sería bug si    │
│     │                                      │ se accediera directamente)   │
├─────┼──────────────────────────────────────┼──────────────────────────────┤
│  4  │ PacketStats con padding no           │ use-of-uninitialized-value   │
│     │ inicializado, luego enviado          │ al escribir bytes del padding│
│     │ byte a byte                          │ en send_stats                │
└─────┴──────────────────────────────────────┴──────────────────────────────┘
```

### Compilar y ejecutar

```bash
# ── C ──
$ clang -fsanitize=memory -fsanitize-memory-track-origins=2 \
        -fno-omit-frame-pointer -g -O1 \
        packet_decoder.c -o packet_decoder
$ ./packet_decoder

# ── Rust ──
$ RUSTFLAGS="-Zsanitizer=memory -Zsanitizer-memory-track-origins=2" \
    cargo +nightly run -Zbuild-std \
    --target x86_64-unknown-linux-gnu
```

### Corrección de todos los bugs

```c
// C: corrección completa

Packet *packet_decode(const uint8_t *data, size_t len) {
    // FIX Bug 1: calloc en vez de malloc (inicializa todo a 0)
    Packet *pkt = (Packet *)calloc(1, sizeof(Packet));

    if (len < sizeof(PacketHeader)) {
        pkt->valid = 0;
        strncpy(pkt->error_msg, "too short", 63);
        return pkt;  // Ahora header/payload/checksum son 0 (inicializados)
    }

    // ... decodificar header igual ...

    // FIX Bug 2: usar calloc o inicializar todo el buffer
    pkt->payload = (uint8_t *)calloc(MAX_PAYLOAD, 1);
    memcpy(pkt->payload, data + sizeof(PacketHeader), pkt->header.length);

    // ... checksum igual ...

    pkt->valid = 1;
    // FIX Bug 3: inicializar error_msg
    memset(pkt->error_msg, 0, 64);
    return pkt;
}

// FIX Bug 4: memset la struct antes de llenar
PacketStats compute_stats(const Packet *packets[], int n) {
    PacketStats stats;
    memset(&stats, 0, sizeof(PacketStats));  // Incluye padding
    stats.count = (uint32_t)n;
    // ... resto igual ...
    return stats;
}
```

```rust
// Rust: corrección completa

unsafe fn packet_decode(data: &[u8]) -> *mut Packet {
    let layout = Layout::new::<Packet>();
    // FIX Bug 1: alloc_zeroed en vez de alloc
    let pkt = alloc_zeroed(layout) as *mut Packet;

    // ... paths de error ahora tienen campos inicializados a 0 ...

    // FIX Bug 2: alloc_zeroed para payload
    let payload_layout = Layout::array::<u8>(MAX_PAYLOAD).unwrap();
    (*pkt).payload = alloc_zeroed(payload_layout);
    // ... copiar datos igual ...

    (*pkt).valid = true;
    // FIX Bug 3: error_msg ya inicializado por alloc_zeroed
    pkt
}

// FIX Bug 4: usar alloc_zeroed para stats
unsafe fn compute_stats(packets: &[*const Packet]) -> PacketStats {
    let layout = Layout::new::<PacketStats>();
    let stats_ptr = alloc_zeroed(layout) as *mut PacketStats;
    // Padding inicializado a 0
    // ... resto igual ...
    ptr::read(stats_ptr)
}
```

---

## 36. Ejercicios

### Ejercicio 1: interpretar reportes MSan

**Objetivo**: Aprender a leer reportes de MSan con origin tracking.

**Tareas**:

**a)** Dado este reporte, identifica:
   - ¿Qué valor no inicializado se usó?
   - ¿Dónde se creó la memoria?
   - ¿Cuántos niveles de propagación hay?

```
==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x4c3f20 in validate_config src/config.c:89:9
    #1 0x4c4100 in startup src/main.c:34:15

  Uninitialized value was stored to memory at
    #0 0x4c3e80 in merge_configs src/config.c:67:5
    #1 0x4c4080 in load_config src/main.c:28:12

  Uninitialized value was stored to memory at
    #0 0x4c3d00 in parse_defaults src/defaults.c:45:20
    #1 0x4c3e00 in init_defaults src/defaults.c:78:5

  Uninitialized value was created by a heap allocation
    #0 0x... in malloc
    #1 0x4c3c80 in new_config src/defaults.c:30:25
```

**b)** Dado este reporte de Rust, ¿qué tipo de creación es?

```
==54321==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x... in packet_decoder::process src/lib.rs:142:12

  Uninitialized value was created in the stack frame of function
  'packet_decoder::decode_header'
    #0 0x... in packet_decoder::decode_header src/lib.rs:98:1
```

**c)** ¿Por qué no hay sección "stored to memory" en el reporte (b)?

---

### Ejercicio 2: encontrar bugs con MSan

**Objetivo**: Usar MSan para encontrar todos los bugs de inicialización.

**Tareas**:

**a)** Compila y ejecuta el siguiente programa con MSan:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int width;
    int height;
    int depth;
    int channels;
} ImageInfo;

ImageInfo *parse_image_header(const unsigned char *data, size_t len) {
    ImageInfo *info = (ImageInfo *)malloc(sizeof(ImageInfo));

    if (len >= 4) {
        info->width = (data[0] << 8) | data[1];
        info->height = (data[2] << 8) | data[3];
    }

    if (len >= 5) {
        info->depth = data[4];
    }
    /* channels no se inicializa nunca */

    return info;
}

void print_image_info(const ImageInfo *info) {
    printf("Image: %dx%d, depth=%d, channels=%d\n",
           info->width, info->height, info->depth, info->channels);
    int total_pixels = info->width * info->height * info->channels;
    printf("Total subpixels: %d\n", total_pixels);
}

int main() {
    unsigned char header1[] = {0x01, 0x00, 0x00, 0xC8, 0x08};
    unsigned char header2[] = {0x00, 0x40};

    ImageInfo *img1 = parse_image_header(header1, 5);
    print_image_info(img1);

    ImageInfo *img2 = parse_image_header(header2, 2);
    print_image_info(img2);

    free(img1);
    free(img2);
    return 0;
}
```

**b)** ¿Cuántos bugs de inicialización hay? Lista cada uno.

**c)** ¿Cuáles activa `header1` y cuáles `header2`?

**d)** Corrige todos los bugs. Verifica con MSan.

---

### Ejercicio 3: MSan en Rust unsafe

**Objetivo**: Detectar bugs de inicialización en código Rust unsafe.

**Tareas**:

**a)** Implementa un `RingBuffer<T: Copy>` en Rust unsafe:
   ```rust
   pub struct RingBuffer<T: Copy> {
       buf: *mut T,
       capacity: usize,
       head: usize,   // Próxima posición para escribir
       tail: usize,   // Próxima posición para leer
       count: usize,
   }
   ```
   Con operaciones: `new(cap)`, `push(val)`, `pop() -> Option<T>`, `peek() -> Option<T>`, `len()`.

**b)** Introduce 2 bugs de inicialización:
   - `new` aloca con `alloc` (no `alloc_zeroed`), por lo que el buffer tiene basura
   - `pop` retorna `ptr::read(buf.add(tail))` sin verificar que esa posición fue escrita (podría haber sido pre-existente basura si el buffer se llenó y se vació parcialmente, y luego se accede a una posición que nunca recibió un `push`)

**c)** Escribe tests que activen cada bug.

**d)** Ejecuta con MSan:
   ```bash
   RUSTFLAGS="-Zsanitizer=memory -Zsanitizer-memory-track-origins=2" \
       cargo +nightly test -Zbuild-std \
       --target x86_64-unknown-linux-gnu
   ```

**e)** Corrige los bugs. ¿`alloc_zeroed` es suficiente para tipo `T` genérico?

---

### Ejercicio 4: MSan en C y Rust — comparación

**Objetivo**: Experimentar las diferencias de MSan entre C y Rust.

**Tareas**:

**a)** Implementa una lookup table en ambos lenguajes:
   ```
   Estructura: array de N slots, cada slot tiene {key: int, value: int, occupied: bool}
   Operaciones: create(N), insert(key, value), lookup(key) -> value, destroy
   ```

**b)** Introduce el MISMO bug en ambos: `create` usa malloc/alloc sin inicializar el campo `occupied`, por lo que `lookup` puede encontrar "fantasmas".

**c)** Compila ambos con MSan y ejecuta una secuencia:
   ```
   create(100)
   insert(42, 100)
   lookup(42)  // Debería retornar 100
   lookup(99)  // Bug: puede retornar basura si occupied[hash(99)] es "true" por casualidad
   ```

**d)** Compara:
   - ¿MSan reporta en ambos?
   - ¿Los reportes son igualmente claros?
   - ¿Origin tracking funciona igual en C y Rust?
   - ¿Cuál requirió más esfuerzo de setup? (Rust: -Zbuild-std)

**e)** Ahora reescribe la versión Rust sin `unsafe`. ¿Cuántas líneas de unsafe eliminaste? ¿El bug aún es posible?

---

## Navegación

- **Anterior**: [T01 - AddressSanitizer (ASan)](../T01_AddressSanitizer/README.md)
- **Siguiente**: [T03 - UndefinedBehaviorSanitizer (UBSan)](../T03_UndefinedBehaviorSanitizer/README.md)

---

> **Sección 3: Sanitizers como Red de Seguridad** — Tópico 2 de 4 completado