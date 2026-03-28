# SysV vs POSIX IPC

## Índice
1. [Contexto histórico](#1-contexto-histórico)
2. [Diferencias fundamentales de diseño](#2-diferencias-fundamentales-de-diseño)
3. [Comparación de APIs: Memoria compartida](#3-comparación-de-apis-memoria-compartida)
4. [Comparación de APIs: Semáforos](#4-comparación-de-apis-semáforos)
5. [Comparación de APIs: Colas de mensajes](#5-comparación-de-apis-colas-de-mensajes)
6. [Identificación y namespace](#6-identificación-y-namespace)
7. [Persistencia y limpieza](#7-persistencia-y-limpieza)
8. [Permisos y seguridad](#8-permisos-y-seguridad)
9. [Rendimiento](#9-rendimiento)
10. [Portabilidad](#10-portabilidad)
11. [Cuándo usar cada uno](#11-cuándo-usar-cada-uno)
12. [Tabla resumen completa](#12-tabla-resumen-completa)
13. [Errores comunes](#13-errores-comunes)
14. [Cheatsheet](#14-cheatsheet)
15. [Ejercicios](#15-ejercicios)

---

## 1. Contexto histórico

### System V IPC (1983)

Apareció en AT&T System V Release 2. Fue el primer estándar ampliamente
adoptado de IPC en Unix. La API se diseñó como un sistema unificado:
los tres mecanismos (memoria compartida, semáforos, colas de mensajes)
comparten el mismo esquema de identificación (`key_t` + `ftok`) y
administración (`ipcs`/`ipcrm`).

```
  1983: System V Release 2 — shmget, semget, msgget
  1988: POSIX.1 (sin IPC)
  1993: POSIX.1b (realtime) — sem_open, mq_open (primeros POSIX IPC)
  2001: POSIX.1-2001 — shm_open (completa la trilogía POSIX)
  2008: POSIX.1-2008 — refinamientos
```

### POSIX IPC (1993-2001)

Diseñado como reemplazo moderno, adoptando la filosofía Unix de "todo
es un archivo". Cada mecanismo tiene su propia API independiente, usa
nombres de archivo (no claves numéricas), y retorna file descriptors
(no IDs opacos).

### Por qué coexisten

System V IPC sigue existiendo porque:
- Código legacy masivo que lo usa (bases de datos, middleware)
- Características únicas sin equivalente POSIX (operaciones atómicas
  multi-semáforo, filtrado por tipo en colas, `SEM_UNDO`)
- Algunos sistemas embebidos/antiguos solo implementan System V

POSIX IPC se prefiere porque:
- API más limpia y consistente con el resto de POSIX
- Integración con file descriptors (poll/epoll/select)
- Mejor documentada y más intuitiva

---

## 2. Diferencias fundamentales de diseño

### Filosofía

```
  System V:                              POSIX:
  ┌──────────────────────┐               ┌──────────────────────┐
  │ "Sistema IPC propio" │               │ "Todo es un archivo" │
  │                      │               │                      │
  │ key_t → ID → operar  │               │ nombre → fd → operar │
  │                      │               │                      │
  │ Namespace: kernel    │               │ Namespace: filesystem│
  │ (tabla interna)      │               │ (/dev/shm, /dev/mq)  │
  │                      │               │                      │
  │ Herramientas propias:│               │ Herramientas estándar│
  │ ipcs, ipcrm          │               │ ls, rm, stat, chmod  │
  │                      │               │                      │
  │ Handle: int (ID)     │               │ Handle: int (fd)     │
  │ No integra con I/O   │               │ Integra con poll/    │
  │ multiplexing         │               │ epoll/select         │
  └──────────────────────┘               └──────────────────────┘
```

### Modelo de identificación

**System V** usa un esquema de dos pasos:

```c
// 1. Generar clave numérica a partir de un archivo
key_t key = ftok("/tmp/myapp", 'A');

// 2. Obtener/crear recurso por clave
int id = shmget(key, 4096, IPC_CREAT | 0666);
```

Problemas de `ftok`:
- Depende del inode del archivo — si se borra y recrea, cambia la clave
- Colisiones posibles (solo usa inode parcial + proj_id de 8 bits)
- El archivo debe existir y ser accesible
- No hay forma de listar qué claves están en uso (sin `ipcs`)

**POSIX** usa nombres directos:

```c
// Nombre legible, como un archivo
int fd = shm_open("/myapp_data", O_CREAT | O_RDWR, 0666);
```

Ventajas:
- Nombre legible y predecible
- Visible en el filesystem (`ls /dev/shm/`)
- Sin dependencia de archivos externos
- Sin colisiones accidentales

### Modelo de handles

```
  System V:                    POSIX:
  ┌──────────┐                ┌──────────┐
  │ shmid    │                │ fd       │
  │ semid    │ ── int ──      │          │ ── int ──
  │ msqid    │   (opaco)      │          │  (file descriptor)
  └──────────┘                └──────────┘
       │                           │
       │ Solo funciones *ctl,      │ Funciona con:
       │ *op, *at, *dt             │ read, write, close,
       │                           │ fstat, fchmod, fchown,
       │                           │ poll, epoll, select,
       │                           │ mmap, dup, fcntl
       │                           │
       ▼                           ▼
  Ecosistema cerrado         Ecosistema Unix completo
```

Un `fd` POSIX se puede pasar a funciones genéricas de I/O. Un `shmid`
System V solo funciona con `shmat`/`shmdt`/`shmctl`. Esta diferencia
es fundamental para diseño de software: POSIX permite construir
abstracciones genéricas sobre file descriptors.

---

## 3. Comparación de APIs: Memoria compartida

### Crear / obtener

```c
// System V
key_t key = ftok("/tmp/shmfile", 'R');
int shmid = shmget(key, 4096, IPC_CREAT | 0666);

// POSIX
int fd = shm_open("/myshm", O_CREAT | O_RDWR, 0666);
ftruncate(fd, 4096);  // paso extra necesario
```

### Mapear en memoria

```c
// System V
void *ptr = shmat(shmid, NULL, 0);
// Dirección elegida por el kernel, lectura/escritura

// POSIX
void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
close(fd);  // el mapeo persiste sin el fd
```

### Desmapear

```c
// System V
shmdt(ptr);

// POSIX
munmap(ptr, 4096);  // necesitas recordar el tamaño
```

### Eliminar

```c
// System V
shmctl(shmid, IPC_RMID, NULL);
// Marca para eliminación — se destruye cuando nattch llega a 0
// Nuevos shmat fallan inmediatamente

// POSIX
shm_unlink("/myshm");
// Elimina el nombre — se destruye cuando todos los mapeos se cierran
// Nuevos shm_open fallan
```

### Diferencias clave

```
┌─────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto             │ System V             │ POSIX                │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Tamaño              │ Fijo al crear        │ Dinámico (ftruncate) │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Paso extra          │ Ninguno              │ ftruncate obligatorio│
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Consultar tamaño    │ shmctl(IPC_STAT)     │ fstat(fd)            │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Offset parcial      │ No (shmat mapea todo)│ Sí (mmap con offset) │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Respaldo a disco    │ No                   │ No (shm_open → tmpfs)│
│                     │                      │ Sí (open → archivo)  │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Tamaño necesario    │ No (shmdt no lo pide)│ Sí (munmap lo pide)  │
│ para desmapear      │                      │                      │
└─────────────────────┴──────────────────────┴──────────────────────┘
```

POSIX tiene la ventaja de poder usar `mmap` con archivos regulares para
persistencia a disco. System V solo vive en el kernel.

---

## 4. Comparación de APIs: Semáforos

### Crear

```c
// System V — conjunto de N semáforos
key_t key = ftok("/tmp/semfile", 'S');
int semid = semget(key, 1, IPC_CREAT | 0666);
// Inicializar (paso separado, race condition)
union semun arg = {.val = 1};
semctl(semid, 0, SETVAL, arg);

// POSIX con nombre — un semáforo
sem_t *sem = sem_open("/mysem", O_CREAT, 0666, 1);
// Creado e inicializado en una sola llamada

// POSIX sin nombre — en memoria
sem_t sem;
sem_init(&sem, 0, 1);  // pshared=0 para threads
```

### Wait (P) y Post (V)

```c
// System V
struct sembuf op = {0, -1, SEM_UNDO};  // sem_num, sem_op, flags
semop(semid, &op, 1);

// POSIX
sem_wait(sem);       // o sem como &sem para sin nombre
```

```c
// System V
struct sembuf op = {0, 1, SEM_UNDO};
semop(semid, &op, 1);

// POSIX
sem_post(sem);
```

### Diferencias clave

```
┌─────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto             │ System V             │ POSIX                │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Granularidad        │ Conjunto de N sems   │ Un semáforo          │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Operación atómica   │ Sí (múltiples sems   │ No (solo uno         │
│ multi-semáforo      │ en un semop)         │ a la vez)            │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Inicialización      │ Paso separado        │ En la creación       │
│                     │ (race condition)     │ (atómico)            │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ SEM_UNDO            │ Sí (auto-recovery    │ No                   │
│                     │ al morir proceso)    │                      │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Wait-for-zero       │ Sí (sem_op = 0)      │ No                   │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Operación por N     │ Sí (sem_op = ±N)     │ No (solo ±1)         │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Timeout             │ semtimedop           │ sem_timedwait        │
│                     │ (relativo)           │ (absoluto)           │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Valor máximo        │ 32767 (SEMVMX)       │ SEM_VALUE_MAX        │
│                     │                      │ (≥32767, Linux:      │
│                     │                      │  INT_MAX)            │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ En memoria compart. │ No (kernel managed)  │ Sí (sem_init con     │
│                     │                      │ pshared=1)           │
└─────────────────────┴──────────────────────┴──────────────────────┘
```

Las tres features únicas de System V que no tienen equivalente POSIX:

1. **Operaciones atómicas multi-semáforo**: adquirir dos recursos a la
   vez sin deadlock. POSIX requiere ordenamiento manual.

2. **SEM_UNDO**: recuperación automática si el proceso muere con el
   semáforo adquirido. POSIX necesita `PTHREAD_MUTEX_ROBUST` (solo
   para mutexes, no semáforos).

3. **Wait-for-zero**: bloquear hasta que el valor del semáforo sea
   exactamente 0 (útil para barreras). POSIX no tiene equivalente
   directo.

---

## 5. Comparación de APIs: Colas de mensajes

### Crear y enviar

```c
// System V
key_t key = ftok("/tmp/msgfile", 'Q');
int msqid = msgget(key, IPC_CREAT | 0666);

struct {
    long mtype;
    char text[256];
} msg = {.mtype = 1};
strcpy(msg.text, "hello");
msgsnd(msqid, &msg, strlen(msg.text) + 1, 0);

// POSIX
struct mq_attr attr = {.mq_maxmsg = 10, .mq_msgsize = 256};
mqd_t mq = mq_open("/myqueue", O_CREAT | O_WRONLY, 0666, &attr);
mq_send(mq, "hello", 6, 0);  // prioridad 0
```

### Recibir

```c
// System V
struct {
    long mtype;
    char text[256];
} msg;
msgrcv(msqid, &msg, sizeof(msg.text), 0, 0);
// msgtyp=0: FIFO, >0: filtrar por tipo, <0: prioridad

// POSIX
char buf[256];
unsigned int prio;
mq_receive(mq, buf, 256, &prio);
// Siempre recibe el de mayor prioridad primero
```

### Diferencias clave

```
┌─────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto             │ System V             │ POSIX                │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Formato mensaje     │ Requiere long mtype  │ Blob de bytes libre  │
│                     │ al inicio            │                      │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Filtrado selectivo  │ Sí (por tipo)        │ No                   │
│ en recepción        │                      │                      │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Prioridad           │ Implícita (tipo      │ Explícita (unsigned  │
│                     │ negativo en msgrcv)  │ int, 0-31)           │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Multiplexación      │ Tipos como "canales" │ No (una cola = un    │
│                     │ en una sola cola     │ canal)               │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Notificación        │ No                   │ mq_notify (señal     │
│ asíncrona           │                      │ o thread)            │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Capacidad           │ Límite en bytes      │ Límite en número     │
│                     │ total (msg_qbytes)   │ de mensajes          │
│                     │                      │ (mq_maxmsg)          │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ poll/epoll          │ No                   │ Sí (Linux, mqd_t     │
│                     │                      │ es un fd)            │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Tamaño máximo       │ Flexible (msg_qbytes)│ Fijo al crear        │
│ por mensaje         │                      │ (mq_msgsize)         │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Timeout             │ No (solo IPC_NOWAIT) │ mq_timedsend/receive │
└─────────────────────┴──────────────────────┴──────────────────────┘
```

La feature única de System V aquí es el **filtrado por tipo**: un
servidor puede usar una sola cola para múltiples clientes, usando
`msgrcv(id, &msg, size, client_pid, 0)` para recibir solo la respuesta
de un cliente específico. POSIX necesitaría una cola separada por
cliente.

---

## 6. Identificación y namespace

### System V: claves numéricas en el kernel

```c
// ftok genera un número basado en inode + proj_id
key_t key = ftok("/tmp/myapp", 65);
// key = (proj_id << 24) | (minor_dev << 16) | (inode & 0xFFFF)
```

```
  Proceso A                    Kernel                    Proceso B
  ┌──────────┐    key_t      ┌──────────────┐  key_t   ┌──────────┐
  │ ftok(    │ ────────────► │  IPC Table   │ ◄─────── │ ftok(    │
  │  path,id)│    12345      │              │   12345  │  path,id)│
  │          │               │ key → shmid  │          │          │
  │ shmget(  │    shmid=7    │ key → semid  │  shmid=7 │ shmget(  │
  │  key,...) │ ◄──────────── │ key → msqid  │ ────────► │  key,...) │
  └──────────┘               └──────────────┘          └──────────┘
```

Problemas:
- La clave depende del inode → cambiar el archivo cambia la clave
- Colisiones: diferentes (path, id) pueden generar la misma clave
- No hay namespace — todas las claves son globales en el sistema
- `IPC_PRIVATE` (key=0) crea sin clave → solo padre↔hijo

### POSIX: nombres en el filesystem

```c
// Nombre legible, como una ruta (pero solo un componente)
int fd = shm_open("/myapp_data", O_CREAT | O_RDWR, 0666);
```

```
  Proceso A                  Filesystem                  Proceso B
  ┌──────────┐   "/myshm"   ┌────────────────┐ "/myshm" ┌──────────┐
  │ shm_open │ ────────────► │ /dev/shm/myshm │ ◄──────── │ shm_open │
  │          │    fd=3       │                │   fd=5   │          │
  └──────────┘               └────────────────┘          └──────────┘
```

Ventajas:
- Nombres descriptivos y predecibles
- Visibles con `ls` estándar
- Permisos Unix normales (chmod, chown)
- Pueden organizarse con prefijos (`/myapp_shm`, `/myapp_sem`)

Limitación: el nombre no puede contener `/` después del inicial. No hay
subdirectorios (`/myapp/data` es inválido). Esto limita la organización
en sistemas con muchos recursos IPC.

---

## 7. Persistencia y limpieza

### System V: kernel persistence

Los recursos System V viven en tablas internas del kernel. Persisten
hasta que:
1. Se eliminan explícitamente (`shmctl(IPC_RMID)`, `semctl(IPC_RMID)`,
   `msgctl(IPC_RMID)`)
2. Se eliminan con `ipcrm`
3. El sistema se reinicia

```bash
# Recursos huérfanos (nadie los usa pero siguen ahí)
$ ipcs

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch
0x52010105 32769      user       666        4096       0      ← huérfano

------ Semaphore Arrays --------
key        semid      owner      perms      nsems
0x41010105 0          user       666        1                 ← huérfano

------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages
0x51010105 32768      user       666        0            0    ← huérfano
```

Script de limpieza:

```bash
#!/bin/bash
# Limpiar todos los recursos IPC del usuario actual
ipcs -m | awk -v uid=$(id -u) '$3 == ENVIRON["USER"] {print $2}' | \
    xargs -r -n1 ipcrm -m
ipcs -s | awk -v uid=$(id -u) '$3 == ENVIRON["USER"] {print $2}' | \
    xargs -r -n1 ipcrm -s
ipcs -q | awk -v uid=$(id -u) '$3 == ENVIRON["USER"] {print $2}' | \
    xargs -r -n1 ipcrm -q
```

### POSIX: filesystem persistence

Los recursos POSIX viven en filesystems virtuales (tmpfs). Persisten
hasta que:
1. Se eliminan con `*_unlink` (`shm_unlink`, `sem_unlink`, `mq_unlink`)
2. Se eliminan con `rm` en el filesystem correspondiente
3. El sistema se reinicia (tmpfs vive en RAM)

```bash
# Ver y limpiar
$ ls /dev/shm/          # memoria compartida y semáforos
$ ls /dev/mqueue/       # colas de mensajes
$ rm /dev/shm/myshm     # equivale a shm_unlink("/myshm")
$ rm /dev/shm/sem.mysem # equivale a sem_unlink("/mysem")
$ rm /dev/mqueue/myq    # equivale a mq_unlink("/myq")
```

### Comparación de ciclo de vida

```
  System V:
  ┌────────┐    ┌────────┐    ┌─────────┐    ┌────────────┐
  │ create │───►│  use   │───►│ process │───►│ PERSISTE   │
  │ shmget │    │ shmat  │    │ exits   │    │ hasta      │
  │        │    │ shmdt  │    │         │    │ IPC_RMID   │
  └────────┘    └────────┘    └─────────┘    └────────────┘

  POSIX:
  ┌────────┐    ┌────────┐    ┌─────────┐    ┌────────────┐
  │ create │───►│  use   │───►│ unlink  │───►│ Destruido  │
  │shm_open│    │  mmap  │    │(nombre) │    │ cuando     │
  │        │    │ munmap │    │         │    │ último fd  │
  └────────┘    └────────┘    └─────────┘    │ se cierra  │
                                             └────────────┘
```

El patrón POSIX de "unlink temprano" (como con archivos) es más seguro:
el nombre desaparece inmediatamente, evitando que nuevos procesos se
conecten, y el recurso se destruye automáticamente cuando los procesos
existentes terminan.

---

## 8. Permisos y seguridad

### System V

Cada recurso tiene permisos estilo Unix almacenados en `ipc_perm`:

```c
struct ipc_perm {
    uid_t  uid;    // owner
    gid_t  gid;    // group
    uid_t  cuid;   // creator
    gid_t  cgid;   // creator group
    mode_t mode;   // permisos (rwx para user/group/other)
};
```

```c
// Ver permisos
struct shmid_ds info;
shmctl(shmid, IPC_STAT, &info);
printf("Owner: %d, Mode: %o\n", info.shm_perm.uid, info.shm_perm.mode);

// Cambiar permisos
info.shm_perm.mode = 0600;  // solo owner
shmctl(shmid, IPC_SET, &info);
```

**Problema**: el creador (`cuid`) no puede cambiarse. Si un proceso
privilegiado crea el recurso y un usuario normal necesita usarlo,
los permisos deben configurarse correctamente al crear. No hay
equivalente de `chown` para System V.

### POSIX

Los objetos POSIX son archivos en un filesystem virtual. Los permisos
se manejan con herramientas estándar:

```c
// En código
fchmod(fd, 0600);
fchown(fd, uid, gid);

// Desde shell
chmod 600 /dev/shm/myshm
chown user:group /dev/shm/myshm
```

La ventaja es que las herramientas existentes (ACLs, SELinux, AppArmor)
funcionan directamente con los objetos POSIX porque son archivos.
System V necesita mecanismos específicos para políticas de seguridad.

### Seguridad: namespace isolation

**System V**: el namespace es global por kernel. Todos los procesos del
sistema ven todos los recursos IPC (restringido solo por permisos).
Con Linux namespaces (`unshare(CLONE_NEWIPC)`), cada container puede
tener su propio namespace IPC System V. Docker usa esto.

**POSIX**: depende del filesystem. `/dev/shm` y `/dev/mqueue` son
puntos de montaje que pueden ser diferentes por container/namespace.
En la práctica, Docker también aísla POSIX IPC mediante mount namespaces.

---

## 9. Rendimiento

### Overhead de las operaciones

```
┌──────────────────┬──────────────┬──────────────┐
│ Operación        │ System V     │ POSIX        │
├──────────────────┼──────────────┼──────────────┤
│ Crear recurso    │ syscall      │ syscall      │
│                  │              │ (+ ftruncate │
│                  │              │  para shm)   │
├──────────────────┼──────────────┼──────────────┤
│ Semáforo         │ syscall      │ futex-based  │
│ wait/post        │ (siempre)    │ (fast path   │
│                  │              │  en userspace)│
├──────────────────┼──────────────┼──────────────┤
│ Mensajes         │ syscall      │ syscall      │
│ send/receive     │              │              │
├──────────────────┼──────────────┼──────────────┤
│ Shm attach/      │ syscall      │ syscall      │
│ mmap             │ (shmat)      │ (mmap)       │
├──────────────────┼──────────────┼──────────────┤
│ Shm access       │ Directo      │ Directo      │
│ (read/write data)│ (memoria)    │ (memoria)    │
└──────────────────┴──────────────┴──────────────┘
```

La diferencia más significativa está en **semáforos**: POSIX sin nombre
(`sem_init`) usa futex en Linux, que tiene un fast path completamente
en userspace cuando no hay contención. System V `semop` siempre hace
syscall.

### Benchmark típico (Linux x86_64)

```
  Operación               System V        POSIX sem_init    Ratio
  ─────────               ────────        ──────────────    ─────
  sem lock+unlock         ~500ns          ~50ns             10x
  (sin contención)        (syscall)       (futex fast path)

  sem lock+unlock         ~1.5µs         ~1.2µs            ~1.3x
  (con contención)        (syscall+wake) (futex+wake)

  msg send+recv           ~2µs           ~2µs              ~1x
  (4 bytes)

  shm create+attach       ~5µs           ~8µs              ~0.6x
  (una vez)               (shmget+shmat) (shm_open+ftrunc
                                          +mmap)
```

Conclusión: para semáforos sin contención (el caso común), POSIX sin
nombre es ~10x más rápido. Para el resto de operaciones, la diferencia
es insignificante.

### Escalabilidad

System V tiene límites globales del kernel que pueden ser un cuello
de botella en sistemas con muchos procesos:

```bash
$ cat /proc/sys/kernel/sem
# SEMMSL  SEMMNS    SEMOPM  SEMMNI
# 32000   1024000000  500   32000

$ cat /proc/sys/kernel/shmall   # total de páginas compartidas
$ cat /proc/sys/kernel/shmmni   # máximo de segmentos
$ cat /proc/sys/kernel/msgmni   # máximo de colas
```

POSIX está limitado por el filesystem (espacio en tmpfs para shm/sem,
espacio de montaje para mqueue):

```bash
$ df -h /dev/shm       # normalmente 50% de RAM
$ df -h /dev/mqueue    # normalmente pequeño
```

---

## 10. Portabilidad

```
┌─────────────────────┬───────┬───────┬─────────┬────────┬─────────┐
│ Plataforma          │ SysV  │ SysV  │ SysV    │ POSIX  │ POSIX   │
│                     │ shm   │ sem   │ msg     │ shm/sem│ mq      │
├─────────────────────┼───────┼───────┼─────────┼────────┼─────────┤
│ Linux               │ ✅    │ ✅    │ ✅      │ ✅     │ ✅      │
├─────────────────────┼───────┼───────┼─────────┼────────┼─────────┤
│ FreeBSD             │ ✅    │ ✅    │ ✅      │ ✅     │ ✅      │
├─────────────────────┼───────┼───────┼─────────┼────────┼─────────┤
│ macOS               │ ✅    │ ✅    │ ✅      │ ✅     │ ❌      │
│                     │       │       │         │ (no    │         │
│                     │       │       │         │sem_init│         │
│                     │       │       │         │ depr.) │         │
├─────────────────────┼───────┼───────┼─────────┼────────┼─────────┤
│ Solaris/illumos     │ ✅    │ ✅    │ ✅      │ ✅     │ ✅      │
├─────────────────────┼───────┼───────┼─────────┼────────┼─────────┤
│ OpenBSD             │ ❌    │ ✅    │ ❌      │ ❌     │ ❌      │
│                     │(remov)│       │ (remov) │        │         │
├─────────────────────┼───────┼───────┼─────────┼────────┼─────────┤
│ Windows (Cygwin)    │ ✅    │ ✅    │ ✅      │ ⚠️     │ ⚠️      │
│                     │       │       │         │(parcial│(parcial)│
└─────────────────────┴───────┴───────┴─────────┴────────┴─────────┘
```

Notas de portabilidad:
- **macOS**: `sem_init` está deprecated y puede no funcionar. Usar
  `sem_open`. POSIX mq no está implementado.
- **OpenBSD**: eliminó System V shm y msg por seguridad. Solo mantiene
  semáforos. Usa `mmap` con archivos regulares como alternativa.
- **Cygwin**: emula ambas APIs con limitaciones.

### Máxima portabilidad

Para código que debe funcionar en todas partes:
- Memoria compartida: `mmap` con archivo regular (funciona en todo Unix)
- Semáforos: `sem_open` (funciona en Linux, FreeBSD, macOS, Solaris)
- Colas de mensajes: alternativa con sockets o pipes (mq no está en macOS)

---

## 11. Cuándo usar cada uno

### Preferir POSIX cuando:

- **Código nuevo** sin restricciones de legacy
- **Semáforos en hot path**: POSIX sin nombre con futex es 10x más rápido
- **Integración con event loop**: necesitas poll/epoll con mq o shm_open
- **Herramientas estándar**: quieres ver/modificar/limpiar con ls/rm/chmod
- **Código portátil**: funciona en la mayoría de sistemas POSIX
- **Seguridad**: ACLs, SELinux/AppArmor trabajan nativamente con archivos

### Preferir System V cuando:

- **Operaciones atómicas multi-semáforo**: adquirir 2+ recursos sin
  deadlock, imposible con POSIX
- **SEM_UNDO**: recuperación automática si el proceso muere — crítico
  para daemons que no pueden coordinar limpieza
- **Filtrado por tipo en colas**: multiplexar múltiples "canales" en
  una sola cola, patrón servidor con tipo=PID
- **Wait-for-zero**: necesitas saber cuándo un recurso está completamente
  libre
- **Código legacy**: interoperar con software existente que use System V
- **Semáforos con valor > SEM_VALUE_MAX**: System V permite incrementos
  arbitrarios con sem_op

### Tabla de decisión rápida

```
  ¿Necesitas operaciones atómicas en múltiples semáforos?
  └─ Sí → System V semáforos
  └─ No ↓

  ¿Necesitas SEM_UNDO (recovery automático)?
  └─ Sí → System V semáforos
  └─ No ↓

  ¿Necesitas filtrar mensajes por tipo en recepción?
  └─ Sí → System V colas
  └─ No ↓

  ¿Necesitas integrar con poll/epoll?
  └─ Sí → POSIX
  └─ No ↓

  ¿Es código nuevo sin restricciones?
  └─ Sí → POSIX
  └─ No → System V (probablemente legacy)
```

---

## 12. Tabla resumen completa

```
┌─────────────────────┬────────────────────────┬────────────────────────┐
│ Aspecto             │ System V               │ POSIX                  │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Año                 │ 1983                   │ 1993-2001              │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Filosofía           │ Sistema IPC propio     │ Todo es un archivo     │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Identificación      │ key_t (ftok)           │ Nombre "/xxx"          │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Handle              │ int (shmid/semid/msqid)│ int (fd) / sem_t*      │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Namespace           │ Kernel global          │ Filesystem virtual     │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Visibilidad         │ ipcs                   │ ls /dev/shm, /dev/mq   │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Limpieza            │ ipcrm / *ctl(IPC_RMID) │ rm / *_unlink          │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ poll/epoll          │ No                     │ Sí (fd-based)          │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Permisos            │ struct ipc_perm        │ chmod/chown/ACL        │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Persistencia        │ Kernel (hasta IPC_RMID │ tmpfs (hasta unlink +  │
│                     │ o reboot)              │ close, o reboot)       │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Multi-sem atómico   │ ✅ semop(sops, nsops)  │ ❌                     │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ SEM_UNDO            │ ✅                     │ ❌                     │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Wait-for-zero       │ ✅                     │ ❌                     │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Filtrado por tipo   │ ✅ msgrcv(msgtyp)      │ ❌                     │
│ en colas            │                        │                        │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Prioridad en colas  │ Implícita (tipo neg.)  │ Explícita (0-31)       │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ mq_notify           │ ❌                     │ ✅ (señal o thread)    │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Sem en mmap         │ ❌                     │ ✅ sem_init(pshared=1) │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Shm tamaño dinámico │ ❌                     │ ✅ ftruncate           │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Respaldo a disco    │ ❌                     │ ✅ mmap + archivo      │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Rendimiento sem     │ ~500ns (syscall)       │ ~50ns (futex fast path)│
│ (sin contención)    │                        │                        │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ macOS               │ ✅ (completo)          │ ⚠️ (no sem_init,       │
│                     │                        │    no mq)              │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ OpenBSD             │ ⚠️ (solo sem)           │ ❌                     │
├─────────────────────┼────────────────────────┼────────────────────────┤
│ Recomendación       │ Legacy, features       │ Código nuevo,          │
│                     │ únicas (multi-sem,     │ rendimiento,           │
│                     │ SEM_UNDO, filtrado)    │ integración            │
└─────────────────────┴────────────────────────┴────────────────────────┘
```

---

## 13. Errores comunes

### Error 1: Mezclar APIs System V y POSIX

```c
// MAL: crear con POSIX, limpiar con System V
int fd = shm_open("/myshm", O_CREAT | O_RDWR, 0666);
// ... usar ...
shmctl(fd, IPC_RMID, NULL);  // ¡fd no es un shmid!

// BIEN: ser consistente con una API
// POSIX:
shm_unlink("/myshm");

// O System V:
int shmid = shmget(key, size, IPC_CREAT | 0666);
shmctl(shmid, IPC_RMID, NULL);
```

**Por qué importa**: los handles de System V (shmid) y POSIX (fd)
son ambos `int`, así que el compilador no detecta el error. El resultado
es eliminar el recurso equivocado o un error silencioso.

### Error 2: Confiar en ftok para identificación única

```c
// MAL: asumir que ftok siempre da la misma clave
key_t key = ftok("/tmp/myapp_marker", 'A');
// Si alguien borra y recrea /tmp/myapp_marker, el inode cambia
// → key diferente → recurso IPC diferente → datos perdidos

// BIEN: usar un archivo que no se toque, o verificar
key_t key = ftok("/usr/local/myapp/ipc_anchor", 'A');
// Archivo en ubicación estable, propiedad de la aplicación

// O mejor: usar POSIX con nombre fijo
int fd = shm_open("/myapp_data", O_CREAT | O_RDWR, 0666);
```

### Error 3: No detectar recurso pre-existente con estado stale

```c
// MAL: abrir semáforo existente que quedó con valor incorrecto
sem_t *sem = sem_open("/mylock", O_CREAT, 0666, 1);
// Si el semáforo ya existía con valor 0 (proceso anterior murió
// con el lock), O_CREAT ignora el valor inicial → deadlock

// BIEN: usar O_EXCL para detectar pre-existencia
sem_t *sem = sem_open("/mylock", O_CREAT | O_EXCL, 0666, 1);
if (sem == SEM_FAILED && errno == EEXIST) {
    // Decisión: ¿re-usar el existente o limpiar y recrear?
    sem_unlink("/mylock");  // limpiar stale
    sem = sem_open("/mylock", O_CREAT | O_EXCL, 0666, 1);
}
```

**La misma trampa aplica a System V**: un semáforo que quedó en 0
porque el proceso murió sin hacer V/post bloquea a todos los futuros
usuarios. `SEM_UNDO` resuelve esto en System V pero no existe en POSIX.

### Error 4: Asumir que POSIX mq funciona en macOS

```c
// Compila pero falla en runtime en macOS
mqd_t mq = mq_open("/myqueue", O_CREAT | O_RDWR, 0666, &attr);
// macOS: errno = ENOSYS (función no implementada)

// Solución portátil: usar pipes, sockets, o #ifdef
#ifdef __linux__
    // usar POSIX mq
#elif defined(__APPLE__)
    // usar socket pair o GCD dispatch
#endif
```

### Error 5: Elegir la API incorrecta para el caso de uso

```c
// MAL: usar System V semáforos en un hot path sin contención
struct sembuf op = {0, -1, 0};
for (int i = 0; i < 1000000; i++) {
    semop(semid, &op, 1);   // syscall cada vez (~500ns)
    // ... trabajo mínimo ...
    op.sem_op = 1;
    semop(semid, &op, 1);   // syscall cada vez
    op.sem_op = -1;
}
// Total: ~1 segundo solo en overhead de semáforos

// BIEN: usar POSIX sem_init para hot path
sem_t sem;
sem_init(&sem, 0, 1);
for (int i = 0; i < 1000000; i++) {
    sem_wait(&sem);          // futex fast path (~50ns)
    // ... trabajo mínimo ...
    sem_post(&sem);
}
// Total: ~100ms — 10x más rápido
```

---

## 14. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────────┐
│                    SysV vs POSIX IPC — DECISIÓN RÁPIDA             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ¿Código nuevo sin restricciones?                                   │
│  └─ Sí → POSIX (siempre)                                          │
│  └─ No → Evaluar features System V                                │
│                                                                     │
│  Features ÚNICAS de System V (sin equivalente POSIX):               │
│    1. semop(sops, N): operar N semáforos atómicamente              │
│    2. SEM_UNDO: auto-recovery si el proceso muere                  │
│    3. msgrcv(type>0): filtrar mensajes por tipo                    │
│    4. sem_op=0: esperar a que el semáforo llegue a 0               │
│                                                                     │
│  Features ÚNICAS de POSIX:                                          │
│    1. poll/epoll/select con mq y shm (fd-based)                    │
│    2. mq_notify: notificación asíncrona (señal/thread)             │
│    3. ftruncate: shm de tamaño dinámico                            │
│    4. sem_init: semáforo en mmap, futex fast path (~10x)           │
│    5. mmap + archivo: persistencia a disco                         │
│    6. Herramientas Unix: ls, rm, chmod, ACLs, SELinux              │
│                                                                     │
│  Herramientas de administración:                                    │
│    System V: ipcs [-m|-s|-q], ipcrm [-m|-s|-q] <id>                │
│    POSIX:    ls /dev/shm/ /dev/mqueue/, rm, stat, chmod            │
│                                                                     │
│  Portabilidad:                                                      │
│    Linux:   ambos completos ✅                                     │
│    macOS:   SysV ✅, POSIX ⚠️ (no sem_init, no mq)                │
│    OpenBSD: SysV ⚠️ (solo sem), POSIX ❌                           │
│    Máxima:  mmap+archivo, sem_open, sockets (evitar mq)           │
│                                                                     │
│  Compilación:                                                       │
│    POSIX shm/mq: gcc prog.c -lrt                                  │
│    POSIX sem:    gcc prog.c -lpthread                              │
│    System V:     gcc prog.c  (sin flags extra)                     │
│                                                                     │
│  Limpieza automática (template):                                    │
│    POSIX:  shm_open → shm_unlink inmediatamente → se destruye     │
│            al cerrar último fd/munmap                              │
│    SysV:   shmget → shmat → shmctl(IPC_RMID) → se destruye       │
│            al último shmdt                                         │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 15. Ejercicios

### Ejercicio 1: Wrapper unificado

Implementa una capa de abstracción que ofrezca la misma interfaz para
semáforos, independientemente de si el backend es System V o POSIX:

```c
// usem.h — unified semaphore
typedef struct usem usem_t;

// Crear con backend SysV o POSIX
usem_t *usem_create(const char *name, int initial_value, int use_sysv);

// Operaciones (misma interfaz, diferente backend)
int usem_wait(usem_t *sem);
int usem_trywait(usem_t *sem);
int usem_post(usem_t *sem);
int usem_getvalue(usem_t *sem, int *val);
void usem_destroy(usem_t *sem);
```

Verificar que ambos backends producen el mismo resultado con un
productor-consumidor de 100000 elementos.

Bonus: agregar una opción `USEM_UNDO` que use `SEM_UNDO` en el
backend System V y emule el comportamiento con `atexit` + flag en
el backend POSIX.

**Pregunta de reflexión**: ¿Es posible emular `SEM_UNDO` perfectamente
en POSIX? ¿Qué pasa si el proceso muere con `kill -9` (que no ejecuta
`atexit`)? ¿Qué alternativa queda — `PTHREAD_MUTEX_ROBUST`? ¿Por qué
POSIX no incluyó una feature equivalente a `SEM_UNDO`?

### Ejercicio 2: Benchmark comparativo

Escribe un benchmark que mida el rendimiento de las 6 combinaciones
de mecanismo × API:

```
  Mecanismo         System V              POSIX
  ─────────         ────────              ─────
  Semáforo          semop (1 op)          sem_wait/sem_post (named)
                                          sem_wait/sem_post (unnamed)
  Mem compartida    shmget+shmat          shm_open+mmap
  Cola mensajes     msgsnd+msgrcv         mq_send+mq_receive
```

Para cada uno, medir:
- **Latencia**: tiempo de una operación round-trip (ns)
- **Throughput**: operaciones por segundo
- **Varianza**: jitter (min/max/p95/p99)

Ejecutar cada test con 1M iteraciones y reportar los resultados en
una tabla.

**Pregunta de reflexión**: ¿Por qué los semáforos POSIX sin nombre son
mucho más rápidos que los demás en el caso sin contención? ¿Qué es un
futex y cómo evita el syscall en el fast path? Si hay contención, ¿la
diferencia se reduce? ¿Por qué?

### Ejercicio 3: Migración SysV → POSIX

Dado el siguiente servidor System V que usa las tres features únicas
de System V simultáneamente, migra a una solución POSIX que preserve
el mismo comportamiento:

```c
// Servidor que:
// 1. Usa semop atómico para adquirir 2 recursos a la vez
// 2. Usa SEM_UNDO para recovery automático
// 3. Usa msgrcv con filtrado por tipo para responder a clientes

// Tu tarea: reescribir usando SOLO API POSIX
// Para (1): implementar ordenamiento de recursos + fallback
// Para (2): implementar cleanup con señales + PTHREAD_MUTEX_ROBUST
// Para (3): implementar una cola por cliente o protocolo de correlación
```

Comparar ambas versiones en:
- Líneas de código
- Complejidad de la lógica de error handling
- Comportamiento cuando un cliente muere con kill -9
- Rendimiento (latencia, throughput)

**Pregunta de reflexión**: La migración de SysV a POSIX ¿siempre vale
la pena? ¿En qué escenarios el código resultante es más complejo que
el original por tener que emular features que System V da gratis?
¿Cuándo es mejor mantener System V aunque sea "legacy"?
