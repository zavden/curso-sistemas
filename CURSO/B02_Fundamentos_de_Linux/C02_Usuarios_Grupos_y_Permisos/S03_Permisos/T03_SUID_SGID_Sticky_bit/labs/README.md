# Lab — SUID, SGID, Sticky bit

## Objetivo

Explorar los tres bits especiales: encontrar programas con SUID,
configurar directorios con SGID para herencia de grupo, y verificar
la proteccion del sticky bit en /tmp.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — SUID

### Objetivo

Encontrar programas con SUID, entender como funcionan, y conocer
los riesgos de seguridad.

### Paso 1.1: Encontrar archivos SUID

```bash
docker compose exec debian-dev bash -c '
echo "=== Archivos con SUID ==="
find / -perm -4000 -type f -ls 2>/dev/null
'
```

La `s` minuscula en la posicion de `x` del owner indica SUID activo.
Estos programas se ejecutan con los privilegios del owner (root).

### Paso 1.2: passwd como ejemplo de SUID

```bash
docker compose exec debian-dev bash -c '
echo "=== /usr/bin/passwd ==="
ls -la /usr/bin/passwd
echo ""
stat -c "Permisos: %a" /usr/bin/passwd
echo "(4755 = SUID + rwxr-xr-x)"

echo ""
echo "=== Por que necesita SUID ==="
echo "passwd modifica /etc/shadow"
ls -la /etc/shadow
echo "Solo root puede escribir en /etc/shadow"
echo "Sin SUID, un usuario no podria cambiar su contrasena"
'
```

`passwd` tiene SUID porque necesita escribir en `/etc/shadow`, que
solo es accesible por root. SUID hace que se ejecute como root
aunque lo lance un usuario regular.

### Paso 1.3: S mayuscula vs s minuscula

```bash
docker compose exec debian-dev bash -c '
echo "=== s minuscula = SUID + x ==="
touch /tmp/suid-test
chmod 4755 /tmp/suid-test
ls -la /tmp/suid-test
echo "(s = SUID activo, x tambien presente)"

echo ""
echo "=== S mayuscula = SUID sin x (sin sentido) ==="
chmod 4644 /tmp/suid-test
ls -la /tmp/suid-test
echo "(S = SUID activo pero SIN ejecucion — error de configuracion)"

rm -f /tmp/suid-test
'
```

`S` mayuscula indica que SUID esta activo pero el bit `x` no. Es
una configuracion sin sentido (el archivo no es ejecutable).

### Paso 1.4: SUID en scripts

```bash
docker compose exec debian-dev bash -c '
echo "=== SUID en scripts (ignorado por seguridad) ==="
echo "#!/bin/bash" > /tmp/suid-script.sh
echo "whoami" >> /tmp/suid-script.sh
chmod 4755 /tmp/suid-script.sh
chown root /tmp/suid-script.sh

echo ""
echo "--- El kernel ignora SUID en scripts ---"
/tmp/suid-script.sh 2>/dev/null || echo "(puede no ejecutarse)"
echo ""
echo "Linux moderno ignora SUID en scripts (#!) por seguridad"
echo "(proteccion contra race conditions)"

rm -f /tmp/suid-script.sh
'
```

### Paso 1.5: Comparar SUID entre distribuciones

```bash
echo "=== Debian: archivos SUID ==="
docker compose exec debian-dev bash -c 'find /usr -perm -4000 -type f 2>/dev/null | sort'

echo ""
echo "=== AlmaLinux: archivos SUID ==="
docker compose exec alma-dev bash -c 'find /usr -perm -4000 -type f 2>/dev/null | sort'
```

La lista de programas SUID puede variar entre distribuciones.
Auditarla regularmente es una practica de seguridad.

---

## Parte 2 — SGID en directorios

### Objetivo

Configurar directorios con SGID para herencia de grupo y entender
el patron de directorio compartido.

### Paso 2.1: Sin SGID — archivo hereda grupo del usuario

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

### Paso 2.2: Con SGID — archivo hereda grupo del directorio

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

SGID en directorios cambia el comportamiento de creacion: el grupo
del archivo nuevo es el grupo del directorio, no del usuario.

### Paso 2.3: Subdirectorios heredan SGID

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear subdirectorio ==="
mkdir /tmp/with-sgid/subdir
ls -ld /tmp/with-sgid/subdir
echo "(s en group = el subdirectorio tambien tiene SGID)"

echo ""
echo "=== Crear archivo en el subdirectorio ==="
touch /tmp/with-sgid/subdir/nested.txt
ls -la /tmp/with-sgid/subdir/nested.txt
echo "(grupo = sgidteam — la herencia se propaga)"

