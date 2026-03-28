# T03 — SUID, SGID, Sticky bit

> **Fuentes**: `README.md`, `README.max.md`, `labs/README.md`
>
> **Correcciones aplicadas**:
> - Ejercicios 4, 5 y 10 de max.md: `newgrp` abre subshell interactiva,
>   bloquea la ejecución del script. Reescritos con `sg` o estructura secuencial.
> - Ejercicio 4 de max.md: usaba el mismo grupo como efectivo y como grupo del
>   directorio, no demostraba que SGID anula al grupo efectivo. Reescrito con
>   grupos distintos.
> - Ejercicio 6 de max.md: caracteres chinos `无效配置` (artefacto de
>   codificación). Eliminados.
> - Ejercicio 8 de max.md: `chmod +t` seguido de `chmod 777` borraba el sticky
>   (777 = 0777, sin bits especiales). Orden corregido.
> - Ejercicio 9 de max.md: `chmod 1747` produce `drwxr--rwt` (others=7=rwx),
>   pero el comentario decía `drwxr-xr-T`. Octal y descripción no coincidían.
>   Reescrito.
> - Lab paso 1.4: la demo de SUID en scripts corría como root dentro del
>   container — `whoami` devolvía root por ser root, no por SUID. Corregido para
>   ejecutar como usuario no-root.
> - Lab paso 3.2: `rm -f` enmascara el mensaje de error del sticky bit.
>   Cambiado a `rm` sin `-f`.

---

## Permisos especiales

Además de rwx, existen tres bits especiales que modifican el comportamiento de
ejecución y creación de archivos:

| Bit | Octal | En archivos | En directorios |
|---|---|---|---|
| SUID | 4000 | Ejecutar como el owner del archivo | (sin efecto en Linux) |
| SGID | 2000 | Ejecutar como el grupo del archivo | Archivos nuevos heredan el grupo del directorio |
| Sticky | 1000 | (sin efecto en Linux moderno) | Solo el owner puede eliminar sus archivos |

Se representan como un cuarto dígito octal (el primero):

```
chmod 4755 file   →  SUID + rwxr-xr-x
chmod 2755 dir    →  SGID + rwxr-xr-x
chmod 1777 dir    →  Sticky + rwxrwxrwx
```

---

## SUID — Set User ID

Cuando un archivo ejecutable tiene SUID activado, se ejecuta con los privilegios
del **owner del archivo**, no del usuario que lo ejecuta.

```bash
ls -la /usr/bin/passwd
# -rwsr-xr-x 1 root root 68208 ... /usr/bin/passwd
#    ^
#    s = SUID activo (x también presente)
```

### Cómo funciona

```bash
# /usr/bin/passwd tiene SUID y pertenece a root
# Cuando dev ejecuta passwd:
# - Real UID      = dev (quién inició el proceso)
# - Effective UID = root (el owner del ejecutable, por SUID)
# - El proceso puede escribir en /etc/shadow (requiere root)
# - Al terminar, los privilegios se eliminan
```

Sin SUID, un usuario regular no podría cambiar su contraseña porque
`/etc/shadow` no es escribible por nadie excepto root.

### SUID en la notación

```bash
# Con x: la s es minúscula
-rwsr-xr-x    # SUID + owner tiene x (configuración normal)

# Sin x: la S es mayúscula (SUID sin ejecución — error de configuración)
-rwSr-xr-x    # SUID activo pero owner NO tiene x — sin sentido práctico
```

### Programas con SUID comunes

```bash
# Buscar archivos SUID en el sistema
find / -perm -4000 -type f 2>/dev/null

# Típicos en la mayoría de distribuciones:
# /usr/bin/passwd      ← Cambiar contraseña (escribe en /etc/shadow)
# /usr/bin/chfn        ← Cambiar info GECOS
# /usr/bin/chsh        ← Cambiar shell
# /usr/bin/newgrp      ← Cambiar grupo efectivo (crea subshell)
# /usr/bin/su          ← Cambiar de usuario
# /usr/bin/sudo        ← Ejecutar como otro usuario
# /usr/bin/mount       ← Montar filesystems (en algunas distros)
# /usr/bin/umount      ← Desmontar filesystems
# /usr/bin/crontab     ← Editar crontab
```

### Riesgos de seguridad

SUID es uno de los vectores de escalamiento de privilegios más explotados:

```bash
# NUNCA aplicar SUID a:
# - Scripts de shell (el kernel los ignora por seguridad, ver abajo)
# - Programas que no han sido auditados
# - Editores de texto (podrían abrir una shell como root: :!bash en vim)
# - Intérpretes (python, perl, bash)

# Auditar regularmente los SUID del sistema
find / -perm -4000 -type f -ls 2>/dev/null
```

### SUID en scripts — ignorado por el kernel

Linux moderno **ignora** el bit SUID en scripts (archivos con `#!`). Es una
protección contra ataques de *race condition* entre el intérprete y el script:

```bash
# El ataque clásico (ya no funciona):
# 1. Script SUID root: #!/bin/bash + código
# 2. El kernel abre el script y lanza bash
# 3. Entre el open() y el exec(), el atacante reemplaza el script
# 4. bash ejecuta el archivo sustituido como root
#
# Solución del kernel: ignorar SUID en scripts
# Si necesitas privilegios en un script, usa sudo con sudoers
```

### Activar y desactivar SUID

```bash
# Activar SUID
chmod u+s program
# o
chmod 4755 program

# Desactivar SUID
chmod u-s program
# o
chmod 0755 program
```

### cp elimina SUID/SGID automáticamente

Al copiar un archivo SUID o SGID, el kernel **elimina** los bits especiales
de la copia. Es una medida de seguridad — si no fuera así, cualquier usuario
podría copiar un binario SUID, modificar la copia y obtener privilegios:

```bash
cp /usr/bin/passwd /tmp/passwd-copy
stat -c '%a' /usr/bin/passwd     # 4755 (SUID)
stat -c '%a' /tmp/passwd-copy    # 755  (SUID eliminado)
```

`mv` dentro del mismo filesystem **preserva** los bits especiales (solo mueve
la entrada de directorio, no copia datos). Pero `mv` entre filesystems
distintos realiza cp + rm, así que los bits se eliminan.

### Opción de montaje `nosuid`

Los filesystems montados con `nosuid` ignoran los bits SUID y SGID de
cualquier ejecutable. Es común en:

```bash
# Particiones temporales (/tmp si es filesystem aparte)
# Dispositivos extraíbles (USB)
# Montajes NFS (por defecto)
# Contenedores Docker (por defecto para montajes bind)

# Verificar
mount | grep nosuid

# Montar con nosuid
mount -o nosuid /dev/sdb1 /mnt/usb
```

---

## SGID — Set Group ID

### En archivos ejecutables

Similar a SUID pero para el grupo: el programa se ejecuta con el **GID
efectivo** del grupo propietario del archivo:

```bash
ls -la /usr/bin/crontab
# -rwxr-sr-x 1 root crontab 43568 ... /usr/bin/crontab
#       ^
#       s = SGID activo

# Cuando dev ejecuta crontab:
# - GID efectivo = grupo crontab
# - Puede leer/escribir en /var/spool/cron/ (propiedad del grupo crontab)
```

### En directorios — Herencia de grupo

SGID en directorios es **mucho más usado** que en archivos. Hace que los
archivos y subdirectorios creados dentro hereden el **grupo del directorio
padre** en vez del grupo primario (o efectivo) del usuario:

```bash
# Sin SGID — archivo hereda grupo primario del usuario
mkdir /tmp/no-sgid
touch /tmp/no-sgid/file.txt
ls -la /tmp/no-sgid/file.txt
# -rw-r--r-- 1 dev dev ... file.txt
#                  ^^^
#           grupo primario de dev

# Con SGID — archivo hereda grupo del directorio
mkdir /tmp/with-sgid
chmod 2775 /tmp/with-sgid
sudo chown root:developers /tmp/with-sgid

touch /tmp/with-sgid/file.txt
ls -la /tmp/with-sgid/file.txt
# -rw-r--r-- 1 dev developers ... file.txt
#                  ^^^^^^^^^^
#           heredó "developers" del directorio
```

**SGID anula al grupo efectivo del proceso.** Incluso si usas `newgrp` para
cambiar tu grupo efectivo a otro grupo, los archivos creados dentro de un
directorio SGID tendrán el grupo del directorio — no tu grupo efectivo
(cubierto en T03 de S02).

### Los subdirectorios heredan el SGID

```bash
mkdir /tmp/with-sgid/subdir
ls -ld /tmp/with-sgid/subdir
# drwxrwsr-x 2 dev developers ... subdir
#       ^
#       s = SGID propagado automáticamente

# La herencia es recursiva: subdir también es SGID,
# así que archivos y sub-subdirectorios dentro también heredarán
```

### Patrón de directorio compartido

El uso estándar de SGID es crear directorios de trabajo colaborativo:

```bash
# Configurar directorio compartido
sudo groupadd project-x
sudo usermod -aG project-x alice
sudo usermod -aG project-x bob

sudo mkdir /srv/project-x
sudo chown root:project-x /srv/project-x
sudo chmod 2775 /srv/project-x

# alice crea un archivo → grupo project-x (por SGID)
# bob puede leer/escribir (permisos de grupo 7)
# subdirectorios nuevos heredan SGID automáticamente
```

Para que la colaboración funcione completamente, los archivos deben tener
permisos de grupo de escritura. Esto depende de la umask del usuario
(cubierto en T04).

---

## Sticky bit

El sticky bit en directorios impide que un usuario elimine o renombre archivos
de **otros usuarios**, incluso si tiene permiso de escritura en el directorio:

