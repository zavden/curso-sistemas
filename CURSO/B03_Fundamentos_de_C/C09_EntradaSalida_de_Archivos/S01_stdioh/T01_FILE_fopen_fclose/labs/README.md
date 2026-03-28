# Lab -- FILE*, fopen, fclose

## Objetivo

Practicar la apertura y cierre de archivos con `fopen`/`fclose`, entender los
modos de apertura y sus efectos, trabajar con los streams estandar
(`stdin`/`stdout`/`stderr`), y experimentar las consecuencias de no cerrar
archivos. Al finalizar, sabras usar el patron seguro de manejo de archivos y
diagnosticar errores comunes.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `open_close.c` | Abrir, escribir, cerrar y releer un archivo |
| `std_streams.c` | Demostrar stdout y stderr como streams separados |
| `open_modes.c` | Comparar los modos w, a, r+ y su efecto en el contenido |
| `fd_limit.c` | Mostrar el error EMFILE al olvidar cerrar archivos |

---

## Parte 1 -- fopen y fclose basico

**Objetivo**: Abrir un archivo, verificar que fopen tuvo exito, escribir
contenido, cerrarlo, y releerlo.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat open_close.c
```

Observa la estructura del programa:

- `fopen` con modo `"w"` para crear/escribir
- Verificacion de `NULL` antes de usar el `FILE*`
- `fprintf` para escribir en el archivo
- `fclose` con verificacion de retorno
- Reapertura con `"r"` para leer

### Paso 1.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o open_close open_close.c
./open_close
```

Salida esperada:

```
File 'testfile.txt' opened and written successfully
File 'testfile.txt' closed successfully
Read back: Hello from open_close
```

### Paso 1.3 -- Verificar el archivo creado

```bash
cat testfile.txt
```

Salida esperada:

```
Hello from open_close
```

El archivo persiste en disco despues de que el programa termino.

### Paso 1.4 -- Provocar un error de apertura

Intenta abrir un archivo que no existe en modo lectura:

```bash
cat > error_test.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    FILE *f = fopen("nonexistent.txt", "r");
    if (f == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }
    fclose(f);
    return EXIT_SUCCESS;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic -o error_test error_test.c
./error_test
```

Salida esperada:

```
fopen: No such file or directory
```

`perror` imprime automaticamente la descripcion del error almacenado en `errno`.
El modo `"r"` requiere que el archivo exista -- si no existe, `fopen` retorna
`NULL`.

### Paso 1.5 -- Verificar el codigo de retorno

```bash
echo $?
```

Salida esperada:

```
1
```

`EXIT_FAILURE` (1) indica que el programa termino con error. Esto es util para
scripts y pipelines que verifican el resultado.

### Limpieza de la parte 1

```bash
rm -f open_close testfile.txt error_test error_test.c
```

---

## Parte 2 -- Los 3 streams estandar

**Objetivo**: Entender que `stdout` y `stderr` son streams `FILE*` separados y
verificar que se pueden redirigir independientemente.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat std_streams.c
```

Observa que el programa escribe a `stdout` y a `stderr` de forma intercalada.

### Paso 2.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o std_streams std_streams.c
./std_streams
```

Veras todas las lineas mezcladas en la terminal. Tanto `stdout` como `stderr` se
muestran en la misma pantalla por defecto.

### Paso 2.3 -- Redirigir solo stdout

Antes de ejecutar, predice:

- Si rediriges stdout a un archivo con `>`, que lineas apareceran en la terminal?
- Y cuales quedaran en el archivo?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4 -- Verificar la prediccion

```bash
./std_streams > stdout_only.txt
```

En la terminal solo aparecen las lineas de `stderr`:

```
This goes to stderr
stderr: order test 2
```

Ahora verifica el archivo:

```bash
cat stdout_only.txt
```

Salida esperada:

```
This goes to stdout
printf is equivalent to fprintf(stdout, ...)
stdout: order test 1
stdout: order test 3
```

Las lineas de `fprintf(stdout, ...)` y `printf(...)` fueron al archivo. Las de
`fprintf(stderr, ...)` fueron a la terminal. Son streams completamente
independientes.

### Paso 2.5 -- Redirigir solo stderr

```bash
./std_streams 2> stderr_only.txt
```

Ahora `stdout` aparece en la terminal y `stderr` va al archivo:

```bash
cat stderr_only.txt
```

Salida esperada:

```
This goes to stderr
stderr: order test 2
```

### Paso 2.6 -- Redirigir ambos por separado

```bash
./std_streams > out.txt 2> err.txt
cat out.txt
cat err.txt
```

Cada stream fue a su propio archivo. Esta separacion es fundamental para
programas en produccion: los mensajes normales van a stdout, los errores a
stderr.

