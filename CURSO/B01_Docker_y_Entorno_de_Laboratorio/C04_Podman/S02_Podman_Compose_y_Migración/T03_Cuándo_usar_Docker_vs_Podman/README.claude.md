# T03 — Cuándo usar Docker vs Podman

## No hay una respuesta universal

Docker y Podman son herramientas que resuelven el mismo problema (gestionar
contenedores) con enfoques diferentes. La elección depende del contexto: el
sistema operativo, los requisitos de seguridad, el equipo, la certificación
objetivo y la infraestructura existente.

## Usar Docker cuando...

### El equipo ya lo usa

Docker es la herramienta de contenedores más extendida. Si el equipo tiene
experiencia, scripts, CI/CD pipelines y documentación basada en Docker, migrar
a Podman solo para migrar no tiene sentido.

### Se necesita Docker Compose con todas sus features

Docker Compose es más maduro y completo que podman-compose. Features avanzados
como profiles, `depends_on` con healthcheck conditions, watch mode, y extensions
funcionan mejor con Docker nativo.

> **Nota**: Usar Docker Compose con el socket de Podman (`DOCKER_HOST`) ofrece
> casi la misma compatibilidad, pero algunas operaciones internas pueden diferir.

### Se depende de features exclusivos de BuildKit

Desde Buildah 1.24+, la mayoría de features de BuildKit están disponibles en
Podman. Sin embargo, algunos siguen siendo exclusivos de BuildKit:

| Feature | BuildKit (Docker) | Buildah (Podman) |
|---|---|---|
| Cache mounts (`--mount=type=cache`) | Sí | Sí (Buildah 1.24+) |
| Secret mounts (`--mount=type=secret`) | Sí | Sí (Buildah 1.24+) |
| SSH forwarding (`--mount=type=ssh`) | Sí | Sí (Buildah 1.24+) |
| Cache export/import (registry) | Sí | No |
| Inline cache (`--cache-from`) | Sí | No |
| `# syntax=` directive | Sí | No |
| Paralelización de stages | Sí | Parcial |

La ventaja real de Docker sobre Podman en builds está en el **cache
export/import con registries** (útil en CI/CD) y la paralelización de
multi-stage builds.

### Desarrollo en macOS/Windows

Docker Desktop tiene mejor soporte para macOS y Windows (VM integrada,
filesystem sharing optimizado). Podman Machine funciona pero es menos maduro.

### CI/CD configurado para Docker

GitHub Actions, GitLab CI, Jenkins — la mayoría de plataformas de CI/CD
tienen soporte nativo para Docker. Podman funciona pero puede requerir
configuración adicional.

## Usar Podman cuando...

### Se trabaja en RHEL/Fedora/CentOS

Podman viene preinstalado y es la herramienta oficial de Red Hat para
contenedores. Docker no está en los repositorios oficiales de RHEL:

```bash
# RHEL/Fedora: Podman está ahí
podman --version

# Docker requiere configurar repos de terceros
# sudo dnf config-manager --add-repo https://download.docker.com/linux/fedora/docker-ce.repo
```

### Se prepara para RHCSA

El examen RHCSA usa **Podman, no Docker**. Los ejercicios prácticos del examen
cubren:
- `podman run`, `podman ps`, `podman exec`
- Configurar contenedores como servicios con systemd (Quadlet)
- Rootless containers
- Persistencia con `loginctl enable-linger`
- SELinux con `:z` y `:Z`

### La seguridad es prioridad

Podman es más seguro por diseño:

```
Docker rootful (configuración por defecto):
  Escape del contenedor → root en el host ← CRÍTICO

Podman rootless (configuración por defecto):
  Escape del contenedor → tu usuario sin privilegios ← Controlado

Docker rootless (requiere configuración extra):
  Similar a Podman pero no es el default
```

En entornos donde cada contenedor procesa datos de terceros o ejecuta código no
confiable, rootless de Podman reduce significativamente el riesgo.

### Se necesita integración con systemd

