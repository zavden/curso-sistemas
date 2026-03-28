# Directorios — Lectura, Recorrido y Manipulación

## Índice

1. [Qué es un directorio a nivel de kernel](#1-qué-es-un-directorio-a-nivel-de-kernel)
2. [opendir, readdir, closedir: la API POSIX](#2-opendir-readdir-closedir-la-api-posix)
3. [struct dirent: lo que readdir devuelve](#3-struct-dirent-lo-que-readdir-devuelve)
4. [Recorrido recursivo](#4-recorrido-recursivo)
5. [Operaciones de directorio: mkdir, rmdir, rename](#5-operaciones-de-directorio-mkdir-rmdir-rename)
6. [Directorio de trabajo: getcwd, chdir, fchdir](#6-directorio-de-trabajo-getcwd-chdir-fchdir)
7. [Las syscalls directas: getdents64](#7-las-syscalls-directas-getdents64)
8. [nftw: recorrido estandarizado](#8-nftw-recorrido-estandarizado)
9. [Patrones prácticos](#9-patrones-prácticos)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es un directorio a nivel de kernel

Un directorio no es una "carpeta" — es un **archivo especial** cuyo
contenido es una tabla de asociaciones nombre→inodo:

```
  Directorio /home/user/  (inodo 131073)
  ┌──────────────────────────────────────┐
  │  nombre          inodo               │
  │  ────────────────────────────────     │
  │  .               131073  (sí mismo)  │
  │  ..              131072  (padre)     │
  │  .bashrc         131074              │
  │  docs/           131080              │
  │  foto.jpg        131099              │
  └──────────────────────────────────────┘
```

Puntos clave:
- El directorio **no contiene** los datos de los archivos, solo nombres
  y números de inodo.
- `.` y `..` son entradas reales almacenadas en el directorio.
- Un "hard link" es simplemente otra entrada en algún directorio que apunta
  al mismo inodo.
- No puedes leer un directorio con `read()` — el kernel lo prohíbe. Debes
  usar la API de directorios o la syscall `getdents64`.

### Permisos de directorio

Los permisos rwx tienen un significado especial en directorios:

```
  Permiso    En archivo             En directorio
  ───────────────────────────────────────────────────────────
  r (read)   leer contenido         listar nombres (readdir)
  w (write)  modificar contenido    crear/borrar/renombrar
                                    entradas
  x (exec)   ejecutar               acceder/atravesar (lookup)
```

La combinación importa:
- **r sin x**: puedes listar nombres pero NO acceder a los archivos
  (no puedes hacer `stat` de las entradas).
- **x sin r**: puedes acceder a un archivo si conoces su nombre, pero
  NO puedes listar el directorio.
- **w sin x**: inútil (no puedes hacer lookup para crear/borrar).

---

## 2. opendir, readdir, closedir: la API POSIX

```c
#include <dirent.h>

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
```

### Flujo básico

```
  opendir()          readdir()           closedir()
      │                  │                    │
      ▼                  ▼                    ▼
  ┌────────┐      ┌─────────────┐      ┌──────────┐
  │ Obtener│      │ Siguiente   │      │ Liberar  │
  │ DIR*   │─────►│ entrada     │─────►│ recursos │
  │ handle │      │ (o NULL)    │      │          │
  └────────┘      └─────────────┘      └──────────┘
                        │ ▲
                        └─┘ (bucle hasta NULL)
```

### Ejemplo completo

```c
#include <stdio.h>
#include <dirent.h>
#include <errno.h>

int list_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return -1;
    }

    struct dirent *entry;
    errno = 0;  // distinguir fin de error

    while ((entry = readdir(dir)) != NULL) {
        // Saltar . y ..
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;

        printf("%s\n", entry->d_name);
    }

    if (errno != 0) {
        perror("readdir");
        closedir(dir);
        return -1;
    }

    closedir(dir);
    return 0;
}
```

### Distinguir fin de directorio de error

`readdir` devuelve `NULL` tanto al terminar como ante un error. La
convención POSIX:

```c
errno = 0;
while ((entry = readdir(dir)) != NULL) {
    // procesar
}
if (errno != 0) {
    // error real de readdir
}
// si errno == 0, simplemente se acabaron las entradas
```

### fdopendir: desde un descriptor abierto

```c
#include <dirent.h>

int dirfd_val = open("/etc", O_RDONLY | O_DIRECTORY);
DIR *dir = fdopendir(dirfd_val);
// IMPORTANTE: fdopendir toma posesión del fd
// No lo cierres — closedir lo hará
```

Útil para evitar TOCTOU: abres el directorio, verificas con `fstat`,
y luego lo recorres.

### dirfd: obtener el fd de un DIR*

```c
DIR *dir = opendir("/etc");
int fd = dirfd(dir);
// fd válido mientras dir esté abierto
// Útil para fstatat(), openat(), etc.
```

---

## 3. struct dirent: lo que readdir devuelve

```c
struct dirent {
    ino_t          d_ino;       // número de inodo
    off_t          d_off;       // offset al siguiente (opaco)
    unsigned short d_reclen;    // tamaño de este registro
    unsigned char  d_type;      // tipo de archivo (DT_*)
    char           d_name[256]; // nombre (null-terminated)
};
```

### d_type: tipo sin necesidad de stat

```
  Constante   Valor   Equivalente S_IS*
  ─────────────────────────────────────────
  DT_REG        8     S_ISREG   (archivo regular)
  DT_DIR        4     S_ISDIR   (directorio)
  DT_LNK       10     S_ISLNK   (symlink)
  DT_CHR        2     S_ISCHR   (dispositivo char)
  DT_BLK        6     S_ISBLK   (dispositivo block)
  DT_FIFO       1     S_ISFIFO  (pipe)
  DT_SOCK      12     S_ISSOCK  (socket)
  DT_UNKNOWN    0     (no disponible)
```

### Cuidado con DT_UNKNOWN

No todos los filesystems soportan `d_type`. XFS con `ftype=0`, algunos
filesystems de red y filesystems antiguos devuelven `DT_UNKNOWN` para
todo. El código robusto **siempre** debe manejar este caso:

```c
int is_directory_entry(const char *dirpath, struct dirent *entry) {
    // Intento rápido: d_type
    if (entry->d_type == DT_DIR) return 1;
    if (entry->d_type != DT_UNKNOWN) return 0;

    // Fallback: stat (necesario para DT_UNKNOWN)
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dirpath, entry->d_name);
    struct stat sb;
    if (lstat(path, &sb) == -1) return 0;
    return S_ISDIR(sb.st_mode);
}
```

### Macro de conversión DTTOIF/IFTODT

```c
// Convertir d_type a st_mode bits y viceversa:
mode_t mode = DTTOIF(entry->d_type);  // DT_REG → S_IFREG
unsigned char dt = IFTODT(sb.st_mode);  // S_IFREG → DT_REG
```

### Búferes y punteros de readdir

El puntero que devuelve `readdir` apunta a un búfer **interno** del
`DIR*`. Esto significa:

1. **No lo liberes** con `free()`.
2. La siguiente llamada a `readdir` **sobreescribe** el resultado anterior.
3. Si necesitas guardar el nombre, cópialo con `strncpy` o `snprintf`.
4. No es thread-safe entre hilos que comparten el mismo `DIR*`.

---

## 4. Recorrido recursivo

### Implementación manual

```c
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

void walk_recursive(const char *dirpath, int depth) {
    DIR *dir = opendir(dirpath);
    if (!dir) {
        perror(dirpath);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Saltar . y ..
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        char path[PATH_MAX];
        int len = snprintf(path, sizeof(path), "%s/%s",
                           dirpath, entry->d_name);
        if (len >= (int)sizeof(path)) {
            fprintf(stderr, "Ruta demasiado larga: %s/%s\n",
                    dirpath, entry->d_name);
            continue;
        }

        // Indentación visual
        for (int i = 0; i < depth; i++) printf("  ");

        struct stat sb;
        if (lstat(path, &sb) == -1) {
            perror(path);
            continue;
        }

        if (S_ISDIR(sb.st_mode)) {
            printf("[DIR] %s/\n", entry->d_name);
            walk_recursive(path, depth + 1);
        } else if (S_ISLNK(sb.st_mode)) {
            printf("[LNK] %s\n", entry->d_name);
        } else {
            printf("      %s (%ld bytes)\n",
                   entry->d_name, (long)sb.st_size);
        }
    }
    closedir(dir);
}
```

### Problemas del recorrido recursivo

```
  Problema                 Solución
  ────────────────────────────────────────────────────────
  Symlink circular         usar lstat, no seguir DT_LNK
  Profundidad excesiva     límite de profundidad
  Descriptores agotados    cerrar dir antes de recursar
                           (o usar iteración con pila)
  PATH_MAX desbordado      verificar snprintf retorno
  Cruce de filesystem      comparar st_dev
  Permisos denegados       manejar EACCES, continuar
```

### Evitar symlinks circulares

```c
// PELIGRO: si sigues symlinks con stat, puedes entrar en bucle
// /tmp/a → /tmp/b/c
// /tmp/b/c → /tmp/a       ← bucle infinito

// SOLUCIÓN: usar lstat para detectar symlinks y no recursarlos
if (S_ISLNK(sb.st_mode)) {
    // reportar pero NO recursar
    continue;
}
```

### Versión iterativa con pila (evita desbordamiento de stack)

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

struct dir_entry {
    char path[PATH_MAX];
    struct dir_entry *next;
};

void walk_iterative(const char *start) {
    // Pila como lista enlazada
    struct dir_entry *stack = malloc(sizeof(*stack));
    snprintf(stack->path, PATH_MAX, "%s", start);
    stack->next = NULL;

    while (stack) {
        // Pop
        struct dir_entry *current = stack;
        stack = stack->next;

        DIR *dir = opendir(current->path);
        if (!dir) {
            perror(current->path);
            free(current);
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
                continue;

            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s",
                     current->path, entry->d_name);

            struct stat sb;
            if (lstat(path, &sb) == -1) continue;

            if (S_ISDIR(sb.st_mode)) {
                // Push al stack
                struct dir_entry *sub = malloc(sizeof(*sub));
                snprintf(sub->path, PATH_MAX, "%s", path);
                sub->next = stack;
                stack = sub;
            }

            printf("%s\n", path);
        }

        closedir(dir);
        free(current);
    }
}
```

---

## 5. Operaciones de directorio: mkdir, rmdir, rename

### mkdir: crear directorio

```c
#include <sys/stat.h>

// El mode se modifica por umask
if (mkdir("/tmp/mydir", 0755) == -1) {
    perror("mkdir");
}
```

Para crear rutas intermedias (equivalente a `mkdir -p`):

```c
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

int mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) == -1 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    return mkdir(tmp, mode);
}
```

### mkdirat: relativo a un directorio

```c
#include <fcntl.h>
#include <sys/stat.h>

int dirfd = open("/var/lib", O_RDONLY | O_DIRECTORY);
mkdirat(dirfd, "myapp", 0755);   // crea /var/lib/myapp
close(dirfd);
```

### rmdir: eliminar directorio vacío

```c
if (rmdir("/tmp/mydir") == -1) {
    if (errno == ENOTEMPTY) {
        fprintf(stderr, "Directorio no está vacío\n");
    } else {
        perror("rmdir");
    }
}
```

`rmdir` solo funciona con directorios **vacíos** (sin contar `.` y `..`).
Para eliminar un directorio con contenido, debes eliminar su contenido
recursivamente primero.

### Eliminar directorio recursivamente

```c
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

int remove_recursive(const char *path) {
    struct stat sb;
    if (lstat(path, &sb) == -1) return -1;

    // Si no es directorio, borrar directamente
    if (!S_ISDIR(sb.st_mode)) {
        return unlink(path);
    }

    DIR *dir = opendir(path);
    if (!dir) return -1;

    struct dirent *entry;
    int ret = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        char child[PATH_MAX];
        snprintf(child, sizeof(child), "%s/%s",
                 path, entry->d_name);

        if (remove_recursive(child) == -1) {
            ret = -1;
        }
    }

    closedir(dir);

    if (rmdir(path) == -1) ret = -1;
    return ret;
}
```

### rename: mover/renombrar

```c
#include <stdio.h>

if (rename("/tmp/old_name", "/tmp/new_name") == -1) {
    perror("rename");
}
```

Propiedades de `rename`:
- **Atómico** si origen y destino están en el mismo filesystem.
- Si el destino existe y es un archivo, lo reemplaza atómicamente.
- Si el destino existe y es un directorio, falla con `EISDIR` (o lo
  reemplaza si está vacío, depende del sistema).
- **Falla con EXDEV** si origen y destino están en distintos filesystems.
  En ese caso, debes copiar + borrar.

```c
if (rename(src, dst) == -1) {
    if (errno == EXDEV) {
        // Distinto filesystem: copiar + unlink
        copy_file(src, dst);
        unlink(src);
    } else {
        perror("rename");
        return -1;
    }
}
```

### renameat2: con flags (Linux)

```c
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>

// RENAME_EXCHANGE: intercambiar dos archivos atómicamente
syscall(SYS_renameat2, AT_FDCWD, "file_a",
        AT_FDCWD, "file_b", RENAME_EXCHANGE);

// RENAME_NOREPLACE: fallar si destino existe
syscall(SYS_renameat2, AT_FDCWD, "src",
        AT_FDCWD, "dst", RENAME_NOREPLACE);
```

---

## 6. Directorio de trabajo: getcwd, chdir, fchdir

Cada proceso tiene un **directorio de trabajo actual** (CWD) que se
hereda con `fork` y persiste a través de `exec`.

### getcwd: obtener CWD

```c
#include <unistd.h>

char cwd[PATH_MAX];
if (getcwd(cwd, sizeof(cwd)) == NULL) {
    perror("getcwd");
} else {
    printf("CWD: %s\n", cwd);
}
```

Con extensión GNU (búfer dinámico):

```c
char *cwd = getcwd(NULL, 0);  // malloc automático
printf("CWD: %s\n", cwd);
free(cwd);
```

### chdir / fchdir: cambiar CWD

```c
if (chdir("/tmp") == -1) {
    perror("chdir");
}

// O con descriptor (más seguro, evita TOCTOU):
int dirfd = open("/tmp", O_RDONLY | O_DIRECTORY);
if (fchdir(dirfd) == -1) {
    perror("fchdir");
}
close(dirfd);
```

### Patrón: cambiar y restaurar CWD

```c
int saved_fd = open(".", O_RDONLY | O_DIRECTORY);
if (saved_fd == -1) { perror("open ."); return -1; }

// Trabajar en otro directorio
if (chdir("/some/path") == -1) {
    close(saved_fd);
    return -1;
}

// ... hacer trabajo ...

// Restaurar CWD original
fchdir(saved_fd);
close(saved_fd);
```

> **Nota**: `chdir` afecta **todo el proceso**, no solo el hilo actual.
> En programas multihilo, cambiar el CWD afecta a todos los hilos.
> Usa `openat()`, `fstatat()`, etc. en su lugar para operaciones
> relativas a un directorio sin cambiar el CWD global.

---

## 7. Las syscalls directas: getdents64

`readdir` es una función de biblioteca. La syscall real que el kernel
expone es `getdents64`:

```c
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/types.h>

struct linux_dirent64 {
    __u64        d_ino;
    __s64        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];  // flexible array member
};

void list_with_getdents(const char *path) {
    int fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd == -1) { perror("open"); return; }

    char buf[4096];
    int nread;

    while ((nread = syscall(SYS_getdents64, fd, buf, sizeof(buf))) > 0) {
        int offset = 0;
        while (offset < nread) {
            struct linux_dirent64 *d =
                (struct linux_dirent64 *)(buf + offset);
            printf("ino=%llu type=%d name=%s\n",
                   (unsigned long long)d->d_ino,
                   d->d_type, d->d_name);
            offset += d->d_reclen;
        }
    }

    if (nread == -1) perror("getdents64");
    close(fd);
}
```

### ¿Cuándo usar getdents64 directamente?

```
  readdir (glibc)              getdents64 (syscall directa)
  ──────────────────────────────────────────────────────────
  Una entrada a la vez         Lee un bloque completo
  Portable POSIX               Solo Linux
  Más fácil de usar            Más eficiente para dirs
                               con millones de archivos
  Suficiente para casi todo    Necesario en herramientas
                               de alto rendimiento
```

En la práctica, usa `readdir`. Solo necesitas `getdents64` si estás
escribiendo algo como `find` o un indexador de filesystem que recorra
millones de entradas.

---

## 8. nftw: recorrido estandarizado

POSIX proporciona `nftw()` (new file tree walk) como solución estándar
para recorrer árboles de directorios:

```c
#include <ftw.h>

int nftw(const char *dirpath,
         int (*fn)(const char *fpath,
                   const struct stat *sb,
                   int typeflag,
                   struct FTW *ftwbuf),
         int nopenfd,
         int flags);
```

### Parámetros

- `dirpath`: directorio raíz del recorrido.
- `fn`: callback invocado para cada archivo/directorio.
- `nopenfd`: máximo de descriptores a usar simultáneamente.
- `flags`: modificadores de comportamiento.

### Flags de nftw

```
  Flag            Efecto
  ──────────────────────────────────────────────────────
  FTW_PHYS        no seguir symlinks (usar lstat)
  FTW_DEPTH       post-order (hijos antes que padre)
  FTW_MOUNT       no cruzar mount points
  FTW_CHDIR       chdir a cada directorio antes de
                  procesar (afecta todo el proceso)
```

### typeflag en el callback

```
  Valor          Significado
  ──────────────────────────────────────────────────────
  FTW_F          archivo regular
  FTW_D          directorio (pre-order)
  FTW_DP         directorio (post-order, con FTW_DEPTH)
  FTW_SL         symlink (con FTW_PHYS)
  FTW_SLN        symlink roto
  FTW_DNR        directorio no legible
  FTW_NS         stat falló
```

### Ejemplo: calcular tamaño total de un directorio

```c
#include <stdio.h>
#include <ftw.h>

static long long total_size = 0;

int sum_size(const char *fpath, const struct stat *sb,
             int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F) {
        total_size += sb->st_size;
    }
    return 0;  // continuar recorrido
}

int main(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : ".";

    total_size = 0;
    if (nftw(path, sum_size, 20, FTW_PHYS) == -1) {
        perror("nftw");
        return 1;
    }

    printf("Tamaño total: %lld bytes (%.2f MiB)\n",
           total_size, (double)total_size / (1024 * 1024));
    return 0;
}
```

### Ejemplo: eliminar recursivamente con nftw

```c
#include <stdio.h>
#include <unistd.h>
#include <ftw.h>

int remove_entry(const char *fpath, const struct stat *sb,
                 int typeflag, struct FTW *ftwbuf) {
    (void)sb; (void)ftwbuf;

    if (typeflag == FTW_DP) {
        return rmdir(fpath) == -1 ? -1 : 0;
    }
    return unlink(fpath) == -1 ? -1 : 0;
}

int remove_tree(const char *path) {
    // FTW_DEPTH: procesar hijos antes que padres
    // FTW_PHYS: no seguir symlinks
    return nftw(path, remove_entry, 64, FTW_DEPTH | FTW_PHYS);
}
```

### Limitaciones de nftw

- **Variable global obligada**: el callback no recibe contexto del
  usuario. Debes usar variables globales o thread-local.
- **No se puede cancelar parcialmente**: si el callback devuelve != 0,
  `nftw` se detiene completamente (no puede "saltar" un subárbol).
- **No es thread-safe**: usa `chdir` internamente con `FTW_CHDIR`.

Para recorridos complejos (filtros, cancelación parcial, estado per-hilo),
la implementación manual con `opendir`/`readdir` es mejor.

---

## 9. Patrones prácticos

### Patrón 1: buscar archivos por extensión

```c
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>

void find_by_extension(const char *dirpath, const char *ext) {
    DIR *dir = opendir(dirpath);
    if (!dir) return;

    size_t ext_len = strlen(ext);
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        size_t name_len = strlen(entry->d_name);
        if (name_len > ext_len &&
            strcmp(entry->d_name + name_len - ext_len, ext) == 0) {
            printf("%s/%s\n", dirpath, entry->d_name);
        }
    }
    closedir(dir);
}

// Uso: find_by_extension("/home/user/src", ".c");
```

### Patrón 2: contar archivos en un directorio

```c
long count_entries(const char *dirpath) {
    DIR *dir = opendir(dirpath);
    if (!dir) return -1;

    long count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            continue;
        count++;
    }
    closedir(dir);
    return count;
}
```

### Patrón 3: operaciones seguras con openat

Las funciones `*at()` permiten operaciones relativas a un directorio
abierto, eliminando race conditions de ruta:

```c
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

