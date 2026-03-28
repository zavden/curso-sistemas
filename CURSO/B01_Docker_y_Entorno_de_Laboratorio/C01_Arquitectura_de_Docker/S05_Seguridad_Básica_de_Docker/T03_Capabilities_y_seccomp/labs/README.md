# Lab — Capabilities y seccomp

## Objetivo

Entender como Docker usa capabilities de Linux y perfiles seccomp para
restringir lo que un contenedor puede hacer, aprender a manipular
capabilities con `--cap-drop` y `--cap-add`, y comparar el impacto de
`--privileged` frente al uso granular de capabilities.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada: `docker pull alpine:latest`
- Estar en el directorio `labs/` de este topico
- Opcional: `capsh` instalado en el host para decodificar bitmasks

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `Dockerfile.captest` | Imagen con servidor HTTP para probar capabilities de red |

---

## Parte 1 — Capabilities por defecto

### Objetivo

Listar las capabilities que Docker mantiene por defecto en un contenedor
y entender el bitmask que las representa.

### Paso 1.1: Ver las capabilities del proceso principal

```bash
docker run --rm alpine cat /proc/1/status | grep -i cap
```

Salida esperada:

```
CapInh:	0000000000000000
CapPrm:	00000000a80425fb
CapEff:	00000000a80425fb
CapBnd:	00000000a80425fb
CapAmb:	0000000000000000
```

Cada linea representa un conjunto de capabilities:

- **CapInh** (Inheritable): capabilities que se heredan a procesos hijos
- **CapPrm** (Permitted): capabilities que el proceso PUEDE activar
- **CapEff** (Effective): capabilities actualmente activas
- **CapBnd** (Bounding): limite maximo de capabilities posibles
- **CapAmb** (Ambient): capabilities que se pasan a programas sin setuid

### Paso 1.2: Decodificar el bitmask

Si tienes `capsh` instalado en el host:

```bash
capsh --decode=00000000a80425fb 2>/dev/null || echo "capsh no disponible - ver lista abajo"
```

Salida esperada:

```
0x00000000a80425fb=cap_chown,cap_dac_override,cap_fowner,cap_fsetid,cap_kill,cap_setgid,cap_setuid,cap_setpcap,cap_net_bind_service,cap_net_raw,cap_sys_chroot,cap_mknod,cap_audit_write,cap_setfcap
```

Estas son las 14 capabilities que Docker **mantiene** por defecto. Todas las
demas (unas 26 adicionales) estan eliminadas.

### Paso 1.3: Verificar una capability eliminada por defecto

```bash
docker run --rm alpine sh -c 'mount -t tmpfs none /mnt 2>&1'
```

Salida esperada:

```
mount: permission denied (are you root?)
```

`mount` requiere la capability `SYS_ADMIN`, que Docker elimina por
defecto. Aunque el proceso es root (UID 0) dentro del contenedor, no
tiene esta capability.

Antes de continuar, responde mentalmente:

- ?Que capabilities de la lista del Paso 1.2 reconoces?
- ?Por que Docker mantiene `NET_BIND_SERVICE` pero elimina `SYS_ADMIN`?

Intenta responder antes de continuar al siguiente paso.

### Paso 1.4: Comparar con capabilities de --cap-drop ALL

```bash
docker run --rm --cap-drop ALL alpine cat /proc/1/status | grep CapEff
```

Salida esperada:

```
CapEff:	0000000000000000
```

Sin ninguna capability activa. El proceso es root (UID 0) pero no tiene
ningun privilegio especial.

---

## Parte 2 — Eliminar capabilities una a una

### Objetivo

Demostrar el efecto de eliminar capabilities individuales con `--cap-drop`
y observar que operaciones especificas fallan.

### Paso 2.1: NET_RAW — controla ping

```bash
echo "=== Con NET_RAW (default) ==="
docker run --rm alpine ping -c 1 -W 2 127.0.0.1 2>&1 | tail -2

echo ""
echo "=== Sin NET_RAW ==="
docker run --rm --cap-drop NET_RAW alpine ping -c 1 -W 2 127.0.0.1 2>&1
```

Salida esperada:

```
=== Con NET_RAW (default) ===
1 packets transmitted, 1 packets received, 0% packet loss
...

=== Sin NET_RAW ===
ping: permission denied (are you root?)
```

`ping` usa sockets raw, que requieren la capability `NET_RAW`.

### Paso 2.2: CHOWN — controla cambio de propietario

```bash
echo "=== Con CHOWN (default) ==="
docker run --rm alpine sh -c 'touch /tmp/test && chown nobody /tmp/test && echo "chown OK"'

echo ""
echo "=== Sin CHOWN ==="
docker run --rm --cap-drop CHOWN alpine sh -c 'touch /tmp/test && chown nobody /tmp/test 2>&1'
```

