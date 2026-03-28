# T04 — Usuarios de sistema vs regulares

## Los dos tipos de usuario

Linux distingue entre usuarios **regulares** (personas) y usuarios **de sistema**
(servicios). La diferencia no está en el kernel — para el kernel, un UID es un
UID. La diferencia está en las convenciones de administración:

| Aspecto | Usuario regular | Usuario de sistema |
|---|---|---|
| Propósito | Persona que usa el sistema | Servicio o daemon |
| UID range | 1000–60000 | 1–999 |
| Home | `/home/usuario` | Varía (`/var/lib/servicio`, `/nonexistent`, etc.) |
| Shell | `/bin/bash`, `/bin/zsh` | `/usr/sbin/nologin` o `/bin/false` |
| Login interactivo | Sí | No |
| Contraseña | Sí | Normalmente bloqueada o `*` |
| Aparece en pantalla de login | Sí | No |
| Se crea con | `useradd -m` | `useradd -r` |

---

## Rangos de UID

Los rangos están definidos en `/etc/login.defs`:

```bash
grep -E '^(UID|SYS_UID|GID|SYS_GID)' /etc/login.defs
```

| Rango | Tipo | Debian | RHEL |
|---|---|---|---|
| 0 | root | root | root |
| 1–99 (Debian) / 1–200 (RHEL) | Sistema (estáticos, asignados por paquetes) | Sí | Sí |
| 100–999 (Debian) / 201–999 (RHEL) | Sistema (dinámicos, asignados por `useradd -r`) | Sí | Sí |
| 1000–60000 | Usuarios regulares | Sí | Sí |
| 65534 | nobody | nobody | nobody |

```bash
# Ver los rangos de tu sistema
grep UID /etc/login.defs
# UID_MIN                  1000
# UID_MAX                 60000
# SYS_UID_MIN               100    [Debian]  /  201  [RHEL]
# SYS_UID_MAX               999
```

### UIDs estáticos conocidos

Algunos UIDs son consistentes entre distribuciones:

| UID | Usuario | Propósito |
|---|---|---|
| 0 | root | Superusuario |
| 1 | daemon (Debian) / bin (RHEL) | Procesos de sistema |
| 2 | bin | Binarios del sistema |
| 65534 | nobody | Sin privilegios, usado por NFS y servicios |

```bash
# Ver usuarios de sistema (UID 1-999, excluyendo root y nobody)
awk -F: '$3 > 0 && $3 < 1000 {print $1, $3, $7}' /etc/passwd
# daemon 1 /usr/sbin/nologin
# bin 2 /usr/sbin/nologin
# sys 3 /usr/sbin/nologin
# ...
# sshd 110 /usr/sbin/nologin
# _apt 100 /usr/sbin/nologin         [Debian]
```

---

## /usr/sbin/nologin vs /bin/false

Ambos impiden el login interactivo, pero de forma diferente:

```bash
# nologin: muestra un mensaje y rechaza
su - sshd
# This account is currently not available.

# false: simplemente retorna exit code 1 (sin mensaje)
su - bin
# (silencio, retorna al prompt)
```

### Cuál usar

| Shell | Comportamiento | Cuándo usar |
|---|---|---|
| `/usr/sbin/nologin` | Muestra mensaje de rechazo, exit 1 | Usuarios de servicio (estándar moderno) |
| `/bin/false` | Exit 1 silencioso | Legacy, cuando no se quiere ningún output |

En la práctica, usar siempre `/usr/sbin/nologin` para usuarios de sistema.
`/bin/false` es legacy y se mantiene por compatibilidad.

> **Nota sobre `/etc/nologin`**: Si el archivo `/etc/nologin` (sin extensión) existe,
> **TODOS** los usuarios no-root son rechazados al hacer login, mostrando el contenido
> del archivo. Esto es diferente de la shell `/usr/sbin/nologin` (que afecta solo al
> usuario que la tiene asignada). `/etc/nologin` es útil durante mantenimiento del
> sistema.

### nologin no bloquea todo

Un usuario con `/usr/sbin/nologin` **no puede** hacer login interactivo
(terminal, SSH con password), pero **sí puede**:

