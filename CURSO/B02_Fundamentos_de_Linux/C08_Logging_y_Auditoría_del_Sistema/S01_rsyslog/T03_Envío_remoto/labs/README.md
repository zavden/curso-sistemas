# Lab — Envío remoto

## Objetivo

Entender el envio remoto de logs con rsyslog: por que centralizar
logs, TCP vs UDP vs RELP, configuracion del cliente (UDP, TCP, TCP
con queue de disco), envio a multiples servidores, configuracion
del servidor receptor, separar logs por host con dynaFile,
seguridad con TLS, RELP para entrega garantizada, firewall, y
verificacion del envio.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Protocolos y cliente

### Objetivo

Entender los protocolos de envio y como configurar el cliente.

### Paso 1.1: Por que centralizar logs

```bash
docker compose exec debian-dev bash -c '
echo "=== Por que centralizar logs ==="
echo ""
echo "Problemas con logs solo locales:"
echo "  1. Si un servidor se compromete, atacante borra logs"
echo "  2. Si crashea, logs pueden perderse o ser inaccesibles"
echo "  3. Correlacionar eventos requiere acceder a cada server"
echo "  4. Monitoreo centralizado necesita punto unico"
echo ""
echo "Solucion: enviar los logs a un servidor central"
echo ""
echo "  web01 ──┐"
echo "  web02 ──┼──→ logserver ──→ /var/log/remote/web01/"
echo "  db01  ──┤                  /var/log/remote/web02/"
echo "  app01 ──┘                  /var/log/remote/db01/"
'
```

### Paso 1.2: TCP vs UDP vs RELP

```bash
docker compose exec debian-dev bash -c '
echo "=== Protocolos de transporte ==="
echo ""
echo "| Aspecto            | UDP (@)      | TCP (@@)     | RELP         |"
echo "|--------------------|--------------|--------------|--------------|"
echo "| Puerto default     | 514          | 514          | 2514         |"
echo "| Fiabilidad         | Baja         | Media        | Alta         |"
echo "| Cifrado            | No           | No (si TLS)  | Opcional     |"
echo "| Perdida mensajes   | Posible      | Rara         | No           |"
echo "| Overhead           | Minimo       | Bajo         | Medio        |"
echo "| Sobrevive restart  | No           | No           | Si           |"
echo "| Uso recomendado    | LAN, no crit | Produccion   | Prod critica |"
echo ""
echo "--- Sintaxis en reglas ---"
echo "  @logserver:514     UDP (un @)"
echo "  @@logserver:514    TCP (dos @@)"
echo ""
echo "--- Modulos ---"
echo "  Input:  imudp (UDP), imtcp (TCP), imrelp (RELP)"
echo "  Output: omfwd (UDP/TCP), omrelp (RELP)"
'
```

### Paso 1.3: Cliente UDP (configuracion minima)

```bash
docker compose exec debian-dev bash -c '
echo "=== Cliente UDP ==="
echo ""
echo "Configuracion minima, un @ = UDP:"
echo ""
cat << '\''EOF'\''
# /etc/rsyslog.d/60-remote.conf

# Enviar todo por UDP:
*.* @logserver.example.com:514

# Solo errores y superiores:
*.err @logserver.example.com:514

# Solo auth:
auth,authpriv.* @logserver.example.com:514
EOF
echo ""
echo "--- Caracteristicas ---"
echo "  Fire and forget (no espera confirmacion)"
echo "  Puede perder mensajes"
echo "  Mas rapido, menos overhead"
echo "  NO recomendado para produccion"
'
```

### Paso 1.4: Cliente TCP (recomendado)

```bash
docker compose exec debian-dev bash -c '
echo "=== Cliente TCP ==="
echo ""
echo "Dos @@ = TCP:"
echo ""
echo "--- Sintaxis legacy ---"
echo "*.* @@logserver.example.com:514"
echo ""
echo "--- RainerScript (mas control) ---"
cat << '\''EOF'\''
# /etc/rsyslog.d/60-remote.conf
*.* action(type="omfwd"
           target="logserver.example.com"
           port="514"
           protocol="tcp"
           )
EOF
echo ""
echo "TCP establece conexion persistente."
echo "Si el servidor no responde, rsyslog detecta el problema."
echo "Pero sin queue, los mensajes se pierden durante la caida."
'
```

