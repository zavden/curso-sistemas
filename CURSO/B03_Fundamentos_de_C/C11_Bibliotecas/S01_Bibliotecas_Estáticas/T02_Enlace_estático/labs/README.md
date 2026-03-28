# Lab -- Enlace estatico

## Objetivo

Practicar el proceso de enlace estatico: construir bibliotecas `.a`, enlazar
con `-L` y `-l`, demostrar que el orden de las bibliotecas importa, inspeccionar
simbolos con `nm`, verificar dependencias con `ldd`, comparar enlace dinamico
contra `gcc -static`, y diagnosticar errores comunes del linker (undefined
reference, multiple definition, cannot find).

## Prerequisitos

- GCC instalado
- `ar`, `nm` y `ldd` disponibles
- Haber completado T01 (creacion de bibliotecas estaticas)
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `format.h` / `format.c` | `format_name()` -- formatea un nombre entre corchetes |
| `greet.h` / `greet.c` | `greet_person()` -- saluda usando `format_name()` (depende de libformat) |
| `main.c` | Programa que llama a `greet_person()` |
| `greet_duplicate.c` | Definicion duplicada de `greet_person()` para provocar error |
| `calc.h` / `calc.c` | `calc_add()` y `calc_sub()` -- biblioteca independiente |
| `main_calc.c` | Programa que usa libcalc (para enlace completamente estatico) |

---

## Parte 1 -- Preparar las bibliotecas

**Objetivo**: Compilar los archivos objeto y crear dos bibliotecas estaticas
con dependencia entre ellas: `libgreet.a` depende de `libformat.a`.

### Paso 1.1 -- Examinar las dependencias entre archivos

```bash
cat format.h
cat greet.c
cat main.c
```

Observa la cadena de dependencias:

- `main.c` llama a `greet_person()` (definida en `greet.c`)
- `greet.c` llama a `format_name()` (definida en `format.c`)
- `format.c` llama a `snprintf()` (definida en libc)

### Paso 1.2 -- Compilar los archivos objeto

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c format.c -o format.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c greet.c -o greet.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c main.c -o main.o
```

No debe producir warnings ni errores.

### Paso 1.3 -- Crear las bibliotecas estaticas

```bash
ar rcs libformat.a format.o
ar rcs libgreet.a greet.o
```

### Paso 1.4 -- Verificar los simbolos de cada biblioteca

```bash
nm libformat.a
```

Salida esperada (las direcciones varian):

```
format.o:
0000000000000000 d buf.0
0000000000000000 T format_name
                 U snprintf
```

`T` = simbolo definido (Text). `U` = simbolo indefinido (Undefined).

```bash
nm libgreet.a
```

Salida esperada:

```
greet.o:
                 U format_name
0000000000000000 T greet_person
                 U printf
