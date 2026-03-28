# T03 — Grupo primario efectivo

## Qué es el grupo primario efectivo

El **grupo primario efectivo** es el GID que el kernel usa para determinar el
grupo propietario de los archivos que un proceso crea. Normalmente coincide con
el grupo primario definido en `/etc/passwd`, pero puede cambiarse en runtime
con `newgrp` o `sg`.

```bash
# Grupo primario (de /etc/passwd)
id -gn
# dev

# Grupo primario efectivo del proceso actual (de /proc)
cat /proc/self/status | grep ^Gid
# Gid:    1000    1000    1000    1000
#          ^       ^       ^       ^
#        real  efectivo  saved   fs
```

### Los cuatro GIDs del proceso

Cada proceso en Linux tiene cuatro GIDs:

| GID | Descripción | Cuándo diverge |
|---|---|---|
| Real | GID original del usuario al hacer login | Casi nunca cambia (requiere root) |
| Efectivo | GID usado para verificar permisos y asignar grupo a archivos nuevos | Con `newgrp`, `sg`, o binarios SGID |
| Saved | Copia del efectivo anterior a un cambio por SGID | Lo usa el kernel para restaurar tras ejecutar binarios SGID |
| Filesystem | GID usado para operaciones de filesystem | Normalmente = efectivo; cambia con `setfsgid()` (requiere `CAP_SETGID`) |

En condiciones normales, los cuatro son iguales. Divergen cuando se usan
mecanismos como SGID en binarios o `newgrp`.

> **Nota sobre saved GID**: El saved GID existe para que un binario con SGID
> pueda volver a su GID original después de ejecutar con privilegios elevados
> (via `setegid()`). Con `newgrp`, el mecanismo es diferente — simplemente
> se abre una nueva shell y al salir con `exit` se vuelve a la shell anterior.

## Impacto en la creación de archivos

Todo archivo o directorio creado hereda:
- **Owner**: el UID efectivo del proceso
- **Group**: el GID efectivo del proceso (salvo que el directorio padre tenga SGID)

```bash
# Grupo primario efectivo: dev
id -gn
# dev

touch /tmp/file-a
ls -la /tmp/file-a
# -rw-r--r-- 1 dev dev ... file-a

# Cambiar grupo primario efectivo con newgrp
newgrp docker

id -gn
# docker

touch /tmp/file-b
ls -la /tmp/file-b
# -rw-r--r-- 1 dev docker ... file-b
#                  ^^^^^^
#                  cambió porque el GID efectivo ahora es docker

exit  # volver al grupo original
```

## newgrp en detalle

`newgrp` crea una **nueva shell** con el GID efectivo cambiado. No modifica
la sesión actual — crea una subshell:

```bash
# Shell actual
echo $SHLVL
# 1

id -gn
# dev

newgrp docker
echo $SHLVL
# 2  (nueva shell, un nivel más profundo)

id -gn
# docker

exit  # vuelve a la shell anterior
echo $SHLVL
# 1

id -gn
# dev
```

### Qué cambia y qué no con newgrp

| Aspecto | ¿Cambia? |
|---|---|
| GID efectivo del proceso | Sí |
| Grupo propietario de archivos nuevos | Sí |
| `/etc/passwd` | No |
| Grupos suplementarios | Se mantienen |
| Variables de entorno | Se heredan |
| Directorio de trabajo | Se mantiene |
| `$SHLVL` | Se incrementa (es una nueva shell) |

### newgrp sin ser miembro

Si el usuario no es miembro del grupo, `newgrp` verifica `/etc/gshadow`:
- Si el grupo tiene contraseña → la pide
- Si no tiene contraseña → rechaza el acceso

```bash
# dev no es miembro de "staff" (y staff no tiene contraseña)
newgrp staff
# newgrp: failed to crypt password with previous salt: Invalid argument
```

En la práctica, las contraseñas de grupo casi nunca se usan.

### newgrp en scripts