### Paso 1.5: Cliente TCP con queue de disco

```bash
docker compose exec debian-dev bash -c '
echo "=== TCP con queue de disco (produccion) ==="
echo ""
cat << '\''EOF'\''
# /etc/rsyslog.d/60-remote.conf
action(type="omfwd"
       target="logserver.example.com"
       port="514"
       protocol="tcp"

       # Queue de disco:
       queue.type="LinkedList"
       queue.filename="fwd-to-logserver"
       queue.maxDiskSpace="1g"
       queue.saveOnShutdown="on"
       queue.size="10000"

       # Reintentos:
       action.resumeRetryCount="-1"
       action.resumeInterval="30"
       )
EOF
echo ""
echo "--- Comportamiento ---"
echo "  1. Intenta enviar por TCP"
echo "  2. Si servidor no responde → almacena en cola de disco"
echo "  3. Reintenta cada 30 segundos indefinidamente (-1)"
echo "  4. Cuando vuelve → envia los acumulados"
echo "  5. Al apagar rsyslog → guarda la cola (saveOnShutdown)"
echo ""
echo "--- Archivos de cola ---"
echo "  Directorio: /var/lib/rsyslog/"
ls /var/lib/rsyslog/ 2>/dev/null || echo "  (directorio vacio o no existe)"
echo "  Archivos .qi y .dat = colas de disco"
'
```

### Paso 1.6: Enviar a multiples servidores

```bash
docker compose exec debian-dev bash -c '
echo "=== Multiples servidores ==="
echo ""
cat << '\''EOF'\''
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
EOF
echo ""
echo "Cada mensaje se envia a AMBOS servidores."
echo "Cada accion tiene su propia queue independiente."
'
```

---

## Parte 2 — Servidor y separacion por host

### Objetivo

Configurar el servidor receptor y separar logs por host.

### Paso 2.1: Servidor receptor TCP

```bash
docker compose exec debian-dev bash -c '
echo "=== Servidor receptor TCP ==="
echo ""
cat << '\''EOF'\''
# /etc/rsyslog.d/10-remote-input.conf

# Cargar modulo TCP:
module(load="imtcp")
input(type="imtcp" port="514")
EOF
echo ""
echo "Despues de crear el archivo:"
echo "  sudo systemctl restart rsyslog"
echo ""
echo "Verificar que escucha:"
echo "  ss -tlnp | grep 514"
echo "  LISTEN  0  25  *:514  *:*  users:((\"rsyslogd\",...))"
echo ""
echo "--- Verificar en este sistema ---"
ss -tlnp 2>/dev/null | grep -E ":(514|6514|2514)\b" || \
    echo "(no hay puertos de syslog escuchando)"
echo ""
echo "--- Receptor UDP ---"
cat << '\''EOF'\''
module(load="imudp")
input(type="imudp" port="514")
EOF
'
```

### Paso 2.2: Separar logs por hostname

```bash
docker compose exec debian-dev bash -c '
echo "=== Separar logs por host ==="
echo ""
cat << '\''EOF'\''
# /etc/rsyslog.d/30-remote-rules.conf

# Template con archivo dinamico por hostname:
template(name="RemoteLogs" type="string"
    string="/var/log/remote/%HOSTNAME%/%programname%.log"
)

# Todo lo remoto va a su directorio:
if $fromhost-ip != "127.0.0.1" then {
    action(type="omfile"
           dynaFile="RemoteLogs"
           dirCreateMode="0755"
           fileCreateMode="0640"
           createDirs="on")
    stop
}
EOF
echo ""
echo "Resultado en el filesystem:"
echo "  /var/log/remote/"
echo "  ├── web01/"
echo "  │   ├── nginx.log"
echo "  │   ├── sshd.log"
echo "  │   └── sudo.log"
echo "  ├── web02/"
echo "  │   └── nginx.log"
echo "  └── db01/"
echo "      ├── postgresql.log"
echo "      └── sshd.log"
echo ""
echo "createDirs=\"on\" crea los directorios automaticamente."
echo "stop evita que los mensajes remotos se mezclen con los locales."
'
```

### Paso 2.3: Separar por IP

