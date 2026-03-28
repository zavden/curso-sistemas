# T04 — umask

> **Fuentes**: `README.md`, `README.max.md`, `labs/README.md`
>
> **Correcciones aplicadas**:
> - Tabla "umask vs permisos" del max.md: filas `0090` y `0099` contenían el
>   dígito 9, que no es válido en octal (solo 0–7). `umask 090` produce error
>   en bash: `"value too great for base"`. Filas eliminadas.
> - Ejercicio 4 del max.md: `NOT(033) = 724` incorrecto — el complemento de
>   033 (000 011 011) es 744 (111 100 100), no 724. Corregido.
> - Ejercicio 7 del max.md: verificar umask de servicios systemd no funciona en
>   containers Docker (systemd no es PID 1). Marcado como ejercicio para
>   sistemas con systemd, no para los labs Docker.

---

## Qué es la umask

La umask (*user file-creation mode mask*) define qué permisos se **quitan** de
los archivos y directorios que un proceso crea. No establece permisos — los
filtra.

```bash
# Ver la umask actual
umask
# 0022

# Ver en formato simbólico (muestra lo que QUEDA, no lo que se quita)
umask -S
# u=rwx,g=rx,o=rx
```

`umask -S` muestra los permisos que un directorio **tendría** al crearse.
Es la inversión de la máscara: `u=rwx,g=rx,o=rx` significa "quito `w` de
group y others".

---

## Cómo se calcula

Cuando un programa crea un archivo o directorio, solicita unos permisos al
kernel. El kernel aplica la umask con una operación bitwise:

```
permisos_finales = permisos_solicitados AND NOT(umask)
```

La mayoría de programas solicitan `666` para archivos y `777` para directorios:

```
Archivo:    666 AND NOT(022) = 644
Directorio: 777 AND NOT(022) = 755
```

### Con umask `022` (default estándar)

| Tipo | Base solicitada | Umask | Resultado | Simbólico |
|---|---|---|---|---|
| Archivo | 666 | 022 | 644 | `rw-r--r--` |
| Directorio | 777 | 022 | 755 | `rwxr-xr-x` |

### Con umask `077` (privado)

| Tipo | Base | Umask | Resultado | Simbólico |
|---|---|---|---|---|
| Archivo | 666 | 077 | 600 | `rw-------` |
| Directorio | 777 | 077 | 700 | `rwx------` |

### Con umask `002` (colaborativo)

| Tipo | Base | Umask | Resultado | Simbólico |
|---|---|---|---|---|
| Archivo | 666 | 002 | 664 | `rw-rw-r--` |
| Directorio | 777 | 002 | 775 | `rwxrwxr-x` |

### Por qué archivos usan 666 y no 777

Los archivos **nunca** se crean con bit de ejecución — el base que solicitan
la mayoría de programas (`touch`, editores, etc.) es `666`, no `777`.
Esto es una protección: un archivo descargado o creado no es ejecutable
por defecto. Hay que agregar `x` explícitamente con `chmod +x`.

Incluso con `umask 000` (la más permisiva posible), `touch` crea archivos
con `666`, no `777`.

### La operación NO es resta aritmética

La umask usa `AND NOT` bitwise, no resta. Para umasks comunes (022, 077, 002)
la resta aritmética da el mismo resultado. Pero para umasks con bits impares:

```
# umask 033:
# Aritmética: 666 - 033 = 633 ← INCORRECTO
# Bitwise:    666 AND NOT(033) = 644 ← CORRECTO
#
# ¿Por qué? Desglose:
#   033 octal = 000 011 011 binario
#   NOT(033)  = 111 100 100 = 744 octal
#   666       = 110 110 110
#   AND       = 110 100 100 = 644
#
# 033 intenta quitar w+x de group y others
# Pero x no existe en el base 666, así que quitar x no hace nada
# Solo se quita w: 6→4 para group y 6→4 para others
```

**Regla práctica:** si algún dígito de la umask tiene bits que no existen en
el base (como quitar `x` de un archivo), esos bits se ignoran silenciosamente.

---

## Umasks comunes

