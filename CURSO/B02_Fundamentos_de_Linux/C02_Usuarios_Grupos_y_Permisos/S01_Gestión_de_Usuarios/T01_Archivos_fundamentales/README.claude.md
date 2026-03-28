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

---

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

# Cambiar tu propio nombre completo (sin privilegios)
chfn -f "Juan García"

# Cambiar el de otro usuario (requiere root)
sudo chfn -f "Juan García" dev
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
| `x` | Contraseña en `/etc/shadow` (normal en sistemas modernos) |
| `*` | Cuenta sin contraseña (no puede hacer login con contraseña) |
| `!` | Cuenta bloqueada |
| (vacío) | Sin contraseña — login sin password (inseguro, evitar) |

> **Nota**: En sistemas modernos con shadow passwords, el campo 2 de `/etc/passwd`
> es casi siempre `x`. El estado real de la contraseña (bloqueada, hash, vacía)
> se determina en `/etc/shadow`.

### UIDs importantes

```bash
# UID 0 — siempre root, no importa el nombre de usuario
awk -F: '$3==0' /etc/passwd
# root:x:0:0:root:/root:/bin/bash

# UID 65534 — nobody (usuario sin privilegios)
awk -F: '$3==65534' /etc/passwd
# nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
```

El UID 0 tiene significado especial en el kernel: cualquier proceso con UID
efectivo 0 tiene privilegios de root, independientemente del nombre de usuario.

> **Por qué `awk -F: '$3==0'` y no `grep ':0:'`**: El patrón `grep ':0:'` es
> frágil porque coincide con *cualquier* campo que contenga `:0:`, incluyendo
> el GID. `awk` permite seleccionar exactamente el campo 3 (UID).

---

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
sshd:!:19500:::::::
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

> Son 9 campos separados por 8 colons (`:`). En entradas como `sshd:!:19500:::::::`,
> los campos vacíos tras `19500` significan "sin restricción" o "no configurado".

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

# Ver el algoritmo configurado en login.defs
grep ENCRYPT_METHOD /etc/login.defs
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

---

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

---

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
| 2 | `hash` | Hash de contraseña de grupo (`!` = sin contraseña, `*` = no se puede usar `newgrp`) |
| 3 | `admins` | Usuarios que pueden administrar el grupo (agregar/quitar miembros) |
| 4 | `miembros` | Usuarios miembros (replica `/etc/group` campo 4) |

Las contraseñas de grupo permiten que un usuario no miembro use `newgrp grupo`
y proporcione la contraseña para obtener membresía temporal. En la práctica,
esto casi nunca se usa — se gestiona con `usermod -aG`.

---

## Tabla resumen: cómo cada archivo se relaciona con los demás

| Archivo | Referencia cruzada | Sincronización automática |
|---|---|---|
| `/etc/passwd` | Campo GID → `/etc/group` | `useradd`, `usermod`, `userdel` |
| `/etc/shadow` | Campo usuario → `/etc/passwd` | `passwd`, `chage` |
| `/etc/group` | Campo miembros → `/etc/passwd` | `groupadd`, `gpasswd`, `usermod -aG` |
| `/etc/gshadow` | Campo grupo → `/etc/group` | `gpasswd` |

---

## Herramientas para consultar

Nunca editar estos archivos directamente con un editor. Usar las herramientas
del sistema:

```bash
# getent — consulta la base de datos de nombres (incluye LDAP, NIS si configurados)
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

id -Gn dev   # Todos los grupos por nombre
# dev sudo docker

# groups — forma alternativa
groups dev
# dev : dev sudo docker
```

`getent` es la forma correcta de consultar porque lee de todas las fuentes
configuradas en `/etc/nsswitch.conf` (archivos locales, LDAP, SSSD, etc.),
no solo de los archivos locales.

---