Podman permite gestionar contenedores como servicios del sistema, con todas las
ventajas de systemd:

- Arranque automático con el sistema
- Logs con journalctl
- Dependencias entre servicios
- Monitoreo con `systemctl status`
- Notificaciones de fallos
- **Quadlet** (Podman 4.4+): archivos `.container` declarativos sin necesidad
  de generar units manualmente

Docker usa restart policies internas que dependen del daemon.

### Se planea migrar a Kubernetes

Los pods de Podman y la capacidad de generar/consumir YAML de Kubernetes hacen
que Podman sea un puente natural hacia Kubernetes:

```bash
# Desarrollo local con Podman
podman pod create --name myapp -p 8080:80
podman run -d --pod myapp --name web docker.io/library/nginx
podman run -d --pod myapp --name api docker.io/library/alpine sleep 3600

# Generar manifiesto de Kubernetes (genera kind: Pod)
podman generate kube myapp > myapp-pod.yaml

# Desplegar en Kubernetes
kubectl apply -f myapp-pod.yaml
```

Con Docker, este paso requiere escribir los manifiestos manualmente o usar
herramientas de conversión.

## Comparación directa

| Criterio | Docker | Podman |
|---|---|---|
| Adopción en la industria | Mayoritaria | Creciente |
| Curva de aprendizaje | Estándar de facto | Compatible (misma CLI) |
| Seguridad por defecto | Root daemon | Rootless, sin daemon |
| Compose | Nativo, completo | Via podman-compose, socket o `podman compose` |
| Build engine | BuildKit (avanzado) | Buildah (comparable desde 1.24+) |
| macOS/Windows | Docker Desktop (maduro) | Podman Machine (en desarrollo) |
| RHEL/Fedora | Repo externo | Preinstalado |
| Certificaciones | — | RHCSA requiere Podman |
| systemd | No nativo | Nativo (Quadlet) |
| Kubernetes workflow | Herramientas externas | Pods, generate/play kube |
| Swarm | Sí | No |
| Community ecosystem | Enorme | Grande y creciente |

## El contexto de Kubernetes

Un punto que genera confusión: **Kubernetes no usa Docker ni Podman en
producción**. Kubernetes usa un **Container Runtime Interface (CRI)** para
ejecutar contenedores:

```
Kubernetes cluster:
  kubelet → CRI → containerd → runc → contenedor
                   o
  kubelet → CRI → CRI-O → runc → contenedor
```

Docker y Podman son **herramientas de desarrollo y administración**:
- Construir imágenes (`docker build` / `podman build`)
- Probar localmente (`docker run` / `podman run`)
- Gestionar registries (`docker push` / `podman push`)

La imagen construida con Docker funciona en Podman, en Kubernetes con containerd,
y en cualquier runtime OCI. **El formato es estándar**.

## Casos de decisión por escenario

| Escenario | Herramienta | Razón |
|---|---|---|
| Mac/Windows para desarrollo | Docker | Soporte maduro, VM integrada |
| Linux laptop personal | Podman | Rootless por defecto, seguro |
| RHEL/Fedora en producción | Podman | Preinstalado, soporte oficial |
| Ubuntu/Debian en producción | Docker o Podman | Ambos funcionan bien |
| Equipo ya usa Docker | Docker | Evitar fricción |
| Examen RHCSA | Podman | Es lo que se evalúa |
| Seguridad máxima | Podman | Rootless nativo, sin daemon |
| CI/CD en GitHub/GitLab | Docker | Soporte nativo mejor |
| Migrar a Kubernetes | Podman | Pods + generate kube |
| Cache export/import en CI | Docker | Feature exclusivo de BuildKit |
| Compose con features avanzados | Docker | Compose nativo completo |

## En este curso

Usamos **Docker como herramienta principal** del laboratorio por:
- Máxima compatibilidad con tutoriales y documentación existente
- Docker Compose completo para el entorno multi-contenedor
- Familiaridad de la mayoría de desarrolladores

