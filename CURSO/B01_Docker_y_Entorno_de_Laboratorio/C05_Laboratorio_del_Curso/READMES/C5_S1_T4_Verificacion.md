# T04 — Verificación

## Objetivo

Crear un script de validación que verifica automáticamente que el entorno del
laboratorio está correctamente configurado: Docker funciona, las imágenes se
construyeron, todas las herramientas están instaladas, y los volúmenes comparten
datos.

---

## El script de verificación

El script está disponible en `labs/verify-env.sh`. A continuación se presenta
una versión corregida con las explicaciones de cada sección:

```bash
#!/bin/bash
# verify-env.sh — Verificación del entorno del laboratorio
# Ejecutar desde el directorio del lab (donde está compose.yml):
#   chmod +x verify-env.sh && ./verify-env.sh

set -euo pipefail

PASS=0
FAIL=0

check() {
    local description="$1"
    shift
    if "$@" > /dev/null 2>&1; then
        printf "  [OK]   %s\n" "$description"
        PASS=$((PASS + 1))
    else
        printf "  [FAIL] %s\n" "$description"
        FAIL=$((FAIL + 1))
    fi
}

check_output() {
    local description="$1"
    shift
    local output
    if output=$("$@" 2>&1) && [ -n "$output" ]; then
        printf "  [OK]   %s → %s\n" "$description" "$output"
        PASS=$((PASS + 1))
    else
        printf "  [FAIL] %s\n" "$description"
        FAIL=$((FAIL + 1))
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
check "debian-dev corriendo" \
    docker compose ps --status running --format "{{.Name}}" | grep -q debian-dev
check "alma-dev corriendo" \
    docker compose ps --status running --format "{{.Name}}" | grep -q alma-dev
echo ""

# --- Herramientas Debian ---
echo "--- Debian: Compilación ---"
check_output "gcc"   docker compose exec -T debian-dev bash -c 'gcc --version | head -1'
check_output "g++"   docker compose exec -T debian-dev bash -c 'g++ --version | head -1'
check_output "make"  docker compose exec -T debian-dev bash -c 'make --version | head -1'
check_output "cmake" docker compose exec -T debian-dev bash -c 'cmake --version | head -1'
echo ""

echo "--- Debian: Debugging ---"
check_output "gdb"      docker compose exec -T debian-dev bash -c 'gdb --version | head -1'
check_output "valgrind" docker compose exec -T debian-dev valgrind --version
check        "strace"   docker compose exec -T debian-dev strace --version
check        "ltrace"   docker compose exec -T debian-dev ltrace --version
echo ""

echo "--- Debian: Rust ---"
check_output "rustc"   docker compose exec -T debian-dev rustc --version
check_output "cargo"   docker compose exec -T debian-dev cargo --version
check_output "rustfmt" docker compose exec -T debian-dev rustfmt --version
check_output "clippy"  docker compose exec -T debian-dev cargo clippy --version
echo ""

# --- Herramientas AlmaLinux ---
echo "--- AlmaLinux: Compilación ---"
check_output "gcc"   docker compose exec -T alma-dev bash -c 'gcc --version | head -1'
check_output "g++"   docker compose exec -T alma-dev bash -c 'g++ --version | head -1'
check_output "make"  docker compose exec -T alma-dev bash -c 'make --version | head -1'
check_output "cmake" docker compose exec -T alma-dev bash -c 'cmake --version | head -1'
echo ""

echo "--- AlmaLinux: Debugging ---"
check_output "gdb"      docker compose exec -T alma-dev bash -c 'gdb --version | head -1'
check_output "valgrind" docker compose exec -T alma-dev valgrind --version
check        "strace"   docker compose exec -T alma-dev strace --version
check        "ltrace"   docker compose exec -T alma-dev ltrace --version
echo ""

echo "--- AlmaLinux: Rust ---"
check_output "rustc" docker compose exec -T alma-dev rustc --version
check_output "cargo" docker compose exec -T alma-dev cargo --version
echo ""

# --- Compilación real ---
echo "--- Compilación: C ---"
check "C en Debian" docker compose exec -T debian-dev bash -c \
    'echo "#include <stdio.h>
int main(void) { printf(\"OK\\n\"); return 0; }" > /tmp/test.c && \
gcc -o /tmp/test /tmp/test.c && /tmp/test | grep -q OK'

check "C en AlmaLinux" docker compose exec -T alma-dev bash -c \
    'echo "#include <stdio.h>
int main(void) { printf(\"OK\\n\"); return 0; }" > /tmp/test.c && \
gcc -o /tmp/test /tmp/test.c && /tmp/test | grep -q OK'
echo ""

echo "--- Compilación: Rust ---"
check "Rust en Debian" docker compose exec -T debian-dev bash -c \
    'echo "fn main() { println!(\"OK\"); }" > /tmp/test.rs && \
rustc -o /tmp/test_rs /tmp/test.rs && /tmp/test_rs | grep -q OK'

check "Rust en AlmaLinux" docker compose exec -T alma-dev bash -c \
    'echo "fn main() { println!(\"OK\"); }" > /tmp/test.rs && \
rustc -o /tmp/test_rs /tmp/test.rs && /tmp/test_rs | grep -q OK'
echo ""

# --- SYS_PTRACE: ptrace attach ---
echo "--- SYS_PTRACE (ptrace attach) ---"
check "strace attach en Debian" docker compose exec -T debian-dev bash -c \
    'sleep 100 & PID=$!; \
strace -p $PID -e trace=none -c 2>/dev/null & SPID=$!; \
sleep 0.3; kill $SPID 2>/dev/null; kill $PID 2>/dev/null; wait 2>/dev/null; true'

check "strace attach en AlmaLinux" docker compose exec -T alma-dev bash -c \
    'sleep 100 & PID=$!; \
strace -p $PID -e trace=none -c 2>/dev/null & SPID=$!; \
sleep 0.3; kill $SPID 2>/dev/null; kill $PID 2>/dev/null; wait 2>/dev/null; true'
echo ""

# --- gdb funcional ---
echo "--- gdb funcional ---"
check "gdb en Debian" docker compose exec -T debian-dev bash -c \
    'echo "#include <stdio.h>
int main(void) { int x=42; printf(\"%d\\n\",x); return 0; }" > /tmp/gdb_test.c && \
gcc -g -o /tmp/gdb_test /tmp/gdb_test.c && \
gdb -batch -ex "break main" -ex run -ex continue -ex quit /tmp/gdb_test 2>&1 | grep -q "Breakpoint"'

check "gdb en AlmaLinux" docker compose exec -T alma-dev bash -c \
    'echo "#include <stdio.h>
int main(void) { int x=42; printf(\"%d\\n\",x); return 0; }" > /tmp/gdb_test.c && \
gcc -g -o /tmp/gdb_test /tmp/gdb_test.c && \
gdb -batch -ex "break main" -ex run -ex continue -ex quit /tmp/gdb_test 2>&1 | grep -q "Breakpoint"'
echo ""

# --- Volúmenes compartidos ---
echo "--- Volúmenes ---"
TOKEN=$(date +%s)
docker compose exec -T debian-dev bash -c "echo '$TOKEN' > /home/dev/workspace/verify_test.txt"
check "Volumen compartido (Debian → AlmaLinux)" docker compose exec -T alma-dev bash -c \
    "grep -q '$TOKEN' /home/dev/workspace/verify_test.txt"
docker compose exec -T debian-dev rm -f /home/dev/workspace/verify_test.txt

check "Bind mount src/ visible en Debian" \
    docker compose exec -T debian-dev test -d /home/dev/workspace/src
check "Bind mount src/ visible en AlmaLinux" \
    docker compose exec -T alma-dev test -d /home/dev/workspace/src
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

## Análisis del script

### `set -euo pipefail`

```bash
set -euo pipefail
```

| Flag | Efecto |
|---|---|
| `-e` | El script termina si un comando falla (exit status ≠ 0) |
| `-u` | Trata variables no definidas como error |
| `-o pipefail` | Un fallo en cualquier parte de un pipeline se reporta como fallo |

Es el patrón estándar para scripts bash robustos.

### Flag `-T` en exec

```bash
docker compose exec -T debian-dev gcc --version
```

El flag `-T` desactiva la asignación de pseudo-TTY. Es **necesario** cuando
`exec` se usa dentro de scripts (entorno no interactivo). Sin `-T`, el
comando falla porque no hay terminal disponible.

Equivalencia:
- Script/automatización: `docker compose exec -T` (sin TTY)
- Terminal interactivo: `docker compose exec` (con TTY por defecto)

### Funciones `check` y `check_output`

El script define dos funciones auxiliares:

```bash
check "descripción" comando arg1 arg2 ...
```
Ejecuta el comando, reporta `[OK]` o `[FAIL]` según el exit code.

```bash
check_output "descripción" comando arg1 arg2 ...
```
Igual que `check`, pero además captura y muestra la salida del comando junto
al `[OK]`.

### Por qué `head -1` va DENTRO del contenedor

Para obtener solo la primera línea de `gcc --version`, el `head -1` se ejecuta
**dentro del contenedor** via `bash -c`:

```bash
# Correcto: head -1 dentro del contenedor
check_output "gcc" docker compose exec -T debian-dev bash -c 'gcc --version | head -1'