### Limpieza de la parte 2

```bash
rm -f std_streams stdout_only.txt stderr_only.txt out.txt err.txt
```

---

## Parte 3 -- Modos de apertura

**Objetivo**: Experimentar con los modos `"w"`, `"a"` y `"r+"`, y observar como
cada uno afecta el contenido del archivo.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat open_modes.c
```

El programa ejecuta 4 pasos en secuencia:

1. Abre con `"w"` y escribe una linea
2. Abre con `"a"` y agrega una linea
3. Abre con `"r+"` y escribe `"OVERWRITTEN"` desde la posicion 0
4. Abre con `"w"` de nuevo (trunca todo)

Antes de ejecutar, predice:

- Despues del paso 3, el archivo tendra 1 linea o 2? Que parte se sobrescribio?
- Despues del paso 4, cuantas lineas quedaran?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o open_modes open_modes.c
./open_modes
```

Salida esperada:

```
=== Step 1: fopen("modes_test.txt", "w") ===
[After w] Contents of 'modes_test.txt':
  line 1 (written with w)

=== Step 2: fopen("modes_test.txt", "a") ===
[After a] Contents of 'modes_test.txt':
  line 1 (written with w)
  line 2 (appended with a)

=== Step 3: fopen("modes_test.txt", "r+") ===
[After r+] Contents of 'modes_test.txt':
  OVERWRITTENtten with w)
  line 2 (appended with a)

=== Step 4: fopen("modes_test.txt", "w") again ===
[After second w] Contents of 'modes_test.txt':
  line 3 (only line after w truncated)
```

### Paso 3.3 -- Analisis del resultado

Observaciones clave:

- **Paso 1 ("w")**: Creo el archivo y escribio la primera linea.
- **Paso 2 ("a")**: Agrego al final sin tocar la linea existente. El modo
  append siempre posiciona al final.
- **Paso 3 ("r+")**: Sobrescribio desde la posicion 0. Escribio 11 bytes
  (`"OVERWRITTEN"`) sobre los primeros 11 bytes de la primera linea. El resto
  del archivo quedo intacto -- `"r+"` no trunca.
- **Paso 4 ("w")**: Trunco **todo** el contenido previo y escribio desde cero.
  Las 2 lineas anteriores desaparecieron.

Este es el peligro de `"w"`: borra todo el contenido existente. Si necesitas
agregar al final, usa `"a"`.

### Paso 3.4 -- Demostrar la diferencia w+ vs r+

```bash
cat > wplus_test.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char *path = "wplus_demo.txt";

    /* Create a file with content first */
    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen"); return EXIT_FAILURE; }
    fprintf(f, "original content\n");
    fclose(f);
    printf("Created file with: original content\n");

    /* Open with w+ -- this TRUNCATES, then allows read+write */
    f = fopen(path, "w+");
    if (!f) { perror("fopen"); return EXIT_FAILURE; }

    /* The file is now empty -- try to read */
    char buf[256];
    if (fgets(buf, sizeof(buf), f) == NULL) {
        printf("w+ read: (nothing -- file was truncated!)\n");
    }

    /* Write new content */
    fprintf(f, "new content after w+\n");

    /* Rewind and read back */
    rewind(f);
    if (fgets(buf, sizeof(buf), f) != NULL) {
        printf("w+ after rewind: %s", buf);
    }
    fclose(f);

    return EXIT_SUCCESS;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic -o wplus_test wplus_test.c
./wplus_test
```

Salida esperada:

```
Created file with: original content
w+ read: (nothing -- file was truncated!)
w+ after rewind: new content after w+
```

`"w+"` permite leer Y escribir, pero primero **trunca**. Es como `"w"` con la
capacidad adicional de leer. Si necesitas leer y escribir sin perder el
contenido existente, usa `"r+"`.

### Limpieza de la parte 3

```bash
rm -f open_modes modes_test.txt wplus_test wplus_test.c wplus_demo.txt
```

---

## Parte 4 -- Errores comunes: olvidar fclose y limite de file descriptors

**Objetivo**: Ver en la practica que pasa cuando un programa abre muchos archivos
sin cerrarlos.

### Paso 4.1 -- Verificar el limite del sistema

```bash
ulimit -n
```

Este es el numero maximo de file descriptors que un proceso puede tener abiertos
simultaneamente. Cada `fopen` consume un file descriptor.

### Paso 4.2 -- Examinar el codigo fuente

```bash
cat fd_limit.c
```

Observa que el programa:

1. Reduce el limite de file descriptors a 30 (con `setrlimit`) para que el
   efecto sea visible rapidamente
2. Abre archivos en un bucle sin cerrarlos
3. Cuando `fopen` falla, muestra el error (EMFILE)

