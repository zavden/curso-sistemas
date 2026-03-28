# Lab — Cuándo usar Docker vs Podman

## Objetivo

Laboratorio práctico para comparar Docker y Podman en situaciones
reales: examinar las diferencias de entorno, ejecutar el mismo workflow
completo en ambas herramientas, y practicar la toma de decisión sobre
cuál usar según el contexto.

## Prerequisitos

- Podman y Docker instalados
- Imágenes descargadas en ambos: `docker pull alpine:latest && podman pull docker.io/library/alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `Dockerfile.test` | Dockerfile para comparar build en ambas herramientas |
| `app.sh` | Script simple para el build |

---

## Parte 1 — Comparar entornos

### Objetivo

Examinar las diferencias prácticas entre Docker y Podman en cuanto a
daemon, storage, y seguridad.

### Paso 1.1: Daemon

```bash
echo "=== Docker ==="
systemctl is-active docker
ps aux | grep dockerd | grep -v grep | awk '{print "Proceso:", $1, $11}'

echo ""
echo "=== Podman ==="
systemctl is-active podman 2>&1
ps aux | grep "podman" | grep -v grep | wc -l
echo "(0 procesos = sin daemon)"
```

Docker tiene un daemon root permanente. Podman no tiene ninguno.

### Paso 1.2: Storage

```bash
echo "=== Docker ==="
docker info 2>/dev/null | grep "Docker Root Dir"

echo ""
echo "=== Podman ==="
podman info | grep graphRoot
```

Docker: `/var/lib/docker` (root, compartido). Podman:
`~/.local/share/containers/` (por usuario, aislado).

### Paso 1.3: Seguridad — proceso del contenedor

```bash
docker run -d --name cmp-docker alpine sleep 120
podman run -d --name cmp-podman docker.io/library/alpine sleep 120

echo "=== Owner del proceso ==="
ps aux | grep "sleep 120" | grep -v grep | awk '{print $1, $2, $11}'
```

Docker: el proceso corre como **root** en el host. Podman: corre
como **tu usuario**. Esta es la diferencia de seguridad más
significativa.

### Paso 1.4: Limpiar

```bash
docker rm -f cmp-docker
podman rm -f cmp-podman
```

---

## Parte 2 — Mismo workflow completo

### Objetivo

Ejecutar un workflow completo idéntico (build → run → verify → clean)
en Docker y Podman, demostrando que el resultado es el mismo.

### Paso 2.1: Examinar los archivos

```bash
cat Dockerfile.test
cat app.sh
```

### Paso 2.2: Workflow con Docker

```bash
echo "===== Docker workflow ====="

docker build -t lab-compare -f Dockerfile.test .
docker run --rm -e CONTAINER_TOOL=docker lab-compare
docker images lab-compare --format "{{.Repository}}:{{.Tag}} {{.Size}}"
docker rmi lab-compare
```

### Paso 2.3: Workflow con Podman

```bash
echo "===== Podman workflow ====="

podman build -t lab-compare -f Dockerfile.test .
podman run --rm -e CONTAINER_TOOL=podman lab-compare
podman images lab-compare --format "{{.Repository}}:{{.Tag}} {{.Size}}"
podman rmi lab-compare
```

La salida funcional es idéntica. El mismo Dockerfile, los mismos
flags, los mismos comandos. Solo cambia el nombre del binario.

---

## Parte 3 — Decisión práctica

### Objetivo

Para cada escenario, decidir qué herramienta usar y por qué.
Responder mentalmente antes de leer la respuesta.

### Paso 3.1: Escenarios

Para cada caso, responde mentalmente: **Docker o Podman, y por qué.**

```
1. Servidor RHEL 9 en producción con SELinux enforcing
2. Laptop de desarrollo con equipo que usa Docker Compose
3. Pipeline CI/CD en GitHub Actions
4. Preparación para examen RHCSA
5. Ejecutar contenedores como servicios del sistema
6. Proyecto con Dockerfiles que usan BuildKit cache mounts
7. Entorno con requisitos estrictos de seguridad rootless
8. Prototipado local antes de migrar a Kubernetes
```

Intenta responder todos antes de continuar al siguiente paso.

### Paso 3.2: Respuestas

```
1. RHEL 9 con SELinux       → Podman (preinstalado, integración SELinux nativa)
2. Equipo con Docker Compose → Docker (compatibilidad completa, sin fricción)
3. GitHub Actions CI/CD      → Docker (soporte nativo en la plataforma)
4. Examen RHCSA              → Podman (el examen usa Podman exclusivamente)
5. Servicios con systemd     → Podman (generate systemd, Quadlet)
6. BuildKit cache mounts     → Docker (Buildah no soporta --mount=type=cache)
7. Seguridad rootless        → Podman (rootless by default, sin daemon root)
8. Prototipo para K8s        → Podman (pods nativos, generate/play kube)
```

### Paso 3.3: La respuesta pragmática

```bash
echo "=== Estado de tu sistema ==="
echo "Docker: $(docker --version 2>/dev/null || echo 'no instalado')"
echo "Podman: $(podman --version 2>/dev/null || echo 'no instalado')"
echo "Compose: $(docker compose version 2>/dev/null || echo 'no instalado')"
echo "podman-compose: $(podman-compose --version 2>/dev/null || echo 'no instalado')"
echo "OS: $(cat /etc/os-release | grep PRETTY_NAME | cut -d= -f2)"
echo "SELinux: $(getenforce 2>/dev/null || echo 'no disponible')"
```

La herramienta correcta depende del contexto, no de preferencias.
Con los conocimientos de este bloque, puedes trabajar con ambas.

---

## Limpieza final

```bash
docker rm -f cmp-docker 2>/dev/null
podman rm -f cmp-podman 2>/dev/null
docker rmi lab-compare 2>/dev/null
podman rmi lab-compare 2>/dev/null
```

---

## Conceptos reforzados

1. Docker y Podman producen **resultados idénticos** para el mismo
   workflow (build, run, verify). La diferencia está en la
   arquitectura (daemon vs daemonless) y la seguridad (rootful vs
   rootless).

2. **Docker** es preferible cuando: el equipo ya lo usa, se necesita
   Docker Compose completo, se depende de BuildKit, o la plataforma
   CI/CD lo requiere.

3. **Podman** es preferible cuando: se trabaja en RHEL/Fedora, la
   seguridad rootless es prioridad, se necesita integración con
   systemd, se prepara para RHCSA, o se planea migrar a Kubernetes.

4. La habilidad valiosa es **entender contenedores OCI**, no depender
   de una herramienta específica. Los conocimientos son transferibles
   entre Docker, Podman, y cualquier runtime OCI.
