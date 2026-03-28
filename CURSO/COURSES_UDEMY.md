# Comparación: Curso Linux-C-Rust vs Cursos Udemy (Red Hat)

## Índice

1. [Identificación de los cursos](#identificación-de-los-cursos)
2. [Análisis del curso IT (Imran Afzal)](#análisis-del-curso-it-imran-afzal)
3. [Análisis del curso Fidel (RHCSA)](#análisis-del-curso-fidel-rhcsa)
4. [Comparación entre los dos cursos Udemy](#comparación-entre-los-dos-cursos-udemy)
5. [Comparación con nuestro curso Linux-C-Rust](#comparación-con-nuestro-curso-linux-c-rust)
6. [Qué cubren ellos que nosotros no](#qué-cubren-ellos-que-nosotros-no)
7. [Qué cubrimos nosotros que ellos no](#qué-cubrimos-nosotros-que-ellos-no)
8. [Tabla de cobertura cruzada](#tabla-de-cobertura-cruzada)
9. [Recomendación: cómo tomar los 3 juntos](#recomendación-cómo-tomar-los-3-juntos)

---

## Identificación de los cursos

### curso_it — Imran Afzal (inglés)

Curso amplio de administración Linux orientado a CentOS/RHEL. Tiene 10 módulos que
cubren desde la instalación de VMs hasta preparación para entrevistas laborales. Es un
curso generalista: toca muchos temas con profundidad variable, y dedica los últimos
dos módulos a career coaching (CV, entrevistas, certificaciones).

**Duración estimada**: ~40-50 horas de video (sumando los tiempos visibles).

**Distribuciones**: CentOS 7, CentOS 8, CentOS Stream 9, RHEL, Ubuntu (opcional).

**Módulos**:

```
  M02  Lab setup (VirtualBox/VMware, instalación de CentOS/RHEL/Ubuntu)
  M03  Filesystem (FHS, navegación, find/locate, links, wildcards)
  M04  Permisos, redirección, pipes, text processing (cut/awk/grep/sort), tar
  M05  vi/vim, sed, usuarios, SUID/SGID/sticky, LDAP intro, systemctl,
       procesos, cron/at, monitoreo, logs, env vars, screen/tmux
  M06  Shell scripting (básico: if/for/while/case, aliases)
  M07  Networking mega-módulo: NIC bonding, SSH, FTP, SCP, rsync, wget,
       yum/rpm, DNS, NTP, mail, Apache, Nginx, cockpit, Squid, rsyslog,
       Nagios, hardening, OpenLDAP, firewall, tuned, podman, Docker,
       kickstart, Ansible, OpenVPN, DHCP, clustering
  M08  Boot process, storage (particiones, LVM, swap, Stratis, RAID, fsck,
       dd), NFS, Samba, MariaDB, LAMP
  M09  Resume workshop, career advice
  M10  IT overview, entrevistas, certificaciones (RHCSA, LPIC, CompTIA)
```

### curso_fidel — Fidel (español, enfoque RHCSA)

Curso enfocado específicamente en la preparación del examen RHCSA (Red Hat Certified
System Administrator). Más estructurado y profundo en los temas que cubre, con muchos
labs prácticos. Sigue de cerca los objetivos del examen RHCSA EX200.

**Duración estimada**: ~20-25 horas de video.

**Distribuciones**: CentOS 8, RHEL 8.

**Secciones**:

```
  - Instalación de VMs (servidor y cliente)
  - Paquetes (yum, repos, GPG keys, module streams)
  - Systemd (procesos, servicios, targets, boot troubleshooting)
  - Almacenamiento (particiones, filesystems, fstab, swap, LVM con
    labs extensos, Stratis, VDO)
  - NFS (montaje, autofs, montaje directo e indirecto)
  - Logging (chronyd, journald, syslog, logrotate)
  - SELinux (modos, contextos, booleans, troubleshooting)
  - Firewall (firewalld, cockpit)
  - Usuarios y grupos (sudo, cuentas, contraseñas, restricciones)
  - Permisos (chmod, chown, especiales, umask, ACLs)
  - Tuning (señales, kill, procesos, tuned, nice/renice)
  - SSH (autenticación por clave)
  - Cron (trabajos programados)
  - Archivos temporales (systemd-tmpfiles)
  - BONUS: LDAP auth, autofs, ACLs avanzadas, mounts con systemd
```

---

## Análisis del curso IT (Imran Afzal)

### Fortalezas

- **Amplitud**: cubre un rango enorme de temas — desde lo más básico hasta
  Ansible, Docker, Nagios, clustering y OpenVPN
- **Múltiples distribuciones**: instala CentOS 7, 8, Stream 9, RHEL y Ubuntu,
  permitiendo ver diferencias entre versiones
- **Career coaching**: módulos 9 y 10 son únicos — CV, entrevistas, mercado
  laboral, certificaciones. Útil para quien está entrando al campo IT
- **Herramientas modernas**: incluye cockpit, podman, tmux, screen

### Debilidades

- **Módulo 7 es un cajón de sastre**: networking, servicios, DNS, web servers,
  firewall, Docker, Ansible, VPN, DHCP y clustering están todos en un solo
  módulo. Cada tema recibe entre 15-35 minutos, lo cual es insuficiente para
  entender en profundidad
- **Shell scripting superficial**: un solo módulo con scripts básicos
  (if/for/while/case). No cubre funciones, arrays, traps, getopts, debugging,
  ni patrones profesionales
- **Poca profundidad en seguridad**: SELinux no tiene sección dedicada (se
  menciona en hardening, 24 min), no hay AppArmor
- **Sin profundidad en almacenamiento**: LVM y RAID reciben poco tiempo
  comparado con su complejidad
- **No es RHCSA-focused**: cubre temas que no están en el examen (Nagios,
  MariaDB, LAMP) y deja sin profundizar otros que sí están (SELinux, LVM
  extendido, troubleshooting de boot)

### Veredicto

Buen curso introductorio para quien nunca ha tocado Linux. Da una vista panorámica
amplia. No es suficiente para aprobar RHCSA sin complemento significativo. Los módulos
de career coaching son un valor añadido real para principiantes en IT.

---

## Análisis del curso Fidel (RHCSA)

### Fortalezas

- **Alineado al examen RHCSA**: sigue los objetivos del EX200 de forma
  estructurada. Cada sección corresponde a un dominio del examen
- **Labs extensos y prácticos**: muchos labs de 10-30 minutos donde se ejecutan
  los procedimientos paso a paso (LVM tiene labs de 31 minutos)
- **Profundidad en temas clave**: LVM (múltiples labs: crear, extender, GPT,
  ext4, swap), SELinux (contextos, booleans, troubleshooting con labs),
  almacenamiento avanzado (Stratis + VDO con labs)
- **En español**: para un hispanohablante, entender conceptos complejos en
  su idioma reduce la carga cognitiva
- **systemd-tmpfiles**: tema poco común pero que aparece en el RHCSA
- **Boot troubleshooting**: reset de root password, problemas de filesystem
  en el arranque — temas críticos del examen

### Debilidades

- **Sin networking avanzado**: no cubre TCP/IP en profundidad, DNS server,
  web servers, ni diagnóstico de red
- **Sin shell scripting**: no hay módulo de scripting. El RHCSA pide scripts
  básicos, y este curso no los cubre
- **Sin Docker/containers**: no hay contenedores (el RHCSA moderno los pide
  con podman)
- **Sin kernel modules**: no cubre módulos del kernel, sysctl, ni compilación
- **Sin backup/restore**: no cubre tar avanzado, rsync, ni estrategias de backup
- **Solo RHEL 8**: no cubre RHEL 9 ni otras distribuciones

### Veredicto

Excelente curso para la parte de almacenamiento, SELinux, systemd y usuarios del
RHCSA. Necesita complemento en scripting, containers y networking. Es el mejor de
los dos para preparar el examen, pero no es completo por sí solo.

---

## Comparación entre los dos cursos Udemy

```
  ┌────────────────────────────┬──────────────────┬──────────────────┐
  │ Aspecto                    │ curso_it (Imran)  │ curso_fidel      │
  ├────────────────────────────┼──────────────────┼──────────────────┤
  │ Enfoque                    │ Generalista       │ RHCSA específico │
  │ Idioma                     │ Inglés            │ Español          │
  │ Profundidad                │ Poca-media        │ Media-alta       │
  │ Amplitud                   │ Muy amplio        │ Enfocado         │
  │ Labs prácticos             │ Pocos, cortos     │ Muchos, extensos │
  │ Shell scripting            │ Básico            │ Ausente          │
  │ SELinux                    │ Superficial       │ Profundo         │
  │ LVM                        │ Básico            │ Profundo + labs  │
  │ Networking                 │ Amplio/superficial │ Mínimo          │
  │ Servicios de red           │ Muchos (breve)    │ NFS solamente    │
  │ Containers                 │ Podman + Docker   │ No               │
  │ Career coaching            │ Sí (2 módulos)    │ No               │
  │ Preparación para RHCSA     │ Parcial           │ Alta (core)      │
  │ Stratis/VDO                │ Mencionados       │ Con labs          │
  │ Boot troubleshooting       │ Básico            │ Detallado        │
  └────────────────────────────┴──────────────────┴──────────────────┘
```

**Se complementan bien**: Imran da la vista panorámica y temas que Fidel no cubre
(scripting, containers, networking, servicios), mientras que Fidel da la profundidad
de examen en storage, SELinux, systemd y usuarios.

---

## Comparación con nuestro curso Linux-C-Rust

### Diferencia fundamental de filosofía

```
  Cursos Udemy                      Nuestro curso
  ══════════════                    ═══════════════

  "Cómo usar las herramientas"      "Cómo funcionan las herramientas
                                     y cómo construirlas"

  Orientación: certificación        Orientación: comprensión profunda
  y empleo IT                       + capacidad de construcción

  Formato: video + seguir pasos     Formato: texto + predicción +
                                     ejercicios de razonamiento

  Lenguaje: ninguno                 Lenguajes: C + Rust
  (solo Bash básico)                (programación de sistemas completa)
```

Los cursos Udemy enseñan a **operar** Linux. Nuestro curso enseña a **entender** Linux
y a **programar** sobre él. Son objetivos complementarios, no competidores.

---

## Qué cubren ellos que nosotros no

| Tema                                | curso_it | curso_fidel | Notas |
|-------------------------------------|----------|-------------|-------|
| Career coaching (CV, entrevistas)   | ✓        | ✗           | Útil para principiantes en IT |
| Nagios (monitoreo)                  | ✓        | ✗           | Herramienta enterprise específica |
| Cockpit (web admin)                 | ✓        | ✓           | Panel web de administración |
| MariaDB / LAMP stack               | ✓        | ✗           | Bases de datos no están en nuestro scope |
| Ansible (introducción)             | ✓        | ✗           | IaC — fuera de scope |
| Kickstart (instalación automática)  | ✓        | ✗           | Automatización de instalaciones |
| Clustering (alta disponibilidad)    | ✓        | ✗           | Pacemaker/Corosync |
| FTP server                          | ✓        | ✗           | Protocolo legacy |
| systemd-tmpfiles                    | ✗        | ✓           | Gestión de temporales |
| autofs (montaje automático NFS)     | ✗        | ✓           | Montaje on-demand |
| tuned (perfiles de rendimiento)     | ✓        | ✓           | Performance tuning |
| screen / tmux (multiplexores)       | ✓        | ✗           | Terminal multiplexing |

La mayoría de estos son temas operacionales puntuales o herramientas específicas que
no afectan la comprensión fundamental. Los más relevantes para el RHCSA son
**systemd-tmpfiles**, **autofs** y **tuned**, que aparecen en el examen.

---

## Qué cubrimos nosotros que ellos no

| Tema                                     | Impacto |
|-----------------------------------------|---------|
| **C completo** (11 capítulos)            | Entender cómo funciona el software del sistema |
| **Rust completo** (11 capítulos)         | Lenguaje moderno de sistemas, carrera |
| **Programación de sistemas en C** (syscalls, fork, exec, signals, IPC, pthreads, sockets, epoll) | Comprensión profunda del kernel |
| **Programación de sistemas en Rust** (threads, async/Tokio, FFI, unsafe) | Capacidad de construir herramientas |
| **Make, CMake, pkg-config**              | Build systems profesionales |
| **Docker en profundidad** (layers, COW, compose, Podman) | vs. solo "run containers" |
| **QEMU/KVM** (virtualización completa)  | Entorno de labs con VMs reales |
| **TCP/IP en profundidad** (modelo de capas, encapsulación, HTTP/2/3, WebSocket) | Teoría de redes sólida |
| **RAID** (niveles, mdadm, cálculos)     | Storage avanzado |
| **Btrfs** (subvolumes, snapshots, RAID, compression) | Filesystem moderno |
| **AppArmor**                             | MAC alternativo a SELinux |
| **LUKS / dm-crypt**                      | Cifrado de disco |
| **GPG**                                  | Criptografía y firma |
| **GRUB2 en profundidad** (UEFI, Secure Boot, password) | Boot avanzado |
| **Kernel** (módulos, sysctl, compilación, dkms) | Kernel internals |
| **rsync en profundidad** (delta algorithm, --link-dest snapshots) | Backup profesional |
| **tar avanzado** (incremental, metadata, pipelines SSH) | Backup profesional |
| **dd avanzado** (cloning, imaging, MBR)  | Disk forensics |
| **Estrategias de backup** (3-2-1, GFS, RPO/RTO) | Metodología |
| **Compilación desde fuente avanzada** (Autotools, DESTDIR, stow) | Mantenimiento |
| **Servicios de red desplegados** (BIND completo, Apache+Nginx+TLS, Samba, DHCP server, Postfix, PAM, LDAP client, Squid) | Infrastructure completa |
| **Hardening y auditoría** (sudo avanzado, auditd, CIS-like) | Seguridad proactiva |
| **Diagramas y modelos mentales**         | Comprensión vs memorización |
| **Preguntas de predicción en cada tema** | Aprendizaje activo |

---

## Tabla de cobertura cruzada

Temas del RHCSA y quién los cubre:

```
  ┌──────────────────────────────────┬────────┬────────┬───────────┐
  │ Tema RHCSA                       │ Imran  │ Fidel  │ Nuestro   │
  ├──────────────────────────────────┼────────┼────────┼───────────┤
  │ Usuarios y grupos                │ ●      │ ●●●    │ ●●●       │
  │ Permisos (chmod, chown, ACLs)    │ ●●     │ ●●●    │ ●●●       │
  │ SUID/SGID/sticky bit             │ ●●     │ ●●     │ ●●●       │
  │ SELinux                          │ ●      │ ●●●    │ ●●●       │
  │ Firewall (firewalld)             │ ●●     │ ●●     │ ●●●       │
  │ Systemd (servicios, targets)     │ ●●     │ ●●●    │ ●●●       │
  │ Particiones y filesystems        │ ●      │ ●●●    │ ●●●       │
  │ LVM (crear, extender)            │ ●      │ ●●●    │ ●●●       │
  │ Swap                             │ ●      │ ●●     │ ●●        │
  │ Stratis                          │ ●      │ ●●●    │ ●●        │
  │ VDO                              │ ✗      │ ●●●    │ ●●        │
  │ NFS (montar, autofs)             │ ●●     │ ●●●    │ ●●        │
  │ autofs                           │ ✗      │ ●●●    │ ✗         │
  │ Cron / at                        │ ●●     │ ●●     │ ●●●       │
  │ Shell scripting                  │ ●      │ ✗      │ ●●●       │
  │ Logging (journald, rsyslog)      │ ●      │ ●●●    │ ●●●       │
  │ logrotate                        │ ✗      │ ●●     │ ●●●       │
  │ Boot process / troubleshooting   │ ●      │ ●●●    │ ●●●       │
  │ Root password reset              │ ●      │ ●●●    │ ●●        │
  │ NTP (chronyd)                    │ ●      │ ●●     │ ●●        │
  │ Containers (podman)              │ ●●     │ ✗      │ ●●● (B01) │
  │ Paquetes (yum/dnf, repos)        │ ●●     │ ●●●    │ ●●●       │
  │ Module streams                   │ ✗      │ ●●●    │ ✗         │
  │ SSH (config, keys)               │ ●●     │ ●●     │ ●●●       │
  │ systemd-tmpfiles                 │ ✗      │ ●●     │ ✗         │
  │ Tuned profiles                   │ ●●     │ ●●     │ ✗         │
  │ Archivos tar básicos             │ ●      │ ✗      │ ●●●       │
  │ Procesos (ps, top, kill)         │ ●●     │ ●●     │ ●●●       │
  │ find / grep                      │ ●●     │ ✗      │ ●●●       │
  │ vi/vim                           │ ●●     │ ✗      │ ●         │
  ├──────────────────────────────────┼────────┼────────┼───────────┤
  │ Leyenda: ✗ no cubre  ● básico  ●● intermedio  ●●● profundo   │
  └──────────────────────────────────┴────────┴────────┴───────────┘
```

### Gaps de nuestro curso para RHCSA

Temas específicos del examen RHCSA que nuestro curso no cubre:

1. **autofs** — montaje automático de NFS on-demand
2. **systemd-tmpfiles** — gestión de archivos temporales del sistema
3. **tuned** — perfiles de rendimiento del sistema
4. **Module streams** (dnf module) — flujos de módulos de paquetes
5. **vi/vim en profundidad** — nuestro curso lo menciona pero no lo enseña
   paso a paso (asume que el usuario ya lo conoce o usará otro editor)

Estos 5 temas son relativamente pequeños y se pueden cubrir en 1-2 días de
estudio adicional con el curso de Fidel.

---

## Recomendación: cómo tomar los 3 juntos

### Estrategia general

```
  Nuestro curso = BASE PROFUNDA (entender el "por qué")
  Fidel          = COMPLEMENTO RHCSA (labs prácticos de examen)
  Imran          = PANORÁMICA + CAREER (amplitud y orientación laboral)
```

No son competidores — se complementan. Nuestro curso da profundidad teórica y
capacidad de programación. Los cursos Udemy dan práctica operacional y preparación
para certificación/empleo.

### Plan de estudio integrado

```
  ═══════════════════════════════════════════════════════════════════
  FASE 1: Fundamentos (nuestro curso B01-B02 + videos de apoyo)
  ═══════════════════════════════════════════════════════════════════

  1. Nuestro B01 (Docker) + B02 (Linux fundamentals)
     → Esta es tu base. Leé, hacé los ejercicios, predecí resultados.

  2. En paralelo o después: Imran M02-M05 como repaso en video
     → Ver los videos DESPUÉS de leer nuestros temas. El video refuerza
       lo que ya entendés y llena gaps menores (vi/vim, screen/tmux).
     → Si un tema de Imran te confunde, volvé a nuestro README.

  ═══════════════════════════════════════════════════════════════════
  FASE 2: Lenguajes (nuestro curso B03-B05)
  ═══════════════════════════════════════════════════════════════════

  3. Nuestro B03 (C) + B04 (Make/CMake) + B05 (Rust)
     → Aquí no hay equivalente en los cursos Udemy. Es contenido
       exclusivo nuestro. Tomá tu tiempo.

  ═══════════════════════════════════════════════════════════════════
  FASE 3: Programación de sistemas (nuestro curso B06-B07)
  ═══════════════════════════════════════════════════════════════════

  4. Nuestro B06 (Sistemas en C) + B07 (Sistemas en Rust)
     → Tampoco tiene equivalente Udemy. Hacé los proyectos
       (mini-shell, HTTP server, grep-like, chat).

  ═══════════════════════════════════════════════════════════════════
  FASE 4: Infraestructura avanzada + preparación RHCSA
  ═══════════════════════════════════════════════════════════════════

  5. Nuestro B08 (Almacenamiento) + Fidel (almacenamiento)
     → Leé nuestro B08 primero para la teoría (QEMU/KVM, RAID, Btrfs).
     → Después hacé los labs de Fidel para LVM, Stratis y VDO.
       Fidel tiene labs más extensos y enfocados al examen.

  6. Nuestro B09 (Redes) + Imran M07 (networking)
     → Nuestro B09 da la teoría TCP/IP sólida.
     → Imran da la práctica operacional (NIC bonding, nmcli, ss).

  7. Nuestro B10 (Servicios) + Imran M07 (servicios)
     → Nuestro B10 es más profundo (BIND completo, TLS, Samba).
     → Imran muestra Nagios, cockpit, OpenVPN que nosotros no cubrimos.

  8. Nuestro B11 (Seguridad/Kernel) + Fidel (SELinux + firewall)
     → Nuestro B11 para SELinux, AppArmor, hardening, GRUB2, kernel.
     → Fidel para los labs prácticos de SELinux (contextos, booleans,
       troubleshooting) — sus labs son muy buenos para el examen.

  ═══════════════════════════════════════════════════════════════════
  FASE 5: Gaps RHCSA específicos
  ═══════════════════════════════════════════════════════════════════

  9. Fidel: autofs, systemd-tmpfiles, tuned, module streams
     → Estos temas no están en nuestro curso. Son breves pero
       aparecen en el examen RHCSA.

  10. Imran: vi/vim en profundidad (M05)
      → 25 minutos de video. Complementa lo que asumimos en nuestro
        curso.

  ═══════════════════════════════════════════════════════════════════
  FASE 6: Career prep (si aplica)
  ═══════════════════════════════════════════════════════════════════

  11. Imran M09 (Resume) + M10 (Interviews, certificaciones)
      → Cuando estés listo para buscar trabajo. No antes.
```

### Tips para combinar los tres

**Video después de texto, no al revés.** Leé nuestro README del tema, hacé los
ejercicios, y después mirá el video de Imran o Fidel sobre el mismo tema. El video
se vuelve un repaso rápido en lugar de una primera exposición. Esto es más eficiente
que ver el video primero y leer después.

**Fidel para practicar, no para aprender.** Los labs de Fidel son excelentes para
ejecutar los procedimientos con tus propias manos. Pero la explicación teórica de
"por qué" suele ser más profunda en nuestros READMEs. Usá Fidel como tu "lab
partner".

**Imran para amplitud, no para profundidad.** Si Imran menciona un tema en 15
minutos y nuestro curso tiene un README completo, priorizá nuestro README. Si
Imran menciona algo que nosotros no cubrimos (Nagios, cockpit, Ansible), tomá
nota y evaluá si lo necesitás para tu objetivo.

**No repitas lo que ya sabés.** Si ya dominás un tema después de nuestro curso,
saltá el video de Udemy de ese tema. No hay mérito en ver 14 minutos de
"chmod explicado" si ya entendés permisos a nivel de implementación.

### Resumen visual

```
  ┌──────────────────────────────────────────────────────┐
  │                                                      │
  │  Nuestro curso Linux-C-Rust                          │
  │  ══════════════════════════                          │
  │  Base teórica profunda + programación de sistemas    │
  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐   │
  │  │ Linux   │ │ C       │ │ Rust    │ │ Infra   │   │
  │  │ B01-B02 │ │ B03-B04 │ │ B05    │ │ B08-B11 │   │
  │  │         │ │ B06     │ │ B07    │ │         │   │
  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘   │
  │                                                      │
  │  + Fidel (RHCSA labs)                                │
  │  ════════════════════                                │
  │  Labs prácticos de examen para:                      │
  │  LVM, SELinux, systemd, autofs, tmpfiles, tuned      │
  │                                                      │
  │  + Imran (panorámica + career)                       │
  │  ═════════════════════════════                       │
  │  Amplitud: Nagios, cockpit, Ansible, LAMP, vi/vim    │
  │  Career: CV, entrevistas, certificaciones            │
  │                                                      │
  └──────────────────────────────────────────────────────┘
```

### Conclusión

Si tu objetivo es **SysAdmin + entender a fondo cómo funciona Linux**, nuestro
curso es tu base principal y los dos cursos Udemy son complementos.

Si tu objetivo es **aprobar el RHCSA lo antes posible**, Fidel es tu curso
principal, nuestro curso te da la profundidad que Fidel asume, e Imran cubre
los temas que Fidel omite (scripting, containers, networking).

Los tres juntos cubren el 100% de lo que necesitás para el RHCSA y mucho más
allá — con la ventaja adicional de saber programar en C y Rust, que ningún
curso de administración Linux ofrece.
