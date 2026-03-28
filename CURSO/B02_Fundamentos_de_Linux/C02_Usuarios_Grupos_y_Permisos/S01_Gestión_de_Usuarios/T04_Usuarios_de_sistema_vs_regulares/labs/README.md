# Lab — Usuarios de sistema vs regulares

## Objetivo

Clasificar usuarios por tipo (sistema vs regular), crear usuarios de
sistema con useradd -r, entender la diferencia entre nologin y
/bin/false, y realizar una auditoria basica de seguridad.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Clasificar usuarios

### Objetivo

Identificar y contar los diferentes tipos de usuarios en el sistema.

### Paso 1.1: Listar por rango de UID

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

UID 0 = root. UIDs 1-999 = sistema. UIDs 1000+ = regulares.
UID 65534 = nobody (sin privilegios).

### Paso 1.2: Contar por tipo

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

La mayoria de usuarios son de sistema. Solo unos pocos son personas
reales con login interactivo.

### Paso 1.3: Usuarios con shell interactiva

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

Solo los usuarios con shell interactiva (/bin/bash, /bin/zsh) pueden
hacer login. Los de sistema tienen /usr/sbin/nologin.

### Paso 1.4: Rangos definidos en login.defs

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

Debian: sistema 100-999. AlmaLinux: sistema 201-999 (reserva 0-200
para UIDs estaticos).

---

## Parte 2 — Crear usuario de sistema

### Objetivo

Crear un usuario de sistema con useradd -r y verificar sus
caracteristicas.

### Paso 2.1: Crear usuario de servicio

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

### Paso 2.2: Verificar que no puede hacer login

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

`/usr/sbin/nologin` muestra un mensaje de rechazo y retorna exit
code 1.

### Paso 2.3: Crear directorio de datos

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
"home") en /var/lib/nombre_servicio. Se crea manualmente con los
permisos adecuados.

### Paso 2.4: Crear en AlmaLinux y comparar

```bash
docker compose exec alma-dev bash -c '
echo "=== Crear usuario de sistema ==="
useradd -r -s /usr/sbin/nologin -c "My Service" myservice

echo ""
echo "--- UID asignado ---"
id myservice
echo ""
echo "(en AlmaLinux el UID de sistema empieza en 201)"
'
```

### Paso 2.5: Comparar con usuario regular

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

## Parte 3 — Seguridad y comparacion

### Objetivo

Entender nologin vs /bin/false y realizar una auditoria basica
de seguridad.

### Paso 3.1: nologin vs /bin/false

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

### Paso 3.2: nologin no bloquea todo

```bash
docker compose exec debian-dev bash -c '
echo "=== nologin impide login interactivo ==="
echo "=== Pero el usuario SI puede: ==="
echo "- Ser propietario de archivos"
echo "- Tener procesos en ejecucion"
echo "- Tener crontabs"

echo ""
echo "=== Para bloquear completamente: ==="
echo "1. nologin (bloquea login interactivo)"
echo "2. passwd -l (bloquea contrasena)"
echo "3. chage -E 1 (expira la cuenta)"

echo ""
echo "=== Verificar: myservice esta completamente bloqueado? ==="
echo "Shell: $(getent passwd myservice | cut -d: -f7)"
echo "Contrasena: $(passwd -S myservice 2>&1 | awk "{print \$2}")"
'
```

### Paso 3.3: Auditoria de seguridad

```bash
docker compose exec debian-dev bash -c '
echo "=== 1. Usuarios con UID 0 (deberia ser solo root) ==="
awk -F: "\$3 == 0 {print \$1}" /etc/passwd

echo ""
echo "=== 2. Usuarios sin contrasena ==="
awk -F: "\$2 == \"\" {print \$1}" /etc/shadow 2>/dev/null || echo "(sin acceso)"

echo ""
echo "=== 3. Usuarios de sistema con shell interactiva (excepto root) ==="
awk -F: "\$3 > 0 && \$3 < 1000 && \$7 !~ /nologin|false/ {print \$1, \$7}" /etc/passwd

echo ""
echo "=== 4. Prefijo _ en nombres (convencion Debian) ==="
grep "^_" /etc/passwd || echo "(ningun usuario con prefijo _)"
'
```

Si hay usuarios con UID 0 que no son root, o usuarios de sistema
con shell interactiva, son senales de alarma de seguridad.

### Paso 3.4: nobody

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

Debian: grupo `nogroup`. AlmaLinux: grupo `nobody`. Es el usuario
sin privilegios usado por NFS, user namespaces y procesos no
confiables.

---

## Limpieza final

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

## Conceptos reforzados

1. Los usuarios **regulares** (UID >= 1000) son personas con shell
   interactiva. Los de **sistema** (UID 1-999) son servicios con
   nologin.

2. `useradd -r` crea usuarios de sistema: UID bajo, sin home, sin
   mail spool. Siempre combinar con `-s /usr/sbin/nologin`.

3. `/usr/sbin/nologin` muestra un mensaje de rechazo.
   `/bin/false` es silencioso. Ambos impiden login interactivo.

4. nologin no bloquea todo: el usuario puede tener archivos,
   procesos y crontabs. Para bloqueo completo: nologin + passwd -l.

5. Auditoria basica: verificar que solo root tiene UID 0, que no
   hay usuarios sin contrasena, y que los usuarios de sistema no
   tienen shell interactiva.
