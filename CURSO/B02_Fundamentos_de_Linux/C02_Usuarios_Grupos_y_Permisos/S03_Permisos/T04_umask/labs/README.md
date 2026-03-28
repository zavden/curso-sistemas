# Lab — umask

## Objetivo

Entender la umask: como filtra permisos en archivos y directorios
nuevos, cambiar la umask y verificar el resultado, y calcular los
permisos resultantes con diferentes valores de umask.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Observar la umask

### Objetivo

Ver la umask actual y su efecto en archivos y directorios nuevos.

### Paso 1.1: Ver la umask actual

```bash
docker compose exec debian-dev bash -c '
echo "=== Umask en octal ==="
umask

echo ""
echo "=== Umask en simbolico ==="
umask -S
'
```

`022` es el default estandar. En simbolico: `u=rwx,g=rx,o=rx`
(quita `w` de group y others).

### Paso 1.2: Efecto en archivos y directorios

```bash
docker compose exec debian-dev bash -c '
echo "=== Umask actual: $(umask) ==="

echo ""
echo "=== Crear archivo ==="
touch /tmp/umask-file
stat -c "%a %A %n" /tmp/umask-file
echo "(base 666 - umask 022 = 644)"

echo ""
echo "=== Crear directorio ==="
mkdir /tmp/umask-dir
stat -c "%a %A %n" /tmp/umask-dir
echo "(base 777 - umask 022 = 755)"

rm -f /tmp/umask-file
rmdir /tmp/umask-dir
'
```

Los archivos usan base 666 (nunca se crean con x). Los directorios
usan base 777. La umask se resta de la base.

### Paso 1.3: Comparar entre distribuciones

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
echo "Umask: $(umask)"
echo "login.defs: $(grep "^UMASK" /etc/login.defs 2>/dev/null || echo "no definido")"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
echo "Umask: $(umask)"
echo "login.defs: $(grep "^UMASK" /etc/login.defs 2>/dev/null || echo "no definido")"
'
```

Ambas distribuciones usan `022` por defecto.

---

## Parte 2 — Cambiar y verificar

### Objetivo

Cambiar la umask a diferentes valores y verificar el efecto.

### Paso 2.1: Umask 077 (privado)

```bash
docker compose exec debian-dev bash -c '
OLD=$(umask)
umask 077

echo "=== Umask 077 ==="
touch /tmp/test-077-file
mkdir /tmp/test-077-dir
stat -c "%a %A %n" /tmp/test-077-file
stat -c "%a %A %n" /tmp/test-077-dir
echo "(600 y 700 — solo el owner tiene acceso)"

rm -f /tmp/test-077-file
rmdir /tmp/test-077-dir
umask $OLD
'
```

Umask `077` quita todos los permisos de group y others. Ideal para
directorios home o archivos sensibles.

### Paso 2.2: Umask 002 (colaborativo)

```bash
docker compose exec debian-dev bash -c '
OLD=$(umask)
umask 002

echo "=== Umask 002 ==="
touch /tmp/test-002-file
mkdir /tmp/test-002-dir
stat -c "%a %A %n" /tmp/test-002-file
stat -c "%a %A %n" /tmp/test-002-dir
echo "(664 y 775 — group tiene escritura)"

rm -f /tmp/test-002-file
rmdir /tmp/test-002-dir
umask $OLD
'
```

Umask `002` permite escritura de grupo. Con UPG (cada usuario tiene
su propio grupo), esto es seguro porque el grupo primario es privado.

### Paso 2.3: Umask 027 (restrictivo)

```bash
docker compose exec debian-dev bash -c '
OLD=$(umask)
umask 027

echo "=== Umask 027 ==="
touch /tmp/test-027-file
mkdir /tmp/test-027-dir
stat -c "%a %A %n" /tmp/test-027-file
stat -c "%a %A %n" /tmp/test-027-dir
echo "(640 y 750 — others sin acceso, group solo lectura)"

