# Lab -- Creacion de bibliotecas dinamicas (.so)

## Objetivo

Compilar una biblioteca dinamica desde cero: generar codigo PIC con `-fPIC`,
crear el archivo `.so` con `gcc -shared`, inspeccionar el resultado con `file`,
`nm -D`, `readelf -d` y `objdump -d`, y comparar con una biblioteca estatica
(`.a`). Al finalizar, sabras exactamente que produce cada flag y que ocurre si
omites `-fPIC`.

## Prerequisitos

- GCC instalado
- Herramientas `file`, `nm`, `readelf`, `objdump` disponibles
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `vec3.h` | Header con la API publica de vectores 3D |
| `vec3.c` | Implementacion de la biblioteca (se compila con -fPIC) |
| `vec3_nopic.c` | Misma implementacion, para compilar SIN -fPIC y comparar |
| `main_vec3.c` | Programa cliente que usa la biblioteca |

---

## Parte 1 -- Compilar objetos con -fPIC

**Objetivo**: Generar archivos objeto con codigo independiente de posicion
(Position-Independent Code) y verificar que son aptos para una biblioteca
dinamica.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat vec3.h
```

Observa la interfaz: cinco funciones que operan sobre `Vec3` (un struct con
tres `double`). Este header es lo que usara el programa cliente.

```bash
cat vec3.c
```

Observa la implementacion: funciones simples que incluyen llamadas a `printf`
y `sqrt` (funciones externas). Estas llamadas externas son las que PIC maneja
de forma diferente.

### Paso 1.2 -- Compilar con -fPIC

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c -fPIC vec3.c -o vec3_pic.o
```

El flag `-fPIC` le dice al compilador que genere codigo donde todas las
referencias a datos y funciones usen direccionamiento relativo al PC (Program
Counter) o pasen por la GOT (Global Offset Table). Esto permite que el codigo
se cargue en cualquier direccion de memoria.

