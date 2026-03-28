# Lab — docker-compose.yml del curso

## Objetivo

Construir el laboratorio completo del curso usando Docker Compose:
entender cada opcion del compose.yml, verificar volumenes compartidos,
bind mounts, SYS_PTRACE, y practicar el flujo de trabajo diario.

## Prerequisitos

- Docker y Docker Compose instalados
- Conexion a internet
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `compose.yml` | Compose file del laboratorio |
| `dockerfiles/debian/Dockerfile` | Imagen Debian dev |
| `dockerfiles/alma/Dockerfile` | Imagen AlmaLinux dev |
| `src/hello.c` | Programa C de prueba |

---

## Parte 1 — Examinar el compose.yml

### Objetivo

Entender cada seccion del compose.yml antes de construir el entorno.

### Paso 1.1: Ver el compose.yml

```bash
cat compose.yml
```

### Paso 1.2: Servicios

El archivo define dos servicios: `debian-dev` y `alma-dev`. Cada uno
construye su imagen desde un Dockerfile diferente.

```bash
docker compose config --services
```

### Paso 1.3: Build contexts

```bash
ls dockerfiles/debian/
ls dockerfiles/alma/
```

Cada servicio tiene su propio build context con su Dockerfile. Compose
construye la imagen desde ese directorio.

### Paso 1.4: Volumenes

El compose.yml declara dos tipos de montaje por contenedor:

```
workspace:/home/dev/workspace    → Named volume (datos persistentes)
./src:/home/dev/workspace/src    → Bind mount (codigo del host)
```

El named volume `workspace` esta declarado en la seccion `volumes:`
raiz y se comparte entre ambos contenedores. El bind mount `./src`
conecta el directorio del host directamente al contenedor.

### Paso 1.5: Opciones de runtime

```bash
grep -A 1 "cap_add" compose.yml
grep "stdin_open\|tty" compose.yml
```

- `cap_add: SYS_PTRACE` — permite gdb, strace, valgrind
- `stdin_open: true` — equivale a `-i` (mantiene stdin abierto)
- `tty: true` — equivale a `-t` (asigna pseudo-TTY)

Sin `stdin_open` y `tty`, los shells interactivos via `exec` no
funcionan correctamente.

---

## Parte 2 — Construir y levantar

### Objetivo

Construir las imagenes y levantar el entorno de desarrollo.

### Paso 2.1: Construir

```bash
docker compose build
```

El primer build tarda varios minutos (descarga paquetes, instala Rust).
Los builds posteriores usan cache y son mucho mas rapidos.

### Paso 2.2: Levantar

```bash
docker compose up -d
```

### Paso 2.3: Verificar estado

```bash
docker compose ps
```

Salida esperada:

```
NAME         IMAGE               STATUS    PORTS
debian-dev   labs-debian-dev     Up
alma-dev     labs-alma-dev       Up
```

Ambos contenedores deben estar en estado "Up".

### Paso 2.4: Ver las imagenes creadas

```bash
docker image ls | grep -E "labs.*(debian|alma)"
```

Compose nombra las imagenes con el prefijo del proyecto (nombre del
directorio) y el nombre del servicio.

### Paso 2.5: Ver logs

```bash
docker compose logs
```

Como ambos servicios usan `CMD ["/bin/bash"]` con `stdin_open` y
`tty`, los logs estan vacios — los contenedores esperan input
interactivo.

---

## Parte 3 — Volumenes y bind mounts

### Objetivo

Verificar que los volumenes compartidos y bind mounts funcionan
correctamente entre ambos contenedores y el host.

### Paso 3.1: Bind mount — archivo del host visible en contenedores

```bash
cat src/hello.c
```

```bash
docker compose exec debian-dev cat src/hello.c
docker compose exec alma-dev cat src/hello.c
```

El mismo archivo es visible desde el host y desde ambos contenedores.

### Paso 3.2: Bind mount — cambios del host reflejados

```bash
echo '// Modified from host' >> src/hello.c
docker compose exec debian-dev tail -1 src/hello.c
```

Los cambios en el host se reflejan inmediatamente en el contenedor.

```bash
sed -i '$ d' src/hello.c
```

### Paso 3.3: Named volume — datos compartidos

```bash
docker compose exec debian-dev bash -c 'echo "shared data" > /home/dev/workspace/test.txt'
docker compose exec alma-dev cat /home/dev/workspace/test.txt
```