- Ejecutar crontabs
- Tener procesos en ejecución
- Ser el propietario de archivos
- Recibir señales

Para bloquear completamente una cuenta, además de nologin se debe:

```bash
# Bloquear la contraseña
sudo usermod -L serviceuser

# O expirar la cuenta completamente
sudo chage -E 0 serviceuser
```

---

## Por qué existen los usuarios de sistema

### Principio de menor privilegio

Cada servicio debe ejecutarse con los mínimos permisos necesarios. Si nginx
se ejecutara como root y tuviera una vulnerabilidad, el atacante tendría
acceso total al sistema. Con un usuario dedicado (`www-data`, `nginx`),
el daño se limita a lo que ese usuario puede hacer:

```bash
# nginx en Debian ejecuta como www-data
ps aux | grep nginx
# root     12345  ... nginx: master process /usr/sbin/nginx
# www-data 12346  ... nginx: worker process
# www-data 12347  ... nginx: worker process

# El master inicia como root (para bind al puerto 80), luego los workers
# bajan a www-data. Si un worker es comprometido, solo tiene acceso a
# los archivos de www-data, no a todo el sistema.
```

### Cada servicio, su propio usuario

```bash
# Usuarios de servicio típicos
grep nologin /etc/passwd | awk -F: '{print $1, $6}'
# daemon /usr/sbin
# bin /bin
# sys /dev
# www-data /var/www              ← nginx/apache
# sshd /run/sshd                 ← servidor SSH
# postfix /var/spool/postfix     ← servidor de correo
# mysql /var/lib/mysql           ← base de datos MySQL
# postgres /var/lib/postgresql   ← base de datos PostgreSQL
# _apt /nonexistent              ← gestor de paquetes apt [Debian]
# systemd-timesync /             ← sincronización NTP
```

Cada servicio puede acceder solo a sus propios archivos. El directorio home
del usuario de servicio suele ser el directorio de datos del servicio.

---

## Crear usuarios de sistema

```bash
# Forma estándar
sudo useradd -r -s /usr/sbin/nologin myservice

# Con directorio de datos específico
sudo useradd -r -s /usr/sbin/nologin -d /var/lib/myservice myservice
sudo mkdir -p /var/lib/myservice
sudo chown myservice:myservice /var/lib/myservice

# Con grupo específico (si el grupo ya existe)
sudo useradd -r -s /usr/sbin/nologin -g myservice myservice
```

La flag `-r` hace que:
- El UID se asigne del rango de sistema (< 1000)
- **No** se cree directorio home (en la mayoría de configuraciones)
- **No** se cree mail spool
- El grupo se cree en el rango de sistema

### Convención de nombres

Debian usa prefijo `_` para algunos usuarios de sistema modernos:

```bash
grep '^_' /etc/passwd
# _apt:x:100:65534::/nonexistent:/usr/sbin/nologin
# _chrony:x:115:121::/var/lib/chrony:/usr/sbin/nologin
```

RHEL no usa esta convención:

```bash
# RHEL
grep chrony /etc/passwd
# chrony:x:993:989::/var/lib/chrony:/sbin/nologin
```

---

## nobody — El usuario sin privilegios

`nobody` (UID 65534) es un usuario especial sin propietario de archivos ni
grupo significativo. Se usa cuando un servicio necesita ejecutar algo con los
mínimos privilegios posibles:

```bash
id nobody
# uid=65534(nobody) gid=65534(nogroup) groups=65534(nogroup)    [Debian]
# uid=65534(nobody) gid=65534(nobody) groups=65534(nobody)      [RHEL]
```

Usos comunes:
- **NFS**: cuando un UID remoto no tiene equivalente local, se mapea a nobody
- **User namespaces**: los UIDs fuera del mapeo se muestran como nobody/65534
- **Procesos no confiables**: se ejecutan como nobody como última línea de defensa

---

## root — UID 0

El UID 0 es el único UID que el kernel trata de forma especial. Cualquier
proceso con UID efectivo 0 tiene privilegios completos:

```bash
# root no necesita permisos de archivo
ls -la /etc/shadow
# ---------- 1 root root ...
# (permisos 000, pero root puede leer/escribir igualmente)

# root puede enviar señales a cualquier proceso
kill -9 <cualquier-pid>

# root puede cambiar a cualquier usuario sin contraseña
su - dev
# (no pide contraseña)
```

### Buenas prácticas con root

```bash
# Nunca hacer login como root directamente — usar sudo
sudo command

# Nunca usar root como usuario de servicio (excepto procesos que lo requieren
# como master de nginx, sshd, o systemd)

# Verificar qué usuarios tienen UID 0 (debería ser solo root)
awk -F: '$3 == 0 {print $1}' /etc/passwd
# root
# Si hay otro, es una señal de alarma de seguridad
```

---

## Listar usuarios por tipo

```bash
# Solo usuarios regulares (UID >= 1000, excluyendo nobody)
awk -F: '$3 >= 1000 && $3 < 65534 {print $1, $3}' /etc/passwd
# dev 1000

# Solo usuarios de sistema (UID 1-999)
awk -F: '$3 > 0 && $3 < 1000 {print $1, $3, $7}' /etc/passwd
# daemon 1 /usr/sbin/nologin
# bin 2 /usr/sbin/nologin
# ...

# Usuarios con shell interactiva
grep -E '(/bin/bash|/bin/zsh|/bin/sh)$' /etc/passwd
# root:x:0:0:root:/root:/bin/bash
# dev:x:1000:1000::/home/dev:/bin/bash
```

---

## Diferencias entre distribuciones

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Rango UID de sistema dinámico | 100–999 | 201–999 |
| UIDs estáticos reservados | 0–99 | 0–200 |
| Grupo de nobody | `nogroup` (65534) | `nobody` (65534) |
| Prefijo `_` en nombres | Sí (moderno) | No |
| nologin path | `/usr/sbin/nologin` | `/sbin/nologin` (symlink a `/usr/sbin/nologin`) |

---

## Tabla: cómo determinar el tipo de usuario

| Criterio | Regular | Sistema |
|---|---|---|
| UID | >= 1000 y < 65534 | 1–999 (o 0 para root) |
| Grupo primario | Propio (UPG, mismo nombre) | Variable |
| Shell | bash/zsh | nologin/false |
| Home | `/home/username` | `/var/lib/service` o `/nonexistent` |
| Contraseña | Configurada | Bloqueada (`!`) o sin contraseña (`*`) |
| ¿Aparece en `who`? | Sí (si logueado) | No |
| ¿Puede hacer login? | Sí | No |

---

## Lab — Usuarios de sistema vs regulares

### Objetivo

Clasificar usuarios por tipo (sistema vs regular), crear usuarios de
sistema con `useradd -r`, entender la diferencia entre nologin y
`/bin/false`, y realizar una auditoría básica de seguridad.

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

### Parte 1 — Clasificar usuarios

#### Paso 1.1: Listar por rango de UID

```bash
docker compose exec debian-dev bash -c '
echo "=== Usuarios regulares (UID >= 1000, < 65534) ==="
awk -F: "\$3 >= 1000 && \$3 < 65534 {print \$1, \$3, \$7}" /etc/passwd

echo ""
echo "=== Usuarios de sistema (UID 1-999) ==="
awk -F: "\$3 > 0 && \$3 < 1000 {print \$1, \$3, \$7}" /etc/passwd

echo ""
echo "=== Especiales (UID 0 y 65534) ==="
awk -F: "\$3 == 0 || \$3 == 65534 {print \$1, \$3, \$7}" /etc/passwd
'
```

UID 0 = root. UIDs 1–999 = sistema. UIDs 1000+ = regulares.
UID 65534 = nobody (sin privilegios).

