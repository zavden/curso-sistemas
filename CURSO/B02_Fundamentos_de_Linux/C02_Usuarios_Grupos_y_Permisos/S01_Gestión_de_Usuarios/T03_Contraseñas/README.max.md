# T03 — Contraseñas (Enhanced)

## Gestión de contraseñas con passwd

`passwd` es la herramienta principal para establecer y modificar contraseñas:

```bash
# Cambiar tu propia contraseña (no requiere sudo)
passwd
# Changing password for dev.
# Current password:
# New password:
# Retype new password:
# passwd: password updated successfully

# Cambiar la contraseña de otro usuario (requiere root)
sudo passwd testuser
# New password:
# Retype new password:
# passwd: password updated successfully
```

### Opciones de passwd

| Flag | Descripción |
|---|---|
| `passwd` | Cambiar tu propia contraseña |
| `passwd -l user` | Bloquear contraseña (antepone `!` al hash) |
| `passwd -u user` | Desbloquear contraseña (quita el `!`) |
| `passwd -d user` | Eliminar contraseña (login sin password — inseguro) |
| `passwd -e user` | Forzar cambio en el próximo login |
| `passwd -S user` | Ver estado de la contraseña |
| `passwd -n days user` | Mínimo de días entre cambios |
| `passwd -x days user` | Máximo de días antes de expiración |
| `passwd -w days user` | Días de aviso antes de expiración |

```bash
# Ver estado de la contraseña
sudo passwd -S dev
# dev P 03/10/2025 0 99999 7 -1
#  ^  ^     ^      ^   ^   ^  ^
#  |  |     |      |   |   |  └── inactive (-1 = no)
#  |  |     |      |   |   └── warn (7 días)
#  |  |     |      |   └── max (99999 = nunca)
#  |  |     |      └── min (0 = puede cambiar inmediatamente)
#  |  |     └── último cambio
#  |  └── estado: P=password set, L=locked, NP=no password
#  └── usuario
```

### Forzar cambio de contraseña al próximo login

```bash
# Método 1: passwd
sudo passwd -e dev
# passwd: password expiry information changed.

# Método 2: chage
sudo chage -d 0 dev

# En el próximo login:
# WARNING: Your password has expired.
# You must change your password now and login again!
# Changing password for dev.
# Current password:
# New password:
```

## chage — Políticas de expiración

`chage` (change age) gestiona las políticas de envejecimiento de contraseñas
definidas en `/etc/shadow`:

```bash
# Ver toda la información de expiración
sudo chage -l dev
# Last password change                : Mar 10, 2025
# Password expires                    : never
# Password inactive                   : never
# Account expires                     : never
# Minimum number of days between password change : 0
# Maximum number of days between password change : 99999
# Number of days of warning before password expires : 7
```

### Opciones de chage

| Flag | Descripción | Ejemplo |
|---|---|---|
| `-l user` | Listar información de expiración | `chage -l dev` |
| `-d YYYY-MM-DD` | Fecha del último cambio de contraseña | `chage -d 2025-01-01 dev` |
| `-d 0` | Forzar cambio en el próximo login | `chage -d 0 dev` |
| `-m N` | Mínimo de días entre cambios | `chage -m 1 dev` |
| `-M N` | Máximo de días antes de expiración | `chage -M 90 dev` |
| `-W N` | Días de aviso antes de expiración | `chage -W 14 dev` |
| `-I N` | Días de gracia después de expiración | `chage -I 30 dev` |
| `-E YYYY-MM-DD` | Fecha de expiración de la cuenta | `chage -E 2025-12-31 dev` |
| `-E -1` | Quitar expiración de la cuenta | `chage -E -1 dev` |

### Ejemplo: política de empresa típica

```bash
# Contraseña expira cada 90 días
sudo chage -M 90 dev

# No se puede cambiar más de una vez al día (evita rotar y volver a la anterior)
sudo chage -m 1 dev

# Aviso 14 días antes
sudo chage -W 14 dev

# 30 días de gracia después de expirar (puede hacer login pero DEBE cambiar)
sudo chage -I 30 dev

# Verificar
sudo chage -l dev
# Password expires                    : Jun 08, 2025
# Password inactive                   : Jul 08, 2025
# Minimum number of days between password change : 1
# Maximum number of days between password change : 90
# Number of days of warning before password expires : 14
```

