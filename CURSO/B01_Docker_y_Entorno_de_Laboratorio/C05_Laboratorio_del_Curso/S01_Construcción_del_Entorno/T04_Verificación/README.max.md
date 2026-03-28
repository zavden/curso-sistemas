# T04 — Verificación

## Objetivo

Crear un script de validación que verifica automáticamente que el entorno del
laboratorio está correctamente configurado: Docker funciona, las imágenes se
construyeron, todas las herramientas están instaladas, y los volúmenes comparten
datos.

---

## El script de verificación

```bash
#!/bin/bash
# verify-env.sh — Verificación del entorno del laboratorio
# Ejecutar desde el directorio del lab: ./verify-env.sh

set -euo pipefail

PASS=0
FAIL=0

check() {
    local description="$1"
    shift
    if "$@" > /dev/null 2>&1; then
        printf "  [OK]   %s\n" "$description"
        ((PASS++))
    else
        printf "  [FAIL] %s\n" "$description"
        ((FAIL++))
    fi
}

check_output() {
    local description="$1"
    shift
    if output=$("$@" 2>&1) && [ -n "$output" ]; then
        printf "  [OK]   %s → %s\n" "$description" "$output"
        ((PASS++))
    else
        printf "  [FAIL] %s\n" "$description"
        ((FAIL++))
    fi
}

echo "============================================"
echo "  Verificación del entorno de laboratorio"
echo "============================================"
echo ""

# --- Docker ---
echo "--- Docker ---"
check "Docker instalado" docker --version
check "Docker daemon activo" docker info
echo ""

# --- Compose ---
echo "--- Docker Compose ---"
check "Docker Compose instalado" docker compose version
check "Servicios definidos" docker compose config --services
echo ""

# --- Contenedores ---
echo "--- Contenedores ---"
check "debian-dev corriendo" docker compose ps --status running --format "{{.Name}}" | grep -q debian-dev
check "alma-dev corriendo" docker compose ps --status running --format "{{.Name}}" | grep -q alma-dev
echo ""

# --- Herramientas Debian ---
echo "--- Debian: Compilación ---"
check_output "gcc" docker compose exec -T debian-dev gcc --version | head -1
check_output "g++" docker compose exec -T debian-dev g++ --version | head -1
check_output "make" docker compose exec -T debian-dev make --version | head -1
check_output "cmake" docker compose exec -T debian-dev cmake --version | head -1
echo ""

echo "--- Debian: Debugging ---"
check_output "gdb" docker compose exec -T debian-dev gdb --version | head -1
check_output "valgrind" docker compose exec -T debian-dev valgrind --version
check "strace" docker compose exec -T debian-dev strace --version
echo ""

echo "--- Debian: Rust ---"
check_output "rustc" docker compose exec -T debian-dev rustc --version
check_output "cargo" docker compose exec -T debian-dev cargo --version
check_output "rustfmt" docker compose exec -T debian-dev rustfmt --version
check_output "clippy" docker compose exec -T debian-dev cargo clippy --version
echo ""

# --- Herramientas AlmaLinux ---
echo "--- AlmaLinux: Compilación ---"
check_output "gcc" docker compose exec -T alma-dev gcc --version | head -1
check_output "g++" docker compose exec -T alma-dev g++ --version | head -1
check_output "make" docker compose exec -T alma-dev make --version | head -1
check_output "cmake" docker compose exec -T alma-dev cmake --version | head -1
echo ""

echo "--- AlmaLinux: Debugging ---"
check_output "gdb" docker compose exec -T alma-dev gdb --version | head -1
check_output "valgrind" docker compose exec -T alma-dev valgrind --version
check "strace" docker compose exec -T alma-dev strace --version
echo ""

echo "--- AlmaLinux: Rust ---"
check_output "rustc" docker compose exec -T alma-dev rustc --version
check_output "cargo" docker compose exec -T alma-dev cargo --version
echo ""

# --- Compilación real ---
echo "--- Compilación: C ---"
check "C en Debian" docker compose exec -T debian-dev bash -c \
    'echo "#include <stdio.h>
int main(void) { printf(\"OK\\n\"); return 0; }" > /tmp/test.c && gcc -o /tmp/test /tmp/test.c && /tmp/test'

check "C en AlmaLinux" docker compose exec -T alma-dev bash -c \
    'echo "#include <stdio.h>
int main(void) { printf(\"OK\\n\"); return 0; }" > /tmp/test.c && gcc -o /tmp/test /tmp/test.c && /tmp/test'
echo ""

echo "--- Compilación: Rust ---"
check "Rust en Debian" docker compose exec -T debian-dev bash -c \
    'echo "fn main() { println!(\"OK\"); }" > /tmp/test.rs && rustc -o /tmp/test_rs /tmp/test.rs && /tmp/test_rs'

check "Rust en AlmaLinux" docker compose exec -T alma-dev bash -c \
    'echo "fn main() { println!(\"OK\"); }" > /tmp/test.rs && rustc -o /tmp/test_rs /tmp/test.rs && /tmp/test_rs'
echo ""

# --- gdb con SYS_PTRACE ---
echo "--- SYS_PTRACE (gdb) ---"
check "gdb en Debian" docker compose exec -T debian-dev bash -c \
    'echo "#include <stdio.h>
int main(void) { int x=42; printf(\"%d\\n\",x); return 0; }" > /tmp/gdb_test.c && \
    gcc -g -o /tmp/gdb_test /tmp/gdb_test.c && \
    echo -e "break main\nrun\ncontinue\nquit" | gdb -batch /tmp/gdb_test 2>&1 | grep -q "Breakpoint"'

check "gdb en AlmaLinux" docker compose exec -T alma-dev bash -c \
    'echo "#include <stdio.h>
int main(void) { int x=42; printf(\"%d\\n\",x); return 0; }" > /tmp/gdb_test.c && \
    gcc -g -o /tmp/gdb_test /tmp/gdb_test.c && \
    echo -e "break main\nrun\ncontinue\nquit" | gdb -batch /tmp/gdb_test 2>&1 | grep -q "Breakpoint"'
echo ""

# --- Volúmenes compartidos ---
echo "--- Volúmenes ---"
TOKEN=$(date +%s)
docker compose exec -T debian-dev bash -c "echo '$TOKEN' > /home/dev/workspace/verify_test.txt"
check "Volumen compartido (Debian → AlmaLinux)" docker compose exec -T alma-dev bash -c \
    "grep -q '$TOKEN' /home/dev/workspace/verify_test.txt"
docker compose exec -T debian-dev rm -f /home/dev/workspace/verify_test.txt

check "Bind mount src/ visible en Debian" docker compose exec -T debian-dev test -d /home/dev/workspace/src
check "Bind mount src/ visible en AlmaLinux" docker compose exec -T alma-dev test -d /home/dev/workspace/src
echo ""

# --- Resumen ---
echo "============================================"
echo "  Resultados: $PASS OK, $FAIL FAIL"
echo "============================================"

if [ "$FAIL" -gt 0 ]; then
    echo ""
    echo "  Hay $FAIL verificaciones fallidas."
    echo "  Revisa los mensajes [FAIL] arriba."
    exit 1
else
    echo ""
    echo "  Entorno verificado correctamente."
    exit 0
fi
```