```

Observa: `libgreet.a` tiene `format_name` como `U` (undefined). Eso significa
que al enlazar, el linker necesitara encontrar `format_name` en otra biblioteca.

### Paso 1.5 -- Mover bibliotecas a un subdirectorio

Para practicar `-L`, mueve las bibliotecas a `lib/`:

```bash
mkdir -p lib
mv libformat.a libgreet.a lib/
```

---

## Parte 2 -- Enlace correcto con -L y -l

**Objetivo**: Enlazar el programa usando `-L` para indicar el directorio y `-l`
para nombrar las bibliotecas, en el orden correcto.

### Paso 2.1 -- Predecir el orden correcto

Antes de enlazar, responde mentalmente:

- `main.o` necesita `greet_person` -- que biblioteca lo provee?
- `greet.o` (dentro de libgreet) necesita `format_name` -- que biblioteca lo provee?
- Segun la regla "dependencias de izquierda a derecha", en que orden deben ir
  `-lgreet` y `-lformat`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2 -- Enlazar en el orden correcto

```bash
gcc main.o -L./lib -lgreet -lformat -o greet_demo
```

El comando debe completar sin errores.

### Paso 2.3 -- Ejecutar el programa

```bash
./greet_demo
```

Salida esperada:

```
=== Static linking demo ===
Hello, [Alice]!
Hello, [Bob]!
```

El linker resolvio toda la cadena: `main.o` -> `greet_person` (de libgreet.a) ->
`format_name` (de libformat.a) -> `snprintf` (de libc).

### Paso 2.4 -- Verificar con nm que los simbolos estan en el binario

```bash
nm greet_demo | grep -E "greet_person|format_name"
```

Salida esperada (las direcciones varian):

```
<addr> T format_name
<addr> T greet_person
```

Ambos simbolos tienen `T` -- fueron copiados dentro del ejecutable. Ya no
dependen de las bibliotecas `.a`.

### Paso 2.5 -- Alternativa: ruta directa al .a

En vez de `-L` y `-l`, puedes pasar la ruta completa del `.a`:

```bash
gcc main.o ./lib/libgreet.a ./lib/libformat.a -o greet_demo_direct
./greet_demo_direct
```

Salida esperada:

```
=== Static linking demo ===
Hello, [Alice]!
Hello, [Bob]!
```

La diferencia: con `-l`, el linker busca `.so` primero y despues `.a`. Con ruta
directa, se usa exactamente el archivo indicado.

### Limpieza de la parte 2

```bash
rm -f greet_demo greet_demo_direct
```

---

## Parte 3 -- El orden de las bibliotecas importa

**Objetivo**: Demostrar que el linker procesa argumentos de izquierda a derecha
y que el orden incorrecto causa errores.

### Paso 3.1 -- Orden invertido de bibliotecas

```bash
gcc main.o -L./lib -lformat -lgreet -o greet_demo 2>&1
```

Salida esperada:

```
/usr/bin/ld: ./lib/libgreet.a(greet.o): in function `greet_person':
greet.c:(.text+0x...): undefined reference to `format_name'
collect2: error: ld returned 1 exit status
```

El linker proceso `libformat.a` primero. En ese momento no habia simbolos
undefined que format pudiera resolver, asi que no incluyo nada. Despues leyo
`libgreet.a`, encontro `greet_person` (que `main.o` necesitaba), pero
`greet_person` necesita `format_name` -- y `libformat.a` ya paso.

### Paso 3.2 -- Bibliotecas antes del .o

```bash
gcc -L./lib -lgreet -lformat main.o -o greet_demo 2>&1
```

Salida esperada:

```
/usr/bin/ld: main.o: in function `main':
main.c:(.text+0x...): undefined reference to `greet_person'
collect2: error: ld returned 1 exit status
```

El linker leyo las bibliotecas primero. Sin ningun simbolo undefined todavia,
no incluyo nada de ellas. Cuando finalmente leyo `main.o`, encontro
`greet_person` como undefined, pero las bibliotecas ya fueron procesadas.

### Paso 3.3 -- Ver el proceso del linker con --trace

```bash
gcc main.o -L./lib -lgreet -lformat -o greet_demo -Wl,--trace
```

Salida esperada:

```
/usr/bin/ld: mode elf_x86_64
/usr/lib/...crt1.o
/usr/lib/...crti.o
...
main.o
(./lib/libgreet.a)greet.o
(./lib/libformat.a)format.o
...
```

Observa: el linker extrajo `greet.o` de `libgreet.a` y `format.o` de
`libformat.a`. Solo extrajo los `.o` que contenian simbolos necesarios --
esto es la **resolucion selectiva**.

### Limpieza de la parte 3

```bash
rm -f greet_demo
```

---

## Parte 4 -- Enlace completamente estatico y verificacion

**Objetivo**: Comparar enlace normal (libc dinamica) contra `gcc -static`
(todo estatico), usando `ldd` y `nm` para verificar.

### Paso 4.1 -- Preparar libcalc

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c calc.c -o calc.o
ar rcs libcalc.a calc.o
gcc -std=c11 -Wall -Wextra -Wpedantic -c main_calc.c -o main_calc.o
```

### Paso 4.2 -- Enlace normal (libc dinamica)

```bash
gcc main_calc.o libcalc.a -o calc_dynamic
```

### Paso 4.3 -- Verificar dependencias con ldd

```bash
ldd calc_dynamic
```

Salida esperada:

```
	linux-vdso.so.1 (0x...)
	libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x...)
	/lib64/ld-linux-x86-64.so.2 (0x...)
```

El binario depende de `libc.so.6` (dinamica). Las funciones de `libcalc.a`
no aparecen porque fueron copiadas dentro del ejecutable (enlace estatico).

### Paso 4.4 -- Verificar simbolos de libcalc en el binario

```bash
nm calc_dynamic | grep calc_
```

Salida esperada:

```
<addr> T calc_add
<addr> T calc_sub
```

Los simbolos de libcalc estan en el binario con `T` (definidos). Fueron copiados
durante el enlace estatico, aunque libc sigue siendo dinamica.

### Paso 4.5 -- Predecir el efecto de -static

Antes de compilar con `-static`, responde mentalmente:

- Que mostrara `ldd` para un binario completamente estatico?
- El tamano del binario sera mayor o menor que la version dinamica?
- Que ventaja tiene un binario completamente estatico?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.6 -- Enlace completamente estatico

```bash
gcc -static main_calc.o libcalc.a -o calc_static
```

Si este comando falla con un error como `cannot find -lc`, necesitas instalar
las bibliotecas estaticas de libc. En Fedora: `sudo dnf install glibc-static`.
En Debian/Ubuntu: `sudo apt install libc-dev`.

### Paso 4.7 -- Comparar con ldd

```bash
ldd calc_static
```

Salida esperada:

```
	not a dynamic executable
```

El binario no tiene dependencias externas. Incluye todo: libc, las funciones de
libcalc, y el startup code del sistema.

### Paso 4.8 -- Comparar tamanos

```bash
ls -lh calc_dynamic calc_static
```

Salida esperada (tamanos aproximados):

```
-rwxr-xr-x 1 <user> <group> ~16K <date> calc_dynamic
-rwxr-xr-x 1 <user> <group> ~800K <date> calc_static
```

El binario estatico es mucho mas grande porque incluye toda libc. A cambio, se
puede copiar a cualquier sistema Linux de la misma arquitectura y ejecutar sin
instalar nada.

