# Capítulo 1: Arquitectura de Docker [A]

**Bloque**: B01 — Docker y Entorno de Laboratorio
**Tipo**: Aislado
**Objetivo**: Comprender la arquitectura interna de Docker: cómo se construyen las imágenes,
cómo funcionan los contenedores, y cómo se gestionan almacenamiento, redes y seguridad
a nivel fundamental.

---

## Sección 1: Modelo de Imágenes

### T01 — Imágenes y capas

**Conceptos a cubrir**:
- Qué es una imagen Docker: un filesystem empaquetado, inmutable, compuesto por capas
- Union filesystem: cómo OverlayFS (por defecto en Linux moderno) apila capas de solo
  lectura y una capa de escritura (writable layer) encima
- Copy-on-write (CoW): cuando un contenedor modifica un archivo de una capa inferior,
  se copia a la capa de escritura antes de modificarse — el archivo original no cambia
- Cada instrucción de un Dockerfile (RUN, COPY, ADD) genera una nueva capa
- Capas compartidas: si dos imágenes comparten el mismo FROM, las capas base se almacenan
  una sola vez en disco (deduplicación por hash SHA256)
- La capa de escritura se destruye al eliminar el contenedor — todo lo que no esté en un
  volumen se pierde

**Comportamientos importantes**:
- El tamaño de una imagen es la suma de sus capas únicas, pero `docker images` muestra
  el tamaño virtual (incluyendo capas compartidas) — `docker system df` muestra el real
- Eliminar un archivo en un RUN posterior no reduce el tamaño de la imagen: el archivo
  sigue en la capa anterior. Para evitarlo, combinar operaciones en un solo RUN
- `docker image inspect` muestra el array de capas con sus hashes

**Práctica**:
- Construir una imagen simple, luego usar `docker history` para ver las capas y sus tamaños
- Demostrar que modificar un archivo dentro de un contenedor no altera la imagen original
- Comparar el tamaño reportado por `docker images` vs `docker system df`

### T02 — Registros

**Conceptos a cubrir**:
- Qué es un registry: servidor que almacena y distribuye imágenes Docker (OCI)
- Docker Hub: el registry público por defecto, rate limits (100 pulls/6h anónimo,
  200 pulls/6h autenticado)
- Nomenclatura de imágenes: `[registry/][namespace/]name[:tag][@sha256:digest]`
  - Sin registry → Docker Hub por defecto
  - Sin namespace → `library/` (imágenes oficiales)
  - Sin tag → `latest` (¡no significa "la más reciente"!, es solo un tag convencional)
- `docker pull`: descarga una imagen, verifica integridad por digest
- `docker push`: sube una imagen a un registry (requiere `docker login`)
- Tags vs digests:
  - Tag: etiqueta mutable (puede reasignarse a otra imagen)
  - Digest: hash SHA256 inmutable de la imagen — la única forma de garantizar reproducibilidad
  - En producción, preferir `image@sha256:abc123` sobre `image:latest`
- Imágenes oficiales vs community: las oficiales son mantenidas por Docker Inc. y los
  upstream maintainers, con escaneo de seguridad automático

**Comportamientos importantes**:
- `latest` no se actualiza automáticamente — si hiciste `pull` hace 6 meses, tu local
  sigue con esa versión hasta que hagas `pull` de nuevo
- `docker pull` descarga solo las capas que no existan localmente (descarga incremental)
- Los rate limits de Docker Hub aplican por IP en modo anónimo, por cuenta en autenticado

**Práctica**:
- Hacer pull de una imagen por tag y por digest, comparar
- Inspeccionar los tags disponibles de una imagen con `docker manifest inspect`
- Verificar el rate limit restante con `docker manifest inspect` headers

### T03 — Inspección

**Conceptos a cubrir**:
- `docker image inspect <image>`: devuelve JSON con toda la metadata de la imagen
  - Architecture, Os: plataforma target
  - Config: CMD, ENTRYPOINT, ENV, ExposedPorts, Volumes, WorkingDir
  - RootFS.Layers: lista de hashes de capas
  - Size vs VirtualSize (deprecated en API recientes)