#### Paso 1.2: Contar por tipo

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
echo "Regulares: $(awk -F: "\$3 >= 1000 && \$3 < 65534" /etc/passwd | wc -l)"
echo "Sistema:   $(awk -F: "\$3 > 0 && \$3 < 1000" /etc/passwd | wc -l)"
echo "Total:     $(wc -l < /etc/passwd)"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
echo "Regulares: $(awk -F: "\$3 >= 1000 && \$3 < 65534" /etc/passwd | wc -l)"
echo "Sistema:   $(awk -F: "\$3 > 0 && \$3 < 1000" /etc/passwd | wc -l)"
echo "Total:     $(wc -l < /etc/passwd)"
'
```

La mayoría de usuarios son de sistema. Solo unos pocos son personas
reales con login interactivo.

#### Paso 1.3: Usuarios con shell interactiva

```bash
docker compose exec debian-dev bash -c '
echo "=== Usuarios con shell interactiva ==="
grep -E "(/bin/bash|/bin/zsh|/bin/sh)$" /etc/passwd

echo ""
echo "=== Usuarios con nologin ==="
grep -c "nologin\|/bin/false" /etc/passwd
echo "usuarios con nologin o /bin/false"
'
```

#### Paso 1.4: Rangos definidos en login.defs

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
grep -E "^(UID_MIN|UID_MAX|SYS_UID_MIN|SYS_UID_MAX)" /etc/login.defs
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
grep -E "^(UID_MIN|UID_MAX|SYS_UID_MIN|SYS_UID_MAX)" /etc/login.defs
'
```

Debian: sistema 100–999. AlmaLinux: sistema 201–999 (reserva 0–200
para UIDs estáticos).

---

### Parte 2 — Crear usuario de sistema

#### Paso 2.1: Crear usuario de servicio

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear usuario de sistema ==="
useradd -r -s /usr/sbin/nologin -d /var/lib/myservice -c "My Service" myservice

echo ""
echo "--- Verificar ---"
getent passwd myservice
id myservice

echo ""
echo "--- UID asignado (debe ser < 1000) ---"
id -u myservice
'
```

`-r` asigna un UID del rango de sistema (< 1000). No crea home ni
mail spool por defecto.

#### Paso 2.2: Verificar que no puede hacer login

```bash
docker compose exec debian-dev bash -c '
echo "=== Intentar login ==="
su - myservice 2>&1 || true
echo ""
echo "(nologin rechaza el acceso con un mensaje)"

echo ""
echo "=== Shell asignada ==="
getent passwd myservice | cut -d: -f7
'
```

#### Paso 2.3: Crear directorio de datos

```bash
docker compose exec debian-dev bash -c '
echo "=== El home no existe (useradd -r no lo crea) ==="
ls -ld /var/lib/myservice 2>/dev/null || echo "no existe"

echo ""
echo "=== Crear manualmente ==="
mkdir -p /var/lib/myservice
chown myservice:myservice /var/lib/myservice
chmod 750 /var/lib/myservice

echo ""
echo "=== Verificar ==="
ls -ld /var/lib/myservice
'
```

Los usuarios de servicio suelen tener su directorio de datos (no
"home") en `/var/lib/nombre_servicio`. Se crea manualmente con los
permisos adecuados.

#### Paso 2.4: Comparar con usuario regular

```bash
docker compose exec debian-dev bash -c '
echo "=== Usuario de sistema vs regular ==="
echo ""
echo "Sistema (myservice):"
getent passwd myservice
echo "  UID: $(id -u myservice)"
echo "  Home: $(getent passwd myservice | cut -d: -f6)"
echo "  Shell: $(getent passwd myservice | cut -d: -f7)"

echo ""
echo "Regular (dev):"
getent passwd dev
echo "  UID: $(id -u dev)"
echo "  Home: $(getent passwd dev | cut -d: -f6)"
echo "  Shell: $(getent passwd dev | cut -d: -f7)"
'
```

---

### Parte 3 — Seguridad y comparación

#### Paso 3.1: nologin vs /bin/false

```bash
docker compose exec debian-dev bash -c '
echo "=== /usr/sbin/nologin ==="
/usr/sbin/nologin 2>&1
echo "Exit code: $?"

echo ""
echo "=== /bin/false ==="
/bin/false 2>&1
echo "Exit code: $?"
echo "(silencioso — solo retorna 1 sin mensaje)"
'
```

Ambos impiden el login interactivo. `nologin` muestra un mensaje,
`/bin/false` es silencioso. Usar `nologin` para usuarios de sistema
modernos.

#### Paso 3.2: Auditoría de seguridad

```bash
docker compose exec debian-dev bash -c '
echo "=== 1. Usuarios con UID 0 (debería ser solo root) ==="
awk -F: "\$3 == 0 {print \$1}" /etc/passwd

