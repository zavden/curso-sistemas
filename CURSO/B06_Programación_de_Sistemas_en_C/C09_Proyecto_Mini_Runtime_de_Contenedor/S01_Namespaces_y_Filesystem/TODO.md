# TODO — Crear README.md (y archivos de soporte)

Ver sugerencia **B06-S3** en `SUGGESTIONS.md` (raíz del proyecto).

Prerrequisito: B02/C03/S04 (namespaces y cgroups desde bash) y B06/C02 (fork/exec).

Contenido esperado:
- `clone()` con flags de namespace: `CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC`
- Stack para el proceso hijo (diferencia con fork)
- `chroot()` vs `pivot_root()` en C — implementar `pivot_root` correctamente
- Montar `/proc` en el nuevo mount namespace: `mount("proc", "/proc", "proc", ...)`
- El proceso hijo arranca con PID 1 y un filesystem propio

Archivos de soporte esperados:
- `container.c` — programa principal, se construye incrementalmente en cada tópico
- `Makefile` — compilación con `-Wall -Wextra`
- `rootfs/` — directorio con un rootfs mínimo (instrucciones para extraer Alpine)
