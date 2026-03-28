# Gestión de Redes Virtuales con virsh

## Erratas corregidas respecto a los archivos fuente

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.max.md` | 115 | `virsh net-define-as isolated --opzione "none"` — el comando `net-define-as` no existe en virsh y `--opzione` es italiano | Eliminado. Las redes solo se definen mediante XML con `virsh net-define` |
| `README.md` | 481 | `--current: aplica a ambos` — incorrecto, contradice la explicación posterior (líneas 539-541) | `--current` aplica a `--live` si la red está activa, o a `--config` si está inactiva; **no** a ambos |

---

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
14. [Caso de uso en el curso](#14-caso-de-uso-en-el-curso)
15. [Errores comunes](#15-errores-comunes)
16. [Cheatsheet](#16-cheatsheet)
17. [Ejercicios](#17-ejercicios)

---

## 1. El subsistema de redes en libvirt

Los comandos `virsh net-*` gestionan **redes virtuales**, no máquinas virtuales. Una red virtual en libvirt es un objeto con su propio ciclo de vida, independiente de las VMs que la usan.

Cuando defines y arrancas una red, libvirt ejecuta estos pasos internamente:

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
                    └──────┬──────┘
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

Existe `net-create` (sin `net-define` previo), que crea una red **transitoria**: se activa inmediatamente pero no se guarda en disco. Útil para pruebas rápidas.

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

Filtros disponibles (combinables entre sí):

```bash
virsh net-list --inactive      # Solo las inactivas
virsh net-list --persistent    # Solo las persistentes
virsh net-list --transient     # Solo las transitorias
virsh net-list --autostart     # Solo las que tienen autostart
virsh net-list --no-autostart  # Solo las que NO tienen autostart

# Combinación: activas Y con autostart
virsh net-list --active --autostart
```

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
#  2026-03-25 15:30:00   52:54:00:11:22:33  ipv4      192.168.122.100/24  debian-lab   ff:11:22:33:...
#  2026-03-25 15:25:00   52:54:00:44:55:66  ipv4      192.168.122.45/24   fedora-test  ff:44:55:66:...
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
5. Añade reglas iptables/nftables (MASQUERADE si es NAT, REJECT si es aislada).

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

Al no incluir `<forward>`, la red es **aislada**. Tampoco incluimos `<uuid>` — libvirt genera uno automáticamente.

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
- `<port start='1024' end='65535'/>`: rango de puertos para PAT (valor por defecto).

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
- `<section>`: qué parte del XML modificar (`ip-dhcp-host`, `ip-dhcp-range`, `dns-host`, etc.)
- `--live`: aplica al estado en ejecución
- `--config`: aplica al XML persistente
- `--current`: aplica a `--live` si la red está activa, o a `--config` si está inactiva (no a ambos)

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
                    ← NO aplica a ambos simultáneamente
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
# Listar interfaces de red de la VM
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

## 14. Caso de uso en el curso

### Setup para B08 (Almacenamiento)

```bash
# Crear red aislada para labs de almacenamiento
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
virsh net-start lab-isolated
virsh net-autostart lab-isolated

# Ahora VMs con --network network=lab-isolated
# están aisladas — pueden usar discos extra sin riesgo
```

### Cleanup después del lab

```bash
virsh net-destroy lab-isolated
virsh net-undefine lab-isolated
```

---

## 15. Errores comunes

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

## 16. Cheatsheet

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

## 17. Ejercicios

### Ejercicio 1: Inspección de la red default

Ejecuta los siguientes comandos y analiza cada campo de la salida:

```bash
virsh net-list --all
virsh net-info default
virsh net-dumpxml default
```

**Predicción antes de ejecutar:**

<details><summary>¿Qué bridge usa la red default? ¿Qué campos del XML generó libvirt automáticamente?</summary>

La red `default` usa el bridge `virbr0`. Al hacer `net-dumpxml`, los campos que libvirt genera automáticamente son:
- `<uuid>`: identificador único (no lo defines tú en el XML original)
- `<mac address='52:54:00:...'>`: MAC del bridge
- Los rangos de puertos NAT si no los especificaste

El XML canónico siempre incluye estos campos aunque no estuvieran en tu XML original.
</details>

---

### Ejercicio 2: Filtros de net-list

Ejecuta estas variantes y compara las salidas:

```bash
virsh net-list                     # Solo activas
virsh net-list --all               # Todas
virsh net-list --inactive          # Solo inactivas
virsh net-list --autostart         # Solo con autostart
virsh net-list --persistent        # Solo persistentes
```

**Predicción antes de ejecutar:**

<details><summary>Si solo tienes la red default activa con autostart, ¿qué mostrará --inactive? ¿Y --transient?</summary>

- `--inactive`: tabla vacía (no hay redes inactivas si solo existe `default` y está activa).
- `--transient`: tabla vacía (la red `default` es persistente, no transitoria).
- `--autostart`: mostrará solo `default` (tiene autostart habilitado).
- `--persistent`: mostrará `default`.
</details>

---

### Ejercicio 3: Ciclo de vida completo de una red

Crea una red aislada desde cero, recorre todo su ciclo de vida y elimínala:

```bash
# 1. Crear el XML
cat > /tmp/test-lifecycle.xml <<'EOF'
<network>
  <name>test-lifecycle</name>
  <bridge name='virbr99' stp='on' delay='0'/>
  <ip address='10.99.0.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.99.0.10' end='10.99.0.50'/>
    </dhcp>
  </ip>
