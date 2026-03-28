# T03 — Diferencia entre stdio y POSIX I/O

## Dos APIs para lo mismo

Linux ofrece dos niveles de I/O:

```c
// POSIX I/O (bajo nivel, unbuffered):
#include <fcntl.h>
#include <unistd.h>

int fd = open("file.txt", O_RDONLY);
char buf[100];
ssize_t n = read(fd, buf, sizeof(buf));
write(STDOUT_FILENO, buf, n);
close(fd);

// stdio (alto nivel, buffered):
#include <stdio.h>

FILE *f = fopen("file.txt", "r");
char buf[100];
fgets(buf, sizeof(buf), f);
printf("%s", buf);
fclose(f);
```

```c
// stdio está CONSTRUIDA sobre POSIX I/O:
//
// fopen → open
// fread → read (con buffer intermedio)
// fwrite → write (con buffer intermedio)
// fclose → flush + close
// fseek → lseek
// fprintf → formatea string → write
// fgets → read del buffer (recarga si vacío)
//
// stdio agrega:
// - Buffering (reduce syscalls)
// - Formateo (printf, scanf)
// - Portabilidad (funciona en Windows)
// - Manejo de texto vs binario
```

## Comparación

| Aspecto | stdio (FILE*) | POSIX (fd) |
|---|---|---|
| Header | `<stdio.h>` | `<fcntl.h>`, `<unistd.h>` |
| Handle | `FILE *` | `int` (fd) |
| Buffering | Sí (automático) | No |
| Formateo | printf/scanf | No (manual) |
| Portabilidad | C estándar (todos los OS) | POSIX (Unix/Linux/macOS) |
| Rendimiento | Menor overhead por syscall | Control total |
| Funciones | fopen, fread, fprintf... | open, read, write... |
| Newline conversion | Sí (modo texto) | No |
| Señales | Manejo interno | EINTR manual |
| Thread safety | Sí (locks internos) | No (manual) |

## Cuándo usar stdio

```c
// 1. Archivos de texto — lectura/escritura formateada:
FILE *f = fopen("config.txt", "r");
char line[256];
while (fgets(line, sizeof(line), f)) {
    int key, val;
    if (sscanf(line, "%d = %d", &key, &val) == 2) {
        // procesar...
    }
}

// 2. Interacción con usuario (stdin/stdout):
printf("Enter name: ");
fgets(name, sizeof(name), stdin);

// 3. Logging:
fprintf(log_file, "[%s] %s\n", timestamp, message);

// 4. Código portátil (debe funcionar en Windows):
// stdio es C estándar — disponible en todos los compiladores.
// POSIX I/O no existe en Windows (usa CreateFile/ReadFile/WriteFile).

// 5. Cuando no necesitas control fino:
// stdio maneja buffering, errores, encoding automáticamente.
```

## Cuándo usar POSIX I/O

```c
// 1. Operaciones que stdio no soporta:
// - Archivos con flags especiales (O_NONBLOCK, O_SYNC)
// - Permisos específicos al crear
// - O_CLOEXEC para exec() seguro
int fd = open("file", O_WRONLY | O_CREAT | O_SYNC | O_CLOEXEC, 0600);

// 2. I/O no-bloqueante:
int fd = open("/dev/ttyUSB0", O_RDWR | O_NONBLOCK);
// Si no hay datos disponibles, read retorna -1 con errno = EAGAIN.
// stdio no tiene equivalente directo.

// 3. Sockets y pipes:
int sockfd = socket(AF_INET, SOCK_STREAM, 0);
read(sockfd, buf, sizeof(buf));
write(sockfd, response, response_len);
// Sockets y pipes son fds — se usan con read/write directamente.
// Se puede crear un FILE* con fdopen, pero es poco común.

// 4. Multiplexación con poll/epoll/select:
struct pollfd fds[2];
fds[0].fd = sockfd;    // fds, no FILE*
fds[0].events = POLLIN;
poll(fds, 2, timeout);

// 5. Memory-mapped I/O:
void *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
// mmap requiere un fd.

// 6. Cuando el buffering de stdio estorba:
// Servidores que necesitan enviar datos inmediatamente.
// Comunicación con hardware/dispositivos.

// 7. Signal handlers:
// write es async-signal-safe; fprintf NO lo es.
void signal_handler(int sig) {
    const char msg[] = "Signal received\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);    // seguro
    // fprintf(stderr, "Signal %d\n", sig);          // NO seguro
}
```

## Mezclar stdio y POSIX I/O

