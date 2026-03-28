# T05 — ACLs

> **Fuentes**: `README.md`, `README.max.md`, `labs/README.md`
>
> **Correcciones aplicadas**:
> - Ejercicio 7 del max.md: título "Finder" → "Encontrar". El patrón
>   `grep '\+$'` no funciona porque el `+` no está al final de la línea en
>   `ls -la` (está tras el campo de permisos, seguido por espacio). Corregido.
> - Ejercicio 10 del max.md: caracteres chinos `限制` (artefacto de
>   codificación). Eliminados.
> - Añadida explicación de que las **default ACLs anulan la umask** — cuando
>   un directorio tiene default ACLs, la umask no se aplica a archivos nuevos
>   creados dentro. Concepto clave ausente en todas las fuentes.
> - Añadida explicación de que el campo de grupo en `ls -la` muestra la
>   **mask** (no los permisos del grupo) cuando hay ACLs. Ausente en fuentes.

---

## Limitaciones del modelo Unix

Los permisos tradicionales Unix (owner/group/others) tienen una limitación
fundamental: solo permiten **un owner y un grupo** por archivo.

```
Problema: alice (grupo dev) y bob (grupo finance) necesitan acceso
a report.csv, pero con diferentes permisos.

Con permisos Unix:
- owner = alice → solo alice tiene permisos de owner
- group = dev → solo dev tiene permisos de grupo
- bob necesitaría estar en dev O tener permisos vía others
- Dar permisos vía others los da a TODO el sistema
```

Las **ACLs** (*Access Control Lists*) resuelven esto añadiendo entradas de
permisos por usuario o por grupo adicionales.

---

## Soporte de ACLs

Las ACLs son una extensión del filesystem. Los filesystems principales las
soportan:

```bash
# En ext4 y XFS, ACLs están habilitadas por defecto
# En otros filesystems, puede necesitar mount -o acl

# Verificar
mount | grep acl
# /dev/sda2 on / type ext4 (rw,...,acl,...)
```

Las herramientas necesarias vienen en el paquete `acl`:

```bash
# Debian
sudo apt install acl

# RHEL (normalmente ya instalado)
sudo dnf install acl
```

---

## getfacl — Ver ACLs

```bash
# Archivo sin ACLs (solo permisos Unix)
getfacl file.txt
# # file: file.txt
# # owner: alice
# # group: dev
# user::rw-       ← permisos del owner
# group::r--      ← permisos del grupo
# other::r--      ← permisos de others

# Archivo con ACLs
getfacl report.csv
# # file: report.csv
# # owner: alice
# # group: dev
# user::rw-           ← owner (alice)
# user:bob:rw-        ← ACL: bob tiene rw
# group::r--          ← grupo propietario (dev)
# group:finance:r--   ← ACL: grupo finance tiene r
# mask::rw-           ← máscara efectiva (techo)
# other::---          ← others
```

### Detectar archivos con ACLs

```bash
# El + al final de los permisos indica ACLs presentes
ls -la report.csv
# -rw-rw-r--+ 1 alice dev ... report.csv
#           ^
#           + = tiene ACLs extendidas
```

> **Importante:** Cuando hay ACLs, el campo de **grupo** en `ls -la` muestra
> la **mask**, no los permisos reales del grupo propietario. En el ejemplo
> anterior, `rw-` (grupo) es la mask, no los permisos de `dev`.
> Usa `getfacl` para ver los permisos reales.

---

## setfacl — Establecer ACLs

### Sintaxis

```
setfacl -m TIPO:NOMBRE:PERMISOS archivo
```

| Tipo | Descripción | Ejemplo |
|---|---|---|
| `u:nombre:perms` | ACL de usuario | `u:bob:rw` |
| `g:nombre:perms` | ACL de grupo | `g:finance:r` |
| `u::perms` | Owner (sin nombre) | `u::rwx` |
| `g::perms` | Grupo propietario | `g::r` |
| `o::perms` | Others | `o::---` |
| `m::perms` | Máscara | `m::rw` |

### Agregar permisos para un usuario

```bash
# Dar permisos rw a bob
setfacl -m u:bob:rw report.csv

# Dar solo lectura a charlie
setfacl -m u:charlie:r report.csv
```

### Agregar permisos para un grupo

```bash
# Dar lectura al grupo finance
setfacl -m g:finance:r report.csv

# Dar rw al grupo editors
setfacl -m g:editors:rw report.csv
```

### Múltiples entradas a la vez

```bash
setfacl -m u:bob:rw,g:finance:r,g:editors:rw report.csv
```

### Eliminar ACLs

```bash
# Eliminar la ACL de un usuario específico
setfacl -x u:bob report.csv

# Eliminar la ACL de un grupo específico
setfacl -x g:finance report.csv

# Eliminar TODAS las ACLs (volver a permisos Unix puros)
setfacl -b report.csv

# Verificar que el + desapareció
ls -la report.csv
# -rw-r--r-- 1 alice dev ... report.csv  (sin +)
```

---

## La máscara efectiva

