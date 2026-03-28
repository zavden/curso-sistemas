# T03 — Dependencias

## Dos ejes: activación y ordenamiento

Las dependencias en systemd se organizan en dos dimensiones independientes:

1. **Activación** — ¿se arranca la otra unidad?
   - `Wants=`, `Requires=`, `BindsTo=`, `Requisite=`
2. **Ordenamiento** — ¿en qué orden arrancan?
   - `After=`, `Before=`

Estos son **independientes**. `Requires=` sin `After=` arranca ambas unidades
**en paralelo**. `After=` sin `Requires=` define orden pero no arranca la
dependencia.

```ini
# Patrón habitual — activación + orden:
[Unit]
Requires=postgresql.service    # arrancar postgresql
After=postgresql.service       # esperar a que esté listo

# Solo orden (postgresql lo arranca otro):
[Unit]
After=postgresql.service       # si postgresql está activo, arrancar después

# Solo activación (arrancar en paralelo):
[Unit]
Requires=redis.service         # arrancar redis, sin esperar
```

## Dependencias de activación

### Wants= — Dependencia débil

```ini
[Unit]
Wants=redis.service
After=redis.service
```

- Intenta arrancar `redis.service`
- Si redis **falla al arrancar**, la unidad actual **arranca igual**
- Si redis **se detiene después**, la unidad actual **sigue corriendo**
- Es la dependencia más usada — tolerante a fallos

```bash
# Crear dependencias Wants sin editar la unidad:
sudo mkdir -p /etc/systemd/system/myapp.service.d/
echo -e "[Unit]\nWants=redis.service\nAfter=redis.service" | \
    sudo tee /etc/systemd/system/myapp.service.d/wants-redis.conf

# O desde el directorio de wants del target:
sudo systemctl enable redis.service
# Crea el symlink en multi-user.target.wants/
# Esto es un Wants= dinámico
```

### Requires= — Dependencia fuerte

```ini
[Unit]
Requires=postgresql.service
After=postgresql.service
```

- Arranca `postgresql.service`
- Si postgresql **falla al arrancar**, la unidad actual **no arranca**
- Si postgresql **se detiene después**, la unidad actual **se detiene también**
- Usar cuando la unidad no puede funcionar sin la dependencia

### BindsTo= — Dependencia muy fuerte

```ini
[Unit]
BindsTo=special-device.device
After=special-device.device
```

- Como `Requires=`, pero aún más estricto
- Si la dependencia entra en **cualquier estado inactivo** (no solo stop
  explícito), la unidad actual se detiene
- Útil para unidades que dependen de dispositivos o mounts

### Requisite= — Dependencia ya activa

```ini
[Unit]
Requisite=postgresql.service
After=postgresql.service
```

- **No** arranca `postgresql.service`
- Si postgresql **ya está activo**, la unidad arranca
- Si postgresql **no está activo**, la unidad **falla inmediatamente**
- Útil cuando quieres verificar que algo ya está corriendo sin arrancarlo

### PartOf= — Propagación de stop/restart

```ini
[Unit]
PartOf=myapp.service
```

- Si `myapp.service` se **detiene o reinicia**, esta unidad también
- NO propaga el start (no arranca esta unidad al arrancar myapp)
- Útil para componentes auxiliares (workers, sidecars)

```ini
# Ejemplo: app principal con worker
# myapp.service
[Unit]
Description=Main Application
Wants=myapp-worker.service

# myapp-worker.service
[Unit]
Description=Background Worker
PartOf=myapp.service
After=myapp.service

# Al hacer systemctl restart myapp:
# 1. myapp se reinicia
# 2. myapp-worker se reinicia automáticamente (PartOf)
```

## Dependencias de ordenamiento

### After= y Before=

```ini
# After= — arrancar DESPUÉS de la unidad indicada:
[Unit]
After=network.target postgresql.service
# Si network.target y postgresql.service están activos o arrancando,
# esperar a que terminen antes de arrancar esta unidad

# Before= — arrancar ANTES de la unidad indicada:
[Unit]
Before=nginx.service
# Si nginx.service va a arrancar, esperar a que esta unidad arranque primero
```

```bash
# After/Before NO activan la dependencia:
# Si postgresql.service no está activo y nadie lo arranca,
# After=postgresql.service simplemente se ignora

# El patrón completo es:
Requires=postgresql.service    # asegurar que arranca
After=postgresql.service       # esperar a que esté listo
```

### Sin ordenamiento — Arranque paralelo

```ini
# Sin After/Before, las unidades arrancan EN PARALELO:
[Unit]
Wants=redis.service
Wants=memcached.service
# redis y memcached arrancan al mismo tiempo que esta unidad
# Esto es bueno para rendimiento del boot (paralelismo)
```

## Conflicts= — Incompatibilidad

```ini
[Unit]
Conflicts=apache2.service
```

