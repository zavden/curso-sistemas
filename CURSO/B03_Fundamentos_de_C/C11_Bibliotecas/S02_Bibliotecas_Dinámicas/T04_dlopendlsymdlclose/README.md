# T04 — dlopen, dlsym, dlclose

## Carga dinámica en runtime

Normalmente, las bibliotecas se declaran en tiempo de compilación
(`-lfoo`). Con `dlopen`, se pueden cargar **en runtime**, sin
saber en compilación qué biblioteca se usará:

```c
#include <dlfcn.h>    // dlopen, dlsym, dlclose, dlerror

// Flujo:
// 1. dlopen  — cargar un .so
// 2. dlsym   — obtener puntero a función o variable
// 3. Usar el puntero como función normal
// 4. dlclose — descargar el .so

// Compilar con: gcc main.c -ldl -o program
// -ldl enlaza con libdl (la biblioteca de carga dinámica).
// En glibc moderno (2.34+), dlopen está en libc directamente,
// pero -ldl sigue funcionando por compatibilidad.
```

## dlopen — Cargar una biblioteca

```c
#include <dlfcn.h>
#include <stdio.h>

int main(void) {
    // Cargar libm.so (la biblioteca matemática):
    void *handle = dlopen("libm.so.6", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return 1;
    }

    // ... usar la biblioteca ...

    dlclose(handle);
    return 0;
}
```

```c
// Flags de dlopen:

// RTLD_LAZY — resolver símbolos cuando se usen (lazy binding):
void *h = dlopen("libfoo.so", RTLD_LAZY);
// Los símbolos se resuelven la primera vez que se llaman.
// Más rápido para cargar, puede fallar después si falta un símbolo.

// RTLD_NOW — resolver TODOS los símbolos inmediatamente:
void *h = dlopen("libfoo.so", RTLD_NOW);
// Si falta algún símbolo, dlopen falla inmediatamente.
// Más seguro — detecta errores antes de usar la biblioteca.

// RTLD_GLOBAL — símbolos visibles para otras bibliotecas:
void *h = dlopen("libfoo.so", RTLD_LAZY | RTLD_GLOBAL);
// Los símbolos de libfoo.so están disponibles para .so cargados después.

// RTLD_LOCAL (default) — símbolos solo para esta biblioteca:
void *h = dlopen("libfoo.so", RTLD_LAZY | RTLD_LOCAL);

// RTLD_NODELETE — no descargar al hacer dlclose:
void *h = dlopen("libfoo.so", RTLD_LAZY | RTLD_NODELETE);
```

## dlsym — Obtener un símbolo

```c
// dlsym retorna un void* al símbolo solicitado.
// Hay que castearlo al tipo correcto.

#include <dlfcn.h>
#include <stdio.h>

int main(void) {
    void *handle = dlopen("libm.so.6", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        return 1;
    }

    // Limpiar errores previos:
    dlerror();

    // Obtener puntero a cos():
    double (*cosine)(double) = dlsym(handle, "cos");

    // Verificar error (dlsym puede retornar NULL legítimamente):
    char *error = dlerror();
    if (error) {
        fprintf(stderr, "dlsym: %s\n", error);
        dlclose(handle);
        return 1;
    }

    // Usar la función:
    printf("cos(2.0) = %f\n", cosine(2.0));

    dlclose(handle);
    return 0;
}
```

```c
// dlerror() retorna un string con el último error, o NULL si no hay error.
// Se limpia después de llamarla → llamar dlerror() una vez por operación.

// Patrón seguro para dlsym:
dlerror();    // limpiar
void *sym = dlsym(handle, "func_name");
char *err = dlerror();
if (err) {
    // error real
} else if (!sym) {
    // el símbolo existe pero vale NULL (raro pero válido)
} else {
    // OK — usar sym
}
```

## dlclose — Descargar

```c
int result = dlclose(handle);
if (result != 0) {
    fprintf(stderr, "dlclose: %s\n", dlerror());
}

// dlclose decrementa el contador de referencias.
// Si llega a 0, la biblioteca se descarga de memoria.
// Si otros dlopen la tienen abierta, NO se descarga.

// Después de dlclose:
// - Los punteros obtenidos con dlsym son INVÁLIDOS
// - Usarlos causa undefined behavior (segfault probable)
// - handle también es inválido
```

## Ejemplo completo: sistema de plugins

```c
// --- plugin.h --- (interfaz común para todos los plugins)
#ifndef PLUGIN_H
#define PLUGIN_H

typedef struct {
    const char *name;
    const char *version;
    int (*init)(void);
    void (*process)(const char *input);
    void (*cleanup)(void);
} Plugin;

// Cada plugin debe exportar esta función:
// Plugin *plugin_create(void);

#endif
```

```c
// --- plugin_upper.c --- (un plugin que convierte a mayúsculas)
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
        putchar(toupper(*p));
    }
    putchar('\n');
}

static void cleanup(void) {
    printf("[upper] cleaned up\n");
}

// Función exportada — el host la busca con dlsym:
Plugin *plugin_create(void) {
    static Plugin p = {
        .name = "uppercase",
        .version = "1.0",
        .init = init,
        .process = process,
        .cleanup = cleanup,
    };
    return &p;
}
```

```bash
# Compilar el plugin:
gcc -shared -fPIC plugin_upper.c -o plugins/plugin_upper.so
```

