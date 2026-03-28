# Lab — Diferencias clave

## Objetivo

Laboratorio práctico para explorar las diferencias clave entre Docker y
Podman que van más allá de los comandos: comportamiento sin daemon,
integración nativa con systemd, etiquetas SELinux para bind mounts, y
el socket API compatible para usar Docker Compose con Podman.

## Prerequisitos

- Podman y Docker instalados
- Imágenes descargadas: `podman pull docker.io/library/nginx:latest && podman pull docker.io/library/alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

No hay archivos de soporte. Los recursos se crean durante el lab.

---

## Parte 1 — Sin daemon

### Objetivo

Demostrar las implicaciones prácticas de no tener daemon: no hay
punto único de fallo, y cada contenedor es gestionado por su propio
proceso conmon.

### Paso 1.1: El daemon de Docker

```bash
systemctl is-active docker
```

Salida esperada: `active`. Docker necesita un daemon corriendo
permanentemente.

```bash
ps aux | grep dockerd | grep -v grep | awk '{print $1, $2, $11}'
```

`dockerd` corre como root. Si este proceso se cae, no puedes
gestionar ningún contenedor.

### Paso 1.2: Podman no tiene daemon

```bash
systemctl is-active podman 2>&1
```

Salida esperada: `inactive` o `could not be found`. No hay servicio
`podman` porque no hay daemon.

### Paso 1.3: conmon — un monitor por contenedor

```bash
podman run -d --name lab-conmon docker.io/library/alpine sleep 300
```

```bash
ps aux | grep conmon | grep -v grep
```

Salida esperada:

```
user  <pid>  ... /usr/bin/conmon ...
```

Cada contenedor tiene su proceso **conmon** que:
- Recolecta el exit code al terminar
- Gestiona stdout/stderr (logs)
- Mantiene el contenedor corriendo independientemente de Podman CLI

### Paso 1.4: Matar conmon vs matar dockerd

Si conmon de un contenedor muere, solo ese contenedor se ve afectado.
Si dockerd muere, **todos** los contenedores quedan sin gestión.

```bash
podman rm -f lab-conmon
```

---

## Parte 2 — Integración con systemd

### Objetivo

Demostrar cómo Podman genera unit files de systemd para gestionar
contenedores como servicios del sistema, con arranque automático,
logs en journalctl, y restart en caso de fallo.

### Paso 2.1: Crear un contenedor

```bash
podman run -d --name lab-systemd -p 8888:80 docker.io/library/nginx
```

### Paso 2.2: Verificar que funciona

```bash
curl -s http://localhost:8888 | head -3
```

### Paso 2.3: Generar unit file de systemd

```bash
mkdir -p ~/.config/systemd/user
podman generate systemd --new --name lab-systemd > ~/.config/systemd/user/container-lab-systemd.service
```

`--new` genera un unit que crea el contenedor desde cero cada vez
(no depende de un contenedor existente).

### Paso 2.4: Examinar el unit file

```bash
cat ~/.config/systemd/user/container-lab-systemd.service
```

El unit file contiene:
- `ExecStart`: comando `podman run` completo
- `ExecStop`: `podman stop`
- `Restart=on-failure`: reinicio automático

### Paso 2.5: Activar como servicio

```bash
podman stop lab-systemd && podman rm lab-systemd
systemctl --user daemon-reload
systemctl --user start container-lab-systemd.service
```

### Paso 2.6: Verificar el servicio

```bash
systemctl --user status container-lab-systemd.service
```

```bash
curl -s http://localhost:8888 | head -3
```

El contenedor ahora está gestionado por systemd, no por Podman
directamente.

### Paso 2.7: Logs via journalctl

```bash
journalctl --user -u container-lab-systemd.service --no-pager | tail -5
```

Los logs del contenedor están disponibles en journalctl, integrados
con el resto del sistema.

### Paso 2.8: Limpiar

```bash
systemctl --user stop container-lab-systemd.service
systemctl --user disable container-lab-systemd.service 2>/dev/null
rm ~/.config/systemd/user/container-lab-systemd.service
systemctl --user daemon-reload
```

---

## Parte 3 — SELinux y bind mounts

### Objetivo

Demostrar que en sistemas con SELinux enforcing (Fedora, RHEL), los
bind mounts necesitan labels `:z` o `:Z` para que el contenedor pueda
acceder a los archivos del host.

### Paso 3.1: Verificar SELinux

```bash
getenforce 2>/dev/null || echo "SELinux no disponible"
```

Si la salida es `Enforcing`, los siguientes pasos demostrarán el
problema y la solución. Si SELinux no está activo, los pasos
funcionarán sin `:z` pero es importante entender el concepto para
entornos RHEL.

### Paso 3.2: Crear directorio de prueba

```bash
mkdir -p /tmp/lab-selinux
echo "test data" > /tmp/lab-selinux/test.txt
```

### Paso 3.3: Bind mount sin label

```bash
podman run --rm -v /tmp/lab-selinux:/data docker.io/library/alpine cat /data/test.txt 2>&1
```

Con SELinux `Enforcing`: **Permission denied**. Sin SELinux: funciona.

SELinux asigna etiquetas de contexto a archivos y procesos. El
contenedor tiene una etiqueta diferente a los archivos del host.

### Paso 3.4: Bind mount con :z

```bash
podman run --rm -v /tmp/lab-selinux:/data:z docker.io/library/alpine cat /data/test.txt
```

Salida esperada:

```
test data
```

`:z` reetiqueta el directorio con `svirt_sandbox_file_t` (shared),
permitiendo acceso desde **cualquier** contenedor.

### Paso 3.5: Diferencia entre :z y :Z

```bash
ls -lZ /tmp/lab-selinux/test.txt 2>/dev/null
```

- `:z` (minúscula): shared — cualquier contenedor puede acceder
- `:Z` (mayúscula): private — solo este contenedor puede acceder

**Precaución**: nunca usar `:Z` en directorios del sistema (`/home`,
`/etc`) — puede romper el acceso del host.

### Paso 3.6: Limpiar

```bash
rm -rf /tmp/lab-selinux
```

---

## Parte 4 — Socket API compatible

### Objetivo

Demostrar que Podman puede exponer un socket API compatible con
Docker, permitiendo que herramientas como Docker Compose funcionen
con Podman como backend.

### Paso 4.1: Activar el socket de Podman

```bash
systemctl --user start podman.socket
```

### Paso 4.2: Verificar el socket

```bash
ls -la /run/user/$(id -u)/podman/podman.sock
```

El socket existe en el directorio de runtime del usuario.

### Paso 4.3: Docker CLI via socket de Podman

```bash
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker info 2>&1 | head -10
```

Docker CLI habla con Podman a través del socket. La salida muestra
información de Podman, no de Docker.

### Paso 4.4: Docker Compose via socket

```bash
mkdir -p /tmp/lab-socket
cat > /tmp/lab-socket/compose.yml << 'EOF'
services:
  web:
    image: docker.io/library/nginx:latest
    ports:
      - "8888:80"
