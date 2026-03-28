# Labs — Enums

## Descripcion

Laboratorio para explorar enums en C: declaracion con valores automaticos y
explicitos, uso como constantes en switch y arrays, el patron COUNT para
dimensionar arrays, y las limitaciones de type safety.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Declaracion | Valores automaticos, valores explicitos, sizeof de enum |
| 2 | Enums como constantes | Uso en switch, arrays indexados por enum |
| 3 | Patron COUNT | Enum con _COUNT al final para dimensionar arrays |
| 4 | Limitaciones | Enums son int, no type-safe, namespace global |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── enum_basics.c      ← Declaracion y valores de enums
├── enum_switch.c      ← Enum en switch y array de nombres
├── enum_count.c       ← Patron COUNT para dimensionar arrays
└── enum_limits.c      ← Limitaciones de type safety y namespace
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
