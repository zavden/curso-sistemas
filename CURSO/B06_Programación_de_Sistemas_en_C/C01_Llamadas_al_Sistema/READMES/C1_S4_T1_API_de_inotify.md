# API de inotify — Monitoreo de Filesystem en Tiempo Real

## Índice

1. [Qué es inotify y por qué existe](#1-qué-es-inotify-y-por-qué-existe)
2. [Arquitectura: fd, watches y eventos](#2-arquitectura-fd-watches-y-eventos)
3. [inotify_init: crear la instancia](#3-inotify_init-crear-la-instancia)
4. [inotify_add_watch: registrar vigilancia](#4-inotify_add_watch-registrar-vigilancia)
5. [Máscara de eventos: qué monitorear](#5-máscara-de-eventos-qué-monitorear)
6. [Leer eventos: struct inotify_event](#6-leer-eventos-struct-inotify_event)
7. [inotify_rm_watch: dejar de vigilar](#7-inotify_rm_watch-dejar-de-vigilar)
8. [Programa completo: monitor de directorio](#8-programa-completo-monitor-de-directorio)
9. [Integración con poll/epoll](#9-integración-con-pollepoll)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es inotify y por qué existe

El problema: necesitas saber cuándo un archivo o directorio cambia.
Sin inotify, la única opción es **polling** — verificar periódicamente
con `stat()`:

```
  Polling (sin inotify):
  ┌──────────────────────────────────────────────────────┐
  │  while (1) {                                        │
  │      stat("config.ini", &sb);                       │
  │      if (sb.st_mtime != last_mtime) {               │
  │          // ¡cambió!                                 │
  │          reload_config();                            │
  │          last_mtime = sb.st_mtime;                   │
  │      }                                              │
  │      sleep(1);  // ¿Cada cuánto? tradeoff           │
  │  }                                                  │
  │                                                      │
  │  Problemas:                                          │
  │  - Consume CPU innecesariamente                      │
  │  - Latencia de hasta sleep_interval                  │
  │  - No escala con miles de archivos                   │
  │  - No detecta qué cambió exactamente                 │
  └──────────────────────────────────────────────────────┘
```

**inotify** es un subsistema del kernel Linux que **notifica** al
proceso cuando ocurren cambios en el filesystem, sin polling:

```
  inotify (notificación del kernel):
  ┌──────────────────────────────────────────────────────┐
  │  Kernel detecta cambio                               │
  │       │                                              │
  │       ▼                                              │
  │  Encola evento en el fd de inotify                   │
  │       │                                              │
  │       ▼                                              │
  │  read(inotify_fd) se desbloquea                      │
  │       │                                              │
  │       ▼                                              │
  │  Proceso recibe: qué archivo, qué tipo de cambio     │
  │                                                      │
  │  Ventajas:                                           │
  │  - 0% CPU mientras espera                            │
  │  - Latencia ~instantánea                             │
  │  - Escala a miles de watches                         │
  │  - Detalle: qué archivo y qué operación              │
  └──────────────────────────────────────────────────────┘
```

### Historia

```
  Mecanismo       Kernel    Estado
  ──────────────────────────────────────────────────
  dnotify         2.4       obsoleto (solo directorios,
                            basado en señales)
  inotify         2.6.13    estable, ampliamente usado
  fanotify        2.6.37    avanzado (monitoreo global,
                            permisos, auditoría)
```

---

## 2. Arquitectura: fd, watches y eventos

inotify se basa en tres conceptos:

```
  ┌──────────────────────────────────────────────────────┐
  │  Proceso                                             │
  │                                                      │
  │  inotify_fd = inotify_init1(IN_NONBLOCK)             │
  │       │                                              │
  │       ├── watch 1 (wd=1): "/etc/config.ini"          │
  │       │   máscara: IN_MODIFY | IN_DELETE_SELF        │
  │       │                                              │
  │       ├── watch 2 (wd=2): "/var/log/"                │
  │       │   máscara: IN_CREATE | IN_DELETE             │
  │       │                                              │
  │       └── watch 3 (wd=3): "/tmp/data/"               │
  │           máscara: IN_ALL_EVENTS                     │
  │                                                      │
  │  Cola de eventos interna del kernel:                  │
  │  ┌─────────┬─────────┬─────────┬─────────┐          │
  │  │ evento1 │ evento2 │ evento3 │  ...    │          │
  │  └─────────┴─────────┴─────────┴─────────┘          │
  │       ↑                                              │
  │  read(inotify_fd, buf, sizeof(buf))                  │
  │  → devuelve uno o más eventos                        │
  └──────────────────────────────────────────────────────┘
```

### Componentes

```
  Componente        Descripción
  ──────────────────────────────────────────────────────
  inotify_fd        descriptor de archivo que representa
                    la instancia inotify (un fd normal:
                    se puede poll/epoll/select/close)

  watch (wd)        vigilancia sobre un path específico.
                    Identificado por un watch descriptor
                    (entero). Un inotify_fd puede tener
                    muchos watches.

  evento            notificación de un cambio: contiene
                    wd, máscara, cookie, nombre del archivo
```

---

## 3. inotify_init: crear la instancia

```c
#include <sys/inotify.h>

int inotify_init(void);
int inotify_init1(int flags);
```

`inotify_init1` es la versión moderna que acepta flags:

```
  Flag              Efecto
  ──────────────────────────────────────────────────────
  IN_NONBLOCK       read() no bloquea si no hay eventos
                    (retorna EAGAIN)
  IN_CLOEXEC        cerrar fd automáticamente en exec()
```

### Ejemplo

```c
int inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
if (inotify_fd == -1) {
    perror("inotify_init1");
    return -1;
}

// inotify_fd es un fd normal
// Se puede usar con poll(), epoll(), select()
// Se cierra con close()
```

**Siempre usa `inotify_init1`** con al menos `IN_CLOEXEC`. La versión
sin flags (`inotify_init`) es legacy.

---

## 4. inotify_add_watch: registrar vigilancia

```c
#include <sys/inotify.h>

int inotify_add_watch(int fd, const char *pathname, uint32_t mask);
```

Registra un path para monitorear. Retorna un **watch descriptor** (wd)
que identifica este watch:

```c
int wd = inotify_add_watch(inotify_fd, "/etc/config.ini",
                            IN_MODIFY | IN_DELETE_SELF);
if (wd == -1) {
    perror("inotify_add_watch");
    return -1;
}
printf("Watch descriptor: %d\n", wd);
```

### Vigilar un archivo vs un directorio

```
  Vigilar un archivo:
  inotify_add_watch(fd, "/etc/passwd", IN_MODIFY)
  → Solo monitorea ESE archivo
  → Detecta: modificación, atributos, borrado
  → NO detecta: creación de otros archivos

  Vigilar un directorio:
  inotify_add_watch(fd, "/var/log/", IN_CREATE | IN_DELETE)
  → Monitorea eventos DENTRO del directorio
  → Detecta: creación, borrado, renombrado de archivos dentro
  → El campo 'name' del evento indica QUÉ archivo
  → NO monitorea subdirectorios (no es recursivo)
```

### Modificar un watch existente

Si llamas `inotify_add_watch` con un path que ya tiene un watch,
**reemplaza** la máscara:

```c
// Primero: solo modificaciones
int wd = inotify_add_watch(fd, "/etc/hosts", IN_MODIFY);

// Después: agregar monitoreo de acceso (reemplaza la máscara)
inotify_add_watch(fd, "/etc/hosts", IN_MODIFY | IN_ACCESS);
// El wd retornado es el MISMO

// Para AGREGAR sin reemplazar, usar IN_MASK_ADD:
inotify_add_watch(fd, "/etc/hosts",
                  IN_DELETE_SELF | IN_MASK_ADD);
// Ahora la máscara es: IN_MODIFY | IN_ACCESS | IN_DELETE_SELF
```

### Watches sobre el mismo inodo

Si dos paths apuntan al mismo inodo (hard links), `inotify_add_watch`
retorna el **mismo wd** y combina las máscaras:

```c
// /tmp/fileA y /tmp/fileB son hard links al mismo inodo
int wd1 = inotify_add_watch(fd, "/tmp/fileA", IN_MODIFY);
int wd2 = inotify_add_watch(fd, "/tmp/fileB", IN_ACCESS);
// wd1 == wd2 (mismo inodo)
// Máscara combinada: IN_MODIFY | IN_ACCESS
```

---

## 5. Máscara de eventos: qué monitorear

### Eventos para archivos y directorios

```
  Evento              Valor       Se genera cuando...
  ──────────────────────────────────────────────────────────
  IN_ACCESS           0x00000001  se leen datos (read)
  IN_MODIFY           0x00000002  se escriben datos (write)
  IN_ATTRIB           0x00000004  cambian metadatos (chmod,
                                  chown, timestamps, link count)
  IN_CLOSE_WRITE      0x00000008  se cierra un fd abierto
                                  para escritura
  IN_CLOSE_NOWRITE    0x00000010  se cierra un fd abierto
                                  solo lectura
  IN_OPEN             0x00000020  se abre el archivo
  IN_MOVED_FROM       0x00000040  archivo renombrado DESDE
                                  este directorio
  IN_MOVED_TO         0x00000080  archivo renombrado HACIA
                                  este directorio
  IN_CREATE           0x00000100  archivo/dir creado dentro
                                  del directorio vigilado
  IN_DELETE           0x00000200  archivo/dir borrado dentro
                                  del directorio vigilado
  IN_DELETE_SELF      0x00000400  el objeto vigilado mismo
                                  fue borrado
  IN_MOVE_SELF        0x00000800  el objeto vigilado mismo
                                  fue movido/renombrado
```

### Macros de conveniencia

```
  IN_CLOSE          IN_CLOSE_WRITE | IN_CLOSE_NOWRITE
  IN_MOVE           IN_MOVED_FROM | IN_MOVED_TO
  IN_ALL_EVENTS     todos los eventos anteriores combinados
```

### Flags adicionales en la máscara

```
  Flag               Efecto
  ──────────────────────────────────────────────────────────
  IN_MASK_ADD        agregar a la máscara existente en vez
                     de reemplazarla
  IN_ONESHOT         auto-eliminar el watch después del
                     primer evento
  IN_ONLYDIR         fallar si el path no es un directorio
  IN_DONT_FOLLOW     no seguir symlinks
  IN_EXCL_UNLINK     no generar eventos para archivos
                     que ya fueron unlinked del directorio
```

### Eventos generados automáticamente (no se piden)

```
  Evento              Siempre se incluye   Significado
  ──────────────────────────────────────────────────────────
  IN_ISDIR            si el evento es      el evento es sobre
                      sobre un directorio  un subdirectorio
  IN_IGNORED          watch eliminado      por inotify_rm_watch
                                           o borrado del archivo
  IN_Q_OVERFLOW       cola llena           se perdieron eventos
  IN_UNMOUNT          filesystem desmontado
```

---

## 6. Leer eventos: struct inotify_event

Los eventos se leen con `read()` normal sobre el `inotify_fd`:

```c
struct inotify_event {
    int      wd;       // watch descriptor que generó el evento
    uint32_t mask;     // tipo de evento (bits)
    uint32_t cookie;   // para vincular MOVED_FROM con MOVED_TO
    uint32_t len;      // longitud de name[] (incluye padding)
    char     name[];   // nombre del archivo (solo para directorios)
};
```

### Layout en el buffer

Cada evento tiene **tamaño variable** porque `name[]` depende del
nombre del archivo:

```
  Buffer leído con read():
  ┌────────────────────┬────────────────────────┬──────────┐
  │ inotify_event #1   │ inotify_event #2       │ ...      │
  │ wd=1, mask, len=16 │ wd=2, mask, len=0      │          │
  │ name="config.ini\0"│ (sin nombre — archivo) │          │
  │ + padding           │                        │          │
  └────────────────────┴────────────────────────┴──────────┘

  Tamaño de un evento = sizeof(struct inotify_event) + event->len
```

### Recorrer eventos en el buffer

```c
#define BUF_SIZE (4096)

char buf[BUF_SIZE]
    __attribute__((aligned(__alignof__(struct inotify_event))));

ssize_t len = read(inotify_fd, buf, sizeof(buf));
if (len == -1) {
    if (errno == EAGAIN) {
        // No hay eventos (con IN_NONBLOCK)
        return;
    }
    perror("read");
    return;
}

// Recorrer eventos
const struct inotify_event *event;
for (char *ptr = buf; ptr < buf + len;
     ptr += sizeof(struct inotify_event) + event->len) {

    event = (const struct inotify_event *)ptr;

    printf("wd=%d mask=0x%08x", event->wd, event->mask);

    if (event->len > 0) {
        printf(" name=%s", event->name);
    }

    if (event->mask & IN_ISDIR) printf(" [DIR]");
    printf("\n");
}
```

### El campo cookie: vincular rename

Cuando un archivo se renombra dentro de un directorio vigilado, se
generan **dos** eventos: `IN_MOVED_FROM` y `IN_MOVED_TO`. El campo
`cookie` es un valor único que los vincula:

```
  mv /watched/old_name /watched/new_name

  Evento 1: wd=1 mask=IN_MOVED_FROM cookie=12345 name="old_name"
  Evento 2: wd=1 mask=IN_MOVED_TO   cookie=12345 name="new_name"
                                     ^^^^^^^^^^^^
                                     mismo cookie → son el mismo rename
```

Si un archivo se mueve **fuera** del directorio vigilado, solo se genera
`IN_MOVED_FROM` (sin un `IN_MOVED_TO` correspondiente). Si se mueve
**dentro** desde otro directorio, solo se genera `IN_MOVED_TO`.

### El campo name

- **Watch sobre un directorio**: `name` contiene el nombre del archivo
  dentro del directorio que causó el evento. No es una ruta completa,
  solo el nombre base.
- **Watch sobre un archivo**: `name` está vacío (`len == 0`) porque
  el evento es sobre el archivo mismo.

```c
// Watch sobre directorio /var/log/
// Se crea /var/log/app.log
// → event->name == "app.log" (no "/var/log/app.log")

// Watch sobre archivo /etc/passwd
// Se modifica
// → event->len == 0, event->name no existe
```

---

## 7. inotify_rm_watch: dejar de vigilar

```c
#include <sys/inotify.h>

int inotify_rm_watch(int fd, int wd);
```

Elimina un watch. Genera un evento `IN_IGNORED` en la cola:

```c
if (inotify_rm_watch(inotify_fd, wd) == -1) {
    perror("inotify_rm_watch");
}
// Se genera IN_IGNORED para este wd
```

### Eliminación automática

Un watch se elimina automáticamente (con `IN_IGNORED`) cuando:
- El archivo vigilado es borrado (`IN_DELETE_SELF`).
- El filesystem se desmonta (`IN_UNMOUNT`).
- Se usó `IN_ONESHOT` y ya se generó un evento.

Después de `IN_IGNORED`, el `wd` ya no es válido. El kernel puede
reutilizarlo para futuros watches.

---

## 8. Programa completo: monitor de directorio

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/inotify.h>

#define BUF_SIZE (4096)

static void print_event(const struct inotify_event *event) {
    // Tipo de evento
    if (event->mask & IN_CREATE)      printf("CREATE");
    if (event->mask & IN_DELETE)      printf("DELETE");
    if (event->mask & IN_MODIFY)      printf("MODIFY");
    if (event->mask & IN_MOVED_FROM)  printf("MOVED_FROM");
    if (event->mask & IN_MOVED_TO)    printf("MOVED_TO");
    if (event->mask & IN_ATTRIB)      printf("ATTRIB");
    if (event->mask & IN_OPEN)        printf("OPEN");
    if (event->mask & IN_CLOSE_WRITE) printf("CLOSE_WRITE");
    if (event->mask & IN_CLOSE_NOWRITE) printf("CLOSE_NOWRITE");
    if (event->mask & IN_DELETE_SELF) printf("DELETE_SELF");
    if (event->mask & IN_MOVE_SELF)   printf("MOVE_SELF");
    if (event->mask & IN_IGNORED)     printf("IGNORED");
    if (event->mask & IN_Q_OVERFLOW)  printf("Q_OVERFLOW");

    if (event->mask & IN_ISDIR) printf(" [DIR]");

    if (event->cookie > 0)
        printf(" cookie=%u", event->cookie);

    if (event->len > 0)
        printf(" name=%s", event->name);

    printf("\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s directorio [directorio2 ...]\n",
                argv[0]);
        return 1;
    }

    // 1. Crear instancia inotify
    int inotify_fd = inotify_init1(IN_CLOEXEC);
    if (inotify_fd == -1) {
        perror("inotify_init1");
        return 1;
    }

    // 2. Registrar watches para cada directorio
    for (int i = 1; i < argc; i++) {
        int wd = inotify_add_watch(inotify_fd, argv[i],
                    IN_CREATE | IN_DELETE | IN_MODIFY |
                    IN_MOVED_FROM | IN_MOVED_TO |
                    IN_ATTRIB | IN_DELETE_SELF);
        if (wd == -1) {
            fprintf(stderr, "No se puede vigilar '%s': %s\n",
                    argv[i], strerror(errno));
            continue;
        }
        printf("Vigilando %s (wd=%d)\n", argv[i], wd);
    }

    // 3. Leer eventos en bucle
    printf("Esperando eventos (Ctrl+C para salir)...\n\n");

    char buf[BUF_SIZE]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    for (;;) {
        ssize_t len = read(inotify_fd, buf, sizeof(buf));
        if (len == -1) {
            if (errno == EINTR) continue;
            perror("read");
            break;
        }

        const struct inotify_event *event;
        for (char *ptr = buf; ptr < buf + len;
             ptr += sizeof(struct inotify_event) + event->len) {

            event = (const struct inotify_event *)ptr;
            printf("  wd=%d ", event->wd);
            print_event(event);
        }
    }

    close(inotify_fd);
    return 0;
}
```

### Ejemplo de uso

```bash
$ gcc -o inotify_monitor monitor.c
$ ./inotify_monitor /tmp/test/

# En otra terminal:
$ mkdir /tmp/test
$ touch /tmp/test/file.txt
$ echo "hello" > /tmp/test/file.txt
$ mv /tmp/test/file.txt /tmp/test/renamed.txt
$ rm /tmp/test/renamed.txt

# Output del monitor:
Vigilando /tmp/test/ (wd=1)
Esperando eventos...

  wd=1 CREATE name=file.txt
  wd=1 OPEN name=file.txt
  wd=1 ATTRIB name=file.txt
  wd=1 CLOSE_WRITE name=file.txt
  wd=1 OPEN name=file.txt
  wd=1 MODIFY name=file.txt
  wd=1 CLOSE_WRITE name=file.txt
  wd=1 MOVED_FROM cookie=54321 name=file.txt
  wd=1 MOVED_TO cookie=54321 name=renamed.txt
  wd=1 DELETE name=renamed.txt
```

### Detalle: lo que genera un simple "echo > file"

```bash
$ echo "hello" > /tmp/test/file.txt

# Genera esta secuencia:
OPEN           name=file.txt    # shell abre el archivo
MODIFY         name=file.txt    # write("hello\n")
CLOSE_WRITE    name=file.txt    # close()
```

Un editor como `vim` genera muchos más eventos porque crea archivos
temporales, renombra, etc.

---

## 9. Integración con poll/epoll

El `inotify_fd` es un fd normal, compatible con los mecanismos de
multiplexación de I/O:

### Con poll

```c
#include <poll.h>

struct pollfd fds[2];

// inotify
fds[0].fd = inotify_fd;
fds[0].events = POLLIN;

// Otro fd (e.g., stdin para comandos)
fds[1].fd = STDIN_FILENO;
fds[1].events = POLLIN;

while (1) {
    int ret = poll(fds, 2, -1);  // bloquear indefinidamente
    if (ret == -1) {
        if (errno == EINTR) continue;
        perror("poll");
        break;
    }

    if (fds[0].revents & POLLIN) {
        // Hay eventos inotify — leerlos
        ssize_t len = read(inotify_fd, buf, sizeof(buf));
        // procesar eventos...
    }

    if (fds[1].revents & POLLIN) {
        // Hay input del usuario
        // procesar comandos...
    }
}
```

### Con epoll

```c
#include <sys/epoll.h>

int epoll_fd = epoll_create1(EPOLL_CLOEXEC);

struct epoll_event ev;
ev.events = EPOLLIN;
ev.data.fd = inotify_fd;
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, inotify_fd, &ev);

struct epoll_event events[10];
while (1) {
    int n = epoll_wait(epoll_fd, events, 10, -1);
    for (int i = 0; i < n; i++) {
        if (events[i].data.fd == inotify_fd) {
            // Leer y procesar eventos inotify
        }
    }
}
```

### ¿Por qué usar poll/epoll?

```
  Sin poll/epoll:
  read(inotify_fd) → bloquea hasta que haya eventos
  → No puedes hacer NADA más mientras esperas

  Con poll/epoll:
  poll({inotify_fd, stdin, socket_fd, timer_fd}, ...)
  → Puedes esperar eventos inotify Y comandos del usuario
    Y datos de red Y timers, TODO en un solo hilo
```

### Tamaño de la cola de eventos

La cola de inotify tiene un límite configurable:

```bash
# Máximo de eventos en cola (por instancia inotify):
$ cat /proc/sys/fs/inotify/max_queued_events
16384

# Si la cola se llena, se genera IN_Q_OVERFLOW y se pierden eventos
```

```c
// Detectar overflow:
if (event->mask & IN_Q_OVERFLOW) {
    fprintf(stderr, "¡Cola de inotify desbordada! "
                    "Se perdieron eventos.\n");
    // Opción: re-escanear los directorios vigilados
}
```

---

## 10. Errores comunes

### Error 1: esperar que inotify sea recursivo

```c
// MAL: esperar que vigilar /var/log detecte cambios en subdirectorios
inotify_add_watch(fd, "/var/log", IN_CREATE);
// Solo detecta archivos creados DIRECTAMENTE en /var/log/
// NO detecta /var/log/nginx/access.log

// BIEN: agregar watches para cada subdirectorio
void watch_recursive(int inotify_fd, const char *path) {
    inotify_add_watch(inotify_fd, path,
                      IN_CREATE | IN_DELETE | IN_MODIFY);

    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char child[PATH_MAX];
        snprintf(child, sizeof(child), "%s/%s",
                 path, entry->d_name);

        struct stat sb;
        if (lstat(child, &sb) == 0 && S_ISDIR(sb.st_mode)) {
            watch_recursive(inotify_fd, child);
        }
    }
    closedir(dir);
}

// TAMBIÉN: detectar IN_CREATE + IN_ISDIR para agregar watches
// a subdirectorios creados después de iniciar el monitoreo
```

### Error 2: no manejar IN_Q_OVERFLOW

```c
// MAL: ignorar overflow
// Si la cola se llena, se pierden eventos silenciosamente

// BIEN: detectar y recuperar
if (event->mask & IN_Q_OVERFLOW) {
    fprintf(stderr, "Eventos perdidos — re-escaneando\n");
    // Recorrer los directorios vigilados con opendir/readdir
    // para detectar cambios que se perdieron
}
```

### Error 3: no manejar rename de directorio vigilado

```c
// Cuando un directorio vigilado se renombra:
// - El watch sigue asociado al INODO (no al nombre)
// - El wd sigue siendo válido
// - Pero tu mapping wd → path ahora es incorrecto

// BIEN: mantener un mapa wd → path y actualizarlo en IN_MOVE_SELF
if (event->mask & IN_MOVE_SELF) {
    printf("wd=%d fue movido — actualizar path\n", event->wd);
    // Necesitas algún mecanismo para descubrir la nueva ruta
}
```

### Error 4: no cerrar el inotify_fd

```c
// MAL: leak de watches y del fd de inotify
int fd = inotify_init1(IN_CLOEXEC);
inotify_add_watch(fd, "/tmp", IN_CREATE);
// ... programa termina sin close(fd)
// El kernel limpia en exit(), pero es mala práctica

// BIEN: cerrar explícitamente
close(inotify_fd);
// Esto elimina TODOS los watches asociados
```

### Error 5: buffer demasiado pequeño para read

```c
// MAL: buffer que puede no caber un solo evento
char buf[16];  // ¡sizeof(inotify_event) ya es 16 bytes!
read(inotify_fd, buf, sizeof(buf));
// Si name[] tiene contenido, el evento no cabe

// BIEN: buffer generoso
// Mínimo: sizeof(struct inotify_event) + NAME_MAX + 1
// Práctico: 4096 o más
char buf[4096]
    __attribute__((aligned(__alignof__(struct inotify_event))));
```

El tamaño mínimo para un evento con nombre es:
`sizeof(struct inotify_event) + NAME_MAX + 1` = 16 + 255 + 1 = 272 bytes.
Pero un `read` puede devolver múltiples eventos, así que un buffer de
4096 bytes es lo recomendado.

---

## 11. Cheatsheet

```
  CREAR / DESTRUIR
  ──────────────────────────────────────────────────────────
  inotify_init1(IN_CLOEXEC)          crear instancia
  inotify_add_watch(fd, path, mask)  agregar watch → wd
  inotify_rm_watch(fd, wd)           eliminar watch
  close(fd)                          cerrar todo

  EVENTOS PRINCIPALES
  ──────────────────────────────────────────────────────────
  IN_CREATE         archivo creado en dir vigilado
  IN_DELETE         archivo borrado en dir vigilado
  IN_MODIFY         datos modificados
  IN_ATTRIB         metadatos cambiados (chmod, chown)
  IN_MOVED_FROM     archivo salió del dir (rename)
  IN_MOVED_TO       archivo entró al dir (rename)
  IN_OPEN           archivo abierto
  IN_CLOSE_WRITE    cerrado (era escritura)
  IN_CLOSE_NOWRITE  cerrado (era lectura)
  IN_DELETE_SELF    el objeto vigilado fue borrado
  IN_MOVE_SELF      el objeto vigilado fue movido

  MACROS COMBINADAS
  ──────────────────────────────────────────────────────────
  IN_CLOSE          CLOSE_WRITE | CLOSE_NOWRITE
  IN_MOVE           MOVED_FROM | MOVED_TO
  IN_ALL_EVENTS     todos los anteriores

  FLAGS DE MÁSCARA
  ──────────────────────────────────────────────────────────
  IN_MASK_ADD       agregar a máscara (no reemplazar)
  IN_ONESHOT        auto-eliminar tras primer evento
  IN_ONLYDIR        solo aceptar directorios
  IN_DONT_FOLLOW    no seguir symlinks
  IN_EXCL_UNLINK    ignorar archivos ya borrados

  EVENTOS AUTOMÁTICOS
  ──────────────────────────────────────────────────────────
  IN_ISDIR          evento sobre subdirectorio
  IN_IGNORED        watch eliminado
  IN_Q_OVERFLOW     cola llena — eventos perdidos
  IN_UNMOUNT        filesystem desmontado

  struct inotify_event
  ──────────────────────────────────────────────────────────
  wd                watch descriptor
  mask              bits de evento
  cookie            vincula MOVED_FROM ↔ MOVED_TO
  len               longitud de name[] (con padding)
  name[]            nombre del archivo (solo en dirs)

  LÍMITES DEL SISTEMA
  ──────────────────────────────────────────────────────────
  /proc/sys/fs/inotify/max_user_watches     (default 8192)
  /proc/sys/fs/inotify/max_user_instances   (default 128)
  /proc/sys/fs/inotify/max_queued_events    (default 16384)

  RECORRER EVENTOS EN EL BUFFER
  ──────────────────────────────────────────────────────────
  for (char *p = buf; p < buf + len;
       p += sizeof(struct inotify_event) + event->len) {
      event = (struct inotify_event *)p;
  }
```

---

## 12. Ejercicios

### Ejercicio 1: monitor de directorio con log

Escribe un programa que vigile un directorio e imprima cada evento
con timestamp, tipo y nombre del archivo afectado, en formato de log:

```
[2026-03-19 14:30:01] CREATE  file.txt
[2026-03-19 14:30:02] MODIFY  file.txt
[2026-03-19 14:30:02] CLOSE_WRITE  file.txt
[2026-03-19 14:30:05] RENAME  old.txt → new.txt
[2026-03-19 14:30:10] DELETE  new.txt
```

```c
// Esqueleto:
// 1. inotify_init1(IN_CLOEXEC)
// 2. inotify_add_watch con IN_CREATE|IN_DELETE|IN_MODIFY|
//    IN_MOVED_FROM|IN_MOVED_TO|IN_CLOSE_WRITE
// 3. Leer eventos en bucle
// 4. Para RENAME: usar cookie para vincular FROM y TO
//    - Guardar el último IN_MOVED_FROM con su cookie
//    - Cuando llega IN_MOVED_TO con mismo cookie, imprimir juntos
// 5. Formatear timestamp con strftime
```

Requisitos:
- Manejar `IN_MOVED_FROM`/`IN_MOVED_TO` como un solo evento `RENAME`.
- Si `IN_MOVED_FROM` no tiene `IN_MOVED_TO` correspondiente, reportar como
  `MOVED_OUT`. Si `IN_MOVED_TO` sin `FROM`, reportar como `MOVED_IN`.
- Opcionalmente escribir el log a un archivo con `-o logfile`.

**Pregunta de reflexión**: si vigilas un directorio y alguien ejecuta
`vim file.txt`, ¿cuántos eventos se generan? Pruébalo. ¿Por qué `vim`
genera tantos eventos comparado con `echo "text" > file.txt`? ¿Qué
estrategia usa `vim` para guardar archivos de forma segura?

---

### Ejercicio 2: hot-reload de configuración

Escribe un daemon que lea un archivo de configuración al inicio y lo
recargue automáticamente cuando detecte que fue modificado:

```c
// Esqueleto:
#define CONFIG_PATH "/tmp/myapp.conf"

typedef struct {
    int port;
    int max_connections;
    char log_level[16];
} config_t;

config_t current_config;

int parse_config(const char *path, config_t *cfg) {
    // Leer archivo línea por línea
    // Formato: key=value
    // port=8080
    // max_connections=100
    // log_level=info
}

int main(void) {
    parse_config(CONFIG_PATH, &current_config);
    printf("Config cargada: port=%d\n", current_config.port);

    int inotify_fd = inotify_init1(IN_CLOEXEC);

    // ¿Vigilar el archivo o el directorio?
    // (pista: vim/emacs borran y recrean el archivo)

    // Bucle principal:
    // - Esperar eventos con poll (con timeout para trabajo periódico)
    // - Si hay IN_MODIFY o IN_CLOSE_WRITE: recargar
    // - Si hay IN_DELETE_SELF: re-registrar watch
}
```

Requisitos:
- Vigilar el **directorio** padre, no solo el archivo (por editores que
  borran y recrean).
- Filtrar eventos: solo recargar cuando el evento es sobre el archivo
  de configuración específico.
- Imprimir la configuración anterior y nueva al recargar.
- No recargar múltiples veces por una sola edición (debounce: esperar
  100ms después del último evento antes de recargar).

**Pregunta de reflexión**: ¿por qué no basta con vigilar el archivo
directamente con `inotify_add_watch(fd, CONFIG_PATH, IN_MODIFY)`?
¿Qué hace `vim` internamente que rompe esta estrategia? (Pista: `vim`
escribe a un temporal y luego hace `rename`.)

---

### Ejercicio 3: sincronizador de directorios

Escribe un programa que detecte archivos nuevos en un directorio "source"
y los copie automáticamente a un directorio "dest":

```c
// Uso: ./sync_dir /tmp/source /tmp/dest

// Esqueleto:
// 1. Verificar que ambos directorios existen
// 2. Copiar archivos existentes de source a dest (sync inicial)
// 3. inotify_add_watch en source: IN_CREATE | IN_CLOSE_WRITE |
//    IN_MOVED_TO | IN_DELETE
// 4. Para cada IN_CREATE o IN_CLOSE_WRITE: copiar a dest
//    (esperar IN_CLOSE_WRITE, no copiar en IN_CREATE porque
//     el archivo puede estar siendo escrito todavía)
// 5. Para cada IN_DELETE: borrar de dest
```

Requisitos:
- Solo copiar archivos regulares (no subdirectorios).
- Usar `IN_CLOSE_WRITE` como señal de "archivo listo para copiar",
  no `IN_CREATE` (el archivo puede estar vacío en el momento de la
  creación).
- Copiar preservando permisos.
- Manejar `IN_DELETE` eliminando la copia en dest.

**Pregunta de reflexión**: este programa tiene un race condition sutil.
Si un archivo se crea y se modifica antes de que el programa empiece
a procesar los eventos, ¿se copia correctamente? ¿Qué pasa si la cola
de inotify se llena (`IN_Q_OVERFLOW`) durante una ráfaga de creaciones?
¿Cómo diseñarías una estrategia de recuperación?

---