## Diferencias entre distribuciones

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Permisos de `/etc/shadow` | `640 root:shadow` | `000 root:root` |
| Grupo shadow | Existe (`shadow`) | No existe |
| Hash por defecto | yescrypt (`$y$`) en Debian 12+ | SHA-512 (`$6$`) en RHEL 9 |
| UID de nobody | 65534 | 65534 |
| Rango de UIDs de sistema | 100–999 | 201–999 |

```bash
# Debian: /etc/shadow tiene grupo shadow
ls -la /etc/shadow
# -rw-r----- 1 root shadow ... /etc/shadow

# RHEL: /etc/shadow sin permisos para nadie excepto root
ls -la /etc/shadow
# ---------- 1 root root ... /etc/shadow
```

> **¿Cómo lee root un archivo con permisos `000`?** El kernel Linux permite al
> UID 0 (root) leer y escribir cualquier archivo independientemente de los bits
> de permiso. Los permisos `000` solo impiden acceso a usuarios no-root.

---

## Consistencia entre archivos

Los cuatro archivos deben estar sincronizados. Si se corrompen o se editan
manualmente de forma incorrecta, las herramientas de gestión de usuarios fallan.

Para verificar la consistencia:

```bash
# Verificar /etc/passwd y /etc/shadow (modo solo lectura con -r)
sudo pwck -r
# user 'dev': directory '/home/dev' does not exist  (si falta el home)

# Verificar /etc/group y /etc/gshadow
sudo grpck -r
```

Estas herramientas detectan entradas huérfanas, campos inválidos, y
inconsistencias entre los archivos. El flag `-r` es importante: ejecuta
verificación sin modificar nada.

---

## Lab — Archivos fundamentales

### Objetivo

Inspeccionar los cuatro archivos de identidad de Linux (`/etc/passwd`,
`/etc/shadow`, `/etc/group`, `/etc/gshadow`): interpretar sus campos,
comparar formatos entre Debian y AlmaLinux, y usar las herramientas
de consulta correctas.

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

### Parte 1 — /etc/passwd

#### Paso 1.1: Contenido completo

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/passwd ==="
cat /etc/passwd
'
```

Cada línea tiene 7 campos separados por `:`.
Formato: `usuario:x:UID:GID:comentario:home:shell`.

#### Paso 1.2: Contar usuarios

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
echo "Total: $(wc -l < /etc/passwd)"
echo "Con shell interactiva: $(grep -c "/bin/bash\|/bin/zsh\|/bin/sh$" /etc/passwd)"
echo "Con nologin: $(grep -c "nologin\|/bin/false" /etc/passwd)"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
echo "Total: $(wc -l < /etc/passwd)"
echo "Con shell interactiva: $(grep -c "/bin/bash\|/bin/zsh\|/bin/sh$" /etc/passwd)"
echo "Con nologin: $(grep -c "nologin\|/bin/false" /etc/passwd)"
'
```

La mayoría de usuarios son de sistema (nologin). Solo unos pocos
tienen shell interactiva.

#### Paso 1.3: Campos clave

```bash
docker compose exec debian-dev bash -c '
echo "=== UID 0 (root) ==="
awk -F: "\$3==0" /etc/passwd

echo ""
echo "=== UID 65534 (nobody) ==="
awk -F: "\$3==65534" /etc/passwd

echo ""
echo "=== Usuario dev ==="
getent passwd dev
'
```

UID 0 siempre es root (significado especial en el kernel). UID 65534
es nobody. `getent` es la forma correcta de consultar — lee de todas
las fuentes configuradas en nsswitch.conf.

#### Paso 1.4: Shells usadas y shells válidas

```bash
docker compose exec debian-dev bash -c '
echo "=== Shells usadas en el sistema ==="
awk -F: "{print \$7}" /etc/passwd | sort | uniq -c | sort -rn

echo ""
echo "=== Shells válidas ==="
cat /etc/shells
'
```

