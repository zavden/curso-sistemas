# Integración con NSS y PAM

## Índice

1. [El flujo completo: de login a directorio](#1-el-flujo-completo-de-login-a-directorio)
2. [NSS — Name Service Switch](#2-nss--name-service-switch)
3. [nsswitch.conf con sssd](#3-nsswitchconf-con-sssd)
4. [PAM — integración con pam_sss](#4-pam--integración-con-pam_sss)
5. [Configuración PAM para sssd](#5-configuración-pam-para-sssd)
6. [Creación automática de home: mkhomedir](#6-creación-automática-de-home-mkhomedir)
7. [El flujo de autenticación paso a paso](#7-el-flujo-de-autenticación-paso-a-paso)
8. [authselect (RHEL) y pam-auth-update (Debian)](#8-authselect-rhel-y-pam-auth-update-debian)
9. [Diagnóstico de la integración](#9-diagnóstico-de-la-integración)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. El flujo completo: de login a directorio

Cuando un usuario remoto (definido en LDAP/AD) intenta hacer login en un sistema Linux, intervienen tres capas cooperando:

```
                           ┌─────────────┐
Usuario teclea             │   sshd /    │
alice + contraseña  ────►  │   login /   │
                           │   gdm       │
                           └──────┬──────┘
                                  │
                    ┌─────────────┼─────────────┐
                    │             │             │
                    ▼             ▼             ▼
              ┌──────────┐ ┌──────────┐ ┌──────────┐
              │   NSS    │ │   PAM    │ │   PAM    │
              │          │ │  (auth)  │ │(session) │
              │ ¿Existe  │ │ ¿Contra- │ │ Crear    │
              │  alice?  │ │  seña    │ │ home,    │
              │ ¿UID?    │ │  válida? │ │ limits,  │
              │ ¿GID?    │ │          │ │ etc.     │
              │ ¿Home?   │ │          │ │          │
              │ ¿Shell?  │ │          │ │          │
              └────┬─────┘ └────┬─────┘ └────┬─────┘
                   │            │            │
                   ▼            ▼            ▼
              ┌────────────────────────────────────┐
              │              sssd                  │
              │  NSS responder ← pam_sss.so        │
              │       │              │             │
              │       ▼              ▼             │
              │     backend (dominio)              │
              │       │                            │
              │       ▼                            │
              │   caché LDB ←→ LDAP/AD/IPA        │
              └────────────────────────────────────┘
```

**Tres preguntas, tres mecanismos**:

| Pregunta | Mecanismo | Archivo de config |
|---|---|---|
| ¿Quién es `alice`? (identidad) | NSS | `/etc/nsswitch.conf` |
| ¿Es su contraseña correcta? (autenticación) | PAM | `/etc/pam.d/*` |
| ¿Puede entrar en este sistema? (autorización) | PAM + sssd | `sssd.conf` (access_provider) |

---

## 2. NSS — Name Service Switch

NSS (*Name Service Switch*) es el mecanismo del sistema para resolver **nombres a datos estructurados**. No es específico de LDAP — es el framework general que decide *dónde buscar* información del sistema.

### Bases de datos NSS

| Base de datos | Qué resuelve | Función C | Ejemplo |
|---|---|---|---|
| `passwd` | Usuarios | `getpwnam()`, `getpwuid()` | alice → uid 1001 |
| `group` | Grupos | `getgrnam()`, `getgrgid()` | developers → gid 2001 |
| `shadow` | Contraseñas | `getspnam()` | Hash de alice |
| `hosts` | Nombres de host | `gethostbyname()` | server1 → 10.0.0.5 |
| `networks` | Redes | `getnetbyname()` | lan → 10.0.0.0 |
| `services` | Puertos/servicios | `getservbyname()` | ssh → 22/tcp |
| `protocols` | Protocolos | `getprotobyname()` | tcp → 6 |
| `netgroup` | Grupos de red (NIS) | `getnetgrent()` | Para exports NFS |
| `automount` | Mapas de autofs | — | Puntos de montaje |
| `sudoers` | Reglas sudo | — | Quién puede sudo |

### Fuentes (sources)

Cada base de datos puede consultar múltiples fuentes, en orden:

| Fuente | Descripción | Respaldada por |
|---|---|---|
| `files` | Archivos locales (`/etc/passwd`, `/etc/group`) | libc |
| `sss` | sssd (LDAP, AD, IPA) | `libnss_sss.so` |
| `dns` | DNS (solo para `hosts`) | libc |
| `nis` / `nisplus` | NIS/YP (legacy) | `libnss_nis.so` |
| `ldap` | LDAP directo (sin sssd, legacy) | `libnss_ldap.so` |
| `systemd` | systemd-resolved (para hosts) | `libnss_resolve.so` |
| `mymachines` | Contenedores systemd | `libnss_mymachines.so` |
| `myhostname` | Nombre local del host | `libnss_myhostname.so` |
| `winbind` | Samba Winbind (alternativa a sssd para AD) | `libnss_winbind.so` |

---

## 3. nsswitch.conf con sssd

### El archivo /etc/nsswitch.conf

```bash
# /etc/nsswitch.conf — configuración NSS con sssd
#
# Formato: base_de_datos: fuente1 [acción] fuente2 [acción] ...

passwd:     files sss
shadow:     files sss
group:      files sss
# ^^^ Para usuarios, contraseñas y grupos:
#     1. Buscar en /etc/passwd, /etc/shadow, /etc/group
#     2. Si no se encuentra, preguntar a sssd

hosts:      files dns myhostname
# ^^^ hosts no necesita sss (DNS ya funciona)

services:   files sss
netgroup:   sss
automount:  files sss
sudoers:    files sss
```

### Orden de las fuentes: importa

```
passwd:  files  sss
         ─(1)─  ─(2)─

Búsqueda de "alice":
  1. ¿Está alice en /etc/passwd?
     ├── Sí → devolver datos de /etc/passwd (NO consulta sss)
     └── No → continuar
  2. ¿Está alice en sssd?
     ├── Sí → devolver datos de sssd/LDAP
     └── No → NOTFOUND
```

> **Implicación crítica**: Si existe un usuario `alice` en `/etc/passwd` Y en LDAP, **gana el local** porque `files` va primero. Por eso con Active Directory se recomienda `use_fully_qualified_names = True` en sssd: los usuarios de AD se llaman `alice@corp.example.com`, que nunca colisiona con `alice` local.

### Acciones NSS (poco usadas pero útiles)

Puedes controlar qué hacer cuando una fuente devuelve cierto resultado:

```bash
passwd: files [NOTFOUND=return] sss
#             ^^^^^^^^^^^^^^^^^^^
# Si files dice NOTFOUND, detenerse (NO consultar sss)
# Uso: cuando SABES que todos los usuarios deben ser locales

passwd: files [UNAVAIL=continue] sss
#             ^^^^^^^^^^^^^^^^^^
# Si files no está disponible (error), continuar con sss
# (Este es el comportamiento por defecto)
```

| Resultado | Significado | Acción por defecto |
|---|---|---|
| `SUCCESS` | Encontrado | `return` (devolver resultado) |
| `NOTFOUND` | No existe en esta fuente | `continue` (probar siguiente) |
| `UNAVAIL` | Fuente no disponible (error) | `continue` |
| `TRYAGAIN` | Fuente ocupada temporalmente | `continue` |

### Verificar que la librería NSS está instalada

```bash
# sssd necesita su módulo NSS
ls -la /lib64/libnss_sss.so.2    # RHEL
ls -la /lib/x86_64-linux-gnu/libnss_sss.so.2  # Debian

# Si no existe, el sistema silenciosamente ignora "sss" en nsswitch.conf
# No hay error visible — simplemente no funciona
```

### Probar la resolución NSS

```bash
# getent consulta NSS (sigue el orden de nsswitch.conf)
getent passwd alice
# alice:*:1001:1001:Alice Johnson:/home/alice:/bin/bash

# getent con fuente específica (prueba aislada)
getent passwd -s files alice    # solo /etc/passwd
getent passwd -s sss alice      # solo sssd

# Si alice aparece con -s sss pero no con getent passwd,
# probablemente existe en /etc/passwd con otro UID
# y files "gana" la resolución
```

---

## 4. PAM — integración con pam_sss

Para autenticación, sssd proporciona el módulo `pam_sss.so` que se integra en los stacks PAM del sistema.

### pam_sss.so vs pam_ldap.so

| Aspecto | `pam_sss.so` (moderno) | `pam_ldap.so` (legacy) |
|---|---|---|
| Conexión | A través de sssd (socket local) | Directa al servidor LDAP |
| Caché offline | Sí (vía sssd) | No |
| Configuración | `sssd.conf` | `pam_ldap.conf` + `ldap.conf` |
| Múltiples dominios | Sí | No (un solo servidor) |
| Kerberos | Integrado (si sssd lo configura) | Necesita `pam_krb5.so` separado |
| Failover | Automático en sssd | Manual |
| Rendimiento | Socket Unix local (rápido) | Conexión TCP por cada auth (lento) |

`pam_sss.so` no habla LDAP directamente. Se comunica con el respondedor PAM de sssd a través de un socket Unix local (`/var/lib/sss/pipes/pam`). Toda la complejidad de LDAP/AD/Kerberos la maneja sssd internamente.

```
pam_sss.so ──socket Unix──► sssd (PAM responder) ──LDAP/KRB5──► servidor
    │                              │
    │                              ├── Verifica contraseña
    │                              ├── Revisa access_provider
    │                              ├── Cachea credenciales
    │                              └── Gestiona tickets Kerberos
    │
    └── Devuelve PAM_SUCCESS o PAM_AUTH_ERR al servicio
```

---

## 5. Configuración PAM para sssd

### Stacks PAM con pam_sss

La configuración PAM para sssd se distribuye en los cuatro stacks:

```bash
# /etc/pam.d/system-auth (RHEL) o common-* (Debian)

# === STACK AUTH ===
# Verifica la identidad del usuario
auth    required      pam_env.so
auth    sufficient    pam_unix.so nullok
auth    sufficient    pam_sss.so use_first_pass
auth    required      pam_deny.so

# === STACK ACCOUNT ===
# Verifica si la cuenta puede acceder
account required      pam_unix.so
account sufficient    pam_localuser.so
account sufficient    pam_sss.so
account required      pam_permit.so

# === STACK PASSWORD ===
# Gestiona cambios de contraseña
password requisite    pam_pwquality.so retry=3
password sufficient   pam_unix.so sha512 shadow nullok use_authtok
password sufficient   pam_sss.so use_authtok
password required     pam_deny.so

# === STACK SESSION ===
# Configura el entorno de la sesión
session  required     pam_limits.so
session  optional     pam_sss.so
session  required     pam_unix.so
```

### Flujo detallado del stack auth

```
Usuario teclea: alice / MiContraseña123

auth required   pam_env.so
│ Carga variables de entorno
│ Resultado: SUCCESS (siempre)
│ Continúa (required → sigue evaluando)
▼
auth sufficient pam_unix.so nullok
│ ¿alice está en /etc/shadow?
├── Sí → ¿contraseña correcta?
│   ├── Sí → SUCCESS → sufficient → PARA, login OK
│   └── No → FAIL → sufficient falla → continúa
└── No (alice no es local) → FAIL → continúa
▼
auth sufficient pam_sss.so use_first_pass
│ ¿alice está en sssd?
│ use_first_pass: usa la contraseña ya introducida
│                  (NO vuelve a pedirla)
├── Sí → ¿contraseña correcta contra LDAP/AD?
│   ├── Sí → SUCCESS → sufficient → PARA, login OK
│   └── No → FAIL → sufficient falla → continúa
└── No → FAIL → continúa
▼
auth required   pam_deny.so
│ Siempre FAIL → login DENEGADO
│ (red de seguridad: si nada funcionó, denegar)
└── RESULTADO FINAL: AUTH_ERR
```

### Opciones de pam_sss.so

| Opción | Stack | Descripción |
|---|---|---|
| `use_first_pass` | auth | Usar la contraseña ya introducida, no pedir otra |
| `forward_pass` | auth | Pasar la contraseña al siguiente módulo en el stack |
| `use_authtok` | password | Usar el nuevo token ya proporcionado (cambio de contraseña) |
| `quiet` | todos | Suprimir mensajes de log para intentos desconocidos |
| `ignore_unknown_user` | account | No fallar si el usuario no existe en sssd |
| `ignore_authinfo_unavail` | auth | No fallar si sssd no está disponible |
| `retry=N` | auth | Número de reintentos si falla la conexión |

### Diferencia entre use_first_pass y forward_pass

```
use_first_pass:
  pam_unix pide contraseña → "secret"
  pam_sss usa "secret" (la misma)
  Si pam_sss falla → NO pide otra contraseña → falla

forward_pass:
  pam_unix pide contraseña → "secret"
  pam_sss usa "secret" (la misma)
  Si pam_sss falla → PUEDE pedir otra contraseña
```

En la práctica, `use_first_pass` es lo habitual: si la contraseña es incorrecta para LDAP, no tiene sentido pedir otra.

---

## 6. Creación automática de home: mkhomedir

Los usuarios remotos no tienen directorio home creado de antemano en cada máquina. Dos mecanismos lo crean automáticamente en el primer login.

### pam_mkhomedir (PAM)

```bash
# En /etc/pam.d/system-auth o common-session:
session required pam_mkhomedir.so skel=/etc/skel umask=0077
```

| Opción | Descripción |
|---|---|
| `skel=/etc/skel` | Directorio plantilla (se copian .bashrc, .profile, etc.) |
| `umask=0077` | Permisos del home creado (0077 → drwx------) |

### oddjob-mkhomedir (D-Bus, preferido en RHEL)

`pam_mkhomedir` se ejecuta como el usuario que hace login, lo cual puede fallar si se necesitan permisos de root (por ejemplo, crear `/home/alice` cuando `/home` es propiedad de root).

`oddjob-mkhomedir` resuelve esto delegando la creación a un servicio con privilegios de root:

```bash
# Instalar y habilitar
dnf install oddjob oddjob-mkhomedir
systemctl enable --now oddjobd

# En PAM:
session optional pam_oddjob_mkhomedir.so umask=0077

# O con authselect (RHEL 8+):
authselect enable-feature with-mkhomedir
systemctl enable --now oddjobd
```

```
pam_mkhomedir:
  login (como alice) → intenta crear /home/alice → puede fallar si
                                                     alice no puede
                                                     escribir en /home

pam_oddjob_mkhomedir:
  login (como alice) → pide a oddjobd (root) → crea /home/alice → OK
                         vía D-Bus
```

### Verificación

```bash
# Antes del primer login de alice:
ls /home/alice
# ls: cannot access '/home/alice': No such file or directory

# Después del primer login (SSH o local):
ls -la /home/alice/
# drwx------ 2 alice alice 4096 Mar 21 10:00 .
# -rw-r--r-- 1 alice alice  220 Mar 21 10:00 .bashrc
# -rw-r--r-- 1 alice alice  807 Mar 21 10:00 .profile
```

---

## 7. El flujo de autenticación paso a paso

Un ejemplo completo de SSH login de un usuario LDAP, paso a paso:

```
┌─────────────────────────────────────────────────────────────────┐
│  ssh alice@server                                               │
│                                                                 │
│  Paso 1: sshd recibe conexión                                  │
│  ─────────────────────────────────                              │
│  sshd necesita saber si "alice" existe                          │
│                                                                 │
│  Paso 2: NSS — resolución de identidad                         │
│  ─────────────────────────────────────                          │
│  sshd llama getpwnam("alice")                                   │
│  glibc lee /etc/nsswitch.conf:  passwd: files sss              │
│    → files: busca en /etc/passwd → NO                           │
│    → sss: pregunta al respondedor NSS de sssd                   │
│      → sssd consulta caché LDB                                  │
│        → si está y no expiró → devuelve datos                   │
│        → si no → consulta LDAP → cachea → devuelve datos       │
│  Resultado: uid=1001, gid=1001, home=/home/alice, shell=/bin/bash│
│                                                                 │
│  Paso 3: PAM auth — verificación de contraseña                 │
│  ─────────────────────────────────────────────                  │
│  sshd invoca pam_authenticate()                                 │
│    → pam_unix: alice no está en /etc/shadow → FAIL → continúa  │
│    → pam_sss (use_first_pass): envía contraseña a sssd          │
│      → sssd hace LDAP bind como alice con la contraseña         │
│        (o Kerberos kinit si auth_provider=krb5/ad)              │
│      → bind exitoso → SUCCESS → sufficient → PARA              │
│      → sssd cachea hash de contraseña para offline              │
│                                                                 │
│  Paso 4: PAM account — verificación de cuenta                  │
│  ────────────────────────────────────────────                   │
│  sshd invoca pam_acct_mgmt()                                    │
│    → pam_unix: alice no es local → FAIL                         │
│    → pam_localuser: alice no es local → FAIL                    │
│    → pam_sss: pregunta a sssd                                   │
│      → sssd evalúa access_provider                              │
│        → simple: ¿alice en simple_allow_users/groups? → Sí      │
│      → SUCCESS                                                  │
│                                                                 │
│  Paso 5: PAM session — configuración del entorno               │
│  ───────────────────────────────────────────────                │
│  sshd invoca pam_open_session()                                 │
│    → pam_limits: aplica /etc/security/limits.conf               │
│    → pam_mkhomedir: /home/alice no existe → CREA               │
│    → pam_sss: notifica a sssd (para Kerberos ticket cache)      │
│    → pam_unix: registra sesión en utmp/wtmp                     │
│                                                                 │
│  Paso 6: Shell                                                  │
│  ─────────                                                      │
│  sshd lanza /bin/bash como alice (uid=1001)                     │
│  alice está logueada                                            │
└─────────────────────────────────────────────────────────────────┘
```

---

## 8. authselect (RHEL) y pam-auth-update (Debian)

Editar manualmente los archivos PAM es propenso a errores. Ambas familias de distribuciones proporcionan herramientas para gestionar la integración.

### authselect (RHEL 8+)

`authselect` gestiona `/etc/nsswitch.conf` y `/etc/pam.d/*` a partir de **perfiles** predefinidos:

```bash
# Listar perfiles disponibles
authselect list
# - sssd         ← para sssd
# - winbind      ← para Samba Winbind
# - nis          ← para NIS (legacy)
# - minimal      ← sin fuentes remotas

# Ver perfil actual
authselect current

# Seleccionar perfil sssd con opciones
authselect select sssd with-mkhomedir with-sudo --force
#                  ^^^^  ^^^^^^^^^^^^^^ ^^^^^^^^^
#                  perfil   opciones (features)

# Features comunes para sssd:
# with-mkhomedir    → añade pam_oddjob_mkhomedir a session
# with-sudo         → añade sss a sudoers en nsswitch.conf
# with-smartcard    → autenticación con tarjeta inteligente
# with-fingerprint  → autenticación biométrica
# with-faillock     → bloqueo tras intentos fallidos
# with-pamaccess    → control de acceso con pam_access
```

**Qué hace authselect internamente**:

```bash
authselect select sssd with-mkhomedir
# 1. Genera /etc/pam.d/system-auth con pam_sss.so en los stacks
# 2. Genera /etc/pam.d/password-auth con pam_sss.so
# 3. Modifica /etc/nsswitch.conf: passwd: files sss
# 4. Añade pam_oddjob_mkhomedir.so al stack session
# 5. Crea symlinks desde /etc/pam.d/sshd → system-auth
```

> **No edites manualmente** los archivos generados por authselect. Si lo haces, authselect se queja y se niega a aplicar futuros cambios. Usa `authselect enable-feature` o `authselect disable-feature` para modificar la configuración.

```bash
# Añadir una feature después
authselect enable-feature with-faillock

# Quitar una feature
authselect disable-feature with-mkhomedir

# Forzar si hay modificaciones manuales
authselect select sssd with-mkhomedir --force
# ^^^ Sobrescribe cambios manuales en PAM/nsswitch
```

### pam-auth-update (Debian/Ubuntu)

```bash
# Modo interactivo (ncurses)
pam-auth-update
# Muestra checkboxes para activar/desactivar:
# [*] Unix authentication
# [*] SSS authentication
# [*] Register user sessions in the systemd login manager
# [*] Create home directory on login
# [ ] GNOME Keyring Daemon - Login keyring management

# Modo no interactivo
pam-auth-update --enable mkhomedir
pam-auth-update --disable mkhomedir
```

Los perfiles de `pam-auth-update` viven en `/usr/share/pam-configs/`:

```bash
# /usr/share/pam-configs/sss
Name: SSS authentication
Default: yes
Priority: 128
Auth-Type: Primary
Auth:
    [success=end default=ignore] pam_sss.so forward_pass
Auth-Initial:
    [success=end default=ignore] pam_sss.so forward_pass
Account-Type: Additional
Account:
    [default=bad success=ok user_unknown=ignore] pam_sss.so
Password-Type: Primary
Password:
    [success=end default=ignore] pam_sss.so use_authtok
Password-Initial:
    [success=end default=ignore] pam_sss.so use_authtok
Session-Type: Additional
Session:
    optional pam_sss.so
```

### Comparación

| Aspecto | authselect (RHEL) | pam-auth-update (Debian) |
|---|---|---|
| Gestiona | PAM + nsswitch.conf | Solo PAM |
| Concepto | Perfiles con features | Módulos individuales |
| nsswitch.conf | Lo genera automáticamente | Lo editas manualmente |
| Edición manual PAM | Rechazada (se sobrescribe) | Posible pero desaconsejada |
| Configurar nsswitch | Automático | `apt install libnss-sss` + editar manualmente |

---

## 9. Diagnóstico de la integración

### Verificar NSS

```bash
# 1. ¿sssd está corriendo?
systemctl status sssd

# 2. ¿La librería NSS está instalada?
ls /lib*/libnss_sss*
# Debe existir libnss_sss.so.2

# 3. ¿nsswitch.conf tiene sss?
grep -E '^(passwd|group|shadow)' /etc/nsswitch.conf
# passwd: files sss
# group:  files sss
# shadow: files sss

# 4. ¿Se resuelve el usuario?
getent passwd alice
# Si devuelve datos → NSS funciona
# Si vacío → NSS no encuentra al usuario

# 5. Probar fuente específica
getent passwd -s sss alice
# Si funciona con -s sss pero no sin él:
#   → Hay un alice local en /etc/passwd que "gana"

# 6. ¿Se resuelven grupos?
getent group developers
id alice
# Verificar que los grupos remotos aparecen
```

### Verificar PAM

```bash
# 1. ¿pam_sss.so está en la configuración?
grep pam_sss /etc/pam.d/system-auth     # RHEL
grep pam_sss /etc/pam.d/common-auth     # Debian

# 2. ¿El módulo existe?
ls /lib*/security/pam_sss.so
# Debe existir

# 3. Probar autenticación (sin hacer login completo)
sssctl user-checks alice
# Verifica:
#   - NSS lookup (identidad)
#   - Grupos
#   - InfoPipe

# 4. Probar autenticación PAM directamente
pamtester sshd alice authenticate
# Pide contraseña y muestra PAM_SUCCESS o el error
# (instalar: dnf install pamtester / apt install pamtester)
```

### Logs de sssd

```bash
# Logs generales
journalctl -u sssd

# Logs detallados por componente (en /var/log/sssd/)
ls /var/log/sssd/
# sssd.log               ← monitor principal
# sssd_nss.log            ← respondedor NSS
# sssd_pam.log            ← respondedor PAM
# sssd_DOMAIN.log         ← backend del dominio

# Aumentar verbosidad temporalmente
sssctl debug-level 6
# Niveles: 0 (fatal) → 9 (trace)
# CUIDADO: nivel 6+ genera MUCHO output

# Volver a nivel normal
sssctl debug-level 2

# O en sssd.conf (persistente):
# [domain/lab.internal]
# debug_level = 6
```

### Diagnóstico completo paso a paso

```bash
# Si alice no puede hacer login, seguir este orden:

# 1. ¿sssd funciona?
systemctl status sssd
sssctl domain-status lab.internal
# ¿Online u Offline?

# 2. ¿NSS resuelve?
getent passwd alice
# Si NO → problema en NSS/nsswitch/sssd

# 3. ¿Contraseña correcta?
# Revisar /var/log/sssd/sssd_pam.log
# Buscar: pam_sss, alice, resultado

# 4. ¿Access provider permite?
# Si auth OK pero login falla → access_provider deniega
# Revisar sssd.conf: simple_allow_users/groups
# Revisar log del dominio: sssd_DOMAIN.log

# 5. ¿Home existe?
ls -la /home/alice
# Si NO → ¿está pam_mkhomedir configurado?

# 6. ¿Shell válida?
getent passwd alice | cut -d: -f7
# Si es /sbin/nologin o vacía → no puede login interactivo
```

---

## 10. Errores comunes

### Error 1: nsswitch.conf sin "sss" después de instalar sssd

```bash
# Síntoma: sssd funciona (sssctl domain-status OK) pero getent no muestra usuarios

# Verificar:
grep passwd /etc/nsswitch.conf
# passwd: files          ← falta sss!

# Causa: se instaló sssd pero no se configuró nsswitch.conf
# En RHEL, authselect lo hace automáticamente
# En Debian, hay que editar manualmente o instalar libnss-sss

# Solución:
# passwd: files sss
# group:  files sss
# shadow: files sss

# En Debian, verificar que el paquete está:
dpkg -l libnss-sss
```

Sin `sss` en nsswitch.conf, el sistema puede autenticar vía PAM (pam_sss.so) pero **no puede resolver la identidad** (UID, GID, home, shell). El resultado: PAM autentica correctamente pero el login falla porque el sistema no sabe quién es el usuario.

### Error 2: Usuario local "sombrea" al usuario LDAP

```bash
# Síntoma: alice hace login pero tiene UID 500 en vez de 1001

# Causa: existe alice en /etc/passwd con UID 500
grep alice /etc/passwd
# alice:x:500:500::/home/alice:/bin/bash

# nsswitch.conf: passwd: files sss
# "files" va primero → alice local gana sobre alice LDAP

# Solución 1: eliminar alice local (si no se necesita)
userdel alice

# Solución 2: usar fully qualified names en sssd
# [domain/corp.example.com]
# use_fully_qualified_names = True
# Ahora el usuario LDAP es alice@corp.example.com (no colisiona)
```

### Error 3: pam_sss.so sin use_first_pass

```bash
# Síntoma: el sistema pide la contraseña DOS VECES

# Causa: pam_sss.so sin use_first_pass pide su propia contraseña
# auth sufficient pam_unix.so nullok
# auth sufficient pam_sss.so              ← pide contraseña otra vez

# Solución:
# auth sufficient pam_sss.so use_first_pass
# Reutiliza la contraseña que pam_unix ya pidió
```

### Error 4: Home no se crea automáticamente

```bash
# Síntoma: login exitoso pero falla con "Could not chdir to home directory"

# Verificar:
grep mkhomedir /etc/pam.d/system-auth    # RHEL
grep mkhomedir /etc/pam.d/common-session # Debian
# Si no aparece → no está configurado

# Solución RHEL:
authselect enable-feature with-mkhomedir
systemctl enable --now oddjobd

# Solución Debian:
pam-auth-update --enable mkhomedir
```

### Error 5: Confundir access_provider con auth_provider

```ini
# Síntoma: alice se autentica correctamente pero NO puede hacer login

# auth_provider verifica la contraseña → OK
# access_provider decide si puede entrar → DENIED

[domain/lab.internal]
auth_provider = ldap       # ← verifica contraseña contra LDAP
access_provider = simple   # ← decide quién puede entrar
simple_allow_groups = sysadmins
# alice no está en sysadmins → login denegado DESPUÉS de autenticarse

# En el log (sssd_pam.log):
# "alice" access denied by access provider
```

La distinción:
- `auth_provider`: "¿tu contraseña es correcta?" → Sí/No
- `access_provider`: "¿tienes permiso de entrar en ESTA máquina?" → Sí/No

Un usuario puede tener la contraseña correcta pero no tener acceso al sistema.

---

## 11. Cheatsheet

```
=== NSS ===
/etc/nsswitch.conf             dónde buscar usuarios/grupos
passwd: files sss              buscar en local, luego sssd
/lib*/libnss_sss.so.2          módulo NSS de sssd (debe existir)
getent passwd usuario          probar resolución NSS
getent passwd -s sss usuario   probar solo la fuente sss

=== PAM ===
pam_sss.so                     módulo PAM de sssd
/lib*/security/pam_sss.so      ubicación del módulo
use_first_pass                 reutilizar contraseña ya introducida
use_authtok                    reutilizar token en stack password
ignore_unknown_user            no fallar si usuario no existe en sss
ignore_authinfo_unavail        no fallar si sssd no disponible

=== MKHOMEDIR ===
pam_mkhomedir.so               crear home en primer login (PAM)
pam_oddjob_mkhomedir.so        crear home vía oddjobd (RHEL, preferido)
skel=/etc/skel                 plantilla para el home
umask=0077                     permisos del home (drwx------)

=== AUTHSELECT (RHEL) ===
authselect list                listar perfiles
authselect current             ver perfil activo
authselect select sssd         activar perfil sssd
  with-mkhomedir               crear home automáticamente
  with-sudo                    reglas sudo desde LDAP
  with-faillock                bloqueo tras fallos
authselect enable-feature X    añadir feature
authselect disable-feature X   quitar feature

=== PAM-AUTH-UPDATE (Debian) ===
pam-auth-update                configurar PAM interactivamente
pam-auth-update --enable X     activar módulo
pam-auth-update --disable X    desactivar módulo
/usr/share/pam-configs/        definiciones de módulos

=== DIAGNÓSTICO ===
sssctl user-checks usuario     verificación completa de un usuario
pamtester sshd usuario auth    probar PAM directamente
sssctl debug-level 6           aumentar verbosidad
/var/log/sssd/sssd_pam.log     log del respondedor PAM
/var/log/sssd/sssd_nss.log     log del respondedor NSS
/var/log/sssd/sssd_DOMAIN.log  log del backend

=== FLUJO DE LOGIN ===
1. NSS (getpwnam)  → ¿Existe el usuario? → nsswitch.conf → sss
2. PAM auth        → ¿Contraseña OK?     → pam_sss.so → sssd → LDAP bind
3. PAM account     → ¿Puede entrar?      → pam_sss.so → access_provider
4. PAM session     → Crear home, limits   → pam_mkhomedir + pam_sss
5. Shell           → Lanzar /bin/bash
```

---

## 12. Ejercicios

### Ejercicio 1: Trazar el flujo de login

Un servidor Linux tiene esta configuración:

```bash
# /etc/nsswitch.conf
passwd: files sss
group:  files sss
shadow: files sss
```

```ini
# /etc/sssd/sssd.conf
[sssd]
config_file_version = 2
services = nss, pam
domains = lab.internal

[domain/lab.internal]
id_provider = ldap
auth_provider = ldap
access_provider = simple
simple_allow_groups = webdevs
ldap_uri = ldaps://ldap.lab.internal
ldap_search_base = dc=lab,dc=internal
cache_credentials = True
```

```bash
# /etc/pam.d/sshd incluye system-auth que tiene:
auth sufficient pam_unix.so nullok
auth sufficient pam_sss.so use_first_pass
auth required   pam_deny.so
```

Para cada escenario, describe paso a paso qué ocurre y cuál es el resultado final:

1. `bob` existe solo en LDAP, pertenece al grupo `webdevs`, contraseña correcta
2. `root` (solo local), contraseña correcta
3. `carol` existe solo en LDAP, pertenece al grupo `marketing`, contraseña correcta
4. `dave` existe en `/etc/passwd` (UID 600) Y en LDAP (UID 1005), contraseña LDAP correcta, contraseña local diferente — usa la contraseña de LDAP

> **Pregunta de predicción**: En el escenario 4, ¿con qué UID se inicia la sesión de dave? ¿La de `/etc/passwd` (600) o la de LDAP (1005)? ¿Por qué? ¿Qué implicaciones tiene para los archivos en su home?

### Ejercicio 2: Configurar la integración completa

Tienes un servidor Debian recién instalado. El servidor LDAP ya funciona en `ldaps://ldap.empresa.local` con base DN `dc=empresa,dc=local`. Escribe los comandos y archivos necesarios para:

1. Instalar los paquetes necesarios
2. Crear `/etc/sssd/sssd.conf` con el dominio LDAP
3. Configurar `/etc/nsswitch.conf`
4. Habilitar `pam_sss` y `mkhomedir` en PAM
5. Arrancar y habilitar sssd
6. Verificar que todo funciona

Para cada paso, explica qué componente configuras (NSS, PAM, sssd) y por qué es necesario.

> **Pregunta de predicción**: Si completas los pasos 1-3 y 5-6 pero olvidas el paso 4 (PAM), ¿qué podrás hacer y qué no? ¿`getent passwd alice` funcionará? ¿`ssh alice@localhost` funcionará? ¿Por qué?

### Ejercicio 3: Diagnosticar una integración rota

Un compañero reporta que el usuario `marcos` no puede hacer SSH al servidor. Estos son los resultados de tu investigación:

```bash
$ systemctl status sssd
● sssd.service - active (running)

$ sssctl domain-status empresa.local
Online status: Online
Active server: ldaps://ldap.empresa.local

$ getent passwd marcos
marcos:*:1042:1042:Marcos Lopez:/home/marcos:/bin/bash

$ ssh marcos@localhost
marcos@localhost's password: ****
Permission denied, please try again.

$ grep marcos /var/log/sssd/sssd_pam.log
pam_sss: authentication success, user: marcos
pam_sss: account management: access denied

$ grep access_provider /etc/sssd/sssd.conf
access_provider = simple
simple_allow_groups = sysadmins
```

1. ¿Cuál es el problema exacto?
2. ¿En qué paso del flujo de login falla?
3. ¿Por qué la autenticación fue exitosa pero el acceso fue denegado?
4. Escribe los comandos necesarios para solucionarlo (ofrece dos alternativas)
5. ¿Qué cambiarías para que este tipo de problema sea más visible para el usuario que intenta login?

> **Pregunta de predicción**: Si cambias `access_provider = simple` a `access_provider = permit`, ¿qué implicaciones de seguridad tiene? ¿Cuántos usuarios del directorio LDAP podrían potencialmente hacer login?

---

> **Siguiente tema**: T04 — Herramientas de diagnóstico (ldapsearch, sssctl, logs de sssd).
