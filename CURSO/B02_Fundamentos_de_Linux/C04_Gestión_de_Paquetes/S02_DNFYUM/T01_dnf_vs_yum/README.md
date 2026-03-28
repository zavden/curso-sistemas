# T01 — dnf vs yum

## Evolución del gestor de paquetes en RHEL

La familia Red Hat ha usado tres gestores de paquetes a lo largo del tiempo:

```
rpm (1997)     Nivel bajo: instala/desinstala paquetes .rpm individuales
                No resuelve dependencias (como dpkg en Debian)

yum (2003)     Nivel alto: resuelve dependencias, descarga de repositorios
                Escrito en Python 2
                Usado en: RHEL 5, 6, 7 / CentOS 5, 6, 7

dnf (2015)     Reemplazo de yum: misma funcionalidad, mejor rendimiento
                Escrito en Python 3, usa libsolv para resolver dependencias
                Usado en: Fedora 22+, RHEL 8+, AlmaLinux 8+, Rocky 8+
```

## La transición yum → dnf

### RHEL 7 y anteriores: yum

```bash
# yum es el gestor nativo
yum install nginx
yum update
yum search nginx
```

### RHEL 8: dnf con compatibilidad yum

```bash
# dnf es el gestor nativo
dnf install nginx

# yum sigue funcionando — es un symlink a dnf
ls -la /usr/bin/yum
# lrwxrwxrwx 1 root root 5 ... /usr/bin/yum -> dnf-3

# Ambos comandos hacen exactamente lo mismo
yum install nginx    # internamente ejecuta dnf
dnf install nginx    # nativo
```

### RHEL 9 / AlmaLinux 9: dnf5

```bash
# RHEL 9 usa dnf (versión 4)
# Fedora 39+ introduce dnf5 (reescritura en C++, mucho más rápido)

# En RHEL 9:
dnf install nginx    # dnf 4
yum install nginx    # sigue siendo symlink a dnf

# yum se mantiene como alias por compatibilidad indefinidamente
# Pero todos los docs nuevos usan dnf
```

## Diferencias técnicas

| Aspecto | yum (legacy) | dnf |
|---|---|---|
| Lenguaje | Python 2 | Python 3 (dnf5: C++) |
| Resolver de dependencias | Propio (lento, a veces inconsistente) | libsolv (rápido, correcto) |
| API de plugins | yum-specific | DNF Plugin API |
| Rendimiento | Lento con muchos repos | Significativamente más rápido |
| Manejo de metadata | Descarga completa | Descarga incremental (deltarpm) |
| Transacciones | A veces inconsistentes | Más robustas y predecibles |
| Autoremove | Limitado | Completo (como apt autoremove) |
| Módulos (streams) | No | Sí (RHEL 8+) |

### Resolución de dependencias

La diferencia más importante es el resolver:

```bash
# yum usaba un resolver propio que podía fallar con conflictos complejos:
# "Error: Package: foo requires bar >= 2.0, but bar-1.5 is available"
# A veces no encontraba solución cuando sí existía una

# dnf usa libsolv (la misma biblioteca que usa zypper en SUSE)
# libsolv es un solver SAT — garantiza encontrar la solución
# si existe, o reportar correctamente que no hay solución posible
```

## Comandos equivalentes yum → dnf

Casi todos los comandos son idénticos:

| yum | dnf | Descripción |
|---|---|---|
| `yum install pkg` | `dnf install pkg` | Instalar |
| `yum remove pkg` | `dnf remove pkg` | Desinstalar |
| `yum update` | `dnf upgrade` | Actualizar todos |
| `yum update pkg` | `dnf upgrade pkg` | Actualizar uno |
| `yum search term` | `dnf search term` | Buscar |
| `yum info pkg` | `dnf info pkg` | Información |
| `yum list installed` | `dnf list installed` | Listar instalados |
| `yum list available` | `dnf list available` | Listar disponibles |
| `yum provides file` | `dnf provides file` | ¿Quién provee este archivo? |
| `yum repolist` | `dnf repolist` | Listar repos |
| `yum clean all` | `dnf clean all` | Limpiar caché |
| `yum history` | `dnf history` | Historial de transacciones |
| `yum groupinstall` | `dnf group install` | Instalar grupo (nota el espacio) |