void process_config_dir(const char *dirpath) {
    int dirfd = open(dirpath, O_RDONLY | O_DIRECTORY);
    if (dirfd == -1) { perror("open dir"); return; }

    // Todas las operaciones son relativas a dirfd
    // No importa si alguien renombra el directorio
    struct stat sb;
    fstatat(dirfd, "config.ini", &sb, 0);

    int fd = openat(dirfd, "config.ini", O_RDONLY);
    // ...

    mkdirat(dirfd, "backups", 0755);
    renameat(dirfd, "config.ini", dirfd, "config.ini.bak");

    close(fd);
    close(dirfd);
}
```

### Patrón 4: realpath para obtener ruta absoluta

```c
#include <stdlib.h>

char *abs = realpath("../src/../docs/./readme.md", NULL);
if (abs) {
    printf("Ruta canónica: %s\n", abs);
    // e.g. "/home/user/docs/readme.md"
    free(abs);
} else {
    perror("realpath");
    // Falla si algún componente no existe
}
```

`realpath` resuelve `.`, `..`, symlinks y componentes duplicados de `/`.
Falla si el archivo o cualquier directorio intermedio no existe.

---

## 10. Errores comunes

### Error 1: no manejar DT_UNKNOWN en d_type

```c
// MAL: asumir que d_type siempre tiene un valor útil
while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_DIR) {
        walk_recursive(path);  // pierde directorios en XFS ftype=0
    }
}

