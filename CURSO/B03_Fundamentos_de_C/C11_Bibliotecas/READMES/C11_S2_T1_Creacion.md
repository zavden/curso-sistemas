# T01 — Creación de bibliotecas dinámicas

## Erratas detectadas en el material base

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.md` | 63–66 | `readelf -d vec3.o \| grep TEXTREL` se presenta como forma de verificar si un `.o` es PIC. Pero `readelf -d` muestra la sección `.dynamic`, que **no existe en archivos `.o`** — solo existe en `.so` y ejecutables dinámicos. El comando produce salida vacía para cualquier `.o`, sea PIC o no, dando un falso resultado "correcto". | Para verificar PIC en un `.o`, inspeccionar los tipos de relocación con `readelf -r`: buscar `R_X86_64_PLT32` y `R_X86_64_GOTPCRELX` (PIC) vs relocaciones absolutas. La verificación de TEXTREL solo tiene sentido sobre el `.so` final: `readelf -d libvec3.so \| grep TEXTREL`. |

---

## 1. ¿Qué es una biblioteca dinámica (.so)?

Una biblioteca dinámica (shared library, shared object) es un archivo
`.so` que se carga en memoria en **runtime** — no se copia dentro del
ejecutable como una `.a`:

```
Compilación:
  vec3.c → gcc -c -fPIC → vec3.o ─→ gcc -shared → libvec3.so

Enlace:
  main.o + libvec3.so → gcc → programa
  (el programa solo guarda una REFERENCIA a libvec3.so)

Runtime:
  programa se ejecuta → ld-linux.so carga libvec3.so → resuelve símbolos
```

### Convención de nombres

```
lib<nombre>.so                           ← linker name (symlink)
lib<nombre>.so.<major>                   ← soname (symlink)
lib<nombre>.so.<major>.<minor>.<patch>   ← real name (archivo)

Ejemplo:
libvec3.so      → libvec3.so.1           (symlink para gcc -lvec3)
libvec3.so.1    → libvec3.so.1.2.3       (symlink para ld-linux)
libvec3.so.1.2.3                         (archivo real)
```

Por ahora creamos sin versionado. El esquema de soname se cubre en T03.

### Diferencia fundamental con `.a`

| Aspecto | `.a` (estática) | `.so` (dinámica) |
|---------|----------------|-----------------|
| En enlace | Código **copiado** al ejecutable | Solo se guarda una **referencia** |
| En runtime | No necesita la biblioteca | Necesita la `.so` presente |
| Tamaño del ejecutable | Mayor | Menor |
| Memoria con N procesos | N copias | 1 copia compartida |

---

## 2. Position-Independent Code (PIC) y `-fPIC`

Para que una biblioteca `.so` pueda cargarse en **cualquier dirección**
de memoria (necesario con ASLR y para compartir entre procesos), su
código debe ser PIC:

```bash
gcc -c -fPIC vec3.c -o vec3.o
```

### ¿Qué cambia `-fPIC`?

```
Sin PIC — dirección absoluta:
  mov eax, [0x402000]          ← dirección fija, solo funciona en esa posición

Con PIC — relativo al Program Counter:
  lea rax, [rip + offset]     ← relativo a la instrucción actual
  → funciona sin importar dónde se cargó en memoria
```

Para funciones y datos **externos** (como `printf`, `sqrt`), PIC usa la
**GOT** (Global Offset Table):

1. El código accede a la GOT (posición conocida, relativa al código)
2. La GOT contiene la dirección real de la función/dato
3. El linker dinámico (`ld-linux.so`) llena la GOT al cargar la biblioteca

### `-fPIC` vs `-fpic`

- `-fPIC`: genera PIC sin restricciones de tamaño — **usar siempre**
- `-fpic`: PIC más eficiente pero con límite de tamaño de GOT (depende
  de la arquitectura). Rara vez se necesita.

### Nota sobre x86-64

En x86-64, GCC genera código con direccionamiento relativo al RIP por
defecto en la mayoría de distribuciones modernas. La diferencia entre
compilar con y sin `-fPIC` puede ser mínima en las instrucciones, pero
los tipos de **relocación** cambian: con `-fPIC` se usan `R_X86_64_PLT32`
y `R_X86_64_GOTPCRELX`, con relocaciones absolutas sin él.

---

## 3. Crear el `.so` con `gcc -shared`

### Paso a paso

```bash
# 1. Compilar con -fPIC:
gcc -std=c11 -Wall -Wextra -Wpedantic -c -fPIC vec3.c -o vec3.o

