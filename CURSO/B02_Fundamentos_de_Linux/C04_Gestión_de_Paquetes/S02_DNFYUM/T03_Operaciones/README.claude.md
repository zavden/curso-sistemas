# T03 — Operaciones DNF

## Errata respecto a las fuentes originales

1. **Texto en frances** (README.max.md): `dnf upgrade rafraîchit
   automatiquement les métadonnées` — deberia estar en espanol.
2. **Comando duplicado** (README.max.md): `dnf remove
   --setopt=clean_requirements_on_remove=False` aparece dos veces con
   descripciones distintas.
3. **`--setopt=assumeno_qualified_be_name=True`** (README.max.md):
   opcion inventada. Para simulacion, usar `--assumeno`.
4. **`apt full-upgrade` ↔ `dnf upgrade`** (README.max.md): la
   comparacion es incompleta. `dnf upgrade` siempre se comporta como
   `apt full-upgrade` (puede instalar nuevas deps y eliminar paquetes
   conflictivos). No existe un modo "conservador" equivalente a
   `apt upgrade` en dnf.

---

## Actualizar indices y paquetes

```bash
# Ver si hay actualizaciones (sin instalar)
dnf check-update
# Exit code 0 = todo al dia
# Exit code 100 = hay actualizaciones pendientes

# Actualizar TODOS los paquetes
sudo dnf upgrade
# Equivalente: dnf update (alias)
# Refresca metadatos automaticamente antes de actualizar

# Actualizar un paquete especifico
sudo dnf upgrade nginx
```

**Diferencia critica con APT**: dnf **no necesita** un "update" previo.
Refresca metadatos automaticamente cuando es necesario (cache con
`metadata_expire`).

```
APT:  apt update (refresca indices) + apt upgrade (actualiza paquetes)
DNF:  dnf upgrade (hace ambos en un solo paso)
```

Ademas, `dnf upgrade` siempre se comporta como `apt full-upgrade` —
puede instalar paquetes nuevos y eliminar existentes para resolver
dependencias. No hay un modo conservador equivalente a `apt upgrade`.

---

## Instalar paquetes

```bash
# Instalacion basica
sudo dnf install nginx

# Multiples paquetes
sudo dnf install nginx curl htop

# Responder yes automaticamente
sudo dnf install -y nginx

# Sin weak dependencies (como --no-install-recommends en apt)
sudo dnf install --setopt=install_weak_deps=False nginx

# Instalar .rpm local (dnf resuelve deps desde repos)
sudo dnf install ./paquete.rpm
# El ./ es obligatorio: sin el, busca en repos por nombre

# Reinstalar
sudo dnf reinstall nginx

# Downgrade (volver a version anterior)
sudo dnf downgrade nginx
```

### Que ocurre durante dnf install

```
1. RESOLUCION DE DEPENDENCIAS (libsolv)
   Analiza el grafo de dependencias como problema SAT
   Determina que instalar, actualizar o eliminar

2. PLAN DE TRANSACCION
   Muestra:
   - [Install]              paquetes nuevos
   - [Install dependency]   dependencias nuevas
   - [Upgrade]              paquetes actualizados
   - [Remove]               paquetes eliminados por conflicto

3. CONFIRMACION (si no es -y)

4. DESCARGA
   .rpm → /var/cache/dnf/

5. INSTALACION con rpm
   rpm instala en orden correcto (deps primero)

6. SCRIPTS POST-TRANSACCION
   Scripts postinst de cada paquete
```

---

## Desinstalar paquetes

```bash
# Desinstalar
sudo dnf remove nginx
# Si clean_requirements_on_remove=True (default en RHEL 9):
# tambien elimina dependencias huerfanas automaticamente

# Sin eliminar dependencias huerfanas
sudo dnf remove --setopt=clean_requirements_on_remove=False nginx

# Eliminar dependencias huerfanas explicitamente
sudo dnf autoremove

# Ver que eliminaria autoremove (sin hacerlo)
dnf autoremove --assumeno
```

**Diferencia con APT**: en Debian, `apt remove` nunca elimina deps
huerfanas (necesitas `apt autoremove`). En RHEL con
`clean_requirements_on_remove=True`, `dnf remove` las elimina
inmediatamente.

**Nota**: RPM no distingue entre "remove" y "purge" como dpkg. `dnf
remove` elimina todo (binarios + configuracion). No existe equivalente
de `apt purge` vs `apt remove`.

