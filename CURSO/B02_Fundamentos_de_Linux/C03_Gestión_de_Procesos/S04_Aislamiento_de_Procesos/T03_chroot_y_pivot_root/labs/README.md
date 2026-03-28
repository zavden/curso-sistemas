# Lab — chroot y pivot_root

## Objetivo

Entender chroot y pivot_root: cambiar la raiz del filesystem,
observar las limitaciones de seguridad de chroot (no aisla PIDs,
red, ni usuarios), y por que los runtimes de contenedores usan
pivot_root en lugar de chroot.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

Nota: las operaciones de chroot requieren root. Dentro de los
contenedores del curso algunos pasos pueden estar limitados por
las capabilities del contenedor.

---

## Parte 1 — chroot basico

### Objetivo

Entender que hace chroot y como se usa.

### Paso 1.1: Que es chroot

```bash
docker compose exec debian-dev bash -c '
echo "=== chroot — cambiar directorio raiz ==="
echo ""
echo "chroot /path/to/new-root [command]"
echo ""
echo "Despues del chroot:"
echo "  - El proceso ve /path/to/new-root como /"
echo "  - No puede acceder a nada fuera (cd .. no funciona)"
echo "  - Todos los hijos heredan el chroot"
echo ""
echo "Requisitos:"
echo "  - El nuevo rootfs debe tener al menos un shell"
echo "  - Y las librerias necesarias"
echo "  - Opcion mas simple: extraer una imagen de Alpine"
'
```

### Paso 1.2: Preparar un rootfs minimo

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear rootfs minimo con busybox ==="
mkdir -p /tmp/chroot-test/{bin,proc,sys,dev,tmp}

echo ""
echo "=== Copiar busybox (si es estatico, no necesita libs) ==="
if [ -f /bin/busybox ]; then
    cp /bin/busybox /tmp/chroot-test/bin/
    # Crear enlaces para sh, ls, cat, ps, etc.
    for cmd in sh ls cat echo ps mount mkdir; do
        ln -sf busybox /tmp/chroot-test/bin/$cmd 2>/dev/null
    done
    echo "busybox copiado y enlaces creados"
else
    echo "busybox no disponible — usando otra alternativa"
    # Copiar sh y librerias necesarias
    cp /bin/sh /tmp/chroot-test/bin/ 2>/dev/null
    mkdir -p /tmp/chroot-test/lib /tmp/chroot-test/lib64
    # Copiar librerias dinamicas
    ldd /bin/sh 2>/dev/null | grep -o "/[^ ]*" | while read lib; do
        cp "$lib" "/tmp/chroot-test$(dirname $lib)/" 2>/dev/null
    done
    echo "sh y librerias copiados"
fi

echo ""
echo "=== Contenido del rootfs ==="
ls -la /tmp/chroot-test/bin/
'
```

### Paso 1.3: Usar chroot

```bash
docker compose exec debian-dev bash -c '
echo "=== Entrar al chroot ==="
if [ -x /tmp/chroot-test/bin/sh ]; then
    chroot /tmp/chroot-test /bin/sh -c "
        echo \"=== Dentro del chroot ===\"
        echo \"pwd: \$(pwd)\"
        echo \"ls /: \$(ls /)\"
        echo \"\"
        echo \"No puedo ver el filesystem del host:\"
        ls /home 2>/dev/null || echo \"  /home esta vacio\"
        ls /etc/hostname 2>/dev/null || echo \"  /etc/hostname no existe\"
    " 2>/dev/null || echo "(chroot fallo — capabilities insuficientes)"
else
    echo "(rootfs no esta listo — mostrando concepto)"
    echo ""
    echo "Dentro de un chroot se veria:"
    echo "  pwd: /"
    echo "  ls /: bin proc sys dev tmp"
    echo "  El filesystem del host es invisible"
