# Certificados TLS

## Índice

1. [TLS y la necesidad de certificados](#1-tls-y-la-necesidad-de-certificados)
2. [Anatomía de un certificado X.509](#2-anatomía-de-un-certificado-x509)
3. [Cadena de confianza](#3-cadena-de-confianza)
4. [Generación de CSR (Certificate Signing Request)](#4-generación-de-csr-certificate-signing-request)
5. [Certificados autofirmados para laboratorio](#5-certificados-autofirmados-para-laboratorio)
6. [Let's Encrypt y ACME](#6-lets-encrypt-y-acme)
7. [Gestión de certificados en el sistema](#7-gestión-de-certificados-en-el-sistema)
8. [Inspección y diagnóstico de certificados](#8-inspección-y-diagnóstico-de-certificados)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. TLS y la necesidad de certificados

TLS (Transport Layer Security) proporciona tres garantías para la comunicación
entre cliente y servidor:

```
┌────────────────────────────────────────────────────────┐
│                    TLS garantiza                       │
│                                                        │
│  1. CONFIDENCIALIDAD                                   │
│     Los datos viajan cifrados                          │
│     (nadie puede leer el tráfico interceptado)         │
│                                                        │
│  2. INTEGRIDAD                                         │
│     Los datos no pueden ser modificados en tránsito    │
│     (detección de alteración)                          │
│                                                        │
│  3. AUTENTICACIÓN                                      │
│     El servidor es quien dice ser                      │
│     (el certificado lo demuestra)                      │
│                                                        │
│  Sin certificado válido, la tercera garantía no existe │
│  → posible ataque Man-in-the-Middle                    │
└────────────────────────────────────────────────────────┘
```

### El problema que resuelve el certificado

Sin autenticación, un atacante puede interceptar la conexión y presentarse
como el servidor legítimo:

```
Sin certificado verificado:

Cliente ──────► Atacante (MitM) ──────► Servidor real
         TLS          ▲           TLS
         con          │           con
         atacante     │           servidor
                  Lee/modifica
                  todo el tráfico

Con certificado verificado por CA:

Cliente ──────────────────────────────► Servidor real
         TLS con certificado
         firmado por CA reconocida
         → cliente verifica identidad
         → atacante no puede suplantar
```

---

## 2. Anatomía de un certificado X.509

Los certificados TLS siguen el estándar X.509v3. Un certificado contiene:

### 2.1 Campos principales

```
┌──────────────────────────────────────────────────┐
│              Certificado X.509v3                 │
├──────────────────────────────────────────────────┤
│                                                  │
│  Version: 3                                      │
│  Serial Number: 04:A3:7B:...                     │
│  Signature Algorithm: sha256WithRSAEncryption    │
│                                                  │
│  Issuer (emisor):                                │
│    CN = Let's Encrypt Authority X3               │
│    O  = Let's Encrypt                            │
│                                                  │
│  Validity:                                       │
│    Not Before: Mar 21 00:00:00 2026 GMT          │
│    Not After:  Jun 19 23:59:59 2026 GMT          │
│                                                  │
│  Subject (titular):                              │
│    CN = www.ejemplo.com                          │
│                                                  │
│  Subject Public Key Info:                        │
│    Algorithm: ECDSA (P-256)                      │
│    Public Key: 04:7A:3B:...                      │
│                                                  │
│  Extensions:                                     │
│    Subject Alternative Name (SAN):               │
│      DNS: ejemplo.com                            │
│      DNS: www.ejemplo.com                        │
│    Key Usage: Digital Signature                   │
│    Extended Key Usage: TLS Web Server Auth        │
│    Basic Constraints: CA:FALSE                   │
│    Authority Info Access:                         │
│      OCSP: http://ocsp.letsencrypt.org           │
│                                                  │
│  Signature: 3A:7C:2D:... (firma del emisor)      │
│                                                  │
└──────────────────────────────────────────────────┘
```

### 2.2 Campos clave

| Campo                    | Contenido                                          |
|--------------------------|----------------------------------------------------|
| **Subject**              | Identidad del titular (CN = Common Name)           |
| **Issuer**               | Identidad de quien firmó (la CA)                   |
| **Validity**             | Fecha de inicio y expiración                       |
| **Public Key**           | Clave pública del servidor                         |
| **SAN**                  | Subject Alternative Name — dominios adicionales    |
| **Signature**            | Firma digital del emisor sobre todo lo anterior    |
| **Key Usage**            | Para qué puede usarse (firma, cifrado, etc.)       |
| **Basic Constraints**    | CA:TRUE si es una CA, CA:FALSE si es certificado final |

### 2.3 CN vs SAN

El campo **CN** (Common Name) es legacy. Los navegadores modernos ignoran CN y
usan exclusivamente **SAN** (Subject Alternative Name) para verificar el
dominio:

```
# SAN permite múltiples dominios en un solo certificado
Subject Alternative Name:
    DNS: ejemplo.com
    DNS: www.ejemplo.com
    DNS: api.ejemplo.com
    DNS: *.ejemplo.com        ← wildcard
```

> **Wildcard**: `*.ejemplo.com` cubre `www.ejemplo.com`, `api.ejemplo.com`,
> pero **no** `ejemplo.com` (sin subdominio) ni `sub.sub.ejemplo.com`
> (multinivel). Para cubrir el dominio raíz hay que incluirlo explícitamente
> como SAN adicional.

### 2.4 Formatos de archivo

| Formato  | Extensión         | Codificación | Contenido                         |
|----------|-------------------|--------------|-----------------------------------|
| PEM      | `.pem`, `.crt`, `.cer` | Base64 (texto) | Certificado y/o clave privada |
| DER      | `.der`, `.cer`    | Binario      | Certificado (sin clave)           |
| PKCS#12  | `.p12`, `.pfx`    | Binario      | Certificado + clave (protegido)   |
| PKCS#7   | `.p7b`, `.p7c`    | Base64       | Cadena de certificados (sin clave)|

**PEM** es el formato estándar en servidores Linux. Se identifica por las
marcas delimitadoras:

```
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQEL...
...base64...
-----END CERTIFICATE-----
```

```
-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQC7...
...base64...
-----END PRIVATE KEY-----
```

---

## 3. Cadena de confianza

### 3.1 Modelo de confianza PKI

Los navegadores confían en un conjunto de **Certificate Authorities (CAs)**
raíz preinstaladas. Una CA firma certificados de servidores, avalando su
identidad:

```
┌──────────────────────┐
│   CA Raíz (Root CA)  │    ← preinstalada en navegadores/SO
│   Autofirmada        │       (~100-150 CAs raíz reconocidas)
│   Validez: 20+ años  │
└──────────┬───────────┘
           │ firma
           ▼
┌──────────────────────┐
│  CA Intermedia       │    ← firmada por la raíz
│  Validez: 5-10 años  │       (la raíz se mantiene offline)
└──────────┬───────────┘
           │ firma
           ▼
┌──────────────────────┐
│  Certificado del     │    ← firmado por la intermedia
│  servidor            │       (lo que compras/obtienes)
│  ejemplo.com         │
│  Validez: 90d-1 año  │
└──────────────────────┘
```

### 3.2 Verificación por el navegador

```
El navegador recibe: [cert servidor] + [cert intermedia]

1. Verificar que cert servidor está firmado por la intermedia
   → comprobar la firma con la clave pública de la intermedia ✓

2. Verificar que cert intermedia está firmado por la raíz
   → buscar la raíz en el almacén local del SO/navegador
   → comprobar la firma con la clave pública de la raíz ✓

3. Verificar validez temporal
   → Not Before ≤ ahora ≤ Not After ✓

4. Verificar que el dominio coincide con SAN
   → ejemplo.com ∈ {ejemplo.com, www.ejemplo.com} ✓

5. Verificar revocación (OCSP/CRL)
   → certificado no revocado ✓

Todo OK → candado verde 🔒
Fallo en cualquier paso → advertencia de seguridad
```

### 3.3 El archivo de cadena (chain file)

El servidor debe enviar no solo su certificado sino también los intermedios.
La raíz no se envía (el cliente ya la tiene):

```
# fullchain.pem (lo que configuras en el servidor)
-----BEGIN CERTIFICATE-----
(certificado del servidor)
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
(certificado de la CA intermedia)
-----END CERTIFICATE-----
```

```nginx
# Nginx
ssl_certificate /etc/ssl/certs/fullchain.pem;       # cert + intermedias
ssl_certificate_key /etc/ssl/private/ejemplo.key;    # clave privada
```

```apache
# Apache (puede ser un solo archivo o separados)
SSLCertificateFile /etc/ssl/certs/ejemplo.crt
SSLCertificateChainFile /etc/ssl/certs/chain.crt     # intermedias
SSLCertificateKeyFile /etc/ssl/private/ejemplo.key

# O con fullchain (Apache 2.4.8+)
SSLCertificateFile /etc/ssl/certs/fullchain.pem
SSLCertificateKeyFile /etc/ssl/private/ejemplo.key
```

---

## 4. Generación de CSR (Certificate Signing Request)

Para obtener un certificado firmado por una CA, se genera un CSR: un archivo
que contiene la clave pública del servidor y la identidad solicitada,
firmado con la clave privada correspondiente.

### 4.1 Flujo de obtención

```
Administrador                    CA (Certificate Authority)
────────────                    ───────────────────────────
1. Generar par de claves
   (pública + privada)
        │
2. Crear CSR con clave
   pública + datos del dominio
        │
3. Enviar CSR ──────────────►  4. Verificar identidad
                                  (DV/OV/EV)
                                      │
                               5. Firmar certificado
                                  con clave privada de CA
                                      │
6. Recibir certificado ◄────────── Certificado firmado

7. Instalar en el servidor:
   - Certificado (.crt)
   - Clave privada (.key)     ← NUNCA sale del servidor
   - Cadena intermedia (.chain)
```

### 4.2 Generar clave privada + CSR

**Con RSA (compatibilidad máxima):**

```bash
# Generar clave privada RSA de 2048 bits
openssl genrsa -out ejemplo.com.key 2048

# Generar CSR a partir de la clave
openssl req -new -key ejemplo.com.key -out ejemplo.com.csr \
    -subj "/C=ES/ST=Madrid/L=Madrid/O=Mi Empresa/CN=ejemplo.com"
```

**Con ECDSA (recomendado — más rápido, claves más cortas):**

```bash
# Generar clave privada ECDSA P-256
openssl ecparam -genkey -name prime256v1 -noout -out ejemplo.com.key

# Generar CSR
openssl req -new -key ejemplo.com.key -out ejemplo.com.csr \
    -subj "/C=ES/ST=Madrid/L=Madrid/O=Mi Empresa/CN=ejemplo.com"
```

**Todo en un comando (clave + CSR):**

```bash
openssl req -newkey rsa:2048 -nodes \
    -keyout ejemplo.com.key \
    -out ejemplo.com.csr \
    -subj "/C=ES/ST=Madrid/L=Madrid/O=Mi Empresa/CN=ejemplo.com"
# -nodes: no cifrar la clave privada con passphrase
```

### 4.3 CSR con Subject Alternative Names (SAN)

Las CAs modernas requieren SAN. Para incluir múltiples dominios:

```bash
# Crear archivo de configuración para el CSR
cat > ejemplo.com.cnf << 'EOF'
[req]
default_bits = 2048
distinguished_name = dn
req_extensions = san
prompt = no

[dn]
C = ES
ST = Madrid
L = Madrid
O = Mi Empresa
CN = ejemplo.com

[san]
subjectAltName = DNS:ejemplo.com,DNS:www.ejemplo.com,DNS:api.ejemplo.com
EOF

# Generar clave + CSR con SAN
openssl req -newkey rsa:2048 -nodes \
    -keyout ejemplo.com.key \
    -out ejemplo.com.csr \
    -config ejemplo.com.cnf
```

### 4.4 Verificar el contenido de un CSR

```bash
openssl req -in ejemplo.com.csr -text -noout

# Verificar que la firma del CSR es correcta
openssl req -in ejemplo.com.csr -verify -noout
# verify OK
```

### 4.5 Niveles de validación

| Nivel | Nombre                   | Verifica                          | Tiempo    | Coste |
|-------|--------------------------|-----------------------------------|-----------|-------|
| DV    | Domain Validation        | Control del dominio (DNS/HTTP)    | Minutos   | Gratis–bajo |
| OV    | Organization Validation  | DV + existencia de la organización| Días      | Medio |
| EV    | Extended Validation      | OV + verificación legal exhaustiva| Semanas   | Alto  |

Let's Encrypt emite certificados DV de forma gratuita y automatizada. Para la
mayoría de sitios web, DV es suficiente. OV y EV son relevantes para entidades
financieras o gubernamentales que necesitan demostrar identidad organizativa.

---

## 5. Certificados autofirmados para laboratorio

Un certificado autofirmado es firmado por su propia clave privada, no por una
CA reconocida. Los navegadores mostrarán una advertencia de seguridad, pero la
conexión estará cifrada.

### 5.1 Generar certificado autofirmado básico

```bash
openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
    -keyout /etc/ssl/private/lab.key \
    -out /etc/ssl/certs/lab.crt \
    -subj "/CN=lab.local"
```

| Flag       | Propósito                                        |
|------------|--------------------------------------------------|
| `-x509`    | Generar certificado en vez de CSR                |
| `-nodes`   | Sin passphrase en la clave privada               |
| `-days 365`| Validez de 1 año                                 |
| `-newkey`  | Generar nueva clave al mismo tiempo              |
| `-keyout`  | Ruta del archivo de clave privada                |
| `-out`     | Ruta del certificado                             |
| `-subj`    | Subject (sin prompt interactivo)                 |

### 5.2 Autofirmado con SAN (necesario para Chrome/Edge)

Chrome rechaza certificados sin SAN incluso si el CN coincide. Para laboratorio
funcional:

```bash
openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
    -keyout /etc/ssl/private/lab.key \
    -out /etc/ssl/certs/lab.crt \
    -subj "/CN=lab.local" \
    -addext "subjectAltName=DNS:lab.local,DNS:*.lab.local,IP:192.168.1.100"
```

La opción `-addext` (OpenSSL 1.1.1+) permite añadir extensiones X.509
directamente desde la línea de comandos.

### 5.3 CA local para laboratorio

Para evitar aceptar excepciones de seguridad en cada servicio del lab, se
puede crear una CA local e instalar su certificado raíz en los navegadores:

```bash
# 1. Crear CA raíz
openssl req -x509 -nodes -days 3650 -newkey rsa:4096 \
    -keyout /etc/ssl/private/lab-ca.key \
    -out /etc/ssl/certs/lab-ca.crt \
    -subj "/CN=Lab CA/O=Laboratorio"

# 2. Generar clave y CSR para el servidor
openssl req -newkey rsa:2048 -nodes \
    -keyout /etc/ssl/private/servidor.key \
    -out /tmp/servidor.csr \
    -subj "/CN=servidor.lab.local" \
    -addext "subjectAltName=DNS:servidor.lab.local"

# 3. Firmar el CSR con la CA local
openssl x509 -req -in /tmp/servidor.csr \
    -CA /etc/ssl/certs/lab-ca.crt \
    -CAkey /etc/ssl/private/lab-ca.key \
    -CAcreateserial \
    -out /etc/ssl/certs/servidor.crt \
    -days 365 \
    -copy_extensions copyall

# 4. Instalar lab-ca.crt en los navegadores del laboratorio
#    → todos los certificados firmados por ella serán confiables
```

### 5.4 Permisos de archivos

```bash
# Clave privada: solo root puede leer
chmod 600 /etc/ssl/private/ejemplo.com.key
chown root:root /etc/ssl/private/ejemplo.com.key

# Certificado: lectura pública (es información pública)
chmod 644 /etc/ssl/certs/ejemplo.com.crt
```

---

## 6. Let's Encrypt y ACME

### 6.1 Concepto

Let's Encrypt es una CA gratuita que automatiza la emisión y renovación de
certificados DV mediante el protocolo **ACME** (Automatic Certificate
Management Environment). Emite certificados con validez de 90 días, renovables
automáticamente.

### 6.2 Protocolo ACME — cómo funciona

```
certbot (cliente ACME)              Let's Encrypt (servidor ACME)
──────────────────────              ────────────────────────────
1. "Quiero certificado
    para ejemplo.com"  ──────────►
                                    2. "Demuestra que controlas
                       ◄──────────     ejemplo.com. Pon este
                                       token en /.well-known/
                                       acme-challenge/XYZ123"
3. Coloca el token en
   el servidor web
   (automáticamente)    ──────────►
                                    4. Verifica accediendo a
                                       http://ejemplo.com/
                                       .well-known/acme-challenge/XYZ123
                                       ✓ Token correcto

                       ◄──────────  5. Emite certificado firmado

6. Instala certificado
   y recarga servidor web
```

### 6.3 Métodos de validación

| Método         | Challenge                | Requisito                          |
|----------------|--------------------------|------------------------------------|
| **HTTP-01**    | Archivo en `.well-known/`| Servidor web accesible en puerto 80|
| **DNS-01**     | Registro TXT en DNS      | Acceso a la zona DNS del dominio   |
| **TLS-ALPN-01**| Certificado temporal TLS | Puerto 443 accesible               |

**HTTP-01** es el más común. **DNS-01** es necesario para certificados wildcard
(`*.ejemplo.com`) y para servidores sin acceso público en puerto 80.

### 6.4 Certbot — cliente ACME

```bash
# Instalar certbot
# Debian/Ubuntu
apt install certbot
# Para integración con Nginx:
apt install python3-certbot-nginx
# Para Apache:
apt install python3-certbot-apache

# RHEL/Fedora
dnf install certbot
dnf install python3-certbot-nginx
# o python3-certbot-apache
```

**Obtener certificado con plugin de Nginx (automático):**

```bash
certbot --nginx -d ejemplo.com -d www.ejemplo.com
# 1. Verifica que Nginx tiene un server block para esos dominios
# 2. Configura el challenge HTTP-01 temporalmente
# 3. Obtiene el certificado
# 4. Modifica la configuración de Nginx automáticamente
# 5. Recarga Nginx
```

**Obtener certificado con plugin de Apache:**

```bash
certbot --apache -d ejemplo.com -d www.ejemplo.com
```

**Modo standalone (sin servidor web corriendo):**

```bash
# Certbot levanta su propio servidor temporal en puerto 80
certbot certonly --standalone -d ejemplo.com
```

**Modo webroot (servidor web ya corriendo, sin modificar config):**

```bash
certbot certonly --webroot -w /var/www/ejemplo/public \
    -d ejemplo.com -d www.ejemplo.com
# Coloca el token en /var/www/ejemplo/public/.well-known/acme-challenge/
```

### 6.5 Archivos generados por certbot

```
/etc/letsencrypt/
├── live/
│   └── ejemplo.com/
│       ├── cert.pem        ← certificado del servidor
│       ├── chain.pem       ← certificados intermedios
│       ├── fullchain.pem   ← cert.pem + chain.pem (usar este)
│       └── privkey.pem     ← clave privada
├── archive/                ← versiones históricas
│   └── ejemplo.com/
│       ├── cert1.pem, cert2.pem...
│       └── ...
└── renewal/                ← configuración de renovación
    └── ejemplo.com.conf
```

### 6.6 Renovación automática

Certbot instala un timer de systemd o un cron que intenta renovar
automáticamente:

```bash
# Ver timer
systemctl list-timers | grep certbot

# Probar renovación (sin ejecutar realmente)
certbot renew --dry-run

# El comando real de renovación
certbot renew
# Solo renueva certificados que expiran en menos de 30 días
```

**Hook post-renovación** (recargar el servidor web):

```bash
# Configurar hook para recargar Nginx tras renovar
certbot renew --deploy-hook "systemctl reload nginx"

# O permanentemente en /etc/letsencrypt/renewal/ejemplo.com.conf:
# [renewalparams]
# deploy_hook = systemctl reload nginx
```

### 6.7 Wildcard con DNS-01

```bash
certbot certonly --manual --preferred-challenges dns \
    -d ejemplo.com -d '*.ejemplo.com'

# Certbot pedirá crear un registro TXT:
# _acme-challenge.ejemplo.com  →  "token_proporcionado"
# Verificar con: dig TXT _acme-challenge.ejemplo.com
```

Para automatización, usar plugins DNS de certbot (Cloudflare, Route53, etc.):

```bash
# Ejemplo con Cloudflare
pip install certbot-dns-cloudflare
certbot certonly --dns-cloudflare \
    --dns-cloudflare-credentials /etc/letsencrypt/cloudflare.ini \
    -d ejemplo.com -d '*.ejemplo.com'
```

---

## 7. Gestión de certificados en el sistema

### 7.1 Almacén de CAs del sistema

El sistema operativo mantiene un almacén de certificados raíz que las
aplicaciones usan para verificar conexiones TLS:

```bash
# Debian/Ubuntu
# Directorio: /etc/ssl/certs/
# Bundle: /etc/ssl/certs/ca-certificates.crt
# Gestión: update-ca-certificates

# RHEL/Fedora
# Directorio: /etc/pki/ca-trust/source/anchors/
# Bundle: /etc/pki/tls/certs/ca-bundle.crt
# Gestión: update-ca-trust
```

### 7.2 Añadir una CA personalizada al almacén

Para que las herramientas del sistema (curl, wget, aplicaciones) confíen en
certificados firmados por una CA local:

```bash
# Debian/Ubuntu
cp lab-ca.crt /usr/local/share/ca-certificates/lab-ca.crt
update-ca-certificates

# RHEL/Fedora
cp lab-ca.crt /etc/pki/ca-trust/source/anchors/lab-ca.crt
update-ca-trust

# Verificar
curl https://servidor.lab.local    # ya no debería dar error de certificado
```

### 7.3 Rutas estándar para certificados de servicio

| Distribución    | Certificados                  | Claves privadas                |
|-----------------|-------------------------------|--------------------------------|
| Debian/Ubuntu   | `/etc/ssl/certs/`             | `/etc/ssl/private/`            |
| RHEL/Fedora     | `/etc/pki/tls/certs/`         | `/etc/pki/tls/private/`        |
| Let's Encrypt   | `/etc/letsencrypt/live/DOMAIN/` | `/etc/letsencrypt/live/DOMAIN/` |

---

## 8. Inspección y diagnóstico de certificados

### 8.1 Examinar un certificado local

```bash
# Ver contenido completo
openssl x509 -in certificado.crt -text -noout

# Solo fechas de validez
openssl x509 -in certificado.crt -dates -noout
# notBefore=Mar 21 00:00:00 2026 GMT
# notAfter=Jun 19 23:59:59 2026 GMT

# Solo el subject y SAN
openssl x509 -in certificado.crt -subject -noout
openssl x509 -in certificado.crt -ext subjectAltName -noout

# Solo el emisor
openssl x509 -in certificado.crt -issuer -noout

# Fingerprint (para verificación manual)
openssl x509 -in certificado.crt -fingerprint -sha256 -noout
```

### 8.2 Examinar un certificado remoto

```bash
# Conectar a un servidor y mostrar su certificado
openssl s_client -connect ejemplo.com:443 -servername ejemplo.com \
    < /dev/null 2>/dev/null | openssl x509 -text -noout

# Solo fechas de expiración
echo | openssl s_client -connect ejemplo.com:443 -servername ejemplo.com \
    2>/dev/null | openssl x509 -dates -noout

# Ver la cadena completa
openssl s_client -connect ejemplo.com:443 -servername ejemplo.com \
    -showcerts < /dev/null

# Verificar la cadena contra el almacén del sistema
openssl s_client -connect ejemplo.com:443 -servername ejemplo.com \
    < /dev/null 2>&1 | grep "Verify return code"
# Verify return code: 0 (ok)
```

> **`-servername`**: Envía el SNI (Server Name Indication), necesario cuando
> el servidor aloja múltiples dominios con distintos certificados.

### 8.3 Verificar correspondencia clave-certificado

La clave privada y el certificado deben formar un par. Comparar los módulos
(RSA) o la clave pública:

```bash
# Para RSA — comparar módulos
openssl rsa -in ejemplo.key -modulus -noout | openssl md5
openssl x509 -in ejemplo.crt -modulus -noout | openssl md5
# Si ambos MD5 coinciden → son par

# Para ECDSA — comparar claves públicas
openssl ec -in ejemplo.key -pubout 2>/dev/null | openssl md5
openssl x509 -in ejemplo.crt -pubkey -noout | openssl md5
# Si coinciden → son par

# Verificar CSR también
openssl req -in ejemplo.csr -modulus -noout | openssl md5
```

### 8.4 Comprobar expiración próxima (monitorización)

```bash
# Script simple de verificación
openssl x509 -in /etc/ssl/certs/ejemplo.crt -checkend 2592000 -noout
# Sale con código 0 si el cert es válido por más de 30 días (2592000 seg)
# Sale con código 1 si expira en menos de 30 días

# Verificar servidor remoto
echo | openssl s_client -connect ejemplo.com:443 2>/dev/null | \
    openssl x509 -checkend 2592000 -noout
```

### 8.5 Conversión entre formatos

```bash
# PEM → DER
openssl x509 -in cert.pem -outform DER -out cert.der

# DER → PEM
openssl x509 -in cert.der -inform DER -outform PEM -out cert.pem

# PEM (cert + clave) → PKCS#12 (para importar en Windows/Java)
openssl pkcs12 -export -out cert.p12 \
    -in cert.pem -inkey key.pem -certfile chain.pem

# PKCS#12 → PEM
openssl pkcs12 -in cert.p12 -out cert.pem -nodes
```

---

## 9. Errores comunes

### Error 1: Cadena de certificados incompleta

```
curl: (60) SSL certificate problem: unable to get local issuer certificate
```

**Causa**: El servidor envía solo su certificado sin los intermedios. El cliente
no puede construir la cadena hasta una CA raíz conocida.

**Diagnóstico**:

```bash
openssl s_client -connect ejemplo.com:443 -servername ejemplo.com < /dev/null
# depth=0  → solo muestra el cert del servidor (falta la intermedia)
```

**Solución**: Configurar `fullchain.pem` (certificado + intermedias):

```nginx
ssl_certificate /etc/ssl/certs/fullchain.pem;    # NO solo cert.pem
```

### Error 2: Certificado expirado

```
NET::ERR_CERT_DATE_INVALID
```

**Causa**: La fecha actual está fuera del rango `Not Before`–`Not After`.

**Diagnóstico**:

```bash
openssl x509 -in cert.crt -dates -noout
```

**Prevención**: Automatizar renovación con certbot y monitorizar expiración.

### Error 3: Nombre de dominio no coincide

```
NET::ERR_CERT_COMMON_NAME_INVALID
```

**Causa**: El dominio accedido no aparece en el SAN del certificado. Ej: el
certificado es para `ejemplo.com` pero se accede a `www.ejemplo.com`.

**Solución**: Incluir todos los dominios como SAN al generar el CSR o solicitar
el certificado. Con Let's Encrypt:

```bash
certbot --nginx -d ejemplo.com -d www.ejemplo.com
```

### Error 4: Clave privada no corresponde al certificado

```
nginx: [emerg] SSL_CTX_use_PrivateKey_file failed
  (SSL: error:0B080074:x509 certificate routines: key values mismatch)
```

**Diagnóstico**:

```bash
openssl rsa -in key.pem -modulus -noout | openssl md5
openssl x509 -in cert.pem -modulus -noout | openssl md5
# Si los hashes difieren → no son par
```

**Causa habitual**: Se regeneró la clave privada sin generar un nuevo CSR, o se
mezcló la clave de un dominio con el certificado de otro.

### Error 5: Permisos incorrectos en la clave privada

```
nginx: [emerg] cannot load certificate key "/etc/ssl/private/key.pem":
  BIO_new_file() failed (SSL: error:02001002:system library:fopen:
  No such file or directory)
```

**Causa**: Nginx no puede leer el archivo. El proceso master ejecuta como root
(puede leer), pero a veces SELinux o AppArmor restringen el acceso.

**Solución**:

```bash
# Permisos POSIX
chmod 600 /etc/ssl/private/key.pem
chown root:root /etc/ssl/private/key.pem

# SELinux (RHEL)
restorecon -v /etc/ssl/private/key.pem
# Verificar que el contexto sea cert_t o similar
ls -Z /etc/ssl/private/key.pem
```

---

## 10. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────┐
│              CERTIFICADOS TLS — REFERENCIA RÁPIDA               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  GENERAR CLAVE + CSR:                                           │
│  openssl req -newkey rsa:2048 -nodes \                          │
│    -keyout dom.key -out dom.csr \                               │
│    -subj "/CN=ejemplo.com"                                      │
│                                                                 │
│  CSR CON SAN:                                                   │
│  openssl req -newkey rsa:2048 -nodes \                          │
│    -keyout dom.key -out dom.csr \                               │
│    -subj "/CN=ejemplo.com" \                                    │
│    -addext "subjectAltName=DNS:ejemplo.com,DNS:www.ejemplo.com" │
│                                                                 │
│  AUTOFIRMADO (LAB):                                             │
│  openssl req -x509 -nodes -days 365 -newkey rsa:2048 \          │
│    -keyout lab.key -out lab.crt -subj "/CN=lab.local" \         │
│    -addext "subjectAltName=DNS:lab.local"                       │
│                                                                 │
│  LET'S ENCRYPT:                                                 │
│  certbot --nginx -d ejemplo.com -d www.ejemplo.com              │
│  certbot --apache -d ejemplo.com                                │
│  certbot certonly --webroot -w /var/www/html -d ejemplo.com     │
│  certbot certonly --standalone -d ejemplo.com                   │
│  certbot renew --dry-run           probar renovación            │
│  certbot renew                     renovar expirados            │
│                                                                 │
│  ARCHIVOS CERTBOT:                                              │
│  /etc/letsencrypt/live/DOMINIO/                                 │
│    fullchain.pem    cert + intermedias (usar este)              │
│    privkey.pem      clave privada                               │
│    cert.pem         solo certificado                            │
│    chain.pem        solo intermedias                            │
│                                                                 │
│  INSPECCIONAR:                                                  │
│  openssl x509 -in cert.crt -text -noout       contenido        │
│  openssl x509 -in cert.crt -dates -noout      validez          │
│  openssl x509 -in cert.crt -checkend 2592000  ¿expira en 30d?  │
│  openssl req -in dom.csr -text -noout          ver CSR          │
│                                                                 │
│  INSPECCIONAR REMOTO:                                           │
│  echo | openssl s_client -connect host:443 \                    │
│    -servername host 2>/dev/null | openssl x509 -text -noout     │
│                                                                 │
│  VERIFICAR PAR CLAVE-CERT:                                      │
│  openssl rsa -in key -modulus -noout | openssl md5              │
│  openssl x509 -in crt -modulus -noout | openssl md5             │
│  (ambos MD5 deben coincidir)                                    │
│                                                                 │
│  CONFIGURAR EN SERVIDOR:                                        │
│  Nginx:                                                         │
│    ssl_certificate fullchain.pem;                               │
│    ssl_certificate_key privkey.pem;                             │
│  Apache:                                                        │
│    SSLCertificateFile fullchain.pem                              │
│    SSLCertificateKeyFile privkey.pem                             │
│                                                                 │
│  CA LOCAL (LAB):                                                │
│  Debian: cp ca.crt /usr/local/share/ca-certificates/            │
│          update-ca-certificates                                 │
│  RHEL:   cp ca.crt /etc/pki/ca-trust/source/anchors/           │
│          update-ca-trust                                        │
│                                                                 │
│  PERMISOS:                                                      │
│  Clave privada: 600 root:root                                   │
│  Certificado:   644 root:root (es público)                      │
│                                                                 │
│  FORMATOS:                                                      │
│  PEM (.pem/.crt) ← estándar Linux (Base64)                     │
│  DER (.der)      ← binario                                     │
│  PKCS#12 (.p12)  ← cert+clave en un archivo (Windows/Java)     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Certificados autofirmados con SAN

1. Genera una clave privada ECDSA P-256 para `lab.local`.
2. Genera un certificado autofirmado con:
   - Validez de 365 días
   - SAN incluyendo: `lab.local`, `*.lab.local`, `IP:127.0.0.1`
3. Examina el certificado generado con `openssl x509 -text -noout` y verifica:
   - El algoritmo de clave pública
   - Las fechas de validez
   - Los Subject Alternative Names
4. Configura el certificado en Nginx o Apache y accede con `curl -v`.
5. Observa el error de certificado. Ahora accede con `curl -k` (ignorar
   verificación). ¿Qué diferencia ves en la salida?
6. Verifica que la clave y el certificado son par usando la comparación de
   claves públicas.

**Pregunta de predicción**: Si generas el certificado con SAN `DNS:lab.local`
pero accedes por `https://127.0.0.1`, ¿el navegador aceptará el certificado?
¿Y si añades `IP:127.0.0.1` al SAN?

### Ejercicio 2: CA local para laboratorio

1. Crea una CA raíz local:
   - Clave RSA 4096
   - Certificado autofirmado con validez de 10 años
   - Subject: `CN=Lab Root CA`
2. Genera una clave y CSR para `web.lab.local` con SAN.
3. Firma el CSR con tu CA raíz (usando `openssl x509 -req -CA ...`).
4. Verifica la cadena: `openssl verify -CAfile lab-ca.crt web.lab.local.crt`
5. Instala `lab-ca.crt` en el almacén de confianza del sistema.
6. Configura el certificado firmado en Nginx o Apache.
7. Accede con `curl https://web.lab.local` (sin `-k`). ¿Funciona sin error?
8. Genera un segundo certificado para `api.lab.local` firmado por la misma CA.
   ¿Necesitas instalar algo adicional en el cliente?

**Pregunta de predicción**: Si tu CA raíz tiene validez de 10 años y firmas un
certificado de servidor con validez de 2 años, ¿qué pasa cuando la CA raíz
expira en 10 años? ¿Los certificados de servidor firmados que aún no expiran
seguirán siendo válidos?

### Ejercicio 3: Inspección y diagnóstico de certificados remotos

1. Conecta a tres sitios públicos diferentes y examina sus certificados:
   ```bash
   echo | openssl s_client -connect google.com:443 -servername google.com \
       2>/dev/null | openssl x509 -text -noout
   ```
2. Para cada sitio, anota: emisor (CA), algoritmo de clave, validez, SANs.
3. Verifica la cadena completa de uno de ellos con `-showcerts` e identifica
   cuántos certificados intermedios envía.
4. Comprueba si alguno de los tres expira en menos de 30 días con `-checkend`.
5. Busca un sitio con certificado Let's Encrypt e identifica cómo difiere
   del certificado de un sitio comercial (emisor, validez, cantidad de SANs).
6. Intenta conectar sin `-servername` (sin SNI) a un servidor que aloje
   múltiples dominios. ¿Qué certificado recibes?

**Pregunta de reflexión**: Let's Encrypt emite certificados con validez de
90 días, mientras que CAs comerciales emiten certificados de 1 año (el máximo
actual permitido). ¿Qué ventajas de seguridad tiene la validez corta? ¿Qué
riesgos operativos introduce? ¿Por qué la industria ha reducido la validez
máxima de 5 años (antiguo) a 1 año (actual)?

---

> **Siguiente tema**: T02 — Configuración TLS en Apache (mod_ssl,
> SSLCertificateFile, cipher suites).