# 2. Crear el .so:
gcc -shared -o libvec3.so vec3.o -lm

# En un solo paso:
gcc -std=c11 -shared -fPIC -o libvec3.so vec3.c -lm
```

`-shared` produce un ELF de tipo `ET_DYN` (shared object) en vez de
`ET_EXEC` (executable).

El `-lm` al crear el `.so` es importante: registra `libm.so.6` como
dependencia (`NEEDED`) del `.so`. Sin él, `sqrt` queda como símbolo
indefinido sin indicar de dónde viene, lo que puede causar problemas
en runtime.

### Verificar

```bash
file libvec3.so
# libvec3.so: ELF 64-bit LSB shared object, x86-64, ...

nm -D libvec3.so     # símbolos dinámicos (visibles para otros)
ldd libvec3.so       # dependencias
```

---

## 4. Inspeccionar: `file`, `nm -D`, `readelf -d`

### `file` — tipo de archivo

```bash
file vec3_pic.o     # ELF 64-bit LSB relocatable (objeto)
file libvec3.a      # current ar archive (contenedor de .o)
file libvec3.so     # ELF 64-bit LSB shared object (cargable en runtime)
```

### `nm -D` — símbolos dinámicos (exportados)

```bash
nm -D libvec3.so
```

```
                 U printf         ← Undefined: se resuelve en runtime
                 U sqrt           ← Undefined: se resuelve cargando libm
<addr> T vec3_add                 ← Text: exportada, visible para otros
<addr> T vec3_create
<addr> T vec3_dot
<addr> T vec3_length
<addr> T vec3_print
```

- `nm -D` muestra solo la tabla dinámica (`.dynsym`) — lo que otros
  programas y bibliotecas pueden ver.
- `nm` (sin `-D`) muestra **todos** los símbolos, incluyendo internos.
- `T` = definido y exportado, `U` = indefinido (dependencia), `w` = weak.

### `readelf -d` — sección `.dynamic`

```bash
readelf -d libvec3.so
```

```
Dynamic section at offset ... contains ... entries:
  Tag        Type         Name/Value
  NEEDED     Shared library: [libm.so.6]
  NEEDED     Shared library: [libc.so.6]
  SYMTAB     ...
  STRTAB     ...
  ...
```

`NEEDED` lista las dependencias del `.so`. Cuando el linker dinámico carga
`libvec3.so`, sabe que también debe cargar `libm.so.6` y `libc.so.6`.

---

## 5. TEXTREL: qué pasa sin `-fPIC`

Si compilas un `.o` sin `-fPIC` e intentas crear un `.so`:

```bash
gcc -c vec3.c -o vec3_nopic.o           # sin -fPIC
gcc -shared -o libvec3_nopic.so vec3_nopic.o -lm
```

Dos posibles resultados según la distribución:

**Error** (distribuciones modernas / hardened):
```
relocation R_X86_64_PC32 against symbol 'printf@@GLIBC_2.2.5'
can not be used when making a shared object; recompile with -fPIC
```

**Éxito con TEXTREL** (distribuciones permisivas):
```bash
readelf -d libvec3_nopic.so | grep TEXTREL
# TEXTREL    0x0
# FLAGS      TEXTREL
```

### ¿Por qué TEXTREL es malo?

`TEXTREL` (text relocations) significa que el linker dinámico necesita
**modificar** las páginas de código al cargar la biblioteca:

- Las páginas de código dejan de ser read-only → no se pueden compartir
  entre procesos (cada uno necesita su propia copia parcheada)
- ASLR pierde efectividad en esas páginas
- Algunos sistemas (Android, Fedora con SELinux) **rechazan** cargar
  `.so` con TEXTREL por seguridad

Un `.so` compilado con `-fPIC` nunca tiene TEXTREL.

---

## 6. Dependencias del `.so` (NEEDED)

Al crear un `.so`, es importante enlazar con las bibliotecas que necesita:

```bash
# Sin -lm: sqrt queda como U sin NEEDED de libm
gcc -shared -o libvec3.so vec3_pic.o
readelf -d libvec3.so | grep NEEDED
# Solo: libc.so.6