| Umask | Archivos | Directorios | Uso típico |
|---|---|---|---|
| `022` | `644` rw-r--r-- | `755` rwxr-xr-x | Default estándar — todos pueden leer |
| `077` | `600` rw------- | `700` rwx------ | Privado — solo el owner |
| `002` | `664` rw-rw-r-- | `775` rwxrwxr-x | Colaborativo — group puede escribir |
| `027` | `640` rw-r----- | `750` rwxr-x--- | Restrictivo — others sin acceso |
| `007` | `660` rw-rw---- | `770` rwxrwx--- | Grupo colaborativo, others sin acceso |

---

## Cambiar la umask

### Para la sesión actual

```bash
# Cambiar umask (solo afecta a la shell actual y sus hijos)
umask 077

# Verificar
umask
# 0077

touch test-file
ls -la test-file
# -rw------- 1 dev dev ... test-file
```

### Permanente por usuario

Se configura en los archivos de perfil de la shell:

```bash
# Agregar al final de ~/.bashrc
echo "umask 027" >> ~/.bashrc

# Aplicar sin re-login
source ~/.bashrc
```

### Permanente para todo el sistema

```bash
# En /etc/login.defs (leído por pam_umask.so)
grep UMASK /etc/login.defs
# UMASK 022

# PAM — el módulo que realmente aplica la umask al iniciar sesión
grep umask /etc/pam.d/common-session      # Debian
grep umask /etc/pam.d/system-auth          # RHEL
# session optional pam_umask.so
```

### Orden de precedencia

De menor a mayor precedencia (el último gana):

1. `/etc/login.defs` → `UMASK` (leído por `pam_umask.so` al hacer login)
2. `/etc/pam.d/` → `pam_umask.so` puede tener su propia config con `umask=`
3. `/etc/profile` (scripts del sistema, se ejecutan al login)
4. `/etc/profile.d/*.sh` (fragmentos del sistema)
5. `~/.profile` o `~/.bash_profile` (perfil del usuario)
6. `~/.bashrc` (configuración de shell interactiva)
7. Comando `umask` explícito en la sesión o en un script

> **Nota:** Para shells no-login (como una nueva ventana de terminal), solo
> se ejecuta `~/.bashrc`. El paso 1–5 solo ocurre en login shells
> (`su -`, `ssh`, login en consola).

---

## Herencia

La umask se hereda de padre a hijo — un proceso hijo tiene la misma umask
que el proceso que lo lanzó:

```bash
umask 027
bash -c 'umask'
# 0027  (heredó la umask del padre)
```

### Umask en servicios systemd

Los servicios no heredan la umask de ninguna shell — systemd establece la
suya directamente:

```ini
# En el unit file del servicio:
[Service]
UMask=0077
```

El default de systemd es `0022`. Cambiar la umask de un servicio no afecta
al sistema ni a los usuarios.

```bash
# Ver la umask de un servicio en ejecución (requiere systemd, no Docker)
pid=$(systemctl show -p MainPID sshd | cut -d= -f2)
grep Umask /proc/$pid/status
# Umask: 0022
```

---

## Umask en scripts

Buena práctica: establecer `umask` al inicio de todo script para no depender
del entorno de quien lo ejecuta:

```bash
#!/bin/bash
umask 027    # archivos 640, directorios 750

# Todos los archivos creados por el script tendrán permisos predecibles
```

---

## Programas que ignoran la umask

Algunos comandos establecen permisos directamente con `chmod()` después de
crear el archivo, ignorando la umask:

```bash
# install -m establece permisos explícitos (ignora umask)
umask 077
install -m 755 script.sh /usr/local/bin/
stat -c '%a' /usr/local/bin/script.sh    # 755 (no 700)

# mkdir -m también ignora la umask
mkdir -m 700 /tmp/private-dir
stat -c '%a' /tmp/private-dir            # 700 (independiente de umask)

# cp --preserve=mode copia permisos del original
# chmod siempre establece permisos exactos
```

**La umask solo afecta la creación inicial** (la syscall `open()/mkdir()`).
Cualquier `chmod()` posterior la ignora completamente.

---

## Umask y SGID en directorios

