# Instalación desatendida (kickstart / preseed)

## Índice

1. [El problema de la instalación manual](#el-problema-de-la-instalación-manual)
2. [Concepto de archivo de respuestas](#concepto-de-archivo-de-respuestas)
3. [Kickstart (RHEL / AlmaLinux / Fedora)](#kickstart-rhel--almalinux--fedora)
4. [Anatomía de un archivo kickstart](#anatomía-de-un-archivo-kickstart)
5. [Kickstart con virt-install](#kickstart-con-virt-install)
6. [Preseed (Debian / Ubuntu)](#preseed-debian--ubuntu)
7. [Anatomía de un archivo preseed](#anatomía-de-un-archivo-preseed)
8. [Preseed con virt-install](#preseed-con-virt-install)
9. [Servir archivos de respuestas](#servir-archivos-de-respuestas)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## El problema de la instalación manual

En T01 instalamos una VM paso a paso: elegir idioma, configurar red,
particionar disco, seleccionar paquetes, crear usuarios... Una VM tarda
20-30 minutos con intervención constante.

Ahora imagina que necesitas 5 VMs idénticas para un laboratorio. O que
rompiste la VM y necesitas recrearla. ¿Repetir todo manualmente cada vez?

```
Instalación manual:

  VM 1: 25 minutos + atención constante     ┐
  VM 2: 25 minutos + atención constante     │
  VM 3: 25 minutos + atención constante     ├── 2+ horas, propenso a errores
  VM 4: 25 minutos + atención constante     │
  VM 5: 25 minutos + atención constante     ┘

Instalación desatendida:

  Escribir archivo de respuestas: 15 minutos (una sola vez)
  VM 1-5: lanzar 5 virt-install en paralelo → ~15 min total, sin intervención
```

La instalación desatendida usa un **archivo de respuestas** que contiene
todas las decisiones que normalmente tomarías durante la instalación. El
instalador lee este archivo y procede automáticamente.

---

## Concepto de archivo de respuestas

El archivo de respuestas es un fichero de texto plano que pre-responde
todas las preguntas del instalador del SO:

```
┌───────────────────────────────────────────────────────────────────┐
│  Instalador del SO                                               │
│                                                                  │
│  Pregunta                    Respuesta manual    Archivo          │
│  ─────────────────────────── ─────────────────── ──────────────── │
│  ¿Idioma?                    "Español"           lang es_ES       │
│  ¿Zona horaria?              "Europe/Madrid"     timezone ...     │
│  ¿Cómo particionar?          "Disco entero"      autopart         │
│  ¿Contraseña root?           "mipass123"          rootpw ...      │
│  ¿Qué paquetes?              "Minimal + SSH"     %packages ...   │
│  ¿Configuración de red?      "DHCP"              network ...     │
│                                                                  │
│  Con archivo → el instalador no pregunta nada, procede solo      │
└───────────────────────────────────────────────────────────────────┘
```

Cada familia de distribuciones tiene su propio formato:

| Familia | Formato | Instalador | Extensión típica |
|---------|---------|------------|------------------|
| RHEL / AlmaLinux / Fedora / Rocky | Kickstart | Anaconda | `.ks` o `.cfg` |
| Debian / Ubuntu | Preseed | debian-installer (d-i) | `.cfg` o `preseed.cfg` |

Ambos logran el mismo resultado: instalación completamente automática.

---

## Kickstart (RHEL / AlmaLinux / Fedora)

Kickstart es el sistema de respuestas automáticas de la familia Red Hat.
Fue creado originalmente por Red Hat en los años 90 y ha evolucionado con
cada versión de RHEL.

### Cómo funciona

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  1. virt-install lanza la VM con la ISO de AlmaLinux        │
│     --extra-args "inst.ks=http://host/ks.cfg"               │
│                  │                                          │
│  2. El kernel del instalador (Anaconda) arranca             │
│                  │                                          │
│  3. Anaconda ve el parámetro inst.ks=...                    │
│                  │                                          │
│  4. Descarga el archivo kickstart por HTTP                  │
│                  │                                          │
│  5. Aplica todas las respuestas del archivo                 │
│                  │                                          │
│  6. Instala paquetes, configura, rearrancar                 │
│                  │                                          │
│  7. VM lista — sin intervención humana                      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Fuente del kickstart

El parámetro `inst.ks=` acepta varias fuentes:

```bash
# Desde un servidor HTTP (más común)
inst.ks=http://192.168.122.1/ks.cfg

# Desde NFS
inst.ks=nfs:server:/path/to/ks.cfg

# Desde el propio CD-ROM (archivo incluido en la ISO)
inst.ks=cdrom:/ks.cfg

# Desde un disco local
inst.ks=hd:vda1:/ks.cfg
```

---

## Anatomía de un archivo kickstart

Un archivo kickstart se divide en tres secciones:

```
┌─────────────────────────────────────────┐
│  SECCIÓN DE COMANDOS                    │  ← Configuración del sistema
│  (líneas sueltas al inicio)             │
├─────────────────────────────────────────┤
│  %packages                              │  ← Paquetes a instalar
│  ...                                    │
│  %end                                   │
├─────────────────────────────────────────┤
│  %pre / %post                           │  ← Scripts pre/post instalación
│  ...                                    │
│  %end                                   │
└─────────────────────────────────────────┘
```

### Ejemplo mínimo para AlmaLinux 9

```bash
# ks-alma9-minimal.cfg
# Kickstart mínimo para AlmaLinux 9 en VM de laboratorio

#── Configuración del sistema ──────────────────────────────────
# Modo de instalación (texto = sin GUI de Anaconda)
text

# Fuente de instalación (el CD-ROM montado por virt-install)
cdrom

# Idioma y teclado
lang en_US.UTF-8
keyboard us

# Zona horaria
timezone Europe/Madrid --utc

# Red: DHCP en la primera interfaz
network --bootproto=dhcp --device=link --activate --onboot=on
network --hostname=alma-lab

# Contraseña de root (en producción usar --iscrypted con hash)
rootpw --plaintext labpass123

# Crear usuario de laboratorio con sudo
user --name=labuser --password=labpass123 --plaintext --groups=wheel

# Particionado automático: LVM, borrar disco existente
ignoredisk --only-use=vda
clearpart --all --initlabel --drives=vda
autopart --type=lvm

# Bootloader
bootloader --append="console=ttyS0,115200n8" --location=mbr --boot-drive=vda

# SELinux
selinux --enforcing

# Firewall
firewall --enabled --ssh

# No configurar X Window
skipx

# Rearrancar al terminar
reboot

#── Paquetes ───────────────────────────────────────────────────
%packages
@^minimal-environment
vim-enhanced
curl
tmux
bash-completion
man-pages
%end

#── Post-instalación ──────────────────────────────────────────
%post --log=/root/ks-post.log
# Habilitar SSH
systemctl enable sshd

# Actualizar sistema
dnf update -y

# Mensaje informativo
echo "Instalación kickstart completada: $(date)" > /root/ks-done.txt
%end
```

### Desglose de cada directiva

#### Sección de comandos

| Directiva | Significado |
|-----------|-------------|
| `text` | Instalación en modo texto (sin GUI de Anaconda) |
| `cdrom` | Instalar desde el CD-ROM (la ISO montada) |
| `lang en_US.UTF-8` | Idioma del sistema |
| `keyboard us` | Layout del teclado |
| `timezone Europe/Madrid --utc` | Zona horaria, reloj hardware en UTC |
| `network --bootproto=dhcp` | Configurar red via DHCP |
| `network --hostname=alma-lab` | Nombre del host |
| `rootpw --plaintext labpass123` | Contraseña de root (texto plano, solo para lab) |
| `user --name=labuser ...` | Crear usuario adicional |
| `ignoredisk --only-use=vda` | Solo usar el disco virtio `vda` |
| `clearpart --all` | Borrar todas las particiones existentes |
| `autopart --type=lvm` | Particionado automático con LVM |
| `bootloader --append="console=ttyS0,115200n8"` | Configurar GRUB con consola serial |
| `selinux --enforcing` | SELinux activo |
| `firewall --enabled --ssh` | Firewall activo, permitir SSH |
| `skipx` | No instalar entorno gráfico |
| `reboot` | Rearrancar al terminar |

#### Sección %packages

```bash
%packages
@^minimal-environment     # grupo de paquetes: instalación mínima
vim-enhanced              # paquete individual
curl
tmux
-Plymouth                 # el guión - EXCLUYE un paquete
%end
```

El prefijo `@^` indica un **environment group** (el nivel más alto de
agrupación). `@` sin `^` indica un grupo regular. Sin prefijo es un paquete
individual. El prefijo `-` excluye.

```bash
# Ver los environment groups disponibles
dnf group list --available
```

#### Sección %post

```bash
%post --log=/root/ks-post.log
# Cualquier comando bash se ejecuta aquí DENTRO del sistema recién instalado
# El filesystem root del nuevo sistema está montado en /
systemctl enable sshd
echo "Instalación completada" > /root/info.txt
%end
```

`%post` se ejecuta **después** de instalar todos los paquetes, con el nuevo
sistema montado como `/`. Es el lugar para personalización final.

Opciones útiles de `%post`:

| Opción | Efecto |
|--------|--------|
| `--log=/root/ks-post.log` | Guardar la salida en un log |
| `--nochroot` | Ejecutar en el entorno del instalador (no dentro del nuevo FS) |
| `--interpreter=/usr/bin/python3` | Usar otro intérprete en vez de bash |

#### Contraseñas seguras con hash

En laboratorio usamos `--plaintext` por simplicidad, pero en entornos reales
**nunca** pongas contraseñas en texto plano:

```bash
# Generar hash para kickstart
python3 -c 'import crypt; print(crypt.crypt("labpass123", crypt.mksalt(crypt.METHOD_SHA512)))'
# $6$rAnDoMsAlT$HashLargoAquí...

# Usar en kickstart
rootpw --iscrypted $6$rAnDoMsAlT$HashLargoAquí...
```

### Extraer kickstart de una instalación existente

Si ya instalaste AlmaLinux manualmente, Anaconda guarda un kickstart con
las opciones que elegiste:

```bash
# Dentro de la VM instalada manualmente
cat /root/anaconda-ks.cfg
```

Este archivo es un punto de partida excelente: editarlo es más fácil que
escribir uno desde cero.

---

## Kickstart con virt-install

Para pasar el kickstart a virt-install necesitas `--location` (no `--cdrom`)
y `--extra-args`:

```bash
sudo virt-install \
    --name alma-ks \
    --ram 2048 \
    --vcpus 2 \
    --disk path=/var/lib/libvirt/images/alma-ks.qcow2,size=20,format=qcow2 \
    --location /srv/iso/AlmaLinux-9-latest-x86_64-minimal.iso \
    --os-variant almalinux9 \
    --network network=default \
    --graphics none \
    --extra-args "inst.ks=http://192.168.122.1:8080/ks-alma9-minimal.cfg console=ttyS0,115200n8"
```

Observa:

- **`--location`** en vez de `--cdrom`: necesario para que `--extra-args`
  funcione (QEMU extrae el kernel/initrd y les pasa los parámetros)
- **`--extra-args`** contiene dos cosas:
  - `inst.ks=http://...` — dónde encontrar el kickstart
  - `console=ttyS0,115200n8` — redirigir consola al serial

> **Predicción**: si usas `--cdrom` en vez de `--location`, el flag
> `--extra-args` será **ignorado silenciosamente**. Anaconda nunca verá
> `inst.ks=...` y te pedirá instalación interactiva.

### Kickstart desde archivo local (sin servidor HTTP)

Si no quieres levantar un servidor HTTP, puedes inyectar el kickstart
directamente usando `--initrd-inject`:

```bash
sudo virt-install \
    --name alma-ks \
    --ram 2048 \
    --vcpus 2 \
    --disk path=/var/lib/libvirt/images/alma-ks.qcow2,size=20,format=qcow2 \
    --location /srv/iso/AlmaLinux-9-latest-x86_64-minimal.iso \
    --os-variant almalinux9 \
    --network network=default \
    --graphics none \
    --initrd-inject /path/to/ks-alma9-minimal.cfg \
    --extra-args "inst.ks=file:/ks-alma9-minimal.cfg console=ttyS0,115200n8"
```

`--initrd-inject` copia el archivo dentro del initrd, y `inst.ks=file:/nombre`
le dice a Anaconda que lo busque allí. No necesitas servidor web.

---

## Preseed (Debian / Ubuntu)

Preseed es el equivalente de Debian al kickstart de RHEL. Usa el framework
de preguntas de `debian-installer` (d-i) para pre-responder cada paso.

### Cómo funciona

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  1. virt-install lanza la VM con la ISO de Debian           │
│     --extra-args "preseed/url=http://host/preseed.cfg"      │
│                  │                                          │
│  2. El kernel del instalador (d-i) arranca                  │
│                  │                                          │
│  3. d-i ve el parámetro preseed/url=...                     │
│                  │                                          │
│  4. Descarga el archivo preseed por HTTP                    │
│                  │                                          │
│  5. Pre-responde todas las preguntas del instalador         │
│                  │                                          │
│  6. Instala paquetes, configura GRUB, rearrancar            │
│                  │                                          │
│  7. VM lista — sin intervención humana                      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

La diferencia principal con kickstart: preseed usa el formato
`componente pregunta tipo respuesta`, que refleja directamente las
preguntas internas de d-i.

---

## Anatomía de un archivo preseed

### Ejemplo mínimo para Debian 12

```bash
# preseed-debian12-minimal.cfg
# Preseed mínimo para Debian 12 en VM de laboratorio

#── Localización ───────────────────────────────────────────────
d-i debian-installer/locale string en_US.UTF-8
d-i keyboard-configuration/xkb-keymap select us

#── Red ────────────────────────────────────────────────────────
d-i netcfg/choose_interface select auto
d-i netcfg/get_hostname string debian-lab
d-i netcfg/get_domain string local

#── Mirror (repositorio de paquetes) ──────────────────────────
d-i mirror/country string manual
d-i mirror/http/hostname string deb.debian.org
d-i mirror/http/directory string /debian
d-i mirror/http/proxy string

#── Zona horaria ──────────────────────────────────────────────
d-i clock-setup/utc boolean true
d-i time/zone string Europe/Madrid
d-i clock-setup/ntp boolean true

#── Particionado ──────────────────────────────────────────────
d-i partman-auto/method string regular
d-i partman-auto/choose_recipe select atomic
d-i partman-partitioning/confirm_write_new_label boolean true
d-i partman/choose_partition select finish
d-i partman/confirm boolean true
d-i partman/confirm_nooverwrite boolean true

#── Usuarios ──────────────────────────────────────────────────
# Contraseña de root
d-i passwd/root-password password labpass123
d-i passwd/root-password-again password labpass123

# Cuenta de usuario regular
d-i passwd/user-fullname string Lab User
d-i passwd/username string labuser
d-i passwd/user-password password labpass123
d-i passwd/user-password-again password labpass123

#── Selección de paquetes ─────────────────────────────────────
tasksel tasksel/first multiselect standard, ssh-server
d-i pkgsel/include string vim curl tmux bash-completion man-db
d-i pkgsel/upgrade select full-upgrade
d-i popularity-contest/participate boolean false

#── GRUB ──────────────────────────────────────────────────────
d-i grub-installer/only_debian boolean true
d-i grub-installer/bootdev string /dev/vda

#── Finalización ──────────────────────────────────────────────
d-i finish-install/reboot_in_progress note

#── Comando post-instalación ──────────────────────────────────
d-i preseed/late_command string \
    in-target sh -c 'echo "Preseed completado: $(date)" > /root/preseed-done.txt'; \
    in-target systemctl enable ssh
```

### Formato de cada línea

Cada línea preseed sigue la estructura:

```
d-i   componente/pregunta   tipo   respuesta
│     │                     │      │
│     │                     │      └── el valor de la respuesta
│     │                     └── tipo de dato: string, boolean, select, multiselect, password, note
│     └── la pregunta interna de d-i (componente/variable)
│
└── prefijo: "d-i" para preguntas del instalador, nombre del paquete para otros
```

Ejemplos:

```bash
# d-i = debian-installer (la mayoría de líneas)
d-i time/zone string Europe/Madrid

# tasksel = selector de tareas (grupos de paquetes)
tasksel tasksel/first multiselect standard, ssh-server

# Un paquete específico (popularity-contest)
d-i popularity-contest/participate boolean false
```

### Desglose por sección

#### Particionado

El particionado preseed es la sección más compleja. La receta `atomic`
crea una sola partición con todo:

```bash
# Método: regular (sin LVM), lvm, crypto (LUKS)
d-i partman-auto/method string regular

# Receta: atomic (todo en una), home (separar /home), multi (separar /home, /usr, /var, /tmp)
d-i partman-auto/choose_recipe select atomic

# Confirmar sin preguntar (4 líneas necesarias para evitar todas las confirmaciones)
d-i partman-partitioning/confirm_write_new_label boolean true
d-i partman/choose_partition select finish
d-i partman/confirm boolean true
d-i partman/confirm_nooverwrite boolean true
```

> **Predicción**: si omites alguna de las 4 líneas de confirmación del
> particionado, d-i se detendrá a preguntar y la instalación dejará de ser
> desatendida. Las 4 son necesarias.

#### late_command: post-instalación

El equivalente de `%post` en kickstart es `preseed/late_command`:

```bash
# Ejecutar comandos dentro del sistema instalado (in-target)
d-i preseed/late_command string \
    in-target sh -c 'apt install -y htop'; \
    in-target systemctl enable ssh; \
    in-target sh -c 'echo "PermitRootLogin yes" >> /etc/ssh/sshd_config'
```

`in-target` ejecuta el comando dentro del chroot del sistema instalado.
Sin `in-target`, el comando se ejecuta en el entorno del instalador.

#### Contraseñas con hash en preseed

```bash
# Generar hash
mkpasswd -m sha-512 labpass123
# $6$rAnDoMsAlT$HashLargoAquí...

# Usar en preseed
d-i passwd/root-password-crypted password $6$rAnDoMsAlT$HashLargoAquí...
d-i passwd/user-password-crypted password $6$rAnDoMsAlT$HashLargoAquí...
```

### Descubrir nombres de preguntas

¿Cómo saber qué preguntas existen y cómo se llaman? Hay dos métodos:

```bash
# 1. Instalar Debian manualmente y revisar el log
# Dentro de la VM instalada:
cat /var/log/installer/cdebconf/questions.dat

# 2. Consultar el ejemplo oficial
# Debian proporciona un preseed completo documentado:
# https://www.debian.org/releases/stable/example-preseed.txt
```

---

## Preseed con virt-install

Similar a kickstart, necesitas `--location` y `--extra-args`:

```bash
sudo virt-install \
    --name debian-preseed \
    --ram 2048 \
    --vcpus 2 \
    --disk path=/var/lib/libvirt/images/debian-preseed.qcow2,size=20,format=qcow2 \
    --location /srv/iso/debian-12.9.0-amd64-netinst.iso \
    --os-variant debian12 \
    --network network=default \
    --graphics none \
    --extra-args "auto=true preseed/url=http://192.168.122.1:8080/preseed-debian12-minimal.cfg console=ttyS0,115200n8"
```

Los parámetros clave en `--extra-args`:

| Parámetro | Función |
|-----------|---------|
| `auto=true` | Activar modo automático (no preguntar lo que ya está respondido) |
| `preseed/url=http://...` | URL del archivo preseed |
| `console=ttyS0,115200n8` | Consola serial |

### Preseed desde archivo local

```bash
sudo virt-install \
    --name debian-preseed \
    --ram 2048 \
    --vcpus 2 \
    --disk path=/var/lib/libvirt/images/debian-preseed.qcow2,size=20,format=qcow2 \
    --location /srv/iso/debian-12.9.0-amd64-netinst.iso \
    --os-variant debian12 \
    --network network=default \
    --graphics none \
    --initrd-inject /path/to/preseed-debian12-minimal.cfg \
    --extra-args "auto=true preseed/file=/preseed-debian12-minimal.cfg console=ttyS0,115200n8"
```

Misma técnica que con kickstart: `--initrd-inject` mete el archivo en el
initrd, `preseed/file=/nombre` lo referencia desde allí.

### Problema de timing con preseed

Hay un detalle sutil: las preguntas de **idioma, país y teclado** se
formulan **antes** de que d-i tenga red disponible. Si el preseed se
descarga por HTTP, esas preguntas no se pueden pre-responder desde el
archivo.

Solución: pasar esas respuestas como parámetros del kernel:

```bash
--extra-args "auto=true \
    preseed/url=http://192.168.122.1:8080/preseed.cfg \
    locale=en_US.UTF-8 \
    keyboard-configuration/xkb-keymap=us \
    console=ttyS0,115200n8"
```

Con `--initrd-inject` este problema no existe porque el archivo ya está
disponible desde el arranque.

---

## Servir archivos de respuestas

Los archivos kickstart y preseed necesitan ser accesibles para el
instalador. El método más simple es un servidor HTTP temporal.

### Servidor HTTP con Python (rápido y simple)

```bash
# En el directorio donde están los archivos
cd /path/to/kickstart-files/

# Levantar servidor HTTP temporal en puerto 8080
python3 -m http.server 8080 --bind 192.168.122.1
```

```
Serving HTTP on 192.168.122.1 port 8080 ...
```

El bind a `192.168.122.1` es la IP del host en la red NAT de libvirt
(`virbr0`). Las VMs con red `default` pueden alcanzar esta IP.

```bash
# Verificar la IP del bridge
ip addr show virbr0
# inet 192.168.122.1/24 ...
```

### Flujo completo

```
Terminal 1: servir los archivos
  $ cd ~/kickstart-files/
  $ python3 -m http.server 8080 --bind 192.168.122.1

Terminal 2: lanzar la instalación
  $ sudo virt-install ... \
      --extra-args "inst.ks=http://192.168.122.1:8080/ks.cfg ..."

                 ┌────────────────────┐
                 │  Host (virbr0)     │
                 │  192.168.122.1     │
  Terminal 1 ──► │  :8080 ── ks.cfg   │ ◄── VM descarga el archivo
                 │           preseed  │
                 └────────────────────┘
```

### Alternativa: directorio local con --initrd-inject

Si solo necesitas instalar VMs localmente, `--initrd-inject` evita la
necesidad de un servidor HTTP. Es más simple para uso individual:

```
Con HTTP:           Con --initrd-inject:
  1. Crear archivo    1. Crear archivo
  2. Levantar HTTP    2. Lanzar virt-install con --initrd-inject
  3. Lanzar VM           (un solo paso)
  4. Apagar HTTP
```

Usa HTTP cuando necesites instalar **múltiples VMs en paralelo** que
compartan el mismo archivo, o cuando la VM esté en un host remoto.

---

## Errores comunes

### 1. Usar --cdrom en vez de --location con --extra-args

```bash
# ❌ --extra-args se ignora silenciosamente con --cdrom
sudo virt-install \
    --name test \
    --ram 2048 --vcpus 2 --disk size=20 \
    --cdrom /srv/iso/AlmaLinux-9-latest-x86_64-minimal.iso \
    --extra-args "inst.ks=http://host/ks.cfg console=ttyS0"
    # Anaconda nunca ve inst.ks → instalación interactiva

# ✅ Usar --location
sudo virt-install \
    --name test \
    --ram 2048 --vcpus 2 --disk size=20 \
    --location /srv/iso/AlmaLinux-9-latest-x86_64-minimal.iso \
    --extra-args "inst.ks=http://host/ks.cfg console=ttyS0,115200n8"
```

### 2. Servidor HTTP no accesible desde la VM

```bash
# ❌ Servidor HTTP escuchando en 127.0.0.1 (solo localhost)
python3 -m http.server 8080
# La VM no puede alcanzar 127.0.0.1 del host

# ❌ Servidor HTTP en la IP de la interfaz física
python3 -m http.server 8080 --bind 192.168.1.100
# La VM en la red NAT (192.168.122.x) puede no tener ruta

# ✅ Servidor HTTP en la IP del bridge virbr0
python3 -m http.server 8080 --bind 192.168.122.1
# La VM en la red default puede alcanzar 192.168.122.1
```

### 3. Preseed: omitir líneas de confirmación del particionado

```bash
# ❌ Solo una línea de confirmación — d-i preguntará de todos modos
d-i partman/confirm boolean true

# ✅ Las 4 líneas necesarias
d-i partman-partitioning/confirm_write_new_label boolean true
d-i partman/choose_partition select finish
d-i partman/confirm boolean true
d-i partman/confirm_nooverwrite boolean true
```

### 4. Kickstart: disco incorrecto en clearpart

```bash
# ❌ El disco virtio es vda, no sda
clearpart --all --initlabel --drives=sda
# Anaconda no encuentra sda, la instalación falla

# ✅ Usar el nombre correcto (virtio = vda)
ignoredisk --only-use=vda
clearpart --all --initlabel --drives=vda
```

Si `--os-variant` aplica virtio (lo normal), el disco se llama `vda`.
Solo si usas `bus=sata` o `bus=ide` será `sda`.

### 5. Contraseñas en texto plano en producción

```bash
# ❌ NUNCA en producción o entornos compartidos
rootpw --plaintext MiContraseña123

# ✅ Para lab está bien, pero documéntalo
rootpw --plaintext labpass123
# TODO: usar --iscrypted con hash SHA-512 en entornos reales
```

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  INSTALACIÓN DESATENDIDA — REFERENCIA RÁPIDA                       ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  KICKSTART (AlmaLinux / RHEL / Fedora):                            ║
║                                                                    ║
║    Estructura:                                                     ║
║      comandos (text, cdrom, lang, rootpw, autopart, ...)           ║
║      %packages ... %end                                            ║
║      %post ... %end                                                ║
║                                                                    ║
║    Vía HTTP:                                                       ║
║      virt-install --location ISO \                                 ║
║        --extra-args "inst.ks=http://HOST:PORT/file.cfg             ║
║                      console=ttyS0,115200n8"                       ║
║                                                                    ║
║    Vía local:                                                      ║
║      virt-install --location ISO \                                 ║
║        --initrd-inject /path/to/file.cfg \                         ║
║        --extra-args "inst.ks=file:/file.cfg                        ║
║                      console=ttyS0,115200n8"                       ║
║                                                                    ║
║    KS base de instalación existente: /root/anaconda-ks.cfg         ║
║                                                                    ║
║  ──────────────────────────────────────────────────────────────     ║
║                                                                    ║
║  PRESEED (Debian / Ubuntu):                                        ║
║                                                                    ║
║    Formato por línea:                                               ║
║      d-i componente/pregunta tipo respuesta                        ║
║                                                                    ║
║    Vía HTTP:                                                       ║
║      virt-install --location ISO \                                 ║
║        --extra-args "auto=true                                     ║
║          preseed/url=http://HOST:PORT/preseed.cfg                  ║
║          locale=en_US.UTF-8                                        ║
║          console=ttyS0,115200n8"                                   ║
║                                                                    ║
║    Vía local:                                                      ║
║      virt-install --location ISO \                                 ║
║        --initrd-inject /path/to/preseed.cfg \                      ║
║        --extra-args "auto=true                                     ║
║          preseed/file=/preseed.cfg                                 ║
║          console=ttyS0,115200n8"                                   ║
║                                                                    ║
║    Post-install: d-i preseed/late_command string in-target CMD      ║
║                                                                    ║
║  ──────────────────────────────────────────────────────────────     ║
║                                                                    ║
║  SERVIR ARCHIVOS:                                                  ║
║    python3 -m http.server 8080 --bind 192.168.122.1                ║
║    (IP del bridge virbr0 de la red NAT default)                    ║
║                                                                    ║
║  REGLAS:                                                           ║
║    • Siempre --location, nunca --cdrom (para --extra-args)         ║
║    • Disco virtio = vda (no sda)                                   ║
║    • Contraseñas en hash para producción, plaintext solo para lab  ║
║    • Preseed: 4 líneas de confirmación de particionado             ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Escribir y analizar un kickstart

**Objetivo**: crear un archivo kickstart personalizado sin ejecutarlo aún.

1. Crea un directorio para los archivos de respuestas:
   ```bash
   mkdir -p ~/lab-kickstart
   cd ~/lab-kickstart
   ```

2. Escribe un archivo `ks-alma9-lab.cfg` con estos requisitos:
   - Instalación en modo texto desde CD-ROM
   - Idioma `en_US.UTF-8`, teclado `us`
   - Zona horaria de tu localidad
   - Red DHCP, hostname `alma-lab`
   - Root con contraseña `labpass123` (plaintext, es lab)
   - Usuario `labuser` con contraseña `labpass123`, en grupo `wheel`
   - Particionado automático LVM en disco `vda`
   - Bootloader con consola serial
   - SELinux enforcing, firewall con SSH
   - Paquetes: `@^minimal-environment`, `vim-enhanced`, `curl`, `tmux`,
     `bash-completion`
   - Post-instalación: habilitar sshd, crear archivo `/root/ks-done.txt`
   - Rearrancar al terminar

3. Revisa tu archivo y verifica:
   - ¿Están las tres secciones (comandos, `%packages`, `%post`)?
   - ¿El disco es `vda` (no `sda`)?
   - ¿Hay `console=ttyS0,115200n8` en el bootloader?

4. Construye el comando `virt-install` que usarías con `--initrd-inject`
   (no lo ejecutes todavía, solo escríbelo en un archivo `install.sh`).

**Pregunta de reflexión**: el archivo kickstart contiene la contraseña de
root en texto plano. ¿Quién podría verla? Si el archivo está en un servidor
HTTP sin autenticación, ¿cualquier máquina en la red podría descargarlo?

---

### Ejercicio 2: Escribir y analizar un preseed

**Objetivo**: crear un archivo preseed para Debian 12.

1. En el mismo directorio:
   ```bash
   cd ~/lab-kickstart
   ```

2. Escribe un archivo `preseed-debian12-lab.cfg` con estos requisitos:
   - Locale `en_US.UTF-8`, teclado `us`
   - Red DHCP automática, hostname `debian-lab`
   - Mirror: `deb.debian.org`, directorio `/debian`, sin proxy
   - Zona horaria de tu localidad, reloj en UTC, NTP activado
   - Particionado regular, receta `atomic` (todo en una partición)
   - Las 4 líneas de confirmación del particionado
   - Root con contraseña `labpass123`
   - Usuario `labuser` con contraseña `labpass123`
   - Tareas: `standard, ssh-server`
   - Paquetes adicionales: `vim`, `curl`, `tmux`, `bash-completion`
   - Full-upgrade, no participar en popularity-contest
   - GRUB en `/dev/vda`
   - late_command: crear `/root/preseed-done.txt` con la fecha

3. Compara tu preseed con el kickstart del ejercicio 1. Para cada línea del
   kickstart, identifica su equivalente en preseed:

   | Kickstart | Preseed |
   |-----------|---------|
   | `lang en_US.UTF-8` | `d-i debian-installer/locale string en_US.UTF-8` |
   | `rootpw --plaintext labpass123` | ¿? |
   | `autopart --type=lvm` | ¿? |
   | `%post ... %end` | ¿? |

4. Construye el comando `virt-install` con `--initrd-inject` para este preseed.
   ¿Qué parámetros extra necesitas en `--extra-args` que no necesitabas con
   kickstart?

**Pregunta de reflexión**: preseed es más verboso que kickstart (más líneas
para lograr lo mismo). ¿Por qué crees que Debian eligió un formato basado
en preguntas de d-i en vez de un formato imperativo como kickstart?

---

### Ejercicio 3: Servir y probar una instalación desatendida

**Objetivo**: ejecutar una instalación desatendida completa (requiere ISO).

> **Prerequisito**: necesitas una ISO de AlmaLinux 9 o Debian 12 descargada.

1. Elige uno de tus archivos (kickstart o preseed) y revísalo una última vez.

2. Levanta el servidor HTTP:
   ```bash
   cd ~/lab-kickstart
   python3 -m http.server 8080 --bind 192.168.122.1 &
   HTTP_PID=$!
   ```

3. Verifica que el archivo es accesible:
   ```bash
   curl http://192.168.122.1:8080/ks-alma9-lab.cfg | head -5
   # o
   curl http://192.168.122.1:8080/preseed-debian12-lab.cfg | head -5
   ```

4. Lanza la instalación desatendida:

   **Para AlmaLinux:**
   ```bash
   sudo virt-install \
       --name alma-auto \
       --ram 2048 --vcpus 2 \
       --disk path=/var/lib/libvirt/images/alma-auto.qcow2,size=15,format=qcow2 \
       --location /srv/iso/AlmaLinux-9-latest-x86_64-minimal.iso \
       --os-variant almalinux9 \
       --network network=default \
       --graphics none \
       --extra-args "inst.ks=http://192.168.122.1:8080/ks-alma9-lab.cfg console=ttyS0,115200n8"
   ```

   **Para Debian:**
   ```bash
   sudo virt-install \
       --name debian-auto \
       --ram 2048 --vcpus 2 \
       --disk path=/var/lib/libvirt/images/debian-auto.qcow2,size=15,format=qcow2 \
       --location /srv/iso/debian-12.9.0-amd64-netinst.iso \
       --os-variant debian12 \
       --network network=default \
       --graphics none \
       --extra-args "auto=true preseed/url=http://192.168.122.1:8080/preseed-debian12-lab.cfg locale=en_US.UTF-8 keyboard-configuration/xkb-keymap=us console=ttyS0,115200n8"
   ```

5. Observa: la instalación debería progresar **sin pedirte nada**. Si se
   detiene en alguna pregunta, anota cuál es — falta esa respuesta en tu
   archivo.

6. Al terminar, verifica dentro de la VM:
   ```bash
   virsh console alma-auto   # o debian-auto
   # Login como labuser
   cat /root/ks-done.txt     # o /root/preseed-done.txt
   hostname
   ```

7. Limpia:
   ```bash
   kill $HTTP_PID
   # Opcionalmente destruir la VM:
   # sudo virsh destroy alma-auto
   # sudo virsh undefine alma-auto --remove-all-storage
   ```

**Pregunta de reflexión**: la instalación desatendida tardó aproximadamente
lo mismo que la manual en tiempo de reloj. La ventaja no es la velocidad
sino la **reproducibilidad** y la posibilidad de lanzar varias en paralelo.
Si necesitaras 10 VMs idénticas, ¿cuántos comandos `virt-install` podrías
ejecutar simultáneamente en un host con 32 GB de RAM y 8 cores?
