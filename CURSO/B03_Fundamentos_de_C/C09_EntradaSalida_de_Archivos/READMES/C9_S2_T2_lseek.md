# T02 — lseek

> Sin erratas detectadas en el material fuente.

---

## 1. El offset — La posición interna de un file descriptor

Cada file descriptor abierto tiene un **offset** (posición actual): un número que indica el siguiente byte que se leerá o escribirá.

```c
// El offset se comporta como un cursor en un editor de texto:
//
// Archivo: [A][B][C][D][E][F][G][H][I][J]
//           ^
//           offset = 0 (recién abierto)
//
// Después de read(fd, buf, 3):
// Archivo: [A][B][C][D][E][F][G][H][I][J]
//                    ^
//                    offset = 3 (avanzó automáticamente)
//
// Después de write(fd, "XY", 2):
// Archivo: [A][B][C][X][Y][F][G][H][I][J]
//                         ^
//                         offset = 5 (avanzó automáticamente)

// Regla: read() y write() SIEMPRE avanzan el offset
// en la cantidad de bytes leídos/escritos.
// lseek() es la única forma de moverlo manualmente.
```

### El offset es propiedad de la descripción de archivo abierta, no del fd

```c
// Cada open() crea una nueva "open file description" en el kernel.
// Esa estructura contiene el offset.

int fd1 = open("data.txt", O_RDONLY);   // Descripción A (offset propio)
int fd2 = open("data.txt", O_RDONLY);   // Descripción B (offset propio)
// fd1 y fd2 son independientes: leer de fd1 no afecta fd2.

// PERO con dup() o fork():
int fd3 = dup(fd1);
// fd1 y fd3 COMPARTEN la misma descripción → el mismo offset.
// Si read(fd1) avanza el offset, fd3 también lo ve avanzado.
```

---

## 2. lseek — Mover la posición del file descriptor

```c
#include <unistd.h>
#include <sys/types.h>   // off_t

off_t lseek(int fd, off_t offset, int whence);
// Mueve la posición del fd según whence + offset.
// Retorna: la nueva posición ABSOLUTA (desde el inicio), o (off_t)-1 si error.
```

### off_t y archivos grandes

```c
// off_t es un tipo con signo (normalmente long o long long).
// En sistemas de 32 bits, off_t = 32 bits → máximo 2 GB.
// Para archivos > 2 GB en 32 bits:
//   #define _FILE_OFFSET_BITS 64   // ANTES de cualquier #include
//   // Esto hace que off_t sea de 64 bits.
//
// En sistemas de 64 bits (x86_64), off_t ya es de 64 bits.
// No necesitas _FILE_OFFSET_BITS (pero no hace daño ponerlo).
```

### Equivalencia con stdio

| POSIX | stdio | Función |
|-------|-------|---------|
| `lseek(fd, offset, SEEK_SET)` | `fseek(f, offset, SEEK_SET)` | Posicionar |
| `lseek(fd, 0, SEEK_CUR)` | `ftell(f)` | Obtener posición |
| `lseek(fd, 0, SEEK_SET)` | `rewind(f)` | Ir al inicio |

**Diferencia clave**: `lseek` retorna la posición nueva (útil para obtener tamaño). `fseek` retorna 0 o -1 (éxito/error) y necesitas `ftell` por separado.

---

## 3. Los tres modos de whence

### SEEK_SET — Posición absoluta desde el inicio

```c
// Posición = offset (debe ser >= 0)

lseek(fd, 0, SEEK_SET);     // Ir al inicio del archivo
lseek(fd, 100, SEEK_SET);   // Ir al byte 100

// Caso de uso: acceder a un registro específico por índice
// offset = index * sizeof(struct Record)
```

### SEEK_CUR — Relativo a la posición actual

```c
// Nueva posición = posición_actual + offset
// offset puede ser negativo (retroceder)

lseek(fd, 50, SEEK_CUR);    // Avanzar 50 bytes
lseek(fd, -10, SEEK_CUR);   // Retroceder 10 bytes

// Caso de uso especial: obtener posición actual sin moverla
off_t pos = lseek(fd, 0, SEEK_CUR);
// Equivale a ftell(f) en stdio
```

### SEEK_END — Relativo al final del archivo

```c
// Nueva posición = tamaño_archivo + offset
// offset típicamente es 0 o negativo

lseek(fd, 0, SEEK_END);     // Ir al final (para obtener tamaño)
lseek(fd, -3, SEEK_END);    // 3 bytes antes del final
lseek(fd, -1, SEEK_END);    // Último byte del archivo

// Caso de uso: implementar tail (leer las últimas N líneas)
```

### Ejemplo completo: los tres modos en acción

