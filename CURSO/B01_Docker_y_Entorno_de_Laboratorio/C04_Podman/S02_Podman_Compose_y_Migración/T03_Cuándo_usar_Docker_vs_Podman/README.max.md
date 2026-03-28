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

### Se depende de BuildKit

BuildKit ofrece optimizaciones de build que Buildah (el backend de Podman) no
tiene:

| Feature | BuildKit (Docker) | Buildah (Podman) |
|---|---|---|
| Cache mounts (`--mount=type=cache`) | Sí | No |
| Secret mounts (`--mount=type=secret`) | Sí | No |
| SSH forwarding (`--mount=type=ssh`) | Sí | No |
| Cache export/import (registry) | Sí | No |
| Paralelización de stages | Sí | Parcial |

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
- Configurar contenedores como servicios con systemd
- Rootless containers
- Persistencia con `loginctl enable-linger`
- SELinux con `:z` y `:Z`

### La seguridad es prioridad

Podman es más seguro por diseño:

```
Docker rootful:
  Escape del contenedor → root en el host ← CRÍTICO

Podman rootless:
  Escape del contenedor → tu usuario sin privilegios ← Controlado

Docker rootless:
  Similar a Podman pero requiere configuración extra
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

Docker usa restart policies internas que dependen del daemon.

### Se planea migrar a Kubernetes

Los pods de Podman y la capacidad de generar/consumir YAML de Kubernetes hacen
que Podman sea un puente natural hacia Kubernetes:

```bash
# Desarrollo local con Podman
podman pod create --name myapp -p 8080:80
podman run --pod myapp --name web nginx
podman run --pod myapp --name api myapi

# Generar manifiesto de Kubernetes
podman generate kube myapp > deployment.yaml

# Desplegar en Kubernetes
kubectl apply -f deployment.yaml
```

Con Docker, este paso requiere escribir los manifiestos manualmente o usar
herramientas de conversión.

## Comparación directa

| Criterio | Docker | Podman |
|---|---|---|
| Adopción en la industria | Mayoritaria | Creciente |
| Curva de aprendizaje | Estándar de facto | Compatible (misma CLI) |
| Seguridad por defecto | Root daemon | Rootless, sin daemon |
| Compose | Nativo, completo | Via podman-compose o socket |
| Build engine | BuildKit (avanzado) | Buildah (estándar) |
| macOS/Windows | Docker Desktop (maduro) | Podman Machine (en desarrollo) |
| RHEL/Fedora | Repo externo | Preinstalado |
| Certificaciones | — | RHCSA requiere Podman |
| systemd | No nativo | Nativo (generate, Quadlet) |
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
y en cualquier runtime OCI. El formato es estándar.

## Casos de decisión por escenario

| Escenario | Herramienta recomendada | Razón |
|---|---|---|
| Mac/Windows para desarrollo | Docker | Soporte maduro, VM integrada |
| Linux laptop personal | Podman | Rootless por defecto, seguro |
| RHEL/Fedora en producción | Podman | Preinstalado, soporte oficial |
| Ubuntu/Debian en producción | Docker o Podman | Ambos funcionan bien |
| Equipo ya usa Docker | Docker | Evitar fricción |
| Examen RHCSA | Podman | Es lo que se prueba |
| Seguridad máxima | Podman | Rootless nativo, sin daemon |
| CI/CD en GitHub/GitLab | Docker | Soporte nativo mejor |
| Migrar a Kubernetes | Podman | Pods + generate kube |
| BuildKit cache mounts | Docker | Feature no disponible en Podman |
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

## Resumen visual

```
┌─────────────────────────────────────────────────────────────────┐
│                    CUÁNDO USAR CADA UNO                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  DOCKER                    PODMAN                                │
│  ──────                    ──────                                │
│  Equipo con experiencia    Equipo nuevo                          │
│  CI/CD GitHub/GitLab      RHEL/Fedora                          │
│  Mac/Windows dev           Seguridad máxima                      │
│  BuildKit features        Preparación RHCSA                     │
│  Docker Compose completo   systemd integration                    │
│  Docker Swarm             Migrar a K8s                           │
│                                                                  │
│  Ambas:                                                  │
│  - Formato OCI estándar (imágenes compatibles)               │
│  - Misma CLI para comandos básicos                          │
│  - Funcionan en Linux                                         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1 — Tabla comparativa práctica

