# Lab — /sys

## Objetivo

Explorar el filesystem virtual /sys (sysfs): estructura de categorias,
informacion de interfaces de red, y la jerarquia de symlinks que
conecta la vista logica (/sys/class) con la fisica (/sys/devices).

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Estructura de /sys

### Objetivo

Explorar las categorias principales de /sys y entender su organizacion.

### Paso 1.1: Directorios principales

```bash
docker compose exec debian-dev bash -c '
echo "=== Categorias de /sys ==="
ls /sys/
'
```

Las categorias mas relevantes: `block/` (discos), `class/`
(dispositivos por funcion), `bus/` (buses del sistema), `devices/`
(jerarquia fisica real).

### Paso 1.2: Tipos de filesystem

```bash
docker compose exec debian-dev bash -c '
echo "=== /sys es sysfs ==="
mount | grep sysfs

echo ""
echo "=== Tamano ==="
df -h /sys
'
```

Como /proc, /sys no ocupa espacio en disco. Es generado por el kernel.

### Paso 1.3: Clases de dispositivos

```bash
docker compose exec debian-dev bash -c '
echo "=== Clases disponibles ==="
ls /sys/class/ | head -15
echo "..."
echo "Total: $(ls /sys/class/ | wc -l) clases"
'
```

Cada "clase" agrupa dispositivos por funcion: `net/` (red), `block/`
(almacenamiento), `tty/` (terminales).

### Paso 1.4: Buses del sistema

```bash
docker compose exec debian-dev bash -c '
echo "=== Buses ==="
ls /sys/bus/ 2>/dev/null | head -10
'
```

Los buses organizan dispositivos por tipo de conexion (pci, usb,
virtio). Dentro de un contenedor, la visibilidad de buses es limitada.

---

## Parte 2 — Interfaces de red

### Objetivo

Inspeccionar interfaces de red a traves de /sys/class/net/.

### Paso 2.1: Listar interfaces

```bash
docker compose exec debian-dev bash -c '
echo "=== Interfaces de red ==="
ls /sys/class/net/
'
```

El contenedor tiene al menos `lo` (loopback) y `eth0` (interfaz de
red del contenedor).

### Paso 2.2: Informacion de cada interfaz

```bash
docker compose exec debian-dev bash -c '
for iface in /sys/class/net/*; do
    name=$(basename "$iface")
    state=$(cat "$iface/operstate" 2>/dev/null || echo "unknown")
    mac=$(cat "$iface/address" 2>/dev/null || echo "n/a")
    mtu=$(cat "$iface/mtu" 2>/dev/null || echo "n/a")
    echo "$name: state=$state mac=$mac mtu=$mtu"
done
'
```

`operstate` indica si la interfaz esta activa (up/down). `mtu` es
el Maximum Transmission Unit (normalmente 1500 bytes).

### Paso 2.3: Estadisticas de red

```bash
docker compose exec debian-dev bash -c '
echo "=== Estadisticas de eth0 ==="
echo "Bytes recibidos: $(cat /sys/class/net/eth0/statistics/rx_bytes)"
echo "Bytes enviados: $(cat /sys/class/net/eth0/statistics/tx_bytes)"
echo "Paquetes recibidos: $(cat /sys/class/net/eth0/statistics/rx_packets)"
echo "Paquetes enviados: $(cat /sys/class/net/eth0/statistics/tx_packets)"
echo "Errores RX: $(cat /sys/class/net/eth0/statistics/rx_errors)"
echo "Errores TX: $(cat /sys/class/net/eth0/statistics/tx_errors)"
'
```

Estas estadisticas son en tiempo real, actualizadas por el kernel.
Equivale a la informacion de `ip -s link show eth0` pero accesible
como archivos individuales.

### Paso 2.4: Comparar con ip command

```bash
docker compose exec debian-dev bash -c '
echo "=== /sys ==="
echo "MAC: $(cat /sys/class/net/eth0/address)"
echo "MTU: $(cat /sys/class/net/eth0/mtu)"

echo ""
echo "=== ip command ==="
ip link show eth0 | head -2
'
```

Ambos muestran la misma informacion. `ip` obtiene los datos de /sys
internamente.

---

## Parte 3 — Jerarquia de symlinks

### Objetivo

Entender como /sys/class apunta a /sys/devices y la relacion entre
vista logica y fisica.

### Paso 3.1: Seguir un symlink

```bash
docker compose exec debian-dev bash -c '
echo "=== /sys/class/net/eth0 es un symlink ==="
readlink /sys/class/net/eth0

echo ""
echo "=== Ruta real (resolving all symlinks) ==="
readlink -f /sys/class/net/eth0
'
```

`/sys/class/net/eth0` es un symlink a la representacion real del
dispositivo en `/sys/devices/`. La clase agrupa por funcion, devices
muestra la jerarquia fisica.

### Paso 3.2: Loopback

```bash
docker compose exec debian-dev bash -c '
echo "=== lo ==="
readlink /sys/class/net/lo
echo ""
readlink -f /sys/class/net/lo
'
```

La interfaz loopback esta en `/sys/devices/virtual/` porque no tiene
hardware fisico asociado.

### Paso 3.3: Comparar /proc y /sys para red

```bash
docker compose exec debian-dev bash -c '
echo "=== /proc/net/dev (legacy) ==="
cat /proc/net/dev | head -5

echo ""
echo "=== /sys/class/net/ (moderno) ==="
for iface in /sys/class/net/*; do
    name=$(basename "$iface")
    rx=$(cat "$iface/statistics/rx_bytes")
    tx=$(cat "$iface/statistics/tx_bytes")
    echo "$name: rx=$rx tx=$tx"
done
'
```

`/proc/net/dev` es la interfaz legacy (herencia historica).
`/sys/class/net/*/statistics/` es la interfaz moderna y limpia.
Ambas muestran la misma informacion.

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `/sys` (sysfs) expone el **modelo de dispositivos del kernel** de
   forma jerarquica. Fue creado para organizar la informacion de
   dispositivos que `/proc` mezclaba con otros datos.

2. `/sys/class/` agrupa dispositivos por **funcion** (net, block,
   tty). `/sys/devices/` muestra la **jerarquia fisica** real.

3. Las entradas en `/sys/class/` son **symlinks** a la representacion
   real en `/sys/devices/`.

4. `/sys/class/net/*/statistics/` ofrece estadisticas de red en
   **archivos individuales** (rx_bytes, tx_bytes), consumibles por
   scripts sin parseo complejo.

5. `/proc/net/dev` y `/sys/class/net/` ofrecen la misma informacion
   de red. `/sys` es la interfaz moderna; `/proc` mantiene la legacy
   por compatibilidad.
