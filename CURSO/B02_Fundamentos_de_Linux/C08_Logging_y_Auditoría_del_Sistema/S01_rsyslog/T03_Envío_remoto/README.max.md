# T03 — Envío remoto

> **Objetivo:** Configurar rsyslog para enviar logs a un servidor central y recibir logs de clientes remotos. Cubrir UDP, TCP, TLS y RELP.

## Por qué centralizar logs

```
┌────────────────────────────────────────────────────────────────────────────┐
│                    PROBLEMAS CON LOGS LOCALES                               │
│                                                                            │
│  1. COMPROMISO DE SERVIDOR                                                │
│     → Un atacante puede borrar/modificar logs locales                      │
│     → Pierdes evidencia forense                                           │
│                                                                            │
│  2. CRASHEO O INDISPONIBILIDAD                                           │
│     → Si el disco falla, los logs se pierden                               │
│     → No puedes acceder a los logs de un servidor caído                   │
│                                                                            │
│  3. CORRELACIÓN DE EVENTOS                                                │
│     → Un ataque puede involucrar 10 servidores                             │
│     → Tienes que ir a cada uno a buscar evidencia                         │
│     → Imposible correlacionar timestamps con precisión                    │
│                                                                            │
│  4. MONITOREO CENTRALIZADO                                                │
│     → Necesitas consultar 50 servidores para generar un reporte           │
│     → Herramientas de monitoreo (ELK, Splunk) esperan un flujo único        │
└────────────────────────────────────────────────────────────────────────────┘
```

```
┌────────────────────────────────────────────────────────────────────────────┐
│                   ARQUITURA DE LOGS CENTRALIZADOS                         │
│                                                                            │
│    web01              web02              db01              app01          │
│      │                 │                 │                 │              │
│      │ TCP/TLS         │ TCP/TLS         │ TCP/TLS         │ TCP/TLS      │
│      ▼                 ▼                 ▼                 ▼              │
│   ┌──────────────────────────────────────────────────────────────┐       │
│   │                  SERVIDOR CENTRAL DE LOGS                     │       │
│   │                    logserver.example.com                      │       │
│   │                                                               │       │
│   │   /var/log/remote/web01/   nginx.log, sshd.log, ...         │       │
│   │   /var/log/remote/web02/   nginx.log, sshd.log, ...         │       │
│   │   /var/log/remote/db01/    postgresql.log, sshd.log, ...   │       │
│   │   /var/log/remote/app01/   app.log, sshd.log, ...          │       │
│   │                                                               │       │
│   │   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐     │       │
│   │   │   ELK       │    │   Grafana   │    │   Backup    │     │       │
│   │   │  (Elastic   │    │  (dashboards│    │  (archivo   │     │       │
│   │   │   Stack)    │    │   + alerts) │    │  redundante)│     │       │
│   │   └─────────────┘    └─────────────┘    └─────────────┘     │       │
│   └──────────────────────────────────────────────────────────────┘       │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## Protocolos de transporte

| Protocolo | Sintaxis | Puerto | Fiabilidad | Cifrado | Uso |
|-----------|----------|--------|------------|---------|-----|
| **UDP** | `@servidor:514` | 514 | Baja (puede perder mensajes) | No | LAN, logs no críticos |
| **TCP** | `@@servidor:514` | 514 | Media (retransmisión) | No | Producción básica |
| **TLS/TCP** | con streamDriver | 6514 | Media | Sí | Producción segura |
| **RELP** | `omrelp` | 2514 | Alta (garantía de entrega) | Opcional | Misión crítica |

### UDP

```bash
# Un @ = UDP (sin conexión, fire-and-forget)
*.* @logserver.example.com:514

# Características:
# - Sin conexión (no hay handshake)
# - Más rápido, menos overhead
# - PUEDE PERDER mensajes (no hay confirmación)
# - No hay cifrado
# - Protocolo syslog original (RFC 3164)
```

### TCP

```bash
# Dos @@ = TCP (conexión persistente)
*.* @@logserver.example.com:514

# Características:
# - Conexión persistente (retransmisión si falla)
# - Garantía de entrega (más confiable)
# - Sin cifrado (usar TLS para eso)
# - Recomendado para producción sobre UDP
```

### RELP (Reliable Event Logging Protocol)

```bash
# Requiere módulo adicional:
sudo apt install rsyslog-relp    # Debian/Ubuntu
sudo dnf install rsyslog-relp    # RHEL/Fedora