# Con -lm: sqrt tiene NEEDED de libm
gcc -shared -o libvec3.so vec3_pic.o -lm
readelf -d libvec3.so | grep NEEDED
# libm.so.6
# libc.so.6
```

Sin la dependencia `NEEDED`, el linker dinámico **no cargará** `libm`
automáticamente. `sqrt` solo se resolverá si otro módulo (el programa
principal u otra `.so`) ya cargó `libm`. Esto crea dependencias
implícitas frágiles.

**Buena práctica**: siempre enlazar la `.so` con todas sus dependencias
(`-lm`, `-lpthread`, etc.) para que `readelf -d` refleje la verdad.

---

## 7. Visibilidad de símbolos

Por defecto, **todas** las funciones de un `.so` son visibles (exportadas).
Esto puede ser un problema: funciones internas quedan expuestas, la tabla
de símbolos crece (carga más lenta), y pueden colisionar con otros `.so`.

### Solución 1: `__attribute__((visibility))`

```c
__attribute__((visibility("default")))   // visible (exportada)
void public_func(void) { }

__attribute__((visibility("hidden")))    // oculta
void internal_helper(void) { }
```

### Solución 2: `-fvisibility=hidden` + macro de exportación

```bash
gcc -c -fPIC -fvisibility=hidden -DBUILDING_MYLIB vec3.c -o vec3.o
```

```c
// En el header:
#ifdef BUILDING_MYLIB
    #define MYLIB_API __attribute__((visibility("default")))
#else
    #define MYLIB_API
#endif

MYLIB_API Vec3 vec3_create(double x, double y, double z);
MYLIB_API Vec3 vec3_add(Vec3 a, Vec3 b);
// Funciones sin MYLIB_API quedan ocultas.
```

### Solución 3: version script

```bash
# libvec3.map:
# {
#     global: vec3_create; vec3_add; vec3_dot; vec3_length; vec3_print;
#     local: *;
# };

gcc -shared -Wl,--version-script=libvec3.map -o libvec3.so vec3.o -lm
```

Solo los símbolos en `global` se exportan. Todo lo demás queda oculto.

---

## 8. Comparación `.o` vs `.a` vs `.so`

| Aspecto | `.o` (objeto) | `.a` (estática) | `.so` (dinámica) |
|---------|--------------|----------------|-----------------|
| Tipo ELF | `relocatable` | ar archive | `shared object` |
| Contenido | Código de 1 archivo | Colección de `.o` | Código + secciones dinámicas |
| Tamaño típico | ~3.5 KB | ~3.7 KB | ~16 KB |
| Secciones extra | — | — | `.dynsym`, `.dynstr`, `.plt`, `.got`, `.dynamic` |
| Uso | Input para linker | Se copia al ejecutable | Se carga en runtime |

El `.so` es más grande que el `.o` porque contiene toda la infraestructura
de enlace dinámico: tabla de símbolos dinámicos, strings de nombres, PLT
(Procedure Linkage Table), GOT (Global Offset Table), hash table, etc.

```bash
# Contar secciones:
readelf -S libvec3.so | grep -c '\['    # ~25-30 secciones
readelf -S vec3_pic.o | grep -c '\['    # ~12-15 secciones
```

---

## 9. Makefile para biblioteca dinámica

```makefile
CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC -Iinclude

SRC = src/vec3.c
OBJ = $(SRC:.c=.o)
LIB = lib/libvec3.so

all: $(LIB)

$(LIB): $(OBJ) | lib
	$(CC) -shared -o $@ $^ -lm

lib:
	mkdir -p lib

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Programa de prueba:
test: main.c $(LIB)
	$(CC) -std=c11 -Wall -Iinclude main.c -Llib -lvec3 -lm -o $@
	LD_LIBRARY_PATH=lib ./$@

clean:
	rm -f $(OBJ) $(LIB) test

