# Lab — Qué es Podman

## Objetivo

Laboratorio práctico para entender qué es Podman y en qué se diferencia
de Docker a nivel arquitectónico: ejecutar contenedores rootless sin
daemon, verificar el almacenamiento independiente por usuario, y
demostrar la compatibilidad OCI entre Docker y Podman.

## Prerequisitos

- Podman y Docker instalados (`podman --version && docker --version`)
- Imagen base descargada: `podman pull docker.io/library/alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `Dockerfile.compat` | Dockerfile para probar compatibilidad OCI |

---

## Parte 1 — Primer contenedor rootless

### Objetivo

Demostrar que Podman ejecuta contenedores **sin root y sin daemon**,
a diferencia del modelo cliente-daemon de Docker.

### Paso 1.1: Verificar que Podman es rootless

```bash
podman info | grep -i rootless
```

Salida esperada:

```
  rootless: true
```

Podman se ejecuta como tu usuario, sin privilegios de root.

### Paso 1.2: Ejecutar el primer contenedor

```bash
podman run --rm docker.io/library/alpine echo "Hello from rootless Podman"
```

Salida esperada:

```
Hello from rootless Podman
```

Sin `sudo`, sin daemon, sin socket. Podman ejecutó directamente el
runtime OCI (runc).

### Paso 1.3: Verificar identidad dentro del contenedor

```bash
podman run --rm docker.io/library/alpine id
```

Salida esperada:

```
uid=0(root) gid=0(root)
```

Dentro del contenedor eres "root". Pero en el host, el proceso
pertenece a tu usuario.

### Paso 1.4: Verificar el proceso en el host

```bash
podman run -d --name lab-rootless docker.io/library/alpine sleep 60
```

```bash
ps aux | grep "sleep 60" | grep -v grep
```

El proceso `sleep 60` corre como **tu usuario** (UID 1000), no como
root. Si un atacante escapa del contenedor, obtiene los permisos de
tu usuario, no de root.

### Paso 1.5: Comparar con Docker

```bash
docker run -d --name lab-docker-root alpine sleep 60
ps aux | grep "sleep 60" | grep -v grep
```

Con Docker (rootful), el proceso corre como **root** en el host.

### Paso 1.6: Limpiar

```bash
podman rm -f lab-rootless
docker rm -f lab-docker-root
```

---

## Parte 2 — Sin daemon

### Objetivo

Demostrar que Podman no tiene daemon central: no hay proceso permanente
corriendo como root, y cada contenedor es independiente.

### Paso 2.1: Docker tiene un daemon

```bash
ps aux | grep dockerd | grep -v grep
```

Salida esperada:

```
root  ... /usr/bin/dockerd ...
```

`dockerd` es un proceso root corriendo permanentemente. Si se cae,
no puedes gestionar contenedores.

### Paso 2.2: Podman NO tiene daemon

```bash
ps aux | grep podman | grep -v grep
```

Salida esperada: sin resultados (o solo tu comando grep). No hay
proceso `podman` corriendo permanentemente.

### Paso 2.3: Cada contenedor tiene su propio monitor

```bash
podman run -d --name lab-nodaemon docker.io/library/alpine sleep 300
```

```bash
ps aux | grep conmon | grep -v grep
```

Salida esperada:

```
user  ... /usr/bin/conmon ...
```

Cada contenedor tiene un proceso **conmon** (container monitor). Es
ligero y específico de ese contenedor. Si Podman CLI se cierra, el
contenedor sigue corriendo gestionado por conmon.

### Paso 2.4: Podman CLI es efímero

Antes de ejecutar, responde mentalmente:

- ¿Qué pasará con el contenedor si Podman no está corriendo como
  proceso?
- ¿Podremos listar el contenedor después?

Intenta predecir antes de continuar al siguiente paso.

```bash
podman ps
```

Podman CLI es un proceso efímero — se ejecuta, hace su trabajo, y
termina. Los contenedores siguen corriendo independientemente.

### Paso 2.5: Limpiar

```bash
podman rm -f lab-nodaemon
```

---

## Parte 3 — Storage por usuario

### Objetivo

Demostrar que Podman rootless almacena imágenes y contenedores en un
directorio **por usuario**, separado del storage rootful.

### Paso 3.1: Storage rootless

```bash
podman info | grep graphRoot
```

Salida esperada:

```
  graphRoot: /home/<user>/.local/share/containers/storage
