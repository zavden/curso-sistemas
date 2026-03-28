# Resumen del Curso Linux-C-Rust (Bloques 1–11)

## Índice

1. [Qué cubre este curso](#qué-cubre-este-curso)
2. [Qué podés construir con este conocimiento](#qué-podés-construir-con-este-conocimiento)
3. [Perfiles laborales alcanzables](#perfiles-laborales-alcanzables)
4. [Recomendaciones para realizar el curso](#recomendaciones-para-realizar-el-curso)
5. [Mapa de dependencias entre bloques](#mapa-de-dependencias-entre-bloques)
6. [Estimación de tiempo](#estimación-de-tiempo)

---

## Qué cubre este curso

El curso está organizado en 11 bloques que abarcan tres pilares:

```
  ┌──────────────────────────────────────────────────────────────┐
  │                     LINUX-C-RUST COURSE                      │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  PILAR 1: INFRAESTRUCTURA                                    │
  │  ─────────────────────────                                   │
  │  B01  Docker y entorno de laboratorio                        │
  │  B02  Fundamentos de Linux (FHS, usuarios, procesos,         │
  │       paquetes, shell scripting, systemd, logging)           │
  │  B08  Almacenamiento (QEMU/KVM, particiones, LVM, RAID,     │
  │       ext4/XFS/Btrfs, quotas, Stratis/VDO)                  │
  │  B09  Redes (TCP/IP, DNS, DHCP, firewall, diagnóstico)      │
  │  B10  Servicios de red (SSH, BIND, Apache/Nginx, NFS,        │
  │       Samba, DHCP server, email, PAM, LDAP, Squid)          │
  │  B11  Seguridad, kernel y arranque (SELinux, AppArmor,       │
  │       hardening, GRUB2, UEFI, módulos, sysctl, backup)      │
  │                                                              │
  │  PILAR 2: PROGRAMACIÓN EN C                                  │
  │  ──────────────────────────                                  │
  │  B03  Fundamentos de C (tipos, punteros, memoria dinámica,   │
  │       structs, I/O, preprocesador, librerías)               │
  │  B04  Sistemas de compilación (Make, CMake, pkg-config)      │
  │  B06  Programación de sistemas en C (syscalls, fork/exec,    │
  │       señales, IPC, pthreads, sockets, epoll)               │
  │       Proyectos: mini-shell, HTTP server en C                │
  │                                                              │
  │  PILAR 3: PROGRAMACIÓN EN RUST                               │
  │  ─────────────────────────────                               │
  │  B05  Fundamentos de Rust (ownership, lifetimes, traits,     │
  │       generics, iteradores, smart pointers, macros)         │
  │  B07  Programación de sistemas en Rust (threads, async/await,│
  │       Tokio, FFI, unsafe, debugging)                        │
  │       Proyectos: grep-like CLI, chat TCP multi-cliente       │
  │                                                              │
  └──────────────────────────────────────────────────────────────┘
```

En total: **~70 capítulos**, **~200 secciones**, **~600+ temas** con ejercicios
prácticos, diagramas, cheatsheets y preguntas de reflexión.

---

## Qué podés construir con este conocimiento

### Herramientas de línea de comandos

Con B02 (shell scripting) + B03/B05 (C/Rust) + B06/B07 (sistemas):

- **Herramientas de texto**: grep, sed, awk — clones o variantes propias
- **Monitores de sistema**: como htop, btop, o glances (lecturas de /proc, TUI)
- **Utilidades de archivos**: compresores, buscadores, sincronizadores
- **Gestores de procesos**: supervisores de servicios, watchdogs
- **CLIs profesionales**: con argument parsing (clap), colores, configuración

### Servidores y servicios de red

Con B06 (sockets en C) + B07 (async networking en Rust) + B09/B10 (redes/servicios):

- **Servidores HTTP** básicos y de complejidad media
- **Servidores de chat** multi-cliente con Tokio
- **Proxies TCP/UDP** — forward proxies, reverse proxies simples
- **Servicios con protocolos propios** — protocolos binarios o de texto
- **Clientes de red**: HTTP clients, DNS resolvers, port scanners

### Infraestructura y automatización

Con B01 (Docker) + B02 (scripting/systemd) + B08-B11:

- **Scripts de mantenimiento** con logging, alertas y cron/systemd timers
- **Herramientas de diagnóstico** del sistema (health checks con reporte)
- **Configuración automatizada** de servidores (servicios, firewall, usuarios)
- **Entornos de laboratorio** reproducibles con Docker y QEMU/KVM
- **Estrategias de backup** implementadas (tar incremental, rsync snapshots)
- **Hardening scripts** para asegurar servidores Linux

### Librerías y componentes reutilizables

Con B03/B05 (fundamentos) + B04 (build systems) + B07 (FFI):

- **Librerías en C** estáticas y dinámicas (.a, .so)
- **Crates en Rust** publicables en crates.io
- **Bindings C↔Rust** con FFI (bindgen, cbindgen)
- **Wrappers seguros** de APIs C en Rust

### Proyectos integrados del curso

Estos son proyectos explícitos que aparecen en los bloques:

| Proyecto                  | Bloque | Tecnología       |
|--------------------------|--------|------------------|
| Mini-shell               | B06    | C (fork, exec, pipes) |
| HTTP server              | B06    | C (sockets, threads) |
| grep-like CLI            | B07    | Rust (regex, clap, memmap) |
| Chat TCP multi-cliente   | B07    | Rust (Tokio, async) |
| System health check      | B11    | Bash + Rust (TUI con ratatui) |

---

## Perfiles laborales alcanzables

### Perfiles directamente alcanzables

**1. Administrador de sistemas Linux (SysAdmin) — Junior/Mid**

Este es el perfil más directamente cubierto por el curso.

```
  Conocimiento del curso que aplica:
  ───────────────────────────────────
  B02  Gestión de usuarios, permisos, procesos, paquetes, scripting
  B08  Almacenamiento: LVM, RAID, filesystems, quotas
  B09  Redes: TCP/IP, DNS, DHCP, firewall, diagnóstico
  B10  Servicios: SSH, DNS server, web server, NFS, Samba, LDAP, PAM
  B11  SELinux/AppArmor, hardening, kernel, boot, backup, mantenimiento

  Certificaciones que podrías preparar después del curso:
  ──────────────────────────────────────────────────────────
  • LPIC-1 / LPIC-2 (Linux Professional Institute)
  • RHCSA (Red Hat Certified System Administrator)
  • CompTIA Linux+
```

El curso cubre el ~80% del temario RHCSA y ~70% del LPIC-2. Lo que falta es
práctica hands-on intensa (que se obtiene usando los labs del curso) y algunos
temas puntuales de cada certificación.

**2. DevOps Engineer — Junior**

```
  Conocimiento del curso que aplica:
  ───────────────────────────────────
  B01  Docker (imágenes, containers, compose, Podman)
  B02  Scripting avanzado, systemd, cron
  B09  Redes, firewall, diagnóstico
  B10  Web servers, reverse proxy, TLS
  B11  Automatización, backup, hardening

  Lo que necesitarías agregar:
  ─────────────────────────────
  • CI/CD (GitHub Actions, GitLab CI, Jenkins)
  • Infrastructure as Code (Terraform, Ansible)
  • Orquestación de containers (Kubernetes)
  • Cloud (AWS, GCP o Azure — al menos uno)
  • Observabilidad (Prometheus, Grafana, ELK)
```

El curso te da la base sólida de Linux y contenedores sobre la que se construye
todo lo demás. Un DevOps que no entiende networking, filesystems, permisos o
procesos a nivel de OS tiene una base frágil.

**3. Programador de sistemas — Junior**

```
  Conocimiento del curso que aplica:
  ───────────────────────────────────
  B03  C completo (punteros, memoria, structs, librerías)
  B04  Make, CMake, pkg-config
  B05  Rust completo (ownership, traits, async, unsafe)
  B06  Syscalls, fork/exec, señales, IPC, sockets, epoll
  B07  Threads, async/Tokio, FFI, debugging avanzado

  Roles típicos:
  ──────────────
  • Desarrollo de herramientas CLI
  • Desarrollo de servicios de red
  • Desarrollo de librerías de bajo nivel
  • Contribución a proyectos open source en C o Rust
```

**4. Ingeniero de seguridad / Pentester — Entry Level**

```
  Conocimiento del curso que aplica:
  ───────────────────────────────────
  B02  Permisos, SUID/SGID, procesos
  B06  Syscalls, sockets, buffer overflows (comprensión)
  B09  TCP/IP, firewall, nmap, tcpdump
  B11  SELinux, AppArmor, hardening, auditoría, LUKS, GPG

  Lo que necesitarías agregar:
  ─────────────────────────────
  • Metodología de pentesting (OWASP, PTES)
  • Herramientas (Burp Suite, Metasploit, Wireshark avanzado)
  • Seguridad web (XSS, SQLi, CSRF en profundidad)
  • Certificaciones (CEH, OSCP, CompTIA Security+)
```

### Perfiles alcanzables con complementos mínimos

| Perfil                          | Qué agregar                                      |
|--------------------------------|--------------------------------------------------|
| Backend developer (Rust)        | Framework web (Axum/Actix), base de datos, REST API design |
| Embedded systems developer      | Hardware específico, RTOS, protocolos (SPI, I2C, UART) |
| Cloud engineer                  | AWS/GCP/Azure, Terraform, Kubernetes              |
| SRE (Site Reliability Engineer) | Observabilidad, SLOs/SLIs, incident management    |
| Database administrator          | PostgreSQL/MySQL en profundidad, replicación, tuning |

### Combinaciones de mayor impacto laboral

```
  COMBO 1: SysAdmin + DevOps (más demandado)
  ───────────────────────────────────────────
  Curso completo + Ansible + Kubernetes + CI/CD + un cloud provider
  → Salario competitivo, alta demanda

  COMBO 2: Rust systems programmer (nicho, bien pagado)
  ─────────────────────────────────────────────────────
  B05 + B07 + framework web (Axum) + contribuciones open source
  → Menos puestos disponibles pero salarios altos y poca competencia

  COMBO 3: Security-focused SysAdmin (diferenciador)
  ─────────────────────────────────────────────────────
  Curso completo + RHCSA + CompTIA Security+ o CySA+
  → SysAdmin con enfoque en seguridad es un perfil muy valorado
```

---

## Recomendaciones para realizar el curso

### 1. Orden de estudio

El orden recomendado respeta las dependencias reales entre bloques:

```
  FASE 1 — Base (paralelo)
  ════════════════════════
  B01 Docker          ─── Para tener el entorno de laboratorio
  B03 C (caps 1-8)    ─┐
  B05 Rust             ─┴─ Los lenguajes se pueden estudiar en paralelo
                           No dependen de Linux profundo

  FASE 2 — Linux
  ═══════════════
  B02 Fundamentos de Linux
      Prerequisito para todo lo que sigue

  FASE 3 — Compilación y lenguajes avanzados (paralelo)
  ═════════════════════════════════════════════════════
  B03 C (caps 9-11)   ─── File I/O, preprocesador, librerías
  B04 Make/CMake       ─── Necesita entender compilación (B03)

  FASE 4 — Programación de sistemas
  ═══════════════════════════════════
  B06 Sistemas en C    ─── Necesita B02 + B03 completo
  B07 Sistemas en Rust ─── Necesita B02 + B05 + conceptos de B06

  FASE 5 — Infraestructura avanzada
  ═════════════════════════════════
  B08 Almacenamiento   ─── Necesita B02
  B09 Redes            ─── Necesita B02
  B10 Servicios        ─── Necesita B02 + B09
  B11 Seguridad/Kernel ─── Necesita B02 + B09 + B10

  B08, B09 se pueden hacer en paralelo.
  B10 necesita B09 antes.
  B11 es el último bloque.
```

### 2. No solo leas — hacé los ejercicios

Cada tema tiene 3 ejercicios con preguntas de predicción. El patrón es:

1. **Predecí** el resultado antes de ejecutar
2. **Ejecutá** y compará con tu predicción
3. **Si fallaste**, entendé por qué tu modelo mental era incorrecto

Este ciclo de "predecir → verificar → corregir" es la forma más efectiva de construir
comprensión profunda. Leer sin ejecutar es como estudiar natación leyendo un libro.

### 3. Usá el entorno de laboratorio

B01 existe por una razón: Docker (y en B08, QEMU/KVM) te dan entornos desechables
donde podés romper cosas sin consecuencias. Usá contenedores para:

- Practicar comandos destructivos (rm -rf, fdisk, iptables)
- Configurar servicios sin ensuciar tu máquina
- Simular escenarios multi-máquina (cliente/servidor)
- Empezar de cero cuando algo sale mal

**No practiques administración de sistemas en tu máquina real.** Un error con
`chmod -R` o `iptables` en tu host puede dejarte sin acceso.

### 4. Un tema por sesión, no un bloque por día

Cada tema está diseñado para ser una unidad de estudio completa de ~30-60 minutos
(lectura + ejercicios). Un ritmo sostenible es:

```
  Ritmo conservador:  1-2 temas/día  →  ~8-12 meses para B01-B11
  Ritmo moderado:     3-4 temas/día  →  ~4-6 meses
  Ritmo intensivo:    5-8 temas/día  →  ~2-3 meses (riesgo de saturación)
```

Es mejor avanzar lento y hacer los ejercicios que avanzar rápido y solo leer.

### 5. Tomá notas propias

Los READMEs son material de referencia. Tus notas personales deberían capturar:

- Qué te sorprendió (modelo mental que tuviste que corregir)
- Qué te costó entender (para revisar después)
- Conexiones entre temas que descubriste
- Comandos que querés recordar

Un cuaderno físico o un archivo de texto plano funciona mejor que sistemas complejos.

### 6. Construí proyectos fuera del curso

Los proyectos del curso (mini-shell, HTTP server, grep-like, chat, health check) son
guiados. Para consolidar, construí algo propio:

```
  Ideas de proyectos graduales:
  ─────────────────────────────
  Después de B02:
    • Script que audite permisos SUID/SGID en el sistema
    • Automatización de setup de un servidor con usuarios y permisos

  Después de B03+B04:
    • Una utilidad de línea de comandos en C (ej: wc clone, tree clone)
    • Una librería en C con Makefile y tests

  Después de B05:
    • Un CLI en Rust que resuelva un problema real tuyo
    • Un crate publicable con documentación

  Después de B06+B07:
    • Un servidor de archivos (FTP simplificado)
    • Un proxy HTTP simple

  Después de B08-B11:
    • Automatización completa de un servidor: particiones, LVM,
      servicios, firewall, usuarios, backup, monitoreo
    • Un script de hardening que implemente CIS benchmarks
```

### 7. Alterná entre teoría y práctica

Los bloques tienen capítulos teóricos [A] y proyectos [P]. No hagas toda la teoría
de un bloque y después todos los proyectos. Alterná:

```
  ✗ Leer todo B06 → hacer proyectos B06  (olvidás lo del principio)
  ✓ Leer C01-C03 → hacer mini-shell → leer C04-C06 → hacer HTTP server
```

### 8. Repasá bloques anteriores periódicamente

Después de terminar un bloque, dedicá una sesión a repasar los cheatsheets de bloques
anteriores. El conocimiento de Linux es acumulativo — B11 asume que recordás B02.

### 9. Para certificaciones: complementá con exámenes de práctica

Si tu objetivo es RHCSA o LPIC:

- Usá el curso como base teórica
- Complementá con **exámenes de práctica** oficiales o de plataformas como
  Asghar Ghori (RHCSA book) o linux-training.be
- Practicá en VMs con restricción de tiempo (RHCSA = 2.5 horas)
- Los ejercicios del curso te preparan conceptualmente, pero los exámenes de
  certificación requieren velocidad y precisión bajo presión

### 10. No te saltes los "errores comunes"

Cada tema tiene una sección de 5 errores comunes. Estos no son relleno — son los
errores que realmente vas a cometer. Leélos con atención y, si podés, intentá
reproducir el error a propósito para entender por qué ocurre.

---

## Mapa de dependencias entre bloques

```
  B01 Docker ────────────────────────────────────────┐
       │                                             │ entorno
       ▼                                             ▼
  B03 C (1-8) ───────┐                          B02 Linux
       │             │                           │  │  │
       ▼             ▼                           │  │  │
  B03 C (9-11)   B05 Rust                        │  │  │
       │             │                           │  │  │
       ▼             │                           │  │  │
  B04 Make/CMake     │                           │  │  │
       │             │                           │  │  │
       ▼             ▼                           │  │  │
  B06 Sistemas C ◄───────────────────────────────┘  │  │
       │                                            │  │
       ▼                                            │  │
  B07 Sistemas Rust ◄───────────────────────────────┘  │
                                                       │
  B08 Almacenamiento ◄─────────────────────────────────┤
                                                       │
  B09 Redes ◄──────────────────────────────────────────┤
       │                                               │
       ▼                                               │
  B10 Servicios ◄──────────────────────────────────────┘
       │
       ▼
  B11 Seguridad/Kernel/Arranque
```

---

## Estimación de tiempo

| Bloque | Capítulos | Temas aprox. | Ritmo moderado |
|--------|-----------|-------------|----------------|
| B01    | 5         | ~30         | 1-2 semanas    |
| B02    | 9         | ~55         | 2-3 semanas    |
| B03    | 11        | ~45         | 2-3 semanas    |
| B04    | 3         | ~12         | 1 semana       |
| B05    | 11        | ~55         | 2-3 semanas    |
| B06    | 8         | ~40         | 2-3 semanas    |
| B07    | 9         | ~45         | 2-3 semanas    |
| B08    | 6 (+C0)   | ~40         | 2-3 semanas    |
| B09    | 7         | ~30         | 1-2 semanas    |
| B10    | 5         | ~30         | 1-2 semanas    |
| B11    | 7         | ~45         | 2-3 semanas    |
| **Total** | **81** | **~427**    | **~5-7 meses** |

Estos tiempos asumen 2-3 horas de estudio diario, haciendo ejercicios. Ajustá según
tu disponibilidad y ritmo.

---

> Este curso te da la base técnica profunda que separa a un operador de un
> ingeniero. Quien entiende cómo funcionan los syscalls, los sockets, el kernel,
> la memoria y los permisos no solo resuelve problemas — entiende por qué
> ocurren. Esa comprensión es lo que te permite diagnosticar lo desconocido,
> que es, en última instancia, el trabajo real.