```c
// Archivo: "ABCDEFGHIJKLMNOPQRSTUVWXYZ" (26 bytes)

char buf[4];

lseek(fd, 10, SEEK_SET);          // posición = 10
read(fd, buf, 3); buf[3] = '\0';  // buf = "KLM", posición = 13

lseek(fd, 5, SEEK_CUR);           // posición = 13 + 5 = 18
read(fd, buf, 3); buf[3] = '\0';  // buf = "STU", posición = 21

lseek(fd, -3, SEEK_END);          // posición = 26 - 3 = 23
read(fd, buf, 3); buf[3] = '\0';  // buf = "XYZ", posición = 26

// Nota: read() avanza el offset automáticamente.
// Después de leer 3 bytes en posición 10, estamos en 13.
```

---

## 4. Obtener tamaño de archivo

### Método 1: lseek al final (patrón save/seek/restore)

```c
off_t get_file_size(int fd) {
    off_t saved = lseek(fd, 0, SEEK_CUR);   // 1. Guardar posición
    off_t size  = lseek(fd, 0, SEEK_END);    // 2. Ir al final = tamaño
    lseek(fd, saved, SEEK_SET);               // 3. Restaurar posición
    return size;
}

// ¡CRÍTICO! Paso 3 (restaurar) es obligatorio.
// Sin él, la siguiente read/write ocurre al final del archivo,
// no donde esperabas. Este es un bug sutil y difícil de diagnosticar.
```

### Método 2: fstat (más eficiente, recomendado)

```c
#include <sys/stat.h>

off_t get_file_size_fstat(int fd) {
    struct stat st;
    if (fstat(fd, &st) == -1) return -1;
    return st.st_size;
}

// Ventajas sobre lseek:
// 1. No modifica la posición — no necesita save/restore
// 2. Una sola syscall en vez de tres
// 3. Obtiene más info: permisos, timestamps, device, inode...

// Para archivos por path (sin abrir):
struct stat st;
stat("data.txt", &st);   // stat() en vez de fstat()
off_t size = st.st_size;
```

### Método 3: stat desde línea de comandos

```bash
# Tamaño en bytes:
stat -c %s data.txt        # Solo tamaño
stat data.txt              # Información completa
ls -l data.txt             # Tamaño en la quinta columna
wc -c < data.txt           # Contar bytes
```

---

## 5. Acceso aleatorio a registros de tamaño fijo

La combinación `lseek + read/write` permite acceso directo a cualquier registro sin recorrer el archivo completo. Es el fundamento de las bases de datos simples.

```c
struct Record {
    int id;
    char name[50];
    double score;
};
// sizeof depende del padding (típicamente 64 bytes en x86_64)

// Clave: con tamaño fijo, la posición del registro i es PREDECIBLE:
//   offset = i * sizeof(struct Record)
//
// Archivo:
// [Record 0][Record 1][Record 2][Record 3][Record 4]
// |--- 64 --|--- 64 --|--- 64 --|--- 64 --|--- 64 --|
// byte 0     byte 64   byte 128  byte 192  byte 256
```

### Funciones de acceso

```c
int write_record(int fd, int index, const struct Record *rec) {
    off_t offset = (off_t)index * (off_t)sizeof(struct Record);
    if (lseek(fd, offset, SEEK_SET) == -1) return -1;
    ssize_t n = write(fd, rec, sizeof(struct Record));
    return (n == (ssize_t)sizeof(struct Record)) ? 0 : -1;
}

int read_record(int fd, int index, struct Record *rec) {
    off_t offset = (off_t)index * (off_t)sizeof(struct Record);
    if (lseek(fd, offset, SEEK_SET) == -1) return -1;
    ssize_t n = read(fd, rec, sizeof(struct Record));
    return (n == (ssize_t)sizeof(struct Record)) ? 0 : -1;
}

int count_records(int fd) {
    off_t saved = lseek(fd, 0, SEEK_CUR);
    off_t size  = lseek(fd, 0, SEEK_END);
    lseek(fd, saved, SEEK_SET);
    return (int)(size / (off_t)sizeof(struct Record));
}
```

### Limitaciones del acceso aleatorio con registros

```c
// 1. Solo funciona con tamaño FIJO:
//    struct con char name[50] ✓ (siempre 50 bytes)
//    struct con char *name    ✗ (puntero no tiene sentido en disco)

// 2. No es portable entre arquitecturas:
//    - Padding puede diferir (32-bit vs 64-bit)
//    - Endianness puede diferir (x86 vs ARM big-endian)
//    - sizeof(int) puede diferir
//    Para portabilidad, usar serialización explícita.

// 3. Borrar un registro del medio es costoso:
//    - Opción A: marcar como "deleted" (flag)
//    - Opción B: mover todos los siguientes (O(n))
//    - Opción C: intercambiar con el último y truncar (O(1))
```

