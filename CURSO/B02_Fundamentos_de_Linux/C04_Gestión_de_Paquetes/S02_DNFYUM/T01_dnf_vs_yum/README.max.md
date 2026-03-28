# T01 — dnf vs yum

## Evolución de la familia RPM

```
┌──────────────────────────────────────────────────────────────┐
│                        rpm (1997)                             │
│   Gestor de bajo nivel: instala .rpm individuales             │
│   No resuelve dependencias (como dpkg)                        │
├──────────────────────────────────────────────────────────────┤
│                        yum (2003)                              │
│   Gestor de alto nivel: resuelve deps, repositorios           │
│   Python 2, API de plugins propia                            │
│   RHEL 5/6/7, CentOS 5/6/7                                   │
├──────────────────────────────────────────────────────────────┤
│                        dnf (2015)                              │
│   Reemplazo de yum: misma interfaz, mejor rendimiento        │
│   Python 3, libsolv para deps, API de plugins DNF            │
│   Fedora 22+, RHEL 8+, AlmaLinux 8+, Rocky 8+                │
├──────────────────────────────────────────────────────────────┤
│                     dnf5 (2024+)                              │
│   Reescritura en C++ para máximo rendimiento                 │
│   Fedora 39+, disponible en RHEL 9+                          │
└──────────────────────────────────────────────────────────────┘
```

## Por qué dnf reemplazó a yum

### Problemas de yum

```bash
# 1. Resolución de dependencias inconsistente
# En conflictos complejos, yum podía:
# - Reportar "no solution" cuando sí existía una
# - Instalar versiones incorrectas de dependencias
# - Dejar el sistema en estado inconsistente

# 2. Performance可怜的 con muchos repositorios
# yum descargaba metadata completo cada vez
# dnf descarga solo cambios (deltarpm)

# 3. Python 2 EOL (End of Life)
# yum basado en Python 2 → security risk
```

### Qué mejoró con dnf

```bash
# 1. libsolv (mismo solver que zypper en SUSE)
# Garantía: si existe una solución, la encuentra
# Si no existe, lo reporta correctamente

# 2. Rendimiento
# Delta RPMs: descarga solo diferencias de metadata
# Downloads paralelas: multiple packages simultaneously
# Cache inteligente: no re-descarga lo mismo

# 3. API de plugins estandarizada
# Todos los plugins usan la misma API
```

## yum → dnf: la transición

### RHEL 7 y anteriores

```bash
# yum es el gestor nativo
yum install nginx
yum update
yum search nginx
yum repolist

# yum.conf
cat /etc/yum.conf
# [main]
# gpgcheck=1
# cachedir=/var/cache/yum
```

### RHEL 8+ (yum es alias de dnf)

```bash
# dnf es el binario nativo
dnf install nginx
dnf update
dnf search nginx

# yum es UN SYMLINK a dnf
ls -la /usr/bin/yum
# lrwxrwxrwx 1 root root 3 Jun  1 00:00 /usr/bin/yum -> dnf-3

# Ejecutar "yum" → ejecuta "dnf-3"
# Internamente idéntico

# Ambos producen resultados IGUALES
yum install nginx    # → llama a dnf-3
dnf install nginx     # → llama directo
```

### RHEL 9 / Fedora 39+

```bash
# Fedora 39+ introduce dnf5 (dnf5, escrito en C++)
dnf5 --version
# dnf-5.2.1

# RHEL 9 usa dnf 4 (Python)
dnf --version
# dnf-4.14.0

# yum sigue siendo symlink en todos lados
```

## Diferencias técnicas yum vs dnf

| Aspecto | `yum` (legacy) | `dnf` |
|---|---|---|
| Lenguaje | Python 2 | Python 3 (dnf5: C++) |
| Resolver de deps | Propio (lento, impreciso) | libsolv (SAT solver, rápido) |
| Metadata repos | Completo por repos | Delta RPMs (solo cambios) |
| Transacciones | A veces inconsistentes | Robustas y predecibles |
| Autoremove | Limitado (RHEL 7.2+) | Completo |
| Módulos/streams | No | Sí (RHEL 8+) |
| Plugins | yum-specific | DNF Plugin API |
| Rendimiento | Lento con muchos repos | 2-3x más rápido |
| `update` vs `upgrade` | `update` es oficial | `upgrade` es oficial, `update` alias |

### Cómo funciona libsolv

```
Problema: Instalar paquete A que depende de B y C
          B depende de D >= 2.0
          C depende de D >= 1.5
          
libsolv analiza:
  - Todas las restricciones
  - Encuentra la versión de D que satisface TODOS
  - Si existe: propone la solución
  - Si no existe: error preciso ("necesitas D >= 2.0 pero C requiere D < 2.0")

yum podía fallar en este caso → libsolv siempre encuentra la mejor solución
```

