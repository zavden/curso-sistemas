# T03 — Integración con rsyslog

> **Objetivo:** Entender cómo journald y rsyslog coexisten, qué papel cumple cada uno, y cómo configurarlos para distintos escenarios (solo journal, solo rsyslog, o ambos).

## Erratas corregidas respecto a `README.max.md`

| # | Línea | Error | Corrección |
|---|-------|-------|------------|
| 1 | 78 | "Maturidad" (no es palabra estándar en español) | "Madurez" |
| 2 | 296 | Texto en chino "TLS加密:" | "Cifrado TLS:" |

---

## Dos sistemas de logging que coexisten

En la mayoría de distribuciones Linux, **journald y rsyslog trabajan juntos**:

```
┌──────────────────────────────────────────────────────────────────┐
│                         Aplicaciones                             │
│                    (stdout, stderr, syslog())                    │
└─────────────────────────────┬────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              │                               │
              ▼                               ▼
     ┌──────────────────┐          ┌──────────────────┐
     │   openlog()      │          │   /dev/log       │
     │   (libc)         │          │   (socket)       │
     └────────┬─────────┘          └────────┬─────────┘
              │                               │
              ▼                               │
     systemd-journald ◄───────────────────────┘
              │
              ├──► /var/log/journal/  (binario, indexado)
              │       ├── system.journal
              │       └── system@*.journal
              │
              └──► ForwardToSyslog=yes
                      │
                      ▼
                 rsyslogd
              (daemon tradicional)
                      │
                      ├──► /var/log/syslog    (texto plano)
                      ├──► /var/log/auth.log  (texto plano)
                      ├──► /var/log/kern.log  (texto plano)
                      └──► Reenvío remoto TCP/UDP
                              │
                              ▼
                        Servidor central
                       (LogAnalyzer, ELK,
                        Splunk, etc.)
```

- **journald** siempre está presente (es parte de systemd). Captura todo: stdout, stderr, syslog(), kernel.
- **rsyslog** es un paquete separado (puede no estar en contenedores). Recibe logs reenviados y los escribe en texto plano.

```bash
# Verificar que ambos están activos:
systemctl is-active systemd-journald rsyslog
```

---

## Por qué coexisten: ventajas de cada uno

### Journal — Ventajas

| Característica | Descripción |
|----------------|-------------|
| **Estructurado** | Cada log es un conjunto de campos clave-valor (PID, UID, PRIORITY, etc.) |
| **Indexado** | Búsquedas extremadamente rápidas por cualquier campo |
| **Centralizado** | Un solo lugar para kernel, servicios, syslog, stdout/stderr |
| **Integridad** | `journalctl --verify` verifica integridad; Seal (FSS) protege criptográficamente |
| **Rotación automática** | Gestiona el espacio sin necesidad de logrotate |
| **Boot logs** | Puede consultar logs de boots anteriores con `-b` |

### rsyslog — Ventajas

| Característica | Descripción |
|----------------|-------------|
| **Texto plano** | Legible con `cat`, `grep`, `tail` sin herramientas especiales |
| **Reenvío remoto** | TCP/UDP/RELP a servidores centralizados |
| **Filtros complejos** | RainerScript, property-based filters, condiciones dinámicas |
| **Compatibilidad** | Herramientas legacy esperan `/var/log/syslog` |
| **Integración externa** | Elasticsearch, Kafka, database outputs, archiving |
| **Madurez** | 20+ años de uso en producción |

### La práctica habitual

```bash
# journald captura todo y lo almacena en formato binario
# rsyslog recibe los logs reenviados y los escribe en texto plano

# El admin usa:
# - journalctl para consultas rápidas, filtros por servicio, debug
# - /var/log/syslog para scripts legacy, herramientas de monitoreo
# - rsyslog para reenviar a un servidor central de logs
```

---

## Cómo se comunican journald y rsyslog

### journald → rsyslog (ForwardToSyslog)

```ini
# /etc/systemd/journald.conf
[Journal]
ForwardToSyslog=yes      # default en la mayoría de distros
```

journald envía los logs al socket `/dev/log`. rsyslog escucha en ese socket y los procesa.

```bash
# Verificar forwarding activo:
grep -i "ForwardToSyslog" /etc/systemd/journald.conf
# Si está comentado o ausente, el default es "yes" en la mayoría

# Verificar qué está escuchando en /dev/log:
ss -x | grep /dev/log
```

### Dos enfoques de rsyslog para leer logs

```
┌─────────────────────────────────────────────────────────────┐
│ Debian/Ubuntu — imuxsock (socket tradicional)               │
│                                                             │
│ journald ──► ForwardToSyslog=yes ──► /dev/log ──► rsyslog  │
│             (socket)                     (imuxsock)          │
│                                                             │
│ Depende de ForwardToSyslog=yes en journald.conf             │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ RHEL/CentOS 7+ — imjournal (lectura directa del journal)   │
│                                                             │
│ journald ──► /var/log/journal/ ◄───────── rsyslog          │
│             (binario)          (imjournal, no usa socket)   │
│                                                             │
│ NO depende de ForwardToSyslog                               │
│ Lee todos los campos estructurados del journal              │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Configuración RHEL (/etc/rsyslog.conf):
module(load="imjournal"
    StateFile="imjournal.state"
    ratelimit.interval="600"
    ratelimit.burst="20000")

# Debian/Ubuntu (/etc/rsyslog.conf):
module(load="imuxsock")    # escucha en /dev/log
module(load="imklog")      # logs del kernel
# Depende de ForwardToSyslog=yes en journald.conf
```

**Diferencia clave:**
- **Debian:** journald → ForwardToSyslog → `/dev/log` → rsyslog (rsyslog depende del forwarding)
- **RHEL:** rsyslog ← imjournal ← `/var/log/journal/` (rsyslog lee directamente, ForwardToSyslog no es estrictamente necesario)