---

## Buscar y consultar

```bash
# Buscar por nombre o summary
dnf search web server

# Buscar en todo (nombre + summary + description)
dnf search all nginx

# Informacion detallada
dnf info nginx
# Name         : nginx
# Version      : 1.22.1
# Release      : 4.module+el9+...
# Architecture : x86_64
# Size         : 614 k
# Repository   : appstream
# Summary      : A high performance web server
```

### Quien provee este archivo (dnf provides)

```bash
# Buscar que paquete instala un archivo (incluso no instalado)
dnf provides /usr/sbin/nginx
# nginx-1:1.22.1-4... : A high performance web server
# Matched from: Filename : /usr/sbin/nginx

# Patrones glob
dnf provides '*/bin/curl'
dnf provides 'libssl.so*'

# Equivalente en APT: apt-file search (requiere apt-file instalado)
# En dnf viene integrado
```

### Listar paquetes

```bash
# Todos los instalados
dnf list installed

# Actualizaciones disponibles
dnf list updates

# Paquetes de un repo especifico
dnf list --repo=epel

# Patrones
dnf list 'python3*'
dnf list installed 'nginx*'
```

---

## Group install — Grupos de paquetes

Los grupos son colecciones de paquetes relacionados:

```bash
# Listar grupos disponibles
dnf group list
# Available Environment Groups:
#   Server with GUI
#   Server
#   Minimal Install
#   Workstation
# Available Groups:
#   Development Tools
#   System Tools
#   Network Servers
```

```bash
# Informacion de un grupo
dnf group info "Development Tools"
# Mandatory Packages:   se instalan siempre
#   autoconf, automake, gcc, gcc-c++, make, ...
# Default Packages:     se instalan por defecto
# Optional Packages:    solo con --with-optional
#   cmake, git, ...

# Instalar un grupo
sudo dnf group install "Development Tools"

# Alias (compatibilidad con yum)
sudo dnf groupinstall "Development Tools"

# Incluir paquetes opcionales
sudo dnf group install --with-optional "Development Tools"

# Eliminar un grupo
sudo dnf group remove "Development Tools"

# Ver grupos instalados
dnf group list --installed
```

---

## Module streams — Multiples versiones

Los modulos (RHEL 8+) permiten tener multiples versiones del mismo
software en el mismo repositorio:

```
+-------------------------------------------------------------+
|                    Module: postgresql                        |
+-------------------------------------------------------------+
|  Stream 15 [d] (default)                                    |
|  +-- postgresql-server-15.x                                 |
|  +-- Profiles: client, server                               |
+-------------------------------------------------------------+
|  Stream 16                                                  |
|  +-- postgresql-server-16.x                                 |
|  +-- Profiles: client, server                               |
+-------------------------------------------------------------+
```

```bash
# Listar modulos disponibles
dnf module list

# Listar streams de un modulo
dnf module list postgresql
# Name          Stream    Profiles           Summary
# postgresql    15 [d]   client, server     PostgreSQL server
# postgresql    16       client, server     PostgreSQL server
# [d] = default, [e] = enabled

# Habilitar un stream
sudo dnf module enable postgresql:16

# Instalar con stream y perfil
sudo dnf module install postgresql:16/server

# Ver modulos habilitados
dnf module list --enabled

# Cambiar de stream (requiere reset primero)
sudo dnf module reset postgresql
sudo dnf module enable postgresql:15

# Deshabilitar un modulo
sudo dnf module disable postgresql
```

### Modulos y filtrado de paquetes

```
Cuando el modulo postgresql:15 esta habilitado:
  → Paquetes de postgresql 16 estan OCULTOS
  → dnf install postgresql NO muestra la version 16

Si un paquete de EPEL es filtrado por un modulo:
  → dnf install paquete → "No match for argument"
  → Soluciones:
    1. dnf module disable modulo
    2. module_hotfixes=1 en el .repo
```

---

## Historial de transacciones

dnf mantiene un historial completo de todas las operaciones (APT no
tiene equivalente nativo):