La **mask** es el límite máximo de permisos para todas las entradas ACL
**excepto** owner y others. Funciona como un techo:

```bash
getfacl file.txt
# user::rw-
# user:bob:rw-         #effective:r--    ← limitado por mask
# group::rw-           #effective:r--    ← limitado por mask
# group:finance:rw-    #effective:r--    ← limitado por mask
# mask::r--            ← la máscara limita a solo lectura
# other::---
```

Aunque bob tiene `rw-` en su ACL, la máscara `r--` limita su acceso efectivo
a solo lectura. `getfacl` muestra el comentario `#effective:` cuando la mask
reduce los permisos.

### Qué afecta y qué no afecta la mask

| Entrada | ¿Afectada por mask? |
|---|---|
| `user::` (owner) | **No** |
| `user:nombre:` (named user) | **Sí** |
| `group::` (owning group) | **Sí** |
| `group:nombre:` (named group) | **Sí** |
| `other::` | **No** |

### Cómo se establece la máscara

```bash
# Se recalcula automáticamente al agregar ACLs
setfacl -m u:bob:rw file.txt
# mask se ajusta a rw (para permitir el rw de bob)

# Establecer máscara manualmente
setfacl -m m::r file.txt
# Todas las ACLs se limitan a lectura máximo

# CUIDADO: chmod sobre el grupo modifica la máscara
chmod g=r file.txt
# Equivale a setfacl -m m::r — reduce permisos de TODAS las ACLs
```

**Interacción sutil:** `chmod g=rw` en un archivo con ACLs modifica la
**máscara**, no los permisos del grupo propietario. Esto puede romper
permisos ACL sin querer.

---

## ACLs en directorios

### ACLs regulares

Funcionan igual que en archivos — controlan el acceso al directorio mismo:

```bash
# Dar acceso a bob al directorio (r=listar, x=entrar)
setfacl -m u:bob:rx /srv/project
```

### ACLs por defecto (default ACLs)

Las ACLs por defecto se aplican automáticamente a los archivos y subdirectorios
**nuevos** creados dentro del directorio:

```bash
# Establecer ACL por defecto
setfacl -d -m u:bob:rw /srv/project

# Verificar
getfacl /srv/project
# user::rwx
# group::r-x
# other::r-x
# default:user::rwx        ← default para owner
# default:user:bob:rw-     ← default para bob
# default:group::r-x       ← default para grupo
# default:mask::rwx         ← default máscara
# default:other::r-x       ← default para others

# Crear archivo dentro
touch /srv/project/new-file.txt
getfacl /srv/project/new-file.txt
# user:bob:rw-    ← heredó la ACL por defecto
```

### Herencia recursiva de defaults

Los subdirectorios nuevos heredan las ACLs por defecto del padre **y también
las establecen como sus propias ACLs por defecto** — la herencia se propaga
indefinidamente:

```bash
mkdir /srv/project/subdir
getfacl /srv/project/subdir
# default:user:bob:rw-    ← heredó y propagará a sus propios hijos
```

### Default ACLs anulan la umask

Cuando un directorio tiene default ACLs, la **umask no se aplica** a los
archivos y subdirectorios nuevos creados dentro. La default ACL mask
reemplaza la función de la umask:

```bash
# Directorio con default ACL mask rwx
setfacl -d -m m::rwx /srv/project

# Aunque el usuario tenga umask 077:
umask 077
touch /srv/project/new-file.txt
getfacl /srv/project/new-file.txt
# Los permisos vienen de las default ACLs, NO de la umask
```

Esto es importante: en directorios con default ACLs, los permisos de
archivos nuevos son **predecibles** independientemente de la umask del usuario.

### Aplicar ACLs recursivamente

```bash
# Aplicar ACL a todos los archivos y directorios existentes
setfacl -R -m u:bob:rw /srv/project

# Aplicar ACL por defecto a todos los directorios existentes
setfacl -R -d -m u:bob:rw /srv/project
```

`-R` aplica a lo que ya existe. Las default ACLs (`-d`) cubren lo que se
cree en el futuro. Normalmente se necesitan ambas.

---

## Orden de evaluación del kernel con ACLs

El kernel evalúa las ACLs en este orden (primera coincidencia gana):

1. **Owner** (`user::`) — si el proceso es el owner del archivo
2. **Named user** (`user:nombre:`) — si coincide con el UID del proceso
3. **Grupos** — unión de todos los grupos coincidentes (owning group +
   named groups), maskeado por `mask::`
4. **Others** (`other::`) — si ninguno de los anteriores coincidió

```
Ejemplo: archivo con estas ACLs:
  user::rw-
  user:bob:rw-
  group::r--
  group:editors:rw-
  mask::rw-
  other::---

- alice (owner) → usa user:: → rw
- bob → usa user:bob: AND mask → rw AND rw = rw
- charlie (grupo editors) → usa group:editors: AND mask → rw AND rw = rw
- dave (grupo dev = owning group) → usa group:: AND mask → r AND rw = r
- eve (otros) → usa other:: → ---
```

