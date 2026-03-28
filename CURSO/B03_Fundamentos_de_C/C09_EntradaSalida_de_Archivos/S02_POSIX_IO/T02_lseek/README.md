# T02 — lseek

## Posicionamiento con lseek

`lseek` mueve la posición de lectura/escritura de un file
descriptor. Es el equivalente POSIX de fseek/ftell:

```c
#include <unistd.h>
#include <sys/types.h>

off_t lseek(int fd, off_t offset, int whence);
// Mueve la posición del fd.
// Retorna la nueva posición absoluta, o -1 si error.

// whence:
// SEEK_SET — offset desde el inicio del archivo
// SEEK_CUR — offset desde la posición actual
// SEEK_END — offset desde el final del archivo
```

### Ejemplos básicos

```c
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int main(void) {
    int fd = open("data.txt", O_RDONLY);
    if (fd == -1) { perror("open"); return 1; }

    // Ir al inicio:
    lseek(fd, 0, SEEK_SET);

    // Ir al byte 100:
    lseek(fd, 100, SEEK_SET);

    // Avanzar 50 bytes desde la posición actual:
    lseek(fd, 50, SEEK_CUR);

    // Retroceder 10 bytes:
    lseek(fd, -10, SEEK_CUR);

    // Ir al final:
    lseek(fd, 0, SEEK_END);

    // Ir 10 bytes antes del final:
    lseek(fd, -10, SEEK_END);

    close(fd);
    return 0;
}
```

### Obtener la posición actual (como ftell)

```c
// lseek con offset 0 y SEEK_CUR retorna la posición actual:
off_t pos = lseek(fd, 0, SEEK_CUR);
printf("Current position: %ld\n", (long)pos);
```

### Obtener el tamaño del archivo

```c
off_t get_file_size(int fd) {
    off_t current = lseek(fd, 0, SEEK_CUR);    // guardar posición
    off_t size = lseek(fd, 0, SEEK_END);        // ir al final
    lseek(fd, current, SEEK_SET);                // restaurar posición
    return size;
}

// Alternativa: usar fstat (más eficiente, no mueve la posición):
#include <sys/stat.h>
struct stat st;
fstat(fd, &st);
off_t size = st.st_size;
```

## Lectura/escritura en posición arbitraria

```c
// Con lseek se puede implementar acceso aleatorio:

struct Record {
    int id;
    char name[50];
    double score;
};

// Leer el registro N de un archivo de registros:
int read_record(int fd, int n, struct Record *rec) {
    off_t offset = n * sizeof(struct Record);
    if (lseek(fd, offset, SEEK_SET) == -1) return -1;
    ssize_t bytes = read(fd, rec, sizeof(struct Record));
    return (bytes == sizeof(struct Record)) ? 0 : -1;
}

// Escribir el registro N:
int write_record(int fd, int n, const struct Record *rec) {
    off_t offset = n * sizeof(struct Record);
    if (lseek(fd, offset, SEEK_SET) == -1) return -1;
    ssize_t bytes = write(fd, rec, sizeof(struct Record));
    return (bytes == sizeof(struct Record)) ? 0 : -1;
}
```

```c
// pread y pwrite — lectura/escritura atómica en posición:
#include <unistd.h>

ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);

// No mueven la posición del fd — son atómicos:
pread(fd, buf, sizeof(buf), 100);    // lee en posición 100
// La posición del fd NO cambió.

// Ventajas sobre lseek + read:
// 1. Atómico — thread-safe (no hay race condition entre seek y read)
// 2. No modifica la posición — otros threads no se ven afectados
// 3. Una syscall en vez de dos
```

## Sparse files

```c
// lseek permite posicionar DESPUÉS del final del archivo.
// Si se escribe ahí, se crea un "hueco" (hole) → sparse file:

int fd = open("sparse.dat", O_WRONLY | O_CREAT | O_TRUNC, 0644);

// Escribir al inicio:
write(fd, "A", 1);                    // byte 0: 'A'

// Saltar a 1 GB:
lseek(fd, 1024L * 1024 * 1024, SEEK_SET);

// Escribir al final:
write(fd, "B", 1);                    // byte 1073741824: 'B'

close(fd);

// El archivo "mide" ~1 GB pero solo usa 2 bloques de disco.
// Los bytes del hueco se leen como '\0' sin ocupar espacio.
```

```bash
# Verificar con ls y du:
ls -l sparse.dat     # 1073741825 bytes (tamaño lógico)
du -h sparse.dat     # 8.0K (tamaño real en disco)

# stat muestra ambos:
stat sparse.dat
#   Size: 1073741825    Blocks: 16
```

```c
// Sparse files se usan en:
// - Bases de datos (SQLite, PostgreSQL)
// - Imágenes de disco (qcow2, vmdk)
// - Core dumps
// - Archivos de swap
//
// Leer el hueco retorna ceros:
char buf[10];
lseek(fd, 500, SEEK_SET);
read(fd, buf, 10);
// buf = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
```

## SEEK_DATA y SEEK_HOLE (Linux 3.1+)

```c
// Encontrar datos y huecos en sparse files:

// SEEK_DATA — buscar el siguiente byte con datos:
off_t data_start = lseek(fd, 0, SEEK_DATA);

// SEEK_HOLE — buscar el siguiente hueco:
off_t hole_start = lseek(fd, 0, SEEK_HOLE);

// Iterar segmentos de un sparse file:
off_t pos = 0;
while (1) {
    off_t data = lseek(fd, pos, SEEK_DATA);
    if (data == -1) break;    // no más datos

    off_t hole = lseek(fd, data, SEEK_HOLE);
    if (hole == -1) hole = lseek(fd, 0, SEEK_END);

    printf("Data at [%ld, %ld)\n", (long)data, (long)hole);
    pos = hole;
}
```

## lseek en pipes y sockets

```c
// lseek NO funciona en:
// - Pipes (|)
// - Sockets
// - Terminales
// - Otros streams no seekable

int pipefd[2];
pipe(pipefd);
off_t pos = lseek(pipefd[0], 0, SEEK_CUR);
// pos = -1, errno = ESPIPE ("Illegal seek")

// Para verificar si un fd es seekable:
if (lseek(fd, 0, SEEK_CUR) == -1 && errno == ESPIPE) {
    // No seekable — es un pipe, socket o terminal
}
```

---

## Ejercicios

### Ejercicio 1 — Base de datos de registros

```c
// Crear un archivo binario de struct Record { int id; char name[50]; double score; }.
// Implementar:
// - add_record(fd, record) — agrega al final
// - get_record(fd, index) — lee el registro N
// - update_record(fd, index, record) — sobreescribe el registro N
// - count_records(fd) — retorna el número de registros
// Probar: agregar 5 registros, leer el 3ro, actualizar el 2do.
```

### Ejercicio 2 — Sparse file

```c
// Crear un sparse file de 100 MB con datos solo en:
// - Byte 0: "START"
// - Byte 50MB: "MIDDLE"
// - Byte 100MB-4: "END"
// Verificar con ls -l y du que el tamaño lógico >> tamaño real.
// Leer y verificar los datos en las tres posiciones.
```

### Ejercicio 3 — tail con lseek

```c
// Implementar una versión simple de tail -n N:
// 1. Abrir el archivo
// 2. Ir al final con lseek
// 3. Retroceder buscando N newlines
// 4. Imprimir desde esa posición hasta el final
// Probar con: ./mytail 5 /var/log/syslog
```
