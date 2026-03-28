# T03 — Versionado de soname

> **Fuentes**: `README.md`, `LABS.md`, `labs/` (7 archivos), `lab_codex/` (8 archivos).
> Regla aplicada: **R3** (sin `.max.md`).

## Erratas detectadas

Sin erratas detectadas en el material fuente.

---

## Tabla de contenidos

1. [El problema del versionado](#1--el-problema-del-versionado)
2. [Los tres nombres](#2--los-tres-nombres)
3. [Semantic versioning en bibliotecas compartidas](#3--semantic-versioning-en-bibliotecas-compartidas)
4. [Crear una biblioteca con soname embebido](#4--crear-una-biblioteca-con-soname-embebido)
5. [La cadena de symlinks](#5--la-cadena-de-symlinks)
6. [Verificar soname con readelf y objdump](#6--verificar-soname-con-readelf-y-objdump)
7. [Actualización compatible: minor/patch bump](#7--actualización-compatible-minorpatch-bump)
8. [Actualización incompatible: major bump y coexistencia](#8--actualización-incompatible-major-bump-y-coexistencia)
9. [ldconfig y gestión del sistema](#9--ldconfig-y-gestión-del-sistema)
10. [Cuándo cambiar cada componente de versión](#10--cuándo-cambiar-cada-componente-de-versión)

---

## 1 — El problema del versionado

Cuando una biblioteca dinámica se actualiza, los programas que dependen de ella pueden romperse si la nueva versión cambia la interfaz:

```
Escenario problemático:
1. Compilas program.c contra libfoo.so (versión 1.0)
2. libfoo.so se actualiza a versión 2.0 (cambia la firma de una función)
3. Tu programa falla: "undefined symbol", crash, o comportamiento corrupto

El problema: el programa se compiló esperando una interfaz que ya no existe.
```

Sin un sistema de versionado, actualizar **cualquier** biblioteca compartida arriesga romper **todos** los programas que dependen de ella. Linux resuelve esto con un esquema de **tres nombres** que separa la identidad del archivo físico de la promesa de compatibilidad.

La raíz del problema es la diferencia entre **API** y **ABI**:

```
API (Application Programming Interface):
  - Interfaz a nivel de código fuente
  - Firmas de funciones, tipos, constantes
  - Si cambia → el código no compila

ABI (Application Binary Interface):
  - Interfaz a nivel binario
  - Layout de structs, calling conventions, tamaños de tipos
  - Si cambia → el binario existente falla sin recompilar

Un cambio puede romper la ABI sin romper la API (y viceversa):
  - Agregar un campo a un struct: API compatible, ABI incompatible
    (sizeof cambia → binarios viejos corrompen memoria)
  - Renombrar un parámetro: ABI compatible, puede romper API
    (si alguien usaba argumentos nombrados en macros/wrappers)
```

---

## 2 — Los tres nombres

Linux usa un sistema de tres nombres para cada biblioteca compartida:

```
Real name:    libcalc.so.1.2.3    ← archivo físico en disco
Soname:       libcalc.so.1        ← symlink, identifica compatibilidad ABI
Linker name:  libcalc.so          ← symlink, lo usa gcc al compilar

Cadena de symlinks:
libcalc.so  →  libcalc.so.1  →  libcalc.so.1.2.3
 (linker)       (soname)          (real name)
```

### Real name (nombre real)

El archivo físico que contiene el código. Lleva la versión completa:

```
lib<nombre>.so.<MAJOR>.<MINOR>.<PATCH>

libcalc.so.1.2.3
             │ │ │
             │ │ └─ PATCH: bug fixes (compatible)
             │ └─── MINOR: nueva funcionalidad (compatible)
             └───── MAJOR: cambio de ABI (INCOMPATIBLE)
```

Cada versión nueva es un archivo diferente. En el sistema de archivos pueden coexistir:

```
libcalc.so.1.0.0    ← versión original
libcalc.so.1.1.0    ← agrega funciones (compatible con 1.0)
libcalc.so.1.2.3    ← bug fixes (compatible con 1.0)
libcalc.so.2.0.0    ← cambia ABI (INCOMPATIBLE con 1.x)
```

### Soname (nombre compartido)

Symlink que incluye solo el **major version**. Es la promesa de compatibilidad binaria:

```bash
# El symlink apunta al real name más reciente del mismo major:
libcalc.so.1 → libcalc.so.1.2.3

# El soname se graba DENTRO del archivo .so al compilarlo:
readelf -d libcalc.so.1.2.3 | grep SONAME
# (SONAME)  Library soname: [libcalc.so.1]

# Y se graba en cada ejecutable que depende de la biblioteca:
readelf -d program | grep NEEDED
# (NEEDED)  Shared library: [libcalc.so.1]
```

En runtime, `ld-linux.so` busca `libcalc.so.1` (el soname), **no** `libcalc.so.1.2.3` (el real name). El symlink `libcalc.so.1 → libcalc.so.1.2.3` conecta ambos.

Esto permite actualizar de `1.2.3` a `1.3.0` sin recompilar nada:

```
Antes:   libcalc.so.1 → libcalc.so.1.2.3
Después: libcalc.so.1 → libcalc.so.1.3.0  ← solo cambió el symlink

El programa busca "libcalc.so.1" → encuentra la nueva versión.
Como el major no cambió → ABI compatible → funciona.
```

### Linker name (nombre de enlace)

Symlink sin versión, usado **solo en tiempo de compilación**:

```bash
libcalc.so → libcalc.so.1

# gcc -lcalc busca "libcalc.so" en los directorios de búsqueda.
# El symlink redirige al soname actual.
```

Este symlink lo crea:
- El paquete `-dev` (e.g., `libfoo-dev` en Debian/Ubuntu, `libfoo-devel` en Fedora)
- `ldconfig` (para el soname; el linker name a veces requiere acción manual)
- O manualmente: `ln -s libcalc.so.1 libcalc.so`

### Ejemplo real en el sistema

```bash
# En Fedora/RHEL:
ls -la /usr/lib64/libz.so*
# libz.so       → libz.so.1.x.x     (linker name)
# libz.so.1     → libz.so.1.x.x     (soname)
# libz.so.1.x.x                      (real name)

# En Debian/Ubuntu:
ls -la /usr/lib/x86_64-linux-gnu/libz*
# (mismo esquema, diferente directorio)
```

---

## 3 — Semantic versioning en bibliotecas compartidas

El esquema `MAJOR.MINOR.PATCH` tiene un significado preciso:

```
MAJOR.MINOR.PATCH
  │     │     │
  │     │     └─ Correcciones de bugs, sin cambio de interfaz
  │     └─────── Nueva funcionalidad, compatible hacia atrás
  └───────────── Cambio incompatible de API/ABI

En bibliotecas compartidas Linux:
  soname    = lib<name>.so.<MAJOR>
  real name = lib<name>.so.<MAJOR>.<MINOR>.<PATCH>
```

La relación entre versión y soname es directa:

```
libcalc.so.1.0.0  →  soname: libcalc.so.1
libcalc.so.1.1.0  →  soname: libcalc.so.1  (mismo — compatible)
libcalc.so.1.2.3  →  soname: libcalc.so.1  (mismo — compatible)
libcalc.so.2.0.0  →  soname: libcalc.so.2  (DIFERENTE — incompatible)
```

Todas las versiones `1.x.y` comparten el soname `libcalc.so.1`. Cuando aparece `2.0.0`, el soname cambia a `libcalc.so.2`. Ambos sonames coexisten en el sistema.

### Comparación con otros ecosistemas

```
C (.so):   soname = libfoo.so.<MAJOR>    ← gestión manual con symlinks
Rust:      crates siguen semver          ← Cargo resuelve automáticamente
Go:        import path incluye /v2, /v3  ← módulo diferente = import diferente
npm:       package.json + node_modules   ← semver con ranges (~, ^)

En todos: MAJOR++ = incompatible, requiere acción del consumidor.
La diferencia es quién gestiona la resolución: en C es el SO, en otros es el gestor de paquetes.
```

---

## 4 — Crear una biblioteca con soname embebido

El proceso tiene tres pasos: compilar con `-fPIC`, enlazar con `-Wl,-soname`, crear symlinks.

```bash
# 1. Compilar a objeto PIC:
gcc -std=c11 -Wall -Wextra -Wpedantic -c -fPIC calc_v1.c -o calc_v1.o

# 2. Crear el .so con soname embebido:
gcc -shared -Wl,-soname,libcalc.so.1 -o libcalc.so.1.0.0 calc_v1.o
```

El flag `-Wl,-soname,libcalc.so.1` pasa al linker la instrucción de grabar `libcalc.so.1` como SONAME dentro de la sección `.dynamic` del archivo ELF. Este soname es **metadato embebido**: no depende del nombre del archivo en disco.

```bash
# Detalle del flag:
-Wl,-soname,libcalc.so.1
 │    │       │
 │    │       └─ valor del soname (lib<name>.so.<MAJOR>)
 │    └───────── opción del linker: -soname
 └────────────── -Wl, = pasar lo siguiente al linker (ld)

# Puedes nombrar el archivo como quieras, pero el soname embebido es fijo:
gcc -shared -Wl,-soname,libcalc.so.1 -o mi_archivo_raro.so calc_v1.o
readelf -d mi_archivo_raro.so | grep SONAME
# (SONAME)  Library soname: [libcalc.so.1]
# El soname NO depende del nombre del archivo
```

### Sin soname

Si omites `-Wl,-soname,...`:

```bash
gcc -shared -o libcalc.so calc_v1.o
readelf -d libcalc.so | grep SONAME
# (nada — no hay campo SONAME)

# El linker usará el nombre del archivo directamente como dependencia NEEDED:
gcc main.c -L. -lcalc -o program
readelf -d program | grep NEEDED
# (NEEDED)  Shared library: [libcalc.so]

# Sin soname, pierdes toda la maquinaria de versionado.
# Cualquier actualización puede romper el programa.
```

---

## 5 — La cadena de symlinks

Después de crear el `.so`, se necesitan dos symlinks:

```bash
# 3. Crear los symlinks:
ln -sf libcalc.so.1.0.0 libcalc.so.1    # soname → real name
ln -sf libcalc.so.1 libcalc.so           # linker name → soname

# Resultado:
ls -la libcalc*
# libcalc.so       → libcalc.so.1        (linker name)
# libcalc.so.1     → libcalc.so.1.0.0    (soname)
# libcalc.so.1.0.0                        (real name — archivo físico)
```

Cada nombre tiene un consumidor diferente:

```
┌────────────────┬──────────────────────┬───────────────────────┐
│ Nombre         │ Quién lo usa         │ Cuándo                │
├────────────────┼──────────────────────┼───────────────────────┤
│ Linker name    │ gcc (-lcalc)         │ Compilación           │
│ libcalc.so     │                      │                       │
├────────────────┼──────────────────────┼───────────────────────┤
│ Soname         │ ld-linux.so          │ Runtime (carga)       │
│ libcalc.so.1   │ (cargador dinámico)  │                       │
├────────────────┼──────────────────────┼───────────────────────┤
│ Real name      │ Sistema de archivos  │ Almacenamiento        │
│ libcalc.so.1.0 │ (archivo en disco)   │                       │
└────────────────┴──────────────────────┴───────────────────────┘
```

### Qué pasa en la compilación

```
gcc main.c -L. -lcalc -o program

1. gcc busca "libcalc.so" (-lcalc → lib + calc + .so)
2. Encuentra el symlink libcalc.so → libcalc.so.1 → libcalc.so.1.0.0
3. Lee el SONAME embebido en libcalc.so.1.0.0: "libcalc.so.1"
4. Graba "libcalc.so.1" en el campo NEEDED del ejecutable

En runtime:
1. ld-linux.so lee NEEDED: "libcalc.so.1"
2. Busca un archivo/symlink llamado "libcalc.so.1" en las rutas de búsqueda
3. Lo encuentra → carga la biblioteca
```

---

## 6 — Verificar soname con readelf y objdump

Dos herramientas para inspeccionar el soname:

```bash
# En la biblioteca — verificar qué soname tiene embebido:
readelf -d libcalc.so.1.0.0 | grep SONAME
# 0x000000000000000e (SONAME)  Library soname: [libcalc.so.1]

objdump -p libcalc.so.1.0.0 | grep SONAME
#   SONAME               libcalc.so.1
```

```bash
# En el ejecutable — verificar qué soname busca:
readelf -d program | grep NEEDED
# 0x0000000000000001 (NEEDED)  Shared library: [libcalc.so.1]
# 0x0000000000000001 (NEEDED)  Shared library: [libc.so.6]
```

```bash
# En runtime — verificar resolución real:
ldd program
#   libcalc.so.1 => ./libcalc.so.1.0.0 (0x7f...)
#   libc.so.6 => /lib64/libc.so.6 (0x7f...)

# ldd muestra: soname => archivo real resuelto (dirección de carga)
```

### Flujo completo de verificación

```bash
# 1. ¿Qué soname tiene la biblioteca?
readelf -d libcalc.so.1.0.0 | grep SONAME

# 2. ¿Qué soname busca el programa?
readelf -d program | grep NEEDED

# 3. ¿A qué archivo real se resuelve?
LD_LIBRARY_PATH=. ldd program

# 4. ¿Los symlinks están bien?
ls -la libcalc*

# Si el soname embebido (paso 1) coincide con lo que busca
# el programa (paso 2), y el symlink existe (paso 4),
# ldd (paso 3) mostrará la resolución correcta.
```

---

## 7 — Actualización compatible: minor/patch bump

Una actualización compatible **mantiene el mismo soname** (mismo major). Solo cambia el real name.

### Escenario: v1.0.0 → v1.1.0 (agrega `calc_sub`)

```bash
# Estado inicial:
# program_v1 fue compilado con libcalc.so.1.0.0
# program_v1 busca "libcalc.so.1" (soname)

# Compilar la nueva versión:
gcc -std=c11 -Wall -Wextra -Wpedantic -c -fPIC calc_v1_minor.c -o calc_v1_minor.o
gcc -shared -Wl,-soname,libcalc.so.1 -o libcalc.so.1.1.0 calc_v1_minor.o
#                        ^^^^^^^^^^^
# El soname sigue siendo libcalc.so.1 (misma ABI major)

# Actualizar el symlink del soname:
ln -sf libcalc.so.1.1.0 libcalc.so.1

# Estado resultante:
# libcalc.so   → libcalc.so.1      (sin cambio)
# libcalc.so.1 → libcalc.so.1.1.0  (ACTUALIZADO)
# libcalc.so.1.0.0                   (versión anterior, en disco)
# libcalc.so.1.1.0                   (versión nueva)
```

```bash
# Ejecutar el programa SIN recompilar:
LD_LIBRARY_PATH=. ./program_v1
# calc_add(10, 3) = 13
# calc_mul(10, 3) = 30
# ¡Funciona! El programa no sabe que la biblioteca cambió.

# Verificar la resolución:
LD_LIBRARY_PATH=. ldd program_v1
# libcalc.so.1 => ./libcalc.so.1.1.0
#                              ^^^^^
# Ahora resuelve a 1.1.0, no a 1.0.0
```

### Por qué funciona

```
1. program_v1 busca "libcalc.so.1" (grabado en NEEDED)
2. libcalc.so.1 ahora apunta a libcalc.so.1.1.0
3. libcalc.so.1.1.0 tiene SONAME = "libcalc.so.1" (mismo major)
4. Todas las funciones que program_v1 usa (calc_add, calc_mul) existen
   con las mismas firmas → ABI compatible → funciona

La función nueva (calc_sub) existe en 1.1.0 pero program_v1 no la usa.
Un programa nuevo compilado contra 1.1.0 sí puede usarla.
```

### Verificar que ambas versiones comparten soname

```bash
readelf -d libcalc.so.1.0.0 | grep SONAME
# (SONAME)  Library soname: [libcalc.so.1]

readelf -d libcalc.so.1.1.0 | grep SONAME
# (SONAME)  Library soname: [libcalc.so.1]

# Mismo soname → la promesa de compatibilidad se mantiene.
```

---

## 8 — Actualización incompatible: major bump y coexistencia

Cuando un cambio rompe la ABI (firma de función diferente, struct con layout diferente, función eliminada), se incrementa el major y se crea un **soname nuevo**.

### Escenario: v1 → v2 (`calc_add` pasa de 2 a 3 parámetros)

```c
// v1: int calc_add(int a, int b);       → soname libcalc.so.1
// v2: int calc_add(int a, int b, int c); → soname libcalc.so.2
//                              ^^^^^^
// Cambio de firma = ABI incompatible = major nuevo
```

```bash
# Compilar v2:
gcc -std=c11 -Wall -Wextra -Wpedantic -c -fPIC calc_v2.c -o calc_v2.o
gcc -shared -Wl,-soname,libcalc.so.2 -o libcalc.so.2.0.0 calc_v2.o
#                        ^^^^^^^^^^^
# Soname NUEVO: libcalc.so.2

# Crear symlinks para v2:
ln -sf libcalc.so.2.0.0 libcalc.so.2   # soname v2
ln -sf libcalc.so.2 libcalc.so          # linker name → v2 (para nuevas compilaciones)
```

### Coexistencia en el sistema de archivos

```
directorio/
├── libcalc.so           → libcalc.so.2       (linker: compila contra v2)
├── libcalc.so.1         → libcalc.so.1.1.0   (soname v1: programas v1)
├── libcalc.so.1.0.0                            (real name v1.0)
├── libcalc.so.1.1.0                            (real name v1.1)
├── libcalc.so.2         → libcalc.so.2.0.0   (soname v2: programas v2)
└── libcalc.so.2.0.0                            (real name v2.0)
```

### Ambos programas funcionan simultáneamente

```bash
# program_v1 (compilado con v1):
readelf -d program_v1 | grep NEEDED
# (NEEDED)  Shared library: [libcalc.so.1]

# program_v2 (compilado con v2):
readelf -d program_v2 | grep NEEDED
# (NEEDED)  Shared library: [libcalc.so.2]

# Ejecutar ambos:
LD_LIBRARY_PATH=. ./program_v1
# calc_add(10, 3) = 13       ← usa libcalc.so.1 (2 args)

LD_LIBRARY_PATH=. ./program_v2
# calc_add(10, 3, 7) = 20    ← usa libcalc.so.2 (3 args)
```

Cada programa carga su versión. No hay conflicto porque buscan sonames **diferentes**.

### Qué pasa si se elimina un soname

```bash
rm libcalc.so.1
LD_LIBRARY_PATH=. ./program_v1
# error: libcalc.so.1: cannot open shared object file: No such file or directory

LD_LIBRARY_PATH=. ./program_v2
# calc_add(10, 3, 7) = 20    ← no afectado, busca libcalc.so.2

# Restaurar:
ln -sf libcalc.so.1.1.0 libcalc.so.1
LD_LIBRARY_PATH=. ./program_v1
# calc_add(10, 3) = 13       ← restaurado
```

El symlink del soname es el punto crítico: si desaparece, todos los programas que dependen de ese major fallan.

---

## 9 — ldconfig y gestión del sistema

En un entorno de desarrollo local se usan symlinks manuales y `LD_LIBRARY_PATH`. En producción, `ldconfig` automatiza la gestión.

### Instalación con ldconfig

```bash
# Copiar la biblioteca al directorio del sistema:
sudo cp libcalc.so.1.2.3 /usr/local/lib/

# ldconfig crea el symlink del soname y actualiza el caché:
sudo ldconfig
# Automáticamente:
# 1. Lee el SONAME embebido en libcalc.so.1.2.3 → "libcalc.so.1"
# 2. Crea: /usr/local/lib/libcalc.so.1 → libcalc.so.1.2.3
# 3. Actualiza /etc/ld.so.cache

# Verificar:
ldconfig -p | grep calc
# libcalc.so.1 (libc6,x86-64) => /usr/local/lib/libcalc.so.1.2.3
```

**Nota**: `ldconfig` crea el **soname symlink** automáticamente, pero **no** el **linker name**. El linker name (`libcalc.so`) lo proporciona el paquete `-dev`/`-devel` o se crea manualmente:

```bash
sudo ln -sf libcalc.so.1 /usr/local/lib/libcalc.so
```

### Actualización compatible con ldconfig

```bash
# Instalar nueva versión:
sudo cp libcalc.so.1.3.0 /usr/local/lib/
sudo ldconfig
# ldconfig actualiza: libcalc.so.1 → libcalc.so.1.3.0
# Todos los programas usan la nueva versión automáticamente.
```

### Actualización incompatible con ldconfig

```bash
# Instalar major nueva:
sudo cp libcalc.so.2.0.0 /usr/local/lib/
sudo ldconfig
# Crea NUEVO symlink: libcalc.so.2 → libcalc.so.2.0.0
# MANTIENE: libcalc.so.1 → libcalc.so.1.3.0
# Ambas coexisten.

# Actualizar linker name para nuevas compilaciones:
sudo ln -sf libcalc.so.2 /usr/local/lib/libcalc.so
```

---

## 10 — Cuándo cambiar cada componente de versión

### Cambio INCOMPATIBLE → MAJOR++ (soname nuevo)

```c
// Rompe ABI — programas existentes fallan sin recompilar:
// - Eliminar una función pública
// - Cambiar la firma de una función (parámetros, tipo de retorno)
// - Cambiar el layout de un struct público (agregar/quitar/reordenar campos)
// - Cambiar el tamaño de un tipo público (sizeof cambia)
// - Cambiar el significado de una constante o enum existente
// - Cambiar la calling convention
```

### Cambio compatible → MINOR++ (mismo soname)

```c
// Extiende ABI — programas existentes siguen funcionando:
// - Agregar una función nueva
// - Agregar un nuevo valor de enum (al final)
// - Agregar nuevas constantes o macros
// - Agregar nuevos typedefs
```

### Bug fix → PATCH++ (mismo soname)

```c
// No cambia la interfaz:
// - Corregir un bug en la implementación
// - Optimización interna
// - Mejorar mensajes de error
// - Corregir documentación
```

### Caso especial: structs opacos

```c
// Si la biblioteca expone el struct solo como puntero opaco:
// struct CalcCtx;  // forward declaration en el header
// CalcCtx *calc_ctx_new(void);
// void calc_ctx_free(CalcCtx *ctx);

// Entonces agregar campos al struct es COMPATIBLE (minor++),
// porque el usuario nunca ve sizeof(CalcCtx) ni accede a los campos.
// Solo la biblioteca aloca y libera la estructura.

// Pero si el struct es público (campos visibles en el header),
// cualquier cambio de layout es INCOMPATIBLE (major++),
// porque sizeof() cambia y los usuarios pueden alocar en stack.
```

---

## Ejercicios

### Ejercicio 1 — Los tres nombres desde cero

Crea `libmath.so.1.0.0` con una función `math_square(int x)` que retorna `x * x`.
Usa el soname `libmath.so.1`. Crea los tres symlinks. Verifica con `readelf -d` que el soname está embebido. Verifica con `ls -la` la cadena completa.

```bash
# Archivo: math_ops.h
# int math_square(int x);

# Archivo: math_ops.c
# #include "math_ops.h"
# int math_square(int x) { return x * x; }

gcc -std=c11 -Wall -Wextra -Wpedantic -c -fPIC math_ops.c -o math_ops.o
gcc -shared -Wl,-soname,libmath.so.1 -o libmath.so.1.0.0 math_ops.o
ln -sf libmath.so.1.0.0 libmath.so.1
ln -sf libmath.so.1 libmath.so
readelf -d libmath.so.1.0.0 | grep SONAME
ls -la libmath*
```

<details><summary>Predicción</summary>

`readelf` mostrará `Library soname: [libmath.so.1]`. `ls -la` mostrará:
- `libmath.so → libmath.so.1` (linker name)
- `libmath.so.1 → libmath.so.1.0.0` (soname)
- `libmath.so.1.0.0` (archivo real)

Solo `libmath.so.1.0.0` tiene tamaño real (~16K). Los otros son symlinks (~11-15 bytes).
</details>

### Ejercicio 2 — NEEDED vs SONAME

Compila un programa `test_math.c` que use `math_square(7)` y enlázalo con `-lmath`. Inspecciona el ejecutable con `readelf -d | grep NEEDED`. ¿Aparece `libmath.so`, `libmath.so.1`, o `libmath.so.1.0.0`?

```bash
# test_math.c: printf("7² = %d\n", math_square(7));
gcc -std=c11 -Wall -Wextra -Wpedantic test_math.c -L. -lmath -o test_math
readelf -d test_math | grep NEEDED
```

<details><summary>Predicción</summary>

El campo NEEDED mostrará `libmath.so.1` — el **soname**, no el linker name (`libmath.so`) ni el real name (`libmath.so.1.0.0`). El linker siguió la cadena de symlinks hasta el archivo real, leyó el SONAME embebido, y lo grabó como dependencia. Esto es lo que `ld-linux.so` buscará en runtime.
</details>

### Ejercicio 3 — Sin soname

Crea una biblioteca **sin** `-Wl,-soname`: `gcc -shared -o libnosn.so nosn.o`. Compila un programa contra ella. ¿Qué aparece en NEEDED? ¿Qué implicaciones tiene para las actualizaciones?

```bash
gcc -std=c11 -c -fPIC math_ops.c -o nosn.o
gcc -shared -o libnosn.so nosn.o
gcc test_math.c -L. -lnosn -o test_nosn
readelf -d libnosn.so | grep SONAME
readelf -d test_nosn | grep NEEDED
```

<details><summary>Predicción</summary>

`readelf -d libnosn.so | grep SONAME` no mostrará nada — no hay soname embebido. `readelf -d test_nosn | grep NEEDED` mostrará `libnosn.so` directamente (el nombre del archivo que el linker encontró). Sin soname, no hay mecanismo de versionado: si reemplazas `libnosn.so` con una versión incompatible, el programa cargará la versión nueva y puede crashear.
</details>

### Ejercicio 4 — Actualización minor sin recompilar

Con la `libmath.so.1.0.0` del ejercicio 1, agrega una función `math_cube(int x)` en una versión `1.1.0`. Actualiza el symlink del soname. Ejecuta el programa original sin recompilar y verifica con `ldd` que usa la versión nueva.

```bash
# math_ops_v1_1.c: incluye math_square + math_cube
gcc -std=c11 -c -fPIC math_ops_v1_1.c -o math_ops_v1_1.o
gcc -shared -Wl,-soname,libmath.so.1 -o libmath.so.1.1.0 math_ops_v1_1.o
ln -sf libmath.so.1.1.0 libmath.so.1
LD_LIBRARY_PATH=. ./test_math
LD_LIBRARY_PATH=. ldd test_math | grep libmath
```

<details><summary>Predicción</summary>

El programa funciona sin recompilar. `ldd` muestra `libmath.so.1 => ./libmath.so.1.1.0` — el soname se resolvió al archivo nuevo. La función `math_cube` existe en la biblioteca pero el programa no la usa, así que no hay diferencia visible en la salida. Si compilas un programa nuevo que usa `math_cube`, enlazará correctamente.
</details>

### Ejercicio 5 — Major bump y coexistencia

Cambia `math_square` para que tome dos parámetros: `math_square(int x, int power)`. Esto es un cambio de ABI. Crea `libmath.so.2.0.0` con soname `libmath.so.2`. Compila un programa nuevo contra v2. Ejecuta ambos programas simultáneamente.

```bash
# math_ops_v2.c: int math_square(int x, int power) { ... }
gcc -shared -Wl,-soname,libmath.so.2 -o libmath.so.2.0.0 math_ops_v2.o
ln -sf libmath.so.2.0.0 libmath.so.2
ln -sf libmath.so.2 libmath.so
gcc test_math_v2.c -L. -lmath -o test_math_v2
LD_LIBRARY_PATH=. ./test_math      # usa libmath.so.1
LD_LIBRARY_PATH=. ./test_math_v2   # usa libmath.so.2
```

<details><summary>Predicción</summary>

Ambos programas funcionan. `test_math` (compilado contra v1) busca `libmath.so.1` → resuelve a `libmath.so.1.1.0`. `test_math_v2` (compilado contra v2) busca `libmath.so.2` → resuelve a `libmath.so.2.0.0`. No hay conflicto porque los sonames son diferentes. `readelf -d test_math | grep NEEDED` muestra `libmath.so.1`; `readelf -d test_math_v2 | grep NEEDED` muestra `libmath.so.2`.
</details>

### Ejercicio 6 — Rotura por soname eliminado

Con los dos programas del ejercicio 5, elimina el symlink `libmath.so.1` y ejecuta ambos. ¿Cuál falla? Restaura el symlink y verifica.

```bash
rm libmath.so.1
LD_LIBRARY_PATH=. ./test_math
LD_LIBRARY_PATH=. ./test_math_v2
ln -sf libmath.so.1.1.0 libmath.so.1
LD_LIBRARY_PATH=. ./test_math
```

<details><summary>Predicción</summary>

`test_math` falla con `error while loading shared libraries: libmath.so.1: cannot open shared object file`. `test_math_v2` funciona normalmente porque busca `libmath.so.2`, que sigue intacto. Después de restaurar el symlink, `test_math` vuelve a funcionar. El symlink del soname es el punto crítico de cada major version.
</details>

### Ejercicio 7 — Inspeccionar bibliotecas del sistema

Usa `readelf` para examinar el soname de una biblioteca real del sistema (como `libz`, `libssl`, o `libcurl`). Identifica los tres nombres y confirma la cadena de symlinks.

```bash
# Encontrar la biblioteca:
ldconfig -p | grep libz
# o: find /usr/lib64 -name "libz*" -o -name "libz*" 2>/dev/null

# Inspeccionar:
readelf -d /usr/lib64/libz.so.* | grep SONAME
ls -la /usr/lib64/libz*
```

<details><summary>Predicción</summary>

En Fedora, verás algo como:
- `libz.so → libz.so.1` o `libz.so → libz.so.1.x.x` (linker name, del paquete `zlib-devel`)
- `libz.so.1 → libz.so.1.x.x` (soname, creado por ldconfig)
- `libz.so.1.x.x` (real name, archivo físico)

El SONAME embebido será `libz.so.1`. La versión exacta (x.x) depende de la versión de zlib instalada. El linker name solo existe si el paquete de desarrollo (`zlib-devel`) está instalado.
</details>

### Ejercicio 8 — Función calc_build para rastreo

Agrega una función `const char *math_version(void)` a tu biblioteca que retorna un string como `"1.0.0"`. Compila v1.0.0 y v1.1.0 con strings diferentes. Actualiza el symlink y ejecuta el mismo programa — observa que la versión reportada cambia sin recompilar.

```bash
# Programa: printf("Library version: %s\n", math_version());
# v1.0.0: return "1.0.0";
# v1.1.0: return "1.1.0";
```

<details><summary>Predicción</summary>

Con el symlink apuntando a v1.0.0: imprime `Library version: 1.0.0`. Después de `ln -sf libmath.so.1.1.0 libmath.so.1` e ejecutar sin recompilar: imprime `Library version: 1.1.0`. Esto demuestra que el programa carga código diferente en cada ejecución según a dónde apunta el symlink. El string está en la sección `.rodata` de la biblioteca, no del ejecutable.
</details>

### Ejercicio 9 — Automatizar con Makefile

Crea un Makefile con targets `v1_0`, `v1_1`, `v2_0`, `program_v1`, `program_v2`, y `clean`. Usa RUNPATH (`-Wl,-rpath,'$$ORIGIN/../lib'`) para que los programas encuentren las bibliotecas sin `LD_LIBRARY_PATH`. Organiza la salida en `runtime/lib/` y `runtime/bin/`.

```makefile
# Estructura esperada:
# runtime/
# ├── bin/
# │   ├── program_v1
# │   └── program_v2
# └── lib/
#     ├── libmath.so → libmath.so.2
#     ├── libmath.so.1 → libmath.so.1.1.0
#     ├── libmath.so.1.0.0
#     ├── libmath.so.1.1.0
#     ├── libmath.so.2 → libmath.so.2.0.0
#     └── libmath.so.2.0.0
```

<details><summary>Predicción</summary>

Con RUNPATH configurado como `$ORIGIN/../lib`, los ejecutables en `runtime/bin/` buscarán bibliotecas en `runtime/lib/` automáticamente. `ldd runtime/bin/program_v1` mostrará la resolución a `runtime/lib/libmath.so.1.x.x` sin necesidad de `LD_LIBRARY_PATH`. El `$$ORIGIN` necesita doble `$` en Makefile porque Make interpreta `$` como variable; el shell recibe `$ORIGIN` que luego el cargador dinámico expande al directorio del ejecutable.
</details>

### Ejercicio 10 — Diagnóstico con LD_DEBUG

Usa `LD_DEBUG=libs` para observar cómo el cargador dinámico resuelve las dependencias de un programa con soname. Luego usa `LD_DEBUG=bindings` para ver las resoluciones de símbolos.

```bash
LD_DEBUG=libs LD_LIBRARY_PATH=. ./test_math 2>&1 | grep -i calc
LD_DEBUG=bindings LD_LIBRARY_PATH=. ./test_math 2>&1 | grep math_square
```

<details><summary>Predicción</summary>

`LD_DEBUG=libs` mostrará la búsqueda del cargador: intenta encontrar `libmath.so.1` en cada directorio de la ruta de búsqueda, y muestra dónde lo encuentra (`.`). `LD_DEBUG=bindings` mostrará la resolución del símbolo `math_square`: binding de `test_math` a `libmath.so.1` para la función `math_square`. La salida va a stderr (por eso `2>&1`). Ambos modos generan mucha salida (incluyendo libc), de ahí el `grep` para filtrar.
</details>