```bash
# Ver historial
dnf history
# ID   | Command line             | Date and time      | Action(s) | Altered
# 20   | install nginx            | 2024-03-19 10:30   | Install   | 3
# 19   | upgrade                  | 2024-03-18 09:00   | Upgrade   | 12
# 18   | remove httpd             | 2024-03-10 11:20   | Remove    | 5

# Detalle de una transaccion
dnf history info 20
# Transaction ID : 20
# User           : root
# Return-Code    : Success
# Packages Altered:
#   Install nginx-1:1.22.1-4.module+el9.x86_64
#   Install nginx-core-1:1.22.1-4.module+el9.x86_64

# Ultima transaccion
dnf history info last

# DESHACER una transaccion (rollback)
sudo dnf history undo 20
# Desinstala lo que se instalo en TX 20
# Reinstala lo que se elimino en TX 20

# REHACER una transaccion
sudo dnf history redo 20

# Paquetes instalados manualmente (no como deps)
dnf history userinstalled

# Historial de un paquete especifico
dnf history list nginx
```

---

## Cache y limpieza

```bash
# Limpiar toda la cache (metadatos + paquetes)
sudo dnf clean all

# Limpiar solo metadatos
sudo dnf clean metadata

# Limpiar solo .rpm descargados
sudo dnf clean packages

# Regenerar cache (equivalente a apt update)
sudo dnf makecache

# Ver tamano de la cache
du -sh /var/cache/dnf/
```

---

## Actualizaciones de seguridad

```bash
# Ver si hay updates de seguridad pendientes
dnf check-update --security
# Exit code: 0 = sin updates, 100 = hay updates

# Instalar SOLO actualizaciones de seguridad
sudo dnf upgrade --security

# Listar advisories (CVEs, bugfixes)
dnf updateinfo list
dnf updateinfo list security

# Informacion de un advisory especifico
dnf updateinfo info RHSA-2024:1234
```

---

## Instalacion no interactiva (scripts, CI)

```bash
# Flags para automatizacion
sudo dnf install -y nginx          # responder yes
sudo dnf install -y -q nginx       # quiet (menos output)

# Sin weak dependencies
sudo dnf install -y --setopt=install_weak_deps=False nginx

# Simular (ver que haria sin ejecutar)
dnf install --assumeno nginx
```

---

## Solucion de problemas

### "No match for argument"

```bash
# Causa 1: paquete no existe
dnf search nombre-paquete

# Causa 2: filtrado por un modulo
dnf module list | grep nombre
sudo dnf module reset nombre

# Causa 3: repo deshabilitado
dnf install --enablerepo=epel nombre-paquete
```

### "Package has unsatisfied dependency"

```bash
# Conflicto de dependencias

# Ver que repos tienen el paquete
dnf repoquery paquete --repo=baseos --repo=appstream

# Permitir eliminar paquetes conflictivos
sudo dnf install --allowerasing paquete
```

### "Failed to download metadata"

```bash
# Deshabilitar repo problematico temporalmente
sudo dnf install --disablerepo=epel paquete

# Limpiar cache y reintentar
sudo dnf clean all
sudo dnf makecache
```

---

## Comparacion con APT

| Operacion | apt | dnf |
|---|---|---|
| Refrescar indices | `apt update` (manual, obligatorio) | Automatico (cache) |
| Actualizar conservador | `apt upgrade` | No existe equivalente |
| Actualizar completo | `apt full-upgrade` | `dnf upgrade` |
| Instalar | `apt install pkg` | `dnf install pkg` |
| Desinstalar | `apt remove pkg` | `dnf remove pkg` |
| Purgar config | `apt purge pkg` | No aplica (rpm no separa) |
| Autoremove | `apt autoremove` | `dnf autoremove` (o auto con remove) |
| Buscar | `apt search term` | `dnf search term` |
| Info | `apt show pkg` | `dnf info pkg` |
| Archivo → paquete | `dpkg -S` / `apt-file` | `dnf provides` (integrado) |
| Historial + undo | No nativo | `dnf history` + undo/redo |
| Grupos | No nativo | `dnf group install` |
| Multiples versiones | No nativo | `dnf module` |
| Security updates | `apt list --upgradable` | `dnf upgrade --security` |
| Simular | `apt install -s` | `dnf install --assumeno` |

---

## Ejercicios

### Ejercicio 1 — Ciclo de vida de un paquete

Instala, usa, consulta y desinstala un paquete.

```bash
# 1. Buscar un paquete
dnf search tree 2>/dev/null | head -5

# 2. Ver informacion antes de instalar
dnf info tree 2>/dev/null
```

**Pregunta**: Que diferencia hay entre `dnf info` y `rpm -qi` para
obtener informacion de un paquete?

