# T02 — Init / PID 1

## Errata y aclaraciones sobre el material original

### Error: "Solo SIGKILL puede matar PID 1" y "kill -KILL 1 → Kernel panic"

README.md líneas 79 y 85:

```bash
# Solo SIGKILL puede matar PID 1, y solo en ciertos contextos.  ← INCORRECTO
kill -KILL 1     # Kernel panic (nunca hacer esto)               ← INCORRECTO
```

La protección del kernel para PID 1 es **más fuerte** de lo que indica el
README. SIGKILL también es bloqueada para el init de cada PID namespace
**desde dentro** de ese namespace:

| Acción | Desde dentro del namespace | Desde el namespace padre |
|---|---|---|
| `kill -TERM 1` | Sin efecto (si no hay handler) | Sin efecto (si no hay handler) |
| `kill -9 1` | **Sin efecto** (bloqueada) | **Mata PID 1** (force=true) |
| PID 1 se crashea solo | Kernel panic (host) / container stops | — |

En el host: `kill -9 1` es **silenciosamente descartada**, no causa kernel
panic. El kernel panic solo ocurre si PID 1 termina por su cuenta (crash,
segfault, bug).

En un contenedor: `kill -9 1` desde dentro tampoco funciona. Solo `docker kill`
(que envía SIGKILL desde el namespace del host) puede matar PID 1 del
contenedor.

### Error: ejercicio 3 — systemd-cgls en contenedores

El ejercicio 3 del README.md usa `systemd-cgls` y busca `sshd`, que no están
disponibles en contenedores (no hay systemd). Reemplazado con alternativas que
funcionan en ambos contextos.

### Aclaración: shell form y exec

El README dice que shell form (`CMD python3 app.py`) siempre crea un shell
como PID 1. Esto es **generalmente cierto**, pero algunos shells (dash, bash)
optimizan ejecutando `exec` cuando el comando es simple (un solo comando
sin pipes ni `&&`). El comportamiento depende de la implementación del shell.

La recomendación de usar exec form sigue siendo correcta como práctica
segura.

### Aclaración: señales desde fuera del namespace

La explicación en los labs dice "Solo SIGKILL funciona siempre contra PID 1".
Precisión: SIGKILL funciona contra PID 1 solo **desde el namespace padre**
(como hace `docker kill`). Desde dentro del container, ni SIGKILL funciona.

---

## Qué es PID 1

PID 1 es el **primer proceso de espacio de usuario** que el kernel lanza
después de completar su inicialización:

```
Secuencia de arranque:
  BIOS/UEFI → bootloader (GRUB) → kernel → PID 1

El kernel:
  1. Monta el rootfs (o initramfs)
  2. Ejecuta /sbin/init (o lo que indique init= en la línea del kernel)
  3. /sbin/init se convierte en PID 1
  4. PID 1 inicia todo lo demás
```

En distribuciones modernas (Debian 8+, RHEL 7+, Ubuntu 15.04+), PID 1 es
**systemd**:

```bash
ls -la /sbin/init
# lrwxrwxrwx ... /sbin/init -> /lib/systemd/systemd

ps -p 1 -o pid,cmd
# PID CMD
#   1 /usr/lib/systemd/systemd --switched-root --system --deserialize ...
```

### PID 1 y PID 2

```bash
ps -eo pid,ppid,cmd | head -5
# PID PPID CMD
#   1    0 /usr/lib/systemd/systemd    ← primer proceso de USUARIO
#   2    0 [kthreadd]                  ← padre de kernel threads
#   3    2 [rcu_gp]                    ← kernel thread (hijo de PID 2)
#   4    2 [rcu_par_gp]               ← kernel thread (hijo de PID 2)

# Ambos tienen PPID 0 (el kernel los creó)
# Los kernel threads (entre corchetes) siempre descienden de PID 2
# Los procesos de usuario siempre descienden de PID 1
```

## Responsabilidades de PID 1

### 1. Inicialización del sistema

```
systemd como PID 1:
  1. Monta filesystems (/proc, /sys, /dev, los de /etc/fstab)
  2. Configura el hostname
  3. Activa swap
  4. Inicia servicios en paralelo según dependencias
  5. Levanta la red
  6. Lanza las sesiones de login (getty, sshd, display manager)
```