```bash
docker compose exec debian-dev bash -c '
echo "=== Separar por IP ==="
echo ""
echo "Alternativa cuando los hostnames no son confiables:"
echo ""
cat << '\''EOF'\''
template(name="RemoteByIP" type="string"
    string="/var/log/remote/%fromhost-ip%/%programname%.log"
)

if $fromhost-ip != "127.0.0.1" then {
    action(type="omfile" dynaFile="RemoteByIP" createDirs="on")
    stop
}
EOF
echo ""
echo "Resultado:"
echo "  /var/log/remote/192.168.1.10/nginx.log"
echo "  /var/log/remote/192.168.1.20/postgresql.log"
echo ""
echo "--- Tambien se puede combinar reglas por IP ---"
cat << '\''EOF'\''
:fromhost-ip, isequal, "192.168.1.10"  /var/log/remote/web.log
:fromhost-ip, isequal, "192.168.1.20"  /var/log/remote/db.log
:fromhost-ip, startswith, "192.168.1." /var/log/remote/other.log
EOF
'
```

### Paso 2.4: Verificar config de envio existente

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar config de envio remoto ==="
echo ""
echo "--- Reglas de envio remoto ---"
grep -rE "@{1,2}" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | head -5 || echo "(sin envio remoto configurado)"
echo ""
echo "--- Modulos de red ---"
grep -rE "imtcp|imudp|imrelp|omfwd|omrelp" \
    /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | head -5 || echo "(sin modulos de red)"
echo ""
echo "--- Queues ---"
grep -r "queue\." /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | head -5 || echo "(sin queues configuradas)"
echo ""
echo "--- Puertos escuchando ---"
ss -tlnp 2>/dev/null | grep -E ":(514|6514|2514)\b" || \
    echo "(sin puertos de syslog)"
'
```

---

## Parte 3 — Seguridad y verificacion

### Objetivo

Entender TLS, RELP, firewall y como verificar el envio.

### Paso 3.1: Seguridad con TLS

```bash
docker compose exec debian-dev bash -c '
echo "=== TLS para syslog ==="
echo ""
echo "--- Sin TLS ---"
echo "  Logs viajan en texto plano"
echo "  Atacante puede leer informacion sensible"
echo "  Atacante puede inyectar logs falsos"
echo "  Atacante puede modificar logs (MITM)"
echo ""
echo "--- Con TLS ---"
echo "  Logs cifrados"
echo "  Identidad del servidor verificada"
echo "  Integridad de los mensajes"
echo ""
echo "--- Puerto estandar ---"
echo "  6514/tcp (syslog-TLS)"
echo ""
echo "--- Servidor TLS ---"
cat << '\''EOF'\''
# /etc/rsyslog.d/10-tls.conf
global(
    defaultNetstreamDriver="gtls"
    defaultNetstreamDriverCAFile="/etc/rsyslog.d/tls/ca.crt"
    defaultNetstreamDriverCertFile="/etc/rsyslog.d/tls/server.crt"
    defaultNetstreamDriverKeyFile="/etc/rsyslog.d/tls/server.key"
)

module(load="imtcp"
    streamDriver.name="gtls"
    streamDriver.mode="1"
    streamDriver.authMode="x509/name"
)

input(type="imtcp" port="6514")
EOF
'
```

### Paso 3.2: Cliente TLS

```bash
docker compose exec debian-dev bash -c '
echo "=== Cliente TLS ==="
echo ""
cat << '\''EOF'\''
# /etc/rsyslog.d/60-remote-tls.conf
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
EOF
echo ""
echo "Requisitos:"
echo "  - Certificado de la CA (ca.crt)"
echo "  - Certificado del cliente (client.crt)"
echo "  - Clave privada del cliente (client.key)"
echo "  - Paquete rsyslog-gnutls instalado"
echo ""
dpkg -l rsyslog-gnutls 2>/dev/null | grep "^ii" || \
    echo "rsyslog-gnutls: no instalado"
