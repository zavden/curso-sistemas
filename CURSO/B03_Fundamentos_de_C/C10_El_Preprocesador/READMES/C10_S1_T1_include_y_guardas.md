# T01 — #include y guardas

> Sin erratas detectadas en el material fuente.

---

## 1. El preprocesador — Fase previa a la compilación

El preprocesador de C es un procesador de texto que transforma el código fuente **antes** de que el compilador lo vea. No entiende C — solo manipula texto.

```
Código fuente (.c / .h)
    │
    ▼
 Preprocesador (cpp)     ← #include, #define, #ifdef, etc.
    │
    ▼
 Unidad de traducción    ← Texto puro: sin directivas, todo expandido
    │
    ▼
 Compilador (cc1)        ← Genera código objeto (.o)
    │
    ▼
 Enlazador (ld)          ← Genera ejecutable
```

```bash
# Ver solo la salida del preprocesador (sin compilar):
gcc -E programa.c              # Salida completa (con marcadores de línea)
gcc -E -P programa.c           # Salida limpia (sin marcadores)
gcc -E -dM programa.c          # Solo macros definidas

# El preprocesador se puede invocar directamente:
cpp programa.c                 # Equivalente a gcc -E
```

Las directivas del preprocesador empiezan con `#`:
- `#include` — copiar archivo
- `#define` / `#undef` — definir/eliminar macros
- `#ifdef` / `#ifndef` / `#if` / `#else` / `#endif` — compilación condicional
- `#pragma` — instrucciones al compilador
- `#error` / `#warning` — mensajes en compilación

---

## 2. #include — Copia literal de archivos

`#include` **no importa** ni crea módulos — **copia literalmente** el contenido del archivo en el punto de la directiva.

```c
// Si greeting.h contiene:
//   void greet(const char *name);

// Y main.c contiene:
#include "greeting.h"
int main(void) { greet("World"); return 0; }

// Después del preprocesador, main.c se convierte en:
void greet(const char *name);
int main(void) { greet("World"); return 0; }

// Es SUSTITUCIÓN DE TEXTO. Nada más.
```

### Dos formas de incluir

```c
#include <stdio.h>        // Comillas angulares
#include "my_header.h"    // Comillas dobles
```

| Forma | Busca primero en | Luego en | Uso típico |
|-------|-----------------|----------|------------|
| `<header.h>` | Directorios del sistema + `-I` | — | Headers del sistema y bibliotecas |
| `"header.h"` | Directorio del archivo fuente | Luego igual que `<>` | Headers del proyecto |

```c
// Regla práctica:
// - <stdio.h>, <stdlib.h>, <string.h>  → del sistema, usar < >
// - "my_module.h", "config.h"           → del proyecto, usar " "
// - <SDL2/SDL.h>, <openssl/ssl.h>       → bibliotecas externas, usar < >
```

### Inclusión transitiva

```c
// a.h incluye b.h:
//   #include "b.h"
//   struct A { struct B data; };

// main.c incluye a.h:
//   #include "a.h"
//   // main.c tiene acceso a struct B aunque no incluyó b.h directamente.
//   // Esto es INCLUSIÓN TRANSITIVA.

// Problema: si main.c depende de struct B, y alguien quita el
// #include "b.h" de a.h, main.c deja de compilar.
// Regla: incluye explícitamente todo lo que USAS directamente.
```

---

## 3. Rutas de búsqueda — -I y organización de proyectos

### -I: agregar directorios de búsqueda

```bash
# Sin -I: el compilador solo busca en directorios del sistema
gcc main.c -o prog
# #include <mylib.h> → fatal error: mylib.h: No such file or directory

# Con -I: agrega un directorio a la ruta de búsqueda
gcc -Iinclude -Ilib/headers main.c -o prog
# #include <mylib.h> → busca en include/ y lib/headers/
```

### Convención de estructura de proyecto

