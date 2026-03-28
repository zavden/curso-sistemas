# Lab — Rootless containers

## Objetivo

Entender la diferencia entre Docker rootful y rootless, observar como los
procesos de contenedores aparecen en el host, explorar el concepto de user
namespaces, y comparar con Podman rootless como alternativa.

**Nota importante**: Docker rootless requiere configuracion especifica del
sistema que puede no estar disponible en todos los entornos. Las partes que
dependen de esta configuracion incluyen las salidas esperadas para que el
analisis sea posible incluso sin ejecutar los comandos.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull alpine:latest`
- Estar en el directorio `labs/` de este topico
- Opcional: Podman instalado para la Parte 4

---

## Parte 1 — Detectar el modo de Docker

### Objetivo

Determinar si Docker esta corriendo en modo rootful (por defecto) o rootless,
e inspeccionar los indicadores clave de cada modo.

### Paso 1.1: Verificar el modo de Docker

```bash
docker info 2>/dev/null | grep -i rootless
```

Salida esperada en modo **rootful** (el mas comun):

```
(sin salida o "rootless: false")
```

Si muestra `rootless: true`, tu Docker esta en modo rootless. En ese caso,
los resultados de algunos pasos seran diferentes a los documentados aqui.

### Paso 1.2: Verificar el directorio de datos

```bash
docker info --format '{{.DockerRootDir}}'
```

Salida esperada en modo rootful:

```
/var/lib/docker
```

En modo rootless, la salida seria algo como `/home/tu_usuario/.local/share/docker`.
La ubicacion del directorio de datos indica quien controla el daemon.

### Paso 1.3: Inspeccionar el socket de Docker

```bash
ls -la /var/run/docker.sock
```

Salida esperada en modo rootful:

```
srw-rw---- 1 root docker ... /var/run/docker.sock
```

El socket pertenece a `root:docker`. Cualquier proceso que pertenezca al
grupo `docker` puede comunicarse con el daemon — y el daemon corre como
root. Por eso, pertenecer al grupo `docker` es **equivalente a tener acceso
root** en el host.

### Paso 1.4: Verificar el usuario del daemon

```bash
ps aux | grep dockerd | grep -v grep
```

Salida esperada en modo rootful (aproximada):

```
root  ... dockerd ...
```

En modo rootless, el proceso `dockerd` correria bajo tu usuario (UID 1000
o similar), no como root.

### Paso 1.5: Resumen del modo detectado

Antes de continuar, confirma mentalmente:

- ?Docker corre en modo rootful o rootless?
- ?Que implicaciones de seguridad tiene que el daemon corra como root?
- Si un contenedor escapa del aislamiento, ?con que UID operaria en el host?

Intenta responder antes de continuar al siguiente paso.

---

## Parte 2 — Propiedad de procesos en el host

### Objetivo

Verificar que en modo rootful, los procesos de los contenedores pertenecen
a root en el host, y entender las implicaciones de seguridad.

### Paso 2.1: Lanzar un contenedor con proceso de larga duracion

```bash
docker run -d --name rootless-check alpine sleep 300
```

### Paso 2.2: Predecir la propiedad del proceso

Antes de ejecutar el siguiente comando, responde mentalmente:

- El proceso `sleep 300` corre dentro del contenedor. ?A que usuario
  pertenecera ese proceso cuando lo veas desde el host?
- Si Docker corre en modo rootful, ?sera root o tu usuario?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3: Ver el proceso en el host

```bash
ps aux | grep "sleep 300" | grep -v grep
```

Salida esperada en modo rootful:

```
root  <PID>  ... sleep 300
```

El proceso `sleep 300` que corre DENTRO del contenedor aparece como
propiedad de **root** en el host. Esto significa que si ese proceso lograra
escapar del contenedor (por una vulnerabilidad del kernel o de Docker),
operaria como root en el host.

### Paso 2.4: Comparar con el UID dentro del contenedor

```bash
docker exec rootless-check id
```

Salida esperada:

```
uid=0(root) gid=0(root) groups=0(root)
```

Dentro del contenedor, el proceso se ve como root (UID 0). En el host,
tambien es root (UID 0). En modo rootful, **no hay mapeo de UIDs** — el
root del contenedor es el mismo root del host, solo limitado por namespaces,
capabilities y seccomp.

### Paso 2.5: Ejecutar como non-root y verificar en el host

```bash
docker rm -f rootless-check

docker run -d --name rootless-check --user 1000:1000 alpine sleep 300

ps aux | grep "sleep 300" | grep -v grep
```

Salida esperada:

```
1000  <PID>  ... sleep 300
```

Ahora el proceso aparece como UID 1000 en el host. Si escapara del
contenedor, operaria como UID 1000 (tu usuario), no como root. Esto
reduce significativamente el riesgo.

### Limpieza de Parte 2

```bash
docker rm -f rootless-check
```

---

## Parte 3 — User namespaces

### Objetivo

Entender como los user namespaces mapean UIDs entre contenedor y host,
inspeccionando la configuracion con `docker inspect`.

### Paso 3.1: Crear un contenedor para inspeccion

```bash
docker create --name ns-inspect alpine sleep 120
```

### Paso 3.2: Inspeccionar la configuracion del contenedor

```bash
docker inspect ns-inspect --format '{{json .HostConfig.UsernsMode}}'
```

Salida esperada en modo rootful sin user namespaces:

```
""
```

Un valor vacio indica que **no se usan user namespaces** — el UID dentro del
contenedor es el mismo que en el host. UID 0 dentro = UID 0 fuera.

### Paso 3.3: Verificar el mapeo de UIDs

```bash
docker start ns-inspect
docker exec ns-inspect cat /proc/1/uid_map
```

Salida esperada en modo rootful:

```
         0          0 4294967295