La umask y el SGID en directorios son independientes:
- **SGID** controla qué **grupo** tiene el archivo nuevo
- **umask** controla qué **permisos** tiene el archivo nuevo

```bash
# Directorio SGID con grupo "devteam"
sudo chmod 2775 /srv/project
sudo chown root:devteam /srv/project

# Alice (umask 022) crea archivo:
# grupo = devteam (por SGID) ✓
# permisos = 644 (por umask 022) — group no puede escribir!

# Para que la colaboración funcione, Alice necesita umask 002:
# grupo = devteam (por SGID) ✓
# permisos = 664 (por umask 002) — group puede escribir ✓
```

Este es el motivo por el cual los directorios SGID colaborativos requieren
que los usuarios tengan `umask 002` o `007`.

---

## Diferencias entre distribuciones

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Umask default (root) | `022` | `022` |
| Umask default (usuario) | `022` | `022` |
| Configuración | `/etc/login.defs` + PAM | `/etc/login.defs` + PAM |
| pam_umask | En `common-session` | En `system-auth` |

Ambas usan `022` por defecto. Algunas distribuciones desktop usan `002`
porque con UPG (*User Private Group*), el grupo primario de cada usuario es
único — así que `rw-rw-r--` (664) no expone archivos a otros usuarios
(su grupo privado no tiene otros miembros).

---

## Labs

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

### Parte 1 — Observar la umask

#### Paso 1.1: Ver la umask actual

```bash
docker compose exec debian-dev bash -c '
echo "=== Umask en octal ==="
umask

echo ""
echo "=== Umask en simbólico ==="
umask -S
echo "(muestra los permisos que QUEDAN, no los que se quitan)"
'
```

#### Paso 1.2: Efecto en archivos y directorios

```bash
docker compose exec debian-dev bash -c '
echo "=== Umask actual: $(umask) ==="

echo ""
echo "=== Crear archivo ==="
touch /tmp/umask-file
stat -c "%a %A %n" /tmp/umask-file
echo "(base 666 AND NOT(022) = 644)"

echo ""
echo "=== Crear directorio ==="
mkdir /tmp/umask-dir
stat -c "%a %A %n" /tmp/umask-dir
echo "(base 777 AND NOT(022) = 755)"

rm -f /tmp/umask-file
rmdir /tmp/umask-dir
'
```

#### Paso 1.3: Comparar entre distribuciones

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
echo "Umask: $(umask)"
echo "login.defs: $(grep "^UMASK" /etc/login.defs 2>/dev/null || echo "no definido")"
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
echo "Umask: $(umask)"
echo "login.defs: $(grep "^UMASK" /etc/login.defs 2>/dev/null || echo "no definido")"
'
```

---

### Parte 2 — Cambiar y verificar

#### Paso 2.1: Umask 077 (privado)

```bash
docker compose exec debian-dev bash -c '
OLD=$(umask)
umask 077

echo "=== Umask 077 ==="
touch /tmp/test-077-file
mkdir /tmp/test-077-dir
stat -c "%a %A %n" /tmp/test-077-file
stat -c "%a %A %n" /tmp/test-077-dir
echo "(600 y 700 — solo el owner tiene acceso)"

rm -f /tmp/test-077-file
rmdir /tmp/test-077-dir
umask $OLD
'
```

#### Paso 2.2: Umask 002 (colaborativo)

```bash
docker compose exec debian-dev bash -c '
OLD=$(umask)
umask 002

echo "=== Umask 002 ==="
touch /tmp/test-002-file
mkdir /tmp/test-002-dir
stat -c "%a %A %n" /tmp/test-002-file
stat -c "%a %A %n" /tmp/test-002-dir
echo "(664 y 775 — group tiene escritura)"

rm -f /tmp/test-002-file
rmdir /tmp/test-002-dir
umask $OLD
'
```

#### Paso 2.3: Umask 027 (restrictivo)

```bash
docker compose exec debian-dev bash -c '
OLD=$(umask)
umask 027

