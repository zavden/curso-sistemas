# Lab -- Creacion de bibliotecas estaticas

## Objetivo

Crear una biblioteca estatica desde cero: compilar codigo fuente a archivos
objeto, empaquetarlos con `ar`, inspeccionar el contenido con `ar t`, `nm` y
`file`, enlazar la biblioteca en un programa, y verificar que el linker solo
incluye los archivos objeto necesarios del `.a`.

## Prerequisitos

- GCC instalado
- `ar`, `nm`, `file` disponibles
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `vec3.h` | Header publico: struct Vec3 y prototipos de todas las funciones |
| `vec3.c` | Operaciones matematicas: create, add, scale, dot, length |
| `vec3_print.c` | Funciones de impresion: print, report |
| `main.c` | Programa cliente que usa todas las funciones de la biblioteca |
| `main_minimal.c` | Programa cliente que usa solo vec3_create y vec3_add |

---

## Parte 1 -- Codigo fuente y archivos objeto

**Objetivo**: Compilar los archivos fuente a archivos objeto (`.o`) e
inspeccionar su contenido con `file` y `nm`.

### Paso 1.1 -- Examinar el header

```bash
cat vec3.h
```

Observa:

- La guarda de inclusion (`#ifndef VEC3_H`) evita inclusion multiple
- La struct `Vec3` contiene tres `double`
- Los prototipos estan separados por archivo de implementacion: las funciones
  matematicas vienen de `vec3.c` y las de impresion de `vec3_print.c`

### Paso 1.2 -- Examinar las implementaciones

```bash
cat vec3.c
```

Observa que `vec3.c` incluye `<math.h>` porque `vec3_length` usa `sqrt()`.
Esto significa que al enlazar necesitaremos `-lm`.

```bash
cat vec3_print.c
```

Este archivo solo usa `<stdio.h>` para `printf()`. No depende de `<math.h>`.

### Paso 1.3 -- Compilar a archivos objeto

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c vec3.c -o vec3.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c vec3_print.c -o vec3_print.o
```

No debe producir warnings ni errores. El flag `-c` le dice a GCC que compile
sin enlazar: produce un `.o` en lugar de un ejecutable.

### Paso 1.4 -- Inspeccionar con file

```bash
file vec3.o
```

Salida esperada:

```
vec3.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
```

La palabra clave es **relocatable**: el archivo objeto no tiene direcciones
de memoria finales. Estas se asignan durante el enlace.

```bash
file vec3_print.o
```

Salida esperada:

```
vec3_print.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
```

### Paso 1.5 -- Predecir los simbolos

Antes de ejecutar `nm`, responde mentalmente:

- En `vec3.o`, cuantos simbolos con `T` (funciones definidas) esperas ver?
- Que simbolo aparecera con `U` (undefined, necesita otra biblioteca)?
- En `vec3_print.o`, que simbolo externo necesita?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.6 -- Inspeccionar simbolos con nm

```bash
nm vec3.o
```

Salida esperada:

```
                 U sqrt
0000000000000046 T vec3_add
0000000000000000 T vec3_create
00000000000000ef T vec3_dot
0000000000000127 T vec3_length
000000000000009c T vec3_scale
```

- `T` = text section (funcion definida en este archivo)
- `U` = undefined (simbolo externo, se resolvera al enlazar con `-lm`)

```bash
nm vec3_print.o
```

Salida esperada:

```
                 U printf
0000000000000000 T vec3_print
0000000000000031 T vec3_report
```

`printf` es un simbolo undefined que se resolvera al enlazar con `libc`
(enlazada automaticamente por GCC).

---

## Parte 2 -- Creacion del .a

**Objetivo**: Empaquetar los archivos objeto en una biblioteca estatica con
`ar rcs` y verificar su contenido.

### Paso 2.1 -- Crear la biblioteca

```bash
ar rcs libvec3.a vec3.o vec3_print.o
```

Los flags significan:

- `r` -- insertar/reemplazar archivos en el archivo
- `c` -- crear el archivo si no existe (suprime un warning)
- `s` -- crear indice de simbolos (equivale a ejecutar `ranlib`)

### Paso 2.2 -- Verificar con file

```bash
file libvec3.a
```

Salida esperada:

```
libvec3.a: current ar archive
```

El `.a` es un archivo `ar` (archiver), no un ELF. Es un contenedor que
agrupa los `.o` dentro.

### Paso 2.3 -- Listar miembros con ar t

```bash
ar t libvec3.a
```

Salida esperada:

```
vec3.o
vec3_print.o
```

`ar t` lista los archivos objeto empaquetados dentro de la biblioteca.

### Paso 2.4 -- Listar todos los simbolos de la biblioteca

```bash
nm libvec3.a
```

Salida esperada:

```
vec3.o:
                 U sqrt