### Diferencia entre expiración de contraseña y de cuenta

```bash
# Expiración de CONTRASEÑA — el usuario debe cambiarla, pero la cuenta sigue activa
sudo chage -M 90 dev

# Expiración de CUENTA — la cuenta se desactiva completamente en esa fecha
sudo chage -E 2025-12-31 dev
```

La expiración de cuenta es útil para cuentas temporales (contratistas,
becarios).

## Algoritmos de hash

### Qué algoritmo usa tu sistema

El algoritmo se configura en `/etc/login.defs` (para `useradd`/`passwd`) y en
la configuración de PAM:

```bash
# Ver la configuración
grep ENCRYPT_METHOD /etc/login.defs
# ENCRYPT_METHOD SHA512       [RHEL 9]
# ENCRYPT_METHOD YESCRYPT     [Debian 12]
```

### Comparación de algoritmos

| Algoritmo | Prefijo | Seguridad | Velocidad | Uso |
|---|---|---|---|---|
| MD5 | `$1$` | Inseguro | Muy rápido | Obsoleto, no usar |
| SHA-256 | `$5$` | Aceptable | Rápido | Legacy |
| SHA-512 | `$6$` | Bueno | Rápido | Default RHEL 9, estándar actual |
| yescrypt | `$y$` | Muy bueno | Lento (diseñado así) | Default Debian 12+, Fedora 35+ |
| bcrypt | `$2b$` | Muy bueno | Lento (diseñado así) | BSDs, algunos Linux |

Los algoritmos modernos (yescrypt, bcrypt) son **intencionalmente lentos** —
esto hace que los ataques de fuerza bruta sean computacionalmente costosos. Un
hash SHA-512 se puede calcular millones de veces por segundo; yescrypt está
diseñado para ser miles de veces más lento por hash.

### Cambiar el algoritmo del sistema

```bash
# RHEL: en /etc/login.defs
# ENCRYPT_METHOD SHA512
#
# Cambiar a yescrypt:
# ENCRYPT_METHOD YESCRYPT

# Los usuarios existentes mantienen su hash actual hasta que
# cambien su contraseña — el nuevo algoritmo solo aplica a
# contraseñas nuevas o cambiadas
```

### El salt

Cada hash incluye un **salt** aleatorio — un valor generado al crear la
contraseña que se mezcla con ella antes de hacer el hash. Esto garantiza que:

- Dos usuarios con la misma contraseña tienen hashes diferentes
- Los ataques con rainbow tables (tablas precalculadas) no funcionan

```bash
# Mismo password, diferentes salts → diferentes hashes
# $6$randomsalt1$abcdef123456...
# $6$randomsalt2$xyz789fedcba...
```

## PAM — Pluggable Authentication Modules

PAM es el framework que controla cómo se autentican los usuarios en Linux.
`passwd` y los programas de login no verifican contraseñas directamente — usan
PAM.

```bash
# Configuración de PAM
ls /etc/pam.d/
# common-auth      ← Autenticación (Debian)
# common-password  ← Cambio de contraseña (Debian)
# system-auth      ← Autenticación (RHEL)
# password-auth    ← Cambio de contraseña (RHEL)
# login            ← Login en terminal
# sshd             ← Login por SSH
# sudo             ← Autenticación de sudo
```

### Políticas de complejidad de contraseñas

PAM puede forzar requisitos de complejidad usando `pam_pwquality`:

```bash
# Ver configuración actual
cat /etc/security/pwquality.conf
# minlen = 8          ← Longitud mínima
# dcredit = -1        ← Al menos 1 dígito
# ucredit = -1        ← Al menos 1 mayúscula
# lcredit = -1        ← Al menos 1 minúscula
# ocredit = -1        ← Al menos 1 carácter especial
# minclass = 3        ← Mínimo de clases de caracteres
# maxrepeat = 3       ← Máximo de caracteres repetidos
# difok = 5           ← Mínimo de caracteres diferentes respecto a la anterior
```

```bash
# Instalar pwquality (si no está)
sudo apt install libpam-pwquality    # Debian
sudo dnf install libpwquality        # RHEL (normalmente ya instalado)
```

### Verificar la línea de PAM

