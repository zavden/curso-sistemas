# Labs — RAII en C

## Descripcion

Laboratorio para practicar los patrones que simulan RAII en C: goto cleanup
(estandar, usado en el kernel de Linux) y `__attribute__((cleanup))` (extension
GCC/Clang). Se comparan ambos enfoques con el anti-patron de if-else anidados.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | goto cleanup | Adquirir archivo + malloc, liberar en orden inverso con un solo label |
| 2 | `__attribute__((cleanup))` | Auto-free al salir del scope, macros AUTO_FREE y AUTO_FCLOSE |
| 3 | Comparacion de estilos | if-else anidado vs goto cleanup vs cleanup attribute, legibilidad y seguridad |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── goto_cleanup.c     ← Programa con patron goto cleanup
├── cleanup_attr.c     ← Programa con __attribute__((cleanup))
└── compare_styles.c   ← Tres estilos en un solo programa para comparar
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~25 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| Ejecutables compilados | Binarios | Si (limpieza final) |
| Archivos de texto temporales | Datos de prueba | Si (limpieza por parte y final) |
