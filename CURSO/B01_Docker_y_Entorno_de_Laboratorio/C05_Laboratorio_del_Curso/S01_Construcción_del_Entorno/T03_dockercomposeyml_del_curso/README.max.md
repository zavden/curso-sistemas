# T03 — docker-compose.yml del curso

## Diseño del entorno

El laboratorio del curso usa Docker Compose para gestionar dos contenedores de
desarrollo (Debian y AlmaLinux) que comparten el mismo código fuente:

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Host (tu máquina)                           │
│                                                                     │
│   ./lab/                                                            │
│   ├── compose.yml                                                   │
│   ├── dockerfiles/                                                  │
│   │   ├── debian/Dockerfile                                         │
│   │   └── alma/Dockerfile                                           │
│   └── src/                    ← Bind mount hacia ambos containers  │
│       ├── main.c                                                    │
│       └── Makefile                                                  │
└─────────────────────────────────────────────────────────────────────┘
                                 │
                                 │ docker compose up -d
                                 ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        Red: lab_default                             │
│                                                                     │
│   ┌───────────────────────┐       ┌───────────────────────┐       │
│   │     debian-dev        │       │       alma-dev         │       │
│   │                       │       │                        │       │
│   │  hostname: debian     │       │  hostname: alma        │       │
│   │  Image: lab-debian-dev│       │  Image: lab-alma-dev   │       │
│   │                       │       │                        │       │
│   │  /home/dev/workspace/ │◄──────┼─►/home/dev/workspace/  │       │
│   │    └── src/ ──────────┼───────┼─── src/               │       │
│   │         │            │       │       │                │       │
│   │         ▼            │       │       ▼                │       │
│   │   Named Volume:      │       │   Named Volume:        │       │
│   │   workspace          │◄──────┼──► workspace          │       │
│   │                       │       │                        │       │
│   └───────────────────────┘       └───────────────────────┘       │
│                                                                     │
│   - SYS_PTRACE enabled (gdb, strace, valgrind)                     │
│   - Interactive shells (stdin + TTY)                               │
└─────────────────────────────────────────────────────────────────────┘
```

---

## El Compose file

```yaml
services:
  debian-dev:
    build:
      context: ./dockerfiles/debian
    container_name: debian-dev
    hostname: debian
    volumes:
      - workspace:/home/dev/workspace
      - ./src:/home/dev/workspace/src
    cap_add:
      - SYS_PTRACE
    stdin_open: true
    tty: true

  alma-dev:
    build:
      context: ./dockerfiles/alma
    container_name: alma-dev
    hostname: alma
    volumes:
      - workspace:/home/dev/workspace
      - ./src:/home/dev/workspace/src
    cap_add:
      - SYS_PTRACE
    stdin_open: true
    tty: true

volumes:
  workspace:
```

---

## Explicación de cada opción

### `build: context`

Apunta al directorio que contiene el Dockerfile de cada imagen:

```
dockerfiles/
├── debian/
│   └── Dockerfile    ← context: ./dockerfiles/debian
└── alma/
    └── Dockerfile    ← context: ./dockerfiles/alma
```

El `context` define el directorio base para:
- `COPY` y `ADD` instrucciones en el Dockerfile
- Construcción de paths relativos

### `container_name` vs nombre generado

| Aspecto | `container_name` | Nombre generado por Compose |
|---|---|---|
| Valor | Fijo: `debian-dev` | `lab-debian-dev-1` |
| Persistencia | Entre `down`/`up` | Puede cambiar |
| Uso en scripts | Conveniente | Requriere interpolación |

Ventajas de nombres fijos:
- Scripts más simples: `docker exec debian-dev ...`
- Referencias predecibles en `/etc/hosts`

### `hostname`

Establece el nombre interno del contenedor (visible via `hostname` command y en
prompts de shell):

```bash
docker compose exec debian-dev hostname
# debian

