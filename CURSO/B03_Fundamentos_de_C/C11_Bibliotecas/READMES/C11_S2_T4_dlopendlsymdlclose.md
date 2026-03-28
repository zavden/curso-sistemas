# T04 — dlopen, dlsym, dlclose

> **Fuentes**: `README.md`, `LABS.md`, `labs/` (8 archivos), `lab_codex/` (6 archivos + `plugins_src/`).
> Regla aplicada: **R3** (sin `.max.md`).

## Erratas detectadas

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| `README.md:89` | `double (*cosine)(double) = dlsym(handle, "cos");` — asignación directa de `void *` a puntero a función. ISO C prohíbe esta conversión; `-Wpedantic` emite warning. | Usar el workaround POSIX con `memcpy`: `void *sym = dlsym(handle, "cos"); memcpy(&cosine, &sym, sizeof(cosine));` — como hace correctamente el código del lab (`dlopen_basic.c:24-25`). |
| `README.md:337-351` | Patrón `goto fail` con `dlerror()`: la línea `if (dlerror()) goto fail;` consume el error, y luego en `fail:` se llama `dlerror()` de nuevo, que retorna `NULL` (ya consumido). El `fprintf` imprimiría `"Error: (null)"`. | Guardar el resultado de `dlerror()` en una variable antes del `goto`: `char *err = dlerror(); if (err) { /* guardar err */ goto fail; }`, y usar la variable guardada en el bloque `fail`. |

---

## Tabla de contenidos