- `docker history <image>`: muestra cada capa, la instrucción que la creó, y su tamaño
  - Las capas con tamaño 0B son metadata (CMD, ENV, EXPOSE, LABEL)
  - `--no-trunc` para ver las instrucciones completas
- Tamaño real vs virtual: el tamaño real es el espacio único que ocupa; el virtual
  incluye capas compartidas con otras imágenes

**Comportamientos importantes**:
- `docker history` puede mostrar `<missing>` en IMAGE ID para capas intermedias que
  fueron limpiadas o vienen de un pull remoto — es normal
- Las imágenes multi-platform (amd64, arm64) comparten el mismo tag pero son imágenes
  distintas; `docker image inspect` muestra la de tu plataforma actual

**Práctica**:
- Inspeccionar una imagen oficial (debian:bookworm) y leer su configuración
- Comparar `docker history` de una imagen base vs una derivada
- Calcular cuánto espacio real ocupan varias imágenes que comparten capas

---

## Sección 2: Contenedores

### T01 — Ciclo de vida

**Conceptos a cubrir**:
- Un contenedor es una instancia en ejecución de una imagen + una capa de escritura
- Estados del contenedor:
  - **created**: existe pero nunca se ha iniciado (`docker create`)
  - **running**: proceso principal activo
  - **paused**: proceso congelado con SIGSTOP via cgroups freezer (`docker pause`)
  - **stopped/exited**: proceso terminó (con exit code), capa de escritura preservada
  - **removing**: en proceso de eliminación
  - **dead**: fallo al eliminar (filesystem busy, etc.)
- Un contenedor vive mientras su proceso principal (PID 1) esté activo
- El exit code del contenedor es el exit code de PID 1 (0 = éxito, 137 = SIGKILL, etc.)
- `docker start` puede reiniciar un contenedor detenido (con su capa de escritura intacta)
- `docker restart` = stop + start (¡con un timeout para SIGTERM antes de SIGKILL!)

**Comportamientos importantes**:
- Un contenedor en estado "exited" sigue ocupando espacio en disco (su capa de escritura)
  hasta que se elimine con `docker rm`
- `docker create` asigna almacenamiento y red pero no ejecuta nada — útil para preparar
  antes de iniciar, o para copiar archivos con `docker cp`
- Si PID 1 no maneja señales correctamente, `docker stop` espera 10s y envía SIGKILL

**Práctica**:
- Crear un contenedor sin iniciarlo, luego iniciarlo, pausarlo, reanudarlo, detenerlo
- Observar los exit codes de contenedores que terminan normalmente vs por señal
- Demostrar que `docker start` preserva la capa de escritura

### T02 — Ejecución

**Conceptos a cubrir**:
- `docker run` = `docker create` + `docker start` + (opcionalmente) `docker attach`
- Flags críticos:
  - `-i` (interactive): mantiene STDIN abierto, necesario para enviar input al proceso
  - `-t` (tty): asigna una pseudo-terminal, necesario para programas que usan ncurses,
    colores, o que detectan si están en terminal
  - `-it` juntos: la combinación estándar para sesiones interactivas (bash, shells)
  - `-d` (detach): ejecuta en background, devuelve el container ID
  - `--rm`: elimina el contenedor automáticamente cuando PID 1 termina (limpieza automática)
  - `--name`: asigna un nombre al contenedor (si no, Docker genera uno aleatorio)
  - `-e KEY=VALUE`: establece variables de entorno dentro del contenedor
  - `-p hostPort:containerPort`: mapea un puerto del host al contenedor
  - `-v hostPath:containerPath`: monta un directorio o volumen
  - `-w`: establece el directorio de trabajo
  - `--user`: ejecuta como un usuario específico (UID:GID)
- El argumento final puede sobrescribir el CMD de la imagen:
  `docker run debian:bookworm echo "hello"` ejecuta `echo` en vez de `bash`
- `--restart` policies: no, on-failure[:max-retries], always, unless-stopped

