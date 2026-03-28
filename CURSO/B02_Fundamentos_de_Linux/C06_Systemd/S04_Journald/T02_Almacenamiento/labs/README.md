# Lab — Almacenamiento

## Objetivo

Configurar el almacenamiento del journal de systemd: modos de
almacenamiento (volatile, persistent, auto), ubicaciones en disco,
limites de tamano, rotacion, compresion, rate limiting, forwarding
a syslog, seal de integridad, y configuracion recomendada para
diferentes escenarios.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Storage y ubicaciones

### Objetivo

Entender los modos de almacenamiento del journal y sus ubicaciones.

### Paso 1.1: Donde se guardan los logs

```bash
docker compose exec debian-dev bash -c '
echo "=== Ubicaciones del journal ==="
echo ""
echo "--- Persistente (sobrevive reboots) ---"
echo "/var/log/journal/<machine-id>/"
echo ""
echo "--- Volatil (se pierde al reiniciar) ---"
echo "/run/log/journal/<machine-id>/"
echo ""
echo "--- Verificar cual esta activo ---"
if [[ -d /var/log/journal ]]; then
    echo "Almacenamiento: PERSISTENTE"
    ls -lh /var/log/journal/*/ 2>/dev/null | head -5
else
    echo "Almacenamiento: VOLATIL"
    ls -lh /run/log/journal/*/ 2>/dev/null | head -5
fi
echo ""
echo "--- Machine ID ---"
cat /etc/machine-id 2>/dev/null || echo "(no disponible)"
echo ""
echo "--- Archivos del journal ---"
echo "system.journal         journal activo del sistema"
echo "system@*.journal       journals rotados (archivados)"
echo "user-1000.journal      journal del usuario UID 1000"
'
```

### Paso 1.2: Modos de Storage=

```bash
docker compose exec debian-dev bash -c '
echo "=== Storage= en journald.conf ==="
echo ""
echo "| Valor      | Comportamiento                            |"
echo "|------------|-------------------------------------------|"
echo "| volatile   | Solo en /run/log/journal/ (RAM)           |"
echo "| persistent | En /var/log/journal/ (disco). Crea el dir |"
echo "| auto       | En /var/log/journal/ SI existe, sino /run |"
echo "| none       | No almacenar (logs se descartan)          |"
echo ""
echo "DEFAULT: auto"
echo ""
echo "--- Verificar configuracion actual ---"
grep -i "Storage" /etc/systemd/journald.conf 2>/dev/null || \
    echo "Storage no definido (default: auto)"
echo ""
echo "--- Cuando usar cada modo ---"
echo "volatile:   contenedores efimeros, testing"
echo "persistent: servidores de produccion, diagnostico de boot"
echo "auto:       el default, funciona si /var/log/journal/ existe"
echo "none:       sistemas que no necesitan logs"
'
```

### Paso 1.3: Habilitar persistencia

```bash
docker compose exec debian-dev bash -c '
echo "=== Habilitar persistencia ==="
echo ""
echo "Si Storage=auto y /var/log/journal/ no existe:"
echo ""
echo "  mkdir -p /var/log/journal"
echo "  systemd-tmpfiles --create --prefix /var/log/journal"
echo "  systemctl restart systemd-journald"
echo ""
echo "O cambiar a Storage=persistent en journald.conf:"
echo "  [Journal]"
echo "  Storage=persistent"
echo ""
echo "--- Diferencia por distribucion ---"
echo "Debian/Ubuntu: auto, /var/log/journal/ generalmente existe → persistente"
echo "RHEL 7:        auto, /var/log/journal/ no existia → volatil"
echo "RHEL 8+/Fedora: persistent por defecto → persistente"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== Storage en AlmaLinux ==="
grep -i "Storage" /etc/systemd/journald.conf 2>/dev/null || \
    echo "Storage no definido"
echo ""
if [[ -d /var/log/journal ]]; then
    echo "Almacenamiento: PERSISTENTE"
else
    echo "Almacenamiento: VOLATIL"
fi
'
```

---

## Parte 2 — Limites y rotacion

