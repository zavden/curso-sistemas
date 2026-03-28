# TODO — Crear README.md

Ver sugerencia **B06-S3** en `SUGGESTIONS.md` (raíz del proyecto).

Contenido esperado:
- Añadir `CLONE_NEWNET` al `clone()` — el proceso arranca sin interfaces de red
- Crear un par veth desde C usando netlink o llamando a `ip link add` via fork/exec
- Mover un extremo del veth al network namespace del contenedor
- Configurar IPs en ambos extremos
- Verificar conectividad desde dentro del contenedor

Este tópico es opcional/avanzado — si resulta demasiado complejo para el
nivel del bloque, puede simplificarse a solo demostrar el aislamiento de red
(contenedor sin red) y dejar la configuración veth como ejercicio propuesto.

Al terminar S03, `container.c` es un runtime funcional: aislamiento de PID,
filesystem, red, IPC, hostname, y límites de CPU/memoria.
