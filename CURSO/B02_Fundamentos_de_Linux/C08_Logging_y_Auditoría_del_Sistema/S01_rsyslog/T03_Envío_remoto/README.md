# T03 — Envío remoto

## Por qué centralizar logs

```bash
# En un entorno con múltiples servidores, los logs locales tienen problemas:
# 1. Si un servidor se compromete, el atacante puede borrar los logs
# 2. Si un servidor crashea, los logs pueden perderse o ser inaccesibles
# 3. Correlacionar eventos entre servidores requiere acceder a cada uno
# 4. Monitoreo centralizado requiere un punto único de consulta

# La solución: enviar los logs a un servidor central
#
#  web01 ──┐
#  web02 ──┼──→ logserver (servidor central de logs)
#  db01  ──┤      │
#  app01 ──┘      ├── /var/log/remote/web01/
#                 ├── /var/log/remote/web02/
#                 ├── /var/log/remote/db01/
#                 └── /var/log/remote/app01/
```

## TCP vs UDP

```bash
# rsyslog soporta dos protocolos de transporte:

# UDP (puerto 514):
# - Un @ en la acción: @logserver:514
# - Sin conexión (fire and forget)
# - Más rápido, menos overhead
# - PUEDE PERDER mensajes (no hay confirmación)
# - No cifrado
# - El protocolo syslog original (RFC 3164)

# TCP (puerto 514):
# - Dos @@ en la acción: @@logserver:514
# - Conexión persistente
# - Más lento, más overhead
# - NO pierde mensajes (retransmisión, buffering)
# - Se puede cifrar con TLS
# - Recomendado para producción

# RELP (puerto 2514):
# - Reliable Event Logging Protocol
# - Garantiza entrega end-to-end
# - Incluso si rsyslog se reinicia, no pierde mensajes
# - Requiere módulo adicional (imrelp/omrelp)
# - La opción más fiable
```

## Configuración del cliente (emisor)

### UDP — Configuración mínima

```bash
# /etc/rsyslog.d/60-remote.conf

# Enviar todo por UDP:
*.* @logserver.example.com:514

# Solo errores y superiores:
*.err @logserver.example.com:514

# Solo auth:
auth,authpriv.* @logserver.example.com:514
```

### TCP — Configuración recomendada

```bash
# /etc/rsyslog.d/60-remote.conf

# Enviar todo por TCP:
*.* @@logserver.example.com:514

# Con RainerScript (más control):
*.* action(type="omfwd"
           target="logserver.example.com"
           port="514"
           protocol="tcp"
           )
```

### TCP con buffering (producción)

```bash
# /etc/rsyslog.d/60-remote.conf

# Queue de disco para no perder mensajes si el servidor está caído:
action(type="omfwd"
       target="logserver.example.com"
       port="514"
       protocol="tcp"

       # Queue de disco — almacena mensajes si el servidor no responde:
       queue.type="LinkedList"
       queue.filename="fwd-to-logserver"
       queue.maxDiskSpace="1g"
       queue.saveOnShutdown="on"
       queue.size="10000"

       # Reintentos:
       action.resumeRetryCount="-1"
       action.resumeInterval="30"
       )

# Comportamiento:
# 1. Intenta enviar por TCP
# 2. Si el servidor no responde → almacena en cola de disco
# 3. Reintenta cada 30 segundos indefinidamente (-1)
# 4. Cuando el servidor vuelve → envía los mensajes acumulados
# 5. Al apagar rsyslog → guarda la cola en disco (saveOnShutdown)
```

### Enviar a múltiples servidores