.PHONY: all test clean
```

Puntos clave:
- `-fPIC` va en `CFLAGS` (para los `.o`)
- `-shared` va en la regla del `.so` (para el linker)
- `LD_LIBRARY_PATH=lib` es necesario para que el linker dinámico encuentre
  la `.so` al ejecutar (alternativas: `rpath`, instalar en `/usr/lib64/`,
  o `ldconfig`)

---

## 10. Comparación con Rust

| Aspecto | C (`.so`) | Rust |
|---------|----------|------|
| Biblioteca para C/FFI | `.so` con `gcc -shared` | `cdylib` en `Cargo.toml` |
| Biblioteca para Rust | No aplica | `rlib` (estática por defecto) |
| PIC | `-fPIC` manual | Automático para `cdylib` |
| Visibilidad | `-fvisibility=hidden` + macros | `pub` en API, todo lo demás es privado |
| Dependencias | `-lm`, `-l...` manual | `cargo` resuelve todo |
| Versionado | soname manual | `Cargo.toml` + semver |

```rust
// Cargo.toml para crear un .so equivalente:
// [lib]
// crate-type = ["cdylib"]    ← produce libfoo.so

// Rust exporta funciones con #[no_mangle]:
#[no_mangle]
pub extern "C" fn vec3_add(ax: f64, ay: f64, az: f64,
                           bx: f64, by: f64, bz: f64)
                           -> (f64, f64, f64) {
    (ax + bx, ay + by, az + bz)
}
```

En Rust, la visibilidad y el PIC se manejan automáticamente. En C, son
responsabilidad del programador.

---

## Ejercicios

### Ejercicio 1 — Compilar con `-fPIC` e inspeccionar

Compila `vec3.c` con y sin `-fPIC`. Compara los tipos de relocación.

```bash
cd labs/
gcc -std=c11 -Wall -Wextra -Wpedantic -c -fPIC vec3.c -o vec3_pic.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c vec3_nopic.c -o vec3_nopic.o

file vec3_pic.o
file vec3_nopic.o

readelf -r vec3_pic.o | head -20
readelf -r vec3_nopic.o | head -20
```

<details><summary>Predicción</summary>

`file` para ambos:
```
vec3_pic.o:   ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
vec3_nopic.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
```

- Ambos son "relocatable" — `file` no distingue PIC de no-PIC.

`readelf -r` del PIC mostrará relocaciones como:
```
R_X86_64_PLT32     printf     ← llamada vía PLT (indirección)
R_X86_64_GOTPCRELX ...        ← acceso vía GOT
```

El no-PIC puede mostrar relocaciones similares en x86-64 (porque GCC
genera código RIP-relative por defecto en esta arquitectura), pero puede
tener relocaciones absolutas en otras. La diferencia es más visible en
arquitecturas de 32 bits.

</details>

---

### Ejercicio 2 — Crear el `.so`

Crea `libvec3.so` a partir del objeto PIC. Enlázalo con `-lm`.

```bash
gcc -shared -o libvec3.so vec3_pic.o -lm
file libvec3.so
```

<details><summary>Predicción</summary>

```
libvec3.so: ELF 64-bit LSB shared object, x86-64, version 1 (SYSV),
dynamically linked, BuildID[sha1]=<hash>, not stripped
```

- Dice "shared object" — no "relocatable" ni "executable".
- "dynamically linked" — tiene dependencias dinámicas (libc, libm).
- `BuildID` es un hash único que identifica esta compilación.

</details>

---

### Ejercicio 3 — Comparar formatos: `.o` vs `.a` vs `.so`

Crea también un `.a` y compara los tres formatos y tamaños.

```bash
ar rcs libvec3.a vec3_pic.o

file vec3_pic.o libvec3.a libvec3.so
ls -lh vec3_pic.o libvec3.a libvec3.so
```

<details><summary>Predicción</summary>

```
vec3_pic.o: ELF 64-bit LSB relocatable, x86-64, ...
libvec3.a:  current ar archive
libvec3.so: ELF 64-bit LSB shared object, x86-64, ...
```

```
~3.5K vec3_pic.o
~3.7K libvec3.a
 ~16K libvec3.so
