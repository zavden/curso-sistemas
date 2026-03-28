# T03 — Contraseñas

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

Ambos métodos tienen el mismo efecto: establecen el campo `last_change`
de `/etc/shadow` a 0, lo que el sistema interpreta como "la contraseña
nunca se ha cambiado".

---

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

# 30 días de gracia después de expirar
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

| Concepto | Controla | Efecto al expirar |
|---|---|---|
| Expiración de contraseña (`-M`) | Validez de la contraseña | Debe cambiarla al hacer login |
| Período inactivo (`-I`) | Gracia después de expirar contraseña | Puede hacer login durante N días; después, cuenta bloqueada |
| Expiración de cuenta (`-E`) | Vida útil de la cuenta | La cuenta se desactiva completamente |

La expiración de cuenta es útil para cuentas temporales (contratistas,
becarios). La expiración de contraseña es para política de seguridad continua.

---

## Tabla: passwd vs chage

Ambas herramientas gestionan aspectos de contraseñas, pero con diferente alcance:

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
| Expiración de cuenta | No | `chage -E` |

`passwd` se centra en la **contraseña misma** (establecer, bloquear, eliminar).
`chage` se centra en las **políticas de tiempo** (cuándo expira, cuánto dura).

---

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

Además, yescrypt es **memory-hard**: requiere una cantidad significativa de RAM
para calcular cada hash, lo que hace que los ataques con hardware especializado
(GPUs, ASICs) sean mucho más difíciles y costosos.

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
# $6$salt_aleatorio_A$abcdef123456...
# $6$salt_aleatorio_B$xyz789fedcba...
```

---

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

> **Nota sobre los valores de credit**: Los valores **negativos** en `dcredit`,
> `ucredit`, etc. significan "requerir al menos N caracteres de ese tipo".
> Los valores **positivos** significan "otorgar crédito de N hacia la longitud
> mínima por cada carácter de ese tipo" (más permisivo). El prefijo `-` es
> el que convierte la opción en un requisito obligatorio.

### Verificar la línea de PAM

```bash
# Debian: verificar que pam_pwquality está activo
grep pwquality /etc/pam.d/common-password
# password requisite pam_pwquality.so retry=3

# RHEL: verificar en system-auth
grep pwquality /etc/pam.d/system-auth
# password requisite pam_pwquality.so try_first_pass local_users_only retry=3
```

---

## Generar contraseñas seguras

```bash
# Método 1: openssl (16 bytes → 24 caracteres en base64)
openssl rand -base64 16
# Ej: kR3xV9mN2bQ5/jW8pL1c+Q==

# Método 2: /dev/urandom
head -c 16 /dev/urandom | base64
# Ej: a3F1k9B2mQ7y+R4wP5xH8g==

# Método 3: pwgen (si está instalado)
pwgen -s 16 1
# Ej: X3kR9mN2bQ5jW8pL

# Método 4: establecer contraseña sin interacción (para scripts)
echo "newuser:password123" | sudo chpasswd

