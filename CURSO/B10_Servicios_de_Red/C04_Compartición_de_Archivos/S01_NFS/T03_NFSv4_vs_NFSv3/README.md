# NFSv4 vs NFSv3

## Índice
1. [Evolución de NFS](#1-evolución-de-nfs)
2. [Diferencias arquitectónicas](#2-diferencias-arquitectónicas)
3. [Protocolo y puertos](#3-protocolo-y-puertos)
4. [Pseudo-filesystem en NFSv4](#4-pseudo-filesystem-en-nfsv4)
5. [ID mapping](#5-id-mapping)
6. [ACLs en NFSv4](#6-acls-en-nfsv4)
7. [Delegaciones](#7-delegaciones)
8. [Seguridad Kerberos](#8-seguridad-kerberos)
9. [Migración de NFSv3 a NFSv4](#9-migración-de-nfsv3-a-nfsv4)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Evolución de NFS

```
1984          1995          2003          2010          2016
 │             │             │             │             │
 v2            v3            v4            v4.1          v4.2
 UDP           TCP/UDP       TCP only      pNFS          Server-side
 Stateless     Stateless     Stateful      Sessions      copy
 32-bit        64-bit        Kerberos      Trunking      Sparse files
 files         files         ACLs          Multipath     Labeled NFS
               Async         Compound                    (SELinux)
               writes        RPCs
```

NFSv4 no es una evolución incremental de NFSv3 — es un **rediseño completo** del protocolo, influenciado por AFS (Andrew File System) y CIFS. Cambió de un modelo stateless con múltiples demonios a un modelo stateful con un único puerto.

---

## 2. Diferencias arquitectónicas

### Tabla comparativa

| Aspecto | NFSv3 | NFSv4 |
|---------|-------|-------|
| **Estado** | Stateless | Stateful |
| **Transporte** | TCP o UDP | Solo TCP |
| **Puerto** | 2049 + dinámicos | Solo 2049 |
| **rpcbind** | Requerido | No necesario |
| **mountd** | Requerido | No necesario |
| **lockd/statd** | Requeridos (separados) | Integrado en el protocolo |
| **Locking** | NLM (Network Lock Manager) | Lease-based locks |
| **Firewall** | Múltiples puertos | Un solo puerto |
| **Namespace** | Exports independientes | Pseudo-filesystem unificado |
| **Seguridad** | AUTH_SYS (UID/GID) | RPCSEC_GSS (Kerberos) |
| **ACLs** | Solo POSIX (parcial) | ACLs propias (estilo Windows) |
| **ID mapping** | Numérico (UID/GID) | Cadenas (user@domain) |
| **Operaciones** | Una por RPC | Compound (varias por RPC) |
| **Delegaciones** | No | Sí (caché agresivo) |
| **Internacionalización** | Limitada | UTF-8 obligatorio |

### Modelo de demonios

```
┌──────────────────────────────────────────────────────────────┐
│                       NFSv3                                   │
│                                                              │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐           │
│  │ rpcbind  │ │ mountd  │ │  nfsd   │ │  lockd  │           │
│  │ :111     │ │ :~20048 │ │ :2049   │ │ :~32803 │           │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘           │
│  ┌─────────┐ ┌─────────┐                                    │
│  │ statd   │ │ idmapd  │   ← 5-6 demonios, puertos          │
│  │ :~32765 │ │         │     dinámicos, difícil de           │
│  └─────────┘ └─────────┘     filtrar en firewall             │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│                       NFSv4                                   │
│                                                              │
│  ┌─────────────────────────────┐                             │
│  │          nfsd               │                             │
│  │         :2049               │  ← Un solo demonio,         │
│  │                             │    un solo puerto,           │
│  │  mount + lock + stat +      │    todo integrado            │
│  │  delegations + ACLs         │                             │
│  └─────────────────────────────┘                             │
│  ┌─────────┐                                                 │
│  │ idmapd  │  ← Mapeo de nombres (opcional con AUTH_SYS)     │
│  └─────────┘                                                 │
└──────────────────────────────────────────────────────────────┘
```

### Compound RPCs

NFSv3 envía una operación por RPC. Para abrir y leer un archivo se necesitan múltiples round-trips:

```
NFSv3 — abrir y leer un archivo:
  Cliente ──► LOOKUP "dir"     ──► Servidor    (1 RTT)
  Cliente ──► LOOKUP "file"    ──► Servidor    (1 RTT)
  Cliente ──► ACCESS           ──► Servidor    (1 RTT)
  Cliente ──► READ             ──► Servidor    (1 RTT)
  Total: 4 round-trips

NFSv4 — la misma operación:
  Cliente ──► COMPOUND {       ──► Servidor    (1 RTT)
                PUTFH "dir"
                LOOKUP "file"
                ACCESS
                READ
              }
  Total: 1 round-trip
```

Esto reduce la latencia significativamente, especialmente en redes WAN con alta latencia.

---

## 3. Protocolo y puertos

### NFSv3 — múltiples puertos

```bash
# Ver todos los puertos que usa NFSv3
rpcinfo -p localhost
```

```
   program vers proto   port  service
    100000    4   tcp    111  portmapper    ← rpcbind
    100000    4   udp    111  portmapper
    100003    3   tcp   2049  nfs           ← nfsd
    100003    3   udp   2049  nfs
    100005    3   tcp  20048  mountd        ← rpc.mountd
    100005    3   udp  20048  mountd
    100021    4   tcp  43215  nlockmgr      ← lockd
    100021    4   udp  43215  nlockmgr
    100024    1   tcp  32765  status        ← statd
    100024    1   udp  32765  status
```

Para usar NFSv3 con firewall, hay que fijar los puertos dinámicos o abrir rangos amplios.

### NFSv4 — un solo puerto

```bash
# NFSv4 puro: solo necesita esto en el firewall
sudo firewall-cmd --permanent --add-port=2049/tcp
sudo firewall-cmd --reload

# O con el servicio predefinido
sudo firewall-cmd --permanent --add-service=nfs
sudo firewall-cmd --reload
```

### Forzar versión en el servidor

```bash
# /etc/nfs.conf — deshabilitar NFSv3 (solo servir NFSv4)
[nfsd]
vers3 = n
vers4 = y
vers4.1 = y
vers4.2 = y
```

En distribuciones antiguas, la configuración está en `/etc/sysconfig/nfs`:

```bash
# /etc/sysconfig/nfs (RHEL 7 y anteriores)
RPCNFSDARGS="-N 3"    # Deshabilitar v3
```

### Verificar qué versiones sirve el servidor

```bash
# Desde el servidor
cat /proc/fs/nfsd/versions
# +3 +4 +4.1 +4.2
# El "+" indica habilitado, "-" deshabilitado

# Desde un cliente
rpcinfo -p server | grep nfs
# Solo muestra v3 si rpcbind está activo
# Para v4 puro, usar:
sudo mount -t nfs4 server:/path /mnt && mount | grep nfs
```

---

## 4. Pseudo-filesystem en NFSv4

NFSv4 introduce un **pseudo-filesystem**: un namespace virtual en el servidor que organiza todos los exports bajo una raíz única.

### Concepto

```
┌──────────────────────────────────────────────────────────┐
│                  NFSv3 — Exports planos                   │
│                                                          │
│  /etc/exports:                                           │
│    /srv/nfs/shared   192.168.1.0/24(rw)                  │
│    /var/data/docs    192.168.1.0/24(ro)                   │
│    /home             192.168.1.0/24(rw)                   │
│                                                          │
│  Cliente monta cada uno por separado:                    │
│    mount server:/srv/nfs/shared  /mnt/shared             │
│    mount server:/var/data/docs   /mnt/docs               │
│    mount server:/home            /mnt/home               │
│                                                          │
├──────────────────────────────────────────────────────────┤
│             NFSv4 — Pseudo-filesystem                     │
│                                                          │
│  Servidor construye un árbol virtual:                    │
│                                                          │
│           / (pseudo-root, fsid=0)                        │
│          ╱│╲                                             │
│         ╱ │ ╲                                            │
│       shared docs home                                   │
│       (real) (real) (real)                                │
│                                                          │
│  Cliente puede montar la raíz y ver todo:                │
│    mount -t nfs4 server:/ /mnt/server                    │
│    ls /mnt/server/                                       │
│    → shared/  docs/  home/                               │
└──────────────────────────────────────────────────────────┘
```

### Configurar el pseudo-root

```bash
# /etc/exports — pseudo-root con fsid=0
/srv/nfs          192.168.1.0/24(ro,fsid=0,no_subtree_check,crossmnt)
/srv/nfs/shared   192.168.1.0/24(rw,sync,no_subtree_check)
/srv/nfs/docs     192.168.1.0/24(ro,sync,no_subtree_check)
```

- `fsid=0` marca el directorio como la raíz del pseudo-filesystem
- `crossmnt` exporta automáticamente los subdirectorios que estén en otros filesystems

### Montar desde el pseudo-root

```bash
# Montar la raíz — ve todos los exports
sudo mount -t nfs4 server:/ /mnt/server
ls /mnt/server/
# shared/  docs/

# Montar un export específico (relativo al pseudo-root)
sudo mount -t nfs4 server:/shared /mnt/shared
```

### Exports fuera del pseudo-root

Si los exports están en rutas dispersas (`/var/data`, `/home`, `/opt/share`), hay que crear un árbol unificado con bind mounts:

```bash
# Crear estructura para el pseudo-root
sudo mkdir -p /srv/nfs/{data,home,share}

# Bind mount las rutas reales
sudo mount --bind /var/data    /srv/nfs/data
sudo mount --bind /home        /srv/nfs/home
sudo mount --bind /opt/share   /srv/nfs/share

# Persistir los bind mounts en fstab
# /etc/fstab
/var/data    /srv/nfs/data    none    bind    0  0
/home        /srv/nfs/home    none    bind    0  0
/opt/share   /srv/nfs/share   none    bind    0  0

# /etc/exports
/srv/nfs          192.168.1.0/24(ro,fsid=0,crossmnt,no_subtree_check)
/srv/nfs/data     192.168.1.0/24(rw,sync,no_subtree_check)
/srv/nfs/home     192.168.1.0/24(rw,sync,no_subtree_check)
/srv/nfs/share    192.168.1.0/24(ro,sync,no_subtree_check)
```

---

## 5. ID mapping

### El problema de los UIDs

NFSv3 transmite UIDs y GIDs numéricos directamente. Si `juan` es UID 1000 en el servidor pero UID 1005 en el cliente, los archivos de Juan aparecen con el usuario equivocado en el cliente:

```
┌──────────────────────────────────────────────────────────┐
│              NFSv3 — Mapping numérico                     │
│                                                          │
│  Servidor:  juan = UID 1000                              │
│  Archivo:   /srv/nfs/shared/report.txt  owned by 1000    │
│                                                          │
│  Cliente:   juan = UID 1005                              │
│             maria = UID 1000                             │
│                                                          │
│  ls -l /mnt/nfs/shared/report.txt                        │
│  → -rw-r--r-- maria staff ... report.txt                 │
│                ↑                                         │
│          ¡Muestra maria porque su UID coincide con 1000! │
│                                                          │
│  Solución NFSv3: mantener UIDs sincronizados             │
│  (LDAP, NIS, o gestión manual) en todos los equipos      │
└──────────────────────────────────────────────────────────┘
```

### NFSv4 — ID mapping con cadenas

NFSv4 transmite identidades como cadenas `user@domain` en lugar de números. El demonio `rpc.idmapd` en cada equipo traduce entre UIDs locales y cadenas:

```
┌──────────────────────────────────────────────────────────┐
│              NFSv4 — ID mapping                           │
│                                                          │
│  Servidor:  juan = UID 1000                              │
│  Archivo:   owned by "juan@example.com"  ← cadena        │
│                                                          │
│  Red NFS:   transmite "juan@example.com"                 │
│                                                          │
│  Cliente:   rpc.idmapd traduce                           │
│             "juan@example.com" → UID 1005 (local)        │
│                                                          │
│  ls -l /mnt/nfs/shared/report.txt                        │
│  → -rw-r--r-- juan staff ... report.txt  ✓ correcto     │
└──────────────────────────────────────────────────────────┘
```

### Configurar idmapd

```bash
# /etc/idmapd.conf — DEBE ser igual en servidor y clientes
[General]
Verbosity = 0
Domain = example.com    # ← Dominio NFSv4 (NO es DNS)

[Mapping]
Nobody-User = nobody
Nobody-Group = nogroup

[Translation]
Method = nsswitch       # Usa /etc/passwd, LDAP, etc.
```

El campo `Domain` debe coincidir **exactamente** en todos los equipos. Si no coincide, todos los archivos aparecen como `nobody:nogroup`.

```bash
# Verificar el dominio actual
cat /sys/fs/nfs/net/nfs_client/idmapper/default_domain

# Reiniciar idmapd tras cambiar configuración
sudo systemctl restart nfs-idmapd

# Limpiar caché de id mapping (forzar re-lookup)
sudo nfsidmap -c
```

### Diagnóstico de id mapping

```bash
# Ver cómo se traduce un usuario
nfsidmap -l juan

# Traducir de nombre a UID
nfsidmap juan@example.com
# uid: 1000, gid: 1000

# Si los archivos aparecen como nobody:nogroup:
# 1. Verificar que Domain coincide en ambos lados
# 2. Verificar que el usuario existe en ambos equipos
# 3. Limpiar caché:
sudo nfsidmap -c
```

### Keyring-based id mapping (kernel moderno)

En kernels modernos, el id mapping usa el keyring del kernel en lugar de `rpc.idmapd`:

```bash
# Ver el estado del keyring NFS
cat /proc/keys | grep nfs

# El módulo nfsidmap se carga automáticamente
# La configuración sigue siendo /etc/idmapd.conf
```

---

## 6. ACLs en NFSv4

NFSv4 define su propio sistema de ACLs, distinto de las ACLs POSIX de Linux. Son más parecidas a las ACLs de Windows (NTFS).

### Comparación

| Aspecto | POSIX ACL (NFSv3) | NFSv4 ACL |
|---------|-------------------|-----------|
| Herencia | Limitada (default ACLs) | Rica (flags de herencia) |
| Permisos | rwx | read_data, write_data, execute, append_data, delete, ... |
| Tipo | Allow | Allow y Deny |
| Orden | No importa | Importa (se evalúan en orden) |
| Identidades | user, group, other | Usuarios y grupos nombrados |

### Sintaxis de ACLs NFSv4

```bash
# Ver ACLs NFSv4
nfs4_getfacl /mnt/nfs/shared/file.txt
```

```
A::OWNER@:rwatTnNcCy
A::GROUP@:rtncy
A::EVERYONE@:rtncy
A::juan@example.com:rwatTnNcCy
D::maria@example.com:w
```

Anatomía de una entrada:

```
A  ::  juan@example.com  :  rwatTnNcCy
│      │                    │
│      │                    └── Permisos
│      └── Principal (usuario/grupo/@OWNER/@GROUP/@EVERYONE)
└── Tipo: A=Allow, D=Deny
```

### Permisos NFSv4

| Letra | Permiso | Aplica a |
|-------|---------|---------|
| `r` | read_data / list_directory | Archivos / Dirs |
| `w` | write_data / create_file | Archivos / Dirs |
| `a` | append_data / create_subdirectory | Archivos / Dirs |
| `x` | execute / change_directory | Archivos / Dirs |
| `d` | delete | Ambos |
| `D` | delete_child | Dirs |
| `t` | read_attributes | Ambos |
| `T` | write_attributes | Ambos |
| `n` | read_named_attrs | Ambos |
| `N` | write_named_attrs | Ambos |
| `c` | read_ACL | Ambos |
| `C` | write_ACL | Ambos |
| `y` | write_owner (chown) | Ambos |

### Establecer ACLs NFSv4

```bash
# Instalar herramientas
sudo apt install nfs4-acl-tools    # Debian
sudo dnf install nfs4-acl-tools    # RHEL

# Dar permisos de lectura/escritura a un usuario
nfs4_setfacl -a A::juan@example.com:rwatTnNcCy /mnt/nfs/shared/project/

# Denegar escritura a un usuario
nfs4_setfacl -a D::invitado@example.com:wa /mnt/nfs/shared/project/

# Aplicar desde un archivo
nfs4_getfacl /mnt/nfs/shared/dir1 > acls.txt
nfs4_setfacl -S acls.txt /mnt/nfs/shared/dir2
```

> **Nota:** Las ACLs NFSv4 requieren soporte en el filesystem del servidor. ext4 y XFS las soportan, pero deben montarse con la opción `nfs4acl` o el servidor debe estar configurado para traducirlas.

---

## 7. Delegaciones

Las delegaciones son un mecanismo por el cual el servidor **delega** el control de un archivo al cliente, permitiéndole cachear agresivamente sin consultar al servidor en cada operación.

### Cómo funcionan

```
┌──────────────────────────────────────────────────────────┐
│              Sin delegación (NFSv3)                       │
│                                                          │
│  Cliente A: open("file")  ──────► Servidor               │
│  Cliente A: read(4096)    ──────► Servidor               │
│  Cliente A: read(4096)    ──────► Servidor               │
│  Cliente A: read(4096)    ──────► Servidor               │
│  Total: 4 RPCs                                           │
│                                                          │
├──────────────────────────────────────────────────────────┤
│              Con delegación (NFSv4)                       │
│                                                          │
│  Cliente A: OPEN("file")  ──────► Servidor               │
│             ◄── "toma, delegación READ"                  │
│                                                          │
│  Cliente A: read(4096)    ──► caché local ✓              │
│  Cliente A: read(4096)    ──► caché local ✓              │
│  Cliente A: read(4096)    ──► caché local ✓              │
│  Total: 1 RPC + 3 lecturas locales                       │
│                                                          │
│  Si Cliente B abre el mismo archivo:                     │
│  Servidor ──► "Cliente A, devuelve la delegación"        │
│  Cliente A ──► flush caché al servidor                   │
│  Ahora ambos operan sin delegación (como NFSv3)          │
└──────────────────────────────────────────────────────────┘
```

### Tipos de delegación

| Tipo | Permite cachear | Condición |
|------|----------------|-----------|
| **READ** | Lecturas locales sin consultar servidor | Ningún otro cliente escribe |
| **WRITE** | Lecturas y escrituras locales | Ningún otro cliente tiene el archivo abierto |

### Impacto en rendimiento

Las delegaciones benefician enormemente a cargas de trabajo donde un archivo es accedido por **un solo cliente** a la vez (el caso más común). En archivos accedidos simultáneamente por múltiples clientes, se revocan y el rendimiento es similar a NFSv3.

```bash
# Ver estadísticas de delegaciones en el servidor
cat /proc/fs/nfsd/clients/*/info

# En el cliente
cat /proc/self/mountstats | grep -A 5 delegation
```

---

## 8. Seguridad Kerberos

### Por qué Kerberos

`sec=sys` (el default) confía en el UID que reporta el cliente. Un usuario con root en un equipo autorizado puede hacerse pasar por cualquier otro usuario:

```bash
# En un cliente con sec=sys, root puede leer archivos de cualquiera:
su - juan       # root puede ser juan
cat /mnt/nfs/confidencial/reporte.txt    # Sin verificación real
```

Kerberos proporciona **autenticación real**: el servidor verifica la identidad del usuario mediante tickets criptográficos, no por UID.

### Niveles de seguridad Kerberos

```
┌──────────────────────────────────────────────────────────┐
│           Niveles de seguridad Kerberos en NFS            │
│                                                          │
│  sec=sys     Sin Kerberos. UID/GID del cliente.          │
│              ← Rápido, inseguro                          │
│                                                          │
│  sec=krb5    Autenticación Kerberos.                     │
│              El servidor verifica quién eres.            │
│              Datos viajan en claro.                      │
│              ← Identidad verificada, datos expuestos     │
│                                                          │
│  sec=krb5i   Kerberos + integridad.                      │
│              Checksum en cada paquete.                   │
│              Datos en claro pero detecta modificación.   │
│              ← Protege contra tampering                  │
│                                                          │
│  sec=krb5p   Kerberos + integridad + cifrado.            │
│              Todo el tráfico cifrado.                    │
│              ← Máxima seguridad, mayor coste de CPU      │
└──────────────────────────────────────────────────────────┘
```

### Requisitos previos

1. **KDC (Key Distribution Center)** funcionando — usualmente un servidor FreeIPA o Active Directory
2. **Keytabs** generados para el servidor y clientes NFS
3. **Sincronización de tiempo** (NTP) — Kerberos falla si los relojes difieren más de 5 minutos
4. **DNS funcional** — los tickets usan nombres DNS, no IPs

### Configuración del servidor

```bash
# 1. Instalar soporte Kerberos para NFS
sudo apt install nfs-common krb5-user    # Debian
sudo dnf install nfs-utils krb5-workstation    # RHEL

# 2. Configurar el keytab del servidor
# (generado desde el KDC — ejemplo con FreeIPA)
# El principal debe ser: nfs/server.example.com@EXAMPLE.COM
sudo klist -ke /etc/krb5.keytab | grep nfs
# nfs/server.example.com@EXAMPLE.COM (aes256-cts-hmac-sha1-96)

# 3. Habilitar el demonio gssd
sudo systemctl enable --now rpc-gssd

# 4. Configurar /etc/exports con sec=krb5p
/srv/nfs/secure    *.example.com(rw,sync,sec=krb5p,no_subtree_check)

# 5. Aplicar
sudo exportfs -ra
```

### Configuración del cliente

```bash
# 1. El cliente también necesita keytab
# Principal: nfs/client.example.com@EXAMPLE.COM
sudo klist -ke /etc/krb5.keytab | grep nfs

# 2. Habilitar gssd
sudo systemctl enable --now rpc-gssd

# 3. Montar con Kerberos
sudo mount -t nfs4 -o sec=krb5p server.example.com:/secure /mnt/secure

# 4. El usuario necesita un ticket Kerberos válido
kinit juan@EXAMPLE.COM
klist    # Verificar ticket

# 5. Ahora puede acceder
ls /mnt/secure/
```

### Flujo de autenticación

```
┌──────────────────────────────────────────────────────────┐
│          Flujo NFS con Kerberos (sec=krb5p)               │
│                                                          │
│  1. Usuario obtiene TGT del KDC                          │
│     kinit juan@EXAMPLE.COM                               │
│     juan ◄──── TGT ────► KDC                            │
│                                                          │
│  2. Cliente solicita ticket de servicio NFS              │
│     gssd ──── "necesito nfs/server" ────► KDC            │
│     gssd ◄──── Service Ticket ────                       │
│                                                          │
│  3. Cliente presenta ticket al servidor NFS              │
│     gssd ──── Service Ticket ────► svcgssd (servidor)    │
│                                    Verifica ticket ✓     │
│                                                          │
│  4. Comunicación cifrada (krb5p)                         │
│     Cliente ◄════ Datos cifrados ════► Servidor          │
└──────────────────────────────────────────────────────────┘
```

### Rendimiento con Kerberos

El cifrado (`krb5p`) tiene un coste de CPU significativo. Benchmark aproximado:

```
Operación              sec=sys    sec=krb5    sec=krb5p
──────────────────────────────────────────────────────
Lectura secuencial     1.0x       0.95x       0.6-0.7x
Escritura secuencial   1.0x       0.95x       0.5-0.6x
Metadatos (ls -lR)     1.0x       0.8x        0.7x
```

Con hardware AES-NI (presente en CPUs modernas), la penalización de `krb5p` se reduce a un 15-20%.

---

## 9. Migración de NFSv3 a NFSv4

### Checklist de migración

```bash
# ── Servidor ──

# 1. Verificar que el kernel soporta NFSv4
cat /proc/fs/nfsd/versions
# Debe incluir +4 +4.1 +4.2

# 2. Configurar el dominio idmapd
# /etc/idmapd.conf → Domain = example.com
sudo systemctl restart nfs-idmapd

# 3. Crear pseudo-root (si exports están dispersos)
sudo mkdir -p /srv/nfs
# Bind mount + fsid=0 (ver sección 4)

# 4. Actualizar /etc/exports
# Agregar sec= si se usa Kerberos
# Asegurar no_subtree_check

# 5. Aplicar
sudo exportfs -ra

# ── Clientes ──

# 6. Configurar mismo Domain en idmapd.conf
# 7. Cambiar montajes de nfs → nfs4 (o vers=4)
# 8. Probar montaje
sudo mount -t nfs4 server:/shared /mnt/shared

# 9. Verificar id mapping
ls -l /mnt/shared/
# Los archivos deben mostrar nombres, no nobody

# 10. Actualizar fstab
# Cambiar tipo de nfs a nfs4
```

### Compatibilidad simultánea

Es posible servir NFSv3 y NFSv4 simultáneamente durante la migración:

```bash
# /etc/nfs.conf
[nfsd]
vers3 = y    # Mantener v3 para clientes legacy
vers4 = y    # Habilitar v4 para nuevos clientes
vers4.1 = y
vers4.2 = y

# Verificar
cat /proc/fs/nfsd/versions
# +3 +4 +4.1 +4.2
```

Cuando todos los clientes usen NFSv4, deshabilitar v3 simplifica la configuración de firewall (solo puerto 2049).

### Tabla de equivalencias para fstab

```bash
# NFSv3
server:/srv/nfs/shared  /mnt/shared  nfs  rw,hard,_netdev  0  0

# NFSv4 — ruta completa
server:/srv/nfs/shared  /mnt/shared  nfs4  rw,hard,_netdev  0  0

# NFSv4 — ruta relativa al pseudo-root (si fsid=0 en /srv/nfs)
server:/shared  /mnt/shared  nfs4  rw,hard,_netdev  0  0
```

---

## 10. Errores comunes

### Error 1 — Dominio idmapd diferente entre servidor y cliente

```bash
# Síntoma: todos los archivos aparecen como nobody:nogroup
ls -l /mnt/nfs/shared/
# -rw-r--r-- nobody nogroup ... file.txt

# Causa: Domain en /etc/idmapd.conf no coincide
# Servidor: Domain = example.com
# Cliente:  Domain = localdomain    ← diferente

# Diagnóstico
cat /sys/fs/nfs/net/nfs_client/idmapper/default_domain
# localdomain ← debe ser example.com

# Solución: igualar Domain, reiniciar, limpiar caché
sudo vim /etc/idmapd.conf    # Domain = example.com
sudo systemctl restart nfs-idmapd
sudo nfsidmap -c
# Remontar
sudo umount /mnt/nfs/shared && sudo mount -a
```

### Error 2 — Montar con ruta NFSv3 en un servidor con pseudo-root NFSv4

```bash
# El servidor exporta /srv/nfs (fsid=0) con /srv/nfs/shared dentro
# El cliente intenta:
sudo mount -t nfs4 server:/srv/nfs/shared /mnt/shared
# mount.nfs4: access denied by server

# Causa: en NFSv4, la ruta es relativa al pseudo-root
# /srv/nfs es la raíz (fsid=0), así que "shared" está en /shared

# Solución:
sudo mount -t nfs4 server:/shared /mnt/shared
```

### Error 3 — Kerberos falla por diferencia de reloj

```bash
# mount -t nfs4 -o sec=krb5 server:/path /mnt
# mount.nfs4: Operation not permitted

# Diagnóstico — verificar sincronización de tiempo
date                           # Hora local
ssh server date                # Hora del servidor
# Si difieren más de 5 minutos → Kerberos rechaza el ticket

# Solución — configurar NTP
sudo systemctl enable --now chronyd
chronyc tracking
```

### Error 4 — showmount no funciona con NFSv4 puro

```bash
# showmount -e server
# clnt_create: RPC: Program not registered

# Causa: showmount usa el protocolo MOUNT (NFSv3)
# Un servidor NFSv4 puro no ejecuta rpc.mountd

# En NFSv4, no existe equivalente de showmount
# El cliente debe conocer la ruta del export
# Alternativa: montar el pseudo-root y navegar
sudo mount -t nfs4 server:/ /mnt/server
ls /mnt/server/
```

### Error 5 — Olvidar crossmnt para sub-exports

```bash
# /etc/exports
/srv/nfs           *(ro,fsid=0,no_subtree_check)
/srv/nfs/shared    *(rw,sync,no_subtree_check)

# Cliente monta la raíz:
sudo mount -t nfs4 server:/ /mnt/server
ls /mnt/server/shared/
# ls: cannot access '/mnt/server/shared/': No such file or directory

# Causa: sin crossmnt, los sub-exports no se ven al montar la raíz
# Solución: agregar crossmnt al pseudo-root
/srv/nfs    *(ro,fsid=0,crossmnt,no_subtree_check)
```

---

## 11. Cheatsheet

```bash
# ── Versiones ─────────────────────────────────────────────
cat /proc/fs/nfsd/versions           # Versiones activas (+3 +4 ...)

# /etc/nfs.conf — deshabilitar NFSv3
# [nfsd]
# vers3 = n

# ── Pseudo-filesystem (NFSv4) ────────────────────────────
# /etc/exports:
/srv/nfs        *(ro,fsid=0,crossmnt)  # Pseudo-root
/srv/nfs/data   *(rw,sync)             # Sub-export

mount -t nfs4 server:/ /mnt            # Montar raíz
mount -t nfs4 server:/data /mnt/data   # Montar sub-export

# ── ID mapping ────────────────────────────────────────────
# /etc/idmapd.conf → Domain = example.com (igual en todos)
nfsidmap -c                             # Limpiar caché
nfsidmap user@domain                    # Probar traducción
systemctl restart nfs-idmapd            # Aplicar cambios

# ── ACLs NFSv4 ────────────────────────────────────────────
nfs4_getfacl /mnt/nfs/file             # Ver ACLs
nfs4_setfacl -a A::user@dom:rw /file   # Agregar ACL
nfs4_setfacl -x A::user@dom:rw /file   # Eliminar ACL

# ── Kerberos ──────────────────────────────────────────────
# /etc/exports:
/srv/nfs/sec  *(rw,sec=krb5p)          # Export con Kerberos

systemctl enable --now rpc-gssd        # Habilitar gssd
klist -ke /etc/krb5.keytab | grep nfs  # Verificar keytab
kinit user@REALM                       # Obtener ticket (cliente)

mount -o sec=krb5p server:/sec /mnt    # Montar con cifrado

# ── Comparación rápida ───────────────────────────────────
#              NFSv3              NFSv4
# Puertos:    111,2049,dinám.    2049
# Estado:     Stateless          Stateful
# Auth:       UID/GID            user@domain + Kerberos
# Locking:    NLM externo        Integrado (leases)
# RPCs:       Una por llamada    Compound (múltiples)
# ACLs:       POSIX              NFSv4 (estilo Windows)

# ── Migración ────────────────────────────────────────────
# 1. Igualar Domain en /etc/idmapd.conf (todos los equipos)
# 2. Crear pseudo-root con fsid=0 y crossmnt
# 3. Cambiar fstab: nfs → nfs4, rutas relativas al pseudo-root
# 4. Probar con mount -t nfs4
# 5. Deshabilitar vers3 cuando todos los clientes migren
```

---

## 12. Ejercicios

### Ejercicio 1 — Configurar pseudo-filesystem NFSv4

Un servidor tiene datos en `/var/data/proyectos` y `/home`. Configura un pseudo-root NFSv4 que los unifique.

```bash
# 1. Crear estructura del pseudo-root
sudo mkdir -p /srv/nfs/{proyectos,home}

# 2. Bind mount las rutas reales
sudo mount --bind /var/data/proyectos /srv/nfs/proyectos
sudo mount --bind /home /srv/nfs/home

# 3. Persistir en fstab
echo '/var/data/proyectos  /srv/nfs/proyectos  none  bind  0  0' | sudo tee -a /etc/fstab
echo '/home               /srv/nfs/home        none  bind  0  0' | sudo tee -a /etc/fstab

# 4. Configurar /etc/exports
cat <<'EOF' | sudo tee /etc/exports
/srv/nfs            192.168.1.0/24(ro,fsid=0,crossmnt,no_subtree_check)
/srv/nfs/proyectos  192.168.1.0/24(rw,sync,no_subtree_check)
/srv/nfs/home       192.168.1.0/24(rw,sync,no_subtree_check)
EOF

# 5. Aplicar y verificar
sudo exportfs -ra
sudo exportfs -v
```

**Pregunta de predicción:** Si un cliente monta `server:/` (la raíz del pseudo-root) con opciones `rw`, ¿puede escribir en `/mnt/server/proyectos/`? ¿Y en `/mnt/server/` directamente (crear un archivo en la raíz)?

> **Respuesta:** Puede escribir en `/mnt/server/proyectos/` porque ese sub-export tiene `rw`. Pero **no puede** crear archivos en `/mnt/server/` directamente porque el pseudo-root (`/srv/nfs`) se exportó como `ro`. Cada punto del árbol hereda las opciones de su export más específico, no del padre. La raíz `ro` solo permite navegar para llegar a los sub-exports.

---

### Ejercicio 2 — Diagnosticar id mapping roto

Tras migrar a NFSv4, todos los archivos aparecen como `nobody:nogroup`. Diagnóstica y corrige.

```bash
# 1. Verificar el dominio en el cliente
cat /sys/fs/nfs/net/nfs_client/idmapper/default_domain
# localdomain ← probablemente incorrecto

# 2. Verificar el dominio en el servidor
ssh server cat /etc/idmapd.conf | grep Domain
# Domain = example.com

# 3. Corregir en el cliente
sudo sed -i 's/^#\?Domain.*/Domain = example.com/' /etc/idmapd.conf

# 4. Reiniciar idmapd y limpiar caché
sudo systemctl restart nfs-idmapd
sudo nfsidmap -c

# 5. Remontar para forzar re-lookup
sudo umount /mnt/nfs/shared
sudo mount -a

# 6. Verificar
ls -l /mnt/nfs/shared/
# Ahora debe mostrar los nombres de usuario correctos
```

**Pregunta de predicción:** Si el servidor tiene `Domain = example.com` y el cliente tiene `Domain = Example.com` (mayúscula), ¿funciona el id mapping?

> **Respuesta:** No. El dominio es **case-sensitive**. `Example.com` ≠ `example.com`. El servidor envía `juan@example.com` pero el cliente espera usuarios en `@Example.com`, no encuentra coincidencia, y mapea todo a `nobody`. Deben ser idénticos, incluyendo mayúsculas y minúsculas.

---

### Ejercicio 3 — Decidir la versión y seguridad adecuada

Para cada escenario, elige la versión NFS y el nivel de seguridad apropiado, justificando tu decisión.

| Escenario | Versión | sec= | Justificación |
|-----------|---------|------|---------------|
| Lab de desarrollo, 3 equipos, red aislada | NFSv4 | sys | Red confiable, simplicidad, un solo puerto |
| Departamento de finanzas, datos sensibles | NFSv4.2 | krb5p | Datos confidenciales requieren cifrado y auth real |
| Cluster HPC, rendimiento máximo | NFSv4.1 | sys | pNFS para I/O paralelo, krb5p demasiado costoso |
| Equipos legacy (Solaris 10) | NFSv3 | sys | Compatibilidad, Solaris 10 tiene soporte NFSv4 limitado |
| Oficina con equipos Windows y Linux | SMB | — | Samba es mejor para entornos mixtos |

**Pregunta de predicción:** Un administrador quiere usar `sec=krb5p` en un cluster de 200 nodos que procesan petabytes de datos científicos. ¿Es buena idea? ¿Qué alternativa propondrías?

> **Respuesta:** No es buena idea. `krb5p` cifra todo el tráfico, lo que reduciría el throughput un 30-40% en transferencias masivas. Para datos científicos (típicamente no confidenciales), mejor usar `sec=krb5i` (integridad sin cifrado) o incluso `sec=krb5` (solo autenticación). Si los datos sí son confidenciales, considerar cifrar a nivel de red (IPsec/WireGuard) en lugar de a nivel NFS, ya que el cifrado de red es más eficiente y protege todo el tráfico.

---

> **Fin de la Sección 1 — NFS.** Siguiente sección: S02 — Samba (T01 — Configuración de Samba: smb.conf, shares, usuarios).