### Paso 1.3 -- Compilar SIN -fPIC para comparar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c vec3_nopic.c -o vec3_nopic.o
```

### Paso 1.4 -- Predecir las diferencias

Antes de inspeccionar, responde mentalmente:

- Ambos `.o` tienen el mismo codigo fuente. El unico cambio es `-fPIC`.
  Seran identicos los archivos objeto?
- En codigo PIC, como accede una funcion a una variable global o a una
  funcion externa como `printf`? (Pista: GOT)
- Que flag en la seccion dinamica indica que un objeto NO es PIC?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.5 -- Comparar el codigo generado con objdump

```bash
objdump -d vec3_pic.o | head -60
```

Busca instrucciones como `lea` con `rip` (relativo al instruction pointer).
Esto es tipico del codigo PIC en x86-64.

```bash
objdump -d vec3_nopic.o | head -60
```

En x86-64, gcc genera codigo PIC-compatible por defecto en la mayoria de las
distribuciones modernas. Para ver la diferencia real, compara las secciones de
relocacion:

```bash
readelf -r vec3_pic.o | head -20
```

```bash
readelf -r vec3_nopic.o | head -20
```

Observa los tipos de relocaciones. En el objeto PIC veras relocaciones de
tipo `R_X86_64_PLT32` y `R_X86_64_REX_GOTPCRELX` (accesos via GOT/PLT). En
el objeto no-PIC pueden aparecer relocaciones absolutas.

### Paso 1.6 -- Verificar ausencia de TEXTREL

```bash
readelf -d vec3_pic.o 2>/dev/null | grep TEXTREL
```

Salida esperada: (nada -- sin salida).

Si no aparece `TEXTREL`, el objeto es PIC correcto. `TEXTREL` indicaria que el
codigo tiene relocaciones en la seccion de texto (instrucciones), lo que
impediria compartir la pagina de codigo entre procesos.

### Limpieza de la parte 1

No eliminar aun -- los `.o` se usan en las partes siguientes.

---

## Parte 2 -- Crear la biblioteca compartida (.so)

**Objetivo**: Enlazar los objetos PIC en un archivo `.so` usando `gcc -shared`
e inspeccionar el resultado.

### Paso 2.1 -- Crear el .so

```bash
gcc -shared -o libvec3.so vec3_pic.o
```

El flag `-shared` le dice a gcc que produzca un shared object en lugar de un
ejecutable. El resultado es un archivo ELF de tipo `ET_DYN`.

### Paso 2.2 -- Verificar el tipo de archivo con file

```bash
file libvec3.so
```

Salida esperada:

```
libvec3.so: ELF 64-bit LSB shared object, x86-64, version 1 (SYSV), dynamically linked, BuildID[sha1]=<hash>, not stripped
```

Observa: dice "shared object" (no "executable" ni "relocatable"). Este es el
formato que el linker dinamico (`ld-linux.so`) sabe cargar en runtime.

### Paso 2.3 -- Comparar con un .a y un .o

Para entender la diferencia de formato, compara los tres tipos de archivo:

```bash
file vec3_pic.o
```

Salida esperada:

```
vec3_pic.o: ELF 64-bit LSB relocatable, x86-64, ...
```

Crea una biblioteca estatica para comparar:

```bash
ar rcs libvec3.a vec3_pic.o
file libvec3.a
```

Salida esperada:

```
libvec3.a: current ar archive
```

Resumen de los tres formatos:

| Archivo | Tipo ELF | Descripcion |
|---------|----------|-------------|
| `vec3_pic.o` | relocatable | Objeto individual, necesita enlace |
| `libvec3.a` | ar archive | Coleccion de .o (se copia al ejecutable) |
| `libvec3.so` | shared object | Codigo cargable en runtime (referencia) |

### Paso 2.4 -- Comparar tamanos

```bash
ls -lh vec3_pic.o libvec3.a libvec3.so
```

Salida esperada (tamanos aproximados):

```
-rw-r--r-- 1 <user> <group> ~3.5K vec3_pic.o
-rw-r--r-- 1 <user> <group> ~3.7K libvec3.a
-rwxr-xr-x 1 <user> <group>  ~16K libvec3.so
```

El `.so` es significativamente mas grande que el `.o` o el `.a`. Esto se debe
a que contiene las secciones adicionales para enlace dinamico: `.dynsym`,
`.dynstr`, `.plt`, `.got`, `.dynamic`, y la hash table de simbolos.

### Limpieza de la parte 2

No eliminar aun -- se usan en la parte siguiente.

---

## Parte 3 -- Inspeccionar simbolos y secciones del .so

**Objetivo**: Usar `nm -D` y `readelf -d` para examinar que simbolos exporta
la biblioteca y que secciones dinamicas contiene.

### Paso 3.1 -- Ver simbolos dinamicos con nm -D

```bash
nm -D libvec3.so
```

Salida esperada:

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

Observa:

- Las funciones con `T` son simbolos exportados (Text section, visibles)
- Las funciones con `U` son simbolos indefinidos (se resolveran en runtime)
- Las funciones con `w` son weak symbols (del runtime de C)
- `printf` y `sqrt` aparecen como `U` porque `libvec3.so` las necesita pero
  no las contiene -- se resolveran cuando el programa se cargue

### Paso 3.2 -- Comparar nm -D con nm (sin -D)

```bash
nm libvec3.so | wc -l
nm -D libvec3.so | wc -l
```

`nm` sin `-D` muestra TODOS los simbolos (internos + externos). `nm -D` solo
muestra los simbolos de la tabla dinamica (`.dynsym`), que son los visibles
para otros programas. La diferencia son los simbolos internos del runtime.

### Paso 3.3 -- Predecir las secciones dinamicas

Antes de inspeccionar con readelf, responde mentalmente:

- El `.so` necesita declarar que bibliotecas necesita (como `libm` para
  `sqrt`). En que seccion estara eso?
- El `.so` necesita una tabla de simbolos para que el linker dinamico los
  encuentre. Como se llama esa seccion?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 -- Inspeccionar la seccion .dynamic con readelf

```bash
readelf -d libvec3.so
```

Salida esperada (extracto):

```
Dynamic section at offset <offset> contains <n> entries:
  Tag        Type                         Name/Value
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x000000000000000c (INIT)               <addr>
 0x000000000000000d (FINI)               <addr>
 ...
 0x0000000000000005 (STRTAB)             <addr>
 0x0000000000000006 (SYMTAB)             <addr>
 ...