### Objetivo

Configurar limites de tamano y entender la rotacion del journal.

### Paso 2.1: Limites de tamano

```bash
docker compose exec debian-dev bash -c '
echo "=== Limites de tamano ==="
echo ""
echo "--- Almacenamiento persistente ---"
echo "SystemMaxUse=500M       tamano maximo total"
echo "                        default: 10% del FS, max 4G"
echo "SystemKeepFree=1G       espacio libre minimo en disco"
echo "                        default: 15% del FS"
echo "SystemMaxFileSize=100M  tamano max de un archivo"
echo "                        default: 1/8 de SystemMaxUse"
echo "SystemMaxFiles=100      numero max de archivos archivados"
echo ""
echo "--- Almacenamiento volatil (RAM) ---"
echo "RuntimeMaxUse=50M       tamano maximo en RAM"
echo "                        default: 10% de RAM, max 4G"
echo "RuntimeKeepFree=100M    RAM libre minima"
echo "                        default: 15% de RAM"
echo "RuntimeMaxFileSize=10M  tamano max por archivo"
echo ""
echo "--- Ver uso actual ---"
journalctl --disk-usage 2>&1 || \
    echo "(journal no disponible)"
'
```

### Paso 2.2: Rotacion

```bash
docker compose exec debian-dev bash -c '
echo "=== Rotacion del journal ==="
echo ""
echo "El journal rota automaticamente cuando:"
echo "  1. Un archivo alcanza SystemMaxFileSize"
echo "  2. El total excede SystemMaxUse"
echo "  3. El espacio libre cae por debajo de SystemKeepFree"
echo ""
echo "Los archivos rotados:"
echo "  system@XXXX.journal (XXXX = secuencia)"
echo "  Se eliminan los mas antiguos primero"
echo ""
echo "--- Forzar rotacion ---"
echo "journalctl --rotate"
echo "  Cierra journal activo, abre uno nuevo"
echo ""
echo "--- Rotar + limpiar ---"
echo "journalctl --rotate --vacuum-size=100M"
echo "journalctl --rotate --vacuum-time=30d"
echo ""
echo "--- Limpieza manual ---"
echo "journalctl --vacuum-size=200M    mantener max 200M"
echo "journalctl --vacuum-time=7d      eliminar mas de 7 dias"
echo "journalctl --vacuum-files=5      mantener 5 archivos"
'
```

### Paso 2.3: Compresion

```bash
docker compose exec debian-dev bash -c '
echo "=== Compresion ==="
echo ""
echo "--- Configuracion ---"
echo "[Journal]"
echo "Compress=yes       comprimir journals (default: si)"
echo "                   reduce significativamente el espacio"
echo "                   overhead de CPU minimo"
echo ""
echo "Compress=512       solo comprimir entradas > 512 bytes"
echo ""
echo "--- Verificar ---"
grep -i "Compress" /etc/systemd/journald.conf 2>/dev/null || \
    echo "Compress no definido (default: yes)"
echo ""
echo "--- MaxRetentionSec ---"
echo "MaxRetentionSec=3month    retener logs por 3 meses maximo"
echo "                          se combinan con los limites de tamano"
'
```

---

## Parte 3 — Rate limiting y configuracion

### Objetivo

Configurar rate limiting, forwarding, y ver la configuracion
recomendada para servidores.

### Paso 3.1: Rate limiting

```bash
docker compose exec debian-dev bash -c '
echo "=== Rate limiting ==="
echo ""
echo "[Journal]"
echo "RateLimitIntervalSec=30s    ventana de tiempo"
echo "RateLimitBurst=10000        max mensajes por ventana por unidad"
echo ""
echo "Si se excede, journald suprime mensajes y registra:"
echo "  \"Suppressed N messages from unit.service\""
echo ""
echo "--- Deshabilitar (solo para debug) ---"
echo "RateLimitIntervalSec=0"
echo "NO recomendado en produccion"
echo ""
echo "La solucion correcta es arreglar el servicio"
echo "que genera demasiados logs, no deshabilitar el rate limit"
echo ""
echo "--- Verificar ---"
grep -i "RateLimit" /etc/systemd/journald.conf 2>/dev/null || \
    echo "RateLimit no definido (defaults activos)"
'
```

