# Configuración de Samba

## Índice
1. [Qué es Samba](#1-qué-es-samba)
2. [Arquitectura y demonios](#2-arquitectura-y-demonios)
3. [Instalación](#3-instalación)
4. [smb.conf — estructura y secciones](#4-smbconf--estructura-y-secciones)
5. [Sección [global]](#5-sección-global)
6. [Definir shares](#6-definir-shares)
7. [Gestión de usuarios Samba](#7-gestión-de-usuarios-samba)
8. [Shares comunes preconfigurados](#8-shares-comunes-preconfigurados)
9. [Validación y diagnóstico](#9-validación-y-diagnóstico)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es Samba

**Samba** implementa el protocolo **SMB/CIFS** en Linux, permitiendo compartir archivos e impresoras con equipos Windows. Mientras NFS es el estándar entre equipos Unix, SMB es el protocolo nativo de Windows para compartir recursos de red.

```
┌──────────────────────────────────────────────────────────┐
│                    Red heterogénea                        │
│                                                          │
│  ┌──────────┐    SMB/CIFS     ┌──────────┐              │
│  │  Windows  │◄──────────────►│  Linux    │              │
│  │  Client   │   Puerto 445   │  Samba    │              │
│  │           │                │  Server   │              │
│  └──────────┘                 └──────────┘              │
│                                    ▲                     │
│  ┌──────────┐    SMB/CIFS         │                     │
│  │  macOS    │◄───────────────────┘                     │
│  │  Client   │                                          │
│  └──────────┘                                           │
│                                    ▲                     │
│  ┌──────────┐    SMB/CIFS         │                     │
│  │  Linux    │◄───────────────────┘                     │
│  │  Client   │   (smbclient / mount.cifs)               │
│  └──────────┘                                           │
└──────────────────────────────────────────────────────────┘
```

### Protocolos SMB

| Versión | Introducida con | Notas |
|---------|----------------|-------|
| SMB1/CIFS | Windows NT 4.0 | **Obsoleto y peligroso** (WannaCry) |
| SMB2.0 | Windows Vista | Menos comandos, mejor rendimiento |
| SMB2.1 | Windows 7 | Leases, large MTU |
| SMB3.0 | Windows 8 | Cifrado, multichannel, failover |
| SMB3.1.1 | Windows 10 | Pre-auth integrity, AES-128-GCM |

Samba 4.x soporta hasta SMB3.1.1 y puede actuar como **controlador de dominio Active Directory** completo.

### Capacidades de Samba

```
┌────────────────────────────────────────────────────┐
│              Roles de Samba                         │
│                                                    │
│  1. Servidor de archivos (file server)             │
│     → Compartir directorios con Windows/Mac/Linux  │
│                                                    │
│  2. Servidor de impresión (print server)           │
│     → Compartir impresoras Linux con Windows       │
│                                                    │
│  3. Controlador de dominio AD (Samba AD DC)        │
│     → Reemplazar Windows Server como DC            │
│     → Autenticación, GPOs, DNS integrado           │
│                                                    │
│  4. Miembro de dominio (domain member)             │
│     → Unir Linux a un dominio AD existente         │
│     → Autenticación centralizada                   │
│                                                    │
│  Este tema cubre el rol 1 (servidor de archivos)   │
└────────────────────────────────────────────────────┘
```

---

## 2. Arquitectura y demonios

### Demonios principales

```
┌──────────────────────────────────────────────────────────┐
│                  Demonios de Samba                        │
│                                                          │
│  ┌────────────────┐     ┌────────────────┐              │
│  │     smbd       │     │     nmbd       │              │
│  │                │     │                │              │
│  │  Servicio SMB  │     │  NetBIOS name  │              │
│  │  Archivos +    │     │  resolution    │              │
│  │  impresoras +  │     │  Browsing      │              │
│  │  autenticación │     │  (legacy)      │              │
│  │                │     │                │              │
│  │  Puerto:       │     │  Puerto:       │              │
│  │  445 (SMB)     │     │  137/138 UDP   │              │
│  │  139 (NetBIOS) │     │  (NetBIOS)     │              │
│  └────────────────┘     └────────────────┘              │
│                                                          │
│  ┌────────────────┐                                     │
│  │     winbindd   │                                     │
│  │                │                                     │
│  │  Integración   │     smbd = siempre necesario        │
│  │  con AD/NT     │     nmbd = solo si hay clientes     │
│  │  Mapeo de      │           legacy sin DNS            │
│  │  usuarios      │     winbindd = solo en miembros     │
│  │  Windows↔Linux │              de dominio AD          │
│  └────────────────┘                                     │
└──────────────────────────────────────────────────────────┘
```

### Puertos

| Puerto | Protocolo | Servicio | Demonio |
|--------|----------|----------|---------|
| 137 | UDP | NetBIOS Name Service | nmbd |
| 138 | UDP | NetBIOS Datagram | nmbd |
| 139 | TCP | NetBIOS Session (SMB over NetBIOS) | smbd |
| 445 | TCP | SMB directo (sin NetBIOS) | smbd |

En redes modernas, sólo el puerto **445 TCP** es necesario. Los puertos 137-139 son para compatibilidad con equipos muy antiguos que no soportan SMB directo.

---

## 3. Instalación

### Debian/Ubuntu

```bash
# Instalar servidor Samba
sudo apt install samba

# Verificar versión
smbd --version
# Version 4.19.x

# El servicio se habilita automáticamente
sudo systemctl status smbd
sudo systemctl status nmbd
```

### RHEL/Fedora

```bash
# Instalar servidor Samba
sudo dnf install samba

# Habilitar e iniciar
sudo systemctl enable --now smb nmb

# Verificar
sudo systemctl status smb
```

> **Nota de nombres:** En Debian el servicio se llama `smbd`/`nmbd`. En RHEL se llama `smb`/`nmb`. El binario siempre es `smbd`/`nmbd`.

### Firewall

```bash
# firewalld (RHEL/Fedora)
sudo firewall-cmd --permanent --add-service=samba
sudo firewall-cmd --reload

# El servicio "samba" abre: 137/udp, 138/udp, 139/tcp, 445/tcp
```

---

## 4. smb.conf — estructura y secciones

El archivo de configuración principal es `/etc/samba/smb.conf`. Su formato es similar a los archivos `.ini` de Windows.

### Ubicación

```bash
# Ubicación estándar
/etc/samba/smb.conf

# Ver dónde Samba busca su configuración
smbd -b | grep CONFIGFILE
# CONFIGFILE: /etc/samba/smb.conf
```

### Estructura general

```ini
# /etc/samba/smb.conf

# ── Sección global ─────────────────────
[global]
    workgroup = WORKGROUP
    server string = Samba Server
    security = user

# ── Shares especiales ─────────────────
[homes]
    comment = Home Directories
    browseable = no
    writable = yes

[printers]
    comment = All Printers
    path = /var/spool/samba
    printable = yes

# ── Shares personalizados ─────────────
[datos]
    comment = Datos compartidos
    path = /srv/samba/datos
    writable = yes
    valid users = @staff
```

### Reglas de sintaxis

```ini
# Comentarios: líneas que empiezan con # o ;
# Esto es un comentario
; Esto también es un comentario

# Parámetros: nombre = valor
workgroup = WORKGROUP

# Nombres de sección: entre corchetes
[global]
[nombre_del_share]

# Booleanos: yes/no, true/false, 1/0
writable = yes
browseable = no
read only = false

# Listas: separadas por comas o espacios
valid users = juan, maria, @grupo
hosts allow = 192.168.1. 10.0.0.

# Continuación de línea: backslash al final
hosts allow = 192.168.1. \
              10.0.0.

# Los parámetros NO son case-sensitive
# Workgroup = WORKGROUP  equivale a  workgroup = WORKGROUP

# Los nombres de sección NO son case-sensitive
# [Datos] equivale a [datos]
```

### Secciones reservadas

| Sección | Propósito |
|---------|-----------|
| `[global]` | Configuración del servidor |
| `[homes]` | Auto-share del home de cada usuario |
| `[printers]` | Auto-share de impresoras CUPS |
| `[print$]` | Drivers de impresora |
| Cualquier otro nombre | Share personalizado |

---

## 5. Sección [global]

La sección `[global]` define el comportamiento general del servidor.

### Configuración básica

```ini
[global]
    # Identificación en la red
    workgroup = WORKGROUP          # Nombre del grupo de trabajo / dominio
    netbios name = FILESERVER      # Nombre NetBIOS (max 15 chars)
    server string = Samba %v       # Descripción (%v = versión)

    # Modo de seguridad
    security = user                # Autenticación contra base local de Samba
    map to guest = never           # No permitir acceso anónimo

    # Logging
    log file = /var/log/samba/log.%m   # Un log por máquina (%m = cliente)
    max log size = 1000                # KB por archivo de log
    log level = 1                      # 0=mínimo, 10=debug extremo

    # Rendimiento
    socket options = TCP_NODELAY IPTOS_LOWDELAY

    # Protocolo mínimo (seguridad)
    server min protocol = SMB2_02      # Rechazar SMB1
    server max protocol = SMB3_11      # Soportar hasta SMB3.1.1
```

### Modos de seguridad

| Valor | Significado |
|-------|------------|
| `user` | Autenticación con usuario/contraseña Samba **(default, recomendado)** |
| `ads` | Miembro de dominio Active Directory (requiere winbindd) |
| `domain` | Miembro de dominio NT4 (legacy) |

> `share` y `server` fueron eliminados en Samba 4. Ya no existe acceso anónimo sin usuario.

### Control de acceso por red

```ini
[global]
    # Permitir solo desde redes específicas
    hosts allow = 192.168.1.0/24 10.0.0.0/8 127.0.0.1
    hosts deny = ALL

    # Vincular a interfaces específicas
    interfaces = eth0 lo
    bind interfaces only = yes
```

`hosts allow` y `hosts deny` se pueden poner en `[global]` (aplica a todo) o en shares individuales (aplica solo a ese share). Si ambos están presentes, `hosts allow` se evalúa primero.

### Guest access (acceso anónimo)

```ini
[global]
    # Mapear invitados a un usuario Unix
    map to guest = Bad User    # Si el usuario no existe → guest
    guest account = nobody     # Usuario Unix para guest

# En un share específico:
[publico]
    path = /srv/samba/publico
    guest ok = yes             # Permitir acceso sin autenticación
    read only = yes
```

`map to guest = Bad User` significa: si alguien intenta autenticarse con un usuario que no existe en Samba, tratarlo como invitado. Con `never`, se rechaza.

---

## 6. Definir shares

### Anatomía de un share

```ini
[nombre_share]                     # Nombre visible en la red
    comment = Descripción          # Descripción del recurso
    path = /ruta/en/servidor       # Directorio local

    # Acceso
    valid users = juan, @grupo     # Quién puede conectar
    invalid users = becario        # Quién NO puede (prioridad sobre valid)
    read only = yes                # Solo lectura (default)
    writable = yes                 # Lectura-escritura (opuesto de read only)
    write list = juan, @admins     # Estos pueden escribir aunque sea read only
    read list = maria              # Estos solo leen aunque sea writable

    # Permisos de archivos creados
    create mask = 0664             # Permisos para archivos nuevos
    directory mask = 0775          # Permisos para directorios nuevos
    force create mode = 0664       # Bits que SIEMPRE se aplican
    force directory mode = 0775    # Bits que SIEMPRE se aplican

    # Visibilidad
    browseable = yes               # Visible al navegar la red (default)

    # Propietario forzado
    force user = www-data          # Todos los accesos como este usuario
    force group = www-data         # Todos los accesos como este grupo
```

### Ejemplo: share departamental

```ini
[ingenieria]
    comment = Departamento de Ingeniería
    path = /srv/samba/ingenieria
    valid users = @ingenieria
    writable = yes
    create mask = 0660
    directory mask = 0770
    force group = ingenieria
```

```bash
# Crear directorio y asignar permisos
sudo mkdir -p /srv/samba/ingenieria
sudo chgrp ingenieria /srv/samba/ingenieria
sudo chmod 2770 /srv/samba/ingenieria    # SGID para herencia de grupo
```

### Ejemplo: share público de solo lectura

```ini
[documentacion]
    comment = Documentación interna
    path = /srv/samba/docs
    guest ok = yes
    read only = yes
    browseable = yes
```

### Ejemplo: share con permisos diferenciados

```ini
[proyectos]
    comment = Proyectos
    path = /srv/samba/proyectos
    read only = yes                    # Por defecto solo lectura
    valid users = @empresa
    write list = @managers, juan       # Estos pueden escribir
    create mask = 0664
    directory mask = 0775
```

### Interacción permisos Samba vs permisos Unix

```
┌──────────────────────────────────────────────────────────┐
│          Doble capa de permisos                           │
│                                                          │
│  1. Samba evalúa:                                        │
│     ¿El usuario está en valid users?                     │
│     ¿El share es writable o el usuario está en           │
│      write list?                                         │
│                                                          │
│  2. Si Samba permite, Unix evalúa:                       │
│     ¿El usuario Unix tiene permiso rwx en el             │
│      directorio/archivo?                                 │
│                                                          │
│  Resultado = Samba AND Unix                              │
│  Se aplica la restricción MÁS estricta de ambas capas   │
│                                                          │
│  Ejemplo:                                                │
│    Samba: writable = yes                                 │
│    Unix:  chmod 555 /srv/samba/share                     │
│    → NO puede escribir (Unix lo bloquea)                 │
│                                                          │
│  Ejemplo:                                                │
│    Samba: read only = yes                                │
│    Unix:  chmod 777 /srv/samba/share                     │
│    → NO puede escribir (Samba lo bloquea)                │
└──────────────────────────────────────────────────────────┘
```

### create mask vs force create mode

```
                create mask              force create mode
                (AND mask)               (OR mask)
Archivo creado: 0666                     0666
Mask:           0644                     0010
Resultado:      0644 (apaga bits)        0676 (enciende bits)

create mask = 0644
  → Apaga bits que no estén en la máscara
  → 0666 AND 0644 = 0644
  → Elimina escritura para group y other

force create mode = 0010
  → Enciende bits incondicionalmente
  → Se aplica DESPUÉS de create mask
  → Fuerza ejecución para group

Orden: primero create mask, luego force create mode
```

---

## 7. Gestión de usuarios Samba

Samba mantiene su **propia base de datos de contraseñas**, separada de `/etc/shadow`. Un usuario Samba debe existir primero como usuario Unix.

### Flujo de creación de usuario

```
┌──────────────────────────────────────────────────────┐
│          Crear un usuario Samba                       │
│                                                      │
│  1. Crear usuario Unix (si no existe)                │
│     useradd juan                                     │
│                                                      │
│  2. Crear contraseña Samba                           │
│     smbpasswd -a juan                                │
│     (la contraseña Samba puede ser diferente          │
│      de la contraseña Unix)                          │
│                                                      │
│  3. El usuario puede autenticarse vía SMB            │
│     \\SERVIDOR\share → usuario: juan, pass: ****     │
└──────────────────────────────────────────────────────┘
```

### Comandos de gestión

```bash
# Crear usuario Unix sin login (solo para Samba)
sudo useradd -M -s /usr/sbin/nologin juan

# Agregar usuario a la base de datos Samba
sudo smbpasswd -a juan
# New SMB password: ****
# Retype new SMB password: ****

# Cambiar contraseña Samba
sudo smbpasswd juan

# Deshabilitar usuario Samba (sin eliminarlo)
sudo smbpasswd -d juan

# Habilitar usuario previamente deshabilitado
sudo smbpasswd -e juan

# Eliminar usuario de Samba
sudo smbpasswd -x juan

# Listar usuarios Samba
sudo pdbedit -L

# Listar con detalles
sudo pdbedit -L -v
```

### pdbedit — gestión avanzada

```bash
# Listar todos los usuarios con formato detallado
sudo pdbedit -L -v

# Ver un usuario específico
sudo pdbedit -L -v -u juan

# Agregar usuario (alternativa a smbpasswd -a)
sudo pdbedit -a -u juan

# Exportar base de datos (backup)
sudo pdbedit -e smbpasswd:/tmp/samba-users.bak

# Importar base de datos
sudo pdbedit -i smbpasswd:/tmp/samba-users.bak
```

### Base de datos de contraseñas

```bash
# Ubicación de la base de datos TDB
ls -l /var/lib/samba/private/

# passdb.tdb — contraseñas Samba (TDB format)
# secrets.tdb — secretos del dominio/machine account
# No son editables manualmente — usar smbpasswd/pdbedit
```

### Usuarios sin login Unix

Para usuarios que solo necesitan acceso a shares Samba (no SSH ni login local):

```bash
# Crear usuario sin home ni shell
sudo useradd -M -s /usr/sbin/nologin -G samba_users consultor

# Agregar a Samba
sudo smbpasswd -a consultor

# El usuario puede acceder a shares SMB pero no puede:
# - Hacer login por SSH
# - Iniciar sesión local
# - Tener home directory
```

---

## 8. Shares comunes preconfigurados

### [homes] — directorios home automáticos

```ini
[homes]
    comment = Home Directories
    browseable = no           # No listar "homes" como share
    writable = yes
    valid users = %S          # %S = nombre del share (= usuario)
    create mask = 0700
    directory mask = 0700
```

Cuando `[homes]` está definido, cada usuario que se conecta ve automáticamente un share con su nombre, mapeado a su directorio home Unix:

```
Usuario "juan" se conecta al servidor:
  → Ve share [juan] → mapeado a /home/juan

Usuario "maria" se conecta:
  → Ve share [maria] → mapeado a /home/maria
```

`browseable = no` oculta el share genérico `homes` del listado — pero cada usuario ve su share personalizado.

### [printers] — impresoras CUPS

```ini
[printers]
    comment = All Printers
    path = /var/spool/samba
    browseable = no
    guest ok = no
    printable = yes            # Esto lo identifica como share de impresora
    create mask = 0700
```

Automáticamente comparte todas las impresoras configuradas en CUPS.

### Variables de sustitución

smb.conf soporta variables que se expanden dinámicamente:

| Variable | Valor |
|----------|-------|
| `%U` | Usuario que se conecta |
| `%S` | Nombre del share actual |
| `%m` | Nombre NetBIOS del cliente |
| `%H` | Home directory del usuario |
| `%L` | Nombre NetBIOS del servidor |
| `%I` | IP del cliente |
| `%M` | DNS hostname del cliente |
| `%v` | Versión de Samba |
| `%D` | Nombre del dominio |

```ini
# Ejemplo: log por máquina cliente
log file = /var/log/samba/log.%m

# Ejemplo: path dinámico por usuario
[datos_personales]
    path = /srv/samba/users/%U
    valid users = %U
    writable = yes
```

---

## 9. Validación y diagnóstico

### testparm — validar smb.conf

**Siempre ejecutar `testparm` después de editar smb.conf.** Detecta errores de sintaxis y muestra la configuración efectiva.

```bash
# Validar configuración
testparm
```

Salida exitosa:

```
Load smb config files from /etc/samba/smb.conf
Loaded services file OK.
Weak crypto is allowed by GnuTLS (e.g. NTLM as a compatibility fallback)

Server role: ROLE_STANDALONE

Press enter to see a dump of your service definitions
```

Si hay errores:

```
Unknown parameter encountered: "wrietable"
Ignoring unknown parameter "wrietable"
```

```bash
# Validar sin pausa interactiva
testparm -s

# Validar y mostrar solo un share específico
testparm -s --section-name=datos

# Ver la configuración efectiva (con todos los defaults)
testparm -v -s
```

### smbstatus — conexiones activas

```bash
# Ver quién está conectado
sudo smbstatus
```

```
Samba version 4.19.5-Debian
PID     Username    Group       Machine           Protocol Version
----------------------------------------------------------------------
12345   juan        staff       192.168.1.100     SMB3_11

Service     pid     Machine       Connected at
----------------------------------------------------------------------
datos       12345   192.168.1.100 Sat Mar 21 10:15:00 2026

Locked files:
Pid     Uid     DenyMode  Access   R/W   Oplock  SharePath   Name
----------------------------------------------------------------------
12345   1000    DENY_NONE 0x120089 RDONLY NONE    /srv/samba  report.txt
```

```bash
# Solo shares conectados
sudo smbstatus -S

# Solo archivos bloqueados
sudo smbstatus -L

# Solo procesos
sudo smbstatus -p
```

### Probar acceso desde el servidor

```bash
# Listar shares disponibles (como usuario juan)
smbclient -L localhost -U juan

# Conectarse a un share
smbclient //localhost/datos -U juan
# smb: \> ls
# smb: \> put testfile.txt
# smb: \> quit
```

### Logs

```bash
# Logs generales de Samba
sudo journalctl -u smbd -f
sudo journalctl -u nmbd -f

# Logs por máquina (si configurado log file = /var/log/samba/log.%m)
ls /var/log/samba/
# log.192.168.1.100  log.DESKTOP-ABC  log.smbd  log.nmbd

# Subir nivel de log temporalmente (sin reiniciar)
sudo smbcontrol smbd debug 3

# Restaurar nivel normal
sudo smbcontrol smbd debug 1
```

### Recargar configuración sin reiniciar

```bash
# Recargar smb.conf sin desconectar clientes
sudo smbcontrol smbd reload-config

# Equivalente con systemctl
sudo systemctl reload smbd    # Debian
sudo systemctl reload smb     # RHEL
```

Al recargar, las conexiones activas mantienen la configuración anterior. Los nuevos clientes usan la configuración nueva. Para forzar la nueva configuración en todos, reiniciar el servicio (desconecta a todos).

---

## 10. Errores comunes

### Error 1 — Usuario Samba sin usuario Unix

```bash
# smbpasswd -a consultor
# Failed to add entry for user consultor.

# Causa: el usuario no existe en /etc/passwd
# smbpasswd requiere un usuario Unix previo

# Solución:
sudo useradd -M -s /usr/sbin/nologin consultor
sudo smbpasswd -a consultor
```

### Error 2 — Permisos Unix bloquean escritura

```ini
# smb.conf
[datos]
    path = /srv/samba/datos
    writable = yes
    valid users = @staff
```

```bash
# El usuario puede conectar pero no escribir
# "NT_STATUS_ACCESS_DENIED"

# Verificar permisos Unix:
ls -la /srv/samba/datos
# drwxr-xr-x root root /srv/samba/datos
#                       ↑ solo root puede escribir

# Solución: ajustar permisos Unix
sudo chgrp staff /srv/samba/datos
sudo chmod 2775 /srv/samba/datos
```

Samba aplica la capa **más restrictiva**. Si smb.conf dice `writable = yes` pero Unix dice `r-x`, gana Unix.

### Error 3 — writable y read only simultáneos

```ini
# CONFUSO — ¿qué gana?
[datos]
    writable = yes
    read only = yes
```

`writable` y `read only` son **opuestos** del mismo parámetro. El último en aparecer gana. Para evitar confusión, usar solo uno de los dos:

```ini
# CLARO
[datos]
    read only = no    # Equivale a writable = yes
```

### Error 4 — Olvidar testparm

```ini
# Typo en smb.conf:
[datos]
    wrietable = yes    # ← typo
    vaild users = juan # ← typo
```

```bash
# Samba ignora silenciosamente parámetros desconocidos
# El share funciona pero con defaults (read only = yes)
# testparm lo habría detectado:
testparm -s
# Unknown parameter encountered: "wrietable"
# Unknown parameter encountered: "vaild users"
```

### Error 5 — SELinux bloquea acceso al directorio

```bash
# En RHEL/Fedora con SELinux enforcing:
# El share está bien configurado, permisos Unix correctos,
# pero los clientes reciben NT_STATUS_ACCESS_DENIED

# Verificar:
ls -Z /srv/samba/datos
# unconfined_u:object_r:var_t:s0  ← contexto incorrecto

# Solución: aplicar contexto Samba
sudo semanage fcontext -a -t samba_share_t '/srv/samba/datos(/.*)?'
sudo restorecon -Rv /srv/samba/datos

# Verificar:
ls -Z /srv/samba/datos
# unconfined_u:object_r:samba_share_t:s0  ← correcto

# Si además necesitas que Samba sirva home directories:
sudo setsebool -P samba_enable_home_dirs on
```

---

## 11. Cheatsheet

```bash
# ── Instalación ──────────────────────────────────────────
apt install samba                    # Debian/Ubuntu
dnf install samba                    # RHEL/Fedora
systemctl enable --now smbd nmbd     # Debian
systemctl enable --now smb nmb       # RHEL

# ── smb.conf — estructura ────────────────────────────────
# /etc/samba/smb.conf
# [global]    → config del servidor
# [homes]     → home directories automáticos
# [printers]  → impresoras CUPS
# [nombre]    → share personalizado

# ── Share mínimo ──────────────────────────────────────────
# [datos]
#     path = /srv/samba/datos
#     valid users = @staff
#     writable = yes
#     create mask = 0664
#     directory mask = 0775

# ── Usuarios ──────────────────────────────────────────────
useradd -M -s /usr/sbin/nologin USER  # Crear usuario Unix
smbpasswd -a USER                     # Agregar a Samba
smbpasswd USER                        # Cambiar contraseña
smbpasswd -d USER                     # Deshabilitar
smbpasswd -e USER                     # Habilitar
smbpasswd -x USER                     # Eliminar de Samba
pdbedit -L                            # Listar usuarios
pdbedit -L -v                         # Listar con detalles

# ── Validación ────────────────────────────────────────────
testparm                              # Validar smb.conf (SIEMPRE)
testparm -s                           # Sin pausa interactiva
testparm -v -s                        # Con todos los defaults

# ── Diagnóstico ──────────────────────────────────────────
smbstatus                             # Conexiones activas
smbstatus -S                          # Shares conectados
smbstatus -L                          # Archivos bloqueados
smbclient -L localhost -U USER        # Listar shares
smbcontrol smbd reload-config         # Recargar sin desconectar

# ── Logs ──────────────────────────────────────────────────
journalctl -u smbd -f                 # Logs del servicio
smbcontrol smbd debug 3               # Subir nivel de log
smbcontrol smbd debug 1               # Restaurar nivel

# ── Firewall ─────────────────────────────────────────────
firewall-cmd --permanent --add-service=samba
firewall-cmd --reload

# ── SELinux (RHEL) ────────────────────────────────────────
semanage fcontext -a -t samba_share_t '/srv/samba/datos(/.*)?'
restorecon -Rv /srv/samba/datos
setsebool -P samba_enable_home_dirs on
```

---

## 12. Ejercicios

### Ejercicio 1 — Configurar un share departamental

Crea un share llamado `contabilidad` accesible solo por los miembros del grupo `contab`, con escritura habilitada.

```bash
# 1. Crear grupo y usuarios
sudo groupadd contab
sudo useradd -M -s /usr/sbin/nologin -G contab ana
sudo useradd -M -s /usr/sbin/nologin -G contab luis
sudo smbpasswd -a ana
sudo smbpasswd -a luis

# 2. Crear directorio
sudo mkdir -p /srv/samba/contabilidad
sudo chgrp contab /srv/samba/contabilidad
sudo chmod 2770 /srv/samba/contabilidad

# 3. Agregar a smb.conf
cat <<'EOF' | sudo tee -a /etc/samba/smb.conf

[contabilidad]
    comment = Departamento de Contabilidad
    path = /srv/samba/contabilidad
    valid users = @contab
    writable = yes
    create mask = 0660
    directory mask = 0770
    force group = contab
EOF

# 4. Validar y recargar
testparm -s
sudo smbcontrol smbd reload-config

# 5. Probar
smbclient //localhost/contabilidad -U ana
```

**Pregunta de predicción:** Si un usuario del grupo `contab` crea un archivo sin la directiva `force group = contab`, ¿qué grupo tendrá el archivo? ¿Y con `force group`?

> **Respuesta:** Sin `force group`, el archivo tendría el grupo primario del usuario (por ejemplo, `ana`), no `contab`, lo que impediría que otros miembros del grupo lo modifiquen. Con `force group = contab`, Samba fuerza el grupo a `contab` en todos los archivos creados, independientemente del grupo primario del usuario. El bit SGID (`chmod 2770`) logra lo mismo a nivel Unix, pero `force group` lo garantiza a nivel Samba.

---

### Ejercicio 2 — Share con acceso diferenciado

Crea un share `informes` donde todos los empleados pueden leer, pero solo los gerentes pueden escribir.

```ini
# En smb.conf:
[informes]
    comment = Informes corporativos
    path = /srv/samba/informes
    valid users = @empleados, @gerentes
    read only = yes
    write list = @gerentes
    create mask = 0664
    directory mask = 0775
    force group = empleados
```

```bash
# Preparar directorio
sudo mkdir -p /srv/samba/informes
sudo chgrp empleados /srv/samba/informes
sudo chmod 2775 /srv/samba/informes

# Validar
testparm -s
```

**Pregunta de predicción:** Si un usuario pertenece tanto al grupo `empleados` como a `gerentes`, ¿puede escribir? ¿Qué parámetro tiene prioridad: `read only = yes` o `write list = @gerentes`?

> **Respuesta:** Sí, puede escribir. `write list` es una **excepción** explícita a `read only`. Si el usuario aparece en `write list` (directamente o por grupo), obtiene acceso de escritura aunque el share sea `read only = yes`. Es el mecanismo diseñado para este patrón exacto: lectura para muchos, escritura para pocos.

---

### Ejercicio 3 — Diagnóstico de un share inaccesible

Un usuario reporta que no puede acceder al share `proyectos`. Investiga paso a paso.

```bash
# 1. ¿La configuración es válida?
testparm -s --section-name=proyectos

# 2. ¿El servicio está corriendo?
sudo systemctl status smbd

# 3. ¿El usuario existe en Samba?
sudo pdbedit -L | grep juan

# 4. ¿Puede autenticarse?
smbclient //localhost/proyectos -U juan

# 5. ¿Los permisos Unix del directorio son correctos?
ls -la /srv/samba/proyectos/

# 6. ¿SELinux bloquea? (RHEL)
sudo ausearch -m avc -ts recent | grep samba
ls -Z /srv/samba/proyectos/

# 7. ¿El firewall permite SMB?
sudo firewall-cmd --list-services | grep samba

# 8. ¿Hay conexiones activas?
sudo smbstatus

# 9. ¿Qué dicen los logs?
sudo smbcontrol smbd debug 3
smbclient //localhost/proyectos -U juan    # Reproducir error
sudo journalctl -u smbd --since "1 minute ago"
sudo smbcontrol smbd debug 1              # Restaurar
```

**Pregunta de predicción:** `testparm` no muestra errores, el usuario existe en `pdbedit`, los permisos Unix son correctos, pero `smbclient` da `NT_STATUS_ACCESS_DENIED`. El servidor es RHEL con SELinux enforcing. ¿Cuál es la causa más probable?

> **Respuesta:** El contexto SELinux del directorio no es `samba_share_t`. SELinux bloquea el acceso aunque todos los demás permisos sean correctos. Ejecutar `ls -Z /srv/samba/proyectos/` confirmará un contexto como `var_t` o `default_t` en lugar de `samba_share_t`. La solución es `semanage fcontext -a -t samba_share_t '/srv/samba/proyectos(/.*)?'` seguido de `restorecon -Rv`.

---

> **Siguiente:** T02 — Cliente SMB/CIFS (smbclient, montaje con mount.cifs).
