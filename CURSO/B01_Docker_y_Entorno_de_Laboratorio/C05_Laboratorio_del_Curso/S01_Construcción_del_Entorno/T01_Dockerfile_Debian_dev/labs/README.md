# Lab — Dockerfile Debian dev

## Objetivo

Construir la imagen de desarrollo Debian del curso paso a paso,
entendiendo cada seccion del Dockerfile, verificando las herramientas
instaladas, y comprobando la necesidad de SYS_PTRACE para debugging.

## Prerequisitos

- Docker instalado y funcionando
- Conexion a internet
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `Dockerfile.debian-dev` | Dockerfile completo de la imagen de desarrollo Debian |

---

## Parte 1 — Examinar el Dockerfile

### Objetivo

Entender cada seccion del Dockerfile antes de construirlo.

### Paso 1.1: Ver el Dockerfile completo

```bash
cat Dockerfile.debian-dev
```

El Dockerfile tiene 7 secciones principales. Identifica cada una antes
de continuar.

### Paso 1.2: Imagen base

```bash
head -1 Dockerfile.debian-dev
```

`debian:bookworm` — Debian 12, soporte hasta 2028. Se usa la version
completa (no slim) porque necesitamos herramientas de desarrollo.

### Paso 1.3: Man pages

```bash
grep -A 2 "man pages" Dockerfile.debian-dev
```

Las imagenes Docker de Debian excluyen man pages por defecto para reducir
tamano. El `sed` revierte esta exclusion **antes** de instalar paquetes.
Si se instalaran los paquetes primero, las man pages no se incluirian.

### Paso 1.4: Paquetes en una sola capa

```bash
grep -c "RUN" Dockerfile.debian-dev
```

Hay pocos `RUN` porque combinar `apt-get update` + `install` + limpieza
en un solo `RUN` evita que archivos temporales queden en capas
intermedias. El `rm -rf /var/lib/apt/lists/*` limpia los indices de apt.

### Paso 1.5: Usuario non-root

```bash
grep -A 1 "USER" Dockerfile.debian-dev
```

Rust se instala como usuario `dev`, no como root. Asi `~/.rustup/` y
`~/.cargo/` pertenecen al usuario correcto y se pueden actualizar sin
privilegios.

### Paso 1.6: --no-install-recommends

```bash
grep "no-install-recommends" Dockerfile.debian-dev
```

Sin este flag, apt instala paquetes "recomendados" que no son
estrictamente necesarios, agregando ~200MB extra a la imagen.

---

## Parte 2 — Construir la imagen

### Objetivo

Construir la imagen y observar el proceso de build.

### Paso 2.1: Construir

```bash
docker build -t lab-debian-dev -f Dockerfile.debian-dev .
```

Antes de ejecutar, predice:

- Cuantas instrucciones `RUN` ejecutara Docker?
- Que paso tardara mas (paquetes apt o Rust)?

El build puede tardar varios minutos dependiendo de la conexion. Observa
las capas que se van creando.

### Paso 2.2: Verificar tamano

```bash
docker image ls lab-debian-dev
```

Salida esperada:

```
REPOSITORY       TAG       IMAGE ID       CREATED          SIZE
lab-debian-dev   latest    <hash>         <time>           ~700-800MB
```

### Paso 2.3: Contar capas

```bash
docker history lab-debian-dev --format "{{.CreatedBy}}" | head -15
```

Observa que cada instruccion del Dockerfile genera una capa. Las capas
de la imagen base (debian:bookworm) estan al final de la lista.

---

## Parte 3 — Verificar herramientas

### Objetivo

Comprobar que todas las herramientas necesarias estan instaladas y
funcionan correctamente.

### Paso 3.1: Compiladores C/C++

```bash
docker run --rm lab-debian-dev bash -c '
echo "=== gcc ==="
gcc --version | head -1
echo ""
echo "=== g++ ==="
g++ --version | head -1
'
```

Salida esperada: GCC 12.x (version incluida en Debian bookworm).

### Paso 3.2: Build tools

```bash
docker run --rm lab-debian-dev bash -c '
echo "=== make ==="
make --version | head -1
echo ""
echo "=== cmake ==="
cmake --version | head -1
'
```

### Paso 3.3: Debugging tools

```bash
docker run --rm lab-debian-dev bash -c '
echo "=== gdb ==="
gdb --version | head -1
echo ""
echo "=== valgrind ==="
valgrind --version
echo ""
echo "=== strace ==="
strace --version 2>&1 | head -1
'
```

### Paso 3.4: Rust

```bash
docker run --rm lab-debian-dev bash -c '
echo "=== rustc ==="
rustc --version
echo ""
echo "=== cargo ==="
cargo --version
echo ""
echo "=== rustfmt ==="
rustfmt --version
echo ""
echo "=== clippy ==="
cargo clippy --version
'
```

### Paso 3.5: Man pages

```bash
docker run --rm lab-debian-dev man -k printf | head -5
```

Si las man pages se habilitaron correctamente, `man -k printf` muestra
entradas como `printf(1)`, `printf(3)`.