0000000000000046 T vec3_add
0000000000000000 T vec3_create
00000000000000ef T vec3_dot
0000000000000127 T vec3_length
000000000000009c T vec3_scale

vec3_print.o:
                 U printf
0000000000000000 T vec3_print
0000000000000031 T vec3_report
```

`nm` aplicado a un `.a` muestra los simbolos de cada `.o` agrupados por
archivo. El linker usa este indice para saber que `.o` extraer cuando un
programa necesita un simbolo.

---

## Parte 3 -- Enlazar y ejecutar

**Objetivo**: Compilar un programa que usa la biblioteca, enlazarlo con
`-L. -lvec3`, y ejecutarlo.

### Paso 3.1 -- Examinar el programa cliente

```bash
cat main.c
```

Observa que `main.c` incluye `"vec3.h"` y usa funciones de ambos archivos
objeto: `vec3_create`, `vec3_add`, `vec3_scale`, `vec3_dot`, `vec3_length`
(de `vec3.o`) y `vec3_report`, `vec3_print` (de `vec3_print.o`).

### Paso 3.2 -- Compilar y enlazar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main.c -L. -lvec3 -lm -o demo_vec3
```

- `-L.` -- buscar bibliotecas en el directorio actual
- `-lvec3` -- enlazar con `libvec3.a` (GCC agrega el prefijo `lib` y la
  extension `.a` automaticamente)
- `-lm` -- enlazar con `libm` porque `vec3_length` usa `sqrt()`

### Paso 3.3 -- Predecir la salida

Antes de ejecutar, responde mentalmente:

- Que resultado dara `vec3_add({1,2,3}, {4,5,6})`?
- Cual es el producto punto de (1,2,3) y (4,5,6)?
- Cual es la longitud de (1,2,3)?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 -- Ejecutar

```bash
./demo_vec3
```

Salida esperada:

```
a + b: (5.00, 7.00, 9.00)
a * 2.5: (2.50, 5.00, 7.50)
dot(a, b): 32.00
length(a): 3.74
(1.00, 2.00, 3.00)
(4.00, 5.00, 6.00)
```

### Paso 3.5 -- Verificar que el ejecutable es independiente

```bash
file demo_vec3
```

Salida esperada:

```
demo_vec3: ELF 64-bit LSB executable, x86-64, ...
```

El ejecutable contiene el codigo de la biblioteca copiado dentro. No
necesita `libvec3.a` en tiempo de ejecucion:

```bash
rm libvec3.a
./demo_vec3
```

El programa sigue funcionando. La biblioteca estatica solo se necesita en
tiempo de compilacion. Recrea la biblioteca para los pasos siguientes:

```bash
ar rcs libvec3.a vec3.o vec3_print.o
```

---

## Parte 4 -- Enlace selectivo

**Objetivo**: Verificar que el linker solo extrae del `.a` los archivos
objeto que contienen simbolos necesarios.

### Paso 4.1 -- Examinar el programa minimal

```bash
cat main_minimal.c
```

Este programa solo usa `vec3_create` y `vec3_add`. No llama a ninguna
funcion de `vec3_print.o` (`vec3_print`, `vec3_report`).

### Paso 4.2 -- Compilar y enlazar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main_minimal.c -L. -lvec3 -lm -o demo_minimal
```

### Paso 4.3 -- Predecir que simbolos contendra el ejecutable

Antes de verificar, responde mentalmente:

- `main_minimal.c` no usa `vec3_print` ni `vec3_report`. Estaran en el
  ejecutable?
- `main_minimal.c` no llama a `vec3_dot` ni `vec3_length` directamente,
  pero estan en el mismo `.o` que `vec3_create` y `vec3_add`. Estaran en
  el ejecutable?

Recuerda: el linker extrae **archivos objeto completos** del `.a`, no
funciones individuales.

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.4 -- Verificar simbolos

```bash
nm demo_minimal | grep vec3
```

Salida esperada:

```
<addr> T vec3_add
<addr> T vec3_create
<addr> T vec3_dot
<addr> T vec3_length
<addr> T vec3_scale
```

Observa:

- `vec3_print` y `vec3_report` **no aparecen**: el linker no extrajo
  `vec3_print.o` del `.a` porque ningun simbolo de ese archivo fue
  solicitado
- `vec3_dot`, `vec3_length` y `vec3_scale` **si aparecen** aunque
  `main_minimal.c` no los usa: estan en `vec3.o`, el mismo archivo objeto
  que contiene `vec3_create` y `vec3_add`. El linker extrae el `.o`
  completo, no funciones sueltas

Este es el comportamiento clave del enlace con bibliotecas estaticas: la
granularidad de seleccion es el **archivo objeto**, no la funcion individual.
Por eso es buena practica separar funciones independientes en archivos `.c`
distintos.

---

## Parte 5 -- Comparacion de tamanios

**Objetivo**: Comparar el tamanio del ejecutable cuando se enlaza con `.a`
(enlace selectivo) versus cuando se enlazan los `.o` directamente (sin
seleccion).

### Paso 5.1 -- Compilar con .o directos

Enlaza `main_minimal.c` directamente con ambos `.o`, sin pasar por la
biblioteca:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic main_minimal.c vec3.o vec3_print.o -lm -o demo_direct
```