```
proyecto/
├── include/           ← Headers públicos (API del proyecto)
│   ├── proyecto.h
│   └── utils.h
├── src/               ← Implementación (.c) + headers internos
│   ├── proyecto.c
│   ├── utils.c
│   └── internal.h     ← Header privado, no en include/
├── tests/
│   └── test_proyecto.c
└── Makefile

# Compilar:
gcc -Iinclude src/*.c -o proyecto
gcc -Iinclude tests/test_proyecto.c src/utils.c -o tests/run_tests
```

### Ver las rutas del compilador

```bash
# Mostrar todas las rutas de búsqueda:
gcc -E -v empty.c 2>&1 | grep -A 20 "search starts here"

# Salida típica:
#include "..." search starts here:
#include <...> search starts here:
 /usr/lib/gcc/x86_64-linux-gnu/13/include
 /usr/local/include
 /usr/include/x86_64-linux-gnu
 /usr/include
End of search list.
```

### -iquote: separar comillas de angulares

```bash
# -I afecta tanto <> como "".
# -iquote solo afecta "":
gcc -iquote src/internal -Iinclude main.c

# #include "config.h"  → busca en src/internal/ (y luego include/)
# #include <config.h>  → busca solo en include/ (no en src/internal/)
```

---

## 4. El problema de la inclusión múltiple

### Caso directo

```c
// point.h (SIN guardas):
struct Point { int x, y; };

// main.c:
#include "point.h"
#include "point.h"    // ← ERROR: redefinition of 'struct Point'

// Después del preprocesador:
struct Point { int x, y; };
struct Point { int x, y; };   // ← Duplicado
```

### Caso transitivo (el más común en la práctica)

```c
// common.h:
struct Config { int debug; };

// logger.h:
#include "common.h"
void log_init(struct Config *c);

// database.h:
#include "common.h"
void db_connect(struct Config *c);

// main.c:
#include "logger.h"      // incluye common.h
#include "database.h"    // incluye common.h OTRA VEZ → ERROR

// Después del preprocesador:
struct Config { int debug; };   // de logger.h → common.h
void log_init(struct Config *c);
struct Config { int debug; };   // de database.h → common.h → DUPLICADO
void db_connect(struct Config *c);
```

Este problema aparece inevitablemente en cualquier proyecto con más de un puñado de archivos. Las include guards son la solución estándar.

---

## 5. Include guards — #ifndef / #define / #endif

```c
// point.h con include guard:
#ifndef POINT_H          // Si POINT_H NO está definido...
#define POINT_H          // ...definirlo ahora

struct Point {
    int x;
    int y;
};

struct Point point_create(int x, int y);
void point_print(const struct Point *p);

#endif /* POINT_H */     // Fin del bloque condicional
```

### Cómo funciona paso a paso

```c
// Primera inclusión de point.h:
//   #ifndef POINT_H  → POINT_H no existe → VERDADERO → procesar contenido
//   #define POINT_H  → ahora POINT_H está definido (vacío, pero existe)
//   struct Point { ... };
//   prototipos...
//   #endif

// Segunda inclusión de point.h:
//   #ifndef POINT_H  → POINT_H YA existe → FALSO → saltar hasta #endif
//   (todo el contenido se ignora)
//   #endif
```

### Convenciones de nombres

```c
// El nombre de la macro DEBE ser único en todo el proyecto.
// Si dos headers usan el mismo nombre, uno será ignorado.

// Estilo simple (nombre del archivo):
#ifndef POINT_H
#define POINT_H

// Estilo con prefijo de proyecto (recomendado):
#ifndef MYPROJECT_POINT_H
#define MYPROJECT_POINT_H

// Estilo con path completo (Google C++ Style):
#ifndef SRC_UTILS_POINT_H_
#define SRC_UTILS_POINT_H_

// Estilo con UUID (máxima unicidad):
#ifndef POINT_H_A3F2B1C8_4F6A_B9C0_1234ABCD5678
#define POINT_H_A3F2B1C8_4F6A_B9C0_1234ABCD5678

// Reglas:
// - Solo mayúsculas y guiones bajos
// - Reemplazar . y / por _
// - NO usar nombres que empiecen con _ o __ (reservados por el estándar)
```

### Errores comunes con include guards