```bash
ls -ld /tmp
# drwxrwxrwt 15 root root ... /tmp
#          ^
#          t = sticky bit activo

# /tmp tiene permisos 1777:
# - Todos pueden crear archivos (rwx para others)
# - Solo el owner del archivo puede eliminarlo (sticky bit)
```

### Quién puede eliminar un archivo en un directorio con sticky bit

Con sticky bit activo, un archivo solo puede ser eliminado o renombrado por:

1. El **owner del archivo**
2. El **owner del directorio**
3. **root**

```bash
# Sin sticky bit en /tmp:
# bob podría eliminar archivos de alice (porque tiene w en /tmp)

# Con sticky bit en /tmp:
# bob solo puede eliminar sus propios archivos
su -c "rm /tmp/archivo-de-alice" bob
# rm: cannot remove '/tmp/archivo-de-alice': Operation not permitted
```

### Notación

```bash
# Con x: la t es minúscula
drwxrwxrwt    # Sticky + others tiene x (configuración normal)

# Sin x: la T es mayúscula
drwxrwxrwT    # Sticky activo pero others NO tiene x
```

`T` mayúscula generalmente indica una configuración cuestionable — si others
no puede entrar al directorio (sin `x`), el sticky bit no es muy útil.

### Dónde se usa

```bash
# /tmp — siempre tiene sticky bit (1777)
ls -ld /tmp
# drwxrwxrwt

# /var/tmp — también (1777)
ls -ld /var/tmp
# drwxrwxrwt

# Directorios compartidos con escritura grupal pueden beneficiarse
sudo chmod 3775 /srv/shared
# 3 = SGID (2) + Sticky (1)
# Resultado: grupo compartido + protección contra eliminación
```

### Nota histórica

En Unix original, el sticky bit en archivos regulares significaba "mantener
el texto del programa en swap después de terminar" para acelerar cargas
futuras. De ahí el nombre *sticky* ("pegajoso" — se quedaba en memoria).
Linux moderno ignora el sticky bit en archivos; solo tiene efecto en
directorios.

### Activar sticky bit

```bash
chmod +t /path/to/dir
# o
chmod 1777 /path/to/dir

# Combinar SGID + Sticky
chmod 3775 /path/to/dir
```

---

## El cuarto dígito octal

```
chmod XABC archivo
       ^^^^
       ││││
       │││└── Others
       ││└─── Group
       │└──── Owner
       └───── Special bits (SUID/SGID/Sticky)
```

| X | Bits activos |
|---|---|
| 0 | Ninguno |
| 1 | Sticky |
| 2 | SGID |
| 3 | SGID + Sticky |
| 4 | SUID |
| 5 | SUID + Sticky |
| 6 | SUID + SGID |
| 7 | SUID + SGID + Sticky |

```bash
# Verificar bits especiales
stat -c '%a' /usr/bin/passwd     # 4755 (SUID)
stat -c '%a' /tmp                # 1777 (Sticky)
stat -c '%a' /usr/bin/crontab    # 2755 (SGID)
```

---

## Resumen visual

```
-rwsr-xr-x    SUID: ejecutar como el owner
-rwxr-sr-x    SGID en archivo: ejecutar como el grupo
drwxrwsr-x    SGID en directorio: archivos heredan grupo del directorio
drwxrwxrwt    Sticky: solo el owner puede eliminar sus archivos
```

## Tabla comparativa

| Bit | Octal | Simbólico | En archivo | En directorio |
|---|---|---|---|---|
| SUID | 4000 | `u+s` | Ejecuta como owner | Sin efecto |
| SGID | 2000 | `g+s` | Ejecuta como grupo | Archivos heredan grupo |
| Sticky | 1000 | `+t` | Sin efecto | Protege contra eliminación |

---

## Labs

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

### Parte 1 — SUID

#### Paso 1.1: Encontrar archivos SUID

```bash
docker compose exec debian-dev bash -c '
echo "=== Archivos con SUID ==="
find / -perm -4000 -type f -ls 2>/dev/null
'
```

La `s` minúscula en la posición de `x` del owner indica SUID activo.
Estos programas se ejecutan con los privilegios del owner (generalmente root).

#### Paso 1.2: passwd como ejemplo de SUID

```bash
docker compose exec debian-dev bash -c '
echo "=== /usr/bin/passwd ==="
ls -la /usr/bin/passwd
stat -c "Permisos: %a" /usr/bin/passwd
echo "(4755 = SUID + rwxr-xr-x)"

echo ""
echo "=== Por qué necesita SUID ==="
echo "passwd modifica /etc/shadow"
ls -la /etc/shadow
echo "Solo root puede escribir en /etc/shadow"
echo "Sin SUID, un usuario no podría cambiar su contraseña"
'
```

#### Paso 1.3: S mayúscula vs s minúscula