---

## 6. pread y pwrite — I/O posicional atómico

`pread` y `pwrite` combinan seek + read/write en **una sola syscall atómica**:

```c
#include <unistd.h>

ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);

// Leen/escriben en la posición 'offset' SIN modificar el offset del fd.
```

### ¿Por qué existen si tenemos lseek + read?

```c
// Problema con lseek + read en multithreading:
//   Thread A: lseek(fd, 100, SEEK_SET);  // offset = 100
//   Thread B: lseek(fd, 200, SEEK_SET);  // offset = 200 ← ¡pisó a A!
//   Thread A: read(fd, buf, 10);         // Lee de 200, NO de 100
// El fd comparte el offset entre threads → race condition.

// Solución: pread es atómico
//   Thread A: pread(fd, buf, 10, 100);   // Lee de 100, no modifica offset
//   Thread B: pread(fd, buf, 10, 200);   // Lee de 200, no modifica offset
// Ambos son independientes. Sin race condition.

// Ventajas de pread/pwrite:
// 1. Thread-safe: no modifica estado compartido
// 2. Una syscall en vez de dos (lseek + read)
// 3. No necesita save/restore de posición
```

### Ejemplo: leer registro con pread

```c
int read_record_safe(int fd, int index, struct Record *rec) {
    off_t offset = (off_t)index * (off_t)sizeof(struct Record);
    ssize_t n = pread(fd, rec, sizeof(struct Record), offset);
    return (n == (ssize_t)sizeof(struct Record)) ? 0 : -1;
}
// Más simple Y más seguro que la versión con lseek.
```

---

## 7. Sparse files — Archivos con huecos

Un **sparse file** es un archivo cuyo tamaño lógico es mayor que el espacio que realmente ocupa en disco. Los "huecos" (holes) son regiones que nunca se escribieron y se leen como ceros.

```c
// Crear un sparse file:
int fd = open("sparse.dat", O_WRONLY | O_CREAT | O_TRUNC, 0644);

write(fd, "START", 5);                    // Bytes 0-4: datos reales

lseek(fd, 50 * 1024 * 1024, SEEK_SET);   // Saltar a 50 MB
write(fd, "MIDDLE", 6);                   // Bytes 50MB-50MB+5: datos reales

lseek(fd, 100 * 1024 * 1024 - 3, SEEK_SET);
write(fd, "END", 3);                      // Últimos 3 bytes: datos reales

close(fd);

// Resultado:
//   Tamaño lógico (ls -l): ~100 MB
//   Espacio real (du -h):  ~8 KB (solo 3 bloques con datos)
//   Los ~100 MB de huecos se leen como '\0' sin ocupar disco
```

### Verificar si un archivo es sparse

```bash
# ls muestra tamaño lógico, du muestra espacio real:
$ ls -lh sparse.dat
-rw-r--r-- 1 user group 100M ... sparse.dat

$ du -h sparse.dat
8.0K    sparse.dat

# stat muestra ambos:
$ stat sparse.dat
  Size: 104857600    Blocks: 16    IO Block: 4096
# 16 bloques × 512 bytes = 8192 bytes (8 KB)
# Si Size >> Blocks × 512, el archivo es sparse
```

### Usos reales de sparse files

| Uso | Por qué sparse |
|-----|----------------|
| Bases de datos (SQLite, PostgreSQL) | Preasignan espacio que llenan gradualmente |
| Imágenes de disco (qcow2, vmdk) | Disco virtual de 50 GB que solo usa 5 GB |
| Core dumps | Mapean todo el espacio de direcciones del proceso |
| Archivos de swap | Regiones reservadas pero no escritas |
| Torrents en descarga | Archivo final preasignado, bloques llegan desordenados |

### Trampa: copiar sparse files

```bash
# cp por defecto detecta y preserva huecos:
cp --sparse=always sparse.dat copia.dat

# PERO estas herramientas pueden "rellenar" los huecos:
cat sparse.dat > copia.dat    # ¡Ahora copia.dat ocupa 100 MB reales!
scp sparse.dat server:        # scp no preserva sparse por defecto

# rsync sí puede preservar sparse:
rsync --sparse sparse.dat server:
```

---

## 8. SEEK_DATA y SEEK_HOLE (Linux 3.1+)

Extensiones de `lseek` para navegar las regiones de datos y huecos en sparse files:

```c
#ifndef SEEK_DATA
#define SEEK_DATA 3   // Buscar siguiente byte con datos reales
#define SEEK_HOLE 4   // Buscar siguiente hueco
#endif

// SEEK_DATA: desde offset, buscar el primer byte que tiene datos (no hueco)
off_t data_start = lseek(fd, 0, SEEK_DATA);

// SEEK_HOLE: desde offset, buscar el primer byte que es hueco
off_t hole_start = lseek(fd, 0, SEEK_HOLE);
```

### Mapear la estructura de un sparse file

```c
// Iterar todos los segmentos de datos y huecos:
void map_sparse(int fd) {
    off_t pos = 0;
    off_t file_size = lseek(fd, 0, SEEK_END);

    while (pos < file_size) {
        off_t data = lseek(fd, pos, SEEK_DATA);
        if (data == -1) break;   // No más datos (solo huecos)

        off_t hole = lseek(fd, data, SEEK_HOLE);
        if (hole == -1) hole = file_size;

        printf("Data: [%ld, %ld) — %ld bytes\n",
               (long)data, (long)hole, (long)(hole - data));
        pos = hole;
    }
}

// Para nuestro sparse file de 100 MB con "START", "MIDDLE", "END":
// Data: [0, 4096)       — 4096 bytes (bloque con "START")
// Data: [52428800, 52432896) — 4096 bytes (bloque con "MIDDLE")
// Data: [104853504, 104857600) — 4096 bytes (bloque con "END")
//
// Nota: los segmentos tienen tamaño mínimo de un bloque (4096)
// porque el filesystem asigna bloques completos, no bytes individuales.
```

### Uso práctico: copia eficiente de sparse files

```c
// cp --sparse=always internamente hace esto:
// 1. SEEK_DATA para encontrar datos
// 2. read solo los datos reales
// 3. SEEK_HOLE para saber dónde terminan
// 4. lseek en destino para crear el hueco (sin escribir ceros)
// Resultado: la copia preserva los huecos y es mucho más rápida.
```

---

## 9. lseek en recursos no seekable

No todos los file descriptors son seekable. `lseek` falla con `ESPIPE` en recursos secuenciales:

```c
// Recursos NO seekable:
// - Pipes (|)
// - Sockets
// - Terminales (/dev/tty)
// - FIFOs (named pipes)
// - /dev/null, /dev/zero, /dev/urandom

int pipefd[2];
pipe(pipefd);
off_t pos = lseek(pipefd[0], 0, SEEK_CUR);
// pos = -1, errno = ESPIPE ("Illegal seek")
// ESPIPE = Error: SPecial file can't be sEEKed (mnemónico)
```

### Detectar si un fd es seekable

```c
int is_seekable(int fd) {
    return lseek(fd, 0, SEEK_CUR) != (off_t)-1;
}

// Uso: elegir estrategia de lectura
if (is_seekable(fd)) {
    // Puedo saltar a cualquier posición (archivo regular)
    off_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    char *buf = malloc(size);
    read(fd, buf, size);
} else {
    // Debo leer secuencialmente (pipe, socket)
    // Usar buffer dinámico que crece según llegan datos
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        process(buf, n);
    }
}
```

### ¿Por qué no son seekable?

```c
// Un pipe es un stream de datos SIN almacenamiento permanente:
//   - Los datos fluyen del escritor al lector
//   - Una vez leídos, desaparecen del pipe
//   - No hay "inicio" ni "final" para posicionar
//
// Un socket es una conexión de red:
//   - Los datos llegan secuencialmente
//   - No puedes "volver" a datos que ya recibiste
//
// Un terminal es interactivo:
//   - El usuario escribe caracteres que llegan en orden
//   - No hay concepto de "posición" en el flujo

// Esto contrasta con archivos regulares en disco:
//   - Los datos persisten
//   - Puedes ir a cualquier posición y releer
//   - El kernel mantiene el offset porque tiene sentido
```

---

## 10. Comparación con Rust

| Concepto C | Rust | Diferencia clave |
|-----------|------|-----------------|
| `lseek(fd, off, SEEK_SET)` | `file.seek(SeekFrom::Start(off))` | Retorna `Result<u64>` |
| `lseek(fd, off, SEEK_CUR)` | `file.seek(SeekFrom::Current(off))` | off es `i64` (con signo) |
| `lseek(fd, off, SEEK_END)` | `file.seek(SeekFrom::End(off))` | off es `i64` (con signo) |
| `lseek(fd, 0, SEEK_CUR)` (ftell) | `file.stream_position()` | Método dedicado |
| `pread(fd, buf, n, off)` | `file.read_at(off, buf)` (Unix ext.) | `use std::os::unix::fs::FileExt` |

