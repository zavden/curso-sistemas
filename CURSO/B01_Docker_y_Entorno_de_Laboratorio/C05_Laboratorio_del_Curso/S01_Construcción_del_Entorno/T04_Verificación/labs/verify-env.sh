#!/bin/bash
# verify-env.sh — Verificacion del entorno del laboratorio
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
        printf "  [OK]   %s -> %s\n" "$description" "$output"
        ((PASS++))
    else
        printf "  [FAIL] %s\n" "$description"
        ((FAIL++))
    fi
}

echo "============================================"
echo "  Verificacion del entorno de laboratorio"
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
echo "--- Debian: Compilacion ---"
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
echo "--- AlmaLinux: Compilacion ---"
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

# --- Compilacion real ---
echo "--- Compilacion: C ---"
check "C en Debian" docker compose exec -T debian-dev bash -c \
    'echo "#include <stdio.h>
int main(void) { printf(\"OK\\n\"); return 0; }" > /tmp/test.c && gcc -o /tmp/test /tmp/test.c && /tmp/test'

check "C en AlmaLinux" docker compose exec -T alma-dev bash -c \
    'echo "#include <stdio.h>
int main(void) { printf(\"OK\\n\"); return 0; }" > /tmp/test.c && gcc -o /tmp/test /tmp/test.c && /tmp/test'
echo ""

echo "--- Compilacion: Rust ---"
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

# --- Volumenes compartidos ---
echo "--- Volumenes ---"
TOKEN=$(date +%s)
docker compose exec -T debian-dev bash -c "echo '$TOKEN' > /home/dev/workspace/verify_test.txt"
check "Volumen compartido (Debian -> AlmaLinux)" docker compose exec -T alma-dev bash -c \
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
