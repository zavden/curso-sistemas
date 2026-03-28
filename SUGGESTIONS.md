# SUGGESTIONS — Curso Linux-C-Rust

Sugerencias para mejorar la estructura original, organizadas por bloque.
Cada sugerencia incluye justificación y nivel de prioridad:
- **[ALTA]** — Llena un vacío significativo o evita problemas serios.
- **[MEDIA]** — Mejora la experiencia de aprendizaje o la cobertura profesional.
- **[BAJA]** — Nice-to-have, se puede omitir sin impacto grave.

---

## Sugerencias Generales

### ~~SG-1: Podman como alternativa a Docker [MEDIA]~~ ✔ Aplicada

RHCSA (EX200) incluye Podman, no Docker, desde la versión 8. Sugiero añadir un
capítulo breve (o al menos tópicos comparativos) en el Bloque 1 que cubra:
- `podman` como drop-in replacement de `docker`
- Diferencias clave: rootless by default, no requiere daemon, compatibilidad OCI
- `podman-compose` vs `docker-compose`

No es necesario duplicar todo el contenido Docker, solo mostrar la equivalencia
y las diferencias. Esto es particularmente relevante para RHCSA.

### ~~SG-2: Proyecto en C de sistemas (no solo Rust) [MEDIA]~~ ✔ Aplicada

El TODO original define proyectos GUI solo en Rust (egui). Sugiero añadir al menos
un proyecto de sistemas en C (además de la mini-shell propuesta en B06):
- Opción A: Un servidor HTTP básico en C (demuestra sockets, fork, file I/O).
- Opción B: Una implementación simplificada de `ls` o `find` (demuestra directorios,
  stat, permisos, formateo).

Esto equilibra la carga práctica entre ambos lenguajes.

### ~~SG-3: Shell scripting como capítulo dedicado [ALTA]~~ ✔ Aplicada

El TODO asume conocimiento básico de Bash, pero LPIC-2 y RHCSA requieren scripting
intermedio (funciones, traps, regex, exit codes, here documents). Sugiero considerar
si el Bloque 2 necesita un capítulo dedicado a scripting avanzado o si el conocimiento
previo del usuario es suficiente. Si lo segundo, al menos incluir tópicos de scripting
en los capítulos de tareas programadas y logging (scripts de cron, scripts de
mantenimiento).

### ~~SG-4: Debugging como sección transversal [MEDIA]~~ ✔ Aplicada

GDB (C) y herramientas de debug de Rust (lldb, cargo-expand, RUST_BACKTRACE) aparecen
en sus bloques respectivos, pero un tópico de "debugging real: cómo investigar un crash"
sería valioso como capítulo integrador en B06 o B07.

---

## Bloque 1: Docker y Entorno de Laboratorio

### ~~B01-S1: Docker security basics [BAJA]~~ ✔ Aplicada

El curso no cubre seguridad de Docker (root en contenedor = root en host por defecto,
user namespaces, seccomp, capabilities). Dado que es un lab de aprendizaje,
no es crítico, pero un tópico breve evitaría malas prácticas.

### ~~B01-S2: Limpieza de recursos Docker [MEDIA]~~ ✔ Aplicada

El TODO enfatiza no abusar del espacio en disco. Sugiero un tópico dedicado a:
- `docker system prune`
- `docker image prune`
- Estrategias de limpieza periódica
- Monitoreo de espacio consumido por Docker

---

## Bloque 2: Fundamentos de Linux

### ~~B02-S1: Text processing tools en profundidad [MEDIA]~~ ✔ Cubierta por SG-3

Ya cubierta por el capítulo de Shell Scripting (B02 C05, Sección 3: Herramientas de
Texto), que incluye sed, awk, cut, sort, uniq, tr, tee, xargs y grep avanzado.

### B02-S3: Sección sobre namespaces y cgroups en C03 [ALTA]

Añadir una nueva sección al final de C03_Gestión_de_Procesos que cubra los
mecanismos del kernel que hacen posibles los contenedores, sin programación —
solo exploración desde la línea de comandos. Complementa la sugerencia B06-S3.

Contenido sugerido (S04_Aislamiento_de_Procesos o similar):

- **T01 — Namespaces**: qué son y qué aíslan. Los 8 tipos de namespace Linux
  (PID, Mount, Network, UTS, IPC, User, Cgroup, Time). Ver namespaces activos
  con `lsns`, entrar en un namespace con `nsenter`, crear uno con `unshare`
