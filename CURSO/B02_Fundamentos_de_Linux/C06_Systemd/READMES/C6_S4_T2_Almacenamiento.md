# T02 — Almacenamiento

## Errata sobre los materiales originales

| # | Archivo | Línea | Error | Corrección |
|---|---------|-------|-------|------------|
| 1 | max.md | 104 | "Logs se pierdan" (modo subjuntivo incorrecto) | Corregido: "se pierden" |
| 2 | base.md | 108 | `SystemMaxFiles` descrito como "Tamaño máximo de archivos activos" | Controla el **número** de archivos archivados, no el tamaño ni los activos |
| 3 | max.md | 182-188 | `systemctl show systemd-journald --property=SystemMaxUse` — no son propiedades D-Bus | Usar `systemd-analyze cat-config systemd/journald.conf` o leer el archivo directamente |
| 4 | max.md | 322 | "corruptado" (forma incorrecta) | Corregido: "corrupto" |

---

## Dónde se guardan los logs

El journal almacena los logs en formato binario en una de dos ubicaciones:

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

# Ver los archivos del journal:
ls -lh /var/log/journal/*/

# El machine-id identifica la instalación:
cat /etc/machine-id
```

---

## Configuración — journald.conf

```bash
# Archivo principal:
cat /etc/systemd/journald.conf

# Drop-ins (sobrescriben el principal):
ls /etc/systemd/journald.conf.d/

# Ver la configuración efectiva (archivo + drop-ins merged):
systemd-analyze cat-config systemd/journald.conf

# Recargar sin reiniciar (no cierra journals abiertos):
sudo systemctl kill -s HUP systemd-journald

# Reiniciar completamente (cierra journals, abre nuevos):
sudo systemctl restart systemd-journald
```

### Storage= — Modo de almacenamiento

```ini
# /etc/systemd/journald.conf
[Journal]
Storage=auto
```

| Valor | Ubicación | Comportamiento |
|---|---|---|
| `volatile` | `/run/log/journal/` | RAM; se pierde al reiniciar |
| `persistent` | `/var/log/journal/` | Disco; sobrevive reboots. **Crea el directorio si no existe** |
| `auto` | `/var/` si existe, sino `/run/` | **Default**. Depende de si `/var/log/journal/` ya existe |
| `none` | — | No almacena logs; se descartan |

```bash
# Verificar el modo actual:
journalctl --header | grep "Storage"

# Habilitar persistencia (si Storage=auto y el directorio no existe):
sudo mkdir -p /var/log/journal
sudo systemd-tmpfiles --create --prefix /var/log/journal
sudo systemctl restart systemd-journald

# O cambiar Storage=persistent en journald.conf (crea el directorio solo):
# [Journal]
# Storage=persistent
```

### Diferencias por distribución

| Distribución | Storage default | Resultado |
|---|---|---|
| Debian/Ubuntu | `auto` | `/var/log/journal/` generalmente existe → **persistente** |
| RHEL 7 | `auto` | `/var/log/journal/` no existía → **volátil** |
| RHEL 8+/Fedora | `persistent` | Siempre **persistente** |
| Arch Linux | `auto` | Depende de configuración manual |

### Cuándo usar cada modo

| Modo | Caso de uso |
|---|---|
| **Volátil** | Contenedores efímeros, sistemas embebidos, testing, discos lentos |
| **Persistente** | Servidores de producción, diagnóstico de boot, auditoría, cumplimiento normativo |

---

## Límites de tamaño

### Almacenamiento persistente (disco)

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

# Número máximo de archivos de journal archivados:
SystemMaxFiles=100
```

### Almacenamiento volátil (RAM)

```ini
[Journal]
# Tamaño máximo en RAM:
RuntimeMaxUse=50M
# Default: 10% de la RAM, máximo 4G

# RAM libre mínima que debe quedar:
RuntimeKeepFree=100M
# Default: 15% de la RAM

# Tamaño máximo de un archivo individual:
RuntimeMaxFileSize=10M
```

```bash
# Ver uso actual:
journalctl --disk-usage
# Archived and active journals take up 256.0M in the file system.

# Ver la configuración efectiva (todos los límites):
systemd-analyze cat-config systemd/journald.conf | grep -E "Max|KeepFree|Runtime"
```

---

## Retención temporal

```ini
[Journal]
# Tiempo máximo de retención de logs:
MaxRetentionSec=3month
# Default: ilimitado (solo limitado por tamaño)

# Se combina con los límites de tamaño:
# Los logs se eliminan cuando se alcanza CUALQUIERA de los límites
# (el que se cumpla primero)
```

---

## Rotación

El journal rota **automáticamente** cuando se alcanzan los límites:

1. Un archivo alcanza `SystemMaxFileSize` → se archiva, se abre uno nuevo
2. El total excede `SystemMaxUse` → se eliminan journals más antiguos
3. El espacio libre cae bajo `SystemKeepFree` → se eliminan journals antiguos

```bash
# Los archivos rotados se nombran:
# system@0000.journal, system@0001.journal, ...
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

---

## Limpieza manual

```bash
# Limpiar por tamaño (reducir a máximo 200MB):
sudo journalctl --vacuum-size=200M
# Vacuuming done, freed 156.0M of archived journals.

# Limpiar por tiempo (eliminar más antiguos que 7 días):
sudo journalctl --vacuum-time=7d

# Limpiar por número de archivos (mantener solo 5):
sudo journalctl --vacuum-files=5

# Verificar el resultado:
journalctl --disk-usage
```

> **Nota**: `--vacuum-*` solo elimina archivos **archivados** (rotados).
> El journal activo no se elimina — para eso, primero `--rotate`.

---

## Otras configuraciones

### Compresión

```ini
[Journal]
# Comprimir journals (default: yes):
Compress=yes
# Entradas >512 bytes se comprimen con LZ4/ZSTD
# Reduce ~80% del espacio en disco
# Overhead de CPU mínimo (~2-3%)

# Umbral de compresión personalizado:
Compress=512
# Solo comprimir entradas mayores a 512 bytes
```

### Rate limiting

```ini
[Journal]
# Máximo de mensajes por unidad por intervalo:
RateLimitIntervalSec=30s
RateLimitBurst=10000
# Default: 10000 mensajes cada 30 segundos por unidad

# Deshabilitar rate limiting (solo para debug):
RateLimitIntervalSec=0
# NO recomendado en producción
```

```bash
# Cuando el rate limit se activa, journald registra:
# "Suppressed N messages from unit.service"
# Esto indica que el servicio genera demasiados logs
# → Solución correcta: arreglar el servicio, no deshabilitar el rate limit
```

### Forwarding (reenvío a otros destinos)

```ini
[Journal]
# Reenviar a syslog (rsyslog/syslog-ng):
ForwardToSyslog=yes              # default en la mayoría de distros

# Reenviar al buffer del kernel (dmesg):
ForwardToKMsg=no

# Reenviar a la consola:
ForwardToConsole=no
TTYPath=/dev/console             # a qué TTY enviar
MaxLevelConsole=info             # nivel máximo a enviar

# Reenviar a wall (todos los terminales):
ForwardToWall=yes                # solo mensajes de emergencia
MaxLevelWall=emerg
```

### Seal (integridad criptográfica)

```ini
[Journal]
# Forward Secure Sealing:
Seal=yes
# Usa criptografía para detectar modificaciones maliciosas en los logs
# Recomendado para auditoría y forense

# Verificar integridad:
# journalctl --verify
# PASS: /var/log/journal/.../system.journal
# Si hay corrupción: FAIL
```

---

## Permisos y acceso

```bash
# Por defecto, solo root y miembros del grupo systemd-journal pueden leer:
ls -la /var/log/journal/
# drwxr-sr-x+ 2 root systemd-journal

# Dar acceso a un usuario sin root:
sudo usermod -aG systemd-journal username
# Necesita re-login para tomar efecto

# Verificar:
groups username
# username : username ... systemd-journal

# Logs de usuario (accesibles por el propio usuario):
journalctl --user
# Servicios de usuario:
journalctl --user-unit=myapp.service
```

---

## Configuración recomendada para servidores

```ini
# /etc/systemd/journald.conf.d/server.conf
[Journal]
Storage=persistent
Compress=yes
Seal=yes
SystemMaxUse=1G
SystemMaxFileSize=100M
SystemKeepFree=2G
RateLimitIntervalSec=30s
RateLimitBurst=10000
MaxRetentionSec=3month
ForwardToSyslog=yes
```

```bash
# Aplicar:
sudo systemd-tmpfiles --create --prefix /var/log/journal
sudo systemctl restart systemd-journald

# Verificar:
journalctl --header | grep "Storage"
journalctl --disk-usage
```

---

## Quick reference — Límites

| Parámetro | Default | Aplica a |
|---|---|---|
| `SystemMaxUse` | 10% del FS (max 4G) | Disco |
| `SystemKeepFree` | 15% del FS | Disco |
| `SystemMaxFileSize` | 1/8 de SystemMaxUse | Disco |
| `SystemMaxFiles` | 100 | Disco |
| `RuntimeMaxUse` | 10% de RAM (max 4G) | RAM |
| `RuntimeKeepFree` | 15% de RAM | RAM |
| `RuntimeMaxFileSize` | 1/8 de RuntimeMaxUse | RAM |
| `MaxRetentionSec` | ilimitado | Ambos |
| `RateLimitBurst` | 10000 | Ambos |
| `Compress` | yes | Ambos |
| `Seal` | no | Ambos |
| `ForwardToSyslog` | yes | Ambos |

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

# 4. ¿Cuál es el machine-id?
cat /etc/machine-id
```

<details><summary>Predicción</summary>

- Si `/var/log/journal/` existe → persistente; si solo existe
  `/run/log/journal/` → volátil
- El espacio usado dependerá de la antigüedad y actividad del sistema
  (típicamente 50-500MB)
- Si es volátil, solo habrá 1 boot (el actual); si es persistente,
  habrá tantos boots como reinicios desde que se habilitó
- El machine-id será un string hex de 32 caracteres, único por instalación

</details>

### Ejercicio 2 — Leer la configuración efectiva

```bash
# Ver la configuración merged (archivo + drop-ins):
systemd-analyze cat-config systemd/journald.conf 2>/dev/null || \
    cat /etc/systemd/journald.conf

# ¿Hay drop-ins?
ls /etc/systemd/journald.conf.d/ 2>/dev/null || echo "Sin drop-ins"
```

<details><summary>Predicción</summary>

- `systemd-analyze cat-config` mostrará el contenido del archivo principal
  con los drop-ins insertados en el lugar correcto
- La mayoría de líneas estarán comentadas (prefijo `#`) — las directivas
  comentadas muestran el valor default
- Si hay drop-ins, sus valores sobrescriben los del archivo principal
- En un sistema sin configuración custom, todo estará en defaults

</details>

### Ejercicio 3 — Inspeccionar archivos del journal

```bash
# 1. Listar archivos de journal con tamaños:
ls -lhS /var/log/journal/*/ 2>/dev/null || \
    ls -lhS /run/log/journal/*/ 2>/dev/null

# 2. ¿Cuántos archivos hay?
find /var/log/journal -name "*.journal" 2>/dev/null | wc -l || \
    find /run/log/journal -name "*.journal" 2>/dev/null | wc -l

# 3. ¿Hay journals de usuario?
ls /var/log/journal/*/user-*.journal 2>/dev/null || echo "Sin journals de usuario"
```

<details><summary>Predicción</summary>

- `system.journal` será el archivo más grande (es el activo)
- `system@*.journal` son los archivados (rotados), más pequeños
- Los journals de usuario (`user-1000.journal`) solo existen si hay
  sesiones de usuario activas con servicios de usuario
- `-lhS` ordena por tamaño (mayor primero) para identificar rápidamente
  cuánto ocupa cada archivo

</details>

### Ejercicio 4 — Verificar integridad

```bash
# 1. Verificar todos los archivos:
journalctl --verify 2>&1