echo ""
echo "=== 2. Usuarios sin contraseña ==="
awk -F: "\$2 == \"\" {print \$1}" /etc/shadow 2>/dev/null || echo "(sin acceso)"

echo ""
echo "=== 3. Usuarios de sistema con shell interactiva (excepto root) ==="
awk -F: "\$3 > 0 && \$3 < 1000 && \$7 !~ /nologin|false/ {print \$1, \$7}" /etc/passwd

echo ""
echo "=== 4. Prefijo _ en nombres (convención Debian) ==="
grep "^_" /etc/passwd || echo "(ningún usuario con prefijo _)"
'
```

Si hay usuarios con UID 0 que no son root, o usuarios de sistema
con shell interactiva, son señales de alarma de seguridad.

#### Paso 3.3: nobody

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
id nobody
echo "Grupo: $(id -gn nobody)"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
id nobody
echo "Grupo: $(id -gn nobody)"
'
```

Debian: grupo `nogroup`. AlmaLinux: grupo `nobody`.

---

### Limpieza final

```bash
docker compose exec debian-dev bash -c '
userdel myservice 2>/dev/null
rm -rf /var/lib/myservice 2>/dev/null
echo "Debian: limpieza completada"
'

docker compose exec alma-dev bash -c '
userdel myservice 2>/dev/null
echo "AlmaLinux: limpieza completada"
'
```

---

## Ejercicios

### Ejercicio 1 — Clasificar usuarios por tipo

```bash
docker compose exec debian-dev bash -c '
echo "=== Regulares ==="
awk -F: "\$3 >= 1000 && \$3 < 65534 {print \$1, \$3}" /etc/passwd

echo ""
echo "=== Sistema ==="
awk -F: "\$3 > 0 && \$3 < 1000 {print \$1, \$3}" /etc/passwd | wc -l
echo "usuarios de sistema"

echo ""
echo "=== Con shell interactiva ==="
grep -c -E "(/bin/bash|/bin/zsh)$" /etc/passwd
echo "usuarios con bash o zsh"
'
```

**Predicción**: ¿Habrá más usuarios regulares o de sistema?

<details><summary>Respuesta</summary>

**Muchos más de sistema**. En un contenedor del curso, habrá probablemente
1-2 usuarios regulares (UID 1000+) pero 15-25 usuarios de sistema (UID 1-999).
La mayoría de entradas en `/etc/passwd` son daemons y servicios
(`sshd`, `daemon`, `bin`, `sys`, `_apt`, etc.) que nunca hacen login interactivo.

</details>

### Ejercicio 2 — Crear un usuario de servicio y verificar

```bash
docker compose exec debian-dev bash -c '
useradd -r -s /usr/sbin/nologin -d /var/lib/labsvc -c "Lab Service" labsvc

echo "=== Verificar ==="
getent passwd labsvc
echo "UID: $(id -u labsvc)"
echo "Shell: $(getent passwd labsvc | cut -d: -f7)"
echo "Home existe: $(ls -ld /var/lib/labsvc 2>/dev/null && echo SÍ || echo NO)"

userdel labsvc
'
```

**Predicción**: ¿El UID asignado estará por debajo de 1000? ¿Se creará el home?

<details><summary>Respuesta</summary>

- **UID**: Sí, estará **por debajo de 1000** (en el rango de sistema definido
  por `SYS_UID_MIN`–`SYS_UID_MAX` en `/etc/login.defs`).
- **Home**: **No** se creará. El flag `-r` no crea home ni mail spool. El `-d`
  solo establece la ruta en `/etc/passwd`, pero no crea el directorio.
  Hay que crearlo manualmente con `mkdir -p` + `chown`.

</details>

### Ejercicio 3 — nologin vs /bin/false

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejecutar nologin directamente ==="
/usr/sbin/nologin 2>&1; echo "Exit: $?"

echo ""
echo "=== Ejecutar /bin/false directamente ==="
/bin/false 2>&1; echo "Exit: $?"

