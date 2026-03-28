# T02 — chmod y chown

## chmod — Cambiar permisos

`chmod` modifica los permisos de archivos y directorios. Tiene dos sintaxis:
octal y simbólica.

### Sintaxis octal

```bash
chmod 755 script.sh    # rwxr-xr-x
chmod 644 config.txt   # rw-r--r--
chmod 600 id_rsa       # rw-------
chmod 700 .ssh/        # rwx------
```

Es concisa y establece los tres conjuntos de permisos a la vez. Es la forma
preferida cuando se conoce el permiso final exacto.

### Sintaxis simbólica

```
chmod [quién][operador][permisos] archivo
```

**Quién**:

| Letra | Significado |
|---|---|
| `u` | User (owner) |
| `g` | Group |
| `o` | Others |
| `a` | All (u+g+o) |

**Operador**:

| Símbolo | Significado |
|---|---|
| `+` | Agregar permiso |
| `-` | Quitar permiso |
| `=` | Establecer exactamente |

```bash
# Agregar ejecución para el owner
chmod u+x script.sh

# Quitar escritura para group y others
chmod go-w file.txt

# Establecer permisos exactos para group
chmod g=rx script.sh

# Agregar lectura para todos
chmod a+r document.txt

# Múltiples cambios separados por coma
chmod u+x,g=rx,o-w script.sh

# Quitar todos los permisos de others
chmod o= file.txt
```

### Cuándo usar cada sintaxis

| Situación | Preferir |
|---|---|
| Establecer permisos desde cero | Octal: `chmod 755` |
| Agregar/quitar un permiso específico | Simbólica: `chmod g+w` |
| Scripts de automatización | Octal (predecible, no depende del estado actual) |
| Ajustes rápidos | Simbólica (más expresiva) |

### chmod -R — Recursivo

```bash
# Aplicar recursivamente a un directorio y todo su contenido
chmod -R 750 /srv/project/
```

El problema con `-R` es que aplica los **mismos permisos** a archivos y
directorios. Pero los directorios necesitan `x` para ser accesibles, mientras
que los archivos generalmente no necesitan `x`:

```bash
# INCORRECTO — da x a todos los archivos
chmod -R 755 /srv/project/

# CORRECTO — permisos distintos para archivos y directorios
find /srv/project -type d -exec chmod 755 {} \;
find /srv/project -type f -exec chmod 644 {} \;
```

### chmod con X mayúscula

`X` (mayúscula) agrega ejecución **solo a directorios y archivos que ya tienen
algún bit x**:

```bash
# Agrega x a directorios pero NO a archivos regulares
chmod -R a+X /srv/project/

# Combinar: legible por todos, x solo en directorios
chmod -R u=rwX,g=rX,o=rX /srv/project/
```

`X` es la forma idiomática de hacer `chmod -R` sin dar ejecución accidental a
archivos regulares.

## chown — Cambiar propietario y grupo

```bash
# Cambiar owner
sudo chown alice file.txt

# Cambiar owner y grupo
sudo chown alice:developers file.txt

# Cambiar solo el grupo (equivalente a chgrp)
sudo chown :developers file.txt

# Cambiar owner, grupo heredado del nuevo owner
sudo chown alice: file.txt
# (el grupo se cambia al grupo primario de alice)
```

### chown -R — Recursivo

```bash
# Cambiar owner y grupo recursivamente
sudo chown -R www-data:www-data /var/www/site/
```

### Solo root puede cambiar el owner

Un usuario regular no puede dar la propiedad de sus archivos a otro usuario
— esto es una medida de seguridad para evitar que un usuario ocupe la cuota
de disco de otro:

```bash
# Como usuario regular
chown bob myfile.txt
# chown: changing ownership of 'myfile.txt': Operation not permitted

# Solo root puede cambiar el owner
sudo chown bob myfile.txt
```

### chgrp — Cambiar solo el grupo

```bash
chgrp developers file.txt

# Equivalente a:
chown :developers file.txt
```

Un usuario regular puede cambiar el grupo de sus archivos, pero solo a un
grupo al que pertenezca:

```bash
# dev pertenece a: dev, sudo, docker
chgrp docker myfile.txt     # OK (dev es miembro de docker)
chgrp www-data myfile.txt   # Permission denied (dev no es miembro de www-data)
```

## Peculiaridades y edge cases

### Permisos de symlinks

