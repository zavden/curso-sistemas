# T01 — Archivos fundamentales

## Los cuatro archivos de identidad

La identidad de usuarios y grupos en Linux se almacena en cuatro archivos de
texto plano en `/etc`. Juntos forman la **base de datos local** de autenticación:

| Archivo | Contenido | Legible por |
|---|---|---|
| `/etc/passwd` | Información pública de usuarios | Todos |
| `/etc/shadow` | Hashes de contraseñas y políticas | Solo root |
| `/etc/group` | Información de grupos | Todos |
| `/etc/gshadow` | Contraseñas de grupos (raro) | Solo root |

Estos archivos se consultan cada vez que un usuario inicia sesión, ejecuta `su`,
o un programa necesita resolver un UID a un nombre.

## /etc/passwd

Cada línea representa un usuario, con 7 campos separados por `:`:

```bash
cat /etc/passwd
```

```
root:x:0:0:root:/root:/bin/bash
daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin
bin:x:2:2:bin:/bin:/usr/sbin/nologin
sys:x:3:3:sys:/dev:/usr/sbin/nologin
nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
sshd:x:110:65534::/run/sshd:/usr/sbin/nologin
dev:x:1000:1000:Developer:/home/dev:/bin/bash
```

### Formato de campos

```
usuario:x:UID:GID:comentario:home:shell
```

| # | Campo | Descripción |
|---|---|---|
| 1 | `usuario` | Nombre de login (máximo 32 caracteres, sin mayúsculas por convención) |
| 2 | `x` | Placeholder de contraseña — indica que el hash real está en `/etc/shadow` |
| 3 | `UID` | User ID numérico (el kernel usa UIDs, no nombres) |
| 4 | `GID` | Group ID del grupo primario |
| 5 | `comentario` | Campo GECOS — nombre completo, teléfono, etc. (separado por comas) |
| 6 | `home` | Directorio home del usuario |
| 7 | `shell` | Shell de login (o `/usr/sbin/nologin` para usuarios sin acceso interactivo) |

### El campo GECOS

El campo 5 (comentario) viene de una convención de los años 70 (General Electric
Comprehensive Operating Supervisor). Hoy contiene subcampos separados por comas:

```
Nombre Completo,Oficina,Teléfono Trabajo,Teléfono Casa,Otro
```

```bash
# Ver el campo GECOS
getent passwd dev
# dev:x:1000:1000:Developer:/home/dev:/bin/bash

# Cambiar el nombre completo (sin privilegios)
chfn -f "Juan García" dev
```

En la práctica, la mayoría de servidores solo usan el primer subcampo (nombre
completo) y dejan el resto vacío.

### El campo password

Históricamente, el hash de la contraseña se almacenaba directamente en el campo
2 de `/etc/passwd`. Esto era un problema de seguridad porque `/etc/passwd` debe
ser legible por todos los usuarios (los programas lo leen para resolver UIDs a
nombres).

La solución fue mover los hashes a `/etc/shadow` (legible solo por root) y dejar
una `x` como placeholder en `/etc/passwd`. Si este campo contiene:

| Valor | Significado |
|---|---|
| `x` | Contraseña en `/etc/shadow` (normal) |
| `*` | Cuenta sin contraseña (no puede hacer login con contraseña) |
| `!` | Cuenta bloqueada |
| (vacío) | Sin contraseña — login sin password (inseguro, evitar) |

### UIDs importantes

```bash
# UID 0 — siempre root, no importa el nombre de usuario
grep ':0:' /etc/passwd | head -1
# root:x:0:0:root:/root:/bin/bash

# UID 65534 — nobody (usuario sin privilegios)
grep '65534' /etc/passwd
# nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
```

El UID 0 tiene significado especial en el kernel: cualquier proceso con UID
efectivo 0 tiene privilegios de root, independientemente del nombre de usuario.

## /etc/shadow

Almacena los hashes de contraseñas y las políticas de expiración. Solo legible
por root:

```bash
ls -la /etc/shadow
# -rw-r----- 1 root shadow 1234 ... /etc/shadow  [Debian]
# ---------- 1 root root   1234 ... /etc/shadow  [RHEL]

sudo cat /etc/shadow
```

```
root:$y$j9T$abc123...:19500:0:99999:7:::
daemon:*:19500:0:99999:7:::
nobody:*:19500:0:99999:7:::
sshd:!:19500::::::::
dev:$y$j9T$def456...:19500:0:99999:7:::
```

