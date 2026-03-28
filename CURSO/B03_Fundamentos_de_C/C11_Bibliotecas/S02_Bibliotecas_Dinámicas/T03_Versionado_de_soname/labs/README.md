# Lab -- Versionado de soname

## Objetivo

Construir bibliotecas compartidas con soname embebido, crear la cadena de
symlinks (linker name, soname, real name), verificar el soname con `readelf` y
`objdump`, demostrar una actualizacion compatible (minor bump) sin recompilar
el programa, y demostrar la coexistencia de versiones incompatibles (major bump).

## Prerequisitos

- GCC instalado
- `readelf`, `objdump` y `ldd` disponibles
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `calc.h` | Header publico para libcalc ABI v1 |
| `calc_v1.c` | Implementacion v1.0.0 (add, mul) |
| `calc_v1_minor.c` | Implementacion v1.1.0 (agrega sub, compatible) |
| `calc_v2.h` | Header publico para libcalc ABI v2 (API incompatible) |
| `calc_v2.c` | Implementacion v2.0.0 (add cambia a 3 argumentos) |
| `program_v1.c` | Cliente que usa la API v1 |
| `program_v2.c` | Cliente que usa la API v2 |

---

## Parte 1 -- Crear una biblioteca con soname

**Objetivo**: Compilar `libcalc.so.1.0.0` con soname embebido y crear la cadena
de tres nombres (real name, soname, linker name).

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat calc.h
```

Observa que el header declara dos funciones: `calc_add` y `calc_mul`.

```bash
cat calc_v1.c
```

Implementacion simple: `calc_add` suma, `calc_mul` multiplica.

### Paso 1.2 -- Compilar el objeto con -fPIC

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c -fPIC calc_v1.c -o calc_v1.o
```

No debe producir warnings ni errores. El flag `-fPIC` genera codigo
independiente de posicion, necesario para bibliotecas compartidas.

### Paso 1.3 -- Crear la biblioteca con soname embebido

```bash
gcc -shared -Wl,-soname,libcalc.so.1 -o libcalc.so.1.0.0 calc_v1.o
```

Observa los argumentos:

- `-shared`: genera una biblioteca compartida (no un ejecutable)
- `-Wl,-soname,libcalc.so.1`: pasa `-soname libcalc.so.1` al linker
- `-o libcalc.so.1.0.0`: el archivo fisico lleva la version completa (real name)

### Paso 1.4 -- Verificar el soname embebido

Antes de continuar, responde mentalmente:

- Que valor esperas ver en el campo SONAME del archivo `.so`?
- Sera `libcalc.so`, `libcalc.so.1`, o `libcalc.so.1.0.0`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.5 -- Inspeccionar con readelf

```bash
readelf -d libcalc.so.1.0.0 | grep SONAME
```

Salida esperada:

```
 0x000000000000000e (SONAME)             Library soname: [libcalc.so.1]
```

El soname grabado dentro del `.so` es `libcalc.so.1` (solo incluye el major).

### Paso 1.6 -- Inspeccionar con objdump

```bash
objdump -p libcalc.so.1.0.0 | grep SONAME
```

Salida esperada:

```
  SONAME               libcalc.so.1
```

Dos herramientas, mismo resultado. `readelf -d` muestra la seccion `.dynamic`
completa; `objdump -p` muestra los headers del programa incluyendo SONAME.

### Paso 1.7 -- Crear los symlinks

```bash
ln -sf libcalc.so.1.0.0 libcalc.so.1
ln -sf libcalc.so.1 libcalc.so
```

El primer symlink es el **soname** (apunta al real name). El segundo es el
**linker name** (apunta al soname). La cadena completa:

```
libcalc.so --> libcalc.so.1 --> libcalc.so.1.0.0
(linker)       (soname)          (real name)
```

### Paso 1.8 -- Verificar la cadena

```bash
ls -la libcalc*
```

Salida esperada:

```
lrwxrwxrwx 1 <user> <group> 12 <date> libcalc.so -> libcalc.so.1
lrwxrwxrwx 1 <user> <group> 16 <date> libcalc.so.1 -> libcalc.so.1.0.0
-rwxr-xr-x 1 <user> <group> ~16K <date> libcalc.so.1.0.0
```