'
```

### Paso 3.3: RELP

```bash
docker compose exec debian-dev bash -c '
echo "=== RELP — Reliable Event Logging Protocol ==="
echo ""
echo "Garantiza entrega end-to-end."
echo "Incluso si rsyslog se reinicia, no pierde mensajes."
echo ""
echo "--- Servidor RELP ---"
cat << '\''EOF'\''
# /etc/rsyslog.d/10-relp.conf
module(load="imrelp")
input(type="imrelp" port="2514")
EOF
echo ""
echo "--- Cliente RELP ---"
cat << '\''EOF'\''
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
EOF
echo ""
echo "--- Instalar ---"
echo "  Debian: apt install rsyslog-relp"
echo "  RHEL:   dnf install rsyslog-relp"
echo ""
dpkg -l rsyslog-relp 2>/dev/null | grep "^ii" || \
    echo "rsyslog-relp: no instalado"
'
```

### Paso 3.4: Firewall

```bash
docker compose exec debian-dev bash -c '
echo "=== Firewall ==="
echo ""
echo "En el servidor receptor, abrir puertos:"
echo ""
echo "--- firewalld (RHEL/Fedora) ---"
echo "  firewall-cmd --permanent --add-port=514/tcp"
echo "  firewall-cmd --permanent --add-port=514/udp"
echo "  firewall-cmd --permanent --add-port=6514/tcp   # TLS"
echo "  firewall-cmd --permanent --add-port=2514/tcp   # RELP"
echo "  firewall-cmd --reload"
echo ""
echo "--- ufw (Debian/Ubuntu) ---"
echo "  ufw allow 514/tcp"
echo "  ufw allow 514/udp"
echo "  ufw allow 6514/tcp"
echo "  ufw allow 2514/tcp"
echo ""
echo "--- iptables (legacy) ---"
echo "  iptables -A INPUT -p tcp --dport 514 -j ACCEPT"
echo "  iptables -A INPUT -p udp --dport 514 -j ACCEPT"
'
```

### Paso 3.5: Verificar envio remoto

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar envio remoto ==="
echo ""
echo "--- En el CLIENTE ---"
echo ""
echo "1. Enviar mensaje de prueba:"
echo "   logger -t remote-test \"Prueba de envio remoto \$(hostname)\""
echo ""
echo "2. Verificar errores de rsyslog:"
echo "   journalctl -u rsyslog --since \"5 min ago\" --no-pager"
echo ""
echo "3. Verificar conexion TCP:"
echo "   ss -tnp | grep 514"
echo "   ESTAB  0  0  192.168.1.10:45678  192.168.1.100:514"
echo ""
echo "--- En el SERVIDOR ---"
echo ""
echo "1. Buscar mensaje de prueba:"
echo "   grep \"remote-test\" /var/log/remote/*/*.log"
echo ""
echo "2. Verificar que escucha:"
echo "   ss -tlnp | grep 514"
echo ""
echo "3. Monitor en tiempo real:"
echo "   tail -f /var/log/remote/*/*.log"
echo ""
echo "--- Estado actual de este sistema ---"
echo ""
echo "Conexiones TCP a puertos syslog:"
ss -tnp 2>/dev/null | grep -E ":(514|6514|2514)\b" || \
    echo "  (sin conexiones syslog)"
echo ""
echo "Errores recientes de rsyslog:"
journalctl -u rsyslog --since "1h ago" --no-pager -p err 2>/dev/null | \
    tail -3 || echo "  (sin errores)"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Centralizar logs protege contra perdida (crash) y
   manipulacion (compromiso). Permite correlacion y
   monitoreo desde un punto unico.

2. UDP (@) es rapido pero pierde mensajes. TCP (@@) es
   fiable. RELP garantiza entrega end-to-end incluso
   si rsyslog se reinicia.

3. La queue de disco (queue.type, queue.filename,
   queue.maxDiskSpace, queue.saveOnShutdown) almacena
   mensajes cuando el servidor esta caido y los envia
   al reconectar.

4. dynaFile con %HOSTNAME% o %fromhost-ip% crea archivos
   automaticamente por host, organizando logs remotos
   en directorios separados.

5. TLS (puerto 6514) cifra los logs en transito. Requiere
   certificados (CA, servidor, opcionalmente cliente) y
   el modulo rsyslog-gnutls.

6. Para produccion: usar TCP con queue de disco como
   minimo. Para produccion critica: RELP. Para seguridad:
   TLS. Abrir puertos en el firewall del servidor.