### 2. Reaping de procesos huérfanos

La responsabilidad más relevante para gestión de procesos. Cuando un padre
muere, sus hijos son re-parenteados a PID 1 (o subreaper). PID 1 **debe**
hacer `wait()` por estos huérfanos para evitar zombies acumulados:

```
Proceso padre (PID 500) muere
  └── hijo huérfano (PID 501)
        ↓
      re-parenteado a PID 1
        ↓
      cuando PID 501 termina, PID 1 hace wait()
        ↓
      PID 501 se limpia correctamente (no queda zombie)
```

SysVinit, Upstart y systemd implementan este reaping. Un programa arbitrario
como PID 1 (en contenedores) **no lo hace automáticamente** — de ahí la
necesidad de tini.

### 3. Gestión de señales

PID 1 tiene un tratamiento **especial** de señales en el kernel:

```
Protección del kernel para PID 1:

  Señales con handler registrado → SE entregan
  Señales sin handler (SIG_DFL)  → DESCARTADAS silenciosamente
  SIGKILL desde dentro           → DESCARTADA (protección absoluta)
  SIGKILL desde namespace padre  → SE entrega (mata PID 1)

  ¿Por qué? Si PID 1 muere → kernel panic en host / container stops
  Esta protección evita muertes accidentales.
```

```bash
# En el host:
kill -TERM 1     # Sin efecto (systemd tiene handler, pero no muere)
kill -9 1        # Sin efecto (bloqueada por el kernel)

# En contenedores (desde dentro):
kill -TERM 1     # Sin efecto si PID 1 no tiene handler para SIGTERM
kill -9 1        # Sin efecto (bloqueada por el kernel)

# Desde el host al contenedor:
docker kill <ctr>  # Envía SIGKILL desde namespace padre → mata PID 1
docker stop <ctr>  # SIGTERM → espera 10s → SIGKILL desde namespace padre
```

### 4. Transiciones de estado del sistema

```bash
# systemd como PID 1 gestiona:
systemctl poweroff     # apagado ordenado
systemctl reboot       # detiene servicios y reinicia
systemctl isolate multi-user.target   # cambio de target

# Apagado ordenado:
# 1. systemd envía SIGTERM a todos los servicios
# 2. Espera TimeoutStopSec (default 90s)
# 3. Envía SIGKILL a los que no terminaron
# 4. Desmonta filesystems
# 5. Envía señal al kernel para apagar/reiniciar
```

## Historia de init systems

### SysVinit (1983–2010s)

```bash
# El init original de System V Unix
# Configuración: /etc/inittab
# Runlevels: 0 (halt), 1 (single), 2-5 (multi-user), 6 (reboot)
# Scripts en /etc/init.d/ ejecutados SECUENCIALMENTE

# Arranque secuencial — lento:
# /etc/init.d/networking start   (espera...)
# /etc/init.d/sshd start         (espera...)
# /etc/init.d/apache2 start      (espera...)
# Un servicio lento bloqueaba a todos los siguientes
```

### Upstart (2006–2015)

```bash
# Event-driven: servicios se inician al ocurrir eventos
# Mejor paralelismo que SysVinit
# Usado por Ubuntu; reemplazado por systemd en Ubuntu 15.04
```

### systemd (2010–presente)

El estándar actual en prácticamente todas las distribuciones mayores:

```bash
# Arranque paralelo con grafo de dependencias
# Socket activation: servicio se inicia al recibir primera conexión
# Cgroups: cada servicio en su propio cgroup
# Journal: logging centralizado y estructurado
# Unit files: declarativos, no scripts imperativos
```

## systemd — Funcionalidades clave

### Arranque paralelo

```
SysVinit (secuencial):
  [networking]──[sshd]──[nginx]──[postgres]──[app]
  Total: suma de tiempos individuales

systemd (paralelo):
  [networking]─┐
  [sshd]───────┤
  [nginx]──────┼── [app] (espera solo a sus dependencias)
  [postgres]───┘
  Total: el camino más largo del grafo
```

```bash
# Ver cuánto tardó el arranque
systemd-analyze
# Startup finished in 2.5s (kernel) + 4.1s (userspace) = 6.6s

# Qué servicios tardaron más
systemd-analyze blame | head -5

# Cadena de dependencias crítica (el "camino más largo")
systemd-analyze critical-chain

# Diagrama SVG del arranque
systemd-analyze plot > boot.svg
```

