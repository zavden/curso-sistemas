# Servidor NFS

## Índice
1. [Qué es NFS](#1-qué-es-nfs)
2. [Arquitectura y componentes](#2-arquitectura-y-componentes)
3. [Instalación del servidor](#3-instalación-del-servidor)
4. [El archivo /etc/exports](#4-el-archivo-etcexports)
5. [Opciones de exportación](#5-opciones-de-exportación)
6. [exportfs — gestión dinámica](#6-exportfs--gestión-dinámica)
7. [Seguridad y control de acceso](#7-seguridad-y-control-de-acceso)
8. [Firewall y puertos](#8-firewall-y-puertos)
9. [Diagnóstico del servidor](#9-diagnóstico-del-servidor)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es NFS

**NFS (Network File System)** es un protocolo que permite compartir directorios entre máquinas Linux/Unix a través de la red. Un servidor exporta directorios y los clientes los montan como si fueran locales. Fue creado por Sun Microsystems en 1984 y es el estándar de facto para compartir archivos en entornos Unix.

```
┌─────────────────────────────────────────────────────────┐
│                    Red local (LAN)                       │
│                                                         │
│  ┌──────────────┐         ┌──────────────┐              │
│  │  NFS Server   │         │  NFS Client   │             │
│  │              │  NFS    │              │              │
│  │  /srv/share/ │◄───────►│  /mnt/nfs/   │              │
│  │  (datos      │  TCP    │  (punto de   │              │
│  │   reales)    │  2049   │   montaje)   │              │
│  └──────────────┘         └──────────────┘              │
│                                                         │
│  El cliente lee/escribe en /mnt/nfs/ y el kernel        │
│  redirige las operaciones al servidor vía red           │
└─────────────────────────────────────────────────────────┘
```

La experiencia del usuario es **transparente**: las aplicaciones leen y escriben archivos sin saber que están en otro equipo. Los permisos se mapean por UID/GID, lo que implica que ambos equipos deben compartir un esquema de usuarios coherente (o usar NFSv4 con id mapping).

---

## 2. Arquitectura y componentes

### Versiones de NFS

| Versión | Transporte | Estado | Características |
|---------|-----------|--------|-----------------|
| NFSv3 | UDP/TCP | Stateless | Necesita portmap, lockd, mountd |
| NFSv4 | TCP | Stateful | Puerto único 2049, ACLs, Kerberos |
| NFSv4.1 | TCP | Stateful | pNFS (parallel NFS), sessions |
| NFSv4.2 | TCP | Stateful | Server-side copy, sparse files |

### Demonios del servidor

```
┌──────────────────────────────────────────────────────┐
│                   NFS Server Stack                    │
│                                                      │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐ │
│  │   nfsd      │  │   mountd    │  │   statd      │ │
│  │  (kernel)   │  │ rpc.mountd  │  │ rpc.statd    │ │
│  │  Maneja     │  │  Autoriza   │  │  Recovery    │ │
│  │  read/write │  │  montajes   │  │  de locks    │ │
│  │  Puerto     │  │  Puerto     │  │  (NFSv3)     │ │
│  │  2049       │  │  dinámico   │  │              │ │
│  └─────────────┘  └─────────────┘  └──────────────┘ │
│                                                      │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐ │
│  │   lockd     │  │  rpcbind    │  │   idmapd     │ │
│  │  (kernel)   │  │ (portmap)   │  │  rpc.idmapd  │ │
│  │  File       │  │  Mapea      │  │  NFSv4       │ │
│  │  locking    │  │  servicios  │  │  user/group  │ │
│  │  (NFSv3)    │  │  Puerto 111 │  │  mapping     │ │
│  └─────────────┘  └─────────────┘  └──────────────┘ │
└──────────────────────────────────────────────────────┘
```

**NFSv4 simplifica** esta arquitectura: sólo necesita `nfsd` en el puerto 2049. No requiere `rpcbind`, `mountd` ni `statd` porque todo se maneja sobre un único puerto TCP. Sin embargo, los paquetes de distribución suelen iniciar todos los demonios por compatibilidad con clientes NFSv3.

### Flujo de una operación de lectura

```
Cliente                          Red              Servidor
───────                         ─────            ─────────
1. cat /mnt/nfs/file.txt
2. VFS detecta que es NFS
3. Cliente NFS (kernel)  ──── RPC READ ────►  nfsd (kernel)
                                              4. Lee del disco
                                              5. Devuelve datos
6. Muestra contenido    ◄──── RPC REPLY ────
```

---

## 3. Instalación del servidor

### Debian/Ubuntu

```bash
# Instalar el servidor NFS
sudo apt install nfs-kernel-server

# El servicio se habilita automáticamente
sudo systemctl status nfs-kernel-server

# Verificar que rpcbind está activo (necesario para NFSv3)
sudo systemctl status rpcbind
```

### RHEL/Fedora

```bash
# Instalar el servidor NFS
sudo dnf install nfs-utils

# Habilitar e iniciar
sudo systemctl enable --now nfs-server

# Verificar
sudo systemctl status nfs-server
```

### Verificar que los demonios están corriendo

```bash
# Ver los servicios RPC registrados
rpcinfo -p localhost
```

Salida esperada:

```
   program vers proto   port  service
    100000    4   tcp    111  portmapper
    100000    4   udp    111  portmapper
    100003    3   tcp   2049  nfs
    100003    4   tcp   2049  nfs
    100005    3   tcp  20048  mountd
    100021    4   tcp  43215  nlockmgr
```

### Crear el directorio a compartir

```bash
# Crear directorio para compartir
sudo mkdir -p /srv/nfs/shared

# Asignar permisos
# nobody:nogroup es el usuario que NFS usa por defecto (root_squash)
sudo chown nobody:nogroup /srv/nfs/shared
sudo chmod 755 /srv/nfs/shared

# Crear contenido de prueba
echo "NFS works!" | sudo tee /srv/nfs/shared/test.txt
```

---

## 4. El archivo /etc/exports

`/etc/exports` es el archivo de configuración principal del servidor NFS. Define qué directorios se exportan, a qué clientes, y con qué opciones.

### Sintaxis

```
/ruta/directorio    cliente1(opciones)    cliente2(opciones)
```

**Sin espacio** entre el cliente y el paréntesis de opciones. Un espacio accidental cambia el significado por completo.

### Formatos de cliente

```bash
# IP específica
/srv/nfs/shared    192.168.1.100(rw,sync,no_subtree_check)

# Subred CIDR
/srv/nfs/shared    192.168.1.0/24(rw,sync,no_subtree_check)

# Hostname
/srv/nfs/shared    client1.example.com(rw,sync,no_subtree_check)

# Wildcard DNS
/srv/nfs/shared    *.example.com(rw,sync,no_subtree_check)

# Todos (peligroso — solo para labs)
/srv/nfs/shared    *(ro,sync,no_subtree_check)
```

### Múltiples clientes con distintas opciones

```bash
# Lectura-escritura para la red de servidores, solo lectura para el resto
/srv/nfs/data    192.168.1.0/24(rw,sync,no_subtree_check)  10.0.0.0/8(ro,sync)
```

### Ejemplo completo de /etc/exports

```bash
# /etc/exports — NFS server exports
#
# Directorio compartido general — acceso rw para la LAN
/srv/nfs/shared     192.168.1.0/24(rw,sync,no_subtree_check)

# Directorio home — acceso por máquina específica
/home               192.168.1.50(rw,sync,no_root_squash,no_subtree_check)

# Directorio de solo lectura — documentación
/srv/nfs/docs       192.168.1.0/24(ro,sync,no_subtree_check)

# ISO images — solo lectura, todos
/srv/nfs/iso        *(ro,sync,no_subtree_check)
```

### El error del espacio fatal

```bash
# CORRECTO — solo 192.168.1.0/24 tiene acceso rw
/srv/nfs/shared    192.168.1.0/24(rw,sync)

# INCORRECTO — espacio antes del paréntesis
# Resultado: 192.168.1.0/24 con opciones DEFAULT (ro)
#            Y todo el mundo (*) con rw,sync
/srv/nfs/shared    192.168.1.0/24 (rw,sync)
#                             ↑
#                       Este espacio separa en dos entradas:
#                       1. "192.168.1.0/24" con defaults
#                       2. "*(rw,sync)" — ¡TODOS con rw!
```

---

## 5. Opciones de exportación

### Opciones de lectura/escritura

| Opción | Significado | Default |
|--------|------------|---------|
| `rw` | Lectura y escritura | No |
| `ro` | Solo lectura | **Sí** |

### Opciones de sincronización

| Opción | Significado | Default |
|--------|------------|---------|
| `sync` | Escribir a disco antes de responder | **Sí** (NFSv4) |
| `async` | Responder antes de escribir a disco | No |

`async` mejora el rendimiento pero **riesgo de corrupción** si el servidor se apaga inesperadamente. Los datos que el cliente cree escritos pueden no estar en disco.

```
         sync                              async
  ────────────────                 ────────────────
  Client: WRITE ──►               Client: WRITE ──►
  Server: disco ✓                 Server: OK ──► Client
  Server: OK ──► Client           Server: disco (después)

  Más lento, seguro               Más rápido, riesgo
```

### Opciones de mapeo de usuarios (squash)

Estas opciones controlan cómo el servidor mapea los UIDs del cliente:

| Opción | Efecto |
|--------|--------|
| `root_squash` | root (UID 0) del cliente → `nobody` en servidor **(default)** |
| `no_root_squash` | root del cliente mantiene UID 0 en servidor **(peligroso)** |
| `all_squash` | Todos los usuarios → `nobody` |
| `anonuid=N` | UID del usuario anónimo (con squash) |
| `anongid=N` | GID del grupo anónimo (con squash) |

```
┌──────────────────────────────────────────────────────────┐
│              root_squash (default)                        │
│                                                          │
│  Cliente (root, UID=0) ──write──► Servidor               │
│                                   Mapea UID 0 → 65534    │
│                                   (nobody:nogroup)        │
│                                   Archivo creado como     │
│                                   nobody:nogroup          │
│                                                          │
│  Cliente (user, UID=1000) ──write──► Servidor            │
│                                      Mantiene UID 1000   │
│                                      Archivo creado como │
│                                      UID 1000            │
└──────────────────────────────────────────────────────────┘
```

**¿Cuándo usar `no_root_squash`?** Casi nunca. El caso principal es cuando el servidor NFS exporta `/home` y el cliente necesita que root pueda administrar archivos de usuarios (por ejemplo, un servidor de backup). Incluso así, es preferible usar Kerberos con NFSv4.

### Opciones de subtree checking

| Opción | Efecto |
|--------|--------|
| `subtree_check` | Verifica que el archivo esté dentro del subárbol exportado |
| `no_subtree_check` | No verifica **(recomendado)** |

`subtree_check` causa problemas cuando archivos se renombran mientras un cliente los tiene abiertos. Con `no_subtree_check` el rendimiento es mejor y los errores NFS_STALE menos frecuentes.

### Opciones adicionales

| Opción | Efecto |
|--------|--------|
| `insecure` | Permite conexiones desde puertos > 1024 |
| `secure` | Solo puertos < 1024 **(default)** |
| `wdelay` | Agrupa escrituras pequeñas (solo con sync) **(default)** |
| `no_wdelay` | Escribe inmediatamente |
| `crossmnt` | Exporta submontajes automáticamente |
| `fsid=0` | Marca el pseudoroot de NFSv4 |

### Ejemplo con all_squash y anonuid

```bash
# Todos los usuarios del cliente se mapean a www-data (UID 33)
# Útil para un directorio web compartido
/srv/nfs/www    192.168.1.0/24(rw,sync,all_squash,anonuid=33,anongid=33,no_subtree_check)
```

---

## 6. exportfs — gestión dinámica

`exportfs` permite gestionar las exportaciones sin reiniciar el servicio NFS.

### Comandos principales

```bash
# Mostrar exportaciones activas (con opciones)
sudo exportfs -v

# Aplicar cambios de /etc/exports sin reiniciar
sudo exportfs -ra

# Exportar un directorio temporalmente (no persiste en reboot)
sudo exportfs -o rw,sync,no_subtree_check 192.168.1.0/24:/srv/nfs/temp

# Dejar de exportar un directorio
sudo exportfs -u 192.168.1.0/24:/srv/nfs/shared

# Dejar de exportar todo
sudo exportfs -ua

# Re-exportar todo (después de -ua)
sudo exportfs -a
```

### Flujo de trabajo habitual

```bash
# 1. Editar /etc/exports
sudo vim /etc/exports

# 2. Aplicar cambios
sudo exportfs -ra

# 3. Verificar
sudo exportfs -v
```

Salida de `exportfs -v`:

```
/srv/nfs/shared    192.168.1.0/24(rw,wdelay,root_squash,no_subtree_check,
                   sec=sys,rw,secure,root_squash,no_all_squash)
/srv/nfs/docs      192.168.1.0/24(ro,wdelay,root_squash,no_subtree_check,
                   sec=sys,ro,secure,root_squash,no_all_squash)
```

### Diferencia entre exportfs -ra y systemctl restart

```
exportfs -ra:
  - Aplica cambios incrementales
  - NO desconecta clientes activos
  - NO interrumpe operaciones de I/O en curso
  ✓ Método preferido

systemctl restart nfs-server:
  - Reinicia todos los demonios
  - DESCONECTA clientes activos
  - Puede causar errores "Stale file handle"
  ✗ Solo para cambios de configuración global
```

---

## 7. Seguridad y control de acceso

### Niveles de seguridad

NFS tiene **tres capas** de seguridad que deben funcionar juntas:

```
┌──────────────────────────────────────────────────────────┐
│                  Capas de Seguridad NFS                   │
│                                                          │
│  1. /etc/exports ──── ¿Quién puede montar?               │
│     └── IP/subred/hostname del cliente                   │
│                                                          │
│  2. Firewall ──── ¿Qué puertos están abiertos?           │
│     └── iptables/nftables/firewalld                      │
│                                                          │
│  3. Permisos Unix ──── ¿Qué puede hacer el usuario?      │
│     └── UID/GID mapping + chmod/chown en servidor        │
│                                                          │
│  (4. Kerberos ──── Autenticación real de usuario)        │
│      └── NFSv4 con sec=krb5/krb5i/krb5p                 │
└──────────────────────────────────────────────────────────┘
```

### Modos de seguridad (sec=)

| Modo | Autenticación | Integridad | Cifrado |
|------|--------------|-----------|---------|
| `sys` | UID/GID (confía en el cliente) | No | No |
| `krb5` | Kerberos | No | No |
| `krb5i` | Kerberos | Sí | No |
| `krb5p` | Kerberos | Sí | Sí |

El modo por defecto es `sec=sys`, que confía ciegamente en el UID que reporta el cliente. Esto significa que si alguien tiene root en un equipo con acceso NFS, puede hacerse pasar por cualquier usuario. En entornos de producción con datos sensibles, `sec=krb5p` es el estándar.

### TCP Wrappers (hosts.allow / hosts.deny)

En sistemas que aún soportan TCP Wrappers (cada vez menos con systemd):

```bash
# /etc/hosts.allow
rpcbind: 192.168.1.0/255.255.255.0
mountd: 192.168.1.0/255.255.255.0
nfsd: 192.168.1.0/255.255.255.0

# /etc/hosts.deny
rpcbind: ALL
mountd: ALL
nfsd: ALL
```

---

## 8. Firewall y puertos

### Puertos necesarios

| Servicio | Puerto | Protocolo | Necesario en |
|----------|--------|----------|-------------|
| rpcbind | 111 | TCP/UDP | NFSv3 |
| nfsd | 2049 | TCP | NFSv3/v4 |
| mountd | Dinámico | TCP/UDP | NFSv3 |
| statd | Dinámico | TCP/UDP | NFSv3 |
| lockd | Dinámico | TCP/UDP | NFSv3 |

**NFSv4 sólo necesita el puerto 2049 TCP.** Esta es una de sus ventajas más significativas: una sola regla de firewall.

### Fijar puertos dinámicos (NFSv3)

Para usar NFSv3 con firewall, hay que fijar los puertos de `mountd`, `statd` y `lockd`:

```bash
# /etc/nfs.conf (sistemas modernos)
[mountd]
port = 20048

[statd]
port = 32765

[lockd]
port = 32803
udp-port = 32803
```

En sistemas más antiguos, se configuran en `/etc/sysconfig/nfs` (RHEL) o `/etc/default/nfs-kernel-server` (Debian).

### firewalld (RHEL/Fedora)

```bash
# NFSv4 — solo necesita nfs
sudo firewall-cmd --permanent --add-service=nfs
sudo firewall-cmd --reload

# NFSv3 — necesita servicios adicionales
sudo firewall-cmd --permanent --add-service=nfs
sudo firewall-cmd --permanent --add-service=rpc-bind
sudo firewall-cmd --permanent --add-service=mountd
sudo firewall-cmd --reload

# Verificar
sudo firewall-cmd --list-services
```

### iptables/nftables directo

```bash
# NFSv4 — minimalista
sudo iptables -A INPUT -p tcp --dport 2049 -s 192.168.1.0/24 -j ACCEPT

# NFSv3 — múltiples puertos
sudo iptables -A INPUT -p tcp --dport 111 -s 192.168.1.0/24 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 2049 -s 192.168.1.0/24 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 20048 -s 192.168.1.0/24 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 32765 -s 192.168.1.0/24 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 32803 -s 192.168.1.0/24 -j ACCEPT
```

---

## 9. Diagnóstico del servidor

### Verificar que NFS está exportando

```bash
# Ver exportaciones activas con todas las opciones
sudo exportfs -v

# Ver exportaciones desde el punto de vista del cliente (local)
showmount -e localhost
```

Salida de `showmount`:

```
Export list for localhost:
/srv/nfs/shared 192.168.1.0/24
/srv/nfs/docs   192.168.1.0/24
```

### Ver clientes conectados

```bash
# Qué clientes tienen montajes activos
sudo showmount -a

# Solo hostnames de clientes
sudo showmount
```

### Estadísticas del servidor

```bash
# Estadísticas generales de NFS
nfsstat -s

# Solo NFSv4
nfsstat -s -4

# Estadísticas en tiempo real (similar a top)
nfsstat -l
```

### Verificar RPC

```bash
# Servicios RPC registrados
rpcinfo -p

# Probar conectividad RPC desde un cliente
rpcinfo -p 192.168.1.10
```

### Logs del sistema

```bash
# Logs del servidor NFS
sudo journalctl -u nfs-server -f

# Mensajes del kernel NFS
sudo dmesg | grep -i nfs

# Habilitar debug del servidor (temporal)
sudo rpcdebug -m nfsd -s all

# Deshabilitar debug
sudo rpcdebug -m nfsd -c all
```

### Tabla de exportaciones del kernel

```bash
# Lo que el kernel realmente exporta (fuente de verdad)
cat /var/lib/nfs/etab

# Tabla de montajes de clientes
cat /var/lib/nfs/rmtab
```

`/var/lib/nfs/etab` muestra las exportaciones con **todas** las opciones expandidas (incluyendo defaults). Es útil para ver exactamente qué opciones se aplicaron.

---

## 10. Errores comunes

### Error 1 — Espacio antes del paréntesis en /etc/exports

```bash
# MAL — exporta a todo el mundo con rw
/srv/nfs/shared    192.168.1.0/24 (rw,sync)

# BIEN — exporta solo a la subred indicada
/srv/nfs/shared    192.168.1.0/24(rw,sync)
```

`exportfs -v` revela el error: aparecerá una entrada con `*` como cliente.

### Error 2 — Olvidar exportfs -ra tras editar /etc/exports

```bash
# Edité /etc/exports pero el cliente no ve los cambios...
# Porque falta aplicar la configuración:
sudo exportfs -ra

# Verificar
sudo exportfs -v
```

Editar `/etc/exports` no tiene efecto automático. Siempre ejecutar `exportfs -ra` después.

### Error 3 — Permisos del directorio exportado

```bash
# El cliente monta pero no puede escribir...
# Verificar permisos en el SERVIDOR:
ls -la /srv/nfs/

# Con root_squash (default), root del cliente es nobody.
# El directorio debe permitir escritura a nobody o al grupo:
sudo chmod 775 /srv/nfs/shared
sudo chown nobody:nogroup /srv/nfs/shared
```

Tener `rw` en `/etc/exports` no basta: los permisos Unix del directorio también deben permitir la escritura al UID mapeado.

### Error 4 — Firewall bloqueando puertos NFS

```bash
# showmount -e server → "clnt_create: RPC: Port mapper failure"
# Significa que el puerto 111 (rpcbind) o 2049 (nfs) están bloqueados

# Diagnóstico rápido desde el cliente:
nc -zv 192.168.1.10 2049
nc -zv 192.168.1.10 111

# Solución en el servidor:
sudo firewall-cmd --permanent --add-service=nfs
sudo firewall-cmd --permanent --add-service=rpc-bind
sudo firewall-cmd --reload
```

### Error 5 — Usar async sin entender las consecuencias

```bash
# "Puse async para mejorar rendimiento"
/srv/nfs/data    *(rw,async)

# Problema: si el servidor pierde energía, los datos que el cliente
# cree escritos pueden no estar en disco → corrupción silenciosa.

# Solución: usar sync (default) y optimizar rendimiento con
# mejores discos o write-back cache con batería (BBU):
/srv/nfs/data    192.168.1.0/24(rw,sync,no_subtree_check)
```

---

## 11. Cheatsheet

```bash
# ── Instalación ──────────────────────────────────────────
apt install nfs-kernel-server        # Debian/Ubuntu
dnf install nfs-utils                # RHEL/Fedora
systemctl enable --now nfs-server    # Habilitar e iniciar

# ── /etc/exports — sintaxis ──────────────────────────────
/ruta  IP(opciones)                  # Exportar a IP específica
/ruta  192.168.1.0/24(rw,sync)       # Exportar a subred
/ruta  *.dom.com(ro,sync)            # Wildcard DNS
/ruta  *(ro,sync)                    # Todos (cuidado)

# ── Opciones clave ───────────────────────────────────────
rw / ro                              # Lectura-escritura / solo lectura
sync / async                         # Sync seguro / async rápido
root_squash                          # root → nobody (default)
no_root_squash                       # root conserva UID 0 (peligroso)
all_squash                           # Todos → nobody
anonuid=N,anongid=N                  # UID/GID para usuarios squashed
no_subtree_check                     # Recomendado (evita stale handles)
sec=sys|krb5|krb5i|krb5p             # Modo de seguridad

# ── exportfs — gestión dinámica ──────────────────────────
exportfs -v                          # Ver exportaciones activas
exportfs -ra                         # Reaplicar /etc/exports
exportfs -u client:/path             # Desexportar
exportfs -o opts client:/path        # Exportar temporal

# ── Diagnóstico ──────────────────────────────────────────
showmount -e localhost               # Listar exports
showmount -a                         # Ver clientes montados
rpcinfo -p                           # Servicios RPC
nfsstat -s                           # Estadísticas servidor
cat /var/lib/nfs/etab                # Exports con todos los defaults

# ── Firewall ─────────────────────────────────────────────
firewall-cmd --add-service=nfs       # NFSv4 (solo 2049)
firewall-cmd --add-service=rpc-bind  # NFSv3 (+ 111)
firewall-cmd --add-service=mountd    # NFSv3 (+ mountd)

# ── Debug ────────────────────────────────────────────────
rpcdebug -m nfsd -s all              # Activar debug kernel
rpcdebug -m nfsd -c all              # Desactivar debug
journalctl -u nfs-server -f          # Logs del servicio
```

---

## 12. Ejercicios

### Ejercicio 1 — Configurar un export básico

Configura un servidor NFS que exporte `/srv/nfs/datos` con lectura-escritura para la subred `192.168.1.0/24`.

```bash
# 1. Crear el directorio
sudo mkdir -p /srv/nfs/datos
sudo chown nobody:nogroup /srv/nfs/datos

# 2. Configurar /etc/exports
echo '/srv/nfs/datos    192.168.1.0/24(rw,sync,no_subtree_check)' | sudo tee -a /etc/exports

# 3. Aplicar
sudo exportfs -ra

# 4. Verificar
sudo exportfs -v
```

**Pregunta de predicción:** Si el directorio `/srv/nfs/datos` tuviera permisos `750` y propietario `root:root`, ¿podría un usuario normal del cliente escribir archivos? ¿Por qué?

> **Respuesta:** No. Aunque `/etc/exports` dice `rw`, los permisos Unix del directorio (`750` = `rwxr-x---`) no permiten escritura a "otros". Los usuarios normales del cliente mantienen su UID (por `root_squash` sólo afecta a root), y si ese UID no coincide con root ni pertenece al grupo root, los permisos Unix bloquean la escritura. NFS aplica la restricción más estricta entre exports y permisos de filesystem.

---

### Ejercicio 2 — Exports con diferentes permisos por cliente

Configura `/srv/nfs/proyectos` para que el servidor de backup (`192.168.1.50`) tenga `rw` con `no_root_squash`, y el resto de la LAN (`192.168.1.0/24`) tenga `ro`.

```bash
# /etc/exports
/srv/nfs/proyectos    192.168.1.50(rw,sync,no_root_squash,no_subtree_check)  192.168.1.0/24(ro,sync,no_subtree_check)
```

```bash
# Aplicar y verificar
sudo exportfs -ra
sudo exportfs -v
```

**Pregunta de predicción:** Si `192.168.1.50` coincide tanto con la regla específica como con la subred `192.168.1.0/24`, ¿qué opciones se aplican? ¿Obtiene `rw,no_root_squash` o `ro`?

> **Respuesta:** Obtiene `rw,no_root_squash`. NFS selecciona la entrada **más específica** que coincida con el cliente. Una IP individual es más específica que una subred, por lo que `192.168.1.50(rw,sync,no_root_squash)` tiene prioridad sobre `192.168.1.0/24(ro,sync)`.

---

### Ejercicio 3 — Diagnóstico de un export roto

Un administrador reporta que configuró NFS pero el cliente no puede montar. Investiga paso a paso.

```bash
# Paso 1 — ¿El servicio está corriendo?
sudo systemctl status nfs-server

# Paso 2 — ¿Qué se está exportando realmente?
sudo exportfs -v

# Paso 3 — ¿El kernel confirma las exports?
cat /var/lib/nfs/etab

# Paso 4 — ¿Los puertos están abiertos?
rpcinfo -p localhost
ss -tlnp | grep -E '2049|111'

# Paso 5 — ¿El firewall permite el tráfico?
sudo firewall-cmd --list-services

# Paso 6 — ¿Desde el cliente se ve el export?
showmount -e 192.168.1.10

# Paso 7 — ¿Hay errores en los logs?
sudo journalctl -u nfs-server --since "10 minutes ago"
```

**Pregunta de predicción:** Si `exportfs -v` muestra la exportación correcta pero `showmount -e` desde el cliente da `clnt_create: RPC: Port mapper failure`, ¿dónde está el problema?

> **Respuesta:** El problema está en el firewall o la conectividad de red. `showmount` usa RPC que contacta primero a `rpcbind` (puerto 111). Si ese puerto está bloqueado por el firewall del servidor, el cliente no puede ni descubrir los exports. La solución es abrir el servicio `rpc-bind` en el firewall (o usar NFSv4 puro, que no necesita `rpcbind` ni `showmount`).

---

> **Siguiente:** T02 — Cliente NFS (montaje manual, fstab, autofs).
