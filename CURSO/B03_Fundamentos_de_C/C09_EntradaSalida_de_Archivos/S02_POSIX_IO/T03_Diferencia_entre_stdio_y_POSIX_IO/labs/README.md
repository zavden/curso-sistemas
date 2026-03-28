# Lab -- Diferencia entre stdio y POSIX I/O

## Objetivo

Comparar las dos APIs de I/O disponibles en Linux (stdio y POSIX) realizando
las mismas operaciones con ambas, convertir entre `FILE*` y fd, medir el
impacto del buffering en el rendimiento, y establecer criterios claros para
elegir una u otra en cada situacion.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Herramientas: `strace`, `diff`, `md5sum` (incluidas en la mayoria de distribuciones)
- Terminal con acceso al directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `copy_stdio.c` | Copia de archivo usando fopen/fread/fwrite/fclose |
| `copy_posix.c` | Copia de archivo usando open/read/write/close |
| `bridge.c` | Conversion entre FILE* y fd con fileno() y fdopen() |
| `mix_danger.c` | Demuestra el peligro de mezclar stdio y POSIX I/O |
| `bench_write.c` | Benchmark de escritura: 4 estrategias con distintos buffers |

---

## Parte 1 -- Misma operacion con ambas APIs

**Objetivo**: Copiar un archivo usando stdio y POSIX I/O, comparar el codigo
y verificar que ambos producen el mismo resultado.

### Paso 1.1 -- Examinar los archivos fuente

```bash
cat copy_stdio.c
```

Observa la estructura: `fopen`, `fread`, `fwrite`, `fclose`. El tipo de handle
es `FILE*` y el buffer es `char buf[4096]`. Los errores se detectan con el
valor de retorno de `fread`/`fwrite` y `ferror`.

```bash
cat copy_posix.c
```

Observa las diferencias: `open`, `read`, `write`, `close`. El handle es un
`int` (fd). `open` recibe flags (`O_WRONLY | O_CREAT | O_TRUNC`) y permisos
(`0644`). El loop de escritura maneja escrituras parciales (cuando `write`
escribe menos bytes de los pedidos).

Antes de compilar, responde mentalmente:

- En `copy_stdio.c`, quien decide cuando se hace una syscall `write` real?
- En `copy_posix.c`, cada llamada a `write()` es una syscall real?
- Cual de los dos maneja el caso de escritura parcial?

### Paso 1.2 -- Compilar ambos programas

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o copy_stdio copy_stdio.c
gcc -std=c11 -Wall -Wextra -Wpedantic -o copy_posix copy_posix.c
```

No debe haber warnings ni errores.

### Paso 1.3 -- Crear un archivo de prueba

```bash
dd if=/dev/urandom of=testfile.bin bs=1K count=64 2>/dev/null
ls -l testfile.bin
```

Salida esperada:

```
-rw-r--r-- 1 user user 65536 ... testfile.bin
```

Un archivo binario de 64 KB con contenido aleatorio.

### Paso 1.4 -- Copiar con stdio y con POSIX

```bash
./copy_stdio testfile.bin copy_stdio.out
./copy_posix testfile.bin copy_posix.out
```

Salida esperada:

```
stdio copy complete: testfile.bin -> copy_stdio.out
POSIX copy complete: testfile.bin -> copy_posix.out
```

### Paso 1.5 -- Verificar que las copias son identicas

```bash
md5sum testfile.bin copy_stdio.out copy_posix.out
```

Los tres hashes deben ser identicos. Ambas APIs producen el mismo resultado
final -- la diferencia esta en como lo hacen internamente.

### Paso 1.6 -- Contar syscalls con strace

Ahora la parte reveladora. Antes de ejecutar, predice:

- `copy_stdio` usa `fread` con buffer de 4096 bytes. stdio tiene un buffer
  interno (tipicamente 8 KB). Cuantas syscalls `read` esperas para 64 KB?
- `copy_posix` usa `read` con buffer de 4096 bytes. Cuantas syscalls `read`
  esperas?

```bash
strace -c ./copy_stdio testfile.bin /dev/null 2>&1 | tail -20
```

```bash
strace -c ./copy_posix testfile.bin /dev/null 2>&1 | tail -20
```

Compara la columna `calls` de las syscalls `read` y `write` en ambos. Con un
archivo de 64 KB y buffer de 4096:

- `copy_posix` deberia hacer ~16 llamadas a `read` (64 KB / 4 KB = 16)
- `copy_stdio` puede hacer ~8 llamadas a `read` porque el buffer interno de
  stdio es de 8 KB (stdio pide 8 KB al kernel y entrega 4 KB a `fread` dos
  veces por cada syscall)

La diferencia clave: stdio agrega una capa de buffering entre tu codigo y el
kernel. `fread` no siempre causa una syscall.

### Limpieza de la parte 1

```bash
rm -f copy_stdio copy_posix testfile.bin copy_stdio.out copy_posix.out
```

---

## Parte 2 -- fileno y fdopen: convertir entre FILE* y fd

**Objetivo**: Usar `fileno()` para obtener el fd de un `FILE*`, y `fdopen()`
para crear un `FILE*` desde un fd. Entender las reglas de cierre.

### Paso 2.1 -- Examinar el codigo

```bash
cat bridge.c
```

Observa dos secciones:

- **Parte A**: abre con `fopen`, obtiene el fd con `fileno()`, escribe con
  ambas APIs (con `fflush` entre medio)
- **Parte B**: abre con `open`, crea un `FILE*` con `fdopen()`, lee con `fgets`

Antes de compilar, predice:

- Despues de `fclose(f)`, el fd obtenido con `fileno()` sigue abierto?
- Despues de `fclose(f2)` (creado con `fdopen`), el fd original sigue abierto?

### Paso 2.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o bridge bridge.c
./bridge
```

