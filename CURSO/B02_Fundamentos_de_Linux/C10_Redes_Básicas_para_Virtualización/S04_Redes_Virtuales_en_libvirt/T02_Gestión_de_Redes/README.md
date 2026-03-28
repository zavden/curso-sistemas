# Gestión de Redes Virtuales con virsh

## Índice

1. [El subsistema de redes en libvirt](#1-el-subsistema-de-redes-en-libvirt)
2. [Ciclo de vida de una red virtual](#2-ciclo-de-vida-de-una-red-virtual)
3. [Inspeccionar redes: net-list, net-info, net-dumpxml](#3-inspeccionar-redes-net-list-net-info-net-dumpxml)
4. [Leases DHCP: net-dhcp-leases](#4-leases-dhcp-net-dhcp-leases)
5. [Iniciar y detener redes: net-start, net-destroy](#5-iniciar-y-detener-redes-net-start-net-destroy)
6. [Inicio automático: net-autostart](#6-inicio-automático-net-autostart)
7. [Crear redes: net-define y net-create](#7-crear-redes-net-define-y-net-create)
8. [Editar redes: net-edit](#8-editar-redes-net-edit)
9. [Modificaciones en caliente: net-update](#9-modificaciones-en-caliente-net-update)
10. [Eliminar redes: net-undefine](#10-eliminar-redes-net-undefine)
11. [Conectar VMs a redes: attach-interface y detach-interface](#11-conectar-vms-a-redes-attach-interface-y-detach-interface)
12. [Hooks de red en libvirt](#12-hooks-de-red-en-libvirt)
13. [Flujo de diagnóstico completo](#13-flujo-de-diagnóstico-completo)
14. [Errores comunes](#14-errores-comunes)
15. [Cheatsheet](#15-cheatsheet)
16. [Ejercicios](#16-ejercicios)

---

## 1. El subsistema de redes en libvirt

Los comandos `virsh net-*` gestionan **redes virtuales**, no máquinas virtuales. Una red virtual en libvirt es un objeto con su propio ciclo de vida independiente de las VMs que la usan.

Internamente, cuando defines y arrancas una red, libvirt:

```
virsh net-define red.xml          virsh net-start mynet
        │                                  │
        ▼                                  ▼
┌─────────────────┐            ┌──────────────────────┐
│ Guarda XML en   │            │ 1. Crea bridge       │
│ /etc/libvirt/   │            │    (ej: virbr0)      │
│ qemu/networks/  │            │ 2. Asigna IP al      │
│ mynet.xml       │            │    bridge             │
│                 │            │ 3. Lanza dnsmasq     │
│ Estado:         │            │    (DHCP + DNS)      │
│ INACTIVA        │            │ 4. Añade reglas      │
│ PERSISTENTE     │            │    iptables/nftables │
└─────────────────┘            │ 5. Estado: ACTIVA    │
                               └──────────────────────┘
```

### Dónde vive cada cosa

| Elemento | Ubicación |
|----------|-----------|
| Definiciones XML de redes | `/etc/libvirt/qemu/networks/` |
| Symlinks de autostart | `/etc/libvirt/qemu/networks/autostart/` |
| PID de dnsmasq por red | `/var/run/libvirt/network/` |
| Leases DHCP de dnsmasq | `/var/lib/libvirt/dnsmasq/` |
| Log de libvirtd | `/var/log/libvirt/libvirtd.log` |
| Hooks de red | `/etc/libvirt/hooks/network` |

Cada red activa tiene **su propia instancia de dnsmasq**. Cuando arrancas `default`, libvirt lanza un proceso dnsmasq exclusivo para `virbr0`. Puedes verificarlo:

```bash
ps aux | grep dnsmasq
# dnsmasq  1234 ... --listen-address=192.168.122.1 --dhcp-range=192.168.122.2,192.168.122.254
# dnsmasq  5678 ... --listen-address=10.0.1.1 --dhcp-range=10.0.1.2,10.0.1.50
```

---

## 2. Ciclo de vida de una red virtual

Una red tiene dos dimensiones de estado que se combinan:

```
                    ┌─────────────┐
                    │  No existe  │
                    └──────┬──────┘
                           │
                  net-define (desde XML)
                           │
                    ┌──────▼──────┐
                    │  Inactiva   │◄──── net-destroy
                    │ Persistente │
                    ┌──────┬──────┐
                           │
                      net-start
                           │
                    ┌──────▼──────┐
                    │   Activa    │
                    │ Persistente │
                    └──────┬──────┘
                           │
                      net-undefine ───► Activa + Transitoria
                           │            (se pierde al detener)
                      net-destroy
                           │
                    ┌──────▼──────┐
                    │  No existe  │
                    └─────────────┘
```

Los estados posibles:

| Estado | Persistente | Activa | Significado |
|--------|:-----------:|:------:|-------------|
| Definida | Sí | No | XML guardado, pero no hay bridge/dnsmasq |
| Activa y persistente | Sí | Sí | Funcionando, sobrevive reinicio del daemon |
| Transitoria activa | No | Sí | Creada con `net-create`; al detenerla, desaparece |

Existe además `net-create` (sin `net-define` previo), que crea una red **transitoria**: se activa inmediatamente pero no se guarda en disco. Útil para pruebas rápidas.

---

## 3. Inspeccionar redes: net-list, net-info, net-dumpxml

### net-list

```bash
# Solo redes activas
virsh net-list

# Todas (activas e inactivas)
virsh net-list --all

# Ejemplo de salida:
#  Name       State    Autostart   Persistent
# ─────────────────────────────────────────────
#  default    active   yes         yes
#  isolated   inactive no          yes
#  lab-nat    active   yes         yes
```

Los filtros disponibles:

```bash
virsh net-list --inactive      # Solo las inactivas
virsh net-list --persistent    # Solo las persistentes
virsh net-list --transient     # Solo las transitorias
virsh net-list --autostart     # Solo las que tienen autostart
virsh net-list --no-autostart  # Solo las que NO tienen autostart
```

Puedes combinarlos: `virsh net-list --active --autostart` muestra redes que están activas Y tienen autostart.

### net-info

Resumen legible de una red específica:

```bash
virsh net-info default

# Name:           default
# UUID:           a1b2c3d4-e5f6-7890-abcd-ef1234567890
# Active:         yes
# Persistent:     yes
# Autostart:      yes
# Bridge:         virbr0
```

Cada campo importa:
- **UUID**: identificador único, útil cuando dos redes podrían tener el mismo nombre en diferentes hosts.
- **Bridge**: el nombre del bridge Linux que esta red crea (`virbr0`, `virbr1`, etc.).
- **Persistent**: si es `no`, la red se perderá al detenerla.

### net-dumpxml

El XML completo y canónico — incluye campos que libvirt genera automáticamente (UUID, MAC del bridge):

```bash
virsh net-dumpxml default
```

```xml
<network>
  <name>default</name>
  <uuid>a1b2c3d4-e5f6-7890-abcd-ef1234567890</uuid>
  <forward mode='nat'>
    <nat>
      <port start='1024' end='65535'/>
    </nat>
  </forward>
  <bridge name='virbr0' stp='on' delay='0'/>
  <mac address='52:54:00:ab:cd:ef'/>
  <ip address='192.168.122.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='192.168.122.2' end='192.168.122.254'/>
      <host mac='52:54:00:11:22:33' name='debian-lab' ip='192.168.122.100'/>
    </dhcp>
  </ip>
</network>
```

**Tip práctico**: usa `net-dumpxml` como base para crear nuevas redes — es más seguro que escribir XML desde cero:

```bash
virsh net-dumpxml default > /tmp/new-net.xml
# Editar: cambiar name, eliminar uuid (libvirt genera uno nuevo),
# cambiar bridge, IP, rango DHCP
vi /tmp/new-net.xml
virsh net-define /tmp/new-net.xml
```

### net-uuid y net-name

Convertir entre nombre y UUID:

```bash
virsh net-uuid default
# a1b2c3d4-e5f6-7890-abcd-ef1234567890

virsh net-name a1b2c3d4-e5f6-7890-abcd-ef1234567890
# default
```

---

## 4. Leases DHCP: net-dhcp-leases

Muestra las direcciones IP que dnsmasq ha entregado a las VMs conectadas:

```bash
virsh net-dhcp-leases default

#  Expiry Time           MAC address        Protocol  IP address          Hostname     Client ID or DUID
# ──────────────────────────────────────────────────────────────────────────────────────────────────────────
#  2026-03-19 15:30:00   52:54:00:11:22:33  ipv4      192.168.122.100/24  debian-lab   ff:11:22:33:...
#  2026-03-19 15:25:00   52:54:00:44:55:66  ipv4      192.168.122.45/24   fedora-test  ff:44:55:66:...
```

Lo que revela cada columna:
- **Expiry Time**: cuándo expira el lease (dnsmasq usa leases de 1 hora por defecto en libvirt).
- **MAC address**: la MAC de la interfaz virtual de la VM (52:54:00:xx es el prefijo de QEMU).
- **IP address**: la IP asignada, con la máscara CIDR.
- **Hostname**: el hostname que la VM envió en la solicitud DHCP.

Si no hay VMs corriendo en esa red, la tabla aparece vacía.

**Dónde están los leases en disco**:

```bash
cat /var/lib/libvirt/dnsmasq/virbr0.status
# JSON con los leases activos

cat /var/lib/libvirt/dnsmasq/default.leases
# Formato dnsmasq clásico: timestamp MAC IP hostname clientid
```

---

## 5. Iniciar y detener redes: net-start, net-destroy

### net-start

Activa una red previamente definida:

```bash
virsh net-start isolated
# Network isolated started
```

Lo que ocurre internamente:
1. Crea el bridge Linux (`ip link add virbr1 type bridge`).
2. Asigna la IP al bridge (`ip addr add 10.0.1.1/24 dev virbr1`).
3. Levanta el bridge (`ip link set virbr1 up`).
4. Lanza dnsmasq escuchando en esa IP.
5. Añade reglas iptables/nftables (MASQUERADE si es NAT, REJECT/DROP si es aislada).

Si la red ya está activa, obtienes un error:

```
error: Requested operation is not valid: network is already active
```

### net-destroy

Detiene una red activa — equivale a "apagar el switch":

```bash
virsh net-destroy isolated
# Network isolated destroyed
```

Lo que ocurre:
1. Mata el proceso dnsmasq asociado.
2. Elimina las reglas iptables/nftables de esa red.
3. Baja y elimina el bridge Linux.
4. Si es persistente, el XML se conserva → puedes hacer `net-start` de nuevo.
5. Si es transitoria, desaparece completamente.

**Impacto en VMs conectadas**: las VMs pierden conectividad **inmediatamente**. La interfaz virtual de la VM queda sin bridge al que conectarse. Dentro de la VM, `ip addr` sigue mostrando la IP, pero no hay comunicación posible hasta que la red se reinicie.

```
 ANTES de net-destroy           DESPUÉS de net-destroy
┌──────┐    ┌──────┐           ┌──────┐    ┌──────┐
│ VM-1 │    │ VM-2 │           │ VM-1 │    │ VM-2 │
│ .100 │    │ .101 │           │ .100 │    │ .101 │
└──┬───┘    └──┬───┘           └──┬───┘    └──┬───┘
   │  virbr0   │                  │    ???    │
   └─────┬─────┘                  │  (sin    │
         │                        │  bridge) │
    ┌────▼────┐                   ▼          ▼
    │  Host   │                 Desconectadas
    └─────────┘
```

### Reiniciar una red

No existe `virsh net-restart`. Hay que detener y arrancar:

```bash
virsh net-destroy default && virsh net-start default
```

Las VMs reconectan automáticamente al bridge recreado y renuevan su lease DHCP.

---

## 6. Inicio automático: net-autostart

Configura si una red se activa automáticamente cuando arranca libvirtd (normalmente al boot del sistema):

```bash
# Habilitar
virsh net-autostart default
# Network default marked as autostarted

# Deshabilitar
virsh net-autostart default --disable
# Network default unmarked as autostarted

# Verificar
virsh net-info default | grep -i autostart
# Autostart:      yes
```

Internamente, `net-autostart` crea (o elimina) un symlink:

```bash
ls -la /etc/libvirt/qemu/networks/autostart/
# default.xml -> /etc/libvirt/qemu/networks/default.xml
# lab-nat.xml -> /etc/libvirt/qemu/networks/lab-nat.xml
```

**Recomendación**: las redes que tus VMs necesitan al arrancar deben tener autostart. Sin esto, si reinicias el host, las VMs arrancarán pero no tendrán red hasta que manualmente hagas `net-start`.

---

## 7. Crear redes: net-define y net-create

### net-define: red persistente

Registra una red a partir de un archivo XML. La red queda **inactiva** hasta que hagas `net-start`:

```bash
cat > /tmp/lab-isolated.xml <<'EOF'
<network>
  <name>lab-isolated</name>
  <bridge name='virbr10' stp='on' delay='0'/>
  <ip address='10.0.10.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.0.10.2' end='10.0.10.50'/>
    </dhcp>
  </ip>
</network>
EOF

virsh net-define /tmp/lab-isolated.xml
# Network lab-isolated defined from /tmp/lab-isolated.xml
```

Nota que al no incluir `<forward>`, la red es **aislada** (equivale a `<forward mode='none'/>`). Tampoco incluimos `<uuid>` — libvirt genera uno automáticamente.

Flujo completo para activar una nueva red:

```bash
virsh net-define /tmp/lab-isolated.xml    # 1. Registrar
virsh net-start lab-isolated              # 2. Activar
virsh net-autostart lab-isolated          # 3. Persistir al boot
```

### net-create: red transitoria

Crea y activa la red en un solo paso, pero **no se guarda en disco**. Al detenerla (o reiniciar el host), desaparece:

```bash
virsh net-create /tmp/test-net.xml
# Network test-net created from /tmp/test-net.xml
```

Útil para pruebas rápidas donde no quieres dejar residuos.

### Ejemplo completo: red NAT personalizada

```xml
<network>
  <name>dev-nat</name>
  <forward mode='nat'>
    <nat>
      <port start='1024' end='65535'/>
    </nat>
  </forward>
  <bridge name='virbr5' stp='on' delay='0'/>
  <domain name='dev.local' localOnly='yes'/>
  <ip address='192.168.50.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='192.168.50.10' end='192.168.50.100'/>
      <host mac='52:54:00:aa:bb:01' name='web-server' ip='192.168.50.200'/>
      <host mac='52:54:00:aa:bb:02' name='db-server' ip='192.168.50.201'/>
    </dhcp>
  </ip>
</network>
```

Elementos destacados:
- `<domain name='dev.local' localOnly='yes'/>`: dnsmasq resuelve nombres como `web-server.dev.local` solo dentro de esta red.
- `<host>`: reservas DHCP — esas MACs siempre reciben la misma IP.
- `<port start='1024' end='65535'/>`: rango de puertos para PAT (el valor por defecto).

---

## 8. Editar redes: net-edit

Abre el XML de una red en el editor del sistema (`$EDITOR` o `vi`):

```bash
virsh net-edit default
```

Esto es equivalente a:
1. `virsh net-dumpxml default > /tmp/tmpfile.xml`
2. `$EDITOR /tmp/tmpfile.xml`
3. Si hubo cambios, `virsh net-define /tmp/tmpfile.xml`

**Los cambios NO se aplican inmediatamente.** El XML se guarda, pero la red activa sigue con la configuración anterior. Para aplicar:

```bash
virsh net-destroy default && virsh net-start default
```

### Qué se puede cambiar con net-edit

| Cambio | Requiere reinicio de red |
|--------|:------------------------:|
| Rango DHCP | Sí |
| Reservas DHCP (host entries) | Sí (o usar net-update) |
| IP del bridge | Sí |
| Máscara de red | Sí |
| Nombre del bridge | Sí |
| Dominio DNS | Sí |
| Forward mode | Sí |

Para cambios que no requieran reinicio, usa `net-update` (siguiente sección).

### Ejemplo: cambiar rango DHCP

```bash
virsh net-edit default
# Buscar:
#   <range start='192.168.122.2' end='192.168.122.254'/>
# Cambiar a:
#   <range start='192.168.122.100' end='192.168.122.200'/>

# Aplicar:
virsh net-destroy default && virsh net-start default
```

---

## 9. Modificaciones en caliente: net-update

`net-update` es el comando más potente y menos conocido. Permite **modificar una red activa sin reiniciarla** — las VMs no pierden conectividad.

### Sintaxis

```bash
virsh net-update <network> <command> <section> <xml> [--live] [--config] [--current]
```

- `<command>`: `add-first`, `add-last`, `delete`, `modify`
- `<section>`: qué parte del XML modificar (`ip-dhcp-host`, `ip-dhcp-range`, `dns-host`, `forward-interface`, etc.)
- `--live`: aplica al estado en ejecución
- `--config`: aplica al XML persistente
- `--current`: aplica a ambos

### Añadir una reserva DHCP en caliente

```bash
virsh net-update default add-last ip-dhcp-host \
  '<host mac="52:54:00:aa:bb:cc" name="new-vm" ip="192.168.122.150"/>' \
  --live --config
# Updated network default persistent config and live state
```

Esto:
1. Añade la reserva al XML en disco (`--config`).
2. Actualiza dnsmasq en ejecución (`--live`).
3. La próxima vez que `new-vm` pida IP por DHCP, recibirá `.150`.

### Eliminar una reserva DHCP

```bash
virsh net-update default delete ip-dhcp-host \
  '<host mac="52:54:00:aa:bb:cc" name="new-vm" ip="192.168.122.150"/>' \
  --live --config
```

### Añadir una entrada DNS estática

```bash
virsh net-update default add-last dns-host \
  '<host ip="192.168.122.150"><hostname>myapp.dev.local</hostname></host>' \
  --live --config
```

Ahora cualquier VM en la red `default` puede resolver `myapp.dev.local` → `192.168.122.150`.

### Secciones disponibles para net-update

| Sección | Descripción |
|---------|-------------|
| `ip-dhcp-host` | Reservas DHCP (MAC→IP) |
| `ip-dhcp-range` | Rangos DHCP |
| `dns-host` | Entradas DNS estáticas (hostname→IP) |
| `dns-txt` | Registros TXT de DNS |
| `dns-srv` | Registros SRV de DNS |
| `forward-interface` | Interfaces de forward |
| `portgroup` | Grupos de puertos (para VLAN tagging) |

### --live vs --config vs --current

```
 --live             Solo afecta la red en ejecución
                    Se pierde al reiniciar la red

 --config           Solo modifica el XML en disco
                    Se aplica en el próximo net-start

 --live --config    Ambos: efecto inmediato Y persistente
                    ← recomendado

 --current          Equivale a --live si la red está activa,
                    o --config si está inactiva
```

---

## 10. Eliminar redes: net-undefine

Elimina la definición persistente (el XML) de una red:

```bash
# Si la red está activa, primero detenerla
virsh net-destroy lab-isolated

# Eliminar la definición
virsh net-undefine lab-isolated
# Network lab-isolated has been undefined
```

**Comportamiento según estado**:

| Estado previo | Resultado de `net-undefine` |
|---------------|----------------------------|
| Inactiva + persistente | Elimina XML. La red deja de existir. |
| Activa + persistente | Elimina XML. La red sigue activa pero se convierte en **transitoria** — desaparecerá al detenerla. |
| Transitoria (activa) | Error: no se puede "undefine" lo que nunca se "defined". Usa `net-destroy`. |

Regla simple: **`net-destroy` apaga, `net-undefine` borra**. Para eliminación completa:

```bash
virsh net-destroy mynet && virsh net-undefine mynet
```

---

## 11. Conectar VMs a redes: attach-interface y detach-interface

### Ver qué red usa una VM

```bash
# Buscar interfaces de red en el XML de la VM
virsh domiflist debian-lab

# Interface   Type      Source    Model    MAC
# ─────────────────────────────────────────────────────
# vnet0       network   default  virtio   52:54:00:11:22:33
```

`domiflist` es más directo que grep en dumpxml. Muestra:
- **Interface**: nombre del dispositivo TAP en el host (`vnet0`).
- **Type**: `network` (red virtual de libvirt) o `bridge` (bridge manual).
- **Source**: nombre de la red o bridge.
- **Model**: tipo de tarjeta de red emulada (`virtio`, `e1000`, `rtl8139`).

### Añadir una segunda interfaz (VM encendida)

```bash
virsh attach-interface debian-lab network isolated \
  --model virtio --persistent
# Interface attached successfully
```

- `network isolated`: conectar a la red llamada `isolated`.
- `--model virtio`: usar virtio (mejor rendimiento).
- `--persistent`: guardar en el XML de la VM, no solo en la sesión activa.

Sin `--persistent`, la interfaz se pierde al apagar la VM.

Dentro de la VM, aparece una nueva interfaz que necesita configuración:

```bash
# Dentro de la VM
ip addr show
# ...
# 3: enp0s7: <BROADCAST,MULTICAST> mtu 1500 ...
#     link/ether 52:54:00:99:88:77 brd ff:ff:ff:ff:ff:ff

# Levantar y pedir IP por DHCP
sudo dhclient enp0s7
# o con NetworkManager:
sudo nmcli device connect enp0s7
```

### Quitar una interfaz

```bash
# Primero, identificar la MAC de la interfaz a quitar
virsh domiflist debian-lab

# Quitar usando la MAC
virsh detach-interface debian-lab network \
  --mac 52:54:00:99:88:77 --persistent
# Interface detached successfully
```

Hay que usar `--mac` cuando la VM tiene múltiples interfaces del mismo tipo, para indicar cuál quitar.

### Cambiar la red de una interfaz existente

No existe un comando directo. Opciones:

**Opción 1**: detach + attach (VM encendida)

```bash
virsh detach-interface debian-lab network \
  --mac 52:54:00:11:22:33 --persistent
virsh attach-interface debian-lab network isolated \
  --mac 52:54:00:11:22:33 --model virtio --persistent
```

**Opción 2**: editar el XML (VM apagada)

```bash
virsh edit debian-lab
```

```xml
<!-- Cambiar: -->
<interface type='network'>
  <source network='default'/>
  <model type='virtio'/>
</interface>

<!-- A: -->
<interface type='network'>
  <source network='isolated'/>
  <model type='virtio'/>
</interface>
```

---

## 12. Hooks de red en libvirt

Libvirt ejecuta scripts en `/etc/libvirt/hooks/` cuando ocurren ciertos eventos. El hook de red se llama `network`:

```bash
# Crear el hook
sudo vi /etc/libvirt/hooks/network
sudo chmod +x /etc/libvirt/hooks/network
```

El script recibe argumentos:

```bash
#!/bin/bash
# /etc/libvirt/hooks/network
# $1 = nombre de la red
# $2 = acción (start, started, stopped, updated, plugged, unplugged)
# $3 = sub-acción (begin, end)

NETWORK="$1"
ACTION="$2"
PHASE="$3"

case "$NETWORK" in
  default)
    case "$ACTION" in
      started)
        logger "Red default levantada — aplicando reglas extra"
        # Ejemplo: abrir puerto 8080 hacia una VM
        iptables -t nat -A PREROUTING -p tcp --dport 8080 \
          -j DNAT --to-destination 192.168.122.100:80
        ;;
      stopped)
        logger "Red default detenida — limpiando reglas"
        iptables -t nat -D PREROUTING -p tcp --dport 8080 \
          -j DNAT --to-destination 192.168.122.100:80
        ;;
    esac
    ;;
esac
```

Eventos disponibles:

| Evento | Cuándo se ejecuta |
|--------|-------------------|
| `start` / `begin` | Antes de activar la red |
| `started` / `begin` | Después de activar la red (bridge ya existe) |
| `stopped` / `end` | Después de detener la red |
| `updated` / `begin` | Después de un `net-update` |
| `plugged` / `begin` | Cuando se conecta una interfaz de VM |
| `unplugged` / `begin` | Cuando se desconecta una interfaz de VM |

Caso de uso habitual: añadir reglas iptables personalizadas que libvirt no genera automáticamente (port forwarding, restricciones específicas).

---

## 13. Flujo de diagnóstico completo

Cuando una VM no tiene red, sigue este flujo sistemático:

```
¿La red existe?
  virsh net-list --all
  │
  ├─ No existe → Crearla: net-define + net-start
  │
  └─ Existe
      │
      ¿Está activa?
      │
      ├─ No → virsh net-start <red>
      │
      └─ Sí
          │
          ¿El bridge existe en el host?
          ip link show virbr0
          │
          ├─ No → Algo corrupto. net-destroy + net-start
          │
          └─ Sí
              │
              ¿La VM está conectada a esa red?
              virsh domiflist <vm>
              │
              ├─ No → virsh attach-interface ...
              │
              └─ Sí
                  │
                  ¿La VM tiene lease DHCP?
                  virsh net-dhcp-leases <red>
                  │
                  ├─ No → Dentro de VM: dhclient <iface>
                  │       ¿dnsmasq corre? ps aux | grep dnsmasq
                  │
                  └─ Sí
                      │
                      ¿Hay conectividad al gateway?
                      (dentro VM) ping 192.168.122.1
                      │
                      ├─ No → Revisar reglas: iptables -L -n
                      │       ¿br_netfilter? sysctl net.bridge.*
                      │
                      └─ Sí
                          │
                          ¿Hay conectividad a internet?
                          ping 8.8.8.8
                          │
                          ├─ No → ¿NAT funciona?
                          │       iptables -t nat -L POSTROUTING
                          │       ¿IP forwarding?
                          │       sysctl net.ipv4.ip_forward
                          │
                          └─ Sí → ¿Resuelve DNS?
                              dig google.com
                              ├─ No → /etc/resolv.conf en la VM
                              └─ Sí → Red OK, problema en otro lado
```

### Comandos de diagnóstico clave desde el host

```bash
# 1. Estado general
virsh net-list --all
virsh net-info default
virsh net-dhcp-leases default

# 2. Bridge y proceso dnsmasq
ip link show virbr0
ip addr show virbr0
ps aux | grep "[d]nsmasq.*virbr0"

# 3. Interfaces de la VM en el host
virsh domiflist debian-lab
ip link show vnet0

# 4. Reglas de firewall para la red
iptables -L FORWARD -n -v | grep virbr0
iptables -t nat -L POSTROUTING -n -v | grep virbr0

# 5. IP forwarding
sysctl net.ipv4.ip_forward
# Debe ser 1 para redes NAT/routed

# 6. Logs
journalctl -u libvirtd --since "5 minutes ago"
tail -20 /var/log/libvirt/libvirtd.log
```

---

## 14. Errores comunes

### Error 1: editar la red pero no reiniciarla

```bash
virsh net-edit default   # Cambiar rango DHCP
# "¡Editado!" pero nada cambia...
```

**Por qué falla**: `net-edit` solo modifica el XML en disco. La red activa sigue con la configuración anterior.

**Solución**: reiniciar la red después de editar:

```bash
virsh net-destroy default && virsh net-start default
```

Para cambios que no necesitan reinicio (reservas DHCP, entradas DNS), usar `net-update --live --config` en su lugar.

### Error 2: net-undefine sin net-destroy previo

```bash
virsh net-undefine default
# Funciona... pero la red sigue activa como transitoria
virsh net-list --all
# default    active    no    no   ← ¡sorpresa!
```

**Por qué pasa**: `net-undefine` borra el XML pero no detiene la red activa. La red se convierte en transitoria.

**Solución**: siempre detener primero:

```bash
virsh net-destroy default && virsh net-undefine default
```

### Error 3: attach-interface sin --persistent

```bash
virsh attach-interface debian-lab network isolated --model virtio
# Funciona... hasta que apagas la VM
virsh shutdown debian-lab && virsh start debian-lab
# La interfaz desapareció
```

**Por qué pasa**: sin `--persistent`, el cambio solo aplica a la sesión activa.

**Solución**: siempre usar `--persistent` si quieres que sobreviva reinicios:

```bash
virsh attach-interface debian-lab network isolated \
  --model virtio --persistent
```

### Error 4: conflicto de bridges o subnets

```bash
virsh net-start new-net
# error: Failed to start network new-net
# error: internal error: Network is already in use by interface virbr0
```

**Por qué falla**: dos redes intentan usar el mismo nombre de bridge o la misma subnet.

**Solución**: verificar antes de crear:

```bash
# ¿Qué bridges ya existen?
ip link show type bridge

# ¿Qué subnets usan las redes activas?
for net in $(virsh net-list --name); do
  echo "=== $net ==="
  virsh net-dumpxml "$net" | grep -E 'bridge|ip address'
done
```

Usar nombres de bridge y subnets únicos en cada red.

### Error 5: VM arranca sin red porque falta autostart

```bash
# Después de reiniciar el host:
virsh start debian-lab
# La VM arranca, pero no tiene red

virsh net-list --all
# lab-nat    inactive   no   yes
# ← El "no" en Autostart es el problema
```

**Por qué pasa**: la red está definida pero no se activa automáticamente al boot.

**Solución**:

```bash
virsh net-autostart lab-nat
virsh net-start lab-nat
```

---

## 15. Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│              GESTIÓN DE REDES VIRTUALES — virsh                  │
├──────────────────────────────────────────────────────────────────┤
│ INSPECCIONAR                                                     │
│   net-list --all              Todas las redes con su estado      │
│   net-info <red>              Resumen: UUID, bridge, autostart   │
│   net-dumpxml <red>           XML completo de la red             │
│   net-dhcp-leases <red>       IPs entregadas por DHCP            │
│   net-uuid <red>              UUID de la red                     │
├──────────────────────────────────────────────────────────────────┤
│ CICLO DE VIDA                                                    │
│   net-define <xml>            Registrar red (queda inactiva)     │
│   net-create <xml>            Crear transitoria (activa ya)      │
│   net-start <red>             Activar red definida               │
│   net-destroy <red>           Detener red (VMs pierden conexión) │
│   net-autostart <red>         Activar al boot                    │
│   net-autostart --disable     Desactivar al boot                 │
│   net-undefine <red>          Borrar definición (XML)            │
├──────────────────────────────────────────────────────────────────┤
│ MODIFICAR                                                        │
│   net-edit <red>              Editar XML (requiere reinicio)     │
│   net-update <red> add-last   Cambio en caliente (sin reinicio)  │
│     ip-dhcp-host '<host       Ej: añadir reserva DHCP            │
│     mac="..." ip="..."/>'                                        │
│     --live --config                                              │
├──────────────────────────────────────────────────────────────────┤
│ VMs ↔ REDES                                                      │
│   domiflist <vm>              Interfaces de red de la VM         │
│   attach-interface <vm>       Añadir interfaz a la VM            │
│     network <red> --model                                        │
│     virtio --persistent                                          │
│   detach-interface <vm>       Quitar interfaz de la VM           │
│     network --mac XX:XX                                          │
│     --persistent                                                 │
├──────────────────────────────────────────────────────────────────┤
│ DIAGNÓSTICO                                                      │
│   ip link show virbr0         ¿Existe el bridge?                 │
│   ps aux | grep dnsmasq       ¿Corre dnsmasq?                   │
│   iptables -t nat -L          Reglas NAT del host                │
│   sysctl net.ipv4.ip_forward  ¿IP forwarding activo?             │
│   journalctl -u libvirtd      Logs de libvirt                    │
└──────────────────────────────────────────────────────────────────┘
```

---

## 16. Ejercicios

### Ejercicio 1: Crear, inspeccionar y eliminar una red completa

Crea una red NAT llamada `test-curso` con estas características:
- Bridge: `virbr20`
- Subnet: `172.20.0.0/24`
- Gateway (IP del bridge): `172.20.0.1`
- Rango DHCP: `.10` a `.50`
- Dominio DNS: `test.local`
- Una reserva DHCP: MAC `52:54:00:de:ad:01`, nombre `server1`, IP `172.20.0.100`

Luego:

1. Define la red y verifícala con `net-list --all` (debe aparecer inactiva).
2. Arráncala y confirma con `net-info` que el bridge es `virbr20`.
3. Verifica que el bridge existe en el host con `ip addr show virbr20`.
4. Verifica que dnsmasq está corriendo para esta red.
5. Habilita autostart.
6. Exporta el XML canónico con `net-dumpxml` — compara con tu XML original. ¿Qué campos añadió libvirt?
7. Elimina la red completamente (destroy + undefine).

> **Pregunta de reflexión**: ¿Por qué libvirt genera un UUID y una MAC para el bridge aunque tú no los incluiste en el XML original? ¿Qué pasaría si dos redes tuvieran el mismo UUID?

### Ejercicio 2: Modificación en caliente con net-update

Con la red `default` activa y al menos una VM conectada:

1. Verifica los leases DHCP actuales con `net-dhcp-leases`.
2. Usa `net-update` para añadir una reserva DHCP en caliente:
   - MAC: la de tu VM (encuéntrala con `domiflist`).
   - IP: `192.168.122.200`
   - Flags: `--live --config`
3. Dentro de la VM, renueva el lease DHCP (`sudo dhclient -r enp1s0 && sudo dhclient enp1s0`).
4. Verifica que la VM ahora tiene `.200` con `net-dhcp-leases`.
5. Añade una entrada DNS estática con `net-update`: `myserver.lab` → `192.168.122.200`.
6. Desde otra VM (o desde el host), verifica la resolución: `dig @192.168.122.1 myserver.lab`.
7. Elimina ambas entradas (la reserva DHCP y la DNS) con `net-update delete`.

> **Pregunta de reflexión**: ¿Qué diferencia práctica hay entre usar `--live`, `--config`, y `--live --config`? Si usas solo `--live` y luego reinician la red, ¿qué pasa con tus cambios?

### Ejercicio 3: Diagnóstico de red rota

Simula un problema de red y diagnostícalo:

1. Anota la IP actual de una VM con `virsh net-dhcp-leases default`.
2. Detén la red default: `virsh net-destroy default`.
3. Dentro de la VM, intenta hacer `ping 8.8.8.8`. ¿Qué error ves? ¿`ip addr` sigue mostrando la IP?
4. Desde el host, ejecuta `ip link show virbr0`. ¿Qué pasa?
5. Reactiva la red: `virsh net-start default`.
6. ¿La VM recupera conectividad automáticamente? Si no, ¿qué tienes que hacer dentro de la VM?
7. Ahora simula otro escenario: desactiva IP forwarding en el host (`sudo sysctl net.ipv4.ip_forward=0`). ¿La VM puede hacer ping al gateway? ¿Y a `8.8.8.8`? Restaura con `sysctl net.ipv4.ip_forward=1`.

> **Pregunta de reflexión**: ¿Por qué la VM sigue mostrando su IP con `ip addr` incluso después de destruir la red? ¿Qué relación hay entre la interfaz virtual de la VM y el bridge del host?

---

**Último tema de C10 completado.** Con el dominio de `virsh net-*` puedes gestionar todo el ciclo de vida de las redes virtuales: crear topologías a medida, modificarlas en caliente sin desconectar VMs, y diagnosticar problemas de conectividad sistemáticamente. Estas habilidades se usarán directamente en los bloques B07–B09 cuando las VMs necesiten redes específicas para cada lab.