`newgrp` abre una shell interactiva, lo que interrumpe la ejecución de un
script. Para usar `newgrp` dentro de un script, usar el patrón **heredoc**:

```bash
#!/bin/bash
echo "Grupo antes: $(id -gn)"

# Heredoc: los comandos se ejecutan en la subshell de newgrp
newgrp backup <<'INNER'
echo "Grupo dentro: $(id -gn)"
tar czf /backup/data.tar.gz /data
INNER

echo "Grupo después: $(id -gn)"  # restaurado al original
```

## sg — Ejecutar un comando con otro grupo

`sg` cambia el GID efectivo solo para ejecutar **un solo comando**, sin abrir
una shell interactiva:

```bash
# Ejecutar un comando con grupo primario "docker"
sg docker -c "touch /tmp/sg-test"
ls -la /tmp/sg-test
# -rw-r--r-- 1 dev docker ... /tmp/sg-test

# La shell actual NO cambió
id -gn
# dev  (sigue siendo dev)
```

### sg vs newgrp

| Aspecto | `newgrp group` | `sg group -c "cmd"` |
|---|---|---|
| Abre shell interactiva | Sí | No |
| Ejecuta un comando | No (abre shell) | Sí |
| Incrementa `$SHLVL` | Sí | No |
| Requiere `exit` | Sí | No |
| Uso en scripts | Requiere heredoc | Uso directo |

`sg` es preferible cuando solo necesitas ejecutar un comando. `newgrp` es
mejor cuando necesitas una sesión completa con otro grupo efectivo.

## Alternativas sin cambiar grupo efectivo

```bash
# chgrp después de crear
touch /tmp/report.csv
chgrp finance /tmp/report.csv

# install con grupo (crea y asigna grupo en un solo comando)
install -g finance -m 664 /dev/null /tmp/report.csv

# chown :grupo (solo cambia grupo)
chown :finance /tmp/report.csv
```

## SGID en directorios — override del grupo efectivo

Cuando un directorio tiene el bit **SGID** activado, los archivos creados
dentro heredan el **grupo del directorio** en vez del grupo efectivo del
proceso:

```bash
# Sin SGID: el grupo del archivo es el GID efectivo del usuario
mkdir /tmp/normal-dir
touch /tmp/normal-dir/file
ls -la /tmp/normal-dir/file
# -rw-r--r-- 1 dev dev ... file

# Con SGID: el grupo del archivo es el del directorio
sudo mkdir /tmp/sgid-dir
sudo chown root:docker /tmp/sgid-dir
sudo chmod 2775 /tmp/sgid-dir

touch /tmp/sgid-dir/file
ls -la /tmp/sgid-dir/file
# -rw-r--r-- 1 dev docker ... file
#                  ^^^^^^
#          heredó "docker" del directorio, no "dev" del usuario

# Los subdirectorios también heredan el bit SGID
mkdir /tmp/sgid-dir/subdir
ls -ld /tmp/sgid-dir/subdir
# drwxrwsr-x 2 dev docker ... subdir
#       ^
#       s = SGID propagado al subdirectorio
```

**SGID siempre gana sobre el grupo efectivo.** Incluso si cambias tu grupo
efectivo con `newgrp`, los archivos creados dentro de un directorio SGID
heredan el grupo del directorio.

Se cubre en detalle en S03 T03 (SUID, SGID, Sticky bit).

## Escenario práctico: desarrollo web compartido

```bash
# Configuración
sudo groupadd webdev
sudo usermod -aG webdev alice
sudo usermod -aG webdev bob

sudo mkdir -p /var/www/project
sudo chown root:webdev /var/www/project
sudo chmod 2775 /var/www/project

# alice crea un archivo
# (como alice)
touch /var/www/project/index.html
ls -la /var/www/project/index.html
# -rw-r--r-- 1 alice webdev ... index.html
#                    ^^^^^^
# Gracias al SGID, bob (también en webdev) puede leer el archivo
# Si se necesita escritura grupal, configurar umask 002 o usar ACLs
```

