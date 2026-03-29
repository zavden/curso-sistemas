# Bloque 18: Scripting y Automatización

**Objetivo**: Dominar Bash avanzado, Python para automatización de sistemas,
Ansible para gestión de configuración, y las herramientas de automatización
cotidiana de un SysAdmin.

## Ubicación en la ruta de estudio

```
B01 → B02 → ★ B18 ★ → B20 (Cloud)
              ↑
         Solo necesita Linux (B02)
```

### Prerequisitos

| Bloque | Aporta a B18 |
|--------|-------------|
| B02 (Linux) | Shell, filesystem, procesos, permisos |

### Recomendado: B09 (Redes)

Ansible y los scripts de monitoring usan SSH y networking. No es
obligatorio pero facilita los capítulos C02–C04.

---

## Capítulo 1: Bash Scripting Avanzado [A]

### Sección 1: Estructuras de Datos en Bash
- **T01 - Arrays indexados y asociativos**: declare -a, declare -A, iteración, longitud, slicing
- **T02 - Manipulación de strings**: expansión de parámetros (${var#}, ${var%}, ${var//}), substring, replace
- **T03 - Aritmética**: (( )), let, bc para flotantes, comparaciones numéricas

### Sección 2: Control de Flujo Avanzado
- **T01 - Funciones**: local, return vs echo, pasar arrays, recursión, funciones como librería (source)
- **T02 - Traps y cleanup**: trap EXIT, ERR, SIGINT, debug traps, cleanup patterns, tempfiles seguros
- **T03 - getopts y parsing de argumentos**: flags cortos, validación, usage functions, shift
- **T04 - Process substitution y named pipes**: <(), >(), mkfifo, diff de dos comandos, pipelines complejos

### Sección 3: Herramientas de Texto
- **T01 - sed avanzado**: in-place (-i), multiline, hold space, recetas útiles
- **T02 - AWK como lenguaje**: BEGIN/END, fields, arrays asociativos, funciones, reportes tabulares
- **T03 - jq para JSON**: filtros, map, select, transformaciones, construir JSON, integrar con curl
- **T04 - yq para YAML**: parsear configs, editar in-place, Kubernetes manifests, diferencias con jq

### Sección 4: Buenas Prácticas
- **T01 - set -euo pipefail**: errexit, nounset, pipefail, cuándo relajar, error handling robusto
- **T02 - ShellCheck**: instalación, reglas comunes, integración con editor y CI
- **T03 - Debugging**: set -x, PS4 custom, bash -n (syntax check), xtrace selectivo
- **T04 - Portabilidad**: POSIX sh vs Bash, dash, /bin/sh vs /bin/bash, por qué importa en Docker y cron

## Capítulo 2: Python para SysAdmin [A]

### Sección 1: Fundamentos Rápidos
- **T01 - Crash course**: tipos, control de flujo, funciones, comprehensions (para quien ya programa en C/Rust)
- **T02 - Archivos y paths**: pathlib, open, os.walk, shutil, glob, encoding
- **T03 - Expresiones regulares**: re module, match vs search vs findall, grupos, compiled patterns
- **T04 - Manejo de errores**: try/except/finally, logging module, niveles, formateo, rotating file handler

### Sección 2: Herramientas de Sistema
- **T01 - subprocess**: run, check_output, pipes entre procesos, timeouts, shell=False y seguridad
- **T02 - os y sys**: environment, signals, exit codes, platform, uname
- **T03 - argparse**: parseo de CLI, subcommands, tipos, defaults, help text
- **T04 - json, yaml, toml**: parsear y generar configs, pyyaml, tomli/tomllib (3.11+), merge de configs

### Sección 3: Networking y HTTP
- **T01 - requests**: GET/POST, auth, sessions, timeouts, retry, verificación de SSL
- **T02 - socket básico**: TCP client/server, netcat en Python, health checks
- **T03 - paramiko**: SSH programático, ejecutar comandos remotos, SCP, key-based auth

### Sección 4: Scripts Prácticos
- **T01 - Monitor de logs**: tail -f + regex + alertas, watchdog, inotify
- **T02 - Health check de servicios**: HTTP + port check + notificación, output JSON
- **T03 - Backup automatizado**: rsync wrapper, rotación, reporting, cron-friendly
- **T04 - Inventory scanner**: network scan básico, service discovery, CSV/JSON output

## Capítulo 3: Ansible [M]

### Sección 1: Fundamentos
- **T01 - Arquitectura**: agentless, SSH push, YAML, idempotencia, por qué no scripts
- **T02 - Inventory**: hosts, groups, variables por host/grupo, inventory dinámico
- **T03 - Ad-hoc commands**: módulos ping, copy, shell, apt/dnf, service, user
- **T04 - Conexión y autenticación**: SSH keys, become/sudo, ansible.cfg, connection types

### Sección 2: Playbooks
- **T01 - Estructura**: plays, tasks, handlers, notify, changed/ok/failed
- **T02 - Variables y facts**: vars, vars_files, gather_facts, hostvars, register
- **T03 - Condicionales y loops**: when, loop, with_items, block/rescue/always
- **T04 - Templates Jinja2**: for, if, filters, template module, archivo .j2

### Sección 3: Roles y Galaxy
- **T01 - Estructura de un role**: tasks, handlers, templates, defaults, meta, files
- **T02 - ansible-galaxy**: instalar roles, crear scaffold, requirements.yml, collections
- **T03 - Ansible Vault**: encrypt, decrypt, vars en vault, vault-password-file, encrypt_string

### Sección 4: Patrones Prácticos
- **T01 - Provisionar un servidor**: users, SSH hardening, firewall (ufw/firewalld), packages
- **T02 - Deploy de aplicación**: git clone, build, systemd service, rolling update, rollback
- **T03 - Idempotencia**: por qué importa, cómo verificar, check mode (--check), diff mode
- **T04 - Testing con Molecule**: escenarios, Docker driver, verify con testinfra, CI integration

## Capítulo 4: Automatización del Sistema [A]

### Sección 1: Programación de Tareas
- **T01 - cron y crontab**: sintaxis de 5 campos, variables de entorno, logging, /etc/cron.d/
- **T02 - systemd timers**: OnCalendar, OnBootSec, Persistent, unit files, listado con systemctl
- **T03 - at y batch**: one-shot scheduling, atq, atrm, batch para carga baja
- **T04 - cron vs systemd timers**: ventajas de timers (logging, dependencias), cuándo usar cada uno

### Sección 2: Gestión de Logs
- **T01 - journald**: journalctl filtros (unit, time, priority), persistencia, tamaño, exportar
- **T02 - rsyslog**: configuración, facility/severity, remote logging, templates
- **T03 - logrotate**: configuración por servicio, compress, timing, postrotate scripts, testing
- **T04 - Structured logging**: JSON logs, por qué importa para Loki/ELK, estándar de la industria

### Sección 3: Backup y Disaster Recovery
- **T01 - rsync**: algoritmo delta, flags esenciales (-avz --delete --exclude), SSH transport
- **T02 - Estrategias de backup**: full, incremental, differential, regla 3-2-1, retención
- **T03 - Herramientas**: borgbackup (dedup, encryption), restic (multi-backend), snapshots LVM
- **T04 - Recovery plan**: documentar procedimiento, probar restores, RTO/RPO, runbooks
