# Lab — Contraseñas

## Objetivo

Gestionar contrasenas con passwd y chage: establecer y verificar
contrasenas, configurar politicas de expiracion, inspeccionar
algoritmos de hash, y comparar el comportamiento entre Debian y
AlmaLinux.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — passwd y estado

### Objetivo

Usar passwd para establecer contrasenas, ver su estado, y entender
bloqueo vs eliminacion.

### Paso 1.1: Crear usuario de prueba

```bash
docker compose exec debian-dev bash -c '
useradd -m -s /bin/bash pwtest
echo "pwtest:Test1234" | chpasswd

echo "=== Usuario creado ==="
getent passwd pwtest
'
```

`chpasswd` permite establecer contrasenas de forma no interactiva.
Util en scripts de automatizacion.

### Paso 1.2: Ver estado de la contrasena

```bash
docker compose exec debian-dev bash -c '
echo "=== Estado de la contrasena ==="
passwd -S pwtest
echo ""
echo "Formato: usuario estado fecha min max warn inactive"
echo "Estado: P=password set, L=locked, NP=no password"
'
```

`passwd -S` muestra un resumen de una linea con el estado de la
contrasena y los parametros de expiracion.

### Paso 1.3: Bloquear contrasena

```bash
docker compose exec debian-dev bash -c '
echo "=== Antes de bloquear ==="
passwd -S pwtest
echo "Hash (primeros 10 chars):"
grep "^pwtest:" /etc/shadow | cut -d: -f2 | head -c 10
echo "..."

echo ""
echo "=== Bloquear ==="
passwd -l pwtest

echo ""
echo "=== Despues de bloquear ==="
passwd -S pwtest
echo "Hash (primeros 10 chars):"
grep "^pwtest:" /etc/shadow | cut -d: -f2 | head -c 10
echo "..."
echo "(el ! se antepuso al hash — el hash original se preserva)"
'
```

`passwd -l` antepone `!` al hash. No destruye la contrasena — se
puede desbloquear con `passwd -u` y la contrasena original funciona.

### Paso 1.4: Desbloquear

```bash
docker compose exec debian-dev bash -c '
echo "=== Desbloquear ==="
passwd -u pwtest

echo ""
echo "=== Estado ==="
passwd -S pwtest
echo "(vuelve a P = password set)"
'
```

### Paso 1.5: Eliminar contrasena (inseguro)

```bash
docker compose exec debian-dev bash -c '
echo "=== Eliminar contrasena ==="
passwd -d pwtest

echo ""
echo "=== Estado ==="
passwd -S pwtest
echo "(NP = no password — el usuario puede hacer login sin contrasena)"

echo ""
echo "=== PELIGRO: campo vacio en shadow ==="
grep "^pwtest:" /etc/shadow | cut -d: -f1-3
echo "(el campo 2 esta vacio — cualquiera puede entrar)"

echo ""
echo "=== Restaurar contrasena ==="
echo "pwtest:Test1234" | chpasswd
'
```

`passwd -d` elimina la contrasena completamente. El usuario puede
hacer login sin contrasena. Nunca hacer esto en produccion.

---

## Parte 2 — chage y politicas

### Objetivo

Configurar politicas de expiracion de contrasenas con chage.

### Paso 2.1: Informacion actual

```bash
docker compose exec debian-dev bash -c '
echo "=== Informacion de expiracion ==="
chage -l pwtest
'
```

Por defecto: sin expiracion (max=99999), sin minimo entre cambios,
7 dias de aviso.

### Paso 2.2: Configurar politica tipica

```bash
docker compose exec debian-dev bash -c '
echo "=== Configurar politica ==="
chage -M 90 pwtest    # expira cada 90 dias
chage -m 1 pwtest     # minimo 1 dia entre cambios
chage -W 14 pwtest    # aviso 14 dias antes
chage -I 30 pwtest    # 30 dias de gracia

echo ""
echo "=== Verificar ==="
chage -l pwtest
'
```

`-M` = maximo dias. `-m` = minimo dias (evita rotar y volver a la
anterior). `-W` = dias de aviso. `-I` = dias de gracia despues de
expirar.

### Paso 2.3: Forzar cambio en proximo login

```bash
docker compose exec debian-dev bash -c '
echo "=== Campo last_change actual ==="
grep "^pwtest:" /etc/shadow | cut -d: -f3

echo ""
echo "=== Forzar cambio ==="
chage -d 0 pwtest

echo ""
echo "=== Campo last_change ahora ==="
grep "^pwtest:" /etc/shadow | cut -d: -f3
echo "(0 = la contrasena se considera expirada inmediatamente)"

echo ""
echo "=== chage -l confirma ==="
chage -l pwtest | head -3

echo ""
echo "=== Restaurar ==="
chage -d $(( $(date +%s) / 86400 )) pwtest
'
```

`chage -d 0` establece el campo last_change a 0. El sistema
interpreta esto como "la contrasena nunca se ha cambiado" y fuerza
el cambio en el proximo login.

### Paso 2.4: Expiracion de cuenta vs contrasena

```bash
docker compose exec debian-dev bash -c '
echo "=== Establecer expiracion de CUENTA ==="
chage -E 2025-12-31 pwtest

echo ""
echo "=== Verificar ==="
chage -l pwtest | grep -i "account expires"

echo ""
echo "=== Quitar expiracion ==="
chage -E -1 pwtest
chage -l pwtest | grep -i "account expires"
'
```