```

- El `.a` es casi del mismo tamaño que el `.o` (es solo un wrapper).
- El `.so` es ~4-5x más grande por las secciones dinámicas adicionales
  (`.dynsym`, `.dynstr`, `.plt`, `.got`, `.dynamic`, hash table).

</details>

---

### Ejercicio 4 — `nm -D` vs `nm`: símbolos exportados

Compara los símbolos totales con los dinámicos (exportados).

```bash
echo "=== nm -D (dinámicos) ==="
nm -D libvec3.so

echo ""
echo "=== Conteo ==="
echo "Total:     $(nm libvec3.so | wc -l)"
echo "Dinámicos: $(nm -D libvec3.so | wc -l)"
```

<details><summary>Predicción</summary>

`nm -D`:
```
                 w __cxa_finalize
                 w __gmon_start__
                 w _ITM_deregisterTMCloneTable
                 w _ITM_registerTMCloneTable
                 U printf
                 U sqrt
<addr> T vec3_add
<addr> T vec3_create
<addr> T vec3_dot
<addr> T vec3_length
<addr> T vec3_print
```

Conteo: `nm` (total) mostrará ~25-40 símbolos; `nm -D` mostrará ~11.

- `nm -D` solo muestra la tabla `.dynsym` — lo que otros módulos ven.
- `nm` muestra todo, incluyendo símbolos internos del runtime de C
  (funciones de inicialización, `_init`, `_fini`, etc.).
- Los `w` (weak) son símbolos del runtime que el linker dinámico puede
  o no resolver.

</details>

---

### Ejercicio 5 — `readelf -d`: dependencias NEEDED

Verifica qué dependencias declara el `.so`.

```bash
readelf -d libvec3.so | grep NEEDED
```

<details><summary>Predicción</summary>

```
 0x... (NEEDED)   Shared library: [libm.so.6]
 0x... (NEEDED)   Shared library: [libc.so.6]
```

- `libm.so.6` aparece porque enlazamos con `-lm` al crear el `.so`.
- `libc.so.6` siempre aparece (GCC la enlaza automáticamente).
- Si no hubiéramos pasado `-lm`, solo aparecería `libc.so.6` y `sqrt`
  quedaría como dependencia implícita (mala práctica).

</details>

---

### Ejercicio 6 — Intentar crear `.so` sin `-fPIC`

Intenta crear una `.so` con el objeto compilado sin `-fPIC`.

```bash
gcc -shared -o libvec3_nopic.so vec3_nopic.o -lm 2>&1
```

<details><summary>Predicción</summary>

En Fedora (con hardened flags), probablemente error:
```
/usr/bin/ld: vec3_nopic.o: relocation R_X86_64_... against symbol
'printf@@GLIBC_2.2.5' can not be used when making a shared object;
recompile with -fPIC
```

Si tuviera éxito (en distros permisivas):
```bash
readelf -d libvec3_nopic.so | grep TEXTREL
# TEXTREL    0x0
```

- TEXTREL indica que el código tiene relocaciones en la sección de texto.
- Las páginas no son compartibles entre procesos.
- SELinux puede impedir cargar la `.so`.
- En la práctica: **siempre usar `-fPIC`** para código de bibliotecas.

</details>

---

### Ejercicio 7 — Compilar programa con `.so` y ejecutar

Compila `main_vec3.c` enlazando con `libvec3.so`. Ejecútalo.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main_vec3.c -L. -lvec3 -lm -o main_dynamic

# Sin LD_LIBRARY_PATH:
./main_dynamic 2>&1 || true

# Con LD_LIBRARY_PATH:
LD_LIBRARY_PATH=. ./main_dynamic
```

<details><summary>Predicción</summary>

Sin `LD_LIBRARY_PATH`:
```
./main_dynamic: error while loading shared libraries: libvec3.so:
cannot open shared object file: No such file or directory
```

Con `LD_LIBRARY_PATH=.`:
```
a = (1.00, 2.00, 3.00)
b = (4.00, 5.00, 6.00)
a + b = (5.00, 7.00, 9.00)
a . b = 32.00
|a| = 3.7417
```

- Sin `LD_LIBRARY_PATH`, el linker dinámico no sabe dónde buscar
  `libvec3.so` (no está en `/usr/lib64/` ni en rutas estándar).
