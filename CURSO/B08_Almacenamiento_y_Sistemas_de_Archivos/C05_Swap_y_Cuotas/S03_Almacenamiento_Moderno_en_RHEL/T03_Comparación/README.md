# Comparación: LVM+Filesystem Manual vs Stratis vs Btrfs

## Índice

1. [Por qué comparar](#1-por-qué-comparar)
2. [Resumen de cada solución](#2-resumen-de-cada-solución)
3. [Comparación funcional](#3-comparación-funcional)
4. [Complejidad operativa](#4-complejidad-operativa)
5. [Disponibilidad por distribución](#5-disponibilidad-por-distribución)
6. [Rendimiento y overhead](#6-rendimiento-y-overhead)
7. [Madurez y producción](#7-madurez-y-producción)
8. [Árbol de decisión](#8-árbol-de-decisión)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Por qué comparar

Linux ofrece múltiples formas de gestionar almacenamiento avanzado. A lo largo de este
bloque has aprendido LVM, RAID con mdadm, y Stratis. Pero existe otra alternativa
importante que no hemos cubierto en profundidad: **Btrfs**, un filesystem que integra
muchas funcionalidades que tradicionalmente requieren capas separadas.

La pregunta no es cuál es "mejor" en abstracto, sino cuál se adapta a cada situación:

```
┌─────────────────────────────────────────────────────────────┐
│                 ¿Qué necesito gestionar?                     │
│                                                              │
│  Volúmenes + filesystem    →  LVM + XFS/ext4  (clásico)     │
│  Simplicidad pool-based    →  Stratis          (RHEL)        │
│  Todo integrado en el FS   →  Btrfs            (SUSE/Fedora) │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Resumen de cada solución

### LVM + Filesystem (XFS o ext4)

La combinación tradicional en Linux enterprise. Cada capa se gestiona por separado:

```
┌───────────────┐
│  XFS / ext4    │  ← mkfs, mount, xfs_growfs / resize2fs
├───────────────┤
│  LVM           │  ← pvcreate, vgcreate, lvcreate, lvextend
├───────────────┤
│  Disco / RAID  │  ← fdisk, mdadm
└───────────────┘

Herramientas: pvcreate, vgcreate, lvcreate, lvextend,
              mkfs.xfs, xfs_growfs, resize2fs, etc.
```

**Filosofía**: cada capa hace una cosa bien. Máxima flexibilidad, máxima complejidad.

### Stratis

Capa de gestión de Red Hat que unifica LVM + XFS bajo una interfaz simplificada:

```
┌──────────────────────────┐
│  stratis CLI              │  ← Una herramienta para todo
├──────────────────────────┤
│  stratisd                 │
│  (Device Mapper + XFS     │  ← Internamente usa LVM thin + XFS
│   thin provisioning)      │
├──────────────────────────┤
│  Disco                    │
└──────────────────────────┘

Herramientas: stratis pool create, stratis fs create
```

**Filosofía**: simplificar la interfaz sin cambiar la base probada (Device Mapper + XFS).

### Btrfs

Filesystem de nueva generación que integra funcionalidades de LVM dentro del propio
filesystem:

```
┌──────────────────────────┐
│  Btrfs                    │  ← Filesystem + gestión de volúmenes
│  (subvolúmenes, snapshots,│     + RAID + compresión + CoW
│   RAID, compresión, CoW)  │     todo en una sola capa
├──────────────────────────┤
│  Disco(s)                 │
└──────────────────────────┘

Herramientas: btrfs subvolume, btrfs filesystem,
              btrfs balance, btrfs scrub
```

**Filosofía**: integrar todo en el filesystem. Una sola capa que lo hace todo.

---

## 3. Comparación funcional

### Tabla de características

```
┌────────────────────────┬──────────────┬──────────────┬──────────────┐
│  Característica         │ LVM+FS       │ Stratis      │ Btrfs        │
├────────────────────────┼──────────────┼──────────────┼──────────────┤
│  Pools de discos        │ VG (manual)  │ Pool (auto)  │ Multi-device │
│  Thin provisioning      │ LVM thin     │ Automático   │ Nativo       │
│  Snapshots              │ LVM snap     │ Thin snap    │ Nativo (CoW) │
│  RAID                   │ mdadm / LVM  │ No (mdadm    │ Nativo       │
│                         │  RAID nativo │  debajo)     │ (RAID 0/1/10)│
│  Compresión             │ VDO          │ No           │ Nativo (zstd)│
│  Deduplicación          │ VDO          │ No           │ Offline only │
│  Checksums de datos     │ No           │ No           │ Sí (crc32c)  │
│  Scrub (verificación)   │ No           │ No           │ Sí           │
│  Send/receive (backup)  │ No           │ No           │ Sí           │
│  Shrink filesystem      │ ext4 sí,     │ No           │ Sí           │
│                         │ XFS no       │              │              │
│  Grow online            │ Sí           │ Automático   │ Sí           │
│  Elegir filesystem      │ Cualquiera   │ Solo XFS     │ ES el FS     │
│  Cuotas                 │ Del FS       │ Del FS (XFS) │ Nativas      │
│                         │ (usrquota)   │              │ (qgroups)    │
│  Cifrado                │ LUKS en      │ Integrado    │ No nativo    │
│                         │ cualquier    │ (stratis     │ (LUKS debajo)│
│                         │ capa         │  pool create │              │
│                         │              │  --key-desc) │              │
└────────────────────────┴──────────────┴──────────────┴──────────────┘
```

### Snapshots: diferencias clave

```
LVM snapshot clásico:
  - Reserva espacio fijo para el snapshot
  - Se invalida si se llena (100% = snapshot perdido)
  - Merge requiere desmontar el origen

LVM thin snapshot:
  - Sin espacio reservado (comparte thin pool)
  - No se invalida por tamaño
  - Merge sigue requiriendo desmontar

Stratis snapshot:
  - Thin snapshot automático
  - Es un filesystem independiente
  - No hay merge: destroy original + rename snapshot

Btrfs snapshot:
  - Instantáneo (solo metadata CoW)
  - Cero espacio inicial
  - Puede ser writable o readonly
  - btrfs send/receive para transferir snapshots entre hosts
```

### RAID: diferencias clave

```
mdadm + LVM:
  - RAID 0, 1, 5, 6, 10 completos
  - Maduro, probado en producción por décadas
  - Dos capas que gestionar

LVM RAID nativo:
  - Mismos niveles, integrado en LVM
  - Una sola herramienta (lvm)
  - Menos flexible que mdadm en configuración avanzada

Btrfs RAID:
  - RAID 0, 1, 10: estable
  - RAID 5/6: INESTABLE — no usar en producción
  - Perfiles de datos y metadata independientes
    (ej: datos en RAID 0, metadata en RAID 1)
```

> **Advertencia crítica**: Btrfs RAID 5/6 tiene un bug conocido llamado "write hole"
> que puede causar pérdida de datos. La wiki oficial de Btrfs lo marca como
> **inestable**. Si necesitas RAID 5/6, usa mdadm.

---

## 4. Complejidad operativa

### Crear un pool con 2 discos + filesystem + snapshot

**LVM + XFS** (10 comandos):

```bash
sudo pvcreate /dev/vdb /dev/vdc
sudo vgcreate datavg /dev/vdb /dev/vdc
sudo lvcreate -L 15G -n datalv datavg
sudo mkfs.xfs /dev/datavg/datalv
sudo mkdir -p /mnt/data
sudo mount /dev/datavg/datalv /mnt/data
# ... escribir datos ...
sudo lvcreate -s -L 2G -n datalv-snap datavg/datalv
sudo mkdir -p /mnt/snap
sudo mount -o ro,nouuid /dev/datavg/datalv-snap /mnt/snap
```

**Stratis** (6 comandos):

```bash
sudo stratis pool create datapool /dev/vdb /dev/vdc
sudo stratis filesystem create datapool mydata
sudo mkdir -p /mnt/data
sudo mount /dev/stratis/datapool/mydata /mnt/data
# ... escribir datos ...
sudo stratis filesystem snapshot datapool mydata mydata-snap
sudo mkdir -p /mnt/snap
sudo mount /dev/stratis/datapool/mydata-snap /mnt/snap
```

**Btrfs** (5 comandos):

```bash
sudo mkfs.btrfs -d raid0 -m raid1 /dev/vdb /dev/vdc
sudo mkdir -p /mnt/data
sudo mount /dev/vdb /mnt/data
sudo btrfs subvolume create /mnt/data/mydata
# ... escribir datos en /mnt/data/mydata/ ...
sudo btrfs subvolume snapshot -r /mnt/data/mydata /mnt/data/mydata-snap
```

### Redimensionar (añadir disco)

```
LVM:     pvcreate /dev/vdd → vgextend datavg /dev/vdd → lvextend → xfs_growfs
Stratis: stratis pool add-data datapool /dev/vdd          (automático)
Btrfs:   btrfs device add /dev/vdd /mnt/data → btrfs balance start /mnt/data
```

### Curva de aprendizaje

```
Complejidad
    ▲
    │
    │  ████████████████████  LVM + mdadm + FS
    │  ████████████████      Btrfs (muchos conceptos propios)
    │  ██████████            Stratis (pocos comandos)
    │
    └──────────────────────────────────────────►
                                         Flexibilidad

LVM: muchas herramientas, mucha flexibilidad, mucha documentación
Btrfs: una herramienta, muchos subcomandos, conceptos únicos (CoW, subvolumes, qgroups)
Stratis: pocos comandos, poca flexibilidad, fácil de aprender
```

---

## 5. Disponibilidad por distribución

```
┌─────────────────────┬──────────────┬──────────────┬──────────────┐
│  Distribución        │ LVM+FS       │ Stratis      │ Btrfs        │
├─────────────────────┼──────────────┼──────────────┼──────────────┤
│  RHEL 8              │ ✔ Default    │ ✔ Soportado  │ ✗ Eliminado  │
│  RHEL 9              │ ✔ Default    │ ✔ Soportado  │ ✗ Eliminado  │
│  CentOS Stream       │ ✔ Default    │ ✔ Soportado  │ ✗ Eliminado  │
│  Fedora              │ ✔ Default    │ ✔ Disponible │ ✔ Default *  │
│  Ubuntu              │ ✔ Default    │ ✗ No oficial │ ✔ Disponible │
│  Debian              │ ✔ Default    │ ✗ No oficial │ ✔ Disponible │
│  openSUSE/SLES       │ ✔ Default    │ ✗ No oficial │ ✔ Default    │
│  Arch Linux          │ ✔ Disponible │ AUR          │ ✔ Disponible │
└─────────────────────┴──────────────┴──────────────┴──────────────┘

* Fedora usa Btrfs como filesystem root por defecto desde Fedora 33
```

### Lo que esto significa en la práctica

- **LVM + XFS/ext4**: funciona en cualquier distribución Linux. Es la opción universal.
  Si estudias para certificaciones RHEL (RHCSA/RHCE), es obligatorio dominarlo.

- **Stratis**: solo en el ecosistema Red Hat. Si trabajas con RHEL/CentOS en producción,
  es una herramienta útil. No transferible a Debian/Ubuntu.

- **Btrfs**: amplio soporte fuera de RHEL. Es el filesystem por defecto en Fedora,
  openSUSE y SUSE Enterprise. Red Hat lo eliminó de RHEL 8 en adelante por considerar
  que XFS + LVM cubren sus necesidades.

> **Implicación para certificaciones RHEL**: el examen RHCSA puede incluir Stratis y
> VDO. No incluirá Btrfs porque RHEL no lo soporta. Sin embargo, el conocimiento de
> Btrfs es valioso fuera del ecosistema Red Hat.

---

## 6. Rendimiento y overhead

### Escritura secuencial

```
Rendimiento relativo (mayor = mejor):

ext4 sobre LVM:    ████████████████████  ~100% (baseline)
XFS sobre LVM:     ███████████████████   ~98%
Stratis (XFS):     ██████████████████    ~95% (thin provisioning overhead)
Btrfs (sin CoW):   ██████████████████    ~95%
Btrfs (con CoW):   ████████████████      ~85-90% (metadata CoW)
Btrfs (compress):  ██████████████        ~80% CPU-bound, menos I/O
VDO (dedup+comp):  ████████████          ~70-80% (dedup index + compress)
```

### Escritura aleatoria

```
ext4 sobre LVM:    ████████████████████  ~100% (baseline)
XFS sobre LVM:     ███████████████████   ~98%
Stratis:           █████████████████     ~90%
Btrfs:             ██████████████        ~75-85% (CoW fragmentación)
VDO:               ████████████          ~65-75%
```

### Notas sobre rendimiento

- **Btrfs y CoW**: cada escritura sobre un bloque existente crea una copia nueva. Esto
  causa fragmentación progresiva. Mitigación: `btrfs filesystem defragment` y
  `nodatacow` para archivos que se sobreescriben frecuentemente (VMs, bases de datos).

- **Stratis**: el overhead es mínimo porque Device Mapper thin provisioning es eficiente.
  XFS sobre thin es prácticamente igual que XFS sobre LV normal.

- **VDO**: el overhead es real y medible. Cada escritura pasa por hash + búsqueda en
  índice + compresión. Justificable solo cuando el ahorro de espacio compensa.

- Los números son orientativos. El rendimiento real depende del hardware, patrón de
  I/O, y configuración específica.

---

## 7. Madurez y producción

### Timeline de madurez

```
1998 ──── LVM en Linux (basado en HP-UX LVM)
2001 ──── LVM2 reescritura
2007 ──── Btrfs desarrollo inicia (Oracle)
2009 ──── Btrfs en mainline kernel (2.6.29)
2013 ──── SUSE adopta Btrfs como default
2014 ──── Red Hat elimina Btrfs de RHEL (tech preview nunca promovido)
2017 ──── Stratis proyecto iniciado (Red Hat)
2018 ──── Stratis en RHEL 8 (tech preview)
2020 ──── Fedora 33 adopta Btrfs como root default
2020 ──── Stratis estable en RHEL 8.3+
2021 ──── RHEL 9: VDO integrado en LVM
2024 ──── Btrfs RAID 5/6 sigue marcado como inestable
```

### Estado de producción

| Solución | Madurez | Confianza en producción |
|---|---|---|
| **LVM + ext4** | ~25 años | Máxima. Estándar de la industria |
| **LVM + XFS** | ~20 años | Máxima. Default en RHEL |
| **mdadm** | ~20 años | Máxima para RAID software |
| **Btrfs (datos)** | ~15 años | Alta en SUSE/Fedora. RAID 0/1/10 estable |
| **Btrfs (RAID 5/6)** | ~15 años | **Baja. No usar en producción** |
| **Stratis** | ~6 años | Media-Alta. Soportado en RHEL pero joven |
| **VDO** | ~7 años | Alta en su nicho (dedup/compresión) |

---

## 8. Árbol de decisión

```
¿Qué distribución usas?
│
├── RHEL / CentOS / Rocky / Alma
│   │
│   ├── ¿Necesitas máximo control y flexibilidad?
│   │   └── Sí → LVM + XFS + mdadm (si RAID)
│   │
│   ├── ¿Quieres simplicidad para pools/snapshots?
│   │   └── Sí → Stratis
│   │
│   └── ¿Necesitas deduplicación/compresión?
│       └── Sí → LVM + VDO (RHEL 9) o VDO clásico (RHEL 8)
│
├── Fedora
│   │
│   ├── ¿Root filesystem?
│   │   └── Btrfs (ya es el default)
│   │
│   └── ¿Datos con necesidades enterprise?
│       └── LVM + XFS o Stratis (ambos disponibles)
│
├── openSUSE / SLES
│   │
│   └── Btrfs (default y bien integrado con snapper)
│
├── Ubuntu / Debian
│   │
│   ├── ¿Necesitas snapshots integrados?
│   │   └── Btrfs (disponible, no default)
│   │
│   └── ¿Caso general?
│       └── LVM + ext4 (combinación más probada)
│
└── ¿Multi-distribución o no sabes?
    └── LVM + ext4/XFS → funciona en todas partes
```

### Reglas prácticas

1. **Si preparas RHCSA/RHCE**: domina LVM + XFS, Stratis, y VDO. Ignora Btrfs.
2. **Si administras Fedora/SUSE**: aprende Btrfs + snapper. LVM como complemento.
3. **Si necesitas portabilidad**: LVM + ext4/XFS. Funciona en cualquier Linux.
4. **Si necesitas RAID 5/6**: mdadm o LVM RAID. Nunca Btrfs RAID 5/6.
5. **Si necesitas deduplicación**: VDO en RHEL, o ZFS si la licencia lo permite.

---

## 9. Errores comunes

### Error 1: Elegir Btrfs RAID 5/6 para producción

```bash
# "Btrfs tiene RAID nativo, perfecto"
sudo mkfs.btrfs -d raid5 /dev/vdb /dev/vdc /dev/vdd
```

Btrfs RAID 5/6 tiene el "write hole" bug y está marcado como **inestable** en la wiki
oficial. Puede causar pérdida silenciosa de datos.

**Correcto**: usar mdadm para RAID 5/6 y Btrfs (o cualquier FS) encima, o usar Btrfs
solo con RAID 0/1/10.

### Error 2: Asumir que Stratis funciona en cualquier distro

```bash
# En Ubuntu
sudo apt install stratisd stratis-cli
# Error: paquete no encontrado
```

Stratis solo está empaquetado oficialmente en Fedora, RHEL y CentOS Stream. En
Debian/Ubuntu no hay soporte oficial.

**Correcto**: en Ubuntu/Debian, usar LVM + filesystem o Btrfs.

### Error 3: Usar Btrfs como LVM usaba bases de datos

```bash
# Base de datos PostgreSQL sobre Btrfs con CoW activado
# Resultado: fragmentación severa, rendimiento degradado
```

El Copy-on-Write de Btrfs fragmenta archivos que se sobrescriben parcialmente (como
archivos de base de datos).

**Correcto**: deshabilitar CoW para directorios de bases de datos:

```bash
chattr +C /var/lib/postgresql/data/
# O montar con nodatacow (afecta a todo el filesystem)
```

### Error 4: Comparar solo características sin considerar soporte

"Btrfs tiene más funcionalidades que LVM+XFS, así que es mejor."

Las funcionalidades sin soporte del vendor no sirven en producción. Si usas RHEL y
algo falla con Btrfs, Red Hat no te ayudará. El soporte del fabricante es un factor
operativo real.

### Error 5: Mezclar capas innecesariamente

```bash
# LVM sobre Btrfs (redundante — Btrfs ya tiene gestión de volúmenes)
sudo pvcreate /dev/vdb
sudo vgcreate btrfsvg /dev/vdb
sudo lvcreate -L 10G -n btrfslv btrfsvg
sudo mkfs.btrfs /dev/btrfsvg/btrfslv

# Btrfs sobre Stratis (redundante — Stratis ya usa XFS internamente)
sudo stratis filesystem create mypool data
sudo mkfs.btrfs /dev/stratis/mypool/data   # Sobreescribe el XFS de Stratis
```

**Correcto**: cada solución es una **alternativa** a las demás, no un complemento.
Elige una ruta y úsala consistentemente.

---

## 10. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║               COMPARACIÓN — REFERENCIA RÁPIDA                        ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                      ║
║  LVM + FILESYSTEM                                                    ║
║  Dónde:      Todas las distribuciones                                ║
║  Ideal para: Servidores enterprise, certificaciones RHEL             ║
║  RAID:       mdadm debajo o --type raid5/raid1 en LVM                ║
║  Snapshots:  lvcreate -s (clásico) o thin snapshots                  ║
║  Debilidad:  Muchos comandos, coordinación manual entre capas        ║
║                                                                      ║
║  STRATIS                                                             ║
║  Dónde:      RHEL, Fedora, CentOS Stream                            ║
║  Ideal para: Pools simples con thin provisioning y snapshots         ║
║  RAID:       No nativo (mdadm debajo)                                ║
║  Snapshots:  stratis fs snapshot (thin, sin tamaño fijo)             ║
║  Debilidad:  Solo XFS, no se pueden quitar discos, joven             ║
║                                                                      ║
║  BTRFS                                                               ║
║  Dónde:      Fedora (default), SUSE (default), Ubuntu, Debian        ║
║  Ideal para: Snapshots frecuentes, rollback, compresión nativa       ║
║  RAID:       Nativo 0/1/10 estable. 5/6 INESTABLE                    ║
║  Snapshots:  btrfs subvolume snapshot (instantáneo, CoW)             ║
║  Debilidad:  RAID 5/6 roto, fragmentación CoW, no en RHEL           ║
║                                                                      ║
║  REGLAS DE ORO                                                       ║
║  • Una solución por stack, no mezclar (LVM O Btrfs, no ambos)        ║
║  • RAID 5/6 → mdadm o LVM RAID, nunca Btrfs                         ║
║  • Dedup/compresión → VDO (RHEL) o Btrfs compress (no dedup)        ║
║  • Portabilidad → LVM + ext4/XFS                                    ║
║  • RHCSA/RHCE → LVM + XFS + Stratis + VDO                           ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: Comparar flujos de trabajo

Sin ejecutar comandos, escribe en papel (o en un editor) la secuencia completa de
comandos para lograr cada uno de estos escenarios con las tres soluciones. El objetivo
es internalizar las diferencias operativas:

**Escenario A**: Crear un pool con `/dev/vdb` y `/dev/vdc`, un filesystem de 10 GiB,
montarlo en `/mnt/data`, y crear un snapshot.

Completa la tabla:

```
┌─────────────────┬───────────────────────────────────────────┐
│  LVM + XFS       │  Tus comandos aquí                        │
├─────────────────┼───────────────────────────────────────────┤
│  Stratis         │  Tus comandos aquí                        │
├─────────────────┼───────────────────────────────────────────┤
│  Btrfs           │  Tus comandos aquí                        │
└─────────────────┴───────────────────────────────────────────┘
```

**Escenario B**: Añadir un tercer disco `/dev/vdd` al pool/volumen existente.

**Escenario C**: Revertir al snapshot creado en el escenario A.

**Pregunta de reflexión**: ¿En cuál de las tres soluciones el flujo de revert es más
sencillo? ¿En cuál es más peligroso (mayor riesgo de perder datos si te equivocas)?

### Ejercicio 2: Elegir la solución correcta

Para cada situación, justifica qué solución elegirías y por qué:

1. **Servidor de base de datos PostgreSQL en RHEL 9** con 4 discos SAS, necesitas
   redundancia RAID 5 y la capacidad de hacer snapshots antes de migraciones de esquema.

2. **Estación de trabajo Fedora** para desarrollo, un solo NVMe de 1 TiB, quieres
   poder hacer rollback del sistema operativo tras actualizaciones fallidas.

3. **Servidor de backups en RHEL 8** que recibe copias diarias de 50 máquinas virtuales
   con el mismo sistema operativo base. El almacenamiento físico es de 10 TiB pero
   necesitas almacenar ~40 TiB lógicos.

4. **Cluster de contenedores (Kubernetes) en Ubuntu** con almacenamiento compartido.
   Los nodos necesitan volúmenes persistentes con snapshots.

5. **NAS doméstico con openSUSE** para fotos y documentos familiares, 4 discos de
   4 TiB, prioridad máxima: no perder datos.

**Pregunta de reflexión**: ¿Existe algún escenario donde usarías Stratis en Ubuntu?
¿Por qué los ecosistemas divergen en lugar de converger hacia una solución única?

### Ejercicio 3: Investigación práctica

1. En tu sistema Fedora, verifica qué filesystem tiene la partición root:
   ```bash
   findmnt / -o FSTYPE
   ```
2. Si es Btrfs (probable), lista los subvolúmenes existentes:
   ```bash
   sudo btrfs subvolume list /
   ```
3. Investiga si tienes snapshots automáticos (Fedora no usa snapper por defecto, pero
   verifica):
   ```bash
   ls /root/.snapshots/ 2>/dev/null || echo "No hay directorio de snapshots"
   ```
4. Revisa la compresión activa:
   ```bash
   findmnt / -o OPTIONS | grep -o 'compress[^ ,]*'
   ```
5. Compara el espacio reportado por `df` con el uso real de Btrfs:
   ```bash
   df -h /
   sudo btrfs filesystem usage /
   ```

**Pregunta de reflexión**: el comando `df` sobre Btrfs a menudo muestra valores
confusos (espacio usado/libre que no cuadra). ¿Por qué ocurre esto? Pista: piensa
en cómo CoW, snapshots compartidos, y metadata afectan al cálculo de espacio "libre".
¿Te recuerda a algún problema similar que vimos con otra tecnología en este bloque?
