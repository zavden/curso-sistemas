# T03 — SUID, SGID, Sticky bit

## Permisos especiales

Además de rwx, existen tres bits especiales que modifican el comportamiento de
ejecución y creación de archivos:

| Bit | Octal | En archivos | En directorios |
|---|---|---|---|
| SUID | 4000 | Ejecutar como el owner del archivo | (sin efecto) |
| SGID | 2000 | Ejecutar como el grupo del archivo | Archivos nuevos heredan el grupo del directorio |
| Sticky | 1000 | (sin efecto práctico) | Solo el owner puede eliminar sus archivos |

Se representan como un cuarto dígito octal (el primero):

```
chmod 4755 file   →  SUID + rwxr-xr-x
chmod 2755 dir    →  SGID + rwxr-xr-x
chmod 1777 dir    →  Sticky + rwxrwxrwx
```

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
# - El proceso se crea con UID efectivo = 0 (root)
# - Puede escribir en /etc/shadow (que requiere root)
# - Al terminar, los privilegios se eliminan
```

Sin SUID, un usuario regular no podría cambiar su contraseña porque
`/etc/shadow` no es escribible por nadie excepto root.

### SUID en la notación

```bash
# Con x: la s es minúscula
-rwsr-xr-x    # SUID + owner tiene x

# Sin x: la S es mayúscula (SUID sin ejecución — sin sentido práctico)
-rwSr-xr-x    # SUID pero owner NO tiene x (error de configuración)
```

### Programas con SUID comunes

```bash
# Buscar archivos SUID en el sistema
find / -perm -4000 -type f 2>/dev/null
# /usr/bin/passwd      ← Cambiar contraseña
# /usr/bin/chfn        ← Cambiar info GECOS
# /usr/bin/chsh        ← Cambiar shell
# /usr/bin/newgrp      ← Cambiar grupo primario
# /usr/bin/su          ← Cambiar de usuario
# /usr/bin/sudo        ← Ejecutar como otro usuario
# /usr/bin/mount       ← Montar filesystems (en algunas distros)
# /usr/bin/umount      ← Desmontar filesystems
# /usr/bin/crontab     ← Editar crontab
# /usr/lib/openssh/ssh-keysign  ← Autenticación SSH host-based
```

### Riesgos de seguridad

SUID es uno de los vectores de escalamiento de privilegios más explotados:

```bash
# Si un programa SUID tiene una vulnerabilidad, un usuario regular puede
# obtener una shell como root

# NUNCA aplicar SUID a:
# - Scripts de shell (la mayoría de kernels lo ignoran por seguridad)
# - Programas que no han sido auditados
# - Editores de texto (podrían abrir una shell como root)

# Auditar regularmente los SUID del sistema
find / -perm -4000 -type f -ls 2>/dev/null
```

Los scripts (bash, python, perl) con SUID son ignorados por el kernel en Linux
moderno — es una protección contra ataques de race condition entre el intérprete
y el script.

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

## SGID — Set Group ID

### En archivos ejecutables

Similar a SUID pero para el grupo: el programa se ejecuta con el GID del grupo
propietario del archivo:

```bash
ls -la /usr/bin/crontab
# -rwxr-sr-x 1 root crontab 43568 ... /usr/bin/crontab
#       ^
#       s = SGID activo

# Cuando dev ejecuta crontab:
# - GID efectivo = grupo crontab
# - Puede leer/escribir en /var/spool/cron/ (propiedad de crontab)
```

### En directorios — Herencia de grupo

SGID en directorios es mucho más usado que en archivos. Hace que los archivos
y subdirectorios creados dentro hereden el **grupo del directorio padre** en
vez del grupo primario del usuario:

```bash
# Sin SGID
mkdir /tmp/no-sgid
touch /tmp/no-sgid/file.txt
ls -la /tmp/no-sgid/file.txt
# -rw-r--r-- 1 dev dev ... file.txt
#                  ^^^
#           grupo primario de dev

# Con SGID
mkdir /tmp/with-sgid
chmod 2775 /tmp/with-sgid
sudo chown root:developers /tmp/with-sgid

touch /tmp/with-sgid/file.txt
ls -la /tmp/with-sgid/file.txt
# -rw-r--r-- 1 dev developers ... file.txt
#                  ^^^^^^^^^^
#           heredó "developers" del directorio

