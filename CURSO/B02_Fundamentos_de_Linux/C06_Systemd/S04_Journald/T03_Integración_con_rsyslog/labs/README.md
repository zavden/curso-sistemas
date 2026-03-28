# Lab — Integración con rsyslog

## Objetivo

Entender como journald y rsyslog coexisten en un sistema Linux:
el flujo de logs entre ambos, ForwardToSyslog, modulos de entrada
de rsyslog (imuxsock vs imjournal), archivos de log que mantiene
rsyslog por distribucion, comparacion de capacidades entre journalctl
y archivos de texto plano, escenarios de configuracion, y reenvio
remoto de logs.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Coexistencia y flujo

### Objetivo

Entender como journald y rsyslog trabajan juntos y el flujo de
logs entre ambos sistemas.

### Paso 1.1: Dos sistemas de logging

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

### Paso 1.2: ForwardToSyslog

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

### Paso 1.3: Modulos de entrada de rsyslog

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
echo "--- imuxsock (Debian/Ubuntu) ---"
echo "  module(load=\"imuxsock\")    escucha en /dev/log"
echo "  module(load=\"imklog\")      logs del kernel"
echo "  Depende de ForwardToSyslog=yes"
echo ""
echo "--- imjournal (RHEL) ---"
echo "  module(load=\"imjournal\""
echo "      StateFile=\"imjournal.state\")"
echo "  Lee directamente de /var/log/journal/"
echo "  NO depende de ForwardToSyslog"
echo "  Accede a todos los campos estructurados"
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
    grep -E "^(module|input)" /etc/rsyslog.conf 2>/dev/null | head -5 || \
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

### Paso 1.4: Archivos de log por distribucion

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
echo "--- Reglas de rsyslog ---"
echo "  auth,authpriv.*          /var/log/auth.log      (Debian)"
echo "  *.*;auth,authpriv.none   /var/log/syslog        (Debian)"
echo "  kern.*                   /var/log/kern.log      (Debian)"
echo "  authpriv.*               /var/log/secure        (RHEL)"
echo "  *.info;...               /var/log/messages      (RHEL)"
echo ""
echo "--- Verificar archivos existentes ---"
echo "Debian:"
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

## Parte 2 — journalctl vs archivos de texto

### Objetivo

Comparar las capacidades de journalctl frente a grep/tail en archivos
de texto plano.

### Paso 2.1: Ventajas de journalctl

```bash
docker compose exec debian-dev bash -c '
echo "=== Ventajas de journalctl ==="
echo ""
echo "--- Filtro por servicio ---"
echo "  journalctl -u nginx"
echo "  vs: grep nginx /var/log/syslog"
echo "       (puede coincidir con otras cosas: URL, configuracion)"
echo ""
echo "--- Filtro por PID ---"
echo "  journalctl _PID=1234"
echo "  vs: grep 1234 /var/log/syslog"
echo "       (coincide con timestamps, IPs, puertos...)"
echo ""
echo "--- Filtro por boot ---"
echo "  journalctl -b -1"
echo "  vs: grep por timestamp en syslog"
echo "       (complicado, propenso a errores)"
echo ""
echo "--- Filtro por prioridad ---"
echo "  journalctl -p err"
echo "  vs: grep -E \"(error|err|crit|alert|emerg)\" /var/log/syslog"
echo "       (falsos positivos: \"no error found\")"
echo ""
echo "--- Formato JSON para procesamiento ---"
echo "  journalctl -o json -u nginx | jq .MESSAGE"
echo "  vs: parsear texto plano con awk/sed (fragil)"
echo ""
echo "--- Busqueda indexada ---"
echo "  journalctl es rapido porque el journal esta indexado"
echo "  grep recorre todo el archivo secuencialmente"
echo ""
echo "--- Rotacion integrada ---"
echo "  journalctl --vacuum-size=100M"
echo "  vs: configurar logrotate por separado"
'
```