```c
// 1. Olvidar el #define:
#ifndef POINT_H
// #define POINT_H   ← ¡falta! Cada inclusión será procesada
struct Point { int x, y; };
#endif

// 2. Nombre duplicado entre headers:
// file_a.h:
#ifndef UTILS_H      // ← Mismo nombre que file_b.h
#define UTILS_H
struct A { int x; };
#endif

// file_b.h:
#ifndef UTILS_H      // ← COLISIÓN: si file_a.h fue incluido primero,
#define UTILS_H      //   UTILS_H ya existe y file_b.h se ignora
struct B { int y; }; //   completamente. struct B nunca se define.
#endif

// 3. Código fuera del guard:
#include <stdio.h>   // ← FUERA del guard → se incluye cada vez
#ifndef POINT_H
#define POINT_H
struct Point { int x, y; };
#endif
// stdio.h se re-incluye (pero stdio.h tiene sus propios guards, así que no falla)
// Aun así, es mala práctica poner código fuera del guard.
```

---

## 6. #pragma once — Alternativa simplificada

```c
// En vez de tres líneas de include guard:
#pragma once

struct Point {
    int x;
    int y;
};

struct Point point_create(int x, int y);
void point_print(const struct Point *p);
```

### Comparación detallada

| Aspecto | Include guards | `#pragma once` |
|---------|---------------|----------------|
| Estándar C | Sí (C89+) | No (extensión) |
| Soporte | Todos los compiladores | GCC, Clang, MSVC, ICC |
| Líneas de código | 3 | 1 |
| Riesgo de colisión de nombres | Sí (macros duplicadas) | No |
| Problemas con symlinks | No | Sí (*)  |
| Problemas con hard links | No | Sí (*) |
| Rendimiento | Puede reabrir archivo | Puede evitar reabrir |

(*) Si el mismo archivo tiene dos paths (por symlink o hard link), el compilador puede no detectar que es el mismo archivo y lo incluye dos veces.

### ¿Cuál usar?

```c
// Kernel Linux: include guards (C estricto, máxima portabilidad)
// Proyectos nuevos modernos: #pragma once (más simple, soporte universal)
// Bibliotecas open source: include guards (portabilidad garantizada)
// Código personal/interno: #pragma once (menos boilerplate)

// Ambos son válidos. Elige uno y sé consistente en el proyecto.
// Muchos proyectos usan AMBOS por seguridad:
#pragma once
#ifndef MYLIB_POINT_H
#define MYLIB_POINT_H
// ...
#endif
```

---

## 7. Qué poner y NO poner en un header

### SÍ poner en headers

```c
#ifndef MYLIB_H
#define MYLIB_H

#include <stddef.h>   // Headers necesarios para ESTE header

// 1. Definiciones de tipos:
struct Widget { int id; char name[64]; };
typedef struct Widget Widget;
enum Status { OK, ERROR, PENDING };

// 2. Prototipos de funciones (declaraciones):
Widget *widget_create(const char *name);
void widget_destroy(Widget *w);
enum Status widget_process(Widget *w);

// 3. Macros y constantes:
#define WIDGET_MAX_NAME 64
#define WIDGET_VERSION "2.0"

// 4. Variables externas (declaración, NO definición):
extern int widget_count;

// 5. Funciones static inline (pequeñas, en el header):
static inline int widget_is_valid(const Widget *w) {
    return w != NULL && w->id > 0;
}

#endif
```

### NO poner en headers

```c
// 1. Definiciones de funciones (excepto static inline):
int widget_process(Widget *w) {   // ← NO en el header
    return do_work(w);
}
// Si dos .c incluyen este header → "multiple definition" al enlazar

// 2. Definiciones de variables globales:
int widget_count = 0;   // ← NO en el header → múltiples definiciones
extern int widget_count; // ← SÍ: solo DECLARACIÓN

// 3. Variables static (excepto inline functions):
static int internal_counter = 0;  // ← Cada .c obtiene su propia copia
// Probablemente un error: parece global pero cada archivo tiene la suya

// 4. #include innecesarios:
#include <math.h>   // ← ¿El header realmente usa algo de math.h?
// Incluir de más ralentiza la compilación y crea dependencias falsas
```

