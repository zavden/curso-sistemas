# Módulos comunes de PAM

## Índice
1. [pam_unix — autenticación tradicional](#1-pam_unix--autenticación-tradicional)
2. [pam_deny y pam_permit — control absoluto](#2-pam_deny-y-pam_permit--control-absoluto)
3. [pam_limits — límites de recursos](#3-pam_limits--límites-de-recursos)
4. [pam_wheel — restringir su](#4-pam_wheel--restringir-su)
5. [pam_faillock — bloqueo por intentos fallidos](#5-pam_faillock--bloqueo-por-intentos-fallidos)
6. [Otros módulos importantes](#6-otros-módulos-importantes)
7. [Errores comunes](#7-errores-comunes)
8. [Cheatsheet](#8-cheatsheet)
9. [Ejercicios](#9-ejercicios)

---

## 1. pam_unix — autenticación tradicional

`pam_unix` es el módulo fundamental de PAM. Maneja autenticación contra `/etc/shadow`, verificación de cuentas, cambio de contraseñas y registro de sesiones. Prácticamente todas las configuraciones PAM lo incluyen.

### Funciones por stack

```
┌──────────────────────────────────────────────────────────┐
│              pam_unix.so — funciones por stack             │
│                                                          │
│  auth:                                                   │
│    Verifica la contraseña contra /etc/shadow              │
│    Soporta NIS si está configurado                       │
│                                                          │
│  account:                                                │
│    Verifica expiración de cuenta (chage)                 │
│    Verifica expiración de contraseña                     │
│    Verifica si la cuenta está bloqueada                  │
│                                                          │
│  password:                                               │
│    Actualiza la contraseña en /etc/shadow                │
│    Aplica el algoritmo de hash seleccionado              │
│                                                          │
│  session:                                                │
│    Registra login/logout en utmp/wtmp/btmp               │
│    (último login, historial de sesiones)                 │
└──────────────────────────────────────────────────────────┘
```

### Argumentos principales

```bash
# auth
auth sufficient pam_unix.so nullok try_first_pass
```

| Argumento | Efecto |
|-----------|--------|
| `nullok` | Permitir login con contraseña vacía (campo vacío en shadow) |
| `try_first_pass` | Usar contraseña ya introducida; si falla, pedir otra |
| `use_first_pass` | Usar contraseña ya introducida; si falla, no pedir otra |
| `nodelay` | No agregar delay tras fallo de autenticación |
| `audit` | Registrar el username en logs (incluso si no existe) |

```bash
# password
password sufficient pam_unix.so sha512 shadow use_authtok remember=5
```

| Argumento | Efecto |
|-----------|--------|
| `sha512` | Usar SHA-512 para el hash (recomendado) |
| `yescrypt` | Usar yescrypt (más moderno, Debian 12+) |
| `shadow` | Actualizar /etc/shadow (no /etc/passwd) |
| `use_authtok` | Usar la contraseña establecida por un módulo previo (ej: pwquality) |
| `remember=N` | Recordar las últimas N contraseñas (impedir reutilización) |
| `rounds=N` | Número de iteraciones del hash (más lento = más seguro) |
| `minlen=N` | Longitud mínima (mejor usar pam_pwquality) |

### Historial de contraseñas

```bash
# Impedir reutilizar las últimas 5 contraseñas
password sufficient pam_unix.so sha512 shadow use_authtok remember=5
```

Las contraseñas anteriores se almacenan en `/etc/security/opasswd`:

```bash
# Ver el archivo (solo root)
sudo cat /etc/security/opasswd
# juan:1000:5:$6$rounds=...hash1..,$6$...hash2...

# Si el usuario intenta reutilizar:
# "Password has been already used. Choose another."
```

### Algoritmos de hash

```bash
# Ver qué algoritmo usa el sistema
grep '^ENCRYPT_METHOD' /etc/login.defs
# ENCRYPT_METHOD SHA512

# O en sistemas modernos:
grep -E '^(SHA|YESCRYPT)' /etc/login.defs
```

| Algoritmo | Prefijo en shadow | Seguridad |
|-----------|-------------------|-----------|
| DES | (sin prefijo) | Obsoleto, 8 chars máx |
| MD5 | `$1$` | Débil |
| SHA-256 | `$5$` | Aceptable |
| SHA-512 | `$6$` | Bueno |
| yescrypt | `$y$` | Mejor (memory-hard) |

---

## 2. pam_deny y pam_permit — control absoluto

### pam_deny.so

Siempre retorna **FAILURE**. Se usa como red de seguridad al final de un stack o para bloquear completamente un servicio.

```bash
# Red de seguridad — si ningún sufficient tuvo éxito, denegar
auth sufficient pam_unix.so
auth sufficient pam_sss.so use_first_pass
auth required   pam_deny.so         # ← Nadie llega aquí si alguno pasó

# Bloquear completamente un servicio
# /etc/pam.d/other
auth     required pam_deny.so
account  required pam_deny.so
password required pam_deny.so
session  required pam_deny.so
```

### pam_permit.so

Siempre retorna **SUCCESS**. Se usa como contraparte de `pam_deny.so` en la lógica de saltos `[success=N]`.

```bash
# Patrón Debian common-auth:
auth [success=2 default=ignore] pam_unix.so     # Si OK → salta 2
auth [success=1 default=ignore] pam_sss.so      # Si OK → salta 1
auth requisite                  pam_deny.so      # Aquí si ambos fallan
auth required                   pam_permit.so    # ← Destino de los saltos
```

`pam_permit.so` como required al final del stack:
- Si un módulo anterior saltó aquí → SUCCESS completa el stack
- `pam_deny.so` antes impide que se llegue sin un salto exitoso

> **Nunca** usar `pam_permit.so` solo en un stack auth — permitiría login sin autenticación.

---

## 3. pam_limits — límites de recursos

Aplica límites definidos en `/etc/security/limits.conf` y `/etc/security/limits.d/*.conf` al inicio de sesión.

### Activación

```bash
# Debe estar en el stack session del servicio
session required pam_limits.so

# Ya incluido por defecto en common-session (Debian)
# y system-auth (RHEL)
```

### Configuración de limits.conf

```bash
# /etc/security/limits.conf
# <domain>  <type>  <item>  <value>

# ── Archivos abiertos ────────────────────────────────────
*              soft    nofile     1024
*              hard    nofile     65536
@webservers    soft    nofile     16384
@webservers    hard    nofile     65536

# ── Procesos ──────────────────────────────────────────────
*              soft    nproc      4096
*              hard    nproc      16384
@fork_bombers  hard    nproc      50        # Limitar fork bombs

# ── Memoria ───────────────────────────────────────────────
@databases     soft    memlock    unlimited  # Para hugepages
@databases     hard    memlock    unlimited

# ── Core dumps ────────────────────────────────────────────
*              hard    core       0          # Deshabilitar (seguridad)
@developers    soft    core       unlimited  # Permitir para debug

# ── Sesiones simultáneas ──────────────────────────────────
*              hard    maxlogins  5
root           hard    maxlogins  2

# ── Tamaño de archivos ───────────────────────────────────
@students      hard    fsize      102400     # 100 MB max file size
```

### Items disponibles

| Item | Significado | Unidad |
|------|------------|--------|
| `nofile` | Archivos abiertos | Número |
| `nproc` | Procesos | Número |
| `core` | Tamaño core dump | KB (0=deshabilitar) |
| `memlock` | Memoria locked | KB |
| `maxlogins` | Sesiones simultáneas | Número |
| `maxsyslogins` | Sesiones totales del sistema | Número |
| `fsize` | Tamaño máximo de archivo | KB |
| `cpu` | Tiempo CPU | Minutos |
| `as` | Address space (memoria virtual) | KB |
| `stack` | Tamaño del stack | KB |
| `priority` | Nice priority | -20 a 19 |
| `locks` | File locks | Número |
| `sigpending` | Señales pendientes | Número |
| `msgqueue` | Memoria POSIX message queues | Bytes |
| `rtprio` | Real-time priority | 0-99 |

### limits.d/ — archivos drop-in

```bash
# /etc/security/limits.d/90-custom.conf
# Los archivos se procesan en orden alfabético
# DESPUÉS de limits.conf
# El último match gana

@nginx    soft    nofile    32768
@nginx    hard    nofile    65536
```

### Verificar límites aplicados

```bash
# Ver los límites del proceso actual
ulimit -a

# Ver un límite específico
ulimit -n     # nofile (soft)
ulimit -Hn    # nofile (hard)
ulimit -u     # nproc

# Ver los límites de un proceso en ejecución
cat /proc/$(pidof nginx)/limits

# Resultado:
# Max open files    32768    65536    files
# Max processes     4096     16384    processes
```

---

## 4. pam_wheel — restringir su

`pam_wheel` restringe el uso de `su` a los miembros de un grupo específico (por defecto `wheel`).

### Configuración

```bash
# /etc/pam.d/su

# Descomentar esta línea para activar la restricción:
auth required pam_wheel.so
# Solo los usuarios del grupo "wheel" pueden usar su

# Con opciones:
auth required pam_wheel.so group=admins
# Usar grupo "admins" en vez de "wheel"

auth required pam_wheel.so deny group=restricted
# Denegar a los usuarios del grupo "restricted" (invertir lógica)

auth required pam_wheel.so root_only
# Solo restringir su hacia root (su a otros usuarios permitido)

auth required pam_wheel.so trust
# Miembros de wheel pueden hacer su SIN contraseña
```

### Activar la restricción

```bash
# 1. Editar /etc/pam.d/su
sudo vim /etc/pam.d/su
# Descomentar: auth required pam_wheel.so

# 2. Agregar usuarios al grupo wheel
sudo usermod -aG wheel juan

# 3. Probar
su -               # Como usuario NO en wheel → falla
# su: Permission denied

su -               # Como usuario en wheel → pide contraseña de root
# Password: ****
```

### Por qué restringir su

```
┌──────────────────────────────────────────────────────────┐
│          ¿Por qué restringir su?                          │
│                                                          │
│  Sin pam_wheel:                                          │
│    Cualquier usuario que conozca la contraseña de root   │
│    puede ejecutar su - y obtener shell de root.          │
│    Si la contraseña se filtra → todos los usuarios       │
│    tienen acceso root.                                   │
│                                                          │
│  Con pam_wheel:                                          │
│    Incluso si un usuario conoce la contraseña de root,   │
│    debe ADEMÁS estar en el grupo wheel.                  │
│    Defensa en profundidad: contraseña + membresía.       │
│                                                          │
│  En la práctica:                                         │
│    RHEL habilita pam_wheel por defecto.                  │
│    Debian lo tiene comentado por defecto.                │
│    Muchos entornos prefieren sudo sobre su.              │
└──────────────────────────────────────────────────────────┘
```

---

## 5. pam_faillock — bloqueo por intentos fallidos

`pam_faillock` (reemplazo moderno de `pam_tally2`) bloquea cuentas después de un número configurable de intentos de autenticación fallidos.

### Configuración

```bash
# /etc/security/faillock.conf
deny = 5               # Bloquear tras 5 intentos fallidos
unlock_time = 900       # Desbloquear automáticamente tras 15 minutos (0=nunca)
fail_interval = 900     # Ventana de tiempo para contar intentos (15 min)
even_deny_root = false  # No bloquear a root (true para máxima seguridad)
root_unlock_time = 60   # Si se bloquea root, desbloquear en 60 seg
audit                   # Registrar intentos en audit log
silent                  # No informar al usuario que la cuenta está bloqueada
```

### Integración con PAM

```bash
# /etc/pam.d/sshd (o system-auth/common-auth)

# ANTES de la autenticación — registrar intento
auth required pam_faillock.so preauth

# Autenticación normal
auth sufficient pam_unix.so nullok

# DESPUÉS de la autenticación — registrar resultado
auth [default=die] pam_faillock.so authfail

# En account:
account required pam_faillock.so
```

```
┌──────────────────────────────────────────────────────────┐
│          Flujo de pam_faillock                            │
│                                                          │
│  1. preauth: ¿La cuenta está bloqueada?                  │
│     ├── Sí → FAIL inmediato (ni pide contraseña)         │
│     └── No → continúa                                    │
│                                                          │
│  2. pam_unix: ¿Contraseña correcta?                      │
│     ├── Sí → sufficient, stack pasa                      │
│     └── No → continúa                                    │
│                                                          │
│  3. authfail: Registrar intento fallido                  │
│     Incrementar contador en /var/run/faillock/           │
│     ¿Contador >= deny (5)?                               │
│     ├── Sí → bloquear cuenta                             │
│     └── No → stack falla (contraseña incorrecta)         │
│                                                          │
│  Tras unlock_time → contador se reinicia                 │
│  Tras login exitoso → contador se reinicia               │
└──────────────────────────────────────────────────────────┘
```

### Gestionar cuentas bloqueadas

```bash
# Ver estado de una cuenta
faillock --user juan
```

```
juan:
When                Type  Source              Valid
2026-03-21 10:00:01 RHOST 192.168.1.100      V
2026-03-21 10:00:05 RHOST 192.168.1.100      V
2026-03-21 10:00:09 RHOST 192.168.1.100      V
2026-03-21 10:00:13 RHOST 192.168.1.100      V
2026-03-21 10:00:17 RHOST 192.168.1.100      V
```

```bash
# Desbloquear una cuenta manualmente
sudo faillock --user juan --reset

# Ver todas las cuentas con fallos
sudo faillock

# Directorio de datos
ls /var/run/faillock/
# juan  maria  admin
```

### RHEL con authselect

```bash
# Habilitar faillock en RHEL
sudo authselect select sssd with-faillock

# Configurar parámetros en /etc/security/faillock.conf
# authselect agrega automáticamente las líneas de PAM
```

---

## 6. Otros módulos importantes

### pam_nologin — bloquear login no-root

```bash
# Si existe /etc/nologin, solo root puede hacer login
# Los demás ven el contenido del archivo como mensaje
auth requisite pam_nologin.so
```

```bash
# Crear bloqueo temporal (mantenimiento)
echo "Sistema en mantenimiento. Volver a las 18:00." | sudo tee /etc/nologin

# Eliminar bloqueo
sudo rm /etc/nologin
```

### pam_securetty — restringir terminales de root

```bash
# Solo permite login de root en TTYs listadas en /etc/securetty
auth required pam_securetty.so
```

```bash
# /etc/securetty
tty1
tty2
# Solo root puede hacer login directo en tty1 y tty2
# SSH no se ve afectado (usa pseudo-TTYs pts/*)
```

> En distribuciones modernas, `/etc/securetty` a veces está vacío o no existe, lo que bloquea login root en todas las TTYs (forzando el uso de sudo).

### pam_mkhomedir — crear home en primer login

```bash
# Crea el directorio home automáticamente si no existe
# Esencial para usuarios LDAP/SSSD que no tienen home preexistente
session required pam_mkhomedir.so skel=/etc/skel umask=0077
```

| Argumento | Efecto |
|-----------|--------|
| `skel=/etc/skel` | Directorio skeleton (archivos iniciales) |
| `umask=0077` | Permisos del home creado (700) |

```bash
# Habilitar en Debian
sudo pam-auth-update --enable mkhomedir

# Habilitar en RHEL
sudo authselect select sssd with-mkhomedir
```

### pam_access — control de acceso por origen

```bash
# Permite/deniega acceso basándose en usuario + origen
account required pam_access.so
# Configuración en /etc/security/access.conf
```

### pam_time — restricciones horarias

```bash
# Permite/deniega acceso basándose en horario
account required pam_time.so
# Configuración en /etc/security/time.conf
```

### pam_pwquality — calidad de contraseñas

```bash
# Verifica que la nueva contraseña cumple políticas de complejidad
password requisite pam_pwquality.so retry=3 minlen=12
# Configuración en /etc/security/pwquality.conf
```

```bash
# Probar políticas interactivamente
# (intenta cambiar la contraseña y observa los rechazos)
passwd
# New password: abc
# BAD PASSWORD: The password is shorter than 12 characters
# New password: abcdefghijkl
# BAD PASSWORD: The password fails the dictionary check
```

### pam_env — variables de entorno

```bash
# Establece variables de entorno al iniciar sesión
session required pam_env.so readenv=1
session required pam_env.so readenv=1 envfile=/etc/default/locale
```

```bash
# /etc/security/pam_env.conf
# VARIABLE   [DEFAULT=valor]  [OVERRIDE=valor]
EDITOR       DEFAULT=vim
PAGER        DEFAULT=less
TZ           DEFAULT=Europe/Madrid
```

### pam_motd — mensaje del día

```bash
# Muestra el mensaje del día al iniciar sesión
session optional pam_motd.so motd=/run/motd.dynamic
session optional pam_motd.so noupdate
```

### pam_warn — registrar en syslog

```bash
# Registra información en syslog (útil para debug)
auth optional pam_warn.so
# Log: pam_warn(sshd:auth): function=[pam_sm_authenticate]
#      service=[sshd] terminal=[ssh] user=[juan] ruser=[<unknown>]
```

### pam_exec — ejecutar un script

```bash
# Ejecutar un comando externo durante la autenticación
session optional pam_exec.so /usr/local/bin/on-login.sh
```

---

## 7. Errores comunes

### Error 1 — faillock bloquea a root y nadie puede desbloquear

```bash
# Con even_deny_root = true y sin acceso físico:
# root se bloquea → nadie puede hacer faillock --reset

# Prevención 1: no bloquear root
# /etc/security/faillock.conf
even_deny_root = false

# Prevención 2: root_unlock_time corto
even_deny_root = true
root_unlock_time = 60    # Se desbloquea solo en 1 minuto

# Recuperación: arrancar en single/rescue mode
# mount -o remount,rw /
# faillock --user root --reset
```

### Error 2 — pam_limits no aplica a servicios systemd

```bash
# pam_limits.so funciona para sesiones PAM (login, SSH, su)
# pero NO para servicios systemd (nginx, postgresql, etc.)

# Los servicios systemd usan directivas propias:
# /etc/systemd/system/nginx.service.d/limits.conf
# [Service]
# LimitNOFILE=65536
# LimitNPROC=4096

sudo systemctl daemon-reload
sudo systemctl restart nginx

# Verificar
cat /proc/$(pidof nginx)/limits
```

### Error 3 — pam_wheel en su pero no en sudo

```bash
# pam_wheel solo afecta a "su"
# Si bloqueas su con pam_wheel pero sudo no está restringido,
# los usuarios simplemente usan sudo su:

sudo su -    # ← Ignora pam_wheel porque es PAM de sudo, no de su

# Solución: restringir sudo por grupo en /etc/sudoers
# %admin ALL=(ALL:ALL) ALL
# Solo el grupo admin puede usar sudo
```

### Error 4 — nullok permite login sin contraseña

```bash
# auth sufficient pam_unix.so nullok
#                                ↑
# Si un usuario tiene el campo de contraseña vacío en /etc/shadow:
# juan::19437:0:99999:7:::
#       ↑ campo vacío = sin contraseña

# Con nullok → login sin contraseña PERMITIDO
# Sin nullok → login sin contraseña DENEGADO

# En la mayoría de entornos, quitar nullok:
auth sufficient pam_unix.so try_first_pass

# Excepción: sistemas donde algunos usuarios usan solo claves SSH
# y nunca necesitan contraseña
```

### Error 5 — remember en pam_unix sin crear opasswd

```bash
# remember=5 necesita /etc/security/opasswd con permisos correctos
# Si no existe, el módulo falla silenciosamente

# Crear el archivo:
sudo touch /etc/security/opasswd
sudo chown root:root /etc/security/opasswd
sudo chmod 600 /etc/security/opasswd
```

---

## 8. Cheatsheet

```bash
# ── pam_unix.so ───────────────────────────────────────────
# auth:     Verifica contra /etc/shadow
#   nullok           Permitir password vacío
#   try_first_pass   Reutilizar password, si falla pedir otra
#   use_first_pass   Reutilizar password, si falla fallar
# password: Actualiza /etc/shadow
#   sha512 / yescrypt   Algoritmo de hash
#   shadow              Usar /etc/shadow
#   use_authtok         Usar password del módulo anterior
#   remember=N          Impedir reutilizar últimas N passwords
# session:  Registra login en utmp/wtmp

# ── pam_deny.so / pam_permit.so ──────────────────────────
# pam_deny.so   → Siempre FAIL (red de seguridad)
# pam_permit.so → Siempre SUCCESS (destino de saltos)
# NUNCA usar pam_permit solo en auth

# ── pam_limits.so ─────────────────────────────────────────
# session required pam_limits.so
# Config: /etc/security/limits.conf, /etc/security/limits.d/
# Items: nofile, nproc, core, memlock, maxlogins, fsize, cpu
# Verificar: ulimit -a, cat /proc/PID/limits
# NO aplica a servicios systemd (usar LimitNOFILE= en unit)

# ── pam_wheel.so ──────────────────────────────────────────
# auth required pam_wheel.so              # Solo grupo wheel usa su
# auth required pam_wheel.so group=admins  # Grupo personalizado
# auth required pam_wheel.so trust         # Sin contraseña para wheel
# auth required pam_wheel.so root_only     # Solo para su a root
# Activar: descomentar en /etc/pam.d/su
# Agregar usuario: usermod -aG wheel usuario

# ── pam_faillock.so ───────────────────────────────────────
# Config: /etc/security/faillock.conf
#   deny=5  unlock_time=900  fail_interval=900
#   even_deny_root=false  root_unlock_time=60
# PAM:
#   auth required   pam_faillock.so preauth    (antes de auth)
#   auth [default=die] pam_faillock.so authfail (después de auth)
#   account required pam_faillock.so
# Gestión:
faillock --user USER                 # Ver estado
faillock --user USER --reset         # Desbloquear
# RHEL: authselect select sssd with-faillock

# ── Otros módulos ─────────────────────────────────────────
# pam_nologin.so    → Bloquear login si existe /etc/nologin
# pam_securetty.so  → Restringir TTYs de root (/etc/securetty)
# pam_mkhomedir.so  → Crear home en primer login (LDAP)
# pam_access.so     → Control por origen (/etc/security/access.conf)
# pam_time.so       → Control por horario (/etc/security/time.conf)
# pam_pwquality.so  → Calidad de contraseñas (pwquality.conf)
# pam_env.so        → Variables de entorno (pam_env.conf)
# pam_motd.so       → Mensaje del día
# pam_warn.so       → Registrar en syslog (debug)
# pam_exec.so       → Ejecutar script externo
```

---

## 9. Ejercicios

### Ejercicio 1 — Configurar bloqueo de cuentas con faillock

Configura el sistema para bloquear cuentas tras 3 intentos fallidos en 10 minutos, con desbloqueo automático a los 5 minutos. Root no debe bloquearse.

```bash
# 1. Configurar faillock.conf
sudo tee /etc/security/faillock.conf << 'EOF'
deny = 3
unlock_time = 300
fail_interval = 600
even_deny_root = false
audit
silent
EOF

# 2. Agregar a PAM (si no está — Debian)
# /etc/pam.d/common-auth — agregar ANTES de pam_unix:
# auth required pam_faillock.so preauth
# (después de pam_unix):
# auth [default=die] pam_faillock.so authfail

# /etc/pam.d/common-account — agregar:
# account required pam_faillock.so

# 3. En RHEL, usar authselect:
# sudo authselect select sssd with-faillock

# 4. Probar (intentar 3 veces con contraseña incorrecta)
su - testuser
# Password: (incorrecta) x3
# su: Permission denied

# 5. Verificar
faillock --user testuser
# Muestra 3 intentos
```

**Pregunta de predicción:** Un usuario se bloquea a las 10:00 tras 3 intentos fallidos. A las 10:04 intenta con la contraseña correcta. ¿Puede entrar?

> **Respuesta:** No. `unlock_time = 300` (5 minutos) significa que la cuenta permanece bloqueada hasta las 10:05. `pam_faillock.so preauth` verifica el bloqueo **antes** de pedir contraseña — ni siquiera llega a verificarla. A las 10:05 (o después), el módulo ve que `unlock_time` ha pasado, reinicia el contador, y permite el intento de autenticación.

---

### Ejercicio 2 — Restringir su al grupo wheel

Configura el sistema para que solo los miembros del grupo `wheel` puedan usar `su`, y verifica que sudo no se ve afectado.

```bash
# 1. Crear grupo wheel si no existe (Debian)
sudo groupadd -f wheel

# 2. Agregar un usuario al grupo
sudo usermod -aG wheel admin_user

# 3. Activar pam_wheel en /etc/pam.d/su
# Descomentar o agregar después de pam_rootok:
# auth required pam_wheel.so

# 4. Probar
su - testuser            # Como usuario NO en wheel
# su: Permission denied  # ← Denegado (correcto)

su - admin_user          # Como usuario en wheel
# su: Password:          # ← Pide contraseña (correcto)

# 5. Verificar que sudo no se ve afectado
sudo -l                  # Como usuario en sudoers pero NO en wheel
# → Funciona (pam_wheel solo afecta a su)
```

**Pregunta de predicción:** Si configuras `auth required pam_wheel.so trust` y un usuario del grupo `wheel` ejecuta `su -`, ¿necesita la contraseña de root?

> **Respuesta:** No. `trust` hace que `pam_wheel.so` retorne SUCCESS para miembros de wheel, y como está marcado con `required`, el éxito contribuye al resultado. Pero la clave es que en `/etc/pam.d/su` típicamente hay `auth sufficient pam_rootok.so` seguido de `auth required pam_wheel.so trust`. Si `pam_wheel.so trust` tiene éxito y se combina con la línea estándar de autenticación de contraseña, el usuario sigue necesitando la contraseña de root. Para evitar la contraseña, `pam_wheel.so` debería ser `sufficient`, no `required`: `auth sufficient pam_wheel.so trust group=wheel` — así se salta los módulos restantes (incluyendo pam_unix).

---

### Ejercicio 3 — Combinar módulos para una política de seguridad

Diseña la configuración PAM de `/etc/pam.d/sshd` que implemente esta política:
- Bloquear tras 5 intentos fallidos (15 min de bloqueo)
- Autenticar contra local y LDAP
- Crear home automáticamente en primer login
- Limitar a 3 sesiones SSH simultáneas
- Registrar todos los intentos en syslog

```bash
# /etc/pam.d/sshd

# ── auth ──────────────────────────────────────────────────
auth     optional     pam_warn.so                          # Log en syslog
auth     required     pam_faillock.so preauth              # Verificar bloqueo
auth     required     pam_env.so                           # Entorno
auth     [success=2 default=ignore] pam_unix.so nullok     # Local
auth     [success=1 default=ignore] pam_sss.so use_first_pass  # LDAP
auth     requisite    pam_deny.so                          # Si ambos fallan
auth     required     pam_permit.so                        # Destino de saltos
auth     [default=die] pam_faillock.so authfail            # Registrar fallo

# ── account ───────────────────────────────────────────────
account  required     pam_faillock.so                      # Verificar bloqueo
account  required     pam_nologin.so                       # /etc/nologin
account  required     pam_unix.so
account  [default=bad success=ok user_unknown=ignore] pam_sss.so

# ── password ──────────────────────────────────────────────
password requisite    pam_pwquality.so retry=3 minlen=12
password sufficient   pam_unix.so sha512 shadow use_authtok remember=5
password sufficient   pam_sss.so use_authtok
password required     pam_deny.so

# ── session ───────────────────────────────────────────────
session  required     pam_limits.so                        # Límites
session  required     pam_unix.so                          # Registrar sesión
session  optional     pam_sss.so
session  optional     pam_mkhomedir.so skel=/etc/skel umask=0077  # Crear home
session  optional     pam_motd.so motd=/run/motd.dynamic
session  optional     pam_systemd.so
```

```bash
# /etc/security/limits.conf — limitar sesiones SSH
*    hard    maxlogins    3
```

**Pregunta de predicción:** Si un usuario LDAP hace login por primera vez y `pam_mkhomedir.so` está como `optional`, ¿qué pasa si la creación del home falla (por ejemplo, disco lleno)? ¿Puede hacer login?

> **Respuesta:** Sí, puede hacer login. Como `pam_mkhomedir.so` es `optional`, su fallo se ignora en el resultado del stack session. El usuario entra pero sin directorio home — su `$HOME` apuntará a un directorio que no existe, y los comandos que escriban en `~/` fallarán. Si fuera `required`, el fallo de creación del home bloquearía el login completamente. `optional` es la elección correcta: es mejor entrar sin home que no poder entrar en absoluto.

---

> **Fin de la Sección 3 — PAM.** Siguiente sección: S04 — LDAP Client (T01 — Conceptos de LDAP: directorio, DN, base DN, LDIF, esquema, operaciones).