```bash
# Debian: verificar que pam_pwquality está activo
grep pwquality /etc/pam.d/common-password
# password requisite pam_pwquality.so retry=3

# RHEL: verificar en system-auth
grep pwquality /etc/pam.d/system-auth
# password requisite pam_pwquality.so try_first_pass local_users_only retry=3
```

## Generar contraseñas seguras

```bash
# Método 1: openssl
openssl rand -base64 16
# kR3xV9mN2bQ5jW8pL1cF

# Método 2: /dev/urandom
head -c 24 /dev/urandom | base64
# a3F1k9B2mQ7yR4wP5xH8tN6s

# Método 3: pwgen (si está instalado)
pwgen -s 16 1
# X3kR9mN2bQ5jW8pL

# Método 4: establecer contraseña sin interacción (para scripts)
echo "newuser:password123" | sudo chpasswd

# Con hash explícito (más seguro en scripts)
echo "newuser:$(openssl passwd -6 'password123')" | sudo chpasswd -e
```

## Diferencias entre distribuciones

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Hash por defecto | yescrypt (`$y$`) | SHA-512 (`$6$`) |
| Módulo de complejidad | `pam_pwquality` (instalar) | `pam_pwquality` (preinstalado) |
| Config de PAM | `/etc/pam.d/common-*` | `/etc/pam.d/system-auth`, `password-auth` |
| Gestión centralizada de PAM | Manual | `authselect` |
| Política por defecto | Mínima | Más restrictiva |

RHEL usa `authselect` para gestionar perfiles de PAM de forma segura:

```bash
# RHEL: ver el perfil activo
sudo authselect current
# Profile ID: sssd

# RHEL: listar perfiles disponibles
sudo authselect list
# sssd
# winbind
# minimal
```

## Tabla: passwd vs chage

| Operación | passwd | chage |
|---|---|---|
| Cambiar contraseña | `passwd user` | No |
| Ver estado | `passwd -S` | `chage -l` |
| Forzar cambio | `passwd -e` | `chage -d 0` |
| Bloquear | `passwd -l` | No |
| Desbloquear | `passwd -u` | No |
| Días mínimos | `passwd -n` | `chage -m` |
| Días máximos | `passwd -x` | `chage -M` |
| Días de aviso | `passwd -w` | `chage -W` |
| Días de gracia | No | `chage -I` |
| Fecha de expiración cuenta | No | `chage -E` |

## Ejercicios

### Ejercicio 1 — Inspeccionar políticas actuales

```bash
# ¿Qué algoritmo de hash usa tu sistema?
grep ENCRYPT_METHOD /etc/login.defs

# ¿Cuándo expira tu contraseña?
sudo chage -l $(whoami)

# ¿Qué estado tiene tu contraseña?
sudo passwd -S $(whoami)
```

### Ejercicio 2 — Configurar una política de expiración

```bash
# Crear usuario de prueba
sudo useradd -m -s /bin/bash policytest
sudo passwd policytest

# Configurar: expira en 90 días, aviso 14 días antes, 7 días de gracia
sudo chage -M 90 -W 14 -I 7 policytest

# Verificar
sudo chage -l policytest

# Limpiar
sudo userdel -r policytest
```

### Ejercicio 3 — Forzar cambio de contraseña

```bash
# Crear usuario
sudo useradd -m -s /bin/bash forcetest
sudo passwd forcetest

# Forzar cambio en el próximo login
sudo chage -d 0 forcetest

# Verificar en /etc/shadow (el campo 3 será 0)
sudo grep '^forcetest:' /etc/shadow

# Limpiar
sudo userdel -r forcetest
```

### Ejercicio 4 — Comparar hashes de算法的

```bash
#!/bin/bash
# Mostrar el tipo de hash de varios usuarios del sistema

for user in root $(whoami); do
    hash=$(sudo getent shadow "$user" | cut -d: -f2)
    if [[ "$hash" == \$* ]]; then
        algo=$(echo "$hash" | cut -d'$' -f2)
        case "$algo" in
            1)  echo "$user: MD5" ;;
            5)  echo "$user: SHA-256" ;;
            6)  echo "$user: SHA-512" ;;
            y)  echo "$user: yescrypt" ;;
            2b) echo "$user: bcrypt" ;;
            *)  echo "$user: desconocido ($algo)" ;;
        esac
    fi
done
```

### Ejercicio 5 — Generador de políticas