Solo `libcalc.so.1.0.0` es un archivo real. Los otros dos son symlinks.

### Limpieza de la parte 1

No limpiar todavia -- los archivos se usan en la parte 2.

---

## Parte 2 -- Enlazar un programa y verificar dependencias

**Objetivo**: Compilar un programa contra `libcalc`, verificar que el ejecutable
registra la dependencia por soname (no por real name ni linker name), y ejecutar
con `LD_LIBRARY_PATH`.

### Paso 2.1 -- Examinar el programa cliente

```bash
cat program_v1.c
```

Observa que incluye `calc.h` y usa `calc_add` y `calc_mul`.

### Paso 2.2 -- Compilar el programa

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic program_v1.c -L. -lcalc -o program_v1
```

- `-L.`: buscar bibliotecas en el directorio actual
- `-lcalc`: buscar `libcalc.so` (el linker name)

El linker encuentra `libcalc.so` (symlink), sigue la cadena hasta
`libcalc.so.1.0.0`, y lee el SONAME embebido para registrar la dependencia.

### Paso 2.3 -- Predecir la dependencia

Antes de verificar, responde mentalmente:

- Que nombre registrara el ejecutable como dependencia NEEDED?
- Sera `libcalc.so`, `libcalc.so.1`, o `libcalc.so.1.0.0`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4 -- Verificar con readelf

```bash
readelf -d program_v1 | grep NEEDED
```

Salida esperada (las lineas de libc y ld-linux pueden variar):

```
 0x0000000000000001 (NEEDED)             Shared library: [libcalc.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
```

El ejecutable registra `libcalc.so.1` (el soname), no `libcalc.so` ni
`libcalc.so.1.0.0`. En runtime, el cargador dinamico buscara un archivo o
symlink llamado exactamente `libcalc.so.1`.

### Paso 2.5 -- Ejecutar el programa

```bash
LD_LIBRARY_PATH=. ./program_v1
```

Salida esperada:

```
calc_add(10, 3) = 13
calc_mul(10, 3) = 30
```

`LD_LIBRARY_PATH=.` indica al cargador dinamico que busque bibliotecas en el
directorio actual (sin necesidad de instalar en `/usr/lib` ni ejecutar
`ldconfig`).

### Paso 2.6 -- Verificar con ldd

```bash
LD_LIBRARY_PATH=. ldd program_v1
```

Salida esperada:

```
	linux-vdso.so.1 (0x<addr>)
	libcalc.so.1 => ./libcalc.so.1.0.0 (0x<addr>)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x<addr>)
	/lib64/ld-linux-x86-64.so.2 (0x<addr>)
```

Observa: `ldd` muestra que `libcalc.so.1` se resuelve a `./libcalc.so.1.0.0`.
El cargador busco el soname, encontro el symlink `libcalc.so.1`, y lo siguio
hasta el archivo real.

### Limpieza de la parte 2

No limpiar todavia -- los archivos se usan en la parte 3.

---

## Parte 3 -- Actualizacion compatible (minor bump)

**Objetivo**: Crear `libcalc.so.1.1.0` (minor bump, agrega `calc_sub`),
actualizar el symlink del soname, y demostrar que el programa existente sigue
funcionando sin recompilar.

### Paso 3.1 -- Examinar la nueva version

```bash
cat calc_v1_minor.c
```

Observa que mantiene `calc_add` y `calc_mul` con la misma firma, y agrega
`calc_sub`. Esto es un cambio compatible: extiende la ABI sin modificar lo
existente.

### Paso 3.2 -- Compilar v1.1.0

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c -fPIC calc_v1_minor.c -o calc_v1_minor.o
gcc -shared -Wl,-soname,libcalc.so.1 -o libcalc.so.1.1.0 calc_v1_minor.o
```

El soname sigue siendo `libcalc.so.1` (misma ABI major). Solo cambia el real
name: `libcalc.so.1.1.0`.

### Paso 3.3 -- Predecir el comportamiento