> **Nota:** Si el owner tiene un named user ACL con más permisos, NO se usa —
> el kernel siempre usa `user::` para el owner. Igual que con permisos Unix
> tradicionales, el owner queda atrapado en sus propios permisos de owner.

---

## Backup y restauración de ACLs

```bash
# Guardar ACLs (incluye rutas relativas)
getfacl -R /srv/project > acl-backup.txt

# Restaurar ACLs (ejecutar desde el mismo directorio base)
cd / && setfacl --restore=acl-backup.txt
```

> **Nota:** `setfacl --restore` usa las rutas del archivo de backup tal cual.
> Si `getfacl -R` se ejecutó desde `/`, las rutas serán relativas a `/`.
> Asegúrate de ejecutar `--restore` desde el mismo directorio.

### Herramientas que preservan ACLs

```bash
# tar con ACLs
tar --acls -czf backup.tar.gz /srv/project

# rsync con ACLs
rsync -a --acls /srv/project/ remote:/srv/project/

# cp -a preserva ACLs (incluye --preserve=all)
cp -a source dest
```

`cp` sin `-a` y `mv` entre filesystems distintos **pierden** las ACLs.

---

## ACLs vs permisos Unix

| Aspecto | Permisos Unix | ACLs |
|---|---|---|
| Granularidad | 1 owner, 1 grupo, others | Múltiples usuarios y grupos |
| Herencia | No (solo SGID para grupo) | Sí (ACLs por defecto) |
| Interacción con umask | Siempre aplica | Default ACLs la anulan |
| Complejidad | Simple, fácil de auditar | Más compleja, requiere getfacl |
| Rendimiento | Mínimo overhead | Ligero overhead (xattrs) |
| Compatibilidad | Universal | Requiere soporte de filesystem |

**Regla general:** usar permisos Unix cuando sean suficientes. Agregar ACLs
solo cuando se necesita granularidad que Unix no puede dar.

---

## Tabla resumen de operaciones

| Operación | Comando |
|---|---|
| Ver ACLs | `getfacl archivo` |
| Agregar ACL usuario | `setfacl -m u:nombre:perms archivo` |
| Agregar ACL grupo | `setfacl -m g:nombre:perms archivo` |
| Modificar máscara | `setfacl -m m::perms archivo` |
| Eliminar ACL específica | `setfacl -x u:nombre archivo` |
| Eliminar todas las ACLs | `setfacl -b archivo` |
| ACL por defecto | `setfacl -d -m u:nombre:perms dir` |
| Aplicar recursivamente | `setfacl -R -m u:nombre:perms dir` |
| Backup | `getfacl -R dir > backup.txt` |
| Restaurar | `setfacl --restore=backup.txt` |

---

## Labs

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

### Parte 1 — ACLs básicas

#### Paso 1.1: Archivo sin ACLs

```bash
docker compose exec debian-dev bash -c '
touch /tmp/acl-test
chmod 640 /tmp/acl-test

echo "=== Sin ACLs ==="
getfacl /tmp/acl-test

echo ""
echo "=== ls -la (sin +) ==="
ls -la /tmp/acl-test
'
```

Sin ACLs, `getfacl` muestra solo los permisos Unix. No hay `+` en `ls -la`.

#### Paso 1.2: Agregar ACL de usuario

```bash
docker compose exec debian-dev bash -c '
echo "=== Agregar lectura para nobody ==="
setfacl -m u:nobody:r /tmp/acl-test

echo ""
echo "=== getfacl muestra la ACL ==="
getfacl /tmp/acl-test

echo ""
echo "=== ls -la muestra el + ==="
ls -la /tmp/acl-test
echo "(el + al final indica ACLs extendidas)"
'
```

#### Paso 1.3: Agregar ACL de grupo

```bash
docker compose exec debian-dev bash -c '
groupadd -f aclteam

echo "=== Agregar ACL de grupo ==="
setfacl -m g:aclteam:rw /tmp/acl-test

echo ""
echo "=== Verificar ==="
getfacl /tmp/acl-test
'
```

#### Paso 1.4: Múltiples ACLs a la vez

```bash
docker compose exec debian-dev bash -c '
useradd -m acluser1 2>/dev/null
useradd -m acluser2 2>/dev/null

setfacl -m u:acluser1:r,u:acluser2:rw /tmp/acl-test

echo "=== Todas las ACLs ==="
getfacl /tmp/acl-test
'
```

#### Paso 1.5: Eliminar ACLs

```bash
docker compose exec debian-dev bash -c '
echo "=== Eliminar ACL de un usuario ==="
setfacl -x u:nobody /tmp/acl-test
getfacl /tmp/acl-test
echo "(nobody ya no aparece)"

echo ""
echo "=== Eliminar TODAS las ACLs ==="
setfacl -b /tmp/acl-test
getfacl /tmp/acl-test
echo "(solo quedan permisos Unix)"

echo ""
echo "=== El + desapareció ==="
ls -la /tmp/acl-test
'
```

---

### Parte 2 — ACLs por defecto

#### Paso 2.1: Crear directorio con ACL por defecto

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear directorio ==="
mkdir /tmp/acl-dir
chmod 750 /tmp/acl-dir

