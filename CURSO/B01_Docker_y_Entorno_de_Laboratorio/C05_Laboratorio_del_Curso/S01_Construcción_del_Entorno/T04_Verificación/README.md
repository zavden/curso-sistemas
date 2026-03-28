# T04 — Verificación

## Objetivo

Crear un script de validación que verifica automáticamente que el entorno del
laboratorio está correctamente configurado: Docker funciona, las imágenes se
construyeron, todas las herramientas están instaladas, y los volúmenes comparten
datos.

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

## Problemas comunes y soluciones

### gdb: Operation not permitted

```
[FAIL] gdb en Debian
```

**Causa**: falta `cap_add: SYS_PTRACE` en el Compose file.

**Solución**: verificar que el compose.yml incluye:
```yaml
cap_add:
  - SYS_PTRACE
```

Recrear los contenedores: `docker compose down && docker compose up -d`.

### rustc: command not found

```
[FAIL] rustc
```

**Causa**: rustup no se instaló correctamente o `PATH` no incluye cargo.

**Solución**: verificar en el Dockerfile:
```dockerfile
ENV PATH="/home/dev/.cargo/bin:${PATH}"
```

Reconstruir: `docker compose build --no-cache && docker compose up -d`.

### man pages no funcionan

```bash
docker compose exec debian-dev man printf
# No manual entry for printf
```

**Causa**: la exclusión de man pages no se revirtió antes de instalar paquetes.

**Solución**: el `sed` que modifica `/etc/dpkg/dpkg.cfg.d/excludes` debe estar
**antes** del `apt-get install`. Si los paquetes ya se instalaron sin man pages,
hay que reconstruir la imagen con `--no-cache`.

### Volúmenes no compartidos

```
[FAIL] Volumen compartido (Debian → AlmaLinux)
```

**Causa**: los contenedores no usan el mismo named volume.

**Solución**: verificar que ambos servicios montan `workspace:/home/dev/workspace`
y que el volumen está declarado en la sección `volumes:` raíz.

## Requisitos del sistema host

| Recurso | Mínimo | Recomendado |
|---|---|---|
| Docker | 20.10+ | 24.0+ |
| Docker Compose | v2.0+ | v2.20+ |
| Espacio en disco | 3 GB | 5 GB |
| RAM | 2 GB libres | 4 GB libres |
| CPU | 2 cores | 4 cores |

El espacio en disco se distribuye aproximadamente:
- Imagen Debian dev: ~700 MB
- Imagen AlmaLinux dev: ~750 MB
- Toolchain Rust (por imagen): ~200 MB
- Volúmenes y workspace: ~100 MB
- Build cache: ~500 MB

---

## Ejercicios

### Ejercicio 1 — Ejecutar la verificación

```bash
cd /path/to/lab
docker compose up -d
./verify-env.sh
# Todos los checks deben pasar
```

### Ejercicio 2 — Provocar y diagnosticar un fallo

```bash
# Simular falta de SYS_PTRACE: recrear sin la capability
# Editar compose.yml: comentar cap_add
docker compose down
docker compose up -d
./verify-env.sh
# [FAIL] gdb en Debian
# [FAIL] gdb en AlmaLinux

# Restaurar y verificar
# Descomentar cap_add
docker compose down
docker compose up -d
./verify-env.sh
# Todo OK
```

### Ejercicio 3 — Verificar espacio en disco

```bash
# Ver espacio usado por Docker
docker system df

# Desglose detallado
docker system df -v

# Espacio de las imágenes del lab
docker image ls --format "{{.Repository}}\t{{.Size}}" | grep -E "(debian|alma)-dev"
```