echo "=== Umask 027 ==="
touch /tmp/test-027-file
mkdir /tmp/test-027-dir
stat -c "%a %A %n" /tmp/test-027-file
stat -c "%a %A %n" /tmp/test-027-dir
echo "(640 y 750 — others sin acceso, group solo lectura)"

rm -f /tmp/test-027-file
rmdir /tmp/test-027-dir
umask $OLD
'
```

#### Paso 2.4: Tabla comparativa con bucle

```bash
docker compose exec debian-dev bash -c '
OLD=$(umask)

echo "=== Comparación de umasks ==="
printf "%-8s %-10s %-10s\n" "umask" "archivo" "directorio"
printf "%-8s %-10s %-10s\n" "-----" "--------" "----------"

for u in 022 077 002 027 007 037; do
    umask $u
    touch "/tmp/test-$u-f"
    mkdir "/tmp/test-$u-d"
    F=$(stat -c "%a" "/tmp/test-$u-f")
    D=$(stat -c "%a" "/tmp/test-$u-d")
    printf "%-8s %-10s %-10s\n" "$u" "$F" "$D"
    rm -f "/tmp/test-$u-f"
    rmdir "/tmp/test-$u-d"
done

umask $OLD
'
```

---

### Parte 3 — Cálculo y configuración

#### Paso 3.1: El cálculo no es aritmético

```bash
docker compose exec debian-dev bash -c '
echo "=== Umask 033 ==="
echo "Aritmético: 666 - 033 = 633 (INCORRECTO)"
echo "Bitwise:    666 AND NOT(033) = 644 (CORRECTO)"
echo ""
echo "033 intenta quitar w+x de group y others"
echo "Pero x no existe en base 666, así que quitar x no hace nada"
echo "Solo se quita w: 6→4 para group y 6→4 para others"

echo ""
echo "=== Verificar ==="
OLD=$(umask)
umask 033
touch /tmp/test-033
stat -c "%a %n" /tmp/test-033
echo "(644, no 633)"
rm -f /tmp/test-033
umask $OLD
'
```

#### Paso 3.2: Herencia de procesos

```bash
docker compose exec debian-dev bash -c '
echo "=== La umask se hereda de padre a hijo ==="
umask 027
echo "Shell actual: $(umask)"
echo "Subshell:     $(bash -c "umask")"
echo "(la subshell heredó 027)"

umask 022
'
```

#### Paso 3.3: Dónde se configura

```bash
docker compose exec debian-dev bash -c '
echo "=== Orden de precedencia (menor a mayor) ==="
echo "1. /etc/login.defs (UMASK) — leído por pam_umask.so"
echo "2. /etc/pam.d/ (pam_umask.so con umask= propio)"
echo "3. /etc/profile"
echo "4. /etc/profile.d/*.sh"
echo "5. ~/.profile o ~/.bash_profile"
echo "6. ~/.bashrc"
echo "7. Comando umask explícito"

echo ""
echo "=== login.defs ==="
grep "^UMASK" /etc/login.defs 2>/dev/null || echo "(no definido)"

echo ""
echo "=== PAM ==="
grep umask /etc/pam.d/common-session 2>/dev/null || echo "(no encontrado en PAM)"
'
```

#### Paso 3.4: Umask en scripts (buena práctica)

```bash
docker compose exec debian-dev bash -c '
echo "=== Buena práctica: establecer umask al inicio del script ==="

cat << "DEMO"
#!/bin/bash
umask 027    # archivos 640, directorios 750