1. [Carga dinámica en runtime vs enlace en compilación](#1--carga-dinámica-en-runtime-vs-enlace-en-compilación)
2. [dlopen — Cargar una biblioteca](#2--dlopen--cargar-una-biblioteca)
3. [dlsym — Obtener un símbolo](#3--dlsym--obtener-un-símbolo)
4. [dlerror — Diagnóstico de errores](#4--dlerror--diagnóstico-de-errores)
5. [dlclose — Descargar una biblioteca](#5--dlclose--descargar-una-biblioteca)
6. [El problema void* ↔ function pointer en ISO C](#6--el-problema-void--function-pointer-en-iso-c)
7. [Sistema de plugins](#7--sistema-de-plugins)
8. [RTLD_DEFAULT y RTLD_NEXT](#8--rtld_default-y-rtld_next)
9. [Patrones de error robusto](#9--patrones-de-error-robusto)
10. [Comparación con otros lenguajes](#10--comparación-con-otros-lenguajes)

---

## 1 — Carga dinámica en runtime vs enlace en compilación

Hasta ahora, las bibliotecas se declaraban en compilación con `-lfoo`. El linker resolvía los símbolos y el cargador dinámico (`ld-linux.so`) las cargaba al iniciar el programa. Con `dlopen`, un programa puede cargar bibliotecas **en runtime**, sin saber en compilación cuáles serán:

```
Enlace en compilación (-lfoo):
  gcc main.c -lfoo -o program
  → El linker verifica que los símbolos existen
  → ld-linux.so carga libfoo.so al arrancar
  → Si libfoo.so no existe → el programa no arranca

Carga dinámica (dlopen):
  gcc main.c -ldl -o program
  → El linker NO verifica los símbolos del plugin
  → El programa arranca sin los plugins
  → En runtime: dlopen("plugin.so") → carga bajo demanda
  → Si plugin.so no existe → el programa maneja el error
```

La API está en `<dlfcn.h>` y se enlaza con `-ldl`:

```c
#include <dlfcn.h>    // dlopen, dlsym, dlclose, dlerror

// Flujo básico:
// 1. dlopen  — cargar un .so en memoria
// 2. dlsym   — obtener puntero a función o variable por nombre
// 3. Usar el puntero como función normal
// 4. dlclose — descargar el .so

// Compilar con: gcc main.c -ldl -o program
```

**Nota sobre `-ldl`**: en glibc 2.34+ (Fedora 35+, Ubuntu 22.04+), las funciones `dlopen`/`dlsym`/`dlclose`/`dlerror` fueron integradas directamente en `libc.so.6`. El flag `-ldl` sigue funcionando (enlaza un stub vacío) pero no es estrictamente necesario. Inclúyelo por portabilidad con glibc más antiguas y otras implementaciones (musl, etc.).

### Casos de uso típicos

```
Plugins:        Editor/navegador carga módulos .so de un directorio
Driver loading: Kernel modules, drivers de hardware
Hot reload:     Recargar código sin reiniciar el programa
Carga opcional: Si libfoo.so existe → funcionalidad extra; si no → modo básico
Testing:        Inyectar mocks en runtime con LD_PRELOAD + RTLD_NEXT
```

---

## 2 — dlopen — Cargar una biblioteca

```c
#include <dlfcn.h>

void *handle = dlopen(const char *filename, int flags);
// Retorna: handle opaco (void *) si éxito, NULL si falla.
// filename: ruta al .so (absoluta, relativa, o nombre para búsqueda en rutas estándar)
// flags: RTLD_LAZY, RTLD_NOW, combinados con RTLD_GLOBAL, RTLD_LOCAL, etc.
```

### Ejemplo básico: cargar libm en runtime

```c
#include <dlfcn.h>
#include <stdio.h>

int main(void) {
    void *handle = dlopen("libm.so.6", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return 1;
    }
    printf("libm cargada exitosamente\n");

    // ... usar la biblioteca ...

    dlclose(handle);
    return 0;
}
```

### Flags de dlopen

```c
// === Resolución de símbolos (mutuamente excluyentes) ===

// RTLD_LAZY — resolver símbolos la primera vez que se usan:
void *h = dlopen("libfoo.so", RTLD_LAZY);
// Más rápido para cargar.
// Si falta un símbolo, el error ocurre cuando se llama esa función.

// RTLD_NOW — resolver TODOS los símbolos inmediatamente:
void *h = dlopen("libfoo.so", RTLD_NOW);
// Si falta algún símbolo, dlopen retorna NULL.
// Más seguro: detecta errores antes de usar la biblioteca.
// Recomendado para plugins.

// === Visibilidad (combinables con | ) ===

// RTLD_LOCAL (default) — símbolos solo accesibles vía este handle:
void *h = dlopen("libfoo.so", RTLD_NOW | RTLD_LOCAL);

// RTLD_GLOBAL — símbolos visibles globalmente (para otros .so cargados después):
void *h = dlopen("libfoo.so", RTLD_NOW | RTLD_GLOBAL);
// Necesario si libfoo.so exporta símbolos que otro .so cargado después necesita.

// === Otros flags ===

// RTLD_NODELETE — no descargar al hacer dlclose (la memoria se mantiene):
void *h = dlopen("libfoo.so", RTLD_NOW | RTLD_NODELETE);

// RTLD_NOLOAD — solo verificar si ya está cargada, sin cargarla:
void *h = dlopen("libfoo.so", RTLD_NOLOAD);
// Retorna handle si ya está cargada, NULL si no.
```

### Búsqueda del archivo

```
Si filename contiene '/':
  → Se usa como ruta directa (absoluta o relativa)
  → dlopen("./plugins/foo.so", ...)
  → dlopen("/usr/lib64/libm.so.6", ...)

Si filename NO contiene '/':
  → Búsqueda en este orden:
    1. LD_LIBRARY_PATH
    2. Rutas en /etc/ld.so.cache (de ldconfig)
    3. /lib, /usr/lib (y equivalentes de 64 bits)
  → dlopen("libm.so.6", ...)  ← busca en rutas estándar
```

### Contador de referencias

```c
// dlopen incrementa un contador. Se necesitan tantos dlclose como dlopen:
void *h1 = dlopen("libfoo.so", RTLD_NOW);  // refcount = 1
void *h2 = dlopen("libfoo.so", RTLD_NOW);  // refcount = 2 (misma instancia)
dlclose(h1);  // refcount = 1 — NO se descarga aún
dlclose(h2);  // refcount = 0 — ahora sí se descarga
```

---

## 3 — dlsym — Obtener un símbolo

```c
void *dlsym(void *handle, const char *symbol);
// Retorna: dirección del símbolo (void *), o NULL si no se encontró.
// handle: obtenido de dlopen (o RTLD_DEFAULT / RTLD_NEXT)
// symbol: nombre del símbolo como string (nombre de la función o variable)
```

### Uso con memcpy (workaround POSIX)

ISO C prohíbe convertir `void *` a puntero a función (ver sección 6). El workaround estándar usa `memcpy`:

```c
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    void *handle = dlopen("libm.so.6", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        return 1;
    }

    // Limpiar errores previos:
    dlerror();

    // Obtener puntero a cos():
    double (*cosine)(double);
    void *sym = dlsym(handle, "cos");
    memcpy(&cosine, &sym, sizeof(cosine));

    // Verificar error:
    char *error = dlerror();
    if (error) {
        fprintf(stderr, "dlsym: %s\n", error);
        dlclose(handle);
        return 1;
    }

    // Usar la función:
    printf("cos(0.0) = %f\n", cosine(0.0));     // 1.000000
    printf("cos(3.14159) = %f\n", cosine(3.14159)); // -1.000000

    dlclose(handle);
    return 0;
}
```

### Verificación segura de dlsym

`dlsym` puede retornar `NULL` legítimamente (un símbolo cuyo valor es 0). Por eso no basta con comprobar `if (!sym)`. El patrón seguro usa `dlerror()`:

```c
dlerror();                               // 1. Limpiar
void *sym = dlsym(handle, "func_name");  // 2. Buscar
char *err = dlerror();                   // 3. Verificar
if (err) {
    // Error real: el símbolo no existe
    fprintf(stderr, "dlsym: %s\n", err);
} else if (!sym) {
    // El símbolo existe pero vale NULL/0 (raro pero válido)
} else {
    // OK: usar sym
}
```

### Obtener variables (no solo funciones)

```c
// dlsym también puede obtener variables globales:
int *version = (int *)dlsym(handle, "lib_version");
// Para datos (no funciones), el cast void* → int* es válido en ISO C.

const char **name = (const char **)dlsym(handle, "lib_name");
```

---

## 4 — dlerror — Diagnóstico de errores

```c
char *dlerror(void);
// Retorna: string con el último error, o NULL si no hay error.
// IMPORTANTE: se limpia después de llamarla (una sola lectura por error).
```

Reglas clave:

```c
// 1. dlerror() se consume al leerla — la segunda llamada retorna NULL:
void *h = dlopen("no_existe.so", RTLD_NOW);
char *e1 = dlerror();  // "no_existe.so: cannot open..."
char *e2 = dlerror();  // NULL — ya fue consumido

// 2. Llamar dlerror() ANTES de dlsym para limpiar errores anteriores:
dlerror();              // limpiar
void *sym = dlsym(h, "foo");
char *err = dlerror();  // ahora refleja SOLO el resultado de este dlsym

// 3. No guardar el puntero a largo plazo — el string puede ser
//    sobreescrito por la siguiente operación dl*:
char *err = dlerror();
// ... usar err inmediatamente o copiarlo con strdup() ...
```

### Mensajes típicos

```
dlopen:
  "libfoo.so: cannot open shared object file: No such file or directory"
  "libfoo.so: wrong ELF class: ELFCLASS32"      (64-bit programa, 32-bit .so)
  "libfoo.so: undefined symbol: bar"             (con RTLD_NOW)

dlsym:
  "/path/to/libfoo.so: undefined symbol: func_name"

dlclose:
  (raro — casi siempre tiene éxito)
```

---

## 5 — dlclose — Descargar una biblioteca

```c
int dlclose(void *handle);
// Retorna: 0 si éxito, distinto de 0 si error.
```

```c
int result = dlclose(handle);
if (result != 0) {
    fprintf(stderr, "dlclose: %s\n", dlerror());
}
```

Comportamiento:

```c
// 1. Decrementa el contador de referencias
// 2. Si llega a 0: la biblioteca se descarga de memoria
// 3. Si otros dlopen la tienen abierta: NO se descarga

// DESPUÉS de dlclose:
// - Los punteros obtenidos con dlsym son INVÁLIDOS
// - Usarlos es undefined behavior (segfault probable)
// - El handle también es inválido

// Patrón seguro:
plugins[i].plugin->cleanup();  // ← llamar callbacks ANTES de dlclose
dlclose(plugins[i].handle);
plugins[i].handle = NULL;      // ← invalidar para evitar uso accidental
plugins[i].plugin = NULL;
```

---

## 6 — El problema void* ↔ function pointer en ISO C

ISO C (§6.3.2.3) define conversiones entre `void *` y punteros a objeto, pero **no** entre `void *` y punteros a función. POSIX, sin embargo, requiere que `dlsym` retorne `void *` y que sea convertible a function pointer.

```c
// ❌ Incorrecto en ISO C estricto (-Wpedantic emite warning):
double (*cosine)(double) = dlsym(handle, "cos");
// "ISO C forbids initialization between function pointer and 'void *'"

// ❌ También incorrecto — cast explícito, mismo problema:
double (*cosine)(double) = (double (*)(double))dlsym(handle, "cos");

// ✅ Workaround POSIX con memcpy (copia el valor del puntero sin cast):
double (*cosine)(double);
void *sym = dlsym(handle, "cos");
memcpy(&cosine, &sym, sizeof(cosine));
// Funciona porque en POSIX sizeof(void*) == sizeof(function_pointer)
// y la representación en memoria es idéntica.
```

El código del lab usa correctamente `memcpy`. En la práctica, el cast directo funciona en todos los sistemas POSIX, pero `-Wpedantic` lo diagnostica. Para código portable y libre de warnings, usa `memcpy`.

---

## 7 — Sistema de plugins

El patrón de plugins separa tres componentes:

```
┌──────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  plugin.h    │     │   host.c        │     │ plugin_upper.c  │
│ (interfaz)   │◄────│ (descubre y     │────►│ (implementa     │
│              │     │  carga .so)     │     │  la interfaz)   │
│ Plugin struct│     │ dlopen/dlsym    │     │ plugin_create() │
└──────────────┘     └─────────────────┘     └─────────────────┘
                            │
                            ▼
                     ┌─────────────────┐
                     │ plugin_reverse.c│
                     │ (otro plugin)   │
                     └─────────────────┘
```

### La interfaz (contrato entre host y plugins)

```c
// plugin.h — header compartido
#ifndef PLUGIN_H
#define PLUGIN_H

typedef struct {
    const char *name;
    const char *version;
    int  (*init)(void);              // retorna 0 si éxito
    void (*process)(const char *input);
    void (*cleanup)(void);
} Plugin;

// Cada plugin .so debe exportar esta función:
// Plugin *plugin_create(void);

#endif
```

El host solo conoce esta interfaz. No incluye headers de los plugins individuales.

### Un plugin

```c
// plugin_upper.c — convierte texto a mayúsculas
#include "plugin.h"
#include <stdio.h>
#include <ctype.h>

static int init(void) {
    printf("[upper] initialized\n");
    return 0;
}

static void process(const char *input) {
    printf("[upper] ");
    for (const char *p = input; *p; p++) {
        putchar(toupper((unsigned char)*p));
    }
    putchar('\n');
}

static void cleanup(void) {
    printf("[upper] cleaned up\n");
}

// Única función exportada — el host la busca con dlsym("plugin_create"):
Plugin *plugin_create(void) {
    static Plugin p = {
        .name    = "uppercase",
        .version = "1.0",
        .init    = init,
        .process = process,
        .cleanup = cleanup,
    };
    return &p;
}
```

Las funciones `init`, `process`, `cleanup` son `static` — no se exportan directamente. Solo `plugin_create` es visible externamente. El host obtiene las funciones a través de los punteros en la estructura `Plugin`.

```bash
# Compilar como .so:
gcc -std=c11 -Wall -Wextra -Wpedantic -shared -fPIC plugin_upper.c -o plugins/plugin_upper.so

# Verificar que solo plugin_create se exporta:
nm -D plugins/plugin_upper.so | grep -E "T |t "
# <addr> T plugin_create      ← visible (T mayúscula)
# init, process, cleanup NO aparecen (son static)
```

### El host

```c
// host.c — carga plugins de plugins/ en runtime
#include "plugin.h"
#include <dlfcn.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>

#define MAX_PLUGINS 16

typedef struct {
    void   *handle;
    Plugin *plugin;
} LoadedPlugin;

static int load_plugin(const char *path, LoadedPlugin *lp) {
    // 1. Cargar el .so:
    lp->handle = dlopen(path, RTLD_NOW);
    if (!lp->handle) {
        fprintf(stderr, "dlopen(%s): %s\n", path, dlerror());
        return -1;
    }

    // 2. Buscar plugin_create (con memcpy para evitar warning -Wpedantic):
    dlerror();
    Plugin *(*create)(void);
    void *sym = dlsym(lp->handle, "plugin_create");
    memcpy(&create, &sym, sizeof(create));
    char *err = dlerror();
    if (err) {
        fprintf(stderr, "dlsym(%s): %s\n", path, err);
        dlclose(lp->handle);
        return -1;
    }

    // 3. Crear el plugin y llamar init:
    lp->plugin = create();
    if (lp->plugin->init() != 0) {
        fprintf(stderr, "Plugin %s init failed\n", path);
        dlclose(lp->handle);
        return -1;
    }

    printf("Loaded plugin: %s v%s\n", lp->plugin->name, lp->plugin->version);
    return 0;
}

int main(void) {
    LoadedPlugin plugins[MAX_PLUGINS];
    int count = 0;

    // Recorrer plugins/ buscando archivos .so:
    DIR *dir = opendir("plugins");
    if (!dir) { perror("opendir"); return 1; }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < MAX_PLUGINS) {
        size_t len = strlen(entry->d_name);
        if (len > 3 && strcmp(entry->d_name + len - 3, ".so") == 0) {
            char path[512];
            snprintf(path, sizeof(path), "./plugins/%s", entry->d_name);
            if (load_plugin(path, &plugins[count]) == 0) {
                count++;
            }
        }
    }
    closedir(dir);

    // Usar todos los plugins:
    for (int i = 0; i < count; i++) {
        plugins[i].plugin->process("Hello World");
    }

    // Limpiar:
    for (int i = 0; i < count; i++) {
        plugins[i].plugin->cleanup();
        dlclose(plugins[i].handle);
    }

    return 0;
}
```

### El poder: extensibilidad sin recompilación

```bash
# Compilar host una vez:
gcc -std=c11 -Wall -Wextra -Wpedantic host.c -ldl -o host

# Plugin 1:
gcc -shared -fPIC plugin_upper.c -o plugins/plugin_upper.so
./host
# Loaded plugin: uppercase v1.0
# [upper] HELLO WORLD

# Plugin 2 — SIN recompilar host:
gcc -shared -fPIC plugin_reverse.c -o plugins/plugin_reverse.so
./host
# Loaded plugin: uppercase v1.0
# Loaded plugin: reverse v1.0
# [upper] HELLO WORLD
# [reverse] dlroW olleH

# Plugin 3 — SIN recompilar host:
gcc -shared -fPIC plugin_count.c -o plugins/plugin_count.so
./host
# Carga los 3 plugins automáticamente.
```

Los plugins **no aparecen** en `ldd host` — no son dependencias de compilación. Se descubren en runtime con `opendir` + `dlopen`.

---

## 8 — RTLD_DEFAULT y RTLD_NEXT

`dlsym` acepta pseudo-handles especiales en lugar de un handle de `dlopen`:

### RTLD_DEFAULT

Busca el símbolo en **todas** las bibliotecas cargadas (scope global):

```c
// Encontrar printf sin tener un handle de libc:
void *sym = dlsym(RTLD_DEFAULT, "printf");
// Busca en el ejecutable principal + todas las .so cargadas.
// Útil para verificar si un símbolo existe en el proceso.
```

### RTLD_NEXT

Busca el símbolo en la **siguiente** biblioteca (no en la actual). Usado para interponer (wrapping):

```c
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

// Interceptar malloc — contar asignaciones:
static int malloc_count = 0;

void *malloc(size_t size) {
    malloc_count++;
    fprintf(stderr, "[malloc #%d] size=%zu\n", malloc_count, size);

    // Obtener el malloc ORIGINAL (el de libc):
    void *(*original_malloc)(size_t);
    void *sym = dlsym(RTLD_NEXT, "malloc");
    memcpy(&original_malloc, &sym, sizeof(original_malloc));

    return original_malloc(size);
}
```

```bash
# Compilar como .so:
gcc -shared -fPIC malloc_spy.c -o libmalloc_spy.so -ldl

# Usar con LD_PRELOAD para monitorear cualquier programa:
LD_PRELOAD=./libmalloc_spy.so ls
# [malloc #1] size=...
# [malloc #2] size=...
# ... (cada malloc de ls queda registrado)
```

**Nota**: `RTLD_NEXT` requiere `#define _GNU_SOURCE` antes de cualquier `#include`. Sin él, el símbolo no está disponible.

---

## 9 — Patrones de error robusto

### Patrón básico: verificar cada operación

```c
void *handle = dlopen(path, RTLD_NOW);
if (!handle) {
    fprintf(stderr, "dlopen: %s\n", dlerror());
    return -1;
}

dlerror();  // limpiar
void *sym = dlsym(handle, "init");
char *err = dlerror();
if (err) {
    fprintf(stderr, "dlsym: %s\n", err);
    dlclose(handle);
    return -1;
}

// ... usar sym ...

dlclose(handle);
```

### Patrón con cleanup centralizado

```c
void *handle = NULL;
char *saved_error = NULL;
int result = -1;

handle = dlopen(path, RTLD_NOW);
if (!handle) {
    saved_error = dlerror();
    goto cleanup;
}

dlerror();
void *sym = dlsym(handle, "init");
char *err = dlerror();
if (err) {
    saved_error = err;  // guardar ANTES del goto
    goto cleanup;
}

// ... usar sym ...
result = 0;

cleanup:
    if (result != 0 && saved_error) {
        fprintf(stderr, "Error: %s\n", saved_error);
    }
    if (handle) {
        dlclose(handle);
    }
    return result;
```

**Nota**: el error del `README.md` original llamaba `dlerror()` dos veces — una para verificar y otra en el bloque `fail` — pero `dlerror()` se consume al leerla. La variable `saved_error` resuelve esto.

### Errores comunes

```c
// 1. "cannot open shared object file: No such file or directory"
//    → El archivo no existe o no está en el path de búsqueda.
//    → Solución: usar ruta con ./ para archivos locales: dlopen("./libfoo.so", ...)

// 2. "undefined symbol: func_name"
//    → La función no está exportada en el .so.
//    → Verificar: nm -D libfoo.so | grep func_name

// 3. Segfault al llamar la función
//    → El tipo del function pointer no coincide con la firma real.
//    → El .so fue descargado con dlclose pero el puntero se sigue usando.

// 4. "undefined reference to `dlopen'" (en compilación)
//    → Falta -ldl en la línea de gcc.
//    → En glibc 2.34+ puede no ocurrir, pero -ldl sigue siendo buena práctica.
```

---

## 10 — Comparación con otros lenguajes

```
C (dlopen/dlsym):
  - Manual: cargar .so por ruta, buscar símbolos por string, cast manual
  - Sin verificación de tipos en compilación
  - Máxima flexibilidad, máximo riesgo

Rust (libloading crate):
  - Similar a dlopen/dlsym pero con wrapper seguro
  - libloading::Library::new("foo.so")
  - lib.get::<fn(i32) -> i32>(b"func_name")
  - El tipo se verifica en compilación (generics)

Python (ctypes):
  - ctypes.CDLL("libfoo.so")
  - lib.func_name.restype = ctypes.c_int
  - Más fácil pero más lento (FFI overhead)

Go (plugin package):
  - plugin.Open("foo.so")
  - plug.Lookup("FuncName")
  - Solo soporta plugins compilados con Go

Java (JNI / ServiceLoader):
  - System.loadLibrary("foo")  (JNI para C)
  - ServiceLoader para plugins Java

Patrón común en todos: cargar → buscar símbolo → usar → descargar.
La diferencia es cuánta seguridad de tipos ofrece cada lenguaje.
```

---

## Ejercicios

### Ejercicio 1 — dlopen/dlsym básico con libm

Carga `libm.so.6` con `dlopen`, obtén un puntero a `sqrt` con `dlsym` (usando el patrón `memcpy`), calcula `sqrt(144.0)`, e imprime el resultado. Compila con `-ldl` y sin enlazar `-lm`.

```c
// math_runtime.c
// 1. dlopen("libm.so.6", RTLD_LAZY)
// 2. dlsym(handle, "sqrt") → memcpy a double (*)(double)
// 3. printf("sqrt(144) = %f\n", my_sqrt(144.0));
// 4. dlclose(handle)
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic math_runtime.c -ldl -o math_runtime
./math_runtime
```

<details><summary>Predicción</summary>

La salida será `sqrt(144) = 12.000000`. El programa no enlaza con `-lm` (no es dependencia de compilación), pero `dlopen` carga `libm.so.6` en runtime y `dlsym` encuentra `sqrt`. `ldd math_runtime` no mostrará `libm` — solo aparecen `libc` y `libdl` (o solo `libc` en glibc 2.34+). La función `sqrt` se usa a través de un puntero obtenido dinámicamente.
</details>

### Ejercicio 2 — Provocar errores con dlerror

Escribe un programa que provoque tres errores deliberados y muestre los mensajes de `dlerror()`:
1. `dlopen` de un `.so` inexistente (`./libfantasma.so`)
2. `dlsym` de un símbolo inexistente en `libm.so.6` (`"funcion_inventada"`)
3. Un caso exitoso (`dlsym` de `"sin"`) para comparar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic dlerror_test.c -ldl -o dlerror_test
./dlerror_test
```

<details><summary>Predicción</summary>

Test 1: `dlerror()` mostrará algo como `"./libfantasma.so: cannot open shared object file: No such file or directory"`. Test 2: `dlerror()` mostrará `"/lib64/libm.so.6: undefined symbol: funcion_inventada"`. Test 3: `dlerror()` retornará `NULL` (sin error), y `sin(1.5708)` dará aproximadamente `1.000000`. Los mensajes de `dlerror` incluyen la ruta del archivo para contexto.
</details>

### Ejercicio 3 — Verificar que -ldl es necesario

Compila el programa del ejercicio 1 **sin** `-ldl`. ¿Qué pasa? Luego verifica tu versión de glibc con `ldd --version`. Si es ≥ 2.34, ¿compila sin error?

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic math_runtime.c -o test_noldl 2>&1 || echo "FALLÓ"
ldd --version | head -1
```

<details><summary>Predicción</summary>

En Fedora con glibc 2.34+: **compila sin error**, porque `dlopen`/`dlsym` están integradas en `libc.so.6`. En glibc < 2.34: falla con `undefined reference to 'dlopen'`, `'dlsym'`, `'dlclose'`, `'dlerror'`. En Fedora 43 (la versión del usuario), glibc será ≥ 2.38, así que compilará sin `-ldl`. Aun así, incluir `-ldl` es buena práctica para portabilidad.
</details>

### Ejercicio 4 — Plugin mínimo

Crea un plugin `plugin_lower.c` que convierta texto a minúsculas, siguiendo la interfaz de `plugin.h`. Compílalo como `.so` en el directorio `plugins/`. Ejecuta el host sin recompilarlo y verifica que descubre el nuevo plugin.

```bash
# Escribir plugin_lower.c con plugin_create() que retorne Plugin con:
# .name = "lowercase", .process = convierte a minúsculas
gcc -shared -fPIC plugin_lower.c -o plugins/plugin_lower.so
nm -D plugins/plugin_lower.so | grep plugin_create
./host
```

<details><summary>Predicción</summary>

`nm -D` mostrará `T plugin_create` — la única función exportada. El host descubrirá `plugin_lower.so` automáticamente junto con los otros plugins. La salida incluirá una línea como `[lower] hello world` (texto convertido a minúsculas). El host **no** fue recompilado — `readdir` encontró el archivo nuevo y `dlopen` lo cargó.
</details>

### Ejercicio 5 — Plugin con estado

Crea un plugin `plugin_stats.c` que cuente cuántas veces se llamó `process()` y lo imprima en `cleanup()`. Usa una variable `static int call_count` dentro del plugin. ¿La variable persiste entre llamadas a `process()`?

```c
// En cleanup():
// printf("[stats] process was called %d time(s)\n", call_count);
```

<details><summary>Predicción</summary>

Sí, la variable `static int call_count` persiste entre llamadas porque el `.so` permanece cargado en memoria entre `dlopen` y `dlclose`. Cada llamada a `process()` incrementa el contador. En `cleanup()` se imprime el total. Si el host llama `process()` 3 veces, verás `process was called 3 time(s)`. El estado se pierde solo cuando `dlclose` descarga la biblioteca (o al terminar el programa).
</details>

### Ejercicio 6 — RTLD_NOW vs RTLD_LAZY

Crea un `.so` con una función que llama a un símbolo inexistente (`undefined_func()`). Cárgalo con `RTLD_LAZY` y luego con `RTLD_NOW`. ¿En cuál caso falla `dlopen` y en cuál falla después?

```c
// broken_plugin.c:
// void broken_func(void) { undefined_func(); }  // undefined_func no existe
// Plugin *plugin_create(void) { ... }
```

```bash
gcc -shared -fPIC broken_plugin.c -o plugins/plugin_broken.so 2>&1
# ¿Compila? ¿Falla?
```

<details><summary>Predicción</summary>

La compilación con `gcc -shared` **tiene éxito** — el linker no resuelve símbolos indefinidos al crear un `.so` (a diferencia de un ejecutable). Con `RTLD_LAZY`: `dlopen` tiene éxito (no resuelve `undefined_func` aún). Si llamas `broken_func()`, el programa crashea o `ld.so` reporta error en ese momento. Con `RTLD_NOW`: `dlopen` retorna `NULL` inmediatamente, y `dlerror()` dice `"undefined symbol: undefined_func"`. Por esto `RTLD_NOW` es más seguro para plugins — detecta el problema al cargar.
</details>

### Ejercicio 7 — Inspeccionar símbolos exportados

Para cada uno de los plugins del lab (`plugin_upper.so`, `plugin_reverse.so`, `plugin_count.so`), usa `nm -D` para listar los símbolos dinámicos. ¿Cuáles funciones aparecen? ¿Aparecen `init`, `process`, `cleanup`?

```bash
for so in plugins/*.so; do
    echo "=== $so ==="
    nm -D "$so" | grep -E " T | W "
done
```

<details><summary>Predicción</summary>

Cada `.so` mostrará solo `T plugin_create` como símbolo exportado en la sección de texto. Las funciones `init`, `process`, `cleanup` **no** aparecen porque son `static` — visibles solo dentro de su unidad de compilación. Esto es intencional: el punto de entrada es `plugin_create`, que retorna punteros a las funciones internas a través de la estructura `Plugin`. El `static` actúa como encapsulación.
</details>

### Ejercicio 8 — Usar punteros después de dlclose

Carga un plugin, obtén el `Plugin *` con `plugin_create()`, llama `dlclose`, y luego intenta llamar `plugin->process()`. ¿Qué pasa?

```c
Plugin *p = create();
dlclose(handle);        // ← descarga el .so
p->process("test");     // ← ¿qué ocurre?
```

<details><summary>Predicción</summary>

Undefined behavior. Lo más probable es un **segfault** (SIGSEGV) porque el código de la función `process` ya no está mapeado en memoria — fue descargado por `dlclose`. El puntero `p` sigue apuntando a una dirección que ya no es válida. Otros resultados posibles: corrupción silenciosa (si otra asignación reutilizó esa memoria) o aparentemente funciona (si el SO no desmapeó la página aún). Siempre llamar callbacks **antes** de `dlclose`.
</details>

### Ejercicio 9 — Wrapper con RTLD_NEXT

Crea un `.so` que intercepte `puts()`: antes de cada llamada, imprime `[SPY]` en stderr, luego llama al `puts` original con `RTLD_NEXT`. Pruébalo con `LD_PRELOAD`:

```bash
# spy_puts.c:
# #define _GNU_SOURCE
# #include <dlfcn.h>
# #include <stdio.h>
# #include <string.h>
# int puts(const char *s) {
#     fprintf(stderr, "[SPY] puts called\n");
#     int (*original)(const char *);
#     void *sym = dlsym(RTLD_NEXT, "puts");
#     memcpy(&original, &sym, sizeof(original));
#     return original(s);
# }

gcc -shared -fPIC spy_puts.c -o libspy.so -ldl
LD_PRELOAD=./libspy.so echo "hello"
```

<details><summary>Predicción</summary>

`echo` es un shell builtin y no usa `puts`, así que no será interceptado. Para ver el efecto, usar un programa C que llame `puts()`, o un comando como `LD_PRELOAD=./libspy.so /usr/bin/printf "hello\n"` (aunque `printf` no usa `puts` internamente). El test más directo: escribir un `test.c` con `puts("hello")`, compilar, y ejecutar con `LD_PRELOAD=./libspy.so ./test`. Verás `[SPY] puts called` en stderr seguido de `hello` en stdout. `RTLD_NEXT` salta nuestra versión de `puts` y encuentra la original en libc.
</details>

### Ejercicio 10 — Orden de carga de plugins

El host del `lab_codex/` ordena los plugins alfabéticamente con `qsort` antes de cargarlos. El host de `labs/` los carga en el orden que `readdir` los devuelve. Ejecuta ambos hosts varias veces. ¿El orden es consistente en cada uno?

```bash
# En labs/:
./host && ./host && ./host
# ¿El orden de plugins cambia entre ejecuciones?

# En lab_codex/:
./host && ./host && ./host
# ¿El orden es siempre el mismo?
```

<details><summary>Predicción</summary>

El host de `labs/` puede mostrar los plugins en **orden diferente** entre ejecuciones, porque `readdir` no garantiza un orden específico (depende del filesystem y del orden de creación de los archivos). El host de `lab_codex/` mostrará los plugins **siempre en el mismo orden** (alfabético por nombre de archivo) porque usa `qsort` con `strcmp` antes de cargarlos. En producción, ordenar es importante para reproducibilidad — el comportamiento no debería depender del orden del filesystem.
</details>
