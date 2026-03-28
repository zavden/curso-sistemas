# Direct Rules — integración con iptables/nftables subyacente

## Índice

1. [Qué son las direct rules](#1-qué-son-las-direct-rules)
2. [Cuándo son necesarias](#2-cuándo-son-necesarias)
3. [Sintaxis de direct rules](#3-sintaxis-de-direct-rules)
4. [Direct rules con iptables](#4-direct-rules-con-iptables)
5. [Direct rules con cadenas personalizadas](#5-direct-rules-con-cadenas-personalizadas)
6. [Direct rules para tráfico FORWARD](#6-direct-rules-para-tráfico-forward)
7. [Passthrough rules](#7-passthrough-rules)
8. [El archivo direct.xml](#8-el-archivo-directxml)
9. [Alternativas modernas a direct rules](#9-alternativas-modernas-a-direct-rules)
10. [Interacción con zonas y rich rules](#10-interacción-con-zonas-y-rich-rules)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Qué son las direct rules

Las direct rules permiten insertar reglas **iptables/nftables nativas** directamente
en la configuración de firewalld, bypaseando el modelo de zonas y servicios. Son
la "escotilla de escape" cuando las abstracciones de firewalld no cubren una
necesidad específica.

```
                Modelo de firewalld
                ───────────────────
                Zonas → Servicios → Rich rules
                         │
                         │  ¿No cubre tu caso?
                         ▼
                ┌────────────────────┐
                │   Direct rules     │  ← Sintaxis iptables cruda
                │   (escape hatch)   │     insertada en firewalld
                └────────┬───────────┘
                         │
                         ▼
                ┌────────────────────┐
                │  nftables / kernel │
                └────────────────────┘
```

### Estado actual: deprecated

A partir de firewalld 0.9.0 (RHEL 9, Fedora 33+), las direct rules están
**deprecadas** oficialmente. Las alternativas modernas (policies, rich rules
extendidas y `--add-passthrough`) cubren la mayoría de los casos.

```
firewalld < 0.9:
  Direct rules = única forma de hacer reglas complejas

firewalld ≥ 0.9:
  Policies           → tráfico entre zonas
  Rich rules         → reglas condicionales en una zona
  --add-passthrough   → reglas nativas sin la capa direct

firewalld futuro:
  Direct rules probablemente eliminadas
```

**Recomendación**: aprende direct rules porque las encontrarás en sistemas RHEL 7/8
existentes, pero en configuraciones nuevas usa policies y rich rules.

---

## 2. Cuándo son necesarias

### Casos donde las direct rules eran la única opción (pre-0.9)

| Caso de uso | Alternativa moderna |
|-------------|-------------------|
| Reglas entre zonas (FORWARD) | **Policies** (firewalld 0.9+) |
| Matches complejos (-m string, -m geoip) | Rich rules o passthrough |
| Cadenas personalizadas (jump chains) | Rich rules + multiple zones |
| Rate limiting avanzado por IP (hashlimit) | nft sets dinámicos |
| Tablas mangle (DSCP, TTL, mark) | Passthrough |
| Reglas con -m owner (filtro por UID) | Passthrough |
| Integración con fail2ban legacy | fail2ban acción `firewallcmd-rich-rules` |

### Casos donde las direct rules siguen siendo útiles

Incluso en firewalld moderno, hay escenarios donde la abstracción de zonas
no alcanza:

```bash
# 1. Modificar campos del paquete (mangle)
#    firewalld no tiene concepto de "marcar paquetes con DSCP"

# 2. Match por propietario del proceso (UID/GID)
#    Rich rules no soportan -m owner

# 3. Match por contenido del paquete (string match)
#    Rich rules no soportan -m string

# 4. Reglas en tabla raw (NOTRACK, optimización conntrack)
#    firewalld no gestiona la tabla raw
```

---

## 3. Sintaxis de direct rules

### Estructura del comando

```bash
firewall-cmd [--permanent] --direct --add-rule <familia> <tabla> <cadena> <prioridad> <argumentos_iptables>
```

| Parámetro | Valores | Descripción |
|-----------|---------|-------------|
| familia | `ipv4`, `ipv6`, `eb` | Familia de protocolo |
| tabla | `filter`, `nat`, `mangle`, `raw`, `security` | Tabla iptables |
| cadena | `INPUT`, `OUTPUT`, `FORWARD`, o personalizada | Cadena de la tabla |
| prioridad | Número entero (0 = más alta) | Orden entre direct rules |
| argumentos | Sintaxis iptables | La regla iptables sin `iptables -t tabla -A cadena` |

### Ejemplo básico

```bash
# Equivalente a: iptables -A INPUT -p tcp --dport 9999 -j ACCEPT
firewall-cmd --direct --add-rule ipv4 filter INPUT 0 \
    -p tcp --dport 9999 -j ACCEPT
```

Desglose:
- `ipv4` — familia IPv4
- `filter` — tabla filter
- `INPUT` — cadena INPUT
- `0` — prioridad máxima (se inserta primero entre las direct rules)
- `-p tcp --dport 9999 -j ACCEPT` — argumentos iptables tal cual

### Operaciones disponibles

```bash
# Añadir regla
firewall-cmd --direct --add-rule ipv4 filter INPUT 0 ...

# Eliminar regla (mismos argumentos exactos)
firewall-cmd --direct --remove-rule ipv4 filter INPUT 0 ...

# Consultar si existe
firewall-cmd --direct --query-rule ipv4 filter INPUT 0 ...

# Listar todas las direct rules
firewall-cmd --direct --get-all-rules

# Listar reglas de una cadena específica
firewall-cmd --direct --get-rules ipv4 filter INPUT
```

---

## 4. Direct rules con iptables

### Filtrado por contenido (string match)

```bash
# Bloquear paquetes HTTP que contengan un User-Agent específico
# (caso: bot malicioso con User-Agent conocido)
firewall-cmd --permanent --direct --add-rule ipv4 filter INPUT 0 \
    -p tcp --dport 80 \
    -m string --string "BadBot/1.0" --algo bm \
    -j DROP
```

### Filtrado por propietario del proceso

```bash
# Solo el usuario "webuser" puede hacer conexiones salientes al puerto 443
firewall-cmd --permanent --direct --add-rule ipv4 filter OUTPUT 0 \
    -p tcp --dport 443 \
    -m owner ! --uid-owner webuser \
    -j REJECT

# Solo procesos del grupo "dbadmin" pueden conectar a PostgreSQL
firewall-cmd --permanent --direct --add-rule ipv4 filter OUTPUT 0 \
    -p tcp --dport 5432 \
    -m owner ! --gid-owner dbadmin \
    -j REJECT
```

### Rate limiting avanzado (hashlimit)

```bash
# Limitar conexiones SSH a 3/minuto POR IP de origen
# (las rich rules solo permiten rate limit global)
firewall-cmd --permanent --direct --add-rule ipv4 filter INPUT 0 \
    -p tcp --dport 22 \
    -m conntrack --ctstate NEW \
    -m hashlimit \
    --hashlimit-above 3/min \
    --hashlimit-burst 3 \
    --hashlimit-mode srcip \
    --hashlimit-name ssh_limit \
    -j DROP
```

### Manipulación de paquetes (tabla mangle)

```bash
# Marcar paquetes de una red para policy routing
firewall-cmd --permanent --direct --add-rule ipv4 mangle PREROUTING 0 \
    -s 192.168.2.0/24 \
    -j MARK --set-mark 0x2

# Establecer DSCP para QoS
firewall-cmd --permanent --direct --add-rule ipv4 mangle POSTROUTING 0 \
    -p tcp --dport 5060 \
    -j DSCP --set-dscp-class EF

# Cambiar TTL (útil para ocultar hop count)
firewall-cmd --permanent --direct --add-rule ipv4 mangle POSTROUTING 0 \
    -j TTL --ttl-set 64
```

### Bypass de connection tracking (tabla raw)

```bash
# Desactivar conntrack para tráfico de alto volumen (DNS server)
# Mejora rendimiento al no rastrear estas conexiones
firewall-cmd --permanent --direct --add-rule ipv4 raw PREROUTING 0 \
    -p udp --dport 53 -j NOTRACK
firewall-cmd --permanent --direct --add-rule ipv4 raw OUTPUT 0 \
    -p udp --sport 53 -j NOTRACK
```

---

## 5. Direct rules con cadenas personalizadas

Puedes crear cadenas personalizadas para organizar lógica compleja.

### Crear y usar cadenas

```bash
# Crear cadena personalizada
firewall-cmd --permanent --direct --add-chain ipv4 filter SSH_PROTECTION

# Añadir reglas a la cadena personalizada
# Rate limiting por IP
firewall-cmd --permanent --direct --add-rule ipv4 filter SSH_PROTECTION 0 \
    -m conntrack --ctstate NEW \
    -m hashlimit \
    --hashlimit-above 5/min \
    --hashlimit-burst 5 \
    --hashlimit-mode srcip \
    --hashlimit-name ssh_brute \
    -j DROP

# Loguear intentos
firewall-cmd --permanent --direct --add-rule ipv4 filter SSH_PROTECTION 1 \
    -m conntrack --ctstate NEW \
    -j LOG --log-prefix "SSH_NEW: " --log-level info

# Aceptar los que pasan el rate limit
firewall-cmd --permanent --direct --add-rule ipv4 filter SSH_PROTECTION 2 \
    -j ACCEPT

# Saltar a la cadena desde INPUT
firewall-cmd --permanent --direct --add-rule ipv4 filter INPUT 0 \
    -p tcp --dport 22 -j SSH_PROTECTION

firewall-cmd --reload
```

### Gestionar cadenas

```bash
# Listar cadenas personalizadas
firewall-cmd --direct --get-all-chains
firewall-cmd --direct --get-chains ipv4 filter

# Eliminar cadena (primero eliminar todas sus reglas y referencias)
firewall-cmd --permanent --direct --remove-chain ipv4 filter SSH_PROTECTION
```

---

## 6. Direct rules para tráfico FORWARD

Antes de las policies (firewalld < 0.9), las direct rules eran la única forma
de controlar tráfico que pasa **a través** del firewall entre interfaces.

### Escenario: router con restricciones entre redes

```
LAN (eth0, internal) ──── Router ──── DMZ (eth1, dmz)
     192.168.1.0/24                    10.0.0.0/24
```

```bash
# Permitir LAN → DMZ (administración)
firewall-cmd --permanent --direct --add-rule ipv4 filter FORWARD 0 \
    -i eth0 -o eth1 \
    -s 192.168.1.0/24 -d 10.0.0.0/24 \
    -j ACCEPT

# Bloquear DMZ → LAN (aislamiento)
firewall-cmd --permanent --direct --add-rule ipv4 filter FORWARD 0 \
    -i eth1 -o eth0 \
    -s 10.0.0.0/24 -d 192.168.1.0/24 \
    -j DROP

# Permitir conexiones establecidas en ambas direcciones
firewall-cmd --permanent --direct --add-rule ipv4 filter FORWARD 0 \
    -m conntrack --ctstate ESTABLISHED,RELATED \
    -j ACCEPT

firewall-cmd --reload
```

### Equivalente moderno con policies (firewalld 0.9+)

```bash
# El mismo escenario sin direct rules:
firewall-cmd --permanent --new-policy=lan-to-dmz
firewall-cmd --permanent --policy=lan-to-dmz --add-ingress-zone=internal
firewall-cmd --permanent --policy=lan-to-dmz --add-egress-zone=dmz
firewall-cmd --permanent --policy=lan-to-dmz --set-target=ACCEPT

firewall-cmd --permanent --new-policy=dmz-to-lan
firewall-cmd --permanent --policy=dmz-to-lan --add-ingress-zone=dmz
firewall-cmd --permanent --policy=dmz-to-lan --add-egress-zone=internal
firewall-cmd --permanent --policy=dmz-to-lan --set-target=DROP

firewall-cmd --reload
```

La versión con policies es más legible, se integra con `--list-all`, y no
depende de la sintaxis iptables.

---

## 7. Passthrough rules

Las passthrough rules son una variante de direct rules que pasa el comando
completo al backend sin procesamiento por parte de firewalld.

### Diferencia: direct rule vs passthrough

```
Direct rule:
  firewall-cmd --direct --add-rule ipv4 filter INPUT 0 -p tcp --dport 80 -j ACCEPT
  → firewalld interpreta: familia=ipv4, tabla=filter, cadena=INPUT, prioridad=0
  → firewalld inserta la regla en su posición dentro de sus cadenas

Passthrough:
  firewall-cmd --direct --passthrough ipv4 -t filter -A INPUT -p tcp --dport 80 -j ACCEPT
  → firewalld pasa el comando directamente a iptables
  → No gestiona prioridad ni posición
  → La regla va exactamente donde iptables la pondría
```

### Sintaxis

```bash
# Passthrough runtime
firewall-cmd --direct --passthrough ipv4 \
    -t mangle -A PREROUTING -s 10.0.0.0/8 -j MARK --set-mark 0x1

# Passthrough permanente
firewall-cmd --permanent --direct --passthrough ipv4 \
    -t raw -A PREROUTING -p udp --dport 53 -j NOTRACK

# Listar passthroughs
firewall-cmd --direct --get-all-passthroughs
firewall-cmd --direct --get-passthroughs ipv4

# Eliminar
firewall-cmd --permanent --direct --remove-passthrough ipv4 \
    -t mangle -A PREROUTING -s 10.0.0.0/8 -j MARK --set-mark 0x1

firewall-cmd --reload
```

### Cuándo usar passthrough vs direct rule

```
Direct rule (--add-rule):
  ✓ Cuando necesitas control de prioridad entre direct rules
  ✓ Cuando quieres que firewalld gestione la posición
  ✓ Más integrado con el modelo de firewalld

Passthrough (--passthrough):
  ✓ Cuando migras reglas iptables existentes sin modificar
  ✓ Cuando necesitas control exacto de posición (-I, -A, número)
  ✓ Para reglas que firewalld no puede representar internamente
  ✗ Menos controlado — firewalld no valida ni gestiona la regla
```

---

## 8. El archivo direct.xml

Todas las direct rules permanentes se almacenan en `/etc/firewalld/direct.xml`.

### Estructura

```xml
<?xml version="1.0" encoding="utf-8"?>
<direct>
  <!-- Cadenas personalizadas -->
  <chain ipv="ipv4" table="filter" chain="SSH_PROTECTION"/>

  <!-- Reglas directas -->
  <rule ipv="ipv4" table="filter" chain="INPUT" priority="0">
    -p tcp --dport 22 -j SSH_PROTECTION
  </rule>

  <rule ipv="ipv4" table="filter" chain="SSH_PROTECTION" priority="0">
    -m conntrack --ctstate NEW
    -m hashlimit --hashlimit-above 5/min --hashlimit-burst 5
    --hashlimit-mode srcip --hashlimit-name ssh_brute -j DROP
  </rule>

  <rule ipv="ipv4" table="filter" chain="SSH_PROTECTION" priority="1">
    -j ACCEPT
  </rule>

  <!-- Passthroughs -->
  <passthrough ipv="ipv4">
    -t mangle -A PREROUTING -s 10.0.0.0/8 -j MARK --set-mark 0x1
  </passthrough>
</direct>
```

### Editar directamente

Puedes editar `direct.xml` manualmente en lugar de usar `firewall-cmd`:

```bash
# Editar
vim /etc/firewalld/direct.xml

# Recargar para aplicar
firewall-cmd --reload

# Verificar
firewall-cmd --direct --get-all-rules
```

**Precaución**: si la sintaxis XML es incorrecta o una regla iptables es
inválida, firewalld puede fallar al recargar. Siempre verifica con
`firewall-cmd --reload` inmediatamente después de editar.

---

## 9. Alternativas modernas a direct rules

### Tabla de migración

| Direct rule (legacy) | Alternativa moderna | Desde |
|----------------------|-------------------|-------|
| FORWARD entre zonas | `--new-policy` + `--add-ingress/egress-zone` | firewalld 0.9 |
| Filtro por IP origen | `--add-rich-rule` con `source address` | firewalld 0.3 |
| Rate limiting global | `--add-rich-rule` con `limit value` | firewalld 0.3 |
| Log selectivo | `--add-rich-rule` con `log prefix` | firewalld 0.3 |
| Bloqueo por ipset | `--add-source=ipset:NAME` en zona drop | firewalld 0.4 |
| Filtro por MAC | `--add-rich-rule` con `source mac` | firewalld 0.3 |
| Port forwarding condicional | `--add-rich-rule` con `forward-port` | firewalld 0.3 |

### Ejemplo de migración: SSH rate limiting

**Antes (direct rules)**:

```bash
firewall-cmd --permanent --direct --add-chain ipv4 filter SSH_LIMIT
firewall-cmd --permanent --direct --add-rule ipv4 filter SSH_LIMIT 0 \
    -m conntrack --ctstate NEW \
    -m recent --name sshattack --set
firewall-cmd --permanent --direct --add-rule ipv4 filter SSH_LIMIT 1 \
    -m conntrack --ctstate NEW \
    -m recent --name sshattack --rcheck --seconds 60 --hitcount 4 \
    -j DROP
firewall-cmd --permanent --direct --add-rule ipv4 filter SSH_LIMIT 2 \
    -j ACCEPT
firewall-cmd --permanent --direct --add-rule ipv4 filter INPUT 0 \
    -p tcp --dport 22 -j SSH_LIMIT
```

**Después (rich rule)**:

```bash
firewall-cmd --permanent --zone=public --add-rich-rule='
  rule family="ipv4"
  service name="ssh"
  accept
  limit value="3/m"'
```

O con nft directamente (sin firewalld):

```
table inet firewall {
    set ssh_blocked {
        type ipv4_addr
        flags dynamic,timeout
        timeout 10m
    }

    chain input {
        type filter hook input priority 0; policy drop;
        ip saddr @ssh_blocked drop
        tcp dport 22 ct state new limit rate over 3/minute add @ssh_blocked { ip saddr } drop
        tcp dport 22 ct state new accept
    }
}
```

### Ejemplo de migración: FORWARD entre zonas

**Antes (direct rules)**:

```bash
firewall-cmd --permanent --direct --add-rule ipv4 filter FORWARD 0 \
    -i eth0 -o eth1 -s 192.168.1.0/24 -d 10.0.0.0/24 -j ACCEPT
firewall-cmd --permanent --direct --add-rule ipv4 filter FORWARD 0 \
    -i eth1 -o eth0 -s 10.0.0.0/24 -d 192.168.1.0/24 -j DROP
```

**Después (policies)**:

```bash
firewall-cmd --permanent --new-policy=lan-to-dmz
firewall-cmd --permanent --policy=lan-to-dmz --add-ingress-zone=internal
firewall-cmd --permanent --policy=lan-to-dmz --add-egress-zone=dmz
firewall-cmd --permanent --policy=lan-to-dmz --set-target=ACCEPT

firewall-cmd --permanent --new-policy=dmz-to-lan
firewall-cmd --permanent --policy=dmz-to-lan --add-ingress-zone=dmz
firewall-cmd --permanent --policy=dmz-to-lan --add-egress-zone=internal
firewall-cmd --permanent --policy=dmz-to-lan --set-target=DROP
```

### Cuándo aún necesitas direct/passthrough

```
Caso                              Rich rule / Policy    Direct / Passthrough
─────────────────────────────     ──────────────────    ────────────────────
-m string (contenido paquete)          ✗                       ✓
-m owner (filtro por UID/GID)          ✗                       ✓
-m geoip (filtro geográfico)           ✗                       ✓
tabla raw (NOTRACK)                    ✗                       ✓
tabla mangle (DSCP, MARK, TTL)         ✗                       ✓
-m connbytes (volumen tráfico)         ✗                       ✓
LOG con nflog-group                    ✗                       ✓
```

Para estos casos, si puedes elegir, usa nft directamente en lugar de direct
rules de firewalld. Pero si el sistema debe usar firewalld (política
corporativa), passthrough es la opción.

---

## 10. Interacción con zonas y rich rules

### Orden de evaluación con direct rules

```
Paquete llega
     │
     ▼
┌─────────────────────────┐
│  Direct rules           │  ← Se evalúan PRIMERO
│  (prioridad numérica)   │
└────────────┬────────────┘
             │ si ninguna direct rule decide
             ▼
┌─────────────────────────┐
│  Zonas de firewalld     │
│  ┌───────────────────┐  │
│  │ Source matching    │  │
│  │ Interface matching │  │
│  │ Default zone      │  │
│  └───────────────────┘  │
│                         │
│  Dentro de la zona:     │
│  1. Port forwarding     │
│  2. Rich rules (deny)   │
│  3. Rich rules (allow)  │
│  4. Servicios/puertos   │
│  5. ICMP blocks         │
│  6. Target default      │
└─────────────────────────┘
```

**Predicción**: una direct rule con `-j ACCEPT` en INPUT puede permitir
tráfico que una zona descartaría. Las direct rules tienen prioridad sobre
todo el modelo de zonas. Por eso son poderosas pero peligrosas.

### Conflictos potenciales

```bash
# Escenario problemático:
# Zona public: SSH permitido solo desde 10.0.0.0/24 (rich rule)
firewall-cmd --zone=public --add-rich-rule='
  rule family="ipv4" source address="10.0.0.0/24"
  service name="ssh" accept'

# Direct rule: permite SSH desde cualquier lugar
firewall-cmd --direct --add-rule ipv4 filter INPUT 0 \
    -p tcp --dport 22 -j ACCEPT
# → La direct rule se evalúa primero → SSH abierto a todos
# → La rich rule nunca se aplica para SSH
```

Para evitar conflictos:
- Usa direct rules solo para funcionalidades que las zonas no soportan
- Nunca dupliques lógica entre direct rules y zonas
- Documenta las direct rules con comentarios en `direct.xml`

### Visibilidad

```bash
# Las direct rules NO aparecen en --list-all
firewall-cmd --zone=public --list-all
# No muestra direct rules

# Hay que consultarlas por separado
firewall-cmd --direct --get-all-rules
firewall-cmd --direct --get-all-chains
firewall-cmd --direct --get-all-passthroughs

# Para ver todo (zonas + direct rules + reglas generadas):
nft list ruleset
```

---

## 11. Errores comunes

### Error 1: direct rule sobrescribe restricción de zona

```bash
# ✗ Rich rule restringe, pero direct rule la invalida
# Rich rule: SSH solo desde oficina
firewall-cmd --zone=public --add-rich-rule='rule family="ipv4"
  source address="10.0.0.0/24" service name="ssh" accept'
# Direct rule: acepta todo SSH (evaluada ANTES)
firewall-cmd --direct --add-rule ipv4 filter INPUT 0 \
    -p tcp --dport 22 -j ACCEPT

# ✓ Si necesitas direct rule para SSH (ej. hashlimit),
# incluir la restricción de IP en la propia direct rule
firewall-cmd --direct --add-rule ipv4 filter INPUT 0 \
    -s 10.0.0.0/24 -p tcp --dport 22 \
    -m hashlimit --hashlimit-above 5/min --hashlimit-burst 5 \
    --hashlimit-mode srcip --hashlimit-name ssh \
    -j DROP
firewall-cmd --direct --add-rule ipv4 filter INPUT 1 \
    -s 10.0.0.0/24 -p tcp --dport 22 -j ACCEPT
```

### Error 2: olvidar la prioridad

```bash
# ✗ Dos reglas con la misma prioridad — orden impredecible
firewall-cmd --direct --add-rule ipv4 filter INPUT 0 \
    -s 10.0.0.0/8 -j ACCEPT
firewall-cmd --direct --add-rule ipv4 filter INPUT 0 \
    -s 10.0.0.50 -j DROP
# ¿Se bloquea 10.0.0.50? Depende del orden de inserción

# ✓ Usar prioridades explícitas para controlar el orden
firewall-cmd --direct --add-rule ipv4 filter INPUT 0 \
    -s 10.0.0.50 -j DROP          # prioridad 0: primero
firewall-cmd --direct --add-rule ipv4 filter INPUT 1 \
    -s 10.0.0.0/8 -j ACCEPT       # prioridad 1: después
```

### Error 3: direct rules solo IPv4

```bash
# ✗ Regla solo para IPv4, olvidando IPv6
firewall-cmd --direct --add-rule ipv4 filter INPUT 0 \
    -p tcp --dport 22 -j ACCEPT
# Los clientes IPv6 no están cubiertos

# ✓ Añadir también para IPv6 si es necesario
firewall-cmd --direct --add-rule ipv6 filter INPUT 0 \
    -p tcp --dport 22 -j ACCEPT
# O mejor aún, usar una rich rule (que aplica a ambas familias en zona inet)
```

### Error 4: direct rules persisten en sistemas migrados

```bash
# ✗ Se migró a policies pero las direct rules antiguas siguen activas
# Las direct rules se evalúan primero → conflictos silenciosos

# ✓ Después de migrar, limpiar direct.xml
cat /etc/firewalld/direct.xml
# Si tiene reglas, verificar que las policies las reemplazan
# Luego vaciar:
firewall-cmd --permanent --direct --remove-all-rules
firewall-cmd --permanent --direct --remove-all-chains
firewall-cmd --permanent --direct --remove-all-passthroughs
firewall-cmd --reload
```

### Error 5: no verificar qué backend usa firewalld

```bash
# ✗ Direct rules con sintaxis iptables en un sistema con backend nftables
# Funciona (firewalld traduce), pero con matices

# Verificar el backend
grep FirewallBackend /etc/firewalld/firewalld.conf

# Con backend nftables:
# - Direct rules se traducen automáticamente
# - Algunos matches iptables pueden no tener equivalente nft
# - Mensajes de error pueden ser confusos

# ✓ Si el backend es nftables y necesitas reglas nativas,
# considerar usar nft directamente en un servicio systemd aparte
```

---

## 12. Cheatsheet

```bash
# ╔══════════════════════════════════════════════════════════════════╗
# ║              DIRECT RULES — CHEATSHEET                         ║
# ╠══════════════════════════════════════════════════════════════════╣
# ║                                                                ║
# ║  ⚠  DEPRECATED en firewalld 0.9+ — usar policies/rich rules   ║
# ║                                                                ║
# ║  REGLAS DIRECTAS:                                             ║
# ║  firewall-cmd --direct --add-rule ipv4 filter INPUT 0 \        ║
# ║    -p tcp --dport 9999 -j ACCEPT                               ║
# ║  firewall-cmd --direct --remove-rule ipv4 filter INPUT 0 \     ║
# ║    -p tcp --dport 9999 -j ACCEPT                               ║
# ║  firewall-cmd --direct --get-all-rules                         ║
# ║  firewall-cmd --direct --get-rules ipv4 filter INPUT           ║
# ║                                                                ║
# ║  CADENAS PERSONALIZADAS:                                      ║
# ║  firewall-cmd --direct --add-chain ipv4 filter MY_CHAIN        ║
# ║  firewall-cmd --direct --get-all-chains                        ║
# ║  firewall-cmd --direct --remove-chain ipv4 filter MY_CHAIN     ║
# ║                                                                ║
# ║  PASSTHROUGH:                                                 ║
# ║  firewall-cmd --direct --passthrough ipv4 \                    ║
# ║    -t mangle -A PREROUTING -j MARK --set-mark 0x1             ║
# ║  firewall-cmd --direct --get-all-passthroughs                  ║
# ║                                                                ║
# ║  PERMANENTE: añadir --permanent a cualquier comando            ║
# ║  ARCHIVO: /etc/firewalld/direct.xml                            ║
# ║                                                                ║
# ║  LIMPIAR TODO:                                                ║
# ║  firewall-cmd --permanent --direct --remove-all-rules          ║
# ║  firewall-cmd --permanent --direct --remove-all-chains         ║
# ║  firewall-cmd --permanent --direct --remove-all-passthroughs   ║
# ║                                                                ║
# ║  EVALUACIÓN: direct rules → zonas → rich rules → servicios    ║
# ║                                                                ║
# ║  ALTERNATIVAS MODERNAS:                                       ║
# ║  FORWARD entre zonas → firewall-cmd --new-policy              ║
# ║  Filtro por IP       → --add-rich-rule con source             ║
# ║  Rate limiting        → --add-rich-rule con limit             ║
# ║  Bloqueo masivo       → ipset + zona drop                     ║
# ║  Mangle/raw/owner    → passthrough o nft directo              ║
# ║                                                                ║
# ╚══════════════════════════════════════════════════════════════════╝
```

---

## 13. Ejercicios

### Ejercicio 1: migrar direct rules a alternativas modernas

**Contexto**: heredas un servidor RHEL 8 con estas direct rules en
`/etc/firewalld/direct.xml`:

```xml
<direct>
  <rule ipv="ipv4" table="filter" chain="INPUT" priority="0">
    -s 10.0.0.0/24 -p tcp --dport 22 -j ACCEPT
  </rule>
  <rule ipv="ipv4" table="filter" chain="INPUT" priority="1">
    -p tcp --dport 22 -j DROP
  </rule>
  <rule ipv="ipv4" table="filter" chain="FORWARD" priority="0">
    -i eth0 -o eth1 -j ACCEPT
  </rule>
  <rule ipv="ipv4" table="filter" chain="FORWARD" priority="0">
    -i eth1 -o eth0 -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
  </rule>
  <rule ipv="ipv4" table="filter" chain="FORWARD" priority="1">
    -i eth1 -o eth0 -j DROP
  </rule>
</direct>
```

**Tareas**:

1. Analiza qué hace cada regla y documéntalo
2. Reemplaza las reglas INPUT con rich rules equivalentes
3. Reemplaza las reglas FORWARD con policies (asumiendo firewalld 0.9+)
4. Limpia las direct rules
5. Verifica que el comportamiento es idéntico

**Pistas**:
- Las dos primeras reglas implementan "SSH solo desde 10.0.0.0/24"
- Las tres reglas FORWARD implementan "eth0→eth1 libre, eth1→eth0 solo established"
- Primero añade las alternativas, prueba, luego elimina las direct rules

> **Pregunta de reflexión**: ¿por qué las direct rules de INPUT son problemáticas
> aquí? Si alguien añade SSH como servicio de la zona public con `--add-service=ssh`,
> ¿la direct rule DROP lo bloquearía o no? ¿Depende del orden de evaluación?

---

### Ejercicio 2: direct rules para funcionalidad no cubierta

**Contexto**: necesitas implementar las siguientes funcionalidades que firewalld
no cubre nativamente:

1. Marcar paquetes del departamento de desarrollo (192.168.2.0/24) con mark 0x2
   para policy routing
2. Desactivar connection tracking para el tráfico DNS en un servidor autoritativo
   (optimización de rendimiento)
3. Solo el usuario `nginx` (UID) puede aceptar conexiones en el puerto 80

**Tareas**:

1. Implementa cada requisito con direct rules o passthrough
2. Verifica cada regla con `firewall-cmd --direct --get-all-rules`
3. Documenta por qué cada caso requiere direct rules (qué funcionalidad falta
   en rich rules)
4. Describe cómo harías estas reglas con nft directo si no usaras firewalld

**Pistas**:
- Mark: tabla mangle, cadena PREROUTING
- NOTRACK: tabla raw, cadena PREROUTING + OUTPUT
- Owner: `-m owner --uid-owner nginx` en tabla filter, cadena INPUT (no funciona
  en INPUT con iptables estándar — reflexiona sobre esto)

> **Pregunta de reflexión**: el match `-m owner` solo funciona en las cadenas
> OUTPUT y POSTROUTING (donde el kernel conoce el proceso que generó el paquete).
> ¿Por qué no funciona en INPUT? ¿Cómo resolverías "solo nginx puede recibir
> en el puerto 80" sin `-m owner`?

---

### Ejercicio 3: auditoría y limpieza

**Contexto**: un servidor de producción tiene una mezcla de direct rules,
rich rules, servicios de zona y reglas nft manuales. Necesitas auditar y
limpiar.

**Tareas**:

1. Ejecuta estos comandos y analiza la salida:
   ```bash
   firewall-cmd --get-active-zones
   firewall-cmd --list-all-zones
   firewall-cmd --direct --get-all-rules
   firewall-cmd --direct --get-all-chains
   firewall-cmd --direct --get-all-passthroughs
   nft list ruleset
   ```
2. Identifica:
   - ¿Hay reglas duplicadas entre direct rules y zonas?
   - ¿Hay direct rules que pueden migrarse a rich rules?
   - ¿Hay reglas nft manuales fuera del control de firewalld?
3. Crea un plan de migración con este orden:
   a. Añadir alternativas modernas
   b. Verificar
   c. Eliminar direct rules
   d. Verificar de nuevo
4. Documenta qué direct rules deben mantenerse (si las hay) y por qué

> **Pregunta de reflexión**: `nft list ruleset` muestra tanto las reglas generadas
> por firewalld como cualquier regla nft manual. ¿Cómo distingues unas de otras?
> ¿Qué pasa con las reglas nft manuales cuando firewalld ejecuta `--reload`?

---

> **Siguiente capítulo**: C06 — Diagnóstico de Red: T01 — ss (reemplazo de netstat, filtros, estados de conexión)