docker compose exec alma-dev hostname
# alma
```

Esto también aparece en:
- Prompt de bash (`user@debian:~$`)
- Archivo `/etc/hostname`
- Logs del sistema

### `volumes` — dos tipos de montaje

```yaml
volumes:
  - workspace:/home/dev/workspace         # Named volume: persistencia
  - ./src:/home/dev/workspace/src         # Bind mount: código del host
```

#### Named volume `workspace`

| Aspecto | Descripción |
|---|---|
| Ubicación en host | `/var/lib/docker/volumes/lab_workspace/` |
| Persistencia | Sobrevive `down`, `up`, `restart` |
| Contenido | Configuración, historial, binarios compilados |
| Compartido | Mismo volume en ambos contenedores |

```bash
# Ver contenido del volume
docker volume inspect lab_workspace

# Resultado:
# [
#     {
#         "Name": "lab_workspace",
#         "Mountpoint": "/var/lib/docker/volumes/lab_workspace/_data",
#         ...
#     }
# ]
```

#### Bind mount `./src`

| Aspecto | Descripción |
|---|---|
| Fuente | Directorio `./src` del host |
| Destino | `/home/dev/workspace/src` en ambos contenedores |
| Sincronización | Inmediata (bidireccional) |
| Uso | Código fuente que editas en tu editor |

```
Host: ./src/main.c
        │
        │ bind mount (same fs)
        ▼
debian-dev: /home/dev/workspace/src/main.c
alma-dev:   /home/dev/workspace/src/main.c
```

**Precaución:** Los bind mounts comparten el contenido directamente. Si compilas
en Debian y luego ejecutas en AlmaLinux, el binario puede no funcionar
(glibc mismatch).

### `cap_add: SYS_PTRACE`

Permite a las herramientas de debugging funcionar:

| Herramienta | Syscall que requiere | Error sin capability |
|---|---|---|
| `gdb -p PID` | `ptrace(PTRACE_ATTACH, ...)` | "Operation not permitted" |
| `strace -p PID` | `ptrace(PTRACE_SYSCALL, ...)` | "Operation not permitted" |
| `valgrind ./prog` | `ptrace` interno | Funciona (usa diferente método) |

```yaml
cap_add:
  - SYS_PTRACE
```

**Nota de seguridad:** `SYS_PTRACE` es una capability poderosa. En producción,
limita su uso a contenedores de desarrollo. Para producción, considera:
- `gdbserver` en el contenedor, `gdb` en el host
- Contenedores separados para debugging

### `stdin_open` y `tty`

| Flag | Flag Docker | Efecto |
|---|---|---|
| `stdin_open: true` | `-i` | Mantiene stdin abierto aunque nadie lo lea |
| `tty: true` | `-t` | Asigna una pseudo-TTY |

Juntos, permiten shells interactivos:

```bash
# Sin stdin_open + tty: el shell cierra inmediatamente
docker compose exec debian-dev bash -c 'echo hi; sleep 10'
# Output: hi
# Exit code: 0 (inmediato, no esperó)

# Con stdin_open + tty: el shell permanece
docker compose exec debian-dev bash
# Entra a shell interactivo con prompt
```

---

## Estructura de directorios del lab

```bash
# Crear la estructura completa
mkdir -p lab/{dockerfiles/debian,dockerfiles/alma,src}
```

```
lab/
├── compose.yml              # Este archivo
├── dockerfiles/
│   ├── debian/
│   │   └── Dockerfile       # T01: Debian dev
│   └── alma/
│       └── Dockerfile       # T02: AlmaLinux dev
└── src/                     # Código fuente (creado por ti)
    ├── main.c
    ├── Makefile
    ├── hello.c
    └── ...
```

---

## Construir y usar el lab

```bash
cd lab

# Construir ambas imágenes desde cero
docker compose build

# Construir solo un servicio
docker compose build debian-dev

# Reconstruir con --no-cache
docker compose build --no-cache

# Arrancar ambos contenedores (modo detached)
docker compose up -d

# Ver estado
docker compose ps
# NAME         IMAGE          STATUS
# debian-dev   lab-debian-dev Up (2 minutes ago)
# alma-dev     lab-alma-dev   Up (2 minutes ago)