<details><summary>Prediccion</summary>

- `dnf info pkg`: muestra informacion del **repositorio** — funciona
  para paquetes instalados Y no instalados
- `rpm -qi pkg`: muestra informacion de la **base de datos local** —
  solo funciona para paquetes **ya instalados**

```bash
# 3. Instalar
sudo dnf install -y tree

# 4. Usar
tree /etc/dnf/ 2>/dev/null

# 5. Ver archivos instalados
rpm -ql tree

# 6. Desinstalar
sudo dnf remove -y tree

# 7. Verificar que se fue
rpm -q tree 2>/dev/null || echo "tree desinstalado"
```

</details>

---

### Ejercicio 2 — dnf provides vs rpm -qf

Compara las dos formas de buscar que paquete instalo un archivo.

```bash
# 1. Con rpm (solo paquetes instalados)
rpm -qf /usr/bin/env
rpm -qf /usr/bin/curl
```

**Pregunta**: Que ventaja tiene `dnf provides` sobre `rpm -qf`?

<details><summary>Prediccion</summary>

`dnf provides` busca en **todos los repositorios**, incluyendo paquetes
no instalados. `rpm -qf` solo busca en la base de datos local (paquetes
instalados).

Si necesitas saber que paquete instalar para obtener un archivo que
**no tienes**, solo `dnf provides` funciona.

```bash
# 2. Con dnf (busca en repos, incluso no instalados)
dnf provides /usr/bin/env
dnf provides /usr/sbin/nginx   # funciona aunque nginx no este instalado

# 3. Buscar con patrones
dnf provides '*/bin/htpasswd'
# Muestra que paquete instalar para obtener htpasswd

# 4. Buscar librerias
dnf provides 'libssl.so*'

# En APT, esta funcionalidad requiere apt-file (paquete separado).
# En dnf viene integrado.
```

</details>

---

### Ejercicio 3 — Grupos de paquetes

Explora los grupos disponibles y su contenido.

```bash
# 1. Listar grupos
dnf group list 2>/dev/null
```

**Pregunta**: Cual es la diferencia entre "Environment Groups" y "Groups"?

<details><summary>Prediccion</summary>

- **Environment Groups**: definen un perfil completo de instalacion del
  sistema (Server, Workstation, Minimal Install). Incluyen multiples
  Groups dentro.
- **Groups**: colecciones tematicas de paquetes para un proposito
  especifico (Development Tools, Network Servers, System Tools).

```bash
# 2. Ver contenido de un grupo
dnf group info "Development Tools" 2>/dev/null | head -20

# 3. Tipos de paquetes en un grupo
echo "Mandatory: se instalan siempre"
echo "Default:   se instalan por defecto"
echo "Optional:  solo con --with-optional"

# 4. Ver grupos instalados
dnf group list --installed 2>/dev/null

# 5. Simular instalacion de un grupo
sudo dnf group install --assumeno "System Tools" 2>/dev/null
```

</details>

---

### Ejercicio 4 — Module streams

Explora el sistema de modulos de AppStream.

```bash
# 1. Listar modulos disponibles
dnf module list 2>/dev/null | head -20
```

**Pregunta**: Si el modulo `postgresql:15` esta habilitado y quieres
instalar postgresql 16, que pasos necesitas?

<details><summary>Prediccion</summary>

Necesitas **resetear** el modulo primero, luego habilitar el nuevo stream:

1. `sudo dnf module reset postgresql` — vuelve al estado sin stream
   habilitado
2. `sudo dnf module enable postgresql:16` — habilita stream 16
3. `sudo dnf module install postgresql:16/server` — instala

No puedes habilitar directamente 16 sin resetear 15 primero — dnf lo
impide para evitar conflictos.

```bash
# 2. Ver modulos habilitados
dnf module list --enabled 2>/dev/null

# 3. Ver streams disponibles
dnf module list nodejs 2>/dev/null
dnf module list php 2>/dev/null

# 4. Ver que pasa cuando un modulo filtra paquetes
echo "Si un modulo esta habilitado, otras versiones quedan OCULTAS"
echo "Soluciones:"
echo "  1. dnf module disable modulo"
echo "  2. module_hotfixes=1 en el .repo"
```

</details>

---

### Ejercicio 5 — Historial de transacciones

Usa `dnf history` para auditar y revertir operaciones.

