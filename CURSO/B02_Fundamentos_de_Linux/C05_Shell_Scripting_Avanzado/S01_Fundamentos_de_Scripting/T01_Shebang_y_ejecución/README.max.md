# T01 — Shebang y ejecución

## Qué es el shebang

El **shebang** (también hashbang) es la primera línea de un script que indica
al kernel qué intérprete debe usarse para ejecutarlo:

```bash
#!/bin/bash
echo "Hola desde bash"
```

```
#!  → "magic number" que el kernel reconoce
/bin/bash → ruta al intérprete

Cuando ejecutas: ./script.sh
El kernel lee los primeros bytes, ve #! y ejecuta:
  /bin/bash ./script.sh
```

Sin shebang, el kernel no sabe qué intérprete usar. Si se ejecuta con
`./script.sh`, el shell actual intenta interpretarlo (puede funcionar o no).
Con `bash script.sh`, se fuerza bash — pero el shebang es la forma correcta
y explícita.

---

## #!/bin/bash vs #!/usr/bin/env bash

### Ruta absoluta

```bash
#!/bin/bash
```

| Aspecto | Detalle |
|---|---|
| Ventaja | Directo, sin ambigüedad |
| Problema | Asume bash está en `/bin/bash` |
| Sistemas problemáticos | FreeBSD, NixOS, contenedores mínimos |

### env lookup

```bash
#!/usr/bin/env bash
```

`env` busca `bash` en el `PATH` del usuario y ejecuta el primero que encuentra:

```
#!/usr/bin/env bash
  → env busca bash en PATH
  → Encuentra /bin/bash (o /usr/local/bin/bash, o donde esté)
  → Ejecuta ese bash con el script
```

| Aspecto | Detalle |
|---|---|
| Ventaja | Funciona sin importar dónde esté instalado bash |
| Desventaja | Depende del PATH (puede ejecutar bash diferente) |

### Cuándo usar cada uno

| Situación | Shebang recomendado |
|---|---|
| Scripts de sistema (init, cron) | `#!/bin/bash` (ruta fija y conocida) |
| Scripts portables (múltiples distros) | `#!/usr/bin/env bash` |
| Scripts de Python | `#!/usr/bin/env python3` |
| Scripts POSIX (máxima portabilidad) | `#!/bin/sh` |

---

## sh vs bash

```bash
#!/bin/sh
# sh es POSIX shell — funcionalidad mínima garantizada
# En Debian: sh → dash (no bash)
# En RHEL: sh → bash

# Si usas features de bash (arrays, [[ ]], ${var//pat/rep}),
# el script FALLARÁ con #!/bin/sh en Debian porque dash no las soporta

ls -la /bin/sh
# Debian: lrwxrwxrwx 1 root root 4 ... /bin/sh -> dash
# RHEL:   lrwxrwxrwx 1 root root 4 ... /bin/sh -> bash
```

**Regla:** si usas cualquier feature de bash (y lo harás), usar `#!/bin/bash` o
`#!/usr/bin/env bash`. Usar `#!/bin/sh` solo para scripts POSIX puros.

### Dash como /bin/sh

| Distribución | Shell en /bin/sh | Notas |
|---|---|---|
| Debian/Ubuntu | dash | Más rápido, menor memoria, POSIX estricto |
| RHEL/CentOS/Fedora | bash | Por defecto |
| Alpine | busybox ash | Contenedores |
| Arch | bash | |

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

# Alternativa: ejecutar explícitamente con el intérprete (no necesita +x)
bash script.sh
# Funciona (no necesita chmod +x)
```

### Permisos típicos para scripts

| Permiso | Octal | rwx | Uso |
|---|---|---|---|
| Público | `755` | `rwxr-xr-x` | Scripts de uso general |
| Solo owner | `700` | `rwx------` | Scripts privados con datos sensibles |
| Owner + grupo | `750` | `rwxr-x---` | Scripts compartidos en grupo |

---

## Ejecución: ./script vs script vs source

### ./script.sh — Subshell

```bash
./script.sh
# 1. El kernel lee el shebang
# 2. Crea un nuevo proceso (fork + exec)
# 3. El script ejecuta en un SUBSHELL (proceso hijo)
# 4. Las variables del script NO afectan al shell padre

# Ejemplo:
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

### bash script.sh — Sin shebang

```bash
bash script.sh
# Ejecuta el script con bash explícitamente
# No necesita permiso de ejecución
# No necesita shebang (se ignora)
# Crea un subshell (como ./)
```

### Resumen comparativo

| Forma | Subshell | Necesita +x | Usa shebang | Variables persisten |
|---|---|---|---|---|
| `./script.sh` | Sí | Sí | Sí | No |
| `bash script.sh` | Sí | No | No (se ignora) | No |
| `source script.sh` | No | No | No (se ignora) | Sí |

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
sudo ln -s /path/to/script.sh /usr/local/bin/script
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
# No todos los sistemas soportan env -S (GNU coreutils 8.30+)
```

**En la práctica**, es más claro poner las opciones dentro del script:

```bash
#!/usr/bin/env bash
set -euo pipefail
# Más portable que ponerlas en el shebang
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
cualquier intérprete.

---

## Ejercicios

### Ejercicio 1 — Shebang y ejecución

```bash
# Crear un script
cat > /tmp/test-shebang.sh << 'EOF'
#!/bin/bash
echo "Shell: $BASH"
echo "PID: $$"
echo "Parent PID: $PPID"
EOF
chmod +x /tmp/test-shebang.sh

# Ejecutar de tres formas
echo "=== ./script ==="
/tmp/test-shebang.sh

echo "=== bash script ==="
bash /tmp/test-shebang.sh

echo "=== source script ==="
source /tmp/test-shebang.sh

# ¿Cuál tiene PID diferente? ¿Cuál comparte PID con tu shell?
# Respuesta: ./ y bash crean subshells (PID diferente del padre)
# source ejecuta en el shell actual (mismo PID)

rm /tmp/test-shebang.sh
```