- **T02 — cgroups v2**: jerarquía en `/sys/fs/cgroup/`, controladores (memory,
  cpu, io, pids), cómo systemd los usa, ver los cgroups de un proceso en
  `/proc/[pid]/cgroup`
- **T03 — chroot y pivot_root**: concepto de cambio de raíz, diferencias entre
  los dos, por qué Docker usa `pivot_root` y no `chroot`
- **T04 — Contenedor a mano con unshare**: crear un entorno aislado completo
  desde bash sin Docker: `unshare --pid --fork --mount-proc chroot /path/a/rootfs
  /bin/sh` — el estudiante ve un shell con PID 1 y un filesystem propio

Esta sección cierra el ciclo conceptual de B01 (Docker) y prepara el terreno para
el proyecto de B06-S3 donde se implementa lo mismo en C.

### ~~B02-S2: Capítulo de SELinux/AppArmor en B02 (preview) [BAJA]~~ ✔ Aplicada

El Bloque 11 cubre SELinux/AppArmor en profundidad, pero un tópico de "primer
contacto" en B02 (solo `getenforce`, `sestatus`, `ls -Z`) puede evitar confusión
cuando los contenedores RHEL muestren errores de SELinux durante otros bloques.

---

## Bloque 3: Fundamentos de C

### ~~B03-S1: Estándar C a usar [ALTA]~~ ✔ Aplicada

El plan usa C11 por defecto (en las flags de GCC), pero no lo declara explícitamente.
Sugiero definir el estándar del curso en un tópico:
- C99 vs C11 vs C17 vs C23: qué cambió en cada uno
- El curso usará C11 como base (excepto tópicos específicos de C23 como `__VA_OPT__`)
- Comportamiento por compilador: GCC default (-std=gnu17 en versiones recientes) vs -std=c11

### ~~B03-S2: Endianness [MEDIA]~~ ✔ Aplicada

No hay un tópico explícito de endianness (big-endian vs little-endian). Es relevante
para:
- Programación de red (network byte order)
- Lectura de archivos binarios
- Portabilidad

Sugiero añadirlo como tópico en B03 (Capítulo 2, Sección 1) o B06 (Capítulo 6,
Sección 1 junto con sockets).

---

## Bloque 5: Fundamentos de Rust

### ~~B05-S1: Closures como sección dedicada [MEDIA]~~ ✔ Aplicada

Las closures en Rust son más complejas que en otros lenguajes (Fn, FnMut, FnOnce,
move closures). El plan las menciona implícitamente en iteradores y threads, pero
una sección dedicada antes de iteradores aclararía:
- Sintaxis y captura de variables
- Los 3 traits de closure y cuándo se infiere cada uno
- move closures vs ref closures
- Closures como parámetros (impl Fn vs dyn Fn vs genéricos)

### ~~B05-S2: Smart pointers [ALTA]~~ ✔ Aplicada

El plan no tiene una sección explícita de smart pointers, aunque aparecen dispersos
(Box en trait objects, Arc/Mutex en concurrencia). Sugiero un capítulo o sección
dedicada que cubra:
- `Box<T>`: heap allocation, cuándo necesario, recursive types
- `Rc<T>` y `Arc<T>`: reference counting, cuándo usar cada uno
- `Cell<T>` y `RefCell<T>`: interior mutability, por qué existen
- `Cow<T>`: clone-on-write, optimización de strings
- `Weak<T>`: romper ciclos de referencia

Esto debería ir entre Capítulo 8 (Genéricos) y Capítulo 9 (Módulos).

### ~~B05-S3: Macros declarativas [BAJA]~~ ✔ Aplicada

El plan no cubre `macro_rules!`. No es esencial para el curso, pero es útil para
entender macros del ecosistema (vec!, println!, assert!). Un tópico breve bastaría.

---

## Bloque 6: Programación de Sistemas en C

### ~~B06-S1: mmap [MEDIA]~~ ✔ Aplicada

`mmap` no aparece como tópico independiente, solo dentro de memoria compartida POSIX.
Es una syscall fundamental con múltiples usos:
- Mapeo de archivos (alternativa a read/write para archivos grandes)
- Memoria compartida
- Asignación anónima (lo que malloc hace internamente para bloques grandes)

