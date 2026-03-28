# Lab — lseek

## Objetivo

Practicar el posicionamiento dentro de archivos con `lseek`, usando los tres
modos (`SEEK_SET`, `SEEK_CUR`, `SEEK_END`). Al finalizar, sabras obtener el
tamano de un archivo, implementar acceso aleatorio a registros de tamano fijo,
y crear sparse files con huecos que no ocupan espacio en disco.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `seek_basics.c` | Posicionamiento con SEEK_SET, SEEK_CUR y SEEK_END |
| `file_size.c` | Obtener tamano de archivo sin perder la posicion |
| `records.c` | Base de datos de registros con acceso aleatorio |
| `sparse.c` | Crear y verificar un sparse file de 100 MB |

---

## Parte 1 — SEEK_SET, SEEK_CUR, SEEK_END

**Objetivo**: Entender los tres modos de posicionamiento de `lseek` y como
interactuan con `read`.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat seek_basics.c
```

Observa la estructura:

- Se crea un archivo con el alfabeto "ABCDEFGHIJKLMNOPQRSTUVWXYZ" (26 bytes)
- Se usan `SEEK_SET`, `SEEK_CUR` y `SEEK_END` para moverse a distintas posiciones
- Despues de cada `lseek`, se lee 3 bytes para verificar donde estamos

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o seek_basics seek_basics.c
```

Antes de ejecutar, predice:

- Si el archivo contiene "ABCDE..." y hacemos `lseek(fd, 10, SEEK_SET)`,
  que 3 letras leeremos?
- Si estamos en la posicion 13 y hacemos `lseek(fd, 5, SEEK_CUR)`,
  a que posicion vamos? Que letras leeremos?
- Si hacemos `lseek(fd, -3, SEEK_END)`, que 3 letras leeremos?

### Paso 1.3 — Verificar las predicciones

```bash
./seek_basics
```

Salida esperada:

```
Wrote 26 bytes: "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

After lseek(fd, 0, SEEK_SET):  position = 0
  read 3 bytes: "ABC"

After lseek(fd, 10, SEEK_SET): position = 10
  read 3 bytes: "KLM"

Current position after read:   position = 13
After lseek(fd, 5, SEEK_CUR):  position = 18
  read 3 bytes: "STU"

After lseek(fd, -6, SEEK_CUR): position = 15
  read 3 bytes: "PQR"

After lseek(fd, 0, SEEK_END):  position = 26
After lseek(fd, -3, SEEK_END): position = 23
  read 3 bytes: "XYZ"
```

Analisis:

- `SEEK_SET` posiciona desde el inicio del archivo. Posicion 10 corresponde
  a la letra 'K' (las letras empiezan en 0: A=0, B=1, ..., K=10).
- `SEEK_CUR` se mueve respecto a la posicion actual. Cada `read` avanza la
  posicion automaticamente, por eso despues de leer 3 bytes en posicion 10,
  la posicion queda en 13.
- `SEEK_END` se mueve desde el final. `-3` significa 3 bytes antes del final,
  que son las ultimas 3 letras: "XYZ".
- `lseek(fd, 0, SEEK_CUR)` es el patron para obtener la posicion actual sin
  moverla (equivale a `ftell`).

### Limpieza de la parte 1

```bash
rm -f seek_basics seek_test.dat
```

---

## Parte 2 — Obtener tamano de archivo

**Objetivo**: Usar `lseek(fd, 0, SEEK_END)` para obtener el tamano de un
archivo, preservando la posicion actual.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat file_size.c
```

Observa la funcion `get_file_size`:

1. Guarda la posicion actual con `lseek(fd, 0, SEEK_CUR)`
2. Salta al final con `lseek(fd, 0, SEEK_END)` -- el valor de retorno es el tamano
3. Restaura la posicion original con `lseek(fd, saved, SEEK_SET)`

Este patron de 3 pasos es esencial: sin restaurar la posicion, la siguiente
operacion de lectura/escritura ocurriria en un lugar inesperado.

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o file_size file_size.c
```

Antes de ejecutar, predice:

- "Hello, lseek!" tiene cuantos bytes?
- Despues de llamar a `get_file_size`, la posicion del fd cambiara?

### Paso 2.3 — Verificar las predicciones

```bash
./file_size
```

Salida esperada:

```
File size:          13 bytes
Position before:    13
Position after:     13
Position preserved: yes

After writing more:  29 bytes
Expected:            29 bytes
```