Antes de actualizar el symlink, responde mentalmente:

- Si actualizamos `libcalc.so.1` para que apunte a `libcalc.so.1.1.0`, el
  programa `program_v1` (compilado con v1.0.0) seguira funcionando?
- Necesitamos recompilar `program_v1`?
- Que busca `program_v1` en runtime: `libcalc.so.1` o `libcalc.so.1.0.0`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 -- Actualizar el symlink del soname

```bash
ln -sf libcalc.so.1.1.0 libcalc.so.1
```

Antes de la actualizacion:

```
libcalc.so.1 --> libcalc.so.1.0.0
```

Despues:

```
libcalc.so.1 --> libcalc.so.1.1.0
```

### Paso 3.5 -- Ejecutar sin recompilar

```bash
LD_LIBRARY_PATH=. ./program_v1
```

Salida esperada:

```
calc_add(10, 3) = 13
calc_mul(10, 3) = 30
```

El programa funciona identicamente. No fue recompilado, pero ahora usa
`libcalc.so.1.1.0` en lugar de `libcalc.so.1.0.0`. Esto es posible porque:

1. `program_v1` busca `libcalc.so.1` (el soname)
2. `libcalc.so.1` ahora apunta a `libcalc.so.1.1.0`
3. La nueva version mantiene las mismas funciones con las mismas firmas

### Paso 3.6 -- Confirmar con ldd

```bash
LD_LIBRARY_PATH=. ldd program_v1
```

Salida esperada:

```
	linux-vdso.so.1 (0x<addr>)
	libcalc.so.1 => ./libcalc.so.1.1.0 (0x<addr>)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x<addr>)
	/lib64/ld-linux-x86-64.so.2 (0x<addr>)
```

Ahora `libcalc.so.1` se resuelve a `./libcalc.so.1.1.0`. La actualizacion fue
transparente para el programa.

### Paso 3.7 -- Verificar que ambos .so tienen el mismo soname

```bash
readelf -d libcalc.so.1.0.0 | grep SONAME
readelf -d libcalc.so.1.1.0 | grep SONAME
```

Salida esperada:

```
 0x000000000000000e (SONAME)             Library soname: [libcalc.so.1]
 0x000000000000000e (SONAME)             Library soname: [libcalc.so.1]
```

Ambas versiones tienen el mismo soname porque comparten la misma ABI major.
El soname es la promesa de compatibilidad: "si buscas `libcalc.so.1`, cualquier
version 1.x.y servira".

### Paso 3.8 -- Verificar los archivos

```bash
ls -la libcalc*
```

Salida esperada:

```
lrwxrwxrwx 1 <user> <group> 12 <date> libcalc.so -> libcalc.so.1
lrwxrwxrwx 1 <user> <group> 16 <date> libcalc.so.1 -> libcalc.so.1.1.0
-rwxr-xr-x 1 <user> <group> ~16K <date> libcalc.so.1.0.0
-rwxr-xr-x 1 <user> <group> ~16K <date> libcalc.so.1.1.0
```

La version anterior (`1.0.0`) sigue en disco pero ya no se usa. En un sistema
real, el paquete de actualizacion puede eliminarla o mantenerla como respaldo.

### Limpieza de la parte 3

No limpiar todavia -- los archivos se usan en la parte 4.

---

## Parte 4 -- Actualizacion incompatible (major bump) y coexistencia

**Objetivo**: Crear `libcalc.so.2.0.0` con un cambio de API incompatible
(soname `libcalc.so.2`), demostrar que ambas versiones coexisten, y que cada
programa usa la version correcta.

### Paso 4.1 -- Examinar la API v2

```bash
cat calc_v2.h
```

Observa que `calc_add` ahora recibe 3 argumentos en lugar de 2. Este cambio
de firma rompe la ABI: un programa compilado con v1 pasaria solo 2 argumentos,
pero v2 espera 3. Esto exige un nuevo major.

```bash
cat calc_v2.c
```