### Diferencias sutiles

```bash
# "update" vs "upgrade"
# En yum: "update" actualiza paquetes
# En dnf: "update" es alias de "upgrade" (ambos funcionan)
#         pero "upgrade" es el nombre oficial

# "groupinstall" vs "group install"
# yum: yum groupinstall "Development Tools"
# dnf: dnf group install "Development Tools"  (con espacio)
#      dnf groupinstall "Development Tools"   (también funciona como alias)

# autoremove
# yum: yum autoremove (solo RHEL 7.2+, limitado)
# dnf: dnf autoremove (completo, como apt autoremove)
```

## Cuándo usar cada uno

```
En sistemas RHEL 8+, AlmaLinux 8+, Rocky 8+, Fedora:
  → dnf (siempre)
  yum funciona pero es un alias — mejor usar el nombre real

En sistemas RHEL 7 / CentOS 7 (legacy):
  → yum (dnf no está disponible por defecto)

En scripts que deben funcionar en RHEL 7 y 8+:
  → yum (funciona en ambos porque en 8+ es alias de dnf)
  O detectar la versión y usar el correcto

En este curso:
  → dnf (RHEL 9 / AlmaLinux 9 es la versión actual)
```

## Configuración de dnf

```bash
# Archivo principal de configuración
cat /etc/dnf/dnf.conf
# [main]
# gpgcheck=1          ← verificar firmas GPG (siempre 1)
# installonly_limit=3  ← máximo de kernels instalados simultáneamente
# clean_requirements_on_remove=True  ← autoremove al hacer remove
# best=True           ← preferir la mejor versión disponible
# skip_if_unavailable=False

# Plugins
ls /etc/dnf/plugins/
# copr.conf
# subscription-manager.conf  (RHEL)
```

### Opciones útiles de dnf.conf

| Opción | Valor | Efecto |
|---|---|---|
| `gpgcheck` | 1 | Verificar firmas GPG (no cambiar) |
| `installonly_limit` | 3 | Mantener 3 kernels (rollback) |
| `clean_requirements_on_remove` | True | Eliminar dependencias huérfanas al remove |
| `best` | True | Instalar la mejor versión o fallar |
| `skip_if_unavailable` | False | Fallar si un repo no responde |
| `max_parallel_downloads` | 3 | Descargas paralelas (subir para conexiones rápidas) |
| `defaultyes` | False | Respuesta default a confirmaciones |
| `keepcache` | 0 | No conservar .rpm descargados |

```bash
# Acelerar descargas
echo 'max_parallel_downloads=10' | sudo tee -a /etc/dnf/dnf.conf
# En Fedora/RHEL con buena conexión, esto acelera notablemente
```

---

## Ejercicios

### Ejercicio 1 — Verificar la versión

```bash
# ¿Qué versión de dnf tienes?
dnf --version

# ¿yum es un alias?
ls -la /usr/bin/yum
# ¿A dónde apunta?

# ¿Qué versión de RHEL/AlmaLinux tienes?
cat /etc/redhat-release
```

### Ejercicio 2 — Comparar yum y dnf

```bash
# Ejecutar el mismo comando con ambos
yum repolist
dnf repolist
# ¿La salida es idéntica?

yum info bash
dnf info bash
```

### Ejercicio 3 — Configuración

```bash
# Ver la configuración actual
cat /etc/dnf/dnf.conf

# ¿Cuántos kernels se mantienen?
grep installonly_limit /etc/dnf/dnf.conf

# ¿Está habilitado gpgcheck?
grep gpgcheck /etc/dnf/dnf.conf

# ¿Cuántas descargas paralelas?
grep max_parallel_downloads /etc/dnf/dnf.conf || echo "default (3)"
```