### La regla: declaraciones sí, definiciones no

```c
// Declaración: "esto EXISTE en algún lugar"
//   → Puede aparecer múltiples veces sin error
extern int count;              // Variable existe en otro .c
void process(int x);           // Función existe en otro .c
struct Data;                   // Tipo existe (forward declaration)

// Definición: "esto ES esto"
//   → Solo puede aparecer UNA vez en todo el programa (ODR)
int count = 0;                 // Asigna memoria
void process(int x) { ... }   // Implementa la función
struct Data { int x; };       // Define el layout (excepción: puede repetirse
                               // si es idéntica, gracias al ODR de tipos)
```

---

## 8. Estructura de un header profesional

```c
/* ================================================================
 * mylib.h — API pública de MyLib
 * ================================================================ */

#ifndef MYLIB_H
#define MYLIB_H

/* ---- Includes necesarios para ESTE header ---- */
#include <stddef.h>     // size_t
#include <stdbool.h>    // bool

/* ---- Macros y constantes ---- */
#define MYLIB_VERSION_MAJOR 1
#define MYLIB_VERSION_MINOR 0
#define MYLIB_MAX_NAME 256

/* ---- Forward declarations (si necesario) ---- */
struct InternalState;    // Opaque type — definido en mylib.c

/* ---- Tipos públicos ---- */
typedef struct MyLib {
    char name[MYLIB_MAX_NAME];
    int id;
    struct InternalState *state;   // Puntero a tipo opaco
} MyLib;

typedef enum {
    MYLIB_OK = 0,
    MYLIB_ERR_NOMEM,
    MYLIB_ERR_INVALID,
} MyLibError;

/* ---- API pública ---- */
MyLib *mylib_create(const char *name);
void mylib_destroy(MyLib *lib);
MyLibError mylib_process(MyLib *lib, const void *data, size_t len);
const char *mylib_strerror(MyLibError err);

/* ---- Inline utilities ---- */
static inline bool mylib_is_valid(const MyLib *lib) {
    return lib != NULL && lib->id > 0;
}

#endif /* MYLIB_H */
```

### Principios

1. **Self-contained**: el header compila por sí solo (incluye sus dependencias)
2. **Minimal**: no incluye más de lo necesario
3. **Documentado**: los comentarios explican la API, no la implementación
4. **Prefijado**: todos los símbolos públicos llevan el prefijo del proyecto (`mylib_`)
5. **Guarded**: include guard o `#pragma once`

---

## 9. Forward declarations — Minimizar includes

Una **forward declaration** dice al compilador que un tipo existe, sin definir su contenido. Solo se necesita la definición completa cuando se accede a los campos o se usa `sizeof`.

```c
// database.h necesita usar struct Logger pero solo como puntero:

// OPCIÓN A — #include (crea dependencia):
#include "logger.h"
void db_set_logger(struct Logger *log);

// OPCIÓN B — Forward declaration (sin dependencia):
struct Logger;    // "struct Logger existe, no sé su contenido"
void db_set_logger(struct Logger *log);

// OPCIÓN B es MEJOR:
// - Compilación más rápida (no procesa logger.h)
// - Menos acoplamiento (cambios en logger.h no recompilan database.h)
// - Solo funciona con PUNTEROS (el compilador sabe el tamaño de un puntero
//   sin conocer el tipo completo)
```

### Cuándo funciona y cuándo no

```c
struct Foo;   // Forward declaration

// FUNCIONA — solo necesita saber que Foo es un tipo:
void process(struct Foo *f);          // Puntero como parámetro
struct Foo *create(void);             // Puntero como retorno
struct Bar { struct Foo *ref; };      // Puntero como campo

// NO FUNCIONA — necesita la definición completa:
struct Bar { struct Foo data; };      // Campo por valor → necesita sizeof
void use(struct Foo f);               // Parámetro por valor → necesita sizeof
sizeof(struct Foo);                   // Necesita conocer el layout
f->field;                             // Acceso a campos → necesita definición
```