Antes de ejecutar, predice: si el limite es 30 y ya estan abiertos
`stdin`/`stdout`/`stderr`, cuantos archivos nuevos se podran abrir?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o fd_limit fd_limit.c
./fd_limit
```

Salida esperada:

```
Original limit: soft=<valor>, hard=<valor>
Reduced soft limit to 30 for this demo

Opening files without closing them...

Failed at file #27: Too many open files (errno=24: EMFILE)
This is what happens when you forget to fclose()!

Opened 27 files before hitting the limit
Cleaning up...
Cleanup complete
```

27 archivos: el limite de 30 menos los 3 que ya estan abiertos
(stdin, stdout, stderr). El file descriptor #27 ya seria el #30 del proceso
(0=stdin, 1=stdout, 2=stderr, 3..29=archivos abiertos).

### Paso 4.4 -- Error EMFILE (errno 24)

El error `EMFILE` ("Too many open files") es uno de los errores clasicos en
programas que manejan muchos archivos o conexiones de red (cada socket tambien
consume un file descriptor).

En produccion, esto puede ocurrir si:

- Un servidor web no cierra conexiones
- Un programa procesa miles de archivos sin cerrar los anteriores
- Un loop abre archivos condicionalmente pero solo cierra en el camino exitoso

El patron seguro es: abrir, usar, cerrar. Siempre.

### Paso 4.5 -- Patron correcto: abrir y cerrar en cada iteracion

```bash
cat > fd_correct.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

int main(void) {
    /* Same reduced limit */
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);
    rl.rlim_cur = 30;
    setrlimit(RLIMIT_NOFILE, &rl);

    printf("Processing 100 files with limit of 30 FDs...\n");

    char filename[64];
    for (int i = 0; i < 100; i++) {
        snprintf(filename, sizeof(filename), "tmp_ok_%04d.txt", i);
        FILE *f = fopen(filename, "w");
        if (f == NULL) {
            perror("fopen");
            return EXIT_FAILURE;
        }
        fprintf(f, "file %d\n", i);
        fclose(f);    /* Close immediately after use */
        remove(filename);
    }

    printf("All 100 files processed successfully!\n");
    printf("Key: fclose() after each fopen() reuses the FD\n");
    return EXIT_SUCCESS;
}
EOF

gcc -std=c11 -Wall -Wextra -Wpedantic -o fd_correct fd_correct.c
./fd_correct
```

Salida esperada:

```
Processing 100 files with limit of 30 FDs...
All 100 files processed successfully!
Key: fclose() after each fopen() reuses the FD
```

Con solo 30 FDs disponibles, procesamos 100 archivos sin problema. Cada
`fclose` libera el file descriptor para reutilizarlo en el siguiente `fopen`.

### Limpieza de la parte 4

```bash
rm -f fd_limit fd_correct fd_correct.c
```

---

## Limpieza final

```bash
rm -f open_close std_streams open_modes fd_limit
rm -f testfile.txt modes_test.txt wplus_demo.txt
rm -f error_test error_test.c wplus_test wplus_test.c
rm -f fd_correct fd_correct.c
rm -f stdout_only.txt stderr_only.txt out.txt err.txt
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  fd_limit.c  open_close.c  open_modes.c  std_streams.c
```

---

## Conceptos reforzados

1. `fopen` retorna `NULL` cuando falla -- verificar **siempre** el retorno antes
   de usar el `FILE*`. Usar el puntero sin verificar causa segfault.

2. `perror` imprime automaticamente la descripcion del error almacenado en
   `errno`. Es la forma mas rapida de diagnosticar por que fallo `fopen`.

3. `fclose` no solo cierra el archivo: flushea el buffer (escribe datos
   pendientes al disco), cierra el file descriptor, y libera la memoria del
   struct `FILE`.

4. `stdout` y `stderr` son streams `FILE*` independientes. Se pueden redirigir
   por separado con `>` (stdout) y `2>` (stderr). Los mensajes de error deben ir
   a `stderr`.

5. El modo `"w"` **trunca** (borra todo el contenido) si el archivo ya existe.
   Para agregar al final sin perder datos, usar `"a"`.

6. El modo `"r+"` permite leer y escribir sin truncar, pero el archivo debe
   existir. `"w+"` permite leer y escribir, pero **trunca primero**.

7. Olvidar `fclose` causa file descriptor leaks. Cuando se agotan los FDs
   disponibles (`EMFILE`, errno 24), `fopen` falla. El patron seguro es abrir,
   usar, y cerrar inmediatamente.

8. Cada proceso arranca con 3 FDs ya abiertos: stdin (0), stdout (1), stderr (2).
   El limite de FDs por proceso se consulta con `ulimit -n`.