### Paso 2.2: Ventajas de archivos de texto

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
echo "  vs: journalctl --directory=/mnt/broken/var/log/journal/"
echo "       (journalctl requiere que el formato binario este intacto)"
echo ""
echo "--- Herramientas externas ---"
echo "  Logstash, Fluentd, Splunk, etc."
echo "  Muchas esperan archivos de texto plano"
echo "  (aunque ya hay plugins para journal)"
echo ""
echo "--- Rotacion granular ---"
echo "  logrotate permite:"
echo "    Compresion individual por archivo"
echo "    Rotacion por tamano O por tiempo"
echo "    Scripts pre/post rotacion"
echo "    Diferentes politicas por servicio"
echo ""
echo "--- Scripts legacy ---"
echo "  grep sshd /var/log/auth.log | wc -l"
echo "  Scripts que llevan anos funcionando asi"
echo ""
echo "--- Reenvio remoto maduro ---"
echo "  rsyslog: @@ servidor:514 (TCP)"
echo "  Protocolo probado por decadas"
echo "  RELP para entrega garantizada"
'
```

### Paso 2.3: Tabla comparativa

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparacion: journalctl vs texto plano ==="
echo ""
echo "| Aspecto              | journalctl          | Texto plano        |"
echo "|----------------------|---------------------|--------------------|"
echo "| Formato              | Binario indexado    | Texto legible      |"
echo "| Filtro por servicio  | Exacto (-u)         | Aproximado (grep)  |"
echo "| Filtro por PID       | Exacto (_PID=)      | Falsos positivos   |"
echo "| Filtro por boot      | Trivial (-b)        | Manual             |"
echo "| Prioridad            | Exacta (-p)         | Fragil (grep)      |"
echo "| Espacio              | Comprimido          | Texto + logrotate  |"
echo "| Velocidad            | Indexado (rapido)   | Secuencial (lento) |"
echo "| Procesamiento        | -o json + jq        | awk/sed/grep       |"
echo "| Rescate sin boot     | Posible pero limitado | cat directo      |"
echo "| Reenvio remoto       | journal-upload      | rsyslog (maduro)   |"
echo "| Herramientas externas| Plugins necesarios  | Soporte nativo     |"
echo "| Rotacion             | Automatica          | logrotate manual   |"
echo ""
echo "Conclusion: se complementan, por eso coexisten"
'
```

### Paso 2.4: Ver la configuracion de rsyslog

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
    echo ""
    echo "--- Ejemplo de /etc/rsyslog.conf tipico ---"
    echo "module(load=\"imuxsock\")"
    echo "module(load=\"imklog\")"
    echo ""
    echo "auth,authpriv.*          /var/log/auth.log"
    echo "*.*;auth,authpriv.none   /var/log/syslog"
    echo "kern.*                   /var/log/kern.log"
    echo "mail.*                   /var/log/mail.log"
fi
echo ""
echo "--- logrotate para rsyslog ---"
if [[ -f /etc/logrotate.d/rsyslog ]]; then
    echo "/etc/logrotate.d/rsyslog:"
    cat /etc/logrotate.d/rsyslog 2>/dev/null | head -15
else
    echo "logrotate config no encontrado"
    echo ""
    echo "Ejemplo tipico:"
    echo "/var/log/syslog {"
    echo "    rotate 7"
    echo "    daily"
    echo "    compress"
    echo "    missingok"
    echo "    notifempty"
    echo "    postrotate"
    echo "        /usr/lib/rsyslog/rsyslog-rotate"
    echo "    endscript"
    echo "}"
fi
'
```

---

## Parte 3 — Escenarios y reenvio

### Objetivo

Conocer los tres escenarios de configuracion y el reenvio remoto
de logs.

### Paso 3.1: Solo journald (sin rsyslog)

```bash
docker compose exec debian-dev bash -c '
echo "=== Escenario: solo journald ==="
echo ""
echo "Cuando usar:"
echo "  Contenedores y sistemas minimalistas"
echo "  Cuando no se necesita reenvio remoto via rsyslog"
echo "  Cuando se quiere reducir procesos e I/O"
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
echo "  Menos procesos"
echo "  Menos I/O de disco (no se duplican logs)"
echo "  Gestion centralizada con journalctl"
echo ""
echo "--- Desventajas ---"
echo "  Sin reenvio remoto nativo robusto"
echo "    (existe systemd-journal-upload pero menos maduro)"
echo "  Herramientas legacy no funcionan"
echo "  Scripts que dependen de /var/log/syslog fallan"
'
```

### Paso 3.2: Solo rsyslog (journal minimo)

```bash
docker compose exec debian-dev bash -c '
echo "=== Escenario: solo rsyslog ==="
echo ""
echo "Cuando usar:"
echo "  Entornos con ecosistema rsyslog completo"
echo "  Cuando se necesita logrotate granular"
echo "  Migracion desde sistemas sin systemd"
echo ""
echo "--- Configuracion ---"
echo "# /etc/systemd/journald.conf"
echo "[Journal]"
echo "Storage=volatile           solo RAM como buffer"
echo "ForwardToSyslog=yes        reenviar todo a rsyslog"
echo "RuntimeMaxUse=50M          limitar uso de RAM"
echo ""
echo "# journald solo mantiene logs en RAM temporalmente"
echo "# Todo se persiste a traves de rsyslog"
echo ""
echo "--- Ventajas ---"
echo "  Control total con rsyslog y logrotate"
echo "  Reenvio remoto maduro (TCP, RELP)"
echo "  Compatibilidad con herramientas legacy"
echo ""
echo "--- Desventajas ---"
echo "  Se pierde la busqueda indexada de journalctl"
echo "  journalctl -b -1 no funciona (no hay persistencia)"
echo "  Filtros por PID/servicio son menos precisos"
'
```

### Paso 3.3: Ambos (produccion tipica)

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
echo "# journald persiste TODO con indexacion y busqueda rapida"
echo "# rsyslog recibe copias y:"
echo "#   Escribe en texto plano para herramientas legacy"
echo "#   Reenvia a un servidor central de logs"
echo "#   Alimenta herramientas de monitoreo"
echo ""
echo "--- Flujo ---"
echo "  Servicio → journald → /var/log/journal/ (binario)"
echo "                  |"
echo "                  +→ rsyslog → /var/log/syslog (texto)"
echo "                          |"
echo "                          +→ logserver:514 (remoto)"
echo ""
echo "--- Ventajas ---"
echo "  Lo mejor de ambos mundos"
echo "  journalctl para diagnostico rapido"
echo "  rsyslog para reenvio remoto y compatibilidad"
echo ""
echo "--- Desventaja ---"
echo "  Duplicacion de I/O (se escribe dos veces)"
echo "  Mas espacio en disco"
echo "  Pero el beneficio justifica el costo"
'
```