---

### Ejercicio 2 — source vs subshell

```bash
cat > /tmp/test-source.sh << 'EOF'
#!/bin/bash
TEST_VAR="valor_del_script"
INNER_DIR=/tmp
cd $INNER_DIR
EOF

# Con subshell
bash /tmp/test-source.sh
echo "Subshell: TEST_VAR=$TEST_VAR"        # Vacío
pwd                                              # Directorio original

# Con source
source /tmp/test-source.sh
echo "Source: TEST_VAR=$TEST_VAR"              # valor_del_script
pwd                                              # /tmp (cambió)

unset TEST_VAR
rm /tmp/test-source.sh
```

---

### Ejercicio 3 — sh vs bash

```bash
# ¿A qué apunta /bin/sh en tu sistema?
ls -la /bin/sh
file /bin/sh

# Crear un script con feature de bash
cat > /tmp/test-sh.sh << 'EOF'
#!/bin/sh
arr=(uno dos tres)
echo "${arr[1]}"
EOF
chmod +x /tmp/test-sh.sh

# ¿Funciona con sh?
/tmp/test-sh.sh 2>&1 || echo "Falló con sh"

# Cambiar a bash
sed -i '1s|.*|#!/bin/bash|' /tmp/test-sh.sh
/tmp/test-sh.sh

rm /tmp/test-sh.sh
```

---

### Ejercicio 4 — Shebang con opciones

```bash
cat > /tmp/test-opts.sh << 'EOF'
#!/bin/bash -e
echo "Paso 1"
false
echo "Paso 2 (nunca se ejecuta)"
EOF
chmod +x /tmp/test-opts.sh

/tmp/test-opts.sh 2>&1 || echo "Script falló como esperado"

# Probar sin -e en shebang pero con set -e dentro
cat > /tmp/test-opts2.sh << 'EOF'
#!/bin/bash
set -e
echo "Paso 1"
false
echo "Paso 2 (nunca se ejecuta)"
EOF
chmod +x /tmp/test-opts2.sh

/tmp/test-opts2.sh 2>&1 || echo "Script falló como esperado"

rm /tmp/test-opts.sh /tmp/test-opts2.sh
```

---

### Ejercicio 5 — PATH yscripts globales

```bash
# Crear script de prueba
cat > /tmp/my-script << 'EOF'
#!/bin/bash
echo "Mi script se ejecutó"
EOF
chmod +x /tmp/my-script

# Opción 1: ejecutar con ruta directa
/tmp/my-script

# Opción 2: copiar a ~/bin
mkdir -p ~/bin
cp /tmp/my-script ~/bin/
chmod +x ~/bin/my-script

# Agregar al PATH (si no existe)
grep -q 'PATH=.*bin' ~/.bashrc || echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Ahora debería funcionar sin ruta
my-script

rm -rf ~/bin/my-script
rm /tmp/my-script
```

---

### Ejercicio 6 —Diferencia entre run y exec

```bash
# docker exec vs docker run es análogo a source vs subshell

# docker run = subshell (crea contenedor nuevo)
# docker exec = source (ejecuta en contenedor existente)
```

---

### Ejercicio 7 — Verificar shebang en scripts del sistema

```bash
# Ver shebangs comunes en scripts del sistema
head -1 /bin/ls
head -1 /usr/bin/grep
head -1 /usr/bin/sed
head -1 /usr/bin/awk

# Ver shebangs en scripts de cron
head -1 /etc/cron.daily/man-db 2>/dev/null || echo "No existe"
head -1 /etc/cron.daily/apt 2>/dev/null || echo "No existe"
```

---

### Ejercicio 8 — Crear script portable

```bash
# Crear script que funcione en cualquier distro
cat > /tmp/portable-script << 'EOF'
#!/usr/bin/env bash
# Este shebang funciona aunque bash esté en /bin o /usr/bin

echo "Bash location: $(which bash)"
echo "Versión: $BASH_VERSION"
EOF
chmod +x /tmp/portable-script

/tmp/portable-script

rm /tmp/portable-script
```

---

### Ejercicio 9 — Permisos y seguridad

```bash
# Crear script con permisos incorrectos
cat > /tmp/secret-script << 'EOF'
#!/bin/bash
echo "Datos secretos"
EOF
chmod +x /tmp/secret-script

# Ver permisos
ls -la /tmp/secret-script

# Hacerlo privado (solo el owner puede leer/ejecutar)
chmod 700 /tmp/secret-script
ls -la /tmp/secret-script

# Intentar ejecutar como otro usuario (si hay sudo)
# sudo -u nobody /tmp/secret-script 2>&1 || echo "Permiso denegado"

rm /tmp/secret-script
```

---

### Ejercicio 10 — Diagnóstico de scripts

```bash
# Script con problema: shebang incorrecto
cat > /tmp/bad-shebang << 'EOF'
# Sin shebang - el shell actual lo interpreta
echo "Esto funciona"
EOF
chmod +x /tmp/bad-shebang
/tmp/bad-shebang

# Ver qué pasa con stderr
cat > /tmp/bad-shebang2 << 'EOF'
# Intentando usar bashismo sin shebang
arr=(uno dos)
echo "${arr[0]}"
EOF
chmod +x /tmp/bad-shebang2
bash /tmp/bad-shebang2 2>&1 || echo "Error detectado"

rm /tmp/bad-shebang /tmp/bad-shebang2
```