```bash
# /etc/rsyslog.d/60-remote.conf

# Servidor primario:
*.* action(type="omfwd"
           target="logserver1.example.com"
           port="514"
           protocol="tcp"
           queue.type="LinkedList"
           queue.filename="fwd-primary"
           queue.maxDiskSpace="500m"
           queue.saveOnShutdown="on"
           action.resumeRetryCount="-1")

# Servidor secundario (copia):
*.* action(type="omfwd"
           target="logserver2.example.com"
           port="514"
           protocol="tcp"
           queue.type="LinkedList"
           queue.filename="fwd-secondary"
           queue.maxDiskSpace="500m"
           queue.saveOnShutdown="on"
           action.resumeRetryCount="-1")

# Cada mensaje se envía a AMBOS servidores
```

## Configuración del servidor (receptor)

### Recibir por TCP

```bash
# /etc/rsyslog.d/10-remote-input.conf

# Cargar el módulo de input TCP:
module(load="imtcp")
input(type="imtcp" port="514")

# Reiniciar rsyslog:
sudo systemctl restart rsyslog

# Verificar que escucha:
ss -tlnp | grep 514
# LISTEN  0  25  *:514  *:*  users:(("rsyslogd",...))
```

### Recibir por UDP

```bash
# /etc/rsyslog.d/10-remote-input.conf

module(load="imudp")
input(type="imudp" port="514")
```

### Separar logs por host

```bash
# /etc/rsyslog.d/30-remote-rules.conf

# Template para archivo dinámico por hostname:
template(name="RemoteLogs" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

# Regla: todo lo que venga de remoto va a su directorio:
if $fromhost-ip != "127.0.0.1" then {
    action(type="omfile"
           dynaFile="RemoteLogs"
           dirCreateMode="0755"
           fileCreateMode="0640"
           createDirs="on")
    stop
}
```

```bash
# Resultado en el filesystem:
# /var/log/remote/
# ├── web01/
# │   ├── nginx.log
# │   ├── sshd.log
# │   └── sudo.log
# ├── web02/
# │   ├── nginx.log
# │   └── sshd.log
# └── db01/
#     ├── postgresql.log
#     └── sshd.log
```

### Separar por IP (alternativa)

```bash
# Template por IP en lugar de hostname:
template(name="RemoteByIP" type="string"
    string="/var/log/remote/%fromhost-ip%/%programname%.log"
)

# Útil cuando los hostnames no son confiables o no se resuelven
```

## Seguridad — TLS

### Por qué TLS

```bash
# Sin TLS:
# - Los logs viajan en texto plano por la red
# - Un atacante puede leer información sensible (contraseñas en logs, IPs...)
# - Un atacante puede inyectar logs falsos (spoofing)
# - Un atacante puede interceptar y modificar logs (MITM)

# Con TLS:
# - Los logs viajan cifrados
# - Se verifica la identidad del servidor (y opcionalmente del cliente)
# - Integridad de los mensajes
```

### Configuración TLS en el servidor

```bash
# Requisitos:
# - Certificado del servidor (server.crt)
# - Clave privada del servidor (server.key)
# - Certificado de la CA (ca.crt)

# /etc/rsyslog.d/10-tls.conf (servidor):
global(
    defaultNetstreamDriver="gtls"
    defaultNetstreamDriverCAFile="/etc/rsyslog.d/tls/ca.crt"
    defaultNetstreamDriverCertFile="/etc/rsyslog.d/tls/server.crt"
    defaultNetstreamDriverKeyFile="/etc/rsyslog.d/tls/server.key"
)

module(load="imtcp"
    streamDriver.name="gtls"
    streamDriver.mode="1"           # 1 = TLS obligatorio
    streamDriver.authMode="x509/name"
)

input(type="imtcp" port="6514")     # Puerto estándar para syslog-TLS
```

### Configuración TLS en el cliente

```bash
# /etc/rsyslog.d/60-remote-tls.conf (cliente):
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
           streamDriverMode="1"
           streamDriverAuthMode="x509/name"
           streamDriverPermittedPeers="logserver.example.com"
           queue.type="LinkedList"
           queue.filename="fwd-tls"
           queue.maxDiskSpace="1g"
           queue.saveOnShutdown="on"
           action.resumeRetryCount="-1")
```

