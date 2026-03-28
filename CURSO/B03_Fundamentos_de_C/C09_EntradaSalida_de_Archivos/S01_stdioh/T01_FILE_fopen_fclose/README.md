# T01 — FILE*, fopen, fclose

## El tipo FILE

`FILE` es un tipo opaco definido en `<stdio.h>` que representa
un **stream** (flujo de datos). Internamente contiene el file
descriptor, el buffer, la posición actual y flags de estado:

```c
#include <stdio.h>

// No se puede crear un FILE directamente.
// Se obtiene con fopen y se usa como puntero:
FILE *f = fopen("data.txt", "r");

// Tres streams están abiertos automáticamente:
// stdin  — entrada estándar (teclado)
// stdout — salida estándar (pantalla)
// stderr — salida de error (pantalla, sin buffer)
```

## fopen — Abrir archivos

```c
FILE *fopen(const char *path, const char *mode);
// Retorna FILE* si éxito, NULL si error (y establece errno).

FILE *f = fopen("data.txt", "r");
if (f == NULL) {
    perror("fopen");    // imprime: fopen: No such file or directory
    return -1;
}
```

### Modos de apertura

```c
// Modos de texto:
"r"    // Read — lectura. El archivo debe existir.
"w"    // Write — escritura. Crea o TRUNCA (borra contenido).
"a"    // Append — escritura al final. Crea si no existe.
"r+"   // Read+Write — lectura y escritura. Debe existir.
"w+"   // Write+Read — lectura y escritura. Crea o trunca.
"a+"   // Append+Read — lectura + escritura al final. Crea si no existe.

// Modos binarios (agregar 'b'):
"rb"   // Read binario
"wb"   // Write binario
"ab"   // Append binario
"rb+"  // o "r+b" — Read+Write binario
"wb+"  // o "w+b" — Write+Read binario
"ab+"  // o "a+b" — Append+Read binario

// En Linux, 'b' no tiene efecto (no hay distinción texto/binario).
// En Windows, 'b' evita la conversión \n ↔ \r\n.
// Para portabilidad, siempre usar 'b' para archivos binarios.
```

```c
// Modo exclusivo 'x' (C11) — falla si el archivo ya existe:
"wx"   // Write exclusivo — crea nuevo, falla si existe
"w+x"  // Write+Read exclusivo

FILE *f = fopen("output.txt", "wx");
if (f == NULL && errno == EEXIST) {
    printf("File already exists\n");
}
// Útil para evitar sobrescribir archivos accidentalmente.
// Equivale a O_CREAT | O_EXCL en POSIX.
```

### Tabla resumen de modos

| Modo | Lectura | Escritura | Crea | Trunca | Posición |
|---|---|---|---|---|---|
| r | Sí | No | No | No | Inicio |
| w | No | Sí | Sí | Sí | Inicio |
| a | No | Sí | Sí | No | Final |
| r+ | Sí | Sí | No | No | Inicio |
| w+ | Sí | Sí | Sí | Sí | Inicio |
| a+ | Sí | Sí | Sí | No | Final (write) |

## fclose — Cerrar archivos

```c
int fclose(FILE *stream);
// Retorna 0 si éxito, EOF si error.

FILE *f = fopen("data.txt", "r");
if (!f) { perror("fopen"); return -1; }

// ... usar f ...

if (fclose(f) != 0) {
    perror("fclose");
    // El error puede indicar que los datos no se escribieron al disco.
}
```

```c
// fclose hace varias cosas:
// 1. Flushea el buffer (escribe datos pendientes al disco)
// 2. Cierra el file descriptor subyacente
// 3. Libera la memoria del struct FILE

// SIEMPRE cerrar los archivos abiertos:
// - El SO tiene un límite de archivos abiertos (ulimit -n, típicamente 1024)
// - Los datos pueden no escribirse si no se cierra
// - Memory leak del struct FILE

// fclose(NULL) es UB — verificar antes de cerrar:
if (f) fclose(f);
```

## Errores comunes

