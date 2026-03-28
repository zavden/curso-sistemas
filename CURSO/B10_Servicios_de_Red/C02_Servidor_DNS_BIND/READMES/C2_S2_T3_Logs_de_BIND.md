# Logs de BIND

## Índice

1. [Arquitectura de logging en BIND](#1-arquitectura-de-logging-en-bind)
2. [Canales (channels)](#2-canales-channels)
3. [Categorías (categories)](#3-categorías-categories)
4. [Configuraciones completas por escenario](#4-configuraciones-completas-por-escenario)
5. [Modo debug](#5-modo-debug)
6. [Análisis de logs en producción](#6-análisis-de-logs-en-producción)
7. [Integración con sistemas de logging](#7-integración-con-sistemas-de-logging)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Arquitectura de logging en BIND

BIND implementa un sistema de logging propio, independiente de syslog, con dos
conceptos fundamentales: **canales** (dónde enviar los mensajes) y **categorías**
(qué mensajes enviar). Esta separación permite una granularidad que syslog por sí
solo no ofrece.

```
┌─────────────────────────────────────────────────────────┐
│                      named                              │
│                                                         │
│  ┌─────────────┐   ┌─────────────┐   ┌──────────────┐  │
│  │  queries     │   │  security   │   │  xfer-in     │  │
│  │  (category)  │   │  (category) │   │  (category)  │  │
│  └──────┬───┬──┘   └──────┬──────┘   └──────┬───────┘  │
│         │   │              │                 │          │
│         │   │    ┌─────────┘                 │          │
│         │   │    │                           │          │
│  ┌──────▼───┼────▼─────┐  ┌─────────────────▼───────┐  │
│  │  file channel       │  │  syslog channel         │  │
│  │  /var/log/named/    │  │  daemon.info            │  │
│  │  query.log          │  │                         │  │
│  └─────────────────────┘  └─────────────────────────┘  │
│                                                         │
│  ┌─────────────────────┐  ┌─────────────────────────┐  │
│  │  stderr channel     │  │  null channel           │  │
│  │  (consola)          │  │  (descarta)             │  │
│  └─────────────────────┘  └─────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### Relación muchos-a-muchos

Una categoría puede enviar mensajes a múltiples canales, y un canal puede recibir
mensajes de múltiples categorías. Esta flexibilidad permite, por ejemplo, enviar
las queries tanto a un archivo dedicado como a syslog simultáneamente.

### Comportamiento por defecto

Sin configuración explícita de `logging {}`, BIND envía todo a syslog con
facility `daemon` y severity `info`. Esto significa que los mensajes se mezclan
con los de otros daemons en `/var/log/messages` o `/var/log/syslog`, haciendo
difícil el análisis específico de DNS.

### Canales predefinidos

BIND incluye cuatro canales que existen sin necesidad de declararlos:

| Canal predefinido    | Destino               | Severity  |
|----------------------|-----------------------|-----------|
| `default_syslog`     | syslog `daemon`       | `info`    |
| `default_debug`      | stderr                | dynamic   |
| `default_stderr`     | stderr                | `info`    |
| `null`               | descarta mensajes     | —         |

---

## 2. Canales (channels)

Un canal define **dónde** y **cómo** se escriben los mensajes. Existen cuatro
tipos de destino.

### 2.1 Canal tipo file

El más usado en producción. Permite rotación automática y control de tamaño:

```
logging {
    channel query_log {
        file "/var/log/named/query.log"
            versions 5          // mantener 5 archivos rotados
            size 50m            // rotar al llegar a 50 MB
            suffix increment;   // query.log.0, .1, .2...
        severity info;
        print-time yes;         // timestamp en cada línea
        print-category yes;     // nombre de categoría
        print-severity yes;     // nivel de severidad
    };
};
```

**Parámetros de archivo:**

| Parámetro          | Descripción                                           |
|--------------------|-------------------------------------------------------|
| `versions N`       | Número de copias rotadas a mantener (0 = sin límite)  |
| `versions unlimited` | Mantener todas las versiones (cuidado con disco)   |
| `size N`           | Tamaño máximo antes de rotar (`k`, `m`, `g`)          |
| `suffix increment` | Nombra archivos `.0`, `.1`, etc. (defecto)            |
| `suffix timestamp` | Nombra archivos con fecha/hora de rotación            |

**Opciones de formato:**

| Opción              | Efecto                                  | Recomendación       |
|---------------------|-----------------------------------------|---------------------|
| `print-time yes`    | Añade timestamp ISO 8601               | Siempre en archivos |
| `print-time iso8601`| Formato explícito ISO 8601 con zona    | Preferido BIND 9.11+|
| `print-category yes`| Prefijo con nombre de categoría        | Útil para filtrar   |
| `print-severity yes`| Prefijo con nivel de severidad         | Útil para alertas   |

> **Nota sobre permisos**: BIND ejecuta como usuario `named` (RHEL) o `bind`
> (Debian). El directorio de logs debe pertenecer a ese usuario:
> ```
> # Debian/Ubuntu
> mkdir -p /var/log/named
> chown bind:bind /var/log/named
> chmod 750 /var/log/named
>
> # RHEL/Fedora
> mkdir -p /var/log/named
> chown named:named /var/log/named
> chmod 750 /var/log/named
> ```
>
> En sistemas con **SELinux** (RHEL), el directorio necesita el contexto correcto:
> ```
> semanage fcontext -a -t named_log_t "/var/log/named(/.*)?"
> restorecon -Rv /var/log/named
> ```

### 2.2 Canal tipo syslog

Envía mensajes al syslog del sistema usando facilities estándar:

```
logging {
    channel security_syslog {
        syslog auth;            // facility: auth
        severity warning;
        print-category yes;
    };
};
```

**Facilities disponibles:** `kern`, `user`, `mail`, `daemon`, `auth`, `syslog`,
`lpr`, `news`, `uucp`, `cron`, `authpriv`, `ftp`, `local0`–`local7`.

La elección de facility determina a qué archivo llegan los mensajes según la
configuración de rsyslog/syslog-ng. Usar `local0`–`local7` permite separar los
logs de BIND sin interferir con otras aplicaciones:

```
// En named.conf
channel dns_syslog {
    syslog local3;
    severity info;
};

// En /etc/rsyslog.d/named.conf
// local3.*    /var/log/named/dns-syslog.log
```

### 2.3 Canal tipo stderr

Envía a la salida de error estándar. Útil solo para depuración manual cuando se
ejecuta `named` en primer plano (`named -g`):

```
logging {
    channel debug_stderr {
        stderr;
        severity debug 3;
        print-time yes;
        print-category yes;
        print-severity yes;
    };
};
```

### 2.4 Canal null

Descarta mensajes. Útil para silenciar categorías ruidosas que no interesa
registrar:

```
logging {
    category lame-servers { null; };
};
```

### Niveles de severity

Los niveles siguen la jerarquía estándar de syslog. Cada nivel incluye todos los
superiores (más severos):

```
    critical  ─── Errores fatales, BIND podría detenerse
        │
    error     ─── Errores que impiden operación normal
        │
    warning   ─── Situaciones anómalas pero recuperables
        │
    notice    ─── Eventos significativos normales
        │
    info      ─── Información operacional general
        │
    debug N   ─── Niveles de depuración (0-99)
        │
    dynamic   ─── Nivel controlado en tiempo real por rndc
```

El nivel `dynamic` es especial: el severity efectivo se controla con
`rndc trace N` sin reiniciar BIND, lo que permite activar debug temporalmente en
producción.

---

## 3. Categorías (categories)

Las categorías agrupan mensajes por funcionalidad. BIND define más de 30
categorías. Las más relevantes para administración:

### 3.1 Categorías esenciales

| Categoría       | Mensajes que genera                                        |
|-----------------|-----------------------------------------------------------|
| `default`       | Todo lo que no tiene categoría explícita asignada          |
| `general`       | Mensajes que no encajan en ninguna otra categoría          |
| `queries`       | Cada consulta DNS recibida (requiere `querylog` activo)    |
| `query-errors`  | Consultas que generaron error (SERVFAIL, etc.)             |
| `security`      | Operaciones aprobadas/denegadas por ACLs                   |
| `config`        | Procesamiento de archivos de configuración                 |
| `database`      | Almacenamiento interno de zonas y cache                    |

### 3.2 Categorías de transferencia

| Categoría       | Mensajes que genera                                        |
|-----------------|-----------------------------------------------------------|
| `xfer-in`       | Transferencias de zona recibidas (como secundario)         |
| `xfer-out`      | Transferencias de zona enviadas (como primario)            |
| `notify`        | Mensajes NOTIFY enviados y recibidos                       |

### 3.3 Categorías de resolución

| Categoría       | Mensajes que genera                                        |
|-----------------|-----------------------------------------------------------|
| `resolver`      | Proceso de resolución recursiva                            |
| `cname`         | Procesamiento de cadenas CNAME                             |
| `delegation-only` | Respuestas filtradas por delegation-only                 |
| `lame-servers`  | Servidores que no responden autoritativamente para su zona |
| `edns-disabled` | Servidores con los que se deshabilitó EDNS                 |

### 3.4 Categorías de DNSSEC

| Categoría       | Mensajes que genera                                        |
|-----------------|-----------------------------------------------------------|
| `dnssec`        | Validación y procesamiento DNSSEC                          |
| `rpz`           | Response Policy Zones                                      |
| `rate-limit`    | Eventos de rate limiting (RRL)                             |

### 3.5 Categorías operacionales

| Categoría       | Mensajes que genera                                        |
|-----------------|-----------------------------------------------------------|
| `network`       | Operaciones de red (apertura de sockets, listeners)        |
| `update`        | Actualizaciones dinámicas (RFC 2136)                       |
| `update-security` | Autorización de actualizaciones dinámicas               |
| `client`        | Procesamiento de solicitudes de clientes                   |
| `dispatch`      | Dispatch de paquetes a módulos internos                    |

### La categoría default

`default` actúa como catch-all. Si no se asigna una categoría a un canal, sus
mensajes van a donde apunte `default`. Si tampoco se configura `default`, todo va
a `default_syslog` y `default_debug`.

```
// Patrón recomendado: configurar default explícitamente
category default { main_log; };
```

> **Advertencia**: Si defines la categoría `default` apuntando solo a un archivo,
> pierdes la salida a syslog. Si necesitas ambos, lista los dos canales:
> ```
> category default { main_log; default_syslog; };
> ```

---

## 4. Configuraciones completas por escenario

### 4.1 Servidor autoritativo en producción

Prioriza seguridad, transferencias y errores de query:

```
logging {
    // --- Canales ---
    channel main_log {
        file "/var/log/named/named.log"
            versions 10 size 50m;
        severity info;
        print-time iso8601;
        print-category yes;
        print-severity yes;
    };

    channel security_log {
        file "/var/log/named/security.log"
            versions 5 size 20m;
        severity info;
        print-time iso8601;
        print-severity yes;
    };

    channel xfer_log {
        file "/var/log/named/xfer.log"
            versions 5 size 20m;
        severity info;
        print-time iso8601;
        print-category yes;
        print-severity yes;
    };

    channel query_errors_log {
        file "/var/log/named/query-errors.log"
            versions 5 size 20m;
        severity info;
        print-time iso8601;
        print-severity yes;
    };

    // --- Categorías ---
    category default     { main_log; };
    category general     { main_log; };
    category config      { main_log; };
    category security    { security_log; };
    category xfer-in     { xfer_log; };
    category xfer-out    { xfer_log; };
    category notify      { xfer_log; };
    category query-errors { query_errors_log; };

    // Silenciar ruido innecesario en autoritativo
    category lame-servers  { null; };
    category edns-disabled { null; };
    category resolver      { null; };
};
```

**Por qué no se activa `queries`**: En un autoritativo público, el log de queries
puede generar gigabytes por hora. Se activa solo temporalmente para diagnóstico
con `rndc querylog on`.

### 4.2 Servidor recursivo/caching

El logging de queries es más viable (menos volumen) y más útil (visibilidad de
qué resuelven los clientes):

```
logging {
    channel main_log {
        file "/var/log/named/named.log"
            versions 10 size 100m;
        severity info;
        print-time iso8601;
        print-category yes;
        print-severity yes;
    };

    channel query_log {
        file "/var/log/named/query.log"
            versions 20 size 200m;
        severity info;
        print-time iso8601;
    };

    channel security_log {
        file "/var/log/named/security.log"
            versions 5 size 20m;
        severity info;
        print-time iso8601;
        print-severity yes;
    };

    channel dnssec_log {
        file "/var/log/named/dnssec.log"
            versions 5 size 20m;
        severity info;
        print-time iso8601;
        print-severity yes;
    };

    channel resolver_log {
        file "/var/log/named/resolver.log"
            versions 5 size 50m;
        severity warning;        // solo problemas, no cada resolución
        print-time iso8601;
        print-severity yes;
    };

    // --- Categorías ---
    category default      { main_log; };
    category general      { main_log; };
    category config       { main_log; };
    category queries      { query_log; };
    category query-errors { query_log; };
    category security     { security_log; };
    category dnssec       { dnssec_log; };
    category resolver     { resolver_log; };
    category lame-servers { resolver_log; };
};
```

### 4.3 Configuración mínima para laboratorio

Para aprender y depurar, registrar todo con máxima visibilidad:

```
logging {
    channel all_log {
        file "/var/log/named/all.log"
            versions 3 size 20m;
        severity dynamic;       // controlable con rndc trace
        print-time iso8601;
        print-category yes;
        print-severity yes;
    };

    category default { all_log; };
};
```

Con `severity dynamic`, se empieza en `info` y se sube el nivel de debug bajo
demanda:

```bash
rndc trace 3       # activar debug nivel 3
# ... reproducir el problema ...
rndc notrace       # volver a info
```

---

## 5. Modo debug

### 5.1 Debug en tiempo real con rndc

La forma preferida de depurar en producción sin reiniciar:

```bash
# Activar debug nivel 1 (básico)
rndc trace

# Activar debug nivel específico
rndc trace 5

# Ver nivel actual
rndc status | grep 'debug level'

# Desactivar debug
rndc notrace
```

Cada nivel de debug incrementa la verbosidad. Los niveles útiles:

| Nivel   | Información adicional                                    |
|---------|----------------------------------------------------------|
| 1       | Eventos básicos de operación                             |
| 3       | Proceso de resolución recursiva paso a paso              |
| 5       | Detalles de procesamiento de paquetes                    |
| 10      | Operaciones de socket y red                              |
| 50+     | Internos del motor (solo para desarrolladores de BIND)   |

> **Impacto en rendimiento**: niveles superiores a 3 pueden degradar
> significativamente el rendimiento. En producción, no exceder nivel 3 y solo
> por periodos breves.

### 5.2 Ejecución en primer plano

Para depuración durante instalación o cambios de configuración:

```bash
# Detener el servicio
systemctl stop named

# Ejecutar en primer plano con debug
named -g -d 3
# -g: primer plano, logs a stderr
# -d 3: nivel de debug 3

# Alternativa: solo verificar configuración y salir
named -g -d 1 -p 5300
# -p 5300: puerto alternativo (no requiere root)
```

Cuando BIND se ejecuta con `-g`, los mensajes se ven directamente en terminal
con colores según severity, facilitando la identificación visual de errores.

### 5.3 Query logging

El registro de cada consulta DNS recibida se controla independientemente:

```bash
# Activar query log
rndc querylog on

# Desactivar
rndc querylog off

# Verificar estado
rndc status | grep 'query logging'
```

**Formato de una línea de query log:**

```
21-Mar-2026 10:15:33.456 queries: info: client @0x7f3a 192.168.1.50#43210
  (www.ejemplo.com): query: www.ejemplo.com IN A +E(0)K (10.0.0.53)
```

Desglose del formato:

```
┌─ Timestamp
│                            ┌─ IP del cliente y puerto origen
│                            │                  ┌─ Nombre consultado
│                            │                  │
21-Mar-2026 10:15:33.456     192.168.1.50#43210 www.ejemplo.com
                                                │
  query: www.ejemplo.com IN A +E(0)K (10.0.0.53)
         │                │  │ │    │  │
         │                │  │ │    │  └─ Dirección que respondió
         │                │  │ │    └─ Cookie presente
         │                │  │ └─ EDNS versión 0
         │                │  └─ Recursión solicitada (+) o no (-)
         │                └─ Tipo de registro
         └─ QNAME
```

**Flags en el query log:**

| Flag | Significado                          |
|------|--------------------------------------|
| `+`  | Recursion Desired (RD) activado      |
| `-`  | RD no activado                       |
| `E`  | EDNS presente                        |
| `S`  | Consulta firmada (TSIG)              |
| `K`  | Cookie DNS presente                  |
| `D`  | DNSSEC OK (DO bit)                   |
| `C`  | Checking Disabled (CD)               |
| `T`  | TCP (ausente = UDP)                  |

---

## 6. Análisis de logs en producción

### 6.1 Patrones de búsqueda con herramientas estándar

**Consultas más frecuentes** (top 10 dominios):

```bash
awk '{print $13}' /var/log/named/query.log | \
    sort | uniq -c | sort -rn | head -10
```

**Clientes más activos** (top 10 IPs):

```bash
grep 'query:' /var/log/named/query.log | \
    awk -F'#' '{print $1}' | awk '{print $NF}' | \
    sort | uniq -c | sort -rn | head -10
```

**Consultas de un tipo específico** (ej. MX):

```bash
grep 'IN MX' /var/log/named/query.log | tail -20
```

**Errores SERVFAIL** (indican problemas de resolución):

```bash
grep 'SERVFAIL' /var/log/named/query-errors.log | \
    awk '{print $13}' | sort | uniq -c | sort -rn | head -10
```

**Transferencias de zona fallidas:**

```bash
grep -i 'transfer.*failed\|refused' /var/log/named/xfer.log
```

**Intentos de acceso denegados:**

```bash
grep 'denied' /var/log/named/security.log | tail -20
```

### 6.2 Detección de anomalías

**Tunneling DNS** — consultas anormalmente largas que pueden indicar exfiltración:

```bash
awk '{print $13}' /var/log/named/query.log | \
    awk 'length > 60' | sort | uniq -c | sort -rn | head -10
```

**Consultas a dominios inexistentes (NXDOMAIN masivo)** — puede indicar malware
buscando su C&C:

```bash
grep 'NXDOMAIN' /var/log/named/query-errors.log | \
    awk '{print $13}' | \
    awk -F. '{print $(NF-1)"."$NF}' | \
    sort | uniq -c | sort -rn | head -10
```

**Picos de queries por minuto:**

```bash
awk '{print substr($1,1,16)}' /var/log/named/query.log | \
    uniq -c | sort -rn | head -10
```

### 6.3 Rotación con logrotate

Aunque BIND tiene rotación interna (`versions`/`size`), en producción es común
complementar o reemplazar con logrotate para integración con políticas del
sistema:

```
# /etc/logrotate.d/named
/var/log/named/*.log {
    daily
    missingok
    rotate 14
    compress
    delaycompress
    notifempty
    create 0640 named named     # bind:bind en Debian
    sharedscripts
    postrotate
        /usr/sbin/rndc reopen 2>/dev/null || true
    endscript
}
```

Puntos clave:

- **`rndc reopen`** en el `postrotate` indica a BIND que cierre y reabra sus
  archivos de log, escribiendo en los archivos nuevos tras la rotación
- Si se usa logrotate, configurar los canales de BIND **sin** `versions`/`size`
  para evitar doble rotación
- `delaycompress` deja el archivo más reciente sin comprimir para facilitar
  análisis inmediato

---

## 7. Integración con sistemas de logging

### 7.1 journald (systemd)

En sistemas con systemd, `named` envía automáticamente a journald además de sus
logs propios:

```bash
# Ver logs de BIND en journald
journalctl -u named          # RHEL
journalctl -u named          # Debian (bind9.service redirige)

# Seguir en tiempo real
journalctl -u named -f

# Solo errores de la última hora
journalctl -u named --since "1 hour ago" -p err

# Exportar a archivo para análisis
journalctl -u named --since today --no-pager > /tmp/named-today.log
```

### 7.2 rsyslog — enrutamiento avanzado

Si BIND envía a syslog con facility `local3`, rsyslog puede enrutar a archivos
separados y a sistemas remotos:

```
# /etc/rsyslog.d/10-named.conf

# Log local separado
local3.*    /var/log/named/dns-syslog.log

# Reenvío a servidor central (TCP para fiabilidad)
local3.*    @@logserver.ejemplo.com:514

# No duplicar en messages/syslog
local3.*    stop
```

### 7.3 Formato JSON para pipelines

BIND 9.16+ puede emitir logs en formato JSON, ideal para Elasticsearch/Loki/Splunk:

```
// En named.conf — requiere BIND compilado con libjson-c
logging {
    channel json_log {
        file "/var/log/named/named.json"
            versions 10 size 100m;
        severity info;
        print-time iso8601;
    };
};
```

Para parsear los query logs estándar hacia JSON con herramientas externas:

```bash
# Ejemplo con jq y awk — convertir query log a JSON Lines
tail -f /var/log/named/query.log | \
    awk '{printf "{\"time\":\"%s %s\",\"client\":\"%s\",\"query\":\"%s\",\"type\":\"%s\"}\n", \
    $1, $2, $6, $8, $10}'
```

### 7.4 dnstap — captura estructurada

Para análisis de alto rendimiento, `dnstap` captura queries y respuestas en
formato Protocol Buffers, con impacto mínimo en rendimiento comparado con query
logging textual:

```
// Compilar BIND con --enable-dnstap
// En named.conf
options {
    dnstap {
        auth;              // queries/respuestas autoritativas
        resolver;          // queries/respuestas recursivas
        client;            // queries de clientes
        forwarder;         // queries reenviadas
    };
    dnstap-output file "/var/run/named/dnstap.sock";
    // O directamente a archivo:
    // dnstap-output file "/var/log/named/dnstap.bin";
};
```

Lectura de capturas dnstap:

```bash
# Instalar herramienta de lectura
# Debian: apt install dnstap-ldns
# RHEL: dnf install dnstap-read (incluido con bind-utils)

# Leer archivo binario
dnstap-read /var/log/named/dnstap.bin

# Tiempo real desde socket con fstrm_capture
fstrm_capture -t protobuf:dnstap.Dnstap \
    -u /var/run/named/dnstap.sock \
    -w /tmp/capture.dnstap
```

---

## 8. Errores comunes

### Error 1: Permisos en el directorio de logs

```
isc_stdio_open '/var/log/named/query.log' failed: permission denied
```

**Causa**: El directorio no pertenece al usuario de BIND o SELinux bloquea.

**Solución**:

```bash
# Permisos POSIX
chown named:named /var/log/named    # bind:bind en Debian
chmod 750 /var/log/named

# SELinux (RHEL)
semanage fcontext -a -t named_log_t "/var/log/named(/.*)?"
restorecon -Rv /var/log/named
```

### Error 2: Categoría asignada a canal inexistente

```
/etc/named.conf:85: unknown channel 'query_log'
```

**Causa**: La categoría referencia un canal que no fue declarado, o hay un typo
en el nombre. Los canales deben declararse **antes** de las categorías dentro del
bloque `logging {}`.

**Solución**: Verificar que el nombre del canal coincida exactamente entre su
declaración y su uso en la categoría.

### Error 3: Query log activado consume todo el disco

**Causa**: En un servidor con alto volumen, query logging sin rotación adecuada
puede llenar el disco en horas.

**Predicción**: Un servidor recursivo con 1000 queries/segundo genera
aproximadamente 200 MB/hora de query log.

**Solución**: Dimensionar `versions` y `size` adecuadamente, o usar logrotate.
En producción, preferir `rndc querylog on` temporalmente en vez de dejarlo activo
permanentemente.

### Error 4: Logs en syslog desaparecen tras configurar logging {}

**Causa**: Al definir un bloque `logging {}`, el comportamiento por defecto
(enviar a syslog) se reemplaza completamente. Si no se incluye un canal syslog
explícito, se pierde la integración.

**Solución**: Si se necesita mantener syslog, incluir explícitamente:

```
category default { main_log; default_syslog; };
```

### Error 5: severity dynamic no muestra debug sin rndc trace

**Causa**: `severity dynamic` comienza en nivel `info`. El debug no se activa
solo por declarar `dynamic` — requiere una activación explícita.

**Solución**: Es el comportamiento esperado. Usar `rndc trace N` para activar
el nivel deseado. Si se necesita debug permanente, usar `severity debug 3`
directamente (no recomendado en producción).

---

## 9. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────┐
│                    BIND LOGGING — REFERENCIA RÁPIDA             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ESTRUCTURA BÁSICA:                                             │
│  logging {                                                      │
│      channel NOMBRE { ... };     // dónde                       │
│      category NOMBRE { CANAL; }; // qué → dónde                │
│  };                                                             │
│                                                                 │
│  TIPOS DE CANAL:                                                │
│  file "PATH" versions N size Nm;  // archivo con rotación       │
│  syslog FACILITY;                 // syslog (daemon, local0..7) │
│  stderr;                          // salida estándar error      │
│  null;                            // descartar                  │
│                                                                 │
│  OPCIONES DE CANAL:                                             │
│  severity info|warning|error|debug N|dynamic;                   │
│  print-time yes|iso8601;                                        │
│  print-category yes;                                            │
│  print-severity yes;                                            │
│                                                                 │
│  CATEGORÍAS CLAVE:                                              │
│  queries ............ cada consulta DNS recibida                │
│  query-errors ....... consultas con error                       │
│  security ........... ACL allow/deny                            │
│  xfer-in/xfer-out ... transferencias de zona                    │
│  notify ............. mensajes NOTIFY                           │
│  dnssec ............. validación DNSSEC                         │
│  resolver ........... resolución recursiva                      │
│  config ............. procesamiento de configuración            │
│  default ............ catch-all                                 │
│  lame-servers ....... delegaciones incorrectas                  │
│                                                                 │
│  DEBUG EN TIEMPO REAL:                                          │
│  rndc trace [N]        activar debug nivel N (defecto: +1)      │
│  rndc notrace          desactivar debug                         │
│  rndc querylog on      activar log de queries                   │
│  rndc querylog off     desactivar log de queries                │
│  rndc status           ver niveles actuales                     │
│  named -g -d N         ejecutar en primer plano con debug       │
│                                                                 │
│  ROTACIÓN:                                                      │
│  Interna: file "x" versions 5 size 50m;                         │
│  Externa: logrotate + rndc reopen (en postrotate)               │
│                                                                 │
│  VERIFICAR LOGS:                                                │
│  journalctl -u named -f              seguir en tiempo real      │
│  tail -f /var/log/named/named.log    archivo directo            │
│  grep 'denied' security.log         buscar denegaciones         │
│  grep 'SERVFAIL' query-errors.log   buscar fallos               │
│                                                                 │
│  SEVERITIES (de mayor a menor):                                 │
│  critical > error > warning > notice > info > debug 0-99        │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 10. Ejercicios

### Ejercicio 1: Configuración de logging separado por función

Configura el bloque `logging {}` de un servidor BIND autoritativo que cumpla:

1. Canal `general_log` a `/var/log/named/general.log` con rotación 7 versiones,
   máximo 30 MB, severity `info`, con timestamp ISO 8601, categoría y severity.
2. Canal `security_log` a `/var/log/named/security.log`, 10 versiones, 10 MB,
   severity `info`.
3. Canal `xfer_log` a `/var/log/named/xfer.log`, 5 versiones, 20 MB.
4. Asignar: `default` y `general` a `general_log`; `security` a `security_log`;
   `xfer-in`, `xfer-out` y `notify` a `xfer_log`; `lame-servers` a `null`.
5. Crear el directorio con permisos adecuados para Debian.
6. Verificar la configuración con `named-checkconf`.
7. Aplicar sin reiniciar el servicio (¿qué comando usarías?).

**Pregunta de predicción**: Si olvidas crear el directorio `/var/log/named` antes
de recargar BIND, ¿qué sucederá? ¿Se detendrá BIND o continuará funcionando?

### Ejercicio 2: Diagnóstico con query logging temporal

Escenario: Los usuarios reportan que `intranet.empresa.local` no resuelve.
Diagnostica usando logs:

1. Verifica que query logging esté desactivado (estado normal en producción).
2. Actívalo temporalmente con `rndc`.
3. Desde otro terminal, realiza la consulta fallida: `dig intranet.empresa.local @localhost`.
4. Busca en el query log la consulta recibida. Identifica: IP del cliente, tipo
   de consulta, flags.
5. Activa debug nivel 3 para ver el proceso de resolución.
6. Repite la consulta y busca en los logs la razón del fallo.
7. Desactiva query logging y debug. Verifica con `rndc status`.
8. Escribe un one-liner que cuente cuántas queries se registraron durante tu
   sesión de diagnóstico.

**Pregunta de predicción**: Si activas `rndc trace 3` pero tus canales usan
`severity info` en vez de `severity dynamic`, ¿verás los mensajes de debug?
¿Por qué?

### Ejercicio 3: Rotación externa con logrotate

Configura logging de BIND con rotación delegada a logrotate:

1. Modifica la configuración de logging de BIND para que los canales **no** usen
   `versions` ni `size` (rotación delegada a logrotate).
2. Crea el archivo `/etc/logrotate.d/named` que:
   - Rote diariamente los archivos en `/var/log/named/*.log`.
   - Mantenga 30 días de retención.
   - Comprima los archivos rotados (excepto el más reciente).
   - Ejecute `rndc reopen` después de rotar.
   - Use los permisos correctos para el usuario de BIND en tu distribución.
3. Prueba la configuración de logrotate sin ejecutarla realmente (`-d`).
4. Fuerza una rotación manual y verifica que BIND sigue escribiendo en el archivo
   nuevo.
5. Compara las ventajas de rotación interna vs logrotate.

**Pregunta de reflexión**: ¿Qué sucede si logrotate rota el archivo pero el
`postrotate` con `rndc reopen` falla (por ejemplo, porque rndc.key tiene
permisos incorrectos)? ¿Dónde escribiría BIND sus logs?

---

> **Fin del Capítulo 2 — Servidor DNS (BIND)**. Siguiente capítulo: C03 —
> Servidor Web (Sección 1: Apache HTTP Server, T01 — Instalación y estructura).