---

## Archivos de log de rsyslog

```bash
# Debian/Ubuntu:
/var/log/syslog           # todos los logs (excepto auth)
/var/log/auth.log         # autenticación (login, sudo, ssh, pam)
/var/log/kern.log         # mensajes del kernel
/var/log/mail.log         # sistema de correo
/var/log/daemon.log       # daemons (servicios sin unidad propia)
/var/log/user.log         # aplicaciones de usuario

# RHEL/Fedora:
/var/log/messages         # todos los logs (equivalente a syslog)
/var/log/secure           # autenticación (equivalente a auth.log)
/var/log/maillog          # correo
/var/log/cron             # tareas programadas
/var/log/boot.log         # mensajes de boot
```

### Reglas de enrutamiento

Las reglas en rsyslog determinan qué mensajes van a qué archivo. El formato es `facility.priority   destino`:

```bash
# Ver las reglas (Debian):
cat /etc/rsyslog.d/50-default.conf

# Ejemplo de reglas típicas:
# auth,authpriv.*          /var/log/auth.log       # todo de auth
# *.*;auth,authpriv.none   /var/log/syslog         # todo excepto auth
# kern.*                   /var/log/kern.log        # todo del kernel
# mail.*                   /var/log/mail.log        # todo de mail

# RHEL (todo en /etc/rsyslog.conf):
# authpriv.*               /var/log/secure
# *.info;mail.none;...     /var/log/messages
```

---

## journalctl vs archivos de texto

| Capacidad | journalctl | Archivos de texto |
|-----------|------------|-------------------|
| Filtrar por servicio | `journalctl -u nginx` (exacto) | `grep nginx /var/log/syslog` (impreciso) |
| Filtrar por PID | `journalctl _PID=1234` (exacto) | `grep "1234"` (coincide con timestamps) |
| Filtrar por boot | `journalctl -b -1` (trivial) | Buscar por timestamp (manual) |
| Filtrar por UID | `journalctl _UID=1000` (directo) | Imposible directamente |
| Filtrar por prioridad | `journalctl -p err` (exacto) | `grep "error"` (falsos positivos) |
| Formato JSON | `journalctl -o json` (nativo) | Parsear con awk/sed (frágil) |
| Ver todos los campos | `journalctl -o verbose` | No disponible |
| Kernel + servicio | `journalctl -k -u docker` | `dmesg + grep docker` (desconectados) |
| Velocidad de búsqueda | Indexado (rápido) | Secuencial (lento) |
| Rotación | Automática integrada | logrotate por separado |

### Lo que archivos de texto hacen mejor

```bash
# 1. Leer logs de un sistema que no puede arrancar:
# Boot desde USB/CD → mount del disco
cat /mnt/broken/var/log/syslog
# journalctl puede hacerlo con --directory pero requiere formato binario intacto:
journalctl --directory=/mnt/broken/var/log/journal/ -b -1

# 2. Integración con pipelines legacy:
# Logstash/Fluentd/Filebeat esperan texto plano
# (aunque ya existen plugins para leer el journal)

# 3. Rotación granular con logrotate:
# Políticas complejas de retención y compresión por servicio

# 4. Scripts heredados:
# grep/awk sobre /var/log/syslog sin cambios
```

---

## Escenarios de configuración

### Escenario 1: Solo journald (contenedores, embedded)

```bash
# Sin rsyslog — menos procesos, menos I/O
sudo systemctl stop rsyslog
sudo systemctl disable rsyslog

# Configuración del journal:
# /etc/systemd/journald.conf
[Journal]
Storage=persistent          # o volatile en contenedores efímeros
SystemMaxUse=500M
Compress=yes

# Ventajas:
# - Menos superficie de ataque
# - Sin duplicación de logs
# - Búsquedas rápidas con journalctl
#
# Desventaja:
# - Sin reenvío remoto nativo robusto
#   → Solución: systemd-journal-upload
# - Scripts legacy que dependen de /var/log/syslog no funcionan
```

### Escenario 2: Solo rsyslog (sistemas legacy)

```bash
# journald mínimo, todo va por rsyslog:
# /etc/systemd/journald.conf
[Journal]
Storage=volatile             # solo RAM como buffer temporal
ForwardToSyslog=yes          # reenviar todo a rsyslog
RuntimeMaxUse=50M            # limitar uso de RAM

# journald actúa como buffer RAM temporal
# rsyslog persiste todo en texto plano
#
# Ventajas:
# - Control total con rsyslog y logrotate
# - Reenvío remoto maduro (TCP, RELP)
# - Compatibilidad con herramientas legacy
#
# Desventaja:
# - Pierdes capacidades de búsqueda estructurada
# - journalctl -b -1 no funciona (no hay persistencia)
```

### Escenario 3: Ambos (producción típica)

```bash
# La configuración más común en servidores:
# /etc/systemd/journald.conf
[Journal]
Storage=persistent
ForwardToSyslog=yes
SystemMaxUse=1G
SystemMaxFileSize=100M
Compress=yes
RateLimitIntervalSec=30s
RateLimitBurst=10000

# journald: indexación, búsqueda rápida, CLI moderna
# rsyslog: texto plano, reenvío remoto, ELK/Splunk
#
# logrotate se encarga de rotar los archivos de texto de rsyslog:
# cat /etc/logrotate.d/rsyslog
```

```
Flujo en escenario "ambos":

  Servicio → journald → /var/log/journal/ (binario, indexado)
                  │
                  └→ rsyslog → /var/log/syslog (texto plano)
                          │
                          └→ logserver:514 (remoto)
```

---

## Reenvío remoto de logs