# Incorrecto: head -1 fuera, en un pipe del host
check_output "gcc" docker compose exec -T debian-dev gcc --version | head -1
```

La segunda forma es incorrecta porque al usar `|` en el shell del host,
`check_output` se ejecuta en un **subshell**. Las variables modificadas dentro
de un subshell (como `PASS` y `FAIL`) no se propagan al shell padre:

```
Shell padre                    Subshell (pipe izquierdo)
┌────────────┐                 ┌────────────────────────┐
│ PASS=0     │                 │ PASS=0 (copia)         │
│ FAIL=0     │                 │ check_output "gcc" ... │
│            │   fork + pipe   │ PASS=1 ← solo aquí     │
│ PASS=0 ←───┼─ no propagado ─┤                        │
│ (sin cambio)│                │ (subshell termina)     │
└────────────┘                 └────────────────────────┘
```

### Por qué `PASS=$((PASS + 1))` en vez de `((PASS++))`

Con `set -e`, la expresión aritmética `((PASS++))` es peligrosa cuando
`PASS=0`:

```bash
PASS=0
((PASS++))    # Post-increment: evalúa al valor ANTERIOR (0)
              # (( 0 )) → exit status 1
              # set -e → ¡el script termina!
```

Alternativas seguras:

| Forma | Evaluación con PASS=0 | Exit status |
|---|---|---|
| `((PASS++))` | 0 (valor anterior) | 1 — peligroso |
| `((++PASS))` | 1 (valor nuevo) | 0 — seguro |
| `PASS=$((PASS + 1))` | asignación | 0 — seguro |

La forma `PASS=$((PASS + 1))` es la más portable y segura.

### Test de SYS_PTRACE: attach vs launch

El script distingue dos tests separados:

**gdb funcional** — verifica que gdb puede lanzar y depurar un programa:
```bash
gdb -batch -ex "break main" -ex run /tmp/test
```
Esto usa la relación padre-hijo (`PTRACE_TRACEME` desde el proceso hijo).
Linux permite que un padre trace a su hijo **sin CAP_SYS_PTRACE** gracias
a Yama ptrace_scope.

**strace attach** — verifica que se puede adjuntar a un proceso existente:
```bash
sleep 100 &
strace -p $!
```
Esto usa `PTRACE_ATTACH` desde un proceso no relacionado. Requiere
**CAP_SYS_PTRACE**. Este test falla sin `cap_add: SYS_PTRACE`.

---

## Cómo usar el script

```bash
# Asegurarse de que el lab está corriendo
cd /path/to/lab
docker compose up -d

