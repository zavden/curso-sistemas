# Bloque 3: Fundamentos de C

**Objetivo**: Dominar C desde cero hasta nivel intermedio, incluyendo gestión manual
de memoria, punteros, y el modelo de compilación.

## Capítulo 1: El Compilador y el Proceso de Compilación [A]

### Sección 1: GCC
- **T01 - Fases de compilación**: preprocesamiento (-E), compilación (-S), ensamblado (-c), enlace
- **T02 - Flags esenciales**: -Wall, -Wextra, -Werror, -std=c11, -pedantic, -g, -O0/-O2/-O3
- **T03 - Archivos objeto y enlace**: .o, enlace estático vs dinámico, -l, -L, -I
- **T04 - Comportamiento indefinido (UB)**: qué es, por qué es peligroso, ejemplos clásicos

### Sección 2: Herramientas de Diagnóstico
- **T01 - GDB básico**: breakpoints, step, next, print, backtrace, watch
- **T02 - Valgrind**: memcheck, detección de leaks, accesos inválidos, uso con GDB
- **T03 - AddressSanitizer/UBSan**: -fsanitize=address, -fsanitize=undefined, cuándo preferir sobre Valgrind

### Sección 3: Estándar del Lenguaje
- **T01 - Evolución de C**: C89/C90 → C99 → C11 → C17 → C23, cambios clave de cada revisión
- **T02 - Estándar del curso (C11)**: por qué C11 como base, features usadas (static_assert, anonymous structs/unions, _Generic, threads.h)
- **T03 - Comportamiento por compilador**: GCC default (-std=gnu17), diferencia entre -std=c11 y -std=gnu11 (extensiones GNU), Clang vs GCC
- **T04 - Features de C23 mencionadas**: __VA_OPT__, constexpr, typeof — se señalan explícitamente cuando aparezcan

## Capítulo 2: Tipos de Datos y Variables [A]

### Sección 1: Tipos Primitivos
- **T01 - Enteros**: char, short, int, long, long long — tamaños (¡no garantizados!), stdint.h
- **T02 - Punto flotante**: float, double, long double — precisión, IEEE 754, errores de redondeo
- **T03 - sizeof y alineación**: padding en structs, #pragma pack, alignof (C11)
- **T04 - Signed vs unsigned**: overflow signed (UB) vs unsigned (wrapping), conversiones implícitas peligrosas
- **T05 - Endianness**: big-endian vs little-endian, byte order en memoria, htonl/htons/ntohl/ntohs, impacto en lectura de archivos binarios y protocolos de red

### Sección 2: Variables y Almacenamiento
- **T01 - Clases de almacenamiento**: auto, static, extern, register, _Thread_local (C11)
- **T02 - Scope y lifetime**: block scope, file scope, linkage interno vs externo
- **T03 - Calificadores**: const, volatile, restrict — semántica real de cada uno
- **T04 - Inicialización**: valores por defecto (¡depende del storage class!), designated initializers (C99)

## Capítulo 3: Operadores y Control de Flujo [A]

### Sección 1: Operadores
- **T01 - Aritméticos y de asignación**: precedencia completa, asociatividad, promoción de enteros
- **T02 - Bitwise**: &, |, ^, ~, <<, >> — aplicaciones prácticas (máscaras, flags)
- **T03 - Lógicos y relacionales**: short-circuit evaluation, diferencia entre & y &&
- **T04 - Operador coma y ternario**: usos legítimos, trampas comunes

### Sección 2: Control de Flujo
- **T01 - if/else, switch**: fall-through en switch (es una feature, no un bug), Duff's device
- **T02 - Loops**: for, while, do-while — diferencias sutiles, loops infinitos idiomáticos
- **T03 - goto**: cuándo es legítimo (cleanup en C), cuándo no, comparación con Rust

## Capítulo 4: Funciones [A]

### Sección 1: Declaración y Definición
- **T01 - Prototipos**: declaración forward, diferencia entre f() y f(void) en C
- **T02 - Paso por valor**: todo en C se pasa por valor — simulando paso por referencia con punteros
- **T03 - Variadic functions**: stdarg.h, va_list, va_start, va_arg, va_end — cómo funciona printf

### Sección 2: Funciones Avanzadas
- **T01 - Punteros a función**: sintaxis, typedef, callbacks, tablas de despacho
- **T02 - Inline**: inline keyword, comportamiento real en C vs expectativa, ODR
- **T03 - _Noreturn y _Static_assert**: C11 features útiles

## Capítulo 5: Arrays y Cadenas [A]

### Sección 1: Arrays
- **T01 - Declaración y acceso**: arrays estáticos, VLAs (C99), inicialización
- **T02 - Arrays y punteros**: la relación array→puntero, por qué array ≠ puntero
- **T03 - Arrays multidimensionales**: layout en memoria (row-major), array de punteros vs array 2D real

