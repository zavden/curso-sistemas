# Lab — fread, fwrite, fgets, fputs

## Objetivo

Practicar la lectura y escritura de archivos en modo texto (fgets/fputs) y en
modo binario (fread/fwrite). Al finalizar, sabras manejar buffers parciales con
fgets, serializar y deserializar structs con fread/fwrite, comparar rendimiento
entre copia byte a byte y copia por bloques, y usar correctamente feof/ferror
para detectar fin de archivo.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `text_lines.c` | Escritura con fputs y lectura con fgets (buffer pequeno) |
| `strip_newline.c` | Eliminar el '\n' de las lineas leidas con fgets |
| `binary_structs.c` | Escribir y leer structs con fwrite/fread |
| `copy_file.c` | Copiar archivos: byte a byte vs bloques de 4096 |
| `eof_error.c` | Uso correcto e incorrecto de feof y ferror |

---

## Parte 1 — fgets y fputs: I/O de texto linea a linea

**Objetivo**: Entender como fgets maneja el '\n', que ocurre cuando el buffer es
mas pequeno que la linea, y como fputs escribe sin agregar '\n' automaticamente.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat text_lines.c
```

Observa:

- `fputs` escribe 4 lineas al archivo, la ultima **sin '\n'** al final
- `fgets` lee con un buffer de solo 20 bytes
- El programa muestra la longitud de cada fragmento y si contiene '\n'

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic text_lines.c -o text_lines
```

Antes de ejecutar, predice:

- La linea "short\n" cabe en 20 bytes. Se leera completa en un chunk?
- La linea "a medium-length line of text\n" tiene 29 caracteres. En cuantos
  chunks se dividira?
- La ultima linea no tiene '\n'. Su ultimo chunk mostrara newline=yes o no?

```bash
./text_lines
```

Salida esperada:

```
=== Reading with buffer of 20 bytes ===

chunk  1 | len= 6 | newline=yes | "short
"
chunk  2 | len=19 | newline=no  | "a medium-length lin"
chunk  3 | len=10 | newline=yes | "e of text
"
chunk  4 | len=19 | newline=no  | "this is a deliberat"
chunk  5 | len=19 | newline=no  | "ely long line that "
chunk  6 | len=19 | newline=no  | "exceeds a small buf"
chunk  7 | len=16 | newline=yes | "fer size easily
"
chunk  8 | len=19 | newline=no  | "last line without n"
chunk  9 | len= 6 | newline=no  | "ewline"
```

Analisis:

- `fgets(buf, 20, f)` lee como maximo 19 caracteres (reserva 1 para '\0')
- "short\n" cabe entera en el buffer: len=6, newline=yes
- "a medium-length line of text\n" no cabe: se divide en chunk 2 (19 chars, sin
  '\n') y chunk 3 (10 chars, con '\n'). La presencia de '\n' al final indica
  que la linea logica esta completa
- La ultima linea no tiene '\n', asi que su ultimo chunk (chunk 9) muestra
  newline=no. Esto es indistinguible de una linea truncada por buffer pequeno

### Paso 1.3 — Examinar el archivo creado

```bash
cat -A sample.txt
```

El flag `-A` muestra caracteres especiales: `$` marca el fin de cada linea (el
'\n'). La ultima linea no tendra `$` al final porque fputs no agrego '\n'.

```bash
wc -c sample.txt
```

Confirma el tamano total en bytes.

### Paso 1.4 — Quitar el '\n' con strcspn

```bash
cat strip_newline.c
```

Observa la linea clave:

```c
buf[strcspn(buf, "\n")] = '\0';
```

`strcspn(buf, "\n")` retorna la posicion del primer '\n' en el string (o la
longitud total si no hay '\n'). Al poner '\0' en esa posicion, se elimina el
salto de linea.

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic strip_newline.c -o strip_newline
./strip_newline
```

Salida esperada:

```
=== Stripping newlines with strcspn ===

  1: "short"
  2: "a medium-length line of text"
  3: "this is a deliberately long line that exceeds a small buffer size easily"
  4: "last line without newline"
```

Ahora el buffer de 256 bytes es suficiente para todas las lineas, y `strcspn`
elimina el '\n' de cada una.

### Limpieza de la parte 1

```bash
rm -f text_lines strip_newline sample.txt
```

---

## Parte 2 — fread y fwrite: I/O binario con structs

**Objetivo**: Serializar un array de structs a un archivo binario con fwrite,
leerlo de vuelta con fread, y verificar que los datos son identicos.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat binary_structs.c
```

Observa:

- El struct `Sensor` tiene campos de tipos distintos: `int`, `char[32]`, `double`
- Se escribe primero `count` (un int) como encabezado, luego el array completo
- Se lee el `count` primero para saber cuantos structs leer
- Se usa `memcmp` para verificar que los datos leidos son identicos

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic binary_structs.c -o binary_structs
./binary_structs
```

Salida esperada:

```
Wrote 3 of 3 structs to sensors.bin
sizeof(struct Sensor) = ~48 bytes
Total file size = ~148 bytes (approx)

File says it contains 3 structs
Read 3 structs:
  id=1    label=temperature   value=23.50
  id=2    label=humidity      value=61.20
  id=3    label=pressure      value=1013.25

Verification: PASS - data matches exactly
```

El tamano del struct puede variar segun la arquitectura por el padding del
compilador. En x86-64 tipicamente sera 48 bytes (4 int + 32 char + 4 padding +
8 double).

### Paso 2.3 — Inspeccionar el archivo binario

```bash
xxd sensors.bin | head -20
```

Observa:

- Los primeros 4 bytes son el valor `3` (little-endian: `03 00 00 00`)
- Despues aparece el primer struct con `01 00 00 00` (id=1), seguido de
  "temperature" en ASCII y bytes nulos de padding, y luego el double en
  formato IEEE 754

```bash
file sensors.bin
```

El comando `file` probablemente dira "data" — no hay formato estandar, es un
volcado crudo de memoria.

### Paso 2.4 — Endianness y portabilidad

El archivo que acabas de crear **no es portable**. Si copiaras `sensors.bin` a
una maquina big-endian, los ints y doubles se leerian con los bytes invertidos.

Verifica el endianness de tu maquina:

```bash
echo -n I | od -to2 | head -1
```

Si ves `0000001` al inicio, tu maquina es little-endian (lo mas comun en x86).

Para archivos binarios portables, se deberia serializar campo por campo con
tipos de tamano fijo (`uint32_t`, `uint64_t`) y un endianness definido. El
README.md del topico detalla estas consideraciones.

### Limpieza de la parte 2

```bash
rm -f binary_structs sensors.bin
```

---

## Parte 3 — Copiar archivos: byte a byte vs bloques

**Objetivo**: Implementar dos estrategias de copia de archivos y medir la
diferencia de rendimiento entre fgetc/fputc (byte a byte) y fread/fwrite
(bloques de 4096 bytes).

### Paso 3.1 — Examinar el codigo fuente

```bash
cat copy_file.c
```

Observa las dos funciones:

- `copy_byte_by_byte` usa `fgetc`/`fputc` en un loop: un syscall (buffered) por
  cada byte
- `copy_block` usa `fread`/`fwrite` con un buffer de 4096 bytes: un syscall
  (buffered) por cada bloque
- `clock()` mide el tiempo de CPU de cada estrategia

### Paso 3.2 — Crear un archivo de prueba

```bash
dd if=/dev/urandom of=testfile.bin bs=1024 count=500
```

Esto crea un archivo de ~500 KB con datos aleatorios.

Antes de ejecutar el programa, predice:

- Cual sera mas rapido: byte a byte o por bloques?
- Por cuanto? 2x? 5x? 50x?

### Paso 3.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic copy_file.c -o copy_file
./copy_file testfile.bin
```

Salida esperada (los tiempos varian):

```
Byte-by-byte: 512000 bytes in ~4 ms
Block (4096): 512000 bytes in ~1 ms
Speedup:      ~5x faster with block copy
```

Aunque stdio usa buffers internos (tipicamente 4096 u 8192 bytes), la copia por
bloques sigue siendo mas rapida porque reduce la cantidad de llamadas a funciones
de la libreria C. Con archivos mas grandes, la diferencia es mas notable.

### Paso 3.4 — Verificar la integridad de las copias

```bash
cmp testfile.bin copy_byte.tmp && echo "Byte copy: identical"
cmp testfile.bin copy_block.tmp && echo "Block copy: identical"
```

Salida esperada:

```
Byte copy: identical
Block copy: identical
```

Ambas copias son bit-a-bit identicas al original. `cmp` sale silenciosamente
si los archivos son iguales, y muestra la primera diferencia si no lo son.

### Paso 3.5 — Probar con un archivo mas grande (opcional)

```bash
dd if=/dev/urandom of=bigfile.bin bs=1024 count=5000
./copy_file bigfile.bin
```

Con ~5 MB la diferencia de rendimiento sera mas evidente.

### Limpieza de la parte 3

```bash
rm -f copy_file testfile.bin bigfile.bin copy_byte.tmp copy_block.tmp
```

---

## Parte 4 — feof y ferror: detectar fin de archivo correctamente

