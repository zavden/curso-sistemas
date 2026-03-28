# Modificar VMs existentes

## Índice

1. [Dos formas de modificar una VM](#dos-formas-de-modificar-una-vm)
2. [virsh edit: editar el XML directamente](#virsh-edit-editar-el-xml-directamente)
3. [Agregar disco en caliente: attach-disk](#agregar-disco-en-caliente-attach-disk)
4. [Quitar disco: detach-disk](#quitar-disco-detach-disk)
5. [Agregar interfaz de red: attach-interface](#agregar-interfaz-de-red-attach-interface)
6. [Quitar interfaz de red: detach-interface](#quitar-interfaz-de-red-detach-interface)
7. [Cambiar RAM: setmem / setmaxmem](#cambiar-ram-setmem--setmaxmem)
8. [Cambiar CPUs: setvcpus](#cambiar-cpus-setvcpus)
9. [Autostart](#autostart)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## Dos formas de modificar una VM

Hay dos enfoques para cambiar la configuración de una VM existente:

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  Método 1: virsh edit                                            │
│  ─────────────────                                               │
│  Editar el XML directamente con un editor de texto.              │
│  Control total sobre cualquier aspecto de la VM.                 │
│  Cambios aplicados al guardar (algunos requieren reboot).        │
│                                                                  │
│  Método 2: Comandos específicos (attach-disk, setmem, etc.)     │
│  ──────────────────────────────────────────────────────────       │
│  Comandos de virsh que modifican aspectos puntuales.             │
│  Algunos funcionan en caliente (VM corriendo).                   │
│  Más seguros: validan la operación antes de aplicar.             │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

La regla general: usa **comandos específicos** cuando existan para tu caso.
Recurre a `virsh edit` para cambios que no tienen comando dedicado o cuando
necesitas ajustar detalles finos del XML.

---

## virsh edit: editar el XML directamente

```bash
virsh edit debian-lab
```

Esto abre el XML de definición de la VM en tu editor (`$EDITOR`, por defecto
`vi`). Al guardar y salir, libvirt:

1. **Valida** el XML (rechaza errores de sintaxis)
2. **Aplica** los cambios a la definición persistente
3. **Avisa** si los cambios requieren reiniciar la VM

```bash
# Cambiar el editor por defecto
export EDITOR=vim
virsh edit debian-lab
```

### Ejemplo: cambiar la RAM editando XML

```xml
<!-- Antes -->
<memory unit='KiB'>2097152</memory>
<currentMemory unit='KiB'>2097152</currentMemory>

<!-- Después (cambiar a 4 GiB) -->
<memory unit='KiB'>4194304</memory>
<currentMemory unit='KiB'>4194304</currentMemory>
```

Al guardar:

```
Domain 'debian-lab' XML configuration edited.
```

> **Importante**: los cambios vía `virsh edit` se aplican a la **definición
> persistente** (el XML en disco). Si la VM está corriendo, los cambios
> no se aplican en caliente — toman efecto en el **próximo arranque**.

### Ejemplo: agregar un dispositivo serial

```xml
<!-- Agregar dentro de <devices> -->
<serial type='pty'>
  <target type='isa-serial' port='0'>
    <model name='isa-serial'/>
  </target>
</serial>
<console type='pty'>
  <target type='serial' port='0'/>
</console>
```

### Validación automática

Si introduces XML inválido, libvirt lo rechaza:

```bash
virsh edit debian-lab
# (introducir XML mal formado)
# Al guardar:
# error: XML document failed to validate against schema
# error: Failed to define domain 'debian-lab'
# Your changes have been saved to a temporary file.
```

libvirt guarda tus cambios en un archivo temporal para que no pierdas el
trabajo. Puedes corregir y reintentar.

### Cuándo usar virsh edit

| Caso | Usar virsh edit | Usar comando específico |
|------|----------------|------------------------|
| Agregar disco | Posible pero tedioso | `attach-disk` (más simple) |
| Cambiar modelo de NIC | Sí (no hay comando) | — |
| Cambiar tipo de bus | Sí | — |
| Agregar dispositivo USB | Sí | — |
| Cambiar RAM | Posible | `setmem` (más simple) |
| Cambiar CPU topology | Sí | — |
| Agregar PCI passthrough | Sí (complejo) | — |

---

## Agregar disco en caliente: attach-disk

`attach-disk` conecta un disco a una VM, opcionalmente **en caliente** (sin
apagar).

### Disco en caliente (VM corriendo)

```bash
# 1. Crear el disco
qemu-img create -f qcow2 /var/lib/libvirt/images/extra-disk.qcow2 10G

# 2. Conectar a la VM (en caliente)
virsh attach-disk debian-lab \
    /var/lib/libvirt/images/extra-disk.qcow2 vdb \
    --driver qemu --subdriver qcow2 --persistent
```

```
Disk attached successfully
```

Los flags:

| Flag | Significado |
|------|-------------|
| `debian-lab` | Nombre de la VM |
| `/path/to/disk.qcow2` | Ruta al archivo de imagen |
| `vdb` | Nombre del dispositivo dentro del guest |
| `--driver qemu` | Driver del hipervisor |
| `--subdriver qcow2` | Formato de la imagen |
| `--persistent` | Guardar en la definición XML (sobrevive reboots) |

### Sin --persistent vs con --persistent

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  Sin --persistent:                                          │
│  El disco se conecta a la VM corriendo, pero si la         │
│  rearrancar, el disco desaparece. Útil para montar          │
│  un ISO temporalmente.                                      │
│                                                             │
│  Con --persistent:                                          │
│  El disco se conecta Y se añade al XML de la VM.           │
│  Sobrevive reinicios.                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Disco en frío (VM apagada)

```bash
virsh attach-disk debian-lab \
    /var/lib/libvirt/images/extra-disk.qcow2 vdb \
    --driver qemu --subdriver qcow2 --persistent --config
```

El flag `--config` indica que el cambio es solo a la configuración
persistente (no a la VM corriendo). Obligatorio si la VM está apagada.

### Verificar dentro del guest

```bash
# Dentro de la VM (en caliente):
lsblk
```

```
NAME   SIZE  TYPE
vda     20G  disk    ← disco original
vdb     10G  disk    ← nuevo disco conectado en caliente
```

El guest detecta el nuevo disco automáticamente gracias a virtio. No
necesitas rearrancar.

### Montar un ISO como CD-ROM

```bash
virsh attach-disk debian-lab \
    /srv/iso/debian-12.9.0-amd64-netinst.iso sda \
    --type cdrom --mode readonly
```

Útil para instalar paquetes adicionales desde el medio de instalación.

---

## Quitar disco: detach-disk

```bash
virsh detach-disk debian-lab vdb --persistent
```

```
Disk detached successfully
```

Esto desconecta el disco de la VM. El archivo qcow2 **no se borra** del
filesystem; solo se desasocia de la VM.

### Detach en caliente

Si la VM está corriendo, el disco se desconecta inmediatamente. Es
recomendable desmontar el filesystem dentro del guest **antes** de hacer
detach:

```bash
# Dentro del guest:
sudo umount /mnt/data
sudo sync

# Desde el host:
virsh detach-disk debian-lab vdb --persistent
```

> **Predicción**: si haces `detach-disk` mientras el filesystem está montado
> y hay escrituras activas, puedes corromper datos en el disco. Siempre
> desmonta primero dentro del guest.

---

## Agregar interfaz de red: attach-interface

### Conectar a una red existente

```bash
virsh attach-interface debian-lab \
    network default \
    --model virtio --persistent
```

```
Interface attached successfully
```

| Argumento | Significado |
|-----------|-------------|
| `debian-lab` | Nombre de la VM |
| `network` | Tipo de conexión (red virtual de libvirt) |
| `default` | Nombre de la red |
| `--model virtio` | Modelo de NIC (virtio = mejor rendimiento) |
| `--persistent` | Guardar en XML |

### Conectar a un bridge

```bash
virsh attach-interface debian-lab \
    bridge br0 \
    --model virtio --persistent
```

### Verificar dentro del guest

```bash
ip link
```

```
1: lo: <LOOPBACK,UP> ...
2: enp1s0: <BROADCAST,MULTICAST,UP> ...    ← interfaz original
3: enp7s0: <BROADCAST,MULTICAST> ...       ← nueva interfaz
```

La nueva interfaz aparece automáticamente. Necesitas configurar IP (DHCP
o estática) dentro del guest para que sea funcional.

### Especificar MAC address

```bash
virsh attach-interface debian-lab \
    network isolated \
    --model virtio --mac 52:54:00:aa:bb:cc --persistent
```

Útil cuando necesitas un MAC address predecible (para reglas DHCP fijas,
por ejemplo).

---

## Quitar interfaz de red: detach-interface

Para desconectar una interfaz necesitas especificar el tipo y el MAC:

```bash
# Ver las interfaces actuales
virsh domiflist debian-lab
```

```
 Interface   Type      Source    Model    MAC
--------------------------------------------------------------
 vnet0       network   default  virtio   52:54:00:12:34:56
 vnet3       network   default  virtio   52:54:00:aa:bb:cc
```

```bash
# Desconectar por MAC
virsh detach-interface debian-lab network --mac 52:54:00:aa:bb:cc --persistent
```

```
Interface detached successfully
```

---

## Cambiar RAM: setmem / setmaxmem

### Conceptos: memory vs maxmemory

libvirt distingue dos valores de memoria:

```xml
<memory unit='KiB'>4194304</memory>          <!-- máximo posible -->
<currentMemory unit='KiB'>2097152</currentMemory>  <!-- actual -->
```

- **memory** (maxmemory): el techo absoluto. Solo se puede cambiar con la
  VM apagada.
- **currentMemory**: la memoria actualmente asignada. Se puede reducir en
  caliente si `maxmemory > currentMemory`.

### Cambiar memoria con VM apagada

```bash
# Cambiar máximo y actual a 4 GiB (VM debe estar apagada)
virsh setmaxmem debian-lab 4G --config
virsh setmem debian-lab 4G --config
```

El flag `--config` modifica la definición persistente.

### Cambiar memoria en caliente (memory ballooning)

Para ajustar memoria con la VM corriendo, el guest debe tener el driver
`virtio-balloon` (incluido por defecto con `--os-variant`):

```bash
# Primero: asegurar que maxmemory es mayor que lo actual
# (esto se configura con VM apagada)
virsh setmaxmem debian-lab 4G --config
# Rearrancar para que el nuevo máximo tome efecto
virsh shutdown debian-lab
virsh start debian-lab

# Ahora sí, ajustar en caliente:
# Reducir a 1 GiB (dentro del rango 0 - maxmemory)
virsh setmem debian-lab 1G --live

# Volver a 3 GiB
virsh setmem debian-lab 3G --live
```

`--live` aplica el cambio a la VM corriendo. Añade `--config` también si
quieres que persista tras reboot:

```bash
virsh setmem debian-lab 3G --live --config
```

### Verificar

```bash
virsh dominfo debian-lab | grep -i mem
# Max memory:     4194304 KiB
# Used memory:    3145728 KiB
```

Dentro del guest:

```bash
free -h
# total: 3.0 GiB  (refleja el cambio en caliente)
```

> **Predicción**: si reduces la memoria demasiado y el guest necesita más
> de lo asignado, el OOM killer empezará a matar procesos dentro del guest.
> El host no se ve afectado — la aislación funciona.

---

## Cambiar CPUs: setvcpus

### Conceptos: vcpus vs maxvcpus

Similar a la memoria, hay un máximo y un actual:

```xml
<vcpu placement='static' current='2'>4</vcpu>
<!--                      actual    máximo -->
```

### Configurar el máximo (VM apagada)

```bash
virsh setvcpus debian-lab 4 --maximum --config
```

### Hotplug de CPUs (VM corriendo)

```bash
# Añadir CPUs (hasta el máximo)
virsh setvcpus debian-lab 4 --live

# Reducir CPUs
virsh setvcpus debian-lab 1 --live
```

Dentro del guest:

```bash
nproc
# 4 (o 1, según lo que hayas configurado)

lscpu | grep "^CPU(s):"
# CPU(s): 4
```

### Hacer persistente

```bash
virsh setvcpus debian-lab 2 --live --config
```

> **Nota**: el hotplug de CPUs requiere que el guest tenga soporte para
> CPU hotplug. Los kernels Linux modernos (4.x+) lo soportan, pero puede
> requerir `echo 1 > /sys/devices/system/cpu/cpuN/online` para activar
> las CPUs añadidas en caliente.

---

## Autostart

`autostart` configura la VM para que arranque automáticamente cuando
libvirtd se inicia (al boot del host):

```bash
# Habilitar autostart
virsh autostart debian-lab
```

```
Domain 'debian-lab' marked as autostarted
```

```bash
# Deshabilitar autostart
virsh autostart debian-lab --disable
```

```
Domain 'debian-lab' unmarked as autostarted
```

### Cómo funciona internamente

Similar a los storage pools, usa symlinks:

```bash
ls -la /etc/libvirt/qemu/autostart/
```

```
debian-lab.xml -> /etc/libvirt/qemu/debian-lab.xml
```

Si el symlink existe, la VM arranca con libvirtd.

### Verificar

```bash
virsh dominfo debian-lab | grep Autostart
# Autostart: enable
```

### Cuándo usar autostart

| Caso | Autostart |
|------|-----------|
| VM de lab temporal | No (la creas, la usas, la destruyes) |
| VM de servicio (DNS, DHCP) | Sí |
| VM de infraestructura permanente | Sí |
| Plantilla | No (nunca debe arrancar directamente) |

---

## Errores comunes

### 1. attach-disk sin --persistent

```bash
# ❌ Sin --persistent: disco desaparece al rearrancar
virsh attach-disk debian-lab /path/to/disk.qcow2 vdb \
    --driver qemu --subdriver qcow2
# Funciona ahora, pero al hacer virsh reboot → disco desaparece

# ✅ Con --persistent: disco permanente
virsh attach-disk debian-lab /path/to/disk.qcow2 vdb \
    --driver qemu --subdriver qcow2 --persistent
```

### 2. Omitir --subdriver (formato de imagen)

```bash
# ❌ Sin especificar formato: QEMU asume raw
virsh attach-disk debian-lab /path/to/disk.qcow2 vdb \
    --driver qemu --persistent
# El disco se monta pero se interpreta como raw
# Corrupción silenciosa si el formato real es qcow2

# ✅ Especificar formato
virsh attach-disk debian-lab /path/to/disk.qcow2 vdb \
    --driver qemu --subdriver qcow2 --persistent
```

### 3. setmem mayor que maxmemory

```bash
# ❌ Intentar asignar más memoria que el máximo
virsh setmem debian-lab 8G --live
# error: invalid argument: cannot set memory higher than max memory

# ✅ Primero aumentar el máximo (VM apagada)
virsh setmaxmem debian-lab 8G --config
virsh shutdown debian-lab && virsh start debian-lab
virsh setmem debian-lab 8G --live
```

### 4. Nombre de dispositivo duplicado

```bash
# ❌ Intentar usar vdb cuando ya existe
virsh attach-disk debian-lab /path/new.qcow2 vdb --persistent
# error: operation failed: target vdb already exists

# ✅ Usar el siguiente nombre disponible
virsh attach-disk debian-lab /path/new.qcow2 vdc --persistent
```

Verifica los discos existentes antes de conectar uno nuevo:

```bash
virsh domblklist debian-lab
```

```
 Target   Source
-------------------------------------------------
 vda      /var/lib/libvirt/images/debian-lab.qcow2
 vdb      /var/lib/libvirt/images/extra-disk.qcow2
```

### 5. Editar XML de VM corriendo esperando cambios inmediatos

```bash
# ❌ Editar XML con VM corriendo
virsh edit debian-lab
# Cambiar <memory> de 2G a 4G, guardar
# "Domain 'debian-lab' XML configuration edited."
# PERO: la VM sigue con 2G hasta el próximo reboot

# ✅ Para cambios inmediatos, usar comandos --live
virsh setmem debian-lab 4G --live --config
```

`virsh edit` modifica la definición **persistente**. Los cambios solo se
aplican al próximo arranque de la VM, a menos que uses comandos con `--live`.

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  MODIFICAR VMs EXISTENTES — REFERENCIA RÁPIDA                      ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  EDITAR XML:                                                       ║
║    virsh edit VM                  abrir XML en $EDITOR             ║
║    virsh dumpxml VM               ver XML completo (solo lectura)  ║
║                                                                    ║
║  DISCOS:                                                           ║
║    virsh domblklist VM            listar discos actuales            ║
║    virsh attach-disk VM PATH DEV \                                 ║
║      --driver qemu --subdriver qcow2 --persistent                  ║
║    virsh detach-disk VM DEV --persistent                           ║
║                                                                    ║
║  RED:                                                              ║
║    virsh domiflist VM             listar interfaces actuales        ║
║    virsh attach-interface VM network NETNAME \                     ║
║      --model virtio --persistent                                   ║
║    virsh detach-interface VM network --mac XX:XX:XX --persistent    ║
║                                                                    ║
║  MEMORIA:                                                          ║
║    virsh setmaxmem VM SIZE --config       máximo (VM apagada)      ║
║    virsh setmem VM SIZE --live --config   actual (caliente)        ║
║                                                                    ║
║  CPUs:                                                             ║
║    virsh setvcpus VM N --maximum --config   máximo (VM apagada)    ║
║    virsh setvcpus VM N --live --config      actual (caliente)      ║
║                                                                    ║
║  AUTOSTART:                                                        ║
║    virsh autostart VM             habilitar                        ║
║    virsh autostart VM --disable   deshabilitar                     ║
║                                                                    ║
║  FLAGS CLAVE:                                                      ║
║    --persistent    guardar en XML (sobrevive reboots)              ║
║    --live          aplicar a VM corriendo (caliente)               ║
║    --config        aplicar solo a definición (próximo boot)        ║
║    --live --config aplicar ambos (caliente + persistente)          ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Agregar y quitar discos en caliente

**Objetivo**: practicar attach-disk y detach-disk con una VM corriendo.

1. Asegúrate de tener una VM corriendo:
   ```bash
   virsh list
   # Si no hay ninguna, crear una desde plantilla y arrancar
   ```

2. Crea dos discos extra:
   ```bash
   sudo qemu-img create -f qcow2 /var/lib/libvirt/images/hot-disk1.qcow2 5G
   sudo qemu-img create -f qcow2 /var/lib/libvirt/images/hot-disk2.qcow2 3G
   ```

3. Verifica los discos actuales de la VM:
   ```bash
   virsh domblklist lab-test
   ```

4. Conecta el primer disco en caliente:
   ```bash
   sudo virsh attach-disk lab-test \
       /var/lib/libvirt/images/hot-disk1.qcow2 vdb \
       --driver qemu --subdriver qcow2 --persistent
   ```

5. Conéctate al guest y verifica:
   ```bash
   virsh console lab-test
   lsblk
   # ¿Aparece vdb de 5G?
   ```

6. Sin salir del guest, conecta el segundo disco desde otra terminal:
   ```bash
   sudo virsh attach-disk lab-test \
       /var/lib/libvirt/images/hot-disk2.qcow2 vdc \
       --driver qemu --subdriver qcow2 --persistent
   ```

7. Dentro del guest, verifica que apareció sin reiniciar:
   ```bash
   lsblk
   # vda  20G
   # vdb   5G
   # vdc   3G
   ```

8. Formatea y monta vdb dentro del guest:
   ```bash
   sudo mkfs.ext4 /dev/vdb
   sudo mkdir /mnt/data
   sudo mount /dev/vdb /mnt/data
   echo "test" | sudo tee /mnt/data/test.txt
   ```

9. Desmonta y desconecta desde el host:
   ```bash
   # Dentro del guest:
   sudo umount /mnt/data

   # Desde el host (otra terminal):
   sudo virsh detach-disk lab-test vdb --persistent
   ```

10. Verifica dentro del guest que `vdb` desapareció:
    ```bash
    lsblk
    # Solo vda y vdc
    ```

**Pregunta de reflexión**: ¿por qué es importante desmontar el filesystem
dentro del guest **antes** de hacer `detach-disk`? ¿Qué pasaría si el guest
está escribiendo activamente al disco en el momento del detach?

---

### Ejercicio 2: Ajustar recursos dinámicamente

**Objetivo**: cambiar RAM y CPUs en caliente.

1. Con la VM corriendo, verifica los recursos actuales:
   ```bash
   virsh dominfo lab-test | grep -E "CPU|memory|Memory"
   ```

2. Configura un máximo más alto (requiere VM apagada+arranque):
   ```bash
   virsh shutdown lab-test
   # Esperar shutdown...
   sudo virsh setmaxmem lab-test 4G --config
   sudo virsh setvcpus lab-test 4 --maximum --config
   virsh start lab-test
   ```

3. Verifica el nuevo máximo:
   ```bash
   virsh dominfo lab-test | grep -E "Max memory|CPU"
   ```

4. Aumenta la RAM en caliente:
   ```bash
   sudo virsh setmem lab-test 3G --live
   ```

5. Dentro del guest, verifica:
   ```bash
   virsh console lab-test
   free -h
   # ¿Muestra ~3 GiB?
   ```

6. Reduce la RAM:
   ```bash
   sudo virsh setmem lab-test 512M --live
   ```

7. Dentro del guest:
   ```bash
   free -h
   # ¿Muestra ~512 MiB? ¿Algún proceso fue matado por OOM?
   ```

8. Aumenta las CPUs en caliente:
   ```bash
   sudo virsh setvcpus lab-test 4 --live
   ```

9. Dentro del guest:
   ```bash
   nproc
   # ¿Muestra 4?
   # Si no, verificar:
   ls /sys/devices/system/cpu/
   cat /sys/devices/system/cpu/cpu*/online
   ```

10. Restaura los valores originales:
    ```bash
    sudo virsh setmem lab-test 1G --live --config
    sudo virsh setvcpus lab-test 1 --live --config
    ```

**Pregunta de reflexión**: en un entorno de producción con múltiples VMs
compartiendo el mismo host, ¿cómo usarías memory ballooning para redistribuir
RAM entre VMs según la demanda sin apagar ninguna?

---

### Ejercicio 3: virsh edit para cambios avanzados

**Objetivo**: modificar aspectos de la VM que no tienen comando dedicado.

1. Apaga la VM:
   ```bash
   virsh shutdown lab-test
   ```

2. Examina el XML actual:
   ```bash
   virsh dumpxml lab-test | head -30
   ```

3. Abre el XML con virsh edit:
   ```bash
   sudo EDITOR=vim virsh edit lab-test
   ```

4. Realiza estos cambios:
   - Busca la sección `<description>` (puede no existir). Agrega justo
     después de `<name>`:
     ```xml
     <description>VM de laboratorio para ejercicios de filesystem</description>
     ```
   - Busca la sección `<on_poweroff>`, `<on_reboot>`, `<on_crash>`.
     Si existen, verifica los valores. Si no, añade antes de `</domain>`:
     ```xml
     <on_poweroff>destroy</on_poweroff>
     <on_reboot>restart</on_reboot>
     <on_crash>destroy</on_crash>
     ```

5. Guarda y verifica:
   ```bash
   virsh dumpxml lab-test | grep -A1 description
   virsh dumpxml lab-test | grep on_
   ```

6. Intenta introducir XML inválido a propósito:
   ```bash
   sudo EDITOR=vim virsh edit lab-test
   # Borrar la etiqueta de cierre </name>
   # Intentar guardar
   ```
   ¿Qué mensaje muestra libvirt? ¿Aplica los cambios inválidos?

7. Arranca la VM para verificar que los cambios válidos se aplicaron:
   ```bash
   virsh start lab-test
   virsh dominfo lab-test
   ```

**Pregunta de reflexión**: `virsh edit` modifica la definición persistente
pero no la VM corriendo. ¿Existe alguna forma de ver la diferencia entre
la configuración "activa" (en memoria) y la "persistente" (en disco) de
una VM?
(Pista: `virsh dumpxml lab-test` vs `virsh dumpxml lab-test --inactive`)
