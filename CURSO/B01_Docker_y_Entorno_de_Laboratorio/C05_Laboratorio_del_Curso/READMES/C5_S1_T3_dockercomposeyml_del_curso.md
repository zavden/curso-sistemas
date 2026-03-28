# T03 — docker-compose.yml del curso

## Diseño del entorno

El laboratorio del curso usa Docker Compose para gestionar dos contenedores de
desarrollo (Debian y AlmaLinux) que comparten el mismo código fuente:

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Host (tu máquina)                           │
│                                                                     │
│   lab/                                                              │
│   ├── compose.yml                                                   │
│   ├── dockerfiles/                                                  │
│   │   ├── debian/Dockerfile                                         │
│   │   └── alma/Dockerfile                                           │
│   └── src/                    ← Bind mount hacia ambos containers  │
│       ├── hello.c                                                   │
│       └── Makefile                                                  │
└─────────────────────────────────────────────────────────────────────┘
                                 │
                                 │ docker compose up -d
                                 ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        Red: lab_default                             │
│                                                                     │
│   ┌───────────────────────┐       ┌───────────────────────┐        │
│   │     debian-dev        │       │       alma-dev         │        │
│   │                       │       │                        │        │
│   │  hostname: debian     │       │  hostname: alma        │        │
│   │  Image: lab-debian-dev│       │  Image: lab-alma-dev   │        │
│   │                       │       │                        │        │
│   │  /home/dev/workspace/ │       │  /home/dev/workspace/  │        │
│   │    ├── src/ ──────────┼─ bind ┼── src/ ────────────────┼─./src  │
│   │    └── (datos) ───────┼─ vol ─┼── (datos)              │        │
│   │                       │       │                        │        │
│   │  SYS_PTRACE ✓         │       │  SYS_PTRACE ✓          │        │
│   │  stdin + TTY ✓        │       │  stdin + TTY ✓         │        │
│   └───────────────────────┘       └───────────────────────┘        │
│                                                                     │
│   DNS interno: debian-dev ←→ alma-dev (por nombre de servicio)     │
└─────────────────────────────────────────────────────────────────────┘
```

Hay dos tipos de montaje:
- **Named volume `workspace`**: datos persistentes compartidos entre ambos
  contenedores (historial, configuración, binarios). Sobrevive `down`/`up`.
- **Bind mount `./src`**: código fuente del host, sincronizado en tiempo real.

---

## El Compose file

El archivo `compose.yml` del laboratorio (disponible en `labs/compose.yml`):

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

El `context` define el directorio base para instrucciones `COPY` y `ADD` en el
Dockerfile, y para paths relativos durante el build.

### `container_name` vs nombre generado

| Aspecto | `container_name` | Nombre generado por Compose |
|---|---|---|
| Valor | Fijo: `debian-dev` | `lab-debian-dev-1` |
| Persistencia | Mismo nombre entre `down`/`up` | Puede cambiar |
| Uso en scripts | Predecible: `docker exec debian-dev ...` | Requiere interpolación |

Ventajas de nombres fijos:
- Scripts más simples: `docker exec debian-dev gcc --version`
- Referencias predecibles para automatización

### `hostname`

Establece el nombre interno del contenedor (visible via el comando `hostname` y
en el prompt del shell):

```bash
docker compose exec debian-dev hostname
# debian

docker compose exec alma-dev hostname
# alma
```

Aparece en:
- Prompt de bash (`dev@debian:~/workspace$`)
- Archivo `/etc/hostname` dentro del contenedor
- Salida de `uname -n`

> **Importante:** `hostname` solo afecta al nombre interno del contenedor. No
> crea una entrada DNS en la red de Compose. Para comunicarte entre contenedores,
> usa el **nombre de servicio** (`debian-dev`, `alma-dev`), no el hostname
> (`debian`, `alma`).

### `volumes` — dos tipos de montaje

```yaml
volumes:
  - workspace:/home/dev/workspace         # Named volume: persistencia
  - ./src:/home/dev/workspace/src         # Bind mount: código del host