# Los subdirectorios también heredan el SGID
mkdir /tmp/with-sgid/subdir
ls -ld /tmp/with-sgid/subdir
# drwxrwsr-x 2 dev developers ... subdir
#       ^
#       s = SGID propagado al subdirectorio
```

### Patrón de directorio compartido

El uso estándar de SGID es directorios de trabajo colaborativo:

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

## Sticky bit

El sticky bit en directorios impide que un usuario elimine archivos de **otros
usuarios**, incluso si tiene permiso de escritura en el directorio:

```bash
ls -ld /tmp
# drwxrwxrwt 15 root root ... /tmp
#          ^
#          t = sticky bit activo

# /tmp tiene permisos 1777:
# - Todos pueden crear archivos (rwx para others)
# - Solo el owner del archivo puede eliminarlo (sticky bit)
```

### Cómo funciona

Con sticky bit, un archivo en el directorio solo puede ser eliminado o
renombrado por:
- El owner del archivo
- El owner del directorio
- root

```bash
# Sin sticky bit en /tmp:
# bob podría eliminar archivos de alice (porque tiene w en /tmp)

# Con sticky bit en /tmp:
# bob solo puede eliminar sus propios archivos
rm /tmp/archivo-de-alice
# rm: cannot remove '/tmp/archivo-de-alice': Operation not permitted
```

### Notación

```bash
# Con x: la t es minúscula
drwxrwxrwt    # Sticky + others tiene x

# Sin x: la T es mayúscula
drwxrwxrwT    # Sticky pero others NO tiene x
```

### Dónde se usa

```bash
# /tmp — siempre tiene sticky bit
ls -ld /tmp
# drwxrwxrwt

# /var/tmp — también
ls -ld /var/tmp
# drwxrwxrwt

# Directorios compartidos con escritura grupal pueden beneficiarse
sudo chmod 3775 /srv/shared
# 3 = SGID (2) + Sticky (1)
# Resultado: grupo compartido + protección contra eliminación
```

### Activar sticky bit

```bash
chmod +t /path/to/dir
# o
chmod 1777 /path/to/dir

# Combinar SGID + Sticky
chmod 3775 /path/to/dir
```

## El cuarto dígito octal

```
chmod XABC archivo
       ^^^
       │││
       ││└── Others
       │└─── Group
       └──── Owner
      X = special bits
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
stat -c '%a' /usr/bin/passwd
# 4755 (SUID)

stat -c '%a' /tmp
# 1777 (Sticky)

stat -c '%a' /usr/bin/crontab
# 2755 (SGID)
```

## Resumen visual

```
-rwsr-xr-x    SUID: ejecutar como el owner
-rwxr-sr-x    SGID en archivo: ejecutar como el grupo
drwxrwsr-x    SGID en directorio: archivos heredan grupo del directorio
drwxrwxrwt    Sticky: solo el owner puede eliminar sus archivos
```

---

## Ejercicios

### Ejercicio 1 — Encontrar SUID y SGID

```bash
# Buscar archivos SUID en el sistema
find /usr -perm -4000 -type f -ls 2>/dev/null

# Buscar archivos SGID
find /usr -perm -2000 -type f -ls 2>/dev/null

# Buscar directorios con sticky bit
find / -perm -1000 -type d -ls 2>/dev/null | head -5
```

### Ejercicio 2 — Probar SGID en directorios

```bash
# Crear directorio con SGID
sudo mkdir /tmp/sgid-test
sudo chgrp $(id -gn) /tmp/sgid-test
sudo chmod 2775 /tmp/sgid-test

# Crear archivo — ¿qué grupo tiene?
touch /tmp/sgid-test/file1
ls -la /tmp/sgid-test/file1

# Crear subdirectorio — ¿hereda SGID?
mkdir /tmp/sgid-test/subdir
ls -ld /tmp/sgid-test/subdir

# Limpiar
rm -rf /tmp/sgid-test
```

### Ejercicio 3 — Probar sticky bit

```bash
# Verificar que /tmp tiene sticky
stat -c '%a' /tmp

# Crear un archivo como tu usuario
touch /tmp/my-sticky-test

# ¿Otro usuario podría eliminarlo?
# (En un entorno con dos usuarios, probar:)
# su - otheruser -c "rm /tmp/my-sticky-test"
# rm: cannot remove: Operation not permitted

rm /tmp/my-sticky-test
```