### Con rsyslog (tradicional, maduro)

```bash
# SERVIDOR RECEPTOR — /etc/rsyslog.conf:
module(load="imtcp")
input(type="imtcp" port="514")

# CLIENTE — /etc/rsyslog.d/remote.conf:
# Reenvío tradicional:
# @  = UDP (rápido, sin garantía de entrega)
# @@ = TCP (confiable)
*.* @@logserver.example.com:514

# Cifrado TLS:
# module(load="imtcp" streamdriver.mode="tls" streamdriver.authmode="x509/name")
# input(type="imtcp" port="514")

# Protocolo RELP (entrega garantizada):
# module(load="omrelp")
# action(type="omrelp" target="logserver" port="2514")
```

### Con systemd-journal-upload (sin rsyslog)

```bash
# journald puede reenviar directamente a otro journal:
# systemd-journal-upload → systemd-journal-remote

# SERVIDOR RECEPTOR:
sudo systemctl enable systemd-journal-remote.socket
sudo systemctl start systemd-journal-remote.socket
# Escucha en http://*:19532/

# CLIENTE — /etc/systemd/journal-upload.conf:
[Upload]
URL=http://logserver.example.com:19532
# ServerKeyFile=...  (para conexión TLS)

sudo systemctl enable systemd-journal-upload
sudo systemctl start systemd-journal-upload

# Ventaja: preserva todos los campos estructurados del journal
# Desventaja: solo reenvía a otro journald (no a syslog tradicional)
```

### Comparación de reenvío remoto

| Aspecto | rsyslog | journal-upload |
|---------|---------|----------------|
| Madurez | Décadas de uso | Relativamente nuevo |
| Protocolo | TCP/UDP/RELP | HTTPS |
| Formato preservado | Texto syslog | Journal nativo (campos estructurados) |
| Filtros en destino | RainerScript (muy potente) | Limitados |
| Ecosistema de destino | Amplio (ELK, Splunk, etc.) | Solo otro journald |

---

## Comparación: Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/CentOS/Fedora |
|---------|---------------|-------------------|
| rsyslog instalado | Sí (default) | Sí (default) |
| Módulo de entrada | `imuxsock` (socket) | `imjournal` (directo) |
| Depende de ForwardToSyslog | Sí | No (lee journal directamente) |
| Logs principales | `/var/log/syslog` | `/var/log/messages` |
| Auth logs | `/var/log/auth.log` | `/var/log/secure` |
| Journal persistence | `auto` → persistente (directorio existe) | `persistent` por defecto |

---

## Diagnóstico de problemas comunes

```bash
# PROBLEMA: Logs aparecen en journal pero NO en /var/log/syslog

# 1. Verificar que rsyslog está corriendo:
systemctl status rsyslog

# 2. Verificar forwarding activo:
grep ForwardToSyslog /etc/systemd/journald.conf

# 3. Ver si rsyslog recibe en el socket:
ss -x | grep /dev/log

# 4. Reiniciar ambos:
sudo systemctl restart systemd-journald
sudo systemctl restart rsyslog

# 5. Probar generando un log:
logger "test from rsyslog integration"
journalctl -n 1 --no-pager
grep "test from rsyslog" /var/log/syslog
```

```bash
# PROBLEMA: rsyslog muestra "imjournal: No sd_journal_open()" (RHEL)
# → Verificar que /var/log/journal/ existe y tiene permisos correctos
# → El usuario rsyslog necesita estar en el grupo systemd-journal

# PROBLEMA: Logs duplicados en /var/log/syslog
# → Puede pasar si ForwardToSyslog=yes Y rsyslog usa imjournal
# → Solución: en RHEL, ForwardToSyslog=no si se usa imjournal
```

---

## Quick reference

```
┌────────────────────────────────────────────────────────────────┐
│                     FLUJO DE LOGS                              │
│                                                                │
│  Aplicaciones                                                  │
│       │                                                        │
│       ├──► systemd-journald ──► /var/log/journal/              │
│       │       │                                                │
│       │       └──► ForwardToSyslog=yes ──► /dev/log ──► rsyslog│
│       │                                              │         │
│       │                                              ▼         │
│       │                                    /var/log/syslog     │
│       │                                    /var/log/auth.log   │
│       │                                    Reenvío remoto      │
│       │                                                        │
│  kernel (dmesg)                                                │
│       ├──► systemd-journald (via /dev/kmsg)                    │
│       └──► rsyslog (via imklog) ──► /var/log/kern.log          │
└────────────────────────────────────────────────────────────────┘
```

| Comando | Uso |
|---------|-----|
| `systemctl is-active systemd-journald rsyslog` | ¿Ambos activos? |
| `grep ForwardToSyslog /etc/systemd/journald.conf` | ¿Forwarding activo? |
| `grep -E "imuxsock\|imjournal" /etc/rsyslog.conf` | ¿Qué módulo usa rsyslog? |
| `journalctl -u nginx -p err` | Errores de nginx (journal) |
| `grep nginx /var/log/syslog` | Modo legacy (texto) |
| `logger "test message"` | Generar log de prueba |
| `cat /etc/rsyslog.d/50-default.conf` | Reglas de rsyslog (Debian) |

---

## Lab — Integración con rsyslog

### Parte 1 — Coexistencia y flujo

#### Paso 1.1: Dos sistemas de logging

