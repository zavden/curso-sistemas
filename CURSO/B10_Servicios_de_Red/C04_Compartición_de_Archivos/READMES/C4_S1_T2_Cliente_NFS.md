# Cliente NFS

## Índice
1. [Montaje manual](#1-montaje-manual)
2. [Opciones de montaje](#2-opciones-de-montaje)
3. [Montaje persistente con fstab](#3-montaje-persistente-con-fstab)
4. [Autofs — montaje automático bajo demanda](#4-autofs--montaje-automático-bajo-demanda)
5. [systemd.automount](#5-systemdautomount)
6. [Diagnóstico del cliente](#6-diagnóstico-del-cliente)
7. [Rendimiento y tuning](#7-rendimiento-y-tuning)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Montaje manual

### Instalación del cliente

```bash
# Debian/Ubuntu
sudo apt install nfs-common

# RHEL/Fedora
sudo dnf install nfs-utils
```

No necesita ningún servicio activo para montar — el soporte NFS está en el kernel. El paquete proporciona las herramientas userspace (`mount.nfs`, `showmount`, etc.).

### Descubrir exports disponibles

```bash
# Ver qué exporta un servidor
showmount -e 192.168.1.10
```

```
Export list for 192.168.1.10:
/srv/nfs/shared 192.168.1.0/24
/srv/nfs/docs   192.168.1.0/24
```

> **Nota:** `showmount` usa RPCv2 y requiere que el servidor tenga `rpcbind` accesible. Con NFSv4 puro (sin `rpcbind`), `showmount` no funciona — hay que conocer la ruta de antemano.

### Montar un export

```bash
# Crear punto de montaje
sudo mkdir -p /mnt/nfs/shared

# Montar (NFS auto-negocia la versión más alta)
sudo mount -t nfs 192.168.1.10:/srv/nfs/shared /mnt/nfs/shared

# Forzar NFSv4
sudo mount -t nfs4 192.168.1.10:/srv/nfs/shared /mnt/nfs/shared

# Forzar NFSv3
sudo mount -t nfs -o vers=3 192.168.1.10:/srv/nfs/shared /mnt/nfs/shared
```

### Verificar el montaje

```bash
# Ver montajes NFS activos
mount | grep nfs

# Más detallado
df -hT | grep nfs

# Información completa del montaje
cat /proc/mounts | grep nfs
```

Salida típica de `mount | grep nfs`:

```
192.168.1.10:/srv/nfs/shared on /mnt/nfs/shared type nfs4
  (rw,relatime,vers=4.2,rsize=1048576,wsize=1048576,...)
```

### Desmontar

```bash
# Desmontaje normal
sudo umount /mnt/nfs/shared

# Si está "busy" (alguien tiene archivos abiertos)
sudo umount -l /mnt/nfs/shared    # Lazy: se desmonta cuando deje de usarse

# Forzar (puede causar pérdida de datos)
sudo umount -f /mnt/nfs/shared
```

```
┌─────────────────────────────────────────────────────┐
│             Flujo de montaje NFS                     │
│                                                     │
│  Cliente                         Servidor           │
│  ───────                        ─────────           │
│  mount -t nfs server:/path /mnt                     │
│       │                                             │
│       ├─► rpcbind (111) ──► "¿dónde está mountd?"   │
│       │                                             │
│       ├─► mountd (20048) ──► "quiero /path"         │
│       │                     Verifica /etc/exports   │
│       │                     ◄── OK, file handle     │
│       │                                             │
│       └─► nfsd (2049) ──► I/O con file handle       │
│                                                     │
│  NFSv4: todo va directo al 2049, sin rpcbind/mountd │
└─────────────────────────────────────────────────────┘
```

---

## 2. Opciones de montaje

Las opciones se pasan con `-o` en el comando `mount` o en la columna de opciones de `/etc/fstab`.

### Opciones de rendimiento

| Opción | Significado | Default |
|--------|------------|---------|
| `rsize=N` | Tamaño de lectura en bytes | 1048576 (1 MB) |
| `wsize=N` | Tamaño de escritura en bytes | 1048576 (1 MB) |
| `async` | Escrituras asíncronas en el cliente | No |
| `noatime` | No actualizar access time | No |

`rsize` y `wsize` se negocian automáticamente al valor óptimo. Cambiarlos manualmente rara vez mejora el rendimiento en redes modernas.

### Opciones de resiliencia

| Opción | Significado | Default |
|--------|------------|---------|
| `hard` | Reintenta indefinidamente si el servidor no responde | **Sí** |
| `soft` | Devuelve error tras `retrans` intentos | No |
| `timeo=N` | Timeout en décimas de segundo | 600 (60s) |
| `retrans=N` | Número de reintentos (con `soft`) | 2 |
| `intr` | Permite interrumpir operaciones NFS (obsoleto en kernels modernos) | — |

```
┌──────────────────────────────────────────────────────┐
│        hard mount vs soft mount                       │
│                                                      │
│  hard (default):                                     │
│  ┌────────────────────────────────────────────┐      │
│  │ Servidor cae → proceso se congela          │      │
│  │ Servidor vuelve → proceso continúa         │      │
│  │ Datos: SEGUROS (nunca retorna error falso) │      │
│  │ Riesgo: procesos colgados indefinidamente  │      │
│  └────────────────────────────────────────────┘      │
│                                                      │
│  soft:                                               │
│  ┌────────────────────────────────────────────┐      │
│  │ Servidor cae → retorna EIO tras timeout    │      │
│  │ Datos: RIESGO (app recibe error de I/O)    │      │
│  │ Ventaja: procesos no se cuelgan            │      │
│  │ Peligro: escrituras parciales, corrupción  │      │
│  └────────────────────────────────────────────┘      │
│                                                      │
│  Recomendación: usar SIEMPRE hard (default)          │
│  soft solo en montajes de solo lectura no críticos   │
└──────────────────────────────────────────────────────┘
```

### Opciones de seguridad

| Opción | Significado | Default |
|--------|------------|---------|
| `sec=sys` | Autenticación por UID/GID | **Sí** |
| `sec=krb5` | Autenticación Kerberos | No |
| `sec=krb5i` | Kerberos + integridad | No |
| `sec=krb5p` | Kerberos + integridad + cifrado | No |

### Opciones de caché

| Opción | Significado | Default |
|--------|------------|---------|
| `ac` | Habilitar caché de atributos | **Sí** |
| `noac` | Deshabilitar caché de atributos | No |
| `acregmin=N` | Tiempo mínimo de caché para archivos (seg) | 3 |
| `acregmax=N` | Tiempo máximo de caché para archivos (seg) | 60 |
| `acdirmin=N` | Tiempo mínimo de caché para directorios (seg) | 30 |
| `acdirmax=N` | Tiempo máximo de caché para directorios (seg) | 60 |
| `lookupcache=all` | Cachear lookups positivos y negativos | **Sí** |

`noac` fuerza al cliente a verificar cada operación con el servidor — útil cuando múltiples clientes escriben los mismos archivos, pero costoso en rendimiento.

### Opciones de versión

| Opción | Significado |
|--------|------------|
| `vers=3` / `nfsvers=3` | Forzar NFSv3 |
| `vers=4` / `nfsvers=4` | Forzar NFSv4 |
| `vers=4.1` | Forzar NFSv4.1 |
| `vers=4.2` | Forzar NFSv4.2 |
| `proto=tcp` | Usar TCP (default en v4) |
| `proto=udp` | Usar UDP (solo v3) |

---

## 3. Montaje persistente con fstab

### Sintaxis en /etc/fstab

```bash
# servidor:/export    punto_montaje    tipo    opciones    dump    fsck
192.168.1.10:/srv/nfs/shared  /mnt/nfs/shared  nfs  defaults,_netdev  0  0
```

**`_netdev`** es crítico: indica al sistema que este montaje requiere red. Sin esta opción, el sistema intentará montar antes de que la red esté disponible, causando un arranque bloqueado.

### Ejemplos de fstab

```bash
# Montaje básico NFSv4
192.168.1.10:/srv/nfs/shared  /mnt/nfs/shared  nfs4  defaults,_netdev  0  0

# Con opciones de rendimiento
192.168.1.10:/srv/nfs/data  /mnt/data  nfs  rw,hard,_netdev,noatime  0  0

# Solo lectura para documentación
192.168.1.10:/srv/nfs/docs  /mnt/docs  nfs  ro,_netdev,noatime  0  0

# NFSv3 explícito
192.168.1.10:/srv/nfs/legacy  /mnt/legacy  nfs  vers=3,_netdev  0  0

# Con timeout personalizado
192.168.1.10:/srv/nfs/data  /mnt/data  nfs  hard,timeo=300,_netdev  0  0
```

### Probar sin reiniciar

```bash
# Montar todo lo de fstab que aún no está montado
sudo mount -a

# Montar solo una entrada específica (por punto de montaje)
sudo mount /mnt/nfs/shared

# Verificar
df -hT | grep nfs
```

### Problema del arranque bloqueado

```
┌──────────────────────────────────────────────────────────┐
│          Arranque con NFS en fstab                        │
│                                                          │
│  SIN _netdev:                                            │
│  ┌────────────────────────────────────────────┐          │
│  │ 1. Kernel arranca                          │          │
│  │ 2. systemd monta filesystems (local-fs)    │          │
│  │ 3. Intenta montar NFS ← red no está lista  │          │
│  │ 4. mount -t nfs ... → TIMEOUT (90 seg)     │          │
│  │ 5. Arranque bloqueado o falla              │          │
│  └────────────────────────────────────────────┘          │
│                                                          │
│  CON _netdev:                                            │
│  ┌────────────────────────────────────────────┐          │
│  │ 1. Kernel arranca                          │          │
│  │ 2. systemd monta filesystems locales       │          │
│  │ 3. Red se configura (network-online.target) │          │
│  │ 4. systemd monta NFS ← red lista ✓        │          │
│  │ 5. Arranque normal                         │          │
│  └────────────────────────────────────────────┘          │
│                                                          │
│  Alternativa: usar nofail + _netdev                      │
│  nofail = no bloquear arranque si el montaje falla       │
└──────────────────────────────────────────────────────────┘
```

### _netdev + nofail

```bash
# Montaje que no bloquea el arranque aunque el servidor NFS esté caído
192.168.1.10:/srv/nfs/shared  /mnt/nfs/shared  nfs  defaults,_netdev,nofail  0  0
```

`nofail` permite que el sistema arranque normalmente incluso si el servidor NFS no responde. El montaje se puede reintentar manualmente después con `mount /mnt/nfs/shared`.

---

## 4. Autofs — montaje automático bajo demanda

**autofs** monta automáticamente cuando se accede al directorio y desmonta tras un período de inactividad. Ventajas sobre fstab:

- No bloquea el arranque si el servidor está caído
- No consume recursos cuando el montaje no se usa
- Se desmonta solo (por defecto a los 300 segundos de inactividad)
- Puede gestionar múltiples exports con un solo wildcard

### Instalación

```bash
# Debian/Ubuntu
sudo apt install autofs

# RHEL/Fedora
sudo dnf install autofs

# Habilitar e iniciar
sudo systemctl enable --now autofs
```

### Arquitectura de autofs

```
┌───────────────────────────────────────────────────────┐
│                  Arquitectura autofs                   │
│                                                       │
│  /etc/auto.master ──── Mapa maestro                   │
│       │                "¿qué directorio base montar?" │
│       │                                               │
│       ├── /etc/auto.nfs ──── Mapa de montaje          │
│       │     "¿qué subdirectorios y de dónde?"         │
│       │                                               │
│       └── /etc/auto.misc ──── Otro mapa               │
│             "CD-ROM, floppy, etc."                    │
│                                                       │
│  Flujo:                                               │
│  1. Usuario accede a /mnt/nfs/shared                  │
│  2. automount detecta el acceso (inotify del kernel)  │
│  3. Busca "shared" en /etc/auto.nfs                   │
│  4. Monta 192.168.1.10:/srv/nfs/shared                │
│  5. Tras 300s sin uso → desmonta automáticamente      │
└───────────────────────────────────────────────────────┘
```

### Configuración paso a paso

**Paso 1 — Mapa maestro** (`/etc/auto.master` o `/etc/auto.master.d/*.autofs`):

```bash
# /etc/auto.master
# punto_base    mapa              opciones
/mnt/nfs        /etc/auto.nfs     --timeout=600
```

Esto dice: "cuando alguien acceda a algo dentro de `/mnt/nfs/`, consulta `/etc/auto.nfs` para saber qué montar".

**Paso 2 — Mapa de montajes** (`/etc/auto.nfs`):

```bash
# /etc/auto.nfs
# subdirectorio    opciones                          servidor:/export
shared             -rw,sync,hard                     192.168.1.10:/srv/nfs/shared
docs               -ro,sync                          192.168.1.10:/srv/nfs/docs
data               -rw,sync,noatime                  192.168.1.20:/export/data
```

Con esta configuración:
- `cd /mnt/nfs/shared` → monta `192.168.1.10:/srv/nfs/shared`
- `cd /mnt/nfs/docs` → monta `192.168.1.10:/srv/nfs/docs`
- `ls /mnt/nfs/` → **no muestra nada** (los directorios sólo aparecen al accederlos)

**Paso 3 — Reiniciar autofs**:

```bash
sudo systemctl restart autofs
```

### El directorio base NO debe existir

```bash
# IMPORTANTE: autofs gestiona /mnt/nfs/
# NO crear subdirectorios manualmente

# MAL: crear el punto de montaje
sudo mkdir -p /mnt/nfs/shared    # ← autofs no funcionará

# BIEN: solo crear el directorio base si no existe
sudo mkdir -p /mnt/nfs
# Los subdirectorios los crea autofs bajo demanda
```

Si el subdirectorio ya existe como directorio real, autofs no puede montarlo — el directorio existente "oculta" el montaje automático.

### Wildcard map (montaje con comodín)

Para montar automáticamente **cualquier** export del servidor sin listarlos uno a uno:

```bash
# /etc/auto.nfs
# El asterisco (*) y el ampersand (&) son clave
*    -rw,sync,hard    192.168.1.10:/srv/nfs/&
```

- `*` coincide con cualquier nombre de subdirectorio
- `&` se reemplaza por el nombre coincidente

```bash
# El usuario hace:
cd /mnt/nfs/shared     # → monta 192.168.1.10:/srv/nfs/shared
cd /mnt/nfs/docs       # → monta 192.168.1.10:/srv/nfs/docs
cd /mnt/nfs/cualquier  # → intenta montar 192.168.1.10:/srv/nfs/cualquier
```

### Montaje directo (direct maps)

Para montar en rutas absolutas arbitrarias (no bajo un directorio base):

```bash
# /etc/auto.master
/-    /etc/auto.direct

# /etc/auto.direct
/var/www/uploads    -rw,sync    192.168.1.10:/srv/nfs/www
/opt/shared/lib     -ro,sync    192.168.1.10:/srv/nfs/lib
```

Con `/-` como punto base, cada línea del mapa especifica la ruta **completa** del punto de montaje. El directorio padre debe existir.

### Timeout y desmontaje

```bash
# /etc/auto.master — timeout global
/mnt/nfs    /etc/auto.nfs    --timeout=600

# --timeout=0 deshabilita el desmontaje automático
# (comportamiento similar a fstab, pero sin bloquear arranque)
/mnt/nfs    /etc/auto.nfs    --timeout=0
```

```bash
# Ver qué tiene autofs montado actualmente
mount | grep autofs

# Forzar verificación de desmontaje (debug)
sudo automount -f -v
```

---

## 5. systemd.automount

systemd incluye su propio mecanismo de automount, alternativa a autofs. Es más simple para casos básicos.

### Cómo funciona

Se crean dos units: una `.mount` y una `.automount`. La unit `.automount` vigila el punto de montaje y activa la `.mount` cuando alguien accede.

**El nombre del archivo** debe coincidir con la ruta del punto de montaje, reemplazando `/` por `-` (omitiendo la primera).

```bash
# Punto de montaje: /mnt/nfs/shared
# Nombre del unit: mnt-nfs-shared.mount y mnt-nfs-shared.automount
```

### Crear la unit .mount

```ini
# /etc/systemd/system/mnt-nfs-shared.mount
[Unit]
Description=NFS mount for shared
After=network-online.target
Requires=network-online.target

[Mount]
What=192.168.1.10:/srv/nfs/shared
Where=/mnt/nfs/shared
Type=nfs
Options=rw,hard,noatime

[Install]
WantedBy=multi-user.target
```

### Crear la unit .automount

```ini
# /etc/systemd/system/mnt-nfs-shared.automount
[Unit]
Description=Automount NFS shared
After=network-online.target

[Automount]
Where=/mnt/nfs/shared
TimeoutIdleSec=600

[Install]
WantedBy=multi-user.target
```

### Activar

```bash
# Recargar units
sudo systemctl daemon-reload

# Habilitar el automount (NO el .mount directamente)
sudo systemctl enable --now mnt-nfs-shared.automount

# Verificar estado
sudo systemctl status mnt-nfs-shared.automount
```

### autofs vs systemd.automount

| Característica | autofs | systemd.automount |
|---------------|--------|-------------------|
| Wildcard maps | Sí (`*` / `&`) | No |
| Configuración | Archivos de mapa | Units systemd |
| Timeout | `--timeout` en auto.master | `TimeoutIdleSec` en .automount |
| Mapas LDAP/NIS | Sí | No |
| Complejidad | Mayor | Menor |
| Integración systemd | Parcial | Nativa |

**Regla práctica:** si necesitas wildcard maps o mapas desde LDAP, usa autofs. Para montajes individuales con automount, systemd es más sencillo y no requiere paquete adicional.

---

## 6. Diagnóstico del cliente

### Verificar conectividad con el servidor

```bash
# ¿El servidor responde?
ping -c 3 192.168.1.10

# ¿El puerto NFS está abierto?
nc -zv 192.168.1.10 2049

# ¿Qué exports ofrece? (requiere rpcbind en servidor)
showmount -e 192.168.1.10

# ¿RPC funciona?
rpcinfo -p 192.168.1.10
```

### Verificar montajes activos

```bash
# Montajes NFS activos
mount -t nfs,nfs4

# Con estadísticas de cada montaje
cat /proc/self/mountstats | head -40

# Estadísticas del cliente NFS
nfsstat -c

# Estadísticas por montaje
nfsiostat
```

### Interpretar mountstats

```bash
# Ver estadísticas detalladas de un montaje
grep -A 30 "192.168.1.10" /proc/self/mountstats
```

Campos importantes:

```
device 192.168.1.10:/srv/nfs/shared mounted on /mnt/nfs/shared
  ...
  nfs:  vers=4, minorvers=2, rsize=1048576, wsize=1048576
  events: 150 200 0 0 120 80 350 0 0 50 ...
  bytes:  5242880 1048576 0 0 5242880 1048576 ...
  RPC calls: 350
  retransmissions: 0        ← si > 0, hay problemas de red
  timeouts: 0               ← si > 0, el servidor no responde
```

### Debug detallado

```bash
# Activar debug del cliente NFS en el kernel
sudo rpcdebug -m nfs -s all

# Ver mensajes en tiempo real
sudo dmesg -w | grep -i nfs

# Desactivar debug
sudo rpcdebug -m nfs -c all
```

### Stale file handle

El error **"Stale NFS file handle"** (ESTALE) ocurre cuando el cliente tiene una referencia a un archivo que ya no existe en el servidor (fue borrado, renombrado, o el export cambió).

```bash
# Diagnóstico
ls /mnt/nfs/shared
# ls: cannot access '/mnt/nfs/shared/file.txt': Stale file handle

# Solución: desmontar y remontar
sudo umount -l /mnt/nfs/shared
sudo mount -a

# Si umount falla (busy):
sudo umount -f /mnt/nfs/shared    # Force
# O
sudo umount -l /mnt/nfs/shared    # Lazy unmount
```

---

## 7. Rendimiento y tuning

### Medir rendimiento NFS

```bash
# Escritura secuencial (1 GB)
dd if=/dev/zero of=/mnt/nfs/shared/testfile bs=1M count=1024 oflag=direct

# Lectura secuencial
dd if=/mnt/nfs/shared/testfile of=/dev/null bs=1M

# Con sync para medir escritura real (no cache)
dd if=/dev/zero of=/mnt/nfs/shared/testfile bs=1M count=1024 conv=fsync
```

### Opciones que afectan el rendimiento

```bash
# Montaje optimizado para rendimiento
sudo mount -t nfs -o rw,hard,noatime,rsize=1048576,wsize=1048576,tcp \
    192.168.1.10:/srv/nfs/data /mnt/data
```

| Ajuste | Efecto |
|--------|--------|
| `noatime` | Elimina actualizaciones de access time (menos writes) |
| `rsize/wsize` altos | Bloques grandes = menos RPCs (ya es 1 MB por defecto) |
| `tcp` | Default en v4; más confiable que UDP en v3 |
| `noac` | Consistencia máxima, rendimiento mínimo |
| `nconnect=N` | Múltiples conexiones TCP (kernel 5.3+, hasta 16) |

### nconnect (kernel 5.3+)

```bash
# Usar 4 conexiones TCP en paralelo
sudo mount -t nfs -o nconnect=4 192.168.1.10:/srv/nfs/data /mnt/data
```

`nconnect` distribuye las operaciones NFS entre múltiples conexiones TCP, aprovechando mejor los enlaces de red de alta velocidad (10 Gbps+). No ayuda mucho en redes de 1 Gbps.

---

## 8. Errores comunes

### Error 1 — Olvidar _netdev en fstab

```bash
# MAL — puede bloquear el arranque
192.168.1.10:/srv/nfs/shared  /mnt/shared  nfs  defaults  0  0

# BIEN — espera a que la red esté lista
192.168.1.10:/srv/nfs/shared  /mnt/shared  nfs  defaults,_netdev  0  0

# MEJOR — no bloquea aunque el servidor esté caído
192.168.1.10:/srv/nfs/shared  /mnt/shared  nfs  defaults,_netdev,nofail  0  0
```

Sin `_netdev`, systemd intenta montar NFS durante `local-fs.target`, antes de que la red esté disponible. Resultado: timeout de 90 segundos o arranque fallido.

### Error 2 — Usar soft mount para datos importantes

```bash
# PELIGROSO para escrituras
sudo mount -t nfs -o soft 192.168.1.10:/data /mnt/data

# Si el servidor se pone lento o hay congestión de red:
# - Operaciones de escritura retornan EIO
# - Aplicaciones reciben error de I/O
# - Datos parcialmente escritos → corrupción

# CORRECTO — usar hard (default) siempre para datos rw
sudo mount -t nfs -o hard 192.168.1.10:/data /mnt/data
```

### Error 3 — Crear subdirectorios que autofs debe gestionar

```bash
# autofs monta bajo /mnt/nfs/ según /etc/auto.nfs
# Si creas los subdirectorios manualmente:
sudo mkdir /mnt/nfs/shared    # ← rompe autofs

# El directorio real "tapa" el montaje automático
# Solución: eliminar el directorio y reiniciar autofs
sudo rmdir /mnt/nfs/shared
sudo systemctl restart autofs
```

### Error 4 — Versión NFS incompatible

```bash
# El servidor solo soporta NFSv4 pero el cliente pide v3:
sudo mount -t nfs -o vers=3 server:/path /mnt
# mount.nfs: Protocol not supported

# Diagnóstico: ver qué versiones soporta el servidor
rpcinfo -p 192.168.1.10 | grep nfs

# Solución: dejar que auto-negocie o especificar v4
sudo mount -t nfs 192.168.1.10:/path /mnt
```

### Error 5 — mount se queda colgado indefinidamente

```bash
# mount -t nfs server:/path /mnt → se queda sin responder

# Causas más probables:
# 1. Firewall bloqueando puerto 2049
# 2. Servidor caído
# 3. DNS no resuelve el hostname

# Diagnóstico rápido:
nc -zv -w 3 192.168.1.10 2049    # ¿Puerto abierto? (3s timeout)
getent hosts server.example.com   # ¿DNS resuelve?

# Workaround: mount con timeout
sudo timeout 10 mount -t nfs 192.168.1.10:/path /mnt
# Si retorna 124 → timeout alcanzado
```

---

## 9. Cheatsheet

```bash
# ── Instalación cliente ──────────────────────────────────
apt install nfs-common               # Debian/Ubuntu
dnf install nfs-utils                # RHEL/Fedora

# ── Descubrir exports ────────────────────────────────────
showmount -e SERVER                  # Listar exports (requiere rpcbind)

# ── Montaje manual ───────────────────────────────────────
mount -t nfs  SRV:/path /mnt        # Auto-negocia versión
mount -t nfs4 SRV:/path /mnt        # Forzar NFSv4
mount -t nfs -o vers=3 SRV:/path /mnt  # Forzar NFSv3
umount /mnt                          # Desmontar
umount -l /mnt                       # Lazy unmount (busy)

# ── fstab ─────────────────────────────────────────────────
SRV:/path  /mnt  nfs  defaults,_netdev       0 0   # Básico
SRV:/path  /mnt  nfs  defaults,_netdev,nofail 0 0  # No bloquea arranque
mount -a                             # Montar todo lo de fstab

# ── autofs ────────────────────────────────────────────────
# /etc/auto.master:
/mnt/nfs  /etc/auto.nfs  --timeout=600
# /etc/auto.nfs:
shared  -rw,sync  192.168.1.10:/srv/nfs/shared
*       -rw,sync  192.168.1.10:/srv/nfs/&         # Wildcard
# /etc/auto.master (direct):
/-  /etc/auto.direct

systemctl restart autofs             # Aplicar cambios

# ── systemd.automount ────────────────────────────────────
# mnt-nfs-shared.mount + mnt-nfs-shared.automount
systemctl enable --now mnt-nfs-shared.automount

# ── Diagnóstico ──────────────────────────────────────────
mount -t nfs,nfs4                    # Montajes NFS activos
df -hT | grep nfs                    # Espacio en montajes NFS
nfsstat -c                           # Estadísticas cliente
nfsiostat                            # I/O por montaje
cat /proc/self/mountstats            # Stats detalladas
rpcdebug -m nfs -s all               # Debug kernel (activar)
rpcdebug -m nfs -c all               # Debug kernel (desactivar)

# ── Rendimiento ──────────────────────────────────────────
mount -o nconnect=4 SRV:/p /mnt     # Multi-conexión (kernel 5.3+)
mount -o noatime SRV:/p /mnt        # Sin access time updates
```

---

## 10. Ejercicios

### Ejercicio 1 — Montaje manual y fstab

Monta un export NFS de forma manual, verifica que funciona, y luego hazlo persistente.

```bash
# 1. Verificar qué exporta el servidor
showmount -e 192.168.1.10

# 2. Crear punto de montaje
sudo mkdir -p /mnt/nfs/shared

# 3. Montar manualmente
sudo mount -t nfs 192.168.1.10:/srv/nfs/shared /mnt/nfs/shared

# 4. Verificar
df -hT /mnt/nfs/shared
ls /mnt/nfs/shared/

# 5. Agregar a fstab para persistencia
echo '192.168.1.10:/srv/nfs/shared  /mnt/nfs/shared  nfs  rw,hard,_netdev,nofail  0  0' | sudo tee -a /etc/fstab

# 6. Probar la entrada de fstab
sudo umount /mnt/nfs/shared
sudo mount -a
df -hT /mnt/nfs/shared
```

**Pregunta de predicción:** Si agregas la línea a fstab **sin** `_netdev` y el servidor NFS está apagado cuando reinicias el cliente, ¿qué ocurre durante el arranque?

> **Respuesta:** El arranque se bloquea esperando que el montaje NFS responda. systemd intenta montar durante `local-fs.target` (antes de la red con `_netdev` faltante, pero el problema real es que incluso con red, si el servidor no responde, `hard` mount — el default — reintenta indefinidamente). Con `_netdev,nofail`, systemd espera la red y, si falla el montaje, continúa el arranque normalmente.

---

### Ejercicio 2 — Configurar autofs con wildcard map

Configura autofs para montar automáticamente cualquier export del servidor `192.168.1.10` bajo `/auto/nfs/`.

```bash
# 1. Instalar autofs
sudo apt install autofs   # o dnf install autofs

# 2. Configurar mapa maestro
echo '/auto/nfs    /etc/auto.nfs    --timeout=300' | sudo tee -a /etc/auto.master

# 3. Crear mapa con wildcard
echo '*    -rw,hard,noatime    192.168.1.10:/srv/nfs/&' | sudo tee /etc/auto.nfs

# 4. Reiniciar autofs
sudo systemctl restart autofs

# 5. Probar — acceder a un export
ls /auto/nfs/           # ← vacío (normal)
cd /auto/nfs/shared     # ← monta automáticamente
ls                      # ← contenido del export
mount | grep auto       # ← verificar montaje activo

# 6. Esperar timeout y verificar desmontaje
# Tras 300 segundos sin uso, se desmonta solo
```

**Pregunta de predicción:** Si ejecutas `ls /auto/nfs/`, ¿ves los subdirectorios `shared`, `docs`, etc., o el directorio aparece vacío? ¿Por qué?

> **Respuesta:** Aparece vacío. autofs no lista los posibles montajes porque no sabe cuáles existen — el wildcard `*` coincide con cualquier nombre. Los subdirectorios sólo aparecen cuando se accede directamente a ellos (por ejemplo, `cd /auto/nfs/shared`). Esto es una diferencia respecto a fstab, donde el punto de montaje siempre es visible. Con autofs, hay que conocer el nombre del export.

---

### Ejercicio 3 — Comparar métodos de montaje

Compara los tres métodos de montaje NFS y elige el adecuado para cada escenario.

| Escenario | Método | ¿Por qué? |
|-----------|--------|-----------|
| Servidor de aplicación que siempre necesita `/data` | fstab + `_netdev,nofail` | Montaje permanente, la app depende de él |
| Estaciones de trabajo que acceden a `/home` compartido | autofs wildcard | Muchos usuarios, monta bajo demanda |
| Montaje temporal para copiar datos una vez | `mount` manual | No necesita persistencia |
| Un solo export accedido esporádicamente | systemd.automount | Simple, se desmonta solo |

```bash
# Verificar con qué método está montado un NFS:
# fstab
grep nfs /etc/fstab

# autofs
grep -r "" /etc/auto.master /etc/auto.nfs 2>/dev/null

# systemd.automount
systemctl list-units --type=automount

# Manual (no aparece en ningún archivo de configuración)
mount -t nfs,nfs4
```

**Pregunta de predicción:** Un servidor tiene 50 directorios home exportados (`/export/home/user1`, `/export/home/user2`, ...). ¿Usarías 50 líneas en fstab o un wildcard autofs? ¿Qué pasa si se agrega el usuario 51?

> **Respuesta:** Wildcard autofs (`*  -rw  server:/export/home/&`). Con una sola línea en `auto.home`, cualquier usuario nuevo funciona automáticamente sin tocar configuración. Con fstab serían 50 líneas y habría que agregar una línea cada vez que se crea un usuario — además de ejecutar `mount -a` o reiniciar. autofs fue diseñado exactamente para este caso de uso: directorios home en NFS con usuarios dinámicos.

---

> **Siguiente:** T03 — NFSv4 vs NFSv3 (diferencias, Kerberos, id mapping).
