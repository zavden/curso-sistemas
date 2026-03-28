# T04 — Usuarios de sistema vs regulares

## Los dos tipos de usuario

Linux distingue entre usuarios **regulares** (personas) y usuarios **de sistema**
(servicios). La diferencia no está en el kernel — para el kernel, un UID es un
UID. La diferencia está en las convenciones de administración:

| Aspecto | Usuario regular | Usuario de sistema |
|---|---|---|
| Propósito | Persona que usa el sistema | Servicio o daemon |
| UID range | 1000-60000 | 1-999 |
| Home | `/home/usuario` | Varía (`/var/lib/servicio`, `/nonexistent`, etc.) |
| Shell | `/bin/bash`, `/bin/zsh` | `/usr/sbin/nologin` o `/bin/false` |
| Login interactivo | Sí | No |
| Contraseña | Sí | Normalmente no |
| Aparece en pantalla de login | Sí | No |
| Se crea con | `useradd -m` | `useradd -r` |

## Rangos de UID

Los rangos están definidos en `/etc/login.defs`:

```bash
grep -E '^(UID|SYS_UID|GID|SYS_GID)' /etc/login.defs
```

| Rango | Tipo | Debian | RHEL |
|---|---|---|---|
| 0 | root | root | root |
| 1-99 | Sistema (estáticos, asignados por paquetes) | Sí | Sí |
| 100-999 (Debian) / 201-999 (RHEL) | Sistema (dinámicos, asignados por `useradd -r`) | Sí | Sí |
| 1000-60000 | Usuarios regulares | Sí | Sí |
| 65534 | nobody | nobody | nobody |

```bash
# Ver los rangos de tu sistema
grep UID /etc/login.defs
# UID_MIN                  1000
# UID_MAX                 60000
# SYS_UID_MIN               201    [RHEL]
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
# Ver usuarios de sistema con UID < 1000
awk -F: '$3 < 1000 {print $1, $3, $7}' /etc/passwd
# root 0 /bin/bash
# daemon 1 /usr/sbin/nologin
# bin 2 /usr/sbin/nologin
# sys 3 /usr/sbin/nologin
# ...
# sshd 110 /usr/sbin/nologin
# _apt 100 /usr/sbin/nologin         [Debian]
# nobody 65534 /usr/sbin/nologin
```

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

```bash
# El mensaje de nologin se puede personalizar
cat /etc/nologin.txt 2>/dev/null
# (si este archivo existe, se muestra su contenido en vez del mensaje por defecto)
```

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

# O bloquear la cuenta con una fecha de expiración pasada
sudo usermod -e 1 serviceuser
```

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
# systemd-timesync /              ← sincronización NTP
```

Cada servicio puede acceder solo a sus propios archivos. El directorio home
del usuario de servicio suele ser el directorio de datos del servicio.

## Crear usuarios de sistema

```bash
# Forma estándar
sudo useradd -r -s /usr/sbin/nologin myservice

# Con directorio de datos específico
sudo useradd -r -s /usr/sbin/nologin -d /var/lib/myservice myservice
sudo mkdir -p /var/lib/myservice
sudo chown myservice:myservice /var/lib/myservice

# Con grupo específico
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
- **User namespaces**: los UIDs fuera del mapeo se muestran como nobody
- **Procesos no confiables**: se ejecutan como nobody como última línea de defensa

```bash
# Dentro de un contenedor sin user namespace mapping
cat /proc/1/uid_map
# Si el mapeo no incluye tu UID, apareces como nobody
```

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

## Listar usuarios por tipo

```bash
# Solo usuarios regulares (UID >= 1000, excluyendo nobody)
awk -F: '$3 >= 1000 && $3 < 65534 {print $1, $3}' /etc/passwd
# dev 1000
# alice 1001
# bob 1002

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

## Diferencias entre distribuciones

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Rango UID de sistema dinámico | 100-999 | 201-999 |
| UIDs estáticos reservados | 0-99 | 0-200 |
| Grupo de nobody | `nogroup` (65534) | `nobody` (65534) |
| Prefijo `_` en nombres | Sí (moderno) | No |
| nologin path | `/usr/sbin/nologin` | `/sbin/nologin` (symlink a `/usr/sbin/nologin`) |

---

## Ejercicios

### Ejercicio 1 — Clasificar usuarios

```bash
# Contar usuarios regulares
awk -F: '$3 >= 1000 && $3 < 65534' /etc/passwd | wc -l

# Contar usuarios de sistema
awk -F: '$3 > 0 && $3 < 1000' /etc/passwd | wc -l

# ¿Cuántos tienen shell interactiva?
grep -c -E '(/bin/bash|/bin/zsh)$' /etc/passwd
```

### Ejercicio 2 — Crear un usuario de servicio

```bash
# Crear un usuario de servicio para una app ficticia
sudo useradd -r -s /usr/sbin/nologin -d /var/lib/myapp -c "My App Service" myapp

# Verificar
getent passwd myapp
id myapp

# ¿Puede hacer login?
su - myapp

# Limpiar
sudo userdel myapp
```

### Ejercicio 3 — Verificar seguridad

```bash
# ¿Hay algún usuario con UID 0 que no sea root?
awk -F: '$3 == 0 && $1 != "root"' /etc/passwd

# ¿Hay algún usuario sin contraseña? (campo 2 vacío en /etc/shadow)
sudo awk -F: '$2 == "" {print $1}' /etc/shadow

# ¿Hay algún usuario de sistema con shell interactiva que no sea root?
awk -F: '$3 > 0 && $3 < 1000 && $7 !~ /nologin|false/ {print $1, $7}' /etc/passwd
```
