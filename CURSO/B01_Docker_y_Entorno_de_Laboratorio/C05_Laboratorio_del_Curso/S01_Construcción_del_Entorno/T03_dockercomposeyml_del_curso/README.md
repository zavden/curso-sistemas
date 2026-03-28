# T03 — docker-compose.yml del curso

## Diseño del entorno

El laboratorio del curso usa Docker Compose para gestionar dos contenedores de
desarrollo (Debian y AlmaLinux) que comparten el mismo código fuente:

```
Host
├── src/                    ← Código fuente (bind mount)
│   ├── hello.c
│   ├── Makefile
│   └── ...
├── dockerfiles/
│   ├── debian/Dockerfile
│   └── alma/Dockerfile
└── compose.yml

     ┌───────────────────────────────────────────────────┐
     │              Red: lab_default                      │
     │                                                   │
     │  ┌──────────────┐       ┌──────────────┐          │
     │  │  debian-dev  │       │  alma-dev    │          │
     │  │              │       │              │          │
     │  │ /home/dev/   │       │ /home/dev/   │          │
     │  │  workspace/  │       │  workspace/  │          │
     │  │   src/ ──────┼───────┼── src/ ──────┼── ./src  │
     │  │              │       │              │  (host)  │
     │  └──────────────┘       └──────────────┘          │
     │                                                   │
     └───────────────────────────────────────────────────┘
```

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

## Explicación de cada opción

### `build: context`

Apunta al directorio que contiene el Dockerfile de cada imagen:

```
dockerfiles/
├── debian/
│   └── Dockerfile    ← debian-dev build context
└── alma/
    └── Dockerfile    ← alma-dev build context
```

### `container_name` y `hostname`

- `container_name`: nombre fijo del contenedor (en lugar del generado por Compose)
- `hostname`: nombre que el contenedor ve internamente (`hostname` command)

```bash
docker compose exec debian-dev hostname
# debian

docker compose exec alma-dev hostname
# alma
```

### `volumes`

Dos tipos de montaje por contenedor:

```yaml
volumes:
  - workspace:/home/dev/workspace     # Named volume: datos persistentes
  - ./src:/home/dev/workspace/src     # Bind mount: código fuente del host
```

**Named volume `workspace`**: almacena datos del workspace que persisten entre
`down` y `up`. Incluye archivos generados, configuración local, historial de
shell, etc. Compartido entre ambos contenedores.

**Bind mount `./src`**: monta el directorio `src/` del host directamente en
ambos contenedores. Los cambios en el host se reflejan inmediatamente en los
contenedores y viceversa.

```
Editar ./src/main.c en tu editor del host
          │
          ▼
debian-dev ve /home/dev/workspace/src/main.c actualizado
alma-dev ve /home/dev/workspace/src/main.c actualizado
```

### `cap_add: SYS_PTRACE`

Permite a gdb, strace, ltrace y valgrind funcionar dentro del contenedor:

```yaml
cap_add:
  - SYS_PTRACE
```

Sin esta capability, las herramientas de debugging fallan con "Operation not
permitted".

### `stdin_open` y `tty`

```yaml
stdin_open: true    # Equivale a -i (mantiene stdin abierto)
tty: true           # Equivale a -t (asigna una pseudo-TTY)
```

Permiten usar `docker compose exec` de forma interactiva:

```bash
docker compose exec debian-dev bash
# Abre un shell interactivo con prompt y colores
```

Sin estos flags, el contenedor no mantiene stdin abierto y los shells
interactivos no funcionan correctamente.

## Estructura de directorios del lab

```bash
# Crear la estructura
mkdir -p lab/{dockerfiles/debian,dockerfiles/alma,src}
```

```
lab/
├── compose.yml
├── dockerfiles/
│   ├── debian/
│   │   └── Dockerfile
│   └── alma/
│       └── Dockerfile
└── src/
    └── (código fuente del curso)
```

## Construir y usar el lab

