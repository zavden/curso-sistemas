# Labs -- AddressSanitizer y UBSan

## Descripcion

Laboratorio para compilar programas con errores de memoria y comportamiento
indefinido, detectarlos con ASan y UBSan, interpretar los reportes que generan,
y comparar con Valgrind para entender las fortalezas de cada herramienta.

## Prerequisitos

- GCC con soporte de sanitizers (`gcc -fsanitize=address -x c /dev/null -o /dev/null` debe compilar sin error)
- Valgrind instalado (`valgrind --version`) -- para la comparacion de la parte 2
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | ASan: errores de heap | Detectar heap-buffer-overflow, use-after-free, double-free; leer el reporte de ASan |
| 2 | ASan: errores de stack | Detectar stack-buffer-overflow; demostrar que Valgrind NO lo detecta |
| 3 | UBSan: comportamiento indefinido | Detectar signed overflow, division por cero, shift invalido, null deref |
| 4 | ASan + UBSan combinados | Usar ambos sanitizers juntos; opciones de runtime; linea de compilacion recomendada |

## Archivos

```
labs/
├── README.md        <- Ejercicios paso a paso
├── heap_errors.c    <- Heap buffer overflow, use-after-free, double-free
├── stack_errors.c   <- Stack buffer overflow (lo que Valgrind no detecta)
├── ub_errors.c      <- Signed overflow, div/0, shift invalido, null deref
└── combined.c       <- Errores de memoria + UB en un solo programa
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~20 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| Ejecutables compilados | Binarios | Si (limpieza por parte y final) |