```c
// PELIGRO: mezclar stdio y POSIX en el mismo fd puede causar
// datos desordenados o perdidos por el buffering de stdio.

FILE *f = fopen("data.txt", "w");
int fd = fileno(f);

fprintf(f, "Hello from stdio\n");    // en el buffer de stdio
write(fd, "Hello from POSIX\n", 17); // directamente al kernel

// Resultado: "Hello from POSIX\nHello from stdio\n"
// ¡El orden se invirtió! write fue directo, fprintf estaba en buffer.

// Solución: fflush antes de usar el fd:
fprintf(f, "Hello from stdio\n");
fflush(f);                            // enviar el buffer al kernel
write(fd, "Hello from POSIX\n", 17);  // ahora está en orden
```

```c
// Regla: NO mezclar stdio y POSIX I/O en el mismo fd.
// Si es necesario:
// 1. Usar solo uno de los dos
// 2. Si debes mezclar: fflush antes de cambiar de API
// 3. Mejor: crear un FILE* con fdopen y usar solo stdio

int fd = open("file.txt", O_RDWR | O_CREAT, 0644);
FILE *f = fdopen(fd, "r+");
// Ahora usar solo f (stdio).
// fclose(f) también cierra fd — no hacer close(fd) después.
```

## Rendimiento

```c
// Lectura de un archivo de 100 MB:

// POSIX read, buffer de 1 byte:
while (read(fd, &c, 1) > 0) { /* ... */ }
// ~100 millones de syscalls → MUY lento (~30 segundos)

// stdio fgetc (internamente usa buffer de 8 KB):
while ((c = fgetc(f)) != EOF) { /* ... */ }
// ~12,000 syscalls → rápido (~0.5 segundos)
// fgetc lee 8 KB del kernel de una vez, retorna byte a byte del buffer.

// POSIX read, buffer de 4 KB:
while ((n = read(fd, buf, 4096)) > 0) { /* procesar buf */ }
// ~25,000 syscalls → rápido (~0.3 segundos)

// POSIX read, buffer de 64 KB:
while ((n = read(fd, buf, 65536)) > 0) { /* procesar buf */ }
// ~1,500 syscalls → aún más rápido (~0.2 segundos)

// mmap:
void *data = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
// 0 syscalls para acceder (page faults manejados por el kernel)
// Para acceso secuencial, rendimiento similar a read con buffer grande.
// Para acceso aleatorio, puede ser más rápido.
```

## Tabla resumen: qué API usar

| Caso de uso | API recomendada |
|---|---|
| Leer/escribir texto con formato | stdio (fprintf, fscanf) |
| Interacción con usuario | stdio (fgets, printf) |
| Archivos de configuración | stdio |
| Archivos binarios simples | stdio (fread, fwrite) |
| Código portátil | stdio |
| Sockets | POSIX (read, write) |
| Non-blocking I/O | POSIX |
| poll/epoll/select | POSIX (requiere fds) |
| mmap | POSIX (requiere fd) |
| Signal handlers | POSIX (write es async-signal-safe) |
| Flags de open especiales | POSIX |
| Control de permisos al crear | POSIX |

---

## Ejercicios

### Ejercicio 1 — Comparar rendimiento

```c
// Leer un archivo grande (>10 MB) de 4 formas:
// 1. read() con buffer de 1 byte
// 2. read() con buffer de 4096 bytes
// 3. fgetc() (stdio, byte a byte)
// 4. fread() con buffer de 4096 bytes
// Medir el tiempo de cada una con clock_gettime.
// ¿Cuál es más rápida? ¿Por qué?
```

### Ejercicio 2 — Mezclar APIs

```c
// Demostrar el problema de mezclar stdio y POSIX:
// 1. Abrir un archivo con fopen
// 2. Escribir "AAA" con fprintf
// 3. Escribir "BBB" con write(fileno(f), ...)
// 4. Escribir "CCC" con fprintf
// 5. Cerrar con fclose
// ¿En qué orden aparecen AAA, BBB, CCC en el archivo?
// Repetir con fflush entre cada cambio de API.
```

### Ejercicio 3 — Elegir la API correcta

```c
// Para cada escenario, decidir si usar stdio o POSIX I/O y por qué:
// 1. Servidor TCP que maneja 1000 conexiones con epoll
// 2. Programa que lee un CSV y genera un reporte formateado
// 3. Logger que debe escribir inmediatamente al disco
// 4. Programa que copia archivos binarios grandes
// 5. Signal handler que debe reportar errores
// Implementar el escenario 2 con stdio y el 5 con POSIX.
```