Salida esperada:

```
=== Con CHOWN (default) ===
chown OK

=== Sin CHOWN ===
chown: /tmp/test: Operation not permitted
```

### Paso 2.3: SETUID — controla cambio de usuario

```bash
echo "=== Con SETUID (default) ==="
docker run --rm alpine su -s /bin/sh nobody -c 'id' 2>&1

echo ""
echo "=== Sin SETUID ==="
docker run --rm --cap-drop SETUID alpine su -s /bin/sh nobody -c 'id' 2>&1
```

Salida esperada:

```
=== Con SETUID (default) ===
uid=65534(nobody) gid=65534(nobody)

=== Sin SETUID ===
su: can't set uid: Operation not permitted
```

Sin `SETUID`, el proceso no puede cambiar de usuario. Esto impide el
patron de drop de privilegios que usan servicios como nginx.

### Paso 2.4: KILL — controla el envio de senales

```bash
docker run --rm --cap-drop KILL alpine sh -c '
    sleep 100 &
    CHILD_PID=$!
    kill $CHILD_PID 2>&1 || echo "kill fallo"
'
```

Salida esperada:

```
kill fallo
```

Sin `KILL`, el proceso no puede enviar senales a otros procesos (ni
siquiera a sus propios hijos en ciertos contextos).

---

## Parte 3 — Drop ALL + add solo lo necesario

### Objetivo

Aplicar el principio de minimo privilegio: eliminar todas las capabilities
y anadir solo las estrictamente necesarias para que la aplicacion funcione.

### Paso 3.1: Construir la imagen de prueba

```bash
cat Dockerfile.captest
```

Este Dockerfile crea una imagen con un servidor HTTP simple (httpd de
BusyBox) que necesita bindear al puerto 80.

```bash
docker build -f Dockerfile.captest -t lab-captest .
```

### Paso 3.2: Predecir el resultado

Antes de ejecutar, responde mentalmente:

- Si eliminamos TODAS las capabilities, ?podra el servidor bindear al
  puerto 80?
- ?Que capability necesita para bindear puertos menores a 1024?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3: Ejecutar sin capabilities

```bash
docker run --rm --cap-drop ALL lab-captest 2>&1 | head -3
```

Salida esperada (aproximada):

```
httpd: can't bind to port 80: Permission denied
```

Sin capabilities, el servidor no puede bindear al puerto 80. Los puertos
menores a 1024 son privilegiados y requieren `NET_BIND_SERVICE`.

### Paso 3.4: Anadir solo NET_BIND_SERVICE

```bash
docker run -d --name cap-minimal \
    --cap-drop ALL \
    --cap-add NET_BIND_SERVICE \
    lab-captest

docker exec cap-minimal wget -qO- http://127.0.0.1:80 2>/dev/null || echo "Verificando..."
sleep 1
docker exec cap-minimal wget -qO- http://127.0.0.1:80
```

Salida esperada:

```
<html><body>Capability test OK</body></html>
```

El servidor funciona con una sola capability. Verificar las capabilities
activas:

```bash
docker exec cap-minimal cat /proc/1/status | grep CapEff
```

Salida esperada:

```
CapEff:	0000000000000400
```

Solo un bit activo — `NET_BIND_SERVICE` (bit 10).

```bash
docker rm -f cap-minimal
```

### Paso 3.5: Verificar que otras operaciones estan bloqueadas

```bash
docker run --rm --cap-drop ALL --cap-add NET_BIND_SERVICE alpine sh -c '
    echo "--- Intentar ping ---"
    ping -c 1 127.0.0.1 2>&1 || true
    echo ""
    echo "--- Intentar chown ---"
    touch /tmp/test && chown nobody /tmp/test 2>&1 || true
    echo ""
    echo "--- Intentar mount ---"
    mount -t tmpfs none /mnt 2>&1 || true
'
```

Salida esperada:

```
--- Intentar ping ---
ping: permission denied (are you root?)

--- Intentar chown ---
chown: /tmp/test: Operation not permitted

--- Intentar mount ---
mount: permission denied (are you root?)
```

Con `--cap-drop ALL --cap-add NET_BIND_SERVICE`, el contenedor solo puede
bindear puertos privilegiados. Todas las demas operaciones privilegiadas
fallan.

### Paso 3.6: Caso del curso — SYS_PTRACE para depuracion

En el contexto de este curso, usamos herramientas de depuracion como
`strace` y `gdb` que requieren la capability `SYS_PTRACE`:

```bash
echo "=== Sin SYS_PTRACE (default) ==="
docker run --rm alpine sh -c '
    apk add --no-cache strace > /dev/null 2>&1
    strace -c ls / 2>&1 | tail -3
'

echo ""
echo "=== Con SYS_PTRACE ==="
docker run --rm --cap-add SYS_PTRACE alpine sh -c '
    apk add --no-cache strace > /dev/null 2>&1
    strace -c ls / 2>&1 | tail -5
'
```

Salida esperada:

```
=== Sin SYS_PTRACE (default) ===
strace: test_ptrace_get_syscall_info: PTRACE_TRACEME: Operation not permitted
...

=== Con SYS_PTRACE ===
% time     seconds  usecs/call     calls    errors syscall
...
total      ...
```

`SYS_PTRACE` esta eliminada por defecto porque permite inspeccionar otros
procesos. Para los laboratorios de C y debugging de este curso, necesitamos
anadirla explicitamente con `--cap-add SYS_PTRACE`.

---

## Parte 4 — seccomp

### Objetivo

Verificar que Docker usa perfiles seccomp para filtrar syscalls a un nivel
mas bajo que las capabilities, y observar su efecto.

### Paso 4.1: Verificar que seccomp esta activo

```bash
docker info | grep -i seccomp
```

Salida esperada (aproximada):

```
 seccomp
  Profile: builtin
```

Docker tiene un perfil seccomp integrado que bloquea ~44 syscalls
peligrosas.

### Paso 4.2: syscalls bloqueadas por defecto

```bash
echo "=== mount (bloqueada por seccomp) ==="
docker run --rm alpine mount -t tmpfs none /mnt 2>&1

echo ""
echo "=== unshare (bloqueada por seccomp) ==="
docker run --rm alpine unshare --mount sh -c 'echo test' 2>&1
```

Salida esperada:

```
=== mount (bloqueada por seccomp) ===
mount: permission denied (are you root?)

=== unshare (bloqueada por seccomp) ===
unshare: unshare(0x20000): Operation not permitted
```

Estas syscalls estan bloqueadas por el perfil seccomp, ademas de estar
restringidas por capabilities. seccomp es una **capa adicional** de
seguridad.

### Paso 4.3: Deshabilitar seccomp (observar la diferencia)

```bash
echo "=== Con seccomp (default) ==="
docker run --rm alpine mount -t tmpfs none /mnt 2>&1

echo ""
echo "=== Sin seccomp (seccomp=unconfined) pero sin SYS_ADMIN ==="
docker run --rm --security-opt seccomp=unconfined alpine mount -t tmpfs none /mnt 2>&1
```

Salida esperada:

```
=== Con seccomp (default) ===
mount: permission denied (are you root?)

=== Sin seccomp (seccomp=unconfined) pero sin SYS_ADMIN ===
mount: permission denied (are you root?)
```

`mount` sigue fallando porque, ademas de seccomp, la capability
`SYS_ADMIN` esta eliminada. Se necesitan AMBAS cosas para que `mount`
funcione: la capability y que seccomp no bloquee la syscall.

### Paso 4.4: Deshabilitar seccomp Y anadir SYS_ADMIN

```bash
docker run --rm \
    --security-opt seccomp=unconfined \
    --cap-add SYS_ADMIN \
    alpine sh -c 'mount -t tmpfs none /mnt && echo "mount OK" && df -h /mnt | tail -1'
```

Salida esperada:

```
mount OK
tmpfs          ...  /mnt
```

Ahora `mount` funciona porque se anadio la capability `SYS_ADMIN` y se
deshabilito el filtro seccomp. Esto demuestra que capabilities y seccomp
son **capas independientes** de seguridad.

### Paso 4.5: Relacion entre capas de seguridad

Antes de continuar, responde mentalmente:

- ?Que pasa si tienes la capability pero seccomp bloquea la syscall?
- ?Que pasa si seccomp permite la syscall pero no tienes la capability?
- ?Por que Docker usa AMBOS mecanismos?

Intenta responder antes de continuar al siguiente paso.

La respuesta: ambos mecanismos deben permitir la operacion para que
funcione. Docker usa ambos como **defensa en profundidad** — si un
mecanismo falla o es bypaseado, el otro sigue protegiendo.

---

## Parte 5 — `--privileged` vs capabilities especificas

### Objetivo

Comparar el modo `--privileged` (que desactiva todas las restricciones)
con el uso granular de capabilities, y entender por que `--privileged`
debe evitarse.

### Paso 5.1: Capabilities de un contenedor normal

```bash
docker run --rm alpine cat /proc/1/status | grep CapEff
```

Salida esperada:

```
CapEff:	00000000a80425fb
```

