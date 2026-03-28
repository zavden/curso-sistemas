# Limitaciones de inotify y Alternativas

## Índice

1. [No es recursivo](#1-no-es-recursivo)
2. [Límites del sistema](#2-límites-del-sistema)
3. [Pérdida de eventos: IN_Q_OVERFLOW](#3-pérdida-de-eventos-in_q_overflow)
4. [No funciona con todo tipo de filesystem](#4-no-funciona-con-todo-tipo-de-filesystem)
5. [Problemas con rename y move](#5-problemas-con-rename-y-move)
6. [No reporta qué proceso causó el evento](#6-no-reporta-qué-proceso-causó-el-evento)
7. [Race conditions inherentes](#7-race-conditions-inherentes)
8. [fanotify: la alternativa avanzada](#8-fanotify-la-alternativa-avanzada)
9. [Otras alternativas y mecanismos](#9-otras-alternativas-y-mecanismos)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. No es recursivo

La limitación más conocida: un watch sobre un directorio solo
monitorea eventos **directamente dentro** de ese directorio. Los
subdirectorios requieren watches adicionales:

```
  Watch en /project/
  ┌─────────────────────────────────────────┐
  │ /project/                               │
  │  ├── file.c          ← detectado ✓     │
  │  ├── Makefile         ← detectado ✓     │
  │  ├── src/                                │
  │  │   ├── main.c       ← NO detectado ✗  │
  │  │   └── utils.c      ← NO detectado ✗  │
  │  └── tests/                              │
  │      └── test.c       ← NO detectado ✗  │
  └─────────────────────────────────────────┘
```

### Costo del monitoreo recursivo manual

```
  Proyecto típico          Subdirectorios    Watches
  ──────────────────────────────────────────────────────
  Proyecto C pequeño       ~10               10
  Aplicación Node.js       ~5000             5000
  (node_modules)
  Repositorio del kernel   ~4500             4500
  Home con dotfiles        ~200              200
```

Cada watch consume ~1 KiB de memoria del kernel (no paginable),
más el overhead de mantener la tabla wd→path en espacio de usuario.

### El problema de los directorios creados en runtime

Agregar watches al inicio no basta. Los subdirectorios creados
**después** de iniciar el monitoreo no se vigilan automáticamente:

```
  t=0   watch_recursive("/project/")  → 10 watches
  t=5   mkdir /project/src/new_module/
        → IN_CREATE + IN_ISDIR en /project/src/
        → Si no agregas un watch aquí, new_module/ es invisible

  t=10  touch /project/src/new_module/file.c
        → SIN watch: evento perdido
        → CON watch agregado en t=5: detectado ✓
```

### Race condition al agregar watch

Existe una ventana entre la creación del directorio y el momento
en que agregas el watch:

```
  Evento IN_CREATE "new_dir/" detectado
       │
       │  ← VENTANA: archivos creados aquí se pierden
       │
       ▼
  inotify_add_watch(fd, "path/new_dir/", mask)
       │
       ▼
  A partir de aquí, se detectan eventos
```

Solución: después de agregar el watch, escanear el contenido del
directorio para detectar archivos que se crearon en la ventana:

```c
if ((event->mask & IN_CREATE) && (event->mask & IN_ISDIR)) {
    char new_path[PATH_MAX];
    snprintf(new_path, sizeof(new_path), "%s/%s",
             wd_to_path(event->wd), event->name);

    // Agregar watch
    add_watch(inotify_fd, new_path, mask);

    // Escanear para archivos creados en la ventana
    DIR *dir = opendir(new_path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            // Procesar como si fuera un evento IN_CREATE
            printf("  Catch-up: %s/%s\n", new_path, entry->d_name);
        }
        closedir(dir);
    }
}
```

---

## 2. Límites del sistema

### Los tres límites configurables

```bash
# Máximo de watches por usuario:
$ cat /proc/sys/fs/inotify/max_user_watches
8192

# Máximo de instancias inotify por usuario:
$ cat /proc/sys/fs/inotify/max_user_instances
128

# Máximo de eventos en cola por instancia:
$ cat /proc/sys/fs/inotify/max_queued_events
16384
```

### max_user_watches: el límite más problemático

```
  Error típico:
  inotify_add_watch: No space left on device (ENOSPC)

  Causa: se alcanzó max_user_watches
  NO es un problema de espacio en disco — el errno es confuso
```

Proyectos JavaScript con `node_modules` son los infractores más
comunes. Un solo `npm install` puede crear miles de directorios:

```bash
# Contar subdirectorios en un proyecto Node.js:
$ find node_modules -type d | wc -l
4827

# VS Code necesita un watch por cada subdirectorio que abre
```

### Aumentar los límites

```bash
# Temporalmente (hasta reboot):
$ echo 524288 | sudo tee /proc/sys/fs/inotify/max_user_watches

# Permanentemente:
$ echo 'fs.inotify.max_user_watches=524288' | \
    sudo tee /etc/sysctl.d/90-inotify.conf
$ sudo sysctl --system
```

### Impacto de memoria

```
  Watches    Memoria kernel (aprox.)
  ──────────────────────────────────
  8192       ~8 MiB
  65536      ~64 MiB
  524288     ~512 MiB
  1048576    ~1 GiB
```

Esta memoria es **no paginable** (no va a swap). En servidores con
poca RAM, aumentar el límite excesivamente puede causar problemas.

### Ver watches en uso

```bash
# Watches totales del sistema:
$ cat /proc/sys/fs/inotify/max_user_watches

# Watches por proceso (Linux 5.4+):
$ find /proc/*/fdinfo -name '*.info' 2>/dev/null | \
    xargs grep -l inotify 2>/dev/null

# Forma más práctica — contar por proceso:
$ for pid in /proc/[0-9]*/fd/*; do
    readlink "$pid" 2>/dev/null
  done | grep inotify | sort | uniq -c | sort -rn | head
```

---

## 3. Pérdida de eventos: IN_Q_OVERFLOW

La cola de eventos tiene un tamaño máximo (`max_queued_events`). Si
el proceso no lee los eventos lo suficientemente rápido, la cola se
llena y los eventos se pierden:

```
  Ráfaga de eventos:
  ┌───────────────────────────────────────────┐
  │ Cola de inotify (max: 16384)              │
  │ ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐    │
  │ │e1│e2│e3│e4│e5│e6│e7│e8│..│..│..│eN│    │
  │ └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘    │
  │                    ↑ LLENA                 │
  │ Evento N+1 → DESCARTADO                   │
  │ Se genera IN_Q_OVERFLOW (wd=-1)            │
  └───────────────────────────────────────────┘
```

### Escenarios que causan overflow

```
  Escenario                    Eventos generados
  ──────────────────────────────────────────────────────
  tar xzf archivo.tar.gz      miles de CREATE+CLOSE
  git checkout otra_rama       cientos de MODIFY+CREATE+DELETE
  npm install                  miles de CREATE
  rm -rf directorio/           cientos de DELETE
  rsync de directorio grande   miles de CREATE+MODIFY+CLOSE
  Build system paralelo        ráfaga de MODIFY+CLOSE
```

### Estrategia de recuperación

```c
const struct inotify_event *event;
for (char *ptr = buf; ptr < buf + len;
     ptr += sizeof(struct inotify_event) + event->len) {
    event = (const struct inotify_event *)ptr;

    if (event->mask & IN_Q_OVERFLOW) {
        fprintf(stderr, "OVERFLOW — eventos perdidos\n");
        // Estrategia de recuperación:
        rescan_all_watched_directories();
        break;
    }

    // procesar evento normal...
}
```

La función `rescan_all_watched_directories()` debe:
1. Recorrer todos los directorios vigilados con `opendir`/`readdir`.
2. Comparar el estado actual con el estado conocido.
3. Procesar las diferencias como si fueran eventos.

### Minimizar el riesgo de overflow

```
  Estrategia                    Efectividad
  ──────────────────────────────────────────────────────
  Buffer de read grande         alta — lee más eventos
  (≥ 65536 bytes)               por syscall

  Leer en hilo dedicado         alta — no se bloquea por
                                procesamiento

  Aumentar max_queued_events    media — retarda pero no
                                elimina el problema

  IN_EXCL_UNLINK                media — reduce eventos
                                para archivos borrados

  Filtrar con máscara precisa   alta — solo pedir los
  (no IN_ALL_EVENTS)            eventos necesarios
```

---

## 4. No funciona con todo tipo de filesystem

inotify depende de que el filesystem **notifique al subsistema de
inotify** en el kernel. Esto funciona con filesystems locales pero
falla con varios tipos:

```
  Filesystem          inotify funciona?    Notas
  ──────────────────────────────────────────────────────────
  ext4                ✓ sí
  XFS                 ✓ sí
  Btrfs               ✓ sí
  tmpfs               ✓ sí
  NFS                 ✗ no                cambios remotos
                                          no generan eventos
  CIFS/SMB            ✗ no                mismo problema
  FUSE                parcial             depende de la
                                          implementación
  sshfs               ✗ no                FUSE + red
  9p (virtio-fs)      parcial             depende
  proc/sysfs          parcial             algunos archivos
  overlayfs           parcial             capa superior sí,
                                          inferior no siempre
```

### El problema de NFS

Cuando otro cliente NFS modifica un archivo en el servidor, tu
cliente local no recibe notificación de inotify:

```
  Cliente A (tu máquina)      Servidor NFS       Cliente B
  ┌───────────────────┐     ┌────────────┐     ┌────────────┐
  │ inotify watch     │     │            │     │            │
  │ en /nfs/data/     │     │ /exports/  │     │ write()    │
  │                   │     │ data/      │◄────│ a file.txt │
  │ ¿evento?          │     │            │     │            │
  │ NO — inotify no   │     │            │     │            │
  │ sabe del cambio   │     │            │     │            │
  └───────────────────┘     └────────────┘     └────────────┘
```

inotify opera a nivel del VFS local. Los cambios hechos por otros
clientes NFS no pasan por el VFS de tu máquina.

### Soluciones para filesystems de red

```
  Solución                 Tradeoff
  ──────────────────────────────────────────────────────
  Polling con stat()       simple, pero consume CPU/red
  Polling con readdir()    detecta nuevos archivos
  Protocolo propio         complejo, pero preciso
  NFS delegations          solo NFS v4, limitado
  kqueue (BSD)             no aplica en Linux
  fanotify (parcial)       no resuelve el problema de red
```

Para NFS, el enfoque pragmático es combinar inotify (para cambios
locales) con polling periódico (para cambios remotos).

---

## 5. Problemas con rename y move

### Rename entre directorios vigilados

Cuando un archivo se mueve entre dos directorios que tienen watches,
el cookie vincula los eventos. Pero si el directorio destino **no
está vigilado**, solo ves `IN_MOVED_FROM`:

```
  Watch en /dir_a/     Watch en /dir_b/     Sin watch en /dir_c/
  ─────────────────    ─────────────────    ─────────────────────
  mv /dir_a/f /dir_b/f:
    IN_MOVED_FROM        IN_MOVED_TO
    cookie=100           cookie=100         ← vinculados ✓

  mv /dir_a/f /dir_c/f:
    IN_MOVED_FROM
    cookie=200                               (no hay evento)
    ← parece un DELETE                       ← f "desapareció"
```

### Rename del directorio vigilado mismo

Si el directorio vigilado se renombra, el watch **sigue activo**
(está asociado al inodo, no al nombre), pero tu tabla wd→path
queda desactualizada:

```c
// Watch en /tmp/mydir/ (wd=1)
// Alguien hace: mv /tmp/mydir /tmp/otherdir

// Tu tabla dice: wd=1 → "/tmp/mydir/"
// Realidad: wd=1 → "/tmp/otherdir/" (mismo inodo)
// Eventos reportan paths incorrectos
```

Se genera `IN_MOVE_SELF`, pero no te dice la nueva ruta.
Descubrir la ruta nueva requiere técnicas como:

```c
// Leer /proc/self/fd/N donde N es el fd del directorio
// (si tienes un fd abierto del directorio)
char link_path[64], real_path[PATH_MAX];
snprintf(link_path, sizeof(link_path), "/proc/self/fd/%d", dir_fd);
ssize_t n = readlink(link_path, real_path, sizeof(real_path) - 1);
if (n > 0) {
    real_path[n] = '\0';
    printf("Nueva ruta: %s\n", real_path);
}
```

---

## 6. No reporta qué proceso causó el evento

inotify te dice **qué** pasó pero no **quién** lo hizo:

```
  Evento inotify:
  wd=1, mask=IN_MODIFY, name="config.ini"

  ¿Quién modificó config.ini?
  - ¿Un usuario con vim?
  - ¿Un script automatizado?
  - ¿Un proceso malicioso?
  → inotify NO lo sabe
```

Esto es una limitación fundamental del diseño. Si necesitas saber
quién hizo el cambio, necesitas **fanotify** o **audit**.

### fanotify: sí reporta el proceso

```
  fanotify evento:
  fd del archivo afectado
  + pid del proceso que lo causó
  + puede BLOQUEAR la operación (permission events)
```

### audit: registro completo

```bash
# Linux audit subsystem:
$ auditctl -w /etc/passwd -p wa -k passwd_changes
# Registra quién (UID, PID, exe) modificó /etc/passwd

$ ausearch -k passwd_changes
type=SYSCALL ... pid=1234 uid=0 exe="/usr/bin/vim" ...
```

---

## 7. Race conditions inherentes

### Race 1: evento llega después de que el archivo desapareció

```
  t=0   Archivo creado → IN_CREATE "file.txt"
  t=1   Archivo borrado (por otro proceso)
  t=2   Tu código procesa IN_CREATE → open("file.txt") → ENOENT

  El evento describe el PASADO, no el PRESENTE
```

Solución: manejar errores en las operaciones subsecuentes.

### Race 2: múltiples eventos sobre el mismo archivo

```
  t=0   IN_CREATE  "file.txt"
  t=1   IN_MODIFY  "file.txt"
  t=2   IN_MODIFY  "file.txt"
  t=3   IN_DELETE  "file.txt"

  Si tu código es lento y procesa IN_CREATE en t=5,
  el archivo ya no existe
```

### Race 3: watch de directorio vs contenido del directorio

```
  t=0   inotify_add_watch("/dir/", IN_CREATE)
  t=1   ← otro proceso crea /dir/file.txt AQUÍ
  t=2   Tu código empieza a leer eventos

  El archivo creado en t=1 no genera evento porque el
  evento se creó entre el add_watch y el primer read
```

Solución: escanear el directorio **después** de agregar el watch
para detectar archivos ya existentes.

### Diseño tolerante a races

```c
// Principio: no confiar en que el estado actual coincide
// con lo que dice el evento

// MAL:
if (event->mask & IN_CREATE) {
    // Asumir que el archivo existe
    int fd = open(path, O_RDONLY);  // puede fallar
}

// BIEN:
if (event->mask & IN_CREATE) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        if (errno == ENOENT) {
            // Ya fue borrado — ignorar
            continue;
        }
        perror(path);
        continue;
    }
    // Usar fd...
    close(fd);
}
```

---

## 8. fanotify: la alternativa avanzada

`fanotify` (filesystem-wide notification) fue añadido en Linux 2.6.37
como evolución de inotify:

```c
#include <fcntl.h>
#include <sys/fanotify.h>

int fanotify_init(unsigned int flags, unsigned int event_f_flags);
int fanotify_mark(int fanotify_fd, unsigned int flags,
                  uint64_t mask, int dirfd, const char *pathname);
```

### Diferencias clave con inotify

```
  Característica       inotify              fanotify
  ──────────────────────────────────────────────────────────
  Granularidad         por archivo/dir      mount point completo
                                            o filesystem
  Recursivo            no (manual)          sí (marca mount)
  Identifica proceso   no                   sí (pid en evento)
  Permission events    no                   sí (puede bloquear
                                            y decidir permitir
                                            o denegar acceso)
  Privilegios          usuario normal       CAP_SYS_ADMIN o
                                            CAP_DAC_READ_SEARCH
  Uso principal        aplicaciones         antivirus, auditoría,
                       de usuario           HSM, backup
  API de eventos       buffer de structs    fd por evento
```

### Ejemplo básico de fanotify

```c
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/fanotify.h>
#include <limits.h>

int main(void) {
    // Requiere privilegios (CAP_SYS_ADMIN)
    int fan_fd = fanotify_init(
        FAN_CLASS_NOTIF | FAN_CLOEXEC,
        O_RDONLY | O_LARGEFILE);
    if (fan_fd == -1) { perror("fanotify_init"); return 1; }

    // Marcar un mount point completo (recursivo automáticamente)
    if (fanotify_mark(fan_fd, FAN_MARK_ADD | FAN_MARK_MOUNT,
                      FAN_OPEN | FAN_CLOSE_WRITE,
                      AT_FDCWD, "/home") == -1) {
        perror("fanotify_mark");
        return 1;
    }

    printf("Monitoreando /home (todo el mount)...\n");

    char buf[4096];
    for (;;) {
        ssize_t len = read(fan_fd, buf, sizeof(buf));
        if (len <= 0) break;

        struct fanotify_event_metadata *event =
            (struct fanotify_event_metadata *)buf;

        while (FAN_EVENT_OK(event, len)) {
            if (event->fd >= 0) {
                // Obtener path del archivo via /proc/self/fd/
                char fd_path[64], file_path[PATH_MAX];
                snprintf(fd_path, sizeof(fd_path),
                         "/proc/self/fd/%d", event->fd);
                ssize_t n = readlink(fd_path, file_path,
                                     sizeof(file_path) - 1);
                if (n > 0) {
                    file_path[n] = '\0';
                    printf("PID=%d %s: %s\n",
                           event->pid,
                           (event->mask & FAN_OPEN) ?
                               "OPEN" : "CLOSE_WRITE",
                           file_path);
                }
                close(event->fd);
            }
            event = FAN_EVENT_NEXT(event, len);
        }
    }

    close(fan_fd);
    return 0;
}
```

### Permission events (bloquear acceso)

fanotify puede **interceptar** operaciones y decidir si permitirlas:

```c
// Inicializar con FAN_CLASS_CONTENT para permission events
int fan_fd = fanotify_init(
    FAN_CLASS_CONTENT | FAN_CLOEXEC,
    O_RDONLY | O_LARGEFILE);

// Marcar con eventos de permiso
fanotify_mark(fan_fd, FAN_MARK_ADD,
              FAN_OPEN_PERM | FAN_ACCESS_PERM,
              AT_FDCWD, "/protected");

// Procesar: decidir si permitir o denegar
struct fanotify_response response;
response.fd = event->fd;
response.response = FAN_ALLOW;  // o FAN_DENY

write(fan_fd, &response, sizeof(response));
```

Esto es lo que usan los **antivirus en Linux** para escanear archivos
antes de permitir su apertura.

### Cuándo usar fanotify vs inotify

```
  Caso de uso                      Usar
  ──────────────────────────────────────────────────────
  Monitorear un directorio         inotify
  Build watcher                    inotify
  Hot reload de config             inotify
  Monitorear filesystem completo   fanotify
  Antivirus / scanner              fanotify (permission)
  Auditoría de accesos             fanotify (+ audit)
  HSM (migrar archivos a cinta)    fanotify (permission)
  Backup incremental               fanotify (FAN_MARK_FILESYSTEM)
  Aplicación sin privilegios       inotify (no requiere CAP)
```

---

## 9. Otras alternativas y mecanismos

### Polling inteligente

A veces, la simplicidad del polling supera la complejidad de inotify:

```c
// Polling eficiente para un solo archivo:
struct stat last_sb, current_sb;
stat(path, &last_sb);

while (1) {
    sleep(2);
    stat(path, &current_sb);

    if (current_sb.st_mtime != last_sb.st_mtime ||
        current_sb.st_size != last_sb.st_size) {
        printf("Archivo cambió\n");
        // procesar...
        last_sb = current_sb;
    }
}
```

Ventajas del polling:
- Funciona en **cualquier** filesystem (NFS, CIFS, FUSE).
- Sin límite de watches.
- Sin race conditions de queue overflow.
- Código más simple.

Desventajas:
- Latencia de hasta `sleep_interval`.
- Consume CPU/I/O proporcionalmente al número de archivos.
- No escala a miles de archivos.

### kqueue (BSD/macOS)

El equivalente en sistemas BSD. Más potente que inotify en algunos
aspectos:

```
  Característica       inotify (Linux)      kqueue (BSD)
  ──────────────────────────────────────────────────────────
  Monitorea por        path                 fd
  No requiere          nada extra           open() por archivo
  Eventos genéricos    solo filesystem      filesystem + sockets
                                            + procesos + señales
  Recursivo            no                   no
  Portabilidad         solo Linux           BSD, macOS
```

En macOS, `FSEvents` proporciona monitoreo recursivo de alto nivel
sobre kqueue.

### Watchman (Meta/Facebook)

Solución de espacio de usuario que combina inotify con caching
inteligente:

```bash
# Instalar y iniciar
$ watchman watch /path/to/project

# Suscribirse a cambios
$ watchman -- trigger /path/to/project buildme '*.c' -- make

# Consultar archivos cambiados desde un punto en el tiempo
$ watchman since /path/to/project "c:1234567"
```

Watchman resuelve el problema de monitoreo recursivo manteniendo un
daemon que trackea el estado del filesystem y usa inotify internamente.

### Tabla de decisión final

```
  Necesidad                           Mecanismo
  ──────────────────────────────────────────────────────
  1-10 archivos locales               inotify o polling
  Directorio local con subdirs        inotify recursivo
  Filesystem completo                 fanotify
  NFS / CIFS / FUSE                   polling
  Antivirus / seguridad               fanotify + permission
  Auditoría con PID/UID               fanotify o audit
  Proyecto grande (IDE)               Watchman
  Portabilidad Linux+macOS            librería (libuv, fswatch)
  Portabilidad POSIX                  polling
```

### Librerías multiplataforma

```
  Librería       Plataformas        Backend
  ──────────────────────────────────────────────────────
  libuv          Linux, macOS,      inotify, kqueue,
                 Windows            ReadDirectoryChanges
  fswatch        Linux, macOS,      inotify, kqueue,
                 BSD, Windows       FSEvents, polling
  notify (Rust)  Linux, macOS,      inotify, kqueue,
                 Windows            ReadDirectoryChanges
  watchdog (Py)  Linux, macOS,      inotify, kqueue,
                 Windows            FSEvents, polling
```

---

## 10. Errores comunes

### Error 1: no tener plan de recuperación para IN_Q_OVERFLOW

```c
// MAL: ignorar overflow
if (event->mask & IN_Q_OVERFLOW) {
    // "Esto nunca pasa en la práctica"
    // Spoiler: sí pasa, especialmente con git y npm
}

// BIEN: re-escanear al detectar overflow
if (event->mask & IN_Q_OVERFLOW) {
    fprintf(stderr, "Overflow — re-escaneando directorios\n");

    // Para cada directorio vigilado:
    for (int i = 0; i < num_watches; i++) {
        scan_directory(watch_table[i].path);
    }
    // Comparar con el estado conocido y procesar diferencias
}
```

### Error 2: usar inotify en NFS y esperar que funcione

```c
// MAL: asumir que inotify detecta cambios remotos
inotify_add_watch(fd, "/nfs/share/config", IN_MODIFY);
// Otro cliente NFS modifica config → NO hay evento

// BIEN: combinar inotify (local) con polling (remoto)
// O simplemente usar polling si el filesystem es de red:
struct stat sb;
while (1) {
    sleep(5);
    stat("/nfs/share/config", &sb);
    if (sb.st_mtime != last_mtime) {
        reload_config();
        last_mtime = sb.st_mtime;
    }
}
```

### Error 3: no manejar ENOSPC en inotify_add_watch

```c
// MAL: no verificar el error
int wd = inotify_add_watch(fd, path, mask);
watch_table[n].wd = wd;  // wd puede ser -1

// BIEN: verificar y diagnosticar
int wd = inotify_add_watch(fd, path, mask);
if (wd == -1) {
    if (errno == ENOSPC) {
        fprintf(stderr,
            "Límite de watches alcanzado. Actual: %d\n"
            "Aumentar con:\n"
            "  echo 524288 | sudo tee "
            "/proc/sys/fs/inotify/max_user_watches\n",
            num_watches);
    } else {
        perror("inotify_add_watch");
    }
    return -1;
}
```

### Error 4: confundir ENOSPC de inotify con disco lleno

```c
// El errno ENOSPC en inotify_add_watch NO significa "disco lleno"
// Significa "límite de watches alcanzado"

// Verificar:
if (errno == ENOSPC) {
    // Podría ser watches agotados O espacio en disco
    // Para inotify_add_watch, SIEMPRE es watches
    struct statvfs vfs;
    statvfs("/", &vfs);
    if (vfs.f_bavail > 0) {
        // Hay espacio en disco — es el límite de inotify
    }
}
```

### Error 5: asumir que fanotify es un reemplazo directo de inotify

```c
// Diferencias que importan:
// 1. fanotify requiere CAP_SYS_ADMIN (root)
// 2. fanotify no da el nombre del archivo en el evento
//    (da un fd, debes leer /proc/self/fd/N)
// 3. fanotify no distingue IN_CREATE de IN_MODIFY
//    (trabaja a nivel de open/close/access)
// 4. fanotify marca mount points, no archivos individuales
//    (puede recibir eventos de TODO el filesystem)
```

---

## 11. Cheatsheet

```
  LIMITACIONES DE INOTIFY
  ──────────────────────────────────────────────────────────
  No recursivo          agregar watch por subdirectorio
  Límite de watches     /proc/sys/fs/inotify/max_user_watches
  Cola finita           IN_Q_OVERFLOW → eventos perdidos
  No funciona en NFS    ni CIFS, FUSE parcial
  No reporta PID        solo qué pasó, no quién
  Race conditions       evento ≠ estado actual
  Rename parcial        MOVED_FROM sin MOVED_TO si
                        destino no está vigilado

  LÍMITES DEL SISTEMA
  ──────────────────────────────────────────────────────────
  max_user_watches      default 8192, subir a 524288
  max_user_instances    default 128
  max_queued_events     default 16384
  Memoria por watch     ~1 KiB (no paginable)
  ENOSPC               = límite de watches (NO disco)

  FANOTIFY vs INOTIFY
  ──────────────────────────────────────────────────────────
  fanotify: recursivo, reporta PID, permission events
  fanotify: requiere CAP_SYS_ADMIN, no da nombre de archivo
  inotify: sin privilegios, da nombre, por directorio
  inotify: no recursivo, no reporta PID

  ALTERNATIVAS
  ──────────────────────────────────────────────────────────
  Polling               funciona en todo, no escala
  fanotify              filesystem completo, privilegios
  audit                 registro completo con PID/UID
  Watchman (Meta)       daemon de alto nivel
  libuv/fswatch         multiplataforma
  kqueue                BSD/macOS

  CUÁNDO NO USAR INOTIFY
  ──────────────────────────────────────────────────────────
  Filesystem de red (NFS/CIFS)     → polling
  Necesitas PID del proceso        → fanotify o audit
  Necesitas bloquear operaciones   → fanotify permission
  Portabilidad a macOS/Windows     → libuv, fswatch
  Monitoreo de TODO el filesystem  → fanotify

  MANEJO DE OVERFLOW
  ──────────────────────────────────────────────────────────
  1. Detectar: event->mask & IN_Q_OVERFLOW
  2. Pausar procesamiento de eventos
  3. Re-escanear directorios vigilados
  4. Comparar con estado conocido
  5. Procesar diferencias
  6. Reanudar procesamiento normal

  ESTRATEGIA PARA NFS
  ──────────────────────────────────────────────────────────
  Cambios locales:   inotify (rápido)
  Cambios remotos:   polling con stat() (cada 5-30s)
  Combinar ambos en el mismo event loop
```

---

## 12. Ejercicios

### Ejercicio 1: monitor con recuperación de overflow

Escribe un monitor de directorio que detecte `IN_Q_OVERFLOW` y se
recupere correctamente:

```c
// Esqueleto:
typedef struct {
    char name[256];
    time_t mtime;
    off_t size;
} file_state_t;

typedef struct {
    file_state_t files[4096];
    int count;
} dir_snapshot_t;

// 1. Tomar snapshot inicial del directorio
void take_snapshot(const char *dir, dir_snapshot_t *snap);

// 2. Al detectar overflow, tomar nuevo snapshot
//    y comparar con el anterior
void diff_snapshots(dir_snapshot_t *old, dir_snapshot_t *new,
                    /* callback para diferencias */);

// 3. Procesar diferencias como eventos sintéticos
```

Requisitos:
- Simular overflow: crear un directorio con `IN_Q_OVERFLOW` provocado
  (crear miles de archivos rápidamente con un script).
- La recuperación debe detectar archivos creados, borrados y modificados
  durante el overflow.
- Medir cuántos eventos se perdieron.

**Pregunta de reflexión**: la estrategia de re-escanear tiene un costo
proporcional al tamaño del directorio. Si el directorio tiene 100,000
archivos, ¿cuánto tarda un re-escaneo completo? ¿Cómo podrías hacer el
re-escaneo incremental en lugar de completo?

---

### Ejercicio 2: monitor que funciona en NFS

Escribe un monitor de archivos que combine inotify para detección
rápida con polling como fallback para filesystems que no soportan
inotify:

```c
// Esqueleto:
#include <sys/vfs.h>

// Detectar si un path está en NFS:
int is_network_fs(const char *path) {
    struct statfs sfs;
    if (statfs(path, &sfs) == -1) return -1;

    // NFS magic: 0x6969, CIFS: 0xFF534D42
    return (sfs.f_type == 0x6969 ||
            sfs.f_type == 0xFF534D42);
}

// Si es NFS: usar polling
// Si es local: usar inotify
// Ambos integrados en el mismo poll() event loop
```

Requisitos:
- Detectar automáticamente el tipo de filesystem.
- Para NFS: polling cada 5 segundos con `stat()`.
- Para local: inotify sin polling.
- Ambos integrados en un solo `poll()` con timerfd.
- Misma interfaz de callback para ambos mecanismos.

**Pregunta de reflexión**: ¿qué pasa si un directorio está en un
filesystem local pero contiene bind mounts de NFS? ¿`statfs` reporta
el tipo correcto para cada subdirectorio? ¿Cómo manejarías este caso?

---

### Ejercicio 3: comparación de rendimiento inotify vs polling

Escribe un programa que mida el consumo de recursos de inotify vs
polling para monitorear N archivos:

```c
// Esqueleto:
// 1. Crear N archivos temporales (100, 1000, 10000)
// 2. Medir con inotify:
//    - Tiempo de setup (agregar watches)
//    - Memoria usada (/proc/self/status VmRSS)
//    - Latencia de detección (modificar archivo, medir
//      tiempo hasta que el evento se lee)
// 3. Medir con polling:
//    - Tiempo de un ciclo de stat() sobre todos los archivos
//    - CPU usado (clock_gettime CLOCK_PROCESS_CPUTIME_ID)
//    - Latencia promedio (poll_interval / 2)
// 4. Comparar y graficar
```

Requisitos:
- Probar con 100, 1000 y 10000 archivos.
- Medir latencia de detección: un hilo/proceso modifica archivos
  aleatoriamente, el monitor mide el tiempo hasta detectar el cambio.
- Medir uso de CPU durante 30 segundos de monitoreo.
- Probar polling con intervalos de 100ms, 1s y 5s.

**Pregunta de reflexión**: a partir de cuántos archivos se justifica
la complejidad de inotify sobre el polling simple? ¿Hay un punto de
cruce? ¿Cómo cambia la respuesta si la latencia de detección no es
importante (por ejemplo, un backup nocturno vs un IDE que necesita
respuesta instantánea)?

---