# Ver todos los contenedores (incluyendo parados)
docker compose ps -a

# Entrar al contenedor Debian (shell interactivo)
docker compose exec debian-dev bash

# Ejecutar comando sin entrar (útil en scripts)
docker compose exec debian-dev gcc --version

# Ver logs de ambos
docker compose logs

# Ver logs de un servicio específico
docker compose logs -f debian-dev

# Detener (preserva volúmenes con datos)
docker compose down

# Detener y eliminar TODO (volúmenes, redes, contenedores)
docker compose down -v

# Ver redes de compose
docker network ls | grep lab

# Inspecionar la red del compose
docker network inspect lab_default
```

---

## Flujo de trabajo diario

### Día típico en el lab

```bash
# 1. Levantar el lab
cd lab
docker compose up -d

# 2. Esperar a que esté listo
sleep 2
docker compose ps

# 3. Editar código en tu editor del host (VS Code, vim, etc.)
vim src/main.c

# 4. Compilar en Debian
docker compose exec debian-dev bash -c 'cd src && gcc -Wall -o main main.c && ./main'

# 5. Compilar en AlmaLinux (mismo código, otra distro)
docker compose exec alma-dev bash -c 'cd src && gcc -Wall -o main main.c && ./main'

# 6. Debug en Debian
docker compose exec debian-dev bash -c 'cd src && gcc -g -o main main.c && gdb ./main'

# 7. Ver resultado en AlmaLinux también
docker compose exec alma-dev bash -c 'cd src && ./main'

# 8. Al terminar: detener
docker compose down
```

### Uso de make (recomendado)

Para evitar comandos largos, usa un Makefile:

```bash
# En ./src/Makefile
CC = gcc
CFLAGS = -Wall -g

all: main

main: main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f main

.PHONY: all clean
```

```bash
# Compilar en Debian
docker compose exec debian-dev bash -c 'cd src && make clean && make && ./main'

# Compilar en AlmaLinux
docker compose exec alma-dev bash -c 'cd src && make clean && make && ./main'
```

---

## Binarios compilados: separar por distro

Los binarios compilados en Debian **no son necesariamente compatibles** con
AlmaLinux y viceversa. Causas:

| Factor | Debian | AlmaLinux |
|---|---|---|
| glibc | 2.36 | 2.34 |
| libstdc++ | GCC 12 | GCC 11 |
| Kernel | 6.1 | 5.14 |

### Verificar incompatibilidad

```bash
# Compilar en Debian
docker compose exec debian-dev bash -c 'cd src && gcc -o hello hello.c'

# Ver tipo de binary
docker compose exec debian-dev file src/hello
# hello: ELF 64-bit LSB executable, x86-64, version 1 (SYSV),
#        dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2

# Intentar ejecutar en AlmaLinux (puede fallar si glibc es incompatible)
docker compose exec alma-dev bash -c 'cd src && ./hello'
# Puede dar error: version 'GLIBC_2.36' not found
```

### Soluciones

**Opción 1: Directorios de build separados**

```bash
src/
├── main.c
├── Makefile
├── build-debian/     ← compilar aquí desde debian-dev
│   └── main
└── build-alma/       ← compilar aquí desde alma-dev
    └── main
```

```bash
# Compilar en Debian
docker compose exec debian-dev bash -c 'cd src && gcc -o build-debian/main main.c'

# Compilar en AlmaLinux
docker compose exec alma-dev bash -c 'cd src && gcc -o build-alma/main main.c'

# Ejecutar donde corresponde
docker compose exec debian-dev bash -c 'cd src && ./build-debian/main'
docker compose exec alma-dev bash -c 'cd src && ./build-alma/main'
```

**Opción 2: Makefile inteligente**

```makefile
# En src/Makefile
ifeq ($(shell hostname),debian)
    BUILD_DIR = build-debian
else ifeq ($(shell hostname),alma)
    BUILD_DIR = build-alma
else
    BUILD_DIR = build-host
endif