rm -f /tmp/test-027-file
rmdir /tmp/test-027-dir
umask $OLD
'
```

### Paso 2.4: Multiples umasks en bucle

```bash
docker compose exec debian-dev bash -c '
OLD=$(umask)

echo "=== Comparacion de umasks ==="
printf "%-8s %-10s %-10s\n" "umask" "archivo" "directorio"
printf "%-8s %-10s %-10s\n" "-----" "--------" "----------"

for u in 022 077 002 027 007 037; do
    umask $u
    touch "/tmp/test-$u-f"
    mkdir "/tmp/test-$u-d"
    F=$(stat -c "%a" "/tmp/test-$u-f")
    D=$(stat -c "%a" "/tmp/test-$u-d")
    printf "%-8s %-10s %-10s\n" "$u" "$F" "$D"
    rm -f "/tmp/test-$u-f"
    rmdir "/tmp/test-$u-d"
done

umask $OLD
'
```

---

## Parte 3 — Calculo y configuracion

### Objetivo

Entender el calculo bitwise, la herencia de umask, y donde
configurarla permanentemente.

### Paso 3.1: El calculo no es aritmetico

```bash
docker compose exec debian-dev bash -c '
echo "=== Umask 033 ==="
echo "Aritmetico: 666 - 033 = 633 (INCORRECTO)"
echo "Bitwise:    666 AND NOT(033) = 644 (CORRECTO)"
echo ""
echo "033 quita w de group y wx de others"
echo "Pero x no existe en base 666, asi que quitar x no hace nada"
echo "Solo se quita w: 6→4 para group y others"

echo ""
echo "=== Verificar ==="
OLD=$(umask)
umask 033
touch /tmp/test-033
stat -c "%a %n" /tmp/test-033
rm -f /tmp/test-033
umask $OLD
'
```

Para umasks comunes (022, 077, 002) la resta aritmetica coincide.
Pero tecnicamente la operacion es bitwise AND con el complemento.

### Paso 3.2: Herencia de procesos

```bash
docker compose exec debian-dev bash -c '
echo "=== La umask se hereda de padre a hijo ==="
umask 027
echo "Shell actual: $(umask)"
echo "Subshell: $(bash -c "umask")"
echo "(la subshell heredo 027)"

umask 022
'
```

### Paso 3.3: Donde se configura

```bash
docker compose exec debian-dev bash -c '
echo "=== Orden de precedencia (menor a mayor) ==="
echo "1. /etc/login.defs (UMASK)"
echo "2. /etc/pam.d/ (pam_umask.so)"
echo "3. /etc/profile"
echo "4. /etc/profile.d/*.sh"
echo "5. ~/.profile o ~/.bash_profile"
echo "6. ~/.bashrc"
echo "7. Comando umask explicito"

echo ""
echo "=== login.defs ==="
grep "^UMASK" /etc/login.defs 2>/dev/null || echo "(no definido)"

echo ""
echo "=== PAM ==="
grep umask /etc/pam.d/common-session 2>/dev/null || echo "(no encontrado en PAM)"
'
```

### Paso 3.4: Umask en scripts

```bash
docker compose exec debian-dev bash -c '
echo "=== Buena practica: establecer umask al inicio del script ==="
echo "Asi no dependes del entorno de quien lo ejecuta"

echo ""
echo "Ejemplo:"
echo "#!/bin/bash"
echo "umask 027  # archivos 640, directorios 750"
echo "# ... resto del script ..."
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. La umask **filtra** permisos de archivos y directorios nuevos.
   No establece permisos — los quita de la base (666/777).

2. Umasks comunes: `022` (default, todos leen), `077` (privado),
   `002` (grupo escribe), `027` (others sin acceso).

3. El calculo es **bitwise** (AND NOT), no aritmetico. Para umasks
   comunes el resultado coincide, pero no siempre.

4. La umask se **hereda** de padre a hijo. Los servicios de systemd
   tienen su propia umask (UMask= en el unit file).

5. En scripts, establecer `umask` al inicio para no depender del
   entorno del usuario que lo ejecuta.
