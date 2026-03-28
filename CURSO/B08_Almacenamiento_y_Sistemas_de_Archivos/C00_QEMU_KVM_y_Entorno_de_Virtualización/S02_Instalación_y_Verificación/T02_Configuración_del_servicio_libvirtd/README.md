# Configuración del servicio libvirtd

## Índice

1. [Qué es libvirtd](#qué-es-libvirtd)
2. [Habilitar e iniciar el servicio](#habilitar-e-iniciar-el-servicio)
3. [Socket activation vs service](#socket-activation-vs-service)
4. [Permisos: el grupo libvirt](#permisos-el-grupo-libvirt)
5. [URIs de conexión](#uris-de-conexión)
6. [Directorios importantes](#directorios-importantes)
7. [Configuración de libvirtd](#configuración-de-libvirtd)
8. [Errores comunes](#errores-comunes)
9. [Cheatsheet](#cheatsheet)
10. [Ejercicios](#ejercicios)

---

## Qué es libvirtd

libvirtd es el **daemon** que gestiona toda la infraestructura de
virtualización. No es un hipervisor — es la capa de gestión que se comunica
con KVM/QEMU por nosotros.

```
┌──────────────────────────────────────────────────────────────┐
│  Sin libvirtd:                                               │
│                                                              │
│  Tú → qemu-system-x86_64 -enable-kvm -m 2048 \             │
│       -drive file=disk.qcow2,if=virtio \                    │
│       -netdev user,id=n0 -device virtio-net,netdev=n0 \     │
│       -display spice-app -monitor stdio ...                  │
│                                                              │
│  → Líneas de comando enormes                                 │
│  → Gestionar redes manualmente                              │
│  → Gestionar storage manualmente                            │
│  → Sin snapshots fáciles                                    │
│  → Sin persistencia (al reiniciar, la VM no arranca sola)   │
│                                                              │
│  Con libvirtd:                                               │
│                                                              │
│  Tú → virsh start debian-lab                                │
│                                                              │
│  → libvirtd lee el XML de la VM                             │
│  → Construye la línea de comandos de QEMU automáticamente   │
│  → Configura la red virtual                                 │
│  → Gestiona permisos y seguridad                            │
│  → Persiste la definición para autostart                    │
│  → Provee snapshots, clones, migración                      │
└──────────────────────────────────────────────────────────────┘
```

---

## Habilitar e iniciar el servicio

### Comando básico

```bash
# Habilitar (arrancar al boot) e iniciar inmediatamente
sudo systemctl enable --now libvirtd
```

### Verificar el estado

```bash
systemctl status libvirtd
```

Salida esperada:

```
● libvirtd.service - libvirt legacy monolithic daemon
     Loaded: loaded (/usr/lib/systemd/system/libvirtd.service; enabled; ...)
     Active: active (running) since Thu 2026-03-20 10:00:00 CET; 2h ago
   Main PID: 1234 (libvirtd)
      Tasks: 21 (limit: 32768)
     Memory: 25.4M
        CPU: 1.234s
     CGroup: /system.slice/libvirtd.service
             └─1234 /usr/sbin/libvirtd --timeout 120
```

### Servicios relacionados

libvirtd no es el único servicio. Hay servicios auxiliares que gestiona:

```bash
# Ver todos los servicios de libvirt
systemctl list-units 'libvirt*' --all
systemctl list-units 'virt*' --all
```

Servicios típicos:

```
┌──────────────────────────────────────────────────────────────┐
│  Servicio                         Función                    │
│  ────────                         ───────                    │
│  libvirtd.service                 Daemon principal           │
│  libvirtd.socket                  Socket para activación     │
│  libvirtd-ro.socket               Socket read-only           │
│  libvirtd-admin.socket            Socket de administración   │
│  virtlogd.service                 Logging de consola de VMs  │
│  virtlockd.service                Locking de discos de VMs   │
│  virtnetworkd.service             Gestión de redes virtuales │
│  virtstoraged.service             Gestión de storage pools   │
└──────────────────────────────────────────────────────────────┘
```

En versiones recientes de libvirt, el daemon monolítico (`libvirtd`) se puede
reemplazar por daemons modulares (`virtqemud`, `virtnetworkd`, `virtstoraged`).
Para el curso, el monolítico (`libvirtd`) es más simple y funciona perfecto.

---

## Socket activation vs service

systemd ofrece dos formas de arrancar libvirtd:

### libvirtd.service (daemon directo)

```bash
# El daemon arranca inmediatamente y se queda corriendo
sudo systemctl enable --now libvirtd.service
```

El daemon consume recursos (memoria, CPU mínimo) incluso si no hay VMs. Para
un sistema con VMs activas, esto es lo normal.

### libvirtd.socket (socket activation)

```bash
# El socket escucha conexiones. Cuando llega una,
# systemd arranca libvirtd automáticamente
sudo systemctl enable --now libvirtd.socket
```

```
┌──────────────────────────────────────────────────────────────┐
│  Socket activation:                                          │
│                                                              │
│  1. systemd crea el socket /run/libvirt/libvirt-sock         │
│  2. libvirtd NO está corriendo (ahorra recursos)            │
│  3. Alguien ejecuta: virsh list                              │
│  4. virsh se conecta al socket                               │
│  5. systemd detecta la conexión → arranca libvirtd           │
│  6. libvirtd responde a la petición de virsh                 │
│  7. Después de un timeout (120s sin actividad),              │
│     libvirtd se detiene automáticamente                      │
│                                                              │
│  Ventaja: no consume recursos cuando no se usa              │
│  Desventaja: el primer comando puede tardar 1-2 segundos    │
│              mientras libvirtd arranca                        │
│                                                              │
│  Para el curso: cualquiera de los dos funciona.              │
│  Si vas a trabajar con VMs frecuentemente,                   │
│  enable libvirtd.service directamente.                       │
└──────────────────────────────────────────────────────────────┘
```

### ¿Cuál usar?

```bash
# En muchas distribuciones, enable libvirtd.service habilita
# también los sockets automáticamente. Verificar:
systemctl is-enabled libvirtd.service
systemctl is-enabled libvirtd.socket

# Si ambos están enabled, el socket maneja las conexiones
# y arranca el service bajo demanda.

# Para el curso, lo más simple:
sudo systemctl enable --now libvirtd
# Esto habilita el service y los sockets asociados
```

---

## Permisos: el grupo libvirt

### El problema

Por defecto, libvirtd requiere privilegios de root para gestionar VMs
del sistema (`qemu:///system`). Sin configurar permisos, necesitas `sudo`
para cada operación:

```bash
# Sin grupo libvirt configurado:
virsh list --all
# error: failed to connect to the hypervisor
# error: authentication unavailable

sudo virsh list --all
# Funciona, pero necesitar sudo para todo es incómodo
```

### La solución: grupo libvirt

```bash
# Agregar tu usuario al grupo libvirt
sudo usermod -aG libvirt $USER

# También al grupo kvm (para acceso a /dev/kvm)
sudo usermod -aG kvm $USER
```

**Importante:** los cambios de grupo no toman efecto hasta que cierras
sesión y vuelves a entrar. Opciones:

```bash
# Opción 1: cerrar sesión y volver a entrar (recomendado)
# logout / login

# Opción 2: reiniciar
# sudo reboot

# Opción 3: nueva shell con el grupo (solo para esta shell)
newgrp libvirt
```

### Verificar

```bash
# ¿Estoy en los grupos correctos?
groups
# ... libvirt kvm ...

# ¿Puedo conectar sin sudo?
virsh -c qemu:///system list --all
#  Id   Name   State
# -------------------
# (lista vacía si no hay VMs, pero sin error)
```

### Cómo funciona la autenticación

```
┌──────────────────────────────────────────────────────────────┐
│  Flujo de autenticación:                                     │
│                                                              │
│  virsh list                                                  │
│     │                                                        │
│     ▼                                                        │
│  Conectar a /run/libvirt/libvirt-sock                        │
│     │                                                        │
│     ▼                                                        │
│  libvirtd verifica credenciales:                             │
│     │                                                        │
│     ├── ¿Es root? → acceso total                            │
│     │                                                        │
│     ├── ¿Está en grupo libvirt? → acceso vía polkit          │
│     │   └── polkit rule permite gestión de VMs              │
│     │                                                        │
│     └── ¿Ninguno de los dos? → rechazado                    │
│                                                              │
│  El archivo de política polkit:                              │
│  /usr/share/polkit-1/rules.d/50-libvirt.rules               │
│  (o similar según distribución)                              │
└──────────────────────────────────────────────────────────────┘
```

### qemu:///system vs qemu:///session

libvirt tiene dos modos de conexión:

```
┌──────────────────────────────────────────────────────────────┐
│  qemu:///system (lo que usaremos):                           │
│  ─ VMs gestionadas por libvirtd (daemon del sistema)        │
│  ─ Redes virtuales con DHCP y NAT                           │
│  ─ Storage pools compartidos                                │
│  ─ VMs persisten entre reinicios                            │
│  ─ Requiere grupo libvirt o root                            │
│                                                              │
│  qemu:///session (alternativa sin privilegios):              │
│  ─ VMs gestionadas por un libvirtd por usuario              │
│  ─ Sin redes virtuales (solo SLIRP usermode networking)     │
│  ─ Sin storage pools del sistema                            │
│  ─ Limitado: no puede crear bridges ni redes NAT            │
│  ─ No requiere privilegios especiales                       │
│                                                              │
│  Para el curso: siempre qemu:///system                      │
│  Las redes virtuales y storage pools son esenciales          │
└──────────────────────────────────────────────────────────────┘
```

---

## URIs de conexión

virsh y otras herramientas usan URIs para especificar a qué hipervisor
conectarse:

```bash
# Conexión local al sistema (la más común)
virsh -c qemu:///system list --all

# Si no especificas -c, usa la URI por defecto.
# Configurar la URI por defecto:
export LIBVIRT_DEFAULT_URI="qemu:///system"
# Agregar a ~/.bashrc o ~/.zshrc para que persista

# Conexión remota por SSH
virsh -c qemu+ssh://usuario@servidor/system list --all

# Conexión a sesión de usuario (sin privilegios)
virsh -c qemu:///session list --all
```

### Configurar URI por defecto

Para no tener que escribir `-c qemu:///system` cada vez:

```bash
# En ~/.bashrc o ~/.zshrc:
echo 'export LIBVIRT_DEFAULT_URI="qemu:///system"' >> ~/.bashrc
source ~/.bashrc

# Ahora virsh list funciona sin -c
virsh list --all
```

Alternativamente, en `/etc/libvirt/libvirt.conf` (configuración del cliente):

```bash
# /etc/libvirt/libvirt.conf (o ~/.config/libvirt/libvirt.conf)
uri_default = "qemu:///system"
```

---

## Directorios importantes

```
┌──────────────────────────────────────────────────────────────┐
│  Directorio                          Contenido               │
│  ──────────                          ────────                │
│                                                              │
│  /var/lib/libvirt/images/            Pool de almacenamiento  │
│  │                                   por defecto. Aquí van   │
│  │                                   los discos .qcow2       │
│  │                                                           │
│  ├── debian-lab.qcow2                                        │
│  ├── alma-lab.qcow2                                          │
│  └── extra-disk.qcow2                                        │
│                                                              │
│  /etc/libvirt/                       Configuración global    │
│  ├── libvirtd.conf                   Config del daemon       │
│  ├── qemu.conf                       Config de QEMU          │
│  ├── qemu/                           Definiciones de VMs     │
│  │   ├── debian-lab.xml              (XML persistente)       │
│  │   └── alma-lab.xml                                        │
│  └── qemu/networks/                  Definiciones de redes   │
│      └── default.xml                                         │
│                                                              │
│  /var/log/libvirt/                   Logs                    │
│  └── qemu/                          Logs por VM             │
│      ├── debian-lab.log              Log de QEMU/consola     │
│      └── alma-lab.log                                        │
│                                                              │
│  /run/libvirt/                       Runtime (sockets, PIDs) │
│  ├── libvirt-sock                    Socket de conexión      │
│  └── qemu/                          PIDs de VMs activas     │
└──────────────────────────────────────────────────────────────┘
```

### /var/lib/libvirt/images/ — pool por defecto

Este directorio es el **storage pool** por defecto donde libvirt almacena
las imágenes de disco de las VMs:

```bash
# Ver el pool por defecto
virsh pool-list --all
#  Name      State    Autostart
# --------------------------------
#  default   active   yes

# Información detallada
virsh pool-info default
# Name:           default
# UUID:           ...
# State:          running
# Persistent:     yes
# Autostart:      yes
# Capacity:       100.00 GiB
# Allocation:     15.32 GiB
# Available:      84.68 GiB

# Ruta del pool
virsh pool-dumpxml default | grep path
# <path>/var/lib/libvirt/images</path>

# Listar volúmenes en el pool
virsh vol-list default
```

### /etc/libvirt/qemu/ — definiciones XML

Cada VM tiene un archivo XML que describe su configuración. libvirt lo
gestiona automáticamente:

```bash
# Ver el XML de una VM
virsh dumpxml debian-lab

# Editar el XML de una VM (validación automática)
virsh edit debian-lab
# Se abre en $EDITOR, al guardar libvirt valida el XML

# Los archivos XML están en:
ls /etc/libvirt/qemu/
# debian-lab.xml  alma-lab.xml  networks/
```

### /var/log/libvirt/qemu/ — logs de VMs

Cada VM tiene un archivo de log con la salida de QEMU:

```bash
# Ver el log de una VM
cat /var/log/libvirt/qemu/debian-lab.log

# Útil para diagnosticar problemas de arranque
# El log contiene:
# - La línea de comandos completa de QEMU
# - Errores de dispositivos
# - Información de migración/snapshot
```

---

## Configuración de libvirtd

### /etc/libvirt/libvirtd.conf

Configuración del daemon. Los valores por defecto son correctos para el
curso. Opciones relevantes:

```bash
# Leer la configuración actual (comentarios incluidos)
cat /etc/libvirt/libvirtd.conf

# Opciones relevantes (generalmente no necesitan cambios):

# Logging
#log_level = 1          # 1=debug, 2=info, 3=warn, 4=error
#log_outputs = "1:file:/var/log/libvirt/libvirtd.log"

# Número máximo de clientes simultáneos
#max_clients = 5000

# Timeout para socket activation (segundos)
#keepalive_interval = 5
```

### /etc/libvirt/qemu.conf

Configuración específica de QEMU. Opciones relevantes:

```bash
# Usuario y grupo bajo el que corren los procesos QEMU
#user = "qemu"      # Fedora default
#user = "libvirt-qemu"  # Debian default
#group = "qemu"
#group = "libvirt-qemu"

# Habilitar SELinux/AppArmor para VMs
#security_driver = "selinux"    # Fedora/RHEL
#security_driver = "apparmor"   # Debian/Ubuntu

# Deshabilitar SELinux para QEMU (no recomendado, pero útil para debug)
#security_driver = "none"
```

### Reiniciar después de cambios

```bash
# Después de modificar configuración
sudo systemctl restart libvirtd

# Las VMs corriendo NO se ven afectadas por el restart
# libvirtd se reconecta a los procesos QEMU existentes
```

---

## Errores comunes

### 1. No habilitar el servicio para el boot

```
❌ sudo systemctl start libvirtd
   # Funciona ahora, pero al reiniciar libvirtd no arranca
   # Las VMs con autostart no se inician

✅ sudo systemctl enable --now libvirtd
   # enable: arranca al boot
   # --now: también arranca ahora
   # Las VMs con autostart se iniciarán al reiniciar el host
```

### 2. Olvidar cerrar sesión después de usermod

```
❌ sudo usermod -aG libvirt $USER
   virsh list --all
   # error: failed to connect to the hypervisor
   # "¿No funcionó? Voy a intentar con sudo..."

✅ sudo usermod -aG libvirt $USER
   # CERRAR SESIÓN Y VOLVER A ENTRAR
   # Verificar con:
   groups | grep libvirt
   # Luego:
   virsh list --all
   # (funciona sin sudo)
```

### 3. Confundir qemu:///system con qemu:///session

```
❌ # Crear una VM con virt-manager conectado a session:
   # La VM no tiene red NAT, no aparece en virsh list
   virsh list --all
   # No muestra la VM porque está en session, no en system

✅ # Asegurar la conexión a system:
   export LIBVIRT_DEFAULT_URI="qemu:///system"

   # O explícitamente:
   virsh -c qemu:///system list --all

   # En virt-manager: File → Add Connection → QEMU/KVM
   # Asegurar que dice "QEMU/KVM" y no "QEMU/KVM user session"
```

### 4. Pool default no activo

```
❌ virsh vol-list default
   # error: failed to get pool 'default'
   # El pool existe pero no está activo

✅ # Activar el pool
   virsh pool-start default
   virsh pool-autostart default

   # Si no existe, crearlo:
   virsh pool-define-as default dir --target /var/lib/libvirt/images
   virsh pool-build default
   virsh pool-start default
   virsh pool-autostart default
```

### 5. Red default no activa

```
❌ virt-install ... --network network=default
   # error: Network not found: no network with matching name 'default'

✅ # Ver estado de la red
   virsh net-list --all
   # Si aparece como inactive:
   virsh net-start default
   virsh net-autostart default

   # Si no existe, definirla:
   # (raro, normalmente viene con la instalación de libvirt)
   virsh net-define /usr/share/libvirt/networks/default.xml
   virsh net-start default
   virsh net-autostart default
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│     CONFIGURACIÓN DE LIBVIRTD CHEATSHEET                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  SERVICIO                                                    │
│  systemctl enable --now libvirtd   Habilitar + iniciar      │
│  systemctl status libvirtd         Ver estado                │
│  systemctl restart libvirtd        Reiniciar (no mata VMs)  │
│  systemctl is-enabled libvirtd     ¿Arranca al boot?        │
│                                                              │
│  PERMISOS                                                    │
│  usermod -aG libvirt $USER   Acceso sin sudo                │
│  usermod -aG kvm $USER       Acceso a /dev/kvm              │
│  → cerrar sesión y volver a entrar                          │
│  groups | grep libvirt       Verificar grupo                 │
│                                                              │
│  URI DE CONEXIÓN                                             │
│  qemu:///system              VMs del sistema (usar esta)    │
│  qemu:///session             VMs del usuario (limitada)     │
│  qemu+ssh://host/system      Remota por SSH                 │
│  LIBVIRT_DEFAULT_URI=...     Variable de entorno             │
│  virsh -c qemu:///system     Explícito en cada comando      │
│                                                              │
│  VERIFICAR CONECTIVIDAD                                      │
│  virsh -c qemu:///system list --all    Conectar + listar    │
│  virsh version                         Versiones de la pila │
│  virsh hostname                        Nombre del host       │
│  virsh uri                             URI actual            │
│                                                              │
│  DIRECTORIOS                                                 │
│  /var/lib/libvirt/images/    Pool de disco por defecto       │
│  /etc/libvirt/               Configuración global           │
│  /etc/libvirt/qemu/          XMLs de VMs                     │
│  /etc/libvirt/qemu/networks/ XMLs de redes                  │
│  /var/log/libvirt/qemu/      Logs por VM                    │
│  /run/libvirt/               Sockets, PIDs runtime          │
│                                                              │
│  CONFIGURACIÓN                                               │
│  /etc/libvirt/libvirtd.conf  Config del daemon              │
│  /etc/libvirt/qemu.conf      Config de QEMU (user, seguridad)│
│  virsh dumpxml <vm>          Ver XML de una VM              │
│  virsh edit <vm>             Editar XML con validación      │
│                                                              │
│  RECURSOS PRE-EXISTENTES                                     │
│  virsh net-list --all        Redes virtuales                │
│  virsh pool-list --all       Storage pools                  │
│  virsh net-start default     Activar red por defecto        │
│  virsh pool-start default    Activar pool por defecto       │
│  virsh net-autostart default Autostart de red               │
│  virsh pool-autostart default Autostart de pool             │
│                                                              │
│  SOCKET ACTIVATION                                           │
│  libvirtd.socket    systemd escucha, arranca daemon on-demand│
│  libvirtd.service   daemon corre permanentemente            │
│  Para el curso: cualquiera funciona                          │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Configurar libvirtd desde cero

Habilita y configura libvirtd en tu sistema:

**Tareas:**
1. Habilita e inicia el servicio:
   ```bash
   sudo systemctl enable --now libvirtd
   ```
2. Verifica que está corriendo:
   ```bash
   systemctl status libvirtd
   ```
3. Agrega tu usuario a los grupos necesarios (si no lo hiciste en T01):
   ```bash
   sudo usermod -aG libvirt $USER
   sudo usermod -aG kvm $USER
   ```
4. Cierra sesión y vuelve a entrar
5. Verifica conectividad sin sudo:
   ```bash
   virsh -c qemu:///system list --all
   ```
6. Configura la URI por defecto:
   ```bash
   echo 'export LIBVIRT_DEFAULT_URI="qemu:///system"' >> ~/.bashrc
   source ~/.bashrc
   virsh list --all    # Debe funcionar sin -c
   ```
7. Verifica que la red y pool por defecto están activos:
   ```bash
   virsh net-list --all
   virsh pool-list --all
   ```
   Si alguno no está activo, actívalo con `start` y `autostart`.

**Pregunta de reflexión:** Al reiniciar libvirtd con `systemctl restart
libvirtd`, las VMs que estaban corriendo **no se detienen**. ¿Cómo es posible
si libvirtd es quien las gestiona? (Pista: piensa en la relación entre
libvirtd y los procesos QEMU.)

---

### Ejercicio 2: Explorar los directorios de libvirt

Familiarízate con la estructura de archivos:

**Tareas:**
1. Explora el pool de almacenamiento por defecto:
   ```bash
   ls -la /var/lib/libvirt/images/
   virsh pool-info default
   ```
2. Explora la configuración:
   ```bash
   ls /etc/libvirt/
   ls /etc/libvirt/qemu/
   ```
3. Lee la configuración del daemon (busca las líneas no comentadas):
   ```bash
   grep -v '^#' /etc/libvirt/libvirtd.conf | grep -v '^$'
   ```
4. Lee la configuración de QEMU y encuentra bajo qué usuario corren las VMs:
   ```bash
   grep -E '^(user|group)' /etc/libvirt/qemu.conf
   # Si no hay resultado, buscar entre los comentarios:
   grep -E '#?(user|group)\s*=' /etc/libvirt/qemu.conf
   ```
5. Inspecciona la definición XML de la red default:
   ```bash
   virsh net-dumpxml default
   ```
   Identifica: rango de IPs, puerta de enlace, DHCP, modo de forwarding.
6. Si hay VMs definidas, lee su log:
   ```bash
   ls /var/log/libvirt/qemu/
   # Si hay archivos, examina uno:
   # head -50 /var/log/libvirt/qemu/nombre-vm.log
   ```

**Pregunta de reflexión:** La configuración XML de una VM (en
`/etc/libvirt/qemu/`) contiene TODA la información para reconstruir la VM:
disco, RAM, CPU, red, dispositivos. Si haces backup de este XML y del
archivo .qcow2, ¿puedes restaurar la VM en otro host? ¿Qué condiciones
deben cumplirse en el host destino?

---

### Ejercicio 3: Diagnosticar problemas de conexión

Practica el diagnóstico de problemas típicos:

**Tareas:**
1. Simula un problema de permisos. Verifica qué pasa si intentas conectar
   como un usuario que NO está en el grupo libvirt:
   ```bash
   # ¿Qué mensaje de error recibirías?
   # (No hace falta crear un usuario nuevo — solo conocer el error)
   ```
2. Verifica los sockets de libvirt:
   ```bash
   ls -la /run/libvirt/libvirt-sock*
   # ¿Quién es el dueño? ¿Qué permisos tienen?
   ```
3. Verifica el journal de libvirtd:
   ```bash
   journalctl -u libvirtd --since "1 hour ago" --no-pager | tail -20
   ```
4. Verifica que el socket está escuchando:
   ```bash
   systemctl status libvirtd.socket
   ss -lnp | grep libvirt
   ```
5. Prueba una conexión remota a tu propia máquina (si tienes SSH):
   ```bash
   virsh -c qemu+ssh://localhost/system hostname
   ```
6. Documenta el estado completo de tu instalación:
   ```bash
   echo "=== Servicio ===" && systemctl is-active libvirtd
   echo "=== Grupos ===" && groups
   echo "=== URI ===" && virsh uri
   echo "=== Red ===" && virsh net-list --all
   echo "=== Pool ===" && virsh pool-list --all
   echo "=== VMs ===" && virsh list --all
   ```

**Pregunta de reflexión:** `virsh -c qemu+ssh://host/system` permite gestionar
VMs en un host remoto por SSH. ¿Necesita el host remoto tener algún software
especial además de libvirtd y un servidor SSH? ¿Cómo afecta esto a la
arquitectura de gestión en un entorno con múltiples servidores de virtualización?