all: $(BUILD_DIR)/main

$(BUILD_DIR)/main: main.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<
```

---

## Comunicación entre contenedores

Ambos contenedores están en la misma red de Compose (`lab_default`) y pueden
comunicarse por nombre de servicio:

```bash
# Ping por nombre de servicio
docker compose exec debian-dev ping -c 1 alma-dev
# PING alma-dev (172.20.0.2) 56(84) bytes of data.

# Ping por nombre de contenedor
docker compose exec debian-dev ping -c 1 alma-dev

# Ping por hostname
docker compose exec debian-dev ping -c 1 alma
```

### Ver IPs en la red

```bash
docker network inspect lab_default -f '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{println}}{{end}}'
# alma-dev: 172.20.0.2/16
# debian-dev: 172.20.0.3/16
```

### Ejemplo: Netcat como servidor/cliente

```bash
# Terminal 1: AlmaLinux como servidor echo
docker compose exec alma-dev bash -c 'ncat -l -p 9090 -e /bin/cat --keep-open &'

# Terminal 2: Debian como cliente
docker compose exec debian-dev bash -c 'echo "Hello from Debian" | ncat alma-dev 9090'
# Hello from Debian
```

---

## Ejercicios

### Ejercicio 1 — Construir el lab completo

```bash
# Crear estructura de directorios
mkdir -p /tmp/lab/{dockerfiles/debian,dockerfiles/alma,src}

# Crear compose.yml (usar contenido de arriba)

# Construir ambas imágenes
cd /tmp/lab
docker compose build

# Ver estado
docker compose ps

# Esperar y verificar
sleep 5
docker compose exec debian-dev echo "Debian OK"
docker compose exec alma-dev echo "Alma OK"
```

---

### Ejercicio 2 — Código compartido entre distros

```bash
# Crear un programa de prueba en el host
cat > /tmp/lab/src/hello.c << 'EOF'
#include <stdio.h>
#include <sys/utsname.h>

int main(void) {
    struct utsname u;
    uname(&u);
    printf("Hello from %s (glibc %s)\n", u.sysname, u.version);
    return 0;
}
EOF

# Compilar y ejecutar en Debian
docker compose exec debian-dev bash -c 'cd src && gcc -o hello hello.c && ./hello'

# Compilar y ejecutar en AlmaLinux
docker compose exec alma-dev bash -c 'cd src && gcc -o hello hello.c && ./hello'

# Verificar que los binarios son diferentes (distinta glibc)
docker compose exec debian-dev file src/hello
docker compose exec alma-dev file src/hello

# Limpiar
docker compose exec debian-dev bash -c 'cd src && rm -f hello'
docker compose exec alma-dev bash -c 'cd src && rm -f hello'
```

---

### Ejercicio 3 — Verificar persistencia del named volume

```bash
# Crear un archivo dentro del contenedor Debian
docker compose exec debian-dev bash -c 'echo "persistente" > /home/dev/workspace/test.txt'

# Verificar que alma-dev lo ve (mismo named volume)
docker compose exec alma-dev cat /home/dev/workspace/test.txt
# Output: persistente

# Verificar persistencia entre down/up
docker compose down
docker compose up -d
docker compose exec debian-dev cat /home/dev/workspace/test.txt
# Output: persistente

# Limpiar con -v
docker compose down -v

# Ahora el archivo NO existe
docker compose up -d
docker compose exec debian-dev ls /home/dev/workspace/test.txt
# Error: file not found
```

---

### Ejercicio 4 — Probar debugging con SYS_PTRACE

```bash
# Crear programa de prueba
cat > /tmp/lab/src/debugme.c << 'EOF'
#include <stdio.h>
#include <unistd.h>

int main(void) {
    int counter = 0;
    while (1) {
        printf("Counter: %d\n", counter++);
        fflush(stdout);
        sleep(1);
    }
    return 0;
}
EOF

# Compilar con debug symbols en Debian
docker compose exec debian-dev bash -c 'cd src && gcc -g -o debugme debugme.c'