### Socket activation

```bash
# systemd escucha en un socket ANTES de iniciar el servicio
# Al recibir la primera conexión → inicia el servicio y le pasa el socket
#
# Ventajas:
# - Servicio se inicia bajo demanda (ahorra recursos)
# - Socket acepta conexiones durante el arranque (las encola)
# - Permite arranque paralelo sin importar orden de dependencias
```

### Cgroups por servicio

```bash
# Cada servicio corre en su propio cgroup
systemd-cgls
# Control group /:
# -.slice
# ├─init.scope
# │ └─1 /usr/lib/systemd/systemd
# ├─system.slice
# │ ├─sshd.service
# │ │ └─842 /usr/sbin/sshd -D
# │ ├─nginx.service
# │ │ ├─1200 nginx: master process
# │ │ └─1201 nginx: worker process

# Permite:
# - Matar un servicio y TODOS sus procesos hijos
# - Limitar recursos (CPU, memoria) por servicio
# - Contabilizar uso de recursos por servicio
```

## PID 1 en contenedores

Dentro de un contenedor, el entrypoint se convierte en PID 1 del PID namespace.
Esto causa problemas si el proceso no está diseñado para ser PID 1:

### El problema

```bash
# Dockerfile típico:
# CMD ["python3", "app.py"]
#
# python3 es PID 1 dentro del contenedor
#
# Problemas:
# 1. No hace wait() por hijos → zombies se acumulan
# 2. No tiene handler para SIGTERM → docker stop tarda 10s
#    (espera timeout y luego SIGKILL desde namespace padre)
# 3. Si lanza subprocesos que mueren, quedan como zombies
```

### Solución: tini / dumb-init

```bash
# tini es un PID 1 mínimo diseñado para contenedores
# Hace exactamente dos cosas:
# 1. Reenvía señales al proceso hijo
# 2. Hace wait() para recoger zombies

# Docker --init (usa tini internamente)
docker run --init myimage

# Explícitamente en Dockerfile
# FROM debian:12
# RUN apt-get update && apt-get install -y tini
# ENTRYPOINT ["/usr/bin/tini", "--"]
# CMD ["python3", "app.py"]
```

### Shell form vs exec form

```bash
# MAL: shell form (crea un shell como PID 1)
# CMD python3 app.py
#
# Docker lo ejecuta como: /bin/sh -c "python3 app.py"
# PID 1 = /bin/sh, PID 2 = python3
# El shell no reenvía señales → python3 nunca recibe SIGTERM

# BIEN: exec form (python3 es PID 1 directamente)
# CMD ["python3", "app.py"]

# MEJOR: exec form con tini
# ENTRYPOINT ["/usr/bin/tini", "--"]
# CMD ["python3", "app.py"]
```

### exec en scripts de entrypoint

Cuando el entrypoint del contenedor es un script shell, es fundamental usar
`exec` para reemplazar el shell con la aplicación:

```bash
#!/bin/bash
# entrypoint.sh — script de inicialización

# Configurar variables de entorno, etc.
export DB_HOST="${DB_HOST:-localhost}"

# exec REEMPLAZA el shell con la aplicación
# Sin exec: shell es PID 1, app es PID 2 → señales no llegan
# Con exec: app es PID 1 → recibe señales directamente
exec python3 app.py "$@"
```

### trap en scripts como PID 1

Si el script necesita hacer cleanup antes de salir, `trap` permite manejar
señales:

```bash
#!/bin/bash
# entrypoint con cleanup

cleanup() {
    echo "Recibido SIGTERM, limpiando..."
    kill -TERM "$CHILD_PID" 2>/dev/null
    wait "$CHILD_PID"
    exit 0
}
trap cleanup TERM INT

python3 app.py &
CHILD_PID=$!
wait "$CHILD_PID"
```

> Este patrón (trap + wait + forward) es esencialmente lo que hace tini.
> Si necesitas cleanup personalizado, usa este patrón. Si no, usa tini.

## Comparación PID 1: host vs contenedor