```

Observa:

- `NEEDED` lista las bibliotecas de las que depende este `.so`
- Aparece `libc.so.6` pero NO `libm.so.6` -- porque no enlazamos con `-lm`
  al crear el `.so` (esto causaria un error en runtime si se llama a
  `vec3_length` que usa `sqrt`)
- `SYMTAB` y `STRTAB` son la tabla de simbolos y la tabla de strings

### Paso 3.5 -- Corregir: crear el .so con -lm

```bash
gcc -shared -o libvec3.so vec3_pic.o -lm
readelf -d libvec3.so | grep NEEDED
```

Salida esperada:

```
 0x0000000000000001 (NEEDED)             Shared library: [libm.so.6]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
```

Ahora `libm.so.6` aparece como dependencia. Cuando el linker dinamico cargue
`libvec3.so`, sabra que tambien debe cargar `libm`.

### Paso 3.6 -- Ver las secciones del .so vs un .o

```bash
readelf -S libvec3.so | grep -c '\['
readelf -S vec3_pic.o | grep -c '\['
```

El `.so` tiene muchas mas secciones que el `.o`. Las secciones adicionales
(`.dynsym`, `.dynstr`, `.plt`, `.got`, `.dynamic`, `.gnu.hash`) son la
infraestructura que permite el enlace dinamico en runtime.

### Limpieza de la parte 3

No eliminar aun -- se usan en la parte siguiente.

---

## Parte 4 -- Que pasa si olvidas -fPIC

**Objetivo**: Intentar crear un `.so` desde un objeto compilado sin `-fPIC` y
observar el error o la advertencia.

### Paso 4.1 -- Intentar crear .so sin PIC

```bash
gcc -shared -o libvec3_nopic.so vec3_nopic.o -lm 2>&1
```

Dependiendo de la distribucion y version de gcc, puedes obtener:

**Caso A** -- Error (distribuciones modernas con `--no-undefined` por defecto
o hardened flags):

```
/usr/bin/ld: vec3_nopic.o: relocation R_X86_64_PC32 against symbol `printf@@GLIBC_2.2.5' can not be used when making a shared object; recompile with -fPIC
```

**Caso B** -- Exito con TEXTREL (distribuciones mas permisivas):

El `.so` se crea, pero contiene text relocations.

### Paso 4.2 -- Verificar TEXTREL si se creo el .so

Si el comando anterior tuvo exito:

```bash
readelf -d libvec3_nopic.so 2>/dev/null | grep TEXTREL
```

Salida esperada (si existe el archivo):

```
 0x0000000000000016 (TEXTREL)            0x0
 0x000000000000001e (FLAGS)              TEXTREL
```

`TEXTREL` significa que el codigo tiene relocaciones en la seccion de texto.
Consecuencias:

- El kernel no puede compartir las paginas de codigo entre procesos
  (cada proceso necesita su propia copia con las direcciones parcheadas)
- Algunos sistemas (Android, Fedora con SELinux) rechazan cargar `.so` con
  TEXTREL por razones de seguridad
- ASLR pierde efectividad en esas paginas

### Paso 4.3 -- Comparar con el .so PIC correcto

```bash
readelf -d libvec3.so | grep TEXTREL
```

Salida esperada: (nada -- sin salida).

La biblioteca compilada con `-fPIC` no tiene TEXTREL. Sus paginas de codigo
son compartibles entre procesos y compatibles con ASLR completo.

### Limpieza de la parte 4

No eliminar aun -- se usan en la parte siguiente.

---

## Parte 5 -- Comparar .so vs .a en el ejecutable final

**Objetivo**: Compilar el mismo programa enlazando con la biblioteca dinamica
y con la biblioteca estatica, y comparar el resultado.

### Paso 5.1 -- Examinar el programa cliente

```bash
cat main_vec3.c
```

Observa que incluye `vec3.h` y llama a las funciones de la biblioteca. El
programa no sabe si se enlazara con `.so` o con `.a` -- el codigo es el mismo.

### Paso 5.2 -- Compilar con enlace estatico

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main_vec3.c -L. -lvec3 -lm -o main_static -static 2>/dev/null || \
gcc -std=c11 -Wall -Wextra -Wpedantic main_vec3.c libvec3.a -lm -o main_static
```

El primer comando intenta enlace completamente estatico. Si falla (porque no
esta instalada la version estatica de glibc), el segundo enlaza solo `libvec3`
estaticamente pasando el `.a` directamente.

### Paso 5.3 -- Compilar con enlace dinamico

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main_vec3.c -L. -lvec3 -lm -o main_dynamic
```

### Paso 5.4 -- Predecir las diferencias

Antes de comparar, responde mentalmente:

- Cual ejecutable sera mas grande: el enlazado con `.a` o con `.so`?
- Cual ejecutable tendra dependencia de `libvec3.so` en `ldd`?
- Si borras `libvec3.so`, cual ejecutable seguira funcionando?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.5 -- Comparar tamanos

```bash
ls -lh main_static main_dynamic
```

Salida esperada (tamanos aproximados):

```
-rwxr-xr-x 1 <user> <group>  ~20K main_static
-rwxr-xr-x 1 <user> <group>  ~16K main_dynamic
```

El ejecutable con enlace estatico es mas grande porque incluye una copia del
codigo de `libvec3` dentro del binario. El dinamico solo contiene una
referencia.

### Paso 5.6 -- Comparar dependencias con ldd

```bash
ldd main_dynamic
```

Salida esperada:

```
	linux-vdso.so.1 (0x...)
	libvec3.so => not found
	libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x...)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x...)
	/lib64/ld-linux-x86-64.so.2 (0x...)
```

```bash
ldd main_static
```

Salida esperada (si enlace fue completamente estatico):

```
	not a dynamic executable
```

O si se enlazo solo `libvec3` estaticamente, mostrara dependencias de `libc`
y `libm` pero NO de `libvec3.so`.

Observa que `libvec3.so => not found` en `main_dynamic`. El ejecutable sabe
que necesita `libvec3.so` pero no sabe donde esta. Eso se resuelve con
`LD_LIBRARY_PATH` o `rpath`, que se cubren en T02.

### Paso 5.7 -- Comparar simbolos

```bash
nm main_dynamic | grep vec3
```

Salida esperada:

```
                 U vec3_add
                 U vec3_create
                 U vec3_dot
                 U vec3_length
                 U vec3_print
```

Las funciones aparecen como `U` (undefined) -- se resolveran en runtime.

```bash
nm main_static | grep vec3
```

Salida esperada:

```
<addr> T vec3_add
<addr> T vec3_create
<addr> T vec3_dot
<addr> T vec3_length
<addr> T vec3_print
```

Las funciones aparecen como `T` (text) con direcciones asignadas -- el codigo
esta dentro del ejecutable.

---

## Limpieza final

```bash
rm -f vec3_pic.o vec3_nopic.o
rm -f libvec3.so libvec3.a libvec3_nopic.so
rm -f main_static main_dynamic
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  main_vec3.c  vec3.c  vec3.h  vec3_nopic.c
```

---

## Conceptos reforzados

1. El flag `-fPIC` genera codigo que usa direccionamiento relativo al PC y
   la GOT (Global Offset Table) para acceder a funciones y datos externos.
   Esto permite que el `.so` se cargue en cualquier direccion de memoria.

2. `gcc -shared` produce un archivo ELF de tipo "shared object" (`ET_DYN`),
   que contiene secciones adicionales (`.dynsym`, `.dynstr`, `.plt`, `.got`,
   `.dynamic`) para el enlace en runtime.

3. `nm -D` muestra solo los simbolos de la tabla dinamica -- los que son
   visibles para otros programas. Las funciones exportadas aparecen con `T`
   y las dependencias externas con `U`.

4. `readelf -d` muestra la seccion `.dynamic`, que incluye las bibliotecas
   `NEEDED` (dependencias) y metadata del enlace dinamico. Olvidar `-lm` al
   crear el `.so` causa que `libm` no aparezca en NEEDED.

5. Compilar sin `-fPIC` produce objetos con relocaciones absolutas. Al
   intentar crear un `.so` con ellos, se obtiene un error o un `.so` con
   `TEXTREL`, que no puede compartir paginas de codigo entre procesos y
   puede ser rechazado por politicas de seguridad.

6. Un `.a` (ar archive) es una coleccion de `.o` que se copia dentro del
   ejecutable en tiempo de enlace. Un `.so` (shared object) se carga en
   runtime y el ejecutable solo guarda una referencia. El ejecutable
   enlazado con `.a` es mas grande pero no tiene dependencias externas de
   la biblioteca.

7. En el ejecutable enlazado dinamicamente, los simbolos de la biblioteca
   aparecen como `U` (undefined) en `nm` -- se resolveran cuando el linker
   dinamico (`ld-linux.so`) cargue el `.so` en runtime. En el enlazado
   estaticamente, aparecen como `T` con direcciones asignadas.