setfacl -d -m u:nobody:r /tmp/acl-dir
setfacl -d -m g:aclteam:rw /tmp/acl-dir

echo ""
echo "=== Verificar ==="
getfacl /tmp/acl-dir
echo "(las líneas default: son las ACLs por defecto)"
'
```

#### Paso 2.2: Verificar herencia en archivos

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear archivo dentro ==="
touch /tmp/acl-dir/file1.txt

echo ""
echo "=== ACLs heredadas ==="
getfacl /tmp/acl-dir/file1.txt
echo "(heredó las ACLs por defecto del directorio padre)"
'
```

#### Paso 2.3: Herencia en subdirectorios

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear subdirectorio ==="
mkdir /tmp/acl-dir/subdir

echo ""
echo "=== ACLs del subdirectorio ==="
getfacl /tmp/acl-dir/subdir
echo "(heredó ACLs regulares Y por defecto — la herencia se propaga)"

echo ""
echo "=== Archivo en el subdirectorio ==="
touch /tmp/acl-dir/subdir/file2.txt
getfacl /tmp/acl-dir/subdir/file2.txt
echo "(también heredó las ACLs)"
'
```

#### Paso 2.4: Aplicar ACLs recursivamente

```bash
docker compose exec debian-dev bash -c '
echo "=== Aplicar ACL a todo lo existente ==="
setfacl -R -m u:acluser1:r /tmp/acl-dir

echo ""
echo "=== Aplicar ACL por defecto a todos los directorios ==="
setfacl -R -d -m u:acluser1:r /tmp/acl-dir

echo ""
echo "=== Verificar ==="
echo "--- Directorio ---"
getfacl /tmp/acl-dir | grep acluser1
echo "--- Archivo ---"
getfacl /tmp/acl-dir/file1.txt | grep acluser1
echo "--- Subdirectorio ---"
getfacl /tmp/acl-dir/subdir | grep acluser1
'
```

---

### Parte 3 — Máscara y backup

#### Paso 3.1: La máscara

```bash
docker compose exec debian-dev bash -c '
touch /tmp/mask-test
setfacl -m u:nobody:rw /tmp/mask-test
setfacl -m g:aclteam:rw /tmp/mask-test

echo "=== ACLs actuales ==="
getfacl /tmp/mask-test
echo "(mask::rw- permite rw)"
'
```

#### Paso 3.2: Restringir la máscara

```bash
docker compose exec debian-dev bash -c '
echo "=== Restringir máscara a solo lectura ==="
setfacl -m m::r /tmp/mask-test

echo ""
echo "=== Verificar ==="
getfacl /tmp/mask-test
echo "(los permisos muestran #effective:r-- aunque la ACL dice rw-)"
'
```

#### Paso 3.3: chmod modifica la máscara

```bash
docker compose exec debian-dev bash -c '
echo "=== chmod g=rw en archivo con ACLs ==="
chmod g=rw /tmp/mask-test

echo ""
getfacl /tmp/mask-test
echo ""
echo "chmod g= modifica la MASCARA, no los permisos del grupo propietario"
echo "(ls -la muestra la mask en la posición de grupo)"

rm -f /tmp/mask-test
'
```

#### Paso 3.4: Backup y restauración de ACLs

```bash
docker compose exec debian-dev bash -c '
echo "=== Guardar ACLs ==="
getfacl -R /tmp/acl-dir > /tmp/acl-backup.txt
head -15 /tmp/acl-backup.txt
echo "..."

echo ""
echo "=== Eliminar todas las ACLs ==="
setfacl -R -b /tmp/acl-dir
echo "ACLs eliminadas. Verificar:"
getfacl /tmp/acl-dir/file1.txt

echo ""
echo "=== Restaurar desde backup ==="
cd /tmp && setfacl --restore=acl-backup.txt
echo "ACLs restauradas. Verificar:"
getfacl /tmp/acl-dir/file1.txt

rm -f /tmp/acl-backup.txt
'
```

> **Nota:** `setfacl --restore` usa rutas relativas del archivo de backup.
> Ejecuta el restore desde el mismo directorio donde corriste `getfacl -R`.

---

### Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/acl-test /tmp/acl-dir /tmp/mask-test /tmp/acl-backup.txt 2>/dev/null
userdel -r acluser1 2>/dev/null
userdel -r acluser2 2>/dev/null
groupdel aclteam 2>/dev/null
echo "Limpieza completada"
'
```

---

## Ejercicios

### Ejercicio 1 — ACL básica: resolver el problema owner/group/others

```bash
useradd -m alice 2>/dev/null
useradd -m bob 2>/dev/null
groupadd -f dev

touch /tmp/report.csv
chown alice:dev /tmp/report.csv
chmod 640 /tmp/report.csv

# alice (owner) tiene rw, grupo dev tiene r
# bob NO es miembro de dev — ¿puede leer?
su -c 'cat /tmp/report.csv' bob 2>&1; echo "exit: $?"

# Dar acceso a bob sin meterlo en el grupo dev
setfacl -m u:bob:r /tmp/report.csv
su -c 'cat /tmp/report.csv' bob 2>&1; echo "exit: $?"

getfacl /tmp/report.csv
ls -la /tmp/report.csv
```