**Comportamientos importantes**:
- `-it` sin `-d`: el terminal queda adjunto al contenedor — Ctrl+C envía SIGINT a PID 1
- `-d` sin `-it`: el contenedor corre en background, `docker logs` para ver la salida
- `-t` sin `-i`: el proceso tiene una TTY pero no puede recibir input — útil para
  procesos que necesitan TTY para formatear su salida pero no requieren input
- `--rm` no se puede combinar con `--restart` (excepto `--restart no`)
- Sin `-t`, algunos programas cambian su comportamiento: ls no muestra colores,
  grep no colorea matches, python buferea la salida

**Práctica**:
- Ejecutar contenedores interactivos y en background
- Demostrar la diferencia entre ejecutar con y sin `-t` (ls con/sin colores)
- Usar `--rm` para contenedores efímeros vs mantenerlos para inspección

### T03 — Gestión

**Conceptos a cubrir**:
- `docker ps`: lista contenedores en ejecución
  - `docker ps -a`: incluye detenidos
  - `docker ps -q`: solo IDs (útil para scripting: `docker rm $(docker ps -aq)`)
  - `docker ps --filter`: filtrar por estado, nombre, label, etc.
  - `docker ps --format`: formateo custom con Go templates
- `docker exec`: ejecuta un comando en un contenedor en ejecución
  - `docker exec -it <container> bash`: abrir una shell interactiva
  - `docker exec -u root <container> command`: ejecutar como root aunque el contenedor
    corra como otro usuario
  - `docker exec -w /path command`: ejecutar en directorio específico
- `docker logs`: ver la salida (stdout/stderr) del proceso principal
  - `-f` (follow): equivalente a `tail -f`
  - `--since`, `--until`: filtrar por tiempo
  - `--tail N`: solo las últimas N líneas
- `docker stats`: monitoreo en tiempo real (CPU, memoria, I/O, red) estilo `top`
  - `--no-stream`: muestra una sola captura y sale
- `attach` vs `exec`:
  - `attach` conecta al stdin/stdout de PID 1 — Ctrl+C puede matar el contenedor
  - `exec` crea un proceso nuevo — Ctrl+C mata solo ese proceso
  - Regla general: usar `exec` para inspeccionar, `attach` rara vez

**Comportamientos importantes**:
- `docker logs` almacena los logs en disco (/var/lib/docker/containers/ID/) —
  en contenedores de larga vida puede llenar el disco. Configurar `--log-opt max-size`
- `docker exec` falla si el contenedor está detenido — solo funciona en "running"
- `docker attach` con `--detach-keys` permite definir una secuencia de escape que no
  mata al contenedor (default: Ctrl+P, Ctrl+Q)

**Práctica**:
- Listar, filtrar y formatear la salida de `docker ps`
- Abrir múltiples shells en un mismo contenedor con `exec`
- Monitorear recursos con `docker stats` mientras se ejecuta un proceso intensivo

### T04 — Diferencias entre stop, kill y rm

**Conceptos a cubrir**:
- `docker stop`:
  - Envía SIGTERM a PID 1
  - Espera un grace period (default: 10s, configurable con `-t`)
  - Si el proceso no termina, envía SIGKILL
  - El contenedor queda en estado "exited" (no se elimina)
- `docker kill`:
  - Envía SIGKILL inmediatamente (o la señal especificada con `-s`)
  - No hay grace period — terminación abrupta
  - `docker kill -s SIGUSR1 container`: enviar señales arbitrarias a PID 1
- `docker rm`:
  - Elimina un contenedor detenido (falla si está corriendo, salvo con `-f`)
  - `docker rm -f` = `docker kill` + `docker rm`
  - `docker rm -v`: también elimina volúmenes anónimos asociados
- Datos persistentes:
  - La capa de escritura persiste entre `stop`/`start` pero se destruye con `rm`
  - Los volúmenes nombrados sobreviven a `rm` (hay que eliminarlos explícitamente)
  - Los volúmenes anónimos sobreviven a `rm` a menos que se use `-v`

**Comportamientos importantes**:
- Muchas imágenes base no manejan señales correctamente en PID 1 (ej: shell script
  como entrypoint que no propaga SIGTERM) — por eso `docker stop` a veces tarda 10s
  completos antes de enviar SIGKILL
