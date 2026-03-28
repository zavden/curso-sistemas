# Lab — Grupo primario efectivo

## Objetivo

Entender el grupo primario efectivo: como determinan los archivos
creados por un proceso, como cambiarlo temporalmente con newgrp y
sg, y como interactua con el SGID en directorios.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Observar el grupo efectivo

### Objetivo

Inspeccionar los cuatro GIDs de un proceso y entender cual
determina el grupo de los archivos nuevos.

### Paso 1.1: Grupo primario actual

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

### Paso 1.2: Los cuatro GIDs del proceso

```bash
docker compose exec debian-dev bash -c '
echo "=== GIDs del proceso actual ==="
cat /proc/self/status | grep ^Gid
echo ""
echo "Formato: Real  Efectivo  Saved  Filesystem"
echo ""
echo "En condiciones normales, los cuatro son iguales."
echo "Divergen con SGID, newgrp, o setgid()."
'
```

El GID **efectivo** es el que el kernel usa para verificar permisos
y asignar grupo a archivos nuevos.

### Paso 1.3: Grupos suplementarios del proceso

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
toda la sesion.

---

## Parte 2 — newgrp y sg

### Objetivo

Cambiar el grupo primario efectivo temporalmente con newgrp y
ejecutar comandos con sg.

### Paso 2.1: Preparar grupos

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear grupo de prueba ==="
groupadd -f labeffective
usermod -aG labeffective dev
echo "Grupos de dev: $(id -Gn dev)"
'
```

### Paso 2.2: newgrp cambia el grupo efectivo

```bash
docker compose exec debian-dev bash -c '
echo "=== Antes de newgrp ==="
echo "Grupo efectivo: $(id -gn)"
echo "SHLVL: $SHLVL"

echo ""
echo "=== Crear archivo antes ==="
touch /tmp/before-newgrp
ls -la /tmp/before-newgrp
echo "(grupo = grupo primario original)"

rm -f /tmp/before-newgrp
'
```

Nota: `newgrp` crea una subshell, asi que en un script `docker exec`
no persiste. El siguiente paso demuestra el efecto completo.

### Paso 2.3: newgrp en una sola sesion

```bash
docker compose exec debian-dev bash -c '
echo "=== newgrp en accion ==="
echo "Grupo antes: $(id -gn)"

newgrp labeffective <<INNER
echo "Grupo despues: \$(id -gn)"
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
(exit), se restaura el grupo original.

### Paso 2.4: sg — un solo comando

```bash
docker compose exec debian-dev bash -c '
echo "=== sg ejecuta un comando con otro grupo ==="
sg labeffective -c "touch /tmp/sg-test"
ls -la /tmp/sg-test
echo "(grupo = labeffective)"

echo ""
echo "=== El grupo actual NO cambio ==="
id -gn
echo "(sg no abre subshell — solo ejecuta el comando)"

rm -f /tmp/sg-test
'
```

`sg` es como `newgrp` pero para un solo comando. No cambia la
shell actual.

### Paso 2.5: newgrp vs usermod -g

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparacion ==="
echo ""
echo "newgrp:    temporal, solo la shell actual, no modifica /etc/passwd"
echo "usermod -g: permanente, todas las sesiones futuras, modifica /etc/passwd"
echo "sg:        temporal, un solo comando, no modifica nada"
'
```

---

## Parte 3 — SGID vs grupo efectivo

### Objetivo

Demostrar que SGID en directorios tiene prioridad sobre el grupo
efectivo del proceso.

### Paso 3.1: Crear directorio con SGID

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear directorio SGID ==="
mkdir -p /tmp/sgid-lab
chown root:labeffective /tmp/sgid-lab
chmod 2777 /tmp/sgid-lab
ls -ld /tmp/sgid-lab
'
```

### Paso 3.2: Archivo fuera vs dentro del directorio SGID

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
Los archivos heredan el grupo del directorio, no del usuario.

### Paso 3.3: SGID con newgrp — que gana?

```bash
docker compose exec debian-dev bash -c '
echo "=== Cambiar grupo efectivo a root (u otro grupo) ==="
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
del proceso. Esta es la razon por la que SGID es la solucion
estandar para directorios compartidos.

### Paso 3.4: Archivo sin SGID — grupo efectivo gana

```bash
docker compose exec debian-dev bash -c '
echo "=== Directorio sin SGID ==="
mkdir -p /tmp/no-sgid
chmod 777 /tmp/no-sgid

newgrp labeffective <<INNER
echo "Grupo efectivo: \$(id -gn)"
touch /tmp/no-sgid/test
ls -la /tmp/no-sgid/test
echo "(grupo = labeffective, el grupo efectivo del proceso)"
INNER

rm -rf /tmp/no-sgid
'
```

Sin SGID, el grupo del archivo es el grupo efectivo del proceso.
Con SGID, el grupo del archivo es el grupo del directorio.

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/sgid-lab /tmp/no-sgid 2>/dev/null
groupdel labeffective 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. El **grupo primario efectivo** es el GID que el kernel usa para
   asignar grupo a archivos nuevos. Normalmente coincide con el
   grupo primario de /etc/passwd.

2. `newgrp` crea una subshell con otro grupo efectivo. `sg` ejecuta
   un solo comando. Ambos son temporales — no modifican /etc/passwd.

3. Cada proceso tiene cuatro GIDs: real, efectivo, saved, filesystem.
   En condiciones normales son iguales.

4. SGID en directorios **sobreescribe** el grupo efectivo del
   proceso: los archivos creados dentro heredan el grupo del
   directorio.

5. Sin SGID, el grupo del archivo es el grupo efectivo. Con SGID,
   es el grupo del directorio. SGID siempre gana.
