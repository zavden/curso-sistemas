# Lab — Pods

## Objetivo

Laboratorio práctico para entender los pods de Podman: un concepto
tomado de Kubernetes que agrupa contenedores que comparten red y se
gestionan como unidad. Demostrar comunicación por localhost, puertos
a nivel de pod, y la generación/consumo de YAML de Kubernetes.

## Prerequisitos

- Podman instalado (`podman --version`)
- Imágenes descargadas: `podman pull docker.io/library/nginx:latest && podman pull docker.io/library/alpine:latest`
- Estar en el directorio `labs/` de este tópico

## Archivos del laboratorio

| Archivo | Propósito |
|---|---|
| `pod.yaml` | YAML de Kubernetes para crear pod con `play kube` |

---

## Parte 1 — Crear un pod

### Objetivo

Crear un pod con múltiples contenedores y entender el contenedor
infra que mantiene los namespaces compartidos.

### Paso 1.1: Crear el pod

```bash
podman pod create --name lab-pod -p 8888:80
```

El pod se crea con un puerto mapeado. Los puertos se definen en el
**pod**, no en los contenedores individuales.

### Paso 1.2: Verificar el contenedor infra

```bash
podman pod ps
```

Salida esperada:

```
POD ID    NAME      STATUS    CREATED        INFRA ID   # OF CONTAINERS
<id>      lab-pod   Created   seconds ago    <id>       1
```

El pod tiene 1 contenedor: el **infra** (pause container). Su único
propósito es mantener los namespaces de red activos.

```bash
podman ps -a --pod --filter pod=lab-pod
```

El contenedor infra aparece con imagen `k8s.gcr.io/pause` o similar.

### Paso 1.3: Añadir contenedores al pod

```bash
podman run -d --pod lab-pod --name lab-web docker.io/library/nginx
```

```bash
podman run -d --pod lab-pod --name lab-client docker.io/library/alpine sleep 3600
```

### Paso 1.4: Verificar

```bash
podman pod ps
```

Ahora muestra 3 contenedores: infra + web + client.

```bash
podman ps --pod --filter pod=lab-pod --format "{{.Names}}\t{{.Image}}\t{{.Status}}"
```

### Paso 1.5: Acceder desde el host

```bash
curl -s http://localhost:8888 | head -3
```

El puerto 8888 del host está mapeado al 80 del pod, donde nginx
escucha.

---

## Parte 2 — Comunicación por localhost

### Objetivo

Demostrar que los contenedores dentro de un pod comparten el mismo
network namespace y se comunican por `localhost`, sin necesidad de
DNS ni redes bridge.

### Paso 2.1: Desde client, acceder a nginx por localhost

```bash
podman exec lab-client wget -qO- http://localhost:80 | head -3
```

Salida esperada:

```html
<!DOCTYPE html>
<html>
<head>
```

`lab-client` accede a nginx en `localhost:80`. No necesita resolver
un nombre de servicio — ambos comparten la misma interfaz de red.

### Paso 2.2: Verificar mismo hostname

```bash
podman exec lab-web hostname
podman exec lab-client hostname
```

Ambos muestran el **mismo hostname**. Comparten el network namespace
del pod.

### Paso 2.3: Verificar misma IP

```bash
podman exec lab-web cat /etc/hosts | grep -v "^#"
podman exec lab-client cat /etc/hosts | grep -v "^#"
```

Ambos ven las mismas entradas en `/etc/hosts` — misma IP.

### Paso 2.4: Comparar con Docker Compose

En Docker Compose, los servicios se comunican por **nombre de
servicio** a través de DNS en una red bridge:

```
Docker Compose:  http://servicio:puerto  (DNS, red bridge)
Podman Pod:      http://localhost:puerto  (mismo network namespace)
```

La comunicación dentro del pod es más rápida (sin overhead de red)
pero los contenedores no pueden usar el mismo puerto.

---

## Parte 3 — Puertos y conflictos

### Objetivo

Demostrar que los puertos se definen a nivel de pod y que dos
contenedores no pueden usar el mismo puerto dentro del mismo pod.

### Paso 3.1: Los puertos pertenecen al pod

Antes de ejecutar, responde mentalmente:

- ¿Se puede mapear un puerto en un contenedor dentro de un pod?
- ¿Qué pasa si dos contenedores usan el mismo puerto?

Intenta predecir antes de continuar al siguiente paso.

```bash
podman run --pod lab-pod -p 9999:80 docker.io/library/alpine echo "test" 2>&1
```

Salida esperada:

```
Error: cannot set port mappings on a container in a pod
```

Los puertos se definen al **crear el pod**, no en los contenedores
individuales.

### Paso 3.2: Conflicto de puertos

```bash
podman run -d --pod lab-pod --name lab-web2 docker.io/library/nginx 2>&1
```

Salida esperada: error. El puerto 80 ya está ocupado por `lab-web`.
Como comparten el mismo network namespace, no pueden escuchar en el
mismo puerto.

### Paso 3.3: Gestionar el pod como unidad

```bash
podman pod stop lab-pod
podman pod ps
```

`pod stop` detiene **todos** los contenedores del pod.

```bash
podman pod start lab-pod
podman pod ps
```

`pod start` arranca todos. El pod se gestiona como una unidad.

### Paso 3.4: Limpiar

```bash
podman pod rm -f lab-pod
```

`pod rm -f` elimina el pod y **todos** sus contenedores.

---

## Parte 4 — YAML de Kubernetes

### Objetivo

Demostrar que Podman puede generar YAML de Kubernetes a partir de pods
existentes, y crear pods a partir de YAML de Kubernetes.

### Paso 4.1: Crear un pod para exportar

```bash
podman pod create --name lab-export -p 8888:80
podman run -d --pod lab-export --name export-web docker.io/library/nginx
podman run -d --pod lab-export --name export-sidecar docker.io/library/alpine sleep 3600
```

### Paso 4.2: Generar YAML de Kubernetes

```bash
podman generate kube lab-export
```

Salida: un YAML de Kubernetes con `apiVersion: v1`, `kind: Pod`,
y la definición de los dos contenedores. Este YAML puede usarse
como punto de partida para deployments en Kubernetes.

### Paso 4.3: Guardar a archivo

```bash
podman generate kube lab-export > /tmp/lab-export.yaml
cat /tmp/lab-export.yaml
```

### Paso 4.4: Limpiar el pod original

```bash
podman pod rm -f lab-export
```

### Paso 4.5: Crear pod desde YAML

```bash
cat pod.yaml
```

Un YAML de Kubernetes predefinido con nginx + sidecar.

```bash
podman play kube pod.yaml
```

### Paso 4.6: Verificar

```bash
podman pod ps
podman ps --pod
```

```bash
curl -s http://localhost:8888 | head -3
```

```bash
podman exec lab-kube-pod-sidecar wget -qO- http://localhost:80 | head -3
```

El sidecar puede acceder a nginx por localhost — mismo comportamiento
que un pod creado manualmente.

### Paso 4.7: Limpiar

```bash
podman play kube --down pod.yaml
rm -f /tmp/lab-export.yaml
```

---

## Limpieza final

```bash
# Eliminar recursos del lab (si alguno quedó)
podman pod rm -f lab-pod lab-export lab-kube-pod 2>/dev/null
rm -f /tmp/lab-export.yaml
```

---

## Conceptos reforzados

1. Un pod es una agrupación de contenedores que comparten **network
   namespace**, IP, puertos y hostname. Es un concepto de Kubernetes
   implementado nativamente en Podman.

2. Los contenedores dentro de un pod se comunican por **localhost**,
   sin necesidad de DNS ni redes bridge. Esto es diferente a Docker
   Compose, donde los servicios usan nombres de servicio.

3. Los puertos se mapean al **pod**, no a los contenedores individuales.
   Dos contenedores dentro del mismo pod **no pueden usar el mismo
   puerto**.

4. `podman generate kube` exporta pods a YAML de Kubernetes.
   `podman play kube` crea pods desde YAML de Kubernetes. Esto
   facilita la transición de desarrollo local a Kubernetes.

5. Cada pod tiene un contenedor **infra** (pause) que mantiene los
   namespaces activos. Los demás contenedores se unen a estos
   namespaces compartidos.
