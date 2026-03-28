# T02 — Almacenamiento

## Dónde se guardan los logs

El journal almacena los logs en formato binario en una de dos ubicaciones:

```bash
# Persistente — sobrevive reboots:
/var/log/journal/<machine-id>/

# Volátil — se pierde al reiniciar:
/run/log/journal/<machine-id>/

# Ver cuál está activo:
ls /var/log/journal/ 2>/dev/null && echo "Persistente" || echo "Volátil"
```

```bash
# El machine-id identifica la instalación:
cat /etc/machine-id
# a1b2c3d4e5f6...

# Los archivos de journal:
ls -lh /var/log/journal/*/
# system.journal         — journal activo del sistema
# system@*.journal       — journals rotados (archivados)
# user-1000.journal      — journal del usuario UID 1000
# user-1000@*.journal    — journals de usuario rotados
```

## Configuración — journald.conf

```bash
# Archivo de configuración:
cat /etc/systemd/journald.conf

# O con drop-ins:
ls /etc/systemd/journald.conf.d/
# La configuración de journald se recarga con:
sudo systemctl restart systemd-journald
```

### Storage= — Modo de almacenamiento

```ini
# /etc/systemd/journald.conf
[Journal]
Storage=auto
```

| Valor | Comportamiento |
|---|---|
| `volatile` | Solo en `/run/log/journal/` (RAM, se pierde al reiniciar) |
| `persistent` | En `/var/log/journal/` (disco, sobrevive reboots). Crea el directorio si no existe |
| `auto` | En `/var/log/journal/` si el directorio **ya existe**, en `/run/log/journal/` si no. **Default** |
| `none` | No almacenar (los logs se descartan). stdout/stderr de servicios se pierden |

```bash
# Verificar el modo actual:
journalctl --header | grep "Storage"

# Habilitar persistencia (si es auto y /var/log/journal/ no existe):
sudo mkdir -p /var/log/journal
sudo systemd-tmpfiles --create --prefix /var/log/journal
sudo systemctl restart systemd-journald
# O cambiar Storage=persistent en journald.conf

# Diferencia por distribución:
# Debian/Ubuntu: Storage=auto, /var/log/journal/ generalmente existe → persistente
# RHEL 7: Storage=auto, /var/log/journal/ no existía → volátil
# RHEL 8+/Fedora: Storage=persistent por defecto → persistente
```

### Volátil vs persistente — Cuándo usar cada uno

```bash
# Volátil (Storage=volatile):
# - Contenedores efímeros (no necesitan logs entre reinicios)
# - Sistemas embebidos con almacenamiento limitado
# - Discos lentos donde el logging impacta rendimiento
# - Testing/desarrollo

# Persistente (Storage=persistent):
# - Servidores de producción
# - Diagnóstico de problemas de boot (necesitas logs del boot anterior)
# - Cumplimiento normativo (retención de logs)
# - Auditoría
```

## Límites de tamaño

### Almacenamiento persistente

```ini
[Journal]
# Tamaño máximo total del journal en disco:
SystemMaxUse=500M
# Default: 10% del filesystem, máximo 4G

# Espacio libre mínimo que debe quedar en disco:
SystemKeepFree=1G
# Default: 15% del filesystem

# Tamaño máximo de un archivo individual de journal:
SystemMaxFileSize=100M
# Default: 1/8 de SystemMaxUse

# Tamaño máximo de archivos de journal activos (no archivados):
SystemMaxFiles=100
# Limita el número de archivos archivados
```

### Almacenamiento volátil (RAM)

```ini
[Journal]
# Tamaño máximo en RAM:
RuntimeMaxUse=50M
# Default: 10% de la RAM, máximo 4G

# RAM libre mínima:
RuntimeKeepFree=100M
# Default: 15% de la RAM

# Tamaño máximo de un archivo individual:
RuntimeMaxFileSize=10M
```

```bash
# Ver el uso actual:
journalctl --disk-usage
# Archived and active journals take up 256.0M in the file system.

# Ver los límites efectivos:
journalctl -u systemd-journald --no-pager | grep -i "max\|limit\|runtime"
# O:
systemctl show systemd-journald --property=SystemMaxUse,RuntimeMaxUse
```

## Rotación

El journal rota automáticamente cuando se alcanzan los límites:

```bash
# La rotación ocurre cuando:
# 1. Un archivo de journal alcanza SystemMaxFileSize/RuntimeMaxFileSize
# 2. El total excede SystemMaxUse/RuntimeMaxUse
# 3. El espacio libre cae por debajo de SystemKeepFree/RuntimeKeepFree

# Los archivos rotados se nombran:
# system@XXXX.journal (XXXX = secuencia)
# Se eliminan los más antiguos primero cuando se necesita espacio
```

### Forzar rotación