// BIEN: fallback a lstat cuando d_type es desconocido
while ((entry = readdir(dir)) != NULL) {
    int is_dir = 0;

    if (entry->d_type == DT_DIR) {
        is_dir = 1;
    } else if (entry->d_type == DT_UNKNOWN) {
        struct stat sb;
        if (lstat(path, &sb) == 0 && S_ISDIR(sb.st_mode))
            is_dir = 1;
    }

    if (is_dir) walk_recursive(path);
}
```

### Error 2: construir rutas sin verificar desbordamiento

```c
// MAL: buffer overflow si dirpath + d_name > PATH_MAX
char path[PATH_MAX];
sprintf(path, "%s/%s", dirpath, entry->d_name);

// BIEN: verificar retorno de snprintf
char path[PATH_MAX];
int len = snprintf(path, sizeof(path), "%s/%s",
                   dirpath, entry->d_name);
if (len >= (int)sizeof(path)) {
    fprintf(stderr, "Ruta truncada: %s/%s\n",
            dirpath, entry->d_name);
    continue;
}
```

### Error 3: seguir symlinks en recorrido recursivo

```c
// MAL: stat sigue symlinks → bucle infinito posible
stat(path, &sb);
if (S_ISDIR(sb.st_mode)) {
    walk_recursive(path);  // si path es symlink a ancestro → infinito
}