## Verificar el grupo efectivo actual

```bash
# Forma simple
id -gn
# dev

# Forma detallada (gid= es el grupo efectivo)
id
# uid=1000(dev) gid=1000(dev) groups=1000(dev),27(sudo),999(docker)

# Desde /proc (muestra los cuatro GIDs del proceso)
cat /proc/self/status | grep ^Gid
# Gid:    1000    1000    1000    1000

# Procesos hijos heredan el grupo efectivo
bash -c 'id -gn'
# dev (heredó el grupo del padre)
```

## Diferencias entre distribuciones

El comportamiento del grupo primario efectivo es idéntico entre Debian y RHEL
— es funcionalidad del kernel, no de la distribución. Las diferencias están en
los defaults:

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| UPG (User Private Group) | Sí (por defecto) | Sí (por defecto) |
| `USERGROUPS_ENAB` | `yes` | `yes` |
| umask por defecto | `022` | `022` |

Ambas familias usan UPG por defecto: cada usuario tiene su propio grupo
privado como grupo primario. Esto hace que la umask `022` sea segura (los
archivos se crean como `user:user`, no como `user:users`).

---

## Labs

### Parte 1 — Observar el grupo efectivo

#### Paso 1.1: Grupo primario actual

```bash
docker compose exec debian-dev bash -c '
echo "=== Grupo primario efectivo ==="
id -gn

echo ""
echo "=== Informacion completa ==="
id
'
```

El `gid=` en la salida de `id` es el grupo primario efectivo.

#### Paso 1.2: Los cuatro GIDs del proceso

```bash
docker compose exec debian-dev bash -c '
echo "=== GIDs del proceso actual ==="
cat /proc/self/status | grep ^Gid
echo ""
echo "Formato: Real  Efectivo  Saved  Filesystem"
echo ""
echo "En condiciones normales, los cuatro son iguales."
echo "Divergen con SGID binarios, newgrp, o setgid()."
'
```

El GID **efectivo** es el que el kernel usa para verificar permisos
y asignar grupo a archivos nuevos.

#### Paso 1.3: Grupos suplementarios del proceso

```bash
docker compose exec debian-dev bash -c '
echo "=== Groups del proceso ==="
cat /proc/self/status | grep ^Groups

echo ""
echo "=== Comparar con id -G ==="
id -G
echo ""
echo "(deben coincidir — son los GIDs que el kernel verifica)"
'
```

Los grupos se establecen al hacer login y permanecen fijos durante
toda la sesión.

### Parte 2 — newgrp y sg

#### Paso 2.1: Preparar grupos

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear grupo de prueba ==="
groupadd -f labeffective
usermod -aG labeffective dev
echo "Grupos de dev: $(id -Gn dev)"
'
```

#### Paso 2.2: newgrp cambia el grupo efectivo

```bash
docker compose exec debian-dev bash -c '
echo "=== Antes de newgrp ==="
echo "Grupo efectivo: $(id -gn)"

echo ""
echo "=== Crear archivo antes ==="
touch /tmp/before-newgrp
ls -la /tmp/before-newgrp
echo "(grupo = grupo primario original)"

rm -f /tmp/before-newgrp
'
```

#### Paso 2.3: newgrp en una sola sesión

```bash
docker compose exec debian-dev bash -c '
echo "=== newgrp en accion ==="
echo "Grupo antes: $(id -gn)"

newgrp labeffective <<INNER
echo "Grupo dentro: \$(id -gn)"
echo "SHLVL: \$SHLVL (nueva shell)"

echo ""
echo "=== GIDs del proceso ==="
cat /proc/self/status | grep ^Gid
echo "(el GID efectivo cambio a labeffective)"

echo ""
echo "=== Crear archivo ==="
touch /tmp/newgrp-test
ls -la /tmp/newgrp-test
echo "(grupo = labeffective)"
INNER

