# Lab — Endianness

## Objetivo

Observar como el sistema almacena los bytes de valores multi-byte en memoria,
detectar el endianness de tu maquina, usar las funciones estandar de conversion
host/network, y escribir archivos binarios portables. Al finalizar, sabras
interpretar el orden de bytes en memoria e inspeccionar datos binarios con
herramientas como `xxd`.

## Prerequisitos

- GCC instalado
- `xxd` disponible (incluido con `vim` en la mayoria de distribuciones)
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `detect_endian.c` | Detecta endianness con cast a `uint8_t*` y con `union` |
| `byte_order.c` | Muestra los bytes en memoria de `uint16_t` y `uint32_t` |
| `net_convert.c` | Demuestra `htonl`, `ntohl`, `htons`, `ntohs` |
| `portable_file.c` | Escribe y lee enteros en big-endian a un archivo binario |

---

## Parte 1 — Detectar endianness del sistema

**Objetivo**: Determinar si tu sistema es little-endian o big-endian usando dos
tecnicas: cast a `uint8_t*` y `union`.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat detect_endian.c
```

Observa las dos funciones de deteccion:

- `is_little_endian_cast()` almacena el valor `1` en un `uint16_t` y examina el
  primer byte mediante un puntero `uint8_t*`
- `is_little_endian_union()` usa una `union` que superpone un `uint32_t` con un
  array de `uint8_t[4]`

Antes de compilar, predice:

- El valor `0x0001` tiene dos bytes: `00` y `01`. Si el sistema es
  little-endian, cual byte estara en la direccion mas baja?
- Ambos metodos daran el mismo resultado?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic detect_endian.c -o detect_endian
./detect_endian
```

Salida esperada (en x86/x86-64):

```
=== Deteccion de endianness ===

Metodo 1 (cast a uint8_t*):
  Resultado: Little-endian

Metodo 2 (union):
  Resultado: Little-endian

=== Verificacion con uint16_t ===
Valor: 0x0001
Byte en direccion baja [0]: 0x01
Byte en direccion alta [1]: 0x00
El byte menos significativo (01) esta primero -> Little-endian
```

El byte `01` (el menos significativo) esta en la direccion mas baja. Eso
confirma little-endian: el "little end" va primero.

### Paso 1.3 — Entender la logica

La clave de ambos metodos es la misma: almacenar un valor conocido y leer
el primer byte:

- Si `byte[0] == 1`, el byte menos significativo esta primero (little-endian)
- Si `byte[0] == 0`, el byte mas significativo esta primero (big-endian)

El metodo con `union` es mas legible. El metodo con cast a `uint8_t*` es mas
comun en codigo de bajo nivel.

### Limpieza de la parte 1

```bash
rm -f detect_endian
```

---

## Parte 2 — Visualizar el orden de bytes en memoria

**Objetivo**: Ver byte a byte como el sistema almacena valores de 16 y 32 bits.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat byte_order.c
```

Observa la funcion `print_bytes()`: recibe un puntero generico (`void*`),
lo castea a `uint8_t*`, y recorre cada byte imprimiendolo en hexadecimal.

Antes de compilar, predice el orden de bytes en memoria para estos valores:

- `0xABCD` (uint16_t, 2 bytes) -- cual byte aparecera primero: AB o CD?
- `0x01020304` (uint32_t, 4 bytes) -- cual byte aparecera primero: 01 o 04?
- `0xDEADBEEF` (uint32_t) -- cual byte aparecera primero: DE o EF?

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic byte_order.c -o byte_order
./byte_order
```

Salida esperada (en x86/x86-64):

```
=== Orden de bytes en memoria ===

--- uint16_t (2 bytes) ---
0xABCD                Valor: 0xABCD  Bytes en memoria: CD AB

--- uint32_t (4 bytes) ---
0x01020304            Valor: 0x01020304  Bytes en memoria: 04 03 02 01
0xDEADBEEF            Valor: 0xDEADBEEF  Bytes en memoria: EF BE AD DE
0x12345678            Valor: 0x12345678  Bytes en memoria: 78 56 34 12

--- Analisis detallado de 0x01020304 ---
Direccion base: <direccion>
  Offset +0: 0x04  (byte menos significativo)
  Offset +1: 0x03
  Offset +2: 0x02
  Offset +3: 0x01  (byte mas significativo)
```

