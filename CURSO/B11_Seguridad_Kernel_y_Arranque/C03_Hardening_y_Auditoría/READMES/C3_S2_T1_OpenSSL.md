# OpenSSL: certificados, CSR, verificación y s_client

## Índice

1. [PKI y certificados X.509](#1-pki-y-certificados-x509)
2. [Componentes de OpenSSL](#2-componentes-de-openssl)
3. [Generar claves privadas](#3-generar-claves-privadas)
4. [CSR: Certificate Signing Request](#4-csr-certificate-signing-request)
5. [Certificados autofirmados](#5-certificados-autofirmados)
6. [Inspeccionar certificados](#6-inspeccionar-certificados)
7. [s_client: diagnóstico de conexiones TLS](#7-s_client-diagnóstico-de-conexiones-tls)
8. [Cadena de confianza y CA bundle](#8-cadena-de-confianza-y-ca-bundle)
9. [Tareas comunes de sysadmin](#9-tareas-comunes-de-sysadmin)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. PKI y certificados X.509

### Qué problema resuelve PKI

```
El problema de la identidad en red
════════════════════════════════════

  Sin PKI:
  ────────
  Cliente: "Hola, ¿eres el banco?"
  Servidor: "Sí, soy el banco. Confía en mí."
  Cliente: "OK" → Envía contraseña
  (¿Y si el servidor es un impostor? Man-in-the-middle)


  Con PKI:
  ────────
  Cliente: "Hola, ¿eres el banco?"
  Servidor: "Sí, aquí tienes mi certificado firmado por DigiCert"
  Cliente: (verifica la firma de DigiCert)
           (verifica que el certificado dice "banco.com")
           (verifica que no está expirado ni revocado)
           "OK, confío" → Establece canal cifrado TLS
```

### Componentes de PKI

```
Infraestructura de Clave Pública (PKI)
════════════════════════════════════════

  ┌───────────────────┐
  │ Certificate       │  Entidad raíz de confianza
  │ Authority (CA)    │  Firma certificados de otros
  │ (DigiCert, Let's  │
  │  Encrypt, etc.)   │
  └────────┬──────────┘
           │ firma
           ▼
  ┌───────────────────┐
  │ Certificado       │  Documento digital que dice:
  │ X.509             │  "Esta clave pública pertenece a
  │                   │   este dominio/organización"
  └────────┬──────────┘
           │ contiene
           ▼
  ┌───────────────────┐     ┌──────────────────┐
  │ Clave pública     │ ←── │ Clave privada    │
  │ del servidor      │     │ (en el servidor, │
  │ (visible a todos) │     │  NUNCA compartir) │
  └───────────────────┘     └──────────────────┘
```

### Certificado X.509: qué contiene

```
Campos de un certificado X.509
════════════════════════════════

  Version:            3 (v3, la actual)
  Serial Number:      Único por CA
  Signature Algorithm: sha256WithRSAEncryption
  Issuer:             CN=DigiCert, O=DigiCert Inc, C=US
                      (quién firmó — la CA)
  Validity:
      Not Before:     Mar 21 2026 00:00:00 GMT
      Not After:      Mar 21 2027 00:00:00 GMT
  Subject:            CN=www.example.com, O=Example Inc, C=US
                      (a quién pertenece)
  Subject Public Key: RSA 2048 bit / EC 256 bit
                      (la clave pública del servidor)
  Extensions:
      Subject Alternative Name (SAN):
          DNS:www.example.com
          DNS:example.com
          DNS:*.example.com
      Key Usage: Digital Signature, Key Encipherment
      Extended Key Usage: TLS Web Server Authentication
      Basic Constraints: CA:FALSE
  Signature:          (firma digital de la CA)
```

### Subject vs SAN

```
CN (Common Name) vs SAN (Subject Alternative Name)
════════════════════════════════════════════════════

  Históricamente:
    Subject: CN=www.example.com  ← el dominio iba aquí

  Actualmente (RFC 6125):
    SAN es OBLIGATORIO para navegadores modernos
    CN está DEPRECADO para verificación de dominio

  Un certificado moderno:
    Subject: CN=example.com      ← todavía se pone, pero no es decisivo
    SAN:
      DNS:example.com            ← estos son los que importan
      DNS:www.example.com
      DNS:mail.example.com
      DNS:*.example.com          ← wildcard

  Los navegadores verifican SAN, no CN.
  Si SAN está ausente, algunos revisan CN como fallback.
```

---

## 2. Componentes de OpenSSL

```bash
# OpenSSL es una suite con múltiples subcomandos
openssl version
# OpenSSL 3.0.x ...

# Subcomandos principales:
openssl genrsa     # Generar clave RSA (legacy)
openssl genpkey    # Generar clave (método moderno, cualquier algoritmo)
openssl req        # Generar CSR o certificado autofirmado
openssl x509       # Inspeccionar/convertir certificados
openssl s_client   # Conectar como cliente TLS (diagnóstico)
openssl verify     # Verificar cadena de certificados
openssl pkcs12     # Convertir formatos (PFX/P12)
openssl enc        # Cifrado simétrico
openssl dgst       # Hash / firma digital
openssl rand       # Generar datos aleatorios
```

### Formatos de archivo

```
Formatos de certificados y claves
═══════════════════════════════════

  PEM (Privacy Enhanced Mail):
  ────────────────────────────
  Formato texto, codificado en Base64
  Empieza con: -----BEGIN CERTIFICATE-----
  Extensiones: .pem, .crt, .cer, .key
  Uso: Linux, Apache, Nginx (el más común)

  DER (Distinguished Encoding Rules):
  ────────────────────────────────────
  Formato binario (no legible)
  Extensiones: .der, .cer
  Uso: Java, Windows

  PFX / PKCS#12:
  ──────────────
  Contenedor binario: clave privada + certificado + cadena
  Extensiones: .pfx, .p12
  Uso: Windows IIS, importación/exportación de todo junto
  Protegido por contraseña

  Conversiones comunes:
    PEM → DER:  openssl x509 -in cert.pem -outform DER -out cert.der
    DER → PEM:  openssl x509 -in cert.der -inform DER -out cert.pem
    PEM → PFX:  openssl pkcs12 -export -in cert.pem -inkey key.pem -out cert.pfx
    PFX → PEM:  openssl pkcs12 -in cert.pfx -out all.pem -nodes
```

---

## 3. Generar claves privadas

### RSA

```bash
# Generar clave RSA 2048 bits (mínimo aceptable)
openssl genrsa -out server.key 2048

# Generar clave RSA 4096 bits (más seguro)
openssl genrsa -out server.key 4096

# Con cifrado (proteger con passphrase)
openssl genrsa -aes256 -out server.key 4096
# Pedirá passphrase — la clave se cifra en disco

# Método moderno (genpkey, recomendado)
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:4096 -out server.key
```

### ECC (Elliptic Curve)

```bash
# Listar curvas disponibles
openssl ecparam -list_curves

# Generar clave EC con P-256 (NIST, amplia compatibilidad)
openssl ecparam -genkey -name prime256v1 -out server-ec.key

# Método moderno (genpkey)
openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out server-ec.key

# Ed25519 (más moderno, OpenSSL 3.0+)
openssl genpkey -algorithm ED25519 -out server-ed25519.key
```

### Proteger la clave privada

```bash
# Verificar permisos
chmod 600 server.key
ls -l server.key
# -rw------- 1 root root 3243 ... server.key

# Verificar que es una clave privada válida
openssl rsa -in server.key -check -noout
# RSA key ok

# Para EC:
openssl ec -in server-ec.key -check -noout

# Ver información de la clave (sin revelar la clave misma)
openssl rsa -in server.key -text -noout | head -5
# Private-Key: (4096 bit, 2 primes)
```

### Eliminar passphrase de una clave (para servidores)

```bash
# Los servidores (nginx, apache) necesitan la clave sin passphrase
# para arrancar automáticamente sin intervención humana

# Quitar passphrase:
openssl rsa -in server-encrypted.key -out server.key

# ¡Proteger el archivo resultante con permisos estrictos!
chmod 600 server.key
chown root:root server.key
```

---

## 4. CSR: Certificate Signing Request

Un CSR es una solicitud formal a una CA para que firme tu clave pública:

```
Flujo del CSR
══════════════

  Tu servidor                              CA (ej: Let's Encrypt)
  ────────────                             ──────────────────────

  1. Generas clave privada
     (server.key)
         │
         ▼
  2. Generas CSR con la clave
     (server.csr)
     Contiene:
       - Tu clave PÚBLICA
       - Tu información (dominio, org)
       - NO la clave privada
         │
         └──── envías CSR ────────────────► 3. CA verifica tu identidad
                                               (validación de dominio,
                                                organización, etc.)
                                                    │
         ◄──── recibes certificado ───────────── 4. CA firma y devuelve
                                               certificado (server.crt)
         │
         ▼
  5. Configuras servidor con:
     - server.key (clave privada)
     - server.crt (certificado firmado)
```

### Generar CSR

```bash
# Método interactivo
openssl req -new -key server.key -out server.csr

# Country Name (2 letter code) [XX]: ES
# State or Province Name (full name) []: Madrid
# Locality Name (eg, city) []: Madrid
# Organization Name (eg, company) []: Example S.L.
# Organizational Unit Name (eg, section) []: IT
# Common Name (eg, FQDN) []: www.example.com
# Email Address []: admin@example.com
# A challenge password []:     ← dejar vacío
# An optional company name []: ← dejar vacío
```

### CSR con SAN (método moderno)

```bash
# Generar CSR con Subject Alternative Names
# Necesitas un archivo de configuración:

cat > san.cnf << 'EOF'
[req]
default_bits = 2048
prompt = no
default_md = sha256
distinguished_name = dn
req_extensions = v3_req

[dn]
C = ES
ST = Madrid
L = Madrid
O = Example S.L.
CN = example.com

[v3_req]
subjectAltName = @alt_names

[alt_names]
DNS.1 = example.com
DNS.2 = www.example.com
DNS.3 = mail.example.com
DNS.4 = *.example.com
IP.1 = 192.168.1.100
EOF

# Generar CSR con la configuración
openssl req -new -key server.key -out server.csr -config san.cnf

# Generar clave + CSR en un solo paso
openssl req -new -newkey rsa:4096 -nodes -keyout server.key \
    -out server.csr -config san.cnf
```

### Verificar un CSR

```bash
# Ver contenido del CSR
openssl req -in server.csr -text -noout

# Salida (extracto):
# Certificate Request:
#     Data:
#         Version: 1
#         Subject: C=ES, ST=Madrid, L=Madrid, O=Example S.L., CN=example.com
#         Subject Public Key Info:
#             Public Key Algorithm: rsaEncryption
#             RSA Public-Key: (4096 bit)
#     Requested Extensions:
#         X509v3 Subject Alternative Name:
#             DNS:example.com, DNS:www.example.com, DNS:mail.example.com

# Verificar que el CSR coincide con la clave
openssl req -in server.csr -verify -noout
# verify OK

# Verificar que CSR y clave privada son del mismo par
openssl req -in server.csr -noout -modulus | openssl md5
openssl rsa -in server.key -noout -modulus | openssl md5
# Si los md5 coinciden → son del mismo par
```

---

## 5. Certificados autofirmados

Un certificado autofirmado es firmado por ti mismo, no por una CA. Útil para
desarrollo, testing y servicios internos:

```bash
# Generar clave + certificado autofirmado en un solo paso
openssl req -x509 -newkey rsa:4096 -nodes \
    -keyout server.key -out server.crt \
    -days 365 -subj "/CN=myserver.local"

# Desglose:
#   -x509    → generar certificado (no CSR)
#   -newkey  → crear clave nueva al mismo tiempo
#   -nodes   → sin passphrase en la clave
#   -days    → validez en días
#   -subj    → Subject sin interacción
```

### Autofirmado con SAN

```bash
# Con SAN (requerido para Chrome/Firefox modernos)
openssl req -x509 -newkey rsa:4096 -nodes \
    -keyout server.key -out server.crt \
    -days 365 -config san.cnf \
    -extensions v3_req

# O inline con -addext (OpenSSL 3.0+)
openssl req -x509 -newkey rsa:4096 -nodes \
    -keyout server.key -out server.crt \
    -days 365 -subj "/CN=myserver.local" \
    -addext "subjectAltName=DNS:myserver.local,DNS:localhost,IP:127.0.0.1"
```

### CA interna (mini-CA)

```bash
# Para organizaciones que quieren su propia CA interna

# 1. Generar clave de la CA
openssl genrsa -aes256 -out ca.key 4096
# (con passphrase — proteger la CA)

# 2. Generar certificado raíz de la CA (autofirmado)
openssl req -x509 -new -key ca.key -out ca.crt \
    -days 3650 -subj "/CN=Internal CA/O=My Company/C=ES"
# 10 años de validez para la CA raíz

# 3. Firmar un CSR con tu CA
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out server.crt -days 365 \
    -extfile san.cnf -extensions v3_req
# -CAcreateserial → crea archivo .srl con serial numbers

# 4. Instalar ca.crt en los clientes que deben confiar
# (ver sección 8 sobre CA bundle)
```

---

## 6. Inspeccionar certificados

### Ver contenido de un certificado

```bash
# Ver todos los campos
openssl x509 -in server.crt -text -noout

# Solo el Subject
openssl x509 -in server.crt -subject -noout
# subject=CN = www.example.com, O = Example S.L., C = ES

# Solo el Issuer (quién lo firmó)
openssl x509 -in server.crt -issuer -noout
# issuer=CN = DigiCert, O = DigiCert Inc, C = US

# Solo las fechas de validez
openssl x509 -in server.crt -dates -noout
# notBefore=Mar 21 00:00:00 2026 GMT
# notAfter=Mar 21 23:59:59 2027 GMT

# Solo el fingerprint
openssl x509 -in server.crt -fingerprint -sha256 -noout

# Solo los SANs
openssl x509 -in server.crt -noout -ext subjectAltName
# X509v3 Subject Alternative Name:
#     DNS:example.com, DNS:www.example.com

# Ver fecha de expiración legible
openssl x509 -in server.crt -enddate -noout
# notAfter=Mar 21 23:59:59 2027 GMT
```

### Verificar que clave y certificado coinciden

```bash
# Comparar el modulus (RSA) o la clave pública
openssl x509 -in server.crt -noout -modulus | openssl md5
openssl rsa -in server.key -noout -modulus | openssl md5

# Si ambos MD5 son iguales → el certificado fue generado con esa clave
# Si son diferentes → hay un desajuste (error de configuración)

# Para EC:
openssl x509 -in server.crt -noout -pubkey | openssl md5
openssl ec -in server-ec.key -pubout | openssl md5
```

### Verificar cadena de certificados

```bash
# Verificar un certificado contra la CA
openssl verify -CAfile ca.crt server.crt
# server.crt: OK

# Verificar con cadena intermedia
openssl verify -CAfile ca.crt -untrusted intermediate.crt server.crt

# Verificar contra el CA bundle del sistema
openssl verify server.crt
# Si la CA está en el trust store del sistema → OK
# Si es autofirmado o CA interna → error
```

---

## 7. s_client: diagnóstico de conexiones TLS

`openssl s_client` es la herramienta más importante para diagnosticar
problemas de TLS/SSL:

### Conexión básica

```bash
# Conectar a un servidor HTTPS
openssl s_client -connect www.example.com:443
```

### Interpretar la salida

```
Salida de s_client — secciones
═══════════════════════════════

  CONNECTED(00000003)
  ─── Sección 1: Cadena de certificados ───
  depth=2 C=US, O=DigiCert, CN=DigiCert Global Root G2
  verify return:1
  depth=1 C=US, O=DigiCert, CN=DigiCert SHA2 Extended Validation
  verify return:1
  depth=0 CN=www.example.com, O=Example Inc
  verify return:1
  │
  │ depth=2 → CA raíz
  │ depth=1 → CA intermedia
  │ depth=0 → certificado del servidor
  │ verify return:1 → verificación OK en cada nivel

  ─── Sección 2: Certificado del servidor ───
  ---
  Certificate chain
   0 s:CN = www.example.com
     i:CN = DigiCert SHA2 Extended Validation
   1 s:CN = DigiCert SHA2 Extended Validation
     i:CN = DigiCert Global Root G2
  ---

  ─── Sección 3: Detalles del certificado ───
  Server certificate
  -----BEGIN CERTIFICATE-----
  MIIEtjCCA56gAw...
  -----END CERTIFICATE-----
  subject=CN = www.example.com
  issuer=CN = DigiCert SHA2 Extended Validation

  ─── Sección 4: Parámetros TLS negociados ───
  SSL handshake has read 3518 bytes and written 395 bytes
  New, TLSv1.3, Cipher is TLS_AES_256_GCM_SHA384
  │                  │              │
  │                  │              └── Cipher suite negociada
  │                  └── Versión de TLS
  │
  Verify return code: 0 (ok)
  │                   │
  │                   └── 0 = todo OK
  │                       Otros códigos = problemas
```

### Usos comunes de diagnóstico

```bash
# Ver solo el certificado del servidor
openssl s_client -connect server:443 -showcerts </dev/null 2>/dev/null | \
    openssl x509 -text -noout

# Verificar la fecha de expiración de un certificado remoto
echo | openssl s_client -connect www.example.com:443 2>/dev/null | \
    openssl x509 -noout -dates
# notBefore=Mar 21 00:00:00 2026 GMT
# notAfter=Mar 21 23:59:59 2027 GMT

# Ver los SANs de un certificado remoto
echo | openssl s_client -connect www.example.com:443 2>/dev/null | \
    openssl x509 -noout -ext subjectAltName

# Verificar un hostname específico (SNI)
openssl s_client -connect server:443 -servername www.example.com </dev/null

# Conectar a STARTTLS (SMTP, IMAP, etc.)
openssl s_client -connect mail.example.com:25 -starttls smtp
openssl s_client -connect mail.example.com:143 -starttls imap
openssl s_client -connect mail.example.com:389 -starttls ldap

# Probar una versión TLS específica
openssl s_client -connect server:443 -tls1_2
openssl s_client -connect server:443 -tls1_3

# Probar un cipher específico
openssl s_client -connect server:443 -cipher 'AES256-GCM-SHA384'

# Ver la cadena completa de certificados
openssl s_client -connect server:443 -showcerts </dev/null
```

### Códigos de error comunes

```
Verify return codes de s_client
═════════════════════════════════

  0  ok                         Todo correcto
  2  unable to get issuer       No se encontró la CA intermedia
  10 certificate has expired    Certificado expirado
  18 self signed certificate    Certificado autofirmado
  19 self signed in chain       CA autofirmada en la cadena
  20 unable to get local issuer No se encontró la CA raíz
                                en el trust store
  21 unable to verify first cert Primer certificado no verificable

  Error más común: 20 (unable to get local issuer certificate)
    → El servidor no envía la cadena intermedia
    → O la CA raíz no está en el trust store local
```

---

## 8. Cadena de confianza y CA bundle

### Cómo funciona la cadena

```
Cadena de confianza (chain of trust)
═════════════════════════════════════

  CA Raíz (DigiCert Global Root G2)      ← En el trust store del SO/browser
  │    Se confía porque viene preinstalada
  │
  │ firma
  ▼
  CA Intermedia (DigiCert SHA2 EV)       ← Debe enviarla el servidor
  │    Se confía porque la firmó la raíz
  │
  │ firma
  ▼
  Certificado del servidor               ← Debe enviarla el servidor
  (www.example.com)
  Se confía porque lo firmó la intermedia

  El servidor debe enviar: certificado + intermedia(s)
  El cliente tiene: la raíz preinstalada
```

### Archivo de cadena (chain file)

```bash
# El servidor necesita enviar su certificado + las intermedias
# Se concatenan en un solo archivo:

cat server.crt intermediate.crt > chain.crt
# Orden: servidor primero, intermedias después
# La raíz NO se incluye (el cliente ya la tiene)

# En Nginx:
# ssl_certificate /etc/nginx/ssl/chain.crt;      ← cert + chain
# ssl_certificate_key /etc/nginx/ssl/server.key;

# En Apache:
# SSLCertificateFile /etc/httpd/ssl/server.crt
# SSLCertificateChainFile /etc/httpd/ssl/intermediate.crt
# SSLCertificateKeyFile /etc/httpd/ssl/server.key
```

### CA bundle del sistema

```bash
# Dónde están las CAs raíz de confianza:

# RHEL / CentOS / Fedora:
/etc/pki/tls/certs/ca-bundle.crt               # Bundle PEM
/etc/pki/ca-trust/source/anchors/              # Agregar CAs custom

# Ubuntu / Debian:
/etc/ssl/certs/ca-certificates.crt             # Bundle PEM
/usr/local/share/ca-certificates/              # Agregar CAs custom
```

### Agregar una CA interna al trust store

```bash
# ═══════ RHEL / CentOS / Fedora ═══════
sudo cp internal-ca.crt /etc/pki/ca-trust/source/anchors/
sudo update-ca-trust

# Verificar
openssl verify -CAfile /etc/pki/tls/certs/ca-bundle.crt server.crt

# ═══════ Ubuntu / Debian ═══════
sudo cp internal-ca.crt /usr/local/share/ca-certificates/internal-ca.crt
sudo update-ca-certificates
# Adding 1 new certificate to /etc/ssl/certs/ca-certificates.crt

# Verificar
openssl verify server.crt
# server.crt: OK
```

---

## 9. Tareas comunes de sysadmin

### Monitorear expiración de certificados

```bash
# Verificar cuántos días quedan hasta la expiración
echo | openssl s_client -connect server:443 2>/dev/null | \
    openssl x509 -noout -checkend 2592000
# Retorna 0 si NO expira en 30 días (2592000 segundos)
# Retorna 1 si SÍ expira en 30 días

# Script para múltiples servidores
for host in web01 web02 mail01; do
    expiry=$(echo | openssl s_client -connect ${host}:443 2>/dev/null | \
        openssl x509 -noout -enddate 2>/dev/null)
    echo "${host}: ${expiry}"
done
```

### Renovar un certificado

```bash
# 1. Generar nuevo CSR (puedes reusar la misma clave o crear una nueva)
openssl req -new -key server.key -out server-renew.csr -config san.cnf

# 2. Enviar CSR a la CA (o usar certbot para Let's Encrypt)

# 3. Instalar el nuevo certificado
sudo cp new-server.crt /etc/nginx/ssl/server.crt
sudo cp new-chain.crt /etc/nginx/ssl/chain.crt

# 4. Verificar que clave y certificado coinciden
openssl x509 -in /etc/nginx/ssl/server.crt -noout -modulus | openssl md5
openssl rsa -in /etc/nginx/ssl/server.key -noout -modulus | openssl md5

# 5. Verificar la cadena
openssl verify -CAfile /etc/pki/tls/certs/ca-bundle.crt \
    -untrusted /etc/nginx/ssl/chain.crt /etc/nginx/ssl/server.crt

# 6. Recargar servicio
sudo nginx -t && sudo systemctl reload nginx
```

### Convertir formatos

```bash
# PEM a DER
openssl x509 -in cert.pem -outform DER -out cert.der

# DER a PEM
openssl x509 -in cert.der -inform DER -outform PEM -out cert.pem

# PEM (clave+cert) a PFX/PKCS12 (para Windows/IIS)
openssl pkcs12 -export -in cert.pem -inkey key.pem \
    -certfile chain.pem -out certificate.pfx
# Pide contraseña para proteger el PFX

# PFX a PEM (extraer todo)
openssl pkcs12 -in certificate.pfx -out all.pem -nodes
# -nodes = no cifrar la clave privada extraída

# Extraer solo el certificado del PFX
openssl pkcs12 -in certificate.pfx -clcerts -nokeys -out cert.pem

# Extraer solo la clave privada del PFX
openssl pkcs12 -in certificate.pfx -nocerts -nodes -out key.pem
```

### Let's Encrypt con certbot

```bash
# Instalar certbot
sudo dnf install certbot python3-certbot-nginx    # RHEL
sudo apt install certbot python3-certbot-nginx     # Debian

# Obtener certificado (Nginx)
sudo certbot --nginx -d example.com -d www.example.com

# Obtener certificado (standalone, sin servidor web)
sudo certbot certonly --standalone -d example.com

# Renovar (se configura automáticamente en cron/systemd timer)
sudo certbot renew

# Verificar renovación automática
sudo certbot renew --dry-run

# Archivos generados por certbot:
# /etc/letsencrypt/live/example.com/
# ├── cert.pem       ← certificado del servidor
# ├── chain.pem      ← cadena de intermedias
# ├── fullchain.pem  ← cert.pem + chain.pem (para Nginx)
# └── privkey.pem    ← clave privada
```

---

## 10. Errores comunes

### Error 1: chain incompleta (falta la intermedia)

```bash
# Síntoma: funciona en algunos navegadores pero no en otros
# Algunos navegadores cachean intermedias, otros no

# Diagnóstico:
echo | openssl s_client -connect server:443 2>/dev/null
# verify error:num=20:unable to get local issuer certificate

# El servidor envía solo su certificado, no la intermedia
# Solución: concatenar certificado + intermedia(s)
cat server.crt intermediate.crt > fullchain.crt
# Configurar el servidor con fullchain.crt
```

### Error 2: certificado y clave no coinciden

```bash
# Síntoma: nginx/apache no arranca
# nginx: SSL_CTX_use_PrivateKey_file failed
# apache: certificate and private key do not match

# Diagnóstico: comparar modulus
openssl x509 -in cert.crt -noout -modulus | openssl md5
# MD5(stdin)= abc123...
openssl rsa -in server.key -noout -modulus | openssl md5
# MD5(stdin)= def456...    ← DIFERENTE = no coinciden

# Causa: se renovó el certificado con un CSR de otra clave
# Solución: generar nuevo CSR con la clave correcta, o generar
# nueva clave y CSR juntos
```

### Error 3: certificado expirado

```bash
# Diagnóstico:
echo | openssl s_client -connect server:443 2>/dev/null | \
    openssl x509 -noout -dates
# notAfter=Jan 15 00:00:00 2025 GMT    ← ya pasó

# Prevención: monitorizar con -checkend
echo | openssl s_client -connect server:443 2>/dev/null | \
    openssl x509 -noout -checkend 2592000
# Certificate will expire  ← ¡renovar ahora!
```

### Error 4: SAN missing (Chrome rechaza sin SAN)

```bash
# Síntoma: Chrome muestra NET::ERR_CERT_COMMON_NAME_INVALID
# Aunque el CN es correcto

# Chrome ignora CN desde 2017, requiere SAN
# Diagnóstico:
echo | openssl s_client -connect server:443 2>/dev/null | \
    openssl x509 -noout -ext subjectAltName
# (vacío) ← no tiene SAN → error en Chrome

# Solución: regenerar certificado con SAN usando -addext o san.cnf
```

### Error 5: permisos incorrectos en la clave privada

```bash
# Síntoma: nginx/apache no arranca
# nginx: cannot load certificate key: permission denied

# Causa:
ls -l /etc/nginx/ssl/server.key
# -rw-r--r-- 1 nginx nginx ... server.key   ← demasiado permisivo

# Solución:
chmod 600 /etc/nginx/ssl/server.key
chown root:root /etc/nginx/ssl/server.key
# Solo root debe poder leer la clave privada
# (nginx la lee como root antes de hacer setuid)
```

---

## 11. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════╗
║                    OpenSSL — Cheatsheet                         ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║  GENERAR CLAVES                                                  ║
║  openssl genrsa -out key.pem 4096          RSA 4096              ║
║  openssl genpkey -algorithm EC \                                 ║
║    -pkeyopt ec_paramgen_curve:P-256 -out key.pem   EC P-256     ║
║  openssl genpkey -algorithm ED25519 -out key.pem   Ed25519      ║
║                                                                  ║
║  GENERAR CSR                                                     ║
║  openssl req -new -key key.pem -out csr.pem       Interactivo   ║
║  openssl req -new -key key.pem -out csr.pem \                   ║
║    -config san.cnf                                Con SAN        ║
║                                                                  ║
║  CERTIFICADO AUTOFIRMADO                                         ║
║  openssl req -x509 -newkey rsa:4096 -nodes \                    ║
║    -keyout key.pem -out cert.pem -days 365 \                    ║
║    -subj "/CN=host" -addext "subjectAltName=DNS:host"           ║
║                                                                  ║
║  INSPECCIONAR                                                    ║
║  openssl x509 -in cert.pem -text -noout    Todo el contenido    ║
║  openssl x509 -in cert.pem -dates -noout   Solo fechas          ║
║  openssl x509 -in cert.pem -subject -noout Solo subject         ║
║  openssl x509 -in cert.pem -issuer -noout  Solo issuer          ║
║  openssl x509 -in cert.pem -noout -ext subjectAltName   SANs   ║
║  openssl req -in csr.pem -text -noout      Inspeccionar CSR     ║
║                                                                  ║
║  VERIFICAR                                                       ║
║  openssl verify cert.pem                   Contra trust store    ║
║  openssl verify -CAfile ca.crt cert.pem    Contra CA específica ║
║  openssl x509 -noout -modulus cert.pem | md5   ┐                ║
║  openssl rsa  -noout -modulus key.pem  | md5   ┘ ¿Coinciden?    ║
║                                                                  ║
║  S_CLIENT (diagnóstico TLS)                                      ║
║  openssl s_client -connect host:443                Conexión      ║
║  ... -servername host                              SNI           ║
║  ... -starttls smtp                                STARTTLS      ║
║  ... -showcerts                                    Ver cadena    ║
║  echo | openssl s_client -connect host:443 2>/dev/null | \      ║
║    openssl x509 -noout -dates              Expiración remota    ║
║                                                                  ║
║  CONVERSIÓN                                                      ║
║  openssl x509 -outform DER                 PEM → DER            ║
║  openssl x509 -inform DER -outform PEM     DER → PEM            ║
║  openssl pkcs12 -export -in cert -inkey key -out f.pfx  → PFX  ║
║  openssl pkcs12 -in f.pfx -out all.pem -nodes          PFX →   ║
║                                                                  ║
║  CA TRUST STORE                                                  ║
║  RHEL:   /etc/pki/ca-trust/source/anchors/ + update-ca-trust    ║
║  Debian: /usr/local/share/ca-certificates/ + update-ca-certs    ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Generar clave, CSR y certificado autofirmado

```bash
# Paso 1: Generar clave RSA 4096
openssl genrsa -out myserver.key 4096

# Paso 2: Generar CSR
openssl req -new -key myserver.key -out myserver.csr \
    -subj "/CN=myserver.local/O=Lab/C=ES"

# Paso 3: Verificar el CSR
openssl req -in myserver.csr -text -noout | head -15
```

> **Pregunta de predicción**: si generas el certificado autofirmado con
> `-days 365` sin agregar SANs, ¿funcionará en Chrome? ¿Y con `curl`?
>
> **Respuesta**: **Chrome** lo rechazará con `NET::ERR_CERT_COMMON_NAME_INVALID`
> porque desde 2017 requiere SANs. **curl** con `-k` (insecure) sí conectará
> pero mostrará advertencia. `curl` sin `-k` contra un certificado autofirmado
> fallará con `SSL certificate problem: self signed certificate`. Para que
> funcione en Chrome necesitas agregar `subjectAltName=DNS:myserver.local`.

```bash
# Paso 4: Generar certificado autofirmado CON SAN
openssl req -x509 -key myserver.key -out myserver.crt \
    -days 365 -subj "/CN=myserver.local" \
    -addext "subjectAltName=DNS:myserver.local,IP:127.0.0.1"
```

### Ejercicio 2: Diagnosticar un servidor con s_client

```bash
# Paso 1: Conectar a un servidor público
echo | openssl s_client -connect google.com:443 2>/dev/null | \
    openssl x509 -noout -subject -issuer -dates -ext subjectAltName
```

> **Pregunta de predicción**: ¿cuántos niveles de `depth` (profundidad)
> esperarías ver en la cadena de certificados de google.com? ¿Qué organización
> esperarías ver como issuer?
>
> **Respuesta**: generalmente 3 niveles: depth=0 (certificado de google.com),
> depth=1 (CA intermedia, probablemente "GTS CA" de Google Trust Services),
> depth=2 (CA raíz, probablemente "GlobalSign" u otra raíz). Google opera su
> propia CA intermedia (Google Trust Services) desde 2017.

```bash
# Paso 2: Verificar cuándo expira
echo | openssl s_client -connect google.com:443 2>/dev/null | \
    openssl x509 -noout -checkend 604800
# ¿Expira en los próximos 7 días? (604800 segundos)
```

### Ejercicio 3: Verificar que clave y certificado coinciden

```bash
# Paso 1: Generar clave y certificado
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout test.key -out test.crt -days 30 -subj "/CN=test"

# Paso 2: Generar OTRA clave (para simular el error)
openssl genrsa -out wrong.key 2048

# Paso 3: Comparar modulus
openssl x509 -in test.crt -noout -modulus | openssl md5
openssl rsa -in test.key -noout -modulus | openssl md5
openssl rsa -in wrong.key -noout -modulus | openssl md5
```

> **Pregunta de predicción**: ¿cuáles de los tres MD5 coincidirán? Si
> configuras nginx con `test.crt` y `wrong.key`, ¿arrancará el servicio?
>
> **Respuesta**: los MD5 de `test.crt` y `test.key` coincidirán (son del
> mismo par). El MD5 de `wrong.key` será diferente. Si configuras nginx con
> `test.crt` y `wrong.key`, nginx **no arrancará** y mostrará un error como
> `SSL_CTX_use_PrivateKey_file ... key values mismatch`. Esta verificación de
> modulus es el diagnóstico estándar para este tipo de error.

---

> **Siguiente tema**: T02 — LUKS (cifrado de disco, cryptsetup, desbloqueo
> al arranque).
