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
│  │  Mantiene   │  │  localhost  │  │localhost│ │
│  │  namespaces │  │  :3000 → app│  │:80→nginx│ │
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
# CONTAINER ID  IMAGE                            STATUS   POD ID    NAMES
# def456        localhost/podman-pause:5.x.y...  Created  abc123    abc123-infra
```

> **Nota sobre la imagen infra**: En Podman 4.0+, la imagen del contenedor infra
> es construida localmente (`localhost/podman-pause:VERSION`). En versiones
> anteriores se descargaba de `k8s.gcr.io/pause:3.5`. Se puede personalizar con
> `--infra-image`.

## Crear y gestionar pods

### Crear un pod

```bash
# Pod básico
podman pod create --name webapp

# Pod con port mapping (los puertos se definen en el pod, no en los contenedores)
podman pod create --name webapp -p 8080:80

# Pod con hostname custom
podman pod create --name webapp -p 8080:80 --hostname webapp

# Pod con DNS configurado
podman pod create --name webapp --dns 8.8.8.8 --dns-search myapp.local
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
| Aislamiento de red | Red bridge entre servicios | Todos en el mismo namespace |

### Gestionar el pod completo

```bash
# Ver pods
podman pod ps

# Detalles del pod (JSON completo)
podman pod inspect webapp

# Detener todos los contenedores del pod
podman pod stop webapp

# Arrancar todos
podman pod start webapp

# Reiniciar el pod
podman pod restart webapp

# Eliminar el pod y todos sus contenedores
podman pod rm -f webapp

# Listar contenedores con su pod
podman ps --pod
```

### Ver los contenedores de un pod

```bash
podman pod inspect webapp --format '{{range .Containers}}{{.Name}} {{end}}'
# abc123-infra web app

# Para ver imagen y estado, usar ps filtrado (pod inspect no incluye campo Image)
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

### Conflictos de puertos dentro del pod

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

## Namespace sharing

Por defecto, los pods comparten **ipc, net y uts** (network, hostname e IPC).
Se puede configurar qué namespaces compartir con la opción `--share`:

```bash
# Compartir PID namespace además de los defaults
podman pod create --name shared-pid --share ipc,net,uts,pid

# Añadir contenedores
podman run -d --pod shared-pid --name web docker.io/library/nginx
podman run -d --pod shared-pid --name app docker.io/library/alpine sleep 3600

# Ver procesos de todos los contenedores (PID compartido)
podman exec app ps aux
# Muestra procesos tanto de app como de web
```

### Opciones de `--share`

La opción `--share` acepta una lista separada por comas de los namespaces a
compartir:

| Valor | Descripción |
|---|---|
| `net` | Network namespace (default) — misma IP, puertos, interfaces |
| `ipc` | IPC namespace (default) — memoria compartida, semáforos |
| `uts` | UTS namespace (default) — hostname compartido |
| `pid` | PID namespace — ver procesos de todos los contenedores |
| `cgroup` | Cgroup namespace |
| `none` | No compartir nada (desactiva incluso los defaults) |

```bash
# Defaults: ipc,net,uts
podman pod create --name default-sharing

# Añadir PID a los defaults
podman pod create --name with-pid --share ipc,net,uts,pid

# Solo compartir red (sin IPC ni hostname)
podman pod create --name net-only --share net

# Sin compartir nada (los contenedores están aislados)
podman pod create --name isolated --share none
```

> **Importante**: `--share` reemplaza los defaults por completo. Si quieres
> añadir PID al comportamiento por defecto, debes listar todos los namespaces:
> `--share ipc,net,uts,pid`. Usar `--share pid` solo compartiría PID (sin red
> compartida, lo que rompería la comunicación por localhost).

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
estructura básica es correcta. El contenedor infra **no** aparece en el YAML —
es un detalle de implementación de Podman que Kubernetes maneja internamente.

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

### Naming en `play kube`

Cuando se crea un pod desde YAML, los contenedores se nombran como
`<pod-name>-<container-name>`:

```bash
podman ps --pod
# NAMES          POD ID    POD
# myapp-web      abc123    myapp
# myapp-api      abc123    myapp
# abc123-infra   abc123    myapp
```

### Ejemplo: pod con volumen compartido (sidecar pattern)

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: webapp
spec:
  containers:
    - name: nginx
      image: docker.io/library/nginx:latest
      ports:
        - containerPort: 80
          hostPort: 8080
      volumeMounts:
        - name: html
          mountPath: /usr/share/nginx/html
    - name: sidecar
      image: docker.io/library/alpine:latest
      command: ["sh", "-c", "while true; do date > /html/index.html; sleep 10; done"]
      volumeMounts:
        - name: html
          mountPath: /html
  volumes:
    - name: html
      emptyDir: {}
```