### Paso 2.3 — Analizar los resultados

Observa el patron en little-endian:

- `0xABCD` se almacena como `CD AB` -- el byte `CD` (menos significativo) va
  primero
- `0x01020304` se almacena como `04 03 02 01` -- completamente invertido
  respecto a como escribimos el numero
- `0xDEADBEEF` se almacena como `EF BE AD DE` -- el famoso "dead beef" se lee
  al reves en memoria

Si el sistema fuera big-endian, los bytes estarian en el mismo orden en que
escribimos el numero: `01 02 03 04`.

### Limpieza de la parte 2

```bash
rm -f byte_order
```

---

## Parte 3 — Funciones de conversion host/network

**Objetivo**: Usar `htonl`, `ntohl`, `htons`, `ntohs` para convertir entre el
byte order del host y el de red (big-endian).

### Paso 3.1 — Examinar el codigo fuente

```bash
cat net_convert.c
```

Observa:

- `htons(port)` convierte el puerto 8080 de host byte order a network byte order
- `htonl(addr)` convierte una direccion IPv4 de host a network
- Se imprimen los bytes en memoria antes y despues de la conversion

Antes de compilar, predice:

- El puerto 8080 es `0x1F90`. En little-endian los bytes host son `90 1F`.
  Despues de `htons()`, los bytes seran `1F 90` o `90 1F`?
- `ntohl(htonl(x))` devolvera el valor original, o un valor diferente?

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic net_convert.c -o net_convert
./net_convert
```

Salida esperada (en x86/x86-64):

```
=== Funciones de conversion host <-> network ===

--- htons / ntohs (16 bits) ---

Puerto original:  8080 (0x1F90)
  Bytes host:     90 1F
Despues de htons: 36895 (0x901F)
  Bytes network:  1F 90
Despues de ntohs: 8080 (0x1F90)
  Bytes host:     90 1F

--- htonl / ntohl (32 bits) ---

Direccion original:  0xC0A80001 (192.168.0.1)
  Bytes host:        01 00 A8 C0
Despues de htonl:    0x0100A8C0
  Bytes network:     C0 A8 00 01
Despues de ntohl:    0xC0A80001
  Bytes host:        01 00 A8 C0

--- Verificacion ida y vuelta ---

Original:              0xDEADBEEF
ntohl(htonl(x)):       0xDEADBEEF
Coinciden: SI
```

### Paso 3.3 — Analizar los resultados

Observa los puntos clave:

1. **htons invierte los bytes en little-endian**. El puerto `0x1F90` pasa de
   bytes `90 1F` (host) a `1F 90` (network/big-endian). Ahora los bytes estan
   en el orden que la red espera.

2. **El valor numerico cambia** despues de `htons`: de 8080 a 36895. Esto es
   normal -- el patron de bits en memoria cambio, asi que interpretado como
   entero del host, el numero es diferente. Pero la red lo interpretara
   correctamente como 8080.

3. **ntohl(htonl(x)) == x** siempre. La conversion es su propia inversa.

4. **En una maquina big-endian**, `htonl`/`htons` serian no-ops (no harian
   nada) porque el host ya esta en network byte order.

### Limpieza de la parte 3

```bash
rm -f net_convert
```

---

## Parte 4 — Escribir y leer datos binarios portables

**Objetivo**: Escribir enteros a un archivo binario en un endianness definido
(big-endian) y verificar que la lectura recupera los valores originales.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat portable_file.c
```

Observa las funciones clave:

- `write_uint32_be()` descompone un `uint32_t` en 4 bytes usando shifts y
  mascaras, escribiendolos en orden big-endian (byte mas significativo primero)
- `read_uint32_be()` lee 4 bytes y los reconstruye con shifts

Este enfoque es portable: el archivo siempre tendra los bytes en big-endian,
sin importar el endianness del sistema que lo escribio.

