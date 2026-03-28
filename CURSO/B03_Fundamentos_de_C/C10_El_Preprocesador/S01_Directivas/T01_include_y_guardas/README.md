# T01 — #include y guardas

## #include — Incluir archivos

`#include` es una directiva del preprocesador que **copia**
el contenido de un archivo en el punto donde aparece:

```c
// Dos formas:

#include <stdio.h>       // busca en directorios del sistema
#include "my_header.h"   // busca primero en el directorio actual

// El preprocesador literalmente copia el contenido del archivo.
// Es sustitución de texto — no hay "importar" ni "módulos".
```

```c
// Rutas de búsqueda:

// <header.h> busca en:
// 1. Directorios del sistema (/usr/include, /usr/local/include, etc.)
// 2. Directorios agregados con -I
// No busca en el directorio actual.

// "header.h" busca en:
// 1. Directorio del archivo que tiene el #include
// 2. Directorios agregados con -iquote
// 3. Todos los directorios de <header.h>

// Ver las rutas de búsqueda:
// gcc -E -v empty.c 2>&1 | grep "search starts here" -A 20
```

```c
// -I agrega directorios de búsqueda:
// gcc -I./include -I../lib/include prog.c
//
// #include <mylib.h>  → busca en ./include/ y ../lib/include/
//
// Convención de proyectos:
// proyecto/
// ├── include/        ← headers públicos
// │   └── mylib.h
// ├── src/            ← implementación
// │   └── mylib.c
// └── main.c
//
// gcc -Iinclude src/mylib.c main.c -o program
```

## El problema de la inclusión múltiple

```c
// Si un header se incluye dos veces, todo se duplica:

// point.h:
struct Point { int x, y; };

// main.c:
#include "point.h"
#include "point.h"    // ERROR: redefinition of 'struct Point'

// Más realista — inclusión transitiva:
// a.h: #include "point.h"
// b.h: #include "point.h"
// main.c:
#include "a.h"    // incluye point.h
#include "b.h"    // incluye point.h OTRA VEZ → error
```

## Include guards

Las guardas de inclusión impiden que un header se procese
más de una vez:

```c
// point.h con include guard:
#ifndef POINT_H
#define POINT_H

struct Point {
    int x;
    int y;
};

void point_print(const struct Point *p);

#endif // POINT_H
```

```c
// Cómo funciona:
//
// Primera inclusión de point.h:
//   #ifndef POINT_H → POINT_H no está definido → TRUE → procesar
//   #define POINT_H → ahora POINT_H está definido
//   (contenido del header)
//   #endif
//
// Segunda inclusión de point.h:
//   #ifndef POINT_H → POINT_H YA está definido → FALSE → saltar todo
//   (nada se incluye)

// Convención de nombres para la macro:
// NOMBRE_DEL_ARCHIVO_H
// PROJECT_NOMBRE_H
// Usar mayúsculas, reemplazar . y / por _
```

```c
// Convenciones de nombres de guards:

// Simple:
#ifndef POINT_H
#define POINT_H

// Con prefijo de proyecto:
#ifndef MYPROJECT_POINT_H
#define MYPROJECT_POINT_H

// Con path completo:
#ifndef SRC_UTILS_POINT_H_
#define SRC_UTILS_POINT_H_

// UUID (garantiza unicidad):
#ifndef POINT_H_5A3F2B1C_8D7E_4F6A_B9C0_1234ABCD5678
#define POINT_H_5A3F2B1C_8D7E_4F6A_B9C0_1234ABCD5678

// Lo importante: que sea ÚNICO en todo el proyecto.
```

## #pragma once

```c
// Alternativa no estándar (pero soportada por GCC, Clang, MSVC):
#pragma once

struct Point {
    int x;
    int y;
};

void point_print(const struct Point *p);

// Ventajas sobre include guards:
// - Más simple — una línea
// - Sin riesgo de colisión de nombres de macro
// - Puede ser más rápido (el compilador puede evitar abrir el archivo)
//
// Desventajas:
// - No es estándar C (pero funciona en todos los compiladores modernos)
// - Problemas con symlinks y hard links (el mismo archivo
//   con dos paths se incluye dos veces)
//
// En la práctica: ambos son aceptables.
// Muchos proyectos usan #pragma once por simplicidad.
// El kernel de Linux usa include guards (es C estricto).
```