### Paso 4.9 -- Ejecutar ambos y comparar

```bash
./calc_dynamic
./calc_static
```

Salida esperada (identica para ambos):

```
add(3, 4) = 7
sub(10, 6) = 4
```

El comportamiento es identico. La diferencia es solo en dependencias y tamano.

### Limpieza de la parte 4

```bash
rm -f calc.o main_calc.o libcalc.a calc_dynamic calc_static
```

---

## Parte 5 -- Diagnostico de errores del linker

**Objetivo**: Provocar intencionalmente los tres errores mas comunes del linker
y aprender a diagnosticarlos.

### Paso 5.1 -- Error: undefined reference (simbolo faltante)

Intenta enlazar sin libformat:

```bash
gcc main.o -L./lib -lgreet -o greet_demo 2>&1
```

Salida esperada:

```
/usr/bin/ld: ./lib/libgreet.a(greet.o): in function `greet_person':
greet.c:(.text+0x...): undefined reference to `format_name'
collect2: error: ld returned 1 exit status
```

Diagnostico con `nm`:

```bash
nm -A lib/lib*.a | grep format_name
```

Salida esperada:

```
lib/libformat.a:format.o:0000000000000000 T format_name
lib/libgreet.a:greet.o:                 U format_name
```

El simbolo existe en `libformat.a` (con `T`). La solucion: agregar `-lformat`
despues de `-lgreet`.

### Paso 5.2 -- Error: cannot find -l

Intenta enlazar con una biblioteca que no existe en la ruta de busqueda:

```bash
gcc main.o -lgreet -o greet_demo 2>&1
```

Salida esperada:

```
/usr/bin/ld: cannot find -lgreet: No such file or directory
collect2: error: ld returned 1 exit status
```

El linker busco `libgreet.a` (y `libgreet.so`) en los directorios por defecto
(`/usr/lib`, `/usr/lib64`, etc.) y no la encontro. La solucion: agregar
`-L./lib` para indicar donde buscar.

### Paso 5.3 -- Error: multiple definition

Compila la version duplicada y crea una biblioteca con dos definiciones del
mismo simbolo:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c greet_duplicate.c -o greet_duplicate.o
```

Ahora enlaza incluyendo ambos `.o` (el original y el duplicado):

```bash
gcc main.o greet.o greet_duplicate.o ./lib/libformat.a -o greet_demo 2>&1
```

Salida esperada:

```
/usr/bin/ld: greet_duplicate.o: in function `greet_person':
greet_duplicate.c:(.text+0x...): multiple definition of `greet_person'; greet.o:greet.c:(.text+0x...): first defined here
collect2: error: ld returned 1 exit status
```

Diagnostico con `nm`:

```bash
nm -A greet.o greet_duplicate.o | grep greet_person
```

Salida esperada:

```
greet.o:0000000000000000 T greet_person
greet_duplicate.o:0000000000000000 T greet_person
```

Ambos `.o` definen `greet_person` con `T`. El linker no puede elegir -- produce
un error. La solucion: eliminar la definicion duplicada, o marcar una como
`static` si es interna.

### Limpieza de la parte 5

```bash
rm -f greet_duplicate.o greet_demo
```

---

## Limpieza final

```bash
rm -f main.o format.o greet.o greet_duplicate.o calc.o main_calc.o
rm -f libcalc.a
rm -rf lib/
rm -f greet_demo greet_demo_direct calc_dynamic calc_static
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  calc.c  calc.h  format.c  format.h  greet.c  greet_duplicate.c  greet.h  main.c  main_calc.c
```

---

## Conceptos reforzados

1. El linker procesa argumentos de izquierda a derecha. Las bibliotecas deben
   aparecer **despues** de los `.o` que las necesitan, y las dependencias
   transitivas deben ir despues de quien las usa: `main.o -lgreet -lformat`.

2. `-L<dir>` agrega un directorio de busqueda para `-l<name>`. Sin `-L`, el
   linker solo busca en los directorios por defecto del sistema.

3. Con `-l`, el linker prefiere `.so` sobre `.a`. Pasar la ruta completa al
   `.a` fuerza enlace estatico de esa biblioteca especifica.

4. La resolucion es selectiva: el linker solo extrae los `.o` del `.a` que
   contienen simbolos necesarios. Se puede verificar con `gcc -Wl,--trace`.

5. `nm` muestra los simbolos de un binario o biblioteca. `T` = definido,
   `U` = indefinido. Es la herramienta principal para diagnosticar por que
   un simbolo no se resuelve.

6. `ldd` muestra las bibliotecas dinamicas de las que depende un binario. Si
   una biblioteca `.a` fue enlazada estaticamente, no aparece en `ldd`.

7. `gcc -static` produce un binario completamente estatico (incluye libc).
   El resultado es mucho mas grande pero no tiene dependencias externas.

8. Los tres errores clasicos del linker son: `undefined reference` (simbolo no
   encontrado -- verificar orden o falta de `-l`), `cannot find -l` (biblioteca
   no encontrada -- falta `-L` o no esta instalada), y `multiple definition`
   (mismo simbolo definido en dos `.o` -- eliminar duplicado o usar `static`).
