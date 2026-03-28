# TODO — Crear README.md

Ver sugerencia **B06-S3** en `SUGGESTIONS.md` (raíz del proyecto).

Contenido esperado:
- Crear un cgroup v2 desde C: `mkdir /sys/fs/cgroup/container-X`
- Escribir el PID del proceso contenedor en `cgroup.procs`
- Aplicar límite de memoria: escribir en `memory.max`
- Aplicar límite de CPU: escribir en `cpu.max`
- Limitar número de procesos: escribir en `pids.max`
- Limpiar el cgroup al salir del contenedor (rmdir)
- Verificar que los límites se aplican (intentar excederlos desde dentro)

Se añade esta funcionalidad al `container.c` iniciado en S01.
