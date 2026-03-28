# Lab — open, close, read, write

## Objetivo

Usar las cuatro syscalls fundamentales de POSIX I/O para abrir, escribir, leer
y cerrar archivos. Al finalizar, sabras manejar file descriptors, interpretar
errores con errno, implementar loops para short reads/writes, y observar los
fds de tu proceso en `/proc/self/fd`.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `open_close.c` | Abrir/cerrar archivos, flags O_CREAT, O_EXCL, errno |
| `write_demo.c` | Escribir a stdout y a archivo, write_all con loop |
| `read_demo.c` | Leer archivo con buffer pequeno, loop hasta EOF |
| `fd_inspect.c` | Inspeccionar /proc/self/fd, reutilizacion de fds |

---

## Parte 1 — open y close

**Objetivo**: Abrir archivos con distintas combinaciones de flags, manejar
errores con errno, y cerrar file descriptors correctamente.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat open_close.c
```

Observa:

- Se intenta abrir un archivo inexistente con `O_RDONLY`
- Se crea un archivo con `O_WRONLY | O_CREAT | O_TRUNC` y permisos `0644`
- Se intenta crear el mismo archivo con `O_EXCL` (que falla si ya existe)
- Se abre para lectura el archivo creado

### Paso 1.2 — Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic open_close.c -o open_close
```

No debe producir warnings ni errores.

### Paso 1.3 — Predecir el resultado

Antes de ejecutar, responde mentalmente:

- Cuando `open()` falla, que valor retorna?
- El primer archivo que abras exitosamente, que numero de fd tendra? (recuerda:
  0, 1, 2 ya estan ocupados por stdin, stdout, stderr)
- Que errno producira `O_EXCL` sobre un archivo que ya existe?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.4 — Ejecutar y verificar

```bash
./open_close
```

Salida esperada:

```
open failed: errno=2 (No such file or directory)
Opened output.txt: fd=3
Closed fd=3
O_EXCL: file already exists (errno=17: File exists)
Opened output.txt for reading: fd=3
```

Observa:

- `open()` retorna -1 cuando falla y `errno` se establece a `ENOENT` (2)
- El primer fd disponible es 3 (despues de 0, 1, 2)
- `O_EXCL` con `O_CREAT` produce `EEXIST` (17) si el archivo ya existe
- Despues de cerrar fd=3, el siguiente `open()` reutiliza fd=3

### Paso 1.5 — Verificar los permisos del archivo creado

```bash
ls -l output.txt
```

Salida esperada:

```
-rw-r--r-- 1 <user> <group> 0 <date> output.txt
```

Los permisos `0644` (`rw-r--r--`) se aplicaron al crear el archivo. El archivo
tiene 0 bytes porque se creo con `O_TRUNC` y no se escribio nada.

Ahora verifica como el umask afecta los permisos:

```bash
umask
```

Si tu umask es `0022`, los permisos reales son `0644 & ~0022 = 0644` (sin
cambio en este caso). Pero si pidieras `0666`, obtendrias `0666 & ~0022 = 0644`.

### Limpieza de la parte 1

```bash
rm -f open_close output.txt
```

---

## Parte 2 — write

**Objetivo**: Escribir datos a stdout y a archivos con `write()`, verificar el
valor de retorno, e implementar `write_all` para manejar short writes.

### Paso 2.1 — Examinar el codigo fuente

```bash
cat write_demo.c
```

Observa:

- `write_all()` usa un loop que maneja short writes y `EINTR`
- Se escribe directamente a `STDOUT_FILENO` (fd 1) sin usar `printf`
- Se escriben 3 lineas a un archivo usando `write_all()`

