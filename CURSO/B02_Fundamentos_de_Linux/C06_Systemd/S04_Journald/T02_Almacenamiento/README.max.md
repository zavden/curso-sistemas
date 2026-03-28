# T02 — Almacenamiento

> **Objetivo:** Entender dónde, cómo y cuánto almacena el journal. Dominar la configuración de `journald.conf` para optimizar espacio, rendimiento y retención de logs.

## Dónde se guardan los logs

El journal almacena logs en formato binario en dos posibles ubicaciones según el modo `Storage=`:

```
/var/log/journal/<machine-id>/    ← Persistente (disco)
    system.journal                ← journal activo del sistema
    system@*.journal              ← journals archivados (rotados)
    user-1000.journal             ← journal del usuario UID 1000
    user-1000@*.journal           ← journals de usuario archivados

/run/log/journal/<machine-id>/   ← Volátil (RAM)
    (misma estructura, se pierde al reiniciar)
```

```bash
# Ver cuál está activo:
ls -d /var/log/journal 2>/dev/null && echo "Persistente" || echo "Volátil"

# Ver todos los archivos de journal:
ls -lh /var/log/journal/*/
# -rw-r-----+ 1 root systemd-journal  80M system.journal
# -rw-r-----+ 1 root systemd-journal  40M system@0000.journal
# -rw-r-----+ 1 root systemd-journal  40M system@0001.journal
# -rw-r-----+ 1 root systemd-journal  10M user-1000.journal

# El machine-id identifica la instalación:
cat /etc/machine-id
# a1b2c3d4e5f6...
```

---

## Configuración — journald.conf

```bash
# Archivo principal:
cat /etc/systemd/journald.conf

# Drop-ins (sobrescriben el principal):
ls /etc/systemd/journald.conf.d/

# Recargar sin reiniciar (no cierra journals abiertos):
sudo systemctl kill -s HUP systemd-journald

# Reiniciar (cierra journals, abre nuevos):
sudo systemctl restart systemd-journald
```

### Storage= — Modo de almacenamiento

```ini
# /etc/systemd/journald.conf
[Journal]
Storage=auto
```

| Valor | Ubicación | Comportamiento |
|-------|-----------|----------------|
| `volatile` | `/run/log/journal/` | RAM; se pierde al reiniciar |
| `persistent` | `/var/log/journal/` | Disco; sobrevive reboots. Crea el directorio si no existe |
| `auto` | `var/` si existe, sino `run/` | **Default**. Comportamiento depende de si `/var/log/journal/` ya existe |
| `none` | — | No almacena logs; se descartan. **Útil para embedded** |

```bash
# Verificar el modo actual:
journalctl --header | grep "Storage"

# Habilitar persistencia (cambiar de auto a persistent):
sudo mkdir -p /var/log/journal
sudo systemd-tmpfiles --create --prefix /var/log/journal
sudo systemctl restart systemd-journald

# O editar journald.conf:
# Storage=persistent
```

### Diferencias por distribución

| Distribución | Storage default | Comportamiento real |
|-------------|-----------------|---------------------|
| Debian/Ubuntu | `auto` | `/var/log/journal/` generalmente existe → **persistente** |
| RHEL 7 | `auto` | `/var/log/journal/` no existía → **volátil** |
| RHEL 8+/Fedora | `persistent` | Siempre persistente |
| Arch Linux | `auto` | Depende de configuración manual |

---

## Volátil vs Persistente — Cuándo usar cada uno

### Volátil (RAM)

```bash
# Ventajas:
# - Sin I/O de disco (mejor rendimiento)
# - Sin retención de datos sensibles en disco
# - Ideal para contenedores efímeros

# Desventajas:
# - Logs se pierdan al reiniciar
# - Límite de tamaño por RAM disponible
# - No sirve para diagnóstico post-mortem

# Casos de uso:
# - Contenedores Docker/Kubernetes
# - Sistemas embebidos con almacenamiento limitado
# - Testing/desarrollo
# - VMs efímeras
```

### Persistente (disco)

```bash
# Ventajas:
# - Logs sobreviven a reboots/crashes
# - Diagnóstico de boot anterior
# - Retención regulatoria (PCI-DSS, HIPAA, etc.)
# - Auditoría forense

# Desventajas:
# - I/O de disco (mitigado con compression=yes)
# - Consumo de espacio en disco

# Casos de uso:
# - Servidores de producción
# - Sistemas que requieren diagnóstico de crashes
# - Entornos regulados
# - Infrastructure as Code (auditoría)
```

---

## Límites de tamaño

### Persistente (disco)

```ini
[Journal]
# Tamaño máximo total del journal en disco
SystemMaxUse=500M
# Default: 10% del filesystem, máximo 4GB

# Espacio libre mínimo que debe quedar en disco
SystemKeepFree=1G
# Default: 15% del filesystem

# Tamaño máximo de un archivo individual de journal
SystemMaxFileSize=100M
# Default: 1/8 de SystemMaxUse

# Número máximo de archivos de journal
SystemMaxFiles=100
# Default: 100
```