```

#### Named volume `workspace`

| Aspecto | Descripción |
|---|---|
| Ubicación en host | `/var/lib/docker/volumes/lab_workspace/_data` |
| Persistencia | Sobrevive `down`, `up`, `restart` |
| Contenido | Configuración, historial, binarios compilados |
| Compartido | Mismo volume montado en ambos contenedores |
| Eliminación | Solo con `docker compose down -v` o `docker volume rm` |

```bash
# Inspeccionar el volume
docker volume inspect lab_workspace
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
| Sincronización | Inmediata, bidireccional |
| Uso | Código fuente que editas en tu editor del host |

```
Host: ./src/hello.c
        │
        │ bind mount (misma referencia de filesystem)
        ▼
debian-dev: /home/dev/workspace/src/hello.c
alma-dev:   /home/dev/workspace/src/hello.c
```

**Orden de montaje:** El bind mount `./src` se monta *sobre* el named volume en
la ruta `/home/dev/workspace/src`. Esto significa que `src/` viene del host
mientras que el resto de `/home/dev/workspace/` viene del named volume.

**Precaución sobre binarios:** Al compartir `./src` entre ambos contenedores,
si compilas en Debian y luego ejecutas ese mismo binario en AlmaLinux, puede
fallar por incompatibilidad de glibc. Más detalles en la sección de binarios.

### `cap_add: SYS_PTRACE`

Permite que herramientas de debugging que usan la syscall `ptrace` funcionen
dentro del contenedor:

| Herramienta | Uso de ptrace | ¿Necesita SYS_PTRACE? |
|---|---|---|
| `gdb ./prog` | No (lanza el proceso hijo) | No |
| `gdb -p PID` | `ptrace(PTRACE_ATTACH)` | **Sí** |
| `strace -p PID` | `ptrace(PTRACE_SYSCALL)` | **Sí** |
| `ltrace ./prog` | `ptrace(PTRACE_TRACEME)` | **Sí** |
| `valgrind ./prog` | DBI (Dynamic Binary Instrumentation) | **No** |

Valgrind **no** usa ptrace — ejecuta el programa sobre una CPU virtual que
instrumenta cada instrucción. Funciona sin SYS_PTRACE. La capability es
necesaria para herramientas que se *adjuntan* a procesos existentes o que
interceptan syscalls via ptrace.

```yaml
cap_add:
  - SYS_PTRACE
```

**Nota de seguridad:** SYS_PTRACE permite a un proceso inspeccionar y modificar
la memoria de otros procesos en el mismo contenedor. Solo usar en contenedores
de desarrollo, nunca en producción.

### `stdin_open` y `tty`

| Opción Compose | Flag Docker equiv. | Efecto |
|---|---|---|
| `stdin_open: true` | `-i` | Mantiene stdin abierto aunque nadie lo lea |
| `tty: true` | `-t` | Asigna una pseudo-TTY |

Juntos permiten shells interactivos con prompt, colores y edición de línea:

```bash
# Con stdin_open + tty: shell interactivo completo
docker compose exec debian-dev bash
dev@debian:~/workspace$          # ← prompt funcional

# Sin ellos: el contenedor no mantiene un shell esperando
```

Para `docker compose exec`, estos flags afectan al contenedor base — el exec
siempre puede ser interactivo si el contenedor está corriendo.

---

## Construir y usar el lab

```bash
cd lab

# Construir ambas imágenes
docker compose build

# Construir solo una
docker compose build debian-dev

# Reconstruir sin cache
docker compose build --no-cache

# Arrancar ambos contenedores (modo detached)
docker compose up -d

# Ver estado
docker compose ps
# NAME         IMAGE            STATUS
# debian-dev   lab-debian-dev   Up (2 minutes ago)
# alma-dev     lab-alma-dev     Up (2 minutes ago)

# Entrar al contenedor Debian (shell interactivo)
docker compose exec debian-dev bash

# Ejecutar un comando directo
docker compose exec debian-dev gcc --version

# Ver logs
docker compose logs
docker compose logs -f debian-dev    # seguir logs de un servicio

# Detener (preserva named volume y datos)
docker compose down

# Detener y eliminar TODO (contenedores, redes, volumes)
docker compose down -v
```

---

## Flujo de trabajo diario