# Con hash explícito (más seguro en scripts — la contraseña no queda en el historial)
echo "newuser:$(openssl passwd -6 'password123')" | sudo chpasswd -e
```

> **Seguridad en scripts**: `chpasswd` lee de stdin, evitando que la contraseña
> aparezca en la línea de comandos (visible en `/proc/*/cmdline` y `ps`).
> Aun así, la contraseña en texto plano dentro del script es un riesgo. En
> producción, usar un gestor de secretos o generar contraseñas aleatorias
> y forzar el cambio en el primer login con `chage -d 0`.

---

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

> **No editar PAM manualmente en RHEL**: `authselect` gestiona los archivos
> de PAM. Editarlos directamente puede ser sobrescrito. En Debian no existe
> esta restricción.

---

## Lab — Contraseñas

### Objetivo

Gestionar contraseñas con passwd y chage: establecer y verificar
contraseñas, configurar políticas de expiración, inspeccionar
algoritmos de hash, y comparar el comportamiento entre Debian y
AlmaLinux.

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

### Parte 1 — passwd y estado

#### Paso 1.1: Crear usuario de prueba

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash pwtest
echo "pwtest:Test1234" | chpasswd

echo "=== Usuario creado ==="
getent passwd pwtest
'
```

`chpasswd` permite establecer contraseñas de forma no interactiva.
Útil en scripts de automatización.

#### Paso 1.2: Ver estado de la contraseña

```bash
docker compose exec debian-dev bash -c '
echo "=== Estado de la contraseña ==="
passwd -S pwtest
echo ""
echo "Formato: usuario estado fecha min max warn inactive"
echo "Estado: P=password set, L=locked, NP=no password"
'
```

`passwd -S` muestra un resumen de una línea con el estado de la
contraseña y los parámetros de expiración.

#### Paso 1.3: Bloquear contraseña

```bash
docker compose exec debian-dev bash -c '
echo "=== Antes de bloquear ==="
passwd -S pwtest
echo "Hash (primeros 10 chars):"
grep "^pwtest:" /etc/shadow | cut -d: -f2 | head -c 10
echo "..."

echo ""
echo "=== Bloquear ==="
passwd -l pwtest

echo ""
echo "=== Después de bloquear ==="
passwd -S pwtest
echo "Hash (primeros 10 chars):"
grep "^pwtest:" /etc/shadow | cut -d: -f2 | head -c 10
echo "..."
echo "(el ! se antepuso al hash — el hash original se preserva)"
'
```

`passwd -l` antepone `!` al hash. No destruye la contraseña — se
puede desbloquear con `passwd -u` y la contraseña original funciona.

#### Paso 1.4: Desbloquear

```bash
docker compose exec debian-dev bash -c '
echo "=== Desbloquear ==="
passwd -u pwtest

echo ""
echo "=== Estado ==="
passwd -S pwtest
echo "(vuelve a P = password set)"
'
```

#### Paso 1.5: Eliminar contraseña (inseguro)

```bash
docker compose exec debian-dev bash -c '
echo "=== Eliminar contraseña ==="
passwd -d pwtest

echo ""
echo "=== Estado ==="
passwd -S pwtest
echo "(NP = no password — el usuario puede hacer login sin contraseña)"

echo ""
echo "=== PELIGRO: campo vacío en shadow ==="
grep "^pwtest:" /etc/shadow | cut -d: -f1-3
echo "(el campo 2 está vacío — cualquiera puede entrar)"

echo ""
echo "=== Restaurar contraseña ==="
echo "pwtest:Test1234" | chpasswd
'
```

`passwd -d` elimina la contraseña completamente. El usuario puede
hacer login sin contraseña. Nunca hacer esto en producción.

---

### Parte 2 — chage y políticas

#### Paso 2.1: Información actual

```bash
docker compose exec debian-dev bash -c '
echo "=== Información de expiración ==="
chage -l pwtest
'
```

Por defecto: sin expiración (max=99999), sin mínimo entre cambios,
7 días de aviso.

#### Paso 2.2: Configurar política típica

```bash
docker compose exec debian-dev bash -c '
echo "=== Configurar política ==="
chage -M 90 pwtest    # expira cada 90 días
chage -m 1 pwtest     # mínimo 1 día entre cambios
chage -W 14 pwtest    # aviso 14 días antes
chage -I 30 pwtest    # 30 días de gracia

echo ""
echo "=== Verificar ==="
chage -l pwtest
'
```

`-M` = máximo días. `-m` = mínimo días (evita rotar y volver a la
anterior). `-W` = días de aviso. `-I` = días de gracia después de
expirar (pasados esos días sin cambiar contraseña, la cuenta se bloquea).

#### Paso 2.3: Forzar cambio en próximo login

```bash
docker compose exec debian-dev bash -c '
echo "=== Campo last_change actual ==="
grep "^pwtest:" /etc/shadow | cut -d: -f3

echo ""
echo "=== Forzar cambio ==="
chage -d 0 pwtest

echo ""
echo "=== Campo last_change ahora ==="
grep "^pwtest:" /etc/shadow | cut -d: -f3
echo "(0 = la contraseña se considera expirada inmediatamente)"

echo ""
echo "=== chage -l confirma ==="
chage -l pwtest | head -3

echo ""
echo "=== Restaurar ==="
chage -d $(( $(date +%s) / 86400 )) pwtest
'
```

`chage -d 0` establece el campo last_change a 0. El sistema
interpreta esto como "la contraseña nunca se ha cambiado" y fuerza
el cambio en el próximo login.

#### Paso 2.4: Expiración de cuenta vs contraseña

```bash
docker compose exec debian-dev bash -c '
echo "=== Establecer expiración de CUENTA ==="
chage -E 2025-12-31 pwtest

echo ""
echo "=== Verificar ==="
chage -l pwtest | grep -i "account expires"

echo ""
echo "=== Quitar expiración ==="
chage -E -1 pwtest
chage -l pwtest | grep -i "account expires"
'
```

Expiración de contraseña: el usuario debe cambiarla pero la cuenta
sigue activa. Expiración de cuenta: la cuenta se desactiva
completamente en esa fecha (útil para contratistas temporales).

#### Paso 2.5: Ver campos raw en shadow

```bash
docker compose exec debian-dev bash -c '
echo "=== Campos de shadow para pwtest ==="
IFS=: read -r user hash lc min max warn inact exp reserved < <(grep "^pwtest:" /etc/shadow)
echo "usuario:    $user"
echo "hash:       ${hash:0:10}..."
echo "last_change: $lc (días desde epoch)"
echo "min:        $min"
echo "max:        $max"
echo "warn:       $warn"
echo "inactive:   $inact"
echo "expire:     $exp"
echo "reserved:   $reserved"
'
```

---

### Parte 3 — Algoritmos y comparación

#### Paso 3.1: Algoritmo de cada distribución

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
echo "Configurado: $(grep ENCRYPT_METHOD /etc/login.defs | awk "{print \$2}")"
echo "Prefijo del hash de dev:"
HASH=$(grep "^dev:" /etc/shadow | cut -d: -f2)
echo "${HASH:0:4}..."
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
echo "Configurado: $(grep ENCRYPT_METHOD /etc/login.defs | awk "{print \$2}")"
echo "Prefijo del hash de dev:"
HASH=$(grep "^dev:" /etc/shadow | cut -d: -f2)
echo "${HASH:0:4}..."
'
```

Debian 12: yescrypt (`$y$`). AlmaLinux 9: SHA-512 (`$6$`).

#### Paso 3.2: Mismo password, diferente salt

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear dos usuarios con la misma contraseña ==="
useradd -m hashtest1
useradd -m hashtest2
echo "hashtest1:SamePassword" | chpasswd
echo "hashtest2:SamePassword" | chpasswd

echo ""
echo "=== Comparar hashes ==="
echo "hashtest1: $(grep "^hashtest1:" /etc/shadow | cut -d: -f2 | head -c 30)..."
echo "hashtest2: $(grep "^hashtest2:" /etc/shadow | cut -d: -f2 | head -c 30)..."
echo ""
echo "(diferentes porque cada uno tiene un salt aleatorio distinto)"

userdel -r hashtest1
userdel -r hashtest2
'
```

El salt aleatorio garantiza que dos usuarios con la misma contraseña
tengan hashes completamente diferentes. Esto invalida los ataques
con rainbow tables.

#### Paso 3.3: Configuración de PAM

```bash
echo "=== Debian: PAM para contraseñas ==="
docker compose exec debian-dev bash -c '
echo "--- Módulo de contraseñas ---"
cat /etc/pam.d/common-password 2>/dev/null | grep -v "^#" | grep -v "^$" || echo "(no existe)"
'

echo ""
echo "=== AlmaLinux: PAM para contraseñas ==="
docker compose exec alma-dev bash -c '
echo "--- Módulo de contraseñas ---"
cat /etc/pam.d/system-auth 2>/dev/null | grep -v "^#" | grep -v "^$" | head -10 || echo "(no existe)"
'
```

Debian usa `/etc/pam.d/common-*`. AlmaLinux usa
`/etc/pam.d/system-auth` y `password-auth`.

---

### Limpieza final

```bash
docker compose exec debian-dev bash -c '
userdel -r pwtest 2>/dev/null
userdel -r hashtest1 2>/dev/null
userdel -r hashtest2 2>/dev/null
echo "Limpieza completada"
'
```

---

## Ejercicios

### Ejercicio 1 — Ver estado de contraseñas con passwd -S

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash statetest
echo "statetest:MyPass123" | chpasswd

echo "=== Estado actual ==="
passwd -S statetest
'
```

**Predicción**: ¿Qué letra aparecerá en el campo de estado: P, L, o NP?

<details><summary>Respuesta</summary>

**P** (password set). Acabamos de establecer la contraseña con `chpasswd`.
Los tres estados posibles son:
- **P** = contraseña establecida (puede hacer login)
- **L** = locked (contraseña bloqueada con `!` antepuesto)
- **NP** = no password (campo de hash vacío — inseguro)

</details>

### Ejercicio 2 — Bloqueo y desbloqueo: observar el hash

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash locktest
echo "locktest:Test1234" | chpasswd

echo "=== Hash original (primeros 6 chars) ==="
grep "^locktest:" /etc/shadow | cut -d: -f2 | head -c 6

echo ""
echo "=== Bloquear con passwd -l ==="
passwd -l locktest
grep "^locktest:" /etc/shadow | cut -d: -f2 | head -c 6

echo ""
echo "=== Desbloquear con passwd -u ==="
passwd -u locktest
grep "^locktest:" /etc/shadow | cut -d: -f2 | head -c 6

userdel -r locktest
'
```

**Predicción**: ¿El hash cambiará después de bloquear y desbloquear?

<details><summary>Respuesta</summary>

El hash **no cambia** — solo se le antepone y quita el `!`. Secuencia:
1. Original: `$y$j9T$...` (6 chars: `$y$j9T`)
2. Bloqueado: `!$y$j9...` (6 chars: `!$y$j9` — el `!` desplaza todo)
3. Desbloqueado: `$y$j9T$...` (vuelve al original)

La contraseña se preserva completamente. El `!` simplemente hace que
ningún intento de autenticación coincida porque el prefijo no es un
identificador de algoritmo válido.

</details>

### Ejercicio 3 — chage: configurar y verificar política

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash poltest
echo "poltest:Test1234" | chpasswd

echo "=== Antes (defaults) ==="
chage -l poltest | grep -E "Minimum|Maximum|Warning"

echo ""
echo "=== Aplicar política ==="
chage -m 1 -M 90 -W 14 -I 30 poltest

echo ""
echo "=== Después ==="
chage -l poltest

userdel -r poltest
'
```

**Predicción**: ¿Cuántos días después del último cambio expirará la contraseña?

<details><summary>Respuesta</summary>

**90 días** (el valor de `-M`). Después de expirar:
- El usuario tiene **30 días de gracia** (`-I 30`) para cambiar la contraseña
  al hacer login
- Recibirá avisos **14 días antes** de la expiración (`-W 14`)
- No puede cambiar la contraseña más de una vez al día (`-m 1`)

Si pasan los 90 + 30 = 120 días sin cambio, la cuenta se bloquea.

</details>

### Ejercicio 4 — Comparar algoritmos entre distribuciones

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
echo "Algoritmo: $(grep ENCRYPT_METHOD /etc/login.defs | awk "{print \$2}")"
for user in root dev; do
    hash=$(grep "^$user:" /etc/shadow | cut -d: -f2)
    if [ -n "$hash" ] && [ "$hash" != "*" ] && [ "$hash" != "!" ]; then
        algo=$(echo "$hash" | cut -d"$" -f2)
        case "$algo" in
            1)  echo "  $user: MD5" ;;
            5)  echo "  $user: SHA-256" ;;
            6)  echo "  $user: SHA-512" ;;
            y)  echo "  $user: yescrypt" ;;
            2b) echo "  $user: bcrypt" ;;
            *)  echo "  $user: $algo" ;;
        esac
    else
        echo "  $user: sin hash ($hash)"
    fi
done
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
echo "Algoritmo: $(grep ENCRYPT_METHOD /etc/login.defs | awk "{print \$2}")"
for user in root dev; do
    hash=$(grep "^$user:" /etc/shadow | cut -d: -f2)
    if [ -n "$hash" ] && [ "$hash" != "*" ] && [ "$hash" != "!" ]; then
        algo=$(echo "$hash" | cut -d"$" -f2)
        case "$algo" in
            1)  echo "  $user: MD5" ;;
            5)  echo "  $user: SHA-256" ;;
            6)  echo "  $user: SHA-512" ;;
            y)  echo "  $user: yescrypt" ;;
            2b) echo "  $user: bcrypt" ;;
            *)  echo "  $user: $algo" ;;
        esac
    else
        echo "  $user: sin hash ($hash)"
    fi
done
'
```

**Predicción**: ¿Usarán el mismo algoritmo todos los usuarios dentro de cada contenedor?

<details><summary>Respuesta</summary>

**Sí**, dentro de cada contenedor todos los usuarios con contraseña usarán el
mismo algoritmo (el configurado en `ENCRYPT_METHOD` al momento de crear/cambiar
la contraseña). Pero entre contenedores difiere:
- Debian: **yescrypt** (`$y$`)
- AlmaLinux: **SHA-512** (`$6$`)

Si cambias `ENCRYPT_METHOD` después de crear usuarios, los existentes
mantienen su hash antiguo hasta que cambien su contraseña.

</details>

### Ejercicio 5 — Salt: mismo password, diferente hash

```bash
docker compose exec debian-dev bash -c '
useradd -m saltest1
useradd -m saltest2
echo "saltest1:IdenticalPassword" | chpasswd
echo "saltest2:IdenticalPassword" | chpasswd

echo "=== Hashes (primeros 25 chars) ==="
echo "saltest1: $(grep "^saltest1:" /etc/shadow | cut -d: -f2 | head -c 25)"
echo "saltest2: $(grep "^saltest2:" /etc/shadow | cut -d: -f2 | head -c 25)"

echo ""
echo "¿Son iguales?"
H1=$(grep "^saltest1:" /etc/shadow | cut -d: -f2)
H2=$(grep "^saltest2:" /etc/shadow | cut -d: -f2)
[ "$H1" = "$H2" ] && echo "IGUALES (esto no debería pasar)" || echo "DIFERENTES (correcto: salt distinto)"

userdel -r saltest1
userdel -r saltest2
'
```

**Predicción**: ¿Los hashes serán iguales o diferentes?

<details><summary>Respuesta</summary>

**Diferentes**. Aunque la contraseña es idéntica ("IdenticalPassword"), cada
hash incluye un **salt aleatorio** generado al momento de crear la contraseña.
El hash resultante es `hash(salt + password)`, y como los salts son distintos,
los hashes son distintos.

Esto es fundamental para la seguridad:
- Un atacante no puede comparar hashes entre usuarios para encontrar contraseñas iguales
- Los ataques con rainbow tables (tablas precalculadas) son inútiles porque
  necesitarían una tabla por cada salt posible

</details>

### Ejercicio 6 — Forzar cambio en próximo login

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash forcetest
echo "forcetest:Initial123" | chpasswd

echo "=== Campo last_change antes ==="
grep "^forcetest:" /etc/shadow | cut -d: -f3

echo ""
echo "=== Forzar cambio ==="
chage -d 0 forcetest

echo ""
echo "=== Campo last_change después ==="
grep "^forcetest:" /etc/shadow | cut -d: -f3

echo ""
echo "=== chage -l muestra ==="
chage -l forcetest | head -2

userdel -r forcetest
'
```

**Predicción**: ¿Qué valor tendrá el campo `last_change` después de `chage -d 0`?

<details><summary>Respuesta</summary>

**0**. El campo `last_change` (campo 3 de `/etc/shadow`) almacena el número
de días desde epoch (1 enero 1970) del último cambio de contraseña.

Al ponerlo a 0, el sistema interpreta que la contraseña se "cambió" el
1 de enero de 1970, lo cual está en el pasado de cualquier política de
expiración. Resultado: la contraseña está expirada y debe cambiarse
inmediatamente al hacer login.

`chage -l` mostrará "Last password change: password must be changed".

</details>

### Ejercicio 7 — Expiración de cuenta con chage -E

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash exptest
echo "exptest:Test1234" | chpasswd

echo "=== Sin expiración ==="
chage -l exptest | grep -i "account expires"

echo ""
echo "=== Expirar el 31 dic 2025 ==="
chage -E 2025-12-31 exptest
chage -l exptest | grep -i "account expires"

echo ""
echo "=== Expirar en el pasado (bloqueo inmediato) ==="
chage -E 1970-01-02 exptest
chage -l exptest | grep -i "account expires"

echo ""
echo "=== Quitar expiración ==="
chage -E -1 exptest
chage -l exptest | grep -i "account expires"

userdel -r exptest
'
```

**Predicción**: ¿Qué pasa cuando la fecha de expiración está en el pasado?

<details><summary>Respuesta</summary>

La cuenta queda **inmediatamente desactivada**. Ningún método de login
funcionará (ni contraseña, ni SSH con claves, ni `su`). A diferencia de
`passwd -l` (que solo bloquea la contraseña pero permite SSH con keys),
la expiración de cuenta con `chage -E` es un **bloqueo completo**.

`chage -E -1` quita la expiración (el campo `expire` en shadow queda vacío).

</details>

### Ejercicio 8 — Tres métodos de bloqueo comparados

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash lockcomp
echo "lockcomp:Test1234" | chpasswd

echo "=== Método 1: passwd -l (bloquea hash) ==="
passwd -l lockcomp 2>&1
passwd -S lockcomp
echo "SSH con keys: SIGUE FUNCIONANDO"

echo ""
echo "=== Restaurar ==="
passwd -u lockcomp 2>&1

echo ""
echo "=== Método 2: usermod -s nologin ==="
usermod -s /usr/sbin/nologin lockcomp
getent passwd lockcomp | cut -d: -f7
echo "SSH con keys: BLOQUEADO (nologin rechaza la sesión)"

echo ""
echo "=== Restaurar ==="
usermod -s /bin/bash lockcomp

echo ""
echo "=== Método 3: chage -E (expira cuenta) ==="
chage -E 1970-01-02 lockcomp
chage -l lockcomp | grep "Account expires"
echo "TODO bloqueado: login, SSH, su"

echo ""
echo "=== Restaurar ==="
chage -E -1 lockcomp

userdel -r lockcomp
'
```

**Predicción**: ¿Cuál de los tres métodos bloquea TODO acceso (incluido SSH con claves)?

<details><summary>Respuesta</summary>

| Método | Bloquea contraseña | Bloquea SSH keys | Bloquea su |
|---|---|---|---|
| `passwd -l` | Sí | **No** | Sí (pide contraseña) |
| `usermod -s nologin` | No (hash intacto) | **Sí** (shell rechaza) | Sí (shell rechaza) |
| `chage -E pasado` | **Sí** | **Sí** | **Sí** |

**`chage -E` con fecha pasada** es el único que bloquea completamente todo
acceso. Para un bloqueo total en producción, combinar: `passwd -l` + `chage -E 0`
(bloquea hash Y expira cuenta).

</details>

### Ejercicio 9 — chpasswd para creación masiva

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear usuarios con contraseña inicial ==="
for u in batch1 batch2 batch3; do
    useradd -m -s /bin/bash "$u"
done

# Establecer contraseñas en un solo comando
printf "batch1:Initial123\nbatch2:Initial456\nbatch3:Initial789\n" | chpasswd

# Forzar cambio en primer login
for u in batch1 batch2 batch3; do
    chage -d 0 "$u"
done

echo ""
echo "=== Verificar ==="
for u in batch1 batch2 batch3; do
    echo "$u: $(passwd -S $u | awk "{print \$2}") - $(chage -l $u | head -1)"
done

# Limpiar
for u in batch1 batch2 batch3; do
    userdel -r "$u"
done
'
```

**Predicción**: ¿Los tres usuarios deberán cambiar contraseña en su primer login?

<details><summary>Respuesta</summary>

**Sí**. `chage -d 0` marca la contraseña como "nunca cambiada" para los tres.
En su primer login verán:

```
WARNING: Your password has expired.
You must change your password now and login again!
```

`chpasswd` acepta múltiples líneas `usuario:contraseña` de una vez,
lo que es eficiente para provisioning masivo. Combinado con `chage -d 0`,
garantiza que cada usuario establezca su propia contraseña personal.

</details>

### Ejercicio 10 — Inspeccionar PAM en ambas distribuciones

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
echo "--- /etc/pam.d/common-password ---"
grep -v "^#" /etc/pam.d/common-password 2>/dev/null | grep -v "^$"
echo ""
echo "--- pwquality instalado? ---"
dpkg -l libpam-pwquality 2>/dev/null | grep "^ii" || echo "NO instalado"
echo ""
echo "--- /etc/security/pwquality.conf ---"
cat /etc/security/pwquality.conf 2>/dev/null | grep -v "^#" | grep -v "^$" | head -5 || echo "no existe"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
echo "--- /etc/pam.d/system-auth (password lines) ---"
grep "^password" /etc/pam.d/system-auth 2>/dev/null
echo ""
echo "--- pwquality instalado? ---"
rpm -q libpwquality 2>/dev/null || echo "NO instalado"
echo ""
echo "--- /etc/security/pwquality.conf ---"
cat /etc/security/pwquality.conf 2>/dev/null | grep -v "^#" | grep -v "^$" | head -5 || echo "no existe"
'
```

**Predicción**: ¿Cuál distribución tendrá `pam_pwquality` configurado por defecto?

<details><summary>Respuesta</summary>

**AlmaLinux** lo tiene preinstalado y configurado por defecto. Debian
**no instala** `libpam-pwquality` por defecto — la política de contraseñas
es mínima.

Esto refleja la filosofía de cada distribución:
- **RHEL/AlmaLinux**: orientado a empresa, seguridad por defecto
- **Debian**: minimalista, el administrador decide qué políticas aplicar

En Debian, instalar con `apt install libpam-pwquality` y configurar
en `/etc/security/pwquality.conf`.

</details>

---

## Conceptos reforzados

1. `passwd -S` muestra el estado de la contraseña en una línea.
   `passwd -l` bloquea anteponiendo `!` al hash (preserva la
   contraseña original).

2. `chage -l` muestra las políticas de expiración. `-M` para máximo
   días, `-m` para mínimo, `-W` para aviso, `-I` para gracia.

3. `chage -d 0` fuerza cambio de contraseña en el próximo login.
   `chage -E` controla la expiración de la **cuenta** (no de la
   contraseña) — es el único método que bloquea todo acceso.

4. Debian usa yescrypt (`$y$`), AlmaLinux usa SHA-512 (`$6$`). Los
   algoritmos modernos son intencionalmente lentos para dificultar
   fuerza bruta. yescrypt además es memory-hard.

5. El salt aleatorio garantiza que la misma contraseña produce
   hashes diferentes para cada usuario.

6. PAM es el framework de autenticación. `pam_pwquality` controla
   la complejidad de contraseñas — preinstalado en RHEL, hay que
   instalarlo en Debian.

7. `chpasswd` permite establecer contraseñas de forma no interactiva
   (útil para scripts). Combinado con `chage -d 0`, permite
   provisioning masivo con cambio obligatorio en primer login.

8. Tres métodos de bloqueo con diferentes alcances: `passwd -l` (solo
   contraseña), `usermod -s nologin` (solo shell), `chage -E` (todo).
