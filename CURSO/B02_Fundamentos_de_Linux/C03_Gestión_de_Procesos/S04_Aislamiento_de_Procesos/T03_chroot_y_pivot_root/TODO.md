# TODO — Crear README.md

Ver sugerencia **B02-S3** en `SUGGESTIONS.md` (raíz del proyecto).

Contenido esperado:
- Qué hace `chroot`: cambiar el directorio raíz de un proceso
- Limitaciones de chroot (no es aislamiento real — el proceso puede escapar con privilegios)
- `pivot_root`: la alternativa segura que usa Docker
- Diferencia técnica entre los dos y por qué importa para seguridad
- Ejemplo práctico con chroot sobre un rootfs mínimo (Alpine extraído o debootstrap)
- Ejercicios paso a paso
