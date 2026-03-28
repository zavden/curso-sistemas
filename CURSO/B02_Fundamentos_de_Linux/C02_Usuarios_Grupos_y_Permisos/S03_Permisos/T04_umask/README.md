# T04 — umask

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

---

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
