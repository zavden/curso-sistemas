# Conceptos de LDAP

## Índice

1. [¿Qué es un directorio?](#1-qué-es-un-directorio)
2. [LDAP como protocolo](#2-ldap-como-protocolo)
3. [El árbol DIT y los Distinguished Names](#3-el-árbol-dit-y-los-distinguished-names)
4. [Entradas, atributos y objectClass](#4-entradas-atributos-y-objectclass)
5. [Esquemas](#5-esquemas)
6. [Formato LDIF](#6-formato-ldif)
7. [Operaciones LDAP](#7-operaciones-ldap)
8. [Seguridad: LDAPS y StartTLS](#8-seguridad-ldaps-y-starttls)
9. [Implementaciones comunes](#9-implementaciones-comunes)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. ¿Qué es un directorio?

Un directorio es una base de datos especializada **optimizada para lectura**. A diferencia de una base de datos relacional (MySQL, PostgreSQL), un directorio:

| Característica | Base de datos relacional | Directorio LDAP |
|---|---|---|
| Optimizado para | Lectura y escritura equilibrada | Lectura masiva |
| Esquema | Tablas con filas y columnas | Árbol jerárquico con entradas |
| Transacciones | ACID completo | Operaciones atómicas simples |
| Relaciones | JOINs complejos | Jerarquía padre-hijo |
| Caso de uso típico | Datos transaccionales | Identidades, configuraciones |
| Ratio lectura/escritura típico | 50/50 a 80/20 | 95/5 o mayor |

Piensa en una guía telefónica: se consulta constantemente, rara vez se modifica, y está organizada jerárquicamente (país → ciudad → persona). LDAP funciona exactamente así.

### Casos de uso reales

- **Autenticación centralizada**: Un usuario, una contraseña para SSH, correo, VPN, aplicaciones web
- **Directorio corporativo**: Nombres, emails, teléfonos, departamentos
- **Políticas de grupo**: Configuraciones de máquinas y usuarios (Active Directory)
- **Certificados PKI**: Distribución de certificados X.509
- **DNS dinámico**: Almacenar zonas DNS en LDAP

---

## 2. LDAP como protocolo

LDAP (*Lightweight Directory Access Protocol*) es un **protocolo**, no un producto. Esto es una distinción crucial:

```
┌──────────────────────────────────────────────────────────┐
│                    Protocolo LDAP                        │
│              (RFC 4510-4519, puerto 389)                 │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  Implementaciones del servidor:                          │
│  ┌──────────┐  ┌──────────────┐  ┌───────────────────┐  │
│  │ OpenLDAP │  │ 389 Directory│  │ Active Directory  │  │
│  │ (slapd)  │  │   Server     │  │ (Microsoft)       │  │
│  └──────────┘  └──────────────┘  └───────────────────┘  │
│                                                          │
│  Implementaciones del cliente:                           │
│  ┌──────────┐  ┌──────────────┐  ┌───────────────────┐  │
│  │ldapsearch│  │    sssd      │  │  Aplicaciones     │  │
│  │ldapadd   │  │              │  │  (PHP, Python...) │  │
│  └──────────┘  └──────────────┘  └───────────────────┘  │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### Historia breve

```
X.500 (1988) ── protocolo DAP ── pesado, OSI completo
      │
      └── LDAP (1993) ── "Lightweight" ── TCP/IP directo
              │
              ├── LDAPv2 (RFC 1777) ── obsoleto
              │
              └── LDAPv3 (RFC 4510, 2006) ── estándar actual
                    ├── UTF-8
                    ├── TLS/SASL
                    ├── Extensiones (controls, extended operations)
                    └── Referrals
```

### Puertos

| Puerto | Protocolo | Descripción |
|---|---|---|
| 389 | LDAP | Texto plano o StartTLS |
| 636 | LDAPS | TLS implícito (como HTTPS) |
| 3268 | GC | Global Catalog (Active Directory) |
| 3269 | GC+TLS | Global Catalog con TLS |

---

## 3. El árbol DIT y los Distinguished Names

El corazón de LDAP es el **DIT** (*Directory Information Tree*): una estructura jerárquica en forma de árbol invertido.

### Anatomía de un DIT

```
                        dc=example,dc=com           ← Base DN (raíz)
                       /          |          \
              ou=People      ou=Groups      ou=Services
              /    |    \        |    \          |
     uid=alice  uid=bob  uid=carol  cn=devs  cn=ldap
                                    cn=ops   cn=mail
```

### Componentes del DN

Cada nodo tiene un **DN** (*Distinguished Name*) que es su ruta completa desde la raíz, leyendo de abajo hacia arriba (como una ruta de archivo invertida):

```
uid=alice,ou=People,dc=example,dc=com
│         │          │
│         │          └── Componente del dominio (domain component)
│         └── Unidad organizativa (organizational unit)
└── Identificador de usuario (user ID)
```

| Abreviatura | Significado | Uso típico |
|---|---|---|
| `dc` | Domain Component | Raíz del árbol (del dominio DNS) |
| `ou` | Organizational Unit | Contenedor/carpeta |
| `cn` | Common Name | Nombre de grupo, servicio |
| `uid` | User ID | Nombre de usuario |
| `o` | Organization | Organización (alternativa a dc) |
| `c` | Country | País (poco usado hoy) |

### Terminología clave

```
DN completo:  uid=alice,ou=People,dc=example,dc=com
              ├─────┘
              │
              RDN (Relative Distinguished Name) ← solo el componente local

Base DN:      dc=example,dc=com  ← punto de inicio de búsquedas

Parent DN:    ou=People,dc=example,dc=com  ← el padre de alice
```

> **Regla fundamental**: Cada DN es **único** en todo el directorio. No puede haber dos `uid=alice,ou=People,dc=example,dc=com`. El RDN solo necesita ser único entre hermanos (puede haber `uid=alice` en `ou=People` y otro `uid=alice` en `ou=Contractors`).

### Del dominio DNS al Base DN

La convención estándar convierte el dominio DNS en base DN:

```
DNS:      example.com        →  dc=example,dc=com
DNS:      corp.example.com   →  dc=corp,dc=example,dc=com
DNS:      lab.internal       →  dc=lab,dc=internal
```

---

## 4. Entradas, atributos y objectClass

### Estructura de una entrada

Cada nodo del DIT es una **entrada**. Una entrada es un conjunto de **atributos** con sus valores:

```
┌─────────────────────────────────────────────────┐
│ DN: uid=alice,ou=People,dc=example,dc=com       │
├─────────────────────────────────────────────────┤
│ objectClass: inetOrgPerson    ← define qué      │
│ objectClass: posixAccount        atributos puede │
│ objectClass: shadowAccount       tener           │
│ uid: alice                    ← RDN              │
│ cn: Alice Johnson             ← nombre completo  │
│ sn: Johnson                   ← apellido (req.)  │
│ givenName: Alice              ← nombre de pila   │
│ mail: alice@example.com       ← email            │
│ uidNumber: 1001               ← UID Unix         │
│ gidNumber: 1001               ← GID Unix         │
│ homeDirectory: /home/alice    ← home Unix        │
│ loginShell: /bin/bash         ← shell Unix       │
│ userPassword: {SSHA}xxxxx     ← contraseña hash  │
└─────────────────────────────────────────────────┘
```

### Tipos de atributos

Los atributos tienen reglas definidas en el esquema:

| Propiedad | Descripción | Ejemplo |
|---|---|---|
| **Nombre/OID** | Identificador único | `cn` / `2.5.4.3` |
| **Sintaxis** | Tipo de dato | DirectoryString, Integer, Boolean |
| **Single-value** | Solo un valor permitido | `uidNumber` (un solo UID) |
| **Multi-value** | Múltiples valores | `mail` (varios emails) |
| **Matching rules** | Cómo se compara | caseIgnoreMatch, integerMatch |

### objectClass: el esquema de la entrada

Los `objectClass` determinan qué atributos **debe** (MUST) y **puede** (MAY) tener una entrada:

```
objectClass: inetOrgPerson
├── MUST: sn, cn                    ← obligatorios
└── MAY:  mail, telephoneNumber,    ← opcionales
          title, description, ...

objectClass: posixAccount
├── MUST: cn, uid, uidNumber, gidNumber, homeDirectory
└── MAY:  loginShell, gecos, description, userPassword
```

### Tipos de objectClass

```
STRUCTURAL ── define la naturaleza de la entrada
│             (exactamente uno por entrada)
│             Ejemplos: inetOrgPerson, organization, organizationalUnit
│
AUXILIARY ──── añade atributos extra a la entrada
│             (cero o más por entrada)
│             Ejemplos: posixAccount, shadowAccount, posixGroup
│
ABSTRACT ──── clase base (no se instancia directamente)
              Ejemplo: top (madre de todos los objectClass)
```

> **Analogía con programación**: STRUCTURAL es como la clase concreta que instancias, AUXILIARY son los traits/interfaces que le añades, y ABSTRACT es la clase base abstracta.

### Herencia de objectClass

```
top (ABSTRACT)
└── person (STRUCTURAL)
    ├── MUST: sn, cn
    └── organizationalPerson (STRUCTURAL)
        ├── MAY: title, ou, telephoneNumber
        └── inetOrgPerson (STRUCTURAL)
            └── MAY: mail, uid, photo, ...
```

Una entrada con `objectClass: inetOrgPerson` hereda automáticamente los atributos de `person` y `organizationalPerson`.

---

## 5. Esquemas

Un esquema define los tipos de atributos y objectClasses disponibles. Los esquemas estándar cubren la mayoría de necesidades:

| Esquema | Contenido | Uso |
|---|---|---|
| `core.schema` | `top`, `person`, `organizationalUnit` | Fundamental, siempre cargado |
| `cosine.schema` | `pilotPerson`, `domainRelatedObject` | Compatibilidad X.500 |
| `inetorgperson.schema` | `inetOrgPerson` | Entradas de personas |
| `nis.schema` | `posixAccount`, `posixGroup`, `shadowAccount` | Integración Unix/Linux |
| `misc.schema` | `inetLocalMailRecipient` | Correo |
| `samba.schema` | `sambaSamAccount` | Integración Samba/Windows |

### Definición de un attributeType (referencia)

```ldif
attributetype ( 2.5.4.42
    NAME 'givenName'
    DESC 'RFC2256: first name(s) for which the entity is known by'
    SUP name
    EQUALITY caseIgnoreMatch
    SUBSTR caseIgnoreSubstringsMatch
    SYNTAX 1.3.6.1.4.1.1466.115.121.1.15 )
```

Cada atributo tiene un **OID** (*Object Identifier*) globalmente único, una sintaxis que define el tipo de dato, y reglas de comparación.

### Definición de un objectClass (referencia)

```ldif
objectclass ( 2.16.840.1.113730.3.2.2
    NAME 'inetOrgPerson'
    DESC 'RFC2798: Internet Organizational Person'
    SUP organizationalPerson
    STRUCTURAL
    MAY ( audio $ businessCategory $ carLicense $ departmentNumber $
          displayName $ employeeNumber $ employeeType $ givenName $
          homePhone $ homePostalAddress $ initials $ jpegPhoto $
          labeledURI $ mail $ manager $ mobile $ o $ pager $ photo $
          roomNumber $ secretary $ uid $ userCertificate $
          x500uniqueIdentifier $ preferredLanguage $
          userSMIMECertificate $ userPKCS12 ) )
```

No necesitas memorizar estas definiciones. Lo importante es entender que **el esquema es el contrato**: si intentas añadir un atributo que no está en el esquema, el servidor lo rechaza.

---

## 6. Formato LDIF

**LDIF** (*LDAP Data Interchange Format*, RFC 2849) es el formato de texto plano para representar entradas LDAP y modificaciones. Es lo que usas para importar, exportar y modificar datos.

### LDIF para definir entradas

```ldif
# Crear la raíz del directorio
dn: dc=example,dc=com
objectClass: top
objectClass: dcObject
objectClass: organization
dc: example
o: Example Corporation

# Crear unidad organizativa
dn: ou=People,dc=example,dc=com
objectClass: top
objectClass: organizationalUnit
ou: People

# Crear un usuario
dn: uid=alice,ou=People,dc=example,dc=com
objectClass: top
objectClass: inetOrgPerson
objectClass: posixAccount
objectClass: shadowAccount
uid: alice
cn: Alice Johnson
sn: Johnson
givenName: Alice
mail: alice@example.com
uidNumber: 1001
gidNumber: 1001
homeDirectory: /home/alice
loginShell: /bin/bash
userPassword: {SSHA}W6ph5Mm5Pz8GgiULbPgzG37mj9g=

# Crear un grupo POSIX
dn: cn=developers,ou=Groups,dc=example,dc=com
objectClass: top
objectClass: posixGroup
cn: developers
gidNumber: 2001
memberUid: alice
memberUid: bob
```

### Reglas de sintaxis LDIF

```
1. Líneas en blanco separan entradas
2. Comentarios empiezan con #
3. Atributo: valor (espacio después de los dos puntos)
4. Valores base64: atributo:: dG9kbw==  (doble dos puntos)
5. Líneas largas se continúan con un espacio al inicio de la siguiente:
   description: Esta es una descripcion muy larga que necesita
    continuarse en la siguiente linea con un espacio al inicio
6. La primera línea siempre es dn: ...
```

### LDIF para modificaciones

Las modificaciones usan un formato especial con `changetype`:

```ldif
# Añadir un atributo
dn: uid=alice,ou=People,dc=example,dc=com
changetype: modify
add: telephoneNumber
telephoneNumber: +34 91 555 1234
-

# Reemplazar un atributo
dn: uid=alice,ou=People,dc=example,dc=com
changetype: modify
replace: loginShell
loginShell: /bin/zsh
-

# Eliminar un atributo específico
dn: uid=alice,ou=People,dc=example,dc=com
changetype: modify
delete: mail
mail: alice-old@example.com
-

# Eliminar TODOS los valores de un atributo
dn: uid=alice,ou=People,dc=example,dc=com
changetype: modify
delete: telephoneNumber
-

# Múltiples operaciones en una entrada
dn: uid=alice,ou=People,dc=example,dc=com
changetype: modify
replace: loginShell
loginShell: /bin/zsh
-
add: mail
mail: alice-new@example.com
-
delete: description
-
```

> **El guion `-` es crítico**: Separa operaciones dentro de un mismo `changetype: modify`. Olvidarlo provoca errores de sintaxis difíciles de diagnosticar.

```ldif
# Eliminar una entrada completa
dn: uid=alice,ou=People,dc=example,dc=com
changetype: delete

# Renombrar (mover) una entrada
dn: uid=alice,ou=People,dc=example,dc=com
changetype: modrdn
newrdn: uid=ajohnson
deleteoldrdn: 1
```

---

## 7. Operaciones LDAP

LDAP define un conjunto de operaciones que un cliente puede solicitar al servidor. Cada operación se mapea a un comando de las herramientas de línea de comandos.

### Bind (autenticación)

Bind establece la identidad del cliente ante el servidor. Sin bind, la conexión es **anónima** (si el servidor lo permite).

```
┌──────────┐    Bind Request (DN + password)     ┌──────────┐
│  Cliente  │ ─────────────────────────────────► │ Servidor │
│           │ ◄───────────────────────────────── │  LDAP    │
└──────────┘    Bind Response (success/error)    └──────────┘
```

Tipos de bind:

| Tipo | Descripción | Uso |
|---|---|---|
| **Simple bind** | DN + contraseña en texto plano | Solo sobre TLS |
| **SASL bind** | Mecanismos como GSSAPI (Kerberos), EXTERNAL (TLS cert) | Entornos corporativos |
| **Anonymous bind** | Sin credenciales | Consultas públicas (si está permitido) |

```bash
# Simple bind con ldapsearch
ldapsearch -x -H ldap://ldap.example.com \
    -D "cn=admin,dc=example,dc=com" \
    -W \
    -b "dc=example,dc=com" "(uid=alice)"
#    │   │                    │   │          │
#    │   │                    │   │          └── Filtro de búsqueda
#    │   │                    │   └── Pide contraseña interactivamente
#    │   │                    └── Bind DN (identidad)
#    │   └── URI del servidor
#    └── Simple authentication (no SASL)
```

### Search (búsqueda)

La operación más frecuente. Tiene cuatro componentes:

```
┌─────────────────────────────────────────────────────────┐
│                    LDAP Search                          │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  1. Base DN ──── Dónde empezar a buscar                │
│     dc=example,dc=com                                   │
│                                                         │
│  2. Scope ───── Cuánto profundizar                     │
│     ├── base ──── solo la entrada del Base DN          │
│     ├── one ───── hijos directos (un nivel)            │
│     └── sub ───── todo el subárbol (recursivo)         │
│                                                         │
│  3. Filter ──── Qué buscar                             │
│     (uid=alice)                                         │
│                                                         │
│  4. Attributes ─ Qué campos devolver                   │
│     cn mail uidNumber                                   │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

**Scope visual**:

```
           dc=example,dc=com          ← base: solo este nodo
           /         \
      ou=People    ou=Groups          ← one: estos dos
      /    \          |
  uid=alice uid=bob  cn=devs          ← sub: todo (incluyendo raíz)
```

### Filtros de búsqueda

Los filtros usan una sintaxis con notación prefija (polaca):

```
Filtro simple:
    (uid=alice)                 ← igualdad exacta
    (cn=Ali*)                   ← wildcard (substring)
    (uidNumber>=1000)           ← mayor o igual
    (uidNumber<=2000)           ← menor o igual
    (description=*)             ← tiene el atributo (presencia)
    (!(loginShell=/sbin/nologin)) ← negación

Filtros compuestos (notación prefija):
    (&(objectClass=posixAccount)(uid=alice))           ← AND
    (|(uid=alice)(uid=bob))                             ← OR
    (&(objectClass=posixAccount)(!(loginShell=/sbin/nologin)))  ← AND + NOT

Filtro complejo combinado:
    (&                                  ← AND de todo
        (objectClass=posixAccount)      ← es cuenta POSIX
        (uidNumber>=1000)               ← UID >= 1000
        (|(ou=Engineering)(ou=DevOps))  ← en Engineering O DevOps
        (!(loginShell=/sbin/nologin))   ← con shell activa
    )
```

> **Cuidado**: Los paréntesis son obligatorios alrededor de **cada** componente. `&(a)(b)` es inválido; debe ser `(&(a)(b))`.

### Add (añadir)

Crea una nueva entrada en el directorio:

```bash
# Desde LDIF
ldapadd -x -H ldap://ldap.example.com \
    -D "cn=admin,dc=example,dc=com" -W \
    -f new_user.ldif

# Desde stdin
ldapadd -x -D "cn=admin,dc=example,dc=com" -W <<EOF
dn: uid=carol,ou=People,dc=example,dc=com
objectClass: inetOrgPerson
objectClass: posixAccount
uid: carol
cn: Carol Smith
sn: Smith
uidNumber: 1003
gidNumber: 1003
homeDirectory: /home/carol
EOF
```

El servidor valida que:
1. El parent DN existe (`ou=People,dc=example,dc=com`)
2. El DN no existe ya
3. Los atributos obligatorios (MUST) están presentes
4. Los atributos cumplen el esquema

### Modify (modificar)

Modifica atributos de una entrada existente:

```bash
# Desde LDIF
ldapmodify -x -D "cn=admin,dc=example,dc=com" -W -f changes.ldif

# Desde stdin
ldapmodify -x -D "cn=admin,dc=example,dc=com" -W <<EOF
dn: uid=alice,ou=People,dc=example,dc=com
changetype: modify
replace: loginShell
loginShell: /bin/zsh
-
add: mail
mail: alice-work@example.com
-
EOF
```

Tres sub-operaciones: `add` (añadir valor), `replace` (reemplazar), `delete` (eliminar).

### Delete (eliminar)

```bash
ldapdelete -x -D "cn=admin,dc=example,dc=com" -W \
    "uid=alice,ou=People,dc=example,dc=com"
```

> **Restricción**: No puedes eliminar una entrada que tiene hijos. Debes eliminar recursivamente de hojas hacia la raíz (o usar el control de eliminación de subárbol si el servidor lo soporta).

### Compare (comparar)

Verifica si un atributo tiene un valor específico sin leer la entrada completa. Útil para verificar contraseñas o membresía:

```bash
ldapcompare -x -D "cn=admin,dc=example,dc=com" -W \
    "uid=alice,ou=People,dc=example,dc=com" \
    "loginShell:/bin/bash"
# TRUE o FALSE (no devuelve la entrada)
```

### Resumen de operaciones y comandos

| Operación | Comando | LDIF changetype | Descripción |
|---|---|---|---|
| Bind | `-D dn -W` | — | Autenticarse |
| Search | `ldapsearch` | — | Buscar entradas |
| Add | `ldapadd` | `add` | Crear entrada |
| Modify | `ldapmodify` | `modify` | Modificar atributos |
| Delete | `ldapdelete` | `delete` | Eliminar entrada |
| ModRDN | `ldapmodrdn` | `modrdn` | Renombrar/mover |
| Compare | `ldapcompare` | — | Comparar valor |

---

## 8. Seguridad: LDAPS y StartTLS

Las contraseñas en un simple bind viajan en **texto plano** si no hay cifrado. Proteger la conexión LDAP es obligatorio en producción.

### Dos mecanismos de cifrado

```
LDAPS (puerto 636):
┌────────┐         ┌─────────────────────┐
│ Cliente │ ──TLS──►│ :636 (TLS inmediato)│
└────────┘         └─────────────────────┘
Toda la conexión está cifrada desde el inicio.
Análogo a HTTPS (puerto 443).

StartTLS (puerto 389):
┌────────┐         ┌─────────────────────┐
│ Cliente │ ──TCP──►│ :389 (texto plano)  │
│         │ ──STARTTLS──►│               │
│         │ ◄──TLS negociado──│          │
│         │ ──operaciones cifradas──►    │
└────────┘         └─────────────────────┘
Empieza en texto plano, luego "upgrade" a TLS.
Análogo a STARTTLS en SMTP (puerto 587).
```

| Aspecto | LDAPS | StartTLS |
|---|---|---|
| Puerto | 636 | 389 |
| Estándar | Originalmente no estándar, ahora aceptado | RFC 4511 |
| Cifrado | Desde el primer byte | Después del upgrade |
| Firewalls | Puerto separado, fácil de filtrar | Mismo puerto que LDAP plano |
| Estado actual | Ampliamente usado, recomendado | También válido |

```bash
# Conexión con StartTLS
ldapsearch -x -H ldap://ldap.example.com -ZZ \
    -D "cn=admin,dc=example,dc=com" -W -b "dc=example,dc=com"
#                                    -ZZ = exigir StartTLS (falla si no puede)
#                                    -Z  = intentar StartTLS (continúa sin TLS)

# Conexión con LDAPS
ldapsearch -x -H ldaps://ldap.example.com \
    -D "cn=admin,dc=example,dc=com" -W -b "dc=example,dc=com"
```

### Configuración del cliente TLS

El archivo `/etc/openldap/ldap.conf` (RHEL) o `/etc/ldap/ldap.conf` (Debian) configura la verificación de certificados:

```bash
# /etc/ldap/ldap.conf  (o /etc/openldap/ldap.conf)
URI     ldap://ldap.example.com
BASE    dc=example,dc=com
TLS_CACERT      /etc/ssl/certs/ca-certificates.crt
TLS_REQCERT     demand    # demand=exigir cert válido (producción)
                          # allow=aceptar cualquiera (testing)
                          # never=no verificar (INSEGURO)
```

> **`TLS_REQCERT never`** desactiva toda verificación de certificados. Usarlo en producción es equivalente a no tener TLS — un atacante puede hacer MITM sin ser detectado. Solo para pruebas.

### Hashing de contraseñas

Las contraseñas en LDAP se almacenan con un prefijo que indica el algoritmo:

```
{SSHA}W6ph5...    ← Salted SHA-1 (legacy, aún común)
{CRYPT}$6$...     ← SHA-512 crypt (como /etc/shadow)
{PBKDF2-SHA256}   ← PBKDF2 (más seguro, OpenLDAP 2.5+)
{ARGON2}          ← Argon2 (el más seguro, soporte reciente)
{CLEARTEXT}       ← NUNCA en producción
```

---

## 9. Implementaciones comunes

### Servidores LDAP

| Implementación | Proyecto | Notas |
|---|---|---|
| **OpenLDAP** (slapd) | Open source | Estándar en Linux, potente pero config compleja |
| **389 Directory Server** | Fedora/Red Hat | Interfaz web, réplica multi-master |
| **Active Directory** | Microsoft | LDAP + Kerberos + DNS + GPO, dominante en empresas |
| **FreeIPA** | Red Hat | Combina 389 DS + Kerberos + DNS + CA, simplifica AD-like para Linux |
| **LLDAP** | Open source (Rust) | Ligero, solo autenticación, interfaz web moderna |
| **GLAuth** | Open source (Go) | Ligero, backends YAML/SQL/LDAP |

### Perspectiva del sysadmin

En la práctica, como administrador de sistemas Linux:

1. **Rara vez instalas un servidor LDAP desde cero** — ya existe uno (Active Directory en empresas, FreeIPA en entornos Linux puros)
2. **Frecuentemente configuras clientes** — sssd, PAM, NSS para autenticar contra el directorio existente
3. **Necesitas entender los conceptos** — para diagnosticar problemas de autenticación, ajustar filtros, leer logs

Por eso este bloque se enfoca en **LDAP Client**, no en administración del servidor.

---

## 10. Errores comunes

### Error 1: Confundir base DN con DN de una entrada

```bash
# MAL — buscar con base DN del usuario (solo devuelve ESA entrada)
ldapsearch -b "uid=alice,ou=People,dc=example,dc=com" "(objectClass=*)"
# Solo devuelve alice

# BIEN — buscar desde la base correcta con filtro
ldapsearch -b "ou=People,dc=example,dc=com" "(uid=alice)"
# Busca alice dentro de ou=People
```

El base DN es el **punto de inicio**, no el objetivo.

### Error 2: Olvidar los paréntesis en filtros compuestos

```bash
# MAL — sin paréntesis exteriores en AND
ldapsearch -b "dc=example,dc=com" "&(uid=alice)(objectClass=posixAccount)"
# Error de sintaxis

# BIEN — AND con paréntesis completos
ldapsearch -b "dc=example,dc=com" "(&(uid=alice)(objectClass=posixAccount))"
```

### Error 3: Usar LDAP sin cifrado en producción

```bash
# MAL — simple bind sin TLS envía contraseña en texto plano
ldapsearch -x -H ldap://ldap.example.com -D "cn=admin,dc=example,dc=com" -w secret

# BIEN — usar LDAPS o StartTLS
ldapsearch -x -H ldaps://ldap.example.com -D "cn=admin,dc=example,dc=com" -W
ldapsearch -x -H ldap://ldap.example.com -ZZ -D "cn=admin,dc=example,dc=com" -W
# Además, -W pide la contraseña interactivamente en vez de -w en línea de comandos
```

### Error 4: No entender la herencia de objectClass

```ldif
# MAL — usar inetOrgPerson sin el atributo sn (obligatorio heredado de person)
dn: uid=test,ou=People,dc=example,dc=com
objectClass: inetOrgPerson
objectClass: posixAccount
uid: test
cn: Test User
uidNumber: 9999
gidNumber: 9999
homeDirectory: /home/test
# Error: sn es MUST en person (padre de inetOrgPerson)

# BIEN — incluir sn
dn: uid=test,ou=People,dc=example,dc=com
objectClass: inetOrgPerson
objectClass: posixAccount
uid: test
cn: Test User
sn: User
uidNumber: 9999
gidNumber: 9999
homeDirectory: /home/test
```

### Error 5: Confundir el guion separador en LDIF modify

```ldif
# MAL — sin guion separador entre operaciones
dn: uid=alice,ou=People,dc=example,dc=com
changetype: modify
replace: loginShell
loginShell: /bin/zsh
add: description
description: Developer
# Error: el servidor interpreta "add" como valor de loginShell

# BIEN — guion solo en una línea entre operaciones
dn: uid=alice,ou=People,dc=example,dc=com
changetype: modify
replace: loginShell
loginShell: /bin/zsh
-
add: description
description: Developer
-
```

---

## 11. Cheatsheet

```
=== ESTRUCTURA ===
DN (Distinguished Name)    Ruta completa: uid=alice,ou=People,dc=example,dc=com
RDN (Relative DN)          Componente local: uid=alice
Base DN                    Raíz del árbol: dc=example,dc=com
DIT                        Árbol completo del directorio
Entrada                    Nodo del árbol con atributos

=== COMPONENTES DEL DN ===
dc=   Domain Component      dc=example,dc=com
ou=   Organizational Unit   ou=People
cn=   Common Name           cn=developers
uid=  User ID               uid=alice

=== objectClass ===
STRUCTURAL    naturaleza de la entrada (1 por entrada)    inetOrgPerson
AUXILIARY     atributos extra (0+)                        posixAccount
ABSTRACT      clase base (no instanciable)                top

=== OPERACIONES ===
bind          autenticarse        -D "dn" -W
search        buscar              ldapsearch -b base filter attrs
add           crear entrada       ldapadd -f file.ldif
modify        modificar           ldapmodify -f changes.ldif
delete        eliminar            ldapdelete "dn"
compare       verificar valor     ldapcompare "dn" "attr:valor"

=== SCOPE DE BÚSQUEDA ===
base    -s base    solo la entrada del base DN
one     -s one     hijos directos (un nivel)
sub     -s sub     todo el subárbol (por defecto)

=== FILTROS ===
(uid=alice)                     igualdad
(cn=Ali*)                       substring/wildcard
(uidNumber>=1000)               mayor o igual
(description=*)                 presencia (tiene el atributo)
(&(a)(b))                       AND
(|(a)(b))                       OR
(!(a))                          NOT

=== SEGURIDAD ===
Puerto 389     LDAP plano o StartTLS
Puerto 636     LDAPS (TLS inmediato)
-ZZ            exigir StartTLS
-Z             intentar StartTLS
ldaps://       usar LDAPS

=== LDIF ===
dn: ...                         primera línea de cada entrada
objectClass: ...                define atributos permitidos
changetype: modify/add/delete   tipo de cambio
-                               separador entre operaciones modify
::                              valor en base64
```

---

## 12. Ejercicios

### Ejercicio 1: Leer y construir DNs

Dado el siguiente árbol:

```
dc=corp,dc=local
├── ou=Madrid
│   ├── ou=Engineering
│   │   ├── uid=elena
│   │   └── uid=marcos
│   └── ou=Sales
│       └── uid=lucia
├── ou=Barcelona
│   └── ou=Engineering
│       └── uid=elena
└── ou=Groups
    ├── cn=developers
    └── cn=sales
```

1. Escribe el DN completo de `elena` en Madrid
2. Escribe el DN completo de `elena` en Barcelona
3. ¿Viola esto la unicidad de DN? ¿Por qué o por qué no?
4. ¿Cuál es el Base DN más eficiente para buscar a todos los ingenieros de todas las oficinas?
5. Escribe el filtro que encontraría todas las entradas `uid=elena` en todo el directorio

> **Pregunta de predicción**: Si ejecutas `ldapsearch -b "ou=Madrid,dc=corp,dc=local" -s one "(uid=elena)"`, ¿se encontrará a elena? ¿Por qué?

### Ejercicio 2: Escribir LDIF

Escribe un archivo LDIF completo que:

1. Cree la entrada raíz `dc=lab,dc=internal`
2. Cree `ou=Users` bajo la raíz
3. Cree un usuario `uid=testuser` con:
   - objectClass: `inetOrgPerson` y `posixAccount`
   - Nombre completo: Test User
   - UID/GID: 5001/5001
   - Home: `/home/testuser`
   - Shell: `/bin/bash`
4. Luego, escribe un segundo bloque LDIF de modificación que:
   - Cambie el shell a `/bin/zsh`
   - Añada un email `testuser@lab.internal`
   - Elimine el atributo `description` si existe

> **Pregunta de predicción**: Si olvidas incluir `sn` en la entrada del usuario con `inetOrgPerson`, ¿qué ocurrirá exactamente y por qué? ¿Qué error devolverá el servidor?

### Ejercicio 3: Filtros de búsqueda

Escribe los filtros LDAP para cada consulta:

1. Todos los usuarios con UID numérico mayor o igual a 1000
2. Usuarios cuyo login sea `alice` O `bob`
3. Cuentas POSIX con shell `/bin/bash` que NO estén en `ou=Service`
4. Entradas que tengan el atributo `mail` definido (cualquier valor)
5. Grupos POSIX cuyo nombre empiece por `dev`

Para cada filtro, indica qué base DN y scope usarías.

> **Pregunta de predicción**: El filtro `(&(uid=alice)(uid=bob))` ¿devolverá algún resultado? ¿Puede una entrada tener dos valores diferentes para `uid`? ¿Depende de la definición del atributo en el esquema?

---

> **Siguiente tema**: T02 — sssd (instalación, configuración de cliente LDAP/AD, sssd.conf, dominios).