```c
// --- main.c --- (host que carga plugins)
#include "plugin.h"
#include <dlfcn.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>

#define MAX_PLUGINS 16

typedef struct {
    void *handle;
    Plugin *plugin;
} LoadedPlugin;

int load_plugin(const char *path, LoadedPlugin *lp) {
    lp->handle = dlopen(path, RTLD_NOW);
    if (!lp->handle) {
        fprintf(stderr, "dlopen(%s): %s\n", path, dlerror());
        return -1;
    }

    // Buscar la función plugin_create:
    dlerror();
    Plugin *(*create)(void) = dlsym(lp->handle, "plugin_create");
    char *err = dlerror();
    if (err) {
        fprintf(stderr, "dlsym: %s\n", err);
        dlclose(lp->handle);
        return -1;
    }

    lp->plugin = create();
    if (lp->plugin->init() != 0) {
        fprintf(stderr, "Plugin %s init failed\n", lp->plugin->name);
        dlclose(lp->handle);
        return -1;
    }

    printf("Loaded plugin: %s v%s\n", lp->plugin->name, lp->plugin->version);
    return 0;
}

int main(void) {
    LoadedPlugin plugins[MAX_PLUGINS];
    int count = 0;

    // Cargar todos los .so del directorio plugins/:
    DIR *dir = opendir("plugins");
    if (!dir) { perror("opendir"); return 1; }

    struct dirent *entry;
    while ((entry = readdir(dir)) && count < MAX_PLUGINS) {
        size_t len = strlen(entry->d_name);
        if (len > 3 && strcmp(entry->d_name + len - 3, ".so") == 0) {
            char path[512];
            snprintf(path, sizeof(path), "plugins/%s", entry->d_name);
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

```bash
# Compilar y ejecutar:
gcc main.c -ldl -o host
mkdir -p plugins
gcc -shared -fPIC plugin_upper.c -o plugins/plugin_upper.so

./host
# Loaded plugin: uppercase v1.0
# [upper] initialized
# [upper] HELLO WORLD
# [upper] cleaned up

# Agregar un nuevo plugin SIN recompilar host:
gcc -shared -fPIC plugin_reverse.c -o plugins/plugin_reverse.so
./host
# Carga ambos plugins automáticamente.
```

## dlsym con RTLD_DEFAULT y RTLD_NEXT

```c
// RTLD_DEFAULT — buscar en el scope global (todas las bibliotecas cargadas):
void *sym = dlsym(RTLD_DEFAULT, "printf");
// Encuentra printf en libc sin necesidad de handle.

// RTLD_NEXT — buscar en la SIGUIENTE biblioteca (wrapper pattern):
// Útil en LD_PRELOAD para llamar a la función original:

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

// Override de malloc que cuenta asignaciones:
static int malloc_count = 0;

void *malloc(size_t size) {
    malloc_count++;
    // Obtener el malloc ORIGINAL (el de libc):
    void *(*original_malloc)(size_t) = dlsym(RTLD_NEXT, "malloc");
    return original_malloc(size);
}
```

## Manejo de errores

```c
// Patrón robusto de manejo de errores:

void *handle = NULL;
int result = -1;

handle = dlopen(path, RTLD_NOW);
if (!handle) goto fail;

dlerror();    // limpiar
void *sym = dlsym(handle, "init");
if (dlerror()) goto fail;

// ... usar sym ...
result = 0;

fail:
    if (result != 0) {
        fprintf(stderr, "Error: %s\n", dlerror());
    }
    if (handle) {
        dlclose(handle);
    }
    return result;
```

```c
// Errores comunes:

// 1. "cannot open shared object file":
//    El archivo no existe o no está en el path de búsqueda.
//    Solución: usar ruta absoluta o relativa con ./

// 2. "undefined symbol: func_name":
//    La función no está en el .so.
//    Verificar: nm -D libfoo.so | grep func_name

// 3. Segfault al llamar la función:
//    Tipo de puntero a función incorrecto.
//    Verificar que la firma del cast coincide con la real.

// 4. Olvidar -ldl:
//    undefined reference to `dlopen'
//    Agregar -ldl al compilar.
```

---

## Ejercicios

### Ejercicio 1 — Calculadora con plugins

```c
// Crear un sistema de plugins para una calculadora:
// - Cada plugin implementa una operación (add, sub, mul, pow, etc.)
// - Interfaz: Operation *operation_create(void)
//   donde Operation = { name, description, double (*compute)(double, double) }
// - El host carga todos los .so de plugins/
// - El usuario ingresa: "add 3 5" → host busca el plugin "add" → 8.0
// Crear al menos 3 plugins. Agregar uno nuevo SIN recompilar host.
```

### Ejercicio 2 — Hot reload

```c
// Implementar hot reload de un plugin:
// 1. Cargar plugin.so con dlopen
// 2. Usar la función process() del plugin
// 3. Vigilar si plugin.so cambió en disco (stat, comparar mtime)
// 4. Si cambió: dlclose el viejo, dlopen el nuevo
// 5. Continuar usando process() con la versión nueva
// Probar: mientras el programa corre, recompilar el plugin
// y verificar que el comportamiento cambia sin reiniciar.
```

### Ejercicio 3 — Wrapper con RTLD_NEXT

```c
// Crear un .so que intercepte open() y write():
// - Registrar en stderr cada archivo abierto y cada write
// - Llamar a la función original con RTLD_NEXT
// Usar con LD_PRELOAD para monitorear qué archivos
// usa un programa arbitrario.
// Probar con: LD_PRELOAD=./libspy.so cat /etc/hostname
```