### Paso 2.2 — Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic write_demo.c -o write_demo
```

### Paso 2.3 — Predecir el resultado

Antes de ejecutar, responde mentalmente:

- `write(STDOUT_FILENO, msg, strlen(msg))` retornara el numero de bytes
  escritos. Cuantos bytes tiene "Hello from write()!\n"?
- Que fd tendra el archivo `written.txt`?
- Si `write()` retorna menos bytes de los pedidos (short write), que hace
  `write_all()` para completar la escritura?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4 — Ejecutar y verificar

```bash
./write_demo
```

Salida esperada:

```
Hello from write()!
write() returned: 20 (expected 20)
Wrote 19 bytes to fd=3
Wrote 30 bytes to fd=3
Wrote 25 bytes to fd=3
Data written to written.txt
```

Observa:

- `write()` retorno 20 bytes (la longitud de "Hello from write()!\n")
- El archivo se abrio como fd=3
- Cada linea se escribio con `write_all()` que garantiza escritura completa

### Paso 2.5 — Verificar el contenido del archivo

```bash
cat written.txt
```

Salida esperada:

```
Line 1: POSIX I/O
Line 2: open/close/read/write
Line 3: file descriptors
```

```bash
wc -c written.txt
```

Salida esperada:

```
74 written.txt
```

Los 74 bytes coinciden con la suma: 19 + 30 + 25 = 74.

### Paso 2.6 — write a stdout vs printf

La diferencia clave: `write()` es una syscall directa, `printf()` pasa por un
buffer en espacio de usuario (stdio). En este programa se mezclan ambas. En
programas reales, mezclar stdio y POSIX I/O en el mismo fd puede causar salida
desordenada si los buffers no se sincronizan.

### Limpieza de la parte 2

```bash
rm -f write_demo written.txt
```

---

## Parte 3 — read

**Objetivo**: Leer datos de un archivo con `read()`, observar partial reads con
un buffer pequeno, e implementar el loop de lectura hasta EOF.

### Paso 3.1 — Preparar un archivo de prueba

Crea un archivo con contenido conocido:

```bash
cat > testdata.txt << 'EOF'
ABCDEFGHIJKLMNOPQRSTUVWXYZ
abcdefghijklmnopqrstuvwxyz
0123456789 POSIX read test
EOF
```

```bash
wc -c testdata.txt
```

Salida esperada:

```
81 testdata.txt
```

### Paso 3.2 — Examinar el codigo fuente

```bash
cat read_demo.c
```

Observa:

- El buffer es de 32 bytes (intencionalmente pequeno)
- El programa lee en un loop hasta que `read()` retorna 0 (EOF)
- Cada iteracion muestra cuantos bytes leyo `read()`
- Los datos leidos se escriben a stdout con `write()`

### Paso 3.3 — Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic read_demo.c -o read_demo
```

### Paso 3.4 — Predecir el resultado

El archivo tiene 81 bytes y el buffer tiene 32 bytes.

Antes de ejecutar, responde mentalmente:

- Cuantas iteraciones del loop se necesitaran para leer los 81 bytes?
- `read()` siempre retornara 32 bytes, o la ultima iteracion sera diferente?
- Que retorna `read()` cuando llega al final del archivo?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.5 — Ejecutar y verificar

```bash
./read_demo testdata.txt
```

Salida esperada:

```
Opened testdata.txt: fd=3
Reading in loop until EOF...

[iter 1] read() returned 32 bytes
ABCDEFGHIJKLMNOPQRSTUVWXYZ
abcde
--- end of chunk ---
[iter 2] read() returned 32 bytes
fghijklmnopqrstuvwxyz
01234567
--- end of chunk ---
[iter 3] read() returned 17 bytes
89 POSIX read test

--- end of chunk ---

EOF reached (read returned 0)
Total: 81 bytes in 3 iterations
```

Observa:

- Las dos primeras iteraciones leyeron 32 bytes (buffer lleno)
- La tercera leyo 17 bytes (lo que quedaba) -- esto es un **partial read**
- Despues del partial read, `read()` retorno 0 indicando EOF
- Los datos NO estan terminados en `'\0'` -- `read()` no agrega terminador

### Paso 3.6 — read de stdin

`read()` tambien funciona con stdin. Prueba:

```bash
echo "test input" | ./read_demo /dev/stdin
```

Salida esperada:

```
Opened /dev/stdin: fd=3
Reading in loop until EOF...

[iter 1] read() returned 11 bytes
test input

--- end of chunk ---

EOF reached (read returned 0)
Total: 11 bytes in 1 iterations
```

`/dev/stdin` es un enlace simbolico al fd 0 del proceso. Abrir `/dev/stdin`
crea un nuevo fd (3) que apunta al mismo recurso.

### Limpieza de la parte 3

```bash
rm -f read_demo testdata.txt
```

---

## Parte 4 — File descriptors y /proc/self/fd

**Objetivo**: Observar los file descriptors del proceso en `/proc/self/fd`,
ver como se asignan y reutilizan los numeros de fd.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat fd_inspect.c
```

Observa:

- Se imprimen los valores de `STDIN_FILENO`, `STDOUT_FILENO`, `STDERR_FILENO`
- Se ejecuta `ls -l /proc/self/fd` en tres momentos: antes de abrir, despues
  de abrir, y despues de cerrar + reabrir
- Se cierra un fd y se abre otro para ver la reutilizacion

### Paso 4.2 — Compilar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic fd_inspect.c -o fd_inspect
```

### Paso 4.3 — Predecir el resultado

Antes de ejecutar, responde mentalmente:

- Los valores de STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO son constantes
  definidas en `<unistd.h>`. Que valores tienen?
- Si abres dos archivos despues de los 3 fds estandar, que numeros tendran?
- Si cierras fd=3 y luego abres otro archivo, que fd recibira?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.4 — Ejecutar y verificar

```bash
./fd_inspect
```

Salida esperada (los paths pueden variar):