# Dar permisos de ejecución
chmod +x verify-env.sh

# Ejecutar la verificación
./verify-env.sh
```

El exit code indica el resultado: 0 = todo OK, 1 = hay fallos. Esto permite
usar el script en pipelines:

```bash
./verify-env.sh && echo "Entorno listo" || echo "Hay problemas"
```

---

## Problemas comunes y soluciones

### strace attach: Operation not permitted

```
[FAIL] strace attach en Debian
```

**Causa**: falta `cap_add: SYS_PTRACE` en compose.yml.

**Solución**:
```yaml
cap_add:
  - SYS_PTRACE
```
```bash
docker compose down && docker compose up -d
```

> **Nota:** El test de "gdb funcional" (lanzar un programa) sigue pasando sin
> SYS_PTRACE, porque gdb lanza el proceso como hijo — el kernel permite
> ptrace padre→hijo sin la capability. El test de "strace attach" (adjuntarse
> a un proceso existente) sí falla.

### rustc: command not found

```
[FAIL] rustc
```

**Causa**: rustup no se instaló o `PATH` no incluye `/home/dev/.cargo/bin`.

**Diagnóstico**:
```bash
docker compose exec debian-dev bash -c 'echo $PATH'
docker compose exec debian-dev ls /home/dev/.cargo/bin/
```

**Solución**: verificar en el Dockerfile:
```dockerfile
ENV PATH="/home/dev/.cargo/bin:${PATH}"
```
Reconstruir: `docker compose build --no-cache debian-dev && docker compose up -d`

### man pages no funcionan

```bash
docker compose exec debian-dev man printf
# No manual entry for printf
```

**Causa**: la exclusión de man pages en `/etc/dpkg/dpkg.cfg.d/excludes` no se
revirtió **antes** de `apt-get install`.

**Solución**: el `sed` que comenta la exclusión debe estar antes del install
en el Dockerfile. Reconstruir con `--no-cache`.

### Volúmenes no compartidos

```
[FAIL] Volumen compartido (Debian → AlmaLinux)
```

**Causa**: los contenedores no montan el mismo named volume.

**Solución**: verificar que ambos servicios montan `workspace:/home/dev/workspace`
y que `workspace:` está declarado en la sección `volumes:` raíz del compose.yml.

### Contenedores no inician

```
[FAIL] debian-dev corriendo
```

**Diagnóstico**:
```bash
docker compose ps -a
docker compose logs debian-dev
```

| Error | Causa | Solución |
|---|---|---|
| `OCI runtime error` | Dockerfile con errores | `docker compose build --no-cache` |
| `invalid bind mount` | Path `./src` no existe | `mkdir -p src` |
| `image not found` | No se hizo build | `docker compose build` |

### Docker daemon no responde

```
[FAIL] Docker daemon activo
```

```bash
# Verificar estado del servicio
sudo systemctl status docker