</network>
EOF

# 2. Definir → verificar estado
virsh net-define /tmp/test-lifecycle.xml
virsh net-list --all | grep test-lifecycle

# 3. Arrancar → verificar bridge
virsh net-start test-lifecycle
ip link show virbr99
ip addr show virbr99

# 4. Verificar dnsmasq
ps aux | grep "dnsmasq.*10.99.0.1"

# 5. Autostart → verificar symlink
virsh net-autostart test-lifecycle
ls -la /etc/libvirt/qemu/networks/autostart/ | grep test

# 6. Limpiar
virsh net-destroy test-lifecycle
virsh net-undefine test-lifecycle
```

**Predicción antes de ejecutar:**

<details><summary>Después de net-define, ¿aparecerá la red como active o inactive? ¿Existirá virbr99 en el host?</summary>

Después de `net-define`, la red aparece como **inactive** — solo se guardó el XML en `/etc/libvirt/qemu/networks/`. El bridge `virbr99` **no existe** todavía; se crea al hacer `net-start`. Del mismo modo, no hay proceso dnsmasq para esta red hasta que se active.
</details>

---

### Ejercicio 4: Red transitoria con net-create

Compara el comportamiento de `net-define` vs `net-create`:

```bash
# Crear red transitoria
cat > /tmp/test-transient.xml <<'EOF'
<network>
  <name>test-transient</name>
  <bridge name='virbr98' stp='on' delay='0'/>
  <ip address='10.98.0.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.98.0.10' end='10.98.0.50'/>
    </dhcp>
  </ip>
</network>
EOF

virsh net-create /tmp/test-transient.xml
virsh net-list --all | grep test-transient
virsh net-info test-transient

# Observar el campo "Persistent"

# Detener y verificar
virsh net-destroy test-transient
virsh net-list --all | grep test-transient
```

**Predicción antes de ejecutar:**

<details><summary>¿Qué muestra el campo "Persistent" de net-info? ¿Qué pasa con la red al hacer net-destroy?</summary>

- `net-info` muestra **Persistent: no** — la red fue creada con `net-create` (transitoria).
- Después de `net-destroy`, la red **desaparece completamente** — no aparece en `net-list --all`. Con `net-define` + `net-destroy`, la red seguiría en la lista como `inactive`.
</details>

---

### Ejercicio 5: Dónde vive cada cosa en disco

Explora los archivos que libvirt genera cuando una red está activa:

```bash
# Con la red default activa:

# 1. XML de definición
sudo ls /etc/libvirt/qemu/networks/
sudo cat /etc/libvirt/qemu/networks/default.xml

# 2. Symlink de autostart
sudo ls -la /etc/libvirt/qemu/networks/autostart/

# 3. PID de dnsmasq
sudo ls /var/run/libvirt/network/
sudo cat /var/run/libvirt/network/default.pid

# 4. Leases
sudo cat /var/lib/libvirt/dnsmasq/default.leases
sudo cat /var/lib/libvirt/dnsmasq/virbr0.status
```

**Predicción antes de ejecutar:**

<details><summary>¿El archivo default.xml contendrá el UUID, o lo calcula libvirt en memoria? ¿Qué formato tiene virbr0.status?</summary>

- `default.xml` **sí contiene el UUID** — es el XML canónico completo (el mismo que devuelve `net-dumpxml`).
- `virbr0.status` es un archivo **JSON** con los leases activos de dnsmasq (formato más moderno que `default.leases`).
- `default.leases` tiene formato plano: `timestamp MAC IP hostname clientid` (una línea por lease).
- El symlink en `autostart/` apunta a `../default.xml`.
</details>

---

### Ejercicio 6: Modificación en caliente con net-update

Con la red `default` activa, practica añadir y eliminar reservas sin reiniciar:

```bash
# 1. Estado actual
virsh net-dumpxml default | grep '<host'

