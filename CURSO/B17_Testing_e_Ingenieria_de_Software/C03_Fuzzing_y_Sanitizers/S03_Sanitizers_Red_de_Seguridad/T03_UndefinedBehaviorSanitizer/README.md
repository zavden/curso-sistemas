# T03 - UndefinedBehaviorSanitizer (UBSan): signed overflow, null deref, alignment — en C

## Índice

1. [Qué es UndefinedBehaviorSanitizer](#1-qué-es-undefinedbehaviorsanitizer)
2. [Historia y contexto](#2-historia-y-contexto)
3. [¿Qué es Undefined Behavior?](#3-qué-es-undefined-behavior)
4. [El catálogo completo de UB en C](#4-el-catálogo-completo-de-ub-en-c)
5. [Arquitectura de UBSan](#5-arquitectura-de-ubsan)
6. [Compilar con UBSan en C](#6-compilar-con-ubsan-en-c)
7. [Grupos de checks en UBSan](#7-grupos-de-checks-en-ubsan)
8. [Checks individuales: lista completa](#8-checks-individuales-lista-completa)
9. [Bug 1: signed integer overflow](#9-bug-1-signed-integer-overflow)
10. [Bug 2: unsigned integer overflow](#10-bug-2-unsigned-integer-overflow)
11. [Bug 3: null pointer dereference](#11-bug-3-null-pointer-dereference)
12. [Bug 4: misaligned pointer access](#12-bug-4-misaligned-pointer-access)
13. [Bug 5: shift exponent out of range](#13-bug-5-shift-exponent-out-of-range)
14. [Bug 6: out-of-bounds array index](#14-bug-6-out-of-bounds-array-index)
15. [Bug 7: signed integer division overflow](#15-bug-7-signed-integer-division-overflow)
16. [Bug 8: float cast overflow](#16-bug-8-float-cast-overflow)
17. [Bug 9: invalid bool/enum value](#17-bug-9-invalid-boolenum-value)
18. [Bug 10: unreachable code reached](#18-bug-10-unreachable-code-reached)
19. [Bug 11: pointer overflow](#19-bug-11-pointer-overflow)
20. [Bug 12: implicit conversion (integer truncation)](#20-bug-12-implicit-conversion-integer-truncation)
21. [Bug 13: function type mismatch](#21-bug-13-function-type-mismatch)
22. [Bug 14: invalid object size](#22-bug-14-invalid-object-size)
23. [Anatomía completa de un reporte UBSan](#23-anatomía-completa-de-un-reporte-ubsan)
24. [Modos de reporte: recover vs trap](#24-modos-de-reporte-recover-vs-trap)
25. [UBSan en Rust: ¿es necesario?](#25-ubsan-en-rust-es-necesario)
26. [Ejemplo completo: math_utils](#26-ejemplo-completo-math_utils)
27. [Ejemplo completo: pixel_buffer](#27-ejemplo-completo-pixel_buffer)
28. [UBSan con tests unitarios](#28-ubsan-con-tests-unitarios)
29. [UBSan con fuzzing](#29-ubsan-con-fuzzing)
30. [UBSan combinado con otros sanitizers](#30-ubsan-combinado-con-otros-sanitizers)
31. [UBSAN_OPTIONS: configuración completa](#31-ubsan_options-configuración-completa)
32. [Impacto en rendimiento](#32-impacto-en-rendimiento)
33. [Falsos positivos en UBSan](#33-falsos-positivos-en-ubsan)
34. [Limitaciones de UBSan](#34-limitaciones-de-ubsan)
35. [Errores comunes](#35-errores-comunes)
36. [Programa de práctica: safe_calc](#36-programa-de-práctica-safe_calc)
37. [Ejercicios](#37-ejercicios)

---

## 1. Qué es UndefinedBehaviorSanitizer

UndefinedBehaviorSanitizer (UBSan) detecta **undefined behavior** en C y C++ en tiempo de ejecución. A diferencia de ASan (que busca errores de acceso a memoria) y MSan (que busca lecturas de memoria no inicializada), UBSan busca operaciones que el estándar C declara como comportamiento indefinido.

```
┌──────────────────────────────────────────────────────────────────┐
│               UndefinedBehaviorSanitizer (UBSan)                  │
│                                                                  │
│  Detecta: operaciones con comportamiento indefinido              │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐     │
│  │  Signed overflow    │  Null dereference  │  Bad shift    │     │
│  │  Division by zero   │  Misalignment      │  Float cast   │     │
│  │  OOB array index    │  Invalid bool      │  Bad enum     │     │
│  │  Unreachable        │  Type mismatch     │  VLA bound    │     │
│  │  Pointer overflow   │  Missing return    │  Nonnull arg  │     │
│  └─────────────────────────────────────────────────────────┘     │
│                                                                  │
│  Características únicas de UBSan:                                │
│  • MUY bajo overhead (~1.2x CPU, ~1x RAM)                       │
│  • Compatible con TODOS los otros sanitizers                     │
│  • Funciona en Clang Y GCC                                       │
│  • Puede continuar después de un error (modo recovery)           │
│  • No usa shadow memory                                          │
│  • El sanitizer más barato de activar                            │
└──────────────────────────────────────────────────────────────────┘
```

### ¿Por qué es especial UBSan?

UBSan es el **único** sanitizer que:
1. Es compatible con **todos** los demás sanitizers (ASan + UBSan, MSan + UBSan, TSan + UBSan)
2. Tiene overhead mínimo (se puede dejar activado en builds de testing permanentemente)
3. Detecta bugs que **ningún otro** sanitizer detecta (overflow aritmético, shifts inválidos)
4. Puede funcionar en modo "trap" sin necesidad de runtime library

---

## 2. Historia y contexto

```
Línea temporal de UBSan:

2009    Clang/LLVM comienza trabajo en sanitizers
        UBSan nace como complemento de ASan

2012    -fsanitize=undefined aparece en Clang 3.3
        Checks iniciales: signed overflow, divide-by-zero, shift

2013    GCC 4.9 añade -fsanitize=undefined
        Compatibilidad entre compiladores (raro entre sanitizers)

2014    Se añaden checks de alignment, bool, enum, vla-bound
        UBSan madura como suite de checks composables

2015    -fsanitize=integer (Clang): checks de unsigned overflow
        No es UB técnico, pero sí bug lógico frecuente

2016    -fsanitize-trap=undefined: modo sin runtime library
        Permite usar UBSan en embedded/kernel

2018    -fsanitize=implicit-conversion (Clang)
        Detecta truncación implícita int → char

2020    Linux kernel adopta UBSan para detección en kernel
        CONFIG_UBSAN=y en producción (overhead aceptable)

2023    UBSan es el sanitizer más ampliamente adoptado
        Usado en Chrome, Firefox, Android, Linux kernel
        Único sanitizer viable en producción (con -fsanitize-trap)
```

### UBSan en el ecosistema de sanitizers

```
┌────────────────────────────────────────────────────────────────┐
│          Jerarquía de adopción de sanitizers                    │
│                                                                │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                      UBSan                                │  │
│  │  El más ligero, compatible con todo, viable en producción │  │
│  │  → SIEMPRE activar como mínimo                           │  │
│  │                                                          │  │
│  │  ┌──────────────────────────────────────────────────┐    │  │
│  │  │                    ASan                           │    │  │
│  │  │  Memory bugs, overhead moderado                   │    │  │
│  │  │  → Siempre en testing/CI                         │    │  │
│  │  │                                                  │    │  │
│  │  │  ┌──────────────────────────────────────────┐    │    │  │
│  │  │  │        MSan / TSan                        │    │    │  │
│  │  │  │  Específicos, overhead alto               │    │    │  │
│  │  │  │  → Cuando se necesite específicamente    │    │    │  │
│  │  │  └──────────────────────────────────────────┘    │    │  │
│  │  └──────────────────────────────────────────────────┘    │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
```

---

## 3. ¿Qué es Undefined Behavior?

Undefined Behavior (UB) es un concepto central del estándar C. Cuando un programa ejecuta una operación cuyo comportamiento el estándar no define, el compilador y el hardware pueden hacer **cualquier cosa**.

### La definición formal

El estándar C (C11, §3.4.3) define:
> **undefined behavior**: behavior, upon use of a nonportable or erroneous program construct or of erroneous data, for which this International Standard imposes **no requirements**

### ¿Qué significa "no requirements"?

```
┌──────────────────────────────────────────────────────────────────┐
│          ¿Qué puede pasar con Undefined Behavior?                │
│                                                                  │
│  int x = INT_MAX;                                                │
│  x = x + 1;  // UB: signed overflow                             │
│                                                                  │
│  Posibles resultados:                                            │
│                                                                  │
│  1. "Funciona": x es INT_MIN (wraparound en complemento a 2)    │
│     → Parece correcto, pero NO está garantizado                  │
│                                                                  │
│  2. El compilador elimina código:                                │
│     if (x + 1 > x) { ... }                                      │
│     → Compilador ASUME que signed overflow no ocurre             │
│     → Simplifica a: if (true) { ... }                            │
│     → El código se ejecuta SIEMPRE, incluso cuando no debería   │
│                                                                  │
│  3. El compilador optimiza el programa de formas inesperadas:    │
│     void check(int *p) {                                         │
│       int v = *p;        // Accede a *p                          │
│       if (p == NULL) {   // Check de NULL                        │
│         error();         // ← ¡Nunca se ejecuta!                │
│       }                                                          │
│     }                                                            │
│     → Compilador razona: "se accedió a *p, luego p no es NULL"  │
│     → Elimina el check de NULL                                   │
│                                                                  │
│  4. El programa crashea (segfault, SIGFPE, etc.)                 │
│                                                                  │
│  5. Se corrompen datos silenciosamente                            │
│                                                                  │
│  Todos estos son resultados VÁLIDOS de UB.                       │
│  El programa está roto, el compilador no tiene obligación de     │
│  hacer nada razonable.                                           │
└──────────────────────────────────────────────────────────────────┘
```

### ¿Por qué existe UB?

```
┌──────────────────────────────────────────────────────────────────┐
│              Razones para que exista UB en C                     │
│                                                                  │
│  1. PORTABILIDAD                                                 │
│     Diferentes CPUs manejan overflow de formas distintas:        │
│     x86: wraparound (complemento a 2)                            │
│     Algunas DSP: saturación (clamp al máximo)                    │
│     Históricamente: complemento a 1, signo-magnitud             │
│     → UB permite que C funcione en TODAS estas arquitecturas    │
│                                                                  │
│  2. OPTIMIZACIÓN                                                 │
│     El compilador puede ASUMIR que UB no ocurre:                │
│     → Si signed overflow es UB, entonces a+1 > a SIEMPRE       │
│     → Esto permite optimizaciones de loops, vectorización        │
│     → Programas C son rápidos PORQUE el compilador explota UB   │
│                                                                  │
│  3. DETECCIÓN DE HARDWARE                                        │
│     Algunas CPUs generan traps en ciertas operaciones:           │
│     → Division by zero genera SIGFPE en x86                     │
│     → Misaligned access genera SIGBUS en ARM (no siempre)       │
│     → UB permite que el hardware reaccione naturalmente         │
│                                                                  │
│  4. EFICIENCIA                                                   │
│     Verificar cada operación sería muy costoso:                  │
│     → Verificar overflow en cada suma: ~3 instrucciones extra   │
│     → Verificar NULL en cada deref: ~2 instrucciones extra      │
│     → UB permite "confiar" en el programador                    │
└──────────────────────────────────────────────────────────────────┘
```

### UB vs Implementation-Defined vs Unspecified

```
┌─────────────────────┬────────────────────────────────────────────┐
│ Categoría           │ Significado                                │
├─────────────────────┼────────────────────────────────────────────┤
│ Defined behavior    │ El estándar dice exactamente qué pasa.    │
│                     │ Ejemplo: 2 + 3 == 5                       │
├─────────────────────┼────────────────────────────────────────────┤
│ Implementation-     │ El estándar da opciones, el compilador    │
│ defined             │ elige una y DOCUMENTA cuál.               │
│                     │ Ejemplo: sizeof(int) (4 en x86_64)       │
│                     │ Ejemplo: right shift de negativo           │
├─────────────────────┼────────────────────────────────────────────┤
│ Unspecified         │ El estándar da opciones, el compilador    │
│                     │ elige una pero NO tiene que documentar.   │
│                     │ Ejemplo: orden de evaluación de args      │
│                     │ f(a(), b()) → ¿se evalúa a() o b() primero? │
├─────────────────────┼────────────────────────────────────────────┤
│ Undefined           │ El estándar NO dice qué pasa.             │
│ behavior (UB)       │ Todo es posible. Programa está roto.      │
│                     │ Ejemplo: signed overflow, NULL deref       │
└─────────────────────┴────────────────────────────────────────────┘
```

---

## 4. El catálogo completo de UB en C

El estándar C define más de 200 formas de undefined behavior. UBSan detecta un subconjunto importante de ellas. Aquí están las categorías principales:

```
┌──────────────────────────────────────────────────────────────────────┐
│          UB en C: lo que UBSan detecta vs lo que no                  │
│                                                                      │
│  UBSan DETECTA:                                 UBSan NO detecta:   │
│  ──────────────                                 ────────────────     │
│  ✓ Signed integer overflow                      ✗ Data races        │
│  ✓ Division by zero / INT_MIN / -1              ✗ Use after free    │
│  ✓ Shift por cantidad inválida                  ✗ Buffer overflow   │
│  ✓ Null pointer dereference                     ✗ Stack overflow    │
│  ✓ Misaligned pointer access                    ✗ Uninitialized mem │
│  ✓ Out-of-bounds array index                    ✗ Double free       │
│  ✓ Float-to-int overflow                        ✗ Format string     │
│  ✓ Invalid bool/enum load                       ✗ Strict aliasing   │
│  ✓ Unreachable code reached                     ✗ Sequence points   │
│  ✓ Missing return value                         ✗ Dangling pointer  │
│  ✓ VLA with non-positive size                   ✗ Object lifetime   │
│  ✓ Pointer overflow (arithmetic)                ✗ Restrict viol.    │
│  ✓ Function type mismatch                       ✗ Volatile access   │
│  ✓ Nonnull attribute violation                  ✗ Infinite loop     │
│  ✓ Integer truncation (Clang)                     (sin side effects)│
│  ✓ Unsigned overflow (Clang, no es UB)                              │
│                                                                      │
│  Para lo que UBSan no detecta, se necesita:                         │
│  ASan (memoria), TSan (data races), MSan (uninit)                   │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 5. Arquitectura de UBSan

UBSan tiene una arquitectura mucho más simple que ASan o MSan. No usa shadow memory, ni interceptores de malloc, ni reescribe el layout de la stack. Solo inserta checks puntuales.

```
┌──────────────────────────────────────────────────────────────────────┐
│                     Arquitectura de UBSan                             │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐     │
│  │  Código fuente (.c)                                         │     │
│  │                                                             │     │
│  │  int result = a + b;                                        │     │
│  └─────────────────────────────────────────────────────────────┘     │
│                          │                                           │
│                          ▼ Compilación con -fsanitize=undefined      │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐     │
│  │  Código instrumentado                                       │     │
│  │                                                             │     │
│  │  // Check insertado ANTES de la operación:                  │     │
│  │  if (__builtin_sadd_overflow(a, b, &result)) {              │     │
│  │      // Overflow detected                                   │     │
│  │      __ubsan_handle_add_overflow(                            │     │
│  │          &source_location,  // archivo:línea:columna        │     │
│  │          a, b               // los operandos                │     │
│  │      );                                                     │     │
│  │  }                                                          │     │
│  │  result = a + b;  // Operación original (sigue ejecutando)  │     │
│  └─────────────────────────────────────────────────────────────┘     │
│                          │                                           │
│                          ▼                                           │
│  ┌─────────────────────────────────────────────────────────────┐     │
│  │  Runtime library (libubsan)                                 │     │
│  │                                                             │     │
│  │  __ubsan_handle_add_overflow():                             │     │
│  │    1. Formatea mensaje de error legible                     │     │
│  │    2. Imprime source location (archivo:línea:columna)       │     │
│  │    3. Imprime los valores de los operandos                  │     │
│  │    4. Según modo: continúa ejecución O aborta               │     │
│  └─────────────────────────────────────────────────────────────┘     │
│                                                                      │
│  Alternativa: modo trap (-fsanitize-trap=undefined)                  │
│  ┌─────────────────────────────────────────────────────────────┐     │
│  │  En vez de llamar a __ubsan_handle_*, inserta:              │     │
│  │  __builtin_trap();  // = instrucción ud2 en x86             │     │
│  │  → No necesita runtime library                              │     │
│  │  → No imprime mensaje (solo SIGILL)                         │     │
│  │  → Overhead MÍNIMO: ~2 instrucciones por check              │     │
│  └─────────────────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────────────────┘
```

### Comparación con ASan/MSan

```
┌───────────────────┬──────────────────┬──────────────────┬──────────────────┐
│                   │ UBSan            │ ASan             │ MSan             │
├───────────────────┼──────────────────┼──────────────────┼──────────────────┤
│ Shadow memory     │ NO               │ SÍ (8:1)         │ SÍ (1:1)         │
│ Runtime library   │ Pequeña (o ninguna)│ Grande          │ Grande           │
│ malloc reemplazo  │ NO               │ SÍ               │ SÍ               │
│ Stack reescritura │ NO               │ SÍ (redzones)    │ NO               │
│ Instrumentación   │ Solo en operaciones│ En cada load/   │ En cada load/   │
│                   │ potencialmente UB│ store            │ store            │
│ CPU overhead      │ ~1.2x            │ ~2x              │ ~3-5x            │
│ RAM overhead      │ ~1x              │ ~2-3x            │ ~2-3x            │
│ Necesita Clang    │ NO (Clang o GCC) │ NO (Clang o GCC) │ SÍ (solo Clang)  │
│ Funciona sin -g   │ SÍ (menos info)  │ SÍ (menos info)  │ SÍ (menos info)  │
└───────────────────┴──────────────────┴──────────────────┴──────────────────┘
```

---

## 6. Compilar con UBSan en C

### Activar UBSan completo

```bash
# Activar TODOS los checks de undefined behavior:
clang -fsanitize=undefined -g -O1 program.c -o program

# O con GCC:
gcc -fsanitize=undefined -g -O1 program.c -o program

# Desglose:
# -fsanitize=undefined  → Grupo "undefined": activa ~15 checks
# -g                    → Símbolos de debug (mejor reporte)
# -O1                   → Optimización leve (recomendado)
```

### Activar checks específicos

```bash
# Solo signed overflow:
clang -fsanitize=signed-integer-overflow -g program.c

# Solo null dereference:
clang -fsanitize=null -g program.c

# Combinaciones:
clang -fsanitize=signed-integer-overflow,null,shift -g program.c

# Grupo "undefined" + unsigned overflow (Clang):
clang -fsanitize=undefined,unsigned-integer-overflow -g program.c

# Todo lo posible (Clang):
clang -fsanitize=undefined,integer,nullability,float-divide-by-zero -g program.c
```

### Desactivar checks específicos dentro de un grupo

```bash
# Todos los de "undefined" EXCEPTO alignment:
clang -fsanitize=undefined -fno-sanitize=alignment -g program.c

# Todos EXCEPTO vptr (útil si no usas RTTI en C++):
clang -fsanitize=undefined -fno-sanitize=vptr -g program.c
```

### Flags recomendados para desarrollo

```bash
# Configuración recomendada para desarrollo:
clang -fsanitize=undefined,integer \
      -fno-sanitize-recover=all \
      -fno-omit-frame-pointer \
      -g \
      -O1 \
      program.c -o program

# -fsanitize=undefined,integer     → Todos los checks + unsigned overflow
# -fno-sanitize-recover=all        → Abortar en primer error (no continuar)
# -fno-omit-frame-pointer          → Stack traces legibles
# -g                               → Símbolos de debug
# -O1                              → Optimización leve
```

### Con Makefile

```makefile
CC = clang
CFLAGS = -fsanitize=undefined,integer \
         -fno-sanitize-recover=all \
         -fno-omit-frame-pointer -g -O1
LDFLAGS = -fsanitize=undefined,integer

all: program

program: main.o utils.o
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o program
```

---

## 7. Grupos de checks en UBSan

UBSan organiza sus checks en **grupos** que se activan con un solo flag:

```
┌──────────────────────────────────────────────────────────────────────┐
│                   Grupos de UBSan (Clang)                            │
│                                                                      │
│  -fsanitize=undefined (el grupo principal)                           │
│  ├── alignment                                                       │
│  ├── bool                                                            │
│  ├── builtin                                                         │
│  ├── enum                                                            │
│  ├── float-cast-overflow                                             │
│  ├── float-divide-by-zero (*)                                        │
│  ├── function                                                        │
│  ├── implicit-unsigned-integer-truncation (**)                       │
│  ├── implicit-signed-integer-truncation (**)                         │
│  ├── integer-divide-by-zero                                          │
│  ├── nonnull-attribute                                               │
│  ├── null                                                            │
│  ├── object-size                                                     │
│  ├── pointer-overflow                                                │
│  ├── return (C++ only: non-void function sin return)                 │
│  ├── returns-nonnull-attribute                                       │
│  ├── shift                                                           │
│  │   ├── shift-base                                                  │
│  │   └── shift-exponent                                              │
│  ├── signed-integer-overflow                                         │
│  ├── unreachable                                                     │
│  ├── vla-bound                                                       │
│  └── vptr (C++ only: virtual call en objeto destruido)               │
│                                                                      │
│  (*) float-divide-by-zero: NO está en undefined por defecto en       │
│      Clang (es implementation-defined en IEEE 754, no UB técnico)    │
│                                                                      │
│  (**) implicit truncation: solo Clang, incluido en -fsanitize=       │
│       implicit-conversion                                            │
│                                                                      │
│  -fsanitize=integer (grupo adicional de Clang)                       │
│  ├── signed-integer-overflow    (ya en undefined)                    │
│  ├── unsigned-integer-overflow  (NO es UB, pero sí bug frecuente)   │
│  ├── implicit-integer-truncation                                     │
│  ├── implicit-integer-sign-change                                    │
│  └── integer-divide-by-zero    (ya en undefined)                    │
│                                                                      │
│  -fsanitize=nullability (grupo adicional de Clang)                   │
│  ├── nullability-arg                                                 │
│  ├── nullability-assign                                              │
│  └── nullability-return                                              │
└──────────────────────────────────────────────────────────────────────┘
```

### Diferencias entre Clang y GCC

```
┌────────────────────────────────┬────────────┬────────────┐
│ Check                          │ Clang      │ GCC        │
├────────────────────────────────┼────────────┼────────────┤
│ -fsanitize=undefined           │ ✓          │ ✓          │
│ -fsanitize=integer             │ ✓          │ ✗          │
│ -fsanitize=nullability         │ ✓          │ ✗          │
│ -fsanitize=implicit-conversion │ ✓          │ ✗          │
│ unsigned-integer-overflow      │ ✓          │ ✗          │
│ implicit-integer-truncation    │ ✓          │ ✗          │
│ implicit-integer-sign-change   │ ✓          │ ✗          │
│ -fsanitize-trap=undefined      │ ✓          │ ✗ (parcial)│
│ float-divide-by-zero en undef  │ ✗ (*)      │ ✓          │
│ Minimal runtime                │ ✓          │ ✗          │
└────────────────────────────────┴────────────┴────────────┘

(*) Clang no incluye float-divide-by-zero en el grupo undefined
    porque IEEE 754 lo define (resultado = ±Inf o NaN).
    GCC sí lo incluye.
```

---

## 8. Checks individuales: lista completa

```
┌─────────────────────────────────┬───────────────────────────────────────────┐
│ Check                           │ Qué detecta                               │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ alignment                       │ Puntero desalineado para su tipo         │
│                                 │ (int* apunta a dirección no múltiplo de 4)│
├─────────────────────────────────┼───────────────────────────────────────────┤
│ bool                            │ Cargar un bool que no es 0 ni 1          │
│                                 │ (valor 2, 255, etc.)                     │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ builtin                         │ Pasar 0 a __builtin_ctz/__builtin_clz    │
│                                 │ (count trailing/leading zeros de 0 = UB) │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ bounds                          │ Acceso fuera de límites en array con     │
│                                 │ tamaño conocido en compilación           │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ enum                            │ Cargar un enum con valor fuera de rango  │
│                                 │ (solo C++, no aplica en C)               │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ float-cast-overflow             │ Convertir float → int cuando el float    │
│                                 │ está fuera del rango del int             │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ float-divide-by-zero            │ División de float por 0.0                │
│                                 │ (IEEE 754 lo define, pero puede ser bug) │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ function                        │ Llamar función a través de puntero con   │
│                                 │ tipo incompatible                        │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ implicit-unsigned-integer-      │ Asignar int grande a char/short,         │
│ truncation                      │ perdiendo bits significativos            │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ implicit-signed-integer-        │ Asignar int negativo a unsigned,         │
│ truncation                      │ cambiando el valor                       │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ implicit-integer-sign-change    │ Conversión que cambia signo del valor    │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ integer-divide-by-zero          │ División entera por cero                 │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ nonnull-attribute               │ Pasar NULL a parámetro marcado nonnull   │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ null                            │ Dereferenciar puntero NULL               │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ object-size                     │ Acceso a memoria con tamaño conocido     │
│                                 │ incorrecto (requiere -O1+)              │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ pointer-overflow                │ Aritmética de punteros que causa overflow│
│                                 │ o wrap-around                            │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ return                          │ Función non-void termina sin return      │
│                                 │ (solo C++)                               │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ returns-nonnull-attribute       │ Función con returns_nonnull retorna NULL │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ shift                           │ Shift por cantidad negativa o >= ancho   │
│                                 │ del tipo, o shift de valor negativo      │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ signed-integer-overflow         │ Resultado de operación signed que no     │
│                                 │ cabe en el tipo (a + b, a * b, etc.)    │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ unreachable                     │ Ejecución alcanza __builtin_unreachable()│
├─────────────────────────────────┼───────────────────────────────────────────┤
│ unsigned-integer-overflow       │ Overflow de unsigned (NO es UB, pero     │
│                                 │ frecuentemente es un bug; solo Clang)    │
├─────────────────────────────────┼───────────────────────────────────────────┤
│ vla-bound                       │ VLA con tamaño ≤ 0                       │
└─────────────────────────────────┴───────────────────────────────────────────┘
```

---

## 9. Bug 1: signed integer overflow

El tipo de UB más común en C. Los enteros con signo no pueden hacer wrap-around; si el resultado de una operación excede el rango, es UB.

### Reglas del estándar

```
┌─────────────────────────────────────────────────────────────────┐
│         Signed vs Unsigned overflow en C                         │
│                                                                  │
│  Signed (int, long, short, char):                                │
│    Overflow = UNDEFINED BEHAVIOR                                 │
│    int x = INT_MAX; x + 1; // UB                                │
│    El compilador ASUME que nunca ocurre                          │
│                                                                  │
│  Unsigned (unsigned int, size_t, etc.):                           │
│    Overflow = DEFINED (wrap-around módulo 2^n)                   │
│    unsigned x = UINT_MAX; x + 1; // == 0 (definido)             │
│    El compilador no puede asumir nada                            │
│                                                                  │
│  Esta distinción es CRUCIAL:                                     │
│  - El compilador optimiza MÁS agresivamente con signed          │
│  - Un loop con signed counter puede ser eliminado si             │
│    el compilador deduce que "nunca termina" (via overflow)       │
└─────────────────────────────────────────────────────────────────┘
```

### Ejemplo: overflow en suma

```c
// signed_overflow.c
#include <stdio.h>
#include <limits.h>

int safe_add(int a, int b) {
    // Bug: no verifica overflow
    return a + b;  // UB si a + b > INT_MAX o < INT_MIN
}

int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;  // Bug: overflow para n >= 13 (32-bit int)
    }
    return result;
}

int main() {
    // Overflow en suma
    int a = INT_MAX;
    int b = 1;
    int sum = safe_add(a, b);  // UB: INT_MAX + 1
    printf("INT_MAX + 1 = %d\n", sum);

    // Overflow en factorial
    int f = factorial(20);  // 20! no cabe en int
    printf("20! = %d\n", f);

    // Overflow en negación
    int neg = -INT_MIN;  // UB: |INT_MIN| > INT_MAX en complemento a 2
    printf("-INT_MIN = %d\n", neg);

    // Overflow en multiplicación
    int m = 100000 * 100000;  // UB: 10^10 > INT_MAX
    printf("100000 * 100000 = %d\n", m);

    return 0;
}
```

```bash
$ clang -fsanitize=signed-integer-overflow -g -O1 signed_overflow.c -o test
$ ./test

signed_overflow.c:6:14: runtime error: signed integer overflow:
  2147483647 + 1 cannot be represented in type 'int'

signed_overflow.c:12:16: runtime error: signed integer overflow:
  1932053504 * 14 cannot be represented in type 'int'

signed_overflow.c:21:15: runtime error: negation of -2147483648 cannot be
  represented in type 'int'; cast to an unsigned type to negate this value

signed_overflow.c:24:21: runtime error: signed integer overflow:
  100000 * 100000 cannot be represented in type 'int'
```

### Cómo corregir

```c
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

// Opción 1: verificar ANTES de operar
bool safe_add_checked(int a, int b, int *result) {
    if ((b > 0 && a > INT_MAX - b) ||
        (b < 0 && a < INT_MIN - b)) {
        return false;  // Overflow
    }
    *result = a + b;
    return true;
}

// Opción 2: usar __builtin_add_overflow (GCC/Clang)
bool safe_add_builtin(int a, int b, int *result) {
    return !__builtin_add_overflow(a, b, result);
}

// Opción 3: usar tipos más grandes
long long safe_multiply(int a, int b) {
    return (long long)a * (long long)b;  // No overflow en 64 bits
}

// Opción 4: usar unsigned para wrap-around intencional
unsigned int hash_combine(unsigned int h1, unsigned int h2) {
    return h1 * 31 + h2;  // Overflow unsigned está definido
}
```

### Consecuencias reales del signed overflow

```c
// Ejemplo de optimización peligrosa por overflow:

// Lo que el programador escribió:
void process(int *array, int length) {
    for (int i = 0; i < length; i++) {
        if (i + 1 < i) {  // "¿Hubo overflow?"
            break;         // Protección contra overflow
        }
        array[i] = compute(i);
    }
}

// Lo que el compilador genera (con -O2):
void process(int *array, int length) {
    for (int i = 0; i < length; i++) {
        // El compilador ELIMINÓ el check: "i + 1 < i"
        // Razonamiento: signed overflow es UB, entonces
        // i + 1 SIEMPRE > i (porque si no, sería UB y
        // podemos asumir que no pasa).
        // Resultado: la "protección" se elimina por completo.
        array[i] = compute(i);
    }
}
```

---

## 10. Bug 2: unsigned integer overflow

**Nota**: unsigned overflow **no es UB** según el estándar C. Es comportamiento definido (wrap-around). Pero frecuentemente indica un bug lógico. Clang ofrece detección de unsigned overflow como extensión de UBSan.

```c
// unsigned_overflow.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Bug clásico: size_t underflow en resta
void process_data(const unsigned char *data, size_t len) {
    // Quitar header de 10 bytes
    size_t payload_len = len - 10;
    // Bug: si len < 10, payload_len wraps a un número ENORME
    // (size_t es unsigned: 5 - 10 = 18446744073709551611 en 64-bit)

    printf("Payload length: %zu\n", payload_len);

    // Intentar allocar ese tamaño absurdo → OOM o crash
    unsigned char *payload = (unsigned char *)malloc(payload_len);
    if (payload) {
        memcpy(payload, data + 10, payload_len);  // Lee fuera de bounds
        free(payload);
    }
}

// Bug: unsigned counter wrap-around en loop
unsigned int count_until(unsigned int start, unsigned int target) {
    unsigned int count = 0;
    // Si target < start, esto hace wrap-around y nunca termina
    // (o termina después de ~4 billones de iteraciones)
    for (unsigned int i = start; i != target; i++) {
        count++;
    }
    return count;
}

int main() {
    unsigned char small_data[] = {1, 2, 3};
    process_data(small_data, 3);  // Bug: len=3 < 10

    unsigned int c = count_until(10, 5);  // Bug: 10 → 5 nunca llega (sin wrap)
    printf("Count: %u\n", c);

    return 0;
}
```

```bash
# Detectar unsigned overflow (solo Clang):
$ clang -fsanitize=unsigned-integer-overflow -g -O1 \
        unsigned_overflow.c -o test
$ ./test

unsigned_overflow.c:7:33: runtime error: unsigned integer overflow:
  3 - 10 cannot be represented in type 'unsigned long'
```

### ¿Cuándo es legítimo el unsigned wrap-around?

```
┌─────────────────────────────────────────────────────────────────┐
│         Unsigned overflow: ¿bug o feature?                       │
│                                                                  │
│  LEGÍTIMO (wrap-around intencional):                             │
│  • Hash functions: h = h * 31 + c;                               │
│  • Circular buffers: index = (index + 1) % capacity;             │
│  • Crypto: operaciones mod 2^32 / 2^64                           │
│  • Sequence numbers: seq_num++;                                   │
│                                                                  │
│  BUG (wrap-around no intencional):                               │
│  • size_t len = len1 - len2; cuando len2 > len1                  │
│  • unsigned count = 0; count--; // → UINT_MAX                    │
│  • malloc(n * sizeof(T)); cuando n * sizeof(T) overflows         │
│  • for(unsigned i = n; i >= 0; i--) // loop infinito             │
│                                                                  │
│  Para suprimir legítimos:                                        │
│  __attribute__((no_sanitize("unsigned-integer-overflow")))       │
│  unsigned int hash(const char *s) { ... }                        │
└─────────────────────────────────────────────────────────────────┘
```

---

## 11. Bug 3: null pointer dereference

```c
// null_deref.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    int value;
    struct Node *next;
} Node;

Node *find(Node *head, int target) {
    Node *current = head;
    while (current != NULL) {
        if (current->value == target) return current;
        current = current->next;
    }
    return NULL;  // No encontrado
}

int main() {
    Node a = {1, NULL};
    Node b = {2, &a};
    Node *head = &b;

    Node *found = find(head, 99);  // No existe → retorna NULL

    // Bug: dereferenciar NULL
    printf("Found: %d\n", found->value);  // UB: found es NULL

    return 0;
}
```

```bash
$ clang -fsanitize=null -g -O1 null_deref.c -o test
$ ./test

null_deref.c:22:38: runtime error: member access within null pointer of type 'Node'
```

### Caso sutil: dereferencia antes del check

```c
// Caso peligroso: el compilador puede eliminar el NULL check

void process(int *ptr) {
    int value = *ptr;     // Dereferencia (si ptr es NULL → UB)
    if (ptr == NULL) {    // Check de NULL
        return;           // ← Compilador puede ELIMINAR esto
    }                     // porque ya hizo *ptr (implica ptr != NULL)
    use(value);
}

// UBSan detecta la dereferencia en la primera línea
```

---

## 12. Bug 4: misaligned pointer access

Cada tipo tiene requisitos de alineamiento. Un `int` (4 bytes) debe estar en dirección múltiplo de 4. Un `double` (8 bytes) en múltiplo de 8. Acceder a un tipo en dirección no alineada es UB.

```c
// misaligned.c
#include <stdio.h>
#include <stdint.h>
#include <string.h>

void print_int_at_offset(const char *buf, int offset) {
    // Bug: cast que puede producir puntero desalineado
    int *p = (int *)(buf + offset);
    printf("Value at offset %d: %d\n", offset, *p);  // UB si offset no alineado
}

// Caso real: parsear protocolo binario empaquetado
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint32_t length;    // Offset 1: NO alineado a 4 bytes
    uint16_t flags;     // Offset 5: NO alineado a 2 bytes
    uint8_t data[];
} PackedHeader;

void parse_header(const uint8_t *raw) {
    PackedHeader *hdr = (PackedHeader *)raw;

    // Bug: acceso a campos desalineados
    printf("Type: %u\n", hdr->type);
    printf("Length: %u\n", hdr->length);   // UB: offset 1, necesita offset 4
    printf("Flags: %u\n", hdr->flags);     // UB: offset 5, necesita offset 2,4,6...
}

int main() {
    char buffer[] = "ABCDEFGHIJKLMNOP";

    // Alineado (offset 0):
    print_int_at_offset(buffer, 0);   // OK si buffer está alineado

    // Desalineado (offset 1):
    print_int_at_offset(buffer, 1);   // UB: int* en dirección impar

    // Desalineado (offset 3):
    print_int_at_offset(buffer, 3);   // UB: int* en dirección no múltiplo de 4

    // Packed struct:
    uint8_t raw[] = {0x01, 0x0A, 0x00, 0x00, 0x00, 0x03, 0x00};
    parse_header(raw);

    return 0;
}
```

```bash
$ clang -fsanitize=alignment -g -O1 misaligned.c -o test
$ ./test

misaligned.c:8:51: runtime error: load of misaligned address 0x7ffd...
  for type 'int', which requires 4 byte alignment

misaligned.c:21:38: runtime error: member access within misaligned address 0x7ffd...
  for type 'PackedHeader', which requires 4 byte alignment
```

### Cómo corregir

```c
// Opción 1: usar memcpy (siempre seguro, el compilador lo optimiza)
void print_int_at_offset_safe(const char *buf, int offset) {
    int value;
    memcpy(&value, buf + offset, sizeof(int));  // No UB: memcpy maneja alignment
    printf("Value at offset %d: %d\n", offset, value);
}

// Opción 2: para packed structs, acceder campo a campo con memcpy
void parse_header_safe(const uint8_t *raw) {
    uint8_t type = raw[0];

    uint32_t length;
    memcpy(&length, raw + 1, sizeof(uint32_t));

    uint16_t flags;
    memcpy(&flags, raw + 5, sizeof(uint16_t));

    printf("Type: %u, Length: %u, Flags: %u\n", type, length, flags);
}
```

### ¿Por qué es un problema real?

```
┌─────────────────────────────────────────────────────────────────┐
│         Misalignment: el problema depende de la CPU              │
│                                                                  │
│  x86/x86_64:                                                     │
│    Accesos desalineados FUNCIONAN (con penalidad de rendimiento)│
│    El hardware maneja la desalineación transparentemente         │
│    → Muchos desarrolladores nunca ven el problema                │
│                                                                  │
│  ARM (pre-v7):                                                   │
│    Accesos desalineados causan SIGBUS (crash)                    │
│    → El programa falla en hardware ARM                           │
│                                                                  │
│  ARM (v7+, default):                                             │
│    Accesos desalineados FUNCIONAN pero con penalidad alta        │
│    (trap al kernel que emula el acceso)                           │
│                                                                  │
│  RISC-V:                                                         │
│    Depende de la implementación: puede fallar o funcionar        │
│                                                                  │
│  MIPS:                                                           │
│    Accesos desalineados causan excepción                         │
│                                                                  │
│  Resultado: código que "funciona" en x86 puede crashear          │
│  en ARM. UBSan detecta esto en cualquier plataforma.             │
└─────────────────────────────────────────────────────────────────┘
```

---

## 13. Bug 5: shift exponent out of range

Shifts con cantidades inválidas son UB:
- Shift por cantidad **negativa**
- Shift por cantidad **≥ ancho del tipo** (≥ 32 para int, ≥ 64 para long long)
- Left shift de valor **negativo** (en C, antes de C23)
- Left shift que causa **overflow de signed**

```c
// bad_shift.c
#include <stdio.h>
#include <stdint.h>

void demonstrate_bad_shifts(void) {
    int x = 1;

    // Bug 1: shift por cantidad negativa
    int a = x << -1;  // UB
    printf("1 << -1 = %d\n", a);

    // Bug 2: shift por cantidad >= ancho del tipo (int = 32 bits)
    int b = x << 32;  // UB: shift amount >= width
    printf("1 << 32 = %d\n", b);

    // Bug 3: shift por 33 (también >= 32)
    int c = x << 33;  // UB
    printf("1 << 33 = %d\n", c);

    // Bug 4: left shift de valor negativo (UB en C11, definido en C23)
    int neg = -1;
    int d = neg << 1;  // UB en C11: left shift of negative
    printf("-1 << 1 = %d\n", d);

    // Bug 5: left shift que causa signed overflow
    int e = 1 << 31;  // UB en C11: resultado no cabe en int
    // (1 << 31 = 2147483648, pero INT_MAX = 2147483647)
    printf("1 << 31 = %d\n", e);

    // OK: right shift de negativo es implementation-defined (no UB)
    int f = (-1) >> 1;  // Implementation-defined (no UB)
    printf("-1 >> 1 = %d\n", f);  // Típicamente -1 (arithmetic shift)
}

// Caso real: extraer bits de un valor
uint32_t extract_bits(uint32_t value, int start, int count) {
    // Bug: si start + count > 32 o count <= 0
    uint32_t mask = (1 << count) - 1;  // Bug si count == 32
    return (value >> start) & mask;
}

int main() {
    demonstrate_bad_shifts();

    // Bug: extract_bits con count=32
    uint32_t bits = extract_bits(0xDEADBEEF, 0, 32);  // 1 << 32 = UB
    printf("Extracted: 0x%08X\n", bits);

    return 0;
}
```

```bash
$ clang -fsanitize=shift -g -O1 bad_shift.c -o test
$ ./test

bad_shift.c:8:17: runtime error: shift exponent -1 is negative
bad_shift.c:11:17: runtime error: shift exponent 32 is too large for 32-bit type 'int'
bad_shift.c:14:17: runtime error: shift exponent 33 is too large for 32-bit type 'int'
bad_shift.c:18:19: runtime error: left shift of negative value -1
bad_shift.c:22:17: runtime error: left shift of 1 by 31 places cannot be
  represented in type 'int'
```

### Corrección

```c
uint32_t extract_bits_safe(uint32_t value, int start, int count) {
    if (count <= 0 || count > 32 || start < 0 || start >= 32) return 0;

    uint32_t mask;
    if (count == 32) {
        mask = UINT32_MAX;
    } else {
        mask = ((uint32_t)1 << count) - 1;  // Usar uint32_t para evitar signed shift
    }
    return (value >> start) & mask;
}
```

---

## 14. Bug 6: out-of-bounds array index

UBSan puede detectar accesos fuera de límites cuando el tamaño del array es conocido en compilación.

```c
// oob_index.c
#include <stdio.h>

int global_array[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

int get_value(int index) {
    // UBSan detecta si index < 0 o index >= 10
    return global_array[index];  // Bug si index fuera de rango
}

void stack_array_bug(void) {
    int local[5] = {10, 20, 30, 40, 50};

    // Bug: acceso fuera de límites
    for (int i = 0; i <= 5; i++) {  // Error: i <= 5 debería ser i < 5
        printf("local[%d] = %d\n", i, local[i]);
    }
}

// Caso real: lookup table
const char *day_name(int day) {
    static const char *days[] = {
        "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday", "Sunday"
    };
    // Bug: si day no está en [0,6]
    return days[day];  // UB si day < 0 o day > 6
}

int main() {
    // Fuera de rango por arriba:
    int v1 = get_value(10);   // Bug: index 10 en array de 10
    printf("global[10] = %d\n", v1);

    // Fuera de rango por abajo:
    int v2 = get_value(-1);   // Bug: index negativo
    printf("global[-1] = %d\n", v2);

    // Array en stack:
    stack_array_bug();

    // Lookup table:
    const char *name = day_name(7);  // Bug: 7 no es día válido
    printf("Day 7: %s\n", name);

    return 0;
}
```

```bash
$ clang -fsanitize=bounds -g -O1 oob_index.c -o test
$ ./test

oob_index.c:7:12: runtime error: index 10 out of bounds for type 'int[10]'
oob_index.c:7:12: runtime error: index -1 out of bounds for type 'int[10]'
oob_index.c:14:47: runtime error: index 5 out of bounds for type 'int[5]'
oob_index.c:23:12: runtime error: index 7 out of bounds for type 'const char *[7]'
```

**Nota**: el check `bounds` de UBSan solo funciona con arrays de tamaño estático. Para arrays dinámicos (punteros), se necesita ASan.

---

## 15. Bug 7: signed integer division overflow

Hay dos casos de UB en división entera:
1. **División por cero** (obvio)
2. **INT_MIN / -1** (resultado INT_MAX + 1, que no cabe en int)

```c
// div_overflow.c
#include <stdio.h>
#include <limits.h>

int safe_divide(int a, int b) {
    return a / b;  // Bug: no verifica división por 0 ni INT_MIN/-1
}

int main() {
    // Bug 1: división por cero
    int a = safe_divide(42, 0);  // UB + SIGFPE en x86
    printf("42 / 0 = %d\n", a);

    // Bug 2: INT_MIN / -1
    int b = safe_divide(INT_MIN, -1);  // UB: resultado sería INT_MAX + 1
    printf("INT_MIN / -1 = %d\n", b);

    // Bug 3: INT_MIN % -1 (mismo problema)
    int c = INT_MIN % -1;  // UB
    printf("INT_MIN %% -1 = %d\n", c);

    return 0;
}
```

```bash
$ clang -fsanitize=integer-divide-by-zero,signed-integer-overflow \
        -g -O1 div_overflow.c -o test
$ ./test

div_overflow.c:5:14: runtime error: division by zero
div_overflow.c:5:14: runtime error: division of -2147483648 by -1 cannot be
  represented in type 'int'
```

### Corrección

```c
#include <stdbool.h>

bool safe_divide_checked(int a, int b, int *result) {
    if (b == 0) return false;                     // División por cero
    if (a == INT_MIN && b == -1) return false;     // Overflow
    *result = a / b;
    return true;
}
```

---

## 16. Bug 8: float cast overflow

Convertir un float/double a int cuando el valor flotante está fuera del rango del tipo entero es UB.

```c
// float_cast.c
#include <stdio.h>
#include <float.h>
#include <math.h>
#include <stdint.h>

int main() {
    // Bug 1: double demasiado grande para int
    double big = 1e18;
    int a = (int)big;  // UB: 10^18 > INT_MAX (2.1 × 10^9)
    printf("(int)1e18 = %d\n", a);

    // Bug 2: double negativo demasiado grande
    double neg_big = -1e18;
    int b = (int)neg_big;  // UB: -10^18 < INT_MIN
    printf("(int)-1e18 = %d\n", b);

    // Bug 3: NaN a int
    double nan_val = NAN;
    int c = (int)nan_val;  // UB: NaN no tiene representación entera
    printf("(int)NaN = %d\n", c);

    // Bug 4: Infinito a int
    double inf_val = INFINITY;
    int d = (int)inf_val;  // UB: infinito no cabe en int
    printf("(int)Inf = %d\n", d);

    // Bug 5: double grande a uint8_t
    double medium = 300.0;
    uint8_t e = (uint8_t)medium;  // UB: 300 > 255
    printf("(uint8_t)300.0 = %u\n", e);

    // OK: valor que cabe
    double ok = 42.7;
    int f = (int)ok;  // OK: trunca a 42
    printf("(int)42.7 = %d\n", f);

    return 0;
}
```

```bash
$ clang -fsanitize=float-cast-overflow -g -O1 float_cast.c -o test
$ ./test

float_cast.c:9:13: runtime error: 1e+18 is outside the range of representable
  values of type 'int'
float_cast.c:13:13: runtime error: -1e+18 is outside the range of representable
  values of type 'int'
float_cast.c:17:13: runtime error: nan is outside the range of representable
  values of type 'int'
float_cast.c:21:13: runtime error: inf is outside the range of representable
  values of type 'int'
float_cast.c:25:19: runtime error: 300 is outside the range of representable
  values of type 'uint8_t'
```

### Caso relevante: en Rust

Rust safe tiene una diferencia significativa en float-to-int casts:

```rust
// Rust safe: saturating cast (NO es UB, definido desde Rust 1.45)
let big: f64 = 1e18;
let a: i32 = big as i32;  // Satura a i32::MAX = 2147483647
println!("{}", a);         // 2147483647 (no UB, comportamiento definido)

let nan: f64 = f64::NAN;
let b: i32 = nan as i32;  // NaN → 0 (definido)
println!("{}", b);         // 0

// Rust unsafe: NO hay forma de hacer UB con float cast en safe code
// (a diferencia de C donde SIEMPRE es UB si está fuera de rango)
```

---

## 17. Bug 9: invalid bool/enum value

En C++, cargar un `bool` que no es `0` ni `1`, o un `enum` con valor fuera del rango definido, es UB. En C, el check `bool` es relevante cuando se usa `_Bool` (C99+).

```c
// invalid_bool.c
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

int main() {
    // Crear un bool con valor inválido
    _Bool b;
    unsigned char raw = 42;  // Valor que no es 0 ni 1
    memcpy(&b, &raw, 1);    // Forzar valor 42 en un _Bool

    // Bug: cargar un bool con valor != 0 y != 1
    if (b) {  // UB: _Bool con valor 42
        printf("True? (value = %d)\n", (int)b);
    }

    return 0;
}
```

```bash
$ clang -fsanitize=bool -g -O1 invalid_bool.c -o test
$ ./test

invalid_bool.c:11:9: runtime error: load of value 42, which is not a valid
  value for type '_Bool'
```

### ¿Cómo ocurre en la práctica?

```
┌─────────────────────────────────────────────────────────────────┐
│      ¿Cuándo puede un bool tener valor distinto de 0/1?         │
│                                                                  │
│  1. Deserialización de datos binarios sin validación             │
│     struct Config { bool debug; ... };                           │
│     read(fd, &config, sizeof(config));                           │
│     // Si el byte en la posición de debug es 42, es UB           │
│                                                                  │
│  2. Unión con aliasing                                           │
│     union { bool b; unsigned char c; } u;                        │
│     u.c = 255;                                                   │
│     if (u.b) { ... }  // UB: bool con valor 255                 │
│                                                                  │
│  3. Memoria no inicializada (detectado por MSan también)         │
│     bool *flags = malloc(10 * sizeof(bool));                     │
│     if (flags[0]) { ... }  // UB: bool con valor basura          │
│                                                                  │
│  4. Corrupción de memoria (buffer overflow sobre un bool)        │
│     Otro bug escribe un valor > 1 en la posición del bool       │
│                                                                  │
│  5. FFI: función C retorna int donde Rust espera bool            │
│     int c_func(void) { return 42; }  // C: "truthy"             │
│     extern "C" fn c_func() -> bool;  // Rust: UB si != 0 o 1    │
└─────────────────────────────────────────────────────────────────┘
```

---

## 18. Bug 10: unreachable code reached

`__builtin_unreachable()` le dice al compilador que cierto punto del código es inalcanzable. Si la ejecución llega ahí, es UB. El compilador puede optimizar basándose en esta promesa.

```c
// unreachable.c
#include <stdio.h>

enum Direction { NORTH, SOUTH, EAST, WEST };

const char *direction_name(enum Direction d) {
    switch (d) {
        case NORTH: return "North";
        case SOUTH: return "South";
        case EAST:  return "East";
        case WEST:  return "West";
    }
    // "Esto nunca debería pasar"
    __builtin_unreachable();  // UB si se llega aquí
}

int main() {
    // Forzar un valor inválido:
    enum Direction bad = (enum Direction)99;
    const char *name = direction_name(bad);  // Llega a __builtin_unreachable
    printf("Direction: %s\n", name);

    return 0;
}
```

```bash
$ clang -fsanitize=unreachable -g -O1 unreachable.c -o test
$ ./test

unreachable.c:13:5: runtime error: execution reached an unreachable program point
```

### Equivalente en Rust

```rust
// Rust tiene unreachable!() y unreachable_unchecked()

fn direction_name(d: u8) -> &'static str {
    match d {
        0 => "North",
        1 => "South",
        2 => "East",
        3 => "West",
        _ => unsafe { std::hint::unreachable_unchecked() }
        // UB si se llega aquí con d >= 4
        // En safe Rust usarías: _ => unreachable!() (que PANICS)
    }
}
```

---

## 19. Bug 11: pointer overflow

Aritmética de punteros que produce un puntero fuera del objeto original o que causa wrap-around es UB.

```c
// ptr_overflow.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main() {
    int arr[10];

    // Bug 1: puntero más allá del final + 1
    int *p = arr + 11;  // UB: solo arr+0 a arr+10 son válidos
    // (arr+10 es válido como "one past the end" pero no dereferenciable)

    // Bug 2: puntero antes del inicio
    int *q = arr - 1;  // UB: no se puede retroceder antes del objeto

    // Bug 3: wrap-around
    char *huge = (char *)malloc(16);
    char *wrapped = huge + SIZE_MAX;  // UB: overflow en aritmética de punteros
    printf("wrapped: %p\n", (void *)wrapped);
    free(huge);

    return 0;
}
```

```bash
$ clang -fsanitize=pointer-overflow -g -O1 ptr_overflow.c -o test
$ ./test

ptr_overflow.c:9:18: runtime error: pointer index expression with base 0x7ffd...
  overflowed to 0x7ffd...
```

---

## 20. Bug 12: implicit conversion (integer truncation)

Clang ofrece checks para conversiones implícitas que pierden datos. No es UB estrictamente, pero es fuente de bugs frecuentes.

```c
// implicit_conv.c
#include <stdio.h>
#include <stdint.h>

void process_byte(uint8_t value) {
    printf("Byte: %u\n", value);
}

int main() {
    // Bug 1: truncar int a uint8_t
    int big = 300;
    uint8_t small = big;  // Truncación: 300 → 44 (300 % 256)
    printf("300 as uint8_t: %u\n", small);  // Imprime 44

    // Bug 2: pasar int grande a función que espera uint8_t
    process_byte(1000);  // Truncación: 1000 → 232

    // Bug 3: cambio de signo
    int negative = -1;
    unsigned int u = negative;  // Cambio de signo: -1 → 4294967295
    printf("-1 as unsigned: %u\n", u);

    // Bug 4: truncar size_t a int (problema de portabilidad 32/64 bit)
    size_t len = 5000000000ULL;  // 5 × 10^9, cabe en size_t pero no en int
    int ilen = (int)len;  // Truncación
    printf("5e9 as int: %d\n", ilen);

    return 0;
}
```

```bash
$ clang -fsanitize=implicit-integer-truncation,implicit-integer-sign-change \
        -g -O1 implicit_conv.c -o test
$ ./test

implicit_conv.c:11:21: runtime error: implicit conversion from type 'int' of value 300
  (32-bit, signed) to type 'uint8_t' (aka 'unsigned char') changed the value to 44
  (8-bit, unsigned)

implicit_conv.c:14:18: runtime error: implicit conversion from type 'int' of value 1000
  (32-bit, signed) to type 'uint8_t' (aka 'unsigned char') changed the value to 232
  (8-bit, unsigned)

implicit_conv.c:18:22: runtime error: implicit conversion from type 'int' of value -1
  (32-bit, signed) to type 'unsigned int' changed the value to 4294967295
  (32-bit, unsigned)
```

---

## 21. Bug 13: function type mismatch

Llamar una función a través de un puntero con tipo incompatible es UB.

```c
// func_mismatch.c
#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

double multiply(double a, double b) {
    return a * b;
}

typedef int (*IntFunc)(int, int);
typedef double (*DoubleFunc)(double, double);

int main() {
    IntFunc f = add;

    // Bug: llamar multiply a través de puntero con tipo de add
    IntFunc g = (IntFunc)multiply;  // Cast de tipo incompatible
    int result = g(3, 4);          // UB: tipo real es double(*)(double,double)
    printf("Result: %d\n", result);

    // Bug: llamar con número incorrecto de argumentos
    typedef int (*NoArgFunc)(void);
    NoArgFunc h = (NoArgFunc)add;   // Cast incompatible
    int r2 = h();                   // UB: add espera 2 args, recibe 0
    printf("Result2: %d\n", r2);

    return 0;
}
```

```bash
$ clang -fsanitize=function -g -O1 func_mismatch.c -o test
$ ./test

func_mismatch.c:18:18: runtime error: call to function multiply through pointer
  to incorrect function type 'int (*)(int, int)'
```

---

## 22. Bug 14: invalid object size

Cuando el compilador puede deducir el tamaño de un objeto, UBSan verifica que los accesos son válidos. Requiere `-O1` o superior.

```c
// object_size.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fill_buffer(char *buf, size_t buf_size, size_t fill_size) {
    // Bug: fill_size puede ser > buf_size
    memset(buf, 'A', fill_size);  // UBSan/ASan puede detectar
}

int main() {
    char small[8];

    // Bug: escribir 16 bytes en buffer de 8
    fill_buffer(small, sizeof(small), 16);

    printf("%.8s\n", small);
    return 0;
}
```

```bash
$ clang -fsanitize=object-size -g -O1 object_size.c -o test
```

**Nota**: este check tiene limitaciones. Solo funciona cuando el compilador puede rastrear el tamaño del objeto a través de las llamadas. ASan es más efectivo para buffer overflows.

---

## 23. Anatomía completa de un reporte UBSan

Los reportes de UBSan son los más legibles de todos los sanitizers.

```
┌──────────────────────────────────────────────────────────────────────┐
│              Reporte UBSan completo                                   │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ FORMATO GENERAL:                                               │  │
│  │                                                                │  │
│  │ archivo.c:línea:columna: runtime error: DESCRIPCIÓN            │  │
│  │                                                                │  │
│  │ Toda la información en UNA línea:                              │  │
│  │ • Ubicación exacta (archivo:línea:columna)                     │  │
│  │ • Tipo de error en lenguaje natural                            │  │
│  │ • Valores concretos involucrados                               │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  EJEMPLOS por tipo:                                                  │
│                                                                      │
│  Signed overflow:                                                    │
│  │ math.c:42:18: runtime error: signed integer overflow:             │
│  │   2147483647 + 1 cannot be represented in type 'int'              │
│  │   ↑            ↑                    ↑                 ↑           │
│  │   ubicación    valores              operación         tipo        │
│                                                                      │
│  Shift:                                                              │
│  │ bits.c:15:12: runtime error: shift exponent 33 is too large       │
│  │   for 32-bit type 'int'                                           │
│                                                                      │
│  Null deref:                                                         │
│  │ ptr.c:8:5: runtime error: load of null pointer of type 'int'      │
│  │                                                                   │
│  │ ptr.c:8:5: runtime error: member access within null pointer       │
│  │   of type 'struct Node'                                           │
│                                                                      │
│  Alignment:                                                          │
│  │ parse.c:22:14: runtime error: load of misaligned address          │
│  │   0x7fff1234abcd for type 'int', which requires 4 byte alignment  │
│  │   0x7fff1234abcd: note: pointer points here                       │
│  │    01 02 03 04 05 06 07 08  09 0a 0b 0c 0d 0e 0f 10              │
│  │             ^                                                     │
│                                                                      │
│  Float cast:                                                         │
│  │ conv.c:10:15: runtime error: 1e+18 is outside the range of       │
│  │   representable values of type 'int'                              │
│                                                                      │
│  Array bounds:                                                       │
│  │ arr.c:5:12: runtime error: index 10 out of bounds for type       │
│  │   'int[10]'                                                       │
│                                                                      │
│  Division:                                                           │
│  │ div.c:3:14: runtime error: division by zero                       │
│  │ div.c:8:14: runtime error: division of -2147483648 by -1          │
│  │   cannot be represented in type 'int'                             │
└──────────────────────────────────────────────────────────────────────┘
```

### Stack trace (con -fno-omit-frame-pointer)

```
program.c:42:18: runtime error: signed integer overflow:
  2147483647 + 1 cannot be represented in type 'int'
    #0 0x4c3e80 in compute program.c:42:18
    #1 0x4c3f00 in process program.c:58:12
    #2 0x4c4020 in main program.c:73:5
    #3 0x7f... in __libc_start_main
```

**Nota**: UBSan incluye stack trace solo con `-fno-omit-frame-pointer`. Sin ese flag, solo muestra la ubicación de una línea.

---

## 24. Modos de reporte: recover vs trap

UBSan tiene tres modos de operación que cambian lo que hace cuando detecta un error:

```
┌──────────────────────────────────────────────────────────────────────┐
│              Modos de reporte de UBSan                                │
│                                                                      │
│  MODO 1: Recovery (default)                                          │
│  ─────────────────────────                                           │
│  Flags: -fsanitize=undefined                                         │
│  Comportamiento:                                                     │
│    1. Imprime mensaje de error detallado                             │
│    2. CONTINÚA ejecutando el programa                                │
│    3. Detecta MÚLTIPLES errores en una ejecución                     │
│  Ventaja: ve todos los errores de una vez                            │
│  Desventaja: el programa continúa con UB (puede crashear después)   │
│  Uso: exploración inicial, "¿cuántos UB hay?"                       │
│                                                                      │
│  MODO 2: No recovery (RECOMENDADO para CI)                           │
│  ────────────────────────────────────────                             │
│  Flags: -fsanitize=undefined -fno-sanitize-recover=all               │
│  Comportamiento:                                                     │
│    1. Imprime mensaje de error detallado                             │
│    2. ABORTA el programa (exit code != 0)                            │
│    3. Solo detecta el PRIMER error                                   │
│  Ventaja: el test falla inmediatamente, CI lo detecta                │
│  Desventaja: solo ve un error por ejecución                         │
│  Uso: CI, testing automatizado, fuzzing                              │
│                                                                      │
│  MODO 3: Trap (MÍNIMO overhead)                                      │
│  ──────────────────────────────                                      │
│  Flags: -fsanitize=undefined -fsanitize-trap=undefined               │
│  Comportamiento:                                                     │
│    1. NO imprime mensaje                                             │
│    2. Ejecuta __builtin_trap() (instrucción ud2 en x86)              │
│    3. Proceso recibe SIGILL y muere                                  │
│  Ventaja: NO necesita runtime library (libubsan)                     │
│           Overhead MÍNIMO (~2 instrucciones por check)               │
│  Desventaja: no dice QUÉ UB se detectó (solo SIGILL + dirección)   │
│  Uso: producción, embedded, kernel, builds sin libubsan             │
│                                                                      │
│  Comparación de overhead:                                            │
│  Recovery   → ~1.2x CPU, necesita libubsan (~200KB)                  │
│  No recovery → ~1.2x CPU, necesita libubsan                         │
│  Trap       → ~1.05x CPU, NO necesita libubsan                       │
└──────────────────────────────────────────────────────────────────────┘
```

### Ejemplo de modo trap

```bash
# Compilar en modo trap (sin runtime):
clang -fsanitize=undefined -fsanitize-trap=undefined -g program.c -o program

# No enlaza libubsan. Si hay UB:
$ ./program
Illegal instruction (core dumped)
# No hay mensaje descriptivo, solo SIGILL

# Para debuggear, usar GDB:
$ gdb ./program
(gdb) run
Program received signal SIGILL, Illegal instruction.
0x00000000004c3e80 in compute () at program.c:42
42	    return a + b;
# Ahora sabes dónde fue el UB
```

### Configuración selectiva de modos

```bash
# Trap para shift y alignment, recovery para el resto:
clang -fsanitize=undefined \
      -fsanitize-trap=shift,alignment \
      -fno-sanitize-recover=signed-integer-overflow \
      program.c -o program

# Resultado:
# - shift o alignment → SIGILL (trap, sin mensaje)
# - signed overflow → mensaje + abort (no recovery)
# - null, bounds, etc → mensaje + continúa (recovery)
```

---

## 25. UBSan en Rust: ¿es necesario?

### La respuesta corta

```
┌─────────────────────────────────────────────────────────────────┐
│            UBSan y Rust: ¿hace falta?                            │
│                                                                  │
│  Safe Rust:  NO necesita UBSan                                   │
│  ─────────                                                       │
│  Rust previene TODO el UB que UBSan detecta:                     │
│  • Signed overflow  → panic! en debug, wrap en release           │
│  • Null dereference → no existen punteros null en safe            │
│  • Misalignment     → el compilador garantiza alineamiento       │
│  • Array bounds     → bounds check en cada indexación            │
│  • Division by zero → panic!                                     │
│  • Invalid bool     → solo true/false, enforced por tipo         │
│  • Shift overflow   → panic! en debug                            │
│  • Float cast       → saturating cast (definido desde 1.45)      │
│                                                                  │
│  Unsafe Rust:  PUEDE beneficiarse de UBSan (limitado)            │
│  ────────────                                                    │
│  Pero en la práctica, ASan y MSan son más útiles para unsafe.    │
│  UBSan en Rust es experimental y no todos los checks funcionan.  │
│                                                                  │
│  Conclusión: UBSan es primariamente una herramienta para C/C++.  │
│  En Rust, el compilador ya hace el trabajo de UBSan.             │
└─────────────────────────────────────────────────────────────────┘
```

### ¿Cómo maneja Rust cada tipo de UB?

```
┌─────────────────────────┬───────────────────────────┬───────────────────┐
│ UB en C                 │ Rust safe                  │ Rust unsafe       │
├─────────────────────────┼───────────────────────────┼───────────────────┤
│ Signed overflow          │ panic! (debug)            │ panic! (debug)    │
│                          │ wrapping (release)        │ wrapping (release)│
│                          │ checked_add() opcional    │                   │
├─────────────────────────┼───────────────────────────┼───────────────────┤
│ Null dereference         │ IMPOSIBLE (no null ptrs)  │ Posible con *ptr  │
├─────────────────────────┼───────────────────────────┼───────────────────┤
│ Misalignment             │ Compilador garantiza      │ Posible con       │
│                          │                           │ ptr::read_unaligned│
├─────────────────────────┼───────────────────────────┼───────────────────┤
│ Array out of bounds      │ panic! (bounds check)     │ get_unchecked()   │
│                          │                           │ no verifica       │
├─────────────────────────┼───────────────────────────┼───────────────────┤
│ Division by zero         │ panic!                    │ panic!            │
├─────────────────────────┼───────────────────────────┼───────────────────┤
│ Bad shift                │ panic! (debug)            │ panic! (debug)    │
│                          │ mask (release)            │ mask (release)    │
├─────────────────────────┼───────────────────────────┼───────────────────┤
│ Float → int overflow     │ Saturating (as cast)      │ Saturating        │
├─────────────────────────┼───────────────────────────┼───────────────────┤
│ Invalid bool             │ IMPOSIBLE                 │ Posible con       │
│                          │                           │ transmute         │
├─────────────────────────┼───────────────────────────┼───────────────────┤
│ Unreachable              │ unreachable!() → panic!   │ unreachable_      │
│                          │                           │ unchecked() → UB  │
└─────────────────────────┴───────────────────────────┴───────────────────┘
```

### Los métodos de aritmética segura de Rust

```rust
// Rust tiene 4 variantes de cada operación aritmética:

let a: i32 = 2_000_000_000;
let b: i32 = 2_000_000_000;

// 1. Operador normal: panic! en debug, wrap en release
// let c = a + b;  // panic! en debug mode

// 2. checked: retorna None en overflow
let c = a.checked_add(b);           // → None
assert_eq!(c, None);

// 3. wrapping: siempre wrap-around (como unsigned en C)
let d = a.wrapping_add(b);          // → -294967296

// 4. saturating: clamp al máximo/mínimo
let e = a.saturating_add(b);        // → i32::MAX = 2147483647

// 5. overflowing: retorna (resultado, bool overflow)
let (f, overflow) = a.overflowing_add(b);
assert!(overflow);

// El desarrollador Rust ELIGE el comportamiento:
// No hay UB posible en safe code.
```

---

## 26. Ejemplo completo: math_utils

Una biblioteca de utilidades matemáticas en C con múltiples UBs:

```c
// math_utils.c
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════
//  Utilidades matemáticas con bugs de UB
// ═══════════════════════════════════════════════════════════════

// ────────────────────────────────────────────────────────────────
// Bug 1: signed overflow en abs
// ────────────────────────────────────────────────────────────────
int my_abs(int x) {
    // Bug: -INT_MIN es UB (resultado sería INT_MAX + 1)
    if (x < 0) return -x;
    return x;
}

// ────────────────────────────────────────────────────────────────
// Bug 2: shift inválido en power-of-2 check
// ────────────────────────────────────────────────────────────────
int is_power_of_2(int n) {
    if (n <= 0) return 0;
    // Verificar cada bit
    for (int i = 0; i < 32; i++) {
        if (n == (1 << i)) return 1;  // Bug: 1 << 31 es UB para signed int
    }
    return 0;
}

// ────────────────────────────────────────────────────────────────
// Bug 3: division overflow
// ────────────────────────────────────────────────────────────────
int safe_div(int a, int b) {
    if (b == 0) return 0;  // Protege contra div/0
    // Bug: NO protege contra INT_MIN / -1
    return a / b;
}

// ────────────────────────────────────────────────────────────────
// Bug 4: float-to-int overflow
// ────────────────────────────────────────────────────────────────
int clamp_to_byte(double value) {
    // Intenta clampar a [0, 255]
    if (value < 0.0) return 0;
    if (value > 255.0) return 255;
    // Bug: si value es NaN, ningún if es true
    // NaN < 0.0 es false, NaN > 255.0 es false
    return (int)value;  // (int)NaN es UB
}

// ────────────────────────────────────────────────────────────────
// Bug 5: signed overflow en multiplicación
// ────────────────────────────────────────────────────────────────
int area(int width, int height) {
    // Bug: puede overflow si width * height > INT_MAX
    return width * height;
}

int scale(int value, int numerator, int denominator) {
    // Bug 1: value * numerator puede overflow
    // Bug 2: denominator == 0
    return (value * numerator) / denominator;
}

// ────────────────────────────────────────────────────────────────
// Bug 6: pointer overflow
// ────────────────────────────────────────────────────────────────
void *advance_pointer(void *base, size_t offset) {
    // Bug: si offset es enorme, (char*)base + offset wraps
    return (char *)base + offset;
}

// ────────────────────────────────────────────────────────────────
// Bug 7: implicit conversion
// ────────────────────────────────────────────────────────────────
uint8_t encode_temperature(int celsius) {
    // Rango esperado: -40 a 215 (mapeado a 0-255)
    int encoded = celsius + 40;
    // Bug: si celsius > 215, encoded > 255, truncación a uint8_t
    return (uint8_t)encoded;
}

// ────────────────────────────────────────────────────────────────
// main: activa todos los bugs
// ────────────────────────────────────────────────────────────────
int main() {
    printf("=== Bug 1: abs(INT_MIN) ===\n");
    int a = my_abs(INT_MIN);  // UB: negación de INT_MIN
    printf("abs(INT_MIN) = %d\n", a);

    printf("\n=== Bug 2: power of 2 check ===\n");
    int p = is_power_of_2(1073741824);  // 2^30, requiere check de 1<<31
    printf("is_power_of_2(2^30) = %d\n", p);

    printf("\n=== Bug 3: INT_MIN / -1 ===\n");
    int d = safe_div(INT_MIN, -1);  // UB
    printf("INT_MIN / -1 = %d\n", d);

    printf("\n=== Bug 4: (int)NaN ===\n");
    int c = clamp_to_byte(NAN);  // UB: NaN → int
    printf("clamp_to_byte(NaN) = %d\n", c);

    printf("\n=== Bug 5: overflow en multiplicación ===\n");
    int ar = area(100000, 100000);  // UB: 10^10 > INT_MAX
    printf("area(100000, 100000) = %d\n", ar);

    printf("\n=== Bug 6: pointer overflow ===\n");
    char buf[4];
    void *far = advance_pointer(buf, SIZE_MAX);  // UB
    printf("advanced: %p\n", far);

    printf("\n=== Bug 7: temperature truncation ===\n");
    uint8_t t = encode_temperature(300);  // 300+40=340 > 255
    printf("encode_temperature(300) = %u\n", t);

    return 0;
}
```

### Compilar y ejecutar

```bash
$ clang -fsanitize=undefined,integer \
        -fno-sanitize-recover=all \
        -fno-omit-frame-pointer -g -O1 \
        math_utils.c -o math_utils -lm
$ ./math_utils

=== Bug 1: abs(INT_MIN) ===
math_utils.c:18:28: runtime error: negation of -2147483648 cannot be represented
  in type 'int'; cast to an unsigned type to negate this value
```

### Corrección de todos los bugs

```c
// Bug 1: usar unsigned para abs
int my_abs_safe(int x) {
    if (x == INT_MIN) return INT_MAX;  // O manejar como caso especial
    if (x < 0) return -x;
    return x;
}

// Bug 2: usar unsigned para shifts
int is_power_of_2_safe(int n) {
    if (n <= 0) return 0;
    return (n & (n - 1)) == 0;  // Truco de bits, sin shifts
}

// Bug 3: proteger INT_MIN / -1
int safe_div_fixed(int a, int b) {
    if (b == 0) return 0;
    if (a == INT_MIN && b == -1) return INT_MAX;
    return a / b;
}

// Bug 4: verificar NaN explícitamente
int clamp_to_byte_safe(double value) {
    if (isnan(value)) return 0;
    if (value < 0.0) return 0;
    if (value > 255.0) return 255;
    return (int)value;
}

// Bug 5: verificar overflow antes de multiplicar
int area_safe(int width, int height) {
    if (width > 0 && height > INT_MAX / width) return -1;  // Overflow
    return width * height;
}

// Bug 7: clampar antes de convertir
uint8_t encode_temperature_safe(int celsius) {
    int encoded = celsius + 40;
    if (encoded < 0) encoded = 0;
    if (encoded > 255) encoded = 255;
    return (uint8_t)encoded;
}
```

---

## 27. Ejemplo completo: pixel_buffer

Un buffer de píxeles para procesamiento de imagen, con UBs comunes en código multimedia:

```c
// pixel_buffer.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ═══════════════════════════════════════════════════════════════
//  Buffer de píxeles con bugs de UB
// ═══════════════════════════════════════════════════════════════

typedef struct {
    uint8_t r, g, b, a;
} Pixel;

typedef struct {
    Pixel *data;
    int width;
    int height;
    int stride;  // bytes por fila (puede tener padding)
} PixelBuffer;

// ────────────────────────────────────────────────────────────────
// Bug 1: overflow en cálculo de tamaño
// ────────────────────────────────────────────────────────────────
PixelBuffer *pbuf_create(int width, int height) {
    PixelBuffer *pb = (PixelBuffer *)malloc(sizeof(PixelBuffer));
    pb->width = width;
    pb->height = height;
    pb->stride = width * (int)sizeof(Pixel);  // Bug: puede overflow

    // Bug 1: width * height puede overflow, resultando en malloc pequeño
    size_t total = (size_t)(width * height) * sizeof(Pixel);
    // Si width=65536 y height=65536:
    // width * height (como int) = 0 (overflow!)
    // total = 0 * sizeof(Pixel) = 0
    // malloc(0) retorna un puntero válido pero sin espacio

    pb->data = (Pixel *)calloc(1, total);
    return pb;
}

// ────────────────────────────────────────────────────────────────
// Bug 2: shift inválido en extracción de canal
// ────────────────────────────────────────────────────────────────
uint8_t extract_channel(uint32_t packed_pixel, int channel) {
    // channel: 0=R, 1=G, 2=B, 3=A
    int shift = channel * 8;
    // Bug: si channel >= 4 o channel < 0, shift es inválido
    return (uint8_t)(packed_pixel >> shift);  // UB si shift >= 32 o < 0
}

// ────────────────────────────────────────────────────────────────
// Bug 3: signed overflow en blend
// ────────────────────────────────────────────────────────────────
Pixel blend_pixels(Pixel a, Pixel b, int alpha_256) {
    // alpha_256: 0-256 (no 255, para poder representar 100%)
    Pixel result;

    // Bug: (a.r * alpha_256) puede overflow si alpha_256 > 256
    // 255 * 257 = 65535, no overflow en int.
    // Pero: la resta puede ir negativa y luego la conversión a uint8 es UB
    int r = a.r * alpha_256 + b.r * (256 - alpha_256);  // Bug si alpha_256 > 256
    result.r = (uint8_t)(r >> 8);  // Bug: si r es negativo, UB en shift de signed negativo

    int g = a.g * alpha_256 + b.g * (256 - alpha_256);
    result.g = (uint8_t)(g >> 8);

    int bl = a.b * alpha_256 + b.b * (256 - alpha_256);
    result.b = (uint8_t)(bl >> 8);

    result.a = 255;
    return result;
}

// ────────────────────────────────────────────────────────────────
// Bug 4: array out of bounds
// ────────────────────────────────────────────────────────────────
Pixel pbuf_get_pixel(const PixelBuffer *pb, int x, int y) {
    // Bug: no verifica límites
    return pb->data[y * pb->width + x];  // UB si x o y fuera de rango
}

void pbuf_set_pixel(PixelBuffer *pb, int x, int y, Pixel p) {
    pb->data[y * pb->width + x] = p;  // UB si fuera de rango
}

// ────────────────────────────────────────────────────────────────
// Bug 5: implicit truncation en brightness
// ────────────────────────────────────────────────────────────────
void pbuf_adjust_brightness(PixelBuffer *pb, int factor) {
    // factor: -255 a 255
    for (int y = 0; y < pb->height; y++) {
        for (int x = 0; x < pb->width; x++) {
            Pixel *p = &pb->data[y * pb->width + x];
            // Bug: el resultado puede ser > 255 o < 0
            // La conversión implícita a uint8_t trunca sin warning
            int new_r = p->r + factor;
            int new_g = p->g + factor;
            int new_b = p->b + factor;

            p->r = (uint8_t)new_r;  // Truncación si > 255 o < 0
            p->g = (uint8_t)new_g;
            p->b = (uint8_t)new_b;
        }
    }
}

// ────────────────────────────────────────────────────────────────
// Bug 6: null dereference
// ────────────────────────────────────────────────────────────────
void pbuf_copy(PixelBuffer *dst, const PixelBuffer *src) {
    // Bug: no verifica NULL ni que dimensiones coincidan
    size_t total = (size_t)(src->width * src->height) * sizeof(Pixel);
    memcpy(dst->data, src->data, total);
}

void pbuf_free(PixelBuffer *pb) {
    if (pb) {
        free(pb->data);
        free(pb);
    }
}

// ────────────────────────────────────────────────────────────────
int main() {
    // Caso normal
    PixelBuffer *pb = pbuf_create(640, 480);

    Pixel white = {255, 255, 255, 255};
    Pixel red = {255, 0, 0, 255};
    Pixel blue = {0, 0, 255, 255};

    pbuf_set_pixel(pb, 0, 0, white);
    pbuf_set_pixel(pb, 100, 100, red);

    // Bug 2: canal inválido
    uint8_t ch = extract_channel(0xDEADBEEF, 5);  // shift 40 >= 32
    printf("Channel 5: %u\n", ch);

    // Bug 3: alpha fuera de rango
    Pixel blended = blend_pixels(white, blue, 300);  // alpha > 256
    printf("Blended R: %u\n", blended.r);

    // Bug 4: pixel fuera de límites
    Pixel oob = pbuf_get_pixel(pb, 700, 500);  // fuera de 640x480
    printf("OOB pixel: R=%u\n", oob.r);

    // Bug 5: brightness overflow
    pbuf_set_pixel(pb, 0, 0, (Pixel){200, 200, 200, 255});
    pbuf_adjust_brightness(pb, 100);  // 200+100=300 > 255

    // Bug 6: NULL
    pbuf_copy(pb, NULL);  // src es NULL

    // Bug 1: overflow en dimensiones
    PixelBuffer *huge = pbuf_create(65536, 65536);
    printf("Huge buffer data: %p\n", (void *)huge->data);
    pbuf_free(huge);

    pbuf_free(pb);
    return 0;
}
```

### Compilar y ejecutar

```bash
$ clang -fsanitize=undefined,integer,implicit-conversion \
        -fno-sanitize-recover=all \
        -fno-omit-frame-pointer -g -O1 \
        pixel_buffer.c -o pixel_buffer
$ ./pixel_buffer

pixel_buffer.c:49:47: runtime error: shift exponent 40 is too large for
  32-bit type 'int'
```

### Corrección de bugs clave

```c
// Bug 1: usar size_t para cálculos de tamaño
PixelBuffer *pbuf_create_safe(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;

    PixelBuffer *pb = (PixelBuffer *)malloc(sizeof(PixelBuffer));
    pb->width = width;
    pb->height = height;
    pb->stride = width * (int)sizeof(Pixel);

    // Verificar overflow con size_t
    size_t w = (size_t)width;
    size_t h = (size_t)height;
    if (w > SIZE_MAX / (h * sizeof(Pixel))) {
        free(pb);
        return NULL;  // Overflow
    }
    size_t total = w * h * sizeof(Pixel);
    pb->data = (Pixel *)calloc(1, total);
    return pb;
}

// Bug 2: validar canal
uint8_t extract_channel_safe(uint32_t packed_pixel, int channel) {
    if (channel < 0 || channel > 3) return 0;
    return (uint8_t)(packed_pixel >> (channel * 8));
}

// Bug 5: clampar valores
static inline uint8_t clamp_u8(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

void pbuf_adjust_brightness_safe(PixelBuffer *pb, int factor) {
    for (int y = 0; y < pb->height; y++) {
        for (int x = 0; x < pb->width; x++) {
            Pixel *p = &pb->data[y * pb->width + x];
            p->r = clamp_u8(p->r + factor);
            p->g = clamp_u8(p->g + factor);
            p->b = clamp_u8(p->b + factor);
        }
    }
}
```

---

## 28. UBSan con tests unitarios

### Integrar UBSan en tests C

```bash
# Compilar tests con UBSan:
clang -fsanitize=undefined,integer \
      -fno-sanitize-recover=all \
      -fno-omit-frame-pointer -g -O1 \
      tests/test_math.c src/math_utils.c -o test_math

# Ejecutar:
./test_math
# Si hay UB, el test falla inmediatamente con:
# runtime error: ... cannot be represented in type 'int'
# Exit code != 0
```

### Estructura de test

```c
// tests/test_math.c
#include <assert.h>
#include <limits.h>
#include "../src/math_utils.h"

void test_abs_normal(void) {
    assert(my_abs(5) == 5);
    assert(my_abs(-5) == 5);
    assert(my_abs(0) == 0);
}

void test_abs_edge_cases(void) {
    assert(my_abs(INT_MAX) == INT_MAX);
    // Este test ACTIVARÁ UBSan si my_abs tiene el bug de INT_MIN:
    assert(my_abs(INT_MIN) == INT_MAX);  // ← UBSan detecta UB aquí
}

void test_safe_div_edge_cases(void) {
    assert(safe_div(10, 2) == 5);
    assert(safe_div(10, 0) == 0);  // Protegido
    // Este test ACTIVARÁ UBSan si no protege INT_MIN/-1:
    int result = safe_div(INT_MIN, -1);  // ← UBSan detecta UB aquí
    (void)result;
}

int main(void) {
    test_abs_normal();
    printf("test_abs_normal PASSED\n");

    test_abs_edge_cases();
    printf("test_abs_edge_cases PASSED\n");

    test_safe_div_edge_cases();
    printf("test_safe_div_edge_cases PASSED\n");

    return 0;
}
```

### Verificar en CI

```bash
# En CI (GitHub Actions, GitLab CI, etc.):
# Si UBSan detecta algo, exit code != 0, el CI falla

# Opción 1: -fno-sanitize-recover=all (abort en primer error)
clang -fsanitize=undefined -fno-sanitize-recover=all \
      -g -O1 test.c -o test
./test  # Exit code != 0 si hay UB

# Opción 2: UBSAN_OPTIONS (alternativa runtime)
UBSAN_OPTIONS="halt_on_error=1" ./test

# Ambos logran el mismo efecto: el proceso termina con error.
```

---

## 29. UBSan con fuzzing

UBSan es especialmente poderoso combinado con fuzzing. El fuzzer genera inputs que el humano no pensaría, y UBSan detecta los UBs que esos inputs provocan.

### En C con libFuzzer

```c
// fuzz_math.c
#include <stdint.h>
#include <stddef.h>
#include "math_utils.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return 0;

    int a, b;
    __builtin_memcpy(&a, data, 4);
    __builtin_memcpy(&b, data + 4, 4);

    // UBSan detectará overflow, div/0, etc.
    int sum = a + b;
    if (b != 0) {
        int div = a / b;
    }
    int abs_a = my_abs(a);
    int shifted = a << (b & 31);  // Limitar shift a 0-31

    return 0;
}
```

```bash
# Compilar con fuzzer + UBSan:
clang -fsanitize=fuzzer,undefined,integer \
      -fno-sanitize-recover=all \
      -fno-omit-frame-pointer -g -O1 \
      fuzz_math.c src/math_utils.c -o fuzz_math

# Ejecutar:
./fuzz_math -max_len=64 -max_total_time=60
```

### En Rust con cargo-fuzz + UBSan (para código C vía FFI)

```bash
# Si el crate tiene dependencias C compiladas con cc::Build:
# En build.rs, añadir flags de UBSan a la compilación C:

# build.rs
cc::Build::new()
    .file("src/c_math.c")
    .flag("-fsanitize=undefined")
    .flag("-fno-sanitize-recover=all")
    .compile("c_math");

# Luego fuzzear normalmente:
cargo +nightly fuzz run fuzz_target
```

### UBSan + ASan combinados (lo más común)

```bash
# LA combinación más recomendada para fuzzing:
clang -fsanitize=fuzzer,address,undefined,integer \
      -fno-sanitize-recover=all \
      -fno-omit-frame-pointer -g -O1 \
      fuzz_target.c src/*.c -o fuzz_target

# Detecta:
# - Buffer overflows, UAF, double-free (ASan)
# - Signed/unsigned overflow, shifts, null deref (UBSan)
# - Array bounds (UBSan para estáticos, ASan para dinámicos)

# Overhead total: ~2-2.5x CPU (ASan domina)
```

```
┌─────────────────────────────────────────────────────────────────┐
│       Orden recomendado de sanitizers para fuzzing               │
│                                                                  │
│  Nivel 1: ASan + UBSan (SIEMPRE)                                │
│  ├── Detecta: memory bugs + undefined behavior                  │
│  ├── Overhead: ~2-2.5x                                          │
│  └── Compatibles: SÍ                                            │
│                                                                  │
│  Nivel 2: MSan + UBSan (build separado)                          │
│  ├── Detecta: uninit reads + undefined behavior                  │
│  ├── Overhead: ~3-5x                                             │
│  └── Incompatible con ASan: builds separados                     │
│                                                                  │
│  Nivel 3: TSan + UBSan (si hay threads)                          │
│  ├── Detecta: data races + undefined behavior                    │
│  ├── Overhead: ~5-15x                                            │
│  └── Incompatible con ASan y MSan: build separado                │
│                                                                  │
│  UBSan está en TODOS los niveles: su bajo overhead lo justifica. │
└─────────────────────────────────────────────────────────────────┘
```

---

## 30. UBSan combinado con otros sanitizers

La combinación de UBSan con otros sanitizers es directa y sin conflictos.

### UBSan + ASan

```bash
# La combinación MÁS común:
clang -fsanitize=address,undefined,integer \
      -fno-sanitize-recover=all \
      -fno-omit-frame-pointer -g -O1 \
      program.c -o program

# Detecta TODO excepto:
# - Lectura de memoria no inicializada (necesita MSan)
# - Data races (necesita TSan)
```

### UBSan + MSan

```bash
# Para buscar uninit + UB:
clang -fsanitize=memory,undefined \
      -fsanitize-memory-track-origins=2 \
      -fno-sanitize-recover=all \
      -fno-omit-frame-pointer -g -O1 \
      program.c -o program

# NOTA: -fsanitize=integer NO es compatible con MSan en
# algunas versiones de Clang. Probar antes de adoptar.
```

### UBSan + TSan

```bash
# Para buscar data races + UB (programas multi-hilo):
clang -fsanitize=thread,undefined \
      -fno-sanitize-recover=all \
      -fno-omit-frame-pointer -g -O1 \
      -pthread program.c -o program
```

### Resumen de compatibilidad

```
┌──────────────────────────────────────────────────────────────────┐
│          Combinaciones válidas de sanitizers                      │
│                                                                  │
│  ✓ ASan + UBSan        (lo más común, recomendado siempre)       │
│  ✓ MSan + UBSan        (para uninit + UB)                        │
│  ✓ TSan + UBSan        (para data races + UB)                    │
│  ✓ ASan + UBSan + LSan (ASan incluye LSan por defecto)           │
│                                                                  │
│  ✗ ASan + MSan          (conflicto de shadow memory)             │
│  ✗ ASan + TSan          (conflicto de shadow memory)             │
│  ✗ MSan + TSan          (conflicto de shadow memory)             │
│  ✗ ASan + MSan + TSan   (incompatibles entre sí)                 │
│                                                                  │
│  UBSan es el PEGAMENTO: combina con cualquiera.                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 31. UBSAN_OPTIONS: configuración completa

```bash
# Sintaxis: clave=valor separados por ':'
UBSAN_OPTIONS="option1=value1:option2=value2" ./program
```

### Opciones principales

```
┌─────────────────────────────────┬──────────┬──────────────────────────────────┐
│ Opción                          │ Default  │ Descripción                       │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ halt_on_error                   │ 0        │ 1=abortar en primer error        │
│                                 │          │ (equivale a -fno-sanitize-recover│
│                                 │          │ pero en runtime)                 │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ print_stacktrace                │ 0        │ 1=imprimir stack trace completo  │
│                                 │          │ (no solo ubicación)              │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ report_error_type               │ 1        │ 1=incluir tipo de error en       │
│                                 │          │ el mensaje                       │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ silence_unsigned_overflow       │ 0        │ 1=no reportar unsigned overflow  │
│                                 │          │ (útil si activaste integer pero  │
│                                 │          │ tienes muchos falsos positivos)  │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ symbolize                       │ 1        │ 1=resolver símbolos              │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ external_symbolizer_path        │ ""       │ Ruta a llvm-symbolizer           │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ log_path                        │ stderr   │ Archivo para reportes            │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ exitcode                        │ 0 (*)    │ Código de salida en error        │
│                                 │          │ (*) 0 = no cambia el exit code   │
│                                 │          │ con halt_on_error=1 usa abort()  │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ strip_path_prefix               │ ""       │ Prefijo a eliminar de paths      │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ suppressions                    │ ""       │ Archivo de suppressions          │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ color                           │ auto     │ Colorear output (auto/always/    │
│                                 │          │ never)                           │
└─────────────────────────────────┴──────────┴──────────────────────────────────┘
```

### Ejemplos de uso

```bash
# Encontrar todos los errores con stack traces:
UBSAN_OPTIONS="halt_on_error=0:print_stacktrace=1" ./program

# Abortar en primer error:
UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" ./program

# Log a archivo:
UBSAN_OPTIONS="log_path=/tmp/ubsan.log:print_stacktrace=1" ./program

# Para CI:
UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1:symbolize=1" ./test

# Suprimir unsigned overflow (si usas -fsanitize=integer):
UBSAN_OPTIONS="silence_unsigned_overflow=1" ./program
```

---

## 32. Impacto en rendimiento

```
┌──────────────────────────────────────────────────────────────────────┐
│              Impacto de UBSan en rendimiento                          │
│                                                                      │
│  Recurso          │ Sin UBSan │ UBSan recover │ UBSan no-recover│Trap│
│  ─────────────────┼───────────┼───────────────┼─────────────────┼────│
│  CPU (tiempo)     │ 1x        │ ~1.2x         │ ~1.2x           │~1.05x
│  RAM              │ 1x        │ ~1x           │ ~1x             │~1x │
│  Tamaño binario   │ 1x        │ ~1.5-2x       │ ~1.5-2x         │~1.1x
│  Velocidad build  │ 1x        │ ~1.1x         │ ~1.1x           │~1.05x
│                                                                      │
│  Con checks individuales:                                            │
│                                                                      │
│  Check              │ Overhead por operación                         │
│  ────────────────── │ ────────────────────────                       │
│  signed-overflow    │ ~3 instrucciones (jo/jno)                      │
│  shift              │ ~4 instrucciones (cmp + branch)                │
│  null               │ ~2 instrucciones (test + jz)                   │
│  alignment          │ ~3 instrucciones (test + jnz)                  │
│  bounds             │ ~3 instrucciones (cmp + jae)                   │
│  float-cast         │ ~5-8 instrucciones (fcmp + branch)             │
│  division           │ ~3 instrucciones (cmp + je)                    │
│                                                                      │
│  ¿Por qué es tan bajo?                                               │
│  1. No hay shadow memory (ni lectura ni escritura de shadow)        │
│  2. Solo se instrumentan operaciones específicas (no cada load)     │
│  3. Los checks son branch hints: el compilador los marca como       │
│     "unlikely" para no afectar la predicción de branches            │
│  4. No se intercepta malloc/free/memcpy                              │
│                                                                      │
│  CONCLUSIÓN: UBSan es el único sanitizer viable para dejar           │
│  activado PERMANENTEMENTE en builds de testing/staging.              │
│  En producción, usar modo trap para overhead < 5%.                   │
└──────────────────────────────────────────────────────────────────────┘
```

### UBSan en producción: ¿sí o no?

```
┌─────────────────────────────────────────────────────────────────┐
│         UBSan en producción: un caso especial                    │
│                                                                  │
│  A diferencia de ASan/MSan/TSan (prohibitivos en producción),    │
│  UBSan en modo trap PUEDE usarse en producción:                  │
│                                                                  │
│  Quién lo hace:                                                  │
│  • Linux kernel: CONFIG_UBSAN=y en producción                    │
│  • Android: UBSan trap en componentes de seguridad              │
│  • Chrome: UBSan en release builds (selectivos)                  │
│                                                                  │
│  Cómo:                                                           │
│  clang -fsanitize=undefined -fsanitize-trap=undefined \          │
│        -fsanitize-recover=all  # O seleccionar checks            │
│        -O2 program.c                                             │
│                                                                  │
│  Ventajas:                                                       │
│  • Overhead < 5%                                                 │
│  • No necesita libubsan en producción                            │
│  • Crash inmediato en UB (mejor que comportamiento silencioso)   │
│  • Convierte UB exploitable en crash no-exploitable              │
│                                                                  │
│  Desventajas:                                                    │
│  • Crash (SIGILL) en vez de reporte legible                      │
│  • Puede crashear en UB que antes era "invisible"                │
│  • Necesita debugging post-mortem con core dump                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 33. Falsos positivos en UBSan

UBSan tiene pocos falsos positivos porque detecta UB según la especificación del lenguaje. Pero hay casos donde código "intencionalmente UB" es legítimo:

```
┌──────────────────────────────────────────────────────────────────────┐
│             Falsos positivos comunes en UBSan                        │
│                                                                      │
│  1. Unsigned overflow intencional (hashing, crypto)                  │
│     unsigned int hash(const char *s) {                               │
│         unsigned int h = 0;                                          │
│         while (*s) h = h * 31 + *s++;  // Overflow unsigned = OK    │
│     }                                                                │
│     Solución: no usar -fsanitize=unsigned-integer-overflow           │
│     o __attribute__((no_sanitize("unsigned-integer-overflow")))      │
│                                                                      │
│  2. Bit manipulation en signed (legacy code)                         │
│     int sign_extend(short s) {                                       │
│         return (int)s << 16 >> 16;  // UB: shift de valor negativo  │
│     }                                                                │
│     Solución: reescribir con unsigned                                │
│                                                                      │
│  3. Desalineamiento intencional (hardware MMIO)                      │
│     volatile uint32_t *reg = (uint32_t *)0x40021000;                 │
│     Solución: __attribute__((no_sanitize("alignment")))              │
│                                                                      │
│  4. Código que depende de complemento a 2 (wrap-around)              │
│     // Asume signed wrap-around:                                     │
│     int next_seq(int seq) { return seq + 1; }  // OK si < INT_MAX   │
│     Compilar con -fwrapv para hacer signed wrap definido.            │
│                                                                      │
│  5. Packed structs con campos desalineados                           │
│     #pragma pack(1)                                                  │
│     struct { uint8_t a; uint32_t b; };                               │
│     // UBSan reporta misalignment al acceder .b                      │
│     Solución: usar memcpy o no_sanitize("alignment")                 │
└──────────────────────────────────────────────────────────────────────┘
```

### Suprimir UBSan en funciones específicas

```c
// Suprimir un check específico:
__attribute__((no_sanitize("signed-integer-overflow")))
int wrapping_add(int a, int b) {
    return a + b;  // Intencional: queremos wrap-around
}

// Suprimir múltiples checks:
__attribute__((no_sanitize("undefined")))
void legacy_function(void) {
    // Código legacy con UB conocido y aceptado
}

// Suprimir solo alignment:
__attribute__((no_sanitize("alignment")))
uint32_t read_unaligned(const void *ptr) {
    return *(const uint32_t *)ptr;
}
```

### Alternativa global: -fno-sanitize

```bash
# Suprimir un check sin atributo:
clang -fsanitize=undefined -fno-sanitize=alignment program.c

# Compilar archivo específico sin UBSan:
# En Makefile:
legacy.o: legacy.c
	$(CC) $(CFLAGS) -fno-sanitize=undefined -c $< -o $@
```

---

## 34. Limitaciones de UBSan

```
┌──────────────────────────────────────────────────────────────────────┐
│                   Limitaciones de UBSan                               │
│                                                                      │
│  1. NO DETECTA TODO EL UB                                            │
│     El estándar C define 200+ formas de UB.                         │
│     UBSan cubre ~20-30 de las más comunes.                          │
│     NO detecta: strict aliasing, sequence points, restrict,          │
│     dangling pointers, infinite loops sin side effects.              │
│                                                                      │
│  2. REQUIERE RECOMPILAR                                              │
│     No puede analizar binarios existentes.                           │
│     (Valgrind sí puede, pero es mucho más lento.)                   │
│                                                                      │
│  3. CHECK bounds LIMITADO                                            │
│     Solo funciona con arrays de tamaño estático.                    │
│     int a[10]; a[15]; // Detectado                                   │
│     int *p = malloc(10*sizeof(int)); p[15]; // NO detectado          │
│     Para arrays dinámicos, se necesita ASan.                         │
│                                                                      │
│  4. CHECK object-size NECESITA OPTIMIZACIÓN                          │
│     Solo funciona con -O1 o superior.                                │
│     Con -O0 no tiene suficiente información de tamaño.              │
│                                                                      │
│  5. MODO RECOVERY PUEDE ENMASCARAR BUGS                             │
│     Si el programa continúa después de UB, el UB puede              │
│     causar más UBs en cascada, haciendo confuso el output.          │
│     Solución: usar -fno-sanitize-recover=all                         │
│                                                                      │
│  6. NO DETECTA UB EN CÓDIGO ELIMINADO POR OPTIMIZACIÓN              │
│     Si el compilador elimina código muerto con -O2,                  │
│     UBSan no inserta checks en ese código.                           │
│     Ejemplo: if (false) { UB; } // No detectado                     │
│                                                                      │
│  7. OVERHEAD DE TAMAÑO DE BINARIO                                    │
│     Con muchos checks, el binario puede crecer ~2x.                  │
│     Cada check inserta instrucciones + datos de source location.    │
│     En modo trap, mucho menor (~1.1x).                               │
│                                                                      │
│  8. DIFERENCIAS ENTRE CLANG Y GCC                                    │
│     Algunos checks solo están en Clang (integer, implicit-conv).     │
│     El output y comportamiento difieren entre compiladores.          │
│     Tests de CI deberían usar el mismo compilador consistentemente.  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 35. Errores comunes

### Error 1: olvidar -fno-sanitize-recover en CI

```bash
# PROBLEMA: UBSan reporta pero el programa termina con exit 0
clang -fsanitize=undefined program.c -o program
./program
# runtime error: signed integer overflow: ...
# (programa continúa y retorna 0)
echo $?  # 0 ← CI piensa que todo está bien!

# SOLUCIÓN: usar -fno-sanitize-recover=all
clang -fsanitize=undefined -fno-sanitize-recover=all program.c -o program
./program
# runtime error: signed integer overflow: ...
echo $?  # != 0 ← CI detecta el fallo
```

### Error 2: confundir unsigned overflow con bug

```bash
# unsigned overflow NO es UB, pero -fsanitize=integer lo reporta:
clang -fsanitize=integer program.c
# runtime error: unsigned integer overflow: 0 - 1 cannot be ...

# Si es intencional (hash, crypto), suprimir:
UBSAN_OPTIONS="silence_unsigned_overflow=1" ./program
# O usar: __attribute__((no_sanitize("unsigned-integer-overflow")))
```

### Error 3: no incluir -fno-omit-frame-pointer

```bash
# Sin -fno-omit-frame-pointer, los stack traces son incompletos:
clang -fsanitize=undefined program.c -o program
UBSAN_OPTIONS="print_stacktrace=1" ./program
# Stack trace truncado o con frames "??"

# SOLUCIÓN:
clang -fsanitize=undefined -fno-omit-frame-pointer -g program.c
```

### Error 4: esperar que UBSan detecte buffer overflows dinámicos

```c
// UBSan NO detecta esto:
int *p = malloc(10 * sizeof(int));
p[20] = 42;  // Buffer overflow en heap → necesitas ASan

// UBSan SÍ detecta esto (tamaño estático):
int arr[10];
arr[20] = 42;  // bounds check para array estático
```

### Error 5: olvidar -fsanitize en el linker

```bash
# ERROR: compilar con UBSan pero linkar sin
clang -fsanitize=undefined -c file.c -o file.o
clang file.o -o program  # ← Falta -fsanitize=undefined!
# undefined reference to __ubsan_handle_add_overflow

# SOLUCIÓN: poner -fsanitize en compilación Y linkeo
clang -fsanitize=undefined -c file.c -o file.o
clang -fsanitize=undefined file.o -o program
```

### Error 6: -fwrapv elimina detección de signed overflow

```bash
# -fwrapv hace que signed overflow sea DEFINED (wrap-around)
# Esto DESACTIVA el check de UBSan para signed overflow:
clang -fsanitize=signed-integer-overflow -fwrapv program.c
# UBSan no reportará signed overflows porque -fwrapv las hace definidas

# Si quieres DETECTAR (no definir) overflow, NO uses -fwrapv.
# -fwrapv es para código que NECESITA wrap-around de signed.
```

---

## 36. Programa de práctica: safe_calc

### Descripción

Una calculadora segura que debe manejar correctamente todos los edge cases aritméticos. Tiene múltiples UBs intencionalmente.

### Implementación con bugs

```c
// safe_calc.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <ctype.h>

// ═══════════════════════════════════════════════════════════════
//  Calculadora con bugs de UB
// ═══════════════════════════════════════════════════════════════

typedef enum {
    RESULT_OK,
    RESULT_OVERFLOW,
    RESULT_DIV_ZERO,
    RESULT_INVALID_OP,
    RESULT_PARSE_ERROR
} ResultCode;

typedef struct {
    int value;
    ResultCode code;
} CalcResult;

// ────────────────────────────────────────────────────────────────
// Bug 1: overflow en operaciones aritméticas
// ────────────────────────────────────────────────────────────────
CalcResult calc_add(int a, int b) {
    CalcResult r;
    r.value = a + b;  // Bug: signed overflow es UB
    r.code = RESULT_OK;
    return r;
}

CalcResult calc_sub(int a, int b) {
    CalcResult r;
    r.value = a - b;  // Bug: signed overflow es UB
    r.code = RESULT_OK;
    return r;
}

CalcResult calc_mul(int a, int b) {
    CalcResult r;
    r.value = a * b;  // Bug: signed overflow es UB
    r.code = RESULT_OK;
    return r;
}

CalcResult calc_div(int a, int b) {
    CalcResult r;
    if (b == 0) {
        r.value = 0;
        r.code = RESULT_DIV_ZERO;
        return r;
    }
    // Bug: no protege INT_MIN / -1
    r.value = a / b;
    r.code = RESULT_OK;
    return r;
}

CalcResult calc_mod(int a, int b) {
    CalcResult r;
    if (b == 0) {
        r.value = 0;
        r.code = RESULT_DIV_ZERO;
        return r;
    }
    // Bug: no protege INT_MIN % -1
    r.value = a % b;
    r.code = RESULT_OK;
    return r;
}

// ────────────────────────────────────────────────────────────────
// Bug 2: negación de INT_MIN
// ────────────────────────────────────────────────────────────────
CalcResult calc_neg(int a) {
    CalcResult r;
    r.value = -a;  // Bug: -INT_MIN = UB
    r.code = RESULT_OK;
    return r;
}

CalcResult calc_abs(int a) {
    CalcResult r;
    r.value = (a < 0) ? -a : a;  // Bug: -INT_MIN = UB
    r.code = RESULT_OK;
    return r;
}

// ────────────────────────────────────────────────────────────────
// Bug 3: shift inválido
// ────────────────────────────────────────────────────────────────
CalcResult calc_shl(int a, int b) {
    CalcResult r;
    // Bug: no verifica que b esté en [0, 31]
    // Bug: left shift de valor negativo es UB
    r.value = a << b;
    r.code = RESULT_OK;
    return r;
}

CalcResult calc_shr(int a, int b) {
    CalcResult r;
    // Bug: no verifica que b esté en [0, 31]
    // (right shift de negativo es implementation-defined, no UB)
    r.value = a >> b;
    r.code = RESULT_OK;
    return r;
}

// ────────────────────────────────────────────────────────────────
// Bug 4: conversión float-to-int
// ────────────────────────────────────────────────────────────────
CalcResult calc_from_float(double f) {
    CalcResult r;
    // Bug: no verifica rango ni NaN
    r.value = (int)f;
    r.code = RESULT_OK;
    return r;
}

// ────────────────────────────────────────────────────────────────
// Bug 5: factorial con overflow
// ────────────────────────────────────────────────────────────────
CalcResult calc_factorial(int n) {
    CalcResult r;
    if (n < 0) {
        r.value = 0;
        r.code = RESULT_INVALID_OP;
        return r;
    }
    r.value = 1;
    for (int i = 2; i <= n; i++) {
        r.value *= i;  // Bug: overflow para n >= 13
    }
    r.code = RESULT_OK;
    return r;
}

// ────────────────────────────────────────────────────────────────
// Bug 6: power con overflow
// ────────────────────────────────────────────────────────────────
CalcResult calc_power(int base, int exp) {
    CalcResult r;
    if (exp < 0) {
        r.value = 0;  // Entero: resultado truncado a 0
        r.code = RESULT_OK;
        return r;
    }
    r.value = 1;
    for (int i = 0; i < exp; i++) {
        r.value *= base;  // Bug: overflow rápido
    }
    r.code = RESULT_OK;
    return r;
}

// ────────────────────────────────────────────────────────────────
// Bug 7: parser con conversión de tipos
// ────────────────────────────────────────────────────────────────
CalcResult parse_int(const char *str) {
    CalcResult r;
    r.code = RESULT_OK;

    int negative = 0;
    const char *p = str;

    if (*p == '-') {
        negative = 1;
        p++;
    }

    long long acc = 0;
    while (*p) {
        if (!isdigit(*p)) {
            r.value = 0;
            r.code = RESULT_PARSE_ERROR;
            return r;
        }
        // Bug: acc puede overflow (long long es grande pero no infinito)
        acc = acc * 10 + (*p - '0');
        p++;
    }

    if (negative) acc = -acc;

    // Bug: implicit truncation long long → int
    r.value = (int)acc;  // Bug si acc fuera de rango de int
    return r;
}

// ────────────────────────────────────────────────────────────────
// Dispatch de operaciones
// ────────────────────────────────────────────────────────────────
CalcResult calc_eval(const char *op, int a, int b) {
    if (strcmp(op, "+") == 0) return calc_add(a, b);
    if (strcmp(op, "-") == 0) return calc_sub(a, b);
    if (strcmp(op, "*") == 0) return calc_mul(a, b);
    if (strcmp(op, "/") == 0) return calc_div(a, b);
    if (strcmp(op, "%") == 0) return calc_mod(a, b);
    if (strcmp(op, "<<") == 0) return calc_shl(a, b);
    if (strcmp(op, ">>") == 0) return calc_shr(a, b);
    if (strcmp(op, "neg") == 0) return calc_neg(a);
    if (strcmp(op, "abs") == 0) return calc_abs(a);
    if (strcmp(op, "!") == 0) return calc_factorial(a);
    if (strcmp(op, "**") == 0) return calc_power(a, b);

    CalcResult r = {0, RESULT_INVALID_OP};
    return r;
}

void print_result(const char *expr, CalcResult r) {
    switch (r.code) {
        case RESULT_OK:
            printf("%s = %d\n", expr, r.value);
            break;
        case RESULT_OVERFLOW:
            printf("%s = OVERFLOW\n", expr);
            break;
        case RESULT_DIV_ZERO:
            printf("%s = DIV_BY_ZERO\n", expr);
            break;
        case RESULT_INVALID_OP:
            printf("%s = INVALID_OP\n", expr);
            break;
        case RESULT_PARSE_ERROR:
            printf("%s = PARSE_ERROR\n", expr);
            break;
    }
}

// ────────────────────────────────────────────────────────────────
int main() {
    // Bug 1: overflow en suma
    print_result("INT_MAX + 1", calc_eval("+", INT_MAX, 1));

    // Bug 1: overflow en resta
    print_result("INT_MIN - 1", calc_eval("-", INT_MIN, 1));

    // Bug 1: overflow en multiplicación
    print_result("100000 * 100000", calc_eval("*", 100000, 100000));

    // Bug 1: INT_MIN / -1
    print_result("INT_MIN / -1", calc_eval("/", INT_MIN, -1));

    // Bug 1: INT_MIN % -1
    print_result("INT_MIN % -1", calc_eval("%", INT_MIN, -1));

    // Bug 2: negación de INT_MIN
    print_result("neg(INT_MIN)", calc_eval("neg", INT_MIN, 0));

    // Bug 2: abs de INT_MIN
    print_result("abs(INT_MIN)", calc_eval("abs", INT_MIN, 0));

    // Bug 3: shifts inválidos
    print_result("1 << 32", calc_eval("<<", 1, 32));
    print_result("1 << -1", calc_eval("<<", 1, -1));
    print_result("-1 << 5", calc_eval("<<", -1, 5));

    // Bug 4: float conversion
    print_result("float(1e18)", calc_from_float(1e18));
    print_result("float(NaN)", calc_from_float(NAN));

    // Bug 5: factorial overflow
    print_result("20!", calc_eval("!", 20, 0));

    // Bug 6: power overflow
    print_result("2**40", calc_eval("**", 2, 40));

    // Bug 7: parser overflow
    print_result("parse(99999999999999)", parse_int("99999999999999"));

    return 0;
}
```

### Compilar y ejecutar

```bash
$ clang -fsanitize=undefined,integer \
        -fno-sanitize-recover=all \
        -fno-omit-frame-pointer -g -O1 \
        safe_calc.c -o safe_calc -lm
$ ./safe_calc

safe_calc.c:32:17: runtime error: signed integer overflow:
  2147483647 + 1 cannot be represented in type 'int'
```

### Resumen de bugs

```
┌─────┬──────────────────────────────────────┬─────────────────────────────────┐
│ Bug │ Descripción                           │ UBSan check que detecta        │
├─────┼──────────────────────────────────────┼─────────────────────────────────┤
│  1  │ Overflow en +, -, *, /, %             │ signed-integer-overflow         │
│     │ (incluye INT_MIN / -1)                │ integer-divide-by-zero          │
├─────┼──────────────────────────────────────┼─────────────────────────────────┤
│  2  │ Negación de INT_MIN                   │ signed-integer-overflow         │
│     │ (-INT_MIN no cabe en int)             │ (negation subcategory)          │
├─────┼──────────────────────────────────────┼─────────────────────────────────┤
│  3  │ Shift por cantidad inválida           │ shift-exponent, shift-base      │
│     │ (negativa, >= 32, valor negativo)     │                                 │
├─────┼──────────────────────────────────────┼─────────────────────────────────┤
│  4  │ Float → int fuera de rango            │ float-cast-overflow             │
│     │ (NaN, infinito, demasiado grande)     │                                 │
├─────┼──────────────────────────────────────┼─────────────────────────────────┤
│  5  │ Factorial overflow (n >= 13)           │ signed-integer-overflow         │
├─────┼──────────────────────────────────────┼─────────────────────────────────┤
│  6  │ Power overflow rápido                  │ signed-integer-overflow         │
├─────┼──────────────────────────────────────┼─────────────────────────────────┤
│  7  │ Truncación long long → int            │ implicit-integer-truncation     │
│     │ en parser                             │ (Clang, -fsanitize=integer)     │
└─────┴──────────────────────────────────────┴─────────────────────────────────┘
```

### Corrección completa

```c
// Todas las funciones con protección contra UB:

CalcResult calc_add_safe(int a, int b) {
    CalcResult r;
    if (__builtin_add_overflow(a, b, &r.value)) {
        r.value = 0;
        r.code = RESULT_OVERFLOW;
    } else {
        r.code = RESULT_OK;
    }
    return r;
}

CalcResult calc_sub_safe(int a, int b) {
    CalcResult r;
    if (__builtin_sub_overflow(a, b, &r.value)) {
        r.value = 0;
        r.code = RESULT_OVERFLOW;
    } else {
        r.code = RESULT_OK;
    }
    return r;
}

CalcResult calc_mul_safe(int a, int b) {
    CalcResult r;
    if (__builtin_mul_overflow(a, b, &r.value)) {
        r.value = 0;
        r.code = RESULT_OVERFLOW;
    } else {
        r.code = RESULT_OK;
    }
    return r;
}

CalcResult calc_div_safe(int a, int b) {
    CalcResult r;
    if (b == 0) {
        r.value = 0;
        r.code = RESULT_DIV_ZERO;
    } else if (a == INT_MIN && b == -1) {
        r.value = 0;
        r.code = RESULT_OVERFLOW;
    } else {
        r.value = a / b;
        r.code = RESULT_OK;
    }
    return r;
}

CalcResult calc_neg_safe(int a) {
    CalcResult r;
    if (a == INT_MIN) {
        r.value = 0;
        r.code = RESULT_OVERFLOW;
    } else {
        r.value = -a;
        r.code = RESULT_OK;
    }
    return r;
}

CalcResult calc_shl_safe(int a, int b) {
    CalcResult r;
    if (b < 0 || b >= 32 || a < 0) {
        r.value = 0;
        r.code = RESULT_INVALID_OP;
    } else {
        // Verificar que no overflow
        unsigned int ua = (unsigned int)a;
        unsigned int result = ua << b;
        if (result > (unsigned int)INT_MAX) {
            r.value = 0;
            r.code = RESULT_OVERFLOW;
        } else {
            r.value = (int)result;
            r.code = RESULT_OK;
        }
    }
    return r;
}

CalcResult calc_from_float_safe(double f) {
    CalcResult r;
    if (isnan(f) || f > (double)INT_MAX || f < (double)INT_MIN) {
        r.value = 0;
        r.code = RESULT_OVERFLOW;
    } else {
        r.value = (int)f;
        r.code = RESULT_OK;
    }
    return r;
}

CalcResult calc_factorial_safe(int n) {
    CalcResult r;
    if (n < 0 || n > 12) {  // 12! = 479001600, 13! > INT_MAX
        r.value = 0;
        r.code = (n < 0) ? RESULT_INVALID_OP : RESULT_OVERFLOW;
        return r;
    }
    r.value = 1;
    for (int i = 2; i <= n; i++) {
        r.value *= i;  // Seguro: n <= 12
    }
    r.code = RESULT_OK;
    return r;
}

CalcResult parse_int_safe(const char *str) {
    CalcResult r;
    r.code = RESULT_OK;

    char *endptr;
    long long val = strtoll(str, &endptr, 10);
    if (*endptr != '\0' || endptr == str) {
        r.value = 0;
        r.code = RESULT_PARSE_ERROR;
    } else if (val > INT_MAX || val < INT_MIN) {
        r.value = 0;
        r.code = RESULT_OVERFLOW;
    } else {
        r.value = (int)val;
    }
    return r;
}
```

---

## 37. Ejercicios

### Ejercicio 1: interpretar reportes UBSan

**Objetivo**: Aprender a leer reportes de UBSan.

**Tareas**:

**a)** Dado este reporte, responde:
   - ¿Qué tipo de UB se detectó?
   - ¿Cuáles son los valores involucrados?
   - ¿Qué tipo de dato causó el problema?

```
parser.c:78:22: runtime error: signed integer overflow:
  1073741824 * 4 cannot be represented in type 'int'
```

**b)** Dado este reporte, responde:
   - ¿El shift es inválido por ser negativo o por ser demasiado grande?
   - ¿Cuál es el ancho del tipo?

```
bits.c:23:18: runtime error: shift exponent 35 is too large for 32-bit type 'unsigned int'
```

**c)** Dado este reporte, responde:
   - ¿Por qué el valor 0xFE no es válido?
   - ¿Cómo llegó un byte con valor 254 a un `_Bool`?

```
config.c:45:9: runtime error: load of value 254, which is not a valid value for type '_Bool'
```

**d)** Dado este reporte, responde:
   - ¿Cuál es la dirección del puntero?
   - ¿A qué alineamiento debería estar?
   - ¿Cuántos bytes de desalineamiento tiene?

```
network.c:112:14: runtime error: load of misaligned address 0x7fff12340003
  for type 'uint32_t', which requires 4 byte alignment
```

---

### Ejercicio 2: encontrar todos los UBs

**Objetivo**: Usar UBSan para encontrar todos los UBs en un programa.

**Tareas**:

**a)** Compila y ejecuta con UBSan:

```c
#include <stdio.h>
#include <limits.h>
#include <stdint.h>

int encode_rgb(int r, int g, int b) {
    return (r << 24) | (g << 16) | (b << 8);
}

int average(int a, int b) {
    return (a + b) / 2;
}

int safe_index(int *arr, int size, int idx) {
    return arr[idx % size];
}

uint8_t brightness(int pixel) {
    int r = (pixel >> 24) & 0xFF;
    int g = (pixel >> 16) & 0xFF;
    int b = (pixel >> 8) & 0xFF;
    return (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
}

int main() {
    int pixel = encode_rgb(255, 128, 64);
    printf("Pixel: 0x%08X\n", pixel);

    int avg = average(INT_MAX, INT_MAX);
    printf("Average: %d\n", avg);

    int arr[4] = {10, 20, 30, 40};
    int val = safe_index(arr, 4, -1);
    printf("Index -1 mod 4: %d\n", val);

    uint8_t br = brightness(pixel);
    printf("Brightness: %u\n", br);

    return 0;
}
```

**b)** ¿Cuántos UBs hay? Lista cada uno con:
   - Línea
   - Tipo de UB
   - Por qué es UB

**c)** Corrige todos los UBs. Verifica con UBSan.

**d)** ¿Cuáles de estos UBs existen en Rust safe? Explica por qué sí o no para cada uno.

---

### Ejercicio 3: UBSan con fuzzing

**Objetivo**: Combinar UBSan con libFuzzer para encontrar UBs.

**Tareas**:

**a)** Escribe un fuzzer para `calc_eval` del programa de práctica:

```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Extraer: operador (1 byte), a (4 bytes), b (4 bytes)
    // Mapear byte de operador a una de las operaciones
    // Llamar calc_eval
    return 0;
}
```

**b)** Compila con `fuzzer,undefined,integer` y ejecuta por 60 segundos.

**c)** ¿Cuántos crashes encontró? ¿Qué tipos de UB?

**d)** Corrige todos los UBs en `calc_eval` y verifica que ya no hay crashes.

---

### Ejercicio 4: UBSan en modo trap vs recovery

**Objetivo**: Experimentar los modos de UBSan.

**Tareas**:

**a)** Compila el mismo programa en los tres modos:
   ```bash
   # Modo 1: recovery (default)
   clang -fsanitize=undefined program.c -o prog_recover

   # Modo 2: no recovery
   clang -fsanitize=undefined -fno-sanitize-recover=all program.c -o prog_norecov

   # Modo 3: trap
   clang -fsanitize=undefined -fsanitize-trap=undefined program.c -o prog_trap
   ```

**b)** Ejecuta cada uno con un programa que tiene 3 UBs.

**c)** Compara:
   - ¿Cuántos UBs reporta cada modo?
   - ¿Qué información da cada reporte?
   - ¿Cuál es el exit code de cada uno?
   - ¿Cuál es el tamaño del binario?

**d)** Mide el tiempo de ejecución de un loop con 10 millones de iteraciones en cada modo. ¿Cuál es más rápido?

**e)** ¿En qué escenario usarías cada modo?

---

## Navegación

- **Anterior**: [T02 - MemorySanitizer (MSan)](../T02_MemorySanitizer/README.md)
- **Siguiente**: [T04 - ThreadSanitizer (TSan)](../T04_ThreadSanitizer/README.md)

---

> **Sección 3: Sanitizers como Red de Seguridad** — Tópico 3 de 4 completado