---

## Cómo usar el script

```bash
# Asegurarse de que el lab está corriendo
docker compose up -d

# Ejecutar la verificación
chmod +x verify-env.sh
./verify-env.sh
```

Salida esperada:

```
============================================
  Verificación del entorno de laboratorio
============================================

--- Docker ---
  [OK]   Docker instalado
  [OK]   Docker daemon activo

--- Docker Compose ---
  [OK]   Docker Compose instalado
  [OK]   Servicios definidos

--- Contenedores ---
  [OK]   debian-dev corriendo
  [OK]   alma-dev corriendo

--- Debian: Compilación ---
  [OK]   gcc → gcc (Debian 12.2.0-14) 12.2.0
  [OK]   g++ → g++ (Debian 12.2.0-14) 12.2.0
  ...

============================================
  Resultados: 28 OK, 0 FAIL
============================================

  Entorno verificado correctamente.
```

---

## Problemas comunes y soluciones

### gdb: Operation not permitted

```
[FAIL] gdb en Debian
```

**Causa**: falta `cap_add: SYS_PTRACE` en el Compose file.

**Síntomas:**
```
(gdb) attach 1234
Could not attach to process.  If you have  denied it, ...
Operation not permitted
```

**Solución**: verificar que el compose.yml incluye:
```yaml
cap_add:
  - SYS_PTRACE
```

Recrear los contenedores:
```bash
docker compose down && docker compose up -d
```

---

### rustc: command not found

```
[FAIL] rustc
```

**Causa**: rustup no se instaló correctamente o `PATH` no incluye cargo.

**Diagnóstico:**
```bash
# Verificar PATH dentro del contenedor
docker compose exec debian-dev bash -c 'echo $PATH'
# Debe incluir: /home/dev/.cargo/bin

# Verificar que cargo existe
docker compose exec debian-dev ls -la /home/dev/.cargo/bin/
```

**Solución**: verificar en el Dockerfile:
```dockerfile
ENV PATH="/home/dev/.cargo/bin:${PATH}"
```

Reconstruir:
```bash
docker compose build --no-cache debian-dev
docker compose up -d
```

---

### man pages no funcionan

```bash
docker compose exec debian-dev man printf
# No manual entry for printf
```