# Así no dependes del entorno de quien lo ejecuta
# Todos los archivos creados por el script serán predecibles
DEMO
'
```

---

### Limpieza final

No hay recursos que limpiar — todos los archivos temporales se eliminan
dentro de cada paso.

---

## Ejercicios

### Ejercicio 1 — Observar y predecir

```bash
umask
umask -S
touch /tmp/ej1-file
mkdir /tmp/ej1-dir
stat -c '%a %A %n' /tmp/ej1-file /tmp/ej1-dir
rm /tmp/ej1-file && rmdir /tmp/ej1-dir
```

**Pregunta:** Si tu umask es `0022`, ¿qué permisos tienen el archivo y el
directorio? ¿Qué significa `umask -S` y por qué no muestra lo que se quita
sino lo que queda?

<details><summary>Predicción</summary>

```
umask:   0022
umask -S: u=rwx,g=rx,o=rx
archivo: 644 (rw-r--r--)
directorio: 755 (rwxr-xr-x)
```

- El archivo tiene `644` porque `666 AND NOT(022) = 644`
- El directorio tiene `755` porque `777 AND NOT(022) = 755`

`umask -S` muestra los permisos que **quedan** (lo que un directorio tendría
al crearse), no lo que se quita. Es la representación simbólica del
complemento de la máscara: `u=rwx,g=rx,o=rx` significa que la máscara quita
`w` de group y `w` de others.

</details>

---

### Ejercicio 2 — Tabla de umasks

```bash
OLD=$(umask)
printf "%-8s %-10s %-10s\n" "umask" "archivo" "directorio"
printf "%-8s %-10s %-10s\n" "-----" "-------" "----------"

for u in 000 002 007 022 027 033 037 077; do
    umask $u
    touch "/tmp/test-$u-f"
    mkdir "/tmp/test-$u-d"
    printf "%-8s %-10s %-10s\n" "$u" \
        "$(stat -c '%a' /tmp/test-$u-f)" \
        "$(stat -c '%a' /tmp/test-$u-d)"
    rm "/tmp/test-$u-f" && rmdir "/tmp/test-$u-d"
done
umask $OLD
```

**Pregunta:** Antes de ejecutar, predice los resultados para `033` y `037`.
¿Coinciden con la resta aritmética `666 - 033` y `666 - 037`?

<details><summary>Predicción</summary>

| umask | Archivo | Directorio | ¿Resta coincide? |
|---|---|---|---|
| `000` | 666 | 777 | Sí |
| `002` | 664 | 775 | Sí |
| `007` | 660 | 770 | Sí |
| `022` | 644 | 755 | Sí |
| `027` | 640 | 750 | Sí |
| `033` | **644** | 744 | **No** para archivo (resta daría 633) |
| `037` | **640** | 740 | **No** para archivo (resta daría 631) |
| `077` | 600 | 700 | Sí |

Para `033` archivo: `666 AND NOT(033)` = `110 110 110 AND 111 100 100` = `110 100 100` = **644**.
La resta `666 - 033 = 633` es incorrecta.

Para `037` archivo: `666 AND NOT(037)` = `110 110 110 AND 111 100 000` = `110 100 000` = **640**.
La resta `666 - 037 = 631` es incorrecta (el 1 es imposible: no puedes tener solo `x` sin `r`).

**Regla:** cuando algún dígito de la umask tiene bits que no existen en el
base (como `x` en archivos con base 666), la resta aritmética falla. El
AND NOT los ignora silenciosamente.

Para directorios (base 777) la resta siempre coincide con el bitwise porque
777 tiene todos los bits activos.

</details>

---

### Ejercicio 3 — umask 000: archivos nunca tienen x

```bash
umask 000
touch /tmp/no-exec-test
stat -c '%a %A' /tmp/no-exec-test
rm /tmp/no-exec-test
umask 022
```

**Pregunta:** Con la umask más permisiva posible (000), ¿el archivo tiene
permisos `777`?

<details><summary>Predicción</summary>

No. El archivo tiene permisos **`666`** (`rw-rw-rw-`), no `777`.

La umask `000` no quita nada, pero `touch` solicita permisos `666` al kernel
(no `777`). La base que solicita el programa es lo que importa, y `touch`
(como la mayoría de programas) nunca solicita el bit `x`.

```
666 AND NOT(000) = 666 AND 777 = 666
```

Para que un archivo tenga `x`, siempre hay que usar `chmod +x` explícitamente.
Esto es una protección de seguridad: evita que archivos descargados o creados
por programas sean ejecutables automáticamente.

`mkdir`, en cambio, sí solicita `777`, así que con umask 000 un directorio
sí tendría `777`.

</details>

---

### Ejercicio 4 — UPG y umask 002: por qué es seguro

```bash
useradd -m upg_alice 2>/dev/null
useradd -m upg_bob 2>/dev/null

