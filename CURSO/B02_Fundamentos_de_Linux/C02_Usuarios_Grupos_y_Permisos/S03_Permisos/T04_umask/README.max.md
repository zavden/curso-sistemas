# T04 — umask (Enhanced)

## Qué es la umask

La umask (user file-creation mode mask) define qué permisos se **quitan** de
los archivos y directorios que un proceso crea. No establece permisos — los
filtra.

```bash
# Ver la umask actual
umask
# 0022

# Ver en formato simbólico
umask -S
# u=rwx,g=rx,o=rx
```

## Cómo se calcula

El sistema intenta crear archivos con permisos `666` y directorios con `777`.
La umask se **resta** (en realidad es un AND con el complemento):

```
Archivo: 666 AND NOT(umask) = permisos resultantes
         666 AND NOT(022)   = 644

Directorio: 777 AND NOT(umask) = permisos resultantes
            777 AND NOT(022)   = 755
```

Con umask `022`:

| Tipo | Permisos base | Umask | Resultado | Simbólico |
|---|---|---|---|---|
| Archivo | 666 | 022 | 644 | `rw-r--r--` |
| Directorio | 777 | 022 | 755 | `rwxr-xr-x` |

Con umask `077`:

| Tipo | Permisos base | Umask | Resultado | Simbólico |
|---|---|---|---|---|
| Archivo | 666 | 077 | 600 | `rw-------` |
| Directorio | 777 | 077 | 700 | `rwx------` |

Con umask `002`:

| Tipo | Permisos base | Umask | Resultado | Simbólico |
|---|---|---|---|---|
| Archivo | 666 | 002 | 664 | `rw-rw-r--` |
| Directorio | 777 | 002 | 775 | `rwxrwxr-x` |

### Por qué archivos usan 666 y no 777

Los archivos nunca se crean con bit de ejecución — el base es `666`, no `777`.
Esto es una protección: un archivo descargado o creado no es ejecutable por
defecto. Hay que agregar `x` explícitamente con `chmod +x`.

### La resta no es aritmética

La umask no es una resta simple — es una operación de bits. En la mayoría de
casos la resta aritmética da el mismo resultado, pero no siempre:

```
# umask 033:
# Aritmética: 666 - 033 = 633 ← INCORRECTO
# Bits: 666 AND NOT(033) = 644 ← CORRECTO
#
# 033 quita w de group y wx de others
# Pero el bit x no existe en 666, así que quitar x no hace nada
# Solo se quita w de group: 6→4, y wx de others: 6→4
```

Para umasks comunes (022, 077, 002) la resta aritmética coincide, pero
técnicamente la operación es bitwise.

## Umasks comunes

| Umask | Archivos | Directorios | Uso |
|---|---|---|---|
| `022` | `644` rw-r--r-- | `755` rwxr-xr-x | Default estándar — todos pueden leer |
| `077` | `600` rw------- | `700` rwx------ | Privado — solo el owner |
| `002` | `664` rw-rw-r-- | `775` rwxrwxr-x | Colaborativo — group puede escribir |
| `027` | `640` rw-r----- | `750` rwxr-x--- | Restrictivo — others sin acceso |
| `007` | `660` rw-rw---- | `770` rwxrwx--- | Grupo colaborativo, others sin acceso |

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
# Debian: ~/.profile o ~/.bashrc
echo "umask 027" >> ~/.bashrc

# Aplicar sin re-login
source ~/.bashrc
```

### Permanente para todo el sistema

```bash
# Debian
cat /etc/login.defs | grep UMASK
# UMASK 022

# RHEL: también en /etc/login.defs
grep UMASK /etc/login.defs
# UMASK 022

