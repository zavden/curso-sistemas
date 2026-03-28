# Casos de Uso de inotify

## Índice

1. [Hot reload de configuración](#1-hot-reload-de-configuración)
2. [Build watcher: recompilar al guardar](#2-build-watcher-recompilar-al-guardar)
3. [Sincronización de archivos](#3-sincronización-de-archivos)
4. [Rotación y monitoreo de logs](#4-rotación-y-monitoreo-de-logs)
5. [Vigilancia de seguridad](#5-vigilancia-de-seguridad)
6. [Debouncing: agrupar ráfagas de eventos](#6-debouncing-agrupar-ráfagas-de-eventos)
7. [Monitoreo recursivo de directorios](#7-monitoreo-recursivo-de-directorios)
8. [Integración con el event loop](#8-integración-con-el-event-loop)
9. [Software real que usa inotify](#9-software-real-que-usa-inotify)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Hot reload de configuración

El caso de uso más clásico: un daemon que recarga su configuración
automáticamente cuando el archivo cambia, sin reiniciar el servicio.

### El problema de los editores

Los editores de texto no simplemente escriben al archivo original.
Cada uno tiene su estrategia:

```
  echo "data" > config.ini:
    OPEN → MODIFY → CLOSE_WRITE

  vim config.ini (guardar con :w):
    CREATE  config.ini.swp       ← archivo temporal
    MODIFY  config.ini.swp
    DELETE  config.ini            ← borra el original
    MOVED_TO config.ini           ← renombra .swp → original
    → El watch sobre config.ini muere (IN_DELETE_SELF)

  sed -i 's/old/new/' config.ini:
    CREATE  sedXXXXXX             ← temporal
    MODIFY  sedXXXXXX
    MOVED_TO config.ini           ← reemplaza atómicamente
    → Mismo problema: inodo nuevo

  nano config.ini:
    OPEN → MODIFY → CLOSE_WRITE  ← edición in-place (simple)
```

### Solución robusta: vigilar el directorio padre

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/inotify.h>
#include <poll.h>

#define BUF_SIZE 4096

typedef struct {
    char key[64];
    char value[256];
} config_entry_t;

typedef struct {
    config_entry_t entries[64];
    int count;
} config_t;

int load_config(const char *path, config_t *cfg) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    cfg->count = 0;
    char line[320];
    while (fgets(line, sizeof(line), f) && cfg->count < 64) {
        // Ignorar comentarios y líneas vacías
        if (line[0] == '#' || line[0] == '\n') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        config_entry_t *e = &cfg->entries[cfg->count];
        snprintf(e->key, sizeof(e->key), "%s", line);
        snprintf(e->value, sizeof(e->value), "%s", eq + 1);

        // Eliminar newline del valor
        char *nl = strchr(e->value, '\n');
        if (nl) *nl = '\0';

        cfg->count++;
    }
    fclose(f);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *config_path = (argc > 1) ? argv[1] : "config.ini";

    // Separar directorio y nombre base
    char *path_copy = strdup(config_path);
    char *dir_copy = strdup(config_path);
    const char *filename = basename(path_copy);
    const char *dirpath = dirname(dir_copy);

    config_t config;
    if (load_config(config_path, &config) == -1) {
        fprintf(stderr, "No se pudo cargar %s\n", config_path);
        return 1;
    }
    printf("Config cargada: %d entradas\n", config.count);

    int inotify_fd = inotify_init1(IN_CLOEXEC);
    if (inotify_fd == -1) { perror("inotify_init1"); return 1; }

    // Vigilar el DIRECTORIO, no el archivo
    int wd = inotify_add_watch(inotify_fd, dirpath,
                IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);
    if (wd == -1) { perror("inotify_add_watch"); return 1; }

    printf("Vigilando %s por cambios en %s\n", dirpath, filename);

    struct pollfd pfd = { .fd = inotify_fd, .events = POLLIN };
    char buf[BUF_SIZE]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    for (;;) {
        int ret = poll(&pfd, 1, -1);
        if (ret == -1) {
            if (errno == EINTR) continue;
            break;
        }

        ssize_t len = read(inotify_fd, buf, sizeof(buf));
        if (len <= 0) continue;

        int should_reload = 0;
        const struct inotify_event *event;

        for (char *ptr = buf; ptr < buf + len;
             ptr += sizeof(struct inotify_event) + event->len) {
            event = (const struct inotify_event *)ptr;

            // Solo nos interesa nuestro archivo
            if (event->len > 0 &&
                strcmp(event->name, filename) == 0) {
                if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) {
                    should_reload = 1;
                }
            }
        }

        if (should_reload) {
            printf("Cambio detectado — recargando...\n");
            config_t new_config;
            if (load_config(config_path, &new_config) == 0) {
                config = new_config;
                printf("Recargado: %d entradas\n", config.count);
            } else {
                fprintf(stderr, "Error al recargar — "
                        "manteniendo config anterior\n");
            }
        }
    }

    free(path_copy);
    free(dir_copy);
    close(inotify_fd);
    return 0;
}
```

### Por qué IN_CLOSE_WRITE y no IN_MODIFY

```
  IN_MODIFY:
  - Se genera en CADA write()
  - Si un editor hace 10 writes, recibes 10 eventos
  - Puedes recargar con datos a medio escribir

  IN_CLOSE_WRITE:
  - Se genera cuando el escritor CIERRA el archivo
  - Garantiza que la escritura terminó
  - Un solo evento por ciclo de edición

  IN_MOVED_TO:
  - Se genera cuando un rename() reemplaza el archivo
  - Cubre el caso vim/sed -i (rename atómico)
```

La combinación `IN_CLOSE_WRITE | IN_MOVED_TO` cubre todos los
patrones de edición.

---

## 2. Build watcher: recompilar al guardar

Detectar cambios en archivos fuente y ejecutar la compilación
automáticamente:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <poll.h>
#include <time.h>

#define BUF_SIZE 4096
#define DEBOUNCE_MS 200

static int has_extension(const char *name, const char *ext) {
    size_t nlen = strlen(name);
    size_t elen = strlen(ext);
    if (nlen < elen) return 0;
    return strcmp(name + nlen - elen, ext) == 0;
}

static void run_build(const char *cmd) {
    printf("\n\033[1;33m>>> Compilando: %s\033[0m\n", cmd);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int status = system(cmd);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) * 1e-9;

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("\033[1;32m>>> OK (%.2fs)\033[0m\n\n", elapsed);
    } else {
        printf("\033[1;31m>>> FALLÓ (%.2fs)\033[0m\n\n", elapsed);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr,
            "Uso: %s directorio \"comando de build\"\n"
            "Ej:  %s src/ \"gcc -o prog src/*.c\"\n",
            argv[0], argv[0]);
        return 1;
    }

    const char *watch_dir = argv[1];
    const char *build_cmd = argv[2];

    int inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotify_fd == -1) { perror("inotify_init1"); return 1; }

    int wd = inotify_add_watch(inotify_fd, watch_dir,
                IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);
    if (wd == -1) { perror("watch"); return 1; }

    printf("Vigilando %s — build: %s\n", watch_dir, build_cmd);

    // Build inicial
    run_build(build_cmd);

    struct pollfd pfd = { .fd = inotify_fd, .events = POLLIN };
    char buf[BUF_SIZE]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    for (;;) {
        int ret = poll(&pfd, 1, -1);
        if (ret == -1) { if (errno == EINTR) continue; break; }

        ssize_t len = read(inotify_fd, buf, sizeof(buf));
        if (len <= 0) continue;

        int trigger = 0;
        const struct inotify_event *event;
        for (char *ptr = buf; ptr < buf + len;
             ptr += sizeof(struct inotify_event) + event->len) {
            event = (const struct inotify_event *)ptr;

            if (event->len > 0 &&
                (has_extension(event->name, ".c") ||
                 has_extension(event->name, ".h"))) {
                printf("  Cambio: %s\n", event->name);
                trigger = 1;
            }
        }

        if (trigger) {
            // Debounce: drenar eventos adicionales
            usleep(DEBOUNCE_MS * 1000);
            while (read(inotify_fd, buf, sizeof(buf)) > 0)
                ;  // descartar eventos acumulados

            run_build(build_cmd);
        }
    }

    close(inotify_fd);
    return 0;
}
```

### Uso

```bash
$ ./buildwatch src/ "gcc -Wall -o myapp src/*.c"
Vigilando src/ — build: gcc -Wall -o myapp src/*.c
>>> Compilando: gcc -Wall -o myapp src/*.c
>>> OK (0.12s)

# Editar src/main.c en otro terminal...
  Cambio: main.c
>>> Compilando: gcc -Wall -o myapp src/*.c
>>> OK (0.11s)
```

Este es el principio detrás de herramientas como `entr`, `watchexec`
y `nodemon`.

---

## 3. Sincronización de archivos

Detectar archivos nuevos o modificados y copiarlos a un directorio
destino:

```c
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <poll.h>
#include <limits.h>

#define BUF_SIZE 4096

static int copy_file(const char *src, const char *dst) {
    int sfd = open(src, O_RDONLY);
    if (sfd == -1) return -1;

    struct stat sb;
    fstat(sfd, &sb);

    int dfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, sb.st_mode);
    if (dfd == -1) { close(sfd); return -1; }

    char buf[65536];
    ssize_t n;
    while ((n = read(sfd, buf, sizeof(buf))) > 0) {
        write(dfd, buf, n);
    }

    close(dfd);
    close(sfd);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s source_dir dest_dir\n", argv[0]);
        return 1;
    }

    const char *src_dir = argv[1];
    const char *dst_dir = argv[2];

    int inotify_fd = inotify_init1(IN_CLOEXEC);
    int wd = inotify_add_watch(inotify_fd, src_dir,
                IN_CLOSE_WRITE | IN_MOVED_TO | IN_DELETE);

    printf("Sincronizando %s → %s\n", src_dir, dst_dir);

    char buf[BUF_SIZE]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    for (;;) {
        ssize_t len = read(inotify_fd, buf, sizeof(buf));
        if (len <= 0) continue;

        const struct inotify_event *event;
        for (char *ptr = buf; ptr < buf + len;
             ptr += sizeof(struct inotify_event) + event->len) {
            event = (const struct inotify_event *)ptr;

            if (event->len == 0) continue;
            if (event->mask & IN_ISDIR) continue;  // solo archivos

            char src_path[PATH_MAX], dst_path[PATH_MAX];
            snprintf(src_path, sizeof(src_path), "%s/%s",
                     src_dir, event->name);
            snprintf(dst_path, sizeof(dst_path), "%s/%s",
                     dst_dir, event->name);

            if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) {
                printf("  SYNC %s\n", event->name);
                if (copy_file(src_path, dst_path) == -1) {
                    perror(dst_path);
                }
            }

            if (event->mask & IN_DELETE) {
                printf("  DEL  %s\n", event->name);
                unlink(dst_path);  // ignorar error si no existe
            }
        }
    }

    close(inotify_fd);
    return 0;
}
```

### Diferencia con rsync

```
  inotify sync:                rsync:
  - Reacciona instantáneamente - Escanea todo el directorio
  - Solo procesa lo que cambió - Compara checksums/timestamps
  - Mantiene estado en memoria - Sin estado (ejecuta y sale)
  - Puede perder eventos       - Siempre completo
    (IN_Q_OVERFLOW)            - Funciona en NFS/remoto
  - Solo local                 - Local y remoto
```

En la práctica, herramientas como `lsyncd` combinan inotify para
detección rápida con rsync para la transferencia confiable.

---

## 4. Rotación y monitoreo de logs

### tail -f: seguir un archivo que crece

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>

#define BUF_SIZE 4096

int tail_follow(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) { perror("open"); return -1; }

    // Posicionar al final
    lseek(fd, 0, SEEK_END);

    int inotify_fd = inotify_init1(IN_CLOEXEC);
    int wd = inotify_add_watch(inotify_fd, path, IN_MODIFY);

    char read_buf[8192];
    char event_buf[BUF_SIZE]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    printf("=== Siguiendo %s ===\n", path);

    for (;;) {
        // Esperar evento de modificación
        ssize_t elen = read(inotify_fd, event_buf, sizeof(event_buf));
        if (elen <= 0) continue;

        // Leer datos nuevos
        ssize_t n;
        while ((n = read(fd, read_buf, sizeof(read_buf))) > 0) {
            write(STDOUT_FILENO, read_buf, n);
        }
    }

    close(fd);
    close(inotify_fd);
    return 0;
}
```

### Manejar rotación de logs

Los sistemas de logging (logrotate) rotan los archivos:
`app.log` → `app.log.1` y crean un nuevo `app.log`. El `tail -f`
real necesita detectar esta rotación:

```c
// Detectar rotación: vigilar el directorio del log
int dir_wd = inotify_add_watch(inotify_fd, log_dir,
                IN_CREATE | IN_MOVED_TO);

// Cuando detectamos que se creó un archivo con el mismo nombre:
if (event->mask & IN_CREATE &&
    strcmp(event->name, log_filename) == 0) {
    // Rotación detectada:
    // 1. Leer lo que quede del fd antiguo
    // 2. Cerrar el fd antiguo
    // 3. Abrir el archivo nuevo
    // 4. Actualizar el watch si era sobre el archivo
    printf("--- Rotación detectada ---\n");
    close(log_fd);
    log_fd = open(log_path, O_RDONLY);
}
```

### Comparación: tail -f vs tail --follow=name

```
  tail -f (--follow=descriptor):
  - Sigue el fd (inodo)
  - Si el archivo se rota (rename+create), sigue leyendo
    el archivo viejo
  - No detecta el archivo nuevo

  tail -F (--follow=name):
  - Sigue el NOMBRE del archivo
  - Si el archivo se rota, reabre por nombre
  - Detecta rotación y cambia al archivo nuevo
  - Implementado con inotify en GNU tail
```

---

## 5. Vigilancia de seguridad

Detectar modificaciones no autorizadas en archivos críticos:

```c
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/inotify.h>
#include <unistd.h>

#define BUF_SIZE 4096

static const char *critical_files[] = {
    "/etc/passwd",
    "/etc/shadow",
    "/etc/sudoers",
    "/etc/ssh/sshd_config",
    NULL
};

static void log_alert(const char *path, const char *action) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);

    fprintf(stderr,
        "[%s] ALERTA: %s fue %s\n", ts, path, action);
}

int main(void) {
    int inotify_fd = inotify_init1(IN_CLOEXEC);
    if (inotify_fd == -1) { perror("inotify_init1"); return 1; }

    // Tabla: wd → path
    struct { int wd; const char *path; } watches[32];
    int nwatches = 0;

    for (int i = 0; critical_files[i]; i++) {
        int wd = inotify_add_watch(inotify_fd, critical_files[i],
                    IN_MODIFY | IN_ATTRIB | IN_DELETE_SELF |
                    IN_MOVE_SELF);
        if (wd == -1) {
            fprintf(stderr, "No se puede vigilar %s: %s\n",
                    critical_files[i], strerror(errno));
            continue;
        }
        watches[nwatches].wd = wd;
        watches[nwatches].path = critical_files[i];
        nwatches++;
    }

    printf("Vigilancia de seguridad activa (%d archivos)\n",
           nwatches);

    char buf[BUF_SIZE]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    for (;;) {
        ssize_t len = read(inotify_fd, buf, sizeof(buf));
        if (len <= 0) continue;

        const struct inotify_event *event;
        for (char *ptr = buf; ptr < buf + len;
             ptr += sizeof(struct inotify_event) + event->len) {
            event = (const struct inotify_event *)ptr;

            // Buscar a qué path corresponde este wd
            const char *path = "desconocido";
            for (int i = 0; i < nwatches; i++) {
                if (watches[i].wd == event->wd) {
                    path = watches[i].path;
                    break;
                }
            }

            if (event->mask & IN_MODIFY)
                log_alert(path, "MODIFICADO");
            if (event->mask & IN_ATTRIB)
                log_alert(path, "ATRIBUTOS CAMBIADOS");
            if (event->mask & IN_DELETE_SELF)
                log_alert(path, "BORRADO");
            if (event->mask & IN_MOVE_SELF)
                log_alert(path, "MOVIDO/RENOMBRADO");
        }
    }

    close(inotify_fd);
    return 0;
}
```

Software real como **AIDE**, **OSSEC** y **Tripwire** usan mecanismos
similares para detección de intrusiones (aunque AIDE y Tripwire
comparan checksums periódicamente en lugar de usar inotify).

---

## 6. Debouncing: agrupar ráfagas de eventos

Un solo `Ctrl+S` en un editor puede generar 5-10 eventos. Sin
debouncing, tu callback se ejecuta múltiples veces innecesariamente:

```
  vim :w genera:
  t=0ms    IN_CREATE  .file.swp
  t=1ms    IN_MODIFY  .file.swp
  t=2ms    IN_MOVED_TO file.txt      ← este es el que importa
  t=3ms    IN_DELETE  .file.swp

  Sin debounce: 4 ejecuciones del callback
  Con debounce: 1 ejecución (después de la ráfaga)
```

### Implementación con timerfd

```c
#include <stdio.h>
#include <sys/inotify.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <unistd.h>
#include <stdint.h>

#define BUF_SIZE 4096
#define DEBOUNCE_MS 300

int main(void) {
    int inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    inotify_add_watch(inotify_fd, ".", IN_CLOSE_WRITE | IN_MOVED_TO);

    // Timer para debounce
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);

    struct pollfd fds[2] = {
        { .fd = inotify_fd, .events = POLLIN },
        { .fd = timer_fd,   .events = POLLIN },
    };

    int pending = 0;
    char buf[BUF_SIZE]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    printf("Vigilando directorio actual...\n");

    for (;;) {
        int ret = poll(fds, 2, -1);
        if (ret == -1) continue;

        // Evento inotify: reiniciar timer
        if (fds[0].revents & POLLIN) {
            // Drenar eventos
            while (read(inotify_fd, buf, sizeof(buf)) > 0)
                ;

            // (Re)iniciar timer de debounce
            struct itimerspec ts = {
                .it_value = {
                    .tv_sec = 0,
                    .tv_nsec = DEBOUNCE_MS * 1000000L
                }
            };
            timerfd_settime(timer_fd, 0, &ts, NULL);
            pending = 1;
        }

        // Timer expiró: ejecutar acción
        if (fds[1].revents & POLLIN) {
            uint64_t expirations;
            read(timer_fd, &expirations, sizeof(expirations));

            if (pending) {
                printf("  Cambio detectado — ejecutando acción\n");
                // Aquí: recompilar, recargar, sincronizar, etc.
                pending = 0;
            }
        }
    }

    close(inotify_fd);
    close(timer_fd);
    return 0;
}
```

### Estrategias de debounce

```
  Estrategia          Comportamiento
  ──────────────────────────────────────────────────────────
  Trailing edge       ejecutar DESPUÉS de que los eventos
  (la más común)      paren por N ms

  Leading edge        ejecutar en el PRIMER evento,
                      ignorar por N ms

  Throttle            ejecutar como máximo cada N ms
                      (no espera a que los eventos paren)
```

La implementación con timerfd es trailing edge: cada evento nuevo
reinicia el timer. La acción se ejecuta solo cuando pasan N ms sin
eventos nuevos.

---

## 7. Monitoreo recursivo de directorios

inotify no es recursivo por diseño. Para monitorear un árbol completo:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <limits.h>

#define MAX_WATCHES 8192
#define BUF_SIZE 4096

// Tabla wd → path
static struct {
    int wd;
    char path[PATH_MAX];
} watch_table[MAX_WATCHES];
static int num_watches = 0;

static const char *wd_to_path(int wd) {
    for (int i = 0; i < num_watches; i++) {
        if (watch_table[i].wd == wd)
            return watch_table[i].path;
    }
    return "???";
}

static int add_watch(int inotify_fd, const char *path, uint32_t mask) {
    int wd = inotify_add_watch(inotify_fd, path, mask);
    if (wd == -1) return -1;

    if (num_watches < MAX_WATCHES) {
        watch_table[num_watches].wd = wd;
        snprintf(watch_table[num_watches].path, PATH_MAX,
                 "%s", path);
        num_watches++;
    }
    return wd;
}

static void watch_recursive(int inotify_fd, const char *base,
                             uint32_t mask) {
    add_watch(inotify_fd, base, mask);

    DIR *dir = opendir(base);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s",
                 base, entry->d_name);

        struct stat sb;
        if (lstat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
            watch_recursive(inotify_fd, path, mask);
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    const char *root = (argc > 1) ? argv[1] : ".";

    int inotify_fd = inotify_init1(IN_CLOEXEC);
    uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY |
                    IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE;

    watch_recursive(inotify_fd, root, mask);
    printf("Vigilando %d directorios bajo %s\n", num_watches, root);

    char buf[BUF_SIZE]
        __attribute__((aligned(__alignof__(struct inotify_event))));

    for (;;) {
        ssize_t len = read(inotify_fd, buf, sizeof(buf));
        if (len <= 0) continue;

        const struct inotify_event *event;
        for (char *ptr = buf; ptr < buf + len;
             ptr += sizeof(struct inotify_event) + event->len) {
            event = (const struct inotify_event *)ptr;

            const char *dir = wd_to_path(event->wd);

            // Si se creó un subdirectorio, agregar watch
            if ((event->mask & IN_CREATE) &&
                (event->mask & IN_ISDIR)) {
                char new_dir[PATH_MAX];
                snprintf(new_dir, sizeof(new_dir), "%s/%s",
                         dir, event->name);
                add_watch(inotify_fd, new_dir, mask);
                printf("  + Watch nuevo: %s\n", new_dir);
            }

            if (event->len > 0) {
                printf("  %s/%s\n", dir, event->name);
            }
        }
    }

    close(inotify_fd);
    return 0;
}
```

### Límite de watches

```bash
# Ver límite actual:
$ cat /proc/sys/fs/inotify/max_user_watches
8192

# Aumentar (requiere root):
$ echo 524288 | sudo tee /proc/sys/fs/inotify/max_user_watches

# Hacer permanente:
$ echo 'fs.inotify.max_user_watches=524288' | \
    sudo tee -a /etc/sysctl.d/99-inotify.conf
$ sudo sysctl --system
```

IDEs como VS Code necesitan muchos watches. El error típico:
`ENOSPC: System limit for number of file watchers reached`.

### Costo de memoria de los watches

Cada watch consume ~1 KiB de memoria del kernel (no swappable).
Con 524288 watches: ~512 MiB de memoria del kernel.

---

## 8. Integración con el event loop

### Patrón: inotify + señales + timers

```c
#include <stdio.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/inotify.h>
#include <poll.h>
#include <unistd.h>
#include <stdint.h>

int main(void) {
    // 1. inotify para cambios en filesystem
    int inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    inotify_add_watch(inotify_fd, "/tmp/watch",
                      IN_CREATE | IN_DELETE);

    // 2. signalfd para manejo limpio de señales
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    int signal_fd = signalfd(-1, &mask, SFD_CLOEXEC);

    // 3. timerfd para tareas periódicas
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    struct itimerspec ts = {
        .it_interval = { .tv_sec = 5 },  // cada 5 segundos
        .it_value = { .tv_sec = 5 }
    };
    timerfd_settime(timer_fd, 0, &ts, NULL);

    // Event loop unificado
    struct pollfd fds[] = {
        { .fd = inotify_fd, .events = POLLIN },
        { .fd = signal_fd,  .events = POLLIN },
        { .fd = timer_fd,   .events = POLLIN },
    };

    printf("Event loop iniciado\n");

    int running = 1;
    while (running) {
        int n = poll(fds, 3, -1);
        if (n == -1) continue;

        if (fds[0].revents & POLLIN) {
            // Procesar eventos inotify
            char buf[4096];
            read(inotify_fd, buf, sizeof(buf));
            printf("  Cambio en filesystem\n");
        }

        if (fds[1].revents & POLLIN) {
            // Señal recibida
            struct signalfd_siginfo si;
            read(signal_fd, &si, sizeof(si));
            printf("  Señal %d — saliendo\n", si.ssi_signo);
            running = 0;
        }

        if (fds[2].revents & POLLIN) {
            // Timer expirado
            uint64_t exp;
            read(timer_fd, &exp, sizeof(exp));
            printf("  Tick periódico\n");
        }
    }

    close(inotify_fd);
    close(signal_fd);
    close(timer_fd);
    return 0;
}
```

Este patrón — todo como descriptores de archivo, todo en un `poll`
— es el enfoque fundamental de programación orientada a eventos
en Linux. Es la base de libev, libuv, y el event loop de Node.js.

---

## 9. Software real que usa inotify

```
  Software          Uso de inotify
  ──────────────────────────────────────────────────────────
  systemd           detectar cambios en unidades de servicio,
                    monitoreo de /etc/machine-id
  tail -f (GNU)     seguir archivos sin polling
  VS Code           detectar cambios en archivos del proyecto
  Docker            monitorear archivos de configuración
  rsyslog           detectar rotación de logs
  incron            cron basado en eventos de filesystem
  lsyncd            sincronización en tiempo real
                    (inotify + rsync)
  Dropbox/Nextcloud sincronización de archivos locales
  entr              ejecutar comando al cambiar archivos
  watchman (Meta)   monitor de filesystem para build tools
  guard (Ruby)      runner de tests automático
  webpack           hot module replacement en desarrollo
```

### incron: crontab basado en filesystem

```
# /etc/incron.d/monitor:
/var/spool/incoming IN_CLOSE_WRITE /usr/local/bin/process $@/$#

# $@ = directorio vigilado
# $# = nombre del archivo
# Se ejecuta cada vez que se cierra un archivo escrito
```

---

## 10. Errores comunes

### Error 1: vigilar el archivo en lugar del directorio para hot reload

```c
// MAL: pierde el watch cuando vim reemplaza el archivo
int wd = inotify_add_watch(fd, "config.ini", IN_MODIFY);
// vim: DELETE config.ini → RENAME .swp → config.ini
// → IN_DELETE_SELF → IN_IGNORED → watch muerto

// BIEN: vigilar el directorio y filtrar por nombre
int wd = inotify_add_watch(fd, ".", IN_CLOSE_WRITE | IN_MOVED_TO);
// Filtrar: strcmp(event->name, "config.ini") == 0
```

### Error 2: no hacer debounce

```c
// MAL: recompilar en cada evento
if (event->mask & IN_MODIFY) {
    system("make");  // se ejecuta 5 veces por un solo Ctrl+S
}

// BIEN: esperar a que la ráfaga termine
// Ver sección 6 para implementación con timerfd
```

### Error 3: no agregar watches para subdirectorios nuevos

```c
// MAL: monitoreo recursivo que no detecta dirs creados después
watch_recursive(inotify_fd, root);  // solo los que existen ahora
// mkdir root/newdir → no se vigila

// BIEN: detectar IN_CREATE+IN_ISDIR y agregar watch
if ((event->mask & IN_CREATE) && (event->mask & IN_ISDIR)) {
    char new_path[PATH_MAX];
    snprintf(new_path, sizeof(new_path), "%s/%s",
             wd_to_path(event->wd), event->name);
    add_watch(inotify_fd, new_path, mask);
}
```

### Error 4: ignorar IN_IGNORED

```c
// MAL: seguir usando un wd después de que fue removido
// El archivo fue borrado → IN_DELETE_SELF → IN_IGNORED
// El wd ya no es válido, el kernel lo puede reutilizar

// BIEN: limpiar el watch de tu tabla
if (event->mask & IN_IGNORED) {
    remove_from_watch_table(event->wd);
}
```

### Error 5: no manejar la creación de archivos en dos fases

```c
// MAL: copiar el archivo en IN_CREATE
if (event->mask & IN_CREATE) {
    copy_file(src, dst);  // el archivo puede estar vacío todavía
}

// BIEN: esperar a IN_CLOSE_WRITE (escritura terminada)
if (event->mask & IN_CLOSE_WRITE) {
    copy_file(src, dst);  // ahora sí tiene contenido completo
}
```

`IN_CREATE` solo significa que el archivo fue creado en el directorio.
El contenido se escribe después con `write()`. Solo cuando el escritor
hace `close()` recibes `IN_CLOSE_WRITE` con garantía de contenido
completo.

---

## 11. Cheatsheet

```
  HOT RELOAD
  ──────────────────────────────────────────────────────────
  Vigilar: directorio padre (no el archivo)
  Eventos: IN_CLOSE_WRITE | IN_MOVED_TO
  Filtrar: strcmp(event->name, filename)
  Debounce: timerfd con 200-500ms

  BUILD WATCHER
  ──────────────────────────────────────────────────────────
  Vigilar: directorio de fuentes
  Eventos: IN_CLOSE_WRITE | IN_MOVED_TO
  Filtrar: extensión (.c, .h, .py, etc.)
  Debounce: sí (trailing edge)
  Acción: system("make") o exec

  SINCRONIZACIÓN
  ──────────────────────────────────────────────────────────
  Vigilar: directorio fuente
  Crear:   IN_CLOSE_WRITE → copiar
  Borrar:  IN_DELETE → unlink destino
  Mover:   IN_MOVED_FROM + IN_MOVED_TO (cookie)
  Overflow: IN_Q_OVERFLOW → re-escanear completo

  TAIL -F
  ──────────────────────────────────────────────────────────
  Vigilar: archivo con IN_MODIFY
  Rotación: vigilar directorio con IN_CREATE
  Al rotar: leer restante, cerrar, abrir nuevo

  SEGURIDAD
  ──────────────────────────────────────────────────────────
  Vigilar: archivos críticos
  Eventos: IN_MODIFY | IN_ATTRIB | IN_DELETE_SELF
  Acción: log de alerta con timestamp

  MONITOREO RECURSIVO
  ──────────────────────────────────────────────────────────
  Inicial: recorrer con opendir/readdir, add_watch cada dir
  Runtime: IN_CREATE + IN_ISDIR → add_watch nuevo subdir
  Límite:  /proc/sys/fs/inotify/max_user_watches
  Memoria: ~1 KiB por watch en kernel

  DEBOUNCE
  ──────────────────────────────────────────────────────────
  timerfd_create + timerfd_settime
  Cada evento reinicia el timer
  poll({inotify_fd, timer_fd})
  Acción al expirar el timer

  EVENT LOOP
  ──────────────────────────────────────────────────────────
  poll/epoll con múltiples fds:
    inotify_fd + signalfd + timerfd + sockets
  Todo como descriptores de archivo
  Un solo hilo, sin bloqueo
```

---

## 12. Ejercicios

### Ejercicio 1: build watcher configurable

Escribe un build watcher que lea su configuración de un archivo:

```ini
# watchconfig.ini
watch_dir=src
extensions=.c,.h
command=gcc -Wall -o app src/*.c -lm
debounce_ms=300
```

```c
// Esqueleto:
// 1. Parsear watchconfig.ini
// 2. inotify_add_watch en watch_dir
// 3. Filtrar por extensiones configuradas
// 4. Debounce con timerfd
// 5. Ejecutar command con system()
// 6. BONUS: hot-reload del propio watchconfig.ini
```

Requisitos:
- Parsear la lista de extensiones separadas por coma.
- Implementar debounce con timerfd (no con usleep).
- Mostrar tiempo de compilación y resultado (OK/FALLÓ).
- Bonus: si `watchconfig.ini` cambia, recargar la configuración.

**Pregunta de reflexión**: si el comando de build tarda 30 segundos
y durante ese tiempo se guarda el archivo otra vez, ¿qué debería pasar?
¿Debería cancelar el build actual y reiniciar? ¿Encolar otro? ¿Ignorar
hasta que termine? ¿Qué hace `make` cuando lo invocas dos veces?

---

### Ejercicio 2: sincronizador bidireccional

Extiende el sincronizador de la sección 3 para que funcione en ambas
direcciones: cambios en A se copian a B y viceversa.

```c
// Esqueleto:
// 1. inotify_add_watch en dir_a Y dir_b
// 2. Cuando cambia en A: copiar a B
// 3. Cuando cambia en B: copiar a A
// 4. ¡CUIDADO! Copiar a B genera un evento en B
//    que intenta copiar a A → bucle infinito
```

Requisitos:
- Implementar detección de bucles: si el archivo fue modificado por
  nosotros (la copia), ignorar el evento.
- Estrategia sugerida: mantener una lista de "archivos que estamos
  copiando ahora" y filtrar esos eventos.
- Manejar conflictos: si ambos cambian el mismo archivo, el último gana.

**Pregunta de reflexión**: la detección de bucles basada en nombre de
archivo tiene una race condition. ¿Cuál es? ¿Cómo lo resuelven
herramientas reales como Syncthing o Unison? (Pista: versión vectors,
timestamps de escritura propia, o archivos marker temporales.)

---

### Ejercicio 3: incron simplificado

Implementa una versión simplificada de `incron`: un daemon que lee
reglas de un archivo de configuración y ejecuta comandos cuando ocurren
eventos:

```
# rules.conf
/tmp/incoming  IN_CLOSE_WRITE  /usr/bin/process_file $path $file
/etc           IN_MODIFY       /usr/bin/notify_admin $path $file
/var/spool     IN_CREATE       echo "Nuevo archivo: $file"
```

```c
// Esqueleto:
typedef struct {
    char path[PATH_MAX];
    uint32_t mask;
    char command[512];
    int wd;
} rule_t;

// 1. Parsear rules.conf en un array de rule_t
// 2. Para cada regla, inotify_add_watch
// 3. Bucle de eventos:
//    - Buscar regla por wd
//    - Sustituir $path y $file en el comando
//    - Ejecutar con system() o fork+exec
```

Requisitos:
- Parsear la máscara de texto: "IN_CLOSE_WRITE" → constante numérica.
  Soportar combinaciones con `|`: "IN_CREATE|IN_MODIFY".
- Sustituir `$path` con el directorio y `$file` con `event->name`.
- Ejecutar el comando con `fork` + `execl("/bin/sh", "sh", "-c", cmd)`.
- No ejecutar comandos como root si el daemon es root (usar `setuid`
  o validar reglas).

**Pregunta de reflexión**: `incron` ejecuta un proceso por cada evento.
Si llegan 1000 archivos a `/tmp/incoming` en un segundo, ¿cuántos
procesos se lanzan? ¿Qué problemas puede causar? ¿Cómo limitarías
la concurrencia (máximo N procesos simultáneos)?

---
