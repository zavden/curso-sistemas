# named-checkconf y named-checkzone

## Índice

1. [Por qué validar antes de recargar](#por-qué-validar-antes-de-recargar)
2. [named-checkconf](#named-checkconf)
3. [named-checkzone](#named-checkzone)
4. [Integración en flujos de trabajo](#integración-en-flujos-de-trabajo)
5. [Catálogo de errores frecuentes](#catálogo-de-errores-frecuentes)
6. [Errores comunes](#errores-comunes)
7. [Cheatsheet](#cheatsheet)
8. [Ejercicios](#ejercicios)

---

## Por qué validar antes de recargar

Un error en la configuración de BIND puede tener consecuencias graves:

```
    Error en named.conf:
    ├── named no arranca → todo el DNS caído
    └── named arranca pero ignora una zona → zona específica caída

    Error en archivo de zona:
    ├── Zona no carga → SERVFAIL para ese dominio
    ├── Registro mal formado → respuestas incorrectas
    └── Serial no incrementado → secundarios obsoletos
```

La regla de oro es simple:

```bash
# SIEMPRE validar antes de recargar
sudo named-checkconf && sudo named-checkzone zona archivo && sudo systemctl reload named
```

Las herramientas `named-checkconf` y `named-checkzone` detectan errores **antes** de que afecten al servicio. Son baratas de ejecutar y evitan incidentes costosos.

---

## named-checkconf

### Uso básico

```bash
# Validar la configuración (named.conf + includes)
sudo named-checkconf
# Sin salida = configuración válida
# Con error = muestra archivo, línea y descripción

# Ejemplo de error:
sudo named-checkconf
# /etc/bind/named.conf.options:15: unknown option 'recursio'
#                                    ↑ typo: "recursio" en vez de "recursion"
```

### Opciones

```bash
# -p: imprimir la configuración completa resuelta (includes expandidos)
sudo named-checkconf -p
# Muestra toda la configuración como la ve named
# Útil para verificar qué valor tiene realmente una directiva

# -p filtrado: extraer directivas específicas
sudo named-checkconf -p | grep -i "recursion"
# recursion yes;

# -x: igual que -p pero oculta claves secretas
sudo named-checkconf -x
# Las claves TSIG/rndc se muestran como "***"

# -z: validar config + cargar y validar todas las zonas referenciadas
sudo named-checkconf -z
# zone ejemplo.com/IN: loaded serial 2026032101
# zone lab.local/IN: loaded serial 2026032101
# zone 1.168.192.in-addr.arpa/IN: loaded serial 2026032101
# Si alguna zona tiene error, lo muestra aquí

# -j: verificar también los journal files (zonas dinámicas)
sudo named-checkconf -z -j

# Archivo alternativo (validar un archivo que no es el default)
sudo named-checkconf /tmp/named.conf.test
```

### Salida de errores

`named-checkconf` reporta errores con formato consistente:

```
archivo:línea: descripción del error
```

Ejemplos reales:

```bash
# Error de sintaxis: falta punto y coma
# /etc/bind/named.conf.options:10: missing ';' before 'recursion'

# Directiva desconocida
# /etc/bind/named.conf:5: unknown option 'recusion'

# Error en ACL
# /etc/bind/named.conf:3: undefined ACL 'trusted'
# → la ACL "trusted" se usa en options pero no está definida

# Zona referencia archivo inexistente
# /etc/bind/named.conf.local:5: file not found
# → con -z, también verifica que los archivos de zona existan

# Error en bloque de zona
# /etc/bind/named.conf.local:8: zone 'ejemplo.com': missing 'file' entry
```

### Validar antes de aplicar cambios

```bash
# Patrón seguro para editar configuración:

# 1. Editar
sudo vim /etc/bind/named.conf.local

# 2. Validar sintaxis
sudo named-checkconf
# Si hay error → corregir y repetir

# 3. Validar con zonas
sudo named-checkconf -z
# Si alguna zona falla → corregir

# 4. Solo si todo está OK → recargar
sudo systemctl reload named

# Todo junto (si el primer fallo detiene la cadena):
sudo named-checkconf -z && sudo systemctl reload named
```

---

## named-checkzone

### Uso básico

```bash
# Sintaxis:
named-checkzone nombre_zona archivo_zona

# Ejemplo:
sudo named-checkzone ejemplo.com /etc/bind/zones/db.ejemplo.com
# zone ejemplo.com/IN: loaded serial 2026032101
# OK

# Si hay error:
sudo named-checkzone ejemplo.com /etc/bind/zones/db.ejemplo.com
# zone ejemplo.com/IN: loading from master file /etc/bind/zones/db.ejemplo.com failed:
# /etc/bind/zones/db.ejemplo.com:12: unknown RR type 'INN'
# zone ejemplo.com/IN: not loaded due to errors.
```

### Opciones

```bash
# -d: modo debug (más detalle)
named-checkzone -d ejemplo.com /etc/bind/zones/db.ejemplo.com

# -i: verificar integridad (MX/SRV apuntan a nombres con A records)
named-checkzone -i full ejemplo.com /etc/bind/zones/db.ejemplo.com
# Niveles: full, full-sibling, none
# full: verifica que MX/NS/SRV resuelven dentro O fuera de la zona
# full-sibling: solo verifica dentro de la zona
# none: no verifica integridad

# -k: verificar formato "check-names" (RFC 952)
named-checkzone -k fail ejemplo.com /etc/bind/zones/db.ejemplo.com
# fail: error si un nombre viola RFC 952
# warn: warning (default)
# ignore: no verificar

# -w: directorio de trabajo (dónde buscar archivos incluidos)
named-checkzone -w /etc/bind ejemplo.com zones/db.ejemplo.com

# -o: escribir la zona en formato canónico (útil para comparar)
named-checkzone -o /tmp/zona-canonica.txt ejemplo.com /etc/bind/zones/db.ejemplo.com
# Genera un archivo con la zona en formato normalizado
# Todos los nombres expandidos, TTLs explícitos, orden canónico
```

### Zona reversa

```bash
# Para zonas reversas, el nombre de zona incluye in-addr.arpa
named-checkzone 1.168.192.in-addr.arpa /etc/bind/zones/db.192.168.1
# zone 1.168.192.in-addr.arpa/IN: loaded serial 2026032101
# OK
```

### Tipos de errores que detecta

```bash
# === Errores de sintaxis ===
# "unknown RR type 'INN'"          → typo en tipo de registro (IN → INN)
# "unexpected end of input"         → falta cierre de paréntesis en SOA
# "syntax error near 'www'"         → falta separador o campo

# === Errores de estructura ===
# "no SOA record"                   → la zona no tiene registro SOA
# "has no NS records"               → la zona no tiene registros NS
# "not at top of zone"              → SOA no está al inicio de la zona

# === Errores de integridad (con -i) ===
# "MX 'mail.ejemplo.com' has no address records (A or AAAA)"
# → MX apunta a un nombre sin registro A
# "NS 'ns1.ejemplo.com' has no address records"
# → NS sin registro A

# === Warnings ===
# "CNAME and other data"            → CNAME coexiste con otro registro
# "has no address records (A or AAAA)" → nombre referenciado sin IP
# "check-names failure"             → nombre con caracteres inválidos
```

### Salida canónica con -o

La opción `-o` normaliza el archivo de zona. Esto es útil para:

- Comparar versiones de zonas
- Verificar cómo interpreta BIND los nombres relativos
- Generar un archivo limpio sin ambigüedades

```bash
named-checkzone -o - ejemplo.com /etc/bind/zones/db.ejemplo.com
# Escribe a stdout (- = stdout)

# Ejemplo de lo que produce:
# ejemplo.com.         86400  IN  SOA  ns1.ejemplo.com. admin.ejemplo.com. 2026032101 3600 900 604800 86400
# ejemplo.com.         86400  IN  NS   ns1.ejemplo.com.
# ejemplo.com.         86400  IN  NS   ns2.ejemplo.com.
# ejemplo.com.         86400  IN  MX   10 mail.ejemplo.com.
# mail.ejemplo.com.    86400  IN  A    192.168.1.10
# ns1.ejemplo.com.     86400  IN  A    192.168.1.1
# ns2.ejemplo.com.     86400  IN  A    192.168.1.2
# www.ejemplo.com.     86400  IN  A    192.168.1.20
#
# Todos los nombres son FQDN (con punto)
# Todos los TTLs son explícitos
# Orden canónico (alfabético por nombre)
```

Esto revela inmediatamente errores de punto final:

```bash
# Si escribiste "ns1.ejemplo.com" (sin punto) en tu zona:
# La salida canónica mostrará:
# ns1.ejemplo.com.ejemplo.com.  86400  IN  A  192.168.1.1
#                  ↑ duplicado — claramente incorrecto
```

---

## Integración en flujos de trabajo

### Pre-commit hook para Git

Si versionas tus zonas en Git:

```bash
#!/bin/bash
# .git/hooks/pre-commit

ZONE_DIR="zones"
ERRORS=0

for zone_file in $(git diff --cached --name-only --diff-filter=ACM | grep "^$ZONE_DIR/"); do
    # Extraer nombre de zona del nombre del archivo
    zone_name=$(basename "$zone_file" | sed 's/^db\.//')

    echo "Validando zona: $zone_name ($zone_file)"
    named-checkzone "$zone_name" "$zone_file"
    if [ $? -ne 0 ]; then
        ERRORS=$((ERRORS + 1))
    fi
done

if [ $ERRORS -gt 0 ]; then
    echo "ERROR: $ERRORS zona(s) con errores. Commit abortado."
    exit 1
fi
```

### Script de despliegue seguro

```bash
#!/bin/bash
# deploy-dns.sh — despliega cambios DNS con validación

set -e

echo "=== Validando configuración ==="
named-checkconf -z
echo "Configuración OK"

echo ""
echo "=== Validando zonas modificadas ==="
for zone_file in /etc/bind/zones/db.*; do
    zone_name=$(basename "$zone_file" | sed 's/^db\.//')
    echo -n "  $zone_name: "
    if named-checkzone -i full "$zone_name" "$zone_file" > /dev/null 2>&1; then
        echo "OK"
    else
        echo "ERROR"
        named-checkzone -i full "$zone_name" "$zone_file"
        exit 1
    fi
done

echo ""
echo "=== Recargando named ==="
systemctl reload named

echo ""
echo "=== Verificación post-recarga ==="
for zone_file in /etc/bind/zones/db.*; do
    zone_name=$(basename "$zone_file" | sed 's/^db\.//')
    SERIAL=$(dig @127.0.0.1 "$zone_name" SOA +short | awk '{print $3}')
    echo "  $zone_name: serial $SERIAL"
done

echo ""
echo "Despliegue completado."
```

### Validar cambios antes de rndc reload

```bash
# Alias para tu shell (en ~/.bashrc o ~/.zshrc):
alias dns-reload='sudo named-checkconf -z && sudo systemctl reload named && echo "DNS recargado OK"'
alias dns-check='sudo named-checkconf -z'
```

### CI/CD para zonas DNS

```yaml
# .gitlab-ci.yml (ejemplo)
validate-dns:
  stage: test
  image: ubuntu:24.04
  before_script:
    - apt-get update && apt-get install -y bind9utils
  script:
    - named-checkconf named.conf
    - |
      for f in zones/db.*; do
        zone=$(basename "$f" | sed 's/^db\.//')
        named-checkzone -i full "$zone" "$f"
      done
  only:
    changes:
      - named.conf*
      - zones/**
```

---

## Catálogo de errores frecuentes

### Errores de named-checkconf

| Error | Causa | Solución |
|---|---|---|
| `missing ';' before 'X'` | Falta `;` al final de una línea | Añadir `;` |
| `unknown option 'X'` | Directiva mal escrita o inexistente | Verificar nombre correcto |
| `undefined ACL 'X'` | ACL usada pero no definida | Definir la ACL antes de usarla |
| `zone 'X': missing 'file' entry` | Zona sin directiva `file` | Añadir `file "ruta";` |
| `open: /path/file: permission denied` | named no puede leer el archivo | Verificar permisos y propietario |
| `'view' X: already exists` | Dos vistas con el mismo nombre | Renombrar una |
| `when using 'view' ...must be in a view` | Zona fuera de un view | Mover zona dentro de un view |

### Errores de named-checkzone

| Error | Causa | Solución |
|---|---|---|
| `no SOA record` | Falta el registro SOA | Añadir SOA al inicio |
| `has no NS records` | Falta registro NS | Añadir al menos 2 NS |
| `unknown RR type 'X'` | Tipo de registro mal escrito | Corregir (ej. `INN` → `IN`) |
| `CNAME and other data` | CNAME coexistiendo con otro tipo | Eliminar uno u otro |
| `unexpected end of input` | Paréntesis no cerrado en SOA | Cerrar los paréntesis |
| `bad dotted quad` | IP mal formada en registro A | Corregir la IP |
| `has no address records (A or AAAA)` | NS/MX apunta a nombre sin A | Añadir registro A |
| `not at top of zone` | SOA no está al inicio del archivo | Mover SOA al principio |
| `ignoring out-of-zone data` | Registro fuera del dominio de la zona | Eliminar o corregir el nombre |

### Ejemplo: diagnosticar un out-of-zone data

```
; Zona: ejemplo.com
; Este registro NO pertenece a esta zona:
test.otrodominio.com.  IN  A  192.168.1.50
; named-checkzone dice: "ignoring out-of-zone data (test.otrodominio.com)"
; El registro se ignora silenciosamente — puede ser un error de punto final
```

Causa habitual: olvidaste el punto final en un CNAME o NS:

```
; Quisiste escribir:
www  IN  CNAME  proxy.ejemplo.com.    ← correcto
; Pero escribiste:
www  IN  CNAME  proxy.ejemplo.com     ← sin punto
; Se expande a: proxy.ejemplo.com.ejemplo.com.
; named dice: "ignoring out-of-zone data"
```

---

## Errores comunes

### 1. Validar named-checkconf pero olvidar named-checkzone

```bash
# named-checkconf SIN -z no verifica el contenido de las zonas
sudo named-checkconf                    # ← solo sintaxis de named.conf
# OK

sudo systemctl reload named
# named carga la zona con un error de punto final
# Los registros incorrectos se ignoran silenciosamente

# BIEN — usar -z para validar config + zonas
sudo named-checkconf -z
# zone ejemplo.com/IN: loaded serial 2026032101
# (detecta errores en las zonas también)
```

### 2. Confiar solo en named-checkzone sin -i

```bash
# named-checkzone sin -i acepta MX que apunta a CNAME
named-checkzone ejemplo.com db.ejemplo.com
# OK  ← pero MX apunta a un CNAME (viola RFC)

# Con -i full:
named-checkzone -i full ejemplo.com db.ejemplo.com
# MX 'mail.ejemplo.com' is a CNAME (illegal)
```

### 3. No usar la salida canónica para depurar

```bash
# El archivo de zona "se ve bien" pero algo falla
# Usa -o para ver exactamente cómo interpreta BIND los nombres:

named-checkzone -o - ejemplo.com db.ejemplo.com | grep "ejemplo.com.ejemplo.com"
# Si ves algo como "ns1.ejemplo.com.ejemplo.com." → punto final olvidado
```

### 4. Validar con usuario incorrecto

```bash
# named-checkconf necesita poder leer los archivos como root (o bind/named)
named-checkconf
# open: /etc/bind/rndc.key: permission denied

# Solución: ejecutar como root
sudo named-checkconf
```

### 5. Cambios en la zona sin recargar

```bash
# Editaste el archivo de zona y validaste con named-checkzone
# Pero olvidaste recargar:
sudo named-checkzone ejemplo.com /etc/bind/zones/db.ejemplo.com
# OK
# ... ¿y luego? named sigue sirviendo la versión antigua

# Recargar la zona específica:
sudo rndc reload ejemplo.com

# O recargar todo:
sudo systemctl reload named
```

---

## Cheatsheet

```bash
# === named-checkconf ===
sudo named-checkconf                     # validar sintaxis
sudo named-checkconf -z                  # validar config + zonas
sudo named-checkconf -p                  # imprimir config resuelta
sudo named-checkconf -x                  # imprimir ocultando secretos
sudo named-checkconf /ruta/named.conf    # validar archivo específico

# === named-checkzone ===
named-checkzone zona archivo             # validar zona
named-checkzone -i full zona archivo     # con verificación de integridad
named-checkzone -o - zona archivo        # salida canónica a stdout
named-checkzone -o /tmp/out zona archivo # salida canónica a archivo
named-checkzone -k fail zona archivo     # estricto con nombres RFC 952

# === Zonas reversas ===
named-checkzone 1.168.192.in-addr.arpa /etc/bind/zones/db.192.168.1

# === Patrón seguro ===
sudo named-checkconf -z && sudo systemctl reload named

# === Diagnóstico de errores ===
# Ver la zona como BIND la interpreta:
named-checkzone -o - zona archivo | less

# Buscar errores de punto final:
named-checkzone -o - zona archivo | grep -E "\.zona\.\."
# (nombres que tienen el dominio duplicado)

# Verificar integridad de MX/NS:
named-checkzone -i full zona archivo 2>&1 | grep -i "no address"

# === Verificación post-recarga ===
dig @127.0.0.1 zona SOA +short          # verificar serial
sudo rndc zonestatus zona               # estado completo de la zona
```

---

## Ejercicios

### Ejercicio 1 — Detectar errores con named-checkconf

Crea un archivo `named.conf.test` con los siguientes errores intencionados y usa `named-checkconf` para encontrarlos:

```c
options {
    directory "/var/cache/bind"
    recursion yess;
    allow-query { trusted };
    listen-on { 127.0.0.1; }
};

zone "test.com" {
    type master;
};
```

1. Ejecuta `named-checkconf named.conf.test` y anota cada error reportado
2. Corrige los errores uno a uno, validando después de cada corrección
3. Explica por qué BIND reporta los errores en el orden que lo hace

<details>
<summary>Solución</summary>

Errores en orden:

1. **Línea 2**: `missing ';'` después de `"/var/cache/bind"` — falta el `;`
2. **Línea 3**: `'yess' unexpected` — después de corregir línea 2, `yess` no es un valor válido para recursion (debe ser `yes` o `no`)
3. **Línea 4**: `undefined ACL 'trusted'` — la ACL `trusted` se usa pero no está definida
4. **Línea 5**: `missing ';'` después del bloque `listen-on` — falta `;` al final
5. **Línea 8-10**: `zone 'test.com': missing 'file' entry` — la zona type master requiere `file`

BIND reporta el **primer** error que encuentra y se detiene (en la mayoría de casos). Por eso debes corregir y re-validar iterativamente.

Archivo corregido:

```c
acl "trusted" {
    localhost;
    localnets;
};

options {
    directory "/var/cache/bind";
    recursion yes;
    allow-query { trusted; };
    listen-on { 127.0.0.1; };
};

zone "test.com" {
    type master;
    file "/etc/bind/zones/db.test.com";
};
```

</details>

---

### Ejercicio 2 — Detectar errores con named-checkzone

El siguiente archivo de zona tiene 6 errores. Usa `named-checkzone` y `named-checkzone -o -` para encontrarlos todos:

```
$TTL 86400
$ORIGIN test.lab.

@   IN  SOA  ns1.test.lab admin.test.lab. (
                2026032101
                3600
                900
                604800
                86400

@   IN  NS   ns1.test.lab.

ns1  IN  A       192.168.1.1
www  IN  A       192.168.1.20
www  IN  CNAME   cdn.test.lab.
ftp  IN  A       192.168.1.300

@    IN  MX  10  mail-alias
mail-alias  IN  CNAME  mail.proveedor.com.
```

<details>
<summary>Solución</summary>

Los 6 errores:

1. **SOA MNAME sin punto final**: `ns1.test.lab` → `ns1.test.lab.`
   Se expande a `ns1.test.lab.test.lab.`

2. **Paréntesis del SOA no cerrado**: falta `)` después de `86400`

3. **CNAME coexistiendo con A**: `www` tiene A y CNAME — prohibido

4. **IP inválida**: `192.168.1.300` — el octeto máximo es 255

5. **MX apunta a un CNAME**: `mail-alias` es un CNAME — MX no debe apuntar a CNAME (RFC 2181)

6. **MX con nombre relativo**: `mail-alias` se expande a `mail-alias.test.lab.` — funciona, pero no tiene registro A propio (es un CNAME), que es el error real del punto 5

Corrección:

```
$TTL 86400
$ORIGIN test.lab.

@   IN  SOA  ns1.test.lab. admin.test.lab. (
                2026032101
                3600
                900
                604800
                86400 )

@   IN  NS   ns1.test.lab.

ns1   IN  A     192.168.1.1
www   IN  A     192.168.1.20
ftp   IN  A     192.168.1.30
mail  IN  A     192.168.1.10

@     IN  MX  10  mail.test.lab.
```

Verificar con salida canónica:

```bash
named-checkzone -o - test.lab db.test.lab
# Todos los nombres deben ser FQDN sin duplicación
```

</details>

---

### Ejercicio 3 — Flujo de validación completo

1. Crea una zona `validate.lab` con SOA, 2 NS, 3 A records, 1 MX y 1 CNAME
2. Introduce un error deliberado (ej. punto final faltante en un NS)
3. Ejecuta `named-checkzone` — ¿lo detecta?
4. Ejecuta `named-checkzone -o -` — ¿lo revela?
5. Ejecuta `named-checkzone -i full` — ¿da información adicional?
6. Corrige el error, valida con las tres opciones y declara la zona en named.conf
7. Ejecuta `named-checkconf -z` para validar todo junto
8. Recarga y verifica con `dig`

<details>
<summary>Solución</summary>

```
; /etc/bind/zones/db.validate.lab (con error intencional)
$TTL 86400
$ORIGIN validate.lab.

@   IN  SOA  ns1.validate.lab. admin.validate.lab. (
                2026032101 3600 900 604800 86400)

@   IN  NS   ns1.validate.lab.
@   IN  NS   ns2.validate.lab    ; ← ERROR: sin punto final

ns1  IN  A    192.168.1.1
ns2  IN  A    192.168.1.2
www  IN  A    192.168.1.20
mail IN  A    192.168.1.10
@    IN  MX   10 mail.validate.lab.
blog IN  CNAME www.validate.lab.
```

```bash
# 3. named-checkzone básico — puede no alertar (el nombre expandido es válido)
named-checkzone validate.lab /etc/bind/zones/db.validate.lab
# Puede decir OK (el error genera un nombre válido pero incorrecto)

# 4. Salida canónica — REVELA el error
named-checkzone -o - validate.lab /etc/bind/zones/db.validate.lab
# validate.lab.  86400  IN  NS  ns2.validate.lab.validate.lab.
#                                   ↑ DUPLICADO — claramente mal

# 5. Con -i full
named-checkzone -i full validate.lab /etc/bind/zones/db.validate.lab
# "NS 'ns2.validate.lab.validate.lab.' has no address records"
# ← detecta que el NS expandido no tiene registro A

# 6. Corregir: ns2.validate.lab → ns2.validate.lab.
# Luego validar con las tres opciones — todo OK

# 7. Declarar en named.conf.local y validar todo
# zone "validate.lab" { type master; file "/etc/bind/zones/db.validate.lab"; };
sudo named-checkconf -z
# zone validate.lab/IN: loaded serial 2026032101

# 8. Recargar y verificar
sudo systemctl reload named
dig @127.0.0.1 www.validate.lab A +short
# 192.168.1.20
dig @127.0.0.1 validate.lab NS +short
# ns1.validate.lab.
# ns2.validate.lab.
```

</details>

---

### Pregunta de reflexión

> Un junior de tu equipo edita una zona DNS, ejecuta `named-checkzone` sin opciones, ve "OK", y recarga named. Al día siguiente descubres que los correos están rebotando porque el MX apunta a un CNAME que a su vez apunta a un servidor externo que cambió de IP. `named-checkzone` sin opciones no alertó sobre esto. ¿Qué opciones de validación habrían prevenido el problema, y qué proceso establecerías para que esto no vuelva a ocurrir?

> **Razonamiento esperado**: `named-checkzone -i full` habría detectado que el MX apunta a un CNAME, lo cual viola RFC 2181 y es una fuente frecuente de problemas (el CNAME añade una indirección que puede romperse si el destino cambia). El proceso a establecer: (1) usar siempre `named-checkzone -i full` en el script de despliegue o como alias, (2) implementar un pre-commit hook en Git que valide las zonas antes de commit, (3) documentar la regla "MX siempre a registro A, nunca a CNAME" en las guías del equipo, y (4) añadir monitorización que verifique periódicamente que los MX de cada dominio resuelven correctamente (un simple `dig MX` seguido de `dig A` del resultado).

---

> **Siguiente tema**: T02 — rndc (control remoto del servidor DNS)