### Formato de campos

```
usuario:hash:last_change:min:max:warn:inactive:expire:reserved
```

| # | Campo | Descripción |
|---|---|---|
| 1 | `usuario` | Nombre (coincide con `/etc/passwd`) |
| 2 | `hash` | Hash de la contraseña (o `*`, `!`, `!!`) |
| 3 | `last_change` | Días desde epoch (1 ene 1970) del último cambio de contraseña |
| 4 | `min` | Días mínimos entre cambios de contraseña |
| 5 | `max` | Días máximos antes de que expire (99999 = nunca) |
| 6 | `warn` | Días de aviso antes de expiración |
| 7 | `inactive` | Días de gracia después de expiración antes de bloquear |
| 8 | `expire` | Fecha de expiración de la cuenta (días desde epoch) |
| 9 | `reserved` | Reservado para uso futuro |

### Interpretar el campo hash

El hash tiene formato `$id$salt$hash`:

```
$y$j9T$LkR3...xyz
 ^  ^    ^
 |  |    └── hash resultante
 |  └── salt (aleatorio, único por usuario)
 └── algoritmo
```

| Prefijo | Algoritmo | Estado |
|---|---|---|
| `$1$` | MD5 | Obsoleto, inseguro |
| `$5$` | SHA-256 | Aceptable |
| `$6$` | SHA-512 | Estándar en la mayoría de distros |
| `$y$` | yescrypt | Default en Debian 12+, Fedora 35+ |
| `$2b$` | bcrypt | Usado en algunos BSDs |

```bash
# Ver qué algoritmo usa tu sistema
sudo grep '^dev:' /etc/shadow | cut -d: -f2 | cut -d'$' -f2
# y   → yescrypt
# 6   → SHA-512
```

### Valores especiales del campo hash

| Valor | Significado |
|---|---|
| `$y$j9T$...` | Hash real — el usuario puede autenticarse |
| `*` | Cuenta sin contraseña (no puede hacer login por contraseña, pero sí por SSH key) |
| `!` o `!!` | Cuenta bloqueada — ningún hash coincidirá jamás |
| `!$y$j9T$...` | Contraseña bloqueada (el `!` se antepone al hash existente) |
| (vacío) | Sin contraseña — login sin password |

```bash
# Bloquear una cuenta (antepone ! al hash)
sudo passwd -l dev
sudo grep '^dev:' /etc/shadow
# dev:!$y$j9T$...:...

# Desbloquear (quita el !)
sudo passwd -u dev
```

### Calcular fechas

Los campos de fecha usan "días desde epoch" (1 enero 1970):

```bash
# Convertir last_change a fecha legible
DAYS=$(sudo grep '^dev:' /etc/shadow | cut -d: -f3)
date -d "1970-01-01 + $DAYS days"
# Mon Mar 10 2025

# O usar chage directamente
sudo chage -l dev
# Last password change                : Mar 10, 2025
# Password expires                    : never
# Account expires                     : never
# Minimum number of days between password change : 0
# Maximum number of days between password change : 99999
# Number of days of warning before password expires : 7
```

## /etc/group

Almacena la información de grupos:

```bash
cat /etc/group
```

```
root:x:0:
daemon:x:1:
sudo:x:27:dev
docker:x:999:dev
dev:x:1000:
```

### Formato de campos

```
grupo:x:GID:miembros
```

| # | Campo | Descripción |
|---|---|---|
| 1 | `grupo` | Nombre del grupo |
| 2 | `x` | Placeholder de contraseña de grupo (en `/etc/gshadow`) |
| 3 | `GID` | Group ID numérico |
| 4 | `miembros` | Lista de usuarios separada por comas (membresía suplementaria) |

### Detalle importante sobre miembros

El campo 4 lista los usuarios que tienen este grupo como **grupo suplementario**.
Los usuarios que lo tienen como **grupo primario** (campo GID en `/etc/passwd`)
**no aparecen aquí**:

```bash
# dev tiene GID 1000 en /etc/passwd → grupo primario "dev"
# Pero en /etc/group, la línea "dev:x:1000:" no lista a "dev"
grep '^dev:' /etc/passwd
# dev:x:1000:1000:...

grep '^dev:' /etc/group
# dev:x:1000:
# (vacío en el campo 4 — dev es miembro implícito por su GID primario)

# Para ver TODOS los grupos de un usuario (primario + suplementarios):
id dev
# uid=1000(dev) gid=1000(dev) groups=1000(dev),27(sudo),999(docker)

groups dev
# dev : dev sudo docker
```