echo ""
echo "Grupo despues de exit: $(id -gn)"
echo "(volvio al original)"

rm -f /tmp/newgrp-test
'
```

`newgrp` crea una subshell con el GID efectivo cambiado. Al salir
(exit o fin del heredoc), se restaura el grupo original.

#### Paso 2.4: sg — un solo comando

```bash
docker compose exec debian-dev bash -c '
echo "=== sg ejecuta un comando con otro grupo ==="
sg labeffective -c "touch /tmp/sg-test"
ls -la /tmp/sg-test
echo "(grupo = labeffective)"

echo ""
echo "=== El grupo actual NO cambio ==="
id -gn
echo "(sg no abre shell interactiva — solo ejecuta el comando)"

rm -f /tmp/sg-test
'
```

`sg` cambia el GID efectivo solo para un comando. No abre una
shell interactiva como `newgrp`.

### Parte 3 — SGID vs grupo efectivo

#### Paso 3.1: Crear directorio con SGID

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear directorio SGID ==="
mkdir -p /tmp/sgid-lab
chown root:labeffective /tmp/sgid-lab
chmod 2775 /tmp/sgid-lab
ls -ld /tmp/sgid-lab
'
```

#### Paso 3.2: Archivo fuera vs dentro del directorio SGID

```bash
docker compose exec debian-dev bash -c '
echo "=== Grupo efectivo actual ==="
id -gn

echo ""
echo "=== Archivo fuera del SGID dir ==="
touch /tmp/outside-sgid
ls -la /tmp/outside-sgid
echo "(grupo = grupo primario efectivo)"

echo ""
echo "=== Archivo dentro del SGID dir ==="
touch /tmp/sgid-lab/inside-sgid
ls -la /tmp/sgid-lab/inside-sgid
echo "(grupo = labeffective, heredado del directorio por SGID)"

rm -f /tmp/outside-sgid /tmp/sgid-lab/inside-sgid
'
```

SGID en el directorio sobreescribe el grupo efectivo del proceso.

#### Paso 3.3: SGID con newgrp — qué gana?

```bash
docker compose exec debian-dev bash -c '
echo "=== Cambiar grupo efectivo a root (diferente al SGID) ==="
newgrp root <<INNER
echo "Grupo efectivo: \$(id -gn)"

echo ""
echo "=== Crear archivo en directorio SGID ==="
touch /tmp/sgid-lab/sgid-wins
ls -la /tmp/sgid-lab/sgid-wins
echo "(grupo = labeffective, NO root — SGID del directorio gana)"
INNER

rm -f /tmp/sgid-lab/sgid-wins
'
```

El SGID del directorio **siempre gana** sobre el grupo efectivo
del proceso.

### Limpieza

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/sgid-lab /tmp/no-sgid 2>/dev/null
groupdel labeffective 2>/dev/null
echo "Limpieza completada"
'
```

---

## Ejercicios

### Ejercicio 1 — Observar el grupo efectivo y crear archivos

```bash
docker compose exec debian-dev bash -c '
groupadd -f efftest
usermod -aG efftest dev

echo "=== Grupo efectivo actual ==="
id -gn

echo ""
echo "=== Crear archivo con grupo efectivo actual ==="
touch /tmp/eff-before
ls -la /tmp/eff-before

echo ""
echo "=== Cambiar grupo efectivo con newgrp ==="
newgrp efftest <<INNER
echo "Grupo efectivo: \$(id -gn)"
touch /tmp/eff-after
ls -la /tmp/eff-after
INNER
'
```

**Pregunta**: ¿Qué grupo tendrá `/tmp/eff-before`? ¿Y `/tmp/eff-after`?

<details><summary>Predicción</summary>

- `eff-before` → grupo **`root`** (o el grupo primario de quien ejecuta en el
  container, que suele ser root)
- `eff-after` → grupo **`efftest`** (el grupo efectivo cambió con `newgrp`)

```
-rw-r--r-- 1 root root    ... /tmp/eff-before
-rw-r--r-- 1 root efftest ... /tmp/eff-after
```

`newgrp` cambió el GID efectivo del proceso dentro del heredoc. Los archivos
creados dentro de la subshell heredan el nuevo grupo efectivo. Fuera del
heredoc, el grupo vuelve al original.

</details>

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/eff-before /tmp/eff-after
groupdel efftest 2>/dev/null
'
```