```bash
# 1. Ver historial
dnf history 2>/dev/null | head -15
```

**Pregunta**: Como deshaces una instalacion que salio mal usando
`dnf history`?

<details><summary>Prediccion</summary>

1. Identificar el ID de la transaccion con `dnf history`
2. Ver que hizo: `dnf history info ID`
3. Revertir: `sudo dnf history undo ID`

`undo` deshace exactamente lo que la transaccion hizo:
- Si instalo paquetes → los desinstala
- Si elimino paquetes → los reinstala
- Si actualizo paquetes → hace downgrade

```bash
# 2. Detalle de la ultima transaccion
dnf history info last 2>/dev/null

# 3. Instalar algo para probar
sudo dnf install -y tree 2>/dev/null

# 4. Ver la transaccion nueva
dnf history 2>/dev/null | head -5

# 5. Deshacer la instalacion
TX_ID=$(dnf history 2>/dev/null | grep "install" | head -1 | awk '{print $1}')
echo "Deshaciendo transaccion $TX_ID"
# sudo dnf history undo $TX_ID -y

# APT no tiene esta funcionalidad nativamente
```

</details>

---

### Ejercicio 6 — Actualizaciones de seguridad

Explora las herramientas de seguridad de dnf.

```bash
# 1. Ver actualizaciones de seguridad
dnf check-update --security 2>/dev/null
echo "Exit code: $?"
```

**Pregunta**: Cual es la diferencia entre `dnf upgrade` y
`dnf upgrade --security`?

<details><summary>Prediccion</summary>

- `dnf upgrade`: actualiza **todos** los paquetes a la ultima version
- `dnf upgrade --security`: actualiza **solo** paquetes que tienen
  advisories de seguridad (RHSA)

En servidores de produccion, `--security` es preferible para minimizar
cambios — solo aplica parches criticos sin actualizar software a
versiones nuevas que podrian romper compatibilidad.

```bash
# 2. Listar advisories
dnf updateinfo list 2>/dev/null | head -10

# 3. Tipos de advisories
echo "RHSA = Red Hat Security Advisory (seguridad)"
echo "RHBA = Red Hat Bug Advisory (bugfix)"
echo "RHEA = Red Hat Enhancement Advisory (mejora)"

# 4. Ver detalle de un advisory
# dnf updateinfo info RHSA-2024:XXXX
```

</details>

---

### Ejercicio 7 — check-update y exit codes

Entiende el comportamiento especial de `dnf check-update`.

```bash
# 1. Ejecutar check-update
dnf check-update 2>/dev/null
echo "Exit code: $?"
```

**Pregunta**: Por que `check-update` usa exit code 100 en vez de 0
cuando hay actualizaciones?

<details><summary>Prediccion</summary>

Es una convencion de dnf/yum para uso en scripts:
- **Exit 0**: no hay actualizaciones — el sistema esta al dia
- **Exit 100**: hay actualizaciones pendientes
- **Exit 1**: error

Esto permite usar `check-update` en scripts condicionales:

```bash
# 2. Ejemplo de uso en script
dnf check-update 2>/dev/null
rc=$?
if [ $rc -eq 0 ]; then
    echo "Sistema al dia"
elif [ $rc -eq 100 ]; then
    echo "Hay actualizaciones disponibles"
    # Contar cuantas
    count=$(dnf check-update 2>/dev/null | grep -c "^\S")
    echo "Total: $count paquetes"
else
    echo "Error al verificar"
fi
```

APT no tiene esta convencion — `apt list --upgradable` siempre
retorna 0.

</details>

---

### Ejercicio 8 — Weak dependencies

Compara el concepto de "weak dependencies" con APT.

```bash
# 1. Ver weak dependencies de un paquete
dnf repoquery --recommends curl 2>/dev/null
dnf repoquery --suggests curl 2>/dev/null
```

**Pregunta**: Cual es el equivalente de `--no-install-recommends` en dnf?

<details><summary>Prediccion</summary>

`--setopt=install_weak_deps=False`

En APT, `Recommends` se instalan por defecto y `Suggests` no. El flag
`--no-install-recommends` omite los Recommends.

En DNF, las "weak dependencies" (Recommends y Supplements) se instalan
por defecto. `install_weak_deps=False` las omite.