```bash
# 1. Levantar el lab
cd lab
docker compose up -d

# 2. Editar código en tu editor del host (VS Code, vim, etc.)
vim src/hello.c

# 3. Compilar y ejecutar en Debian
docker compose exec debian-dev bash -c 'cd src && gcc -Wall -o hello hello.c && ./hello'

# 4. Compilar y ejecutar en AlmaLinux (mismo código, otra distro)
docker compose exec alma-dev bash -c 'cd src && gcc -Wall -o hello hello.c && ./hello'

# 5. Debug en Debian
docker compose exec debian-dev bash -c 'cd src && gcc -g -o hello hello.c && gdb ./hello'

# 6. Al terminar
docker compose down
```

### Uso de make (recomendado)

Para evitar comandos largos, usa un Makefile en `./src/`:

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -g

all: hello

hello: hello.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f hello

.PHONY: all clean
```

```bash
# Compilar en Debian
docker compose exec debian-dev bash -c 'cd src && make && ./hello'

# Compilar en AlmaLinux
docker compose exec alma-dev bash -c 'cd src && make clean && make && ./hello'
```

---

## Binarios compilados: separar por distro

Los binarios compilados en Debian **no son necesariamente compatibles** con
AlmaLinux y viceversa:

| Factor | Debian (bookworm) | AlmaLinux 9 |
|---|---|---|
| glibc | 2.36 | 2.34 |
| GCC | 12.x | 11.x |
| libstdc++ | GCC 12 | GCC 11 |

> **Nota:** Ambos contenedores comparten el **mismo kernel** — el del host.
> Aunque Debian bookworm usa kernel 6.1 y AlmaLinux 9 usa kernel 5.14 de forma
> nativa, los contenedores no ejecutan su propio kernel. `uname -r` dentro de
> cualquier contenedor mostrará la misma versión: la de tu máquina host.

La incompatibilidad viene de las **bibliotecas de espacio de usuario** (glibc,
libstdc++), no del kernel. Si compilas en Debian con glibc 2.36 y el binario
usa símbolos de esa versión, AlmaLinux con glibc 2.34 no los encontrará:

```bash
# Compilar en Debian
docker compose exec debian-dev bash -c 'cd src && gcc -o hello hello.c'

# Intentar ejecutar en AlmaLinux
docker compose exec alma-dev bash -c 'cd src && ./hello'
# Posible error: version 'GLIBC_2.36' not found
```

### Solución: directorios de build separados

```
src/
├── hello.c
├── Makefile
├── build-debian/     ← compilar aquí desde debian-dev
│   └── hello
└── build-alma/       ← compilar aquí desde alma-dev
    └── hello
```

```bash
# Compilar en Debian
docker compose exec debian-dev bash -c 'cd src && mkdir -p build-debian && gcc -o build-debian/hello hello.c'

# Compilar en AlmaLinux
docker compose exec alma-dev bash -c 'cd src && mkdir -p build-alma && gcc -o build-alma/hello hello.c'

# Cada binario se ejecuta en su distro
docker compose exec debian-dev bash -c 'cd src && ./build-debian/hello'
docker compose exec alma-dev bash -c 'cd src && ./build-alma/hello'
```

### Makefile multi-distro

```makefile
CC = gcc
CFLAGS = -Wall -g

ifeq ($(shell hostname),debian)
    BUILD_DIR = build-debian
else ifeq ($(shell hostname),alma)
    BUILD_DIR = build-alma
else
    BUILD_DIR = build-host
endif

all: $(BUILD_DIR)/hello

$(BUILD_DIR)/hello: hello.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf build-debian/ build-alma/ build-host/

.PHONY: all clean
```

---

## Comunicación entre contenedores

Ambos contenedores están en la misma red de Compose (`lab_default`) y pueden
comunicarse **por nombre de servicio**:

```bash
# Ping por nombre de servicio (resolución DNS de Compose)
docker compose exec debian-dev ping -c 1 alma-dev
# PING alma-dev (172.20.0.2) 56(84) bytes of data.

docker compose exec alma-dev ping -c 1 debian-dev
# PING debian-dev (172.20.0.3) 56(84) bytes of data.
```

> **Nota:** `ping alma` (el hostname) **no funciona** desde otro contenedor.
> Docker Compose registra en DNS los **nombres de servicio** (`alma-dev`,
> `debian-dev`), no los hostnames (`alma`, `debian`). El `hostname` solo afecta
> al nombre interno del propio contenedor.

### Ver IPs en la red

```bash
docker network inspect lab_default \
  -f '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{println}}{{end}}'