Los permisos de symlinks no se usan — el kernel siempre sigue el link y
aplica los permisos del archivo destino:

```bash
ls -la link -> target
# lrwxrwxrwx 1 dev dev ... link -> target
# (siempre muestra 777 pero no importa — se usan los permisos de target)

# chmod sobre un symlink modifica el TARGET, no el link
chmod 600 link
# Cambia los permisos de target, no de link
```

Para modificar el symlink en sí (en los raros casos que se necesita):

```bash
# -h flag: opera sobre el symlink, no sobre el target
chown -h alice link    # Cambia el owner del symlink
```

### Permisos y hard links

Todos los hard links a un mismo archivo comparten los mismos permisos (porque
apuntan al mismo inodo):

```bash
echo "test" > original
ln original hardlink

chmod 600 hardlink
ls -la original hardlink
# -rw------- 2 dev dev ... original
# -rw------- 2 dev dev ... hardlink
# (ambos cambiaron porque son el mismo archivo)
```

### Directorios: w sin x es inútil

Dar `w` a un directorio sin `x` no permite crear ni eliminar archivos:

```bash
mkdir testdir
chmod 760 testdir   # rwxrw---- (group tiene rw pero no x)

# Un miembro del grupo puede listar (r) pero no crear archivos (w sin x)
touch testdir/file
# Permission denied
```

`x` en directorios es el permiso que permite "entrar" — sin él, `w` no sirve.

### Archivos inmutables (chattr)

Los permisos se pueden reforzar con atributos extendidos:

```bash
# Hacer un archivo inmutable (ni root puede modificar sin quitar el flag)
sudo chattr +i /etc/resolv.conf

# Verificar
lsattr /etc/resolv.conf
# ----i--------e-- /etc/resolv.conf

# Ni root puede modificar
sudo echo "nameserver 8.8.8.8" >> /etc/resolv.conf
# Permission denied

# Quitar el flag
sudo chattr -i /etc/resolv.conf
```

Los atributos de `chattr` operan a nivel de filesystem (ext4, xfs) y son
independientes de los permisos Unix.

## Buenas prácticas

```bash
# Claves SSH: 600 o 400 obligatorio (ssh rechaza permisos laxos)
chmod 600 ~/.ssh/id_rsa
chmod 644 ~/.ssh/id_rsa.pub
chmod 700 ~/.ssh/

# Directorios web
sudo chown -R www-data:www-data /var/www/site
sudo find /var/www/site -type d -exec chmod 755 {} \;
sudo find /var/www/site -type f -exec chmod 644 {} \;

# Scripts
chmod 755 script.sh    # Ejecutable por todos, editable solo por owner
chmod 700 admin.sh     # Solo el owner puede ejecutar

# Configuraciones con secretos
chmod 640 /etc/app/config.yml    # Owner lee/escribe, grupo lee, others nada
chown root:appgroup /etc/app/config.yml
```

---

## Ejercicios

### Ejercicio 1 — Práctica con chmod

```bash
touch /tmp/chmod-test

# Establecer 644 con octal
chmod 644 /tmp/chmod-test && stat -c '%a %A' /tmp/chmod-test

# Agregar ejecución para owner con simbólica
chmod u+x /tmp/chmod-test && stat -c '%a %A' /tmp/chmod-test

# Quitar lectura para others
chmod o-r /tmp/chmod-test && stat -c '%a %A' /tmp/chmod-test

# Establecer group = exactamente r
chmod g=r /tmp/chmod-test && stat -c '%a %A' /tmp/chmod-test

rm /tmp/chmod-test
```

### Ejercicio 2 — Permisos recursivos correctos

```bash
mkdir -p /tmp/webdir/css /tmp/webdir/js
touch /tmp/webdir/index.html /tmp/webdir/css/style.css /tmp/webdir/js/app.js

# Aplicar permisos diferenciados
find /tmp/webdir -type d -exec chmod 755 {} \;
find /tmp/webdir -type f -exec chmod 644 {} \;

# Verificar
find /tmp/webdir -exec stat -c '%a %n' {} \;

rm -rf /tmp/webdir
```

### Ejercicio 3 — Probar que owner no acumula permisos de group

```bash
touch /tmp/perm-test
chmod 074 /tmp/perm-test

# ¿Puedes leer el archivo? (owner tiene 0)
cat /tmp/perm-test

rm /tmp/perm-test
```