Sugiero un tópico dedicado en el Capítulo 1 o Capítulo 4.

### ~~B06-S2: inotify [BAJA]~~ ✔ Aplicada

Monitoreo de cambios en el filesystem. Útil para daemons y herramientas de sistema.
No es esencial pero es un ejercicio práctico valioso.

### B06-S3: Proyecto — Mini runtime de contenedor en C [ALTA]

Implementar un contenedor mínimo desde cero en C usando las primitivas reales del
kernel. El ejercicio desmitifica completamente qué hace Docker por dentro y refuerza
syscalls, procesos y montajes en un contexto concreto.

Contenido sugerido como nuevo capítulo (o sección del Capítulo 7):

- **Namespaces**: `clone()` con flags `CLONE_NEWPID`, `CLONE_NEWNS`, `CLONE_NEWUTS`,
  `CLONE_NEWNET`, `CLONE_NEWIPC` — cada uno aísla una dimensión del proceso
- **Filesystem isolation**: `chroot()` sobre un rootfs mínimo (Alpine extraído),
  luego `pivot_root()` como hace Docker realmente
- **Montar /proc en el nuevo namespace**: el proceso necesita su propio `/proc` o
  los comandos como `ps` muestran procesos del host
- **cgroups v2**: crear un cgroup, escribir el PID del proceso, aplicar límites de
  memoria y CPU via el filesystem `/sys/fs/cgroup/`
- **Red básica**: crear un par `veth`, mover un extremo al network namespace del
  contenedor, configurar IPs

Resultado: un programa en C de ~300 líneas que ejecuta un proceso aislado con su
propio PID 1, filesystem, hostname, y límites de recursos — funcionalmente un
contenedor sin Docker.

Prerrequisito natural para este capítulo: B06 C02 (fork/exec), C01 (syscalls),
C01 S3 (mmap). Conecta directamente con B01 (Docker) dándole una base técnica real.

---

## Bloque 8: Almacenamiento

### ~~B08-S1: Stratis y VDO [BAJA]~~ ✔ Aplicada

Stratis (gestión de almacenamiento simplificada) y VDO (deduplicación y compresión)
son tecnologías RHEL que aparecen en RHCSA. Son relativamente nuevas y poco
extendidas fuera de RHEL. Sugiero un tópico breve que al menos mencione su existencia
y propósito, sin profundizar.

---

## Bloque 9: Redes

### ~~B09-S1: Protocolo HTTP en mayor profundidad [MEDIA]~~ ✔ Aplicada

El plan cubre HTTP como parte de los servicios web (B10), pero dado que es
prerrequisito para WebAssembly, sugiero una sección dedicada al protocolo HTTP que
cubra:
- Request/Response structure (métodos, headers, status codes)
- Content-Type y MIME types
- CORS (relevante para WASM en navegador)
- HTTP/2 y HTTP/3: multiplexing, server push

Esto podría ir en B09 (Capítulo 1) o como capítulo independiente.

### ~~B09-S2: Wireguard [BAJA]~~ ✔ Aplicada

Más moderno que OpenVPN, kernel-integrado, y cada vez más adoptado. Un tópico
comparativo con OpenVPN podría ser valioso pero no es prioritario.

---

## Bloque 10: Servicios de Red

### ~~B10-S1: LDAP client [MEDIA]~~ ✔ Aplicada

LPIC-2 topic 210 incluye LDAP client configuration (nsswitch.conf, pam_ldap,
nss_ldap o sssd). El plan menciona PAM pero no LDAP. Sugiero al menos un tópico
básico de:
- Concepto de LDAP y directorio
- sssd: configuración de cliente LDAP/AD
- Integración con PAM y NSS

### ~~B10-S2: Proxy inverso con más contexto [BAJA]~~ ✔ Aplicada

Nginx como proxy inverso se cubre, pero el concepto de proxy forward (Squid) solo
se menciona en el TODO original. Sugiero decidir si incluirlo o no — LPIC-2 no lo
requiere en versiones recientes, pero es útil en redes corporativas.

---

## Bloque 12: GUI con Rust (egui)

### ~~B12-S1: egui vs otras opciones [BAJA]~~ ✔ Aplicada

Un tópico introductorio que compare brevemente egui con iced, gtk-rs, Slint y Tauri
ayudaría a contextualizar la elección. No se trata de enseñar otros frameworks, solo
de justificar por qué se eligió egui.

