# Ciclo de vida de una VM

## Índice

1. [Estados de una VM](#estados-de-una-vm)
2. [Listar VMs: virsh list](#listar-vms-virsh-list)
3. [Iniciar: virsh start](#iniciar-virsh-start)
4. [Apagar gracefully: virsh shutdown](#apagar-gracefully-virsh-shutdown)
5. [Forzar apagado: virsh destroy](#forzar-apagado-virsh-destroy)
6. [Reiniciar: virsh reboot](#reiniciar-virsh-reboot)
7. [Pausar y reanudar: suspend / resume](#pausar-y-reanudar-suspend--resume)
8. [Guardar y restaurar: save / restore](#guardar-y-restaurar-save--restore)
9. [Eliminar: virsh undefine](#eliminar-virsh-undefine)
10. [Diagrama completo de transiciones](#diagrama-completo-de-transiciones)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## Estados de una VM

Una VM gestionada por libvirt puede estar en uno de estos estados:

```
┌──────────────────────────────────────────────────────────────────┐
│                     ESTADOS DE UNA VM                           │
├──────────────┬───────────────────────────────────────────────────┤
│  Estado      │  Significado                                     │
├──────────────┼───────────────────────────────────────────────────┤
│  undefined   │  No existe en libvirt (no hay XML registrado)     │
│  defined     │  XML registrado, VM no arrancada (shut off)       │
│  running     │  VM en ejecución, proceso QEMU activo             │
│  paused      │  vCPUs detenidas, RAM sigue reservada             │
│  saved       │  Estado volcado a disco, proceso QEMU terminado   │
│  crashed     │  El proceso QEMU terminó inesperadamente          │
│  pmsuspended │  Suspensión vía ACPI (sleep/hibernate del guest)  │
└──────────────┴───────────────────────────────────────────────────┘
```

Los estados más frecuentes en el día a día son `running`, `shut off`
(defined) y `paused`. Los otros son situaciones especiales.

---

## Listar VMs: virsh list

### Solo VMs activas (running)

```bash
virsh list
```

```
 Id   Name         State
-----------------------------
 1    debian-lab   running
 3    alma-lab     running
```

### Todas las VMs (incluidas apagadas)

```bash
virsh list --all
```

```
 Id   Name          State
-------------------------------
 1    debian-lab    running
 3    alma-lab      running
 -    lab-lvm       shut off
 -    lab-raid      shut off
 -    template-deb  shut off
```

Las VMs apagadas no tienen `Id` (se asigna al arrancar).

### Filtrar por estado

```bash
# Solo las apagadas
virsh list --all --state-shutoff

# Solo las pausadas
virsh list --all --state-paused
```

### Información detallada de una VM

```bash
virsh dominfo debian-lab
```

```
Id:             1
Name:           debian-lab
UUID:           a1b2c3d4-e5f6-7890-abcd-ef1234567890
OS Type:        hvm
State:          running
CPU(s):         2
CPU time:       45.3s
Max memory:     2097152 KiB
Used memory:    2097152 KiB
Persistent:     yes
Autostart:      disable
Managed save:   no
Security model: selinux
Security DOI:   0
Security label: system_u:system_r:svirt_t:s0:c100,c200 (enforcing)
```

Campos clave:

| Campo | Significado |
|-------|-------------|
| `Id` | PID-like asignado por libvirt (solo mientras corre) |
| `State` | Estado actual |
| `CPU(s)` | Número de vCPUs |
| `CPU time` | Tiempo total de CPU consumido |
| `Max memory` | RAM máxima asignada |
| `Persistent` | `yes` = definida en XML, sobrevive reinicios del host |
| `Autostart` | `enable` = arranca automáticamente con libvirtd |

---

## Iniciar: virsh start

```bash
virsh start debian-lab
```

```
Domain 'debian-lab' started
```

Lo que sucede internamente:

```
virsh start debian-lab
       │
       ▼
libvirtd lee /etc/libvirt/qemu/debian-lab.xml
       │
       ▼
Construye la línea de comandos de QEMU
       │
       ▼
Lanza el proceso: qemu-system-x86_64 -name guest=debian-lab ...
       │
       ▼
QEMU abre /dev/kvm, crea la VM, inicia las vCPUs
       │
       ▼
El guest arranca (BIOS/UEFI → GRUB → kernel → systemd)
```

### Verificar que arrancó

```bash
# Ver en la lista
virsh list

# Ver el proceso QEMU
ps aux | grep "guest=debian-lab"

# Ver el estado
virsh domstate debian-lab
# running
```

### Arrancar y conectar a consola serial

```bash
# start no conecta la consola automáticamente
virsh start debian-lab
virsh console debian-lab
```

O en un solo paso si ya está definida pero apagada:

```bash
virsh start debian-lab --console
```

---

## Apagar gracefully: virsh shutdown

```bash
virsh shutdown debian-lab
```

```
Domain 'debian-lab' is being shutdown
```

`shutdown` envía una señal **ACPI power button** al guest. Es exactamente
como pulsar el botón de apagado de un PC: el SO recibe la señal y ejecuta
su secuencia de apagado limpio.

```
virsh shutdown debian-lab
       │
       ▼
libvirtd envía ACPI power button event al guest
       │
       ▼
El guest (systemd) recibe la señal
       │
       ▼
systemd inicia shutdown: para servicios, desmonta filesystems, sync
       │
       ▼
El guest se apaga limpiamente
       │
       ▼
QEMU termina, libvirtd actualiza el estado a "shut off"
```

### El shutdown no es instantáneo

Después de `virsh shutdown`, la VM sigue corriendo mientras el guest
ejecuta su secuencia de apagado. Puedes monitorearlo:

```bash
virsh shutdown debian-lab

# La VM sigue apareciendo como running brevemente
virsh domstate debian-lab
# running

# Esperar y volver a verificar
sleep 10
virsh domstate debian-lab
# shut off
```

### ¿Qué pasa si el guest no responde?

Si el guest está colgado, no tiene ACPI configurado, o simplemente ignora
la señal, `virsh shutdown` no tiene efecto. La VM sigue corriendo
indefinidamente.

```bash
# Si después de 60 segundos sigue corriendo:
virsh domstate debian-lab
# running  ← no se apagó

# Opción: forzar apagado
virsh destroy debian-lab
```

> **Predicción**: las imágenes cloud y las instalaciones modernas de Linux
> siempre tienen ACPI configurado, así que `virsh shutdown` funciona. Donde
> puede fallar: VMs muy antiguas, kernels custom sin ACPI, o guests con
> el servicio `acpid` deshabilitado.

---

## Forzar apagado: virsh destroy

```bash
virsh destroy debian-lab
```

```
Domain 'debian-lab' destroyed
```

> **El nombre es engañoso**: `destroy` **no elimina** la VM. Solo la apaga
> forzosamente. Es el equivalente a **desconectar el cable de alimentación**
> de un servidor físico.

```
virsh destroy debian-lab
       │
       ▼
libvirtd envía SIGKILL al proceso QEMU
       │
       ▼
QEMU termina inmediatamente
       │
       ▼
Estado → shut off

La VM sigue DEFINIDA en libvirt.
Puedes arrancarla de nuevo con: virsh start debian-lab
```

### Cuándo usar destroy

| Situación | Usar |
|-----------|------|
| Apagado normal de lab | `virsh shutdown` |
| Guest colgado, no responde | `virsh destroy` |
| Guest atascado en shutdown | `virsh destroy` |
| Antes de eliminar la VM | `virsh destroy` + `virsh undefine` |

### Riesgo de corrupción

Al igual que desconectar un cable de corriente, `virsh destroy` puede dejar
el filesystem del guest en estado inconsistente:

- Archivos abiertos no se cierran
- Buffers no se escriben a disco
- El journal de ext4/xfs tendrá que recuperar al siguiente arranque

En la práctica, los filesystems con journal (ext4, xfs) se recuperan bien.
Pero **no** hagas `virsh destroy` rutinariamente: usa `virsh shutdown`
para apagados normales.

---

## Reiniciar: virsh reboot

```bash
virsh reboot debian-lab
```

```
Domain 'debian-lab' is being rebooted
```

Envía una señal ACPI reboot al guest. El SO se rearrancar limpiamente.
Mismo mecanismo que `shutdown` pero el guest rearrancar en vez de apagarse.

```bash
# Equivale a ejecutar "reboot" dentro del guest
# pero sin necesidad de tener sesión abierta
virsh reboot debian-lab
```

### reset (forzar reinicio)

```bash
virsh reset debian-lab
```

El equivalente a pulsar el botón de reset físico. Reinicia la VM
inmediatamente sin secuencia de apagado limpio. Mismo riesgo que `destroy`
pero sin apagar.

---

## Pausar y reanudar: suspend / resume

### Pausar

```bash
virsh suspend debian-lab
```

```
Domain 'debian-lab' suspended
```

Lo que sucede:

```
virsh suspend debian-lab
       │
       ▼
libvirtd detiene todas las vCPUs de la VM
       │
       ▼
El guest se congela en el estado exacto donde estaba
       │
       ▼
La RAM sigue reservada en el host
El proceso QEMU sigue vivo (pero sin consumir CPU)
       │
       ▼
Estado → paused
```

### Reanudar

```bash
virsh resume debian-lab
```

```
Domain 'debian-lab' resumed
```

La VM continúa exactamente donde estaba. Desde la perspectiva del guest,
no pasó nada (aunque el reloj puede saltar si estuvo pausada mucho tiempo).

### Caso de uso

Pausar es útil cuando necesitas liberar CPU temporalmente sin apagar la VM:

```bash
# Estás compilando algo pesado en el host
# Pausar las VMs para tener todos los cores disponibles
virsh suspend debian-lab
virsh suspend alma-lab

# Compilar...

# Reanudar
virsh resume debian-lab
virsh resume alma-lab
```

> **Nota**: `suspend` no libera RAM. Si necesitas liberar RAM, usa
> `virsh save` (siguiente sección) o apaga la VM.

---

## Guardar y restaurar: save / restore

### Guardar (hibernar a disco)

```bash
virsh save debian-lab /var/lib/libvirt/qemu/save/debian-lab.save
```

```
Domain 'debian-lab' saved to /var/lib/libvirt/qemu/save/debian-lab.save
```

Lo que sucede:

```
virsh save debian-lab /path/to/file
       │
       ▼
libvirtd pausa la VM
       │
       ▼
Vuelca toda la RAM de la VM a disco (el archivo .save)
       │
       ▼
Termina el proceso QEMU
       │
       ▼
Libera la RAM del host
       │
       ▼
Estado → saved (la VM desaparece de "virsh list")
```

El archivo `.save` contiene el estado completo de la VM: toda la RAM,
estado de las vCPUs, estado de los dispositivos. Puede ser bastante grande
(similar al tamaño de la RAM asignada).

```bash
ls -lh /var/lib/libvirt/qemu/save/debian-lab.save
# -rw------- 1 root root 2.1G ... debian-lab.save
# (VM con 2 GB de RAM → archivo de ~2 GB)
```

### Restaurar

```bash
virsh restore /var/lib/libvirt/qemu/save/debian-lab.save
```

```
Domain restored from /var/lib/libvirt/qemu/save/debian-lab.save
```

La VM continúa exactamente donde estaba cuando se guardó. Es el equivalente
a hibernar un laptop: cierras la tapa, y al abrir continúas donde estabas.

### Diferencia con suspend

```
┌───────────────────────────────────────────────────────────────┐
│                                                               │
│  virsh suspend:                    virsh save:                 │
│  • vCPUs detenidas                 • RAM volcada a disco       │
│  • RAM sigue en memoria del host   • RAM liberada del host     │
│  • Proceso QEMU vivo              • Proceso QEMU terminado    │
│  • Restaurar: virsh resume        • Restaurar: virsh restore  │
│  • Instantáneo                    • Tarda (escribir RAM)      │
│  • No libera RAM                  • Libera RAM                 │
│                                                               │
└───────────────────────────────────────────────────────────────┘
```

Usa `suspend` para pausas cortas (no libera RAM pero es instantáneo). Usa
`save` para pausas largas donde necesitas la RAM del host para otra cosa.

---

## Eliminar: virsh undefine

`virsh undefine` elimina la **definición** de la VM de libvirt. Según los
flags, puede o no borrar los discos asociados.

### Solo eliminar la definición (conservar discos)

```bash
virsh undefine debian-lab
```

```
Domain 'debian-lab' has been undefined
```

Esto borra `/etc/libvirt/qemu/debian-lab.xml`. La VM desaparece de
`virsh list --all`. Pero los discos siguen en disco:

```bash
ls /var/lib/libvirt/images/debian-lab.qcow2
# El archivo sigue existiendo
```

### Eliminar definición Y discos

```bash
virsh undefine debian-lab --remove-all-storage
```

```
Domain 'debian-lab' has been undefined
Volume 'vda'(/var/lib/libvirt/images/debian-lab.qcow2) removed.
```

Esto borra el XML **y** todos los volúmenes de almacenamiento asociados.

### Eliminar VM con firmware UEFI

Si la VM usa UEFI (OVMF), tiene archivos de firmware adicionales:

```bash
virsh undefine debian-lab --remove-all-storage --nvram
```

Sin `--nvram`, `undefine` fallará si la VM tiene variables UEFI almacenadas.

### El flujo completo de eliminación

```bash
# 1. Si la VM está corriendo, primero apagarla
virsh shutdown debian-lab
# esperar a que se apague...

# 2. Si no se apaga, forzar
virsh destroy debian-lab

# 3. Eliminar definición y discos
virsh undefine debian-lab --remove-all-storage

# 4. Verificar
virsh list --all | grep debian-lab
# (sin output — ya no existe)
```

> **Cuidado**: `--remove-all-storage` es **irreversible**. No hay papelera
> de reciclaje. Una vez ejecutado, los discos se eliminan permanentemente.

### Tabla de combinaciones

| Comando | XML | Discos | Resultado |
|---------|-----|--------|-----------|
| `virsh undefine vm` | Borrado | Intactos | Puedes reimportar los discos |
| `virsh undefine vm --remove-all-storage` | Borrado | Borrados | Eliminación total |
| `virsh destroy vm` | Intacto | Intactos | Solo apaga (nombre engañoso) |
| `virsh destroy vm` + `virsh undefine vm --remove-all-storage` | Borrado | Borrados | Flujo completo |

---

## Diagrama completo de transiciones

```
                              virsh define XML
                              (o virt-install)
                                    │
                                    ▼
                           ┌────────────────┐
                ┌─────────►│   SHUT OFF     │◄──────────┐
                │          │   (defined)     │           │
                │          └───────┬────────┘           │
                │                  │                    │
                │            virsh start               │
                │                  │              virsh shutdown
                │                  ▼              virsh destroy
                │          ┌────────────────┐           │
                │    ┌────►│    RUNNING     ├───────────┘
                │    │     │                │
                │    │     └──┬──────────┬──┘
                │    │        │          │
          virsh restore   virsh       virsh
                │    │    suspend      save
                │    │        │          │
                │    │        ▼          ▼
                │    │ ┌──────────┐  ┌──────────┐
                │    │ │  PAUSED  │  │  SAVED   │
                │    │ │          │  │ (a disco) │
                │    │ └────┬─────┘  └──────────┘
                │    │      │
                │    │  virsh resume
                │    │      │
                │    └──────┘
                │
                │   virsh undefine
                │          │
                │          ▼
                │   ┌──────────────┐
                └───│  UNDEFINED   │
                    │ (no existe)  │
                    └──────────────┘
```

### Resumen de comandos por transición

| De → A | Comando | Tipo |
|--------|---------|------|
| undefined → shut off | `virsh define XML` o `virt-install` | Registrar VM |
| shut off → running | `virsh start` | Arrancar |
| running → shut off | `virsh shutdown` | Apagado limpio (ACPI) |
| running → shut off | `virsh destroy` | Apagado forzoso |
| running → running | `virsh reboot` | Reinicio limpio |
| running → running | `virsh reset` | Reinicio forzoso |
| running → paused | `virsh suspend` | Pausar vCPUs |
| paused → running | `virsh resume` | Reanudar vCPUs |
| running → saved | `virsh save` | Hibernar a disco |
| saved → running | `virsh restore` | Restaurar de disco |
| shut off → undefined | `virsh undefine` | Eliminar definición |

---

## Errores comunes

### 1. Confundir destroy con undefine

```bash
# ❌ Creencia: "destroy borra la VM"
virsh destroy debian-lab
# La VM sigue definida, solo se apagó forzosamente

# ✅ Realidad:
# destroy = apagar forzosamente (como desconectar el cable)
# undefine = eliminar la definición de libvirt
# undefine --remove-all-storage = eliminar todo
```

Muchos usuarios nuevos esperan que `destroy` elimine la VM por completo.
No es así: solo la apaga. Puedes arrancarla de nuevo con `virsh start`.

### 2. shutdown no apaga la VM

```bash
virsh shutdown debian-lab
# ... 2 minutos después ...
virsh domstate debian-lab
# running  ← ¡sigue viva!

# Posibles causas:
# 1. El guest no tiene ACPI habilitado
# 2. El guest tiene acpid deshabilitado o no instalado
# 3. El guest está colgado y no procesa eventos ACPI

# ✅ Verificar dentro del guest:
# Debian: apt install acpid && systemctl enable acpid
# AlmaLinux: generalmente ya está instalado

# Si no se puede acceder al guest:
virsh destroy debian-lab   # forzar apagado
```

### 3. Intentar start de una VM ya corriendo

```bash
# ❌ VM ya está en ejecución
virsh start debian-lab
# error: Domain is already active

# ✅ Verificar primero
virsh domstate debian-lab
# running
```

### 4. undefine de una VM corriendo

```bash
# ❌ Intentar eliminar sin apagar
virsh undefine debian-lab --remove-all-storage
# error: Failed to undefine domain 'debian-lab'
# error: Requested operation is not valid: cannot undefine running domain

# ✅ Apagar primero
virsh destroy debian-lab
virsh undefine debian-lab --remove-all-storage
```

### 5. Olvidar --nvram con VMs UEFI

```bash
# ❌ VM creada con --boot uefi
virsh undefine debian-uefi --remove-all-storage
# error: Failed to undefine domain 'debian-uefi'
# error: Requested operation is not valid: cannot undefine domain
# with nvram

# ✅ Agregar --nvram
virsh undefine debian-uefi --remove-all-storage --nvram
```

---

## Cheatsheet

```
╔══════════════════════════════════════════════════════════════════════╗
║  CICLO DE VIDA DE VMs — REFERENCIA RÁPIDA                          ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  LISTAR:                                                           ║
║    virsh list              VMs activas (running)                   ║
║    virsh list --all        todas (incluidas apagadas)              ║
║    virsh dominfo VM        info detallada                          ║
║    virsh domstate VM       solo el estado                          ║
║                                                                    ║
║  ARRANCAR:                                                         ║
║    virsh start VM          arrancar                                ║
║    virsh start VM --console arrancar + consola serial              ║
║                                                                    ║
║  APAGAR:                                                           ║
║    virsh shutdown VM       apagado limpio (ACPI)                   ║
║    virsh destroy VM        apagado forzoso (SIGKILL a QEMU)        ║
║                            ⚠ destroy ≠ eliminar                    ║
║                                                                    ║
║  REINICIAR:                                                        ║
║    virsh reboot VM         reinicio limpio (ACPI)                  ║
║    virsh reset VM          reinicio forzoso (como botón reset)     ║
║                                                                    ║
║  PAUSAR:                                                           ║
║    virsh suspend VM        pausar vCPUs (RAM queda reservada)      ║
║    virsh resume VM         reanudar vCPUs                          ║
║                                                                    ║
║  HIBERNAR:                                                         ║
║    virsh save VM FILE      volcar RAM a disco, liberar recursos    ║
║    virsh restore FILE      restaurar desde disco                   ║
║                                                                    ║
║  ELIMINAR:                                                         ║
║    virsh undefine VM                    solo definición (XML)      ║
║    virsh undefine VM --remove-all-storage   definición + discos    ║
║    virsh undefine VM --remove-all-storage --nvram   + UEFI vars   ║
║                                                                    ║
║  FLUJO COMPLETO DE ELIMINACIÓN:                                    ║
║    virsh destroy VM  (si está corriendo)                           ║
║    virsh undefine VM --remove-all-storage                          ║
║                                                                    ║
║  ANALOGÍAS CON HARDWARE FÍSICO:                                    ║
║    shutdown  = pulsar botón de power (el SO reacciona)             ║
║    destroy   = desconectar cable de corriente                      ║
║    reboot    = Ctrl+Alt+Del                                        ║
║    reset     = pulsar botón reset de la carcasa                    ║
║    suspend   = congelar (sin liberar RAM)                          ║
║    save      = hibernar (libera RAM, estado a disco)               ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Recorrer todos los estados

**Objetivo**: transicionar una VM por todos los estados del diagrama.

> **Prerequisito**: tener una VM definida (puede ser un overlay de la
> plantilla, no necesita SO funcional para algunos pasos).

1. Verifica que la VM está apagada:
   ```bash
   virsh domstate lab-test
   # shut off
   ```

2. Arráncala:
   ```bash
   virsh start lab-test
   virsh domstate lab-test
   # running
   ```

3. Pausar y reanudar:
   ```bash
   virsh suspend lab-test
   virsh domstate lab-test
   # paused

   # Verifica: ¿el proceso QEMU sigue vivo?
   ps aux | grep "guest=lab-test"
   # Sí, sigue vivo pero las vCPUs están detenidas

   virsh resume lab-test
   virsh domstate lab-test
   # running
   ```

4. Guardar a disco y restaurar:
   ```bash
   virsh save lab-test /tmp/lab-test.save
   virsh domstate lab-test
   # ¿Qué estado muestra? ¿Aparece en "virsh list"?

   ls -lh /tmp/lab-test.save
   # ¿Cuánto ocupa el archivo comparado con la RAM asignada?

   virsh restore /tmp/lab-test.save
   virsh domstate lab-test
   # running (restaurada)

   rm /tmp/lab-test.save
   ```

5. Apagado limpio:
   ```bash
   virsh shutdown lab-test

   # Monitorear hasta que se apague
   watch -n 1 virsh domstate lab-test
   # Ctrl+C cuando cambie a "shut off"
   ```
   ¿Cuántos segundos tardó el shutdown?

6. Arrancar de nuevo y forzar apagado:
   ```bash
   virsh start lab-test
   virsh destroy lab-test
   virsh domstate lab-test
   # shut off (inmediato)
   ```

7. Verificar que `destroy` no eliminó la VM:
   ```bash
   virsh list --all | grep lab-test
   # lab-test   shut off  ← sigue definida
   ```

**Pregunta de reflexión**: si haces `virsh save` de una VM con 4 GB de RAM,
¿cuánto espacio necesitarás en disco para el archivo de estado? ¿Tiene
sentido usar `save` con VMs de lab que puedes recrear en segundos desde un
overlay?

---

### Ejercicio 2: shutdown vs destroy

**Objetivo**: entender la diferencia práctica entre apagado limpio y forzoso.

1. Arranca la VM y conéctate por consola:
   ```bash
   virsh start lab-test
   virsh console lab-test
   ```

2. Dentro de la VM, crea un archivo de prueba:
   ```bash
   echo "datos importantes" > /tmp/test-file.txt
   sync   # asegurar escritura a disco
   ```

3. Sal de la consola (`Ctrl+]`) y haz shutdown limpio:
   ```bash
   virsh shutdown lab-test
   # Esperar a que se apague
   ```

4. Arranca de nuevo y verifica que el archivo está intacto:
   ```bash
   virsh start lab-test
   virsh console lab-test
   cat /tmp/test-file.txt
   # "datos importantes"
   ```

5. Ahora crea otro archivo **sin** hacer sync:
   ```bash
   # Escribir mucho para que quede en buffers
   dd if=/dev/urandom of=/tmp/big-file.bin bs=1M count=10
   # NO hacer sync
   ```

6. Sal (`Ctrl+]`) y fuerza el apagado:
   ```bash
   virsh destroy lab-test
   ```

7. Arranca y verifica:
   ```bash
   virsh start lab-test
   virsh console lab-test
   ls -lh /tmp/big-file.bin
   # ¿Existe? ¿Tiene el tamaño completo (10M)?
   ```

8. Revisa si el journal reportó recuperación:
   ```bash
   journalctl -b | grep -i "recover\|journal\|replay" | head -5
   ```

**Pregunta de reflexión**: en un servidor de producción con una base de datos
escribiendo activamente, ¿qué consecuencias podría tener un `virsh destroy`?
¿Por qué `virsh shutdown` es siempre preferible cuando es posible?

---

### Ejercicio 3: Automatizar el ciclo de vida

**Objetivo**: escribir un script que gestione el ciclo de vida de forma
segura.

1. Escribe un script `vm-control.sh` que acepte dos argumentos (acción y
   nombre de VM) y gestione los casos de borde:

   ```bash
   #!/bin/bash
   # vm-control.sh — Gestión segura del ciclo de vida
   # Uso: ./vm-control.sh {start|stop|restart|status|nuke} VM_NAME

   ACTION="${1:?Uso: $0 {start|stop|restart|status|nuke} VM_NAME}"
   VM="${2:?Especificar nombre de VM}"
   SHUTDOWN_TIMEOUT=30
   ```

2. Implementa cada acción:
   - `start`: verificar que no está corriendo, luego `virsh start`
   - `stop`: intentar `virsh shutdown`, esperar hasta `$SHUTDOWN_TIMEOUT`
     segundos, si sigue corriendo usar `virsh destroy`
   - `restart`: stop + start
   - `status`: mostrar `virsh domstate` y `virsh dominfo`
   - `nuke`: destroy + undefine --remove-all-storage (pedir confirmación)

3. Para la acción `stop`, implementa la espera con timeout:
   ```bash
   virsh shutdown "$VM"
   for i in $(seq 1 "$SHUTDOWN_TIMEOUT"); do
       if virsh domstate "$VM" 2>/dev/null | grep -q "shut off"; then
           echo "VM apagada limpiamente."
           exit 0
       fi
       sleep 1
   done
   echo "Timeout — forzando apagado..."
   virsh destroy "$VM"
   ```

4. Prueba el script:
   ```bash
   ./vm-control.sh status lab-test
   ./vm-control.sh start lab-test
   ./vm-control.sh stop lab-test
   ./vm-control.sh start lab-test
   ./vm-control.sh restart lab-test
   ```

5. Prueba `nuke` (con una VM desechable):
   ```bash
   ./vm-control.sh nuke lab-test
   # ¿Te pidió confirmación antes de eliminar?
   ```

**Pregunta de reflexión**: el script intenta `shutdown` primero y recurre
a `destroy` solo tras un timeout. ¿Qué timeout sería razonable para un
servidor de producción con base de datos? ¿Y para una VM de lab?