**Pregunta:** ¿Puede bob leer antes y después de la ACL? ¿Qué indica el `+`
en `ls -la`?

<details><summary>Predicción</summary>

**Antes de la ACL:** bob **no** puede leer. Los permisos son `640`:
- owner (alice): rw
- group (dev): r — bob no es miembro de dev
- others: --- — bob cae aquí, sin acceso

```
cat: /tmp/report.csv: Permission denied
exit: 1
```

**Después de la ACL:** bob **sí** puede leer. La ACL `u:bob:r` le da acceso
directo sin necesitar membresía de grupo.

```
exit: 0
```

```
getfacl:
  user::rw-
  user:bob:r--      ← ACL explícita para bob
  group::r--
  mask::r--
  other::---

ls -la:
  -rw-r-----+ 1 alice dev ... /tmp/report.csv
             ^
             + indica ACLs extendidas presentes
```

Este es exactamente el problema que ACLs resuelven: dar acceso granular a
usuarios/grupos específicos sin modificar owner, group, ni others.

</details>

---

### Ejercicio 2 — La máscara como techo

```bash
touch /tmp/mask-demo
setfacl -m u:nobody:rw /tmp/mask-demo
echo "=== Mask auto-calculada ==="
getfacl /tmp/mask-demo

setfacl -m m::r /tmp/mask-demo
echo "=== Mask restringida a r ==="
getfacl /tmp/mask-demo
```

**Pregunta:** Después de restringir la mask a `r`, ¿qué permiso efectivo
tiene nobody? ¿Su ACL (`rw-`) cambió?

<details><summary>Predicción</summary>

```
=== Mask auto-calculada ===
user::rw-
user:nobody:rw-      ← ACL: rw
mask::rw-            ← mask: rw (auto-calculada para permitir rw)
other::r--

=== Mask restringida a r ===
user::rw-
user:nobody:rw-      #effective:r--    ← ACL dice rw, PERO...
mask::r--            ← mask: r (limita a solo lectura)
other::r--
```

La ACL de nobody **no cambió** — sigue diciendo `rw-`. Pero el permiso
**efectivo** es solo `r--` porque la mask lo limita.

El permiso efectivo se calcula como: `ACL_entry AND mask`
```
rw- AND r-- = r--
```

La mask es un techo: ninguna entrada ACL (excepto owner y others) puede
tener más permisos que la mask. Esto permite restringir temporalmente
todos los accesos ACL con un solo comando.

</details>

---

### Ejercicio 3 — chmod g= modifica la mask (trampa)

```bash
touch /tmp/chmod-mask
chmod 644 /tmp/chmod-mask
setfacl -m u:nobody:rw /tmp/chmod-mask

echo "=== Antes de chmod ==="
getfacl /tmp/chmod-mask

chmod g=r /tmp/chmod-mask

echo "=== Después de chmod g=r ==="
getfacl /tmp/chmod-mask
ls -la /tmp/chmod-mask
```

**Pregunta:** `chmod g=r` cambió los permisos del grupo propietario o de la
mask? ¿Qué muestra el campo de grupo en `ls -la`?

<details><summary>Predicción</summary>

`chmod g=r` modificó la **mask**, no los permisos del grupo propietario.

```
=== Antes de chmod ===
user::rw-
user:nobody:rw-
group::r--            ← grupo propietario: r
mask::rw-             ← mask: rw
other::r--

=== Después de chmod g=r ===
user::rw-
user:nobody:rw-      #effective:r--    ← limitado por nueva mask
group::r--            ← grupo propietario: SIGUE r (no cambió)
mask::r--             ← mask: ahora r (fue modificada por chmod)
other::r--
```

`ls -la` mostraría:
```
-rw-r--r--+ 1 ...
```

El campo de grupo (`r--`) muestra la **mask**, no los permisos del grupo
propietario. Esto es una fuente frecuente de confusión:

- **Sin ACLs:** `ls -la` muestra permisos del grupo en la posición de grupo
- **Con ACLs:** `ls -la` muestra la **mask** en la posición de grupo

Para ver los permisos reales del grupo, usar `getfacl`.

</details>

---

### Ejercicio 4 — Default ACLs: herencia automática

```bash
useradd -m devuser 2>/dev/null
groupadd -f projteam

mkdir /tmp/acl-project
chown root:projteam /tmp/acl-project
chmod 770 /tmp/acl-project

# ACL por defecto: devuser tendrá rw en todo archivo nuevo
setfacl -d -m u:devuser:rw /tmp/acl-project

# Crear archivo y subdirectorio dentro
touch /tmp/acl-project/main.py
mkdir /tmp/acl-project/src
touch /tmp/acl-project/src/utils.py

echo "=== Directorio ==="
getfacl /tmp/acl-project
echo "=== Archivo ==="
getfacl /tmp/acl-project/main.py
echo "=== Subdirectorio ==="
getfacl /tmp/acl-project/src
echo "=== Archivo en subdirectorio ==="
getfacl /tmp/acl-project/src/utils.py
```