- Usar `tini` o `--init` para un init system ligero que maneja señales correctamente
- `docker rm` de un contenedor con volúmenes anónimos sin `-v` deja volúmenes huérfanos

**Práctica**:
- Comparar el tiempo que tarda `docker stop` vs `docker kill`
- Demostrar un contenedor que no maneja SIGTERM y requiere el timeout completo
- Usar `--init` y ver la diferencia en el manejo de señales

---

## Sección 3: Almacenamiento

### T01 — Volúmenes

**Conceptos a cubrir**:
- Tres tipos de montaje en Docker:
  1. **Named volumes** (`-v mydata:/path`): gestionados por Docker en `/var/lib/docker/volumes/`,
     persistentes, portables, el mecanismo recomendado
  2. **Bind mounts** (`-v /host/path:/container/path`): monta un directorio del host
     directamente en el contenedor, útil para desarrollo (código fuente, configuración)
  3. **tmpfs mounts** (`--tmpfs /path`): filesystem en memoria RAM, no persiste,
     útil para datos sensibles temporales (secrets, caches)
- Sintaxis `--mount` vs `-v`:
  - `-v` es la forma corta: `source:target[:options]`
  - `--mount` es la forma explícita: `type=volume,source=mydata,target=/path,readonly`
  - `--mount` falla si el source no existe; `-v` lo crea automáticamente (para volúmenes)
- Volúmenes anónimos: se crean sin nombre cuando usas `-v /path` (sin `source:`),
  Docker genera un hash como nombre — difíciles de rastrear

**Comportamientos importantes**:
- Si el destino del volumen ya contiene datos en la imagen, Docker copia esos datos
  al volumen la primera vez (solo named volumes, no bind mounts)
- Los bind mounts **ocultan** el contenido que existía en ese path de la imagen
- Named volumes persisten incluso si no hay contenedores usándolos — limpiar periódicamente
  con `docker volume prune`
- Un named volume se puede compartir entre múltiples contenedores simultáneamente

**Práctica**:
- Crear un named volume, escribir datos, destruir el contenedor, verificar que los datos
  persisten al crear un nuevo contenedor con el mismo volumen
- Demostrar la diferencia entre bind mount (oculta contenido) vs named volume (copia datos)
- Usar un tmpfs mount y verificar que no persiste

### T02 — Persistencia

**Conceptos a cubrir**:
- Qué se pierde al destruir un contenedor (docker rm):
  - Todo lo escrito en la capa de escritura (archivos nuevos, modificaciones)
  - Logs internos (salvo que se hayan redirigido a volúmenes)
  - Estado del proceso, archivos temporales en /tmp
- Qué sobrevive:
  - Named volumes
  - Bind mounts (los datos están en el host)
  - La imagen original (inmutable)
- Estrategias de persistencia:
  - Configuración: bind mount de archivos de config del host
  - Datos de aplicación: named volumes
  - Código fuente (desarrollo): bind mount del directorio de trabajo
  - Bases de datos: named volumes obligatorios — nunca usar la capa de escritura
  - Logs: logging driver (json-file, syslog, journald) o bind mount de /var/log

**Comportamientos importantes**:
- `docker stop` + `docker start` preserva la capa de escritura (no se pierden datos)
- `docker commit` puede crear una imagen a partir de un contenedor con su capa de
  escritura — útil para debug, no para workflows de producción (usa Dockerfile)
- Los volúmenes anónimos de instrucciones VOLUME en Dockerfile se crean automáticamente
  en cada `docker run` — pueden acumularse rápidamente

**Práctica**:
- Simular pérdida de datos por no usar volúmenes
- Implementar un patrón de desarrollo con bind mounts: editar en el host, ejecutar en contenedor

### T03 — Permisos en bind mounts

**Conceptos a cubrir**:
- Los contenedores Linux comparten el kernel del host — los UIDs/GIDs son los mismos
- Si el host tiene UID 1000 (tu usuario) y el contenedor corre como root (UID 0),
  los archivos creados por el contenedor en el bind mount serán propiedad de root en el host