// BIEN: lstat detecta el symlink, evita recursión
lstat(path, &sb);
if (S_ISDIR(sb.st_mode)) {
    // Solo directorios reales, no symlinks a directorios
    walk_recursive(path);
}
```

### Error 4: olvidar closedir

```c
// MAL: leak de descriptores en recorrido profundo
void walk(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // ...
        walk(child_path);  // abre más DIRs sin cerrar la actual
    }
    // ¡closedir falta! Cada nivel de recursión pierde un fd
}

// BIEN: siempre cerrar
void walk(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // ...
        walk(child_path);
    }
    closedir(dir);  // liberar fd en cada nivel
}
```

Con recursión profunda, cada nivel mantiene un `DIR*` abierto. Si
alcanzas el límite de descriptores (`EMFILE`), `opendir` falla. El
parámetro `nopenfd` de `nftw` existe precisamente para controlar esto.

### Error 5: asumir orden de readdir

```c
// MAL: esperar que readdir devuelva entradas ordenadas
while ((entry = readdir(dir)) != NULL) {
    printf("%s\n", entry->d_name);
    // El orden NO está garantizado por POSIX
    // Puede variar entre filesystems y ejecuciones
}

// BIEN: si necesitas orden, almacena y ordena
// O usa scandir() que tiene soporte nativo de ordenamiento:
struct dirent **namelist;
int n = scandir(dirpath, &namelist, NULL, alphasort);
for (int i = 0; i < n; i++) {
    printf("%s\n", namelist[i]->d_name);
    free(namelist[i]);
}
free(namelist);
```

---

## 11. Cheatsheet

```
  ABRIR / LEER / CERRAR DIRECTORIO
  ──────────────────────────────────────────────────────────
  opendir(path)              abrir directorio por ruta
  fdopendir(fd)              abrir directorio por fd
  readdir(dir)               siguiente entrada (o NULL)
  closedir(dir)              cerrar y liberar
  dirfd(dir)                 obtener fd del DIR*
  rewinddir(dir)             volver al inicio
  seekdir(dir, pos)          ir a posición (opaco)
  telldir(dir)               posición actual (opaco)

  CAMPOS DE struct dirent
  ──────────────────────────────────────────────────────────
  d_name[]                   nombre del archivo
  d_ino                      número de inodo
  d_type                     DT_REG/DT_DIR/DT_LNK/...
  d_type puede ser DT_UNKNOWN → usar lstat como fallback

  CREAR / BORRAR / RENOMBRAR
  ──────────────────────────────────────────────────────────
  mkdir(path, mode)          crear directorio
  mkdirat(dirfd, path, mode) crear relativo a dirfd
  rmdir(path)                borrar directorio vacío
  unlink(path)               borrar archivo
  rename(old, new)           renombrar (atómico en mismo fs)
  renameat2(...)             con RENAME_EXCHANGE/NOREPLACE

  DIRECTORIO DE TRABAJO
  ──────────────────────────────────────────────────────────
  getcwd(buf, size)          obtener CWD
  chdir(path)                cambiar CWD
  fchdir(fd)                 cambiar CWD por fd

  RECORRIDO RECURSIVO
  ──────────────────────────────────────────────────────────
  nftw(path, fn, nfd, flags) recorrido estándar POSIX
  FTW_PHYS                   no seguir symlinks
  FTW_DEPTH                  post-order (hijos primero)
  FTW_MOUNT                  no cruzar mount points

  UTILIDADES
  ──────────────────────────────────────────────────────────
  scandir(dir, &list, filter, compar)  leer + filtrar + ordenar
  alphasort                  comparador alfabético
  realpath(path, resolved)   ruta canónica absoluta

  OPERACIONES RELATIVAS (*at)
  ──────────────────────────────────────────────────────────
  openat(dirfd, path, flags)      open relativo
  fstatat(dirfd, path, &sb, fl)   stat relativo
  mkdirat(dirfd, path, mode)      mkdir relativo
  renameat(ofd, old, nfd, new)    rename relativo
  AT_FDCWD                        usa CWD (compatibilidad)