### ~~B12-S2: Accesibilidad y temas [BAJA]~~ ✔ Aplicada

egui tiene soporte limitado de accesibilidad. Un tópico breve sobre:
- Dark/light mode
- Escalado (DPI awareness)
- Limitaciones de accesibilidad en immediate mode GUIs

### ~~B12-S3: Atajos de teclado globales [MEDIA]~~ ✔ Aplicada

Los proyectos de texto (editor, calculadora) se benefician de manejo de atajos de
teclado. El plan cubre `egui input` brevemente pero un tópico dedicado a:
- ctx.input() para detectar teclas
- Combinaciones (Ctrl+S, Ctrl+Z)
- Conflictos entre input de texto y atajos

...sería valioso antes de los proyectos de la Fase 2.

---

## Bloque 13: Multimedia con GStreamer

### ~~B13-S1: Instalación de GStreamer en contenedores vs host [ALTA]~~ ✔ Aplicada

GStreamer tiene muchas dependencias y plugins. El plan dice "se ejecuta en el host",
pero no detalla la instalación. Sugiero un tópico explícito de:
- Paquetes necesarios en Debian y en Fedora/Alma
- gstreamer1.0-plugins-{base,good,bad,ugly}: qué contiene cada uno y licencias
- Verificación con gst-inspect-1.0 y gst-launch-1.0
- Cómo el link entre Rust (gstreamer-rs) y las librerías nativas funciona

### ~~B13-S2: Subtítulos [BAJA]~~ ✔ Aplicada

Los reproductores de video no mencionan subtítulos. Para un reproductor profesional
sería relevante, pero dado que el objetivo es aprender GStreamer+egui, no es
prioritario. Mencionarlo como posible extensión es suficiente.

---

## Bloque 14: Interoperabilidad y WebAssembly

### ~~B14-S1: WASM más allá del navegador [MEDIA]~~ ✔ Aplicada

El plan cubre WASI brevemente, pero el ecosistema WASM fuera del navegador está
creciendo rápido (Wasmtime, Wasmer, WasmEdge). Sugiero expandir:
- Ejecutar WASM en el servidor con Wasmtime
- WASM como formato de plugin portable (ej: filtros en un pipeline)
- Comparación con contenedores Docker para aislamiento

### ~~B14-S2: wasm-pack vs trunk [BAJA]~~ ✔ Aplicada

Para proyectos web en Rust, trunk es una alternativa a wasm-pack que simplifica el
bundling. Mencionarlo como alternativa sería útil para el estudiante que continúe
hacia desarrollo web con Rust.

---

## Proyectos: Sugerencias Adicionales

### ~~P-S1: Proyecto de red en C/Rust [MEDIA]~~ ✔ Aplicada

Los proyectos están enfocados en GUI y multimedia. Faltan proyectos que apliquen
conocimientos de red:
- Opción A: Chat TCP (C y/o Rust) — cliente + servidor, multi-cliente con threads/async
- Opción B: Monitor de red simple — captura de paquetes con raw sockets, muestra
  estadísticas en terminal

Esto conectaría los bloques 6/7 (sistemas) con el bloque 9 (redes).

### ~~P-S2: Proyecto de administración de sistemas [BAJA]~~ ✔ Aplicada

Un proyecto que integre conocimientos de Linux admin:
- Tool tipo "system health check": disk usage, memory, services status, failed units
- Interfaz: puede ser TUI (ratatui) o GUI (egui)
- Conecta bloques 2, 8, 11

### ~~P-S3: Orden de proyectos GUI y prerrequisitos [ALTA]~~ ✔ Aplicada

El plan coloca los capítulos teóricos de egui (Painter, estado, serialización)
entre los proyectos de las fases. Sugiero verificar que el orden sea estrictamente
progresivo:

1. Introducción a egui (C01) → teoría base
2. Calculadoras (C02) y Reloj (C03) → Fase 1, solo necesitan widgets básicos
3. Estado y persistencia (C04) → teoría de estado
4. Editor, Todo, Tablas (C05-C07) → Fase 2, necesitan estado + archivos
5. Painter y gráficos (C08) → teoría de dibujo
6. Dibujo y Apps de imágenes (C09-C10) → Fase 3, necesitan Painter

El orden actual en el PLAN.md ya sigue esta progresión. La sugerencia es confirmar
que se mantenga durante la implementación.