# alice crea archivo con umask 002
su -c '
    umask 002
    touch /tmp/upg-test-file
    ls -la /tmp/upg-test-file
    id -gn
' upg_alice

# bob intenta escribir
su -c 'echo "hack" >> /tmp/upg-test-file' upg_bob 2>&1; echo "exit: $?"

rm -f /tmp/upg-test-file
userdel -r upg_alice 2>/dev/null
userdel -r upg_bob 2>/dev/null
```

**Pregunta:** Con umask `002`, el archivo tiene permisos `664` (group puede
escribir). ¿Puede bob escribir en el archivo de alice?

<details><summary>Predicción</summary>

No. Bob **no** puede escribir. El archivo tiene:

```
-rw-rw-r-- 1 upg_alice upg_alice ... /tmp/upg-test-file
```

Permisos `664` significan que el **grupo** `upg_alice` puede escribir. Pero
bob **no es miembro** del grupo `upg_alice`.

Esto es lo que hace seguro usar umask `002` con UPG (*User Private Group*):
cada usuario tiene su propio grupo privado como grupo primario. Aunque
group tenga `rw`, solo los miembros de ese grupo privado (solo alice)
pueden escribir.

Si en cambio todos los usuarios compartieran un grupo primario (como `users`),
umask `002` sería peligroso — cualquier miembro de `users` podría escribir
en los archivos de otros.

</details>

---

### Ejercicio 5 — Herencia padre→hijo

```bash
umask 027
echo "Shell actual: $(umask)"
echo "Subshell:     $(bash -c 'umask')"
echo "Sub-subshell: $(bash -c 'bash -c "umask"')"

# Cambiar en subshell no afecta al padre
bash -c 'umask 077; echo "Subshell cambió a: $(umask)"'
echo "Padre sigue con: $(umask)"

umask 022
```

**Pregunta:** ¿La sub-subshell hereda la umask? Si la subshell cambia su
umask a 077, ¿cambia la del padre?

<details><summary>Predicción</summary>

```
Shell actual: 0027
Subshell:     0027
Sub-subshell: 0027
Subshell cambió a: 0077
Padre sigue con: 0027
```

La umask se hereda de padre a hijo en cada `fork()`. Cada nivel de subshell
hereda la umask del proceso que lo creó.

Pero cambiar la umask en un hijo **no afecta al padre**. El hijo tiene su
propia copia de la umask. Esto es una propiedad fundamental de los procesos
Unix: un proceso hijo no puede modificar el entorno de su padre.

Esto explica por qué `umask 027` en un script no afecta a la shell que lo
ejecutó — el script corre en un proceso aparte.

</details>

---

### Ejercicio 6 — umask en scripts: buena práctica

```bash
cat > /tmp/script-umask.sh << 'SCRIPT'
#!/bin/bash
echo "umask heredada: $(umask)"
umask 027
echo "umask establecida: $(umask)"
touch /tmp/script-created-file
stat -c '%a %n' /tmp/script-created-file
SCRIPT
chmod +x /tmp/script-umask.sh

# Ejecutar con diferentes umasks en el padre
echo "--- Con umask 022 en el padre ---"
umask 022
/tmp/script-umask.sh

echo "--- Con umask 077 en el padre ---"
umask 077
/tmp/script-umask.sh

rm -f /tmp/script-umask.sh /tmp/script-created-file
umask 022
```

**Pregunta:** ¿El archivo creado por el script siempre tiene los mismos
permisos, independientemente de la umask del padre?

<details><summary>Predicción</summary>

Sí. En ambos casos, el archivo tiene permisos **`640`**.

```
--- Con umask 022 en el padre ---
umask heredada: 0022
umask establecida: 0027
640 /tmp/script-created-file

--- Con umask 077 en el padre ---
umask heredada: 0077
umask establecida: 0027
640 /tmp/script-created-file
```

El script hereda la umask del padre (primero muestra `0022`, luego `0077`),
pero inmediatamente la cambia a `027`. Después de ese punto, todos los
archivos se crean con `666 AND NOT(027) = 640`.

**Esta es la buena práctica:** al inicio de todo script, establecer la umask
explícitamente para que el comportamiento sea predecible,
independientemente de quién o cómo ejecute el script.

</details>

---

### Ejercicio 7 — install -m y mkdir -m ignoran la umask

```bash
umask 077
echo "umask: $(umask)"

