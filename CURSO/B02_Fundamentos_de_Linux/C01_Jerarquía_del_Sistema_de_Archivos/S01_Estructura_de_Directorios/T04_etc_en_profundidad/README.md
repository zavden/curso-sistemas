# T04 — /etc en profundidad

## Propósito de /etc

`/etc` (históricamente "et cetera", hoy entendido como "Editable Text
Configuration") contiene todos los archivos de configuración del sistema.
Es el directorio que define el **comportamiento específico de esta máquina**.

Dos servidores con el mismo software instalado pero diferente `/etc` se
comportan de forma completamente distinta.

```
/usr → software instalado (igual entre máquinas con los mismos paquetes)
/etc → configuración (única por máquina)
```

## Convenciones de archivos

### Archivos de texto plano

La mayoría de archivos en `/etc` son texto plano editable:

```bash
file /etc/hostname
# /etc/hostname: ASCII text

file /etc/fstab
# /etc/fstab: ASCII text

file /etc/shadow
# /etc/shadow: ASCII text (permisos restrictivos)
```

Esto es una decisión de diseño de Unix: la configuración es texto legible y
editable con cualquier editor. No hay registros binarios ni bases de datos
opacas (con excepciones como `/etc/shadow` que tiene formato estricto pero
sigue siendo texto).

### Directorios `.d/`

Patrón moderno para configuración modular: en vez de un solo archivo
monolítico, se usa un directorio `.d/` con fragmentos:

```
/etc/apt/sources.list.d/     ← Fragmentos de sources.list
/etc/sudoers.d/              ← Fragmentos de sudoers
/etc/sysctl.d/               ← Fragmentos de sysctl.conf
/etc/cron.d/                 ← Fragmentos de crontab del sistema
/etc/logrotate.d/            ← Configuración de rotación por servicio
/etc/profile.d/              ← Scripts ejecutados al iniciar sesión
```

Los archivos dentro de `.d/` se procesan en orden alfabético (por eso muchos
se nombran con prefijo numérico: `01-custom.conf`, `99-override.conf`):

```bash
ls /etc/sysctl.d/
# 10-console-messages.conf
# 30-postgresql.conf
# 99-sysctl.conf
```

**Ventaja**: paquetes e instaladores pueden agregar/quitar su propia
configuración sin modificar el archivo principal. Esto evita conflictos
en actualizaciones.

### Archivos con extensión `.conf`

Convención estándar para archivos de configuración:

```bash
ls /etc/*.conf
# /etc/adduser.conf
# /etc/host.conf
# /etc/nsswitch.conf
# /etc/resolv.conf
# /etc/sysctl.conf
```

No todos los archivos siguen esta convención (muchos no tienen extensión),
pero `.conf` es el estándar para nuevos servicios.

## Archivos fundamentales

### `/etc/hostname`

Nombre del host en una sola línea:

```bash
cat /etc/hostname
# webserver01
```

Cambiar el hostname:

```bash
# Método moderno (persistente)
sudo hostnamectl set-hostname newname

# Verificar
hostnamectl
# Static hostname: newname
```

### `/etc/hosts`

Resolución estática de nombres (antes de DNS):

```bash
cat /etc/hosts
# 127.0.0.1       localhost
# 127.0.1.1       webserver01
# ::1             localhost ip6-localhost
#
# 192.168.1.10    db-server
# 192.168.1.11    app-server
```

Se consulta **antes** que DNS (configurable en `/etc/nsswitch.conf`). Útil
para entornos locales, desarrollo, y sobrescribir DNS temporalmente.

### `/etc/resolv.conf`

Configuración de resolución DNS:

```bash
cat /etc/resolv.conf
# nameserver 8.8.8.8
# nameserver 8.8.4.4
# search example.com
```

En sistemas modernos con systemd-resolved o NetworkManager, este archivo
puede ser un symlink gestionado automáticamente:

```bash
ls -la /etc/resolv.conf
# lrwxrwxrwx ... /etc/resolv.conf -> ../run/systemd/resolve/stub-resolv.conf
```

### `/etc/fstab`

Tabla de sistemas de archivos a montar al arranque:

```bash
cat /etc/fstab
```

```
# <filesystem>        <mount>   <type>  <options>           <dump> <pass>
UUID=abc123-...       /         ext4    errors=remount-ro   0      1
UUID=def456-...       /home     ext4    defaults            0      2
UUID=ghi789-...       none      swap    sw                  0      0
tmpfs                 /tmp      tmpfs   defaults,size=2G    0      0
```

Campos:

| Campo | Significado |
|---|---|
| filesystem | Dispositivo (UUID, LABEL, o path) |
| mount | Punto de montaje |
| type | Tipo de filesystem (ext4, xfs, tmpfs, nfs) |
| options | Opciones de montaje (defaults, ro, noexec, nosuid) |
| dump | Backup con dump (0 = no, obsoleto) |
| pass | Orden de fsck al arranque (0 = no verificar) |

### `/etc/passwd` y `/etc/shadow`

Base de datos de usuarios:

```bash
# passwd: información pública de usuarios
cat /etc/passwd
# root:x:0:0:root:/root:/bin/bash
# dev:x:1000:1000::/home/dev:/bin/bash
# nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
```

Formato: `usuario:x:UID:GID:comentario:home:shell`

```bash
# shadow: hashes de contraseñas (solo legible por root)
sudo cat /etc/shadow
# root:$y$j9T$...:19500:0:99999:7:::
# dev:$y$j9T$...:19500:0:99999:7:::
```

Cubierto en detalle en C02 (Usuarios, Grupos y Permisos).

### `/etc/group` y `/etc/gshadow`

Base de datos de grupos:

```bash
cat /etc/group
# root:x:0:
# sudo:x:27:dev
# docker:x:999:dev
# dev:x:1000:
```

Formato: `grupo:x:GID:miembros`

### `/etc/sudoers`

Configuración de privilegios sudo. **Nunca editar directamente** — usar
`visudo` que valida la sintaxis:

```bash
sudo visudo
```

```
# Formato: quién dónde=(como_quién) qué
root    ALL=(ALL:ALL) ALL
%sudo   ALL=(ALL:ALL) ALL          # Grupo sudo en Debian
%wheel  ALL=(ALL:ALL) ALL          # Grupo wheel en RHEL
dev     ALL=(ALL) NOPASSWD: ALL    # Sin contraseña (desarrollo)
```

Los fragmentos modulares van en `/etc/sudoers.d/`:

```bash
ls /etc/sudoers.d/
# 01-dev-nopasswd
```

### `/etc/ssh/sshd_config`

Configuración del servidor SSH:

```bash
cat /etc/ssh/sshd_config
# Port 22
# PermitRootLogin prohibit-password
# PasswordAuthentication yes
# PubkeyAuthentication yes
# X11Forwarding yes
```

Diferencias entre distribuciones:

| Opción | Debian default | RHEL default |
|---|---|---|
| PermitRootLogin | `prohibit-password` | `yes` (RHEL 8), `prohibit-password` (RHEL 9) |
| UsePAM | `yes` | `yes` |
| X11Forwarding | `yes` | `no` |

### `/etc/nsswitch.conf`

Name Service Switch — define el orden de búsqueda para nombres, usuarios,
hosts, etc.:

```bash
cat /etc/nsswitch.conf
# passwd:     files systemd
# group:      files systemd
# shadow:     files
# hosts:      files dns mymachines
# networks:   files
```

`hosts: files dns` significa: buscar primero en `/etc/hosts`, luego en DNS.

## Archivos de configuración de red

### Debian/Ubuntu (netplan o ifupdown)

```bash
# Moderno (netplan)
ls /etc/netplan/
cat /etc/netplan/01-netcfg.yaml

# Legacy (ifupdown)
cat /etc/network/interfaces
```

### RHEL/AlmaLinux (NetworkManager)

```bash
# Conexiones de NetworkManager
ls /etc/NetworkManager/system-connections/
nmcli connection show
```

En RHEL 9, los archivos legacy de `/etc/sysconfig/network-scripts/` fueron
deprecados a favor de keyfiles de NetworkManager.

## Configuración de servicios

Los servicios instalados por paquetes colocan su configuración en `/etc`:

```
/etc/nginx/              ← Configuración de nginx
├── nginx.conf           ← Archivo principal
├── sites-available/     ← Sitios disponibles [Debian]
├── sites-enabled/       ← Sitios activos (symlinks) [Debian]
└── conf.d/              ← Fragmentos de configuración [RHEL]

/etc/ssh/                ← Configuración de SSH
├── sshd_config          ← Servidor
└── ssh_config           ← Cliente

/etc/systemd/            ← Overrides de systemd
├── system/              ← Overrides de servicios del sistema
└── user/                ← Overrides de servicios de usuario
```

### Configuración del sistema vs defaults

Los archivos en `/etc` sobrescriben los defaults del software:

```
/usr/lib/systemd/system/sshd.service  ← Default (del paquete)
/etc/systemd/system/sshd.service      ← Override (del administrador)
```

El gestor de paquetes actualiza `/usr/lib/` sin conflictos. El administrador
edita `/etc/` sin que sus cambios se pierdan en actualizaciones.

## Backup de /etc

`/etc` es el directorio más importante para respaldar. Con un backup de `/etc`
y la lista de paquetes instalados, se puede reconstruir un servidor:

```bash
# Backup completo de /etc
sudo tar czf /backup/etc-$(date +%Y%m%d).tar.gz /etc/

# Lista de paquetes instalados
dpkg --get-selections > /backup/packages.list    # Debian
rpm -qa > /backup/packages.list                  # RHEL

# Restaurar paquetes + /etc = servidor reconstruido
```

Para gestión de configuración a escala, se usan herramientas como Ansible,
Puppet o Salt que versionan los cambios de `/etc` y los aplican
reproduciblemente.

---

## Ejercicios

### Ejercicio 1 — Explorar /etc

```bash
# Contar archivos en /etc
find /etc -type f | wc -l

# Ver los directorios .d/
find /etc -name "*.d" -type d | sort

# Ver los archivos de configuración modificados recientemente
find /etc -type f -mtime -7 | head -20
```

### Ejercicio 2 — Comparar entre distribuciones

```bash
# ¿Qué archivos existen en Debian pero no en AlmaLinux?
docker compose exec debian-dev ls /etc/ > /tmp/etc-debian.txt
docker compose exec alma-dev ls /etc/ > /tmp/etc-alma.txt
diff /tmp/etc-debian.txt /tmp/etc-alma.txt
```

### Ejercicio 3 — Inspeccionar la cadena de resolución de nombres

```bash
# 1. ¿Qué dice nsswitch.conf?
cat /etc/nsswitch.conf | grep hosts

# 2. ¿Qué hay en /etc/hosts?
cat /etc/hosts

# 3. ¿Qué DNS se usa?
cat /etc/resolv.conf

# 4. Probar la resolución
getent hosts localhost
getent hosts google.com
```