Las shells listadas en `/etc/shells` son las únicas que `chsh`
permite asignar a usuarios.

#### Paso 1.5: Comparar con AlmaLinux

```bash
echo "=== Debian: usuario dev ==="
docker compose exec debian-dev getent passwd dev

echo ""
echo "=== AlmaLinux: usuario dev ==="
docker compose exec alma-dev getent passwd dev
```

Mismo formato, mismos campos. La diferencia puede estar en el UID
asignado y la shell por defecto.

---

### Parte 2 — /etc/shadow

#### Paso 2.1: Permisos de /etc/shadow

```bash
echo "=== Debian ==="
docker compose exec debian-dev ls -la /etc/shadow

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev ls -la /etc/shadow
```

Debian: `640 root:shadow` (existe el grupo shadow). AlmaLinux:
`000 root:root` (sin grupo shadow). En ambos casos, solo root
puede leer el contenido.

#### Paso 2.2: Contenido de /etc/shadow

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/shadow (primeras líneas) ==="
head -5 /etc/shadow

echo ""
echo "=== Campos del usuario dev ==="
grep "^dev:" /etc/shadow
'
```

Formato: `usuario:hash:last_change:min:max:warn:inactive:expire:reserved`.
El campo hash contiene `$id$salt$hash` o valores especiales (`*`, `!`, `!!`).

#### Paso 2.3: Identificar algoritmo de hash

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
echo "Algoritmo configurado:"
grep ENCRYPT_METHOD /etc/login.defs
echo ""
echo "Prefijo del hash de dev:"
grep "^dev:" /etc/shadow | cut -d: -f2 | cut -d"$" -f2
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
echo "Algoritmo configurado:"
grep ENCRYPT_METHOD /etc/login.defs
echo ""
echo "Prefijo del hash de dev:"
grep "^dev:" /etc/shadow | cut -d: -f2 | cut -d"$" -f2
'
```

Debian 12 usa yescrypt (`$y$`). AlmaLinux 9 usa SHA-512 (`$6$`).
Los algoritmos modernos son intencionalmente lentos para dificultar
ataques de fuerza bruta.

#### Paso 2.4: Valores especiales en el hash

```bash
docker compose exec debian-dev bash -c '
echo "=== Estados de contraseña ==="
while IFS=: read -r user hash rest; do
    case "$hash" in
        \$*) state="password set" ;;
        \!\$*) state="locked (hash preserved)" ;;
        \!|!!) state="locked" ;;
        \*) state="no password (cannot login)" ;;
        "") state="EMPTY (insecure!)" ;;
        *) state="unknown" ;;
    esac
    echo "$user: $state"
done < /etc/shadow
'
```

`*` = no puede hacer login por contraseña. `!` = cuenta bloqueada.
`$y$...` o `$6$...` = hash real.

#### Paso 2.5: Información de expiración

```bash
docker compose exec debian-dev bash -c '
echo "=== Expiración del usuario dev ==="
chage -l dev

echo ""
echo "=== Campo last_change en días desde epoch ==="
DAYS=$(grep "^dev:" /etc/shadow | cut -d: -f3)
echo "Días: $DAYS"
date -d "1970-01-01 + $DAYS days" 2>/dev/null || echo "(no se pudo convertir)"
'
```

Los campos de fecha usan "días desde epoch" (1 enero 1970).
`chage -l` los muestra en formato legible.

---

### Parte 3 — /etc/group y herramientas

#### Paso 3.1: Contenido de /etc/group

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/group ==="
cat /etc/group

echo ""
echo "=== Formato: grupo:x:GID:miembros ==="
echo "(el campo 4 lista miembros suplementarios, NO el primario)"
'
```

El campo 4 solo lista usuarios que tienen este grupo como
**suplementario**. Los usuarios con este grupo como primario
(campo GID en /etc/passwd) no aparecen aquí.

#### Paso 3.2: Relación entre passwd y group

```bash
docker compose exec debian-dev bash -c '
echo "=== GID primario de dev en /etc/passwd ==="
getent passwd dev | cut -d: -f4