Salida esperada:

```
=== fileno: FILE* -> fd ===
FILE* opened, underlying fd = 3
Wrote 2 lines to bridge_test.txt

=== fdopen: fd -> FILE* ===
Opened fd = 3 with open()
Created FILE* from fd with fdopen()
Contents of bridge_test.txt:
  Line 1: written with fprintf
  Line 2: written with write()

fclose(f2) also closed fd 3
```

Observa:

- El fd es 3 (despues de stdin=0, stdout=1, stderr=2)
- Las dos lineas aparecen en el orden correcto porque se uso `fflush` antes
  de `write`
- `fclose` cierra tanto el `FILE*` como el fd subyacente -- en ambas
  direcciones

### Paso 2.3 -- Verificar el contenido del archivo

```bash
cat bridge_test.txt
```

Salida esperada:

```
Line 1: written with fprintf
Line 2: written with write()
```

Las dos lineas estan en orden. Sin el `fflush`, la linea de `write` apareceria
primero (porque `fprintf` solo escribe al buffer de stdio, no al archivo).

### Paso 2.4 -- Observar el peligro de mezclar APIs

```bash
cat mix_danger.c
```

Este programa escribe AAA, BBB, CCC con `fprintf`, `write`, `fprintf`. Primero
sin `fflush` (orden incorrecto), luego con `fflush` (orden correcto).

Predice antes de ejecutar: en la version sin `fflush`, en que orden apareceran
las tres lineas en el archivo?

### Paso 2.5 -- Ejecutar mix_danger

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o mix_danger mix_danger.c
./mix_danger
```

Salida esperada:

```
=== Without fflush (broken order) ===
  BBB: written with write() (second)
  AAA: written with fprintf (first)
  CCC: written with fprintf (third)

=== With fflush (correct order) ===
  AAA: written with fprintf (first)
  BBB: written with write() (second)
  CCC: written with fprintf (third)