```

---

## 12. Ejercicios

### Ejercicio 1: árbol de directorios con estadísticas

Escribe un programa que reciba una ruta como argumento y muestre el árbol
de directorios con indentación visual (como `tree`), mostrando para cada
archivo: tipo (`[DIR]`, `[LNK]`, `[REG]`), permisos en octal y tamaño.

```c
// Esqueleto:
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

void print_tree(const char *path, int depth, dev_t root_dev) {
    DIR *dir = opendir(path);
    if (!dir) { perror(path); return; }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Saltar . y ..
        // Construir ruta completa
        // lstat para metadatos
        // Imprimir con indentación (2 espacios × depth)
        // Si es directorio (y no symlink), recursar
        // Si es directorio en otro filesystem (st_dev), ¿recursar?
    }
    closedir(dir);
}
```

Requisitos:
- Usar `lstat` (no `stat`).
- No seguir symlinks — mostrar `[LNK] → destino` usando `readlink`.
- No cruzar mount points (comparar `st_dev`).
- Manejar `DT_UNKNOWN` correctamente.

**Pregunta de reflexión**: ¿cuántos descriptores de archivo tiene abiertos
tu programa en el punto más profundo de la recursión? Si tienes un árbol
con 500 niveles de profundidad, ¿qué pasaría? ¿Cómo lo resolverías sin
cambiar a una versión iterativa?

---

### Ejercicio 2: find simplificado

Implementa un programa que busque archivos en un árbol de directorios
con filtros similares a `find`:

```c
// Uso: ./myfind /ruta -name "*.c" -type f -maxdepth 3
```

```c
// Esqueleto:
struct find_options {
    const char *name_pattern;  // NULL = sin filtro
    char type_filter;          // 'f', 'd', 'l', '\0' = todos
    int max_depth;             // -1 = sin límite
};

