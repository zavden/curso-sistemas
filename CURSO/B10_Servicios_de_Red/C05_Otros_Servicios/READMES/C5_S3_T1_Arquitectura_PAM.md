# Arquitectura PAM

## Índice
1. [Qué es PAM](#1-qué-es-pam)
2. [El problema que resuelve PAM](#2-el-problema-que-resuelve-pam)
3. [Arquitectura general](#3-arquitectura-general)
4. [Los cuatro tipos de módulo (stacks)](#4-los-cuatro-tipos-de-módulo-stacks)
5. [Flags de control](#5-flags-de-control)
6. [Flujo de evaluación](#6-flujo-de-evaluación)
7. [Módulos y la cadena de responsabilidad](#7-módulos-y-la-cadena-de-responsabilidad)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Qué es PAM

**PAM (Pluggable Authentication Modules)** es un framework que permite a las aplicaciones Linux delegar la autenticación y la gestión de sesiones a módulos intercambiables, sin modificar el código de la aplicación.

```
┌──────────────────────────────────────────────────────────┐
│                    Sin PAM                                │
│                                                          │
│  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐        │
│  │  login  │  │  sshd  │  │   su   │  │  sudo  │        │
│  │        │  │        │  │        │  │        │         │
│  │ verif. │  │ verif. │  │ verif. │  │ verif. │         │
│  │/etc/   │  │/etc/   │  │/etc/   │  │/etc/   │         │
│  │shadow  │  │shadow  │  │shadow  │  │shadow  │         │
│  └────────┘  └────────┘  └────────┘  └────────┘        │
│  Cada aplicación implementa su propia autenticación      │
│  → Duplicación de código, difícil agregar LDAP/Kerberos  │
│                                                          │
├──────────────────────────────────────────────────────────┤
│                    Con PAM                                │
│                                                          │
│  ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐        │
│  │  login  │  │  sshd  │  │   su   │  │  sudo  │        │
│  └───┬────┘  └───┬────┘  └───┬────┘  └───┬────┘        │
│      │           │           │           │              │
│      └───────────┴───────────┴───────────┘              │
│                      │                                   │
│                      ▼                                   │
│              ┌──────────────┐                            │
│              │   libpam     │  ← Framework PAM           │
│              └──────┬───────┘                            │
│                     │                                    │
│          ┌──────────┼──────────┐                         │
│          ▼          ▼          ▼                          │
│     ┌─────────┐ ┌────────┐ ┌──────────┐                │
│     │pam_unix │ │pam_ldap│ │pam_krb5  │                │
│     │/etc/    │ │LDAP    │ │Kerberos  │                │
│     │shadow   │ │server  │ │KDC      │                  │
│     └─────────┘ └────────┘ └──────────┘                │
│  Las aplicaciones no saben cómo se autentica             │
│  → Agregar LDAP = cambiar config PAM, no recompilar apps │
└──────────────────────────────────────────────────────────┘
```

---

## 2. El problema que resuelve PAM

### Sin PAM

Si quieres agregar autenticación LDAP al sistema, necesitas modificar o recompilar cada aplicación que autentica usuarios: `login`, `sshd`, `su`, `sudo`, `passwd`, `cron`, screensavers, etc.

### Con PAM

Cambias un archivo de configuración de PAM y **todas** las aplicaciones que usan PAM automáticamente soportan LDAP. La aplicación no necesita saber qué método de autenticación se usa.

### Qué aplicaciones usan PAM

```bash
# Verificar si una aplicación usa PAM
ldd /usr/sbin/sshd | grep pam
# libpam.so.0 => /lib/x86_64-linux-gnu/libpam.so.0

# Si aparece libpam.so → la aplicación usa PAM
# Si no aparece → la aplicación gestiona autenticación por su cuenta

# Aplicaciones típicas que usan PAM:
# login, sshd, su, sudo, passwd, cron, gdm/lightdm,
# vsftp, dovecot, samba (opcional)
```

---

## 3. Arquitectura general

```
┌──────────────────────────────────────────────────────────┐
│              Arquitectura PAM                             │
│                                                          │
│  Aplicación (ej: sshd)                                   │
│       │                                                  │
│       │ "¿Puedo autenticar a juan?"                      │
│       ▼                                                  │
│  ┌──────────────┐                                        │
│  │   libpam     │  Biblioteca central                    │
│  │              │  Lee /etc/pam.d/sshd                   │
│  └──────┬───────┘                                        │
│         │                                                │
│         │  Ejecuta los módulos en orden:                  │
│         │                                                │
│         ├──► pam_sepermit.so   → ¿SELinux permite?       │
│         ├──► pam_env.so        → Configurar entorno      │
│         ├──► pam_unix.so       → Verificar /etc/shadow   │
│         ├──► pam_sss.so        → Verificar SSSD/LDAP     │
│         ├──► pam_faillock.so   → Contar intentos fallidos│
│         └──► pam_limits.so     → Aplicar límites         │
│                                                          │
│         │                                                │
│         ▼                                                │
│  Resultado: SUCCESS o FAILURE                            │
│  La aplicación recibe un sí o no                         │
└──────────────────────────────────────────────────────────┘
```

### Componentes

| Componente | Ubicación | Función |
|-----------|-----------|---------|
| **libpam** | `/lib/x86_64-linux-gnu/libpam.so` | Biblioteca que las aplicaciones llaman |
| **Archivos de configuración** | `/etc/pam.d/` | Un archivo por aplicación |
| **Módulos** | `/lib/x86_64-linux-gnu/security/` (Debian) o `/lib64/security/` (RHEL) | Plugins `.so` que hacen el trabajo real |
| **Configuración de módulos** | `/etc/security/` | Archivos de config de módulos específicos |

```bash
# Listar módulos PAM instalados
ls /lib/x86_64-linux-gnu/security/pam_*.so    # Debian
ls /lib64/security/pam_*.so                    # RHEL

# Ver configuraciones de módulos
ls /etc/security/
# access.conf  faillock.conf  limits.conf  namespace.conf
# pam_env.conf  pwquality.conf  time.conf  ...
```

---

## 4. Los cuatro tipos de módulo (stacks)

PAM organiza la autenticación en cuatro fases llamadas **stacks** o **tipos de módulo**. Cada fase responde una pregunta diferente:

```
┌──────────────────────────────────────────────────────────┐
│              Los cuatro stacks de PAM                      │
│                                                          │
│  ┌──────────────────────────────────────────────────┐    │
│  │  auth     — ¿QUIÉN eres?                         │    │
│  │            Verifica identidad (contraseña,        │    │
│  │            token, biometría, Kerberos)             │    │
│  │            También puede otorgar credenciales     │    │
│  │            (tickets Kerberos, tokens)              │    │
│  └──────────────────────────────────────────────────┘    │
│                         │                                │
│                         ▼                                │
│  ┌──────────────────────────────────────────────────┐    │
│  │  account  — ¿PUEDES entrar?                      │    │
│  │            Verifica que la cuenta sea válida:      │    │
│  │            ¿Expiró? ¿Está bloqueada?              │    │
│  │            ¿Tiene acceso a esta hora/terminal?     │    │
│  └──────────────────────────────────────────────────┘    │
│                         │                                │
│                         ▼                                │
│  ┌──────────────────────────────────────────────────┐    │
│  │  password — Cambiar credenciales                  │    │
│  │            Se invoca al cambiar contraseña         │    │
│  │            Verifica políticas de complejidad       │    │
│  │            Actualiza la base de datos              │    │
│  └──────────────────────────────────────────────────┘    │
│                         │                                │
│                         ▼                                │
│  ┌──────────────────────────────────────────────────┐    │
│  │  session  — Preparar/limpiar el entorno           │    │
│  │            Antes y después de la sesión:           │    │
│  │            Montar home, crear tmpdir, aplicar      │    │
│  │            límites (ulimit), registrar login,      │    │
│  │            configurar variables de entorno          │    │
│  └──────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────┘
```

### Orden de ejecución

Cuando un usuario hace login:

```
1. auth      → "Dame tu contraseña" → verifica → OK/FAIL
2. account   → "¿Tu cuenta es válida?" → verifica → OK/FAIL
3. session   → "Preparo tu entorno" → monta home, aplica límites
4. (al salir)
   session   → "Limpio tu entorno" → desmonta, registra logout

Cuando un usuario cambia su contraseña:
1. auth      → "Dame tu contraseña actual"
2. password  → "¿La nueva cumple las políticas?" → actualiza
```

### Ejemplo real: /etc/pam.d/login

```bash
# Cada línea: type  control  module  [arguments]
auth       required   pam_securetty.so           # ¿root puede en esta tty?
auth       required   pam_unix.so                # Verificar /etc/shadow
auth       required   pam_nologin.so             # ¿Existe /etc/nologin?

account    required   pam_unix.so                # ¿Cuenta válida?
account    required   pam_access.so              # ¿Acceso permitido?

password   required   pam_pwquality.so retry=3   # Calidad de contraseña
password   required   pam_unix.so sha512 shadow  # Actualizar shadow

session    required   pam_limits.so              # Aplicar límites
session    required   pam_unix.so                # Registrar login
session    optional   pam_motd.so                # Mostrar /etc/motd
```

---

## 5. Flags de control

El flag de control determina **cómo afecta el resultado de cada módulo** al resultado final del stack.

### Los cuatro flags básicos

```
┌──────────────────────────────────────────────────────────┐
│              Flags de control PAM                         │
│                                                          │
│  required                                                │
│  ────────                                                │
│  DEBE tener éxito para que el stack pase.                │
│  Si falla, el stack falla, PERO los demás módulos        │
│  se siguen ejecutando (para no revelar cuál falló).      │
│                                                          │
│  requisite                                               │
│  ─────────                                               │
│  DEBE tener éxito. Si falla, el stack falla              │
│  INMEDIATAMENTE — no ejecuta más módulos.                │
│  Revela que este módulo específico falló.                 │
│                                                          │
│  sufficient                                              │
│  ──────────                                              │
│  Si tiene éxito Y ningún required previo falló,          │
│  el stack PASA inmediatamente (ignora módulos restantes). │
│  Si falla, se ignora y continúa con el siguiente.        │
│                                                          │
│  optional                                                │
│  ────────                                                │
│  Su resultado solo importa si es el ÚNICO módulo         │
│  del stack. Normalmente se ignora el resultado.           │
│  Útil para acciones que no deben afectar el login.       │
└──────────────────────────────────────────────────────────┘
```

### Comparación visual

```
Módulo retorna:     SUCCESS              FAILURE
─────────────────────────────────────────────────────
required            Continúa ✓           Continúa, pero stack fallará ✗
requisite           Continúa ✓           PARA inmediatamente ✗
sufficient          PARA inmediatamente ✓ Continúa (ignora fallo)
optional            Continúa (ignora)    Continúa (ignora)
```

### Sintaxis extendida de control

Además de los cuatro keywords, PAM soporta una sintaxis con corchetes para control fino:

```bash
# Sintaxis extendida
auth [success=2 default=ignore] pam_unix.so
# Si éxito: salta 2 módulos
# Si falla: ignora y continúa

auth [success=done new_authtok_reqd=done default=die] pam_sss.so
# Si éxito: stack pasa (done)
# Si necesita nuevo token: stack pasa
# Cualquier otro resultado: stack falla (die)
```

Valores posibles:

| Acción | Significado |
|--------|------------|
| `ignore` | No afecta el resultado del stack |
| `ok` | El resultado contribuye al resultado final |
| `done` | Éxito inmediato del stack |
| `die` | Fallo inmediato del stack |
| `bad` | Marca como fallo pero continúa |
| `reset` | Reinicia el estado del stack |
| `N` | Salta N módulos |

---

## 6. Flujo de evaluación

### Ejemplo paso a paso con required

```
auth  required   pam_env.so
auth  required   pam_unix.so
auth  required   pam_deny.so

Escenario: usuario con contraseña correcta
  pam_env.so    → SUCCESS (configura entorno) → continúa
  pam_unix.so   → SUCCESS (contraseña OK)     → continúa
  pam_deny.so   → FAILURE (siempre falla)     → continúa
  Resultado: FAILURE (un required falló)

Escenario: quitar pam_deny.so
  pam_env.so    → SUCCESS → continúa
  pam_unix.so   → SUCCESS → continúa
  Resultado: SUCCESS (todos los required pasaron)
```

### Ejemplo con sufficient

```
auth  required     pam_env.so
auth  sufficient   pam_unix.so
auth  sufficient   pam_sss.so
auth  required     pam_deny.so

Escenario: usuario local con contraseña correcta
  pam_env.so    → SUCCESS (required OK)       → continúa
  pam_unix.so   → SUCCESS (sufficient OK)     → PARA, stack PASA ✓
  (pam_sss.so y pam_deny.so nunca se ejecutan)

Escenario: usuario LDAP (no existe en local)
  pam_env.so    → SUCCESS (required OK)       → continúa
  pam_unix.so   → FAILURE (user not found)    → ignora, continúa
  pam_sss.so    → SUCCESS (LDAP auth OK)      → PARA, stack PASA ✓
  (pam_deny.so nunca se ejecuta)

Escenario: usuario inexistente
  pam_env.so    → SUCCESS                     → continúa
  pam_unix.so   → FAILURE (sufficient fail)   → ignora, continúa
  pam_sss.so    → FAILURE (sufficient fail)   → ignora, continúa
  pam_deny.so   → FAILURE (required fail)     → stack FALLA ✗
```

Este es el patrón **"intentar local, luego LDAP, luego denegar"** — muy común en producción.

### Ejemplo con requisite

```
auth  requisite  pam_nologin.so
auth  required   pam_unix.so

Escenario: existe /etc/nologin
  pam_nologin.so → FAILURE (requisite) → PARA inmediatamente ✗
  (pam_unix.so nunca se ejecuta — ni pide contraseña)
  El usuario ve "System is going down for maintenance"

Escenario: no existe /etc/nologin
  pam_nologin.so → SUCCESS → continúa
  pam_unix.so    → pide contraseña...
```

`requisite` es útil cuando quieres **cortocircuitar** sin ejecutar el resto (no pedir contraseña si el sistema está en mantenimiento).

---

## 7. Módulos y la cadena de responsabilidad

### Cómo interactúan los stacks

```
┌──────────────────────────────────────────────────────────┐
│     Flujo completo de un login SSH                        │
│                                                          │
│  sshd llama a PAM → lee /etc/pam.d/sshd                 │
│                                                          │
│  Stack auth:                                             │
│  ├── pam_sepermit.so   required   → ¿SELinux OK? ✓      │
│  ├── pam_env.so        required   → Cargar env ✓        │
│  ├── pam_unix.so       sufficient → ¿Shadow OK?         │
│  │   ├── Sí → stack auth PASA ────────────────────►     │
│  │   └── No → continúa                                  │
│  ├── pam_sss.so        sufficient → ¿LDAP OK?           │
│  │   ├── Sí → stack auth PASA ────────────────────►     │
│  │   └── No → continúa                                  │
│  └── pam_deny.so       required   → FALLA ✗              │
│                                    ▼                     │
│  Stack account:                                          │
│  ├── pam_unix.so       required   → ¿Cuenta válida? ✓   │
│  ├── pam_access.so     required   → ¿Acceso permitido? ✓│
│  └── pam_time.so       required   → ¿Hora correcta? ✓   │
│                                    ▼                     │
│  Stack session (open):                                   │
│  ├── pam_limits.so     required   → Aplicar ulimits      │
│  ├── pam_unix.so       required   → Registrar login      │
│  ├── pam_mkhomedir.so  optional   → Crear home si falta  │
│  └── pam_motd.so       optional   → Mostrar MOTD         │
│                                    ▼                     │
│  Sesión SSH activa...                                    │
│                                    ▼                     │
│  Stack session (close):                                  │
│  ├── pam_limits.so     required   → Limpiar límites      │
│  └── pam_unix.so       required   → Registrar logout     │
└──────────────────────────────────────────────────────────┘
```

### Include y substack

Las configuraciones PAM pueden incluir otras para evitar duplicación:

```bash
# /etc/pam.d/sshd
auth       include      common-auth      # Incluye reglas de common-auth
account    include      common-account
session    include      common-session
password   include      common-password
```

```bash
# /etc/pam.d/common-auth (Debian)
auth  [success=2 default=ignore]  pam_unix.so nullok
auth  [success=1 default=ignore]  pam_sss.so use_first_pass
auth  requisite                   pam_deny.so
auth  required                    pam_permit.so
```

| Directiva | Comportamiento |
|-----------|---------------|
| `include` | Incluye las líneas del archivo referenciado. Si el stack incluido falla, el stack padre falla. |
| `substack` | Similar a include, pero el resultado del substack se evalúa como una unidad. Un `sufficient` en el substack NO termina el stack padre. |
| `@include` | Include de archivo (Debian legacy). |

### Diferencia include vs substack

```
# Con include:
auth sufficient pam_unix.so    ← (dentro del include)
# Si pam_unix.so pasa → el stack COMPLETO del padre pasa

# Con substack:
auth sufficient pam_unix.so    ← (dentro del substack)
# Si pam_unix.so pasa → solo el substack pasa
# El stack padre continúa evaluando
```

---

## 8. Errores comunes

### Error 1 — Bloquear root al editar PAM incorrectamente

```bash
# PELIGRO: si pones pam_deny.so como required en auth
# y no hay otro módulo sufficient antes, NADIE puede hacer login
# Incluyendo root

# SIEMPRE tener una sesión root abierta mientras editas PAM
# Si te bloqueas:
# 1. Arrancar en modo single/rescue
# 2. Montar el filesystem
# 3. Revertir /etc/pam.d/

# Regla de oro: NUNCA cerrar la última sesión root
# hasta verificar que puedes abrir una nueva
```

### Error 2 — Confundir required con requisite

```bash
# required: falla pero ejecuta los demás módulos
auth required   pam_unix.so     # Falla → sigue ejecutando
auth required   pam_env.so      # Se ejecuta aunque el anterior falló
# Resultado: FAIL (pero sin revelar cuál módulo falló)

# requisite: falla y para inmediatamente
auth requisite  pam_unix.so     # Falla → PARA
auth required   pam_env.so      # NUNCA se ejecuta
# Resultado: FAIL (revela que pam_unix falló)

# En autenticación, "required" es más seguro: no revela
# a un atacante si el usuario existe (el módulo de
# contraseña siempre se ejecuta)
```

### Error 3 — sufficient antes de required necesarios

```bash
# MAL: sufficient antes de un required que configura el entorno
auth sufficient pam_unix.so     # Si pasa → para, salta pam_env
auth required   pam_env.so      # Nunca se ejecuta si unix pasó

# BIEN: required primero, sufficient después
auth required   pam_env.so      # Siempre se ejecuta
auth sufficient pam_unix.so     # Si pasa → para con éxito
```

### Error 4 — Olvidar que optional solo importa si es el único

```bash
# Si todos los módulos son optional:
auth optional pam_unix.so
auth optional pam_sss.so
# Si ambos fallan → stack FALLA (es el único resultado)
# Si uno pasa → stack PASA

# En la práctica, optional se usa para acciones secundarias
# que no deben afectar el resultado del login:
session optional pam_motd.so    # Si falla, el login sigue
session optional pam_mail.so    # Si falla, el login sigue
```

### Error 5 — No probar con una sesión root activa

```bash
# Flujo seguro para editar PAM:
# 1. Abrir DOS terminales/SSH como root
# 2. En terminal 1: editar PAM
# 3. En terminal 2: probar login (su - usuario)
# 4. Si funciona: cerrar terminal 1
# 5. Si NO funciona: revertir en terminal 1

# NUNCA editar PAM y cerrar tu sesión para probar
```

---

## 9. Cheatsheet

```bash
# ── Archivos y directorios ────────────────────────────────
/etc/pam.d/                          # Configuración por aplicación
/etc/pam.d/common-auth               # Auth compartido (Debian)
/etc/pam.d/system-auth               # Auth compartido (RHEL)
/etc/pam.d/common-account            # Account compartido (Debian)
/etc/pam.d/password-auth             # Auth para apps remotas (RHEL)
/etc/security/                       # Config de módulos específicos
/lib/x86_64-linux-gnu/security/      # Módulos .so (Debian)
/lib64/security/                     # Módulos .so (RHEL)

# ── Sintaxis de línea ────────────────────────────────────
# type    control    module.so    [arguments]
# auth    required   pam_unix.so  nullok

# ── Los 4 tipos (stacks) ─────────────────────────────────
# auth      → ¿Quién eres? (verificar identidad)
# account   → ¿Puedes entrar? (cuenta válida, no expirada)
# password  → Cambiar credenciales (política + actualización)
# session   → Preparar/limpiar entorno (límites, home, motd)

# ── Flags de control ─────────────────────────────────────
# required    → Debe pasar. Si falla, sigue ejecutando, stack falla
# requisite   → Debe pasar. Si falla, PARA inmediatamente
# sufficient  → Si pasa (y no hay required fallido), stack pasa y PARA
# optional    → Resultado ignorado (salvo que sea el único módulo)

# ── Inclusión ─────────────────────────────────────────────
# include /etc/pam.d/common-auth    → Incluir otro archivo
# substack /etc/pam.d/common-auth   → Incluir como subunidad

# ── Verificaciones ────────────────────────────────────────
ldd /usr/sbin/sshd | grep pam       # ¿App usa PAM?
ls /lib/x86_64-linux-gnu/security/  # Módulos disponibles
cat /etc/pam.d/sshd                 # Config PAM de sshd

# ── Depuración ────────────────────────────────────────────
# Agregar temporalmente a /etc/pam.d/servicio:
# auth required pam_warn.so         → registra en syslog
# Ver: journalctl | grep pam_warn

# ── Regla de seguridad ───────────────────────────────────
# SIEMPRE mantener una sesión root abierta mientras editas PAM
# Probar login en OTRA terminal antes de cerrar
```

---

## 10. Ejercicios

### Ejercicio 1 — Analizar un stack de autenticación

Dado el siguiente `/etc/pam.d/login`, determina qué ocurre en cada escenario.

```bash
auth     requisite    pam_nologin.so
auth     required     pam_env.so
auth     sufficient   pam_unix.so nullok
auth     sufficient   pam_sss.so use_first_pass
auth     required     pam_deny.so
```

**Escenario A:** Existe `/etc/nologin` y un usuario intenta login.

```
pam_nologin.so → FAILURE (requisite) → PARA inmediatamente
Resultado: FALLA — el usuario ve el mensaje de /etc/nologin
pam_unix.so nunca se ejecuta (no pide contraseña)
```

**Escenario B:** No existe `/etc/nologin`, usuario local con contraseña correcta.

```
pam_nologin.so → SUCCESS → continúa
pam_env.so     → SUCCESS → continúa
pam_unix.so    → SUCCESS (sufficient) → PARA, stack PASA ✓
```

**Escenario C:** No existe `/etc/nologin`, usuario LDAP con contraseña correcta.

```
pam_nologin.so → SUCCESS → continúa
pam_env.so     → SUCCESS → continúa
pam_unix.so    → FAILURE (no existe localmente) → ignora, continúa
pam_sss.so     → SUCCESS (sufficient, LDAP OK) → PARA, stack PASA ✓
```

**Pregunta de predicción:** ¿Qué pasa si un usuario introduce una contraseña incorrecta tanto en local como en LDAP?

> **Respuesta:** `pam_nologin.so` pasa, `pam_env.so` pasa, `pam_unix.so` falla (sufficient se ignora al fallar), `pam_sss.so` falla (se ignora), `pam_deny.so` falla (required). El stack **falla**. El usuario recibe "Login incorrect". El módulo `pam_deny.so` al final es la **red de seguridad**: si ningún sufficient tuvo éxito, `pam_deny.so` garantiza que el stack falle — sin él, el stack podría pasar inadvertidamente si todos los módulos fueran sufficient u optional.

---

### Ejercicio 2 — Diseñar un stack PAM

Diseña la configuración auth de `/etc/pam.d/custom-app` con estos requisitos:
- Rechazar si existe `/etc/nologin`
- Intentar autenticación local primero
- Si falla local, intentar LDAP
- Si ambos fallan, denegar acceso
- Registrar todos los intentos en syslog

```bash
# /etc/pam.d/custom-app
auth     requisite    pam_nologin.so
auth     required     pam_env.so
auth     optional     pam_warn.so             # Registra en syslog
auth     sufficient   pam_unix.so nullok
auth     sufficient   pam_sss.so use_first_pass
auth     required     pam_deny.so
```

**Pregunta de predicción:** ¿Qué hace el argumento `use_first_pass` en `pam_sss.so`? ¿Qué pasaría si se omitiera?

> **Respuesta:** `use_first_pass` le dice a `pam_sss.so` que use la contraseña que ya se introdujo para `pam_unix.so`, sin pedir una segunda vez. Sin este argumento, el módulo LDAP pediría la contraseña otra vez — el usuario tendría que escribirla dos veces. La alternativa `try_first_pass` es similar pero si la contraseña almacenada falla, pide una nueva. `use_first_pass` nunca pide otra contraseña — si la primera no sirve, falla directamente.

---

### Ejercicio 3 — Evaluar impacto de cambios

Un administrador propone cambiar el stack auth de `sshd`. Evalúa cada cambio propuesto.

**Cambio 1:** Reemplazar `required` con `requisite` en `pam_unix.so`.

```bash
# Antes:  auth required   pam_unix.so
# Después: auth requisite  pam_unix.so
```

> **Impacto:** Si la contraseña es incorrecta, PAM falla inmediatamente sin ejecutar módulos posteriores. Esto revela al atacante que el **usuario existe** (porque si no existiera, el error sería diferente). Con `required`, todos los módulos se ejecutan igualmente, ocultando dónde ocurrió el fallo. **No recomendado** para auth por razones de seguridad.

**Cambio 2:** Agregar `pam_permit.so` al final en lugar de `pam_deny.so`.

```bash
# Antes:  auth required pam_deny.so
# Después: auth required pam_permit.so
```

> **Impacto:** `pam_permit.so` siempre retorna SUCCESS. Si ningún `sufficient` tuvo éxito, `pam_permit.so` con `required` haría que el stack **pase sin autenticación real**. Cualquier usuario con cualquier contraseña (o sin contraseña) podría hacer login. **Extremadamente peligroso** — nunca usar `pam_permit.so` como red de seguridad en auth.

**Cambio 3:** Cambiar `optional` a `required` en `pam_motd.so` (session stack).

```bash
# Antes:  session optional pam_motd.so
# Después: session required pam_motd.so
```

**Pregunta de predicción:** Si `/etc/motd` no existe o `pam_motd.so` falla por cualquier razón, ¿qué ocurre con el login del usuario?

> **Respuesta:** Con `required`, si `pam_motd.so` falla, **todo el stack session falla** y el login se rechaza, incluso si la autenticación fue correcta. Esto es absurdo — nadie quiere que un MOTD faltante impida el login. Por eso se usa `optional` para módulos de session que no son críticos: si fallan, el login continúa normalmente. La regla es: `required` solo para módulos cuyo fallo debe bloquear el acceso.

---

> **Siguiente:** T02 — Archivos de configuración PAM (/etc/pam.d/, sintaxis, orden de módulos).
