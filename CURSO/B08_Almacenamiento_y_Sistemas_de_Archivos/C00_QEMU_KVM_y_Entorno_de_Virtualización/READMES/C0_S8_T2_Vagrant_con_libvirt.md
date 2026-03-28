# Vagrant con libvirt (opcional)

## Índice

1. [Qué es Vagrant](#1-qué-es-vagrant)
2. [Instalación con el plugin libvirt](#2-instalación-con-el-plugin-libvirt)
3. [El Vagrantfile](#3-el-vagrantfile)
4. [Ciclo de vida con Vagrant](#4-ciclo-de-vida-con-vagrant)
5. [Vagrantfile multi-VM para labs](#5-vagrantfile-multi-vm-para-labs)
6. [Discos extra en Vagrant](#6-discos-extra-en-vagrant)
7. [Redes en Vagrant](#7-redes-en-vagrant)
8. [Vagrant vs scripts bash](#8-vagrant-vs-scripts-bash)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

> **Nota**: este tema es **opcional**. Los scripts bash de T01 son suficientes
> para todos los labs del curso. Vagrant es una alternativa que puede resultarte
> útil si ya lo conoces o si quieres explorar un enfoque declarativo.

---

## 1. Qué es Vagrant

Vagrant es una herramienta de HashiCorp que permite definir entornos de VMs en
un archivo declarativo (`Vagrantfile`). En vez de escribir una secuencia de
comandos imperativos, describes el estado final que quieres:

```
Scripts bash (imperativo):          Vagrant (declarativo):
┌──────────────────────────┐        ┌──────────────────────────┐
│ qemu-img create ...      │        │ Vagrant.configure do |c| │
│ virt-install ...         │        │   c.vm.box = "debian12"  │
│ virsh attach-disk ...    │        │   c.vm.disk :disk,       │
│ virsh attach-disk ...    │        │     size: "2GB"          │
│ ...20 comandos más...    │        │ end                      │
└──────────────────────────┘        └──────────────────────────┘
      ▼                                    ▼
 ./create-lab.sh                      vagrant up
 ./destroy-lab.sh                     vagrant destroy
```

Vagrant originalmente fue diseñado para VirtualBox. El plugin `vagrant-libvirt`
le permite usar KVM/QEMU a través de libvirt — las mismas herramientas que hemos
aprendido en este capítulo.

```
┌────────────────────────────────────────────────────────┐
│                    STACK                               │
│                                                        │
│  Vagrantfile         ← tú escribes esto                │
│       │                                                │
│  Vagrant CLI         ← vagrant up / destroy / ssh      │
│       │                                                │
│  vagrant-libvirt     ← plugin que traduce a libvirt    │
│       │                                                │
│  libvirtd            ← el daemon que ya conoces        │
│       │                                                │
│  QEMU/KVM            ← el hipervisor                   │
└────────────────────────────────────────────────────────┘
```

---

## 2. Instalación con el plugin libvirt

### 2.1. Instalar Vagrant

```bash
# Fedora
sudo dnf install vagrant

# Debian/Ubuntu
# Descargar desde https://developer.hashicorp.com/vagrant/install
# El paquete de repositorios Debian no siempre está actualizado
```

### 2.2. Instalar el plugin vagrant-libvirt

```bash
# Dependencias de compilación (el plugin incluye componentes nativos)
# Fedora
sudo dnf install libvirt-devel ruby-devel gcc make

# Debian/Ubuntu
sudo apt install libvirt-dev ruby-dev gcc make

# Instalar el plugin
vagrant plugin install vagrant-libvirt
```

### 2.3. Verificar la instalación

```bash
vagrant --version
# Vagrant 2.4.x

vagrant plugin list
# vagrant-libvirt (0.12.x, global)
```

### 2.4. Boxes: las imágenes base de Vagrant

Vagrant usa **boxes** como imágenes base (equivalente a nuestras plantillas
`base-debian12.qcow2`). Los boxes se descargan desde Vagrant Cloud:

```bash
# Descargar un box de Debian 12 para libvirt
vagrant box add debian/bookworm64 --provider libvirt

# Descargar un box de AlmaLinux 9
vagrant box add almalinux/9 --provider libvirt

# Listar boxes instalados
vagrant box list
```

**Predicción**: la descarga toma varios minutos (los boxes pesan ~500MB-1GB).
Se almacenan en `~/.vagrant.d/boxes/`.

Internamente, un box para libvirt contiene un archivo qcow2 + metadatos. Vagrant
lo importa como un volumen en el pool `default` de libvirt.

---

## 3. El Vagrantfile

### 3.1. Vagrantfile mínimo

Crea un directorio para tu lab y un `Vagrantfile`:

```bash
mkdir ~/lab-vagrant && cd ~/lab-vagrant
```

```ruby
# Vagrantfile
Vagrant.configure("2") do |config|
  config.vm.box = "debian/bookworm64"

  config.vm.provider :libvirt do |libvirt|
    libvirt.memory = 2048
    libvirt.cpus = 2
  end
end
```

Este archivo define una sola VM con Debian 12, 2GB de RAM y 2 vCPUs. Equivale a:

```bash
# Lo mismo con nuestras herramientas:
qemu-img create -f qcow2 -b base-debian12.qcow2 -F qcow2 overlay.qcow2
virt-install --name lab-vm --ram 2048 --vcpus 2 \
  --disk path=overlay.qcow2 --import --os-variant debian12 \
  --network network=default --graphics none --noautoconsole
```

### 3.2. Sintaxis del Vagrantfile

El Vagrantfile es código Ruby, pero no necesitas saber Ruby para usarlo. La
estructura siempre es:

```ruby
Vagrant.configure("2") do |config|
  # Configuración global
  config.vm.box = "nombre/del-box"

  # Configuración específica del provider (libvirt)
  config.vm.provider :libvirt do |libvirt|
    libvirt.memory = 2048
    libvirt.cpus = 2
  end

  # Provisioning (ejecutar comandos al crear)
  config.vm.provision "shell", inline: <<-SHELL
    apt-get update
    apt-get install -y vim
  SHELL
end
```

### 3.3. Opciones comunes del provider libvirt

| Opción | Significado | Ejemplo |
|--------|-------------|---------|
| `libvirt.memory` | RAM en MB | `2048` |
| `libvirt.cpus` | Número de vCPUs | `2` |
| `libvirt.machine_virtual_size` | Tamaño del disco principal (GB) | `20` |
| `libvirt.storage_pool_name` | Pool de libvirt a usar | `"default"` |
| `libvirt.management_network_name` | Red de gestión | `"default"` |
| `libvirt.graphics_type` | Tipo de display | `"none"` |

---

## 4. Ciclo de vida con Vagrant

### 4.1. Comandos principales

```
vagrant up        ──► Crear y arrancar la VM (o arrancar si ya existe)
       │
       ▼
vagrant ssh       ──► Conectar por SSH a la VM
       │
       ▼
vagrant halt      ──► Apagar la VM (shutdown limpio)
       │
       ▼
vagrant destroy   ──► Eliminar la VM y sus discos
```

### 4.2. Flujo de trabajo

```bash
# Crear el directorio del lab
mkdir ~/lab-b08-vagrant && cd ~/lab-b08-vagrant

# Crear el Vagrantfile (o copiarlo)
vim Vagrantfile

# Levantar el entorno
vagrant up

# Conectar a la VM
vagrant ssh

# ... hacer el lab ...

# Salir de la VM
exit

# Apagar (conserva el estado)
vagrant halt

# O destruir todo (elimina la VM)
vagrant destroy -f
```

### 4.3. Comparación con virsh

| Acción | virsh | Vagrant |
|--------|-------|---------|
| Crear VM | `virt-install ...` (muchos flags) | `vagrant up` |
| Conectar | `virsh console` o SSH manual | `vagrant ssh` |
| Apagar | `virsh shutdown VM` | `vagrant halt` |
| Eliminar | `virsh destroy` + `undefine --remove-all-storage` | `vagrant destroy` |
| Estado | `virsh list --all` | `vagrant status` |
| IP | `virsh domifaddr VM` | `vagrant ssh` (automático) |

La gran ventaja de Vagrant: `vagrant ssh` configura automáticamente la conexión
SSH (clave, usuario, puerto). No necesitas buscar la IP ni configurar claves.

---

## 5. Vagrantfile multi-VM para labs

### 5.1. Lab de B08: Debian + AlmaLinux

```ruby
# Vagrantfile — B08 Storage Lab
Vagrant.configure("2") do |config|

  # VM 1: Debian 12
  config.vm.define "debian" do |debian|
    debian.vm.box = "debian/bookworm64"
    debian.vm.hostname = "lab-b08-debian"

    debian.vm.provider :libvirt do |libvirt|
      libvirt.memory = 2048
      libvirt.cpus = 2

      # 4 extra disks for RAID/LVM labs
      libvirt.storage :file, size: "2G", type: "qcow2"
      libvirt.storage :file, size: "2G", type: "qcow2"
      libvirt.storage :file, size: "2G", type: "qcow2"
      libvirt.storage :file, size: "2G", type: "qcow2"
    end

    debian.vm.provision "shell", inline: <<-SHELL
      apt-get update
      apt-get install -y mdadm lvm2 parted
    SHELL
  end

  # VM 2: AlmaLinux 9
  config.vm.define "alma" do |alma|
    alma.vm.box = "almalinux/9"
    alma.vm.hostname = "lab-b08-alma"

    alma.vm.provider :libvirt do |libvirt|
      libvirt.memory = 2048
      libvirt.cpus = 2

      # 4 extra disks
      libvirt.storage :file, size: "2G", type: "qcow2"
      libvirt.storage :file, size: "2G", type: "qcow2"
      libvirt.storage :file, size: "2G", type: "qcow2"
      libvirt.storage :file, size: "2G", type: "qcow2"
    end

    alma.vm.provision "shell", inline: <<-SHELL
      dnf install -y mdadm lvm2 parted
    SHELL
  end
end
```

### 5.2. Uso del Vagrantfile multi-VM

```bash
# Levantar todo
vagrant up

# Levantar solo una VM
vagrant up debian
vagrant up alma

# Conectar a cada VM
vagrant ssh debian
vagrant ssh alma

# Estado
vagrant status
# debian    running (libvirt)
# alma      running (libvirt)

# Destruir todo
vagrant destroy -f

# Destruir solo una
vagrant destroy debian -f
```

### 5.3. Qué pasa por debajo

Cuando ejecutas `vagrant up`, Vagrant + vagrant-libvirt:

```
vagrant up
    │
    ├─► Importa el box como volumen en el pool de libvirt
    ├─► Crea un overlay (COW) sobre el box
    ├─► Define la VM en libvirt (genera XML)
    ├─► Crea los discos extra (storage :file)
    ├─► Arranca la VM
    ├─► Espera SSH (configura clave automáticamente)
    └─► Ejecuta provisioners (shell scripts)
```

Puedes ver las VMs creadas por Vagrant con `virsh`:

```bash
virsh list --all
# lab-b08-vagrant_debian    running
# lab-b08-vagrant_alma      running

virsh domblklist lab-b08-vagrant_debian
# vda    ...box-disk001.qcow2     (disco del SO)
# vdb    ...lab-b08-vagrant_debian-vdb.qcow2
# vdc    ...lab-b08-vagrant_debian-vdc.qcow2
# vdd    ...lab-b08-vagrant_debian-vdd.qcow2
# vde    ...lab-b08-vagrant_debian-vde.qcow2
```

Las VMs de Vagrant son VMs normales de libvirt. Puedes inspeccionarlas con
`virsh`, pero es mejor gestionarlas con `vagrant` para mantener la coherencia.

---

## 6. Discos extra en Vagrant

### 6.1. Sintaxis de `libvirt.storage`

```ruby
config.vm.provider :libvirt do |libvirt|
  # Disco de 2G en formato qcow2
  libvirt.storage :file, size: "2G", type: "qcow2"

  # Disco de 5G con bus virtio (default)
  libvirt.storage :file, size: "5G", type: "qcow2", bus: "virtio"

  # Disco con device específico
  libvirt.storage :file, size: "2G", type: "qcow2", device: "vdf"
end
```

### 6.2. Comparación con virsh

| Aspecto | virsh attach-disk | Vagrant storage |
|---------|-------------------|-----------------|
| Sintaxis | Comando con muchos flags | Una línea en Ruby |
| Target | Especificado manualmente (vdb, vdc...) | Asignado automáticamente |
| Persistencia | Requiere `--persistent` | Siempre persistente |
| Creación del disco | Paso separado (`qemu-img create`) | Automática |

### 6.3. Limitación: no hay hot-plug

En Vagrant, los discos extra se definen en el Vagrantfile y se crean con
`vagrant up`. No puedes agregar discos a una VM ya corriendo desde Vagrant
(tendrías que usar `virsh attach-disk` directamente).

Para labs donde necesitas agregar discos dinámicamente, los scripts bash
de T01 son más flexibles.

---

## 7. Redes en Vagrant

### 7.1. Red privada (aislada)

```ruby
Vagrant.configure("2") do |config|
  config.vm.define "server" do |server|
    server.vm.box = "debian/bookworm64"

    # Red privada con IP estática
    server.vm.network :private_network,
      ip: "10.0.1.10",
      libvirt__network_name: "lab-isolated",
      libvirt__dhcp_enabled: false,
      libvirt__forward_mode: "none"
  end

  config.vm.define "client" do |client|
    client.vm.box = "debian/bookworm64"

    client.vm.network :private_network,
      ip: "10.0.1.20",
      libvirt__network_name: "lab-isolated",
      libvirt__dhcp_enabled: false,
      libvirt__forward_mode: "none"
  end
end
```

### 7.2. Opciones de red

| Opción Vagrant | Equivalente libvirt |
|----------------|---------------------|
| `libvirt__forward_mode: "none"` | `<forward>` ausente (aislada) |
| `libvirt__forward_mode: "nat"` | `<forward mode='nat'/>` |
| `libvirt__dhcp_enabled: false` | Sin bloque `<dhcp>` |
| `libvirt__network_name: "X"` | `<name>X</name>` en el XML |

### 7.3. Vagrant siempre crea una red de gestión

Vagrant necesita SSH para funcionar. Siempre crea una interfaz de gestión
conectada a una red NAT (normalmente `vagrant-libvirt`) además de las redes
que definas. Esto es diferente de nuestros scripts, donde controlamos
exactamente qué redes tiene cada VM.

```
VM de Vagrant:
  eth0 ──► red de gestión (vagrant-libvirt, NAT)    ← siempre presente
  eth1 ──► tu red privada (lab-isolated)             ← la que defines tú
```

---

## 8. Vagrant vs scripts bash

### 8.1. Comparación directa

| Aspecto | Scripts bash (T01) | Vagrant |
|---------|-------------------|---------|
| **Transparencia** | Ves cada comando que se ejecuta | Abstracción, magia por debajo |
| **Aprendizaje** | Refuerza virsh/qemu-img | Otra herramienta que aprender |
| **Flexibilidad** | Total control (hot-plug, redes, XML) | Limitado a lo que el plugin soporta |
| **SSH** | Manual (buscar IP, configurar clave) | Automático (`vagrant ssh`) |
| **Provisioning** | Añadir al script | `config.vm.provision` integrado |
| **Reproducibilidad** | Depende del script | Vagrantfile es declarativo |
| **Dependencias** | Solo libvirt (ya instalado) | Vagrant + plugin + Ruby |
| **Debugging** | `virsh` directo | Capa extra de abstracción |

### 8.2. Recomendación para este curso

```
┌───────────────────────────────────────────────────────────────┐
│              ¿QUÉ USAR PARA LOS LABS?                        │
│                                                               │
│  RECOMENDADO: Scripts bash                                    │
│                                                               │
│  Razones:                                                     │
│  1. Ya sabes todo lo que hacen (S03-S07)                      │
│  2. Máxima transparencia — cada paso es visible               │
│  3. Refuerza el aprendizaje de virsh/qemu-img                 │
│  4. Sin dependencias adicionales                              │
│  5. Flexibilidad total para cada tipo de lab                  │
│                                                               │
│  Vagrant es útil si:                                          │
│  - Ya lo conoces de otros proyectos                           │
│  - Prefieres un enfoque declarativo                           │
│  - Quieres SSH automático sin buscar IPs                      │
│  - Necesitas compartir entornos con otros (Vagrantfile)       │
│                                                               │
│  Vagrant NO conviene si:                                      │
│  - No lo has usado antes (curva de aprendizaje extra)         │
│  - Necesitas hot-plug de discos o redes                       │
│  - Quieres entender exactamente qué pasa por debajo           │
│  - Quieres mantener las dependencias mínimas                  │
└───────────────────────────────────────────────────────────────┘
```

### 8.3. Equivalencias rápidas

```
Scripts bash:                      Vagrant:
────────────────────────────────   ────────────────────────
./create-lab.sh                    vagrant up
./destroy-lab.sh                   vagrant destroy -f
virsh console lab-vm               vagrant ssh
virsh list --all                   vagrant status
virsh shutdown lab-vm              vagrant halt
virsh start lab-vm                 vagrant up (si ya existe)
```

---

## 9. Errores comunes

### Error 1: instalar Vagrant sin el plugin libvirt

```bash
# ✗ Vagrant sin plugin usa VirtualBox por defecto
vagrant up
# Error: "The provider 'virtualbox' could not be found"

# ✓ Instalar el plugin primero
vagrant plugin install vagrant-libvirt

# ✓ O especificar el provider
vagrant up --provider=libvirt
```

### Error 2: box no disponible para libvirt

```bash
# ✗ Algunos boxes solo tienen versión VirtualBox
vagrant box add ubuntu/focal64 --provider libvirt
# Error: "No matching version for the requested provider"

# ✓ Buscar boxes que soporten libvirt
# En Vagrant Cloud: filtrar por provider "libvirt"
# Boxes confiables:
#   debian/bookworm64     (libvirt)
#   almalinux/9           (libvirt)
#   generic/debian12      (multi-provider)
#   generic/alma9         (multi-provider)
```

### Error 3: conflicto con VMs manuales

```bash
# ✗ Mezclar gestión Vagrant y virsh
vagrant up                          # crea la VM
virsh undefine vagrant-vm           # la eliminas con virsh
vagrant destroy                     # Vagrant no la encuentra
# Error: confusión de estado

# ✓ Gestionar las VMs de Vagrant solo con Vagrant
vagrant up      # crear
vagrant halt    # apagar
vagrant destroy # eliminar

# ✓ Gestionar tus VMs manuales solo con virsh
# No mezclar las dos herramientas sobre las mismas VMs
```

### Error 4: olvidar `--provider libvirt`

```bash
# ✗ Si tienes VirtualBox instalado, Vagrant puede elegirlo
vagrant up
# Crea la VM en VirtualBox en vez de KVM

# ✓ Forzar libvirt
vagrant up --provider=libvirt

# ✓ O configurar como default en el entorno
export VAGRANT_DEFAULT_PROVIDER=libvirt
# Agregar a ~/.bashrc para que sea permanente
```

### Error 5: permisos de libvirt

```bash
# ✗ Vagrant intenta conectar a libvirt y falla
vagrant up
# Error: "Call to virConnectOpen failed: Failed to connect socket"

# ✓ Tu usuario debe estar en el grupo libvirt
sudo usermod -aG libvirt $USER
# Cerrar sesión y volver a entrar

# ✓ Verificar
groups | grep libvirt
```

---

## 10. Cheatsheet

```bash
# ── Instalación ──────────────────────────────────────────────────
vagrant plugin install vagrant-libvirt
export VAGRANT_DEFAULT_PROVIDER=libvirt    # en ~/.bashrc

# ── Boxes ────────────────────────────────────────────────────────
vagrant box add debian/bookworm64 --provider libvirt
vagrant box add almalinux/9 --provider libvirt
vagrant box list
vagrant box remove NOMBRE

# ── Ciclo de vida ────────────────────────────────────────────────
vagrant up                    # crear y arrancar
vagrant up --provider=libvirt # forzar libvirt
vagrant up debian             # solo una VM del multi-VM
vagrant ssh debian            # conectar por SSH
vagrant status                # ver estado
vagrant halt                  # apagar (conserva la VM)
vagrant destroy -f            # eliminar todo

# ── Vagrantfile: VM básica ───────────────────────────────────────
# Vagrant.configure("2") do |config|
#   config.vm.box = "debian/bookworm64"
#   config.vm.provider :libvirt do |lv|
#     lv.memory = 2048
#     lv.cpus = 2
#   end
# end

# ── Vagrantfile: discos extra ────────────────────────────────────
# config.vm.provider :libvirt do |lv|
#   lv.storage :file, size: "2G", type: "qcow2"
# end

# ── Vagrantfile: red privada ─────────────────────────────────────
# config.vm.network :private_network,
#   ip: "10.0.1.10",
#   libvirt__network_name: "lab-net",
#   libvirt__forward_mode: "none"

# ── Ver lo que Vagrant creó en libvirt ───────────────────────────
virsh list --all | grep vagrant
virsh domblklist NOMBRE_VAGRANT
virsh net-list --all | grep vagrant
```

---

## 11. Ejercicios

### Ejercicio 1: Vagrantfile mínimo

1. Instala Vagrant y el plugin vagrant-libvirt (si no lo tienes)
2. Crea un directorio `~/vagrant-test` con un Vagrantfile que defina una VM
   Debian 12 con 1GB de RAM y 1 vCPU
3. Ejecuta `vagrant up --provider=libvirt`
4. Ejecuta `vagrant ssh` y dentro de la VM ejecuta `lsblk` y `hostname`
5. Ejecuta `vagrant destroy -f`
6. Verifica con `virsh list --all` que no quedan VMs

**Pregunta de reflexión**: con `vagrant ssh` no necesitaste buscar la IP ni
configurar claves SSH. ¿Cómo lo hace Vagrant? Pista: examina el directorio
`.vagrant/` que se creó en `~/vagrant-test/`.

### Ejercicio 2: comparar tiempos

1. Mide cuánto tarda `vagrant up` con un Vagrantfile de una VM con 2 discos extra
2. Mide cuánto tarda tu script `create-lab.sh` (de T01) para crear una VM
   equivalente con 2 discos extra
3. Mide `vagrant destroy -f` vs `./destroy-lab.sh`

**Pregunta de reflexión**: ¿cuál fue más rápido? ¿La diferencia de velocidad
justifica la dependencia adicional de Vagrant? ¿Y si el Vagrantfile tiene 5 VMs?

### Ejercicio 3: Vagrantfile multi-VM

1. Crea un Vagrantfile con 2 VMs: `server` (Debian 12) y `client` (Debian 12)
2. Ambas en una red privada `lab-net` con IPs 10.0.1.10 y 10.0.1.20
3. Ejecuta `vagrant up` y verifica que ambas VMs arrancan
4. Desde `vagrant ssh server`, haz `ping 10.0.1.20`
5. Ejecuta `virsh net-list --all` y observa las redes que Vagrant creó
6. Destruye con `vagrant destroy -f`

**Pregunta de reflexión**: al ejecutar `virsh net-list`, ¿cuántas redes creó
Vagrant? ¿Por qué hay una red extra además de `lab-net`? ¿Qué pasaría si
intentas eliminar esa red de gestión con `virsh net-destroy`?