**Objetivo**: Demostrar por que `while (!feof(f))` es un error comun y como usar
correctamente el valor de retorno de fread para controlar el loop.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat eof_error.c
```

Observa los tres bloques:

1. Leer mas elementos de los disponibles (pedir 10, solo hay 5)
2. El loop incorrecto con `while (!feof(f))` — produce una iteracion extra
3. El loop correcto con `while (fread(...) == 1)` — exactamente 5 iteraciones

### Paso 4.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic eof_error.c -o eof_error
./eof_error
```

Salida esperada:

```
Wrote 5 ints (20 bytes) to numbers.bin

Requested 10 ints, got 5
feof()   = 1
ferror() = 0

Values read:
  buf[0] = 10
  buf[1] = 20
  buf[2] = 30
  buf[3] = 40
  buf[4] = 50

=== Common mistake: while(!feof(f)) ===

  iteration 1: fread returned 1, val=10, feof=0
  iteration 2: fread returned 1, val=20, feof=0
  iteration 3: fread returned 1, val=30, feof=0
  iteration 4: fread returned 1, val=40, feof=0
  iteration 5: fread returned 1, val=50, feof=0
  iteration 6: fread returned 0, val=50, feof=1
Total iterations: 6 (expected 5, got 6 -- one extra!)

=== Correct: check fread return value ===

  iteration 1: val=10
  iteration 2: val=20
  iteration 3: val=30
  iteration 4: val=40
  iteration 5: val=50
Total iterations: 5 (correct!)
Stopped because: end of file
```

### Paso 4.3 — Analisis del problema

El error con `while (!feof(f))` ocurre porque `feof()` **solo devuelve true
despues** de que una lectura intento pasar el final del archivo. La secuencia
es:

1. Iteracion 5: fread lee el ultimo int (50). El cursor queda al final del
   archivo, pero feof **aun es 0** porque no se ha intentado leer mas alla.
2. Iteracion 6: el loop entra porque feof sigue siendo 0. fread intenta leer
   pero no hay datos: retorna 0 y ahora feof es 1. Pero `val` conserva el
   valor anterior (50), lo que procesa un dato duplicado.

La solucion correcta es verificar el valor de retorno de fread directamente:

```c
while (fread(&val, sizeof(int), 1, fr) == 1) {
    /* process val */
}
```

Despues del loop, se usa `feof()` y `ferror()` para distinguir si se termino
por fin de archivo o por un error de lectura.

### Paso 4.4 — Aplicar la misma logica a fgets

El mismo principio aplica a fgets en modo texto:

```c
/* INCORRECTO */
while (!feof(f)) {
    fgets(buf, sizeof(buf), f);
    /* puede procesar una linea vacia extra */
}

/* CORRECTO */
while (fgets(buf, sizeof(buf), f) != NULL) {
    /* process buf */
}
```

Regla general: **siempre usar el valor de retorno de la funcion de lectura como
condicion del loop**, y reservar feof/ferror para diagnosticar la causa despues.

### Limpieza de la parte 4

```bash
rm -f eof_error numbers.bin
```

---

## Limpieza final

```bash
rm -f *.o text_lines strip_newline binary_structs copy_file eof_error
rm -f sample.txt sensors.bin numbers.bin testfile.bin bigfile.bin
rm -f copy_byte.tmp copy_block.tmp
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  binary_structs.c  copy_file.c  eof_error.c  strip_newline.c  text_lines.c
```

---

## Conceptos reforzados

1. `fgets(buf, n, f)` lee como maximo `n-1` caracteres e **incluye el '\n'** en
   el buffer si la linea cabe. La presencia o ausencia de '\n' al final del
   buffer indica si la linea se leyo completa o fue truncada.

2. `fputs` **no agrega '\n'** automaticamente, a diferencia de `puts`. Cada '\n'
   debe ser explicito en el string que se escribe.

3. `strcspn(buf, "\n")` es la forma mas limpia de localizar y eliminar el '\n'
   que fgets incluye, porque funciona correctamente tanto si el '\n' esta
   presente como si no.

4. `fwrite` y `fread` trabajan con datos binarios crudos — escriben y leen la
   representacion en memoria de los datos, incluyendo padding del struct y el
   endianness de la maquina. Esto los hace rapidos pero **no portables** entre
   arquitecturas distintas.

5. Copiar un archivo con `fread`/`fwrite` en bloques de 4096 bytes es
   significativamente mas rapido que hacerlo byte a byte con `fgetc`/`fputc`,
   incluso con el buffering interno de stdio, porque reduce la cantidad de
   llamadas a funciones de la libreria.

6. `while (!feof(f))` es un error comun: `feof()` solo devuelve true **despues**
   de que una lectura fallo por haber alcanzado el final del archivo. La forma
   correcta es usar el valor de retorno de `fread` o `fgets` como condicion del
   loop, y usar `feof`/`ferror` despues para diagnosticar la causa de la
   terminacion.
