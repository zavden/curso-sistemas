# DNSSEC — Extensiones de seguridad para DNS

## Índice

1. [El problema que resuelve DNSSEC](#el-problema-que-resuelve-dnssec)
2. [Conceptos fundamentales](#conceptos-fundamentales)
3. [Cadena de confianza](#cadena-de-confianza)
4. [Registros DNSSEC](#registros-dnssec)
5. [Validación en BIND (resolver)](#validación-en-bind-resolver)
6. [Firma de zonas en BIND (autoritativo)](#firma-de-zonas-en-bind-autoritativo)
7. [Gestión de claves](#gestión-de-claves)
8. [Diagnóstico y verificación](#diagnóstico-y-verificación)
9. [Consideraciones operativas](#consideraciones-operativas)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## El problema que resuelve DNSSEC

DNS fue diseñado en los años 80 sin autenticación. Un resolver no tiene forma de saber si la respuesta que recibe es legítima o fue manipulada:

```
    Sin DNSSEC:

    Resolver ──── query: www.banco.com ────► DNS autoritativo
                                               │
                      Atacante                 │
                    ┌──────────┐               │
                    │ Inyecta  │               │
                    │ respuesta│               │
                    │ falsa    │               │
                    └────┬─────┘               │
                         │                     │
    Resolver ◄─── "www.banco.com = 6.6.6.6" ──┘
                  (IP del atacante)
                         │
    El resolver no puede distinguir
    la respuesta real de la falsa
```

Ataques que explota la falta de autenticación DNS:

| Ataque | Descripción |
|---|---|
| **DNS spoofing** | Inyectar respuestas falsas en la red |
| **DNS cache poisoning** | Insertar entradas falsas en el caché del resolver |
| **Man-in-the-middle** | Interceptar y modificar respuestas DNS en tránsito |
| **Kaminsky attack** | Envenenar caché explotando transaction IDs predecibles |

DNSSEC añade **firmas criptográficas** a las respuestas DNS. El resolver puede verificar que la respuesta proviene del autoritativo legítimo y no fue modificada:

```
    Con DNSSEC:

    Resolver ──── query: www.banco.com ────► DNS autoritativo
                                               │
                      Atacante                 │
                    ┌──────────┐               │
                    │ Inyecta  │               │
                    │ respuesta│               │
                    │ falsa    │               │
                    └────┬─────┘               │
                         │                     │
    Resolver ◄─── respuesta falsa ─────────────┘
                  SIN firma válida
                         │
                  Resolver RECHAZA
                  (firma no verifica)

    Resolver ◄─── respuesta real + firma ──────┘
                  Firma VERIFICA ✓
                  → acepta la respuesta
```

> **Lo que DNSSEC no hace**: no cifra las consultas DNS (todo sigue en texto plano). Para cifrado, existen DNS over HTTPS (DoH) y DNS over TLS (DoT), que son complementarios a DNSSEC.

---

## Conceptos fundamentales

### Firmado y verificación

DNSSEC usa criptografía asimétrica (igual que SSH o TLS):

```
    Autoritativo (firma):              Resolver (verifica):
    ┌────────────────────┐            ┌────────────────────┐
    │                    │            │                    │
    │ Clave privada ─────┤            │ Clave pública ─────┤
    │ (ZSK private)      │            │ (DNSKEY record)    │
    │                    │            │                    │
    │ Firma cada         │            │ Verifica la firma  │
    │ registro de la     │            │ con la clave       │
    │ zona               │            │ pública            │
    │                    │            │                    │
    │ Genera: RRSIG      │───────────►│ Compara: RRSIG     │
    │ (firma digital)    │            │ vs datos + DNSKEY  │
    └────────────────────┘            └────────────────────┘
```

### Dos tipos de claves

DNSSEC usa dos pares de claves por zona:

```
    ┌─────────────────────────────────────────────────────┐
    │                                                     │
    │   KSK (Key Signing Key)                            │
    │   "La clave que firma claves"                      │
    │   - Firma el DNSKEY RRset (el conjunto de claves)  │
    │   - Más larga (2048+ bits RSA o ECDSAP256)         │
    │   - Se rota con menos frecuencia                    │
    │   - Su hash va en el DS record del padre            │
    │                                                     │
    │   ZSK (Zone Signing Key)                           │
    │   "La clave que firma la zona"                     │
    │   - Firma todos los demás registros de la zona     │
    │   - Más corta (1024+ bits RSA o ECDSAP256)         │
    │   - Se rota con más frecuencia                      │
    │   - No requiere coordinar con el registro padre    │
    │                                                     │
    └─────────────────────────────────────────────────────┘
```

¿Por qué dos claves? La separación permite rotar la ZSK frecuentemente (sin coordinar con el registro padre) mientras la KSK se mantiene estable (cambiarla requiere actualizar el DS record en la zona padre, lo cual implica coordinar con el registrador).

```
    Zona padre (.com)            Zona ejemplo.com
    ┌──────────────────┐        ┌──────────────────────────┐
    │                  │        │                          │
    │ DS record ───────┼───────►│ KSK (firma DNSKEY set)   │
    │ (hash del KSK    │        │  │                       │
    │  de ejemplo.com) │        │  └─► DNSKEY RRset        │
    │                  │        │       (KSK pub + ZSK pub)│
    │                  │        │                          │
    │                  │        │ ZSK (firma todo lo demás)│
    │                  │        │  ├─► A records + RRSIG   │
    │                  │        │  ├─► MX records + RRSIG  │
    │                  │        │  ├─► NS records + RRSIG  │
    │                  │        │  └─► ... + RRSIG         │
    └──────────────────┘        └──────────────────────────┘
```

---

## Cadena de confianza

DNSSEC funciona porque la confianza se propaga desde la raíz DNS hasta la zona consultada:

```
    Root (.)
    │  Trust anchor: la clave pública de la raíz está
    │  hardcodeada en los resolvers (RFC 5011, auto-update)
    │
    │  DS record de .com ──────────────────────┐
    │  (firmado con la clave de la raíz)       │
    ▼                                          ▼
    .com (TLD)                          Verificar con
    │  DNSKEY de .com ◄─────────────── DS de la raíz ✓
    │
    │  DS record de ejemplo.com ───────────────┐
    │  (firmado con la clave de .com)          │
    ▼                                          ▼
    ejemplo.com                         Verificar con
    │  DNSKEY de ejemplo.com ◄──────── DS de .com ✓
    │
    │  www IN A 192.168.1.20
    │  RRSIG (firma del registro A)
    │  (firmado con ZSK de ejemplo.com)
    ▼
    Resolver verifica RRSIG con DNSKEY ✓
    → La respuesta es auténtica
```

Si **cualquier eslabón** de la cadena se rompe (zona padre no tiene DS, o zona hija no está firmada), la validación DNSSEC falla y el resolver trata la zona como insegura (pero no rechaza — a menos que sea un "island of security" incorrecto).

### Trust anchor

El punto de partida de la cadena es el **trust anchor**: la clave pública de la zona raíz (`.`). Los resolvers modernos la incluyen de fábrica:

```bash
# BIND incluye el trust anchor de la raíz
# Gestionado automáticamente por managed-keys (RFC 5011)

# Debian: /etc/bind/bind.keys o /usr/share/dns/root.key
# RHEL: /etc/named.root.key

# En named.conf:
dnssec-validation auto;
# "auto" = usar el trust anchor incluido de la raíz
```

---

## Registros DNSSEC

DNSSEC añade cuatro tipos de registros:

### DNSKEY — clave pública de la zona

```
ejemplo.com.  86400  IN  DNSKEY  257 3 13 (
    mdsswUyr3DPW132mOi8V9xESWE8jTo0d...
)
; 257 = KSK (Key Signing Key)
; 256 = ZSK (Zone Signing Key)
; 3   = protocolo (siempre 3 = DNSSEC)
; 13  = algoritmo (13 = ECDSAP256SHA256)
```

Algoritmos comunes:

| ID | Nombre | Estado |
|---|---|---|
| 8 | RSA/SHA-256 | Ampliamente soportado |
| 13 | ECDSAP256SHA256 | **Recomendado** — firmas pequeñas |
| 14 | ECDSAP384SHA384 | Alta seguridad |
| 15 | Ed25519 | Moderno, eficiente |
| 16 | Ed448 | Mayor seguridad |

### RRSIG — firma de un RRset

Cada conjunto de registros (RRset) del mismo nombre y tipo tiene una firma:

```
www.ejemplo.com.  86400  IN  A      192.168.1.20
www.ejemplo.com.  86400  IN  RRSIG  A 13 3 86400 (
    20260420000000 20260321000000 12345 ejemplo.com.
    base64-de-la-firma...
)
; A       = tipo de registro firmado
; 13      = algoritmo
; 3       = labels (www.ejemplo.com = 3 labels)
; 86400   = TTL original
; 20260420 = expiración de la firma
; 20260321 = inicio de validez
; 12345   = key tag (identificador de la ZSK usada)
; ejemplo.com = zona que firma
```

Las firmas tienen **fecha de expiración**. Si no se re-firman antes, los resolvers rechazan las respuestas. Esto es una fuente frecuente de incidentes operativos.

### DS — Delegation Signer

El DS record vive en la zona **padre** y contiene un hash de la KSK de la zona hija:

```
; En la zona .com (padre):
ejemplo.com.  86400  IN  DS  12345 13 2 (
    49FD46E6C4B45C55D4AC69CBD...
)
; 12345 = key tag de la KSK de ejemplo.com
; 13    = algoritmo
; 2     = tipo de digest (2 = SHA-256)
; hash  = SHA-256 de la KSK DNSKEY
```

El DS se registra a través del registrador de dominio (Namecheap, Cloudflare, etc.), que lo inserta en la zona del TLD.

### NSEC / NSEC3 — prueba de inexistencia

Cuando un nombre no existe, DNSSEC necesita **probar** la inexistencia de forma autenticada (si no, un atacante podría negar la existencia de registros legítimos):

```
; NSEC: lista el siguiente nombre que SÍ existe
; "Entre api.ejemplo.com y mail.ejemplo.com no hay nada"
api.ejemplo.com.  IN  NSEC  mail.ejemplo.com.  A RRSIG NSEC

; Problema: NSEC permite enumerar TODA la zona
; (caminar de NSEC en NSEC revela todos los nombres)
```

NSEC3 resuelve la enumeración usando hashes:

```
; NSEC3: usa hashes en vez de nombres
; No revela los nombres directamente
1abc2def3456.ejemplo.com. IN NSEC3 1 0 10 aabbccdd (
    5678efgh9012 A RRSIG
)
; Para verificar un nombre, hasheas el nombre y comparas
; No puedes caminar la zona porque solo ves hashes
```

> **NSEC3 opt-out**: permite tener delegaciones sin firmar dentro de una zona firmada, reduciendo el tamaño de zonas grandes (como los TLDs).

---

## Validación en BIND (resolver)

### Habilitar validación

La validación DNSSEC la realiza el **resolver** (no el autoritativo). Es la parte más sencilla de configurar:

```c
// En options:
options {
    // Habilitar validación DNSSEC
    dnssec-validation auto;
    // auto: usa el trust anchor de la raíz incluido en BIND
    // yes:  valida, pero necesitas configurar trust anchors manualmente
    // no:   no valida (inseguro)
};
```

Con `auto`, BIND usa el trust anchor de la raíz que viene incluido y lo actualiza automáticamente (RFC 5011).

### Verificar que la validación funciona

```bash
# Consultar un dominio con DNSSEC válido
dig @127.0.0.1 example.com +dnssec
# Buscar el flag "ad" (Authenticated Data):
# ;; flags: qr rd ra ad; QUERY: 1, ANSWER: 2
#                     ↑↑ ad = la respuesta fue validada con DNSSEC

# Consultar un dominio sin DNSSEC
dig @127.0.0.1 ejemplo-sin-dnssec.com +dnssec
# ;; flags: qr rd ra; QUERY: 1, ANSWER: 1
# Sin flag "ad" → no se pudo validar (la zona no está firmada)

# Consultar un dominio con DNSSEC ROTO (firma inválida)
# Hay zonas de test para esto:
dig @127.0.0.1 dnssec-failed.org
# ;; status: SERVFAIL
# → el resolver rechaza la respuesta porque la firma no verifica
```

### Dominios de test DNSSEC

```bash
# Validación exitosa (DNSSEC correcto):
dig @127.0.0.1 internetsociety.org +dnssec +short
# Respuesta + flag ad

# Validación fallida (DNSSEC intencionalmente roto):
dig @127.0.0.1 dnssec-failed.org +dnssec
# SERVFAIL (el resolver rechaza)

# Sin DNSSEC:
dig @127.0.0.1 example.com +dnssec
# Respuesta sin flag ad (insegure, no firmado — no falla, solo no valida)
```

### Negative trust anchors

Si una zona tiene DNSSEC roto pero necesitas acceder a ella temporalmente:

```bash
# Crear un negative trust anchor temporal (desactivar validación para esa zona)
sudo rndc nta ejemplo-roto.com
# Válido por 1 hora por defecto

# Con duración personalizada
sudo rndc nta -lifetime 3600 ejemplo-roto.com

# Listar NTAs activos
sudo rndc nta -dump

# Eliminar un NTA
sudo rndc nta -remove ejemplo-roto.com
```

---

## Firma de zonas en BIND (autoritativo)

### BIND 9.16+ — dnssec-policy (método moderno)

Las versiones modernas de BIND simplifican enormemente la firma con `dnssec-policy`:

```c
// Definir una política DNSSEC
dnssec-policy "standard" {
    // Algoritmo de las claves
    keys {
        // KSK: ECDSAP256, vida ilimitada (rotación manual)
        ksk key-directory lifetime unlimited algorithm ecdsap256sha256;
        // ZSK: ECDSAP256, rotación cada 90 días
        zsk key-directory lifetime P90D algorithm ecdsap256sha256;
    };

    // Vigencia de las firmas
    signatures-validity P14D;          // firmas válidas 14 días
    signatures-refresh P5D;            // refirmar 5 días antes de expirar

    // NSEC3 para evitar enumeración de zona
    nsec3param iterations 0 optout no salt-length 0;
    // iterations 0 y salt-length 0: recomendación actual (RFC 9276)
};

// Aplicar la política a una zona
zone "ejemplo.com" IN {
    type master;
    file "/etc/bind/zones/db.ejemplo.com";
    dnssec-policy "standard";
    inline-signing yes;               // firmar automáticamente
    // BIND genera las claves, firma la zona y re-firma automáticamente
};
```

Con `dnssec-policy` + `inline-signing`:

1. BIND **genera las claves** automáticamente
2. **Firma la zona** al cargarla
3. **Re-firma** antes de que expiren las firmas
4. **Rota la ZSK** según el lifetime configurado
5. El archivo de zona original **no se modifica** — la versión firmada se mantiene separada

### inline-signing

```
    Sin inline-signing:
    Admin edita → db.ejemplo.com → named carga → sirve sin firmar

    Con inline-signing:
    Admin edita → db.ejemplo.com → named firma → db.ejemplo.com.signed → sirve firmado
                  (zona original)                 (generado automáticamente)
```

El admin trabaja con el archivo de zona sin firmar. BIND genera internamente una versión firmada y la sirve.

### Políticas predefinidas

```c
// BIND incluye algunas políticas predefinidas:

// "default" — política básica razonable
zone "ejemplo.com" {
    type master;
    file "db.ejemplo.com";
    dnssec-policy default;
    inline-signing yes;
};

// "insecure" — eliminar DNSSEC de una zona
zone "ejemplo.com" {
    type master;
    file "db.ejemplo.com";
    dnssec-policy insecure;
    inline-signing yes;
};

// "none" — sin DNSSEC (default si no se especifica)
```

### Método manual (legacy, pre-9.16)

Para versiones anteriores o control total:

```bash
# 1. Generar KSK
dnssec-keygen -a ECDSAP256SHA256 -f KSK ejemplo.com
# Genera: Kejemplo.com.+013+12345.key  (pública)
#         Kejemplo.com.+013+12345.private (privada)

# 2. Generar ZSK
dnssec-keygen -a ECDSAP256SHA256 ejemplo.com
# Genera: Kejemplo.com.+013+67890.key
#         Kejemplo.com.+013+67890.private

# 3. Incluir las claves públicas en el archivo de zona
cat Kejemplo.com.+013+*.key >> db.ejemplo.com

# 4. Firmar la zona
dnssec-signzone -o ejemplo.com -S db.ejemplo.com
# Genera: db.ejemplo.com.signed

# 5. Configurar BIND para usar la zona firmada
zone "ejemplo.com" {
    type master;
    file "db.ejemplo.com.signed";    # ← la versión firmada
};

# 6. Re-firmar periódicamente (antes de que expiren las firmas)
# Automatizar con cron
```

> **Recomendación**: usar `dnssec-policy` siempre que la versión de BIND lo soporte. El método manual es propenso a errores operativos (olvidar re-firmar, rotar claves, etc.).

---

## Gestión de claves

### Ciclo de vida de una clave

```
    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
    │ Generate │───►│ Publish  │───►│ Active   │───►│ Retire   │
    │          │    │          │    │          │    │          │
    │ Se crea  │    │ DNSKEY   │    │ Se usa   │    │ Ya no    │
    │ la clave │    │ en la    │    │ para     │    │ firma,   │
    │          │    │ zona     │    │ firmar   │    │ pero     │
    │          │    │ (no firma│    │          │    │ DNSKEY   │
    │          │    │  aún)    │    │          │    │ aún en   │
    │          │    │          │    │          │    │ la zona  │
    └──────────┘    └──────────┘    └──────────┘    └────┬─────┘
                                                         │
                                                    ┌────┴─────┐
                                                    │ Remove   │
                                                    │          │
                                                    │ DNSKEY   │
                                                    │ eliminado│
                                                    │ de la    │
                                                    │ zona     │
                                                    └──────────┘
```

El período de "Publish" sin firmar permite que los resolvers cacheen la nueva clave DNSKEY antes de que se empiece a usar para firmar. Igualmente, "Retire" mantiene la clave pública disponible mientras resolvers aún pueden tener firmas viejas en caché.

### Rotación de ZSK con dnssec-policy

Con `dnssec-policy`, la rotación es automática:

```c
dnssec-policy "auto-rotate" {
    keys {
        ksk key-directory lifetime unlimited algorithm ecdsap256sha256;
        zsk key-directory lifetime P90D algorithm ecdsap256sha256;
        // Cada 90 días, BIND:
        // 1. Genera nueva ZSK
        // 2. Publica la DNSKEY
        // 3. Espera propagación
        // 4. Activa la nueva ZSK (firma con ella)
        // 5. Retira la vieja ZSK
        // 6. Elimina la vieja ZSK
    };
};
```

### Rotación de KSK — requiere coordinar con el padre

La rotación de KSK es más compleja porque implica actualizar el DS record en la zona padre:

```
    1. Generar nueva KSK
    2. Publicar nueva DNSKEY (KSK pub) en la zona
    3. Esperar propagación
    4. Generar nuevo DS record
    5. Enviar nuevo DS al registrador
    6. Registrador actualiza DS en la zona padre (.com)
    7. Esperar propagación del DS
    8. Activar nueva KSK (firmar DNSKEY set con ella)
    9. Retirar vieja KSK
    10. Pedir al registrador que elimine el DS viejo
```

```bash
# Generar el DS record para enviar al registrador
dnssec-dsfromkey Kejemplo.com.+013+12345.key
# ejemplo.com. IN DS 12345 13 2 49FD46E6C4B45C55D4AC...

# O desde dig:
dig @127.0.0.1 ejemplo.com DNSKEY | dnssec-dsfromkey -f - ejemplo.com
```

### Directorio de claves

```bash
# Las claves se almacenan en el directorio de la zona
ls /etc/bind/keys/
# Kejemplo.com.+013+12345.key        ← KSK pública
# Kejemplo.com.+013+12345.private    ← KSK privada
# Kejemplo.com.+013+12345.state      ← estado de la clave
# Kejemplo.com.+013+67890.key        ← ZSK pública
# Kejemplo.com.+013+67890.private    ← ZSK privada
# Kejemplo.com.+013+67890.state      ← estado de la clave

# Permisos: solo legible por el usuario de BIND
chmod 640 /etc/bind/keys/*
chown bind:bind /etc/bind/keys/*
```

---

## Diagnóstico y verificación

### dig con opciones DNSSEC

```bash
# Consultar con información DNSSEC
dig @127.0.0.1 ejemplo.com A +dnssec
# Muestra los registros RRSIG junto con los A

# Pedir solo el flag AD (Authenticated Data)
dig @127.0.0.1 ejemplo.com A +adflag
# flags: qr rd ra ad  ← ad presente = validado

# Consultar las claves DNSKEY
dig @127.0.0.1 ejemplo.com DNSKEY
# Muestra KSK (flag 257) y ZSK (flag 256)

# Consultar los DS records (en la zona padre)
dig ejemplo.com DS
# Muestra el hash de la KSK registrado en .com

# Consultar NSEC/NSEC3 (prueba de inexistencia)
dig @127.0.0.1 noexiste.ejemplo.com A +dnssec
# Muestra registros NSEC3 que prueban la inexistencia

# Trazar la cadena de confianza completa
dig ejemplo.com A +sigchase +trusted-key=/etc/bind/bind.keys
# Sigue la cadena: ejemplo.com → .com → .
```

### delv — herramienta de validación DNSSEC

`delv` (Domain Entity Lookup & Validation) es la herramienta moderna para diagnosticar DNSSEC:

```bash
# Validar un dominio
delv @127.0.0.1 ejemplo.com A
# ; fully validated          ← DNSSEC válido
# ejemplo.com.  86400  IN  A  192.168.1.20

# Si la validación falla:
delv @127.0.0.1 dnssec-failed.org A
# ;; resolution failed: SERVFAIL
# ;; validating ejemplo.com/A: no valid signature found

# Modo detallado
delv @127.0.0.1 ejemplo.com A +vtrace
# Muestra cada paso de la validación
```

### Herramientas web

```bash
# DNSViz (visualización de la cadena DNSSEC):
# https://dnsviz.net/d/ejemplo.com/dnssec/

# Verisign DNSSEC debugger:
# https://dnssec-debugger.verisignlabs.com/

# DNSSEC Analyzer (Sandia):
# https://dnssec-analyzer.verisignlabs.com/
```

### Verificar el estado en BIND

```bash
# Estado de DNSSEC para una zona
sudo rndc zonestatus ejemplo.com
# ...
# dynamic: yes
# key-directory: /etc/bind/keys
# inline-signing: yes
# DNSSEC: signed
# ...

# Ver las claves activas
sudo rndc dnssec -status ejemplo.com
# key: 12345 (KSK), algorithm ECDSAP256SHA256
#   state: active, published
# key: 67890 (ZSK), algorithm ECDSAP256SHA256
#   state: active, published

# Forzar re-firma de la zona
sudo rndc sign ejemplo.com

# Forzar rotación de ZSK
sudo rndc dnssec -rollover -key 67890 ejemplo.com
```

---

## Consideraciones operativas

### Tamaño de las respuestas

DNSSEC aumenta significativamente el tamaño de las respuestas DNS (firmas, claves):

```
    Sin DNSSEC:
    Consulta A: ~60 bytes respuesta

    Con DNSSEC:
    Consulta A + RRSIG: ~300-500 bytes respuesta
    Consulta DNSKEY: ~500-1500 bytes respuesta
```

Implicaciones:

- Respuestas pueden exceder los 512 bytes de UDP clásico
- EDNS0 (Extension mechanisms for DNS) es necesario para paquetes UDP mayores
- Si EDNS0 falla, fallback a TCP (más lento)
- Mayor consumo de ancho de banda (relevante para grandes resolvers)

### Elección de algoritmo

```
    RSA (algoritmo 8):
    + Mayor compatibilidad
    - Firmas grandes (>128 bytes con RSA-2048)
    - Claves grandes
    - Más lento

    ECDSA P-256 (algoritmo 13):     ← RECOMENDADO
    + Firmas pequeñas (~64 bytes)
    + Claves pequeñas
    + Rápido
    + Ampliamente soportado (>99% de resolvers)

    Ed25519 (algoritmo 15):
    + Más eficiente que ECDSA
    + Firmas compactas
    - Soporte menor (pero creciente)
```

### Monitorización

Las firmas DNSSEC expiran. Si no se re-firman, el dominio se vuelve irresolvable para resolvers que validan:

```bash
# Verificar cuándo expiran las firmas
dig @127.0.0.1 ejemplo.com A +dnssec | grep RRSIG
# La fecha de expiración está en el RRSIG:
# ... 20260420000000 20260321000000 ...
#     ↑ expira        ↑ válido desde

# Script de monitorización básico:
EXPIRY=$(dig @127.0.0.1 ejemplo.com SOA +dnssec | grep "RRSIG.*SOA" | awk '{print $9}')
EXPIRY_EPOCH=$(date -d "${EXPIRY:0:8}" +%s)
NOW_EPOCH=$(date +%s)
DAYS_LEFT=$(( (EXPIRY_EPOCH - NOW_EPOCH) / 86400 ))
echo "Firmas expiran en $DAYS_LEFT días"
if [ "$DAYS_LEFT" -lt 7 ]; then
    echo "¡ALERTA! Las firmas DNSSEC expiran pronto"
fi
```

### Impacto de DNSSEC roto

Un dominio con DNSSEC incorrecto (firmas expiradas, DS incorrecto, claves perdidas) es **peor** que un dominio sin DNSSEC:

- Sin DNSSEC: el dominio se resuelve normalmente (inseguro pero funcional)
- DNSSEC roto: los resolvers que validan devuelven **SERVFAIL** → el dominio es irresolvable

Por eso, desplegar DNSSEC requiere compromiso operativo: monitorización, alertas y procesos de rotación de claves.

---

## Errores comunes

### 1. Firmas expiradas (el error más frecuente)

```bash
# Síntoma: SERVFAIL para tu dominio en resolvers que validan
# Causa: las firmas (RRSIG) expiraron y no se re-firmaron

# Diagnóstico:
dig @8.8.8.8 ejemplo.com +dnssec
# status: SERVFAIL

delv ejemplo.com
# ;; validating ejemplo.com/A: verify failed: no valid RRSIG

# Comprobar expiración:
dig @tu-servidor ejemplo.com SOA +dnssec | grep RRSIG
# Ver la fecha de expiración en el campo

# Solución con dnssec-policy: no debería pasar (refirma automático)
# Verificar que BIND está corriendo y la zona está cargada:
sudo rndc zonestatus ejemplo.com

# Solución manual: re-firmar
sudo rndc sign ejemplo.com
```

### 2. DS record incorrecto en la zona padre

```bash
# Síntoma: SERVFAIL — la cadena de confianza está rota
# Causa: el DS en .com no coincide con la KSK de tu zona

# Diagnóstico:
# Obtener el DS que debería estar en el padre:
dig ejemplo.com DNSKEY | dnssec-dsfromkey -f - ejemplo.com
# Comparar con lo que está en .com:
dig ejemplo.com DS

# Si no coinciden: actualizar el DS en el registrador
```

### 3. Activar DNSSEC sin registrar el DS

```bash
# Firmaste tu zona (DNSKEY y RRSIG están presentes)
# Pero no registraste el DS en la zona padre

# Resultado: no hay cadena de confianza
# dig muestra DNSKEY y RRSIG, pero no hay flag "ad"
# No es un error funcional, pero DNSSEC no protege nada

# Solución: generar DS y enviarlo al registrador
dnssec-dsfromkey Kejemplo.com.+013+12345.key
# Copiar el DS record al panel del registrador
```

### 4. Rotación de KSK sin actualizar el DS

```bash
# Rotaste la KSK pero olvidaste actualizar el DS en el padre
# La vieja KSK fue eliminada de la zona

# Resultado: el DS en .com apunta a una KSK que ya no existe
# → Cadena de confianza rota → SERVFAIL

# Protocolo correcto:
# 1. Generar nueva KSK
# 2. Publicar en la zona (ambas KSKs activas)
# 3. Registrar nuevo DS en el padre
# 4. Esperar propagación del DS (24-48h)
# 5. SOLO ENTONCES retirar la vieja KSK
# 6. Eliminar viejo DS del padre
```

### 5. Resolver con dnssec-validation no pero zona firmada

```c
// El resolver no valida DNSSEC:
options {
    dnssec-validation no;
};

// Las respuestas NUNCA tienen flag "ad"
// Aunque la zona esté firmada, no se verifica
// = DNSSEC desplegado pero sin efecto

// Solución:
options {
    dnssec-validation auto;
};
```

---

## Cheatsheet

```bash
# === VALIDACIÓN (RESOLVER) ===
# En named.conf options:
# dnssec-validation auto;           habilitar validación

# Verificar con dig:
dig @resolver dominio +dnssec       # ver RRSIG
dig @resolver dominio +adflag       # ver flag ad
dig @resolver dominio DNSKEY        # ver claves públicas
dig dominio DS                      # ver DS en zona padre

# Verificar con delv:
delv @resolver dominio A            # "fully validated" = OK
delv @resolver dominio A +vtrace   # traza completa

# NTA (desactivar validación temporalmente):
sudo rndc nta dominio              # desactivar 1h
sudo rndc nta -dump                # listar NTAs

# === FIRMA (AUTORITATIVO MODERNO) ===
# En named.conf:
# dnssec-policy "default";
# inline-signing yes;

# Verificar estado:
sudo rndc zonestatus dominio        # estado general
sudo rndc dnssec -status dominio    # estado de claves

# Operaciones:
sudo rndc sign dominio              # forzar re-firma
sudo rndc dnssec -rollover -key ID dominio  # forzar rotación

# === FIRMA (MÉTODO MANUAL LEGACY) ===
dnssec-keygen -a ECDSAP256SHA256 -f KSK dominio  # generar KSK
dnssec-keygen -a ECDSAP256SHA256 dominio          # generar ZSK
dnssec-signzone -o dominio -S archivo-zona        # firmar zona

# === DS RECORD ===
dnssec-dsfromkey Kdominio.+013+12345.key           # generar DS
dig dominio DNSKEY | dnssec-dsfromkey -f - dominio # DS desde DNSKEY

# === DIAGNÓSTICO ===
dig dominio A +dnssec +cd           # Checking Disabled (ver datos sin validar)
dig dominio A +trace +dnssec        # trazar resolución con DNSSEC
dig dominio NSEC                    # ver cadena NSEC
dig dominio NSEC3PARAM              # ver parámetros NSEC3

# === ALGORITMOS RECOMENDADOS ===
# ECDSAP256SHA256 (13) — balance de compatibilidad y eficiencia
# Ed25519 (15) — más moderno, soporte creciente

# === FLUJO DE DESPLIEGUE ===
# 1. Configurar dnssec-policy en la zona
# 2. Recargar BIND (genera claves, firma zona)
# 3. Verificar con dig +dnssec y delv
# 4. Obtener DS: dnssec-dsfromkey o dig DNSKEY | dnssec-dsfromkey
# 5. Registrar DS en el registrador de dominio
# 6. Esperar propagación (24-48h)
# 7. Verificar cadena completa con delv +vtrace
# 8. Monitorizar expiración de firmas
```

---

## Ejercicios

### Ejercicio 1 — Verificar validación DNSSEC

1. Verifica que tu resolver BIND tiene `dnssec-validation auto` habilitado
2. Consulta `internetsociety.org` con `+dnssec` y busca el flag `ad`
3. Consulta `dnssec-failed.org` y observa el resultado (SERVFAIL)
4. Crea un negative trust anchor para `dnssec-failed.org` y repite la consulta
5. Elimina el NTA y verifica que vuelve a fallar

<details>
<summary>Solución</summary>

```bash
# 1. Verificar configuración
sudo sshd -T 2>/dev/null; sudo named-checkconf -p | grep dnssec-validation
# dnssec-validation auto;

# 2. Dominio con DNSSEC válido
dig @127.0.0.1 internetsociety.org A +dnssec | grep flags
# flags: qr rd ra ad; ...
# ↑ "ad" presente = validado

# 3. Dominio con DNSSEC roto
dig @127.0.0.1 dnssec-failed.org A
# ;; status: SERVFAIL
# No resuelve porque la validación falla

# 4. Crear NTA
sudo rndc nta dnssec-failed.org
# Negative trust anchor added: dnssec-failed.org/_default, lifetime 3600

dig @127.0.0.1 dnssec-failed.org A
# Ahora resuelve (sin flag ad — no validado pero aceptado)

# 5. Eliminar NTA
sudo rndc nta -remove dnssec-failed.org

dig @127.0.0.1 dnssec-failed.org A
# SERVFAIL de nuevo
```

</details>

---

### Ejercicio 2 — Firmar una zona con dnssec-policy

1. Crea una zona `secure.lab` con registros SOA, NS, A para www y mail
2. Configura `dnssec-policy "default"` e `inline-signing yes` en la declaración de zona
3. Recarga BIND y verifica:
   - `rndc zonestatus secure.lab` muestra `DNSSEC: signed`
   - `dig @127.0.0.1 secure.lab DNSKEY` muestra las claves (flags 256 y 257)
   - `dig @127.0.0.1 www.secure.lab +dnssec` muestra los RRSIG
4. Genera el DS record que enviarías al padre (aunque no haya padre real en este lab)

<details>
<summary>Solución</summary>

```
; /etc/bind/zones/db.secure.lab
$TTL 86400
$ORIGIN secure.lab.
@   IN  SOA  ns1.secure.lab. admin.secure.lab. (
                2026032101 3600 900 604800 86400)
@   IN  NS   ns1.secure.lab.
ns1 IN  A    192.168.1.1
www IN  A    192.168.1.20
mail IN A    192.168.1.10
@   IN  MX   10 mail.secure.lab.
```

```c
// En named.conf.local:
zone "secure.lab" IN {
    type master;
    file "/etc/bind/zones/db.secure.lab";
    dnssec-policy default;
    inline-signing yes;
    allow-transfer { none; };
};
```

```bash
# Crear directorio de claves si no existe
sudo mkdir -p /etc/bind/keys
sudo chown bind:bind /etc/bind/keys

# Validar y recargar
sudo named-checkconf && sudo systemctl reload named

# Verificar
sudo rndc zonestatus secure.lab | grep -i dnssec
# DNSSEC: yes

dig @127.0.0.1 secure.lab DNSKEY
# Debe mostrar registros con flag 257 (KSK) y 256 (ZSK)

dig @127.0.0.1 www.secure.lab A +dnssec
# Debe mostrar el registro A + RRSIG

# Generar DS
dig @127.0.0.1 secure.lab DNSKEY | dnssec-dsfromkey -f - secure.lab
# secure.lab. IN DS 12345 13 2 ABCDEF...
```

</details>

---

### Ejercicio 3 — Diagnosticar la cadena de confianza

Usando `delv` y `dig`, investiga la cadena de confianza de un dominio real con DNSSEC:

1. Elige un dominio con DNSSEC (ej. `isc.org`, `nic.cz`, `internetsociety.org`)
2. Consulta las claves DNSKEY del dominio
3. Consulta el DS record en la zona padre
4. Verifica que el DS coincide con la KSK (usa `dnssec-dsfromkey`)
5. Usa `delv +vtrace` para ver la validación paso a paso

<details>
<summary>Solución</summary>

```bash
DOMAIN="isc.org"

# 1-2. Claves DNSKEY
dig $DOMAIN DNSKEY +short
# 257 3 13 mdsswUyr3DPW...  ← KSK (flag 257)
# 256 3 13 oJMRESz5E4gY...  ← ZSK (flag 256)

# 3. DS en la zona padre
dig $DOMAIN DS +short
# 12345 13 2 49FD46E6C4B45C55...

# 4. Verificar que DS coincide con KSK
dig $DOMAIN DNSKEY | dnssec-dsfromkey -f - $DOMAIN
# Comparar la salida con el DS obtenido en paso 3
# Deben coincidir (mismo key tag, mismo hash)

# 5. Traza completa de validación
delv $DOMAIN A +vtrace
# Muestra:
# ;; validating ./DNSKEY: ... ← valida claves de la raíz
# ;; validating org./DS: ...  ← valida DS de .org
# ;; validating org./DNSKEY: ... ← valida claves de .org
# ;; validating isc.org./DS: ... ← valida DS de isc.org
# ;; validating isc.org./DNSKEY: ... ← valida claves de isc.org
# ;; validating isc.org./A: ... ← valida el registro A
# ; fully validated ← cadena completa verificada
```

</details>

---

### Pregunta de reflexión

> Tu empresa despliega DNSSEC en el dominio `empresa.com`. Todo funciona perfectamente durante 6 meses. Un sábado por la noche, el servidor BIND se reinicia tras una actualización del kernel. El lunes, el equipo de soporte recibe llamadas: "la página web no carga". Los clientes con resolvers que validan DNSSEC (Google 8.8.8.8, Cloudflare 1.1.1.1) obtienen SERVFAIL, pero los clientes con resolvers sin validación funcionan bien. `dig @tu-servidor empresa.com +dnssec` muestra registros RRSIG con fechas de expiración en el pasado. ¿Qué ocurrió y qué proceso operativo falló?

> **Razonamiento esperado**: las firmas RRSIG expiraron. Con `inline-signing` y `dnssec-policy`, BIND debería re-firmar automáticamente, pero algo falló: posiblemente BIND no arrancó correctamente tras el reinicio (error de configuración introducido en la actualización), o las claves privadas no eran accesibles (permisos cambiaron), o el filesystem donde están las claves estaba en modo read-only. El proceso que falló es la **monitorización**: debería haber alertas que detecten cuando las firmas están próximas a expirar (ej. menos de 7 días). También faltó un **test post-reinicio** que verificara no solo que BIND arrancó, sino que las zonas están firmadas correctamente (`delv` o `dig +dnssec`). La lección: DNSSEC requiere monitorización activa de la validez de las firmas, no solo del servicio DNS en sí.

---

> **Fin de la Sección 1 — Instalación y Configuración**. Los 4 temas cubren desde la instalación de BIND hasta la firma de zonas con DNSSEC.
>
> **Siguiente sección**: S02 — Diagnóstico: named-checkconf, named-checkzone, rndc, Logs de BIND.