### Impacto en tiempos de compilación

```
// Con includes transitivos:
// Cambiar logger.h → recompila logger.c, database.c, server.c, main.c
//                     (todos incluyen logger.h directa o indirectamente)

// Con forward declarations:
// Cambiar logger.h → recompila solo logger.c y los .c que realmente
//                     #include "logger.h" (no los que solo usan punteros)

// En proyectos grandes (miles de archivos), esto reduce los tiempos
// de recompilación de minutos a segundos.
```

---

## 10. Comparación con Rust

| Concepto C | Rust | Diferencia |
|-----------|------|-----------|
| `#include "file.h"` | `mod module;` + `use module::Type;` | Rust: módulos con namespaces, no copia de texto |
| Include guards | No necesario | Cada módulo se procesa exactamente una vez |
| `-I` para paths | `mod.rs` / directorio | La estructura de archivos ES la estructura de módulos |
| Forward declaration | No necesario | El compilador resuelve orden automáticamente |
| Header (.h) + implementación (.c) | Un solo archivo .rs | Todo en un archivo, `pub` controla visibilidad |
| `extern int count;` | `pub static COUNT: i32` | Visibilidad explícita con `pub` |
| `static inline` en header | `#[inline]` en cualquier lugar | El compilador decide inlining automáticamente |

```rust
// En C: punto.h + punto.c + include guards + #include
// En Rust: punto.rs con pub

// punto.rs
pub struct Point {
    pub x: i32,    // pub = visible fuera del módulo
    pub y: i32,
}

impl Point {
    pub fn new(x: i32, y: i32) -> Self {
        Point { x, y }
    }

    pub fn print(&self) {
        println!("Point({}, {})", self.x, self.y);
    }
}

// main.rs
mod punto;                       // "importar" el módulo
use punto::Point;                // traer Point al scope

fn main() {
    let p = Point::new(3, 7);
    p.print();
}

// No hay:
// - Include guards (imposible incluir dos veces)
// - Copia de texto (los módulos se compilan una vez)
// - Problema de inclusión transitiva
// - Separación header/implementación (todo en un archivo)
// - Forward declarations (el compilador resuelve el grafo de dependencias)
```

**La razón fundamental**: C fue diseñado en 1972 cuando la memoria era limitada y los compiladores eran simples. El preprocesador era la solución más pragmática para reutilizar código. Lenguajes modernos como Rust usan sistemas de módulos que resuelven estos problemas a nivel del compilador, no del procesador de texto.

---

## Ejercicios

### Ejercicio 1 — Include guard vs sin guard

```c
// Crear dos versiones de un header vector.h:
//   a) vector_noguard.h (sin guardas)
//   b) vector.h (con include guard)
//
// Ambos definen: struct Vec2 { float x, y; };
//
// Crear main.c que incluya cada uno dos veces.
// Compilar ambos. Verificar:
//   - vector_noguard.h: error de redefinición
//   - vector.h: compila sin errores
//
// Usar gcc -E -P main.c | grep "struct Vec2" para ver
// cuántas veces aparece la definición en cada caso.
```

<details><summary>Predicción</summary>

**Sin guard**: `gcc -E -P` mostrará `struct Vec2 { float x, y; };` dos veces. El compilador dará error: `redefinition of 'struct Vec2'`.

**Con guard**: `gcc -E -P` mostrará `struct Vec2 { float x, y; };` **una sola vez**. La segunda inclusión es ignorada por `#ifndef`. Compila sin errores.

El `grep` confirma: sin guard → 2 coincidencias. Con guard → 1 coincidencia.

</details>

### Ejercicio 2 — Inclusión transitiva

```c
// Crear tres headers:
//   color.h   → struct Color { uint8_t r, g, b; };
//   pixel.h   → #include "color.h"
//               struct Pixel { struct Color color; int x, y; };
//   canvas.h  → #include "pixel.h"
//               struct Canvas { struct Pixel *pixels; int w, h; };
//
// Crear main.c que incluya los tres:
//   #include "color.h"
//   #include "pixel.h"
//   #include "canvas.h"
//
// Sin guardas: ¿cuántas veces se incluye color.h?
// Con guardas: ¿compila? Verificar con gcc -E.
```

