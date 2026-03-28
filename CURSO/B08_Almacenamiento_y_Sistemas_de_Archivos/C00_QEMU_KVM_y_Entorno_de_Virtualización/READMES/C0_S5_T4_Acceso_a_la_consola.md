# Acceso a la consola

## Índice

1. [Tres formas de acceder a una VM](#tres-formas-de-acceder-a-una-vm)
2. [Consola serial: virsh console](#consola-serial-virsh-console)
3. [Configurar getty en ttyS0](#configurar-getty-en-ttys0)
4. [Configurar GRUB para consola serial](#configurar-grub-para-consola-serial)
5. [Consola gráfica: virt-viewer y virt-manager](#consola-gráfica-virt-viewer-y-virt-manager)
6. [SSH: la forma preferida para trabajo real](#ssh-la-forma-preferida-para-trabajo-real)
7. [Cuándo usar cada método](#cuándo-usar-cada-método)
8. [Troubleshooting de consola serial](#troubleshooting-de-consola-serial)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Tres formas de acceder a una VM

```
┌──────────────────────────────────────────────────────────────────┐
│                  MÉTODOS DE ACCESO A UNA VM                     │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐           │
│  │   Consola    │  │   Consola    │  │              │           │
│  │   serial     │  │   gráfica   │  │     SSH      │           │
│  │              │  │              │  │              │           │
│  │ virsh        │  │ virt-viewer  │  │ ssh user@ip  │           │
│  │ console VM   │  │ virt-manager │  │              │           │
│  │              │  │              │  │              │           │
│  │ Texto puro   │  │ Escritorio   │  │ Red          │           │
│  │ Via emulador  │  │ Via SPICE/VNC│  │ Via TCP/IP   │           │
│  │ serial       │  │              │  │              │           │
│  │              │  │              │  │              │           │
│  │ Funciona     │  │ Necesita     │  │ Necesita     │           │
│  │ SIEMPRE      │  │ GUI en host  │  │ red + sshd   │           │
│  │ (incluso sin │  │              │  │              │           │
│  │ red ni GUI)  │  │              │  │              │           │
│  └──────────────┘  └──────────────┘  └──────────────┘           │
│                                                                  │
│  Emergencia ◄──────────────────────────────────► Trabajo diario │
│  (GRUB roto,                                     (scripts,      │
│  red caída)                                      ansible, etc.) │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

Cada método tiene su lugar. La consola serial es tu **red de seguridad**
cuando todo lo demás falla. SSH es lo que usarás en el día a día.

---

## Consola serial: virsh console

### Qué es

QEMU emula un puerto serial (UART 16550) dentro de la VM. El guest ve un
dispositivo `/dev/ttyS0`. QEMU conecta ese puerto serial virtual a un
pseudoterminal (pty) en el host. `virsh console` conecta tu terminal a
ese pty.

```
┌─────────────────────────────────────────────────────────┐
│                                                         │
│  Tu terminal ◄──► virsh console ◄──► pty del host       │
│                                       │                 │
│                                  QEMU serial            │
│                                       │                 │
│                              Guest /dev/ttyS0           │
│                                       │                 │
│                              getty (login prompt)       │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### Conectar

```bash
virsh console debian-lab
```

```
Connected to domain 'debian-lab'
Escape character is ^] (Ctrl + ])

debian-lab login: _
```

Si no ves el prompt de login inmediatamente, pulsa **Enter** una vez.
A veces la consola está conectada pero el prompt no se ha dibujado.

### Desconectar

```
Ctrl+]
```

Esto desconecta tu terminal del pty pero **no** afecta a la VM. La VM
sigue corriendo. Puedes reconectarte con `virsh console` cuantas veces
quieras.

### Arrancar y conectar en un solo paso

```bash
virsh start debian-lab --console
```

Arranca la VM y conecta automáticamente a la consola serial. Verás la
secuencia de arranque completa: BIOS/UEFI → GRUB → kernel → systemd →
login.

---

## Configurar getty en ttyS0

Para que aparezca un prompt de login en la consola serial, el guest necesita
ejecutar un servicio `getty` en `/dev/ttyS0`.

### Debian / Ubuntu

```bash
# Dentro de la VM:
sudo systemctl enable serial-getty@ttyS0.service
sudo systemctl start serial-getty@ttyS0.service
```

`serial-getty@ttyS0` es una instancia del template systemd
`serial-getty@.service`. Al habilitarlo, systemd arranca `agetty` en
`/dev/ttyS0`, que presenta el prompt de login.

Verificar:

```bash
systemctl status serial-getty@ttyS0.service
```

```
● serial-getty@ttyS0.service - Serial Getty on ttyS0
     Loaded: loaded
     Active: active (running)
   Main PID: 234 (agetty)
     CGroup: /system.slice/serial-getty@ttyS0.service
             └─234 /sbin/agetty -o -p -- \u --keep-baud 115200,57600,38400...
```

### AlmaLinux / RHEL / Fedora

Las imágenes cloud ya tienen getty habilitado en ttyS0 por defecto. Si
usaste una instalación desde ISO:

```bash
sudo systemctl enable serial-getty@ttyS0.service
sudo systemctl start serial-getty@ttyS0.service
```

### Verificar que ttyS0 existe

```bash
# Dentro del guest:
ls -l /dev/ttyS0
# crw-rw---- 1 root dialout 4, 64 ... /dev/ttyS0

# Si no existe, el hardware serial no está configurado en la VM
# Verificar en el XML:
# virsh dumpxml debian-lab | grep -A5 serial
```

### Configurar velocidad del puerto

Por defecto, `agetty` negocia automáticamente la velocidad (115200, 57600,
38400, etc.). Si quieres forzar una velocidad:

```bash
# Crear override del servicio
sudo systemctl edit serial-getty@ttyS0.service
```

```ini
[Service]
ExecStart=
ExecStart=-/sbin/agetty --keep-baud 115200 %I $TERM
```

---

## Configurar GRUB para consola serial

El getty solo maneja el login **después** de que el kernel ha arrancado. Si
necesitas ver GRUB y la salida del kernel por la consola serial (esencial
para depurar problemas de arranque), necesitas configurar GRUB.

### Debian

```bash
sudo vim /etc/default/grub
```

```bash
# Añadir consola serial al kernel
GRUB_CMDLINE_LINUX_DEFAULT="quiet"
GRUB_CMDLINE_LINUX="console=tty0 console=ttyS0,115200n8"

# Configurar terminal de GRUB
GRUB_TERMINAL="serial console"
GRUB_SERIAL_COMMAND="serial --speed=115200 --unit=0 --word=8 --parity=no --stop=1"
```

```bash
sudo update-grub
```

### AlmaLinux / RHEL

```bash
sudo vim /etc/default/grub
```

```bash
GRUB_CMDLINE_LINUX="... console=tty0 console=ttyS0,115200n8"
GRUB_TERMINAL="serial console"
GRUB_SERIAL_COMMAND="serial --speed=115200 --unit=0 --word=8 --parity=no --stop=1"
```

```bash
sudo grub2-mkconfig -o /boot/grub2/grub.cfg
```

### Qué significan los parámetros

```bash
console=tty0 console=ttyS0,115200n8
│             │
│             └── Puerto serial: ttyS0, 115200 baud, no parity, 8 bits
│
└── Consola VGA (mantener para no romper la GUI si existe)
```

El kernel de Linux acepta múltiples parámetros `console=`. La salida va a
**todos**, pero solo el **último** listado se convierte en `/dev/console`
(donde van los mensajes de systemd).

> **Predicción**: si pones `console=ttyS0` antes de `console=tty0`, la
> consola gráfica será la principal y la serial solo recibirá una copia.
> Para trabajo sin GUI, pon `console=ttyS0,115200n8` **al final**.

### Qué ves con GRUB serial configurado

Con `virsh start debian-lab --console`:

```
                GNU GRUB  version 2.06

 ┌────────────────────────────────────────────────────┐
 │*Debian GNU/Linux                                   │
 │ Advanced options for Debian GNU/Linux              │
 │                                                    │
 │                                                    │
 └────────────────────────────────────────────────────┘

      Use the ↑ and ↓ keys to select which entry ...
```

Luego los mensajes del kernel:

```
[    0.000000] Linux version 6.1.0-18-amd64 ...
[    0.000000] Command line: ... console=ttyS0,115200n8
[    0.123456] Serial: 8250/16550 driver, 4 ports, IRQ sharing enabled
...
[    3.456789] systemd[1]: Started Serial Getty on ttyS0.
```

Y finalmente el login:

```
Debian GNU/Linux 12 debian-lab ttyS0

debian-lab login: _
```

### Sin GRUB serial configurado

Sin la configuración, `virsh console` conecta pero solo ves una pantalla
vacía hasta que el getty arranca (si está habilitado). No ves GRUB ni los
mensajes del kernel. Si GRUB tiene un timeout o pide seleccionar kernel,
no puedes interactuar.

---

## Consola gráfica: virt-viewer y virt-manager

### virt-viewer

```bash
virt-viewer debian-lab
```

Abre una ventana con la salida gráfica de la VM (como un monitor conectado
a la máquina virtual). Usa el protocolo SPICE o VNC según la configuración
de `--graphics` de la VM.

```
┌──────────────────────────────────────────────────────┐
│  virt-viewer: debian-lab                        ─ □ ✕│
│                                                      │
│  ┌──────────────────────────────────────────────┐    │
│  │                                              │    │
│  │  Debian GNU/Linux 12 debian-lab tty1         │    │
│  │                                              │    │
│  │  debian-lab login: _                         │    │
│  │                                              │    │
│  └──────────────────────────────────────────────┘    │
│                                                      │
│  Send key ▼  │ USB device redirection │ Resize       │
└──────────────────────────────────────────────────────┘
```

Funcionalidades:

| Función | Cómo |
|---------|------|
| Enviar Ctrl+Alt+Del | Menú Send Key |
| Captura del ratón | Click dentro de la ventana |
| Soltar el ratón | Ctrl+Alt (izquierdos) |
| Pantalla completa | F11 |
| Redimensionar ventana | La resolución del guest se ajusta (con SPICE) |

### virt-manager

```bash
virt-manager
```

GUI completa de gestión de VMs. Incluye la consola gráfica pero también
permite crear, configurar y gestionar VMs visualmente.

```
┌──────────────────────────────────────────────────────┐
│  Virtual Machine Manager                        ─ □ ✕│
│                                                      │
│  File  Edit  View  Help                              │
│                                                      │
│  Name              CPU   RAM    Status               │
│  ─────────────────────────────────────────           │
│  debian-lab         12%  2 GiB  Running  ▶           │
│  alma-lab            0%  2 GiB  Shut off ■           │
│  lab-lvm             0%  1 GiB  Shut off ■           │
│                                                      │
│  [Open]  [New]  [Delete]  [Start]  [Pause]           │
└──────────────────────────────────────────────────────┘
```

Doble-click en una VM abre su consola gráfica y las opciones de
configuración de hardware.

### Conexión remota con virt-viewer

Si la VM está en un servidor remoto:

```bash
# VNC directo (si la VM tiene vnc con listen en 0.0.0.0)
virt-viewer --connect qemu+ssh://user@server/system debian-lab

# O configurar un túnel SSH
ssh -L 5900:localhost:5900 user@server
virt-viewer vnc://localhost:5900
```

### Cuándo usar la consola gráfica

- Instalación de VMs con GUI (modo `--graphics spice`)
- Depurar problemas de arranque cuando la consola serial no está configurada
- VMs con entorno de escritorio
- Cuando necesitas ver la pantalla de BIOS/UEFI

---

## SSH: la forma preferida para trabajo real

SSH es el método más cómodo y potente para trabajar con VMs:

```bash
# Obtener la IP de la VM
virsh domifaddr debian-lab
```

```
 Name       MAC address          Protocol   Address
------------------------------------------------------
 vnet0      52:54:00:12:34:56    ipv4       192.168.122.45/24
```

```bash
# Conectar
ssh labuser@192.168.122.45
```

### Ventajas sobre virsh console

| Aspecto | virsh console | SSH |
|---------|--------------|-----|
| Múltiples sesiones | Una sola | Ilimitadas |
| Copiar/pegar | Limitado | Completo |
| Transferir archivos | No | `scp`, `rsync`, `sftp` |
| Port forwarding | No | `-L`, `-R`, `-D` |
| Agente SSH | No | Sí |
| Automatización | Limitada | Ansible, scripts |
| Requiere | Solo VM definida | Red + sshd |
| Funciona sin red | Sí | No |

### Configurar SSH en la VM

Si la VM fue creada con cloud-init o con nuestra plantilla, SSH ya está
configurado. Si la instalaste manualmente:

```bash
# Dentro de la VM:

# Debian
sudo apt install openssh-server
sudo systemctl enable ssh

# AlmaLinux
sudo dnf install openssh-server
sudo systemctl enable sshd
```

### Acceso directo por hostname (opcional)

Para evitar recordar IPs, puedes añadir la VM a `/etc/hosts` del host:

```bash
# En el host:
echo "192.168.122.45 debian-lab" | sudo tee -a /etc/hosts

# Ahora:
ssh labuser@debian-lab
```

O configurar `~/.ssh/config`:

```
Host debian-lab
    HostName 192.168.122.45
    User labuser
    StrictHostKeyChecking no
    UserKnownHostsFile /dev/null
```

`StrictHostKeyChecking no` y `UserKnownHostsFile /dev/null` evitan warnings
de host key changed cuando destruyes y recreas VMs (las host keys cambian).
**Solo para entornos de lab**, nunca en producción.

### Obtener IP automáticamente en scripts

```bash
# Obtener la IP de la primera interfaz
VM_IP=$(virsh domifaddr debian-lab | awk '/ipv4/ {print $4}' | cut -d/ -f1)
echo "$VM_IP"
# 192.168.122.45

# Esperar a que SSH esté disponible y conectar
while ! ssh -o ConnectTimeout=2 labuser@"$VM_IP" true 2>/dev/null; do
    sleep 2
done
ssh labuser@"$VM_IP"
```

---

## Cuándo usar cada método

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  ¿La VM tiene red funcionando?                                  │
│       │                                                         │
│    Sí │              No                                         │
│       │               │                                         │
│       ▼               ▼                                         │
│  ¿Necesitas GUI?    ¿Tienes consola serial configurada?        │
│       │               │                                         │
│    Sí │   No       Sí │            No                           │
│       │    │          │             │                            │
│       ▼    ▼          ▼             ▼                            │
│  virt-   SSH      virsh          virt-viewer                    │
│  viewer            console       (acceso de emergencia          │
│                                   por consola gráfica)          │
│                                                                 │
│  Prioridad para trabajo diario:                                 │
│    1. SSH (siempre que haya red)                                │
│    2. virsh console (cuando la red falla)                       │
│    3. virt-viewer (cuando el serial no está configurado)        │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Escenarios de emergencia

| Situación | Solución |
|-----------|----------|
| Rompí la red de la VM | `virsh console` (no depende de red) |
| Rompí GRUB | `virsh console` con GRUB serial (o `virt-viewer` si no hay serial) |
| Kernel panic al boot | `virsh console` (ver los mensajes del kernel) |
| SSH no arranca | `virsh console` para depurar sshd |
| Todo está bien | SSH |

> **Regla del curso**: siempre configura consola serial en tus VMs de lab.
> Es tu **acceso de emergencia** cuando experimentas con la red, el
> particionado o GRUB. Sin consola serial y sin red, tu única opción es
> `virt-viewer` (que necesita GUI en el host).

---

## Troubleshooting de consola serial

### Síntoma: virsh console conecta pero pantalla vacía

```bash
virsh console debian-lab
# Connected to domain 'debian-lab'
# Escape character is ^] (Ctrl + ])
#
# (nada más — pantalla en blanco)
```

**Causa**: getty no está corriendo en ttyS0.

**Solución** (requiere otro método de acceso, como SSH o virt-viewer):

```bash
# Dentro de la VM:
sudo systemctl enable serial-getty@ttyS0.service
sudo systemctl start serial-getty@ttyS0.service
```

### Síntoma: caracteres basura o salida corrupta

```bash
virsh console debian-lab
# @@##$$%%^^&& ... (basura)
```

**Causa**: velocidad del serial desincronizada entre QEMU y getty.

**Solución**:

```bash
# Dentro de la VM, verificar la velocidad de getty:
systemctl cat serial-getty@ttyS0.service | grep ExecStart
# Debe incluir 115200

# En el host, verificar la configuración de la VM:
virsh dumpxml debian-lab | grep -A5 serial
```

### Síntoma: no se puede reconectar tras Ctrl+]

```bash
virsh console debian-lab
# error: Failed to get console for domain
# error: internal error: character device pty not yet assigned
```

**Causa**: otra sesión de consola ya está conectada.

**Solución**:

```bash
# Desconectar la sesión anterior forzosamente no es directo.
# Opción: reconectar con --force (libvirt 4.10+)
virsh console debian-lab --force
```

### Síntoma: no hay dispositivo serial en la VM

```bash
virsh dumpxml debian-lab | grep serial
# (sin resultado)
```

**Solución**: añadir el dispositivo serial con `virsh edit`:

```bash
virsh edit debian-lab
```

Añadir dentro de `<devices>`:

```xml
<serial type='pty'>
  <target type='isa-serial' port='0'>
    <model name='isa-serial'/>
  </target>
</serial>
<console type='pty'>
  <target type='serial' port='0'/>
</console>
```

Reiniciar la VM para que tome efecto.

---

## Errores comunes

### 1. No configurar getty ni GRUB serial

```bash
# ❌ VM instalada desde ISO con --graphics spice
#    Nunca configuró serial
#    Ahora quiere acceder por virsh console
virsh console debian-lab
# (pantalla vacía para siempre)

# ✅ Siempre configurar serial en VMs de lab:
# 1. GRUB: console=ttyS0,115200n8
# 2. Getty: systemctl enable serial-getty@ttyS0
# 3. Las imágenes cloud y nuestras plantillas ya lo incluyen
```

### 2. Olvidar Ctrl+] para salir

```bash
virsh console debian-lab
# (trabajando dentro de la VM...)
# ¿Cómo salgo?

# ❌ Ctrl+C → mata el proceso dentro de la VM, no desconecta
# ❌ Ctrl+D → cierra la sesión de login, vuelve al prompt de login
# ❌ exit   → cierra la sesión, vuelve al prompt de login

# ✅ Ctrl+]  → desconecta la consola (la VM sigue corriendo)
```

`Ctrl+]` es el **escape character**. No cierra la sesión dentro de la VM,
desconecta tu terminal del pty.

### 3. SSH rechaza la conexión: host key changed

```bash
ssh labuser@192.168.122.45
# @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
# @    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!  @
# @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
# Host key verification failed.
```

**Causa**: destruiste y recreaste la VM. La nueva VM tiene diferentes
SSH host keys pero la misma IP.

```bash
# Solución: borrar la key vieja
ssh-keygen -R 192.168.122.45

# O para labs, ignorar host keys (en ~/.ssh/config):
# StrictHostKeyChecking no
# UserKnownHostsFile /dev/null
```

### 4. virsh domifaddr no muestra IP

```bash
virsh domifaddr debian-lab
# Name   MAC   Protocol   Address
# (vacío)
```

**Causas posibles**:

```bash
# 1. La VM acaba de arrancar — DHCP no ha respondido aún
#    Esperar unos segundos y reintentar

# 2. La VM no tiene QEMU guest agent
#    domifaddr usa el agente para consultar. Sin agente, intentar:
virsh net-dhcp-leases default | grep debian-lab

# 3. La VM no tiene red configurada (network=none)
#    Solo puedes usar virsh console
```

### 5. virt-viewer no conecta remotamente

```bash
# ❌ Desde un cliente sin acceso directo al hipervisor
virt-viewer debian-lab
# error: Failed to connect to the hypervisor

# ✅ Especificar la conexión
virt-viewer --connect qemu+ssh://user@server/system debian-lab
```

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  ACCESO A LA CONSOLA — REFERENCIA RÁPIDA                           ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CONSOLA SERIAL:                                                   ║
║    virsh console VM              conectar                          ║
║    virsh start VM --console      arrancar + conectar               ║
║    Ctrl+]                        desconectar (VM sigue corriendo)  ║
║    virsh console VM --force      forzar reconexión                 ║
║                                                                    ║
║  CONFIGURAR SERIAL (dentro del guest):                             ║
║    systemctl enable serial-getty@ttyS0.service                     ║
║    GRUB: console=tty0 console=ttyS0,115200n8                      ║
║    Debian:  update-grub                                            ║
║    Alma:    grub2-mkconfig -o /boot/grub2/grub.cfg                ║
║                                                                    ║
║  CONSOLA GRÁFICA:                                                  ║
║    virt-viewer VM                SPICE/VNC local                   ║
║    virt-viewer --connect URI VM  SPICE/VNC remoto                  ║
║    virt-manager                  GUI completa                      ║
║                                                                    ║
║  SSH:                                                              ║
║    virsh domifaddr VM            obtener IP de la VM               ║
║    virsh net-dhcp-leases default buscar leases DHCP                ║
║    ssh user@IP                   conectar                          ║
║                                                                    ║
║  PRIORIDAD:                                                        ║
║    1. SSH        → trabajo diario (múltiples sesiones, scp, etc.)  ║
║    2. console    → emergencia (red caída, depurar boot)            ║
║    3. virt-viewer → GUI o cuando serial no está configurado        ║
║                                                                    ║
║  TROUBLESHOOTING SERIAL:                                           ║
║    Pantalla vacía → getty no habilitado en ttyS0                   ║
║    Basura        → velocidad desincronizada                        ║
║    No reconecta  → sesión previa abierta (--force)                 ║
║    Sin serial    → añadir <serial> con virsh edit + reboot         ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Configurar consola serial completa

**Objetivo**: configurar GRUB y getty para acceso serial desde cero.

> **Prerequisito**: VM con acceso por SSH o virt-viewer (para configurar
> el serial la primera vez).

1. Arranca una VM y verifica si la consola serial funciona:
   ```bash
   virsh start lab-test
   virsh console lab-test
   ```
   ¿Ves el prompt de login o una pantalla vacía?

2. Si la pantalla está vacía, accede por SSH (u otro método):
   ```bash
   VM_IP=$(virsh domifaddr lab-test | awk '/ipv4/ {print $4}' | cut -d/ -f1)
   ssh labuser@"$VM_IP"
   ```

3. Habilita el getty:
   ```bash
   sudo systemctl enable serial-getty@ttyS0.service
   sudo systemctl start serial-getty@ttyS0.service
   ```

4. Verifica que funciona sin reiniciar:
   ```bash
   # Desde el host (otra terminal):
   virsh console lab-test
   # Debería mostrar el prompt de login ahora
   # Ctrl+]
   ```

5. Configura GRUB para serial:
   ```bash
   # Dentro de la VM (por SSH):
   sudo sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT="quiet"/GRUB_CMDLINE_LINUX_DEFAULT="quiet console=tty0 console=ttyS0,115200n8"/' /etc/default/grub

   # Verificar el cambio:
   grep GRUB_CMDLINE /etc/default/grub

   # Añadir terminal serial a GRUB
   echo 'GRUB_TERMINAL="serial console"' | sudo tee -a /etc/default/grub
   echo 'GRUB_SERIAL_COMMAND="serial --speed=115200 --unit=0 --word=8 --parity=no --stop=1"' | sudo tee -a /etc/default/grub

   # Regenerar GRUB
   sudo update-grub   # Debian
   # sudo grub2-mkconfig -o /boot/grub2/grub.cfg  # AlmaLinux
   ```

6. Reinicia la VM y observa la secuencia completa por consola serial:
   ```bash
   # Desde el host:
   virsh reboot lab-test
   virsh console lab-test
   ```
   Deberías ver: menú de GRUB → mensajes del kernel → prompt de login.

7. Si ves el menú de GRUB, prueba navegar con las flechas del teclado
   para verificar que la interactividad funciona.

**Pregunta de reflexión**: si rompes GRUB dentro de la VM (por ejemplo,
borrando `/boot/grub/grub.cfg`), ¿podrías repararlo vía consola serial?
¿Qué verías al intentar arrancar?

---

### Ejercicio 2: Comparar los tres métodos de acceso

**Objetivo**: usar los tres métodos y entender las diferencias.

1. Con la VM corriendo, obtén la IP:
   ```bash
   virsh domifaddr lab-test
   ```

2. **Método 1 — SSH**: abre dos sesiones SSH simultáneas:
   ```bash
   # Terminal 1:
   ssh labuser@IP

   # Terminal 2:
   ssh labuser@IP
   ```
   Ambas funcionan al mismo tiempo. Ejecuta `tty` en cada una para ver
   el dispositivo pty asignado.

3. **Método 2 — virsh console**: intenta abrir dos sesiones:
   ```bash
   # Terminal 3:
   virsh console lab-test

   # Terminal 4 (mientras Terminal 3 sigue conectada):
   virsh console lab-test
   ```
   ¿Puedes tener dos sesiones de consola serial simultáneas?

4. **Método 3 — virt-viewer** (si tienes GUI):
   ```bash
   virt-viewer lab-test
   ```
   ¿Puedes usarlo simultáneamente con virsh console?

5. Transfiere un archivo usando cada método:
   ```bash
   # SSH: scp (fácil)
   echo "test file" > /tmp/test.txt
   scp /tmp/test.txt labuser@IP:/tmp/

   # virsh console: no hay transferencia directa
   # Tendrías que copiar/pegar texto manualmente o usar base64
   ```

6. Desconecta la red dentro de la VM:
   ```bash
   # Por SSH:
   sudo ip link set enp1s0 down
   ```
   ¿Qué pasó con la sesión SSH? ¿Puedes seguir usando `virsh console`?

7. Restaura la red:
   ```bash
   # Por virsh console (el único acceso que queda):
   virsh console lab-test
   sudo ip link set enp1s0 up
   sudo dhclient enp1s0   # o: sudo systemctl restart NetworkManager
   ```

**Pregunta de reflexión**: acabas de experimentar exactamente el escenario
donde la consola serial salva el día. Si no la tuvieras configurada y la
red estuviera caída, ¿qué otras opciones tendrías para recuperar la VM?

---

### Ejercicio 3: Automatizar acceso SSH a VMs de lab

**Objetivo**: crear un script que espere a que la VM esté lista y conecte
por SSH.

1. Escribe un script `vm-ssh.sh`:
   ```bash
   #!/bin/bash
   # vm-ssh.sh — Conectar por SSH a una VM, esperando si es necesario
   # Uso: ./vm-ssh.sh VM_NAME [USER]

   VM="${1:?Uso: $0 VM_NAME [USER]}"
   USER="${2:-labuser}"
   TIMEOUT=60
   ```

2. Implementa la lógica:
   - Verificar que la VM está corriendo (`virsh domstate`)
   - Obtener la IP (`virsh domifaddr`)
   - Si no tiene IP todavía, esperar (DHCP puede tardar unos segundos)
   - Esperar a que SSH esté accesible (`ssh -o ConnectTimeout=2`)
   - Conectar

3. Gestiona el caso de host key changed:
   ```bash
   ssh -o StrictHostKeyChecking=no \
       -o UserKnownHostsFile=/dev/null \
       -o LogLevel=ERROR \
       "$USER@$VM_IP"
   ```

4. Prueba:
   ```bash
   chmod +x vm-ssh.sh
   ./vm-ssh.sh lab-test
   ```

5. Prueba con una VM recién creada (que todavía está arrancando):
   ```bash
   sudo ./create-lab-vm.sh lab-ssh-test 0
   ./vm-ssh.sh lab-ssh-test
   # El script debería esperar hasta que SSH esté listo
   ```

6. Limpia:
   ```bash
   sudo ./destroy-lab-vm.sh lab-ssh-test
   ```

**Pregunta de reflexión**: el script usa `StrictHostKeyChecking=no` para
evitar problemas con host keys cambiantes. ¿Por qué esto es aceptable en
un entorno de lab pero peligroso en producción? ¿Qué ataque previene
la verificación de host keys?