## Qué poner en un header

```c
// --- point.h ---
#ifndef POINT_H
#define POINT_H

// SÍ poner en headers:
// 1. Declaraciones de tipos:
struct Point { int x, y; };
typedef struct Point Point;
enum Color { RED, GREEN, BLUE };

// 2. Prototipos de funciones:
Point point_create(int x, int y);
void point_print(const Point *p);

// 3. Macros:
#define PI 3.14159265358979
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// 4. extern para variables globales (declaración, no definición):
extern int global_count;

// 5. static inline para funciones pequeñas:
static inline int point_manhattan(Point p) {
    return abs(p.x) + abs(p.y);
}

// 6. Otros #includes necesarios:
#include <stddef.h>    // para size_t usado en prototipos

#endif // POINT_H
```

```c
// NO poner en headers:
// 1. Definiciones de funciones (excepto static inline):
//    Causaría "multiple definition" al enlazar.

// 2. Definiciones de variables globales:
//    int count = 0;    // en el header → múltiples definiciones
//    extern int count; // correcto — solo declaración

// 3. Variables static no-inline:
//    static int x = 5; // cada .c que incluye el header tiene su propia copia
//    Puede ser intencional, pero generalmente es un error.
```

## Estructura de un header

```c
// Patrón completo para un header profesional:

#ifndef MYLIB_H
#define MYLIB_H

// Includes necesarios para ESTE header:
#include <stddef.h>
#include <stdbool.h>

// Macros y constantes:
#define MYLIB_VERSION "1.0.0"
#define MYLIB_MAX_NAME 256

// Forward declarations si son necesarias:
struct InternalData;

// Tipos públicos:
typedef struct MyLib {
    char name[MYLIB_MAX_NAME];
    int id;
} MyLib;

// API pública:
MyLib *mylib_create(const char *name);
void mylib_destroy(MyLib *lib);
bool mylib_process(MyLib *lib, const void *data, size_t len);
const char *mylib_error(void);

// Inline utilities:
static inline bool mylib_is_valid(const MyLib *lib) {
    return lib != NULL && lib->id > 0;
}

#endif // MYLIB_H
```

## Forward includes vs minimal includes

```c
// Incluir solo lo NECESARIO — no incluir de más:

// MAL — incluye todo:
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
// ¿Realmente necesita todas?

// BIEN — solo lo necesario:
// En el header: solo lo que necesitan los tipos/prototipos.
// En el .c: todo lo que necesita la implementación.

// Si el header solo usa punteros a un tipo, un forward
// declaration es suficiente (no necesita incluir el header):
struct Database;    // forward declaration — no #include "database.h"
void process(struct Database *db);
// Solo cuando se necesita el tamaño (sizeof, acceso a campos)
// se necesita la definición completa → #include.
```

---

## Ejercicios

### Ejercicio 1 — Header con include guard

```c
// Crear point.h con include guard y point.c:
// - struct Point { int x, y; }
// - Point point_create(int x, int y)
// - void point_print(const Point *p)
// - static inline double point_distance(Point a, Point b)
// Crear main.c que incluya point.h DOS veces (directa e indirectamente).
// Verificar que compila sin errores.
```

### Ejercicio 2 — Organización de proyecto

```c
// Crear un proyecto con esta estructura:
// proyecto/
// ├── include/
// │   ├── vec.h      ← struct Vec3, prototipos
// │   └── matrix.h   ← struct Mat3, prototipos (usa Vec3)
// ├── src/
// │   ├── vec.c
// │   └── matrix.c
// └── main.c
// Compilar con: gcc -Iinclude src/*.c main.c -o program
// Verificar que include guards funcionan con inclusiones cruzadas.
```

### Ejercicio 3 — Verificar preprocessor

```bash
# Usar gcc -E para ver la salida del preprocesador:
# 1. Crear un archivo con varios #include
# 2. gcc -E main.c | wc -l → ¿cuántas líneas genera?
# 3. gcc -E -P main.c → sin marcadores de línea
# 4. ¿Cuántas líneas agrega #include <stdio.h>?
# 5. ¿Y #include <stdlib.h> si ya incluiste stdio.h?
```