```

Sin `fflush`: BBB aparece primero porque `write` va directo al kernel,
mientras AAA y CCC estaban en el buffer de stdio esperando el `fclose`. Con
`fflush`: el orden es el esperado porque se fuerza el vaciado del buffer
antes de usar `write`.

Regla: **nunca mezclar stdio y POSIX I/O en el mismo fd**. Si es inevitable,
usar `fflush` antes de cambiar de API.

### Limpieza de la parte 2

```bash
rm -f bridge mix_danger bridge_test.txt mix_broken.txt mix_fixed.txt
```

---

## Parte 3 -- Benchmark de rendimiento

**Objetivo**: Medir el tiempo de escritura con 4 estrategias y entender el
impacto del buffering y el tamano de buffer en el rendimiento.

### Paso 3.1 -- Examinar el codigo

```bash
cat bench_write.c
```

Las 4 estrategias son:

1. `write()` con buffer de 1 byte -- una syscall por byte
2. `write()` con buffer de 4096 bytes -- una syscall cada 4 KB
3. `fputc()` un byte a la vez -- stdio agrega buffering transparente
4. `fwrite()` con buffer de 4096 bytes -- stdio + writes grandes

Antes de compilar, predice:

- Cual sera la estrategia mas lenta? Por cuanto margen?
- `fputc()` escribe 1 byte a la vez como `write(fd, &c, 1)`. Sera igual de
  lenta? Por que si o por que no?
- Entre las estrategias 2 y 4, cual sera mas rapida?

### Paso 3.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o bench_write bench_write.c
./bench_write
```

Salida esperada (los tiempos varian segun el hardware):

```
Writing 4194304 bytes (4.0 MB) per strategy...

Strategy                            Time (ms) Syscalls (approx)
-----------------------------------  ---------- ------------
1. write() 1 byte at a time            ~800      4194304
2. write() 4096-byte buffer              ~3         1024
3. fputc() 1 byte at a time             ~20         ~512
4. fwrite() 4096-byte buffer              ~2        ~1024
```

### Paso 3.3 -- Analizar los resultados

Observa los numeros y responde:

- **Estrategia 1 vs 2**: write(1 byte) es ~200-400x mas lenta. Cada byte causa
  una syscall (cambio de contexto user -> kernel -> user). Con buffer de 4 KB,
  se hacen 1024 syscalls en vez de 4 millones.

- **Estrategia 1 vs 3**: `fputc` escribe 1 byte a la vez desde la perspectiva
  de tu codigo, pero stdio acumula bytes en un buffer interno (~8 KB) y hace
  una sola syscall `write` cuando el buffer se llena. Resultado: `fputc` hace
  ~512 syscalls, no 4 millones.

- **Estrategia 2 vs 4**: Rendimiento similar. `fwrite` con buffer grande
  agrega poco overhead sobre `write` directo con el mismo tamano de buffer.

La leccion: **el buffering de stdio existe para protegerte de hacer millones
de syscalls accidentalmente**. Con POSIX I/O, el programador es responsable
de elegir un tamano de buffer adecuado.

### Paso 3.4 -- Verificar con strace

```bash
strace -c ./bench_write 2>&1 | grep -E "write|calls"
```

Confirma el numero real de syscalls `write`. El total debe estar cerca de
los valores de la tabla (la estrategia 1 domina con millones de llamadas).

### Limpieza de la parte 3

```bash
rm -f bench_write bench_tmp.dat
```

---

## Parte 4 -- Cuando usar cada API

**Objetivo**: Consolidar los criterios de decision entre stdio y POSIX I/O con
una tabla practica y un ejercicio de clasificacion.

### Paso 4.1 -- Tabla de decision

Lee la siguiente tabla y contrasta con lo que observaste en las partes
anteriores:

| Necesitas... | Usa | Por que |
|---|---|---|
| Leer/escribir texto con formato | stdio | `fprintf`, `fscanf` simplifican el formateo |
| Interaccion con usuario (terminal) | stdio | `fgets`, `printf` manejan buffering de linea |
| Archivos de configuracion | stdio | Lectura linea a linea con `fgets` + `sscanf` |
| Copiar archivos binarios | Ambas | stdio con `fread`/`fwrite`, POSIX con `read`/`write` |
| Codigo que debe compilar en Windows | stdio | C estandar, disponible en todos los OS |
| Sockets de red | POSIX | Sockets son fds, se usan con `read`/`write` |
| Non-blocking I/O | POSIX | `O_NONBLOCK` solo existe con `open` |
| Multiplexar fds con poll/epoll | POSIX | `poll`/`epoll` trabajan con fds, no con `FILE*` |
| Memory-mapped I/O (mmap) | POSIX | `mmap` requiere un fd |
| Escribir en signal handlers | POSIX | `write` es async-signal-safe, `fprintf` no |
| Permisos especificos al crear archivo | POSIX | `open` acepta modo (0600, 0755, etc.) |
| Control fino de buffering | Depende | `setvbuf` para stdio; buffer manual para POSIX |

