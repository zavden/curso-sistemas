# Lab — Archivos fundamentales

## Objetivo

Inspeccionar los cuatro archivos de identidad de Linux (/etc/passwd,
/etc/shadow, /etc/group, /etc/gshadow): interpretar sus campos,
comparar formatos entre Debian y AlmaLinux, y usar las herramientas
de consulta correctas.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — /etc/passwd

### Objetivo

Leer e interpretar cada campo de /etc/passwd.

### Paso 1.1: Contenido completo

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/passwd ==="
cat /etc/passwd
'
```

Cada linea tiene 7 campos separados por `:`.
Formato: `usuario:x:UID:GID:comentario:home:shell`.

### Paso 1.2: Contar usuarios

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

La mayoria de usuarios son de sistema (nologin). Solo unos pocos
tienen shell interactiva.

### Paso 1.3: Campos clave

```bash
docker compose exec debian-dev bash -c '
echo "=== UID 0 (root) ==="
grep ":0:" /etc/passwd | head -1

echo ""
echo "=== UID 65534 (nobody) ==="
grep "65534" /etc/passwd

echo ""
echo "=== Usuario dev ==="
getent passwd dev
'
```

UID 0 siempre es root (significado especial en el kernel). UID 65534
es nobody. `getent` es la forma correcta de consultar — lee de todas
las fuentes configuradas en nsswitch.conf.

### Paso 1.4: Campo GECOS y shells

```bash
docker compose exec debian-dev bash -c '
echo "=== Shells usadas en el sistema ==="
awk -F: "{print \$7}" /etc/passwd | sort | uniq -c | sort -rn

echo ""
echo "=== Shells validas ==="
cat /etc/shells
'
```

Las shells listadas en `/etc/shells` son las unicas que `chsh`
permite asignar a usuarios.

### Paso 1.5: Comparar con AlmaLinux

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

## Parte 2 — /etc/shadow

### Objetivo

Inspeccionar hashes de contrasenas, algoritmos y fechas de expiracion.

### Paso 2.1: Permisos de /etc/shadow

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

### Paso 2.2: Contenido de /etc/shadow

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/shadow (primeras lineas) ==="
head -5 /etc/shadow

echo ""
echo "=== Campos del usuario dev ==="
grep "^dev:" /etc/shadow
'
```

Formato: `usuario:hash:last_change:min:max:warn:inactive:expire:reserved`.
El campo hash contiene `$id$salt$hash` o valores especiales (`*`, `!`, `!!`).

### Paso 2.3: Identificar algoritmo de hash

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

### Paso 2.4: Valores especiales en el hash

```bash
docker compose exec debian-dev bash -c '
echo "=== Estados de contrasena ==="
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

`*` = no puede hacer login por contrasena. `!` = cuenta bloqueada.
`$y$...` o `$6$...` = hash real.

### Paso 2.5: Informacion de expiracion

```bash
docker compose exec debian-dev bash -c '
echo "=== Expiracion del usuario dev ==="
chage -l dev

echo ""
echo "=== Campo last_change en dias desde epoch ==="
DAYS=$(grep "^dev:" /etc/shadow | cut -d: -f3)
echo "Dias: $DAYS"
date -d "1970-01-01 + $DAYS days" 2>/dev/null || echo "(no se pudo convertir)"
'
```

Los campos de fecha usan "dias desde epoch" (1 enero 1970).
`chage -l` los muestra en formato legible.

---

## Parte 3 — /etc/group y herramientas

### Objetivo

Inspeccionar /etc/group, entender la relacion entre archivos, y
usar las herramientas de consulta.

### Paso 3.1: Contenido de /etc/group

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
(campo GID en /etc/passwd) no aparecen aqui.

### Paso 3.2: Relacion entre passwd y group

```bash
docker compose exec debian-dev bash -c '
echo "=== GID primario de dev en /etc/passwd ==="
getent passwd dev | cut -d: -f4

echo ""
echo "=== Linea del grupo dev en /etc/group ==="
getent group dev

echo ""
echo "=== Observa: dev NO aparece en el campo 4 de su propio grupo ==="
echo "(es miembro implicito por su GID primario)"

echo ""
echo "=== Todos los grupos de dev (primario + suplementarios) ==="
id dev
'
```

`id` muestra la vision completa: grupo primario y suplementarios.
Es la unica forma de ver todos los grupos de un usuario.

### Paso 3.3: getent vs leer archivos directamente

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
cat /etc/nsswitch.conf | grep -E "^(passwd|group|shadow)"
'
```

`getent` es la forma correcta porque consulta todas las fuentes
configuradas (archivos locales, LDAP, SSSD). Nunca usar `grep`
directamente sobre los archivos para consultas en produccion.

### Paso 3.4: Herramientas de consulta

```bash
docker compose exec debian-dev bash -c '
echo "=== id (informacion completa) ==="
id dev

echo ""
echo "=== id -u (solo UID) ==="
id -u dev

echo ""
echo "=== id -gn (solo grupo primario) ==="
id -gn dev

echo ""
echo "=== id -Gn (todos los grupos por nombre) ==="
id -Gn dev

echo ""
echo "=== groups (forma alternativa) ==="
groups dev
'
```

### Paso 3.5: Verificar consistencia

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
en modo solo lectura (no modifican nada). Detectan entradas huerfanas,
campos invalidos y desincronizaciones.

### Paso 3.6: Comparar entre distribuciones

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
lean /etc/shadow sin ser root. AlmaLinux no lo usa — protege con
permisos `000`.

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `/etc/passwd` almacena informacion publica de usuarios (7 campos).
   El campo 2 (`x`) indica que el hash real esta en `/etc/shadow`.

2. `/etc/shadow` almacena hashes y politicas de expiracion. Solo
   legible por root. El formato del hash (`$id$salt$hash`) identifica
   el algoritmo: `$y$` = yescrypt, `$6$` = SHA-512.

3. `/etc/group` lista miembros suplementarios en el campo 4. Los
   miembros primarios **no aparecen** — hay que usar `id` para ver
   todos los grupos de un usuario.

4. `getent` es la forma correcta de consultar porque lee de todas
   las fuentes de nsswitch.conf, no solo los archivos locales.

5. `pwck` y `grpck` verifican la consistencia entre los cuatro
   archivos de identidad.