```bash
cd lab

# Construir ambas imágenes
docker compose build

# Arrancar ambos contenedores
docker compose up -d

# Verificar estado
docker compose ps
# NAME         IMAGE          STATUS
# debian-dev   lab-debian-dev Up
# alma-dev     lab-alma-dev   Up

# Entrar al contenedor Debian
docker compose exec debian-dev bash

# Entrar al contenedor AlmaLinux
docker compose exec alma-dev bash

# Ver logs
docker compose logs

# Detener (preserva volúmenes)
docker compose down

# Detener y eliminar todo (incluyendo datos)
docker compose down -v
```

## Flujo de trabajo diario

```bash
# 1. Levantar el lab
docker compose up -d

# 2. Editar código en tu editor del host (VS Code, vim, etc.)
vim src/main.c

# 3. Compilar en Debian
docker compose exec debian-dev bash -c 'cd src && gcc -o main main.c && ./main'

# 4. Compilar en AlmaLinux (mismo código, otra distro)
docker compose exec alma-dev bash -c 'cd src && gcc -o main main.c && ./main'

# 5. Debug
docker compose exec debian-dev bash -c 'cd src && gcc -g -o main main.c && gdb ./main'

# 6. Al terminar
docker compose down
```

## Binarios compilados: separar por distro

Los binarios compilados en Debian **no son necesariamente compatibles** con
AlmaLinux y viceversa (diferentes versiones de glibc, diferentes librerías
dinámicas). Para evitar confusiones:

```bash
# Opción 1: directorios de build separados
src/
├── main.c
├── build-debian/    ← compilar aquí desde debian-dev
│   └── main
└── build-alma/      ← compilar aquí desde alma-dev
    └── main

# Opción 2: limpiar antes de compilar en la otra distro
docker compose exec debian-dev bash -c 'cd src && make clean && make'
docker compose exec alma-dev bash -c 'cd src && make clean && make'
```

## Comunicación entre contenedores

Ambos contenedores están en la misma red de Compose y pueden comunicarse:

```bash
# Desde debian-dev, conectar a alma-dev
docker compose exec debian-dev ping -c 1 alma-dev
# PING alma-dev (172.20.0.3)

# Útil para ejercicios de red: un contenedor como servidor, otro como cliente
docker compose exec alma-dev bash -c 'ncat -l 9090 -e /bin/echo &'
docker compose exec debian-dev bash -c 'echo "hello" | ncat alma-dev 9090'
```

---

## Ejercicios

### Ejercicio 1 — Construir el lab completo

```bash
mkdir -p /tmp/lab/{dockerfiles/debian,dockerfiles/alma,src}

# (Crear los Dockerfiles de T01 y T02 en sus directorios)
# (Crear el compose.yml con el contenido de arriba)

cd /tmp/lab
docker compose build
docker compose up -d
docker compose ps
```

### Ejercicio 2 — Código compartido entre distros

```bash
# Crear un programa de prueba en el host
cat > /tmp/lab/src/hello.c << 'EOF'
#include <stdio.h>
int main(void) {
    printf("Hello World!\n");
    return 0;
}
EOF

# Compilar y ejecutar en ambas distros
docker compose exec debian-dev bash -c 'cd src && gcc -o hello hello.c && ./hello'
docker compose exec alma-dev bash -c 'cd src && gcc -o hello hello.c && ./hello'

# Verificar que los binarios son diferentes (distinta glibc)
docker compose exec debian-dev file src/hello
docker compose exec alma-dev file src/hello
```

### Ejercicio 3 — Verificar persistencia

```bash
# Crear un archivo dentro del contenedor
docker compose exec debian-dev bash -c 'echo "persistente" > /home/dev/workspace/test.txt'

# Verificar que alma-dev lo ve (mismo named volume)
docker compose exec alma-dev cat /home/dev/workspace/test.txt
# persistente

# Reiniciar: los datos persisten
docker compose down
docker compose up -d
docker compose exec debian-dev cat /home/dev/workspace/test.txt
# persistente

# Limpiar
docker compose down -v
```