void myfind(const char *path, struct find_options *opts, int depth) {
    if (opts->max_depth >= 0 && depth > opts->max_depth)
        return;

    // Abrir directorio
    // Para cada entrada:
    //   - Construir ruta completa
    //   - lstat
    //   - Verificar filtro de tipo
    //   - Verificar filtro de nombre (fnmatch)
    //   - Si pasa los filtros, imprimir
    //   - Si es directorio, recursar
}
```

Requisitos:
- Usar `fnmatch()` para el matching de nombres (soporta `*`, `?`, `[a-z]`).
- Filtro de tipo: `f` (regular), `d` (directorio), `l` (symlink).
- `-maxdepth` para limitar la profundidad.
- Parsear argumentos de `argv` en `main`.

**Pregunta de reflexión**: el comando `find` real en GNU/Linux tiene la
opción `-xdev` para no cruzar filesystems. ¿Cómo implementarías esto
usando los campos de `struct stat`? ¿Por qué esta opción es importante
cuando se ejecuta `find /` (buscar desde la raíz)?

---

### Ejercicio 3: detector de hard links

Escribe un programa que recorra un directorio recursivamente y detecte
archivos que comparten el mismo inodo (hard links). Debe agrupar los
archivos por `(st_dev, st_ino)` y mostrar los grupos.

```c
// Esqueleto:
struct inode_entry {
    dev_t dev;
    ino_t ino;
    char paths[16][PATH_MAX];  // hasta 16 hard links
    int count;
};