# alma-dev: 172.20.0.2/16
# debian-dev: 172.20.0.3/16
```

### Ejemplo: ncat como servidor/cliente

Los dos contenedores usan herramientas de red distintas:
- **Debian**: `netcat-openbsd` → comando `nc`
- **AlmaLinux**: `nmap-ncat` → comando `ncat`

```bash
# Terminal 1: AlmaLinux como servidor echo (ncat de nmap)
docker compose exec alma-dev bash -c 'ncat -l 9090 -e /bin/cat --keep-open &'

# Terminal 2: Debian como cliente (nc de netcat-openbsd)
docker compose exec debian-dev bash -c 'echo "Hello from Debian" | nc alma-dev 9090'
# Hello from Debian
```

Diferencias clave de sintaxis:

| Acción | Debian (`nc`, netcat-openbsd) | AlmaLinux (`ncat`, nmap) |
|---|---|---|
| Escuchar | `nc -l 9090` | `ncat -l 9090` |
| Escuchar con -p | `nc -l -p 9090` (redundante) | `ncat -l 9090` |
| Ejecutar comando | No soportado nativamente | `ncat -l 9090 -e /bin/cat` |
| Keep-open | No soportado | `ncat -l 9090 --keep-open` |

---

## Ejercicios

### Ejercicio 1 — Levantar el lab y verificar servicios

Construye el laboratorio completo y verifica que ambos contenedores están
operativos:

```bash
cd labs   # o donde tengas el compose.yml

# Construir ambas imágenes
docker compose build

# Arrancar ambos contenedores
docker compose up -d

# Verificar estado
docker compose ps

# Probar que ambos responden
docker compose exec debian-dev echo "Debian OK"
docker compose exec alma-dev echo "AlmaLinux OK"

# Ver versiones de compilador en cada distro
docker compose exec debian-dev gcc --version | head -1
docker compose exec alma-dev gcc --version | head -1
```

<details><summary>Predicción</summary>

`docker compose ps` muestra ambos contenedores con status "Up":
```
NAME         IMAGE            STATUS
debian-dev   lab-debian-dev   Up
alma-dev     lab-alma-dev     Up
```

Ambos `echo` devuelven sus mensajes. Las versiones de gcc difieren:
- Debian bookworm: `gcc (Debian 12.x.x) 12.x.x`
- AlmaLinux 9: `gcc (GCC) 11.x.x`

</details>

---

### Ejercicio 2 — Compilar el mismo código en ambas distros

Usa el programa `hello.c` del lab (disponible en `labs/src/hello.c`) para
observar diferencias entre distros:

```bash
# Compilar y ejecutar en Debian
docker compose exec debian-dev bash -c 'cd src && gcc -Wall -o hello hello.c && ./hello'

# Compilar y ejecutar en AlmaLinux
docker compose exec alma-dev bash -c 'cd src && gcc -Wall -o hello hello.c && ./hello'
```

<details><summary>Predicción</summary>

Ambas compilaciones tienen éxito y muestran `Hello from the lab workspace!`.

Pero hay un problema sutil: como `./src` es un bind mount compartido, la
segunda compilación (alma-dev) **sobrescribe** el binario `hello` que creó
debian-dev. Ambos contenedores escriben en la misma ubicación del host.

Esto ilustra por qué necesitas directorios de build separados cuando quieres
mantener binarios de ambas distros simultáneamente.

</details>

---

### Ejercicio 3 — Verificar persistencia del named volume

Crea un archivo en el named volume, verifica que ambos contenedores lo ven, y
que sobrevive `docker compose down`:

```bash
# Crear archivo en workspace (named volume) desde debian-dev
docker compose exec debian-dev bash -c 'echo "datos persistentes" > /home/dev/workspace/test.txt'

# Verificar que alma-dev lo ve (mismo named volume)
docker compose exec alma-dev cat /home/dev/workspace/test.txt

# Detener y destruir contenedores (sin -v: preserva volumes)
docker compose down

# Volver a levantar
docker compose up -d

