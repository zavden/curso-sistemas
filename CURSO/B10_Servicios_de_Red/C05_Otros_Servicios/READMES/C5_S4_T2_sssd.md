# sssd — System Security Services Daemon

## Índice

1. [¿Qué es sssd y por qué existe?](#1-qué-es-sssd-y-por-qué-existe)
2. [Arquitectura de sssd](#2-arquitectura-de-sssd)
3. [Instalación](#3-instalación)
4. [sssd.conf: estructura general](#4-sssdconf-estructura-general)
5. [Dominio LDAP](#5-dominio-ldap)
6. [Dominio Active Directory](#6-dominio-active-directory)
7. [Caché offline](#7-caché-offline)
8. [Gestión del servicio y herramientas](#8-gestión-del-servicio-y-herramientas)
9. [Múltiples dominios](#9-múltiples-dominios)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. ¿Qué es sssd y por qué existe?

**sssd** (*System Security Services Daemon*) es un servicio que actúa como intermediario entre el sistema Linux y fuentes de identidad remotas (LDAP, Active Directory, FreeIPA). Es el reemplazo moderno de las antiguas combinaciones `nslcd` + `pam_ldap` + `nscd`.

### El problema que resuelve

Sin sssd, cada servicio del sistema que necesita consultar usuarios remotos (login, SSH, sudo, cron) haría conexiones independientes al servidor LDAP:

```
Sin sssd (antiguo):
┌───────┐
│ login │──┐
├───────┤  │    Cada proceso abre su propia
│  sshd │──┼──► conexión al servidor LDAP
├───────┤  │    (lento, frágil, sin caché)
│  sudo │──┘
└───────┘

Con sssd (moderno):
┌───────┐
│ login │──┐
├───────┤  │    ┌──────┐     ┌─────────────┐
│  sshd │──┼──► │ sssd │────►│ LDAP / AD / │
├───────┤  │    │      │     │   FreeIPA    │
│  sudo │──┘    │ caché│     └─────────────┘
└───────┘       └──────┘
                   │
                   └── Un solo punto de contacto
                       con caché local
```

### Ventajas de sssd

| Característica | Descripción |
|---|---|
| **Caché offline** | Los usuarios pueden autenticarse incluso si el servidor LDAP no está disponible |
| **Conexión única** | Un solo proceso gestiona todas las conexiones al directorio |
| **Múltiples backends** | LDAP, AD, FreeIPA, Kerberos, archivos locales |
| **Múltiples dominios** | Conectar a varios directorios simultáneamente |
| **Integración NSS+PAM** | Un solo servicio reemplaza `nslcd` + `nscd` + `pam_ldap` |
| **Renovación automática** | Renueva tickets Kerberos y refresca caché en segundo plano |
| **Enumeración controlada** | No necesita descargar todos los usuarios al inicio |

---

## 2. Arquitectura de sssd

sssd es modular: un proceso monitor coordina varios procesos hijo, cada uno con una responsabilidad específica.

```
┌─────────────────────────────────────────────────────────────┐
│                        sssd (monitor)                       │
│                   lee /etc/sssd/sssd.conf                   │
├─────────────┬──────────────┬──────────────┬────────────────┤
│             │              │              │                │
│  Respondedor│  Respondedor │  Respondedor │   Respondedor  │
│     NSS     │     PAM      │    sudo      │     ssh        │
│             │              │              │                │
│ getpwnam()  │ pam_authen-  │ reglas sudo  │ claves SSH     │
│ getgrnam()  │ ticate()     │ en LDAP      │ en LDAP        │
│ getpwuid()  │ pam_acct_    │              │                │
│             │ mgmt()       │              │                │
├─────────────┴──────────────┴──────────────┴────────────────┤
│                                                             │
│                    Backend por dominio                      │
│  ┌──────────────────┐  ┌──────────────────┐                │
│  │ Dominio: corp.com│  │ Dominio: lab.int │                │
│  │ Provider: ad     │  │ Provider: ldap   │                │
│  │                  │  │                  │                │
│  │ ┌──────────────┐ │  │ ┌──────────────┐ │                │
│  │ │  id_provider │ │  │ │  id_provider │ │                │
│  │ │  (identidad) │ │  │ │  (identidad) │ │                │
│  │ ├──────────────┤ │  │ ├──────────────┤ │                │
│  │ │ auth_provider│ │  │ │ auth_provider│ │                │
│  │ │  (autenticac)│ │  │ │  (autenticac)│ │                │
│  │ ├──────────────┤ │  │ ├──────────────┤ │                │
│  │ │access_provid.│ │  │ │access_provid.│ │                │
│  │ │  (autoriza.) │ │  │ │  (autoriza.) │ │                │
│  │ └──────────────┘ │  │ └──────────────┘ │                │
│  └──────────────────┘  └──────────────────┘                │
│                                                             │
│                   Caché LDB (local)                         │
│              /var/lib/sss/db/*.ldb                          │
└─────────────────────────────────────────────────────────────┘
```

### Componentes clave

| Componente | Función |
|---|---|
| **Monitor** (`sssd`) | Proceso padre, lee config, arranca hijos |
| **Respondedor NSS** | Atiende peticiones de `getpwnam()`, `getgrnam()` |
| **Respondedor PAM** | Atiende autenticación y gestión de cuentas |
| **Respondedor sudo** | Proporciona reglas sudo almacenadas en LDAP |
| **Respondedor ssh** | Sirve claves SSH públicas desde LDAP |
| **Backend** | Un proceso por dominio, conecta al directorio |
| **Caché LDB** | Base de datos local con entradas cacheadas |

### Providers (proveedores)

Cada dominio configura proveedores independientes para distintas funciones:

| Provider | Función | Opciones comunes |
|---|---|---|
| `id_provider` | Resolver usuarios y grupos | `ldap`, `ad`, `ipa`, `files` |
| `auth_provider` | Autenticar contraseñas | `ldap`, `ad`, `ipa`, `krb5` |
| `access_provider` | Control de acceso (quién puede login) | `ldap`, `ad`, `ipa`, `simple`, `permit` |
| `chpass_provider` | Cambio de contraseña | `ldap`, `ad`, `ipa`, `krb5` |
| `sudo_provider` | Reglas sudo remotas | `ldap`, `ad`, `ipa`, `none` |

---

## 3. Instalación

### RHEL/Fedora

```bash
# Paquetes base
dnf install sssd sssd-ldap sssd-tools

# Para Active Directory
dnf install sssd-ad realmd oddjob oddjob-mkhomedir adcli

# Para FreeIPA
dnf install sssd-ipa ipa-client
```

### Debian/Ubuntu

```bash
# Paquetes base
apt install sssd sssd-ldap sssd-tools

# Para Active Directory
apt install sssd-ad realmd adcli packagekit

# Para FreeIPA
apt install sssd-ipa freeipa-client
```

### Verificación post-instalación

```bash
# El archivo de configuración debe existir (se crea manualmente o vía realm/ipa-client)
ls -la /etc/sssd/sssd.conf
# Permisos DEBEN ser 0600, propietario root:root
# sssd RECHAZA arrancar si los permisos son incorrectos
```

---

## 4. sssd.conf: estructura general

`/etc/sssd/sssd.conf` tiene formato INI con secciones:

```ini
# /etc/sssd/sssd.conf — estructura general

[sssd]
# Sección principal: qué servicios y dominios activar
services = nss, pam, sudo, ssh
domains = corp.example.com

config_file_version = 2
# ^^^ OBLIGATORIO, siempre valor 2

[nss]
# Configuración del respondedor NSS
filter_groups = root
filter_users = root
# ^^^ No consultar root al directorio remoto

[pam]
# Configuración del respondedor PAM
offline_credentials_expiration = 3
# ^^^ Días que las credenciales offline son válidas

[sudo]
# Configuración del respondedor sudo (normalmente vacío)

[domain/corp.example.com]
# Configuración del dominio (backend)
# Aquí van los providers, URIs, base DNs, etc.
```

### Jerarquía de configuración

```
/etc/sssd/sssd.conf          ← archivo principal
/etc/sssd/conf.d/*.conf      ← drop-ins (sssd 2.x)
                                se fusionan con el principal
```

> **Permiso crítico**: `sssd.conf` debe tener permisos `0600` y pertenecer a `root:root`. Si los permisos son incorrectos, sssd se niega a arrancar con un mensaje de error explícito. Esta es una medida de seguridad porque el archivo puede contener contraseñas de bind.

```bash
chmod 0600 /etc/sssd/sssd.conf
chown root:root /etc/sssd/sssd.conf
```

### Sección [sssd]

```ini
[sssd]
config_file_version = 2          # obligatorio
services = nss, pam              # respondedores a activar
domains = example.com            # dominios a activar (orden = prioridad)
# Si un usuario existe en dos dominios, el primero gana
```

| Servicio | Función |
|---|---|
| `nss` | Resolución de nombres (getpwnam, getgrnam) |
| `pam` | Autenticación |
| `sudo` | Reglas sudo centralizadas |
| `ssh` | Claves SSH desde el directorio |
| `autofs` | Mapas de automount desde LDAP |
| `ifp` | D-Bus InfoPipe (para GDM, cockpit) |

---

## 5. Dominio LDAP

La configuración más fundamental: conectar a un servidor LDAP genérico (OpenLDAP, 389 DS).

### Configuración mínima

```ini
[domain/lab.internal]
# --- Tipo de backend ---
id_provider = ldap
auth_provider = ldap
chpass_provider = ldap

# --- Conexión al servidor ---
ldap_uri = ldaps://ldap.lab.internal
# Alternativas:
# ldap_uri = ldap://ldap1.lab.internal, ldap://ldap2.lab.internal
# (failover automático al segundo si el primero falla)
# ldap_uri = ldap://ldap.lab.internal  (con ldap_id_use_start_tls = True)

# --- Base DN ---
ldap_search_base = dc=lab,dc=internal

# --- Credenciales de bind (para consultas) ---
ldap_default_bind_dn = cn=readonly,dc=lab,dc=internal
ldap_default_authtok = SecretReadOnlyPassword
# ^^^ Por esto sssd.conf DEBE ser 0600

# --- TLS ---
ldap_tls_reqcert = demand
ldap_tls_cacert = /etc/ssl/certs/ca-certificates.crt
# Si usas StartTLS en vez de LDAPS:
# ldap_id_use_start_tls = True
```

### Mapeo de atributos

sssd asume un esquema RFC 2307 por defecto. Si tu servidor usa nombres de atributos diferentes, puedes mapearlos:

```ini
[domain/lab.internal]
# Esquema RFC 2307 (por defecto) vs RFC 2307bis
# La diferencia principal: cómo se expresan los miembros de grupos
ldap_schema = rfc2307
# rfc2307:    memberUid = alice        (nombre plano)
# rfc2307bis: member = uid=alice,ou=People,dc=...  (DN completo)

# Mapeo de atributos de usuario
ldap_user_object_class = posixAccount
ldap_user_name = uid
ldap_user_uid_number = uidNumber
ldap_user_gid_number = gidNumber
ldap_user_home_directory = homeDirectory
ldap_user_shell = loginShell
ldap_user_gecos = cn

# Mapeo de atributos de grupo
ldap_group_object_class = posixGroup
ldap_group_name = cn
ldap_group_gid_number = gidNumber
ldap_group_member = memberUid          # rfc2307
# ldap_group_member = member           # rfc2307bis
```

### Filtros de búsqueda

Puedes restringir qué usuarios y grupos son visibles:

```ini
[domain/lab.internal]
# Solo usuarios con UID >= 1000
ldap_user_search_filter = (&(objectClass=posixAccount)(uidNumber>=1000))

# Solo grupos con GID >= 1000
ldap_group_search_filter = (&(objectClass=posixGroup)(gidNumber>=1000))

# Base DN específica para cada tipo
ldap_user_search_base = ou=People,dc=lab,dc=internal
ldap_group_search_base = ou=Groups,dc=lab,dc=internal
```

### Control de acceso

```ini
[domain/lab.internal]
# Quién puede hacer login en este sistema
access_provider = simple
simple_allow_users = alice, bob
simple_allow_groups = sysadmins, developers

# Alternativa: permitir a todos los usuarios del dominio
# access_provider = permit

# Alternativa: usar LDAP (host attribute o filtro)
# access_provider = ldap
# ldap_access_filter = (memberOf=cn=linux-users,ou=Groups,dc=lab,dc=internal)
```

### Ejemplo completo: dominio LDAP

```ini
[sssd]
config_file_version = 2
services = nss, pam, sudo
domains = lab.internal

[nss]
filter_groups = root
filter_users = root

[pam]
offline_credentials_expiration = 3

[domain/lab.internal]
id_provider = ldap
auth_provider = ldap
chpass_provider = ldap
access_provider = simple

ldap_uri = ldaps://ldap.lab.internal
ldap_search_base = dc=lab,dc=internal
ldap_default_bind_dn = cn=readonly,dc=lab,dc=internal
ldap_default_authtok = S3cur3R34d0nly

ldap_tls_reqcert = demand
ldap_tls_cacert = /etc/pki/tls/certs/ca-bundle.crt

ldap_schema = rfc2307
ldap_user_search_base = ou=People,dc=lab,dc=internal
ldap_group_search_base = ou=Groups,dc=lab,dc=internal

simple_allow_groups = sysadmins

cache_credentials = True
entry_cache_timeout = 300

enumerate = False
# ^^^ IMPORTANTE: no activar en directorios grandes
#     (descarga TODOS los usuarios al inicio)
```

> **`enumerate = False`** (por defecto): sssd no descarga la lista completa de usuarios. `getent passwd` solo muestra usuarios locales + los que ya están en caché. Los usuarios remotos se resuelven bajo demanda cuando alguien intenta login o se consulta por nombre. Con `enumerate = True`, `getent passwd` muestra todos, pero esto puede ser extremadamente lento con miles de usuarios.

---

## 6. Dominio Active Directory

Para unir un sistema Linux a Active Directory, la herramienta más simple es `realm`:

### Join con realm

```bash
# 1. Descubrir el dominio
realm discover corp.example.com
# Muestra: tipo, servidores, paquetes necesarios

# 2. Unirse al dominio (pide contraseña de un admin de AD)
realm join corp.example.com
# Automáticamente:
#   - Crea la cuenta de máquina en AD
#   - Genera /etc/krb5.keytab
#   - Configura /etc/sssd/sssd.conf
#   - Configura /etc/krb5.conf
#   - Habilita PAM y NSS

# 3. Verificar
realm list
id administrator@corp.example.com
```

### sssd.conf generado por realm

```ini
[sssd]
config_file_version = 2
services = nss, pam
domains = corp.example.com

[domain/corp.example.com]
id_provider = ad
access_provider = ad
auth_provider = ad
chpass_provider = ad

ad_domain = corp.example.com
krb5_realm = CORP.EXAMPLE.COM
realmd_tags = manages-system joined-with-adcli
cache_credentials = True
krb5_store_password_if_offline = True

# Formato del nombre de usuario
use_fully_qualified_names = True
# ^^^ Obliga a usar alice@corp.example.com en vez de solo alice
#     Recomendado con AD, evita colisiones con usuarios locales

# Directorio home
fallback_homedir = /home/%u@%d
# %u = username, %d = domain → /home/alice@corp.example.com
# Alternativa más simple:
# fallback_homedir = /home/%u
# override_homedir = /home/%u    (ignora lo que AD diga)

default_shell = /bin/bash

# Mapeo de IDs (AD no tiene uidNumber por defecto)
ldap_id_mapping = True
# ^^^ sssd genera UID/GID automáticamente a partir del SID de AD
#     Rango por defecto: 200000-2000200000
#     Consistente: mismo SID → mismo UID en todas las máquinas
```

### Control de acceso con AD

```bash
# Permitir solo ciertos grupos de AD
realm permit -g 'Linux Admins@corp.example.com'
realm permit -g 'Developers@corp.example.com'

# Permitir usuarios específicos
realm permit alice@corp.example.com

# Permitir a todos (peligroso en AD grandes)
realm permit --all

# Denegar a todos
realm deny --all
```

Esto modifica `sssd.conf`:

```ini
[domain/corp.example.com]
access_provider = simple
simple_allow_groups = Linux Admins@corp.example.com, Developers@corp.example.com
```

### GPO-based access control

Active Directory puede controlar el acceso mediante GPOs:

```ini
[domain/corp.example.com]
ad_gpo_access_control = enforcing
# enforcing:  respetar GPOs (Allow/Deny log on locally)
# permissive: logear pero no bloquear (para testing)
# disabled:   ignorar GPOs
```

---

## 7. Caché offline

Una de las ventajas principales de sssd: los usuarios pueden autenticarse **sin conexión** al servidor.

### Cómo funciona

```
Estado ONLINE:
┌────────┐   auth    ┌──────┐   bind    ┌──────────┐
│ Usuario│──────────►│ sssd │──────────►│ LDAP/AD  │
│        │◄──────────│      │◄──────────│          │
└────────┘   ok/fail │      │  ok/fail  └──────────┘
                     │ caché│
                     │  ↓   │
                     │ guarda hash de la contraseña
                     └──────┘

Estado OFFLINE (servidor caído):
┌────────┐   auth    ┌──────┐    X    ┌──────────┐
│ Usuario│──────────►│ sssd │────X───►│ LDAP/AD  │
│        │◄──────────│      │         │ (caído)  │
└────────┘   ok/fail │      │         └──────────┘
                     │ caché│
                     │  ↑   │
                     │ verifica contra hash almacenado
                     └──────┘
```

### Configuración de caché

```ini
[domain/lab.internal]
cache_credentials = True
# ^^^ Habilita almacenamiento de hashes de contraseña localmente

[pam]
offline_credentials_expiration = 3
# ^^^ Días que las credenciales offline siguen siendo válidas
#     0 = nunca expiran (no recomendado)

pam_id_timeout = 5
# ^^^ Segundos a esperar por el proveedor de identidad antes de
#     declarar offline y usar caché
```

### Comportamiento offline

| Operación | Funciona offline | Notas |
|---|---|---|
| Login con contraseña | Sí (si se ha hecho login online previamente) | Contraseña comparada contra hash local |
| `id usuario` | Sí | Datos de caché |
| `getent passwd usuario` | Sí | Datos de caché |
| Cambio de contraseña | No | Requiere servidor |
| Primer login | No | Necesita al menos un login online primero |
| `getent passwd` (listar todos) | Solo usuarios cacheados | |

### Ubicación de la caché

```bash
ls /var/lib/sss/db/
# cache_DOMAIN.ldb       ← identidades cacheadas
# timestamps_DOMAIN.ldb  ← cuándo se actualizó cada entrada
# ccache_DOMAIN          ← Kerberos credential cache

ls /var/lib/sss/mc/
# passwd   ← memory-mapped cache para NSS (fast path)
# group
# initgroups
```

---

## 8. Gestión del servicio y herramientas

### Gestión del servicio

```bash
# Arrancar/parar
systemctl start sssd
systemctl enable sssd

# Ver estado
systemctl status sssd

# Reiniciar (recarga config completa)
systemctl restart sssd
```

### sssctl: herramienta de diagnóstico principal

```bash
# Estado general
sssctl domain-status lab.internal
# Muestra: Online/Offline, servidor activo, failover

# Listar dominios
sssctl domain-list

# Estado del usuario (qué sabe sssd de este usuario)
sssctl user-checks alice
# Verifica: NSS lookup, PAM auth, grupo primario, shell, home

# Verificar configuración
sssctl config-check
# Valida sssd.conf contra errores de sintaxis
```

### Gestión de caché

```bash
# Invalidar toda la caché (fuerza reconsulta al servidor)
sssctl cache-expire -E

# Invalidar solo un usuario
sssctl cache-expire -u alice

# Invalidar solo grupos
sssctl cache-expire -g developers

# NUCLEAR: borrar caché completamente y reiniciar
systemctl stop sssd
rm -rf /var/lib/sss/db/*
rm -rf /var/lib/sss/mc/*
systemctl start sssd
# ^^^ Útil cuando la caché está corrupta
#     Pero los usuarios no podrán login offline hasta que
#     se reconstruya la caché con un login online
```

> **`sssctl cache-expire -E`** vs borrar archivos: `cache-expire` marca las entradas como expiradas pero las mantiene. sssd las re-consultará al servidor en la próxima petición. Borrar archivos elimina todo incluyendo credenciales offline.

### Consultas de identidad

```bash
# Verificar que sssd resuelve usuarios
getent passwd alice
# alice:*:1001:1001:Alice Johnson:/home/alice:/bin/bash

# Verificar grupos
getent group developers
# developers:*:2001:alice,bob

# Verificar con id
id alice
# uid=1001(alice) gid=1001(alice) groups=1001(alice),2001(developers)

# Para usuarios de AD con fully qualified names
id alice@corp.example.com
getent passwd alice@corp.example.com
```

---

## 9. Múltiples dominios

sssd puede conectarse a varios directorios simultáneamente:

```ini
[sssd]
config_file_version = 2
services = nss, pam
domains = corp.example.com, lab.internal
#         ^^^ orden importa: primera coincidencia gana

[domain/corp.example.com]
id_provider = ad
ad_domain = corp.example.com
# ... configuración AD ...

[domain/lab.internal]
id_provider = ldap
ldap_uri = ldaps://ldap.lab.internal
# ... configuración LDAP ...
```

### Resolución de conflictos

```
¿Qué pasa si "alice" existe en ambos dominios?

domains = corp.example.com, lab.internal

getent passwd alice  →  devuelve alice de corp.example.com (primer dominio)

Para evitar ambigüedad:
  use_fully_qualified_names = True  (en cada dominio)

  getent passwd alice@corp.example.com  →  alice de AD
  getent passwd alice@lab.internal      →  alice de LDAP
```

### Subdominios (AD trusts)

Si Active Directory tiene relaciones de confianza con otros dominios, sssd los descubre automáticamente:

```ini
[domain/corp.example.com]
id_provider = ad
ad_domain = corp.example.com
subdomains_provider = ad
# sssd descubre automáticamente dominios confiados (trusted)
# Los usuarios de dominios trusted se pueden autenticar:
# id bob@partner.com  (si corp.example.com confía en partner.com)
```

---

## 10. Errores comunes

### Error 1: Permisos incorrectos en sssd.conf

```bash
# Síntoma: sssd no arranca
# Log: "SSSD couldn't load the configuration database"

journalctl -u sssd
# "File ownership and permissions check failed. Expected root:root and 0600."

# Causa: sssd.conf tiene permisos incorrectos
ls -la /etc/sssd/sssd.conf
# -rw-r--r-- 1 root root   ← 0644, MAL

# Solución:
chmod 0600 /etc/sssd/sssd.conf
chown root:root /etc/sssd/sssd.conf
systemctl restart sssd
```

sssd es estricto con los permisos porque el archivo puede contener `ldap_default_authtok` (contraseña de bind) en texto plano.

### Error 2: Caché obsoleta tras cambios en el servidor

```bash
# Síntoma: el usuario cambió de grupo en LDAP pero id muestra lo viejo
id alice
# Muestra grupos antiguos

# Causa: sssd cachea entradas por entry_cache_timeout (por defecto 5400s = 90min)

# Solución inmediata:
sssctl cache-expire -u alice
id alice
# Ahora muestra datos frescos

# Para entornos donde los cambios deben reflejarse rápido:
# [domain/lab.internal]
# entry_cache_timeout = 60
```

### Error 3: enumerate = True con directorio grande

```ini
# MAL — directorio con 50.000 usuarios
[domain/corp.example.com]
enumerate = True
# Resultado: sssd tarda minutos en arrancar, consume memoria,
#            getent passwd tarda eternamente

# BIEN — dejar el valor por defecto
[domain/corp.example.com]
enumerate = False
# Los usuarios se resuelven bajo demanda
# getent passwd solo muestra usuarios ya cacheados + locales
```

### Error 4: TLS_REQCERT no configurado

```bash
# Síntoma: sssd no conecta al servidor LDAPS
# Log: "TLS: peer cert untrusted or revoked"

# Causa: sssd no puede verificar el certificado del servidor
# Posiblemente la CA no está instalada o la ruta es incorrecta

# Solución:
# 1. Instalar el certificado de la CA
cp ca-cert.pem /etc/pki/ca-trust/source/anchors/
update-ca-trust extract   # RHEL
# o
cp ca-cert.pem /usr/local/share/ca-certificates/ldap-ca.crt
update-ca-certificates    # Debian

# 2. En sssd.conf, apuntar al bundle correcto
# ldap_tls_cacert = /etc/pki/tls/certs/ca-bundle.crt        # RHEL
# ldap_tls_cacert = /etc/ssl/certs/ca-certificates.crt       # Debian

# NUNCA como solución permanente:
# ldap_tls_reqcert = never   ← desactiva verificación, INSEGURO
```

### Error 5: Olvidar config_file_version = 2

```ini
# MAL — falta config_file_version
[sssd]
services = nss, pam
domains = lab.internal
# Error: sssd se niega a arrancar con error críptico

# BIEN — siempre incluir
[sssd]
config_file_version = 2
services = nss, pam
domains = lab.internal
```

---

## 11. Cheatsheet

```
=== INSTALACIÓN ===
dnf install sssd sssd-ldap sssd-tools         RHEL: LDAP genérico
dnf install sssd-ad realmd adcli              RHEL: Active Directory
apt install sssd sssd-ldap sssd-tools         Debian: LDAP genérico
chmod 0600 /etc/sssd/sssd.conf                permisos obligatorios

=== sssd.conf ESTRUCTURA ===
[sssd]                   sección principal (services, domains)
config_file_version = 2  OBLIGATORIO
[nss]                    respondedor de nombres
[pam]                    respondedor de autenticación
[domain/NOMBRE]          backend por dominio

=== PROVIDERS (por dominio) ===
id_provider        identidad (quién es)        ldap, ad, ipa, files
auth_provider      autenticación (verificar)   ldap, ad, ipa, krb5
access_provider    acceso (puede entrar)        simple, ad, permit, ldap
chpass_provider    cambio de contraseña         ldap, ad, ipa, krb5

=== DOMINIO LDAP (opciones clave) ===
ldap_uri                     servidor(es) LDAP
ldap_search_base             base DN
ldap_default_bind_dn         DN para consultas
ldap_default_authtok         contraseña de bind
ldap_tls_reqcert             verificación TLS (demand)
ldap_schema                  rfc2307 | rfc2307bis
enumerate = False            NO listar todos al inicio (default)
cache_credentials = True     habilitar caché offline

=== ACTIVE DIRECTORY ===
realm discover DOMINIO       descubrir dominio
realm join DOMINIO           unirse (crea sssd.conf)
realm permit -g 'GRUPO'     permitir grupo AD
realm deny --all             denegar a todos
use_fully_qualified_names    True = user@domain
ldap_id_mapping = True       SID → UID automático

=== HERRAMIENTAS ===
sssctl config-check              validar sssd.conf
sssctl domain-status DOMINIO     online/offline, servidor
sssctl domain-list               listar dominios
sssctl user-checks USUARIO       diagnóstico completo
sssctl cache-expire -E           invalidar toda la caché
sssctl cache-expire -u USUARIO   invalidar un usuario

=== CACHÉ ===
/var/lib/sss/db/              bases de datos LDB
/var/lib/sss/mc/              memory-mapped cache (fast)
sssctl cache-expire -E        marcar expirado (soft)
rm -rf /var/lib/sss/db/*      borrar todo (hard, pierde offline)

=== VERIFICACIÓN ===
getent passwd usuario         resolver usuario
getent group grupo            resolver grupo
id usuario                    UID, GID, grupos
```

---

## 12. Ejercicios

### Ejercicio 1: Configurar un dominio LDAP

Escribe un archivo `sssd.conf` completo para un entorno con estas características:

- Servidor LDAP: `ldaps://ldap.empresa.local`
- Base DN: `dc=empresa,dc=local`
- Bind DN: `cn=sssd-bind,ou=Service,dc=empresa,dc=local`
- Contraseña de bind: `Bind4SSSD!`
- Usuarios en `ou=Empleados,dc=empresa,dc=local`
- Grupos en `ou=Departamentos,dc=empresa,dc=local`
- Solo el grupo `sysadmins` puede hacer login
- Caché offline habilitada
- Esquema RFC 2307bis (miembros como DNs)

Incluye la sección `[sssd]`, `[nss]`, `[pam]` y `[domain/...]`.

> **Pregunta de predicción**: Si un usuario del grupo `developers` intenta hacer SSH a este servidor, ¿qué ocurrirá exactamente? ¿Podrá resolver su nombre con `id`? ¿Podrá autenticarse? ¿En qué paso falla?

### Ejercicio 2: Diagnosticar caché

Estás en la siguiente situación:

1. El usuario `marcos` fue movido del grupo `developers` al grupo `ops` en el servidor LDAP hace 10 minutos
2. En el servidor Linux, `id marcos` sigue mostrando `developers` y no muestra `ops`
3. `sssd.conf` tiene `entry_cache_timeout = 5400` (90 minutos)

Responde:

1. ¿Por qué `id` muestra información antigua?
2. Escribe el comando exacto para forzar la actualización de `marcos` sin afectar a otros usuarios
3. ¿Qué valor de `entry_cache_timeout` pondrías si los cambios necesitan reflejarse en menos de 5 minutos? ¿Qué impacto tiene en el servidor LDAP?
4. Si el servidor LDAP estuviera caído, ¿qué mostraría `id marcos` después de invalidar la caché?

> **Pregunta de predicción**: Después de ejecutar `sssctl cache-expire -u marcos`, ¿se actualizan los datos inmediatamente? ¿O necesitas hacer algo más para que `id marcos` muestre la información nueva?

### Ejercicio 3: Comparar configuraciones LDAP vs AD

Compara estos dos escenarios lado a lado:

**Escenario A**: Servidor OpenLDAP con usuarios POSIX (uidNumber, gidNumber en cada entrada)
**Escenario B**: Active Directory estándar (sin extensiones POSIX, usuarios solo con SID)

Para cada escenario, responde:

1. ¿Qué valor tiene `id_provider`?
2. ¿Necesitas `ldap_id_mapping`? ¿Qué hace exactamente?
3. ¿Cómo se determina el UID numérico de un usuario?
4. Si migras 100 servidores Linux del escenario A al B, ¿los archivos en `/home` seguirán teniendo el propietario correcto? ¿Por qué?
5. ¿Qué pasa si dos servidores Linux en el escenario B tienen diferentes `ldap_id_mapping` ranges?

> **Pregunta de predicción**: En el escenario B con `ldap_id_mapping = True`, si creas un usuario nuevo en AD, ¿necesitas asignarle manualmente un uidNumber? ¿Qué pasa si después instalas las extensiones POSIX y le asignas un uidNumber diferente al que sssd había generado?

---

> **Siguiente tema**: T03 — Integración con NSS y PAM (nsswitch.conf, pam_sss, autenticación centralizada).