# even si rsyslog se reinicia, no pierde mensajes
# El cliente sabe qué mensajes fueron recibidos
# El servidor confirma cada mensaje
```

---

## Configuración del cliente (emisor)

### UDP — Configuración mínima

```bash
# /etc/rsyslog.d/60-remote.conf
# Enviar TODO por UDP:
*.* @logserver.example.com:514

# Solo errores y superiores:
*.err @logserver.example.com:514

# Solo auth:
auth,authpriv.* @logserver.example.com:514
```

### TCP — Configuración básica

```bash
# /etc/rsyslog.d/60-remote.conf
# Enviar TODO por TCP:
*.* @@logserver.example.com:514

# Con RainerScript:
*.* action(type="omfwd"
           target="logserver.example.com"
           port="514"
           protocol="tcp")
```

### TCP con queue de disco (producción)

```bash
# Si el servidor está caído, los mensajes se almacenan en disco
# Cuando el servidor vuelve, se envían los mensajes acumulados

# /etc/rsyslog.d/60-remote.conf
action(type="omfwd"
       target="logserver.example.com"
       port="514"
       protocol="tcp"

       # Queue de disco (si el servidor no responde):
       queue.type="LinkedList"           # queue en memoria + disco
       queue.filename="fwd-logserver"    # prefijo de archivos en /var/lib/rsyslog/
       queue.maxDiskSpace="1g"           # máximo 1GB de cola
       queue.size="100000"               # mensajes en cola antes de ir a disco
       queue.lowwatermark="50000"        # cuando llegar a 50k, empieza a escribir a disco
       queue.highwatermark="100000"      # cuando llegar a 100k, todo a disco
       queue.saveOnShutdown="on"         # guardar cola al apagar rsyslog

       # Reintentos:
       action.resumeRetryCount="-1"      # reintentar infinitamente
       action.resumeInterval="30"         # cada 30 segundos
       action.execOnlyWhenPreviousIsSuspended="on"
)
```

### Enviar a múltiples servidores

```bash
# /etc/rsyslog.d/60-remote.conf

# Cada mensaje se envía a AMBOS servidores:
# PRIMARY:
*.* action(type="omfwd"
           target="logserver1.example.com"
           port="514"
           protocol="tcp"
           queue.type="LinkedList"
           queue.filename="fwd-primary"
           queue.maxDiskSpace="500m"
           action.resumeRetryCount="-1")

# SECONDARY (backup):
*.* action(type="omfwd"
           target="logserver2.example.com"
           port="514"
           protocol="tcp"
           queue.type="LinkedList"
           queue.filename="fwd-secondary"
           queue.maxDiskSpace="500m"
           action.resumeRetryCount="-1")
```

---

## Configuración del servidor (receptor)

### Recibir por TCP

```bash
# /etc/rsyslog.d/10-remote-input.conf
module(load="imtcp")
input(type="imtcp" port="514")

# Reiniciar:
sudo systemctl restart rsyslog

# Verificar que escucha:
ss -tlnp | grep 514
# LISTEN  0  25  *:514  *:*  users:(("rsyslogd",pid=1234,fd=5))
```

### Recibir por UDP

```bash
# /etc/rsyslog.d/10-remote-input-udp.conf
module(load="imudp")
input(type="imudp" port="514")
```

### Recibir por ambos

```bash
# Combinar en un solo archivo:
# /etc/rsyslog.d/10-remote-input.conf
module(load="imtcp")
input(type="imtcp" port="514")

module(load="imudp")
input(type="imudp" port="514")
```

### Separar logs por host (dynaFile)

```bash
# /etc/rsyslog.d/30-remote-rules.conf