# PAM puede establecer umask (tiene precedencia sobre login.defs)
grep umask /etc/pam.d/common-session      # Debian
grep umask /etc/pam.d/system-auth          # RHEL
# session optional pam_umask.so
```

### Orden de precedencia

El umask final se determina por (de menor a mayor precedencia):

1. `/etc/login.defs` (UMASK)
2. `/etc/pam.d/` (pam_umask.so)
3. `/etc/profile` (scripts del sistema)
4. `/etc/profile.d/*.sh` (fragmentos del sistema)
5. `~/.profile` o `~/.bash_profile` (usuario)
6. `~/.bashrc` (usuario)
7. Comando `umask` explícito en la sesión

## Herencia

La umask se hereda de padre a hijo — un proceso hijo tiene la misma umask
que el proceso que lo lanzó:

```bash
umask 027
bash -c 'umask'
# 0027  (heredó la umask del padre)

# Los servicios de systemd tienen su propia umask:
# En el unit file:
# [Service]
# UMask=0077
```

### Umask en servicios systemd

Los servicios no heredan la umask de la shell — systemd establece la suya:

```bash
# Default de systemd para servicios
# UMask=0022

# Cambiar en un unit file
# [Service]
# UMask=0077

# Ver la umask de un servicio en ejecución
sudo cat /proc/$(systemctl show -p MainPID sshd | cut -d= -f2)/status | grep Umask
# Umask: 0022
```

## Umask en scripts

```bash
#!/bin/bash
# Al inicio del script, establecer umask explícitamente
# para no depender del entorno de quien lo ejecuta
umask 027

# Todos los archivos creados por el script tendrán permisos 640/750
```

## Diferencias entre distribuciones

| Aspecto | Debian/Ubuntu | AlmaLinux/RHEL |
|---|---|---|
| Umask default (root) | `022` | `022` |
| Umask default (usuario) | `022` | `022` |
| Configuración | `/etc/login.defs` + PAM | `/etc/login.defs` + PAM |
| pam_umask | En `common-session` | En `system-auth` |

Ambas usan `022` por defecto. Algunas distribuciones orientadas a desktop usan
`002` para facilitar la colaboración (porque con UPG, el grupo primario es
privado del usuario, así que `002` no expone archivos a otros usuarios).

## Tabla: umask vs permisos resultantes

| umask | Archivo | Directorio | Cálculo |
|---|---|---|---|
| 0000 | 666 | 777 | 666-0=666, 777-0=777 |
| 0022 | 644 | 755 | 666-022=644, 777-022=755 |
| 0027 | 640 | 750 | 666-027=640, 777-027=750 |
| 0077 | 600 | 700 | 666-077=600, 777-077=700 |
| 0090 | 660 | 770 | 666-090=660, 777-090=770 |
| 0099 | 660 | 770 | 666-099=660, 777-099=770 |

## Ejercicios

### Ejercicio 1 — Observar la umask

```bash
# Ver la umask actual en octal y simbólica
umask
umask -S

# Crear un archivo y un directorio — verificar permisos
touch /tmp/umask-file
mkdir /tmp/umask-dir
stat -c '%a %n' /tmp/umask-file /tmp/umask-dir

rm /tmp/umask-file && rmdir /tmp/umask-dir
```

### Ejercicio 2 — Cambiar umask y verificar

```bash
# Guardar umask actual
OLD_UMASK=$(umask)

# Probar con 077
umask 077
touch /tmp/test-077
stat -c '%a %n' /tmp/test-077

# Probar con 002
umask 002
touch /tmp/test-002
stat -c '%a %n' /tmp/test-002

# Restaurar
umask $OLD_UMASK

rm /tmp/test-077 /tmp/test-002
```

### Ejercicio 3 — Calcular mentalmente

```bash
# ¿Qué permisos resultan de estas umasks?
# umask 037 → archivo = ? directorio = ?
# umask 077 → archivo = ? directorio = ?
# umask 002 → archivo = ? directorio = ?

# Verificar
for u in 037 077 002; do
    umask $u
    touch /tmp/test-$u-file
    mkdir /tmp/test-$u-dir
    echo "umask $u: file=$(stat -c '%a' /tmp/test-$u-file) dir=$(stat -c '%a' /tmp/test-$u-dir)"
    rm /tmp/test-$u-file && rmdir /tmp/test-$u-dir
done
umask 022  # restaurar
```

### Ejercicio 4 — umask 033 (caso especial)

```bash
#!/bin/bash
# umask 033 es el caso donde la resta NO funciona como se espera

umask 033
touch /tmp/umask-033-test
mkdir /tmp/umask-033-dir

echo "umask 033:"
echo "  Archivo: $(stat -c '%a' /tmp/umask-033-test) (esperado: 633, obtenido: 644)"
echo "  Directorio: $(stat -c '%a' /tmp/umask-033-dir) (esperado: 744, obtenido: 744)"

echo ""
echo "Por qué? 033 en binario = 000011011"
echo "666 en binario = 110110110"
echo "NOT(033) = 110100100 = 724"
echo "666 AND 724 = 644"
echo "El bit x no se quita porque no existe en el base 666"

rm -rf /tmp/umask-033-test /tmp/umask-033-dir
umask 022
```

### Ejercicio 5 — UPG y umask 002

```bash
#!/bin/bash
# Con UPG (grupo privado), umask 002 es seguro

echo "=== Tu grupo primario (UPG) ==="
id -gn

echo ""
echo "=== Con umask 002, archivo creado: ==="
umask 002
touch /tmp/collaborative-file.txt
stat -c '%a %A' /tmp/collaborative-file.txt

echo ""
echo "Otros usuarios (que no sean tú ni el grupo) tienen solo lectura."
echo "Tú puedes leer/escribir. Tu grupo (UPG único) también."

rm /tmp/collaborative-file.txt
umask 022
```

### Ejercicio 6 — umask en script vs shell interactiva

```bash
#!/bin/bash
# Demostrar que la umask del script puede diferir de la shell que lo llama

echo "umask dentro del script: $(umask)"
echo "Si ejecutas 'umask' en tu terminal, ¿es la misma?"

# Verificar que un proceso hijo tiene la misma umask
bash -c 'echo "umask del hijo: $(umask)"'
```

### Ejercicio 7 — umask de servicios systemd

```bash
#!/bin/bash
# Verificar umask de varios servicios

for svc in sshd systemd-logind cron; do
    pid=$(systemctl show -p MainPID $svc 2>/dev/null | cut -d= -f2)
    if [[ -n "$pid" && "$pid" != "0" ]]; then
        um=$(sudo cat /proc/$pid/status 2>/dev/null | grep Umask | awk '{print $2}')
        echo "$svc (PID $pid): Umask = $um"
    fi
done
```

### Ejercicio 8 — umask vs chmod (diferencia)

```bash
#!/bin/bash
# umask filtra AL CREAR, chmod cambia DESPUÉS

umask 027
touch /tmp/umask-test.txt
echo "Con umask 027, archivo creado: $(stat -c '%a' /tmp/umask-test.txt)"

chmod 777 /tmp/umask-test.txt
echo "Tras chmod 777: $(stat -c '%a' /tmp/umask-test.txt)"

rm /tmp/umask-test.txt
```

### Ejercicio 9 — Permisos de archivos vs directorios con diferentes umasks

```bash
#!/bin/bash
# Tabla completa de umasks comunes

echo "umask  | archivo | directorio"
echo "-------|--------|------------"
for um in 000 002 007 022 027 077; do
    umask $um
    touch /tmp/f-$um
    mkdir /tmp/d-$um
    fa=$(stat -c '%a' /tmp/f-$um)
    da=$(stat -c '%a' /tmp/d-$um)
    echo "$um     | $fa      | $da"
    rm /tmp/f-$um
    rmdir /tmp/d-$um
done
umask 022
```

### Ejercicio 10 — Permisos de archivos con ejecución

```bash
#!/bin/bash
# Los archivos nunca se crean con x, aunque la umask sea muy abierta

umask 000  # lo más abierto posible
touch /tmp/no-exec-test.txt
stat -c '%a %A' /tmp/no-exec-test.txt
# Resultado: 666 = rw-rw-rw-
# Nunca 777 = rwxrwxrwx

echo ""
echo "Para tener x en un archivo, hay que usar chmod +x"

rm /tmp/no-exec-test.txt
umask 022
```