### Paso 5.2: Capabilities de un contenedor privilegiado

```bash
docker run --rm --privileged alpine cat /proc/1/status | grep CapEff
```

Salida esperada:

```
CapEff:	000001ffffffffff
```

Todos los bits activos — TODAS las capabilities habilitadas.

### Paso 5.3: Comparar acceso a devices

```bash
echo "=== Devices visibles (normal) ==="
docker run --rm alpine ls /dev | wc -l

echo ""
echo "=== Devices visibles (privileged) ==="
docker run --rm --privileged alpine ls /dev | wc -l
```

Salida esperada (aproximada):

```
=== Devices visibles (normal) ===
~15

=== Devices visibles (privileged) ===
~200+
```

Un contenedor privilegiado ve TODOS los devices del host, incluyendo
discos, GPUs, y dispositivos USB.

### Paso 5.4: Comparar mount

```bash
echo "=== mount (normal) ==="
docker run --rm alpine mount -t tmpfs none /mnt 2>&1

echo ""
echo "=== mount (privileged) ==="
docker run --rm --privileged alpine sh -c 'mount -t tmpfs none /mnt && echo "mount OK"'
```

Salida esperada:

```
=== mount (normal) ===
mount: permission denied (are you root?)

=== mount (privileged) ===
mount OK
```

### Paso 5.5: Comparar con capabilities especificas

En lugar de `--privileged`, se puede usar `--cap-add` para habilitar solo
lo necesario:

```bash
echo "=== Con --privileged ==="
docker run --rm --privileged alpine sh -c '
    mount -t tmpfs none /mnt && echo "mount: OK"
    ping -c 1 -W 1 127.0.0.1 > /dev/null 2>&1 && echo "ping: OK"
    ls /dev | wc -l | xargs -I{} echo "devices: {}"
'

echo ""
echo "=== Con --cap-add SYS_ADMIN (solo lo necesario para mount) ==="
docker run --rm --cap-add SYS_ADMIN --security-opt seccomp=unconfined alpine sh -c '
    mount -t tmpfs none /mnt && echo "mount: OK"
    ping -c 1 -W 1 127.0.0.1 > /dev/null 2>&1 && echo "ping: OK"
    ls /dev | wc -l | xargs -I{} echo "devices: {}"
'
```

Salida esperada (aproximada):

```
=== Con --privileged ===
mount: OK
ping: OK
devices: ~200+

=== Con --cap-add SYS_ADMIN (solo lo necesario para mount) ===
mount: OK
ping: OK
devices: ~15
```

Con `--cap-add SYS_ADMIN`, el `mount` funciona pero los devices siguen
restringidos. `--privileged` abre TODAS las restricciones de golpe.

### Paso 5.6: Resumen de lo que --privileged desactiva

`--privileged` equivale a:

| Restriccion | Estado con --privileged |
|---|---|
| Capabilities | Todas habilitadas |
| seccomp | Desactivado |
| AppArmor/SELinux | Desactivado |
| Devices | Acceso a todos |
| Read-only mounts (/sys, /proc) | Escribibles |

Por eso `--privileged` solo debe usarse cuando es estrictamente necesario
(Docker-in-Docker, acceso a hardware, debugging de kernel).

---

## Limpieza final

```bash
# Eliminar imagenes del lab
docker rmi lab-captest 2>/dev/null

# Eliminar contenedores huerfanos (si quedo alguno por un paso saltado)
docker rm -f cap-default cap-minimal priv-test 2>/dev/null
```

---

## Conceptos reforzados

1. Docker mantiene **14 capabilities** por defecto y elimina unas 26. Las
   eliminadas incluyen las mas peligrosas como `SYS_ADMIN`, `SYS_PTRACE`
   y `NET_ADMIN`.

2. `--cap-drop` elimina capabilities individuales. Eliminar `NET_RAW`
   bloquea `ping`, eliminar `CHOWN` bloquea `chown`, eliminar `SETUID`
   impide cambiar de usuario.

3. El patron de minimo privilegio es `--cap-drop ALL --cap-add <necesaria>`.
   Esto reduce la superficie de ataque al minimo indispensable.

4. **seccomp** opera a nivel de syscalls, independiente de capabilities.
   Ambos mecanismos deben permitir una operacion para que funcione
   (defensa en profundidad).

5. `--privileged` desactiva **todas las restricciones** de seguridad:
   capabilities, seccomp, AppArmor/SELinux, y acceso a devices. Debe
   evitarse excepto en casos justificados (dind, hardware, debugging).

6. Para los laboratorios de C y debugging de este curso, se necesita
   `--cap-add SYS_PTRACE` para usar `strace` y `gdb`. Esto es preferible
   a `--privileged`.