```bash
#!/bin/bash
# Aplicar política corporativa estándar a un usuario

apply_corporate_policy() {
    local user="$1"
    echo "Aplicando política corporativa a $user..."
    sudo chage -m 1 -M 90 -W 14 -I 7 "$user"
    echo "Política aplicada:"
    sudo chage -l "$user"
}

sudo useradd -m -s /bin/bash corpuser
sudo passwd corpuser
apply_corporate_policy corpuser

# Limpiar
sudo userdel -r corpuser
```

### Ejercicio 6 — pwquality y contraseñas débiles

```bash
#!/bin/bash
# Intentar establecer una contraseña débil (para ver qué rechaza PAM)

sudo useradd -m -s /bin/bash weakuser

echo "=== Intentando contraseña '12345678' ==="
sudo passwd weakuser 2>&1 || true

echo ""
echo "=== Intentando contraseña 'abc' ==="
sudo passwd weakuser 2>&1 || true

sudo userdel -r weakuser
```

### Ejercicio 7 — Estado de todas las cuentas

```bash
#!/bin/bash
# Listar estado de contraseñas de todos los usuarios del sistema

echo "Estado de todas las cuentas de usuario:"
echo "=========================================="
getent passwd | while IFS=: read -r user pass uid gid gecos home shell; do
    # Solo usuarios regulares
    [[ $uid -ge 1000 && $uid -lt 60000 ]] || continue
    status=$(sudo passwd -S "$user" 2>/dev/null | awk '{print $2}')
    last_change=$(sudo chage -l "$user" 2>/dev/null | head -1 | cut -d: -f2)
    echo "$user: status=$status last_change=$last_change"
done
```

### Ejercicio 8 — Analizar fecha de expiración

```bash
#!/bin/bash
# Mostrar cuentas que expiran en los próximos 30 días

echo "=== Cuentas por expirar en los próximos 30 días ==="
getent passwd | while IFS=: read -r user pass uid gid gecos home shell; do
    [[ $uid -ge 1000 && $uid -lt 60000 ]] || continue
    expire_info=$(sudo chage -l "$user" 2>/dev/null)
    expire_date=$(echo "$expire_info" | grep "Password expires" | cut -d: -f2 | xargs)
    account_expire=$(echo "$expire_info" | grep "Account expires" | cut -d: -f2 | xargs)
    [[ "$expire_date" != " never" && -n "$expire_date" ]] && echo "$user: password_expires=$expire_date"
done
```

### Ejercicio 9 — chpasswd para creación masiva

```bash
#!/bin/bash
# Crear múltiples usuarios con contraseñas inicial común

declare -a USERS=(user1 user2 user3)
INITIAL_PASS="TempPass123!"

for u in "${USERS[@]}"; do
    sudo useradd -m -s /bin/bash "$u"
    echo "$u:$INITIAL_PASS" | sudo chpasswd
    # Forzar cambio en primer login
    sudo chage -d 0 "$u"
done

echo "Usuarios creados. Todos deben cambiar contraseña en primer login."
echo "Contraseña inicial para todos: $INITIAL_PASS"

# Limpiar
for u in "${USERS[@]}"; do
    sudo userdel -r "$u"
done
```

### Ejercicio 10 — Comparar métodos de lock

```bash
#!/bin/bash
# Comparar los tres métodos de "bloquear" una cuenta

sudo useradd -m -s /bin/bash lockdemo
sudo passwd -d lockdemo 2>/dev/null

echo "=== 1. passwd -l (bloquea hash) ==="
sudo passwd -l lockdemo
grep "^lockdemo:" /etc/shadow | cut -d: -f2

echo ""
echo "=== 2. usermod -L (mismo que passwd -l) ==="
sudo passwd -u lockdemo 2>/dev/null
sudo usermod -L lockdemo
grep "^lockdemo:" /etc/shadow | cut -d: -f2

echo ""
echo "=== 3. Expiración de cuenta pasada ==="
sudo usermod -U lockdemo
sudo chage -E 1970-01-01 lockdemo
sudo chage -l lockdemo | grep "Account expires"

echo ""
echo "=== Desbloquear ==="
sudo usermod -U lockdemo
sudo chage -E -1 lockdemo
sudo passwd -u lockdemo 2>/dev/null

sudo userdel -r lockdemo
```
