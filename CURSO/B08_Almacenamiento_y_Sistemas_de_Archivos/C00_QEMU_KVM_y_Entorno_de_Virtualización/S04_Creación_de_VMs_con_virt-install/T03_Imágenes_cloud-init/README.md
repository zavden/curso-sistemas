# Imágenes cloud-init

## Índice

1. [El problema que resuelve cloud-init](#el-problema-que-resuelve-cloud-init)
2. [Imágenes cloud: qcow2 pre-construidas](#imágenes-cloud-qcow2-pre-construidas)
3. [Cómo funciona cloud-init](#cómo-funciona-cloud-init)
4. [El disco seed: meta-data y user-data](#el-disco-seed-meta-data-y-user-data)
5. [Escribir meta-data](#escribir-meta-data)
6. [Escribir user-data](#escribir-user-data)
7. [Generar la ISO seed](#generar-la-iso-seed)
8. [Lanzar la VM con virt-install](#lanzar-la-vm-con-virt-install)
9. [Módulos avanzados de user-data](#módulos-avanzados-de-user-data)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## El problema que resuelve cloud-init

En T01 instalamos desde ISO (20-30 minutos, interactivo). En T02 usamos
kickstart/preseed (20 minutos, desatendido pero lento). Cloud-init cambia
el enfoque completamente:

```
┌───────────────────────────────────────────────────────────────────┐
│                                                                   │
│  Instalación desde ISO           Cloud-init                       │
│  ─────────────────────           ──────────                       │
│  1. Descargar ISO (600 MB+)      1. Descargar imagen cloud        │
│  2. Crear disco vacío                (qcow2 ya instalada)         │
│  3. Arrancar instalador          2. Crear ficheros de config      │
│  4. Responder preguntas             (meta-data + user-data)       │
│     (o kickstart/preseed)        3. Generar ISO seed              │
│  5. Instalar paquetes           4. Arrancar con --import          │
│  6. Configurar GRUB             5. VM lista en ~30 segundos       │
│  7. Rearrancar                                                    │
│                                                                   │
│  Tiempo: 15-30 minutos           Tiempo: ~30 segundos             │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

La idea central: alguien (el proveedor de la distro) ya instaló el SO en
una imagen qcow2. Tú solo necesitas **personalizar** esa imagen al primer
arranque: hostname, usuarios, SSH keys, paquetes. Eso es lo que hace
cloud-init.

---

## Imágenes cloud: qcow2 pre-construidas

Las distribuciones publican imágenes "cloud" listas para usar. Son archivos
qcow2 con el SO instalado pero sin configurar (sin contraseñas, sin hostname
fijo, sin SSH keys):

### Dónde descargar

| Distribución | URL | Tamaño típico |
|-------------|-----|---------------|
| Debian 12 | `cloud.debian.org/images/cloud/bookworm/latest/` | ~350 MB |
| AlmaLinux 9 | `repo.almalinux.org/almalinux/9/cloud/x86_64/images/` | ~550 MB |
| Ubuntu 24.04 | `cloud-images.ubuntu.com/noble/current/` | ~550 MB |
| Fedora 41 | `download.fedoraproject.org/pub/fedora/linux/releases/41/Cloud/x86_64/images/` | ~450 MB |

### Descargar Debian 12 cloud

```bash
cd /var/lib/libvirt/images/

# Descargar imagen genérica (funciona con KVM/QEMU)
sudo wget https://cloud.debian.org/images/cloud/bookworm/latest/debian-12-generic-amd64.qcow2

# Verificar
sudo qemu-img info debian-12-generic-amd64.qcow2
```

```
image: debian-12-generic-amd64.qcow2
file format: qcow2
virtual size: 2 GiB (2147483648 bytes)
disk size: 351 MiB
```

Observa: el disco virtual es de solo 2 GiB. Las imágenes cloud vienen con
tamaño mínimo. Lo redimensionaremos al crear la VM.

### Descargar AlmaLinux 9 cloud

```bash
sudo wget https://repo.almalinux.org/almalinux/9/cloud/x86_64/images/AlmaLinux-9-GenericCloud-latest.x86_64.qcow2

sudo qemu-img info AlmaLinux-9-GenericCloud-latest.x86_64.qcow2
```

### Preparar la imagen para uso

Las imágenes cloud son la **base**. Nunca arranques la imagen original
directamente; crea un overlay o una copia:

```bash
# Opción 1: overlay (recomendado — base intacta, overlay ligero)
sudo qemu-img create -f qcow2 \
    -b debian-12-generic-amd64.qcow2 -F qcow2 \
    debian-lab.qcow2 20G

# Opción 2: copia con resize
sudo cp debian-12-generic-amd64.qcow2 debian-lab.qcow2
sudo qemu-img resize debian-lab.qcow2 20G
```

Con la opción 1, el overlay hereda la base y su tamaño se extiende a 20 GiB.
cloud-init y `growpart` (dentro del guest) se encargarán de expandir la
partición y el filesystem al primer arranque.

> **Predicción**: si arrancas la imagen cloud sin un disco seed de cloud-init,
> la VM arrancará pero no podrás hacer login — no hay contraseña de root
> configurada ni SSH keys. cloud-init se quedará esperando un datasource que
> nunca llegará.

---

## Cómo funciona cloud-init

cloud-init es un servicio que se ejecuta dentro del guest al arrancar. Busca
un **datasource** (fuente de configuración) y aplica la personalización:

```
┌─────────────────────────────────────────────────────────────────┐
│                    ARRANQUE CON CLOUD-INIT                      │
│                                                                 │
│  1. BIOS/UEFI → GRUB → kernel + initramfs                      │
│                                                                 │
│  2. systemd arranca servicios del SO                            │
│            │                                                    │
│            ▼                                                    │
│  3. cloud-init.service busca un datasource:                     │
│     ┌────────────────────────────────────────────┐              │
│     │  ¿Hay un CD-ROM con label "cidata"?        │              │
│     │  ¿Hay un servidor de metadata (169.254.x)? │              │
│     │  ¿Hay un disco seed?                       │              │
│     └──────────────────┬─────────────────────────┘              │
│                        │                                        │
│  4. Lee meta-data: hostname, instance-id                        │
│                        │                                        │
│  5. Lee user-data: usuarios, SSH keys, paquetes, scripts        │
│                        │                                        │
│  6. Aplica configuración:                                       │
│     • Establece hostname                                        │
│     • Crea usuarios y contraseñas                               │
│     • Instala SSH keys                                          │
│     • Expande el filesystem (growpart)                          │
│     • Instala paquetes adicionales                              │
│     • Ejecuta scripts custom                                    │
│                        │                                        │
│  7. VM lista para login                                         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Datasources

cloud-init soporta múltiples fuentes de datos. En la nube (AWS, GCP, Azure),
el datasource es un servidor HTTP de metadata. Para KVM local, usamos el
datasource **NoCloud**: un disco ISO con los archivos de configuración.

```
┌─────────────────────────────────────────────────┐
│  Datasource NoCloud (para KVM local)            │
│                                                 │
│  ISO "seed" con label "cidata"                  │
│  ├── meta-data   (identidad de la instancia)    │
│  └── user-data   (configuración del usuario)    │
│                                                 │
│  La VM ve el ISO como un CD-ROM:                │
│  /dev/sr0 → montado automáticamente             │
│  cloud-init lee los archivos y los aplica       │
└─────────────────────────────────────────────────┘
```

---

## El disco seed: meta-data y user-data

Para el datasource NoCloud necesitas crear dos archivos y empaquetarlos
en una ISO:

```
seed.iso
├── meta-data    ← YAML: identidad de la instancia
└── user-data    ← YAML: configuración a aplicar
```

Ambos archivos usan formato **YAML**. Errores de indentación o sintaxis
YAML harán que cloud-init falle silenciosamente.

---

## Escribir meta-data

El archivo `meta-data` es minimalista. Solo necesita dos campos:

```yaml
instance-id: debian-lab-001
local-hostname: debian-lab
```

| Campo | Significado |
|-------|-------------|
| `instance-id` | Identificador único de esta instancia. cloud-init usa este ID para saber si ya se ejecutó. Si cambias el ID, cloud-init se re-ejecuta. |
| `local-hostname` | El hostname que tendrá la VM |

> **Detalle importante**: si recreas la VM con el **mismo** `instance-id`
> y la misma imagen, cloud-init **no** se re-ejecutará porque cree que ya
> configuró esta instancia. Cambia el `instance-id` cuando quieras forzar
> una re-configuración.

### Ejemplo para cada VM de lab

```yaml
# meta-data para VM de lab de filesystems
instance-id: lab-fs-001
local-hostname: lab-fs
```

```yaml
# meta-data para VM de lab de LVM
instance-id: lab-lvm-001
local-hostname: lab-lvm
```

---

## Escribir user-data

El archivo `user-data` es donde sucede la personalización. **Debe** empezar
con la línea `#cloud-config` (sin espacio antes del `#`):

### Ejemplo mínimo

```yaml
#cloud-config

# Contraseña de root (solo para lab)
chpasswd:
  list: |
    root:labpass123
  expire: false

# Crear usuario de laboratorio
users:
  - name: labuser
    plain_text_passwd: labpass123
    lock_passwd: false
    sudo: ALL=(ALL) NOPASSWD:ALL
    shell: /bin/bash
    groups: [adm, sudo]

# Permitir login con contraseña por SSH (default: solo keys)
ssh_pwauth: true
```

### Ejemplo completo para laboratorio

```yaml
#cloud-config

# ── Hostname (también puede ir en meta-data) ──────────────
hostname: debian-lab
fqdn: debian-lab.local

# ── Usuarios ──────────────────────────────────────────────
users:
  - default
  - name: labuser
    plain_text_passwd: labpass123
    lock_passwd: false
    sudo: ALL=(ALL) NOPASSWD:ALL
    shell: /bin/bash
    groups: [adm, sudo]
    ssh_authorized_keys:
      - ssh-ed25519 AAAA...tu_clave_publica_aqui... user@host

# ── Contraseña de root ────────────────────────────────────
chpasswd:
  list: |
    root:labpass123
  expire: false

# ── SSH ───────────────────────────────────────────────────
ssh_pwauth: true
disable_root: false

# ── Paquetes ──────────────────────────────────────────────
package_update: true
package_upgrade: true
packages:
  - vim
  - curl
  - tmux
  - bash-completion
  - man-db
  - htop

# ── Timezone ──────────────────────────────────────────────
timezone: Europe/Madrid

# ── Expandir filesystem ──────────────────────────────────
growpart:
  mode: auto
  devices: ['/']

# ── Comandos al final ────────────────────────────────────
runcmd:
  - echo "cloud-init completado: $(date)" > /root/cloud-init-done.txt
  - systemctl enable --now ssh || systemctl enable --now sshd

# ── Mensaje final en consola ─────────────────────────────
final_message: "Sistema listo después de $UPTIME segundos"
```

### Desglose de cada módulo

#### users

```yaml
users:
  - default              # mantener el usuario "default" de la imagen
  - name: labuser        # nombre del usuario
    plain_text_passwd: labpass123   # contraseña (texto plano, solo lab)
    lock_passwd: false    # permitir login con contraseña
    sudo: ALL=(ALL) NOPASSWD:ALL   # sudo sin contraseña
    shell: /bin/bash
    groups: [adm, sudo]   # grupos adicionales
    ssh_authorized_keys:   # SSH keys autorizadas
      - ssh-ed25519 AAAA...clave...
```

`- default` mantiene el usuario que la imagen cloud trae pre-configurado
(normalmente `debian` en Debian, `almalinux` en Alma). Sin esta línea, ese
usuario se elimina.

#### chpasswd

```yaml
chpasswd:
  list: |
    root:labpass123
    labuser:otraclave456
  expire: false           # no forzar cambio de contraseña al primer login
```

El pipe (`|`) después de `list:` indica un bloque YAML literal: cada línea
es `usuario:contraseña`.

#### packages

```yaml
package_update: true      # apt update / dnf check-update
package_upgrade: true     # apt upgrade / dnf update (actualizar todo)
packages:                  # paquetes adicionales a instalar
  - vim
  - curl
  - tmux
```

cloud-init detecta automáticamente si usar `apt` o `dnf` según la distro.

#### runcmd

```yaml
runcmd:
  - echo "hola" > /tmp/test.txt          # comando simple
  - [ls, -la, /root]                      # comando como lista (sin shell)
  - |                                      # bloque multilínea
    if [ -f /etc/debian_version ]; then
      apt install -y net-tools
    fi
```

`runcmd` se ejecuta **una sola vez**, al final del primer arranque, después
de todos los demás módulos de cloud-init.

#### ssh_authorized_keys

```yaml
users:
  - name: labuser
    ssh_authorized_keys:
      - ssh-ed25519 AAAA...clave1...
      - ssh-rsa AAAA...clave2...
```

Esto coloca las claves en `/home/labuser/.ssh/authorized_keys`. Es la forma
recomendada de acceder a VMs cloud: **SSH key en vez de contraseña**.

Para generar una SSH key (si no tienes una):

```bash
ssh-keygen -t ed25519 -C "lab-key"
cat ~/.ssh/id_ed25519.pub
# Copiar esta línea al user-data
```

---

## Generar la ISO seed

Una vez que tienes `meta-data` y `user-data`, necesitas empaquetarlos en una
ISO con label `cidata`:

### Con cloud-localds (recomendado)

```bash
# Instalar cloud-image-utils (contiene cloud-localds)
# Fedora:
sudo dnf install cloud-utils

# Debian/Ubuntu:
sudo apt install cloud-image-utils

# Generar la ISO
cloud-localds seed.iso user-data meta-data
```

`cloud-localds` es un wrapper que crea la ISO con el label correcto
(`cidata`) y el formato que cloud-init espera.

### Con genisoimage (alternativa manual)

Si no tienes `cloud-localds`:

```bash
genisoimage -output seed.iso -volid cidata -joliet -rock \
    user-data meta-data
```

El flag `-volid cidata` es **crítico**: cloud-init busca un disco con
exactamente ese label.

### Verificar la ISO

```bash
# Ver el contenido
isoinfo -i seed.iso -l
```

```
Directory listing of /
d---------   0    0    0            2048 Mar 20 2026 [     23 02]  .
d---------   0    0    0            2048 Mar 20 2026 [     23 02]  ..
----------   0    0    0              52 Mar 20 2026 [     25 00]  META-DATA;1
----------   0    0    0             847 Mar 20 2026 [     26 00]  USER-DATA;1
```

```bash
# Ver tamaño (será muy pequeño, < 1 MB)
ls -lh seed.iso
# -rw-r--r-- 1 user user 368K ... seed.iso
```

---

## Lanzar la VM con virt-install

La clave es `--import` (no instalar, importar imagen existente) y montar
la ISO seed como CD-ROM:

```bash
sudo virt-install \
    --name debian-cloud \
    --ram 2048 \
    --vcpus 2 \
    --import \
    --disk path=/var/lib/libvirt/images/debian-lab.qcow2 \
    --disk path=/var/lib/libvirt/images/seed.iso,device=cdrom \
    --os-variant debian12 \
    --network network=default \
    --graphics none \
    --noautoconsole
```

Flags nuevos:

| Flag | Significado |
|------|-------------|
| `--import` | No instalar, arrancar directamente desde la imagen qcow2 |
| `--disk path=seed.iso,device=cdrom` | Montar la ISO seed como CD-ROM |
| `--noautoconsole` | No conectar automáticamente a la consola (volver al shell) |

### Flujo completo desde cero

```bash
# 1. Descargar imagen cloud (si no la tienes)
cd /var/lib/libvirt/images/
sudo wget https://cloud.debian.org/images/cloud/bookworm/latest/debian-12-generic-amd64.qcow2

# 2. Crear overlay con tamaño expandido
sudo qemu-img create -f qcow2 \
    -b debian-12-generic-amd64.qcow2 -F qcow2 \
    debian-lab.qcow2 20G

# 3. Crear meta-data
cat > /tmp/meta-data <<'EOF'
instance-id: debian-lab-001
local-hostname: debian-lab
EOF

# 4. Crear user-data
cat > /tmp/user-data <<'EOF'
#cloud-config
users:
  - default
  - name: labuser
    plain_text_passwd: labpass123
    lock_passwd: false
    sudo: ALL=(ALL) NOPASSWD:ALL
    shell: /bin/bash

chpasswd:
  list: |
    root:labpass123
  expire: false

ssh_pwauth: true
disable_root: false

package_update: true
packages:
  - vim
  - curl
  - tmux

timezone: Europe/Madrid

runcmd:
  - echo "cloud-init done: $(date)" > /root/cloud-init-done.txt
EOF

# 5. Generar ISO seed
cloud-localds /var/lib/libvirt/images/seed-debian-lab.iso \
    /tmp/user-data /tmp/meta-data

# 6. Lanzar VM
sudo virt-install \
    --name debian-lab \
    --ram 2048 \
    --vcpus 2 \
    --import \
    --disk path=/var/lib/libvirt/images/debian-lab.qcow2 \
    --disk path=/var/lib/libvirt/images/seed-debian-lab.iso,device=cdrom \
    --os-variant debian12 \
    --network network=default \
    --graphics none \
    --noautoconsole

# 7. Esperar ~30 segundos y conectar
sleep 30
virsh console debian-lab
# Login: labuser / labpass123
```

### Comparación de tiempos

```
Método                          Tiempo hasta VM lista
────────────────────────────    ─────────────────────
ISO manual interactivo          20-30 minutos
ISO con kickstart/preseed       15-20 minutos (desatendido)
Cloud-init                      30-60 segundos
```

> **Predicción**: la primera vez que arranques con cloud-init, tardará algo
> más si pusiste `package_update: true` y `package_upgrade: true` porque
> necesita descargar actualizaciones. Sin esos flags, el arranque es casi
> instantáneo.

---

## Módulos avanzados de user-data

### write_files: crear archivos arbitrarios

```yaml
write_files:
  - path: /etc/motd
    content: |
      ╔══════════════════════════════════╗
      ║   VM de laboratorio - Curso B08  ║
      ╚══════════════════════════════════╝
    owner: root:root
    permissions: '0644'

  - path: /etc/sysctl.d/99-lab.conf
    content: |
      net.ipv4.ip_forward = 1
    owner: root:root
    permissions: '0644'
```

### disk_setup y fs_setup: formatear discos adicionales

Si la VM tiene un segundo disco (para labs de filesystems):

```yaml
disk_setup:
  /dev/vdb:
    table_type: gpt
    layout: true
    overwrite: true

fs_setup:
  - label: data
    filesystem: ext4
    device: /dev/vdb1

mounts:
  - [/dev/vdb1, /mnt/data, ext4, "defaults", "0", "2"]
```

### bootcmd vs runcmd

```yaml
# bootcmd: se ejecuta en CADA arranque, antes del resto de cloud-init
bootcmd:
  - echo "Arrancando..." > /dev/console

# runcmd: se ejecuta UNA SOLA VEZ, al final del primer arranque
runcmd:
  - echo "Configuración inicial completada"
```

### phone_home: notificar que la VM está lista

```yaml
phone_home:
  url: http://192.168.122.1:8080/phone-home/$INSTANCE_ID
  post: [instance_id, hostname, fqdn]
```

Útil para scripts que lanzan múltiples VMs y necesitan saber cuándo están
listas.

### power_state: apagar o rearrancar al terminar

```yaml
power_state:
  mode: reboot
  message: "Rearrancando después de cloud-init"
  timeout: 30
  condition: true
```

---

## Errores comunes

### 1. Olvidar `#cloud-config` en la primera línea

```yaml
# ❌ Sin la directiva, cloud-init ignora todo el archivo
users:
  - name: labuser
    ...

# ✅ La primera línea DEBE ser #cloud-config (sin espacio antes del #)
#cloud-config
users:
  - name: labuser
    ...
```

cloud-init usa esa línea para identificar el formato del user-data. Sin
ella, interpreta el archivo como un script shell o lo ignora.

### 2. Errores de indentación YAML

```yaml
# ❌ Indentación inconsistente (mezcla de tabs y espacios)
users:
  - name: labuser
	plain_text_passwd: pass    # ← TAB en vez de espacios

# ❌ Nivel de indentación incorrecto
users:
- name: labuser               # ← debe estar indentado bajo users:

# ✅ Siempre 2 espacios, nunca tabs
users:
  - name: labuser
    plain_text_passwd: labpass123
```

Valida tu YAML antes de usarlo:

```bash
python3 -c "import yaml; yaml.safe_load(open('user-data'))"
# Sin output = válido
# Con error = muestra la línea problemática
```

### 3. Label de la ISO incorrecto

```bash
# ❌ Label incorrecto — cloud-init no encontrará el datasource
genisoimage -output seed.iso -volid SEED user-data meta-data

# ✅ El label DEBE ser "cidata" (exacto, minúsculas)
genisoimage -output seed.iso -volid cidata user-data meta-data

# ✅ O usar cloud-localds que lo hace automáticamente
cloud-localds seed.iso user-data meta-data
```

### 4. Arrancar imagen cloud sin seed

```bash
# ❌ Sin disco seed — cloud-init no tiene datasource
sudo virt-install \
    --name test --ram 2048 --vcpus 2 --import \
    --disk path=debian-cloud.qcow2 \
    --os-variant debian12

# La VM arranca pero:
#   - No hay contraseña de root
#   - No hay SSH keys
#   - cloud-init espera ~120 segundos buscando datasource
#   - No puedes hacer login

# ✅ Siempre incluir el seed
    --disk path=seed.iso,device=cdrom
```

### 5. Mismo instance-id al recrear la VM

```yaml
# ❌ Si destruyes y recreas la VM con el mismo instance-id,
#    cloud-init NO se re-ejecuta (cree que ya configuró esta instancia)
instance-id: lab-001    # mismo que la vez anterior

# ✅ Cambiar instance-id para forzar re-configuración
instance-id: lab-002    # incrementar o cambiar
```

cloud-init guarda el `instance-id` en `/var/lib/cloud/data/instance-id`.
Si coincide con el del seed, no aplica la configuración de nuevo.

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  CLOUD-INIT — REFERENCIA RÁPIDA                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DESCARGAR IMÁGENES CLOUD:                                         ║
║    Debian:  cloud.debian.org/images/cloud/bookworm/latest/         ║
║    Alma:    repo.almalinux.org/almalinux/9/cloud/x86_64/images/    ║
║                                                                    ║
║  PREPARAR IMAGEN:                                                  ║
║    qemu-img create -f qcow2 -b base-cloud.qcow2 -F qcow2 \       ║
║        vm.qcow2 20G                                               ║
║                                                                    ║
║  CREAR meta-data:                                                  ║
║    instance-id: mi-vm-001                                          ║
║    local-hostname: mi-vm                                           ║
║                                                                    ║
║  CREAR user-data (debe empezar con #cloud-config):                 ║
║    #cloud-config                                                   ║
║    users:                                                          ║
║      - name: USER                                                  ║
║        plain_text_passwd: PASS                                     ║
║        lock_passwd: false                                          ║
║        sudo: ALL=(ALL) NOPASSWD:ALL                                ║
║    ssh_pwauth: true                                                ║
║    packages: [vim, curl, tmux]                                     ║
║                                                                    ║
║  GENERAR ISO SEED:                                                 ║
║    cloud-localds seed.iso user-data meta-data                      ║
║                                                                    ║
║  LANZAR VM:                                                        ║
║    virt-install --name VM --ram 2048 --vcpus 2 --import \          ║
║      --disk path=vm.qcow2 \                                       ║
║      --disk path=seed.iso,device=cdrom \                           ║
║      --os-variant OS --network network=default                     ║
║                                                                    ║
║  VALIDAR YAML:                                                     ║
║    python3 -c "import yaml; yaml.safe_load(open('user-data'))"     ║
║                                                                    ║
║  DEPURAR CLOUD-INIT (dentro de la VM):                             ║
║    cloud-init status                    # estado actual             ║
║    cloud-init status --long             # detalle con errores       ║
║    cat /var/log/cloud-init-output.log   # log completo             ║
║    cat /var/lib/cloud/data/instance-id  # ID actual                ║
║    cloud-init clean --reboot            # forzar re-ejecución      ║
║                                                                    ║
║  REGLAS:                                                           ║
║    • Primera línea: #cloud-config (sin espacio antes del #)        ║
║    • Label de ISO: cidata (exacto)                                 ║
║    • Cambiar instance-id para forzar re-configuración              ║
║    • YAML: 2 espacios, nunca tabs                                  ║
║    • Nunca arrancar imagen cloud sin seed                          ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Crear y validar archivos seed

**Objetivo**: escribir meta-data y user-data sin lanzar la VM todavía.

1. Crea un directorio de trabajo:
   ```bash
   mkdir -p ~/lab-cloud-init
   cd ~/lab-cloud-init
   ```

2. Escribe `meta-data`:
   ```bash
   cat > meta-data <<'EOF'
   instance-id: lab-test-001
   local-hostname: lab-test
   EOF
   ```

3. Escribe `user-data` con estos requisitos:
   - Primer línea: `#cloud-config`
   - Usuario `labuser` con contraseña `labpass123`, sudo sin contraseña
   - Contraseña de root `labpass123`
   - SSH con contraseña habilitado
   - Paquetes: `vim`, `curl`, `tmux`, `bash-completion`
   - Timezone de tu localidad
   - `runcmd` que cree `/root/cloud-init-done.txt` con la fecha

4. Valida el YAML:
   ```bash
   python3 -c "import yaml; yaml.safe_load(open('user-data')); print('YAML válido')"
   ```

5. Genera la ISO seed:
   ```bash
   cloud-localds seed.iso user-data meta-data
   ls -lh seed.iso
   ```

6. Verifica el label de la ISO:
   ```bash
   isoinfo -d -i seed.iso | grep "Volume id"
   # Debe mostrar: Volume id: cidata
   ```

**Pregunta de reflexión**: el archivo `user-data` contiene contraseñas en
texto plano. ¿Dónde queda almacenado ese archivo después de que cloud-init
lo procesa? (Pista: mira en `/var/lib/cloud/` dentro de la VM.)

---

### Ejercicio 2: Lanzar VM con cloud-init

**Objetivo**: crear una VM funcional con cloud-init en menos de un minuto.

> **Prerequisito**: necesitas una imagen cloud de Debian 12 o AlmaLinux 9
> descargada en `/var/lib/libvirt/images/`.

1. Crea un overlay desde la imagen cloud con tamaño expandido:
   ```bash
   sudo qemu-img create -f qcow2 \
       -b /var/lib/libvirt/images/debian-12-generic-amd64.qcow2 -F qcow2 \
       /var/lib/libvirt/images/lab-test.qcow2 20G
   ```

2. Copia la ISO seed al directorio de imágenes:
   ```bash
   sudo cp ~/lab-cloud-init/seed.iso /var/lib/libvirt/images/seed-lab-test.iso
   ```

3. Lanza la VM:
   ```bash
   sudo virt-install \
       --name lab-test \
       --ram 1024 \
       --vcpus 1 \
       --import \
       --disk path=/var/lib/libvirt/images/lab-test.qcow2 \
       --disk path=/var/lib/libvirt/images/seed-lab-test.iso,device=cdrom \
       --os-variant debian12 \
       --network network=default \
       --graphics none \
       --noautoconsole
   ```

4. Espera ~30 segundos y conecta a la consola:
   ```bash
   virsh console lab-test
   ```
   Login con `labuser` / `labpass123`.

5. Dentro de la VM, verifica:
   ```bash
   hostname                                    # ¿es lab-test?
   cat /root/cloud-init-done.txt               # ¿existe?
   cloud-init status                           # ¿status: done?
   lsblk                                       # ¿disco expandido a 20G?
   dpkg -l vim curl tmux 2>/dev/null | tail -3 # ¿paquetes instalados?
   timedatectl                                 # ¿timezone correcta?
   ```

6. Examina los logs de cloud-init:
   ```bash
   cat /var/log/cloud-init-output.log | tail -20
   ```

7. Sal con `Ctrl+]`.

**Pregunta de reflexión**: desde que ejecutaste `virt-install` hasta que
pudiste hacer login, ¿cuánto tiempo pasó? Compara con lo que tardaría
una instalación desde ISO.

---

### Ejercicio 3: Múltiples VMs desde una sola imagen base

**Objetivo**: crear 3 VMs diferentes a partir de la misma imagen cloud.

1. Crea tres pares de archivos seed, cada uno con diferente hostname e
   instance-id:

   ```bash
   cd ~/lab-cloud-init

   for name in vm-web vm-db vm-cache; do
       mkdir -p $name
       cat > $name/meta-data <<EOF
   instance-id: ${name}-001
   local-hostname: ${name}
   EOF
       cat > $name/user-data <<'EOF'
   #cloud-config
   users:
     - name: labuser
       plain_text_passwd: labpass123
       lock_passwd: false
       sudo: ALL=(ALL) NOPASSWD:ALL
   ssh_pwauth: true
   chpasswd:
     list: |
       root:labpass123
     expire: false
   EOF
       cloud-localds $name/seed.iso $name/user-data $name/meta-data
   done
   ```

2. Crea tres overlays desde la misma base:
   ```bash
   for name in vm-web vm-db vm-cache; do
       sudo qemu-img create -f qcow2 \
           -b /var/lib/libvirt/images/debian-12-generic-amd64.qcow2 -F qcow2 \
           /var/lib/libvirt/images/${name}.qcow2 20G
       sudo cp ~/lab-cloud-init/$name/seed.iso \
           /var/lib/libvirt/images/seed-${name}.iso
   done
   ```

3. Lanza las tres VMs:
   ```bash
   for name in vm-web vm-db vm-cache; do
       sudo virt-install \
           --name $name \
           --ram 512 \
           --vcpus 1 \
           --import \
           --disk path=/var/lib/libvirt/images/${name}.qcow2 \
           --disk path=/var/lib/libvirt/images/seed-${name}.iso,device=cdrom \
           --os-variant debian12 \
           --network network=default \
           --graphics none \
           --noautoconsole
   done
   ```

4. Verifica que las tres están corriendo:
   ```bash
   virsh list
   ```

5. Conéctate a cada una y verifica que el hostname es correcto:
   ```bash
   virsh console vm-web
   # hostname → vm-web
   # Ctrl+]

   virsh console vm-db
   # hostname → vm-db
   # Ctrl+]
   ```

6. Compara el espacio en disco:
   ```bash
   ls -lh /var/lib/libvirt/images/vm-*.qcow2
   ls -lh /var/lib/libvirt/images/debian-12-generic-amd64.qcow2
   ```
   Los overlays deberían ser significativamente menores que la base.

7. Limpia cuando termines:
   ```bash
   for name in vm-web vm-db vm-cache; do
       sudo virsh destroy $name
       sudo virsh undefine $name --remove-all-storage
   done
   sudo rm /var/lib/libvirt/images/seed-vm-*.iso
   ```

**Pregunta de reflexión**: lanzaste 3 VMs en segundos a partir de una
sola imagen base de ~350 MB. Si cada una fuera una instalación desde ISO
completa, ¿cuánto espacio y tiempo se habría consumido? ¿Por qué cloud-init
+ overlays es el patrón dominante en entornos cloud?