## Comandos equivalentes yum ↔ dnf

| yum | dnf | Descripción |
|---|---|---|
| `yum install pkg` | `dnf install pkg` | Instalar paquete(s) |
| `yum remove pkg` | `dnf remove pkg` | Desinstalar |
| `yum update` | `dnf upgrade` | Actualizar todos los paquetes |
| `yum update pkg` | `dnf upgrade pkg` | Actualizar un paquete específico |
| `yum check-update` | `dnf check-update` | Verificar actualizaciones disponibles |
| `yum search term` | `dnf search term` | Buscar por nombre/descripción |
| `yum info pkg` | `dnf info pkg` | Mostrar información del paquete |
| `yum list installed` | `dnf list installed` | Listar instalados |
| `yum list available` | `dnf list available` | Listar disponibles |
| `yum provides file` | `dnf provides file` | ¿Qué paquete provee este archivo? |
| `yum repolist` | `dnf repolist` | Listar repositorios activos |
| `yum repolist all` | `dnf repolist --all` | Listar TODOS los repos (activos e inactivos) |
| `yum clean all` | `dnf clean all` | Limpiar caché |
| `yum history` | `dnf history` | Ver historial de transacciones |
| `yum group list` | `dnf group list` | Listar grupos de paquetes |
| `yum groupinstall "name"` | `dnf group install "name"` | Instalar grupo |
| `yum localinstall pkg.rpm` | `dnf install pkg.rpm` | Instalar .rpm local |
| `yum deplist pkg` | `dnf repoquery --deplist` | Ver dependencias de un paquete |

### Diferencias sutiles entre yum y dnf

```bash
# 1. "update" vs "upgrade"
yum update              # oficial en yum
dnf update              # funciona (alias), pero "dnf upgrade" es el oficial

# 2. groupinstall vs group install
yum groupinstall "Development Tools"   # un sola palabra
dnf group install "Development Tools"   # dos palabras
dnf groupinstall "Development Tools"   # también funciona (alias)

# 3. autoremove
yum autoremove         # solo RHEL 7.2+ con yum-plugin-auto-update
dnf autoremove         # completo, como apt autoremove

# 4. repoquery (consultas avanzadas)
yum resolvedep python3          # qué paquete provee python3
dnf repoquery --provides python3   # más detallado

# 5. download
yumdownloader pkg             # (paquete yum-utils) descarga .rpm
dnf download pkg              # nativo en dnf
```

## Comandos exclusivos de dnf

```bash
# Descargar sin instalar (muy útil)
dnf download nginx
# Guarda nginx-1.22.1-9.el9.x86_64.rpm en directorio actual

dnf download --resolve nginx
# También descarga las dependencias

# Consultar qué provee un archivo (incluye archivos NO instalados)
dnf repoquery --provides /usr/sbin/nginx

# Verificar qué paquete instalaría un archivo
dnf whatprovides /usr/bin/python3

# Listar todos los archivos de un paquete (sin instalar)
dnf repoquery -l nginx

# Historial de un paquete específico
dnf history info nginx
dnf history userinstalled | grep nginx
```

## Comandos exclusivos de yum

```bash
# Yum solo (no tienen equivalente directo en dnf):
yum-utils
├── yumdownloader pkg          # descargar .rpm
├── yum-complete-transaction   # completar transacción incompleta
├── package-cleanup --oldkernels  # limpiar kernels antiguos
└── yum-debug-dump             # dump para debug

# yum-builddep (instalar dependencias de compilación)
yum-builddep kernel-devel
# dnf tiene: dnf builddep kernel-devel (dnf-plugins-core)
```

## Configuración de dnf

### dnf.conf

```bash
cat /etc/dnf/dnf.conf
```

```ini
[main]
gpgcheck=1                             # siempre 1 (verificar firmas)
installonly_limit=3                    # máximo kernels instalados
clean_requirements_on_remove=True      # autoremove al desinstalar
best=True                              # instalar mejor versión o fallar
skip_if_unavailable=False              # fallar si repo no responde
max_parallel_downloads=3               # descargas paralelas
defaultyes=False                       # esperar confirmación
keepcache=0                             # no guardar .rpm descargados
```

### Opciones explicadas

| Opción | Valor por defecto | Efecto |
|---|---|---|
| `gpgcheck` | 1 | Verificar firmas GPG (no cambiar a 0) |
| `installonly_limit` | 3 | Kernels simultáneos (2 = actuales + 1 anterior) |
| `clean_requirements_on_remove` | True | Eliminar deps huérfanas con `dnf remove` |
| `best` | True | Instalar la versión disponible más nueva o fallar |
| `skip_if_unavailable` | False | Si un repo falla, no instalar de él |
| `max_parallel_downloads` | 3 | Conexiones simultáneas (subir si带宽 es alta) |
| `defaultyes` | False | Si True, Enter acepta sin preguntar |
| `keepcache` | 0 | 0=no guardar .rpm (ahorra espacio), 1=guardar |

