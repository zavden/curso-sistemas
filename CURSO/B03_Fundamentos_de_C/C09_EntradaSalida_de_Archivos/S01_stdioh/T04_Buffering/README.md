# T04 — Buffering

## Qué es el buffering

stdio.h no escribe datos al SO inmediatamente. Los acumula
en un **buffer** en memoria del proceso y los envía en lotes.
Esto reduce las syscalls (write) y mejora el rendimiento:

```c
// Sin buffering (cada printf hace una syscall write):
// printf("H");  → write(1, "H", 1)
// printf("e");  → write(1, "e", 1)
// printf("l");  → write(1, "l", 1)
// printf("l");  → write(1, "l", 1)
// printf("o");  → write(1, "o", 1)
// 5 syscalls — lento

// Con buffering (los datos se acumulan y se envían juntos):
// printf("H");  → buffer: "H"
// printf("e");  → buffer: "He"
// printf("l");  → buffer: "Hel"
// printf("l");  → buffer: "Hell"
// printf("o");  → buffer: "Hello"
// flush         → write(1, "Hello", 5)
// 1 syscall — rápido
```

## Tres modos de buffering

```c
// 1. FULL BUFFERING (_IOFBF):
// Los datos se envían cuando el buffer se llena
// o cuando se cierra el archivo.
// Usado por: archivos regulares abiertos con fopen.
// Tamaño de buffer típico: BUFSIZ (8192 bytes en Linux).

// 2. LINE BUFFERING (_IOLBF):
// Los datos se envían cuando se encuentra un '\n'
// o cuando el buffer se llena.
// Usado por: stdout cuando está conectado a una terminal.

// 3. UNBUFFERED (_IONBF):
// Los datos se envían inmediatamente (cada escritura = syscall).
// Usado por: stderr (siempre unbuffered).
```

```c
#include <stdio.h>

int main(void) {
    // stdout a terminal: line buffered
    printf("Hello");     // NO se imprime todavía (sin '\n')
    printf(" World\n");  // Se imprime "Hello World\n" (el '\n' flushea)

    // stderr: unbuffered
    fprintf(stderr, "Error");   // Se imprime inmediatamente

    // Archivo: full buffered
    FILE *f = fopen("log.txt", "w");
    fprintf(f, "data");    // NO se escribe al disco todavía
    fclose(f);             // fclose flushea el buffer → se escribe

    return 0;
}
```

## Por qué printf sin \n "no imprime"

```c
// El problema clásico de los principiantes:
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("Loading...");
    sleep(5);              // espera 5 segundos
    printf(" Done!\n");

    return 0;
}

// Comportamiento en terminal:
// (5 segundos de espera sin nada visible)
// Loading... Done!
//
// ¿Por qué? stdout es line buffered en terminal.
// "Loading..." no tiene '\n' → queda en el buffer.
// Después de 5 segundos, " Done!\n" tiene '\n' → flushea todo.
```

```c
// Soluciones:

// 1. Agregar '\n':
printf("Loading...\n");    // flushea inmediatamente

// 2. fflush explícito:
printf("Loading...");
fflush(stdout);            // forzar el flush
sleep(5);
printf(" Done!\n");
// Ahora "Loading..." aparece inmediatamente.

// 3. Hacer stdout unbuffered:
setbuf(stdout, NULL);      // stdout sin buffer
printf("Loading...");      // se imprime inmediatamente

// 4. Usar stderr (siempre unbuffered):
fprintf(stderr, "Loading...");
```

## fflush — Forzar el flush

```c
int fflush(FILE *stream);
// Envía todos los datos del buffer al SO.
// Retorna 0 si éxito, EOF si error.

printf("Progress: 50%%");
fflush(stdout);    // aparece inmediatamente en pantalla

fprintf(log_file, "important event");
fflush(log_file);  // se escribe al disco inmediatamente
// Sin fflush, si el programa crashea, el dato puede perderse.
```

```c
// fflush(NULL) flushea TODOS los streams abiertos:
fflush(NULL);

// fflush en stream de input es UB en el estándar.
// Algunas implementaciones (Linux, MSVC) lo permiten
// para descartar el input no leído, pero no es portátil.
// fflush(stdin);    // NO portátil — no usar
```

## setvbuf — Configurar el buffering