echo ""
echo "=== Línea del grupo dev en /etc/group ==="
getent group dev

echo ""
echo "=== Observa: dev NO aparece en el campo 4 de su propio grupo ==="
echo "(es miembro implícito por su GID primario)"

echo ""
echo "=== Todos los grupos de dev (primario + suplementarios) ==="
id dev
'
```

`id` muestra la visión completa: grupo primario y suplementarios.
Es la única forma de ver todos los grupos de un usuario.

#### Paso 3.3: getent vs leer archivos directamente

```bash
docker compose exec debian-dev bash -c '
echo "=== getent passwd dev ==="
getent passwd dev

echo ""
echo "=== getent group ==="
getent group sudo 2>/dev/null || echo "(grupo sudo no existe)"

echo ""
echo "=== getent es mejor que grep ==="
echo "getent consulta TODAS las fuentes de nsswitch.conf"
echo "grep solo lee el archivo local"
grep -E "^(passwd|group|shadow)" /etc/nsswitch.conf
'
```

`getent` es la forma correcta porque consulta todas las fuentes
configuradas (archivos locales, LDAP, SSSD). Nunca usar `grep`
directamente sobre los archivos para consultas en producción.

#### Paso 3.4: Verificar consistencia

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
echo "--- Verificar /etc/passwd y /etc/shadow ---"
pwck -r 2>&1 | tail -3

echo ""
echo "--- Verificar /etc/group y /etc/gshadow ---"
grpck -r 2>&1 | tail -3
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
echo "--- Verificar /etc/passwd y /etc/shadow ---"
pwck -r 2>&1 | tail -3

echo ""
echo "--- Verificar /etc/group y /etc/gshadow ---"
grpck -r 2>&1 | tail -3
'
```

`pwck -r` y `grpck -r` verifican la consistencia entre los archivos
en modo solo lectura (no modifican nada).

#### Paso 3.5: Comparar entre distribuciones

```bash
echo "=== Debian: grupo shadow ==="
docker compose exec debian-dev bash -c 'getent group shadow 2>/dev/null || echo "no existe"'

echo ""
echo "=== AlmaLinux: grupo shadow ==="
docker compose exec alma-dev bash -c 'getent group shadow 2>/dev/null || echo "no existe"'

echo ""
echo "=== Rango de UIDs de sistema ==="
echo "Debian:"
docker compose exec debian-dev grep -E "^SYS_UID" /etc/login.defs
echo "AlmaLinux:"
docker compose exec alma-dev grep -E "^SYS_UID" /etc/login.defs
```

Debian tiene el grupo `shadow` para permitir que ciertos programas
(como `passwd` con SGID) lean `/etc/shadow` sin ser root completo.
AlmaLinux no lo usa — protege con permisos `000`.

---

## Ejercicios

### Ejercicio 1 — Contar y clasificar usuarios

```bash
docker compose exec debian-dev bash -c '
echo "Total usuarios: $(wc -l < /etc/passwd)"
echo "Con shell interactiva: $(grep -c "/bin/bash\|/bin/zsh" /etc/passwd)"
echo "Con nologin/false: $(grep -c "nologin\|/bin/false" /etc/passwd)"
'
```

**Predicción**: ¿Habrá más usuarios con shell interactiva o con nologin?

<details><summary>Respuesta</summary>

**Muchos más con nologin**. La mayoría de entradas en `/etc/passwd` son
usuarios de sistema (daemons como `sshd`, `daemon`, `nobody`, etc.) que
no necesitan acceso interactivo. En un sistema típico, solo 1-3 usuarios
tienen shell interactiva (`/bin/bash`); el resto tiene `/usr/sbin/nologin`
o `/bin/false`.

</details>