| Aspecto | Host (systemd) | Contenedor (sin tini) | Contenedor (con tini) |
|---|---|---|---|
| PID 1 | systemd | app directamente | tini |
| Reaping | Sí | No | Sí |
| Señales | Handlers registrados | Depende de la app | tini reenvía |
| SIGTERM | Shutdown ordenado | Ignorada (10s + SIGKILL) | Reenviada a app |
| Zombies | Limpiados | Se acumulan | Limpiados |

---

## Labs

### Parte 1 — Inspeccionar PID 1

#### Paso 1.1: Qué es PID 1

```bash
echo "=== PID 1 en este sistema ==="
ps -p 1 -o pid,ppid,user,stat,cmd

echo ""
echo "=== Información de /proc ==="
grep -E '^(Name|Pid|PPid|Uid|Threads):' /proc/1/status

echo ""
echo "=== Ejecutable ==="
readlink /proc/1/exe 2>/dev/null || echo "(no disponible)"
```

#### Paso 1.2: PID 1 y PID 2

```bash
echo "=== Primeros procesos ==="
ps -eo pid,ppid,cmd | head -5

echo ""
echo "PID 1 = primer proceso de USUARIO (init/systemd/entrypoint)"
echo "PID 2 = kthreadd (padre de kernel threads, solo en host)"
echo "Ambos tienen PPID 0 (creados por el kernel)"
```

#### Paso 1.3: Árbol desde PID 1

```bash
echo "=== Árbol de procesos ==="
pstree -p 1

echo ""
echo "Todos los procesos del sistema descienden de PID 1"
echo "Si PID 1 muere → kernel panic (host) o container stops"
```

#### Paso 1.4: PPID 0 — exclusivo del kernel

```bash
echo "=== PPID de PID 1 ==="
grep '^PPid:' /proc/1/status

echo ""
echo "=== ¿Quién más tiene PPID 0? ==="
ps -eo pid,ppid,cmd | awk '$2 == 0'
echo ""
echo "Solo PID 1 y PID 2 tienen PPID 0 (creados por el kernel)"
```

### Parte 2 — Reaping de huérfanos

#### Paso 2.1: PID 1 adopta huérfanos

```bash
bash -c 'sleep 120 & echo "hijo=$! padre=$$"; exit' &
wait $!
sleep 1

CHILD=$(pgrep -nf "sleep 120")
if [ -n "$CHILD" ]; then
    echo "=== Hijo adoptado por PID 1 ==="
    ps -o pid,ppid,stat,cmd -p $CHILD
    echo ""
    echo "PPID = 1 confirma la adopción"
fi

pkill -f "sleep 120" 2>/dev/null
```

#### Paso 2.2: PID 1 limpia automáticamente

```bash
echo "Zombies antes: $(ps -eo stat --no-headers | grep -c '^Z')"

# Crear huérfano que termina inmediatamente
bash -c 'bash -c "exit 42" & exit' &
wait $!
sleep 1

echo "Zombies después: $(ps -eo stat --no-headers | grep -c '^Z')"
echo ""
echo "No hay zombie: PID 1 hizo wait() automáticamente"
echo "Este es el rol clave de PID 1: recoger huérfanos muertos"
```

### Parte 3 — Protección de señales

#### Paso 3.1: SIGTERM a PID 1 vs proceso normal

```bash
echo "=== Proceso normal: SIGTERM lo mata ==="
sleep 300 &
PID=$!
kill -TERM $PID
sleep 0.5
ps -o pid,stat -p $PID 2>/dev/null || echo "PID $PID: muerto (esperado)"
wait $PID 2>/dev/null

echo ""
echo "=== PID 1: SIGTERM no lo mata ==="
kill -TERM 1 2>&1
ps -o pid,stat -p 1
echo "PID 1 sigue vivo (protegido por el kernel)"
```

#### Paso 3.2: SIGKILL a PID 1 desde dentro

```bash
echo "=== SIGKILL a PID 1 desde dentro ==="
kill -9 1 2>&1
ps -o pid,stat -p 1
echo ""
echo "PID 1 sigue vivo"
echo "SIGKILL también es bloqueada para PID 1 desde dentro del namespace"
echo "Solo funciona desde el namespace padre (docker kill)"
```

#### Paso 3.3: Tabla resumen