Antes de compilar, predice:

- El valor `0x01020304` escrito con `write_uint32_be()` -- los bytes en el
  archivo seran `01 02 03 04` o `04 03 02 01`?
- Si usaramos `fwrite(&value, 4, 1, f)` directamente (sin la funcion portable),
  los bytes en el archivo serian iguales en todas las plataformas?

### Paso 4.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic portable_file.c -o portable_file
./portable_file
```

Salida esperada:

```
=== Escritura portable (big-endian) ===

Escrito uint32: 0x01020304
Escrito uint32: 0xDEADBEEF
Escrito uint32: 0x00000001
Escrito uint16: 0xABCD
Escrito uint16: 0x1F90

Archivo 'data.bin' creado.

=== Lectura portable ===

Leidos uint32:
  [0] 0x01020304  (correcto)
  [1] 0xDEADBEEF  (correcto)
  [2] 0x00000001  (correcto)
Leidos uint16:
  [0] 0xABCD  (correcto)
  [1] 0x1F90  (correcto)
```

Todos los valores se recuperaron correctamente. Las funciones de
escritura/lectura son independientes del endianness del host.

### Paso 4.3 — Inspeccionar el archivo binario con xxd

```bash
xxd data.bin
```

Salida esperada:

```
00000000: 0102 0304 dead beef 0000 0001 abcd 1f90  ................
```

Observa:

- `0x01020304` aparece como `01 02 03 04` en el archivo -- big-endian, el byte
  mas significativo primero
- `0xDEADBEEF` aparece como `DE AD BE EF` -- el orden "natural" de lectura
- `0xABCD` aparece como `AB CD`

Si hubieramos usado `fwrite(&value, 4, 1, f)` directamente en un sistema
little-endian, el archivo contendria `04 03 02 01` para el primer valor. Otro
sistema big-endian leeria ese archivo y obtendria `0x04030201` -- un valor
incorrecto.

### Paso 4.4 — Contraste: escritura NO portable

Para entender la diferencia, compara con lo que `fwrite` directamente produciria.
Ya viste en la Parte 2 que `0x01020304` se almacena en memoria como `04 03 02 01`
en little-endian. Un `fwrite` copiaria esos mismos bytes al archivo, produciendo
un archivo que solo se leeria correctamente en otro sistema little-endian.

La solucion del lab (descomponer con shifts) garantiza que el archivo siempre
tiene los bytes en el mismo orden, sin importar la plataforma.

### Limpieza de la parte 4

```bash
rm -f portable_file data.bin
```

---

## Limpieza final

```bash
rm -f detect_endian byte_order net_convert portable_file data.bin
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  byte_order.c  detect_endian.c  net_convert.c  portable_file.c
```

---

## Conceptos reforzados

1. En little-endian (x86/x86-64), el byte menos significativo se almacena en
   la direccion de memoria mas baja. El valor `0x01020304` se almacena como
   `04 03 02 01`.

2. Endianness se puede detectar en runtime almacenando un valor conocido y
   leyendo sus bytes individuales con un cast a `uint8_t*` o con una `union`.

3. Las funciones `htonl`, `htons`, `ntohl`, `ntohs` convierten entre el byte
   order del host y el de red (big-endian). En una maquina little-endian,
   invierten los bytes. En una big-endian, no hacen nada.

4. Despues de `htons(8080)`, el valor numerico interpretado por el host cambia
   (a 36895), pero los bytes en memoria estan ahora en el orden que la red
   espera. Esto es correcto -- no se debe usar el valor convertido para
   aritmetica local.

5. Para escribir archivos binarios portables, se deben serializar los enteros
   byte a byte usando shifts y mascaras, en un endianness definido. Usar
   `fwrite(&value, ...)` directamente produce archivos que dependen del
   endianness del sistema que los escribio.

6. Las operaciones aritmeticas y de bits trabajan sobre el valor abstracto del
   entero, no sobre sus bytes en memoria. Endianness solo importa cuando se
   accede a los bytes individuales (con `uint8_t*`, `fwrite` directo, o
   protocolos de red).