### Paso 3.4: Reenvio remoto

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
echo "Protocolo RELP (entrega garantizada):"
echo "  module(load=\"omrelp\")"
echo "  action(type=\"omrelp\" target=\"logserver\" port=\"2514\")"
echo ""
echo "--- Con systemd-journal-upload (alternativa) ---"
echo ""
echo "Servidor receptor:"
echo "  systemctl enable systemd-journal-remote.socket"
echo ""
echo "Cliente:"
echo "  # /etc/systemd/journal-upload.conf"
echo "  [Upload]"
echo "  URL=http://logserver.example.com:19532"
echo ""
echo "  systemctl enable systemd-journal-upload"
echo ""
echo "--- Comparacion ---"
echo "| Aspecto    | rsyslog          | journal-upload      |"
echo "|------------|------------------|---------------------|"
echo "| Madurez    | Decadas          | Relativamente nuevo |"
echo "| Protocolo  | TCP/UDP/RELP     | HTTPS               |"
echo "| Formato    | Texto syslog     | Journal nativo      |"
echo "| Filtros    | RainerScript     | Limitados           |"
echo "| Ecosistema | Amplio           | Reducido            |"
'
```

### Paso 3.5: Debian vs RHEL

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
echo "Debian: journald → ForwardToSyslog → /dev/log → rsyslog"
echo "  rsyslog depende del forwarding de journald"
echo ""
echo "RHEL:   rsyslog ← imjournal ← /var/log/journal/"
echo "  rsyslog lee directamente del journal"
echo "  ForwardToSyslog no es estrictamente necesario"
echo ""
echo "--- Verificar en Debian ---"
echo "journald:"
grep -i "Storage\|ForwardToSyslog" /etc/systemd/journald.conf 2>/dev/null || \
    echo "  (defaults: Storage=auto, ForwardToSyslog=yes)"
echo ""
echo "rsyslog:"
if [[ -f /etc/rsyslog.conf ]]; then
    grep -c "imuxsock\|imjournal" /etc/rsyslog.conf 2>/dev/null && \
        echo "  (modulo encontrado)"
else
    echo "  (rsyslog no instalado)"
fi
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
    grep -c "imuxsock\|imjournal" /etc/rsyslog.conf 2>/dev/null && \
        echo "  (modulo encontrado)"
    echo ""
    echo "Buscar imjournal:"
    grep "imjournal" /etc/rsyslog.conf 2>/dev/null || \
        echo "  (no encontrado)"
else
    echo "  (rsyslog no instalado)"
fi
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. journald y rsyslog coexisten: journald captura todo en formato
   binario indexado, rsyslog recibe copias y las escribe en texto
   plano. `ForwardToSyslog=yes` (default) controla el reenvio.

2. Debian usa `imuxsock` (rsyslog escucha en /dev/log), RHEL usa
   `imjournal` (rsyslog lee directamente del journal). Diferente
   mecanismo, mismo resultado.

3. Archivos de log difieren por distribucion: `/var/log/syslog` y
   `auth.log` (Debian) vs `/var/log/messages` y `secure` (RHEL).

4. journalctl es superior para filtros precisos (servicio, PID,
   boot, prioridad). Archivos de texto son mejores para rescate,
   herramientas legacy, y reenvio remoto maduro.

5. Tres escenarios: solo journald (contenedores), solo rsyslog
   (ecosistema legacy), ambos (produccion tipica). Cada uno tiene
   su configuracion especifica de `Storage=` y `ForwardToSyslog=`.

6. rsyslog ofrece reenvio remoto maduro (TCP/UDP/RELP).
   `systemd-journal-upload` es la alternativa nativa de systemd
   pero menos madura.