Si devuelve "nothing appropriate", el `sed` no se ejecuto antes de
instalar paquetes y hay que reconstruir con `--no-cache`.

### Paso 3.6: Herramientas de red

```bash
docker run --rm lab-debian-dev bash -c '
echo "=== ip ==="
ip -V 2>&1
echo ""
echo "=== ss ==="
ss --version 2>&1 | head -1
echo ""
echo "=== nc ==="
nc -h 2>&1 | head -1
echo ""
echo "=== dig ==="
dig -v 2>&1 | head -1
'
```

### Paso 3.7: Compilar y ejecutar C

```bash
docker run --rm lab-debian-dev bash -c '
cat > /tmp/hello.c << EOF
#include <stdio.h>
int main(void) {
    printf("Hello from Debian dev!\n");
    return 0;
}
EOF
gcc -o /tmp/hello /tmp/hello.c && /tmp/hello
'
```

### Paso 3.8: Compilar y ejecutar Rust

```bash
docker run --rm lab-debian-dev bash -c '
cat > /tmp/hello.rs << EOF
fn main() {
    println!("Hello from Debian Rust!");
}
EOF
rustc -o /tmp/hello_rs /tmp/hello.rs && /tmp/hello_rs
'
```

### Paso 3.9: Usuario correcto

```bash
docker run --rm lab-debian-dev bash -c '
echo "User: $(whoami)"
echo "UID: $(id -u)"
echo "Home: $HOME"
echo "Cargo: $(which cargo)"
ls -la ~/.cargo/bin/ | head -5
'
```

El contenedor ejecuta como usuario `dev` (no root). Los binarios de
Rust estan en `/home/dev/.cargo/bin/`.

---

## Parte 4 — SYS_PTRACE

### Objetivo

Demostrar por que gdb, strace y valgrind necesitan la capability
SYS_PTRACE para funcionar dentro de un contenedor.

### Paso 4.1: gdb sin SYS_PTRACE

```bash
docker run --rm lab-debian-dev bash -c '
cat > /tmp/test.c << EOF
#include <stdio.h>
int main(void) { int x = 42; printf("%d\n", x); return 0; }
EOF
gcc -g -o /tmp/test /tmp/test.c
echo -e "break main\nrun\ncontinue\nquit" | gdb -batch /tmp/test 2>&1 | tail -5
'
```

Antes de ejecutar, predice: funcionara gdb sin SYS_PTRACE?

Sin SYS_PTRACE, gdb puede fallar con "Could not trace the inferior
process" o "Operation not permitted", dependiendo de la configuracion
de seguridad del host.

### Paso 4.2: gdb con SYS_PTRACE

```bash
docker run --rm --cap-add SYS_PTRACE lab-debian-dev bash -c '
cat > /tmp/test.c << EOF
#include <stdio.h>
int main(void) { int x = 42; printf("%d\n", x); return 0; }
EOF
gcc -g -o /tmp/test /tmp/test.c
echo -e "break main\nrun\nprint x\ncontinue\nquit" | gdb -batch /tmp/test 2>&1 | tail -10
'
```

Con `--cap-add SYS_PTRACE`, gdb puede poner breakpoints, inspeccionar
variables y controlar la ejecucion. Deberia mostrar `$1 = 42`.

### Paso 4.3: strace con y sin SYS_PTRACE

```bash
echo "=== Sin SYS_PTRACE ==="
docker run --rm lab-debian-dev strace -c echo hello 2>&1 | tail -3

echo ""
echo "=== Con SYS_PTRACE ==="
docker run --rm --cap-add SYS_PTRACE lab-debian-dev strace -c echo hello 2>&1 | tail -10
```

Con SYS_PTRACE, strace muestra la tabla de syscalls. Sin el, falla.

### Paso 4.4: valgrind

```bash
docker run --rm --cap-add SYS_PTRACE lab-debian-dev bash -c '
cat > /tmp/leak.c << EOF
#include <stdlib.h>
int main(void) {
    int *p = malloc(100 * sizeof(int));
    p[0] = 42;
    /* leak: no free(p) */
    return 0;
}
EOF
gcc -g -o /tmp/leak /tmp/leak.c
valgrind --leak-check=full /tmp/leak 2>&1 | tail -10
'
```

Valgrind detecta el memory leak: "100 bytes in 1 blocks are definitely
lost". Esta herramienta es fundamental para el desarrollo C del curso.

---

## Limpieza final

```bash
docker rmi lab-debian-dev
```

---

## Conceptos reforzados

1. El Dockerfile combina `apt-get update` + `install` + limpieza en un
   solo `RUN` para evitar capas intermedias con archivos temporales.

2. Las **man pages** de Docker Debian estan excluidas por defecto. Hay
   que revertir la exclusion **antes** de instalar paquetes.

3. Rust se instala como usuario non-root (`dev`) para que los archivos
   de rustup y cargo pertenezcan al usuario correcto.

4. `--no-install-recommends` reduce ~200MB eliminando paquetes
   recomendados pero no necesarios.

5. **SYS_PTRACE** es necesario para gdb, strace y valgrind dentro de
   contenedores. En el compose.yml del curso se declara con
   `cap_add: SYS_PTRACE`.
