# Integración con usuarios Linux

## Índice
1. [El problema de la doble identidad](#1-el-problema-de-la-doble-identidad)
2. [Backends de autenticación (passdb)](#2-backends-de-autenticación-passdb)
3. [smbpasswd en profundidad](#3-smbpasswd-en-profundidad)
4. [tdbsam — backend por defecto](#4-tdbsam--backend-por-defecto)
5. [Mapping de permisos Unix ↔ SMB](#5-mapping-de-permisos-unix--smb)
6. [VFS objects — extensiones de filesystem](#6-vfs-objects--extensiones-de-filesystem)
7. [Integración con dominio AD](#7-integración-con-dominio-ad)
8. [Gestión centralizada de usuarios](#8-gestión-centralizada-de-usuarios)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. El problema de la doble identidad

Samba vive entre dos mundos: **Unix** (UIDs, GIDs, permisos rwx) y **Windows** (SIDs, ACLs NTFS, cuentas SAM). El reto principal es que un usuario debe existir en ambos sistemas para acceder a shares.

```
┌──────────────────────────────────────────────────────────┐
│          Doble identidad de un usuario Samba              │
│                                                          │
│  Mundo Unix                    Mundo Windows/SMB         │
│  ──────────                    ──────────────────         │
│  /etc/passwd                   passdb (tdbsam)           │
│  UID: 1000                     SID: S-1-5-21-...-1000    │
│  GID: 1000                     RID: 1000                 │
│  Shell: /bin/bash              Flags: [U    ]            │
│  Password: /etc/shadow         Password: NT hash (tdb)   │
│                                                          │
│        ↕ Mapping ↕                                       │
│                                                          │
│  Cuando "juan" se conecta por SMB:                       │
│  1. Samba autentica con su propia base (passdb)          │
│  2. Samba mapea a UID Unix para acceso al filesystem     │
│  3. El kernel aplica permisos Unix con ese UID           │
│                                                          │
│  Las contraseñas SON INDEPENDIENTES:                     │
│  - passwd/shadow → login, SSH, su                        │
│  - passdb → SMB/CIFS                                     │
└──────────────────────────────────────────────────────────┘
```

### ¿Por qué contraseñas separadas?

SMB usa un esquema de autenticación (NTLM) incompatible con los hashes Unix de `/etc/shadow`. No es posible compartir la misma base de datos de contraseñas. Las soluciones son:

1. **Gestión manual** — mantener ambas contraseñas sincronizadas (tedioso)
2. **PAM integration** — `pam_smbpass` sincroniza al cambiar contraseña con `passwd` (obsoleto)
3. **Active Directory** — un único directorio para ambos mundos (solución real)
4. **LDAP** — backend compartido para Unix y Samba (alternativa open source)

---

## 2. Backends de autenticación (passdb)

Samba almacena las credenciales SMB en un **passdb backend**. La directiva `passdb backend` en `[global]` define cuál usar.

```ini
[global]
    passdb backend = tdbsam    # Default
```

### Backends disponibles

```
┌──────────────────────────────────────────────────────────┐
│              Backends de passdb                           │
│                                                          │
│  ┌─────────────────────────────────────────────┐         │
│  │  smbpasswd                                  │         │
│  │  Archivo plano: /etc/samba/smbpasswd         │         │
│  │  Formato legacy, funcionalidad limitada      │         │
│  │  Sin atributos extendidos de cuenta          │         │
│  │  ✗ Deprecated — no usar en nuevas instancias │         │
│  └─────────────────────────────────────────────┘         │
│                                                          │
│  ┌─────────────────────────────────────────────┐         │
│  │  tdbsam ★ DEFAULT                           │         │
│  │  Base de datos TDB: /var/lib/samba/private/  │         │
│  │  passdb.tdb                                  │         │
│  │  Soporta atributos extendidos               │         │
│  │  Adecuado para servidores standalone         │         │
│  │  ✓ Recomendado para uso sin dominio          │         │
│  └─────────────────────────────────────────────┘         │
│                                                          │
│  ┌─────────────────────────────────────────────┐         │
│  │  ldapsam                                    │         │
│  │  Almacena cuentas en LDAP (OpenLDAP/389DS)  │         │
│  │  Centralizado, replicable                   │         │
│  │  Requiere schema Samba en LDAP              │         │
│  │  ✓ Para múltiples servidores Samba sin AD   │         │
│  └─────────────────────────────────────────────┘         │
│                                                          │
│  ┌─────────────────────────────────────────────┐         │
│  │  Active Directory (Samba como DC o member)   │         │
│  │  No usa passdb — autenticación contra AD    │         │
│  │  Solución definitiva para entornos mixtos   │         │
│  │  ✓ Para entornos con Windows y Linux        │         │
│  └─────────────────────────────────────────────┘         │
└──────────────────────────────────────────────────────────┘
```

### Migrar entre backends

```bash
# De smbpasswd a tdbsam
sudo pdbedit -i smbpasswd:/etc/samba/smbpasswd -e tdbsam

# Verificar migración
sudo pdbedit -L

# Cambiar backend en smb.conf
# passdb backend = tdbsam
sudo smbcontrol smbd reload-config
```

---

## 3. smbpasswd en profundidad

`smbpasswd` es tanto un **archivo** (backend legacy) como un **comando** (herramienta de gestión).

### El comando smbpasswd

```bash
# Agregar usuario a la base Samba (requiere usuario Unix previo)
sudo smbpasswd -a juan

# Cambiar contraseña Samba de un usuario
sudo smbpasswd juan

# Un usuario cambia su propia contraseña (sin sudo)
smbpasswd

# Deshabilitar cuenta (no puede autenticarse)
sudo smbpasswd -d juan

# Habilitar cuenta deshabilitada
sudo smbpasswd -e juan

# Eliminar cuenta de Samba (no borra el usuario Unix)
sudo smbpasswd -x juan

# Establecer contraseña nula (acceso sin password — peligroso)
sudo smbpasswd -n juan
```

### El archivo smbpasswd (legacy)

```bash
# /etc/samba/smbpasswd — formato del backend legacy
# username:UID:LM_HASH:NT_HASH:FLAGS:LAST_CHANGE
juan:1000:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX:A1B2C3D4E5F6...:U:::
```

| Campo | Significado |
|-------|------------|
| username | Nombre de usuario (debe existir en /etc/passwd) |
| UID | UID Unix del usuario |
| LM_HASH | LAN Manager hash (obsoleto, rellenado con X) |
| NT_HASH | NTLM hash de la contraseña |
| FLAGS | `[U ]` = user activo, `[DU ]` = disabled |

> **LM hash** se deshabilita por defecto en Samba moderno (`lanman auth = no`). Es extremadamente débil criptográficamente y no debe usarse.

### Relación smbpasswd ↔ usuario Unix

```bash
# Flujo completo para crear un usuario Samba
# Paso 1: crear usuario Unix
sudo useradd -m -s /bin/bash juan           # Con home y shell
# o
sudo useradd -M -s /usr/sbin/nologin juan   # Sin home ni shell

# Paso 2: crear cuenta Samba
sudo smbpasswd -a juan
# Esto falla si "juan" no existe en /etc/passwd

# Paso 3: (opcional) agregar a grupo para valid users
sudo usermod -aG staff juan
```

```
┌──────────────────────────────────────────────────────────┐
│           Tipos de usuario según necesidad                │
│                                                          │
│  Solo Samba (share access):                              │
│    useradd -M -s /usr/sbin/nologin usuario               │
│    smbpasswd -a usuario                                  │
│    → Sin home, sin shell, sin login SSH                   │
│    → Solo accede a shares SMB                            │
│                                                          │
│  Samba + login Unix:                                     │
│    useradd -m -s /bin/bash usuario                       │
│    passwd usuario                                        │
│    smbpasswd -a usuario                                  │
│    → Tiene home, puede hacer SSH, accede a shares        │
│    → DOS contraseñas independientes                      │
│                                                          │
│  Solo Unix (sin Samba):                                  │
│    useradd -m -s /bin/bash usuario                       │
│    passwd usuario                                        │
│    → No está en passdb, no puede acceder vía SMB         │
└──────────────────────────────────────────────────────────┘
```

---

## 4. tdbsam — backend por defecto

tdbsam usa la base de datos TDB (Trivial Database), un formato binario propio de Samba que almacena más información que el archivo smbpasswd plano.

### Atributos adicionales en tdbsam

```bash
# Ver todos los atributos de un usuario
sudo pdbedit -L -v -u juan
```

```
Unix username:        juan
NT username:          juan
Account Flags:        [U          ]
User SID:             S-1-5-21-1234567890-1234567890-1234567890-1000
Primary Group SID:    S-1-5-21-1234567890-1234567890-1234567890-513
Full Name:            Juan García
Home Directory:       \\FILESERVER\juan
HomeDir Drive:        H:
Logon Script:
Profile Path:         \\FILESERVER\profiles\juan
Account desc:
Workstations:
Logon time:           0
Logoff time:          never
Kickoff time:         never
Password last set:    Sat, 21 Mar 2026 10:00:00 UTC
Password can change:  Sat, 21 Mar 2026 10:00:00 UTC
Password must change: never
Last bad password:    0
Bad password count:   0
Logon hours:          FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
```

### Gestión con pdbedit

```bash
# Listar usuarios
sudo pdbedit -L

# Listar con detalles
sudo pdbedit -L -v

# Agregar usuario (alternativa a smbpasswd -a)
sudo pdbedit -a -u juan

# Modificar atributos
sudo pdbedit -u juan --fullname="Juan García"
sudo pdbedit -u juan --homedir="\\\\FILESERVER\\juan"
sudo pdbedit -u juan --drive="H:"
sudo pdbedit -u juan --profile="\\\\FILESERVER\\profiles\\juan"
sudo pdbedit -u juan --account-desc="Departamento de ingeniería"

# Establecer políticas de contraseña
sudo pdbedit -u juan -P "password must change" -C 0
# Fuerza cambio de contraseña en próximo login

# Exportar/importar (backup)
sudo pdbedit -e tdbsam:/tmp/passdb-backup.tdb
sudo pdbedit -i tdbsam:/tmp/passdb-backup.tdb

# Exportar a formato texto (debug)
sudo pdbedit -e smbpasswd:/tmp/samba-export.txt
```

### SIDs — identificadores de seguridad

Windows identifica usuarios y grupos por **SID** (Security Identifier), no por UID. Samba genera SIDs automáticamente.

```bash
# Ver el SID del servidor
sudo net getlocalsid
# SID for domain FILESERVER is: S-1-5-21-1234567890-1234567890-1234567890

# Convertir UID a SID
sudo net lookup name juan
# S-1-5-21-1234567890-...-1000

# Convertir SID a UID
sudo wbinfo -S S-1-5-21-1234567890-...-1000
# 1000
```

El mapeo UID ↔ SID es determinístico: `RID = UID * 2 + 1000` (por defecto). Si dos servidores Samba tienen el mismo SID local, los permisos son portables entre ellos.

---

## 5. Mapping de permisos Unix ↔ SMB

### El problema fundamental

Unix tiene permisos `rwx` para user/group/other (9 bits + SUID/SGID/sticky). Windows tiene ACLs NTFS con permisos granulares (Read, Write, Execute, Delete, Change Permissions, Take Ownership, etc.) para múltiples usuarios/grupos.

```
┌──────────────────────────────────────────────────────────┐
│           Unix rwx vs ACL Windows/NTFS                    │
│                                                          │
│  Unix:                                                   │
│    -rwxr-x--- juan staff file.txt                        │
│    3 entidades: user, group, other                       │
│    3 permisos: read, write, execute                      │
│                                                          │
│  Windows NTFS:                                           │
│    juan: Full Control                                    │
│    staff: Read & Execute                                 │
│    contab: Modify                                        │
│    becarios: Read Only                                   │
│    maria: Deny Write                                     │
│    N entidades, permisos granulares, allow + deny         │
│                                                          │
│  Samba debe traducir entre ambos mundos                  │
└──────────────────────────────────────────────────────────┘
```

### Parámetros de mapping en smb.conf

```ini
[share]
    # ── Permisos de archivos nuevos ──────────────────
    create mask = 0664           # Máscara AND (apaga bits)
    force create mode = 0000     # Máscara OR (enciende bits)

    # ── Permisos de directorios nuevos ───────────────
    directory mask = 0775        # Máscara AND
    force directory mode = 0000  # Máscara OR

    # ── Propietario forzado ──────────────────────────
    force user = nobody          # Todo acceso como este UID
    force group = staff          # Todo acceso como este GID

    # ── Herencia de permisos ─────────────────────────
    inherit permissions = no     # Heredar permisos del directorio padre
    inherit owner = no           # Heredar propietario del directorio padre
```

### Flujo de creación de un archivo

```
Cliente Windows crea "report.docx" en \\SERVER\datos
                    │
                    ▼
1. Samba autentica al usuario → "juan" (UID 1000)
                    │
                    ▼
2. El archivo se crea con permisos iniciales
   (depende de la aplicación, típicamente 0666)
                    │
                    ▼
3. create mask (AND) → 0666 AND 0664 = 0664
                    │
                    ▼
4. force create mode (OR) → 0664 OR 0000 = 0664
                    │
                    ▼
5. Propietario:
   - Sin force user/group → juan:juan (UID/GID del usuario)
   - Con force group = staff → juan:staff
   - Con inherit owner = yes → mismo dueño que el directorio
                    │
                    ▼
Resultado: -rw-rw-r-- juan staff report.docx
```

### inherit permissions

```ini
[proyectos]
    path = /srv/samba/proyectos
    inherit permissions = yes
```

Con `inherit permissions = yes`, los archivos y directorios nuevos heredan los permisos del directorio padre en lugar de usar `create mask`/`directory mask`:

```bash
# Si el directorio padre tiene permisos 2770:
ls -ld /srv/samba/proyectos/
# drwxrws--- root staff /srv/samba/proyectos/

# Un archivo nuevo heredará 0660 (el padre sin ejecución)
# Un directorio nuevo heredará 2770 (copia exacta del padre)
```

Esto es útil cuando diferentes subdirectorios dentro de un share necesitan permisos distintos.

### Mapping de atributos DOS

Windows tiene atributos de archivo (Hidden, System, Archive, Read-only) que Unix no tiene nativamente. Samba puede mapearlos a bits de permiso Unix:

```ini
[global]
    # Mapear atributos DOS a bits Unix
    map hidden = no         # Hidden → bit execute de other
    map system = no         # System → bit execute de group
    map archive = yes       # Archive → bit execute de user
    map readonly = yes      # Read-only → invertir bit write de user

    # O usar xattrs (mejor — no roba bits de permiso)
    store dos attributes = yes   # Usar extended attributes
    map archive = no
    map hidden = no
    map system = no
```

```
┌──────────────────────────────────────────────────────────┐
│        Mapping de atributos DOS a bits Unix               │
│                                                          │
│  Con map hidden/system/archive:                          │
│                                                          │
│  Permisos:  r w x   r w x   r w x                       │
│             user    group   other                        │
│                 ↑       ↑       ↑                        │
│             Archive  System  Hidden                      │
│                                                          │
│  Problema: "ocultar" un archivo lo hace ejecutable       │
│  para other, lo cual es confuso                          │
│                                                          │
│  Con store dos attributes = yes:                         │
│  Los atributos se guardan en xattrs del filesystem       │
│  → user.DOSATTRIB                                        │
│  → No interfiere con permisos Unix ✓                     │
│  Requiere que el filesystem soporte xattrs (ext4, xfs)   │
└──────────────────────────────────────────────────────────┘
```

### ACLs POSIX para granularidad

Cuando `rwx` user/group/other no es suficiente, las ACLs POSIX permiten asignar permisos a múltiples usuarios/grupos:

```bash
# Dar acceso rw a un usuario adicional
setfacl -m u:maria:rwx /srv/samba/proyectos/
setfacl -m g:contab:rx /srv/samba/proyectos/

# Default ACL (herencia para archivos nuevos)
setfacl -d -m g:staff:rwx /srv/samba/proyectos/

# Ver ACLs
getfacl /srv/samba/proyectos/
```

Samba traduce automáticamente entre ACLs Windows y ACLs POSIX si el filesystem las soporta. Desde Windows, el usuario ve la pestaña "Security" con permisos familiares.

```ini
[proyectos]
    path = /srv/samba/proyectos
    # Habilitar edición de ACLs desde Windows
    nt acl support = yes           # Default: yes
    inherit acls = yes             # Heredar ACLs POSIX
```

---

## 6. VFS objects — extensiones de filesystem

Samba permite interceptar operaciones de filesystem con módulos VFS (Virtual File System). Son como plugins que modifican el comportamiento del share.

### Módulos más útiles

```ini
[share]
    vfs objects = acl_xattr recycle shadow_copy2
```

### recycle — papelera de reciclaje

```ini
[datos]
    path = /srv/samba/datos
    vfs objects = recycle

    # Configuración de la papelera
    recycle:repository = .recycle/%U    # Ruta de la papelera (por usuario)
    recycle:keeptree = yes              # Mantener estructura de directorios
    recycle:versions = yes              # Mantener versiones (si mismo nombre)
    recycle:touch = yes                 # Actualizar timestamp al reciclar
    recycle:maxsize = 104857600         # Máximo 100 MB por archivo
    recycle:exclude = *.tmp *.bak       # No reciclar estos
    recycle:exclude_dir = /tmp          # No reciclar de estos dirs
```

```
Archivo eliminado desde Windows:
  \\SERVER\datos\informes\q1.pdf
                    │
                    ▼
  No se borra realmente, se mueve a:
  /srv/samba/datos/.recycle/juan/informes/q1.pdf
                                 ↑
                          Preserva la estructura
```

### full_audit — auditoría de operaciones

```ini
[confidencial]
    path = /srv/samba/confidencial
    vfs objects = full_audit

    full_audit:prefix = %u|%I|%m|%S     # Usuario|IP|máquina|share
    full_audit:success = mkdir rmdir rename unlink write open
    full_audit:failure = none
    full_audit:facility = local5
    full_audit:priority = notice
```

```bash
# Configurar rsyslog para capturar la auditoría
echo 'local5.* /var/log/samba/audit.log' | sudo tee /etc/rsyslog.d/samba-audit.conf
sudo systemctl restart rsyslog
```

Registro de ejemplo:

```
Mar 21 10:30:15 server smbd_audit: juan|192.168.1.100|DESKTOP-ABC|confidencial|open|ok|informe.pdf
Mar 21 10:31:22 server smbd_audit: juan|192.168.1.100|DESKTOP-ABC|confidencial|unlink|ok|borrador.txt
```

### shadow_copy2 — versiones anteriores (snapshots)

Permite que Windows muestre "Versiones anteriores" de archivos a partir de snapshots del filesystem (ZFS, Btrfs, LVM):

```ini
[datos]
    path = /srv/samba/datos
    vfs objects = shadow_copy2

    shadow:snapdir = .snapshots            # Directorio con snapshots
    shadow:sort = desc                     # Más reciente primero
    shadow:format = @GMT-%Y.%m.%d-%H.%M.%S  # Formato de nombre
    shadow:localtime = no                  # Timestamps en UTC
```

---

## 7. Integración con dominio AD

Cuando Linux se une a un dominio Active Directory, los usuarios del dominio pueden acceder a shares Samba sin necesidad de cuentas locales ni `smbpasswd`.

### Flujo de autenticación en dominio

```
┌──────────────────────────────────────────────────────────┐
│        Samba como miembro de dominio AD                    │
│                                                          │
│  Windows Client                                          │
│       │                                                  │
│       │ Kerberos ticket                                  │
│       ▼                                                  │
│  Linux + Samba (domain member)                           │
│       │                                                  │
│       │ Verifica ticket con AD                           │
│       ▼                                                  │
│  Active Directory (DC)                                   │
│       │                                                  │
│       │ "juan@CORP.COM es válido, SID=..."              │
│       ▼                                                  │
│  winbindd mapea SID → UID Unix                          │
│       │                                                  │
│       │ UID 10000 + permisos Unix                        │
│       ▼                                                  │
│  Acceso al share con UID mapeado                        │
└──────────────────────────────────────────────────────────┘
```

### Configuración básica como domain member

```ini
# /etc/samba/smb.conf
[global]
    workgroup = CORP
    realm = CORP.COM
    security = ads                    # Autenticación contra AD

    # Winbind — mapeo de usuarios AD a UIDs Unix
    idmap config * : backend = tdb
    idmap config * : range = 3000-7999

    idmap config CORP : backend = rid
    idmap config CORP : range = 10000-999999

    winbind use default domain = yes  # No requerir CORP\usuario
    winbind enum users = yes
    winbind enum groups = yes

    # Template para usuarios del dominio
    template homedir = /home/%U
    template shell = /bin/bash
```

### Unir al dominio

```bash
# 1. Configurar DNS para apuntar al DC
# /etc/resolv.conf o systemd-resolved → nameserver = IP del DC

# 2. Configurar Kerberos
# /etc/krb5.conf
# [realms]
#   CORP.COM = { kdc = dc1.corp.com }

# 3. Obtener ticket Kerberos del administrador
kinit administrator@CORP.COM

# 4. Unir al dominio
sudo net ads join -k
# o
sudo net ads join -U administrator

# 5. Iniciar winbindd
sudo systemctl enable --now winbindd

# 6. Verificar
wbinfo -u    # Listar usuarios del dominio
wbinfo -g    # Listar grupos del dominio
id juan      # Verificar mapping UID de un usuario AD
```

### idmap backends

El **idmap** mapea SIDs de Windows a UIDs/GIDs Unix. El backend determina cómo se calcula este mapeo:

| Backend | Mecanismo | Persistencia | Uso |
|---------|-----------|-------------|-----|
| `tdb` | Base TDB local | Sí, pero no replicable | Default para `*` |
| `rid` | Algorítmico: UID = RID + offset | Determinístico | Recomendado para un solo dominio |
| `ad` | Lee UID/GID de atributos AD | Requiere schema Unix en AD | AD con extensiones Unix |
| `autorid` | Algorítmico con rangos auto | Determinístico | Múltiples dominios trusted |

```ini
# rid: UID = base_range_start + RID
# Si el RID de "juan" en AD es 1105 y el rango es 10000-999999:
# UID = 10000 + 1105 = 11105
# Ventaja: todos los servidores miembros calculan el mismo UID
```

---

## 8. Gestión centralizada de usuarios

### Problema de escala

```
┌──────────────────────────────────────────────────────────┐
│        Gestión de usuarios: standalone vs centralizado    │
│                                                          │
│  Standalone (tdbsam):                                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐              │
│  │ Samba A   │  │ Samba B   │  │ Samba C   │             │
│  │ passdb    │  │ passdb    │  │ passdb    │             │
│  │ local     │  │ local     │  │ local     │             │
│  └──────────┘  └──────────┘  └──────────┘              │
│  3 bases, 3 contraseñas, 3 smbpasswd -a por usuario     │
│                                                          │
│  Centralizado (AD/LDAP):                                │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐              │
│  │ Samba A   │  │ Samba B   │  │ Samba C   │             │
│  │ member    │  │ member    │  │ member    │             │
│  └─────┬────┘  └─────┬────┘  └─────┬────┘              │
│        │             │             │                     │
│        └──────┬──────┘─────────────┘                     │
│               ▼                                          │
│        ┌──────────────┐                                 │
│        │  AD / LDAP    │                                │
│        │  Una base,    │                                │
│        │  una          │                                │
│        │  contraseña   │                                │
│        └──────────────┘                                 │
└──────────────────────────────────────────────────────────┘
```

### Sincronización de contraseñas (standalone)

Si no hay AD/LDAP, se puede sincronizar la contraseña Unix con la de Samba usando PAM:

```ini
# /etc/samba/smb.conf
[global]
    # Cuando el usuario cambia su password Unix, sincronizar a Samba
    unix password sync = yes
    passwd program = /usr/bin/passwd %u
    passwd chat = *Enter\snew\s*\spassword:* %n\n *Retype\snew\s*\spassword:* %n\n *password\supdated\ssuccessfully* .
    pam password change = yes
```

Con `unix password sync = yes`, cuando un usuario cambia su contraseña Samba (`smbpasswd`), Samba también actualiza la contraseña Unix. Esto evita tener dos contraseñas diferentes.

### Gestión por grupos

```ini
# Usar grupos Unix para controlar acceso a shares
[ingenieria]
    valid users = @ingenieria
    write list = @ing_leads

[rrhh]
    valid users = @rrhh
    write list = @rrhh_managers
```

```bash
# Gestión de grupos Unix
sudo groupadd ingenieria
sudo groupadd ing_leads
sudo usermod -aG ingenieria juan
sudo usermod -aG ing_leads juan    # Lead puede escribir

# El usuario solo necesita UNA cuenta smbpasswd
# El acceso se controla por pertenencia a grupos Unix
```

---

## 9. Errores comunes

### Error 1 — Usuario existe en Unix pero no en Samba

```bash
# El usuario puede hacer SSH pero no acceder al share
smbclient //localhost/datos -U juan
# session setup failed: NT_STATUS_LOGON_FAILURE

# Verificar si existe en la base Samba
sudo pdbedit -L | grep juan
# (vacío)

# El usuario está en /etc/passwd pero NO en passdb
# Solución:
sudo smbpasswd -a juan
```

### Error 2 — Contraseñas Unix y Samba desincronizadas

```bash
# El usuario cambió su contraseña Unix con passwd
# pero la contraseña Samba sigue siendo la anterior

# Opción 1: cambiar manualmente
sudo smbpasswd juan

# Opción 2: habilitar sincronización automática
# En [global]:
#   unix password sync = yes
#   pam password change = yes
```

### Error 3 — force user oculta la identidad real

```ini
[datos]
    force user = nobody
    force group = nogroup
```

```bash
# TODOS los archivos se crean como nobody:nogroup
# El log y smbstatus muestran el usuario real (juan)
# pero el filesystem solo ve nobody
# → Imposible saber quién creó cada archivo

# Solución: usar force group (no force user)
# y configurar permisos por grupo
[datos]
    force group = staff
    create mask = 0660
    directory mask = 0770
# Los archivos conservan el usuario real como propietario
```

### Error 4 — idmap ranges solapados

```ini
# MAL — los rangos se solapan
idmap config * : range = 3000-20000
idmap config CORP : range = 10000-50000
#                           ↑ 10000-20000 solapado

# Resultado: mapeos inconsistentes, archivos con dueño incorrecto

# BIEN — rangos separados
idmap config * : range = 3000-7999
idmap config CORP : range = 10000-999999
```

### Error 5 — Permisos demasiado abiertos por create mask incorrecto

```ini
# El admin quiere que todos puedan leer
[docs]
    create mask = 0666    # Archivos rwx para todos
    directory mask = 0777

# Problema: 0666 da escritura a "other" — cualquier
# usuario Unix del servidor puede modificar

# Mejor:
[docs]
    create mask = 0664     # rw para user/group, r para other
    directory mask = 0775  # rwx para user/group, rx para other
    force group = docs     # Escritura controlada por grupo
```

---

## 10. Cheatsheet

```bash
# ── Backends de passdb ────────────────────────────────────
# passdb backend = tdbsam          # Default (recomendado)
# passdb backend = smbpasswd       # Legacy
# passdb backend = ldapsam         # LDAP

# ── Gestión de usuarios ──────────────────────────────────
useradd -M -s /usr/sbin/nologin USER  # Crear usuario Unix
smbpasswd -a USER                     # Agregar a Samba
smbpasswd -d USER                     # Deshabilitar
smbpasswd -e USER                     # Habilitar
smbpasswd -x USER                     # Eliminar de Samba
pdbedit -L                            # Listar usuarios
pdbedit -L -v -u USER                 # Detalles de usuario
pdbedit -a -u USER                    # Agregar (alternativa)

# ── Mapping de permisos ──────────────────────────────────
# create mask = 0664               # AND mask para archivos
# force create mode = 0000         # OR mask para archivos
# directory mask = 0775            # AND mask para dirs
# force directory mode = 0000      # OR mask para dirs
# force user = nobody              # Forzar UID (evitar si posible)
# force group = staff              # Forzar GID (útil)
# inherit permissions = yes        # Heredar del padre

# ── Atributos DOS ────────────────────────────────────────
# store dos attributes = yes       # Usar xattrs (recomendado)
# map archive = no                 # No robar bits de permiso
# map hidden = no
# map system = no

# ── ACLs POSIX ────────────────────────────────────────────
setfacl -m u:USER:rwx /path        # ACL para usuario
setfacl -m g:GROUP:rx /path         # ACL para grupo
setfacl -d -m g:GROUP:rwx /path     # Default ACL (herencia)
getfacl /path                       # Ver ACLs
# nt acl support = yes              # Editar desde Windows

# ── VFS objects ───────────────────────────────────────────
# vfs objects = recycle             # Papelera
# vfs objects = full_audit          # Auditoría
# vfs objects = shadow_copy2        # Versiones anteriores

# ── Dominio AD ────────────────────────────────────────────
# security = ads                    # Modo AD
net ads join -U administrator       # Unir al dominio
wbinfo -u                           # Usuarios del dominio
wbinfo -g                           # Grupos del dominio
id user@DOMAIN                      # Verificar mapping UID
# idmap config CORP : backend = rid
# idmap config CORP : range = 10000-999999

# ── Sincronización de passwords ──────────────────────────
# unix password sync = yes          # Sync Samba → Unix
# pam password change = yes         # Via PAM
```

---

## 11. Ejercicios

### Ejercicio 1 — Crear usuarios Samba con diferentes niveles de acceso

Configura un share `proyectos` con tres niveles: lectura para `@empleados`, escritura para `@devs`, y administración total para `@leads`.

```bash
# 1. Crear grupos
sudo groupadd empleados
sudo groupadd devs
sudo groupadd leads

# 2. Crear usuarios (solo Samba, sin login)
for user in ana luis carlos elena; do
    sudo useradd -M -s /usr/sbin/nologin -G empleados "$user"
    echo -e "password\npassword" | sudo smbpasswd -a -s "$user"
done
sudo usermod -aG devs luis
sudo usermod -aG devs carlos
sudo usermod -aG leads carlos

# 3. Crear directorio
sudo mkdir -p /srv/samba/proyectos
sudo chgrp empleados /srv/samba/proyectos
sudo chmod 2775 /srv/samba/proyectos

# 4. ACLs POSIX para granularidad
sudo setfacl -m g:devs:rwx /srv/samba/proyectos
sudo setfacl -d -m g:devs:rwx /srv/samba/proyectos
sudo setfacl -d -m g:empleados:rx /srv/samba/proyectos
```

```ini
# 5. smb.conf
[proyectos]
    path = /srv/samba/proyectos
    valid users = @empleados, @devs, @leads
    read only = yes
    write list = @devs, @leads
    admin users = @leads
    create mask = 0664
    directory mask = 0775
    force group = empleados
    inherit acls = yes
```

```bash
# 6. Validar y recargar
testparm -s
sudo smbcontrol smbd reload-config
```

**Pregunta de predicción:** `carlos` pertenece a `empleados`, `devs` y `leads`. Tiene `admin users` por `@leads`. ¿Qué significa `admin users` en la práctica? ¿Puede hacer algo que un usuario de `write list` no pueda?

> **Respuesta:** `admin users` mapea al usuario como **root** (UID 0) para todas las operaciones del filesystem en ese share. Puede leer/escribir/borrar cualquier archivo independientemente de los permisos Unix, cambiar permisos, y cambiar propietarios. Es equivalente a tener acceso root al directorio. Un usuario en `write list` puede escribir pero sigue sujeto a permisos Unix — si un archivo pertenece a otro usuario con `600`, no podrá modificarlo. `admin users` debe usarse con extrema cautela.

---

### Ejercicio 2 — Configurar papelera de reciclaje

Habilita una papelera en el share `datos` para que los archivos eliminados se recuperen fácilmente.

```ini
# smb.conf
[datos]
    path = /srv/samba/datos
    writable = yes
    valid users = @staff
    vfs objects = recycle
    recycle:repository = .papelera/%U
    recycle:keeptree = yes
    recycle:versions = yes
    recycle:touch = yes
    recycle:maxsize = 52428800
    recycle:exclude = *.tmp ~$* *.bak
```

```bash
# Validar y aplicar
testparm -s
sudo smbcontrol smbd reload-config

# Probar desde smbclient
smbclient //localhost/datos -U juan -c "del informe.pdf"

# Verificar que el archivo está en la papelera
ls -la /srv/samba/datos/.papelera/juan/
```

**Pregunta de predicción:** Si un usuario elimina `informe.pdf` dos veces (crea uno nuevo y lo borra), ¿qué pasa con `recycle:versions = yes`?

> **Respuesta:** Con `versions = yes`, la segunda eliminación no sobreescribe el primer archivo reciclado. Samba crea una copia con sufijo numérico: `informe.pdf` (primera eliminación) y `Copy #1 of informe.pdf` (segunda). Ambas versiones se conservan en la papelera. Sin `versions = yes`, la segunda eliminación sobreescribiría la primera y sólo quedaría la última versión.

---

### Ejercicio 3 — Diagnosticar permisos incorrectos

Un usuario reporta que puede leer archivos en `\\SERVER\proyectos` pero no puede crear archivos nuevos. El share tiene `writable = yes` y el usuario está en `valid users`. Diagnostica el problema.

```bash
# 1. Verificar configuración Samba
testparm -s --section-name=proyectos
# Confirmar writable = yes

# 2. Verificar que el usuario existe en passdb
sudo pdbedit -L | grep juan

# 3. Verificar permisos Unix del directorio
ls -la /srv/samba/proyectos/
# drwxr-xr-x root root /srv/samba/proyectos/
#     ↑ solo root puede escribir — ENCONTRADO

# 4. Verificar ACLs
getfacl /srv/samba/proyectos/

# 5. Verificar SELinux (RHEL)
ls -Z /srv/samba/proyectos/

# 6. Solución: corregir permisos Unix
sudo chgrp staff /srv/samba/proyectos/
sudo chmod 2775 /srv/samba/proyectos/
# O si el usuario no está en "staff":
sudo usermod -aG staff juan
```

**Pregunta de predicción:** ¿Por qué `writable = yes` en smb.conf no es suficiente para que el usuario pueda escribir? ¿En qué orden evalúa Samba los permisos?

> **Respuesta:** Samba evalúa en dos capas secuenciales: primero la capa SMB (valid users, writable, write list) y luego la capa Unix (permisos del filesystem). Ambas deben permitir la operación. `writable = yes` sólo pasa la primera capa. Si los permisos Unix del directorio son `755` con propietario root, el usuario mapeado no tiene bit `w` en la capa Unix, y la operación se deniega con `NT_STATUS_ACCESS_DENIED`. La restricción más estricta siempre gana.

---

> **Fin de la Sección 2 — Samba**, y del **Capítulo 4 — Compartición de Archivos**. Siguiente capítulo: C05 — Otros Servicios (Sección 1: DHCP Server, T01 — ISC DHCP server).