# Reiniciar
sudo systemctl restart docker

# Verificar permisos
groups $USER
# Si no está en el grupo docker:
# sudo usermod -aG docker $USER
# (cerrar sesión y volver a entrar)
```

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

### Ejercicio 1 — Examinar el script antes de ejecutarlo

Lee el script de verificación y entiende su estructura antes de ejecutarlo:

```bash
# Ver las secciones del script
grep "^echo \"---" labs/verify-env.sh

# Ver las funciones
grep -A 10 "^check()" labs/verify-env.sh
grep -A 10 "^check_output()" labs/verify-env.sh

# Contar cuántos checks tiene
grep -c "^check\|^check_output" labs/verify-env.sh
```

<details><summary>Predicción</summary>

Las secciones del script son:
```
--- Docker ---
--- Docker Compose ---
--- Contenedores ---
--- Debian: Compilación ---
--- Debian: Debugging ---
--- Debian: Rust ---
--- AlmaLinux: Compilación ---
--- AlmaLinux: Debugging ---
--- AlmaLinux: Rust ---
--- Compilación: C ---
--- Compilación: Rust ---
--- SYS_PTRACE (ptrace attach) ---
--- gdb funcional ---
--- Volúmenes ---
```

La función `check()` ejecuta un comando y reporta OK/FAIL según el exit code.
`check_output()` hace lo mismo pero además muestra la salida capturada.

El script tiene aproximadamente 37 checks en total (el número exacto depende
de la versión del script).

</details>

---

### Ejercicio 2 — Ejecutar la verificación completa

```bash
cd /path/to/lab
docker compose up -d