```
Standard file descriptors:
  STDIN_FILENO  = 0
  STDOUT_FILENO = 1
  STDERR_FILENO = 2

=== /proc/self/fd (before opening files) ===
total 0
lrwx------ 1 <user> <group> 64 <date> 0 -> /dev/pts/<n>
lrwx------ 1 <user> <group> 64 <date> 1 -> /dev/pts/<n>
lrwx------ 1 <user> <group> 64 <date> 2 -> /dev/pts/<n>
...

Opened fd_test_1.txt: fd=3
Opened fd_test_2.txt: fd=4

=== /proc/self/fd (after opening 2 files) ===
total 0
lrwx------ 1 <user> <group> 64 <date> 0 -> /dev/pts/<n>
lrwx------ 1 <user> <group> 64 <date> 1 -> /dev/pts/<n>
lrwx------ 1 <user> <group> 64 <date> 2 -> /dev/pts/<n>
l-wx------ 1 <user> <group> 64 <date> 3 -> .../fd_test_1.txt
l-wx------ 1 <user> <group> 64 <date> 4 -> .../fd_test_2.txt
...

Closed fd=3
Opened fd_test_3.txt: fd=3
(notice: fd 3 was reused after closing it)

=== /proc/self/fd (after close + reopen) ===
total 0
lrwx------ 1 <user> <group> 64 <date> 0 -> /dev/pts/<n>
lrwx------ 1 <user> <group> 64 <date> 1 -> /dev/pts/<n>
lrwx------ 1 <user> <group> 64 <date> 2 -> /dev/pts/<n>
l-wx------ 1 <user> <group> 64 <date> 3 -> .../fd_test_3.txt
l-wx------ 1 <user> <group> 64 <date> 4 -> .../fd_test_2.txt
...
```

Observa:

- `/proc/self/fd` muestra los fds como symlinks al recurso real
- Los fds 0, 1, 2 apuntan al terminal (`/dev/pts/<n>`)
- Los fds nuevos se asignan en orden: 3, 4, 5...
- Al cerrar fd=3 y abrir otro, el kernel reutiliza el numero 3 (siempre asigna
  el fd libre mas bajo)
- Los permisos del symlink (`l-wx------`) reflejan el modo de apertura:
  `O_WRONLY` = solo escritura

### Paso 4.5 — Limite de file descriptors

Verifica cuantos fds puede abrir tu proceso:

```bash
ulimit -n
```

Salida esperada:

```
1024
```

Este es el limite blando (soft limit). Cada `open()` sin su correspondiente
`close()` consume un fd. Si llegas al limite, `open()` fallara con
`errno = EMFILE`. En servidores de larga duracion, no cerrar fds causa **file
descriptor leaks**.

### Limpieza de la parte 4

```bash
rm -f fd_inspect fd_test_1.txt fd_test_2.txt fd_test_3.txt
```

---

## Limpieza final

```bash
rm -f open_close write_demo read_demo fd_inspect
rm -f output.txt written.txt testdata.txt
rm -f fd_test_1.txt fd_test_2.txt fd_test_3.txt
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  fd_inspect.c  open_close.c  read_demo.c  write_demo.c
```

---

## Conceptos reforzados

1. `open()` retorna un file descriptor (entero no negativo) si tiene exito, o
   -1 si falla. Siempre verificar el retorno antes de usar el fd.

2. Los flags de `open()` se combinan con `|`: `O_WRONLY | O_CREAT | O_TRUNC`
   crea el archivo, lo trunca, y lo abre para escritura. El parametro `mode`
   es obligatorio cuando se usa `O_CREAT`.

3. `errno` se establece en el ultimo error. Valores comunes: `ENOENT` (no
   existe), `EEXIST` (ya existe con `O_EXCL`), `EACCES` (sin permisos).

4. `write()` puede escribir menos bytes de los pedidos (short write). El
   patron `write_all` con loop garantiza que se escriban todos los bytes,
   manejando tambien la interrupcion por senales (`EINTR`).

5. `read()` retorna el numero de bytes leidos (puede ser menos que el buffer),
   0 cuando llega a EOF, o -1 en error. El loop de lectura hasta retorno 0 es
   el patron estandar para leer archivos completos.

6. Los file descriptors se asignan en orden ascendente (3, 4, 5...) despues
   de los 3 estandar (0=stdin, 1=stdout, 2=stderr). Al cerrar un fd, el kernel
   reutiliza ese numero para el siguiente `open()`.

7. `/proc/self/fd` muestra los file descriptors del proceso actual como
   symlinks. Es la herramienta de inspeccion para diagnosticar fd leaks.

8. `close()` libera el file descriptor. No cerrar fds causa leaks que
   eventualmente agotan el limite del proceso (`ulimit -n`, tipicamente 1024).
