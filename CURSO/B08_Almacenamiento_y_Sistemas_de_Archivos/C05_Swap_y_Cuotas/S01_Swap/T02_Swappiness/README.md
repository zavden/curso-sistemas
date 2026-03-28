# Swappiness

## Índice

1. [Qué es vm.swappiness](#qué-es-vmswappiness)
2. [Cómo decide el kernel qué liberar](#cómo-decide-el-kernel-qué-liberar)
3. [Valores de swappiness](#valores-de-swappiness)
4. [Consultar y modificar swappiness](#consultar-y-modificar-swappiness)
5. [Persistencia con sysctl](#persistencia-con-sysctl)
6. [Impacto en rendimiento](#impacto-en-rendimiento)
7. [Cuándo ajustar swappiness](#cuándo-ajustar-swappiness)
8. [Otros parámetros relacionados](#otros-parámetros-relacionados)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Qué es vm.swappiness

`vm.swappiness` es un parámetro del kernel que influye en la **preferencia** entre dos estrategias para liberar memoria:

1. **Reclamar caché de páginas** (page cache) — liberar RAM usada como caché de disco
2. **Mover páginas anónimas a swap** — sacar datos de procesos de la RAM al swap

```
┌─────────────────────────────────────────────────────────┐
│            RAM bajo presión — ¿qué liberar?              │
│                                                         │
│   vm.swappiness = 60 (default)                          │
│                                                         │
│   ┌──────────────────┐    ┌──────────────────┐          │
│   │  Page cache      │    │  Páginas anónimas │          │
│   │  (archivos       │    │  (memoria de      │          │
│   │   leídos del     │    │   procesos, heap, │          │
│   │   disco, caché)  │    │   stack, mmap)    │          │
│   │                  │    │                   │          │
│   │  Liberar: rápido │    │  Swap out: lento  │          │
│   │  (solo descartar)│    │  (escribir a disco)│         │
│   └──────────────────┘    └──────────────────┘          │
│         ▲                          ▲                    │
│         │                          │                    │
│    swappiness bajo            swappiness alto           │
│    (prefiere descartar        (prefiere mover a         │
│     caché)                     swap)                    │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

Swappiness **no** controla cuánto swap se usa ni a qué porcentaje de RAM se empieza a swapear. Es una **preferencia relativa** entre dos formas de recuperar memoria.

---

## Cómo decide el kernel qué liberar

Cuando la RAM está bajo presión, el kernel tiene dos pools de memoria reclamable:

### Page cache (caché de archivos)

Datos de archivos leídos del disco que el kernel mantiene en RAM para acelerar lecturas futuras. Son descartables porque siempre se pueden releer del disco.

```bash
# Ver cuánta RAM usa el page cache
free -h
#               total   used   free  shared  buff/cache  available
# Mem:          1.9Gi  500Mi   200Mi   10Mi    1.2Gi      1.3Gi
#                                              ^^^^
#                                         buff/cache = page cache + buffers
```

### Páginas anónimas

Memoria asignada por procesos (variables, objetos, heap) que no tiene respaldo en disco. Para liberarla, el kernel debe escribirla al swap.

```
Page cache:     leer de disco → cachear en RAM → descartar si se necesita RAM
                (descartable sin escribir, el archivo sigue en disco)

Páginas anónimas: proceso pide malloc() → kernel asigna RAM
                  (para liberar: escribir a swap, operación costosa)
```

### El rol de swappiness

```
swappiness = 0:
  Kernel: "Casi nunca uses swap. Descarta page cache primero.
           Solo swap si es absolutamente necesario."

swappiness = 60 (default):
  Kernel: "Balance. Descarta algo de caché Y haz algo de swap,
           según qué páginas sean menos útiles."

swappiness = 100:
  Kernel: "Trata ambos pools con la misma agresividad.
           Swapea tanto como descartas caché."
```

> **Nota técnica**: internamente, swappiness se usa como peso en la función de reclaim del kernel. No es un porcentaje directo de "cuánto swapear".

---

## Valores de swappiness

| Valor | Comportamiento | Efecto práctico |
|-------|---------------|-----------------|
| `0` | Evitar swap al máximo | Solo swapea si la RAM está realmente agotada |
| `1` | Mínimo swap posible | Prácticamente igual a 0 pero no desactiva swap |
| `10` | Swap muy conservador | Preferencia fuerte por descartar caché |
| `30` | Swap moderadamente bajo | Buen balance para desktops |
| `60` | **Default** | Balance general, funciona para la mayoría |
| `80` | Swap agresivo | Más swapping, más RAM libre para caché |
| `100` | Máximo swap | Trata caché y swap igualmente |

### Rango

```
0 ─────────────────────────── 60 ──────────────────────── 100
│                              │                           │
▼                              ▼                           ▼
Casi nunca swap          Default Linux              Swap agresivo
Descartar caché          Balance                    Mantener caché
primero                                             swapear más
```

### vm.swappiness = 0: caso especial

En kernels modernos (≥3.5), `swappiness=0` significa: "no swapear a menos que la presión de memoria sea crítica". No desactiva swap completamente — el kernel todavía puede swapear en situaciones extremas para evitar el OOM killer.

Para desactivar swap completamente: `swapoff -a` (no recomendado en producción).

---

## Consultar y modificar swappiness

### Consultar valor actual

```bash
# Método 1: sysctl
sysctl vm.swappiness
# vm.swappiness = 60

# Método 2: /proc
cat /proc/sys/vm/swappiness
# 60
```

### Modificar temporalmente (hasta reinicio)

```bash
# Método 1: sysctl -w
sysctl -w vm.swappiness=10
# vm.swappiness = 10

# Método 2: escribir directamente
echo 10 > /proc/sys/vm/swappiness

# Verificar
sysctl vm.swappiness
# vm.swappiness = 10
```

> **Predicción**: el cambio es inmediato pero no retroactivo. Si ya hay páginas en swap, no se traen de vuelta a RAM automáticamente. El nuevo valor afecta las **futuras** decisiones de reclaim.

---

## Persistencia con sysctl

Para que el cambio sobreviva al reinicio, se configura en archivos de sysctl.

### Método estándar

```bash
# Crear archivo de configuración
echo 'vm.swappiness=10' > /etc/sysctl.d/99-swappiness.conf

# Aplicar sin reiniciar
sysctl -p /etc/sysctl.d/99-swappiness.conf
# vm.swappiness = 10

# Verificar
sysctl vm.swappiness
```

### Archivos de configuración sysctl

```
┌────────────────────────────────────────────────────────┐
│  Orden de lectura de sysctl:                           │
│                                                        │
│  /usr/lib/sysctl.d/*.conf   ← defaults del sistema     │
│  /run/sysctl.d/*.conf       ← runtime                  │
│  /etc/sysctl.d/*.conf       ← configuración local ✓    │
│  /etc/sysctl.conf           ← legacy (funciona)        │
│                                                        │
│  Los archivos se leen en orden alfabético.              │
│  99-swappiness.conf se lee al final → sobreescribe.     │
└────────────────────────────────────────────────────────┘
```

### Verificar persistencia

```bash
# Reiniciar
reboot

# Después del reinicio
sysctl vm.swappiness
# vm.swappiness = 10    ← persistió
```

---

## Impacto en rendimiento

### Swappiness bajo (0-10): priorizar procesos

```
Ventajas:
  ✓ Procesos rara vez sufren swap → respuesta más rápida
  ✓ No hay latencia de swap para aplicaciones interactivas
  ✓ Mejor para bases de datos que usan mucha RAM

Desventajas:
  ✗ Page cache se descarta más agresivamente
  ✗ Lecturas de archivos pueden ser más lentas (menos caché)
  ✗ En situaciones extremas, el OOM killer actúa antes
```

### Swappiness alto (60-100): priorizar caché

```
Ventajas:
  ✓ Más RAM disponible para page cache
  ✓ Lecturas de disco más rápidas (más datos cacheados)
  ✓ Procesos inactivos se mueven a swap sin problema

Desventajas:
  ✗ Procesos que vuelven de swap sufren latencia
  ✗ Swap storms posibles bajo carga pesada
  ✗ Peor experiencia interactiva (desktop)
```

### Swap storm

Un swap storm ocurre cuando el sistema swapea y des-swapea constantemente, gastando I/O en mover páginas en vez de trabajo útil:

```
Proceso A necesita RAM → swap out de B → B se activa →
swap in de B → swap out de A → A se activa →
swap in de A → swap out de B → ...

El sistema se vuelve extremadamente lento
(thrashing)
```

Swappiness bajo no previene thrashing — si la RAM es insuficiente, ocurre independientemente del swappiness. La solución real es más RAM o menos procesos.

---

## Cuándo ajustar swappiness

### Recomendaciones por caso de uso

| Escenario | Swappiness | Justificación |
|-----------|-----------|---------------|
| Servidor de base de datos | `10` | La DB gestiona su propia caché; no quiere swap |
| Servidor web/aplicaciones | `30-60` | Balance entre procesos y caché |
| Desktop/laptop | `10-30` | Respuesta interactiva, evitar lag |
| Servidor de archivos/NFS | `60-80` | Page cache es crítico para rendimiento |
| VM de lab | `60` (default) | No vale la pena ajustar |
| Sistemas con SSD | `10-60` | Swap en SSD es más rápido, menos urgente ajustar |
| Sistemas con HDD | `10-30` | Swap en HDD es lento, evitar si es posible |

### Recomendaciones de software específico

Algunos softwares documentan su preferencia:

```
PostgreSQL:        vm.swappiness = 1-10
Redis:             vm.swappiness = 1
Elasticsearch:     vm.swappiness = 1
Kubernetes nodes:  swap desactivado (swapoff -a)
Oracle Database:   vm.swappiness = 10
```

> **Kubernetes**: históricamente, Kubernetes requería swap desactivado. Desde v1.28+ soporta swap con limitaciones. En la práctica, la mayoría de clusters siguen sin swap.

### Cuándo NO ajustar

- Si el sistema funciona bien con el default (60)
- Si no entiendes el patrón de uso de memoria de tu carga de trabajo
- En VMs de lab donde el rendimiento no es crítico
- Cuando el problema real es RAM insuficiente (swappiness no lo resuelve)

---

## Otros parámetros relacionados

### vm.vfs_cache_pressure

Controla la tendencia a reclamar memoria usada para caché de dentries e inodos (metadatos de filesystem).

```bash
sysctl vm.vfs_cache_pressure
# vm.vfs_cache_pressure = 100    (default)

# < 100: mantener más caché de metadatos
# > 100: reclamar caché de metadatos más agresivamente
# = 0:   nunca reclamar (puede causar problemas de memoria)

# Para servidores con muchos archivos pequeños:
sysctl -w vm.vfs_cache_pressure=50
```

### vm.dirty_ratio y vm.dirty_background_ratio

Controlan cuántos datos "sucios" (escritos en RAM pero no en disco) se permiten antes de forzar la escritura.

```bash
# Porcentaje de RAM con datos sucios antes de que los procesos se bloqueen
sysctl vm.dirty_ratio
# vm.dirty_ratio = 20

# Porcentaje a partir del cual el kernel empieza a escribir en background
sysctl vm.dirty_background_ratio
# vm.dirty_background_ratio = 10
```

### vm.overcommit_memory

Controla si el kernel permite asignar más memoria virtual que la RAM + swap disponible.

```bash
sysctl vm.overcommit_memory
# 0 = heurístico (default) — permite algo de overcommit
# 1 = siempre permitir — nunca falla malloc
# 2 = no overcommit — malloc falla si no hay memoria real
```

> **Estos parámetros** son para referencia. En la práctica, `vm.swappiness` es el más comúnmente ajustado. Los demás se tocan en escenarios específicos.

---

## Errores comunes

### 1. Pensar que swappiness=0 desactiva swap

```bash
# ✗ "Con swappiness=0 nunca se usa swap"
sysctl -w vm.swappiness=0
# El kernel TODAVÍA puede swapear en situaciones extremas

# ✓ Para desactivar swap completamente:
swapoff -a
# Pero esto NO es recomendado en producción
# (OOM killer actúa inmediatamente sin swap como colchón)
```

### 2. Pensar que swappiness controla "a qué % de RAM se activa el swap"

```bash
# ✗ "swappiness=60 significa que se swapea cuando RAM llega al 60%"
# INCORRECTO — swappiness es una preferencia relativa, no un umbral

# ✓ Swappiness = peso entre descartar caché vs swapear
# El kernel puede swapear con cualquier valor de swappiness
# cuando la presión de memoria lo requiere
```

### 3. Ajustar swappiness en vez de añadir RAM

```bash
# ✗ Sistema constantemente swapeando → bajar swappiness
sysctl -w vm.swappiness=1
# Los procesos siguen sin RAM suficiente → thrashing continúa

# ✓ Si el sistema swapea constantemente, el problema es RAM insuficiente
# Soluciones reales:
#   - Añadir RAM
#   - Reducir procesos/carga
#   - Optimizar uso de memoria de las aplicaciones
```

### 4. Cambiar swappiness sin hacer persistente

```bash
# ✗ Cambiar con sysctl -w y olvidar
sysctl -w vm.swappiness=10
# Después del reinicio → vuelve a 60

# ✓ Siempre guardar en archivo
echo 'vm.swappiness=10' > /etc/sysctl.d/99-swappiness.conf
sysctl -p /etc/sysctl.d/99-swappiness.conf
```

### 5. Confundir swappiness con velocidad de swap

```bash
# ✗ "Swappiness alto = swap más rápido"
# INCORRECTO — swappiness no afecta la velocidad del disco

# ✓ Swappiness = cuánto prefiere el kernel swapear vs descartar caché
# La velocidad de swap depende del dispositivo (SSD vs HDD)
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│                   Swappiness — Referencia rápida                 │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  QUÉ ES:                                                         │
│    Preferencia entre descartar page cache vs swapear             │
│    NO es umbral de RAM. NO es velocidad de swap.                 │
│                                                                  │
│  CONSULTAR:                                                      │
│    sysctl vm.swappiness                                          │
│    cat /proc/sys/vm/swappiness                                   │
│                                                                  │
│  CAMBIAR (temporal):                                             │
│    sysctl -w vm.swappiness=10                                    │
│                                                                  │
│  CAMBIAR (persistente):                                          │
│    echo 'vm.swappiness=10' > /etc/sysctl.d/99-swappiness.conf   │
│    sysctl -p /etc/sysctl.d/99-swappiness.conf                   │
│                                                                  │
│  VALORES:                                                        │
│    0    = casi nunca swap (solo emergencia)                       │
│    10   = muy conservador (DB, Redis)                            │
│    30   = moderado (desktop, laptop)                             │
│    60   = default Linux                                          │
│    100  = swap y caché tratados igual                             │
│                                                                  │
│  RECOMENDACIONES:                                                │
│    Base de datos:        10                                      │
│    Servidor web:         30-60                                   │
│    Desktop:              10-30                                   │
│    Servidor de archivos: 60-80                                   │
│    Lab/default:          60                                      │
│                                                                  │
│  DIAGNÓSTICO:                                                    │
│    free -h                    uso de RAM y swap                   │
│    vmstat 1                   swap in/out por segundo             │
│    sar -W                     histórico de swap                  │
│    swapon --show              áreas de swap activas              │
│                                                                  │
│  REGLAS:                                                         │
│    swappiness=0 ≠ swap desactivado                               │
│    Swapping constante = necesitas más RAM                        │
│    No ajustar sin entender la carga de trabajo                   │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Consultar y modificar swappiness

En tu VM de lab:

1. Consulta el valor actual:
   ```bash
   sysctl vm.swappiness
   cat /proc/sys/vm/swappiness
   ```
2. Verifica el uso actual de swap: `free -h` y `swapon --show`
3. Cambia temporalmente a 10: `sysctl -w vm.swappiness=10`
4. Verifica: `sysctl vm.swappiness`
5. Hazlo persistente:
   ```bash
   echo 'vm.swappiness=10' > /etc/sysctl.d/99-swappiness.conf
   ```
6. Verifica que el archivo existe y tiene el contenido correcto
7. Simula un reinicio de sysctl: `sysctl -p /etc/sysctl.d/99-swappiness.conf`
8. Reinicia la VM y verifica que persiste: `sysctl vm.swappiness`
9. Restaura el default:
   ```bash
   rm /etc/sysctl.d/99-swappiness.conf
   sysctl -w vm.swappiness=60
   ```

> **Pregunta de reflexión**: si configuras `vm.swappiness=10` pero el sistema tiene 4 GiB de RAM y una base de datos usando 3.8 GiB, ¿el sistema evitará swap? ¿O swapeará de todos modos? ¿Por qué?

### Ejercicio 2: Observar el efecto de swappiness

1. Configura swappiness a 60 (default)
2. Verifica el estado inicial: `free -h` y `vmstat 1 5`
3. Genera carga de memoria:
   ```bash
   # Crear presión de memoria leyendo muchos archivos (llena page cache)
   find / -type f -name "*.conf" -exec cat {} \; > /dev/null 2>&1
   ```
4. Observa: `free -h` — ¿cuánto buff/cache creció?
5. Ahora genera presión de memoria con procesos:
   ```bash
   # Reservar memoria (ajusta el número según tu RAM)
   stress-ng --vm 2 --vm-bytes 80% --timeout 30s 2>/dev/null || \
   python3 -c "
   import time
   blocks = []
   for i in range(20):
       blocks.append('X' * (50 * 1024 * 1024))
       time.sleep(0.5)
   time.sleep(10)
   "
   ```
6. Mientras corre, en otra terminal: `watch -n1 'free -h; echo; swapon --show'`
7. ¿Se usó swap? ¿Se redujo buff/cache?
8. Repite con `vm.swappiness=10` — ¿hay diferencia en cuánto se descartó caché vs cuánto se swapeó?

> **Pregunta de reflexión**: ¿por qué es difícil observar el efecto de swappiness en una VM de lab con poca carga? ¿Qué condiciones necesitarías para ver una diferencia clara entre swappiness=10 y swappiness=90?

### Ejercicio 3: Configuración para un servicio específico

Escenario: vas a instalar PostgreSQL en un servidor con 8 GiB de RAM.

1. Investiga: ¿qué valor de `vm.swappiness` recomienda PostgreSQL?
2. Crea el archivo de configuración persistente con ese valor
3. Verifica que otros parámetros relacionados tienen valores razonables:
   ```bash
   sysctl vm.vfs_cache_pressure
   sysctl vm.dirty_ratio
   sysctl vm.dirty_background_ratio
   sysctl vm.overcommit_memory
   ```
4. Documenta cada valor y qué hace en un comentario del archivo sysctl:
   ```bash
   # /etc/sysctl.d/99-postgresql.conf
   # PostgreSQL: minimizar swap, la DB gestiona su propia caché
   vm.swappiness = 10
   ```
5. Aplica y verifica: `sysctl -p /etc/sysctl.d/99-postgresql.conf`

> **Pregunta de reflexión**: PostgreSQL usa `shared_buffers` para su propia caché en RAM. Si el kernel swapea esas páginas, el rendimiento de la DB cae drásticamente. ¿Cómo complementa `vm.swappiness=10` al ajuste de `shared_buffers` en PostgreSQL?