# Template para archivo dinámico:
template(name="RemoteLogs" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

# Template con formato ISO 8601:
template(name="RemoteFormat" type="string"
    string="%timegenerated:::date-rfc3339% %fromhost-ip% %programname%: %msg%\n"
)

# Todo lo que venga de remoto → su directorio:
if $fromhost-ip != "127.0.0.1" then {
    action(type="omfile"
           dynaFile="RemoteLogs"
           template="RemoteFormat"
           dirCreateMode="0755"
           fileCreateMode="0640"
           createDirs="on")
    stop
}
```

```
# Resultado en el filesystem:
/var/log/remote/
├── web01/
│   ├── nginx.log
│   ├── sshd.log
│   └── sudo.log
├── web02/
│   ├── nginx.log
│   └── sshd.log
└── db01/
    ├── postgresql.log
    └── sshd.log
```

### Alternativa: por IP en lugar de hostname

```bash
# Útil cuando los hostnames no se resuelven bien por DNS:
template(name="RemoteByIP" type="string"
    string="/var/log/remote/%fromhost-ip%/%programname%.log"
)
```

---

## Seguridad — TLS

### Por qué TLS

```
┌─────────────────────────────────────────────────────────────────────┐
│                    SIN TLS (texto plano)                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Cliente ──────────────────────────────► Servidor                 │
│          192.168.1.10                              192.168.1.100   │
│                                                                     │
│   Un atacante en el camino puede:                                  │
│   ✗ Leer mensajes (passwords en logs, tokens, IPs)               │
│   ✗ Modificar mensajes (injectar logs falsos)                     │
│   ✗ Eliminar mensajes (borrar evidencia)                           │
│   ✗ Spoofing (enviar logs falsos desde IP falsa)                  │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                    CON TLS (cifrado)                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Cliente ═══════════════════════════════════► Servidor           │
│          192.168.1.10  (cifrado)                    192.168.1.100  │
│                                                                     │
│   ✓ Mensajes cifrados (no se pueden leer)                         │
│   ✓ Integridad (no se pueden modificar)                           │
│   ✓ Autenticación del servidor (certificado)                      │
│   ✓ Opcional: autenticación del cliente (certificado de cliente)  │
└─────────────────────────────────────────────────────────────────────┘
```

### Certificados necesarios

```bash
# GENERAR EN EL SERVIDOR (o usar una CA):
# 1. CA (Certificate Authority):
#    ca.crt + ca.key

# 2. Certificado del servidor:
#    server.crt + server.key (firmado por la CA)
#    CN = logserver.example.com

# 3. Certificado del cliente (opcional, para autenticación mutua):
#    client.crt + client.key (firmado por la CA)
```

### Configuración TLS — Servidor

```bash
# /etc/rsyslog.d/10-tls-server.conf
global(
    defaultNetstreamDriver="gtls"
    defaultNetstreamDriverCAFile="/etc/rsyslog.d/tls/ca.crt"
    defaultNetstreamDriverCertFile="/etc/rsyslog.d/tls/server.crt"
    defaultNetstreamDriverKeyFile="/etc/rsyslog.d/tls/server.key"
)

module(load="imtcp"
    streamDriver.name="gtls"
    streamDriver.mode="1"                # 1 = TLS obligatorio
    streamDriver.authMode="x509/name"     # verificar CN del cliente
    streamDriver.authMode="x509/fingerprint"  # alternativa por fingerprint
)

input(type="imtcp" port="6514")           # Puerto estándar syslog-TLS
```

### Configuración TLS — Cliente

```bash
# /etc/rsyslog.d/60-tls-remote.conf
global(
    defaultNetstreamDriver="gtls"
    defaultNetstreamDriverCAFile="/etc/rsyslog.d/tls/ca.crt"
    defaultNetstreamDriverCertFile="/etc/rsyslog.d/tls/client.crt"
    defaultNetstreamDriverKeyFile="/etc/rsyslog.d/tls/client.key"
)

*.* action(type="omfwd"
           target="logserver.example.com"
           port="6514"
           protocol="tcp"
           streamDriver="gtls"
           streamDriverMode="1"               # TLS obligatorio
           streamDriverAuthMode="x509/name"   # verificar CN del servidor
           streamDriverPermittedPeers="logserver.example.com"  # CN permitido

           queue.type="LinkedList"
           queue.filename="fwd-tls"
           queue.maxDiskSpace="1g"
           queue.saveOnShutdown="on"
           action.resumeRetryCount="-1")
```

---

## RELP — Reliable Event Logging Protocol

```bash
# Garantía de entrega end-to-end:
# - El cliente sabe exactamente qué mensajes fueron recibidos
# - Si rsyslog se reinicia, los mensajes se reenvían
# - No hay pérdida de mensajes

# Instalar módulo:
sudo apt install rsyslog-relp    # Debian/Ubuntu
sudo dnf install rsyslog-relp    # RHEL/Fedora
```

### RELP — Servidor

```bash
# /etc/rsyslog.d/10-relp-server.conf
module(load="imrelp")
input(type="imrelp" port="2514")
```

### RELP — Cliente

```bash
# /etc/rsyslog.d/60-relp-client.conf
module(load="omrelp")

*.* action(type="omrelp"
           target="logserver.example.com"
           port="2514"

           queue.type="LinkedList"
           queue.filename="fwd-relp"
           queue.maxDiskSpace="1g"
           queue.saveOnShutdown="on"
           action.resumeRetryCount="-1")
```

---

## Firewall

```bash
# En el SERVIDOR, abrir los puertos necesarios:

# firewalld (RHEL/Fedora):
sudo firewall-cmd --permanent --add-port=514/tcp
sudo firewall-cmd --permanent --add-port=514/udp
sudo firewall-cmd --permanent --add-port=6514/tcp   # TLS
sudo firewall-cmd --permanent --add-port=2514/tcp   # RELP
sudo firewall-cmd --reload

# ufw (Debian/Ubuntu):
sudo ufw allow 514/tcp
sudo ufw allow 514/udp
sudo ufw allow 6514/tcp
sudo ufw allow 2514/tcp

# iptables (legacy):
sudo iptables -A INPUT -p tcp --dport 514 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 514 -j ACCEPT
```

---

## Verificar el envío remoto

### En el CLIENTE

```bash
# 1. Enviar mensaje de prueba:
logger -t remote-test "Prueba de envío remoto desde $(hostname)"

# 2. Ver logs de rsyslog:
journalctl -u rsyslog --since "5 min ago" --no-pager

# 3. Ver conexión TCP activa:
ss -tnp | grep 514
# ESTAB  0  0  192.168.1.10:45678  192.168.1.100:514
#   ↑ conectado

# 4. Verificar queue (si está configurada):
ls -la /var/lib/rsyslog/
# -rw-------. 1 syslog syslog  1.2M Mar 19 10:30 fwd-logserver-00001.qi
```

### En el SERVIDOR

```bash
# 1. Buscar el mensaje de prueba:
grep "remote-test" /var/log/remote/*/syslog 2>/dev/null
grep "remote-test" /var/log/remote/*/* 2>/dev/null

# 2. Ver que está escuchando:
ss -tlnp | grep 514

# 3. Ver conexiones activas:
ss -tnp | grep rsyslog

# 4. Monitor en tiempo real:
journalctl -f -u rsyslog

# 5. Ver logs por host:
ls /var/log/remote/
tail -f /var/log/remote/*/*.log
```

### Diagnóstico de problemas

```bash
# PROBLEMA: Mensajes no llegan al servidor

# CLIENTE:
# 1. Verificar que rsyslog está corriendo:
systemctl status rsyslog

# 2. Ver si hay errores de conexión:
journalctl -u rsyslog --since "10 min ago" --no-pager | grep -i error

# 3. Verificar que el target está configurado:
grep -rE "@|omfwd|omrelp" /etc/rsyslog.d/*.conf

# 4. Probar conectividad básica:
nc -zv logserver.example.com 514

# SERVIDOR:
# 1. Ver si está escuchando:
ss -tlnp | grep 514

# 2. Ver logs de entrada:
journalctl -u rsyslog --since "10 min ago" --no-pager

# 3. Verificar que el firewall no bloquea:
sudo ss -tlnp | grep 514
```

---

## Quick reference

```
CLIENTE (enviar logs):
  UDP:        *.* @servidor:514
  TCP:        *.* @@servidor:514
  TLS:        con streamDriver="gtls" (ver configuración completa)
  RELP:       action(type="omrelp" target="..." port="2514")

SERVIDOR (recibir logs):
  TCP:        module(load="imtcp") + input(type="imtcp" port="514")
  UDP:        module(load="imudp") + input(type="imudp" port="514")
  TLS:        con streamDriver="gtls" (ver configuración completa)
  RELP:       module(load="imrelp") + input(type="imrelp" port="2514")

QUEUE DE DISCO:
  queue.type="LinkedList"
  queue.filename="fwd-servidor"
  queue.maxDiskSpace="1g"
  queue.saveOnShutdown="on"
  action.resumeRetryCount="-1"

PUERTOS:
  UDP:    514
  TCP:    514
  TLS:    6514
  RELP:   2514
```

---

## Comparativa de protocolos

| Aspecto | UDP | TCP | TLS | RELP |
|---------|-----|-----|-----|------|
| **Puerto default** | 514 | 514 | 6514 | 2514 |
| **Fiabilidad** | Baja | Media | Media | Alta |
| **Pérdida de mensajes** | Posible | Rara | Rara | **Ninguna** |
| **Cifrado** | No | No | Sí | No (TLS separado) |
| **Overhead** | Mínimo | Bajo | Medio | Medio |
| **Sobrevive restart** | No | No | No | **Sí** |
| **Handshake** | Ninguno | TCP handshake | TLS handshake | RELP handshake |
| **Uso recomendado** | LAN, dev | Producción | Producción segura | Misión crítica |

---

## Ejercicios

### Ejercicio 1 — Verificar configuración actual

```bash
# 1. ¿Hay reglas de envío remoto?
grep -rE '@{1,2}' /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null | grep -v '^#'
grep -r "omfwd\|omrelp" /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null | grep -v '^#'

# 2. ¿Qué módulos de red tiene?
grep -rE "imtcp|imudp|imrelp" /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null | grep -v '^#'

# 3. ¿Hay puertos de syslog escuchando?
ss -tlnp 2>/dev/null | grep -E ':(514|6514|2514)\b'
```

### Ejercicio 2 — Configurar servidor de logs

> ⚠️ **Requiere acceso root a una VM**

```bash
# 1. Crear configuración para recibir TCP:
cat << 'EOF' | sudo tee /etc/rsyslog.d/10-server.conf
module(load="imtcp")
input(type="imtcp" port="514")

template(name="RemoteLogs" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

if $fromhost-ip != "127.0.0.1" then {
    action(type="omfile"
           dynaFile="RemoteLogs"
           createDirs="on")
    stop
}
EOF

# 2. Crear directorio:
sudo mkdir -p /var/log/remote
sudo chown syslog:adm /var/log/remote

# 3. Reiniciar y verificar:
sudo systemctl restart rsyslog
ss -tlnp | grep 514
```

### Ejercicio 3 — Configurar cliente y probar

```bash
# 1. En el CLIENTE, agregar regla de envío:
# (asumiendo que 192.168.1.100 es el servidor del ejercicio anterior)
cat << 'EOF' | sudo tee /etc/rsyslog.d/60-test-forward.conf
*.* @@192.168.1.100:514
EOF

# 2. Reiniciar:
sudo systemctl restart rsyslog

# 3. Enviar mensaje de prueba:
logger -t test "Mensaje de prueba de envío remoto"

# 4. En el SERVIDOR, verificar:
grep test /var/log/remote/*/syslog 2>/dev/null
```

### Ejercicio 4 — Configurar queue de disco

```bash
# Objetivo: verificar que los mensajes se guardan si el servidor no responde

# 1. Ver si hay colas configuradas:
grep -r "queue\." /etc/rsyslog.d/*.conf 2>/dev/null | grep -v '^#'

# 2. Ver archivos de cola en disco:
ls -la /var/lib/rsyslog/ 2>/dev/null

# 3. Si el servidor está caído y hay cola configurada:
#    - Los mensajes se acumulan en disco
#    - Cuando el servidor vuelve, se reenvían

# 4. Ver tamaño de la cola:
du -sh /var/lib/rsyslog/
```

### Ejercicio 5 — Comparar UDP vs TCP

```bash
# 1. Medir overhead (con many mensajes):
#    - Enviar 10000 mensajes por UDP y por TCP
#    - Comparar tiempo

# 2. Simular pérdida (servidor abajo):
#    - Bajar el servidor
#    - Enviar mensajes
#    - Verificar queue en disco
#    - Subir el servidor
#    - Verificar que los mensajes llegan

# 3. Ver logs de rsyslog para detectar reintentos:
journalctl -u rsyslog --since "1 hour ago" --no-pager | grep -i "retry\|suspend\|resume"
```
