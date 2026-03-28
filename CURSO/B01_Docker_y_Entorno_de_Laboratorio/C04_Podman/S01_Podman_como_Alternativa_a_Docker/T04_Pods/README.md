# T04 — Pods

## ¿Qué es un Pod?

Un pod es una **agrupación de uno o más contenedores** que comparten recursos de
red y (opcionalmente) PID namespace. Es un concepto tomado directamente de
**Kubernetes** y es exclusivo de Podman — Docker no tiene equivalente.

Los contenedores dentro de un pod:
- Comparten la **misma IP y los mismos puertos** (se ven como `localhost`)
- Comparten el **hostname**
- Se gestionan como una unidad (start, stop, rm del pod afecta a todos)

```
┌─────────────────────────────────────────────────┐
│                    Pod: webapp                   │
│                                                 │
│  IP: 10.88.0.5                                  │
│  Puertos: 8080 (mapeado al host)                │
│                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │
│  │   infra     │  │   nginx     │  │   app   │ │
│  │  (pause)    │  │   :80       │  │  :3000  │ │
│  │             │  │             │  │         │ │
│  │  Mantiene   │  │  localhost  │  │ localhost│ │
│  │  namespaces │  │  :3000 → app│  │ :80→nginx│ │
│  └─────────────┘  └─────────────┘  └─────────┘ │
│                                                 │
└─────────────────────────────────────────────────┘
```

## El contenedor infra