### Ejercicio 2 — Identificar el algoritmo de hash

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
grep ENCRYPT_METHOD /etc/login.defs
grep "^dev:" /etc/shadow | cut -d: -f2 | cut -d"$" -f2
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
grep ENCRYPT_METHOD /etc/login.defs
grep "^dev:" /etc/shadow | cut -d: -f2 | cut -d"$" -f2
'
```

**Predicción**: ¿Usarán el mismo algoritmo de hash ambos contenedores?

<details><summary>Respuesta</summary>

No. **Debian 12** usa **yescrypt** (`$y$`) y **AlmaLinux 9** usa **SHA-512**
(`$6$`). Yescrypt es más moderno y resistente a ataques con hardware
especializado (GPU/ASIC) porque es memory-hard (requiere mucha RAM para
computar). SHA-512 es más rápido de calcular, lo que lo hace más vulnerable
a fuerza bruta. RHEL 10 probablemente adoptará yescrypt.

</details>

### Ejercicio 3 — El grupo primario no aparece en /etc/group

```bash
docker compose exec debian-dev bash -c '
echo "=== Entrada de dev en /etc/passwd ==="
getent passwd dev

echo ""
echo "=== Línea del grupo dev en /etc/group ==="
getent group dev

echo ""
echo "=== Todos los grupos de dev ==="
id dev
'
```

**Predicción**: ¿Aparecerá "dev" en el campo 4 (miembros) de la línea `dev:x:1000:`?

<details><summary>Respuesta</summary>

**No**. El campo 4 de `/etc/group` solo lista miembros **suplementarios**.
El usuario `dev` tiene `dev` (GID 1000) como grupo **primario** (definido
por el campo 4 de `/etc/passwd`), así que es miembro implícito y no aparece
en `/etc/group`. El único comando que muestra la membresía completa
(primario + suplementarios) es `id`.

</details>

### Ejercicio 4 — Permisos de /etc/shadow en ambas distribuciones

```bash
echo "=== Debian ==="
docker compose exec debian-dev ls -la /etc/shadow

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev ls -la /etc/shadow
```

**Predicción**: ¿Tendrán los mismos permisos y grupo propietario?

<details><summary>Respuesta</summary>

No.
- **Debian**: `-rw-r----- root shadow` (permisos `0640`, grupo `shadow`)
- **AlmaLinux**: `---------- root root` (permisos `0000`, grupo `root`)

Debian usa un grupo especial `shadow` con permiso de lectura grupal.
Programas como `passwd` tienen SGID `shadow` para poder verificar
contraseñas sin ejecutarse como root completo (principio de menor privilegio).
AlmaLinux usa permisos `000` y confía en que solo root lee el archivo.

</details>

### Ejercicio 5 — Clasificar UIDs por rango

```bash
docker compose exec debian-dev bash -c '
echo "UID 0 (root):"
awk -F: "\$3==0{print \$1}" /etc/passwd

echo ""
echo "UIDs de sistema (1-999):"
awk -F: "\$3>0 && \$3<1000{print \$1\": \"\$3}" /etc/passwd | sort -t: -k2 -n

echo ""
echo "UIDs regulares (1000+):"
awk -F: "\$3>=1000 && \$3<60000{print \$1\": \"\$3}" /etc/passwd