```bash
echo "=== Protección de señales de PID 1 ==="
printf "%-25s %-15s %-15s\n" "Señal" "Desde dentro" "Desde padre"
printf "%-25s %-15s %-15s\n" "-----------------------" "-------------" "-------------"
printf "%-25s %-15s %-15s\n" "SIGTERM (sin handler)" "Descartada" "Descartada"
printf "%-25s %-15s %-15s\n" "SIGTERM (con handler)" "Entregada" "Entregada"
printf "%-25s %-15s %-15s\n" "SIGKILL" "Descartada" "Mata PID 1"
printf "%-25s %-15s %-15s\n" "SIGSTOP" "Descartada" "Detiene PID 1"
```

---

## Ejercicios

### Ejercicio 1 — Identificar PID 1

```bash
# a) ¿Qué proceso es PID 1?
ps -p 1 -o pid,ppid,user,stat,cmd

# b) ¿Es systemd?
readlink /proc/1/exe 2>/dev/null

# c) ¿Cuál es su PPID?
grep '^PPid:' /proc/1/status

# d) ¿Cuántos threads tiene?
grep '^Threads:' /proc/1/status
```

<details><summary>Predicción</summary>

a) Depende del contexto:
- En el host: `/usr/lib/systemd/systemd` (o similar)
- En un contenedor: el entrypoint (`bash`, `python3`, etc.)

b) En el host: apunta a `systemd`. En un contenedor: apunta al binario del
entrypoint.

c) PPID = **0**. PID 1 fue creado directamente por el kernel. Solo PID 1 y
PID 2 (kthreadd) tienen PPID 0.

d) systemd en el host tiene múltiples threads (típicamente 1+). En un
contenedor, depende del proceso.

</details>

### Ejercicio 2 — Árbol de procesos y kthreadd

```bash
# a) Ver los primeros procesos del sistema
ps -eo pid,ppid,cmd | head -10

# b) ¿Cuántos procesos tienen PPID 1 vs PPID 2?
echo "Hijos de PID 1 (procesos de usuario): $(ps -eo ppid --no-headers | grep -c '^ *1$')"
echo "Hijos de PID 2 (kernel threads):      $(ps -eo ppid --no-headers | grep -c '^ *2$')"

# c) ¿Cómo distinguir kernel threads?
echo ""
echo "Kernel threads (entre corchetes):"
ps -eo pid,ppid,cmd | awk '$2 == 2 {print}' | head -5
```

<details><summary>Predicción</summary>

a) PID 1 = init/systemd, PID 2 = [kthreadd], PIDs 3+ = kernel threads (hijos
de PID 2) y procesos de usuario (hijos de PID 1).

b) PID 2 tiene **muchos más** hijos directos que PID 1. Los kernel threads
(rcu, kworker, migration, etc.) son decenas. PID 1 tiene relativamente pocos
hijos directos (los servicios principales).

c) Los kernel threads se muestran entre corchetes: `[rcu_gp]`, `[kworker/0:0]`,
etc. Siempre tienen PPID 2. No tienen un ejecutable en `/proc/PID/exe`.

En un contenedor, PID 2 no es kthreadd — es simplemente el segundo proceso
del contenedor. Los kernel threads no son visibles dentro del PID namespace
del contenedor.

</details>

### Ejercicio 3 — PPID 0: exclusivo del kernel

```bash
# a) ¿Qué procesos tienen PPID 0?
echo "Procesos con PPID 0:"
ps -eo pid,ppid,cmd | awk '$2 == 0'

# b) ¿Puede un proceso de usuario tener PPID 0?
echo ""
echo "Intentar crear un proceso con PPID 0:"
echo "(no es posible — PPID 0 es exclusivo del kernel)"

# c) ¿Qué pasa con PPID cuando el padre muere?
bash -c 'sleep 60 & echo "hijo=$!"; exit' &
wait $!
sleep 0.5
CHILD=$(pgrep -nf "sleep 60")
echo ""
echo "Huérfano PPID: $(ps -o ppid= -p $CHILD | tr -d ' ')"
echo "(re-parenteado a PID 1, no a PID 0)"
pkill -f "sleep 60" 2>/dev/null
```

<details><summary>Predicción</summary>

a) Solo PID 1 y PID 2 tienen PPID 0. En un contenedor, solo PID 1 (no hay
kthreadd visible).