- Problema clásico: `docker run -v $(pwd):/app image` → los archivos generados
  dentro de /app pertenecen a root:root en el host, y tu usuario no puede editarlos
- Soluciones:
  - `--user $(id -u):$(id -g)`: ejecutar el contenedor con tu UID/GID
  - Crear un usuario en el Dockerfile con el mismo UID que el del host
  - User namespace remapping (avanzado): mapear UID 0 del contenedor a un UID sin
    privilegios del host
- `COPY --chown` y `ADD --chown` en Dockerfile: establecer propietario al copiar archivos

**Comportamientos importantes**:
- `--user` sin crear el usuario en la imagen puede causar warnings ("I have no name!")
  pero funciona para la mayoría de operaciones de I/O
- Los named volumes no tienen el problema de permisos porque Docker gestiona los permisos
  internamente
- En macOS/Windows con Docker Desktop, los bind mounts usan una capa de virtualización
  que maneja los permisos de forma diferente — en Linux nativo el problema es real

**Práctica**:
- Demostrar el problema de permisos: crear archivos en un bind mount como root
- Solucionar con `--user`, verificar que los archivos tienen el UID correcto
- Crear un Dockerfile que cree un usuario non-root con UID configurable via ARG

---

## Sección 4: Redes

### T01 — Drivers de red

**Conceptos a cubrir**:
- Docker crea una red bridge por defecto al instalarse (`docker0`, 172.17.0.0/16)
- Drivers disponibles:
  - **bridge**: red aislada por defecto. Contenedores obtienen IP en una subred privada.
    Comunicación con el host via NAT. El driver más usado
  - **host**: el contenedor comparte el network stack del host directamente (sin aislamiento
    de red). No necesita port mapping (-p). Máximo rendimiento de red
  - **none**: sin red. El contenedor solo tiene loopback. Para contenedores que no necesitan
    conectividad (procesamiento de datos local)
  - **overlay**: redes multi-host para Docker Swarm (no se cubre en este curso)
  - **macvlan**: asigna una MAC address real del host al contenedor, haciéndolo aparecer
    como un dispositivo físico en la red (avanzado, no cubierto)
- `docker network create`: crear redes custom
  - `--subnet`, `--gateway`: configurar subredes específicas
  - `--driver`: especificar el driver

**Comportamientos importantes**:
- La red bridge por defecto (`docker0`) no tiene DNS interno — los contenedores solo
  pueden comunicarse por IP. Las redes bridge custom sí tienen DNS (por nombre de contenedor)
- `host` network no funciona en Docker Desktop (macOS/Windows) — solo en Linux nativo
- Cada red bridge es aislada: contenedores en redes diferentes no se ven entre sí
  a menos que se conecten explícitamente

**Práctica**:
- Inspeccionar la red bridge por defecto con `docker network inspect bridge`
- Crear una red custom y demostrar la resolución DNS entre contenedores
- Ejecutar un contenedor con `--network host` y verificar que comparte las interfaces del host

### T02 — Comunicación entre contenedores

**Conceptos a cubrir**:
- DNS interno: en redes bridge custom, Docker tiene un DNS server embebido (127.0.0.11)
  que resuelve nombres de contenedor a IPs
- El nombre del contenedor (--name) es el hostname en la red — `ping container_name` funciona
- Links (legacy): `--link container:alias` — obsoleto, usar redes custom en su lugar
- Un contenedor puede estar conectado a múltiples redes simultáneamente:
  `docker network connect mynet container`
- `docker network disconnect`: quitar un contenedor de una red sin detenerlo

**Comportamientos importantes**:
- Los contenedores en la red bridge por defecto solo se comunican por IP
  (no por nombre) a menos que uses `--link` (deprecated)
- Si dos contenedores necesitan comunicarse, ponerlos en la misma red custom
  es la forma correcta y moderna
- El DNS interno actualiza las entradas cuando un contenedor se reinicia
  (la IP puede cambiar, pero el nombre sigue resolviendo)