### Sección 2: Cadenas
- **T01 - El modelo de strings en C**: null-terminated, por qué es peligroso
- **T02 - string.h**: strlen, strcpy, strncpy, strcmp, strcat, memcpy, memmove — y por qué strncpy no hace lo que crees
- **T03 - Buffer overflows**: cómo ocurren, exploits clásicos (stack smashing), mitigaciones

## Capítulo 6: Punteros [A]

### Sección 1: Fundamentos
- **T01 - Qué es un puntero**: dirección de memoria, operadores & y *, NULL
- **T02 - Aritmética de punteros**: incremento, diferencia, relación con arrays
- **T03 - Punteros a punteros**: doble indirección, uso en parámetros de salida
- **T04 - Puntero void***: genericidad en C, casting, reglas de aliasing

### Sección 2: Punteros Avanzados
- **T01 - const y punteros**: const int*, int* const, const int* const — lectura derecha a izquierda
- **T02 - Punteros a arrays vs arrays de punteros**: sintaxis confusa, regla de decodificación
- **T03 - restrict**: qué promete al compilador, cuándo usarlo para optimización
- **T04 - Punteros colgantes y wild pointers**: causas, detección, prevención

## Capítulo 7: Memoria Dinámica [A]

### Sección 1: Asignación y Liberación
- **T01 - malloc, calloc, realloc, free**: diferencias, errores comunes, patrones seguros
- **T02 - Fragmentación de heap**: qué es, por qué importa, allocators alternativos
- **T03 - Memory leaks**: detección con Valgrind, patrones de ownership en C

### Sección 2: Patrones de Gestión
- **T01 - Ownership manual**: convenciones para quién libera qué
- **T02 - Arena allocators**: concepto, implementación simple, ventajas
- **T03 - RAII en C**: cleanup con goto, __attribute__((cleanup)), comparación con Rust

## Capítulo 8: Estructuras, Uniones y Enumeraciones [A]

### Sección 1: Structs
- **T01 - Declaración y uso**: inicialización, designated initializers, acceso con . y ->
- **T02 - Padding y alineación**: reglas del compilador, #pragma pack, offsetof
- **T03 - Structs anidados y self-referential**: listas enlazadas, árboles
- **T04 - Bit fields**: sintaxis, portabilidad (¡no garantizada!), uso en protocolos

### Sección 2: Uniones y Enums
- **T01 - Unions**: overlapping memory, tagged unions (patrón struct+union+enum)
- **T02 - Enums**: valores, conversión implícita a int, buenas prácticas
- **T03 - typedef**: convenciones, opaque types, forward declarations

## Capítulo 9: Entrada/Salida de Archivos [A]

### Sección 1: stdio.h (Buffered I/O)
- **T01 - FILE*, fopen, fclose**: modos de apertura, errores comunes
- **T02 - fread, fwrite, fgets, fputs**: I/O binario vs texto, diferencias de newline entre OS
- **T03 - fprintf, fscanf, sprintf, sscanf**: formateo, vulnerabilidades de format string
- **T04 - Buffering**: full, line, unbuffered — setvbuf, fflush, por qué printf sin \n no imprime

### Sección 2: POSIX I/O (Unbuffered)
- **T01 - open, close, read, write**: file descriptors, flags (O_RDONLY, O_CREAT, O_APPEND...)
- **T02 - lseek**: posicionamiento, SEEK_SET/CUR/END, sparse files
- **T03 - Diferencia entre stdio y POSIX I/O**: cuándo usar cada uno, mixing

## Capítulo 10: El Preprocesador [A]

### Sección 1: Directivas
- **T01 - #include y guardas**: guardas de inclusión, #pragma once, include paths
- **T02 - #define**: macros con y sin parámetros, operadores # y ##, macros multi-línea
- **T03 - Macros peligrosas**: side effects, evaluación múltiple, do { } while(0) idiom
- **T04 - Compilación condicional**: #if, #ifdef, #ifndef, #elif, #else, #endif

### Sección 2: Macros Avanzadas
- **T01 - Variadic macros**: __VA_ARGS__, __VA_OPT__ (C23)
- **T02 - _Generic**: despacho por tipo (pseudo-overloading en C11)
- **T03 - Macros predefinidas**: __FILE__, __LINE__, __func__, __DATE__, __STDC_VERSION__

## Capítulo 11: Bibliotecas [A]

### Sección 1: Bibliotecas Estáticas
- **T01 - Creación**: ar, ranlib, convención de nombres (lib*.a)
- **T02 - Enlace estático**: -l, -L, orden de los argumentos (¡importa!)
- **T03 - Ventajas y desventajas**: tamaño binario, independencia, actualización

### Sección 2: Bibliotecas Dinámicas
- **T01 - Creación**: -fPIC, -shared, convención de nombres (lib*.so.X.Y.Z)
- **T02 - Enlace dinámico**: ld-linux.so, LD_LIBRARY_PATH, ldconfig, rpath
- **T03 - Versionado de soname**: soname, real name, linker name — por qué existen los 3
- **T04 - dlopen/dlsym/dlclose**: carga dinámica en runtime, plugins
