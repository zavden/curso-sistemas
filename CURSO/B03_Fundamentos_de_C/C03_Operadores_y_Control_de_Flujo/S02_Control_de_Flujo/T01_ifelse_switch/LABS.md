# Labs — if/else, switch

## Descripcion

Laboratorio para practicar branching con if/else y switch en C. Se exploran
condiciones encadenadas, fall-through intencional y accidental, la generacion
de assembly distinta entre if y switch, y los errores clasicos (dangling else,
switch sin break, = vs ==).

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | if/else if/else | Branching basico, condiciones encadenadas, truthiness en C |
| 2 | switch | break, fall-through intencional, agrupacion de cases, default |
| 3 | Comparacion if vs switch | Assembly generado con -O0 vs -O2, comparaciones secuenciales vs lookup table |
| 4 | Errores comunes | Dangling else, switch sin break, = vs == |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── branching.c        ← if/else basico, condiciones encadenadas, truthiness
├── switch_basic.c     ← switch con break, fall-through, agrupacion de cases
├── if_vs_switch.c     ← Misma logica implementada con if y con switch
├── dangling_else.c    ← Bug de dangling else y version corregida
├── switch_nobreak.c   ← Bug de switch sin break y version corregida
└── assign_vs_equal.c  ← Bug de = vs == en condiciones
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
| *.s | Archivos assembly | Si (limpieza final) |
