# Bloque 1: Docker y Entorno de Laboratorio

**Objetivo**: Dominar Docker a nivel suficiente para construir y mantener el entorno
del curso, y entender los fundamentos que conectan Docker con el kernel de Linux.

## Capítulo 1: Arquitectura de Docker [A]

### Sección 1: Modelo de Imágenes
- **T01 - Imágenes y capas**: Union filesystem, copy-on-write, capas de solo lectura
- **T02 - Registros**: Docker Hub, pull, push, tags vs digests, imágenes oficiales vs community
- **T03 - Inspección**: `docker image inspect`, `docker history`, tamaño real vs virtual

### Sección 2: Contenedores
- **T01 - Ciclo de vida**: created → running → paused → stopped → removed
- **T02 - Ejecución**: `docker run` flags críticos (-it, -d, --rm, --name, -e, -p, -v)
- **T03 - Gestión**: `docker ps`, `exec`, `logs`, `stats`, `attach` vs `exec`
- **T04 - Diferencias entre stop, kill y rm**: señales enviadas, timeouts, datos persistentes

### Sección 3: Almacenamiento
- **T01 - Volúmenes**: named volumes, bind mounts, tmpfs mounts
- **T02 - Persistencia**: qué se pierde al destruir un contenedor, estrategias de persistencia
- **T03 - Permisos en bind mounts**: UID/GID mapping, problemas comunes host↔contenedor

### Sección 4: Redes
- **T01 - Drivers de red**: bridge (default), host, none, overlay
- **T02 - Comunicación entre contenedores**: DNS interno, links (legacy), redes custom
- **T03 - Exposición de puertos**: -p vs -P, binding a interfaces específicas

### Sección 5: Seguridad Básica de Docker
- **T01 - Root en contenedor**: por qué root dentro del contenedor puede ser peligroso, USER en Dockerfile
- **T02 - Rootless containers**: modo rootless de Docker, comparación con Podman rootless
- **T03 - Capabilities y seccomp**: --cap-drop, --cap-add, perfiles seccomp, principio de mínimo privilegio
- **T04 - Imágenes de confianza**: imágenes oficiales vs community, escaneo de vulnerabilidades (docker scout), firmar imágenes

## Capítulo 2: Dockerfile [A]

### Sección 1: Sintaxis y Directivas
- **T01 - Instrucciones básicas**: FROM, RUN, COPY, ADD, WORKDIR, CMD, ENTRYPOINT
- **T02 - CMD vs ENTRYPOINT**: diferencias, forma exec vs shell, combinaciones
- **T03 - ARG vs ENV**: scope de build vs runtime, precedencia
- **T04 - HEALTHCHECK**: sintaxis, intervalos, estados de salud

### Sección 2: Optimización
- **T01 - Orden de instrucciones y caché**: invalidación de caché, impacto en build time
- **T02 - Multi-stage builds**: reducir tamaño final, separar build de runtime
- **T03 - .dockerignore**: sintaxis, impacto en contexto de build

## Capítulo 3: Docker Compose [A]

### Sección 1: Fundamentos
- **T01 - Sintaxis YAML**: servicios, redes, volúmenes, versiones del formato
- **T02 - Ciclo de vida**: up, down, start, stop, restart, ps, logs
- **T03 - Variables de entorno**: .env files, interpolación, precedencia

### Sección 2: Orquestación
- **T01 - Dependencias entre servicios**: depends_on, condiciones de salud
- **T02 - Redes multi-servicio**: comunicación por nombre de servicio
- **T03 - Volúmenes compartidos**: datos persistentes entre servicios

## Capítulo 4: Podman [A]

### Sección 1: Podman como Alternativa a Docker
- **T01 - Qué es Podman**: arquitectura daemonless, rootless by default, compatibilidad OCI
- **T02 - Equivalencia de comandos**: podman vs docker — drop-in replacement, alias
- **T03 - Diferencias clave**: no hay daemon, fork/exec model, integración con systemd (podman generate systemd)
- **T04 - Pods**: concepto de pod (agrupación de contenedores), podman pod create, relación con Kubernetes

### Sección 2: Podman Compose y Migración
- **T01 - podman-compose**: instalación, diferencias con docker-compose, limitaciones
- **T02 - Migrar de Docker a Podman**: archivos Dockerfile sin cambios, compatibilidad de imágenes
- **T03 - Cuándo usar Docker vs Podman**: contextos profesionales, RHCSA requiere Podman, industria usa Docker

## Capítulo 5: Laboratorio del Curso [M]

### Sección 1: Construcción del Entorno
- **T01 - Dockerfile Debian dev**: gcc, make, cmake, gdb, valgrind, man-pages, rustup
- **T02 - Dockerfile Alma dev**: gcc, make, cmake, gdb, valgrind, man-pages, rustup
- **T03 - docker-compose.yml del curso**: servicios, volúmenes compartidos, red del lab
- **T04 - Verificación**: script de validación del entorno completo

### Sección 2: Mantenimiento y Limpieza
- **T01 - Monitoreo de espacio**: docker system df, du en /var/lib/docker
- **T02 - Limpieza selectiva**: docker image prune, docker container prune, docker volume prune
- **T03 - Limpieza agresiva**: docker system prune -a, cuándo es seguro, qué se pierde
- **T04 - Estrategia del curso**: rutina de limpieza periódica, reutilización de contenedores con volúmenes