```bash
# 2. Comparar instalacion con y sin weak deps
echo "Con weak deps (default):"
dnf install --assumeno nginx 2>/dev/null | grep "Install" | tail -3

echo "Sin weak deps:"
dnf install --assumeno --setopt=install_weak_deps=False nginx 2>/dev/null | grep "Install" | tail -3

# 3. Configurar permanentemente (en contenedores)
echo "En /etc/dnf/dnf.conf:"
echo "  install_weak_deps=False"
```

En contenedores y servidores minimos, deshabilitar weak deps ahorra
espacio (como `--no-install-recommends` en Dockerfiles Debian).

</details>

---

### Ejercicio 9 — Solucion de problemas

Diagnostica errores comunes de dnf.

```bash
# 1. Simular "No match for argument"
dnf install paquete-inexistente 2>&1 | head -3
```

**Pregunta**: Si `dnf install paquete` dice "No match", cuales son las
3 causas mas comunes?

<details><summary>Prediccion</summary>

1. **El paquete no existe** en ningun repo habilitado — verificar con
   `dnf search nombre`
2. **Un modulo lo filtra** — si el modulo del software esta habilitado
   en otro stream, oculta versiones alternativas. Solucionar con
   `dnf module reset` o `module_hotfixes=1`
3. **El repo esta deshabilitado** — el paquete esta en un repo que no
   esta activo (ej: EPEL, CRB). Usar `--enablerepo=nombre` o
   habilitarlo permanentemente

```bash
# 2. Verificar causa 1
dnf search paquete-inexistente 2>/dev/null

# 3. Verificar causa 2
dnf module list --enabled 2>/dev/null

# 4. Verificar causa 3
dnf repolist --all 2>/dev/null | head -10

# 5. Resolver con --allowerasing (conflictos)
echo "Si hay conflicto de dependencias:"
echo "  sudo dnf install --allowerasing paquete"
echo "  Permite eliminar paquetes conflictivos"
```

</details>

---

### Ejercicio 10 — Panorama: operaciones DNF completas

Construye el mapa mental de todas las operaciones.

```bash
cat <<'EOF'
OPERACIONES DNF:

  CONSULTA:
    dnf search term           buscar paquetes
    dnf info pkg              informacion detallada
    dnf provides /path        que paquete tiene este archivo
    dnf list installed        paquetes instalados
    dnf check-update          actualizaciones disponibles (exit 100)
    dnf repoquery             consultas avanzadas

  INSTALACION:
    dnf install pkg           instalar
    dnf install ./file.rpm    instalar .rpm local con deps
    dnf reinstall pkg         reinstalar
    dnf downgrade pkg         volver a version anterior
    dnf group install "name"  instalar grupo
    dnf module install m:s    instalar module stream

  DESINSTALACION:
    dnf remove pkg            desinstalar (+ deps huerfanas si config)
    dnf autoremove            limpiar deps huerfanas
    dnf group remove "name"   desinstalar grupo

  ACTUALIZACION:
    dnf upgrade               actualizar todos (= apt full-upgrade)
    dnf upgrade --security    solo updates de seguridad
    dnf upgrade pkg           actualizar uno

  HISTORIAL:
    dnf history               ver transacciones
    dnf history info N        detalle de transaccion N
    dnf history undo N        revertir transaccion N
    dnf history redo N        repetir transaccion N

  CACHE:
    dnf clean all             limpiar cache
    dnf makecache             regenerar cache
EOF
```

**Pregunta**: Que funcionalidades exclusivas tiene dnf que APT no ofrece?

<details><summary>Prediccion</summary>

Funcionalidades exclusivas de dnf:

1. **`dnf history undo`** — revertir una transaccion completa
2. **`dnf group install`** — instalar grupos predefinidos de paquetes
3. **`dnf module`** — multiples versiones via module streams
4. **`dnf provides`** — buscar que paquete tiene un archivo, integrado
   (APT necesita `apt-file` separado)
5. **`dnf upgrade --security`** — actualizar solo parches de seguridad
6. **Metadata automatico** — no necesita "update" previo
7. **`dnf check-update`** — exit code 100 para scripting
8. **`dnf downgrade`** — volver a version anterior facilmente
9. **`dnf updateinfo`** — consultar advisories de seguridad (RHSA/RHBA)

APT tiene sobre dnf:
- `apt purge` vs `apt remove` (separar binarios de config)
- Formato mas conciso de repos (one-line)
- `apt-mark hold/unhold` mas intuitivo que `dnf versionlock`

</details>
