# Red del Contenedor

## Indice
1. [Donde estamos en el proyecto](#1-donde-estamos-en-el-proyecto)
2. [CLONE_NEWNET: aislamiento de red](#2-clone_newnet-aislamiento-de-red)
3. [Interfaces veth: conectar host y contenedor](#3-interfaces-veth-conectar-host-y-contenedor)
4. [Configurar la red manualmente (para entender)](#4-configurar-la-red-manualmente-para-entender)
5. [Ejecutar comandos de red desde C](#5-ejecutar-comandos-de-red-desde-c)
6. [setup_network: implementacion completa](#6-setup_network-implementacion-completa)
7. [NAT: acceso a internet desde el contenedor](#7-nat-acceso-a-internet-desde-el-contenedor)
8. [Integrar en container.c](#8-integrar-en-containerc)
9. [container.c final completo](#9-containerc-final-completo)
10. [Probando el runtime completo](#10-probando-el-runtime-completo)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Donde estamos en el proyecto

```
    S01 (completado):
    [✓] clone() con namespaces PID, mount, UTS, IPC
    [✓] pivot_root() + rootfs Alpine
    [✓] /proc montado

    S02 (completado):
    [✓] cgroup v2: limites de memoria, CPU, PIDs
    [✓] Cleanup del cgroup al salir

    S03 (esta seccion):
    [ ] CLONE_NEWNET para aislamiento de red
    [ ] Par veth para conectar host <-> contenedor
    [ ] IPs en ambos extremos
    [ ] NAT para acceso a internet
    [ ] container.c final integrado
```

Al terminar esta seccion, `container.c` sera un runtime
funcional con aislamiento completo: PID, filesystem, hostname,
IPC, red y limites de recursos.

---

## 2. CLONE_NEWNET: aislamiento de red

Agregar `CLONE_NEWNET` a los flags de `clone()` crea un
**network namespace** nuevo. El proceso hijo arranca con
una pila de red **completamente vacia**: sin interfaces (ni
siquiera loopback), sin rutas, sin reglas de firewall:

```c
int clone_flags = CLONE_NEWPID
                | CLONE_NEWNS
                | CLONE_NEWUTS
                | CLONE_NEWIPC
                | CLONE_NEWNET    /* NUEVO */
                | SIGCHLD;
```

### Que ve el contenedor sin configuracion

```bash
$ sudo ./container ./rootfs /bin/sh
container# ip link
1: lo: <LOOPBACK> mtu 65536 qdisc noop state DOWN
    link/loopback 00:00:00:00:00:00

container# ping 127.0.0.1
PING 127.0.0.1: sendmsg: Network is unreachable
# Ni siquiera loopback funciona — hay que levantarlo
```

```
    Host network namespace:          Contenedor network namespace:
    +-------------------------+      +-------------------------+
    | lo: 127.0.0.1 (UP)     |      | lo: (DOWN, sin IP)      |
    | eth0: 192.168.1.100    |      |                         |
    | docker0: 172.17.0.1    |      | (nada mas)              |
    | wlan0: ...             |      |                         |
    +-------------------------+      +-------------------------+
```

El contenedor esta **completamente** aislado de la red. Para
darle conectividad necesitamos crear un "cable virtual" entre
ambos namespaces.

---

## 3. Interfaces veth: conectar host y contenedor

Un par **veth** (virtual ethernet) es un par de interfaces de
red conectadas entre si. Lo que entra por un extremo sale por
el otro — como un cable de red virtual:

```
    Host namespace                    Container namespace
    +------------------+              +------------------+
    |                  |   cable      |                  |
    |  veth-host  <=====virtual=====>  veth-cont        |
    |  10.0.0.1        |              |  10.0.0.2       |
    |                  |              |                  |
    +------------------+              +------------------+
```

### Como funciona

1. Crear el par veth en el host namespace (ambos extremos
   nacen aqui)
2. Mover **un** extremo al network namespace del contenedor
3. Configurar IP en ambos extremos
4. Levantar ambas interfaces

Despues de esto, host y contenedor pueden comunicarse via
las IPs asignadas (10.0.0.1 y 10.0.0.2 en nuestro ejemplo).

---

## 4. Configurar la red manualmente (para entender)

Antes de escribir codigo C, hagamoslo a mano para entender
cada paso. Necesitas dos terminales.

### Terminal 1: crear contenedor sin red

```bash
# Crear un proceso en un nuevo network namespace
$ sudo unshare --net --pid --fork --mount-proc /bin/bash

# Dentro, verificar que no hay red
root# ip link
1: lo: <LOOPBACK> mtu 65536 qdisc noop state DOWN

# Obtener el PID de este shell (desde otra terminal)
```

### Terminal 2: configurar la red desde el host

```bash
# Obtener PID del proceso en el namespace
$ CPID=$(pgrep -f "unshare --net")

# 1. Crear par veth
$ sudo ip link add veth-host type veth peer name veth-cont

# 2. Mover un extremo al network namespace del contenedor
$ sudo ip link set veth-cont netns $CPID

# 3. Configurar IP en el extremo del host
$ sudo ip addr add 10.0.0.1/24 dev veth-host
$ sudo ip link set veth-host up

# 4. Verificar
$ ip addr show veth-host
    inet 10.0.0.1/24 scope global veth-host
```

### Terminal 1: configurar dentro del contenedor

```bash
# Ahora el contenedor tiene la interfaz veth-cont
root# ip link
1: lo: <LOOPBACK> mtu 65536 qdisc noop state DOWN
2: veth-cont: <BROADCAST,MULTICAST> mtu 1500 qdisc noop state DOWN

# Configurar IP
root# ip addr add 10.0.0.2/24 dev veth-cont
root# ip link set veth-cont up
root# ip link set lo up

# Probar conectividad
root# ping -c 1 10.0.0.1
PING 10.0.0.1: 64 bytes from 10.0.0.1: seq=0 ttl=64 time=0.1 ms
```

```
    Resultado:

    Host (10.0.0.1)  <---veth--->  Contenedor (10.0.0.2)
           ping OK                        ping OK
```

---

## 5. Ejecutar comandos de red desde C

La configuracion de red requiere herramientas como `ip`. Hay
dos formas de hacerlo desde C:

**Opcion A: Netlink sockets** (la forma "correcta")

Netlink es la interfaz del kernel para configuracion de red.
Potente pero compleja — requiere construir mensajes binarios
con structs de `linux/netlink.h` y `linux/rtnetlink.h`.

**Opcion B: fork+exec de `ip`** (la forma practica)

Ejecutar `ip link add ...` como subproceso. Mas simple, menos
eficiente, pero perfectamente funcional para un runtime
educativo.

Usamos la **opcion B** porque el codigo es legible y los
conceptos de red son el foco — no la API de netlink:

```c
#include <sys/wait.h>
#include <stdarg.h>

/*
 * run_cmd — ejecutar un comando con fork+exec.
 * Acepta una lista variable de argumentos terminada en NULL.
 * Retorna 0 si el comando salio con status 0, -1 en caso contrario.
 */
static int run_cmd(const char *cmd, ...)
{
    va_list ap;
    va_start(ap, cmd);

    /* Construir array de argumentos */
    const char *argv[64];
    argv[0] = cmd;
    int i = 1;
    const char *arg;
    while ((arg = va_arg(ap, const char *)) != NULL && i < 63) {
        argv[i++] = arg;
    }
    argv[i] = NULL;
    va_end(ap);

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        /* Hijo: ejecutar el comando */
        execvp(cmd, (char *const *)argv);
        perror(cmd);
        _exit(127);
    }

    /* Padre: esperar */
    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "[net] command failed: %s (status %d)\n",
                cmd, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return -1;
    }
    return 0;
}
```

Uso:

```c
run_cmd("ip", "link", "add", "veth-host", "type", "veth",
        "peer", "name", "veth-cont", NULL);
```

---

## 6. setup_network: implementacion completa

La configuracion de red se hace en dos partes:
- **Parte host** (en `main`, despues de `clone`): crear veth,
  mover un extremo, configurar el lado host
- **Parte contenedor** (en `child_func`): configurar el lado
  contenedor, levantar loopback

### Parte host: setup_network_host

```c
#define VETH_HOST "veth-host"
#define VETH_CONT "veth-cont"
#define HOST_IP   "10.0.0.1/24"
#define CONT_IP   "10.0.0.2/24"
#define CONT_GW   "10.0.0.1"

static int setup_network_host(pid_t child_pid)
{
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", child_pid);

    fprintf(stderr, "[net] creating veth pair\n");

    /* 1. Crear par veth */
    if (run_cmd("ip", "link", "add", VETH_HOST,
                "type", "veth", "peer", "name", VETH_CONT, NULL) != 0) {
        return -1;
    }

    /* 2. Mover veth-cont al network namespace del hijo */
    if (run_cmd("ip", "link", "set", VETH_CONT,
                "netns", pid_str, NULL) != 0) {
        /* Cleanup: eliminar el par si falla */
        run_cmd("ip", "link", "del", VETH_HOST, NULL);
        return -1;
    }

    /* 3. Configurar IP en el extremo del host */
    if (run_cmd("ip", "addr", "add", HOST_IP,
                "dev", VETH_HOST, NULL) != 0) {
        return -1;
    }

    /* 4. Levantar la interfaz del host */
    if (run_cmd("ip", "link", "set", VETH_HOST, "up", NULL) != 0) {
        return -1;
    }

    fprintf(stderr, "[net] host side ready: %s = %s\n", VETH_HOST, HOST_IP);
    return 0;
}
```

### Parte contenedor: setup_network_container

```c
static int setup_network_container(void)
{
    /* 1. Levantar loopback */
    if (run_cmd("ip", "link", "set", "lo", "up", NULL) != 0) {
        return -1;
    }

    /* 2. Configurar IP en veth-cont */
    if (run_cmd("ip", "addr", "add", CONT_IP,
                "dev", VETH_CONT, NULL) != 0) {
        return -1;
    }

    /* 3. Levantar la interfaz */
    if (run_cmd("ip", "link", "set", VETH_CONT, "up", NULL) != 0) {
        return -1;
    }

    /* 4. Ruta default via el host */
    if (run_cmd("ip", "route", "add", "default",
                "via", CONT_GW, NULL) != 0) {
        return -1;
    }

    fprintf(stderr, "[net] container side ready: %s = %s\n",
            VETH_CONT, CONT_IP);
    return 0;
}
```

### Cleanup: eliminar la interfaz del host

Cuando el contenedor termina, su network namespace se destruye
y `veth-cont` desaparece automaticamente. Pero `veth-host`
tambien desaparece porque al eliminar un extremo de un par
veth, el otro se elimina automaticamente:

```
    Contenedor termina
         |
         v
    Network namespace se destruye
         |
         v
    veth-cont se destruye
         |
         v
    veth-host se destruye automaticamente (par veth es simetrico)
```

Si el padre necesita cleanup explicito (por ejemplo, si el
contenedor muere de forma anormal), se puede hacer:

```c
static void cleanup_network(void)
{
    /* Intentar eliminar — no importa si ya no existe */
    run_cmd("ip", "link", "del", VETH_HOST, NULL);
}
```

### Diagrama del flujo completo

```
    main():
    |-- clone(child_func, CLONE_NEWNET | ...)  --> child_pid
    |
    |-- setup_network_host(child_pid):
    |   |-- ip link add veth-host type veth peer name veth-cont
    |   |-- ip link set veth-cont netns $child_pid
    |   |-- ip addr add 10.0.0.1/24 dev veth-host
    |   |-- ip link set veth-host up
    |
    |   (child_func ejecuta en paralelo)
    |   |-- setup_network_container():
    |   |   |-- ip link set lo up
    |   |   |-- ip addr add 10.0.0.2/24 dev veth-cont
    |   |   |-- ip link set veth-cont up
    |   |   |-- ip route add default via 10.0.0.1
    |   |
    |   |-- execvp("/bin/sh")
    |
    |-- waitpid(child_pid)
    |-- cleanup
```

### Sincronizacion entre padre e hijo

Hay un problema de timing: el padre necesita mover `veth-cont`
al namespace del hijo, y el hijo necesita configurar `veth-cont`.
Si el hijo intenta configurar antes de que el padre haya movido
la interfaz, fallara.

Solucion: usar un **pipe** para sincronizar:

```c
struct container_args {
    const char *rootfs;
    char *const *argv;
    int sync_fd;    /* NUEVO: fd del pipe para sincronizacion */
};
```

```c
/* main(): */
int sync_pipe[2];
pipe(sync_pipe);

/* ... clone() ... */

/* Padre hace setup de red */
setup_network_host(child_pid);
cgroup_setup(...);

/* Senalizar al hijo que puede continuar */
close(sync_pipe[1]);  /* cerrar el extremo de escritura = senal */

/* ... waitpid ... */
```

```c
/* child_func(): */
static int child_func(void *arg)
{
    struct container_args *ca = arg;

    /* Esperar a que el padre termine la configuracion */
    char buf;
    read(ca->sync_fd, &buf, 1);  /* bloquea hasta que padre cierre el pipe */
    close(ca->sync_fd);

    /* Ahora es seguro configurar la red del contenedor */
    sethostname(HOSTNAME, strlen(HOSTNAME));
    setup_network_container();
    setup_root(ca->rootfs);
    setup_proc();
    execvp(ca->argv[0], ca->argv);
    perror("execvp");
    return 1;
}
```

```
    Padre                          Hijo
    ------                         ------
    clone() ------------------->   child_func()
    setup_network_host()           read(sync_fd) -- bloquea
    cgroup_setup()                      |
    close(sync_pipe[1]) --------->  read() retorna (EOF)
    waitpid()                      setup_network_container()
                                   setup_root()
                                   execvp()
```

---

## 7. NAT: acceso a internet desde el contenedor

Con la configuracion anterior, el contenedor puede comunicarse
con el host (10.0.0.1 <-> 10.0.0.2), pero **no** con el mundo
exterior. Para eso necesitamos **NAT** (Network Address
Translation) en el host:

```
    Sin NAT:
    Contenedor (10.0.0.2) --> Host (10.0.0.1) --> Internet
                                                    X (bloqueado)
    El router/ISP no sabe como rutear paquetes a 10.0.0.2

    Con NAT:
    Contenedor (10.0.0.2) --> Host (10.0.0.1) --> NAT --> Internet
                                                 ^
                                    El host reescribe la IP de origen
                                    de 10.0.0.2 a su propia IP publica
```

### Configurar NAT en el host

```bash
# 1. Habilitar IP forwarding (el host actua como router)
$ echo 1 > /proc/sys/net/ipv4/ip_forward

# 2. Agregar regla de NAT (masquerade)
# "Los paquetes de 10.0.0.0/24 que salgan por la interfaz
#  del host reescriben su IP de origen"
$ iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -j MASQUERADE

# 3. Permitir forwarding para la subred del contenedor
$ iptables -A FORWARD -s 10.0.0.0/24 -j ACCEPT
$ iptables -A FORWARD -d 10.0.0.0/24 -j ACCEPT
```

### Desde C

```c
static int setup_nat(void)
{
    /* Habilitar IP forwarding */
    if (write_file("/proc/sys/net/ipv4/ip_forward", "1") != 0) {
        return -1;
    }

    /* NAT masquerade */
    if (run_cmd("iptables", "-t", "nat", "-A", "POSTROUTING",
                "-s", "10.0.0.0/24", "-j", "MASQUERADE", NULL) != 0) {
        return -1;
    }

    /* Permitir forwarding */
    if (run_cmd("iptables", "-A", "FORWARD",
                "-s", "10.0.0.0/24", "-j", "ACCEPT", NULL) != 0) {
        return -1;
    }
    if (run_cmd("iptables", "-A", "FORWARD",
                "-d", "10.0.0.0/24", "-j", "ACCEPT", NULL) != 0) {
        return -1;
    }

    fprintf(stderr, "[net] NAT configured\n");
    return 0;
}
```

### DNS dentro del contenedor

El contenedor tambien necesita un servidor DNS configurado.
El archivo `/etc/resolv.conf` del rootfs debe apuntar a un
DNS publico:

```c
static int setup_dns(void)
{
    /* Escribir resolv.conf dentro del contenedor
       (llamar DESPUES de pivot_root) */
    int fd = open("/etc/resolv.conf", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open /etc/resolv.conf");
        return -1;
    }
    const char *dns = "nameserver 8.8.8.8\nnameserver 8.8.4.4\n";
    write(fd, dns, strlen(dns));
    close(fd);
    return 0;
}
```

### Cleanup de NAT

```c
static void cleanup_nat(void)
{
    run_cmd("iptables", "-t", "nat", "-D", "POSTROUTING",
            "-s", "10.0.0.0/24", "-j", "MASQUERADE", NULL);
    run_cmd("iptables", "-D", "FORWARD",
            "-s", "10.0.0.0/24", "-j", "ACCEPT", NULL);
    run_cmd("iptables", "-D", "FORWARD",
            "-d", "10.0.0.0/24", "-j", "ACCEPT", NULL);
}
```

---

## 8. Integrar en container.c

### Nueva opcion: --net

```bash
sudo ./container --net --mem 256M --pids 64 ./rootfs /bin/sh
```

Cuando se pasa `--net`, el runtime:
1. Agrega `CLONE_NEWNET` a los flags de clone
2. Ejecuta `setup_network_host()` y `setup_nat()` en el padre
3. Ejecuta `setup_network_container()` y `setup_dns()` en el hijo
4. Limpia NAT y veth al salir

### Cambios en la configuracion

```c
struct container_config {
    const char *rootfs;
    char *const *argv;
    const char *mem_limit;
    int cpu_percent;
    int pids_limit;
    int enable_net;    /* NUEVO */
};
```

### Cambios en el parser de argumentos

```c
} else if (strcmp(argv[argi], "--net") == 0) {
    cfg.enable_net = 1;
}
```

### Cambios en clone flags

```c
int clone_flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS
                | CLONE_NEWIPC | SIGCHLD;
if (cfg.enable_net) {
    clone_flags |= CLONE_NEWNET;
}
```

---

## 9. container.c final completo

Este es el programa integrado con todas las funcionalidades
de S01, S02 y S03:

```c
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>

#define STACK_SIZE (1024 * 1024)
#define HOSTNAME   "container"
#define CGROUP_ROOT "/sys/fs/cgroup"

#define VETH_HOST "veth-host"
#define VETH_CONT "veth-cont"
#define HOST_IP   "10.0.0.1/24"
#define CONT_IP   "10.0.0.2/24"
#define CONT_GW   "10.0.0.1"

/* ======== Globals for signal cleanup ======== */

static volatile sig_atomic_t g_child_pid = 0;
static char g_cgroup_path[512] = {0};
static int g_net_enabled = 0;

/* ======== Utility functions ======== */

static int write_file(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY);
    if (fd == -1) { perror(path); return -1; }
    ssize_t len = strlen(value);
    ssize_t ret = write(fd, value, len);
    close(fd);
    return (ret == len) ? 0 : -1;
}

static int run_cmd(const char *cmd, ...)
{
    va_list ap;
    va_start(ap, cmd);
    const char *argv[64];
    argv[0] = cmd;
    int i = 1;
    const char *arg;
    while ((arg = va_arg(ap, const char *)) != NULL && i < 63)
        argv[i++] = arg;
    argv[i] = NULL;
    va_end(ap);

    pid_t pid = fork();
    if (pid == -1) { perror("fork"); return -1; }
    if (pid == 0) {
        execvp(cmd, (char *const *)argv);
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

static int pivot_root(const char *new_root, const char *put_old)
{
    return syscall(SYS_pivot_root, new_root, put_old);
}

/* ======== Filesystem setup (S01) ======== */

static int setup_root(const char *rootfs)
{
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        perror("mount MS_PRIVATE"); return -1;
    }
    if (mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL) == -1) {
        perror("mount bind"); return -1;
    }
    char old_root[256];
    snprintf(old_root, sizeof(old_root), "%s/old_root", rootfs);
    if (mkdir(old_root, 0700) == -1 && errno != EEXIST) {
        perror("mkdir old_root"); return -1;
    }
    if (pivot_root(rootfs, old_root) == -1) {
        perror("pivot_root"); return -1;
    }
    if (chdir("/") == -1) {
        perror("chdir"); return -1;
    }
    if (umount2("/old_root", MNT_DETACH) == -1) {
        perror("umount2"); return -1;
    }
    rmdir("/old_root");
    return 0;
}

static int setup_proc(void)
{
    if (mkdir("/proc", 0555) == -1 && errno != EEXIST) {
        perror("mkdir /proc"); return -1;
    }
    if (mount("proc", "/proc", "proc",
              MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) == -1) {
        perror("mount /proc"); return -1;
    }
    return 0;
}

/* ======== Cgroup setup (S02) ======== */

struct container_config {
    const char *rootfs;
    char *const *argv;
    const char *mem_limit;
    int cpu_percent;
    int pids_limit;
    int enable_net;
};

static int cgroup_setup(const char *path, const struct container_config *cfg,
                        pid_t child_pid)
{
    if (mkdir(path, 0755) == -1 && errno != EEXIST) {
        perror("mkdir cgroup"); return -1;
    }

    char file[512], buf[64];
    snprintf(file, sizeof(file), "%s/cgroup.procs", path);
    snprintf(buf, sizeof(buf), "%d", child_pid);
    if (write_file(file, buf) == -1) return -1;

    if (cfg->mem_limit) {
        snprintf(file, sizeof(file), "%s/memory.max", path);
        if (write_file(file, cfg->mem_limit) == -1) return -1;
        snprintf(file, sizeof(file), "%s/memory.swap.max", path);
        write_file(file, "0");
        fprintf(stderr, "[cgroup] memory.max = %s\n", cfg->mem_limit);
    }
    if (cfg->cpu_percent > 0) {
        snprintf(file, sizeof(file), "%s/cpu.max", path);
        snprintf(buf, sizeof(buf), "%d 100000", cfg->cpu_percent * 1000);
        if (write_file(file, buf) == -1) return -1;
        fprintf(stderr, "[cgroup] cpu.max = %s\n", buf);
    }
    if (cfg->pids_limit > 0) {
        snprintf(file, sizeof(file), "%s/pids.max", path);
        snprintf(buf, sizeof(buf), "%d", cfg->pids_limit);
        if (write_file(file, buf) == -1) return -1;
        fprintf(stderr, "[cgroup] pids.max = %s\n", buf);
    }
    return 0;
}

/* ======== Network setup (S03) ======== */

static int setup_network_host(pid_t child_pid)
{
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", child_pid);

    if (run_cmd("ip", "link", "add", VETH_HOST,
                "type", "veth", "peer", "name", VETH_CONT, NULL) != 0)
        return -1;
    if (run_cmd("ip", "link", "set", VETH_CONT,
                "netns", pid_str, NULL) != 0) {
        run_cmd("ip", "link", "del", VETH_HOST, NULL);
        return -1;
    }
    if (run_cmd("ip", "addr", "add", HOST_IP,
                "dev", VETH_HOST, NULL) != 0)
        return -1;
    if (run_cmd("ip", "link", "set", VETH_HOST, "up", NULL) != 0)
        return -1;

    fprintf(stderr, "[net] host side ready: %s = %s\n", VETH_HOST, HOST_IP);
    return 0;
}

static int setup_nat(void)
{
    write_file("/proc/sys/net/ipv4/ip_forward", "1");
    run_cmd("iptables", "-t", "nat", "-A", "POSTROUTING",
            "-s", "10.0.0.0/24", "-j", "MASQUERADE", NULL);
    run_cmd("iptables", "-A", "FORWARD",
            "-s", "10.0.0.0/24", "-j", "ACCEPT", NULL);
    run_cmd("iptables", "-A", "FORWARD",
            "-d", "10.0.0.0/24", "-j", "ACCEPT", NULL);
    fprintf(stderr, "[net] NAT configured\n");
    return 0;
}

static int setup_network_container(void)
{
    run_cmd("ip", "link", "set", "lo", "up", NULL);
    if (run_cmd("ip", "addr", "add", CONT_IP,
                "dev", VETH_CONT, NULL) != 0)
        return -1;
    if (run_cmd("ip", "link", "set", VETH_CONT, "up", NULL) != 0)
        return -1;
    run_cmd("ip", "route", "add", "default",
            "via", CONT_GW, NULL);
    fprintf(stderr, "[net] container side ready: %s = %s\n",
            VETH_CONT, CONT_IP);
    return 0;
}

static int setup_dns(void)
{
    int fd = open("/etc/resolv.conf", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return -1;
    const char *dns = "nameserver 8.8.8.8\nnameserver 8.8.4.4\n";
    write(fd, dns, strlen(dns));
    close(fd);
    return 0;
}

static void cleanup_nat(void)
{
    run_cmd("iptables", "-t", "nat", "-D", "POSTROUTING",
            "-s", "10.0.0.0/24", "-j", "MASQUERADE", NULL);
    run_cmd("iptables", "-D", "FORWARD",
            "-s", "10.0.0.0/24", "-j", "ACCEPT", NULL);
    run_cmd("iptables", "-D", "FORWARD",
            "-d", "10.0.0.0/24", "-j", "ACCEPT", NULL);
}

/* ======== Child process ======== */

struct container_args {
    const char *rootfs;
    char *const *argv;
    int sync_fd;
    int enable_net;
};

static int child_func(void *arg)
{
    struct container_args *ca = arg;

    /* Wait for parent to finish setup */
    char buf;
    read(ca->sync_fd, &buf, 1);
    close(ca->sync_fd);

    if (sethostname(HOSTNAME, strlen(HOSTNAME)) == -1)
        perror("sethostname");

    /* Network (before pivot_root — needs ip from host PATH) */
    if (ca->enable_net) {
        if (setup_network_container() == -1)
            fprintf(stderr, "[container] network setup failed\n");
    }

    /* Filesystem */
    if (setup_root(ca->rootfs) == -1) return 1;
    fprintf(stderr, "[container] pivot_root: OK\n");

    if (setup_proc() == -1) return 1;
    fprintf(stderr, "[container] /proc mounted\n");

    /* DNS (after pivot_root — writes to container /etc/resolv.conf) */
    if (ca->enable_net) {
        setup_dns();
    }

    /* Execute command */
    fprintf(stderr, "[container] executing: %s\n", ca->argv[0]);
    execvp(ca->argv[0], ca->argv);
    perror("execvp");
    return 1;
}

/* ======== Signal handler ======== */

static void cleanup_handler(int sig)
{
    if (g_child_pid > 0) {
        kill(g_child_pid, SIGKILL);
        waitpid(g_child_pid, NULL, 0);
    }
    if (g_cgroup_path[0])
        rmdir(g_cgroup_path);
    if (g_net_enabled)
        cleanup_nat();
    _exit(128 + sig);
}

/* ======== Main ======== */

int main(int argc, char *argv[])
{
    struct container_config cfg = {0};
    int argi = 1;

    while (argi < argc && argv[argi][0] == '-') {
        if (strcmp(argv[argi], "--mem") == 0 && argi + 1 < argc)
            cfg.mem_limit = argv[++argi];
        else if (strcmp(argv[argi], "--cpu") == 0 && argi + 1 < argc)
            cfg.cpu_percent = atoi(argv[++argi]);
        else if (strcmp(argv[argi], "--pids") == 0 && argi + 1 < argc)
            cfg.pids_limit = atoi(argv[++argi]);
        else if (strcmp(argv[argi], "--net") == 0)
            cfg.enable_net = 1;
        else {
            fprintf(stderr,
                "Usage: %s [--mem <bytes>] [--cpu <%%>] [--pids <n>] "
                "[--net] <rootfs> <cmd> [args...]\n", argv[0]);
            return 1;
        }
        argi++;
    }

    if (argc - argi < 2) {
        fprintf(stderr,
            "Usage: %s [--mem <bytes>] [--cpu <%%>] [--pids <n>] "
            "[--net] <rootfs> <cmd> [args...]\n", argv[0]);
        return 1;
    }

    cfg.rootfs = argv[argi];
    cfg.argv = &argv[argi + 1];

    struct stat st;
    if (stat(cfg.rootfs, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a valid directory\n", cfg.rootfs);
        return 1;
    }

    /* Signal handler for cleanup */
    struct sigaction sa = {.sa_handler = cleanup_handler};
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Cgroup name */
    int use_cgroup = (cfg.mem_limit || cfg.cpu_percent > 0 || cfg.pids_limit > 0);
    if (use_cgroup) {
        snprintf(g_cgroup_path, sizeof(g_cgroup_path),
                 "%s/container-%d", CGROUP_ROOT, getpid());
    }
    g_net_enabled = cfg.enable_net;

    /* Sync pipe */
    int sync_pipe[2];
    if (pipe(sync_pipe) == -1) {
        perror("pipe"); return 1;
    }

    struct container_args ca = {
        .rootfs = cfg.rootfs,
        .argv = cfg.argv,
        .sync_fd = sync_pipe[0],
        .enable_net = cfg.enable_net,
    };

    /* Stack */
    char *stack = mmap(NULL, STACK_SIZE,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) { perror("mmap"); return 1; }

    int clone_flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS
                    | CLONE_NEWIPC | SIGCHLD;
    if (cfg.enable_net)
        clone_flags |= CLONE_NEWNET;

    pid_t child_pid = clone(child_func, stack + STACK_SIZE,
                            clone_flags, &ca);
    if (child_pid == -1) {
        perror("clone"); munmap(stack, STACK_SIZE); return 1;
    }
    close(sync_pipe[0]);  /* parent doesn't read */
    g_child_pid = child_pid;

    fprintf(stderr, "[parent] container PID: %d\n", child_pid);

    /* Parent setup: cgroup + network */
    if (use_cgroup)
        cgroup_setup(g_cgroup_path, &cfg, child_pid);

    if (cfg.enable_net) {
        setup_network_host(child_pid);
        setup_nat();
    }

    /* Signal child to proceed */
    close(sync_pipe[1]);

    /* Wait */
    int status;
    waitpid(child_pid, &status, 0);

    /* Cleanup */
    if (cfg.enable_net)
        cleanup_nat();
    if (use_cgroup) {
        rmdir(g_cgroup_path);
        fprintf(stderr, "[cgroup] removed: %s\n", g_cgroup_path);
    }
    munmap(stack, STACK_SIZE);

    if (WIFEXITED(status)) {
        fprintf(stderr, "[parent] exited with status %d\n",
                WEXITSTATUS(status));
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "[parent] killed by signal %d\n", WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }
    return 1;
}
```

---

## 10. Probando el runtime completo

### Compilar

```bash
gcc -Wall -Wextra -std=c11 -o container container.c
```

### Ejecucion basica (sin red)

```bash
$ sudo ./container ./rootfs /bin/sh
[parent] container PID: 12345
[container] pivot_root: OK
[container] /proc mounted
[container] executing: /bin/sh
container# ps aux
PID   USER     COMMAND
    1 root     /bin/sh
    7 root     ps aux
container# hostname
container
container# exit
[parent] exited with status 0
```

### Con limites de recursos

```bash
$ sudo ./container --mem 67108864 --cpu 50 --pids 32 ./rootfs /bin/sh
[parent] container PID: 12346
[cgroup] memory.max = 67108864
[cgroup] cpu.max = 50000 100000
[cgroup] pids.max = 32
[container] pivot_root: OK
[container] /proc mounted
[container] executing: /bin/sh
container# free -m
# Muestra ~64MB disponibles
container# exit
[cgroup] removed: /sys/fs/cgroup/container-12345
[parent] exited with status 0
```

### Con red

```bash
$ sudo ./container --net --pids 64 ./rootfs /bin/sh
[parent] container PID: 12347
[net] host side ready: veth-host = 10.0.0.1/24
[net] NAT configured
[net] container side ready: veth-cont = 10.0.0.2/24
[container] pivot_root: OK
[container] /proc mounted
[container] executing: /bin/sh

container# ip addr
1: lo: <LOOPBACK,UP> mtu 65536
    inet 127.0.0.1/8 scope host lo
2: veth-cont: <BROADCAST,MULTICAST,UP> mtu 1500
    inet 10.0.0.2/24 scope global veth-cont

container# ping -c 1 10.0.0.1
PING 10.0.0.1: 64 bytes, seq=0 ttl=64 time=0.1ms

container# ping -c 1 8.8.8.8
PING 8.8.8.8: 64 bytes, seq=0 ttl=117 time=12ms

container# wget -qO- ifconfig.me
<tu IP publica>

container# exit
[parent] exited with status 0
```

### Comparacion con Docker

```bash
# Nuestro runtime:
$ sudo ./container --net --mem 268435456 --pids 64 ./rootfs /bin/sh

# Docker equivalente:
$ docker run -it --memory 256m --pids-limit 64 alpine /bin/sh
```

Nuestro runtime hace esencialmente lo mismo que Docker a
nivel de kernel: namespaces + cgroups + filesystem aislado.
Docker agrega capas encima (imagen layers, networking avanzado,
storage drivers, API REST, etc.), pero la base es identica.

---

## 11. Errores comunes

### Error 1: ip no encontrado dentro del contenedor

```bash
container# ip addr
/bin/sh: ip: not found
```

El rootfs de Alpine incluye `ip` en `/sbin/ip` (via busybox).
Si usas otro rootfs minimo, puede faltar. La solucion para
`setup_network_container()` es ejecutar `ip` **antes** de
`pivot_root()`, cuando todavia tienes acceso al `ip` del host:

```c
static int child_func(void *arg) {
    // ...
    setup_network_container();  // ANTES de setup_root()
    setup_root(ca->rootfs);     // pivot_root cambia el filesystem
    // ...
}
```

Nuestro `container.c` final ya hace esto en el orden correcto.

### Error 2: veth-host ya existe

```bash
[net] command failed: ip (status 2)
# RTNETLINK answers: File exists
```

Si una ejecucion anterior no limpio correctamente:

```bash
# Limpiar manualmente
$ sudo ip link del veth-host 2>/dev/null
```

Para robustez, limpiar al inicio:

```c
static int setup_network_host(pid_t child_pid)
{
    /* Limpiar restos de ejecuciones anteriores */
    run_cmd("ip", "link", "del", VETH_HOST, NULL);  /* ignorar error */
    // ... crear nuevo par ...
}
```

### Error 3: contenedor no tiene acceso a internet

Checklist de depuracion:

```bash
# 1. Verificar IP forwarding en el host
$ cat /proc/sys/net/ipv4/ip_forward
1   # debe ser 1

# 2. Verificar regla NAT
$ sudo iptables -t nat -L POSTROUTING -n
MASQUERADE  all  --  10.0.0.0/24  0.0.0.0/0

# 3. Verificar ruta default en el contenedor
container# ip route
default via 10.0.0.1 dev veth-cont
10.0.0.0/24 dev veth-cont

# 4. Verificar DNS
container# cat /etc/resolv.conf
nameserver 8.8.8.8

# 5. Probar paso a paso
container# ping -c 1 10.0.0.1     # gateway
container# ping -c 1 8.8.8.8      # internet (IP)
container# ping -c 1 google.com   # internet (DNS)
```

### Error 4: race condition en setup de red

```
[net] command failed: ip (status 1)
# Cannot find device "veth-cont"
```

El hijo intenta configurar `veth-cont` antes de que el padre
lo haya movido al namespace. Solucion: usar el pipe de
sincronizacion (ya implementado en `container.c` final).

### Error 5: reglas de iptables se acumulan

```bash
# Cada ejecucion agrega reglas sin limpiar las anteriores
$ sudo iptables -t nat -L POSTROUTING -n
MASQUERADE  10.0.0.0/24
MASQUERADE  10.0.0.0/24   # duplicada!
MASQUERADE  10.0.0.0/24   # triplicada!
```

```c
// BIEN: limpiar antes de agregar (idempotente)
static int setup_nat(void)
{
    /* Eliminar reglas existentes (ignorar errores si no existen) */
    cleanup_nat();

    /* Ahora agregar las reglas frescas */
    write_file("/proc/sys/net/ipv4/ip_forward", "1");
    // ...
}
```

---

## 12. Cheatsheet

```
RED DEL CONTENEDOR — REFERENCIA RAPIDA
========================================

CLONE_NEWNET:
  Crea network namespace vacio (ni loopback funciona)
  Requiere configuracion manual para tener red

Par veth (cable virtual):
  ip link add veth-host type veth peer name veth-cont
  ip link set veth-cont netns $PID     # mover al contenedor
  ip addr add 10.0.0.1/24 dev veth-host
  ip link set veth-host up

Dentro del contenedor:
  ip link set lo up
  ip addr add 10.0.0.2/24 dev veth-cont
  ip link set veth-cont up
  ip route add default via 10.0.0.1

NAT (acceso a internet):
  echo 1 > /proc/sys/net/ipv4/ip_forward
  iptables -t nat -A POSTROUTING -s 10.0.0.0/24 -j MASQUERADE
  iptables -A FORWARD -s 10.0.0.0/24 -j ACCEPT
  iptables -A FORWARD -d 10.0.0.0/24 -j ACCEPT

DNS:
  echo "nameserver 8.8.8.8" > /etc/resolv.conf

Sincronizacion padre-hijo:
  pipe() antes de clone()
  Hijo: read(sync_fd) bloquea
  Padre: close(write_end) desbloquea al hijo

Cleanup:
  veth se destruye auto al destruir el namespace
  Reglas iptables: borrar con -D en vez de -A

Orden de operaciones en el hijo:
  1. read(sync_fd)              esperar al padre
  2. sethostname()              UTS namespace
  3. setup_network_container()  ANTES de pivot_root (necesita ip)
  4. setup_root() (pivot_root)  filesystem aislado
  5. setup_proc()               montar /proc
  6. setup_dns()                DESPUES de pivot_root (escribe en rootfs)
  7. execvp()                   ejecutar comando

container.c final soporta:
  sudo ./container [opciones] <rootfs> <comando> [args...]
  --mem <bytes>    limite de memoria
  --cpu <percent>  limite de CPU
  --pids <max>     limite de procesos
  --net            habilitar red (veth + NAT)
```

---

## 13. Ejercicios

### Ejercicio 1: port forwarding

Implementa port forwarding para que un servicio dentro del
contenedor sea accesible desde el host:

```bash
# Objetivo:
$ sudo ./container --net --port 8080:80 ./rootfs /bin/sh
# El puerto 8080 del host redirige al puerto 80 del contenedor
```

Necesitas agregar una regla de iptables DNAT:

```bash
# En el host, despues de setup_network_host():
iptables -t nat -A PREROUTING -p tcp --dport 8080 \
    -j DNAT --to-destination 10.0.0.2:80
```

Tareas:
1. Agrega la opcion `--port HOST:CONT` al parser
2. Implementa `setup_port_forward(int host_port, int cont_port)`
   que agregue la regla DNAT
3. Implementa el cleanup correspondiente (`-D` en vez de `-A`)
4. Prueba: dentro del contenedor, lanza un servidor HTTP simple:
   `echo "hello" > /tmp/index.html && httpd -f -p 80 -h /tmp`
   Desde el host: `curl http://localhost:8080`

**Prediccion**: si el contenedor no tiene `--net`, el port
forwarding es posible? Por que?

> **Pregunta de reflexion**: Docker usa el mismo mecanismo
> (iptables DNAT) para `-p 8080:80`. Ejecuta `sudo iptables
> -t nat -L -n` en un host con Docker corriendo y busca las
> reglas DOCKER. Que similitudes ves con lo que implementaste?

### Ejercicio 2: multiples contenedores simultaneos

El runtime actual usa IPs hardcoded (10.0.0.1/10.0.0.2) y
nombres de veth fijos. Modifica para soportar multiples
instancias corriendo al mismo tiempo:

```c
/* Generar IPs y nombres unicos basados en un ID */
static void generate_network_config(int id,
    char *veth_host, char *veth_cont,
    char *host_ip, char *cont_ip)
{
    // TODO:
    // veth-host-1, veth-cont-1, 10.0.1.1/24, 10.0.1.2/24
    // veth-host-2, veth-cont-2, 10.0.2.1/24, 10.0.2.2/24
    // etc.
}
```

Tareas:
1. Usa el PID del padre como ID unico
2. Genera subredes diferentes: 10.0.ID.1/24, 10.0.ID.2/24
3. Genera nombres de veth unicos: veth-host-PID, veth-cont-PID
4. Ajusta las reglas de NAT para cubrir todas las subredes
   (o usa una regla mas amplia: `10.0.0.0/16`)
5. Prueba lanzando dos contenedores en terminales separadas
   y verifica que ambos tienen red independiente

> **Pregunta de reflexion**: Docker usa una red **bridge**
> (`docker0`) con una subred (172.17.0.0/16) y asigna IPs
> incrementalmente (172.17.0.2, 172.17.0.3, ...). Todos los
> contenedores se conectan al mismo bridge. Nuestro enfoque
> crea una subred separada por contenedor. Cuales son las
> ventajas y desventajas de cada enfoque? Que pasa si dos
> contenedores Docker necesitan comunicarse entre si?

### Ejercicio 3: verificar el aislamiento completo

Escribe un script de verificacion que ejecute dentro del
contenedor y confirme que todos los aislamientos funcionan:

```bash
#!/bin/sh
# verify.sh — ejecutar dentro del contenedor
echo "=== Isolation Verification ==="

echo ""
echo "--- PID namespace ---"
echo "My PID: $$"
echo "Process count: $(ls /proc | grep -c '^[0-9]')"
# Esperado: PID bajo, pocos procesos

echo ""
echo "--- UTS namespace ---"
echo "Hostname: $(hostname)"
# Esperado: "container"

echo ""
echo "--- Mount namespace ---"
echo "Root contents: $(ls /)"
# Esperado: rootfs de Alpine, no el host

echo ""
echo "--- Network namespace ---"
if command -v ip > /dev/null 2>&1; then
    echo "Interfaces: $(ip -brief link | wc -l)"
    ip -brief addr
else
    echo "ip command not available"
fi
# Esperado: solo lo y veth-cont

echo ""
echo "--- Resource limits ---"
# Intentar leer los limites (si /sys/fs/cgroup esta montado)
if [ -f /sys/fs/cgroup/memory.max ]; then
    echo "Memory limit: $(cat /sys/fs/cgroup/memory.max)"
fi
echo "(cgroup limits enforced by parent, not visible here)"

echo ""
echo "=== All checks passed ==="
```

Tareas:
1. Copia `verify.sh` al rootfs: `cp verify.sh rootfs/tmp/`
2. Ejecuta: `sudo ./container --net --mem 67108864 --pids 32
   ./rootfs /bin/sh /tmp/verify.sh`
3. Verifica que cada seccion muestra aislamiento correcto
4. Agrega una prueba de que no puedes ver procesos del host:
   intenta acceder a `/proc/1/cmdline` — que deberia mostrar
   tu propio proceso, no systemd

> **Pregunta de reflexion**: nuestro contenedor tiene 6 tipos
> de aislamiento: PID, mount, UTS, IPC, network, y limites via
> cgroups. Docker agrega `CLONE_NEWUSER` (UID mapping — ejecutar
> como root dentro pero como usuario fuera) y `seccomp` (filtro
> de syscalls — bloquear syscalls peligrosas como `reboot`).
> Sin `seccomp`, un root dentro del contenedor podria ejecutar
> `reboot()` y reiniciar el host. Que otras syscalls serian
> peligrosas de permitir dentro de un contenedor?