### Volátil (RAM)

```ini
[Journal]
# Tamaño máximo en RAM:
RuntimeMaxUse=50M
# Default: 10% de la RAM, máximo 4GB

# RAM libre mínima que debe quedar:
RuntimeKeepFree=100M
# Default: 15% de la RAM

# Tamaño máximo de un archivo individual:
RuntimeMaxFileSize=10M
```

```bash
# Ver uso actual:
journalctl --disk-usage
# Archived and active journals take up 256.0M in /var/log/journal.

# Ver límites efectivos:
systemctl show systemd-journald --property=SystemMaxUse,RuntimeMaxUse
# SystemMaxUse=524288000
# RuntimeMaxUse=52428800

# Ver todos los límites de una vez:
systemctl show systemd-journald | grep -E "Max|KeepFree"
```

---

## Rotación automática

El journal rota **automáticamente** cuando se alcanzan los límites configurados:

```
┌─────────────────────────────────────────────────────────────┐
│  journald monitoriza continuamente:                         │
│                                                             │
│  1. Tamaño del archivo activo                              │
│     → si > SystemMaxFileSize → rota                        │
│                                                             │
│  2. Espacio total usado                                    │
│     → si > SystemMaxUse → elimina journals más antiguos   │
│                                                             │
│  3. Espacio libre en disco                                 │
│     → si < SystemKeepFree → elimina journals más antiguos │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Archivos rotados se nombran:
# system@0000.journal, system@0001.journal, ... (secuenciales)
# user-1000@0000.journal, ...

# Se eliminan los más antiguos primero cuando se necesita espacio
```

### Forzar rotación

```bash
# Rotar inmediatamente (cerrar journal activo, abrir uno nuevo):
sudo journalctl --rotate

# Rotar Y limpiar en un paso:
sudo journalctl --rotate --vacuum-size=100M
sudo journalctl --rotate --vacuum-time=30d
```

---

## Limpieza manual

```bash
# Limpiar por tamaño (reducir a máximo 200MB):
sudo journalctl --vacuum-size=200M
# Vacuuming done, freed 156.0M of archived journals.

# Limpiar por tiempo (solo últimos 7 días):
sudo journalctl --vacuum-time=7d

# Limpiar por número de archivos (mantener solo 5):
sudo journalctl --vacuum-files=5

# Verificar resultado:
journalctl --disk-usage
```

---

## Otras configuraciones

### Compresión

```ini
[Journal]
# Comprimir journals (default: yes):
Compress=yes
# Entradas >512 bytes se comprimen con LZ4/ZSTD
# Reduce ~80% del espacio en disco
# CPU overhead mínimo (~2-3%)

# Umbral de compresión (no comprimir entradas pequeñas):
Compress=512
# Solo comprimir entradas > 512 bytes
```

### Rate limiting

```ini
[Journal]
# Máximo de mensajes por unidad por intervalo:
RateLimitIntervalSec=30s
RateLimitBurst=1000
# Default: 1000 mensajes cada 30 segundos por unidad

# Deshabilitar rate limiting (ÚTIL PARA DEBUG, NO producción):
RateLimitIntervalSec=0
```

```bash
# Cuando el rate limit se activa, journald registra:
# "Suppressed N messages from unit.service"
# Esto indica que el servicio está generando demasiados logs
# → Solución: arreglar el servicio, no deshabilitar el rate limit
```

### Forwarding (reenvío a otros destinos)

```ini
[Journal]
# Reenviar a syslog (rsyslog/syslog-ng):
ForwardToSyslog=yes           # default en la mayoría

# Reenviar al buffer del kernel (dmesg):
ForwardToKMsg=no

# Reenviar a consola:
ForwardToConsole=no
TTYPath=/dev/console          # qué TTY
MaxLevelConsole=info          # nivel máximo

# Reenviar a wall (todos los terminales):
ForwardToWall=yes
MaxLevelWall=emerg            # solo emergencias van a wall
```

### Seal (integridad criptográfica)

```ini
[Journal]
# Forward Secure Sealing:
Seal=yes
# Protege contra modificaciones maliciosas de los logs
# Usa criptografía de clave pública para firmar entradas
# Recomendado para auditoría y forense

# Verificar integridad:
journalctl --verify
# PASS: /var/log/journal/.../system.journal
# PASS: /var/log/journal/.../user-1000.journal
# Si está corruptado: FAIL
```

---

## Permisos y acceso

```bash
# Por defecto, solo root y grupo systemd-journal pueden leer:
ls -la /var/log/journal/
# drwxr-sr-x+ 2 root systemd-journal  system/

# Agregar usuario al grupo (da acceso sin root):
sudo usermod -aG systemd-journal username
# Necesita re-login para que tome efecto

# Verificar:
groups username
# username : username ... systemd-journal

# Un usuario en el grupo puede ahora:
journalctl --no-pager
# Sin sudo y sin ser root
```

### Logs de usuario