# touch respeta umask
touch /tmp/umask-touch
stat -c '%a %n' /tmp/umask-touch

# install -m ignora umask
echo "test" > /tmp/source-file
install -m 755 /tmp/source-file /tmp/umask-install
stat -c '%a %n' /tmp/umask-install

# mkdir -m ignora umask
mkdir -m 755 /tmp/umask-mkdir
stat -c '%a %n' /tmp/umask-mkdir

rm -f /tmp/umask-touch /tmp/source-file /tmp/umask-install
rmdir /tmp/umask-mkdir
umask 022
```

**Pregunta:** Con umask `077`, ¿qué permisos tiene cada recurso?

<details><summary>Predicción</summary>

```
umask: 0077
600 /tmp/umask-touch       ← umask aplicada: 666 AND NOT(077) = 600
755 /tmp/umask-install     ← umask IGNORADA: install -m establece 755
755 /tmp/umask-mkdir       ← umask IGNORADA: mkdir -m establece 755
```

`install -m` y `mkdir -m` crean el recurso y luego llaman a `chmod()`
internamente, sobreescribiendo cualquier efecto de la umask.

La umask **solo afecta la creación inicial** (la syscall `open()/mkdir()`).
Cualquier `chmod()` posterior — ya sea explícito o hecho internamente por
un programa — la ignora completamente.

Otros programas que ignoran la umask:
- `chmod` (obviamente)
- `cp --preserve=mode` (copia permisos del original)
- Muchos servidores web (configuran permisos en sus archivos de config)

</details>

---

### Ejercicio 8 — Umask y SGID: son independientes

```bash
groupadd -f collab
useradd -m umask_user -G collab 2>/dev/null

mkdir -p /tmp/sgid-umask-test
chown root:collab /tmp/sgid-umask-test
chmod 2775 /tmp/sgid-umask-test

# Crear archivo con umask 022 (default)
su -c '
    umask 022
    touch /tmp/sgid-umask-test/file-022
    stat -c "%a %G %n" /tmp/sgid-umask-test/file-022
' umask_user

# Crear archivo con umask 002 (colaborativo)
su -c '
    umask 002
    touch /tmp/sgid-umask-test/file-002
    stat -c "%a %G %n" /tmp/sgid-umask-test/file-002
' umask_user

rm -rf /tmp/sgid-umask-test
userdel -r umask_user 2>/dev/null
groupdel collab 2>/dev/null
```

**Pregunta:** Ambos archivos están en un directorio SGID con grupo `collab`.
¿Qué grupo y qué permisos tiene cada uno? ¿Cuál permite que el grupo
`collab` escriba?

<details><summary>Predicción</summary>

```
644 collab /tmp/sgid-umask-test/file-022
664 collab /tmp/sgid-umask-test/file-002
```

Ambos archivos tienen grupo `collab` (por SGID del directorio). Pero los
**permisos** son diferentes:

- Con umask `022`: archivo `644` → group tiene `r--` (solo lectura).
  Los miembros de `collab` pueden leer pero **no escribir**.
- Con umask `002`: archivo `664` → group tiene `rw-` (lectura + escritura).
  Los miembros de `collab` **sí pueden escribir**.

SGID controla **qué grupo** hereda el archivo. La umask controla **qué
permisos** tiene. Son mecanismos independientes. Para que un directorio SGID
funcione como espacio colaborativo, los usuarios necesitan umask `002` o
`007`.

</details>

---

### Ejercicio 9 — Trazar de dónde viene la umask

```bash
echo "=== /etc/login.defs ==="
grep "^UMASK" /etc/login.defs 2>/dev/null || echo "(no definido)"

echo ""
echo "=== PAM (pam_umask.so) ==="
grep -r "pam_umask" /etc/pam.d/ 2>/dev/null || echo "(no encontrado)"

echo ""
echo "=== /etc/profile ==="
grep -n "umask" /etc/profile 2>/dev/null || echo "(no encontrado)"