Este patrón (sidecar que genera contenido + nginx que lo sirve) es muy común en
Kubernetes y se puede prototipar directamente con Podman.

## Cuándo usar pods vs contenedores individuales

| Caso de uso | Contenedores individuales | Pod |
|---|---|---|
| Servicios independientes | ✓ | — |
| Sidecar pattern (log, proxy) | — | ✓ |
| Servicios que comparten localhost | — | ✓ |
| Prototipado para Kubernetes | — | ✓ |
| Desarrollo con Docker Compose | ✓ | — |
| Máximo aislamiento de red | ✓ | — |
| Microservicios independientes | ✓ | — |
| Helper containers tightly coupled | — | ✓ |

Los pods son útiles cuando dos servicios están **fuertemente acoplados** y
necesitan comunicación de baja latencia (localhost). Para servicios independientes,
los contenedores con redes son más flexibles.

## Limitaciones de los pods

1. **Puertos inmutables**: no se pueden agregar puertos a un pod existente — hay
   que recrear el pod
2. **Contenedores fijos**: no se pueden mover contenedores entre pods
3. **Exclusivo de Podman**: Docker no tiene concepto de pods
4. **Sin orquestación**: a diferencia de Kubernetes, Podman no replica pods
   automáticamente ni los reinicia ante fallos (a menos que se integre con
   systemd/Quadlet)

---

## Ejercicios

### Ejercicio 1 — Pod básico con dos contenedores

```bash
# Crear pod con puerto
podman pod create --name lab_pod -p 8080:80

# Añadir nginx
podman run -d --pod lab_pod --name lab_web docker.io/library/nginx

# Añadir un contenedor que accede a nginx via localhost
podman run -d --pod lab_pod --name lab_client \
  docker.io/library/alpine sleep 3600
```

**Predicción**: ¿Qué devuelve este comando?

```bash
podman exec lab_client wget -qO- http://localhost:80 | head -3
```

<details><summary>Respuesta</summary>

Devuelve el HTML de la página de bienvenida de nginx. Aunque `lab_client` es un
contenedor Alpine separado, comparte el network namespace con `lab_web` (nginx).
Para `lab_client`, `localhost:80` apunta al nginx que está en el mismo pod.

</details>

**Predicción**: ¿Y este, desde el host?

```bash
curl -s http://localhost:8080 | head -3
```

<details><summary>Respuesta</summary>

El mismo HTML de nginx, pero accedido desde el host. El pod mapea el puerto
`8080 (host) → 80 (pod)`, y nginx escucha en el puerto 80 del pod.

</details>

```bash
# Ver el pod y sus contenedores
podman pod ps
podman ps --pod --format "{{.Names}}\t{{.Pod}}\t{{.Status}}"

# Limpiar
podman pod rm -f lab_pod
```

### Ejercicio 2 — Conflicto de puertos en un pod

```bash
podman pod create --name conflict_pod -p 8080:80

# Primer nginx: puerto 80
podman run -d --pod conflict_pod --name web1 docker.io/library/nginx
```

**Predicción**: ¿Qué pasa al ejecutar?

```bash
podman run -d --pod conflict_pod --name web2 docker.io/library/nginx
```

<details><summary>Respuesta</summary>

Falla con un error tipo `bind: address already in use`. Ambos contenedores nginx
intentan escuchar en el puerto 80, pero comparten el mismo network namespace.
Es como intentar ejecutar dos procesos en el mismo puerto en tu máquina — el
segundo no puede hacer bind.

</details>

```bash
# Limpiar
podman pod rm -f conflict_pod
```

### Ejercicio 3 — Namespace sharing con `--share`