**Causa**: la exclusión de man pages no se revirtió antes de instalar paquetes.

**Diagnóstico:**
```bash
# Verificar el archivo de exclusión
docker compose exec debian-dev cat /etc/dpkg/dpkg.cfg.d/excludes
# Si contiene "path-exclude /usr/share/man" sin comentar, ese es el problema
```

**Solución**: el `sed` que modifica `/etc/dpkg/dpkg.cfg.d/excludes` debe estar
**antes** del `apt-get install`. Si los paquetes ya se instalaron sin man pages,
hay que reconstruir la imagen con `--no-cache`:

```bash
docker compose build --no-cache debian-dev
docker compose up -d
```

---

### Volúmenes no compartidos

```
[FAIL] Volumen compartido (Debian → AlmaLinux)
```

**Causa**: los contenedores no usan el mismo named volume.

**Diagnóstico:**
```bash
# Ver volúmenes de cada contenedor
docker inspect debian-dev | grep -A 10 Mounts
docker inspect alma-dev | grep -A 10 Mounts

# Ver volúmenes disponibles
docker volume ls | grep workspace
```

**Solución**: verificar que ambos servicios montan `workspace:/home/dev/workspace`
y que el volumen está declarado en la sección `volumes:` raíz:

```yaml
services:
  debian-dev:
    volumes:
      - workspace:/home/dev/workspace
  alma-dev:
    volumes:
      - workspace:/home/dev/workspace

volumes:
  workspace:
```

---

### bind mount src/ no visible

```
[FAIL] Bind mount src/ visible en Debian
```

**Causa**: el directorio `./src` no existe en el host o tiene permisos incorrectos.

**Solución:**
```bash
# Crear el directorio si no existe
mkdir -p src

# Verificar permisos
ls -la src/
# Debe ser legible por el usuario dev (uid 1000)

# Si hay problemas de permisos
chmod 755 src
```

---

### Contenedores no inician

```
[FAIL] debian-dev corriendo
```

**Diagnóstico:**
```bash
# Ver estado detallado
docker compose ps -a

# Ver logs del contenedor
docker compose logs debian-dev

# Ver logs en tiempo real
docker compose logs -f debian-dev
```

**Causas comunes:**

| Error | Causa | Solución |
|---|---|---|
| `Cannot start ... OCI runtime` | Dockerfile con errores | `docker compose build --no-cache` |
| `Port is already allocated` | Puerto en uso | Cambiar puerto en compose.yml |
| `Invalid bind mount` | Path no existe | Crear directorio faltante |

---

### Docker daemon no responde

```
[FAIL] Docker daemon activo
```

**Causa**: Docker no está corriendo o el usuario no tiene permisos.

**Solución:**
```bash
# Verificar estado del servicio (Linux)
sudo systemctl status docker

# Reiniciar Docker
sudo systemctl restart docker

# Verificar que el usuario está en el grupo docker
groups $USER
# Si no está: sudo usermod -aG docker $USER
# Luego cerrar sesión y volver a entrar
```

---

### compilación C falla silenciosamente

```
[FAIL] C en Debian
```

**Diagnóstico:**
```bash
# Ver el error exacto
docker compose exec -T debian-dev bash -c \
    'echo "#include <stdio.h>
int main(void) { printf(\"OK\\n\"); return 0; }" > /tmp/test.c && gcc -o /tmp/test /tmp/test.c 2>&1'
```

**Causas comunes:**

| Error | Causa | Solución |
|---|---|---|
| `gcc: command not found` | gcc no instalado | Reconstruir imagen |
| `fatal error: stdio.h` | libc6-dev faltante | Reconstruir imagen |
| `/tmp/test.c:1:20: error` | Syntax error en el script | Verificar quoting |

---

## Requisitos del sistema host

| Recurso | Mínimo | Recomendado |
|---|---|---|
| Docker | 20.10+ | 24.0+ |
| Docker Compose | v2.0+ | v2.20+ |
| Espacio en disco | 3 GB | 5 GB |
| RAM | 2 GB libres | 4 GB libres |
| CPU | 2 cores | 4 cores |

### Distribución del espacio

| Componente | Tamaño aproximado |
|---|---|
| Imagen Debian dev | ~700 MB |
| Imagen AlmaLinux dev | ~750 MB |
| Toolchain Rust (por imagen) | ~200 MB |
| Volúmenes y workspace | ~100 MB |
| Build cache de Docker | ~500 MB |
| **Total** | **~2.5 GB** |

---

## Ejercicios

### Ejercicio 1 — Ejecutar la verificación completa

```bash
cd /path/to/lab
docker compose up -d
./verify-env.sh

# Verificar que todos los checks pasan
# Expected: 28 OK, 0 FAIL
```

---