```bash
# Aumentar descargas paralelas para conexiones rápidas
echo 'max_parallel_downloads=10' | sudo tee -a /etc/dnf/dnf.conf

# Deshabilitar installonly (no recomendado)
# echo 'installonly_limit=2' | sudo tee -a /etc/dnf/dnf.conf
```

### Repositorios en dnf

```bash
# Ver repositorios configurados
dnf repolist
dnf repolist --all        # incluir deshabilitados

# Habilitar/deshabilitar un repo
dnf config-manager --enable rhel-9-for-x86_64-baseos-rpms
dnf config-manager --disable epel

# Agregar un repo desde archivo .repo
dnf config-manager --add-repo https://example.com.repo

# Ver configuración de un repo
dnf repolist -v epel | grep baseurl
```

---

## Ejercicios

### Ejercicio 1 — Verificar entorno

```bash
# 1. ¿Qué versión de dnf tienes?
dnf --version

# 2. ¿yum es un symlink?
ls -la /usr/bin/yum
readlink -f /usr/bin/yum

# 3. ¿Qué distribución estás usando?
cat /etc/redhat-release

# 4. ¿Qué versión de kernel tienes?
uname -r

# 5. ¿Cuál es el binario real de yum?
file /usr/bin/yum
```

### Ejercicio 2 — Comparar salida de yum vs dnf

```bash
# Los comandos SON IDENTICOS en RHEL 8+
# Porque yum es un symlink a dnf

# Ejecutar ambos y verificar que producen igual output
yum repolist 2>&1 | tee /tmp/yum_repolist.txt
dnf repolist 2>&1 | tee /tmp/dnf_repolist.txt
diff /tmp/yum_repolist.txt /tmp/dnf_repolist.txt
# Debe estar vacío (son idénticos)

# Comparar info de un paquete
yum info bash > /tmp/yum_info.txt
dnf info bash > /tmp/dnf_info.txt
diff /tmp/yum_info.txt /tmp/dnf_info.txt
```

### Ejercicio 3 — Explorar dnf.conf

```bash
# 1. Ver configuración actual
cat /etc/dnf/dnf.conf

# 2. ¿Cuántos kernels permitir?
grep installonly_limit /etc/dnf/dnf.conf

# 3. ¿Está habilitado gpgcheck?
grep gpgcheck /etc/dnf/dnf.conf

# 4. ¿Cuántas descargas paralelas?
grep max_parallel_downloads /etc/dnf/dnf.conf || echo "default: 3"

# 5. Agregar una opción de mejora (sin romper nada)
# Imaginá que querés 5 descargas paralelas
# ¿Qué archivo editás?

# 6. ¿Cuántos repos tenés?
dnf repolist | tail -n +2 | wc -l
```

### Ejercicio 4 — Consultas de paquetes

```bash
# 1. ¿Cuántos paquetes hay instalados?
dnf list installed | wc -l

# 2. ¿Cuántos paquetes están disponibles?
dnf list available | wc -l

# 3. Buscar un paquete
dnf search "web server"

# 4. Ver información de httpd
dnf info httpd

# 5. ¿Qué paquete provee /usr/sbin/useradd?
dnf provides /usr/sbin/useradd

# 6. Ver las dependencias de un paquete
dnf repoquery --deplist httpd | head -20
```

### Ejercicio 5 — Descargar sin instalar

```bash
# 1. Crear directorio de prueba
mkdir /tmp/rpm-test && cd /tmp/rpm-test

# 2. Descargar un paquete (sin instalar)
dnf download nginx

# 3. Ver el archivo descargado
ls -lh *.rpm

# 4. Inspeccionar el .rpm sin instalar
rpm -qip nginx-*.rpm   # info del paquete
rpm -qlp nginx-*.rpm   # lista de archivos

# 5. Descargar con sus dependencias
dnf download --resolve nginx

# 6. Limpiar
cd ~ && rm -rf /tmp/rpm-test
```

### Ejercicio 6 — Simular operaciones

```bash
# 1. Simular instalación de un paquete
dnf install --assumeno httpd

# ¿Qué haría dnf? (sin ejecutar)

# 2. Simular remoción
dnf remove --assumeno curl

# 3. Ver el historial de transacciones
dnf history
dnf history info 5    # ver detalle de transacción 5

# 4. ¿Cuántas transacciones tenés?
dnf history | grep -E "^[[:space:]]+[0-9]" | tail -5
```
