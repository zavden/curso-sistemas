# T03 — Integración con rsyslog

> **Objetivo:** Entender cómo journald y rsyslog coexisten, qué papel cumple cada uno, y cómo configurarlos para distintos escenarios (solo journal, solo rsyslog, o ambos).

## Dos sistemas de logging que coexisten

En la mayoría de distribuciones Linux, **journald y rsyslog trabajan juntos**:

```
┌──────────────────────────────────────────────────────────────────┐
│                         Aplicaciones                              │
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
         (PID 26)
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

```bash
# Verificar que ambos están activos:
systemctl is-active systemd-journald rsyslog

# journald SIEMPRE está presente (es parte de systemd)
# rsyslog es un paquete separado (puede no estar en contenedores)
```

---

## Journal — Ventajas

| Característica | Descripción |
|----------------|-------------|
| **Estructurado** | Cada log es un conjunto de campos clave-valor (PID, UID, PRIORITY, etc.) |
| **Indexado** | Búsquedas extremadamente rápidas por cualquier campo |
| **Centralizado** | Un solo lugar para kernel, servicios, syslog, stdout/stderr |
| **Integral** | `journalctl --verify` verifica integridad criptográfica |
| **Rotación automática** | Gestiona el espacio sin necesidad de logrotate |
| **Boot logs** | Puede consultar logs de boots anteriores |

## rsyslog — Ventajas

| Característica | Descripción |
|----------------|-------------|
| **Texto plano** | Legible con `cat`, `grep`, `tail` sin herramientas especiales |
| **Reenvío remoto** | TCP/UDP/RELP a servidores centralizados |
| **Filtros complejos** | RainerScript, property-based filters, condiciones dinámicas |
| **Compatibilidad** | Herramientas legacy esperan `/var/log/syslog` |
| **Integración externa** | Elasticsearch, Kafka, database outputs, archiving |
| **Maturidad** | 20+ años de uso en producción |

---

## Cómo se comunican journald y rsyslog

### journald → rsyslog (ForwardToSyslog)

```bash
# journald reenvía logs al socket /dev/log:
# /etc/systemd/journald.conf
[Journal]
ForwardToSyslog=yes      # default en la mayoría de distros

# rsyslog escucha en /dev/log (módulo imuxsock)
```

```bash
# Verificar forwarding activo:
grep -i "ForwardToSyslog" /etc/systemd/journald.conf
# Si está comentado o ausente, el default es "yes" en la mayoría

# Verificar qué está escuchando en /dev/log:
ss -x | grep /dev/log
# u_str  LISTEN  0  queue...
```

### Dos enfoques de rsyslog para leer logs

```
┌─────────────────────────────────────────────────────────────┐
│ Debian/Ubuntu — imuxsock (socket tradicional)              │
│                                                             │
│ journald ──► ForwardToSyslog=yes ──► /dev/log ──► rsyslog  │
│             (socket)                     (imuxsock)          │
│                                                             │
│ Depende de ForwardToSyslog=yes en journald.conf            │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ RHEL/CentOS 7+ — imjournal (lectura directa del journal)  │
│                                                             │
│ journald ──► /var/log/journal/ ◄───────── rsyslog          │
│             (binario)          (imjournal, no usa socket)   │
│                                                             │
│ NO depende de ForwardToSyslog                             │
│ Lee todos los campos estructurados del journal             │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Configuración RHEL (/etc/rsyslog.conf):
module(load="imjournal"
    StateFile="imjournal.state"
    ratelimit.interval="600"
    ratelimit.burst="20000")

# Debian/Ubuntu (/etc/rsyslog.conf):
module(load="imuxsock")
module(load="imklog")      # logs del kernel
```

---

## Archivos de log de rsyslog

```bash
# Debian/Ubuntu:
/var/log/syslog         # todos los logs excepto auth
/var/log/auth.log       # autenticación (login, sudo, ssh, pam)
/var/log/kern.log       # mensajes del kernel
/var/log/mail.log       # sistema de correo
/var/log/daemon.log     # daemons (servicios sin unidad propia)
/var/log/user.log       # aplicaciones de usuario
/var/log/cron.log       # tareas programadas