### Paso 3.2: Forwarding y seal

```bash
docker compose exec debian-dev bash -c '
echo "=== Forwarding ==="
echo ""
echo "[Journal]"
echo "ForwardToSyslog=yes      reenviar a rsyslog (default: yes)"
echo "ForwardToKMsg=no         reenviar al kernel log"
echo "ForwardToConsole=no      reenviar a la consola"
echo "ForwardToWall=yes        mensajes de emergencia a todos los terminales"
echo ""
echo "--- Verificar ---"
grep -i "ForwardTo" /etc/systemd/journald.conf 2>/dev/null || \
    echo "ForwardTo no definido (defaults activos)"
echo ""
echo "=== Seal (integridad) ==="
echo ""
echo "[Journal]"
echo "Seal=yes    Forward Secure Sealing"
echo "            Usa criptografia para detectar modificaciones"
echo "            Util para auditoria y forense"
echo ""
echo "Verificar integridad:"
echo "  journalctl --verify"
echo "  PASS: /var/log/journal/.../system.journal"
'
```

### Paso 3.3: Permisos de acceso

```bash
docker compose exec debian-dev bash -c '
echo "=== Permisos de acceso ==="
echo ""
echo "Los logs del sistema solo son legibles por:"
echo "  root"
echo "  miembros del grupo systemd-journal"
echo ""
echo "--- Verificar ---"
ls -la /var/log/journal/ 2>/dev/null || \
    ls -la /run/log/journal/ 2>/dev/null || \
    echo "(directorio de journal no encontrado)"
echo ""
echo "--- Dar acceso a un usuario ---"
echo "usermod -aG systemd-journal username"
echo "  Despues de re-login, puede usar journalctl sin sudo"
echo ""
echo "--- Logs de usuario ---"
echo "journalctl --user"
echo "  Muestra logs de la sesion del usuario actual"
echo ""
echo "--- Grupo systemd-journal ---"
grep "systemd-journal" /etc/group 2>/dev/null || \
    echo "Grupo systemd-journal no encontrado"
'
```

### Paso 3.4: Configuracion recomendada

```bash
docker compose exec debian-dev bash -c '
echo "=== Configuracion recomendada para servidores ==="
echo ""
echo "Archivo: /etc/systemd/journald.conf.d/server.conf"
echo ""
cat << '\''EOF'\''
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
EOF
echo ""
echo "Despues de cambiar la configuracion:"
echo "  systemctl restart systemd-journald"
echo ""
echo "--- Verificar la configuracion efectiva ---"
echo "systemd-analyze cat-config systemd/journald.conf"
echo "  Muestra la configuracion merged (archivo + drop-ins)"
echo ""
echo "--- Leer configuracion actual ---"
cat /etc/systemd/journald.conf 2>/dev/null | \
    grep -v "^#" | grep -v "^$" || \
    echo "(solo defaults, sin configuracion explicita)"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `Storage=auto` (default) usa `/var/log/journal/` si existe,
   sino `/run/log/journal/` (volatil). `Storage=persistent`
   crea el directorio si no existe.

2. Los limites `SystemMaxUse=` y `RuntimeMaxUse=` controlan el
   tamano total del journal. Defaults: 10% del FS/RAM, max 4G.
   Siempre configurar en produccion.

3. La rotacion es automatica al alcanzar limites. `--rotate`
   fuerza rotacion. `--vacuum-*` limpia journals antiguos
   por tamano, tiempo o numero de archivos.

4. `Compress=yes` (default) reduce el espacio con overhead de
   CPU minimo. `MaxRetentionSec=` limita la retencion temporal.

5. `RateLimitBurst=` previene que un servicio problematico
   llene el journal. La solucion es arreglar el servicio,
   no deshabilitar el rate limit.

6. Solo root y miembros de `systemd-journal` pueden leer los
   logs del sistema. `usermod -aG systemd-journal user` da
   acceso sin sudo.