```bash
docker compose exec debian-dev bash -c '
echo "=== Dos sistemas de logging ==="
echo ""
echo "En la mayoria de distribuciones coexisten:"
echo ""
echo "  Aplicaciones / Servicios"
echo "          |"
echo "          v"
echo "  systemd-journald        <- captura TODO"
echo "          |"
echo "          |--- /var/log/journal/    (binario, indexado)"
echo "          |"
echo "          +--- ForwardToSyslog=yes"
echo "                  |"
echo "                  v"
echo "             rsyslog              <- recibe logs reenviados"
echo "                  |"
echo "                  +--- /var/log/syslog, auth.log, etc. (texto plano)"
echo ""
echo "--- journald ---"
echo "  Siempre presente (es parte de systemd)"
echo "  Captura: stdout, stderr, syslog(), kernel"
echo "  Almacena: formato binario indexado"
echo "  Consulta: journalctl"
echo ""
echo "--- rsyslog ---"
echo "  Instalado por defecto en la mayoria de distros"
echo "  Recibe logs via socket o desde el journal"
echo "  Almacena: archivos de texto plano"
echo "  Consulta: cat, grep, tail, awk"
'
```

#### Paso 1.2: ForwardToSyslog

```bash
docker compose exec debian-dev bash -c '
echo "=== ForwardToSyslog ==="
echo ""
echo "journald decide si reenvia logs a rsyslog:"
echo ""
echo "[Journal]"
echo "ForwardToSyslog=yes    default en la mayoria de distros"
echo ""
echo "--- Mecanismo ---"
echo "journald envia los logs a /dev/log (socket de syslog)"
echo "rsyslog escucha en ese socket y los procesa"
echo ""
echo "--- Verificar en Debian ---"
grep -i "ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null || \
    echo "ForwardToSyslog no definido (default: yes)"
echo ""
echo "--- Otros forwarding ---"
echo "ForwardToSyslog=yes     a rsyslog (default: yes)"
echo "ForwardToKMsg=no        al kernel log"
echo "ForwardToConsole=no     a la consola"
echo "ForwardToWall=yes       emergencias a todos los terminales"
echo ""
echo "--- Verificar configuracion completa ---"
grep -i "ForwardTo" /etc/systemd/journald.conf 2>/dev/null || \
    echo "(solo defaults activos)"
'
```

#### Paso 1.3: Módulos de entrada de rsyslog

```bash
docker compose exec debian-dev bash -c '
echo "=== Modulos de entrada de rsyslog ==="
echo ""
echo "rsyslog tiene dos formas de recibir logs del journal:"
echo ""
echo "| Modulo    | Como funciona                | Distribucion     |"
echo "|-----------|------------------------------|------------------|"
echo "| imuxsock  | Escucha en /dev/log (socket) | Debian/Ubuntu    |"
echo "| imjournal | Lee directo de journal       | RHEL/Fedora      |"
echo ""
echo "--- Verificar en Debian ---"
if [[ -f /etc/rsyslog.conf ]]; then
    echo "rsyslog.conf:"
    grep -E "^(module|input)" /etc/rsyslog.conf 2>/dev/null | head -5 || \
        echo "  (sin modulos explicitamente definidos)"
    echo ""
    echo "Archivos de configuracion:"
    ls /etc/rsyslog.d/ 2>/dev/null || echo "  (directorio no encontrado)"
else
    echo "rsyslog no instalado en este contenedor"
fi
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== Modulos de rsyslog en AlmaLinux ==="
echo ""
if [[ -f /etc/rsyslog.conf ]]; then
    echo "rsyslog.conf:"
    grep "imjournal\|imuxsock" /etc/rsyslog.conf 2>/dev/null | head -5 || \
        echo "  (buscar modulos manualmente)"
    echo ""
    echo "Archivos de configuracion:"
    ls /etc/rsyslog.d/ 2>/dev/null || echo "  (directorio no encontrado)"
else
    echo "rsyslog no instalado en este contenedor"
fi
'
```

#### Paso 1.4: Archivos de log por distribución

```bash
docker compose exec debian-dev bash -c '
echo "=== Archivos de log que mantiene rsyslog ==="
echo ""
echo "--- Debian/Ubuntu ---"
echo "| Archivo                | Contenido                          |"
echo "|------------------------|------------------------------------|"
echo "| /var/log/syslog        | todos los logs (excepto auth)      |"
echo "| /var/log/auth.log      | autenticacion (login, sudo, ssh)   |"
echo "| /var/log/kern.log      | mensajes del kernel                |"
echo "| /var/log/mail.log      | correo                             |"
echo "| /var/log/daemon.log    | daemons                            |"
echo "| /var/log/user.log      | aplicaciones de usuario            |"
echo ""
echo "--- RHEL/Fedora ---"
echo "| Archivo                | Contenido                          |"
echo "|------------------------|------------------------------------|"
echo "| /var/log/messages      | todos los logs (equivale a syslog) |"
echo "| /var/log/secure        | autenticacion (equivale a auth.log)|"
echo "| /var/log/maillog       | correo                             |"
echo "| /var/log/cron          | cron                               |"
echo "| /var/log/boot.log      | mensajes de boot                   |"
echo ""
echo "--- Verificar archivos existentes ---"
ls -lh /var/log/syslog /var/log/auth.log /var/log/kern.log 2>/dev/null || \
    echo "  (archivos no encontrados — rsyslog puede no estar activo)"
echo ""
echo "Verificar /var/log/:"
ls /var/log/*.log /var/log/messages /var/log/secure 2>/dev/null | head -10 || \
    echo "  (sin archivos de log de rsyslog)"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== Archivos de log en AlmaLinux ==="
echo ""
ls -lh /var/log/messages /var/log/secure /var/log/cron 2>/dev/null || \
    echo "(archivos no encontrados — rsyslog puede no estar activo)"
echo ""
echo "Todo en /var/log/:"
ls /var/log/ 2>/dev/null | head -15
'
```

---

### Parte 2 — journalctl vs archivos de texto

#### Paso 2.1: Ventajas de journalctl