## RELP — Reliable Event Logging Protocol

```bash
# RELP garantiza entrega de mensajes incluso si rsyslog se reinicia
# durante el envío

# Instalar el módulo:
sudo apt install rsyslog-relp    # Debian
sudo dnf install rsyslog-relp    # RHEL
```

```bash
# Servidor RELP:
# /etc/rsyslog.d/10-relp.conf
module(load="imrelp")
input(type="imrelp" port="2514")
```

```bash
# Cliente RELP:
# /etc/rsyslog.d/60-remote-relp.conf
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

## Firewall

```bash
# En el servidor receptor, abrir los puertos necesarios:

# firewalld (RHEL/Fedora):
sudo firewall-cmd --permanent --add-port=514/tcp    # syslog TCP
sudo firewall-cmd --permanent --add-port=514/udp    # syslog UDP
sudo firewall-cmd --permanent --add-port=6514/tcp   # syslog TLS
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

## Verificar el envío remoto

```bash
# En el CLIENTE:
# 1. Enviar un mensaje de prueba:
logger -t remote-test "Prueba de envío remoto $(hostname)"

# 2. Verificar que rsyslog no tiene errores:
journalctl -u rsyslog --since "5 min ago" --no-pager

# 3. Verificar la conexión TCP:
ss -tnp | grep 514
# ESTAB  0  0  192.168.1.10:45678  192.168.1.100:514

# En el SERVIDOR:
# 1. Buscar el mensaje de prueba:
grep "remote-test" /var/log/remote/*/syslog 2>/dev/null
grep "remote-test" /var/log/syslog /var/log/messages 2>/dev/null

# 2. Verificar que está recibiendo:
ss -tlnp | grep 514
# Debe mostrar LISTEN

# 3. Monitor en tiempo real:
tail -f /var/log/remote/*/*.log
```

## Tabla comparativa de protocolos

| Aspecto | UDP (@) | TCP (@@) | TLS | RELP |
|---|---|---|---|---|
| Puerto por defecto | 514 | 514 | 6514 | 2514 |
| Fiabilidad | Baja | Media | Media | Alta |
| Cifrado | No | No | Sí | Opcional |
| Pérdida de mensajes | Posible | Rara | Rara | No |
| Overhead | Mínimo | Bajo | Medio | Medio |
| Sobrevive restart | No | No | No | Sí |
| Uso recomendado | LAN, no crítico | Producción básica | Producción segura | Producción crítica |

---

## Ejercicios

### Ejercicio 1 — Verificar capacidad de envío

```bash
# 1. ¿Hay alguna regla de envío remoto configurada?
grep -rE '@{1,2}' /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v '^#'
grep -r "omfwd\|omrelp" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v '^#'

# 2. ¿rsyslog tiene módulos de red cargados?
grep -rE "imtcp|imudp|imrelp" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v '^#'

# 3. ¿Hay puertos de syslog abiertos?
ss -tlnp 2>/dev/null | grep -E ':(514|6514|2514)\b'
```

### Ejercicio 2 — Simular envío local

```bash
# Enviar a localhost por TCP (verificar que rsyslog procesa correctamente):
# Agregar temporalmente a /etc/rsyslog.d/99-test-remote.conf:
# module(load="imtcp")
# input(type="imtcp" port="10514")
# Y probar con:
# logger --tcp --port 10514 -t remote-test "Prueba TCP local"
echo "Este ejercicio requiere configurar un puerto de escucha temporal"
```

### Ejercicio 3 — Cola de disco

```bash
# ¿Hay colas de disco configuradas?
grep -r "queue\." /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v '^#'

# ¿Hay archivos de cola en el directorio de trabajo?
ls /var/lib/rsyslog/ 2>/dev/null
# Archivos .qi y .dat son colas de disco
```