**Pregunta:** ¿`main.py` y `utils.py` tienen la ACL de `devuser`? ¿El
subdirectorio `src/` tiene default ACLs que propagará?

<details><summary>Predicción</summary>

Sí a todo.

**`main.py`**: Heredó la ACL `user:devuser:rw-` del directorio padre.
```
user:devuser:rw-    ← heredada de la default ACL del padre
```

**`src/`**: Heredó la ACL como acceso directo Y como default ACL propia.
```
user:devuser:rw-           ← ACL de acceso (puede entrar al dir)
default:user:devuser:rw-   ← default ACL (propagará a sus hijos)
```

**`utils.py`**: Heredó la ACL del subdirectorio `src/`, que a su vez la
heredó del directorio padre.
```
user:devuser:rw-    ← heredada de src/, que la heredó del padre
```

La herencia es recursiva: cada subdirectorio nuevo hereda las default ACLs
del padre y las establece como sus propias default ACLs. Así, toda la
jerarquía mantiene las ACLs indefinidamente.

</details>

---

### Ejercicio 5 — Default ACLs anulan la umask

```bash
mkdir /tmp/acl-umask-test
setfacl -d -m u::rwx,g::rwx,o::r /tmp/acl-umask-test

echo "=== Default ACLs del directorio ==="
getfacl /tmp/acl-umask-test | grep default

# Crear archivo con umask restrictiva
umask 077
touch /tmp/acl-umask-test/file-with-acl.txt
stat -c '%a' /tmp/acl-umask-test/file-with-acl.txt
getfacl /tmp/acl-umask-test/file-with-acl.txt

# Comparar: archivo fuera del directorio con la misma umask
touch /tmp/file-without-acl.txt
stat -c '%a' /tmp/file-without-acl.txt

umask 022
rm -rf /tmp/acl-umask-test /tmp/file-without-acl.txt
```

**Pregunta:** Con umask `077`, ¿el archivo dentro del directorio con default
ACLs tiene permisos `600` (como lo haría normalmente)?

<details><summary>Predicción</summary>

No. Los permisos son **diferentes** en cada caso:

**Dentro del directorio con default ACLs:**
```
stat: 664 (o similar, dependiendo de las default ACLs)
```
La umask `077` fue **ignorada**. Las default ACLs del directorio reemplazan
la función de la umask. Los permisos vienen de las entradas default, no
de `666 AND NOT(umask)`.

**Fuera del directorio (sin default ACLs):**
```
stat: 600
```
La umask `077` se aplicó normalmente: `666 AND NOT(077) = 600`.

Esta es una propiedad fundamental: cuando un directorio tiene default ACLs,
la umask del proceso **no se aplica** a los archivos creados dentro. Es
la default ACL mask la que actúa como techo de permisos.

Esto tiene una ventaja práctica: los permisos de archivos nuevos son
**predecibles** sin importar la umask de cada usuario. Es más confiable
que depender de que todos los usuarios configuren su umask correctamente.

</details>

---

### Ejercicio 6 — Evaluación de ACLs: el owner queda "atrapado"

```bash
useradd -m aclowner 2>/dev/null

touch /tmp/owner-trap
chown aclowner /tmp/owner-trap
chmod 400 /tmp/owner-trap      # owner: r--, group: ---, others: ---

# Dar rw al owner vía ACL named user
setfacl -m u:aclowner:rw /tmp/owner-trap

echo "=== ACLs ==="
getfacl /tmp/owner-trap

# ¿Puede el owner escribir?
su -c 'echo "test" >> /tmp/owner-trap' aclowner 2>&1; echo "exit: $?"
```

**Pregunta:** El owner tiene `user::r--` pero también
`user:aclowner:rw-`. ¿Puede escribir?

<details><summary>Predicción</summary>

**No.** El owner no puede escribir. Exit code: 1.

```
ACLs:
user::r--              ← owner entry: solo lectura
user:aclowner:rw-      ← named user entry: lectura/escritura
group::---
mask::rw-
other::---
```

Aunque `aclowner` tiene una named user ACL con `rw`, el kernel usa la regla
**owner entry primero**: si el UID del proceso coincide con el owner del
archivo, se usa `user::` y se ignoran las named user entries.

El orden de evaluación es estricto:
1. ¿Es el owner? → usa `user::` → **primera coincidencia gana**
2. ¿Tiene named user ACL? → (no se evalúa porque ya coincidió como owner)

Para que el owner pueda escribir, hay que cambiar `user::rw-` (con
`chmod u+w` o `setfacl -m u::rw`), no agregar una named user ACL.

Esto es análogo al comportamiento de permisos Unix tradicionales: el owner
queda "atrapado" en sus propios permisos de owner, incluso si el grupo
o named ACLs darían más permisos.

</details>

---

### Ejercicio 7 — setfacl -R vs setfacl -Rd

