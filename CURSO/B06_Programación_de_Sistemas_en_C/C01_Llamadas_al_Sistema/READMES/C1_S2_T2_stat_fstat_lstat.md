# stat, fstat, lstat — Metadatos de Archivos

## Índice

1. [struct stat: el registro completo de un archivo](#1-struct-stat-el-registro-completo-de-un-archivo)
2. [Las tres variantes: stat, fstat, lstat](#2-las-tres-variantes-stat-fstat-lstat)
3. [Tipo de archivo: st_mode y las macros S_IS*](#3-tipo-de-archivo-st_mode-y-las-macros-s_is)
4. [Permisos: bits, macros y representación simbólica](#4-permisos-bits-macros-y-representación-simbólica)
5. [Bits especiales: setuid, setgid, sticky](#5-bits-especiales-setuid-setgid-sticky)
6. [Timestamps: atime, mtime, ctime](#6-timestamps-atime-mtime-ctime)
7. [Tamaño, bloques y enlaces](#7-tamaño-bloques-y-enlaces)
8. [Identificación: dispositivo e inodo](#8-identificación-dispositivo-e-inodo)
9. [Patrones prácticos con stat](#9-patrones-prácticos-con-stat)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. struct stat: el registro completo de un archivo

Cada archivo en un sistema de archivos Linux tiene un **inodo** que almacena
todos sus metadatos. La syscall `stat` copia esos metadatos a una estructura
en espacio de usuario:

```
  Espacio de usuario                    Kernel (VFS → filesystem)
  ┌─────────────────┐                  ┌────────────────────┐
  │ struct stat buf  │◄── stat() ──────│  inodo en disco    │
  │                  │                  │                    │
  │ st_mode          │                  │ tipo + permisos    │
  │ st_ino           │                  │ número de inodo    │
  │ st_dev           │                  │ dispositivo        │
  │ st_nlink         │                  │ conteo de enlaces  │
  │ st_uid, st_gid   │                  │ propietario/grupo  │
  │ st_size          │                  │ tamaño en bytes    │
  │ st_blocks        │                  │ bloques asignados  │
  │ st_atime         │                  │ último acceso      │
  │ st_mtime         │                  │ última modificación│
  │ st_ctime         │                  │ último cambio      │
  └─────────────────┘                  └────────────────────┘
```

### Definición completa

```c
#include <sys/stat.h>

struct stat {
    dev_t     st_dev;      // dispositivo que contiene el archivo
    ino_t     st_ino;      // número de inodo
    mode_t    st_mode;     // tipo de archivo + permisos
    nlink_t   st_nlink;    // número de hard links
    uid_t     st_uid;      // UID del propietario
    gid_t     st_gid;      // GID del grupo
    dev_t     st_rdev;     // dispositivo (si es archivo especial)
    off_t     st_size;     // tamaño total en bytes
    blksize_t st_blksize;  // tamaño de bloque óptimo para I/O
    blkcnt_t  st_blocks;   // bloques de 512 bytes asignados

    // timestamps con precisión de nanosegundos
    struct timespec st_atim;  // último acceso
    struct timespec st_mtim;  // última modificación de datos
    struct timespec st_ctim;  // último cambio de metadatos
};

// Macros de compatibilidad (POSIX)
#define st_atime  st_atim.tv_sec
#define st_mtime  st_mtim.tv_sec
#define st_ctime  st_ctim.tv_sec
```

> **Nota**: `st_atim` (con `m`) es la estructura `timespec` con nanosegundos.
> `st_atime` (sin `m`) es la macro que extrae solo los segundos. Linux expone
> ambas; el código portable debe usar las macros `st_atime/st_mtime/st_ctime`.

---

## 2. Las tres variantes: stat, fstat, lstat

```c
#include <sys/stat.h>

int stat(const char *pathname, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);
int lstat(const char *pathname, struct stat *statbuf);
```

Las tres llenan la misma `struct stat`, pero difieren en **cómo identifican
el archivo** y **cómo tratan los symlinks**:

```
                    ¿Argumento?     ¿Sigue symlinks?
  ┌──────────┬──────────────────┬───────────────────┐
  │  stat()  │  pathname        │  SÍ (resuelve)    │
  │  fstat() │  fd              │  N/A (ya abierto) │
  │  lstat() │  pathname        │  NO (el symlink)  │
  └──────────┴──────────────────┴───────────────────┘
```

### stat: la más común

```c
struct stat sb;
if (stat("/etc/passwd", &sb) == -1) {
    perror("stat");
    return -1;
}
printf("Tamaño: %ld bytes\n", (long)sb.st_size);
```

Si `/etc/passwd` fuera un symlink, `stat` seguiría el enlace y reportaría
los metadatos del archivo **destino**.

### fstat: sobre un descriptor abierto

```c
int fd = open("/etc/passwd", O_RDONLY);
if (fd == -1) { perror("open"); return -1; }

struct stat sb;
if (fstat(fd, &sb) == -1) {
    perror("fstat");
    close(fd);
    return -1;
}
printf("Inodo: %lu\n", (unsigned long)sb.st_ino);
close(fd);
```

**Ventajas de fstat**:
- **Sin TOCTOU**: el fd ya apunta al archivo; no hay ventana entre verificar
  y usar donde otro proceso pueda reemplazar el archivo.
- **Sin resolución de ruta**: más rápido (no recorre directorios).
- **Sin errores de permisos de directorio**: solo necesitas el fd válido.

### lstat: inspeccionar el symlink mismo

```c
struct stat sb;
if (lstat("/usr/bin/python3", &sb) == -1) {
    perror("lstat");
    return -1;
}

if (S_ISLNK(sb.st_mode)) {
    printf("Es un symlink de %ld bytes\n", (long)sb.st_size);
    // st_size de un symlink = longitud de la ruta destino
}
```

### Diagrama de resolución

```
  /tmp/link → /tmp/real_file

  stat("/tmp/link")    →  metadatos de /tmp/real_file
  lstat("/tmp/link")   →  metadatos del symlink /tmp/link

  fd = open("/tmp/link", O_RDONLY);   // abre real_file
  fstat(fd)            →  metadatos de /tmp/real_file

  fd = open("/tmp/link", O_RDONLY | O_NOFOLLOW);  // falla con ELOOP
```

### fstatat: variante moderna

```c
#include <fcntl.h>
#include <sys/stat.h>

int fstatat(int dirfd, const char *pathname,
            struct stat *statbuf, int flags);
```

Combina las tres en una sola syscall:

```c
// Equivalente a stat():
fstatat(AT_FDCWD, "/etc/passwd", &sb, 0);

// Equivalente a lstat():
fstatat(AT_FDCWD, "/tmp/link", &sb, AT_SYMLINK_NOFOLLOW);

// Relativo a un directorio abierto (evita TOCTOU en rutas):
int dirfd = open("/etc", O_RDONLY | O_DIRECTORY);
fstatat(dirfd, "passwd", &sb, 0);
close(dirfd);
```

`fstatat` es la syscall real en Linux moderno; `stat`, `fstat` y `lstat`
son wrappers de glibc que la invocan internamente.

---

## 3. Tipo de archivo: st_mode y las macros S_IS*

El campo `st_mode` codifica **dos cosas** en un solo valor de 16 bits:

```
  st_mode (16 bits)
  ┌────────┬─────────┬──────────┬───────────┐
  │ tipo   │ setuid  │ permisos │ permisos  │
  │ (4 b)  │ sgid    │ owner    │ group+oth │
  │        │ sticky  │ rwx      │ rwx rwx   │
  │ TTTT   │ SSS     │ RWX      │ RWX RWX   │
  └────────┴─────────┴──────────┴───────────┘
   bits:
   15-12    11-9      8-6        5-0
```

### Macros de tipo (S_IS*)

La forma correcta de extraer el tipo es usando las macros POSIX:

```c
#include <sys/stat.h>

if (S_ISREG(sb.st_mode))  printf("Archivo regular\n");
if (S_ISDIR(sb.st_mode))  printf("Directorio\n");
if (S_ISLNK(sb.st_mode))  printf("Symlink\n");
if (S_ISCHR(sb.st_mode))  printf("Dispositivo de caracteres\n");
if (S_ISBLK(sb.st_mode))  printf("Dispositivo de bloques\n");
if (S_ISFIFO(sb.st_mode)) printf("FIFO (named pipe)\n");
if (S_ISSOCK(sb.st_mode)) printf("Socket Unix\n");
```

### Tabla de tipos y constantes

```
  Macro        Constante    Octal     Ejemplo típico
  ─────────────────────────────────────────────────────
  S_ISREG()    S_IFREG      0100000   /etc/passwd
  S_ISDIR()    S_IFDIR      0040000   /home/user
  S_ISLNK()    S_IFLNK      0120000   /usr/bin/python3
  S_ISCHR()    S_IFCHR      0020000   /dev/tty, /dev/null
  S_ISBLK()    S_IFBLK      0060000   /dev/sda
  S_ISFIFO()   S_IFIFO      0010000   pipe creado con mkfifo
  S_ISSOCK()   S_IFSOCK     0140000   /var/run/docker.sock
```

### Extraer el tipo con máscara

Las macros son la forma preferida, pero internamente hacen esto:

```c
// Lo que S_ISREG(m) hace internamente:
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)

// S_IFMT es la máscara: 0170000 (bits 15-12)
```

### Ejemplo: identificador de tipo completo

```c
const char *file_type_str(mode_t mode) {
    if (S_ISREG(mode))  return "regular";
    if (S_ISDIR(mode))  return "directory";
    if (S_ISLNK(mode))  return "symlink";
    if (S_ISCHR(mode))  return "char device";
    if (S_ISBLK(mode))  return "block device";
    if (S_ISFIFO(mode)) return "FIFO";
    if (S_ISSOCK(mode)) return "socket";
    return "unknown";
}
```

> **Importante**: `S_ISLNK` solo dará verdadero si usaste `lstat`. Con `stat`,
> los symlinks se resuelven y el tipo reportado es el del archivo destino.

---

## 4. Permisos: bits, macros y representación simbólica

Los bits 0-8 de `st_mode` codifican los permisos rwx para owner, group y others:

```
  Macro       Valor    Descripción
  ───────────────────────────────────────────
  S_IRUSR     0400     lectura para owner
  S_IWUSR     0200     escritura para owner
  S_IXUSR     0100     ejecución para owner
  S_IRGRP     0040     lectura para group
  S_IWGRP     0020     escritura para group
  S_IXGRP     0010     ejecución para group
  S_IROTH     0004     lectura para others
  S_IWOTH     0002     escritura para others
  S_IXOTH     0001     ejecución para others
```

### Verificar un permiso específico

```c
// ¿El owner puede escribir?
if (sb.st_mode & S_IWUSR) {
    printf("Owner tiene permiso de escritura\n");
}

// ¿Others pueden leer?
if (sb.st_mode & S_IROTH) {
    printf("El archivo es legible por todos\n");
}
```

### Generar la cadena rwx (como ls -l)

```c
void mode_to_string(mode_t mode, char *str) {
    // str debe tener al menos 11 bytes

    // Tipo
    if (S_ISREG(mode))       str[0] = '-';
    else if (S_ISDIR(mode))  str[0] = 'd';
    else if (S_ISLNK(mode))  str[0] = 'l';
    else if (S_ISCHR(mode))  str[0] = 'c';
    else if (S_ISBLK(mode))  str[0] = 'b';
    else if (S_ISFIFO(mode)) str[0] = 'p';
    else if (S_ISSOCK(mode)) str[0] = 's';
    else                     str[0] = '?';

    // Owner
    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    str[3] = (mode & S_IXUSR) ? 'x' : '-';

    // Group
    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    str[6] = (mode & S_IXGRP) ? 'x' : '-';

    // Others
    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    str[9] = (mode & S_IXOTH) ? 'x' : '-';

    // Bits especiales (sobreescriben x)
    if (mode & S_ISUID) str[3] = (str[3] == 'x') ? 's' : 'S';
    if (mode & S_ISGID) str[6] = (str[6] == 'x') ? 's' : 'S';
    if (mode & S_ISVTX) str[9] = (str[9] == 'x') ? 't' : 'T';

    str[10] = '\0';
}

// Uso:
char perms[11];
mode_to_string(sb.st_mode, perms);
printf("%s\n", perms);  // e.g. "-rwxr-xr-x"
```

### access(): verificar permisos efectivos

`stat` lee los bits crudos. Para saber si el **proceso actual** tiene
permiso (considerando UID efectivo, grupos suplementarios, ACLs), usa
`access()`:

```c
#include <unistd.h>

if (access("/etc/shadow", R_OK) == -1) {
    // No puedo leerlo (probablemente no soy root)
    perror("access");
} else {
    printf("Tengo permiso de lectura\n");
}

// Constantes: R_OK, W_OK, X_OK, F_OK (solo existencia)
```

> **Advertencia**: `access()` usa el UID/GID **real**, no el efectivo.
> Esto lo hace vulnerable a TOCTOU en programas setuid. Para verificar
> con el UID efectivo, usa `faccessat(AT_FDCWD, path, mode, AT_EACCESS)`.

---

## 5. Bits especiales: setuid, setgid, sticky

Tres bits (9-11) tienen significados especiales que van más allá de rwx:

```
  Bit       Macro     Octal    En archivo           En directorio
  ──────────────────────────────────────────────────────────────────
  setuid    S_ISUID   04000    ejecuta como owner   (ignorado)
  setgid    S_ISGID   02000    ejecuta como group   nuevos archivos
                                                     heredan el grupo
  sticky    S_ISVTX   01000    (ignorado)           solo el owner
                                                     puede borrar
```

### Detectar bits especiales

```c
if (sb.st_mode & S_ISUID) {
    printf("SETUID activo — ejecuta como UID %d\n", sb.st_uid);
}

if (sb.st_mode & S_ISGID) {
    if (S_ISDIR(sb.st_mode)) {
        printf("SETGID en directorio — archivos heredan grupo %d\n",
               sb.st_gid);
    } else {
        printf("SETGID en archivo — ejecuta como GID %d\n", sb.st_gid);
    }
}

if (sb.st_mode & S_ISVTX) {
    printf("Sticky bit — solo owner puede borrar (como /tmp)\n");
}
```

### Ejemplo práctico: auditoría de binarios setuid

```c
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>

void scan_setuid(const char *dirpath) {
    DIR *dir = opendir(dirpath);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dirpath, entry->d_name);

        struct stat sb;
        if (stat(path, &sb) == -1) continue;

        if (S_ISREG(sb.st_mode) && (sb.st_mode & S_ISUID)) {
            char perms[11];
            mode_to_string(sb.st_mode, perms);
            printf("%s  %s\n", perms, path);
        }
    }
    closedir(dir);
}
```

---

## 6. Timestamps: atime, mtime, ctime

Cada inodo registra tres marcas de tiempo:

```
  Campo     Nombre completo       Se actualiza cuando...
  ─────────────────────────────────────────────────────────────────
  st_atime  access time           se leen los datos (read, mmap)
  st_mtime  modification time     se modifican los datos (write, truncate)
  st_ctime  change time           se modifican los metadatos (chmod,
                                  chown, link, rename) O los datos
```

### Relación entre timestamps

```
  Operación             atime    mtime    ctime
  ────────────────────────────────────────────────
  read()                  ✓
  write()                          ✓        ✓
  chmod()                                   ✓
  chown()                                   ✓
  link()/unlink()                           ✓
  rename()                                  ✓
  open(O_TRUNC)                    ✓        ✓
  utime()/utimensat()    ✓*       ✓*       ✓

  * = se establecen al valor proporcionado
```

> **Nota sobre ctime**: no es "creation time" (tiempo de creación). Linux
> no registra el tiempo de creación en ext4 clásico. El campo es "change time"
> (cambio de metadatos). ext4 moderno tiene `st_btime` (birth time) pero no
> está expuesto en `struct stat` estándar — requiere `statx()`.

### Precisión de nanosegundos

```c
// Segundos (macro de compatibilidad):
printf("mtime: %ld\n", (long)sb.st_mtime);

// Nanosegundos (estructura timespec):
printf("mtime: %ld.%09ld\n",
       (long)sb.st_mtim.tv_sec,
       sb.st_mtim.tv_nsec);
```

### Opciones de montaje que afectan atime

Actualizar `atime` en cada lectura es costoso en disco. Linux ofrece opciones:

```
  Opción        Comportamiento
  ──────────────────────────────────────────────────────────
  strictatime   actualiza siempre (costoso, casi nunca se usa)
  relatime      actualiza si atime < mtime o si han pasado
                24 horas (default en kernels modernos)
  noatime       nunca actualiza atime (mejor rendimiento)
  lazytime      actualiza en memoria, escribe a disco lazily
```

### Modificar timestamps: utimensat

```c
#include <fcntl.h>
#include <sys/stat.h>

struct timespec times[2];

// times[0] = atime, times[1] = mtime
times[0].tv_nsec = UTIME_OMIT;  // no cambiar atime
times[1].tv_sec = 0;
times[1].tv_nsec = UTIME_NOW;   // mtime = ahora

if (utimensat(AT_FDCWD, "file.txt", times, 0) == -1) {
    perror("utimensat");
}
```

Valores especiales para `tv_nsec`:
- `UTIME_NOW`: establece al tiempo actual.
- `UTIME_OMIT`: no modifica este timestamp.

### Comparar timestamps (¿necesita recompilar?)

```c
int needs_rebuild(const char *source, const char *object) {
    struct stat src_sb, obj_sb;

    if (stat(source, &src_sb) == -1) return 1;
    if (stat(object, &obj_sb) == -1) return 1;  // no existe → compilar

    // ¿El fuente es más nuevo que el objeto?
    if (src_sb.st_mtim.tv_sec > obj_sb.st_mtim.tv_sec)
        return 1;
    if (src_sb.st_mtim.tv_sec == obj_sb.st_mtim.tv_sec &&
        src_sb.st_mtim.tv_nsec > obj_sb.st_mtim.tv_nsec)
        return 1;

    return 0;
}
```

---

## 7. Tamaño, bloques y enlaces

### st_size: tamaño lógico

```
  Tipo de archivo      st_size contiene
  ─────────────────────────────────────────────────
  Archivo regular      bytes de datos
  Symlink              longitud de la ruta destino
  Directorio           depende del filesystem
  Pipe/FIFO            0 (ó datos pendientes)
  Dispositivo          0
```

### st_blocks vs st_size: archivos sparse

```c
struct stat sb;
stat("sparse.img", &sb);

printf("Tamaño lógico:  %ld bytes\n", (long)sb.st_size);
printf("Bloques en disco: %ld (× 512 = %ld bytes)\n",
       (long)sb.st_blocks,
       (long)sb.st_blocks * 512);
```

Un archivo sparse puede tener `st_size = 1GiB` pero `st_blocks = 8`
(solo 4KiB en disco). Esto es fundamental para imágenes de disco qcow2
y bases de datos con preasignación:

```
  Archivo de 1 GiB sparse:

  st_size   = 1073741824 (1 GiB — tamaño lógico)
  st_blocks = 8          (4 KiB — espacio real en disco)

  ┌────┬──────────────────────────────────────────────┐
  │dato│               hueco (hole)                   │
  │4K  │               sin bloques asignados          │
  └────┴──────────────────────────────────────────────┘
  ↑                                                    ↑
  offset 0                                    offset 1GiB
```

### st_blksize: tamaño óptimo de I/O

`st_blksize` no es el tamaño de bloque del filesystem. Es el tamaño
sugerido por el kernel para operaciones eficientes de I/O. Típicamente
4096 para filesystems locales, pero puede ser mayor para NFS:

```c
char *buf = malloc(sb.st_blksize);
ssize_t n;
while ((n = read(fd, buf, sb.st_blksize)) > 0) {
    // I/O óptimo usando el tamaño sugerido
}
```

### st_nlink: conteo de hard links

```c
printf("Hard links: %lu\n", (unsigned long)sb.st_nlink);
```

Para **archivos regulares**, `st_nlink` cuenta cuántas entradas de
directorio apuntan al mismo inodo. Cuando llega a 0, el archivo se borra.

Para **directorios**, el conteo mínimo es 2:
- La entrada en el directorio padre (`nombre/`)
- La entrada `.` dentro del propio directorio

Cada subdirectorio añade 1 (por su `..`):

```
  /home/user/           ← st_nlink = 4
    ├── .               ← +1 (siempre)
    ├── ..              ← apunta al padre
    ├── docs/           ← su ".." suma +1 a user/
    └── code/           ← su ".." suma +1 a user/

  2 (base) + 2 (subdirs) = 4
```

---

## 8. Identificación: dispositivo e inodo

### st_dev y st_ino: identidad única

La combinación `(st_dev, st_ino)` identifica de forma única un archivo
en el sistema:

```c
// ¿Son el mismo archivo? (maneja hard links y symlinks)
int same_file(const char *path1, const char *path2) {
    struct stat sb1, sb2;

    if (stat(path1, &sb1) == -1) return -1;
    if (stat(path2, &sb2) == -1) return -1;

    return (sb1.st_dev == sb2.st_dev && sb1.st_ino == sb2.st_ino);
}
```

Este patrón es lo que `test file1 -ef file2` hace internamente en la shell.

### st_rdev: para dispositivos

Solo tiene significado cuando el archivo es un dispositivo de caracteres
o de bloques. Codifica el número major:minor:

```c
if (S_ISCHR(sb.st_mode) || S_ISBLK(sb.st_mode)) {
    printf("Device: major=%d, minor=%d\n",
           major(sb.st_rdev),
           minor(sb.st_rdev));
}
```

### Detectar cruce de filesystem

```c
// Útil para recorrido recursivo: ¿debo seguir montajes?
void walk(const char *path, dev_t root_dev, int cross_mounts) {
    struct stat sb;
    if (lstat(path, &sb) == -1) return;

    // ¿Cruzamos a otro filesystem?
    if (!cross_mounts && sb.st_dev != root_dev) {
        return;  // no cruzar mount points
    }

    // ... procesar archivo ...
}
```

---

## 9. Patrones prácticos con stat

### Patrón 1: verificar existencia sin abrir

```c
int file_exists(const char *path) {
    struct stat sb;
    return (stat(path, &sb) == 0);
}

int is_directory(const char *path) {
    struct stat sb;
    if (stat(path, &sb) == -1) return 0;
    return S_ISDIR(sb.st_mode);
}

int is_regular_file(const char *path) {
    struct stat sb;
    if (stat(path, &sb) == -1) return 0;
    return S_ISREG(sb.st_mode);
}
```

> **Advertencia TOCTOU**: `if (file_exists(f)) { open(f) }` tiene una
> ventana donde otro proceso puede borrar el archivo. Mejor:
> intenta `open()` directamente y maneja el error `ENOENT`.

### Patrón 2: verificar que un fd apunta a lo que esperas

```c
int verify_regular_file(int fd) {
    struct stat sb;
    if (fstat(fd, &sb) == -1) return -1;

    if (!S_ISREG(sb.st_mode)) {
        fprintf(stderr, "Error: esperaba archivo regular, "
                "obtuve tipo %o\n", sb.st_mode & S_IFMT);
        errno = EINVAL;
        return -1;
    }
    return 0;
}
```

### Patrón 3: obtener tamaño de archivo para malloc

```c
char *read_entire_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) return NULL;

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        return NULL;
    }

    if (!S_ISREG(sb.st_mode)) {
        close(fd);
        errno = EINVAL;
        return NULL;
    }

    char *buf = malloc(sb.st_size + 1);
    if (!buf) {
        close(fd);
        return NULL;
    }

    ssize_t total = 0;
    while (total < sb.st_size) {
        ssize_t n = read(fd, buf + total, sb.st_size - total);
        if (n <= 0) break;
        total += n;
    }
    buf[total] = '\0';

    close(fd);
    return buf;
}
```

### Patrón 4: stat desde la línea de comandos

El comando `stat` en Linux muestra toda la información:

```bash
$ stat /etc/passwd
  File: /etc/passwd
  Size: 2747        Blocks: 8          IO Block: 4096   regular file
Device: 0,38        Inode: 1835029     Links: 1
Access: (0644/-rw-r--r--)  Uid: (    0/    root)   Gid: (    0/    root)
Access: 2026-03-15 10:23:01.000000000 -0300
Modify: 2026-03-10 14:15:22.000000000 -0300
Change: 2026-03-10 14:15:22.000000000 -0300
 Birth: 2026-01-05 08:00:00.000000000 -0300
```

Formatos útiles con `stat -c`:

```bash
stat -c '%s'   file    # solo tamaño
stat -c '%a'   file    # permisos en octal (644)
stat -c '%A'   file    # permisos simbólicos (-rw-r--r--)
stat -c '%U:%G' file   # owner:group por nombre
stat -c '%i'   file    # número de inodo
stat -c '%F'   file    # tipo de archivo en texto
stat -c '%y'   file    # mtime legible
```

### statx(): la versión moderna

Linux 4.11 introdujo `statx()` con campos adicionales y la capacidad de
pedir solo los campos que necesitas:

```c
#include <linux/stat.h>
#include <sys/syscall.h>

struct statx stx;
if (syscall(SYS_statx, AT_FDCWD, "file.txt", 0,
            STATX_BASIC_STATS | STATX_BTIME, &stx) == 0) {
    printf("Birth time: %lld.%u\n",
           (long long)stx.stx_btime.tv_sec,
           stx.stx_btime.tv_nsec);
}
```

Campos exclusivos de `statx`:
- `stx_btime`: tiempo de creación del archivo (birth time).
- `stx_attributes`: flags como `STATX_ATTR_COMPRESSED`, `STATX_ATTR_IMMUTABLE`.
- `stx_mnt_id`: ID del mount point.
- Solicitud selectiva con máscara (evita lecturas innecesarias del inodo).

---

## 10. Errores comunes

### Error 1: usar stat en lugar de lstat para detectar symlinks

```c
// MAL: stat resuelve symlinks, S_ISLNK nunca será verdadero
struct stat sb;
stat("/tmp/link", &sb);
if (S_ISLNK(sb.st_mode)) {  // SIEMPRE falso con stat()
    printf("Es un symlink\n");
}

// BIEN: lstat examina el symlink mismo
lstat("/tmp/link", &sb);
if (S_ISLNK(sb.st_mode)) {
    printf("Es un symlink\n");
}
```

### Error 2: confundir st_size con espacio en disco

```c
// MAL: asumir que st_size refleja el espacio ocupado
off_t disk_usage = sb.st_size;  // INCORRECTO para sparse files

// BIEN: calcular espacio real a partir de st_blocks
off_t disk_usage = sb.st_blocks * 512;  // bloques de 512 bytes siempre
```

`du` muestra el espacio real; `ls -l` muestra `st_size` (tamaño lógico).
Para archivos normales no-sparse ambos coinciden, pero para sparse files
la diferencia puede ser enorme.

### Error 3: TOCTOU con stat seguido de open

```c
// MAL: ventana de race condition
struct stat sb;
if (stat("config.txt", &sb) == 0) {
    // Entre stat() y open(), otro proceso puede:
    // - borrar el archivo
    // - reemplazarlo con un symlink a /etc/shadow
    int fd = open("config.txt", O_RDONLY);
    // ...
}

// BIEN: abrir primero, verificar después
int fd = open("config.txt", O_RDONLY);
if (fd == -1) { perror("open"); return -1; }

struct stat sb;
fstat(fd, &sb);  // verificar sobre el fd ya abierto
if (!S_ISREG(sb.st_mode)) {
    fprintf(stderr, "No es un archivo regular\n");
    close(fd);
    return -1;
}
```

### Error 4: olvidar que st_blocks usa bloques de 512 bytes

```c
// MAL: asumir que st_blocks usa bloques de st_blksize
off_t bytes = sb.st_blocks * sb.st_blksize;  // INCORRECTO

// BIEN: st_blocks siempre usa unidades de 512 bytes
off_t bytes = sb.st_blocks * 512;
```

Esto es así independientemente del tamaño de bloque del filesystem.
Es una convención POSIX, no una propiedad del disco.

### Error 5: confundir ctime con creation time

```c
// MAL: interpretar ctime como "fecha de creación"
printf("Creado: %s", ctime(&sb.st_ctime));  // INCORRECTO

// BIEN: ctime = último cambio de metadatos
printf("Último cambio de metadatos: %s", ctime(&sb.st_ctime));

// Para birth time real, necesitas statx():
// stx.stx_btime (solo en Linux 4.11+, con ext4/xfs)
```

`ctime` se actualiza con `chmod`, `chown`, `link` y cualquier modificación
de datos. No hay forma de "resetearla" al tiempo de creación.

---

## 11. Cheatsheet

```
  OBTENER METADATOS
  ──────────────────────────────────────────────────────────
  stat(path, &sb)       info por ruta (sigue symlinks)
  lstat(path, &sb)      info por ruta (NO sigue symlinks)
  fstat(fd, &sb)        info por descriptor (sin TOCTOU)
  fstatat(dirfd,p,&sb,flags)  moderna, combina las tres

  TIPO (st_mode)
  ──────────────────────────────────────────────────────────
  S_ISREG(m)            archivo regular
  S_ISDIR(m)            directorio
  S_ISLNK(m)            symlink (solo con lstat)
  S_ISCHR(m) / S_ISBLK(m)  dispositivo char/block
  S_ISFIFO(m)           named pipe
  S_ISSOCK(m)           socket Unix

  PERMISOS (st_mode)
  ──────────────────────────────────────────────────────────
  S_IRUSR/S_IWUSR/S_IXUSR   owner rwx
  S_IRGRP/S_IWGRP/S_IXGRP   group rwx
  S_IROTH/S_IWOTH/S_IXOTH   others rwx
  S_ISUID / S_ISGID / S_ISVTX  setuid/setgid/sticky

  CAMPOS CLAVE
  ──────────────────────────────────────────────────────────
  st_size               tamaño lógico en bytes
  st_blocks             bloques de 512 bytes (espacio real)
  st_blksize            tamaño óptimo de I/O
  st_nlink              conteo de hard links
  st_uid / st_gid       owner y grupo (numéricos)
  st_dev / st_ino       identidad única del archivo
  st_rdev               major:minor (solo dispositivos)
  st_atime              último acceso
  st_mtime              última modificación de datos
  st_ctime              último cambio de metadatos

  TIMESTAMPS
  ──────────────────────────────────────────────────────────
  st_atim.tv_nsec       nanosegundos de atime
  utimensat()           modificar timestamps
  UTIME_NOW / UTIME_OMIT   valores especiales

  VERIFICACIÓN DE ACCESO
  ──────────────────────────────────────────────────────────
  access(path, R_OK)    ¿puedo leer? (usa UID real)
  faccessat(...,AT_EACCESS)  usa UID efectivo

  IDENTIDAD DE ARCHIVOS
  ──────────────────────────────────────────────────────────
  (st_dev, st_ino)      identifica unívocamente un archivo
  test f1 -ef f2        equivalente en shell

  LÍNEA DE COMANDOS
  ──────────────────────────────────────────────────────────
  stat file             toda la info
  stat -c '%a' file     permisos en octal
  stat -c '%s' file     tamaño
  stat -c '%F' file     tipo en texto
```

---

## 12. Ejercicios

### Ejercicio 1: mini-ls con metadatos

Escribe un programa que reciba un directorio como argumento y muestre para
cada archivo: permisos en formato simbólico (como `ls -l`), número de
hard links, owner UID, tamaño en bytes y nombre.

```c
// Esqueleto:
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    const char *dirpath = (argc > 1) ? argv[1] : ".";

    DIR *dir = opendir(dirpath);
    if (!dir) { perror("opendir"); return 1; }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Construir ruta completa con snprintf
        // Llamar a lstat (¿por qué lstat y no stat?)
        // Generar cadena de permisos
        // Imprimir con formato alineado
    }
    closedir(dir);
    return 0;
}
```

Requisitos:
- Usar `lstat`, no `stat`.
- Mostrar el tipo de archivo en la primera columna (d, l, -, etc.).
- Ordenar no es necesario.

**Pregunta de reflexión**: ¿por qué este programa debe usar `lstat` en
lugar de `stat`? ¿Qué información se perdería si usaras `stat`? Piensa en
qué pasaría con un symlink roto (cuyo destino no existe).

---

### Ejercicio 2: detector de archivos sparse

Escribe un programa que recorra un directorio y detecte archivos sparse,
reportando para cada uno: la ruta, el tamaño lógico (`st_size`), el
espacio real en disco (`st_blocks * 512`) y la proporción.

```c
// Esqueleto:
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

void check_sparse(const char *path) {
    struct stat sb;
    if (stat(path, &sb) == -1) return;
    if (!S_ISREG(sb.st_mode)) return;

    off_t disk_bytes = sb.st_blocks * 512;

    // ¿Es sparse? El espacio en disco < tamaño lógico
    // Cuidado: archivos pequeños pueden ocupar un bloque
    //          mínimo sin ser realmente sparse

    if (disk_bytes < sb.st_size && sb.st_size > 0) {
        double ratio = (double)disk_bytes / sb.st_size * 100.0;
        printf("SPARSE: %s\n", path);
        printf("  Lógico: %ld bytes, Disco: %ld bytes (%.1f%%)\n",
               (long)sb.st_size, (long)disk_bytes, ratio);
    }
}
```

Requisitos:
- Recorrer el directorio de forma recursiva (los subdirectorios también).
- Ignorar symlinks (usar `lstat` para detectarlos, `stat` para los datos).
- Crear un archivo sparse de prueba con: `truncate -s 1G test_sparse.img`

**Pregunta de reflexión**: si copias un archivo sparse con `cp`, ¿el
resultado sigue siendo sparse? ¿Y con `cp --sparse=always`? ¿Qué
implicaciones tiene esto para backups de imágenes de disco virtuales?

---

### Ejercicio 3: monitor de cambios con timestamps

Escribe un programa que reciba una lista de archivos y los monitoree
consultando `mtime` periódicamente. Cuando detecte un cambio, debe
imprimir qué archivo cambió y la diferencia de tiempo.

```c
// Esqueleto:
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define MAX_FILES 64

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s archivo1 [archivo2 ...]\n", argv[0]);
        return 1;
    }

    int nfiles = argc - 1;
    if (nfiles > MAX_FILES) nfiles = MAX_FILES;

    struct timespec last_mtime[MAX_FILES];

    // Obtener mtime inicial de cada archivo
    for (int i = 0; i < nfiles; i++) {
        struct stat sb;
        if (stat(argv[i + 1], &sb) == -1) {
            perror(argv[i + 1]);
            return 1;
        }
        last_mtime[i] = sb.st_mtim;
    }

    printf("Monitoreando %d archivos (Ctrl+C para salir)...\n", nfiles);

    while (1) {
        sleep(1);
        // Comparar mtime actual con el guardado
        // Si cambió, imprimir y actualizar
        // Usar tv_sec Y tv_nsec para precisión
    }

    return 0;
}
```

Requisitos:
- Comparar con precisión de nanosegundos (no solo segundos).
- Si un archivo se borra durante el monitoreo, reportarlo y continuar.
- Mostrar la hora del cambio en formato legible.

**Pregunta de reflexión**: este enfoque de polling es funcional pero
ineficiente. ¿Cuántas syscalls por segundo genera con 100 archivos y
`sleep(1)`? ¿Qué mecanismo del kernel (que veremos en S04) resuelve
esto de forma eficiente? ¿Por qué ese mecanismo es mejor que polling?

---

Creado `T02_stat_fstat_lstat/README.md` (~590 líneas). Cubre:

- **struct stat** completa con diagrama user→kernel
- **Tres variantes** (stat/fstat/lstat) con diferencias de symlink y TOCTOU
- **fstatat** como syscall unificada moderna
- **Tipo de archivo**: macros S_IS*, tabla de constantes, máscara S_IFMT
- **Permisos**: bits rwx, macros, función `mode_to_string` completa, `access()`
- **Bits especiales**: setuid/setgid/sticky con significado en archivo vs directorio
- **Timestamps**: atime/mtime/ctime con tabla de operaciones, `relatime`, `utimensat`, comparación de nanosegundos
- **Tamaño/bloques**: sparse files, st_blocks×512, st_blksize, st_nlink
- **Identidad**: (st_dev, st_ino), st_rdev, cruce de filesystem
- **Patrones prácticos**: existencia, verificación post-open, read_entire_file, statx()
- **5 errores comunes** + cheatsheet + 3 ejercicios con código C