fi
'
```

### Paso 1.4: chroot sin /proc

```bash
docker compose exec debian-dev bash -c '
echo "=== chroot sin montar /proc ==="
echo ""
echo "Sin /proc montado dentro del chroot:"
echo "  - ps no funciona (lee de /proc)"
echo "  - top no funciona"
echo "  - /proc/self no existe"
echo ""
echo "Para que funcione, hay que montar /proc:"
echo "  mount -t proc proc /path/chroot/proc"
echo "  (o mount --bind /proc /path/chroot/proc)"
echo ""
echo "PERO: al montar /proc del host, el chroot ve"
echo "TODOS los procesos del host — no hay aislamiento de PIDs"
'
```

---

## Parte 2 — Limitaciones de chroot

### Objetivo

Entender por que chroot NO es un mecanismo de seguridad.

### Paso 2.1: chroot NO aisla

```bash
docker compose exec debian-dev bash -c '
echo "=== Lo que chroot NO aisla ==="
echo ""
echo "1. PIDs"
echo "   Con /proc montado, ve TODOS los procesos del host"
echo "   Puede enviar senales a procesos del host"
echo ""
echo "2. Red"
echo "   Comparte interfaces, puertos, rutas del host"
echo "   Puede escuchar en cualquier puerto"
echo ""
echo "3. Usuarios"
echo "   UID 0 dentro = UID 0 fuera"
echo "   root en el chroot = root en el host"
echo ""
echo "4. IPC"
echo "   Puede acceder a shared memory del host"
echo ""
echo "5. Dispositivos"
echo "   Con /dev montado, accede a todos los dispositivos"
'
```

### Paso 2.2: Escape de chroot (concepto)

```bash
docker compose exec debian-dev bash -c '
echo "=== Escape de chroot (con root) ==="
echo ""
echo "Un proceso root dentro del chroot puede escapar:"
echo ""
echo "Metodo clasico (en C):"
echo "  1. mkdir(\"escape\")"
echo "  2. fd = open(\".\", O_RDONLY)  // guardar fd actual"
echo "  3. chroot(\"escape\")          // cambiar raiz"
echo "  4. fchdir(fd)               // volver al fd guardado"
echo "  5. chdir(\"..\") repetido     // subir hasta la raiz real"
echo "  6. chroot(\".\")              // raiz real restaurada"
echo ""
echo "=== Consecuencia ==="
echo "chroot NO es un mecanismo de seguridad"
echo "Es util para reparacion y builds, no para aislamiento"
'
```

### Paso 2.3: Usos legitimos de chroot

```bash
docker compose exec debian-dev bash -c '
echo "=== Usos correctos de chroot ==="
echo ""
echo "1. REPARACION DEL SISTEMA"
echo "   Arrancar desde USB, montar disco, chroot al sistema roto"
echo "   sudo mount /dev/sda2 /mnt"
echo "   sudo mount --bind /proc /mnt/proc"
echo "   sudo mount --bind /sys /mnt/sys"
echo "   sudo mount --bind /dev /mnt/dev"
echo "   sudo chroot /mnt"
echo "   → Ahora puedes reinstalar GRUB, arreglar fstab, etc."
echo ""
echo "2. BUILD ENVIRONMENTS"
echo "   Compilar software en un rootfs limpio"
echo "   debootstrap crea rootfs Debian para esto"
echo ""
echo "3. TESTING"
echo "   Probar software en un rootfs de otra distribucion"
echo "   Sin VM ni contenedor completo"
'
```

---

## Parte 3 — pivot_root y comparacion

### Objetivo

Entender pivot_root como la alternativa segura a chroot.

### Paso 3.1: Que es pivot_root

```bash
docker compose exec debian-dev bash -c '
echo "=== pivot_root — intercambiar raices ==="
echo ""
echo "Sintaxis: pivot_root new_root put_old"
echo ""
echo "chroot:"
echo "  Solo cambia la referencia de / para el proceso"
echo "  El viejo rootfs sigue accesible (via fds abiertos)"
echo "  Escape posible con root"
echo ""
echo "pivot_root:"
echo "  Intercambia la raiz del mount namespace completo"
echo "  El viejo rootfs se mueve a un subdirectorio"
echo "  Despues se puede desmontar completamente"
echo "  Una vez desmontado, NO hay forma de acceder al host"
echo ""
echo "Requisitos de pivot_root:"
echo "  - new_root debe ser un mount point"
echo "  - Debe ejecutarse en un mount namespace aislado"
echo "  - new_root no puede ser el rootfs actual"
'
```

### Paso 3.2: Flujo de un runtime de contenedor

```bash
docker compose exec debian-dev bash -c '
echo "=== Como Docker crea un contenedor (simplificado) ==="
echo ""
echo "1. clone() con CLONE_NEWNS"
echo "   → nuevo mount namespace"
echo ""
echo "2. Montar el rootfs de la imagen"
echo "   → overlay2 combina las capas de la imagen"
echo ""
echo "3. pivot_root al rootfs"
echo "   → la imagen se convierte en /"
echo ""
echo "4. Desmontar el viejo rootfs"
echo "   → filesystem del host ya no accesible"
echo ""
echo "5. Montar /proc, /sys, /dev"
echo "   → el contenedor tiene sus propios filesystems virtuales"
echo ""
echo "6. Ejecutar el entrypoint"
echo "   → PID 1 del contenedor"
echo ""
echo "Si usara chroot en vez de pivot_root:"
echo "  El rootfs del host seguiria accesible"
echo "  Un proceso root podria escapar"
'
```

### Paso 3.3: Tabla comparativa

```bash
docker compose exec debian-dev bash -c '
echo "=== chroot vs pivot_root ==="
printf "%-20s %-25s %-25s\n" "Aspecto" "chroot" "pivot_root"
printf "%-20s %-25s %-25s\n" "------------------" "-----------------------" "-----------------------"
printf "%-20s %-25s %-25s\n" "Mecanismo" "Cambia ref de /" "Intercambia rootfs"
printf "%-20s %-25s %-25s\n" "Escape con root" "Posible" "No (si desmonta viejo)"
printf "%-20s %-25s %-25s\n" "Requiere namespace" "No" "Si (mount namespace)"
printf "%-20s %-25s %-25s\n" "Viejo rootfs" "Sigue accesible" "Se puede desmontar"
printf "%-20s %-25s %-25s\n" "Complejidad" "Baja (un comando)" "Mayor (ns + mounts)"
printf "%-20s %-25s %-25s\n" "Uso moderno" "Reparacion, builds" "Contenedores (Docker)"
printf "%-20s %-25s %-25s\n" "Seguridad" "Baja" "Alta (con namespaces)"
'
```

### Paso 3.4: Verificar que estamos en un rootfs aislado

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar aislamiento del rootfs ==="
echo ""
echo "=== / de este contenedor ==="
ls /
echo ""
echo "=== Distribucion del rootfs ==="
cat /etc/os-release | head -3
echo ""
echo "=== Puntos de montaje ==="
mount | head -5
echo "..."
echo ""
echo "Este contenedor usa pivot_root internamente"
echo "No podemos ver el filesystem del host"
echo "No hay /host ni forma de subir mas alla de /"
echo ""
echo "readlink /proc/1/root:"
readlink /proc/1/root
echo "(/ — la raiz del contenedor)"
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/chroot-test 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. **chroot** cambia la raiz del filesystem para un proceso.
   Es simple pero **no es aislamiento real**: no aisla PIDs,
   red, usuarios, y root puede escapar.

2. **pivot_root** intercambia la raiz del mount namespace
   completo. El viejo rootfs se puede desmontar, haciendo
   imposible acceder al filesystem del host.

3. Docker y todos los runtimes OCI usan **pivot_root** (no
   chroot) por seguridad. El flujo es: clone+NEWNS, montar
   rootfs, pivot_root, desmontar viejo, montar /proc.

4. chroot es util para **reparacion** (chroot al disco roto)
   y **build environments**, no para seguridad.

5. pivot_root requiere un **mount namespace aislado** para ser
   util. Sin namespaces, no hay aislamiento real.

6. El rootfs del contenedor viene de las capas de la imagen
   combinadas con overlay2, montado como / via pivot_root.
