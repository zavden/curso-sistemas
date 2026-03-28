# Unix domain sockets

## Índice
1. [¿Qué son los Unix domain sockets?](#1-qué-son-los-unix-domain-sockets)
2. [AF_UNIX vs AF_INET](#2-af_unix-vs-af_inet)
3. [sockaddr_un](#3-sockaddr_un)
4. [Stream (SOCK_STREAM)](#4-stream-sock_stream)
5. [Datagram (SOCK_DGRAM)](#5-datagram-sock_dgram)
6. [socketpair()](#6-socketpair)
7. [Abstract namespace (Linux)](#7-abstract-namespace-linux)
8. [Paso de file descriptors (SCM_RIGHTS)](#8-paso-de-file-descriptors-scm_rights)
9. [Paso de credenciales (SCM_CREDENTIALS)](#9-paso-de-credenciales-scm_credentials)
10. [Quién los usa en Linux](#10-quién-los-usa-en-linux)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. ¿Qué son los Unix domain sockets?

Los Unix domain sockets (UDS) son sockets que comunican procesos
**en la misma máquina** usando el filesystem como espacio de nombres,
en vez de direcciones IP y puertos:

```
Sockets de red (AF_INET):          Unix domain sockets (AF_UNIX):
┌─────────┐     ┌─────────┐       ┌─────────┐     ┌─────────┐
│Proceso A│     │Proceso B│       │Proceso A│     │Proceso B│
│         │     │         │       │         │     │         │
│ TCP/UDP │     │ TCP/UDP │       │  AF_UNIX│     │  AF_UNIX│
└────┬────┘     └────┬────┘       └────┬────┘     └────┬────┘
     │    ┌─────┐    │                 │    ┌─────┐    │
     └────┤ Red ├────┘                 └────┤Kernel├───┘
          │stack│                            │(IPC) │
          └─────┘                            └──────┘
     Pasa por IP/TCP/                   Nunca toca la
     buffers de red                     pila de red
```

### ¿Por qué no usar simplemente TCP en localhost?

| Aspecto | TCP localhost | Unix domain socket |
|---------|--------------|-------------------|
| Rendimiento | Pasa por toda la pila TCP/IP | Bypass directo en el kernel |
| Throughput | ~6 GB/s | ~10-12 GB/s |
| Latencia | ~10 µs | ~2-5 µs |
| Overhead por paquete | Headers IP+TCP (40 bytes) | Sin headers de red |
| Seguridad | Cualquier usuario puede conectar | Permisos del filesystem |
| Funcionalidades extra | — | Pasar fds, credenciales |
| Portabilidad | Cualquier OS | Unix/Linux/macOS |

Los Unix domain sockets son **la forma más eficiente** de IPC con
la API de sockets. El kernel detecta que ambos extremos están en
la misma máquina y copia datos directamente entre buffers del
kernel, sin pasar por la pila de red.

---

## 2. AF_UNIX vs AF_INET

La API es prácticamente idéntica. Solo cambian la familia de
direcciones y la estructura de dirección:

```c
// TCP sobre IPv4
int fd = socket(AF_INET, SOCK_STREAM, 0);
struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port   = htons(8080),
    .sin_addr.s_addr = htonl(INADDR_ANY)
};
bind(fd, (struct sockaddr *)&addr, sizeof(addr));

// Unix domain socket (stream)
int fd = socket(AF_UNIX, SOCK_STREAM, 0);
struct sockaddr_un addr = {
    .sun_family = AF_UNIX,
    .sun_path   = "/tmp/my_app.sock"
};
bind(fd, (struct sockaddr *)&addr, sizeof(addr));
```

Todo lo demás (`listen`, `accept`, `connect`, `read`, `write`,
`close`) funciona igual.

### Tipos soportados

| Tipo | AF_INET | AF_UNIX |
|------|---------|---------|
| `SOCK_STREAM` | TCP | Stream local (bidireccional, confiable) |
| `SOCK_DGRAM` | UDP | Datagramas locales (confiable — no se pierden) |
| `SOCK_SEQPACKET` | SCTP (raro) | Datagramas ordenados con conexión |

> **Diferencia clave**: `SOCK_DGRAM` sobre AF_UNIX **no pierde
> datagramas** porque no hay red de por medio. La entrega es
> confiable y ordenada (a diferencia de UDP real).

---

## 3. sockaddr_un

```c
#include <sys/un.h>

struct sockaddr_un {
    sa_family_t sun_family;   // AF_UNIX
    char        sun_path[108]; // ruta en el filesystem
};
```

### Llenar la estructura

```c
#include <sys/un.h>
#include <string.h>

struct sockaddr_un addr;
memset(&addr, 0, sizeof(addr));
addr.sun_family = AF_UNIX;
strncpy(addr.sun_path, "/tmp/my_app.sock", sizeof(addr.sun_path) - 1);
```

### El archivo socket en el filesystem

Cuando haces `bind()` con un Unix domain socket, se crea un archivo
especial en el filesystem:

```bash
$ ls -l /tmp/my_app.sock
srwxrwxr-x 1 user user 0 Mar 20 10:00 /tmp/my_app.sock
# ^
# s = socket file
```

Este archivo **no contiene datos**. Es solo un punto de rendezvous
para que los clientes encuentren al servidor. Los datos fluyen por
el kernel, no por el filesystem.

### Permisos

Los permisos del archivo socket controlan quién puede conectarse:

```c
// Crear con permisos restrictivos
mode_t old_mask = umask(077);  // solo el propietario
bind(fd, (struct sockaddr *)&addr, sizeof(addr));
umask(old_mask);

// O cambiar permisos después
chmod("/tmp/my_app.sock", 0660);  // propietario + grupo
```

### El archivo persiste después de close

A diferencia de los sockets de red, el archivo socket **no se
elimina automáticamente** cuando el proceso termina. Debes
eliminarlo manualmente:

```c
// Antes de bind (por si quedó de una ejecución anterior)
unlink("/tmp/my_app.sock");
bind(fd, ...);

// Al terminar
close(fd);
unlink("/tmp/my_app.sock");
```

Si no haces `unlink` antes de `bind`, obtienes `EADDRINUSE`.

### Longitud de la ruta

`sun_path` tiene 108 bytes en Linux (104 en algunos BSDs). Esto
limita la longitud de la ruta. Si la ruta es muy larga, usa un
directorio más corto o el abstract namespace:

```c
// MAL: puede exceder 108 bytes
"/home/user/.local/share/my_application/sockets/main.sock"

// BIEN: rutas cortas
"/tmp/myapp.sock"
"/run/myapp/sock"
```

---

## 4. Stream (SOCK_STREAM)

El uso más común. Equivale a TCP pero local y más rápido.

### Servidor stream

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_PATH "/tmp/echo_unix.sock"

int main(void) {
    // 1. Crear socket
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. Eliminar socket anterior si existe
    unlink(SOCK_PATH);

    // 3. Bind
    struct sockaddr_un addr = {
        .sun_family = AF_UNIX
    };
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // 4. Listen
    if (listen(server_fd, 5) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on %s\n", SOCK_PATH);

    // 5. Accept loop
    for (;;) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }
        printf("Client connected\n");

        // Echo
        char buf[4096];
        ssize_t n;
        while ((n = read(client_fd, buf, sizeof(buf))) > 0) {
            write(client_fd, buf, n);
        }

        printf("Client disconnected\n");
        close(client_fd);
    }

    close(server_fd);
    unlink(SOCK_PATH);
    return 0;
}
```

### Cliente stream

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_PATH "/tmp/echo_unix.sock"

int main(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX
    };
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(fd);
        exit(EXIT_FAILURE);
    }
    printf("Connected to %s\n", SOCK_PATH);

    const char *msg = "Hello Unix socket!";
    write(fd, msg, strlen(msg));

    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("Server replied: %s\n", buf);
    }

    close(fd);
    return 0;
}
```

```bash
# Terminal 1:
gcc -o uds_server uds_server.c && ./uds_server
# Terminal 2:
gcc -o uds_client uds_client.c && ./uds_client
# O con socat:
socat - UNIX-CONNECT:/tmp/echo_unix.sock
```

---

## 5. Datagram (SOCK_DGRAM)

Datagramas sobre AF_UNIX son **confiables y ordenados** (a
diferencia de UDP). Preservan los límites de mensaje sin riesgo
de pérdida:

```c
// Servidor datagram
int server_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
unlink("/tmp/dgram_server.sock");

struct sockaddr_un server_addr = {
    .sun_family = AF_UNIX
};
strncpy(server_addr.sun_path, "/tmp/dgram_server.sock",
        sizeof(server_addr.sun_path) - 1);

bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

// Recibir datagramas
char buf[1024];
struct sockaddr_un client_addr;
socklen_t client_len = sizeof(client_addr);

ssize_t n = recvfrom(server_fd, buf, sizeof(buf), 0,
                     (struct sockaddr *)&client_addr, &client_len);
// client_addr.sun_path contiene la ruta del socket del cliente
```

### El cliente también necesita bind

A diferencia de UDP sobre IP (donde el kernel asigna un puerto
efímero), con `SOCK_DGRAM` + AF_UNIX el cliente **debe hacer bind**
para que el servidor pueda responderle:

```c
// Cliente datagram
int client_fd = socket(AF_UNIX, SOCK_DGRAM, 0);

// El cliente NECESITA su propia dirección para recibir respuestas
unlink("/tmp/dgram_client.sock");
struct sockaddr_un client_addr = {
    .sun_family = AF_UNIX
};
strncpy(client_addr.sun_path, "/tmp/dgram_client.sock",
        sizeof(client_addr.sun_path) - 1);
bind(client_fd, (struct sockaddr *)&client_addr, sizeof(client_addr));

// Enviar al servidor
struct sockaddr_un server_addr = {
    .sun_family = AF_UNIX
};
strncpy(server_addr.sun_path, "/tmp/dgram_server.sock",
        sizeof(server_addr.sun_path) - 1);

sendto(client_fd, "Hello", 5, 0,
       (struct sockaddr *)&server_addr, sizeof(server_addr));

// Recibir respuesta
char buf[1024];
ssize_t n = recvfrom(client_fd, buf, sizeof(buf), 0, NULL, NULL);

// Limpiar
close(client_fd);
unlink("/tmp/dgram_client.sock");
```

> **Tip**: usa el abstract namespace para el cliente, así no necesitas
> gestionar archivos temporales.

### SOCK_SEQPACKET

`SOCK_SEQPACKET` combina lo mejor de stream y datagram:
- **Orientado a conexión** (como `SOCK_STREAM`)
- **Preserva límites de mensaje** (como `SOCK_DGRAM`)
- **Ordenado y confiable**

```c
int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
// Usa listen/accept/connect como SOCK_STREAM
// Pero cada write() se recibe como un mensaje completo en un read()
```

Ideal para protocolos basados en mensajes donde necesitas conexión:

```
SOCK_STREAM:    [Hello][Wor][ld]   → read puede devolver cualquier corte
SOCK_SEQPACKET: [Hello][World]     → read devuelve un mensaje completo
SOCK_DGRAM:     [Hello][World]     → recvfrom devuelve un mensaje completo
                                     (pero sin conexión)
```

---

## 6. socketpair()

`socketpair()` crea **dos sockets ya conectados entre sí**. Es
como `pipe()` pero bidireccional:

```c
#include <sys/socket.h>

int sv[2];
int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
// sv[0] y sv[1] son dos sockets conectados
// Escribir en sv[0] → leer en sv[1]
// Escribir en sv[1] → leer en sv[0]
```

```
pipe():
  fd[0] ◀────── fd[1]     (unidireccional)

socketpair():
  sv[0] ◀─────▶ sv[1]     (bidireccional)
```

### Uso típico: comunicación padre-hijo

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>

int main(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
        perror("socketpair");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Hijo: usa sv[1], cierra sv[0]
        close(sv[0]);

        char buf[256];
        ssize_t n = read(sv[1], buf, sizeof(buf) - 1);
        buf[n] = '\0';
        printf("Child received: %s\n", buf);

        const char *reply = "Hello from child!";
        write(sv[1], reply, strlen(reply));

        close(sv[1]);
        _exit(0);
    }

    // Padre: usa sv[0], cierra sv[1]
    close(sv[1]);

    const char *msg = "Hello from parent!";
    write(sv[0], msg, strlen(msg));

    char buf[256];
    ssize_t n = read(sv[0], buf, sizeof(buf) - 1);
    buf[n] = '\0';
    printf("Parent received: %s\n", buf);

    close(sv[0]);
    wait(NULL);
    return 0;
}
```

### Ventajas sobre pipe

| Aspecto | pipe() | socketpair() |
|---------|--------|-------------|
| Dirección | Unidireccional | Bidireccional |
| Tipos | Solo flujo | Stream, datagram, seqpacket |
| Pasar fds | No | Sí (con SCM_RIGHTS) |
| SOCK_DGRAM | No | Sí (límites de mensaje) |

### socketpair + SOCK_DGRAM

Si quieres mensajes con límites preservados entre padre e hijo:

```c
int sv[2];
socketpair(AF_UNIX, SOCK_DGRAM, 0, sv);
// Cada write() = un mensaje completo
// Cada read()  = recibe exactamente un mensaje
```

---

## 7. Abstract namespace (Linux)

En Linux, puedes usar un espacio de nombres **abstracto** que no
crea archivos en el filesystem. Se indica poniendo un byte `\0` al
inicio de `sun_path`:

```c
struct sockaddr_un addr;
memset(&addr, 0, sizeof(addr));
addr.sun_family = AF_UNIX;

// Abstract: empieza con \0
// El nombre es "my_abstract_socket" (sin archivo en disco)
addr.sun_path[0] = '\0';
strncpy(addr.sun_path + 1, "my_abstract_socket",
        sizeof(addr.sun_path) - 2);

// El tamaño incluye la familia + \0 + nombre (sin el padding)
socklen_t len = offsetof(struct sockaddr_un, sun_path)
              + 1  // el \0 inicial
              + strlen("my_abstract_socket");

bind(fd, (struct sockaddr *)&addr, len);
```

### Ventajas del abstract namespace

| Ventaja | Detalle |
|---------|---------|
| Sin archivo en disco | No hay que hacer `unlink()` |
| Limpieza automática | Desaparece cuando el último fd se cierra |
| Sin conflictos de permisos | No depende de permisos del directorio |
| Sin race conditions | No hay ventana entre `unlink` y `bind` |

### Desventajas

| Desventaja | Detalle |
|------------|---------|
| Solo Linux | No funciona en macOS ni BSDs |
| Sin permisos por archivo | Cualquier proceso puede conectar |
| Difícil de inspeccionar | `ss` lo muestra, pero no `ls` |

### Inspeccionar sockets abstractos

```bash
# Ver todos los Unix domain sockets (incluyendo abstract)
ss -xlnp
# Los abstract se muestran con @ al inicio:
# u_str  LISTEN  @my_abstract_socket

# Con lsof
lsof -U
```

### Ejemplo completo con abstract namespace

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stddef.h>

#define ABSTRACT_NAME "my_echo_server"

int main(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    addr.sun_path[0] = '\0';
    memcpy(addr.sun_path + 1, ABSTRACT_NAME, strlen(ABSTRACT_NAME));

    socklen_t addrlen = offsetof(struct sockaddr_un, sun_path)
                      + 1 + strlen(ABSTRACT_NAME);

    // No necesita unlink — no hay archivo
    if (bind(fd, (struct sockaddr *)&addr, addrlen) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    listen(fd, 5);
    printf("Listening on abstract socket @%s\n", ABSTRACT_NAME);

    // ... accept loop igual que filesystem socket ...

    close(fd);  // el socket abstracto desaparece automáticamente
    return 0;
}
```

---

## 8. Paso de file descriptors (SCM_RIGHTS)

Esta es la funcionalidad más poderosa y única de los Unix domain
sockets: **enviar file descriptors de un proceso a otro**. El kernel
duplica el fd en el espacio del receptor:

```
Proceso A (fd=7: archivo.txt)        Proceso B
┌──────────────────────┐             ┌──────────────────────┐
│ fd table:            │             │ fd table:            │
│   7 → [archivo.txt]  │─ sendmsg ─▶│   4 → [archivo.txt]  │
│                      │  SCM_RIGHTS │                      │
└──────────────────────┘             └──────────────────────┘

B recibe fd=4 que apunta al mismo archivo abierto que fd=7 de A.
B puede leer/escribir el archivo aunque no tenga permisos para abrirlo.
```

### ¿Por qué es útil?

1. **Pasar un archivo abierto**: un proceso privilegiado abre un
   archivo y pasa el fd a un proceso sin privilegios.
2. **Pasar un socket abierto**: un supervisor acepta conexiones y
   distribuye los fds a workers (nginx hace esto).
3. **Pasar un pipe**: crear un pipe y enviar un extremo a otro
   proceso no relacionado.

### Implementación

El paso de fds usa `sendmsg()`/`recvmsg()` con datos auxiliares
(ancillary data / control messages):

```c
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Enviar un fd a través de un Unix domain socket
int send_fd(int socket, int fd_to_send) {
    // Se necesita enviar al menos 1 byte de datos reales
    char dummy = 'F';
    struct iovec iov = {
        .iov_base = &dummy,
        .iov_len  = 1
    };

    // Buffer para el mensaje de control
    // CMSG_SPACE calcula el tamaño necesario con alineamiento
    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    memset(cmsg_buf, 0, sizeof(cmsg_buf));

    struct msghdr msg = {
        .msg_iov        = &iov,
        .msg_iovlen     = 1,
        .msg_control    = cmsg_buf,
        .msg_controllen = sizeof(cmsg_buf)
    };

    // Construir el mensaje de control
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;       // "estoy enviando fds"
    cmsg->cmsg_len   = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));

    return sendmsg(socket, &msg, 0);
}

// Recibir un fd de un Unix domain socket
int recv_fd(int socket) {
    char dummy;
    struct iovec iov = {
        .iov_base = &dummy,
        .iov_len  = 1
    };

    char cmsg_buf[CMSG_SPACE(sizeof(int))];

    struct msghdr msg = {
        .msg_iov        = &iov,
        .msg_iovlen     = 1,
        .msg_control    = cmsg_buf,
        .msg_controllen = sizeof(cmsg_buf)
    };

    if (recvmsg(socket, &msg, 0) <= 0)
        return -1;

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == NULL ||
        cmsg->cmsg_level != SOL_SOCKET ||
        cmsg->cmsg_type  != SCM_RIGHTS)
        return -1;

    int received_fd;
    memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
    return received_fd;
}
```

### Ejemplo: pasar un archivo abierto entre procesos

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>

// ... send_fd() y recv_fd() definidos arriba ...

int main(void) {
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

    pid_t pid = fork();
    if (pid == 0) {
        // Hijo: recibe el fd
        close(sv[0]);

        int fd = recv_fd(sv[1]);
        if (fd == -1) {
            fprintf(stderr, "Failed to receive fd\n");
            _exit(1);
        }
        printf("Child received fd %d\n", fd);

        // Leer del archivo recibido
        char buf[256];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("Child read: %s\n", buf);
        }

        close(fd);
        close(sv[1]);
        _exit(0);
    }

    // Padre: abre un archivo y envía el fd
    close(sv[1]);

    int file_fd = open("/etc/hostname", O_RDONLY);
    if (file_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    printf("Parent opened /etc/hostname as fd %d\n", file_fd);

    send_fd(sv[0], file_fd);
    close(file_fd);  // el padre ya no necesita el fd

    close(sv[0]);
    wait(NULL);
    return 0;
}
```

### Enviar múltiples fds

Puedes enviar varios fds en un solo `sendmsg()`:

```c
int fds[3] = { fd1, fd2, fd3 };

char cmsg_buf[CMSG_SPACE(sizeof(fds))];
memset(cmsg_buf, 0, sizeof(cmsg_buf));

struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
cmsg->cmsg_level = SOL_SOCKET;
cmsg->cmsg_type  = SCM_RIGHTS;
cmsg->cmsg_len   = CMSG_LEN(sizeof(fds));
memcpy(CMSG_DATA(cmsg), fds, sizeof(fds));
```

### Anatomía de sendmsg/recvmsg

```
struct msghdr:
┌────────────────────────────────────────────┐
│ msg_name / msg_namelen  ← destino (NULL    │
│                           para connected)  │
│                                            │
│ msg_iov / msg_iovlen    ← datos reales     │
│   ┌──────────────────┐    (scatter/gather) │
│   │ "Hello" (5 bytes)│                     │
│   └──────────────────┘                     │
│                                            │
│ msg_control / msg_controllen ← ancillary   │
│   ┌──────────────────────────────┐         │
│   │ cmsghdr:                     │         │
│   │   level = SOL_SOCKET         │         │
│   │   type  = SCM_RIGHTS         │         │
│   │   data  = [fd1, fd2, ...]    │         │
│   └──────────────────────────────┘         │
│                                            │
│ msg_flags               ← flags de salida  │
└────────────────────────────────────────────┘
```

Las macros `CMSG_*` navegan la estructura de control:

| Macro | Propósito |
|-------|-----------|
| `CMSG_FIRSTHDR(&msg)` | Puntero al primer `cmsghdr` |
| `CMSG_NXTHDR(&msg, cmsg)` | Puntero al siguiente `cmsghdr` |
| `CMSG_DATA(cmsg)` | Puntero a los datos del `cmsghdr` |
| `CMSG_LEN(n)` | Tamaño de `cmsghdr` + n bytes de datos |
| `CMSG_SPACE(n)` | Igual que LEN pero con alineamiento |

---

## 9. Paso de credenciales (SCM_CREDENTIALS)

Linux permite enviar y **verificar** las credenciales del proceso
remitente (PID, UID, GID) a través de Unix domain sockets:

```c
// Habilitar recepción de credenciales
int opt = 1;
setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &opt, sizeof(opt));
```

### Enviar credenciales

```c
#include <sys/socket.h>

struct ucred creds = {
    .pid = getpid(),
    .uid = getuid(),
    .gid = getgid()
};

char cmsg_buf[CMSG_SPACE(sizeof(struct ucred))];
memset(cmsg_buf, 0, sizeof(cmsg_buf));

char dummy = 'C';
struct iovec iov = { .iov_base = &dummy, .iov_len = 1 };

struct msghdr msg = {
    .msg_iov        = &iov,
    .msg_iovlen     = 1,
    .msg_control    = cmsg_buf,
    .msg_controllen = sizeof(cmsg_buf)
};

struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
cmsg->cmsg_level = SOL_SOCKET;
cmsg->cmsg_type  = SCM_CREDENTIALS;
cmsg->cmsg_len   = CMSG_LEN(sizeof(struct ucred));
memcpy(CMSG_DATA(cmsg), &creds, sizeof(creds));

sendmsg(fd, &msg, 0);
```

### Recibir y verificar credenciales

```c
char dummy;
struct iovec iov = { .iov_base = &dummy, .iov_len = 1 };

char cmsg_buf[CMSG_SPACE(sizeof(struct ucred))];
struct msghdr msg = {
    .msg_iov        = &iov,
    .msg_iovlen     = 1,
    .msg_control    = cmsg_buf,
    .msg_controllen = sizeof(cmsg_buf)
};

recvmsg(fd, &msg, 0);

struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
if (cmsg && cmsg->cmsg_level == SOL_SOCKET
         && cmsg->cmsg_type == SCM_CREDENTIALS) {
    struct ucred *creds = (struct ucred *)CMSG_DATA(cmsg);
    printf("Peer: pid=%d, uid=%d, gid=%d\n",
           creds->pid, creds->uid, creds->gid);

    // Verificar: solo aceptar conexiones de root
    if (creds->uid != 0) {
        fprintf(stderr, "Unauthorized: uid=%d\n", creds->uid);
        close(fd);
        return;
    }
}
```

> **Seguridad**: el kernel **verifica** las credenciales. Un proceso
> no puede falsificar su UID/PID (a menos que sea root). Esto hace
> que `SCM_CREDENTIALS` sea un mecanismo de autenticación confiable.

### SO_PEERCRED — alternativa más simple

Para sockets `SOCK_STREAM` conectados, puedes obtener las
credenciales del peer sin `sendmsg`/`recvmsg`:

```c
struct ucred creds;
socklen_t len = sizeof(creds);
getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &creds, &len);
printf("Client: pid=%d, uid=%d, gid=%d\n",
       creds.pid, creds.uid, creds.gid);
```

Esto es más simple y no requiere cooperación del otro lado.
D-Bus y systemd usan `SO_PEERCRED` para autenticación.

---

## 10. Quién los usa en Linux

Los Unix domain sockets son ubicuos en un sistema Linux moderno:

| Servicio | Socket | Propósito |
|----------|--------|-----------|
| D-Bus | `/run/dbus/system_bus_socket` | Comunicación entre servicios del sistema |
| systemd | `/run/systemd/journal/socket` | Journald recibe logs |
| Docker | `/var/run/docker.sock` | CLI ↔ daemon |
| MySQL | `/var/run/mysqld/mysqld.sock` | Conexiones locales |
| PostgreSQL | `/var/run/postgresql/.s.PGSQL.5432` | Conexiones locales |
| X11 | `/tmp/.X11-unix/X0` | Display server |
| Wayland | `$XDG_RUNTIME_DIR/wayland-0` | Compositor |
| nginx | — | Pasar fds de conexión a workers |
| SSH agent | `$SSH_AUTH_SOCK` | Agente de autenticación |
| snapd | `/run/snapd.socket` | Snap daemon |
| containerd | `/run/containerd/containerd.sock` | Container runtime |

### Docker como ejemplo

```bash
# El CLI de Docker se comunica con el daemon por Unix socket
$ curl --unix-socket /var/run/docker.sock http://localhost/v1.41/containers/json
# Equivalente a: docker ps

# Por eso puedes montar el socket en un contenedor para darle
# acceso al daemon Docker del host:
# docker run -v /var/run/docker.sock:/var/run/docker.sock ...
```

### Rendimiento comparado

```
Benchmark: mensajes de 64 bytes, 1M iteraciones (mismo host)

Mecanismo           Mensajes/seg     Latencia media
──────────────────────────────────────────────────────
pipe                   2,100,000      ~0.5 µs
Unix stream            1,900,000      ~0.5 µs
Unix dgram             1,800,000      ~0.6 µs
TCP localhost          1,200,000      ~0.8 µs
UDP localhost          1,300,000      ~0.8 µs
Shared memory + futex  8,000,000      ~0.1 µs
```

Unix domain sockets son ~1.5x más rápidos que TCP localhost.
La memoria compartida sigue siendo la más rápida, pero los sockets
ofrecen una API más limpia y control de flujo automático.

---

## 11. Errores comunes

### Error 1: no hacer unlink antes de bind

```c
// MAL: si el archivo ya existe de una ejecución anterior
bind(fd, (struct sockaddr *)&addr, sizeof(addr));
// → EADDRINUSE: "Address already in use"
```

**Solución**: `unlink()` antes de `bind()`:

```c
unlink(SOCK_PATH);  // ignorar error si no existe
bind(fd, (struct sockaddr *)&addr, sizeof(addr));
```

### Error 2: ruta demasiado larga en sun_path

```c
// MAL: excede 108 bytes
strncpy(addr.sun_path,
        "/very/long/path/to/application/directory/sockets/main.sock",
        sizeof(addr.sun_path) - 1);
// Si la cadena > 107 bytes, se trunca silenciosamente → socket incorrecto
```

**Solución**: usar rutas cortas o abstract namespace:

```c
// BIEN: ruta corta
"/tmp/myapp.sock"
"/run/myapp.sock"

// BIEN: abstract (Linux)
addr.sun_path[0] = '\0';
strncpy(addr.sun_path + 1, "myapp", sizeof(addr.sun_path) - 2);
```

### Error 3: no cerrar fds recibidos con SCM_RIGHTS

```c
// MAL: recibir fds y olvidar cerrarlos
int received_fd = recv_fd(socket);
// ... usar received_fd ...
// olvidar close(received_fd) → fd leak
// Peor: si recibes fds en un loop y nunca los cierras
```

Los fds recibidos con SCM_RIGHTS son **fds reales** en tu proceso.
Si no los cierras, son un fd leak exactamente igual que un
`open()` sin `close()`.

**Solución**: cerrar siempre los fds recibidos:

```c
int received_fd = recv_fd(socket);
if (received_fd >= 0) {
    // ... usar ...
    close(received_fd);
}
```

> **Cuidado extra**: si llamas a `recvmsg()` y no procesas los
> datos de control (msg_control), los fds recibidos se **filtran
> silenciosamente**. Siempre inspecciona los cmsghdr.

### Error 4: cliente SOCK_DGRAM sin bind

```c
// MAL: cliente datagram sin bind → el servidor no puede responder
int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
sendto(fd, "Hello", 5, 0, (struct sockaddr *)&server_addr, ...);
// El servidor recibe el mensaje pero client_addr está vacío
// sendto() de vuelta falla
```

**Solución**: el cliente datagram debe tener su propia dirección:

```c
// Opción A: bind a un archivo temporal
struct sockaddr_un client_addr = { .sun_family = AF_UNIX };
snprintf(client_addr.sun_path, sizeof(client_addr.sun_path),
         "/tmp/client_%d.sock", getpid());
unlink(client_addr.sun_path);
bind(fd, (struct sockaddr *)&client_addr, sizeof(client_addr));

// Opción B: abstract namespace (Linux)
struct sockaddr_un client_addr = { .sun_family = AF_UNIX };
client_addr.sun_path[0] = '\0';
snprintf(client_addr.sun_path + 1, sizeof(client_addr.sun_path) - 1,
         "client_%d", getpid());
```

### Error 5: permisos inadecuados en el socket

```c
// MAL: socket creado con umask restrictivo en directorio compartido
// Otros usuarios no pueden conectar
bind(fd, ...);  // hereda umask → puede ser 0700

// O al revés: socket demasiado permisivo para servicio privilegiado
```

**Solución**: controlar permisos explícitamente:

```c
// Para servicio que solo el propietario debe acceder
mode_t old = umask(077);
bind(fd, ...);
umask(old);

// Para servicio compartido con un grupo específico
bind(fd, ...);
chmod(SOCK_PATH, 0660);
chown(SOCK_PATH, -1, target_gid);
```

---

## 12. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                  Unix Domain Sockets                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  CREAR:                                                      │
│    fd = socket(AF_UNIX, SOCK_STREAM, 0);    // stream        │
│    fd = socket(AF_UNIX, SOCK_DGRAM, 0);     // datagram      │
│    fd = socket(AF_UNIX, SOCK_SEQPACKET, 0); // msg+conexión  │
│    socketpair(AF_UNIX, SOCK_STREAM, 0, sv); // par conectado │
│                                                              │
│  DIRECCIÓN (filesystem):                                     │
│    struct sockaddr_un addr = { .sun_family = AF_UNIX };      │
│    strncpy(addr.sun_path, "/tmp/app.sock", 107);             │
│    unlink("/tmp/app.sock");  // ¡antes de bind!              │
│    bind(fd, (struct sockaddr*)&addr, sizeof(addr));           │
│                                                              │
│  DIRECCIÓN (abstract, Linux):                                │
│    addr.sun_path[0] = '\0';                                  │
│    memcpy(addr.sun_path + 1, "name", 4);                     │
│    bind(fd, ..., offsetof(sockaddr_un,sun_path) + 1 + 4);    │
│                                                              │
│  STREAM: igual que TCP                                       │
│    listen() → accept() → read()/write() → close()            │
│                                                              │
│  DGRAM: sendto()/recvfrom()                                  │
│    ¡Cliente DEBE hacer bind para recibir respuestas!          │
│                                                              │
│  PASAR FDs:                                                  │
│    sendmsg() con cmsg: level=SOL_SOCKET, type=SCM_RIGHTS     │
│    recvmsg() → CMSG_DATA contiene los fds recibidos          │
│    ¡Cerrar los fds recibidos cuando termines!                 │
│                                                              │
│  CREDENCIALES:                                               │
│    setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &(int){1}, 4);    │
│    sendmsg() con SCM_CREDENTIALS + struct ucred              │
│    O más simple: getsockopt(SO_PEERCRED) en SOCK_STREAM      │
│                                                              │
│  INSPECCIONAR:                                               │
│    ss -xlnp          // listar Unix sockets                  │
│    lsof -U           // listar por proceso                   │
│    ls -l /tmp/*.sock // ver archivo socket (tipo 's')         │
│                                                              │
│  REGLAS:                                                     │
│    • unlink() antes de bind() (EADDRINUSE)                   │
│    • unlink() + close() al terminar                          │
│    • sun_path ≤ 107 chars (108 con \0)                       │
│    • SOCK_DGRAM local es confiable (no pierde datos)         │
│    • Abstract namespace: sin archivo, limpieza automática     │
│    • ~1.5x más rápido que TCP localhost                      │
└──────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: servidor de comandos por Unix socket

Implementa un servidor que reciba comandos por un Unix domain
socket y devuelva el resultado:

1. **Servidor** (`./cmd_server`):
   - Bind a `/tmp/cmd_server.sock` (SOCK_STREAM).
   - Accept conexiones en un loop.
   - Leer una línea del cliente (comando de shell).
   - Ejecutar el comando con `popen()` y enviar la salida al cliente.
   - Cerrar la conexión.
2. **Cliente** (`./cmd_client "ls -la"`):
   - Conectar al socket, enviar el comando, leer e imprimir la
     respuesta.
3. Seguridad: usar `SO_PEERCRED` para verificar que el cliente tiene
   el mismo UID que el servidor. Rechazar conexiones de otros
   usuarios.

Probar:
```bash
./cmd_server &
./cmd_client "uptime"
./cmd_client "df -h"
```

**Pregunta de reflexión**: ¿por qué este diseño (ejecutar comandos
recibidos por socket) es un riesgo de seguridad incluso con
verificación de UID? ¿Qué pasaría si otro proceso del mismo usuario
se conectara? ¿Cómo se protege Docker contra el abuso de
`/var/run/docker.sock`?

---

### Ejercicio 2: paso de file descriptor

Implementa un "file server" que abra archivos a pedido y pase los
fds al cliente:

1. **Servidor** (`./fd_server`):
   - Escuchar en un socket abstract `@fd_server`.
   - Recibir una ruta de archivo del cliente.
   - Abrir el archivo con `open()` y enviar el fd con SCM_RIGHTS.
   - Si el archivo no existe, enviar un mensaje de error.
2. **Cliente** (`./fd_client /etc/hostname`):
   - Conectar, enviar la ruta, recibir el fd.
   - Leer del fd recibido e imprimir el contenido.
   - Cerrar el fd recibido.
3. Verificar que el fd funciona correctamente imprimiendo el
   contenido del archivo.

Bonus: hacer que el servidor pueda abrir un archivo al que el
cliente **no tiene permisos** (por ejemplo, ejecutar el servidor
como root y el cliente como usuario normal).

**Pregunta de reflexión**: al recibir el fd, el cliente puede leer
el archivo aunque no tenga permisos de acceso directo. ¿En qué se
diferencia esto de simplemente enviar el contenido del archivo por
el socket? (Pista: piensa en archivos grandes, seek, mmap, y en
que el fd permite operaciones que el contenido plano no.)

---

### Ejercicio 3: broker de mensajes con SOCK_SEQPACKET

Implementa un broker simple de publicación/suscripción:

1. **Broker** (`./broker`):
   - Escuchar en `/tmp/broker.sock` con `SOCK_SEQPACKET`.
   - Mantener una lista de clientes conectados.
   - Cuando un cliente envía un mensaje, reenviarlo a **todos los
     demás** clientes conectados (fanout).
   - Usar `poll()` para manejar múltiples clientes concurrentemente
     en un solo thread.
2. **Cliente** (`./pub_client "canal"`):
   - Conectar al broker.
   - En un fork: el hijo lee mensajes del socket e imprime, el padre
     lee de stdin y envía al socket.
3. Verificar que los límites de mensaje se preservan (cada `read()`
   devuelve exactamente un mensaje completo, no fragmentos).

Probar con 3 clientes en terminales distintas, escribiendo mensajes
en cada uno y verificando que llegan a los demás.

**Pregunta de reflexión**: ¿qué ventaja tiene `SOCK_SEQPACKET` sobre
`SOCK_STREAM` para este caso de uso? ¿Qué pasaría si usaras
`SOCK_STREAM` y un cliente enviara dos mensajes rápidamente — cómo
los distinguirían los receptores?