Cada pod tiene automáticamente un contenedor **infra** (también llamado "pause
container"). Este contenedor no hace nada visible — su único propósito es
mantener los namespaces de red activos para que los demás contenedores los
compartan:

```bash
podman pod create --name test_pod
podman pod ps
# POD ID    NAME       STATUS   CONTAINERS
# abc123    test_pod   Created  1          ← el infra container

podman ps -a --pod
# CONTAINER ID  IMAGE                 STATUS   POD ID    NAMES
# def456        k8s.gcr.io/pause:3.5  Created  abc123    abc123-infra
```

## Crear y gestionar pods

### Crear un pod

```bash
# Pod básico
podman pod create --name webapp

# Pod con port mapping (los puertos se definen en el pod, no en los contenedores)
podman pod create --name webapp -p 8080:80

# Pod con hostname
podman pod create --name webapp -p 8080:80 --hostname webapp
```

### Añadir contenedores al pod

```bash
# Nginx sirviendo en el puerto 80 del pod
podman run -d --pod webapp --name web docker.io/library/nginx

# Aplicación en el puerto 3000 del pod
podman run -d --pod webapp --name app docker.io/library/node:20 \
  sh -c 'npx http-server -p 3000'
```

### Comunicación interna via localhost

Dentro del pod, los contenedores se comunican por `localhost` — **no necesitan
red ni DNS**:

```bash
# Desde nginx, la app está en localhost:3000
podman exec web curl -s http://localhost:3000

# Desde la app, nginx está en localhost:80
podman exec app curl -s http://localhost:80
```

Esto es fundamentalmente diferente a Docker Compose, donde los servicios se
comunican por **nombre de servicio** a través de una red bridge:

| | Docker Compose | Podman Pod |
|---|---|---|
| Comunicación | `http://servicio:puerto` | `http://localhost:puerto` |
| Mecanismo | DNS en red bridge | Mismo network namespace |
| Overhead | Red virtual, NAT | Ninguno (mismo stack de red) |
| Puertos | Cada servicio tiene su IP | Todos comparten IP |

### Gestionar el pod completo

```bash
# Ver pods
podman pod ps

# Detalles del pod
podman pod inspect webapp

# Detener todos los contenedores del pod
podman pod stop webapp

# Arrancar todos
podman pod start webapp

# Eliminar el pod y todos sus contenedores
podman pod rm -f webapp

# Listar contenedores con su pod
podman ps --pod
```

### Ver los contenedores de un pod

```bash
podman pod inspect webapp --format '{{range .Containers}}{{.Name}} {{end}}'
# abc123-infra web app

# O con ps filtrado
podman ps --filter pod=webapp --format "{{.Names}}\t{{.Image}}\t{{.Status}}"
# web    docker.io/library/nginx    Up 5 minutes
# app    docker.io/library/node:20  Up 5 minutes
```

## Puertos: a nivel de pod, no de contenedor

Los puertos se mapean al **pod**, no a los contenedores individuales. Todos los
contenedores del pod comparten los mismos puertos:

```bash
# Correcto: definir puertos en el pod
podman pod create --name web_pod -p 8080:80 -p 3000:3000

# Incorrecto: no se pueden mapear puertos en contenedores dentro de un pod
podman run -d --pod web_pod -p 9090:80 docker.io/library/nginx
# Error: cannot set port mappings on a container inside a pod
```

Si necesitas exponer múltiples puertos, defínelos todos al crear el pod.

## Conflictos de puertos dentro del pod

Como todos los contenedores comparten el mismo network namespace, **no pueden
usar el mismo puerto**:

```bash
podman pod create --name conflict_pod -p 8080:80

# Primer contenedor en puerto 80: OK
podman run -d --pod conflict_pod --name web1 docker.io/library/nginx

# Segundo contenedor en puerto 80: FALLA
podman run -d --pod conflict_pod --name web2 docker.io/library/nginx
# Error: bind: address already in use — el puerto 80 ya está ocupado por web1
```

Cada servicio dentro del pod debe usar un **puerto diferente**.

## Generar YAML de Kubernetes

Una de las funcionalidades más potentes de los pods es la capacidad de generar
YAML de Kubernetes directamente:

```bash
# Crear un pod con contenedores
podman pod create --name myapp -p 8080:80
podman run -d --pod myapp --name web docker.io/library/nginx
podman run -d --pod myapp --name api docker.io/library/alpine sleep 3600

# Generar YAML de Kubernetes
podman generate kube myapp > myapp.yaml
```

YAML generado (simplificado):

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: myapp
spec:
  containers:
    - name: web
      image: docker.io/library/nginx:latest
      ports:
        - containerPort: 80
          hostPort: 8080
    - name: api
      image: docker.io/library/alpine:latest
      command:
        - sleep
        - "3600"
```

Este YAML puede usarse como punto de partida para deployments en Kubernetes.
Necesitará ajustes para producción (recursos, probes, secrets), pero la
estructura básica es correcta.

## Crear pods desde YAML de Kubernetes

La operación inversa: crear pods de Podman a partir de YAML de Kubernetes:

```bash
# Crear desde YAML
podman play kube myapp.yaml

# Ver el pod creado
podman pod ps
podman ps --pod

# Detener y eliminar
podman play kube --down myapp.yaml
```

`podman play kube` soporta un subconjunto de la especificación de Kubernetes:
Pods, Deployments, Services (parcial), ConfigMaps, PersistentVolumeClaims.

## Cuándo usar pods vs contenedores individuales

| Caso de uso | Contenedores individuales | Pod |
|---|---|---|
| Servicios independientes | ✓ | — |
| Sidecar pattern (log, proxy) | — | ✓ |
| Servicios que comparten localhost | — | ✓ |
| Prototipado para Kubernetes | — | ✓ |
| Desarrollo con Docker Compose | ✓ | — |
| Máximo aislamiento de red | ✓ | — |

Los pods son útiles cuando dos servicios están **fuertemente acoplados** y
necesitan comunicación de baja latencia (localhost). Para servicios independientes,
los contenedores con redes son más flexibles.

---

## Ejercicios

### Ejercicio 1 — Pod básico con dos contenedores

```bash
# Crear pod con puerto
podman pod create --name lab_pod -p 8080:80

# Añadir nginx
podman run -d --pod lab_pod --name lab_web docker.io/library/nginx

# Añadir un contenedor que accede a nginx via localhost
podman run -d --pod lab_pod --name lab_client docker.io/library/alpine sleep 3600

# Verificar comunicación interna por localhost
podman exec lab_client wget -qO- http://localhost:80 | head -3
# ¡Responde nginx! — mismo network namespace

# Acceder desde el host
curl -s http://localhost:8080 | head -3

# Ver el pod
podman pod ps
podman ps --pod --format "{{.Names}}\t{{.Pod}}\t{{.Status}}"

# Limpiar
podman pod rm -f lab_pod
```

### Ejercicio 2 — Generar YAML de Kubernetes

```bash
podman pod create --name kube_test -p 8080:80
podman run -d --pod kube_test --name kube_web docker.io/library/nginx
podman run -d --pod kube_test --name kube_app docker.io/library/alpine sleep 3600

# Generar YAML
podman generate kube kube_test
# Muestra el YAML en stdout — revisarlo

# Guardar a archivo
podman generate kube kube_test > /tmp/kube_test.yaml
cat /tmp/kube_test.yaml

# Limpiar
podman pod rm -f kube_test
```

### Ejercicio 3 — Crear pod desde YAML

```bash
cat > /tmp/pod_from_yaml.yaml << 'EOF'
apiVersion: v1
kind: Pod
metadata:
  name: yaml-pod
spec:
  containers:
    - name: web
      image: docker.io/library/nginx:latest
      ports:
        - containerPort: 80
          hostPort: 8080
    - name: sidecar
      image: docker.io/library/alpine:latest
      command: ["sleep", "3600"]
EOF

# Crear pod desde YAML
podman play kube /tmp/pod_from_yaml.yaml

# Verificar
podman pod ps
podman exec yaml-pod-sidecar wget -qO- http://localhost:80 | head -3

# Limpiar
podman play kube --down /tmp/pod_from_yaml.yaml
rm /tmp/pod_from_yaml.yaml
```