### Paso 4.2 -- Ejercicio de clasificacion

Para cada escenario, decide si usarias stdio o POSIX I/O. Escribe tu respuesta
antes de leer la solucion en el paso siguiente.

1. Un programa que lee un CSV y genera un reporte con columnas alineadas
2. Un servidor TCP que maneja 500 conexiones con epoll
3. Un daemon que debe escribir un log inmediatamente al disco (sin buffering)
4. Un programa que copia archivos grandes (>1 GB) lo mas rapido posible
5. Un signal handler que reporta errores a stderr
6. Un programa que parsea archivos JSON de configuracion

### Paso 4.3 -- Soluciones y justificacion

1. **stdio** -- `fprintf` con especificadores de ancho (`%-20s %10d`) facilita
   el formateo de columnas. Hacerlo con `write` requiere construir strings
   manualmente con `snprintf`.

2. **POSIX** -- `epoll` trabaja con fds. No se puede pasar un `FILE*` a
   `epoll_ctl`. Los sockets se crean con `socket()` que retorna un fd.

3. **POSIX** -- `open` con `O_SYNC` garantiza que cada `write` llega al disco.
   Con stdio, tendrias que llamar `fflush` + `fsync(fileno(f))` despues de cada
   escritura, anulando el proposito del buffering.

4. **POSIX** -- Con buffer grande (64 KB+) y `read`/`write` directos se
   minimiza overhead. Para archivos muy grandes, `mmap` o `sendfile` (ambos
   requieren fd) pueden ser mas eficientes.

5. **POSIX** -- `write(STDERR_FILENO, msg, len)` es async-signal-safe.
   `fprintf(stderr, ...)` no lo es -- puede causar deadlock si la senal
   interrumpe otra operacion de stdio que tiene el lock del buffer.

6. **stdio** -- Lectura secuencial de texto con `fgets` o `fread`, parsing
   con funciones de string. Portabilidad garantizada.

---

## Limpieza final

```bash
rm -f copy_stdio copy_posix bridge mix_danger bench_write
rm -f testfile.bin copy_stdio.out copy_posix.out
rm -f bridge_test.txt mix_broken.txt mix_fixed.txt
rm -f bench_tmp.dat
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  bench_write.c  bridge.c  copy_posix.c  copy_stdio.c  mix_danger.c
```

---

## Conceptos reforzados

1. stdio (`FILE*`) esta construida sobre POSIX I/O (`fd`) -- cada `fread` o
   `fwrite` eventualmente llama a `read` o `write`, pero agrega un buffer
   intermedio que reduce el numero de syscalls.

2. Copiar un archivo con stdio y con POSIX I/O produce el mismo resultado.
   La diferencia esta en quien gestiona el buffering: stdio lo hace
   automaticamente, POSIX I/O delega la responsabilidad al programador.

3. `fileno()` obtiene el fd de un `FILE*`. `fdopen()` crea un `FILE*` desde
   un fd. En ambos casos, `fclose` cierra tanto el `FILE*` como el fd
   subyacente -- no llamar `close(fd)` despues de `fclose`.

4. Mezclar stdio y POSIX I/O en el mismo fd causa datos desordenados porque
   `write` va directo al kernel mientras `fprintf` escribe al buffer de stdio.
   La solucion es `fflush` antes de cambiar de API, pero la mejor practica es
   no mezclar.

5. `write()` con buffer de 1 byte es cientos de veces mas lenta que con buffer
   de 4 KB, porque cada byte causa una syscall. `fputc()` escribe 1 byte a la
   vez para el programador pero acumula internamente ~8 KB antes de hacer la
   syscall, resultando mucho mas rapida que `write()` byte a byte.

6. Usar stdio para texto formateado, interaccion con usuario, y codigo
   portátil. Usar POSIX I/O para sockets, non-blocking I/O, poll/epoll,
   mmap, signal handlers, y cuando se necesitan flags especiales de `open`.