# 2. Añadir reserva DHCP
virsh net-update default add-last ip-dhcp-host \
  '<host mac="52:54:00:de:ad:01" name="test-server" ip="192.168.122.200"/>' \
  --live --config

# 3. Verificar que se aplicó
virsh net-dumpxml default | grep '<host'

# 4. Añadir entrada DNS estática
virsh net-update default add-last dns-host \
  '<host ip="192.168.122.200"><hostname>test-server.lab</hostname></host>' \
  --live --config

# 5. Verificar DNS
dig @192.168.122.1 test-server.lab

# 6. Limpiar (eliminar ambas entradas)
virsh net-update default delete ip-dhcp-host \
  '<host mac="52:54:00:de:ad:01" name="test-server" ip="192.168.122.200"/>' \
  --live --config

virsh net-update default delete dns-host \
  '<host ip="192.168.122.200"><hostname>test-server.lab</hostname></host>' \
  --live --config
```

**Predicción antes de ejecutar:**

<details><summary>Si usas solo --live (sin --config), ¿la reserva sobrevive un net-destroy + net-start?</summary>

**No.** Con solo `--live`, la reserva se aplica al dnsmasq en ejecución pero **no se guarda en el XML**. Al hacer `net-destroy + net-start`, libvirt recrea todo desde el XML en disco, y la reserva se pierde. Por eso se recomienda siempre `--live --config` (efecto inmediato y persistente).
</details>

---

### Ejercicio 7: Conectar una VM a múltiples redes

Practica la gestión de interfaces de red en una VM:

```bash
# 1. Ver interfaces actuales
virsh domiflist debian-lab

# 2. Crear una red aislada para el ejercicio
cat > /tmp/ex-isolated.xml <<'EOF'
<network>
  <name>ex-isolated</name>
  <bridge name='virbr50' stp='on' delay='0'/>
  <ip address='10.50.0.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='10.50.0.10' end='10.50.0.50'/>
    </dhcp>
  </ip>
</network>
EOF
virsh net-define /tmp/ex-isolated.xml
virsh net-start ex-isolated

# 3. Añadir segunda interfaz a la VM
virsh attach-interface debian-lab network ex-isolated \
  --model virtio --persistent

# 4. Verificar que ahora tiene dos interfaces
virsh domiflist debian-lab

# 5. Dentro de la VM, configurar la nueva interfaz
# ip addr show  (ver nueva interfaz)
# sudo dhclient <nueva_interfaz>

# 6. Limpiar
virsh detach-interface debian-lab network \
  --mac <MAC_de_la_segunda_interfaz> --persistent
virsh net-destroy ex-isolated
virsh net-undefine ex-isolated
```

**Predicción antes de ejecutar:**

<details><summary>¿Qué nombre tendrá la segunda interfaz TAP en el host? ¿Y dentro de la VM?</summary>

- En el host: la interfaz TAP se llamará `vnet1` (o el siguiente número disponible si ya hay otras; `vnet0` es la primera interfaz).
- Dentro de la VM: el nombre depende del bus PCI que asigne QEMU. Típicamente será `enp0s7`, `enp0s8` o similar (no `eth1` en distribuciones modernas con nombres predecibles).
- La nueva interfaz aparecerá sin IP hasta que pidas un lease DHCP manualmente.
</details>

---

### Ejercicio 8: Diagnóstico de pérdida de conectividad

Simula una red rota y practica el flujo de diagnóstico:

```bash
# 1. Anotar estado actual
virsh net-dhcp-leases default

# 2. Destruir la red
virsh net-destroy default

# 3. Desde el host, verificar efectos
ip link show virbr0          # ¿Existe el bridge?
ps aux | grep dnsmasq        # ¿Corre dnsmasq?
virsh net-list --all         # ¿Estado de la red?

# 4. Si tienes una VM encendida, comprobar dentro:
# ip addr show               (¿sigue teniendo IP?)
# ping 192.168.122.1          (¿puede contactar al gateway?)

# 5. Restaurar
virsh net-start default