- `LD_LIBRARY_PATH=.` agrega el directorio actual a la búsqueda.
- Alternativas: `-Wl,-rpath,'$ORIGIN'` (embebe la ruta en el binario),
  instalar la `.so` en `/usr/lib64/`, o ejecutar `ldconfig`.

</details>

---

### Ejercicio 8 — Comparar ejecutable dinámico vs estático

Compila el mismo programa enlazando con `.a` y compara.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main_vec3.c libvec3.a -lm -o main_static

ls -lh main_dynamic main_static
ldd main_dynamic
ldd main_static
```

<details><summary>Predicción</summary>

```
~16K main_dynamic
~20K main_static
```

`ldd main_dynamic`:
```
libvec3.so => not found      ← dependencia del .so
libm.so.6 => /lib64/libm.so.6
libc.so.6 => /lib64/libc.so.6
/lib64/ld-linux-x86-64.so.2
```

`ldd main_static`:
```
libm.so.6 => /lib64/libm.so.6
libc.so.6 => /lib64/libc.so.6
/lib64/ld-linux-x86-64.so.2
```

- `main_static` es más grande (~4K más) porque contiene el código de
  `libvec3` copiado dentro.
- `main_static` **no** lista `libvec3.so` como dependencia — el código
  ya está embebido. Pero sí depende de `libm.so` y `libc.so` (enlazadas
  dinámicamente).
- `main_dynamic` lista `libvec3.so => not found` porque no está en las
  rutas estándar.

</details>

---

### Ejercicio 9 — Símbolos `U` vs `T` en el ejecutable

Compara cómo aparecen los símbolos de vec3 en cada ejecutable.

```bash
echo "=== Dinámico ==="
nm main_dynamic | grep vec3

echo ""
echo "=== Estático ==="
nm main_static | grep vec3
```

<details><summary>Predicción</summary>

Dinámico:
```
                 U vec3_add
                 U vec3_create
                 U vec3_dot
                 U vec3_length
                 U vec3_print
```

Estático:
```
<addr> T vec3_add
<addr> T vec3_create
<addr> T vec3_dot
<addr> T vec3_length
<addr> T vec3_print
```

- En el dinámico, los símbolos son `U` (undefined) — se resolverán cuando
  `ld-linux.so` cargue `libvec3.so` en runtime.
- En el estático, los símbolos son `T` (text) con direcciones asignadas —
  el código fue copiado dentro del ejecutable durante el enlace.
- Esta es la diferencia visible entre enlace estático y dinámico.

</details>

---

### Ejercicio 10 — Contar secciones: `.o` vs `.so`

Compara cuántas secciones ELF tiene cada formato.

```bash
echo "Secciones en .o:  $(readelf -S vec3_pic.o 2>/dev/null | grep -c '\[')"
echo "Secciones en .so: $(readelf -S libvec3.so 2>/dev/null | grep -c '\[')"

# Ver las secciones exclusivas del .so:
readelf -S libvec3.so | grep -E '\.(dyn|plt|got|gnu\.hash)'
```

<details><summary>Predicción</summary>

```
Secciones en .o:  ~13
Secciones en .so: ~27
```

Secciones exclusivas del `.so`:
```
[N] .dynsym         ← tabla de símbolos dinámicos
[N] .dynstr         ← strings de nombres dinámicos
[N] .gnu.hash       ← hash table para búsqueda rápida de símbolos
[N] .plt            ← Procedure Linkage Table (trampolines)
[N] .plt.got        ← PLT para GOT
[N] .got            ← Global Offset Table
[N] .got.plt        ← GOT para PLT
[N] .dynamic        ← metadatos de enlace dinámico (NEEDED, etc.)
```

- El `.so` tiene ~2x más secciones que el `.o`.
- Las secciones extra son la infraestructura de enlace dinámico que permite
  al linker dinámico cargar y resolver símbolos en runtime.
- `.plt` contiene stubs que saltan a la dirección real vía `.got`.
- `.got` contiene las direcciones reales, llenadas por `ld-linux.so`.

</details>

---

### Limpieza

```bash
rm -f vec3_pic.o vec3_nopic.o
rm -f libvec3.so libvec3.a libvec3_nopic.so
rm -f main_static main_dynamic
```