Analisis:

- "Hello, lseek!" tiene 13 caracteres (incluyendo la coma y el espacio).
- La posicion se preserva porque `get_file_size` la restaura despues de ir
  al final. Si no hiciera esto, las escrituras posteriores ocurririan al
  final en lugar de donde esperamos.
- El tamano actualizado (29 bytes) demuestra que `get_file_size` funciona
  en cualquier momento, no solo al principio.

### Paso 2.4 — Verificar con herramientas del sistema

```bash
ls -l size_test.dat
```

Salida esperada:

```
-rw-r--r-- 1 ... 29 ... size_test.dat
```

El tamano reportado por `ls` debe coincidir con lo que `get_file_size` retorno.

### Limpieza de la parte 2

```bash
rm -f file_size size_test.dat
```

---

## Parte 3 — Acceso aleatorio a registros

**Objetivo**: Implementar lectura y escritura de registros de tamano fijo en
posiciones arbitrarias, usando `lseek` para calcular el offset de cada registro.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat records.c
```

Observa las funciones clave:

- `write_record(fd, index, rec)` -- calcula `index * sizeof(struct Record)` y
  escribe en esa posicion
- `read_record(fd, index, rec)` -- misma logica para lectura
- `count_records(fd)` -- usa `get_file_size / sizeof` para contar registros

La aritmetica es simple: si cada registro ocupa N bytes, el registro con
indice `i` empieza en el byte `i * N`.

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o records records.c
```

Antes de ejecutar, predice:

- Si `sizeof(struct Record)` es, digamos, 64 bytes, y hay 5 registros,
  cuantos bytes tendra el archivo?
- Si actualizamos el registro en indice 1, los demas registros se veran
  afectados?

### Paso 3.3 — Verificar las predicciones

```bash
./records
```

Salida esperada (el sizeof puede variar segun la plataforma):

```
sizeof(struct Record) = 72 bytes

Wrote 5 records. Total in file: 5

--- Reading index 2 ---
  id=3    name=Charlie               score=8.8

--- Reading index 0 ---
  id=1    name=Alice                 score=9.5

--- Updating index 1 ---
Updated.

--- All records ---
  id=1    name=Alice                 score=9.5
  id=2    name=Robert                score=8.5
  id=3    name=Charlie               score=8.8
  id=4    name=Diana                 score=6.1
  id=5    name=Eve                   score=9.9
```

Analisis:

- El acceso aleatorio funciona: podemos leer el registro 2, luego el 0, sin
  recorrer todo el archivo. Esto es posible porque los registros tienen
  tamano fijo.
- La actualizacion del indice 1 (Bob -> Robert) solo toca esos bytes; los
  demas registros no se mueven ni se corrompen.
- El `sizeof` puede variar (64, 72, etc.) debido al padding de la struct.
  Lo importante es que es consistente para lectura y escritura.

### Paso 3.4 — Verificar el tamano del archivo

```bash
ls -l records.dat
```

El tamano debe ser exactamente `5 * sizeof(struct Record)`. Si la salida del
programa mostro `sizeof = 72`, el archivo tendra 360 bytes.

### Paso 3.5 — Examinar el contenido binario

```bash
hexdump -C records.dat | head -20
```

Observa:

- Los nombres (ASCII) son legibles en la columna derecha
- Los numeros `int` e `id` aparecen como bytes en little-endian
- Los bytes `00` entre campos son padding de la struct

### Limpieza de la parte 3

```bash
rm -f records records.dat
```

---

## Parte 4 — Sparse files

**Objetivo**: Crear un archivo con huecos (holes) usando `lseek` mas alla del
final, y verificar que el tamano logico es mucho mayor que el espacio real en
disco.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat sparse.c
```

Observa la estrategia:

1. Escribir "START" en el byte 0
2. Saltar a 50 MB y escribir "MIDDLE"
3. Saltar a 100 MB - 3 y escribir "END"

Los bytes entre estas posiciones nunca se escriben explicitamente. El kernel
los trata como huecos (holes): se leen como ceros pero no ocupan bloques en
disco.

### Paso 4.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o sparse sparse.c
```

Antes de ejecutar, predice:

- El archivo "medira" ~100 MB segun `ls -l`. Cuanto espacio real ocupara en
  disco segun `du`?
- Si leemos bytes del hueco (posicion 1000, por ejemplo), que valores
  obtendremos?

### Paso 4.3 — Verificar las predicciones