- Si `apache2.service` está corriendo, se **detiene** al arrancar esta unidad
- Si esta unidad está corriendo, se **detiene** al arrancar apache2
- Garantiza que ambas no corran simultáneamente

```ini
# Caso típico: iptables vs firewalld
# firewalld.service:
[Unit]
Conflicts=iptables.service ip6tables.service
```

## Targets como puntos de sincronización

Los targets agrupan dependencias y sirven como puntos de sincronización en
el proceso de boot:

```
sysinit.target
    ↓
basic.target
    ↓
network.target → network-online.target
    ↓
multi-user.target
    ↓
graphical.target
```

```ini
# network.target vs network-online.target:

# network.target — las interfaces están configuradas (IP asignada)
# PERO puede no haber conectividad real todavía
After=network.target     # suficiente para servicios que escuchan

# network-online.target — hay conectividad real verificada
# Más lento (espera ping/route check)
After=network-online.target
Wants=network-online.target
# Necesario para servicios que CONECTAN a hosts remotos al arrancar
# (ej: NFS mounts, clientes de base de datos remota)
```

## Visualizar dependencias

```bash
# Árbol de dependencias de una unidad:
systemctl list-dependencies nginx.service
# nginx.service
# ● ├─system.slice
# ● └─sysinit.target
# ●   ├─...

# Dependencias inversas (quién depende de esta unidad):
systemctl list-dependencies nginx.service --reverse
# nginx.service
# ● └─multi-user.target
# ●   └─graphical.target

# Todas las dependencias (recursivo):
systemctl list-dependencies nginx.service --all

# Árbol de boot completo:
systemctl list-dependencies default.target

# Diagrama de tiempos de boot:
systemd-analyze critical-chain
# graphical.target @12.345s
# └─multi-user.target @12.340s
#   └─nginx.service @10.100s +2.240s
#     └─network.target @10.050s
```

```bash
# systemd-analyze plot genera un gráfico SVG del boot:
systemd-analyze plot > /tmp/boot.svg
# Muestra cuánto tarda cada unidad y el paralelismo

# Tiempos de arranque por unidad:
systemd-analyze blame
# 5.100s cloud-init.service
# 3.200s NetworkManager.service
# 2.240s nginx.service
# ...
```

## Dependencias implícitas

Systemd crea algunas dependencias automáticamente:

```bash
# DefaultDependencies=yes (default para la mayoría de unidades):
# Agrega implícitamente:
#   Requires=sysinit.target
#   After=sysinit.target basic.target
#   Conflicts=shutdown.target
#   Before=shutdown.target

# Esto asegura que los servicios normales:
# - Arrancan después de la inicialización del sistema
# - Se detienen al apagar

# Para unidades que arrancan MUY temprano (antes de sysinit):
[Unit]
DefaultDependencies=no
# Necesario para unidades de early boot
```

## Errores comunes

### Dependency loop

```bash
# Si A After B, y B After A → loop de dependencias
# systemd detecta esto y lo reporta:
# systemd[1]: myapp.service: Found ordering cycle on other.service
# systemd[1]: myapp.service: job type fix attempt

# Solución: revisar la cadena de After/Before y romper el ciclo
systemd-analyze verify myapp.service
```

### Requires sin After

```ini
# Error sutil: Requires SIN After arranca en paralelo
[Unit]
Requires=postgresql.service
# postgresql arranca EN PARALELO con esta unidad
# Si la unidad necesita que postgresql esté listo → falla

# Correcto:
[Unit]
Requires=postgresql.service
After=postgresql.service
```

### network.target vs network-online.target

```ini
# Error: usar network.target para servicios que conectan a hosts remotos
[Unit]
After=network.target    # las interfaces tienen IP, pero puede no haber ruta

# Correcto para clientes remotos:
[Unit]
After=network-online.target
Wants=network-online.target
# Wants= necesario porque network-online.target no se activa por defecto
```

---

## Ejercicios

### Ejercicio 1 — Explorar dependencias

```bash
# 1. ¿De qué depende sshd.service?
systemctl list-dependencies sshd.service

# 2. ¿Qué depende de network.target?
systemctl list-dependencies network.target --reverse

# 3. ¿Cuántas unidades dependen del target por defecto?
systemctl list-dependencies "$(systemctl get-default)" --no-pager | wc -l
```

### Ejercicio 2 — Tiempos de boot

```bash
# ¿Cuáles son los 5 servicios más lentos al arrancar?
systemd-analyze blame --no-pager | head -5

# ¿Cuál es la cadena crítica del boot?
systemd-analyze critical-chain --no-pager
```

### Ejercicio 3 — Verificar una unidad

```bash
# Verificar que una unidad no tiene errores de dependencias:
systemd-analyze verify /usr/lib/systemd/system/sshd.service 2>&1

# Ver las dependencias implícitas:
systemctl show sshd.service --property=After,Requires,Wants --no-pager
```