# ¿Sigue el archivo?
docker compose exec debian-dev cat /home/dev/workspace/test.txt

# Ahora destruir TODO incluyendo volumes (-v)
docker compose down -v

# Volver a levantar
docker compose up -d

# ¿Sigue el archivo?
docker compose exec debian-dev cat /home/dev/workspace/test.txt
```

<details><summary>Predicción</summary>

1. **alma-dev ve el archivo**: `datos persistentes` — ambos contenedores
   comparten el mismo named volume `workspace`.

2. **Tras `down` + `up`**: el archivo sigue ahí. `docker compose down` sin
   `-v` elimina contenedores y redes, pero **preserva los named volumes**.

3. **Tras `down -v` + `up`**: el archivo **no existe**. `down -v` elimina
   los named volumes, destruyendo todos los datos persistentes.

```
cat: /home/dev/workspace/test.txt: No such file or directory
```

</details>

---

### Ejercicio 4 — Debug con gdb y strace (SYS_PTRACE)

Crea un programa que se ejecuta continuamente, luego adjúntate con gdb y
strace para observar su estado:

```bash
# Crear programa de prueba
cat > labs/src/debugme.c << 'EOF'
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

# Ejecutar en background dentro de debian-dev
docker compose exec -d debian-dev bash -c 'cd src && ./debugme > /tmp/debugme.log 2>&1'

# Adjuntarse con gdb DESDE EL MISMO contenedor
docker compose exec debian-dev bash -c \
  'PID=$(pgrep debugme) && gdb -batch -ex "attach $PID" -ex "print counter" -ex "detach"'

# Observar syscalls con strace DESDE EL MISMO contenedor
docker compose exec debian-dev bash -c \
  'PID=$(pgrep debugme) && timeout 3 strace -c -p $PID 2>&1'

# Limpiar: matar el proceso
docker compose exec debian-dev bash -c 'pkill debugme'
```

<details><summary>Predicción</summary>

**gdb attach** muestra el valor actual de `counter` — un número que va subiendo
cada segundo:
```
Attaching to process 42
...
$1 = 15
```

**strace -c** muestra un resumen de syscalls, dominado por `nanosleep` (del
`sleep(1)`) y `write` (del `printf`):
```
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
 80.00    0.003000        3000         1           nanosleep
 20.00    0.001000         500         2           write
...
```

**Importante:** El debugging debe hacerse **dentro del mismo contenedor** donde
corre el proceso. `ptrace` requiere que ambos procesos (debugger y target)
estén en el **mismo PID namespace**. No puedes adjuntarte desde alma-dev a un
proceso que corre en debian-dev — cada contenedor tiene su propio PID namespace
y `pgrep debugme` en alma-dev no encontraría el proceso.

</details>

---

### Ejercicio 5 — Valgrind sin SYS_PTRACE

Demuestra que valgrind no necesita SYS_PTRACE creando un contenedor sin esa
capability:

```bash
# Lanzar un contenedor temporal SIN SYS_PTRACE usando la imagen ya construida
docker run --rm -it -v "$(pwd)/labs/src:/home/dev/workspace/src" \
  lab-debian-dev bash -c '
    cd src
    gcc -g -o debugme debugme.c
    echo "=== valgrind sin SYS_PTRACE ==="
    valgrind --leak-check=full ./debugme &
    VPID=$!
    sleep 2
    kill $VPID
    echo "=== valgrind funcionó correctamente ==="
  '
```

<details><summary>Predicción</summary>

Valgrind ejecuta el programa sin problemas, mostrando su cabecera habitual:
```
==1== Memcheck, a memory error detector
==1== Copyright (C) ...
==1== Command: ./debugme
Counter: 0
Counter: 1
```

Esto funciona porque valgrind usa **Dynamic Binary Instrumentation (DBI)** — no
se adjunta a un proceso existente con ptrace, sino que **lanza** el programa
sobre una CPU virtual que instrumenta cada instrucción de máquina. Es similar
a cómo funciona un intérprete, no un debugger.

En contraste, `gdb -p PID` y `strace -p PID` sí fallarían en este contenedor
sin SYS_PTRACE con "Operation not permitted".

</details>

---

### Ejercicio 6 — Comunicación entre contenedores

Verifica la conectividad de red entre los dos contenedores:

```bash
# Ping por nombre de servicio
docker compose exec debian-dev ping -c 2 alma-dev
docker compose exec alma-dev ping -c 2 debian-dev

