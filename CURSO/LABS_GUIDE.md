# Guía de Entornos de Laboratorio

Esta guía define qué plataforma usar para los laboratorios de cada bloque
del curso y cómo evoluciona el entorno a medida que avanzas.

---

## Resumen: Docker vs QEMU/KVM

| Aspecto | Docker | QEMU/KVM |
|---|---|---|
| Kernel | Compartido con el host | Propio (kernel completo del guest) |
| Init system | No tiene (PID 1 = tu proceso) | systemd completo |
| Boot | No existe — el contenedor arranca un proceso | BIOS/UEFI → GRUB → kernel → initramfs → systemd |
| Discos | Filesystem del host (overlayfs) | Discos virtuales propios (particiones, LVM, RAID) |
| Red | Namespaces de red del kernel host | Stack de red completo (iptables propios, routing) |
| SELinux | No soportado dentro del contenedor | Funciona completo (enforcing) |
| Módulos kernel | No se pueden cargar | Kernel propio, módulos propios |
| Peso | ~MB, arranca en segundos | ~GB, arranca en ~30s (con cloud-init) |
| Ideal para | Compilar, ejecutar programas, aprender CLI | Administrar un sistema operativo completo |

---

## Plataforma por bloque

### B01 — Docker y Entorno de Laboratorio
**Plataforma: Docker**
- Es el propio tema de estudio.
- Se construye el entorno de contenedores para B02–B07.

### B02 — Fundamentos de Linux
**Plataforma: Docker**
- Los contenedores Debian y Alma son suficientes para: FHS, usuarios, permisos,
  procesos, paquetes, shell scripting, systemd (parcial), cron, logging, monitoreo.
- Limitaciones conocidas: systemd dentro de Docker requiere `--privileged` y no
  cubre targets de arranque ni el proceso de boot.

### B03 — Fundamentos de C
**Plataforma: Docker**
- Compilar con gcc, depurar con gdb/valgrind, todo funciona en contenedores.
- POSIX I/O (C09) funciona correctamente en Docker.

### B04 — Sistemas de Compilación
**Plataforma: Docker**
- Make, CMake, pkg-config — todo es compilación, no requiere VMs.

### B05 — Fundamentos de Rust
**Plataforma: Docker**
- rustup, cargo, compilación, tests — todo funciona en contenedores.

### B06 — Programación de Sistemas en C
**Plataforma: Docker**
- Syscalls, fork/exec, pipes, signals, sockets — Docker soporta todo esto.
- Nota: algunos labs de sockets podrían beneficiarse de VMs (red real), pero
  Docker con redes custom es suficiente.

### B07 — Programación de Sistemas en Rust
**Plataforma: Docker**
- Misma lógica que B06.

---

### B08 — Almacenamiento y Sistemas de Archivos
**Plataforma: QEMU/KVM** (prerrequisito: C00 del propio bloque)

Docker no puede:
- Crear particiones (no hay dispositivos de bloque)
- Crear filesystems (mkfs requiere un device, no un directorio)
- Configurar LVM (sin acceso al device-mapper del host)
- Crear RAID con mdadm (sin block devices)
- Montar swap real
- Configurar cuotas de disco

**Entorno sugerido**:
- 1 VM Debian 12 + 1 VM AlmaLinux 9
- 4 discos virtuales extra por VM (2–5 GB cada uno) para:
  - Labs de particionado con fdisk/gdisk/parted
  - Labs de LVM: crear PVs, VGs, LVs, extender en caliente
  - Labs de RAID: mdadm con 3 discos + 1 spare
  - Labs de filesystems: mkfs.ext4, mkfs.xfs, mkfs.btrfs
  - Labs de swap: mkswap en partición o archivo
- Red NAT (para instalar paquetes)
- **Tip**: Crear snapshot de la VM antes de cada lab destructivo (particionar,
  RAID, LUKS). Revertir y repetir sin reinstalar.

**Workflow por lab**:
```bash
# Antes del lab
virsh snapshot-create-as debian-lab --name "pre-raid"

# Hacer el lab (particionar, crear RAID, etc.)
...

# Si necesitas repetir
virsh snapshot-revert debian-lab --snapshotname "pre-raid"

# Si todo salió bien, eliminar snapshot
virsh snapshot-delete debian-lab --snapshotname "pre-raid"
```

---

### B09 — Redes
**Plataforma: QEMU/KVM**

Docker no puede:
- Ejecutar iptables/nftables propios (comparte netfilter del host)
- Hacer routing real entre subnets
- Ejecutar firewalld
- Configurar VLANs, bonding, bridges reales
- Simular topologías de red complejas

**Entorno sugerido**:
- 3 VMs: 1 router + 2 hosts
- 2 redes aisladas (sin acceso a internet):
  - Red A: 10.0.1.0/24 — "oficina" (host1 + router)
  - Red B: 10.0.2.0/24 — "servidores" (host2 + router)
- 1 red NAT en el router (para salir a internet e instalar paquetes)
- La VM router tiene 3 interfaces (una por red)

```
                    Internet
                       |
                    [NAT red]
                       |
                   [Router VM]
                   /         \
              [Red A]       [Red B]
            10.0.1.0/24   10.0.2.0/24
                |              |
           [Host1 VM]    [Host2 VM]
```