echo ""
echo "UID especial (65534/nobody):"
awk -F: "\$3==65534{print \$1\": \"\$3}" /etc/passwd
'
```

**Predicción**: ¿Dónde estará la mayoría de los UIDs: sistema (1-999) o regulares (1000+)?

<details><summary>Respuesta</summary>

La mayoría estará en el **rango de sistema (1-999)**. Los UIDs de sistema
corresponden a daemons y servicios (`sshd`, `daemon`, `sys`, `bin`, etc.).
En un contenedor del curso, habrá probablemente solo 1-2 usuarios regulares
(UID 1000+) pero 10-20 usuarios de sistema. El rango exacto de sistema
depende de la distribución: Debian usa 100-999 (`SYS_UID_MIN=100`),
AlmaLinux usa 201-999 (`SYS_UID_MIN=201`).

</details>

### Ejercicio 6 — Estados de contraseña en /etc/shadow

```bash
docker compose exec debian-dev bash -c '
echo "=== Estados de contraseña ==="
while IFS=: read -r user hash rest; do
    case "$hash" in
        \$*) state="password set" ;;
        \!\$*) state="locked (hash preserved)" ;;
        \!|!!) state="locked" ;;
        \*) state="no password" ;;
        "") state="EMPTY (insecure!)" ;;
        *) state="unknown: $hash" ;;
    esac
    printf "%-15s %s\n" "$user" "$state"
done < /etc/shadow
'
```

**Predicción**: ¿Cuántos usuarios tendrán "password set" vs "no password" vs "locked"?

<details><summary>Respuesta</summary>

Típicamente:
- **`password set`** (`$y$...` o `$6$...`): pocos (root, dev — los que tienen login)
- **`no password`** (`*`): la mayoría (usuarios de sistema como `daemon`, `nobody`)
- **`locked`** (`!` o `!!`): algunos (usuarios de servicio como `sshd`)

La diferencia entre `*` y `!`: ambos impiden login por contraseña, pero
`!` se usa cuando la cuenta fue explícitamente bloqueada (con `passwd -l`),
mientras que `*` indica que la cuenta nunca tuvo contraseña por diseño.

</details>

### Ejercicio 7 — Verificar consistencia con pwck

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c 'pwck -r 2>&1'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c 'pwck -r 2>&1'
```

**Predicción**: ¿Reportará algún problema `pwck`?

<details><summary>Respuesta</summary>

Probablemente sí. Errores comunes en contenedores:
- `user 'xxx': directory '/home/xxx' does not exist` — muchos usuarios de
  sistema tienen home definido pero el directorio no existe (normal en
  contenedores)
- `no matching password file entry in /etc/shadow` — si algún usuario de
  `/etc/passwd` no tiene entrada correspondiente en `/etc/shadow`

`pwck -r` solo reporta, no modifica nada (el flag `-r` es read-only).
Sin `-r`, pwck intentaría corregir los problemas interactivamente.

</details>

### Ejercicio 8 — Comparar el mismo usuario en ambas distribuciones

```bash
for container in debian-dev alma-dev; do
    echo "=== $container ==="
    docker compose exec "$container" bash -c '
    echo "passwd: $(getent passwd dev)"
    echo "shadow hash prefix: $(grep "^dev:" /etc/shadow | cut -d: -f2 | cut -c1-4)"
    echo "groups: $(id dev)"
    echo "shadow perms: $(ls -la /etc/shadow | awk "{print \$1, \$3, \$4}")"
    '
    echo ""
done
```

**Predicción**: ¿Qué campos de `getent passwd dev` diferirán entre Debian y AlmaLinux?

<details><summary>Respuesta</summary>

Los campos que **podrían** diferir:
- **UID** (campo 3): probablemente el mismo (1000) ya que es el primer usuario regular
- **Shell** (campo 7): podría diferir — Debian por defecto asigna `/bin/sh` con `useradd`,
  AlmaLinux asigna `/bin/bash`. Pero las imágenes del curso pueden haberlo configurado igual
- **Hash prefix**: definitivamente diferente — `$y$` (Debian) vs `$6$` (AlmaLinux)
- **Shadow perms**: diferente — `rw-r----- root shadow` vs `---------- root root`

Los campos que **no** difieren: formato de los archivos (idéntico entre familias).

</details>

### Ejercicio 9 — getent vs lectura directa