# 6. Verificar recuperación
virsh net-dhcp-leases default
# Dentro de la VM: sudo dhclient -r enp1s0 && sudo dhclient enp1s0
```

**Predicción antes de ejecutar:**

<details><summary>Después de net-destroy, ¿la VM pierde su IP? ¿virbr0 existe?</summary>

- La VM **no pierde su IP** — `ip addr show` sigue mostrándola porque la IP está configurada en la interfaz virtual de la VM, que es independiente del bridge. Pero no puede comunicarse con nada porque el bridge ya no existe.
- `virbr0` **no existe** — `ip link show virbr0` da error `Device "virbr0" does not exist`. El bridge se destruyó con la red.
- Después de `net-start`, la VM puede necesitar renovar su lease DHCP manualmente (`dhclient`) porque dnsmasq arrancó fresco sin recordar la asignación anterior (aunque los leases pueden persistir en disco).
</details>

---

### Ejercicio 9: Crear una red NAT personalizada completa

Crea una red NAT con dominio DNS y reservas, inspecciona todo lo que genera libvirt, y elimínala:

```bash
# 1. Crear la red
cat > /tmp/dev-net.xml <<'EOF'
<network>
  <name>dev-net</name>
  <forward mode='nat'>
    <nat>
      <port start='1024' end='65535'/>
    </nat>
  </forward>
  <bridge name='virbr20' stp='on' delay='0'/>
  <domain name='dev.local' localOnly='yes'/>
  <ip address='172.20.0.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='172.20.0.10' end='172.20.0.50'/>
      <host mac='52:54:00:de:ad:01' name='server1' ip='172.20.0.100'/>
    </dhcp>
  </ip>
</network>
EOF

virsh net-define /tmp/dev-net.xml
virsh net-start dev-net

# 2. Inspeccionar
virsh net-dumpxml dev-net        # ¿Qué campos añadió libvirt?
ip addr show virbr20             # ¿IP correcta?
ps aux | grep "dnsmasq.*172.20"  # ¿dnsmasq escucha?

# 3. Verificar reglas NAT
sudo iptables -t nat -L POSTROUTING -n -v | grep 172.20

# 4. Verificar DNS del dominio
dig @172.20.0.1 server1.dev.local

# 5. Limpiar
virsh net-destroy dev-net
virsh net-undefine dev-net
```

**Predicción antes de ejecutar:**

<details><summary>¿Qué regla de iptables genera libvirt para esta red NAT? ¿Puede el host resolver server1.dev.local?</summary>

- Libvirt genera una regla POSTROUTING con MASQUERADE: `MASQUERADE ... source 172.20.0.0/24 !destination 172.20.0.0/24` — traduce el tráfico saliente de la subnet a la IP del host.
- El host **sí puede resolver** `server1.dev.local` haciendo `dig @172.20.0.1 server1.dev.local` (consultando directamente al dnsmasq de esa red). Pero la resolución DNS normal del host (sin especificar `@172.20.0.1`) **no resolverá** ese nombre, porque el dnsmasq de esta red no es el resolver del host.
- `localOnly='yes'` en `<domain>` evita que dnsmasq reenvíe consultas del dominio `dev.local` a servidores DNS externos.
</details>

---

### Ejercicio 10: Hook de red para port forwarding

Crea un hook que ejecute un comando cuando una red se activa:

```bash
# 1. Crear el hook (con un logger simple, no iptables real)
sudo tee /etc/libvirt/hooks/network <<'HOOKEOF'
#!/bin/bash
NETWORK="$1"
ACTION="$2"
PHASE="$3"

logger "libvirt-hook-network: $NETWORK $ACTION $PHASE"

case "$NETWORK" in
  default)
    case "$ACTION" in
      started)
        logger "libvirt-hook: red default activada"
        ;;
      stopped)
        logger "libvirt-hook: red default detenida"
        ;;
    esac
    ;;
esac
HOOKEOF

sudo chmod +x /etc/libvirt/hooks/network

# 2. Reiniciar libvirtd para que detecte el hook
sudo systemctl restart libvirtd

# 3. Disparar el hook
virsh net-destroy default
virsh net-start default

# 4. Verificar en el journal
journalctl --since "2 minutes ago" | grep libvirt-hook

# 5. Limpiar (si no quieres el hook permanente)
sudo rm /etc/libvirt/hooks/network
sudo systemctl restart libvirtd
```

**Predicción antes de ejecutar:**

<details><summary>¿Cuántas líneas de log generará el ciclo net-destroy + net-start? ¿Qué argumentos recibe el hook?</summary>

El ciclo genera al menos **4 entradas** de log (logger genérico + case específico para cada evento):
1. `libvirt-hook-network: default stopped end` + `libvirt-hook: red default detenida` (del `net-destroy`)
2. `libvirt-hook-network: default start begin` (antes de activar, no entra en `started`)
3. `libvirt-hook-network: default started begin` + `libvirt-hook: red default activada` (después de activar)

Los argumentos son:
- `$1`: nombre de la red (`default`)
- `$2`: acción (`start`, `started`, `stopped`)
- `$3`: fase (`begin`, `end`)

El hook se ejecuta con permisos de root (lo lanza libvirtd).
</details>