### Ejercicio 2 — SGID sobreescribe grupo efectivo

```bash
docker compose exec debian-dev bash -c '
groupadd -f sgidgrp
groupadd -f othergrp
usermod -aG sgidgrp dev
usermod -aG othergrp dev

echo "=== Directorio con SGID de sgidgrp ==="
mkdir /tmp/sgid-test
chown root:sgidgrp /tmp/sgid-test
chmod 2775 /tmp/sgid-test

echo ""
echo "=== Cambiar grupo efectivo a othergrp ==="
newgrp othergrp <<INNER
echo "Grupo efectivo: \$(id -gn)"
touch /tmp/sgid-test/testfile
ls -la /tmp/sgid-test/testfile
INNER
'
```

**Pregunta**: El grupo efectivo es `othergrp` pero el directorio tiene SGID
de `sgidgrp`. ¿Qué grupo tendrá `testfile`?

<details><summary>Predicción</summary>

El grupo será **`sgidgrp`**, NO `othergrp`.

```
-rw-r--r-- 1 root sgidgrp ... testfile
```

El SGID del directorio **siempre tiene prioridad** sobre el grupo efectivo del
proceso. Este es el comportamiento diseñado para directorios compartidos: sin
importar qué grupo efectivo tenga cada usuario, todos los archivos creados
dentro del directorio pertenecen al grupo del directorio.

La jerarquía es: SGID del directorio padre > grupo efectivo del proceso.

</details>

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/sgid-test
groupdel sgidgrp; groupdel othergrp
'
```

### Ejercicio 3 — $SHLVL y la subshell de newgrp

```bash
docker compose exec debian-dev bash -c '
groupadd -f shlvlgrp
usermod -aG shlvlgrp dev

echo "=== Shell actual ==="
echo "SHLVL: $SHLVL"
echo "Grupo: $(id -gn)"

echo ""
echo "=== Dentro de newgrp ==="
newgrp shlvlgrp <<INNER
echo "SHLVL: \$SHLVL"
echo "Grupo: \$(id -gn)"
INNER

echo ""
echo "=== Despues de newgrp ==="
echo "SHLVL: $SHLVL"
echo "Grupo: $(id -gn)"
'
```

**Pregunta**: ¿Qué valor tiene `$SHLVL` dentro del heredoc de `newgrp`?
¿Y después de salir?

<details><summary>Predicción</summary>

```
Shell actual:     SHLVL: 1, Grupo: root
Dentro de newgrp: SHLVL: 2, Grupo: shlvlgrp
Después:          SHLVL: 1, Grupo: root
```

`newgrp` crea una **nueva shell** (subshell), lo que incrementa `$SHLVL`.
Al terminar el heredoc (equivalente a `exit`), se destruye la subshell y
`$SHLVL` vuelve al valor anterior. El grupo efectivo también se restaura.

Esto demuestra que `newgrp` NO modifica la shell actual — crea una nueva.
Toda modificación (grupo efectivo, variables) se pierde al salir.

</details>

```bash
docker compose exec debian-dev bash -c 'groupdel shlvlgrp'
```

### Ejercicio 4 — sg vs newgrp

```bash
docker compose exec debian-dev bash -c '
groupadd -f sgtest
usermod -aG sgtest dev

echo "=== Con sg (un solo comando) ==="
echo "Antes: $(id -gn)"
sg sgtest -c "echo \"Dentro de sg: \$(id -gn)\""
echo "Despues: $(id -gn)"

