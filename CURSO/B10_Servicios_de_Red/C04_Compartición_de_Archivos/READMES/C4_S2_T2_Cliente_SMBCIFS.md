# Cliente SMB/CIFS

## Índice
1. [Herramientas del cliente](#1-herramientas-del-cliente)
2. [smbclient — cliente interactivo](#2-smbclient--cliente-interactivo)
3. [Montaje con mount.cifs](#3-montaje-con-mountcifs)
4. [Opciones de montaje](#4-opciones-de-montaje)
5. [Montaje persistente con fstab](#5-montaje-persistente-con-fstab)
6. [Archivos de credenciales](#6-archivos-de-credenciales)
7. [Autofs para shares SMB](#7-autofs-para-shares-smb)
8. [Acceso desde Windows y macOS](#8-acceso-desde-windows-y-macos)
9. [Diagnóstico del cliente](#9-diagnóstico-del-cliente)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Herramientas del cliente

Linux ofrece varias formas de acceder a shares SMB/CIFS:

```
┌──────────────────────────────────────────────────────────┐
│            Herramientas cliente SMB en Linux               │
│                                                          │
│  ┌─────────────┐  Interactivo, similar a FTP              │
│  │  smbclient   │  Transferencia puntual de archivos      │
│  │              │  No monta filesystem                    │
│  └─────────────┘                                          │
│                                                          │
│  ┌─────────────┐  Monta el share como filesystem local    │
│  │  mount.cifs  │  Las aplicaciones acceden de forma      │
│  │  (cifs-utils)│  transparente (como NFS)                │
│  └─────────────┘                                          │
│                                                          │
│  ┌─────────────┐  Explorador gráfico (Nautilus, Dolphin)  │
│  │  gvfs-smb   │  smb://server/share en barra de         │
│  │              │  dirección                              │
│  └─────────────┘                                          │
└──────────────────────────────────────────────────────────┘
```

### Instalación

```bash
# Debian/Ubuntu
sudo apt install smbclient cifs-utils

# RHEL/Fedora
sudo dnf install samba-client cifs-utils

# smbclient → cliente interactivo + listado de shares
# cifs-utils → mount.cifs, umount.cifs, cifscreds
```

---

## 2. smbclient — cliente interactivo

`smbclient` funciona de forma similar a un cliente FTP. No monta el share — permite explorar y transferir archivos de forma interactiva.

### Descubrir shares

```bash
# Listar shares disponibles en un servidor
smbclient -L //192.168.1.10 -U juan
# Password: ****
```

```
Sharename       Type      Comment
---------       ----      -------
datos           Disk      Datos compartidos
ingenieria      Disk      Departamento de Ingeniería
IPC$            IPC       IPC Service
juan            Disk      Home Directories

Server               Comment
---------            -------
FILESERVER           Samba 4.19.5

Workgroup            Master
---------            -------
WORKGROUP            FILESERVER
```

```bash
# Listar sin autenticación (guest)
smbclient -L //192.168.1.10 -N

# Listar usando un dominio
smbclient -L //192.168.1.10 -U 'DOMINIO\juan'
```

### Sesión interactiva

```bash
# Conectar a un share
smbclient //192.168.1.10/datos -U juan
```

```
smb: \> help                    # Ver comandos disponibles

# ── Navegación ────────────────────────
smb: \> ls                      # Listar archivos
smb: \> cd subdir               # Cambiar directorio remoto
smb: \> lcd /tmp                # Cambiar directorio LOCAL
smb: \> pwd                     # Directorio remoto actual

# ── Transferencia ─────────────────────
smb: \> get report.txt          # Descargar archivo
smb: \> put local.txt           # Subir archivo
smb: \> mget *.pdf              # Descargar múltiples
smb: \> mput *.log              # Subir múltiples
smb: \> prompt off              # Desactivar confirmación en mget/mput

# ── Gestión ───────────────────────────
smb: \> mkdir nueva_carpeta     # Crear directorio
smb: \> rmdir carpeta_vacia     # Eliminar directorio vacío
smb: \> del archivo.txt         # Eliminar archivo
smb: \> rename old.txt new.txt  # Renombrar

# ── Salir ─────────────────────────────
smb: \> quit
```

### Uso no interactivo (scripting)

```bash
# Ejecutar un comando y salir
smbclient //192.168.1.10/datos -U juan%password -c "ls"

# Descargar un archivo
smbclient //192.168.1.10/datos -U juan%password -c "get report.txt"

# Subir un archivo
smbclient //192.168.1.10/datos -U juan%password -c "put /tmp/backup.tar.gz"

# Múltiples comandos
smbclient //192.168.1.10/datos -U juan%password -c "cd informes; ls; get q1.pdf"

# Backup recursivo de un share
smbclient //192.168.1.10/datos -U juan%password \
    -c "prompt off; recurse on; mget *"
```

> **Seguridad:** Pasar la contraseña con `-U juan%password` la expone en el historial de shell y en `ps`. Para scripts, usar un archivo de autenticación (sección 6).

### Protocolo específico

```bash
# Forzar SMB2
smbclient //server/share -U juan -m SMB2

# Forzar SMB3
smbclient //server/share -U juan -m SMB3

# Ver qué protocolo se negoció
smbclient //server/share -U juan -d 3 2>&1 | grep "protocol"
```

---

## 3. Montaje con mount.cifs

`mount.cifs` monta un share SMB como un filesystem local, permitiendo que cualquier aplicación acceda a los archivos de forma transparente.

### Montaje básico

```bash
# Crear punto de montaje
sudo mkdir -p /mnt/smb/datos

# Montar
sudo mount -t cifs //192.168.1.10/datos /mnt/smb/datos -o username=juan
# Password: ****

# Verificar
df -hT /mnt/smb/datos
mount | grep cifs
```

### Montaje con opciones completas

```bash
# Especificar usuario, dominio y versión
sudo mount -t cifs //192.168.1.10/datos /mnt/smb/datos \
    -o username=juan,password=secreto,domain=WORKGROUP,vers=3.0

# Montar con UID/GID local (para permisos de archivos)
sudo mount -t cifs //192.168.1.10/datos /mnt/smb/datos \
    -o username=juan,uid=1000,gid=1000,file_mode=0644,dir_mode=0755
```

### Montaje como usuario normal (sin sudo)

Para que un usuario no root pueda montar CIFS:

```bash
# Opción 1: entrada en fstab con "user" o "users"
# (ver sección 5)

# Opción 2: suid en mount.cifs (no recomendado en producción)
sudo chmod u+s /sbin/mount.cifs
```

### Desmontar

```bash
# Desmontaje normal
sudo umount /mnt/smb/datos

# Lazy unmount (si está busy)
sudo umount -l /mnt/smb/datos

# Forzar (cierra conexiones)
sudo umount -f /mnt/smb/datos
```

---

## 4. Opciones de montaje

### Autenticación

| Opción | Significado |
|--------|------------|
| `username=USER` | Usuario para autenticación |
| `password=PASS` | Contraseña (evitar, usar credentials) |
| `domain=DOM` | Dominio o workgroup |
| `credentials=/path` | Archivo con credenciales (recomendado) |
| `guest` | Conectar como invitado (sin usuario) |
| `sec=ntlmssp` | Método de autenticación |

### Permisos y propiedad

```
┌──────────────────────────────────────────────────────────┐
│       Permisos en montajes CIFS                           │
│                                                          │
│  SMB no tiene UIDs/GIDs Unix. Al montar, hay que         │
│  indicar qué usuario/grupo local "posee" los archivos:   │
│                                                          │
│  uid=1000      → Todos los archivos aparecen de UID 1000 │
│  gid=1000      → Todos los archivos aparecen de GID 1000 │
│  file_mode=0644 → Permisos de archivos                   │
│  dir_mode=0755  → Permisos de directorios                │
│                                                          │
│  Sin uid/gid, todo aparece como root:root                │
│                                                          │
│  IMPORTANTE: estos son permisos COSMÉTICOS locales.       │
│  El control de acceso real lo hace el servidor Samba      │
│  basándose en el usuario autenticado.                    │
└──────────────────────────────────────────────────────────┘
```

| Opción | Significado | Default |
|--------|------------|---------|
| `uid=N` | UID propietario de archivos | 0 (root) |
| `gid=N` | GID propietario de archivos | 0 (root) |
| `file_mode=0NNN` | Permisos de archivos | 0755 |
| `dir_mode=0NNN` | Permisos de directorios | 0755 |
| `forceuid` | Forzar uid incluso si el servidor envía info | No |
| `forcegid` | Forzar gid incluso si el servidor envía info | No |
| `noperm` | No verificar permisos locales (dejar al servidor) | No |

### Versión de protocolo

| Opción | Versión SMB |
|--------|------------|
| `vers=2.0` | SMB 2.0 |
| `vers=2.1` | SMB 2.1 |
| `vers=3.0` | SMB 3.0 |
| `vers=3.1.1` | SMB 3.1.1 |
| `vers=default` | Negocia la más alta (default) |

> **SMB1 fue eliminado** del kernel Linux 5.15+. Ya no es posible montar con `vers=1.0`.

### Rendimiento y resiliencia

| Opción | Significado |
|--------|------------|
| `cache=strict` | Coherencia de caché estricta |
| `cache=loose` | Caché agresivo (mejor rendimiento, menos coherencia) |
| `cache=none` | Sin caché (cada lectura va al servidor) |
| `rsize=N` | Tamaño de lectura máximo |
| `wsize=N` | Tamaño de escritura máximo |
| `actimeo=N` | Timeout de caché de atributos (segundos) |
| `echo_interval=N` | Intervalo de keepalive (segundos, default 60) |
| `multichannel` | Múltiples conexiones TCP (kernel 5.17+, SMB3) |

### Cifrado

```bash
# Forzar cifrado de transporte (SMB3)
sudo mount -t cifs //server/share /mnt -o username=juan,seal

# seal → cifra todo el tráfico SMB3 (AES-128-GCM / AES-256-GCM)
```

| Opción | Significado |
|--------|------------|
| `seal` | Cifrar todo el tráfico (SMB3 requerido) |
| `sign` | Firmar paquetes (integridad, sin cifrado) |

---

## 5. Montaje persistente con fstab

### Entrada básica en fstab

```bash
# /etc/fstab
//192.168.1.10/datos  /mnt/smb/datos  cifs  credentials=/root/.smbcreds,_netdev  0  0
```

Al igual que NFS, **`_netdev`** es obligatorio para evitar que el arranque se bloquee esperando la red.

### Ejemplos de fstab

```bash
# Share con credenciales y permisos
//server/datos  /mnt/datos  cifs  credentials=/root/.smbcreds,uid=1000,gid=1000,file_mode=0644,dir_mode=0755,_netdev,nofail  0  0

# Share de solo lectura
//server/docs  /mnt/docs  cifs  credentials=/root/.smbcreds,ro,_netdev,nofail  0  0

# Share con cifrado SMB3
//server/secure  /mnt/secure  cifs  credentials=/root/.smbcreds,vers=3.0,seal,_netdev,nofail  0  0

# Share montable por usuario (no root)
//server/shared  /mnt/shared  cifs  credentials=/home/juan/.smbcreds,users,uid=1000,gid=1000,_netdev,nofail  0  0
```

### Probar

```bash
# Montar todo lo de fstab
sudo mount -a

# Verificar
mount -t cifs
df -hT | grep cifs
```

---

## 6. Archivos de credenciales

Poner la contraseña en fstab o en la línea de comandos es un riesgo de seguridad. Un archivo de credenciales con permisos restrictivos es la solución.

### Crear archivo de credenciales

```bash
# Crear el archivo
sudo vim /root/.smbcreds
```

```
username=juan
password=mi_contraseña_segura
domain=WORKGROUP
```

```bash
# Permisos restrictivos — solo root puede leer
sudo chmod 600 /root/.smbcreds
sudo chown root:root /root/.smbcreds
```

### Usar en mount

```bash
# En línea de comandos
sudo mount -t cifs //server/datos /mnt/datos -o credentials=/root/.smbcreds

# En fstab
//server/datos  /mnt/datos  cifs  credentials=/root/.smbcreds,_netdev  0  0
```

### Credenciales por usuario (para montajes con "users")

```bash
# Archivo en el home del usuario
vim /home/juan/.smbcreds
chmod 600 /home/juan/.smbcreds

# fstab con opción "users" permite que juan monte sin sudo
//server/shared  /mnt/shared  cifs  credentials=/home/juan/.smbcreds,users,uid=1000,gid=1000,_netdev  0  0
```

### Comparación de métodos de autenticación

```
┌──────────────────────────────────────────────────────────┐
│     Métodos para pasar credenciales SMB                   │
│                                                          │
│  1. Línea de comandos (PEOR)                             │
│     mount -o username=juan,password=secreto              │
│     → Visible en ps, history, /proc/mounts               │
│                                                          │
│  2. Variable de entorno (MALO)                           │
│     export PASSWD=secreto                                │
│     mount -o username=juan                               │
│     → Visible en /proc/PID/environ                       │
│                                                          │
│  3. Prompt interactivo (BUENO para manual)               │
│     mount -o username=juan   # pide password             │
│     → No queda registrado, pero no sirve para fstab      │
│                                                          │
│  4. Archivo credentials (RECOMENDADO)                    │
│     mount -o credentials=/root/.smbcreds                 │
│     → Seguro con chmod 600, funciona en fstab            │
│                                                          │
│  5. Kerberos (MEJOR — en dominio AD)                     │
│     mount -o sec=krb5                                    │
│     → Sin contraseña, usa ticket del usuario             │
└──────────────────────────────────────────────────────────┘
```

---

## 7. Autofs para shares SMB

autofs funciona con shares CIFS igual que con NFS.

### Configuración

```bash
# /etc/auto.master
/mnt/smb    /etc/auto.smb    --timeout=300
```

```bash
# /etc/auto.smb
datos       -fstype=cifs,credentials=/root/.smbcreds,uid=1000,gid=1000    ://192.168.1.10/datos
docs        -fstype=cifs,credentials=/root/.smbcreds,ro                   ://192.168.1.10/docs
```

> **Nota la sintaxis:** el servidor va precedido de `:` (sin `//` inicial, autofs lo agrega). El formato es `://servidor/share`.

```bash
# Reiniciar autofs
sudo systemctl restart autofs

# Probar — acceder al directorio activa el montaje
ls /mnt/smb/datos/
```

### Wildcard para shares SMB

No es tan directo como con NFS porque cada share puede tener un nombre diferente en el servidor. Un script de mapa puede resolverlo:

```bash
# /etc/auto.master
/mnt/smb    /etc/auto.smb.sh    --timeout=300
```

```bash
#!/bin/bash
# /etc/auto.smb.sh — script de mapa ejecutable
# autofs pasa el nombre del subdirectorio como $1

key="$1"
echo "-fstype=cifs,credentials=/root/.smbcreds,uid=1000 ://192.168.1.10/${key}"
```

```bash
sudo chmod 755 /etc/auto.smb.sh
sudo systemctl restart autofs

# Ahora cualquier acceso a /mnt/smb/NOMBRE intenta montar //server/NOMBRE
cd /mnt/smb/datos        # monta //192.168.1.10/datos
cd /mnt/smb/ingenieria   # monta //192.168.1.10/ingenieria
```

---

## 8. Acceso desde Windows y macOS

### Desde Windows

```
┌──────────────────────────────────────────────────────────┐
│  Acceder a un share Samba desde Windows                   │
│                                                          │
│  Explorador de archivos:                                 │
│    \\192.168.1.10\datos                                  │
│    o \\FILESERVER\datos (si NetBIOS funciona)             │
│                                                          │
│  Mapear unidad de red:                                   │
│    Clic derecho en "Este equipo" → Conectar a unidad     │
│    Carpeta: \\192.168.1.10\datos                         │
│    ✓ Conectar con credenciales diferentes                │
│    Usuario: juan (o WORKGROUP\juan)                      │
│                                                          │
│  CMD / PowerShell:                                       │
│    net use Z: \\192.168.1.10\datos /user:juan            │
│    net use Z: /delete                                    │
│                                                          │
│  Listar shares:                                          │
│    net view \\192.168.1.10                               │
└──────────────────────────────────────────────────────────┘
```

### Desde macOS

```
┌──────────────────────────────────────────────────────────┐
│  Acceder a un share Samba desde macOS                     │
│                                                          │
│  Finder:                                                 │
│    Cmd+K → smb://192.168.1.10/datos                      │
│    Aparece en la barra lateral de Finder                 │
│                                                          │
│  Terminal:                                               │
│    mount_smbfs //juan@192.168.1.10/datos /mnt/datos      │
│    # Pide contraseña                                     │
│                                                          │
│  El share se monta en /Volumes/datos                     │
└──────────────────────────────────────────────────────────┘
```

### Problemas comunes con Windows 10/11

Windows 10+ deshabilita SMB1 y guest access por defecto, lo que puede causar problemas con servidores Samba mal configurados:

```ini
# Asegurar en smb.conf del servidor:
[global]
    server min protocol = SMB2_02      # No ofrecer SMB1
    map to guest = Never               # Exigir autenticación
    # O si necesitas guest:
    # map to guest = Bad User
```

---

## 9. Diagnóstico del cliente

### Probar conectividad

```bash
# ¿El puerto 445 está abierto?
nc -zv 192.168.1.10 445

# ¿El servidor responde a SMB?
smbclient -L //192.168.1.10 -U juan

# ¿Puedo autenticarme?
smbclient //192.168.1.10/datos -U juan -c "ls"
```

### Verificar montajes CIFS activos

```bash
# Montajes activos
mount -t cifs

# Con detalles
cat /proc/mounts | grep cifs

# Estadísticas detalladas
cat /proc/fs/cifs/Stats
```

### Debug de montaje

```bash
# Montaje con verbose para diagnóstico
sudo mount -t cifs //server/datos /mnt/datos -o username=juan -v

# Debug detallado del módulo kernel CIFS
echo 7 | sudo tee /proc/fs/cifs/cifsFYI
sudo dmesg -w | grep -i cifs

# Restaurar nivel normal
echo 0 | sudo tee /proc/fs/cifs/cifsFYI
```

### Información del módulo CIFS

```bash
# Versión del módulo
modinfo cifs | grep -E "^(version|description)"

# Sesiones activas del módulo
cat /proc/fs/cifs/DebugData
```

### findsmb — descubrir servidores en la red

```bash
# Buscar servidores SMB en la red local (broadcast)
findsmb

# Salida:
# IP           NETBIOS NAME   WORKGROUP   VERSION
# 192.168.1.10 FILESERVER     WORKGROUP   Samba 4.19.5
# 192.168.1.20 WIN-PC1        WORKGROUP   Windows 10
```

> `findsmb` usa NetBIOS broadcast, que no funciona a través de routers. En redes modernas sin NetBIOS, usar `smbclient -L` directamente con la IP.

---

## 10. Errores comunes

### Error 1 — Contraseña en fstab visible para todos

```bash
# MAL — la contraseña está en texto plano en fstab
//server/datos  /mnt/datos  cifs  username=juan,password=secreto,_netdev  0  0

# /etc/fstab tiene permisos 644 — cualquier usuario puede leerlo
ls -l /etc/fstab
# -rw-r--r-- root root /etc/fstab

# BIEN — usar archivo de credenciales con permisos 600
//server/datos  /mnt/datos  cifs  credentials=/root/.smbcreds,_netdev  0  0
```

### Error 2 — Archivos montados como root:root

```bash
# Montar sin uid/gid:
sudo mount -t cifs //server/datos /mnt/datos -o credentials=/root/.smbcreds
ls -l /mnt/datos/
# -rwxr-xr-x root root ... archivo.txt
# El usuario normal no puede escribir (permisos locales)

# Solución: especificar uid/gid del usuario
sudo mount -t cifs //server/datos /mnt/datos \
    -o credentials=/root/.smbcreds,uid=1000,gid=1000

ls -l /mnt/datos/
# -rwxr-xr-x juan juan ... archivo.txt
```

### Error 3 — mount error(13): Permission denied

```bash
# Causas posibles:
# 1. Contraseña incorrecta
# 2. Usuario no existe en Samba (existe en Unix pero no en smbpasswd)
# 3. valid users en smb.conf no incluye al usuario
# 4. hosts allow bloquea la IP del cliente

# Diagnóstico paso a paso:
# Probar con smbclient primero (más verbose)
smbclient //server/datos -U juan -c "ls"

# Si smbclient funciona pero mount falla, verificar versión SMB:
sudo mount -t cifs //server/datos /mnt -o username=juan,vers=3.0
```

### Error 4 — mount error(112): Host is down

```bash
# Este error es engañoso — no siempre significa que el host esté caído

# Causa habitual: negociación de protocolo falló
# El cliente intenta SMB1 pero el servidor lo rechaza (o viceversa)

# Solución: especificar versión
sudo mount -t cifs //server/datos /mnt -o username=juan,vers=3.0

# Si el servidor es muy antiguo y solo habla SMB1:
# (requiere kernel < 5.15 o módulo cifs compilado con SMB1)
sudo mount -t cifs //server/datos /mnt -o username=juan,vers=1.0
```

### Error 5 — Caracteres especiales en contraseña rompen el montaje

```bash
# Si la contraseña tiene caracteres como , ; = &
# pueden interferir con el parsing de opciones de mount

# Solución 1: usar archivo de credenciales (siempre funciona)
# El archivo se lee línea por línea, sin parsing de separadores

# Solución 2: escapar en línea de comandos
sudo mount -t cifs //server/datos /mnt \
    -o 'username=juan,password=p@ss,w0rd!'
# Comillas simples protegen los caracteres especiales
```

---

## 11. Cheatsheet

```bash
# ── Instalación ──────────────────────────────────────────
apt install smbclient cifs-utils     # Debian/Ubuntu
dnf install samba-client cifs-utils  # RHEL/Fedora

# ── smbclient ─────────────────────────────────────────────
smbclient -L //SERVER -U USER       # Listar shares
smbclient //SERVER/SHARE -U USER    # Sesión interactiva
smbclient //SRV/SH -U USER -c "ls"  # Comando no interactivo
smbclient //SRV/SH -U USER%PASS     # Con password (inseguro)
# Comandos interactivos: ls, cd, get, put, mget, mput, mkdir, del, quit

# ── mount.cifs ────────────────────────────────────────────
mount -t cifs //SRV/SH /mnt -o username=USER           # Básico
mount -t cifs //SRV/SH /mnt -o credentials=/root/.creds # Seguro
mount -t cifs //SRV/SH /mnt -o credentials=...,uid=1000,gid=1000  # Con UID
umount /mnt                          # Desmontar
umount -l /mnt                       # Lazy unmount

# ── Archivo de credenciales ──────────────────────────────
# /root/.smbcreds (chmod 600):
# username=USER
# password=PASS
# domain=WORKGROUP

# ── fstab ─────────────────────────────────────────────────
# //SRV/SH  /mnt  cifs  credentials=/root/.smbcreds,_netdev,nofail  0  0
# Con permisos: ...,uid=1000,gid=1000,file_mode=0644,dir_mode=0755
mount -a                             # Montar todo de fstab

# ── autofs ────────────────────────────────────────────────
# /etc/auto.master:
# /mnt/smb  /etc/auto.smb  --timeout=300
# /etc/auto.smb:
# datos  -fstype=cifs,credentials=/root/.creds  ://SERVER/datos

# ── Opciones importantes ─────────────────────────────────
credentials=/path       # Archivo con user/pass
uid=N,gid=N             # Propietario local
file_mode=0644          # Permisos de archivos
dir_mode=0755           # Permisos de directorios
vers=3.0                # Versión SMB
seal                    # Cifrado SMB3
_netdev                 # Esperar red (fstab)
nofail                  # No bloquear arranque

# ── Diagnóstico ──────────────────────────────────────────
nc -zv SERVER 445                    # Puerto abierto?
smbclient -L //SERVER -U USER       # Listar shares
mount -t cifs                        # Montajes activos
cat /proc/fs/cifs/Stats              # Estadísticas kernel
echo 7 > /proc/fs/cifs/cifsFYI      # Debug on
echo 0 > /proc/fs/cifs/cifsFYI      # Debug off
dmesg | grep -i cifs                 # Mensajes kernel
```

---

## 12. Ejercicios

### Ejercicio 1 — Montar un share con credenciales seguras

Monta el share `//192.168.1.10/datos` de forma segura y persistente.

```bash
# 1. Crear archivo de credenciales
sudo bash -c 'cat > /root/.smbcreds << EOF
username=juan
password=mi_clave_segura
domain=WORKGROUP
EOF'
sudo chmod 600 /root/.smbcreds

# 2. Crear punto de montaje
sudo mkdir -p /mnt/smb/datos

# 3. Probar montaje manual
sudo mount -t cifs //192.168.1.10/datos /mnt/smb/datos \
    -o credentials=/root/.smbcreds,uid=1000,gid=1000

# 4. Verificar
df -hT /mnt/smb/datos
ls -l /mnt/smb/datos/

# 5. Agregar a fstab
echo '//192.168.1.10/datos  /mnt/smb/datos  cifs  credentials=/root/.smbcreds,uid=1000,gid=1000,_netdev,nofail  0  0' | sudo tee -a /etc/fstab

# 6. Probar fstab
sudo umount /mnt/smb/datos
sudo mount -a
```

**Pregunta de predicción:** Si el archivo `/root/.smbcreds` tiene permisos `644` en lugar de `600`, ¿funciona el montaje? ¿Cuál es el riesgo?

> **Respuesta:** Sí, el montaje funciona — `mount.cifs` no verifica los permisos del archivo de credenciales. Pero con `644`, cualquier usuario del sistema puede leer la contraseña SMB con `cat /root/.smbcreds`. Aunque el directorio `/root/` tiene `700`, un administrador podría cambiar eso o el archivo podría copiarse a otro lugar. `chmod 600` es la práctica correcta como defensa en profundidad.

---

### Ejercicio 2 — smbclient para backup scriptado

Escribe un script que descargue todos los archivos PDF de un share remoto.

```bash
#!/bin/bash
# backup_pdfs.sh — descarga PDFs de un share SMB

SERVER="192.168.1.10"
SHARE="informes"
CREDS="/root/.smbcreds"
DEST="/backup/pdfs"

mkdir -p "$DEST"
cd "$DEST" || exit 1

# Autenticación desde archivo de credenciales
AUTH="-A ${CREDS}"

smbclient "//${SERVER}/${SHARE}" $AUTH -c "
    prompt off
    recurse on
    mask *.pdf
    mget *
"

echo "Backup completado: $(ls -1 *.pdf 2>/dev/null | wc -l) archivos PDF"
```

```bash
chmod 700 backup_pdfs.sh
```

**Pregunta de predicción:** ¿Qué hace `prompt off` en la sesión smbclient? ¿Qué pasaría si se omite al ejecutar `mget *`?

> **Respuesta:** `prompt off` desactiva la confirmación interactiva archivo por archivo. Sin esta opción, `mget *` preguntaría "¿Descargar archivo1.pdf? (y/n)" por cada archivo encontrado, y como el script no es interactivo, cada pregunta recibiría EOF y no descargaría nada. Es obligatorio para uso no interactivo de `mget`/`mput`.

---

### Ejercicio 3 — Comparar métodos de acceso SMB

Configura acceso al share `//server/proyectos` usando tres métodos y evalúa cuándo usar cada uno.

```bash
# ── Método 1: smbclient ──
smbclient //server/proyectos -U juan -c "ls"
# Ventaja: no necesita root, no monta filesystem
# Uso: exploración puntual, transferencias únicas

# ── Método 2: mount.cifs + fstab ──
# /etc/fstab:
# //server/proyectos  /mnt/proyectos  cifs  credentials=/root/.smbcreds,uid=1000,_netdev,nofail  0  0
sudo mount -a
ls /mnt/proyectos/
# Ventaja: acceso transparente para todas las aplicaciones
# Uso: share que se necesita permanentemente

# ── Método 3: autofs ──
# /etc/auto.master:
# /mnt/smb  /etc/auto.smb  --timeout=300
# /etc/auto.smb:
# proyectos  -fstype=cifs,credentials=/root/.smbcreds,uid=1000  ://server/proyectos
ls /mnt/smb/proyectos/
# Ventaja: monta bajo demanda, no bloquea arranque
# Uso: share accedido esporádicamente
```

| Criterio | smbclient | mount.cifs/fstab | autofs |
|----------|-----------|------------------|--------|
| Necesita root | No | Sí (mount) | Sí (config) |
| Transparente a apps | No | Sí | Sí |
| Persistente | No | Sí | Sí (bajo demanda) |
| Bloquea arranque | — | Sin nofail sí | No |
| Scripting | Bueno | N/A | N/A |
| Múltiples shares | Reconectar cada vez | Una línea por share | Wildcard posible |

**Pregunta de predicción:** Una aplicación Java necesita leer archivos de `//server/proyectos` como si fuera un directorio local. ¿Puede usar smbclient? ¿Qué método es adecuado?

> **Respuesta:** No puede usar smbclient — es una herramienta interactiva/CLI, no un filesystem. La aplicación Java usa llamadas POSIX estándar (`open()`, `read()`) y necesita un path local real. Se necesita `mount.cifs` (o autofs) para montar el share como filesystem en, por ejemplo, `/mnt/proyectos`. Entonces la app accede a `/mnt/proyectos/archivo.txt` como si fuera local.

---

> **Siguiente:** T03 — Integración con usuarios Linux (smbpasswd, mapping de permisos).
