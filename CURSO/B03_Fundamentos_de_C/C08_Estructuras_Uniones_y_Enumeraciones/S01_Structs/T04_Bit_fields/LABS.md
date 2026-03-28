# Labs -- Bit fields

## Descripcion

Laboratorio para declarar bit fields de distintos anchos, observar el
truncamiento por overflow, comparar bit fields con operaciones bitwise, y
verificar las limitaciones del estandar (direccion, portabilidad, campo de
ancho 0).

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Declaracion | Campos de 1, 5, 4 y 7 bits, sizeof vs struct con int |
| 2 | Overflow de bit fields | Truncamiento al asignar fuera del rango, signed vs unsigned |
| 3 | Bit fields vs bitwise | Permisos rwx con ambos enfoques, legibilidad vs control |
| 4 | Limitaciones | Direccion (&), campo de ancho 0, portabilidad del layout |

## Archivos

```
labs/
├── README.md              ← Ejercicios paso a paso
├── declaration.c          ← Structs con campos de distintos anchos
├── overflow.c             ← Asignacion de valores fuera del rango
├── bitfield_vs_bitwise.c  ← Permisos con bit fields y con mascaras
└── limitations.c          ← Direccion, campo de ancho 0, portabilidad
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
| Ejecutables compilados | Binarios | Si (limpieza final) |
| `address_test.c` | Archivo temporal | Si (limpieza parte 4) |
