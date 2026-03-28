# Lab — Migrar de Docker a Podman

## Objetivo

Laboratorio práctico para realizar una migración de Docker a Podman:
transferir imágenes entre herramientas, adaptar Compose files para
compatibilidad, manejar la limitación de puertos privilegiados en
rootless, y reemplazar restart policies con systemd units.

## Prerequisitos

- Podman y Docker instalados
- Imágenes descargadas en Docker: `docker pull nginx:latest && docker pull alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `compose.docker.yml` | Compose original con estilo Docker |
| `compose.podman.yml` | Compose adaptado para Podman |
| `html/index.html` | Contenido HTML para bind mount |

---

## Parte 1 — Migrar imágenes

### Objetivo

Demostrar cómo transferir imágenes de Docker a Podman usando
`docker save` y `podman load`.

### Paso 1.1: Verificar que la imagen existe en Docker

```bash
docker images | grep nginx
```

### Paso 1.2: Exportar desde Docker

```bash
docker save nginx:latest -o /tmp/nginx-migration.tar
ls -lh /tmp/nginx-migration.tar
```

`docker save` exporta la imagen con todas sus capas a un archivo tar.

### Paso 1.3: Importar en Podman

```bash
podman load -i /tmp/nginx-migration.tar
```

### Paso 1.4: Verificar

```bash
podman images | grep nginx
```

La imagen está ahora disponible en Podman. El formato OCI es estándar
— la transferencia es directa.

### Paso 1.5: Probar

```bash
podman run --rm -d --name lab-migrated -p 8888:80 docker.io/library/nginx:latest
curl -s http://localhost:8888 | head -3
podman rm -f lab-migrated
```

### Paso 1.6: Limpiar

```bash
rm -f /tmp/nginx-migration.tar
```

---

## Parte 2 — Adaptar Compose files

### Objetivo

Demostrar los cambios necesarios en un archivo Compose para que
funcione con Podman: nombres de imagen completos y labels SELinux.

### Paso 2.1: Examinar el archivo Docker original

```bash
cat compose.docker.yml
```

Tres problemas para Podman:
1. `image: nginx:latest` — nombre corto (Podman puede preguntar registry)
2. `volumes: - ./html:/usr/share/nginx/html` — sin label SELinux
3. `restart: unless-stopped` — funciona pero la persistencia real
   requiere systemd

### Paso 2.2: Examinar el archivo adaptado

```bash
cat compose.podman.yml
```

Cambios realizados:
1. `image: docker.io/library/nginx:latest` — nombre completo
2. `volumes: - ./html:/usr/share/nginx/html:z` — label `:z` para SELinux

### Paso 2.3: Comparar ambos archivos

```bash
diff compose.docker.yml compose.podman.yml
```

Salida esperada:

```
<     image: nginx:latest
---
>     image: docker.io/library/nginx:latest
<       - ./html:/usr/share/nginx/html
---
>       - ./html:/usr/share/nginx/html:z
```

Los cambios son mínimos. La mayoría de Compose files necesitan solo
estos dos ajustes.

### Paso 2.4: Probar con podman-compose

```bash
podman-compose -f compose.podman.yml up -d
curl -s http://localhost:8888
podman-compose -f compose.podman.yml down
```

El archivo adaptado funciona con Podman.

---

## Parte 3 — Puertos privilegiados

### Objetivo

Demostrar que Podman rootless no puede usar puertos < 1024 por defecto,
y las soluciones disponibles.

### Paso 3.1: Intentar puerto privilegiado

```bash
podman run --rm -p 80:80 docker.io/library/nginx 2>&1
```

Salida esperada (en rootless):

```
Error: rootlessport cannot expose privileged port 80...
```

Los puertos < 1024 están reservados para procesos con privilegios.
Podman rootless no tiene estos privilegios.

### Paso 3.2: Solución 1 — usar puerto alto

```bash
podman run --rm -d --name lab-port -p 8080:80 docker.io/library/nginx
curl -s http://localhost:8080 | head -3
podman rm -f lab-port
```

La solución más simple: usar puertos >= 1024 (8080, 8443, 3000, etc.).

### Paso 3.3: Solución 2 — configurar el kernel

```bash
cat /proc/sys/net/ipv4/ip_unprivileged_port_start
```

Salida esperada: `1024` (valor por defecto).

Para permitir puertos desde el 80:

```bash
sudo sysctl net.ipv4.ip_unprivileged_port_start=80
```

Ahora los puertos >= 80 están disponibles para procesos sin
privilegios. Este cambio se pierde al reiniciar salvo que se
configure en `/etc/sysctl.d/`.

### Paso 3.4: Restaurar valor original

```bash
sudo sysctl net.ipv4.ip_unprivileged_port_start=1024
```

---

## Parte 4 — Restart policies y systemd

### Objetivo

Demostrar cómo reemplazar `--restart unless-stopped` de Docker con
systemd units en Podman para persistencia real.

### Paso 4.1: Docker restart policy

```bash
docker run -d --restart unless-stopped --name lab-docker-restart nginx
docker ps --filter name=lab-docker-restart --format "{{.Names}} {{.Status}}"
```

Docker daemon gestiona los reinicios. Si Docker se cae, la policy
no funciona.

### Paso 4.2: Podman — restart funciona parcialmente

```bash
podman run -d --restart unless-stopped --name lab-podman-restart -p 8888:80 docker.io/library/nginx
```

Podman soporta `--restart` pero sin daemon, la persistencia después
del logout **no está garantizada**.

### Paso 4.3: La solución correcta — systemd

```bash
mkdir -p ~/.config/systemd/user
podman generate systemd --new --name lab-podman-restart > ~/.config/systemd/user/container-lab-restart.service
```

### Paso 4.4: Gestionar como servicio

```bash
podman stop lab-podman-restart && podman rm lab-podman-restart
systemctl --user daemon-reload
systemctl --user start container-lab-restart.service
systemctl --user status container-lab-restart.service
```

```bash
curl -s http://localhost:8888 | head -3
```

Ahora el contenedor está gestionado por systemd: arranque automático,
restart en fallo, logs en journalctl.

### Paso 4.5: Limpiar

```bash
systemctl --user stop container-lab-restart.service
systemctl --user disable container-lab-restart.service 2>/dev/null
rm ~/.config/systemd/user/container-lab-restart.service
systemctl --user daemon-reload
docker rm -f lab-docker-restart
```

---

## Limpieza final

```bash
# Eliminar recursos del lab (si alguno quedó)
podman rm -f lab-migrated lab-port lab-podman-restart 2>/dev/null
docker rm -f lab-docker-restart 2>/dev/null
systemctl --user stop container-lab-restart.service 2>/dev/null
rm -f ~/.config/systemd/user/container-lab-restart.service
systemctl --user daemon-reload 2>/dev/null
rm -f /tmp/nginx-migration.tar
sudo sysctl net.ipv4.ip_unprivileged_port_start=1024 2>/dev/null
```

---

## Conceptos reforzados

1. Las imágenes se migran entre Docker y Podman con `docker save` →
   `podman load`. El formato OCI es intercambiable.

2. Los Compose files para Podman necesitan dos ajustes: **nombres de
   imagen completos** (`docker.io/library/...`) y **labels SELinux**
   (`:z`) en bind mounts.

3. Podman rootless **no puede usar puertos < 1024** por defecto.
   Soluciones: usar puertos altos (recomendado) o configurar
   `net.ipv4.ip_unprivileged_port_start`.

4. Las **restart policies** de Docker dependen del daemon. En Podman,
   la persistencia real requiere **systemd units** generadas con
   `podman generate systemd`.

5. La migración de Docker a Podman es mayormente directa. Los ajustes
   principales son: nombres de imagen, SELinux, puertos privilegiados,
   y persistencia con systemd.