```bash
docker compose exec debian-dev bash -c '
echo "=== Lectura directa de /etc/passwd ==="
head -5 /etc/passwd

echo ""
echo "=== getent passwd ==="
getent passwd | head -5

echo ""
echo "=== ¿Son idénticos? ==="
diff <(getent passwd) <(cat /etc/passwd) && echo "IDÉNTICOS" || echo "DIFERENTES"

echo ""
echo "=== Fuentes configuradas en nsswitch.conf ==="
grep -E "^(passwd|group|shadow)" /etc/nsswitch.conf
'
```

**Predicción**: ¿Darán el mismo resultado `getent passwd` y `cat /etc/passwd`?

<details><summary>Respuesta</summary>

En un contenedor con solo fuentes locales, probablemente **idénticos**.
Pero en un servidor real con LDAP o SSSD configurado, `getent` devolvería
usuarios adicionales que **no existen** en `/etc/passwd` local.

El archivo `/etc/nsswitch.conf` mostrará algo como `passwd: files` (solo
archivos locales) o `passwd: files sss ldap` (archivos + SSSD + LDAP).
Por eso `getent` es la herramienta correcta en producción: consulta
todas las fuentes configuradas, no solo archivos locales.

</details>

### Ejercicio 10 — Extraer información GECOS y shells inválidas

```bash
docker compose exec debian-dev bash -c '
echo "=== Nombres completos (GECOS) de usuarios con bash ==="
getent passwd | awk -F: "\$7==\"/bin/bash\"{split(\$5,g,\",\"); if(g[1]!=\"\") print \$1\": \"g[1]}"

echo ""
echo "=== Usuarios con shell no válida ==="
while IFS=: read -r user _ _ _ _ _ shell; do
    if [ -n "$shell" ] && ! grep -qx "$shell" /etc/shells 2>/dev/null; then
        if [ "$shell" != "/usr/sbin/nologin" ] && [ "$shell" != "/bin/false" ]; then
            echo "$user: $shell (no está en /etc/shells)"
        fi
    fi
done < /etc/passwd
'
```

**Predicción**: ¿Habrá usuarios con shells que no estén en `/etc/shells`?

<details><summary>Respuesta</summary>

`/usr/sbin/nologin` y `/bin/false` son shells especiales de "rechazo" —
no están en `/etc/shells` pero son válidas para usuarios de sistema.
El script las excluye explícitamente.

El archivo `/etc/shells` lista shells *interactivas* válidas (`/bin/bash`,
`/bin/sh`, `/bin/zsh`, etc.). `chsh` solo permite cambiar a shells listadas
ahí. Si un usuario tuviera una shell que no existe en el sistema ni está
en `/etc/shells` (por ejemplo, si se desinstaló zsh), sería un problema
real — ese usuario no podría iniciar sesión.

</details>

---

## Conceptos reforzados

1. `/etc/passwd` almacena información pública de usuarios (7 campos).
   El campo 2 (`x`) indica que el hash real está en `/etc/shadow`.

2. `/etc/shadow` almacena hashes y políticas de expiración. Solo
   legible por root. El formato del hash (`$id$salt$hash`) identifica
   el algoritmo: `$y$` = yescrypt, `$6$` = SHA-512.

3. `/etc/group` lista miembros suplementarios en el campo 4. Los
   miembros primarios **no aparecen** — hay que usar `id` para ver
   todos los grupos de un usuario.

4. `getent` es la forma correcta de consultar porque lee de todas
   las fuentes de nsswitch.conf, no solo los archivos locales.

5. `pwck -r` y `grpck -r` verifican la consistencia entre los cuatro
   archivos de identidad sin modificarlos.

6. Los cuatro archivos se sincronizan automáticamente cuando se usan
   las herramientas correctas (`useradd`, `usermod`, `passwd`, `gpasswd`).
   Nunca editarlos directamente.

7. Para buscar UIDs específicos, `awk -F: '$3==0'` es más preciso que
   `grep ':0:'`, ya que awk opera sobre campos exactos.