Pero cubrimos **Podman** porque:
- RHCSA lo requiere
- Es importante entender la alternativa rootless/daemonless
- En entornos RHEL/Fedora es la opción estándar
- Las diferencias son mínimas y es fácil cambiar entre ambos

La habilidad valiosa es **entender contenedores OCI**, no depender de una
herramienta específica. Con los conocimientos de este curso, puedes trabajar
con Docker, Podman, containerd, o cualquier runtime OCI.

---

## Ejercicios

### Ejercicio 1 — Comparar entornos

```bash
echo "=== Docker ==="
docker info 2>/dev/null | grep -E '(Storage Driver|Server Version|Rootless)' | head -3
echo ""

echo "=== Podman ==="
podman info 2>/dev/null | grep -E '(graphDriverName|rootless|version)' | head -3
```

**Predicción**: ¿Qué diferencias esperas ver en Storage Driver y rootless?

<details><summary>Respuesta</summary>

- **Storage Driver**: Docker suele usar `overlay2`. Podman rootless también usa
  `overlay` (con fuse-overlayfs o native overlay según el kernel).
- **Rootless**: Docker muestra `Rootless: false` por defecto (corre como root).
  Podman muestra `rootless: true` si se ejecuta sin sudo.
- **Versión**: Son proyectos independientes con versiones diferentes.

</details>

### Ejercicio 2 — Verificar modelo de procesos

```bash
# Docker: contenedor via daemon
docker run -d --name cmp_docker docker.io/library/alpine sleep 120 > /dev/null

# Podman: contenedor sin daemon
podman run -d --name cmp_podman docker.io/library/alpine sleep 120 > /dev/null

echo "Procesos 'sleep 120' en el host:"
ps -eo user,pid,ppid,comm | grep "sleep" | grep -v grep
```

**Predicción**: ¿Bajo qué usuario corren los procesos en Docker vs Podman?

<details><summary>Respuesta</summary>

- **Docker (rootful)**: El proceso `sleep` corre como `root` porque el daemon
  de Docker ejecuta contenedores como root por defecto.
- **Podman (rootless)**: El proceso `sleep` corre como tu usuario (ej. `zavden`)
  porque Podman rootless ejecuta todo en tu user namespace.

Esta es la diferencia fundamental de seguridad: si el proceso escapa del
contenedor, en Docker tiene acceso root al host; en Podman rootless, solo
tiene los privilegios de tu usuario.

</details>

```bash
# Limpiar
docker rm -f cmp_docker > /dev/null 2>&1
podman rm -f cmp_podman > /dev/null 2>&1
```

### Ejercicio 3 — Verificar rootless

```bash
echo "=== Docker ==="
docker info 2>/dev/null | grep -i "rootless" || echo "Docker: rootful (default)"
ls -la /var/run/docker.sock 2>/dev/null | awk '{print "Socket owner:", $3, $4}'

echo ""

echo "=== Podman ==="
podman info --format '{{.Host.Security.Rootless}}'
echo "Storage: $(podman info --format '{{.Store.GraphRoot}}')"
```

**Predicción**: ¿Dónde almacena cada uno las imágenes?

<details><summary>Respuesta</summary>

- **Docker (rootful)**: `/var/lib/docker` — directorio del sistema, propiedad
  de root.
- **Podman (rootless)**: `~/.local/share/containers/storage` — dentro del home
  del usuario, sin privilegios.
- **Podman (rootful)**: `/var/lib/containers/storage` — similar a Docker pero
  en directorio diferente.

Los almacenamientos son independientes — no comparten imágenes ni volúmenes.

</details>

### Ejercicio 4 — Decidir la herramienta correcta

Para cada escenario, responder Docker o Podman y pensar en la razón:

```
1. Servidor RHEL 9 en producción con SELinux enforcing         → ?
2. MacBook de desarrollo con equipo que usa Docker              → ?
3. Pipeline CI/CD en GitHub Actions                             → ?
4. Preparación para examen RHCSA                                → ?
5. Ejecutar contenedores como servicios con systemd             → ?
6. Proyecto que necesita cache export/import para CI builds     → ?
7. Entorno con requisitos estrictos de seguridad rootless       → ?
8. Migración futura a Kubernetes                                → ?
```