echo ""
echo "=== Con newgrp (heredoc) ==="
echo "Antes: $(id -gn)"
newgrp sgtest <<INNER
echo "Dentro de newgrp: \$(id -gn)"
INNER
echo "Despues: $(id -gn)"
'
```

**Pregunta**: ¿En qué se diferencian `sg` y `newgrp` en términos de cómo
ejecutan el comando? ¿Cuál es más apropiado para un script?

<details><summary>Predicción</summary>

Ambos cambian el grupo efectivo y lo restauran después. La diferencia:

- **`sg`**: ejecuta un solo comando y retorna. No abre shell interactiva.
  Uso directo, ideal para scripts.
- **`newgrp`**: abre una shell interactiva (o heredoc). Requiere `exit` o
  fin del heredoc para volver.

```
sg:      Antes: root → Dentro: sgtest → Después: root
newgrp:  Antes: root → Dentro: sgtest → Después: root
```

Para scripts, `sg` es preferible porque:
1. No requiere heredoc ni gestión de subshell
2. Sintaxis más simple: `sg group -c "comando"`
3. Se integra naturalmente en pipelines

`newgrp` es mejor para sesiones interactivas donde necesitas ejecutar varios
comandos con otro grupo.

</details>

```bash
docker compose exec debian-dev bash -c 'groupdel sgtest'
```

### Ejercicio 5 — Los 4 GIDs del proceso

```bash
docker compose exec debian-dev bash -c '
groupadd -f procgrp
usermod -aG procgrp dev

echo "=== GIDs normales ==="
cat /proc/self/status | grep ^Gid

echo ""
echo "=== GIDs dentro de newgrp ==="
newgrp procgrp <<INNER
cat /proc/self/status | grep ^Gid
INNER
'
```

**Pregunta**: ¿Cuáles de los 4 GIDs (real, efectivo, saved, fs) cambian
dentro de `newgrp`?

<details><summary>Predicción</summary>

**Los cuatro cambian:**

```
Normal:    Gid: 0    0    0    0       (todo root en el container)
En newgrp: Gid: GID  GID  GID  GID    (todo el GID de procgrp)
```

`newgrp` crea una **nueva shell completa** que establece los 4 GIDs al grupo
especificado. Esto es diferente de un binario con SGID, donde solo cambiaría
el efectivo (y el saved guardaría el anterior).

Con `newgrp`, al crear un proceso completamente nuevo, el kernel establece
los 4 GIDs del nuevo proceso al grupo solicitado. No hay necesidad de
"restaurar" via saved GID porque la restauración se logra simplemente
saliendo de la subshell con `exit`.

</details>

```bash
docker compose exec debian-dev bash -c 'groupdel procgrp'
```

### Ejercicio 6 — install -g vs touch + chgrp

```bash
docker compose exec debian-dev bash -c '
groupadd -f installgrp
usermod -aG installgrp dev

echo "=== Metodo 1: touch + chgrp (dos operaciones) ==="
touch /tmp/method1.txt
chgrp installgrp /tmp/method1.txt
ls -la /tmp/method1.txt

echo ""
echo "=== Metodo 2: install -g (una operacion) ==="
install -g installgrp -m 664 /dev/null /tmp/method2.txt
ls -la /tmp/method2.txt

echo ""
echo "=== Metodo 3: sg (sin cambiar shell) ==="
sg installgrp -c "touch /tmp/method3.txt"
ls -la /tmp/method3.txt
'
```

**Pregunta**: Los tres métodos crean un archivo con grupo `installgrp`.
¿Cuál es la diferencia en permisos entre `method1.txt`, `method2.txt` y
`method3.txt`?

<details><summary>Predicción</summary>

- `method1.txt`: permisos según la umask (probablemente `644` con umask 022).
  Grupo cambiado con `chgrp` después de crear.
- `method2.txt`: permisos **`664`** (especificados explícitamente con `-m 664`).
  `install` crea el archivo con grupo y permisos en una sola operación.
- `method3.txt`: permisos según la umask (probablemente `644`). `sg` cambió
  el grupo efectivo para el `touch`.

```
-rw-r--r-- 1 root installgrp ... method1.txt    (644)
-rw-rw-r-- 1 root installgrp ... method2.txt    (664)
-rw-r--r-- 1 root installgrp ... method3.txt    (644)
```

`install -g` es el más preciso porque controla grupo Y permisos en un solo
comando atómico. `touch + chgrp` requiere dos operaciones (hay una ventana
donde el archivo existe con el grupo incorrecto). `sg` es útil cuando
necesitas que múltiples comandos hereden el grupo.

</details>

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/method1.txt /tmp/method2.txt /tmp/method3.txt
groupdel installgrp
'
```

