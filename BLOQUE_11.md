# Bloque 11: Seguridad, Kernel y Arranque

**Objetivo**: Dominar seguridad del sistema, gestión del kernel y el proceso de
arranque a nivel LPIC-2 y RHCSA.

## Capítulo 1: SELinux [A]

### Sección 1: Fundamentos
- **T01 - MAC vs DAC**: por qué SELinux existe, qué protege que los permisos normales no
- **T02 - Modos**: Enforcing, Permissive, Disabled — getenforce, setenforce
- **T03 - Contextos**: user:role:type:level, ls -Z, ps -Z, id -Z
- **T04 - Política targeted**: qué procesos están confinados, unconfined_t

### Sección 2: Gestión
- **T01 - Booleans**: getsebool, setsebool -P, semanage boolean
- **T02 - File contexts**: semanage fcontext, restorecon, chcon (temporal vs permanente)
- **T03 - Troubleshooting**: audit2why, audit2allow, /var/log/audit/audit.log, sealert
- **T04 - Ports**: semanage port, permitir servicios en puertos no estándar

## Capítulo 2: AppArmor [A]

### Sección 1: Fundamentos
- **T01 - Perfiles**: enforce vs complain, aa-status
- **T02 - Gestión de perfiles**: aa-enforce, aa-complain, aa-disable
- **T03 - Crear perfiles**: aa-genprof, aa-logprof, sintaxis de perfiles
- **T04 - SELinux vs AppArmor**: comparación práctica, cuándo encontrar cada uno

## Capítulo 3: Hardening y Auditoría [A]

### Sección 1: Seguridad del Sistema
- **T01 - sudo avanzado**: /etc/sudoers, visudo, aliases, NOPASSWD, restricción por comando
- **T02 - auditd**: reglas de auditoría, ausearch, aureport, monitoreo de archivos sensibles
- **T03 - Hardening básico**: desactivar servicios innecesarios, kernel parameters de seguridad
- **T04 - GPG**: generación de claves, cifrado, firma, verificación, keyservers

### Sección 2: Criptografía y PKI
- **T01 - OpenSSL**: generación de certificados, CSR, verificación, s_client
- **T02 - LUKS**: cifrado de disco, cryptsetup, desbloqueo al arranque
- **T03 - dm-crypt**: capa inferior de LUKS, configuración manual

## Capítulo 4: Proceso de Arranque [A]

### Sección 1: Boot
- **T01 - BIOS vs UEFI**: diferencias, Secure Boot, EFI System Partition
- **T02 - GRUB2**: instalación, grub.cfg, grub-mkconfig, GRUB_DEFAULT, parámetros del kernel
- **T03 - Edición de GRUB en boot**: recovery mode, init=/bin/bash, password de GRUB
- **T04 - Proceso de arranque completo**: firmware → bootloader → kernel → initramfs → init → target

### Sección 2: Initramfs
- **T01 - Qué es initramfs**: para qué sirve, cuándo se necesita regenerar
- **T02 - dracut (RHEL) vs update-initramfs (Debian)**: regeneración, módulos, troubleshooting

## Capítulo 5: Kernel [A]

### Sección 1: Módulos del Kernel
- **T01 - lsmod, modprobe, modinfo**: listar, cargar, descargar módulos
- **T02 - /etc/modprobe.d/**: blacklisting, opciones de módulo, aliases
- **T03 - depmod**: dependencias entre módulos, modules.dep

### Sección 2: Configuración del Kernel en Runtime
- **T01 - sysctl**: /proc/sys, /etc/sysctl.conf, sysctl -w, parámetros comunes
- **T02 - /proc y /sys en profundidad**: información del kernel, tuning de red, scheduler
- **T03 - Compilación del kernel**: descargar, .config, make menuconfig, make, make modules_install, make install
- **T04 - dkms**: módulos de terceros, reconstrucción automática al actualizar kernel

## Capítulo 6: Mantenimiento del Sistema [A]

### Sección 1: Backup y Recuperación
- **T01 - tar**: creación de backups, compresión, incrementales (--listed-incremental)
- **T02 - rsync**: sincronización, --delete, -a, exclusiones, backups remotos con SSH
- **T03 - dd**: clonar discos/particiones, crear imágenes, precauciones
- **T04 - Estrategias de backup**: completo, incremental, diferencial, rotación, 3-2-1

### Sección 2: Automatización
- **T01 - Scripts de mantenimiento**: limpieza de logs, verificación de espacio, alertas
- **T02 - Compilación desde fuente avanzada**: configure options, make check/test, DESTDIR

## Capítulo 7: Proyecto — System Health Check [P]

### Sección 1: Herramienta de Diagnóstico del Sistema
- **T01 - Recolección de datos**: disk usage (df), memory (free), load average, servicios fallidos (systemctl --failed)
- **T02 - Formateo y reporte**: salida coloreada en terminal, umbrales de alerta (amarillo/rojo), resumen ejecutivo
- **T03 - Mejora 1**: exportar reporte como JSON o HTML
- **T04 - Mejora 2**: interfaz TUI con ratatui (paneles de disco, memoria, servicios, logs recientes)