b) No es posible. PPID 0 significa "creado por el kernel". Ningún proceso de
usuario puede tener PPID 0.

c) El huérfano tiene PPID = **1** (no 0). Cuando un padre muere, los hijos se
re-parentean a PID 1 (o al subreaper más cercano), nunca a PID 0. PID 0 es
exclusivo del kernel como creador de PID 1 y PID 2.

</details>

### Ejercicio 4 — PID 1 como reaper de zombies

```bash
# a) Crear zombie con padre vivo (PID 1 NO interviene aún)
bash -c '
    bash -c "exit 0" &
    echo "PADRE=$$"
    sleep 30
' &
PARENT=$!
sleep 1

echo "Zombie con padre vivo:"
ps --ppid $PARENT -o pid,ppid,stat --no-headers

# b) Matar al padre → PID 1 adopta y limpia
kill $PARENT
wait $PARENT 2>/dev/null
sleep 0.5

echo ""
echo "Zombies después de matar padre:"
ps -eo stat --no-headers | grep -c '^Z'
```

<details><summary>Predicción</summary>

a) Hay un zombie (estado Z) con PPID = $PARENT. El padre está vivo pero no
hace `wait()`, así que PID 1 no interviene — el padre es responsable.

b) 0 zombies. La cadena:
1. Matar padre → padre muere
2. Zombie queda huérfano → re-parenteado a PID 1
3. PID 1 detecta nuevo hijo adoptado que ya está muerto → hace `wait()`
4. Zombie desaparece

PID 1 solo interviene cuando un proceso se convierte en **su hijo**. Mientras
el padre original esté vivo, PID 1 no tiene responsabilidad sobre sus nietos.

</details>

### Ejercicio 5 — Protección de señales de PID 1

```bash
# a) SIGTERM a un proceso normal
sleep 300 &
PID_NORMAL=$!
kill -TERM $PID_NORMAL
sleep 0.5
echo "Normal tras SIGTERM:"
ps -o pid,stat -p $PID_NORMAL 2>/dev/null || echo "  Muerto"

# b) SIGTERM a PID 1
kill -TERM 1 2>&1
echo "PID 1 tras SIGTERM:"
ps -o pid,stat -p 1

# c) SIGKILL a PID 1 (desde dentro)
kill -9 1 2>&1
echo "PID 1 tras SIGKILL:"
ps -o pid,stat -p 1

# d) Ver handlers de señales de PID 1
echo ""
echo "SigCgt de PID 1:"
grep SigCgt /proc/1/status 2>/dev/null
```

<details><summary>Predicción</summary>

a) Muerto. Un proceso normal recibe SIGTERM y muere (acción por defecto:
terminar).

b) PID 1 sigue vivo. El kernel descarta SIGTERM porque estamos **dentro** del
mismo PID namespace. Si PID 1 tiene handler para SIGTERM (systemd lo tiene),
lo procesa pero no muere. Si no tiene handler, la señal es silenciosamente
descartada.

c) PID 1 **sigue vivo**. SIGKILL también es bloqueada para PID 1 desde dentro
del namespace. Esta es la protección más fuerte del kernel — sin ella, un
`kill -9 1` accidental tumbaría el sistema.

d) SigCgt muestra las señales para las que PID 1 tiene handler. systemd
registra handlers para muchas señales. Solo las señales en SigCgt serán
procesadas por PID 1.

</details>

### Ejercicio 6 — ¿Quién puede matar a PID 1?

```bash
# a) Verificar que estamos en un PID namespace
echo "Mi PID: $$"
echo "PID 1: $(ps -o cmd= -p 1)"

# b) SigIgn y SigCgt de PID 1
echo ""
echo "Señales ignoradas (SigIgn):"
grep SigIgn /proc/1/status 2>/dev/null
echo "Señales capturadas (SigCgt):"
grep SigCgt /proc/1/status 2>/dev/null

# c) Intentar varias señales
for SIG in TERM INT HUP USR1 QUIT; do
    kill -$SIG 1 2>/dev/null
    echo "kill -$SIG 1: PID 1 $(ps -o stat= -p 1 2>/dev/null && echo 'vivo' || echo 'muerto')"
done
```

<details><summary>Predicción</summary>