### Ejercicio 7 — newgrp a grupo no propio

```bash
docker compose exec debian-dev bash -c '
groupadd -f secretgrp
useradd -m -s /bin/bash notsecret

echo "=== notsecret NO esta en secretgrp ==="
id notsecret

echo ""
echo "=== Intentar newgrp como notsecret ==="
su -c "newgrp secretgrp" notsecret 2>&1 || true

echo ""
echo "=== Poner contrasena al grupo ==="
echo "secretgrp:testpass" | chgpasswd

echo ""
echo "=== Verificar gshadow ==="
grep "^secretgrp:" /etc/gshadow | cut -d: -f1-2
echo "(ya no es ! — tiene hash de contrasena)"
'
```

**Pregunta**: ¿Qué pasa cuando un usuario no miembro intenta `newgrp` a un
grupo sin contraseña? ¿Y si el grupo tiene contraseña?

<details><summary>Predicción</summary>

- **Sin contraseña** (`!` en gshadow): `newgrp` **rechaza** el acceso. No hay
  forma de entrar al grupo sin ser miembro.

- **Con contraseña**: `newgrp` **pide la contraseña** del grupo. Si el usuario
  la conoce, puede entrar temporalmente al grupo sin ser miembro permanente.

```
Sin contraseña: newgrp: failed to crypt password...
Con contraseña: Password: (espera input)
```

En la práctica, las contraseñas de grupo casi nunca se usan. Son un mecanismo
legacy. La forma moderna de dar acceso temporal es agregar al usuario al grupo
(`gpasswd -a`) y luego quitarlo cuando ya no lo necesite (`gpasswd -d`).

</details>

```bash
docker compose exec debian-dev bash -c '
userdel -r notsecret
groupdel secretgrp
'
```

### Ejercicio 8 — Directorio sin SGID: problema de colaboración

```bash
docker compose exec debian-dev bash -c '
groupadd -f teamgrp
useradd -m -s /bin/bash -G teamgrp alice
useradd -m -s /bin/bash -G teamgrp bob

echo "=== Directorio compartido SIN SGID ==="
mkdir /tmp/no-sgid-collab
chown root:teamgrp /tmp/no-sgid-collab
chmod 775 /tmp/no-sgid-collab

echo ""
echo "=== alice crea un archivo ==="
su -c "touch /tmp/no-sgid-collab/alice.txt" alice
ls -la /tmp/no-sgid-collab/alice.txt

echo ""
echo "=== bob intenta escribir en el archivo de alice ==="
su -c "echo test >> /tmp/no-sgid-collab/alice.txt" bob 2>&1 || true
'
```

**Pregunta**: `alice.txt` fue creado en un directorio con grupo `teamgrp` y
permisos 775. ¿Puede bob escribir en él? ¿Cuál es el problema?

<details><summary>Predicción</summary>

**bob NO puede escribir** en `alice.txt`.

El archivo fue creado con grupo **`alice`** (grupo primario de alice), no
`teamgrp`. Sin SGID, el grupo del archivo es el grupo efectivo del creador:

```
-rw-r--r-- 1 alice alice ... alice.txt
```

bob no es miembro del grupo `alice`, así que accede como `others` (permisos
`r--`). Aunque ambos están en `teamgrp`, el archivo no pertenece a ese grupo.

