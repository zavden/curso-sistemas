# Niveles RAID

## Índice

1. [Qué es RAID](#qué-es-raid)
2. [Conceptos fundamentales](#conceptos-fundamentales)
3. [RAID 0 — Striping](#raid-0--striping)
4. [RAID 1 — Mirroring](#raid-1--mirroring)
5. [RAID 5 — Striping con paridad distribuida](#raid-5--striping-con-paridad-distribuida)
6. [RAID 6 — Doble paridad](#raid-6--doble-paridad)
7. [RAID 10 — Mirror + Stripe](#raid-10--mirror--stripe)
8. [Tabla comparativa](#tabla-comparativa)
9. [Cálculos de capacidad](#cálculos-de-capacidad)
10. [Hot spare](#hot-spare)
11. [RAID por hardware vs software](#raid-por-hardware-vs-software)
12. [Elegir el nivel correcto](#elegir-el-nivel-correcto)
13. [Errores comunes](#errores-comunes)
14. [Cheatsheet](#cheatsheet)
15. [Ejercicios](#ejercicios)

---

## Qué es RAID

RAID (Redundant Array of Independent Disks) combina múltiples discos en un solo dispositivo lógico para obtener **redundancia**, **rendimiento**, o ambos.

```
Sin RAID:                          Con RAID:
┌───────┐ ┌───────┐ ┌───────┐     ┌─────────────────────────┐
│ Disco │ │ Disco │ │ Disco │     │       /dev/md0          │
│   1   │ │   2   │ │   3   │     │   (un solo dispositivo) │
│       │ │       │ │       │     │   3 discos combinados   │
└───────┘ └───────┘ └───────┘     └─────────────────────────┘
3 dispositivos independientes      1 dispositivo con redundancia
Si uno falla → datos perdidos      Si uno falla → datos intactos*

* Depende del nivel RAID
```

RAID **no es backup**. Protege contra fallos de disco, no contra borrado accidental, corrupción de software, ni ransomware.

---

## Conceptos fundamentales

### Striping (distribución)

Dividir los datos entre múltiples discos. Cada disco recibe una porción (stripe) de los datos. Aumenta el rendimiento porque se leen/escriben múltiples discos en paralelo.

```
Datos: [A][B][C][D][E][F][G][H]

Striping en 2 discos:
  Disco 1: [A][C][E][G]
  Disco 2: [B][D][F][H]

Lectura de A+B: ambos discos en paralelo → 2× velocidad
```

### Mirroring (espejo)

Escribir los mismos datos en dos o más discos. Si uno falla, el otro tiene una copia idéntica.

```
Datos: [A][B][C][D]

Mirroring en 2 discos:
  Disco 1: [A][B][C][D]   ← copia completa
  Disco 2: [A][B][C][D]   ← copia idéntica

Disco 1 falla → Disco 2 sigue funcionando con todos los datos
```

### Paridad

Información calculada a partir de los datos que permite **reconstruir** un bloque perdido. Se distribuye entre los discos.

```
Datos: A=0101, B=1100

Paridad (XOR): P = A ⊕ B = 1001

Si A se pierde:  A = P ⊕ B = 1001 ⊕ 1100 = 0101  ✓ recuperado
Si B se pierde:  B = P ⊕ A = 1001 ⊕ 0101 = 1100  ✓ recuperado
```

### Rebuild (reconstrucción)

Cuando un disco falla y se reemplaza, el array reconstruye los datos en el disco nuevo usando los datos restantes (y paridad si aplica). Este proceso puede durar horas y durante ese tiempo el array está en estado **degradado**.

---

## RAID 0 — Striping

Los datos se distribuyen entre todos los discos sin redundancia.

```
┌─────────────────────────────────────────┐
│                 RAID 0                   │
│                                         │
│   Disco 1     Disco 2     Disco 3       │
│  ┌───────┐   ┌───────┐   ┌───────┐     │
│  │ A1    │   │ A2    │   │ A3    │     │
│  │ B1    │   │ B2    │   │ B3    │     │
│  │ C1    │   │ C2    │   │ C3    │     │
│  │ D1    │   │ D2    │   │ D3    │     │
│  └───────┘   └───────┘   └───────┘     │
│                                         │
│  Cada fila (A1,A2,A3) = un stripe       │
│  Los datos se reparten entre discos     │
└─────────────────────────────────────────┘
```

| Propiedad | Valor |
|-----------|-------|
| Mínimo de discos | 2 |
| Redundancia | **Ninguna** |
| Tolerancia a fallos | 0 discos — cualquier fallo = pérdida total |
| Capacidad útil | N × tamaño del disco más pequeño |
| Rendimiento lectura | N× (paralelo) |
| Rendimiento escritura | N× (paralelo) |

**Uso**: cuando necesitas máximo rendimiento y los datos son prescindibles (caché, scratch, datos regenerables). **Nunca para datos importantes**.

---

## RAID 1 — Mirroring

Cada disco contiene una copia idéntica de los datos.

```
┌─────────────────────────────────────────┐
│                 RAID 1                   │
│                                         │
│      Disco 1         Disco 2            │
│    ┌─────────┐     ┌─────────┐          │
│    │ A       │     │ A       │  ← copia │
│    │ B       │     │ B       │  ← copia │
│    │ C       │     │ C       │  ← copia │
│    │ D       │     │ D       │  ← copia │
│    └─────────┘     └─────────┘          │
│                                         │
│  Ambos discos tienen los mismos datos   │
│  Si uno falla, el otro sigue            │
└─────────────────────────────────────────┘
```

| Propiedad | Valor |
|-----------|-------|
| Mínimo de discos | 2 |
| Redundancia | Copia completa |
| Tolerancia a fallos | N-1 discos (normalmente 1 de 2) |
| Capacidad útil | Tamaño de 1 disco (50% del total) |
| Rendimiento lectura | Hasta 2× (leer de ambos) |
| Rendimiento escritura | 1× (escribir en ambos) |

**Uso**: datos críticos donde la simplicidad y la fiabilidad importan más que la capacidad. Boot, rootfs, bases de datos pequeñas.

---

## RAID 5 — Striping con paridad distribuida

Los datos y la paridad se distribuyen entre todos los discos. Tolera el fallo de **un** disco.

```
┌─────────────────────────────────────────────────────┐
│                      RAID 5                          │
│                                                     │
│   Disco 1     Disco 2     Disco 3     Disco 4       │
│  ┌───────┐   ┌───────┐   ┌───────┐   ┌───────┐     │
│  │ A1    │   │ A2    │   │ A3    │   │ Ap    │     │
│  │ B1    │   │ B2    │   │ Bp    │   │ B3    │     │
│  │ C1    │   │ Cp    │   │ C2    │   │ C3    │     │
│  │ Dp    │   │ D1    │   │ D2    │   │ D3    │     │
│  └───────┘   └───────┘   └───────┘   └───────┘     │
│                                                     │
│  p = paridad (rota entre discos)                     │
│  Si Disco 3 falla: A3 = A1⊕A2⊕Ap                   │
│                     C2 y D2 se reconstruyen igual    │
│                     Bp se recalcula                  │
└─────────────────────────────────────────────────────┘
```

| Propiedad | Valor |
|-----------|-------|
| Mínimo de discos | 3 |
| Redundancia | Paridad distribuida |
| Tolerancia a fallos | 1 disco |
| Capacidad útil | (N-1) × tamaño del disco |
| Rendimiento lectura | (N-1)× (bueno) |
| Rendimiento escritura | Menor que RAID 0 (cálculo de paridad) |

**Uso**: equilibrio entre capacidad, rendimiento y redundancia. Servidores de archivos, almacenamiento general.

### El problema de RAID 5 con discos grandes

Con discos modernos (4+ TiB), el rebuild tras un fallo puede durar **días**. Durante ese tiempo, si otro disco falla → pérdida total. Esto ha hecho que RAID 5 sea cada vez menos recomendado para discos grandes. RAID 6 o RAID 10 son preferibles.

---

## RAID 6 — Doble paridad

Como RAID 5, pero con **dos bloques de paridad** por stripe. Tolera el fallo de **dos** discos simultáneamente.

```
┌─────────────────────────────────────────────────────┐
│                      RAID 6                          │
│                                                     │
│   Disco 1     Disco 2     Disco 3     Disco 4       │
│  ┌───────┐   ┌───────┐   ┌───────┐   ┌───────┐     │
│  │ A1    │   │ A2    │   │ Ap    │   │ Aq    │     │
│  │ B1    │   │ Bp    │   │ Bq    │   │ B2    │     │
│  │ Cp    │   │ Cq    │   │ C1    │   │ C2    │     │
│  │ Dq    │   │ D1    │   │ D2    │   │ Dp    │     │
│  └───────┘   └───────┘   └───────┘   └───────┘     │
│                                                     │
│  p = paridad P    q = paridad Q (algoritmo distinto) │
│  Dos discos pueden fallar sin perder datos           │
└─────────────────────────────────────────────────────┘
```

| Propiedad | Valor |
|-----------|-------|
| Mínimo de discos | 4 |
| Redundancia | Doble paridad |
| Tolerancia a fallos | 2 discos |
| Capacidad útil | (N-2) × tamaño del disco |
| Rendimiento lectura | (N-2)× (bueno) |
| Rendimiento escritura | Menor que RAID 5 (doble cálculo de paridad) |

**Uso**: almacenamiento con discos grandes donde RAID 5 no ofrece suficiente protección durante el rebuild. NAS empresarial, almacenamiento a largo plazo.

---

## RAID 10 — Mirror + Stripe

Combinación de RAID 1 (mirror) y RAID 0 (stripe). Los discos se agrupan en parejas de mirrors, y los datos se distribuyen (stripe) entre las parejas.

```
┌───────────────────────────────────────────────────────┐
│                      RAID 10                           │
│                                                       │
│       Mirror 1            Mirror 2                    │
│   ┌─────┐ ┌─────┐    ┌─────┐ ┌─────┐                 │
│   │  D1 │ │  D2 │    │  D3 │ │  D4 │                 │
│   ├─────┤ ├─────┤    ├─────┤ ├─────┤                 │
│   │ A1  │ │ A1  │    │ A2  │ │ A2  │                 │
│   │ B1  │ │ B1  │    │ B2  │ │ B2  │                 │
│   │ C1  │ │ C1  │    │ C2  │ │ C2  │                 │
│   └─────┘ └─────┘    └─────┘ └─────┘                 │
│   │← mirror ─→│      │← mirror ─→│                   │
│   │                                │                  │
│   └──── stripe entre mirrors ──────┘                  │
│                                                       │
│   Puede fallar 1 disco por mirror sin perder datos    │
│   D1+D3 fallan → OK (D2 y D4 cubren)                 │
│   D1+D2 fallan → pérdida (mirror 1 perdido)          │
└───────────────────────────────────────────────────────┘
```

| Propiedad | Valor |
|-----------|-------|
| Mínimo de discos | 4 (siempre par) |
| Redundancia | Mirror por pareja |
| Tolerancia a fallos | 1 disco por mirror (hasta N/2 si son de mirrors distintos) |
| Capacidad útil | N/2 × tamaño del disco (50%) |
| Rendimiento lectura | N× (excelente) |
| Rendimiento escritura | N/2× (bueno) |

**Uso**: máximo rendimiento con redundancia. Bases de datos de producción, aplicaciones con I/O intensivo. Es el nivel más recomendado para cargas de trabajo exigentes.

### ¿RAID 10 o RAID 01?

```
RAID 10 (mirror, luego stripe) — PREFERIDO:
  Stripe[ Mirror(D1,D2), Mirror(D3,D4) ]
  Si D1 falla → Mirror 1 sigue con D2 → array operativo

RAID 01 (stripe, luego mirror) — EVITAR:
  Mirror[ Stripe(D1,D3), Stripe(D2,D4) ]
  Si D1 falla → Stripe 1 pierde un disco → stripe entero muerto
  Si luego D4 falla → ambos stripes muertos → pérdida total

RAID 10 tolera más patrones de fallo que RAID 01
```

---

## Tabla comparativa

```
┌────────┬──────┬───────────┬──────────────┬──────────────┬──────────┐
│ Nivel  │ Min  │ Tolerancia│ Capacidad    │ Lectura      │ Escritura│
│        │discos│ a fallos  │ útil         │              │          │
├────────┼──────┼───────────┼──────────────┼──────────────┼──────────┤
│ RAID 0 │  2   │ 0         │ N × disco    │ N×           │ N×       │
│ RAID 1 │  2   │ N-1       │ 1 × disco    │ ~2×          │ 1×       │
│ RAID 5 │  3   │ 1         │ (N-1)× disco │ (N-1)×       │ moderada │
│ RAID 6 │  4   │ 2         │ (N-2)× disco │ (N-2)×       │ baja     │
│ RAID 10│  4   │ 1/mirror  │ N/2 × disco  │ N×           │ N/2×     │
└────────┴──────┴───────────┴──────────────┴──────────────┴──────────┘
```

---

## Cálculos de capacidad

### Fórmulas

| Nivel | Capacidad útil | Ejemplo (4 discos × 1 TiB) |
|-------|---------------|---------------------------|
| RAID 0 | N × D | 4 × 1 = **4 TiB** |
| RAID 1 | D | **1 TiB** (mirror de 2 discos, o 2 parejas) |
| RAID 5 | (N-1) × D | 3 × 1 = **3 TiB** |
| RAID 6 | (N-2) × D | 2 × 1 = **2 TiB** |
| RAID 10 | (N/2) × D | 2 × 1 = **2 TiB** |

Donde N = número de discos, D = tamaño del disco más pequeño.

### Eficiencia de espacio

```
Con 6 discos de 1 TiB (6 TiB brutos):

  RAID 0:   6 TiB útiles  (100%)  — sin redundancia
  RAID 1:   1 TiB útil    (17%)   — 2 discos, o 3 mirrors de 2
  RAID 5:   5 TiB útiles  (83%)   — pierde 1 disco a paridad
  RAID 6:   4 TiB útiles  (67%)   — pierde 2 discos a paridad
  RAID 10:  3 TiB útiles  (50%)   — la mitad a mirrors
```

### Discos de distinto tamaño

RAID usa el **tamaño del disco más pequeño** como referencia. Si mezclas discos de 1 TiB y 2 TiB, el espacio extra de los discos de 2 TiB se desperdicia.

```
RAID 5 con 3 discos: 500 GiB, 1 TiB, 1 TiB

  Capacidad = (N-1) × disco_menor = (3-1) × 500 GiB = 1 TiB
  Desperdicio = 2 × 500 GiB = 1 TiB sin usar

  Recomendación: usar discos del mismo tamaño
```

---

## Hot spare

Un **hot spare** es un disco de repuesto conectado al array pero sin uso activo. Cuando un disco falla, el hot spare se activa automáticamente y el rebuild comienza sin intervención.

```
Estado normal:
  [Disco 1] [Disco 2] [Disco 3] [Spare]
   activo    activo    activo    standby

Disco 2 falla:
  [Disco 1] [  ✗  ]  [Disco 3] [Spare]
   activo    FALLO    activo    standby

Rebuild automático:
  [Disco 1] [  ✗  ]  [Disco 3] [Spare]
   activo    FALLO    activo   →rebuilding→

Rebuild completo:
  [Disco 1] [  ✗  ]  [Disco 3] [Spare]
   activo    retirar  activo    activo
```

```bash
# Crear RAID 5 con hot spare
mdadm --create /dev/md0 --level=5 --raid-devices=3 \
    /dev/vdb /dev/vdc /dev/vdd --spare-devices=1 /dev/vde
```

El hot spare reduce el tiempo que el array pasa en estado degradado, disminuyendo el riesgo de un segundo fallo.

---

## RAID por hardware vs software

### RAID por hardware

Un controlador dedicado (tarjeta PCIe o integrado en la placa) gestiona el RAID. El sistema operativo ve un solo disco.

```
  Discos ──► Controladora RAID (hardware) ──► /dev/sda (un solo disco)
                 │
                 └── CPU dedicada, caché con batería, firmware propio
```

### RAID por software (md)

El kernel Linux gestiona el RAID. Los discos individuales son visibles y el array se crea con `mdadm`.

```
  Discos ──► Kernel Linux (md driver) ──► /dev/md0
              │
              └── Usa CPU del sistema, sin hardware extra
```

### Comparación

```
┌──────────────────────┬──────────────────┬──────────────────┐
│ Aspecto              │ Hardware         │ Software (md)    │
├──────────────────────┼──────────────────┼──────────────────┤
│ Coste                │ Alto             │ Gratis           │
│ Rendimiento          │ Caché con BBU    │ CPU del sistema  │
│ Portabilidad         │ Atado al modelo  │ Cualquier Linux  │
│ Visibilidad          │ Opaco al OS      │ Transparente     │
│ Monitorización       │ Herramientas     │ mdadm, mdstat    │
│                      │ propietarias     │                  │
│ Recuperación         │ Mismo modelo de  │ Cualquier Linux  │
│                      │ controladora     │                  │
│ Virtualización       │ No disponible    │ Ideal para VMs   │
│ Labs                 │ No aplica        │ Lo que usaremos  │
└──────────────────────┴──────────────────┴──────────────────┘
```

> **En este curso**: usaremos RAID por software con `mdadm`. En VMs no hay controladora hardware, y md es el estándar de facto en Linux.

### Fake RAID (evitar)

Algunas placas baratas ofrecen "RAID" que en realidad es un driver propietario sobre discos normales. No es hardware real ni software estándar. Linux puede usarlos con `dmraid`, pero es preferible ignorarlos y usar `md` directamente.

---

## Elegir el nivel correcto

```
¿Necesitas redundancia?
    │
    No → RAID 0 (o sin RAID)
    │
    Sí
    │
¿Cuántos discos tienes?
    │
    2 → RAID 1
    │
    3 → RAID 5 (si discos < 2 TiB)
    │     RAID 1 + spare (si discos grandes)
    │
    4+ ──► ¿Prioridad?
           │
           ├── Capacidad → RAID 5 (discos < 2 TiB)
           │                RAID 6 (discos ≥ 2 TiB)
           │
           ├── Rendimiento → RAID 10
           │
           └── Máxima seguridad → RAID 6
                                   RAID 10 + spare
```

### Resumen de recomendaciones

| Escenario | Nivel | Por qué |
|-----------|-------|---------|
| Boot/rootfs de servidor | RAID 1 | Simple, fiable, 2 discos |
| Servidor de archivos | RAID 5 o 6 | Buen balance capacidad/seguridad |
| Base de datos | RAID 10 | Máximo rendimiento I/O |
| Almacenamiento masivo | RAID 6 | Protección durante rebuild largo |
| Lab temporal | RAID 0 o sin RAID | No importa si se pierde |
| NAS doméstico | RAID 5 (pocos discos) | Capacidad razonable |

---

## Errores comunes

### 1. Pensar que RAID reemplaza backups

```
✗ "Tengo RAID 1, no necesito backup"
   → RAID protege contra fallo de disco
   → NO protege contra: borrado accidental, ransomware,
     corrupción de software, error humano, incendio

✓ RAID + backups = protección real
   RAID = disponibilidad (el servicio no se cae)
   Backup = recuperabilidad (los datos se pueden restaurar)
```

### 2. Usar RAID 5 con discos grandes modernos

```
✗ RAID 5 con 4 discos de 8 TiB
   Rebuild: 24+ horas
   Probabilidad de segundo fallo durante rebuild: significativa
   → Pérdida total

✓ RAID 6 o RAID 10 con discos grandes
   RAID 6: tolera 2 fallos durante rebuild
   RAID 10: rebuild más rápido (solo copia mirror)
```

### 3. Mezclar discos de distinto tamaño

```
✗ RAID 5: 1 TiB + 1 TiB + 2 TiB
   Capacidad: (3-1) × 1 TiB = 2 TiB
   Se desperdicia 1 TiB del disco grande

✓ Usar discos del mismo modelo y tamaño
   Mismo firmware, mismo rendimiento, misma vida útil
```

### 4. Confundir RAID 10 con RAID 01

```
✗ RAID 01: stripe primero, mirror después
   Menor tolerancia a fallos

✓ RAID 10: mirror primero, stripe después
   mdadm --level=10 crea RAID 10 (el correcto)
```

### 5. No tener hot spare

```
✗ RAID 5 con 3 discos y sin spare
   Fallo de disco → array degradado
   Hay que comprar disco, enviarlo, instalarlo, rebuild...
   Ventana de vulnerabilidad: días

✓ RAID 5 con 3 discos + 1 spare
   Fallo de disco → rebuild automático inmediato
   Ventana de vulnerabilidad: horas (duración del rebuild)
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│                  Niveles RAID — Referencia rápida                │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  NIVELES:                                                        │
│    RAID 0:  Stripe      N discos, 0 redundancia, N× capacidad   │
│    RAID 1:  Mirror      2 discos, N-1 tolerancia, 1× capacidad  │
│    RAID 5:  Paridad     3+ discos, 1 tolerancia, (N-1)× cap.    │
│    RAID 6:  Doble par.  4+ discos, 2 tolerancia, (N-2)× cap.    │
│    RAID 10: Mirror+Str  4+ discos, 1/mirror, N/2 × cap.         │
│                                                                  │
│  CAPACIDAD (N discos × D tamaño):                                │
│    RAID 0:  N × D                                                │
│    RAID 1:  1 × D                                                │
│    RAID 5:  (N-1) × D                                            │
│    RAID 6:  (N-2) × D                                            │
│    RAID 10: (N/2) × D                                            │
│                                                                  │
│  ELEGIR:                                                         │
│    2 discos          → RAID 1                                    │
│    3+ discos, cap.   → RAID 5 (<2TiB) o RAID 6 (≥2TiB)          │
│    4+ discos, I/O    → RAID 10                                   │
│    Sin redundancia   → RAID 0                                    │
│                                                                  │
│  CONCEPTOS:                                                      │
│    Stripe    = distribuir datos entre discos                     │
│    Mirror    = copiar datos en discos duplicados                 │
│    Paridad   = XOR para reconstruir disco perdido                │
│    Hot spare = disco de repuesto automático                      │
│    Degradado = array funcionando con disco(s) faltante(s)        │
│    Rebuild   = reconstrucción en disco nuevo                     │
│                                                                  │
│  REGLAS:                                                         │
│    RAID ≠ backup                                                 │
│    Discos del mismo tamaño y modelo                              │
│    RAID 5 no recomendado para discos > 2 TiB                    │
│    RAID 10 > RAID 01                                             │
│    Hot spare reduce ventana de vulnerabilidad                    │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Cálculos de capacidad

Sin ejecutar comandos, calcula la capacidad útil para cada escenario:

1. RAID 0 con 3 discos de 500 GiB
2. RAID 1 con 2 discos de 1 TiB
3. RAID 5 con 4 discos de 2 TiB
4. RAID 6 con 6 discos de 1 TiB
5. RAID 10 con 6 discos de 500 GiB
6. RAID 5 con 3 discos: 500 GiB, 1 TiB, 1 TiB — ¿cuánto se desperdicia?

Verifica tus respuestas con las fórmulas de la tabla.

> **Pregunta de reflexión**: un cliente tiene 4 discos de 4 TiB y necesita almacenar 8 TiB de datos con tolerancia a un fallo. ¿RAID 5 o RAID 10? Ambos cumplen el requisito de capacidad — ¿cuál elegirías y por qué?

### Ejercicio 2: Identificar el nivel por el diagrama

Para cada diagrama, identifica el nivel RAID:

**Diagrama A:**
```
D1: [A][B][C][D]
D2: [A][B][C][D]
```

**Diagrama B:**
```
D1: [A1][B2][Cp]
D2: [Ap][B1][C2]
D3: [A2][Bp][C1]
```

**Diagrama C:**
```
D1: [A1][B1]  D2: [A1][B1]
D3: [A2][B2]  D4: [A2][B2]
```

**Diagrama D:**
```
D1: [A][C][E]
D2: [B][D][F]
```

> **Pregunta de reflexión**: en el diagrama B (RAID 5), si D2 falla, ¿cómo se reconstruyen los bloques Ap, B1 y C2? Describe el cálculo de XOR para cada uno.

### Ejercicio 3: Escenarios de fallo

Para cada situación, indica si hay pérdida de datos:

1. RAID 1 (2 discos): falla disco 1
2. RAID 1 (2 discos): fallan disco 1 y disco 2
3. RAID 5 (4 discos): falla disco 3
4. RAID 5 (4 discos): fallan disco 2 y disco 4
5. RAID 6 (5 discos): fallan disco 1 y disco 3
6. RAID 6 (5 discos): fallan disco 1, disco 3 y disco 5
7. RAID 10 (4 discos, mirrors: D1+D2, D3+D4): falla D1
8. RAID 10 (4 discos, mirrors: D1+D2, D3+D4): fallan D1 y D3
9. RAID 10 (4 discos, mirrors: D1+D2, D3+D4): fallan D1 y D2

> **Pregunta de reflexión**: en el caso 8 (RAID 10, fallan D1 y D3 de mirrors distintos), el array sobrevive. Pero en el caso 9 (fallan D1 y D2 del mismo mirror), no. ¿Esto significa que RAID 10 "a veces" tolera 2 fallos? ¿Cómo describirías correctamente su tolerancia a fallos?