echo ""
echo "=== Intentar su con nologin ==="
useradd -r -s /usr/sbin/nologin nologindemo
su - nologindemo 2>&1; echo "Exit: $?"

echo ""
echo "=== Intentar su con /bin/false ==="
useradd -r -s /bin/false falsedemo
su - falsedemo 2>&1; echo "Exit: $?"

userdel nologindemo
userdel falsedemo
'
```

**Predicción**: ¿Cuál de los dos mostrará un mensaje al rechazar el login?

<details><summary>Respuesta</summary>

Solo **nologin** muestra un mensaje: "This account is currently not available."
`/bin/false` es silencioso — simplemente retorna exit code 1 sin output.

Ambos tienen exit code 1 (fallo), pero nologin es más informativo para el
usuario. Por eso es el estándar moderno para usuarios de servicio.

</details>

### Ejercicio 4 — Auditoría: UID 0 y contraseñas vacías

```bash
docker compose exec debian-dev bash -c '
echo "=== Usuarios con UID 0 ==="
COUNT=$(awk -F: "\$3 == 0" /etc/passwd | wc -l)
awk -F: "\$3 == 0 {print \$1}" /etc/passwd
if [ "$COUNT" -gt 1 ]; then
    echo "ALERTA: $COUNT usuarios con UID 0 (debería ser solo 1)"
else
    echo "OK: Solo root tiene UID 0"
fi

echo ""
echo "=== Usuarios sin contraseña (campo vacío en shadow) ==="
awk -F: "\$2 == \"\" {print \$1}" /etc/shadow 2>/dev/null || echo "(sin permisos)"
COUNT=$(awk -F: "\$2 == \"\"" /etc/shadow 2>/dev/null | wc -l)
echo "Total: $COUNT"

echo ""
echo "=== Usuarios de sistema con shell interactiva ==="
awk -F: "\$3 > 0 && \$3 < 1000 && \$7 !~ /nologin|false/ {print \$1, \$3, \$7}" /etc/passwd
'
```

**Predicción**: ¿Debería haber algún usuario de sistema con shell interactiva (además de root)?

<details><summary>Respuesta</summary>

**No** (idealmente). Los usuarios de sistema ejecutan servicios, no sesiones
interactivas. Si aparece alguno con `/bin/bash` o `/bin/sh`, es una
**señal de alarma** que debe investigarse:
- ¿Es un usuario mal configurado?
- ¿Alguien cambió la shell intencionalmente?
- ¿Es un indicador de compromiso?

La excepción es `root` (UID 0) que legítimamente tiene `/bin/bash`, y en
algunos sistemas `sync` (UID 5) tiene `/bin/sync` como shell (ejecuta
sync y sale).

</details>

### Ejercicio 5 — nobody en ambas distribuciones

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
id nobody
echo "Grupo primario: $(id -gn nobody)"
echo "Tiene home: $(getent passwd nobody | cut -d: -f6)"
echo "Shell: $(getent passwd nobody | cut -d: -f7)"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
id nobody
echo "Grupo primario: $(id -gn nobody)"
echo "Tiene home: $(getent passwd nobody | cut -d: -f6)"
echo "Shell: $(getent passwd nobody | cut -d: -f7)"
'
```

**Predicción**: ¿El grupo de nobody será el mismo en ambas distribuciones?

<details><summary>Respuesta</summary>

**No**:
- **Debian**: nobody pertenece al grupo **`nogroup`** (GID 65534)
- **AlmaLinux**: nobody pertenece al grupo **`nobody`** (GID 65534)

El GID es el mismo (65534), pero el **nombre** del grupo difiere. Esto es
una diferencia histórica entre las familias. En scripts portables, usar
el GID numérico (65534) en vez del nombre.

</details>

### Ejercicio 6 — Inventario de usuarios de sistema

```bash
docker compose exec debian-dev bash -c '
echo "=== Inventario de usuarios de sistema ==="
printf "%-15s %-6s %-30s %s\n" "USUARIO" "UID" "HOME" "SHELL"
echo "-------------------------------------------------------------------"
awk -F: "\$3 > 0 && \$3 < 1000 {printf \"%-15s %-6s %-30s %s\n\", \$1, \$3, \$6, \$7}" /etc/passwd
'
```