Ejecutar las mismas operaciones con Docker y Podman, documentar las diferencias:

```bash
echo "=== Pull ==="
echo "Docker:" && time docker pull alpine:latest 2>&1 | tail -1
echo "Podman:" && time podman pull docker.io/library/alpine:latest 2>&1 | tail -1

echo ""
echo "=== Run ==="
echo "Docker:" && docker run --rm alpine echo "Docker OK"
echo "Podman:" && podman run --rm docker.io/library/alpine echo "Podman OK"

echo ""
echo "=== Build info ==="
echo "Docker:" && docker info 2>/dev/null | grep -E '(Storage|rootless|Server Version)' | head -3
echo "Podman:" && podman info | grep -E '(graphDriverName|rootless|version)' | head -3

echo ""
echo "=== Proceso del contenedor ==="
docker run -d --name cmp_docker alpine sleep 60 > /dev/null
podman run -d --name cmp_podman docker.io/library/alpine sleep 60 > /dev/null

echo "Docker (proceso en host):"
ps aux | grep "sleep 60" | grep -v grep | awk '{print $1, $2}' | head -2
echo ""

docker rm -f cmp_docker > /dev/null
podman rm -f cmp_podman > /dev/null
```

### Ejercicio 2 — Verificar rootless

```bash
# Docker: ¿rootful o rootless?
echo "=== Docker ==="
docker info 2>/dev/null | grep -i "rootless" || echo "Docker: rootful (default)"
ls -la /var/run/docker.sock 2>/dev/null | awk '{print "Socket owner:", $3, $4}'

echo ""

# Podman: ¿rootful o rootless?
echo "=== Podman ==="
podman info 2>/dev/null | grep -i "rootless"
echo "Storage: $(podman info 2>/dev/null | grep graphRoot | awk '{print $2}')"
```

### Ejercicio 3 — Decidir la herramienta correcta

Para cada escenario, responder Docker o Podman y explicar por qué:

```
1. Servidor RHEL 9 en producción con SELinux enforcing    → ?
2. MacBook de desarrollo con equipo que usa Docker         → ?
3. Pipeline CI/CD en GitHub Actions                        → ?
4. Preparación para examen RHCSA                           → ?
5. Ejecutar contenedores como servicios con systemd        → ?
6. Proyecto que necesita BuildKit cache mounts             → ?
7. Entorno con requisitos estrictos de seguridad rootless  → ?
8. Migración futura a Kubernetes                           → ?
```

Soluciones:
1. **Podman**: Preinstalado en RHEL, SELinux integrado
2. **Docker**: Docker Desktop maduro en Mac, equipo usa Docker
3. **Docker**: GitHub Actions tiene soporte nativo para Docker
4. **Podman**: RHCSA usa Podman en los exámenes
5. **Podman**: systemd integration nativa con generate
6. **Docker**: BuildKit cache mounts no disponibles en Buildah
7. **Podman**: Rootless por defecto, sin daemon
8. **Podman**: Pods y generate kube facilitan la migración

### Ejercicio 4 — Escenario real

Lee cada escenario y decide qué herramienta recomiendas:

```
ESCENARIO A:
Un equipo de 5 desarrolladores trabajando en laptops Mac,
usando GitHub Actions para CI, con una aplicación
monolítica en Docker Compose.
→ Recomendación: Docker (equipo familiarizado, CI ya configurado)

ESCENARIO B:
Un sysadmin preparando un servidor RHEL 9, con
aplicaciones containerizadas que deben arrancar con
el servidor y comunicarse entre sí via systemd.
→ Recomendación: Podman (sistema RHEL, systemd integration)

ESCENARIO C:
Un desarrollador estudiando para RHCSA y CKAD, quiere
practicar localmente con la misma herramienta del examen.
→ Recomendación: Podman (prepara para RHCSA, similar para CKAD)

ESCENARIO D:
Una startup con equipo diverso (Mac/Linux/Windows), sin
infraestructura legacy, planea migrar a Kubernetes en 6 meses.
→ Recomendación: Podman (pods + generate kube para preparación)
```