```bash
docker compose exec debian-dev bash -c '
echo "=== s minúscula = SUID + x ==="
touch /tmp/suid-test
chmod 4755 /tmp/suid-test
ls -la /tmp/suid-test
echo "(s = SUID activo, x también presente)"

echo ""
echo "=== S mayúscula = SUID sin x (error de configuración) ==="
chmod 4644 /tmp/suid-test
ls -la /tmp/suid-test
echo "(S = SUID activo pero SIN ejecución — sin sentido práctico)"

rm -f /tmp/suid-test
'
```

#### Paso 1.4: SUID en scripts — el kernel lo ignora

```bash
docker compose exec debian-dev bash -c '
echo "=== SUID en scripts (ignorado por el kernel) ==="
useradd -m scripttest 2>/dev/null

# Crear script SUID root
cat > /tmp/suid-script.sh << "SCRIPT"
#!/bin/bash
echo "whoami: $(whoami)"
echo "id: $(id)"
SCRIPT
chmod 4755 /tmp/suid-script.sh
chown root:root /tmp/suid-script.sh

echo "--- Permisos del script ---"
ls -la /tmp/suid-script.sh

echo ""
echo "--- Ejecutado por scripttest (no root) ---"
su -c "/tmp/suid-script.sh" scripttest
echo ""
echo "Si SUID funcionara en scripts, whoami diría root."
echo "Pero el kernel ignora SUID en scripts (#!) por seguridad."

rm -f /tmp/suid-script.sh
userdel -r scripttest 2>/dev/null
'
```

El kernel ignora SUID en scripts como protección contra *race conditions*.
Si necesitas ejecutar un script con privilegios, usa `sudo` con sudoers.

#### Paso 1.5: Comparar SUID entre distribuciones

```bash
echo "=== Debian: archivos SUID ==="
docker compose exec debian-dev bash -c 'find /usr -perm -4000 -type f 2>/dev/null | sort'

echo ""
echo "=== AlmaLinux: archivos SUID ==="
docker compose exec alma-dev bash -c 'find /usr -perm -4000 -type f 2>/dev/null | sort'
```

La lista puede variar entre distribuciones. Auditarla regularmente es una
práctica de seguridad.

---

### Parte 2 — SGID en directorios

#### Paso 2.1: Sin SGID — archivo hereda grupo del usuario

```bash
docker compose exec debian-dev bash -c '
echo "=== Sin SGID ==="
mkdir -p /tmp/no-sgid
touch /tmp/no-sgid/file.txt
ls -la /tmp/no-sgid/file.txt
echo "(grupo = grupo primario del usuario)"

rm -rf /tmp/no-sgid
'
```

#### Paso 2.2: Con SGID — archivo hereda grupo del directorio

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear grupo y directorio con SGID ==="
groupadd -f sgidteam
mkdir -p /tmp/with-sgid
chown root:sgidteam /tmp/with-sgid
chmod 2775 /tmp/with-sgid
ls -ld /tmp/with-sgid
echo "(la s en group = SGID)"

echo ""
echo "=== Crear archivo dentro ==="
touch /tmp/with-sgid/file.txt
ls -la /tmp/with-sgid/file.txt
echo "(grupo = sgidteam, heredado del directorio)"
'
```

#### Paso 2.3: Subdirectorios heredan SGID

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear subdirectorio ==="
mkdir /tmp/with-sgid/subdir
ls -ld /tmp/with-sgid/subdir
echo "(s en group = el subdirectorio también tiene SGID)"

echo ""
echo "=== Crear archivo en el subdirectorio ==="
touch /tmp/with-sgid/subdir/nested.txt
ls -la /tmp/with-sgid/subdir/nested.txt
echo "(grupo = sgidteam — la herencia se propaga)"

rm -rf /tmp/with-sgid
groupdel sgidteam 2>/dev/null
'
```

#### Paso 2.4: Archivos SGID (ejecutables)

```bash
docker compose exec debian-dev bash -c '
echo "=== Buscar archivos con SGID ==="
find /usr -perm -2000 -type f -ls 2>/dev/null | head -5
echo ""
echo "SGID en ejecutables: se ejecutan con el GID del grupo del archivo"
echo "Ejemplo típico: crontab (accede a /var/spool/cron/)"
'
```

---

### Parte 3 — Sticky bit

#### Paso 3.1: Sticky bit en /tmp

```bash
docker compose exec debian-dev bash -c '
echo "=== Permisos de /tmp ==="
ls -ld /tmp
stat -c "Octal: %a" /tmp

echo ""
echo "La t al final indica sticky bit activo"
echo "1777 = sticky (1) + rwxrwxrwx (777)"

echo ""
echo "Efecto: solo el owner del archivo puede eliminarlo"
echo "(incluso si otros tienen w en el directorio)"
'
```

#### Paso 3.2: Verificar protección del sticky bit

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear dos usuarios ==="
useradd -m sticky1 2>/dev/null
useradd -m sticky2 2>/dev/null

echo ""
echo "=== sticky1 crea un archivo en /tmp ==="
su -c "touch /tmp/sticky-test" sticky1
ls -la /tmp/sticky-test