EOF

cd /tmp/lab-socket
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose up -d
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose ps
curl -s http://localhost:8888 | head -3
```

Docker Compose crea contenedores usando Podman como backend.

### Paso 4.5: Limpiar

```bash
cd /tmp/lab-socket
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose down
cd -
rm -rf /tmp/lab-socket
systemctl --user stop podman.socket
```

---

## Limpieza final

```bash
# Eliminar recursos del lab (si alguno quedó)
podman rm -f lab-conmon lab-systemd 2>/dev/null
systemctl --user stop container-lab-systemd.service 2>/dev/null
rm -f ~/.config/systemd/user/container-lab-systemd.service
systemctl --user daemon-reload 2>/dev/null
systemctl --user stop podman.socket 2>/dev/null
rm -rf /tmp/lab-selinux /tmp/lab-socket
```

---

## Conceptos reforzados

1. Docker depende de un **daemon root permanente** (`dockerd`). Si se
   cae, todos los contenedores quedan sin gestión. Podman no tiene
   daemon — cada contenedor tiene su propio **conmon** independiente.

2. `podman generate systemd --new` genera unit files que permiten
   gestionar contenedores como **servicios de systemd**: arranque
   automático, restart en fallo, logs en journalctl.

3. En sistemas con **SELinux enforcing**, los bind mounts necesitan
   `:z` (shared) o `:Z` (private) para que el contenedor tenga permiso
   de acceso. Sin label, SELinux bloquea el acceso.

4. El **socket API** de Podman (`podman.socket`) es compatible con
   Docker. Permite usar Docker Compose y otras herramientas Docker con
   Podman como backend via `DOCKER_HOST`.

5. Para que contenedores rootless **persistan después del logout**, se
   necesita `loginctl enable-linger` y gestión via systemd units.