```bash
./sparse
```

Salida esperada:

```
Wrote "START" at offset 0
Wrote "MIDDLE" at offset 52428800
Wrote "END" at offset 104857597

Read at offset 0:        "START"
Read at offset 52428800: "MIDDLE"
Read at offset 104857597: "END"

Read 8 bytes from hole (offset 1000):
  0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  All zeros: yes

Logical size: 104857600 bytes (~100 MB)

Usa estos comandos para comparar tamano logico vs real:
  ls -l sparse.dat
  du -h sparse.dat
  stat sparse.dat
```

### Paso 4.4 — Comparar tamano logico vs real

```bash
ls -lh sparse.dat
```

Salida esperada:

```
-rw-r--r-- 1 ... 100M ... sparse.dat
```

El sistema reporta ~100 MB. Ahora veamos cuanto ocupa realmente en disco:

```bash
du -h sparse.dat
```

Salida esperada:

```
~8.0K sparse.dat
```

Apenas unos kilobytes. La diferencia es enorme: el archivo "mide" 100 MB pero
solo los bloques con datos reales ("START", "MIDDLE", "END") ocupan espacio.

### Paso 4.5 — Detalles con stat

```bash
stat sparse.dat
```

Salida esperada (valores aproximados):

```
  File: sparse.dat
  Size: 104857600       Blocks: ~16        IO Block: 4096   regular file
  ...
```

Observa:

- `Size` es el tamano logico (100 MB)
- `Blocks` es la cantidad de bloques de 512 bytes que realmente se asignaron.
  Con ~16 bloques de 512 = 8192 bytes (~8 KB)
- La relacion `Size / (Blocks * 512)` revela que el archivo es sparse:
  100 MB logicos en 8 KB reales

### Paso 4.6 — Verificar que el hueco contiene ceros

```bash
hexdump -C -s 100 -n 32 sparse.dat
```

Salida esperada:

```
00000064  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00000084
```

Los bytes del hueco se leen como `0x00`. El asterisco indica que las lineas
intermedias son identicas (todo ceros). Estos ceros no estan almacenados en
disco; el kernel los genera al vuelo.

### Paso 4.7 — Verificar los datos reales

```bash
hexdump -C -n 16 sparse.dat
```

Salida esperada:

```
00000000  53 54 41 52 54 00 00 00  00 00 00 00 00 00 00 00  |START...........|
00000010
```

Los primeros 5 bytes son "START", seguidos de ceros del hueco.

```bash
hexdump -C -s 52428800 -n 16 sparse.dat
```

Salida esperada:

```
03200000  4d 49 44 44 4c 45 00 00  00 00 00 00 00 00 00 00  |MIDDLE..........|
03200010
```

"MIDDLE" en el offset 50 MB, seguido de ceros.

### Limpieza de la parte 4

```bash
rm -f sparse sparse.dat
```

---

## Limpieza final

```bash
rm -f seek_basics file_size records sparse
rm -f seek_test.dat size_test.dat records.dat sparse.dat
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  file_size.c  records.c  seek_basics.c  sparse.c
```

---

## Conceptos reforzados

1. `lseek` con `SEEK_SET` posiciona en una direccion absoluta desde el inicio.
   `SEEK_CUR` se mueve relativo a la posicion actual (acepta offsets
   negativos para retroceder). `SEEK_END` se mueve relativo al final.

2. `lseek(fd, 0, SEEK_CUR)` retorna la posicion actual sin moverla -- es el
   equivalente POSIX de `ftell`.

3. Para obtener el tamano de un archivo con `lseek`, hay que guardar la
   posicion actual, ir al final con `SEEK_END`, y restaurar la posicion.
   Si no se restaura, las siguientes operaciones de I/O ocurriran en el
   lugar equivocado.

4. Con registros de tamano fijo, `lseek` permite acceso aleatorio: el offset
   del registro `i` es `i * sizeof(struct Record)`. Esto evita recorrer todo
   el archivo para leer o actualizar un registro especifico.

5. Escribir mas alla del final del archivo con `lseek` crea un sparse file.
   Los huecos se leen como ceros pero no ocupan bloques en disco. `ls -l`
   muestra el tamano logico; `du` muestra el espacio real consumido.

6. `stat` revela ambos tamanos: `Size` (logico) y `Blocks` (bloques de 512
   bytes realmente asignados). Un archivo con `Size` mucho mayor que
   `Blocks * 512` es sparse.