### Paso 4.2 -- Compilar v2.0.0 con soname libcalc.so.2

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c -fPIC calc_v2.c -o calc_v2.o
gcc -shared -Wl,-soname,libcalc.so.2 -o libcalc.so.2.0.0 calc_v2.o
```

El soname ahora es `libcalc.so.2` (nuevo major). Es un nombre diferente a
`libcalc.so.1`, por lo que no interfiere con las versiones 1.x.

### Paso 4.3 -- Crear symlinks para v2

```bash
ln -sf libcalc.so.2.0.0 libcalc.so.2
```

Para compilar un programa con v2, el linker name (`libcalc.so`) debe apuntar
a la nueva version:

```bash
ln -sf libcalc.so.2 libcalc.so
```

### Paso 4.4 -- Verificar soname de v2

```bash
readelf -d libcalc.so.2.0.0 | grep SONAME
```

Salida esperada:

```
 0x000000000000000e (SONAME)             Library soname: [libcalc.so.2]
```

### Paso 4.5 -- Compilar un programa contra v2

```bash
cat program_v2.c
```

Observa que usa `calc_add(a, b, c)` con 3 argumentos e incluye `calc_v2.h`.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic program_v2.c -L. -lcalc -o program_v2
```

### Paso 4.6 -- Predecir las dependencias

Antes de verificar, responde mentalmente:

- `program_v1` fue compilado con v1 -- que soname registra como NEEDED?
- `program_v2` fue compilado con v2 -- que soname registra como NEEDED?
- Pueden ambos programas ejecutarse simultaneamente si ambas bibliotecas
  estan presentes?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.7 -- Verificar las dependencias de ambos programas

```bash
readelf -d program_v1 | grep NEEDED
```

Salida esperada:

```
 0x0000000000000001 (NEEDED)             Shared library: [libcalc.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
```

```bash
readelf -d program_v2 | grep NEEDED
```

Salida esperada:

```
 0x0000000000000001 (NEEDED)             Shared library: [libcalc.so.2]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
```

Cada programa registra un soname diferente. No hay conflicto.

### Paso 4.8 -- Ejecutar ambos programas

```bash
LD_LIBRARY_PATH=. ./program_v1
```

Salida esperada:

```
calc_add(10, 3) = 13
calc_mul(10, 3) = 30
```

```bash
LD_LIBRARY_PATH=. ./program_v2
```

Salida esperada:

```
calc_add(10, 3, 7) = 20
calc_mul(10, 3) = 30
```

Ambos programas funcionan simultaneamente. Cada uno carga la version que
necesita:

- `program_v1` carga `libcalc.so.1` (resuelve a `libcalc.so.1.1.0`)
- `program_v2` carga `libcalc.so.2` (resuelve a `libcalc.so.2.0.0`)

### Paso 4.9 -- Confirmar coexistencia con ldd

```bash
LD_LIBRARY_PATH=. ldd program_v1
```

Salida esperada:

```
	linux-vdso.so.1 (0x<addr>)
	libcalc.so.1 => ./libcalc.so.1.1.0 (0x<addr>)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x<addr>)
	/lib64/ld-linux-x86-64.so.2 (0x<addr>)
```

```bash
LD_LIBRARY_PATH=. ldd program_v2
```

Salida esperada:

```
	linux-vdso.so.1 (0x<addr>)
	libcalc.so.2 => ./libcalc.so.2.0.0 (0x<addr>)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x<addr>)
	/lib64/ld-linux-x86-64.so.2 (0x<addr>)
```

### Paso 4.10 -- Ver todos los archivos de la biblioteca

```bash
ls -la libcalc*
```

Salida esperada:

```
lrwxrwxrwx 1 <user> <group> 12 <date> libcalc.so -> libcalc.so.2
lrwxrwxrwx 1 <user> <group> 16 <date> libcalc.so.1 -> libcalc.so.1.1.0
-rwxr-xr-x 1 <user> <group> ~16K <date> libcalc.so.1.0.0
-rwxr-xr-x 1 <user> <group> ~16K <date> libcalc.so.1.1.0
lrwxrwxrwx 1 <user> <group> 16 <date> libcalc.so.2 -> libcalc.so.2.0.0
-rwxr-xr-x 1 <user> <group> ~16K <date> libcalc.so.2.0.0
```