**Práctica**:
- Crear dos contenedores en la red por defecto e intentar comunicarlos por nombre (falla)
- Crear una red custom, conectar ambos, y demostrar la resolución DNS
- Conectar un contenedor a dos redes y demostrar que puede hablar con contenedores en ambas

### T03 — Exposición de puertos

**Conceptos a cubrir**:
- `EXPOSE` en Dockerfile: solo documentación, no abre puertos realmente
- `-p hostPort:containerPort`: mapea un puerto del host a uno del contenedor
  - `-p 8080:80`: el host escucha en 8080, redirige al 80 del contenedor
  - `-p 127.0.0.1:8080:80`: bind solo a localhost del host (no accesible externamente)
  - `-p 8080:80/udp`: mapear puertos UDP explícitamente
  - `-p 8080-8090:80-90`: rangos de puertos
- `-P` (mayúscula): mapea todos los puertos EXPOSE a puertos aleatorios del host
  (32768+). Ver asignación con `docker port container`
- `docker port container`: muestra los mapeos activos

**Comportamientos importantes**:
- `-p` crea reglas iptables/nftables en el host — bypassea firewalld/ufw por defecto
  (un puerto publicado con `-p` es accesible aunque el firewall del host lo bloquee)
- Si el puerto del host ya está en uso, `docker run -p` falla (bind error)
- Sin `-p`, los puertos del contenedor solo son accesibles desde dentro de la red Docker
  (otros contenedores en la misma red pueden acceder directamente sin port mapping)
- Publicar en `0.0.0.0` (default) expone el puerto a todas las interfaces del host,
  incluyendo la red externa — en servidores, considerar bindear a `127.0.0.1`

**Práctica**:
- Ejecutar un servidor web en un contenedor y acceder desde el host
- Demostrar la diferencia entre `-p 80:80` (accesible externamente) y `-p 127.0.0.1:80:80`
- Usar `-P` y verificar los puertos asignados con `docker port`

---

## Sección 5: Seguridad Básica de Docker

### T01 — Root en contenedor

**Conceptos a cubrir**:
- Por defecto, los procesos dentro de un contenedor corren como root (UID 0)
- Aunque los namespaces aíslan el contenedor, root dentro del contenedor tiene capabilities
  que podrían explotarse para escapar (container breakout)
- La instrucción USER en Dockerfile cambia el usuario que ejecuta CMD/ENTRYPOINT:
  ```dockerfile
  RUN useradd -r -s /bin/false appuser
  USER appuser
  ```
- `docker run --user 1000:1000`: sobrescribe el USER de la imagen
- Principio de mínimo privilegio: correr como non-root siempre que sea posible

**Comportamientos importantes**:
- Algunas imágenes requieren root para funcionar (instalación de paquetes, bind a puertos < 1024)
  pero el proceso principal debería correr como non-root después de la inicialización
- Las imágenes oficiales de servicios (nginx, postgres) ya incluyen mecanismos para
  iniciar como root y luego hacer drop de privilegios
- Un contenedor como root + bind mount de /etc/shadow del host = acceso a los hashes
  de contraseñas del host (ejemplo de por qué rootless importa)

**Práctica**:
- Ejecutar `id` en un contenedor por defecto (muestra root)
- Crear un Dockerfile con USER non-root y demostrar las limitaciones
- Demostrar por qué `-v /etc:/host-etc` como root es peligroso

### T02 — Rootless containers

**Conceptos a cubrir**:
- Docker en modo rootless: el daemon corre como un usuario sin privilegios
  - Ventaja: incluso si un atacante escapa del contenedor, no tiene root en el host
  - Limitaciones: no puede bindear puertos < 1024, usa slirp4netns para networking
    (más lento), no puede usar ciertos storage drivers
- Podman rootless: no requiere modo especial, es rootless por diseño (sin daemon)
- Comparación:
  - Docker rootful (default): daemon corre como root, contenedores pueden ser root
  - Docker rootless: daemon corre como usuario, menos capabilities
  - Podman rootless: sin daemon, fork/exec, rootless natural