echo ""
echo "=== sticky2 intenta eliminar ==="
su -c "rm /tmp/sticky-test" sticky2 2>&1 || true

echo ""
echo "=== ¿El archivo sigue existiendo? ==="
ls -la /tmp/sticky-test 2>/dev/null \
  && echo "SÍ (protegido por sticky bit)" \
  || echo "NO (fue eliminado)"

rm -f /tmp/sticky-test
userdel -r sticky1 2>/dev/null
userdel -r sticky2 2>/dev/null
'
```

Con sticky bit, solo pueden eliminar un archivo: su owner, el owner del
directorio, y root.

#### Paso 3.3: t vs T

```bash
docker compose exec debian-dev bash -c '
echo "=== t minúscula = sticky + x para others ==="
mkdir /tmp/sticky-t
chmod 1777 /tmp/sticky-t
ls -ld /tmp/sticky-t

echo ""
echo "=== T mayúscula = sticky sin x para others ==="
chmod 1776 /tmp/sticky-t
ls -ld /tmp/sticky-t
echo "(T = sticky activo pero others no tiene x)"

rm -rf /tmp/sticky-t
'
```

#### Paso 3.4: Combinar SGID + sticky

```bash
docker compose exec debian-dev bash -c '
echo "=== Directorio con SGID + sticky (3775) ==="
groupadd -f combiteam
mkdir /tmp/combi-dir
chown root:combiteam /tmp/combi-dir
chmod 3775 /tmp/combi-dir

echo ""
ls -ld /tmp/combi-dir
stat -c "Octal: %a" /tmp/combi-dir

echo ""
echo "3 = SGID (2) + Sticky (1)"
echo "Resultado:"
echo "  - Archivos heredan grupo del directorio (SGID)"
echo "  - Solo el owner puede eliminar su archivo (Sticky)"

rm -rf /tmp/combi-dir
groupdel combiteam 2>/dev/null
'
```

#### Paso 3.5: cp elimina bits especiales

```bash
docker compose exec debian-dev bash -c '
echo "=== Original: passwd con SUID ==="
stat -c "%a %n" /usr/bin/passwd

echo ""
echo "=== Copiar passwd ==="
cp /usr/bin/passwd /tmp/passwd-copy
stat -c "%a %n" /tmp/passwd-copy
echo "(SUID eliminado automáticamente por seguridad)"

rm -f /tmp/passwd-copy
'
```

---

### Limpieza final

```bash
docker compose exec debian-dev bash -c '
userdel -r sticky1 2>/dev/null
userdel -r sticky2 2>/dev/null
userdel -r scripttest 2>/dev/null
groupdel sgidteam 2>/dev/null
groupdel combiteam 2>/dev/null
rm -rf /tmp/no-sgid /tmp/with-sgid /tmp/sticky-t /tmp/combi-dir 2>/dev/null
rm -f /tmp/suid-test /tmp/suid-script.sh /tmp/sticky-test /tmp/passwd-copy 2>/dev/null
echo "Limpieza completada"
'
```

---

## Ejercicios

### Ejercicio 1 — Inventario de bits especiales

Ejecuta estos tres comandos y responde:

```bash
find /usr -perm -4000 -type f 2>/dev/null | wc -l
find /usr -perm -2000 -type f 2>/dev/null | wc -l
find / -perm -1000 -type d 2>/dev/null | wc -l
```

**Pregunta:** ¿Por qué hay significativamente más archivos SUID que SGID en
la mayoría de sistemas? ¿Y por qué los directorios con sticky bit son pocos?

<details><summary>Predicción</summary>

Hay más archivos SUID porque muchos comandos de administración de usuarios
necesitan acceso como root (`passwd`, `su`, `sudo`, `mount`, `chsh`, `chfn`,
`newgrp`, etc.). SGID en archivos es menos común porque pocas aplicaciones
necesitan ejecutar con un GID específico (principalmente `crontab` para
acceder a `/var/spool/cron/`).

Los directorios con sticky bit son pocos porque solo se necesita en directorios
world-writable (como `/tmp` y `/var/tmp`), y tener directorios world-writable
es en sí una práctica poco frecuente.

</details>

---

### Ejercicio 2 — Verificar SUID con /proc

Ejecuta como usuario no-root:

```bash
# Desde dentro del container:
useradd -m suidcheck 2>/dev/null

su -c '
  echo "=== Antes de ejecutar SUID ==="
  cat /proc/self/status | grep -E "^(Uid|Gid):"
  echo ""
  echo "Real UID = Effective UID (ambos son suidcheck)"