**Con SGID** (`chmod 2775`), el archivo heredaría grupo `teamgrp`:
```
-rw-r--r-- 1 alice teamgrp ... alice.txt
```
Y bob podría al menos leer via permisos de grupo. Para escritura grupal,
también se necesita umask `002` (o ACLs).

</details>

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/no-sgid-collab
userdel -r alice; userdel -r bob
groupdel teamgrp
'
```

### Ejercicio 9 — Heredar grupo en subdirectorios SGID

```bash
docker compose exec debian-dev bash -c '
groupadd -f projgrp

echo "=== Directorio SGID ==="
mkdir -p /tmp/sgid-inherit
chown root:projgrp /tmp/sgid-inherit
chmod 2775 /tmp/sgid-inherit
ls -ld /tmp/sgid-inherit

echo ""
echo "=== Crear subdirectorio ==="
mkdir /tmp/sgid-inherit/subdir
ls -ld /tmp/sgid-inherit/subdir

echo ""
echo "=== Crear archivo en subdirectorio ==="
touch /tmp/sgid-inherit/subdir/deep-file.txt
ls -la /tmp/sgid-inherit/subdir/deep-file.txt
'
```

**Pregunta**: ¿El subdirectorio también tiene SGID? ¿Y `deep-file.txt`
hereda el grupo de `projgrp`?

<details><summary>Predicción</summary>

**Sí** a ambas preguntas.

```
drwxrwsr-x 2 root projgrp ... /tmp/sgid-inherit/subdir
                ^
                s = SGID heredado

-rw-r--r-- 1 root projgrp ... deep-file.txt
                   ^^^^^^^
                   grupo heredado
```

Cuando se crea un subdirectorio dentro de un directorio con SGID:
1. El subdirectorio hereda el **grupo** del directorio padre
2. El subdirectorio hereda el **bit SGID** del directorio padre

Esto crea una **propagación recursiva**: cualquier subdirectorio creado a
cualquier profundidad hereda SGID y grupo. Es lo que hace que el patrón
`chmod 2775` funcione para proyectos con múltiples niveles de directorios.

</details>

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/sgid-inherit
groupdel projgrp
'
```

### Ejercicio 10 — Script con sg para backup

```bash
docker compose exec debian-dev bash -c '
groupadd -f backupgrp
mkdir -p /tmp/backupdir
chown root:backupgrp /tmp/backupdir
chmod 770 /tmp/backupdir

echo "=== Grupo efectivo actual ==="
id -gn

echo ""
echo "=== Crear backup con sg ==="
sg backupgrp -c "tar czf /tmp/backupdir/test-backup.tar.gz /etc/hostname 2>/dev/null"
ls -la /tmp/backupdir/test-backup.tar.gz

echo ""
echo "=== Grupo efectivo no cambio ==="
id -gn

echo ""
echo "=== Comparar: archivo fuera de sg ==="
touch /tmp/backupdir/manual.txt 2>/dev/null || echo "Sin permisos (no estamos en backupgrp como primario)"
'
```

**Pregunta**: ¿Por qué `sg` es preferible a `newgrp` para scripts de backup?
¿El `touch` final funciona?

<details><summary>Predicción</summary>

`sg` es preferible porque:
1. Ejecuta un solo comando y retorna — no requiere heredoc ni gestión de subshell
2. No abre shell interactiva — no interrumpe el flujo del script
3. El grupo efectivo se restaura automáticamente después

El archivo `test-backup.tar.gz` tendrá grupo `backupgrp` (por sg).

El `touch` final **sí funciona** en este caso porque estamos ejecutando como
root en el container. Root puede escribir en cualquier directorio
independientemente de permisos. En un sistema real con un usuario no-root que
no está en `backupgrp`, el `touch` fallaría con "Permission denied" porque
el directorio tiene permisos `770` y el usuario solo tendría acceso como
`others` (---).

</details>

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/backupdir
groupdel backupgrp
'
```