```rust
use std::fs::File;
use std::io::{self, Read, Seek, SeekFrom};

fn read_at_position(path: &str, offset: u64, len: usize) -> io::Result<Vec<u8>> {
    let mut file = File::open(path)?;
    file.seek(SeekFrom::Start(offset))?;    // lseek(fd, offset, SEEK_SET)

    let mut buf = vec![0u8; len];
    file.read_exact(&mut buf)?;              // read_all equivalente
    Ok(buf)
}

// Obtener tamaño sin mover la posición:
fn file_size(file: &mut File) -> io::Result<u64> {
    let saved = file.stream_position()?;     // lseek(fd, 0, SEEK_CUR)
    let size = file.seek(SeekFrom::End(0))?; // lseek(fd, 0, SEEK_END)
    file.seek(SeekFrom::Start(saved))?;      // lseek(fd, saved, SEEK_SET)
    Ok(size)
}

// O más simple con metadata (equivale a fstat):
fn file_size_simple(path: &str) -> io::Result<u64> {
    Ok(std::fs::metadata(path)?.len())
}
```

**Seek es un trait**: cualquier tipo que implemente `Seek` puede usar `seek()`. `File` lo implementa, pero `TcpStream` no (igual que lseek falla con `ESPIPE` en sockets).

```rust
use std::io::Cursor;

// Cursor<Vec<u8>> también implementa Seek:
let mut cursor = Cursor::new(vec![0u8; 100]);
cursor.seek(SeekFrom::Start(50)).unwrap();
// Útil para testing sin archivos reales.
```

---

## Ejercicios

### Ejercicio 1 — Explorar los tres modos de seek

```c
// Crea un archivo con "0123456789ABCDEF" (16 bytes).
// Usando lseek y read (buffer de 4 bytes), lee y muestra:
//   a) Bytes 0-3  (SEEK_SET a 0)
//   b) Bytes 8-11 (SEEK_SET a 8)
//   c) Bytes 4-7  (SEEK_CUR desde posición 12: offset = -8)
//   d) Últimos 4 bytes (SEEK_END con -4)
//
// Después de cada read, imprime la posición actual con lseek(fd, 0, SEEK_CUR).
```

<details><summary>Predicción</summary>

Contenido: `"0123456789ABCDEF"` — cada carácter es un byte.

a) `lseek(fd, 0, SEEK_SET)` → posición 0. `read(fd, buf, 4)` → `"0123"`. Posición después: 4.

b) `lseek(fd, 8, SEEK_SET)` → posición 8. `read(fd, buf, 4)` → `"89AB"`. Posición después: 12.

c) `lseek(fd, -8, SEEK_CUR)` desde posición 12 → posición 4. `read(fd, buf, 4)` → `"4567"`. Posición después: 8.

d) `lseek(fd, -4, SEEK_END)` → posición 16-4 = 12. `read(fd, buf, 4)` → `"CDEF"`. Posición después: 16.

El patrón clave: read siempre avanza el offset. SEEK_CUR es relativo a ese offset avanzado, no al inicio del último read.

</details>

### Ejercicio 2 — Obtener tamaño con y sin mover la posición

```c
// Implementa dos funciones:
//   off_t size_with_lseek(int fd);   // usa lseek (save/seek/restore)
//   off_t size_with_fstat(int fd);   // usa fstat
//
// Para cada una:
// 1. Posiciona en el byte 10 del archivo
// 2. Llama la función
// 3. Verifica que la posición sigue siendo 10
//
// Crea un archivo de prueba con un contenido conocido.
// ¿Cuál de las dos funciones es más sencilla de escribir correctamente?
```

<details><summary>Predicción</summary>

Ambas funciones deben retornar el mismo tamaño. La diferencia:

`size_with_lseek`:
- Debe guardar posición (lseek SEEK_CUR), ir al final (lseek SEEK_END), restaurar (lseek SEEK_SET): 3 syscalls.
- Si olvidas restaurar, la siguiente operación falla silenciosamente.

`size_with_fstat`:
- `fstat(fd, &st)` → `st.st_size`: 1 syscall.
- No modifica la posición en absoluto. Más simple y menos propenso a errores.

Después de ambas llamadas, la posición debe seguir siendo 10. Si `size_with_lseek` está bien implementada, la posición se preserva. `size_with_fstat` nunca la modifica.

`fstat` es más sencilla: una sola línea, sin estado que preservar, imposible romper la posición.

</details>

### Ejercicio 3 — Base de datos de estudiantes