**Predicción**: ¿Los homes de usuarios de sistema estarán todos en `/home/`?

<details><summary>Respuesta</summary>

**No**. Los usuarios de sistema tienen homes en ubicaciones diversas que
corresponden a sus directorios de datos:
- `sshd` → `/run/sshd`
- `www-data` → `/var/www`
- `_apt` → `/nonexistent`
- `sys` → `/dev`
- `daemon` → `/usr/sbin`

Solo los usuarios regulares usan `/home/`. Los de sistema usan `/var/lib/`,
`/run/`, `/nonexistent`, o incluso directorios del sistema como `/dev` o
`/usr/sbin` (heredados de diseños históricos).

</details>

### Ejercicio 7 — Verificar UPG (User Private Groups)

```bash
docker compose exec debian-dev bash -c '
echo "=== User Private Groups ==="
awk -F: "\$3 >= 1000 && \$3 < 60000" /etc/passwd | while IFS=: read -r user _ uid gid _ _ _; do
    grp_name=$(getent group "$gid" | cut -d: -f1)
    grp_gid=$(getent group "$user" 2>/dev/null | cut -d: -f3)
    if [ "$grp_gid" = "$uid" ]; then
        echo "$user: UPG activo (UID=$uid, grupo=$grp_name, GID=$gid)"
    else
        echo "$user: NO tiene UPG (UID=$uid, GID=$gid, grupo=$grp_name)"
    fi
done
'
```

**Predicción**: ¿Los usuarios regulares tendrán UPG (grupo con su mismo nombre)?

<details><summary>Respuesta</summary>

**Sí**, si `USERGROUPS_ENAB yes` está configurado en `/etc/login.defs`
(default en ambas familias). Cada usuario regular creado con `useradd`
tendrá un grupo con su mismo nombre y mismo ID numérico. Por ejemplo,
el usuario `dev` (UID 1000) tendrá un grupo `dev` (GID 1000).

UPG (User Private Groups) es un esquema de seguridad: cada usuario tiene
su propio grupo primario, evitando que archivos nuevos pertenezcan a un
grupo compartido por defecto.

</details>

### Ejercicio 8 — Prefijo _ en Debian vs RHEL

```bash
echo "=== Debian: usuarios con prefijo _ ==="
docker compose exec debian-dev bash -c '
grep "^_" /etc/passwd | awk -F: "{print \$1, \$3}" || echo "ninguno"
'

echo ""
echo "=== AlmaLinux: usuarios con prefijo _ ==="
docker compose exec alma-dev bash -c '
grep "^_" /etc/passwd | awk -F: "{print \$1, \$3}" || echo "ninguno"
'
```

**Predicción**: ¿Cuál distribución tendrá usuarios con prefijo `_`?

<details><summary>Respuesta</summary>

Solo **Debian**. El prefijo `_` es una convención de Debian para usuarios
de sistema creados por paquetes modernos (como `_apt`, `_chrony`). El
underscore evita colisiones con nombres de usuarios regulares.

RHEL no usa esta convención — sus usuarios de sistema tienen nombres sin
prefijo (`chrony`, `polkitd`, etc.). Es puramente cosmético; no tiene
efecto funcional.

</details>

### Ejercicio 9 — El patrón master/worker

```bash
docker compose exec debian-dev bash -c '
echo "=== Procesos y sus usuarios ==="
echo "(en un contenedor, la lista será limitada)"
ps aux --no-headers | awk "{print \$1}" | sort | uniq -c | sort -rn | head -10

echo ""
echo "=== Concepto: master como root, workers como usuario de servicio ==="
echo "Ejemplo nginx:"
echo "  root     → nginx: master process (bind puerto 80)"
echo "  www-data → nginx: worker process (maneja conexiones)"
echo ""
echo "El master necesita root para puertos < 1024."
echo "Los workers bajan privilegios al usuario de servicio."
'
```

**Predicción**: ¿Por qué el proceso master de nginx necesita ser root?

<details><summary>Respuesta</summary>