<details><summary>Respuestas</summary>

1. **Podman**: Preinstalado en RHEL, SELinux integrado con `:z`/`:Z`
2. **Docker**: Docker Desktop maduro en Mac, equipo ya lo usa
3. **Docker**: GitHub Actions tiene soporte nativo para Docker
4. **Podman**: RHCSA evalúa Podman en los exámenes
5. **Podman**: Integración systemd nativa con Quadlet
6. **Docker**: Cache export/import con registries es exclusivo de BuildKit
7. **Podman**: Rootless por defecto, sin daemon con privilegios
8. **Podman**: Pods y `generate kube` facilitan la transición

Nota sobre el #6: si solo necesitas cache mounts (`--mount=type=cache`) para
builds locales, Buildah 1.24+ los soporta — ambas herramientas sirven. Docker
solo es necesario si necesitas exportar/importar cache entre máquinas via
registry.

</details>

### Ejercicio 5 — Escenario real: analizar contexto

Lee cada escenario y decide qué herramienta recomiendas:

```
ESCENARIO A:
Un equipo de 5 desarrolladores en laptops Mac, usando GitHub Actions
para CI, con una aplicación monolítica en Docker Compose.
```

**Predicción**: ¿Docker o Podman? ¿Por qué?

<details><summary>Respuesta</summary>

**Docker**. Tres razones:
- macOS: Docker Desktop es más maduro que Podman Machine
- CI: GitHub Actions tiene soporte nativo para Docker
- Equipo: ya tienen conocimiento y configuración de Docker

Cambiar a Podman no aportaría valor y generaría fricción.

</details>

```
ESCENARIO B:
Un sysadmin preparando un servidor RHEL 9 con aplicaciones
containerizadas que deben arrancar con el sistema y comunicarse
entre sí via systemd.
```

<details><summary>Respuesta</summary>

**Podman**. Tres razones:
- RHEL 9: Podman preinstalado, Docker requiere repos de terceros
- systemd: Quadlet permite gestionar contenedores como servicios nativos
- Seguridad: rootless por defecto en un servidor de producción

</details>

```
ESCENARIO C:
Un desarrollador estudiando para RHCSA y CKAD, quiere
practicar localmente con la misma herramienta del examen.
```

<details><summary>Respuesta</summary>

**Podman** para RHCSA (es lo que se evalúa). Para CKAD, la herramienta de
desarrollo local es secundaria — lo importante es saber Kubernetes (kubectl,
deployments, services). Podman además ofrece `generate kube` / `play kube` para
prototipar pods localmente.

</details>

```
ESCENARIO D:
Una startup con equipo diverso (Mac/Linux/Windows), sin
infraestructura legacy, planea migrar a Kubernetes en 6 meses.
```

<details><summary>Respuesta</summary>

**Depende del equipo**. Opciones:
- **Docker** para desarrollo diario (mejor soporte multiplataforma) + Podman
  para prototipar pods con `generate kube`
- **Podman** si el equipo está en Linux mayoritariamente, aprovechando pods
  como puente a Kubernetes

Sin infraestructura legacy, la decisión es más libre. El factor más importante
es la experiencia del equipo y las plataformas de desarrollo.

</details>

### Ejercicio 6 — El formato es estándar

```bash
# Construir una imagen con Docker
mkdir -p /tmp/oci_test && cd /tmp/oci_test

cat > Dockerfile << 'EOF'
FROM docker.io/library/alpine:latest
RUN echo "Built with Docker or Podman" > /info.txt
CMD ["cat", "/info.txt"]
EOF

docker build -t oci-test:docker .
```

**Predicción**: ¿Se puede exportar la imagen de Docker y usarla en Podman sin
reconstruir?

<details><summary>Respuesta</summary>