```c
// Implementa un mini sistema de registros con:
//
// struct Student {
//     int id;
//     char name[32];
//     float grades[5];
//     int active;     // 1 = activo, 0 = borrado
// };
//
// Funciones:
//   void add_student(int fd, struct Student *s);
//   int  get_student(int fd, int index, struct Student *s);
//   void update_student(int fd, int index, struct Student *s);
//   void delete_student(int fd, int index);  // marcar active = 0
//   int  count_active(int fd);
//   void list_all(int fd);
//
// Probar: agregar 5 estudiantes, borrar el 2do, listar (debería
// mostrar 4 activos), actualizar el 3ro, verificar que el borrado
// no aparece.
```

<details><summary>Predicción</summary>

`sizeof(struct Student)`: `int`(4) + `char[32]`(32) + `float[5]`(20) + `int`(4) = 60 bytes. Con padding a alineamiento de 4: 60 bytes (ya alineado).

- `add_student`: `lseek(fd, 0, SEEK_END)` → escribir al final
- `get_student`: `lseek(fd, index * sizeof(Student), SEEK_SET)` → leer
- `delete_student`: leer registro, poner `active = 0`, escribir de vuelta en la misma posición
- `count_active`: recorrer todos, contar los que tienen `active == 1`

Después de borrar el índice 1 (0-indexed), `list_all` debería mostrar 4 registros con `active=1` y saltar el borrado. El registro sigue en el archivo pero marcado como inactivo — esto es un "soft delete".

Ventaja del soft delete: no hay que mover registros. Desventaja: el archivo no se reduce y los índices no cambian.

</details>

### Ejercicio 4 — Crear y verificar un sparse file

```c
// Crea un sparse file de 1 GB con datos solo en 3 puntos:
//   Byte 0:    "HEAD" (4 bytes)
//   Byte 500MB: "HALF" (4 bytes)
//   Byte 1GB-4: "TAIL" (4 bytes)
//
// Verificar:
// 1. leer las 3 posiciones y confirmar los datos
// 2. leer 16 bytes de un hueco (offset 1000) y verificar que son \0
// 3. Imprimir tamaño lógico con lseek
// 4. Después de ejecutar, usar du -h y ls -lh para comparar
//
// Pregunta: ¿cuántos bytes de disco usa realmente este archivo?
```

<details><summary>Predicción</summary>

Tamaño lógico: 1 GB = 1073741824 bytes. Al escribir "TAIL" en byte 1073741820 (1GB - 4), el tamaño final es 1073741824 bytes.

Espacio real en disco: solo 3 bloques de datos de 4096 bytes cada uno = 12288 bytes (~12 KB). El filesystem asigna un bloque completo por cada write, aunque solo escribimos 4 bytes.

Los 16 bytes leídos del hueco serán todos `0x00`. El kernel genera estos ceros al vuelo — no están almacenados en disco.

```
$ ls -lh sparse_1gb.dat → 1.0G
$ du -h sparse_1gb.dat  → 12K
```

La relación tamaño lógico / real es ~87000:1. Esto es posible porque ext4/xfs solo asignan bloques para datos escritos explícitamente.

</details>

### Ejercicio 5 — Implementar tail con lseek

```c
// Implementa: ./mytail N archivo
// Muestra las últimas N líneas de un archivo.
//
// Algoritmo:
// 1. Ir al final con lseek(fd, 0, SEEK_END)
// 2. Retroceder byte a byte (o en bloques de 4096) contando '\n'
// 3. Cuando encuentres N+1 newlines, la posición marca el inicio
// 4. Leer desde esa posición hasta el final y escribir a stdout
//
// Probar con:
//   seq 100 > nums.txt
//   ./mytail 5 nums.txt
//   # Debería mostrar: 96 97 98 99 100
//   diff <(./mytail 10 nums.txt) <(tail -10 nums.txt)
```

<details><summary>Predicción</summary>

Hay dos enfoques:

**Enfoque simple (byte a byte)**: `lseek(fd, -1, SEEK_END)`, leer 1 byte, si es `'\n'` incrementar contador. Repetir con `lseek(fd, -2, SEEK_CUR)` (retroceder: -1 del byte leído + -1 para el anterior). Cuando el contador llegue a N+1 (o al inicio del archivo), imprimir desde ahí.

**Enfoque eficiente (bloques)**: leer los últimos 4096 bytes, buscar `'\n'` dentro del bloque. Si no hay suficientes, leer los 4096 anteriores. Mucho menos syscalls.

Para `seq 100 > nums.txt`:
- El archivo tiene 292 bytes (1+1 + 2+1 × 9 + 3+1 × 90 + 4+1 × 1 = 292 bytes)
- Las últimas 5 líneas son "96\n97\n98\n99\n100\n"
- N+1 newlines porque la N-ésima newline marca el *fin* de la línea antes de las últimas N

