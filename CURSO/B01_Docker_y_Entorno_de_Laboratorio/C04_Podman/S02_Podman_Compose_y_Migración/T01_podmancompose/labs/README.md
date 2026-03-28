# Lab — podman-compose

## Objetivo

Laboratorio práctico para trabajar con archivos Compose en Podman usando
dos alternativas: `podman-compose` (herramienta third-party) y Docker
Compose via socket API compatible. Comparar ambas opciones y entender
sus limitaciones.

## Prerequisitos

- Podman, podman-compose, y Docker Compose instalados
- Imágenes descargadas: `podman pull docker.io/library/nginx:latest && podman pull docker.io/library/alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `compose.yml` | Web (nginx) + app (alpine con loop) |

---

## Parte 1 — podman-compose

### Objetivo

Usar `podman-compose` para levantar servicios definidos en un archivo
Compose, demostrando que los mismos archivos funcionan con Podman.

### Paso 1.1: Examinar el archivo

```bash
cat compose.yml
```

Dos servicios con nombres de imagen completos (`docker.io/library/...`).
Los nombres completos son necesarios para evitar la pregunta interactiva
de selección de registry.

### Paso 1.2: Levantar con podman-compose

```bash
podman-compose up -d
```

`podman-compose` parsea el YAML y ejecuta comandos `podman run` por
cada servicio.

### Paso 1.3: Verificar

```bash
podman-compose ps
```

```bash
curl -s http://localhost:8888 | head -3
```

### Paso 1.4: Logs

```bash
podman-compose logs app --tail 3
```

### Paso 1.5: Exec

```bash
podman-compose exec web nginx -v
```

Los comandos de `podman-compose` siguen la misma interfaz que
`docker compose`.

### Paso 1.6: Limpiar

```bash
podman-compose down
```

---

## Parte 2 — Docker Compose via socket API

### Objetivo

Usar Docker Compose nativo con Podman como backend a través del
socket API compatible. Esta alternativa ofrece mayor compatibilidad
de features que `podman-compose`.

### Paso 2.1: Activar el socket

```bash
systemctl --user start podman.socket
```

### Paso 2.2: Verificar el socket

```bash
ls -la /run/user/$(id -u)/podman/podman.sock
```

### Paso 2.3: Levantar con Docker Compose

```bash
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose up -d
```

Docker Compose habla con Podman a través del socket. Los contenedores
los crea Podman, no Docker.

### Paso 2.4: Verificar

```bash
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose ps
```

```bash
curl -s http://localhost:8888 | head -3
```

### Paso 2.5: Los contenedores son de Podman

```bash
podman ps
```

Los contenedores aparecen en `podman ps` — confirma que Podman los
gestiona, no Docker.

### Paso 2.6: Limpiar

```bash
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose down
systemctl --user stop podman.socket
```

---

## Parte 3 — Comparación

### Objetivo

Comparar las diferencias prácticas entre `podman-compose` y Docker
Compose via socket.

### Paso 3.1: Validación de configuración

```bash
podman-compose config 2>&1 | head -10
```

```bash
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose config 2>&1 | head -10
```

`docker compose config` es más completo y muestra la configuración
expandida. `podman-compose config` tiene soporte parcial.

### Paso 3.2: Detección de cambios

Con Docker Compose via socket, `up -d` detecta cambios y recrea solo
los servicios modificados (como Docker nativo). Con `podman-compose`,
la detección de cambios es limitada.

### Paso 3.3: Resumen

```
podman-compose:
  + Instalación simple (pip/dnf)
  + No requiere socket ni Docker CLI
  - Features parciales (config, healthcheck conditions)
  - Detección de cambios limitada

Docker Compose via socket:
  + Compatibilidad completa de features
  + Detección de cambios nativa
  + Misma experiencia que Docker nativo
  - Requiere Docker Compose CLI instalado
  - Requiere activar podman.socket
```

Para desarrollo con Compose files complejos, Docker Compose via socket
es la opción recomendada. Para archivos simples, `podman-compose`
funciona correctamente.

---

## Limpieza final

```bash
# Eliminar recursos del lab (si alguno quedó)
podman-compose down 2>/dev/null
DOCKER_HOST=unix:///run/user/$(id -u)/podman/podman.sock docker compose down 2>/dev/null
systemctl --user stop podman.socket 2>/dev/null
```

---

## Conceptos reforzados

1. `podman-compose` es un script Python que traduce el archivo Compose
   a comandos `podman run`. Soporta la mayoría de features básicos
   pero tiene limitaciones con features avanzados.

2. Docker Compose via **socket API** (`podman.socket`) ofrece
   compatibilidad completa. Los contenedores los crea Podman pero
   la interfaz es Docker Compose nativo.

3. Los archivos Compose para Podman deben usar **nombres de imagen
   completos** (`docker.io/library/nombre:tag`) para evitar la
   pregunta interactiva de selección de registry.

4. Los contenedores creados via socket aparecen en `podman ps` — son
   contenedores de Podman gestionados a través de la API compatible.