```bash
# Crear pod con PID namespace compartido (además de los defaults)
podman pod create --name pid_pod --share ipc,net,uts,pid -p 8080:80

# Añadir nginx y Alpine
podman run -d --pod pid_pod --name pid_web docker.io/library/nginx
podman run -d --pod pid_pod --name pid_app \
  docker.io/library/alpine sleep 3600
```

**Predicción**: ¿Qué muestra este comando?

```bash
podman exec pid_app ps aux
```

<details><summary>Respuesta</summary>

Muestra los procesos de **todos** los contenedores del pod: el `sleep 3600` de
`pid_app`, los procesos de nginx de `pid_web`, y el proceso `pause` del
contenedor infra. Esto es porque comparten PID namespace — cada contenedor ve
los procesos de los demás.

</details>

**Predicción**: ¿Y si el pod se hubiera creado sin `pid` en `--share` (solo los
defaults)?

<details><summary>Respuesta</summary>

`ps aux` en `pid_app` solo mostraría sus propios procesos (`sleep 3600`). Sin
compartir PID namespace, cada contenedor tiene su propio espacio de PIDs aislado
— ese es el comportamiento por defecto.

</details>

```bash
# Limpiar
podman pod rm -f pid_pod
```

### Ejercicio 4 — Ver el contenedor infra

```bash
# Crear un pod vacío
podman pod create --name infra_test
```

**Predicción**: ¿Cuántos contenedores aparecen? ¿Qué imagen usa?

```bash
podman ps -a --pod --filter pod=infra_test
```

<details><summary>Respuesta</summary>

Aparece **1 contenedor** (el infra/pause). En Podman 4.0+, la imagen es
`localhost/podman-pause:VERSION` (construida localmente). En versiones anteriores,
era `k8s.gcr.io/pause:3.5`. Su nombre termina en `-infra`.

</details>

```bash
# Añadir un contenedor
podman run -d --pod infra_test --name infra_app \
  docker.io/library/alpine sleep 3600
```

**Predicción**: ¿Cuántos contenedores hay ahora?

```bash
podman ps -a --pod --filter pod=infra_test
```

<details><summary>Respuesta</summary>

**2 contenedores**: el infra (`*-infra`) y `infra_app`. El pod siempre mantiene
su contenedor infra para preservar los namespaces, incluso si no hay otros
contenedores corriendo.

</details>

```bash
# Ver detalles completos del pod
podman pod inspect infra_test

# Limpiar
podman pod rm -f infra_test
```

### Ejercicio 5 — Generar YAML de Kubernetes

```bash
podman pod create --name kube_test -p 8080:80
podman run -d --pod kube_test --name kube_web docker.io/library/nginx
podman run -d --pod kube_test --name kube_app \
  docker.io/library/alpine sleep 3600

# Generar YAML
podman generate kube kube_test
```

**Predicción**: ¿Qué `kind` tiene el YAML generado? ¿Aparece el contenedor
infra en la lista de containers?

<details><summary>Respuesta</summary>

El `kind` es `Pod` (no Deployment ni Service). El contenedor infra **no**
aparece en la lista de `containers` del YAML — es un detalle de implementación
de Podman que Kubernetes maneja internamente. Solo aparecen `kube_web` y
`kube_app`.

</details>

```bash
# Guardar a archivo
podman generate kube kube_test > /tmp/kube_test.yaml

# Limpiar el pod original
podman pod rm -f kube_test
```

**Predicción**: ¿Se puede recrear el pod desde ese YAML?

```bash
podman play kube /tmp/kube_test.yaml
podman pod ps
```

<details><summary>Respuesta</summary>

Sí. `podman play kube` lee el YAML y recrea el pod con todos sus contenedores.
Los nombres de los contenedores serán `kube_test-kube_web` y
`kube_test-kube_app` (formato `<pod>-<container>`).

</details>

```bash
# Limpiar
podman play kube --down /tmp/kube_test.yaml
rm /tmp/kube_test.yaml
```

### Ejercicio 6 — Crear pod desde YAML (`labs/pod.yaml`)

El archivo `labs/pod.yaml` contiene:

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: lab-kube-pod
spec:
  containers:
    - name: web
      image: docker.io/library/nginx:latest
      ports:
        - containerPort: 80
          hostPort: 8888
    - name: sidecar
      image: docker.io/library/alpine:latest
      command: ["sleep", "3600"]