```

Las imágenes y contenedores rootless se guardan en tu directorio home.

### Paso 3.2: Listar imágenes rootless

```bash
podman images
```

Solo muestra las imágenes descargadas por tu usuario.

### Paso 3.3: Comparar con rootful

```bash
sudo podman images
```

Salida diferente (posiblemente vacía). El storage rootful
(`/var/lib/containers/`) es completamente independiente.

```bash
sudo podman info | grep graphRoot
```

Salida esperada:

```
  graphRoot: /var/lib/containers/storage
```

### Paso 3.4: Las imágenes NO se comparten

```bash
podman images | grep alpine
```

Alpine está en tu storage rootless.

```bash
sudo podman images | grep alpine
```

Alpine **no aparece** en el storage rootful (salvo que la hayas
descargado con `sudo`). Cada contexto tiene su propio almacenamiento.

### Paso 3.5: Comparar con Docker

```bash
docker info 2>/dev/null | grep "Docker Root Dir"
```

Salida esperada:

```
 Docker Root Dir: /var/lib/docker
```

Docker almacena todo en un solo directorio gestionado por el daemon
root. No hay separación por usuario.

---

## Parte 4 — Compatibilidad OCI

### Objetivo

Demostrar que las imágenes OCI son intercambiables entre Docker y
Podman: el mismo Dockerfile produce la misma imagen en ambos.

### Paso 4.1: Examinar el Dockerfile

```bash
cat Dockerfile.compat
```

Un Dockerfile estándar que funciona en cualquier herramienta OCI.

### Paso 4.2: Construir con Podman

```bash
podman build -t lab-compat -f Dockerfile.compat .
podman run --rm lab-compat
```

Salida esperada:

```
Built with OCI tools
```

### Paso 4.3: Construir con Docker

```bash
docker build -t lab-compat -f Dockerfile.compat .
docker run --rm lab-compat
```

Salida esperada (idéntica):

```
Built with OCI tools
```

El mismo Dockerfile, el mismo resultado. Podman usa Buildah
internamente, pero la compatibilidad con la sintaxis de Dockerfile
es completa.

### Paso 4.4: Migrar imagen entre herramientas

```bash
docker save lab-compat -o /tmp/lab-compat.tar
podman rmi lab-compat
podman load -i /tmp/lab-compat.tar
podman run --rm lab-compat
```

Salida esperada:

```
Built with OCI tools
```

La imagen exportada de Docker se carga en Podman sin problemas.
El formato OCI es estándar e intercambiable.

### Paso 4.5: Limpiar

```bash
podman rmi lab-compat
docker rmi lab-compat
rm -f /tmp/lab-compat.tar
```

---

## Limpieza final

```bash
# Eliminar recursos del lab (si alguno quedó)
podman rm -f lab-rootless lab-nodaemon 2>/dev/null
docker rm -f lab-docker-root 2>/dev/null
podman rmi lab-compat 2>/dev/null
docker rmi lab-compat 2>/dev/null
rm -f /tmp/lab-compat.tar
```

---

## Conceptos reforzados

1. Podman ejecuta contenedores **sin daemon y sin root**. El proceso
   dentro del contenedor pertenece a tu usuario en el host, no a root.

2. No hay proceso `podman` corriendo permanentemente. Cada contenedor
   tiene su propio **conmon** (container monitor) independiente.

3. El storage rootless está en `~/.local/share/containers/`. Las
   imágenes de un usuario **no son visibles** para otro usuario ni
   para `sudo podman`.

4. Las imágenes OCI son **intercambiables** entre Docker y Podman.
   El mismo Dockerfile produce la misma imagen en ambos. `docker save`
   y `podman load` permiten migrar imágenes entre herramientas.

5. Para usar imágenes de Docker Hub en Podman, usar el nombre completo:
   `docker.io/library/nombre:tag`.
