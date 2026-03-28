# rndc — Control remoto del servidor DNS

## Índice

1. [Qué es rndc](#qué-es-rndc)
2. [Arquitectura y autenticación](#arquitectura-y-autenticación)
3. [Configuración](#configuración)
4. [Comandos de estado](#comandos-de-estado)
5. [Comandos de recarga](#comandos-de-recarga)
6. [Comandos de caché](#comandos-de-caché)
7. [Comandos de logging](#comandos-de-logging)
8. [Comandos de zonas](#comandos-de-zonas)
9. [Comandos DNSSEC](#comandos-dnssec)
10. [Acceso remoto](#acceso-remoto)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## Qué es rndc

`rndc` (Remote Name Daemon Control) es la herramienta de línea de comandos para controlar un servidor BIND en ejecución. Permite recargar configuración, gestionar zonas, manipular el caché y diagnosticar problemas **sin reiniciar** el servicio.

```
    ┌──────────┐     TCP :953      ┌──────────────┐
    │   rndc   │────────────────── │   named      │
    │ (cliente)│  autenticado con  │  (servidor)  │
    │          │  clave TSIG       │              │
    └──────────┘                   └──────────────┘
```

La diferencia con `systemctl`:

| Acción | systemctl | rndc |
|---|---|---|
| Reiniciar todo | `systemctl restart named` | — |
| Recargar config | `systemctl reload named` | `rndc reload` |
| Recargar una zona | — | `rndc reload zona` |
| Vaciar caché | — | `rndc flush` |
| Ver estado | `systemctl status named` | `rndc status` |
| Activar query log | — | `rndc querylog on` |
| Estado de zona | — | `rndc zonestatus zona` |

`rndc` ofrece un control mucho más granular que `systemctl`. Y lo más importante: puede operar de forma remota (desde otra máquina), mientras que `systemctl` solo funciona localmente.

---

## Arquitectura y autenticación

### Protocolo

rndc se comunica con named a través de TCP en el puerto 953 (por defecto). La comunicación está autenticada con TSIG (Transaction SIGnature), una clave compartida simétrica:

```
    rndc                                named
    ┌──────────┐                       ┌──────────┐
    │          │                       │          │
    │ Tiene:   │                       │ Tiene:   │
    │ rndc.key │                       │ rndc.key │
    │          │                       │          │
    │ Comando: │── TCP :953 ──────────►│          │
    │ "reload" │   TSIG autenticado    │ Ejecuta  │
    │          │                       │ reload   │
    │          │◄─ respuesta ──────────│          │
    │          │                       │          │
    └──────────┘                       └──────────┘
```

La clave TSIG garantiza que:
- Solo clientes con la clave correcta pueden controlar named
- Los comandos no pueden ser falsificados (integridad)
- No hay replay attacks (incluye timestamp)

### Archivos de clave

```bash
# La clave compartida está en:
/etc/bind/rndc.key      # Debian
/etc/rndc.key           # RHEL

# Formato:
cat /etc/bind/rndc.key
# key "rndc-key" {
#     algorithm hmac-sha256;
#     secret "base64-encoded-secret==";
# };
```

---

## Configuración

### Configuración por defecto (generada automáticamente)

La mayoría de instalaciones generan la clave durante la instalación:

```bash
# Verificar si existe
ls -la /etc/bind/rndc.key   # Debian
ls -la /etc/rndc.key         # RHEL

# Si no existe, generarla:
sudo rndc-confgen -a
# wrote key file "/etc/bind/rndc.key"
# Genera una clave aleatoria hmac-sha256
```

### named.conf — bloque controls

El bloque `controls` en named.conf define cómo named acepta conexiones de rndc:

```c
// Configuración implícita por defecto (cuando no hay bloque controls):
// named lee rndc.key y acepta conexiones desde localhost:953

// Configuración explícita:
controls {
    inet 127.0.0.1 port 953
        allow { 127.0.0.1; }
        keys { "rndc-key"; };
};

// Incluir la clave
include "/etc/bind/rndc.key";
```

### rndc.conf — configuración del cliente

El cliente rndc puede configurarse explícitamente:

```bash
# /etc/bind/rndc.conf (o ~/.rndc.conf)
options {
    default-key "rndc-key";
    default-server 127.0.0.1;
    default-port 953;
};

key "rndc-key" {
    algorithm hmac-sha256;
    secret "la-misma-clave-que-en-named==";
};

server 127.0.0.1 {
    key "rndc-key";
};
```

En la práctica, si rndc y named están en la misma máquina, el archivo `rndc.key` compartido es suficiente — no necesitas `rndc.conf`.

### Generar una nueva clave

```bash
# Generar clave automáticamente (escribe /etc/bind/rndc.key)
sudo rndc-confgen -a -A hmac-sha256 -b 256
# -a: escribir solo el archivo de clave (no la config completa)
# -A: algoritmo
# -b: bits de la clave

# Generar configuración completa (muestra en stdout)
sudo rndc-confgen
# Muestra:
# 1. El rndc.conf completo
# 2. Lo que añadir a named.conf
# Copia cada parte al archivo correspondiente

# Regenerar si sospechas que la clave está comprometida:
sudo rndc-confgen -a -A hmac-sha256
sudo systemctl restart named    # necesita restart, no reload
```

---

## Comandos de estado

### rndc status

```bash
sudo rndc status
# version: BIND 9.18.28 (Extended Support Version) ...
# running on localhost: Linux x86_64 6.19.8-200.fc43.x86_64
# boot time: Fri, 21 Mar 2026 10:00:00 GMT
# last configured: Fri, 21 Mar 2026 14:30:00 GMT
# configuration file: /etc/bind/named.conf
# CPUs found: 4
# worker threads: 4
# UDP listeners per interface: 4
# number of zones: 5 (0 automatic)
# debug level: 0
# xfers running: 0
# xfers deferred: 0
# soa queries in progress: 0
# query logging is OFF
# recursive clients: 0/900/1000
# tcp clients: 0/150
# TCP high-water: 3
# server is up and running
```

Campos importantes:

| Campo | Significado |
|---|---|
| `last configured` | Última vez que se recargó la configuración |
| `number of zones` | Zonas cargadas |
| `debug level` | Nivel de debug (0 = normal) |
| `query logging` | Si está registrando cada consulta |
| `recursive clients` | Consultas recursivas activas/softlimit/hardlimit |
| `tcp clients` | Conexiones TCP activas/límite |
| `xfers running` | Transferencias de zona en progreso |

### rndc zonestatus

```bash
sudo rndc zonestatus ejemplo.com
# name: ejemplo.com
# type: master
# files: /etc/bind/zones/db.ejemplo.com
# serial: 2026032101
# nodes: 8
# last loaded: Fri, 21 Mar 2026 14:30:00 GMT
# secure: no                        ← DNSSEC no activo
# dynamic: no                       ← no acepta actualizaciones dinámicas
# reconfigurable via modzone: no
# key-directory: /etc/bind/keys
# inline-signing: no
# next refresh: UNSET
# expires: UNSET

# Para zonas secundarias, muestra además:
# next refresh: Fri, 21 Mar 2026 15:30:00 GMT   ← cuándo revisa al primario
# expires: Fri, 28 Mar 2026 14:30:00 GMT         ← cuándo deja de servir
```

### rndc recursing

```bash
# Ver consultas recursivas en curso
sudo rndc recursing
# Muestra las consultas que named está resolviendo activamente
# Útil para detectar consultas lentas o atascadas
```

---

## Comandos de recarga

### rndc reload

```bash
# Recargar toda la configuración y todas las zonas
sudo rndc reload
# server reload successful

# Recargar solo una zona específica (más rápido, menos disruptivo)
sudo rndc reload ejemplo.com
# zone reload up-to-date

# Si el serial no cambió:
sudo rndc reload ejemplo.com
# zone reload up-to-date    ← no recarga porque el serial es igual
```

### rndc reconfig

```bash
# Recargar SOLO la configuración (named.conf), no las zonas
sudo rndc reconfig
# Útil cuando añadiste una zona nueva en named.conf
# pero no quieres recargar todas las zonas existentes
```

Diferencia:

```
    rndc reload:
    1. Re-lee named.conf
    2. Re-lee TODOS los archivos de zona
    3. Recarga zonas cuyo serial cambió

    rndc reconfig:
    1. Re-lee named.conf
    2. Carga zonas NUEVAS (recién añadidas)
    3. NO re-lee zonas existentes

    rndc reload zona:
    1. NO re-lee named.conf
    2. Re-lee SOLO ese archivo de zona
    3. Recarga si el serial cambió
```

### rndc retransfer

```bash
# Forzar transferencia de zona desde el primario (en un secundario)
sudo rndc retransfer ejemplo.com
# Ignora el serial — transfiere la zona completa
# Útil cuando sospechas que el secundario tiene datos corruptos

# Diferencia con reload en secundarios:
# reload: verifica serial, solo transfiere si el primario tiene serial mayor
# retransfer: transfiere incondicionalmente
```

### rndc refresh

```bash
# Verificar si hay actualizaciones en el primario (en un secundario)
sudo rndc refresh ejemplo.com
# Consulta el SOA del primario y compara seriales
# Si el primario tiene serial mayor → inicia transferencia
# Si son iguales → no hace nada
```

### rndc freeze / thaw

```bash
# Congelar una zona (prevenir actualizaciones dinámicas)
sudo rndc freeze ejemplo.com
# La zona se vuelve read-only
# Ahora puedes editar el archivo de zona manualmente sin conflictos

# Descongelar (re-habilitar actualizaciones dinámicas)
sudo rndc thaw ejemplo.com
# Recarga la zona y re-habilita DDNS

# Flujo para editar zonas dinámicas:
# 1. rndc freeze ejemplo.com
# 2. Editar archivo de zona (incrementar serial)
# 3. rndc thaw ejemplo.com
```

---

## Comandos de caché

### rndc flush

```bash
# Vaciar todo el caché DNS
sudo rndc flush
# Todas las entradas cacheadas se eliminan
# Las próximas consultas requieren resolución completa

# Vaciar caché de una vista específica
sudo rndc flush internal
# Solo limpia el caché de la vista "internal"
```

### rndc flushname

```bash
# Vaciar un nombre específico del caché
sudo rndc flushname www.google.com
# Solo elimina las entradas de www.google.com
# Mucho más preciso que flush total

# Vaciar un dominio completo
sudo rndc flushtree google.com
# Elimina google.com y todos sus subdominios del caché
```

### rndc dumpdb

```bash
# Volcar el caché a un archivo (para inspección)
sudo rndc dumpdb -cache
# Escribe en:
# Debian: /var/cache/bind/named_dump.db
# RHEL: /var/named/data/cache_dump.db

# Ver qué hay en el caché para un dominio:
sudo grep "google.com" /var/cache/bind/named_dump.db

# Volcar solo las zonas autoritativas
sudo rndc dumpdb -zones

# Volcar todo (caché + zonas)
sudo rndc dumpdb -all
```

El dump del caché tiene un formato específico:

```
; Cache dump
;
$ORIGIN .
google.com.         170  IN  A     142.250.80.46
                    170  IN  RRSIG A 8 2 300 ...
www.google.com.     120  IN  A     142.250.80.36
;                    ↑ TTL restante (decrece con el tiempo)
```

### rndc secroots

```bash
# Ver los trust anchors y negative trust anchors activos
sudo rndc secroots
# Escribe en el archivo de dump
# Muestra:
# - Trust anchors para DNSSEC (clave de la raíz)
# - Negative trust anchors (NTAs) activos
```

---

## Comandos de logging

### rndc querylog

```bash
# Activar logging de cada consulta DNS
sudo rndc querylog on
# Ahora cada consulta se registra en los logs

# Ver las consultas en tiempo real:
sudo journalctl -u named -f
# named[1234]: client @0x... 192.168.1.100#54321 (www.google.com):
#   query: www.google.com IN A +E(0)K (192.168.1.10)
#          ↑ qué preguntaron  ↑ tipo  ↑ flags   ↑ interfaz que recibió

# Desactivar (produce muchos logs en servidores concurridos)
sudo rndc querylog off

# Verificar si está activo
sudo rndc status | grep "query logging"
# query logging is ON
```

> **Cuidado**: en un servidor con tráfico alto, `querylog on` puede generar gigabytes de logs por hora. Usar solo para diagnóstico temporal.

### rndc trace / notrace

```bash
# Incrementar nivel de debug
sudo rndc trace
# debug level is now 1

sudo rndc trace
# debug level is now 2

sudo rndc trace
# debug level is now 3
# (cada invocación incrementa en 1)

# Establecer un nivel específico
sudo rndc trace 5

# Volver a nivel 0 (normal)
sudo rndc notrace
# debug level is now 0
```

Niveles de debug:

| Nivel | Información |
|---|---|
| 0 | Normal (info, warnings, errores) |
| 1 | Información básica de resolución |
| 2 | Detalles de consultas y respuestas |
| 3 | Detalles de funciones internas |
| 4+ | Extremadamente detallado (solo desarrollo) |

---

## Comandos de zonas

### rndc notify

```bash
# Enviar NOTIFY a los secundarios de una zona
sudo rndc notify ejemplo.com
# Fuerza el envío de NOTIFY incluso si notify está deshabilitado
# Los secundarios verificarán el serial y transferirán si es necesario
```

### rndc sync

```bash
# Escribir las zonas dinámicas (journal) al archivo de zona
sudo rndc sync
# Sincroniza el journal con el archivo de zona
# Útil antes de hacer backup de archivos de zona

# Para una zona específica
sudo rndc sync ejemplo.com

# Limpiar el journal después de sincronizar
sudo rndc sync -clean
# Escribe el journal al archivo de zona y elimina el journal
```

### rndc addzone / delzone / modzone

```bash
# Añadir una zona en caliente (sin editar named.conf)
sudo rndc addzone 'nueva.com { type master; file "/etc/bind/zones/db.nueva.com"; };'
# La zona se añade inmediatamente sin recargar
# Se guarda en un archivo NZF para persistir

# Eliminar una zona añadida con addzone
sudo rndc delzone nueva.com

# Modificar una zona existente
sudo rndc modzone 'nueva.com { type master; file "/etc/bind/zones/db.nueva.com.v2"; };'
# Solo funciona con zonas añadidas con addzone o que tienen
# allow-new-zones yes en named.conf
```

> `addzone` requiere `allow-new-zones yes;` en el bloque options de named.conf. Las zonas añadidas así se almacenan en un archivo NZF (New Zone File) separado, no en named.conf.

### rndc signing

```bash
# Ver el estado de firma de una zona
sudo rndc signing -list ejemplo.com
# Muestra las operaciones de firma pendientes o en curso

# Limpiar firmas pendientes
sudo rndc signing -clear all ejemplo.com
```

---

## Comandos DNSSEC

```bash
# Ver estado de claves DNSSEC de una zona
sudo rndc dnssec -status ejemplo.com

# Forzar re-firma de una zona
sudo rndc sign ejemplo.com

# Forzar rotación de una clave específica
sudo rndc dnssec -rollover -key 12345 ejemplo.com

# Crear un negative trust anchor (desactivar validación temporal)
sudo rndc nta ejemplo-roto.com
# Default: 1 hora

# Con duración personalizada
sudo rndc nta -lifetime 7200 ejemplo-roto.com
# 2 horas

# Listar NTAs activos
sudo rndc nta -dump

# Eliminar un NTA
sudo rndc nta -remove ejemplo-roto.com

# Validar una zona (verificar firmas)
sudo rndc validation check ejemplo.com
```

---

## Acceso remoto

### Configurar rndc para acceso desde otra máquina

En el servidor named:

```c
// named.conf

// Clave compartida
key "admin-key" {
    algorithm hmac-sha256;
    secret "clave-secreta-en-base64==";
};

// Aceptar control desde la IP del administrador
controls {
    inet 127.0.0.1 port 953 allow { 127.0.0.1; } keys { "rndc-key"; };
    inet 192.168.1.10 port 953 allow { 192.168.1.100; } keys { "admin-key"; };
    //    ↑ IP del servidor          ↑ IP del admin         ↑ clave del admin
};
```

En la máquina del administrador:

```bash
# /etc/rndc.conf (o ~/.rndc.conf)
key "admin-key" {
    algorithm hmac-sha256;
    secret "clave-secreta-en-base64==";
};

options {
    default-key "admin-key";
    default-server 192.168.1.10;
    default-port 953;
};

server 192.168.1.10 {
    key "admin-key";
};
```

```bash
# Usar rndc remotamente
rndc -s 192.168.1.10 status
rndc -s 192.168.1.10 reload ejemplo.com

# O sin rndc.conf, especificando todo en la línea:
rndc -s 192.168.1.10 -p 953 -y "admin-key:clave-en-base64==" status
```

### Seguridad del acceso remoto

```bash
# Abrir el puerto 953 en el firewall (solo desde IPs de admin)
sudo firewall-cmd --permanent --add-rich-rule='
    rule family="ipv4"
    source address="192.168.1.100"
    port port="953" protocol="tcp"
    accept'
sudo firewall-cmd --reload

# NUNCA exponer rndc a Internet
# Si necesitas control remoto a través de Internet, usa un túnel SSH:
ssh -L 953:127.0.0.1:953 admin@dns-server
rndc -s 127.0.0.1 status
```

### Múltiples claves para diferentes niveles de acceso

BIND no soporta ACLs granulares por comando en rndc nativo. Si necesitas diferentes niveles de acceso, la solución es usar scripts wrapper con sudo:

```bash
# /usr/local/bin/dns-reload
#!/bin/bash
# Solo permite reload de zonas específicas
ALLOWED_ZONES="staging.ejemplo.com dev.ejemplo.com"
ZONE=$1
if echo "$ALLOWED_ZONES" | grep -qw "$ZONE"; then
    sudo rndc reload "$ZONE"
else
    echo "No autorizado para recargar $ZONE"
    exit 1
fi
```

---

## Errores comunes

### 1. rndc: connect failed: connection refused

```bash
sudo rndc status
# rndc: connect failed: 127.0.0.1#953: connection refused

# Causas posibles:
# a) named no está corriendo
sudo systemctl status named

# b) named no tiene bloque controls (no escucha en 953)
sudo ss -tlnp | grep :953
# Si no hay nada → named no tiene controls configurado

# c) rndc.key no existe o tiene permisos incorrectos
ls -la /etc/bind/rndc.key
# Debe ser legible por el usuario que ejecuta rndc

# Solución:
sudo rndc-confgen -a
sudo systemctl restart named
```

### 2. rndc: decode64 failed / authentication failure

```bash
sudo rndc status
# rndc: decode64 failed
# O:
# rndc: 'reload' failed: tsig verify failure

# Causa: la clave en rndc.key no coincide con la de named.conf
# O: la clave tiene formato base64 inválido

# Solución: regenerar
sudo rndc-confgen -a -A hmac-sha256
sudo systemctl restart named    # restart, no reload
```

### 3. Confundir reload con reconfig

```bash
# Escenario: añadiste una zona nueva en named.conf.local
# y editaste una zona existente

# MAL — solo reconfig
sudo rndc reconfig
# Carga la zona nueva pero NO recarga las existentes
# La zona editada sigue con datos viejos

# BIEN — reload (hace ambas cosas)
sudo rndc reload
# Recarga todo: config + zonas nuevas + zonas modificadas

# O selectivamente:
sudo rndc reconfig             # cargar zonas nuevas
sudo rndc reload ejemplo.com   # recargar la zona editada
```

### 4. querylog dejado activo en producción

```bash
# Activaste querylog para diagnosticar un problema
sudo rndc querylog on

# ... resolviste el problema, olvidaste desactivar ...

# Resultado: disco lleno de logs después de horas/días
# Un servidor con 1000 queries/segundo genera ~100MB/hora de logs

# Verificar:
sudo rndc status | grep "query logging"
# query logging is ON  ← ¡apagar!

sudo rndc querylog off
```

### 5. flush sin necesidad (perdiendo el caché útil)

```bash
# Alguien cambió un registro DNS y quieres ver el cambio inmediatamente
# MAL — vaciar TODO el caché
sudo rndc flush
# Pierdes miles de entradas cacheadas, las próximas consultas son lentas

# BIEN — vaciar solo el nombre específico
sudo rndc flushname www.ejemplo.com
# O todo el dominio:
sudo rndc flushtree ejemplo.com
```

---

## Cheatsheet

```bash
# === ESTADO ===
sudo rndc status                         # estado general del servidor
sudo rndc zonestatus zona                # estado de una zona
sudo rndc recursing                      # consultas recursivas activas

# === RECARGA ===
sudo rndc reload                         # recargar config + todas las zonas
sudo rndc reload zona                    # recargar solo una zona
sudo rndc reconfig                       # recargar config (zonas nuevas)
sudo rndc retransfer zona                # forzar transferencia (secundarios)
sudo rndc refresh zona                   # verificar si hay actualización
sudo rndc notify zona                    # enviar NOTIFY a secundarios

# === CACHÉ ===
sudo rndc flush                          # vaciar todo el caché
sudo rndc flushname nombre               # vaciar un nombre específico
sudo rndc flushtree dominio              # vaciar dominio + subdominios
sudo rndc dumpdb -cache                  # volcar caché a archivo
sudo rndc dumpdb -zones                  # volcar zonas a archivo
sudo rndc dumpdb -all                    # volcar todo

# === LOGGING ===
sudo rndc querylog on                    # activar log de queries
sudo rndc querylog off                   # desactivar log de queries
sudo rndc trace                          # incrementar nivel debug (+1)
sudo rndc trace 3                        # establecer nivel debug
sudo rndc notrace                        # volver a debug 0

# === ZONAS DINÁMICAS ===
sudo rndc freeze zona                    # congelar (editar manualmente)
sudo rndc thaw zona                      # descongelar (re-habilitar DDNS)
sudo rndc sync                           # journal → archivo de zona
sudo rndc sync -clean                    # sync + eliminar journal

# === ZONAS EN CALIENTE ===
sudo rndc addzone 'zona { ... };'        # añadir zona sin restart
sudo rndc delzone zona                   # eliminar zona
sudo rndc modzone 'zona { ... };'        # modificar zona

# === DNSSEC ===
sudo rndc sign zona                      # forzar re-firma
sudo rndc dnssec -status zona            # estado de claves
sudo rndc dnssec -rollover -key ID zona  # forzar rotación
sudo rndc nta dominio                    # negative trust anchor (1h)
sudo rndc nta -lifetime 3600 dominio     # NTA con duración
sudo rndc nta -dump                      # listar NTAs
sudo rndc nta -remove dominio            # eliminar NTA

# === CONFIGURACIÓN ===
sudo rndc-confgen -a                     # generar rndc.key
sudo rndc-confgen -a -A hmac-sha256      # con algoritmo específico
sudo rndc-confgen                        # generar config completa

# === REMOTO ===
rndc -s IP status                        # consultar servidor remoto
rndc -s IP -p 953 -y "key:secret" cmd    # sin rndc.conf

# === DIAGNÓSTICO RÁPIDO ===
sudo rndc status | grep "query logging"  # ¿querylog activo?
sudo rndc status | grep "recursive"      # clientes recursivos
sudo rndc status | grep "zones"          # número de zonas
sudo rndc status | grep "configured"     # última recarga
```

---

## Ejercicios

### Ejercicio 1 — Explorar el estado del servidor

1. Ejecuta `rndc status` y anota: versión, número de zonas, nivel de debug, estado de query logging
2. Si tienes zonas configuradas, ejecuta `rndc zonestatus` para una de ellas y anota: tipo, serial, última carga
3. Activa query logging, haz 5 consultas con `dig`, observa los logs y desactiva query logging
4. Compara la información de `rndc status` vs `systemctl status named` — ¿qué información da cada uno que el otro no?

<details>
<summary>Solución</summary>

```bash
# 1. Estado general
sudo rndc status
# Anotar:
# - version: BIND 9.18.x
# - number of zones: X
# - debug level: 0
# - query logging is OFF

# 2. Estado de zona
sudo rndc zonestatus ejemplo.com   # o la zona que tengas
# - type: master
# - serial: 2026032101
# - last loaded: fecha/hora

# 3. Query logging
sudo rndc querylog on
sudo rndc status | grep "query logging"
# query logging is ON

dig @127.0.0.1 google.com
dig @127.0.0.1 github.com
dig @127.0.0.1 example.com
dig @127.0.0.1 isc.org
dig @127.0.0.1 cloudflare.com

sudo journalctl -u named --since "1 min ago" | grep "query:"
# Muestra las 5 consultas con IP del cliente, nombre, tipo, flags

sudo rndc querylog off

# 4. Comparación:
# systemctl status named:
#   - PID, memoria, CPU, tiempo activo
#   - Últimas líneas de log
#   - Estado del servicio systemd (enabled, active)
#
# rndc status:
#   - Versión de BIND
#   - Número de zonas cargadas
#   - Nivel de debug
#   - Query logging activo/inactivo
#   - Clientes recursivos activos
#   - Transferencias en curso
#   - Última reconfiguración
#
# rndc da información específica de DNS que systemctl no tiene
```

</details>

---

### Ejercicio 2 — Gestión del caché

1. Consulta `dig @127.0.0.1 google.com` dos veces y observa el Query time (primera vs segunda)
2. Vuelca el caché con `rndc dumpdb -cache` y busca la entrada de google.com
3. Vacía solo google.com del caché con `flushname`
4. Consulta de nuevo y verifica que el Query time volvió a ser alto (se resolvió de nuevo)
5. Vacía todo el caché con `flush` y verifica que ya no hay entradas

<details>
<summary>Solución</summary>

```bash
# 1. Comparar tiempos
dig @127.0.0.1 google.com +stats 2>&1 | grep "Query time"
# ;; Query time: 45 msec   ← primera vez, recursión completa

dig @127.0.0.1 google.com +stats 2>&1 | grep "Query time"
# ;; Query time: 0 msec    ← segunda vez, desde caché

# 2. Volcar caché
sudo rndc dumpdb -cache
sudo grep "google.com" /var/cache/bind/named_dump.db   # Debian
# google.com.  TTL  IN  A  142.250.x.x
# Muestra el registro A con el TTL restante

# 3. Vaciar solo google.com
sudo rndc flushname google.com

# 4. Consultar de nuevo
dig @127.0.0.1 google.com +stats 2>&1 | grep "Query time"
# ;; Query time: 30 msec   ← resolvió de nuevo (no estaba en caché)

# 5. Vaciar todo
sudo rndc flush

# Verificar
sudo rndc dumpdb -cache
sudo wc -l /var/cache/bind/named_dump.db
# Mucho menos entradas que antes
# (nota: algunas entradas como root hints permanecen)
```

</details>

---

### Ejercicio 3 — Recarga selectiva

1. Crea dos zonas: `zona1.lab` y `zona2.lab` con seriales `2026032101`
2. Modifica solo `zona1.lab` (añade un registro A, incrementa serial a `2026032102`)
3. Ejecuta `rndc reload zona1.lab` — verifica con `dig` que el nuevo registro existe
4. Verifica que `zona2.lab` NO fue recargada (compara el "last loaded" con `rndc zonestatus`)
5. Ejecuta `rndc reload` (sin zona) — verifica que ambas zonas se revisan

<details>
<summary>Solución</summary>

```bash
# 1. Crear zonas (ya sabes cómo, ver temas anteriores)
# db.zona1.lab y db.zona2.lab con serial 2026032101
# Declararlas en named.conf.local
sudo named-checkconf -z && sudo systemctl reload named

# 2. Editar zona1.lab: añadir "nuevo IN A 192.168.1.99", serial → 2026032102

# 3. Recargar solo zona1
sudo rndc reload zona1.lab
# zone reload up-to-date (si el serial no cambió, dice esto)
# O: zone reload successful

dig @127.0.0.1 nuevo.zona1.lab A +short
# 192.168.1.99   ← el nuevo registro está ahí

# 4. Verificar que zona2 no fue tocada
sudo rndc zonestatus zona1.lab | grep "last loaded"
# last loaded: (timestamp reciente)

sudo rndc zonestatus zona2.lab | grep "last loaded"
# last loaded: (timestamp anterior)  ← no fue recargada

# 5. Reload global
sudo rndc reload
# Ambas zonas se revisan (zona2 no recarga porque serial no cambió)
```

</details>

---

### Pregunta de reflexión

> Tienes un servidor DNS con 200 zonas y 50,000 entradas en caché. Un cliente reporta que un dominio externo resuelve a una IP incorrecta (el sitio migró hace 2 horas pero el TTL original era 24h). ¿Qué comando de rndc usas para solucionar el problema del cliente sin afectar a los otros 49,999 registros en caché? ¿Y si el problema afecta a todo un dominio (ejemplo.com y todos sus subdominios)?

> **Razonamiento esperado**: para un nombre específico, usar `rndc flushname www.sitio.com` — esto elimina solo esa entrada del caché sin afectar nada más. La próxima consulta del cliente resolverá de nuevo y obtendrá la IP correcta. Para todo un dominio y subdominios, usar `rndc flushtree sitio.com` — elimina `sitio.com`, `www.sitio.com`, `api.sitio.com`, etc. No usar `rndc flush` porque vaciaría las 50,000 entradas, degradando temporalmente el rendimiento para todos los clientes (cada consulta requiere resolución completa hasta que el caché se repuebla). La granularidad de rndc (`flushname` → `flushtree` → `flush`) permite resolver problemas con el mínimo impacto.

---

> **Siguiente tema**: T03 — Logs de BIND (canales de logging, debugging)