```bash
# Cada usuario tiene su propio journal:
journalctl --user
# Logs de la sesión del usuario actual

# Servicios de usuario (user@1000.service):
journalctl --user-unit=myapp.service

# Ubicación:
# /var/log/journal/<machine-id>/user-1000.journal
# /run/log/journal/<machine-id>/user-1000.journal
```

---

## Configuración recomendada para servidores

```ini
# /etc/systemd/journald.conf.d/server.conf
[Journal]
# Persistencia
Storage=persistent

# Compresión
Compress=yes
Seal=yes

# Límites de tamaño (ajustar según disco disponible):
SystemMaxUse=1G
SystemMaxFileSize=100M
SystemKeepFree=2G
SystemMaxFiles=50

# Rate limiting
RateLimitIntervalSec=30s
RateLimitBurst=10000

# Retención máxima
MaxRetentionSec=3month

# Forwarding
ForwardToSyslog=yes
```

```bash
# Validar la configuración antes de reiniciar:
sudo systemd-tmpfiles --create --prefix /var/log/journal

# Reiniciar:
sudo systemctl restart systemd-journald

# Verificar:
journalctl --header | grep "Storage"
```

---

## Límites de journald.conf — Quick reference

```
┌──────────────────────────────────────────────────────────────┐
│ Parámetro               │ Default          │ Persistente    │
├─────────────────────────┼──────────────────┼────────────────┤
│ SystemMaxUse            │ 10% filesystem    │ Sí            │
│ SystemKeepFree          │ 15% filesystem    │ Sí            │
│ SystemMaxFileSize       │ 1/8 SystemMaxUse  │ Sí            │
│ SystemMaxFiles          │ 100               │ Sí            │
├─────────────────────────┼──────────────────┼────────────────┤
│ RuntimeMaxUse           │ 10% RAM           │ No (RAM)       │
│ RuntimeKeepFree         │ 15% RAM           │ No (RAM)       │
│ RuntimeMaxFileSize      │ 1/8 RuntimeMaxUse │ No (RAM)       │
├─────────────────────────┼──────────────────┼────────────────┤
│ RateLimitIntervalSec    │ 30s               │ Sí            │
│ RateLimitBurst          │ 1000              │ Sí            │
│ Compress                │ yes               │ Sí            │
│ Seal                    │ no                │ Sí            │
│ ForwardToSyslog         │ yes               │ Sí            │
│ MaxRetentionSec         │ unlimited         │ Sí            │
└─────────────────────────┴──────────────────┴────────────────┘
```

---

## Ejercicios

### Ejercicio 1 — Diagnosticar el almacenamiento actual

```bash
# 1. ¿Es persistente o volátil?
ls -d /var/log/journal 2>/dev/null && echo "Persistente" || echo "Volátil"

# 2. ¿Cuánto espacio usa?
journalctl --disk-usage

# 3. ¿Cuántos boots tiene registrados?
journalctl --list-boots --no-pager | wc -l

# 4. ¿Cuál es el modo Storage?
journalctl --header | grep "^Storage"

# 5. ¿Está configurado ForwardToSyslog?
grep ForwardToSyslog /etc/systemd/journald.conf
```

### Ejercicio 2 — Calcular límites para un servidor

```bash
# Dado un disco de 50GB para /var/log:
# SystemMaxUse = 1G  (2% del disco)
# SystemKeepFree = 2G
# SystemMaxFiles = 50

# Calcular:
# 50 archivos × 100M max cada uno = 5G máximo posible
# Pero SystemMaxUse=1G lo limita a 1GB real

# Ver los valores actuales:
systemctl show systemd-journald | grep -E "^(System|Runtime)"
```

### Ejercicio 3 — Migrar de volátil a persistente

> ⚠️ **Solo en entorno de pruebas**

```bash
# Hipótesis: servidor que quiere logs persistentes

# 1. Verificar estado actual:
ls -d /var/log/journal 2>/dev/null || echo "No existe (volátil)"

# 2. Crear directorio y configurar:
sudo mkdir -p /var/log/journal
sudo systemd-tmpfiles --create --prefix /var/log/journal
# Editar: Storage=persistent en /etc/systemd/journald.conf

# 3. Reiniciar:
sudo systemctl restart systemd-journald

# 4. Verificar:
ls -d /var/log/journal && echo "Ahora es persistente"
journalctl --header | grep "^Storage"
```

### Ejercicio 4 — Probar rate limiting

```bash
# 1. Ver la configuración actual:
grep -E "RateLimit" /etc/systemd/journald.conf

# 2. Simular un servicio que genera muchos logs:
# (crear unidad de test — NO hacer en producción)

# 3. Forzar rate limit y observar:
# journalctl debería mostrar "Suppressed N messages"
```

### Ejercicio 5 — Verificar integridad

```bash
# Verificar todos los archivos de journal:
journalctl --verify 2>&1

# ¿Cuántos archivos hay?
find /var/log/journal -name "*.journal" | wc -l

# ¿Cuántos boots hay?
journalctl --list-boots --no-pager | wc -l

# Limpiar journals antiguos conservando solo 3 boots:
sudo journalctl --vacuum-files=3
journalctl --disk-usage
```
