# T04 — Features de C23 mencionadas

## Rol de C23 en este curso

Este curso usa C11 como estándar base. Sin embargo, C23 introduce
features que vale la pena conocer porque:

1. Ya están disponibles como extensiones de GCC/Clang
2. Se están adoptando rápidamente en proyectos nuevos
3. Algunas estandarizan lo que antes eran extensiones GNU

Cuando una feature de C23 aparezca en el curso, se marcará
explícitamente. Este tópico cubre las tres más relevantes.

## __VA_OPT__ — Macros variádicas sin la coma extra

### El problema

```c
// Macro variádica clásica:
#define LOG(fmt, ...) printf("[LOG] " fmt "\n", __VA_ARGS__)

LOG("x=%d y=%d", x, y);   // OK: printf("[LOG] x=%d y=%d\n", x, y)
LOG("hello");               // ERROR: printf("[LOG] hello\n", )
//                                                           ^ coma extra
```

```c
// El problema: si no pasas argumentos variádicos,
// queda una coma colgante antes del paréntesis de cierre.

// Solución GNU (extensión, no estándar):
#define LOG(fmt, ...) printf("[LOG] " fmt "\n", ##__VA_ARGS__)
// ## elimina la coma si __VA_ARGS__ está vacío

LOG("hello");    // printf("[LOG] hello\n")  ← funciona, pero es extensión GNU
```

### La solución de C23

```c
// __VA_OPT__(contenido) — se expande a "contenido" solo si
// hay argumentos variádicos. Si no hay, desaparece completo:

#define LOG(fmt, ...) printf("[LOG] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)

LOG("x=%d y=%d", x, y);   // printf("[LOG] x=%d y=%d\n", x, y)
LOG("hello");               // printf("[LOG] hello\n")
//                           __VA_OPT__(,) desaparece porque no hay args
```

```c
// __VA_OPT__ puede contener cualquier tokens, no solo coma:

#define CALL(func, ...) func(__VA_OPT__(__VA_ARGS__))
CALL(foo, 1, 2, 3);    // foo(1, 2, 3)
CALL(bar);              // bar()

#define PRINT(...) printf(__VA_OPT__(__VA_ARGS__) __VA_OPT__(,) "done\n")
PRINT("x=%d ", 42);    // printf("x=%d ", 42, "done\n")
PRINT();                // printf("done\n")
```

```c
// Ejemplo práctico — debug logging:
#define DEBUG(fmt, ...) \
    fprintf(stderr, "[%s:%d] " fmt "\n", \
            __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)

DEBUG("starting");                // [main.c:10] starting
DEBUG("value: %d", x);           // [main.c:11] value: 42
DEBUG("a=%d b=%d c=%d", a, b, c); // [main.c:12] a=1 b=2 c=3
```

### Disponibilidad

```bash
# GCC: soportado desde GCC 8 (incluso sin -std=c23)
# Clang: soportado desde Clang 12

# Para usar en modo C11:
gcc -std=gnu11 main.c -o main   # funciona como extensión
gcc -std=c11 main.c -o main     # warning con -Wpedantic

# Para usar en modo C23:
gcc -std=c23 main.c -o main     # estándar
```

## constexpr — Constantes en compilación

### El problema con const

```c
// const en C NO significa "constante en compilación":
const int SIZE = 10;

int arr[SIZE];           // ERROR en C11 estricto
                         // SIZE no es una "constant expression"
                         // Es una variable de solo lectura

// En C11, para constantes en compilación se usan #define:
#define SIZE 10
int arr[SIZE];           // OK — el preprocesador reemplaza antes de compilar

// Pero #define tiene problemas:
// - No tiene tipo (es sustitución de texto)
// - No tiene scope
// - Puede causar errores sutiles en macros complejas
```

### La solución de C23

```c
// constexpr crea una verdadera constante de compilación:
constexpr int SIZE = 10;
int arr[SIZE];                  // OK en C23

constexpr double PI = 3.14159265358979;
constexpr int MAX_THREADS = 8;
```

