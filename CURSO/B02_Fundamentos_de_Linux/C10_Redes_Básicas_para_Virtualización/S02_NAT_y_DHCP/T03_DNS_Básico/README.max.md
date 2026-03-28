# T03 — DNS Básico

## El problema

¿Memorizar `142.250.80.46` para acceder a Google? Los humanos记忆 mejor nombres que números.

DNS (Domain Name System) traduce nombres de dominio a IPs.

## Jerarquía DNS

```
google.com
    └── .com (TLD — Top Level Domain)
          └── google (dominio de segundo nivel)
                └── (subdominios: mail.google.com, maps.google.com)
```

### Tipos de servidores DNS

| Tipo | Rol | Ejemplo |
|------|-----|---------|
| Root server | Conoce dónde están los TLD (.com, .org, .net) | 13 root servers globally |
| TLD server | Gestiona los dominios de un TLD (.com, .es, .io) | Verisign (.com) |
| Authoritative | Tiene la respuesta definitiva para un dominio | dns.google.com |
| Recursive/Caching | Busca en nombre del cliente, guarda caché | Tu router, 8.8.8.8 |

## Cómo funciona una consulta DNS

```
[Tu laptop]  ── "google.com?" ──→  [DNS recursivo: 8.8.8.8]
                                        │
                                        ↓
                              [Root server] (.com?)
                                        │
                                        ↓
                              [TLD .com server] (google.com?)
                                        │
                                        ↓
                              [dns.google.com] = 142.250.80.46
                                        │
                                        ↓
[Tu laptop]  ←─ "142.250.80.46" ── [DNS recursivo: 8.8.8.8]
```

Los servidores DNS recursivos (como 8.8.8.8 de Google o 1.1.1.1 de Cloudflare) **cachan** las respuestas, así que no siempre consultan toda la cadena.

## Herramientas DNS

### `dig` — consulta DNS detallada

```bash
dig google.com
```

```
;; ANSWER SECTION:
google.com.        300    IN    A    142.250.80.46
```

| Campo | Significado |
|-------|-------------|
| 300 | TTL (segundos que se puede cachear) |
| IN | Internet class |
| A | Address record (IPv4) |

```bash
# Consulta corta (solo la respuesta)
dig +short google.com

# Consulta IPv6
dig AAAA google.com

# Consulta específica de un servidor DNS
dig @1.1.1.1 google.com
```

### `nslookup` — alternativa simple

```bash
nslookup google.com
```

### `host` — aún más simple

```bash
host google.com
```

## `/etc/resolv.conf` — Configuración del Cliente DNS

En este archivo se configuran los servidores DNS que usa tu máquina:

```bash
cat /etc/resolv.conf
```

```
nameserver 192.168.1.1
nameserver 8.8.8.8
search localdomain
```

| Campo | Significado |
|-------|-------------|
| nameserver | IP del servidor DNS (máximo 3) |
| search | Sufijo agregado automáticamente a nombres cortos |

**Importante**: en sistemas con NetworkManager o systemd-resolved, este archivo puede ser sobreescrito automáticamente.

### ¿Cómo sabe Linux qué DNS usar?

El orden está en `/etc/nsswitch.conf`:

```bash
grep hosts /etc/nsswitch.conf
```

```
hosts: files dns
```

1. Primero consulta `/etc/hosts`
2. Si no encuentra, consulta DNS

## `/etc/hosts` — Override Local

Para mapear nombres a IPs **sin DNS**:

```bash
cat /etc/hosts
```

```
127.0.0.1   localhost
::1         localhost
192.168.1.100  mi-servidor.local
```

Útil para:
- Testing sin comprar un dominio
- Bloquear publicidad (0.0.0.0 ads.example.com)
- Overrides temporales

## DNS en QEMU/KVM (libvirt)

En la red NAT de libvirt (`192.168.122.0/24`), el gateway de la VM (`192.168.122.1`) también actúa como **DNS proxy**:

```
┌─────────────────────────────────────────┐
│ VM: 192.168.122.101                     │
│     /etc/resolv.conf                    │
│     nameserver 192.168.122.1            │
└────────────────┬────────────────────────┘
                 │ "google.com?"
                 │ (reenvía al DNS del host)
                 ↓
         Host: 192.168.1.1 (router real)
                 │
                 ↓
         Internet DNS
```

## Ejercicios

1. Consulta DNS:
   ```bash
   dig wikipedia.org
   # ¿Cuál es la IP de wikipedia.org?
   # ¿Cuánto dura el TTL?
   
   dig AAAA wikipedia.org
   # ¿Tiene IPv6?
   ```

2. Compara tiempos de respuesta:
   ```bash
   time dig +short google.com
   time dig +short +tcp google.com
   # ¿Alguna diferencia?
   ```

3. Simula un override de DNS:
   ```bash
   # Añadir a /etc/hosts:
   # 127.0.0.1  myapp.local
   
   ping myapp.local
   # Debería ir a 127.0.0.1
   ```

4. Verifica el DNS de libvirt:
   ```bash
   # Dentro de una VM en libvirt:
   cat /etc/resolv.conf
   # ¿Apunta al gateway de libvirt (192.168.122.1)?
   
   # En el host:
   dig @192.168.122.1 google.com
   # ¿Funciona el DNS proxy de libvirt?
   ```

5. Investiga: ¿por qué `ping google.com` funciona incluso sin `/etc/resolv.conf` configurado?

## Resumen

| Concepto | Descripción |
|---|---|
| DNS | Traduce nombres de dominio a IPs |
| `dig` | Herramienta para hacer consultas DNS |
| `/etc/resolv.conf` | Archivo de configuración de DNS del cliente |
| `/etc/hosts` | Override local nombre → IP |
| TTL | Tiempo que una respuesta DNS se puede cachear |
| DNS recursivo | Busca respuestas por ti (8.8.8.8, 1.1.1.1) |
| libvirt DNS | El gateway de libvirt reenvía DNS al host |