b) SigCgt depende de qué es PID 1:
- systemd: captura muchas señales (SIGTERM, SIGHUP, SIGINT, etc.)
- bash (en contenedor): captura pocas o ninguna por defecto
- tini: captura las necesarias para reenviarlas

c) Todas las señales: PID 1 sigue **vivo**. Ninguna señal enviada desde dentro
del namespace puede matar a PID 1.

Para señales en SigCgt: PID 1 las recibe y procesa (pero no muere).
Para señales fuera de SigCgt: el kernel las descarta silenciosamente.

Solo `docker kill` (o `kill` desde el host al PID real del contenedor) puede
matar PID 1, porque envía la señal desde el **namespace padre** con
force=true.

</details>

### Ejercicio 7 — Shell form vs exec form

```bash
# a) Shell form: shell como intermediario
bash -c 'sleep 300' &
PID_SHELL=$!
sleep 0.5
echo "Shell form:"
echo "  bash PID: $PID_SHELL"
echo "  Hijos:"
ps --ppid $PID_SHELL -o pid,cmd --no-headers 2>/dev/null
echo "  (bash es padre, sleep es hijo)"

kill $PID_SHELL
wait $PID_SHELL 2>/dev/null

# b) Exec form: exec reemplaza al shell
bash -c 'exec sleep 301' &
PID_EXEC=$!
sleep 0.5
echo ""
echo "Exec form:"
echo "  PID: $PID_EXEC"
echo "  Proceso:"
ps -o pid,cmd -p $PID_EXEC --no-headers 2>/dev/null
echo "  Hijos:"
ps --ppid $PID_EXEC -o pid,cmd --no-headers 2>/dev/null || echo "  (ninguno)"
echo "  (sleep REEMPLAZÓ a bash, no hay intermediario)"

kill $PID_EXEC
wait $PID_EXEC 2>/dev/null
```

<details><summary>Predicción</summary>

a) Shell form: `bash` tiene PID $PID_SHELL, y `sleep 300` es su hijo con un
PID diferente. Dos procesos: uno intermediario (bash) y uno real (sleep).

b) Exec form: `exec` reemplaza bash por sleep. El PID $PID_EXEC ahora ES
`sleep 301`. No hay proceso intermediario. No tiene hijos.

La diferencia es crucial para contenedores:
- Shell form: señales llegan a bash (PID 1), no a la app
- Exec form: señales llegan directamente a la app

Esto es exactamente lo que pasa con `CMD app` vs `CMD ["app"]` en Docker.

</details>

### Ejercicio 8 — exec en scripts de entrypoint

```bash
# a) Script SIN exec: shell queda como padre
bash -c '
    echo "Script PID: $$"
    sleep 300 &
    CHILD=$!
    echo "Sleep PID: $CHILD"
    wait $CHILD
' &
SCRIPT=$!
sleep 0.5
echo "Sin exec:"
echo "  Script (bash): PID $SCRIPT"
ps --ppid $SCRIPT -o pid,cmd --no-headers 2>/dev/null
kill $SCRIPT; wait $SCRIPT 2>/dev/null

echo ""

# b) Script CON exec: sleep reemplaza al shell
bash -c '
    echo "Script PID: $$"
    exec sleep 302
' &
SCRIPT2=$!
sleep 0.5
echo "Con exec:"
echo "  PID $SCRIPT2 ahora es:"
ps -o pid,cmd -p $SCRIPT2 --no-headers 2>/dev/null
echo "  (sleep reemplazó a bash)"

kill $SCRIPT2; wait $SCRIPT2 2>/dev/null
```

<details><summary>Predicción</summary>

a) Sin exec: bash (PID $SCRIPT) es el padre, sleep es el hijo. Si enviamos
SIGTERM a $SCRIPT, bash muere pero sleep puede quedar huérfano (depende de
si bash propaga la señal).

b) Con exec: PID $SCRIPT2 ahora muestra `sleep 302` (no bash). `exec`
reemplazó el proceso bash por sleep. No hay intermediario. SIGTERM a $SCRIPT2
llega directamente a sleep.

En un Dockerfile:
```
# entrypoint.sh
#!/bin/bash
# ... configuración ...
exec python3 app.py   # ← CRUCIAL: reemplaza shell por app
```

