# Lab — /dev

## Objetivo

Explorar /dev: clasificar dispositivos de bloque y caracter, usar los
dispositivos especiales esenciales, y entender el /dev minimo que Docker
proporciona a los contenedores.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Tipos de dispositivos

### Objetivo

Clasificar los dispositivos en /dev y entender los major/minor numbers.

### Paso 1.1: Listar /dev en el contenedor

```bash
docker compose exec debian-dev bash -c '
echo "=== Contenido de /dev ==="
ls /dev/
'
```

El contenedor tiene un /dev **minimo**. Solo incluye los dispositivos
esenciales (null, zero, random, pts, etc.), no los discos del host.

### Paso 1.2: Dispositivos de caracter

```bash
docker compose exec debian-dev bash -c '
echo "=== Dispositivos de caracter (c) ==="
ls -la /dev/ | grep "^c" | head -10
'
```

La `c` al inicio indica dispositivo de **caracter**: transmite datos
como flujo de bytes secuencial (terminales, /dev/null, /dev/random).

### Paso 1.3: Dispositivos de bloque

```bash
docker compose exec debian-dev bash -c '
echo "=== Dispositivos de bloque (b) ==="
ls -la /dev/ | grep "^b" | head -10 || echo "(ninguno visible en el contenedor)"
'
```

La `b` indica dispositivo de **bloque**: datos en bloques de tamano
fijo con acceso aleatorio (discos, particiones). Los contenedores no
ven los discos del host por seguridad.

### Paso 1.4: Major y minor numbers

```bash
docker compose exec debian-dev bash -c '
echo "=== Major/minor numbers ==="
ls -la /dev/null /dev/zero /dev/random /dev/urandom /dev/full
'
```

Los numeros antes de la fecha son major (driver) y minor (dispositivo
especifico). Por ejemplo, major=1 es el driver "mem" que gestiona
null, zero, full, random.

### Paso 1.5: Registro de major numbers

```bash
docker compose exec debian-dev bash -c '
echo "=== Registro de dispositivos ==="
cat /proc/devices | head -15
'
```

`/proc/devices` muestra el mapeo entre major numbers y drivers del
kernel.

---

## Parte 2 — Dispositivos especiales

### Objetivo

Usar los dispositivos especiales esenciales y entender su
comportamiento.

### Paso 2.1: /dev/null — agujero negro

```bash
docker compose exec debian-dev bash -c '
echo "=== /dev/null descarta todo ==="
echo "esto se pierde" > /dev/null
echo "Exit code: $?"

echo ""
echo "=== Leer /dev/null da vacio ==="
wc -c < /dev/null
'
```

Uso principal: suprimir salida no deseada (`comando > /dev/null 2>&1`).

### Paso 2.2: /dev/zero — fuente de ceros

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear 1KB de ceros ==="
dd if=/dev/zero of=/tmp/zeros.bin bs=1K count=1 2>&1

echo ""
echo "=== Verificar contenido ==="
xxd /tmp/zeros.bin | head -3

rm /tmp/zeros.bin
'
```

`/dev/zero` produce bytes `\0` infinitamente. Usado para crear
archivos de tamano especifico o sobrescribir datos.

### Paso 2.3: /dev/urandom — aleatoriedad

```bash
docker compose exec debian-dev bash -c '
echo "=== 16 bytes aleatorios ==="
dd if=/dev/urandom bs=16 count=1 2>/dev/null | xxd

echo ""
echo "=== Password aleatorio ==="
head -c 24 /dev/urandom | base64
'
```

`/dev/urandom` genera bytes pseudo-aleatorios. Nunca bloquea y es
suficiente para la mayoria de usos (passwords, tokens, salt).

### Paso 2.4: /dev/full — disco lleno

```bash
docker compose exec debian-dev bash -c '
echo "=== Simular disco lleno ==="
echo "test" > /dev/full 2>&1
echo "Exit code: $?"
'
```

`/dev/full` simula el error ENOSPC (No space left on device). Util
para probar que programas manejan correctamente este error.

### Paso 2.5: Combinacion practica

```bash
docker compose exec debian-dev bash -c '
echo "=== Descartar stderr, mantener stdout ==="
ls /dev/null /dev/no_existe 2>/dev/null
echo ""
echo "=== Descartar todo ==="
ls /dev/null /dev/no_existe > /dev/null 2>&1
echo "Exit code: $?"
'
```

Redirigir a `/dev/null` es la forma estandar de suprimir salida
no deseada en scripts.

---

## Parte 3 — Terminales y /dev en contenedores

### Objetivo

Explorar pseudo-terminales y entender el /dev minimo de Docker.

### Paso 3.1: Terminal actual

```bash
docker compose exec debian-dev bash -c '
echo "=== Terminal actual ==="
tty

echo ""
echo "=== /dev/pts/ ==="
ls /dev/pts/
'
```

`tty` muestra la pseudo-terminal asignada a esta sesion (`/dev/pts/N`).
Cada sesion `exec` obtiene un nuevo `/dev/pts/`.

### Paso 3.2: Escribir a la terminal

```bash
docker compose exec debian-dev bash -c '
echo "=== Escribir a /dev/tty ==="
echo "este mensaje va directo a la terminal" > /dev/tty
'
```

`/dev/tty` siempre apunta a la terminal del proceso actual. Util en
scripts para mostrar mensajes al usuario incluso cuando stdout esta
redirigido.

### Paso 3.3: /dev en contenedor vs host

```bash
echo "=== /dev en contenedor ==="
docker compose exec debian-dev bash -c 'ls /dev/ | sort'

echo ""
echo "=== Comparar: /dev en host es mucho mas grande ==="
echo "(en el host: ls /dev/ | wc -l)"
```

Docker proporciona un /dev minimo por seguridad. Los contenedores
no pueden acceder a discos, puertos serie, ni otros dispositivos
del host sin permiso explicito (`--device` o `--privileged`).

### Paso 3.4: Contar dispositivos

```bash
docker compose exec debian-dev bash -c '
echo "Caracter: $(ls -la /dev/ | grep -c "^c")"
echo "Bloque:   $(ls -la /dev/ | grep -c "^b")"
echo "Total:    $(ls /dev/ | wc -l)"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Los dispositivos de **bloque** (b) almacenan datos en bloques con
   acceso aleatorio (discos). Los de **caracter** (c) transmiten
   datos como flujo secuencial (terminales, /dev/null).

2. `/dev/null` descarta todo, `/dev/zero` produce ceros, `/dev/urandom`
   produce aleatoriedad, `/dev/full` simula disco lleno.

3. Los **major/minor numbers** identifican el driver y el dispositivo
   especifico. `/proc/devices` mapea major numbers a nombres de
   drivers.

4. Docker proporciona un `/dev` **minimo** a los contenedores por
   seguridad. No incluye discos ni dispositivos del host.

5. Cada sesion interactiva obtiene una **pseudo-terminal**
   (`/dev/pts/N`). `/dev/tty` siempre apunta a la terminal del
   proceso actual.