Sí. Las imágenes usan el formato OCI estándar, que es compatible entre Docker
y Podman. El proceso es:

```bash
docker save oci-test:docker -o /tmp/oci-test.tar
podman load -i /tmp/oci-test.tar
podman run --rm oci-test:docker
# Output: Built with Docker or Podman
```

Las capas, el manifiesto y la configuración son idénticos. Este es el punto
clave: la **imagen** es el artefacto portable, no la herramienta que la
construyó.

</details>

```bash
# También funciona construir con Podman y cargar en Docker
podman build -t oci-test:podman .
podman save oci-test:podman -o /tmp/oci-test-podman.tar
docker load -i /tmp/oci-test-podman.tar 2>/dev/null
docker run --rm oci-test:podman 2>/dev/null

# Limpiar
docker rmi oci-test:docker oci-test:podman 2>/dev/null
podman rmi oci-test:docker oci-test:podman 2>/dev/null
rm -rf /tmp/oci_test /tmp/oci-test.tar /tmp/oci-test-podman.tar
```

### Ejercicio 7 — Verificar que Kubernetes no usa Docker

```bash
# Si tienes acceso a un cluster Kubernetes (minikube, kind, etc.)
kubectl get nodes -o wide 2>/dev/null
```

**Predicción**: ¿Qué CONTAINER-RUNTIME muestra?

<details><summary>Respuesta</summary>

Desde Kubernetes 1.24, el runtime por defecto es **containerd** (no Docker ni
Podman). La columna CONTAINER-RUNTIME muestra algo como `containerd://1.7.x`.

Kubernetes eliminó el soporte para dockershim en 1.24. Ahora usa exclusivamente
CRI (Container Runtime Interface) con containerd o CRI-O. Docker y Podman son
herramientas de desarrollo — construyen imágenes y las suben a registries.
Kubernetes las descarga y ejecuta con su propio runtime.

</details>

### Ejercicio 8 — Resumen personal

Basándote en tu situación personal, responde:

```
1. ¿Qué sistema operativo usas para desarrollo?
2. ¿Tienes planes de certificación (RHCSA, CKAD)?
3. ¿Tu equipo/empresa ya usa una herramienta?
4. ¿Necesitas seguridad rootless?
5. ¿Planeas usar Kubernetes?
```

<details><summary>Guía de decisión</summary>

- Si respondiste "RHEL/Fedora" + "RHCSA" → **Podman** como principal
- Si respondiste "macOS" + "equipo usa Docker" → **Docker** como principal
- Si respondiste "Linux" + "seguridad rootless" + "Kubernetes" → **Podman**
- Si respondiste "cualquiera" + "no tengo preferencia" → **ambos** — aprende
  los dos y usa el que encaje en cada proyecto

La respuesta correcta siempre depende del contexto. Lo importante es que ya
conoces ambas herramientas y puedes trabajar con cualquiera.

</details>

---

## Resumen visual

```
┌──────────────────────────────────────────────────────────────────┐
│                    CUÁNDO USAR CADA UNO                          │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  DOCKER                          PODMAN                          │
│  ──────                          ──────                          │
│  Equipo con experiencia Docker   RHEL/Fedora/CentOS              │
│  CI/CD GitHub/GitLab             Seguridad rootless              │
│  Mac/Windows dev                 Preparación RHCSA               │
│  Cache export/import (CI)        systemd/Quadlet integration     │
│  Docker Compose completo         Migrar a Kubernetes (pods)      │
│  Docker Swarm                    Sin daemon (sin SPOF)           │
│                                                                  │
│  AMBAS HERRAMIENTAS COMPARTEN:                                   │
│  • Formato OCI estándar (imágenes 100% compatibles)              │
│  • Misma CLI para comandos básicos                               │
│  • Cache/secret/SSH mounts en builds (Buildah 1.24+)             │
│  • Funcionan en Linux                                            │
│                                                                  │
│  LO IMPORTANTE: entender contenedores OCI,                       │
│  no depender de una herramienta específica                       │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```
