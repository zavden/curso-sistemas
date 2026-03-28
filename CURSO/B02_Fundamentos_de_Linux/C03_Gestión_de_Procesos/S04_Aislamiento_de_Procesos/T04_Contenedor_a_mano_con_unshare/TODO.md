# TODO — Crear README.md

Ver sugerencia **B02-S3** en `SUGGESTIONS.md` (raíz del proyecto).

Contenido esperado:
- Integrar T01, T02 y T03 en un ejercicio completo
- Crear un entorno aislado completo desde bash sin Docker:
  `unshare --pid --fork --mount --uts --ipc --net --mount-proc chroot /path/rootfs /bin/sh`
- El estudiante observa: PID 1 propio, hostname modificable, filesystem aislado, red vacía
- Comparar con `docker run --rm -it alpine sh` — la experiencia es idéntica
- Limpieza y qué queda en el host tras salir
- Ejercicios paso a paso

Este tópico es el cierre práctico de S04 y el puente conceptual hacia
B06/C09 donde se implementa lo mismo en C.