# 2. Contar archivos verificados:
journalctl --verify 2>&1 | grep -c "PASS"

# 3. ¿Hay algún archivo corrupto?
journalctl --verify 2>&1 | grep "FAIL" || echo "Todos los archivos están íntegros"
```

<details><summary>Predicción</summary>

- En un sistema saludable, todos los archivos mostrarán PASS
- `--verify` comprueba la estructura interna del formato binario
- Si `Seal=yes` está configurado, también verifica las firmas criptográficas
- Un FAIL indicaría corrupción (disco defectuoso, apagado abrupto, o
  manipulación)

</details>

### Ejercicio 5 — Rotación y limpieza

```bash
# 1. Ver espacio antes:
journalctl --disk-usage

# 2. Forzar rotación:
sudo journalctl --rotate

# 3. Ver cuántos archivos hay ahora:
ls /var/log/journal/*/*.journal 2>/dev/null | wc -l

# 4. Simular limpieza (ver cuánto se liberaría manteniendo 100M):
echo "Espacio actual:"
journalctl --disk-usage
echo "Para limpiar: sudo journalctl --vacuum-size=100M"
```

<details><summary>Predicción</summary>

- `--rotate` cerrará el journal activo y creará uno nuevo — el número
  de archivos aumentará en 1
- `--disk-usage` mostrará el mismo total (rotar no libera espacio,
  solo cierra el archivo activo)
- `--vacuum-size=100M` eliminaría archivos archivados hasta que el
  total sea ≤ 100MB — solo los archivos rotados se eliminan, nunca
  el activo

</details>

### Ejercicio 6 — Volátil vs persistente (teórico)

```bash
# Pregunta: En estos escenarios, ¿qué Storage usarías?

echo "Escenario 1: Servidor de producción con requisito de auditoría"
echo "Escenario 2: Contenedor Docker efímero que se recrea cada hora"
echo "Escenario 3: Raspberry Pi con tarjeta SD de 8GB"
echo "Escenario 4: VM de desarrollo personal"
```

<details><summary>Predicción</summary>

1. **Servidor con auditoría**: `Storage=persistent` — los logs deben
   sobrevivir reboots y crashes para cumplir requisitos regulatorios
2. **Contenedor efímero**: `Storage=volatile` — se recrea cada hora, no
   tiene sentido escribir a disco; o `Storage=none` si los logs van a
   un sistema centralizado
3. **Raspberry Pi 8GB**: `Storage=volatile` — las escrituras constantes
   acortan la vida de la SD; usar SystemMaxUse bajo o enviar logs a un
   servidor remoto
4. **VM de desarrollo**: `Storage=auto` (default) — persistente si
   `/var/log/journal/` existe, volátil si no; cualquiera funciona para
   desarrollo

</details>

### Ejercicio 7 — Rate limiting

```bash
# 1. Ver si hay mensajes suprimidos:
journalctl --no-pager | grep -i "suppressed" | tail -5

# 2. Ver la configuración actual:
grep -i "RateLimit" /etc/systemd/journald.conf 2>/dev/null || \
    echo "RateLimit en defaults (10000/30s)"

# 3. Generar muchos logs para probar (con logger):
for i in $(seq 1 100); do
    logger -t rate-test "Mensaje de prueba $i"
done
journalctl -t rate-test --no-pager | wc -l
```

<details><summary>Predicción</summary>

- Si hay mensajes "Suppressed N messages from...", algún servicio está
  generando más logs de los permitidos por RateLimitBurst
- 100 mensajes con `logger` no deberían activar el rate limit (el default
  es 10000 mensajes en 30 segundos)
- El rate limit se aplica **por unidad**, no globalmente — un servicio
  ruidoso no afecta los logs de otros servicios
- Si se ven mensajes suprimidos en producción, investigar qué servicio
  los genera y por qué

</details>

### Ejercicio 8 — Permisos

```bash
# 1. ¿Quién puede leer los logs?
ls -la /var/log/journal/ 2>/dev/null || ls -la /run/log/journal/ 2>/dev/null

# 2. ¿Existe el grupo systemd-journal?
getent group systemd-journal

# 3. ¿Tu usuario está en el grupo?
groups | grep -q systemd-journal && \
    echo "Sí, puedes leer logs sin sudo" || \
    echo "No, necesitas sudo para journalctl"
```

<details><summary>Predicción</summary>

- El directorio tendrá permisos `drwxr-sr-x+` con grupo `systemd-journal`
  (el `s` = setgid, los archivos nuevos heredan el grupo)
- El grupo `systemd-journal` existirá como grupo del sistema
- Si el usuario no está en el grupo, `journalctl` sin sudo mostrará
  solo los logs del propio usuario o dará error de permisos
- `sudo usermod -aG systemd-journal username` + re-login resuelve esto

</details>

### Ejercicio 9 — Diseñar configuración para un servidor

```bash
# Dado:
# - Disco /var de 100GB
# - Retención requerida: 90 días
# - Necesita forwarding a rsyslog
# - Producción crítica

# Diseñar la configuración:
cat << 'EOF'
# /etc/systemd/journald.conf.d/production.conf
[Journal]
Storage=persistent
Compress=yes
Seal=yes
SystemMaxUse=2G
SystemMaxFileSize=128M
SystemKeepFree=5G
MaxRetentionSec=90day
RateLimitIntervalSec=30s
RateLimitBurst=10000
ForwardToSyslog=yes
EOF
```

<details><summary>Predicción</summary>

- `SystemMaxUse=2G` es ~2% del disco — conservador pero suficiente
  para 90 días con compresión
- `SystemKeepFree=5G` garantiza que los logs nunca llenen el disco
- `Seal=yes` protege la integridad para auditoría
- `ForwardToSyslog=yes` permite que rsyslog procese los logs en paralelo
  (enviar a central, filtrar, etc.)
- Si los 2G no alcanzan para 90 días, el límite de tamaño tiene prioridad
  — se eliminarán los más antiguos aunque no hayan cumplido 90 días
- Regla: tamaño y retención son **dos límites independientes**; el que
  se cumpla primero gana

</details>

### Ejercicio 10 — Migrar de volátil a persistente

```bash
# Verificar estado actual:
ls -d /var/log/journal 2>/dev/null && echo "Ya es persistente" && exit 0

# Pasos para migrar (en un entorno de prueba):
echo "1. sudo mkdir -p /var/log/journal"
echo "2. sudo systemd-tmpfiles --create --prefix /var/log/journal"
echo "3. Editar /etc/systemd/journald.conf → Storage=persistent"
echo "4. sudo systemctl restart systemd-journald"
echo "5. journalctl --header | grep Storage"
echo "6. journalctl --disk-usage"
```

<details><summary>Predicción</summary>

- Si `/var/log/journal/` no existe y `Storage=auto`, el journal está en
  `/run/log/journal/` (volátil)
- Crear el directorio con `mkdir` + `systemd-tmpfiles` establece los
  permisos correctos (grupo systemd-journal, setgid)
- Al reiniciar journald, los logs **nuevos** se escriben en disco; los
  logs anteriores (en RAM) se pierden
- Alternativa: cambiar `Storage=persistent` en journald.conf — journald
  crea el directorio automáticamente al reiniciar
- Después de migrar, `--list-boots` empezará con 1 boot (el actual);
  los boots anteriores que estaban en RAM se perdieron

</details>