# Intentar ping por hostname (¿funciona?)
docker compose exec debian-dev ping -c 1 alma

# Ver la red de Compose
docker network inspect lab_default \
  -f '{{range .Containers}}{{.Name}}: {{.IPv4Address}}{{println}}{{end}}'

# Ver rutas y DNS desde debian-dev
docker compose exec debian-dev ip route
docker compose exec debian-dev cat /etc/hosts
docker compose exec debian-dev cat /etc/resolv.conf
```

<details><summary>Predicción</summary>

**Ping por nombre de servicio** funciona perfectamente — Docker Compose
registra cada servicio en su DNS integrado:
```
PING alma-dev (172.20.0.2) 56(84) bytes of data.
64 bytes from alma-dev.lab_default (172.20.0.2): ...
```

**Ping por hostname `alma`** falla:
```
ping: alma: Name or service not known
```

Docker Compose solo registra **nombres de servicio** en DNS (los keys bajo
`services:` en compose.yml), no los hostnames configurados con `hostname:`.
El hostname solo afecta al nombre que el contenedor se da a sí mismo
(`/etc/hostname`), no a cómo lo ven otros contenedores.

**`/etc/resolv.conf`** muestra el DNS interno de Docker (`127.0.0.11`).

</details>

---

### Ejercicio 7 — Servidor/cliente con ncat entre contenedores

Usa las herramientas de red instaladas para comunicar los contenedores:

```bash
# Terminal 1: AlmaLinux como servidor echo (ncat de nmap-ncat)
docker compose exec alma-dev bash -c 'ncat -l 9090 -e /bin/cat --keep-open &'

# Esperar un momento a que el puerto esté escuchando
sleep 1

# Terminal 2: Debian como cliente (nc de netcat-openbsd)
docker compose exec debian-dev bash -c 'echo "Hello from Debian" | nc alma-dev 9090'

# Verificar que alma-dev tiene el puerto abierto
docker compose exec alma-dev bash -c 'ss -tlnp | grep 9090'

# Limpiar
docker compose exec alma-dev bash -c 'pkill ncat'
```

<details><summary>Predicción</summary>

El servidor ncat en AlmaLinux escucha en el puerto 9090. Cuando Debian envía
"Hello from Debian", ncat ejecuta `/bin/cat` que devuelve el mismo texto
(servidor echo):

```
Hello from Debian
```

`ss -tlnp` muestra el puerto:
```
LISTEN   0   10   0.0.0.0:9090   0.0.0.0:*   users:(("ncat",pid=XX,fd=3))
```

**Nota de sintaxis:** Debian usa `nc` (de `netcat-openbsd`) y AlmaLinux usa
`ncat` (de `nmap-ncat`). Son herramientas similares pero con flags diferentes.
El `nc` de OpenBSD no soporta `-e` para ejecutar comandos, por eso el servidor
usa `ncat`.

</details>

---

### Ejercicio 8 — Build separado por distro con Makefile

Crea un Makefile que detecte la distro y compile en directorios separados:

```bash
cat > labs/src/Makefile << 'EOF'
CC = gcc
CFLAGS = -Wall -Wextra -g

ifeq ($(shell hostname),debian)
    BUILD_DIR = build-debian
else ifeq ($(shell hostname),alma)
    BUILD_DIR = build-alma
else
    BUILD_DIR = build-host
endif

all: $(BUILD_DIR)/hello

$(BUILD_DIR)/hello: hello.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf build-debian/ build-alma/ build-host/

.PHONY: all clean
EOF

# Compilar en Debian
docker compose exec debian-dev bash -c 'cd src && make'

# Compilar en AlmaLinux
docker compose exec alma-dev bash -c 'cd src && make'

# Verificar: dos binarios distintos
docker compose exec debian-dev file src/build-debian/hello
docker compose exec alma-dev file src/build-alma/hello

# Comparar versiones de glibc requeridas
docker compose exec debian-dev bash -c 'cd src && ldd build-debian/hello'
docker compose exec alma-dev bash -c 'cd src && ldd build-alma/hello'