Sin `exec`, el shell sería PID 1 y la app PID 2. Las señales de `docker stop`
no llegarían a la app.

</details>

### Ejercicio 9 — trap para signal handling como PID 1

```bash
# Simular un PID 1 con trap que maneja SIGTERM
bash -c '
    cleanup() {
        echo "  [PID 1] Recibido SIGTERM"
        echo "  [PID 1] Enviando SIGTERM al hijo..."
        kill -TERM $CHILD 2>/dev/null
        wait $CHILD 2>/dev/null
        echo "  [PID 1] Cleanup completo, saliendo"
        exit 0
    }
    trap cleanup TERM

    sleep 300 &
    CHILD=$!
    echo "[PID 1] Hijo lanzado (PID $CHILD)"
    echo "[PID 1] Esperando... (enviar SIGTERM para probar)"
    wait $CHILD
' &
FAKE_PID1=$!
sleep 1

# a) Enviar SIGTERM (simula docker stop)
echo "Enviando SIGTERM a PID $FAKE_PID1..."
kill -TERM $FAKE_PID1
wait $FAKE_PID1 2>/dev/null
echo "Exit status: $?"
```

<details><summary>Predicción</summary>

Se ve la secuencia:
1. `[PID 1] Hijo lanzado (PID ...)`
2. `Enviando SIGTERM...`
3. `[PID 1] Recibido SIGTERM`
4. `[PID 1] Enviando SIGTERM al hijo...`
5. `[PID 1] Cleanup completo, saliendo`
6. `Exit status: 0`

El `trap cleanup TERM` registra un handler para SIGTERM. Cuando llega, el
handler:
1. Reenvía SIGTERM al hijo (sleep)
2. Hace `wait` para recoger al hijo (evita zombie)
3. Sale con exit 0 (shutdown limpio)

Este patrón es lo que necesita un script de entrypoint que actúa como PID 1
en un contenedor. Sin el trap, SIGTERM sería descartada por la protección del
kernel, y `docker stop` tardaría 10s antes de enviar SIGKILL.

</details>

### Ejercicio 10 — Mini-tini: forward + reaping

```bash
# Construir un "mini-tini" que hace las dos cosas clave:
# 1. Reenviar señales al hijo
# 2. Recoger zombies (wait)

bash -c '
    # Forward de señales al hijo
    forward() {
        kill -TERM $CHILD 2>/dev/null
    }
    trap forward TERM INT

    # Lanzar la "app"
    bash -c "
        trap \"echo APP: recibido SIGTERM; exit 0\" TERM
        echo \"APP: corriendo (PID \$\$)\"
        while true; do sleep 1; done
    " &
    CHILD=$!
    echo "TINI: app lanzada (PID $CHILD)"

    # Reaping loop
    while true; do
        wait $CHILD
        EXIT=$?
        if [ $EXIT -ne 127 ]; then
            echo "TINI: app terminó (exit $EXIT)"
            break
        fi
    done
    exit $EXIT
' &
TINI_PID=$!
sleep 1

# a) Verificar estructura
echo "=== Estructura ==="
echo "Mini-tini PID: $TINI_PID"
ps --ppid $TINI_PID -o pid,cmd --no-headers

# b) Enviar SIGTERM (simula docker stop)
echo ""
echo "=== Enviando SIGTERM ==="
kill -TERM $TINI_PID
sleep 1
wait $TINI_PID 2>/dev/null
echo "Exit status: $?"
```

<details><summary>Predicción</summary>

Estructura: mini-tini (bash) es el padre, la "app" (bash con trap) es el hijo.

Al enviar SIGTERM:
1. `TINI: app lanzada (PID ...)` — ya se mostró al inicio
2. `APP: recibido SIGTERM` — la señal fue reenviada por mini-tini
3. `TINI: app terminó (exit 0)` — mini-tini recogió al hijo con wait
4. Exit status: 0

Este es el mismo patrón que implementa `tini` en C:
- `trap` → registra handler para reenviar señales
- `wait $CHILD` → recoge el exit status (reaping)
- `exit $EXIT` → propaga el exit status al caller

En producción, usar `tini` real (compilado en C, maneja edge cases como
SIGCHLD de múltiples hijos). Pero este ejercicio demuestra que el concepto
es simple: **forward + wait**.

</details>