**Labs que habilita**:
- Configurar routing estático en la VM router
- Firewall con iptables/nftables: bloquear tráfico entre redes
- firewalld con zonas: Red A = trusted, Red B = public
- NAT/MASQUERADE: los hosts salen a internet via el router
- tcpdump entre las VMs para ver paquetes reales
- Bonding/teaming en una VM con múltiples interfaces
- DNS y DHCP propios (preparación para B10)

**Nota**: El capítulo 7 de B09 (Redes en Docker) sí usa Docker — es el único
capítulo de este bloque donde Docker es la plataforma.

---

### B10 — Servicios de Red
**Plataforma: QEMU/KVM**

Docker no puede:
- Ejecutar systemd para gestionar servicios (Apache, Nginx, BIND, Postfix)
- Ejecutar DHCP server real (necesita acceso raw a la red)
- Simular clientes y servidores con resolución DNS propia
- Configurar PAM/LDAP/SSSD de forma realista

**Entorno sugerido**:
- 2–3 VMs: servidor Debian + servidor Alma + cliente
- 1 red aislada (el servidor DNS/DHCP controla la red del lab)
- 1 red NAT (para instalar paquetes inicialmente)

**Esquema por capítulo**:

| Capítulo | VMs necesarias | Red |
|---|---|---|
| C01 SSH | 2 (servidor + cliente) | NAT o aislada |
| C02 DNS BIND | 2 (servidor DNS + cliente que resuelve) | Aislada |
| C03 Web | 2 (servidor web + cliente con curl) | Aislada + NAT |
| C04 NFS/Samba | 2 (servidor + cliente que monta) | Aislada |
| C05 DHCP/Email/PAM/LDAP | 2–3 (servidor + clientes) | Aislada |

**Labs que habilita**:
- Servidor DNS autoritativo con zonas forward y reverse
- Servidor DHCP que asigna IPs a los otros VMs del lab
- Virtual hosts en Apache/Nginx con TLS self-signed
- NFS export y mount entre VMs
- Samba share accesible desde otra VM
- SSH tunneling entre VMs
- LDAP client con sssd para autenticación centralizada

**Tip**: Crear una VM de "infraestructura" que corra DNS + DHCP para toda
la red del lab. Los demás servidores la usan como resolvedor.

---

### B11 — Seguridad, Kernel y Arranque
**Plataforma: QEMU/KVM**

Docker no puede:
- Ejecutar SELinux en modo enforcing
- Cargar/descargar módulos del kernel
- Modificar GRUB ni el proceso de arranque
- Cifrar discos con LUKS (no hay block devices)
- Compilar un kernel y arrancar con él
- Ejecutar dracut/update-initramfs
- Modificar parámetros de arranque

**Entorno sugerido**:
- 1 VM AlmaLinux 9 (SELinux, kernel RHEL, dracut)
- 1 VM Debian 12 (AppArmor, update-initramfs)
- 1 disco extra para labs de LUKS/dm-crypt
- Snapshots **obligatorios** antes de cada lab de boot/GRUB

**Labs que habilita**:
- SELinux: cambiar contextos, troubleshooting con audit2why, crear políticas
- AppArmor: crear perfiles, enforce/complain
- Modificar GRUB: cambiar parámetros del kernel, password de GRUB
- Recovery: arrancar en rescue.target, init=/bin/bash
- LUKS: cifrar disco, desbloqueo al arranque con passphrase
- Módulos del kernel: cargar, blacklist, compilar módulo simple
- Compilar kernel custom y arrancar con él
- sysctl: tuning de red y memoria

**ADVERTENCIA**: Los labs de GRUB y boot pueden dejar la VM sin arrancar.
Siempre crear snapshot antes:
```bash
virsh snapshot-create-as alma-lab --name "pre-grub-lab"
# ... romper GRUB ...
# Si no puedes arreglar:
virsh snapshot-revert alma-lab --snapshotname "pre-grub-lab"
```

---

## Transición Docker → QEMU/KVM

Al llegar a B08, el cambio de plataforma es:

1. **Estudiar C00 de B08**: aprende QEMU/KVM y libvirt (el capítulo 0)
2. **Crear VMs plantilla**: 1 Debian + 1 Alma, configuradas con usuario de lab
3. **Para cada lab**: clonar la plantilla, agregar discos/redes según necesidad
4. **Después del lab**: destruir las VMs de lab (las plantillas permanecen)

El entorno Docker del curso (B01/C05) **no se elimina** — sigue siendo útil
para B03–B07 y para los capítulos de programación.

---

## Recursos de hardware sugeridos

Para correr 2–3 VMs simultáneamente:
- **RAM**: 8 GB mínimo en el host (2 GB por VM + host)
- **CPU**: 4 cores mínimo (2 vCPUs por VM)
- **Disco**: 50 GB libres (imágenes qcow2 con thin provisioning)
- **Virtualización**: VT-x/AMD-V habilitado en BIOS

Si tu máquina tiene 16 GB+ de RAM y un SSD, podrás correr el entorno
completo de B09 (3 VMs) sin problemas.