**Comportamientos importantes**:
- Docker rootless almacena datos en ~/.local/share/docker en vez de /var/lib/docker
- Algunos Dockerfiles que asumen root (apt install, mkdir /opt/...) fallarán en rootless
  si el contenedor no corre con las capabilities necesarias
- En RHCSA/RHEL, Podman rootless es la forma estándar de trabajar con contenedores

**Práctica**:
- Comparar `docker info` en modo rootful vs rootless (si es posible en el setup del lab)
- Demostrar las diferencias de comportamiento de networking en rootless

### T03 — Capabilities y seccomp

**Conceptos a cubrir**:
- Linux capabilities: el kernel divide los privilegios de root en ~40 capacidades
  independientes (CAP_NET_BIND_SERVICE, CAP_SYS_ADMIN, CAP_CHOWN, etc.)
- Docker por defecto elimina muchas capabilities peligrosas pero mantiene algunas
  (CHOWN, DAC_OVERRIDE, FOWNER, KILL, NET_BIND_SERVICE, etc.)
- `--cap-drop ALL`: eliminar todas las capabilities
- `--cap-add NET_RAW`: añadir solo las necesarias
- Patrón recomendado: `--cap-drop ALL --cap-add <solo las necesarias>`
- seccomp (Secure Computing): filtra syscalls que el contenedor puede ejecutar
  - Docker tiene un perfil seccomp por defecto que bloquea ~44 syscalls peligrosas
    (mount, reboot, kexec_load, etc.)
  - `--security-opt seccomp=profile.json`: perfil custom
  - `--security-opt seccomp=unconfined`: deshabilitar seccomp (peligroso)
- Principio de mínimo privilegio: solo otorgar lo estrictamente necesario

**Comportamientos importantes**:
- `--privileged` desactiva todas las restricciones de seguridad (capabilities, seccomp,
  AppArmor/SELinux, cgroups) — equivale a darle acceso completo al host. Solo usar
  para casos muy específicos (Docker-in-Docker, acceso a hardware)
- Sin CAP_NET_RAW, `ping` no funciona dentro del contenedor (requiere raw sockets)
- El perfil seccomp por defecto de Docker es un buen balance entre seguridad y compatibilidad

**Práctica**:
- Ejecutar un contenedor con `--cap-drop ALL` e intentar operaciones que requieren
  capabilities específicas
- Añadir capabilities una a una y observar qué operaciones se desbloquean
- Comparar el comportamiento con y sin `--privileged`

### T04 — Imágenes de confianza

**Conceptos a cubrir**:
- Imágenes oficiales (library/): mantenidas y escaneadas por Docker Inc., base
  minimal, documentación detallada, actualizaciones de seguridad regulares
- Imágenes community: cualquiera puede publicar, calidad y seguridad variables
- Riesgos de imágenes no confiables:
  - Malware embebido, crypto miners, backdoors
  - Dependencias desactualizadas con vulnerabilidades conocidas (CVEs)
  - Base images abandonadas sin parches de seguridad
- Docker Scout (antes docker scan): escaneo de vulnerabilidades integrado
  - `docker scout cves <image>`: listar CVEs conocidos
  - `docker scout quickview`: resumen de la postura de seguridad
- Firmar imágenes: Docker Content Trust (DCT), DOCKER_CONTENT_TRUST=1 fuerza
  la verificación de firma en cada pull
- Buenas prácticas:
  - Usar imágenes oficiales o verificadas como base
  - Especificar versiones exactas (no latest)
  - Escanear regularmente
  - Reconstruir periódicamente para incorporar parches de seguridad

**Comportamientos importantes**:
- DOCKER_CONTENT_TRUST=1 bloquea el pull de imágenes no firmadas — puede romper
  scripts que usan imágenes community
- Los escaneos de seguridad tienen falsos positivos — evaluar si la CVE aplica
  a tu caso de uso antes de entrar en pánico

**Práctica**:
- Escanear una imagen con `docker scout cves`
- Comparar las CVEs de una imagen base antigua vs una reciente
- Habilitar Docker Content Trust y verificar el comportamiento con imágenes firmadas/no firmadas