' suidcheck
```

**Pregunta:** Los campos Uid y Gid en `/proc/self/status` muestran cuatro
valores: Real, Effective, SavedSet, Filesystem. ¿Qué cambiaría en el
Effective UID si este proceso fuera lanzado por un binario SUID root?

<details><summary>Predicción</summary>

Los cuatro valores en `/proc/self/status` para Uid son:
`Real  Effective  SavedSet  Filesystem`

Para un proceso normal de `suidcheck` (UID=1001 por ejemplo):
```
Uid:  1001  1001  1001  1001
```

Si el proceso fuera lanzado desde un binario SUID root:
```
Uid:  1001  0  0  0
```

- **Real** = 1001 (quién ejecutó el programa — no cambia)
- **Effective** = 0 (root, por SUID — determina los permisos)
- **SavedSet** = 0 (copia del Effective al inicio, permite restaurar con `seteuid()`)
- **Filesystem** = 0 (sigue al Effective, usado para verificar acceso a archivos)

</details>

---

### Ejercicio 3 — SUID en scripts: demostrar que el kernel lo ignora

```bash
useradd -m scriptuser 2>/dev/null

cat > /tmp/test-suid.sh << 'SCRIPT'
#!/bin/bash
echo "whoami: $(whoami)"
echo "EUID: $EUID"
SCRIPT

chown root:root /tmp/test-suid.sh
chmod 4755 /tmp/test-suid.sh
ls -la /tmp/test-suid.sh

su -c '/tmp/test-suid.sh' scriptuser
```

**Pregunta:** ¿Qué muestra `whoami` y `$EUID`? ¿El SUID funcionó?

<details><summary>Predicción</summary>

```
whoami: scriptuser
EUID: 1001       (o el UID que tenga scriptuser)
```

El SUID **no** funcionó. El kernel de Linux ignora el bit SUID en archivos
que comienzan con `#!` (shebang). Esto es una protección de seguridad contra
*race conditions* entre la apertura del script y la ejecución del intérprete.

El bit `s` sigue visible con `ls`, pero el kernel no lo respeta. Solo los
binarios compilados (ELF) pueden aprovechar SUID.

Si necesitas que un script ejecute algo como root, las alternativas son:
- `sudo` con una entrada en sudoers
- Un binario wrapper compilado (no recomendado — complejo y riesgoso)

</details>

---

### Ejercicio 4 — SGID en directorio: demostrar que anula al grupo efectivo

```bash
groupadd -f teamalpha
groupadd -f teambeta
useradd -m sgiduser -g teamalpha 2>/dev/null
usermod -aG teambeta sgiduser

# Directorio SGID con grupo teambeta
mkdir -p /tmp/sgid-override
chown root:teambeta /tmp/sgid-override
chmod 2775 /tmp/sgid-override

# sgiduser tiene grupo primario teamalpha
# Crear archivo dentro del directorio SGID
su -c 'touch /tmp/sgid-override/file1.txt' sgiduser

# Verificar
ls -la /tmp/sgid-override/file1.txt
stat -c 'Owner: %U  Group: %G' /tmp/sgid-override/file1.txt
```

**Pregunta:** ¿Qué grupo tiene `file1.txt`? ¿`teamalpha` (grupo primario de
sgiduser) o `teambeta` (grupo del directorio)?

<details><summary>Predicción</summary>

El grupo de `file1.txt` es **`teambeta`** — el grupo del directorio, no el
grupo primario del usuario.

El bit SGID en directorios **siempre** anula el comportamiento por defecto.
Sin importar cuál sea el grupo primario o efectivo del proceso que crea el
archivo, el nuevo archivo hereda el grupo del directorio SGID.

Esto es lo que hace SGID tan útil para directorios compartidos: garantiza
que todos los archivos creados dentro pertenezcan al grupo correcto,
independientemente de la configuración de grupos de cada usuario.

</details>

---

### Ejercicio 5 — Propagación recursiva de SGID

```bash
groupadd -f projteam
mkdir -p /tmp/sgid-prop
chown root:projteam /tmp/sgid-prop
chmod 2775 /tmp/sgid-prop

# Crear jerarquía de subdirectorios
mkdir -p /tmp/sgid-prop/src/components
touch /tmp/sgid-prop/src/main.c
touch /tmp/sgid-prop/src/components/header.h

# Verificar
ls -ld /tmp/sgid-prop/src
ls -ld /tmp/sgid-prop/src/components
ls -la /tmp/sgid-prop/src/main.c
ls -la /tmp/sgid-prop/src/components/header.h
stat -c '%a %n' /tmp/sgid-prop/src /tmp/sgid-prop/src/components
```

**Pregunta:** ¿Los subdirectorios `src/` y `src/components/` tienen SGID?
¿Los archivos `main.c` y `header.h` también tienen SGID?

<details><summary>Predicción</summary>

Los **subdirectorios** sí heredan SGID:
```
drwxrwsr-x  projteam  /tmp/sgid-prop/src          (2775)
drwxrwsr-x  projteam  /tmp/sgid-prop/src/components (2775)
```

Los **archivos** NO tienen SGID (no se propaga a archivos regulares), pero
sí heredan el **grupo** del directorio:
```
-rw-r--r--  projteam  main.c       (644)
-rw-r--r--  projteam  header.h     (644)
```

