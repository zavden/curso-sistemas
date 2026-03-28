# Labs — Estados de unidad

## Descripcion

Laboratorio practico para entender los estados de unidades systemd:
load states (loaded, not-found, masked), active states (active,
inactive, failed), sub-states, UnitFileState (enabled, disabled,
static, masked), presets, el flujo de transiciones, y diagnostico
de fallos.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Load y active states | loaded/not-found, active/inactive, sub-states |
| 2 | UnitFileState | enabled/disabled/static/masked, presets |
| 3 | Masked y diagnostico | mask/unmask, failed state, transiciones |

## Archivos

```
labs/
└── README.md    ← Guia paso a paso (documento principal del lab)
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~20 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.