```bash
docker compose exec debian-dev bash -c '
echo "=== Ventajas de journalctl ==="
echo ""
echo "--- Filtro por servicio ---"
echo "  journalctl -u nginx            (exacto por unidad)"
echo "  vs: grep nginx /var/log/syslog (coincide con URLs, config...)"
echo ""
echo "--- Filtro por PID ---"
echo "  journalctl _PID=1234            (exacto)"
echo "  vs: grep 1234 /var/log/syslog   (coincide con timestamps, IPs...)"
echo ""
echo "--- Filtro por boot ---"
echo "  journalctl -b -1                  (trivial)"
echo "  vs: grep por timestamp en syslog  (manual, propenso a errores)"
echo ""
echo "--- Filtro por prioridad ---"
echo "  journalctl -p err                                     (exacto)"
echo "  vs: grep -E \"(error|err|crit)\" /var/log/syslog       (falsos positivos)"
echo ""
echo "--- Formato JSON para procesamiento ---"
echo "  journalctl -o json -u nginx | jq .MESSAGE"
echo "  vs: parsear texto plano con awk/sed (fragil)"
echo ""
echo "--- Busqueda indexada ---"
echo "  journalctl es rapido porque el journal esta indexado"
echo "  grep recorre todo el archivo secuencialmente"
'
```

#### Paso 2.2: Ventajas de archivos de texto

```bash
docker compose exec debian-dev bash -c '
echo "=== Ventajas de archivos de texto ==="
echo ""
echo "--- Legibilidad directa ---"
echo "  cat /var/log/syslog | tail -20"
echo "  No necesitas ningun comando especial"
echo ""
echo "--- Sistema que no arranca ---"
echo "  Montar disco desde USB de rescate:"
echo "  cat /mnt/broken/var/log/syslog"
echo "  (journalctl requiere que el formato binario este intacto)"
echo ""
echo "--- Herramientas externas ---"
echo "  Logstash, Fluentd, Splunk, etc."
echo "  Muchas esperan archivos de texto plano"
echo ""
echo "--- Rotacion granular ---"
echo "  logrotate permite politicas por servicio,"
echo "  compresion individual, scripts pre/post rotacion"
echo ""
echo "--- Reenvio remoto maduro ---"
echo "  rsyslog: @@ servidor:514 (TCP)"
echo "  RELP para entrega garantizada"
echo "  Protocolo probado por decadas"
'
```

#### Paso 2.3: Ver la configuración de rsyslog

```bash
docker compose exec debian-dev bash -c '
echo "=== Configuracion de rsyslog ==="
echo ""
echo "--- Archivo principal ---"
if [[ -f /etc/rsyslog.conf ]]; then
    echo "/etc/rsyslog.conf:"
    cat /etc/rsyslog.conf 2>/dev/null | grep -v "^#" | grep -v "^$" | head -20
    echo ""
    echo "--- Drop-ins ---"
    if [[ -d /etc/rsyslog.d ]]; then
        for f in /etc/rsyslog.d/*; do
            if [[ -f "$f" ]]; then
                echo "--- $f ---"
                cat "$f" | grep -v "^#" | grep -v "^$" | head -10
                echo ""
            fi
        done
    fi
else
    echo "rsyslog no instalado"
fi
echo ""
echo "--- logrotate para rsyslog ---"
if [[ -f /etc/logrotate.d/rsyslog ]]; then
    echo "/etc/logrotate.d/rsyslog:"
    cat /etc/logrotate.d/rsyslog 2>/dev/null | head -15
else
    echo "logrotate config no encontrado"
fi
'
```

---

### Parte 3 — Escenarios y reenvío

#### Paso 3.1: Solo journald (sin rsyslog)

```bash
docker compose exec debian-dev bash -c '
echo "=== Escenario: solo journald ==="
echo ""
echo "Cuando usar:"
echo "  Contenedores y sistemas minimalistas"
echo "  Cuando no se necesita reenvio remoto via rsyslog"
echo ""
echo "--- Configuracion ---"
echo "# Deshabilitar rsyslog:"
echo "systemctl stop rsyslog"
echo "systemctl disable rsyslog"
echo ""
echo "# /etc/systemd/journald.conf"
echo "[Journal]"
echo "Storage=persistent"
echo "SystemMaxUse=500M"
echo "Compress=yes"
echo ""
echo "--- Ventajas ---"
echo "  Menos procesos, menos I/O de disco"
echo "  Sin duplicacion de logs"
echo ""
echo "--- Desventajas ---"
echo "  Sin reenvio remoto nativo robusto"
echo "  Scripts que dependen de /var/log/syslog fallan"
'
```

#### Paso 3.2: Solo rsyslog (journal mínimo)

```bash
docker compose exec debian-dev bash -c '
echo "=== Escenario: solo rsyslog ==="
echo ""
echo "Cuando usar:"
echo "  Entornos con ecosistema rsyslog completo"
echo "  Migracion desde sistemas sin systemd"
echo ""
echo "--- Configuracion ---"
echo "# /etc/systemd/journald.conf"
echo "[Journal]"
echo "Storage=volatile           solo RAM como buffer"
echo "ForwardToSyslog=yes        reenviar todo a rsyslog"
echo "RuntimeMaxUse=50M          limitar uso de RAM"
echo ""
echo "--- Ventajas ---"
echo "  Control total con rsyslog y logrotate"
echo "  Reenvio remoto maduro (TCP, RELP)"
echo ""
echo "--- Desventajas ---"
echo "  Se pierde la busqueda indexada de journalctl"
echo "  journalctl -b -1 no funciona (no hay persistencia)"
'
```

#### Paso 3.3: Ambos (producción típica)