# Limpiar
docker compose exec debian-dev bash -c 'cd src && make clean'
```

<details><summary>Predicción</summary>

`make` en debian-dev crea `build-debian/hello`, en alma-dev crea
`build-alma/hello`. Ambos binarios coexisten porque están en directorios
separados.

`file` muestra que ambos son ELF 64-bit pero enlazados con diferentes
versiones:
```
build-debian/hello: ELF 64-bit LSB executable, x86-64, dynamically linked,
                    interpreter /lib64/ld-linux-x86-64.so.2, ...
```

`ldd` revela las diferencias en glibc:
```
# Debian:  libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
# AlmaLinux: libc.so.6 => /lib64/libc.so.6
```

Las rutas de las bibliotecas difieren (`/lib/x86_64-linux-gnu/` vs `/lib64/`),
reflejando las convenciones de cada distro. El truco del Makefile con
`$(shell hostname)` funciona porque `hostname` devuelve `debian` o `alma`
(configurado en compose.yml).

</details>

---

### Ejercicio 9 — Explorar la red de Compose

Investiga la red que Docker Compose crea para el lab:

```bash
# Listar redes
docker network ls | grep lab

# Inspeccionar la red completa
docker network inspect lab_default

# Desde debian-dev: ver configuración de red
docker compose exec debian-dev ip addr show eth0
docker compose exec debian-dev ip route

# Verificar resolución DNS del servicio
docker compose exec debian-dev bash -c 'getent hosts alma-dev'
docker compose exec debian-dev bash -c 'getent hosts debian-dev'

# ¿Resuelve el hostname?
docker compose exec debian-dev bash -c 'getent hosts alma || echo "No resuelve hostname"'
```

<details><summary>Predicción</summary>

`docker network ls` muestra la red creada por Compose:
```
NETWORK ID     NAME          DRIVER    SCOPE
abc123...      lab_default   bridge    local
```

`ip addr show eth0` muestra la IP del contenedor en la red bridge (ej.
`172.20.0.3/16`).

`ip route` muestra la ruta por defecto via el gateway de Docker:
```
default via 172.20.0.1 dev eth0
172.20.0.0/16 dev eth0 proto kernel scope link src 172.20.0.3
```

`getent hosts alma-dev` resuelve correctamente:
```
172.20.0.2    alma-dev
```

`getent hosts alma` **falla** — confirma que los hostnames no se registran
en el DNS de Compose.

</details>

---

### Ejercicio 10 — Cleanup completo

Practica la limpieza ordenada del laboratorio:

```bash
# Ver estado actual
docker compose ps -a
docker volume ls | grep workspace
docker image ls | grep lab
docker network ls | grep lab

# Paso 1: detener contenedores (preserva volumes)
docker compose down
docker volume ls | grep workspace    # ← sigue existiendo

# Paso 2: eliminar todo incluyendo volumes
docker compose up -d && docker compose down -v
docker volume ls | grep workspace    # ← ya no existe

# Paso 3: eliminar las imágenes construidas
docker image ls | grep lab
docker image rm lab-debian-dev lab-alma-dev

# Verificar que todo está limpio
docker compose ps -a
docker volume ls | grep workspace
docker image ls | grep lab
```

<details><summary>Predicción</summary>

Después de `docker compose down` (sin `-v`):
- Contenedores: eliminados
- Red `lab_default`: eliminada
- Volume `lab_workspace`: **sigue existiendo** (datos preservados)
- Imágenes: siguen existiendo

Después de `docker compose down -v`:
- Volume `lab_workspace`: **eliminado**
- Todos los datos del workspace se pierden

Después de `docker image rm`:
- Imágenes `lab-debian-dev` y `lab-alma-dev`: eliminadas
- Para volver a usar el lab necesitas `docker compose build` de nuevo

La secuencia de limpieza de menor a mayor destrucción:
1. `docker compose stop` — para contenedores, todo se preserva
2. `docker compose down` — elimina contenedores y red, preserva volumes e imágenes
3. `docker compose down -v` — elimina también volumes (pérdida de datos)
4. `docker image rm` — elimina imágenes (requiere rebuild)

</details>
