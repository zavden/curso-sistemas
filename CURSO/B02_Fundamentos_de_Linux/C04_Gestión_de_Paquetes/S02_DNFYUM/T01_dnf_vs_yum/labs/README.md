# Lab — dnf vs yum

## Objetivo

Entender la evolucion de yum a dnf, verificar que yum es un symlink
a dnf en sistemas modernos, comparar la salida de ambos comandos,
y explorar la configuracion de dnf.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Evolucion yum a dnf

### Objetivo

Verificar la relacion entre yum y dnf en el sistema.

### Paso 1.1: Version del sistema

```bash
docker compose exec alma-dev bash -c '
echo "=== Sistema operativo ==="
cat /etc/redhat-release
echo ""
echo "=== Jerarquia de herramientas RPM ==="
echo ""
echo "Nivel BAJO:  rpm"
echo "  Instala/desinstala .rpm individuales"
echo "  NO resuelve dependencias"
echo ""
echo "Nivel ALTO:  dnf (reemplazo de yum)"
echo "  Resuelve dependencias con libsolv"
echo "  Descarga de repositorios"
echo "  Historial de transacciones"
echo ""
echo "La relacion rpm↔dnf es la misma que dpkg↔apt en Debian"
'
```

### Paso 1.2: yum es un symlink a dnf

```bash
docker compose exec alma-dev bash -c '
echo "=== Verificar que yum es un alias ==="
echo ""
echo "--- /usr/bin/yum ---"
ls -la /usr/bin/yum
echo ""
echo "--- /usr/bin/dnf ---"
ls -la /usr/bin/dnf
echo ""
echo "yum apunta a dnf (o dnf-3)"
echo "Ejecutar yum = ejecutar dnf"
echo ""
echo "=== Versiones ==="
echo "dnf version:"
dnf --version 2>/dev/null | head -3
echo ""
echo "yum version:"
yum --version 2>/dev/null | head -3
echo ""
echo "(misma version — son el mismo binario)"
'
```

### Paso 1.3: La evolucion

```bash
docker compose exec alma-dev bash -c '
echo "=== Timeline ==="
echo ""
echo "1997: rpm     — herramienta de bajo nivel"
echo "2003: yum     — Python 2, resolver propio (lento)"
echo "2015: dnf     — Python 3, libsolv (rapido, correcto)"
echo "202x: dnf5    — C++, aun mas rapido (Fedora 39+)"
echo ""
echo "=== En cada version de RHEL ==="
echo "RHEL 5-7:  yum nativo"
echo "RHEL 8:    dnf nativo, yum = symlink a dnf"
echo "RHEL 9:    dnf nativo, yum = symlink (mantiene compat)"
echo ""
echo "=== libsolv ==="
echo "yum usaba un resolver propio que podia fallar"
echo "dnf usa libsolv: un solver SAT que GARANTIZA encontrar"
echo "la solucion si existe, o reportar correctamente que no hay"
echo ""
echo "libsolv instalado:"
rpm -q libsolv 2>/dev/null || echo "(no como paquete separado)"
'
```

---

## Parte 2 — Comandos equivalentes

### Objetivo

Comparar los mismos comandos entre yum y dnf.

### Paso 2.1: Listar repos

```bash
docker compose exec alma-dev bash -c '
echo "=== Comparar repolist ==="
echo ""
echo "--- dnf repolist ---"
dnf repolist 2>/dev/null
echo ""
echo "--- yum repolist ---"
yum repolist 2>/dev/null
echo ""
echo "(misma salida — son el mismo binario)"
'
```

### Paso 2.2: Buscar e info

```bash
docker compose exec alma-dev bash -c '
echo "=== Buscar un paquete ==="
echo ""
echo "--- dnf search bash ---"
dnf search bash 2>/dev/null | head -8
echo ""
echo "--- dnf info bash ---"
dnf info bash 2>/dev/null | head -15
echo ""
echo "Equivalencias:"
echo "  yum search = dnf search"
echo "  yum info   = dnf info"
echo "  yum list   = dnf list"
'
```

### Paso 2.3: Diferencias sutiles

```bash
docker compose exec alma-dev bash -c '
echo "=== Diferencias entre yum y dnf ==="
echo ""
printf "%-25s %-20s %-20s\n" "Operacion" "yum" "dnf"
printf "%-25s %-20s %-20s\n" "------------------------" "-------------------" "-------------------"
printf "%-25s %-20s %-20s\n" "Actualizar todos" "yum update" "dnf upgrade"
printf "%-25s %-20s %-20s\n" "Instalar grupo" "yum groupinstall" "dnf group install"
printf "%-25s %-20s %-20s\n" "Autoremove" "yum autoremove" "dnf autoremove"
printf "%-25s %-20s %-20s\n" "Modulos" "(no existe)" "dnf module"
printf "%-25s %-20s %-20s\n" "Historial undo" "yum history undo" "dnf history undo"
echo ""
echo "--- update vs upgrade ---"
echo "yum: update actualiza paquetes"
echo "dnf: update es ALIAS de upgrade (ambos funcionan)"
echo "     upgrade es el nombre oficial"
echo ""
echo "--- groupinstall ---"
echo "yum: yum groupinstall \"Development Tools\""
echo "dnf: dnf group install \"Development Tools\" (con espacio)"
echo "     dnf groupinstall tambien funciona (alias)"
'
```

### Paso 2.4: Cuando usar cada uno