Caso borde: si el archivo termina en `'\n'` (la mayoría), el primer `'\n'` encontrado desde el final no cuenta como separador de línea — es el terminador de la última línea.

</details>

### Ejercicio 6 — pread vs lseek+read en threads

```c
// Crea un programa con 2 threads que leen del MISMO fd:
//
// Thread A: lee los primeros 100 bytes del archivo
// Thread B: lee los bytes 200-300 del archivo
//
// Versión 1: usar lseek + read (sin mutex)
//   → Demostrar que los datos leídos pueden ser incorrectos
//
// Versión 2: usar pread
//   → Demostrar que siempre es correcto
//
// Crear un archivo de prueba con contenido predecible:
//   python3 -c "print('A'*100 + 'B'*100 + 'C'*100 + 'D'*100)" > test.dat
//
// Thread A debería leer solo 'A's, Thread B solo 'C's.
// Ejecutar 1000 veces y contar errores.
```

<details><summary>Predicción</summary>

**Versión 1 (lseek + read)**:
Thread A hace `lseek(fd, 0, SEEK_SET)` y Thread B hace `lseek(fd, 200, SEEK_SET)`. Si Thread B ejecuta su lseek entre el lseek y el read de Thread A, Thread A leerá desde posición 200 en vez de 0. El resultado son datos incorrectos.

En 1000 ejecuciones, debería haber varias colisiones donde Thread A lee 'C' en vez de 'A', o Thread B lee 'A' en vez de 'C'.

**Versión 2 (pread)**:
`pread(fd, buf, 100, 0)` y `pread(fd, buf, 100, 200)` son completamente independientes. No hay estado compartido (el offset del fd no se modifica). Thread A siempre lee 'A's y Thread B siempre lee 'C's.

0 errores en 1000 ejecuciones.

Nota: necesitas compilar con `-pthread` y usar `<pthread.h>`.

</details>

### Ejercicio 7 — Editor hexadecimal simple

```c
// Implementa un editor hexadecimal que:
// 1. Abre un archivo en modo O_RDWR
// 2. Acepta comandos por stdin:
//    r <offset> <length>   — leer y mostrar en hex
//    w <offset> <hex>      — escribir bytes en hex
//    s                     — mostrar tamaño del archivo
//    q                     — salir
//
// Ejemplo de sesión:
//   $ ./hexedit test.bin
//   > r 0 16
//   0000: 48 65 6c 6c 6f 20 57 6f 72 6c 64 0a 00 00 00 00
//   > w 5 2D
//   Wrote 1 byte at offset 5
//   > r 0 16
//   0000: 48 65 6c 6c 6f 2d 57 6f 72 6c 64 0a 00 00 00 00
//   > q
```

<details><summary>Predicción</summary>

El comando `r 0 16` lee 16 bytes desde offset 0 y los muestra en hexadecimal. "Hello World\n" en hex: `48 65 6c 6c 6f 20 57 6f 72 6c 64 0a`.

El comando `w 5 2D` escribe el byte 0x2D (que es '-') en offset 5, reemplazando el espacio (0x20). Después, leer muestra `6f 2d 57` en vez de `6f 20 57`.

Para implementar:
- `r`: `lseek(fd, offset, SEEK_SET)` + `read(fd, buf, length)` + loop para imprimir cada byte con `printf("%02x ", buf[i])`
- `w`: parsear la cadena hex a bytes, `lseek(fd, offset, SEEK_SET)` + `write(fd, bytes, count)`
- `s`: `lseek(fd, 0, SEEK_END)` retorna el tamaño

Los comandos se leen con `fgets` de stdin. Parsear con `sscanf`.

</details>

### Ejercicio 8 — Medir rendimiento de acceso aleatorio vs secuencial

```c
// Crea un archivo con 10000 registros de tamaño fijo (64 bytes cada uno).
//
// Medir tiempo de:
// a) Lectura secuencial: leer todos los registros del 0 al 9999
// b) Lectura aleatoria: leer 10000 registros en orden aleatorio
//    (generar índices con rand())
//
// Usar clock_gettime(CLOCK_MONOTONIC) para medir.
// Imprimir: "Sequential: X.XXXs   Random: X.XXXs   Ratio: X.Xx"
//
// Pregunta: ¿cuál es más rápido y por qué?
```

<details><summary>Predicción</summary>

Para un archivo de 10000 × 64 = 640000 bytes (~625 KB) en un SSD moderno:

- **Secuencial**: extremadamente rápido. El page cache precarga el archivo completo en memoria. Cada read es un memcpy del kernel, no I/O real. Tiempos esperados: < 5ms.