### Ejercicio 2 — Provocar y diagnosticar fallo de SYS_PTRACE

```bash
# Simular falta de SYS_PTRACE
# Editar compose.yml: comentar cap_add

# Recrear contenedores
docker compose down
docker compose up -d

# Ejecutar verificación
./verify-env.sh

# Verificar que gdb falla
# [FAIL] gdb en Debian
# [FAIL] gdb en AlmaLinux

# Restaurar: descomentar cap_add y recrear
docker compose down
docker compose up -d
./verify-env.sh

# Ahora todo OK
```

---

### Ejercicio 3 — Verificar espacio en disco

```bash
# Ver espacio usado por Docker
docker system df

# Desglose detallado
docker system df -v

# Espacio de las imágenes del lab
docker image ls --format "{{.Repository}}\t{{.Size}}" | grep -E "(debian|alma)-dev"

# Limpiar si es necesario
docker system prune -a
```

---

### Ejercicio 4 — Diagnosticar man pages faltantes

```bash
# Simular el problema: crear imagen sin man pages
# (editar el Dockerfile para comentar el sed)

# Construir y verificar
docker compose build --no-cache debian-dev
docker compose up -d

# Verificar el problema
docker compose exec debian-dev man -k printf
# No manual entry for printf

# Verificar que el archivo de exclusión tiene la línea activa
docker compose exec debian-dev grep -v "^#" /etc/dpkg/dpkg.cfg.d/excludes

# Solución: reconstruir con el sed correcto
```

---

### Ejercicio 5 — Diagnosticar volumen no compartido

```bash
# Provocar el problema: usar volumes diferentes
# Editar compose.yml para que cada uno tenga volume distinto:
# debian-dev: debian-workspace:/home/dev/workspace
# alma-dev: alma-workspace:/home/dev/workspace

docker compose down
docker compose up -d

# Verificar que fallan los checks de volumen compartido
./verify-env.sh
# [FAIL] Volumen compartido (Debian → AlmaLinux)

# Diagnosticar
docker volume ls | grep workspace
# Aparecerán dos volumes diferentes

# Corregir y verificar
```

---

### Ejercicio 6 — Verificar compilación cruzada

```bash
# Compilar en Debian
docker compose exec debian-dev bash -c \
    'echo "int main(){return 0;}" > /tmp/cross.c && gcc -o /tmp/cross /tmp/cross.c'

# Copiar a AlmaLinux via volumen compartido
docker compose exec debian-dev cp /tmp/cross /home/dev/workspace/

# Intentar ejecutar en AlmaLinux
docker compose exec alma-dev bash -c \
    '/home/dev/workspace/cross && echo "OK" || echo "FAILED"'

# Debería fallar por incompatibilidad de glibc
```

---

### Ejercicio 7 — Test de estrés del entorno

```bash
# Ejecutar múltiples comandos simultáneos
docker compose exec -T debian-dev bash -c 'for i in {1..10}; do gcc --version & done; wait'

# Verificar que no hay memory leaks o problemas de recursos
docker stats --no-stream
```

---

### Ejercicio 8 — Verificar herramientas de red

```bash
# Test de conectividad entre contenedores
docker compose exec -T debian-dev ping -c 1 alma-dev
docker compose exec -T alma-dev ping -c 1 debian-dev

# Test de netcat
docker compose exec -d alma-dev bash -c 'nc -l -p 9999 -e /bin/cat --keep-open'
docker compose exec -T debian-dev bash -c 'echo "test" | nc alma-dev 9999'

# Verificar herramientas instaladas
docker compose exec debian-dev which ss ip dig tcpdump nc
docker compose exec alma-dev which ss ip dig tcpdump nc
```

---

### Ejercicio 9 — Reconstrucción desde cero

```bash
# Limpiar todo
docker compose down -v
docker system prune -a -f

# Ver que no quedan imágenes ni volúmenes
docker image ls
docker volume ls

# Reconstruir todo
docker compose build
docker compose up -d

# Verificar
./verify-env.sh
```

---

### Ejercicio 10 — Script de verificación personalizado

Crear un script que verifique solo algunas herramientas específicas:

```bash
cat > /tmp/quick-check.sh << 'EOF'
#!/bin/bash
set -euo pipefail

echo "=== Quick Check ==="

# Solo verificar lo esencial
docker compose exec -T debian-dev gcc --version > /dev/null && echo "Debian gcc: OK"
docker compose exec -T alma-dev gcc --version > /dev/null && echo "Alma gcc: OK"
docker compose exec -T debian-dev rustc --version > /dev/null && echo "Debian rust: OK"
docker compose exec -T alma-dev rustc --version > /dev/null && echo "Alma rust: OK"

echo "=== Done ==="
EOF

chmod +x /tmp/quick-check.sh
/tmp/quick-check.sh
```