<details><summary>Predicción</summary>

Sin guardas, color.h se incluye **3 veces**:
1. Directamente por main.c
2. A través de pixel.h (incluido por main.c)
3. A través de canvas.h → pixel.h → color.h

Con guardas en los tres headers: compila sin errores. La primera inclusión de color.h define la macro guard. Las siguientes dos inclusiones encuentran la macro ya definida y saltan el contenido.

`gcc -E -P main.c | grep "struct Color"` mostrará: sin guardas → 3 veces. Con guardas → 1 vez.

</details>

### Ejercicio 3 — #pragma once vs include guards

```c
// Crear el mismo header en dos versiones:
//   a) config_guard.h con include guard
//   b) config_pragma.h con #pragma once
//
// Ambos definen: struct Config { int port; char host[64]; };
//
// Crear main.c que incluya cada uno dos veces.
// Compilar con: gcc -std=c11 -Wall -Wextra -Wpedantic
//
// ¿Alguno genera warnings con -Wpedantic?
// ¿Ambos compilan correctamente?
```

<details><summary>Predicción</summary>

Ambos compilarán sin errores. `#pragma once` NO genera warning con `-Wpedantic` en GCC moderno, porque el estándar dice que pragmas desconocidos se ignoran (y GCC reconoce `once`).

En compiladores antiguos o estrictos, `#pragma once` podría generar un warning. Pero en GCC y Clang actuales, es silencioso.

El comportamiento es idéntico: la doble inclusión se previene en ambos casos.

</details>

### Ejercicio 4 — Ruta de búsqueda con -I

```c
// Crear esta estructura:
//   myproject/
//   ├── include/
//   │   └── math_utils.h    (declaración de add, multiply)
//   ├── src/
//   │   └── math_utils.c    (implementación)
//   └── main.c              (usa #include <math_utils.h>)
//
// a) Compilar sin -I: debe fallar
// b) Compilar con -Iinclude: debe funcionar
// c) Cambiar <math_utils.h> por "math_utils.h": ¿funciona sin -I?
// d) Ejecutar: gcc -E -v main.c -Iinclude 2>&1 | grep "search"
//    ¿Dónde aparece "include" en la lista?
```

<details><summary>Predicción</summary>

a) Sin `-I`: `fatal error: math_utils.h: No such file or directory`. Las comillas angulares solo buscan en directorios del sistema.

b) Con `-Iinclude`: compila correctamente. `include/` se agrega a la ruta de búsqueda.

c) Con `"math_utils.h"` sin `-I`: **depende** de dónde está main.c. Las comillas dobles buscan primero en el directorio del archivo fuente (myproject/). Como math_utils.h está en myproject/include/, NO lo encontrará. Seguirá fallando. Necesitaría `"include/math_utils.h"` o `-I`.

d) `include` aparecerá al principio de la lista de `#include <...> search starts here:`, antes de los directorios del sistema.

</details>

### Ejercicio 5 — gcc -E: inspeccionar la expansión

```c
// Crear un archivo test.c con:
//   #include <stdio.h>
//   int main(void) { printf("hello\n"); return 0; }
//
// Ejecutar:
//   gcc -E test.c | wc -l       → ¿cuántas líneas?
//   gcc -E -P test.c | wc -l    → ¿cuántas sin marcadores?
//
// Ahora agregar #include <stdlib.h> y repetir.
//   ¿Cuántas líneas más agrega stdlib.h?
//   ¿Por qué NO duplica el doble (stdio.h tiene guardas)?
//
// Finalmente:
//   echo 'int main(void){return 0;}' | gcc -E -P -x c - | wc -l
//   ¿Cuántas líneas sin ningún #include?
```

<details><summary>Predicción</summary>

Solo con stdio.h: ~800-900 líneas (depende de la versión de glibc). Con `-P`: ~300-400 líneas.