```bash
docker compose exec debian-dev bash -c '
echo "=== Escenario: ambos (produccion tipica) ==="
echo ""
echo "El escenario mas comun en servidores de produccion."
echo ""
echo "--- Configuracion ---"
echo "# /etc/systemd/journald.conf"
echo "[Journal]"
echo "Storage=persistent"
echo "ForwardToSyslog=yes"
echo "SystemMaxUse=1G"
echo "Compress=yes"
echo ""
echo "--- Flujo ---"
echo "  Servicio -> journald -> /var/log/journal/ (binario)"
echo "                  |"
echo "                  +-> rsyslog -> /var/log/syslog (texto)"
echo "                          |"
echo "                          +-> logserver:514 (remoto)"
echo ""
echo "Lo mejor de ambos mundos:"
echo "  journalctl para diagnostico rapido"
echo "  rsyslog para reenvio remoto y compatibilidad"
'
```

#### Paso 3.4: Reenvío remoto

```bash
docker compose exec debian-dev bash -c '
echo "=== Reenvio remoto ==="
echo ""
echo "--- Con rsyslog (maduro, recomendado) ---"
echo ""
echo "Servidor receptor (/etc/rsyslog.conf):"
echo "  module(load=\"imtcp\")"
echo "  input(type=\"imtcp\" port=\"514\")"
echo ""
echo "Cliente que envia (/etc/rsyslog.d/remote.conf):"
echo "  *.*  @@logserver.example.com:514"
echo "  @  = UDP (rapido, sin garantia)"
echo "  @@ = TCP (confiable)"
echo ""
echo "--- Con systemd-journal-upload (alternativa) ---"
echo ""
echo "Servidor receptor:"
echo "  systemctl enable systemd-journal-remote.socket"
echo ""
echo "Cliente (/etc/systemd/journal-upload.conf):"
echo "  [Upload]"
echo "  URL=http://logserver.example.com:19532"
echo ""
echo "--- Comparacion ---"
echo "| Aspecto    | rsyslog          | journal-upload      |"
echo "|------------|------------------|---------------------|"
echo "| Madurez    | Decadas          | Relativamente nuevo |"
echo "| Protocolo  | TCP/UDP/RELP     | HTTPS               |"
echo "| Formato    | Texto syslog     | Journal nativo      |"
echo "| Ecosistema | Amplio           | Solo otro journald  |"
'
```

#### Paso 3.5: Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian vs RHEL: integracion journal + rsyslog ==="
echo ""
echo "| Aspecto               | Debian/Ubuntu          | RHEL/Fedora            |"
echo "|-----------------------|------------------------|------------------------|"
echo "| rsyslog instalado     | Si (por defecto)       | Si (por defecto)       |"
echo "| Modulo de rsyslog     | imuxsock (socket)      | imjournal (directo)    |"
echo "| Logs principales      | /var/log/syslog        | /var/log/messages      |"
echo "| Logs de auth          | /var/log/auth.log      | /var/log/secure        |"
echo "| Journal persistente   | auto (dir existe)      | persistent (explicito) |"
echo "| ForwardToSyslog       | yes (default)          | yes (pero usa imjournal)|"
echo ""
echo "--- Diferencia clave ---"
echo ""
echo "Debian: journald -> ForwardToSyslog -> /dev/log -> rsyslog"
echo "  rsyslog depende del forwarding de journald"
echo ""
echo "RHEL:   rsyslog <- imjournal <- /var/log/journal/"
echo "  rsyslog lee directamente del journal"
echo "  ForwardToSyslog no es estrictamente necesario"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== Verificar en AlmaLinux ==="
echo ""
echo "journald:"
grep -i "Storage\|ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null || \
    echo "  (defaults)"
echo ""
echo "rsyslog:"
if [[ -f /etc/rsyslog.conf ]]; then
    grep "imjournal\|imuxsock" /etc/rsyslog.conf 2>/dev/null || \
        echo "  (modulos no encontrados)"
else
    echo "  (rsyslog no instalado)"
fi
'
```

---

## Ejercicios

### Ejercicio 1 — Verificar la coexistencia

¿Están ambos sistemas de logging activos? ¿Cómo se comunican?

```bash
# 1. ¿Están ambos activos?
systemctl is-active systemd-journald rsyslog

# 2. ¿ForwardToSyslog está activo (o es default)?
grep -i "ForwardToSyslog" /etc/systemd/journald.conf || \
    echo "(no definido — default: yes en la mayoría)"

# 3. ¿Qué módulo usa rsyslog para recibir logs?
grep -E "imuxsock|imjournal|imklog" /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null
```

<details><summary>Predicción</summary>

- `systemd-journald` mostrará `active` (siempre está presente como parte de systemd).
- `rsyslog` mostrará `active` si está instalado (default en Debian y RHEL), o `inactive`/error si no está instalado (contenedores minimalistas).
- Si `ForwardToSyslog` no aparece en `journald.conf`, el default es `yes`.
- En **Debian** verás `imuxsock` (escucha en `/dev/log`). En **RHEL** verás `imjournal` (lee directo del journal).

</details>

### Ejercicio 2 — Comparar la misma información

Genera un log de prueba y compara cómo aparece en journal vs texto plano.

```bash
# 1. Generar un log de prueba con logger:
logger "ejercicio-integracion: test de comparación"

# 2. Ver en journal:
journalctl -n 1 --no-pager

# 3. Ver en texto plano:
grep "ejercicio-integracion" /var/log/syslog 2>/dev/null || \
grep "ejercicio-integracion" /var/log/messages 2>/dev/null