Esta es la estructura tipica de coexistencia de versiones:

- `libcalc.so` (linker name): apunta a v2, para nuevas compilaciones
- `libcalc.so.1` (soname v1): apunta a la ultima 1.x (1.1.0)
- `libcalc.so.2` (soname v2): apunta a la ultima 2.x (2.0.0)
- Los real names contienen la version completa

### Paso 4.11 -- Que pasa si eliminamos libcalc.so.1?

Antes de probar, responde mentalmente:

- Si eliminamos el symlink `libcalc.so.1`, que le pasara a `program_v1`?
- Y a `program_v2`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.12 -- Probar la rotura

```bash
rm libcalc.so.1
LD_LIBRARY_PATH=. ./program_v1
```

Salida esperada:

```
./program_v1: error while loading shared libraries: libcalc.so.1: cannot open shared object file: No such file or directory
```

`program_v1` falla porque busca `libcalc.so.1` y ese symlink ya no existe.
`program_v2` no se ve afectado porque busca `libcalc.so.2`:

```bash
LD_LIBRARY_PATH=. ./program_v2
```

Salida esperada:

```
calc_add(10, 3, 7) = 20
calc_mul(10, 3) = 30
```

### Paso 4.13 -- Restaurar el symlink

```bash
ln -sf libcalc.so.1.1.0 libcalc.so.1
LD_LIBRARY_PATH=. ./program_v1
```

Salida esperada:

```
calc_add(10, 3) = 13
calc_mul(10, 3) = 30
```

Restaurado. El symlink del soname es el punto critico: si desaparece, los
programas que dependen de ese major fallan. En un sistema real, `ldconfig` se
encarga de crear y mantener estos symlinks automaticamente.

---

## Limpieza final

```bash
rm -f calc_v1.o calc_v1_minor.o calc_v2.o
rm -f libcalc.so libcalc.so.1 libcalc.so.2
rm -f libcalc.so.1.0.0 libcalc.so.1.1.0 libcalc.so.2.0.0
rm -f program_v1 program_v2
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  calc.h  calc_v1.c  calc_v1_minor.c  calc_v2.c  calc_v2.h  program_v1.c  program_v2.c
```

---

## Conceptos reforzados

1. El esquema de versionado `MAJOR.MINOR.PATCH` tiene un significado preciso
   en bibliotecas compartidas: MAJOR indica compatibilidad de ABI, MINOR indica
   nueva funcionalidad compatible, PATCH indica correcciones sin cambio de
   interfaz.

2. El soname (`lib<nombre>.so.<MAJOR>`) se graba dentro del archivo `.so` con
   `-Wl,-soname,<soname>` al compilar. Es la identidad de compatibilidad de la
   biblioteca.

3. El ejecutable registra la dependencia por soname (campo NEEDED), no por real
   name ni linker name. En runtime, el cargador dinamico busca un archivo o
   symlink que coincida con el soname.

4. La cadena de tres nombres (`libcalc.so` -> `libcalc.so.1` -> `libcalc.so.1.0.0`)
   separa tres responsabilidades: compilacion (linker name), compatibilidad
   runtime (soname), y archivo fisico versionado (real name).

5. Una actualizacion compatible (minor/patch bump) solo requiere copiar el nuevo
   `.so` y actualizar el symlink del soname. Los programas existentes siguen
   funcionando sin recompilar.

6. Una actualizacion incompatible (major bump) crea un soname diferente. Las
   versiones coexisten: programas viejos usan `libfoo.so.1`, programas nuevos
   usan `libfoo.so.2`, ambos funcionan simultaneamente.

7. `readelf -d <archivo> | grep SONAME` y `objdump -p <archivo> | grep SONAME`
   muestran el soname embebido en una biblioteca. `readelf -d <ejecutable> |
   grep NEEDED` muestra las dependencias registradas en un programa.

8. Si se elimina el symlink del soname, todos los programas que dependen de ese
   major fallan con "cannot open shared object file". En sistemas de produccion,
   `ldconfig` crea y mantiene estos symlinks automaticamente.