Agregando stdlib.h: las líneas totales aumentan, pero **no se duplican** las partes comunes. stdio.h y stdlib.h comparten headers internos (features.h, types.h, etc.). Los include guards de esos headers compartidos evitan la duplicación.

El incremento será ~100-200 líneas extra (solo lo que stdlib.h agrega que stdio.h no incluía).

Sin ningún include: **1 línea** (solo tu código). Esto demuestra que toda la "hinchazón" viene de los `#include`.

</details>

### Ejercicio 6 — Header self-contained

```c
// Verificar si un header es self-contained (compila solo):
//
// Crear point.h con:
//   struct Point { int x, y; };
//   void point_print(const struct Point *p);   // usa printf internamente
//
// Crear test_header.c con SOLO:
//   #include "point.h"
//
// Compilar: gcc -std=c11 -fsyntax-only test_header.c
// ¿Compila? ¿Necesita point.h incluir algo?
//
// Ahora agregar a point.h:
//   static inline const char *point_str(struct Point p, char *buf, size_t n) {
//       snprintf(buf, n, "(%d,%d)", p.x, p.y);
//       return buf;
//   }
// ¿Compila ahora? ¿Qué falta?
```

<details><summary>Predicción</summary>

Primera versión: compila correctamente. `struct Point` y el prototipo `void point_print(...)` no necesitan headers externos — son declaraciones puras de C.

Segunda versión: **falla**. `point_str` usa `snprintf` (de `<stdio.h>`) y `size_t` (de `<stddef.h>`). Sin incluir estos headers, el compilador dará errores:
- `error: unknown type name 'size_t'`
- `error: implicit declaration of function 'snprintf'`

Solución: agregar `#include <stdio.h>` (que incluye `size_t` indirectamente) o `#include <stddef.h>` + `<stdio.h>` dentro de point.h, dentro del include guard.

Un header self-contained incluye TODO lo que necesita para compilar solo.

</details>

### Ejercicio 7 — Forward declaration vs #include

```c
// Crear:
//   logger.h  → struct Logger { FILE *output; int level; };
//                void logger_log(struct Logger *l, const char *msg);
//
//   server.h  → Necesita usar struct Logger como parámetro
//
// Implementar server.h de DOS formas:
//   a) Con #include "logger.h"
//   b) Con forward declaration: struct Logger;
//
// Para cada versión, contar líneas con gcc -E -P.
// ¿Cuál genera menos líneas? ¿Cuál recompila menos al cambiar logger.h?
//
// En server.c, ¿cuál versión necesitas usar? ¿Por qué?
```

<details><summary>Predicción</summary>