# 4. Ver los campos estructurados del journal:
journalctl -n 1 -o verbose --no-pager
```

<details><summary>Predicción</summary>

- `journalctl -n 1` mostrará una línea con timestamp, hostname, identificador y mensaje.
- En `/var/log/syslog` (Debian) o `/var/log/messages` (RHEL) verás una línea similar pero sin campos estructurados.
- Con `-o verbose` verás todos los campos del journal: `_PID`, `_UID`, `_GID`, `_COMM`, `SYSLOG_IDENTIFIER`, `PRIORITY`, `_TRANSPORT=syslog`, etc.
- El texto plano **no tiene** estos campos — es imposible filtrar por PID o UID con precisión en texto plano.

</details>

### Ejercicio 3 — Inventario de archivos de log

¿Qué archivos de log mantiene rsyslog y cuánto espacio usan?

```bash
# 1. Listar archivos de log de rsyslog:
ls -lh /var/log/syslog /var/log/auth.log /var/log/kern.log 2>/dev/null
ls -lh /var/log/messages /var/log/secure 2>/dev/null

# 2. Espacio total de /var/log/:
du -sh /var/log/

# 3. Líneas por archivo:
wc -l /var/log/syslog /var/log/auth.log 2>/dev/null || \
wc -l /var/log/messages /var/log/secure 2>/dev/null

# 4. Configuración de rotación:
cat /etc/logrotate.d/rsyslog 2>/dev/null
```

<details><summary>Predicción</summary>

- En **Debian** verás `syslog`, `auth.log`, `kern.log`, `mail.log`, `daemon.log`.
- En **RHEL** verás `messages`, `secure`, `maillog`, `cron`, `boot.log`.
- El tamaño depende de la actividad del sistema. En un sistema poco activo, `syslog`/`messages` tendrá decenas de KB; en un servidor de producción, puede tener cientos de MB.
- `logrotate` mostrará la configuración de rotación (diaria, semanal, cuántas copias retener).

</details>

### Ejercicio 4 — Identificar el módulo de rsyslog

¿Qué mecanismo usa tu sistema para que rsyslog reciba logs del journal?

```bash
# 1. Buscar en la configuración principal:
grep -E "imuxsock|imjournal" /etc/rsyslog.conf 2>/dev/null

# 2. Buscar en drop-ins:
grep -rE "imuxsock|imjournal" /etc/rsyslog.d/ 2>/dev/null

# 3. Verificar si ForwardToSyslog es relevante:
# Si usa imjournal → ForwardToSyslog no importa (lee directo)
# Si usa imuxsock → ForwardToSyslog=yes es necesario
```

<details><summary>Predicción</summary>

- En **Debian/Ubuntu** aparecerá `imuxsock` — rsyslog escucha en el socket `/dev/log` y depende de `ForwardToSyslog=yes`.
- En **RHEL/Fedora** aparecerá `imjournal` — rsyslog lee directamente de `/var/log/journal/` sin depender del forwarding.
- Si ves ambos módulos, el sistema puede estar configurado con redundancia (imjournal como fuente principal + imuxsock como fallback).

</details>

### Ejercicio 5 — Filtro preciso vs impreciso

Demuestra la diferencia de precisión entre `journalctl` y `grep` en texto plano.

```bash
# 1. Filtrar logs de sshd con journalctl (preciso):
journalctl -u sshd -n 5 --no-pager 2>/dev/null

# 2. Filtrar logs de sshd con grep (impreciso):
grep sshd /var/log/auth.log 2>/dev/null | tail -5 || \
grep sshd /var/log/secure 2>/dev/null | tail -5

# 3. Buscar un PID con journalctl (exacto):
journalctl _PID=1 -n 3 --no-pager

# 4. Buscar el mismo PID con grep (falsos positivos):
grep "1" /var/log/syslog 2>/dev/null | head -5
# ¡Coincide con timestamps, IPs, puertos, etc.!
```

<details><summary>Predicción</summary>

- `journalctl -u sshd` mostrará **solo** los logs generados por el servicio sshd, sin falsos positivos.
- `grep sshd` en texto plano también funcionará razonablemente bien en este caso porque "sshd" es un identificador bastante único.
- El contraste real se ve con `_PID=1`: journalctl devolverá **solo** los logs de PID 1 (systemd). `grep "1"` coincidirá con prácticamente todas las líneas (timestamps como "21:", IPs, puertos, etc.).

</details>

### Ejercicio 6 — Verificar reglas de enrutamiento

¿Qué reglas de rsyslog determinan qué mensajes van a qué archivo?

```bash
# 1. Ver reglas en Debian:
cat /etc/rsyslog.d/50-default.conf 2>/dev/null

# 2. Ver reglas en RHEL (todo en rsyslog.conf):
grep -E "^[a-z].*\s+/var/log/" /etc/rsyslog.conf 2>/dev/null

# 3. Interpretar una regla:
# auth,authpriv.*          /var/log/auth.log
# ↑ facility               ↑ prioridad (todas)   ↑ destino
#
# *.*;auth,authpriv.none   /var/log/syslog
# ↑ todo excepto auth     → todo menos auth va a syslog
```

<details><summary>Predicción</summary>

- En **Debian**, `50-default.conf` mostrará reglas como:
  - `auth,authpriv.* → /var/log/auth.log`
  - `*.*;auth,authpriv.none → /var/log/syslog`
  - `kern.* → /var/log/kern.log`
- En **RHEL**, las reglas están en `/etc/rsyslog.conf`:
  - `authpriv.* → /var/log/secure`
  - `*.info;mail.none;authpriv.none;cron.none → /var/log/messages`
- La sintaxis es `facility.priority   destino`. `none` excluye una facility.

</details>

### Ejercicio 7 — Probar el forwarding

Genera logs y verifica que llegan tanto al journal como a los archivos de texto.

```bash
# 1. Generar un log con identificador único:
logger -t mitest "mensaje-$(date +%s)"

# 2. Verificar en journal:
journalctl -t mitest -n 1 --no-pager