rm -rf /tmp/with-sgid
groupdel sgidteam 2>/dev/null
'
```

Los subdirectorios nuevos heredan automaticamente el bit SGID,
propagando la herencia de grupo a toda la jerarquia.

### Paso 2.4: Archivos SGID (ejecutables)

```bash
docker compose exec debian-dev bash -c '
echo "=== Buscar archivos con SGID ==="
find /usr -perm -2000 -type f -ls 2>/dev/null | head -5
echo ""
echo "SGID en ejecutables: se ejecutan con el GID del grupo del archivo"
echo "Ejemplo tipico: crontab (accede a /var/spool/cron/)"
'
```

---

## Parte 3 — Sticky bit

### Objetivo

Verificar la proteccion del sticky bit y combinar bits especiales.

### Paso 3.1: Sticky bit en /tmp

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

### Paso 3.2: Verificar proteccion del sticky bit

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear dos usuarios ==="
useradd -m sticky1
useradd -m sticky2

echo ""
echo "=== sticky1 crea un archivo en /tmp ==="
su -c "touch /tmp/sticky-test" sticky1 2>/dev/null || { touch /tmp/sticky-test; chown sticky1 /tmp/sticky-test; }
ls -la /tmp/sticky-test

echo ""
echo "=== sticky2 intenta eliminar ==="
su -c "rm -f /tmp/sticky-test" sticky2 2>&1 || true
echo "Exit code: $?"

echo ""
echo "=== El archivo sigue existiendo? ==="
ls -la /tmp/sticky-test 2>/dev/null && echo "SI (protegido por sticky bit)" || echo "NO (fue eliminado)"

rm -f /tmp/sticky-test
userdel -r sticky1 2>/dev/null
userdel -r sticky2 2>/dev/null
'
```

Con sticky bit, solo pueden eliminar un archivo: su owner, el owner
del directorio, y root.

### Paso 3.3: t vs T

```bash
docker compose exec debian-dev bash -c '
echo "=== t minuscula = sticky + x para others ==="
mkdir /tmp/sticky-t
chmod 1777 /tmp/sticky-t
ls -ld /tmp/sticky-t

echo ""
echo "=== T mayuscula = sticky sin x para others ==="
chmod 1776 /tmp/sticky-t
ls -ld /tmp/sticky-t
echo "(T = sticky activo pero others no tiene x)"

rm -rf /tmp/sticky-t
'
```

### Paso 3.4: Combinar SGID + sticky

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

La combinacion SGID + sticky es util para directorios compartidos
donde se quiere herencia de grupo Y proteccion contra eliminacion.

### Paso 3.5: El cuarto digito octal

```bash
docker compose exec debian-dev bash -c '
echo "=== Tabla del cuarto digito ==="
echo "0 = ninguno"
echo "1 = sticky"
echo "2 = SGID"
echo "3 = SGID + sticky"
echo "4 = SUID"
echo "5 = SUID + sticky"
echo "6 = SUID + SGID"
echo "7 = SUID + SGID + sticky"

echo ""
echo "=== Verificar en el sistema ==="
echo "/usr/bin/passwd: $(stat -c "%a" /usr/bin/passwd) (SUID)"
echo "/tmp: $(stat -c "%a" /tmp) (sticky)"
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
userdel -r sticky1 2>/dev/null
userdel -r sticky2 2>/dev/null
groupdel sgidteam 2>/dev/null
groupdel combiteam 2>/dev/null
rm -rf /tmp/no-sgid /tmp/with-sgid /tmp/sticky-t /tmp/combi-dir 2>/dev/null
rm -f /tmp/suid-test /tmp/suid-script.sh /tmp/sticky-test 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. **SUID** (4000): ejecutar como el owner del archivo. Ejemplo:
   `/usr/bin/passwd` ejecuta como root para escribir en /etc/shadow.

2. **SGID** (2000) en directorios: archivos nuevos heredan el grupo
   del directorio. Patron estandar para directorios compartidos.

3. **Sticky bit** (1000): solo el owner del archivo puede eliminarlo.
   Siempre activo en /tmp y /var/tmp.

4. Los bits especiales se representan como un cuarto digito octal
   al inicio: `chmod 4755` = SUID, `chmod 2775` = SGID,
   `chmod 1777` = sticky.

5. `s`/`t` minuscula = bit especial + x. `S`/`T` mayuscula = bit
   especial sin x (generalmente un error de configuracion).