```

Esto se lee como: "UID 0 dentro del contenedor se mapea a UID 0 en el host,
con un rango de 4294967295 UIDs". Es un mapeo 1:1 — no hay remapeo.

En modo rootless con user namespaces, la salida seria diferente:

```
         0       1000          1
         1     100000      65536
```

Esto significaria: "UID 0 dentro = UID 1000 fuera (tu usuario), UID 1
dentro = UID 100000 fuera, etc." Incluso si un proceso es root dentro del
contenedor, fuera seria UID 1000 (sin privilegios).

### Paso 3.4: Verificar subuid (si esta disponible)

```bash
cat /etc/subuid 2>/dev/null || echo "Archivo /etc/subuid no encontrado"
```

Salida esperada (si existe):

```
tu_usuario:100000:65536
```

Este archivo define los rangos de UIDs subordinados que cada usuario puede
mapear. Es necesario para user namespaces en Docker rootless y Podman.

Si el archivo no existe o esta vacio, los user namespaces no estan
configurados en este sistema.

### Paso 3.5: Inspeccionar la estructura de security options

```bash
docker info --format '{{.SecurityOptions}}'
```

Salida esperada (aproximada):

```
[name=apparmor name=seccomp,profile=builtin]
```

En Docker rootless, apareceria `name=rootless` entre las opciones de
seguridad. La presencia de user namespaces se indica ahi.

### Limpieza de Parte 3

```bash
docker rm -f ns-inspect
```

---

## Parte 4 — Podman rootless (comparacion)

### Objetivo

Comparar el modelo de seguridad de Podman (sin daemon, rootless nativo) con
Docker rootful. Si Podman no esta instalado, esta parte es observacional.

### Paso 4.1: Verificar si Podman esta disponible

```bash
podman --version 2>/dev/null || echo "Podman no esta instalado en este sistema"
```

Si Podman no esta instalado, lee los pasos siguientes y las salidas
esperadas para entender las diferencias conceptuales. No es necesario
instalar Podman para completar esta parte.

### Paso 4.2: Comparar la arquitectura (conceptual)

Docker rootful:

```
docker CLI --> docker.sock --> dockerd (root) --> containerd --> runc
```

Podman rootless:

```
podman CLI --> fork/exec --> conmon --> runc  (sin daemon, sin root)
```

La diferencia fundamental: Docker rootful tiene un daemon permanente que
corre como root y escucha en un socket. Podman no tiene daemon — cada
`podman run` es un fork/exec directo.

### Paso 4.3: Ejecutar un contenedor con Podman (si esta disponible)

```bash
podman run --rm alpine id 2>/dev/null || echo "Podman no disponible - ver salida esperada abajo"
```

Salida esperada:

```
uid=0(root) gid=0(root) groups=0(root)
```

Root **dentro** del contenedor, pero el proceso pertenece a tu usuario
en el host.

### Paso 4.4: Verificar propiedad del proceso en el host (si Podman esta disponible)

```bash
if command -v podman &>/dev/null; then
    podman run -d --name podman-check alpine sleep 120
    ps aux | grep "sleep 120" | grep -v grep
    podman rm -f podman-check
else
    echo "Podman no disponible"
    echo "Salida esperada: tu_usuario <PID> ... sleep 120"
    echo "(El proceso pertenece a tu usuario, no a root)"
fi
```

Salida esperada con Podman:

```
tu_usuario  <PID>  ... sleep 120
```

A diferencia de Docker rootful (donde el proceso es de root), con Podman
el proceso pertenece a tu usuario. Un escape del contenedor resultaria
en acceso como tu usuario, no como root.

### Paso 4.5: Comparacion resumida

| Aspecto | Docker rootful | Podman rootless |
|---|---|---|
| Daemon | Si (root) | No |
| Proceso en host | root | Tu usuario |
| Escape del contenedor | root en host | Tu usuario en host |
| Socket de ataque | /var/run/docker.sock | No existe |
| Configuracion | Por defecto | Por defecto |

---

## Limpieza final

```bash
# Eliminar contenedores huerfanos de Docker
docker rm -f rootless-check ns-inspect 2>/dev/null

# Eliminar contenedores huerfanos de Podman (si aplica)
podman rm -f podman-check 2>/dev/null
```

---

## Conceptos reforzados

1. En modo rootful (el predeterminado), el daemon Docker corre como **root**
   y los procesos de los contenedores aparecen como root en el host. Un
   escape del contenedor otorga acceso root al host.

2. Pertenecer al grupo `docker` es **equivalente a tener acceso root** en
   el host, porque el socket permite instruir al daemon (root) a realizar
   cualquier operacion.

3. En modo rootful sin user namespaces, el UID dentro del contenedor es
   el mismo que en el host. UID 0 dentro = UID 0 fuera. Usar `--user`
   en `docker run` reduce el riesgo.

4. Los **user namespaces** remapean UIDs: root dentro del contenedor se
   convierte en un UID sin privilegios en el host. Docker rootless y
   Podman usan este mecanismo.

5. Podman funciona **sin daemon y como rootless por defecto**. No hay un
   proceso permanente con socket al que un atacante pueda conectarse.

6. La diferencia practica mas importante: en Docker rootful, el escape
   del contenedor da root. En Docker rootless o Podman, el escape da
   acceso como tu usuario sin privilegios.