```bash
# Rotar inmediatamente (cerrar journal activo, abrir uno nuevo):
sudo journalctl --rotate

# Rotar Y limpiar en un solo paso:
sudo journalctl --rotate --vacuum-size=100M
sudo journalctl --rotate --vacuum-time=30d
```

## Limpieza manual

```bash
# Limpiar por tamaño (mantener máximo N):
sudo journalctl --vacuum-size=200M
# Vacuuming done, freed 156.0M of archived journals from /var/log/journal/...

# Limpiar por tiempo (eliminar más antiguos que N):
sudo journalctl --vacuum-time=7d       # 7 días
sudo journalctl --vacuum-time=1month   # 1 mes

# Limpiar por número de archivos:
sudo journalctl --vacuum-files=5       # mantener solo 5 archivos

# Verificar el resultado:
journalctl --disk-usage
```

## Otras configuraciones

### Compresión

```ini
[Journal]
# Comprimir journals (default: sí):
Compress=yes
# Reduce significativamente el espacio en disco
# Overhead de CPU mínimo

# Umbral de compresión (no comprimir entradas pequeñas):
Compress=512
# Solo comprimir entradas mayores a 512 bytes
```

### Rate limiting

```ini
[Journal]
# Limitar la cantidad de mensajes por servicio:
RateLimitIntervalSec=30s
RateLimitBurst=10000
# Máximo 10000 mensajes cada 30 segundos por unidad
# Si se excede, journald suprime mensajes y registra cuántos se perdieron

# Deshabilitar rate limiting:
RateLimitIntervalSec=0
# Útil para depuración, NO recomendado en producción
```

```bash
# Cuando se activa el rate limiting:
# journald registra:
# "Suppressed N messages from unit.service"
# Esto indica que el servicio está generando demasiados logs
# → solución: arreglar el servicio, no deshabilitar el rate limit
```

### Forwarding

```ini
[Journal]
# Reenviar a syslog (para rsyslog/syslog-ng):
ForwardToSyslog=yes      # default en la mayoría de distros

# Reenviar a la consola del kernel (kmsg):
ForwardToKMsg=no

# Reenviar a la consola:
ForwardToConsole=no
# Si se habilita:
TTYPath=/dev/console      # a qué TTY enviar
MaxLevelConsole=info       # nivel máximo a enviar

# Reenviar a un socket:
ForwardToWall=yes          # mensajes de emergencia a todos los terminales
MaxLevelWall=emerg
```

### Seal (integridad)

```ini
[Journal]
# Forward Secure Sealing — proteger la integridad de los logs:
Seal=yes
# Usa criptografía para detectar modificaciones en los journals
# Útil para auditoría y forense

# Verificar integridad:
journalctl --verify
# PASS: /var/log/journal/.../system.journal
# PASS: /var/log/journal/.../user-1000.journal
```

## Permisos y acceso

```bash
# Por defecto, los logs del sistema solo son legibles por root
# y miembros del grupo systemd-journal:
ls -la /var/log/journal/
# drwxr-sr-x 2 root systemd-journal

# Dar acceso a un usuario sin root:
sudo usermod -aG systemd-journal username
# Después de re-login, el usuario puede usar journalctl sin sudo

# Los logs de usuario son accesibles por el propio usuario:
journalctl --user
# Muestra logs de la sesión del usuario actual
```

## Configuración recomendada para servidores

```ini
# /etc/systemd/journald.conf.d/server.conf
[Journal]
Storage=persistent
Compress=yes
SystemMaxUse=1G
SystemMaxFileSize=100M
SystemKeepFree=2G
RateLimitIntervalSec=30s
RateLimitBurst=10000
ForwardToSyslog=yes
MaxRetentionSec=3month
```

```bash
sudo systemctl restart systemd-journald
```

---

## Ejercicios

### Ejercicio 1 — Verificar almacenamiento

```bash
# 1. ¿Almacenamiento persistente o volátil?
ls -d /var/log/journal 2>/dev/null && echo "Persistente" || echo "Volátil"

# 2. ¿Cuánto espacio usa el journal?
journalctl --disk-usage

# 3. ¿Cuántos boots tiene registrados?
journalctl --list-boots --no-pager | wc -l
```

### Ejercicio 2 — Configuración actual

```bash
# Ver la configuración efectiva:
systemd-analyze cat-config systemd/journald.conf 2>/dev/null || \
    cat /etc/systemd/journald.conf
```

### Ejercicio 3 — Limpieza

```bash
# Ver el uso actual:
journalctl --disk-usage

# Simular limpieza (ver cuánto se liberaría manteniendo 7 días):
# sudo journalctl --vacuum-time=7d  (quitar sudo para ver sin ejecutar)

# Verificar integridad:
journalctl --verify 2>&1 | tail -5
```