# 3. Verificar en texto plano:
grep mitest /var/log/syslog 2>/dev/null | tail -1 || \
grep mitest /var/log/messages 2>/dev/null | tail -1

# 4. Si el log aparece en journal pero NO en syslog:
# → Verificar ForwardToSyslog
# → Verificar que rsyslog está corriendo
# → Verificar ss -x | grep /dev/log
```

<details><summary>Predicción</summary>

- El log con `logger -t mitest` llegará primero a journald (aparece con `journalctl -t mitest`).
- Si `ForwardToSyslog=yes` (default) y rsyslog está activo, también aparecerá en `/var/log/syslog` o `/var/log/messages`.
- Si rsyslog no está instalado o no está corriendo, el log solo aparecerá en el journal.
- `-t mitest` establece el `SYSLOG_IDENTIFIER`, lo cual facilita el filtrado.

</details>

### Ejercicio 8 — Comparar escenarios de configuración

Analiza las tres configuraciones posibles de `Storage=` y `ForwardToSyslog=`.

```bash
# Escenario 1: Solo journald
echo "Storage=persistent, ForwardToSyslog=no (o rsyslog deshabilitado)"
echo "  → Logs solo en /var/log/journal/"
echo "  → Sin archivos de texto plano"

# Escenario 2: Solo rsyslog
echo "Storage=volatile, ForwardToSyslog=yes"
echo "  → Journal solo en RAM (buffer temporal)"
echo "  → Todo persiste via rsyslog en texto plano"

# Escenario 3: Ambos
echo "Storage=persistent, ForwardToSyslog=yes"
echo "  → Logs en journal Y en texto plano"
echo "  → Duplicación de I/O pero máxima flexibilidad"

# Verificar tu configuración actual:
echo "--- Tu configuración ---"
grep -E "^(Storage|ForwardToSyslog)" /etc/systemd/journald.conf 2>/dev/null || \
    echo "(defaults: Storage=auto, ForwardToSyslog=yes)"
systemctl is-active rsyslog 2>/dev/null || echo "rsyslog: no instalado/activo"
```

<details><summary>Predicción</summary>

- En un sistema típico verás `Storage=auto` (o comentado) + `ForwardToSyslog=yes` (o comentado, default es yes) + rsyslog activo = **Escenario 3 (ambos)**.
- En contenedores Docker verás probablemente rsyslog inactivo o no instalado = **Escenario 1**.
- El escenario 2 es raro en instalaciones modernas, pero se usa en migraciones desde sistemas SysVinit.

</details>

### Ejercicio 9 — Diagnóstico: logs no llegan a syslog

Simula y diagnostica el problema más común de integración.

```bash
# Pasos de diagnóstico cuando los logs NO aparecen en /var/log/syslog:

# 1. ¿rsyslog está corriendo?
systemctl is-active rsyslog

# 2. ¿ForwardToSyslog está habilitado?
systemd-analyze cat-config systemd/journald.conf 2>/dev/null | grep ForwardToSyslog

# 3. ¿El socket /dev/log existe?
ls -la /dev/log

# 4. ¿Quién escucha en /dev/log?
ss -xlp | grep /dev/log 2>/dev/null

# 5. Generar un log de prueba y verificar:
logger "diagnostico-test-$(date +%s)"
journalctl -n 1 --no-pager
grep "diagnostico-test" /var/log/syslog 2>/dev/null || echo "NO llegó a syslog"
```

<details><summary>Predicción</summary>

- Si rsyslog está `inactive`, los logs no pueden llegar a `/var/log/syslog` → reiniciar con `systemctl start rsyslog`.
- Si `ForwardToSyslog=no`, journald no reenvía al socket → cambiar a `yes` y reiniciar journald.
- Si `/dev/log` no existe, hay un problema con el socket → `systemctl restart systemd-journald`.
- Si todo está correcto pero no hay logs, verificar los permisos del directorio `/var/log/` y las reglas de rsyslog.

</details>

### Ejercicio 10 — Comparación Debian vs RHEL

Identifica las diferencias de integración journal-rsyslog entre distribuciones.

```bash
# En Debian:
docker compose exec debian-dev bash -c '
echo "=== Debian ==="
echo "Modulo rsyslog:"
grep -E "imuxsock|imjournal" /etc/rsyslog.conf 2>/dev/null || echo "  (no encontrado)"
echo "Archivos de log:"
ls /var/log/syslog /var/log/auth.log 2>/dev/null || echo "  (no encontrados)"
echo "ForwardToSyslog:"
grep ForwardToSyslog /etc/systemd/journald.conf 2>/dev/null || echo "  (default: yes)"
'

# En AlmaLinux (RHEL):
docker compose exec alma-dev bash -c '
echo "=== AlmaLinux (RHEL) ==="
echo "Modulo rsyslog:"
grep -E "imuxsock|imjournal" /etc/rsyslog.conf 2>/dev/null || echo "  (no encontrado)"
echo "Archivos de log:"
ls /var/log/messages /var/log/secure 2>/dev/null || echo "  (no encontrados)"
echo "ForwardToSyslog:"
grep ForwardToSyslog /etc/systemd/journald.conf 2>/dev/null || echo "  (default: yes)"
'
```

<details><summary>Predicción</summary>

- **Debian**: módulo `imuxsock`, archivos `syslog` + `auth.log`, depende de `ForwardToSyslog=yes`.
- **RHEL/AlmaLinux**: módulo `imjournal`, archivos `messages` + `secure`, **no depende** de ForwardToSyslog porque lee directamente del journal.
- Si rsyslog no está instalado en los contenedores, los archivos de log no existirán, pero journald seguirá funcionando normalmente.
- Esta diferencia de mecanismo (socket vs lectura directa) es la diferencia arquitectónica más importante entre familias de distribuciones en cuanto a logging.

</details>