```

```bash
podman play kube labs/pod.yaml
```

**Predicción**: ¿Cuál es el nombre del contenedor sidecar? ¿En qué puerto del
host responde nginx?

<details><summary>Respuesta</summary>

- El contenedor sidecar se llama `lab-kube-pod-sidecar` (formato
  `<pod>-<container>`)
- Nginx responde en el puerto **8888** del host (`hostPort: 8888`)

Verificación:

```bash
podman ps --pod --filter pod=lab-kube-pod
curl -s http://localhost:8888 | head -3
```

</details>

```bash
# Verificar comunicación interna
podman exec lab-kube-pod-sidecar wget -qO- http://localhost:80 | head -3

# Limpiar
podman play kube --down labs/pod.yaml
```

### Ejercicio 7 — Ciclo de vida del pod

```bash
podman pod create --name lifecycle_pod -p 8080:80
podman run -d --pod lifecycle_pod --name lc_web docker.io/library/nginx

# Detener el pod
podman pod stop lifecycle_pod
```

**Predicción**: ¿Qué muestra `podman pod ps`? ¿Y `podman ps -a --pod`?

<details><summary>Respuesta</summary>

`podman pod ps` muestra el pod con STATUS `Exited` (o `Stopped`).
`podman ps -a --pod` muestra los contenedores (`lc_web` e infra) con estado
`Exited`. Los contenedores no se eliminan al detener el pod — solo se paran.

</details>

```bash
# Arrancar de nuevo
podman pod start lifecycle_pod
curl -s http://localhost:8080 | head -3
```

**Predicción**: ¿Responde nginx después del restart?

<details><summary>Respuesta</summary>

Sí. `podman pod start` arranca todos los contenedores del pod. Nginx vuelve a
escuchar en el puerto 80 del pod, mapeado al 8080 del host. El contenedor no se
recrea — se reanuda el mismo.

</details>

```bash
# Limpiar
podman pod rm -f lifecycle_pod
```

### Ejercicio 8 — Pod con volumen compartido entre contenedores

```bash
podman pod create --name vol_pod -p 8080:80

# Crear un volumen
podman volume create shared_html

# Nginx usando el volumen (solo lectura)
podman run -d --pod vol_pod --name vol_web \
  -v shared_html:/usr/share/nginx/html:ro \
  docker.io/library/nginx

# Sidecar que escribe en el volumen
podman run -d --pod vol_pod --name vol_writer \
  -v shared_html:/html \
  docker.io/library/alpine \
  sh -c 'while true; do date > /html/index.html; sleep 5; done'
```

**Predicción**: ¿Qué devuelve `curl http://localhost:8080` la primera vez?
¿Y 10 segundos después?

<details><summary>Respuesta</summary>

La primera vez devuelve la fecha/hora actual (escrita por el sidecar en
`/html/index.html`). 10 segundos después devuelve una fecha más reciente. El
sidecar escribe la fecha cada 5 segundos en el volumen compartido, y nginx lo
sirve.

Este es el patrón **sidecar** clásico: un contenedor genera contenido y otro lo
sirve. Nota: nginx tiene el volumen como `:ro` (solo lectura), pero el sidecar
lo tiene como lectura-escritura. Los permisos de montaje son por contenedor, no
por volumen.

</details>

```bash
# Verificar
curl -s http://localhost:8080
sleep 6
curl -s http://localhost:8080

# Limpiar
podman pod rm -f vol_pod
podman volume rm shared_html
```

---

## Comandos de pods resumidos

```bash
# Crear
podman pod create --name <name> [-p host:container] [--share namespaces]

# Gestionar
podman pod ps                    # listar pods
podman pod inspect <pod>         # detalles JSON
podman pod start <pod>           # arrancar todos los contenedores
podman pod stop <pod>            # detener todos los contenedores
podman pod restart <pod>         # reiniciar el pod
podman pod rm -f <pod>           # eliminar pod y sus contenedores
podman pod top <pod>             # ver procesos de todos los contenedores

# Contenedores
podman run -d --pod <pod> --name <name> <image>
podman ps --pod [--filter pod=<pod>]

# Kubernetes
podman generate kube <pod>       # generar YAML desde pod existente
podman play kube <file>          # crear pod desde YAML
podman play kube --down <file>   # eliminar pod creado desde YAML
```
