# Bloque 6: Programación de Sistemas en C

**Objetivo**: Usar C para interactuar directamente con el kernel de Linux a través
de llamadas al sistema. Entender los mecanismos de bajo nivel del SO.

## Capítulo 1: Llamadas al Sistema [A]

### Sección 1: Interfaz Syscall
- **T01 - Qué es una syscall**: user space vs kernel space, tabla de syscalls, wrapper de glibc
- **T02 - Manejo de errores**: errno, perror, strerror, convenciones de retorno
- **T03 - strace**: trazar syscalls de un programa, filtros, timing, conteo

### Sección 2: Operaciones de Archivo (POSIX)
- **T01 - open/close/read/write avanzado**: O_CLOEXEC, O_NONBLOCK, short reads, reintentos con EINTR
- **T02 - stat, fstat, lstat**: struct stat, permisos, tipo de archivo, timestamps
- **T03 - Directorios**: opendir, readdir, closedir, recorrido recursivo
- **T04 - File locking**: flock, fcntl, advisory vs mandatory locking

### Sección 3: mmap
- **T01 - Mapeo de archivos**: mmap con fd, PROT_READ/PROT_WRITE, MAP_SHARED/MAP_PRIVATE, munmap
- **T02 - Mapeo anónimo**: MAP_ANONYMOUS, lo que malloc hace internamente para bloques grandes
- **T03 - Memoria compartida con mmap**: mmap + shm_open vs mmap + archivo, sincronización
- **T04 - Ventajas sobre read/write**: zero-copy, lazy loading, page cache del kernel, archivos grandes

### Sección 4: inotify
- **T01 - API de inotify**: inotify_init, inotify_add_watch, read de eventos, IN_CREATE/IN_MODIFY/IN_DELETE
- **T02 - Casos de uso**: monitoreo de configuración (hot reload), sincronización de archivos, build watchers
- **T03 - Limitaciones**: no recursivo por defecto, límite de watches (/proc/sys/fs/inotify/max_user_watches), alternativas (fanotify)

## Capítulo 2: Procesos [A]

### Sección 1: Creación y Gestión
- **T01 - fork()**: duplicación del proceso, PID, valor de retorno en padre e hijo
- **T02 - exec family**: execvp, execle, etc. — cuándo se usa cada variante
- **T03 - wait y waitpid**: recoger estado de salida, WIFEXITED, WIFSIGNALED, zombies
- **T04 - Patrón fork+exec**: el idioma estándar de Unix para lanzar programas

### Sección 2: Entorno del Proceso
- **T01 - Variables de entorno**: getenv, setenv, environ
- **T02 - Directorio de trabajo**: getcwd, chdir
- **T03 - Límites de recursos**: getrlimit, setrlimit, ulimit

## Capítulo 3: Señales [A]

### Sección 1: Manejo de Señales
- **T01 - signal() vs sigaction()**: por qué preferir sigaction, portabilidad
- **T02 - Señales comunes**: SIGINT, SIGTERM, SIGCHLD, SIGUSR1/2, SIGPIPE, SIGALRM
- **T03 - Signal handlers seguros**: funciones async-signal-safe, sig_atomic_t, volatile
- **T04 - Máscara de señales**: sigprocmask, sigpending, señales bloqueadas

### Sección 2: Señales Avanzadas
- **T01 - Señales en tiempo real**: SIGRTMIN-SIGRTMAX, sigqueue, datos adjuntos
- **T02 - signalfd**: manejar señales como file descriptors (Linux-específico)
- **T03 - Self-pipe trick**: despertar un poll/select desde un signal handler

## Capítulo 4: IPC — Comunicación entre Procesos [A]

### Sección 1: Pipes
- **T01 - Pipes anónimos**: pipe(), comunicación padre↔hijo, cierre de extremos
- **T02 - Named pipes (FIFOs)**: mkfifo, comunicación entre procesos no relacionados
- **T03 - Redirección con dup2**: implementar tuberías de shell

### Sección 2: IPC System V y POSIX
- **T01 - Memoria compartida**: shmget/shmat (SysV), shm_open/mmap (POSIX)
- **T02 - Semáforos**: semget (SysV), sem_open/sem_post/sem_wait (POSIX)
- **T03 - Colas de mensajes**: msgget (SysV), mq_open (POSIX)
- **T04 - SysV vs POSIX IPC**: diferencias de API, cuál preferir, portabilidad

## Capítulo 5: Hilos (pthreads) [A]

### Sección 1: Fundamentos
- **T01 - pthread_create y pthread_join**: creación, espera, paso de argumentos
- **T02 - Datos compartidos**: race conditions, por qué son peligrosas
- **T03 - Mutex**: pthread_mutex_lock/unlock, deadlocks, PTHREAD_MUTEX_ERRORCHECK

### Sección 2: Sincronización
- **T01 - Variables de condición**: pthread_cond_wait, pthread_cond_signal, spurious wakeups
- **T02 - Read-write locks**: pthread_rwlock, cuándo son ventajosos
- **T03 - Thread-local storage**: pthread_key_create, __thread keyword
- **T04 - Barriers y spinlocks**: pthread_barrier, pthread_spin — cuándo usar cada uno

## Capítulo 6: Sockets [A]

### Sección 1: API de Sockets
- **T01 - socket, bind, listen, accept, connect**: el flujo completo TCP
- **T02 - UDP**: sendto, recvfrom, diferencias con TCP
- **T03 - Unix domain sockets**: AF_UNIX, comunicación local, paso de file descriptors
- **T04 - Network byte order**: htonl, htons, ntohl, ntohs, struct sockaddr_in, inet_pton/inet_ntop

### Sección 2: I/O Multiplexado
- **T01 - select**: API, limitación FD_SETSIZE, timeout
- **T02 - poll**: mejora sobre select, POLLIN, POLLOUT, POLLERR
- **T03 - epoll**: epoll_create, epoll_ctl, epoll_wait — edge vs level triggered
- **T04 - Servidor concurrente**: fork por conexión, thread pool, event loop con epoll

## Capítulo 7: Proyecto — Herramienta de Sistema en C [P]

### Sección 1: Mini-shell
- **T01 - Parser de comandos**: tokenización, pipes, redirección
- **T02 - Ejecución**: fork+exec, gestión de pipes encadenados
- **T03 - Built-ins**: cd, exit, manejo de señales (Ctrl+C no mata la shell)
- **T04 - Job control básico**: background (&), fg, lista de jobs

## Capítulo 8: Proyecto — Servidor HTTP Básico en C [P]

### Sección 1: Servidor Single-thread
- **T01 - Socket TCP listener**: bind a puerto, accept en loop, fork por conexión
- **T02 - Parser de HTTP request**: método, path, headers, parseo línea por línea
- **T03 - Response HTTP**: status line, headers (Content-Type, Content-Length), body
- **T04 - Servir archivos estáticos**: mapear path a filesystem, MIME types, 404

### Sección 2: Mejoras
- **T01 - Mejora 1**: thread pool en lugar de fork (reutilizar hilos)
- **T02 - Mejora 2**: directory listing (índice HTML autogenerado)
- **T03 - Mejora 3**: logging de requests (IP, método, path, status code, timestamp)