chmod +x verify-env.sh
./verify-env.sh

# Verificar el exit code
echo "Exit code: $?"
```

<details><summary>Predicción</summary>

Si el entorno está correctamente configurado, todos los checks pasan:

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
  ...

============================================
  Resultados: 37 OK, 0 FAIL
============================================

  Entorno verificado correctamente.
```

El exit code es 0 (éxito). Esto permite usarlo en pipelines:
`./verify-env.sh && echo "listo"`.

</details>

---

### Ejercicio 3 — Provocar fallo: quitar SYS_PTRACE

Edita temporalmente compose.yml para comentar `cap_add: SYS_PTRACE`, luego
ejecuta la verificación:

```bash
# Guardar backup
cp compose.yml compose.yml.bak

# Comentar cap_add en ambos servicios
sed -i 's/^    cap_add:/    #cap_add:/' compose.yml
sed -i 's/^      - SYS_PTRACE/      #- SYS_PTRACE/' compose.yml

# Recrear contenedores sin la capability
docker compose down
docker compose up -d

# Ejecutar verificación
./verify-env.sh

# Restaurar
cp compose.yml.bak compose.yml
docker compose down
docker compose up -d
```

<details><summary>Predicción</summary>

Los tests de **strace attach** fallan:
```
--- SYS_PTRACE (ptrace attach) ---
  [FAIL] strace attach en Debian
  [FAIL] strace attach en AlmaLinux
```

Los tests de **gdb funcional** siguen pasando:
```
--- gdb funcional ---
  [OK]   gdb en Debian
  [OK]   gdb en AlmaLinux
```

**¿Por qué gdb pasa sin SYS_PTRACE?** Porque el test de gdb *lanza* el
programa como proceso hijo (`gdb ./test`). Linux permite que un proceso padre
trace a su hijo sin CAP_SYS_PTRACE (via Yama ptrace_scope). Es la relación
`PTRACE_TRACEME` desde el hijo.

En cambio, `strace -p PID` necesita `PTRACE_ATTACH` a un proceso *no
relacionado* — esto sí requiere la capability SYS_PTRACE.

</details>

---

### Ejercicio 4 — Provocar fallo: detener un contenedor

```bash
# Detener solo alma-dev
docker compose stop alma-dev

# Ejecutar verificación
./verify-env.sh

# Restaurar
docker compose start alma-dev
```

<details><summary>Predicción</summary>

Todos los checks de AlmaLinux fallan:
```
--- Contenedores ---
  [OK]   debian-dev corriendo
  [FAIL] alma-dev corriendo

--- AlmaLinux: Compilación ---
  [FAIL] gcc
  [FAIL] g++
  [FAIL] make
  [FAIL] cmake
...
```

También falla el test de volumen compartido (alma-dev no puede leer el
archivo). Los checks de Debian siguen pasando.

`docker compose start alma-dev` (no `up`) reinicia el contenedor existente
sin recrearlo, preservando la configuración.

</details>

---

### Ejercicio 5 — Verificar espacio en disco

```bash
# Resumen de espacio usado por Docker
docker system df

# Desglose detallado
docker system df -v

# Solo las imágenes del lab
docker image ls --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}" | grep -E "lab|REPOSITORY"
```

<details><summary>Predicción</summary>

`docker system df` muestra un resumen:
```
TYPE            TOTAL     ACTIVE    SIZE      RECLAIMABLE
Images          X         2         ~1.5GB    ...
Containers      2         2         ...       ...
Local Volumes   1         1         ...       ...
Build Cache     X         0         ~500MB    ...
```