```c
// Diferencias entre const y constexpr:

const int a = 10;
constexpr int b = 10;

int arr1[a];     // NO válido en C estándar (a es variable)
int arr2[b];     // Válido en C23 (b es constante de compilación)

switch (x) {
    case a: break;   // NO válido (a no es constant expression)
    case b: break;   // Válido en C23
}

// constexpr REQUIERE que el valor sea conocido en compilación:
int runtime_val = get_value();
constexpr int c = runtime_val;  // ERROR — no es constante
```

### Restricciones de constexpr en C

```c
// constexpr en C es MÁS LIMITADO que en C++:

// C23 constexpr:
// - Solo para variables escalares y arrays
// - El inicializador debe ser evaluable en compilación
// - No hay funciones constexpr (a diferencia de C++)

constexpr int X = 5;            // OK
constexpr int Y = X + 3;        // OK — expresión constante
constexpr int arr[] = {1,2,3};  // OK — array

// NO válido en C23 (sí en C++):
constexpr int square(int x) { return x * x; }  // NO en C
```

### Disponibilidad

```bash
# GCC: soporte desde GCC 13
# Clang: soporte desde Clang 17

gcc -std=c23 main.c -o main
```

```c
// Hasta que C23 sea universal, alternativas:

// 1. #define (sin tipo, sin scope):
#define MAX 100

// 2. enum (solo int):
enum { MAX = 100 };
int arr[MAX];           // OK — enum es constant expression

// 3. const (con la limitación):
static const int MAX = 100;
// Funciona como constante en la práctica con GCC/Clang
// (aunque técnicamente no es "constant expression" en C11)
```

## typeof — Deducir el tipo de una expresión

### El problema

```c
// A veces necesitas declarar una variable del mismo tipo que otra:
int x = 42;
int y = x * 2;         // tienes que escribir "int" explícitamente

// Si cambias x a long, tienes que cambiar y también.
// Si x viene de una macro genérica, no sabes qué tipo tiene.
```

### typeof en C23

```c
// typeof(expresión) produce el tipo de la expresión:
int x = 42;
typeof(x) y = x * 2;           // y es int

double arr[10];
typeof(arr[0]) sum = 0.0;      // sum es double

// typeof no evalúa la expresión — solo determina el tipo:
typeof(printf("hello")) ret;    // ret es int (tipo de retorno de printf)
                                 // "hello" NO se imprime
```

```c
// typeof_unqual — tipo sin calificadores (const, volatile):
const int x = 42;
typeof(x) y = 10;              // y es const int — no se puede modificar
typeof_unqual(x) z = 10;       // z es int — modificable
z = 20;                         // OK
```

### Uso en macros genéricas

```c
// typeof es especialmente útil en macros type-safe:

// Macro MAX sin typeof:
#define MAX(a, b) ((a) > (b) ? (a) : (b))
// Problema: evalúa a o b dos veces
int x = MAX(i++, j++);    // i++ o j++ se ejecuta dos veces — bug

// Macro MAX con typeof (segura):
#define MAX(a, b) ({            \
    typeof(a) _a = (a);         \
    typeof(b) _b = (b);         \
    _a > _b ? _a : _b;          \
})
int x = MAX(i++, j++);    // cada uno se evalúa exactamente una vez

// Nota: esta macro usa statement expressions (extensión GNU).
// Sin statement expressions no se puede lograr exactamente
// lo mismo en una macro.
```

```c
// Swap genérico:
#define SWAP(a, b) do {         \
    typeof(a) _tmp = (a);       \
    (a) = (b);                   \
    (b) = _tmp;                  \
} while (0)

int x = 5, y = 10;
SWAP(x, y);              // funciona con cualquier tipo

double a = 3.14, b = 2.71;
SWAP(a, b);              // funciona sin cambiar la macro
```

```c
// Container_of — obtener el struct a partir de un miembro:
// (usado extensivamente en el kernel de Linux)
#define container_of(ptr, type, member)         \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// Declarar un puntero al tipo correcto:
#define field_ptr(struct_ptr, field)             \
    &((typeof(*(struct_ptr)) *)(struct_ptr))->field
```

### Disponibilidad

```bash
# Como extensión GNU: GCC y Clang siempre lo soportaron
gcc -std=gnu11 main.c -o main   # typeof disponible

# Como estándar C23:
gcc -std=c23 main.c -o main     # typeof es estándar

# Con -std=c11 -Wpedantic:
# warning: ISO C does not support 'typeof'
# Porque en C11 no es estándar, es extensión GNU

# Alternativa en C11 para GCC/Clang:
__typeof__(x) y;                 # variante con __ siempre disponible
```

