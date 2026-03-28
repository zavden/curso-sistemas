# Herramientas de diagnóstico LDAP

## Índice

1. [Panorama de herramientas](#1-panorama-de-herramientas)
2. [ldapsearch — la navaja suiza](#2-ldapsearch--la-navaja-suiza)
3. [ldapwhoami, ldapcompare y ldappasswd](#3-ldapwhoami-ldapcompare-y-ldappasswd)
4. [ldapadd y ldapmodify](#4-ldapadd-y-ldapmodify)
5. [sssctl — diagnóstico de sssd](#5-sssctl--diagnóstico-de-sssd)
6. [Logs de sssd](#6-logs-de-sssd)
7. [Diagnóstico de red y TLS](#7-diagnóstico-de-red-y-tls)
8. [Escenarios de diagnóstico](#8-escenarios-de-diagnóstico)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Panorama de herramientas

El diagnóstico de una infraestructura LDAP-client involucra tres capas, cada una con sus herramientas:

```
┌─────────────────────────────────────────────────────────┐
│                  Aplicación / Login                     │
│  pamtester, su, ssh, getent, id                         │
├─────────────────────────────────────────────────────────┤
│                       sssd                              │
│  sssctl, sssd logs (/var/log/sssd/)                     │
├─────────────────────────────────────────────────────────┤
│              Protocolo LDAP directo                     │
│  ldapsearch, ldapwhoami, ldapadd, ldapmodify            │
├─────────────────────────────────────────────────────────┤
│              Red / TLS                                  │
│  openssl s_client, tcpdump, ss                          │
└─────────────────────────────────────────────────────────┘
```

La estrategia general de diagnóstico es **de abajo hacia arriba**: primero verificar que la red y TLS funcionan, luego que las consultas LDAP directas devuelven resultados, después que sssd funciona, y finalmente que NSS/PAM están integrados correctamente.

### Paquetes necesarios

```bash
# RHEL/Fedora
dnf install openldap-clients sssd-tools

# Debian/Ubuntu
apt install ldap-utils sssd-tools
```

---

## 2. ldapsearch — la navaja suiza

`ldapsearch` es la herramienta fundamental para consultar directorios LDAP directamente, sin pasar por sssd. Esto permite aislar si un problema es de sssd/NSS/PAM o del propio directorio.

### Anatomía del comando

```bash
ldapsearch [opciones de conexión] [opciones de bind] [opciones de búsqueda] \
    [filtro] [atributos...]
```

```
ldapsearch -x -H ldaps://ldap.example.com \
           -D "cn=admin,dc=example,dc=com" -W \
           -b "dc=example,dc=com" -s sub \
           "(uid=alice)" cn mail uidNumber
│          │                                │
│          │                                └── Atributos a devolver
│          │                                    (vacío = todos)
│          │
│          ├── -x         simple auth (no SASL)
│          ├── -H URI     servidor
│          ├── -D DN      bind DN (identidad)
│          ├── -W         pedir contraseña interactivamente
│          ├── -b DN      base de búsqueda
│          ├── -s scope   base | one | sub
│          └── "(filter)" filtro LDAP
│
└── El comando
```

### Opciones de conexión

| Opción | Descripción | Ejemplo |
|---|---|---|
| `-H URI` | Servidor LDAP | `-H ldaps://ldap.example.com` |
| `-ZZ` | Exigir StartTLS (falla si no puede) | `-H ldap://... -ZZ` |
| `-Z` | Intentar StartTLS (continúa sin TLS) | `-H ldap://... -Z` |
| `-h host` | Host (legacy, preferir `-H`) | `-h ldap.example.com` |
| `-p port` | Puerto (legacy) | `-p 636` |

### Opciones de bind (autenticación)

| Opción | Descripción | Ejemplo |
|---|---|---|
| `-x` | Simple authentication (no SASL) | Casi siempre necesario |
| `-D DN` | Bind DN (quién eres) | `-D "cn=admin,dc=example,dc=com"` |
| `-W` | Pedir contraseña interactivamente | Seguro, recomendado |
| `-w pass` | Contraseña en línea de comandos | Visible en `ps`, inseguro |
| `-y file` | Leer contraseña de archivo | `-y /root/.ldap-pass` |
| (ninguno) | Bind anónimo | Solo si el servidor lo permite |

### Opciones de búsqueda

| Opción | Descripción | Ejemplo |
|---|---|---|
| `-b DN` | Base DN de búsqueda | `-b "ou=People,dc=example,dc=com"` |
| `-s scope` | Alcance: `base`, `one`, `sub` | `-s one` (solo hijos directos) |
| `-l time` | Límite de tiempo (segundos) | `-l 10` |
| `-z num` | Límite de entradas devueltas | `-z 100` |
| `-S attr` | Ordenar por atributo | `-S uid` |

### Opciones de formato de salida

| Opción | Descripción | Uso |
|---|---|---|
| `-LLL` | Salida LDIF limpia (sin comentarios ni versión) | Scripting |
| `-LL` | Salida LDIF con comentarios | Legibilidad |
| `-L` | Salida LDIF con versión | Compatibilidad |
| (ninguno) | Salida con metadatos (entradas, tiempo) | Debug |

### Ejemplos prácticos

```bash
# Buscar un usuario específico
ldapsearch -x -H ldaps://ldap.example.com \
    -D "cn=readonly,dc=example,dc=com" -W \
    -b "dc=example,dc=com" \
    "(uid=alice)"

# Buscar todos los usuarios POSIX con UID >= 1000
ldapsearch -x -H ldaps://ldap.example.com \
    -D "cn=readonly,dc=example,dc=com" -W \
    -b "ou=People,dc=example,dc=com" \
    "(&(objectClass=posixAccount)(uidNumber>=1000))" \
    uid cn uidNumber

# Listar todos los grupos
ldapsearch -x -H ldaps://ldap.example.com \
    -D "cn=readonly,dc=example,dc=com" -W \
    -b "ou=Groups,dc=example,dc=com" \
    "(objectClass=posixGroup)" \
    cn gidNumber memberUid

# Contar entradas (sin mostrar datos)
ldapsearch -x -H ldaps://ldap.example.com \
    -D "cn=readonly,dc=example,dc=com" -W \
    -b "dc=example,dc=com" \
    "(objectClass=posixAccount)" dn | grep "^dn:" | wc -l

# Buscar en todo el árbol qué OUs existen
ldapsearch -x -H ldaps://ldap.example.com \
    -D "cn=readonly,dc=example,dc=com" -W \
    -b "dc=example,dc=com" \
    "(objectClass=organizationalUnit)" \
    -LLL dn

# Bind anónimo (si el servidor lo permite)
ldapsearch -x -H ldap://ldap.example.com \
    -b "dc=example,dc=com" "(uid=alice)"
```

### Interpretar la salida

```
# search result                          ← resumen al final
search: 2
result: 0 Success                        ← código 0 = OK
# numResponses: 2                        ← 1 entrada + 1 resultado
# numEntries: 1                          ← 1 entrada encontrada
```

Códigos de resultado comunes:

| Código | Nombre | Causa típica |
|---|---|---|
| 0 | Success | OK (aunque puede devolver 0 entradas) |
| 1 | Operations error | Error interno del servidor |
| 4 | Size limit exceeded | Más resultados que el límite (`-z`) |
| 10 | Referral | El servidor redirige a otro (seguir con `-C`) |
| 32 | No such object | El base DN no existe |
| 34 | Invalid DN syntax | DN mal formado |
| 49 | Invalid credentials | Contraseña incorrecta o DN de bind inexistente |
| 50 | Insufficient access | Sin permisos para esta operación |

### ldapsearch con SASL/Kerberos

```bash
# Si el sistema tiene ticket Kerberos (en entorno AD/IPA)
ldapsearch -Y GSSAPI \
    -H ldap://dc.corp.example.com \
    -b "dc=corp,dc=example,dc=com" \
    "(sAMAccountName=alice)"
# -Y GSSAPI: usa ticket Kerberos existente (klist para verificar)
# No necesita -x, -D, -W
```

---

## 3. ldapwhoami, ldapcompare y ldappasswd

### ldapwhoami — verificar bind

La forma más rápida de verificar que las credenciales funcionan y la conexión es correcta:

```bash
# ¿Puedo conectar y autenticarme?
ldapwhoami -x -H ldaps://ldap.example.com \
    -D "cn=readonly,dc=example,dc=com" -W
# Resultado exitoso:
# dn:cn=readonly,dc=example,dc=com

# Bind anónimo
ldapwhoami -x -H ldap://ldap.example.com
# anonymous

# Con Kerberos
ldapwhoami -Y GSSAPI -H ldap://dc.corp.example.com
# dn:uid=alice,cn=users,dc=corp,dc=example,dc=com
```

Este comando es ideal como primera prueba de conectividad: si `ldapwhoami` falla, ninguna otra operación LDAP funcionará.

### ldapcompare — verificar un atributo

Compara si un atributo tiene un valor específico sin leer la entrada:

```bash
# ¿alice tiene loginShell /bin/bash?
ldapcompare -x -H ldaps://ldap.example.com \
    -D "cn=readonly,dc=example,dc=com" -W \
    "uid=alice,ou=People,dc=example,dc=com" \
    "loginShell:/bin/bash"
# TRUE    ← sí
# FALSE   ← no

# Útil para verificar membresía en un grupo
ldapcompare -x -H ldaps://ldap.example.com \
    -D "cn=readonly,dc=example,dc=com" -W \
    "cn=developers,ou=Groups,dc=example,dc=com" \
    "memberUid:alice"
# TRUE si alice es miembro
```

### ldappasswd — cambiar contraseña LDAP

```bash
# Cambiar contraseña de otro usuario (como admin)
ldappasswd -x -H ldaps://ldap.example.com \
    -D "cn=admin,dc=example,dc=com" -W \
    -S "uid=alice,ou=People,dc=example,dc=com"
# -W: pide contraseña del admin (bind)
# -S: pide la nueva contraseña de alice

# Cambiar tu propia contraseña
ldappasswd -x -H ldaps://ldap.example.com \
    -D "uid=alice,ou=People,dc=example,dc=com" -W \
    -S -A
# -A: pide la contraseña antigua (verificación extra)
# -S: pide la nueva contraseña
```

---

## 4. ldapadd y ldapmodify

Aunque son herramientas de escritura, son útiles en diagnóstico para probar permisos y verificar que el esquema acepta ciertas operaciones.

### ldapadd — crear entradas

```bash
# Desde un archivo LDIF
ldapadd -x -H ldaps://ldap.example.com \
    -D "cn=admin,dc=example,dc=com" -W \
    -f new_entries.ldif

# Desde stdin (para pruebas rápidas)
ldapadd -x -H ldaps://ldap.example.com \
    -D "cn=admin,dc=example,dc=com" -W <<EOF
dn: uid=testuser,ou=People,dc=example,dc=com
objectClass: inetOrgPerson
objectClass: posixAccount
uid: testuser
cn: Test User
sn: User
uidNumber: 9999
gidNumber: 9999
homeDirectory: /home/testuser
loginShell: /bin/bash
EOF
# adding new entry "uid=testuser,ou=People,dc=example,dc=com"
```

### ldapmodify — modificar entradas

```bash
# Cambiar un atributo
ldapmodify -x -H ldaps://ldap.example.com \
    -D "cn=admin,dc=example,dc=com" -W <<EOF
dn: uid=testuser,ou=People,dc=example,dc=com
changetype: modify
replace: loginShell
loginShell: /bin/zsh
-
EOF
# modifying entry "uid=testuser,ou=People,dc=example,dc=com"
```

### ldapdelete — eliminar entradas

```bash
ldapdelete -x -H ldaps://ldap.example.com \
    -D "cn=admin,dc=example,dc=com" -W \
    "uid=testuser,ou=People,dc=example,dc=com"
```

### Errores frecuentes de escritura

```bash
# Error 65: Object class violation
# Falta un atributo MUST o objectClass inválido
adding new entry "uid=test,ou=People,dc=example,dc=com"
ldap_add: Object class violation (65)
    additional info: object class 'inetOrgPerson' requires attribute 'sn'

# Error 68: Already exists
ldap_add: Already exists (68)

# Error 50: Insufficient access rights
# El bind DN no tiene permisos de escritura
ldap_add: Insufficient access (50)

# Error 32: No such object
# El parent DN no existe
ldap_add: No such object (32)
    matched DN: dc=example,dc=com
#   ^^^ matched DN indica hasta dónde SÍ existe el árbol
```

---

## 5. sssctl — diagnóstico de sssd

`sssctl` es el frontend de diagnóstico de sssd. Reemplaza herramientas antiguas y proporciona una interfaz unificada.

### Estado del dominio

```bash
# Estado general de un dominio
sssctl domain-status lab.internal
# Online status: Online
# Active server: ldaps://ldap.lab.internal
# Number of discovered servers: 1

# Si dice Offline:
# → sssd no puede contactar al servidor
# → Está usando caché
# → Verificar red/TLS/firewall

# Listar dominios configurados
sssctl domain-list
# lab.internal
# corp.example.com
```

### Verificación de usuarios

```bash
# Verificación completa de un usuario
sssctl user-checks alice
# user: alice
# action: acct
# service: system-auth
#
# SSSD nss user lookup result:
#  - user name: alice
#  - user id: 1001
#  - group id: 1001
#  - gecos: Alice Johnson
#  - home directory: /home/alice
#  - shell: /bin/bash
#
# SSSD InfoPipe user lookup result:
#  - name: alice
#  - uidNumber: 1001
#  - gidNumber: 1001
#  - ...
#
# testing pam_acct_mgmt
# pam_acct_mgmt: Success       ← access_provider permite a alice

# Con servicio específico (útil si PAM difiere por servicio)
sssctl user-checks alice -s sshd
sssctl user-checks alice -s su

# Si pam_acct_mgmt dice "Permission denied":
# → access_provider deniega al usuario
# → Revisar simple_allow_users/groups en sssd.conf
```

### Validación de configuración

```bash
# Verificar errores en sssd.conf
sssctl config-check
# No issues found.
# O:
# Issues identified by validators: 1
# [rule/allowed_sections]: Section [domian/lab.internal] is not allowed.
#                          ^^^ typo: "domian" en vez de "domain"

# Esto detecta:
# - Secciones inválidas
# - Opciones desconocidas
# - Valores con formato incorrecto
# - Permisos incorrectos del archivo
```

### Gestión de caché

```bash
# Invalidar toda la caché (marca como expirada, no borra)
sssctl cache-expire -E
# sssd reconsultará al servidor en la próxima petición

# Invalidar un usuario específico
sssctl cache-expire -u alice

# Invalidar un grupo
sssctl cache-expire -g developers

# Invalidar todo de un dominio
sssctl cache-expire -d lab.internal -E

# Ver caché de un usuario (qué tiene sssd almacenado)
sssctl cache-list -u
# Lista todos los usuarios cacheados

# Exportar entrada de caché de un usuario
ldbsearch -H /var/lib/sss/db/cache_lab.internal.ldb \
    "(&(objectClass=user)(name=alice))"
# ^^^ acceso directo a la base de datos LDB de sssd
```

### Control de debug

```bash
# Cambiar nivel de debug en caliente (sin reiniciar)
sssctl debug-level 6
# Aplica a todos los componentes inmediatamente

# Volver a nivel normal
sssctl debug-level 2

# Niveles:
# 0 = fatal errors only
# 1 = critical errors
# 2 = serious errors (default)
# 3 = non-critical failures
# 4 = configuration info
# 5 = function data
# 6 = trace messages for operations
# 7 = trace messages for internal functions
# 8 = contents of internal variables
# 9 = extremely detailed trace
```

> **Nivel 6** suele ser suficiente para diagnosticar la mayoría de problemas. Niveles 7-9 generan volúmenes enormes de logs y están destinados a los desarrolladores de sssd.

---

## 6. Logs de sssd

### Ubicación y estructura

```bash
ls /var/log/sssd/
# sssd.log                    ← monitor principal
# sssd_nss.log                ← respondedor NSS
# sssd_pam.log                ← respondedor PAM
# sssd_sudo.log               ← respondedor sudo (si habilitado)
# sssd_ssh.log                ← respondedor ssh (si habilitado)
# sssd_lab.internal.log       ← backend del dominio "lab.internal"
# sssd_corp.example.com.log   ← backend de otro dominio
```

### Qué buscar en cada log

| Log | Cuándo consultar | Qué buscar |
|---|---|---|
| `sssd.log` | sssd no arranca | Errores de configuración, permisos |
| `sssd_nss.log` | `getent` no devuelve usuarios | Peticiones NSS, respuestas del backend |
| `sssd_pam.log` | Login falla | auth result, acct_mgmt result |
| `sssd_DOMAIN.log` | Problemas de conexión LDAP/AD | Bind, search, TLS, failover |

### Leer logs de sssd

```bash
# Formato típico de un log de sssd:
# (TIMESTAMP) [sssd[be[DOMAIN]]] [COMPONENTE] (NIVEL): MENSAJE

# Ejemplo de búsqueda exitosa (debug_level >= 6):
(2026-03-21 10:15:32): [sssd[be[lab.internal]]] [sdap_get_generic_op_finished]
    (0x0400): Search result: SUCCESS (0), no errmsg set
(2026-03-21 10:15:32): [sssd[be[lab.internal]]] [sdap_get_generic_op_finished]
    (0x1000): Search returned 1 results.
```

### Patrones de búsqueda en logs

```bash
# Buscar errores de autenticación
grep -i "authentication" /var/log/sssd/sssd_pam.log | tail -20

# Buscar fallos de conexión LDAP
grep -iE "(connect|bind|tls|error|fail)" /var/log/sssd/sssd_lab.internal.log | tail -20

# Buscar cambios online/offline
grep -i "offline\|online\|going" /var/log/sssd/sssd_lab.internal.log

# Buscar un usuario específico
grep "alice" /var/log/sssd/sssd_pam.log
grep "alice" /var/log/sssd/sssd_nss.log

# Buscar resultados de access control
grep "access" /var/log/sssd/sssd_pam.log

# Seguir logs en tiempo real (mientras pruebas login en otra terminal)
tail -f /var/log/sssd/sssd_pam.log /var/log/sssd/sssd_lab.internal.log
```

### Mensajes clave y su significado

```
=== sssd_pam.log ===

"Pam account mgmt: SUCCESS"
  → access_provider permite al usuario

"Pam account mgmt: Permission denied"
  → access_provider deniega (simple_allow_users/groups)

"Pam authenticate: SUCCESS"
  → contraseña correcta

"Pam authenticate: Authentication failure"
  → contraseña incorrecta o usuario no encontrado

=== sssd_DOMAIN.log ===

"sdap_cli_connect_recv: Successful connection"
  → Conexión LDAP establecida

"sdap_auth_bind: Bind successful"
  → Autenticación del usuario contra LDAP exitosa

"Going offline"
  → sssd pierde contacto con el servidor

"Back online"
  → sssd reconecta al servidor

"Server not found"
  → DNS no resuelve o el servidor no responde

"TLS: peer cert untrusted or revoked"
  → Problema con certificado TLS del servidor

"Invalid credentials"
  → Contraseña del bind DN incorrecta (ldap_default_authtok)
```

### journalctl para sssd

```bash
# Logs vía systemd journal
journalctl -u sssd --since "10 minutes ago"

# Solo errores
journalctl -u sssd -p err

# Seguir en tiempo real
journalctl -u sssd -f

# Logs de arranque (problemas de inicio)
journalctl -u sssd -b
```

---

## 7. Diagnóstico de red y TLS

Cuando `ldapsearch` falla antes de poder hacer bind, el problema suele estar en la capa de red o TLS.

### Conectividad básica

```bash
# ¿El puerto está abierto?
ss -tn state established dst ldap.example.com
# O probar la conexión:
nc -zv ldap.example.com 636    # LDAPS
nc -zv ldap.example.com 389    # LDAP/StartTLS

# ¿DNS resuelve el servidor?
host ldap.example.com
dig ldap.example.com

# ¿Descubrimiento SRV para AD?
dig _ldap._tcp.corp.example.com SRV
# Devuelve servidores DC con prioridad y peso

# ¿Firewall local bloquea?
# RHEL:
firewall-cmd --list-all
# Debian:
iptables -L -n | grep -E "636|389"
```

### Diagnóstico TLS con openssl

```bash
# Verificar certificado del servidor LDAPS (puerto 636)
openssl s_client -connect ldap.example.com:636 -showcerts
# Muestra:
#   - Cadena de certificados
#   - Fecha de expiración
#   - CA que lo firmó
#   - Si la verificación pasa (Verify return code: 0 = ok)

# Verificar StartTLS (puerto 389)
openssl s_client -connect ldap.example.com:389 -starttls ldap -showcerts

# Verificar fecha de expiración del certificado
echo | openssl s_client -connect ldap.example.com:636 2>/dev/null \
    | openssl x509 -noout -dates
# notBefore=Jan  1 00:00:00 2025 GMT
# notAfter=Dec 31 23:59:59 2026 GMT

# Verificar que la CA local puede validar el cert del servidor
openssl s_client -connect ldap.example.com:636 \
    -CAfile /etc/ssl/certs/ca-certificates.crt
# Verify return code: 0 (ok)       ← TLS OK
# Verify return code: 19 (self-signed certificate in chain) ← CA no instalada
# Verify return code: 21 (unable to verify the first certificate) ← CA falta
```

### Errores TLS comunes

```
Error: "TLS: peer cert untrusted or revoked"
Causa: La CA que firmó el certificado del servidor no está instalada
Fix:
  # Instalar certificado de CA
  cp ldap-ca.pem /etc/pki/ca-trust/source/anchors/   # RHEL
  update-ca-trust extract
  cp ldap-ca.pem /usr/local/share/ca-certificates/ldap-ca.crt  # Debian
  update-ca-certificates

Error: "TLS: hostname does not match CN"
Causa: El CN o SAN del certificado no coincide con el hostname usado
Fix:  Usar el hostname exacto que aparece en el certificado
      O añadir TLS_REQCERT allow (solo testing, NUNCA producción)

Error: "TLS: can't connect: error:14090086"
Causa: Certificado expirado
Fix:  Renovar certificado en el servidor LDAP
```

### Configuración TLS del cliente LDAP

```bash
# /etc/ldap/ldap.conf (Debian) o /etc/openldap/ldap.conf (RHEL)
# Este archivo afecta a ldapsearch y otras herramientas ldap*

URI         ldaps://ldap.example.com
BASE        dc=example,dc=com
TLS_CACERT  /etc/ssl/certs/ca-certificates.crt
TLS_REQCERT demand

# NOTA: sssd usa sus propios parámetros TLS (ldap_tls_*)
# en sssd.conf, NO lee este archivo
```

```
ldap.conf (afecta)     →  ldapsearch, ldapadd, ldapmodify, ldapwhoami
sssd.conf (afecta)     →  sssd (ldap_tls_reqcert, ldap_tls_cacert)
Son INDEPENDIENTES — un cambio en uno NO afecta al otro
```

### Captura de tráfico LDAP

```bash
# Capturar tráfico LDAP plano (puerto 389 sin TLS)
tcpdump -i eth0 -n port 389 -w /tmp/ldap.pcap
# Abrir con Wireshark: se ven las operaciones LDAP en claro

# Tráfico LDAPS/StartTLS: el contenido está cifrado
# Solo verás el handshake TLS, no las operaciones LDAP
tcpdump -i eth0 -n port 636 -w /tmp/ldaps.pcap

# Ver si hay conexiones activas a LDAP
ss -tnp | grep -E "389|636"
# ESTAB  0  0  10.0.0.50:43210  10.0.0.10:636  users:(("sssd_be",pid=1234))
```

---

## 8. Escenarios de diagnóstico

### Escenario 1: sssd no arranca

```bash
# Paso 1: ver por qué
systemctl status sssd
journalctl -u sssd -b --no-pager | tail -30

# Causas comunes:
# ┌──────────────────────────────────────────────────────────────┐
# │ "File ownership and permissions check failed"                │
# │  → chmod 0600 /etc/sssd/sssd.conf && chown root:root ...    │
# ├──────────────────────────────────────────────────────────────┤
# │ "No domains configured"                                     │
# │  → falta "domains = ..." en [sssd]                          │
# ├──────────────────────────────────────────────────────────────┤
# │ "Missing config_file_version"                               │
# │  → añadir config_file_version = 2 en [sssd]                 │
# ├──────────────────────────────────────────────────────────────┤
# │ "Unknown option" o "Section not allowed"                     │
# │  → typo en nombre de sección u opción (sssctl config-check) │
# └──────────────────────────────────────────────────────────────┘

# Paso 2: validar config
sssctl config-check
```

### Escenario 2: getent no devuelve usuario LDAP

```bash
# Paso 1: ¿sssd corre y está online?
systemctl is-active sssd
sssctl domain-status lab.internal
# Si offline → problema de red/TLS (ir a escenario 4)

# Paso 2: ¿nsswitch.conf tiene sss?
grep passwd /etc/nsswitch.conf
# Si falta sss → añadirlo

# Paso 3: ¿libnss_sss está instalada?
ls /lib*/libnss_sss*
# Si no existe → instalar sssd-client o libnss-sss

# Paso 4: ¿ldapsearch directo funciona?
ldapsearch -x -H ldaps://ldap.lab.internal \
    -D "cn=readonly,dc=lab,dc=internal" -W \
    -b "ou=People,dc=lab,dc=internal" "(uid=alice)"
# Si devuelve resultados → problema en sssd, no en LDAP
# Si no devuelve → problema en LDAP/permisos/filtro

# Paso 5: ¿sssd busca en el lugar correcto?
# Verificar ldap_search_base en sssd.conf
# Verificar ldap_user_search_filter (puede estar filtrando)

# Paso 6: invalidar caché y reintentar
sssctl cache-expire -u alice
getent passwd alice
```

### Escenario 3: contraseña correcta pero login denegado

```bash
# Paso 1: verificar qué dice PAM
sssctl user-checks alice -s sshd
# Si pam_acct_mgmt dice "Permission denied":

# Paso 2: ¿qué access_provider está configurado?
grep access_provider /etc/sssd/sssd.conf
# access_provider = simple

# Paso 3: ¿alice está en los grupos permitidos?
grep simple_allow /etc/sssd/sssd.conf
# simple_allow_groups = sysadmins

id alice
# Verificar si alice pertenece a sysadmins

# Paso 4: solución
# Añadir alice al grupo o añadir su grupo a simple_allow_groups
# O cambiar a access_provider = permit (permite a todos)
```

### Escenario 4: sssd pasa a offline

```bash
# Paso 1: ¿desde cuándo?
grep -i "going offline\|back online" /var/log/sssd/sssd_lab.internal.log | tail -10

# Paso 2: ¿red funciona?
ping ldap.lab.internal
nc -zv ldap.lab.internal 636

# Paso 3: ¿TLS funciona?
openssl s_client -connect ldap.lab.internal:636 </dev/null 2>&1 | head -20

# Paso 4: ¿el bind DN funciona?
ldapwhoami -x -H ldaps://ldap.lab.internal \
    -D "cn=readonly,dc=lab,dc=internal" -W
# Si error 49 → contraseña de bind incorrecta en sssd.conf

# Paso 5: ¿DNS SRV funciona? (si usas _srv_ en ldap_uri)
dig _ldap._tcp.lab.internal SRV

# Paso 6: forzar reconexión
sssctl cache-expire -E
systemctl restart sssd
sssctl domain-status lab.internal
```

### Escenario 5: home no se crea en primer login

```bash
# Paso 1: ¿mkhomedir está en PAM?
grep mkhomedir /etc/pam.d/system-auth     # RHEL
grep mkhomedir /etc/pam.d/common-session  # Debian
# Si no aparece → habilitarlo

# Paso 2: ¿oddjobd corre? (RHEL con oddjob_mkhomedir)
systemctl status oddjobd
# Si no → systemctl enable --now oddjobd

# Paso 3: ¿permisos de /home?
ls -ld /home
# drwxr-xr-x 5 root root ...
# Si los permisos son restrictivos, pam_mkhomedir puede fallar

# Paso 4: ¿SELinux bloquea?
ausearch -m AVC -ts recent | grep mkhomedir
# Si hay AVC denials:
setsebool -P use_nfs_home_dirs on    # si home en NFS
# o revisar contexto SELinux
```

---

## 9. Errores comunes

### Error 1: Usar ldapsearch con credenciales de sssd.conf para debug

```bash
# MAL — copiar la contraseña de sssd.conf en la línea de comandos
ldapsearch -x -H ldaps://ldap.example.com \
    -D "cn=readonly,dc=example,dc=com" -w 'S3cur3P4ss' \
    -b "dc=example,dc=com" "(uid=alice)"
# La contraseña queda en: historial del shell, /proc/PID/cmdline, ps aux

# BIEN — pedir interactivamente o usar archivo
ldapsearch -x -H ldaps://ldap.example.com \
    -D "cn=readonly,dc=example,dc=com" -W \
    -b "dc=example,dc=com" "(uid=alice)"
# -W pide la contraseña de forma segura (no aparece en ps)

# MEJOR — archivo de contraseña con permisos restrictivos
echo -n 'S3cur3P4ss' > /root/.ldap-pass
chmod 600 /root/.ldap-pass
ldapsearch -x -H ldaps://ldap.example.com \
    -D "cn=readonly,dc=example,dc=com" -y /root/.ldap-pass \
    -b "dc=example,dc=com" "(uid=alice)"
```

### Error 2: Confundir ldap.conf con sssd.conf para TLS

```bash
# Síntoma: ldapsearch funciona con TLS pero sssd reporta error TLS
# Causa: ldapsearch lee /etc/ldap/ldap.conf
#        sssd lee /etc/sssd/sssd.conf (opciones ldap_tls_*)
#        Son INDEPENDIENTES

# Fix en ldap.conf (para ldapsearch):
# TLS_CACERT /etc/ssl/certs/ca-certificates.crt
# TLS_REQCERT demand

# Fix en sssd.conf (para sssd):
# ldap_tls_cacert = /etc/ssl/certs/ca-certificates.crt
# ldap_tls_reqcert = demand
```

### Error 3: Debug level alto permanente en producción

```ini
# MAL — olvidar debug_level = 9 en producción
[domain/lab.internal]
debug_level = 9
# Resultado: /var/log/sssd/ crece GB por hora, llena disco

# BIEN — usar sssctl para debug temporal
sssctl debug-level 6   # activar
# ... reproducir el problema ...
sssctl debug-level 2   # restaurar

# Si necesitas debug persistente, usar logrotate:
# /etc/logrotate.d/sssd ya existe pero verificar frecuencia
```

### Error 4: No usar -LLL en scripts

```bash
# MAL — parsear salida de ldapsearch con metadatos
result=$(ldapsearch -x -b "dc=example,dc=com" "(uid=alice)" uidNumber)
echo "$result" | grep uidNumber
# Puede incluir líneas de comentarios (#), versión, resultados

# BIEN — salida limpia para scripts
uidnumber=$(ldapsearch -x -LLL -b "dc=example,dc=com" \
    "(uid=alice)" uidNumber | grep "^uidNumber:" | awk '{print $2}')
echo "$uidnumber"
# 1001
```

### Error 5: Reiniciar sssd sin invalidar caché

```bash
# Síntoma: cambié sssd.conf pero el comportamiento no cambia

# MAL — solo reiniciar
systemctl restart sssd
# sssd puede seguir usando datos cacheados

# BIEN — invalidar caché Y reiniciar
sssctl cache-expire -E
systemctl restart sssd

# NUCLEAR — limpiar todo (si la caché parece corrupta)
systemctl stop sssd
rm -rf /var/lib/sss/db/* /var/lib/sss/mc/*
systemctl start sssd
# Advertencia: los usuarios no podrán login offline hasta
# reconstruir la caché con un login online exitoso
```

---

## 10. Cheatsheet

```
=== LDAPSEARCH ===
ldapsearch -x -H URI -D bindDN -W -b baseDN filter attrs
  -x          simple auth
  -H URI      servidor (ldaps:// o ldap://)
  -D DN       bind DN
  -W          pedir contraseña interactivamente
  -w pass     contraseña en línea (inseguro)
  -y file     contraseña desde archivo
  -b DN       base de búsqueda
  -s scope    base | one | sub (default: sub)
  -ZZ         exigir StartTLS
  -LLL        salida LDIF limpia (para scripts)
  -z N        máximo N entradas
  -l N        timeout en segundos
  -Y GSSAPI   autenticación Kerberos

=== OTRAS HERRAMIENTAS LDAP ===
ldapwhoami    verificar bind / conectividad
ldapcompare   comparar valor de atributo (TRUE/FALSE)
ldappasswd    cambiar contraseña LDAP
ldapadd       crear entradas (desde LDIF)
ldapmodify    modificar entradas (desde LDIF)
ldapdelete    eliminar entradas

=== CÓDIGOS DE ERROR LDAP ===
0   Success              49  Invalid credentials
4   Size limit exceeded  50  Insufficient access
10  Referral             32  No such object
34  Invalid DN syntax    65  Object class violation
68  Already exists

=== SSSCTL ===
sssctl config-check               validar sssd.conf
sssctl domain-list                 listar dominios
sssctl domain-status DOMAIN        online/offline, servidor activo
sssctl user-checks USER            diagnóstico completo de usuario
sssctl user-checks USER -s sshd    con servicio PAM específico
sssctl cache-expire -E             invalidar toda la caché
sssctl cache-expire -u USER        invalidar un usuario
sssctl cache-expire -g GROUP       invalidar un grupo
sssctl debug-level N               cambiar verbosidad (0-9)

=== LOGS DE SSSD ===
/var/log/sssd/sssd.log             monitor principal
/var/log/sssd/sssd_nss.log         respondedor NSS
/var/log/sssd/sssd_pam.log         respondedor PAM
/var/log/sssd/sssd_DOMAIN.log      backend del dominio
journalctl -u sssd                 logs vía systemd

=== DIAGNÓSTICO TLS ===
openssl s_client -connect host:636 -showcerts    verificar cert LDAPS
openssl s_client -connect host:389 -starttls ldap   verificar StartTLS
nc -zv host 636                    probar conectividad al puerto
/etc/ldap/ldap.conf                TLS para ldapsearch (Debian)
/etc/openldap/ldap.conf            TLS para ldapsearch (RHEL)
sssd.conf: ldap_tls_*              TLS para sssd (INDEPENDIENTE)

=== ORDEN DE DIAGNÓSTICO ===
1. systemctl status sssd            ¿sssd corre?
2. sssctl domain-status             ¿online/offline?
3. nc -zv host 636                  ¿red OK?
4. openssl s_client                 ¿TLS OK?
5. ldapwhoami                       ¿bind OK?
6. ldapsearch                       ¿datos accesibles?
7. getent passwd user               ¿NSS funciona?
8. sssctl user-checks user          ¿PAM permite?
```

---

## 11. Ejercicios

### Ejercicio 1: Interpretar salida de ldapsearch

Analiza la siguiente sesión de diagnóstico y responde las preguntas:

```bash
$ ldapsearch -x -H ldaps://ldap.empresa.local \
    -D "cn=readonly,dc=empresa,dc=local" -W \
    -b "ou=People,dc=empresa,dc=local" \
    "(&(objectClass=posixAccount)(uidNumber>=1000))" \
    uid uidNumber loginShell

Enter LDAP Password:
# extended LDIF
#
# LDAPv3
# base <ou=People,dc=empresa,dc=local> with scope subtree
# filter: (&(objectClass=posixAccount)(uidNumber>=1000))
# requesting: uid uidNumber loginShell

# elena, People, empresa.local
dn: uid=elena,ou=People,dc=empresa,dc=local
uid: elena
uidNumber: 1001
loginShell: /bin/bash

# marcos, People, empresa.local
dn: uid=marcos,ou=People,dc=empresa,dc=local
uid: marcos
uidNumber: 1042
loginShell: /sbin/nologin

# bot-deploy, ServiceAccounts, People, empresa.local
dn: uid=bot-deploy,ou=ServiceAccounts,ou=People,dc=empresa,dc=local
uid: bot-deploy
uidNumber: 1500
loginShell: /bin/false

# search result
search: 2
result: 0 Success
# numResponses: 4
# numEntries: 3
```

1. ¿Cuántas entradas devolvió la búsqueda? ¿Por qué `numResponses` es diferente de `numEntries`?
2. ¿`marcos` puede hacer login interactivo? ¿Por qué?
3. ¿Por qué `bot-deploy` apareció si está en `ou=ServiceAccounts,ou=People`? ¿Qué scope se usó?
4. Escribe un filtro que excluya las cuentas de servicio (las que tienen shell `/bin/false` o `/sbin/nologin`)
5. Si quisieras que solo aparecieran usuarios directamente en `ou=People` (sin los de sub-OUs), ¿qué cambiarías en el comando?

> **Pregunta de predicción**: Si añades `-z 2` al comando, ¿cuántas entradas verás? ¿El código de resultado seguirá siendo 0 (Success)?

### Ejercicio 2: Diagnóstico paso a paso

Un servidor Linux se unió a LDAP hace una semana y todo funcionaba. Hoy, los usuarios reportan que no pueden hacer SSH. Tienes los siguientes resultados:

```bash
$ systemctl status sssd
● sssd.service — active (running), since 7 days ago

$ sssctl domain-status empresa.local
Online status: Offline
Active server: not connected

$ nc -zv ldap.empresa.local 636
Connection to ldap.empresa.local 636 port [tcp/ldaps] succeeded!

$ openssl s_client -connect ldap.empresa.local:636 </dev/null 2>&1 | grep "Verify"
Verify return code: 10 (certificate has expired)

$ ldapwhoami -x -H ldaps://ldap.empresa.local \
    -D "cn=readonly,dc=empresa,dc=local" -W
ldap_sasl_bind(SIMPLE): Can't contact LDAP server (-1)

$ getent passwd elena
elena:*:1001:1001:Elena Garcia:/home/elena:/bin/bash

$ ssh elena@localhost
elena@localhost's password: ****
Last login: Wed Mar 14 09:30:00 2026
```

1. ¿Cuál es la causa raíz del problema?
2. ¿Por qué sssd está "Offline" si `nc` muestra que el puerto responde?
3. ¿Por qué `getent passwd elena` funciona si sssd está offline?
4. ¿Por qué elena **sí puede** hacer SSH si sssd está offline?
5. ¿Qué usuario NO podrá hacer SSH en esta situación (y por qué)?
6. ¿Cuál es la solución definitiva?

> **Pregunta de predicción**: Si elena cambiara su contraseña en LDAP (desde otra máquina) mientras sssd está offline, ¿podría seguir haciendo login con su contraseña antigua en este servidor? ¿Por cuánto tiempo?

### Ejercicio 3: Construir comandos ldapsearch

Para cada necesidad, escribe el comando `ldapsearch` completo (con todas las opciones necesarias):

1. Verificar conectividad y credenciales sin hacer ninguna búsqueda
2. Buscar todos los grupos POSIX y mostrar solo su nombre y miembros
3. Encontrar todos los usuarios cuyo email termine en `@empresa.local`
4. Listar todas las OUs (unidades organizativas) del directorio
5. Buscar al usuario `elena` y obtener TODOS sus atributos en formato LDIF limpio

Datos de conexión:
- Servidor: `ldaps://ldap.empresa.local`
- Bind DN: `cn=readonly,dc=empresa,dc=local`
- Base DN: `dc=empresa,dc=local`

> **Pregunta de predicción**: Para la búsqueda 3, ¿funcionará el filtro `(mail=*@empresa.local)` tal cual, o necesitas escapar el `@`? ¿Por qué los filtros LDAP no usan las mismas reglas de escape que las expresiones regulares?

---

> **Fin de la Sección 4 — LDAP Client.** Siguiente sección: S05 — Proxy Forward (T01 — Concepto de proxy forward vs reverse: diferencias y casos de uso corporativos).