# RHEL/Fedora:
/var/log/messages       # todos los logs (equivalente a syslog)
/var/log/secure         # autenticación (equivalente a auth.log)
/var/log/maillog        # correo
/var/log/cron           # tareas programadas
/var/log/boot.log       # mensajes de boot
```

### Reglas de enrutamiento (Debian)

```bash
cat /etc/rsyslog.d/50-default.conf

# auth,authpriv.*        /var/log/auth.log
# *.*;auth,authpriv.none /var/log/syslog
# kern.*                 /var/log/kern.log
# mail.*                 /var/log/mail.log
# daemon.*               /var/log/daemon.log
```

---

## journalctl vs archivos de texto

| Capacidad | journalctl | archivos de texto |
|-----------|------------|-------------------|
| Filtrar por unidad | `journalctl -u nginx` | `grep nginx /var/log/syslog` (impreciso) |
| Filtrar por PID | `journalctl _PID=1234` | `grep "1234"` (coincide con timestamps) |
| Filtrar por boot | `journalctl -b -1` | Buscar por timestamp en syslog |
| Filtrar por UID | `journalctl _UID=1000` | Imposible directamente |
| Formato JSON | `journalctl -o json` | Parsear con awk/sed (frágil) |
| Ver todos los campos | `journalctl -o verbose` | No disponible |
| Logs del kernel + servicio | `journalctl -k -u docker` | `dmesg + grep docker` (desconectados) |

### Lo que archivos de texto hacen mejor

```bash
# 1. Leer logs de un sistema que no puede arrancar:
# Boot desde USB/CD → mount del disco
cat /mnt/broken/var/log/syslog

# journalctl puede hacerlo con --directory:
journalctl --directory=/mnt/broken/var/log/journal/ -b -1

# 2. Integración con pipelines legacy:
# Logstash/Fluentd/Filebeat esperan texto plano

# 3. Rotación granular con logrotate:
# Políticas complejas de retención y compresión

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

# Desventaja:
# - Sin reenvío remoto nativo
#   → Solución: systemd-journal-upload
```

### Escenario 2: Solo rsyslog (sistemas legacy)

```bash
# journald mínimo, todo va por rsyslog:
# /etc/systemd/journald.conf
[Journal]
Storage=volatile
ForwardToSyslog=yes
RuntimeMaxUse=50M

# journald actúa como buffer RAM temporal
# rsyslog persiste todo en texto plano

# Ventajas:
# - Control total con rsyslog
# - logrotate para políticas de retención
# - Reenvío remoto robusto

# Desventaja:
# - Pierdes capacidades de búsqueda estructurada
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

# logrotate para archivos de rsyslog:
cat /etc/logrotate.d/rsyslog
# /var/log/syslog, /var/log/auth.log, etc.
```

---

## Reenvío remoto de logs

### Con rsyslog (tradicional)

```bash
# SERVIDOR RECEPTOR — /etc/rsyslog.conf:
module(load="imtcp")
input(type="imtcp" port="514")

# Reenvío tradicional:
# @  = UDP (no confiable para producción)
# @@ = TCP (confiable)
*.* @@logserver.example.com:514

# Más seguro — TLS加密:
# module(load="imtcp" streamdriver.mode="tls" streamdriver.authmode="x509/name")
# input(type="imtcp" port="514")
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
# TLS=...  (para conexión segura)

sudo systemctl enable systemd-journal-upload
sudo systemctl start systemd-journal-upload

# Ventaja: preserva todos los campos estructurados
# Desventaja: solo reenvía a otro journald (no a syslog tradicional)
```

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
| journald.conf.d | Soportado | Soportado |

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
# PROBLEMA: rsyslog no puede escribir (permissions)

# El socket /dev/log tiene permisos:
ls -la /dev/log
# srw-rw-rw- 1 root root ...

# Si rsyslog no puede escribir:
sudo usermod -aG adm rsyslog_user
```

---

## Quick reference