Para hacer **bind a puertos privilegiados** (< 1024). En Linux, solo
procesos con UID 0 (o la capability `CAP_NET_BIND_SERVICE`) pueden abrir
puertos por debajo de 1024 (como el 80 para HTTP y 443 para HTTPS).

El patrón master/worker es un ejemplo del principio de menor privilegio:
1. El master inicia como root (necesario para el bind)
2. Crea los workers
3. Los workers bajan a un usuario sin privilegios (`www-data`)
4. Si un worker es comprometido, el atacante solo tiene acceso de `www-data`

El mismo patrón lo usan: Apache, PostgreSQL, sshd, y otros servicios.

</details>

### Ejercicio 10 — Script de auditoría completo

```bash
docker compose exec debian-dev bash -c '
echo "=========================================="
echo "  AUDITORÍA DE USUARIOS DEL SISTEMA"
echo "=========================================="

echo ""
echo "1. Usuarios con UID 0 (solo root permitido):"
RESULT=$(awk -F: "\$3 == 0 && \$1 != \"root\" {print \$1}" /etc/passwd)
[ -z "$RESULT" ] && echo "   OK" || echo "   ALERTA: $RESULT"

echo ""
echo "2. Contraseñas vacías:"
RESULT=$(awk -F: "\$2 == \"\" {print \$1}" /etc/shadow)
[ -z "$RESULT" ] && echo "   OK" || echo "   ALERTA: $RESULT"

echo ""
echo "3. Usuarios de sistema con shell interactiva (excepto root):"
RESULT=$(awk -F: "\$3 > 0 && \$3 < 1000 && \$7 !~ /nologin|false/ {print \$1}" /etc/passwd)
[ -z "$RESULT" ] && echo "   OK" || echo "   REVISAR: $RESULT"

echo ""
echo "4. Resumen:"
echo "   Total usuarios: $(wc -l < /etc/passwd)"
echo "   Regulares: $(awk -F: "\$3 >= 1000 && \$3 < 65534" /etc/passwd | wc -l)"
echo "   Sistema: $(awk -F: "\$3 > 0 && \$3 < 1000" /etc/passwd | wc -l)"
echo "   Con bash/zsh: $(grep -c -E "(/bin/bash|/bin/zsh)$" /etc/passwd)"
'
```

**Predicción**: ¿Debería haber algún hallazgo de seguridad en un contenedor del curso?

<details><summary>Respuesta</summary>

Probablemente **no** en los checks críticos (UID 0 duplicado, contraseñas vacías).
Pero podrían aparecer:

- **Check 3 (shell interactiva en sistema)**: Algunos usuarios de sistema heredados
  pueden tener `/bin/sh` en vez de nologin (como `sync` que usa `/bin/sync`). Esto
  no es necesariamente un problema pero merece revisión.

En un servidor de producción, este script sería la base de una auditoría
de seguridad regular. Automatizado con cron, podría alertar sobre cambios
inesperados (como un nuevo usuario con UID 0 — indicador clásico de compromiso).

</details>

---

## Conceptos reforzados

1. Los usuarios **regulares** (UID >= 1000) son personas con shell
   interactiva. Los de **sistema** (UID 1–999) son servicios con nologin.

2. `useradd -r` crea usuarios de sistema: UID bajo, sin home, sin
   mail spool. Siempre combinar con `-s /usr/sbin/nologin`.

3. `/usr/sbin/nologin` muestra un mensaje de rechazo.
   `/bin/false` es silencioso. Ambos impiden login interactivo.

4. nologin no bloquea todo: el usuario puede tener archivos,
   procesos y crontabs. Para bloqueo completo: nologin + `passwd -l`
   + `chage -E 0`.

5. El principio de menor privilegio: cada servicio ejecuta como su
   propio usuario, limitando el daño de una vulnerabilidad.

6. Auditoría básica: verificar que solo root tiene UID 0, que no
   hay contraseñas vacías, y que los usuarios de sistema no tienen
   shell interactiva.

7. Debian usa el prefijo `_` para usuarios de sistema modernos; RHEL no.
   El grupo de nobody se llama `nogroup` en Debian y `nobody` en RHEL.