echo ""
echo "=== /etc/profile.d/ ==="
grep -rn "umask" /etc/profile.d/ 2>/dev/null || echo "(no encontrado)"

echo ""
echo "=== ~/.bashrc ==="
grep -n "umask" ~/.bashrc 2>/dev/null || echo "(no encontrado)"

echo ""
echo "=== ~/.profile ==="
grep -n "umask" ~/.profile 2>/dev/null || echo "(no encontrado)"

echo ""
echo "=== Umask actual ==="
umask
```

**Pregunta:** ¿En cuántos lugares está configurada la umask? ¿Cuál tiene
mayor precedencia?

<details><summary>Predicción</summary>

En un sistema Debian/Ubuntu típico, la umask aparece en:

1. `/etc/login.defs` → `UMASK 022`
2. `/etc/pam.d/common-session` → `session optional pam_umask.so`
3. Posiblemente `/etc/profile` → condicional basado en UID

En un sistema RHEL/AlmaLinux:

1. `/etc/login.defs` → `UMASK 022`
2. `/etc/pam.d/system-auth` → `session optional pam_umask.so`

La **precedencia de mayor a menor** es:
`~/.bashrc` > `~/.profile` > `/etc/profile.d/` > `/etc/profile` > PAM > login.defs

Si agregas `umask 077` a `~/.bashrc`, esa será tu umask final,
independientemente de lo que digan los demás archivos.

`pam_umask.so` lee el valor de `/etc/login.defs` (directiva `UMASK`) a menos
que tenga su propia configuración con `umask=NNN`. Es el mecanismo que aplica
la umask al iniciar una sesión de login.

</details>

---

### Ejercicio 10 — umask y chmod: cuándo se aplica cada uno

```bash
umask 077

# Crear archivo (umask aplica)
touch /tmp/umask-vs-chmod
stat -c '%a' /tmp/umask-vs-chmod

# chmod sobreescribe (umask ya no importa)
chmod 755 /tmp/umask-vs-chmod
stat -c '%a' /tmp/umask-vs-chmod

# Crear otro archivo (umask sigue aplicando a creaciones nuevas)
touch /tmp/umask-vs-chmod-2
stat -c '%a' /tmp/umask-vs-chmod-2

rm -f /tmp/umask-vs-chmod /tmp/umask-vs-chmod-2
umask 022
```

**Pregunta:** ¿La umask afecta al `chmod`? ¿Y a la creación del segundo
archivo?

<details><summary>Predicción</summary>

```
600    ← primer archivo: umask 077 aplicada (666 AND NOT(077) = 600)
755    ← después de chmod: umask NO aplica, chmod establece exactamente 755
600    ← segundo archivo: umask 077 sigue aplicando a creaciones nuevas
```

- **La umask solo se aplica en el momento de creación** del archivo (syscall
  `open()` con `O_CREAT`).
- **`chmod` ignora la umask** completamente. Establece exactamente los
  permisos indicados.
- La umask no es "consumida" ni "gastada" — permanece activa para todas las
  creaciones futuras en esa sesión/proceso.

Analogía: la umask es como un filtro de café permanente. Cada vez que creas
un archivo, el café (permisos solicitados) pasa por el filtro (umask).
Pero `chmod` es como servir café directamente sin pasar por el filtro.

</details>

---

## Limpieza de ejercicios

```bash
userdel -r upg_alice 2>/dev/null
userdel -r upg_bob 2>/dev/null
userdel -r umask_user 2>/dev/null
groupdel collab 2>/dev/null
rm -f /tmp/ej1-file /tmp/script-umask.sh /tmp/script-created-file 2>/dev/null
rm -f /tmp/umask-touch /tmp/umask-install /tmp/source-file 2>/dev/null
rm -rf /tmp/umask-mkdir /tmp/sgid-umask-test 2>/dev/null
rm -f /tmp/umask-vs-chmod /tmp/umask-vs-chmod-2 2>/dev/null
rm -f /tmp/no-exec-test /tmp/upg-test-file 2>/dev/null
echo "Limpieza completada"
```