# Ejecutar en background
docker compose exec -d debian-dev bash -c 'cd src && ./debugme'

# Attach con gdb desde AlmaLinux (misma red, misma capability)
docker compose exec alma-dev bash -c 'gdb -batch -ex "attach $(pgrep debugme)" -ex "print counter" -ex "detach"'

# Alternativa: usar strace
docker compose exec debian-dev strace -c -p $(pgrep debugme) 2>&1
```

---

### Ejercicio 5 — Comunicación entre contenedores

```bash
# Arrancar servidor en AlmaLinux
docker compose exec -d alma-dev bash -c '
nc -l -p 8080 -c "echo Server: AlmaLinux; cat" &
sleep 1
'

# Cliente en Debian
docker compose exec debian-dev bash -c 'echo "Hello from Debian" | nc alma-dev 8080'
# Debería ver: Server: AlmaLinux / Hello from Debian

# Ver procesos en AlmaLinux
docker compose exec alma-dev ps aux | grep nc
```

---

### Ejercicio 6 — Comparar herramientas entre distros

```bash
# Versiones de compilador
docker compose exec debian-dev gcc --version | head -1
docker compose exec alma-dev gcc --version | head -1

# Versiones de glibc
docker compose exec debian-dev ldd --version | head -1
docker compose exec alma-dev ldd --version | head -1

# Versiones de Rust
docker compose exec debian-dev rustc --version
docker compose exec alma-dev rustc --version
```

---

### Ejercicio 7 — Usar Makefile para múltiples targets

```bash
cat > /tmp/lab/src/Makefile << 'EOF'
CC = gcc
CFLAGS = -Wall -Wextra -g

TARGETS = hello math memory

all: $(TARGETS)

hello: hello.c
	$(CC) $(CFLAGS) -o $@ $<

math: math.c
	$(CC) $(CFLAGS) -o $@ $<

memory: memory.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean
EOF

# Compilar todo en Debian
docker compose exec debian-dev bash -c 'cd src && make'

# Ver binarios
docker compose exec debian-dev bash -c 'cd src && ls -la hello math memory'

# Limpiar
docker compose exec debian-dev bash -c 'cd src && make clean'
```

---

### Ejercicio 8 — Build separado por distro

```bash
# Crear Makefile con soporte multi-distro
cat > /tmp/lab/src/Makefile << 'EOF'
CC = gcc
CFLAGS = -Wall -g

# Detectar distro
ifeq ($(shell hostname),debian)
    BUILD_DIR = build/debian
else ifeq ($(shell hostname),alma)
    BUILD_DIR = build/alma
else
    BUILD_DIR = build/host
endif

all: $(BUILD_DIR)/hello

$(BUILD_DIR)/hello: hello.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf build/

.PHONY: all clean
EOF

# Compilar en Debian
docker compose exec debian-dev bash -c 'cd src && make'
docker compose exec debian-dev ls -la build/debian/

# Limpiar y compilar en AlmaLinux
docker compose exec debian-dev bash -c 'cd src && make clean'
docker compose exec alma-dev bash -c 'cd src && make'
docker compose exec alma-dev ls -la build/alma/

# Verificar que son binarios diferentes
docker compose exec debian-dev file build/debian/hello
docker compose exec alma-dev file build/alma/hello
```

---

### Ejercicio 9 — Explorar la red de Compose

```bash
# Ver información de red
docker network inspect lab_default

# IPs de cada contenedor
docker inspect debian-dev | grep -A 2 Networks
docker inspect alma-dev | grep -A 2 Networks

# Rutas desde un contenedor
docker compose exec debian-dev ip route
docker compose exec debian-dev cat /etc/hosts
```

---

### Ejercicio 10 — Cleanup completo

```bash
# Ver estado antes de limpiar
docker compose ps -a
docker volume ls | grep workspace
docker image ls | grep lab

# Limpiar todo
docker compose down -v
docker image prune -f

# Ver estado después
docker compose ps -a
docker volume ls | grep workspace
docker image ls | grep lab
```