### Paso 5.2 -- Predecir la diferencia

Antes de comparar, responde mentalmente:

- Al enlazar con `.o` directamente, el linker incluye **todo** el contenido
  de cada `.o` listado en la linea de comandos, sin seleccion.
- Al enlazar con `.a`, el linker solo extrae los `.o` cuyos simbolos se
  necesitan.
- `demo_direct` incluira `vec3_print` y `vec3_report`?
- `demo_minimal` los incluira?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.3 -- Comparar tamanios

```bash
ls -l demo_minimal demo_direct
```

Salida esperada (los tamanios exactos pueden variar):

```
-rwxr-xr-x 1 <user> <group> ~12900 <date> demo_direct
-rwxr-xr-x 1 <user> <group> ~12800 <date> demo_minimal
```

`demo_direct` es ligeramente mas grande que `demo_minimal`.

### Paso 5.4 -- Verificar que simbolos tiene demo_direct

```bash
nm demo_direct | grep vec3
```

Salida esperada:

```
<addr> T vec3_add
<addr> T vec3_create
<addr> T vec3_dot
<addr> T vec3_length
<addr> T vec3_print
<addr> T vec3_report
<addr> T vec3_scale
```

`demo_direct` contiene **todos** los simbolos de ambos `.o`, incluyendo
`vec3_print` y `vec3_report` que el programa nunca llama. Al pasar `.o`
en la linea de comandos, el linker los incluye incondicionalmente.

En contraste, `demo_minimal` (enlazado con `-lvec3`) no contiene los
simbolos de `vec3_print.o` porque el linker determino que ese archivo
objeto no era necesario.

En este ejemplo la diferencia es pequenia (~100 bytes) porque la biblioteca
es trivial. En proyectos reales con decenas de modulos, el enlace selectivo
del `.a` puede reducir significativamente el tamanio del ejecutable.

---

## Limpieza final

```bash
rm -f vec3.o vec3_print.o
rm -f libvec3.a
rm -f demo_vec3 demo_minimal demo_direct
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  main.c  main_minimal.c  vec3.c  vec3.h  vec3_print.c
```

---

## Conceptos reforzados

1. El flag `-c` de GCC compila sin enlazar, produciendo un archivo objeto
   (`.o`) marcado como "relocatable" por `file`. Los `.o` no tienen
   direcciones de memoria finales.

2. `nm` muestra los simbolos de un archivo objeto o biblioteca. `T` indica
   una funcion definida (text section), `U` indica un simbolo externo que
   debe resolverse al enlazar.

3. `ar rcs` empaqueta uno o mas `.o` en un archivo `.a` (biblioteca
   estatica) y crea un indice de simbolos. `ar t` lista los miembros y
   `nm` sobre el `.a` muestra los simbolos de cada miembro.

4. Para enlazar con una biblioteca estatica se usa `gcc main.c -L<dir>
   -l<nombre>`, donde GCC busca `lib<nombre>.a` en `<dir>`. El prefijo
   `lib` y la extension `.a` son implicitos.

5. El codigo de la biblioteca se copia dentro del ejecutable durante el
   enlace. El ejecutable resultante no necesita el `.a` en tiempo de
   ejecucion.

6. El linker extrae del `.a` solo los archivos objeto que contienen
   simbolos necesarios. La granularidad es el **archivo objeto completo**,
   no funciones individuales. Por eso separar funciones independientes en
   archivos `.c` distintos permite un enlace mas eficiente.

7. Al enlazar `.o` directamente (sin `.a`), el linker incluye todo el
   contenido de cada `.o` sin seleccion. Esto puede producir ejecutables
   mas grandes si se incluyen archivos objeto innecesarios.