## /etc/gshadow

Contraseñas de grupos y administradores de grupo. En la práctica se usa muy
poco — la mayoría de administradores no asignan contraseñas a grupos:

```bash
sudo cat /etc/gshadow
```

```
root:*::
sudo:!::dev
docker:!::dev
dev:!::
```

### Formato de campos

```
grupo:hash:admins:miembros
```

| # | Campo | Descripción |
|---|---|---|
| 1 | `grupo` | Nombre del grupo |
| 2 | `hash` | Hash de contraseña de grupo (`!` = sin contraseña) |
| 3 | `admins` | Usuarios que pueden administrar el grupo (agregar/quitar miembros) |
| 4 | `miembros` | Usuarios miembros (replica `/etc/group` campo 4) |

Las contraseñas de grupo permiten que un usuario no miembro use `newgrp grupo`
y proporcione la contraseña para obtener membresía temporal. En la práctica,
esto casi nunca se usa — se gestiona con `usermod -aG`.

## Herramientas para consultar

Nunca editar estos archivos directamente con un editor. Usar las herramientas
del sistema:

```bash
# getent — consulta la base de datos de nombres (incluye LDAP, NIS si están configurados)
getent passwd dev
# dev:x:1000:1000:Developer:/home/dev:/bin/bash

getent group sudo
# sudo:x:27:dev

getent shadow dev   # Requiere root
# dev:$y$j9T$...:19500:...

# id — muestra UID, GID y grupos de un usuario
id dev
# uid=1000(dev) gid=1000(dev) groups=1000(dev),27(sudo),999(docker)

id -u dev    # Solo UID
# 1000

id -gn dev   # Solo nombre del grupo primario
# dev

# finger / pinky — información GECOS (si está instalado)
finger dev
```

`getent` es la forma correcta de consultar porque lee de todas las fuentes
configuradas en `/etc/nsswitch.conf` (archivos locales, LDAP, SSSD, etc.),
no solo de los archivos locales.

## Diferencias entre distribuciones

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Permisos de `/etc/shadow` | `640 root:shadow` | `000 root:root` |
| Grupo shadow | Existe (`shadow`) | No existe |
| Hash por defecto | yescrypt (`$y$`) en Debian 12+ | SHA-512 (`$6$`) en RHEL 9 |
| UID de nobody | 65534 | 65534 |
| Rango de UIDs de sistema | 100-999 | 201-999 |

```bash
# Debian: /etc/shadow tiene grupo shadow
ls -la /etc/shadow
# -rw-r----- 1 root shadow ... /etc/shadow

# RHEL: /etc/shadow sin permisos para nadie excepto root
ls -la /etc/shadow
# ---------- 1 root root ... /etc/shadow
```

## Consistencia entre archivos

Los cuatro archivos deben estar sincronizados. Si se corrompen o se editan
manualmente de forma incorrecta, las herramientas de gestión de usuarios fallan.

Para verificar la consistencia:

```bash
# Verificar /etc/passwd y /etc/shadow
sudo pwck
# user 'dev': directory '/home/dev' does not exist  (si falta el home)

# Verificar /etc/group y /etc/gshadow
sudo grpck
```

Estas herramientas detectan entradas huérfanas, campos inválidos, y
inconsistencias entre los archivos.

---

## Ejercicios

### Ejercicio 1 — Inspeccionar los archivos

```bash
# Contar usuarios en el sistema
wc -l /etc/passwd

# ¿Cuántos tienen shell interactiva?
grep -c '/bin/bash\|/bin/zsh\|/bin/sh$' /etc/passwd

# ¿Cuántos tienen nologin?
grep -c 'nologin\|/bin/false' /etc/passwd

# Ver tu propia entrada
getent passwd $(whoami)
```

### Ejercicio 2 — Interpretar /etc/shadow

```bash
# Ver la información de expiración de tu usuario
sudo chage -l $(whoami)

# ¿Qué algoritmo de hash usa tu sistema?
sudo grep "^$(whoami):" /etc/shadow | cut -d: -f2 | cut -d'$' -f2
```

### Ejercicio 3 — Entender la relación entre archivos

```bash
# Ver todos los grupos de un usuario y verificar contra /etc/group
id $(whoami)
grep $(whoami) /etc/group

# ¿Por qué el grupo primario no aparece en el campo 4 de /etc/group?
```