```bash
mkdir -p /tmp/acl-recursive/{src,docs}
touch /tmp/acl-recursive/README
touch /tmp/acl-recursive/src/main.c
touch /tmp/acl-recursive/docs/guide.txt

# Aplicar solo a lo existente
setfacl -R -m u:nobody:r /tmp/acl-recursive

# Crear archivo nuevo DESPUÉS de -R
touch /tmp/acl-recursive/src/new-file.c

echo "=== Archivo existente (aplicado por -R) ==="
getfacl /tmp/acl-recursive/src/main.c | grep nobody

echo "=== Archivo nuevo (creado después de -R) ==="
getfacl /tmp/acl-recursive/src/new-file.c | grep nobody

# Ahora aplicar -Rd para cubrir archivos futuros
setfacl -R -d -m u:nobody:r /tmp/acl-recursive

touch /tmp/acl-recursive/src/future-file.c

echo "=== Archivo futuro (después de -Rd) ==="
getfacl /tmp/acl-recursive/src/future-file.c | grep nobody

rm -rf /tmp/acl-recursive
```

**Pregunta:** ¿El archivo `new-file.c` (creado después de `-R`) tiene la ACL?
¿Y `future-file.c` (creado después de `-Rd`)?

<details><summary>Predicción</summary>

```
=== Archivo existente ===
user:nobody:r--         ← SÍ (aplicado por -R)

=== Archivo nuevo ===
(ningún resultado)      ← NO (creado DESPUÉS de -R, sin default ACLs)

=== Archivo futuro ===
user:nobody:r--         ← SÍ (heredado de default ACL)
```

- `setfacl -R -m` aplica ACLs a **lo que ya existe** en ese momento.
  No afecta archivos creados después.
- `setfacl -R -d -m` establece **default ACLs** en todos los directorios
  existentes. Los archivos creados **después** dentro de esos directorios
  heredarán las ACLs automáticamente.

**En la práctica, se necesitan ambos comandos:**
```bash
setfacl -R -m u:nobody:r /path       # para lo que ya existe
setfacl -R -d -m u:nobody:r /path    # para lo que se cree en el futuro
```

</details>

---

### Ejercicio 8 — Backup y restore de ACLs

```bash
mkdir -p /tmp/acl-backup-test/data
touch /tmp/acl-backup-test/data/file1.txt
setfacl -m u:nobody:rw /tmp/acl-backup-test/data/file1.txt
setfacl -d -m u:nobody:rw /tmp/acl-backup-test/data

echo "=== ACLs originales ==="
getfacl /tmp/acl-backup-test/data/file1.txt

# Backup
getfacl -R /tmp/acl-backup-test > /tmp/acl-snapshot.txt

# Destruir ACLs
setfacl -R -b /tmp/acl-backup-test
echo "=== ACLs después de -b ==="
getfacl /tmp/acl-backup-test/data/file1.txt

# Restaurar
cd /tmp && setfacl --restore=acl-snapshot.txt
echo "=== ACLs restauradas ==="
getfacl /tmp/acl-backup-test/data/file1.txt

rm -rf /tmp/acl-backup-test /tmp/acl-snapshot.txt
```

**Pregunta:** Después de `setfacl -b` (eliminar todas), ¿la restauración
recupera tanto las ACLs de acceso como las default?

<details><summary>Predicción</summary>

Sí. `setfacl --restore` recupera todo lo que `getfacl -R` guardó:

```
=== ACLs originales ===
user::rw-
user:nobody:rw-        ← ACL de acceso
group::r--
mask::rw-
other::r--

=== ACLs después de -b ===
user::rw-
group::r--
other::r--             ← solo permisos Unix, sin ACLs

=== ACLs restauradas ===
user::rw-
user:nobody:rw-        ← ACL restaurada
group::r--
mask::rw-
other::r--
```

El archivo de backup contiene las ACLs en formato texto. `--restore` las
aplica exactamente como estaban. Las default ACLs del directorio `data/`
también se restauran.

**Buena práctica:** hacer `getfacl -R > backup` antes de operaciones que
puedan perder ACLs (como `cp` sin `-a`, `tar` sin `--acls`, o scripts
que hacen `chmod -R`).

</details>

---

### Ejercicio 9 — Encontrar archivos con ACLs

```bash
# Preparar archivos de prueba
touch /tmp/acl-find-{1,2,3}
setfacl -m u:nobody:r /tmp/acl-find-1
setfacl -m u:nobody:rw /tmp/acl-find-3

# Buscar archivos con ACLs
echo "=== Archivos con ACLs en /tmp ==="
ls -la /tmp/acl-find-* | grep '+'

echo ""
echo "=== Método más confiable con getfacl ==="
for f in /tmp/acl-find-*; do
    if getfacl -p "$f" 2>/dev/null | grep -q "^user:[^:]*:"; then
        echo "$f tiene ACLs de usuario"
    fi
done

rm -f /tmp/acl-find-{1,2,3}
```

**Pregunta:** ¿Cuáles de los tres archivos tienen ACLs? ¿Cómo los
identifica `ls -la`?