```bash
docker compose exec alma-dev bash -c '
echo "=== Cuando usar yum vs dnf ==="
echo ""
echo "RHEL 8+, AlmaLinux 8+, Rocky 8+, Fedora:"
echo "  → dnf (siempre)"
echo "  yum funciona pero es un alias"
echo ""
echo "RHEL 7 / CentOS 7 (legacy):"
echo "  → yum (dnf no disponible por defecto)"
echo ""
echo "Scripts que deben funcionar en RHEL 7 Y 8+:"
echo "  → yum (funciona en ambos)"
echo ""
echo "Este curso: dnf (RHEL 9 / AlmaLinux 9)"
'
```

---

## Parte 3 — Configuracion de dnf

### Objetivo

Explorar y entender el archivo de configuracion de dnf.

### Paso 3.1: dnf.conf

```bash
docker compose exec alma-dev bash -c '
echo "=== /etc/dnf/dnf.conf ==="
cat /etc/dnf/dnf.conf
echo ""
echo "--- Campos principales ---"
echo "gpgcheck=1:                    verificar firmas (NO cambiar)"
echo "installonly_limit=3:           max kernels simultaneos"
echo "clean_requirements_on_remove:  autoremove al hacer remove"
echo "best=True:                     preferir la mejor version"
echo "skip_if_unavailable=False:     fallar si un repo no responde"
'
```

### Paso 3.2: Opciones utiles

```bash
docker compose exec alma-dev bash -c '
echo "=== Opciones configurables ==="
printf "%-30s %-10s %s\n" "Opcion" "Default" "Efecto"
printf "%-30s %-10s %s\n" "-----------------------------" "---------" "-------------------------"
printf "%-30s %-10s %s\n" "gpgcheck" "1" "Verificar firmas GPG"
printf "%-30s %-10s %s\n" "installonly_limit" "3" "Kernels simultaneos"
printf "%-30s %-10s %s\n" "clean_requirements_on_remove" "True" "Limpiar deps al remove"
printf "%-30s %-10s %s\n" "best" "True" "Mejor version o fallar"
printf "%-30s %-10s %s\n" "max_parallel_downloads" "3" "Descargas en paralelo"
printf "%-30s %-10s %s\n" "keepcache" "0" "No conservar .rpm"
printf "%-30s %-10s %s\n" "defaultyes" "False" "Default para confirms"
echo ""
echo "=== max_parallel_downloads actual ==="
grep max_parallel_downloads /etc/dnf/dnf.conf 2>/dev/null || echo "No configurado (default: 3)"
echo ""
echo "Para acelerar descargas:"
echo "  echo \"max_parallel_downloads=10\" >> /etc/dnf/dnf.conf"
'
```

### Paso 3.3: Plugins

```bash
docker compose exec alma-dev bash -c '
echo "=== Plugins de dnf ==="
echo ""
echo "--- Plugins instalados ---"
ls /etc/dnf/plugins/ 2>/dev/null || echo "(sin plugins configurados)"
echo ""
echo "--- Plugins cargados ---"
dnf repolist --verbose 2>/dev/null | grep -i "loaded plugins" || \
    echo "Plugins se cargan automaticamente"
echo ""
echo "--- Directorio de plugins ---"
ls /usr/lib/python*/site-packages/dnf-plugins/ 2>/dev/null | head -10
echo ""
echo "Plugins comunes:"
echo "  copr:    repositorios COPR (como PPAs de Ubuntu)"
echo "  versionlock: bloquear version de un paquete"
echo "  builddep:    instalar deps de compilacion"
'
```

### Paso 3.4: Comparacion con APT

```bash
docker compose exec alma-dev bash -c '
echo "=== dnf vs apt: diferencias de diseno ==="
printf "%-25s %-20s %-20s\n" "Aspecto" "apt" "dnf"
printf "%-25s %-20s %-20s\n" "------------------------" "-------------------" "-------------------"
printf "%-25s %-20s %-20s\n" "Actualizar indices" "apt update (manual)" "(automatico)"
printf "%-25s %-20s %-20s\n" "Resolver deps" "libapt" "libsolv (SAT)"
printf "%-25s %-20s %-20s\n" "Historial" "No nativo" "dnf history + undo"
printf "%-25s %-20s %-20s\n" "Grupos" "No nativo" "dnf group install"
printf "%-25s %-20s %-20s\n" "Modulos/streams" "No nativo" "dnf module"
printf "%-25s %-20s %-20s\n" "Purge config" "apt purge" "(rpm no separa)"
printf "%-25s %-20s %-20s\n" "Herramienta baja" "dpkg (.deb)" "rpm (.rpm)"
printf "%-25s %-20s %-20s\n" "Config" "sources.list" ".repo files"
echo ""
echo "dnf no necesita \"update\" antes de instalar — refresca automaticamente"
echo "apt NECESITA \"apt update\" previo"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. **yum** fue reemplazado por **dnf** en RHEL 8+. En sistemas
   modernos, `/usr/bin/yum` es un symlink a dnf. Ambos comandos
   producen la misma salida.

2. dnf usa **libsolv** (un solver SAT) para resolver dependencias,
   lo que es mas rapido y correcto que el resolver propio de yum.

3. Los comandos son casi identicos, con diferencias sutiles:
   `yum update` → `dnf upgrade`, `yum groupinstall` →
   `dnf group install`. dnf agrega `module` y `autoremove`.

4. La configuracion de dnf esta en `/etc/dnf/dnf.conf`.
   `gpgcheck=1` siempre debe estar activo.
   `max_parallel_downloads` acelera las descargas.

5. dnf no necesita un comando `update` previo como apt — refresca
   los metadatos automaticamente antes de operar.

6. En scripts que deben funcionar en RHEL 7 y 8+, usar `yum`
   (es alias en 8+). En sistemas modernos, usar `dnf`.