- **Aleatorio**: también rápido para este tamaño porque el archivo cabe completamente en el page cache (~625 KB). La primera lectura carga todo en cache; las siguientes son memcpys.

El ratio debería ser cercano a 1.0-2.0x porque el archivo es pequeño y cabe en RAM.

Si el archivo fuera de varios GB (más grande que la RAM), la diferencia sería dramática:
- Secuencial: el readahead del kernel predice los siguientes bloques
- Aleatorio: cada acceso puede causar un page fault → I/O real al disco
- En HDD: el ratio podría ser 100-1000x (seeks mecánicos)
- En SSD: el ratio sería ~2-10x (sin seek mecánico pero aún penalización)

</details>

### Ejercicio 9 — Truncar un archivo con ftruncate

```c
// Investiga ftruncate(fd, length) y escribe un programa que:
// 1. Crea un archivo con "ABCDEFGHIJKLMNOPQRSTUVWXYZ" (26 bytes)
// 2. Muestra tamaño: 26 bytes
// 3. Trunca a 10 bytes con ftruncate(fd, 10)
// 4. Muestra tamaño: 10 bytes. Lee y verifica: "ABCDEFGHIJ"
// 5. Extiende a 20 bytes con ftruncate(fd, 20)
// 6. Muestra tamaño: 20 bytes. Lee bytes 10-19 — ¿qué contienen?
//
// Pregunta: ¿ftruncate al extender crea un sparse file?
```

<details><summary>Predicción</summary>

Paso 3: `ftruncate(fd, 10)` corta el archivo a 10 bytes. Los bytes 10-25 desaparecen. Leer retorna "ABCDEFGHIJ". Esta operación es **irreversible** — los datos truncados se pierden.

Paso 5: `ftruncate(fd, 20)` extiende el archivo a 20 bytes. Los bytes 10-19 se rellenan con `'\0'` (ceros). Esto crea un "hueco" similar a un sparse file, pero el comportamiento depende del filesystem:
- En ext4/xfs: los bytes de extensión son ceros y pueden ser sparse (no ocupar bloques)
- En tmpfs: los ceros se almacenan en RAM

Leer bytes 10-19: todos serán `0x00`.

Sí, `ftruncate` al extender puede crear un sparse file (los bytes nuevos son un hueco). Se puede verificar con `du -h` vs `ls -lh`.

`ftruncate` es diferente de escribir ceros manualmente: escribir ceros asigna bloques, ftruncate para extender puede no asignarlos.

</details>

### Ejercicio 10 — Mapa de sparse file con SEEK_DATA/SEEK_HOLE

```c
// Crea un sparse file con un patrón conocido:
//   Offset 0:     "AAAA" (4 bytes de datos)
//   Offset 1MB:   "BBBB" (4 bytes de datos)
//   Offset 5MB:   "CCCC" (4 bytes de datos)
//   Offset 10MB:  "DDDD" (4 bytes de datos, final del archivo)
//
// Implementa map_segments(int fd) que use SEEK_DATA y SEEK_HOLE
// para encontrar todos los segmentos de datos e imprima:
//   Segment 1: data at [0, 4096)         — 4096 bytes
//   Segment 2: data at [1048576, 1052672) — 4096 bytes
//   Segment 3: data at [5242880, 5246976) — 4096 bytes
//   Segment 4: data at [10485760, 10489860) — 4096 bytes
//
// Verifica con: filefrag -v sparse_map.dat
//
// Pregunta: ¿por qué los segmentos de datos son de 4096 bytes
// aunque solo escribiste 4 bytes?
```

<details><summary>Predicción</summary>

Los segmentos de datos reportados por SEEK_DATA/SEEK_HOLE tienen tamaño mínimo de **4096 bytes** (un bloque del filesystem) porque:

1. El filesystem (ext4/xfs) asigna espacio en **bloques**, no en bytes
2. Escribir 4 bytes asigna un bloque completo de 4096 bytes
3. SEEK_DATA/SEEK_HOLE operan a nivel de bloques, no de bytes

Los offsets exactos de inicio/fin pueden variar ligeramente dependiendo del filesystem y su block size. Los segmentos reflejan los bloques físicamente asignados.

`filefrag -v` muestra la lista de extents (bloques físicos asignados) y confirma que solo hay 4 extents para todo el archivo de 10 MB+.

El archivo mide ~10 MB lógicamente pero solo ocupa ~16 KB en disco (4 bloques × 4096 bytes).

Nota: SEEK_DATA y SEEK_HOLE no están disponibles en todos los filesystems. En tmpfs y NFS antiguos, SEEK_HOLE puede devolver siempre el final del archivo (considerando todo como datos).

</details>
