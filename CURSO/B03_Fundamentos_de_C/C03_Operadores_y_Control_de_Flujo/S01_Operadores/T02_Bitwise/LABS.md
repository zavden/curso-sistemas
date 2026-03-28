# Labs — Operadores bitwise

## Descripcion

Laboratorio para practicar los seis operadores bitwise de C (&, |, ^, ~, <<, >>),
visualizar la representacion binaria de valores, y aplicar los patrones clasicos
de manipulacion de bits: set, clear, toggle y check sobre flags, incluyendo un
caso practico con permisos estilo Unix.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | AND, OR, XOR, NOT | Visualizar bits y verificar las tablas de verdad de cada operador |
| 2 | Shifts (<< y >>) | Desplazamientos como multiplicacion/division por potencias de 2, creacion de mascaras |
| 3 | Mascaras y flags | Patrones set, clear, toggle, check sobre bits individuales |
| 4 | Permisos Unix | Caso practico: construir y manipular permisos rwx con operaciones bitwise |

## Archivos

```
labs/
├── README.md        <- Ejercicios paso a paso
├── print_bits.c     <- Operadores AND, OR, XOR, NOT con visualizacion binaria
├── shifts.c         <- Shift left/right y extraccion de nibbles
├── masks_flags.c    <- Patrones set/clear/toggle/check con flags
└── unix_perms.c     <- Permisos estilo Unix (rwxrwxrwx)
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