<details><summary>Predicción</summary>

Solo `acl-find-1` y `acl-find-3` tienen ACLs. `acl-find-2` no.

```
=== ls -la ===
-rw-r--r--+ ... /tmp/acl-find-1    ← + presente
-rw-r--r--  ... /tmp/acl-find-2    ← sin +
-rw-r--r--+ ... /tmp/acl-find-3    ← + presente
```

El `+` al final del campo de permisos en `ls -la` indica que el archivo
tiene ACLs extendidas. Es una señal visual rápida.

Para auditorías más completas, usar `getfacl` con grep es más confiable
que parsear la salida de `ls`. El patrón `^user:[^:]*:` busca líneas que
empiecen con `user:` seguido de un nombre (no vacío), que son las named
user ACL entries.

En sistemas grandes, se puede usar:
```bash
find /path -type f -exec sh -c \
  'getfacl -p "$1" 2>/dev/null | grep -q "^user:[^:]*:" && echo "$1"' \
  _ {} \;
```

</details>

---

### Ejercicio 10 — ACL completa: escenario multiusuario

```bash
useradd -m pm 2>/dev/null       # project manager
useradd -m dev1 2>/dev/null     # developer
useradd -m auditor 2>/dev/null  # auditor externo
groupadd -f desarrollo

# Directorio de proyecto
mkdir /tmp/proyecto
chown pm:desarrollo /tmp/proyecto
chmod 770 /tmp/proyecto

# Requisitos:
# - pm (owner): rwx en todo
# - grupo desarrollo: rwx en todo
# - dev1 (no en grupo desarrollo): rw en archivos
# - auditor: solo lectura en archivos, puede entrar a directorios
setfacl -m u:dev1:rwx,u:auditor:rx /tmp/proyecto
setfacl -d -m u:dev1:rw,u:auditor:r /tmp/proyecto

# Crear estructura
su -c 'touch /tmp/proyecto/roadmap.md' pm
su -c 'mkdir /tmp/proyecto/sprint-1' pm
su -c 'touch /tmp/proyecto/sprint-1/task.py' pm

echo "=== ACLs del directorio ==="
getfacl /tmp/proyecto

echo "=== ACLs del archivo ==="
getfacl /tmp/proyecto/roadmap.md

echo "=== Pruebas de acceso ==="
echo "dev1 escribe:"
su -c 'echo "# TODO" >> /tmp/proyecto/roadmap.md && echo OK' dev1 2>&1

echo "auditor lee:"
su -c 'cat /tmp/proyecto/roadmap.md' auditor 2>&1

echo "auditor escribe:"
su -c 'echo "hack" >> /tmp/proyecto/roadmap.md' auditor 2>&1; echo "exit: $?"
```

**Pregunta:** ¿dev1 puede escribir? ¿El auditor puede leer pero no escribir?
¿Los archivos en `sprint-1/` heredan las ACLs?

<details><summary>Predicción</summary>

**dev1 puede escribir:** Sí. Tiene ACL `u:dev1:rw` (heredada de default
ACL). `echo OK`.

**auditor puede leer:** Sí. Tiene ACL `u:auditor:r` (heredada).

**auditor no puede escribir:** Correcto. Su ACL solo tiene `r`, no `w`.
```
Permission denied
exit: 1
```

**Archivos en sprint-1/ heredan ACLs:** Sí. `sprint-1/` heredó las default
ACLs del directorio padre (incluyendo sus propias default ACLs), y
`task.py` dentro de `sprint-1/` heredó las ACLs de `sprint-1/`.

```
getfacl /tmp/proyecto/roadmap.md:
  user::rw-
  user:dev1:rw-        ← puede escribir
  user:auditor:r--     ← solo lectura
  group::rwx           #effective:rw-
  mask::rw-
  other::---

getfacl /tmp/proyecto/sprint-1/task.py:
  user:dev1:rw-        ← heredado recursivamente
  user:auditor:r--     ← heredado recursivamente
```

Este escenario sería **imposible** con permisos Unix tradicionales:
se necesitarían al menos 3 niveles de acceso distintos (rwx, rw, r) para
3 entidades diferentes (owner, dev1, auditor), pero Unix solo permite
owner/group/others.

</details>

---

## Limpieza de ejercicios

```bash
userdel -r alice 2>/dev/null
userdel -r bob 2>/dev/null
userdel -r devuser 2>/dev/null
userdel -r aclowner 2>/dev/null
userdel -r pm 2>/dev/null
userdel -r dev1 2>/dev/null
userdel -r auditor 2>/dev/null
groupdel dev 2>/dev/null
groupdel projteam 2>/dev/null
groupdel desarrollo 2>/dev/null
rm -rf /tmp/report.csv /tmp/mask-demo /tmp/chmod-mask 2>/dev/null
rm -rf /tmp/acl-project /tmp/acl-umask-test /tmp/file-without-acl.txt 2>/dev/null
rm -rf /tmp/owner-trap /tmp/acl-recursive /tmp/proyecto 2>/dev/null
echo "Limpieza completada"
```
