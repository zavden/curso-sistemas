# Labs -- Padding y alineacion (avanzado)

## Descripcion

Laboratorio avanzado sobre padding y alineacion en structs: visualizacion byte
a byte del layout, las tres reglas de alineacion en detalle (campo, struct, tail
padding), uso de `__attribute__((packed))` con protocolos de red y
`_Static_assert`, packing selectivo, y control de alineacion con `_Alignas` y
cache line alignment para prevencion de false sharing.

Nota: el lab de C02/S01/T03 cubre sizeof basico, offsetof introductorio y
packed elemental. Este lab profundiza con mapeo byte a byte, reglas detalladas,
`_Static_assert`, serializacion, packing selectivo, `_Alignas`, y alineacion
a cache line.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Visualizar padding | Mapa byte por byte con memset, Bad vs Good, impacto en arrays |
| 2 | Reglas de alineacion | Regla de campo, regla de struct, tail padding, por que existen |
| 3 | packed avanzado | Protocolo de red, static_assert, serializacion, packing selectivo |
| 4 | _Alignas y cache line | alignof, alignas, alineacion a 64 bytes, prevencion de false sharing |

## Archivos

```
labs/
├── README.md        ← Ejercicios paso a paso
├── padding_map.c    ← Mapa byte por byte de structs Bad vs Good
├── layout_rules.c   ← Las tres reglas de alineacion con structs de ejemplo
├── packed_deep.c    ← packed con protocolo de red, static_assert, packing selectivo
└── alignas_demo.c   ← _Alignas, _Alignof, cache line alignment, false sharing
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
| packed_deep_broken.c | Archivo temporal (parte 3) | Si (limpieza de parte 3) |