La propagación funciona así:
- Subdirectorios: heredan **grupo** + **bit SGID** del directorio padre
- Archivos: heredan solo el **grupo** del directorio padre, no el bit SGID

Esto garantiza que toda la jerarquía mantenga la herencia de grupo
indefinidamente.

</details>

---

### Ejercicio 6 — Sticky bit: protección entre usuarios

```bash
useradd -m alice 2>/dev/null
useradd -m bob 2>/dev/null

# Crear directorio world-writable CON sticky
mkdir -p /tmp/sticky-yes
chmod 1777 /tmp/sticky-yes

# Crear directorio world-writable SIN sticky
mkdir -p /tmp/sticky-no
chmod 0777 /tmp/sticky-no

# alice crea archivos en ambos
su -c 'touch /tmp/sticky-yes/alice-file' alice
su -c 'touch /tmp/sticky-no/alice-file' alice

# bob intenta eliminar
su -c 'rm /tmp/sticky-yes/alice-file' bob 2>&1; echo "exit: $?"
su -c 'rm /tmp/sticky-no/alice-file' bob 2>&1; echo "exit: $?"

# Verificar
ls /tmp/sticky-yes/ 2>/dev/null
ls /tmp/sticky-no/ 2>/dev/null
```

**Pregunta:** ¿En cuál directorio bob pudo eliminar el archivo de alice?

<details><summary>Predicción</summary>

- **`/tmp/sticky-no/`** (sin sticky): bob **SÍ** pudo eliminar `alice-file`.
  Tiene permiso `w` en el directorio (777), lo que le permite eliminar
  cualquier entrada.
  ```
  rm: éxito (exit: 0)
  ls /tmp/sticky-no/ → (vacío)
  ```

- **`/tmp/sticky-yes/`** (con sticky): bob **NO** pudo eliminar `alice-file`.
  Aunque tiene `w` en el directorio, el sticky bit solo permite eliminar
  archivos que le pertenecen.
  ```
  rm: cannot remove: Operation not permitted (exit: 1)
  ls /tmp/sticky-yes/ → alice-file
  ```

El sticky bit existe precisamente para este escenario: directorios donde
todos pueden escribir pero nadie puede borrar archivos ajenos.

</details>

---

### Ejercicio 7 — Combinar SGID + sticky (chmod 3775)

```bash
groupadd -f devteam
useradd -m dev1 -G devteam 2>/dev/null
useradd -m dev2 -G devteam 2>/dev/null

mkdir -p /tmp/shared-project
chown root:devteam /tmp/shared-project
chmod 3775 /tmp/shared-project

# dev1 crea un archivo
su -c 'touch /tmp/shared-project/dev1-code.py' dev1
ls -la /tmp/shared-project/dev1-code.py

# dev2 intenta eliminar el archivo de dev1
su -c 'rm /tmp/shared-project/dev1-code.py' dev2 2>&1; echo "exit: $?"

# dev2 crea su propio archivo
su -c 'touch /tmp/shared-project/dev2-code.py' dev2
ls -la /tmp/shared-project/

# dev2 elimina su propio archivo
su -c 'rm /tmp/shared-project/dev2-code.py' dev2 2>&1; echo "exit: $?"
```

**Pregunta:** ¿Qué grupo tienen los archivos? ¿Puede dev2 eliminar el archivo
de dev1? ¿Y el suyo propio?

<details><summary>Predicción</summary>

Grupo de los archivos: **devteam** (heredado por SGID del directorio).

```
-rw-r--r-- 1 dev1 devteam ... dev1-code.py
-rw-r--r-- 1 dev2 devteam ... dev2-code.py
```

- dev2 **NO** puede eliminar `dev1-code.py` → "Operation not permitted"
  (sticky bit protege: solo el owner del archivo, el owner del directorio, o
  root pueden eliminar)
- dev2 **SÍ** puede eliminar `dev2-code.py` → es su propio archivo

La combinación 3775 es el patrón ideal para directorios de proyecto:
- **SGID** (2): todos los archivos pertenecen al grupo del proyecto
- **Sticky** (1): cada desarrollador solo puede borrar sus propios archivos
- **775**: el grupo puede leer/escribir/entrar, others solo leer/entrar

</details>

---

### Ejercicio 8 — cp elimina SUID, mv lo preserva

```bash
# Copiar un binario SUID
cp /usr/bin/passwd /tmp/passwd-cp
stat -c '%a %n' /usr/bin/passwd /tmp/passwd-cp

echo "---"

# Comparar: crear un archivo SUID y moverlo (mismo filesystem)
touch /tmp/suid-original
chmod 4755 /tmp/suid-original
stat -c '%a %n' /tmp/suid-original

mv /tmp/suid-original /tmp/suid-moved
stat -c '%a %n' /tmp/suid-moved
```

**Pregunta:** ¿`cp` preservó los bits especiales? ¿Y `mv`?

<details><summary>Predicción</summary>

```
4755 /usr/bin/passwd
755  /tmp/passwd-cp         ← cp ELIMINÓ el SUID
```