// Tabla hash simple para agrupar por (dev, ino):
#define HASH_SIZE 4096
struct inode_entry *hash_table[HASH_SIZE];

unsigned hash_inode(dev_t dev, ino_t ino) {
    return (unsigned)((dev * 31 + ino) % HASH_SIZE);
}

// Para cada archivo encontrado:
//   1. stat para obtener st_dev, st_ino, st_nlink
//   2. Si st_nlink > 1, buscar en la tabla hash
//   3. Si ya existe la entrada, agregar esta ruta
//   4. Si no existe, crear nueva entrada
//   5. Al final, imprimir grupos con count > 1
```

Requisitos:
- Solo reportar archivos regulares con `st_nlink > 1`.
- Mostrar el tamaño (solo una vez por grupo, ya que es el mismo inodo).
- Calcular el ahorro de espacio: `(nlinks - 1) × size` por grupo.
- Ignorar symlinks.

**Pregunta de reflexión**: ¿por qué es necesario considerar `st_dev`
además de `st_ino` para identificar un archivo de forma única? ¿Qué
pasaría si solo usaras `st_ino` en un sistema con múltiples filesystems
montados?

---

Creado `T03_Directorios/README.md` (~600 líneas). Cubre:

- **Estructura interna** de directorios (tabla nombre→inodo, permisos rwx especiales)
- **API POSIX**: opendir/readdir/closedir con manejo de errores, fdopendir, dirfd
- **struct dirent**: d_type con fallback para DT_UNKNOWN, búferes internos
- **Recorrido recursivo**: versión recursiva y versión iterativa con pila, problemas (symlinks circulares, descriptores agotados, PATH_MAX)
- **Operaciones**: mkdir/mkdir_p, rmdir, remove recursivo, rename con EXDEV, renameat2
- **CWD**: getcwd/chdir/fchdir, patrón guardar-restaurar, nota sobre multihilo
- **getdents64**: syscall directa vs readdir
- **nftw**: recorrido POSIX estándar con flags, limitaciones
- **Patrones prácticos**: búsqueda por extensión, operaciones con openat, realpath, scandir
- **5 errores comunes** + cheatsheet + 3 ejercicios con C