El named volume `workspace` es compartido. Lo que escribe un contenedor
es visible para el otro.

### Paso 3.4: Named volume — persistencia

```bash
docker compose down
docker compose up -d
docker compose exec debian-dev cat /home/dev/workspace/test.txt
```

Antes de ejecutar el ultimo comando, predice: estara el archivo?

`docker compose down` elimina los contenedores pero **preserva los
volumenes**. El archivo sigue ahi al recrear los contenedores.

### Paso 3.5: down -v elimina volumenes

```bash
docker compose exec debian-dev bash -c 'echo "will be lost" > /home/dev/workspace/temp.txt'
docker compose down -v
docker compose up -d
docker compose exec debian-dev cat /home/dev/workspace/temp.txt 2>&1
```

`down -v` elimina los volumenes. El archivo ya no existe. Pero el bind
mount `src/` sigue intacto porque esta en el host:

```bash
docker compose exec debian-dev cat src/hello.c
```

El codigo fuente en `src/` nunca se pierde con `down -v` porque es un
bind mount, no un named volume.

---

## Parte 4 — Flujo de trabajo diario

### Objetivo

Practicar el flujo de trabajo que se usara durante todo el curso:
editar en el host, compilar en los contenedores, comunicarse entre
ellos.

### Paso 4.1: Compilar en Debian

```bash
docker compose exec debian-dev bash -c 'cd src && gcc -o hello hello.c && ./hello'
```

### Paso 4.2: Compilar en AlmaLinux

```bash
docker compose exec alma-dev bash -c 'cd src && gcc -o hello hello.c && ./hello'
```

El mismo codigo compila y ejecuta en ambas distribuciones.

### Paso 4.3: Shell interactivo

```bash
docker compose exec debian-dev bash
```

Dentro del shell:

```bash
whoami
hostname
pwd
ls src/
exit
```

`stdin_open` y `tty` permiten esta sesion interactiva con prompt y
colores.

### Paso 4.4: Compilar Rust

```bash
docker compose exec debian-dev bash -c '
cd src
cat > hello.rs << EOF
fn main() {
    println!("Hello from Rust in Docker!");
}
EOF
rustc -o hello_rs hello.rs && ./hello_rs
'
```

### Paso 4.5: gdb con SYS_PTRACE

```bash
docker compose exec debian-dev bash -c '
cd src
gcc -g -o hello hello.c
echo -e "break main\nrun\ncontinue\nquit" | gdb -batch ./hello 2>&1 | tail -8
'
```

gdb funciona gracias a `cap_add: SYS_PTRACE` en el compose.yml.

### Paso 4.6: Comunicacion entre contenedores

```bash
docker compose exec debian-dev ping -c 2 alma-dev
docker compose exec alma-dev ping -c 2 debian-dev
```

Ambos contenedores estan en la misma red de Compose y se resuelven por
nombre de servicio.

### Paso 4.7: Hostnames

```bash
docker compose exec debian-dev hostname
docker compose exec alma-dev hostname
```

`hostname: debian` y `hostname: alma` facilitan identificar en que
contenedor se esta trabajando.

---

## Limpieza final

```bash
docker compose down -v
rm -f src/hello src/hello_rs src/hello.rs
```

```bash
docker image ls | grep -E "labs.*(debian|alma)" | awk '{print $3}' | xargs docker rmi 2>/dev/null
```

---

## Conceptos reforzados

1. El compose.yml del curso define **dos servicios** (debian-dev y
   alma-dev) que comparten un **named volume** (`workspace`) y un
   **bind mount** (`./src`).

2. El bind mount `./src` conecta el codigo fuente del host con ambos
   contenedores. Los cambios en el editor del host se reflejan
   inmediatamente.

3. `docker compose down` preserva volumenes; `docker compose down -v`
   los elimina. El bind mount `src/` nunca se pierde porque esta en
   el host.

4. `cap_add: SYS_PTRACE` es necesario para gdb, strace y valgrind
   dentro de los contenedores.

5. `stdin_open` y `tty` permiten sesiones interactivas con
   `docker compose exec`. Sin ellos, los shells no funcionan
   correctamente.

6. Los contenedores se comunican por **nombre de servicio** dentro de
   la red de Compose (`ping alma-dev` desde debian-dev).