## Otras features de C23 notables

Estas features no se cubren en detalle pero vale la pena
conocer su existencia:

```c
// 1. nullptr (reemplaza NULL):
int *p = nullptr;
// nullptr tiene tipo nullptr_t — más seguro que (void*)0 o 0

// 2. bool, true, false como keywords:
bool flag = true;               // ya no necesita stdbool.h

// 3. Atributos estándar:
[[nodiscard]] int compute(void);
[[maybe_unused]] int x;
[[deprecated("usar new_func")]] void old_func(void);
[[noreturn]] void die(void);

// 4. Binary literals y digit separators:
int mask = 0b1111'0000;
int million = 1'000'000;

// 5. #embed — incluir archivos binarios:
const unsigned char font[] = {
    #embed "font.bin"
};

// 6. static_assert sin mensaje:
static_assert(sizeof(int) == 4);  // OK en C23 (C11 requería mensaje)

// 7. #elifdef / #elifndef:
#ifdef A
    // ...
#elifdef B       // más claro que #elif defined(B)
    // ...
#endif

// 8. auto para deducción de tipo:
auto x = 42;     // x es int (limitado comparado con C++)

// 9. Eliminaciones:
// - K&R function definitions eliminadas
// - Trigraphs eliminados
```

## Política del curso con C23

```
Cuando en el curso aparezca una feature de C23, se indicará así:

    "typeof es una extensión GNU que fue estandarizada en C23.
     En este curso (C11), usar __typeof__ con -std=gnu11
     o un approach alternativo."

Reglas:
1. El código del curso compila con -std=c11
2. Si una feature de C23 simplifica mucho algo, se muestra
   como alternativa con la nota de que requiere C23 o extensión GNU
3. Nunca se requiere C23 para completar los ejercicios

Soporte de C23 en compiladores (2024-2025):
- GCC 13+: soporte parcial con -std=c23
- Clang 17+: soporte parcial con -std=c23
- El soporte completo aún está en progreso
```

## Tabla resumen

| Feature | Antes de C23 | C23 | Extensión GNU |
|---|---|---|---|
| __VA_OPT__ | ##__VA_ARGS__ (extensión) | __VA_OPT__(,) | Sí (GCC 8+) |
| constexpr | #define o enum | constexpr int X = 10; | No |
| typeof | __typeof__ (extensión) | typeof(x) | Sí (siempre) |
| nullptr | NULL, (void*)0, 0 | nullptr | No |
| bool keyword | stdbool.h | keyword bool | No |
| Atributos [[]] | __attribute__(()) | [[nodiscard]], etc. | __attribute__ sí |
| Binary literals | No estándar | 0b1010 | Sí (GCC 4.3+) |
| Digit separators | No | 1'000'000 | No |

---

## Ejercicios

### Ejercicio 1 — __VA_OPT__

```c
// Crear una macro ASSERT(cond, ...) que:
// - Si solo recibe la condición: imprima "Assertion failed: cond"
// - Si recibe mensaje extra: imprima el mensaje
// Usar __VA_OPT__
// Compilar con -std=gnu11 o -std=c23

// ASSERT(x > 0);           → "Assertion failed: x > 0"
// ASSERT(x > 0, "x must be positive, got %d", x);
//                           → "x must be positive, got -5"
```

### Ejercicio 2 — typeof

```c
// Crear una macro ARRAY_SIZE(arr) que retorne el número
// de elementos de un array estático.
// Usar typeof para verificar que el argumento es realmente
// un array y no un puntero.
// Compilar con -std=gnu11

// int arr[10];
// ARRAY_SIZE(arr)   → 10
// int *p = arr;
// ARRAY_SIZE(p)     → error de compilación
```

### Ejercicio 3 — Alternativas en C11

```c
// Reescribir este código C23 para que compile en C11 estricto:

constexpr int SIZE = 10;
typeof(SIZE) arr[SIZE];

for (auto i = 0; i < SIZE; i++) {
    arr[i] = i * i;
}
```
