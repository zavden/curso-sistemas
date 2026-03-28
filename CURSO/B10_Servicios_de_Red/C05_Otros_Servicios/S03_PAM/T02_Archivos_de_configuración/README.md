# Archivos de configuración PAM

## Índice
1. [Estructura de /etc/pam.d/](#1-estructura-de-etcpamd)
2. [Formato de línea](#2-formato-de-línea)
3. [Archivos comunes (common-* / system-auth)](#3-archivos-comunes-common--system-auth)
4. [Configuraciones por servicio](#4-configuraciones-por-servicio)
5. [/etc/security/ — configuración de módulos](#5-etcsecurity--configuración-de-módulos)
6. [pam-auth-update (Debian)](#6-pam-auth-update-debian)
7. [authselect (RHEL)](#7-authselect-rhel)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Estructura de /etc/pam.d/

Cada aplicación que usa PAM tiene un archivo de configuración en `/etc/pam.d/` con su nombre:

```bash
ls /etc/pam.d/
```

```
common-account     login          sshd
common-auth        other          su
common-password    passwd         sudo
common-session     polkit-1       systemd-user
common-session-noninteractive
```

```
┌──────────────────────────────────────────────────────────┐
│          Mapeo aplicación → archivo PAM                   │
│                                                          │
│  Aplicación        Archivo PAM                           │
│  ───────────       ─────────────                         │
│  /usr/sbin/sshd    /etc/pam.d/sshd                       │
│  /bin/login        /etc/pam.d/login                      │
│  /bin/su           /etc/pam.d/su                          │
│  /usr/bin/sudo     /etc/pam.d/sudo                       │
│  /usr/bin/passwd   /etc/pam.d/passwd                     │
│  (desconocido)     /etc/pam.d/other    ← fallback        │
│                                                          │
│  La aplicación pasa su nombre a libpam al inicializar.   │
│  libpam busca /etc/pam.d/<nombre>.                       │
│  Si no existe → usa /etc/pam.d/other                     │
└──────────────────────────────────────────────────────────┘
```

### /etc/pam.d/other — el fallback

```bash
# /etc/pam.d/other — se aplica a apps sin config propia
# Política segura: denegar todo
auth     required  pam_deny.so
account  required  pam_deny.so
password required  pam_deny.so
session  required  pam_deny.so
```

Si una aplicación PAM no tiene su propio archivo en `/etc/pam.d/`, `other` se aplica como política por defecto. Configurarlo como `pam_deny.so` es la práctica segura — deniega acceso a cualquier aplicación no configurada explícitamente.

---

## 2. Formato de línea

### Sintaxis

```
type    control    module.so    [argumentos]
```

```bash
# Ejemplos:
auth     required   pam_unix.so    nullok
account  requisite  pam_account.so
session  optional   pam_motd.so    motd=/run/motd.dynamic
password required   pam_pwquality.so retry=3 minlen=8
```

### Campos

| Campo | Descripción | Valores |
|-------|------------|---------|
| type | Stack/fase de PAM | `auth`, `account`, `password`, `session` |
| control | Cómo afecta al resultado | `required`, `requisite`, `sufficient`, `optional`, `[...]` |
| module | Módulo a ejecutar | Nombre del `.so` (ruta relativa al directorio de módulos) |
| argumentos | Parámetros del módulo | Específicos de cada módulo, separados por espacios |

### Comentarios y formato

```bash
# Comentarios con #
# Esto es un comentario

# Líneas vacías se ignoran

# Los campos se separan por espacios o tabs
auth    required    pam_unix.so    nullok

# NO hay continuación de línea — cada regla en una línea
```

### Argumentos comunes entre módulos

| Argumento | Módulos | Significado |
|-----------|---------|------------|
| `nullok` | pam_unix | Permitir contraseñas vacías |
| `try_first_pass` | Varios | Usar la contraseña ya introducida, si falla pedir otra |
| `use_first_pass` | Varios | Usar la contraseña ya introducida, si falla no pedir otra |
| `debug` | Casi todos | Registrar información de debug en syslog |
| `audit` | Varios | Registrar información de auditoría |
| `shadow` | pam_unix | Usar /etc/shadow |
| `sha512` | pam_unix | Hash SHA-512 para contraseñas |

---

## 3. Archivos comunes (common-* / system-auth)

Para evitar duplicar las mismas reglas en cada servicio, las distribuciones usan archivos compartidos.

### Debian/Ubuntu — common-*

```
┌──────────────────────────────────────────────────────────┐
│          Estructura Debian                                │
│                                                          │
│  /etc/pam.d/sshd ──include──► /etc/pam.d/common-auth    │
│  /etc/pam.d/login ──include──► /etc/pam.d/common-auth   │
│  /etc/pam.d/su ──include──► /etc/pam.d/common-auth      │
│  /etc/pam.d/sudo ──include──► /etc/pam.d/common-auth    │
│                                                          │
│  common-auth      Autenticación compartida               │
│  common-account   Verificación de cuenta compartida      │
│  common-password  Cambio de contraseña compartido        │
│  common-session   Sesión compartida                      │
│  common-session-noninteractive  Sesión sin terminal      │
│                                                          │
│  Cambiar common-auth = cambiar auth para TODAS las apps  │
└──────────────────────────────────────────────────────────┘
```

```bash
# /etc/pam.d/common-auth (Debian típico)
auth  [success=2 default=ignore]  pam_unix.so nullok
auth  [success=1 default=ignore]  pam_sss.so use_first_pass
auth  requisite                   pam_deny.so
auth  required                    pam_permit.so
auth  optional                    pam_cap.so
```

Desglose de la lógica con `success=N`:

```
Módulo 1: pam_unix.so [success=2]
  → Si SUCCESS: salta 2 módulos → llega a pam_permit.so
  → Si FAIL: default=ignore → continúa al siguiente

Módulo 2: pam_sss.so [success=1]
  → Si SUCCESS: salta 1 módulo → llega a pam_permit.so
  → Si FAIL: default=ignore → continúa al siguiente

Módulo 3: pam_deny.so (requisite)
  → Siempre FAIL → stack falla inmediatamente
  → Solo se alcanza si unix Y sss fallaron

Módulo 4: pam_permit.so (required)
  → Siempre SUCCESS → stack pasa
  → Solo se alcanza si unix O sss tuvieron éxito (por los saltos)
```

```bash
# /etc/pam.d/common-session (Debian típico)
session [default=1]            pam_permit.so
session requisite              pam_deny.so
session required               pam_permit.so
session optional               pam_umask.so
session required               pam_unix.so
session optional               pam_sss.so
session optional               pam_systemd.so
session optional               pam_mkhomedir.so
```

### RHEL/Fedora — system-auth / password-auth

```
┌──────────────────────────────────────────────────────────┐
│          Estructura RHEL                                  │
│                                                          │
│  /etc/pam.d/system-auth                                  │
│    → Usado por servicios LOCALES (login, su, sudo)       │
│                                                          │
│  /etc/pam.d/password-auth                                │
│    → Usado por servicios REMOTOS (sshd, vsftpd)          │
│                                                          │
│  Ambos son symlinks gestionados por authselect            │
│  → /etc/authselect/system-auth                           │
│  → /etc/authselect/password-auth                         │
│                                                          │
│  NO editar directamente — usar authselect                │
└──────────────────────────────────────────────────────────┘
```

```bash
# /etc/pam.d/system-auth (RHEL típico)
auth        required      pam_env.so
auth        required      pam_faildelay.so delay=2000000
auth        sufficient    pam_unix.so nullok
auth        sufficient    pam_sss.so use_first_pass
auth        required      pam_deny.so

account     required      pam_unix.so
account     sufficient    pam_localuser.so
account     sufficient    pam_succeed_if.so uid < 500 quiet
account     [default=bad success=ok user_unknown=ignore]  pam_sss.so
account     required      pam_permit.so

password    requisite     pam_pwquality.so retry=3
password    sufficient    pam_unix.so sha512 shadow nullok use_authtok
password    sufficient    pam_sss.so use_authtok
password    required      pam_deny.so

session     optional      pam_keyinit.so revoke
session     required      pam_limits.so
session     optional      pam_systemd.so
session     [success=1 default=ignore]  pam_succeed_if.so service in crond quiet use_uid
session     required      pam_unix.so
session     optional      pam_sss.so
```

---

## 4. Configuraciones por servicio

### /etc/pam.d/sshd

```bash
# /etc/pam.d/sshd (Debian)
# Autenticación
@include common-auth

# Verificación de cuenta
account    required     pam_nologin.so     # Bloquea si existe /etc/nologin
@include common-account

# Cambio de contraseña (si la cuenta lo requiere)
@include common-password

# Sesión
session    [success=ok ignore=ignore module_unknown=ignore default=bad] pam_selinux.so close
session    required     pam_loginuid.so    # Registrar UID de login en audit
session    optional     pam_keyinit.so force revoke
@include common-session
session    [success=ok ignore=ignore module_unknown=ignore default=bad] pam_selinux.so open
session    optional     pam_motd.so  motd=/run/motd.dynamic
session    optional     pam_mail.so  standard noenv
```

### /etc/pam.d/su

```bash
# /etc/pam.d/su (Debian)
auth       sufficient   pam_rootok.so         # root no necesita contraseña para su
# auth     required     pam_wheel.so          # Descomentar para exigir grupo wheel
@include common-auth

account    sufficient   pam_rootok.so
@include common-account

session    required     pam_env.so readenv=1
session    required     pam_env.so readenv=1 envfile=/etc/default/locale
@include common-session
```

`pam_rootok.so` con `sufficient` permite que root ejecute `su` sin contraseña. Si se descomenta `pam_wheel.so`, solo los usuarios del grupo `wheel` pueden usar `su`.

### /etc/pam.d/sudo

```bash
# /etc/pam.d/sudo (Debian)
@include common-auth
@include common-account
@include common-session-noninteractive
```

sudo es típicamente simple porque delega toda la lógica a los archivos common-*.

### /etc/pam.d/passwd

```bash
# /etc/pam.d/passwd
password   required   pam_pwquality.so retry=3
@include common-password
```

Solo define el stack `password` porque `passwd` solo cambia contraseñas.

---

## 5. /etc/security/ — configuración de módulos

Los módulos PAM buscan su configuración en `/etc/security/`:

### limits.conf — límites de recursos

```bash
# /etc/security/limits.conf
# Usado por pam_limits.so

# <domain>  <type>  <item>       <value>
# domain: usuario, @grupo, * (todos)
# type: soft (puede subir hasta hard), hard (máximo absoluto)

*          soft    nofile        1024      # Archivos abiertos (soft)
*          hard    nofile        65536     # Archivos abiertos (hard)
@developers soft   nproc         4096      # Procesos máximos
juan       hard    maxlogins     3         # Máximo sesiones simultáneas
@students  hard    cpu           60        # Minutos de CPU
*          soft    core          0         # Deshabilitar core dumps
```

### access.conf — control de acceso

```bash
# /etc/security/access.conf
# Usado por pam_access.so

# + (permitir) o - (denegar) : usuarios : orígenes

# Solo usuarios del grupo admin pueden hacer login remoto
- : ALL EXCEPT root @admin : ALL

# Solo root puede hacer login en tty1
+ : root : tty1
- : ALL : tty1

# Permitir acceso desde la red local, denegar el resto
+ : ALL : 192.168.1.0/24
- : ALL : ALL

# Solo maria puede acceder desde la VPN
+ : maria : 10.8.0.0/24
- : ALL : 10.8.0.0/24
```

```bash
# Activar en el stack PAM correspondiente:
# account required pam_access.so
```

### time.conf — restricciones temporales

```bash
# /etc/security/time.conf
# Usado por pam_time.so

# services ; ttys ; users ; times

# Solo permitir login en horario laboral (L-V 8:00-18:00)
login ; * ; !root ; !Al0800-1800

# SSH solo de lunes a viernes
sshd ; * ; ALL ; Wk0000-2400

# Notación de tiempo:
# Mo Tu We Th Fr Sa Su  (días individuales)
# Wk = lunes a viernes
# Wd = sábado y domingo
# Al = todos los días
# HHMM-HHMM = rango de horas
```

### pwquality.conf — política de contraseñas

```bash
# /etc/security/pwquality.conf
# Usado por pam_pwquality.so

minlen = 12          # Longitud mínima
dcredit = -1         # Al menos 1 dígito
ucredit = -1         # Al menos 1 mayúscula
lcredit = -1         # Al menos 1 minúscula
ocredit = -1         # Al menos 1 carácter especial
maxrepeat = 3        # Máximo 3 caracteres iguales seguidos
maxclassrepeat = 4   # Máximo 4 del mismo tipo seguidos
difok = 5            # Al menos 5 caracteres diferentes de la anterior
reject_username      # No permitir el username en la contraseña
enforce_for_root     # Aplicar también a root
```

### namespace.conf — aislamiento de directorios

```bash
# /etc/security/namespace.conf
# Usado por pam_namespace.so
# Crea directorios aislados por usuario

# /tmp aislado por usuario
/tmp     /tmp-inst/     level    root,admin
# Cada usuario ve un /tmp diferente
# root y grupo admin ven el /tmp real
```

---

## 6. pam-auth-update (Debian)

En Debian/Ubuntu, **no se editan los archivos common-* manualmente**. Se usa `pam-auth-update` que gestiona los módulos de forma segura.

### Uso

```bash
# Interfaz interactiva (ncurses)
sudo pam-auth-update
```

```
┌─────────────────────────────────────────────────────┐
│  PAM profiles to enable:                            │
│                                                     │
│  [*] Unix authentication                            │
│  [*] SSSD authentication (for LDAP/AD)              │
│  [ ] Kerberos authentication                        │
│  [*] Register user sessions in systemd              │
│  [*] Create home directory on login                 │
│  [*] Inheritable Capabilities Management            │
│                                                     │
│              <Ok>               <Cancel>             │
└─────────────────────────────────────────────────────┘
```

```bash
# Habilitar un perfil sin interfaz interactiva
sudo pam-auth-update --enable mkhomedir

# Deshabilitar un perfil
sudo pam-auth-update --disable mkhomedir

# Forzar regeneración de archivos common-*
sudo pam-auth-update --force
```

### Cómo funciona

Cada paquete que proporciona un módulo PAM instala un perfil en `/usr/share/pam-configs/`:

```bash
# /usr/share/pam-configs/unix
Name: Unix authentication
Default: yes
Priority: 256
Auth-Type: Primary
Auth:
    [success=end default=ignore]    pam_unix.so nullok
Auth-Initial:
    [success=end default=ignore]    pam_unix.so nullok
Account-Type: Primary
Account:
    [success=end new_authtok_reqd=done default=ignore]    pam_unix.so
Password-Type: Primary
Password:
    [success=end default=ignore]    pam_unix.so obscure use_authtok try_first_pass yescrypt
Password-Initial:
    [success=end default=ignore]    pam_unix.so obscure yescrypt
Session-Type: Additional
Session-Interactive-Only: no
Session:
    required    pam_unix.so
```

`pam-auth-update` combina todos los perfiles habilitados en los archivos `common-*`, respetando prioridades y tipos.

---

## 7. authselect (RHEL)

En RHEL 8+, **authselect** reemplaza a `authconfig` para gestionar la configuración PAM.

### Perfiles disponibles

```bash
# Listar perfiles disponibles
sudo authselect list
```

```
- minimal                 Local users only
- sssd                    SSSD for remote auth (LDAP, AD, Kerberos)
- winbind                 Winbind for AD integration
- nis                     NIS authentication (legacy)
```

### Uso

```bash
# Ver perfil actual
sudo authselect current

# Seleccionar perfil SSSD (para LDAP/AD)
sudo authselect select sssd

# Con opciones adicionales
sudo authselect select sssd with-mkhomedir with-faillock

# Opciones comunes:
# with-mkhomedir     → Crear home en primer login
# with-faillock      → Bloquear tras intentos fallidos
# with-fingerprint   → Autenticación biométrica
# with-smartcard     → Tarjeta inteligente
# with-silent-lastlog → No mostrar último login
```

### Archivos que gestiona

```bash
# authselect gestiona estos archivos (NO editar directamente):
/etc/pam.d/system-auth        → symlink a /etc/authselect/system-auth
/etc/pam.d/password-auth      → symlink a /etc/authselect/password-auth
/etc/pam.d/fingerprint-auth
/etc/pam.d/smartcard-auth
/etc/pam.d/postlogin
/etc/nsswitch.conf
```

```bash
# Verificar que no se editaron manualmente
sudo authselect check
# Current configuration is valid.
# o
# File /etc/pam.d/system-auth was modified outside authselect.
```

### Personalización

```bash
# Crear perfil personalizado basado en sssd
sudo authselect create-profile custom-corp --base-on sssd

# Editar el perfil personalizado
sudo vim /etc/authselect/custom/custom-corp/system-auth

# Aplicar el perfil personalizado
sudo authselect select custom/custom-corp with-mkhomedir
```

---

## 8. Errores comunes

### Error 1 — Editar common-auth directamente en Debian

```bash
# MAL — la próxima ejecución de pam-auth-update sobreescribirá los cambios
sudo vim /etc/pam.d/common-auth

# BIEN — crear un perfil en /usr/share/pam-configs/
# O agregar reglas en el archivo del servicio ANTES del include:
# /etc/pam.d/sshd
auth required pam_faillock.so preauth    # ← Regla específica para sshd
@include common-auth                      # ← Include estándar
```

### Error 2 — Editar system-auth en RHEL sin authselect

```bash
# MAL — authselect detecta la modificación y puede revertir
sudo vim /etc/pam.d/system-auth

# BIEN
sudo authselect create-profile mi-perfil --base-on sssd
sudo vim /etc/authselect/custom/mi-perfil/system-auth
sudo authselect select custom/mi-perfil
```

### Error 3 — /etc/pam.d/other demasiado permisivo

```bash
# MAL — permite acceso a cualquier app sin config PAM propia
auth     required  pam_permit.so
account  required  pam_permit.so
password required  pam_permit.so
session  required  pam_permit.so

# BIEN — deniega acceso a apps no configuradas
auth     required  pam_deny.so
account  required  pam_deny.so
password required  pam_deny.so
session  required  pam_deny.so
# Con logging para detectar apps que usan other:
auth     required  pam_warn.so
```

### Error 4 — Orden incorrecto en limits.conf

```bash
# limits.conf se procesa de arriba a abajo
# La ÚLTIMA coincidencia gana

# MAL — el wildcard sobreescribe la regla específica
juan     hard    nofile    65536
*        hard    nofile    1024     # ← Esta gana para juan también

# BIEN — wildcard primero, excepciones después
*        hard    nofile    1024
juan     hard    nofile    65536    # ← Sobreescribe el wildcard para juan
```

### Error 5 — Olvidar activar el módulo en el stack PAM

```bash
# Editaste /etc/security/access.conf con reglas de acceso
# pero nadie lo respeta...

# Causa: pam_access.so no está en el stack PAM del servicio
# Verificar:
grep pam_access /etc/pam.d/sshd
# (vacío — no está)

# Solución: agregar al archivo PAM del servicio
# /etc/pam.d/sshd
account required pam_access.so
```

Los archivos en `/etc/security/` no hacen nada por sí solos — el módulo PAM correspondiente debe estar activo en el stack del servicio.

---

## 9. Cheatsheet

```bash
# ── Estructura ────────────────────────────────────────────
/etc/pam.d/                          # Un archivo por servicio
/etc/pam.d/other                     # Fallback (debe ser pam_deny.so)
/etc/pam.d/common-auth               # Auth compartido (Debian)
/etc/pam.d/system-auth               # Auth local (RHEL, symlink)
/etc/pam.d/password-auth             # Auth remoto (RHEL, symlink)

# ── Formato de línea ─────────────────────────────────────
# type  control  module.so  [args]
# auth  required pam_unix.so nullok

# ── Archivos de configuración de módulos ──────────────────
/etc/security/limits.conf            # pam_limits.so
/etc/security/access.conf            # pam_access.so
/etc/security/time.conf              # pam_time.so
/etc/security/pwquality.conf         # pam_pwquality.so
/etc/security/faillock.conf          # pam_faillock.so
/etc/security/namespace.conf         # pam_namespace.so
/etc/security/pam_env.conf           # pam_env.so

# ── Debian — pam-auth-update ─────────────────────────────
pam-auth-update                      # Interfaz interactiva
pam-auth-update --enable MODULE      # Habilitar perfil
pam-auth-update --disable MODULE     # Deshabilitar perfil
# Perfiles: /usr/share/pam-configs/

# ── RHEL — authselect ────────────────────────────────────
authselect list                      # Perfiles disponibles
authselect current                   # Perfil actual
authselect select sssd               # Seleccionar perfil
authselect select sssd with-mkhomedir with-faillock
authselect create-profile NAME --base-on sssd
authselect check                     # Verificar integridad

# ── limits.conf ───────────────────────────────────────────
# domain  type  item       value
# *       soft  nofile     1024
# @grupo  hard  nproc      4096
# user    hard  maxlogins  3
# Items: nofile, nproc, cpu, maxlogins, core, memlock, as

# ── access.conf ───────────────────────────────────────────
# +/- : users : origins
# + : root : LOCAL
# - : ALL EXCEPT @admin : ALL
# + : ALL : 192.168.1.0/24

# ── time.conf ─────────────────────────────────────────────
# services ; ttys ; users ; times
# sshd ; * ; ALL ; Wk0800-1800
# Wk=weekdays Wd=weekend Al=all MoTuWeThFrSaSu

# ── pwquality.conf ────────────────────────────────────────
# minlen=12 dcredit=-1 ucredit=-1 lcredit=-1 ocredit=-1
# maxrepeat=3 difok=5 reject_username enforce_for_root

# ── Verificar qué módulos usa un servicio ─────────────────
cat /etc/pam.d/sshd                  # Config directa
grep -r "include\|substack" /etc/pam.d/sshd  # Includes
```

---

## 10. Ejercicios

### Ejercicio 1 — Analizar la configuración PAM de sshd

Traza todos los archivos incluidos y lista los módulos que se ejecutan en el stack `auth`.

```bash
# 1. Ver el archivo principal
cat /etc/pam.d/sshd | grep auth

# 2. Seguir los includes
# Si aparece: @include common-auth
cat /etc/pam.d/common-auth

# 3. Construir la secuencia completa
# Ejemplo resultado (Debian con SSSD):

# Desde /etc/pam.d/sshd:
# (no tiene reglas auth propias, solo include)

# Desde common-auth:
# auth [success=2 default=ignore] pam_unix.so nullok
# auth [success=1 default=ignore] pam_sss.so use_first_pass
# auth requisite                  pam_deny.so
# auth required                   pam_permit.so
```

**Pregunta de predicción:** Si quieres agregar `pam_faillock.so` solo para sshd (no para login ni sudo), ¿dónde lo agregas: en `common-auth` o en `/etc/pam.d/sshd`?

> **Respuesta:** En `/etc/pam.d/sshd`, **antes** de la línea `@include common-auth`. Si lo agregas en `common-auth`, afectará a todos los servicios que lo incluyen (login, su, sudo, etc.), no solo a sshd. Agregar reglas específicas por servicio en su archivo propio y dejar `common-*` para la política base compartida.

---

### Ejercicio 2 — Configurar límites de recursos

Configura `pam_limits.so` para que el grupo `developers` pueda abrir más archivos y los usuarios del grupo `students` tengan CPU limitada.

```bash
# 1. Editar limits.conf
sudo tee -a /etc/security/limits.conf << 'EOF'
# Developers — más archivos abiertos
@developers    soft    nofile    4096
@developers    hard    nofile    65536

# Students — CPU limitada, sin core dumps
@students      hard    cpu       30
@students      hard    nproc     100
@students      hard    core      0
@students      hard    maxlogins 2
EOF

# 2. Verificar que pam_limits.so está en el stack session
grep pam_limits /etc/pam.d/common-session
# session required pam_limits.so   ← debe existir

# 3. Probar (como usuario del grupo developers)
su - dev_user -c 'ulimit -n'
# 4096 (soft limit)
su - dev_user -c 'ulimit -Hn'
# 65536 (hard limit)
```

**Pregunta de predicción:** Si un usuario pertenece a `developers` Y a `students`, ¿qué límite de `nofile` se aplica? ¿Y de `cpu`?

> **Respuesta:** Se aplica el **último match** en el archivo. Si la línea de `@developers` está después de `@students`, el `nofile` será 4096/65536 (de developers). El `cpu` será 30 (de students, porque developers no define `cpu`). Cada `item` se resuelve independientemente — la membresía en un grupo no anula las reglas del otro grupo, sino que se evalúa línea por línea y el último match para cada item gana.

---

### Ejercicio 3 — Restringir acceso SSH por red y horario

Configura PAM para que solo el grupo `admin` pueda hacer SSH desde fuera de la LAN, y solo en horario laboral para el resto.

```bash
# 1. Configurar access.conf
sudo tee /etc/security/access.conf << 'EOF'
# Admins pueden acceder desde cualquier parte
+ : @admin : ALL

# Empleados solo desde la LAN
+ : ALL : 192.168.1.0/24

# Denegar el resto
- : ALL : ALL
EOF

# 2. Configurar time.conf
sudo tee -a /etc/security/time.conf << 'EOF'
# SSH solo en horario laboral para no-admins
sshd ; * ; !@admin ; Wk0800-1800
EOF

# 3. Agregar módulos al stack PAM de sshd
# /etc/pam.d/sshd — agregar ANTES de los includes:
# account required pam_access.so
# account required pam_time.so
```

```bash
# 4. Probar
# Desde 192.168.1.50 como usuario normal, lunes 10:00 → OK
# Desde 10.0.0.5 como usuario normal → DENIED (access.conf)
# Desde 10.0.0.5 como admin → OK
# Como usuario normal, domingo 22:00 → DENIED (time.conf)
```

**Pregunta de predicción:** Si agregas `account required pam_access.so` en `common-account` en lugar de en `/etc/pam.d/sshd`, ¿qué otros servicios se verán afectados?

> **Respuesta:** **Todos** los servicios que incluyen `common-account`: login en consola, su, sudo, cron, screensavers, etc. Las reglas de `access.conf` bloquearán acceso local también si no coincide con la LAN (por ejemplo, `su` desde la consola del servidor podría fallar si la regla final es `- : ALL : ALL` y el acceso local no está explícitamente permitido). Para evitar esto, habría que agregar `+ : ALL : LOCAL` antes del deny final, o simplemente configurar `pam_access.so` solo en `/etc/pam.d/sshd`.

---

> **Siguiente:** T03 — Módulos comunes (pam_unix, pam_deny, pam_limits, pam_wheel, pam_faillock).