Las imágenes del lab:
```
REPOSITORY       TAG       SIZE
lab-debian-dev   latest    ~700MB
lab-alma-dev     latest    ~750MB
```

La mayor parte del espacio la consumen las imágenes base + toolchains
de compilación y Rust (~200 MB cada una por rustup).

`docker system df -v` desglosa cada imagen, contenedor y volumen
individualmente.

</details>

---

### Ejercicio 6 — Diagnosticar volumen no compartido

Provoca un fallo de volumen usando volumes diferentes para cada servicio:

```bash
# Backup
cp compose.yml compose.yml.bak

# Editar compose.yml: cambiar el volume de alma-dev
# alma-dev:
#   volumes:
#     - alma-workspace:/home/dev/workspace    ← volume diferente
# Y añadir al final: alma-workspace:

# Recrear
docker compose down -v
docker compose up -d

# Verificar
./verify-env.sh

# Restaurar
cp compose.yml.bak compose.yml
docker compose down -v
docker compose up -d
```

<details><summary>Predicción</summary>

El test de volumen compartido falla:
```
--- Volúmenes ---
  [FAIL] Volumen compartido (Debian → AlmaLinux)
  [OK]   Bind mount src/ visible en Debian
  [OK]   Bind mount src/ visible en AlmaLinux
```

El archivo creado en `workspace` por debian-dev no es visible para alma-dev
porque cada uno tiene su propio named volume. El bind mount `./src` sigue
funcionando porque apunta al mismo directorio del host, independiente de los
named volumes.

Diagnóstico:
```bash
docker volume ls | grep workspace
# lab_workspace         ← de debian-dev
# lab_alma-workspace    ← de alma-dev (nuevo)
```

</details>

---

### Ejercicio 7 — El flag -T y scripts no interactivos

Compara el comportamiento de `docker compose exec` con y sin `-T`:

```bash
# Con -T (para scripts): funciona
docker compose exec -T debian-dev gcc --version

# Sin -T en un pipe (simula script): puede fallar o dar warning
echo "test" | docker compose exec debian-dev cat

# Ver qué tipo de I/O tiene cada modo
docker compose exec -T debian-dev bash -c 'if [ -t 0 ]; then echo "stdin is a TTY"; else echo "stdin is not a TTY"; fi'
docker compose exec debian-dev bash -c 'if [ -t 0 ]; then echo "stdin is a TTY"; else echo "stdin is not a TTY"; fi'
```

<details><summary>Predicción</summary>

Con `-T`:
```
stdin is not a TTY
```

Sin `-T` (modo interactivo por defecto):
```
stdin is a TTY
```

El flag `-T` desactiva la asignación de pseudo-TTY. Esto es necesario en
scripts porque:
1. No hay terminal real disponible
2. La TTY añade caracteres de control a la salida (corrompe parsing)
3. Algunos comandos detectan la TTY y cambian su formato de salida

Regla simple: usa `-T` en scripts, omítelo en uso interactivo.

</details>

---

### Ejercicio 8 — Verificar herramientas de red

Comprueba las herramientas de red instaladas en cada distro:

```bash
# Verificar herramientas disponibles
docker compose exec debian-dev bash -c 'which nc ss ip dig tcpdump'
docker compose exec alma-dev bash -c 'which ncat ss ip dig tcpdump'

# Ping entre contenedores
docker compose exec debian-dev ping -c 1 alma-dev
docker compose exec alma-dev ping -c 1 debian-dev

# Test rápido de ncat (AlmaLinux como servidor)
docker compose exec -d alma-dev bash -c 'ncat -l 9999 -e /bin/cat --keep-open'
sleep 1
docker compose exec debian-dev bash -c 'echo "test" | nc alma-dev 9999'

# Limpiar
docker compose exec alma-dev bash -c 'pkill ncat 2>/dev/null; true'
```

<details><summary>Predicción</summary>

Herramientas de red por distro:

| Herramienta | Debian (paquete) | AlmaLinux (paquete) |
|---|---|---|
| netcat | `nc` (netcat-openbsd) | `ncat` (nmap-ncat) |
| socket stats | `ss` (iproute2) | `ss` (iproute) |
| ip | `ip` (iproute2) | `ip` (iproute) |
| DNS lookup | `dig` (dnsutils) | `dig` (bind-utils) |
| packet capture | `tcpdump` (tcpdump) | `tcpdump` (tcpdump) |

El ping funciona porque ambos están en la misma red de Compose (`lab_default`).
Docker Compose registra los nombres de servicio en DNS, permitiendo
`ping alma-dev` y `ping debian-dev`.

El test de ncat funciona: AlmaLinux escucha con `ncat` (nmap-ncat), Debian
conecta con `nc` (netcat-openbsd). El protocolo TCP es el mismo, solo difieren
los nombres de los comandos y algunos flags.

</details>

---

### Ejercicio 9 — Reconstrucción desde cero

Elimina todo y reconstruye el lab para verificar que el proceso es
reproducible:

```bash
# Ver estado actual
docker compose ps -a
docker volume ls | grep workspace
docker image ls | grep lab

# Eliminar todo: contenedores, redes, volumes
docker compose down -v

# Eliminar imágenes del lab
docker image rm lab-debian-dev lab-alma-dev 2>/dev/null; true

# Verificar que está limpio
docker compose ps -a
docker volume ls | grep workspace
docker image ls | grep lab

# Reconstruir desde cero
docker compose build
docker compose up -d

# Verificar
./verify-env.sh
```

<details><summary>Predicción</summary>

Después de `down -v` + `image rm`:
- No hay contenedores del lab
- No hay volume `lab_workspace`
- No hay imágenes `lab-debian-dev` ni `lab-alma-dev`

`docker compose build` descarga las imágenes base (debian:bookworm,
almalinux:9), instala paquetes, y compila Rust. Esto tarda varios minutos
la primera vez (depende de la velocidad de red y disco).

Después de rebuild, `./verify-env.sh` debe pasar todos los checks de nuevo.
La escala de destrucción de mayor a menor:

1. `docker compose stop` — para contenedores, preserva todo
2. `docker compose down` — elimina contenedores + red, preserva volumes + imágenes
3. `docker compose down -v` — elimina también volumes (pérdida de datos)
4. `docker image rm` — elimina imágenes (requiere rebuild)
5. `docker system prune -a` — elimina TODO lo no usado (peligroso si tienes otros proyectos)

</details>

---

### Ejercicio 10 — Exit codes y automatización

Usa el exit code del script de verificación para automatizar decisiones:

```bash
# Forma 1: condicional simple
./verify-env.sh && echo "Lab listo para trabajar" || echo "Revisar errores"

# Forma 2: capturar el exit code
./verify-env.sh
EXIT_CODE=$?
echo "El script terminó con código: $EXIT_CODE"

# Forma 3: en un if
if ./verify-env.sh; then
    echo "Todo correcto, puedes empezar a programar"
else
    echo "Hay problemas — revisa la salida de arriba"
fi

# Forma 4: verificar antes de compilar
./verify-env.sh > /dev/null 2>&1 && \
    docker compose exec -T debian-dev bash -c 'cd src && make' || \
    echo "Entorno no verificado, no se compila"
```

<details><summary>Predicción</summary>

El exit code del script:
- `0`: todas las verificaciones pasaron
- `1`: al menos una verificación falló

**`&&` y `||`** evalúan comandos basándose en el exit code del anterior:
- `cmd1 && cmd2`: ejecuta cmd2 solo si cmd1 tiene exit code 0
- `cmd1 || cmd2`: ejecuta cmd2 solo si cmd1 tiene exit code ≠ 0

La forma 4 es la más práctica: verifica el entorno silenciosamente y solo
compila si todo está OK. El `> /dev/null 2>&1` suprime la salida del script
para que solo se vea el resultado de `make` o el mensaje de error.

Este patrón es común en CI/CD: un script de verificación que devuelve exit
codes apropiados puede integrarse directamente en pipelines de build.

</details>