```c
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
// Configura el modo de buffering de un stream.
// DEBE llamarse ANTES de cualquier lectura/escritura.
// mode: _IOFBF (full), _IOLBF (line), _IONBF (unbuffered)
// buf: buffer a usar (o NULL para que stdio aloque uno)
// size: tamaño del buffer

// Hacer stdout unbuffered:
setvbuf(stdout, NULL, _IONBF, 0);

// Hacer stdout full buffered con buffer de 64 KB:
static char big_buffer[65536];
setvbuf(stdout, big_buffer, _IOFBF, sizeof(big_buffer));

// Hacer un archivo line buffered:
FILE *f = fopen("log.txt", "w");
setvbuf(f, NULL, _IOLBF, 0);
// Ahora cada '\n' flushea al disco.
```

```c
// setbuf — versión simplificada:
void setbuf(FILE *stream, char *buf);

setbuf(stdout, NULL);    // equivale a setvbuf(stdout, NULL, _IONBF, 0)

char buf[BUFSIZ];
setbuf(f, buf);          // equivale a setvbuf(f, buf, _IOFBF, BUFSIZ)
```

## Cuándo stdout cambia de buffering

```c
// stdout es LINE BUFFERED cuando está conectado a un terminal:
// $ ./program              ← line buffered (terminal)

// stdout es FULL BUFFERED cuando está redirigido:
// $ ./program > output.txt ← full buffered (archivo)
// $ ./program | grep foo   ← full buffered (pipe)

// Esto causa sorpresas:
//   printf("tick\n");
//   sleep(1);
//   printf("tock\n");
//
// En terminal: "tick" aparece, 1 segundo, "tock" aparece
// En pipe:     (1 segundo), "tick\ntock\n" aparece todo junto
//              (porque '\n' no causa flush en full buffered)

// Solución para pipes/redirección:
// fflush(stdout) después de cada output que debe ser inmediato.
// O setvbuf(stdout, NULL, _IOLBF, 0) al inicio para forzar line buffered.
```

## Buffer y crash

```c
// Si el programa crashea, los datos en el buffer se PIERDEN:
fprintf(log_file, "About to do risky operation\n");
// Esta línea puede NO aparecer en el archivo si el programa
// crashea antes del flush.

// Soluciones:
// 1. fflush después de escrituras importantes:
fprintf(log_file, "Critical event\n");
fflush(log_file);

// 2. Hacer el log file unbuffered o line buffered:
setvbuf(log_file, NULL, _IOLBF, 0);

// 3. Usar stderr para mensajes de depuración:
fprintf(stderr, "Debug: x = %d\n", x);
// stderr es unbuffered → siempre se imprime.

// 4. Usar write() directamente (POSIX, sin buffer):
#include <unistd.h>
write(STDERR_FILENO, "crash!\n", 7);
// write es async-signal-safe — se puede usar en signal handlers.
```

## Tabla resumen

| Stream | Destino | Buffering por defecto |
|---|---|---|
| stdout (terminal) | Terminal | Line buffered |
| stdout (redirigido) | Archivo/pipe | Full buffered |
| stderr | Terminal | Unbuffered |
| fopen (archivo) | Archivo | Full buffered |

| Modo | Cuándo flushea | Uso típico |
|---|---|---|
| _IOFBF (full) | Buffer lleno o fclose | Archivos |
| _IOLBF (line) | '\n' o buffer lleno | stdout a terminal |
| _IONBF (none) | Cada escritura | stderr, depuración |

---

## Ejercicios

### Ejercicio 1 — Demostrar buffering

```c
// Escribir un programa que:
// 1. Imprima "tick" (sin \n) a stdout, espere 1 segundo, repita 5 veces
// 2. Ejecutar en terminal — ¿qué se ve?
// 3. Ejecutar con redirección (./prog > out.txt) — ¿qué pasa?
// 4. Agregar fflush(stdout) después de cada printf — ¿cambia?
// 5. Probar con setvbuf(stdout, NULL, _IONBF, 0) al inicio
```

### Ejercicio 2 — Log seguro

```c
// Implementar void log_init(const char *path) y
// void log_write(const char *fmt, ...)
// que escriban a un archivo de log con line buffering.
// Verificar que cada línea aparece inmediatamente en el archivo
// (monitorear con tail -f log.txt en otra terminal).
```

### Ejercicio 3 — Buffer personalizado

```c
// Abrir un archivo con fopen y configurar un buffer de 64 bytes.
// Escribir 100 bytes de datos en escrituras de 1 byte.
// Contar cuántas syscalls write se hacen (con strace):
//   strace -e write ./program
// Repetir sin buffer (unbuffered) y comparar el número de syscalls.
```