```
┌────────────────────────────────────────────────────────────────┐
│                     FLUJO DE LOGS                              │
│                                                                │
│  Aplicaciones                                                  │
│       │                                                        │
│       ├──► systemd-journald ──► /var/log/journal/            │
│       │       │                                              │
│       │       └──► ForwardToSyslog=yes ──► /dev/log ──► rsyslog
│       │                                              │         │
│       │                                              ▼         │
│       │                                    /var/log/syslog    │
│       │                                    /var/log/auth.log   │
│       │                                    Reenvío remoto      │
│       │                                                        │
│  kernel (dmesg)                                                 │
│       ├──► systemd-journald (via /dev/kmsg)                   │
│       └──► rsyslog (via imklog) ──► /var/log/kern.log        │
└────────────────────────────────────────────────────────────────┘
```

| Comando | Uso |
|---------|-----|
| `journalctl --no-pager` | Ver todo |
| `journalctl -u nginx -p err` | Errores de nginx |
| `journalctl -b -1` | Boot anterior |
| `grep nginx /var/log/syslog` | Modo legacy |
| `systemctl status rsyslog` | ¿Está corriendo? |
| `grep ForwardToSyslog /etc/systemd/journald.conf` | ¿Forwarding activo? |

---

## Ejercicios

### Ejercicio 1 — Verificar la coexistencia

```bash
# 1. ¿Están ambos activos?
systemctl is-active systemd-journald rsyslog

# 2. ¿ForwardToSyslog está activo (o es default)?
grep -i "ForwardToSyslog" /etc/systemd/journald.conf || \
    echo "(no definido — default: yes en la mayoría)"

# 3. ¿Qué módulo usa rsyslog para recibir logs?
grep -E "imuxsock|imjournal|imklog" /etc/rsyslog.conf /etc/rsyslog.d/*.conf
```

### Ejercicio 2 — Comparar outputs

```bash
# Comparar la misma info en journal vs texto:
echo "=== journalctl -u sshd -n 3 ==="
journalctl -u sshd -n 3 --no-pager 2>/dev/null

echo "=== grep sshd /var/log/auth.log ==="
grep sshd /var/log/auth.log 2>/dev/null | tail -3 || \
grep sshd /var/log/secure 2>/dev/null | tail -3

# Observar: journal tiene campos estructurados (PID, UID, TRANSPORT)
# syslog tiene texto plano legible
```

### Ejercicio 3 — Inventario de archivos de log

```bash
# ¿Qué archivos mantiene rsyslog?
ls -lh /var/log/*.log /var/log/messages /var/log/secure 2>/dev/null

# ¿Cuánto espacio total?
du -sh /var/log/

# ¿Cuántas líneas tiene cada uno?
wc -l /var/log/syslog /var/log/auth.log 2>/dev/null

# ¿Rotación configurada?
cat /etc/logrotate.d/rsyslog 2>/dev/null
```

### Ejercicio 4 — Escenario: migrar a solo journald

> ⚠️ **Solo en entorno de pruebas**

```bash
# Objetivo: deshabilitar rsyslog y depender solo de journald

# 1. Medir baseline:
du -sh /var/log/
systemctl is-active rsyslog

# 2. Detener y deshabilitar rsyslog:
sudo systemctl stop rsyslog
sudo systemctl disable rsyslog

# 3. Asegurar que journald tiene persistencia:
grep Storage /etc/systemd/journald.conf
# Should show: Storage=persistent

# 4. Verificar que los logs siguen llegando:
logger "test sin rsyslog"
journalctl -n 1 --no-pager

# 5. Comparar uso de disco después de un tiempo
```

### Ejercicio 5 — Configurar reenvío remoto

> ⚠️ **Requiere dos máquinas**

```bash
# MAQUINA A (servidor):
# /etc/rsyslog.conf — añadir:
module(load="imtcp")
input(type="imtcp" port="514")

# Reiniciar:
sudo systemctl restart rsyslog

# MAQUINA B (cliente):
# /etc/rsyslog.d/remote.conf:
*.* @@ip-de-maquina-A:514

sudo systemctl restart rsyslog

# Test:
logger "test de reenvío"
# En máquina A:
tail -1 /var/log/syslog
```