```
4755 /tmp/suid-original     ← antes del mv
4755 /tmp/suid-moved        ← mv PRESERVÓ el SUID
```

**`cp` elimina SUID/SGID automáticamente.** Es una medida de seguridad del
kernel: si un usuario pudiera copiar un binario SUID, modificar la copia y
mantener SUID, podría obtener privilegios.

**`mv` dentro del mismo filesystem preserva** los bits especiales porque solo
actualiza la entrada del directorio (no copia datos, no crea nuevo inode).

Si `mv` mueve entre filesystems distintos, internamente realiza `cp` + `rm`,
así que los bits especiales se perderían.

> **Nota:** Incluso `cp --preserve=mode` (o `cp -p`) preserva SUID/SGID solo
> si lo ejecuta root. Un usuario normal no puede crear copias SUID de
> binarios ajenos.

</details>

---

### Ejercicio 9 — S y T mayúsculas: bits especiales sin x

```bash
# Caso 1: SUID sin x
touch /tmp/suid-nox
chmod 4644 /tmp/suid-nox
ls -la /tmp/suid-nox

# Caso 2: SGID sin x en grupo
touch /tmp/sgid-nox
chmod 2654 /tmp/sgid-nox
ls -la /tmp/sgid-nox

# Caso 3: Sticky sin x en others
mkdir /tmp/sticky-nox
chmod 1776 /tmp/sticky-nox
ls -ld /tmp/sticky-nox
```

**Pregunta:** ¿Qué letras mayúsculas aparecen en cada caso y en qué posición?

<details><summary>Predicción</summary>

```
-rwSr--r--  /tmp/suid-nox       ← S en posición 4 (owner x)
-rw-r-Sr--  /tmp/sgid-nox       ← S en posición 7 (group x)
drwxrwxrwT  /tmp/sticky-nox     ← T en posición 10 (others x)
```

Las letras **mayúsculas** indican que el bit especial está activo pero el bit
`x` correspondiente **no** está presente:

| Bit | Con x | Sin x | Posición |
|---|---|---|---|
| SUID | `s` | `S` | Owner execute (posición 4) |
| SGID | `s` | `S` | Group execute (posición 7) |
| Sticky | `t` | `T` | Others execute (posición 10) |

**`S` mayúscula en SUID/SGID es generalmente un error** — sin `x` el archivo
no es ejecutable, así que el bit especial no tiene efecto.

**`T` mayúscula en sticky puede tener sentido** en algunos casos (directorio
donde others no tiene `x` pero el sticky protege archivos de miembros del
grupo), aunque es poco común.

</details>

---

### Ejercicio 10 — Auditoría: directorios world-writable sin sticky bit

```bash
find / -type d -perm -0002 ! -perm -1000 -ls 2>/dev/null
```

**Pregunta:** ¿Qué busca este comando? ¿Por qué es un hallazgo de seguridad?

<details><summary>Predicción</summary>

El comando busca directorios que son:
- `-perm -0002`: world-writable (others tiene `w`)
- `! -perm -1000`: **sin** sticky bit

Esta combinación es un **riesgo de seguridad**: cualquier usuario puede
eliminar o renombrar archivos de otros usuarios dentro de ese directorio.

- `-perm -0002` busca directorios donde others tiene permiso de escritura
  (el `-` significa "al menos estos bits")
- `! -perm -1000` excluye directorios que ya tienen sticky bit

En un sistema bien configurado, este comando no debería devolver resultados
(o solo directorios en filesystems temporales/proc/sys). Si encuentra
directorios en `/srv/`, `/var/`, u otras ubicaciones de datos, es un
problema que debe corregirse con `chmod +t`.

Equivalentes útiles para auditoría:
```bash
# Solo en particiones reales (excluir proc, sys, etc.)
find / -type d -perm -0002 ! -perm -1000 \
  -not -path "/proc/*" -not -path "/sys/*" \
  -not -path "/dev/*" -ls 2>/dev/null
```

</details>

---

## Limpieza de ejercicios

```bash
userdel -r suidcheck 2>/dev/null
userdel -r scriptuser 2>/dev/null
userdel -r sgiduser 2>/dev/null
userdel -r alice 2>/dev/null
userdel -r bob 2>/dev/null
userdel -r dev1 2>/dev/null
userdel -r dev2 2>/dev/null
groupdel teamalpha 2>/dev/null
groupdel teambeta 2>/dev/null
groupdel projteam 2>/dev/null
groupdel devteam 2>/dev/null
rm -rf /tmp/sgid-override /tmp/sgid-prop /tmp/sticky-yes /tmp/sticky-no 2>/dev/null
rm -rf /tmp/shared-project /tmp/suid-nox /tmp/sgid-nox /tmp/sticky-nox 2>/dev/null
rm -f /tmp/passwd-cp /tmp/suid-moved /tmp/test-suid.sh 2>/dev/null
echo "Limpieza completada"
```