Expiracion de contrasena: el usuario debe cambiarla pero la cuenta
sigue activa. Expiracion de cuenta: la cuenta se desactiva
completamente en esa fecha (util para contratistas temporales).

### Paso 2.5: Ver campos raw en shadow

```bash
docker compose exec debian-dev bash -c '
echo "=== Campos de shadow para pwtest ==="
IFS=: read -r user hash lc min max warn inact exp reserved < <(grep "^pwtest:" /etc/shadow)
echo "usuario:    $user"
echo "hash:       ${hash:0:10}..."
echo "last_change: $lc (dias desde epoch)"
echo "min:        $min"
echo "max:        $max"
echo "warn:       $warn"
echo "inactive:   $inact"
echo "expire:     $exp"
echo "reserved:   $reserved"
'
```

---

## Parte 3 — Algoritmos y comparacion

### Objetivo

Comparar los algoritmos de hash entre distribuciones y entender
los prefijos.

### Paso 3.1: Algoritmo de cada distribucion

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
echo "Configurado: $(grep ENCRYPT_METHOD /etc/login.defs | awk "{print \$2}")"
echo "Prefijo del hash de dev:"
HASH=$(grep "^dev:" /etc/shadow | cut -d: -f2)
echo "${HASH:0:4}..."
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
echo "Configurado: $(grep ENCRYPT_METHOD /etc/login.defs | awk "{print \$2}")"
echo "Prefijo del hash de dev:"
HASH=$(grep "^dev:" /etc/shadow | cut -d: -f2)
echo "${HASH:0:4}..."
'
```

Debian 12: yescrypt (`$y$`). AlmaLinux 9: SHA-512 (`$6$`).
Los algoritmos modernos son intencionalmente lentos para dificultar
fuerza bruta.

### Paso 3.2: Mismo password, diferente salt

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear dos usuarios con la misma contrasena ==="
useradd -m hashtest1
useradd -m hashtest2
echo "hashtest1:SamePassword" | chpasswd
echo "hashtest2:SamePassword" | chpasswd

echo ""
echo "=== Comparar hashes ==="
echo "hashtest1: $(grep "^hashtest1:" /etc/shadow | cut -d: -f2 | head -c 30)..."
echo "hashtest2: $(grep "^hashtest2:" /etc/shadow | cut -d: -f2 | head -c 30)..."
echo ""
echo "(diferentes porque cada uno tiene un salt aleatorio distinto)"

userdel -r hashtest1
userdel -r hashtest2
'
```

El salt aleatorio garantiza que dos usuarios con la misma contrasena
tengan hashes completamente diferentes. Esto invalida los ataques
con rainbow tables.

### Paso 3.3: Tabla de prefijos

```bash
docker compose exec debian-dev bash -c '
echo "=== Prefijos de algoritmos ==="
echo "\$1\$  = MD5       (obsoleto, inseguro)"
echo "\$5\$  = SHA-256   (aceptable)"
echo "\$6\$  = SHA-512   (estandar actual en RHEL)"
echo "\$y\$  = yescrypt  (default Debian 12+, mas lento = mas seguro)"
echo "\$2b\$ = bcrypt    (usado en algunos BSDs)"
'
```

### Paso 3.4: Configuracion de PAM

```bash
echo "=== Debian: PAM para contrasenas ==="
docker compose exec debian-dev bash -c '
ls /etc/pam.d/ | head -10
echo "..."
echo ""
echo "--- Modulo de contrasenas ---"
cat /etc/pam.d/common-password 2>/dev/null | grep -v "^#" | grep -v "^$" || echo "(no existe)"
'

echo ""
echo "=== AlmaLinux: PAM para contrasenas ==="
docker compose exec alma-dev bash -c '
ls /etc/pam.d/ | head -10
echo "..."
echo ""
echo "--- Modulo de contrasenas ---"
cat /etc/pam.d/system-auth 2>/dev/null | grep -v "^#" | grep -v "^$" | head -10 || echo "(no existe)"
'
```

Debian usa `/etc/pam.d/common-*`. AlmaLinux usa
`/etc/pam.d/system-auth` y `password-auth`. PAM es el framework
que controla como se autentican los usuarios.

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
userdel -r pwtest 2>/dev/null
userdel -r hashtest1 2>/dev/null
userdel -r hashtest2 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. `passwd -S` muestra el estado de la contrasena en una linea.
   `passwd -l` bloquea anteponiendo `!` al hash (preserva la
   contrasena original).

2. `chage -l` muestra las politicas de expiracion. `-M` para maximo
   dias, `-m` para minimo, `-W` para aviso, `-I` para gracia.

3. `chage -d 0` fuerza cambio de contrasena en el proximo login.
   `chage -E` controla la expiracion de la **cuenta** (no de la
   contrasena).

4. Debian usa yescrypt (`$y$`), AlmaLinux usa SHA-512 (`$6$`). Los
   algoritmos modernos son intencionalmente lentos para dificultar
   fuerza bruta.

5. El salt aleatorio garantiza que la misma contrasena produce
   hashes diferentes para cada usuario.