**Versión a (con #include)**: `gcc -E -P server.h` incluye todo el contenido de logger.h (y sus includes, como `<stdio.h>` para `FILE *`). Muchas líneas.

**Versión b (con forward declaration)**: `gcc -E -P server.h` solo tiene unas pocas líneas. No procesa logger.h ni stdio.h.

La versión b genera **mucho menos** líneas preprocessadas. Si logger.h cambia, los archivos que usan la forward declaration no necesitan recompilarse.

**Pero en server.c**: si server.c necesita acceder a campos de `struct Logger` (ej: `l->level`), DEBE usar `#include "logger.h"` para conocer el layout completo. Si solo pasa punteros sin acceder a campos, la forward declaration basta.

El header usa forward declaration (para minimizar dependencias). El .c usa #include (para acceder a campos).

</details>

### Ejercicio 8 — Colisión de nombres de guard

```c
// Crear DOS headers diferentes que usen la misma macro de guard:
//
//   utils_a.h:
//     #ifndef UTILS_H
//     #define UTILS_H
//     int utility_a(void);
//     #endif
//
//   utils_b.h:
//     #ifndef UTILS_H    ← ¡MISMA macro!
//     #define UTILS_H
//     int utility_b(void);
//     #endif
//
// En main.c:
//   #include "utils_a.h"
//   #include "utils_b.h"
//
// ¿Qué funciones están disponibles? ¿utility_a, utility_b, o ambas?
// Verificar con gcc -E -P.
```

<details><summary>Predicción</summary>

Solo `utility_a` estará disponible.

Flujo:
1. `#include "utils_a.h"`: `UTILS_H` no definida → procesar → define `UTILS_H` → declara `utility_a()`
2. `#include "utils_b.h"`: `#ifndef UTILS_H` → `UTILS_H` YA definida → **FALSO** → todo el contenido se salta

`utility_b()` nunca se declara. Si main.c intenta usarla, obtendrá `implicit declaration of function 'utility_b'`.

Este es exactamente el peligro de nombres de guard genéricos. Solución: usar nombres únicos como `UTILS_A_H` y `UTILS_B_H`, o mejor `MYPROJECT_UTILS_A_H`.

`#pragma once` no tiene este problema porque no usa macros con nombres.

</details>

### Ejercicio 9 — Header con extern y variable global

```c
// Crear counter.h:
//   extern int global_counter;      // Declaración
//   void counter_increment(void);
//   int counter_get(void);
//
// Crear counter.c:
//   int global_counter = 0;         // Definición (UNA sola vez)
//   void counter_increment(void) { global_counter++; }
//   int counter_get(void) { return global_counter; }
//
// Crear main.c y helper.c que ambos incluyan counter.h.
// main.c llama counter_increment() 3 veces.
// helper.c llama counter_increment() 2 veces.
// Al final, counter_get() debería retornar 5.
//
// ¿Qué pasa si pones "int global_counter = 0;" en el header?
```

<details><summary>Predicción</summary>

Con `extern int global_counter;` en el header: funciona correctamente. `global_counter` se define UNA vez en counter.c. main.c y helper.c comparten la misma variable. Resultado: 5.

Con `int global_counter = 0;` en el header: **error de enlazado** `multiple definition of 'global_counter'`. Cada .c que incluye el header crea su propia definición de la variable, y el enlazador encuentra duplicados.

La distinción `extern` (declaración) vs sin `extern` (definición) es crucial para variables globales en headers.

Nota: GCC con `-fcommon` (por defecto en GCC < 10) permitía múltiples definiciones tentativas (sin inicializador). GCC 10+ usa `-fno-common` por defecto y rechaza esta práctica.

</details>

### Ejercicio 10 — Proyecto multi-archivo completo

```c
// Crear un mini-proyecto con esta estructura:
//
//   include/
//   ├── stack.h         (struct Stack opaco, API: create/destroy/push/pop/size)
//   └── utils.h         (MIN, MAX macros, swap inline)
//   src/
//   ├── stack.c          (implementación con array dinámico)
//   └── main.c           (programa de prueba)
//
// Requisitos:
// 1. Todos los headers con include guards
// 2. stack.h usa forward declaration (struct Stack opaco)
// 3. utils.h es self-contained
// 4. Compilar con: gcc -Iinclude src/*.c -o programa
//
// Verificar:
//   gcc -E -P -Iinclude src/main.c | wc -l  → ¿cuántas líneas?
//   gcc -fsyntax-only -Iinclude include/stack.h  → ¿compila solo?
//   gcc -fsyntax-only -Iinclude include/utils.h  → ¿compila solo?
```

<details><summary>Predicción</summary>

El proyecto debería compilar sin errores ni warnings con `-Wall -Wextra -Wpedantic`.

Estructura del tipo opaco:
- stack.h declara `typedef struct Stack Stack;` (forward declaration)
- stack.h declara la API con punteros a Stack
- stack.c define `struct Stack { int *data; int top; int capacity; };`
- main.c no puede acceder a los campos de Stack directamente

`gcc -E -P` de main.c generará muchas líneas (por stdio.h, stdlib.h incluidos en stack.c), pero la expansión es controlada gracias a los include guards.

`gcc -fsyntax-only` verifica que cada header es self-contained:
- stack.h debe incluir `<stddef.h>` si usa `size_t`
- utils.h no necesita includes si solo define macros e inline functions sin dependencias externas

Si algún header no compila solo, falta un `#include` interno.

</details>