```c
// 1. No verificar el retorno de fopen:
FILE *f = fopen("noexiste.txt", "r");
fgets(buf, sizeof(buf), f);    // segfault si f es NULL

// 2. Usar el modo incorrecto:
FILE *f = fopen("data.txt", "r");
fprintf(f, "hello");    // no escribe — modo lectura
// No da error inmediato — los datos se descartan silenciosamente.

// 3. "w" trunca el archivo existente:
FILE *f = fopen("important.txt", "w");
// ¡El contenido anterior se BORRÓ! Usar "a" para agregar.

// 4. Olvidar fclose:
void process(void) {
    FILE *f = fopen("data.txt", "r");
    // ... procesar ...
    return;    // LEAK — f no se cerró
}

// 5. Usar f después de fclose:
fclose(f);
fgets(buf, sizeof(buf), f);    // UB — f ya fue cerrado
```

## Patrón seguro

```c
#include <stdio.h>
#include <stdlib.h>

int process_file(const char *path) {
    int result = -1;
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return -1;
    }

    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        // procesar línea...
    }

    if (ferror(f)) {
        perror("read error");
        goto cleanup;
    }

    result = 0;

cleanup:
    fclose(f);
    return result;
}
```

## Verificación de errores y EOF

```c
FILE *f = fopen("data.txt", "r");

// Después de leer, verificar POR QUÉ terminó:
while (fgets(buf, sizeof(buf), f)) {
    // procesar...
}

// ¿Terminó por EOF o por error?
if (feof(f)) {
    // Fin del archivo — normal
} else if (ferror(f)) {
    // Error de I/O
    perror("read error");
}

// clearerr — resetear indicadores de error y EOF:
clearerr(f);
// Permite seguir leyendo/escribiendo después de un error.
```

```c
// errno se establece cuando fopen falla:
#include <errno.h>
#include <string.h>

FILE *f = fopen(path, "r");
if (!f) {
    // errno contiene el código de error:
    fprintf(stderr, "Error %d: %s\n", errno, strerror(errno));

    // Errores comunes:
    // ENOENT — archivo no existe
    // EACCES — sin permisos
    // EMFILE — demasiados archivos abiertos
    // EEXIST — ya existe (con modo "x")

    // perror es el shortcut:
    perror(path);    // imprime: path: mensaje de error
}
```

## stdin, stdout, stderr

```c
// Los tres streams estándar están siempre abiertos:

// Leer de stdin:
char buf[100];
fgets(buf, sizeof(buf), stdin);
// Equivale a: scanf, getchar, gets (obsoleta)

// Escribir a stdout:
fprintf(stdout, "Hello\n");
// Equivale a: printf, puts, putchar

// Escribir a stderr (para mensajes de error):
fprintf(stderr, "Error: %s\n", message);
// stderr NO tiene buffer de línea — se imprime inmediatamente.
// No se mezcla con stdout en pipes:
//   ./prog > output.txt    ← solo stdout va al archivo
//   ./prog 2> errors.txt   ← solo stderr va al archivo
//   ./prog > out.txt 2>&1  ← ambos al mismo archivo

// NO cerrar stdin/stdout/stderr:
// fclose(stdout);    // legal pero causa problemas
```

## tmpfile — Archivos temporales

```c
// tmpfile crea un archivo temporal que se elimina al cerrarse:
FILE *tmp = tmpfile();
if (!tmp) { perror("tmpfile"); return -1; }

fprintf(tmp, "data temporal\n");
rewind(tmp);    // volver al inicio

char buf[100];
fgets(buf, sizeof(buf), tmp);
printf("Leído: %s", buf);

fclose(tmp);    // el archivo se elimina automáticamente
```

---

## Ejercicios

### Ejercicio 1 — Contar líneas

```c
// Implementar int count_lines(const char *path)
// que abra un archivo, cuente las líneas, y cierre.
// Manejar errores de fopen correctamente.
// Probar con un archivo existente y uno que no existe.
```

### Ejercicio 2 — Copiar archivo

```c
// Implementar int copy_file(const char *src, const char *dst)
// que copie el contenido de src a dst.
// Usar fgets/fputs (texto) o fread/fwrite (binario).
// Si dst ya existe, preguntar antes de sobrescribir.
// Manejar todos los errores.
```

### Ejercicio 3 — Modos de apertura

```c
// Crear un programa que demuestre la diferencia entre
// los modos "w", "a" y "r+":
// 1. Crear un archivo con "w", escribir "line 1\n"
// 2. Abrir con "a", escribir "line 2\n"
// 3. Abrir con "r+", escribir "XXXX" (¿qué pasa?)
// 4. Abrir con "w", escribir "line 3\n" (¿qué pasa con lines 1-2?)
// Después de cada paso, leer e imprimir el contenido completo.
```
